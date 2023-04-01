#ifndef IMGUI_OPEN_FILE_HH
#define IMGUI_OPEN_FILE_HH

#include "ImGuiPart.hh"

#include "TclObject.hh"

#include <functional>
#include <map>
#include <string>

namespace openmsx {

class ImGuiManager;

class ImGuiOpenFile final : public ImGuiPart
{
public:
	ImGuiOpenFile(ImGuiManager& manager_)
		: manager(manager_) {}

	void selectFileCommand(const std::string& title, std::string filters, TclObject command);
	void selectFile(const std::string& title, std::string filters,
	                std::function<void(const std::string&)> callback);

	[[nodiscard]] zstring_view iniName() const override { return "open file dialog"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	ImGuiManager& manager;
	std::map<std::string, std::string> lastPath;
	std::string lastFileDialog;
	std::function<void(const std::string&)> openFileCallback;
};

} // namespace openmsx

#endif