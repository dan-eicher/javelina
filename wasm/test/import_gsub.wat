;; import_gsub.wasm — an IMPORTED global whose type participates in subtyping:
;; the module imports "e"."g" as an immutable funcref global and exports a
;; function returning it, so instantiation has to match the supplied extern's
;; type against the declared one (§4.5.4 import matching).
;;
;; Assembles byte-for-byte to the committed import_gsub.wasm:
;;   water import_gsub.wat -o import_gsub.wasm
(module
  (import "e" "g" (global funcref))
  (func (export "f") (result funcref)
    global.get 0))
