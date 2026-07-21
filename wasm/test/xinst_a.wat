;; Cross-instance store test, module A: owns a memory, exports it, and a func that
;; writes 42 to mem[8]. Paired with xinst_b.wat (imports this memory). The test puts a
;; dummy memory at store index 0 first, so A's memory is NOT at heap index 0 — exposing
;; whether the engine resolves memidx through the instance's memaddr map (§4.2.3 store)
;; or as a raw heap index.
(module
  (memory (export "mem") 1)
  (func (export "poke") (i32.store8 (i32.const 8) (i32.const 42)))
)
