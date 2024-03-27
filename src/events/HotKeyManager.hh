#ifndef HOTKEYMANAGER_HH
#define HOTKEYMANAGER_HH

#include "HotKey.hh"
#include "Command.hh"
#include "Event.hh"
#include "EventListener.hh"
#include "EventDistributor.hh"
#include "RTSchedulable.hh"

#include "TclObject.hh"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

// struct HotKeyInfo {
// 	HotKeyInfo(Event event_, std::string command_, bool repeat_ = false,
// 	           bool passEvent_ = false, bool global_ = false)
// 		: event(std::move(event_)), command(std::move(command_))
// 		, repeat(repeat_)
// 		, passEvent(passEvent_)
// 		, global(global_) {}
// 	Event event;
// 	std::string command;
// 	bool repeat;
// 	bool passEvent; // whether to pass event with args back to command
// 	bool global; // whether event has highest priority
// };

// using BindMap = std::vector<HotKeyInfo>; // unsorted
// using KeySet  = std::vector<Event>;   // unsorted

class HotKeyManager final
{
public:
	HotKeyManager(RTScheduler& rtScheduler,
	              GlobalCommandController& commandController,
	              EventDistributor& eventDistributor);
	~HotKeyManager();

// 	void loadInit();
// 	void loadBind(std::string_view key, std::string_view cmd, bool repeat, bool event, bool global);
// 	void loadUnbind(std::string_view key);

// 	template<typename XmlStream>
// 	void saveBindings(XmlStream& xml) const
// 	{
// 		xml.begin("bindings");
// 		// add explicit bind's
// 		for (const auto& k : boundKeys) {
// 			xml.begin("bind");
// 			xml.attribute("key", toString(k));
// 			const auto& info = *find_unguarded(cmdMap, k, &HotKeyInfo::event);
// 			if (info.repeat) {
// 				xml.attribute("repeat", "true");
// 			}
// 			if (info.passEvent) {
// 				xml.attribute("event", "true");
// 			}
// 			if (info.global) {
// 				xml.attribute("global", "true");
// 			}
// 			xml.data(info.command);
// 			xml.end("bind");
// 		}
// 		// add explicit unbinds
// 		for (const auto& k : unboundKeys) {
// 			xml.begin("unbind");
// 			xml.attribute("key", toString(k));
// 			xml.end("unbind");
// 		}
// 		xml.end("bindings");
// 	}

// 	const auto& getGlobalBindings() const { return cmdMap; }

// private:
// 	void initDefaultBindings();
// 	void bind         (HotKeyInfo&& info);
// 	void unbind       (const Event& event);
// 	void bindDefault  (HotKeyInfo&& info);
// 	void unbindDefault(const Event& event);
// 	void bindLayer    (HotKeyInfo&& info, const std::string& layer);
// 	void unbindLayer  (const Event& event, const std::string& layer);
// 	void unbindFullLayer(const std::string& layer);
// 	void activateLayer  (std::string layer, bool blocking);
// 	void deactivateLayer(std::string_view layer);

private:
  HotKey hotKey;
	// GlobalCommandController& commandController;
	// EventDistributor& eventDistributor;
};

} // namespace openmsx

#endif
