#pragma once

#include <functional>
#include <iterator>
#include <limits>
#include <set>
#include <type_traits>
#include <vector>

namespace sufftree {

  struct node_or_leaf {};

  struct leaf_base : public node_or_leaf {};

  /** a leaf of a node tree, put data here */
  template <typename T>
  struct leaf : public leaf_base {
    T value;

    leaf(T &&value): value(std::forward<T>(value)) {}
  };

  constexpr size_t bit_width(size_t itree_size) {
    return std::numeric_limits<unsigned>::digits - __builtin_clz(itree_size);
  }
  constexpr size_t the_bit(size_t bit) {
    return (size_t) 1 << bit;
  }

  /** a single node of a suffix tree */
  struct node : public node_or_leaf {
    const node_or_leaf *lhs, *rhs;

    node(const node_or_leaf *lhs, const node_or_leaf *rhs);

    const node_or_leaf *const &operator[](bool bit) const {
      return bit ? rhs : lhs;
    }

    bool operator<(const node &o) const {
      std::less<> lt;
      if (lt(lhs, o.lhs)) return true;
      if (lhs == o.lhs) return lt(rhs, o.rhs);
      return false;
    }

    struct iterator {
      size_t bit, idx;
      std::vector<const node_or_leaf *> stack;
      bool over = false;

      iterator(): iterator(0, nullptr) {}
      iterator(size_t bit,
               const node_or_leaf *root,
               size_t idx = 0)
        : bit(bit), idx(idx) {
        stack.resize(bit + 1);
        stack.shrink_to_fit();
        stack[bit] = root;
        resolve_from(bit);
      }

      using difference_type = ptrdiff_t;
      using value_type = const node_or_leaf *;
      using pointer = value_type const *;
      using reference = value_type const &;
      using iterator_category = std::bidirectional_iterator_tag;

    private:
      size_t size() const {
        return the_bit(1);
      }

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

      difference_type operator-(const iterator &o) const {
        return idx - o.idx;
      }

      bool operator==(const iterator &o) const {
        return idx == o.idx && over == o.over;
      }
      bool operator!=(const iterator &o) const {
        return !(*this == o);
      }

      const value_type &operator*() const {
        return stack.front();
      }

      const value_type *operator->() const {
        return &**this;
      }
    };
    // static_assert(std::bidirectional_iterator<iterator>, "must be a bidirectional iterator");
  };

  /** manager of a set of suffix trees, holds interned nodes */
  struct forest {
    std::set<node> nodes;

    template <typename ...Args>
    const node *intern(Args &&...args) {
      return &*nodes.emplace(std::forward<Args>(args)...).first;
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

    indexed_string(forest &f, const leaves &leaves);

    indexed_string(const leaf_base *leaf)
      : assocs{{{}, {leaf}}, {{leaf}, {}}} {}

    size_t size() const {
      return assocs.size() - 1;
    }

    bool empty() const {
      return size() == 0;
    }

    const split &association(size_t on_right) const {
      return assocs.rbegin()[on_right];
    }
  };

  constexpr size_t top_mask(size_t itree_size) {
    return the_bit(bit_width(itree_size)) - 1;
  }

  constexpr size_t compute_association(size_t tree_size, size_t itree_size) {
    size_t mask = top_mask(itree_size);
    size_t masked_size = tree_size & mask;
    if (masked_size <= itree_size) {
      return masked_size;
    } else {
      return tree_size & (mask >> 1);
    }
  }

  /** a suffix tree, supports O(log n) append, truncate, and suffix comparison */
  struct tree {
    forest &owner;
    using nodes = indexed_string::nodes;
    // smallest tree first
    nodes trees;
    size_t _size = 0;

    using string = indexed_string;

    tree(forest &owner);

    bool has_suffix(const indexed_string &itree) const;

    void append(const indexed_string &itree);

    void truncate(size_t size);

    void pop(size_t count) {
      if (count > _size) {
        count = _size;
      }
      truncate(_size - count);
    }

    size_t size() const {
      return _size;
    }

    bool empty() const {
      return size() == 0;
    }

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
      r_iterator(const tree *tree)
        : size(tree->size())
        , bit(__builtin_ctz(size))
        , owner(tree)
        , nodes(bit, owner->trees[bit], the_bit(bit) - 1)
        , over(false)
      {}
      r_iterator(const tree *tree, bool /* reverse */)
        : size(tree->size())
        , bit(tree->trees.size() - 1)
        , owner(tree)
        , nodes(bit, owner->trees[bit], 0)
        , over(true)
      {
        --nodes;
      }

      r_iterator &operator++() {
        --nodes;
        if (!nodes.over) {
          return *this;
        }
        size_t remaining_size = size & ~(the_bit(bit + 1) - 1);
        if (remaining_size == 0) {
          over = true;
          return *this;
        }
        bit = __builtin_ctz(remaining_size);
        nodes = {bit, owner->trees[bit], the_bit(bit) - 1};
        return *this;
      }
      r_iterator operator++(int) {
        r_iterator cp = *this;
        ++cp;
        return cp;
      }

      bool operator==(const r_iterator &o) const {
        return bit == o.bit && over == o.over && nodes == o.nodes;
      }
      bool operator!=(const r_iterator &o) const {
        return !(*this == o);
      }

      const node_or_leaf *const &operator*() const {
        return *nodes;
      }

      const node_or_leaf *const *operator->() const {
        return &*nodes;
      }
    };
    // static_assert(std::input_iterator<r_iterator>, "must be an input iterator");

    using reverse_iterator = r_iterator;

    reverse_iterator rbegin() const {
      return r_iterator(this);
    }

    reverse_iterator rend() const {
      return r_iterator(this, true);
    }

    const node_or_leaf * const &back() const {
      size_t bit = __builtin_ctz(size());
      node_or_leaf const * const *tree = &trees[bit];
      for (; bit; --bit) {
        tree = &static_cast<const node *>(*tree)->rhs;
      }
      return *tree;
    }
  };

}
