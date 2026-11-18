#include "internal.h"

#include <array>
#include <cstring>
#include <exception>

namespace {
thread_local std::string last_error;
thread_local nector_status last_status = nector_ok;
}

const char* nectorGetVersion(void) {
    return "0.1.0";
}

const char* nectorGetLastError(void) {
    return last_error.c_str();
}

void nectorClearError(void) {
    last_error.clear();
    last_status = nector_ok;
}

const char* nectorStatusText(nector_status status) {
    switch (status) {
        case nector_ok: return "ok";
        case nector_invalid_argument: return "invalid argument";
        case nector_out_of_memory: return "out of memory";
        case nector_io_error: return "I/O error";
        case nector_invalid_format: return "invalid format";
        case nector_unsupported: return "unsupported";
        case nector_invalid_state: return "invalid state";
        case nector_not_found: return "not found";
        case nector_limit_exceeded: return "limit exceeded";
        case nector_backend_error: return "backend error";
        case nector_callback_error: return "callback error";
        default: return "unknown status";
    }
}

void nectorCoreSetError(nector_status status, const char* message) {
    last_status = status;
    try {
        last_error = message == nullptr ? nectorStatusText(status) : message;
    } catch (...) {
        last_error.clear();
    }
}

nector_status nectorCoreValidateUtf8(const char* text, size_t length) {
    if (text == nullptr && length != 0) {
        return nectorFail(nector_invalid_argument, "UTF-8 buffer is null");
    }
    size_t index = 0;
    while (index < length) {
        const uint8_t first = static_cast<uint8_t>(text[index]);
        if (first <= 0x7f) {
            ++index;
            continue;
        }
        int continuation_count = 0;
        uint32_t codepoint = 0;
        uint32_t minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            continuation_count = 1;
            codepoint = first & 0x1f;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            continuation_count = 2;
            codepoint = first & 0x0f;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuation_count = 3;
            codepoint = first & 0x07;
            minimum = 0x10000;
        } else {
            return nectorFail(nector_invalid_format, "invalid UTF-8 leading byte");
        }
        if (index + static_cast<size_t>(continuation_count) >= length) {
            return nectorFail(nector_invalid_format, "truncated UTF-8 sequence");
        }
        for (int offset = 1; offset <= continuation_count; ++offset) {
            const uint8_t next = static_cast<uint8_t>(text[index + static_cast<size_t>(offset)]);
            if ((next & 0xc0) != 0x80) {
                return nectorFail(nector_invalid_format, "invalid UTF-8 continuation byte");
            }
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            return nectorFail(nector_invalid_format, "non-scalar or overlong UTF-8 sequence");
        }
        index += static_cast<size_t>(continuation_count + 1);
    }
    return nector_ok;
}

uint32_t nectorCoreCrc32(const uint8_t* data, size_t length) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> result{};
        for (uint32_t index = 0; index < 256; ++index) {
            uint32_t value = index;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1) ? 0xedb88320u ^ (value >> 1) : value >> 1;
            }
            result[index] = value;
        }
        return result;
    }();
    if (data == nullptr && length != 0) {
        return 0;
    }
    uint32_t crc = 0xffffffffu;
    for (size_t index = 0; index < length; ++index) {
        crc = table[(crc ^ data[index]) & 0xff] ^ (crc >> 8);
    }
    return crc ^ 0xffffffffu;
}
