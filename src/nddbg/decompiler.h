#ifndef nector_nddbg_decompiler_h
#define nector_nddbg_decompiler_h

#include "../dsl_compile/package_schema.h"

#include <iosfwd>

namespace nddbg {

void writeDecompilation(std::ostream& output, const nector_cdsl_detail::PackageRecords& records);

}
#endif
