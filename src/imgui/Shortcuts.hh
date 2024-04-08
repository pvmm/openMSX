#ifndef IMGUI_SHORTCUTS_HH
#define IMGUI_SHORTCUTS_HH

#include "ImGuiUtils.hh"

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
	STEP,
	BREAK,
	GOTO_DISASM_ADDRESS,
	MOVE_UP_MEMORY,
	MOVE_DOWN_MEMORY,
	MOVE_LEFT_MEMORY,
	MOVE_RIGHT_MEMORY,
	NUM_SHORTCUTS,
};

class Shortcuts final
{
public:
	Shortcuts(const Shortcuts&) = delete;
	Shortcuts& operator=(const Shortcuts&) = delete;
	explicit Shortcuts(SettingsConfig& config_);

	// Shortcuts
	struct Data {
		ImGuiKeyChord keyChord;
		bool local = false;
		bool repeat = false;
	};
	static const std::string_view getShortcutName(ShortcutIndex index);
	static const std::string_view getShortcutDescription(ShortcutIndex index);
	static std::optional<ShortcutIndex> getShortcutIndex(const std::string_view& name);
	Shortcuts::Data& getShortcut(ShortcutIndex index);
	void setShortcut(ShortcutIndex index, std::optional<ImGuiKeyChord> keyChord = {}, std::optional<bool> local = {}, std::optional<bool> repeat = {});
	void setDefaultShortcut(ShortcutIndex index);
	bool checkShortcut(ShortcutIndex index);

	template<typename XmlStream>
	void saveShortcuts(XmlStream& xml) const
	{
		int index = 0;
		xml.begin("shortcuts");
		for (const auto& data : shortcuts) {
			xml.begin("shortcut");
			if (auto name = getKeyChordName(data.keyChord)) {
				xml.attribute("keyChord", *name);
			}
			if (data.repeat) xml.attribute("repeat", "true");
			if (data.local) xml.attribute("local", "true");
			xml.data(getShortcutName(static_cast<ShortcutIndex>(index++)));
			xml.end("shortcut");
		}
		xml.end("shortcuts");
	}

	void setDefaultShortcuts();

private:
	// Shortcuts
	std::array<Shortcuts::Data, ShortcutIndex::NUM_SHORTCUTS> shortcuts;
};

} // namespace openmsx

#endif
