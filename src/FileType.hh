#include <fstream>

#ifdef HAVE_FSTREAM_TEMPL
	class IFileType : public std::ifstream <signed_byte> {
		public:
			IFileType (const char *name) : std::ifstream <signed_byte> (name) {}
			void read (byte *p, size_t s) {
				std::ifstream <signed_byte>::read(reinterpret_cast <signed_byte *>(p), s);
			}
			void read (signed_byte *p, size_t s) {
				std::ifstream <signed_byte>::read(p, s);
			}
	};

	class OFileType : public std::ofstream <signed_byte> {
		public:
			OFileType (const char *name) : std::ofstream (name) {}
			void write (const byte *p, size_t s) {
				std::ofstream <signed_byte>::write(reinterpret_cast <const signed_byte *>(p), s);
			}
			void write (const signed_byte *p, size_t s) {
				std::ofstream <signed_byte>::write(p, s);
			}
	};

	class IOFileType : public std::fstream <signed_byte> {
		public:
			IOFileType (const char *name,std::_Ios_Openmode m) : std::fstream (name, m) {}
			void read (byte *p, size_t s) {
				std::fstream <signed_byte>::read(reinterpret_cast <signed_byte *>(p), s);
			}
			void read (signed_byte *p, size_t s) {
				std::fstream <signed_byte>::read(p, s);
			}
			void write (const byte *p, size_t s) {
				std::fstream <signed_byte>::write(reinterpret_cast <const signed_byte *>(p), s);
			}
			void write (const signed_byte *p, size_t s) {
				std::fstream <signed_byte>::write(p, s);
			}
			void get (byte &b) {
				std::fstream <signed_byte>::get(*reinterpret_cast <signed_byte *>(&b));
			}
			void get (signed_byte &b) {
				std::fstream <signed_byte>::get(b);
			}
	};
	#define IFILETYPE IFileType
	#define OFILETYPE OFileType
	#define IOFILETYPE IOFileType
#else
	#define IFILETYPE ifstream
	#define OFILETYPE ofstream
	#define IOFILETYPE fstream
#endif
