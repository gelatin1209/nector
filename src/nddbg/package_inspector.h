#ifndef nector_nddbg_package_inspector_h
#define nector_nddbg_package_inspector_h

#include "record_decoder.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nddbg {

enum class InspectionResult {
    valid,
    invalid,
    io_error
};

struct PackageInspection {
    std::vector<uint8_t> bytes;
    nector_cdsl_detail::PackageLayout layout;
    nector_cdsl_detail::PackageRecords records;
    std::vector<DecodedRecord> decoded_records;
    std::string error;
    bool data_decoded = false;
    bool text_decoded = false;
};

InspectionResult inspectPackage(const char* path, PackageInspection& inspection);

}
#endif
