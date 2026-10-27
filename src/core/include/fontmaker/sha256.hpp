#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace fontmaker {

/// Lowercase hexadecimal SHA-256 digest.
[[nodiscard]] std::string sha256Hex(std::span<const std::uint8_t> bytes);

} // namespace fontmaker
