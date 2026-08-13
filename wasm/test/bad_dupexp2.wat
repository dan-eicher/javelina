;; §3.5.10 export names not distinct — the duplicate pair is (0,3), NOT adjacent.
;; bad_dupexp.wat's dup is (0,1), which a uniqueness check that only ever compares
;; against the immediately preceding export still rejects. This one separates the
;; pair by two distinct names, so every earlier export must still be reachable and
;; correctly addressed at the point export 3 is checked.
(module (func) (func) (func) (func)
  (export "a" (func 0))
  (export "b" (func 1))
  (export "c" (func 2))
  (export "a" (func 3)))
