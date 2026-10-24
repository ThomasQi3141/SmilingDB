#include "log_record.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "coding.h"
#include "crc32.h"

std::string SmilingDB::EncodeRecord(RecordType type, std::string_view key,
                                    std::string_view value) {
	std::string body;
	body.push_back(static_cast<char>(static_cast<uint8_t>(type)));
	SmilingDB::PutFixed32(body, static_cast<uint32_t>(key.size()));
	SmilingDB::PutFixed32(body, static_cast<uint32_t>(value.size()));
	body.append(key);
	body.append(value);
	std::string result;
	PutFixed32(result, Crc32(body));
	result.append(body);
	return result;
}

namespace {
// crc (4) + type (1) + key_len (4) + value_len (4)
constexpr size_t kHeaderSize = 13;
} // namespace

std::expected<SmilingDB::DecodedRecord, SmilingDB::DecodeError>
SmilingDB::DecodeRecord(std::string_view input) {
	if (input.size() < kHeaderSize) {
		return std::unexpected(DecodeError::Truncated);
	}
	uint32_t stored_crc = DecodeFixed32(input.data());
	uint32_t key_len = DecodeFixed32(input.data() + 5);
	uint32_t value_len = DecodeFixed32(input.data() + 9);
	size_t total = kHeaderSize + static_cast<size_t>(key_len) + value_len;
	if (input.size() < total) {
		return std::unexpected(DecodeError::Truncated);
	}
	if (Crc32(input.substr(4, total - 4)) != stored_crc) {
		return std::unexpected(DecodeError::Corrupt);
	}
	// validate before casting, a RecordType can hold any uint8_t
	uint8_t raw_type = static_cast<uint8_t>(input[4]);
	if (raw_type != static_cast<uint8_t>(RecordType::Put) &&
	    raw_type != static_cast<uint8_t>(RecordType::Delete)) {
		return std::unexpected(DecodeError::Corrupt);
	}
	return DecodedRecord{
	    .type = static_cast<RecordType>(raw_type),
	    .key = input.substr(kHeaderSize, key_len),
	    .value = input.substr(kHeaderSize + key_len, value_len),
	    .size = total,
	};
}