#include "fontmaker/io.hpp"

#include "fontmaker/error.hpp"

#include <fstream>
#include <iterator>
#include <string>

namespace fontmaker {

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
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw Error("cannot open '" + pathToUtf8(path) + "' for writing");
    }
    if (!bytes.empty()) {
        stream.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    stream.flush();
    if (!stream) {
        throw Error("cannot write '" + pathToUtf8(path) + "'");
    }
}

} // namespace fontmaker
