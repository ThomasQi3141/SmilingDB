#include "table_builder.h"
#include "coding.h"
#include "entry.h"
#include "log_record.h"

#include <cassert>
#include <string>
#include <string_view>
#include <utility>

namespace SmilingDB {

void TableBuilder::Add(std::string_view key, const Entry &entry) {
	assert(!has_last_key_ || key > last_key_);
	block_ += EncodeRecord(entry.deleted ? RecordType::Delete : RecordType::Put,
	                       key, entry.value);
	last_key_ = key;
	has_last_key_ = true;
	if (block_.size() >= block_size_) {
		FlushBlock();
	}
}

void TableBuilder::FlushBlock() {
	if (block_.empty()) {
		return;
	}
	// index entry: last key of the block -> (offset, size)
	std::string handle;
	PutFixed64(handle, result_.size());
	PutFixed64(handle, block_.size());
	index_ += EncodeRecord(RecordType::Put, last_key_, handle);
	result_ += block_;
	block_.clear();
}

std::string TableBuilder::Finish() {
	FlushBlock();
	uint64_t index_offset = result_.size();
	result_ += index_;
	PutFixed64(result_, index_offset);
	PutFixed64(result_, index_.size());
	PutFixed64(result_, kTableMagic);
	return std::move(result_);
}

} // namespace SmilingDB
