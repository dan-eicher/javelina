// jav_error.c — jav_err_t → the official WebAssembly testsuite error string.
#include "jav_error.h"

const char* jav_err_str(jav_err_t e) {
    switch (e) {
    case JAV_E_NONE:                          return "";
    case JAV_E_TYPE_MISMATCH:                 return "type mismatch";
    case JAV_E_CONST_EXPR_REQUIRED:           return "constant expression required";
    case JAV_E_DUPLICATE_EXPORT_NAME:         return "duplicate export name";
    case JAV_E_UNKNOWN_FUNCTION:              return "unknown function";
    case JAV_E_UNKNOWN_GLOBAL:                return "unknown global";
    case JAV_E_UNKNOWN_TABLE:                 return "unknown table";
    case JAV_E_UNKNOWN_MEMORY:                return "unknown memory";
    case JAV_E_UNKNOWN_TYPE:                  return "unknown type";
    case JAV_E_UNKNOWN_TAG:                   return "unknown tag";
    case JAV_E_UNDECLARED_FUNCTION_REFERENCE: return "undeclared function reference";
    case JAV_E_SIZE_MIN_GT_MAX:               return "size minimum must not be greater than maximum";
    case JAV_E_MEMORY_SIZE:                   return "memory size must be at most 65536 pages (4GiB)";
    case JAV_E_TABLE_SIZE:                    return "table size must be at most 2^32";
    case JAV_E_START_FUNCTION:                return "start function";
    case JAV_E_OOB_MEMORY:                    return "out of bounds memory access";
    case JAV_E_OOB_TABLE:                     return "out of bounds table access";
    case JAV_E_INCOMPATIBLE_IMPORT:           return "incompatible import type";
    case JAV_E_UNKNOWN_IMPORT:                return "unknown import";
    case JAV_E_UNKNOWN_LOCAL:                 return "unknown local";
    case JAV_E_UNKNOWN_LABEL:                 return "unknown label";
    case JAV_E_UNKNOWN_DATA:                  return "unknown data segment";
    case JAV_E_UNKNOWN_ELEM:                  return "unknown elem segment";
    case JAV_E_ALIGNMENT:                     return "alignment must not be larger than natural";
    case JAV_E_INVALID_LANE:                  return "invalid lane index";
    case JAV_E_UNINITIALIZED_LOCAL:           return "uninitialized local";
    case JAV_E_IMMUTABLE_GLOBAL:              return "immutable global";
    case JAV_E_IMMUTABLE_ARRAY:               return "immutable array";
    case JAV_E_IMMUTABLE_FIELD:               return "immutable field";
    case JAV_E_SUB_TYPE:                      return "sub type";
    case JAV_E_ARRAY_TYPES_MISMATCH:          return "array types do not match";
    case JAV_E_OFFSET_OUT_OF_RANGE:           return "offset out of range";
    case JAV_E_ARRAY_NOT_NUMERIC:             return "array type is not numeric or vector";
    case JAV_E_INVALID_RESULT_ARITY:          return "invalid result arity";
    case JAV_E_NONEMPTY_TAG_RESULT:           return "non-empty tag result type";
    case JAV_E_SECTION_ORDER:                 return "unexpected content after last section";
    case JAV_E_FUNC_CODE_LENGTHS:             return "function and code section have inconsistent lengths";
    case JAV_E_DATA_COUNT_LENGTHS:            return "data count and data section have inconsistent lengths";
    case JAV_E_TOO_MANY_LOCALS:               return "too many locals";
    case JAV_E_DATA_COUNT_REQUIRED:           return "data count section required";
    }
    return "";
}
