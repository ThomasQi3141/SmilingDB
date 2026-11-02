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
	// only reads footer + filter + index, data blocks get read when needed
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

	struct Meta {
		std::vector<IndexEntry> index;
		std::string filter;
	};

	TableReader(int fd, std::filesystem::path path, Meta meta)
	    : fd_(fd), path_(std::move(path)), index_(std::move(meta.index)),
	      filter_(std::move(meta.filter)) {}

	static std::expected<Meta, std::string>
	ReadMeta(int fd, const std::filesystem::path &path);

	int fd_;
	std::filesystem::path path_;
	std::vector<IndexEntry> index_;
	std::string filter_;
};

} // namespace SmilingDB
