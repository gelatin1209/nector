#ifndef nector_input_clock_h
#define nector_input_clock_h

#include <cstdint>

namespace input_internal {

constexpr int64_t nanoseconds_per_millisecond = 1000000;

int64_t monotonicTimeNs();
bool timestampToNs(int64_t timestamp_ms, int64_t& timestamp_ns);

}

#endif
