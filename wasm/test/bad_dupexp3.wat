;; §3.5.10 export names not distinct — the duplicate is hidden BEHIND a hash collision.
;; "ad" and "bC" are distinct names with the same djb2 hash: the accumulator takes each
;; byte as c*33^i, so +1 on one position and -33 on the next-lower one cancel exactly
;; ((5381*33+97)*33+100 == (5381*33+98)*33+67 == 5863210). Both therefore land in one
;; bucket, and the duplicate "ad" is reachable only PAST the colliding "bC".
;; A membership check that treats a bucket as the answer — or keeps only the newest name
;; in it — compares the third export against "bC", sees bytes that differ, and accepts an
;; invalid module. Only a bucket that keeps every name that hashed there rejects this.
(module (func) (func) (func)
  (export "ad" (func 0))
  (export "bC" (func 1))
  (export "ad" (func 2)))
