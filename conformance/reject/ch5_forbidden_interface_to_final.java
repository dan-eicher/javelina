// JLS 5.1.7
// EXPECT does not implement
//
// "There is no permitted conversion other than string conversion from interface type J to
// class type T if T is final and does not implement J." The mirror of the class-to-interface
// bullet, and it needs its own case: the two are separate list entries and a compiler can
// easily implement one direction only.
interface Ch5IfaceJ { void j(); }
final class Ch5FinalT { int x; }

class Ch5IfaceToFinal {
    static Ch5FinalT f(Ch5IfaceJ j) {
        return (Ch5FinalT) j;
    }
}
