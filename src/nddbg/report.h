#ifndef nector_nddbg_report_h
#define nector_nddbg_report_h

#include "package_inspector.h"

#include <iosfwd>
#include <string>

namespace nddbg {

void writeError(std::ostream& output, const std::string& message);
void writeSummary(std::ostream& output, const PackageInspection& inspection, bool valid);
void writeStructure(std::ostream& output, const PackageInspection& inspection);
void writeSymbols(std::ostream& output, const PackageInspection& inspection);
void writeRecords(std::ostream& output, const PackageInspection& inspection);

}
#endif
