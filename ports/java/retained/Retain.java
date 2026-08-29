package retained;

import java.util.Iterator;
import java.util.NoSuchElementException;

/**
 * retain/recall over a per-type thread-local stack -- the portable Java
 * analogue of retain&lt;T&gt; in include/retain.hpp.
 *
 * <p>C++ gets a distinct {@code static inline thread_local} slot for every
 * template instantiation, so {@code retain<T>::s_current} costs nothing to
 * find.  Generic type erasure denies Java that, so the per-type static is
 * written out by hand: each retained type declares one {@code Retain}
 * instance, and that field plays the role of {@code retain<T>::s_current}.
 *
 * <pre>
 *   class Facts {
 *       static final Retain&lt;Facts&gt; RETAIN = new Retain&lt;Facts&gt;();
 *   }
 *
 *   try (Retain.Handle&lt;Facts&gt; as = Facts.RETAIN.retain(facts)) {
 *       ...                              // deeper frames call recall()
 *   }                                    // close() pops, exception or not
 * </pre>
 *
 * <p>Recall is one {@link ThreadLocal} lookup and a field read.  Unlike
 * {@link Static}, there is no list shared between types and no per-recall
 * allocation or {@code isAssignableFrom} scan.
 *
 * <p>Java has no RAII, so try-with-resources stands in for the destructor:
 * a {@link Handle} is both the stack link and the {@link AutoCloseable}
 * that unlinks it.  The C++ version tolerates out-of-order destruction
 * silently ({@code if (s_current == this)}); this one throws, since a
 * handle closed out of order can only mean an inner retain outlived its
 * scope.
 *
 * <p>Each thread has its own stack, so instances are safe to share as
 * statics.  A single stack is confined to one thread and needs no locking.
 */
public final class Retain<T> implements Iterable<T>
{
    /**
     * One link in the calling thread's stack, and the handle that unlinks
     * it.  Hold it in a try-with-resources resource variable; nothing else
     * should keep a reference past the enclosing scope.
     */
    public static final class Handle<T> implements AutoCloseable
    {
	private final Retain<T> owner;
	private final Handle<T> previous;
	private T value;
	private boolean linked;

	private Handle(Retain<T> owner, T value, Handle<T> previous, boolean use)
	{
	    this.owner = owner;
	    this.value = value;
	    this.previous = previous;
	    this.linked = use;
	}

	/** the retained value */
	public T get() { return value; }

	/** rebind this retain -- the mutable half of C++ {@code T*& recall()} */
	public void set(T value) { this.value = value; }

	/**
	 * the retain this one encloses (nearer the bottom of the stack), or
	 * null.  Walking from here is how the cascading patterns -- nested
	 * logging, profile accumulation, checkpoint parents -- are written.
	 */
	public Handle<T> enclosing() { return previous; }

	/** false if this handle was skipped by retainIf, or is already closed */
	public boolean isLinked() { return linked; }

	public void close()
	{
	    if (!linked) return;              // never pushed, or closed already
	    if (owner.head.get() != this) {
		throw new IllegalStateException(
		    "retain closed out of order: an inner retain outlived its scope");
	    }
	    linked = false;
	    if (previous != null) {
		owner.head.set(previous);
	    } else {
		owner.head.remove();          // leave no entry behind on pooled threads
	    }
	}
    }

    private final ThreadLocal<Handle<T>> head = new ThreadLocal<Handle<T>>();

    /** push value onto this thread's stack */
    public Handle<T> retain(T value)
    {
	return retainIf(value, true);
    }

    /**
     * push value only if use, without changing the shape of the calling
     * code -- the second constructor argument of C++ {@code retain<T>}.
     * When use is false the handle is never linked, so it is invisible to
     * {@link #recall()} and to iteration, and closing it does nothing.
     */
    public Handle<T> retainIf(T value, boolean use)
    {
	Handle<T> handle = new Handle<T>(this, value, head.get(), use);
	if (use) head.set(handle);
	return handle;
    }

    /** is anything retained for this type on this thread? */
    public boolean retained()
    {
	return head.get() != null;
    }

    /** the innermost retained value; guard with {@link #retained()} */
    public T recall()
    {
	return required().value;
    }

    /**
     * reassign the innermost retain, as C++ does through the reference
     * returned by {@code T*& recall()}.  Rebinds that one frame; the
     * retains it encloses are untouched.
     */
    public void set(T value)
    {
	required().value = value;
    }

    /** the innermost handle, or null */
    public Handle<T> top()
    {
	return head.get();
    }

    private Handle<T> required()
    {
	Handle<T> handle = head.get();
	if (handle == null) {
	    throw new IllegalStateException("nothing retained for this type");
	}
	return handle;
    }

    /** walk the stack from innermost to outermost */
    public Iterator<T> iterator()
    {
	return new HandleIterator<T>(head.get());
    }

    /**
     * walk the stack from start (inclusive) outward, for the cascading
     * patterns: {@code for (T t : RETAIN.from(as.enclosing()))}.  A null
     * start iterates nothing.
     */
    public Iterable<T> from(final Handle<T> start)
    {
	if (start != null && start.owner != this) {
	    throw new IllegalArgumentException("handle belongs to another Retain");
	}
	return new Iterable<T>() {
	    public Iterator<T> iterator() { return new HandleIterator<T>(start); }
	};
    }

    private static final class HandleIterator<T> implements Iterator<T>
    {
	private Handle<T> at;

	HandleIterator(Handle<T> at) { this.at = at; }

	public boolean hasNext() { return at != null; }

	public T next()
	{
	    if (at == null) throw new NoSuchElementException();
	    T value = at.value;
	    at = at.previous;
	    return value;
	}

	public void remove() { throw new UnsupportedOperationException(); }
    }
}
