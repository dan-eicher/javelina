;; add.wasm — the walking skeleton's fixture: one function, no export.
;; test_skeleton/test_load/test_interp recover the code body off the c-lite span
;; index and run it through both tiers, asserting add(3,5) == 8.
;;
;; Assembles byte-for-byte to the committed add.wasm:
;;   water add.wat -o add.wasm
(module
  (func (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add))
