// Ch10b — the §10.6 / §10.2 / §15.15 array-shape rules that javelinac could not parse until
// the grammar carried the spec's repetitions. Kept as its own class because these were found
// as compile failures, and the compile is half of what they assert: each construct here is
// legal Java that the parser rejected.
//
// The defect they pinned: `dims` is an int32 COUNT in Java.asdl, but Java.peg wrote 1 into it
// and sema.c tested it with `> 0` and wrapped once. Three layers, each doing the minimum,
// agreeing on a wrong type. Fixing only the grammar would have turned a parse error into a
// SILENT int[] for `int v[][]`, which is why the two halves landed together.

public class Ch10b {

    // JLS 10.6
    static void s10_6_trailing() {
        // ArrayInitializer: { VariableInitializers_opt ,_opt } — both parts optional
        int[] one       = { 1, };
        int[] several   = { 1, 2, 3, };
        int[] empty     = { };
        int[] justComma = { , };
        int[][] nested  = { { 1, }, };

        Check.eq("10.6", "a trailing comma is ignored", one.length, 1);
        Check.eq("10.6", "a trailing comma is ignored after a list", several.length, 3);
        Check.eq("10.6", "an empty initializer yields length 0", empty.length, 0);
        Check.eq("10.6", "VariableInitializers_opt with only a comma is length 0",
                 justComma.length, 0);
        Check.eq("10.6", "a trailing comma nests", nested.length, 1);
        Check.eq("10.6", "the nested row keeps its own trailing comma", nested[0].length, 1);
        Check.eq("10.6", "components survive the trailing comma", several[2], 3);
    }

    // JLS 10.2
    static void s10_2_declarator() {
        // §10.2: the bracket pairs may sit on the type, on the declarator, or be split
        // between them; all three spell the same type.
        int onDeclarator[][] = new int[2][3];
        int[] split[]        = new int[2][3];
        int[][] onType       = new int[2][3];

        Check.same("10.2", "int v[][] and int[][] v are the same type",
                   onDeclarator.getClass(), onType.getClass());
        Check.same("10.2", "int[] v[] is that same type too",
                   split.getClass(), onType.getClass());
        Check.eq("10.2", "the type is two-dimensional", onType.getClass().getName(), "[[I");
        Check.eq("10.2", "the inner dimension is real", onDeclarator[0].length, 3);

        int deep[][][] = new int[1][2][3];
        Check.eq("10.2", "three declarator dimensions", deep.getClass().getName(), "[[[I");
        Check.eq("10.2", "the innermost dimension is real", deep[0][1].length, 3);
    }

    // JLS 15.15
    static void s15_15_cast() {
        // §4.3: an array type nests, so a cast may carry any number of [] pairs (§15.15).
        Object o2 = new int[2][3];
        Object o3 = new String[1][1][1];

        int[][] a = (int[][]) o2;
        Check.eq("15.15", "a 2-D primitive array cast round-trips", a.length, 2);
        Check.eq("15.15", "and keeps its inner dimension", a[0].length, 3);

        String[][][] b = (String[][][]) o3;
        Check.eq("15.15", "a 3-D reference array cast round-trips", b.length, 1);
        Check.eq("15.15", "and reaches its innermost row", b[0][0].length, 1);

        // a cast to the wrong array rank must still fail
        try {
            int[] wrong = (int[]) o2;
            Check.notThrown("15.15", "casting int[][] to int[] throws ClassCastException");
        } catch (ClassCastException e) {
            Check.thrown("15.15", "casting int[][] to int[] throws ClassCastException");
        }
    }

    public static void run() {
        s10_6_trailing();
        s10_2_declarator();
        s15_15_cast();
    }
}
