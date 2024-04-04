#ifndef IMGUI_SHORTCUTS_HH
#define IMGUI_SHORTCUTS_HH

#include "ImGuiUtils.hh"
//#include "ImGuiManager.hh"

#include "strCat.hh"
#include "imgui_stdlib.h"

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace openmsx {

class ImGuiManager;
class GlobalSettings;
class SettingsConfig;

enum ShortcutIndex {
	GOTO_MEMORY_ADDRESS = 0,
	GOTO_DISASM_ADDRESS,
	NUM_SHORTCUTS,
};

enum ShortcutType {
	LOCAL,    // if ImGui widget has focus
	GLOBAL,   // inside ImGui layer only
};

struct ShortcutData {
	ShortcutIndex index;
	ImGuiKeyChord keychord;
	bool local = false;
	bool repeat = false;
};

class ImGuiShortcuts final
{
public:
	// userlevel shortcut type
	using KeyChordSet = std::vector<ImGuiKeyChord>;   // unsorted

	ImGuiShortcuts(const ImGuiShortcuts&) = delete;
	ImGuiShortcuts& operator=(const ImGuiShortcuts&) = delete;
	explicit ImGuiShortcuts(SettingsConfig& config_);

	// Shortcuts
	static std::string getShortcutName(ShortcutIndex index);
	ShortcutData& getShortcut(ShortcutIndex index);
	void setShortcut(ShortcutIndex index, ImGuiKeyChord keychord, std::optional<bool> local = {}, std::optional<bool> repeat = {});
	bool isPressed(ShortcutIndex index);

        template<typename XmlStream>
        void saveShortcuts(XmlStream& xml) const
        {
                xml.begin("shortcuts");
                for (const auto& data : shortcuts) {
			xml.begin("shortcut");
			if (data.keychord != ImGuiKey_None) {
				xml.attribute("keychord", std::to_string(static_cast<int>(data.keychord)));
			}
			if (data.repeat) xml.attribute("repeat", "true");
			if (data.local) xml.attribute("local", "true");
			xml.data(std::to_string(static_cast<int>(data.index)));
			xml.end("shortcut");
                }
		xml.end("shortcuts");
	}

	void loadInit();

private:
	//[[maybe_unused]] ImGuiManager& manager;
	//[[maybe_unused]] GlobalSettings& settings;
	[[maybe_unused]] SettingsConfig& config;

	// Shortcuts
	std::array<ShortcutData, ShortcutIndex::NUM_SHORTCUTS> shortcuts;
	KeyChordSet boundShortcuts;     // different than default
	KeyChordSet unboundShortcuts;   // removed from default
};

} // namespace openmsx

#endif
