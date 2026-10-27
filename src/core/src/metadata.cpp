#include "fontmaker/metadata.hpp"

#include <sstream>
#include <string_view>

namespace fontmaker {
namespace {

std::string escapeJson(std::string_view value)
{
    std::string result;
    result.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                constexpr char digits[] = "0123456789ABCDEF";
                result += "\\u00";
                result.push_back(digits[(static_cast<unsigned char>(ch) >> 4) & 0x0F]);
                result.push_back(digits[static_cast<unsigned char>(ch) & 0x0F]);
            } else {
                result.push_back(ch);
            }
            break;
        }
    }
    return result;
}

std::string quote(std::string_view value)
{
    return '"' + escapeJson(value) + '"';
}

} // namespace

std::string toJson(const FontMetadata& metadata)
{
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"generator\": \"FontMaker\",\n";
    stream << "  \"version\": " << quote(metadata.generatorVersion) << ",\n";
    stream << "  \"requestedFont\": " << quote(metadata.requestedFont) << ",\n";
    stream << "  \"actualFont\": " << quote(metadata.actualFont) << ",\n";
    stream << "  \"fontPixelHeight\": " << metadata.fontPixelHeight << ",\n";
    stream << "  \"cellWidth\": " << metadata.cellWidth << ",\n";
    stream << "  \"cellHeight\": " << metadata.cellHeight << ",\n";
    stream << "  \"offsetX\": " << metadata.offsetX << ",\n";
    stream << "  \"offsetY\": " << metadata.offsetY << ",\n";
    stream << "  \"scan\": " << quote(toString(metadata.packing.scan)) << ",\n";
    stream << "  \"bitOrder\": " << quote(toString(metadata.packing.bitOrder)) << ",\n";
    stream << "  \"variableWidth\": " << (metadata.packing.variableWidth ? "true" : "false") << ",\n";
    stream << "  \"glyphCount\": " << metadata.glyphCount << ",\n";
    stream << "  \"emptyGlyphs\": " << metadata.emptyGlyphs << ",\n";
    stream << "  \"edgeGlyphs\": " << metadata.edgeGlyphs << ",\n";
    stream << "  \"recordSize\": " << metadata.recordSize << ",\n";
    stream << "  \"charsetSource\": " << quote(metadata.charsetSource) << ",\n";
    stream << "  \"outputFormat\": " << quote(metadata.outputFormat) << ",\n";
    stream << "  \"payloadSha256\": " << quote(metadata.payloadSha256) << "\n";
    stream << "}\n";
    return stream.str();
}

} // namespace fontmaker
