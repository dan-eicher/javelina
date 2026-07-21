;; gc_refforms.wat — every form in §4.2.1's value production, as an exported builder.
;;
;;   val  ::= num | vec | ref
;;   num  ::= numtype.const num_numtype                    (i32 i64 f32 f64)
;;   vec  ::= vectype.const vec_vectype                    (v128)
;;   ref  ::= ref.i31 u31 | ref.null | ref.struct structaddr | ref.array arrayaddr
;;          | ref.func funcaddr | ref.exn exnaddr | ref.host hostaddr | ref.extern ref
;;
;; §4.2.1: "Any of the aforementioned references can furthermore be wrapped up as an external
;; reference" — so ref.extern may wrap a ref.struct, i.e. an externref CAN carry a store address.
;; §4.2.3: the store's collectable instances are structs, arrays and exns.
;; => the tracer must follow exactly: ref.struct, ref.array, ref.exn, and ref.extern wrapping one.
;;
;; The address forms use a build/check PAIR around a collection, held in a rooted global
;; (jav_instance.c:483 scans T_GCREF globals). Checking only that the outer aggregate survives is
;; worthless — it is the rooted result, so it survives unconditionally; the check must reach the
;; INNER object's payload, which is the thing a missed trace would have freed.
(module
  (type $leaf    (struct (field i32)))
  (type $refbox  (struct (field (mut anyref))))   ;; default-initialised ref field must be ref.null
  (type $i32arr  (array (mut i32)))
  (type $i64arr  (array (mut i64)))
  (type $f32arr  (array (mut f32)))
  (type $f64arr  (array (mut f64)))
  (type $v128arr (array (mut v128)))
  (type $anyarr  (array (mut anyref)))
  (type $eqarr   (array (mut eqref)))
  (type $i31arr  (array (mut i31ref)))
  (type $strarr  (array (mut structref)))
  (type $arrarr  (array (mut arrayref)))
  (type $funarr  (array (mut funcref)))
  (type $extarr  (array (mut externref)))
  (type $exnarr  (array (mut exnref)))

  (import "host" "ref" (global $hostref externref))   ;; ref.host — only the embedder can make one
  (table $t 4 funcref)                                ;; untouched slots must read back as ref.null
  (table $ta 4 anyref)
  (elem $e func $dummy)
  (global $held (mut anyref) (ref.null any))          ;; a GC root spanning the collection
  (tag $e)
  (func $dummy)
  (elem declare func $dummy)

  ;; ── ref.struct structaddr — a store address, MUST be traced ─────────────────
  (func (export "build_struct_in_structref")
    (global.set $held (array.new $strarr (struct.new $leaf (i32.const 42)) (i32.const 1))))
  (func (export "check_struct_in_structref") (result i32)
    (struct.get $leaf 0 (ref.cast (ref $leaf)
      (array.get $strarr (ref.cast (ref $strarr) (global.get $held)) (i32.const 0)))))

  (func (export "build_struct_in_anyref")
    (global.set $held (array.new $anyarr (struct.new $leaf (i32.const 42)) (i32.const 1))))
  (func (export "check_struct_in_anyref") (result i32)
    (struct.get $leaf 0 (ref.cast (ref $leaf)
      (array.get $anyarr (ref.cast (ref $anyarr) (global.get $held)) (i32.const 0)))))

  (func (export "build_struct_in_eqref")
    (global.set $held (array.new $eqarr (struct.new $leaf (i32.const 42)) (i32.const 1))))
  (func (export "check_struct_in_eqref") (result i32)
    (struct.get $leaf 0 (ref.cast (ref $leaf)
      (array.get $eqarr (ref.cast (ref $eqarr) (global.get $held)) (i32.const 0)))))

  ;; ── ref.array arrayaddr — a store address, MUST be traced (inner len 42) ────
  (func (export "build_array_in_arrayref")
    (global.set $held (array.new $arrarr (array.new_default $i32arr (i32.const 42)) (i32.const 1))))
  (func (export "check_array_in_arrayref") (result i32)
    (array.len (ref.cast (ref $i32arr)
      (array.get $arrarr (ref.cast (ref $arrarr) (global.get $held)) (i32.const 0)))))

  (func (export "build_array_in_anyref")
    (global.set $held (array.new $anyarr (array.new_default $i32arr (i32.const 42)) (i32.const 1))))
  (func (export "check_array_in_anyref") (result i32)
    (array.len (ref.cast (ref $i32arr)
      (array.get $anyarr (ref.cast (ref $anyarr) (global.get $held)) (i32.const 0)))))

  ;; ── ref.exn exnaddr — a store address, MUST be traced ───────────────────────
  (func (export "build_exn_in_exnref")
    (global.set $held (array.new $exnarr
      (block $b (result exnref)
        (try_table (catch_ref $e $b) (throw $e))
        (unreachable))
      (i32.const 1))))
  (func (export "check_exn_in_exnref") (result i32)   ;; 42 iff the exnref is still non-null
    (select (i32.const 42) (i32.const 0)
      (i32.eqz (ref.is_null
        (array.get $exnarr (ref.cast (ref $exnarr) (global.get $held)) (i32.const 0))))))

  ;; ── ref.extern ref — wraps ref.struct, so it carries a STORE ADDRESS ────────
  (func (export "build_extern_wrapping_struct")
    (global.set $held (array.new $extarr
      (extern.convert_any (struct.new $leaf (i32.const 42))) (i32.const 1))))
  (func (export "check_extern_wrapping_struct") (result i32)
    (struct.get $leaf 0 (ref.cast (ref $leaf) (any.convert_extern
      (array.get $extarr (ref.cast (ref $extarr) (global.get $held)) (i32.const 0))))))

  ;; ── forms carrying NO store address: must never be dereferenced by the tracer ──
  (func (export "i31_in_i31ref") (result (ref $i31arr))
    (array.new $i31arr (ref.i31 (i32.const 42)) (i32.const 1)))
  (func (export "i31_in_anyref") (result (ref $anyarr))
    (array.new $anyarr (ref.i31 (i32.const 42)) (i32.const 1)))
  (func (export "i31_in_eqref") (result (ref $eqarr))
    (array.new $eqarr (ref.i31 (i32.const 42)) (i32.const 1)))
  (func (export "func_in_funcref") (result (ref $funarr))
    (array.new $funarr (ref.func $dummy) (i32.const 1)))
  (func (export "host_in_externref") (result (ref $extarr))
    (array.new $extarr (global.get $hostref) (i32.const 1)))
  (func (export "null_in_externref") (result (ref $extarr))
    (array.new $extarr (ref.null extern) (i32.const 1)))
  (func (export "null_in_anyref") (result (ref $anyarr))
    (array.new $anyarr (ref.null any) (i32.const 1)))
  (func (export "null_in_funcref") (result (ref $funarr))
    (array.new $funarr (ref.null func) (i32.const 1)))
  (func (export "null_in_exnref") (result (ref $exnarr))
    (array.new $exnarr (ref.null exn) (i32.const 1)))

  ;; ── num / vec fidelity: compared INSIDE wasm so no host-side stale slot can fake it ──
  (func (export "num_i32") (result i32)
    (i32.eq (array.get $i32arr (array.new $i32arr (i32.const 0x7fffffff) (i32.const 1)) (i32.const 0))
            (i32.const 0x7fffffff)))
  (func (export "num_i64") (result i32)
    (i64.eq (array.get $i64arr (array.new $i64arr (i64.const 0x0123456789abcdef) (i32.const 1)) (i32.const 0))
            (i64.const 0x0123456789abcdef)))
  (func (export "num_f32") (result i32)
    (i32.eq (i32.reinterpret_f32
              (array.get $f32arr (array.new $f32arr (f32.const 1.5) (i32.const 1)) (i32.const 0)))
            (i32.reinterpret_f32 (f32.const 1.5))))
  (func (export "num_f64") (result i32)
    (i64.eq (i64.reinterpret_f64
              (array.get $f64arr (array.new $f64arr (f64.const 1.5) (i32.const 1)) (i32.const 0)))
            (i64.reinterpret_f64 (f64.const 1.5))))
  (func (export "vec_v128") (result i32)
    (local $a (ref $v128arr))
    (local.set $a (array.new $v128arr (v128.const i32x4 1 2 3 0xdeadbeef) (i32.const 1)))
    (i32.and
      (i32.eq (i32x4.extract_lane 0 (array.get $v128arr (local.get $a) (i32.const 0))) (i32.const 1))
      (i32.eq (i32x4.extract_lane 3 (array.get $v128arr (local.get $a) (i32.const 0))) (i32.const 0xdeadbeef))))

  ;; ── ref.i31's TAGGED REPRESENTATION: (v << 3) | 1 ───────────────────────────
  ;; These pin the properties that make the encoding sound. None of them are implied by the
  ;; existing i31 tests, which were written against the untagged encoding and pass either way
  ;; because encode/decode are symmetric. The first two are the collision the shift width exists
  ;; to avoid: with the naive (v << 1) | 1, u31 max encodes to exactly 0xFFFFFFFF = JAV_NULLREF,
  ;; and `ref.i31 0x7fffffff` silently becomes null with every other test still green.
  (func (export "i31_zero_not_null") (result i32)      ;; bare 0 also reads as null (JAV_REF_ISNULL)
    (i32.eqz (ref.is_null (ref.i31 (i32.const 0)))))
  (func (export "i31_max_not_null") (result i32)       ;; THE collision case
    (i32.eqz (ref.is_null (ref.i31 (i32.const 0x7fffffff)))))
  (func (export "i31_get_u_zero") (result i32)
    (i32.eq (i31.get_u (ref.i31 (i32.const 0))) (i32.const 0)))
  (func (export "i31_get_u_max") (result i32)
    (i32.eq (i31.get_u (ref.i31 (i32.const 0x7fffffff))) (i32.const 0x7fffffff)))
  (func (export "i31_get_s_max") (result i32)          ;; u31 max is -1 signed: sign-extend at bit 30
    (i32.eq (i31.get_s (ref.i31 (i32.const 0x7fffffff))) (i32.const -1)))
  (func (export "i31_get_s_signbit") (result i32)      ;; bit 30 set = most negative i31
    (i32.eq (i31.get_s (ref.i31 (i32.const 0x40000000))) (i32.const -1073741824)))
  (func (export "i31_masks_high_bits") (result i32)    ;; §4.2.1 ref.i31 u31 — only 31 bits survive
    (i32.eq (i31.get_u (ref.i31 (i32.const -1))) (i32.const 0x7fffffff)))
  (func (export "i31_is_i31") (result i32)
    (ref.test i31ref (ref.i31 (i32.const 42))))
  (func (export "i31_is_not_struct") (result i32)      ;; a scalar must not read as a pointer
    (i32.eqz (ref.test structref (ref.i31 (i32.const 42)))))
  (func (export "i31_eq_same") (result i32)
    (ref.eq (ref.i31 (i32.const 12345)) (ref.i31 (i32.const 12345))))
  (func (export "i31_eq_diff") (result i32)
    (i32.eqz (ref.eq (ref.i31 (i32.const 12345)) (ref.i31 (i32.const 12346)))))

  ;; Payload INTACT across a collection, not merely "did not fault": gc_mark1's return value is
  ;; written back into the slot, so a skipped i31 must come back bit-identical. Boundary values,
  ;; because 42 would survive a great many wrong implementations.
  (func (export "build_i31_max_gc")
    (global.set $held (array.new $anyarr (ref.i31 (i32.const 0x7fffffff)) (i32.const 1))))
  (func (export "check_i31_max_gc") (result i32)
    (select (i32.const 42) (i32.const 0)
      (i32.eq (i31.get_u (ref.cast (ref i31)
                (array.get $anyarr (ref.cast (ref $anyarr) (global.get $held)) (i32.const 0))))
              (i32.const 0x7fffffff))))
  (func (export "build_i31_zero_gc")
    (global.set $held (array.new $anyarr (ref.i31 (i32.const 0)) (i32.const 1))))
  (func (export "check_i31_zero_gc") (result i32)
    (select (i32.const 42) (i32.const 0)
      (i32.eq (i31.get_u (ref.cast (ref i31)
                (array.get $anyarr (ref.cast (ref $anyarr) (global.get $held)) (i32.const 0))))
              (i32.const 0))))
  (func (export "build_i31_signbit_gc")
    (global.set $held (array.new $eqarr (ref.i31 (i32.const 0x40000000)) (i32.const 1))))
  (func (export "check_i31_signbit_gc") (result i32)
    (select (i32.const 42) (i32.const 0)
      (i32.eq (i31.get_s (ref.cast (ref i31)
                (array.get $eqarr (ref.cast (ref $eqarr) (global.get $held)) (i32.const 0))))
              (i32.const -1073741824))))

  ;; ── ref.null's REPRESENTATION ───────────────────────────────────────────────
  ;; §4.2.1 `ref.null` is one of the eight ref forms and every hierarchy has one. Its bit pattern
  ;; moved (all-ones -> 0) because §2.3.4's one reserved tag bit makes every i31 odd, so null must
  ;; be even. Nothing tested that pattern, so every place that spelled null as a LITERAL rather
  ;; than through the authority kept the old encoding and silently stopped being null. These pin
  ;; each container that has to produce a null, which is what a literal would break.
  (func (export "null_func_is_null")   (result i32) (ref.is_null (ref.null func)))
  (func (export "null_extern_is_null") (result i32) (ref.is_null (ref.null extern)))
  (func (export "null_any_is_null")    (result i32) (ref.is_null (ref.null any)))
  (func (export "null_exn_is_null")    (result i32) (ref.is_null (ref.null exn)))
  (func (export "null_none_is_null")   (result i32) (ref.is_null (ref.null none)))
  (func (export "null_nofunc_is_null") (result i32) (ref.is_null (ref.null nofunc)))
  (func (export "null_i31_is_null")    (result i32) (ref.is_null (ref.null i31)))
  ;; a zero-initialised ref LOCAL is null (jav_call_fn zeroes locals)
  (func (export "null_local_is_null")  (result i32) (local $r anyref) (ref.is_null (local.get $r)))
  ;; an UNTOUCHED TABLE SLOT — this is the one that had a hardcoded -1 in jav_instance.c
  (func (export "null_table_slot_funcref") (result i32) (ref.is_null (table.get $t (i32.const 0))))
  (func (export "null_table_slot_anyref")  (result i32) (ref.is_null (table.get $ta (i32.const 0))))
  ;; table.set / table.fill / table.grow must all round-trip a null
  (func (export "null_table_set_get") (result i32)
    (table.set $t (i32.const 1) (ref.null func))
    (ref.is_null (table.get $t (i32.const 1))))
  (func (export "null_table_fill_get") (result i32)
    (table.fill $t (i32.const 2) (ref.null func) (i32.const 2))
    (ref.is_null (table.get $t (i32.const 3))))
  (func (export "null_table_grow_get") (result i32)
    (drop (table.grow $ta (ref.null any) (i32.const 1)))
    (ref.is_null (table.get $ta (i32.const 4))))
  ;; a default-initialised struct field / array element
  (func (export "null_struct_field") (result i32)
    (ref.is_null (struct.get $refbox 0 (struct.new_default $refbox))))
  (func (export "null_array_elem") (result i32)
    (ref.is_null (array.get $anyarr (array.new_default $anyarr (i32.const 1)) (i32.const 0))))
  ;; and the crossover with the tag change: a NON-null ref must not read as null
  (func (export "funcref_is_not_null") (result i32) (i32.eqz (ref.is_null (ref.func $dummy))))
  (func (export "struct_is_not_null")  (result i32)
    (i32.eqz (ref.is_null (struct.new $leaf (i32.const 42)))))
  (func (export "i31_zero_is_not_null2") (result i32)
    (i32.eqz (ref.is_null (ref.i31 (i32.const 0)))))
  ;; ── §4.2.1 default values ───────────────────────────────────────────────────
  (func (export "default_i32") (result i32)
    (i32.eq (array.get $i32arr (array.new_default $i32arr (i32.const 1)) (i32.const 0)) (i32.const 0)))
  (func (export "default_f64") (result i32)
    (i64.eq (i64.reinterpret_f64
              (array.get $f64arr (array.new_default $f64arr (i32.const 1)) (i32.const 0)))
            (i64.const 0)))                       ;; +0 has the all-zero bit pattern
  (func (export "default_v128") (result i32)
    (i32.eq (i32x4.extract_lane 3 (array.get $v128arr (array.new_default $v128arr (i32.const 1)) (i32.const 0)))
            (i32.const 0)))
  (func (export "default_ref") (result i32)
    (ref.is_null (array.get $anyarr (array.new_default $anyarr (i32.const 1)) (i32.const 0))))
)
