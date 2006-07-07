// $Id$

#ifndef Y8950PERIPHERY_HH
#define Y8950PERIPHERY_HH

#include "openmsx.hh"

namespace openmsx {

class EmuTime;

/** Models the 4 general purpose I/O pins on the Y8950
  * (controlled by registers r#18 and r#19)
  */
class Y8950Periphery
{
public:
	virtual ~Y8950Periphery() {}

	/** Write to (some of) the pins
	  * @param outputs A '1' bit indicates the corresponding bit is
	  *                programmed as output.
	  * @param values The actual value that is written, only bits for
	  *               which the corresponding bit in the 'outputs'
	  *               parameter is set are meaningful.
	  */
	virtual void write(nibble outputs, nibble values, const EmuTime& time) = 0;

	/** Read from (some of) the pins
	  * Some of the pins might be programmed as output, but this method
	  * doesn't care about that, it should return the value of all pins
	  * as-if they were all programmed as input.
	  */
	virtual nibble read(const EmuTime& time) = 0;
};

} // namespace openmsx

#endif
