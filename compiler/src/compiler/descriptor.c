/* descriptor.c — JVM type and method descriptor parsing/generation */

#include "javelina/compiler/descriptor.h"
#include "javelina/compiler/jtype_meta.h"
#include "bbq_buf.h"
#include <string.h>

/* ── Parsing ──────────────────────────────────────────────── */

java_type_t desc_parse_type(bbq_arena* a, const char* desc, int* pos,
                         const sema_ctx_t* ctx) {
    if (!desc || !desc[*pos]) return jt_error();

    switch (desc[*pos]) {
    case 'B': (*pos)++; return jt_prim(JT_BYTE);
    case 'S': (*pos)++; return jt_prim(JT_SHORT);
    case 'I': (*pos)++; return jt_prim(JT_INT);
    case 'Z': (*pos)++; return jt_prim(JT_BOOL);
    case 'V': (*pos)++; return jt_prim(JT_VOID);
    case 'Q': (*pos)++; return jt_prim(JT_V128);   /* internal v128 (see jtype_desc_char) */

    case '[': {
        (*pos)++;
        java_type_t elem = desc_parse_type(a, desc, pos, ctx);
        if (jt_is_error(elem)) return jt_error();
        java_type_t* ep = (java_type_t*)bbq_arena_alloc(a, sizeof(java_type_t));
        *ep = elem;
        return jt_array(ep);
    }

    case 'L': {
        (*pos)++; /* skip 'L' */
        const char* start = &desc[*pos];
        while (desc[*pos] && desc[*pos] != ';') (*pos)++;
        if (desc[*pos] != ';') return jt_error();
        int len = (int)(&desc[*pos] - start);
        (*pos)++; /* skip ';' */

        /* Convert "java/lang/String" → "java.lang.String"
           for class lookup, or try the slash form too */
        char* name = (char*)bbq_arena_alloc(a, (size_t)(len + 1));
        for (int i = 0; i < len; i++)
            name[i] = (start[i] == '/') ? '.' : start[i];
        name[len] = '\0';

        /* Descriptors carry the FULLY QUALIFIED name (built from fq_name),
         * and the class table is FQN-keyed (§7.5.1) — one lookup, no
         * simple-name fallback (a lucky simple hit would mask a bad FQN). */
        int cid = sema_find_class(ctx, name);
        if (cid < 0) return jt_error();
        return jt_class(cid);
    }

    default:
        return jt_error();
    }
}

java_type_t desc_parse_method(bbq_arena* a, const char* desc,
                           java_type_t** out_params, int* out_param_count,
                           const sema_ctx_t* ctx) {
    *out_params = NULL;
    *out_param_count = 0;

    int pos = 0;
    if (desc[pos] != '(') return jt_error();
    pos++;

    /* Parse parameters — keep going on unresolved class refs so we
     * always get the correct param count (important for method
     * overload resolution when descriptors reference not-yet-loaded
     * classes, e.g. Applet.process(APDU) parsed before APDU exists). */
    java_type_t params[64]; /* max 64 params, more than enough */
    int pc = 0;
    while (desc[pos] && desc[pos] != ')' && pc < 64) {
        params[pc] = desc_parse_type(a, desc, &pos, ctx);
        if (jt_is_error(params[pc])) {
            /* Skip past the unresolved type so parsing can continue.
             * For primitives this is already handled in desc_parse_type.
             * For 'L...;' types, we need to skip to the ';'. */
        }
        pc++;
    }
    if (desc[pos] != ')') return jt_error();
    pos++;

    /* Copy params to arena */
    if (pc > 0) {
        *out_params = (java_type_t*)bbq_arena_alloc(a, (size_t)pc * sizeof(java_type_t));
        memcpy(*out_params, params, (size_t)pc * sizeof(java_type_t));
    }
    *out_param_count = pc;

    /* Parse return type */
    return desc_parse_type(a, desc, &pos, ctx);
}

/* ── Generation ───────────────────────────────────────────── */

static void append_type_desc(bbq_buf* buf, java_type_t type, const sema_ctx_t* ctx) {
    /* Single descriptor character (JVMS §4.3) for primitives + the
     * lead char for class / array. Trailing structure handled below
     * for the structural tags. */
    char lead = jtype_desc_char[type.tag];
    if (!lead) return;
    bbq_buf_append(buf, &lead, 1);
    if (type.tag == JT_ARRAY) {
        if (type.element) append_type_desc(buf, *type.element, ctx);
    } else if (type.tag == JT_CLASS) {
        const char* name = ctx->classes[type.class_id].name;
        /* Convert dots to slashes */
        for (const char* p = name; *p; p++) {
            char c = (*p == '.') ? '/' : *p;
            bbq_buf_append(buf, &c, 1);
        }
        bbq_buf_append(buf, ";", 1);
    }
}

const char* desc_from_type(bbq_arena* a, java_type_t type, const sema_ctx_t* ctx) {
    bbq_buf buf;
    bbq_buf_init(&buf);
    append_type_desc(&buf, type, ctx);
    /* Copy to arena and free buf */
    char* result = (char*)bbq_arena_alloc(a, buf.len + 1);
    memcpy(result, buf.data, buf.len);
    result[buf.len] = '\0';
    bbq_buf_free(&buf);
    return result;
}

const char* desc_from_method(bbq_arena* a, const java_type_t* params, int param_count,
                             java_type_t return_type, const sema_ctx_t* ctx) {
    bbq_buf buf;
    bbq_buf_init(&buf);
    bbq_buf_append(&buf, "(", 1);
    for (int i = 0; i < param_count; i++)
        append_type_desc(&buf, params[i], ctx);
    bbq_buf_append(&buf, ")", 1);
    append_type_desc(&buf, return_type, ctx);
    char* result = (char*)bbq_arena_alloc(a, buf.len + 1);
    memcpy(result, buf.data, buf.len);
    result[buf.len] = '\0';
    bbq_buf_free(&buf);
    return result;
}
