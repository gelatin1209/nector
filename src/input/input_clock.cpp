#include "input_clock.h"

#include <chrono>
#include <limits>

namespace input_internal {

int64_t monotonicTimeNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool timestampToNs(int64_t timestamp_ms, int64_t& timestamp_ns) {
    if (timestamp_ms < 0 || timestamp_ms > std::numeric_limits<int64_t>::max() / nanoseconds_per_millisecond) {
        return false;
    }
    timestamp_ns = timestamp_ms * nanoseconds_per_millisecond;
    return true;
}

}
