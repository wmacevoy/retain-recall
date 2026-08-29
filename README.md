# retain/recall

[![build-test](https://github.com/wmacevoy/retain-recall/actions/workflows/build-test.yml/badge.svg)](https://github.com/wmacevoy/retain-recall/actions/workflows/build-test.yml)

A C++17 header-only library implementing **retain/recall semantics** — a
pattern for accessing data in ancestor stack frames through a thread-local
stack, without passing it as explicit parameters.

The same pattern is ported to four other languages under [`ports/`](ports/),
and all five are held to one test: each prints a byte-identical 110-line
transcript, on every push.

| | library | covered by CI |
|---|---|---|
| **C++** — home base | [`include/retain.hpp`](include/retain.hpp) | Linux x64 · Linux arm64 · macOS · Windows/MSVC |
| **C** | [`ports/c/retain.h`](ports/c/retain.h) | the above · Windows/mingw — each across three thread-local storage backends |
| **Java** | [`ports/java/retained/Retain.java`](ports/java/retained/Retain.java) | JDK 17 · 21, on Linux · macOS · Windows |
| **Python** | [`ports/python/retain.py`](ports/python/retain.py) | 3.9 · 3.13, on Linux · macOS · Windows |
| **Rust** | [`ports/rust/retain.rs`](ports/rust/retain.rs) | Linux · macOS · Windows |

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

## Ports

The same pattern in four other languages, under [`ports/`](ports/). C++ is home
base; each of these is a translation, not a rewrite.

| Language | Scope bound by | Per-type stack declared by |
|----------|----------------|----------------------------|
| [C](ports/c/) | a brace, via `cleanup` or `__try`/`__finally` | `RETAIN_DECLARE(T)` |
| [Java](ports/java/) | try-with-resources | a `static final Retain<T>` field |
| [Python](ports/python/) | the `with` statement | a module-level `Retain(...)` |
| [Rust](ports/rust/) | the closure passed to `retain` | the `retain!` macro |

The last column is the same concession four times over. Only a C++ template
synthesizes a `static` per instantiation, so `retain<T>::s_current` has no
equivalent anywhere else and the per-type slot gets written out by hand.

### The transcript is the test

Every port runs [`src/test.cpp`](src/test.cpp) structurally — ten threads, eight
levels of nesting, a hundred repetitions — and prints a byte-identical 110-line
transcript. Randomized sleeps vary the interleaving, so identical output across
runs *and* across languages is what demonstrates the stacks are thread-confined.

All four run from the repository root:

```sh
make -C ports/c test
cargo run --release --manifest-path ports/rust/Cargo.toml --bin retain_test
python3 ports/python/retain_test.py
javac -d /tmp/classes ports/java/retained/*.java && java -cp /tmp/classes retained.RetainTest
```

CI runs all of this on every push — see
[`.github/workflows/build-test.yml`](.github/workflows/build-test.yml). C++ on
Linux (x64 and arm64), macOS and Windows/MSVC; the C port across all three
storage backends on Linux, macOS and Windows/mingw, warning-free under
`-Werror`; Java 17 and 21, Python 3.9 and 3.13, and Rust on all three
platforms; the cross-language transcript diff; and Thread-, Address- and
UndefinedBehaviorSanitizer over the C++ and C tests.

### What each language had to change

**C** has the strongest claim on the pattern, because the C callback problem is
what it was written for. [`ports/c/csort.c`](ports/c/csort.c) sorts a double
array and a string array — one nested inside the other — through an untouched
[`src/sort.c`](src/sort.c), which [`src/oldsort.c`](src/oldsort.c) shows can
otherwise sort exactly one array forever. C has no destructors, so `RETAIN` is
built on `__attribute__((cleanup))`, and no templates, so `RETAIN_DECLARE(T)`
writes out the stack.

Scope is spelled `{ RETAIN_BEGIN(T, p, label); ... RETAIN_END(label); }`. You
write the braces, which keeps the call site balanced for indenters and brace
matching; `RETAIN_BEGIN` opens a block of its own, which makes the `RETAIN_END`
mandatory on every compiler rather than only on the one that needs it. It lowers
to the cleanup attribute under GCC/Clang and to `__try`/`__finally` under MSVC,
and both pop on an early `return`. A plain brace rather than `do { } while (0)`,
so a `break` still reaches the enclosing loop. Where MSVC is not a concern, the
shorter `RETAIN(T, p)` needs neither label nor `END`.

C also gets something no other port does: `RECALL_REF(T)`
is a `T**`, making C++'s assignable `T*& recall()` almost verbatim. C11's
`_Thread_local` is the default, with the Win32 `TlsAlloc` / POSIX
`pthread_key_create` layer and a no-TLS single-threaded backend still selectable
— see [`ports/c/retain.h`](ports/c/retain.h).

**Java** has no RAII, so a `Handle` is both the stack link and an
`AutoCloseable`. [`Static.java`](ports/java/retained/Static.java) is the older
port, kept for comparison: one `ThreadLocal` list shared by every type and
filtered with `isAssignableFrom`, where `Retain.java` gives each type its own.

**Python** keeps the stack in a `contextvars.ContextVar` rather than a
`threading.local`. Threads behave the same either way, but each asyncio task
runs in its own copy of the context, so a retain inside one coroutine is
invisible to its siblings and survives an `await` intact — a property the C++
version has no way to express.

**Rust** could not keep the shape at all. Returning a `T*` that outlives the
call is what the borrow checker exists to reject, and the true lifetime — until
the retain that published it goes out of scope — is dynamic and cannot be
written down. So the scope becomes a closure: `FACTS.retain(&facts, || ...)`
owns the region and `recall` lends the reference to a callback instead of
returning it.

## Publications

- [Dr. Dobb's: Access Data Items in Ancestor Stack Frames](http://www.drdobbs.com/cpp/access-data-items-in-ancestor-stack-fram/240155450)
- [Design paper](https://docs.google.com/document/d/1st0uPvzHE5Ea9BthfJ7z8Rr2g-hUwPzDFRoYPfRokZU/edit?hl=en_US&authkey=CIaB7b0M)

## License

MIT — Copyright (c) 2014 Warren MacEvoy
