;; Cross-instance store test, module B: imports module A's memory and reads mem[8].
;; If the shared-store model is correct, B sees the 42 that A wrote (same meminst).
(module
  (memory (import "a" "mem") 1)
  (func (export "peek") (result i32) (i32.load8_u (i32.const 8)))
)
