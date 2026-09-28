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

/* Casio CZ phase distortion and Vector Phaseshaping oscillators.
 *
 * A CZ oscillator reads a cosine through a bent phase: the phase climbs from 0 to 1 once per cycle, the waveform
 * picks how the climb is bent, and DCW (here the oscillator's wave index parameter) how far. The five phase waves
 * are a pure cosine at DCW 0; the three resonant waves are a cosine train at k times the note under a window that
 * closes towards the top peak, 1 - window * (1 - cos), with k sweeping from 1 to 15.
 *
 * The shapes follow the hardware: M. Rickard's captures and sysex study of a CZ-1 ("CZ Spelunking",
 * kasploosh.com/cz/11800-spelunking). They fix the cosine read, each wave's edges, holds and dip widths, where in its
 * cycle each wave starts, the resonant windows and how wave combine glues two cycles. Open-source ports were consulted
 * only as references. Not known from the hardware, and marked where used: how DCW steps between its ends, and whether
 * the double sine is the plain sine at DCW 0 (here it is).
 *
 * Vector Phaseshaping is J. Kleimola, V. Lazzarini, J. Timoney and V. Valimaki, "Vector Phaseshaping Synthesis",
 * DAFx 2011: the phase bends through one movable point (d, v), d coming from the oscillator's pulse width and v from
 * DCW.
 *
 * Output is at half scale (a cosine peaks at 2^30) with each shape's DC removed, as the CZ's AC-coupled output
 * does, so the deepest dips still fit. With the amplitude doubled, as for the saw tables, a phase wave at DCW 0
 * sounds exactly like the Deluge's sine oscillator, starting on the same zero crossing.
 */

#pragma once

#include <cstdint>

namespace deluge::dsp::phase_distortion {

/// The CZ-101's eight waveforms, in its panel order (WAVE 1 to 8).
enum class Wave : uint8_t {
	SAW,
	SQUARE,
	PULSE,
	DOUBLE_SINE,
	SAW_PULSE,
	RESO_SAW,
	RESO_TRIANGLE,
	RESO_TRAPEZOID,
};
constexpr int32_t kNumWaves = 8;
/// SAW to SAW_PULSE bend the phase; only these can be combined.
constexpr int32_t kNumPhaseWaves = 5;
constexpr uint8_t kNoCombine = 0xFF;

/// Which CZ waveform an oscillator plays, and the wave it alternates with each cycle (the CZ's wave combine).
struct Settings {
	Wave wave = Wave::SAW;
	uint8_t combine = kNoCombine; ///< a phase wave (0 to kNumPhaseWaves - 1), or kNoCombine
};

constexpr bool isPhaseWave(Wave wave) {
	return static_cast<int32_t>(wave) < kNumPhaseWaves;
}

/// The wave's name in preset files.
char const* waveToString(Wave wave);
/// Parses a name written by waveToString(); returns false, leaving `wave` alone, for anything else.
bool waveFromString(char const* string, Wave& wave);

/// One cycle's shape at a fixed DCW.
struct Shape {
	static constexpr int32_t kMaxSegments = 5;
	// Phase waves and VPS: a piecewise-linear phase map.
	int32_t numSegments = 0;
	uint32_t start[kMaxSegments]{}; ///< where each segment starts in the cycle, Q32; start[0] = 0
	uint32_t base[kMaxSegments]{};  ///< bent phase at the segment's start, Q32 turns (wraps)
	int32_t slope[kMaxSegments]{};  ///< Q20 turns per turn
	// Resonant waves.
	bool resonant = false;
	Wave window = Wave::RESO_SAW;
	uint32_t resonance = 1 << 16; ///< Q16 multiple of the note
	int32_t dc = 0;               ///< the shape's mean, half scale (Q30), removed from every sample
};

/// Highest phase speed (turns per turn) whose fundamental stays under 0.45 of the sample rate at this pitch.
float maxSpeedFor(uint32_t phaseIncrement);

/// A CZ wave at `dcw` (0 to 1), its bend held under `maxSpeed`.
Shape makeCzShape(Wave wave, float dcw, float maxSpeed);

/// Vector phaseshaping with the inflection at horizontal position `d` (0 to 1, exclusive), v swept by `dcw` from d
/// (an unbent cosine) up to 4.
Shape makeVpsShape(float d, float dcw, float maxSpeed);

/// The shape's value at cycle position `q` (Q32), half scale.
int32_t valueAt(const Shape& shape, uint32_t q);

/// Everything renderOsc() passes an oscillator, for a CZ or VPS oscillator.
struct RenderArgs {
	Settings settings;
	bool vps = false;
	int32_t amplitude = 0;
	int32_t amplitudeIncrement = 0;
	bool applyAmplitude = true;
	uint32_t phaseIncrement = 0;
	uint32_t pulseWidth = 0; ///< VPS inflection: 0 puts it mid-cycle, full width near the start
	uint32_t retriggerPhase = 0;
	bool doOscSync = false;
	uint32_t resetterPhase = 0;
	uint32_t resetterPhaseIncrement = 0;
	int32_t waveIndex = 0;          ///< DCW at the first sample, 0 to 2^31
	int32_t waveIndexIncrement = 0; ///< per sample
};

/// Renders numSamples into buffer (adding when args.applyAmplitude, else writing half-scale values), advancing
/// *phase.
void render(const RenderArgs& args, int32_t* buffer, int32_t numSamples, uint32_t* phase);

} // namespace deluge::dsp::phase_distortion
