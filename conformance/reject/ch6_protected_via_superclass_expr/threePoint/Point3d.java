// JLS 6.6.2
// EXPECT protected member 'x' is not accessible through
//
// §6.6.7's worked example, verbatim in its own package names. Let C be the class declaring the
// protected member and S the subclass in whose body the use occurs -- here C is points.Point
// and S is threePoint.Point3d. §6.6.2:
//
//   "If the access is by a qualified name Q.Id, where Q is an ExpressionName, then the access
//    is permitted if and only if the type of the expression Q is S or a subclass of S."
//
// `p` has type Point, which is C, not S or a subclass of S, so `p.x` is a compile-time error.
// §6.6.7 says why: "while Point3d (the class in which the references to fields x and y occur)
// is a subclass of Point (the class in which x and y are declared), it is not involved in the
// implementation of a Point (the type of the parameter p)."
//
// delta3d is the CONTRAST and must keep compiling -- q has type Point3d, which is S. It is in
// this file on purpose: a compiler that fixed the error by banning protected access outright
// would satisfy the rejection and break the accompanying positive case in conformance/gen.
package threePoint;

import points.Point;

public class Point3d extends Point {
    protected int z;

    public void delta(Point p) {
        p.x += this.x;
        p.y += this.y;
    }

    public void delta3d(Point3d q) {
        q.x += this.x;
        q.y += this.y;
        q.z += this.z;
    }
}
