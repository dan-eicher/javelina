(module
  (table 2 funcref)
  (memory 1)
  (func $a)
  (data "hi")
  (data (i32.const 0) "XY")
  (elem func 0)
  (elem (i32.const 0) func 0)
  (export "a" (func $a)))
