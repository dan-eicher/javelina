;; k_smand — the same iteration hand-lowered clean: z = z*z + (z<<1) + c.
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
            (i32.add (i32.add (i32.mul (local.get $z) (local.get $z))
                              (i32.shl (local.get $z) (i32.const 1)))
                     (local.get $c)))
          (local.set $s (i32.add (local.get $s) (i32.const 1)))
          (br $LS)))
        (local.set $acc (i32.xor (local.get $acc) (local.get $z)))
        (local.set $k (i32.add (local.get $k) (i32.const 1)))
        (br $LK)))
      (local.set $r (i32.add (local.get $r) (i32.const 1)))
      (br $LR)))
    (local.get $acc)))
