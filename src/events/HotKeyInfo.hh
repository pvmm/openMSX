#ifndef HOTKEYINFO_HH
#define HOTKEYINFO_HH

#include "Event.hh"

#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

struct HotKeyInfo {
	HotKeyInfo(Event event_, std::string command_,
	           bool repeat_ = false, bool passEvent_ = false)
		: event(std::move(event_)), command(std::move(command_))
		, repeat(repeat_)
		, passEvent(passEvent_) {}
	Event event;
	std::string command;
	bool repeat;
	bool passEvent; // whether to pass event with args back to command
};

using BindMap = std::vector<HotKeyInfo>;

}

#endif
