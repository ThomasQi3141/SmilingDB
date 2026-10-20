#include "memtable.h"
#include "entry.h"
#include "skiplist.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

void MemTable::Put(std::string key, std::string value) {
	AddEntry(std::move(key), Entry{std::move(value)});
}

// only sets the deleted flag to true, doesn't actually delete
void MemTable::Remove(std::string key) {
	// stores a tombstone! -> overwrites the key if present, inserts a tombstone
	// if not
	AddEntry(std::move(key), Entry{"", true});
}

LookupResult MemTable::Get(std::string_view key, std::string &value) const {
	const Entry *candidate = list_.Search(key);
	if (candidate == nullptr) {
		return LookupResult::NotFound;
	}
	if (candidate->deleted) {
		return LookupResult::Deleted;
	}
	value = candidate->value;
	return LookupResult::Found;
}

void MemTable::AddEntry(std::string key, Entry entry) {
	size_t key_size = key.size();
	size_t value_size = entry.value.size();
	std::optional<size_t> old_size =
	    list_.Add(std::move(key), std::move(entry));
	if (old_size == std::nullopt) {
		memory_usage_ += key_size + value_size + SkipList::kApproxNodeOverhead;
	} else {
		memory_usage_ += value_size;
		memory_usage_ -= *old_size;
	}
}