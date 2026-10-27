#pragma once

#include "memtable.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace SmilingDB {

class TableReader {
public:
	// only reads footer + index, data blocks get read when needed
	static std::expected<std::unique_ptr<TableReader>, std::string>
	Open(const std::filesystem::path &path);
	~TableReader();

	TableReader(const TableReader &) = delete;
	TableReader &operator=(const TableReader &) = delete;

	// same as MemTable::Get, error -> file is corrupt/unreadable
	std::expected<LookupResult, std::string> Get(std::string_view key,
	                                             std::string &value) const;

private:
	struct IndexEntry {
		std::string last_key;
		uint64_t offset;
		uint64_t size;
	};

	TableReader(int fd, std::filesystem::path path,
	            std::vector<IndexEntry> index)
	    : fd_(fd), path_(std::move(path)), index_(std::move(index)) {}

	static std::expected<std::vector<IndexEntry>, std::string>
	ReadIndex(int fd, const std::filesystem::path &path);

	int fd_;
	std::filesystem::path path_;
	std::vector<IndexEntry> index_;
};

} // namespace SmilingDB
