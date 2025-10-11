#ifndef FUJINET_HH
#define FUJINET_HH

#include "MSXCPUInterface.hh"
#include "MSXCliComm.hh"
#include "MSXDevice.hh"
#include "Rom.hh"

// #include "BooleanSetting.hh"
// #include "EventListener.hh"
// #include "Socket.hh"
// #include "StringSetting.hh"

#include "Poller.hh"
#include "circular_buffer.hh"

// #include <atomic>
#include <cstdint>
#include <mutex>
// #include <optional>
// #include <span>
#include <thread>

namespace openmsx {

class DeviceConfig;

class FujiNet final
    : public MSXDevice
{
public:
	explicit FujiNet(DeviceConfig& config);
	~FujiNet() override;

	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] uint8_t peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, uint8_t value, EmuTime time) override;
	// void globalWrite(uint16_t address, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void readPty(); // loop of helper thread that reads from 'sockfd'

	Rom rom;
	std::thread thread; // receiving thread (reads from pty)
	mutable std::mutex mtx; // to protect shared data between emulation and receiving thread
	Poller poller;
	cb_queue<char> rxBuffer; // read/written by both the main and the receiver thread. Must hold 'mutex' while doing so.
	// std::atomic<SOCKET> sockfd; // read/written by both threads (use std::atomic as an alternative for locking)
	int pty_fd;
	std::string pty_name;
	bool stopReading;
};

} // namespace openmsx

#endif
