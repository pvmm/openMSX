// $Id$

//TODO:
// - specify prefix for auto file name generation when recording (setting?)
// - append to existing wav files when recording (record command), but this is
//   basically a special case (pointer at the end) of:
// - (partly) overwrite an existing wav file from any given time index
// - seek in cassette images for the next and previous file (using empty space?)
// - (partly) overwrite existing wav files with new tape data (not very hi prio)
// - handle read-only cassette images (e.g.: CAS images or WAV files with a RO
//   flag): refuse to go to record mode when those are selected
// - CLEAN UP! It's a bit messy now.
// - smartly auto-set the position of tapes: if you insert an existing WAV
//   file, it will have the position at the start, assuming PLAY mode by
//   default.  When specifiying record mode at insert (somehow), it should be
//   at the back.
//   Alternatively, we could remember the index in tape images by storing the
//   index in some persistent data file with its SHA1 sum as it was as we last
//   saw it. When there are write actions to the tape, the hash has to be
//   recalculated and replaced in the data file. An optimization would be to
//   first simply check on the length of the file and fall back to SHA1 if that
//   results in multiple matches.

#include "BooleanSetting.hh"
#include "CassettePlayer.hh"
#include "Connector.hh"
#include "CassettePort.hh"
#include "CommandController.hh"
#include "Command.hh"
#include "GlobalSettings.hh"
#include "XMLElement.hh"
#include "FileContext.hh"
#include "WavImage.hh"
#include "CasImage.hh"
#include "DummyCassetteImage.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "Reactor.hh"
#include "Scheduler.hh"
#include "MSXEventDistributor.hh"
#include "InputEvents.hh"
#include "FileOperations.hh"
#include "WavWriter.hh"
#include "ThrottleManager.hh"
#include "TclObject.hh"
#include "checked_cast.hh"
#include <algorithm>
#include <cstdlib>
#include <cassert>

using std::auto_ptr;
using std::list;
using std::string;
using std::vector;
using std::set;

namespace openmsx {

static const unsigned RECORD_FREQ = 44100;
static const double OUTPUT_AMP = 60.0;

class TapeCommand : public SimpleCommand
{
public:
	TapeCommand(CommandController& commandController,
	            CassettePlayer& cassettePlayer);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	CassettePlayer& cassettePlayer;
};


MSXCassettePlayerCLI::MSXCassettePlayerCLI(CommandLineParser& commandLineParser)
	: commandController(commandLineParser.getReactor().getCommandController())
{
	commandLineParser.registerOption("-cassetteplayer", this);
	commandLineParser.registerFileClass("cassetteimage", this);
}

bool MSXCassettePlayerCLI::parseOption(const string& option,
                                       list<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
	return true;
}
const string& MSXCassettePlayerCLI::optionHelp() const
{
	static const string text(
	  "Put cassette image specified in argument in virtual cassetteplayer");
	return text;
}

void MSXCassettePlayerCLI::parseFileType(const string& filename,
                                         list<string>& /*cmdLine*/)
{
	if (!commandController.hasCommand("cassetteplayer")) {
		throw MSXException("No cassetteplayer.");
	}
	TclObject command(commandController.getInterpreter());
	command.addListElement("cassetteplayer");
	command.addListElement(filename);
	command.executeCommand();
}
const string& MSXCassettePlayerCLI::fileTypeHelp() const
{
	static const string text(
		"Cassette image, raw recording or fMSX CAS image");
	return text;
}


CassettePlayer::CassettePlayer(
		CommandController& commandController_,
		Mixer& mixer, Scheduler& scheduler_,
		MSXEventDistributor& msxEventDistributor_)
	: SoundDevice(mixer, getName(), getDescription())
	, motor(false), motorControl(true), isLoading(false)
	, tapeTime(EmuTime::zero)
	, recTime(EmuTime::zero)
	, prevTime(EmuTime::zero)
	, lastOutput(false)
	, sampcnt(0)
	, commandController(commandController_)
	, scheduler(scheduler_)
	, msxEventDistributor(msxEventDistributor_)
	, tapeCommand(new TapeCommand(commandController, *this))
	, playTapeTime(EmuTime::zero)
	, cliComm(commandController.getCliComm())
	, throttleManager(commandController.getGlobalSettings().getThrottleManager())
{
	autoRunSetting.reset(new BooleanSetting(commandController,
		"autoruncassettes", "automatically try to run cassettes", false));
	removeTape(EmuTime::zero);

	static XMLElement cassettePlayerConfig("cassetteplayer");
	static bool init = false;
	if (!init) {
		init = true;
		auto_ptr<XMLElement> sound(new XMLElement("sound"));
		sound->addChild(auto_ptr<XMLElement>(new XMLElement("volume", "5000")));
		cassettePlayerConfig.addChild(sound);
	}
	registerSound(cassettePlayerConfig);
	msxEventDistributor.registerEventListener(*this);
}

CassettePlayer::~CassettePlayer()
{
	msxEventDistributor.unregisterEventListener(*this);
	unregisterSound();
	if (Connector* connector = getConnector()) {
		connector->unplug(scheduler.getCurrentTime());
	}
}

void CassettePlayer::updateLoadingState()
{
	bool newState = (motor && !casImage.empty());
	if (isLoading != newState) {
		isLoading = newState;
		throttleManager.indicateLoadingState(isLoading);
	}
}

void CassettePlayer::insertTape(const string& filename, const EmuTime& time)
{
	stopRecording(scheduler.getCurrentTime());
	try {
		// first try WAV
		cassette.reset(new WavImage(filename));
	} catch (MSXException &e) {
		// if that fails use CAS
		cassette.reset(new CasImage(filename, cliComm));
	}
	rewind(time);
	// try to automatically run the tape, if that's set
	CassetteImage::FileType type = cassette->getFirstFileType();
	if (autoRunSetting->getValue() && type != CassetteImage::UNKNOWN) {
		string loadingInstruction;
		switch (type) {
			case CassetteImage::ASCII:
				loadingInstruction = "RUN\"CAS:\"";
				break;
			case CassetteImage::BINARY:
				loadingInstruction = "BLOAD\"CAS:\",R";
				break;
			case CassetteImage::BASIC:
				loadingInstruction = "CLOAD";
				break;
			default:
				assert(false); // Shouldn't be possible
		}
		try {
			commandController.executeCommand(
				"after time 2 { type " + loadingInstruction + "\\r }");
		} catch (CommandException& e) {
			cliComm.printWarning(
				"Error executing loading instruction for AutoRun: " +
				e.getMessage() + " Please report a bug.");
		}
	}
	setMute(!isPlaying());
	casImage = filename;
	updateLoadingState();
}

void CassettePlayer::startRecording(const string& filename,
                                    const EmuTime& time)
{
	wavWriter.reset(new WavWriter(filename, 1, 8, RECORD_FREQ));
	reinitRecording(time);
}

void CassettePlayer::reinitRecording(const EmuTime& time)
{
	setMute(true);
	recTime = time;
	partialOut = 0.0;
	partialInterval = 0.0;
	lastX = lastOutput ? OUTPUT_AMP : -OUTPUT_AMP;
	lastY = 0.0;
}

void CassettePlayer::stopRecording(const EmuTime& time)
{
	if (wavWriter.get()) {
		setSignal(lastOutput, time);
		if (sampcnt) {
			flushOutput();
		}
		wavWriter.reset();
	}
}

void CassettePlayer::removeTape(const EmuTime& time)
{
	stopRecording(time);
	setMute(true);
	cassette.reset(new DummyCassetteImage());
	casImage.clear();
	updateLoadingState();
}

void CassettePlayer::rewind(const EmuTime& time)
{
	stopRecording(time);
	tapeTime = EmuTime::zero;
	playTapeTime = EmuTime::zero;
}

bool CassettePlayer::isPlaying() const
{
	// we're playing if there's a cassette inserted AND when (motor is
	// enabled by MSX OR motor control is disabled). Note that this assumes
	// there's no STOP mode when a cassette is inserted.
	return (motor || !motorControl && !casImage.empty());
}

void CassettePlayer::updatePosition(const EmuTime& time)
{
	assert(!wavWriter.get());
	if (isPlaying()) {
		tapeTime += (time - prevTime);
		playTapeTime = tapeTime;
	}
	prevTime = time;
}

void CassettePlayer::fillBuf(size_t length, double x)
{
	assert(wavWriter.get());
	static const double A = 252.0 / 256.0;

	double y = lastY + (x - lastX);

	while (length) {
		int len = std::min(length, BUF_SIZE - sampcnt);
		for (int j = 0; j < len; ++j) {
			buf[sampcnt++] = (int)y + 128;
			y *= A;
		}
		length -= len;
		assert(sampcnt <= BUF_SIZE);
		if (BUF_SIZE == sampcnt) {
			flushOutput();
		}
	}
	lastY = y;
	lastX = x;
}

void CassettePlayer::flushOutput()
{
	wavWriter->write8mono(buf, sampcnt);
	sampcnt = 0;
	wavWriter->flush(); // update wav header
}

void CassettePlayer::updateAll(const EmuTime& time)
{
	if (wavWriter.get()) {
		// recording
		if (isPlaying()) {
			// was already recording, update output
			setSignal(lastOutput, time);
		} else {
			// (possibly) restart recording, reset parameters
			reinitRecording(time);
		}
		flushOutput();
	} else {
		// playing
		updatePosition(time);
	}
}

void CassettePlayer::setMotor(bool status, const EmuTime& time)
{
	if (status!=motor)
	{
		updateAll(time);
		motor = status;
		setMute(wavWriter.get() || !isPlaying());
		updateLoadingState();
	}
}

void CassettePlayer::setMotorControl(bool status, const EmuTime& time)
{
	updateAll(time);
	motorControl = status;
	setMute(wavWriter.get() || !isPlaying());
}

short CassettePlayer::getSample(const EmuTime& time)
{
	assert(!wavWriter.get());
	return isPlaying() ? cassette->getSampleAt(time) : 0;
}

short CassettePlayer::readSample(const EmuTime& time)
{
	if (!wavWriter.get()) {
		// playing
		updatePosition(time);
		return getSample(tapeTime);
	} else {
		// recording
		return 0;
	}
}

void CassettePlayer::setSignal(bool output, const EmuTime& time)
{
	if (wavWriter.get() && isPlaying()) {
		double out = output ? OUTPUT_AMP : -OUTPUT_AMP;
		double samples = (time - recTime).toDouble() * RECORD_FREQ;
		double rest = 1.0 - partialInterval;
		if (rest <= samples) {
			// enough to fill next interval
			partialOut += out * rest;
			fillBuf(1, (int)partialOut);
			samples -= rest;

			// fill complete intervals
			int count = (int)samples;
			if (count > 0) {
				fillBuf(count, (int)out);
			}
			samples -= count;

			// partial last interval
			partialOut = samples * out;
			partialInterval = 0.0;
		} else {
			partialOut += samples * out;
			partialInterval += samples;
		}
	}
	recTime = time;
	lastOutput = output;
}

const string& CassettePlayer::getName() const
{
	static const string name("cassetteplayer");
	return name;
}

const string& CassettePlayer::getDescription() const
{
	// TODO: this description is not entirely accurate, but it is used
	// as an identifier for this device in e.g. Catapult. We should use
	// another way to identify devices A.S.A.P.!
	static const string desc(
		"Cassetteplayer, use to read .cas or .wav files.");
	return desc;
}

void CassettePlayer::plugHelper(Connector& connector, const EmuTime& time)
{
	lastOutput = static_cast<CassettePortInterface&>(connector).lastOut();
	updateAll(time);
}

void CassettePlayer::unplugHelper(const EmuTime& time)
{
	stopRecording(time);
}


void CassettePlayer::setVolume(int newVolume)
{
	volume = newVolume;
}

void CassettePlayer::setSampleRate(int sampleRate)
{
	delta = EmuDuration(1.0 / sampleRate);
}

void CassettePlayer::updateBuffer(unsigned length, int* buffer,
     const EmuTime& /*time*/, const EmuDuration& /*sampDur*/)
{
	if (!wavWriter.get()) {
		while (length--) {
			*(buffer++) = (((int)getSample(playTapeTime)) * volume) >> 15;
			playTapeTime += delta;
		}
	} else { // not reachable, mute is set
		assert(false);
		//while (length--) {
		//	*(buffer++) = 0; // TODO
		//playTapeTime += delta;
	}
}

void CassettePlayer::sendCommandEvent(const vector<string>& tokens)
{
	// note: might throw CommandException
	msxEventDistributor.distributeEvent(
		MSXEventDistributor::EventPtr(new MSXCommandEvent(tokens)),
		scheduler.getCurrentTime());
}

void CassettePlayer::signalEvent(
	shared_ptr<const Event> event, const EmuTime& time)
{
	if (event->getType() != OPENMSX_MSX_COMMAND_EVENT) return;

	const MSXCommandEvent* commandEvent =
		checked_cast<const MSXCommandEvent*>(event.get());
	const vector<string>& tokens = commandEvent->getTokens();
	if (tokens[0] != "cassetteplayer") return;

	commandResult.clear();
	if (tokens.size() == 1) {
		// Returning TCL lists here, similar to the disk commands in
		// DiskChanger
		TclObject tmp(commandController.getInterpreter());
		tmp.addListElement(getName() + ':');
		tmp.addListElement(casImage);

		TclObject options(commandController.getInterpreter());
		options.addListElement(wavWriter.get() ?
		                       "record" : "play");
		tmp.addListElement(options);
		// tmp.addListElement(cassette->getFirstFileTypeAsString()); <-- temporarily disabled, so that we can release openMSX 0.6.0 and Catapult 0.6.0-R1 without having to fix Catapult which breaks because of this
		commandResult += tmp.getString();
	} else if (tokens[1] == "new") {
		if (wavWriter.get()) {
			stopRecording(time);
			commandResult += "Stopping recording to " + casImage;
		}
		string filename;
		if (tokens.size() == 3) {
			filename = tokens[2];
		} else {
			filename = FileOperations::getNextNumberedFileName(
				"taperecordings", "openmsx", ".wav");
		}
		startRecording(filename, time);
		commandResult += "Created new cassette image file: " + filename
		               + ", inserted it and set recording mode.";
		casImage = filename;
		cliComm.update(CliComm::MEDIA, "cassetteplayer", filename);
		cliComm.update(CliComm::STATUS, "cassetteplayer", "record");
	} else if (tokens[1] == "insert" && tokens.size() == 3) {
		try {
			commandResult += "Changing tape";
			UserFileContext context(commandController);
			insertTape(context.resolve(tokens[2]), time);
			cliComm.update(CliComm::MEDIA,
			        "cassetteplayer", tokens[1]);
			cliComm.update(CliComm::STATUS, "cassetteplayer", "play");
		} catch (MSXException &e) {
			throw CommandException(e.getMessage());
		}
	} else if (tokens[1] == "motorcontrol" && tokens.size() == 3) {
		if (tokens[2] == "on") {
			if (!motorControl) {
				setMotorControl(true, time);
				commandResult += "Motor control enabled.";
			} else {
				commandResult += "Already enabled...";
			}
		} else if (tokens[2] == "off") {
			if (motorControl) {
				setMotorControl(false, time);
				commandResult += "Motor control disabled.";
				if (!casImage.empty()) {
					commandResult += " Tape will run now!";
				}
			} else {
				commandResult += "Already disabled...";
				if (!casImage.empty()) {
					commandResult += " (tape is running!)";
				}
			}
		} else throw SyntaxError();
	} else if (tokens.size() != 2) {
		throw SyntaxError();
	} else if (tokens[1] == "motorcontrol") {
			commandResult += "Motor control is ";
			commandResult += motorControl ? "on" : "off";
	} else if (tokens[1] == "record") {
			commandResult += "TODO: implement this... (sorry)";
	} else if (tokens[1] == "play") {
		if (wavWriter.get()) {
			try {
				commandResult += "Play mode set, rewinding tape.";
				insertTape(casImage, time);
				cliComm.update(
					CliComm::STATUS, "cassetteplayer", "play");
			} catch (MSXException &e) {
				throw CommandException(e.getMessage());
			}
		} else {
			commandResult += "Already in play mode.";
		}
	} else if (tokens[1] == "eject") {
		commandResult += "Tape ejected";
		removeTape(time);
		cliComm.update(CliComm::MEDIA, "cassetteplayer", "");
		cliComm.update(CliComm::STATUS, "cassetteplayer", "play");

	} else if (tokens[1] == "rewind") {
		if (wavWriter.get()) {
			try {
				commandResult += "First stopping recording... ";
				insertTape(casImage, time);
				cliComm.update(
					CliComm::STATUS, "cassetteplayer", "play");
				// this also did the rewinding
			} catch (MSXException &e) {
				throw CommandException(e.getMessage());
			}
		} else {
			rewind(time);
		}
		commandResult += "Tape rewound";
	} else {
		commandResult += "This syntax is deprecated, please use insert!\n";
		try {
			commandResult += "Changing tape";
			UserFileContext context(commandController);
			insertTape(context.resolve(tokens[1]), time);
			cliComm.update(CliComm::MEDIA, "cassetteplayer", tokens[1]);
			cliComm.update(CliComm::STATUS, "cassetteplayer", "play");
		} catch (MSXException &e) {
			throw CommandException(e.getMessage());
		}
	}
}


TapeCommand::TapeCommand(CommandController& commandController,
                         CassettePlayer& cassettePlayer_)
	: SimpleCommand(commandController, "cassetteplayer")
	, cassettePlayer(cassettePlayer_)
{
}

string TapeCommand::execute(const vector<string>& tokens)
{
	cassettePlayer.sendCommandEvent(tokens);
	string result = cassettePlayer.commandResult;
	if (!cassettePlayer.getConnector()) {
		if (!result.empty() && result[result.size() - 1] != '\n') {
			result += '\n';
		}
		result += "Warning: cassetteplayer not plugged in.";
	}
	return result;
}

string TapeCommand::help(const vector<string>& tokens) const
{
	string helptext;
	if (tokens.size() >= 2) {
		if (tokens[1] == "eject") {
			helptext = "Well, just eject the cassette from the cassette player/recorder!";
		} else if (tokens[1] == "rewind") {
			helptext = "Indeed, rewind the tape that is currently in the cassette player/recorder...";
		} else if (tokens[1] == "motorcontrol") {
			helptext = "Setting this to 'off' is equivalent to disconnecting the black remote plug from\n"
				"the cassette player: it makes the cassette player run (if in play mode); the\n"
				"motor signal from the MSX will be ignored. Normally this is set to 'on': the\n"
				"cassetteplayer obeys the motor control signal from the MSX.";
		} else if (tokens[1] == "play") {
			helptext = "Go to play mode. Only useful if you were in record mode (which is currently\n"
				"the only other mode available).";
		} else if (tokens[1] == "new") {
			helptext = "Create a new cassette image. If the file name is omitted, one will be\n"
				"generated in the default directory for tape recordings. Implies going to\n"
				"record mode (why else do you want a new cassette image?).";
		} else if (tokens[1] == "insert") {
			helptext = "Inserts the specified cassette image into the cassette player, rewinds it and\n"
				"switches to play mode.";
		} else if (tokens[1] == "record") {
			helptext = "Go to record mode. NOT IMPLEMENTED YET. Will be used to be able to resume\n"
				"recording to an existing cassette image, previously inserted with the insert\n"
				"command.";
		}
	} else {
		helptext = "cassetteplayer eject             : remove tape from virtual player\n"
	       "cassetteplayer rewind            : rewind tape in virtual player\n"
	       "cassetteplayer motorcontrol      : enables or disables motor control (remote)\n"
	       "cassetteplayer play              : change to play mode (default)\n"
	       "cassetteplayer record            : change to record mode (NOT IMPLEMENTED YET)\n"
	       "cassetteplayer new [<filename>]  : create and insert new tape image file and\ngo to record mode\n"
	       "cassetteplayer insert <filename> : insert (a different) tape file\n";
	}
	return helptext;
}

void TapeCommand::tabCompletion(vector<string>& tokens) const
{
	set<string> extra;
	if (tokens.size() == 2) {
		extra.insert("eject");
		extra.insert("rewind");
		extra.insert("motorcontrol");
		extra.insert("insert");
		extra.insert("new");
		extra.insert("play");
	//	extra.insert("record");
		completeString(tokens, extra);
	} else if ((tokens.size() == 3) && (tokens[1] == "insert")) {
		UserFileContext context(getCommandController());
		completeFileName(tokens, context, extra);
	} else if ((tokens.size() == 3) && (tokens[1] == "motorcontrol")) {
		extra.insert("on");
		extra.insert("off");
		completeString(tokens, extra);
	}
}

} // namespace openmsx
