#!/usr/bin/env python3
# Derives wasm/src/gen/wat_mnemonics.h — the TEXT-side mnemonic table — from the
# committed spec/instructions.toml (itself spec-derived by gen_instr_toml.py).
#
# Why: the .wat reader resolves "i32.add" -> (prefix, opcode, shape, align,
# operand index-spaces). That table used to be read from instructions.toml AT
# RUNTIME, which is why `water` carried an -i/--instrs flag, why ten test
# binaries carried a file dependency, and why the VM gates had to run with
# `cd test &&` for the relative path to resolve. The same TOML is already
# consumed at BUILD time twice (gen_wasm_ops.py -> the compiler's wasm_ops.h,
# gen_trap_reasons -> jav_trap_reason.h); this makes the third consumer match,
# so `water` is a standalone binary with no data file to find.
#
# The one legitimate runtime read that remains is test_instr, which walks the
# table as DATA (every instruction: synthesize -> decode -> round-trip).
#
#   usage:  python3 tools/gen_wat_mnemonics.py spec/instructions.toml > src/gen/wat_mnemonics.h
import sys

try:
    import tomllib
    def load(path):
        with open(path, 'rb') as f:
            return tomllib.load(f)['instr']
except ModuleNotFoundError:
    sys.exit("gen_wat_mnemonics: needs Python 3.11+ (tomllib)")

# Immediate shapes, in the order the .wat grammar's WAT_SH_* enum uses. Must stay
# in step with the N[] list in spec/wat.peg — the generator asserts membership so
# a new shape in the toml fails the build rather than silently resolving to -1.
SHAPES = ["none","idx","idx2","i32","i64","f32","f64","heap","lane",
          "v128","memarg","memlane","brtable","selectt","broncast","block","if","trytable"]

# An `operands` index-kind token -> the module index space a $id resolves against,
# or -1 for kinds that are function-local, per-type, or not a $id at all.
SPACES = {"funcidx":"SP_FUNC","typeidx":"SP_TYPE","tableidx":"SP_TABLE","memidx":"SP_MEM",
          "globalidx":"SP_GLOBAL","tagidx":"SP_TAG","elemidx":"SP_ELEM","dataidx":"SP_DATA",
          "localidx":"SP_LOCAL","labelidx":"SP_LABEL"}

def space(ops, i):
    if i < len(ops) and ops[i] in SPACES:
        return SPACES[ops[i]]
    return "-1"

def main(path):
    instrs = load(path)
    out = []
    w = out.append
    w("/* AUTO-GENERATED from spec/instructions.toml by tools/gen_wat_mnemonics.py — do not edit.")
    w(" * The TEXT-side mnemonic authority: name -> (prefix, opcode, shape, align,")
    w(" * operand index spaces). Built here so the .wat reader needs no data file at")
    w(" * runtime and `water` is a standalone binary. */")
    w("#ifndef WAT_MNEMONICS_H")
    w("#define WAT_MNEMONICS_H")
    w("")
    w("#include <stdint.h>")
    w("")
    w("typedef struct {")
    w("    const char* name;")
    w("    uint8_t     prefix;   /* 0 for single-byte, else 0xFC/0xFD */")
    w("    uint32_t    op;")
    w("    int16_t     shape;    /* index into the WAT_SH_* order */")
    w("    int16_t     align;    /* log2 natural alignment; -1 if not a memory op */")
    w("    int16_t     sp0, sp1; /* index space of each idx/idx2 operand; -1 if none */")
    w("} wat_mnemonic_t;")
    w("")
    w("static const wat_mnemonic_t wat_mnemonics[] = {")
    n = 0
    for it in instrs:
        name = it.get("name")
        if not name:
            continue
        sh = it.get("shape", "none")
        if sh not in SHAPES:
            sys.exit(f"gen_wat_mnemonics: unknown shape {sh!r} for {name!r} — "
                     f"add it to SHAPES here AND to N[] in spec/wat.peg")
        opc = it["opcode"]
        if isinstance(opc, list):
            prefix, op = opc[0], opc[1]
        else:
            prefix, op = 0, opc
        align = it.get("align", -1)
        ops = it.get("operands", []) or []
        w(f'    {{ "{name}", 0x{prefix:02X}, {op}, {SHAPES.index(sh)}, {align}, '
          f'{space(ops,0)}, {space(ops,1)} }},')
        n += 1
    w("};")
    w("")
    w(f"#define WAT_MNEMONIC_COUNT {n}")
    w("")
    w("#endif /* WAT_MNEMONICS_H */")
    print("\n".join(out))

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: gen_wat_mnemonics.py spec/instructions.toml > src/gen/wat_mnemonics.h")
    main(sys.argv[1])
