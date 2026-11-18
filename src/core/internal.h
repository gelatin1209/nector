#ifndef nector_core_internal_h
#define nector_core_internal_h

#include "core.h"
#include <cmath>
#include <limits>
#include <new>
#include <string>

inline nector_status nectorFail(nector_status status, const char* message) {
    nectorCoreSetError(status, message);
    return status;
}

inline bool nectorFinite(float value) {
    return std::isfinite(value) != 0;
}

inline bool nectorValidRect(nector_rect rect) {
    return nectorFinite(rect.x) && nectorFinite(rect.y) && nectorFinite(rect.width) &&
           nectorFinite(rect.height) && rect.width >= 0.0f && rect.height >= 0.0f;
}

#define nector_api_try try
#define nector_api_catch \
    catch (const std::bad_alloc&) { return nectorFail(nector_out_of_memory, "out of memory"); } \
    catch (const std::exception& exception) { return nectorFail(nector_backend_error, exception.what()); } \
    catch (...) { return nectorFail(nector_backend_error, "unknown native error"); }

#endif
