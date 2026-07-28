// JLS 6.3
// EXPECT forward reference
//
// §6.3's first Test program, verbatim:
//
//   class Test {
//       int i = j;   // compile-time error: incorrect forward reference
//       int j = 1;
//   }
//
// The rule is stated as an exception rather than a blanket ordering requirement: "The
// declaration of a member needs to appear before it is used ONLY when the use is in a field
// initialization expression (§8.3.2, §12.4.2, §12.5)." So this is an error while the same
// reference from a constructor is not -- that legal direction is Lib6's t6.scope.ctor.uses
// .later.field, and the pair is the rule. Rejecting both would be as wrong as accepting both.
class Ch6ForwardRef {
    int i = j;
    int j = 1;
}
