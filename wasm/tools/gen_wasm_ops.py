#!/usr/bin/env python3
# Derives wasm/src/gen/wasm_ops.h — the EMIT-side opcode authority — from the
# committed spec/instructions.toml (itself spec-derived by gen_instr_toml.py).
#
# Why a separate header: opgen's opcodes.h is a DISPATCH view (256-entry tables
# keyed on the first byte), so the 0xFC/0xFD prefixed families collapse to a
# single OP_* — useless for emission, which needs each (prefix, opcode) pair.
# instructions.toml carries the full encoding uniformly (`opcode = 0x6A` or
# `[0xFC, 2]`), so this turns it into C the compiler's emitter links against:
# a WOP_* enum + a {prefix, opcode} table. Single source of truth for the bytes
# the compiler writes.
#
#   usage:  python3 tools/gen_wasm_ops.py spec/instructions.toml > src/gen/wasm_ops.h
import sys, re

try:
    import tomllib
    def load(path):
        with open(path, 'rb') as f:
            return tomllib.load(f)['instr']
except ModuleNotFoundError:
    # Minimal fallback for pre-3.11: the file is regular ([[instr]] blocks with
    # name + opcode), so a line scan suffices.
    def load(path):
        out, cur = [], None
        for ln in open(path):
            ln = ln.strip()
            if ln == '[[instr]]':
                cur = {}; out.append(cur)
            elif cur is not None and ln.startswith('name'):
                cur['name'] = re.search(r'"(.*)"', ln).group(1)
            elif cur is not None and ln.startswith('opcode'):
                rhs = ln.split('=', 1)[1].strip()
                if rhs.startswith('['):
                    a, b = re.findall(r'0x[0-9a-fA-F]+|\d+', rhs)
                    cur['opcode'] = [int(a, 0), int(b, 0)]
                else:
                    cur['opcode'] = int(rhs, 0)
        return out

def ident(name):
    return 'WOP_' + re.sub(r'[^0-9A-Za-z]', '_', name).upper()

def enc(opcode):
    if isinstance(opcode, list):
        return opcode[0], opcode[1]       # [prefix, sub]
    return 0, opcode                      # single byte (prefix 0 sentinel)

instrs = load(sys.argv[1] if len(sys.argv) > 1 else 'spec/instructions.toml')

# A few spec ops share a .wat TEXT mnemonic but have distinct binary encodings:
# the assembler picks the variant from the operand, but the compiler emits BY
# IDENTITY, so each variant needs its own constant — and burg will reach for the
# typed/null GC variants directly once the WASM-GC object model lands. Keep EVERY
# encoding (dropping one is a silent miscompile) and give each its real name, via
# an explicit, auditable policy keyed on the opcode byte. A NEW, unlisted dup
# hard-errors below rather than getting silently mangled.
DISAMBIG = {
    'select':   {0x1B: '',     0x1C: '_T'},     # select / select (result t)
    'ref.test': {0x14: '',     0x15: '_NULL'},  # (ref ht) / (ref null ht)
    'ref.cast': {0x16: '',     0x17: '_NULL'},
}
used = set()
rows = []
for it in instrs:
    p, op = enc(it['opcode'])
    name = it['name']
    if name in DISAMBIG:
        if op not in DISAMBIG[name]:
            sys.exit(f"gen_wasm_ops: {name} opcode 0x{op:02X} missing from DISAMBIG policy")
        wid = ident(name) + DISAMBIG[name][op]
    else:
        wid = ident(name)
    if wid in used:
        sys.exit(f"gen_wasm_ops: unhandled duplicate mnemonic {name} (0x{op:02X}) "
                 f"→ {wid}; add it to DISAMBIG")
    used.add(wid)
    rows.append((wid, name, p, op))

w = sys.stdout.write
w("/* AUTO-GENERATED from spec/instructions.toml by tools/gen_wasm_ops.py — do not edit.\n")
w(" * The EMIT-side opcode authority: each WASM instruction's full byte encoding\n")
w(" * (prefix + opcode), single-byte and 0xFC/0xFD-prefixed uniformly. */\n")
w("#ifndef WASM_OPS_H\n#define WASM_OPS_H\n\n#include <stdint.h>\n\n")
w("typedef enum {\n")
for wid, name, _, _ in rows:
    w(f"    {wid},  /* {name} */\n")
w("    WOP__COUNT\n} wasm_op_t;\n\n")
w("/* prefix == 0 → a single-byte opcode; else emit `prefix` then `opcode` as uleb. */\n")
w("typedef struct { uint8_t prefix; uint32_t opcode; } wasm_op_enc_t;\n\n")
w("static const wasm_op_enc_t wasm_op_enc[WOP__COUNT] = {\n")
for wid, _, p, op in rows:
    w(f"    [{wid}] = {{ 0x{p:02X}, 0x{op:02X} }},\n")
w("};\n\n#endif /* WASM_OPS_H */\n")
