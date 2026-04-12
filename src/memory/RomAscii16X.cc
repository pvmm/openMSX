// ASCII-X 16kB cartridges
//
// Banks:
//  first  16kB: 0x4000 - 0x7FFF and 0xC000 - 0xFFFF
//  second 16kB: 0x8000 - 0xBFFF and 0x0000 - 0x3FFF
//
// Address to change banks:
//  first  16kB: 0x6000 - 0x6FFF, 0xA000 - 0xAFFF, 0xE000 - 0xEFFF, 0x2000 - 0x2FFF
//  second 16kB: 0x7000 - 0x7FFF, 0xB000 - 0xBFFF, 0xF000 - 0xFFFF, 0x3000 - 0x3FFF
//
// 12-bit bank number is composed by address bits 8-11 and the data bits.
//
// Backed by FlashROM.

#include "RomAscii16X.hh"
#include "narrow.hh"
#include "outer.hh"
#include "serialize.hh"

namespace openmsx {

RomAscii16X::RomAscii16X(DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, debuggable(*this)
	, debuggableExt(*this, getName() + " romext", mappedHigh, 0x4000, 0x8000, 14)
	, flash(rom, AmdFlashChip::S29GL064S70TFI040, {}, config)
{
	reset(EmuTime::dummy());
}

void RomAscii16X::reset(EmuTime /*time*/)
{
	ranges::iota(bankRegs, uint16_t(0));

	flash.reset();

	invalidateDeviceRCache(); // flush all to be sure
}

unsigned RomAscii16X::getFlashAddr(uint16_t addr) const
{
	const uint16_t index = ((addr >> 14) & 1) ^ 1;
	// new
	uint16_t bank1 = (mappedHigh[index] << 8) | mappedLow[index];
	unsigned tmp1 = (bank1 << 14) | (addr & 0x3FFF);
	// old
	uint16_t bank2 = bankRegs[index];
	// "(addr >> 14 & 1) ^ 1" defines a page
	// 0x0000 -> 1, 0x4000 -> 0, 0x8000 -> 1, 0xc000 -> 0
	unsigned tmp2 = (bank2 << 14) | (addr & 0x3FFF);
	// compare
std::cout << std::hex << addr << ": getFlashAddr(" << tmp1 << ", " << tmp2 << ")\n";
	return tmp2;
}

byte RomAscii16X::readMem(uint16_t addr, EmuTime time)
{
	return flash.read(getFlashAddr(addr), time);
}

byte RomAscii16X::peekMem(uint16_t addr, EmuTime time) const
{
	return flash.peek(getFlashAddr(addr), time);
}

const byte* RomAscii16X::getReadCacheLine(uint16_t addr) const
{
	return flash.getReadCacheLine(getFlashAddr(addr));
}

void RomAscii16X::writeMem(uint16_t addr, byte value, EmuTime time)
{
	flash.write(getFlashAddr(addr), value, time);

	if ((addr & 0x3FFF) >= 0x2000) {
		const uint16_t index = (addr >> 12) & 1; // 0x6000 -> 0 ou 0x7000 -> 1
		// new
		mappedLow[index] = value;
		mappedHigh[index] = (addr >> 8) & 0x0F;
		unsigned tmp1 = (mappedHigh[index] << 8) | mappedLow[index];
		// old
		unsigned tmp2 = (addr & 0x0F00) | value; // banknumber (lsb + msb)
		bankRegs[index] = tmp2; // banknumber (lsb + msb)
		invalidateDeviceRCache(0x4000 ^ (index << 14), 0x4000);
		invalidateDeviceRCache(0xC000 ^ (index << 14), 0x4000);
		// compare
std::cout << std::hex << addr << ": writeMem(" << tmp1 << ", " << tmp2 << ")\n";
	}
}

byte* RomAscii16X::getWriteCacheLine(uint16_t /* addr */)
{
	return nullptr; // not cacheable
}

unsigned RomAscii16X::Debuggable::readExt(unsigned address)
{
	auto& outer = OUTER(RomAscii16X, debuggable);
	uint8_t index = ((address >> 14) & 1) ^ 1;
	// new
	unsigned tmp1 = (outer.mappedHigh[index] << 8) | outer.mappedLow[index];
	// old
	unsigned tmp2 = outer.bankRegs[index];
	// compare
std::cout << std::hex << address << ": readExt(" << tmp1 << ", " << tmp2 << ")\n";
	return tmp2;
}

template<typename Archive>
void RomAscii16X::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("flash",       flash,
	             "bankRegs",    bankRegs);
}
INSTANTIATE_SERIALIZE_METHODS(RomAscii16X);
REGISTER_MSXDEVICE(RomAscii16X, "RomAscii16X");

} // namespace openmsx
