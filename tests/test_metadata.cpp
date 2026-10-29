#include "test_framework.hpp"

#include "fontmaker/metadata.hpp"
#include "fontmaker/sha256.hpp"

#include <cstdint>
#include <string>
#include <vector>

using namespace fontmaker;

FM_TEST(sha256_known_vectors)
{
    const std::vector<std::uint8_t> abc = {'a', 'b', 'c'};
    FM_CHECK_EQ(sha256Hex(abc),
        std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));

    const std::vector<std::uint8_t> empty;
    FM_CHECK_EQ(sha256Hex(empty),
        std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

FM_TEST(metadata_json_escapes_and_contains_payload_hash)
{
    FontMetadata metadata;
    metadata.generatorVersion = "2.0.0";
    metadata.requestedFont = "A\"B\\C";
    metadata.actualFont = "Arial";
    metadata.fontPixelHeight = 16;
    metadata.cellWidth = 16;
    metadata.cellHeight = 16;
    metadata.packing.variableWidth = true;
    metadata.glyphCount = 96;
    metadata.emptyGlyphs = 1;
    metadata.clippedGlyphs = 2;
    metadata.overhangGlyphs = 3;
    metadata.recordSize = 34;
    metadata.charsetSource = "charset/ASCII.cst";
    metadata.outputFormat = "bin";
    metadata.payloadSha256 = "abc123";

    const std::string json = toJson(metadata);
    FM_CHECK(json.find("\"requestedFont\": \"A\\\"B\\\\C\"") != std::string::npos);
    FM_CHECK(json.find("\"actualFont\": \"Arial\"") != std::string::npos);
    FM_CHECK(json.find("\"variableWidth\": true") != std::string::npos);
    FM_CHECK(json.find("\"emptyGlyphs\": 1") != std::string::npos);
    FM_CHECK(json.find("\"clippedGlyphs\": 2") != std::string::npos);
    FM_CHECK(json.find("\"overhangGlyphs\": 3") != std::string::npos);
    FM_CHECK(json.find("\"payloadSha256\": \"abc123\"") != std::string::npos);
}
