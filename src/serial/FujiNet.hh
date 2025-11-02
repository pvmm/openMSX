#ifndef FUJINET_HH
#define FUJINET_HH

#include "MSXCPUInterface.hh"
#include "MSXCliComm.hh"
#include "MSXDevice.hh"
#include "Socket.hh"
#include "Rom.hh"

#include "circular_buffer.hh"

#include <cstdint>
#include <mutex>
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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
    void close();
	void readSocket(); // loop of helper thread that reads from socket

	Rom rom;
	std::thread thread; // receiving thread (reads from pty)
	mutable std::mutex mtx; // to protect shared data between emulation and receiving thread
	cb_queue<char> rxBuffer; // read/written by both the main and the receiver thread. Must hold 'mutex' while doing so.
	std::atomic<SOCKET> sock = OPENMSX_INVALID_SOCKET;
	bool stopReading;
};

} // namespace openmsx

#endif
