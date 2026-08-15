;; k_poly — Horner evaluation, macro-expanded NAIVE. The i32 rule-family
;; sweep: every step wears a different identity (* 1, - 0, & -1, ^ (x-x),
;; shr 0, + t*0), coefficients stay as inline constant arithmetic, the
;; template writes constants on the LEFT (0 + x, -1 & c — the comm shapes),
;; refold chains appear in the scale (((i*2)*4), the mask ((x&255)&15), the
;; counter ((i+1)+2)), and the accumulator absorbs all ten self-compares and
;; two constant compares. Every junk op is an identity or a differently
;; spelled equal, so the checksum equals k_poly_clean's by construction.
(module
  (func (export "run") (param $n i32) (result i32)
    (local $i i32) (local $x i32) (local $a i32) (local $t i32) (local $acc i32)
    (block $R (loop $LR
      (br_if $R (i32.ge_u (local.get $i) (local.get $n)))
      ;; x = ((i*2)*4) + 7   — mul refold, then strength to shl 3
      (local.set $x (i32.add (i32.mul (i32.mul (local.get $i) (i32.const 2))
                                      (i32.const 4))
                             (i32.const 7)))
      ;; x = (0 + x) | 0     — left-zero add (comm) + or 0
      (local.set $x (i32.or (i32.add (i32.const 0) (local.get $x)) (i32.const 0)))
      (local.set $a (i32.const 0))
      (local.set $a (i32.add (i32.add (i32.mul (i32.mul (local.get $a) (local.get $x)) (i32.const 1))
                                      (i32.add (i32.mul (i32.const 2) (i32.const 3)) (i32.const 1)))
                             (i32.const 0)))
      (local.set $a (i32.or (i32.add (i32.sub (i32.mul (local.get $a) (local.get $x)) (i32.const 0))
                                     (i32.const 25))
                            (i32.const 0)))
      (local.set $a (i32.add (i32.and (i32.mul (local.get $a) (local.get $x)) (i32.const -1))
                             (i32.and (i32.const -1) (i32.const 61))))
      (local.set $a (i32.xor (i32.add (i32.mul (local.get $a) (local.get $x)) (i32.const 41))
                             (i32.sub (local.get $x) (local.get $x))))
      (local.set $a (i32.add (i32.shr_u (i32.mul (local.get $a) (local.get $x)) (i32.const 0))
                             (i32.xor (i32.const 0) (i32.const 57))))
      (local.set $a (i32.add (i32.add (i32.shr_s (i32.mul (local.get $a) (local.get $x)) (i32.const 0))
                                      (i32.const 23))
                             (i32.mul (local.get $x) (i32.const 0))))
      ;; the and/or/xor refold chains
      (local.set $t (i32.add (i32.add (i32.and (i32.and (local.get $x) (i32.const 255)) (i32.const 15))
                                      (i32.or (i32.or (local.get $x) (i32.const 1)) (i32.const 2)))
                             (i32.xor (i32.xor (i32.xor (local.get $x) (i32.const 5)) (i32.const 3))
                                      (i32.xor (local.get $x) (local.get $x)))))
      ;; all ten self-compares (sum 5) and two constant compares (sum 1)
      (local.set $acc (i32.add (local.get $acc) (local.get $a)))
      (local.set $acc (i32.add (local.get $acc) (local.get $t)))
      (local.set $acc
        (i32.add (local.get $acc)
          (i32.add
            (i32.add
              (i32.add (i32.add (i32.eq (local.get $x) (local.get $x))
                                (i32.ne (local.get $x) (local.get $x)))
                       (i32.add (i32.lt_s (local.get $x) (local.get $x))
                                (i32.lt_u (local.get $x) (local.get $x))))
              (i32.add (i32.add (i32.gt_s (local.get $x) (local.get $x))
                                (i32.gt_u (local.get $x) (local.get $x)))
                       (i32.add (i32.le_s (local.get $x) (local.get $x))
                                (i32.le_u (local.get $x) (local.get $x)))))
            (i32.add (i32.add (i32.ge_s (local.get $x) (local.get $x))
                              (i32.ge_u (local.get $x) (local.get $x)))
                     (i32.add (i32.gt_u (i32.const 5) (i32.const 4))
                              (i32.eq (i32.const 3) (i32.const 7)))))))
      ;; i = (i+1)+2 — the add refold
      (local.set $i (i32.add (i32.add (local.get $i) (i32.const 1)) (i32.const 2)))
      (br $LR)))
    (local.get $acc)))
