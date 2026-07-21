;; addrtype_spec.wat — §2.3.11 address types, as the spec resolves them.
;;
;; §2.3.11:  addrtype ::= i32 | i64        "a subset of number types that classify the values that
;;                                          can be used as offsets into memories and tables"
;; §2.3.15:  memtype   ::= addrtype limits page
;; §2.3.16:  tabletype ::= addrtype limits reftype
;;
;; So `at` is NOT an abstract variable — it is i32 or i64, DECLARED by each memory/table. Every
;; typing rule reads it from the module's declared type, never from a value:
;;
;;   C.mems[x]   = at lim page  =>  C |- nt.load x memarg : at -> nt          (§3.4.5)
;;   C.tables[x] = at lim rt    =>  C |- table.get  x : at -> rt              (§3.4.4)
;;                                  C |- table.size x : eps -> at
;;                                  C |- table.grow x : rt at -> at
;;                                  C |- table.fill x : at rt at -> eps
;;                                  C |- table.copy x1 x2 : at1 at2 min(at1,at2) -> eps
;;
;; and §4.6.8 executes it as "ASSERT: Due to validation, a number value is on the top of the stack.
;; Pop the value (at.const i)" — asserted from validation, never tested at runtime.
;;
;; javelina resolves the width at RUNTIME instead, from the operand's value tag (GPOP_ADDR:
;; `tag == T_LONG ? 64-bit : truncate to 32`). That is a divergence in MECHANISM: a tested fact can
;; disagree with the declared type, an asserted one cannot. These cases construct exactly that
;; disagreement — an i64 address whose runtime tag says otherwise — and assert the spec's answer.
(module
  (type $i64arr (array (mut i64)))

  (memory $m64 i64 1)
  (table  $t32 4 externref)          ;; addrtype i32 (default)
  (table  $t64 i64 4 externref)      ;; addrtype i64

  ;; ── controls: the declared addrtype IS the operand/result type ────────────
  ;; table.size : eps -> at. On $t64 that is i64, so i64.eqz only type-checks if it really is.
  (func (export "size64_is_i64") (result i32)
    (i64.eq (table.size $t64) (i64.const 4)))
  (func (export "size32_is_i32") (result i32)
    (i32.eq (table.size $t32) (i32.const 4)))
  ;; table.grow : rt at -> at. On $t64 both the delta and the result are i64.
  (func (export "grow64_is_i64") (result i32)
    (i64.eq (table.grow $t64 (ref.null extern) (i64.const 1)) (i64.const 4)))
  ;; §3.4.4 table.copy x1 x2 : at1 at2 min(at1,at2) -> eps. Copying $t64 <- $t32 makes the LENGTH
  ;; operand min(i64,i32) = i32, while the destination offset stays i64. Nothing else covers a type
  ;; computed from two immediates.
  (func (export "copy_len_is_min_at") (result i32)
    (table.copy $t64 $t32 (i64.const 0) (i32.const 0) (i32.const 2))
    (i32.const 1))

  ;; ── the divergence: an address whose DECLARED type and runtime tag disagree ──
  ;; An i64 read out of a GC array arrives with the aggregate's reconstructed tag, not i64's. The
  ;; declared addrtype of $m64/$t64 is i64 regardless, so the full 64-bit value must be used and
  ;; 2^32 must trap as out of bounds. A runtime-tag lowering truncates it to 0 and succeeds.
  (func $carried_2p32 (result i64)
    (array.get $i64arr
      (array.new $i64arr (i64.const 0x100000000) (i32.const 1)) (i32.const 0)))

  (func (export "mem64_carried_addr_traps") (result i32)
    (i32.load (call $carried_2p32)))          ;; must TRAP (OOB), not read offset 0
  (func (export "table64_carried_index_traps") (result i32)
    (drop (table.get $t64 (call $carried_2p32)))   ;; must TRAP (OOB), not read entry 0
    (i32.const 1))

  ;; controls proving those two addresses are genuinely out of bounds when spelled directly
  (func (export "mem64_direct_2p32_traps") (result i32)
    (i32.load (i64.const 0x100000000)))
  (func (export "table64_direct_2p32_traps") (result i32)
    (drop (table.get $t64 (i64.const 0x100000000)))
    (i32.const 1))
  ;; and that in-bounds access through the SAME carrier path works, so a trap above is about the
  ;; width and not about the carrier plumbing
  (func (export "mem64_carried_inbounds_ok") (result i32)
    (i32.store (i64.const 8) (i32.const 0xABC))
    (i32.load (array.get $i64arr
      (array.new $i64arr (i64.const 8) (i32.const 1)) (i32.const 0))))
)
