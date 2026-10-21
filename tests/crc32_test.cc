#include "coding.h"
#include "crc32.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <string>
#include <string_view>

namespace SmilingDB {
namespace {

// slow bit-by-bit CRC-32 with no lookup table, used as an independent
// reference for the table-driven version
uint32_t BitwiseCrc32(std::string_view data) {
	uint32_t crc = 0xFFFFFFFF;
	for (unsigned char byte : data) {
		crc ^= byte;
		for (int k = 0; k < 8; ++k) {
			crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
		}
	}
	return crc ^ 0xFFFFFFFF;
}

std::string RandomBytes(std::mt19937 &rng, size_t n) {
	std::uniform_int_distribution<int> byte(0, 255);
	std::string out(n, '\0');
	for (char &c : out) {
		c = static_cast<char>(byte(rng));
	}
	return out;
}

// expected values match zlib's crc32() (same IEEE polynomial)
TEST(Crc32Test, KnownVectors) {
	EXPECT_EQ(Crc32(""), 0x00000000u);
	EXPECT_EQ(Crc32("a"), 0xE8B7BE43u);
	EXPECT_EQ(Crc32("abc"), 0x352441C2u);
	// the standard CRC-32 "check" value
	EXPECT_EQ(Crc32("123456789"), 0xCBF43926u);
	EXPECT_EQ(Crc32("The quick brown fox jumps over the lazy dog"),
	          0x414FA339u);
}

TEST(Crc32Test, KnownBinaryVectors) {
	EXPECT_EQ(Crc32(std::string(32, '\0')), 0x190A55ADu);
	EXPECT_EQ(Crc32(std::string(32, '\xff')), 0xFF6CAB0Bu);
	std::string ascending;
	for (int i = 0; i < 32; ++i) {
		ascending.push_back(static_cast<char>(i));
	}
	EXPECT_EQ(Crc32(ascending), 0x91267E8Au);
}

// bytes >= 0x80 are negative as a signed char; they must be treated as
// 128..255, not sign-extended
TEST(Crc32Test, HighBytesAreUnsigned) {
	EXPECT_EQ(Crc32("\xff"), 0xFF000000u);
	EXPECT_EQ(Crc32("\x80\x90\xa0"), BitwiseCrc32("\x80\x90\xa0"));
}

// a string_view with an explicit length must include embedded NULs
TEST(Crc32Test, EmbeddedNulIsHashed) {
	std::string with_nul("a\0b", 3);
	EXPECT_EQ(Crc32(with_nul), 0x15E87871u);
	EXPECT_NE(Crc32(with_nul), Crc32("a"));
}

TEST(Crc32Test, MatchesBitwiseReferenceOnRandomData) {
	std::mt19937 rng(12345);
	for (size_t len = 0; len < 300; ++len) {
		std::string data = RandomBytes(rng, len);
		ASSERT_EQ(Crc32(data), BitwiseCrc32(data)) << "len " << len;
	}
}

// the whole point of the checksum in the WAL: any single-bit corruption must
// change it
TEST(Crc32Test, DetectsEverySingleBitFlip) {
	std::mt19937 rng(7);
	std::string data = RandomBytes(rng, 64);
	uint32_t original = Crc32(data);
	for (size_t i = 0; i < data.size(); ++i) {
		for (int bit = 0; bit < 8; ++bit) {
			std::string corrupted = data;
			corrupted[i] = static_cast<char>(corrupted[i] ^ (1 << bit));
			EXPECT_NE(Crc32(corrupted), original)
			    << "byte " << i << " bit " << bit;
		}
	}
}

TEST(Crc32Test, DetectsSwappedBytesAndTruncation) {
	EXPECT_NE(Crc32("ab"), Crc32("ba"));
	EXPECT_NE(Crc32("hello"), Crc32("hell"));
	// trailing zero bytes still change the checksum
	EXPECT_NE(Crc32(std::string("x")), Crc32(std::string("x\0", 2)));
}

// appending a message's CRC (little-endian, via PutFixed32) and checksumming
// the result always gives the same CRC-32 "residue" constant
TEST(Crc32Test, AppendedChecksumGivesResidue) {
	for (std::string_view message : {"", "hello", "something else entirely"}) {
		std::string record(message);
		PutFixed32(record, Crc32(message));
		EXPECT_EQ(Crc32(record), 0x2144DF1Cu) << message;
	}
}

// how a WAL record will use it: store the CRC next to the payload, then
// decode and verify on read
TEST(Crc32Test, RoundTripsThroughFixed32Encoding) {
	std::string payload = "key=value";
	std::string record;
	PutFixed32(record, Crc32(payload));
	record += payload;

	uint32_t stored = DecodeFixed32(record.data());
	std::string_view read_payload(record.data() + 4, record.size() - 4);
	EXPECT_EQ(stored, Crc32(read_payload));

	record.back() ^= 0x01;
	read_payload = std::string_view(record.data() + 4, record.size() - 4);
	EXPECT_NE(stored, Crc32(read_payload));
}

} // namespace
} // namespace SmilingDB
