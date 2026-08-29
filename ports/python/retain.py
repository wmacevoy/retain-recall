"""retain/recall over a context-local stack -- the Python analogue of
retain<T> in include/retain.hpp.

A ``Retain`` instance is the stand-in for ``retain<T>::s_current``: one per
retained concept, declared next to the type it carries.  Entering its
context manager pushes; leaving pops, on the normal path and while an
exception unwinds::

    FACTS = Retain("facts")

    with FACTS.retain(facts):
        ...                     # deeper frames call FACTS.recall()

The stack lives in a :class:`contextvars.ContextVar` rather than
:class:`threading.local`.  Threads behave the same either way -- a new
thread starts with an empty context, exactly as ``thread_local`` gives a
new thread an empty stack.  The difference is asyncio: each Task runs in
its own copy of the context, so a retain inside one coroutine is invisible
to its siblings and cannot leak across an ``await``.  With
``threading.local`` every task on the event-loop thread would share one
stack and trample each other.  That is a correctness property the C++
version has no way to express.
"""

from __future__ import annotations

import contextvars
from typing import Generic, Iterator, Optional, TypeVar

__all__ = ["Retain", "Frame"]

T = TypeVar("T")


class Frame(Generic[T]):
    """One link in the stack, and the handle the context manager yields.

    ``previous`` is the retain this one encloses, which is how the
    cascading patterns -- nested logging, profile accumulation, checkpoint
    parents -- walk outward.
    """

    __slots__ = ("value", "previous", "linked")

    def __init__(self, value: T, previous: Optional["Frame[T]"], linked: bool) -> None:
        self.value = value
        self.previous = previous
        self.linked = linked

    def __repr__(self) -> str:
        return f"Frame({self.value!r}, linked={self.linked})"


class _Scope(Generic[T]):
    """The context manager returned by :meth:`Retain.retain`."""

    __slots__ = ("_owner", "_value", "_use", "_frame", "_token")

    def __init__(self, owner: "Retain[T]", value: T, use: bool) -> None:
        self._owner = owner
        self._value = value
        self._use = use
        self._frame: Optional[Frame[T]] = None
        self._token: Optional[contextvars.Token] = None

    def __enter__(self) -> Frame[T]:
        # built here, not in __init__, so `previous` is the stack as it
        # stands at the moment the scope is actually entered
        self._frame = Frame(self._value, self._owner._var.get(), self._use)
        if self._use:
            self._token = self._owner._var.set(self._frame)
        return self._frame

    def __exit__(self, *exc_info: object) -> bool:
        if self._token is not None:
            if self._owner._var.get() is not self._frame:
                raise RuntimeError(
                    f"{self._owner!r} left out of order: "
                    "an inner retain outlived its scope"
                )
            self._owner._var.reset(self._token)
            self._token = None
        if self._frame is not None:
            self._frame.linked = False
        return False


class Retain(Generic[T]):
    """A per-concept stack of retained values, local to the current context."""

    __slots__ = ("_var",)

    def __init__(self, name: str) -> None:
        self._var: contextvars.ContextVar[Optional[Frame[T]]] = contextvars.ContextVar(
            name, default=None
        )

    def __repr__(self) -> str:
        return f"Retain({self._var.name!r})"

    def retain(self, value: T) -> _Scope[T]:
        """Push ``value`` for the duration of the ``with`` block."""
        return _Scope(self, value, True)

    def retain_if(self, value: T, use: bool) -> _Scope[T]:
        """Push only if ``use``, without changing the shape of the calling code.

        This is the second constructor argument of C++ ``retain<T>``.  When
        ``use`` is false nothing is pushed, so the frame is invisible to
        :meth:`recall` and to iteration, and leaving the block pops nothing.
        """
        return _Scope(self, value, use)

    def retained(self) -> bool:
        """Is anything retained here?"""
        return self._var.get() is not None

    def recall(self) -> T:
        """The innermost retained value; guard with :meth:`retained`."""
        return self._required().value

    def set(self, value: T) -> None:
        """Rebind the innermost retain.

        The mutable half of C++ ``T*& recall()``: it replaces the value in
        that one frame and leaves the retains it encloses alone.
        """
        self._required().value = value

    def top(self) -> Optional[Frame[T]]:
        """The innermost frame, or ``None``."""
        return self._var.get()

    def _required(self) -> Frame[T]:
        frame = self._var.get()
        if frame is None:
            raise LookupError(f"nothing retained for {self._var.name!r}")
        return frame

    def __iter__(self) -> Iterator[T]:
        """Walk the stack from innermost to outermost."""
        return self.walk(self._var.get())

    def walk(self, start: Optional[Frame[T]]) -> Iterator[T]:
        """Walk from ``start`` (inclusive) outward.

        ``retain.walk(frame.previous)`` is the cascade the logging and
        checkpoint examples need.  A ``None`` start walks nothing.
        """
        at = start
        while at is not None:
            yield at.value
            at = at.previous
