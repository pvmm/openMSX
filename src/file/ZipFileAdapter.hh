#ifndef ZIPFILEADAPTER_HH
#define ZIPFILEADAPTER_HH

#include "CompressedFileAdapter.hh"

#include <string>

namespace openmsx {

class ZipFileAdapter final : public CompressedFileAdapter
{
public:
	explicit ZipFileAdapter(std::unique_ptr<FileBase> file, zstring_view filename, zstring_view extension);

private:
	void decompress(FileBase& file, Decompressed& decompressed) override;
	std::string extension;
	std::string archiveStem;
};

} // namespace openmsx

#endif
