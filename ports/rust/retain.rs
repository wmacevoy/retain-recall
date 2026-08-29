//! retain/recall over a thread-local stack -- the Rust analogue of
//! `retain<T>` in `include/retain.hpp`.
//!
//! The C++ design cannot be transliterated.  There, `recall<T>()` hands
//! back a `T*` that outlives the call and that nothing checks; here the
//! borrow checker asks how long that reference is good for, and the honest
//! answer -- "until the retain that published it goes out of scope, which
//! is a dynamic property" -- is not one lifetimes can express.
//!
//! So the scope becomes a closure.  `retain` owns the region in which the
//! value is published, and `recall` hands the reference to a callback
//! rather than returning it:
//!
//! ```
//! # use retain::retain;
//! struct Facts { sky: &'static str }
//! retain!(static FACTS: Facts);
//!
//! let facts = Facts { sky: "pink" };
//! FACTS.retain(&facts, || {
//!     FACTS.recall(|f| assert_eq!(f.sky, "pink"));
//! });
//! ```
//!
//! The callback is higher-ranked, so the reference cannot escape it, and
//! the frame is a local of `retain` -- alive for exactly as long as the
//! body runs.  That is what makes the raw pointers below sound.  A panic
//! unwinding through the body pops the stack on the way out, which is the
//! destructor's job in C++.
//!
//! Statics declared inside a generic function are shared across every
//! instantiation rather than monomorphized, so Rust cannot synthesize
//! `retain<T>::s_current` per type the way a C++ template does.  The
//! [`retain!`] macro writes out the per-type `thread_local!` instead --
//! the same hand-written stand-in the Java port declares as a field.

use std::cell::Cell;
use std::thread::LocalKey;

/// One link in the calling thread's stack.
///
/// Public only because [`retain!`] names it; there is nothing to do with
/// one directly.
#[doc(hidden)]
pub struct Frame<T: 'static> {
    value: *const T,
    previous: *const Frame<T>,
}

/// A per-type stack of retained references, local to the calling thread.
///
/// Declare one with [`retain!`], not with [`Retain::new`].
pub struct Retain<T: 'static> {
    head: &'static LocalKey<Cell<*const Frame<T>>>,
}

// SAFETY: the only state is a reference to a `LocalKey`, whose contents
// are reachable exclusively from the thread that owns them.  Nothing in
// `Retain` is shared between threads.
unsafe impl<T: 'static> Sync for Retain<T> {}

/// Declare a retain point for one type.
///
/// ```
/// # use retain::retain;
/// struct Pool;
/// retain!(static POOL: Pool);
/// retain!(pub static SHARED_POOL: Pool);
/// ```
#[macro_export]
macro_rules! retain {
    ($(#[$attr:meta])* $vis:vis static $name:ident : $ty:ty $(;)?) => {
        $(#[$attr])*
        $vis static $name: $crate::Retain<$ty> = {
            ::std::thread_local! {
                static HEAD: ::std::cell::Cell<*const $crate::Frame<$ty>> =
                    const { ::std::cell::Cell::new(::std::ptr::null()) };
            }
            $crate::Retain::new(&HEAD)
        };
    };
}

/// Restores the stack head however the body leaves -- return or unwind.
struct Pop<T: 'static> {
    head: &'static LocalKey<Cell<*const Frame<T>>>,
    previous: *const Frame<T>,
    active: bool,
}

impl<T: 'static> Drop for Pop<T> {
    fn drop(&mut self) {
        if self.active {
            // `try_with` because a thread-local may already be destroyed
            // if a retain is somehow still open during thread teardown
            let _ = self.head.try_with(|head| head.set(self.previous));
        }
    }
}

impl<T: 'static> Retain<T> {
    #[doc(hidden)]
    pub const fn new(head: &'static LocalKey<Cell<*const Frame<T>>>) -> Self {
        Retain { head }
    }

    /// Publish `value` for the duration of `body`.
    pub fn retain<R>(&self, value: &T, body: impl FnOnce() -> R) -> R {
        self.retain_if(value, true, body)
    }

    /// Publish `value` only if `use_`, without changing the shape of the
    /// calling code -- the second constructor argument of C++ `retain<T>`.
    ///
    /// When `use_` is false nothing is pushed, so the value is invisible
    /// to [`recall`](Self::recall) and to [`for_each`](Self::for_each),
    /// and the retains this one encloses are what deeper frames see.
    pub fn retain_if<R>(&self, value: &T, use_: bool, body: impl FnOnce() -> R) -> R {
        let previous = self.head.with(|head| head.get());
        let frame = Frame { value: value as *const T, previous };
        if use_ {
            self.head.with(|head| head.set(&frame as *const Frame<T>));
        }
        // dropped before `frame`, so the head is restored while the frame
        // it points at is still alive
        let _pop = Pop { head: self.head, previous, active: use_ };
        body()
    }

    /// Is anything retained for this type on this thread?
    pub fn retained(&self) -> bool {
        self.head.with(|head| !head.get().is_null())
    }

    /// Hand the innermost retained value to `body`.
    ///
    /// Panics if nothing is retained; guard with [`retained`](Self::retained).
    pub fn recall<R>(&self, body: impl FnOnce(&T) -> R) -> R {
        let head = self.head.with(|head| head.get());
        assert!(!head.is_null(), "nothing retained for this type");
        // SAFETY: `head` points at a `Frame` local to a `retain_if` call
        // still on this thread's stack -- `Pop` clears the head before
        // that frame dies -- and its `value` points at a `&T` that
        // outlives the same call.  `body` is higher-ranked over the
        // reference's lifetime, so it cannot outlive this statement.
        let value = unsafe { &*(*head).value };
        body(value)
    }

    /// Walk the stack from innermost to outermost.
    ///
    /// The cascading patterns -- nested logging, profile accumulation,
    /// checkpoint parents -- are written with this.
    pub fn for_each(&self, mut body: impl FnMut(&T)) {
        let mut at = self.head.with(|head| head.get());
        while !at.is_null() {
            // SAFETY: as in `recall`; `previous` links stay valid for as
            // long as the retains that published them are on the stack.
            let frame = unsafe { &*at };
            body(unsafe { &*frame.value });
            at = frame.previous;
        }
    }

    /// How many retains are open for this type on this thread.
    pub fn depth(&self) -> usize {
        let mut n = 0;
        self.for_each(|_| n += 1);
        n
    }
}
