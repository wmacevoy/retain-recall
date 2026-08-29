"""The C++ src/test.cpp stress test, ported to :mod:`retain`.

Ten threads, eight levels of nesting, a hundred repetitions.  Randomized
sleeps make the interleaving vary, so a byte-identical transcript every run
is what proves the stacks are context-confined.  Prints the same transcript
as ./build/test_retain and retained.RetainTest.
"""

from __future__ import annotations

import asyncio
import io
import random
import threading

from retain import Retain

NUMBERS: Retain[int] = Retain("numbers")

x = list(range(100))


class Worker:
    def __init__(self, id: int) -> None:
        self.id = id
        self.out = io.StringIO()

    def state(self) -> None:
        self.out.write(f"{self.id}: ")
        for value in NUMBERS:
            self.out.write(f"[{value}]")
        if NUMBERS.retained():
            self.out.write(f"@{NUMBERS.recall()}")
        self.out.write("\n")
        threading.Event().wait(random.randint(0, 9) / 1000.0)

    def run(self) -> None:
        id = self.id
        self.state()
        with NUMBERS.retain(x[id + 0]):
            self.state()
            with NUMBERS.retain(x[id + 1]):
                self.state()
            with NUMBERS.retain_if(x[id + 2], True):
                self.state()
                with NUMBERS.retain(x[id + 3]):
                    self.state()
                with NUMBERS.retain(x[id + 4]):
                    self.state()
                self.state()
            with NUMBERS.retain_if(x[id + 5], False):
                self.state()
                with NUMBERS.retain(x[id + 6]):
                    self.state()
                with NUMBERS.retain(x[id + 7]):
                    self.state()
                self.state()


def test() -> str:
    n = 10
    workers = [Worker(10 * k) for k in range(n)]
    threads = [threading.Thread(target=w.run) for w in workers[1:]]
    for t in threads:
        t.start()
    workers[0].run()
    for t in threads:
        t.join()
    return "".join(w.out.getvalue() for w in workers)


def check(ok: bool, what: str) -> None:
    if not ok:
        raise AssertionError(f"failed: {what}")


def check_scope() -> None:
    """The retain is gone once its block ends, normal path or unwinding."""
    r: Retain[str] = Retain("scope")
    check(not r.retained(), "empty to start")
    with r.retain("outer"):
        check(r.recall() == "outer", "recall outer")
        with r.retain("inner"):
            check(r.recall() == "inner", "recall inner")
        check(r.recall() == "outer", "inner popped")
    check(not r.retained(), "outer popped")

    try:
        with r.retain("thrown"):
            raise RuntimeError("boom")
    except RuntimeError:
        pass
    check(not r.retained(), "popped while unwinding")

    try:
        r.recall()
        check(False, "recall on empty must raise")
    except LookupError:
        pass


def check_conditional() -> None:
    """retain_if(v, False) is invisible to recall and to iteration."""
    r: Retain[str] = Retain("conditional")
    with r.retain("outer"):
        with r.retain_if("skipped", False) as skipped:
            check(r.recall() == "outer", "skipped retain is not recalled")
            check(not skipped.linked, "skipped retain is not linked")
            check(list(r) == ["outer"], "skipped retain is not walked")
        check(r.recall() == "outer", "leaving a skipped retain pops nothing")


def check_rebind() -> None:
    """set() rebinds one frame, as C++ does through T*& recall()."""
    r: Retain[str] = Retain("rebind")
    with r.retain("english"):
        with r.retain("english"):
            r.set("spanish")
            check(r.recall() == "spanish", "innermost rebound")
        check(r.recall() == "english", "enclosing retain untouched")


def check_cascade() -> None:
    """walk(frame.previous) is the cascade the logging example needs."""
    r: Retain[str] = Retain("cascade")
    with r.retain("a"):
        with r.retain("b"):
            with r.retain("c") as c:
                check(list(r.walk(r.top())) == ["c", "b", "a"], "walk from innermost")
                check(list(r.walk(c.previous)) == ["b", "a"], "walk from enclosing")
                check(list(r.walk(None)) == [], "walk from None")


def check_threads_isolated() -> None:
    """One thread's retain is invisible to another."""
    r: Retain[str] = Retain("threads")
    seen: list[bool] = []
    with r.retain("main"):
        t = threading.Thread(target=lambda: seen.append(r.retained()))
        t.start()
        t.join()
    check(seen == [False], "a new thread starts with an empty stack")


def check_tasks_isolated() -> None:
    """asyncio tasks do not share a stack -- what threading.local cannot do."""
    r: Retain[str] = Retain("tasks")
    order: list[str] = []

    async def worker(name: str) -> None:
        with r.retain(name):
            await asyncio.sleep(0)          # let the sibling interleave
            order.append(r.recall())        # still ours after the await

    async def main() -> None:
        await asyncio.gather(worker("a"), worker("b"))
        check(not r.retained(), "nothing leaked back to the caller")

    asyncio.run(main())
    check(sorted(order) == ["a", "b"], "each task recalled its own retain")


def main() -> None:
    check_scope()
    check_conditional()
    check_rebind()
    check_cascade()
    check_threads_isolated()
    check_tasks_isolated()
    print("ContextVar stack (retain.py)")

    ans = test()
    print(ans, end="")

    for _ in range(100):
        if test() != ans:
            raise AssertionError("output differed between runs")
    print("ok: 100 runs identical")


if __name__ == "__main__":
    main()
