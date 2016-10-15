#include "catch.hpp"

#include "common/encoding.hpp"

using namespace hydra;

TEST_CASE("Utf8 can be encoded", "[Encoding] [Utf8]")
{
    uint8_t output[10];

    SECTION("Encoding in 1 byte")
    {
        const uint32_t TEST_CHAR = 10;

        auto size = Encoding::EncodeUtf8(output, TEST_CHAR) - output;
        REQUIRE(size == 1);
        REQUIRE(output[0] == TEST_CHAR);
    }

    SECTION("Encoding in 2 bytes")
    {
        const uint32_t TEST_CHAR = 0x200;
        const uint8_t ANSWER_OUTPUT[2] = { 0xC8, 0x80 };

        auto size = Encoding::EncodeUtf8(output, TEST_CHAR) - output;
        REQUIRE(size == 2);
        REQUIRE(output[0] == ANSWER_OUTPUT[0]);
        REQUIRE(output[1] == ANSWER_OUTPUT[1]);
    }

    SECTION("Encoding in 3 bytes")
    {
        const uint32_t TEST_CHAR = 0x2000;
        const uint8_t ANSWER_OUTPUT[3] = { 0xE2, 0x80, 0x80 };

        auto size = Encoding::EncodeUtf8(output, TEST_CHAR) - output;
        REQUIRE(size == 3);
        REQUIRE(output[0] == ANSWER_OUTPUT[0]);
        REQUIRE(output[1] == ANSWER_OUTPUT[1]);
        REQUIRE(output[2] == ANSWER_OUTPUT[2]);
    }

    SECTION("Encoding in 4 bytes")
    {
        const uint32_t TEST_CHAR = 0x10000;
        const uint8_t ANSWER_OUTPUT[4] = { 0xF0, 0x90, 0x80, 0x80 };

        auto size = Encoding::EncodeUtf8(output, TEST_CHAR) - output;
        REQUIRE(size == 4);
        REQUIRE(output[0] == ANSWER_OUTPUT[0]);
        REQUIRE(output[1] == ANSWER_OUTPUT[1]);
        REQUIRE(output[2] == ANSWER_OUTPUT[2]);
        REQUIRE(output[3] == ANSWER_OUTPUT[3]);
    }

    SECTION("Encoding with U")
    {
        const uint32_t TEST_CHAR = U'��';
        const uint8_t ANSWER_OUTPUT[4] = u8"��";

        auto size = Encoding::EncodeUtf8(output, TEST_CHAR) - output;
        REQUIRE(size == 3);
        REQUIRE(output[0] == ANSWER_OUTPUT[0]);
        REQUIRE(output[1] == ANSWER_OUTPUT[1]);
        REQUIRE(output[2] == ANSWER_OUTPUT[2]);
    }
}

TEST_CASE("Utf8 can be decoded", "[Encoding] [Utf8]")
{
    uint32_t output;

    SECTION("Decoding with 1 byte")
    {
        const uint8_t TEST_INPUT[1] = { 'a' };
        const uint32_t ANSWER = U'a';

        auto size = Encoding::DecodeUtf8(TEST_INPUT, output) - TEST_INPUT;
        REQUIRE(size == 1);
        REQUIRE(ANSWER == output);
    }

    SECTION("Decoding with 2 bytes")
    {
        const uint8_t TEST_INPUT[2] = { 0xC8, 0x80 };
        const uint32_t ANSWER = 0x200;

        auto size = Encoding::DecodeUtf8(TEST_INPUT, output) - TEST_INPUT;
        REQUIRE(size == 2);
        REQUIRE(ANSWER == output);
    }

    SECTION("Decoding with 3 bytes")
    {
        const uint8_t TEST_INPUT[3] = { 0xE2, 0x80, 0x80 };
        const uint32_t ANSWER = 0x2000;

        auto size = Encoding::DecodeUtf8(TEST_INPUT, output) - TEST_INPUT;
        REQUIRE(size == 3);
        REQUIRE(ANSWER == output);
    }

    SECTION("Decoding with 4 bytes")
    {
        const uint8_t TEST_INPUT[4] = { 0xF0, 0x90, 0x80, 0x80 };
        const uint32_t ANSWER = 0x10000;

        auto size = Encoding::DecodeUtf8(TEST_INPUT, output) - TEST_INPUT;
        REQUIRE(size == 4);
        REQUIRE(ANSWER == output);
    }

    SECTION("Decoding with U")
    {
        const uint8_t TEST_INPUT[] = u8"��";
        const uint32_t ANSWER = U'��';

        auto size = Encoding::DecodeUtf8(TEST_INPUT, output) - TEST_INPUT;
        REQUIRE(size == 3);
        REQUIRE(ANSWER == output);
    }
}

TEST_CASE("Utf16 can be encoded", "[Encoding] [Utf16]")
{
    uint16_t output[5];

    SECTION("Encoding in 1 short")
    {
        const uint32_t TEST_CHAR = 0x200;
        const uint16_t ANSWER_OUTPUT[1] = { 0x200 };

        auto size = Encoding::EncodeUtf16(output, TEST_CHAR) - output;
        REQUIRE(size == 1);
        REQUIRE(output[0] == ANSWER_OUTPUT[0]);
    }

    SECTION("Encoding in 2 shorts")
    {
        const uint32_t TEST_CHAR = 0x10437;
        const uint16_t ANSWER_OUTPUT[2] = { 0xD801, 0xDC37 };

        auto size = Encoding::EncodeUtf16(output, TEST_CHAR) - output;
        REQUIRE(size == 2);
        REQUIRE(output[0] == ANSWER_OUTPUT[0]);
        REQUIRE(output[1] == ANSWER_OUTPUT[1]);
    }

    SECTION("Encoding in U")
    {
        const uint32_t TEST_CHAR = U'��';
        const char16_t ANSWER_OUTPUT[] = u"��";

        auto size = Encoding::EncodeUtf16(output, TEST_CHAR) - output;
        for (int i = 0; i < size; ++i)
        {
            REQUIRE(output[i] == ANSWER_OUTPUT[i]);
        }
    }
}

TEST_CASE("Utf16 can be decoded", "[Encoding] [Utf16]")
{
    uint32_t output;

    SECTION("Decoding with 1 byte")
    {
        const uint16_t TEST_INPUT[1] = { 'a' };
        const uint32_t ANSWER = U'a';

        auto size = Encoding::DecodeUtf16(TEST_INPUT, output) - TEST_INPUT;
        REQUIRE(size == 1);
        REQUIRE(ANSWER == output);
    }

    SECTION("Decoding with 2 bytes")
    {
        const uint16_t TEST_INPUT[2] = { 0xD801, 0xDC37 };
        const uint32_t ANSWER = 0x10437;

        auto size = Encoding::DecodeUtf16(TEST_INPUT, output) - TEST_INPUT;
        REQUIRE(size == 2);
        REQUIRE(ANSWER == output);
    }

    SECTION("Encoding in U")
    {
        const char16_t TEST_INPUT[] = u"��";
        const uint32_t ANSWER = U'��';

        auto size = Encoding::DecodeUtf16(TEST_INPUT, output) - TEST_INPUT;
        REQUIRE(size == 1);
        REQUIRE(ANSWER == output);
    }
}