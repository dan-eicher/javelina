;; Stage-3 regression: a funcref crossing instances through a shared table. A fills a table
;; with a funcref to A's $f (returns 42) and exports the table; B imports it and call_indirects
;; index 0 — which must dispatch to A's $f (return 42), NOT index into B's own function space.
;; Requires funcref = an instance-independent handle (funcinst pointer), not a module funcidx.
(module $A
  (func $f (result i32) (i32.const 42))
  (table (export "t") 1 funcref)
  (elem (table 0) (i32.const 0) func $f)
)
(register "a" $A)
(module $B
  (type $ft (func (result i32)))
  (table (import "a" "t") 1 funcref)
  (func (export "callit") (result i32) (call_indirect (type $ft) (i32.const 0)))
)
(assert_return (invoke $B "callit") (i32.const 42))
