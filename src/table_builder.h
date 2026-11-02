#pragma once

#include "entry.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SmilingDB {

// [data block]...[data block][filter block][index block][footer]
// footer = filter offset + size, index offset + size, magic (8 each)
inline constexpr size_t kFooterSize = 40;
inline constexpr uint64_t kTableMagic = 0x534D494C44423032; // "SMILDB02"

class TableBuilder {
public:
	// bits_per_key = 0 -> no bloom filter
	TableBuilder(size_t block_size, int bits_per_key)
	    : block_size_(block_size), bits_per_key_(bits_per_key) {}

	// keys MUST be added in sorted order (no duplicates)
	void Add(std::string_view key, const Entry &entry);
	// returns the whole file
	std::string Finish();

private:
	void FlushBlock();

	size_t block_size_;
	int bits_per_key_;
	std::string result_;
	std::string block_;
	std::string index_;
	std::string last_key_;
	std::vector<std::string> keys_;
	// need this since "" is a valid key
	bool has_last_key_ = false;
};

} // namespace SmilingDB
