// $Id$

/*
 *  openmsx - the MSX emulator that aims for perfection
 *
 */

#include "Reactor.hh"
#include "CommandController.hh"
#include "CommandLineParser.hh"
#include "SettingsConfig.hh"
#include "CliServer.hh"
#include "Interpreter.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "EnumSetting.hh"
#include "MSXException.hh"
#include "HotKey.hh"
#include <memory>
#include <iostream>
#include <exception>
#include <cstdlib>
#include <SDL.h>

using std::auto_ptr;
using std::cerr;
using std::endl;
using std::string;

namespace openmsx {

static void initializeSDL()
{
	int flags = SDL_INIT_TIMER;
#ifndef NDEBUG
	flags |= SDL_INIT_NOPARACHUTE;
#endif
	if (SDL_Init(flags) < 0) {
		throw FatalError(string("Couldn't init SDL: ") + SDL_GetError());
	}
}

static void unexpectedExceptionHandler()
{
	cerr << "Unexpected exception." << endl;
}

static int main(int argc, char **argv)
{
	std::set_unexpected(unexpectedExceptionHandler);

	int err = 0;
	try {
		Reactor reactor;
		reactor.getCommandController().getInterpreter().init(argv[0]);

		// TODO cleanup once singleton mess is cleaned up
		HotKey hotKey(reactor.getCommandController(),
		              reactor.getEventDistributor());
		reactor.getCommandController().getSettingsConfig().setHotKey(&hotKey);

		CommandLineParser parser(reactor);
		parser.parse(argc, argv);
		CommandLineParser::ParseStatus parseStatus = parser.getParseStatus();

		if (parseStatus != CommandLineParser::EXIT) {
			initializeSDL();
			reactor.getIconStatus();
			if (!parser.isHiddenStartup()) {
				reactor.getDisplay().getRenderSettings().
					getRenderer().restoreDefault();
			}
			CliServer cliServer(reactor.getCommandController(),
			                    reactor.getEventDistributor());
			if (parseStatus != CommandLineParser::TEST) {
				reactor.run(parser);
			}
		}
	} catch (FatalError& e) {
		cerr << "Fatal error: " << e.getMessage() << endl;
		err = 1;
	} catch (MSXException& e) {
		cerr << "Uncaught exception: " << e.getMessage() << endl;
		err = 1;
	} catch (std::exception& e) {
		cerr << "Uncaught std::exception: " << e.what() << endl;
		err = 1;
	} catch (...) {
		cerr << "Uncaught exception of unexpected type." << endl;
		err = 1;
	}
	// Clean up.
	if (SDL_WasInit(SDL_INIT_EVERYTHING)) {
		SDL_Quit();
	}
	return err;
}

} // namespace openmsx

// Enter the openMSX namespace.
int main(int argc, char **argv)
{
	exit(openmsx::main(argc, argv)); // need exit() iso return on win32/SDL
}
