#include "CppUTest/TestHarness.h"
#include "dsp/oscillators/phase_distortion.h"
#include "util/waves.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace pd = deluge::dsp::phase_distortion;

namespace {

struct CzReference {
	int wave;
	float dcw;
	double p;
	double value;
};
struct VpsReference {
	float d;
	float v;
	double p;
	double value;
};

// Reference values from an independent floating-point (Python) implementation of the same model:
// {wave, dcw, cycle position, value before DC removal}.
const CzReference kCzReference[] = {
    {0, 0.0f, 0.013, 0.996665928},  {0, 0.0f, 0.27, -0.125333234}, {0, 0.0f, 0.5, -1.000000000},
    {0, 0.0f, 0.61, -0.770513243},  {0, 0.0f, 0.97, 0.982287251},  {0, 0.3f, 0.013, 0.977074752},
    {0, 0.3f, 0.27, -0.952637251},  {0, 0.3f, 0.5, -0.360994046},  {0, 0.3f, 0.61, 0.057463315},
    {0, 0.3f, 0.97, 0.993232260},   {0, 0.7f, 0.013, 0.712693588}, {0, 0.7f, 0.27, -0.751093754},
    {0, 0.7f, 0.5, -0.086979625},   {0, 0.7f, 0.61, 0.274091836},  {0, 0.7f, 0.97, 0.995056623},
    {0, 1.0f, 0.013, -0.453990500}, {0, 1.0f, 0.27, -0.695682551}, {0, 1.0f, 0.5, -0.032051578},
    {0, 1.0f, 0.61, 0.315108218},   {0, 1.0f, 0.97, 0.995379113},  {1, 0.0f, 0.013, 0.996665928},
    {1, 0.0f, 0.27, -0.125333234},  {1, 0.0f, 0.5, -1.000000000},  {1, 0.0f, 0.61, -0.770513243},
    {1, 0.0f, 0.97, 0.982287251},   {1, 0.3f, 0.013, 0.977074752}, {1, 0.3f, 0.27, -1.000000000},
    {1, 0.3f, 0.5, -1.000000000},   {1, 0.3f, 0.61, 0.242099725},  {1, 0.3f, 0.97, 1.000000000},
    {1, 0.7f, 0.013, 0.712693588},  {1, 0.7f, 0.27, -1.000000000}, {1, 0.7f, 0.5, -1.000000000},
    {1, 0.7f, 0.61, 1.000000000},   {1, 0.7f, 0.97, 1.000000000},  {1, 1.0f, 0.013, -0.453990500},
    {1, 1.0f, 0.27, -1.000000000},  {1, 1.0f, 0.5, -1.000000000},  {1, 1.0f, 0.61, 1.000000000},
    {1, 1.0f, 0.97, 1.000000000},   {2, 0.0f, 0.013, 0.996665928}, {2, 0.0f, 0.27, -0.125333234},
    {2, 0.0f, 0.5, -1.000000000},   {2, 0.0f, 0.61, -0.770513243}, {2, 0.0f, 0.97, 0.982287251},
    {2, 0.3f, 0.013, 1.000000000},  {2, 0.3f, 0.27, 1.000000000},  {2, 0.3f, 0.5, -1.000000000},
    {2, 0.3f, 0.61, 0.242099725},   {2, 0.3f, 0.97, 1.000000000},  {2, 0.7f, 0.013, 1.000000000},
    {2, 0.7f, 0.27, 1.000000000},   {2, 0.7f, 0.5, -1.000000000},  {2, 0.7f, 0.61, 1.000000000},
    {2, 0.7f, 0.97, 1.000000000},   {2, 1.0f, 0.013, 1.000000000}, {2, 1.0f, 0.27, 1.000000000},
    {2, 1.0f, 0.5, -1.000000000},   {2, 1.0f, 0.61, 1.000000000},  {2, 1.0f, 0.97, 1.000000000},
    {3, 0.0f, 0.013, 0.996665928},  {3, 0.0f, 0.27, -0.125333234}, {3, 0.0f, 0.5, -1.000000000},
    {3, 0.0f, 0.61, -0.770513243},  {3, 0.0f, 0.97, 0.982287251},  {3, 0.3f, 0.013, 0.783953574},
    {3, 0.3f, 0.27, -0.068276481},  {3, 0.3f, 0.5, -0.999229036},  {3, 0.3f, 0.61, -0.789673393},
    {3, 0.3f, 0.97, 0.981843018},   {3, 0.7f, 0.013, 0.116601400}, {3, 0.7f, 0.27, 0.008115692},
    {3, 0.7f, 0.5, -0.995804928},   {3, 0.7f, 0.61, -0.814064839}, {3, 0.7f, 0.97, 0.981242228},
    {3, 1.0f, 0.013, -0.453990500}, {3, 1.0f, 0.27, 0.065403129},  {3, 1.0f, 0.5, -0.991444861},
    {3, 1.0f, 0.61, -0.831469612},  {3, 1.0f, 0.97, 0.980785280},  {4, 0.0f, 0.013, 0.996665928},
    {4, 0.0f, 0.27, -0.125333234},  {4, 0.0f, 0.5, -1.000000000},  {4, 0.0f, 0.61, -0.770513243},
    {4, 0.0f, 0.97, 0.982287251},   {4, 0.3f, 0.013, 0.998264178}, {4, 0.3f, 0.27, 0.339967736},
    {4, 0.3f, 0.5, -0.640931052},   {4, 0.3f, 0.61, -0.929974001}, {4, 0.3f, 0.97, 1.000000000},
    {4, 0.7f, 0.013, 0.997893617},  {4, 0.7f, 0.27, 0.220683842},  {4, 0.7f, 0.5, -0.799234182},
    {4, 0.7f, 0.61, -0.995445551},  {4, 0.7f, 0.97, 1.000000000},  {4, 1.0f, 0.013, 0.996790295},
    {4, 1.0f, 0.27, -0.093568203},  {4, 1.0f, 0.5, -0.998249240},  {4, 1.0f, 0.61, 1.000000000},
    {4, 1.0f, 0.97, 1.000000000},   {5, 0.0f, 0.013, 0.996709271}, {5, 0.0f, 0.27, 0.178506739},
    {5, 0.0f, 0.5, 0.000000000},    {5, 0.0f, 0.61, 0.309499835},  {5, 0.0f, 0.97, 0.999468618},
    {5, 0.3f, 0.013, 0.983328988},  {5, 0.3f, 0.27, -0.297116233}, {5, 0.3f, 0.5, 0.849820394},
    {5, 0.3f, 0.61, 0.335027494},   {5, 0.3f, 0.97, 0.981785760},  {5, 0.7f, 0.013, 0.857657354},
    {5, 0.7f, 0.27, 0.483900814},   {5, 0.7f, 0.5, 0.263574589},   {5, 0.7f, 0.61, 0.972039153},
    {5, 0.7f, 0.97, 0.941084680},   {5, 1.0f, 0.013, 0.347334327}, {5, 1.0f, 0.27, 0.964271257},
    {5, 1.0f, 0.5, 0.000000000},    {5, 1.0f, 0.61, 0.839236248},  {5, 1.0f, 0.97, 0.941468305},
    {6, 0.0f, 0.013, 0.999913314},  {6, 0.0f, 0.27, 0.392320054},  {6, 0.0f, 0.5, -1.000000000},
    {6, 0.0f, 0.61, -0.381000329},  {6, 0.0f, 0.97, 0.998937235},  {6, 0.3f, 0.013, 0.999560845},
    {6, 0.3f, 0.27, 0.040489362},   {6, 0.3f, 0.5, 0.699640788},   {6, 0.3f, 0.61, -0.329945013},
    {6, 0.3f, 0.97, 0.963571520},   {6, 0.7f, 0.013, 0.996250346}, {6, 0.7f, 0.27, 0.618227999},
    {6, 0.7f, 0.5, -0.472850822},   {6, 0.7f, 0.61, 0.944078306},  {6, 0.7f, 0.97, 0.882169361},
    {6, 1.0f, 0.013, 0.982807186},  {6, 1.0f, 0.27, 0.973570519},  {6, 1.0f, 0.5, -1.000000000},
    {6, 1.0f, 0.61, 0.678472497},   {6, 1.0f, 0.97, 0.882936609},  {7, 0.0f, 0.013, 0.996665928},
    {7, 0.0f, 0.27, -0.125333234},  {7, 0.0f, 0.5, -1.000000000},  {7, 0.0f, 0.61, -0.381000329},
    {7, 0.0f, 0.97, 0.998937235},   {7, 0.3f, 0.013, 0.983109410}, {7, 0.3f, 0.27, -0.776871552},
    {7, 0.3f, 0.5, 0.699640788},    {7, 0.3f, 0.61, -0.329945013}, {7, 0.3f, 0.97, 0.963571520},
    {7, 0.7f, 0.013, 0.855782527},  {7, 0.7f, 0.27, 0.293014813},  {7, 0.7f, 0.5, -0.472850822},
    {7, 0.7f, 0.61, 0.944078306},   {7, 0.7f, 0.97, 0.882169361},  {7, 1.0f, 0.013, 0.338737920},
    {7, 1.0f, 0.27, 0.951056516},   {7, 1.0f, 0.5, -1.000000000},  {7, 1.0f, 0.61, 0.678472497},
    {7, 1.0f, 0.97, 0.882936609},
};
const VpsReference kVpsReference[] = {
    {0.5f, 0.5f, 0.013, 0.996665928}, {0.5f, 0.5f, 0.27, -0.125333234},  {0.5f, 0.5f, 0.5, -1.000000000},
    {0.5f, 0.5f, 0.61, -0.770513243}, {0.5f, 0.5f, 0.97, 0.982287251},   {0.25f, 2.0f, 0.013, 0.793990399},
    {0.25f, 2.0f, 0.27, 0.985996037}, {0.25f, 2.0f, 0.5, -0.500000000},  {0.25f, 2.0f, 0.61, -0.992114701},
    {0.25f, 2.0f, 0.97, 0.968583161}, {0.1f, 4.0f, 0.013, -0.992114701}, {0.1f, 4.0f, 0.27, -0.913545458},
    {0.1f, 4.0f, 0.5, -0.500000000},  {0.1f, 4.0f, 0.61, -0.309016994},  {0.1f, 4.0f, 0.97, 0.809016994},
    {0.7f, 1.5f, 0.013, 0.984721038}, {0.7f, 1.5f, 0.27, -0.880595532},  {0.7f, 1.5f, 0.5, 0.900968868},
    {0.7f, 1.5f, 0.61, -0.351374824}, {0.7f, 1.5f, 0.97, 0.951056516},
};

constexpr float kNoCeiling = 1e6f;
constexpr double kHalfScale = 1073741824.0; // where a half-scale cosine peaks
constexpr uint32_t kReadOffset = 3u << 30;

uint32_t q32(double p) {
	return static_cast<uint32_t>(p * 4294967296.0);
}

/// Back to the model's -1..1 units, with the shape's DC restored.
double rawValue(const pd::Shape& shape, double p) {
	return (pd::valueAt(shape, q32(p)) + shape.dc) / kHalfScale;
}

std::vector<double> cycle(const pd::Shape& shape, int n) {
	std::vector<double> out(n);
	for (int i = 0; i < n; i++) {
		out[i] = pd::valueAt(shape, static_cast<uint32_t>((static_cast<uint64_t>(i) << 32) / n)) / kHalfScale;
	}
	return out;
}

double harmonic(const std::vector<double>& x, int k) {
	double re = 0, im = 0;
	const double n = static_cast<double>(x.size());
	for (size_t i = 0; i < x.size(); i++) {
		re += x[i] * std::cos(2 * M_PI * k * i / n);
		im += x[i] * std::sin(2 * M_PI * k * i / n);
	}
	return 2 * std::hypot(re, im) / n;
}

pd::Shape shapeOf(int wave, float dcw) {
	return pd::makeCzShape(static_cast<pd::Wave>(wave), dcw, kNoCeiling);
}

pd::RenderArgs rawArgs(pd::Settings settings, uint32_t phaseIncrement, float dcw) {
	pd::RenderArgs args;
	args.settings = settings;
	args.applyAmplitude = false;
	args.phaseIncrement = phaseIncrement;
	args.waveIndex = static_cast<int32_t>(std::lround(dcw * 2147483647.0));
	return args;
}

} // namespace

TEST_GROUP(PhaseDistortion){};

TEST(PhaseDistortion, czWavesMatchTheReferenceModel) {
	for (const auto& r : kCzReference) {
		DOUBLES_EQUAL(r.value, rawValue(shapeOf(r.wave, r.dcw), r.p), 2e-3);
	}
}

TEST(PhaseDistortion, vpsMatchesTheReferenceModel) {
	for (const auto& r : kVpsReference) {
		// v = d * (4 / d) ^ dcw
		const float dcw = std::log(r.v / r.d) / std::log(4.0f / r.d);
		DOUBLES_EQUAL(r.value, rawValue(pd::makeVpsShape(r.d, dcw, kNoCeiling), r.p), 2e-3);
	}
}

TEST(PhaseDistortion, fullDcwMatchesTheCz1Captures) {
	// Measured on the CZ-1 captures, 51 pixels a period from its start marker.
	const auto pulse = cycle(shapeOf(static_cast<int>(pd::Wave::PULSE), 1.f), 1000);
	int first = -1, last = -1;
	for (int i = 0; i < 1000; i++) {
		if (pulse[i] < 0.0) {
			first = first < 0 ? i : first;
			last = i;
		}
	}
	CHECK(first > 470 && last < 530); // one dip, about 4% wide, in the middle of the period

	const pd::Shape sawPulse = shapeOf(static_cast<int>(pd::Wave::SAW_PULSE), 1.f);
	const double held = rawValue(sawPulse, 0.53);
	DOUBLES_EQUAL(1.0, held, 1e-3);                // holding on its peak for the last 48%
	CHECK(rawValue(sawPulse, 0.49) < -0.9);        // after falling for half the period
	CHECK(rawValue(shapeOf(0, 1.f), 0.03) < -0.9); // the saw's edge is over within 2%
	DOUBLES_EQUAL(-1.0, rawValue(shapeOf(static_cast<int>(pd::Wave::DOUBLE_SINE), 1.f), 0.02),
	              1e-3); // dip at the start
}

TEST(PhaseDistortion, phaseWavesAtDcwZeroAreTheDelugeSine) {
	constexpr uint32_t increment = 12345678;
	for (int wave = 0; wave < pd::kNumPhaseWaves; wave++) {
		int32_t buffer[256];
		uint32_t phase = 0;
		pd::render(rawArgs({static_cast<pd::Wave>(wave)}, increment, 0.f), buffer, 256, &phase);
		for (int n = 0; n < 256; n++) {
			const int32_t sine = getSine((n + 1) * increment) >> 1;
			CHECK(std::abs(buffer[n] - sine) <= 64);
		}
		CHECK_EQUAL(256 * increment, phase);
	}
}

TEST(PhaseDistortion, dcIsRemoved) {
	for (int wave = 0; wave < pd::kNumWaves; wave++) {
		for (float dcw : {0.3f, 0.75f, 1.0f}) {
			double sum = 0;
			for (double x : cycle(shapeOf(wave, dcw), 4096)) {
				sum += x;
			}
			DOUBLES_EQUAL(0.0, sum / 4096, 2e-3);
		}
	}
}

TEST(PhaseDistortion, cyclesJoinWithoutAClick) {
	for (int wave = 0; wave < pd::kNumWaves; wave++) {
		for (float dcw : {0.f, 0.4f, 0.85f, 1.f}) {
			const pd::Shape shape = shapeOf(wave, dcw);
			DOUBLES_EQUAL(pd::valueAt(shape, 0) / kHalfScale, pd::valueAt(shape, 0xFFFFFFFFu) / kHalfScale, 2e-3);
		}
	}
}

TEST(PhaseDistortion, squareHasOnlyOddHarmonics) {
	for (float dcw : {0.3f, 0.7f, 1.f}) {
		const auto samples = cycle(shapeOf(static_cast<int>(pd::Wave::SQUARE), dcw), 1024);
		const double fundamental = harmonic(samples, 1);
		for (int k : {2, 4, 6, 8}) {
			CHECK(harmonic(samples, k) < 1e-3 * fundamental);
		}
	}
}

TEST(PhaseDistortion, resonantPeakFollowsDcw) {
	for (int wave = pd::kNumPhaseWaves; wave < pd::kNumWaves; wave++) {
		for (int k : {4, 8, 15}) {
			const float dcw = std::log(static_cast<float>(k)) / std::log(15.f);
			const auto samples = cycle(shapeOf(wave, dcw), 1024);
			int peak = 2;
			for (int h = 3; h <= 40; h++) {
				if (harmonic(samples, h) > harmonic(samples, peak)) {
					peak = h;
				}
			}
			CHECK_EQUAL(k, peak);
		}
	}
}

TEST(PhaseDistortion, bendStaysUnderNyquist) {
	const uint32_t increment = static_cast<uint32_t>(4000.0 / 44100.0 * 4294967296.0); // 4 kHz
	const float ceiling = pd::maxSpeedFor(increment);
	CHECK(ceiling > 4.9f && ceiling < 5.0f);
	for (int wave = 0; wave < pd::kNumWaves; wave++) {
		const pd::Shape shape = pd::makeCzShape(static_cast<pd::Wave>(wave), 1.f, ceiling);
		if (shape.resonant) {
			CHECK(shape.resonance <= static_cast<uint32_t>(ceiling * 65536.f) + 1);
		}
		for (int i = 0; i < shape.numSegments; i++) {
			CHECK(std::abs(shape.slope[i]) <= static_cast<int32_t>(ceiling * 1048576.f) + 1);
		}
	}
	const pd::Shape vps = pd::makeVpsShape(0.1f, 1.f, ceiling);
	for (int i = 0; i < vps.numSegments; i++) {
		CHECK(std::abs(vps.slope[i]) <= static_cast<int32_t>(ceiling * 1048576.f) + 1);
	}
}

TEST(PhaseDistortion, waveCombineAlternatesCycles) {
	constexpr uint32_t increment = 1u << 23; // 512 samples per cycle
	pd::Settings settings{pd::Wave::SAW, static_cast<uint8_t>(pd::Wave::SQUARE)};
	int32_t buffer[2048];
	uint32_t phase = 0;
	pd::render(rawArgs(settings, increment, 1.f), buffer, 2048, &phase);
	const pd::Shape saw = pd::makeCzShape(pd::Wave::SAW, 1.f, pd::maxSpeedFor(increment));
	const pd::Shape square = pd::makeCzShape(pd::Wave::SQUARE, 1.f, pd::maxSpeedFor(increment));
	int differing = 0;
	for (int n = 0; n < 2048; n++) {
		// the phase runs at half speed over the pair of cycles; its top bit picks the wave
		const uint32_t pair = (n + 1) * (increment >> 1) + (kReadOffset >> 1);
		const pd::Shape& expected = (pair >> 31) ? square : saw;
		CHECK_EQUAL(pd::valueAt(expected, pair << 1), buffer[n]);
		differing += pd::valueAt(saw, pair << 1) != pd::valueAt(square, pair << 1);
	}
	CHECK(differing > 1000);
	CHECK_EQUAL(2048 * (increment >> 1), phase);
}

TEST(PhaseDistortion, amplitudeIsAddedAtTheSineOscillatorsLevel) {
	pd::RenderArgs args = rawArgs({pd::Wave::SAW}, 12345678, 0.f);
	args.applyAmplitude = true;
	args.amplitude = 1 << 28;
	int32_t buffer[64];
	for (int32_t& x : buffer) {
		x = 1000;
	}
	uint32_t phase = 0;
	pd::render(args, buffer, 64, &phase);
	for (int n = 0; n < 64; n++) {
		// the sine oscillator adds sine * amplitude >> 32
		const int64_t sine = getSine((n + 1) * 12345678u);
		const int64_t expected = 1000 + ((sine * args.amplitude) >> 32);
		CHECK(std::llabs(buffer[n] - expected) <= 2);
	}
}

TEST(PhaseDistortion, dcwSweepCrossfadesAcrossTheBuffer) {
	constexpr int n = 128;
	constexpr uint32_t increment = 1u << 22;
	pd::RenderArgs args = rawArgs({pd::Wave::PULSE}, increment, 0.f);
	args.waveIndexIncrement = 2147483647 / n;
	int32_t buffer[n];
	uint32_t phase = 0;
	pd::render(args, buffer, n, &phase);
	const float ceiling = pd::maxSpeedFor(increment);
	const pd::Shape start = pd::makeCzShape(pd::Wave::PULSE, 0.f, ceiling);
	const pd::Shape end = pd::makeCzShape(pd::Wave::PULSE, 1.f, ceiling);
	CHECK_EQUAL(pd::valueAt(start, increment + kReadOffset), buffer[0]);
	const int32_t last = pd::valueAt(end, n * increment + kReadOffset);
	CHECK(std::abs(buffer[n - 1] - last) < (1 << 24));
}

TEST(PhaseDistortion, oscSyncRestartsTheCycle) {
	pd::RenderArgs args = rawArgs({pd::Wave::SAW}, 1u << 24, 0.5f);
	args.doOscSync = true;
	args.resetterPhaseIncrement = 3u << 24;
	args.resetterPhase = 0u - 2 * args.resetterPhaseIncrement - 5; // wraps on the third sample
	args.retriggerPhase = 0;
	int32_t buffer[8];
	uint32_t phase = 0x40000000u;
	pd::render(args, buffer, 8, &phase);
	CHECK(phase < 7 * args.phaseIncrement);
}

TEST(PhaseDistortion, waveNamesRoundTrip) {
	for (int wave = 0; wave < pd::kNumWaves; wave++) {
		pd::Wave parsed = pd::Wave::SAW;
		CHECK(pd::waveFromString(pd::waveToString(static_cast<pd::Wave>(wave)), parsed));
		CHECK_EQUAL(wave, static_cast<int>(parsed));
	}
	pd::Wave untouched = pd::Wave::PULSE;
	CHECK_FALSE(pd::waveFromString("triangle", untouched));
	CHECK_EQUAL(static_cast<int>(pd::Wave::PULSE), static_cast<int>(untouched));
}
