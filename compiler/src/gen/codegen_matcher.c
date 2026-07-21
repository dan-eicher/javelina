#include "codegen_matcher.h"

void burg_ctx_init(burg_ctx_t* ctx) {
    bbq_arena_init(&ctx->arena, 4096);
    ctx->state_cache = bbq_htree_create();
    ctx->burg_error_msg = NULL;
    ctx->burg_error_arg = 0;
}

void burg_ctx_free(burg_ctx_t* ctx) {
    bbq_arena_free(&ctx->arena);
    bbq_htree_destroy(ctx->state_cache);
    ctx->state_cache = NULL;
}

bool burg_has_error(const burg_ctx_t* ctx) {
    return ctx->burg_error_msg != NULL;
}

const char* burg_get_error(const burg_ctx_t* ctx) {
    return ctx->burg_error_msg;
}

int burg_get_error_arg(const burg_ctx_t* ctx) {
    return ctx->burg_error_arg;
}

void burg_clear_error(burg_ctx_t* ctx) {
    ctx->burg_error_msg = NULL;
    ctx->burg_error_arg = 0;
}

void burg_set_error(const char* msg, int arg, burg_ctx_t* ctx) {
    if (ctx->burg_error_msg == NULL) {
        ctx->burg_error_msg = msg;
        ctx->burg_error_arg = arg;
    }
}


static BURG_UNUSED void* arena_alloc(size_t size, burg_ctx_t* ctx) {
    return bbq_arena_alloc(&ctx->arena, size);
}

static BURG_UNUSED void arena_reset(burg_ctx_t* ctx) {
    bbq_arena_reset(&ctx->arena);
}

static BURG_UNUSED burg_state_t* burg_cache_lookup(uint32_t id, burg_ctx_t* ctx) {
    return (burg_state_t*)bbq_htree_search(ctx->state_cache, id);
}

static BURG_UNUSED void burg_cache_store(uint32_t id, burg_state_t* state, burg_ctx_t* ctx) {
    bbq_htree_insert(ctx->state_cache, id, state);
}

static BURG_UNUSED void burg_cache_clear(burg_ctx_t* ctx) {
    bbq_htree_clear(ctx->state_cache);
}

static void closure_ref(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f64(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i64(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f32(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i32(burg_state_t* p, int c, BURG_NODE_TYPE node);

static void closure_ref(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 404 < p->cost[1]) {
        p->cost[1] = c + 404;
        p->rule[1] = 198;
    }
}

static void closure_f64(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 403 < p->cost[1]) {
        p->cost[1] = c + 403;
        p->rule[1] = 197;
    }
}

static void closure_i64(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 401 < p->cost[1]) {
        p->cost[1] = c + 401;
        p->rule[1] = 195;
    }
}

static void closure_f32(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 402 < p->cost[1]) {
        p->cost[1] = c + 402;
        p->rule[1] = 196;
    }
}

static void closure_i32(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 400 < p->cost[1]) {
        p->cost[1] = c + 400;
        p->rule[1] = 194;
    }
}


static void burg_dp(burg_state_t* p, BURG_NODE_TYPE node, burg_ctx_t* ctx) {
    (void)ctx;
    int op = p->op;
    switch (op) {
    case BURG_ExceptionEntry:
        {
            int c = 0 + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 192;
            }
        }
        break;
    case BURG_CheckCast:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 191;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_InstanceOf:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 190;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_InvokeInterface:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (DT_IS_I32(node->invoke_interface.return_type))) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 181;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 182;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 183;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 184;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 185;
                closure_ref(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 189;
            }
        }
        break;
    case BURG_InvokeVirtual:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (DT_IS_I32(node->invoke_virtual.return_type))) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 176;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 177;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 178;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 179;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 180;
                closure_ref(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 188;
            }
        }
        break;
    case BURG_InvokeSpecial:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (DT_IS_I32(node->invoke_special.return_type))) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 171;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 172;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 173;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 174;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 175;
                closure_ref(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 187;
            }
        }
        break;
    case BURG_MemLoad8:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 161;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_ArrayLength:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 160;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_ArrayStore:
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 156;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 157;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[4]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 158;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[5]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 159;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 164;
            }
        }
        break;
    case BURG_Nop:
        {
            int c = 0 + 0;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 193;
            }
        }
        break;
    case BURG_NewArray:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 149;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_MemStore8:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 148;
            }
        }
        break;
    case BURG_PutStatic:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 143;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 144;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 145;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 146;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 147;
            }
        }
        break;
    case BURG_ClassConstruct:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 123;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_PutField:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 132;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 133;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 134;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 135;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 136;
            }
        }
        break;
    case BURG_Lt:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 50;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 56;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 62;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 68;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Shl:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 21;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 34;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_LoadConst:
        {
            int c = 0 + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 1;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Ge:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 53;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 59;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 65;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 71;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_I2L:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 74;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_Eq:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 48;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 54;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 60;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 66;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 72;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_D2I:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 83;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MoveF2I:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 86;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Xor:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 20;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 33;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_D2L:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 85;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_Return:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 99;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 100;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 101;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 102;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 103;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 0;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 105;
            }
        }
        break;
    case BURG_LoadFloatConst:
        {
            int c = 0 + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 3;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_L2F:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 78;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_NewRefArray:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 162;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_GetField:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTINT)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 125;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTBYTE || node->get_field.data_type == SIR_DTSHORT)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 126;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTCHAR)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 127;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 128;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 129;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 130;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 131;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_LoadDoubleConst:
        {
            int c = 0 + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 4;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_ArrayLoad:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTINT)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 150;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTBYTE || node->array_load.data_type == SIR_DTSHORT)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 151;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTCHAR)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 152;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 153;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 154;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 155;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 163;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_LoadNull:
        {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 11;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_Add:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 13;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 26;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 38;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 43;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_ClassInstantiable:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 122;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Ne:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 49;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 55;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 61;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 67;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 2;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 73;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MoveL2D:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 89;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_Ushr:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 23;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 36;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_ArrayCopy:
        if (p->child_count >= 5 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[6] && p->children[3]->rule[2] && p->children[4]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[6] + p->children[3]->cost[2] + p->children[4]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 165;
            }
        }
        break;
    case BURG_LoadThis:
        {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 10;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_ReturnVoid:
        {
            int c = 0 + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 104;
            }
        }
        break;
    case BURG_Mul:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 15;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 28;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 40;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 45;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_Le:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 51;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 57;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 63;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 69;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_I2F:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 75;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_Shr:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 22;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 35;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_I2D:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 76;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_F2L:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 84;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_Rem:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 17;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 30;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_Div:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 16;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 29;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 41;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 46;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_Neg:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 24;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 37;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 42;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 47;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_LoadLongConst:
        {
            int c = 0 + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 2;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_GetStatic:
        if ((DT_IS_I32(node->get_static.data_type))) {
            int c = 0 + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 138;
                closure_i32(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTLONG)) {
            int c = 0 + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 139;
                closure_i64(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTFLOAT)) {
            int c = 0 + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 140;
                closure_f32(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTDOUBLE)) {
            int c = 0 + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 141;
                closure_f64(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTREF)) {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 142;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_SetHeader:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 137;
            }
        }
        break;
    case BURG_LoadClass:
        {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 12;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_I2S:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 95;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_F64Nearest:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 93;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_MoveI2F:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 87;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_LogNot:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 25;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_D2F:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 81;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_L2I:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 77;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_F64Ceil:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 92;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_Or:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 19;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 32;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_L2D:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 79;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_I2B:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 94;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_CloneCopy:
        {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 124;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_F2D:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 80;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_Gt:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 52;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 58;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 64;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 70;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_F2I:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 82;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MoveD2L:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 88;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_New:
        {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 121;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_F64Sqrt:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 90;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_F64Floor:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 91;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_StoreLocal:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 107;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 108;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 109;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 110;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 111;
            }
        }
        break;
    case BURG_I2C:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 96;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_S2B:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 97;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_S2I:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 0;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 98;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_InvokeStatic:
        if ((DT_IS_I32(node->invoke_static.return_type))) {
            int c = 0 + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 166;
                closure_i32(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTLONG)) {
            int c = 0 + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 167;
                closure_i64(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTFLOAT)) {
            int c = 0 + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 168;
                closure_f32(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTDOUBLE)) {
            int c = 0 + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 169;
                closure_f64(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTREF)) {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 170;
                closure_ref(p, c, node);
            }
        }
        {
            int c = 0 + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 186;
            }
        }
        break;
    case BURG_Throw:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 106;
            }
        }
        break;
    case BURG_Sub:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 14;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 27;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 39;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 44;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_And:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 18;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 31;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_Inc:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 112;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 113;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 114;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 115;
            }
        }
        break;
    case BURG_LoadLocal:
        if ((DT_IS_I32(ll_dt(node)))) {
            int c = 0 + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 5;
                closure_i32(p, c, node);
            }
        }
        if ((ll_dt(node) == SIR_DTLONG)) {
            int c = 0 + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 6;
                closure_i64(p, c, node);
            }
        }
        if ((ll_dt(node) == SIR_DTFLOAT)) {
            int c = 0 + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 7;
                closure_f32(p, c, node);
            }
        }
        if ((ll_dt(node) == SIR_DTDOUBLE)) {
            int c = 0 + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 8;
                closure_f64(p, c, node);
            }
        }
        if ((ll_dt(node) == SIR_DTREF)) {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 9;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_ExprEffect:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 116;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 117;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 118;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 119;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 120;
            }
        }
        break;
    default:
        burg_set_error("burg: unknown opcode in match", op, ctx);
        break;
    }
}

static burg_state_t* burg_label_tree(BURG_NODE_TYPE node, burg_ctx_t* ctx) {
    int arity = BURG_NODE_ARITY(node);
    burg_state_t* p = (burg_state_t*)arena_alloc(sizeof(burg_state_t), ctx);
    p->op = BURG_NODE_OP(node);
    p->child_count = arity;
    p->children = arity > 0
        ? (burg_state_t**)arena_alloc(arity * sizeof(burg_state_t*), ctx)
        : NULL;
    for (int i = 0; i <= BURG_MAX_NT; i++) {
        p->cost[i] = BURG_MAX_COST;
        p->rule[i] = 0;
    }

    for (int i = 0; i < arity; i++)
        p->children[i] = burg_label_tree(BURG_NODE_CHILD(node, i), ctx);

    burg_dp(p, node, ctx);
    return p;
}

static burg_state_t* burg_label(BURG_NODE_TYPE node, burg_ctx_t* ctx) {
    uint32_t id = (uint32_t)(uintptr_t)BURG_NODE_ID(node);
    burg_state_t* cached = burg_cache_lookup(id, ctx);
    if (cached) return cached;

    int arity = BURG_NODE_ARITY(node);
    burg_state_t* p = (burg_state_t*)arena_alloc(sizeof(burg_state_t), ctx);
    p->op = BURG_NODE_OP(node);
    p->child_count = arity;
    p->children = arity > 0
        ? (burg_state_t**)arena_alloc(arity * sizeof(burg_state_t*), ctx)
        : NULL;
    for (int i = 0; i <= BURG_MAX_NT; i++) {
        p->cost[i] = BURG_MAX_COST;
        p->rule[i] = 0;
    }

    /* Cache BEFORE DP (back-edge cut-point safety) */
    burg_cache_store(id, p, ctx);

    for (int i = 0; i < arity; i++)
        p->children[i] = burg_label(BURG_NODE_CHILD(node, i), ctx);

    burg_dp(p, node, ctx);
    return p;
}

int burg_rule(burg_state_t* state, int goalnt) {
    if (!state || goalnt < 1 || goalnt > BURG_MAX_NT) return 0;
    return state->rule[goalnt];
}

const char* burg_nt_name(int nt) {
    static const char* names[] = {
        "<invalid>",
        "stmt",
        "i32",
        "i64",
        "f32",
        "f64",
        "ref",
        "tail",
    };
    if (nt >= 1 && nt <= BURG_MAX_NT) return names[nt];
    return names[0];
}

void burg_reduce(BURG_NODE_TYPE node, burg_state_t* state, int goalnt, burg_ctx_t* ctx) {
    if (burg_has_error(ctx)) return;
    int rule = burg_rule(state, goalnt);
    switch (rule) {
    case 1: { // i32: LoadConst
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, const_val(node));
        break;
    }
    case 2: { // i64: LoadLongConst
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, node->load_long_const.value);
        break;
    }
    case 3: { // f32: LoadFloatConst
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_CONST); ew_f32(E, node->load_float_const.value);
        break;
    }
    case 4: { // f64: LoadDoubleConst
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CONST); ew_f64(E, node->load_double_const.value);
        break;
    }
    case 5: { // i32: LoadLocal
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_GET); ew_u32(E, local_slot(node));
        break;
    }
    case 6: { // i64: LoadLocal
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_GET); ew_u32(E, local_slot(node));
        break;
    }
    case 7: { // f32: LoadLocal
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_GET); ew_u32(E, local_slot(node));
        break;
    }
    case 8: { // f64: LoadLocal
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_GET); ew_u32(E, local_slot(node));
        break;
    }
    case 9: { // ref: LoadLocal
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_GET); ew_u32(E, local_slot(node));
        break;
    }
    case 10: { // ref: LoadThis
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_GET); ew_u32(E, 0); ew_emit(E, WOP_REF_CAST); ew_i32(E, struct_idx(node->load_this.class_id));
        break;
    }
    case 11: { // ref: LoadNull
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_NULL); ew_i64(E, -15);
        break;
    }
    case 12: { // ref: LoadClass
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, wasm_class_singleton_global_index(ctx->types, node->load_class.class_id));
        break;
    }
    case 13: { // i32: Add(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_ADD);
        break;
    }
    case 14: { // i32: Sub(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_SUB);
        break;
    }
    case 15: { // i32: Mul(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_MUL);
        break;
    }
    case 16: { // i32: Div(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_DIV_S);
        break;
    }
    case 17: { // i32: Rem(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_REM_S);
        break;
    }
    case 18: { // i32: And(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_AND);
        break;
    }
    case 19: { // i32: Or(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_OR);
        break;
    }
    case 20: { // i32: Xor(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_XOR);
        break;
    }
    case 21: { // i32: Shl(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_SHL);
        break;
    }
    case 22: { // i32: Shr(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_SHR_S);
        break;
    }
    case 23: { // i32: Ushr(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_SHR_U);
        break;
    }
    case 24: { // i32: Neg(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, -1); ew_emit(E, WOP_I32_MUL);
        break;
    }
    case 25: { // i32: LogNot(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 26: { // i64: Add(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_ADD);
        break;
    }
    case 27: { // i64: Sub(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_SUB);
        break;
    }
    case 28: { // i64: Mul(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_MUL);
        break;
    }
    case 29: { // i64: Div(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_DIV_S);
        break;
    }
    case 30: { // i64: Rem(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_REM_S);
        break;
    }
    case 31: { // i64: And(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_AND);
        break;
    }
    case 32: { // i64: Or(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_OR);
        break;
    }
    case 33: { // i64: Xor(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_XOR);
        break;
    }
    case 34: { // i64: Shl(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_SHL);
        break;
    }
    case 35: { // i64: Shr(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_SHR_S);
        break;
    }
    case 36: { // i64: Ushr(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_SHR_U);
        break;
    }
    case 37: { // i64: Neg(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, -1); ew_emit(E, WOP_I64_MUL);
        break;
    }
    case 38: { // f32: Add(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_ADD);
        break;
    }
    case 39: { // f32: Sub(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_SUB);
        break;
    }
    case 40: { // f32: Mul(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_MUL);
        break;
    }
    case 41: { // f32: Div(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_DIV);
        break;
    }
    case 42: { // f32: Neg(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_NEG);
        break;
    }
    case 43: { // f64: Add(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_ADD);
        break;
    }
    case 44: { // f64: Sub(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_SUB);
        break;
    }
    case 45: { // f64: Mul(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_MUL);
        break;
    }
    case 46: { // f64: Div(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_DIV);
        break;
    }
    case 47: { // f64: Neg(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_NEG);
        break;
    }
    case 48: { // i32: Eq(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQ);
        break;
    }
    case 49: { // i32: Ne(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_NE);
        break;
    }
    case 50: { // i32: Lt(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_LT_S);
        break;
    }
    case 51: { // i32: Le(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_LE_S);
        break;
    }
    case 52: { // i32: Gt(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_GT_S);
        break;
    }
    case 53: { // i32: Ge(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_GE_S);
        break;
    }
    case 54: { // i32: Eq(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQ);
        break;
    }
    case 55: { // i32: Ne(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_NE);
        break;
    }
    case 56: { // i32: Lt(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_LT_S);
        break;
    }
    case 57: { // i32: Le(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_LE_S);
        break;
    }
    case 58: { // i32: Gt(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_GT_S);
        break;
    }
    case 59: { // i32: Ge(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_GE_S);
        break;
    }
    case 60: { // i32: Eq(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_EQ);
        break;
    }
    case 61: { // i32: Ne(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_NE);
        break;
    }
    case 62: { // i32: Lt(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_LT);
        break;
    }
    case 63: { // i32: Le(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_LE);
        break;
    }
    case 64: { // i32: Gt(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_GT);
        break;
    }
    case 65: { // i32: Ge(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_GE);
        break;
    }
    case 66: { // i32: Eq(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_EQ);
        break;
    }
    case 67: { // i32: Ne(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_NE);
        break;
    }
    case 68: { // i32: Lt(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_LT);
        break;
    }
    case 69: { // i32: Le(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_LE);
        break;
    }
    case 70: { // i32: Gt(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_GT);
        break;
    }
    case 71: { // i32: Ge(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_GE);
        break;
    }
    case 72: { // i32: Eq(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_EQ);
        break;
    }
    case 73: { // i32: Ne(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_EQ); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 74: { // i64: I2L(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EXTEND_I32_S);
        break;
    }
    case 75: { // f32: I2F(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_CONVERT_I32_S);
        break;
    }
    case 76: { // f64: I2D(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CONVERT_I32_S);
        break;
    }
    case 77: { // i32: L2I(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_WRAP_I64);
        break;
    }
    case 78: { // f32: L2F(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_CONVERT_I64_S);
        break;
    }
    case 79: { // f64: L2D(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CONVERT_I64_S);
        break;
    }
    case 80: { // f64: F2D(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_PROMOTE_F32);
        break;
    }
    case 81: { // f32: D2F(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_DEMOTE_F64);
        break;
    }
    case 82: { // i32: F2I(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_TRUNC_SAT_F32_S);
        break;
    }
    case 83: { // i32: D2I(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_TRUNC_SAT_F64_S);
        break;
    }
    case 84: { // i64: F2L(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_TRUNC_SAT_F32_S);
        break;
    }
    case 85: { // i64: D2L(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_TRUNC_SAT_F64_S);
        break;
    }
    case 86: { // i32: MoveF2I(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_REINTERPRET_F32);
        break;
    }
    case 87: { // f32: MoveI2F(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_REINTERPRET_I32);
        break;
    }
    case 88: { // i64: MoveD2L(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_REINTERPRET_F64);
        break;
    }
    case 89: { // f64: MoveL2D(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_REINTERPRET_I64);
        break;
    }
    case 90: { // f64: F64Sqrt(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_SQRT);
        break;
    }
    case 91: { // f64: F64Floor(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_FLOOR);
        break;
    }
    case 92: { // f64: F64Ceil(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CEIL);
        break;
    }
    case 93: { // f64: F64Nearest(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_NEAREST);
        break;
    }
    case 94: { // i32: I2B(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EXTEND8_S);
        break;
    }
    case 95: { // i32: I2S(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EXTEND16_S);
        break;
    }
    case 96: { // i32: I2C(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, 0xFFFF); ew_emit(E, WOP_I32_AND);
        break;
    }
    case 97: { // i32: S2B(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EXTEND8_S);
        break;
    }
    case 98: { // i32: S2I(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        /* short→int: both i32, no-op (the operand already an i32) */
        break;
    }
    case 99: { // stmt: Return(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 100: { // stmt: Return(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 101: { // stmt: Return(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 102: { // stmt: Return(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 103: { // stmt: Return(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 104: { // stmt: ReturnVoid
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 105: { // stmt: Return(tail)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        /* the tail invoke already emitted return_call* and returned */
        break;
    }
    case 106: { // stmt: Throw(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_THROW); ew_u32(E, 0);
        break;
    }
    case 107: { // stmt: StoreLocal(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 108: { // stmt: StoreLocal(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 109: { // stmt: StoreLocal(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 110: { // stmt: StoreLocal(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 111: { // stmt: StoreLocal(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 112: { // stmt: Inc(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, node->inc.delta); ew_emit(E, WOP_I32_ADD); CG_NARROW(E, node->inc.data_type); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 113: { // stmt: Inc(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, (int64_t)node->inc.delta); ew_emit(E, WOP_I64_ADD); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 114: { // stmt: Inc(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_CONST); ew_f32(E, (float)node->inc.delta); ew_emit(E, WOP_F32_ADD); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 115: { // stmt: Inc(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CONST); ew_f64(E, (double)node->inc.delta); ew_emit(E, WOP_F64_ADD); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 116: { // stmt: ExprEffect(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        if (!node->expr_effect.is_void) ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 117: { // stmt: ExprEffect(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        if (!node->expr_effect.is_void) ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 118: { // stmt: ExprEffect(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        if (!node->expr_effect.is_void) ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 119: { // stmt: ExprEffect(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        if (!node->expr_effect.is_void) ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 120: { // stmt: ExprEffect(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        if (!node->expr_effect.is_void) ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 121: { // ref: New
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wasm_types_emit_new(ctx->types, E, node->new_.class_id);
        break;
    }
    case 122: { // i32: ClassInstantiable(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET);  ew_u32(E, (uint32_t)wasm_class_reflect_typeidx(ctx->types));
                                 ew_u32(E, (uint32_t)wasm_class_factory_field_index(ctx->types));
    ew_emit(E, WOP_REF_IS_NULL);
    ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 123: { // ref: ClassConstruct(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET);  ew_u32(E, (uint32_t)wasm_class_reflect_typeidx(ctx->types));
                                 ew_u32(E, (uint32_t)wasm_class_factory_field_index(ctx->types));
    ew_emit(E, WOP_REF_CAST);    ew_i32(E, (int32_t)wasm_factory_functype_idx(ctx->types));
    ew_emit(E, WOP_CALL_REF);    ew_u32(E, (uint32_t)wasm_factory_functype_idx(ctx->types));
        break;
    }
    case 124: { // ref: CloneCopy
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wasm_types_emit_clone_copy(ctx->types, E, node->clone_copy.class_id);
        break;
    }
    case 125: { // i32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET);   ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 126: { // i32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET_S); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 127: { // i32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET_U); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 128: { // i64: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 129: { // f32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 130: { // f64: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 131: { // ref: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 132: { // stmt: PutField(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 133: { // stmt: PutField(ref, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 134: { // stmt: PutField(ref, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 135: { // stmt: PutField(ref, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 136: { // stmt: PutField(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 137: { // stmt: SetHeader(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->set_header.struct_class_id)); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 138: { // i32: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 139: { // i64: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 140: { // f32: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 141: { // f64: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 142: { // ref: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 143: { // stmt: PutStatic(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 144: { // stmt: PutStatic(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 145: { // stmt: PutStatic(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 146: { // stmt: PutStatic(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 147: { // stmt: PutStatic(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 148: { // stmt: MemStore8(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_STORE8); ew_u32(E, 0); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 149: { // ref: NewArray(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_NEW_DEFAULT); ew_u32(E, wasm_types_array_for_atype(ctx->types, node->new_array.elem_type));
        break;
    }
    case 150: { // i32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET);   ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 151: { // i32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET_S); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 152: { // i32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET_U); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 153: { // i64: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 154: { // f32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 155: { // f64: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 156: { // stmt: ArrayStore(ref, i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 157: { // stmt: ArrayStore(ref, i32, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 158: { // stmt: ArrayStore(ref, i32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 4, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 159: { // stmt: ArrayStore(ref, i32, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 5, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 160: { // i32: ArrayLength(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_LEN);
        break;
    }
    case 161: { // i32: MemLoad8(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_LOAD8_U); ew_u32(E, 0); ew_u32(E, 0);
        break;
    }
    case 162: { // ref: NewRefArray(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_NEW_DEFAULT); ew_u32(E, wasm_types_array_for_dt(ctx->types, SIR_DTREF));
        break;
    }
    case 163: { // ref: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, SIR_DTREF)); ew_emit(E, WOP_REF_CAST_NULL); ew_i32(E, wasm_types_ref_typeidx(ctx->types, node->array_load.elem_ref));
        break;
    }
    case 164: { // stmt: ArrayStore(ref, i32, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, SIR_DTREF)); cg_jump(E, node);
        break;
    }
    case 165: { // stmt: ArrayCopy(ref, i32, ref, i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 4), state->children[4], 2, ctx);
        for (int _ci = 5; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_COPY); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_copy.width)); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_copy.width)); cg_jump(E, node);
        break;
    }
    case 166: { // i32: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 167: { // i64: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 168: { // f32: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 169: { // f64: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 170: { // ref: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 171: { // i32: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 172: { // i64: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 173: { // f32: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 174: { // f64: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 175: { // ref: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 176: { // i32: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 177: { // i64: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 178: { // f32: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 179: { // f64: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 180: { // ref: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 181: { // i32: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 182: { // i64: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 183: { // f32: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 184: { // f64: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 185: { // ref: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 186: { // tail: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 187: { // tail: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 188: { // tail: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        TVCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 189: { // tail: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        TIVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 190: { // i32: InstanceOf(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        int _iid = node->instance_of.class_id;
    if (wasm_class_is_interface(ctx->types, _iid)) {
        ew_emit(E, WOP_I32_CONST); ew_i32(E, _iid);
        ew_emit(E, WOP_CALL);      ew_u32(E, (uint32_t)wasm_iface_helper_funcidx(ctx->types));
    } else {
        ew_emit(E, WOP_REF_TEST);  ew_i32(E, struct_idx(_iid));
    }
        break;
    }
    case 191: { // ref: CheckCast(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        int _cc = node->check_cast.class_id;
    int _to = wasm_class_is_interface(ctx->types, _cc) ? wasm_root_class(ctx->types) : _cc;
    ew_emit(E, WOP_REF_CAST_NULL); ew_i32(E, struct_idx(_to));
        break;
    }
    case 192: { // stmt: ExceptionEntry
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->exception_entry.local_slot);
    cg_jump(E, node);
        break;
    }
    case 193: { // stmt: Nop
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        /* label anchor — structurer owns the scope (S5.10) */
        break;
    }
    case 194: { // stmt: i32
        burg_reduce(node, state, 2, ctx);
        break;
    }
    case 195: { // stmt: i64
        burg_reduce(node, state, 3, ctx);
        break;
    }
    case 196: { // stmt: f32
        burg_reduce(node, state, 4, ctx);
        break;
    }
    case 197: { // stmt: f64
        burg_reduce(node, state, 5, ctx);
        break;
    }
    case 198: { // stmt: ref
        burg_reduce(node, state, 6, ctx);
        break;
    }
    default:
        burg_set_error("burg: no rule for goal nonterminal", goalnt, ctx);
        break;
    }
}

void burg_rewrite(BURG_NODE_TYPE root, burg_ctx_t* ctx) {
    burg_clear_error(ctx);
    arena_reset(ctx);

    if (BURG_NODE_SUCC_COUNT(root) == 0) {
        burg_state_t* state = burg_label_tree(root, ctx);
        if (state->rule[1])
            burg_reduce(root, state, 1, ctx);
        else
            burg_set_error("burg: start nonterminal has no rule at root", (int)BURG_NODE_OP(root), ctx);
        return;
    }

    burg_cache_clear(ctx);

    /* Compute reverse postorder via iterative DFS */
    BURG_NODE_TYPE* rpo = NULL;
    {
        bbq_htree* visited = bbq_htree_create();
        typedef struct { BURG_NODE_TYPE node; int succ; } Frame;
        Frame* stack = NULL;
        Frame f0;
        f0.node = root; f0.succ = 0;
        bbq_vec_push(stack, f0);
        bbq_htree_insert(visited, (uint32_t)(uintptr_t)BURG_NODE_ID(root), (void*)(uintptr_t)1);

        while (bbq_vec_len(stack) > 0) {
            Frame* f = &stack[bbq_vec_len(stack) - 1];
            int sc = BURG_NODE_SUCC_COUNT(f->node);
            if (f->succ < sc) {
                BURG_NODE_TYPE s = BURG_NODE_SUCC(f->node, f->succ);
                f->succ++;
                if (s && !bbq_htree_contains(visited, (uint32_t)(uintptr_t)BURG_NODE_ID(s))) {
                    bbq_htree_insert(visited, (uint32_t)(uintptr_t)BURG_NODE_ID(s), (void*)(uintptr_t)1);
                    Frame fn;
                    fn.node = s; fn.succ = 0;
                    bbq_vec_push(stack, fn);
                }
            } else {
                bbq_vec_push(rpo, f->node);
                bbq__vec_hdr(stack)->len--;
            }
        }
        bbq_vec_reverse(rpo);
        bbq_vec_free(stack);
        bbq_htree_destroy(visited);
    }

    /* Label every node */
    {
        int _i, _n = bbq_vec_len(rpo);
        for (_i = 0; _i < _n; _i++)
            burg_label(rpo[_i], ctx);
    }

    /* Reduce every node with the start nonterminal */
    {
        int _i, _n = bbq_vec_len(rpo);
        for (_i = 0; _i < _n; _i++) {
            burg_state_t* s = burg_cache_lookup((uint32_t)(uintptr_t)BURG_NODE_ID(rpo[_i]), ctx);
            if (s && s->rule[1])
                burg_reduce(rpo[_i], s, 1, ctx);
            else
                burg_set_error("burg: start nonterminal does not cover graph node", (int)BURG_NODE_OP(rpo[_i]), ctx);
        }
    }
    bbq_vec_free(rpo);
}
