package javelina.peg;

// A grammar is wrong — an undefined rule, or left recursion. Raised by
// Grammar.finish, never by a parse: a malformed grammar is a programming error
// found once, not an input that failed to match.
public class PegException extends RuntimeException {
    public PegException(String message) {
        super(message);
    }
}
