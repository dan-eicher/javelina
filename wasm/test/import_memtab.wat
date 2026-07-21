(module
  (import "env" "m" (memory 1))
  (import "env" "t" (table 2 funcref))
  (func $f)
  (data (i32.const 0) "yo")
  (elem (i32.const 0) func 0)
  (export "f" (func 0)))
