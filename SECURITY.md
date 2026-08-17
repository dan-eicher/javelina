# Security

javelina is a WebAssembly engine, so its whole job is running code it has every
reason to distrust. Two rules follow from that and are treated as correctness
requirements rather than aspirations:

- **The engine never segfaults on any input.** A malformed, truncated,
  adversarial or merely strange module is a *rejection* — a validation error or a
  trap with the official §7 reason — never a crash. A segfault is a bug of the
  highest severity available here, even when the input is nonsense.

- **A library never aborts its host.** javelina is linked into someone else's
  program. It does not call `abort()`, does not `exit()`, and does not write to
  the host's streams on an error path. A failure traps, poisons the affected
  instance, and returns a status the embedder can act on.

If you find input that violates either, that is the report worth sending.

## Maturity

Version 0.1.0 — a first release, and not independently audited. The conformance
numbers in the README say the engine agrees with the specification on the
official suite; they do not say it has withstood an adversary. There is no
fuzzing harness in-tree yet — the closest thing is the Java corpus in
`conformance/`, which is a bug-finding instrument rather than a security one.

Treat it accordingly: it is not yet the thing to put in front of untrusted input
from the open internet without your own sandbox around it.

## Reporting

Report privately through GitHub's **Security → Report a vulnerability** on this
repository, which opens a draft advisory only the maintainer can see. Please
include the module (or the program that generates it), the tier it reproduces at
(`--tier 0`–`3`, since the interpreter and the three JIT levels are separate
code paths), and the platform.

If a report turns out to be an ordinary correctness bug rather than a security
one, it will be moved to a public issue and credited there — no harm done in
sending it privately first.

For bugs with no security dimension — a wrong result, a spec disagreement, a
build failure — open a normal issue.
