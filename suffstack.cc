#include "suffstack.hpp"
#include <cassert>

namespace suffstack {

bool node::operator<(const node &o) const {
  std::less<> lt;
  if (lt(lhs, o.lhs)) return true;
  if (lhs == o.lhs) return lt(rhs, o.rhs);
  return false;
}

void indexed_string::index_from(node_arena &f, nodes &&paired) {
  assocs.resize(paired.size() + 1);
  if (paired.size() == 0) return;

  for (size_t bit = 0;; ++bit)
  /* O(log N) iterations */ {
    size_t bit_m = the_bit(bit);
    for (size_t sz = bit_m; sz <= size(); ++sz)
    /* O(\sum_{i=0}^(log N) N - log i)
       = O(N log N) iterations total */
    {
      // constant work in body
      bool set = sz & bit_m;
      auto &left = assocs.begin()[sz].left, &right = assocs.rbegin()[sz].right;
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
    size_t pairings = paired.size() - bit_m /* = O(N - log i) */;
    for (size_t i = 0; i < pairings; ++i)
    /* O(\sum_{i=0}^(log N) N - log i)
       = O(N log N) iterations total */
    {
      // constant work (and allocation)
      paired[i] = f.intern(paired[i], paired[i + bit_m]);
    }
    paired.resize(pairings);
  }
}

bool tree_stack_base::has_suffix(const indexed_string &itree) const {
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
  // O(log(on_right))
  if (!std::equal(split.right.begin(), split.right.end(), trees.begin())) {
    return false;
  }

  // check left tree
  if (!on_left) {
    return true;
  }

  size_t borrowed_bit = std::countr_zero(_size - on_right);
  const node_or_leaf *borrowed = trees[borrowed_bit];
  size_t left_bit = split.left.size();
  while (borrowed_bit > left_bit) {
    borrowed = static_cast<const node *>(borrowed)->rhs;
    --borrowed_bit;
  }
  for (; left_bit; --left_bit) {
    const node_or_leaf *left_tree = split.left[left_bit - 1];
    const node *our_tree = static_cast<const node *>(borrowed);
    if (on_left & the_bit(left_bit - 1)) {
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

void tree_stack_base::append(const indexed_string &itree) {
  if (itree.empty()) {
    return;
  }

  size_t new_size = _size + itree.size();
  size_t on_right = compute_association(new_size, itree.size());
  size_t on_left = itree.size() - on_right;
  const indexed_string::split &split = itree.association(on_right);

  trees.resize(std::bit_width(new_size), nullptr);

  if (on_left) {
    // a present bit in on_left indicates that split.left contains a
    // tree that we need a LHS for, an absent bit means we need to
    // find a tree of that size to combine our existing tree with
    size_t bit_no = std::countr_zero(on_left);
    const node_or_leaf *constructing = trees[bit_no];
    // 1 << bit_no is the size of `constructing` at the start of the
    // loop
    trees[bit_no] = nullptr;
    for (; the_bit(bit_no) <= on_left; ++bit_no) {
      bool supplied = on_left & the_bit(bit_no);
      if (supplied) {
        constructing = arena.intern(constructing, split.left[bit_no]);
      } else {
        const node_or_leaf *&tr = trees[bit_no];
        constructing = arena.intern(tr, constructing);
        tr = nullptr;
      }
    }
    while (true) {
      const node_or_leaf *&lhs = trees[bit_no];
      if (!lhs) break;
      constructing = arena.intern(lhs, constructing);
      lhs = nullptr;
      ++bit_no;
    }
    trees[bit_no] = constructing;
  }

  size_t remaining_right = on_right;
  auto src_iter = split.right.begin();
  auto dst_iter = trees.begin();
  while (remaining_right) {
    size_t step = std::countr_zero(remaining_right);
    src_iter += step;
    dst_iter += step;
    assert(!*dst_iter);
    *dst_iter = *src_iter;
    ++dst_iter;
    ++src_iter;
    remaining_right >>= step + 1;
  }

  _size = new_size;
}

void tree_stack_base::truncate(size_t new_size) {
  size_t to_remove = _size - new_size;

  size_t on_right = compute_association(_size, to_remove);
  size_t on_left = to_remove - on_right;

  auto my_iter = trees.begin();
  size_t right_iter = on_right;
  while (right_iter) {
    unsigned step = std::countr_zero(right_iter);
    my_iter += step;
    assert(*my_iter);
    *my_iter = nullptr;
    my_iter++;
    right_iter >>= step + 1;
  }

  if (on_left) {
    size_t to_deconstruct = std::countr_zero(_size - on_right);
    size_t to_remain = the_bit(to_deconstruct) - on_left;
    // deconstruct this tree
    const node_or_leaf *splitting = trees[to_deconstruct];
    trees[to_deconstruct] = nullptr;
    size_t bit_no = to_deconstruct - 1;
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
  trees.resize(std::bit_width(_size));
}

const node_or_leaf *const &tree_stack_base::back() const {
  size_t bit = std::countr_zero(size());
  node_or_leaf const *const *tree = &trees[bit];
  for (; bit; --bit) {
    tree = &static_cast<const node *>(*tree)->rhs;
  }
  return *tree;
}

void node::iterator::move(difference_type by) {
  if (by == 0) {
    return;
  }
  size_t oldIdx = idx;
  size_t newIdx;
  if (by < 0 && idx < (size_t)-by) {
    over = true;
    newIdx = 0;
  } else if (by > 0 && size() - idx < (size_t)by) {
    over = true;
    newIdx = size() - 1;
  } else {
    over = false;
    newIdx = idx + by;
  }
  size_t delta = newIdx ^ oldIdx;
  if (!delta) return;
  idx = newIdx;
  resolve_from(std::bit_width(delta));
}
void node::iterator::resolve_from(size_t width) {
  for (int it = width - 1; it >= 0; --it) {
    stack[it] = static_cast<const node &>(*stack[it + 1])[idx & the_bit(it)];
  }
}

/* iterator implemenations */

node::iterator node::iterator::operator-(difference_type delta) {
  iterator cp = *this;
  cp -= delta;
  return cp;
}
node::iterator &node::iterator::operator-=(difference_type delta) {
  move(-delta);
  return *this;
}
node::iterator node::iterator::operator+(difference_type delta) {
  iterator cp = *this;
  cp += delta;
  return cp;
}
node::iterator &node::iterator::operator+=(difference_type delta) {
  move(delta);
  return *this;
}
node::iterator node::iterator::operator--(int) {
  iterator cp = *this;
  ++*this;
  return cp;
}
node::iterator &node::iterator::operator--() {
  move(-1);
  return *this;
}
node::iterator node::iterator::operator++(int) {
  iterator cp = *this;
  ++*this;
  return cp;
}
node::iterator &node::iterator::operator++() {
  move(1);
  return *this;
}
node::iterator::iterator(size_t bit, const node_or_leaf *root, size_t idx)
    : bit(bit), idx(idx) {
  stack.resize(bit + 1);
  stack.shrink_to_fit();
  stack[bit] = root;
  resolve_from(bit);
}

tree_stack_base::r_iterator::r_iterator(const tree_stack_base *tree, bool end)
    : size(tree->size()), owner(tree) {
  if (size == 0) {
    bit = 0;
    over = true;
    return;
  }
  bit = end ? tree->trees.size() - 1 : std::countr_zero(size);
  nodes = {bit, owner->trees[bit], end ? 0 : the_bit(bit) - 1};
  over = end;
  if (end) {
    --nodes;
  }
}

tree_stack_base::r_iterator &tree_stack_base::r_iterator::operator++() {
  --nodes;
  if (!nodes.over) {
    return *this;
  }
  size_t remaining_size = size & ~(the_bit(bit + 1) - 1);
  if (remaining_size == 0) {
    over = true;
    return *this;
  }
  bit = std::countr_zero(remaining_size);
  nodes = {bit, owner->trees[bit], the_bit(bit) - 1};
  return *this;
}
tree_stack_base::r_iterator tree_stack_base::r_iterator::operator++(int) {
  r_iterator cp = *this;
  ++cp;
  return cp;
}
bool tree_stack_base::r_iterator::operator==(const r_iterator &o) const {
  return bit == o.bit && over == o.over && nodes == o.nodes;
}
bool tree_stack_base::r_iterator::operator!=(const r_iterator &o) const {
  return !(*this == o);
}

} // namespace suffstack
