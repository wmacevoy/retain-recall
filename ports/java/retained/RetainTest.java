package retained;

import java.util.concurrent.ThreadLocalRandom;

/**
 * The C++ src/test.cpp stress test, ported to {@link Retain}: ten threads,
 * eight levels of nesting, a hundred repetitions.  Randomized sleeps make
 * the interleaving vary, so a byte-identical transcript every run is what
 * proves the stacks are thread-confined.  Also covers stack discipline,
 * both arms of retainIf, and iteration.
 */
public final class RetainTest implements Runnable
{
    static final Retain<Integer> RETAIN = new Retain<Integer>();

    static final Integer[] x = new Integer[100];
    static {
	for (int i = 0; i < 100; ++i) x[i] = Integer.valueOf(i);
    }

    final int id;
    final StringBuilder out = new StringBuilder();

    RetainTest(int id) { this.id = id; }

    void state()
    {
	out.append(id).append(": ");
	for (Integer value : RETAIN) {
	    out.append('[').append(value).append(']');
	}
	if (RETAIN.retained()) {
	    out.append('@').append(RETAIN.recall());
	}
	out.append('\n');
	try {
	    Thread.sleep(ThreadLocalRandom.current().nextInt(10));
	} catch (InterruptedException ex) {
	    Thread.currentThread().interrupt();
	}
    }

    public void run()
    {
	state();
	try (Retain.Handle<Integer> as = RETAIN.retain(x[id + 0])) { state();
	    try (Retain.Handle<Integer> a1 = RETAIN.retain(x[id + 1])) { state(); }
	    try (Retain.Handle<Integer> a2 = RETAIN.retainIf(x[id + 2], true)) { state();
		try (Retain.Handle<Integer> a3 = RETAIN.retain(x[id + 3])) { state(); }
		try (Retain.Handle<Integer> a4 = RETAIN.retain(x[id + 4])) { state(); }
		state();
	    }
	    try (Retain.Handle<Integer> a5 = RETAIN.retainIf(x[id + 5], false)) { state();
		try (Retain.Handle<Integer> a6 = RETAIN.retain(x[id + 6])) { state(); }
		try (Retain.Handle<Integer> a7 = RETAIN.retain(x[id + 7])) { state(); }
		state();
	    }
	}
    }

    static String test() throws InterruptedException
    {
	int n = 10;
	RetainTest[] tests = new RetainTest[n];
	for (int k = 0; k < n; ++k) tests[k] = new RetainTest(10 * k);

	Thread[] threads = new Thread[n];
	for (int k = 1; k < n; ++k) {
	    threads[k] = new Thread(tests[k]);
	    threads[k].start();
	}
	tests[0].run();
	for (int k = 1; k < n; ++k) threads[k].join();

	StringBuilder ans = new StringBuilder();
	for (int k = 0; k < n; ++k) ans.append(tests[k].out);
	return ans.toString();
    }

    /** the retain is gone once its scope ends, on the normal and throwing paths */
    static void checkScope()
    {
	Retain<String> r = new Retain<String>();
	check(!r.retained(), "empty to start");
	try (Retain.Handle<String> as = r.retain("outer")) {
	    check(r.recall().equals("outer"), "recall outer");
	    try (Retain.Handle<String> in = r.retain("inner")) {
		check(r.recall().equals("inner"), "recall inner");
	    }
	    check(r.recall().equals("outer"), "inner popped");
	}
	check(!r.retained(), "outer popped");

	try {
	    try (Retain.Handle<String> as = r.retain("thrown")) {
		throw new RuntimeException("boom");
	    }
	} catch (RuntimeException expected) { }
	check(!r.retained(), "popped while unwinding");

	try {
	    r.recall();
	    check(false, "recall on empty must throw");
	} catch (IllegalStateException expected) { }
    }

    /** retainIf(v, false) is invisible to recall and to iteration */
    static void checkConditional()
    {
	Retain<String> r = new Retain<String>();
	try (Retain.Handle<String> as = r.retain("outer")) {
	    try (Retain.Handle<String> skipped = r.retainIf("skipped", false)) {
		check(r.recall().equals("outer"), "skipped retain is not recalled");
		check(!skipped.isLinked(), "skipped retain is not linked");
		int seen = 0;
		for (String s : r) { check(s.equals("outer"), "only outer is walked"); ++seen; }
		check(seen == 1, "skipped retain is not walked");
	    }
	    check(r.recall().equals("outer"), "closing a skipped retain pops nothing");
	}
    }

    /** set() rebinds one frame, as C++ does through T*& recall() */
    static void checkRebind()
    {
	Retain<String> r = new Retain<String>();
	try (Retain.Handle<String> as = r.retain("english")) {
	    try (Retain.Handle<String> in = r.retain("english")) {
		r.set("spanish");
		check(r.recall().equals("spanish"), "innermost rebound");
	    }
	    check(r.recall().equals("english"), "enclosing retain untouched");
	}
    }

    /** from(enclosing()) is the cascade the logging/checkpoint examples need */
    static void checkCascade()
    {
	Retain<String> r = new Retain<String>();
	try (Retain.Handle<String> a = r.retain("a")) {
	    try (Retain.Handle<String> b = r.retain("b")) {
		try (Retain.Handle<String> c = r.retain("c")) {
		    check(join(r, r.top()).equals("c,b,a"), "walk from innermost");
		    check(join(r, c.enclosing()).equals("b,a"), "walk from enclosing");
		    check(join(r, null).equals(""), "walk from null");
		}
	    }
	}
    }

    /** closing out of order is a bug, not something to absorb quietly */
    static void checkOutOfOrder()
    {
	Retain<String> r = new Retain<String>();
	Retain.Handle<String> outer = r.retain("outer");
	Retain.Handle<String> inner = r.retain("inner");
	try {
	    outer.close();
	    check(false, "out-of-order close must throw");
	} catch (IllegalStateException expected) { }
	inner.close();
	outer.close();
	check(!r.retained(), "stack drains once closed in order");
	outer.close();                    // idempotent
	check(!r.retained(), "second close is a no-op");
    }

    static String join(Retain<String> r, Retain.Handle<String> start)
    {
	StringBuilder sb = new StringBuilder();
	for (String s : r.from(start)) {
	    if (sb.length() > 0) sb.append(',');
	    sb.append(s);
	}
	return sb.toString();
    }

    static void check(boolean ok, String what)
    {
	if (!ok) throw new IllegalStateException("failed: " + what);
    }

    public static void main(String[] args) throws InterruptedException
    {
	checkScope();
	checkConditional();
	checkRebind();
	checkCascade();
	checkOutOfOrder();
	System.out.println("ThreadLocal stack (Retain)");

	String ans = test();
	System.out.print(ans);

	for (int i = 0; i < 100; ++i) {
	    if (!test().equals(ans)) {
		throw new IllegalStateException("output differed between runs");
	    }
	}
	System.out.println("ok: 100 runs identical");
    }
}
