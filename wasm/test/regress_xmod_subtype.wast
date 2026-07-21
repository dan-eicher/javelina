;; Stage-5 regression: cross-MODULE closed-type import matching (§3.3.10 / §4.7.2).
;; Import matching compares CLOSED defined types: clos(provided) = clos(required), OR a
;; declared supertype of provided matches required. clos() carries finality AND the full
;; rec-group structure.
;;
;; STATUS: GREEN (all assert_unlinkable below correctly reject). Fixed by the session-wide
;; (heap) canonical-id registry: link_imports interns each module's closed types into ONE
;; global id space (out->gcanon) and compares via jav_ht_sub over those ids, so finality,
;; rec-group structure, and declared-supertype identity all survive cross-module. The old
;; hand-rolled per-field structural matchers (imp_ft_sub/imp_globval_sub) are DELETED.
;; The RUNTIME twin — ref.cast/ref.test on a cross-module GC struct/array ref — is registry-
;; based too (gc_rtt.gid + gcref_closed_matches; regression in test_capi.c).
;;
;; Every case is lifted verbatim from testsuite/type-subtyping.wast (authoritative verdicts);
;; isolated here so the behavior is gated on its own, not buried in the corpus.

;; ── sub-rule 1: FINALITY is part of clos() ───────────────────────────────────
;; (sub (func)) and (sub final (func)) have the same flattened sig but differ in finality.
(module $FIN
  (type $t1 (sub (func)))
  (type $t2 (sub final (func)))
  (func (export "f1") (type $t1))
  (func (export "f2") (type $t2))
)
(register "fin" $FIN)
;; provider f1 : (sub (func));  import wants (sub final (func)) — finality differs → reject
(assert_unlinkable
  (module (type $t2 (sub final (func))) (func (import "fin" "f1") (type $t2)))
  "incompatible import type"
)
;; provider f2 : (sub final (func));  import wants (sub (func)) — finality differs → reject
(assert_unlinkable
  (module (type $t1 (sub (func))) (func (import "fin" "f2") (type $t1)))
  "incompatible import type"
)
;; positive control: exact finality match must still LINK
(module (type $t1 (sub (func))) (func (import "fin" "f1") (type $t1)))

;; ── sub-rule 2: rec-group canonical equality ─────────────────────────────────
;; provider $g2 subs $f2 (a func whose sibling struct fields (ref $f1)); importer $g1 subs $f1.
;; Flattened func sigs are identical (both empty func), but the closed rec-group forms differ.
(module $REC
  (rec (type $f1 (sub (func))) (type (struct (field (ref $f1)))))
  (rec (type $f2 (sub (func))) (type (struct (field (ref $f1)))))
  (rec (type $g2 (sub $f2 (func))) (type (struct)))
  (func (export "g") (type $g2))
)
(register "rec" $REC)
(assert_unlinkable
  (module
    (rec (type $f1 (sub (func))) (type (struct (field (ref $f1)))))
    (rec (type $g1 (sub $f1 (func))) (type (struct)))
    (func (import "rec" "g") (type $g1))
  )
  "incompatible import type"
)
;; positive control: closed forms genuinely equal across modules must LINK
(module $REC_OK
  (rec (type $f2 (sub (func))) (type (struct (field (ref $f2)))))
  (rec (type $g2 (sub $f2 (func))) (type (struct)))
  (func (export "g") (type $g2))
)
(register "rec_ok" $REC_OK)
(module
  (rec (type $f1 (sub (func))) (type (struct (field (ref $f1)))))
  (rec (type $g1 (sub $f1 (func))) (type (struct)))
  (func (import "rec_ok" "g") (type $g1))
)

;; ── other external kinds: §3.3 matching reduces every concrete comparison to the
;;    same closed-type relation. Each must reject a cross-module finality mismatch. ──
;; GLOBAL (§3.3.13: const valtype covariant): a (ref null $t1)[open] global imported as
;; (ref null $t2)[final] — $t1 ≰ $t2 (finality differs) → reject.
(module $GP
  (type $t1 (sub (func)))
  (type $t2 (sub final (func)))
  (global (export "g1") (ref null $t1) (ref.null $t1))
)
(register "gp" $GP)
(assert_unlinkable
  (module (type $t2 (sub final (func))) (global (import "gp" "g1") (ref null $t2)))
  "incompatible import type"
)
;; GLOBAL signature mismatch: provider (ref null $a) where $a=(func), imported as (ref null $b)
;; where $b=(func (param i32)) — different closed func types → reject.
(module $GS (type $a (sub (func))) (global (export "g") (ref null $a) (ref.null $a)))
(register "gs" $GS)
(assert_unlinkable
  (module (type $b (sub (func (param i32)))) (global (import "gs" "g") (ref null $b)))
  "incompatible import type"
)
;; positive control: same closed type links
(module $GO (type $t1 (sub (func))) (global (export "g") (ref null $t1) (ref.null $t1)))
(register "go" $GO)
(module (type $t1 (sub (func))) (global (import "go" "g") (ref null $t1)))

;; TABLE (§3.3.15: reftype INVARIANT): element (ref null $t1)[open] imported as
;; (ref null $t2)[final] — neither direction holds across finality → reject.
(module $TP
  (type $t1 (sub (func)))
  (type $t2 (sub final (func)))
  (table (export "t") 1 (ref null $t1))
)
(register "tp" $TP)
(assert_unlinkable
  (module (type $t2 (sub final (func))) (table (import "tp" "t") 1 (ref null $t2)))
  "incompatible import type"
)
;; positive control
(module $TO (type $t1 (sub (func))) (table (export "t") 1 (ref null $t1)))
(register "to" $TO)
(module (type $t1 (sub (func))) (table (import "to" "t") 1 (ref null $t1)))

;; ── sub-rule 3: cross-module declared-supertype chain ────────────────────────
;; provider exports $f21 (whose super chain does NOT reach the importer's required $f11).
(module $SUP
  (rec (type $f01 (sub (func))) (type $f02 (sub $f01 (func))))
  (rec (type $f11 (sub (func))) (type $f12 (sub $f01 (func))))
  (rec (type $f21 (sub (func))) (type $f22 (sub $f11 (func))))
  (func (export "f") (type $f21))
)
(register "sup" $SUP)
(assert_unlinkable
  (module
    (rec (type $f01 (sub (func))) (type $f02 (sub $f01 (func))))
    (rec (type $f11 (sub (func))) (type $f12 (sub $f01 (func))))
    (func (import "sup" "f") (type $f11))
  )
  "incompatible import type"
)
