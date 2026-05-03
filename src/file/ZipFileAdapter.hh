#ifndef ZIPFILEADAPTER_HH
#define ZIPFILEADAPTER_HH

#include "CompressedFileAdapter.hh"

namespace openmsx {

class ZipFileAdapter final : public CompressedFileAdapter
{
public:
	explicit ZipFileAdapter(std::unique_ptr<FileBase> file, zstring_view filename);

protected:
	std::array<std::string, 2> getExtensions() const;

private:
	void decompress(FileBase& file, Decompressed& decompressed) override;
};

} // namespace openmsx

#endif
