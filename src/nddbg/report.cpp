#include "report.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace nddbg {
namespace {

std::string crc(uint32_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << std::setfill('0') << std::setw(8) << value;
    return output.str();
}

std::string address(size_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << std::setfill('0') << std::setw(8) << value;
    return output.str();
}

std::string quoted(const std::string& value) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    output.put('"');
    for (const unsigned char byte : value) {
        switch (byte) {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (byte < 32 || byte == 127) {
                    output << "\\x" << std::setw(2) << static_cast<unsigned>(byte);
                } else {
                    output.put(static_cast<char>(byte));
                }
                break;
        }
    }
    output.put('"');
    return output.str();
}

const char* yesNo(bool value) {
    return value ? "yes" : "no";
}

void writeRange(std::ostream& output, const char* name, size_t start, size_t end,
                const std::string& details = std::string()) {
    output << "  " << name << ": " << address(start) << '-' << address(end)
           << " size=" << (end - start);
    if (!details.empty()) output << ' ' << details;
    output << '\n';
}

bool colorErrors(std::ostream& output) {
#ifdef _WIN32
    DWORD standard_handle = 0;
    if (&output == &std::cout) standard_handle = STD_OUTPUT_HANDLE;
    else if (&output == &std::cerr || &output == &std::clog) standard_handle = STD_ERROR_HANDLE;
    else return false;
    const HANDLE handle = GetStdHandle(standard_handle);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    if (GetConsoleMode(handle, &mode) == 0) return false;
    constexpr DWORD virtual_terminal_processing = 0x0004;
    if ((mode & virtual_terminal_processing) != 0) return true;
    return SetConsoleMode(handle, mode | virtual_terminal_processing) != 0;
#else
    int descriptor = -1;
    if (&output == &std::cout) descriptor = STDOUT_FILENO;
    else if (&output == &std::cerr || &output == &std::clog) descriptor = STDERR_FILENO;
    else return false;
    return ::isatty(descriptor) != 0;
#endif
}

}

void writeError(std::ostream& output, const std::string& message) {
    if (colorErrors(output)) {
        output << "\x1b[31merror:\x1b[37m " << message << "\x1b[0m\n";
    } else {
        output << "error: " << message << '\n';
    }
}

void writeSummary(std::ostream& output, const PackageInspection& inspection, bool valid) {
    const auto& layout = inspection.layout;
    output << "header:\n"
           << "  magic: " << (layout.magic_valid ? "NECTOR" : "invalid") << '\n'
           << "  version: " << layout.version << '\n'
           << "  flags: " << layout.flags << '\n'
           << "  reserved: " << layout.reserved << '\n'
           << "package_size: " << layout.package_size << '\n'
           << "sections:\n"
           << "  data:\n"
           << "    header_offset: " << layout.data_header_offset << '\n'
           << "    offset: " << layout.data_offset << '\n'
           << "    size: " << layout.data_size << '\n'
           << "    crc32: " << crc(layout.data_crc32) << '\n'
           << "    actual_crc32: " << crc(layout.actual_data_crc32) << '\n'
           << "    crc_valid: " << yesNo(layout.data_crc_valid) << '\n'
           << "  text:\n"
           << "    header_offset: " << layout.text_header_offset << '\n'
           << "    offset: " << layout.text_offset << '\n'
           << "    size: " << layout.text_size << '\n'
           << "    crc32: " << crc(layout.text_crc32) << '\n'
           << "    actual_crc32: " << crc(layout.actual_text_crc32) << '\n'
           << "    crc_valid: " << yesNo(layout.text_crc_valid) << '\n'
           << "  end:\n"
           << "    offset: " << layout.end_offset << '\n'
           << "    size: " << nector_cdsl_detail::end_section_size << '\n'
           << "    declared_package_size: " << layout.declared_package_size << '\n'
           << "record_counts:\n"
           << "  nodes: " << inspection.records.nodes.size() << '\n'
           << "  bindings: " << inspection.records.bindings.size() << '\n'
           << "  actions: " << inspection.records.actions.size() << '\n'
           << "  operations: " << inspection.records.total_operations << '\n';
    if (!valid) writeError(output, inspection.error);
    output << "valid: " << yesNo(valid) << '\n';
}

void writeStructure(std::ostream& output, const PackageInspection& inspection) {
    output << "structure:\n";
    const auto& layout = inspection.layout;
    if (!layout.envelope_valid) {
        output << "  unavailable: package envelope was not decoded\n";
        return;
    }
    output << "  range_end: exclusive\n";
    writeRange(output, "file.header", 0, layout.data_header_offset);
    writeRange(output, "data.header", layout.data_header_offset, layout.data_offset);
    writeRange(output, "data.payload", layout.data_offset, layout.text_header_offset,
               "nodes=" + std::to_string(inspection.records.nodes.size()) +
               " bindings=" + std::to_string(inspection.records.bindings.size()));
    writeRange(output, "text.header", layout.text_header_offset, layout.text_offset);
    writeRange(output, "text.payload", layout.text_offset, layout.end_offset,
               "actions=" + std::to_string(inspection.records.actions.size()) +
               " operations=" + std::to_string(inspection.records.total_operations));
    writeRange(output, "end", layout.end_offset, layout.package_size);
    if (inspection.data_decoded && layout.data_size >= 8) {
        writeRange(output, "data.records", layout.data_offset + 8, layout.text_header_offset);
    }
    if (inspection.text_decoded && layout.text_size >= 4) {
        writeRange(output, "text.records", layout.text_offset + 4, layout.end_offset);
    }
}

void writeSymbols(std::ostream& output, const PackageInspection& inspection) {
    output << "symbols:\n";
    if (!inspection.data_decoded || !inspection.text_decoded) {
        output << "  unavailable: package records were not decoded\n";
        return;
    }
    struct Symbol {
        size_t offset = 0;
        size_t size = 0;
        size_t index = 0;
        std::string section;
        std::string kind;
        std::string name;
    };
    std::vector<Symbol> symbols;
    for (size_t index = 0; index < inspection.records.nodes.size(); ++index) {
        const auto& node = inspection.records.nodes[index];
        symbols.push_back({node.offset, nodeRecordSize(inspection.records, index), index,
                           "Data", "node", node.id});
    }
    for (size_t index = 0; index < inspection.records.actions.size(); ++index) {
        const auto& action = inspection.records.actions[index];
        symbols.push_back({action.offset, actionRecordSize(inspection.records, index), index,
                           "Text", "action", action.id});
    }
    std::sort(symbols.begin(), symbols.end(), [](const Symbol& left, const Symbol& right) {
        if (left.offset != right.offset) return left.offset < right.offset;
        if (left.kind != right.kind) return left.kind < right.kind;
        return left.index < right.index;
    });
    for (const auto& symbol : symbols) {
        output << "  offset=" << address(symbol.offset) << " size=" << symbol.size
               << " section=" << symbol.section << " kind=" << symbol.kind
               << " index=" << symbol.index << " name=" << quoted(symbol.name) << '\n';
    }
}

void writeRecords(std::ostream& output, const PackageInspection& inspection) {
    output << "records:\n";
    if (!inspection.data_decoded) {
        output << "  unavailable: Data Section was not decoded\n";
        return;
    }
    for (const auto& record : inspection.decoded_records) {
        output << "  offset=" << record.offset << " size=" << record.size << ' ' << record.fields << '\n';
    }
}

}
