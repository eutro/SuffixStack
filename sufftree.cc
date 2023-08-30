#include "sufftree.hpp"
#include <cassert>

sufftree::node::node(const node_or_leaf *lhs, const node_or_leaf *rhs)
    : lhs(lhs), rhs(rhs) {}

sufftree::indexed_string::indexed_string(forest &f, const leaves &leaves) {
  assocs.resize(leaves.size() + 1);
  if (leaves.empty()) return;

  nodes paired(leaves.begin(), leaves.end());

  for (size_t bit = 0;; ++bit) {
    size_t bit_m = the_bit(bit);
    for (size_t sz = bit_m; sz <= size(); ++sz) {
      bool set = sz & bit_m;
      auto
        &left = assocs.begin()[sz].left,
        &right = assocs.rbegin()[sz].right;
      if (set) {
        size_t offset = sz & (bit_m - 1);
        left.push_back(paired.begin()[offset]);
        right.push_back(paired.rbegin()[offset]);
      } else {
        left.push_back(nullptr);
        right.push_back(nullptr);
      }
    }
    if (the_bit(bit + 1) > size()) break;
    size_t pairings = paired.size() - bit_m;
    for (size_t i = 0; i < pairings; ++i) {
      paired[i] = f.intern(paired[i], paired[i + bit_m]);
    }
    paired.resize(pairings);
  }
}

sufftree::tree::tree(forest &owner) : owner(owner) {}

bool sufftree::tree::has_suffix(const indexed_string &itree) const {
  if (_size < itree.size()) {
    return false;
  }
  if (itree.empty()) {
    return true;
  }

  size_t on_right = compute_association(_size, itree.size()); // O(1)
  size_t on_left = itree.size() - on_right;
  const indexed_string::split &split = itree.association(on_right); // O(1)

  // check right tree
  auto acc_iter = trees.begin();
  // O(log(on_right))
  for (auto exp_iter = split.right.begin(); exp_iter != split.right.end();
       ++exp_iter, ++acc_iter) {
    if (*acc_iter != *exp_iter) {
      return false;
    }
  }

  // check left tree
  if (!on_left) {
    return true;
  }

  size_t borrowed_bit = __builtin_ctz(_size - on_right);
  const node_or_leaf *borrowed = trees[borrowed_bit];
  size_t left_bit = split.left.size();
  while (borrowed_bit > left_bit) {
    borrowed = static_cast<const node *>(borrowed)->rhs;
    --borrowed_bit;
  }
  for (auto left_iter = split.left.rbegin();
       left_iter != split.left.rend();
       ++left_iter) {
    const node_or_leaf *left_tree = *left_iter;
    const node *our_tree = static_cast<const node *>(borrowed);
    if (left_tree) {
      if (our_tree->rhs != left_tree) {
        return false;
      }
      borrowed = our_tree->lhs;
    } else {
      borrowed = our_tree->rhs;
    }
  }

  return true;
}

void sufftree::tree::append(const indexed_string &itree) {
  if (itree.empty()) {
    return;
  }

  size_t new_size = _size + itree.size();
  size_t on_right = compute_association(new_size, itree.size());
  size_t on_left = itree.size() - on_right;
  const indexed_string::split &split = itree.association(on_right);

  trees.resize(bit_width(new_size), nullptr);

  if (on_left) {
    // a present bit in on_left indicates that split.left contains a
    // tree that we need a LHS for, an absent bit means we need to
    // find a tree of that size to combine our existing tree with
    size_t bit_no = __builtin_ctz(on_left);
    const node_or_leaf *constructing = trees[bit_no];
    // 1 << bit_no is the size of `constructing` at the start of the
    // loop
    trees[bit_no] = nullptr;
    for (; the_bit(bit_no) <= on_left; ++bit_no) {
      bool supplied = on_left & the_bit(bit_no);
      if (supplied) {
        constructing = owner.intern(constructing, split.left[bit_no]);
      } else {
        const node_or_leaf *&tr = trees[bit_no];
        constructing = owner.intern(tr, constructing);
        tr = nullptr;
      }
    }
    while (true) {
      const node_or_leaf *&lhs = trees[bit_no];
      if (!lhs) break;
      constructing = owner.intern(lhs, constructing);
      lhs = nullptr;
      ++bit_no;
    }
    trees[bit_no] = constructing;
  }

  size_t remaining_right = on_right;
  auto src_iter = split.right.begin();
  auto dst_iter = trees.begin();
  while (remaining_right) {
    size_t step = __builtin_ctz(remaining_right);
    src_iter += step;
    dst_iter += step;
    assert(!*dst_iter);
    *dst_iter = *src_iter;
    ++dst_iter; ++src_iter;
    remaining_right >>= step + 1;
  }

  _size = new_size;
}

void sufftree::tree::truncate(size_t new_size) {
  size_t to_remove = _size - new_size;

  size_t on_right = compute_association(_size, to_remove);
  size_t on_left = to_remove - on_right;

  auto my_iter = trees.begin();
  size_t right_iter = on_right;
  while (right_iter) {
    unsigned step = __builtin_ctz(right_iter);
    my_iter += step;
    assert(*my_iter);
    *my_iter = nullptr;
    my_iter++;
    right_iter >>= step + 1;
  }

  if (on_left) {
    size_t to_deconstruct = __builtin_ctz(_size - on_right);
    size_t to_remain = the_bit(to_deconstruct) - on_left;
    // deconstruct this tree
    const node_or_leaf *splitting = trees[to_deconstruct];
    size_t bit_no = bit_width(to_remain) - 1;
    size_t bit = the_bit(bit_no);
    for (; bit; --bit_no, bit >>= 1) {
      bool keeping = to_remain & bit;
      const node *branch = static_cast<const node *>(splitting);
      if (keeping) {
        trees[bit_no] = branch->lhs;
        splitting = branch->rhs;
      } else {
        splitting = branch->lhs;
      }
    }
  }

  _size = new_size;
  trees.resize(bit_width(_size));
}

void sufftree::node::iterator::move(difference_type by) {
  if (by == 0) {
    return;
  }
  size_t oldIdx = idx;
  size_t newIdx;
  if (by < 0 && idx < (size_t) -by) {
    over = true;
    newIdx = 0;
  } else if (by > 0 && size() - idx < (size_t) by) {
    over = true;
    newIdx = size() - 1;
  } else {
    over = false;
    newIdx = idx + by;
  }
  size_t delta = newIdx ^ oldIdx;
  if (!delta) return;
  idx = newIdx;
  resolve_from(bit_width(delta));
}

void sufftree::node::iterator::resolve_from(size_t width) {
  for (int it = width - 1; it >= 0; --it) {
    stack[it] = static_cast<const node &>(*stack[it + 1])[idx & the_bit(it)];
  }
}

sufftree::node::iterator
sufftree::node::iterator::operator-(difference_type delta) {
  iterator cp = *this;
  cp -= delta;
  return cp;
}
sufftree::node::iterator &
sufftree::node::iterator::operator-=(difference_type delta) {
  move(-delta);
  return *this;
}
sufftree::node::iterator
sufftree::node::iterator::operator+(difference_type delta) {
  iterator cp = *this;
  cp += delta;
  return cp;
}
sufftree::node::iterator &
sufftree::node::iterator::operator+=(difference_type delta) {
  move(delta);
  return *this;
}
sufftree::node::iterator sufftree::node::iterator::operator--(int) {
  iterator cp = *this;
  ++*this;
  return cp;
}
sufftree::node::iterator &sufftree::node::iterator::operator--() {
  move(-1);
  return *this;
}
sufftree::node::iterator sufftree::node::iterator::operator++(int) {
  iterator cp = *this;
  ++*this;
  return cp;
}
sufftree::node::iterator &sufftree::node::iterator::operator++() {
  move(1);
  return *this;
}
