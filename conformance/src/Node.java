// The graph node. Every shape in Graph.java is built out of these.
//
// `guard` is set from the id at construction and never written again, so any
// later mismatch means the object's bytes moved wrong, were overwritten by a
// neighbour, or were never copied at all. `next`/`prev`/`cross` are checked for
// object IDENTITY rather than value: a collector that copies an object but
// misses one of the references pointing at it leaves a stale pointer that still
// carries a valid-looking guard, and only an identity check catches that.
//
// Size note: 3 ints + 5 references + the 24-byte header lands under
// IMX_SMALL_MAX (128), so a Node is a SMALL object — bump-allocated into holes
// in recyclable blocks (immix_space.c:59-72), which is the path that fragments.

class Node {
    int id;
    int guard;
    int sub;          // subgraph tag, for the cross-link kernel
    Node next;
    Node prev;
    Node cross;
    Node[] kids;
    Object payload;

    Node(int id) {
        this.id = id;
        this.guard = GcTorture.mix(id);
    }
}
