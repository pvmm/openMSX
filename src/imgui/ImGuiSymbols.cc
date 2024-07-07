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
		if (file.slot) {
			buf.appendf("slotsubslot=%d\n", *file.slot);
		}
	}
	for (const auto& [file, error, type, slot] : fileError) {
		buf.appendf("symbolfile=%s\n", file.c_str());
		buf.appendf("symbolfiletype=%s\n", SymbolFile::toString(type).c_str());
		if (slot) {
			buf.appendf("slotsubslot=%d\n", *slot);
		}
	}
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
				       std::nullopt); // slot-subslot
	} else if (name == "symbolfiletype") {
		if (!fileError.empty()) {
			fileError.back().type =
				SymbolFile::parseType(value).value_or(SymbolFile::Type::AUTO_DETECT);
		}
	} else if (name == "slotsubslot") {
		if (!fileError.empty()) {
			std::istringstream ss(value.data());
			int slot;

			if (ss >> slot) {
				fileError.back().slot = slot;
			}
		}
	}
}

void ImGuiSymbols::loadEnd()
{
	std::vector<FileInfo> tmp;
	std::swap(tmp, fileError);
	for (const auto& info : tmp) {
		loadFile(info.filename, SymbolManager::LoadEmpty::ALLOWED, info.type, info.slot);
	}
}

void ImGuiSymbols::loadFile(const std::string& filename, SymbolManager::LoadEmpty loadEmpty, SymbolFile::Type type, std::optional<uint8_t> slot)
{
	auto& cliComm = manager.getCliComm();
	auto it = ranges::find(fileError, filename, &FileInfo::filename);
	try {
		if (!symbolManager.reloadFile(filename, loadEmpty, type, slot)) {
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
			fileError.emplace_back(filename, e.getMessage(), type, it->slot); // set error
		}
	}
}

static void checkSort(const SymbolManager& manager, std::vector<SymbolRef>& symbols)
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
	case 2: // file or slot
		sortUpDown_String(symbols, sortSpecs, [&](const auto& sym) { return sym.file(manager); });
		break;
	case 3: // segment
		sortUpDown_T(symbols, sortSpecs, [&](const auto& sym) { return sym.segment(manager); });
		break;
	case 4: // file (all symbols)
		sortUpDown_String(symbols, sortSpecs, [&](const auto& sym) { return sym.file(manager); });
		break;
	default:
		UNREACHABLE;
	}
}

void ImGuiSymbols::drawContext(MSXMotherBoard* motherBoard, const SymbolRef& sym)
{
	if (ImGui::MenuItem("Set breakpoint")) {
		std::string cond;
		if (sym.slot(symbolManager)) {
			strAppend(cond, "[pc_in_slot ", narrow<int>(*sym.slot(symbolManager) & 3), ' ', narrow<int>((*sym.slot(symbolManager) >> 2) & 3));
			if (sym.segment(symbolManager)) {
				strAppend(cond, ' ', narrow<int>(*sym.segment(symbolManager)));
			}
			strAppend(cond, ']');
		}
		BreakPoint newBp(sym.value(symbolManager), TclObject("debug break"), TclObject(cond), false);
		auto& cpuInterface = motherBoard->getCPUInterface();
		cpuInterface.insertBreakPoint(std::move(newBp));
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
		ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("slot", ImGuiTableColumnFlags_DefaultHide);
		ImGui::TableSetupColumn("segment", ImGuiTableColumnFlags_DefaultHide);
		if (!FILTER_FILE) {
			ImGui::TableSetupColumn("file", ImGuiTableColumnFlags_NoHide);
		}
		ImGui::TableHeadersRow();
		checkSort(symbolManager, symbols);

		for (const auto& sym : symbols) {
			if (FILTER_FILE && (sym.file(symbolManager) != file)) continue;

			if (ImGui::TableNextColumn()) { // name
				im::ScopedFont sf(manager.fontMono);
				auto symName = sym.name(symbolManager);
				ImGui::Selectable(symName.data(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
				auto symNameMenu = tmpStrCat("symbol-manager##", symName).data();
				if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
					ImGui::OpenPopup(symNameMenu);
				}
				im::Popup(symNameMenu, [&]{ drawContext(motherBoard, sym); });
			}
			if (ImGui::TableNextColumn()) { // value
				im::ScopedFont sf(manager.fontMono);
				ImGui::StrCat(hex_string<4>(sym.value(symbolManager)));	
			}
			if (ImGui::TableNextColumn()) { // slot
				im::ScopedFont sf(manager.fontMono);
				if (sym.slot(symbolManager)) {
					auto& cpuInterface = motherBoard->getCPUInterface();
					auto ps = *sym.slot(symbolManager) & 3;
					if (cpuInterface.isExpanded(ps)) {
						auto ss = (*sym.slot(symbolManager) >> 2) & 3;
						ImGui::TextUnformatted(tmpStrCat(ps, "-", ss));
					} else {
						ImGui::TextUnformatted(tmpStrCat(ps));
					}
				} else {
					ImGui::TextUnformatted("-");
				}
			}
			if (ImGui::TableNextColumn()) { // segment
				im::ScopedFont sf(manager.fontMono);
				ImGui::TextUnformatted(sym.segment(symbolManager) ? tmpStrCat(*sym.segment(symbolManager)).c_str() : "-");
			}
			if (!FILTER_FILE && ImGui::TableNextColumn()) { // file
				ImGui::TextUnformatted(sym.file(symbolManager));
			}
		}
	});
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
				auto* file = symbolManager.findFile(info.filename);
				im::StyleColor(!info.error.empty(), ImGuiCol_Text, getColor(imColor::ERROR), [&]{
					auto title = strCat("File: ", info.filename);
					im::TreeNode(title.c_str(), [&]{
						if (!info.error.empty()) {
							ImGui::TextUnformatted(info.error);
						}
						im::StyleColor(ImGuiCol_Text, getColor(imColor::TEXT), [&]{
							auto& cpuInterface = motherBoard->getCPUInterface();
							auto arrowSize = ImGui::GetFrameHeight();
							auto extra = arrowSize + 2.0f * style.FramePadding.x;
							ImGui::SetNextItemWidth(ImGui::CalcTextSize("3-3").x + extra);
						        ImGui::AlignTextToFramePadding();

							// build slot-subslot preview string
							const auto preview = !file->slot ? tmpStrCat('-') : cpuInterface.isExpanded(*file->slot & 3) ?
								tmpStrCat(*file->slot & 3, '-', (*file->slot >> 2) & 3) : tmpStrCat(*file->slot & 3);

				                        im::Combo(tmpStrCat("Slot##", info.filename).data(), preview.c_str(), [&]{
								int psSs = 0;

								// initial state
								if (ImGui::Selectable("-", !file->slot)) {
									file->slot = {};
									for (auto& symbol: file->getSymbols()) {
										symbol.slot = file->slot;
									}
								}

								for (uint8_t ps = 0; ps < 4; ++ps) {
									if (cpuInterface.isExpanded(ps)) {
										for (uint8_t ss = 0; ss < 4; ++ss) {
											auto slotInfo = tmpStrCat(ps, "-", ss);
											if (ImGui::Selectable(slotInfo.c_str(), file->slot && psSs == file->slot)) {
												file->slot = psSs;
												for (auto& symbol: file->getSymbols()) {
													symbol.slot = file->slot;
												}
											}
											psSs += 1 << 2; // add to subslot bits
										}
									} else {
										if (ImGui::Selectable(tmpStrCat(ps).c_str(), file->slot && psSs == file->slot)) {
											file->slot = psSs;
											for (auto& symbol: file->getSymbols()) {
												symbol.slot = file->slot;
											}
										}
										psSs++; // add to slot bits
									}
								}
							});
							ImGui::SameLine();

							if (ImGui::Button("Reload")) {
								loadFile(info.filename, SymbolManager::LoadEmpty::NOT_ALLOWED, info.type, info.slot);
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
				return FileInfo{file.filename, std::string{}, file.type, file.slot};
			}));
			append(infos, fileError);
			for (auto& info : infos) {
				drawFile(info);
			}

		});
		im::TreeNode("All symbols", [&]{
			if (ImGui::Button("Reload all")) {
				auto tmp = to_vector(view::transform(symbolManager.getFiles(), [&](const auto& file) {
					return FileInfo{file.filename, std::string{}, file.type, file.slot};
				}));
				append(tmp, std::move(fileError));
				fileError.clear();
				for (const auto& info : tmp) {
					loadFile(info.filename, SymbolManager::LoadEmpty::NOT_ALLOWED, info.type, info.slot);
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
			symbols.push_back(SymbolRef{narrow<unsigned>(fileIdx),
			                            narrow<unsigned>(symbolIdx)});
		}
	}

	manager.breakPoints->refreshSymbols();
	manager.watchExpr->refreshSymbols();
}

} // namespace openmsx
