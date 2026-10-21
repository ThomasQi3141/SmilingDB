#include "skiplist.h"

#include <gtest/gtest.h>

#include <map>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace SmilingDB {
namespace {

// shorthand for a live (non-tombstone) entry
Entry Val(std::string v) { return Entry{std::move(v), false}; }

// collect the whole list in iteration order
std::vector<std::pair<std::string, std::string>> Dump(const SkipList &list) {
	std::vector<std::pair<std::string, std::string>> out;
	for (auto it = list.begin(); it != list.end(); ++it) {
		out.emplace_back(it.key(), it.value().value);
	}
	return out;
}

TEST(SkipListTest, EmptyListFindsNothing) {
	SkipList list;
	EXPECT_TRUE(list.Empty());
	EXPECT_EQ(list.begin(), list.end());
	EXPECT_EQ(list.Search("a"), nullptr);
	EXPECT_EQ(list.Search(""), nullptr);
	EXPECT_FALSE(list.Erase("a"));
}

TEST(SkipListTest, AddThenSearchReturnsValue) {
	SkipList list;
	EXPECT_EQ(list.Add("b", Val("2")), std::nullopt);
	EXPECT_EQ(list.Add("a", Val("1")), std::nullopt);
	EXPECT_EQ(list.Add("c", Val("3")), std::nullopt);
	EXPECT_FALSE(list.Empty());
	ASSERT_NE(list.Search("a"), nullptr);
	EXPECT_EQ(list.Search("a")->value, "1");
	EXPECT_EQ(list.Search("b")->value, "2");
	EXPECT_EQ(list.Search("c")->value, "3");
	EXPECT_EQ(list.Search("d"), nullptr);
	EXPECT_EQ(list.Search("aa"), nullptr);
}

// the head sentinel also stores "", make sure a real "" key isn't confused
// with it
TEST(SkipListTest, EmptyStringKey) {
	SkipList list;
	list.Add("", Val("empty"));
	ASSERT_NE(list.Search(""), nullptr);
	EXPECT_EQ(list.Search("")->value, "empty");
	EXPECT_TRUE(list.Erase(""));
	EXPECT_EQ(list.Search(""), nullptr);
	EXPECT_FALSE(list.Erase(""));
	EXPECT_TRUE(list.Empty());
}

TEST(SkipListTest, DuplicateAddOverwritesAndReturnsOldSize) {
	SkipList list;
	EXPECT_EQ(list.Add("x", Val("abc")), std::nullopt);
	EXPECT_EQ(list.Add("x", Val("hello")), 3u);
	EXPECT_EQ(list.Search("x")->value, "hello");
	EXPECT_EQ(list.Add("x", Val("")), 5u);
	EXPECT_EQ(list.Search("x")->value, "");
	// still only one node for the key
	EXPECT_EQ(Dump(list).size(), 1u);
	EXPECT_TRUE(list.Erase("x"));
	EXPECT_EQ(list.Search("x"), nullptr);
	EXPECT_FALSE(list.Erase("x"));
}

// tombstones are just entries to the skiplist: stored, found and overwritten
// like any other value
TEST(SkipListTest, TombstoneIsStoredAndOverwritable) {
	SkipList list;
	list.Add("k", Val("live"));
	EXPECT_EQ(list.Add("k", Entry{"", true}), 4u);
	const Entry *e = list.Search("k");
	ASSERT_NE(e, nullptr);
	EXPECT_TRUE(e->deleted);
	list.Add("k", Val("back"));
	e = list.Search("k");
	ASSERT_NE(e, nullptr);
	EXPECT_FALSE(e->deleted);
	EXPECT_EQ(e->value, "back");
}

TEST(SkipListTest, IterationIsSortedByKey) {
	SkipList list;
	for (const char *k : {"m", "c", "x", "a", "q", "", "aa"}) {
		list.Add(k, Val(std::string("v") + k));
	}
	std::vector<std::pair<std::string, std::string>> expected = {
	    {"", "v"},   {"a", "va"}, {"aa", "vaa"}, {"c", "vc"},
	    {"m", "vm"}, {"q", "vq"}, {"x", "vx"}};
	EXPECT_EQ(Dump(list), expected);
	// operator* yields the same pair
	auto it = list.begin();
	auto [k, v] = *it;
	EXPECT_EQ(k, "");
	EXPECT_EQ(v.value, "v");
}

TEST(SkipListTest, EraseOnlyRemovesTarget) {
	SkipList list;
	for (char c = 'a'; c <= 'z'; ++c) {
		list.Add(std::string(1, c), Val(std::string(2, c)));
	}
	EXPECT_TRUE(list.Erase("m"));
	EXPECT_EQ(list.Search("m"), nullptr);
	for (char c = 'a'; c <= 'z'; ++c) {
		if (c != 'm') {
			const Entry *e = list.Search(std::string(1, c));
			ASSERT_NE(e, nullptr) << c;
			EXPECT_EQ(e->value, std::string(2, c));
		}
	}
	EXPECT_EQ(Dump(list).size(), 25u);
}

TEST(SkipListTest, EraseEverythingThenReuse) {
	SkipList list;
	for (int i = 0; i < 1000; ++i) {
		list.Add(std::to_string(i), Val(std::to_string(i)));
	}
	for (int i = 0; i < 1000; ++i) {
		EXPECT_TRUE(list.Erase(std::to_string(i)));
	}
	EXPECT_TRUE(list.Empty());
	for (int i = 0; i < 1000; ++i) {
		EXPECT_EQ(list.Search(std::to_string(i)), nullptr);
	}
	list.Add("again", Val("v"));
	EXPECT_EQ(list.Search("again")->value, "v");
}

TEST(SkipListTest, CopyConstructorIsDeepAndIndependent) {
	SkipList original;
	for (int i = 0; i < 500; ++i) {
		original.Add(std::to_string(i), Val("v" + std::to_string(i)));
	}
	SkipList copy(original);
	EXPECT_EQ(Dump(copy), Dump(original));
	copy.Erase("42");
	copy.Add("new", Val("n"));
	copy.Add("7", Val("changed"));
	EXPECT_EQ(original.Search("42")->value, "v42");
	EXPECT_EQ(original.Search("new"), nullptr);
	EXPECT_EQ(original.Search("7")->value, "v7");
	EXPECT_EQ(copy.Search("42"), nullptr);
	EXPECT_EQ(copy.Search("new")->value, "n");
	EXPECT_EQ(copy.Search("7")->value, "changed");
}

TEST(SkipListTest, CopyAssignment) {
	SkipList a;
	SkipList b;
	a.Add("a1", Val("x"));
	b.Add("b1", Val("y"));
	b = a;
	EXPECT_EQ(b.Search("a1")->value, "x");
	EXPECT_EQ(b.Search("b1"), nullptr);
	b.Add("b2", Val("z"));
	EXPECT_EQ(a.Search("b2"), nullptr);
}

TEST(SkipListTest, SelfCopyAssignment) {
	SkipList a;
	a.Add("k", Val("v"));
	SkipList &ref = a;
	a = ref;
	EXPECT_EQ(a.Search("k")->value, "v");
}

TEST(SkipListTest, MoveConstructor) {
	SkipList a;
	a.Add("k", Val("v"));
	SkipList b(std::move(a));
	EXPECT_EQ(b.Search("k")->value, "v");
}

TEST(SkipListTest, MoveAssignment) {
	SkipList a;
	SkipList b;
	a.Add("a1", Val("x"));
	b.Add("b1", Val("y"));
	b = std::move(a);
	EXPECT_EQ(b.Search("a1")->value, "x");
	EXPECT_EQ(b.Search("b1"), nullptr);
}

// a moved-from list gets a fresh empty head, so it must behave like a new list
TEST(SkipListTest, MovedFromListIsEmptyAndReusable) {
	SkipList a;
	for (int i = 0; i < 100; ++i) {
		a.Add(std::to_string(i), Val(std::to_string(i)));
	}
	SkipList b(std::move(a));
	EXPECT_TRUE(a.Empty());
	for (int i = 0; i < 100; ++i) {
		EXPECT_EQ(a.Search(std::to_string(i)), nullptr);
		EXPECT_FALSE(a.Erase(std::to_string(i)));
		EXPECT_NE(b.Search(std::to_string(i)), nullptr);
	}
	a.Add("fresh", Val("f"));
	EXPECT_EQ(a.Search("fresh")->value, "f");
	EXPECT_EQ(b.Search("fresh"), nullptr);
	EXPECT_TRUE(a.Erase("fresh"));
	EXPECT_EQ(a.Search("fresh"), nullptr);
}

TEST(SkipListTest, MovedFromListCanBeCopiedAndAssigned) {
	SkipList a;
	a.Add("k", Val("v"));
	SkipList b(std::move(a));
	SkipList copy(a);
	EXPECT_EQ(copy.Search("k"), nullptr);
	copy.Add("c", Val("c"));
	EXPECT_EQ(a.Search("c"), nullptr);
	a = b;
	EXPECT_EQ(a.Search("k")->value, "v");
	SkipList c;
	c.Add("x", Val("x"));
	a = std::move(c);
	EXPECT_NE(a.Search("x"), nullptr);
	EXPECT_EQ(a.Search("k"), nullptr);
}

// moving twice from the same list must leave it usable each time
TEST(SkipListTest, RepeatedMoveFrom) {
	SkipList a;
	a.Add("one", Val("1"));
	SkipList b(std::move(a));
	a.Add("two", Val("2"));
	SkipList c(std::move(a));
	EXPECT_NE(b.Search("one"), nullptr);
	EXPECT_EQ(b.Search("two"), nullptr);
	EXPECT_NE(c.Search("two"), nullptr);
	EXPECT_EQ(c.Search("one"), nullptr);
	EXPECT_EQ(a.Search("one"), nullptr);
	EXPECT_EQ(a.Search("two"), nullptr);
}

// move assignment swaps, so the source keeps the target's old contents; it
// just has to stay a valid, usable list
TEST(SkipListTest, MoveAssignedFromListIsUsable) {
	SkipList a;
	SkipList b;
	a.Add("a1", Val("x"));
	b.Add("b1", Val("y"));
	b = std::move(a);
	a.Add("new", Val("n"));
	EXPECT_NE(a.Search("new"), nullptr);
	EXPECT_EQ(b.Search("new"), nullptr);
	a.Erase("new");
	EXPECT_EQ(a.Search("new"), nullptr);
}

// the level-0 chain is a linked list of unique_ptrs; a recursive destructor
// would blow the stack here
TEST(SkipListTest, LargeListDestructsAndCopiesWithoutStackOverflow) {
	auto list = std::make_unique<SkipList>();
	for (int i = 0; i < 1'000'000; ++i) {
		list->Add(std::to_string(i), Val(""));
	}
	SkipList copy(*list);
	EXPECT_NE(copy.Search("999999"), nullptr);
	list.reset();
}

// randomized differential test against std::map as the reference model
TEST(SkipListTest, MatchesStdMapUnderRandomOps) {
	std::mt19937 rng(12345);
	std::uniform_int_distribution<int> key_dist(0, 2000);
	std::uniform_int_distribution<int> op_dist(0, 2);
	SkipList list;
	std::map<std::string, std::string> reference;
	for (int step = 0; step < 200'000; ++step) {
		std::string key = std::to_string(key_dist(rng));
		switch (op_dist(rng)) {
		case 0: {
			std::string value = std::to_string(step);
			auto it = reference.find(key);
			std::optional<size_t> expected;
			if (it != reference.end()) {
				expected = it->second.size();
			}
			ASSERT_EQ(list.Add(key, Val(value)), expected)
			    << "step " << step << " key " << key;
			reference[key] = value;
			break;
		}
		case 1:
			ASSERT_EQ(list.Erase(key), reference.erase(key) == 1)
			    << "step " << step << " key " << key;
			break;
		case 2: {
			const Entry *e = list.Search(key);
			auto it = reference.find(key);
			ASSERT_EQ(e != nullptr, it != reference.end())
			    << "step " << step << " key " << key;
			if (e != nullptr) {
				ASSERT_EQ(e->value, it->second)
				    << "step " << step << " key " << key;
			}
			break;
		}
		}
	}
	// final contents and order must match exactly
	std::vector<std::pair<std::string, std::string>> expected(reference.begin(),
	                                                          reference.end());
	EXPECT_EQ(Dump(list), expected);
}

} // namespace
} // namespace SmilingDB
