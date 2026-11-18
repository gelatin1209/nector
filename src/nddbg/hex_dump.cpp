#include "hex_dump.h"

#include <iomanip>
#include <ostream>
#include <sstream>

namespace nddbg {

void writeHexDump(std::ostream& output, const uint8_t* data, size_t size) {
    output << "hex:\n";
    for (size_t offset = 0; offset < size; offset += 16) {
        const size_t count = size - offset < 16 ? size - offset : 16;
        std::ostringstream line;
        line << std::hex << std::setfill('0') << std::setw(8) << offset << "  ";
        for (size_t index = 0; index < 16; ++index) {
            if (index < count) {
                line << std::setw(2) << static_cast<unsigned>(data[offset + index]) << ' ';
            } else {
                line << "   ";
            }
        }
        line << " |";
        for (size_t index = 0; index < count; ++index) {
            const uint8_t value = data[offset + index];
            line << static_cast<char>(value >= 32 && value <= 126 ? value : '.');
        }
        line << "|\n";
        output << line.str();
    }
}

}
