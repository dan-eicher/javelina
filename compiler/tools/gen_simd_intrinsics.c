/* gen_simd_intrinsics.c — instructions.toml -> the javelina.simd intrinsic
 * surface: the RTL stub classes (lib/javelina/simd/) and the compiler's
 * recognition table (src/gen/simd_intrinsics.h).
 *
 * The ONE authority is the spec table, read through the ONE toml reader (the
 * bbqc-generated parser + toml_doc, the same pair gen_trap_reasons links) —
 * never a second parser. Naming is mechanical: the spec name splits at the
 * dot into class/method, spelling kept verbatim (a Java KEYWORD gains one
 * trailing underscore — exactly `v128.const` -> `V128.const_`). Parameter
 * order is stack operands first, then immediates; 16-byte immediates ride
 * two longs, little-endian.
 *
 * Every row must land in exactly one family or this generator exits 1 — no
 * silent caps. The memarg/memlane rows are `V128.*` methods over the I/O-floor
 * linear memory (the spec class of all 22 is `v128`); their `align` column is
 * CARRIED into the table (the burg emits it verbatim — never recomputed), and
 * a memlane row's lane count is 128/width, width read from the mnemonic. */
#include "toml/toml_doc.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Family codes — must match sir.asdl's 19 Simd* nodes and the ddcg steering. */
enum { F_BIN = 1, F_UN, F_SHIFT, F_TERN, F_TESTI,
       F_SPLATI, F_SPLATL, F_SPLATF, F_SPLATD,
       F_EXTRACTI, F_EXTRACTL, F_EXTRACTF, F_EXTRACTD,
       F_REPLACEI, F_REPLACEL, F_REPLACEF, F_REPLACED,
       F_CONST, F_SHUFFLE,
       F_MEMLOAD, F_MEMSTORE, F_MEMLOADLANE, F_MEMSTORELANE,   /* 20-23: v128 memory */
       F_MEMLOADI, F_MEMLOADL, F_MEMLOADF, F_MEMLOADD,         /* 24-27: scalar loads */
       F_MEMSTOREI, F_MEMSTOREL, F_MEMSTOREF, F_MEMSTORED,     /* 28-31: scalar stores */
       F_MEMSIZE, F_MEMGROW, F_MEMFILL, F_MEMCOPY };           /* 32-35: memory admin */

typedef struct {
    char method[48];
    char wop[64];
    int  family;
    int  nargs;
    int  lanes;
    int  align;         /* memarg/memlane rows: the toml align column; else 0 */
    char sig[128];      /* the finished Java parameter list */
    char ret[8];        /* Java return type */
    /* One char per parameter — V=V128, I=int, J=long, F=float, D=double. The
     * stamp that binds a stub to its opcode matches on THIS, not on the name:
     * a name does not identify a method (§8.4.7), and the moment the API grows
     * a convenience overload (an `add` taking a scalar beside one taking a
     * vector) a name-keyed bind silently attaches the wrong opcode. */
    char ptypes[32];
} row_t;

/* The ptypes char for a toml operand type. */
static char pty(const char* t) {
    if (!strcmp(t, "v128")) return 'V';
    if (!strcmp(t, "i32"))  return 'I';
    if (!strcmp(t, "i64"))  return 'J';
    if (!strcmp(t, "f32"))  return 'F';
    if (!strcmp(t, "f64"))  return 'D';
    if (!strncmp(t, "at", 2)) return 'I';   /* memory32 address */
    return '?';
}

/* Fixed class universe — a row landing outside it is a hard error. Memory-
 * surface rows (shape memarg/memlane + memory.size/grow/fill/copy) route to
 * `Mem` by SHAPE, not by the name's dot-prefix (their prefixes are the value
 * types i32/i64/f32/f64/v128/memory); Mem methods keep the FULL spec mnemonic
 * with '.'→'_' (i32_load, v128_load8_lane, memory_grow) — greppable both
 * ways, collision-free. Mem's spec column is "!" so prefix matching can never
 * hit it. */
static const struct { const char* spec; const char* java; int lanes; } CLASSES[] = {
    { "v128",  "V128",  0 }, { "i8x16", "I8x16", 16 }, { "i16x8", "I16x8", 8 },
    { "i32x4", "I32x4", 4 }, { "i64x2", "I64x2", 2 },
    { "f32x4", "F32x4", 4 }, { "f64x2", "F64x2", 2 },
    { "!",     "Mem",   0 },
};
#define NCLASSES 8
#define MEM_CI   7
static row_t g_rows[NCLASSES][80];
static int   g_nrows[NCLASSES];

static const char* jty(const char* t) {
    if (!strcmp(t, "v128")) return "V128";
    if (!strcmp(t, "i32"))  return "int";
    if (!strcmp(t, "i64"))  return "long";
    if (!strcmp(t, "f32"))  return "float";
    if (!strcmp(t, "f64"))  return "double";
    /* address types — the module memory is a memory32 (at1/at2 = copy's
     * two-memory form; ONE memory here, both are int addresses) */
    if (!strncmp(t, "at", 2)) return "int";
    return NULL;
}

/* The keywords a spec mnemonic could collide with (only `const` does today;
 * the rule is stated once, mechanically, so a future spec word stays safe). */
static int is_java_keyword(const char* s) {
    static const char* kw[] = { "const", "goto", "new", "int", "long", "float",
                                "double", "byte", "short", "char", "boolean",
                                "void", "class", "final", "static", NULL };
    for (int i = 0; kw[i]; i++) if (!strcmp(s, kw[i])) return 1;
    return 0;
}

static char* slurp(const char* path, int* len) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "gen_simd_intrinsics: cannot open %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "gen_simd_intrinsics: cannot read %s\n", path); exit(1);
    }
    b[n] = 0; fclose(f); *len = (int)n; return b;
}

/* "[v128 i32] -> [v128]" -> in-token list + the out token ("" if none). */
static int parse_type(const char* ty, char tin[4][8], char* tout) {
    const char* arrow = strstr(ty, "->");
    if (!arrow) return -1;
    int nin = 0;
    for (const char* p = ty; p < arrow; p++) {
        if (*p == '[' || *p == ']' || *p == ' ') continue;
        if (nin == 4) return -1;
        int o = 0;
        while (p < arrow && *p != ' ' && *p != ']' && o + 1 < 8) tin[nin][o++] = *p++;
        tin[nin][o] = 0; nin++;
    }
    tout[0] = 0;
    for (const char* p = arrow + 2; *p; p++) {
        if (*p == '[' || *p == ']' || *p == ' ') continue;
        int o = 0;
        while (*p && *p != ' ' && *p != ']' && o + 1 < 8) tout[o++] = *p++;
        tout[o] = 0; break;
    }
    return nin;
}

static int classify(const char* shape, int nin, char tin[4][8], const char* tout,
                    int* needs_lane) {
    *needs_lane = 0;
    if (!strcmp(shape, "memarg")) {
        if (tout[0]) {                                      /* a load, by result type */
            if (!strcmp(tout, "v128")) return F_MEMLOAD;
            if (!strcmp(tout, "i32"))  return F_MEMLOADI;
            if (!strcmp(tout, "i64"))  return F_MEMLOADL;
            if (!strcmp(tout, "f32"))  return F_MEMLOADF;
            if (!strcmp(tout, "f64"))  return F_MEMLOADD;
            return -1;
        }
        if (nin == 2) {                                     /* a store, by value type */
            if (!strcmp(tin[1], "v128")) return F_MEMSTORE;
            if (!strcmp(tin[1], "i32"))  return F_MEMSTOREI;
            if (!strcmp(tin[1], "i64"))  return F_MEMSTOREL;
            if (!strcmp(tin[1], "f32"))  return F_MEMSTOREF;
            if (!strcmp(tin[1], "f64"))  return F_MEMSTORED;
        }
        return -1;
    }
    if (!strcmp(shape, "memlane")) {
        *needs_lane = 1;
        return !strcmp(tout, "v128") ? F_MEMLOADLANE : F_MEMSTORELANE;
    }
    if (!strcmp(shape, "lane")) {
        *needs_lane = 1;
        if (!strcmp(tout, "v128")) {                      /* replace_lane, by scalar */
            const char* s = tin[1];
            if (!strcmp(s, "i32")) return F_REPLACEI;
            if (!strcmp(s, "i64")) return F_REPLACEL;
            if (!strcmp(s, "f32")) return F_REPLACEF;
            if (!strcmp(s, "f64")) return F_REPLACED;
            return -1;
        }
        if (!strcmp(tout, "i32")) return F_EXTRACTI;
        if (!strcmp(tout, "i64")) return F_EXTRACTL;
        if (!strcmp(tout, "f32")) return F_EXTRACTF;
        if (!strcmp(tout, "f64")) return F_EXTRACTD;
        return -1;
    }
    if (!strcmp(shape, "v128")) return nin == 0 ? F_CONST : F_SHUFFLE;
    /* shape == "none" */
    if (nin == 3) return F_TERN;
    if (nin == 2 && !strcmp(tin[1], "v128")) return F_BIN;
    if (nin == 2 && !strcmp(tin[1], "i32"))  return F_SHIFT;
    if (nin == 1 && !strcmp(tin[0], "v128"))
        return !strcmp(tout, "v128") ? F_UN : F_TESTI;
    if (nin == 1 && !strcmp(tout, "v128")) {
        if (!strcmp(tin[0], "i32")) return F_SPLATI;
        if (!strcmp(tin[0], "i64")) return F_SPLATL;
        if (!strcmp(tin[0], "f32")) return F_SPLATF;
        if (!strcmp(tin[0], "f64")) return F_SPLATD;
    }
    return -1;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <instructions.toml> <lib/javelina/simd dir> <out.h>\n",
                argv[0]);
        return 1;
    }
    bbq_arena ar; bbq_arena_init(&ar, 0);
    int len; char* src = slurp(argv[1], &len);
    toml_doc_t* doc = toml_parse(src, len, &ar);
    if (toml_doc_has_errors(doc)) {
        fprintf(stderr, "gen_simd_intrinsics: %s: %s\n", argv[1],
                toml_doc_error_at(doc, 0)->message);
        return 1;
    }
    const toml_val_t* instrs = toml_tbl_get(toml_doc_root(doc), "instr");
    int n = toml_val_array_count(instrs);
    int total = 0, fam_counts[F_MEMCOPY + 1] = {0};

    for (int i = 0; i < n; i++) {
        const toml_tbl_t* t = toml_val_as_table(toml_val_array_at(instrs, i));
        if (!t) continue;
        const char* shape = "none";
        toml_val_as_string(toml_tbl_get(t, "shape"), &shape);
        const char* name = NULL;
        toml_val_as_string(toml_tbl_get(t, "name"), &name);
        /* The surface: every 0xFD (SIMD) row, every memarg/memlane row (the
         * scalar loads/stores included), and the four memory admin ops.
         * memory.init needs a data segment — not part of the Mem surface. */
        const toml_val_t* ov = toml_tbl_get(t, "opcode");
        int64_t pfx, sub;
        int is_fd = toml_val_type(ov) == TOML_VT_ARRAY && toml_val_array_count(ov) == 2 &&
                    toml_val_as_int(toml_val_array_at(ov, 0), &pfx) &&
                    toml_val_as_int(toml_val_array_at(ov, 1), &sub) && pfx == 0xFD;
        int is_mem_shape = !strcmp(shape, "memarg") || !strcmp(shape, "memlane");
        int is_mem_admin = name && (!strcmp(name, "memory.size") || !strcmp(name, "memory.grow") ||
                                    !strcmp(name, "memory.fill") || !strcmp(name, "memory.copy"));
        if (!is_fd && !is_mem_shape && !is_mem_admin) continue;
        const char* ty = NULL;
        if (!name || !toml_val_as_string(toml_tbl_get(t, "type"), &ty)) {
            fprintf(stderr, "gen_simd_intrinsics: row %d lacks name/type\n", i);
            return 1;
        }

        char tin[4][8]; char tout[8];
        int nin = parse_type(ty, tin, tout);
        int needs_lane = 0;
        int fam;
        if (is_mem_admin) {                     /* by name — their shapes are idx/idx2 */
            fam = !strcmp(name, "memory.size") ? F_MEMSIZE
                : !strcmp(name, "memory.grow") ? F_MEMGROW
                : !strcmp(name, "memory.fill") ? F_MEMFILL : F_MEMCOPY;
        } else {
            fam = nin < 0 ? -1 : classify(shape, nin, tin, tout, &needs_lane);
        }
        if (fam < 0 || nin < 0) {
            fprintf(stderr, "gen_simd_intrinsics: unclassifiable row %s: %s %s\n",
                    name, shape, ty);
            return 1;
        }

        /* Class routing: memory-surface rows → Mem, method = the FULL spec
         * mnemonic '.'→'_'; value rows split at the dot as before. */
        int ci;
        char method[48];
        if (is_mem_shape || is_mem_admin) {
            ci = MEM_CI;
            int o = 0;
            for (const char* p = name; *p && o + 1 < (int)sizeof method; p++)
                method[o++] = (*p == '.') ? '_' : *p;
            method[o] = 0;
        } else {
            const char* dot = strchr(name, '.');
            if (!dot) { fprintf(stderr, "gen_simd_intrinsics: undotted name %s\n", name); return 1; }
            ci = -1;
            for (int c = 0; c < NCLASSES; c++)
                if (!strncmp(name, CLASSES[c].spec, (size_t)(dot - name)) &&
                    CLASSES[c].spec[dot - name] == 0) { ci = c; break; }
            if (ci < 0) { fprintf(stderr, "gen_simd_intrinsics: unknown class in %s\n", name); return 1; }
            snprintf(method, sizeof method, "%s%s", dot + 1,
                     is_java_keyword(dot + 1) ? "_" : "");
        }

        if (g_nrows[ci] == 80) { fprintf(stderr, "gen_simd_intrinsics: class overflow\n"); return 1; }
        row_t* r = &g_rows[ci][g_nrows[ci]++];
        snprintf(r->method, sizeof r->method, "%s", method);
        /* WOP_* name: the spec name upper-cased, '.' -> '_'. */
        { int o = 0; r->wop[o++] = 'W'; r->wop[o++] = 'O'; r->wop[o++] = 'P'; r->wop[o++] = '_';
          for (const char* p = name; *p && o + 1 < (int)sizeof r->wop; p++)
              r->wop[o++] = *p == '.' ? '_' : (char)((*p >= 'a' && *p <= 'z') ? *p - 32 : *p);
          r->wop[o] = 0; }
        r->family = fam;
        r->lanes  = CLASSES[ci].lanes;
        r->align  = 0;
        if (is_mem_shape) {                     /* memarg/memlane: CARRY the align column */
            int64_t al;
            if (!toml_val_as_int(toml_tbl_get(t, "align"), &al)) {
                fprintf(stderr, "gen_simd_intrinsics: %s lacks the align column\n", name);
                return 1;
            }
            r->align = (int)al;
        }
        if (fam == F_MEMLOADLANE || fam == F_MEMSTORELANE) {
            /* load/storeN_lane: lane count = 128/N, N read off the mnemonic
             * AFTER the load/store word (the v128_ prefix has digits too). */
            const char* d = strstr(r->method, "load");
            if (d) d += 4; else { d = strstr(r->method, "store"); if (d) d += 5; }
            int w = d ? atoi(d) : 0;
            if (w != 8 && w != 16 && w != 32 && w != 64) {
                fprintf(stderr, "gen_simd_intrinsics: no lane width in %s\n", name);
                return 1;
            }
            r->lanes = 128 / w;
        }
        snprintf(r->ret, sizeof r->ret, "%s", tout[0] ? jty(tout) : "void");
        /* Parameter list: stack operands a, b, c…, then the immediates. */
        { int o = 0, argn = 0, p = 0;
          if (fam != F_CONST)
              for (int k = 0; k < nin; k++) {
                  o += snprintf(r->sig + o, sizeof r->sig - (size_t)o, "%s%s %c",
                                k ? ", " : "", jty(tin[k]), 'a' + argn);
                  r->ptypes[p++] = pty(tin[k]);
                  argn++;
              }
          if (fam == F_CONST) {
              o += snprintf(r->sig + o, sizeof r->sig - (size_t)o, "long lo, long hi");
              r->ptypes[p++] = 'J'; r->ptypes[p++] = 'J'; argn += 2;
          } else if (fam == F_SHUFFLE) {
              o += snprintf(r->sig + o, sizeof r->sig - (size_t)o, ", long maskLo, long maskHi");
              r->ptypes[p++] = 'J'; r->ptypes[p++] = 'J'; argn += 2;
          } else if (needs_lane) {
              o += snprintf(r->sig + o, sizeof r->sig - (size_t)o, ", int lane");
              r->ptypes[p++] = 'I'; argn++;
          }
          r->ptypes[p] = '\0';
          r->nargs = argn; }
        fam_counts[fam]++; total++;
    }

    fprintf(stderr, "gen_simd_intrinsics: %d ops across %d classes\n", total, NCLASSES);
    static const char* fam_names[] = { "", "BIN", "UN", "SHIFT", "TERN", "TESTI",
        "SPLATI", "SPLATL", "SPLATF", "SPLATD", "EXTRACTI", "EXTRACTL", "EXTRACTF",
        "EXTRACTD", "REPLACEI", "REPLACEL", "REPLACEF", "REPLACED", "CONST", "SHUFFLE",
        "MEMLOAD", "MEMSTORE", "MEMLOADLANE", "MEMSTORELANE",
        "MEMLOADI", "MEMLOADL", "MEMLOADF", "MEMLOADD",
        "MEMSTOREI", "MEMSTOREL", "MEMSTOREF", "MEMSTORED",
        "MEMSIZE", "MEMGROW", "MEMFILL", "MEMCOPY" };
    for (int f = 1; f <= F_MEMCOPY; f++)
        fprintf(stderr, "  %-12s %d\n", fam_names[f], fam_counts[f]);
    /* 256 SIMD (234 value + 22 v128 memory) + 23 scalar memarg + 4 memory
     * admin ops = 283. A changed toml reconciles HERE first. */
    if (total != 283) {
        fprintf(stderr, "gen_simd_intrinsics: %d ops != the spec's 283 — the "
                "toml changed; reconcile the family table first\n", total);
        return 1;
    }

    /* ── the recognition table ── */
    FILE* o = fopen(argv[3], "wb");
    if (!o) { fprintf(stderr, "gen_simd_intrinsics: cannot write %s\n", argv[3]); return 1; }
    fprintf(o,
"/* AUTO-GENERATED by tools/gen_simd_intrinsics.c from spec instructions.toml.\n"
" * Do not edit. */\n"
"#ifndef SIMD_INTRINSICS_H\n#define SIMD_INTRINSICS_H\n"
"#include \"gen/wasm_ops.h\"\n"
"typedef struct { const char* cls; const char* method; int family;\n"
"                 int wop; int nargs; int lanes; int align;\n"
"                 const char* ptypes; } simd_intrinsic_t;\n"
"/* ptypes: one char per parameter (V=V128 I=int J=long F=float D=double).\n"
" * The bind from stub to opcode matches class + method + THIS, because a name\n"
" * does not identify a method (JLS 8.4.7) and an overload would otherwise take\n"
" * the first entry's opcode. */\n"
"/* families: 1 Bin 2 Un 3 Shift 4 Tern 5 TestI 6-9 Splat(I/L/F/D)\n"
" * 10-13 Extract(I/L/F/D) 14-17 Replace(I/L/F/D) 18 Const 19 Shuffle\n"
" * 20 MemLoad 21 MemStore 22 MemLoadLane 23 MemStoreLane (v128 memory)\n"
" * 24-27 MemLoad(I/L/F/D) 28-31 MemStore(I/L/F/D) (scalar memory)\n"
" * 32 MemSize 33 MemGrow 34 MemFill 35 MemCopy (memory admin).\n"
" * align = the toml align column, emitted verbatim in the memarg. */\n"
"static const simd_intrinsic_t simd_intrinsics[] = {\n");
    int emitted = 0;
    for (int c = 0; c < NCLASSES; c++)
        for (int r = 0; r < g_nrows[c]; r++) {
            const row_t* w = &g_rows[c][r];
            fprintf(o, "    { \"%s\", \"%s\", %d, %s, %d, %d, %d, \"%s\" },\n",
                    CLASSES[c].java, w->method, w->family, w->wop, w->nargs, w->lanes,
                    w->align, w->ptypes);
            emitted++;
        }
    fprintf(o, "};\n#define SIMD_INTRINSIC_COUNT %d\n#endif /* SIMD_INTRINSICS_H */\n",
            emitted);
    fclose(o);

    /* ── the RTL stub classes ── */
    for (int c = 0; c < NCLASSES; c++) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s.java", argv[2], CLASSES[c].java);
        FILE* j = fopen(path, "wb");
        if (!j) { fprintf(stderr, "gen_simd_intrinsics: cannot write %s\n", path); return 1; }
        fprintf(j,
"package javelina.simd;\n\n"
"// AUTO-GENERATED by tools/gen_simd_intrinsics.c from the spec's\n"
"// instructions.toml — do not edit. Every method is a compiler\n"
"// intrinsic: sema stamps it, the ddcg lowers it to its wasm opcode\n"
"// inline, and it is NEVER a host import. `native` is the stamped-\n"
"// intrinsic convention (the Math.sqrt pattern).\n");
        if (!strcmp(CLASSES[c].java, "V128"))
            fprintf(j,
"// V128 is also THE v128 VALUE type: sema maps this class to the\n"
"// v128 value width at every use site — never a reference, never\n"
"// null, no ==, no Object conversion.\n");
        else if (c == MEM_CI)
            fprintf(j,
"// Mem: the module's linear memory. Methods keep the full wasm\n"
"// mnemonic ('.'->'_'); addresses are byte offsets (int); memory_grow\n"
"// takes/returns 64 KiB pages (old size, or -1 on failure — the wasm\n"
"// contract, surfaced as the return value). The low pages carry the\n"
"// runtime's transient host-I/O staging traffic DURING library calls;\n"
"// user data and I/O never run concurrently (single-threaded), so the\n"
"// memory is one flat user-managed space between calls.\n");
        else
            fprintf(j, "// %s: the %s-shaped lane view of a V128.\n",
                    CLASSES[c].java, CLASSES[c].spec);
        fprintf(j, "public final class %s {\n    private %s() {}\n\n",
                CLASSES[c].java, CLASSES[c].java);
        for (int r = 0; r < g_nrows[c]; r++)
            fprintf(j, "    public static native %s %s(%s);\n",
                    g_rows[c][r].ret, g_rows[c][r].method, g_rows[c][r].sig);
        if (c == MEM_CI)
            fprintf(j, "%s",
"\n"
"    // Bounce helpers: GC arrays <-> linear memory, plain Java over the ops\n"
"    // (E8.3). Bounds come from the ops' own guards + the array checks.\n"
"    public static void copyIn(byte[] src, int off, int len, int addr) {\n"
"        for (int i = 0; i < len; i++) i32_store8(addr + i, src[off + i]);\n"
"    }\n"
"    public static void copyOut(int addr, byte[] dst, int off, int len) {\n"
"        for (int i = 0; i < len; i++) dst[off + i] = (byte) i32_load8_u(addr + i);\n"
"    }\n"
"    public static void copyIn(javelina.simd.V128[] src, int off, int len, int addr) {\n"
"        for (int i = 0; i < len; i++) v128_store(addr + i * 16, src[off + i]);\n"
"    }\n"
"    public static void copyOut(int addr, javelina.simd.V128[] dst, int off, int len) {\n"
"        for (int i = 0; i < len; i++) dst[off + i] = v128_load(addr + i * 16);\n"
"    }\n");
        fprintf(j, "}\n");
        fclose(j);
    }
    fprintf(stderr, "gen_simd_intrinsics: wrote %d classes + %d-row table -> %s\n",
            NCLASSES, emitted, argv[3]);
    free(src);
    bbq_arena_free(&ar);
    return 0;
}
