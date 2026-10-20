#pragma once
#include <string>

struct Entry {
	std::string value;
	// true = tombstone entry
	bool deleted = false;
};