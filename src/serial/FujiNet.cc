#include "FujiNet.hh"
#include "Timer.hh"

#include "GlobalSettings.hh"
#include "serialize.hh"
#include <sys/_types/_ssize_t.h>

namespace openmsx {

FujiNet::FujiNet(DeviceConfig& config)
    : MSXDevice(config)
    , rom(getName() + " ROM", "rom", config)
{
    stopReading = false;
    thread = std::thread(&FujiNet::readPty, this);

    pty_fd = open("/dev/ptmx", O_RDWR);
    grantpt(pty_fd);
    unlockpt(pty_fd);
    pty_name = std::string(ptsname(pty_fd));

    getCliComm().printInfo("FujiNet: opened ", pty_name);
}

FujiNet::~FujiNet()
{
    stopReading = true;

    if (thread.joinable()) {
		poller.abort();
		thread.join();
	}
}

void FujiNet::readPty()
{
    if (pty_fd == -1) {
        return;
    }

    getCliComm().printInfo("FujiNet: Start read loop");
    while (!stopReading) {
        if (!poller.poll(pty_fd)) {
            char buf[2048];
            ssize_t len = read(pty_fd, &buf, 2048);
            getCliComm().printInfo("FujiNet: Received ", len, " bytes");

            if (len > 0) {
                std::lock_guard lock(mtx);
                for (auto i : xrange(std::min<size_t>(len, 2048 - rxBuffer.size()))) {
                    rxBuffer.push_back(buf[i]);
                }
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

// IO_OFFSET       EQU     (0x4000 + 0x3FFC)
// IO_GETC         EQU     (IO_OFFSET + 0)
// IO_STATUS       EQU     (IO_OFFSET + 1)
// IO_PUTC         EQU     (IO_OFFSET + 2)
// IO_CONTROL      EQU     (IO_OFFSET + 3)

uint8_t FujiNet::readMem(uint16_t address, EmuTime time)
{
    std::lock_guard lock(mtx);
    // getCliComm().printInfo("FujiNet: readMem() ", address);
    switch (address) {
        case 0x7FFC: // IO_GETC
            getCliComm().printInfo("FujiNet: GETC");
            if (!rxBuffer.empty()) {
				return rxBuffer.pop_front();
			}
            return 0x00;
        case 0x7FFD: // IO_STATUS
            getCliComm().printInfo("FujiNet: STATUS");
            if (!rxBuffer.empty()) {
                return 0b1000000; // data available
			}
            return 0x00;
        default:
            if (0x4000 <= address && address < 0x8000) {
          		return rom[address & 0x3FFF];
           	}
           	return 0xFF;
    }
}

uint8_t FujiNet::peekMem(uint16_t address, EmuTime time) const
{
    std::lock_guard lock(mtx);
    // getCliComm().printInfo("FujiNet: peekMem() ", address);
    switch (address) {
        case 0x7FFC: // IO_GETC
        case 0x7FFD: // IO_STATUS
            if (!rxBuffer.empty()) {
                return 0b1000000; // data available
			}
            return 0x00;
        default:
            break;
    }
}

void FujiNet::writeMem(uint16_t address, uint8_t value, EmuTime time)
{
    std::lock_guard lock(mtx);
    // getCliComm().printInfo("FujiNet: writeMem() ", address, " ", value);
    switch (address) {
        case 0x7FFE: // IO_PUTC
            if (pty_fd > -1) {
                getCliComm().printInfo("FujiNet: PUTC ", value);
                write(pty_fd, &value, 1);
            }
            return;
        default:
            break;
    }
}

// void FujiNet::globalWrite(uint16_t address, uint8_t value, EmuTime time)
// {
//     getCliComm().printInfo("FujiNet: globalWrite()", address, value);
//     switch (address) {
//         case 0x7FFE: // IO_PUTC
//             if (pty_fd > -1) {
//                 std::lock_guard lock(mtx);
//                 write(pty_fd, &value, 1);
//             }
//             return;
//         default:
//             return;
//     }
// }

template<typename Archive>
void FujiNet::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);

	// ar.serialize("FujiNet", 1);
}

INSTANTIATE_SERIALIZE_METHODS(FujiNet);
REGISTER_MSXDEVICE(FujiNet, "FujiNet");

} // namespace openmsx
