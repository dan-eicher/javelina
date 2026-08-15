;; k_wide — the same arithmetic staying at width, every identity folded:
;; 32-bit ops in 32 bits, (w+3)<<8, folded masks, the compare sum as 6.
(module
  (func (export "run") (param $n i32) (result i32)
    (local $i i32) (local $acc i32) (local $w i64) (local $t i64)
    (block $R (loop $LR
      (br_if $R (i32.ge_u (local.get $i) (local.get $n)))
      (local.set $acc (i32.add (local.get $acc) (local.get $i)))
      (local.set $acc (i32.xor (local.get $acc) (i32.const 40503)))
      (local.set $w (i64.add (local.get $w) (i64.extend_i32_s (local.get $acc))))
      (local.set $w (i64.shl (i64.add (local.get $w) (i64.const 3)) (i64.const 8)))
      (local.set $t (i64.add (i64.add (i64.and (local.get $w) (i64.const 15))
                                      (i64.or (local.get $w) (i64.const 3)))
                             (i64.xor (local.get $w) (i64.const 6))))
      (local.set $w (i64.add (local.get $w) (local.get $t)))
      (local.set $acc (i32.add (local.get $acc) (i32.const 6)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $LR)))
    (i32.xor (local.get $acc) (i32.wrap_i64 (local.get $w)))))
