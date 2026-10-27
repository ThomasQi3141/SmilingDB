#pragma once

#include "log_writer.h"
#include "memtable.h"
#include "table_reader.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SmilingDB {

struct Options {
	// flush memtable to an sstable once it gets this big
	size_t write_buffer_size = 4 * 1024 * 1024;
	size_t block_size = 4 * 1024;
	// fsync after every write, off -> survives a process crash but not power
	// loss (same default as leveldb)
	bool sync = false;
};

class DB {
public:
	static std::expected<std::unique_ptr<DB>, std::string>
	Open(const std::filesystem::path &dir, Options options = {});

	// delete copy ctor + assignment operator, dont want multiple DB writing to
	// same file
	DB(const DB &other) = delete;
	DB &operator=(const DB &other) = delete;

	std::expected<void, std::string> Put(std::string key, std::string value);
	std::expected<void, std::string> Remove(std::string key);
	std::expected<std::optional<std::string>, std::string>
	Get(std::string_view key) const;

private:
	DB(std::filesystem::path dir, Options options)
	    : dir_(std::move(dir)), options_(options) {}

	std::expected<void, std::string> MaybeFlush();
	std::expected<void, std::string> WriteTable(const MemTable &mem);
	std::expected<void, std::string> NewLog();

	std::filesystem::path dir_;
	Options options_;
	std::unique_ptr<MemTable> mem_ = std::make_unique<MemTable>();
	std::unique_ptr<LogWriter> log_;
	uint64_t log_number_ = 0;
	uint64_t next_file_number_ = 1;
	// oldest -> newest
	std::vector<std::unique_ptr<TableReader>> tables_;
};

} // namespace SmilingDB
