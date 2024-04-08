#include "Shortcuts.hh"
#include "imgui_internal.h"

#include "SettingsConfig.hh"

#include <array>

namespace openmsx {

//-- (Only) the table in this section will be edited when adding new shortcuts

struct AllShortcutInfo {
    ShortcutIndex index;
    ImGuiKeyChord keyChord;
    bool local;
    bool repeat;
    std::string_view name; // used in settings.xml
    std::string_view description; // shown in GUI
};

// When adding a new Shortcut, we only:
//  * Add a new value in the above 'enum ShortcutIndex'
//  * Add a single line in this table
// Note: if you look at the generated code on the right, this table does NOT end up in the generated code
//       instead it's only used to calculate various other tables (that do end up in the generated code)
//       those 'calculations' are all done at compile-time
// Note: the first column is ONLY used to verify (at-compile-time) whether the rows in this
//       table are in the same order as the values in ShortcutIndex
static constexpr auto allShortcutInfo = std::to_array<AllShortcutInfo>({
    {GOTO_MEMORY_ADDRESS, ImGuiMod_Ctrl | ImGuiKey_G,                  true,   false, "goto_memory_address",       "Go to address in hex viewer"},
    {STEP,                ImGuiKey_F6,                                 true,   true,  "step",                      "Single step in debugger"},
    {BREAK,               ImGuiKey_F7,                                 true,   false, "break",                     "Break CPU emulation in debugger"},
    {GOTO_DISASM_ADDRESS, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_G, true,   false, "goto_disasm_address",       "Go to address in dissassembler"},
    {MOVE_UP_MEMORY,      ImGuiKey_UpArrow,                            false,  true,  "move_up_memory_address",    "Move up in hex viewer"},
    {MOVE_DOWN_MEMORY,    ImGuiKey_DownArrow,                          false,  true,  "move_down_memory_address",  "Move down in hex viewer"},
    {MOVE_LEFT_MEMORY,    ImGuiKey_LeftArrow,                          false,  true,  "move_left_memory_address",  "Move left in hex viewer"},
    {MOVE_RIGHT_MEMORY,   ImGuiKey_RightArrow,                         false,  true,  "move_right_memory_address", "Move right hex viewer"},
});
static_assert(allShortcutInfo.size() == ShortcutIndex::NUM_SHORTCUTS);
// Note: if we forget a row we get a compile error.
//       if we swap the order of some rows we also get a compile error (when asserts are enabled)

static constexpr auto defaultShortcuts = []{
    std::array<Shortcuts::Data, ShortcutIndex::NUM_SHORTCUTS> result = {};
    for (int i = 0; i < ShortcutIndex::NUM_SHORTCUTS; ++i) {
        auto& all = allShortcutInfo[i];
        assert(all.index == i); // verify that table is in-order
        result[i].keyChord = all.keyChord;
        result[i].local    = all.local;
        result[i].repeat   = all.repeat;
    }
    return result;
}();

static constexpr auto shortcutNames = []{
    std::array<std::string_view, ShortcutIndex::NUM_SHORTCUTS> result = {};
    for (int i = 0; i < ShortcutIndex::NUM_SHORTCUTS; ++i) {
        result[i] = allShortcutInfo[i].name;
    }
    return result;
}();

static constexpr auto shortcutDescriptions = []{
    std::array<std::string_view, ShortcutIndex::NUM_SHORTCUTS> result = {};
    for (int i = 0; i < ShortcutIndex::NUM_SHORTCUTS; ++i) {
        result[i] = allShortcutInfo[i].description;
    }
    return result;
}();

Shortcuts::Shortcuts(SettingsConfig& config)
	: shortcuts{}
{
	config.setShortcuts(*this);
	setDefaultShortcuts();
}

void Shortcuts::setDefaultShortcuts()
{
	int index = 0;
	for (const auto& shortcut : defaultShortcuts) {
		setShortcut(static_cast<ShortcutIndex>(index++), shortcut.keyChord, shortcut.local, shortcut.repeat);
	}
}

Shortcuts::Data& Shortcuts::getShortcut(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	return shortcuts[index];
}

void Shortcuts::setShortcut(ShortcutIndex index, std::optional<ImGuiKeyChord> keyChord, std::optional<bool> local, std::optional<bool> repeat)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	if (keyChord) shortcuts[index].keyChord = *keyChord;
	if (local) shortcuts[index].local = *local;
	if (repeat) shortcuts[index].repeat = *repeat;
}

void Shortcuts::setDefaultShortcut(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	const auto& shortcut = defaultShortcuts[index];
	setShortcut(index, shortcut.keyChord, shortcut.local, shortcut.repeat);
}

const std::string_view Shortcuts::getShortcutName(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	return shortcutNames[index];
}

std::optional<ShortcutIndex> Shortcuts::getShortcutIndex(const std::string_view& name)
{
        auto it = ranges::find_if(allShortcutInfo, [&](const auto& p) { return p.name == name; });
        if (it == allShortcutInfo.end()) return {};
        return std::optional<ShortcutIndex>(static_cast<ShortcutIndex>(std::distance(allShortcutInfo.begin(), it)));
}	

const std::string_view Shortcuts::getShortcutDescription(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	return shortcutDescriptions[index];
}

bool Shortcuts::checkShortcut(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	auto& shortcut = shortcuts[index];
	auto flags = (shortcut.local ? ImGuiInputFlags_RouteUnlessBgFocused : ImGuiInputFlags_RouteGlobalLow | ImGuiInputFlags_RouteUnlessBgFocused)
		| (shortcut.repeat ? ImGuiInputFlags_Repeat : 0);
	// invalid shortcut causes SIGTRAP so we test if keyChord is valid
	return shortcut.keyChord && ImGui::Shortcut(shortcut.keyChord, 0, flags);
}

}
