;; Stage-2 regression: two REAL instances in one store. A exports a memory and writes 42;
;; B imports A's memory and reads it. Cross-instance visibility requires a SHARED store/heap
;; (the runner gave each module its own heap → this fails until stage 2).
(module $A
  (memory (export "mem") 1)
  (func (export "poke") (i32.store (i32.const 0) (i32.const 42)))
)
(register "a" $A)
(module $B
  (memory (import "a" "mem") 1)
  (func (export "peek") (result i32) (i32.load (i32.const 0)))
)
(invoke $A "poke")
(assert_return (invoke $B "peek") (i32.const 42))
