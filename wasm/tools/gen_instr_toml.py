#!/usr/bin/env python3
# ONE-TIME tool (NOT part of the build). Derives wasm/spec/instructions.toml —
# the opcode table (name + immediate shape + typing + traps) — from authoritative
# spec sources, joined by OPCODE:
#   * the reference decoder interpreter/binary/decode.ml  -> opcode + immediate shape
#   * the spec's §7.10 "Index of Instructions" (WebAssembly.pdf) -> the canonical
#     TEXT mnemonic (decode.ml only has mangled OCaml constructor names like
#     `i32_add`/`br_table`; the text format needs `i32.add` — the underscore↔dot
#     mapping is NOT mechanical, e.g. memory.atomic.notify, so we take real names
#     from §7.10 and join on the opcode, which is unambiguous) and the Type
#     column of the same table -> `type`, the per-instruction typing in the
#     spec's own notation.
#   * the reference interpreter's execution sources -> `traps`: each eval.ml
#     step arm is scanned for the `Trapping` messages it can produce; numeric
#     ops resolve through eval_num.ml's dispatch tables to the actual `raise`
#     sites in ixx.ml/fxx.ml/convert.ml; generic `with exn ->` catches resolve
#     the table.ml/memory.ml calls they guard; instructions that reduce to
#     other instructions (fill/copy/init, return_call*) inherit theirs.
#   * valid.ml's typing arrows cross-check the §7.10 Type column wherever the
#     arrow is closed-form (the concretely-typed const/test/unary/binary/
#     compare and v128 families) — two independent sources agreeing.
# The vendored .ml inputs are read from the directory containing decode.ml.
# Run once when the spec changes; the committed instructions.toml is the artifact.
#
#   usage:  python3 tools/gen_instr_toml.py [decode.ml] [WebAssembly.pdf] > spec/instructions.toml

import re, sys, subprocess, math, os

DECODE = sys.argv[1] if len(sys.argv) > 1 else 'tools/decode.ml'
PDF    = sys.argv[2] if len(sys.argv) > 2 else '../WebAssembly.pdf'
L = open(DECODE).read().splitlines()
MLDIR = os.path.dirname(DECODE) or '.'

def mlread(name):
    return open(os.path.join(MLDIR, name)).read()

def find(pat, s=0):
    for i in range(s, len(L)):
        if re.search(pat, L[i]): return i
    return -1

READ = (r'\b(idx|u32|s32|s64|f32|f64|u8|laneidx|memop|blocktype|heaptype'
        r'|valtype|reftype|v128|byte)\s+s\b')

def shape(body):
    rs = re.findall(READ, body); R = set(rs); has_vec = 'vec' in body
    if 'repeat 16 laneidx' in body: return 'v128'          # i8x16.shuffle = 16-byte imm
    if 'blocktype' in R: return 'blocktype'                # refined by opcode
    if 'memop' in R: return 'memlane' if ('laneidx' in R or 'u8' in R) else 'memarg'
    if has_vec:
        if 'valtype' in R: return 'selectt'
        if 'idx' in R: return 'brtable'
    if 'heaptype' in R: return 'broncast' if rs.count('heaptype') >= 2 else 'heap'
    if 'v128' in R: return 'v128'
    if 's32' in R: return 'i32'
    if 's64' in R: return 'i64'
    if 'f32' in R: return 'f32'
    if 'f64' in R: return 'f64'
    if 'laneidx' in R or 'u8' in R: return 'lane'
    n = rs.count('idx') + rs.count('u32')                  # both single ulebs
    return 'idx2' if n >= 2 else 'idx' if n == 1 else 'none'

def arms(a, b):
    out = []; i = a
    while a <= i < b:
        m = re.match(r'\s*\|\s*((?:0x[0-9a-f]+l?\s*(?:as \w+\s*)?(?:\|\s*)?)+)->(.*)', L[i])
        if not m: i += 1; continue
        codes = re.findall(r'0x([0-9a-f]+)l?', m.group(1)); body = m.group(2); j = i + 1
        while j < b and not re.match(r'\s*\|\s*(0x|n\b|_\b)', L[j]) and 'match u32 s with' not in L[j]:
            body += ' ' + L[j]; j += 1
        if 'illegal' not in body and 'error s pos' not in body:
            for c in codes: out.append((int(c, 16), body))
        i = j
    return out

# ── §7.10 Index of Instructions -> {(prefix|None, code): text mnemonic} plus
# {key: normalized Type column} ──
# Each row is `<mnemonic> <operands…>  <opcode hex…>  [<type>] <links>`. The
# opcode is the hex run immediately before the `[type]`; one hex = single byte,
# two = prefixed. The Type column is the spec's own typing notation; pdftotext
# renders its metavariables as Unicode math italics, normalized to ASCII here.
def norm_type(s):
    out = []
    for ch in s:
        o = ord(ch)
        if 0x1D44E <= o <= 0x1D467: out.append(chr(ord('a') + o - 0x1D44E))  # math italic a-z
        elif 0x1D434 <= o <= 0x1D44D: out.append(chr(ord('A') + o - 0x1D434))  # math italic A-Z
        elif o == 0x210E: out.append('h')                # planck ℎ = math italic h
        elif o == 0x2032: out.append("'")                # prime ′
        elif ch == '→': out.append('->')
        else: out.append(ch)
    s = re.sub(r'\s+', ' ', ''.join(out)).strip()
    s = re.sub(r'([A-Za-z]) (\d)', r'\1\2', s)           # rejoin split subscripts: "at 1" -> "at1"
    return s.replace('[ ', '[').replace(' ]', ']').replace(' )', ')')

def index_names(pdf):
    txt = subprocess.run(['pdftotext', '-layout', pdf, '-'],
                         capture_output=True, text=True).stdout.splitlines()
    start = next((i for i, l in enumerate(txt)
                  if re.match(r'\s*7\.10 Index of Instructions\s*$', l)), None)
    if start is None: sys.exit("could not find §7.10 in the PDF")
    names, types = {}, {}
    for l in txt[start + 1:]:
        l = re.sub(r'f (32|64)', r'f\1', l)              # pdftotext splits "f32"->"f 32" (also mid-id: "trunc_f32")
        mm = re.match(r'\s*([a-z][\w.]*)', l)            # mnemonic = first lowercase token
        if not mm: continue
        cut = l.find('[')                                # type signature ends the opcode column
        hexes = re.findall(r'0x([0-9A-Fa-f]+)', l[:cut] if cut >= 0 else l)
        if not hexes: continue
        if len(hexes) == 1:                              # single-byte opcode
            key = (None, int(hexes[0], 16))
        else:                                            # prefix + ULEB-encoded subop bytes
            prefix = int(hexes[0], 16); sub = 0          # (the §7.10 Note: subop is a u32 LEB)
            for i, h in enumerate(hexes[1:]):
                sub |= (int(h, 16) & 0x7f) << (7 * i)
            key = (prefix, sub)
        names.setdefault(key, mm.group(1))               # first occurrence wins
        if cut >= 0:
            seg = l[cut:].split(' validation')[0]        # drop the links columns
            if '→' in seg: types.setdefault(key, norm_type(seg))
    return names, types

# §6.5.6 Memory Instructions -> {text mnemonic: natural alignment (log2)}. Each
# rule annotates its memory op with `memargN` (N = natural byte alignment), the
# default the text format uses when `align=` is omitted. decode.ml reads memarg
# generically and §7.10's type sig doesn't distinguish e.g. i32.load/i32.load8_s,
# so this is the only source for the per-op default alignment.
def memarg_aligns(pdf):
    txt = subprocess.run(['pdftotext', '-layout', pdf, '-'],
                         capture_output=True, text=True).stdout.splitlines()
    start = next((i for i, l in enumerate(txt) if re.match(r'\s*6\.5\.6 ', l)), None)
    if start is None: sys.exit("could not find §6.5.6 in the PDF")
    al = {}
    for l in txt[start + 1:]:
        if 'Abbreviations' in l: break                   # stop before the abbreviation rules
        l = re.sub(r'f (32|64)', r'f\1', l)
        m = re.search(r'([a-z][a-z0-9_]*\.[a-z0-9_]+)\b.*?\bmemarg(\d+)', l)
        if m: al.setdefault(m.group(1), int(math.log2(int(m.group(2)))))
    return al

# §5.4 Instructions (binary format) -> {opcode key: [immediate operand tokens, in
# binary order]}. Keyed by OPCODE because it's unambiguous (`select`/`select t` share
# a mnemonic). This is the COMPLETE encoding signature in §5.4's own vocabulary: the
# index kinds (funcidx/typeidx/fieldidx/…), blocktype, const value types (i32/i64/
# f32/f64), memarg, laneidx, heaptype, the castop compound, the byte^16 / laneidx^16
# SIMD immediates, and vec(...) forms. The nested instruction body `(in:instr)*` and
# the 0x0B terminator are not immediates and are dropped; the prefixed subop `N:u32`
# is part of the opcode, not an operand.
def operands(pdf):
    txt = subprocess.run(['pdftotext', '-layout', pdf, '-'],
                         capture_output=True, text=True).stdout.splitlines()
    start = next((i for i, l in enumerate(txt) if re.match(r'\s*5\.4 ', l)), None)
    end   = next((i for i, l in enumerate(txt) if re.match(r'\s*5\.5 Modules\s*$', l)), None)
    if start is None or end is None: sys.exit("could not find §5.4/§5.5 in the PDF")
    KINDS = {'funcidx','localidx','globalidx','labelidx','tableidx','tagidx','typeidx',
             'elemidx','dataidx','memidx','fieldidx','blocktype','heaptype','valtype',
             'catch','laneidx','byte','memarg','castop','i32','i64','f32','f64','u32'}
    FORM = re.compile(r':\s*list\(\s*(\w+)\s*\)'                  # vec element
                      r'|\(\s*[^()]*?:\s*(byte|laneidx)\s*\)\s*16'  # X^16 SIMD imm
                      r'|\([^()]*\)\s*:\s*(memarg|castop)'        # compound immediate
                      r'|:\s*(\w+)')                              # plain kind
    ops = {}; cur = None
    for l in txt[start:end]:
        h = re.match(r'\s*(\w+)\s*::=', l)                   # production header
        if h: cur = h.group(1)
        if cur != 'instr': continue                         # skip blocktype/catch/castop/…
        if '⇒' not in l: continue                           # a real rule, not the `...` header
        l = re.sub(r'f (32|64)', r'f\1', l)
        lhs = l.split('⇒', 1)[0]
        m = re.search(r'0x([0-9A-Fa-f]+)\s*(?:(\d+)\s*:\s*u32)?', lhs)   # first 0x = opcode
        if not m: continue
        b = int(m.group(1), 16)
        if b in (0xFB, 0xFC, 0xFD):
            if m.group(2) is None: continue                      # prefixed needs a subop
            key = (b, int(m.group(2)))
        else:
            key = (None, b)
        rest = lhs[m.end():]
        rest = re.sub(r'\([^()]*:\s*instr\)\s*\*?\s*[12]?', ' ', rest)   # nested body
        rest = rest.replace('0x0B', ' ')                                 # end terminator
        toks = []
        for f in FORM.finditer(rest):
            if   f.group(1): toks.append('vec(%s)' % f.group(1))
            elif f.group(2): toks.append('%s^16' % f.group(2))
            elif f.group(3): toks.append(f.group(3))
            elif f.group(4) in KINDS: toks.append(f.group(4))
        ops.setdefault(key, toks)
    return ops

# ── Reference-interpreter trap extraction ────────────────────────────────
# Join chain: decode.ml arm -> smart-constructor name -> mnemonics.ml -> AST
# constructor (+ numeric type tag + operator pattern) -> eval.ml step arm /
# eval_num.ml dispatch. Every link is a hard error if it fails to resolve.
# `traps` lists the spec's execution traps as the reference interpreter spells
# them (`Trapping` messages, concat-built messages cut at the first comma).
# Host-defined resource exhaustion (call-stack depth) is a separate exception
# in eval.ml (Exhaustion, not Trapping) and is deliberately not listed.

RAISE = re.compile(r'raise\s+\(?\s*([A-Za-z_][\w.]*)')

# decode.ml arm body -> the smart-constructor it applies (tail expression
# head). A shared arm dispatching on the opcode (`(if opcode = 0xNNl then a
# else b) …`) resolves through the row's own sub-opcode.
def smart_of_body(body, key):
    tail = re.sub(r'\(\*.*?\*\)', ' ', body).split(';')[-1]
    tail = re.split(r'\bin\b', tail)[-1]
    om = re.search(r"if opcode = 0x([0-9a-f]+)l then ([a-z_][\w']*) else ([a-z_][\w']*)",
                   tail)
    if om:
        return om.group(2) if key[1] == int(om.group(1), 16) else om.group(3)
    m = re.match(r"\s*\(?\s*([a-z_][a-z0-9_']*)", tail)
    if not m: sys.exit("cannot find smart constructor in decode.ml arm: " + body)
    return m.group(1)

# mnemonics.ml -> {smart name: (AST constructor, I32/I64/F32/F64/V128 tag | None,
# operator pattern like "Div S" | None)}
def mnemonics_map(txt):
    out = {}
    for m in re.finditer(r"(?m)^let ([a-z_][\w']*)[^=\n]*=\s*(.*?)(?=^let |\Z)",
                         txt, re.S):
        expr = m.group(2)
        cm = re.search(r'\b([A-Z]\w*)', expr)
        tm = re.search(r'\((I32|I64|F32|F64|V128)\b', expr)
        om = re.search(r'[IFV]\w*Op\.(?:\(([^()]*)\)|(\w+))', expr)
        out[m.group(1)] = (cm.group(1) if cm else None,
                           tm.group(1) if tm else None,
                           (om.group(1) or om.group(2)).strip() if om else None)
    return out

# one .ml file -> {fn name: exception last-segments it can raise}, transitive
# over same-file calls. Split only at the file's outermost `let` indent (0 for
# plain modules, 2 inside a functor body) so nested let-in bindings stay
# attached to their enclosing function.
def file_raises(txt):
    ind = min(len(m.group(1)) for m in re.finditer(r"(?m)^([ ]*)let\s", txt))
    parts = re.split(r"(?m)^ {%d}let\s+(?:rec\s+)?([a-z_][\w']*)" % ind, txt)
    bodies = {}
    for i in range(1, len(parts), 2):
        bodies[parts[i]] = bodies.get(parts[i], '') + parts[i + 1]
    fns = {n: {r.split('.')[-1] for r in RAISE.findall(b)} for n, b in bodies.items()}
    for _ in range(3):
        for n, b in bodies.items():
            for m, rs in fns.items():
                if rs and m != n and re.search(r'\b' + re.escape(m) + r'\b', b):
                    fns[n] |= rs
    return fns

# eval_num.ml dispatch tables: (family, group|tag) -> ({op pattern: fn}, file)
def eval_num_tables(txt):
    heads = [(m.group(1), m.end()) for m in re.finditer(r'(?m)^module (\w+)', txt)]
    def section(name):
        for i, (n, s) in enumerate(heads):
            if n == name:
                return txt[s:heads[i + 1][1] if i + 1 < len(heads) else len(txt)]
        sys.exit("eval_num.ml: no module " + name)
    tables = {}
    for sec, fam, fname, callee in ((section('IntOp'), 'int', 'ixx', 'IXX'),
                                    (section('FloatOp'), 'float', 'fxx', 'FXX')):
        for group in ('unop', 'binop', 'testop', 'relop'):
            gm = re.search(r'let ' + group + r'\b(.*?)(?=\n  let |\Z)', sec, re.S)
            tbl = {}
            if gm:
                for pat, fn in re.findall(
                        r'\|\s*([A-Za-z][\w ]*?)\s*->\s*' + callee + r'\.([a-z_0-9]+)',
                        gm.group(1)):
                    tbl[pat.strip()] = fn
            tables[(fam, group)] = (tbl, fname)
    for mod, tag in (('I32CvtOp', 'I32'), ('I64CvtOp', 'I64'),
                     ('F32CvtOp', 'F32'), ('F64CvtOp', 'F64')):
        tbl = {}
        for pat, fn in re.findall(
                r'\|\s*([A-Za-z][\w ]*?)\s*->\s*Convert\.[IF]\d+_\.([a-z_0-9]+)',
                section(mod)):
            tbl[pat.strip()] = fn
        tables[('cvt', tag)] = (tbl, 'convert')
    return tables

# OCaml pattern vs concrete operator: lowercase-initial pattern tokens bind
def pat_match(key, op):
    kt, ot = key.split(), op.split()
    return len(kt) == len(ot) and all(
        k == o or k == '_' or k[0].islower() for k, o in zip(kt, ot))

def canon_msg(lit):
    return lit.split(',')[0].strip()

# eval.ml -> ({AST constructor: trap message set}, error-mapper tables)
def eval_traps(txt, runtime_raises):
    mappers = {}
    for name in ('table_error', 'memory_error', 'numeric_error'):
        blk = re.search(r'let ' + name + r' at = function(.*?)\n\n', txt, re.S)
        if not blk: sys.exit("eval.ml: no mapper " + name)
        mappers[name] = {e.split('.')[-1]: msg for e, msg in
                         re.findall(r'\|\s*([\w.]+)\s*->\s*"([^"]*)"', blk.group(1))}
    pre = txt[:txt.index('let rec step')]
    hbodies = {m.group(1): m.group(2) for m in
               re.finditer(r'(?m)^let (\w+)(.*?)(?=^let |\Z)', pre, re.S)}
    helpers = {}
    for n, b in hbodies.items():
        msgs = {canon_msg(s) for s in
                re.findall(r'Trap\.error\s+\w+\s+\(?"([^"]*)"', b)}
        if msgs: helpers[n] = msgs
    for _ in range(3):                                   # func_ref calls any_ref
        for n, b in hbodies.items():
            for h in list(helpers):
                if h != n and re.search(r'\b' + h + r'\b', b):
                    helpers.setdefault(n, set()).update(helpers[h])
    step = txt[txt.index('let rec step'):]
    marks = [(m.start(), m.group(1)) for m in
             re.finditer(r'(?m)^      \| (\w+)', step)]
    outer = [m.start() for m in re.finditer(r'(?m)^    \S', step)]
    bodies = {}
    for i, (pos, ctor) in enumerate(marks):
        end = marks[i + 1][0] if i + 1 < len(marks) else len(step)
        end = min([end] + [o for o in outer if pos < o < end])
        bodies[ctor] = bodies.get(ctor, '') + step[pos:end]
    traps, deps = {}, {}
    for ctor, b in bodies.items():
        s = {canon_msg(lit) for lit in re.findall(r'Trapping\s+\(?"([^"]*)"', b)}
        for fn, exn in re.findall(r'Trapping\s+\((\w+_error)\s+\S+\s+([\w.]+)\)', b):
            if exn != 'exn':
                s.add(mappers[fn][exn.split('.')[-1]])
        for fn in re.findall(
                r'with\s+exn\s*->.*?Trapping\s+\((\w+_error)\s+\S+\s+exn\)', b, re.S):
            for mod, rfn in re.findall(r'\b(Table|Memory)\.([a-z_0-9]+)', b):
                for exn in runtime_raises[mod].get(rfn, ()):
                    if exn in mappers[fn]: s.add(mappers[fn][exn])
        for h, msgs in helpers.items():
            if re.search(r'\b' + h + r'\b', b): s |= msgs
        traps[ctor] = s
        deps[ctor] = set(re.findall(r'Plain\s+\((\w+)', b))
    for _ in range(4):                                   # reduction-to-instr inheritance
        for ctor, ds in deps.items():
            for d in ds:
                traps[ctor] |= traps.get(d, set())
    return traps, mappers

# valid.ml check_instr -> {AST constructor: [arm texts]}
def valid_arrows(txt):
    body = txt[txt.index('let rec check_instr'):txt.index('\nand check_instrs')]
    ms = list(re.finditer(r'(?m)^  \| (\w+)', body))
    arms = {}
    for i, m in enumerate(ms):
        end = ms[i + 1].start() if i + 1 < len(ms) else len(body)
        arms.setdefault(m.group(1), []).append(body[m.start():end])
    return arms

VALID_ATOM = {'NumT I32T': 'i32', 'NumT I64T': 'i64', 'NumT F32T': 'f32',
              'NumT F64T': 'f64', 'VecT V128T': 'v128'}

# render a closed-form valid.ml arrow as token lists, `t` bound to the tag.
# `-->...` (stack-polymorphic) arrows are not closed-form and are skipped.
def render_arrow(armtext, tag):
    am = None
    for am in re.finditer(r'\[([^][]*)\]\s*-->\s*\[([^][]*)\]', armtext):
        pass
    if am is None or '-->...' in armtext: return None
    def side(s):
        toks = []
        for a in (t.strip() for t in s.split(';') if t.strip()):
            if a in VALID_ATOM: toks.append(VALID_ATOM[a])
            elif a == 't' and tag: toks.append(tag.lower())
            else: return None
        return toks
    lhs, rhs = side(am.group(1)), side(am.group(2))
    return None if lhs is None or rhs is None else (lhs, rhs)

def type_tokens(tstr):
    m = re.fullmatch(r'\[([^]]*)\] -> \[([^]]*)\]', tstr)
    return (m.group(1).split(), m.group(2).split()) if m else None

main0 = find(r'^let rec instr s =')
fb = find(r'\|\s*0xfb\b.*->'); fc = find(r'\|\s*0xfc\b.*->'); fd = find(r'\|\s*0xfd\b.*->')
endfd = find(r'^and instr_block')
CTRL_SHAPE = {0x02: 'block', 0x03: 'block', 0x04: 'if', 0x1f: 'trytable'}

rows = []  # (op_repr, key, shape, smart constructor)
for code, body in arms(main0, fb):
    if code in (0xfb, 0xfc, 0xfd): continue
    if code in (0x05, 0x0b): continue                      # else/end: structural terminators
    sh = CTRL_SHAPE.get(code, shape(body))
    rows.append((f"0x{code:02X}", (None, code), sh, smart_of_body(body, (None, code))))
for pfx, lo, hi in ((0xFB, fb, fc), (0xFC, fc, fd), (0xFD, fd, endfd)):
    for code, body in arms(lo + 1, hi):
        rows.append((f"[0x{pfx:02X}, {code}]", (pfx, code), shape(body),
                     smart_of_body(body, (pfx, code))))

# Join authoritative text names by opcode; a missing name is a hard error (no
# silent fallback to a mangled name — that would defeat the whole point).
idx, ityp = index_names(PDF)

# §7.10's index drops the `_zero` suffix on these two relaxed-trunc ops — a spec-doc
# inconsistency: the §6.5 text-format definition, the opcode, and the conformance suite
# all spell them `..._zero` (cf. the non-relaxed i32x4.trunc_sat_f64x2_s_zero). Restore it.
idx.update({
    (0xFD, 259): "i32x4.relaxed_trunc_f64x2_s_zero",
    (0xFD, 260): "i32x4.relaxed_trunc_f64x2_u_zero",
})
missing = [op for op, key, _, _ in rows if key not in idx]
if missing:
    sys.exit("no §7.10 text mnemonic for opcodes: " + ", ".join(missing))

# Natural-alignment default for memory ops, joined by mnemonic (hard error if a
# memarg/memlane op has no §6.5.6 memargN — completeness, no silent gap).
aligns = memarg_aligns(PDF)
mem_missing = [idx[key] for op, key, sh, _ in rows if sh in ('memarg', 'memlane') and idx[key] not in aligns]
if mem_missing:
    sys.exit("no §6.5.6 memargN for: " + ", ".join(mem_missing))

# Complete immediate operand signature per opcode, from §5.4 (binary format). A
# missing opcode is a hard error (no silent gap). This subsumes the index spaces a
# $id resolves against: the idx-kind tokens (funcidx/typeidx/fieldidx/…) say which.
ops_sig = operands(PDF)
op_missing = [op for op, key, _, _ in rows if key not in ops_sig]
if op_missing:
    sys.exit("no §5.4 operand signature for opcodes: " + ", ".join(op_missing))

# Cross-check: the §5.4 operand signature must imply the same immediate `shape` the
# decode.ml pass derived — two independent spec sources agreeing. Control opcodes
# carry a blocktype that can't on its own distinguish block/loop from if, so the
# expected set admits the family (the opcode disambiguates via CTRL_SHAPE).
def shape_from_ops(o):
    j = ' '.join(o)
    if not o:                              return {'none'}
    if 'catch' in j:                       return {'trytable'}
    if o == ['blocktype']:                 return {'block', 'if'}
    if 'memarg' in o:                      return {'memlane' if 'laneidx' in j else 'memarg'}
    if 'byte^16' in o or 'laneidx^16' in o:return {'v128'}
    if 'laneidx' in o:                     return {'lane'}
    if 'castop' in o or o.count('heaptype') >= 2: return {'broncast'}
    if 'heaptype' in o:                    return {'heap'}
    if 'vec(valtype)' in o:                return {'selectt'}
    if 'vec(labelidx)' in o:               return {'brtable'}
    for t in ('i32', 'i64', 'f32', 'f64'):
        if o == [t]:                       return {t}
    IDX = {'funcidx','localidx','globalidx','labelidx','tableidx','tagidx','typeidx',
           'elemidx','dataidx','memidx','fieldidx','u32'}
    n = sum(1 for x in o if x in IDX)
    return {'idx2'} if n >= 2 else {'idx'} if n == 1 else {'none'}

mismatch = [f"{idx[key]} (shape={sh}, operands={ops_sig[key]})"
            for op, key, sh, _ in rows if sh not in shape_from_ops(ops_sig[key])]
if mismatch:
    sys.exit("§5.4 operands disagree with decode.ml shape for:\n  " + "\n  ".join(mismatch))

# §7.10 Type column per opcode — a missing type is a hard error (no silent gap).
t_missing = [idx[key] for op, key, _, _ in rows if key not in ityp]
if t_missing:
    sys.exit("no §7.10 Type column entry for: " + ", ".join(t_missing))

# decode.ml smart constructor -> mnemonics.ml AST constructor; hard error on gaps.
mnem = mnemonics_map(mlread('mnemonics.ml'))
m_missing = [f"{idx[key]} ({sm})" for op, key, _, sm in rows if sm not in mnem]
if m_missing:
    sys.exit("no mnemonics.ml entry for: " + ", ".join(m_missing))

# Trap extraction. Numeric ops (Test/Compare/Unary/Binary/Convert) resolve
# through eval_num.ml's dispatch into ixx/fxx/convert raise sites; everything
# else comes from its eval.ml step arm. Vector numeric ops fall out as
# trap-free from the arm scan; the assert below backs that with the fact that
# eval_vec.ml/v128.ml contain no numeric-error raise site.
runtime_raises = {'Table': file_raises(mlread('table.ml')),
                  'Memory': file_raises(mlread('memory.ml'))}
arm_traps, mappers = eval_traps(mlread('eval.ml'), runtime_raises)
num_tables = eval_num_tables(mlread('eval_num.ml'))
raises_by_file = {'ixx': file_raises(mlread('ixx.ml')),
                  'fxx': file_raises(mlread('fxx.ml')),
                  'convert': file_raises(mlread('convert.ml'))}
vec_bad = {r.split('.')[-1] for r in RAISE.findall(mlread('eval_vec.ml') + mlread('v128.ml'))} \
          & set(mappers['numeric_error'])
if vec_bad:
    sys.exit("eval_vec.ml/v128.ml now raise numeric errors (%s); the vector "
             "trap extraction must resolve their dispatch" % ", ".join(sorted(vec_bad)))

NUMERIC_CTORS = {'Test', 'Compare', 'Unary', 'Binary', 'Convert'}

def numeric_traps(name, ctor, tag, oppat):
    if not tag or not oppat:
        sys.exit(f"{name}: numeric op without type tag/operator pattern")
    if ctor == 'Convert':
        tbl, fname = num_tables[('cvt', tag)]
    else:
        group = {'Unary': 'unop', 'Binary': 'binop',
                 'Test': 'testop', 'Compare': 'relop'}[ctor]
        tbl, fname = num_tables[('int' if tag in ('I32', 'I64') else 'float', group)]
    hits = sorted({fn for pat, fn in tbl.items() if pat_match(pat, oppat)})
    if len(hits) != 1:
        sys.exit(f"{name}: eval_num dispatch for {ctor} {tag} '{oppat}' -> {hits}")
    exns = raises_by_file[fname].get(hits[0], set())
    return sorted(mappers['numeric_error'][e]
                  for e in exns if e in mappers['numeric_error'])

traps_by_key = {}
for op, key, sh, sm in rows:
    ctor, tag, oppat = mnem[sm]
    if ctor in NUMERIC_CTORS:
        traps_by_key[key] = numeric_traps(idx[key], ctor, tag, oppat)
    elif ctor in arm_traps:
        traps_by_key[key] = sorted(arm_traps[ctor])
    else:
        sys.exit(f"{idx[key]}: no eval.ml step arm for constructor {ctor}")

# Cross-check: wherever valid.ml's typing arrow is closed-form (literal atoms,
# `t` bound by the operator's type tag), it must render to exactly the §7.10
# Type column — the two typing sources agreeing.
v_arms = valid_arrows(mlread('valid.ml'))
v_checked, v_bad = 0, []
for op, key, sh, sm in rows:
    ctor, tag, _ = mnem[sm]
    rendered = render_arrow(''.join(v_arms.get(ctor, [])), tag)
    if rendered is None: continue
    if type_tokens(ityp[key]) != rendered:
        v_bad.append(f"{idx[key]}: valid.ml {rendered} vs §7.10 '{ityp[key]}'")
    v_checked += 1
if v_bad:
    sys.exit("valid.ml typing disagrees with §7.10 for:\n  " + "\n  ".join(v_bad))
print(f"# gen_instr_toml: {v_checked} typings cross-checked against valid.ml",
      file=sys.stderr)

print("# WebAssembly 3.0 instruction opcode table.")
print("# GENERATED ONCE by tools/gen_instr_toml.py, joining the reference decoder")
print("# (interpreter/binary/decode.ml: opcode + shape) with the §7.10 Index of")
print("# Instructions (WebAssembly.pdf: canonical TEXT mnemonic), by opcode. This")
print("# file is the artifact; regenerate only when the spec changes. `name` is the")
print("# .wat text mnemonic. `operands` is the COMPLETE ordered immediate signature")
print("# (§5.4 binary format, its own vocabulary): index kinds (funcidx/typeidx/…),")
print("# blocktype, const types (i32/i64/f32/f64), memarg, laneidx, heaptype, castop,")
print("# byte^16 / laneidx^16, and vec(...) forms. `shape` is a coarse summary of it,")
print("# cross-checked against `operands`. `opcode` is a byte, or [prefix, sub] for")
print("# 0xFB/0xFC/0xFD. memarg/memlane entries also carry `align` = natural alignment")
print("# (log2), the default when the text format omits `align=` (from §6.5.6).")
print("# `type` is the instruction's typing from the §7.10 Type column, in the")
print("# spec's own notation (t/t1/t2 metavariables, stack-polymorphic t*);")
print("# cross-checked against valid.ml's typing arrows where those are")
print("# closed-form. `traps` is the COMPLETE set of execution-trap conditions,")
print("# extracted from the reference interpreter (eval.ml Trapping arms; numeric")
print("# ops via eval_num.ml dispatch into the ixx/fxx/convert raise sites;")
print("# reduction instructions inherit from the instructions they expand to),")
print("# spelled as the interpreter's trap messages. traps = [] means the spec")
print("# defines no trap for the instruction. Host resource exhaustion (call")
print("# depth) is not a trap and is not listed.")
print(f"# {len(rows)} instructions. else (0x05) / end (0x0B) excluded (terminators).")
for op, key, sh, sm in rows:
    print()
    print("[[instr]]")
    print(f'name = "{idx[key]}"')
    print(f"opcode = {op}")
    print(f'shape = "{sh}"')
    print('operands = [' + ', '.join(f'"{t}"' for t in ops_sig[key]) + ']')
    if sh in ('memarg', 'memlane'):                       # natural alignment (log2), §6.5.6
        print(f"align = {aligns[idx[key]]}")
    print(f'type = "{ityp[key]}"')
    print('traps = [' + ', '.join(f'"{t}"' for t in traps_by_key[key]) + ']')
