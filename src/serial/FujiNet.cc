#include "FujiNet.hh"
#include "Timer.hh"

#include "GlobalSettings.hh"
#include "serialize.hh"
#include <sys/_types/_ssize_t.h>

namespace openmsx {

static constexpr size_t MAX_BUF_LEN     = 16 * 1024;
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
            getCliComm().printInfo("FujiNet: Received ", n, " bytes");
            std::lock_guard lock(mtx);
            for (auto i : xrange(std::min<size_t>(n, MAX_BUF_LEN - rxBuffer.size()))) {
                rxBuffer.push_back(buf[i]);
            }
        }
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
            getCliComm().printInfo("FujiNet: GETC");
            if (!rxBuffer.empty()) {
				return rxBuffer.pop_front();
			}
            return 0x00;
        case IO_STATUS_ADDR:
            // getCliComm().printInfo("FujiNet: STATUS");
            if (!rxBuffer.empty()) {
                return 0b10000000; // data available
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
                getCliComm().printInfo("FujiNet: PUTC ", value);
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
