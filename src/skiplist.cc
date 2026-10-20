#include "skiplist.h"

#include <algorithm>
#include <array>
#include <random>
#include <string>

// initialize head (left sentinel) + 0 height, initialize the engine (seeded via
// random_device{}())
SkipList::SkipList()
    : head_(std::make_unique<Node>("", kMaxHeight)), height_(0),
      rng_(std::random_device{}()) {}

SkipList::~SkipList() {
	std::unique_ptr<Node> current = std::move(head_);
	while (current.get() != nullptr) {
		current = std::move(current->next);
	}
}

SkipList::SkipList(const SkipList &other)
    : head_(std::make_unique<Node>("", kMaxHeight)), height_(other.height_),
      rng_(std::random_device{}()) {
	// last[i] = previous node we created at level i
	std::array<Node *, kMaxHeight> last;
	last.fill(head_.get());
	Node *source = other.head_->next.get();
	while (source != nullptr) {
		// build a tower with empty forward pointers first
		int source_height = static_cast<int>(source->forward.size());
		last[0]->next = std::make_unique<Node>(source->key, source_height);
		// make a local copy of the new node first, last[0] will be overwritten
		// by the loop
		Node *new_node = last[0]->next.get();
		// link pointers from each level from last
		for (int i = 0; i < source_height; ++i) {
			last[i]->forward[i] = new_node;
			last[i] = new_node;
		}
		source = source->next.get();
	}
}

SkipList &SkipList::operator=(const SkipList &other) {
	// implement via copy-and-swap
	// NOTE: copy constructor already freshly seeds "copy", so our rng_ is
	// freshly seeded
	SkipList copy = other;
	std::swap(head_, copy.head_);
	std::swap(height_, copy.height_);
	std::swap(rng_, copy.rng_);
	return *this;
}

SkipList::SkipList(SkipList &&other) noexcept
    : head_(std::move(other.head_)), height_(std::exchange(other.height_, 0)),
      rng_(std::move(other.rng_)) {
	// assign the other skiplist an empty head so it doesn't crash
	other.head_ = std::make_unique<Node>("", kMaxHeight);
}

SkipList &SkipList::operator=(SkipList &&other) noexcept {
	// NOTE: swap instead of move, since move would cause old head_ to be freed
	// (recursively), and could cause stack-depth issues. Swapping ensures the
	// iterative destructor is called on head_
	std::swap(head_, other.head_);
	std::swap(height_, other.height_);
	std::swap(rng_, other.rng_);
	return *this;
}

bool SkipList::Search(const std::string &key) const {
	std::array<SkipList::Node *, SkipList::kMaxHeight> predecessors =
	    FindPredecessors(key);
	Node *candidate = predecessors[0]->forward[0];
	return candidate != nullptr && candidate->key == key;
}

void SkipList::Add(const std::string &key) {
	// check if the key already exists, no-op if so
	std::array<Node *, kMaxHeight> predecessors = FindPredecessors(key);
	Node *candidate = predecessors[0]->forward[0];
	if (candidate != nullptr && candidate->key == key) {
		return;
	}
	int tower_height = RandomHeight();
	height_ = std::max(height_, tower_height);
	std::unique_ptr<Node> new_tower = std::make_unique<Node>(key, tower_height);
	for (int i = 0; i < tower_height; ++i) {
		// relink all the predecessors to include the new tower
		new_tower->forward[i] = predecessors[i]->forward[i];
		predecessors[i]->forward[i] = new_tower.get();
	}
	new_tower->next = std::move(predecessors[0]->next);
	predecessors[0]->next = std::move(new_tower);
}

bool SkipList::Erase(const std::string &key) {
	std::array<Node *, kMaxHeight> predecessors = FindPredecessors(key);
	Node *candidate = predecessors[0]->next.get();
	if (candidate == nullptr || candidate->key != key) {
		// element does not exist, return false
		return false;
	}
	// we only care about unlinking everything related to tower, so we take the
	// tower size
	int candidate_tower_size = static_cast<int>(candidate->forward.size());
	for (int i = candidate_tower_size - 1; i >= 0; --i) {
		predecessors[i]->forward[i] = candidate->forward[i];
	}
	// make the predecessor own the next ptr, frees the candidate
	predecessors[0]->next = std::move(candidate->next);
	// shrink height to max tower height (keep going down until the next value
	// is != nullptr)
	while (height_ > 0 && head_->forward[height_ - 1] == nullptr) {
		--height_;
	}
	return true;
}

int SkipList::RandomHeight() const {
	// coin(rng_) returns true with probability 0.5
	std::bernoulli_distribution coin(0.5);
	int current_random_height = 1;
	// keep on flipping until we get false, or
	while (current_random_height < kMaxHeight && coin(rng_)) {
		++current_random_height;
	}
	return current_random_height;
}

// NOTE: need to include SkipList::kMaxHeight since the compiler doesn't know
// its inside the class yet
std::array<SkipList::Node *, SkipList::kMaxHeight>
SkipList::FindPredecessors(const std::string &key) const {
	std::array<Node *, kMaxHeight> result;
	// the levels we don't walk keep head_ as their predecessor
	result.fill(head_.get());
	Node *current = head_.get();
	for (int i = height_ - 1; i >= 0; --i) {
		// keep on going until we've hit the "right sentinel" or we've hit a key
		// that's >= target
		while (current->forward[i] != nullptr &&
		       current->forward[i]->key < key) {
			current = current->forward[i];
		}
		result[i] = current;
	}
	return result;
}