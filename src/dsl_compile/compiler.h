#ifndef nector_cdsl_compiler_h
#define nector_cdsl_compiler_h

#include <cstdint>
#include <string>
#include <vector>

namespace nector_cdsl_detail {
bool compileSource(const char* source, std::vector<uint8_t>& output, std::string& error);
}
#endif
