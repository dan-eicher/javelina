;; gc_roots_real.wat — the collector's actual contract, over objects the ENGINE builds.
;;
;; jav_gc_enum_roots scans four sources: the operand stack, frame locals, module globals, and table
;; entries. Each gates on the slot's tag == T_GCREF. The contract is two-sided and BOTH sides matter:
;;   reachable through a root  => survives with its payload intact
;;   unreachable               => reclaimed
;; A collector that traces nothing passes the second; one that never frees passes the first. Testing
;; only "survives" is how a leak ships; only "reclaimed" is how a use-after-free ships.
;;
;; `$collect` is a host import that runs a collection MID-EXECUTION, which is the only way to hold a
;; root that exists solely while a frame is live — a local, or an operand on the stack. Every
;; previous GC test collected between calls, when those roots are already gone, so nothing covered
;; them at all.
(module
  (type $leaf (struct (field i32)))
  (type $node (struct (field (mut anyref))))     ;; for multi-level reachability
  (type $arr  (array (mut anyref)))
  (type $i64arr (array (mut i64)))               ;; for the large-object-space case

  (import "host" "collect" (func $collect))      ;; run a GC right here, with this frame live

  (global $g (mut anyref) (ref.null any))
  (table $t 2 anyref)

  ;; §4.2.10 + const rules: a global whose INIT is a GC allocation. The tag upgrade that makes this
  ;; a scanned root lives at jav_instance.c's global-init evaluation, whose own comment names the
  ;; use-after-free it prevents — and nothing ever tested it under a collection.
  ;;
  ;; The payload is a 64 KB array DELIBERATELY: a small struct's space is line-shared with live
  ;; neighbours, so Immix never reuses it and a payload re-read stays green even when the root scan
  ;; is broken (verified — the first version of these rows survived their falsifier). A large
  ;; object is individually free()d by los_sweep, so a missed trace is a genuine dangling pointer.
  (global $ginit (mut (ref null $i64arr)) (array.new_default $i64arr (i32.const 8192)))

  ;; §4.2.16: exninst ::= {tag tagaddr, fields val*} — exception FIELDS hold vals, including refs.
  ;; The per-tag rtt (jav_exn_rtt_for) builds ref_offsets from the field tags; same LOS payload,
  ;; same reason.
  (tag $er (param (ref null $i64arr)))
  (global $hexn (mut exnref) (ref.null exn))

  ;; §4.2.12: eleminst ::= {type elemtype, refs ref*} — element instances HOLD REFERENCES, and 3.0
  ;; elem items are constant expressions, which include GC allocations. A passive segment parks its
  ;; refs from instantiation until table.init/elem.drop; an active one writes them into its table at
  ;; instantiation. Both payloads are 64 KB (the observability rule — see the falsification record
  ;; in the driver).
  (elem $eseg anyref (item (array.new_default $i64arr (i32.const 8192))))
  (elem (table $t) (i32.const 1) anyref (item (array.new_default $i64arr (i32.const 8192))))

  ;; ── root: MODULE GLOBAL ──────────────────────────────────────────────────
  (func (export "global_hold") (global.set $g (struct.new $leaf (i32.const 42))))
  (func (export "global_read") (result i32)
    (struct.get $leaf 0 (ref.cast (ref $leaf) (global.get $g))))
  (func (export "global_drop") (global.set $g (ref.null any)))

  ;; ── root: TABLE ENTRY ────────────────────────────────────────────────────
  (func (export "table_hold") (table.set $t (i32.const 0) (struct.new $leaf (i32.const 42))))
  (func (export "table_read") (result i32)
    (struct.get $leaf 0 (ref.cast (ref $leaf) (table.get $t (i32.const 0)))))
  (func (export "table_drop") (table.set $t (i32.const 0) (ref.null any)))

  ;; $churn: allocate N structs carrying a POISON payload and drop them. This is what makes the
  ;; tests below real. A wrongly-freed object's memory is not overwritten by the collection itself,
  ;; so simply re-reading it still returns the right answer and the test passes on a broken GC —
  ;; verified: disabling the locals root scan entirely left every row green until this existed.
  ;; Churning after the collection forces the freed space to be REUSED, so a lost root shows up as
  ;; the poison value (or a fault) rather than as an intact payload.
  (func $churn (param $n i32)
    (local $i i32)
    (block $done
      (loop $more
        (br_if $done (i32.ge_s (local.get $i) (local.get $n)))
        (drop (struct.new $leaf (i32.const 0x5EED)))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $more))))

  ;; Baseline: a mid-execution collection with nothing held. Every case below must count strictly
  ;; more surviving bytes than this.
  (func (export "nothing_held") (call $collect))

  ;; ── root: FRAME LOCAL, collected mid-execution ───────────────────────────
  ;; The object is reachable ONLY from a local while $collect runs. If locals are not scanned it is
  ;; freed, the churn reuses its space, and the read returns poison instead of 42.
  (func (export "local_survives_gc") (result i32)
    (local $x (ref $leaf))
    (local.set $x (struct.new $leaf (i32.const 42)))
    (call $collect)
    (call $churn (i32.const 256))
    (call $collect)
    (call $churn (i32.const 256))
    (struct.get $leaf 0 (local.get $x)))

  ;; ── root: OPERAND STACK, collected mid-execution ─────────────────────────
  ;; The object is reachable only as an operand under the call.
  (func (export "operand_survives_gc") (result i32)
    (struct.new $leaf (i32.const 42))
    (call $collect)
    (call $churn (i32.const 256))
    (call $collect)
    (call $churn (i32.const 256))
    (struct.get $leaf 0))

  ;; ── transitive reachability through an aggregate, mid-execution ──────────
  (func (export "transitive_survives_gc") (result i32)
    (local $n (ref $node))
    (local.set $n (struct.new $node (struct.new $leaf (i32.const 42))))
    (call $collect)
    (call $churn (i32.const 256))
    (call $collect)
    (call $churn (i32.const 256))
    (struct.get $leaf 0 (ref.cast (ref $leaf) (struct.get $node 0 (local.get $n)))))

  (func (export "array_elem_survives_gc") (result i32)
    (local $a (ref $arr))
    (local.set $a (array.new $arr (struct.new $leaf (i32.const 42)) (i32.const 1)))
    (call $collect)
    (call $churn (i32.const 256))
    (call $collect)
    (call $churn (i32.const 256))
    (struct.get $leaf 0 (ref.cast (ref $leaf) (array.get $arr (local.get $a) (i32.const 0)))))

  ;; ── the OTHER half: unreachable objects must actually be reclaimed ───────
  ;; Allocate and drop on the floor; the host collects afterwards and checks the heap shrank back.
  (func (export "garbage") (drop (struct.new $leaf (i32.const 42))))

  ;; churn, exported: the driver interleaves it with collections so a wrongly-freed object's space
  ;; is REUSED (poison 0x5EED) instead of merely re-read.
  (func (export "churn256") (call $churn (i32.const 256)))

  ;; ── root: GLOBAL INITIALIZED BY A GC CONST-EXPR (§4.2.10) ─────────────────
  (func (export "init_global_read") (result i32)   ;; 42 iff length survived intact
    (select (i32.const 42) (i32.const 0)
      (i32.eq (array.len (global.get $ginit)) (i32.const 8192))))

  ;; ── EXCEPTION FIELDS (§4.2.16): a ref payload traced through the exn instance ──
  ;; §3.4.1 catch_ref: the label receives the tag's params AND the exnref — `t* (ref exn)` — so a
  ;; one-param tag needs a two-result block.
  (func (export "exn_field_hold")
    (local $a (ref $i64arr))
    (local.set $a (array.new_default $i64arr (i32.const 8192)))
    (array.set $i64arr (local.get $a) (i32.const 7) (i64.const 42))
    (block $b (result (ref null $i64arr) exnref)
      (try_table (catch_ref $er $b)
        (throw $er (local.get $a)))
      (unreachable))
    (global.set $hexn)     ;; pops the exnref (top of stack)
    (drop))                ;; drops the payload ref — the exn instance itself keeps it alive
  ;; read the field back by re-throwing the HELD exnref and catching its payload
  (func (export "exn_field_read") (result i32)
    (local $p (ref null $i64arr))
    (local.set $p
      (block $b (result (ref null $i64arr))
        (try_table (catch $er $b) (throw_ref (global.get $hexn)))
        (unreachable)))
    (select (i32.const 42) (i32.const 0)
      (i32.and
        (i32.eq (array.len (local.get $p)) (i32.const 8192))
        (i64.eq (array.get $i64arr (local.get $p) (i32.const 7)) (i64.const 42)))))
  (func (export "exn_drop") (global.set $hexn (ref.null exn)))
  (func (export "init_global_drop") (global.set $ginit (ref.null $i64arr)))

  ;; ── LARGE OBJECT SPACE: a 64 KB array (> the ~32 KB LOS threshold) is marked in place, never
  ;; evacuated, and swept by its own path (los_sweep). Held in the same $g root.
  (func (export "los_hold")
    (local $a (ref $i64arr))
    (local.set $a (array.new_default $i64arr (i32.const 8192)))
    (array.set $i64arr (local.get $a) (i32.const 0) (i64.const 42))
    (global.set $g (local.get $a)))
  (func (export "los_read") (result i32)
    (local $a (ref $i64arr))
    (local.set $a (ref.cast (ref $i64arr) (global.get $g)))
    (select (i32.const 42) (i32.const 0)
      (i32.and
        (i32.eq (array.len (local.get $a)) (i32.const 8192))
        (i64.eq (array.get $i64arr (local.get $a) (i32.const 0)) (i64.const 42)))))

  ;; ── ELEMENT INSTANCES (§4.2.12) ───────────────────────────────────────────
  ;; passive: the parked ref must have survived every collection since instantiation, and
  ;; table.init must carry its RUNTIME TAG into the table (not a hardcoded funcref tag)
  (func (export "elem_init_read") (result i32)
    (table.init $t $eseg (i32.const 0) (i32.const 0) (i32.const 1))
    (select (i32.const 42) (i32.const 0)
      (i32.eq (array.len (ref.cast (ref $i64arr) (table.get $t (i32.const 0)))) (i32.const 8192))))
  ;; active: written into slot 1 at instantiation; the slot's tag decides whether it was traced
  (func (export "active_elem_read") (result i32)
    (select (i32.const 42) (i32.const 0)
      (i32.eq (array.len (ref.cast (ref $i64arr) (table.get $t (i32.const 1)))) (i32.const 8192))))
  (func (export "elem_drop_all")
    (elem.drop $eseg)
    (table.set $t (i32.const 0) (ref.null any))
    (table.set $t (i32.const 1) (ref.null any)))

  ;; ── FRAGMENTATION / EVACUATION stress: 64 live leaves interleaved with garbage, then repeated
  ;; churn+collect cycles. When opportunistic evacuation fires, every held slot must be rewritten to
  ;; the forwarded copy with its payload intact; a stale pointer reads poison or faults. (Immix
  ;; evacuation is opportunistic, so this row cannot PROVE evacuation ran — it pins that WHEN it
  ;; runs, no held object is corrupted. The deterministic mechanics are unit-tested in
  ;; test_gc_evac_stress; this is the engine-path complement.)
  (func (export "frag_hold")
    (local $a (ref $arr)) (local $i i32)
    (local.set $a (array.new_default $arr (i32.const 64)))
    (block $done (loop $more
      (br_if $done (i32.ge_s (local.get $i) (i32.const 64)))
      (array.set $arr (local.get $a) (local.get $i) (struct.new $leaf (local.get $i)))
      (drop (struct.new $leaf (i32.const 0x5EED)))          ;; interleave garbage → fragmentation
      (drop (struct.new $leaf (i32.const 0x5EED)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $more)))
    (global.set $g (local.get $a)))
  (func (export "frag_check") (result i32)                  ;; sum of 0..63 = 2016 iff all intact
    (local $a (ref $arr)) (local $i i32) (local $sum i32)
    (local.set $a (ref.cast (ref $arr) (global.get $g)))
    (block $done (loop $more
      (br_if $done (i32.ge_s (local.get $i) (i32.const 64)))
      (local.set $sum (i32.add (local.get $sum)
        (struct.get $leaf 0 (ref.cast (ref $leaf) (array.get $arr (local.get $a) (local.get $i))))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $more)))
    (local.get $sum))
)
