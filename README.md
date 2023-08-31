# Suffix Stack data structure

The suffix stack is a stack datastructure supporting fast (logarithmic time)
`append(s : S)`, `has_suffix(s : S) -> bool`, and `truncate(to : int)` over a
set of strings `S`, each indexed (in pseudo-linear (`O(N log N)`) time and
space) before use. See comments in the [source code](./suffstack.hpp) for
details.

The motivation behind this datastructure is linear time Wasm
[Validation](https://webassembly.github.io/spec/core/valid/index.html) in the
face of the standardised [multi-value
proposal](https://webassembly.github.io/spec/core/appendix/changes.html#multiple-values),
which has that an arbitrary number of values on the stack may need to be
verified per instruction:

```wat
(module
  (type $f
    (param i32 i32 (; ... x N ;) i32)
    (result i32 i32 (; ... x N ;) i32))
  (func $g (import "a" "b") (type $f))
  (func
    (i32.const 0) (i32.const 0) (; ... x N ;) (i32.const 0)
    (call $g) ;; each call must verify that the correct types are on the stack
    (call $g)
    (; ... x M ;)
    (call $g)))
```

# Compiling

Builds with CMake and C++20:

```sh
$ cd /path/to/sources
$ cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
$ cmake --build build
$ build/tests
```

# License

This code is public domain. See [LICENSE](LICENSE).
