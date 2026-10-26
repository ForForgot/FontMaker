#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace fontmaker {

[[nodiscard]] std::string pathToUtf8(const std::filesystem::path& path);
[[nodiscard]] std::vector<std::uint8_t> readBinaryFile(const std::filesystem::path& path);
[[nodiscard]] std::string readTextFile(const std::filesystem::path& path);
void writeBinaryFile(const std::filesystem::path& path, std::span<const std::uint8_t> bytes);

} // namespace fontmaker
