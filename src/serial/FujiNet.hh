#ifndef FUJINET_HH
#define FUJINET_HH

#include "MSXDevice.hh"

#include "BooleanSetting.hh"
#include "EventListener.hh"
#include "Socket.hh"
#include "StringSetting.hh"

#include "Poller.hh"
#include "circular_buffer.hh"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <thread>

namespace openmsx {

class DeviceConfig;

class FujiNet final : public MSXDevice
{
public:
	explicit FujiNet(const DeviceConfig& config);
	~FujiNet() override;

	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] uint8_t peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run(); // loop of helper thread that reads from 'sockfd'

	std::thread thread; // receiving thread (reads from pty)
	std::mutex mutex; // to protect shared data between emulation and receiving thread
	std::optional<Poller> poller; // safe to use from main and receiver thread without extra locking
	cb_queue<char> queue; // read/written by both the main and the receiver thread. Must hold 'mutex' while doing so.
	std::atomic<SOCKET> sockfd; // read/written by both threads (use std::atomic as an alternative for locking)
	int pty_fd;
};

} // namespace openmsx

#endif
