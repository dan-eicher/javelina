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

static void closure_v128(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_ref(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f64(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i64(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f32(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i32(burg_state_t* p, int c, BURG_NODE_TYPE node);

static void closure_v128(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 405 < p->cost[1]) {
        p->cost[1] = c + 405;
        p->rule[1] = 247;
    }
}

static void closure_ref(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 404 < p->cost[1]) {
        p->cost[1] = c + 404;
        p->rule[1] = 246;
    }
}

static void closure_f64(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 403 < p->cost[1]) {
        p->cost[1] = c + 403;
        p->rule[1] = 245;
    }
}

static void closure_i64(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 401 < p->cost[1]) {
        p->cost[1] = c + 401;
        p->rule[1] = 243;
    }
}

static void closure_f32(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 402 < p->cost[1]) {
        p->cost[1] = c + 402;
        p->rule[1] = 244;
    }
}

static void closure_i32(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 400 < p->cost[1]) {
        p->cost[1] = c + 400;
        p->rule[1] = 242;
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
                p->rule[1] = 240;
            }
        }
        break;
    case BURG_CheckCast:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 239;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_InvokeInterface:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (DT_IS_I32(node->invoke_interface.return_type))) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 228;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 229;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 230;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 231;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 232;
                closure_ref(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTV128)) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 233;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 6;
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 237;
            }
        }
        break;
    case BURG_InvokeVirtual:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (DT_IS_I32(node->invoke_virtual.return_type))) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 222;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 223;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 224;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 225;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 226;
                closure_ref(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTV128)) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 227;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 5;
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 236;
            }
        }
        break;
    case BURG_InvokeSpecial:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (DT_IS_I32(node->invoke_special.return_type))) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 216;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 217;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 218;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 219;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 220;
                closure_ref(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTV128)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 221;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 235;
            }
        }
        break;
    case BURG_ArrayLength:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 205;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_ArrayStore:
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 200;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 201;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[4]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 202;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[5]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 203;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[7]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[7] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 204;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 208;
            }
        }
        break;
    case BURG_Nop:
        {
            int c = 0 + 0;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 241;
            }
        }
        break;
    case BURG_NewArray:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 191;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_MemStoreF:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 187;
            }
        }
        break;
    case BURG_SimdMemStoreLane:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 184;
            }
        }
        break;
    case BURG_ClassConstruct:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 154;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_MemStoreD:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 188;
            }
        }
        break;
    case BURG_New:
        {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 152;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_MemLoadD:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 139;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_InstanceOf:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 238;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MemLoadF:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 138;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_SimdMemLoadLane:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 135;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SimdMemLoad:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 134;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_PutStatic:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 177;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 178;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 179;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 180;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 181;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 182;
            }
        }
        break;
    case BURG_SimdConst:
        {
            int c = 0 + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 132;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SimdReplaceF:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[4] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 130;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_MemFill:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 189;
            }
        }
        break;
    case BURG_SimdExtractF:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 126;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_SimdMemStore:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 183;
            }
        }
        break;
    case BURG_MemGrow:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 141;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_SimdExtractL:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 125;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_SimdExtractI:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 124;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_SimdSplatL:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 121;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SimdSplatI:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 120;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SimdTestI:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 119;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_SimdTern:
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[7] && p->children[2]->rule[7]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + p->children[2]->cost[7] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 118;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_PutField:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 164;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 165;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 166;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 167;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 168;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[7] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 169;
            }
        }
        break;
    case BURG_Lt:
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
    case BURG_Shl:
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
    case BURG_SimdReplaceL:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 129;
                closure_v128(p, c, node);
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
                p->rule[2] = 54;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 60;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 66;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 72;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_I2L:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 75;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_MemStoreI:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 185;
            }
        }
        break;
    case BURG_Eq:
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
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 73;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_D2I:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 84;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MoveF2I:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 87;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Xor:
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
    case BURG_D2L:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 86;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_Return:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 100;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 101;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 102;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 103;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 104;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 105;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 107;
            }
        }
        break;
    case BURG_SimdShift:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 117;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_MemCopy:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 190;
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
    case BURG_SimdSplatD:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 123;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_L2F:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 79;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_NewRefArray:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 206;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_GetField:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTINT)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 156;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTBYTE || node->get_field.data_type == SIR_DTSHORT)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 157;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTCHAR)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 158;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 159;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 160;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 161;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 162;
                closure_ref(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTV128)) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 163;
                closure_v128(p, c, node);
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
                p->rule[2] = 193;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTBYTE || node->array_load.data_type == SIR_DTSHORT)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 194;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTCHAR)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 195;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 196;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 197;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 198;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTV128)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 199;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 207;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_SimdSplatF:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 122;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_LoadNull:
        {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 12;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_Add:
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
    case BURG_ClassInstantiable:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 153;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Ne:
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
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 2;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 74;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MoveL2D:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 90;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_ExprEffect:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 146;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 147;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 148;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 149;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 150;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 151;
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
        if ((ll_dt(node) == SIR_DTV128)) {
            int c = 0 + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 10;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SimdReplaceI:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 128;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_Ushr:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 24;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 37;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_ArrayCopy:
        if (p->child_count >= 5 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[6] && p->children[3]->rule[2] && p->children[4]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[6] + p->children[3]->cost[2] + p->children[4]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 209;
            }
        }
        break;
    case BURG_LoadThis:
        {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 11;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_ReturnVoid:
        {
            int c = 0 + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 106;
            }
        }
        break;
    case BURG_Mul:
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
    case BURG_Le:
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
    case BURG_MemLoadL:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 137;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_I2F:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 76;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_Shr:
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
    case BURG_I2D:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 77;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_F2L:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 85;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_Rem:
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
    case BURG_Div:
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
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 42;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 47;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_Neg:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 25;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 38;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 43;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 48;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_SimdReplaceD:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[5] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 131;
                closure_v128(p, c, node);
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
                p->rule[2] = 171;
                closure_i32(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTLONG)) {
            int c = 0 + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 172;
                closure_i64(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTFLOAT)) {
            int c = 0 + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 173;
                closure_f32(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTDOUBLE)) {
            int c = 0 + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 174;
                closure_f64(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTREF)) {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 175;
                closure_ref(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTV128)) {
            int c = 0 + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 176;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SetHeader:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 170;
            }
        }
        break;
    case BURG_LoadClass:
        {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 13;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_I2S:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 96;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_ArrayNewData:
        {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 192;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_MemSize:
        {
            int c = 0 + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 140;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_F64Nearest:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 94;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_SimdBin:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 115;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_LogNot:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 26;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Inc:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 142;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 143;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 144;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 145;
            }
        }
        break;
    case BURG_Sub:
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
    case BURG_And:
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
    case BURG_D2F:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 82;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_SimdShuffle:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 133;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_L2I:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 78;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MemStoreL:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 186;
            }
        }
        break;
    case BURG_F64Ceil:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 93;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_MemLoadI:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 136;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Or:
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
    case BURG_L2D:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 80;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_I2B:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 95;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_CloneCopy:
        {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 155;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_F2D:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 81;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_Gt:
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
    case BURG_F2I:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 83;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MoveD2L:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 89;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_F64Sqrt:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 91;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_F64Floor:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 92;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_StoreLocal:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 109;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 110;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 111;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 112;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 113;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 114;
            }
        }
        break;
    case BURG_I2C:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 97;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_S2B:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 98;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_S2I:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 0;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 99;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_InvokeStatic:
        if ((DT_IS_I32(node->invoke_static.return_type))) {
            int c = 0 + 1;
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 210;
                closure_i32(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTLONG)) {
            int c = 0 + 1;
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 211;
                closure_i64(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTFLOAT)) {
            int c = 0 + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 212;
                closure_f32(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTDOUBLE)) {
            int c = 0 + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 213;
                closure_f64(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTREF)) {
            int c = 0 + 1;
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 214;
                closure_ref(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTV128)) {
            int c = 0 + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 215;
                closure_v128(p, c, node);
            }
        }
        {
            int c = 0 + 1;
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 234;
            }
        }
        break;
    case BURG_SimdExtractD:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 1;
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 127;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_Throw:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 108;
            }
        }
        break;
    case BURG_MoveI2F:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 88;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_SimdUn:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 1;
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 116;
                closure_v128(p, c, node);
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
        "v128",
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
    case 10: { // v128: LoadLocal
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_GET); ew_u32(E, local_slot(node));
        break;
    }
    case 11: { // ref: LoadThis
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_GET); ew_u32(E, 0); ew_emit(E, WOP_REF_CAST); ew_i32(E, struct_idx(node->load_this.class_id));
        break;
    }
    case 12: { // ref: LoadNull
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_NULL); ew_i64(E, -15);
        break;
    }
    case 13: { // ref: LoadClass
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, wasm_class_singleton_global_index(ctx->types, node->load_class.class_id));
        break;
    }
    case 14: { // i32: Add(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_ADD);
        break;
    }
    case 15: { // i32: Sub(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_SUB);
        break;
    }
    case 16: { // i32: Mul(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_MUL);
        break;
    }
    case 17: { // i32: Div(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_DIV_S);
        break;
    }
    case 18: { // i32: Rem(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_REM_S);
        break;
    }
    case 19: { // i32: And(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_AND);
        break;
    }
    case 20: { // i32: Or(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_OR);
        break;
    }
    case 21: { // i32: Xor(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_XOR);
        break;
    }
    case 22: { // i32: Shl(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_SHL);
        break;
    }
    case 23: { // i32: Shr(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_SHR_S);
        break;
    }
    case 24: { // i32: Ushr(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_SHR_U);
        break;
    }
    case 25: { // i32: Neg(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, -1); ew_emit(E, WOP_I32_MUL);
        break;
    }
    case 26: { // i32: LogNot(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 27: { // i64: Add(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_ADD);
        break;
    }
    case 28: { // i64: Sub(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_SUB);
        break;
    }
    case 29: { // i64: Mul(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_MUL);
        break;
    }
    case 30: { // i64: Div(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_DIV_S);
        break;
    }
    case 31: { // i64: Rem(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_REM_S);
        break;
    }
    case 32: { // i64: And(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_AND);
        break;
    }
    case 33: { // i64: Or(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_OR);
        break;
    }
    case 34: { // i64: Xor(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_XOR);
        break;
    }
    case 35: { // i64: Shl(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_SHL);
        break;
    }
    case 36: { // i64: Shr(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_SHR_S);
        break;
    }
    case 37: { // i64: Ushr(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_SHR_U);
        break;
    }
    case 38: { // i64: Neg(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, -1); ew_emit(E, WOP_I64_MUL);
        break;
    }
    case 39: { // f32: Add(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_ADD);
        break;
    }
    case 40: { // f32: Sub(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_SUB);
        break;
    }
    case 41: { // f32: Mul(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_MUL);
        break;
    }
    case 42: { // f32: Div(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_DIV);
        break;
    }
    case 43: { // f32: Neg(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_NEG);
        break;
    }
    case 44: { // f64: Add(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_ADD);
        break;
    }
    case 45: { // f64: Sub(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_SUB);
        break;
    }
    case 46: { // f64: Mul(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_MUL);
        break;
    }
    case 47: { // f64: Div(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_DIV);
        break;
    }
    case 48: { // f64: Neg(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_NEG);
        break;
    }
    case 49: { // i32: Eq(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQ);
        break;
    }
    case 50: { // i32: Ne(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_NE);
        break;
    }
    case 51: { // i32: Lt(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_LT_S);
        break;
    }
    case 52: { // i32: Le(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_LE_S);
        break;
    }
    case 53: { // i32: Gt(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_GT_S);
        break;
    }
    case 54: { // i32: Ge(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_GE_S);
        break;
    }
    case 55: { // i32: Eq(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQ);
        break;
    }
    case 56: { // i32: Ne(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_NE);
        break;
    }
    case 57: { // i32: Lt(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_LT_S);
        break;
    }
    case 58: { // i32: Le(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_LE_S);
        break;
    }
    case 59: { // i32: Gt(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_GT_S);
        break;
    }
    case 60: { // i32: Ge(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_GE_S);
        break;
    }
    case 61: { // i32: Eq(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_EQ);
        break;
    }
    case 62: { // i32: Ne(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_NE);
        break;
    }
    case 63: { // i32: Lt(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_LT);
        break;
    }
    case 64: { // i32: Le(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_LE);
        break;
    }
    case 65: { // i32: Gt(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_GT);
        break;
    }
    case 66: { // i32: Ge(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_GE);
        break;
    }
    case 67: { // i32: Eq(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_EQ);
        break;
    }
    case 68: { // i32: Ne(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_NE);
        break;
    }
    case 69: { // i32: Lt(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_LT);
        break;
    }
    case 70: { // i32: Le(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_LE);
        break;
    }
    case 71: { // i32: Gt(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_GT);
        break;
    }
    case 72: { // i32: Ge(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_GE);
        break;
    }
    case 73: { // i32: Eq(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_EQ);
        break;
    }
    case 74: { // i32: Ne(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_EQ); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 75: { // i64: I2L(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EXTEND_I32_S);
        break;
    }
    case 76: { // f32: I2F(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_CONVERT_I32_S);
        break;
    }
    case 77: { // f64: I2D(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CONVERT_I32_S);
        break;
    }
    case 78: { // i32: L2I(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_WRAP_I64);
        break;
    }
    case 79: { // f32: L2F(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_CONVERT_I64_S);
        break;
    }
    case 80: { // f64: L2D(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CONVERT_I64_S);
        break;
    }
    case 81: { // f64: F2D(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_PROMOTE_F32);
        break;
    }
    case 82: { // f32: D2F(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_DEMOTE_F64);
        break;
    }
    case 83: { // i32: F2I(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_TRUNC_SAT_F32_S);
        break;
    }
    case 84: { // i32: D2I(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_TRUNC_SAT_F64_S);
        break;
    }
    case 85: { // i64: F2L(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_TRUNC_SAT_F32_S);
        break;
    }
    case 86: { // i64: D2L(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_TRUNC_SAT_F64_S);
        break;
    }
    case 87: { // i32: MoveF2I(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_REINTERPRET_F32);
        break;
    }
    case 88: { // f32: MoveI2F(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_REINTERPRET_I32);
        break;
    }
    case 89: { // i64: MoveD2L(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_REINTERPRET_F64);
        break;
    }
    case 90: { // f64: MoveL2D(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_REINTERPRET_I64);
        break;
    }
    case 91: { // f64: F64Sqrt(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_SQRT);
        break;
    }
    case 92: { // f64: F64Floor(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_FLOOR);
        break;
    }
    case 93: { // f64: F64Ceil(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CEIL);
        break;
    }
    case 94: { // f64: F64Nearest(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_NEAREST);
        break;
    }
    case 95: { // i32: I2B(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EXTEND8_S);
        break;
    }
    case 96: { // i32: I2S(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EXTEND16_S);
        break;
    }
    case 97: { // i32: I2C(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, 0xFFFF); ew_emit(E, WOP_I32_AND);
        break;
    }
    case 98: { // i32: S2B(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EXTEND8_S);
        break;
    }
    case 99: { // i32: S2I(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        /* short→int: both i32, no-op (the operand already an i32) */
        break;
    }
    case 100: { // stmt: Return(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 101: { // stmt: Return(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 102: { // stmt: Return(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 103: { // stmt: Return(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 104: { // stmt: Return(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 105: { // stmt: Return(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 106: { // stmt: ReturnVoid
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 107: { // stmt: Return(tail)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        /* the tail invoke already emitted return_call* and returned */
        break;
    }
    case 108: { // stmt: Throw(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_THROW); ew_u32(E, 0);
        break;
    }
    case 109: { // stmt: StoreLocal(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 110: { // stmt: StoreLocal(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 111: { // stmt: StoreLocal(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 112: { // stmt: StoreLocal(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 113: { // stmt: StoreLocal(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 114: { // stmt: StoreLocal(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 115: { // v128: SimdBin(v128, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_bin.op);
        break;
    }
    case 116: { // v128: SimdUn(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_un.op);
        break;
    }
    case 117: { // v128: SimdShift(v128, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_shift.op);
        break;
    }
    case 118: { // v128: SimdTern(v128, v128, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_tern.op);
        break;
    }
    case 119: { // i32: SimdTestI(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_test_i.op);
        break;
    }
    case 120: { // v128: SimdSplatI(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_i.op);
        break;
    }
    case 121: { // v128: SimdSplatL(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_l.op);
        break;
    }
    case 122: { // v128: SimdSplatF(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_f.op);
        break;
    }
    case 123: { // v128: SimdSplatD(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_d.op);
        break;
    }
    case 124: { // i32: SimdExtractI(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_i.op); ew_byte(E, (uint8_t)node->simd_extract_i.lane);
        break;
    }
    case 125: { // i64: SimdExtractL(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_l.op); ew_byte(E, (uint8_t)node->simd_extract_l.lane);
        break;
    }
    case 126: { // f32: SimdExtractF(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_f.op); ew_byte(E, (uint8_t)node->simd_extract_f.lane);
        break;
    }
    case 127: { // f64: SimdExtractD(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_d.op); ew_byte(E, (uint8_t)node->simd_extract_d.lane);
        break;
    }
    case 128: { // v128: SimdReplaceI(v128, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_i.op); ew_byte(E, (uint8_t)node->simd_replace_i.lane);
        break;
    }
    case 129: { // v128: SimdReplaceL(v128, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_l.op); ew_byte(E, (uint8_t)node->simd_replace_l.lane);
        break;
    }
    case 130: { // v128: SimdReplaceF(v128, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_f.op); ew_byte(E, (uint8_t)node->simd_replace_f.lane);
        break;
    }
    case 131: { // v128: SimdReplaceD(v128, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_d.op); ew_byte(E, (uint8_t)node->simd_replace_d.lane);
        break;
    }
    case 132: { // v128: SimdConst
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_V128_CONST);
                      for (int b = 0; b < 8; b++) ew_byte(E, (uint8_t)((uint64_t)node->simd_const.lo >> (8*b)));
                      for (int b = 0; b < 8; b++) ew_byte(E, (uint8_t)((uint64_t)node->simd_const.hi >> (8*b)));
        break;
    }
    case 133: { // v128: SimdShuffle(v128, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I8X16_SHUFFLE);
                      for (int b = 0; b < 8; b++) ew_byte(E, (uint8_t)((uint64_t)node->simd_shuffle.lo >> (8*b)));
                      for (int b = 0; b < 8; b++) ew_byte(E, (uint8_t)((uint64_t)node->simd_shuffle.hi >> (8*b)));
        break;
    }
    case 134: { // v128: SimdMemLoad(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_load.op); ew_u32(E, (uint32_t)node->simd_mem_load.align); ew_u32(E, 0);
        break;
    }
    case 135: { // v128: SimdMemLoadLane(i32, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_load_lane.op); ew_u32(E, (uint32_t)node->simd_mem_load_lane.align); ew_u32(E, 0); ew_byte(E, (uint8_t)node->simd_mem_load_lane.lane);
        break;
    }
    case 136: { // i32: MemLoadI(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_load_i.op); ew_u32(E, (uint32_t)node->mem_load_i.align); ew_u32(E, 0);
        break;
    }
    case 137: { // i64: MemLoadL(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_load_l.op); ew_u32(E, (uint32_t)node->mem_load_l.align); ew_u32(E, 0);
        break;
    }
    case 138: { // f32: MemLoadF(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_load_f.op); ew_u32(E, (uint32_t)node->mem_load_f.align); ew_u32(E, 0);
        break;
    }
    case 139: { // f64: MemLoadD(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_load_d.op); ew_u32(E, (uint32_t)node->mem_load_d.align); ew_u32(E, 0);
        break;
    }
    case 140: { // i32: MemSize
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_MEMORY_SIZE); ew_u32(E, 0);
        break;
    }
    case 141: { // i32: MemGrow(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_MEMORY_GROW); ew_u32(E, 0);
        break;
    }
    case 142: { // stmt: Inc(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, node->inc.delta); ew_emit(E, WOP_I32_ADD); CG_NARROW(E, node->inc.data_type); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 143: { // stmt: Inc(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, (int64_t)node->inc.delta); ew_emit(E, WOP_I64_ADD); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 144: { // stmt: Inc(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_CONST); ew_f32(E, (float)node->inc.delta); ew_emit(E, WOP_F32_ADD); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 145: { // stmt: Inc(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CONST); ew_f64(E, (double)node->inc.delta); ew_emit(E, WOP_F64_ADD); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 146: { // stmt: ExprEffect(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        if (!node->expr_effect.is_void) ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 147: { // stmt: ExprEffect(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        if (!node->expr_effect.is_void) ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 148: { // stmt: ExprEffect(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        if (!node->expr_effect.is_void) ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 149: { // stmt: ExprEffect(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        if (!node->expr_effect.is_void) ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 150: { // stmt: ExprEffect(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        if (!node->expr_effect.is_void) ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 151: { // stmt: ExprEffect(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        if (!node->expr_effect.is_void) ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 152: { // ref: New
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wasm_types_emit_new(ctx->types, E, node->new_.class_id);
        break;
    }
    case 153: { // i32: ClassInstantiable(ref)
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
    case 154: { // ref: ClassConstruct(ref)
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
    case 155: { // ref: CloneCopy
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wasm_types_emit_clone_copy(ctx->types, E, node->clone_copy.class_id);
        break;
    }
    case 156: { // i32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET);   ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 157: { // i32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET_S); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 158: { // i32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET_U); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 159: { // i64: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 160: { // f32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 161: { // f64: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 162: { // ref: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 163: { // v128: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 164: { // stmt: PutField(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 165: { // stmt: PutField(ref, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 166: { // stmt: PutField(ref, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 167: { // stmt: PutField(ref, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 168: { // stmt: PutField(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 169: { // stmt: PutField(ref, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 170: { // stmt: SetHeader(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->set_header.struct_class_id)); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 171: { // i32: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 172: { // i64: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 173: { // f32: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 174: { // f64: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 175: { // ref: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 176: { // v128: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 177: { // stmt: PutStatic(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 178: { // stmt: PutStatic(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 179: { // stmt: PutStatic(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 180: { // stmt: PutStatic(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 181: { // stmt: PutStatic(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 182: { // stmt: PutStatic(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 183: { // stmt: SimdMemStore(i32, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_store.op); ew_u32(E, (uint32_t)node->simd_mem_store.align); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 184: { // stmt: SimdMemStoreLane(i32, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_store_lane.op); ew_u32(E, (uint32_t)node->simd_mem_store_lane.align); ew_u32(E, 0); ew_byte(E, (uint8_t)node->simd_mem_store_lane.lane); cg_jump(E, node);
        break;
    }
    case 185: { // stmt: MemStoreI(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_store_i.op); ew_u32(E, (uint32_t)node->mem_store_i.align); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 186: { // stmt: MemStoreL(i32, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_store_l.op); ew_u32(E, (uint32_t)node->mem_store_l.align); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 187: { // stmt: MemStoreF(i32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_store_f.op); ew_u32(E, (uint32_t)node->mem_store_f.align); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 188: { // stmt: MemStoreD(i32, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_store_d.op); ew_u32(E, (uint32_t)node->mem_store_d.align); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 189: { // stmt: MemFill(i32, i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_MEMORY_FILL); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 190: { // stmt: MemCopy(i32, i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_MEMORY_COPY); ew_u32(E, 0); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 191: { // ref: NewArray(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_NEW_DEFAULT); ew_u32(E, wasm_types_array_for_atype(ctx->types, node->new_array.elem_type));
        break;
    }
    case 192: { // ref: ArrayNewData
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, node->array_new_data.byte_off);
                            ew_emit(E, WOP_I32_CONST); ew_i32(E, node->array_new_data.count);
                            ew_emit(E, WOP_ARRAY_NEW_DATA);
                            ew_u32(E, wasm_types_array_for_atype(ctx->types, node->array_new_data.elem_type));
                            ew_u32(E, (uint32_t)node->array_new_data.seg);
        break;
    }
    case 193: { // i32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET);   ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 194: { // i32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET_S); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 195: { // i32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET_U); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 196: { // i64: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 197: { // f32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 198: { // f64: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 199: { // v128: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 200: { // stmt: ArrayStore(ref, i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 201: { // stmt: ArrayStore(ref, i32, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 202: { // stmt: ArrayStore(ref, i32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 4, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 203: { // stmt: ArrayStore(ref, i32, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 5, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 204: { // stmt: ArrayStore(ref, i32, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 205: { // i32: ArrayLength(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_LEN);
        break;
    }
    case 206: { // ref: NewRefArray(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_NEW_DEFAULT); ew_u32(E, wasm_types_array_for_dt(ctx->types, SIR_DTREF));
        break;
    }
    case 207: { // ref: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, SIR_DTREF)); ew_emit(E, WOP_REF_CAST_NULL); ew_i32(E, wasm_types_ref_typeidx(ctx->types, node->array_load.elem_ref));
        break;
    }
    case 208: { // stmt: ArrayStore(ref, i32, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, SIR_DTREF)); cg_jump(E, node);
        break;
    }
    case 209: { // stmt: ArrayCopy(ref, i32, ref, i32, i32)
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
    case 210: { // i32: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 211: { // i64: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 212: { // f32: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 213: { // f64: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 214: { // ref: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 215: { // v128: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 216: { // i32: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 217: { // i64: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 218: { // f32: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 219: { // f64: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 220: { // ref: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 221: { // v128: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 222: { // i32: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 223: { // i64: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 224: { // f32: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 225: { // f64: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 226: { // ref: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 227: { // v128: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 228: { // i32: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 229: { // i64: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 230: { // f32: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 231: { // f64: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 232: { // ref: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 233: { // v128: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 234: { // tail: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 235: { // tail: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 236: { // tail: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        TVCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 237: { // tail: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        TIVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 238: { // i32: InstanceOf(ref)
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
    case 239: { // ref: CheckCast(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        int _cc = node->check_cast.class_id;
    int _to = wasm_class_is_interface(ctx->types, _cc) ? wasm_root_class(ctx->types) : _cc;
    ew_emit(E, WOP_REF_CAST_NULL); ew_i32(E, struct_idx(_to));
        break;
    }
    case 240: { // stmt: ExceptionEntry
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->exception_entry.local_slot);
    cg_jump(E, node);
        break;
    }
    case 241: { // stmt: Nop
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        /* label anchor — structurer owns the scope */
        break;
    }
    case 242: { // stmt: i32
        burg_reduce(node, state, 2, ctx);
        break;
    }
    case 243: { // stmt: i64
        burg_reduce(node, state, 3, ctx);
        break;
    }
    case 244: { // stmt: f32
        burg_reduce(node, state, 4, ctx);
        break;
    }
    case 245: { // stmt: f64
        burg_reduce(node, state, 5, ctx);
        break;
    }
    case 246: { // stmt: ref
        burg_reduce(node, state, 6, ctx);
        break;
    }
    case 247: { // stmt: v128
        burg_reduce(node, state, 7, ctx);
        break;
    }
    default:
        burg_set_error("burg: no rule for goal nonterminal", goalnt, ctx);
        break;
    }
}

void burg_rewrite(BURG_NODE_TYPE root, burg_ctx_t* ctx) {
    if (burg_has_error(ctx)) return;
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
