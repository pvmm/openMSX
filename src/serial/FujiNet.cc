#include "FujiNet.hh"
#include "Timer.hh"

#include "GlobalSettings.hh"
#include "serialize.hh"
#include <cstddef>
#include <cstdint>
#include <sys/_types/_ssize_t.h>
#include <stdio.h>

namespace openmsx {

static constexpr size_t MAX_BUF_LEN     = 64 * 1024;
// static constexpr size_t MAX_BUF_LEN     = 256;
static constexpr size_t IO_GETC_ADDR    = 0xBFFC;
static constexpr size_t IO_STATUS_ADDR  = 0xBFFD;
static constexpr size_t IO_PUTC_ADDR    = 0xBFFE;
static constexpr size_t IO_CONTROL_ADDR = 0xBFFF;

FujiNet::FujiNet(DeviceConfig& config)
    : MSXDevice(config)
    , rom(getName() + " ROM", "rom", config)
{
    pty_fd = open("/dev/ptmx", O_RDWR | O_NONBLOCK);
    grantpt(pty_fd);
    unlockpt(pty_fd);
    pty_name = std::string(ptsname(pty_fd));

    getCliComm().printInfo("FujiNet: opened ", pty_name);

    thread = std::thread(&FujiNet::readPty, this);
    stopReading = false;
}

FujiNet::~FujiNet()
{
    stopReading = true;

    if (thread.joinable()) {
		thread.join();
	}
}

void FujiNet::readPty()
{
    getCliComm().printInfo("FujiNet: Start read loop");
    char buf[MAX_BUF_LEN];
    while (!stopReading) {
        // Check if we can read; Next read() may block if not
        if (read(pty_fd, &buf, 0) == -1) {
            continue;
        }

        ssize_t n = read(pty_fd, &buf, MAX_BUF_LEN);
        if (n > 0) {
            getCliComm().printInfo("FujiNet: Read ", n, " bytes from pty");
            std::string str(buf, n);
            getCliComm().printInfo(str);
            std::lock_guard lock(mtx);
            for (auto i : xrange(std::min<size_t>(n, MAX_BUF_LEN - rxBuffer.size()))) {
                rxBuffer.push_back(buf[i]);
            }
        }

        // Timer::sleep(20'000);
    }
}

void FujiNet::reset(EmuTime time)
{
    if (pty_fd > -1) {
        // write(pty_fd, "reset\n", 6);
    }
}

uint8_t FujiNet::readMem(uint16_t address, EmuTime time)
{
    std::lock_guard lock(mtx);
    // getCliComm().printInfo("FujiNet: readMem() ", address);
    switch (address) {
        case IO_GETC_ADDR:
            if (!rxBuffer.empty()) {
                uint8_t value = rxBuffer.pop_front();

                char formatted[16];
                if (value > 31 && value < 127)
                    sprintf(formatted, "$%02X %c", value, value);
                else
                    sprintf(formatted, "$%02X", value);
                getCliComm().printInfo("FujiNet: GETC -> ", formatted);

				return value;
			}
			else {
			    getCliComm().printInfo("FujiNet: GETC -> empty!");
			}
            return 0x00;
        case IO_STATUS_ADDR:
            if (!rxBuffer.empty()) {
                getCliComm().printInfo("FujiNet: STAT -> data available");
                return 0b10000000; // data available
			}
			else {
    			getCliComm().printInfo("FujiNet: STAT -> no data");
			}
            return 0x00;
        default:
            if (0x4000 <= address && address < 0xC000) {
          		return rom[address - 0x4000];
           	}
           	return 0xFF;
    }
}

uint8_t FujiNet::peekMem(uint16_t address, EmuTime time) const
{
    std::lock_guard lock(mtx);
    // getCliComm().printInfo("FujiNet: peekMem() ", address);
    switch (address) {
        case IO_GETC_ADDR:
        case IO_STATUS_ADDR:
            if (!rxBuffer.empty()) {
                return 0b10000000; // data available
			}
            return 0x00;
        default:
            break;
    }
    return 0x00;
}

void FujiNet::writeMem(uint16_t address, uint8_t value, EmuTime time)
{
    std::lock_guard lock(mtx);
    // getCliComm().printInfo("FujiNet: writeMem() ", address, " ", value);
    switch (address) {
        case IO_PUTC_ADDR: // IO_PUTC
            if (pty_fd > -1) {
                char formatted[16];
                if (value > 31 && value < 127)
                    sprintf(formatted, "$%02X %c", value, value);
                else
                    sprintf(formatted, "$%02X", value);
                getCliComm().printInfo("FujiNet: PUTC ", formatted);
                write(pty_fd, &value, 1);
            }
            return;
        default:
            break;
    }
}

template<typename Archive>
void FujiNet::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
}

INSTANTIATE_SERIALIZE_METHODS(FujiNet);
REGISTER_MSXDEVICE(FujiNet, "FujiNet");

} // namespace openmsx
