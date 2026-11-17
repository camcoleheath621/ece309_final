// include/util/min_heap.h
//
// YOU IMPLEMENT (this whole file; it is a template, so the code lives in the
// header). Spec §4.1.
//
// A binary MIN-heap stored in an array (a std::vector), plus top_k(), which
// uses a size-k min-heap to pick the k largest items in O(n log k).
//
// Rules for this file:
//   * Write sift-up and sift-down yourself.
//   * Do NOT use std::priority_queue, std::make_heap / push_heap / pop_heap /
//     sort_heap, std::sort / stable_sort / partial_sort / nth_element, or
//     std::set / multiset / map / multimap (full list: spec §7.3).
//   * std::vector is fine as the storage array; std::swap, std::reverse and
//     std::pair are fine too.
//
// `Less` is a "less-than" function object, exactly like the one std::sort
// takes: less(a, b) is true when a should come before b. The heap keeps the
// SMALLEST element (by less) at the top.

#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

template <typename T, typename Less = std::less<T>>
class MinHeap
{
public:
    explicit MinHeap(Less less = Less()) : less_(std::move(less)) {}

    // Adds value. O(log n).
    void push(T value)
    {
        // TODO: append to data_, then sift the new element up.
        // Clue: compare with less_(a, b), never a < b. Some element types
        // (e.g. the provided memory_store.cpp's Scored struct) have no
        // operator<, and a min-heap of std::greater must still work.

        data_.push_back(std::move(value));
        sift_up(data_.size() - 1);
    }

    // The smallest element. Throws std::out_of_range if empty. O(1).
    const T &top() const
    {
        // An empty heap has no root to return
        if (data_.empty())
        {
            throw std::out_of_range("heap is empty");
        }

        return data_[0]; // Return the root without removing it
    }

    // Removes and returns the smallest element. Throws std::out_of_range if
    // empty. O(log n).
    T pop()
    {
        // TODO: save the root, move the last element to the root, shrink,
        // sift down, return the saved root.
        // Clue: check empty() first. With one element, there is no "last
        // element" to move: just take it and pop_back().
        // Clue: T result = std::move(data_[0]); then move the last element
        // into slot 0 (moving avoids copying big strings).
        
        // An empty heap has nothing to remove
        if (data_.empty())
        {
            throw std::out_of_range("heap is empty");
        }

        T result = std::move(data_[0]); // save the original root

        if (data_.size() == 1) {
            data_.pop_back();           // remove the only item
            return result;
        }

        data_[0] = std::move(data_.back());  // move last item to root
        data_.pop_back();                   // remove last item and decrease size
        sift_down(0);                       // restore the heap order from root
        
        return result;  // return original root
    }

    std::size_t size() const noexcept { return data_.size(); }
    bool empty() const noexcept { return data_.empty(); }
    void clear() noexcept { data_.clear(); }

private:
    // ===== YOUR HELPERS AND MEMBERS =====
    // Children of index i are 2i+1 and 2i+2; its parent is (i-1)/2.
    // Suggested: void sift_up(std::size_t i);  void sift_down(std::size_t i);
    // Clue: use std::size_t (not int) for indices, or -Wsign-compare warns
    // every time you compare with data_.size(). Careful: (i-1)/2 with i == 0
    // underflows, so sift_up must stop at i == 0.
    // Clue: in sift_down, the right child may not exist: check
    // 2*i+2 < data_.size() before reading it. Swap with the SMALLER child.

    void sift_up(std::size_t i)
    {
        // Stop when the item reaches the root
        while (i > 0)
        {
            std::size_t parent = (i - 1) / 2; // Parent's index

            // Stop if the child is not smaller than its parent
            if (!less_(data_[i], data_[parent]))
            {
                break;
            }

            // Otherwise swap upward
            std::swap(data_[i], data_[parent]);
            i = parent; // follow that item
        }
    }

    void sift_down(std:: size_t i) {
        
        while (true) {
            std::size_t left = 2 * i + 1;   // Left child's index
            std::size_t right = left + 1;   // Right child's index

            // No left child means no children
            if (left >= data_.size()) {
                break;
            }

            // Otherwise...
            std::size_t smaller = left;     // assume Left child is smallest

            // Choose right child if its smaller
            if (right < data_.size() && less_(data_[right], data_[left])) {
                smaller = right;
            }

            // Stop if item is less than both children
            if (!less_(data_[smaller], data_[i])) {
                break;
            }

            std::swap(data_[i], data_[smaller]);    // Move item down
            i = smaller;    // follow that item                            
        }
    }

    std::vector<T> data_;
    Less less_;
};

// Returns the k LARGEST items of `items` (by `less`), largest first.
//   * k == 0 or items empty -> empty result
//   * k >= items.size()     -> every item, largest first
// Must keep a MinHeap of at most k items (the k largest seen so far, with
// the smallest of them on top), so it runs in O(n log k) time and O(k) extra
// space. Items that compare equal may come out in any order.
template <typename T, typename Less = std::less<T>>
std::vector<T> top_k(const std::vector<T> &items, std::size_t k, Less less = Less())
{
    // TODO: keep a MinHeap<T, Less> of at most k items. For each item: if
    // the heap has fewer than k items, push it; otherwise, if the heap's top
    // is less than the item, pop the top and push the item. At the end, pop
    // everything (smallest first) and return it reversed.
    //
    // Clues:
    //  * k == 0: return {} BEFORE the loop (top() on an empty heap throws).
    //  * Build the heap WITH the comparator:  MinHeap<T, Less> heap(less);
    //    Writing  MinHeap<T, Less> heap;  fails for lambdas ("use of deleted
    //    function ... lambda") because a lambda can't be default-constructed.
    //  * "top is less than the item" means  less(heap.top(), item),  not
    //    heap.top() < item.  Read less(a, b) as "a is WORSE than b": the heap
    //    keeps the k best items with the worst of them on top.
    //  * std::reverse (from <algorithm>) flips a vector.

    // Nothing requested means nothing to return
    if (k == 0) {
        return {};
    }

    MinHeap<T, Less> heap(less);    // create a candidate heap

    // Examine every item without copying it
    for (const T& item : items) {
        // Fill candidate heap until it has k items
        if(heap.size() < k) {
            heap.push(item);
        }
        // if a larger value is found, replace the smallest candidate
        else if (less(heap.top(), item)) {
            heap.pop();         // remove smallest candidate
            heap.push(item);    // add the larger value
        }
    }

    std:: vector<T> result; // store the selected items

    // Pop the selected values into ascending order
    while(!heap.empty()) {
        result.push_back(heap.pop());
    }

    // Reverse the results into decesending order
    std::reverse(result.begin(), result.end());

    return result;  // return the items, largest first
}
