;; k_vmand — the same four-lane iteration hand-lowered clean:
;; z = z*z + (z<<1) + c, nothing else.
(module
  (func (export "run") (param $n i32) (result i32)
    (local $r i32) (local $b i32) (local $s i32) (local $acc i32)
    (local $z v128) (local $c v128)
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
          (local.set $z
            (i32x4.add
              (i32x4.add (i32x4.mul (local.get $z) (local.get $z))
                         (i32x4.shl (local.get $z) (i32.const 1)))
              (local.get $c)))
          (local.set $s (i32.add (local.get $s) (i32.const 1)))
          (br $LS)))
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
