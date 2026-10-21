#include "db.h"
#include "memtable.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

std::optional<std::string> DB::Get(std::string_view key) const {
	std::string value;
	LookupResult result = mem_->Get(key, value);
	switch (result) {
	case LookupResult::Found:
		return value;
	case LookupResult::Deleted:
		return std::nullopt; // tombstone: stop here
	case LookupResult::NotFound:
		return std::nullopt; // to implement later: check SSTables
	}
	std::unreachable(); // this point cant be reached, to disable compiler
	                    // warnings
}
