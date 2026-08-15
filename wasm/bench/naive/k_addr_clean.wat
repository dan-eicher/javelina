;; k_addr — the same sum, hand-lowered clean: shl addressing, no identities.
(module
  (memory 1)
  (func (export "run") (param $n i32) (result i32)
    (local $i i32) (local $r i32) (local $acc i32)
    (local.set $i (i32.const 0))
    (block $I (loop $LI
      (br_if $I (i32.ge_u (local.get $i) (i32.const 256)))
      (i32.store
        (i32.shl (local.get $i) (i32.const 2))
        (i32.add (i32.mul (local.get $i) (i32.const 7)) (i32.const 3)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $LI)))
    (block $R (loop $LR
      (br_if $R (i32.ge_u (local.get $r) (local.get $n)))
      (local.set $i (i32.const 0))
      (block $S (loop $LS
        (br_if $S (i32.ge_u (local.get $i) (i32.const 256)))
        (local.set $acc
          (i32.add (local.get $acc)
                   (i32.load (i32.shl (local.get $i) (i32.const 2)))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $LS)))
      (local.set $r (i32.add (local.get $r) (i32.const 1)))
      (br $LR)))
    (local.get $acc)))
