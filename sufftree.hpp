#pragma once

#include <bit>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace sufftree {

/** abstract interface for a stack as we need it */
template <typename StringType, typename ValueType> struct suffix_stack {
  using string_type = StringType;
  using value_type = ValueType;

  virtual bool has_suffix(const string_type &itree) const = 0;
  virtual void append(const string_type &itree) = 0;
  virtual void truncate(size_t size) = 0;
  virtual void pop(size_t count) = 0;
  virtual const value_type &back() const = 0;

  virtual size_t size() const = 0;
  virtual bool empty() const = 0;
};

/** naive implementation of the stack interface we need */
template <typename T> struct naive_stack : suffix_stack<std::vector<T>, T> {
  using string = std::vector<T>;
  string values;

  // O(n) where n = suff.size()
  bool has_suffix(const string &suff) const override {
    if (suff.size() > values.size()) return false;
    return std::equal(suff.rbegin(), suff.rend(), values.rbegin());
  }
  // O(n) amortized where n = suff.size()
  void append(const string &suff) override {
    values.reserve(values.size() + suff.size());
    std::copy(suff.begin(), suff.end(), std::back_inserter(values));
  }
  // O(1)
  void truncate(size_t count) override { values.resize(count); }
  // O(1)
  void pop(size_t count) override {
    truncate(count > size() ? 0 : size() - count);
  }
  const T &back() const override { return values.back(); }

  // O(1)
  size_t size() const override { return values.size(); }
  bool empty() const override { return size() == 0; }

  auto rbegin() const { return values.rbegin(); }
  auto rend() const { return values.rend(); }
  operator string() const { return values; }
};

/** base type for tree nodes */
struct node_or_leaf {};

/** base type for leaf nodes */
struct leaf_base : public node_or_leaf {};

/** returns a size_t with the `bit`th (from the right) bit set */
constexpr size_t the_bit(size_t bit) { return (size_t)1 << bit; }

/** a single node of a suffix tree, with two children; these should be
    obtained by interning in a `node_arena` */
struct node : public node_or_leaf {
  const node_or_leaf *lhs, *rhs;

  node(const node_or_leaf *lhs, const node_or_leaf *rhs) : lhs(lhs), rhs(rhs) {}

  bool operator<(const node &o) const;
  bool operator==(const node &o) const { return lhs == o.lhs && rhs == o.rhs; }

  const node_or_leaf *const &operator[](bool bit) const {
    return bit ? rhs : lhs;
  }

  struct iterator {
    size_t bit, idx;
    std::vector<const node_or_leaf *> stack;
    bool over = false;

    iterator() : iterator(0, nullptr) {}
    iterator(size_t bit, const node_or_leaf *root, size_t idx = 0);

    using difference_type = ptrdiff_t;
    using value_type = const node_or_leaf *;
    using pointer = value_type const *;
    using reference = value_type const &;
    using iterator_category = std::bidirectional_iterator_tag;

  private:
    size_t size() const { return the_bit(bit); }
    void move(difference_type by);
    void resolve_from(size_t width);

  public:
    iterator &operator++();
    iterator operator++(int);
    iterator &operator--();
    iterator operator--(int);
    iterator &operator+=(difference_type delta);
    iterator operator+(difference_type delta);
    iterator &operator-=(difference_type delta);
    iterator operator-(difference_type delta);

    difference_type operator-(const iterator &o) const { return idx - o.idx; }
    bool operator==(const iterator &o) const {
      return idx == o.idx && over == o.over;
    }
    bool operator!=(const iterator &o) const { return !(*this == o); }
    const value_type &operator*() const { return stack.front(); }
    const value_type *operator->() const { return &**this; }
  };
  static_assert(
      std::bidirectional_iterator<iterator>, "must be a bidirectional iterator"
  );
};
} // namespace sufftree

namespace std {
using namespace sufftree;
/** hash implementation for nodes, for interning in constant time */
template <> struct hash<node> {
  size_t operator()(const node &node) const {
    return (size_t)node.lhs * 27 + (size_t)node.rhs;
  }
};
} // namespace std

namespace sufftree {

/** an arena for holding interned nodes */
struct node_arena {
  node_arena *parent;
  std::unordered_set<node> nodes;

  node_arena(node_arena *parent = nullptr) : parent(parent) {}

  const node *intern(const node_or_leaf *lhs, const node_or_leaf *rhs) {
    node to_intern(lhs, rhs);
    if (parent) {
      auto found = parent->nodes.find(to_intern);
      if (found != parent->nodes.end()) return &*found;
    }
    return &*nodes.emplace(to_intern).first;
  }
};

/** a string indexed for use with a suffix tree */
struct indexed_string {
  using leaves = std::vector<const leaf_base *>;
  using nodes = std::vector<const node_or_leaf *>;

  struct split {
    nodes left, right;
  };
  std::vector<split> assocs;

  indexed_string() = default;
  indexed_string(node_arena &f, const leaves &leaves) {
    index_from(f, {leaves.begin(), leaves.end()});
  }
  indexed_string(const leaf_base *leaf) : assocs{{{}, {leaf}}, {{leaf}, {}}} {}

  void index_from(node_arena &f, nodes &&leaves);

  size_t size() const { return assocs.size() - 1; }
  bool empty() const { return size() == 0; }
  const split &association(size_t on_right) const {
    return assocs.rbegin()[on_right];
  }
};

template <typename T>
concept can_hide_in_pointer =
    (std::is_trivial_v<T> && sizeof(T) <= sizeof(void *) &&
     alignof(T) <= alignof(void *));

template <typename Pointee, typename T>
  requires can_hide_in_pointer<T>
const Pointee *hide_in_pointer(const T &t) {
  const Pointee *dst = nullptr;
  std::memcpy(&dst, &t, sizeof(T));
  return dst;
}
template <typename T, typename Pointee>
  requires can_hide_in_pointer<T>
const T &find_in_pointer(const Pointee *const &ptr) {
  /* this doesn't violate strict aliasing, `ptr` has dynamic type T if
     it was created with hide_in_pointer */
  return reinterpret_cast<T const &>(ptr);
}

template <typename T>
  requires can_hide_in_pointer<T>
struct indexed_string_over : indexed_string {
  indexed_string_over(node_arena &f, const std::vector<T> &leaves) {
    nodes nodes;
    nodes.reserve(leaves.size());
    for (const T &leaf : leaves) {
      nodes.push_back(hide_in_pointer<leaf_base>(leaf));
    }
    index_from(f, std::move(nodes));
  }
  indexed_string_over(const T &t)
      : indexed_string(hide_in_pointer<leaf_base>(t)) {}
};

/** returns the association required to compare a tree of size
    `tree_size` to an indexed string of length `string_size`; that is,
    this returns the largest number <= `string_size` which shares all
    its bits with `tree_size`. */
constexpr size_t compute_association(size_t tree_size, size_t string_size) {
  size_t mask = the_bit(std::bit_width(string_size)) - 1;
  ;
  size_t masked_size = tree_size & mask;
  if (masked_size <= string_size) {
    return masked_size;
  } else {
    return tree_size & (mask >> 1);
  }
}

/** a suffix tree, supports O(log n) append, truncate, and suffix comparison */
struct tree {
  using nodes = indexed_string::nodes;

private:
  node_arena &arena;
  // smallest tree first
  nodes trees;
  size_t _size = 0;

public:
  tree(node_arena &arena) : arena(arena) {}

  bool has_suffix(const indexed_string &itree) const;
  void append(const indexed_string &itree);
  void truncate(size_t size);
  void pop(size_t count) { truncate(count > _size ? 0 : _size - count); }
  const node_or_leaf *const &back() const;

  size_t size() const { return _size; }
  bool empty() const { return size() == 0; }

  friend struct r_iterator;
  struct r_iterator {
    size_t size, bit;
    const tree *owner;
    node::iterator nodes;
    bool over;

    using difference_type = ptrdiff_t;
    using value_type = const node_or_leaf *;
    using pointer = value_type const *;
    using reference = value_type const &;
    using iterator_category = std::input_iterator_tag;

    r_iterator() : size(0), bit(0), owner(nullptr) {}
    r_iterator(const tree *tree, bool reverse = false);

    r_iterator &operator++();
    r_iterator operator++(int);

    bool operator==(const r_iterator &o) const;
    bool operator!=(const r_iterator &o) const;

    const node_or_leaf *const &operator*() const { return *nodes; }
    const node_or_leaf *const *operator->() const { return &*nodes; }
  };
  // static_assert(std::input_iterator<r_iterator>, "must be an input
  // iterator");

  using reverse_iterator = r_iterator;

  reverse_iterator rbegin() const { return r_iterator(this); }
  reverse_iterator rend() const { return r_iterator(this, true); }
};

/** an explicitly typed suffix_stack implementation */
template <typename T>
  requires can_hide_in_pointer<T>
struct tree_stack : suffix_stack<indexed_string_over<T>, T>, tree {
  tree_stack(node_arena &arena) : tree(arena) {}

  // O(log(size()) + log(str.size()))
  bool has_suffix(const indexed_string_over<T> &str) const override {
    return tree::has_suffix(str);
  }
  // O(log(size()) + log(str.size()))
  void append(const indexed_string_over<T> &str) override {
    return tree::append(str);
  }
  // O(log(size()))
  void truncate(size_t size) override { tree::truncate(size); }
  // O(log(size()))
  void pop(size_t count) override { tree::pop(count); }
  // O(log(size()))
  const T &back() const override { return find_in_pointer<T>(tree::back()); }

  // O(1)
  size_t size() const override { return tree::size(); }
  bool empty() const override { return tree::empty(); }

  struct rv_iterator : r_iterator {
    rv_iterator(r_iterator &&r) : r_iterator(std::forward<r_iterator>(r)) {}

    const T &operator*() { return find_in_pointer<T>(r_iterator::operator*()); }
    const T *operator->() { return &**this; }
  };

  rv_iterator rbegin() const { return tree::rbegin(); }
  rv_iterator rend() const { return tree::rend(); }

  operator std::vector<T>() {
    std::vector<T> ret(rbegin(), rend());
    std::reverse(ret.begin(), ret.end());
    return ret;
  }
};

} // namespace sufftree
