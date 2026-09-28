/*
 * Copyright (c) 2014-2023 Synthstrom Audible Limited
 *
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
#include "gui/menu_item/formatted_title.h"
#include "gui/menu_item/source/patched_param.h"
#include "processing/sound/sound.h"

namespace deluge::gui::menu_item::osc::source {
class WaveIndex final : public menu_item::source::PatchedParam, public FormattedTitle {
public:
	WaveIndex(l10n::String name, l10n::String title_format_str, int32_t newP, uint8_t source_id)
	    : PatchedParam(name, newP, source_id), FormattedTitle(title_format_str, source_id + 1) {}

	[[nodiscard]] std::string_view getTitle() const override {
		return isDcw() ? dcwTitle_.title() : FormattedTitle::title();
	}

	// For phase distortion and vector phaseshaping, the wave index is the CZ's DCW.
	[[nodiscard]] std::string_view getName() const override {
		return isDcw() ? l10n::getView(l10n::String::STRING_FOR_DCW) : PatchedParam::getName();
	}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		const auto sound = static_cast<Sound*>(modControllable);
		auto& source = sound->sources[source_id_];
		if (sound->getSynthMode() == SynthMode::FM) {
			return false;
		}
		return (source.oscType == OscType::WAVETABLE && source.hasAtLeastOneAudioFileLoaded())
		       || source.oscType == OscType::PHASE_DIST || source.oscType == OscType::VPS;
	}

	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return SLIDER; }

private:
	[[nodiscard]] bool isDcw() const {
		if (soundEditor.currentSound == nullptr) {
			return false;
		}
		const OscType type = soundEditor.currentSound->sources[source_id_].oscType;
		return type == OscType::PHASE_DIST || type == OscType::VPS;
	}

	FormattedTitle dcwTitle_{l10n::String::STRING_FOR_OSC_DCW_MENU_TITLE, source_id_ + 1};
};
} // namespace deluge::gui::menu_item::osc::source
