// JLS 5.1.7
// EXPECT unrelated classes
//
// "There is no permitted conversion other than string conversion from class type S to a
// different class type T if S is not a subclass of T and T is not a subclass of S."
//
// UNCONDITIONAL for class-to-class — neither type needs to be final. Java has single
// inheritance, so no object is ever an instance of two unrelated classes, and §5.5 says a
// cast that can be proven incorrect at compile time IS a compile-time error. Accepting it
// and letting a ClassCastException report at run time reports the right fact at the wrong
// time; this file is the one that caught that.
class Ch5UnrelatedA { int x; }
class Ch5UnrelatedB { int y; }

class Ch5Unrelated {
    static Ch5UnrelatedB f() {
        Ch5UnrelatedA a = new Ch5UnrelatedA();
        return (Ch5UnrelatedB) a;
    }
}
