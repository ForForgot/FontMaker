#include "fontmaker/io.hpp"

#include "fontmaker/error.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fontmaker {
namespace {

void writeTemporaryFile(const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw Error("cannot open '" + pathToUtf8(path) + "' for writing");
    }
    if (!bytes.empty()) {
        stream.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    stream.flush();
    stream.close();
    if (!stream) {
        throw Error("cannot write '" + pathToUtf8(path) + "'");
    }
}

std::filesystem::path makeTemporaryPath(const std::filesystem::path& target)
{
    static std::atomic<unsigned long long> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
    const auto processId = static_cast<unsigned long>(GetCurrentProcessId());
#else
    const auto processId = static_cast<unsigned long>(getpid());
#endif
    std::filesystem::path name = target.filename();
    name += std::filesystem::path(".tmp." + std::to_string(processId) + "."
        + std::to_string(stamp) + "." + std::to_string(counter++));
    return target.parent_path() / name;
}

} // namespace

std::string pathToUtf8(const std::filesystem::path& path)
{
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

std::vector<std::uint8_t> readBinaryFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw Error("cannot open '" + pathToUtf8(path) + "' for reading");
    }

    const auto end = stream.tellg();
    if (end < 0) {
        throw Error("cannot determine the size of '" + pathToUtf8(path) + "'");
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    if (!bytes.empty()
        && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        throw Error("cannot read '" + pathToUtf8(path) + "'");
    }
    return bytes;
}

std::string readTextFile(const std::filesystem::path& path)
{
    const auto bytes = readBinaryFile(path);
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void writeBinaryFile(const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    const std::filesystem::path temporary = makeTemporaryPath(path);
    try {
        writeTemporaryFile(temporary, bytes);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }

#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw Error("cannot replace '" + pathToUtf8(path) + "' (Windows error "
            + std::to_string(static_cast<unsigned long>(error)) + ")");
    }
#else
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw Error("cannot replace '" + pathToUtf8(path) + "': " + error.message());
    }
#endif
}

} // namespace fontmaker
