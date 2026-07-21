;; Module B: imports A's getter, has its OWN global = 222, calls the import.
(module
  (import "a" "getg" (func $getg (result i32)))
  (global $g i32 (i32.const 222))
  (func (export "callg") (result i32) (call $getg))
)
