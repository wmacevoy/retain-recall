# retain/recall

A C++17 header-only library implementing **retain/recall semantics** — a
pattern for accessing data in ancestor stack frames through a thread-local
stack, without passing it as explicit parameters.

## The idea

Declare a `retain<T>` as a local variable.  It pushes a pointer onto a
per-type, per-thread stack.  Any function deeper in the call chain can
`recall<T>()` to retrieve it.  When the retain goes out of scope, its
destructor pops the stack.

```cpp
#include "retain.hpp"

struct Config { int verbosity; };

void do_work() {
    Config *cfg = recall<Config>();   // retrieve from caller's frame
    if (cfg->verbosity > 1) { /* ... */ }
}

int main() {
    Config cfg{2};
    retain<Config> as(&cfg);          // publish to callees
    do_work();
}   // destructor pops the stack
```

The entire mechanism lives in a single header: [`include/retain.hpp`](include/retain.hpp).

## On the `auto` keyword

Before C++11, the `auto` storage-class specifier declared a variable with
**automatic storage duration** — allocated on entry to its enclosing scope,
destroyed on exit.  That is exactly the lifetime model that makes
retain/recall work: a `retain<T>` object *is* an automatic variable in the
original sense.  It pushes on construction, pops on destruction, and its
lifetime is governed entirely by the enclosing block.

C++11 repurposed `auto` for type inference, erasing the keyword that once
named this fundamental concept.  The stack discipline that underpins RAII —
and this library — became implicit rather than explicit.  There is no longer
a keyword that says "this variable lives on the stack."  You just have to
know.

## How it works

`retain<T>` maintains a `thread_local` singly-linked list per type `T`.
Each retain node stores a pointer to the user's data and a pointer to the
previous node.  The constructor links, the destructor unlinks.

```
   caller's frame          callee's frame
  ┌──────────────┐       ┌──────────────┐
  │ retain<T> ───┼──────>│ retain<T> ───┼──> nullptr
  │  m_as ──> obj│       │  m_as ──> obj│
  └──────────────┘       └──────────────┘
        ▲
        │
   s_current (thread_local)
```

### Key properties

- **Thread safety**: each thread has its own stack via `thread_local`.
- **Zero overhead when not used**: no allocation, no virtual dispatch.
- **Conditional retain**: pass `false` as the second argument to skip
  linking without changing calling code.
- **Iterator**: `retain<T>::begin()` / `retain<T>::end()` walk the stack
  from newest to oldest, enabling patterns like nested logging or
  profiling across retained layers.
- **Mutable recall**: `recall<T>()` returns a `T*&`, so the pointer
  itself can be reassigned (see the i18n example).

## Building

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

Requires a C++17 compiler (Clang, GCC, MSVC).

## Examples

| Example | Demonstrates |
|---------|-------------|
| [`midi1`](src/midi1.cpp) | Basic retain/recall — passing context to a function without parameters |
| [`midi2`](src/midi2.cpp) | Class inheriting from `retain<T>` — self-retaining objects |
| [`pool`](src/pool.cpp) | Object pooling — `operator new` finds the current pool via recall |
| [`graph`](src/graph.cpp) | Operator overloading — `a-b-c` builds edges in the retained graph |
| [`i18n`](src/i18n.cpp) | Implicit language context — `recall<Language>()` drives translation |
| [`logging`](src/logging.cpp) | Nested stream logging — iterator walks the retain stack to cascade output |
| [`profile`](src/profile.cpp) | Profiling — nested profiles accumulate counts through the stack |
| [`checkpoint`](src/checkpoint.cpp) | Transactional state — nested checkpoints with accept/rollback |
| [`newsort`](src/newsort.cpp) | Bridging C and C++ — retain provides polymorphic callbacks to a C sort |
| [`oldsort`](src/oldsort.c) | The original C sort with global callbacks (for comparison) |
| [`test`](src/test.cpp) | Multi-threaded stress test — 10 threads, 100 iterations |

## API reference

```cpp
#include "retain.hpp"

retain<T> as(&obj);             // push obj onto the T stack
retain<T> as(&obj, false);      // conditional: don't actually push

bool retained<T>();             // is anything retained for T?
T*& recall<T>();                // get (mutable) pointer to top of T stack

retain<T>::begin();             // iterator to top of stack
retain<T>::end();               // past-the-end sentinel
```

## Publications

- [Dr. Dobb's: Access Data Items in Ancestor Stack Frames](http://www.drdobbs.com/cpp/access-data-items-in-ancestor-stack-fram/240155450)
- [Design paper](https://docs.google.com/document/d/1st0uPvzHE5Ea9BthfJ7z8Rr2g-hUwPzDFRoYPfRokZU/edit?hl=en_US&authkey=CIaB7b0M)

## License

MIT — Copyright (c) 2014 Warren MacEvoy
