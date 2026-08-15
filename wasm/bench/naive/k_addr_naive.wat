;; k_addr — memory sum with template-producer addressing.
;; The NAIVE lowering: every element address is base + (i * 4) + 0, the scale a
;; real multiply, a |0 "normalization" on the address, an xor-0 on the
;; accumulator, and a cleared-register idiom (i - i) folded into the sum.
;; Identical arithmetic to k_addr_clean by construction: every extra op is an
;; identity, so the checksum cannot differ.
(module
  (memory 1)
  (func (export "run") (param $n i32) (result i32)
    (local $i i32) (local $r i32) (local $acc i32)
    ;; init: mem[i*4] = i*7+3
    (local.set $i (i32.const 0))
    (block $I (loop $LI
      (br_if $I (i32.ge_u (local.get $i) (i32.const 256)))
      (i32.store
        (i32.or (i32.add (i32.mul (local.get $i) (i32.const 4)) (i32.const 0))
                (i32.const 0))
        (i32.add (i32.mul (local.get $i) (i32.const 7)) (i32.const 3)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $LI)))
    ;; n passes over the array
    (block $R (loop $LR
      (br_if $R (i32.ge_u (local.get $r) (local.get $n)))
      (local.set $i (i32.const 0))
      (block $S (loop $LS
        (br_if $S (i32.ge_u (local.get $i) (i32.const 256)))
        (local.set $acc
          (i32.add
            (i32.xor (local.get $acc) (i32.const 0))
            (i32.add
              (i32.load
                (i32.or (i32.or (i32.add (i32.mul (local.get $i) (i32.const 4))
                                         (i32.const 0))
                                (i32.const 0))
                        (i32.and (local.get $i) (i32.const 0))))
              (i32.sub (local.get $i) (local.get $i)))))
        (local.set $i (i32.add (i32.mul (local.get $i) (i32.const 1)) (i32.const 1)))
        (br $LS)))
      (local.set $r (i32.add (local.get $r) (i32.const 1)))
      (br $LR)))
    (local.get $acc)))
