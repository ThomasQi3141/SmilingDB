#pragma once

#include "entry.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace SmilingDB {

// [data block]...[data block][index block][footer]
// footer = index offset (8) + index size (8) + magic (8)
inline constexpr size_t kFooterSize = 24;
inline constexpr uint64_t kTableMagic = 0x534D494C494E4744; // "SMILINGD"

class TableBuilder {
public:
	explicit TableBuilder(size_t block_size) : block_size_(block_size) {}

	// keys MUST be added in sorted order (no duplicates)
	void Add(std::string_view key, const Entry &entry);
	// returns the whole file
	std::string Finish();

private:
	void FlushBlock();

	size_t block_size_;
	std::string result_;
	std::string block_;
	std::string index_;
	std::string last_key_;
	// need this since "" is a valid key
	bool has_last_key_ = false;
};

} // namespace SmilingDB
