#include "ImGuiShortcuts.hh"
#include "imgui_internal.h"

#include "SettingsConfig.hh"

#include <array>

namespace openmsx {

static constexpr auto defaultShortcuts = std::array{
	ShortcutData{ ShortcutIndex::GOTO_MEMORY_ADDRESS, ImGuiMod_Ctrl | ImGuiKey_G, ShortcutType::LOCAL },
	ShortcutData{ ShortcutIndex::GOTO_DISASM_ADDRESS, ImGuiMod_None, ShortcutType::LOCAL },
};

ImGuiShortcuts::ImGuiShortcuts(SettingsConfig& config_)
	: config(config_)
	, shortcuts{}
{
	config.setShortcuts(*this);
	loadInit();
}

void ImGuiShortcuts::loadInit()
{
	for (auto& shortcut : defaultShortcuts) {
		setShortcut(shortcut.index, shortcut.keychord, shortcut.local, shortcut.repeat);
	}
}

ShortcutData& ImGuiShortcuts::getShortcut(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	return shortcuts[index];
}

void ImGuiShortcuts::setShortcut(ShortcutIndex index, ImGuiKeyChord keychord, std::optional<bool> local, std::optional<bool> repeat)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	shortcuts[index].index    = index;
	shortcuts[index].keychord = keychord;
	if (local) shortcuts[index].local = *local;
	if (repeat) shortcuts[index].repeat = *repeat;
}

std::string ImGuiShortcuts::getShortcutName(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	static constexpr auto shortcutNames = std::array{
		"GotoMemoryAddress", "GotoDisasmAddress"
	};
	return shortcutNames[index];
}

bool ImGuiShortcuts::isPressed(ShortcutIndex index)
{
	// what kind of flag should be tested?
	return shortcuts[index].keychord != ImGuiKey_None;
}

}
