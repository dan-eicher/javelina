;; div0.wasm — i32.div_s by ZERO. The §4.3.2 trap fixture: identical to div.wat
;; except the divisor, so a test can attribute the trap to the division and
;; nothing else. Used by test_div.
;;
;; Assembles byte-for-byte to the committed div0.wasm:
;;   water div0.wat -o div0.wasm
(module
  (func (result i32)
    i32.const 10
    i32.const 0
    i32.div_s))
