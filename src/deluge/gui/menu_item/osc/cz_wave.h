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

/// The Casio CZ waveform a phase-distortion oscillator plays.
class CzWave final : public Selection, public FormattedTitle {
public:
	CzWave(l10n::String name, l10n::String title_format_str, uint8_t source_id)
	    : Selection(name), FormattedTitle(title_format_str, source_id + 1), source_id_{source_id} {}

	[[nodiscard]] std::string_view getTitle() const override { return FormattedTitle::title(); }

	void readCurrentValue() override {
		setValue(util::to_underlying(soundEditor.currentSound->sources[source_id_].pdSettings.wave));
	}

	void writeCurrentValue() override {
		soundEditor.currentSound->sources[source_id_].pdSettings.wave = getValue<dsp::phase_distortion::Wave>();
	}

	deluge::vector<std::string_view> getOptions(OptType optType) override {
		(void)optType;
		using enum l10n::String;
		return {
		    l10n::getView(STRING_FOR_SAW),           l10n::getView(STRING_FOR_SQUARE),
		    l10n::getView(STRING_FOR_CZ_PULSE),      l10n::getView(STRING_FOR_DOUBLE_SINE),
		    l10n::getView(STRING_FOR_SAW_PULSE),     l10n::getView(STRING_FOR_RESO_SAW),
		    l10n::getView(STRING_FOR_RESO_TRIANGLE), l10n::getView(STRING_FOR_RESO_TRAPEZOID),
		};
	}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		Sound* sound = static_cast<Sound*>(modControllable);
		return sound->getSynthMode() != SynthMode::FM && sound->sources[source_id_].oscType == OscType::PHASE_DIST;
	}

private:
	uint8_t source_id_;
};

} // namespace deluge::gui::menu_item::osc
