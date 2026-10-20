#pragma once

#include "entry.h"
#include "skiplist.h"

#include <cstddef>
#include <string>
#include <string_view>

enum class LookupResult { Found, Deleted, NotFound };

class MemTable {
public:
	// MemTable::Iterator = SkipList::Iterator -> dont need to redefine as
	// list_.begin()
	using Iterator = SkipList::Iterator;

	MemTable() = default;

	// remove copy ctor + assignment op to prevent accidental copying
	MemTable(const MemTable &) = delete;
	MemTable &operator=(const MemTable &) = delete;

	void Put(std::string key, std::string value);
	// only sets the deleted flag to true, doesn't actually delete
	void Remove(std::string key);
	// returns Found, Deleted, NotFound, updates value with the string if found
	LookupResult Get(std::string_view key, std::string &value) const;
	// approximates memory usage so we know when to flush
	size_t ApproximateMemoryUsage() const { return memory_usage_; }
	// returns if the memtable is empty
	bool Empty() const { return list_.Empty(); }
	// need to provide begin and end because list_ is private
	// NOTE: dont need to do this for operators because anything we can do with
	// a SkipList::Iterator, we can do with MemTable::Iterator
	Iterator begin() const { return list_.begin(); }
	Iterator end() const { return list_.end(); }

private:
	void AddEntry(std::string key, Entry entry);

	SkipList list_;
	size_t memory_usage_ = 0;
};