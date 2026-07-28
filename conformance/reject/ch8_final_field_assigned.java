// JLS 8.3.1.2
// EXPECT final field
//
// "Any attempt to assign to a final field results in a compile-time error." (p.146)
//
// The field here HAS its required initializer, so §8.3.1.2's first sentence is satisfied and
// only the second is at issue — an assignment from an ordinary method, which is the case a
// blank-final implementation would still reject but for the wrong reason (already assigned)
// and only inside a constructor.
class Ch8FinalAssigned {
    final int fixed = 7;

    void bump() {
        fixed = 8;
    }
}
