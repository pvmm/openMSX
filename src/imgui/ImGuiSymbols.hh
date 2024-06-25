#ifndef IMGUI_SYMBOLS_HH
#define IMGUI_SYMBOLS_HH

#include "ImGuiPart.hh"

#include "SymbolManager.hh"

#include "hash_map.hh"

#include <cstdint>
#include <optional>
#include <vector>

namespace openmsx {

struct SymbolRef {
	unsigned fileIdx;
	unsigned symbolIdx;

	[[nodiscard]] std::string_view file(SymbolManager& m) const { return m.getFiles()[fileIdx].filename; }
	[[nodiscard]] std::string_view name(SymbolManager& m) const { return m.getFiles()[fileIdx].symbols[symbolIdx].name; }
	[[nodiscard]] uint16_t        value(SymbolManager& m) const { return m.getFiles()[fileIdx].symbols[symbolIdx].value; }
	[[nodiscard]] int16_t          slot(SymbolManager& m) const { return m.getFiles()[fileIdx].symbols[symbolIdx].slot; }
	[[nodiscard]] int16_t       subslot(SymbolManager& m) const { return m.getFiles()[fileIdx].symbols[symbolIdx].subslot; }
	[[nodiscard]] int16_t       segment(SymbolManager& m) const { return m.getFiles()[fileIdx].symbols[symbolIdx].segment; }
};

class ImGuiSymbols final : public ImGuiPart, private SymbolObserver
{
public:
	struct FileInfo {
		FileInfo(std::string f, std::string e, SymbolFile::Type t, int p, int b)
			: filename(std::move(f)), error(std::move(e)), type(t), pos(p), base(b) {} // clang-15 workaround

		std::string filename;
		std::string error;
		SymbolFile::Type type;
		int pos;  // slot/subslot index
		int base; // base address index
	};

	explicit ImGuiSymbols(ImGuiManager& manager);
	~ImGuiSymbols();

	[[nodiscard]] zstring_view iniName() const override { return "symbols"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadStart() override;
	void loadLine(std::string_view name, zstring_view value) override;
	void loadEnd() override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;
	bool showSlot = false;
	bool showSeg = false;

private:
	void loadFile(const std::string& filename, SymbolManager::LoadEmpty loadEmpty, SymbolFile::Type type);

	template<bool FILTER_FILE>
	void drawTable(MSXMotherBoard* motherBoard, const std::string& file = {});

	void drawContext(MSXMotherBoard* motherBoard, const SymbolRef& sym);

	// SymbolObserver
	void notifySymbolsChanged() override;

private:
	SymbolManager& symbolManager;
	std::vector<SymbolRef> symbols;

	std::vector<FileInfo> fileError;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiSymbols::show}
	};
};

} // namespace openmsx

#endif
