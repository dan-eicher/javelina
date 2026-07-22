/* javelina_test.h — the one test convention, shared by the compiler and the VM.
 *
 * A javelina test is a plain C `main` that returns 0 to pass and non-zero to
 * fail. There is no framework, no registration, no runner protocol: the exit
 * code IS the verdict, and that is what every Makefile gate reads. This header
 * does not change that contract — it only removes the copy of the same counter
 * and the same printf that had been pasted into a hundred files.
 *
 * Two assertion styles, both counting into the same per-file tally:
 *
 *   CHECK(cond, "label")        Quiet on success; prints "  FAIL  label" on
 *                               failure. For suites with many assertions where
 *                               only the failures are worth reading.
 *
 *   CHECK_CASE("label", cond)   Prints "  label ... [PASS|FAIL]" either way.
 *                               For suites whose run log doubles as a coverage
 *                               ledger (most of the VM's gates read this way).
 *
 *   CHECK_EXPR(cond)            Quiet on success; on failure prints the source
 *                               text of the condition. For mechanical checks
 *                               not worth naming.
 *
 * End `main` with:  return TEST_SUMMARY("suite name");
 *
 * The counters are per translation unit. A test built from two .c files (the
 * conformance runner is the one that is) gets an independent tally in each, so
 * each TU reports and returns its own verdict — which is the existing behaviour,
 * not a new constraint.
 */
#ifndef JAVELINA_TEST_H
#define JAVELINA_TEST_H

#include <stdio.h>

/* Define JT_REPORT_RSS before including this header to have the summary state
 * the suite's peak resident set. A suite whose cost is invisible is a suite
 * whose cost regresses quietly; the ones that build the whole prelude per case
 * turn this on so the number is in their own output rather than something you
 * have to remember to measure from outside. */
#ifdef JT_REPORT_RSS
#include <sys/resource.h>
/* A function, not a statement macro: TEST_SUMMARY is an expression, so this
 * has to be usable inside one. */
static void jt_rss_line(void) {
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) != 0) return;
    long kb = (long)ru.ru_maxrss;       /* Linux reports kilobytes */
    printf("  peak RSS %ld.%03ld GB\n",
           kb / 1048576L, (kb % 1048576L) * 1000L / 1048576L);
}
#define JT_RSS_LINE() jt_rss_line()
#else
#define JT_RSS_LINE() ((void)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define JT_UNUSED __attribute__((unused))
#else
#define JT_UNUSED
#endif

static int jt_fails  JT_UNUSED = 0;   /* assertions that failed */
static int jt_checks JT_UNUSED = 0;   /* assertions evaluated — a suite that */
                                      /* runs zero checks is a suite that is  */
                                      /* not testing anything, and says so.   */

/* Define JT_VERBOSE before including this header to make CHECK announce its
 * successes as well as its failures. A suite that takes minutes wants a
 * progress log; a suite that takes a second wants silence. */
#ifdef JT_VERBOSE
#define CHECK(cond, label)                                                     \
    do {                                                                       \
        jt_checks++;                                                           \
        if (!(cond)) { printf("  FAIL  %s\n", (label)); jt_fails++; }           \
        else         { printf("  ....  %s\n", (label)); }                      \
    } while (0)
#else
#define CHECK(cond, label)                                                     \
    do {                                                                       \
        jt_checks++;                                                           \
        if (!(cond)) { printf("  FAIL  %s\n", (label)); jt_fails++; }           \
    } while (0)
#endif

#define CHECK_CASE(label, cond)                                                \
    do {                                                                       \
        int jt_ok_ = (cond) ? 1 : 0;                                           \
        jt_checks++;                                                           \
        if (!jt_ok_) jt_fails++;                                               \
        printf("  %-46s [%s]\n", (label), jt_ok_ ? "PASS" : "FAIL");           \
    } while (0)

#define CHECK_EXPR(cond)                                                       \
    do {                                                                       \
        jt_checks++;                                                           \
        if (!(cond)) { printf("  FAIL  %s\n", #cond); jt_fails++; }             \
    } while (0)

/* Record a failure whose message the caller has already printed itself. For the
 * error paths that carry more context than a fixed label can (a filename, a
 * parser position), keep the existing printf and follow it with this. */
#define TEST_FAILED()                                                          \
    do { jt_checks++; jt_fails++; } while (0)

/* Print the verdict and yield the process exit code. A suite that evaluated no
 * assertions at all fails: silence from a test that never checked anything is
 * indistinguishable from success, and that is the one failure mode a plain
 * exit-code harness cannot otherwise catch. */
#define TEST_SUMMARY(name)                                                     \
    (JT_RSS_LINE(),                                                            \
     jt_checks == 0                                                            \
        ? (printf("\n%s: NO CHECKS RAN\n", (name)), 1)                         \
        : jt_fails                                                             \
            ? (printf("\n%s: %d of %d checks FAILED\n",                        \
                      (name), jt_fails, jt_checks), 1)                         \
            : (printf("\n%s: OK (%d checks)\n", (name), jt_checks), 0))

#endif /* JAVELINA_TEST_H */
