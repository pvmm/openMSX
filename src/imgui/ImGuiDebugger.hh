#ifndef IMGUI_DEBUGGER_HH
#define IMGUI_DEBUGGER_HH

#include "DebuggableEditor.hh"
#include "ImGuiPart.hh"

#include "EmuTime.hh"

#include <memory>
#include <optional>
#include <string>

namespace openmsx {

class CPURegs;
class Debuggable;
class Debugger;
class MSXCPUInterface;
class SymbolManager;

class ImGuiDebugger final : public ImGuiPart
{
public:
	explicit ImGuiDebugger(ImGuiManager& manager);
	~ImGuiDebugger();

	static void loadIcons();

	void signalBreak();

	[[nodiscard]] zstring_view iniName() const override { return "debugger"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadStart() override;
	void loadLine(std::string_view name, zstring_view value) override;
	bool createHexEditor(const std::string& name, uint16_t address = 0x0);
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void drawControl(MSXCPUInterface& cpuInterface);
	void drawDisassembly(CPURegs& regs, MSXCPUInterface& cpuInterface, Debugger& debugger, EmuTime::param time);
	void drawSlots(MSXCPUInterface& cpuInterface, Debugger& debugger);
	void drawStack(const CPURegs& regs, const MSXCPUInterface& cpuInterface, EmuTime::param time);
	void drawRegisters(CPURegs& regs);
	void drawFlags(CPURegs& regs);

	void checkShortcuts(MSXCPUInterface& cpuInterface);
	void actionBreakContinue(MSXCPUInterface& cpuInterface);
	void actionStepIn(MSXCPUInterface& cpuInterface);
	void actionStepOver();
	void actionStepOut();
	void actionStepBack();

private:
	SymbolManager& symbolManager;
	size_t cycleLabelsCounter = 0;

	std::vector<std::unique_ptr<DebuggableEditor>> hexEditors; // sorted on 'getDebuggableName()'

	std::string gotoAddr;
	std::string runToAddr;
	std::optional<unsigned> gotoTarget;
	bool followPC = false;

	bool showControl = false;
	bool showDisassembly = false;
	bool showSlots = false;
	bool showStack = false;
	bool showRegisters = false;
	bool showFlags = false;
	bool showXYFlags = false;
	int flagsLayout = 1;

	bool syncDisassemblyWithPC = false;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"showControl",     &ImGuiDebugger::showControl},
		PersistentElement{"showDisassembly", &ImGuiDebugger::showDisassembly},
		PersistentElement{"followPC",        &ImGuiDebugger::followPC},
		PersistentElement{"showRegisters",   &ImGuiDebugger::showRegisters},
		PersistentElement{"showSlots",       &ImGuiDebugger::showSlots},
		PersistentElement{"showStack",       &ImGuiDebugger::showStack},
		PersistentElement{"showFlags",       &ImGuiDebugger::showFlags},
		PersistentElement{"showXYFlags",     &ImGuiDebugger::showXYFlags},
		PersistentElementMax{"flagsLayout",  &ImGuiDebugger::flagsLayout, 2}
		// manually handle "showDebuggable.xxx"
	};
};

} // namespace openmsx

#endif
