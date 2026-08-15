;; k_wide — the 64-bit rule-family sweep, in a producer that normalizes every
;; value through i64 registers: wrap(extend) round trips on the 32-bit side,
;; and on the 64-bit accumulator the full identity chain (| 0, & -1, ^ 0,
;; + 0, - 0, * 1, shifts by 0), the left-const comm shapes (0 + w, 1 * w),
;; the add/mul/and/or/xor refold chains, a *4*64 scale that folds to shl 8,
;; zero products, self-subtractions, all ten 64-bit self-compares and a
;; 64-bit constant compare. Checksum equals k_wide_clean's by construction.
(module
  (func (export "run") (param $n i32) (result i32)
    (local $i i32) (local $acc i32) (local $w i64) (local $t i64)
    (block $R (loop $LR
      (br_if $R (i32.ge_u (local.get $i) (local.get $n)))
      ;; the 32-bit side: wrap(extend) both ways
      (local.set $acc
        (i32.add (i32.wrap_i64 (i64.extend_i32_s (local.get $acc)))
                 (i32.mul (i32.wrap_i64 (i64.extend_i32_u (local.get $i)))
                          (i32.const 1))))
      (local.set $acc
        (i32.xor (i32.wrap_i64 (i64.extend_i32_s (local.get $acc)))
                 (i32.const 40503)))
      ;; the 64-bit identity chain
      (local.set $w
        (i64.sub (i64.add (i64.xor (i64.and (i64.or (local.get $w) (i64.const 0))
                                            (i64.const -1))
                                   (i64.const 0))
                          (i64.const 0))
                 (i64.const 0)))
      ;; left-const comm shapes, then the real feed
      (local.set $w (i64.add (i64.const 0) (local.get $w)))
      (local.set $w (i64.mul (i64.const 1) (local.get $w)))
      (local.set $w (i64.add (local.get $w) (i64.extend_i32_s (local.get $acc))))
      ;; refolds: (w+1)+2, then ((w*4)*64) -> *256 -> shl 8
      (local.set $w (i64.add (i64.add (local.get $w) (i64.const 1)) (i64.const 2)))
      (local.set $w (i64.mul (i64.mul (local.get $w) (i64.const 4)) (i64.const 64)))
      ;; and/or/xor refold chains
      (local.set $t (i64.add (i64.add (i64.and (i64.and (local.get $w) (i64.const 255)) (i64.const 15))
                                      (i64.or (i64.or (local.get $w) (i64.const 1)) (i64.const 2)))
                             (i64.xor (i64.xor (local.get $w) (i64.const 5)) (i64.const 3))))
      ;; self erasers and zero products, shifts by zero, self/zero bitwise
      (local.set $w (i64.add (local.get $w) (i64.sub (local.get $t) (local.get $t))))
      (local.set $w (i64.add (local.get $w) (i64.mul (local.get $t) (i64.const 0))))
      (local.set $w (i64.add (local.get $w) (i64.xor (local.get $t) (local.get $t))))
      (local.set $w (i64.or (local.get $w) (i64.and (local.get $t) (i64.const 0))))
      (local.set $w (i64.and (local.get $w) (local.get $w)))
      (local.set $w (i64.or (local.get $w) (local.get $w)))
      (local.set $w (i64.shr_u (i64.shr_s (i64.shl (local.get $w) (i64.const 0))
                                          (i64.const 0))
                               (i64.const 0)))
      (local.set $w (i64.add (local.get $w) (local.get $t)))
      ;; ten 64-bit self-compares (sum 5) and one 64-bit constant compare
      (local.set $acc
        (i32.add (local.get $acc)
          (i32.add
            (i32.add
              (i32.add (i32.add (i64.eq (local.get $w) (local.get $w))
                                (i64.ne (local.get $w) (local.get $w)))
                       (i32.add (i64.lt_s (local.get $w) (local.get $w))
                                (i64.lt_u (local.get $w) (local.get $w))))
              (i32.add (i32.add (i64.gt_s (local.get $w) (local.get $w))
                                (i64.gt_u (local.get $w) (local.get $w)))
                       (i32.add (i64.le_s (local.get $w) (local.get $w))
                                (i64.le_u (local.get $w) (local.get $w)))))
            (i32.add (i32.add (i64.ge_s (local.get $w) (local.get $w))
                              (i64.ge_u (local.get $w) (local.get $w)))
                     (i64.gt_u (i64.const 5) (i64.const 4))))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $LR)))
    (i32.xor (local.get $acc) (i32.wrap_i64 (local.get $w)))))
