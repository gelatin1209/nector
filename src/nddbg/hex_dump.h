#ifndef nector_nddbg_hex_dump_h
#define nector_nddbg_hex_dump_h

#include <cstddef>
#include <cstdint>
#include <iosfwd>

namespace nddbg {

void writeHexDump(std::ostream& output, const uint8_t* data, size_t size);

}
#endif
