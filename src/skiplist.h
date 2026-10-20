#pragma once

#include <array>
#include <memory>
#include <random>
#include <string>
#include <vector>

class SkipList {
public:
	SkipList();
	~SkipList();

	SkipList(const SkipList &other);
	SkipList &operator=(const SkipList &other);
	SkipList(SkipList &&other) noexcept;
	SkipList &operator=(SkipList &&other) noexcept;

	bool Search(const std::string &key) const;
	void Add(const std::string &key);
	bool Erase(const std::string &key);

private:
	// each node is a tower
	struct Node {
		// makes a tower with `height` levels (0 .. height - 1)
		Node(const std::string &k, int height)
		    : key(k), forward(height, nullptr) {}

		std::string key;
		// level-0 connection, this node OWNS the next node
		std::unique_ptr<Node> next;
		// forward[i] points to next tower that's at least (i + 1) tall (i.e, at
		// that level)
		std::vector<Node *> forward;
	};

	static constexpr int kMaxHeight = 32;

	int RandomHeight() const;

	// return the rightmost nodes of each level that have value < key
	std::array<Node *, kMaxHeight>
	FindPredecessors(const std::string &key) const;

	std::unique_ptr<Node> head_;
	int height_; // # of levels currently in use

	// initialize the rng engine with the mersenne twister algo (seeded with a
	// call to random_device{}) later mutable so the const RandomHeight()
	// function can advance the engine w/o breaking the const promise
	mutable std::mt19937 rng_;
};