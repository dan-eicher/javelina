/* gen_eqsat_ops.c — the tier-3 e-node admission fence, from the spec table.
 *
 * One authority (the working analog's law: "One definition of purity, not
 * two"): which opcodes may intern as PURE e-nodes is generated from
 * spec/instructions.toml, never hand-listed in jav_eqsat.c. Emits
 * src/gen/jav_eqsat_ops.h.
 *
 * The predicate — the spec table's own facts, nothing hand-judged:
 *
 *   admitted  <=>  the opcode is a plain byte (the prefixed families are
 *                  outside the v1 vocabulary)
 *              AND `traps` is empty (a trapping op is not a value equality —
 *                  WASM §4.3's div/rem/trunc partiality)
 *              AND the name is in the i32./i64. families (integer arithmetic
 *                  is defined modulo 2^N, §4.3.2 — total, deterministic;
 *                  floats stay OPAQUE by D8: NaN payload nondeterminism
 *                  makes syntactic equality weaker than value equality, and
 *                  locals/globals/memory/refs/control are state, not value).
 *
 * The axiom file may mention fewer ops than this fence admits; it may never
 * mention more — jav_eqsat.c interns only what this table admits, so a rule
 * over an unadmitted op simply never matches.
 */
#include "toml/toml_doc.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* slurp(const char* path, int* len) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "gen_eqsat_ops: cannot open %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "gen_eqsat_ops: cannot read %s\n", path); exit(1);
    }
    b[n] = 0; fclose(f); *len = (int)n; return b;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: gen_eqsat_ops <instructions.toml> <out.h>\n");
        return 2;
    }
    bbq_arena ar; bbq_arena_init(&ar, 1 << 20);
    int len; char* src = slurp(argv[1], &len);
    toml_doc_t* doc = toml_parse(src, len, &ar);
    if (toml_doc_has_errors(doc)) {
        fprintf(stderr, "gen_eqsat_ops: %s: %s\n", argv[1],
                toml_doc_error_at(doc, 0)->message);
        return 1;
    }
    const toml_val_t* instrs = toml_tbl_get(toml_doc_root(doc), "instr");
    int n = toml_val_array_count(instrs);
    if (n == 0) { fprintf(stderr, "gen_eqsat_ops: no [[instr]] entries\n"); return 1; }

    uint8_t pure[256] = {0};
    char why[256][40] = {{0}};
    uint8_t pure_fd[256] = {0};
    char why_fd[256][40] = {{0}};
    int npure = 0, nfd = 0;
    for (int i = 0; i < n; i++) {
        const toml_tbl_t* t = toml_val_as_table(toml_val_array_at(instrs, i));
        if (!t) continue;
        const char* name;
        if (!toml_val_as_string(toml_tbl_get(t, "name"), &name)) continue;
        const toml_val_t* traps = toml_tbl_get(t, "traps");
        if (traps && toml_val_array_count(traps) > 0) continue;
        const toml_val_t* opv = toml_tbl_get(t, "opcode");
        int64_t op;
        if (toml_val_as_int(opv, &op)) {
            /* an unprefixed byte: the scalar integer families */
            if (op < 0 || op > 255) {
                fprintf(stderr, "gen_eqsat_ops: opcode %lld out of byte range\n",
                        (long long)op);
                return 1;
            }
            if (strncmp(name, "i32.", 4) != 0 && strncmp(name, "i64.", 4) != 0)
                continue;
            if (pure[op]) {
                fprintf(stderr, "gen_eqsat_ops: opcode 0x%02x listed twice (%s, %s)\n",
                        (unsigned)op, why[op], name);
                return 1;
            }
            pure[op] = 1;
            snprintf(why[op], sizeof why[op], "%s", name);
            npure++;
            continue;
        }
        /* [prefix, sub]: only the 0xFD vector family enters, and only its
         * VALUE ops — integer lanes and the v128 bitwise/const/shuffle set.
         * Float-lane arithmetic stays out for the same NaN ground as scalar
         * floats (§4.3's non-deterministic results); anything "relaxed" is
         * non-deterministic BY DESIGN; loads and stores are state and carry
         * traps anyway (belt and suspenders: the traps check above already
         * dropped them). */
        int cnt = toml_val_array_count(opv);
        if (cnt != 2) continue;
        int64_t pfx, sub;
        if (!toml_val_as_int(toml_val_array_at(opv, 0), &pfx)) continue;
        if (!toml_val_as_int(toml_val_array_at(opv, 1), &sub)) continue;
        if (pfx != 0xFD || sub < 0 || sub > 255) continue;
        if (strstr(name, "relaxed")) continue;
        if (strstr(name, "load") || strstr(name, "store")) continue;
        if (strncmp(name, "v128.", 5) != 0 && strncmp(name, "i8x16.", 6) != 0
            && strncmp(name, "i16x8.", 6) != 0 && strncmp(name, "i32x4.", 6) != 0
            && strncmp(name, "i64x2.", 6) != 0) continue;
        if (pure_fd[sub]) {
            fprintf(stderr, "gen_eqsat_ops: 0xFD %lld listed twice (%s, %s)\n",
                    (long long)sub, why_fd[sub], name);
            return 1;
        }
        pure_fd[sub] = 1;
        snprintf(why_fd[sub], sizeof why_fd[sub], "%s", name);
        nfd++;
    }
    if (npure == 0 || nfd == 0) {
        fprintf(stderr, "gen_eqsat_ops: the fence admitted nothing (%d scalar, "
                        "%d vector) — the predicate or the table is broken\n",
                npure, nfd);
        return 1;
    }

    FILE* o = fopen(argv[2], "w");
    if (!o) { fprintf(stderr, "gen_eqsat_ops: cannot write %s\n", argv[2]); return 1; }
    fprintf(o,
        "/* AUTO-GENERATED by tools/gen_eqsat_ops.c from spec/instructions.toml —\n"
        " * do not edit. The tier-3 e-node admission fence: 1 = this unprefixed\n"
        " * opcode is a total, deterministic integer op (traps == [], i32./i64.\n"
        " * family) and may intern as a pure e-node. Everything else is OPAQUE —\n"
        " * interned by identity, its pure subtrees still graphed. %d admitted. */\n"
        "#ifndef JAV_EQSAT_OPS_H\n#define JAV_EQSAT_OPS_H\n\n"
        "#include <stdint.h>\n\n"
        "static const uint8_t jav_eqsat_pure[256] = {\n", npure);
    for (int op = 0; op < 256; op++)
        if (pure[op]) fprintf(o, "    [0x%02x] = 1,  /* %s */\n", op, why[op]);
    fprintf(o,
        "};\n\n"
        "/* …and the 0xFD vector family's admitted SUB-opcodes (%d). The e-node\n"
        " * key for a prefixed op is JAV_EQ_OP_FD(sub) — above every byte and\n"
        " * every synthetic, so the two vocabularies cannot collide. */\n"
        "static const uint8_t jav_eqsat_pure_fd[256] = {\n", nfd);
    for (int s = 0; s < 256; s++)
        if (pure_fd[s]) fprintf(o, "    [0x%02x] = 1,  /* %s */\n", s, why_fd[s]);
    fprintf(o,
        "};\n\n"
        "#define JAV_EQ_OP_FD(sub) (0x10000 | (sub))\n\n"
        "/* One named key per admitted vector op, so the axiom file's TERMs and\n"
        " * the consumer's switch arms restate no number. */\n");
    for (int s = 0; s < 256; s++) {
        if (!pure_fd[s]) continue;
        char mac[48]; size_t mo = 0;
        for (const char* p = why_fd[s]; *p && mo + 1 < sizeof mac; p++) {
            char ch = *p;
            if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
            else if (!(ch >= '0' && ch <= '9') && !(ch >= 'A' && ch <= 'Z')) ch = '_';
            mac[mo++] = ch;
        }
        mac[mo] = 0;
        fprintf(o, "#define JAV_EQ_OP_%s JAV_EQ_OP_FD(0x%02x)\n", mac, s);
    }
    fprintf(o,
        "\n/* Synthetic e-node operators, disjoint from every opcode byte and\n"
        " * every JAV_EQ_OP_FD key. A local.get interns as (JAV_EQ_OP_LOCAL,\n"
        " * data = slot | version<<32; versions thread the region so a load\n"
        " * resolves against the store order it really saw) — and an\n"
        " * unadmitted subtree as (JAV_EQ_OP_OPAQUE, data = the jav_tnode_t\n"
        " * address): its own congruence class, no inputs, the tree keeps its\n"
        " * place. */\n"
        "#define JAV_EQ_OP_LOCAL  0x100\n"
        "#define JAV_EQ_OP_OPAQUE 0x101\n"
        "\n#endif /* JAV_EQSAT_OPS_H */\n");
    fclose(o);
    fprintf(stderr, "gen_eqsat_ops: %d scalar + %d vector opcode(s)\n", npure, nfd);
    return 0;
}
