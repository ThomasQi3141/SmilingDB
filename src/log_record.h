#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace SmilingDB {

enum class RecordType : uint8_t { Put = 1, Delete = 2 };
enum class DecodeError { Truncated, Corrupt };

// key/value point into DecodeRecord's input, only valid while it's alive
struct DecodedRecord {
	RecordType type;
	std::string_view key;
	std::string_view value;
	size_t size; // total # bytes this record took
};

std::string EncodeRecord(RecordType type, std::string_view key,
                         std::string_view value);
std::expected<DecodedRecord, DecodeError> DecodeRecord(std::string_view input);
} // namespace SmilingDB
