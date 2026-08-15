;; k_smand — scalar integer escape-map iteration (mandelbrot-shaped, wrapping
;; i32, fixed 8 steps per cell): z = z*z + 2z + c. The NAIVE lowering wraps
;; every step in the template's normalizations — * 1, & -1, ^ 0, shl 0 — and
;; spells the doubling as a multiply (the strength-reduction target; the clean
;; twin writes shl 1, so the values are equal and only the spelling differs).
(module
  (func (export "run") (param $n i32) (result i32)
    (local $r i32) (local $k i32) (local $s i32) (local $z i32) (local $c i32) (local $acc i32)
    (block $R (loop $LR
      (br_if $R (i32.ge_u (local.get $r) (local.get $n)))
      (local.set $k (i32.const 0))
      (block $K (loop $LK
        (br_if $K (i32.ge_u (local.get $k) (i32.const 256)))
        (local.set $c
          (i32.xor (i32.mul (i32.add (local.get $k)
                                     (i32.mul (local.get $r) (i32.const 17)))
                            (i32.const 2654435761))
                   (i32.const 2654435769)))
        (local.set $z (local.get $c))
        (local.set $s (i32.const 0))
        (block $S (loop $LS
          (br_if $S (i32.ge_u (local.get $s) (i32.const 8)))
          (local.set $z
            (i32.shl
              (i32.and
                (i32.xor
                  (i32.add
                    (i32.add (i32.mul (i32.mul (local.get $z) (local.get $z))
                                      (i32.const 1))
                             (i32.mul (local.get $z) (i32.const 2)))
                    (i32.add (local.get $c) (i32.const 0)))
                  (i32.const 0))
                (i32.const -1))
              (i32.const 0)))
          (local.set $z (i32.or (local.get $z) (local.get $z)))
          (local.set $z (i32.and (local.get $z) (local.get $z)))
          (local.set $s (i32.add (local.get $s) (i32.const 1)))
          (br $LS)))
        (local.set $acc (i32.xor (local.get $acc) (local.get $z)))
        (local.set $k (i32.add (local.get $k) (i32.const 1)))
        (br $LK)))
      (local.set $r (i32.add (local.get $r) (i32.const 1)))
      (br $LR)))
    (local.get $acc)))
