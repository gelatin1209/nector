#include "package_inspector.h"

#include <fstream>
#include <limits>

namespace nddbg {
namespace {

InspectionResult invalid(PackageInspection& inspection, const std::string& error) {
    inspection.error = error;
    return InspectionResult::invalid;
}

}

InspectionResult inspectPackage(const char* path, PackageInspection& inspection) {
    inspection = PackageInspection{};
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        inspection.error = "cannot open input file";
        return InspectionResult::io_error;
    }
    const std::streamoff end = stream.tellg();
    if (end < 0) {
        inspection.error = "cannot determine input file size";
        return InspectionResult::io_error;
    }
    const uint64_t file_size = static_cast<uint64_t>(end);
    if (file_size > nector_cdsl_detail::max_package_size) {
        inspection.layout.package_size = file_size > std::numeric_limits<size_t>::max()
            ? std::numeric_limits<size_t>::max()
            : static_cast<size_t>(file_size);
        return invalid(inspection, "input file exceeds 64 MiB");
    }
    inspection.bytes.resize(static_cast<size_t>(file_size));
    stream.seekg(0, std::ios::beg);
    if (!inspection.bytes.empty() &&
        !stream.read(reinterpret_cast<char*>(inspection.bytes.data()), static_cast<std::streamsize>(inspection.bytes.size()))) {
        inspection.error = "cannot read input file";
        return InspectionResult::io_error;
    }

    const uint8_t* data = inspection.bytes.empty() ? nullptr : inspection.bytes.data();
    if (!nector_cdsl_detail::readPackageLayout(data, inspection.bytes.size(), inspection.layout, inspection.error)) {
        return InspectionResult::invalid;
    }
    inspection.data_decoded = nector_cdsl_detail::readDataSection(
        data + inspection.layout.data_offset, inspection.layout.data_size, inspection.layout.data_offset,
        inspection.records, inspection.error);
    if (!inspection.data_decoded) return InspectionResult::invalid;
    inspection.text_decoded = nector_cdsl_detail::readTextSection(
        data + inspection.layout.text_offset, inspection.layout.text_size, inspection.layout.text_offset,
        inspection.records, inspection.error);
    inspection.decoded_records = decodeRecords(inspection.records);
    if (!inspection.text_decoded) return InspectionResult::invalid;
    inspection.error.clear();
    return InspectionResult::valid;
}

}
