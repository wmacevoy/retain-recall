//! The C++ `src/test.cpp` stress test, ported to the `retain` crate.
//!
//! Ten threads, eight levels of nesting, a hundred repetitions.  Sleeps of
//! varying length make the interleaving differ between runs, so a
//! byte-identical transcript every time is what proves the stacks are
//! thread-confined.  Prints the same transcript as `./build/test_retain`.

use std::cell::{Cell, RefCell};
use std::panic::{self, AssertUnwindSafe};
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use retain::retain;

retain!(static NUMBERS: usize);
retain!(static LANGUAGE: Cell<&'static str>);

fn xorshift(state: &Cell<u64>) -> u64 {
    let mut s = state.get();
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    state.set(s);
    s
}

fn seed(k: usize) -> u64 {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .subsec_nanos() as u64;
    ((nanos << 8) ^ (k as u64).wrapping_add(0x9E37_79B9_7F4A_7C15)) | 1
}

fn state(id: usize, out: &RefCell<String>, rng: &Cell<u64>) {
    {
        let mut s = out.borrow_mut();
        s.push_str(&format!("{}: ", id));
        NUMBERS.for_each(|value| s.push_str(&format!("[{}]", value)));
        if NUMBERS.retained() {
            NUMBERS.recall(|value| s.push_str(&format!("@{}", value)));
        }
        s.push('\n');
    }
    thread::sleep(Duration::from_millis(xorshift(rng) % 10));
}

fn run(id: usize, out: &RefCell<String>, rng: &Cell<u64>) {
    let x: Vec<usize> = (0..100).collect();
    state(id, out, rng);
    NUMBERS.retain(&x[id], || {
        state(id, out, rng);
        NUMBERS.retain(&x[id + 1], || state(id, out, rng));
        NUMBERS.retain_if(&x[id + 2], true, || {
            state(id, out, rng);
            NUMBERS.retain(&x[id + 3], || state(id, out, rng));
            NUMBERS.retain(&x[id + 4], || state(id, out, rng));
            state(id, out, rng);
        });
        NUMBERS.retain_if(&x[id + 5], false, || {
            state(id, out, rng);
            NUMBERS.retain(&x[id + 6], || state(id, out, rng));
            NUMBERS.retain(&x[id + 7], || state(id, out, rng));
            state(id, out, rng);
        });
    });
}

fn transcript(k: usize) -> String {
    let out = RefCell::new(String::new());
    let rng = Cell::new(seed(k));
    run(10 * k, &out, &rng);
    out.into_inner()
}

fn test() -> String {
    let handles: Vec<_> = (1..10)
        .map(|k| thread::spawn(move || transcript(k)))
        .collect();
    let mut ans = transcript(0);
    for handle in handles {
        ans.push_str(&handle.join().unwrap());
    }
    ans
}

fn open() -> Vec<usize> {
    let mut open = Vec::new();
    NUMBERS.for_each(|n| open.push(*n));
    open
}

/// The retain is gone once the body ends, returning or unwinding.
fn check_scope() {
    assert!(!NUMBERS.retained(), "empty to start");
    let outer = 1usize;
    NUMBERS.retain(&outer, || {
        NUMBERS.recall(|n| assert_eq!(*n, 1, "recall outer"));
        let inner = 2usize;
        NUMBERS.retain(&inner, || {
            NUMBERS.recall(|n| assert_eq!(*n, 2, "recall inner"));
        });
        NUMBERS.recall(|n| assert_eq!(*n, 1, "inner popped"));
    });
    assert!(!NUMBERS.retained(), "outer popped");

    let thrown = 3usize;
    let hook = panic::take_hook();
    panic::set_hook(Box::new(|_| {}));      // this panic is the point of the test
    let caught = panic::catch_unwind(AssertUnwindSafe(|| {
        NUMBERS.retain(&thrown, || panic!("boom"));
    }));
    panic::set_hook(hook);
    assert!(caught.is_err(), "the panic propagated");
    assert!(!NUMBERS.retained(), "popped while unwinding");
}

/// retain_if(v, false) is invisible to recall and to for_each.
fn check_conditional() {
    let outer = 1usize;
    NUMBERS.retain(&outer, || {
        let skipped = 2usize;
        NUMBERS.retain_if(&skipped, false, || {
            NUMBERS.recall(|n| assert_eq!(*n, 1, "skipped retain is not recalled"));
            assert_eq!(open(), vec![1], "skipped retain is not walked");
        });
        NUMBERS.recall(|n| assert_eq!(*n, 1, "leaving a skipped retain pops nothing"));
    });
}

/// for_each cascades outward, innermost first.
fn check_cascade() {
    let (a, b, c) = (1usize, 2usize, 3usize);
    NUMBERS.retain(&a, || {
        NUMBERS.retain(&b, || {
            NUMBERS.retain(&c, || {
                assert_eq!(open(), vec![3, 2, 1], "walk innermost to outermost");
                assert_eq!(NUMBERS.depth(), 3, "depth counts open retains");
            });
        });
    });
    assert_eq!(NUMBERS.depth(), 0, "all popped");
}

/// One thread's retain is invisible to another.
fn check_threads_isolated() {
    let mine = 1usize;
    NUMBERS.retain(&mine, || {
        let seen = thread::spawn(|| NUMBERS.retained()).join().unwrap();
        assert!(!seen, "a new thread starts with an empty stack");
    });
}

/// C++ rebinds through the `T*&` that recall returns.  Rust will not hand
/// out that reference, so the mutability moves into the retained value --
/// retain a Cell and set it.  This is the i18n example's move.
fn check_rebind() {
    let outer = Cell::new("english");
    LANGUAGE.retain(&outer, || {
        let inner = Cell::new("english");
        LANGUAGE.retain(&inner, || {
            LANGUAGE.recall(|language| language.set("spanish"));
            LANGUAGE.recall(|language| assert_eq!(language.get(), "spanish", "rebound"));
        });
        LANGUAGE.recall(|language| {
            assert_eq!(language.get(), "english", "enclosing retain untouched")
        });
    });
}

fn main() {
    check_scope();
    check_conditional();
    check_cascade();
    check_threads_isolated();
    check_rebind();
    println!("thread_local stack (retain.rs)");

    let ans = test();
    print!("{}", ans);

    for _ in 0..100 {
        assert!(test() == ans, "output differed between runs");
    }
    println!("ok: 100 runs identical");
}
