package javelina.peg;

// A semantic action: the library calling into user code.
//
// One method, deliberately. A single-method interface becomes a functional
// interface the moment this is compiled against Java 8 or later, so what is one
// top-level class per action today is a lambda after a port, with no change to
// this file or to any caller. An abstract class could never be a lambda target,
// which is why this is an interface even though Java 1.0 gains nothing from it.
//
// The whole input is passed rather than the matched substring: an action that
// wants the text calls `input.substring(start, end)`, and one that only wants
// the extent pays nothing.
//
// `parts` holds the values returned by the actions nested directly inside this
// one, in the order they matched — empty, never null, when there are none.
public interface PegAction {
    Object act(String input, int start, int end, Object[] parts);
}
