package retained;

import java.util.*;

public class Static 
{
    public static class RecallIterator implements java.util.Iterator 
    {
	Class type;
	java.util.Iterator iterator;
	Object at;
	Object it;

	public RecallIterator(Class _type) 
	{
	    type=_type;
	    iterator = retains().iterator();
	    increment();
	}
	
	public final void increment() 
	{
	    for (;;) {
		if (iterator.hasNext()) {
		    it=iterator.next();
		    if (it != null && type.isAssignableFrom(it.getClass())) break;
		} else {
		    it=null;
		    break;
		}
	    }
	}

	public final boolean hasNext() 
	{
	    return it != null;
	}

	public final Object next() 
	{
	    at=it;
	    increment();
	    return at;
	}

	public final void remove() 
	{
	    throw new UnsupportedOperationException();
	}
    }

    private static class ThreadLocalLinkedList extends ThreadLocal 
    {
	public Object initialValue() 
	{
	    return new LinkedList();
	}
    }

    private static ThreadLocalLinkedList threadLocalLinkedList = new ThreadLocalLinkedList();

    private static LinkedList retains() 
    {
	return (LinkedList) threadLocalLinkedList.get();
    }

    public static void retain(Object obj) 
    { 
	retains().addFirst(obj); 
    }

    public static void retain_if(Object obj, boolean condition) 
    { 
	retains().addFirst(condition ? obj : null); 
    }

    public static Object forget() 
    {
	return retains().removeFirst(); 
    }

    public static void forget(Object object) 
    { 
	Object forgotten = retains().removeFirst();
	if (forgotten != null && object != forgotten) {
	    throw new IllegalStateException();
	}
    }

    public static boolean retained(Class type) 
    { 
	return new RecallIterator(type).hasNext(); 
    }

    public static Object recall(Class type) 
    {
	return new RecallIterator(type).next();
    }
}
