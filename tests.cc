#include "sufftree.hpp"

#include <iostream>
#include <cassert>

using namespace sufftree;

int main() {
  forest forest;
  const std::vector<leaf<int>> leaves {
    0,
    1,
    2,
    3,
  };
  indexed_string str(forest, {
      &leaves[0],
      &leaves[0],
      &leaves[1],
      &leaves[1],
      &leaves[2],
    });
  indexed_string ostr1(forest, {
      &leaves[0],
      &leaves[0],
      &leaves[1],
    });
  indexed_string ostr2(forest, {
      &leaves[1],
      &leaves[2],
    });

  tree the_tree(forest);

  the_tree.append(str);
  // 0 0 1 1 2
  assert(the_tree.size() == 5);
  assert(the_tree.has_suffix(str));

  the_tree.append(str);
  // 0 0 1 1 2 0 0 1 1 2
  assert(the_tree.size() == 10);
  assert(the_tree.has_suffix(str));

  the_tree.pop(str.size());
  // 0 0 1 1 2
  assert(the_tree.size() == 5);
  assert(the_tree.has_suffix(str));
  assert(the_tree.has_suffix(ostr2));

  the_tree.pop(ostr2.size());
  // 0 0 1
  assert(the_tree.size() == 3);
  assert(the_tree.has_suffix(ostr1));

  the_tree.append(ostr1);
  the_tree.append(ostr2);
  // 0 0 1 0 0 1 1 2
  assert(the_tree.size() == 8);
  assert(the_tree.has_suffix(str));

  the_tree.pop(1);
  // 0 0 1 0 0 1 1
  assert(the_tree.size() == 7);
  assert(!the_tree.has_suffix(str));
  assert(!the_tree.has_suffix(ostr1));
  assert(!the_tree.has_suffix(ostr2));

  the_tree.pop(1);
  // 0 0 1 0 0 1
  assert(the_tree.size() == 6);
  assert(the_tree.has_suffix(ostr1));
  assert(!the_tree.has_suffix(str));
  assert(!the_tree.has_suffix(ostr2));
  assert(the_tree.back() == &leaves[1]);

  the_tree.append(&leaves[2]);
  // 0 0 1 0 0 1 2
  assert(the_tree.size() == 7);
  assert(the_tree.has_suffix(ostr2));
  assert(!the_tree.has_suffix(ostr1));
  assert(!the_tree.has_suffix(str));
  assert(the_tree.has_suffix(&leaves[2]));
  assert(the_tree.back() == &leaves[2]);

  tree::nodes stack_r(the_tree.rbegin(), the_tree.rend());
  assert(stack_r.size() == the_tree.size());
  tree::nodes expected;
  for (auto i : { 2, 1, 0, 0, 1, 0, 0 }) expected.push_back(&leaves[i]);
  assert(stack_r == expected);

  the_tree.truncate(0);
  assert(the_tree.empty());
}
