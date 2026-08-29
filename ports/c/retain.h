#pragma once

/*
  retain/recall semantics -- C11
  ==============================

  The C analogue of retain<T> in include/retain.hpp, and the one port where
  the pattern is not a curiosity: C callback APIs are the reason retain/recall
  was written.  include/sort.h declares compare() and swap() as plain globals
  the caller must define, so src/oldsort.c can only ever sort ONE array.
  src/newsort.cpp fixes that for C++ callers.  This header fixes it for C
  callers, against the same unmodified src/sort.c -- see csort.c.

  Two things C lacks, and what stands in for each:

  1. Templates.  There is no way to synthesize a stack per type, so
     RETAIN_DECLARE(T) writes one out.  Rust's retain! macro and the Java
     port's hand-written static field are the same concession in other
     clothes.

  2. Destructors.  A retain must pop when its scope ends -- including on an
     early return, which is exactly the path a hand-written pop is forgotten
     on.  __attribute__((cleanup)) is what gives C back that half of
     automatic storage duration; it is a GCC/Clang extension, and it is what
     systemd's _cleanup_ macros are built on.  Where it is unavailable
     (MSVC), call Retain_T_link/Retain_T_unlink in a matched pair and accept
     that early returns are yours to get right.

  What C keeps that no other port could
  -------------------------------------
  C++ `recall<T>()` returns `T*&` -- a reference to the stored pointer, so
  the pointer itself can be reassigned (the i18n example).  Java and Python
  need a setter for that, and Rust cannot express it safely at all.  C has
  pointers to pointers, so RECALL_REF(T) is a `T**` and the assignment reads
  almost exactly as it does in C++:

      *RECALL_REF(Language) = &SPANISH_LANGUAGE;

  Portability, and a layer this repo already threw away once
  ----------------------------------------------------------
  Before commit 441b58c ("Modernize to C++17 and CMake") this project
  carried retain_thread_local_storage<T> in include/retain.hpp -- three
  backends behind one interface: TlsAlloc/TlsGetValue on Win32,
  pthread_key_create/pthread_getspecific elsewhere, and a plain static under
  RETAIN_SINGLE_THREADED.  It also carried include/thread.h, a Thread class
  over CreateThread and pthread_create.  C++11 made both dead weight:
  `thread_local` and <thread> are portable, so the layer was deleted.

  C never got that reprieve, so the layer comes back here.  C11 gives a
  usable `_Thread_local`, but <threads.h> is optional and macOS still does
  not ship it, so a test that runs on win/mac/linux needs thread.h again.
  And a storage-class TLS is not always available or wanted: freestanding
  and wasm builds have no threads at all, and __declspec(thread) historically
  broke in DLLs loaded with LoadLibrary, which is why TlsAlloc existed.

  Three backends, selected at compile time:

      (default)               _Thread_local / __declspec(thread) / __thread
      -DRETAIN_TLS_KEYS       pthread_key_create / TlsAlloc -- the 2014 layer
      -DRETAIN_SINGLE_THREADED  a plain static; no TLS at all

  Usage
  -----
      RETAIN_DECLARE(Sortable)          // in a header
      RETAIN_DEFINE(Sortable)           // in exactly one .c

      void sort_sortable(Sortable *s) {
          RETAIN(Sortable, s);          // pops when this scope ends
          sort(0, Sortable_size(s));
      }

      int compare(size_t i, size_t j) {
          return Sortable_compare(RECALL(Sortable), i, j);
      }
*/

#include <assert.h>
#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
#  define RETAIN_HAVE_CLEANUP 1
#  define RETAIN_UNUSED __attribute__((unused))
#else
#  define RETAIN_UNUSED
#endif

/*
 * thread-local storage backend
 *
 * Each thread gets its own stack, so nothing here needs a lock -- the same
 * bargain retain.hpp makes.  Only the head pointer is per-thread; frames
 * are automatic variables in the retaining thread's own stack.
 */

#if defined(RETAIN_SINGLE_THREADED)

#  define RETAIN_BACKEND "static, no TLS (RETAIN_SINGLE_THREADED)"

/* No TLS at all.  For freestanding, embedded and wasm builds, where there
   is one thread by construction and a thread-local would only cost. */
#  define RETAIN_STORAGE_DECLARE(T)                                            \
       extern Retain_##T##_Frame *Retain_##T##_storage;
#  define RETAIN_STORAGE_DEFINE(T)                                             \
       Retain_##T##_Frame *Retain_##T##_storage = NULL
#  define RETAIN_STORAGE_GET(T) (Retain_##T##_storage)
#  define RETAIN_STORAGE_SET(T, v) ((void)(Retain_##T##_storage = (v)))

#elif defined(RETAIN_TLS_KEYS)

#  define RETAIN_BACKEND "TLS keys (RETAIN_TLS_KEYS)"

/* The 2014 layer: a TLS *key* rather than a storage class.  Use this where
   the compiler has no __thread, or where __declspec(thread) is unreliable
   (DLLs loaded with LoadLibrary before Vista).  A head pointer is exactly
   the void* these APIs store, so the fit is exact. */

#  ifdef _WIN32
#    include <windows.h>
/* The create function is carried in the struct rather than through the
   callback's PVOID: converting a function pointer to an object pointer is
   not conforming C (and -Wpedantic says so), and a per-type creator is
   known at the point of definition anyway. */
typedef struct {
    INIT_ONCE once;
    DWORD key;
    void (*create)(void);
} Retain_Tls;
#    define RETAIN_TLS_INIT(create_fn) { INIT_ONCE_STATIC_INIT, 0, (create_fn) }

static BOOL CALLBACK Retain_tls_once_run(PINIT_ONCE once, PVOID parameter,
                                         PVOID *context)
{
    Retain_Tls *tls = (Retain_Tls *)parameter;
    (void)once;
    (void)context;
    tls->create();
    return TRUE;
}

RETAIN_UNUSED static void Retain_tls_once(Retain_Tls *tls, void (*create)(void))
{
    (void)create;   /* taken from tls->create, fixed at definition */
    InitOnceExecuteOnce(&tls->once, Retain_tls_once_run, tls, NULL);
}

RETAIN_UNUSED static void Retain_tls_create(Retain_Tls *tls)
{
    tls->key = TlsAlloc();
    assert(tls->key != TLS_OUT_OF_INDEXES && "out of TLS indexes");
}

RETAIN_UNUSED static void *Retain_tls_get(Retain_Tls *tls)
{
    return TlsGetValue(tls->key);
}

RETAIN_UNUSED static void Retain_tls_set(Retain_Tls *tls, void *value)
{
    TlsSetValue(tls->key, value);
}
#  else
#    include <pthread.h>
typedef struct {
    pthread_once_t once;
    pthread_key_t key;
} Retain_Tls;
#    define RETAIN_TLS_INIT(create_fn) { PTHREAD_ONCE_INIT, 0 }

RETAIN_UNUSED static void Retain_tls_once(Retain_Tls *tls, void (*create)(void))
{
    pthread_once(&tls->once, create);
}

RETAIN_UNUSED static void Retain_tls_create(Retain_Tls *tls)
{
    int failed = pthread_key_create(&tls->key, NULL);
    assert(failed == 0 && "pthread_key_create failed");
    (void)failed;
}

RETAIN_UNUSED static void *Retain_tls_get(Retain_Tls *tls)
{
    return pthread_getspecific(tls->key);
}

RETAIN_UNUSED static void Retain_tls_set(Retain_Tls *tls, void *value)
{
    pthread_setspecific(tls->key, value);
}
#  endif

#  define RETAIN_STORAGE_DECLARE(T)                                            \
       extern Retain_Tls Retain_##T##_storage;                                 \
       RETAIN_UNUSED static void Retain_##T##_storage_create(void)             \
       {                                                                       \
           Retain_tls_create(&Retain_##T##_storage);                           \
       }                                                                       \
       RETAIN_UNUSED static Retain_##T##_Frame *Retain_##T##_storage_get(void) \
       {                                                                       \
           Retain_tls_once(&Retain_##T##_storage,                              \
                           Retain_##T##_storage_create);                       \
           return (Retain_##T##_Frame *)                                       \
               Retain_tls_get(&Retain_##T##_storage);                          \
       }                                                                       \
       RETAIN_UNUSED static void                                               \
       Retain_##T##_storage_set(Retain_##T##_Frame *value)                     \
       {                                                                       \
           Retain_tls_once(&Retain_##T##_storage,                              \
                           Retain_##T##_storage_create);                       \
           Retain_tls_set(&Retain_##T##_storage, value);                       \
       }
#  define RETAIN_STORAGE_DEFINE(T)                                             \
       Retain_Tls Retain_##T##_storage =                                       \
           RETAIN_TLS_INIT(Retain_##T##_storage_create)
#  define RETAIN_STORAGE_GET(T) Retain_##T##_storage_get()
#  define RETAIN_STORAGE_SET(T, v) Retain_##T##_storage_set(v)

#else

#  define RETAIN_BACKEND "thread-local storage class"

/* The storage class, where the compiler has one.  This is what C++11 made
   universal and what deleted the layer above from retain.hpp. */
#  if defined(_MSC_VER)
#    define RETAIN_THREAD_LOCAL __declspec(thread)
#  elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#    define RETAIN_THREAD_LOCAL _Thread_local
#  else
#    define RETAIN_THREAD_LOCAL __thread
#  endif

#  define RETAIN_STORAGE_DECLARE(T)                                            \
       extern RETAIN_THREAD_LOCAL Retain_##T##_Frame *Retain_##T##_storage;
#  define RETAIN_STORAGE_DEFINE(T)                                             \
       RETAIN_THREAD_LOCAL Retain_##T##_Frame *Retain_##T##_storage = NULL
#  define RETAIN_STORAGE_GET(T) (Retain_##T##_storage)
#  define RETAIN_STORAGE_SET(T, v) ((void)(Retain_##T##_storage = (v)))

#endif

#define RETAIN_CAT_(a, b) a##b
#define RETAIN_CAT(a, b) RETAIN_CAT_(a, b)
#ifdef __COUNTER__
#  define RETAIN_UNIQUE(prefix) RETAIN_CAT(prefix, __COUNTER__)
#else
#  define RETAIN_UNIQUE(prefix) RETAIN_CAT(prefix, __LINE__)
#endif

/*
  Declare the stack for one type.  Put this in a header, beside the type it
  carries; put RETAIN_DEFINE(T) in exactly one translation unit.  The split
  is the same one oopc uses for its Types and vtables -- declared where
  clients can name them, defined once.

  A frame is a local of the retaining function, so retaining never allocates
  and never touches the heap.
*/
#define RETAIN_DECLARE(T)                                                      \
    typedef struct Retain_##T##_Frame Retain_##T##_Frame;                      \
    struct Retain_##T##_Frame {                                                \
        T *value;                                                              \
        Retain_##T##_Frame *previous;                                          \
        int linked;                                                            \
    };                                                                         \
                                                                               \
    RETAIN_STORAGE_DECLARE(T)                                                  \
                                                                               \
    RETAIN_UNUSED static void Retain_##T##_link(Retain_##T##_Frame *frame,     \
                                                T *value, int use)             \
    {                                                                          \
        frame->value = value;                                                  \
        frame->previous = RETAIN_STORAGE_GET(T);                               \
        frame->linked = (use != 0);                                            \
        if (frame->linked) RETAIN_STORAGE_SET(T, frame);                       \
    }                                                                          \
                                                                               \
    RETAIN_UNUSED static void Retain_##T##_unlink(Retain_##T##_Frame *frame)   \
    {                                                                          \
        if (!frame->linked) return;                                            \
        assert(RETAIN_STORAGE_GET(T) == frame &&                               \
               "retain popped out of order: an inner retain outlived it");     \
        RETAIN_STORAGE_SET(T, frame->previous);                                \
        frame->linked = 0;                                                     \
    }                                                                          \
                                                                               \
    RETAIN_UNUSED static Retain_##T##_Frame *Retain_##T##_top(void)            \
    {                                                                          \
        return RETAIN_STORAGE_GET(T);                                          \
    }                                                                          \
                                                                               \
    RETAIN_UNUSED static int Retain_##T##_retained(void)                       \
    {                                                                          \
        return RETAIN_STORAGE_GET(T) != NULL;                                  \
    }                                                                          \
                                                                               \
    RETAIN_UNUSED static T *Retain_##T##_recall(void)                          \
    {                                                                          \
        Retain_##T##_Frame *head = RETAIN_STORAGE_GET(T);                      \
        assert(head != NULL && "nothing retained for this type");              \
        return head->value;                                                    \
    }                                                                          \
                                                                               \
    RETAIN_UNUSED static T **Retain_##T##_recall_ref(void)                     \
    {                                                                          \
        Retain_##T##_Frame *head = RETAIN_STORAGE_GET(T);                      \
        assert(head != NULL && "nothing retained for this type");              \
        return &head->value;                                                   \
    }                                                                          \
                                                                               \
    struct Retain_##T##_RequireSemicolon

/* The one definition of the head slot.  Exactly one .c per program. */
#define RETAIN_DEFINE(T) RETAIN_STORAGE_DEFINE(T)

/*
  Scope-bound retain, in two spellings.
  =====================================

  RETAIN_BEGIN / RETAIN_END is the portable pair.  You write the braces:

      { RETAIN_BEGIN(Sortable, s, guard);
        ...
        RETAIN_END(guard); }

  Writing them yourself is what keeps the call site balanced at the token
  level, so indenters, clang-format, brace matching and folding all behave --
  a macro that opens a brace its partner closes defeats every one of them.
  It also puts the end of retention where you can see it, at a brace you
  wrote, rather than wherever the macro chose to put it.

  Underneath, the two arms differ and the guarantee does not.  BEGIN opens a
  block; END closes it, having first run the pop -- via the cleanup attribute
  on one arm and a __finally on the other.  Either way the frame is popped
  however the block is left: falling off the end, break, goto, or an early
  return.  That last one is the whole point -- it is the case a hand-written
  push/pop pair gets wrong, and the case a for-loop scope macro silently
  skips.

  BEGIN opening a block is what makes the pairing mandatory rather than
  advisory.  Omit the END and the brace never closes; write an END with no
  BEGIN and its label is undeclared.  Both are compile errors on every
  compiler, so a missing END cannot reach the MSVC build as a surprise.  It
  also forces statement context: neither half is usable at file scope.

  A plain brace, deliberately, and not do { } while (0).  Both force the
  pairing, but do{}while(0) also captures a break or continue meant for an
  enclosing loop -- silently, with no diagnostic, which is a far worse bug
  than the one it prevents.  check_break_reaches_enclosing_loop in
  retain_test.c pins that down.

  Because BEGIN opens a block, a variable declared between BEGIN and END is
  scoped to it.  That is not a quirk of the cleanup arm -- MSVC's __try
  scopes it the same way -- so both arms agree, and code that compiles on
  one compiles on the other.

  `label` names the frame.  It makes each END pair unambiguously with its
  BEGIN, and stops nested retains from shadowing one another.

  RETAIN / RETAIN_IF is the shorter spelling, needing no END and no label,
  and it is available only where the cleanup attribute is.  Use it when the
  code does not have to build under MSVC.
*/

#ifdef RETAIN_HAVE_CLEANUP

#  define RETAIN_BEGIN(T, value, label) RETAIN_BEGIN_IF(T, value, 1, label)

/*
  Publish only if `use`, without changing the shape of the calling code --
  the second constructor argument of C++ retain<T>.  When `use` is zero
  nothing is pushed, so the value is invisible to RECALL and to
  RETAIN_FOREACH, and deeper frames see the retain this one encloses.
*/
#  define RETAIN_BEGIN_IF(T, value, use, label)                                \
      {                                                                        \
          Retain_##T##_Frame label                                             \
              __attribute__((cleanup(Retain_##T##_unlink)));                   \
          Retain_##T##_link(&label, (value), (use))

#  define RETAIN_END(label) ((void)&(label)); }

/* Shorthand: no label, no END, scoped to the enclosing block.  Opens no
   block of its own, so it cannot enforce anything -- and cannot be given a
   portable spelling either. */
#  define RETAIN(T, value) RETAIN_IF(T, value, 1)

#  define RETAIN_IF(T, value, use)                                             \
      RETAIN_IF_(T, value, use, RETAIN_UNIQUE(retain_frame_))

#  define RETAIN_IF_(T, value, use, frame)                                     \
      Retain_##T##_Frame frame                                                 \
          __attribute__((cleanup(Retain_##T##_unlink)));                       \
      Retain_##T##_link(&frame, (value), (use))

#else

/*
  MSVC: no cleanup attribute, so SEH stands in for it.  __finally is a
  termination handler, which runs when the __try block is left by any route
  -- fall-through, return, goto, or a break or continue aimed at an enclosing
  loop.  That is the same guarantee the cleanup arm gives.

  RETAIN_END takes only a label, so it cannot name Retain_T_unlink itself.
  The frame's unlink is captured in a local beside it instead, which costs
  one pointer of stack and keeps the calling shape identical on both arms.
*/
#  define RETAIN_BEGIN(T, value, label) RETAIN_BEGIN_IF(T, value, 1, label)

#  define RETAIN_BEGIN_IF(T, value, use, label)                                \
      {                                                                        \
          Retain_##T##_Frame label;                                            \
          void (*label##_unlink)(Retain_##T##_Frame *) =                       \
              Retain_##T##_unlink;                                             \
          Retain_##T##_link(&label, (value), (use));                           \
          __try {

#  define RETAIN_END(label)                                                    \
          } __finally { label##_unlink(&label); } }

/* The short spelling has no equivalent here: there is no way to attach an
   action to the end of a scope without wrapping the scope.  Rather than
   leaving RETAIN undefined and letting the compiler say only "RETAIN is not
   a function", it expands to an identifier that names the problem. */
#  define RETAIN(T, value)                                                     \
      RETAIN_needs_cleanup_attribute__use_RETAIN_BEGIN_and_RETAIN_END_instead
#  define RETAIN_IF(T, value, use)                                             \
      RETAIN_needs_cleanup_attribute__use_RETAIN_BEGIN_and_RETAIN_END_instead

#endif /* RETAIN_HAVE_CLEANUP */

/* Is anything retained for T on this thread? */
#define RETAINED(T) Retain_##T##_retained()

/* The innermost frame, or NULL -- walk ->previous from it to cascade. */
#define RETAIN_TOP(T) Retain_##T##_top()

/* The innermost retained T*; guard with RETAINED(T). */
#define RECALL(T) Retain_##T##_recall()

/* T** to the innermost slot -- C++'s `T*& recall()`, assignable. */
#define RECALL_REF(T) Retain_##T##_recall_ref()

/*
  Walk the stack innermost to outermost.  `frame` is declared by the macro;
  the value is frame->value.  This is how the cascading patterns are written
  -- nested logging, profile accumulation, checkpoint parents.
*/
#define RETAIN_FOREACH(T, frame)                                               \
    for (Retain_##T##_Frame *frame = Retain_##T##_top(); frame != NULL;        \
         frame = frame->previous)
