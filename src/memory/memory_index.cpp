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

// Remove a record and rebalance on the way back up
MemoryIndex::Node* MemoryIndex::remove_node(
    Node* n, std::uint64_t id, bool& removed) {

    // An empty subtree means the ID wasn't found
    if (n == nullptr) {
        return nullptr;
    }

    // Search the left subtree for a smaller ID
    if (id < n->rec.id) {
        // Reconnect the subtree returned by the recursive call
        n->left = remove_node(n->left, id, removed);
    }
    // Search the right subtree for a larger ID
    else if (id > n->rec.id) {
        // Reconnect the subtree returned by the recursive call
        n->right = remove_node(n->right, id, removed);
    }
    // Otherwise, this is the node to remove
    else {
        // Mark removal as successful
        removed = true;

        // No Child Case: delete the leaf and return empty subtree
        if (n->left == nullptr && n->right == nullptr) {
            delete n;           // free the leaf
            return nullptr;     // replace it with empty subtree
        }

        // One Child Case: save child, delete current node, return child
        if (n->left == nullptr || n->right == nullptr) {
            // Declare a child
            Node* child = nullptr;  
            
            // Save the left child
            if(n->left != nullptr) {
                child = n->left;
            }

            // Save the right child
            else {
                child = n->right;
            }

            delete n;           // free the original node
            return child;       // return child and let caller reconnect it
        }

        // Two Child Case: Use In-Order Successor
        
        // Declare successor in current nodes right subtree
        Node* successor = n->right;  
        
        // Find the smallest ID in the right subtree
        while (successor->left != nullptr) {
            successor = successor->left;
        }

        // Copy successor into current node and remove orignal successor and reconnect
        n->rec = successor->rec;
        n->right = remove_node(n->right, successor->rec.id, removed); 
    }

    return rebalance(n); // Restore balance and return the subtree root
}

// Traverse left, process this record, then traverse right
void MemoryIndex::in_order(
    const Node* n, const std::function<void(const MemoryRecord&)>& fn) {

    // Stop at an empty subtree
    if (n == nullptr) {
        return;
    }

    // Recursively traverse the left subtree using the same action
    in_order(n->left, fn);

    // Run it on this nodes record exactly once
    fn(n->rec);

    // Recursively transverse the right subtree using the same action
    in_order(n->right, fn);
}

// Free every node using post-order traversal
void MemoryIndex::post_order(Node* n) {
    // Stop at an empty subtree
    if (n == nullptr) {
        return;
    }

    // Recursively delete the left subtree
    post_order(n->left);

    // Recursively delete the right subtree
    post_order(n->right);

    // Delete the current node
    delete n;

}

// Destructor: free all nodes when memory index is destroyed
MemoryIndex::~MemoryIndex() {
    clear();
}

// Insert a unique record and update the root and node count
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

// Find a record ID or return nullptr if missing
const MemoryRecord* MemoryIndex::find(std::uint64_t id) const {
    const Node* n = root_;

    // Keep searching while the current node exists
    while( n != nullptr) {
        // If ID matches, return the address of n's record
        if (id == n->rec.id) {
            return &n->rec;
        }

        // If target ID is smaller, move to current nodes left child
        if (id < n->rec.id) {
            n = n->left;
        }

        // Otherwise, target ID is larger, move to current nodes right child
        else {
            n = n->right;
        }
    }

    return nullptr; // ID was not found

}

// Remove a record by ID and update root and node count
bool MemoryIndex::remove(std::uint64_t id) {
    // Track whether a new node was removed
    bool removed = false;  
    
    // Remove the ID and save the updated root
    root_ = remove_node(root_, id, removed); 

    // Only decrement count if remove was successful
    if (removed) {
        --count_;
    }

    return removed;    // report whether removal succeeded
}

// Return the number of stored records
std::size_t MemoryIndex::size() const noexcept { return count_; }

// Return the tree's height or 0 if empty
std::size_t MemoryIndex::height() const {
    // Return the whole tree's height
    return height_of(root_);
}

// Apply the fn action to every record in ascending ID order
void MemoryIndex::for_each_in_order(const std::function<void(const MemoryRecord&)>& fn) const {
    in_order(root_, fn);
}

// Delete every node and reset the tree to be empty
void MemoryIndex::clear() {
    post_order(root_);
    root_ = nullptr;
    count_ = 0;
}
