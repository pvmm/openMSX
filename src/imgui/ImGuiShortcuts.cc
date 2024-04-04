#include "ImGuiShortcuts.hh"
#include "imgui_internal.h"

#include <array>

namespace openmsx {

static constexpr auto defaultShortcuts = std::array{
	ImGuiShortcuts::ShortcutData { ImGuiMod_Ctrl | ImGuiKey_G, 0 },	  // MEMORY_GOTO_ADDRESS
	ImGuiShortcuts::ShortcutData { ImGuiMod_None, 0 },                // DISASM_GOTO_ADDRESS
};

ImGuiShortcuts::ImGuiShortcuts(ImGuiManager& manager_)
	: manager(manager_)
{
	initShortcuts();
}

void ImGuiShortcuts::initShortcuts()
{
	// check ini file.
	// populate shortcuts
	// populate unboundShortcuts and boundShortcuts.
}

ImGuiShortcuts::ShortcutData ImGuiShortcuts::getShortcut(ShortcutIndex index)
{
	return shortcuts[index];
}

void ImGuiShortcuts::setShortcut(ShortcutIndex index, ImGuiKeyChord keychord, int flags)
{
	shortcuts[index] = ImGuiShortcuts::ShortcutData{keychord, flags};
}

bool ImGuiShortcuts::isPressed(ShortcutIndex index)
{
	// what kind of flag should be tested?
	return shortcuts[index].keychord && shortcuts[index].flags != ImGuiInputFlags_None;
}

}