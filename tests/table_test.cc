#include "entry.h"
#include "file_util.h"
#include "memtable.h"
#include "table_builder.h"
#include "table_reader.h"
#include "test_util.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace SmilingDB {
namespace {

std::filesystem::path BuildTable(const TempDir &dir,
                                 const std::map<std::string, Entry> &entries,
                                 size_t block_size, int bits_per_key = 10) {
	TableBuilder builder(block_size, bits_per_key);
	for (const auto &[key, entry] : entries) {
		builder.Add(key, entry);
	}
	std::filesystem::path path = dir.path() / "test.sst";
	Check(WriteFileAtomic(path, builder.Finish()));
	return path;
}

std::string Key(int i) {
	char buf[16];
	std::snprintf(buf, sizeof(buf), "key%05d", i);
	return buf;
}

LookupResult Get(const TableReader &table, const std::string &key,
                 std::string &value) {
	return Check(table.Get(key, value));
}

TEST(TableTest, EmptyTable) {
	TempDir dir;
	auto table = Check(TableReader::Open(BuildTable(dir, {}, 4096)));
	std::string value;
	EXPECT_EQ(Get(*table, "", value), LookupResult::NotFound);
	EXPECT_EQ(Get(*table, "k", value), LookupResult::NotFound);
}

TEST(TableTest, SingleEntry) {
	TempDir dir;
	auto table =
	    Check(TableReader::Open(BuildTable(dir, {{"m", {"v"}}}, 4096)));
	std::string value;
	EXPECT_EQ(Get(*table, "m", value), LookupResult::Found);
	EXPECT_EQ(value, "v");
	EXPECT_EQ(Get(*table, "a", value), LookupResult::NotFound);
	EXPECT_EQ(Get(*table, "z", value), LookupResult::NotFound);
}

// tiny blocks -> hundreds of blocks, every key has to go through the index
TEST(TableTest, ManyBlocks) {
	TempDir dir;
	std::map<std::string, Entry> entries;
	for (int i = 0; i < 1000; ++i) {
		entries[Key(i)] = Entry{"value" + std::to_string(i)};
	}
	auto table = Check(TableReader::Open(BuildTable(dir, entries, 64)));
	std::string value;
	for (int i = 0; i < 1000; ++i) {
		ASSERT_EQ(Get(*table, Key(i), value), LookupResult::Found) << i;
		EXPECT_EQ(value, "value" + std::to_string(i));
	}
	EXPECT_EQ(Get(*table, "a", value), LookupResult::NotFound);
	EXPECT_EQ(Get(*table, Key(10) + "x", value), LookupResult::NotFound);
	EXPECT_EQ(Get(*table, "zzz", value), LookupResult::NotFound);
}

TEST(TableTest, TombstonesAreDeleted) {
	TempDir dir;
	std::map<std::string, Entry> entries = {
	    {"a", {"1"}}, {"b", {"", true}}, {"c", {"3"}}};
	auto table = Check(TableReader::Open(BuildTable(dir, entries, 4096)));
	std::string value = "untouched";
	EXPECT_EQ(Get(*table, "b", value), LookupResult::Deleted);
	EXPECT_EQ(value, "untouched");
	EXPECT_EQ(Get(*table, "c", value), LookupResult::Found);
}

TEST(TableTest, EmptyAndBinaryKeys) {
	TempDir dir;
	std::string binary("a\0b", 3);
	std::map<std::string, Entry> entries = {
	    {"", {"empty"}}, {binary, {std::string("\0\xff", 2)}}};
	auto table = Check(TableReader::Open(BuildTable(dir, entries, 4096)));
	std::string value;
	EXPECT_EQ(Get(*table, "", value), LookupResult::Found);
	EXPECT_EQ(value, "empty");
	EXPECT_EQ(Get(*table, binary, value), LookupResult::Found);
	EXPECT_EQ(value, std::string("\0\xff", 2));
	EXPECT_EQ(Get(*table, "a", value), LookupResult::NotFound);
}

TEST(TableTest, ValueBiggerThanBlock) {
	TempDir dir;
	std::string big(10'000, 'x');
	std::map<std::string, Entry> entries = {
	    {"a", {"1"}}, {"big", {big}}, {"c", {"3"}}};
	auto table = Check(TableReader::Open(BuildTable(dir, entries, 64)));
	std::string value;
	EXPECT_EQ(Get(*table, "big", value), LookupResult::Found);
	EXPECT_EQ(value, big);
	EXPECT_EQ(Get(*table, "c", value), LookupResult::Found);
}

TEST(TableTest, MissingFileIsError) {
	TempDir dir;
	EXPECT_FALSE(TableReader::Open(dir.path() / "nope.sst").has_value());
}

TEST(TableTest, TooSmallFileIsError) {
	TempDir dir;
	WriteRaw(dir.path() / "small.sst", "abc");
	EXPECT_FALSE(TableReader::Open(dir.path() / "small.sst").has_value());
}

TEST(TableTest, BadMagicIsError) {
	TempDir dir;
	std::filesystem::path path = BuildTable(dir, {{"k", {"v"}}}, 4096);
	std::string data = Check(ReadFile(path));
	data.back() ^= 0x01;
	WriteRaw(path, data);
	EXPECT_FALSE(TableReader::Open(path).has_value());
}

TEST(TableTest, TruncatedFileIsError) {
	TempDir dir;
	std::filesystem::path path = BuildTable(dir, {{"k", {"v"}}}, 4096);
	std::string data = Check(ReadFile(path));
	WriteRaw(path, data.substr(0, data.size() - 5));
	EXPECT_FALSE(TableReader::Open(path).has_value());
}

// index is fine so Open works, but reading the bad block has to fail
TEST(TableTest, CorruptBlockIsGetError) {
	TempDir dir;
	std::filesystem::path path =
	    BuildTable(dir, {{"key", {"value"}}, {"zzz", {"v"}}}, 4096);
	std::string data = Check(ReadFile(path));
	data[15] ^= 0x01;
	WriteRaw(path, data);
	auto table = Check(TableReader::Open(path));
	std::string value;
	EXPECT_FALSE(table->Get("key", value).has_value());
}

TEST(TableTest, WorksWithoutBloomFilter) {
	TempDir dir;
	std::map<std::string, Entry> entries;
	for (int i = 0; i < 500; ++i) {
		entries[Key(i)] = Entry{std::to_string(i)};
	}
	entries[Key(7)] = Entry{"", true};
	auto table = Check(TableReader::Open(BuildTable(dir, entries, 64, 0)));
	std::string value;
	for (int i = 0; i < 500; ++i) {
		ASSERT_EQ(Get(*table, Key(i), value),
		          i == 7 ? LookupResult::Deleted : LookupResult::Found)
		    << i;
	}
	EXPECT_EQ(Get(*table, Key(10) + "x", value), LookupResult::NotFound);
}

// tombstone keys have to be in the filter, otherwise Deleted turns into
// NotFound and an older table's value comes back
TEST(TableTest, TombstonesPassTheFilter) {
	TempDir dir;
	std::map<std::string, Entry> entries;
	for (int i = 0; i < 1000; ++i) {
		entries[Key(i)] = Entry{"", i % 2 == 0};
	}
	auto table = Check(TableReader::Open(BuildTable(dir, entries, 4096)));
	std::string value;
	for (int i = 0; i < 1000; i += 2) {
		ASSERT_EQ(Get(*table, Key(i), value), LookupResult::Deleted) << i;
	}
}

// corrupt the only data block: a key the filter rules out never reads it (no
// error), without a filter the same lookup hits the bad block
TEST(TableTest, BloomFilterSkipsBlockRead) {
	std::map<std::string, Entry> entries = {{"apple", {"1"}},
	                                        {"cherry", {"3"}}};
	for (int bits : {10, 0}) {
		TempDir dir;
		std::filesystem::path path = BuildTable(dir, entries, 4096, bits);
		std::string data = Check(ReadFile(path));
		data[15] ^= 0x01;
		WriteRaw(path, data);
		auto table = Check(TableReader::Open(path));
		std::string value;
		auto result = table->Get("banana", value);
		if (bits > 0) {
			ASSERT_TRUE(result.has_value());
			EXPECT_EQ(*result, LookupResult::NotFound);
		} else {
			EXPECT_FALSE(result.has_value());
		}
	}
}

TEST(TableTest, CorruptFilterBoundsIsError) {
	TempDir dir;
	std::filesystem::path path = BuildTable(dir, {{"k", {"v"}}}, 4096);
	std::string data = Check(ReadFile(path));
	// filter size is the 2nd footer field
	data[data.size() - kFooterSize + 8 + 7] = '\x7f';
	WriteRaw(path, data);
	EXPECT_FALSE(TableReader::Open(path).has_value());
}

} // namespace
} // namespace SmilingDB
