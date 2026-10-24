#include "log_record.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

namespace SmilingDB {
namespace {

constexpr size_t kHeaderSize = 13;

TEST(LogRecordTest, PutRoundTrip) {
	std::string encoded = EncodeRecord(RecordType::Put, "key", "value");
	EXPECT_EQ(encoded.size(), kHeaderSize + 3 + 5);
	auto decoded = DecodeRecord(encoded);
	ASSERT_TRUE(decoded.has_value());
	EXPECT_EQ(decoded->type, RecordType::Put);
	EXPECT_EQ(decoded->key, "key");
	EXPECT_EQ(decoded->value, "value");
	EXPECT_EQ(decoded->size, encoded.size());
}

TEST(LogRecordTest, DeleteRoundTrip) {
	std::string encoded = EncodeRecord(RecordType::Delete, "key", "");
	EXPECT_EQ(encoded.size(), kHeaderSize + 3);
	auto decoded = DecodeRecord(encoded);
	ASSERT_TRUE(decoded.has_value());
	EXPECT_EQ(decoded->type, RecordType::Delete);
	EXPECT_EQ(decoded->key, "key");
	EXPECT_EQ(decoded->value, "");
}

TEST(LogRecordTest, EmptyKeyAndValue) {
	std::string encoded = EncodeRecord(RecordType::Put, "", "");
	EXPECT_EQ(encoded.size(), kHeaderSize);
	auto decoded = DecodeRecord(encoded);
	ASSERT_TRUE(decoded.has_value());
	EXPECT_EQ(decoded->key, "");
	EXPECT_EQ(decoded->value, "");
	EXPECT_EQ(decoded->size, kHeaderSize);
}

TEST(LogRecordTest, BinaryKeyAndValue) {
	std::string key("a\0b\xff", 4);
	std::string value("\0\0\x80", 3);
	std::string encoded = EncodeRecord(RecordType::Put, key, value);
	auto decoded = DecodeRecord(encoded);
	ASSERT_TRUE(decoded.has_value());
	EXPECT_EQ(decoded->key, key);
	EXPECT_EQ(decoded->value, value);
}

TEST(LogRecordTest, LargeValue) {
	std::string value(1 << 20, 'x');
	std::string encoded = EncodeRecord(RecordType::Put, "big", value);
	auto decoded = DecodeRecord(encoded);
	ASSERT_TRUE(decoded.has_value());
	EXPECT_EQ(decoded->value, value);
}

// decoded key/value are views into the input, not copies
TEST(LogRecordTest, DecodedViewsPointIntoInput) {
	std::string encoded = EncodeRecord(RecordType::Put, "key", "value");
	auto decoded = DecodeRecord(encoded);
	ASSERT_TRUE(decoded.has_value());
	EXPECT_EQ(decoded->key.data(), encoded.data() + kHeaderSize);
	EXPECT_EQ(decoded->value.data(), encoded.data() + kHeaderSize + 3);
}

TEST(LogRecordTest, EveryTruncationIsTruncated) {
	std::string encoded = EncodeRecord(RecordType::Put, "key", "value");
	for (size_t n = 0; n < encoded.size(); ++n) {
		auto decoded = DecodeRecord(std::string_view(encoded).substr(0, n));
		ASSERT_FALSE(decoded.has_value()) << "n " << n;
		EXPECT_EQ(decoded.error(), DecodeError::Truncated) << "n " << n;
	}
}

TEST(LogRecordTest, EveryBitFlipIsDetected) {
	std::string encoded = EncodeRecord(RecordType::Put, "key", "value");
	for (size_t i = 0; i < encoded.size(); ++i) {
		for (int bit = 0; bit < 8; ++bit) {
			std::string corrupted = encoded;
			corrupted[i] = static_cast<char>(corrupted[i] ^ (1 << bit));
			EXPECT_FALSE(DecodeRecord(corrupted).has_value())
			    << "byte " << i << " bit " << bit;
		}
	}
}

// flips outside the length fields can't change framing, so the CRC must
// catch them
TEST(LogRecordTest, PayloadBitFlipIsCorrupt) {
	std::string encoded = EncodeRecord(RecordType::Put, "key", "value");
	for (size_t i : {size_t{0}, size_t{4}, kHeaderSize, encoded.size() - 1}) {
		std::string corrupted = encoded;
		corrupted[i] = static_cast<char>(corrupted[i] ^ 0x01);
		auto decoded = DecodeRecord(corrupted);
		ASSERT_FALSE(decoded.has_value()) << "byte " << i;
		EXPECT_EQ(decoded.error(), DecodeError::Corrupt) << "byte " << i;
	}
}

// a type byte that isn't Put/Delete but still has a matching CRC
TEST(LogRecordTest, UnknownTypeIsCorrupt) {
	std::string encoded =
	    EncodeRecord(static_cast<RecordType>(7), "key", "value");
	auto decoded = DecodeRecord(encoded);
	ASSERT_FALSE(decoded.has_value());
	EXPECT_EQ(decoded.error(), DecodeError::Corrupt);
}

TEST(LogRecordTest, DecodesOnlyFirstRecordOfLongerInput) {
	std::string encoded = EncodeRecord(RecordType::Put, "key", "value");
	std::string input = encoded + "trailing garbage";
	auto decoded = DecodeRecord(input);
	ASSERT_TRUE(decoded.has_value());
	EXPECT_EQ(decoded->size, encoded.size());
	EXPECT_EQ(decoded->value, "value");
}

// how recovery will read a log: decode, then skip ahead by size
TEST(LogRecordTest, SequentialRecords) {
	struct Op {
		RecordType type;
		std::string key;
		std::string value;
	};
	std::vector<Op> ops = {{RecordType::Put, "a", "1"},
	                       {RecordType::Put, "b", "22"},
	                       {RecordType::Delete, "a", ""},
	                       {RecordType::Put, "", "empty key"},
	                       {RecordType::Put, "c", ""}};
	std::string log;
	for (const Op &op : ops) {
		log += EncodeRecord(op.type, op.key, op.value);
	}

	std::string_view input = log;
	for (const Op &op : ops) {
		auto decoded = DecodeRecord(input);
		ASSERT_TRUE(decoded.has_value());
		EXPECT_EQ(decoded->type, op.type);
		EXPECT_EQ(decoded->key, op.key);
		EXPECT_EQ(decoded->value, op.value);
		input.remove_prefix(decoded->size);
	}
	EXPECT_TRUE(input.empty());
}

// a crash mid-write leaves a partial last record; everything before it must
// still decode
TEST(LogRecordTest, TornLastRecordIsTruncated) {
	std::string log = EncodeRecord(RecordType::Put, "a", "1") +
	                  EncodeRecord(RecordType::Put, "b", "2");
	std::string last = EncodeRecord(RecordType::Put, "c", "3");
	log += last.substr(0, last.size() - 1);

	std::string_view input = log;
	for (int i = 0; i < 2; ++i) {
		auto decoded = DecodeRecord(input);
		ASSERT_TRUE(decoded.has_value()) << i;
		input.remove_prefix(decoded->size);
	}
	auto torn = DecodeRecord(input);
	ASSERT_FALSE(torn.has_value());
	EXPECT_EQ(torn.error(), DecodeError::Truncated);
}

} // namespace
} // namespace SmilingDB
