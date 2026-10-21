#include "db.h"

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <random>
#include <string>

namespace SmilingDB {
namespace {

TEST(DBTest, PutThenGet) {
	DB db;
	db.Put("k", "v");
	EXPECT_EQ(db.Get("k"), "v");
}

TEST(DBTest, GetMissingKeyIsNullopt) {
	DB db;
	EXPECT_EQ(db.Get("missing"), std::nullopt);
	db.Put("other", "v");
	EXPECT_EQ(db.Get("missing"), std::nullopt);
}

TEST(DBTest, PutOverwrites) {
	DB db;
	db.Put("k", "first");
	db.Put("k", "second");
	EXPECT_EQ(db.Get("k"), "second");
}

TEST(DBTest, RemoveThenGetIsNullopt) {
	DB db;
	db.Put("k", "v");
	db.Remove("k");
	EXPECT_EQ(db.Get("k"), std::nullopt);
}

TEST(DBTest, RemoveMissingKeyIsHarmless) {
	DB db;
	db.Remove("ghost");
	EXPECT_EQ(db.Get("ghost"), std::nullopt);
	db.Put("ghost", "now here");
	EXPECT_EQ(db.Get("ghost"), "now here");
}

TEST(DBTest, PutAfterRemoveIsVisible) {
	DB db;
	db.Put("k", "old");
	db.Remove("k");
	db.Put("k", "new");
	EXPECT_EQ(db.Get("k"), "new");
}

// an empty value is a real value, distinct from a missing key or a tombstone
TEST(DBTest, EmptyValueIsFound) {
	DB db;
	db.Put("k", "");
	std::optional<std::string> result = db.Get("k");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, "");
}

TEST(DBTest, EmptyKey) {
	DB db;
	EXPECT_EQ(db.Get(""), std::nullopt);
	db.Put("", "v");
	EXPECT_EQ(db.Get(""), "v");
	db.Remove("");
	EXPECT_EQ(db.Get(""), std::nullopt);
}

// keys and values are arbitrary bytes, not C strings
TEST(DBTest, BinaryKeysAndValues) {
	DB db;
	std::string key("a\0b", 3);
	std::string value("\0\xff\0", 3);
	db.Put(key, value);
	EXPECT_EQ(db.Get(key), value);
	EXPECT_EQ(db.Get(std::string("a", 1)), std::nullopt);
}

TEST(DBTest, GetWorksOnConstDB) {
	DB db;
	db.Put("k", "v");
	const DB &ref = db;
	EXPECT_EQ(ref.Get("k"), "v");
}

TEST(DBTest, ManyKeys) {
	DB db;
	for (int i = 0; i < 10'000; ++i) {
		db.Put("key" + std::to_string(i), "value" + std::to_string(i));
	}
	for (int i = 0; i < 10'000; i += 2) {
		db.Remove("key" + std::to_string(i));
	}
	for (int i = 0; i < 10'000; ++i) {
		std::optional<std::string> result = db.Get("key" + std::to_string(i));
		if (i % 2 == 0) {
			EXPECT_EQ(result, std::nullopt) << i;
		} else {
			EXPECT_EQ(result, "value" + std::to_string(i)) << i;
		}
	}
}

// randomized differential test against std::map as the reference model
TEST(DBTest, MatchesStdMapUnderRandomOps) {
	std::mt19937 rng(12345);
	std::uniform_int_distribution<int> key_dist(0, 500);
	std::uniform_int_distribution<int> op_dist(0, 2);
	DB db;
	std::map<std::string, std::string> reference;
	for (int step = 0; step < 100'000; ++step) {
		std::string key = std::to_string(key_dist(rng));
		switch (op_dist(rng)) {
		case 0: {
			std::string value = std::to_string(step);
			db.Put(key, value);
			reference[key] = value;
			break;
		}
		case 1:
			db.Remove(key);
			reference.erase(key);
			break;
		case 2: {
			auto it = reference.find(key);
			std::optional<std::string> expected;
			if (it != reference.end()) {
				expected = it->second;
			}
			ASSERT_EQ(db.Get(key), expected)
			    << "step " << step << " key " << key;
			break;
		}
		}
	}
}

} // namespace
} // namespace SmilingDB
