#include "test_framework.hpp"

#include "fontmaker/charset.hpp"
#include "fontmaker/error.hpp"
#include "fontmaker/io.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace fontmaker;

namespace {

std::filesystem::path temporaryPath(const char* suffix)
{
    static int counter = 0;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();

    // Keep test files inside the build tree. Some CI sandboxes expose
    // %TEMP% read-only to child processes even when the shell can write it.
    const std::filesystem::path root = std::filesystem::current_path() / ".fontmaker-test";
    std::error_code error;
    std::filesystem::create_directories(root, error);
    return root / ("fontmaker-test-" + std::to_string(stamp) + "-"
        + std::to_string(counter++) + suffix);
}

struct TempFile {
    explicit TempFile(std::filesystem::path value) : path(std::move(value)) {}
    ~TempFile()
    {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
    std::filesystem::path path;
};

} // namespace

FM_TEST(charset_from_utf8_preserves_order)
{
    const Charset charset = Charset::fromUtf8("A\u4E2D\U0001F600");
    FM_CHECK_EQ(charset.size(), std::size_t{3});
    FM_CHECK_EQ(charset[0], U'A');
    FM_CHECK_EQ(charset[1], U'\u4E2D');
    FM_CHECK_EQ(charset[2], U'\U0001F600');
    FM_CHECK_EQ(charset.toUtf8(), std::string("A\u4E2D\U0001F600"));
}

FM_TEST(cst_reader_handles_bom_and_surrogate_units)
{
    std::vector<std::uint8_t> bytes = {
        0xFF, 0xFE,             // UTF-16LE BOM
        'A', 0x00,
        0x3D, 0xD8, 0x00, 0xDE, // surrogate units, not combined by .cst
    };
    const Charset charset = Charset::fromUtf16Le(bytes);
    FM_CHECK_EQ(charset.size(), std::size_t{3});
    FM_CHECK_EQ(charset[0], U'A');
    FM_CHECK_EQ(charset[1], U'\uFFFD');
    FM_CHECK_EQ(charset[2], U'\uFFFD');
}

FM_TEST(cst_reader_replaces_unpaired_surrogates)
{
    const std::vector<std::uint8_t> bytes = {0x00, 0xD8, 'A', 0x00, 0x00, 0xDC};
    const Charset charset = Charset::fromUtf16Le(bytes);
    FM_CHECK_EQ(charset.size(), std::size_t{3});
    FM_CHECK_EQ(charset[0], U'\uFFFD');
    FM_CHECK_EQ(charset[1], U'A');
    FM_CHECK_EQ(charset[2], U'\uFFFD');
}

FM_TEST(cst_reader_rejects_odd_length)
{
    const std::vector<std::uint8_t> bytes = {'A', 0x00, 'B'};
    bool threw = false;
    try {
        static_cast<void>(Charset::fromUtf16Le(bytes));
    } catch (const CharsetError&) {
        threw = true;
    }
    FM_CHECK(threw);
}

FM_TEST(cst_file_round_trip)
{
    TempFile file(temporaryPath(".cst"));
    const std::vector<std::uint8_t> bytes = {'A', 0x00, 0x2D, 0x4E};
    writeBinaryFile(file.path, bytes);

    const Charset charset = Charset::loadCst(file.path);
    FM_CHECK_EQ(charset.size(), std::size_t{2});
    FM_CHECK_EQ(charset[0], U'A');
    FM_CHECK_EQ(charset[1], U'\u4E2D');
}

FM_TEST(utf8_file_skips_bom)
{
    TempFile file(temporaryPath(".txt"));
    const std::vector<std::uint8_t> bytes = {0xEF, 0xBB, 0xBF, 'A', 0xE4, 0xB8, 0xAD};
    writeBinaryFile(file.path, bytes);

    const Charset charset = Charset::loadUtf8File(file.path);
    FM_CHECK_EQ(charset.size(), std::size_t{2});
    FM_CHECK_EQ(charset[0], U'A');
    FM_CHECK_EQ(charset[1], U'\u4E2D');
}
