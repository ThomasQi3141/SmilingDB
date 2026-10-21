#pragma once
#include <string>

namespace SmilingDB {

struct Entry {
	std::string value;
	// true = tombstone entry
	bool deleted = false;
};

} // namespace SmilingDB
