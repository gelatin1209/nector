#ifndef nector_input_state_h
#define nector_input_state_h

#include "input.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>

struct nector_input {
    std::deque<nector_input_event> queue;
    std::array<bool, 4096> keys{};
    std::array<int64_t, 4096> key_pressed_since_ns{};
    std::array<bool, 8> buttons{};
    std::map<int64_t, nector_input_event> touches;
    float mouse_x = 0;
    float mouse_y = 0;
    float wheel_x = 0;
    float wheel_y = 0;
    mutable std::mutex mutex;
};

namespace input_internal {

constexpr size_t input_queue_capacity = 4096;

// Caller must hold input->mutex.
void clearInputStateLocked(nector_input_handle input);

}

#endif
