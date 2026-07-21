;; Cross-instance call context, module A: a global = 111 and a getter that returns it.
;; B (xinst_gb.wat) imports this getter. When B calls it, §4.2.6 says the getter runs
;; against A's instance, so it must read A's global (111), not B's own global (222).
(module
  (global $g i32 (i32.const 111))
  (func (export "getg") (result i32) (global.get $g))
)
