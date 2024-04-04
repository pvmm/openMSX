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

class ImGuiShortcuts final
{
public:
	ImGuiShortcuts(const ImGuiShortcuts&) = delete;
	ImGuiShortcuts& operator=(const ImGuiShortcuts&) = delete;

	explicit ImGuiShortcuts(ImGuiManager& manager_);
	// ~ImGuiShortcuts();

	enum ShortcutIndex {
		MEMORY_GOTO_ADDRESS,
		DISASM_GOTO_ADDRESS,
		NUM,
	};
	struct ShortcutData {
		ImGuiKeyChord keychord;
		int flags;
	};
	using ShortcutSet = std::vector<ShortcutIndex>;   // unsorted

	ImGuiShortcuts::ShortcutData getShortcut(ShortcutIndex index);
	void setShortcut(ShortcutIndex index, ImGuiKeyChord keychord, int flags = 0);
	bool isPressed(ShortcutIndex index);

	template<typename XmlStream>
	void saveShortcuts([[maybe_unused]] XmlStream& xml) const 
	{}

private:
	void initShortcuts();

private:
	ImGuiManager& manager;

	// Shortcuts
	std::array<ShortcutData, ShortcutIndex::NUM> shortcuts;
	ShortcutSet boundShortcuts;     // different than default
	ShortcutSet unboundShortcuts;   // removed from default
};

} // namespace openmsx

#endif
