#ifndef PRIORITY_HH
#define PRIORITY_HH

namespace openmsx {

/** Priorities from high to low, higher priority listeners can block
  * events for lower priority listeners.
  */
enum Priority {
	OTHER,
	HOTKEY, // global hot keys are resolved here
	IMGUI,
	LOW_PRIORITY, // normal hot keys are resolved after ImGui
	MSX,
	LOWEST, // should only be used internally in EventDistributor
};

}

#endif
