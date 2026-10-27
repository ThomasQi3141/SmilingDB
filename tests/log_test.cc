#include "file_util.h"
#include "log_reader.h"
#include "log_record.h"
#include "log_writer.h"
#include "memtable.h"
#include "test_util.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace SmilingDB {
namespace {

TEST(LogTest, WriteThenReplay) {
	TempDir dir;
	std::filesystem::path path = dir.path() / "test.log";
	{
		auto log = Check(LogWriter::Open(path, false));
		Check(log->AddRecord(RecordType::Put, "a", "1"));
		Check(log->AddRecord(RecordType::Put, "b", "2"));
		Check(log->AddRecord(RecordType::Delete, "a", ""));
	}
	MemTable mem;
	Check(ReplayLog(path, mem));
	std::string value;
	EXPECT_EQ(mem.Get("a", value), LookupResult::Deleted);
	EXPECT_EQ(mem.Get("b", value), LookupResult::Found);
	EXPECT_EQ(value, "2");
	EXPECT_EQ(mem.Get("c", value), LookupResult::NotFound);
}

TEST(LogTest, OpenCreatesEmptyLog) {
	TempDir dir;
	std::filesystem::path path = dir.path() / "test.log";
	Check(LogWriter::Open(path, false));
	EXPECT_TRUE(std::filesystem::exists(path));
	MemTable mem;
	Check(ReplayLog(path, mem));
	EXPECT_TRUE(mem.Empty());
}

TEST(LogTest, ReopenAppends) {
	TempDir dir;
	std::filesystem::path path = dir.path() / "test.log";
	Check(Check(LogWriter::Open(path, false))
	          ->AddRecord(RecordType::Put, "a", "1"));
	Check(Check(LogWriter::Open(path, false))
	          ->AddRecord(RecordType::Put, "b", "2"));
	MemTable mem;
	Check(ReplayLog(path, mem));
	std::string value;
	EXPECT_EQ(mem.Get("a", value), LookupResult::Found);
	EXPECT_EQ(mem.Get("b", value), LookupResult::Found);
}

TEST(LogTest, SyncMode) {
	TempDir dir;
	std::filesystem::path path = dir.path() / "test.log";
	Check(Check(LogWriter::Open(path, true))
	          ->AddRecord(RecordType::Put, "k", "v"));
	MemTable mem;
	Check(ReplayLog(path, mem));
	std::string value;
	EXPECT_EQ(mem.Get("k", value), LookupResult::Found);
	EXPECT_EQ(value, "v");
}

TEST(LogTest, ReplayAppliesInOrder) {
	TempDir dir;
	std::filesystem::path path = dir.path() / "test.log";
	{
		auto log = Check(LogWriter::Open(path, false));
		Check(log->AddRecord(RecordType::Put, "k", "1"));
		Check(log->AddRecord(RecordType::Delete, "k", ""));
		Check(log->AddRecord(RecordType::Put, "k", "3"));
	}
	MemTable mem;
	Check(ReplayLog(path, mem));
	std::string value;
	EXPECT_EQ(mem.Get("k", value), LookupResult::Found);
	EXPECT_EQ(value, "3");
}

TEST(LogTest, TornLastRecordIsSkipped) {
	TempDir dir;
	std::filesystem::path path = dir.path() / "test.log";
	{
		auto log = Check(LogWriter::Open(path, false));
		Check(log->AddRecord(RecordType::Put, "a", "1"));
		Check(log->AddRecord(RecordType::Put, "b", "2"));
	}
	std::string torn = EncodeRecord(RecordType::Put, "c", "3");
	WriteRaw(path, torn.substr(0, torn.size() - 1), true);

	MemTable mem;
	Check(ReplayLog(path, mem));
	std::string value;
	EXPECT_EQ(mem.Get("a", value), LookupResult::Found);
	EXPECT_EQ(mem.Get("b", value), LookupResult::Found);
	EXPECT_EQ(mem.Get("c", value), LookupResult::NotFound);
}

TEST(LogTest, CorruptRecordIsError) {
	TempDir dir;
	std::filesystem::path path = dir.path() / "test.log";
	{
		auto log = Check(LogWriter::Open(path, false));
		Check(log->AddRecord(RecordType::Put, "key", "value"));
		Check(log->AddRecord(RecordType::Put, "key2", "value2"));
	}
	std::string data = Check(ReadFile(path));
	data[15] ^= 0x01;
	WriteRaw(path, data);
	MemTable mem;
	EXPECT_FALSE(ReplayLog(path, mem).has_value());
}

TEST(LogTest, MissingFileIsError) {
	TempDir dir;
	MemTable mem;
	EXPECT_FALSE(ReplayLog(dir.path() / "nope.log", mem).has_value());
}

} // namespace
} // namespace SmilingDB
