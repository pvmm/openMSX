#include "SettingsConfig.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "MemBuffer.hh"
#include "CliComm.hh"
#include "HotKey.hh"
#include "Shortcuts.hh"
#include "CommandException.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "XMLOutputStream.hh"
#include "outer.hh"
#include "rapidsax.hh"
#include "unreachable.hh"

using std::string;

namespace openmsx {

struct SettingsParser : rapidsax::NullHandler
{
	void start(std::string_view tag);
	void attribute(std::string_view name, std::string_view value);
	void text(std::string_view txt);
	void stop();
	void doctype(std::string_view txt);

	// parse result
	struct Setting {
		std::string_view name;
		std::string_view value;
	};
	std::vector<Setting> settings;
	std::vector<HotKey::Data> binds;
	std::vector<std::string_view> unbinds;
	std::vector<std::pair<int, Shortcuts::Data>> shortcutTuples;
	std::string_view systemID;

	// parse state
	unsigned unknownLevel = 0;
	enum State {
		START,
		TOP,
		SETTINGS,
		SETTING,
		BINDINGS,
		BIND,
		UNBIND,
		SHORTCUTS,
		SHORTCUT,
		END
	} state = START;
	Setting currentSetting;
	HotKey::Data currentBind;
	int currentShortcutIndex;
	Shortcuts::Data currentShortcut;
	std::string_view currentUnbind;
};

SettingsConfig::SettingsConfig(
		GlobalCommandController& globalCommandController,
		HotKey& hotKey_)
	: commandController(globalCommandController)
	, saveSettingsCommand(commandController)
	, loadSettingsCommand(commandController)
	, settingsManager(globalCommandController)
	, hotKey(hotKey_)
{
}

SettingsConfig::~SettingsConfig()
{
	if (mustSaveSettings) {
		try {
			saveSetting();
		} catch (FileException& e) {
			commandController.getCliComm().printWarning(
				"Auto-saving of settings failed: ", e.getMessage());
		}
	}
}

void SettingsConfig::loadSetting(const FileContext& context, std::string_view filename)
{
	string resolved = context.resolve(filename);

	MemBuffer<char> buf;
	try {
		File file(resolved);
		auto size = file.getSize();
		buf.resize(size + rapidsax::EXTRA_BUFFER_SPACE);
		file.read(std::span{buf.data(), size});
		buf[size] = 0;
	} catch (FileException& e) {
		throw MSXException("Failed to read settings file '", filename,
		                   "': ", e.getMessage());
	}
	SettingsParser parser;
	rapidsax::parse<0>(parser, buf.data());
	if (parser.systemID != "settings.dtd") {
		throw MSXException(
			"Failed to parser settings file '", filename,
		        "': systemID doesn't match (expected 'settings.dtd' got '",
			parser.systemID, "')\n"
			"You're probably using an old incompatible file format.");
	}

	settingValues.clear();
	settingValues.reserve(unsigned(parser.settings.size()));
	for (const auto& [name, value] : parser.settings) {
		settingValues.emplace(name, value);
	}

	hotKey.loadInit();
	for (const auto& bind : parser.binds) {
		try {
			hotKey.loadBind(bind);
		} catch (MSXException& e) {
			commandController.getCliComm().printWarning(
				"Couldn't restore key-binding: ", e.getMessage());
		}
	}
	for (const auto& key : parser.unbinds) {
		try {
			hotKey.loadUnbind(key);
		} catch (MSXException& e) {
			commandController.getCliComm().printWarning(
				"Couldn't restore key-binding: ", e.getMessage());
		}
	}

	shortcuts->setDefaultShortcuts();
	for (const auto& [index, shortcut]: parser.shortcutTuples) {
		shortcuts->setShortcut(static_cast<ShortcutIndex>(index), shortcut.keychord, shortcut.local, shortcut.repeat);
	}

	getSettingsManager().loadSettings(*this);

	// only set saveName after file was successfully parsed
	saveName = resolved;
}

void SettingsConfig::setSaveFilename(const FileContext& context, std::string_view filename)
{
	saveName = context.resolveCreate(filename);
}

const std::string* SettingsConfig::getValueForSetting(std::string_view setting) const
{
	return lookup(settingValues, setting);
}

void SettingsConfig::setValueForSetting(std::string_view setting, std::string_view value)
{
	settingValues.insert_or_assign(setting, value);
}

void SettingsConfig::removeValueForSetting(std::string_view setting)
{
	settingValues.erase(setting);
}

void SettingsConfig::saveSetting(std::string filename)
{
	if (filename.empty()) filename = saveName;
	if (filename.empty()) return;

	struct SettingsWriter {
		explicit SettingsWriter(std::string filename)
			: file(std::move(filename), File::TRUNCATE)
		{
			std::string_view header =
				"<!DOCTYPE settings SYSTEM 'settings.dtd'>\n";
			write(header);
		}

		void write(std::span<const char> buf) {
			file.write(buf);
		}
		void write1(char c) {
			file.write(std::span{&c, 1});
		}
		void check(bool condition) const {
			assert(condition); (void)condition;
		}

	private:
		File file;
	};

	SettingsWriter writer(std::move(filename));
	XMLOutputStream xml(writer);
	xml.with_tag("settings", [&]{
		xml.with_tag("settings", [&]{
			for (const auto& [name_, value_] : settingValues) {
				auto name = name_;    // clang-15 workaround
				auto value = value_;  // fixed in clang-16
				xml.with_tag("setting", [&]{
					xml.attribute("id", name);
					xml.data(value);
				});
			}
		});
		hotKey.saveBindings(xml);
		shortcuts->saveShortcuts(xml);
	});
}


// class SaveSettingsCommand

SettingsConfig::SaveSettingsCommand::SaveSettingsCommand(
		CommandController& commandController_)
	: Command(commandController_, "save_settings")
{
}

void SettingsConfig::SaveSettingsCommand::execute(
	std::span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, Between{1, 2}, Prefix{1}, "?filename?");
	auto& settingsConfig = OUTER(SettingsConfig, saveSettingsCommand);
	try {
		switch (tokens.size()) {
		case 1:
			settingsConfig.saveSetting();
			break;
		case 2:
			settingsConfig.saveSetting(FileOperations::expandTilde(
				string(tokens[1].getString())));
			break;
		}
	} catch (FileException& e) {
		throw CommandException(std::move(e).getMessage());
	}
}

string SettingsConfig::SaveSettingsCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Save the current settings.";
}

void SettingsConfig::SaveSettingsCommand::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		completeFileName(tokens, systemFileContext());
	}
}


// class LoadSettingsCommand

SettingsConfig::LoadSettingsCommand::LoadSettingsCommand(
		CommandController& commandController_)
	: Command(commandController_, "load_settings")
{
}

void SettingsConfig::LoadSettingsCommand::execute(
	std::span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 2, "filename");
	auto& settingsConfig = OUTER(SettingsConfig, loadSettingsCommand);
	settingsConfig.loadSetting(systemFileContext(), tokens[1].getString());
}

string SettingsConfig::LoadSettingsCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Load settings from given file.";
}

void SettingsConfig::LoadSettingsCommand::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		completeFileName(tokens, systemFileContext());
	}
}


void SettingsParser::start(std::string_view tag)
{
	if (unknownLevel) {
		++unknownLevel;
		return;
	}
	switch (state) {
	case START:
		if (tag == "settings") {
			state = TOP;
			return;
		}
		throw MSXException("Expected <settings> as root tag.");
	case TOP:
		if (tag == "settings") {
			state = SETTINGS;
			return;
		} else if (tag == "bindings") {
			state = BINDINGS;
			return;
		} else if (tag == "shortcuts") {
			state = SHORTCUTS;
			return;
		}
		break;
	case SETTINGS:
		if (tag == "setting") {
			state = SETTING;
			currentSetting = Setting{};
			return;
		}
		break;
	case BINDINGS:
		if (tag == "bind") {
			state = BIND;
			currentBind = HotKey::Data{};
			return;
		} else if (tag == "unbind") {
			state = UNBIND;
			currentUnbind = std::string_view{};
			return;
		}
		break;
	case SHORTCUTS:
		if (tag == "shortcut") {
			state = SHORTCUT;
			currentShortcut = Shortcuts::Data{};
			return;
		}
		break;
	case SETTING:
	case BIND:
	case UNBIND:
	case SHORTCUT:
		break;
	case END:
		throw MSXException("Unexpected opening tag: ", tag);
	default:
		UNREACHABLE;
	}

	++unknownLevel;
}

void SettingsParser::attribute(std::string_view name, std::string_view value)
{
	if (unknownLevel) return;

	switch (state) {
	case SETTING:
		if (name == "id") {
			currentSetting.name = value;
		}
		break;
	case SHORTCUT:
		if (name == "keychord") {
			if (auto keychord = StringOp::stringTo<int>(value))
				currentShortcut.keychord = static_cast<ImGuiKeyChord>(*keychord);
		} else if (name == "local") {
			currentShortcut.local = StringOp::stringToBool(value);
		} else if (name == "repeat") {
			currentShortcut.repeat = StringOp::stringToBool(value);
		}
		break;
	case BIND:
		if (name == "key") {
			currentBind.key = value;
		} else if (name == "repeat") {
			currentBind.repeat = StringOp::stringToBool(value);
		} else if (name == "event") {
			currentBind.event = StringOp::stringToBool(value);
		} else if (name == "msx") {
			currentBind.msx = StringOp::stringToBool(value);
		}
		break;
	case UNBIND:
		if (name == "key") {
			currentUnbind = value;
		}
		break;
	default:
		break; //nothing
	}
}

void SettingsParser::text(std::string_view txt)
{
	if (unknownLevel) return;

	switch (state) {
	case SETTING:
		currentSetting.value = txt;
		break;
	case BIND:
		currentBind.cmd = txt;
		break;
	case SHORTCUT:
		if (auto value = StringOp::stringTo<int>(txt))
			currentShortcutIndex = static_cast<ShortcutIndex>(*value);
		break;
	default:
		break; //nothing
	}
}

void SettingsParser::stop()
{
	if (unknownLevel) {
		--unknownLevel;
		return;
	}

	switch (state) {
	case TOP:
		state = END;
		break;
	case SETTINGS:
		state = TOP;
		break;
	case BINDINGS:
		state = TOP;
		break;
	case SHORTCUTS:
		state = TOP;
		break;
	case SETTING:
		if (!currentSetting.name.empty()) {
			settings.push_back(currentSetting);
		}
		state = SETTINGS;
		break;
	case BIND:
		if (!currentBind.key.empty()) {
			binds.push_back(currentBind);
		}
		state = BINDINGS;
		break;
	case UNBIND:
		if (!currentUnbind.empty()) {
			unbinds.push_back(currentUnbind);
		}
		state = BINDINGS;
		break;
	case SHORTCUT:
		if (currentShortcut.keychord != ImGuiKey_None) {
			shortcutTuples.push_back(std::pair(currentShortcutIndex, currentShortcut));
		}
		state = SHORTCUTS;
		break;
	case START:
	case END:
		throw MSXException("Unexpected closing tag");
	default:
		UNREACHABLE;
	}
}

void SettingsParser::doctype(std::string_view txt)
{
	auto pos1 = txt.find(" SYSTEM ");
	if (pos1 == std::string_view::npos) return;
	if ((pos1 + 8) >= txt.size()) return;
	char q = txt[pos1 + 8];
	if (q != one_of('"', '\'')) return;
	auto t = txt.substr(pos1 + 9);
	auto pos2 = t.find(q);
	if (pos2 == std::string_view::npos) return;

	systemID = t.substr(0, pos2);
}

} // namespace openmsx
