#include "table_reader.h"
#include "coding.h"
#include "file_util.h"
#include "log_record.h"
#include "table_builder.h"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace SmilingDB {

std::expected<std::unique_ptr<TableReader>, std::string>
TableReader::Open(const std::filesystem::path &path) {
	int fd = ::open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		return std::unexpected(ErrnoMessage("open " + path.string()));
	}
	auto index = ReadIndex(fd, path);
	if (!index) {
		::close(fd);
		return std::unexpected(index.error());
	}
	return std::unique_ptr<TableReader>(
	    new TableReader(fd, path, std::move(*index)));
}

TableReader::~TableReader() { ::close(fd_); }

std::expected<std::vector<TableReader::IndexEntry>, std::string>
TableReader::ReadIndex(int fd, const std::filesystem::path &path) {
	auto corrupt = [&](std::string_view why) {
		return std::unexpected("corrupt table " + path.string() + ": " +
		                       std::string(why));
	};
	struct stat st;
	if (::fstat(fd, &st) != 0) {
		return std::unexpected(ErrnoMessage("stat " + path.string()));
	}
	uint64_t file_size = static_cast<uint64_t>(st.st_size);
	if (file_size < kFooterSize) {
		return corrupt("too small");
	}
	uint64_t data_end = file_size - kFooterSize;
	std::expected<std::string, std::string> footer =
	    ReadAt(fd, data_end, kFooterSize);
	if (!footer) {
		return std::unexpected(footer.error());
	}
	uint64_t index_offset = DecodeFixed64(footer->data());
	uint64_t index_size = DecodeFixed64(footer->data() + 8);
	if (DecodeFixed64(footer->data() + 16) != kTableMagic) {
		return corrupt("bad magic number");
	}
	if (index_offset > data_end || index_size > data_end - index_offset) {
		return corrupt("index out of bounds");
	}
	std::expected<std::string, std::string> raw =
	    ReadAt(fd, index_offset, index_size);
	if (!raw) {
		return std::unexpected(raw.error());
	}

	std::vector<IndexEntry> index;
	std::string_view input = *raw;
	while (!input.empty()) {
		auto record = DecodeRecord(input);
		if (!record || record->value.size() != 16) {
			return corrupt("bad index entry");
		}
		uint64_t offset = DecodeFixed64(record->value.data());
		uint64_t size = DecodeFixed64(record->value.data() + 8);
		if (offset > index_offset || size > index_offset - offset) {
			return corrupt("block out of bounds");
		}
		if (!index.empty() && record->key <= index.back().last_key) {
			return corrupt("index keys out of order");
		}
		index.push_back({std::string(record->key), offset, size});
		input.remove_prefix(record->size);
	}
	return index;
}

std::expected<LookupResult, std::string>
TableReader::Get(std::string_view key, std::string &value) const {
	// first block with last key >= key is the only one that can have it
	auto it = std::lower_bound(
	    index_.begin(), index_.end(), key,
	    [](const IndexEntry &e, std::string_view k) { return e.last_key < k; });
	if (it == index_.end()) {
		return LookupResult::NotFound;
	}
	std::expected<std::string, std::string> block =
	    ReadAt(fd_, it->offset, it->size);
	if (!block) {
		return std::unexpected(block.error());
	}
	std::string_view input = *block;
	while (!input.empty()) {
		auto record = DecodeRecord(input);
		if (!record) {
			return std::unexpected("corrupt block in " + path_.string());
		}
		if (record->key == key) {
			if (record->type == RecordType::Delete) {
				return LookupResult::Deleted;
			}
			value = record->value;
			return LookupResult::Found;
		}
		if (record->key > key) {
			break;
		}
		input.remove_prefix(record->size);
	}
	return LookupResult::NotFound;
}

} // namespace SmilingDB
