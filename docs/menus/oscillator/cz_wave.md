The Casio CZ waveform a <string-for name="STRING_FOR_PHASE_DISTORTION">PHASE_DIST</string-for> oscillator plays.
How far the waveform is bent is set by <string-for name="STRING_FOR_DCW">DCW</string-for>, the oscillator's
[wave index](./wave_index.md); patch an envelope to it for the CZ's DCW envelope.

Can select from:
 - <string-for name="STRING_FOR_SAW">SAW</string-for>: a fast edge and a slow ramp, brightening towards a sawtooth.
 - <string-for name="STRING_FOR_SQUARE">SQUARE</string-for>: holds on the peaks; odd harmonics only.
 - <string-for name="STRING_FOR_CZ_PULSE">PULSE</string-for>: one narrow dip per cycle, thinning towards a pulse.
 - <string-for name="STRING_FOR_DOUBLE_SINE">DOUBLE_SINE</string-for>: a sine with a narrow extra dip at its peak.
 - <string-for name="STRING_FOR_SAW_PULSE">SAW_PULSE</string-for>: a saw that parks on its peak for part of the cycle.
 - <string-for name="STRING_FOR_RESO_SAW">RESO_SAW</string-for>,
   <string-for name="STRING_FOR_RESO_TRIANGLE">RESO_TRIANGLE</string-for> and
   <string-for name="STRING_FOR_RESO_TRAPEZOID">RESO_TRAPEZOID</string-for>: a resonant peak sweeping from 1 to 15 times
   the note under a falling, a rise-and-fall or a held-then-falling window.

The first five are a pure sine at DCW 0.
