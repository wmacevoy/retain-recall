# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and test

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

Requires C++17 (set in CMakeLists.txt). The core library is header-only (`include/retain.hpp`), so only examples and tests need compilation.

## Architecture

**retain/recall** is a single-header C++17 library implementing dynamic scoping via thread-local stacks. `retain<T>` is an automatic (stack-scoped) variable whose constructor pushes a `T*` onto a per-type, per-thread linked list, and whose destructor pops it. `recall<T>()` retrieves the most recently retained pointer from anywhere deeper in the call chain.

The entire library is `include/retain.hpp` (~75 lines). Everything else is examples and tests.

### Key design points

- `retain<T>` maintains a `static inline thread_local` singly-linked list per template instantiation — no platform-specific TLS, no mutexes.
- `recall<T>()` returns `T*&` (mutable reference), allowing the pointer itself to be reassigned (used in the i18n example).
- `retain<T>::iterator` walks the stack from newest to oldest, enabling cascading patterns (logging, profiling, checkpoint chains).
- The second constructor parameter (`bool use`) enables conditional retain without changing calling code.
- Non-copyable, non-movable — lifetime is bound to the enclosing scope.

### C/C++ bridge pattern

`src/newsort.cpp` demonstrates the most important use case: C callback APIs (like `qsort`) that have no context pointer. `extern "C"` callbacks use `recall<Sortable>()` to access a polymorphic C++ object retained by the caller. The C sort library lives in `src/sort.c` with its header at `include/sort.h`.

### Examples by pattern

- **midi1/midi2**: basic retain/recall; self-retaining class (`class T : public retain<T>`)
- **pool**: custom `operator new` finds the current pool via recall
- **graph**: operator overloading builds edges in the retained graph
- **i18n**: mutable recall reassigns the language pointer
- **logging**: iterator cascades output through nested stream retains
- **profile**: iterator accumulates counts across all retained profiles
- **checkpoint**: nested transactional rollback via iterator walking to parent checkpoints

### Test strategy

`src/test.cpp` runs 10 threads each nesting retain/recall 8 levels deep, repeated 100 times, asserting deterministic output. It validates thread isolation, stack discipline, conditional retain, and iterator correctness.

### Historical note

The pattern's correctness depends on automatic storage duration — the original meaning of the `auto` keyword before C++11 repurposed it for type inference. This is noted in the header and README.
