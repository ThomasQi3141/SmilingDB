#include "skiplist.h"

#include <gtest/gtest.h>

#include <random>
#include <set>
#include <string>
#include <utility>

namespace {

TEST(SkipListTest, EmptyListFindsNothing) {
	SkipList list;
	EXPECT_FALSE(list.Search("a"));
	EXPECT_FALSE(list.Search(""));
	EXPECT_FALSE(list.Erase("a"));
}

TEST(SkipListTest, AddThenSearch) {
	SkipList list;
	list.Add("b");
	list.Add("a");
	list.Add("c");
	EXPECT_TRUE(list.Search("a"));
	EXPECT_TRUE(list.Search("b"));
	EXPECT_TRUE(list.Search("c"));
	EXPECT_FALSE(list.Search("d"));
	EXPECT_FALSE(list.Search("aa"));
}

// the head sentinel also stores "", make sure a real "" key isn't confused
// with it
TEST(SkipListTest, EmptyStringKey) {
	SkipList list;
	list.Add("");
	EXPECT_TRUE(list.Search(""));
	EXPECT_TRUE(list.Erase(""));
	EXPECT_FALSE(list.Search(""));
	EXPECT_FALSE(list.Erase(""));
}

TEST(SkipListTest, DuplicateAddIsNoOp) {
	SkipList list;
	list.Add("x");
	list.Add("x");
	EXPECT_TRUE(list.Erase("x"));
	EXPECT_FALSE(list.Search("x"));
	EXPECT_FALSE(list.Erase("x"));
}

TEST(SkipListTest, EraseOnlyRemovesTarget) {
	SkipList list;
	for (char c = 'a'; c <= 'z'; ++c) {
		list.Add(std::string(1, c));
	}
	EXPECT_TRUE(list.Erase("m"));
	EXPECT_FALSE(list.Search("m"));
	for (char c = 'a'; c <= 'z'; ++c) {
		if (c != 'm') {
			EXPECT_TRUE(list.Search(std::string(1, c))) << c;
		}
	}
}

TEST(SkipListTest, EraseEverythingThenReuse) {
	SkipList list;
	for (int i = 0; i < 1000; ++i) {
		list.Add(std::to_string(i));
	}
	for (int i = 0; i < 1000; ++i) {
		EXPECT_TRUE(list.Erase(std::to_string(i)));
	}
	for (int i = 0; i < 1000; ++i) {
		EXPECT_FALSE(list.Search(std::to_string(i)));
	}
	list.Add("again");
	EXPECT_TRUE(list.Search("again"));
}

TEST(SkipListTest, CopyConstructorIsDeepAndIndependent) {
	SkipList original;
	for (int i = 0; i < 500; ++i) {
		original.Add(std::to_string(i));
	}
	SkipList copy(original);
	for (int i = 0; i < 500; ++i) {
		EXPECT_TRUE(copy.Search(std::to_string(i)));
	}
	copy.Erase("42");
	copy.Add("new");
	EXPECT_TRUE(original.Search("42"));
	EXPECT_FALSE(original.Search("new"));
	EXPECT_FALSE(copy.Search("42"));
	EXPECT_TRUE(copy.Search("new"));
}

TEST(SkipListTest, CopyAssignment) {
	SkipList a;
	SkipList b;
	a.Add("a1");
	b.Add("b1");
	b = a;
	EXPECT_TRUE(b.Search("a1"));
	EXPECT_FALSE(b.Search("b1"));
	b.Add("b2");
	EXPECT_FALSE(a.Search("b2"));
}

TEST(SkipListTest, SelfCopyAssignment) {
	SkipList a;
	a.Add("k");
	SkipList &ref = a;
	a = ref;
	EXPECT_TRUE(a.Search("k"));
}

TEST(SkipListTest, MoveConstructor) {
	SkipList a;
	a.Add("k");
	SkipList b(std::move(a));
	EXPECT_TRUE(b.Search("k"));
}

TEST(SkipListTest, MoveAssignment) {
	SkipList a;
	SkipList b;
	a.Add("a1");
	b.Add("b1");
	b = std::move(a);
	EXPECT_TRUE(b.Search("a1"));
	EXPECT_FALSE(b.Search("b1"));
}

// a moved-from list gets a fresh empty head, so it must behave like a new list
TEST(SkipListTest, MovedFromListIsEmptyAndReusable) {
	SkipList a;
	for (int i = 0; i < 100; ++i) {
		a.Add(std::to_string(i));
	}
	SkipList b(std::move(a));
	for (int i = 0; i < 100; ++i) {
		EXPECT_FALSE(a.Search(std::to_string(i)));
		EXPECT_FALSE(a.Erase(std::to_string(i)));
		EXPECT_TRUE(b.Search(std::to_string(i)));
	}
	a.Add("fresh");
	EXPECT_TRUE(a.Search("fresh"));
	EXPECT_FALSE(b.Search("fresh"));
	EXPECT_TRUE(a.Erase("fresh"));
	EXPECT_FALSE(a.Search("fresh"));
}

TEST(SkipListTest, MovedFromListCanBeCopiedAndAssigned) {
	SkipList a;
	a.Add("k");
	SkipList b(std::move(a));
	SkipList copy(a);
	EXPECT_FALSE(copy.Search("k"));
	copy.Add("c");
	EXPECT_FALSE(a.Search("c"));
	a = b;
	EXPECT_TRUE(a.Search("k"));
	SkipList c;
	c.Add("x");
	a = std::move(c);
	EXPECT_TRUE(a.Search("x"));
	EXPECT_FALSE(a.Search("k"));
}

// moving twice from the same list must leave it usable each time
TEST(SkipListTest, RepeatedMoveFrom) {
	SkipList a;
	a.Add("one");
	SkipList b(std::move(a));
	a.Add("two");
	SkipList c(std::move(a));
	EXPECT_TRUE(b.Search("one"));
	EXPECT_FALSE(b.Search("two"));
	EXPECT_TRUE(c.Search("two"));
	EXPECT_FALSE(c.Search("one"));
	EXPECT_FALSE(a.Search("one"));
	EXPECT_FALSE(a.Search("two"));
}

// move assignment swaps, so the source keeps the target's old contents; it
// just has to stay a valid, usable list
TEST(SkipListTest, MoveAssignedFromListIsUsable) {
	SkipList a;
	SkipList b;
	a.Add("a1");
	b.Add("b1");
	b = std::move(a);
	a.Add("new");
	EXPECT_TRUE(a.Search("new"));
	EXPECT_FALSE(b.Search("new"));
	a.Erase("new");
	EXPECT_FALSE(a.Search("new"));
}

// the level-0 chain is a linked list of unique_ptrs; a recursive destructor
// would blow the stack here
TEST(SkipListTest, LargeListDestructsAndCopiesWithoutStackOverflow) {
	auto list = std::make_unique<SkipList>();
	for (int i = 0; i < 1'000'000; ++i) {
		list->Add(std::to_string(i));
	}
	SkipList copy(*list);
	EXPECT_TRUE(copy.Search("999999"));
	list.reset();
}

// randomized differential test against std::set as the reference model
TEST(SkipListTest, MatchesStdSetUnderRandomOps) {
	std::mt19937 rng(12345);
	std::uniform_int_distribution<int> key_dist(0, 2000);
	std::uniform_int_distribution<int> op_dist(0, 2);
	SkipList list;
	std::set<std::string> reference;
	for (int step = 0; step < 200'000; ++step) {
		std::string key = std::to_string(key_dist(rng));
		switch (op_dist(rng)) {
		case 0:
			list.Add(key);
			reference.insert(key);
			break;
		case 1:
			ASSERT_EQ(list.Erase(key), reference.erase(key) == 1)
			    << "step " << step << " key " << key;
			break;
		case 2:
			ASSERT_EQ(list.Search(key), reference.count(key) == 1)
			    << "step " << step << " key " << key;
			break;
		}
	}
}

} // namespace
