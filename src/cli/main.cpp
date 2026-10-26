#include "fontmaker/charset.hpp"
#include "fontmaker/error.hpp"
#include "fontmaker/io.hpp"
#include "fontmaker/packing.hpp"
#include "fontmaker/version.hpp"
#include "fontmaker/writer.hpp"

#include "font_catalog.hpp"
#include "gdi_rasterizer.hpp"
#include "win32_utils.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace fontmaker;

namespace {

struct Options {
    win32::FontDescriptor font;
    int width = 16;
    int height = 16;
    int offsetX = 0;
    int offsetY = 0;
    PackOptions packing;
    OutputFormat format = OutputFormat::CSource;
    std::filesystem::path charsetPath;
    std::filesystem::path outputPath;
    std::string arrayName = "bits";
    bool listFonts = false;
    bool verbose = false;
    bool help = false;
    bool version = false;
};

void printUsage()
{
    std::cout
        << "FontMaker " << FONTMAKER_VERSION_A << " - bitmap font generator\n\n"
        << "Usage:\n"
        << "  fontmaker_cli --font <name> --charset <file> --output <file> [options]\n"
        << "  fontmaker_cli --list-fonts\n\n"
        << "Options:\n"
        << "  --font <name>       Font family name (default: Segoe UI)\n"
        << "  --size <px>         Font pixel height (default: 16)\n"
        << "  --width <px>        Glyph cell width (default: 16)\n"
        << "  --height <px>       Glyph cell height (default: 16)\n"
        << "  --offset-x <px>     Horizontal text offset (default: 0)\n"
        << "  --offset-y <px>     Vertical text offset (default: 0)\n"
        << "  --charset <file>    .cst (UTF-16LE) or .txt (UTF-8) charset\n"
        << "  --output <file>     Output file\n"
        << "  --format c|bin      Output format (default: c)\n"
        << "  --scan h|v          Scan direction (default: h)\n"
        << "  --bit-order msb|lsb Bit order (default: msb)\n"
        << "  --variable          Store a 2-byte advance width before each glyph\n"
        << "  --fixed             Fixed-width output (default)\n"
        << "  --array-name <name> C array name (default: bits)\n"
        << "  --bold              Use a bold font\n"
        << "  --italic            Use an italic font\n"
        << "  --list-fonts        List installed font families\n"
        << "  --verbose           Show progress\n"
        << "  --help              Show this help\n";
}

std::optional<int> parseInt(const std::wstring& value)
{
    if (value.empty()) {
        return std::nullopt;
    }
    wchar_t* end = nullptr;
    const long parsed = std::wcstol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != L'\0' || parsed < -32768 || parsed > 32767) {
        return std::nullopt;
    }
    return static_cast<int>(parsed);
}

std::optional<Options> parseArguments(int argc, wchar_t** argv)
{
    Options options;

    int index = 1;
    const auto nextValue = [&](const wchar_t* name, std::wstring& value) {
        if (index + 1 >= argc) {
            std::cerr << "error: missing value for " << win32::narrow(name) << '\n';
            return false;
        }
        value = argv[++index];
        return true;
    };
    const auto nextInt = [&](const wchar_t* name, int& target) {
        std::wstring value;
        if (!nextValue(name, value)) {
            return false;
        }
        const auto parsed = parseInt(value);
        if (!parsed) {
            std::cerr << "error: invalid integer for " << win32::narrow(name) << ": "
                      << win32::narrow(value) << '\n';
            return false;
        }
        target = *parsed;
        return true;
    };

    for (; index < argc; ++index) {
        const std::wstring argument = argv[index];
        std::wstring value;

        if (argument == L"--help" || argument == L"-h") {
            options.help = true;
        } else if (argument == L"--version") {
            options.version = true;
        } else if (argument == L"--list-fonts") {
            options.listFonts = true;
        } else if (argument == L"--verbose") {
            options.verbose = true;
        } else if (argument == L"--variable") {
            options.packing.variableWidth = true;
        } else if (argument == L"--fixed") {
            options.packing.variableWidth = false;
        } else if (argument == L"--bold") {
            options.font.weight = FW_BOLD;
        } else if (argument == L"--italic") {
            options.font.italic = true;
        } else if (argument == L"--font") {
            if (!nextValue(L"--font", value)) {
                return std::nullopt;
            }
            options.font.family = value;
        } else if (argument == L"--size") {
            if (!nextInt(L"--size", options.font.pixelHeight)) {
                return std::nullopt;
            }
        } else if (argument == L"--width") {
            if (!nextInt(L"--width", options.width)) {
                return std::nullopt;
            }
        } else if (argument == L"--height") {
            if (!nextInt(L"--height", options.height)) {
                return std::nullopt;
            }
        } else if (argument == L"--offset-x") {
            if (!nextInt(L"--offset-x", options.offsetX)) {
                return std::nullopt;
            }
        } else if (argument == L"--offset-y") {
            if (!nextInt(L"--offset-y", options.offsetY)) {
                return std::nullopt;
            }
        } else if (argument == L"--charset") {
            if (!nextValue(L"--charset", value)) {
                return std::nullopt;
            }
            options.charsetPath = value;
        } else if (argument == L"--output") {
            if (!nextValue(L"--output", value)) {
                return std::nullopt;
            }
            options.outputPath = value;
        } else if (argument == L"--array-name") {
            if (!nextValue(L"--array-name", value)) {
                return std::nullopt;
            }
            options.arrayName = win32::narrow(value);
        } else if (argument == L"--format") {
            if (!nextValue(L"--format", value)) {
                return std::nullopt;
            }
            if (value == L"c" || value == L"c-source") {
                options.format = OutputFormat::CSource;
            } else if (value == L"bin" || value == L"binary") {
                options.format = OutputFormat::Binary;
            } else {
                std::cerr << "error: --format must be c or bin\n";
                return std::nullopt;
            }
        } else if (argument == L"--scan") {
            if (!nextValue(L"--scan", value)) {
                return std::nullopt;
            }
            if (value == L"h" || value == L"horizontal") {
                options.packing.scan = ScanDirection::Horizontal;
            } else if (value == L"v" || value == L"vertical") {
                options.packing.scan = ScanDirection::Vertical;
            } else {
                std::cerr << "error: --scan must be h or v\n";
                return std::nullopt;
            }
        } else if (argument == L"--bit-order") {
            if (!nextValue(L"--bit-order", value)) {
                return std::nullopt;
            }
            if (value == L"msb") {
                options.packing.bitOrder = BitOrder::MsbFirst;
            } else if (value == L"lsb") {
                options.packing.bitOrder = BitOrder::LsbFirst;
            } else {
                std::cerr << "error: --bit-order must be msb or lsb\n";
                return std::nullopt;
            }
        } else {
            std::cerr << "error: unknown argument: " << win32::narrow(argument) << '\n';
            return std::nullopt;
        }
    }
    return options;
}

Charset loadCharset(const std::filesystem::path& path)
{
    if (path.extension() == L".txt") {
        return Charset::loadUtf8File(path);
    }
    return Charset::loadCst(path);
}

} // namespace

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        std::cerr << "error: cannot read command line\n";
        return 1;
    }

    const auto parsed = parseArguments(argc, argv);
    LocalFree(argv);
    if (!parsed) {
        return 2;
    }

    try {
        if (parsed->version) {
            std::cout << FONTMAKER_VERSION_A << '\n';
            return 0;
        }

        if (parsed->help) {
            printUsage();
            return 0;
        }

        if (parsed->listFonts) {
            for (const auto& family : win32::enumerateFontFamilies()) {
                std::cout << win32::narrow(family) << '\n';
            }
            return 0;
        }

        if (parsed->charsetPath.empty() || parsed->outputPath.empty()) {
            printUsage();
            return 2;
        }
        if (parsed->font.pixelHeight <= 0 || parsed->width <= 0 || parsed->height <= 0) {
            std::cerr << "error: size, width and height must be positive\n";
            return 2;
        }

        const Charset charset = loadCharset(parsed->charsetPath);
        win32::GdiGlyphRasterizer rasterizer(
            parsed->font, parsed->width, parsed->height, parsed->offsetX, parsed->offsetY);

        const auto font = encodeFont(
            rasterizer,
            charset,
            parsed->packing,
            parsed->verbose
                ? ProgressCallback([&](std::size_t done, std::size_t total) {
                      if (done == total || done % 256 == 0) {
                          std::cerr << "\r" << done << '/' << total;
                      }
                  })
                : ProgressCallback{});

        saveFont(parsed->outputPath, font, parsed->format, {parsed->arrayName});
        if (parsed->verbose) {
            std::cerr << "\r";
        }
        std::cout << "wrote " << win32::narrow(parsed->outputPath.wstring()) << ": "
                  << font.glyphs.size() << " glyphs, " << font.recordSize << " bytes/glyph\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
