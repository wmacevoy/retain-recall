package retained;

import static retained.Static.*;

public class Test extends Thread {

    static Integer[] x=new Integer[100];
    static {
	for (int i=0; i<100; ++i) x[i]=new Integer(i);
    }

    int id;
    StringBuffer out = new StringBuffer();

    Test(int _id) { id = _id; }

    void state()
    {
	out.append(id);
	out.append(": ");
	RecallIterator i=new RecallIterator(Integer.class); 
	while (i.hasNext()) {
	    out.append("[");
	    Object obj=i.next();
	    if (obj != null) {
		out.append(obj.toString());
	    } else {
		out.append("null");
	    }
	    out.append("]");
	}
	if (retained(Integer.class)) { 
	    Object obj=recall(Integer.class);
	    if (obj != null) {
		out.append(" ");
		out.append(obj.toString());
	    } else {
		out.append(" ");
		out.append("null");
	    }
	}
	out.append("\n");
	try {
	    Thread.sleep((int)(Math.random()*10));
	} catch (InterruptedException ex) { }
    }
  
    public void run()
    { state();
	try { retain(x[id+0]); state();
	    try { retain(new Double(Math.random()));
                try { retain(x[id+1]); state(); } finally { forget(x[id+1]); }
		try { retain_if(x[id+2],true); state(); 
		    try { retain(x[id+3]); state(); } finally { forget(x[id+3]); }
		    try { retain_if(new Double(Math.random()),Math.random() < 0.5);
			try { retain(x[id+4]); state(); } finally { forget(x[id+4]); }
			state();
		    } finally { forget(); }
		} finally { forget(x[id+2]); }
		try { retain_if(x[id+5],false); state(); 
		    try { retain(x[id+6]); state(); } finally { forget(x[id+6]); }
		    try { retain(x[id+7]); state(); } finally { forget(x[id+7]); }
		    state();
		} finally { forget(x[id+5]); }
	    } finally { forget(); }
	} finally { forget(x[id+0]); }
    }
    
    static String test()
    {
	int n=10;
	Test [] tests = new Test[n];

	for (int k=0; k<n; ++k) {
	    tests[k]=new Test(10*k);
	}
  
	for (int k=1; k<n; ++k) {
	    tests[k].start();
	}

	tests[0].run();

	for (int k=1; k<n; ++k) {
	    try {
		tests[k].join();
	    } catch (InterruptedException ex) {}
	}


	StringBuffer ans = new StringBuffer();
	for (int k=0; k<n; ++k) {
	    ans.append(tests[k].out);
	}

	return ans.toString();
    }

    public static void main(String[] args) {
	String ans=test();
	System.out.println(ans);

	for (int i=0; i<100; ++i) {
	    String check=test();
	    if (! ans.equals(check)) {
		throw new IllegalStateException();
	    }
	}
    }
}
