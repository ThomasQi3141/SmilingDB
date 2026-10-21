#pragma once

#include "entry.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class SkipList {
private:
	// declare here so that Iterator can use it
	// each node is a tower
	struct Node {
		// makes a tower with `height` levels (0 .. height - 1)
		Node(std::string k, Entry v, int height)
		    : key(std::move(k)), value(std::move(v)), forward(height, nullptr) {
		}

		std::string key;
		// the value entry for a given node
		Entry value;
		// level-0 connection, this node OWNS the next node
		std::unique_ptr<Node> next;
		// forward[i] points to next tower that's at least (i + 1) tall (i.e, at
		// that level)
		std::vector<Node *> forward;
	};

public:
	SkipList();
	~SkipList();

	SkipList(const SkipList &other);
	SkipList &operator=(const SkipList &other);
	SkipList(SkipList &&other);
	SkipList &operator=(SkipList &&other) noexcept;

	class Iterator {
	public:
		explicit Iterator(const Node *node) : node_(node) {}

		const std::string &key() const { return node_->key; }
		const Entry &value() const { return node_->value; }
		std::pair<const std::string &, const Entry &> operator*() const {
			return {node_->key, node_->value};
		}

		Iterator &operator++() {
			node_ = node_->next.get();
			return *this;
		}
		bool operator==(const Iterator &other) const {
			return node_ == other.node_;
		}
		bool operator!=(const Iterator &other) const {
			return !(*this == other);
		}

	private:
		const Node *node_;
	};

	Iterator begin() const { return Iterator(head_->next.get()); }
	Iterator end() const { return Iterator(nullptr); }

	const Entry *Search(std::string_view key) const;
	std::optional<size_t> Add(std::string key, Entry value);
	bool Erase(std::string_view key);
	bool Empty() const { return head_->next == nullptr; }

	// the approximate overhead each skiplist node uses (beyond
	// the key & value bytes)
	static constexpr size_t kApproxNodeOverhead =
	    sizeof(Node) + 2 * sizeof(Node *);

private:
	static constexpr int kMaxHeight = 16;

	int RandomHeight() const;

	// return the rightmost nodes of each level that have value < key
	std::array<Node *, kMaxHeight> FindPredecessors(std::string_view key) const;

	std::unique_ptr<Node> head_;
	int height_; // # of levels currently in use

	// initialize the rng engine with the mersenne twister algo (seeded with a
	// call to random_device{}) later mutable so the const RandomHeight()
	// function can advance the engine w/o breaking the const promise
	mutable std::mt19937 rng_;
};