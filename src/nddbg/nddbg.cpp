#include "decompiler.h"
#include "hex_dump.h"
#include "package_inspector.h"
#include "report.h"

#include <exception>
#include <iostream>
#include <string>

namespace {

struct Views {
    bool decompile = false;
    bool symbols = false;
    bool structure = false;
    bool records = false;
    bool hex = false;
    bool all = false;

    bool any() const {
        return decompile || symbols || structure || records || hex || all;
    }
};

void usage(std::ostream& output) {
    output << "usage: nddbg [-d|--d] [-s|--s] [-t|--t] [-records|--records]\n"
           << "             [-hex|--hex] [-all|--all] <ui.dll-nector>\n"
           << "       nddbg -help|--help\n"
           << "  -d, --d              decompile to .nector DSL (pure DSL when used alone)\n"
           << "  -s, --s              list node and action symbols\n"
           << "  -t, --t              list structure and hexadecimal file ranges\n"
           << "  -records, --records  list decoded Data/Text records\n"
           << "  -hex, --hex          dump hexadecimal bytes and ASCII\n"
           << "  -all, --all          enable every report view\n"
           << "  -help, --help        show this help\n"
           << "The default report includes the summary and hexadecimal bytes.\n";
}

}

int main(int argc, char** argv) {
    if (argc == 2) {
        const std::string argument = argv[1];
        if (argument == "-help" || argument == "--help") {
            usage(std::cout);
            return 0;
        }
    }
    Views views;
    const char* package_path = nullptr;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const std::string option_name = argument.compare(0, 2, "--") == 0 ? argument.substr(1) : argument;
        bool option = true;
        if (option_name == "-d") views.decompile = true;
        else if (option_name == "-s") views.symbols = true;
        else if (option_name == "-t") views.structure = true;
        else if (option_name == "-records") views.records = true;
        else if (option_name == "-hex") views.hex = true;
        else if (option_name == "-all") views.all = true;
        else option = false;
        if (option) continue;
        if (argument.compare(0, 1, "-") == 0) {
            nddbg::writeError(std::cerr, "unknown option '" + argument + "'");
            usage(std::cerr);
            return 2;
        }
        if (package_path != nullptr) {
            nddbg::writeError(std::cerr, "multiple package paths were provided");
            usage(std::cerr);
            return 2;
        }
        package_path = argv[index];
    }
    if (package_path == nullptr || package_path[0] == '\0') {
        nddbg::writeError(std::cerr, "missing package path");
        usage(std::cerr);
        return 2;
    }
    const bool explicit_views = views.any();
    if (views.all) {
        views.decompile = true;
        views.symbols = true;
        views.structure = true;
        views.records = true;
        views.hex = true;
    }
    if (!explicit_views) views.hex = true;
    const bool decompile_only = views.decompile && !views.symbols && !views.structure &&
                                !views.records && !views.hex;

    try {
        nddbg::PackageInspection inspection;
        const nddbg::InspectionResult result = nddbg::inspectPackage(package_path, inspection);
        if (result == nddbg::InspectionResult::io_error) {
            nddbg::writeError(std::cerr, inspection.error);
            return 3;
        }
        const bool valid = result == nddbg::InspectionResult::valid;
        if (!decompile_only || !valid) nddbg::writeSummary(std::cout, inspection, valid);
        if (decompile_only) {
            if (valid) nddbg::writeDecompilation(std::cout, inspection.records);
            return valid ? 0 : 1;
        }
        if (views.structure) nddbg::writeStructure(std::cout, inspection);
        if (views.symbols) nddbg::writeSymbols(std::cout, inspection);
        if (views.records) nddbg::writeRecords(std::cout, inspection);
        if (views.decompile) {
            std::cout << "decompile:\n";
            if (valid) nddbg::writeDecompilation(std::cout, inspection.records);
            else std::cout << "  unavailable: package records were not decoded\n";
        }
        if (views.hex) {
            const uint8_t* data = inspection.bytes.empty() ? nullptr : inspection.bytes.data();
            nddbg::writeHexDump(std::cout, data, inspection.bytes.size());
        }
        return valid ? 0 : 1;
    } catch (const std::exception& error) {
        nddbg::writeError(std::cerr, error.what());
        return 1;
    } catch (...) {
        nddbg::writeError(std::cerr, "unknown inspection failure");
        return 1;
    }
}
