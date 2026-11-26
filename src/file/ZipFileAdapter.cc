#include "ZipFileAdapter.hh"

#include "FileException.hh"
#include "FileOperations.hh"
#include "MappedFile.hh"
#include "StringOp.hh"
#include "ZlibInflate.hh"

#include "ranges.hh"

#include <array>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

namespace openmsx {

namespace {

// Extensions openMSX accepts on the command line (see --help).
constexpr auto KNOWN_EXTENSIONS = std::to_array<std::string_view>({
	"cas", "tsx", "wav",
	"col", "mx1", "mx2", "ri", "rom", "sg",
	"di1", "di2", "dmk", "dsk", "fd1", "fd2", "xsa",
	"omr", "oms", "tcl", "ogv"
});

[[nodiscard]] bool isKnownExtension(std::string_view extension)
{
	return std::ranges::any_of(KNOWN_EXTENSIONS, [&](auto known) {
		return StringOp::casecmp{}(extension, known);
	});
}

// Only check the first few entries. Archives can contain a huge number of
// files, while the file we are looking for is usually near the start.
constexpr unsigned MAX_ENTRIES = 3;

struct Entry {
	std::string name;
	size_t dataOffset;
	unsigned method;
	unsigned originalSize;
};

} // namespace

ZipFileAdapter::ZipFileAdapter(std::unique_ptr<FileBase> file_, zstring_view filename_, zstring_view extension_)
	: CompressedFileAdapter(std::move(file_), filename_)
	, extension(extension_)
	, archiveStem(FileOperations::stem(filename_))
{
}

void ZipFileAdapter::decompress(FileBase& f, Decompressed& d)
{
	auto mmap = MappedFile<const uint8_t>(f.mmap(0, true));
	ZlibInflate zlib(mmap);

	// Keep track of our position ourselves, so we can later start inflating
	// at the data of the selected entry.
	size_t offset = 0;
	auto read16 = [&] { offset += 2; return zlib.get16LE(); };
	auto read32 = [&] { offset += 4; return zlib.get32LE(); };
	auto skip = [&](size_t n) { offset += n; zlib.skip(n); };
	auto readString = [&](size_t n) { offset += n; return zlib.getString(n); };

	std::optional<Entry> fallback;  // first entry with an acceptable extension
	std::optional<Entry> selected;  // entry with the same name as the archive

	for (unsigned i = 0; i < MAX_ENTRIES; ++i) {
		if (read32() != 0x04034B50) break; // not a local file header

		// skip "version needed to extract" and "general purpose bit flag"
		skip(2 + 2);

		// compression method
		unsigned method = read16();
		if (method != 0 && method != 0x0008) {
			throw FileException("Unsupported zip compression method");
		}

		// skip "last mod file time", "last mod file data", "crc32"
		skip(2 + 2 + 4);

		unsigned compSize = read32(); // compressed size
		unsigned origSize = read32(); // uncompressed size
		unsigned filenameLen = read16();
		unsigned extraFieldLen = read16();
		std::string name = readString(filenameLen);
		skip(extraFieldLen); // skip "extra field"
		size_t dataOffset = offset;

		bool acceptable;
		if (extension.empty()) {
			auto ext = FileOperations::getExtension(name);
			if (!ext.empty()) ext.remove_prefix(1);
			acceptable = isKnownExtension(ext);
		} else {
			// A specific extension was requested (e.g. a ROM from the
			// software database); that takes priority over everything else.
			acceptable = StringOp::toLower(name).ends_with(extension);
		}

		if (acceptable) {
			Entry entry{std::move(name), dataOffset, method, origSize};
			if (!extension.empty() ||
			    StringOp::casecmp{}(FileOperations::stem(entry.name), archiveStem)) {
				selected = std::move(entry);
				break;
			}
			if (!fallback) fallback = std::move(entry);
		}
		skip(compSize);
	}

	auto& choice = selected ? selected : fallback;
	if (!choice) {
		throw FileException("No suitable file found in ZIP archive");
	}

	if (choice->method == 0) {
		// Entry was stored without compression.
		d.buf = MemBuffer<uint8_t>(choice->originalSize);
		std::memcpy(d.buf.data(), mmap.data() + choice->dataOffset,
		            choice->originalSize);
	} else {
		ZlibInflate data(std::span{mmap.data() + choice->dataOffset,
		                           mmap.size() - choice->dataOffset});
		d.buf = data.inflate(choice->originalSize);
	}
	d.originalName = std::move(choice->name);
}

} // namespace openmsx
