#include "memtable.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

namespace SmilingDB {
namespace {

constexpr size_t kOverhead = SkipList::kApproxNodeOverhead;

TEST(MemTableTest, PutThenGetIsFound) {
	MemTable table;
	table.Put("k", "v");
	std::string value;
	EXPECT_EQ(table.Get("k", value), LookupResult::Found);
	EXPECT_EQ(value, "v");
}

TEST(MemTableTest, GetNeverWrittenKeyIsNotFound) {
	MemTable table;
	std::string value = "untouched";
	EXPECT_EQ(table.Get("missing", value), LookupResult::NotFound);
	EXPECT_EQ(value, "untouched");
	table.Put("other", "v");
	EXPECT_EQ(table.Get("missing", value), LookupResult::NotFound);
	EXPECT_EQ(value, "untouched");
}

// a tombstone must report Deleted, not NotFound, so reads don't fall through
// to older SSTables
TEST(MemTableTest, PutRemoveGetIsDeleted) {
	MemTable table;
	table.Put("k", "v");
	table.Remove("k");
	std::string value = "untouched";
	EXPECT_EQ(table.Get("k", value), LookupResult::Deleted);
	EXPECT_EQ(value, "untouched");
}

// the key may live in an older SSTable, so Remove must still leave a tombstone
TEST(MemTableTest, RemoveNeverSeenKeyIsDeleted) {
	MemTable table;
	table.Remove("ghost");
	std::string value;
	EXPECT_EQ(table.Get("ghost", value), LookupResult::Deleted);
	EXPECT_FALSE(table.Empty());
}

TEST(MemTableTest, PutAfterRemoveIsFoundWithNewValue) {
	MemTable table;
	table.Put("k", "old");
	table.Remove("k");
	table.Put("k", "new");
	std::string value;
	EXPECT_EQ(table.Get("k", value), LookupResult::Found);
	EXPECT_EQ(value, "new");
}

TEST(MemTableTest, PutOverwritesValue) {
	MemTable table;
	table.Put("k", "first");
	table.Put("k", "second");
	std::string value;
	EXPECT_EQ(table.Get("k", value), LookupResult::Found);
	EXPECT_EQ(value, "second");
}

TEST(MemTableTest, EmptyTable) {
	MemTable table;
	EXPECT_TRUE(table.Empty());
	EXPECT_EQ(table.ApproximateMemoryUsage(), 0u);
	EXPECT_EQ(table.begin(), table.end());
}

TEST(MemTableTest, MemoryNewKeyAddsKeyValueAndOverhead) {
	MemTable table;
	table.Put("key", "value");
	EXPECT_EQ(table.ApproximateMemoryUsage(), 3 + 5 + kOverhead);
	table.Put("k2", "");
	EXPECT_EQ(table.ApproximateMemoryUsage(),
	          (3 + 5 + kOverhead) + (2 + 0 + kOverhead));
}

// overwrites must only change usage by the value-size difference, never
// re-add the key or node overhead
TEST(MemTableTest, MemoryOverwriteChangesByExactDifference) {
	MemTable table;
	table.Put("k", "aaaa");
	size_t base = table.ApproximateMemoryUsage();

	table.Put("k", "aaaaaaaaaa"); // bigger: +6
	EXPECT_EQ(table.ApproximateMemoryUsage(), base + 6);

	table.Put("k", "a"); // smaller: -9
	EXPECT_EQ(table.ApproximateMemoryUsage(), base - 3);

	table.Put("k", "a"); // same size: no change
	EXPECT_EQ(table.ApproximateMemoryUsage(), base - 3);
}

TEST(MemTableTest, MemoryRemoveExistingDropsOldValueSize) {
	MemTable table;
	table.Put("k", "hello");
	size_t before = table.ApproximateMemoryUsage();
	table.Remove("k");
	EXPECT_EQ(table.ApproximateMemoryUsage(), before - 5);
	EXPECT_EQ(table.ApproximateMemoryUsage(), 1 + kOverhead);
}

// a tombstone for an unseen key is a brand-new node: key + overhead, no value
TEST(MemTableTest, MemoryRemoveNeverSeenKeyAddsKeyAndOverhead) {
	MemTable table;
	table.Remove("ghost");
	EXPECT_EQ(table.ApproximateMemoryUsage(), 5 + kOverhead);
}

TEST(MemTableTest, IterationIsSortedAndIncludesTombstones) {
	MemTable table;
	table.Put("m", "vm");
	table.Remove("c");
	table.Put("x", "vx");
	table.Put("a", "va");
	table.Remove("x");

	std::vector<std::pair<std::string, bool>> seen;
	for (auto [key, entry] : table) {
		seen.emplace_back(key, entry.deleted);
	}
	std::vector<std::pair<std::string, bool>> expected = {
	    {"a", false}, {"c", true}, {"m", false}, {"x", true}};
	EXPECT_EQ(seen, expected);

	auto it = table.begin();
	EXPECT_EQ(it.key(), "a");
	EXPECT_EQ(it.value().value, "va");
}

} // namespace
} // namespace SmilingDB
