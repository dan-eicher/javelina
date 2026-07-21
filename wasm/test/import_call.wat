(module
  (import "env" "f" (func $f (param i32) (result i32)))
  (func $callit (param i32) (result i32) local.get 0 call $f)
  (export "callit" (func $callit)))
