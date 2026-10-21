#include "coding.h"

void SmilingDB::PutFixed32(std::string &dst, uint32_t value) {
	// temp storage for data before writing
	char buffer[4];
	for (int i = 0; i < 4; ++i) {
		// get the ith byte from the value, then cast to char
		buffer[i] = static_cast<char>((value >> (8 * i)) & 0xFF);
	}
	dst.append(buffer, 4);
}

void SmilingDB::PutFixed64(std::string &dst, uint64_t value) {
	// temp storage for data before writing
	char buffer[8];
	for (int i = 0; i < 8; ++i) {
		// get the ith byte from the value, then cast to char
		buffer[i] = static_cast<char>((value >> (8 * i)) & 0xFF);
	}
	dst.append(buffer, 8);
}

// caller MUST guarantee src points to at LEAST 4 readable bytes
uint32_t SmilingDB::DecodeFixed32(const char *src) {
	uint32_t result = 0;
	for (int i = 0; i < 4; ++i) {
		// cast to uint8_t first -> char may be signed, and widening a negative
		// char sign-extends (fills the upper bits with 1s)
		result |= static_cast<uint32_t>(static_cast<uint8_t>(src[i]))
		          << (8 * i);
	}
	return result;
}

// caller MUST guarantee src points to at LEAST 8 readable bytes
uint64_t SmilingDB::DecodeFixed64(const char *src) {
	uint64_t result = 0;
	for (int i = 0; i < 8; ++i) {
		// cast to uint8_t first -> char may be signed, and widening a negative
		// char sign-extends (fills the upper bits with 1s)
		result |= static_cast<uint64_t>(static_cast<uint8_t>(src[i]))
		          << (8 * i);
	}
	return result;
}
