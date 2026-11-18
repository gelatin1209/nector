#include "dsl_comp.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct ToolResult {
    int status = -1;
    std::string output;
    std::string error;
};

struct TemporaryDirectory {
    fs::path path;

    TemporaryDirectory() {
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("nector_nddbg_test_" + std::to_string(stamp));
        fs::create_directories(path);
    }
    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}

std::string quote(const fs::path& path) {
    return '"' + path.string() + '"';
}

std::string readText(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot read captured tool output");
    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    size_t position = 0;
    while ((position = text.find("\r\n", position)) != std::string::npos) text.erase(position, 1);
    return text;
}

ToolResult runTool(const fs::path& executable, const std::string& arguments, const fs::path& directory,
                   const std::string& capture_name) {
    const fs::path output_path = directory / (capture_name + ".out");
    const fs::path error_path = directory / (capture_name + ".err");
    const std::string command = quote(executable) + " " + arguments + " >" + quote(output_path) + " 2>" + quote(error_path);
#ifdef _WIN32
    const std::string shell_command = '"' + command + '"';
#else
    const std::string& shell_command = command;
#endif
    const int raw_status = std::system(shell_command.c_str());
    require(raw_status != -1, "std::system failed to start a tool");
    require(fs::exists(output_path) && fs::exists(error_path), "tool output capture failed; command: " + command);
    ToolResult result;
#ifdef _WIN32
    result.status = raw_status;
#else
    result.status = (raw_status & 0x7f) == 0 ? (raw_status >> 8) & 0xff : raw_status;
#endif
    result.output = readText(output_path);
    result.error = readText(error_path);
    return result;
}

void writeBytes(const fs::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("cannot create test package");
    if (!bytes.empty()) stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) throw std::runtime_error("cannot write complete test package");
}

uint64_t read64(const std::vector<uint8_t>& bytes, size_t offset) {
    require(offset <= bytes.size() && bytes.size() - offset >= 8, "test offset is outside package");
    uint64_t value = 0;
    for (int index = 7; index >= 0; --index) value = (value << 8) | bytes[offset + static_cast<size_t>(index)];
    return value;
}

void write16(std::vector<uint8_t>& bytes, size_t offset, uint16_t value) {
    require(offset <= bytes.size() && bytes.size() - offset >= 2, "test offset is outside package");
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void write32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    require(offset <= bytes.size() && bytes.size() - offset >= 4, "test offset is outside package");
    for (size_t index = 0; index < 4; ++index) bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8));
}

void write64(std::vector<uint8_t>& bytes, size_t offset, uint64_t value) {
    require(offset <= bytes.size() && bytes.size() - offset >= 8, "test offset is outside package");
    for (size_t index = 0; index < 8; ++index) bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8));
}

size_t dataSize(const std::vector<uint8_t>& bytes) {
    return static_cast<size_t>(read64(bytes, 32));
}

size_t textHeader(const std::vector<uint8_t>& bytes) {
    return 40 + dataSize(bytes);
}

size_t textOffset(const std::vector<uint8_t>& bytes) {
    return textHeader(bytes) + 24;
}

void repairDataCrc(std::vector<uint8_t>& bytes) {
    const size_t end = bytes.size() - 36;
    write32(bytes, end + 24, nectorCoreCrc32(bytes.data() + 40, dataSize(bytes)));
}

void repairTextCrc(std::vector<uint8_t>& bytes) {
    const size_t end = bytes.size() - 36;
    const size_t offset = textOffset(bytes);
    write32(bytes, end + 28, nectorCoreCrc32(bytes.data() + offset, end - offset));
}

}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: test_nddbg <nddbg-path> <ndvwr-path>\n";
        return 2;
    }
    try {
        TemporaryDirectory temporary;
        const fs::path nddbg = fs::absolute(argv[1]);
        const fs::path ndvwr = fs::absolute(argv[2]);
        require(fs::exists(nddbg), "nddbg executable does not exist");
        require(fs::exists(ndvwr), "ndvwr executable does not exist");

        const char source[] =
            "window \"main\" { title=\"nddbg \\u6d4b\\u8bd5\\n\\\"quote\\\"\\\\slash\\t\\u0001\";"
            " width=320; height=240; padding=8; background=#20283080;"
            " label \"status\" { text=\"before\"; visible=true; }"
            " button \"go\" { text=\"Go\"; on_click=\"run\"; } }"
            " action \"run\" { set \"status\".text=\"after\"; set \"status\".font_size=22;"
            " set \"status\".visible=false; set \"status\".foreground=#ffffff7f;"
            " emit \"done\", \"payload\", 0.30000000000000004; }";
        uint8_t* compiled = nullptr;
        size_t compiled_size = 0;
        check(nectorCdslCompileText(source, &compiled, &compiled_size));
        std::unique_ptr<uint8_t, decltype(&nectorCdslFreeBuffer)> compiled_guard(compiled, nectorCdslFreeBuffer);
        const std::vector<uint8_t> valid_bytes(compiled, compiled + compiled_size);
        const fs::path valid_path = temporary.path / "valid.dll-nector";
        writeBytes(valid_path, valid_bytes);

        ToolResult result = runTool(nddbg, quote(valid_path), temporary.path, "valid");
        require(result.status == 0, "nddbg rejected a valid package: " + result.error + result.output);
        require(result.output.find("version: 1") != std::string::npos &&
                result.output.find("crc_valid: yes") != std::string::npos &&
                result.output.find("nodes: 3") != std::string::npos &&
                result.output.find("bindings: 1") != std::string::npos &&
                result.output.find("actions: 1") != std::string::npos &&
                result.output.find("operations: 5") != std::string::npos &&
                result.output.find("valid: yes") != std::string::npos &&
                result.output.find("hex:\n") != std::string::npos &&
                result.output.find("4e 45 43 54 4f 52") != std::string::npos &&
                result.output.find("|NECTOR") != std::string::npos,
                "nddbg default output omitted the summary or hexadecimal bytes");

        result = runTool(nddbg, "--records " + quote(valid_path), temporary.path, "records");
        require(result.status == 0 && result.output.find("offset=48 size=") != std::string::npos &&
                result.output.find("node[0]") != std::string::npos &&
                result.output.find("property[0,0]") != std::string::npos &&
                result.output.find("binding[0]") != std::string::npos &&
                result.output.find("operation[0,4]") != std::string::npos &&
                result.output.find("name=\"done\"") != std::string::npos,
                "nddbg record output omitted absolute offsets or fields");

        result = runTool(nddbg, "--hex " + quote(valid_path), temporary.path, "hex");
        require(result.status == 0 && result.output.find("hex:\n") != std::string::npos &&
                result.output.find("00000000") != std::string::npos &&
                result.output.find("4e 45 43 54 4f 52") != std::string::npos &&
                result.output.find("|NECTOR") != std::string::npos,
                "nddbg hex output omitted offsets, bytes, or ASCII:\n" + result.output);

        result = runTool(nddbg, "-t " + quote(valid_path), temporary.path, "structure");
        require(result.status == 0 && result.output.find("structure:\n") != std::string::npos &&
                result.output.find("range_end: exclusive") != std::string::npos &&
                result.output.find("file.header: 0x00000000-0x00000010 size=16") != std::string::npos &&
                result.output.find("data.header: 0x00000010-0x00000028 size=24") != std::string::npos &&
                result.output.find("data.records: 0x00000030-0x") != std::string::npos &&
                result.output.find("text.payload: 0x") != std::string::npos,
                "nddbg -t omitted structure ranges or used inclusive ends:\n" + result.output);

        result = runTool(nddbg, "-s " + quote(valid_path), temporary.path, "symbols");
        const size_t node_symbol = result.output.find("kind=node index=0 name=\"main\"");
        const size_t action_symbol = result.output.find("kind=action index=0 name=\"run\"");
        require(result.status == 0 && result.output.find("offset=0x") != std::string::npos &&
                result.output.find("size=") != std::string::npos && node_symbol != std::string::npos &&
                action_symbol != std::string::npos && node_symbol < action_symbol,
                "nddbg -s omitted or misordered node/action symbols:\n" + result.output);

        const ToolResult decompiled = runTool(nddbg, "-d " + quote(valid_path), temporary.path, "decompile");
        require(decompiled.status == 0 && decompiled.error.empty() &&
                decompiled.output.find("window \"main\"") == 0 &&
                decompiled.output.find("header:\n") == std::string::npos &&
                decompiled.output.find("decompile:\n") == std::string::npos &&
                decompiled.output.find("\\u0001") != std::string::npos &&
                decompiled.output.find("0.30000000000000004") != std::string::npos,
                "nddbg -d did not emit pure decompiled DSL:\n" + decompiled.output + decompiled.error);
        uint8_t* recompiled = nullptr;
        size_t recompiled_size = 0;
        check(nectorCdslCompileText(decompiled.output.c_str(), &recompiled, &recompiled_size));
        std::unique_ptr<uint8_t, decltype(&nectorCdslFreeBuffer)> recompiled_guard(recompiled, nectorCdslFreeBuffer);
        const std::vector<uint8_t> recompiled_bytes(recompiled, recompiled + recompiled_size);
        require(recompiled_bytes == valid_bytes, "decompiled DSL did not recompile to identical package bytes");
        const fs::path recompiled_path = temporary.path / "recompiled.dll-nector";
        writeBytes(recompiled_path, recompiled_bytes);
        require(runTool(nddbg, "-t " + quote(recompiled_path), temporary.path, "recompiled_debug").status == 0,
                "nddbg rejected the decompiled/recompiled package");
        require(runTool(ndvwr, "--check " + quote(recompiled_path), temporary.path, "recompiled_viewer").status == 0,
                "ndvwr rejected the decompiled/recompiled package");

        const ToolResult short_views = runTool(nddbg, "-s -t -d " + quote(valid_path), temporary.path, "short_views");
        const size_t short_structure = short_views.output.find("structure:\n");
        const size_t short_symbols = short_views.output.find("symbols:\n");
        const size_t short_decompile = short_views.output.find("decompile:\n");
        require(short_views.status == 0 && short_structure != std::string::npos &&
                short_symbols != std::string::npos && short_decompile != std::string::npos &&
                short_structure < short_symbols && short_symbols < short_decompile &&
                short_views.output.find("hex:\n") == std::string::npos,
                "combined -d/-s/-t views are missing or misordered");
        const ToolResult double_dash_views = runTool(nddbg, "--s --t --d " + quote(valid_path), temporary.path, "double_dash_views");
        require(double_dash_views.status == 0 && double_dash_views.output == short_views.output &&
                double_dash_views.error == short_views.error,
                "--d/--s/--t differ from -d/-s/-t");
        const ToolResult double_dash_decompile = runTool(nddbg, "--d " + quote(valid_path), temporary.path, "double_dash_decompile");
        require(double_dash_decompile.status == 0 && double_dash_decompile.output == decompiled.output &&
                double_dash_decompile.error == decompiled.error,
                "--d did not produce the same pure DSL as -d");

        const ToolResult all_first = runTool(nddbg, "--all " + quote(valid_path), temporary.path, "all_first");
        const ToolResult all_second = runTool(nddbg, "--all " + quote(valid_path), temporary.path, "all_second");
        require(all_first.status == 0 && all_second.status == 0 &&
                all_first.output == all_second.output && all_first.error == all_second.error,
                "nddbg --all output is not deterministic");
        const size_t header_position = all_first.output.find("header:\n");
        const size_t structure_position = all_first.output.find("structure:\n");
        const size_t symbols_position = all_first.output.find("symbols:\n");
        const size_t records_position = all_first.output.find("records:\n");
        const size_t decompile_position = all_first.output.find("decompile:\n");
        const size_t hex_position = all_first.output.find("hex:\n");
        require(header_position != std::string::npos && structure_position != std::string::npos &&
                symbols_position != std::string::npos && records_position != std::string::npos &&
                decompile_position != std::string::npos && hex_position != std::string::npos &&
                header_position < structure_position && structure_position < symbols_position &&
                symbols_position < records_position && records_position < decompile_position &&
                decompile_position < hex_position,
                "nddbg --all sections are not in fixed order");
        const ToolResult single_dash_all = runTool(nddbg, "-all " + quote(valid_path), temporary.path, "single_dash_all");
        require(single_dash_all.status == 0 && single_dash_all.output == all_first.output &&
                single_dash_all.error == all_first.error,
                "-all differs from --all");

        result = runTool(nddbg, "--help", temporary.path, "help");
        require(result.status == 0 && result.output.find("usage: nddbg") != std::string::npos &&
                result.output.find("-d, --d") != std::string::npos &&
                result.output.find("-records, --records") != std::string::npos,
                "nddbg --help failed or omitted option aliases");
        const ToolResult single_dash_help = runTool(nddbg, "-help", temporary.path, "single_dash_help");
        require(single_dash_help.status == 0 && single_dash_help.output == result.output,
                "-help differs from --help");
        result = runTool(nddbg, "", temporary.path, "usage");
        require(result.status == 2 && result.error.find("error: missing package path\n") == 0 &&
                result.error.find("\x1b[") == std::string::npos,
                "missing-argument error was not plain error-prefixed text");
        result = runTool(nddbg, "--records --hex " + quote(valid_path), temporary.path, "mixed");
        require(result.status == 0 && result.output.find("records:\n") != std::string::npos &&
                result.output.find("hex:\n") != std::string::npos &&
                result.output.find("records:\n") < result.output.find("hex:\n"),
                "nddbg rejected or misordered combined records/hex views");
        const ToolResult single_dash_mixed = runTool(nddbg, "-records -hex " + quote(valid_path), temporary.path, "single_dash_mixed");
        require(single_dash_mixed.status == 0 && single_dash_mixed.output == result.output &&
                single_dash_mixed.error == result.error,
                "-records/-hex differ from --records/--hex");
        result = runTool(nddbg, quote(temporary.path / "missing.dll-nector"), temporary.path, "missing");
        require(result.status == 3 && result.error.find("error: ") == 0 &&
                result.error.find("nddbg:") == std::string::npos && result.error.find("\x1b[") == std::string::npos,
                "nddbg I/O error did not use plain error-prefixed text");
        result = runTool(nddbg, "--unknown", temporary.path, "unknown_option");
        require(result.status == 2 && result.error.find("error: unknown option '--unknown'\n") == 0 &&
                result.error.find("\x1b[") == std::string::npos,
                "unknown-option error did not use plain error-prefixed text");
        result = runTool(nddbg, quote(valid_path) + " " + quote(valid_path), temporary.path, "multiple_paths");
        require(result.status == 2 && result.error.find("error: multiple package paths were provided\n") == 0,
                "multiple-path error did not use the error prefix");

        result = runTool(ndvwr, "--check " + quote(valid_path), temporary.path, "viewer_valid");
        require(result.status == 0 && result.output.find("valid:") != std::string::npos,
                "ndvwr --check rejected a valid package: " + result.error);
        result = runTool(ndvwr, "--debug " + quote(valid_path), temporary.path, "viewer_legacy_debug");
        require(result.status == 2 && result.error.find("use nddbg") != std::string::npos,
                "ndvwr --debug did not direct users to nddbg");

        int case_index = 0;
        const auto expect_invalid = [&](const std::string& name, const std::vector<uint8_t>& bytes) {
            const fs::path path = temporary.path / (name + ".dll-nector");
            writeBytes(path, bytes);
            const std::string capture = "invalid_" + std::to_string(case_index++);
            const ToolResult debug = runTool(nddbg, quote(path), temporary.path, capture);
            require(debug.status == 1 && debug.output.find("valid: no") != std::string::npos &&
                    debug.output.find("error: ") != std::string::npos &&
                    debug.output.find("\x1b[") == std::string::npos,
                    "nddbg accepted invalid case or emitted non-plain error text " + name + ": " + debug.output + debug.error);
            const ToolResult viewer = runTool(ndvwr, "--check " + quote(path), temporary.path, capture + "_viewer");
            require(viewer.status == 1, "ndvwr --check accepted invalid case " + name);
        };

        std::vector<uint8_t> damaged = valid_bytes;
        damaged[0] ^= 0xff;
        expect_invalid("bad_magic", damaged);
        const fs::path bad_magic_path = temporary.path / "bad_magic.dll-nector";
        const ToolResult invalid_decompile = runTool(nddbg, "-d " + quote(bad_magic_path), temporary.path, "invalid_decompile");
        require(invalid_decompile.status == 1 && invalid_decompile.output.find("valid: no") != std::string::npos &&
                invalid_decompile.output.find("window \"main\"") == std::string::npos,
                "nddbg -d emitted DSL for an invalid package");

        damaged = valid_bytes;
        write16(damaged, 6, 2);
        expect_invalid("bad_version", damaged);

        damaged = valid_bytes;
        damaged[16] ^= 1;
        expect_invalid("bad_data_marker", damaged);

        damaged = valid_bytes;
        damaged[textHeader(damaged)] ^= 1;
        expect_invalid("bad_text_marker", damaged);

        damaged = valid_bytes;
        damaged[damaged.size() - 36] ^= 1;
        expect_invalid("bad_end_marker", damaged);

        damaged = valid_bytes;
        write64(damaged, 32, std::numeric_limits<uint64_t>::max());
        expect_invalid("bad_data_size", damaged);

        damaged = valid_bytes;
        write64(damaged, textHeader(damaged) + 16, std::numeric_limits<uint64_t>::max());
        expect_invalid("bad_text_size", damaged);

        damaged = valid_bytes;
        write64(damaged, damaged.size() - 36 + 16, static_cast<uint64_t>(damaged.size()) + 1);
        expect_invalid("bad_total_size", damaged);

        damaged = valid_bytes;
        damaged[40] ^= 0x80;
        expect_invalid("bad_data_crc", damaged);

        damaged = valid_bytes;
        damaged[textOffset(damaged)] ^= 0x80;
        expect_invalid("bad_text_crc", damaged);

        damaged = valid_bytes;
        damaged.pop_back();
        expect_invalid("truncated_end", damaged);

        damaged.assign(valid_bytes.begin(), valid_bytes.begin() + 40);
        expect_invalid("truncated_data", damaged);

        damaged.assign(valid_bytes.begin(), valid_bytes.begin() + 1);
        expect_invalid("truncated_header", damaged);

        damaged = valid_bytes;
        damaged.push_back(0);
        expect_invalid("trailing_bytes", damaged);

        damaged = valid_bytes;
        damaged[52] = 0xff;
        repairDataCrc(damaged);
        expect_invalid("invalid_node_record", damaged);

        damaged = valid_bytes;
        const size_t first_operation = textOffset(damaged) + 4 + 4 + 3 + 4;
        damaged[first_operation + 2] = 1;
        repairTextCrc(damaged);
        expect_invalid("invalid_operation_record", damaged);

        uint8_t* invalid_dimensions = nullptr;
        size_t invalid_dimensions_size = 0;
        check(nectorCdslCompileText("window \"bad_dimensions\" { width=0; }", &invalid_dimensions, &invalid_dimensions_size));
        std::unique_ptr<uint8_t, decltype(&nectorCdslFreeBuffer)> dimensions_guard(invalid_dimensions, nectorCdslFreeBuffer);
        expect_invalid("invalid_root_dimensions",
                       std::vector<uint8_t>(invalid_dimensions, invalid_dimensions + invalid_dimensions_size));

        std::cout << "test_nddbg: PASS; hex/decompile/symbol/structure views, single/double-dash aliases, plain redirected errors, byte-identical recompilation, deterministic combinations, 17 corruption cases, and ndvwr --check\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_nddbg: FAIL: " << error.what() << '\n';
        return 1;
    }
}
