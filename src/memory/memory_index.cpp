// src/memory/memory_index.cpp — YOU IMPLEMENT. Spec §4.3.

#include "memory/memory_index.h"
#include <utility>  // provides std::move

// Insert a record and rebalance on the way back up
MemoryIndex::Node* MemoryIndex::insert_node(
    Node* n, MemoryRecord rec, bool& inserted) {

    // An empty subtree is a fresh slot
    if (n == nullptr) {
        Node* new_node = new Node{std::move(rec)}; // create a node containing rec
        inserted = true;    // Set inserted to true
        return new_node;    // return new node
    }

    // Leave a duplicate ID unchanged
    if (rec.id == n->rec.id) {
        return n;
    }

    // Insert smaller IDs into the left subtree
    if (rec.id < n->rec.id) {
        // Assign the recursive result to n->left
        n->left = insert_node(n->left, std::move(rec), inserted);
    }
    // Insert larger IDs into the right subtree
    else {
        // Assign the recursive result to n->right
        n->right = insert_node(n->right, std::move(rec), inserted);
    }

    return rebalance(n); // Return the balanced subtree root
}

MemoryIndex::~MemoryIndex() {
    // TODO: free every node (post-order: both children before the node).
    (void)root_;
}

bool MemoryIndex::insert(MemoryRecord rec) {
    // Track whether a new node was added
    bool inserted = false;  
    
    // Pass the record to the helper and save the updated root
    root_ = insert_node(root_, std::move(rec), inserted); 
    
    // Only count newly inserted records
    if (inserted) {
        ++count_;
    }

    return inserted;    // report whether insert succeeded
}

const MemoryRecord* MemoryIndex::find(std::uint64_t id) const {
    // TODO
    (void)id;
    return nullptr;
}

bool MemoryIndex::remove(std::uint64_t id) {
    // TODO: leaf, one child, two children (use the in-order successor or
    // predecessor). Spec Appendix C draws all three.
    // Clue: the successor (leftmost node of the right subtree) has no left
    // child but MAY have a right child: that child takes its place.
    // Clue: --count_ exactly once per successful remove. Counting inside
    // the helper AND here double-counts the two-children case.
    // Clue: when the removed node is the root, root_ itself must change.
    (void)id;
    return false;
}

std::size_t MemoryIndex::size() const noexcept { return count_; }

std::size_t MemoryIndex::height() const {
    // TODO: empty tree = 0, one node = 1, otherwise 1 + the taller child.
    return 0;
}

void MemoryIndex::for_each_in_order(const std::function<void(const MemoryRecord&)>& fn) const {
    // TODO: left subtree, then fn(node->rec), then right subtree.
    (void)fn;
}

void MemoryIndex::clear() {
    // TODO
    // Clue: after freeing the nodes, set root_ = nullptr and count_ = 0, or
    // the destructor frees them a second time.
}
