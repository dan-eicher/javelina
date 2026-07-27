// JLS 5.1.7
// EXPECT different return types
//
// "There is no permitted conversion from interface type J to interface type K if J and K
// declare methods with the same signature but different return types."
//
// A class implementing both would have to declare one method with two return types, so no
// object can satisfy the cast. Same signature but the SAME return type is fine, and
// conformance/jls has no way to assert that half — an accepted program proves it by
// compiling, which is exactly what makes this the negative corpus's job.
interface Ch5ClashJ { int m(); }
interface Ch5ClashK { long m(); }

class Ch5IfaceClash {
    static Ch5ClashK f(Ch5ClashJ j) {
        return (Ch5ClashK) j;
    }
}
