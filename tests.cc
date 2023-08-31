#include "suffstack.hpp"

#include <chrono>
#include <iostream>
#include <limits>
#include <map>
#include <random>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

/**
 * Benchmarks and tests for the suffix stack implementation.
 *
 * Tip: on Linux, use `column -s=$'\t' -t -R 1,2` to view the output table
 */
using namespace suffstack;

struct config {
  /** don't log integer configurations as they are parsed */
  bool no_log_config = std::getenv("NO_LOG_CONFIG");
  /** print each operation performed */
  bool print_ops = std::getenv("PRINT_OPS");
  /** print the vectors at each step */
  bool print_vecs = std::getenv("PRINT_VECS");
  /** the maximum number of elements to push */
  unsigned long max_push = integer("MAX_PUSH", 1024);
  /** pop operations divide a random number [0, stack.size()) by this
      to get the count to pop */
  unsigned long pop_ratio = integer("POP_RATIO", 2);
  /** number of random tests to run */
  unsigned long random_count = integer("RANDOM_COUNT", 1 << 10);
  /** seed for the random number generator */
  unsigned long seed = integer("RANDOM_SEED", 0);

  unsigned long integer(const char *env, unsigned long dflt) {
    char *value = std::getenv(env);
    if (!value) return dflt;
    unsigned long i = std::stoul(value);
    if (!no_log_config) {
      std::cout << env << "=" << i << "\n";
    }
    return i;
  }
};

static config cfg;

static void escape(void *p) {
#if defined __GNUC__
  asm volatile("" : : "g"(p) : "memory");
#else
  (void)p;
#endif
}

static void clobber() {
#if defined __GNUC__
  asm volatile("" ::: "memory");
#endif
}

struct cumulative_timer {
  using clock_t = std::chrono::steady_clock;

  struct data {
    clock_t::duration duration{0};
    size_t count = 0;
  };

  clock_t clock;
  clock_t::duration last_time;
  std::map<std::string, data> totals;

  void start() { last_time = clock.now().time_since_epoch(); }
  void finish(const std::string &tag) {
    auto time = clock.now().time_since_epoch();
    data &total = totals[tag];
    total.duration += time - last_time;
    ++total.count;
  }

  template <typename F> auto time(const std::string &tag, F &&f) {
    clobber();
    start();
    if constexpr (std::is_same_v<void, decltype(f())>) {
      f();
      finish(tag);
    } else {
      auto ret = f();
      escape(&ret);
      finish(tag);
      return ret;
    }
  }

  friend std::ostream &
  operator<<(std::ostream &os, const cumulative_timer &tm) {
    os << "\nOperation\tTime\tCount";
    for (const auto &e : tm.totals) {
      os << "\n"
         << e.first << "\t" << e.second.duration << "\t" << e.second.count;
    }
    return os;
  }
};

template <typename V>
void print_vector(std::ostream &os, const std::vector<V> &v) {
  if (v.empty()) {
    os << "{}";
    return;
  }
  os << "{";
  for (auto it = v.begin();;) {
    os << *it;
    if (++it == v.end()) {
      os << "}\n";
      break;
    } else {
      os << ", ";
    }
  }
}

template <typename Stack, typename... PrefixArgs> struct tester {
  using stack = Stack;
  using string = typename stack::string_type;

  std::tuple<PrefixArgs &...> args;
  tester(PrefixArgs &...args) : args(args...) {}

  void run() { call_helper(&tester::run_with); }

  void randomised(unsigned long seed, unsigned long op_count) {
    call_helper(&tester::randomised_with, seed, op_count);
  }

  template <typename F, typename... Args>
  void call_helper(F f, Args &&...pargs) {
    call_helper_0(
        f,
        std::make_index_sequence<sizeof...(PrefixArgs)>(),
        std::forward<Args>(pargs)...
    );
  }
  template <typename F, size_t... Idx, typename... Args>
  void call_helper_0(F f, std::index_sequence<Idx...>, Args &&...pargs) {
    (this->*f)(std::forward<Args>(pargs)..., std::get<Idx>(args)...);
  }

  void run_with(PrefixArgs &...args) {
    string str(args..., {0, 0, 1, 1, 2});
    string ostr1(args..., {0, 0, 1});
    string ostr2(args..., {1, 2});
    stack stk(args...);

    stk.append(str);
    // 0 0 1 1 2
    assert(stk.size() == 5);
    assert(stk.has_suffix(str));

    stk.append(str);
    // 0 0 1 1 2 0 0 1 1 2
    assert(stk.size() == 10);
    assert(stk.has_suffix(str));

    stk.pop(str.size());
    // 0 0 1 1 2
    assert(stk.size() == 5);
    assert(stk.has_suffix(str));
    assert(stk.has_suffix(ostr2));

    stk.pop(ostr2.size());
    // 0 0 1
    assert(stk.size() == 3);
    assert(stk.has_suffix(ostr1));

    stk.append(ostr1);
    stk.append(ostr2);
    // 0 0 1 0 0 1 1 2
    assert(stk.size() == 8);
    assert(stk.has_suffix(str));

    stk.pop(1);
    // 0 0 1 0 0 1 1
    assert(stk.size() == 7);
    assert(!stk.has_suffix(str));
    assert(!stk.has_suffix(ostr1));
    assert(!stk.has_suffix(ostr2));

    stk.pop(1);
    // 0 0 1 0 0 1
    assert(stk.size() == 6);
    assert(stk.has_suffix(ostr1));
    assert(!stk.has_suffix(str));
    assert(!stk.has_suffix(ostr2));
    assert(stk.back() == 1);

    stk.append({2});
    // 0 0 1 0 0 1 2
    assert(stk.size() == 7);
    assert(stk.has_suffix(ostr2));
    assert(!stk.has_suffix(ostr1));
    assert(!stk.has_suffix(str));
    assert(stk.has_suffix({2}));
    assert(stk.back() == 2);

    std::vector<int> stack_r(stk.rbegin(), stk.rend());
    assert(stack_r.size() == stk.size());
    std::vector<int> expected{2, 1, 0, 0, 1, 0, 0};
    assert(stack_r == expected);

    stk.truncate(0);
    assert(stk.empty());

    trunc_to_nineteen(args..., stk);
  }

  void trunc_to_nineteen(PrefixArgs &...args, stack &stk) {
    string nineteen(
        args...,
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}
    ),
        padding(args..., std::vector<int>(176 - 19, 1));
    stk.append(nineteen);
    stk.append(padding);
    stk.truncate(19);
    assert(stk.has_suffix(nineteen));
  }

  void randomised_with(
      unsigned long seed,
      unsigned long op_count,
      PrefixArgs &...args
  ) {
    std::mt19937 rng(seed);
    auto rand_int = [&](unsigned i) {
      return std::uniform_int_distribution<unsigned>(0, i)(rng);
    };

    cumulative_timer baseline_clk, impl_clk;
    constexpr const char *tag_trunc = "truncate", *tag_check = "has_suffix",
                         *tag_append = "append", *tag_index = "index";

    double total_height = 0;

    naive_stack<int> baseline;
    stack stk(args...);
    for (unsigned idx = 0; idx < op_count; ++idx) {
      unsigned op = rand_int(3);

      switch (op) {
      case 0: {
        if (baseline.size() != 0) {
          size_t count = rand_int(baseline.size()) / cfg.pop_ratio;
          if (cfg.print_ops) {
            std::cout << "Popping p=" << count << "\n";
          }
          baseline_clk.time(tag_trunc, [&]() { baseline.pop(count); });
          impl_clk.time(tag_trunc, [&]() { stk.pop(count); });
          break;
        }
      }
        // fall through
      case 2: {
        if (baseline.size() != 0) {
          size_t count = rand_int(baseline.size());
          std::vector<int> to_check(
              baseline.values.end() - count, baseline.values.end()
          );
          assert(to_check.size() == count);
          if (cfg.print_ops) {
            std::cout << "Checking suffix p=" << count << "\n";
            if (cfg.print_vecs) {
              std::cout << " v = ";
              print_vector(std::cout, to_check);
            }
          }

          string indexed = impl_clk.time(tag_index, [&]() {
            return string(args..., to_check);
          });
          bool base_correct = baseline_clk.time(tag_check, [&]() {
            return baseline.has_suffix(to_check);
          });
          assert(base_correct);
          bool correct = impl_clk.time(tag_check, [&]() {
            return stk.has_suffix(indexed);
          });
          if (!correct) {
            std::cout << "Failed, incorrect suffix\n";
            if (cfg.print_vecs) {
              std::cout << " Expected: ";
              print_vector(std::cout, baseline.values);
              std::cout << "   Actual: ";
              print_vector<int>(std::cout, stk);
            }
            std::exit(1);
          }
          break;
        }
      }
        // fall through
      case 1: {
        size_t count = rand_int(cfg.max_push);
        if (cfg.print_ops) {
          std::cout << "Appending p=" << count << "\n";
        }
        std::vector<int> to_push;
        to_push.reserve(count);
        for (size_t i = 0; i < count; ++i) {
          to_push.push_back((int)rand_int(128));
        }
        string indexed = impl_clk.time(tag_index, [&]() {
          return string(args..., to_push);
        });
        baseline_clk.time(tag_append, [&]() { baseline.append(to_push); });
        impl_clk.time(tag_append, [&]() { stk.append(indexed); });
        break;
      }
      }

      if (cfg.print_ops) {
        std::cout << "Checking length n=" << baseline.size() << "\n";
      }
      assert(baseline.size() == stk.size());
      if (cfg.print_vecs) {
        std::cout << " Expected: ";
        print_vector(std::cout, baseline.values);
        std::cout << "   Actual: ";
        print_vector<int>(std::cout, stk);
      }

      total_height += baseline.size();
    }

    std::cout << "=========\t=========\t======\n";
    std::cout << "Average height:\t" << total_height / op_count << "\n";
    std::cout << "=========\t=========\t======\n";
    std::cout << "Baseline:\n---" << baseline_clk << "\n";
    std::cout << "=========\t=========\t======\n";
    std::cout << "Benchmarked:\n---" << impl_clk << "\n\n";
  }
};

int main() {
  tester<naive_stack<int>> naive_stack_test;
  naive_stack_test.run();

  node_arena arena;
  tester<tree_stack<int>, node_arena> tree_stack_test(arena);
  tree_stack_test.run();
  tree_stack_test.randomised(cfg.seed, cfg.random_count);
}
