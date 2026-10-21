#pragma once

#include "memtable.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace SmilingDB {

class DB {
public:
	DB() = default;

	// delete copy ctor + assignment operator, dont want multiple DB writing to
	// same file
	DB(const DB &other) = delete;
	DB &operator=(const DB &other) = delete;

	void Put(std::string key, std::string value) {
		mem_->Put(std::move(key), std::move(value));
	}

	void Remove(std::string key) { mem_->Remove(std::move(key)); }

	std::optional<std::string> Get(std::string_view key) const;

private:
	std::unique_ptr<MemTable> mem_ = std::make_unique<MemTable>();
};

} // namespace SmilingDB
