#include "ImGuiSymbols.hh"

#include "ImGuiBreakPoints.hh"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiDebugger.hh"
#include "ImGuiOpenFile.hh"
#include "ImGuiUtils.hh"
#include "ImGuiWatchExpr.hh"

#include "CliComm.hh"
#include "File.hh"
#include "Interpreter.hh"
#include "MSXCliComm.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPUInterface.hh"
#include "Reactor.hh"

#include "enumerate.hh"
#include "ranges.hh"
#include "StringOp.hh"
#include "stl.hh"
#include "strCat.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <imgui.h>

#include <sstream>
#include <cassert>

namespace openmsx {

ImGuiSymbols::ImGuiSymbols(ImGuiManager& manager_)
	: ImGuiPart(manager_)
	, symbolManager(manager.getReactor().getSymbolManager())
{
	symbolManager.setObserver(this);
}

ImGuiSymbols::~ImGuiSymbols()
{
	symbolManager.setObserver(nullptr);
}

void ImGuiSymbols::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	for (const auto& file : symbolManager.getFiles()) {
		buf.appendf("symbolfile=%s\n", file.filename.c_str());
		buf.appendf("symbolfiletype=%s\n", SymbolFile::toString(file.type).c_str());
	}
	for (const auto& [file, error, type, slot, subslot, index] : fileError) {
		assert(false);
		buf.appendf("symbolfile=%s\n", file.c_str());
		buf.appendf("symbolfiletype=%s\n", SymbolFile::toString(type).c_str());
		if (subslot) {
			buf.appendf("slotsubslot=%d-%d\n", slot, *subslot);
		} else {
			buf.appendf("slotsubslot=%d\n", slot);
		}
	}
	//assert(false);
}

void ImGuiSymbols::loadStart()
{
	symbolManager.removeAllFiles();
	fileError.clear();
}

void ImGuiSymbols::loadLine(std::string_view name, zstring_view value)
{
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name == "symbolfile") {
		fileError.emplace_back(std::string{value}, // filename
		                       std::string{}, // error
		                       SymbolFile::Type::AUTO_DETECT, // type
		                       0, // slot
				       std::nullopt, // subslot
				       0); // index
	} else if (name == "symbolfiletype") {
		if (!fileError.empty()) {
			fileError.back().type =
				SymbolFile::parseType(value).value_or(SymbolFile::Type::AUTO_DETECT);
		}
	} else if (name == "slotsubslot") {
		if (!fileError.empty()) {
			std::istringstream ss(value.data());
			int subslot;
			char dash;

			ss >> fileError.back().slot;
			// optional subslot
			if (ss >> dash && dash == '-' && ss >> subslot) {
				fileError.back().subslot = subslot;
			}
		}
	}
}

void ImGuiSymbols::loadEnd()
{
	std::vector<FileInfo> tmp;
	std::swap(tmp, fileError);
	for (const auto& info : tmp) {
		loadFile(info.filename, SymbolManager::LoadEmpty::ALLOWED, info.type);
	}
}

void ImGuiSymbols::loadFile(const std::string& filename, SymbolManager::LoadEmpty loadEmpty, SymbolFile::Type type)
{
	auto& cliComm = manager.getCliComm();
	auto it = ranges::find(fileError, filename, &FileInfo::filename);
	try {
		if (!symbolManager.reloadFile(filename, loadEmpty, type)) {
			cliComm.printWarning("Symbol file \"", filename,
			                     "\" doesn't contain any symbols");
		}
		if (it != fileError.end()) fileError.erase(it); // clear previous error
	} catch (MSXException& e) {
		cliComm.printWarning(
			"Couldn't load symbol file \"", filename, "\": ", e.getMessage());
		if (it != fileError.end()) {
			it->error = e.getMessage(); // overwrite previous error
			it->type = type;
		} else {
			//assert(false);
			fileError.emplace_back(filename, e.getMessage(), type, it->slot, it->subslot, it->index); // set error
		}
	}
}

static void checkSort(SymbolManager& manager, std::vector<SymbolRef>& symbols)
{
	auto* sortSpecs = ImGui::TableGetSortSpecs();
	if (!sortSpecs->SpecsDirty) return;

	sortSpecs->SpecsDirty = false;
	assert(sortSpecs->SpecsCount == 1);
	assert(sortSpecs->Specs);
	assert(sortSpecs->Specs->SortOrder == 0);

	switch (sortSpecs->Specs->ColumnIndex) {
	case 0: // name
		sortUpDown_String(symbols, sortSpecs, [&](const auto& sym) { return sym.name(manager); });
		break;
	case 1: // value
		sortUpDown_T(symbols, sortSpecs, [&](const auto& sym) { return sym.value(manager); });
		break;
	case 2: // file
		sortUpDown_String(symbols, sortSpecs, [&](const auto& sym) { return sym.file(manager); });
		break;
	default:
		UNREACHABLE;
	}
}

void ImGuiSymbols::drawContext(MSXMotherBoard* motherBoard, const SymbolRef& sym)
{
	if (ImGui::MenuItem("Set breakpoint")) {
		std::string cond = strCat("[pc_in_slot ", sym.slot(symbolManager), ' ', sym.subslot(symbolManager) ? (*sym.subslot(symbolManager)) : 0, ' ', sym.segment(symbolManager), ']');
		BreakPoint newBp(sym.value(symbolManager), TclObject("debug break"), TclObject(cond), false);
		auto& cpuInterface = motherBoard->getCPUInterface();
		cpuInterface.insertBreakPoint(std::move(newBp));
	}
	if (ImGui::MenuItem("Open on hex editor...")) {
		auto     filename = sym.file(symbolManager);
		auto old_filename = filename;
		if (auto pos = filename.rfind("/"); pos != std::string_view::npos) {
			filename = filename.substr(pos + 1);
			old_filename = filename;
		}
	        if (auto pos = filename.rfind("."); pos != std::string_view::npos) {
			filename = filename.substr(0, pos);
		}
		if (!manager.debugger->createHexEditor(std::string(filename), sym.value(symbolManager))) {
			// try .rom file next
			auto romfile = tmpStrCat(filename, ".rom");
			if (!manager.debugger->createHexEditor(std::string(romfile), sym.value(symbolManager))) {
				motherBoard->getMSXCliComm().printWarning("Failed to find ROM for ", old_filename);
			}
		}
	}
}

template<bool FILTER_FILE>
void ImGuiSymbols::drawTable(MSXMotherBoard* motherBoard, const std::string& file)
{
	assert(FILTER_FILE == !file.empty());

	int flags = ImGuiTableFlags_RowBg |
	            ImGuiTableFlags_BordersV |
	            ImGuiTableFlags_BordersOuter |
	            ImGuiTableFlags_ContextMenuInBody |
	            ImGuiTableFlags_Resizable |
	            ImGuiTableFlags_Reorderable |
	            ImGuiTableFlags_Sortable |
	            ImGuiTableFlags_Hideable |
	            ImGuiTableFlags_SizingStretchProp |
	            (FILTER_FILE ? ImGuiTableFlags_ScrollY : 0);
	im::Table(file.c_str(), (FILTER_FILE ? 4 : 5), flags, {0, 100}, [&]{
		ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
		ImGui::TableSetupColumn("name");
		ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("slot", showSlot ? 0 : ImGuiTableColumnFlags_DefaultHide);
		ImGui::TableSetupColumn("segment", showSeg ? 0 : ImGuiTableColumnFlags_DefaultHide);
		if (!FILTER_FILE) {
			ImGui::TableSetupColumn("file");
		}
		ImGui::TableHeadersRow();
		checkSort(symbolManager, symbols);

		for (const auto& sym : symbols) {
			if (FILTER_FILE && (sym.file(symbolManager) != file)) continue;

			if (ImGui::TableNextColumn()) { // name
				im::ScopedFont sf(manager.fontMono);
				im::StyleColor(sym.segment(symbolManager) > 0, ImGuiCol_Text, getColor(imColor::YELLOW), [&]{
					ImGui::TextUnformatted(sym.name(symbolManager));
				});
				auto symNameMenu = tmpStrCat("symbol-manager##", sym.name(symbolManager)).data();
				if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
					ImGui::OpenPopup(symNameMenu);
				}
				im::Popup(symNameMenu, [&]{ drawContext(motherBoard, sym); });
			}
			if (ImGui::TableNextColumn()) { // value
				im::ScopedFont sf(manager.fontMono);
				ImGui::TextUnformatted(tmpStrCat(hex_string<4>(sym.value(symbolManager))).c_str());
			}
			if (ImGui::TableNextColumn()) { // slot
				im::ScopedFont sf(manager.fontMono);
				if (sym.subslot(symbolManager)) {
					ImGui::TextUnformatted(tmpStrCat(sym.slot(symbolManager), "-", narrow<int>(*sym.subslot(symbolManager))));
				} else {
					ImGui::TextUnformatted(tmpStrCat(sym.slot(symbolManager)));
				}
			}
			if (ImGui::TableNextColumn()) { // segment
				im::ScopedFont sf(manager.fontMono);
				ImGui::TextUnformatted(tmpStrCat(sym.segment(symbolManager)).c_str());
			}
			if (!FILTER_FILE && ImGui::TableNextColumn()) { // file
				ImGui::TextUnformatted(sym.file(symbolManager));
			}
		}
	});
}

static int toSlotIndex(MSXMotherBoard* motherBoard, int slot, std::optional<int> subslot)
{
	auto& cpuInterface = motherBoard->getCPUInterface();
	int slotIndex = 0;
	for (int ps = 0; ps < slot; ++ps) {
		slotIndex += cpuInterface.isExpanded(ps) ? 4 : 1;
	}
	return slotIndex + (subslot ? *subslot : 0);
}

void ImGuiSymbols::paint(MSXMotherBoard* motherBoard)
{
	if (!show) return;

	const auto& style = ImGui::GetStyle();
	ImGui::SetNextWindowSize(gl::vec2{24, 18} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Symbol Manager", &show, [&]{
		if (ImGui::Button("Load symbol file...")) {
			manager.openFile->selectFile(
				"Select symbol file",
				SymbolManager::getFileFilters(),
				[this](const std::string& filename) {
					auto type = SymbolManager::getTypeForFilter(ImGuiOpenFile::getLastFilter());
					loadFile(filename, SymbolManager::LoadEmpty::NOT_ALLOWED, type);
				});
		}

		im::TreeNode("Symbols per file", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			auto drawFile = [&](FileInfo& info) {
				auto fileIdx = symbolManager.findFile(info.filename);
				im::StyleColor(!info.error.empty(), ImGuiCol_Text, getColor(imColor::ERROR), [&]{
					auto title = strCat("File: ", info.filename);
					im::TreeNode(title.c_str(), [&]{
						if (!info.error.empty()) {
							ImGui::TextUnformatted(info.error);
						}
						im::StyleColor(ImGuiCol_Text, getColor(imColor::TEXT), [&]{
							auto& cpuInterface = motherBoard->getCPUInterface();
							std::string slotInfo;
							std::vector<std::pair<uint16_t, int16_t>> slotSubslots;
							int nSlots = 0;
							for (auto ps = 0; ps < 4; ++ps) {
								if (cpuInterface.isExpanded(ps)) {
									slotInfo = tmpStrCat(slotInfo, ps, "-0", '\000', ps, "-1", '\000', ps, "-2", '\000', ps, "-3", '\000');
									slotSubslots.push_back({ps, 0});
									slotSubslots.push_back({ps, 1});
									slotSubslots.push_back({ps, 2});
									slotSubslots.push_back({ps, 3});
									nSlots += 4;
								} else {
									slotInfo = tmpStrCat(slotInfo, ps, '\000');
									slotSubslots.push_back({ps, -1});
									nSlots++;
								}
							}
							auto arrowSize = ImGui::GetFrameHeight();
							auto extra = arrowSize + 2.0f * style.FramePadding.x;
							ImGui::SetNextItemWidth(ImGui::CalcTextSize("3-3").x + extra);
						        ImGui::AlignTextToFramePadding();

							// slot
							ImGui::Combo(tmpStrCat("Slot##", info.filename).data(), &info.index, slotInfo.c_str(), nSlots);
							if (fileIdx.has_value()) {
								auto& file = symbolManager.getFiles()[*fileIdx];
								// detect slot/subslot changes
								if (slotSubslots[info.index].first != file.slot || slotSubslots[info.index].second != file.subslot) {
									file.slot = slotSubslots[info.index].first;
									file.subslot = slotSubslots[info.index].second;
									for (auto& symbol: file.getSymbols()) {
										symbol.slot = slotSubslots[info.index].first;
										symbol.subslot = slotSubslots[info.index].second;
									}
								}
							}
							ImGui::SameLine();

							if (ImGui::Button("Reload")) {
								loadFile(info.filename, SymbolManager::LoadEmpty::NOT_ALLOWED, info.type);
							}
							ImGui::SameLine();
							if (ImGui::Button("Remove")) {
								symbolManager.removeFile(info.filename);
								if (auto it = ranges::find(fileError, info.filename, &FileInfo::filename);
								    it != fileError.end()) {
									fileError.erase(it);
								}
							}
							drawTable<true>(motherBoard, info.filename);
						});
					});
				});
			};
			// make copy because cache may get dropped
			auto infos = to_vector(view::transform(symbolManager.getFiles(), [&](const auto& file) {
				return FileInfo{file.filename, std::string{}, file.type, file.slot, file.subslot, toSlotIndex(motherBoard, file.slot, file.subslot)};
			}));
			append(infos, fileError);
			for (auto& info : infos) {
				drawFile(info);
			}

		});
		im::TreeNode("All symbols", [&]{
			if (ImGui::Button("Reload all")) {
				auto tmp = to_vector(view::transform(symbolManager.getFiles(), [&](const auto& file) {
					return FileInfo{file.filename, std::string{}, file.type, file.slot, file.subslot, toSlotIndex(motherBoard, file.slot, file.subslot)};
				}));
				append(tmp, std::move(fileError));
				fileError.clear();
				for (const auto& info : tmp) {
					loadFile(info.filename, SymbolManager::LoadEmpty::NOT_ALLOWED, info.type);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove all")) {
				symbolManager.removeAllFiles();
				fileError.clear();
			}
			drawTable<false>(motherBoard);
		});
	});
}

void ImGuiSymbols::notifySymbolsChanged()
{
	symbols.clear();
	for (const auto& [fileIdx, file] : enumerate(symbolManager.getFiles())) {
		for (auto symbolIdx : xrange(file.symbols.size())) {
			//symbols.emplace_back(narrow<unsigned>(fileIdx),
			//                     narrow<unsigned>(symbolIdx));
			// clang workaround
			//assert(false);
			symbols.push_back(SymbolRef{narrow<unsigned>(fileIdx),
			                            narrow<unsigned>(symbolIdx)});
		}
	}

	manager.breakPoints->refreshSymbols();
	manager.watchExpr->refreshSymbols();
}

} // namespace openmsx
