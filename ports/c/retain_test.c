/*
  The C++ src/test.cpp stress test, in C.
  =======================================

  Ten threads, eight levels of nesting, a hundred repetitions.  Sleeps of
  varying length make the interleaving differ between runs, so a
  byte-identical transcript every time is what proves the stacks are
  thread-confined.  Prints the same 110 lines as ./build/test_retain.

  Because __attribute__((cleanup)) restores scope-bound lifetime, run()
  below is brace-for-brace the same shape as the C++ original -- no pop
  calls, no goto chain, no cleanup label.  check_early_return() is the case
  that shape buys: a matched push/pop pair gets it wrong every time.

  Build against any of the three storage backends; all three must produce
  this same transcript (RETAIN_SINGLE_THREADED excepted -- it has no
  per-thread stacks, so it is checked separately).
*/

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "retain.h"
#include "thread.h"

RETAIN_DECLARE(int);
RETAIN_DEFINE(int);

static int x[100];

/*
 * a worker's transcript
 *
 * Only built for the threaded backends -- RETAIN_SINGLE_THREADED has one
 * stack for the whole program, so there is no transcript to gather.
 */

#ifndef RETAIN_SINGLE_THREADED

typedef struct {
    int id;
    char out[1024];
    size_t length;
    unsigned long long seed;
} Worker;

static unsigned long long xorshift(unsigned long long *state)
{
    unsigned long long s = *state;
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    *state = s;
    return s;
}

static void append(Worker *me, const char *text)
{
    size_t n = strlen(text);
    assert(me->length + n + 1 < sizeof(me->out) && "transcript buffer too small");
    memcpy(me->out + me->length, text, n + 1);
    me->length += n;
}

static void state(Worker *me)
{
    char field[32];

    snprintf(field, sizeof(field), "%d: ", me->id);
    append(me, field);

    RETAIN_FOREACH(int, frame) {
        snprintf(field, sizeof(field), "[%d]", *frame->value);
        append(me, field);
    }
    if (RETAINED(int)) {
        snprintf(field, sizeof(field), "@%d", *RECALL(int));
        append(me, field);
    }
    append(me, "\n");

    Thread_sleep((unsigned)(xorshift(&me->seed) % 10));
}

static void run(void *argument)
{
    Worker *me = (Worker *)argument;
    int id = me->id;

    state(me);
    { RETAIN(int, &x[id + 0]); state(me);
        { RETAIN(int, &x[id + 1]); state(me); }
        { RETAIN_IF(int, &x[id + 2], 1); state(me);
            { RETAIN(int, &x[id + 3]); state(me); }
            { RETAIN(int, &x[id + 4]); state(me); }
            state(me);
        }
        { RETAIN_IF(int, &x[id + 5], 0); state(me);
            { RETAIN(int, &x[id + 6]); state(me); }
            { RETAIN(int, &x[id + 7]); state(me); }
            state(me);
        }
    }
}

static void test(char *answer, size_t capacity)
{
    Worker workers[10];
    Thread threads[10];
    int k;

    for (k = 0; k < 10; ++k) {
        workers[k].id = 10 * k;
        workers[k].out[0] = '\0';
        workers[k].length = 0;
        workers[k].seed = (unsigned long long)(k + 1) * 0x9E3779B97F4A7C15ULL;
        workers[k].seed ^= (unsigned long long)(size_t)&workers[k];
        workers[k].seed |= 1;
    }

    for (k = 1; k < 10; ++k) Thread_start(&threads[k], run, &workers[k]);
    run(&workers[0]);
    for (k = 1; k < 10; ++k) Thread_join(&threads[k]);

    answer[0] = '\0';
    for (k = 0; k < 10; ++k) {
        assert(strlen(answer) + workers[k].length + 1 < capacity);
        strcat(answer, workers[k].out);
    }
}

#endif /* !RETAIN_SINGLE_THREADED */

/*
 * focused checks
 */

/* The retain is gone once its block ends. */
static void check_scope(void)
{
    int outer = 1, inner = 2;
    assert(!RETAINED(int) && "empty to start");
    { RETAIN(int, &outer);
        assert(*RECALL(int) == 1 && "recall outer");
        { RETAIN(int, &inner);
            assert(*RECALL(int) == 2 && "recall inner");
        }
        assert(*RECALL(int) == 1 && "inner popped");
    }
    assert(!RETAINED(int) && "outer popped");
}

/*
  The case that justifies the cleanup attribute.  This function leaves its
  retain's scope through a `return` in the middle -- no pop is written, and
  none could be without a goto chain.  A matched push/pop pair leaks here.
*/
static int early_return(int *value)
{
    RETAIN(int, value);
    if (*value > 0) return 1;
    return 0;
}

static void check_early_return(void)
{
    int value = 1;
    assert(early_return(&value) == 1 && "returned from inside the retain");
    assert(!RETAINED(int) && "popped on the early return");
}

/* RETAIN_IF(..., 0) is invisible to recall and to the walk. */
static void check_conditional(void)
{
    int outer = 1, skipped = 2;
    int seen = 0;
    { RETAIN(int, &outer);
        { RETAIN_IF(int, &skipped, 0);
            assert(*RECALL(int) == 1 && "skipped retain is not recalled");
            RETAIN_FOREACH(int, frame) {
                assert(*frame->value == 1 && "only outer is walked");
                ++seen;
            }
            assert(seen == 1 && "skipped retain is not walked");
        }
        assert(*RECALL(int) == 1 && "leaving a skipped retain pops nothing");
    }
}

/* The walk cascades outward, innermost first. */
static void check_cascade(void)
{
    int a = 1, b = 2, c = 3;
    int walked[4];
    int n = 0;
    { RETAIN(int, &a);
        { RETAIN(int, &b);
            { RETAIN(int, &c);
                RETAIN_FOREACH(int, frame) walked[n++] = *frame->value;
                assert(n == 3 && walked[0] == 3 && walked[1] == 2 &&
                       walked[2] == 1 && "innermost to outermost");
                assert(RETAIN_TOP(int)->previous->value == &b && "enclosing frame");
            }
        }
    }
}

/*
  C++ rebinds through the T*& that recall() returns.  Java and Python need a
  setter for this and Rust cannot express it safely at all; C has pointers to
  pointers, so RECALL_REF is a plain int** and the assignment reads the way
  it does in C++.  This is the i18n example's move.
*/
static void check_rebind(void)
{
    int english = 1, spanish = 2;
    { RETAIN(int, &english);
        { RETAIN(int, &english);
            *RECALL_REF(int) = &spanish;
            assert(*RECALL(int) == 2 && "innermost rebound");
        }
        assert(*RECALL(int) == 1 && "enclosing retain untouched");
    }
}

#ifndef RETAIN_SINGLE_THREADED
static void observe(void *argument)
{
    *(int *)argument = RETAINED(int);
}

/* One thread's retain is invisible to another. */
static void check_threads_isolated(void)
{
    int mine = 1;
    int seen = -1;
    Thread thread;
    { RETAIN(int, &mine);
        Thread_start(&thread, observe, &seen);
        Thread_join(&thread);
    }
    assert(seen == 0 && "a new thread starts with an empty stack");
}
#endif

int main(void)
{
    static char answer[8192];
    static char check[8192];
    int i;

    for (i = 0; i < 100; ++i) x[i] = i;

    check_scope();
    check_early_return();
    check_conditional();
    check_cascade();
    check_rebind();
#ifndef RETAIN_SINGLE_THREADED
    check_threads_isolated();
#endif

    printf("%s\n", RETAIN_BACKEND);

#ifdef RETAIN_SINGLE_THREADED
    /* One stack shared by the whole program, by construction.  Running the
       threaded transcript against it would be a data race, not a test. */
    (void)answer;
    (void)check;
    printf("ok: single-threaded checks (no transcript)\n");
#else
    test(answer, sizeof(answer));
    fputs(answer, stdout);

    for (i = 0; i < 100; ++i) {
        test(check, sizeof(check));
        assert(strcmp(check, answer) == 0 && "output differed between runs");
    }
    printf("ok: 100 runs identical\n");
#endif

    return 0;
}
