#ifndef nector_font_woff2_h
#define nector_font_woff2_h

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nector_font_detail {

bool decodeWoff2(const uint8_t* data, size_t size, std::vector<uint8_t>& out_sfnt,
                 std::string& error);

}
#endif
