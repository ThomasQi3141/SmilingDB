#include "coding.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace SmilingDB {
namespace {

// raw bytes of `s` as unsigned values, so failures print readable numbers
std::vector<uint8_t> Bytes(const std::string &s) {
	return std::vector<uint8_t>(s.begin(), s.end());
}

TEST(CodingTest, Fixed32IsLittleEndian) {
	std::string dst;
	PutFixed32(dst, 0x04030201);
	EXPECT_EQ(Bytes(dst), (std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04}));
}

TEST(CodingTest, Fixed64IsLittleEndian) {
	std::string dst;
	PutFixed64(dst, 0x0807060504030201);
	EXPECT_EQ(Bytes(dst), (std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05,
	                                            0x06, 0x07, 0x08}));
}

TEST(CodingTest, PutAppendsExactSize) {
	std::string dst;
	PutFixed32(dst, 0);
	EXPECT_EQ(dst.size(), 4u);
	PutFixed64(dst, 0);
	EXPECT_EQ(dst.size(), 12u);
}

// Put must append, never overwrite what's already in dst
TEST(CodingTest, PutAppendsAfterExistingData) {
	std::string dst = "abc";
	PutFixed32(dst, 0xDDCCBBAA);
	ASSERT_EQ(dst.size(), 7u);
	EXPECT_EQ(dst.substr(0, 3), "abc");
	EXPECT_EQ(DecodeFixed32(dst.data() + 3), 0xDDCCBBAAu);
}

TEST(CodingTest, Fixed32RoundTripsEdgeValues) {
	for (uint32_t v :
	     {uint32_t{0}, uint32_t{1}, uint32_t{0x7F}, uint32_t{0x80},
	      uint32_t{0xFF}, uint32_t{0x100}, uint32_t{0x7FFFFFFF},
	      uint32_t{0x80000000}, std::numeric_limits<uint32_t>::max()}) {
		std::string dst;
		PutFixed32(dst, v);
		EXPECT_EQ(DecodeFixed32(dst.data()), v) << std::hex << v;
	}
}

TEST(CodingTest, Fixed64RoundTripsEdgeValues) {
	for (uint64_t v :
	     {uint64_t{0}, uint64_t{1}, uint64_t{0xFF}, uint64_t{0xFFFFFFFF},
	      uint64_t{0x100000000}, uint64_t{0x7FFFFFFFFFFFFFFF},
	      uint64_t{0x8000000000000000}, std::numeric_limits<uint64_t>::max()}) {
		std::string dst;
		PutFixed64(dst, v);
		EXPECT_EQ(DecodeFixed64(dst.data()), v) << std::hex << v;
	}
}

// every byte set to a value >= 0x80 is negative as a signed char; decoding
// must not sign-extend it into the higher bytes
TEST(CodingTest, DecodeHandlesHighBytes) {
	const char four[] = {'\x80', '\xff', '\x90', '\xfe'};
	EXPECT_EQ(DecodeFixed32(four), 0xFE90FF80u);
	const char eight[] = {'\xff', '\x00', '\x80', '\x00',
	                      '\x00', '\x00', '\x00', '\x00'};
	EXPECT_EQ(DecodeFixed64(eight), 0x008000FFull);
}

// every single-byte value in every position
TEST(CodingTest, EveryBytePosition) {
	for (int pos = 0; pos < 8; ++pos) {
		for (int b = 0; b < 256; ++b) {
			uint64_t v = static_cast<uint64_t>(b) << (8 * pos);
			std::string dst;
			PutFixed64(dst, v);
			ASSERT_EQ(DecodeFixed64(dst.data()), v)
			    << "pos " << pos << " b " << b;
			if (pos < 4) {
				std::string dst32;
				PutFixed32(dst32, static_cast<uint32_t>(v));
				ASSERT_EQ(DecodeFixed32(dst32.data()), static_cast<uint32_t>(v))
				    << "pos " << pos << " b " << b;
			}
		}
	}
}

// many values packed back to back, then decoded at the right offsets
TEST(CodingTest, SequentialRoundTrip) {
	std::mt19937_64 rng(12345);
	std::vector<uint32_t> v32;
	std::vector<uint64_t> v64;
	std::string dst;
	for (int i = 0; i < 1000; ++i) {
		v32.push_back(static_cast<uint32_t>(rng()));
		v64.push_back(rng());
		PutFixed32(dst, v32.back());
		PutFixed64(dst, v64.back());
	}
	ASSERT_EQ(dst.size(), 1000u * 12);
	const char *p = dst.data();
	for (int i = 0; i < 1000; ++i) {
		ASSERT_EQ(DecodeFixed32(p), v32[i]) << i;
		p += 4;
		ASSERT_EQ(DecodeFixed64(p), v64[i]) << i;
		p += 8;
	}
}

// a 64-bit value is its low 32 bits followed by its high 32 bits
TEST(CodingTest, Fixed64IsTwoFixed32Halves) {
	uint64_t v = 0x1122334455667788;
	std::string dst;
	PutFixed64(dst, v);
	EXPECT_EQ(DecodeFixed32(dst.data()), 0x55667788u);
	EXPECT_EQ(DecodeFixed32(dst.data() + 4), 0x11223344u);
}

} // namespace
} // namespace SmilingDB
