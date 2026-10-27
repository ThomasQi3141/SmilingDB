#include "log_reader.h"
#include "file_util.h"
#include "log_record.h"
#include "memtable.h"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace SmilingDB {

std::expected<void, std::string> ReplayLog(const std::filesystem::path &path,
                                           MemTable &mem) {
	std::expected<std::string, std::string> data = ReadFile(path);
	if (!data) {
		return std::unexpected(data.error());
	}
	std::string_view input = *data;
	while (!input.empty()) {
		auto record = DecodeRecord(input);
		if (!record) {
			if (record.error() == DecodeError::Truncated) {
				break;
			}
			return std::unexpected("corrupt record in " + path.string());
		}
		if (record->type == RecordType::Put) {
			mem.Put(std::string(record->key), std::string(record->value));
		} else {
			mem.Remove(std::string(record->key));
		}
		input.remove_prefix(record->size);
	}
	return {};
}

} // namespace SmilingDB
