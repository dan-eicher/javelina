// Ch10 — JLS chapter 10, Arrays. One method per leaf section; the `// JLS <n>` marker on
// each is what the coverage join reads to fill the ledger's COVERED column, so the marker
// is load-bearing, not a comment.
//
// The behaviour asserted in each method is the one the ledger records for that section.
// Where the spec's own example is the rule (§10.5), the example is the test.

class Ch10Elem {}                     // §10.1: a concrete type for interface/abstract holders
interface Ch10Face {}
class Ch10A implements Ch10Face {}
class Ch10B implements Ch10Face {}
class Ch10Base {}
class Ch10Derived extends Ch10Base {}

public class Ch10 {

    // JLS 10.1
    static void s10_1() {
        // component type may be an interface: holds null or any implementor
        Ch10Face[] fs = new Ch10Face[3];
        Check.eq("10.1", "interface-component array defaults to null", fs[0], null);
        fs[0] = new Ch10A();
        fs[1] = new Ch10B();
        Check.isTrue("10.1", "holds any implementing type", fs[0] instanceof Ch10A);
        Check.isTrue("10.1", "holds any implementing type", fs[1] instanceof Ch10B);

        // length is not part of the type: arrays of different lengths share a type
        int[] short_ = new int[1];
        int[] long_  = new int[9];
        Check.same("10.1", "length is not part of the array type",
                   short_.getClass(), long_.getClass());
    }

    // JLS 10.2
    static void s10_2() {
        int[] v = null;                      // declaring creates no array
        Check.eq("10.2", "an array variable declaration creates no array", v, null);

        int[] braceStyle = new int[2];
        int cStyle[]     = new int[2];       // `T v[]` declares the same type as `T[] v`
        Check.same("10.2", "T[] v and T v[] are the same type",
                   braceStyle.getClass(), cStyle.getClass());

        // one variable successively holds arrays of different lengths
        v = new int[3];
        Check.eq("10.2", "holds a length-3 array", v.length, 3);
        v = new int[7];
        Check.eq("10.2", "then a length-7 array", v.length, 7);
    }

    // JLS 10.3
    static void s10_3() {
        int[] a = new int[4];
        Check.eq("10.3", "new T[n] sets length", a.length, 4);
        Check.eq("10.3", "every component initialized to the default", a[0], 0);
        Check.eq("10.3", "every component initialized to the default", a[3], 0);

        String[] s = new String[2];
        Check.eq("10.3", "reference components default to null", s[0], null);

        int[] init = { 10, 20, 30 };
        Check.eq("10.3", "an initializer creates the array too", init.length, 3);
        Check.eq("10.3", "with the given components", init[1], 20);
    }

    // JLS 10.4
    static void s10_4() {
        int[] a = { 5, 6, 7 };
        Check.eq("10.4", "array access is 0-origin", a[0], 5);

        byte  bi = 2;                        // byte/short/char indices promote to int
        short si = 1;
        char  ci = 0;
        Check.eq("10.4", "byte index promotes to int", a[bi], 7);
        Check.eq("10.4", "short index promotes to int", a[si], 6);
        Check.eq("10.4", "char index promotes to int", a[ci], 5);

        try { int x = a[3]; Check.notThrown("10.4", "index >= length throws"); }
        catch (IndexOutOfBoundsException e) { Check.thrown("10.4", "index >= length throws"); }

        try { int x = a[-1]; Check.notThrown("10.4", "negative index throws"); }
        catch (IndexOutOfBoundsException e) { Check.thrown("10.4", "negative index throws"); }
    }

    // JLS 10.5
    static void s10_5() {
        // the spec's own Gauss program: fill int[101] via ia.length, sum it, expect 5050
        int[] ia = new int[101];
        for (int i = 0; i < ia.length; i++) ia[i] = i;
        int sum = 0;
        for (int i = 0; i < ia.length; i++) sum += ia[i];
        Check.eq("10.5", "the spec's Gauss example sums to 5050", sum, 5050);
    }

    // JLS 10.6
    static void s10_6() {
        int[] a = { 1, 2, 3 };
        Check.eq("10.6", "initializer length equals the expression count", a.length, 3);

        int[][] nested = { { 1, 2 }, { 3, 4, 5 } };   // initializers nest
        Check.eq("10.6", "initializers nest", nested.length, 2);
        Check.eq("10.6", "nested rows keep their own lengths", nested[1].length, 3);
        Check.eq("10.6", "nested components are reachable", nested[1][2], 5);

        int[][] withNull = new int[2][];     // sub-arrays start null
        Check.eq("10.6", "an uninitialized subarray is null", withNull[0], null);
        try { int x = withNull[0][0]; Check.notThrown("10.6", "indexing a null subarray throws"); }
        catch (NullPointerException e) { Check.thrown("10.6", "indexing a null subarray throws"); }
    }

    // JLS 10.7
    static void s10_7() {
        int[] a = { 1, 2, 3 };
        Check.eq("10.7", "arrays have a length member", a.length, 3);

        Check.isTrue("10.7", "arrays implement Cloneable", a instanceof Cloneable);

        int[] c = (int[]) a.clone();
        Check.notSame("10.7", "clone() returns a distinct array", a, c);
        Check.eq("10.7", "the clone has the same length", c.length, a.length);
        Check.eq("10.7", "the clone has the same contents", c[1], 2);
        c[1] = 99;
        Check.eq("10.7", "the clone is a separate array", a[1], 2);

        // shallow: a cloned array of arrays SHARES its subarrays
        int[][] outer = { { 1, 2 }, { 3, 4 } };
        int[][] oc = (int[][]) outer.clone();
        Check.notSame("10.7", "the outer array is copied", outer, oc);
        Check.same("10.7", "clone is shallow: subarrays are shared", outer[0], oc[0]);
    }

    // JLS 10.8
    static void s10_8() {
        int[] a = new int[1];
        int[] b = new int[5];
        Check.same("10.8", "same component type yields the same Class object",
                   a.getClass(), b.getClass());
        Check.eq("10.8", "int[]'s class is named \"[I\"", a.getClass().getName(), "[I");
        Check.eq("10.8", "an array class's superclass is Object",
                 a.getClass().getSuperclass().getName(), "java.lang.Object");

        String[] s = new String[1];
        Check.notSame("10.8", "different component types are different classes",
                      a.getClass(), s.getClass());
    }

    // JLS 10.9
    static void s10_9() {
        char[] cs = { 'a', 'b', 'c' };
        // a char[] is not a String — it is not even assignable to one, and its length is
        // its declared length with no NUL terminator
        Check.eq("10.9", "a char array is not NUL-terminated", cs.length, 3);
        Check.isTrue("10.9", "a char array is not a String", !(((Object) cs) instanceof String));

        String str = "abc";
        char[] from = str.toCharArray();
        Check.eq("10.9", "toCharArray has the same characters", from[0], 'a');
        from[0] = 'z';
        Check.eq("10.9", "toCharArray returns a separate array", str.charAt(0), 'a');
    }

    // JLS 10.10
    static void s10_10() {
        // the array's ACTUAL element type is the subtype; the reference is widened
        Ch10Derived[] actual = new Ch10Derived[2];
        Ch10Base[] widened = actual;

        // reading through the widened reference is fine
        Check.eq("10.10", "reads through the widened reference succeed", widened[0], null);

        // storing a supertype instance throws
        try {
            widened[0] = new Ch10Base();
            Check.notThrown("10.10", "storing a supertype instance throws ArrayStoreException");
        } catch (ArrayStoreException e) {
            Check.thrown("10.10", "storing a supertype instance throws ArrayStoreException");
        }

        // storing the correct type succeeds
        widened[1] = new Ch10Derived();
        Check.isTrue("10.10", "storing the element type succeeds",
                     actual[1] instanceof Ch10Derived);
    }

    public static void run() {
        s10_1(); s10_2(); s10_3(); s10_4(); s10_5();
        s10_6(); s10_7(); s10_8(); s10_9(); s10_10();
    }
}
