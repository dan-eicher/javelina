;; k_vmand — k_smand's iteration four lanes at a time (i32x4), cells 4b+lane,
;; same constants, so the scalar and vector checksums are equal by
;; construction. The NAIVE lowering drags the vector template's junk through
;; every step: & all-ones, * splat(1), + zero, ^ zero, an identity shuffle, a
;; shift by 0, the doubling spelled as * splat(2) (strength target), and a
;; bitselect against an all-ones mask closing each cell.
(module
  (func (export "run") (param $n i32) (result i32)
    (local $r i32) (local $b i32) (local $s i32) (local $acc i32)
    (local $z v128) (local $c v128) (local $t v128)
    (block $R (loop $LR
      (br_if $R (i32.ge_u (local.get $r) (local.get $n)))
      (local.set $b (i32.const 0))
      (block $B (loop $LB
        (br_if $B (i32.ge_u (local.get $b) (i32.const 64)))
        (local.set $c
          (v128.xor
            (i32x4.mul
              (i32x4.add
                (i32x4.add (i32x4.splat (i32.shl (local.get $b) (i32.const 2)))
                           (v128.const i32x4 0 1 2 3))
                (i32x4.splat (i32.mul (local.get $r) (i32.const 17))))
              (v128.const i32x4 2654435761 2654435761 2654435761 2654435761))
            (v128.const i32x4 2654435769 2654435769 2654435769 2654435769)))
        (local.set $z (local.get $c))
        (local.set $s (i32.const 0))
        (block $S (loop $LS
          (br_if $S (i32.ge_u (local.get $s) (i32.const 8)))
          (local.set $t
            (i32x4.mul (v128.and (i32x4.mul (local.get $z) (local.get $z))
                                 (v128.const i32x4 -1 -1 -1 -1))
                       (v128.const i32x4 1 1 1 1)))
          (local.set $t
            (v128.xor
              (i32x4.add
                (i32x4.add (local.get $t)
                           (i32x4.mul (local.get $z)
                                      (v128.const i32x4 2 2 2 2)))
                (i32x4.add (local.get $c) (v128.const i32x4 0 0 0 0)))
              (v128.const i32x4 0 0 0 0)))
          (local.set $z
            (i32x4.shl
              (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
                             (local.get $t) (local.get $t))
              (i32.const 0)))
          (local.set $s (i32.add (local.get $s) (i32.const 1)))
          (br $LS)))
        ;; the per-cell width sweep: every vector family once, each an identity
        (local.set $z (i16x8.add (v128.const i32x4 0 0 0 0) (local.get $z)))
        (local.set $z (i16x8.sub (local.get $z) (v128.const i32x4 0 0 0 0)))
        (local.set $z (i16x8.mul (v128.const i16x8 1 1 1 1 1 1 1 1) (local.get $z)))
        (local.set $z (i16x8.shr_s (i16x8.shr_u (i16x8.shl (local.get $z) (i32.const 0))
                                                (i32.const 0))
                                   (i32.const 0)))
        (local.set $z (i8x16.add (v128.const i32x4 0 0 0 0) (local.get $z)))
        (local.set $z (i8x16.sub (local.get $z) (v128.const i32x4 0 0 0 0)))
        (local.set $z (i8x16.shr_s (i8x16.shr_u (i8x16.shl (local.get $z) (i32.const 0))
                                                (i32.const 0))
                                   (i32.const 0)))
        (local.set $z (i64x2.add (v128.const i32x4 0 0 0 0) (local.get $z)))
        (local.set $z (i64x2.sub (local.get $z) (v128.const i32x4 0 0 0 0)))
        (local.set $z (i64x2.mul (v128.const i64x2 1 1) (local.get $z)))
        (local.set $z (i64x2.shr_s (i64x2.shr_u (i64x2.shl (local.get $z) (i32.const 0))
                                                (i32.const 0))
                                   (i32.const 0)))
        (local.set $z (i32x4.shr_s (i32x4.shr_u (local.get $z) (i32.const 0))
                                   (i32.const 0)))
        (local.set $z (i32x4.sub (local.get $z) (v128.const i32x4 0 0 0 0)))
        (local.set $z (v128.or (v128.const i32x4 0 0 0 0) (local.get $z)))
        (local.set $z (v128.and (v128.const i32x4 -1 -1 -1 -1) (local.get $z)))
        (local.set $z (v128.xor (v128.const i32x4 0 0 0 0) (local.get $z)))
        (local.set $z (v128.and (local.get $z) (local.get $z)))
        (local.set $z (v128.or (local.get $z) (local.get $z)))
        (local.set $z (v128.not (v128.not (local.get $z))))
        (local.set $z (v128.andnot (local.get $z) (v128.const i32x4 0 0 0 0)))
        (local.set $z (v128.xor (v128.xor (local.get $z)
                                          (v128.const i32x4 -1 -1 -1 -1))
                                (v128.const i32x4 -1 -1 -1 -1)))
        ;; absorbers reached through a live operand
        (local.set $z (v128.and (local.get $z)
                                (v128.or (local.get $c) (v128.const i32x4 -1 -1 -1 -1))))
        (local.set $z (v128.or (local.get $z)
                               (v128.and (local.get $c) (v128.const i32x4 0 0 0 0))))
        ;; self erasers and zero products, fed into adds
        (local.set $t (v128.xor (local.get $z) (local.get $z)))
        (local.set $z (i32x4.add (local.get $z) (local.get $t)))
        (local.set $z (i32x4.add (local.get $z) (i8x16.sub (local.get $c) (local.get $c))))
        (local.set $z (i32x4.add (local.get $z) (i16x8.sub (local.get $c) (local.get $c))))
        (local.set $z (i32x4.add (local.get $z) (i32x4.sub (local.get $c) (local.get $c))))
        (local.set $z (i64x2.add (local.get $z) (i64x2.sub (local.get $c) (local.get $c))))
        (local.set $z (i32x4.add (local.get $z) (i16x8.mul (local.get $c) (v128.const i32x4 0 0 0 0))))
        (local.set $z (i32x4.add (local.get $z) (i32x4.mul (local.get $c) (v128.const i32x4 0 0 0 0))))
        (local.set $z (i64x2.add (local.get $z) (i64x2.mul (local.get $c) (v128.const i32x4 0 0 0 0))))
        ;; the second shuffle identity: every lane from the SECOND operand
        (local.set $z (i8x16.shuffle 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
                                     (local.get $c) (local.get $z)))
        ;; bitselect with an all-zero mask picks b
        (local.set $z (v128.bitselect (local.get $c) (local.get $z)
                                      (v128.const i32x4 0 0 0 0)))
        ;; the strength rules, exercised then erased: the mul-by-splat-pow2
        ;; is computed (tier 2 pays for it) and rewrites to a shift (the rule
        ;; fires), and the self-subtraction turns the whole term to zero, so
        ;; the value is untouched and the scalar twin needs no 16/64-lane
        ;; emulation
        (local.set $t (i16x8.mul (local.get $z) (v128.const i16x8 4 4 4 4 4 4 4 4)))
        (local.set $z (i32x4.add (local.get $z) (i16x8.sub (local.get $t) (local.get $t))))
        (local.set $t (i64x2.mul (local.get $z) (v128.const i64x2 8 8)))
        (local.set $z (i32x4.add (local.get $z) (i64x2.sub (local.get $t) (local.get $t))))
        (local.set $z (v128.bitselect (local.get $z) (local.get $c)
                                      (v128.const i32x4 -1 -1 -1 -1)))
        (local.set $acc
          (i32.xor (i32.xor (i32.xor (i32.xor (local.get $acc)
                                              (i32x4.extract_lane 0 (local.get $z)))
                                     (i32x4.extract_lane 1 (local.get $z)))
                            (i32x4.extract_lane 2 (local.get $z)))
                   (i32x4.extract_lane 3 (local.get $z))))
        (local.set $b (i32.add (local.get $b) (i32.const 1)))
        (br $LB)))
      (local.set $r (i32.add (local.get $r) (i32.const 1)))
      (br $LR)))
    (local.get $acc)))
