// Lib10 — JLS chapter 10, Arrays.
//
// The eight sections here were carried only by conformance/jls/Ch10.java and Ch10b.java, which
// are deliberately not read: crisp-tallying-chapters §4 has the corpus written from the spec,
// and porting old assertions forward moves their blind spots into the generator.
//
// Transcribed from pp.194-199:
//
//   §10.1  "An array's length is not part of its type." Arrays with an interface component
//          type are allowed, elements being null or instances of any implementing class; same
//          for an abstract class component type, with any non-abstract subclass.
//   §10.2  "Declaring a variable of array type does not create an array object or allocate any
//          space for array components." The [] "may appear as part of the type ... or as part
//          of the declarator for a particular variable, or both": `byte[] rowvector, colvector,
//          matrix[];` is equivalent to `byte rowvector[], colvector[], matrix[][];`. "Once an
//          array object is created, its length never changes."
//   §10.3  created by a creation expression (§15.9) or an initializer (§10.6); "the array's
//          length is available as a final instance variable length"; an initializer "provides
//          initial values for ALL its components".
//   §10.4  "All arrays are 0-origin." short/byte/char indices promote to int (§5.6.1); "an
//          attempt to access an array component with a long index value results in a
//          compile-time error"; out-of-range throws IndexOutOfBoundsException.
//   §10.5  the Gauss example, whose stated output is 5050.
//   §10.7  members: "the public final field length"; "the public method clone, which overrides
//          the method of the same name in class Object and THROWS NO CHECKED EXCEPTIONS"; all
//          members inherited from Object except its clone.
//   §10.8  "Every array has an associated Class object, shared with all other arrays with the
//          same component type. The superclass of an array type is considered to be Object" --
//          worked output `class [I` then `class java.lang.Object`.
//   §10.9  "an array of char is not a String, and neither a String nor an array of char is
//          terminated by the NUL character"; a String is immutable "while an array of char has
//          mutable elements"; toCharArray returns the same character sequence.
//
// A NOTE ON THIS FILE'S OWN BYTES. The line above spells the NUL character out in words. The
// first draft quoted the spec's literal escape and put an actual 0x00 byte in the comment,
// which is legal Java -- §3.7 makes a comment's content any InputCharacter -- but javelinac
// silently stopped lexing there, so `class Lib10` was never declared and the only symptom was
// "undefined 'Lib10'" from GenMain. grep hid it too, treating the file as binary and matching
// nothing. Recorded because the compiler accepting the file with exit 0 while dropping half of
// it is a real divergence, not a quirk of writing this comment.
public class Lib10 {

    private Lib10() {}

    public static void install(Registry r) {
        r.register(new Sn10ElementTypes());
        r.register(new Sn10DeclaratorBrackets());
        r.register(new Sn10Creation());
        r.register(new Sn10Access());
        r.register(new Sn10Gauss());
        r.register(new Sn10Members());
        r.register(new Sn10ClassObjects());
        r.register(new Sn10CharArrayNotString());

        // §10.4: "An attempt to access an array component with a long index value results in a
        // compile-time error." The promoting kinds are exercised positively by Sn10Access, so
        // the pair states the whole rule: byte/short/char promote, long does not.
        r.register(new Sn10RejectLongIndex());
    }

    /** The component types §10.1 calls out. Shared; Emit dedupes by text. */
    static String[] decls() {
        String[] d = { "interface T10Face {\n"
                     + "    int mark();\n"
                     + "}",

                       "class T10Impl implements T10Face {\n"
                     + "    public int mark() { return 3; }\n"
                     + "}",

                       "abstract class T10Abs {\n"
                     + "    abstract int mark();\n"
                     + "}",

                       "class T10Concrete extends T10Abs {\n"
                     + "    int mark() { return 5; }\n"
                     + "}" };
        return d;
    }
}

/** §10.1: the element type may be any type. The two the section calls out are the ones a naive
 *  implementation gets wrong -- an INTERFACE and an ABSTRACT class, neither of which can be
 *  instantiated, so the array's slots hold null or an instance of something that can. */
class Sn10ElementTypes implements Snippet, Declaring {
    public String   id()        { return "t10.type.element.kinds"; }
    public String[] sections()  { return Strs.of("10.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib10.decls(); }

    public String render(String[] h) {
        return "{ T10Face[] fa = new T10Face[2];"
             + " fa[1] = new T10Impl();"
             + " T10Abs[] aa = new T10Abs[2];"
             + " aa[1] = new T10Concrete();"
             + " int got = 0;"
             + " if (fa[0] == null) got += 1;"          // a null element is a legal value
             + " got += fa[1].mark();"                   // ...or any implementing class
             + " if (aa[0] == null) got += 8;"
             + " got += aa[1].mark();"                   // ...or any non-abstract subclass
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(17 + h[0].asInt()); }
}

/** §10.2: the spec's own declaration, verbatim --
 *
 *      byte[] rowvector, colvector, matrix[];
 *
 *  -- "is equivalent to `byte rowvector[], colvector[], matrix[][];`". So the [] on the TYPE
 *  distributes to every declarator and the [] on a declarator adds to that one alone: matrix is
 *  two-dimensional while its neighbours are one-dimensional. A compiler that applied the
 *  trailing bracket to all three, or ignored it, still compiles -- and gets matrix's depth
 *  wrong, which is what indexing it twice detects.
 *
 *  Also "an array's length is not part of its type": one variable holds arrays of two lengths. */
class Sn10DeclaratorBrackets implements Snippet {
    public String   id()        { return "t10.var.declarator.brackets"; }
    public String[] sections()  { return Strs.of("10.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ byte[] rowvector, colvector, matrix[];"
             + " rowvector = new byte[3];"
             + " colvector = new byte[7];"
             + " matrix = new byte[2][4];"
             + " matrix[1][3] = (byte) 9;"
             + " int[] varying = new int[2];"
             + " varying = new int[5];"                  // length is not part of the type
             + " System.out.println(rowvector.length + colvector.length + matrix.length"
             + " + matrix[0].length + matrix[1][3] + varying.length + (" + h[0] + ")); }";
    }
    // 3 + 7 + 2 + 4 + 9 + 5
    public Val expect(Val[] h) { return Val.ofInt(30 + h[0].asInt()); }
}

/** §10.3: both ways of creating an array, and "the array's length is available as a final
 *  instance variable length". An initializer "provides initial values for ALL its components",
 *  so its length equals the number of expressions -- contrast with C, which the spec draws. */
class Sn10Creation implements Snippet {
    public String   id()        { return "t10.create.two.ways"; }
    public String[] sections()  { return Strs.of("10.3"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int[] made = new int[3];"
             + " int[] init = { 10, 20, 30, 40 };"
             + " int sum = 0;"
             + " for (int i = 0; i < init.length; i++) sum += init[i];"
             + " System.out.println(made.length + init.length + sum + (" + h[0] + ")); }";
    }
    // 3 + 4 + 100
    public Val expect(Val[] h) { return Val.ofInt(107 + h[0].asInt()); }
}

/** §10.4: 0-origin, indices of type byte/short/char promoted by §5.6.1, and an out-of-range
 *  index throwing IndexOutOfBoundsException -- caught here so the rest of the line still runs,
 *  which is what shows the throw happened rather than the access silently succeeding. */
class Sn10Access implements Snippet {
    public String   id()        { return "t10.access.origin.and.index.types"; }
    public String[] sections()  { return Strs.of("10.4", "5.6.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ int[] a = { 100, 200, 300 };"
             + " byte bi = 0; short si = 1; char ci = 2;"
             + " int got = a[bi] + a[si] + a[ci];"       // all three promote to int
             + " int caught = 0;"
             + " try { got += a[3]; } catch (IndexOutOfBoundsException e) { caught += 1; }"
             + " try { got += a[-1]; } catch (IndexOutOfBoundsException e) { caught += 2; }"
             + " System.out.println(got + caught); }";
    }
    // 100 + 200 + 300 = 600, plus 3 for both throws
    public Val expect(Val[] h) { return Val.ofInt(603); }
}

/** §10.5's worked example, whose stated output is 5050. Reproduced with its shape intact --
 *  `new int[101]`, filled 0..100 through ia.length, summed through ia.length again. */
class Sn10Gauss implements Snippet {
    public String   id()        { return "t10.gauss"; }
    public String[] sections()  { return Strs.of("10.5"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ int[] ia = new int[101];"
             + " for (int i = 0; i < ia.length; i++) ia[i] = i;"
             + " int sum = 0;"
             + " for (int i = 0; i < ia.length; i++) sum += ia[i];"
             + " System.out.println(sum); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(5050); }
}

/** §10.7: an array's members. The sharp one is clone -- it "overrides the method of the same
 *  name in class Object and THROWS NO CHECKED EXCEPTIONS", so the call needs no try/catch and
 *  no throws clause. Object.clone declares CloneNotSupportedException; if an array inherited
 *  that signature instead of overriding it, this would not compile at all.
 *
 *  clone is also the one Object method an array does NOT inherit, and length is public final. */
class Sn10Members implements Snippet {
    public String   id()        { return "t10.members.length.and.clone"; }
    public String[] sections()  { return Strs.of("10.7"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int[] a = { 1, 2, 3 };"
             + " int[] copy = (int[]) a.clone();"        // no try/catch: no checked exception
             + " copy[0] = 40;"
             + " int got = a.length + copy.length + a[0] + copy[0];"
             + " if (a != copy) got += 100;"             // a duplicate, not the same object
             + " if (a.getClass() == copy.getClass()) got += 1000;"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    // 3 + 3 + 1 + 40 + 100 + 1000
    public Val expect(Val[] h) { return Val.ofInt(1147 + h[0].asInt()); }
}

/** §10.8's worked example, whose stated output is
 *
 *      class [I
 *      class java.lang.Object
 *
 *  -- "[I" being "the run-time type signature for the class object 'array with component type
 *  int' (§20.1.1)", and the superclass of an array type being Object. Printed as the spec
 *  prints it, on one line, because the claim is those exact names. */
class Sn10ClassObjects implements Snippet {
    public String   id()        { return "t10.class.objects"; }
    public String[] sections()  { return Strs.of("10.8"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ int[] ia = new int[3];"
             + " int[] other = new int[9];"
             + " System.out.println(ia.getClass() + \"|\" + ia.getClass().getSuperclass()"
             + " + \"|\" + (ia.getClass() == other.getClass() ? 1 : 0)); }";
    }
    // The Class objects are shared across all int arrays whatever their length.
    public Val expect(Val[] h) {
        return Val.ofString("class [I|class java.lang.Object|1");
    }
}

/** §10.9: "an array of char is not a String, and neither a String nor an array of char is
 *  terminated by the NUL character". The length is the decisive part -- a C-minded
 *  implementation would make toCharArray return three elements for a two-character string.
 *
 *  Also the asymmetry the section states: a String is immutable, an array of char is not. */
class Sn10CharArrayNotString implements Snippet {
    public String   id()        { return "t10.chararray.not.string"; }
    public String[] sections()  { return Strs.of("10.9"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ String s = \"hi\";"
             + " char[] c = s.toCharArray();"
             + " c[0] = 'H';"                            // char[] elements are mutable
             + " String rebuilt = new String(c);"
             + " System.out.println(c.length + \"|\" + s + \"|\" + rebuilt); }";
    }
    // length 2, not 3: no trailing NUL. `s` is unchanged by the mutation of the copy.
    public Val expect(Val[] h) { return Val.ofString("2|hi|Hi"); }
}

/** §10.4: "An attempt to access an array component with a long index value results in a
 *  compile-time error." Deliberately paired with t10.access.origin.and.index.types, where
 *  byte, short and char indices all succeed -- the rule is that unary numeric promotion
 *  (§5.6.1) reaches int and stops, not that non-int indices are banned. */
class Sn10RejectLongIndex implements Snippet {
    public String   id()        { return "t10.reject.long.index"; }
    public String[] sections()  { return Strs.of("10.4"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T10LongIndex {\n"
             + "    static int f() {\n"
             + "        int[] a = new int[3];\n"
             + "        long i = 1L;\n"
             + "        return a[i];\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("array index"); }
}
