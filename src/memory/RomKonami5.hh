// $Id$

#ifndef __ROMKONAMI5_HH__
#define __ROMKONAMI5_HH__

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomKonami5 : public Rom8kBBlocks
{
public:
	RomKonami5(Config* config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~RomKonami5();
	
	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	class SCC* scc;
	bool sccEnabled;
};

} // namespace openmsx

#endif
