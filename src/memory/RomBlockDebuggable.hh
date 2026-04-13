#ifndef ROMBLOCKDEBUGGABLE_HH
#define ROMBLOCKDEBUGGABLE_HH

#include "MSXDevice.hh"
#include "MSXRom.hh"
#include "SimpleDebuggable.hh"
#include "outer.hh"

#include <span>
#include <string>

namespace openmsx {

class RomBlockDebuggableBase
{
public:
	explicit RomBlockDebuggableBase(const MSXDevice& device)
		: debuggable(
			device.getMotherBoard(),
			device.getName() + " romblocks",
			"Shows for each byte of the mapper which memory block is selected.")
		, debuggable16(
			device.getMotherBoard(),
			device.getName() + " romblocks16",
			"Shows for each byte of the mapper which memory block is selected (16-bit version).")
	{
	}

	// To support larger than 8 bit segment numbers.
	[[nodiscard]] virtual unsigned readExt(unsigned address) = 0;

protected:
        struct Debuggable : SimpleDebuggable {
                explicit Debuggable(MSXMotherBoard& motherBoard_, const std::string name_, static_string_view description_)
			: SimpleDebuggable(
				motherBoard_,
				name_,
				description_,
				0x10000)
		{
		}
		[[nodiscard]] byte read(unsigned address) override
		{
			auto& outer = OUTER(RomBlockDebuggableBase, debuggable);
			return narrow_cast<byte>(outer.readExt(address));
		}
        } debuggable;

        struct DebuggableExt final : Debuggable {
                explicit DebuggableExt(MSXMotherBoard& motherBoard_, const std::string name_, static_string_view description_)
			: Debuggable(motherBoard_, name_, description_)
		{
		}
		[[nodiscard]] byte read(unsigned address) override
		{
			auto& outer = OUTER(RomBlockDebuggableBase, debuggable16);
			return narrow_cast<byte>(address & 1 ? outer.readExt(address) >> 8 : outer.readExt(address));
		}
        } debuggable16;

	std::array<uint8_t, 2> mappedLow = {0, 0};
	std::array<uint8_t, 2> mappedHigh = {0, 0};

	~RomBlockDebuggableBase() = default;
};

// Generic implementation. This (only) works when the segment numbers are stored
// in some byte-array (so limited to 8-bit segment numbers).
class RomBlockDebuggable final : public RomBlockDebuggableBase
{
public:
	RomBlockDebuggable(const MSXDevice& device, std::span<const byte> blockNr_,
	                   unsigned startAddress_, unsigned mappedSize_,
	                   unsigned bankSizeShift_, unsigned debugShift_ = 0)
		: RomBlockDebuggableBase(device)
		, blockNr(blockNr_.data()), startAddress(startAddress_)
		, mappedSize(mappedSize_), bankSizeShift(bankSizeShift_)
		, debugShift(debugShift_), debugMask(~((1 << debugShift) - 1))
	{
		assert((mappedSize >> bankSizeShift) == blockNr_.size()); // no need to include 'debugMask' here
	}
	RomBlockDebuggable(const MSXDevice& device, std::span<const byte> blockNr_,
	                   unsigned startAddress_, unsigned mappedSize_,
	                   unsigned bankSizeShift_, unsigned debugShift_,
	                   unsigned debugMask_)
		: RomBlockDebuggableBase(device)
		, blockNr(blockNr_.data()), startAddress(startAddress_)
		, mappedSize(mappedSize_), bankSizeShift(bankSizeShift_)
		, debugShift(debugShift_), debugMask(debugMask_)
	{
		assert(((mappedSize >> bankSizeShift) & debugMask) < blockNr_.size()); // here we do need 'debugMask'
	}

	[[nodiscard]] unsigned readExt(unsigned address) override
	{
		unsigned addr = address - startAddress;
		if (addr < mappedSize) {
			byte tmp = blockNr[(addr >> bankSizeShift) & debugMask];
			return (tmp != 255) ? (tmp >> debugShift) : tmp;
		} else {
			return unsigned(-1); // outside mapped address space
		}
	}

private:
	const byte* blockNr;
	const unsigned startAddress;
	const unsigned mappedSize;
	const unsigned bankSizeShift;
	const unsigned debugShift;
	const unsigned debugMask;
};

} // namespace openmsx

#endif
