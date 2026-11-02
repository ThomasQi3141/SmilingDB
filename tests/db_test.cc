#include "db.h"
#include "file_util.h"
#include "log_record.h"
#include "test_util.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>

namespace SmilingDB {
namespace {

// flushes every few writes so tests hit the sstable path constantly
Options TinyOptions() {
	Options options;
	options.write_buffer_size = 1024;
	options.block_size = 64;
	return options;
}

std::unique_ptr<DB> OpenDB(const TempDir &dir, Options options = {}) {
	return Check(DB::Open(dir.path(), options));
}

std::optional<std::string> Get(const DB &db, std::string_view key) {
	return Check(db.Get(key));
}

int CountFiles(const TempDir &dir, std::string_view extension) {
	int count = 0;
	for (const auto &file : std::filesystem::directory_iterator(dir.path())) {
		if (file.path().extension() == extension) {
			++count;
		}
	}
	return count;
}

TEST(DBTest, PutThenGet) {
	TempDir dir;
	auto db = OpenDB(dir);
	Check(db->Put("k", "v"));
	EXPECT_EQ(Get(*db, "k"), "v");
}

TEST(DBTest, GetMissingKeyIsNullopt) {
	TempDir dir;
	auto db = OpenDB(dir);
	EXPECT_EQ(Get(*db, "missing"), std::nullopt);
	Check(db->Put("other", "v"));
	EXPECT_EQ(Get(*db, "missing"), std::nullopt);
}

TEST(DBTest, PutOverwrites) {
	TempDir dir;
	auto db = OpenDB(dir);
	Check(db->Put("k", "first"));
	Check(db->Put("k", "second"));
	EXPECT_EQ(Get(*db, "k"), "second");
}

TEST(DBTest, RemoveThenGetIsNullopt) {
	TempDir dir;
	auto db = OpenDB(dir);
	Check(db->Put("k", "v"));
	Check(db->Remove("k"));
	EXPECT_EQ(Get(*db, "k"), std::nullopt);
}

TEST(DBTest, RemoveMissingKeyIsHarmless) {
	TempDir dir;
	auto db = OpenDB(dir);
	Check(db->Remove("ghost"));
	EXPECT_EQ(Get(*db, "ghost"), std::nullopt);
	Check(db->Put("ghost", "now here"));
	EXPECT_EQ(Get(*db, "ghost"), "now here");
}

TEST(DBTest, PutAfterRemoveIsVisible) {
	TempDir dir;
	auto db = OpenDB(dir);
	Check(db->Put("k", "old"));
	Check(db->Remove("k"));
	Check(db->Put("k", "new"));
	EXPECT_EQ(Get(*db, "k"), "new");
}

TEST(DBTest, EmptyValueIsFound) {
	TempDir dir;
	auto db = OpenDB(dir);
	Check(db->Put("k", ""));
	std::optional<std::string> result = Get(*db, "k");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, "");
}

TEST(DBTest, EmptyKey) {
	TempDir dir;
	auto db = OpenDB(dir);
	EXPECT_EQ(Get(*db, ""), std::nullopt);
	Check(db->Put("", "v"));
	EXPECT_EQ(Get(*db, ""), "v");
	Check(db->Remove(""));
	EXPECT_EQ(Get(*db, ""), std::nullopt);
}

TEST(DBTest, BinaryKeysAndValues) {
	TempDir dir;
	auto db = OpenDB(dir);
	std::string key("a\0b", 3);
	std::string value("\0\xff\0", 3);
	Check(db->Put(key, value));
	EXPECT_EQ(Get(*db, key), value);
	EXPECT_EQ(Get(*db, "a"), std::nullopt);
}

TEST(DBTest, GetWorksOnConstDB) {
	TempDir dir;
	auto db = OpenDB(dir);
	Check(db->Put("k", "v"));
	const DB &ref = *db;
	EXPECT_EQ(Get(ref, "k"), "v");
}

TEST(DBTest, ManyKeys) {
	TempDir dir;
	auto db = OpenDB(dir);
	for (int i = 0; i < 10'000; ++i) {
		Check(db->Put("key" + std::to_string(i), "value" + std::to_string(i)));
	}
	for (int i = 0; i < 10'000; i += 2) {
		Check(db->Remove("key" + std::to_string(i)));
	}
	for (int i = 0; i < 10'000; ++i) {
		std::optional<std::string> result = Get(*db, "key" + std::to_string(i));
		if (i % 2 == 0) {
			EXPECT_EQ(result, std::nullopt) << i;
		} else {
			EXPECT_EQ(result, "value" + std::to_string(i)) << i;
		}
	}
}

TEST(DBTest, OpenCreatesDirectory) {
	TempDir dir;
	std::filesystem::path nested = dir.path() / "a" / "b";
	auto db = Check(DB::Open(nested));
	Check(db->Put("k", "v"));
	EXPECT_TRUE(std::filesystem::exists(nested));
}

TEST(DBTest, DataSurvivesReopen) {
	TempDir dir;
	{
		auto db = OpenDB(dir);
		Check(db->Put("a", "1"));
		Check(db->Put("b", "2"));
	}
	auto db = OpenDB(dir);
	EXPECT_EQ(Get(*db, "a"), "1");
	EXPECT_EQ(Get(*db, "b"), "2");
}

TEST(DBTest, RemoveSurvivesReopen) {
	TempDir dir;
	{
		auto db = OpenDB(dir);
		Check(db->Put("k", "v"));
		Check(db->Remove("k"));
	}
	auto db = OpenDB(dir);
	EXPECT_EQ(Get(*db, "k"), std::nullopt);
}

TEST(DBTest, OverwriteSurvivesReopen) {
	TempDir dir;
	{
		auto db = OpenDB(dir);
		Check(db->Put("k", "old"));
		Check(db->Put("k", "new"));
	}
	auto db = OpenDB(dir);
	EXPECT_EQ(Get(*db, "k"), "new");
}

TEST(DBTest, ReopenLeavesOneLog) {
	TempDir dir;
	for (int i = 0; i < 3; ++i) {
		auto db = OpenDB(dir);
		Check(db->Put("k" + std::to_string(i), "v"));
	}
	auto db = OpenDB(dir);
	EXPECT_EQ(CountFiles(dir, ".log"), 1);
	for (int i = 0; i < 3; ++i) {
		EXPECT_EQ(Get(*db, "k" + std::to_string(i)), "v");
	}
}

TEST(DBTest, FlushCreatesTables) {
	TempDir dir;
	auto db = OpenDB(dir, TinyOptions());
	for (int i = 0; i < 200; ++i) {
		Check(db->Put("key" + std::to_string(i), "value" + std::to_string(i)));
	}
	EXPECT_GT(CountFiles(dir, ".sst"), 1);
	EXPECT_EQ(CountFiles(dir, ".log"), 1);
	for (int i = 0; i < 200; ++i) {
		EXPECT_EQ(Get(*db, "key" + std::to_string(i)),
		          "value" + std::to_string(i));
	}
}

// key lives in an older table, newer tombstone has to hide it
TEST(DBTest, DeleteOfFlushedKeyStaysDeleted) {
	TempDir dir;
	auto db = OpenDB(dir, TinyOptions());
	Check(db->Put("k", "v"));
	for (int i = 0; i < 50; ++i) {
		Check(db->Put("filler" + std::to_string(i), "x"));
	}
	Check(db->Remove("k"));
	for (int i = 50; i < 100; ++i) {
		Check(db->Put("filler" + std::to_string(i), "x"));
	}
	EXPECT_EQ(Get(*db, "k"), std::nullopt);
	db.reset();
	db = OpenDB(dir, TinyOptions());
	EXPECT_EQ(Get(*db, "k"), std::nullopt);
}

TEST(DBTest, NewerTableWins) {
	TempDir dir;
	auto db = OpenDB(dir, TinyOptions());
	Check(db->Put("k", "v1"));
	for (int i = 0; i < 50; ++i) {
		Check(db->Put("filler" + std::to_string(i), "x"));
	}
	Check(db->Put("k", "v2"));
	for (int i = 50; i < 100; ++i) {
		Check(db->Put("filler" + std::to_string(i), "x"));
	}
	EXPECT_EQ(Get(*db, "k"), "v2");
	db.reset();
	db = OpenDB(dir, TinyOptions());
	EXPECT_EQ(Get(*db, "k"), "v2");
}

TEST(DBTest, ReopenAfterManyFlushes) {
	TempDir dir;
	{
		auto db = OpenDB(dir, TinyOptions());
		for (int i = 0; i < 300; ++i) {
			Check(db->Put("key" + std::to_string(i), std::to_string(i)));
		}
	}
	auto db = OpenDB(dir, TinyOptions());
	for (int i = 0; i < 300; ++i) {
		EXPECT_EQ(Get(*db, "key" + std::to_string(i)), std::to_string(i)) << i;
	}
}

// crash mid write -> half a record at the end of the log
TEST(DBTest, TornLogTailIsRecovered) {
	TempDir dir;
	{
		auto db = OpenDB(dir);
		Check(db->Put("a", "1"));
		Check(db->Put("b", "2"));
	}
	std::filesystem::path log;
	for (const auto &file : std::filesystem::directory_iterator(dir.path())) {
		if (file.path().extension() == ".log") {
			log = file.path();
		}
	}
	ASSERT_FALSE(log.empty());
	std::string torn = EncodeRecord(RecordType::Put, "c", "3");
	WriteRaw(log, torn.substr(0, torn.size() - 1), true);

	{
		auto db = OpenDB(dir);
		EXPECT_EQ(Get(*db, "a"), "1");
		EXPECT_EQ(Get(*db, "b"), "2");
		EXPECT_EQ(Get(*db, "c"), std::nullopt);
		Check(db->Put("d", "4"));
	}
	auto db = OpenDB(dir);
	EXPECT_EQ(Get(*db, "a"), "1");
	EXPECT_EQ(Get(*db, "d"), "4");
}

TEST(DBTest, CorruptLogFailsOpen) {
	TempDir dir;
	{
		auto db = OpenDB(dir);
		Check(db->Put("key", "value"));
		Check(db->Put("key2", "value2"));
	}
	for (const auto &file : std::filesystem::directory_iterator(dir.path())) {
		if (file.path().extension() == ".log") {
			std::string data = Check(ReadFile(file.path()));
			data[15] ^= 0x01;
			WriteRaw(file.path(), data);
		}
	}
	EXPECT_FALSE(DB::Open(dir.path()).has_value());
}

TEST(DBTest, CorruptTableFailsOpen) {
	TempDir dir;
	{
		auto db = OpenDB(dir);
		Check(db->Put("k", "v"));
	}
	{
		auto db = OpenDB(dir);
	}
	ASSERT_EQ(CountFiles(dir, ".sst"), 1);
	for (const auto &file : std::filesystem::directory_iterator(dir.path())) {
		if (file.path().extension() == ".sst") {
			std::string data = Check(ReadFile(file.path()));
			data.back() ^= 0x01;
			WriteRaw(file.path(), data);
		}
	}
	EXPECT_FALSE(DB::Open(dir.path()).has_value());
}

TEST(DBTest, LeftoverTmpFileIsRemoved) {
	TempDir dir;
	WriteRaw(dir.path() / "000099.sst.tmp", "half written");
	auto db = OpenDB(dir);
	EXPECT_FALSE(std::filesystem::exists(dir.path() / "000099.sst.tmp"));
}

TEST(DBTest, SyncOption) {
	TempDir dir;
	Options options;
	options.sync = true;
	{
		auto db = OpenDB(dir, options);
		Check(db->Put("k", "v"));
	}
	auto db = OpenDB(dir, options);
	EXPECT_EQ(Get(*db, "k"), "v");
}

TEST(DBTest, WorksWithoutBloomFilter) {
	TempDir dir;
	Options options = TinyOptions();
	options.bloom_bits_per_key = 0;
	{
		auto db = OpenDB(dir, options);
		for (int i = 0; i < 200; ++i) {
			Check(db->Put("key" + std::to_string(i), std::to_string(i)));
		}
		for (int i = 0; i < 200; i += 3) {
			Check(db->Remove("key" + std::to_string(i)));
		}
	}
	auto db = OpenDB(dir, options);
	for (int i = 0; i < 200; ++i) {
		std::optional<std::string> expected;
		if (i % 3 != 0) {
			expected = std::to_string(i);
		}
		EXPECT_EQ(Get(*db, "key" + std::to_string(i)), expected) << i;
	}
}

// random ops + random reopens vs std::map
TEST(DBTest, MatchesStdMapUnderRandomOpsAndReopens) {
	TempDir dir;
	Options options = TinyOptions();
	options.write_buffer_size = 4096;
	std::mt19937 rng(12345);
	std::uniform_int_distribution<int> key_dist(0, 200);
	std::uniform_int_distribution<int> op_dist(0, 99);
	auto db = OpenDB(dir, options);
	std::map<std::string, std::string> reference;
	for (int step = 0; step < 5'000; ++step) {
		std::string key = std::to_string(key_dist(rng));
		int op = op_dist(rng);
		if (op < 40) {
			std::string value = std::to_string(step);
			Check(db->Put(key, value));
			reference[key] = value;
		} else if (op < 60) {
			Check(db->Remove(key));
			reference.erase(key);
		} else if (op < 99) {
			auto it = reference.find(key);
			std::optional<std::string> expected;
			if (it != reference.end()) {
				expected = it->second;
			}
			ASSERT_EQ(Get(*db, key), expected)
			    << "step " << step << " key " << key;
		} else {
			db.reset();
			db = OpenDB(dir, options);
		}
	}
	db.reset();
	db = OpenDB(dir, options);
	for (int i = 0; i <= 200; ++i) {
		std::string key = std::to_string(i);
		auto it = reference.find(key);
		std::optional<std::string> expected;
		if (it != reference.end()) {
			expected = it->second;
		}
		ASSERT_EQ(Get(*db, key), expected) << "key " << key;
	}
}

} // namespace
} // namespace SmilingDB
