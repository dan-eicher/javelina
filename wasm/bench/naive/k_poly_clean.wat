;; k_poly — the same evaluation hand-lowered clean: shl scale, folded
;; coefficients (7, 25, 61, 41, 57, 23), folded masks, folded compare sums.
(module
  (func (export "run") (param $n i32) (result i32)
    (local $i i32) (local $x i32) (local $a i32) (local $t i32) (local $acc i32)
    (block $R (loop $LR
      (br_if $R (i32.ge_u (local.get $i) (local.get $n)))
      (local.set $x (i32.add (i32.shl (local.get $i) (i32.const 3)) (i32.const 7)))
      (local.set $a (i32.const 0))
      (local.set $a (i32.add (i32.mul (local.get $a) (local.get $x)) (i32.const 7)))
      (local.set $a (i32.add (i32.mul (local.get $a) (local.get $x)) (i32.const 25)))
      (local.set $a (i32.add (i32.mul (local.get $a) (local.get $x)) (i32.const 61)))
      (local.set $a (i32.add (i32.mul (local.get $a) (local.get $x)) (i32.const 41)))
      (local.set $a (i32.add (i32.mul (local.get $a) (local.get $x)) (i32.const 57)))
      (local.set $a (i32.add (i32.mul (local.get $a) (local.get $x)) (i32.const 23)))
      (local.set $t (i32.add (i32.add (i32.and (local.get $x) (i32.const 15))
                                      (i32.or (local.get $x) (i32.const 3)))
                             (i32.xor (local.get $x) (i32.const 6))))
      (local.set $acc (i32.add (local.get $acc) (local.get $a)))
      (local.set $acc (i32.add (local.get $acc) (local.get $t)))
      (local.set $acc (i32.add (local.get $acc) (i32.const 6)))
      (local.set $i (i32.add (local.get $i) (i32.const 3)))
      (br $LR)))
    (local.get $acc)))
