// include/util/lru_cache.h
//
// YOU IMPLEMENT (this whole file; it is a template). Spec §4.2.
//
// A fixed-capacity cache that evicts the Least Recently Used entry.
// Both get() and put() must be O(1) (expected), so it is built from two
// structures that point at the same nodes:
//
//   std::unordered_map<Key, Node*>   finds a key's node in O(1)
//   a doubly linked list of Nodes    keeps recency order:
//        head_ = most recently used ... tail_ = least recently used
//
// Rules for this file:
//   * Write the doubly linked list yourself (Node with prev/next pointers,
//     new/delete inside this class only). Do NOT use std::list, std::deque
//     or std::forward_list (don't even #include their headers), or a
//     std::vector you shift around (spec §7.3).
//   * std::unordered_map is allowed (and expected) for the key -> Node* index.
//   * No leaks, no double frees: ASan runs on every test.

#pragma once
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LruCache {
public:
    struct Stats {
        std::size_t hits = 0;        // get() found the key
        std::size_t misses = 0;      // get() did not find the key
        std::size_t evictions = 0;   // put() removed the LRU entry to make room
    };

    // A cache holding at most `capacity` entries. Capacity 0 is legal: such a
    // cache never stores anything (every get() is a miss).
    explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

    // Frees every node.
    ~LruCache() { clear(); }

    // Copying a cache of raw-pointer nodes is not needed; forbid it.
    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    // lookup a key and mark it as most recently used if found
    const Value* get(const Key& key) {
        auto it = map_.find(key); // lookup the key
        // Check whether key was found
        if (it != map_.end()) {
            ++stats_.hits;          // count a hit (successful lookup)
            Node* n = it->second;   // get the node pointer
            move_to_front(n);       // mark the node as most recently used
            return &n->value;       // return the address of its cached value
        }
        // Otherwise, the key isn't cached
        else {
            ++stats_.misses;        // count a miss (unsuccessful lookup)
            return nullptr;         // return with no cache found
        }
    }

    // Key present: replaces its value and marks it most recently used (no
    //   eviction, no hit/miss counted).
    // Key absent: if the cache is full, evicts the least recently used entry
    //   (counting one eviction), then inserts the new entry as most recently
    //   used. With capacity 0, does nothing.
    void put(const Key& key, Value value) {
        // lookup the key
        auto it = map_.find(key); 
        // If key exist replace its value
        if(it != map_.end()) {
            Node* n = it->second;           // get existing node
            n->value = std::move(value);    // replace its cache value
            move_to_front(n);               // mark it as most recently used
            return;
        }
        //Otherwise, key does not exist
        else {
            // A cache with no capacity cannot store anything
            if(capacity_ == 0) {
                return;
            }
            // If full, evict the least recently used entry
            if (map_.size() == capacity_) {
                Node* victim = tail_;      // save least recently used node
                unlink(victim);            // unlink it from the list
                map_.erase(victim->key);    // remove its key
                delete victim;             // free the removed node
                ++stats_.evictions;         // count one eviction
            }

            // Create and insert the new entry
            Node* n = new Node{key, std::move(value), nullptr, nullptr};
            map_[key] = n;                  // connect key to its node
            push_front(n);                  // mark it most recently used
        }
    }

    // True if the key is cached. Does NOT change recency or stats.
    bool contains(const Key& key) const {
        // lookup key
        auto it = map_.find(key); 
        // Return whether the key was found
        if(it != map_.end()) {
            return true;
        }

        return false;
    }

    // Removes the key if present (not an eviction). Returns whether it was.
    bool erase(const Key& key) {
        // lookup key
        auto it = map_.find(key);
        // If key was found
        if (it != map_.end()) {
            Node* n = it->second;       // save existing node
            unlink(n);                  // unlink it
            map_.erase(it);             // remove the map entry
            delete n;                   // free the node
            return true;                // return successful erase
        }
        // If key wasn't found
        return false;       // return unsuccessful erase
    }

    // Removes every entry. Stats are kept.
    void clear() {
        // TODO: delete every node, empty the map, reset head_/tail_.
        // Clue: save n->next in a local BEFORE  delete n;  a loop like
        //   for (Node* n = head_; n; n = n->next) delete n;
        // reads n->next from freed memory.
        
        // Start at the head
        Node* current = head_;

        // Repeat until the current node is nullptr
        while (current != nullptr) {
            Node* next = current->next; // save next node before deleting current node
            delete current;             // free current node
            current = next;             // move to the saved next node
        }

        // Empty the map
        map_.clear();

        // Reset the head and tail
        head_ = nullptr;
        tail_ = nullptr;


    }

    std::size_t size() const noexcept { return map_.size(); }
    std::size_t capacity() const noexcept { return capacity_; }
    Stats       stats() const noexcept { return stats_; }

    // Keys from most to least recently used, by walking the list from head_.
    // Used by the tests to check your recency order.
    std::vector<Key> keys_mru_to_lru() const {
        std::vector<Key> out;
        for (Node* n = head_; n != nullptr; n = n->next) out.push_back(n->key);
        return out;
    }

private:
    struct Node {
        Key   key;      // identify this entry in the hash map
        Value value;    // the cached data
        Node* prev;     // points towards the more recently used node
        Node* next;     // points towards the less recently used node
    };

    void unlink(Node* n) {
        // If a previous node exists, connect it to the next node
        if (n->prev != nullptr) {
            n->prev->next = n->next;
        }
        // Otherwise update the next node as the new head
        else {
            head_ = n->next;
        }

        // If the next node exists, connect it to the previous node
        if(n->next != nullptr) {
            n->next->prev = n->prev;
        }
        // Otherwise update the tail as the previous node
        else {
            tail_ = n->prev;
        }

        // Clear the detached nodes links
        n->prev = nullptr;
        n->next = nullptr;
    }

    void push_front(Node* n) {
        // If the list has nodes
        if(head_ != nullptr) {
            n->prev = nullptr;
            n->next = head_;
            head_->prev = n;
            head_ = n;
        }
        // Otherwise, the list is empty
        else {
            n->prev = nullptr;
            n->next = nullptr;
            head_ = n;
            tail_ = n;
        }
    }

    void move_to_front(Node* n) {
        // If node is the head
        if(n == head_) {
            return;
        }
        // Otherwise, unlinked node and push to front
        else {
            unlink(n);
            push_front(n);
        }
    }

    // ===== YOUR HELPERS AND MEMBERS =====
    // Suggested helpers: unlink(Node*), push_front(Node*), move_to_front(Node*).
    // Clue for unlink(n): four cases. If n has a prev, prev->next skips n,
    // otherwise n was head_ and head_ moves to n->next. Likewise, if n has
    // a next, next->prev skips n, otherwise n was tail_ and tail_ moves to
    // n->prev. Unlinking the only node leaves head_ == tail_ == nullptr.
    // Spec Appendix B lists every pointer write.

    std::unordered_map<Key, Node*, Hash> map_;
    Node*       head_ = nullptr;
    Node*       tail_ = nullptr;
    std::size_t capacity_;
    Stats       stats_;
};
