#pragma once

#include "MSXDevice.hh"
#include "DeviceConfig.hh"
#include "EmuTime.hh"
#include "serialize.hh"
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <string>

namespace openmsx {

class MSXPiDeviceSimple : public MSXDevice {
public:
    explicit MSXPiDeviceSimple(const DeviceConfig& config);
    ~MSXPiDeviceSimple() override;

    void reset(EmuTime time) override;
    byte readIO(uint16_t port, EmuTime time) override;
    byte peekIO(uint16_t port, EmuTime time) const override;
    void writeIO(uint16_t port, byte value, EmuTime time) override;

private:
    void serverThread();

    // Thread & connection
    std::thread worker;
    std::atomic<bool> running{false};
    int sockfd{-1};
    std::string serverIP{"127.0.0.1"};
    int serverPort{5000};

    // Queues & synchronization
    mutable std::mutex mtx;
    std::queue<byte> rxQueue;
    std::queue<byte> txQueue;

    // Registers
    byte statusReg = 0x00;
    byte controlReg = 0x00;
    byte dataReg = 0x00;

    // MSXPi logic
    std::atomic<bool> serverAvailable{false};
    bool readRequested{false};
};
REGISTER_POLYMORPHIC_INITIALIZER(MSXDevice, MSXPiDeviceSimple, "MSXPiDeviceSimple");
} // namespace openmsx
