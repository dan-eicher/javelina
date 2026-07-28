// JLS 8.3.1.2
// EXPECT final field
//
// "A field can be declared final, in which case its declarator must include a variable
// initializer or a compile-time error occurs." (p.146)
//
// Java 1.0 has no blank finals: the initializer is required IN THE DECLARATOR, so a
// constructor that assigns the field is not a substitute. That is the 1.1 rule, and applying
// it here would accept this program.
class Ch8FinalNoInit {
    final int blank;

    Ch8FinalNoInit(int v) {
        blank = v;
    }
}
