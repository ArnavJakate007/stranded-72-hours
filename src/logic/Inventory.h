#pragma once
#include <algorithm>
#include <functional>
#include <vector>

// Template container for things Kiran carries (GDD "add later": templates).
// Each item takes sizeOf(item) slots; the total may not exceed the capacity.
template <typename T>
class Inventory {
public:
    explicit Inventory(int capacity, std::function<int(const T&)> sizeOf = [](const T&) { return 1; })
        : capacity_(capacity), sizeOf_(std::move(sizeOf)) {}

    int capacity() const { return capacity_; }
    int used() const {
        int n = 0;
        for (const T& it : items_) n += sizeOf_(it);
        return n;
    }
    bool canAdd(const T& item) const { return used() + sizeOf_(item) <= capacity_; }
    bool add(const T& item) {
        if (!canAdd(item)) return false;
        items_.push_back(item);
        return true;
    }
    bool remove(const T& item) {
        auto it = std::find(items_.begin(), items_.end(), item);
        if (it == items_.end()) return false;
        items_.erase(it);
        return true;
    }
    bool contains(const T& item) const { return std::find(items_.begin(), items_.end(), item) != items_.end(); }
    int sizeOf(const T& item) const { return sizeOf_(item); }
    const std::vector<T>& items() const { return items_; }
    void clear() { items_.clear(); }

    // Moves every item matching keep==false out of the inventory and returns them.
    std::vector<T> takeAllExcept(const std::function<bool(const T&)>& keep) {
        std::vector<T> out, rest;
        for (const T& it : items_) (keep(it) ? rest : out).push_back(it);
        items_ = rest;
        return out;
    }

private:
    int capacity_;
    std::function<int(const T&)> sizeOf_;
    std::vector<T> items_;
};
