#include "dsl_comp.h"
#include "core.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: nectorc <input.nector> <output.dll-nector>\n";
        return 2;
    }
    const nector_status status = nectorCdslCompileFile(argv[1], argv[2]);
    if (status != nector_ok) {
        std::cerr << "nectorc: " << nectorGetLastError() << '\n';
        return 1;
    }
    std::cout << "compiled " << argv[1] << " -> " << argv[2] << '\n';
    return 0;
}
