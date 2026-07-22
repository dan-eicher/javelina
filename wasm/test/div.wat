;; div.wasm — a non-trapping i32.div_s (10 / 3 == 3), the control for div0.wat.
;; Used by test_div to check the divide path returns rather than traps.
;;
;; Assembles byte-for-byte to the committed div.wasm:
;;   water div.wat -o div.wasm
(module
  (func (result i32)
    i32.const 10
    i32.const 3
    i32.div_s))
