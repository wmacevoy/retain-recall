# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and test

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

Requires C++17 (set in CMakeLists.txt). The core library is header-only (`include/retain.hpp`),
so only examples and tests need compilation.

Executables land directly in `build/` (e.g. `./build/graph`), not in a `bin/` subdirectory.

Run one test — note the ctest names differ from the CMake target names
(`test_retain` is the target; `retain_test` is the test):

```sh
ctest --test-dir build -R retain_test -V     # the multithreaded stress test (~8s)
ctest --test-dir build -R newsort_test        # others: oldsort_test, i18n_test
```

Only four of the eleven programs are wired into ctest (`retain_test`, `oldsort_test`,
`newsort_test`, `i18n_test`). The rest are demonstrations verified by reading their output:
run them directly from the repo root, e.g. `./build/checkpoint`.

## Architecture

**retain/recall** is a single-header C++17 library implementing dynamic scoping via
thread-local stacks. `retain<T>` is an automatic (stack-scoped) variable whose constructor
pushes a `T*` onto a per-type, per-thread linked list, and whose destructor pops it.
`recall<T>()` retrieves the most recently retained pointer from anywhere deeper in the
call chain.

The entire library is `include/retain.hpp` (~75 lines). Everything else is examples and tests.

### Key design points

- `retain<T>` maintains a `static inline thread_local` singly-linked list per template
  instantiation — no platform-specific TLS, no mutexes, no allocation. Each `T` gets its
  own independent stack.
- `recall<T>()` returns `T*&` (mutable reference), allowing the pointer itself to be
  reassigned (used in the i18n example).
- **`recall<T>()` does not check for an empty stack** — it dereferences `s_current`
  unconditionally. Callers must guard with `retained<T>()` first (see `src/midi1.cpp`),
  or know statically that a retain is in scope.
- `retain<T>::iterator` walks the stack from newest to oldest. Constructing
  `iterator(this)` from inside a self-retaining object gives a cursor at that object,
  so `++` reaches the *enclosing* retain — the basis of the cascading patterns
  (logging, checkpoint).
- The second constructor parameter (`bool use`) enables conditional retain without
  changing calling code. When `false` the object never links, so it is invisible to both
  `recall` and the iterator — nested retains see straight through to the outer one.
- Non-copyable and (by suppression) non-movable — lifetime is bound to the enclosing
  scope. The destructor only pops when `s_current == this`, so out-of-order destruction
  silently leaves the stack alone rather than corrupting it.
- Two usage forms appear throughout: a separate local (`retain<T> as(&obj);`) and the
  self-retaining base class (`class T : public retain<T>`), which the iterator-based
  examples require.

### C/C++ bridge pattern

`src/newsort.cpp` demonstrates the motivating use case. `include/sort.h` declares
`compare()` and `swap()` as plain global functions that the caller must define — a C
callback API with no context pointer, so only **one** implementation can exist per binary.
That is why `oldsort` and `newsort` are separate executables linking the same
`src/sort.c`: `src/oldsort.c` defines them against a file-scope array, while
`src/newsort.cpp` defines them as `extern "C"` shims that `recall<Sortable>()` and
dispatch polymorphically. Retain/recall restores the context parameter the C API lacks.

### Examples by pattern

- **midi1/midi2**: basic retain/recall; self-retaining class (`class T : public retain<T>`)
- **pool**: custom `operator new` finds the current pool via recall
- **graph**: operator overloading builds edges in the retained graph
- **i18n**: mutable recall reassigns the language pointer
- **logging**: iterator cascades output through nested stream retains
- **profile**: iterator accumulates counts across all retained profiles
- **checkpoint**: nested transactional rollback via iterator walking to parent checkpoints

Expected behaviors that look like failures but are not:

- `midi2` **aborts with exit 134** by design — its destructor throws on an error code
  during normal unwind, which calls `std::terminate`. This is why it is not a ctest test.
- `logging` writes `tmp/messages.log` and `tmp/song.out` **relative to the working
  directory**. Run it from the repo root (`tmp/` exists there). Elsewhere the `ofstream`
  fails, the conditional retain drops those sinks, and output goes to stderr only — the
  example is written to degrade gracefully.

### Test strategy

`src/test.cpp` runs 10 threads each nesting retain/recall 8 levels deep, repeated 100
times, asserting the concatenated output is byte-identical every run. Randomized sleeps
inside `state()` make interleaving vary, so determinism of the *result* is what proves
thread isolation. It also covers stack discipline, conditional retain (both `true` and
`false`), and iterator correctness.

### Ports to other languages

Four ports live under `ports/`, outside the CMake build: C in `ports/c/`, Java in
`ports/java/` (the `retained/` subdirectory is its package path), Python in
`ports/python/`, Rust in `ports/rust/`. C++ is not a port -- it is home base, and stays in
`include/` and `src/`.

**The transcript is the acceptance test.** Every port runs `src/test.cpp` structurally —
ten threads, eight levels of nesting, a hundred repetitions — and prints a byte-identical
110-line transcript. Randomized sleeps vary the interleaving, so identical output across
runs *and* across languages is what proves the stacks are thread-confined. Check a port
after changing it, by stripping each program's header line, `ok:` line, and blanks:

```sh
strip() { sed '1d;/^ok: /d;/^$/d'; }
./build/test_retain | strip > /tmp/cpp.txt
(cd ports/c && make --no-print-directory transcript) | diff /tmp/cpp.txt -
python3 ports/python/retain_test.py 2>/dev/null | strip | diff /tmp/cpp.txt -
(cd ports/rust && cargo run --release --quiet --bin retain_test 2>/dev/null) | strip | diff /tmp/cpp.txt -
docker run --rm -v "$PWD:/work:ro" -w /work eclipse-temurin:21-jdk \
  sh -c 'javac -nowarn -d /tmp/classes ports/java/retained/*.java \
         && java -cp /tmp/classes retained.RetainTest' | strip | diff /tmp/cpp.txt -
```

Each port also carries focused checks (scope exit including the unwinding path, both arms
of conditional retain, the cascade walk, thread isolation) that run before the transcript.

**C — `ports/c/`.** The port with the strongest claim on the idea, because the C callback
problem is what retain/recall was written for. `include/sort.h` declares `compare`/`swap`
as globals with no context parameter, so `src/oldsort.c` can sort exactly one array
forever; `src/newsort.cpp` escapes that in C++. `ports/c/csort.c` escapes it *without
leaving C* — it sorts a double array and a string array, one nested inside the other,
through an untouched `src/sort.c`. Its object model is the oopc pattern (`../oopc`): one
`static const` vtable per class, every slot taking the root `Sortable *` so no
function-pointer cast ever arises, prefix layout, and a `Type` carrying name/parent/size
for a checked downcast.

C lacks both things retain<T> is built from, and each has a stand-in. There are no
templates, so `RETAIN_DECLARE(T)`/`RETAIN_DEFINE(T)` write out one stack per type — the
same concession as Rust's macro and Java's hand-written field. There are no destructors,
so `RETAIN(T, p)` is built on `__attribute__((cleanup))`, which is what gives C back the
scope-bound half of automatic storage duration. That is why `run()` in `retain_test.c` is
brace-for-brace the C++ original, and why `check_early_return` is the test that matters:
a hand-written push/pop pair leaks on exactly that path. On MSVC, where `cleanup` does not
exist, call `Retain_T_link`/`Retain_T_unlink` in a matched pair and own that risk.

One thing C gets that no other port does: `RECALL_REF(T)` is a `T**`, so
`*RECALL_REF(Language) = &SPANISH;` is C++'s assignable `T*& recall()` almost verbatim.
Java and Python need a setter; Rust cannot express it safely at all.

**C — the restored compatibility layer.** Commit 441b58c deleted `include/thread.h` and
`retain_thread_local_storage<T>` from `retain.hpp` — correctly, because C++11's
`thread_local` and `<thread>` made a Win32/pthread shim dead weight. C never got that
reprieve, so the layer is back in `ports/c/`, and the git history is where it came from
(`git show 441b58c^:include/thread.h`). `thread.h`/`thread.c` is the Thread shim in C,
needed because `<threads.h>` is optional in C11 and macOS does not ship it. `retain.h`
carries three storage backends, selected at compile time and all covered by `make
test-all`:

| `BACKEND=` | storage | for |
|---|---|---|
| *(empty)* | `_Thread_local` / `__declspec(thread)` / `__thread` | the normal case |
| `-DRETAIN_TLS_KEYS` | `pthread_key_create` / `TlsAlloc` | the 2014 mechanism: no TLS storage class, or `__declspec(thread)` unreliable in `LoadLibrary`ed DLLs |
| `-DRETAIN_SINGLE_THREADED` | a plain static | freestanding, embedded, wasm — no threads by construction, so no transcript either |

Verified with no warnings under `-Wall -Wextra -Wpedantic` on clang/macOS and gcc/Linux
(both threaded backends reproduce the transcript). The `c-windows` CI job runs all three
backends under MSYS2/mingw on a real Windows host, so `TlsAlloc`, `InitOnceExecuteOnce`
and `CreateThread` are executed, not merely compiled. MSVC remains unsupported for the C
port — `RETAIN` needs `__attribute__((cleanup))`, and `retain.h` says so at the use site.

**Java — `ports/java/retained/Retain.java`, `RetainTest.java`.** A per-type thread-local stack.
Each retained type declares one `static final Retain<T>` field, written by hand because
erasure cannot synthesize the per-instantiation `static` C++ gets for free. `Handle` is both
the stack link and an `AutoCloseable`, so try-with-resources stands in for the destructor.
No JDK on this machine — use the container above. That image's `/bin/sh` is dash, so keep
bashisms out of the `-c` script; `-nowarn` is for the older port's `new Integer(...)`.

**Java — `ports/java/retained/Static.java`, `Test.java`.** The original port, kept as the
historical record. One `ThreadLocal` `LinkedList` shared by every type, filtered with
`Class.isAssignableFrom` at recall, with explicit `retain()`/`forget()` in `try`/`finally`.
Slower than both the C++ version and `Retain.java` — a linear type scan and an iterator
allocation per recall, even for `retained()` — which is a known and accepted limitation of
the shared-list design, not an oversight. Leave it alone; put new Java work in `Retain.java`.

**Python — `ports/python/retain.py`.** `with FACTS.retain(facts):`, where the stack lives in
a `contextvars.ContextVar` rather than a `threading.local`. Threads behave identically either
way (a new thread starts with an empty context, as `thread_local` gives it an empty stack).
The reason for the choice is asyncio: each Task runs in its own copy of the context, so a
retain inside one coroutine is invisible to its siblings and cannot leak across an `await`.
With `threading.local`, every task on the event-loop thread would share one stack and
trample each other. `retain_test.py` asserts that isolation — it is a property the C++
version has no way to express.

**Rust — `ports/rust/retain.rs`.** The one port that could not keep the C++ shape.
`recall<T>()` returning a `T*` that outlives the call is exactly what the borrow checker
rejects, and the true lifetime — "until the retain that published it goes out of scope" — is
dynamic, so it cannot be written down. The scope therefore becomes a closure:
`FACTS.retain(&facts, || ...)` owns the region, and `recall` passes the reference to a
callback instead of returning it. The callback is higher-ranked, so the reference cannot
escape; the frame is a local of `retain`, alive exactly as long as the body; and a `Drop`
guard pops while unwinding a panic. That is the soundness argument for the raw pointers,
and it is written out in the `SAFETY` comments — preserve them if you touch that file.

Two further Rust consequences. Statics inside a generic function are shared across
instantiations rather than monomorphized, so the `retain!` macro writes out the per-type
`thread_local!` — the same hand-written stand-in Java needs. And there is no `set()`:
rebinding through the `T*&` that `recall<T>()` returns has no safe equivalent, so the
mutability moves into the retained value (retain a `Cell`, as `check_rebind` shows).

Build with `cargo` from `ports/rust/` (flat layout: `[lib] path` and `[[bin]] path` point at
the two files). `cargo test` runs the doc examples; the release profile keeps
`debug-assertions` on because the test's asserts *are* the test.

None of the ports track `include/retain.hpp` automatically. Changing the C++ semantics does
not propagate, and the transcript diff is what catches the drift.

The header comments in each port carry the reasoning behind its divergences -- Rust's
`SAFETY` blocks, C's account of `cleanup` and the restored TLS layer. Preserve them; they
are the part that cannot be recovered by reading the code.

### CI

`.github/workflows/build-test.yml`, 33 runs. Things worth knowing before editing it:

- **Debug builds are deliberate.** Every test here is an `assert`, and a Release build
  defines `NDEBUG` and compiles them all away. The Rust release profile keeps
  `debug-assertions = true` for the same reason.
- **`-Werror` applies to the C port only.** `src/midi2.cpp` still warns under
  `-Wall -Wextra` (`-Wexceptions`: its destructor throws on purpose, which is what makes
  `midi2` abort), so C++ is built without it.
- **The `c-windows` job is the valuable one.** It runs under MSYS2/mingw, so it actually
  executes the `_WIN32` paths — `TlsAlloc`, `InitOnceExecuteOnce`, `CreateThread` — rather
  than only compiling them. MSVC is deliberately absent: `RETAIN` needs
  `__attribute__((cleanup))`, and `retain.h` emits a named diagnostic there instead.
- **Backends must live in the base matrix**, not in `include`. An `include` entry that only
  adds new keys does not cross-multiply; it collapses to the last entry. The `include`
  block here keys on an existing `backend` value, which only attaches a display name.
- **`TSAN_OPTIONS: halt_on_error=1`** is required — ThreadSanitizer reports races but exits
  0 by default, so without it a race would pass CI silently.

### Repository stragglers

`bin/`, `lib/`, and `tmp/*.o` are untracked leftovers from a pre-CMake Makefile build
(the stale permissions for `make all` / `./bin/test` in `.claude/settings.local.json` come
from that era). Ignore them and build into `build/` — running `./bin/*` executes stale binaries.
