// jav_error.h — the fine failure reason behind a JAV_INVALID / JAV_MALFORMED verdict.
//
// jav_status_t gives the coarse §-category; jav_err_t gives the precise reason, and its
// vocabulary IS the official WebAssembly testsuite's expected error strings (the .wast
// assert_invalid / assert_malformed messages). jav_err_str is the SINGLE place that text
// lives — checks return the enum, never inline string literals.
//
// The W3C wasm-c-api carries no message for module decode/validate failures
// (wasm_module_new→NULL, wasm_module_validate→bool); only wasm_trap_t has a message. So a
// conformance runner reads this reason THROUGH the API: as a wasm_trap_t message (trap
// cases) or via a store-local last-error extension on the wasm.h shim (module cases) —
// never by reaching past the API into the validator.
#ifndef JAV_ERROR_H
#define JAV_ERROR_H

typedef enum {
    JAV_E_NONE = 0,
    JAV_E_TYPE_MISMATCH,                  // §7.6 body / init / offset / reftype typing
    JAV_E_CONST_EXPR_REQUIRED,            // §3.3.10 non-const op or mutable global.get in a const-expr
    JAV_E_DUPLICATE_EXPORT_NAME,          // §3.5.10 export names not distinct
    JAV_E_UNKNOWN_FUNCTION,               // funcidx out of range (export/start/elem/ref.func)
    JAV_E_UNKNOWN_GLOBAL,                 // globalidx out of range / out of const-expr scope
    JAV_E_UNKNOWN_TABLE,                  // tableidx out of range (export / active elem)
    JAV_E_UNKNOWN_MEMORY,                 // memidx out of range (export / active data)
    JAV_E_UNKNOWN_TYPE,                   // typeidx out of range
    JAV_E_UNKNOWN_TAG,                    // tagidx out of range (export)
    JAV_E_UNDECLARED_FUNCTION_REFERENCE,  // §3.5.10 ref.func of a func not in C.refs
    JAV_E_SIZE_MIN_GT_MAX,                // §3.2.12 limits min > max
    JAV_E_MEMORY_SIZE,                    // §3.2.15 memory limits exceed the page bound
    JAV_E_TABLE_SIZE,                     // §3.2.16 table limits exceed 2^|addrtype|
    JAV_E_START_FUNCTION,                 // §3.5.12 start function type not [] -> []
    JAV_E_OOB_MEMORY,                     // §4.5.5 active data segment out of memory bounds
    JAV_E_OOB_TABLE,                      // §4.5.7 active element segment out of table bounds
    JAV_E_INCOMPATIBLE_IMPORT,            // §4.5.2 a supplied import's type does not match the declared one
    JAV_E_UNKNOWN_IMPORT,                 // §4.5.2 import arity mismatch / a declared import has no supplied value
    // §7.6 body-typecheck reasons — threaded out of jav_typecheck so a rejection carries the
    // specific testsuite vocabulary instead of collapsing to "type mismatch".
    JAV_E_UNKNOWN_LOCAL,                  // localidx out of range
    JAV_E_UNKNOWN_LABEL,                  // labelidx out of range (br / br_if / br_table / br_on_*)
    JAV_E_UNKNOWN_DATA,                   // dataidx out of range (memory.init / array.new_data / data.drop)
    JAV_E_UNKNOWN_ELEM,                   // elemidx out of range (table.init / array.new_elem / elem.drop)
    JAV_E_ALIGNMENT,                      // §3.4.5 memarg alignment greater than the natural width
    JAV_E_INVALID_LANE,                   // §3.4.11 SIMD lane index out of range
    JAV_E_UNINITIALIZED_LOCAL,            // §3.4.2 read of a non-defaultable local before it is set
    JAV_E_IMMUTABLE_GLOBAL,               // §3.4.3 global.set on an immutable global
    JAV_E_IMMUTABLE_ARRAY,                // §3.4.8 array.set/fill/copy on an immutable array
    JAV_E_IMMUTABLE_FIELD,                // §3.4.7 struct.set on an immutable field
    JAV_E_SUB_TYPE,                       // §3.2.11 declared sub type does not match its supertype
    JAV_E_ARRAY_TYPES_MISMATCH,           // §3.4.8 array.copy source/dest element types incompatible
    JAV_E_OFFSET_OUT_OF_RANGE,            // §3.4.5 memarg offset ≥ 2^32 on a 32-bit memory
    JAV_E_ARRAY_NOT_NUMERIC,              // §3.4.8 array.new_data/init_data element is not a numeric/vector type
    JAV_E_INVALID_RESULT_ARITY,           // §3.4.2 typed select result type sequence is not length 1
    JAV_E_NONEMPTY_TAG_RESULT,            // §3.2.10 a tag's type must have an empty result sequence
} jav_err_t;

// The official testsuite text for a reason ("" for JAV_E_NONE). The one source of truth.
const char* jav_err_str(jav_err_t e);

#endif // JAV_ERROR_H
