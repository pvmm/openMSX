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
	[[nodiscard]] int16_t         slots(SymbolManager& m) const { return m.getFiles()[fileIdx].symbols[symbolIdx].slots; }
};

class ImGuiSymbols final : public ImGuiPart, private SymbolObserver
{
	struct FileInfo {
		FileInfo(std::string f, std::string e, SymbolFile::Type t)
			: filename(std::move(f)), error(std::move(e)), type(t) {} // clang-15 workaround

		std::string filename;
		std::string error;
		SymbolFile::Type type;
	};

public:
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

private:
	void loadFile(const std::string& filename, SymbolManager::LoadEmpty loadEmpty, SymbolFile::Type type);

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
