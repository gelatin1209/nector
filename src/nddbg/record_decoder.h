#ifndef nector_nddbg_record_decoder_h
#define nector_nddbg_record_decoder_h

#include "../dsl_compile/package_schema.h"

#include <cstddef>
#include <string>
#include <vector>

namespace nddbg {

struct DecodedRecord {
    size_t offset = 0;
    size_t size = 0;
    std::string fields;
};

size_t nodeRecordSize(const nector_cdsl_detail::PackageRecords& records, size_t node_index);
size_t actionRecordSize(const nector_cdsl_detail::PackageRecords& records, size_t action_index);

std::vector<DecodedRecord> decodeRecords(const nector_cdsl_detail::PackageRecords& records);

}
#endif
