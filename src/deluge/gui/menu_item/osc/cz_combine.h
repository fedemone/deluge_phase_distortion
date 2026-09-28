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
#pragma once
#include "definitions_cxx.hpp"
#include "dsp/oscillators/phase_distortion.h"
#include "gui/menu_item/formatted_title.h"
#include "gui/menu_item/selection.h"
#include "gui/ui/sound_editor.h"
#include "processing/sound/sound.h"

namespace deluge::gui::menu_item::osc {

/// The CZ's wave combine: a second waveform the oscillator alternates with each cycle, or off.
class CzCombine final : public Selection, public FormattedTitle {
public:
	CzCombine(l10n::String name, l10n::String title_format_str, uint8_t source_id)
	    : Selection(name), FormattedTitle(title_format_str, source_id + 1), source_id_{source_id} {}

	[[nodiscard]] std::string_view getTitle() const override { return FormattedTitle::title(); }

	// Option 0 is off; options 1 to 5 are the five waves that bend the phase.
	void readCurrentValue() override {
		const uint8_t combine = soundEditor.currentSound->sources[source_id_].pdSettings.combine;
		setValue(combine < dsp::phase_distortion::kNumPhaseWaves ? combine + 1 : 0);
	}

	void writeCurrentValue() override {
		const int32_t value = getValue();
		soundEditor.currentSound->sources[source_id_].pdSettings.combine =
		    value == 0 ? dsp::phase_distortion::kNoCombine : static_cast<uint8_t>(value - 1);
	}

	deluge::vector<std::string_view> getOptions(OptType optType) override {
		(void)optType;
		using enum l10n::String;
		return {
		    l10n::getView(STRING_FOR_OFF),         l10n::getView(STRING_FOR_SAW),
		    l10n::getView(STRING_FOR_SQUARE),      l10n::getView(STRING_FOR_CZ_PULSE),
		    l10n::getView(STRING_FOR_DOUBLE_SINE), l10n::getView(STRING_FOR_SAW_PULSE),
		};
	}

	// Like the CZ, only the phase-bending waves combine; the resonant ones use their own window.
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		Sound* sound = static_cast<Sound*>(modControllable);
		const Source& source = sound->sources[source_id_];
		return sound->getSynthMode() != SynthMode::FM && source.oscType == OscType::PHASE_DIST
		       && dsp::phase_distortion::isPhaseWave(source.pdSettings.wave);
	}

private:
	uint8_t source_id_;
};

} // namespace deluge::gui::menu_item::osc
