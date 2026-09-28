/*
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "dsp/oscillators/phase_distortion.h"
#include "util/fixedpoint.h"
#include "util/waves.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <utility>

namespace deluge::dsp::phase_distortion {
namespace {

// Measured from the CZ-1 captures (51 pixels a period). At full DCW the fast part of a bent cycle runs this many times
// faster than the unbent phase: the saw's edge fits in one pixel, 2% of the period, and the pulse's dip in two.
constexpr float kMaxSpeedup = 25.f;
// Resonant multiple at full DCW: the CZ-1 captures carry 15 sine cycles in each period, windowed or raw ("about 15
// sine waves in each period").
constexpr float kMaxResonance = 15.f;
// Share of the cycle the saw pulse parks on its peak at full DCW: 48% on the CZ-1 capture, after a 50% fall and a
// one-pixel rise.
constexpr float kSawPulseMaxHold = 0.48f;
// Double sine at full DCW: the extra excursion to the trough and back takes this share of the cycle.
constexpr float kDoubleSineDip = 1.f / kMaxSpeedup;
// VPS vertical inflection at full DCW (the reach pd-hybrid-synth gives its amount knob; VPS is not a CZ feature, so
// there is no hardware to follow).
constexpr float kVpsMaxV = 4.f;
// Keep the VPS inflection off the cycle's ends, so neither segment collapses.
constexpr float kVpsMinD = 0.02f;
// A bend's fastest part is kept under this fraction of the sample rate.
constexpr float kNyquistShare = 0.45f;
constexpr float kTwoPi = 6.28318530718f;
constexpr double kQ32 = 4294967296.0;
// The cosine is read 3/4 of a turn on, so a phase wave at DCW 0 is sin(): the Deluge's sine oscillator, starting on
// its zero crossing. The same offset in the combined two-cycle phase, which runs at half speed.
constexpr uint32_t kReadOffset = 3u << 30;
constexpr uint32_t kCombinedReadOffset = kReadOffset >> 1;

constexpr char const* kWaveNames[kNumWaves] = {
    "saw", "square", "pulse", "doubleSine", "sawPulse", "resoSaw", "resoTriangle", "resoTrapezoid",
};

uint32_t cyclePositionToQ32(double x) {
	return static_cast<uint32_t>(std::clamp(x, 0.0, 1.0 - 1.0 / kQ32) * kQ32);
}

uint32_t turnsToQ32(double turns) {
	return cyclePositionToQ32(turns - std::floor(turns));
}

/// Builds a piecewise-linear phase map from its breakpoints and works out its DC as it goes.
class MapBuilder {
public:
	explicit MapBuilder(Shape& shape) : shape_(shape) {}

	void through(std::initializer_list<std::pair<double, double>> points) {
		const auto* previous = points.begin();
		for (const auto* point = previous + 1; point != points.end(); previous = point++) {
			line(previous->first, previous->second, point->first, point->second);
		}
		shape_.dc = static_cast<int32_t>(std::lround(mean_ * (1 << 30)));
	}

private:
	void line(double x0, double y0, double x1, double y1) {
		if (x1 - x0 < 1e-7 || shape_.numSegments == Shape::kMaxSegments) {
			return; // an empty segment
		}
		const double slope = (y1 - y0) / (x1 - x0);
		const int32_t i = shape_.numSegments++;
		if (i == 0) {
			shape_.start[0] = 0;
			shape_.base[0] = turnsToQ32(y0);
		}
		else {
			// Chain each segment on from where the last one really ends, so the map has no steps.
			shape_.start[i] = cyclePositionToQ32(x0);
			const uint32_t length = shape_.start[i] - shape_.start[i - 1];
			shape_.base[i] =
			    shape_.base[i - 1] + static_cast<uint32_t>((static_cast<int64_t>(length) * shape_.slope[i - 1]) >> 20);
		}
		shape_.slope[i] = static_cast<int32_t>(std::lround(slope * (1 << 20)));

		// The mean of cos(2 pi y) over this segment.
		if (std::fabs(y1 - y0) < 1e-12) {
			mean_ += std::cos(kTwoPi * static_cast<float>(y0 - std::floor(y0))) * (x1 - x0);
		}
		else {
			mean_ += (std::sin(kTwoPi * static_cast<float>(y1 - std::floor(y1)))
			          - std::sin(kTwoPi * static_cast<float>(y0 - std::floor(y0))))
			         / (kTwoPi * slope);
		}
	}

	Shape& shape_;
	double mean_ = 0.0;
};

/// The integral of (alpha + beta q) cos(omega q) from a to b.
float windowedCosine(float a, float b, float alpha, float beta, float omega) {
	auto antiderivative = [&](float q) {
		return (alpha + beta * q) * std::sin(omega * q) / omega + beta * std::cos(omega * q) / (omega * omega);
	};
	return antiderivative(b) - antiderivative(a);
}

/// Mean of 1 - window * (1 - cos(2 pi k q)) over one cycle.
float resonantMean(Wave window, float k) {
	const float omega = kTwoPi * k;
	switch (window) {
	case Wave::RESO_TRIANGLE:
		return 0.5f + windowedCosine(0.f, 0.5f, 0.f, 2.f, omega) + windowedCosine(0.5f, 1.f, 2.f, -2.f, omega);
	case Wave::RESO_TRAPEZOID:
		return 0.25f + windowedCosine(0.f, 0.5f, 1.f, 0.f, omega) + windowedCosine(0.5f, 1.f, 2.f, -2.f, omega);
	default: // RESO_SAW
		return 0.5f + windowedCosine(0.f, 1.f, 1.f, -1.f, omega);
	}
}

/// The resonant window at cycle position q, Q31; it closes to 0 at the cycle's end.
[[gnu::always_inline]] inline uint32_t windowAt(Wave window, uint32_t q) {
	switch (window) {
	case Wave::RESO_TRIANGLE: // rises to full depth mid-cycle, then falls
		return (q < 0x80000000u) ? q : ~q;
	case Wave::RESO_TRAPEZOID: // holds full depth for half the cycle, then falls
		return (q < 0x80000000u) ? 0x7FFFFFFFu : ~q;
	default: // RESO_SAW: falls across the cycle
		return (~q) >> 1;
	}
}

/// valueAt(), inlined into the render loop.
[[gnu::always_inline]] inline int32_t evaluate(const Shape& shape, uint32_t q) {
	if (!shape.resonant) {
		int32_t i = shape.numSegments - 1;
		while (q < shape.start[i]) {
			--i;
		}
		const uint32_t bent =
		    shape.base[i] + static_cast<uint32_t>((static_cast<int64_t>(q - shape.start[i]) * shape.slope[i]) >> 20);
		return (getSine(bent + (1u << 30)) >> 1) - shape.dc;
	}
	const uint32_t window = windowAt(shape.window, q);
	const uint32_t train = static_cast<uint32_t>((static_cast<uint64_t>(q) * shape.resonance) >> 16);
	const int32_t cosine = getSine(train + (1u << 30));
	const uint32_t halfOneMinusCos = static_cast<uint32_t>((2147483648LL - cosine) >> 1);
	const int32_t dip = static_cast<int32_t>((static_cast<uint64_t>(window) * halfOneMinusCos) >> 31);
	return (1 << 30) - dip - shape.dc;
}

} // namespace

char const* waveToString(Wave wave) {
	return kWaveNames[static_cast<int32_t>(wave) % kNumWaves];
}

bool waveFromString(char const* string, Wave& wave) {
	for (int32_t i = 0; i < kNumWaves; i++) {
		if (!strcmp(string, kWaveNames[i])) {
			wave = static_cast<Wave>(i);
			return true;
		}
	}
	return false;
}

float maxSpeedFor(uint32_t phaseIncrement) {
	return kNyquistShare * static_cast<float>(kQ32) / static_cast<float>(std::max<uint32_t>(phaseIncrement, 1));
}

Shape makeCzShape(Wave wave, float dcw, float maxSpeed) {
	Shape shape;
	dcw = std::clamp(dcw, 0.f, 1.f);
	const float ceiling = std::max(1.f, maxSpeed);

	if (!isPhaseWave(wave)) {
		const float k = std::min(std::pow(kMaxResonance, dcw), ceiling);
		shape.resonant = true;
		shape.window = wave;
		shape.resonance = static_cast<uint32_t>(std::lround(k * 65536.f));
		shape.dc = static_cast<int32_t>(std::lround(resonantMean(wave, k) * (1 << 30)));
		return shape;
	}

	MapBuilder map(shape);
	// How much faster the fast part runs. The hardware's DCW curve was not measured: the speed-up is spaced
	// exponentially, so an envelope on DCW brightens evenly. Each wave's cycle starts where the CZ-1 captures start it.
	const double s = std::min<double>(std::pow(kMaxSpeedup, dcw), ceiling);
	switch (wave) {
	case Wave::SAW: {
		// The first half of the cosine (peak to trough) rushed, the second half crawling back: a fast edge and a slow
		// ramp.
		const double d = 0.5 / s;
		map.through({{0, 0}, {d, 0.5}, {1, 1}});
		break;
	}
	case Wave::SQUARE: {
		// Rush to the trough, hold it, rush back to the peak, hold it. The holds sit on the cosine's peaks, so the
		// output holds too.
		const double r = 0.5 / s;
		const double h = 0.5 - r;
		map.through({{0, 0}, {r, 0.5}, {r + h, 0.5}, {2 * r + h, 1}, {1, 1}});
		break;
	}
	case Wave::PULSE: {
		// The whole cycle inside a shrinking window, parked on the peak either side: one narrow dip from a high
		// baseline, in the middle of the period as on the CZ-1.
		const double w = 1.0 / s;
		const double start = 0.5 - 0.5 * w;
		map.through({{0, 0}, {start, 0}, {start + w, 1}, {1, 1}});
		break;
	}
	case Wave::DOUBLE_SINE: {
		// The CZ-1's "sine pulse": the phase runs out to the trough and back within the dip, then takes one slow
		// cycle. DCW blends that map with the unbent phase, so the dip grows from nothing.
		const double w = kDoubleSineDip;
		const double fastest = 1.0 / w - 1.0; // extra speed of the dip at full depth
		const double depth = std::min<double>(dcw, (ceiling - 1.0) / fastest);
		auto bent = [&](double x, double g) { return x + depth * (g - x); };
		map.through({{0, 0}, {0.5 * w, bent(0.5 * w, 0.5)}, {w, bent(w, 0.0)}, {1, 1}});
		break;
	}
	case Wave::SAW_PULSE: {
		// A saw bent the other way (slow fall, fast rise) squeezed into the first part of the cycle, then parked on
		// the peak.
		const double active = 1.0 - kSawPulseMaxHold * dcw;
		const double fast = std::min<double>(std::pow(kMaxSpeedup, dcw), ceiling * active);
		const double knee = (1.0 - 0.5 / fast) * active;
		map.through({{0, 0}, {knee, 0.5}, {active, 1}, {1, 1}});
		break;
	}
	default:
		break;
	}
	return shape;
}

Shape makeVpsShape(float d, float dcw, float maxSpeed) {
	Shape shape;
	d = std::clamp(d, kVpsMinD, 1.f - kVpsMinD);
	dcw = std::clamp(dcw, 0.f, 1.f);
	const float ceiling = std::max(1.f, maxSpeed);
	// v = d is the unbent phase; above 1 it packs extra cycles into the first segment, and the second then runs
	// backwards to 1. Both segments' speeds are held under the ceiling.
	float v = d * std::pow(kVpsMaxV / d, dcw);
	v = std::min({v, d * ceiling, 1.f + ceiling * (1.f - d)});
	v = std::max(v, d);
	MapBuilder map(shape);
	map.through({{0, 0}, {d, v}, {1, 1}});
	return shape;
}

int32_t valueAt(const Shape& shape, uint32_t q) {
	return evaluate(shape, q);
}

void render(const RenderArgs& args, int32_t* buffer, int32_t numSamples, uint32_t* phase) {
	if (numSamples <= 0) {
		return;
	}
	const float maxSpeed = maxSpeedFor(args.phaseIncrement);
	const Wave wave = args.settings.wave;
	const bool combine = !args.vps && isPhaseWave(wave) && args.settings.combine < kNumPhaseWaves;
	const Wave partner = static_cast<Wave>(args.settings.combine);

	const float dcwStart = std::clamp(static_cast<float>(args.waveIndex) / 2147483648.f, 0.f, 1.f);
	const int64_t lastIndex =
	    static_cast<int64_t>(args.waveIndex) + static_cast<int64_t>(args.waveIndexIncrement) * numSamples;
	const float dcwEnd = std::clamp(static_cast<float>(lastIndex) / 2147483648.f, 0.f, 1.f);
	const bool morphing = dcwEnd != dcwStart;

	const int32_t pulseWidth = static_cast<int32_t>(args.pulseWidth);
	const float widthShare = std::min(std::fabs(static_cast<float>(pulseWidth)) / 2147483648.f, 1.f);
	const float inflection = 0.5f - (0.5f - kVpsMinD) * widthShare;
	auto shapeFor = [&](Wave w, float dcw) {
		return args.vps ? makeVpsShape(inflection, dcw, maxSpeed) : makeCzShape(w, dcw, maxSpeed);
	};

	// A buffer's DCW sweep crossfades between the shapes at its start and end, as a wavetable crossfades frames.
	Shape first[2];
	Shape second[2];
	first[0] = shapeFor(wave, dcwStart);
	if (morphing) {
		first[1] = shapeFor(wave, dcwEnd);
	}
	if (combine) {
		second[0] = shapeFor(partner, dcwStart);
		if (morphing) {
			second[1] = shapeFor(partner, dcwEnd);
		}
	}
	int32_t fade = 0;
	const int32_t fadeIncrement = morphing ? 2147483647 / numSamples : 0;

	// Wave combine runs the phase at half speed over the two glued cycles; its top bit picks the cycle.
	const uint32_t increment = combine ? (args.phaseIncrement >> 1) : args.phaseIncrement;
	const uint32_t readOffset = combine ? kCombinedReadOffset : kReadOffset;

	bool syncing = args.doOscSync;
	int32_t resetterDivideByPhaseIncrement = 0;
	if (syncing) {
		const uint16_t resetterIncrementRounded = (args.resetterPhaseIncrement + 65535) >> 16;
		syncing = resetterIncrementRounded != 0;
		if (syncing) {
			resetterDivideByPhaseIncrement = static_cast<int32_t>(2147483648u / resetterIncrementRounded);
		}
	}
	uint32_t resetterPhase = args.resetterPhase;

	uint32_t phaseNow = *phase;
	int32_t amplitude = args.amplitude << 1; // half-scale output, so twice the amplitude
	const int32_t amplitudeIncrement = args.amplitudeIncrement << 1;

	for (int32_t n = 0; n < numSamples; n++) {
		phaseNow += increment;
		if (syncing) {
			resetterPhase += args.resetterPhaseIncrement;
			if (resetterPhase < args.resetterPhaseIncrement) {
				phaseNow = (multiply_32x32_rshift32(multiply_32x32_rshift32(resetterPhase, increment),
				                                    resetterDivideByPhaseIncrement)
				            << 17)
				           + 1 + args.retriggerPhase;
			}
		}

		uint32_t q = phaseNow + readOffset;
		const Shape* shapes = first;
		if (combine) {
			if (q >> 31) {
				shapes = second;
			}
			q <<= 1;
		}

		int32_t value = evaluate(shapes[0], q);
		if (morphing) {
			const int64_t difference = static_cast<int64_t>(evaluate(shapes[1], q)) - value;
			value += static_cast<int32_t>((difference * fade) >> 31);
			fade += fadeIncrement;
		}

		if (args.applyAmplitude) {
			amplitude += amplitudeIncrement;
			buffer[n] = multiply_accumulate_32x32_rshift32_rounded(buffer[n], value, amplitude);
		}
		else {
			buffer[n] = value;
		}
	}
	*phase = phaseNow;
}

} // namespace deluge::dsp::phase_distortion
