// $Id$

#include "MSXRomCLI.hh"
#include "CartridgeSlotManager.hh"
#include "MSXConfig.hh"
#include "FileOperations.hh"


MSXRomCLI::MSXRomCLI()
{
	CommandLineParser::instance()->registerOption("-cart", this);
	CommandLineParser::instance()->registerOption("-carta", this);
	CommandLineParser::instance()->registerOption("-cartb", this);
	CommandLineParser::instance()->registerOption("-cartc", this);
	CommandLineParser::instance()->registerOption("-cartd", this);
	CommandLineParser::instance()->registerFileType("rom", this);
}

void MSXRomCLI::parseOption(const std::string &option,
                         std::list<std::string> &cmdLine)
{
	std::string arg = cmdLine.front();
	cmdLine.pop_front();
	if (option.length() == 6) {
		int slot = option[5] - 'a';
		CartridgeSlotManager::instance()->reserveSlot(slot);
		CommandLineParser::instance()->registerPostConfig(new MSXRomPostName(slot, arg));
	} else {
		CommandLineParser::instance()->registerPostConfig(new MSXRomPostNoName(arg));
	}
}
const std::string& MSXRomCLI::optionHelp() const
{
	static const std::string text("Insert the ROM file (cartridge) specified in argument");
	return text;
}
MSXRomPostName::MSXRomPostName(int slot_, const std::string &arg_)
	: MSXRomCLIPost(arg_), slot(slot_)
{
}
void MSXRomPostName::execute(MSXConfig *config)
{
	CartridgeSlotManager::instance()->getSlot(slot, ps, ss);
	MSXRomCLIPost::execute(config);
}

void MSXRomCLI::parseFileType(const std::string &arg)
{
	CommandLineParser::instance()->registerPostConfig(new MSXRomPostNoName(arg));
}
const std::string& MSXRomCLI::fileTypeHelp() const
{
	static const std::string text("ROM image of a cartridge");
	return text;
}
MSXRomPostNoName::MSXRomPostNoName(const std::string &arg_)
	: MSXRomCLIPost(arg_)
{
}
void MSXRomPostNoName::execute(MSXConfig *config)
{
	CartridgeSlotManager::instance()->getSlot(ps, ss);
	MSXRomCLIPost::execute(config);
}

MSXRomCLIPost::MSXRomCLIPost(const std::string &arg_)
	: arg(arg_)
{
}
void MSXRomCLIPost::execute(MSXConfig *config)
{
	std::string filename, mapper;
	int pos = arg.find_last_of(',');
	int pos2 = arg.find_last_of('.');
	if ((pos != -1) && (pos > pos2)) {
		filename = arg.substr(0, pos);
		mapper = arg.substr(pos + 1);
	} else {
		filename = arg;
		mapper = "auto";
	}
	std::string file = FileOperations::getFilename(filename);

	XML::Escape(filename);
	XML::Escape(file);
	std::ostringstream s;
	s << "<?xml version=\"1.0\"?>";
	s << "<msxconfig>";
	s << "<device id=\"MSXRom"<<ps<<"-"<<ss<<"\">";
	s << "<type>Rom</type>";
	s << "<slotted><ps>"<<ps<<"</ps><ss>"<<ss<<"</ss><page>0</page></slotted>";
	s << "<slotted><ps>"<<ps<<"</ps><ss>"<<ss<<"</ss><page>1</page></slotted>";
	s << "<slotted><ps>"<<ps<<"</ps><ss>"<<ss<<"</ss><page>2</page></slotted>";
	s << "<slotted><ps>"<<ps<<"</ps><ss>"<<ss<<"</ss><page>3</page></slotted>";
	s << "<parameter name=\"filename\">"<<filename<<"</parameter>";
	s << "<parameter name=\"filesize\">auto</parameter>";
	s << "<parameter name=\"volume\">9000</parameter>";
	s << "<parameter name=\"mappertype\">"<<mapper<<"</parameter>";
	s << "<parameter name=\"loadsram\">true</parameter>";
	s << "<parameter name=\"savesram\">true</parameter>";
	s << "<parameter name=\"sramname\">"<<file<<".SRAM</parameter>";
	s << "</device>";
	s << "</msxconfig>";
	PRT_DEBUG("DEBUG " << file);
	UserFileContext *context = new UserFileContext("roms/" + file);
	config->loadStream(context, s);
	delete this;
}

