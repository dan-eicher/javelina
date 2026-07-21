#!/bin/sh
# test_cli.sh — E7.1 / E7.1a: the shipped CLIs end-to-end (javelinac + javelina).
# The runner IS the harness: compile real .java with javelinac, run with javelina, assert
# exit codes + stdout + stderr. Exercises what the C unit tests can't — the command surface:
# diagnostics, bad input, directory input, argv round-trip, System.exit, uncaught exceptions.
#
# Run from compiler/ with the binaries + jre.wasm built (the Makefile `test-cli` target does).
set -u
JC=build/javelinac
JV="build/javelina --jre build/jre.wasm"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
pass=0; fail=0
ok()  { echo "  ....  $1"; pass=$((pass + 1)); }
no()  { echo "  FAIL  $1  ($2)"; fail=$((fail + 1)); }

# compile SRC (a file/dir) → OUT.wasm; returns javelinac's exit code
compile() { $JC --mode plugin "$1" -o "$2" 2>"$TMP/cc.err"; }

# ── E7.1 — javelinac CLI ────────────────────────────────────────────────────
$JC --version >"$TMP/o" 2>&1
[ $? -eq 0 ] && grep -q "javelinac" "$TMP/o" && ok "javelinac --version -> 0 + version" || no "--version" "$(cat "$TMP/o")"

$JC >"$TMP/o" 2>"$TMP/e"; rc=$?
[ $rc -eq 2 ] && grep -q "no input files" "$TMP/e" && ok "javelinac (no input) -> exit 2 + diagnostic" || no "no-input" "rc=$rc"

$JC "$TMP/nope.java" >"$TMP/o" 2>"$TMP/e"; rc=$?
[ $rc -eq 2 ] && ok "javelinac missing-file -> exit 2" || no "missing-file" "rc=$rc"

# sema error: a type error must fail-closed (exit 1) with a file:line:col diagnostic to stderr
cat >"$TMP/Bad.java" <<'EOF'
public class Bad { public static int f() { int x = "not an int"; return x; } }
EOF
$JC --mode plugin "$TMP/Bad.java" -o "$TMP/Bad.wasm" >"$TMP/o" 2>"$TMP/e"; rc=$?
[ $rc -eq 1 ] && grep -Eq "$TMP/Bad.java:[0-9]+:[0-9]+:" "$TMP/e" && ok "sema error -> exit 1 + file:line:col to stderr" || no "sema-diag" "rc=$rc err=$(cat "$TMP/e")"

# directory input: a package-nested source tree passed as one argument
mkdir -p "$TMP/src/app"
cat >"$TMP/src/app/Main.java" <<'EOF'
public class Main { public static void main(String[] a){ System.out.println("dir:"+Helper.two()); } }
EOF
cat >"$TMP/src/app/Helper.java" <<'EOF'
public class Helper { public static int two(){ return 2; } }
EOF
compile "$TMP/src" "$TMP/Dir.wasm" && out=$($JV "$TMP/Dir.wasm" 2>/dev/null)
[ "$out" = "dir:2" ] && ok "directory input (recursive *.java) -> compiles + runs" || no "dir-input" "out=$out"

# ── E7.1a — program entry + argv + exit codes ──────────────────────────────
cat >"$TMP/Echo.java" <<'EOF'
public class Echo {
    public static void main(String[] a){
        System.out.print(a.length);
        for (int i = 0; i < a.length; i++) System.out.print(":" + a[i]);
        System.out.println();
    }
}
EOF
compile "$TMP/Echo.java" "$TMP/Echo.wasm" || no "echo-compile" "$(cat "$TMP/cc.err")"

out=$($JV "$TMP/Echo.wasm" alpha beta gamma 2>/dev/null)
[ "$out" = "3:alpha:beta:gamma" ] && ok "argv round-trip (3 args)" || no "argv-3" "out=$out"

out=$($JV "$TMP/Echo.wasm" 2>/dev/null)
[ "$out" = "0" ] && ok "argv round-trip (empty args)" || no "argv-0" "out=$out"

# non-ASCII: assert the UTF-8 DECODE produced the right code points (not the printed bytes —
# JLS 1.0 PrintStream writes chars as low-8-bits, so non-ASCII output is Latin-1, lossy by spec).
# arg "é€" is é=U+00E9 (2-byte UTF-8) + €=U+20AC (3-byte); decode -> 2 chars 233, 8364.
cat >"$TMP/Uni.java" <<'EOF'
public class Uni {
    public static void main(String[] a){
        System.out.print(a[0].length());
        for (int i = 0; i < a[0].length(); i++) System.out.print(":" + (int)a[0].charAt(i));
        System.out.println();
    }
}
EOF
compile "$TMP/Uni.java" "$TMP/Uni.wasm"
out=$($JV "$TMP/Uni.wasm" "é€" 2>/dev/null)
[ "$out" = "2:233:8364" ] && ok "argv UTF-8 decode (2- and 3-byte -> code points)" || no "argv-utf8" "out=$out"

# exit codes: fall-off main -> 0
cat >"$TMP/Void.java" <<'EOF'
public class Void { public static void main(String[] a){ System.out.println("done"); } }
EOF
compile "$TMP/Void.java" "$TMP/Void.wasm"
$JV "$TMP/Void.wasm" >/dev/null 2>&1
[ $? -eq 0 ] && ok "normal completion -> exit 0" || no "exit-0" "rc=$?"

# System.exit(n) -> exit n, and code after it does not run
cat >"$TMP/ExitN.java" <<'EOF'
public class ExitN { public static void main(String[] a){ System.out.println("before"); System.exit(5); System.out.println("after"); } }
EOF
compile "$TMP/ExitN.java" "$TMP/ExitN.wasm"
out=$($JV "$TMP/ExitN.wasm" 2>/dev/null); rc=$?
[ $rc -eq 5 ] && [ "$out" = "before" ] && ok "System.exit(5) -> exit 5, no code after" || no "exit-n" "rc=$rc out=$out"

# uncaught exception -> exit 1 + stack trace on stderr (via $main's catch(Throwable))
cat >"$TMP/Throw.java" <<'EOF'
public class Throw { public static void main(String[] a){ throw new RuntimeException("boom"); } }
EOF
compile "$TMP/Throw.java" "$TMP/Throw.wasm"
$JV "$TMP/Throw.wasm" >"$TMP/o" 2>"$TMP/e"; rc=$?
[ $rc -eq 1 ] && grep -q "boom" "$TMP/e" && ok "uncaught exception -> exit 1 + trace on stderr" || no "uncaught" "rc=$rc err=$(cat "$TMP/e")"

# ── JLS §14.19 — "It is a compile-time error if a statement cannot be executed because it is
# unreachable." The condition is a §15.27 constant expression, not just a literal. `if (false)` is
# the one shape the spec declares REACHABLE (the conditional-compilation idiom), so it must compile.
reject() {   # source, label — must fail with `unreachable statement`
    printf '%s\n' "$1" > "$TMP/U.java"
    $JC --mode plugin "$TMP/U.java" -o "$TMP/U.wasm" >/dev/null 2>"$TMP/u.err"; rc=$?
    [ $rc -eq 1 ] && grep -q "unreachable statement" "$TMP/u.err" \
        && ok "§14.19 rejects: $2" || no "§14.19 should reject: $2" "rc=$rc $(head -1 "$TMP/u.err")"
}
accept() {   # source, label — must compile
    printf '%s\n' "$1" > "$TMP/A.java"
    $JC --mode plugin "$TMP/A.java" -o "$TMP/A.wasm" >/dev/null 2>"$TMP/a.err" \
        && ok "§14.19 accepts: $2" || no "§14.19 should accept: $2" "$(head -1 "$TMP/a.err")"
}
reject 'public class U { static int t(){ while (false) { int x = 3; } return 1; } }'   'while(false) body'
reject 'public class U { static int t(){ while (1 == 2) { int x = 3; } return 1; } }'  'while(1==2) body (constant expression)'
reject 'public class U { static int t(){ for (; false;) { int x = 3; } return 1; } }'  'for(;false;) body'
reject 'public class U { static int t(){ return 1; } static void u(){ return; int x = 3; } }' 'statement after return'
accept 'public class U { static int t(){ if (false) { int x = 3; } return 1; } }'      'if(false) body (ACTUAL if rule)'
accept 'public class U { static int t(){ while (1 == 1) { return 1; } } }'             'while(1==1) needs no trailing return (§8.4.7)'
accept 'public class U { static int t(){ for (int i = 0; i < 10; i++) { return 1; } return 0; } }' 'ForUpdate is not a statement (§14.14)'
accept 'public class U { static int t(){ do { return 1; } while (true); } }'           'do-while(true) tail condition is an expression'
accept 'public class U { static int t(int x){ switch (x) { case 1: return 1; } return 0; } }' 'switch with no default falls out (§14.9)'
reject 'public class U { static void t(){ switch (1) { case 1: return; int x = 3; } } }'      'statement after return inside a switch group'
accept 'public class U { static void t(){ switch (1) { case 1: return; case 2: int x = 3; } } }' 'next group bears a label, so it is reachable'
reject 'public class U { static void t(){ try { return; int x = 3; } finally { } } }'        'statement after return inside a try block'
reject 'public class U { static int t(){ do { return 1; } while (true); int x = 3; } }'      'statement after a do-while(true)'

# §8.4.7 "a compile-time error occurs if the body of the method can complete normally" — the SAME
# §14.19 completion predicate the reachability rules and codegen read. One rule, three consumers.
missing() {
    printf '%s\n' "$1" > "$TMP/M.java"
    $JC --mode plugin "$TMP/M.java" -o "$TMP/M.wasm" >/dev/null 2>"$TMP/m.err"; rc=$?
    [ $rc -eq 1 ] && grep -q "missing return statement" "$TMP/m.err" \
        && ok "§8.4.7 rejects: $2" || no "§8.4.7 should reject: $2" "rc=$rc $(head -1 "$TMP/m.err")"
}
missing 'public class M { static int t(int x){ switch (x) { case 1: return 1; } } }'         'switch with no default can complete normally'
missing 'public class M { static int t(int x){ if (x > 0) return 1; } }'                     'if-then can complete normally'
accept  'public class M { static int t(int x){ switch (x) { case 1: return 1; default: return 2; } } }' 'switch with a default whose groups all return'
accept  'public class M { static int t(int x){ while (1 == 1) { return 1; } } }'             'while(1==1) cannot complete normally (§15.27)'
accept  'public class M { static int t(int x){ if (x > 0) return 1; else throw new RuntimeException("x"); } }' 'both arms complete abruptly'

# ── §14.19 catch-block reachability + §11.2. A catch for an UNCHECKED type (or for a supertype of
# one) is always reachable; an unreachable CHECKED catch is a compile-time error, at its own position.
creject() {
    printf '%s\n' "$1" > "$TMP/E.java"
    $JC --mode plugin "$TMP/E.java" -o "$TMP/E.wasm" >/dev/null 2>"$TMP/e2.err"; rc=$?
    [ $rc -eq 1 ] && grep -q "unreachable catch clause" "$TMP/e2.err" \
        && grep -Eq "$TMP/E.java:[0-9]+:[0-9]+:" "$TMP/e2.err" \
        && ok "§14.19 rejects: $2" || no "§14.19 should reject: $2" "rc=$rc $(head -1 "$TMP/e2.err")"
}
cacc() {
    printf '%s\n' "$1" > "$TMP/F.java"
    $JC --mode plugin "$TMP/F.java" -o "$TMP/F.wasm" >/dev/null 2>"$TMP/f.err" \
        && ok "§11.2 accepts: $2" || no "§11.2 should accept: $2" "$(head -1 "$TMP/f.err")"
}
creject 'public class E { static void t(){ try { int x=1; } catch (InterruptedException e) {} } }'            'unreachable checked catch'
creject 'public class E { static void t(){ try { int x=1; } catch (java.lang.InterruptedException e) {} } }'  'unreachable checked catch, qualified type name'
cacc 'public class F { static void t(){ try { int x=1; } catch (RuntimeException e) {} } }'                   'catch(RuntimeException) — unchecked'
cacc 'public class F { static void t(){ try { int x=1; } catch (NullPointerException e) {} } }'               'catch(NullPointerException) — unchecked subclass'
cacc 'public class F { static void t(){ try { int x=1; } catch (Exception e) {} } }'                          'catch(Exception) — assignable from RuntimeException'
cacc 'public class F { static void t(){ try { int x=1; } catch (Throwable e) {} } }'                          'catch(Throwable)'
cacc 'public class F { static void g() throws InterruptedException {} static void t(){ try { g(); } catch (InterruptedException e) {} } }' 'checked catch the try can throw'

# ── E7.1 — the tier flag: -jit runs the copy-and-patch tier, and the tier is semantics-free.
# (Breadth across both tiers is the E7.4 corpus's job; these pin that the flag reaches the engine
# and that a JIT'd frame can still reach the →HOST natives.)
JIT="build/javelina --jre build/jre.wasm -jit"
out_i=$($JV "$TMP/Echo.wasm" alpha beta 2>/dev/null)
out_j=$($JIT "$TMP/Echo.wasm" alpha beta 2>/dev/null); rc=$?
[ $rc -eq 0 ] && [ "$out_i" = "$out_j" ] && ok "-jit agrees with -nojit (stdout + argv)" || no "jit-echo" "interp=$out_i jit=$out_j rc=$rc"

out=$($JIT "$TMP/ExitN.wasm" 2>/dev/null); rc=$?
[ $rc -eq 5 ] && [ "$out" = "before" ] && ok "-jit: System.exit(5) host native from a JIT'd frame" || no "jit-exit" "rc=$rc out=$out"

echo "cli tests: $pass passed, $fail failed"
[ $fail -eq 0 ]
