#include "FujiNet.hh"

#include "EventDistributor.hh"
#include "PlugException.hh"
#include "Scheduler.hh"
#include "serialize.hh"

#include "StringOp.hh"
#include "checked_cast.hh"

// #include <sys/stat.h>
// #include <fcntl.h>
// #include <stdlib.h>
// #include <stdio.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>


namespace openmsx {

FujiNet::FujiNet(const DeviceConfig& config)
    : MSXDevice(config)
{
	// int master, slave;
	// char *name;
	// // openpty(&master, &slave, name, NULL, NULL);
	// openpty(&master, &slave, NULL, NULL, NULL);
	// // pty = (FILE*)master;

	// char *slavename;
    // int masterfd;
    // masterfd = open("/dev/ptmx", O_RDWR);
    // grantpt(masterfd);
    // unlockpt(masterfd);
    // slavename = ptsname(masterfd);
    // pty_fd = masterfd;
    // pty_fd = fdopen(masterfd, NULL);

    thread = std::thread(&FujiNet::run, this);

    // std::fputs("Hi!\n", pty_fd);
    // write(masterfd, "Hi\n", 3);

    // eventDistributor.registerEventListener(EventType::RS232_NET, *this);
	//
}

FujiNet::~FujiNet()
{
	if (thread.joinable()) {
		// poller.abort();
		thread.join();
	}

	// std::fputs("Bye!\n", pty_fd);
}

void FujiNet::run()
{
    printf("x");
    // std::fputc('.', pty_fd);
}

void FujiNet::reset(EmuTime time)
{
    printf("x");
}

uint8_t FujiNet::readMem(uint16_t address, EmuTime time)
{
    return 0xFF;
}

uint8_t FujiNet::peekMem(uint16_t address, EmuTime time) const
{
    return 0xFF;
}

void FujiNet::writeMem(uint16_t address, uint8_t value, EmuTime time)
{
    printf("x");
}

template<typename Archive>
void FujiNet::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);

	// ar.serialize("FujiNet", 1);
}

INSTANTIATE_SERIALIZE_METHODS(FujiNet);
REGISTER_MSXDEVICE(FujiNet, "FujiNet");

} // namespace openmsx
