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
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 376;
    }
}

static void closure_ref(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 375;
    }
}

static void closure_f64(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 374;
    }
}

static void closure_i64(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 372;
    }
}

static void closure_f32(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 373;
    }
}

static void closure_i32(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[8]) {
        p->cost[8] = c + 0;
        p->rule[8] = 147;
    }
    if (c + 1 < p->cost[9]) {
        p->cost[9] = c + 1;
        p->rule[9] = 148;
    }
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 371;
    }
}


static void burg_dp(burg_state_t* p, BURG_NODE_TYPE node, burg_ctx_t* ctx) {
    (void)ctx;
    int op = p->op;
    switch (op) {
    case BURG_ExceptionEntry:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 369;
            }
        }
        break;
    case BURG_CheckCast:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 368;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_InvokeInterface:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (DT_IS_I32(node->invoke_interface.return_type))) {
            int c = p->children[0]->cost[6] + 23;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 357;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 23;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 358;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 23;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 359;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 23;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 360;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 23;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 361;
                closure_ref(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_interface.return_type == SIR_DTV128)) {
            int c = p->children[0]->cost[6] + 23;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 362;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 23;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[10]) {
                p->cost[10] = c;
                p->rule[10] = 366;
            }
        }
        break;
    case BURG_InvokeVirtual:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (DT_IS_I32(node->invoke_virtual.return_type))) {
            int c = p->children[0]->cost[6] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 351;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 352;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 353;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 354;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 355;
                closure_ref(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_virtual.return_type == SIR_DTV128)) {
            int c = p->children[0]->cost[6] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 356;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[10]) {
                p->cost[10] = c;
                p->rule[10] = 365;
            }
        }
        break;
    case BURG_InvokeSpecial:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (DT_IS_I32(node->invoke_special.return_type))) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 345;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 346;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 347;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 348;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 349;
                closure_ref(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->invoke_special.return_type == SIR_DTV128)) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 350;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[10]) {
                p->cost[10] = c;
                p->rule[10] = 364;
            }
        }
        break;
    case BURG_ArrayLength:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 334;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_ArrayStore:
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[2] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 329;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[3] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 330;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[4]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[4] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 331;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[5]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[5] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 332;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[7]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[7] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 333;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[6] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 337;
            }
        }
        break;
    case BURG_Nop:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 370;
            }
        }
        break;
    case BURG_NewArray:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 320;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_MemStoreF:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[4] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 316;
            }
        }
        break;
    case BURG_SimdMemStoreLane:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[7] && (ew_op_width((wasm_op_t)node->simd_mem_store_lane.op) == 2)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 312;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[7] && (ew_op_width((wasm_op_t)node->simd_mem_store_lane.op) == 3)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 313;
            }
        }
        break;
    case BURG_ClassConstruct:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 281;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_MemStoreD:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[5] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 317;
            }
        }
        break;
    case BURG_New:
        {
            int c = 0 + 5;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 279;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_MemLoadD:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 258;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_InstanceOf:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 367;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MemLoadF:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 257;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_SimdMemLoadLane:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[7] && (ew_op_width((wasm_op_t)node->simd_mem_load_lane.op) == 2)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 253;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[7] && (ew_op_width((wasm_op_t)node->simd_mem_load_lane.op) == 3)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 254;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SimdMemLoad:
        if (p->child_count >= 1 && p->children[0]->rule[2] && (ew_op_width((wasm_op_t)node->simd_mem_load.op) == 2)) {
            int c = p->children[0]->cost[2] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 251;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2] && (ew_op_width((wasm_op_t)node->simd_mem_load.op) == 3)) {
            int c = p->children[0]->cost[2] + 5;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 252;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_PutStatic:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 304;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 305;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 306;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 307;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 308;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 309;
            }
        }
        break;
    case BURG_SimdConst:
        {
            int c = 0 + 18;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 249;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SimdReplaceF:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[4] && (ew_op_width((wasm_op_t)node->simd_replace_f.op) == 2)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[4] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 245;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[4] && (ew_op_width((wasm_op_t)node->simd_replace_f.op) == 3)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[4] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 246;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_MemFill:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[2] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 318;
            }
        }
        break;
    case BURG_SimdExtractF:
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_extract_f.op) == 2)) {
            int c = p->children[0]->cost[7] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 237;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_extract_f.op) == 3)) {
            int c = p->children[0]->cost[7] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 238;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_SimdMemStore:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[7] && (ew_op_width((wasm_op_t)node->simd_mem_store.op) == 2)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 310;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[7] && (ew_op_width((wasm_op_t)node->simd_mem_store.op) == 3)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 311;
            }
        }
        break;
    case BURG_MemGrow:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 260;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_SimdExtractL:
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_extract_l.op) == 2)) {
            int c = p->children[0]->cost[7] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 235;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_extract_l.op) == 3)) {
            int c = p->children[0]->cost[7] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 236;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_SimdExtractI:
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_extract_i.op) == 2)) {
            int c = p->children[0]->cost[7] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 233;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_extract_i.op) == 3)) {
            int c = p->children[0]->cost[7] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 234;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_SimdSplatL:
        if (p->child_count >= 1 && p->children[0]->rule[3] && (ew_op_width((wasm_op_t)node->simd_splat_l.op) == 2)) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 227;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3] && (ew_op_width((wasm_op_t)node->simd_splat_l.op) == 3)) {
            int c = p->children[0]->cost[3] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 228;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SimdSplatI:
        if (p->child_count >= 1 && p->children[0]->rule[2] && (ew_op_width((wasm_op_t)node->simd_splat_i.op) == 2)) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 225;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2] && (ew_op_width((wasm_op_t)node->simd_splat_i.op) == 3)) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 226;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SimdTestI:
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_test_i.op) == 2)) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 223;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_test_i.op) == 3)) {
            int c = p->children[0]->cost[7] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 224;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_SimdTern:
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[7] && p->children[2]->rule[7] && (ew_op_width((wasm_op_t)node->simd_tern.op) == 2)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + p->children[2]->cost[7] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 221;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[7] && p->children[2]->rule[7] && (ew_op_width((wasm_op_t)node->simd_tern.op) == 3)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + p->children[2]->cost[7] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 222;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SimdShift:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2] && (ew_op_width((wasm_op_t)node->simd_shift.op) == 2)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 219;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2] && (ew_op_width((wasm_op_t)node->simd_shift.op) == 3)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 220;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_GeU:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 55;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_PutField:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 291;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 292;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[4] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 293;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[5] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 294;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 295;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[7] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 296;
            }
        }
        break;
    case BURG_Lt:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 51;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 58;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 64;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 70;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Shl:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 22;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 35;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 100;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 115;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->op == BURG_LoadConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 124;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->op == BURG_LoadLongConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 136;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_SimdReplaceL:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3] && (ew_op_width((wasm_op_t)node->simd_replace_l.op) == 2)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 243;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3] && (ew_op_width((wasm_op_t)node->simd_replace_l.op) == 3)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 244;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_LoadConst:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
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
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 54;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 61;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 67;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 73;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_I2L:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 175;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_MemStoreI:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 314;
            }
        }
        break;
    case BURG_Eq:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 49;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 56;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 62;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 68;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 74;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 76;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (CV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 77;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 80;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (LV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 81;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->op == BURG_LoadNull) {
            int c = p->children[0]->cost[6] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 84;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadNull && p->children[1]->rule[6]) {
            int c = p->children[1]->cost[6] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 85;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 155;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (CV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 156;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 157;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (CV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 158;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 159;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (LV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 160;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 161;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (LV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[3] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 162;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->op == BURG_LoadNull) {
            int c = p->children[0]->cost[6] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 167;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadNull && p->children[1]->rule[6]) {
            int c = p->children[1]->cost[6] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 168;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->op == BURG_LoadNull) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 169;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadNull && p->children[1]->rule[6]) {
            int c = p->children[1]->cost[6] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 170;
            }
        }
        break;
    case BURG_D2I:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 184;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MoveF2I:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 187;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Xor:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 21;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 34;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 96;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (CV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 97;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 111;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (LV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 112;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->op == BURG_LoadConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 123;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->op == BURG_LoadLongConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 135;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_D2L:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 186;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_Return:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 200;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 201;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 202;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 203;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 204;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 205;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[10]) {
            int c = p->children[0]->cost[10] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 207;
            }
        }
        break;
    case BURG_MemCopy:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 319;
            }
        }
        break;
    case BURG_LoadFloatConst:
        {
            int c = 0 + 5;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 3;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_SimdSplatD:
        if (p->child_count >= 1 && p->children[0]->rule[5] && (ew_op_width((wasm_op_t)node->simd_splat_d.op) == 2)) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 231;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5] && (ew_op_width((wasm_op_t)node->simd_splat_d.op) == 3)) {
            int c = p->children[0]->cost[5] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 232;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_L2F:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 179;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_NewRefArray:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 335;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_GetField:
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTINT)) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 283;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTBYTE || node->get_field.data_type == SIR_DTSHORT)) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 284;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTCHAR)) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 285;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 286;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 287;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 288;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 289;
                closure_ref(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->get_field.data_type == SIR_DTV128)) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 290;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_LoadDoubleConst:
        {
            int c = 0 + 9;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 4;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_ArrayLoad:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTINT)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 322;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTBYTE || node->array_load.data_type == SIR_DTSHORT)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 323;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTCHAR)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 324;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTLONG)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 325;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTFLOAT)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 326;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTDOUBLE)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 327;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTV128)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 328;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2] && (node->array_load.data_type == SIR_DTREF)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 336;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_SimdSplatF:
        if (p->child_count >= 1 && p->children[0]->rule[4] && (ew_op_width((wasm_op_t)node->simd_splat_f.op) == 2)) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 229;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4] && (ew_op_width((wasm_op_t)node->simd_splat_f.op) == 3)) {
            int c = p->children[0]->cost[4] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 230;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_LoadNull:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
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
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 14;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 27;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 39;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 44;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 88;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (CV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 89;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 103;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (LV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 104;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->op == BURG_LoadConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 118;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->op == BURG_LoadLongConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 130;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_ClassInstantiable:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 6;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 280;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Ne:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 50;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 57;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 63;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 69;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 75;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 78;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (CV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[2] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 79;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 82;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (LV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[3] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 83;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->op == BURG_LoadNull) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 86;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadNull && p->children[1]->rule[6]) {
            int c = p->children[1]->cost[6] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 87;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 151;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (CV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 152;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 153;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (CV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 154;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 163;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (LV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[3] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 164;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 165;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (LV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 166;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->op == BURG_LoadNull) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 171;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadNull && p->children[1]->rule[6]) {
            int c = p->children[1]->cost[6] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 172;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->op == BURG_LoadNull) {
            int c = p->children[0]->cost[6] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 173;
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadNull && p->children[1]->rule[6]) {
            int c = p->children[1]->cost[6] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 174;
            }
        }
        break;
    case BURG_MoveL2D:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 190;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_ExprEffect:
        if (p->child_count >= 1 && p->children[0]->rule[2] && (node->expr_effect.is_void)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 267;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3] && (node->expr_effect.is_void)) {
            int c = p->children[0]->cost[3] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 268;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4] && (node->expr_effect.is_void)) {
            int c = p->children[0]->cost[4] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 269;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5] && (node->expr_effect.is_void)) {
            int c = p->children[0]->cost[5] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 270;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (node->expr_effect.is_void)) {
            int c = p->children[0]->cost[6] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 271;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7] && (node->expr_effect.is_void)) {
            int c = p->children[0]->cost[7] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 272;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2] && (!node->expr_effect.is_void)) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 273;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3] && (!node->expr_effect.is_void)) {
            int c = p->children[0]->cost[3] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 274;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4] && (!node->expr_effect.is_void)) {
            int c = p->children[0]->cost[4] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 275;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5] && (!node->expr_effect.is_void)) {
            int c = p->children[0]->cost[5] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 276;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6] && (!node->expr_effect.is_void)) {
            int c = p->children[0]->cost[6] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 277;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7] && (!node->expr_effect.is_void)) {
            int c = p->children[0]->cost[7] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 278;
            }
        }
        break;
    case BURG_LoadLocal:
        if ((DT_IS_I32(ll_dt(node)))) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 5;
                closure_i32(p, c, node);
            }
        }
        if ((ll_dt(node) == SIR_DTLONG)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 6;
                closure_i64(p, c, node);
            }
        }
        if ((ll_dt(node) == SIR_DTFLOAT)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 7;
                closure_f32(p, c, node);
            }
        }
        if ((ll_dt(node) == SIR_DTDOUBLE)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 8;
                closure_f64(p, c, node);
            }
        }
        if ((ll_dt(node) == SIR_DTREF)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 9;
                closure_ref(p, c, node);
            }
        }
        if ((ll_dt(node) == SIR_DTV128)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 10;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SimdReplaceI:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2] && (ew_op_width((wasm_op_t)node->simd_replace_i.op) == 2)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 241;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2] && (ew_op_width((wasm_op_t)node->simd_replace_i.op) == 3)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 242;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_Ushr:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 24;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 37;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 102;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 117;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->op == BURG_LoadConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 126;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->op == BURG_LoadLongConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 138;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_ArrayCopy:
        if (p->child_count >= 5 && p->children[0]->rule[6] && p->children[1]->rule[2] && p->children[2]->rule[6] && p->children[3]->rule[2] && p->children[4]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + p->children[2]->cost[6] + p->children[3]->cost[2] + p->children[4]->cost[2] + 4;
            for (int _ci = 5; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 338;
            }
        }
        break;
    case BURG_LoadThis:
        {
            int c = 0 + 5;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
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
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 206;
            }
        }
        break;
    case BURG_Mul:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 16;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 29;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 41;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 46;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 1)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 91;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (CV(KID(node,0)) == 1)) {
            int c = p->children[1]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 92;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 1)) {
            int c = p->children[0]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 106;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (LV(KID(node,0)) == 1)) {
            int c = p->children[1]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 107;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->op == BURG_LoadConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 120;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->op == BURG_LoadLongConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 132;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (JF_POW2(CV(KID(node,1))))) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 142;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (JF_POW2(CV(KID(node,0))))) {
            int c = p->children[1]->cost[2] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 143;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (JF_POW2(LV(KID(node,1))))) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 144;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (JF_POW2(LV(KID(node,0))))) {
            int c = p->children[1]->cost[3] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 145;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_Le:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 52;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 59;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 65;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 71;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Shr:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 23;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 36;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 101;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 116;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->op == BURG_LoadConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 125;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->op == BURG_LoadLongConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 137;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_I2D:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 177;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_Neg:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 25;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 38;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 43;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 48;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->op == BURG_LoadConst) {
            int c = 0 + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 127;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->op == BURG_LoadLongConst) {
            int c = 0 + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 139;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_SimdReplaceD:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[5] && (ew_op_width((wasm_op_t)node->simd_replace_d.op) == 2)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[5] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 247;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[5] && (ew_op_width((wasm_op_t)node->simd_replace_d.op) == 3)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[5] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 248;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_LoadLongConst:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 2;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_GetStatic:
        if ((DT_IS_I32(node->get_static.data_type))) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 298;
                closure_i32(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTLONG)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 299;
                closure_i64(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTFLOAT)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 300;
                closure_f32(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTDOUBLE)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 301;
                closure_f64(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTREF)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 302;
                closure_ref(p, c, node);
            }
        }
        if ((node->get_static.data_type == SIR_DTV128)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 303;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_SetHeader:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 297;
            }
        }
        break;
    case BURG_LoadClass:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
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
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 196;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_ArrayNewData:
        {
            int c = 0 + 8;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 321;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_MemSize:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 259;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_F64Nearest:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 194;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_SimdBin:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[7] && (ew_op_width((wasm_op_t)node->simd_bin.op) == 2)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 215;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[7] && (ew_op_width((wasm_op_t)node->simd_bin.op) == 3)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 216;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_LogNot:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 26;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[9]) {
            int c = p->children[0]->cost[9] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 149;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[9]) {
                p->cost[9] = c;
                p->rule[9] = 150;
            }
        }
        break;
    case BURG_Inc:
        if (p->child_count >= 1 && p->children[0]->rule[2] && (node->inc.data_type == SIR_DTINT)) {
            int c = p->children[0]->cost[2] + 5;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 261;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2] && (node->inc.data_type == SIR_DTBYTE || node->inc.data_type == SIR_DTSHORT)) {
            int c = p->children[0]->cost[2] + 6;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 262;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2] && (node->inc.data_type == SIR_DTCHAR)) {
            int c = p->children[0]->cost[2] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 263;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 5;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 264;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 265;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 266;
            }
        }
        break;
    case BURG_Sub:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 15;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 28;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 40;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 45;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 90;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 105;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->op == BURG_LoadConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 119;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->op == BURG_LoadLongConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 131;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_And:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 19;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 32;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == -1)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 98;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (CV(KID(node,0)) == -1)) {
            int c = p->children[1]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 99;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == -1)) {
            int c = p->children[0]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 113;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (LV(KID(node,0)) == -1)) {
            int c = p->children[1]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 114;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->op == BURG_LoadConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 121;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->op == BURG_LoadLongConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 133;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_D2F:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 182;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_F2L:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 185;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_Div:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 17;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 30;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 42;
                closure_f32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 47;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 1)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 93;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 1)) {
            int c = p->children[0]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 108;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->op == BURG_LoadConst && (JF_DIVOK32(CV(KID(node,0)), CV(KID(node,1))))) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 128;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->op == BURG_LoadLongConst && (JF_DIVOK64(LV(KID(node,0)), LV(KID(node,1))))) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 140;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (JF_POW2(CV(KID(node,1)))
                                  && jf_pub_nonneg_i32(ctx->facts, KID(node,0)))) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 146;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Rem:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 18;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 31;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->op == BURG_LoadConst && (JF_DIVOK32(CV(KID(node,0)), CV(KID(node,1))))) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 129;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->op == BURG_LoadLongConst && (JF_DIVOK64(LV(KID(node,0)), LV(KID(node,1))))) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 141;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_MemLoadL:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 256;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_I2F:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 176;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_SimdShuffle:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + 18;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 250;
                closure_v128(p, c, node);
            }
        }
        break;
    case BURG_L2I:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 178;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MemStoreL:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 315;
            }
        }
        break;
    case BURG_F64Ceil:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 193;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_MemLoadI:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 255;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_Or:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 20;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 33;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->op == BURG_LoadConst && (CV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 94;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->rule[2] && (CV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[2] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 95;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->op == BURG_LoadLongConst && (LV(KID(node,1)) == 0)) {
            int c = p->children[0]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 109;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->rule[3] && (LV(KID(node,0)) == 0)) {
            int c = p->children[1]->cost[3] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 110;
                closure_i64(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadConst && p->children[1]->op == BURG_LoadConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 122;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->op == BURG_LoadLongConst && p->children[1]->op == BURG_LoadLongConst) {
            int c = 0 + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 134;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_L2D:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 180;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_I2B:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 195;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_CloneCopy:
        {
            int c = 0 + 3;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 282;
                closure_ref(p, c, node);
            }
        }
        break;
    case BURG_F2D:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 181;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_Gt:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 53;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 60;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 66;
                closure_i32(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 1;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 72;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_F2I:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 183;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_MoveD2L:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 189;
                closure_i64(p, c, node);
            }
        }
        break;
    case BURG_F64Sqrt:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 191;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_F64Floor:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 192;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_StoreLocal:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 209;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 210;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 211;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 212;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 213;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 214;
            }
        }
        break;
    case BURG_I2C:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 5;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 197;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_S2B:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 198;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_S2I:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 199;
                closure_i32(p, c, node);
            }
        }
        break;
    case BURG_InvokeStatic:
        if ((DT_IS_I32(node->invoke_static.return_type))) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 339;
                closure_i32(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTLONG)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 340;
                closure_i64(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTFLOAT)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 341;
                closure_f32(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTDOUBLE)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 342;
                closure_f64(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTREF)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 343;
                closure_ref(p, c, node);
            }
        }
        if ((node->invoke_static.return_type == SIR_DTV128)) {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 344;
                closure_v128(p, c, node);
            }
        }
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[10]) {
                p->cost[10] = c;
                p->rule[10] = 363;
            }
        }
        break;
    case BURG_SimdExtractD:
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_extract_d.op) == 2)) {
            int c = p->children[0]->cost[7] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 239;
                closure_f64(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_extract_d.op) == 3)) {
            int c = p->children[0]->cost[7] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 240;
                closure_f64(p, c, node);
            }
        }
        break;
    case BURG_Throw:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 208;
            }
        }
        break;
    case BURG_MoveI2F:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 1;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 188;
                closure_f32(p, c, node);
            }
        }
        break;
    case BURG_SimdUn:
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_un.op) == 2)) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 217;
                closure_v128(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7] && (ew_op_width((wasm_op_t)node->simd_un.op) == 3)) {
            int c = p->children[0]->cost[7] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 218;
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

int burg_cost(burg_state_t* state, int goalnt) {
    if (!state || goalnt < 1 || goalnt > BURG_MAX_NT) return BURG_MAX_COST;
    return state->cost[goalnt];
}

burg_state_t* burg_label_root(BURG_NODE_TYPE root, burg_ctx_t* ctx) {
    if (burg_has_error(ctx)) return NULL;
    arena_reset(ctx);
    return burg_label_tree(root, ctx);
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
        "cond",
        "ncond",
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
    case 55: { // i32: GeU(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_GE_U);
        break;
    }
    case 56: { // i32: Eq(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQ);
        break;
    }
    case 57: { // i32: Ne(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_NE);
        break;
    }
    case 58: { // i32: Lt(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_LT_S);
        break;
    }
    case 59: { // i32: Le(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_LE_S);
        break;
    }
    case 60: { // i32: Gt(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_GT_S);
        break;
    }
    case 61: { // i32: Ge(i64, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_GE_S);
        break;
    }
    case 62: { // i32: Eq(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_EQ);
        break;
    }
    case 63: { // i32: Ne(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_NE);
        break;
    }
    case 64: { // i32: Lt(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_LT);
        break;
    }
    case 65: { // i32: Le(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_LE);
        break;
    }
    case 66: { // i32: Gt(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_GT);
        break;
    }
    case 67: { // i32: Ge(f32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_GE);
        break;
    }
    case 68: { // i32: Eq(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_EQ);
        break;
    }
    case 69: { // i32: Ne(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_NE);
        break;
    }
    case 70: { // i32: Lt(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_LT);
        break;
    }
    case 71: { // i32: Le(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_LE);
        break;
    }
    case 72: { // i32: Gt(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_GT);
        break;
    }
    case 73: { // i32: Ge(f64, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_GE);
        break;
    }
    case 74: { // i32: Eq(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_EQ);
        break;
    }
    case 75: { // i32: Ne(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_EQ); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 76: { // i32: Eq(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 77: { // i32: Eq(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 78: { // i32: Ne(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQZ); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 79: { // i32: Ne(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQZ); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 80: { // i32: Eq(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ);
        break;
    }
    case 81: { // i32: Eq(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ);
        break;
    }
    case 82: { // i32: Ne(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 83: { // i32: Ne(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 84: { // i32: Eq(ref, LoadNull)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL);
        break;
    }
    case 85: { // i32: Eq(LoadNull, ref)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL);
        break;
    }
    case 86: { // i32: Ne(ref, LoadNull)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 87: { // i32: Ne(LoadNull, ref)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 88: { // i32: Add(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 89: { // i32: Add(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 90: { // i32: Sub(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 91: { // i32: Mul(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 92: { // i32: Mul(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 93: { // i32: Div(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 94: { // i32: Or(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 95: { // i32: Or(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 96: { // i32: Xor(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 97: { // i32: Xor(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 98: { // i32: And(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 99: { // i32: And(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 100: { // i32: Shl(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 101: { // i32: Shr(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 102: { // i32: Ushr(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 103: { // i64: Add(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 104: { // i64: Add(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 105: { // i64: Sub(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 106: { // i64: Mul(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 107: { // i64: Mul(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 108: { // i64: Div(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 109: { // i64: Or(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 110: { // i64: Or(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 111: { // i64: Xor(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 112: { // i64: Xor(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 113: { // i64: And(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 114: { // i64: And(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 115: { // i64: Shl(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 116: { // i64: Shr(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 117: { // i64: Ushr(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 118: { // i32: Add(LoadConst, LoadConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, jf_add32(CV(KID(node,0)), CV(KID(node,1))));
        break;
    }
    case 119: { // i32: Sub(LoadConst, LoadConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, jf_sub32(CV(KID(node,0)), CV(KID(node,1))));
        break;
    }
    case 120: { // i32: Mul(LoadConst, LoadConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, jf_mul32(CV(KID(node,0)), CV(KID(node,1))));
        break;
    }
    case 121: { // i32: And(LoadConst, LoadConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, CV(KID(node,0)) & CV(KID(node,1)));
        break;
    }
    case 122: { // i32: Or(LoadConst, LoadConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, CV(KID(node,0)) | CV(KID(node,1)));
        break;
    }
    case 123: { // i32: Xor(LoadConst, LoadConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, CV(KID(node,0)) ^ CV(KID(node,1)));
        break;
    }
    case 124: { // i32: Shl(LoadConst, LoadConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, jf_shl32(CV(KID(node,0)), CV(KID(node,1))));
        break;
    }
    case 125: { // i32: Shr(LoadConst, LoadConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, jf_shr32(CV(KID(node,0)), CV(KID(node,1))));
        break;
    }
    case 126: { // i32: Ushr(LoadConst, LoadConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, jf_ushr32(CV(KID(node,0)), CV(KID(node,1))));
        break;
    }
    case 127: { // i32: Neg(LoadConst)
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, jf_sub32(0, CV(KID(node,0))));
        break;
    }
    case 128: { // i32: Div(LoadConst, LoadConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, CV(KID(node,0)) / CV(KID(node,1)));
        break;
    }
    case 129: { // i32: Rem(LoadConst, LoadConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, CV(KID(node,0)) % CV(KID(node,1)));
        break;
    }
    case 130: { // i64: Add(LoadLongConst, LoadLongConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, jf_add64(LV(KID(node,0)), LV(KID(node,1))));
        break;
    }
    case 131: { // i64: Sub(LoadLongConst, LoadLongConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, jf_sub64(LV(KID(node,0)), LV(KID(node,1))));
        break;
    }
    case 132: { // i64: Mul(LoadLongConst, LoadLongConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, jf_mul64(LV(KID(node,0)), LV(KID(node,1))));
        break;
    }
    case 133: { // i64: And(LoadLongConst, LoadLongConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, LV(KID(node,0)) & LV(KID(node,1)));
        break;
    }
    case 134: { // i64: Or(LoadLongConst, LoadLongConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, LV(KID(node,0)) | LV(KID(node,1)));
        break;
    }
    case 135: { // i64: Xor(LoadLongConst, LoadLongConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, LV(KID(node,0)) ^ LV(KID(node,1)));
        break;
    }
    case 136: { // i64: Shl(LoadLongConst, LoadLongConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, jf_shl64(LV(KID(node,0)), LV(KID(node,1))));
        break;
    }
    case 137: { // i64: Shr(LoadLongConst, LoadLongConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, jf_shr64(LV(KID(node,0)), LV(KID(node,1))));
        break;
    }
    case 138: { // i64: Ushr(LoadLongConst, LoadLongConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, jf_ushr64(LV(KID(node,0)), LV(KID(node,1))));
        break;
    }
    case 139: { // i64: Neg(LoadLongConst)
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, jf_sub64(0, LV(KID(node,0))));
        break;
    }
    case 140: { // i64: Div(LoadLongConst, LoadLongConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, LV(KID(node,0)) / LV(KID(node,1)));
        break;
    }
    case 141: { // i64: Rem(LoadLongConst, LoadLongConst)
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, LV(KID(node,0)) % LV(KID(node,1)));
        break;
    }
    case 142: { // i32: Mul(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, jf_ctz32(CV(KID(node,1)))); ew_emit(E, WOP_I32_SHL);
        break;
    }
    case 143: { // i32: Mul(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, jf_ctz32(CV(KID(node,0)))); ew_emit(E, WOP_I32_SHL);
        break;
    }
    case 144: { // i64: Mul(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, jf_ctz64(LV(KID(node,1)))); ew_emit(E, WOP_I64_SHL);
        break;
    }
    case 145: { // i64: Mul(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, jf_ctz64(LV(KID(node,0)))); ew_emit(E, WOP_I64_SHL);
        break;
    }
    case 146: { // i32: Div(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, jf_ctz32(CV(KID(node,1)))); ew_emit(E, WOP_I32_SHR_S);
        break;
    }
    case 147: { // cond: i32
        burg_reduce(node, state, 2, ctx);
        break;
    }
    case 148: { // ncond: i32
        burg_reduce(node, state, 2, ctx);
        ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 149: { // cond: LogNot(ncond)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 150: { // ncond: LogNot(cond)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 151: { // cond: Ne(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 152: { // cond: Ne(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 153: { // ncond: Ne(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 154: { // ncond: Ne(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 155: { // cond: Eq(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 156: { // cond: Eq(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 157: { // ncond: Eq(i32, LoadConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 158: { // ncond: Eq(LoadConst, i32)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 159: { // cond: Eq(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ);
        break;
    }
    case 160: { // cond: Eq(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ);
        break;
    }
    case 161: { // ncond: Eq(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 162: { // ncond: Eq(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 163: { // cond: Ne(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 164: { // cond: Ne(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 165: { // ncond: Ne(i64, LoadLongConst)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ);
        break;
    }
    case 166: { // ncond: Ne(LoadLongConst, i64)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EQZ);
        break;
    }
    case 167: { // cond: Eq(ref, LoadNull)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL);
        break;
    }
    case 168: { // cond: Eq(LoadNull, ref)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL);
        break;
    }
    case 169: { // ncond: Eq(ref, LoadNull)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 170: { // ncond: Eq(LoadNull, ref)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 171: { // cond: Ne(ref, LoadNull)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 172: { // cond: Ne(LoadNull, ref)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL); ew_emit(E, WOP_I32_EQZ);
        break;
    }
    case 173: { // ncond: Ne(ref, LoadNull)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL);
        break;
    }
    case 174: { // ncond: Ne(LoadNull, ref)
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_REF_IS_NULL);
        break;
    }
    case 175: { // i64: I2L(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_EXTEND_I32_S);
        break;
    }
    case 176: { // f32: I2F(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_CONVERT_I32_S);
        break;
    }
    case 177: { // f64: I2D(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CONVERT_I32_S);
        break;
    }
    case 178: { // i32: L2I(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_WRAP_I64);
        break;
    }
    case 179: { // f32: L2F(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_CONVERT_I64_S);
        break;
    }
    case 180: { // f64: L2D(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CONVERT_I64_S);
        break;
    }
    case 181: { // f64: F2D(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_PROMOTE_F32);
        break;
    }
    case 182: { // f32: D2F(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_DEMOTE_F64);
        break;
    }
    case 183: { // i32: F2I(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_TRUNC_SAT_F32_S);
        break;
    }
    case 184: { // i32: D2I(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_TRUNC_SAT_F64_S);
        break;
    }
    case 185: { // i64: F2L(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_TRUNC_SAT_F32_S);
        break;
    }
    case 186: { // i64: D2L(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_TRUNC_SAT_F64_S);
        break;
    }
    case 187: { // i32: MoveF2I(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_REINTERPRET_F32);
        break;
    }
    case 188: { // f32: MoveI2F(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_REINTERPRET_I32);
        break;
    }
    case 189: { // i64: MoveD2L(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_REINTERPRET_F64);
        break;
    }
    case 190: { // f64: MoveL2D(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_REINTERPRET_I64);
        break;
    }
    case 191: { // f64: F64Sqrt(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_SQRT);
        break;
    }
    case 192: { // f64: F64Floor(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_FLOOR);
        break;
    }
    case 193: { // f64: F64Ceil(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CEIL);
        break;
    }
    case 194: { // f64: F64Nearest(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_NEAREST);
        break;
    }
    case 195: { // i32: I2B(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EXTEND8_S);
        break;
    }
    case 196: { // i32: I2S(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EXTEND16_S);
        break;
    }
    case 197: { // i32: I2C(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, 0xFFFF); ew_emit(E, WOP_I32_AND);
        break;
    }
    case 198: { // i32: S2B(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_EXTEND8_S);
        break;
    }
    case 199: { // i32: S2I(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        /* short→int: both i32, no-op (the operand already an i32) */
        break;
    }
    case 200: { // stmt: Return(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 201: { // stmt: Return(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 202: { // stmt: Return(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 203: { // stmt: Return(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 204: { // stmt: Return(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 205: { // stmt: Return(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 206: { // stmt: ReturnVoid
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN);
        break;
    }
    case 207: { // stmt: Return(tail)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        /* the tail invoke already emitted return_call* and returned */
        break;
    }
    case 208: { // stmt: Throw(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_THROW); ew_u32(E, 0);
        break;
    }
    case 209: { // stmt: StoreLocal(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 210: { // stmt: StoreLocal(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 211: { // stmt: StoreLocal(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 212: { // stmt: StoreLocal(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 213: { // stmt: StoreLocal(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 214: { // stmt: StoreLocal(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->store_local.slot); cg_jump(E, node);
        break;
    }
    case 215: { // v128: SimdBin(v128, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_bin.op);
        break;
    }
    case 216: { // v128: SimdBin(v128, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_bin.op);
        break;
    }
    case 217: { // v128: SimdUn(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_un.op);
        break;
    }
    case 218: { // v128: SimdUn(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_un.op);
        break;
    }
    case 219: { // v128: SimdShift(v128, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_shift.op);
        break;
    }
    case 220: { // v128: SimdShift(v128, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_shift.op);
        break;
    }
    case 221: { // v128: SimdTern(v128, v128, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_tern.op);
        break;
    }
    case 222: { // v128: SimdTern(v128, v128, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_tern.op);
        break;
    }
    case 223: { // i32: SimdTestI(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_test_i.op);
        break;
    }
    case 224: { // i32: SimdTestI(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_test_i.op);
        break;
    }
    case 225: { // v128: SimdSplatI(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_i.op);
        break;
    }
    case 226: { // v128: SimdSplatI(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_i.op);
        break;
    }
    case 227: { // v128: SimdSplatL(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_l.op);
        break;
    }
    case 228: { // v128: SimdSplatL(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_l.op);
        break;
    }
    case 229: { // v128: SimdSplatF(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_f.op);
        break;
    }
    case 230: { // v128: SimdSplatF(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_f.op);
        break;
    }
    case 231: { // v128: SimdSplatD(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_d.op);
        break;
    }
    case 232: { // v128: SimdSplatD(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_splat_d.op);
        break;
    }
    case 233: { // i32: SimdExtractI(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_i.op); ew_byte(E, (uint8_t)node->simd_extract_i.lane);
        break;
    }
    case 234: { // i32: SimdExtractI(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_i.op); ew_byte(E, (uint8_t)node->simd_extract_i.lane);
        break;
    }
    case 235: { // i64: SimdExtractL(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_l.op); ew_byte(E, (uint8_t)node->simd_extract_l.lane);
        break;
    }
    case 236: { // i64: SimdExtractL(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_l.op); ew_byte(E, (uint8_t)node->simd_extract_l.lane);
        break;
    }
    case 237: { // f32: SimdExtractF(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_f.op); ew_byte(E, (uint8_t)node->simd_extract_f.lane);
        break;
    }
    case 238: { // f32: SimdExtractF(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_f.op); ew_byte(E, (uint8_t)node->simd_extract_f.lane);
        break;
    }
    case 239: { // f64: SimdExtractD(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_d.op); ew_byte(E, (uint8_t)node->simd_extract_d.lane);
        break;
    }
    case 240: { // f64: SimdExtractD(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_extract_d.op); ew_byte(E, (uint8_t)node->simd_extract_d.lane);
        break;
    }
    case 241: { // v128: SimdReplaceI(v128, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_i.op); ew_byte(E, (uint8_t)node->simd_replace_i.lane);
        break;
    }
    case 242: { // v128: SimdReplaceI(v128, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_i.op); ew_byte(E, (uint8_t)node->simd_replace_i.lane);
        break;
    }
    case 243: { // v128: SimdReplaceL(v128, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_l.op); ew_byte(E, (uint8_t)node->simd_replace_l.lane);
        break;
    }
    case 244: { // v128: SimdReplaceL(v128, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_l.op); ew_byte(E, (uint8_t)node->simd_replace_l.lane);
        break;
    }
    case 245: { // v128: SimdReplaceF(v128, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_f.op); ew_byte(E, (uint8_t)node->simd_replace_f.lane);
        break;
    }
    case 246: { // v128: SimdReplaceF(v128, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_f.op); ew_byte(E, (uint8_t)node->simd_replace_f.lane);
        break;
    }
    case 247: { // v128: SimdReplaceD(v128, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_d.op); ew_byte(E, (uint8_t)node->simd_replace_d.lane);
        break;
    }
    case 248: { // v128: SimdReplaceD(v128, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_replace_d.op); ew_byte(E, (uint8_t)node->simd_replace_d.lane);
        break;
    }
    case 249: { // v128: SimdConst
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_V128_CONST);
                      for (int b = 0; b < 8; b++) ew_byte(E, (uint8_t)((uint64_t)node->simd_const.lo >> (8*b)));
                      for (int b = 0; b < 8; b++) ew_byte(E, (uint8_t)((uint64_t)node->simd_const.hi >> (8*b)));
        break;
    }
    case 250: { // v128: SimdShuffle(v128, v128)
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
    case 251: { // v128: SimdMemLoad(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_load.op); ew_u32(E, (uint32_t)node->simd_mem_load.align); ew_u32(E, 0);
        break;
    }
    case 252: { // v128: SimdMemLoad(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_load.op); ew_u32(E, (uint32_t)node->simd_mem_load.align); ew_u32(E, 0);
        break;
    }
    case 253: { // v128: SimdMemLoadLane(i32, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_load_lane.op); ew_u32(E, (uint32_t)node->simd_mem_load_lane.align); ew_u32(E, 0); ew_byte(E, (uint8_t)node->simd_mem_load_lane.lane);
        break;
    }
    case 254: { // v128: SimdMemLoadLane(i32, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_load_lane.op); ew_u32(E, (uint32_t)node->simd_mem_load_lane.align); ew_u32(E, 0); ew_byte(E, (uint8_t)node->simd_mem_load_lane.lane);
        break;
    }
    case 255: { // i32: MemLoadI(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_load_i.op); ew_u32(E, (uint32_t)node->mem_load_i.align); ew_u32(E, 0);
        break;
    }
    case 256: { // i64: MemLoadL(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_load_l.op); ew_u32(E, (uint32_t)node->mem_load_l.align); ew_u32(E, 0);
        break;
    }
    case 257: { // f32: MemLoadF(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_load_f.op); ew_u32(E, (uint32_t)node->mem_load_f.align); ew_u32(E, 0);
        break;
    }
    case 258: { // f64: MemLoadD(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_load_d.op); ew_u32(E, (uint32_t)node->mem_load_d.align); ew_u32(E, 0);
        break;
    }
    case 259: { // i32: MemSize
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_MEMORY_SIZE); ew_u32(E, 0);
        break;
    }
    case 260: { // i32: MemGrow(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_MEMORY_GROW); ew_u32(E, 0);
        break;
    }
    case 261: { // stmt: Inc(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, node->inc.delta); ew_emit(E, WOP_I32_ADD); CG_NARROW(E, node->inc.data_type); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 262: { // stmt: Inc(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, node->inc.delta); ew_emit(E, WOP_I32_ADD); CG_NARROW(E, node->inc.data_type); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 263: { // stmt: Inc(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I32_CONST); ew_i32(E, node->inc.delta); ew_emit(E, WOP_I32_ADD); CG_NARROW(E, node->inc.data_type); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 264: { // stmt: Inc(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_I64_CONST); ew_i64(E, (int64_t)node->inc.delta); ew_emit(E, WOP_I64_ADD); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 265: { // stmt: Inc(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F32_CONST); ew_f32(E, (float)node->inc.delta); ew_emit(E, WOP_F32_ADD); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 266: { // stmt: Inc(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_F64_CONST); ew_f64(E, (double)node->inc.delta); ew_emit(E, WOP_F64_ADD); ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->inc.slot); cg_jump(E, node);
        break;
    }
    case 267: { // stmt: ExprEffect(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        cg_jump(E, node);
        break;
    }
    case 268: { // stmt: ExprEffect(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        cg_jump(E, node);
        break;
    }
    case 269: { // stmt: ExprEffect(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        cg_jump(E, node);
        break;
    }
    case 270: { // stmt: ExprEffect(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        cg_jump(E, node);
        break;
    }
    case 271: { // stmt: ExprEffect(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        cg_jump(E, node);
        break;
    }
    case 272: { // stmt: ExprEffect(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        cg_jump(E, node);
        break;
    }
    case 273: { // stmt: ExprEffect(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 274: { // stmt: ExprEffect(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 275: { // stmt: ExprEffect(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 276: { // stmt: ExprEffect(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 277: { // stmt: ExprEffect(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 278: { // stmt: ExprEffect(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_DROP); cg_jump(E, node);
        break;
    }
    case 279: { // ref: New
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wasm_types_emit_new(ctx->types, E, node->new_.class_id);
        break;
    }
    case 280: { // i32: ClassInstantiable(ref)
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
    case 281: { // ref: ClassConstruct(ref)
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
    case 282: { // ref: CloneCopy
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wasm_types_emit_clone_copy(ctx->types, E, node->clone_copy.class_id);
        break;
    }
    case 283: { // i32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET);   ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 284: { // i32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET_S); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 285: { // i32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET_U); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 286: { // i64: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 287: { // f32: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 288: { // f64: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 289: { // ref: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 290: { // v128: GetField(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_GET); ew_u32(E, struct_idx(node->get_field.class_id)); ew_u32(E, field_abs(node->get_field.class_id, node->get_field.field_idx));
        break;
    }
    case 291: { // stmt: PutField(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 292: { // stmt: PutField(ref, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 293: { // stmt: PutField(ref, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 294: { // stmt: PutField(ref, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 295: { // stmt: PutField(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 296: { // stmt: PutField(ref, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->put_field.class_id)); ew_u32(E, field_abs(node->put_field.class_id, node->put_field.field_idx)); cg_jump(E, node);
        break;
    }
    case 297: { // stmt: SetHeader(ref, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_STRUCT_SET); ew_u32(E, struct_idx(node->set_header.struct_class_id)); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 298: { // i32: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 299: { // i64: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 300: { // f32: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 301: { // f64: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 302: { // ref: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 303: { // v128: GetStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_GET); ew_u32(E, global_abs(node->get_static.class_id, node->get_static.field_idx));
        break;
    }
    case 304: { // stmt: PutStatic(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 305: { // stmt: PutStatic(i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 306: { // stmt: PutStatic(f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 307: { // stmt: PutStatic(f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 308: { // stmt: PutStatic(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 309: { // stmt: PutStatic(v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_GLOBAL_SET); ew_u32(E, global_abs(node->put_static.class_id, node->put_static.field_idx)); cg_jump(E, node);
        break;
    }
    case 310: { // stmt: SimdMemStore(i32, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_store.op); ew_u32(E, (uint32_t)node->simd_mem_store.align); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 311: { // stmt: SimdMemStore(i32, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_store.op); ew_u32(E, (uint32_t)node->simd_mem_store.align); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 312: { // stmt: SimdMemStoreLane(i32, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_store_lane.op); ew_u32(E, (uint32_t)node->simd_mem_store_lane.align); ew_u32(E, 0); ew_byte(E, (uint8_t)node->simd_mem_store_lane.lane); cg_jump(E, node);
        break;
    }
    case 313: { // stmt: SimdMemStoreLane(i32, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->simd_mem_store_lane.op); ew_u32(E, (uint32_t)node->simd_mem_store_lane.align); ew_u32(E, 0); ew_byte(E, (uint8_t)node->simd_mem_store_lane.lane); cg_jump(E, node);
        break;
    }
    case 314: { // stmt: MemStoreI(i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_store_i.op); ew_u32(E, (uint32_t)node->mem_store_i.align); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 315: { // stmt: MemStoreL(i32, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_store_l.op); ew_u32(E, (uint32_t)node->mem_store_l.align); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 316: { // stmt: MemStoreF(i32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_store_f.op); ew_u32(E, (uint32_t)node->mem_store_f.align); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 317: { // stmt: MemStoreD(i32, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, (wasm_op_t)node->mem_store_d.op); ew_u32(E, (uint32_t)node->mem_store_d.align); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 318: { // stmt: MemFill(i32, i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_MEMORY_FILL); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 319: { // stmt: MemCopy(i32, i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_MEMORY_COPY); ew_u32(E, 0); ew_u32(E, 0); cg_jump(E, node);
        break;
    }
    case 320: { // ref: NewArray(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_NEW_DEFAULT); ew_u32(E, wasm_types_array_for_atype(ctx->types, node->new_array.elem_type));
        break;
    }
    case 321: { // ref: ArrayNewData
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
    case 322: { // i32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET);   ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 323: { // i32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET_S); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 324: { // i32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET_U); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 325: { // i64: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 326: { // f32: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 327: { // f64: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 328: { // v128: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_load.data_type));
        break;
    }
    case 329: { // stmt: ArrayStore(ref, i32, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 330: { // stmt: ArrayStore(ref, i32, i64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 331: { // stmt: ArrayStore(ref, i32, f32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 4, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 332: { // stmt: ArrayStore(ref, i32, f64)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 5, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 333: { // stmt: ArrayStore(ref, i32, v128)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, node->array_store.data_type)); cg_jump(E, node);
        break;
    }
    case 334: { // i32: ArrayLength(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_LEN);
        break;
    }
    case 335: { // ref: NewRefArray(i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_NEW_DEFAULT); ew_u32(E, wasm_types_array_for_dt(ctx->types, SIR_DTREF));
        break;
    }
    case 336: { // ref: ArrayLoad(ref, i32)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_GET); ew_u32(E, wasm_types_array_for_dt(ctx->types, SIR_DTREF)); ew_emit(E, WOP_REF_CAST_NULL); ew_i32(E, wasm_types_ref_typeidx(ctx->types, node->array_load.elem_ref));
        break;
    }
    case 337: { // stmt: ArrayStore(ref, i32, ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_ARRAY_SET); ew_u32(E, wasm_types_array_for_dt(ctx->types, SIR_DTREF)); cg_jump(E, node);
        break;
    }
    case 338: { // stmt: ArrayCopy(ref, i32, ref, i32, i32)
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
    case 339: { // i32: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 340: { // i64: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 341: { // f32: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 342: { // f64: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 343: { // ref: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 344: { // v128: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 345: { // i32: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 346: { // i64: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 347: { // f32: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 348: { // f64: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 349: { // ref: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 350: { // v128: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 351: { // i32: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 352: { // i64: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 353: { // f32: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 354: { // f64: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 355: { // ref: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 356: { // v128: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        VCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 357: { // i32: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 358: { // i64: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 359: { // f32: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 360: { // f64: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 361: { // ref: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 362: { // v128: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        IVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 363: { // tail: InvokeStatic
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_static.class_id, node->invoke_static.method_idx));
        break;
    }
    case 364: { // tail: InvokeSpecial(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_RETURN_CALL); ew_u32(E, wasm_func_index(ctx->types, node->invoke_special.class_id, node->invoke_special.method_idx));
        break;
    }
    case 365: { // tail: InvokeVirtual(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        TVCALL(node->invoke_virtual.obj, node->invoke_virtual.class_id, node->invoke_virtual.method_idx);
        break;
    }
    case 366: { // tail: InvokeInterface(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        TIVCALL(node->invoke_interface.obj, node->invoke_interface.class_id, node->invoke_interface.method_idx);
        break;
    }
    case 367: { // i32: InstanceOf(ref)
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
    case 368: { // ref: CheckCast(ref)
        burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        int _cc = node->check_cast.class_id;
    int _to = wasm_class_is_interface(ctx->types, _cc) ? wasm_root_class(ctx->types) : _cc;
    ew_emit(E, WOP_REF_CAST_NULL); ew_i32(E, struct_idx(_to));
        break;
    }
    case 369: { // stmt: ExceptionEntry
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        ew_emit(E, WOP_LOCAL_SET); ew_u32(E, node->exception_entry.local_slot);
    cg_jump(E, node);
        break;
    }
    case 370: { // stmt: Nop
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        /* label anchor — structurer owns the scope */
        break;
    }
    case 371: { // stmt: i32
        burg_reduce(node, state, 2, ctx);
        break;
    }
    case 372: { // stmt: i64
        burg_reduce(node, state, 3, ctx);
        break;
    }
    case 373: { // stmt: f32
        burg_reduce(node, state, 4, ctx);
        break;
    }
    case 374: { // stmt: f64
        burg_reduce(node, state, 5, ctx);
        break;
    }
    case 375: { // stmt: ref
        burg_reduce(node, state, 6, ctx);
        break;
    }
    case 376: { // stmt: v128
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
        if (!state->rule[1])
            burg_set_error("burg: start nonterminal has no rule at root", (int)BURG_NODE_OP(root), ctx);
        else
            burg_reduce(root, state, 1, ctx);
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

    /* Cover every node with the start nonterminal */
    {
        int _i, _n = bbq_vec_len(rpo);
        for (_i = 0; _i < _n; _i++) {
            burg_state_t* s = burg_cache_lookup((uint32_t)(uintptr_t)BURG_NODE_ID(rpo[_i]), ctx);
            if (!s || !s->rule[1])
                burg_set_error("burg: start nonterminal does not cover graph node", (int)BURG_NODE_OP(rpo[_i]), ctx);
            else
                burg_reduce(rpo[_i], s, 1, ctx);
        }
    }
    bbq_vec_free(rpo);
}

static const struct burg_pat_node_t burg_pat_1[] = {{1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_2[] = {{1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_3[] = {{1, (int)BURG_LoadFloatConst, 0}};
static const struct burg_pat_node_t burg_pat_4[] = {{1, (int)BURG_LoadDoubleConst, 0}};
static const struct burg_pat_node_t burg_pat_5[] = {{1, (int)BURG_LoadLocal, 0}};
static const struct burg_pat_node_t burg_pat_6[] = {{1, (int)BURG_LoadLocal, 0}};
static const struct burg_pat_node_t burg_pat_7[] = {{1, (int)BURG_LoadLocal, 0}};
static const struct burg_pat_node_t burg_pat_8[] = {{1, (int)BURG_LoadLocal, 0}};
static const struct burg_pat_node_t burg_pat_9[] = {{1, (int)BURG_LoadLocal, 0}};
static const struct burg_pat_node_t burg_pat_10[] = {{1, (int)BURG_LoadLocal, 0}};
static const struct burg_pat_node_t burg_pat_11[] = {{1, (int)BURG_LoadThis, 0}};
static const struct burg_pat_node_t burg_pat_12[] = {{1, (int)BURG_LoadNull, 0}};
static const struct burg_pat_node_t burg_pat_13[] = {{1, (int)BURG_LoadClass, 0}};
static const struct burg_pat_node_t burg_pat_14[] = {{1, (int)BURG_Add, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_15[] = {{1, (int)BURG_Sub, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_16[] = {{1, (int)BURG_Mul, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_17[] = {{1, (int)BURG_Div, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_18[] = {{1, (int)BURG_Rem, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_19[] = {{1, (int)BURG_And, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_20[] = {{1, (int)BURG_Or, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_21[] = {{1, (int)BURG_Xor, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_22[] = {{1, (int)BURG_Shl, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_23[] = {{1, (int)BURG_Shr, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_24[] = {{1, (int)BURG_Ushr, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_25[] = {{1, (int)BURG_Neg, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_26[] = {{1, (int)BURG_LogNot, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_27[] = {{1, (int)BURG_Add, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_28[] = {{1, (int)BURG_Sub, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_29[] = {{1, (int)BURG_Mul, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_30[] = {{1, (int)BURG_Div, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_31[] = {{1, (int)BURG_Rem, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_32[] = {{1, (int)BURG_And, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_33[] = {{1, (int)BURG_Or, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_34[] = {{1, (int)BURG_Xor, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_35[] = {{1, (int)BURG_Shl, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_36[] = {{1, (int)BURG_Shr, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_37[] = {{1, (int)BURG_Ushr, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_38[] = {{1, (int)BURG_Neg, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_39[] = {{1, (int)BURG_Add, 2}, {0, (int)4, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_40[] = {{1, (int)BURG_Sub, 2}, {0, (int)4, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_41[] = {{1, (int)BURG_Mul, 2}, {0, (int)4, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_42[] = {{1, (int)BURG_Div, 2}, {0, (int)4, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_43[] = {{1, (int)BURG_Neg, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_44[] = {{1, (int)BURG_Add, 2}, {0, (int)5, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_45[] = {{1, (int)BURG_Sub, 2}, {0, (int)5, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_46[] = {{1, (int)BURG_Mul, 2}, {0, (int)5, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_47[] = {{1, (int)BURG_Div, 2}, {0, (int)5, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_48[] = {{1, (int)BURG_Neg, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_49[] = {{1, (int)BURG_Eq, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_50[] = {{1, (int)BURG_Ne, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_51[] = {{1, (int)BURG_Lt, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_52[] = {{1, (int)BURG_Le, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_53[] = {{1, (int)BURG_Gt, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_54[] = {{1, (int)BURG_Ge, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_55[] = {{1, (int)BURG_GeU, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_56[] = {{1, (int)BURG_Eq, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_57[] = {{1, (int)BURG_Ne, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_58[] = {{1, (int)BURG_Lt, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_59[] = {{1, (int)BURG_Le, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_60[] = {{1, (int)BURG_Gt, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_61[] = {{1, (int)BURG_Ge, 2}, {0, (int)3, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_62[] = {{1, (int)BURG_Eq, 2}, {0, (int)4, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_63[] = {{1, (int)BURG_Ne, 2}, {0, (int)4, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_64[] = {{1, (int)BURG_Lt, 2}, {0, (int)4, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_65[] = {{1, (int)BURG_Le, 2}, {0, (int)4, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_66[] = {{1, (int)BURG_Gt, 2}, {0, (int)4, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_67[] = {{1, (int)BURG_Ge, 2}, {0, (int)4, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_68[] = {{1, (int)BURG_Eq, 2}, {0, (int)5, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_69[] = {{1, (int)BURG_Ne, 2}, {0, (int)5, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_70[] = {{1, (int)BURG_Lt, 2}, {0, (int)5, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_71[] = {{1, (int)BURG_Le, 2}, {0, (int)5, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_72[] = {{1, (int)BURG_Gt, 2}, {0, (int)5, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_73[] = {{1, (int)BURG_Ge, 2}, {0, (int)5, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_74[] = {{1, (int)BURG_Eq, 2}, {0, (int)6, 0}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_75[] = {{1, (int)BURG_Ne, 2}, {0, (int)6, 0}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_76[] = {{1, (int)BURG_Eq, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_77[] = {{1, (int)BURG_Eq, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_78[] = {{1, (int)BURG_Ne, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_79[] = {{1, (int)BURG_Ne, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_80[] = {{1, (int)BURG_Eq, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_81[] = {{1, (int)BURG_Eq, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_82[] = {{1, (int)BURG_Ne, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_83[] = {{1, (int)BURG_Ne, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_84[] = {{1, (int)BURG_Eq, 2}, {0, (int)6, 0}, {1, (int)BURG_LoadNull, 0}};
static const struct burg_pat_node_t burg_pat_85[] = {{1, (int)BURG_Eq, 2}, {1, (int)BURG_LoadNull, 0}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_86[] = {{1, (int)BURG_Ne, 2}, {0, (int)6, 0}, {1, (int)BURG_LoadNull, 0}};
static const struct burg_pat_node_t burg_pat_87[] = {{1, (int)BURG_Ne, 2}, {1, (int)BURG_LoadNull, 0}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_88[] = {{1, (int)BURG_Add, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_89[] = {{1, (int)BURG_Add, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_90[] = {{1, (int)BURG_Sub, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_91[] = {{1, (int)BURG_Mul, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_92[] = {{1, (int)BURG_Mul, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_93[] = {{1, (int)BURG_Div, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_94[] = {{1, (int)BURG_Or, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_95[] = {{1, (int)BURG_Or, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_96[] = {{1, (int)BURG_Xor, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_97[] = {{1, (int)BURG_Xor, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_98[] = {{1, (int)BURG_And, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_99[] = {{1, (int)BURG_And, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_100[] = {{1, (int)BURG_Shl, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_101[] = {{1, (int)BURG_Shr, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_102[] = {{1, (int)BURG_Ushr, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_103[] = {{1, (int)BURG_Add, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_104[] = {{1, (int)BURG_Add, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_105[] = {{1, (int)BURG_Sub, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_106[] = {{1, (int)BURG_Mul, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_107[] = {{1, (int)BURG_Mul, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_108[] = {{1, (int)BURG_Div, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_109[] = {{1, (int)BURG_Or, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_110[] = {{1, (int)BURG_Or, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_111[] = {{1, (int)BURG_Xor, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_112[] = {{1, (int)BURG_Xor, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_113[] = {{1, (int)BURG_And, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_114[] = {{1, (int)BURG_And, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_115[] = {{1, (int)BURG_Shl, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_116[] = {{1, (int)BURG_Shr, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_117[] = {{1, (int)BURG_Ushr, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_118[] = {{1, (int)BURG_Add, 2}, {1, (int)BURG_LoadConst, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_119[] = {{1, (int)BURG_Sub, 2}, {1, (int)BURG_LoadConst, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_120[] = {{1, (int)BURG_Mul, 2}, {1, (int)BURG_LoadConst, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_121[] = {{1, (int)BURG_And, 2}, {1, (int)BURG_LoadConst, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_122[] = {{1, (int)BURG_Or, 2}, {1, (int)BURG_LoadConst, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_123[] = {{1, (int)BURG_Xor, 2}, {1, (int)BURG_LoadConst, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_124[] = {{1, (int)BURG_Shl, 2}, {1, (int)BURG_LoadConst, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_125[] = {{1, (int)BURG_Shr, 2}, {1, (int)BURG_LoadConst, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_126[] = {{1, (int)BURG_Ushr, 2}, {1, (int)BURG_LoadConst, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_127[] = {{1, (int)BURG_Neg, 1}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_128[] = {{1, (int)BURG_Div, 2}, {1, (int)BURG_LoadConst, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_129[] = {{1, (int)BURG_Rem, 2}, {1, (int)BURG_LoadConst, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_130[] = {{1, (int)BURG_Add, 2}, {1, (int)BURG_LoadLongConst, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_131[] = {{1, (int)BURG_Sub, 2}, {1, (int)BURG_LoadLongConst, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_132[] = {{1, (int)BURG_Mul, 2}, {1, (int)BURG_LoadLongConst, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_133[] = {{1, (int)BURG_And, 2}, {1, (int)BURG_LoadLongConst, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_134[] = {{1, (int)BURG_Or, 2}, {1, (int)BURG_LoadLongConst, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_135[] = {{1, (int)BURG_Xor, 2}, {1, (int)BURG_LoadLongConst, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_136[] = {{1, (int)BURG_Shl, 2}, {1, (int)BURG_LoadLongConst, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_137[] = {{1, (int)BURG_Shr, 2}, {1, (int)BURG_LoadLongConst, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_138[] = {{1, (int)BURG_Ushr, 2}, {1, (int)BURG_LoadLongConst, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_139[] = {{1, (int)BURG_Neg, 1}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_140[] = {{1, (int)BURG_Div, 2}, {1, (int)BURG_LoadLongConst, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_141[] = {{1, (int)BURG_Rem, 2}, {1, (int)BURG_LoadLongConst, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_142[] = {{1, (int)BURG_Mul, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_143[] = {{1, (int)BURG_Mul, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_144[] = {{1, (int)BURG_Mul, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_145[] = {{1, (int)BURG_Mul, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_146[] = {{1, (int)BURG_Div, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_147[] = {{0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_148[] = {{0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_149[] = {{1, (int)BURG_LogNot, 1}, {0, (int)9, 0}};
static const struct burg_pat_node_t burg_pat_150[] = {{1, (int)BURG_LogNot, 1}, {0, (int)8, 0}};
static const struct burg_pat_node_t burg_pat_151[] = {{1, (int)BURG_Ne, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_152[] = {{1, (int)BURG_Ne, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_153[] = {{1, (int)BURG_Ne, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_154[] = {{1, (int)BURG_Ne, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_155[] = {{1, (int)BURG_Eq, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_156[] = {{1, (int)BURG_Eq, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_157[] = {{1, (int)BURG_Eq, 2}, {0, (int)2, 0}, {1, (int)BURG_LoadConst, 0}};
static const struct burg_pat_node_t burg_pat_158[] = {{1, (int)BURG_Eq, 2}, {1, (int)BURG_LoadConst, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_159[] = {{1, (int)BURG_Eq, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_160[] = {{1, (int)BURG_Eq, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_161[] = {{1, (int)BURG_Eq, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_162[] = {{1, (int)BURG_Eq, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_163[] = {{1, (int)BURG_Ne, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_164[] = {{1, (int)BURG_Ne, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_165[] = {{1, (int)BURG_Ne, 2}, {0, (int)3, 0}, {1, (int)BURG_LoadLongConst, 0}};
static const struct burg_pat_node_t burg_pat_166[] = {{1, (int)BURG_Ne, 2}, {1, (int)BURG_LoadLongConst, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_167[] = {{1, (int)BURG_Eq, 2}, {0, (int)6, 0}, {1, (int)BURG_LoadNull, 0}};
static const struct burg_pat_node_t burg_pat_168[] = {{1, (int)BURG_Eq, 2}, {1, (int)BURG_LoadNull, 0}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_169[] = {{1, (int)BURG_Eq, 2}, {0, (int)6, 0}, {1, (int)BURG_LoadNull, 0}};
static const struct burg_pat_node_t burg_pat_170[] = {{1, (int)BURG_Eq, 2}, {1, (int)BURG_LoadNull, 0}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_171[] = {{1, (int)BURG_Ne, 2}, {0, (int)6, 0}, {1, (int)BURG_LoadNull, 0}};
static const struct burg_pat_node_t burg_pat_172[] = {{1, (int)BURG_Ne, 2}, {1, (int)BURG_LoadNull, 0}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_173[] = {{1, (int)BURG_Ne, 2}, {0, (int)6, 0}, {1, (int)BURG_LoadNull, 0}};
static const struct burg_pat_node_t burg_pat_174[] = {{1, (int)BURG_Ne, 2}, {1, (int)BURG_LoadNull, 0}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_175[] = {{1, (int)BURG_I2L, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_176[] = {{1, (int)BURG_I2F, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_177[] = {{1, (int)BURG_I2D, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_178[] = {{1, (int)BURG_L2I, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_179[] = {{1, (int)BURG_L2F, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_180[] = {{1, (int)BURG_L2D, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_181[] = {{1, (int)BURG_F2D, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_182[] = {{1, (int)BURG_D2F, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_183[] = {{1, (int)BURG_F2I, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_184[] = {{1, (int)BURG_D2I, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_185[] = {{1, (int)BURG_F2L, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_186[] = {{1, (int)BURG_D2L, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_187[] = {{1, (int)BURG_MoveF2I, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_188[] = {{1, (int)BURG_MoveI2F, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_189[] = {{1, (int)BURG_MoveD2L, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_190[] = {{1, (int)BURG_MoveL2D, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_191[] = {{1, (int)BURG_F64Sqrt, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_192[] = {{1, (int)BURG_F64Floor, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_193[] = {{1, (int)BURG_F64Ceil, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_194[] = {{1, (int)BURG_F64Nearest, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_195[] = {{1, (int)BURG_I2B, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_196[] = {{1, (int)BURG_I2S, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_197[] = {{1, (int)BURG_I2C, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_198[] = {{1, (int)BURG_S2B, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_199[] = {{1, (int)BURG_S2I, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_200[] = {{1, (int)BURG_Return, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_201[] = {{1, (int)BURG_Return, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_202[] = {{1, (int)BURG_Return, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_203[] = {{1, (int)BURG_Return, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_204[] = {{1, (int)BURG_Return, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_205[] = {{1, (int)BURG_Return, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_206[] = {{1, (int)BURG_ReturnVoid, 0}};
static const struct burg_pat_node_t burg_pat_207[] = {{1, (int)BURG_Return, 1}, {0, (int)10, 0}};
static const struct burg_pat_node_t burg_pat_208[] = {{1, (int)BURG_Throw, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_209[] = {{1, (int)BURG_StoreLocal, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_210[] = {{1, (int)BURG_StoreLocal, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_211[] = {{1, (int)BURG_StoreLocal, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_212[] = {{1, (int)BURG_StoreLocal, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_213[] = {{1, (int)BURG_StoreLocal, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_214[] = {{1, (int)BURG_StoreLocal, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_215[] = {{1, (int)BURG_SimdBin, 2}, {0, (int)7, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_216[] = {{1, (int)BURG_SimdBin, 2}, {0, (int)7, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_217[] = {{1, (int)BURG_SimdUn, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_218[] = {{1, (int)BURG_SimdUn, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_219[] = {{1, (int)BURG_SimdShift, 2}, {0, (int)7, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_220[] = {{1, (int)BURG_SimdShift, 2}, {0, (int)7, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_221[] = {{1, (int)BURG_SimdTern, 3}, {0, (int)7, 0}, {0, (int)7, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_222[] = {{1, (int)BURG_SimdTern, 3}, {0, (int)7, 0}, {0, (int)7, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_223[] = {{1, (int)BURG_SimdTestI, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_224[] = {{1, (int)BURG_SimdTestI, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_225[] = {{1, (int)BURG_SimdSplatI, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_226[] = {{1, (int)BURG_SimdSplatI, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_227[] = {{1, (int)BURG_SimdSplatL, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_228[] = {{1, (int)BURG_SimdSplatL, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_229[] = {{1, (int)BURG_SimdSplatF, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_230[] = {{1, (int)BURG_SimdSplatF, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_231[] = {{1, (int)BURG_SimdSplatD, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_232[] = {{1, (int)BURG_SimdSplatD, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_233[] = {{1, (int)BURG_SimdExtractI, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_234[] = {{1, (int)BURG_SimdExtractI, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_235[] = {{1, (int)BURG_SimdExtractL, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_236[] = {{1, (int)BURG_SimdExtractL, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_237[] = {{1, (int)BURG_SimdExtractF, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_238[] = {{1, (int)BURG_SimdExtractF, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_239[] = {{1, (int)BURG_SimdExtractD, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_240[] = {{1, (int)BURG_SimdExtractD, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_241[] = {{1, (int)BURG_SimdReplaceI, 2}, {0, (int)7, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_242[] = {{1, (int)BURG_SimdReplaceI, 2}, {0, (int)7, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_243[] = {{1, (int)BURG_SimdReplaceL, 2}, {0, (int)7, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_244[] = {{1, (int)BURG_SimdReplaceL, 2}, {0, (int)7, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_245[] = {{1, (int)BURG_SimdReplaceF, 2}, {0, (int)7, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_246[] = {{1, (int)BURG_SimdReplaceF, 2}, {0, (int)7, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_247[] = {{1, (int)BURG_SimdReplaceD, 2}, {0, (int)7, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_248[] = {{1, (int)BURG_SimdReplaceD, 2}, {0, (int)7, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_249[] = {{1, (int)BURG_SimdConst, 0}};
static const struct burg_pat_node_t burg_pat_250[] = {{1, (int)BURG_SimdShuffle, 2}, {0, (int)7, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_251[] = {{1, (int)BURG_SimdMemLoad, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_252[] = {{1, (int)BURG_SimdMemLoad, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_253[] = {{1, (int)BURG_SimdMemLoadLane, 2}, {0, (int)2, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_254[] = {{1, (int)BURG_SimdMemLoadLane, 2}, {0, (int)2, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_255[] = {{1, (int)BURG_MemLoadI, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_256[] = {{1, (int)BURG_MemLoadL, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_257[] = {{1, (int)BURG_MemLoadF, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_258[] = {{1, (int)BURG_MemLoadD, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_259[] = {{1, (int)BURG_MemSize, 0}};
static const struct burg_pat_node_t burg_pat_260[] = {{1, (int)BURG_MemGrow, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_261[] = {{1, (int)BURG_Inc, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_262[] = {{1, (int)BURG_Inc, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_263[] = {{1, (int)BURG_Inc, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_264[] = {{1, (int)BURG_Inc, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_265[] = {{1, (int)BURG_Inc, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_266[] = {{1, (int)BURG_Inc, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_267[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_268[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_269[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_270[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_271[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_272[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_273[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_274[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_275[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_276[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_277[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_278[] = {{1, (int)BURG_ExprEffect, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_279[] = {{1, (int)BURG_New, 0}};
static const struct burg_pat_node_t burg_pat_280[] = {{1, (int)BURG_ClassInstantiable, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_281[] = {{1, (int)BURG_ClassConstruct, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_282[] = {{1, (int)BURG_CloneCopy, 0}};
static const struct burg_pat_node_t burg_pat_283[] = {{1, (int)BURG_GetField, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_284[] = {{1, (int)BURG_GetField, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_285[] = {{1, (int)BURG_GetField, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_286[] = {{1, (int)BURG_GetField, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_287[] = {{1, (int)BURG_GetField, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_288[] = {{1, (int)BURG_GetField, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_289[] = {{1, (int)BURG_GetField, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_290[] = {{1, (int)BURG_GetField, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_291[] = {{1, (int)BURG_PutField, 2}, {0, (int)6, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_292[] = {{1, (int)BURG_PutField, 2}, {0, (int)6, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_293[] = {{1, (int)BURG_PutField, 2}, {0, (int)6, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_294[] = {{1, (int)BURG_PutField, 2}, {0, (int)6, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_295[] = {{1, (int)BURG_PutField, 2}, {0, (int)6, 0}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_296[] = {{1, (int)BURG_PutField, 2}, {0, (int)6, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_297[] = {{1, (int)BURG_SetHeader, 2}, {0, (int)6, 0}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_298[] = {{1, (int)BURG_GetStatic, 0}};
static const struct burg_pat_node_t burg_pat_299[] = {{1, (int)BURG_GetStatic, 0}};
static const struct burg_pat_node_t burg_pat_300[] = {{1, (int)BURG_GetStatic, 0}};
static const struct burg_pat_node_t burg_pat_301[] = {{1, (int)BURG_GetStatic, 0}};
static const struct burg_pat_node_t burg_pat_302[] = {{1, (int)BURG_GetStatic, 0}};
static const struct burg_pat_node_t burg_pat_303[] = {{1, (int)BURG_GetStatic, 0}};
static const struct burg_pat_node_t burg_pat_304[] = {{1, (int)BURG_PutStatic, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_305[] = {{1, (int)BURG_PutStatic, 1}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_306[] = {{1, (int)BURG_PutStatic, 1}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_307[] = {{1, (int)BURG_PutStatic, 1}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_308[] = {{1, (int)BURG_PutStatic, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_309[] = {{1, (int)BURG_PutStatic, 1}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_310[] = {{1, (int)BURG_SimdMemStore, 2}, {0, (int)2, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_311[] = {{1, (int)BURG_SimdMemStore, 2}, {0, (int)2, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_312[] = {{1, (int)BURG_SimdMemStoreLane, 2}, {0, (int)2, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_313[] = {{1, (int)BURG_SimdMemStoreLane, 2}, {0, (int)2, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_314[] = {{1, (int)BURG_MemStoreI, 2}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_315[] = {{1, (int)BURG_MemStoreL, 2}, {0, (int)2, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_316[] = {{1, (int)BURG_MemStoreF, 2}, {0, (int)2, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_317[] = {{1, (int)BURG_MemStoreD, 2}, {0, (int)2, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_318[] = {{1, (int)BURG_MemFill, 3}, {0, (int)2, 0}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_319[] = {{1, (int)BURG_MemCopy, 3}, {0, (int)2, 0}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_320[] = {{1, (int)BURG_NewArray, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_321[] = {{1, (int)BURG_ArrayNewData, 0}};
static const struct burg_pat_node_t burg_pat_322[] = {{1, (int)BURG_ArrayLoad, 2}, {0, (int)6, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_323[] = {{1, (int)BURG_ArrayLoad, 2}, {0, (int)6, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_324[] = {{1, (int)BURG_ArrayLoad, 2}, {0, (int)6, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_325[] = {{1, (int)BURG_ArrayLoad, 2}, {0, (int)6, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_326[] = {{1, (int)BURG_ArrayLoad, 2}, {0, (int)6, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_327[] = {{1, (int)BURG_ArrayLoad, 2}, {0, (int)6, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_328[] = {{1, (int)BURG_ArrayLoad, 2}, {0, (int)6, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_329[] = {{1, (int)BURG_ArrayStore, 3}, {0, (int)6, 0}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_330[] = {{1, (int)BURG_ArrayStore, 3}, {0, (int)6, 0}, {0, (int)2, 0}, {0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_331[] = {{1, (int)BURG_ArrayStore, 3}, {0, (int)6, 0}, {0, (int)2, 0}, {0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_332[] = {{1, (int)BURG_ArrayStore, 3}, {0, (int)6, 0}, {0, (int)2, 0}, {0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_333[] = {{1, (int)BURG_ArrayStore, 3}, {0, (int)6, 0}, {0, (int)2, 0}, {0, (int)7, 0}};
static const struct burg_pat_node_t burg_pat_334[] = {{1, (int)BURG_ArrayLength, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_335[] = {{1, (int)BURG_NewRefArray, 1}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_336[] = {{1, (int)BURG_ArrayLoad, 2}, {0, (int)6, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_337[] = {{1, (int)BURG_ArrayStore, 3}, {0, (int)6, 0}, {0, (int)2, 0}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_338[] = {{1, (int)BURG_ArrayCopy, 5}, {0, (int)6, 0}, {0, (int)2, 0}, {0, (int)6, 0}, {0, (int)2, 0}, {0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_339[] = {{1, (int)BURG_InvokeStatic, 0}};
static const struct burg_pat_node_t burg_pat_340[] = {{1, (int)BURG_InvokeStatic, 0}};
static const struct burg_pat_node_t burg_pat_341[] = {{1, (int)BURG_InvokeStatic, 0}};
static const struct burg_pat_node_t burg_pat_342[] = {{1, (int)BURG_InvokeStatic, 0}};
static const struct burg_pat_node_t burg_pat_343[] = {{1, (int)BURG_InvokeStatic, 0}};
static const struct burg_pat_node_t burg_pat_344[] = {{1, (int)BURG_InvokeStatic, 0}};
static const struct burg_pat_node_t burg_pat_345[] = {{1, (int)BURG_InvokeSpecial, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_346[] = {{1, (int)BURG_InvokeSpecial, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_347[] = {{1, (int)BURG_InvokeSpecial, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_348[] = {{1, (int)BURG_InvokeSpecial, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_349[] = {{1, (int)BURG_InvokeSpecial, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_350[] = {{1, (int)BURG_InvokeSpecial, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_351[] = {{1, (int)BURG_InvokeVirtual, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_352[] = {{1, (int)BURG_InvokeVirtual, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_353[] = {{1, (int)BURG_InvokeVirtual, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_354[] = {{1, (int)BURG_InvokeVirtual, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_355[] = {{1, (int)BURG_InvokeVirtual, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_356[] = {{1, (int)BURG_InvokeVirtual, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_357[] = {{1, (int)BURG_InvokeInterface, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_358[] = {{1, (int)BURG_InvokeInterface, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_359[] = {{1, (int)BURG_InvokeInterface, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_360[] = {{1, (int)BURG_InvokeInterface, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_361[] = {{1, (int)BURG_InvokeInterface, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_362[] = {{1, (int)BURG_InvokeInterface, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_363[] = {{1, (int)BURG_InvokeStatic, 0}};
static const struct burg_pat_node_t burg_pat_364[] = {{1, (int)BURG_InvokeSpecial, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_365[] = {{1, (int)BURG_InvokeVirtual, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_366[] = {{1, (int)BURG_InvokeInterface, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_367[] = {{1, (int)BURG_InstanceOf, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_368[] = {{1, (int)BURG_CheckCast, 1}, {0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_369[] = {{1, (int)BURG_ExceptionEntry, 0}};
static const struct burg_pat_node_t burg_pat_370[] = {{1, (int)BURG_Nop, 0}};
static const struct burg_pat_node_t burg_pat_371[] = {{0, (int)2, 0}};
static const struct burg_pat_node_t burg_pat_372[] = {{0, (int)3, 0}};
static const struct burg_pat_node_t burg_pat_373[] = {{0, (int)4, 0}};
static const struct burg_pat_node_t burg_pat_374[] = {{0, (int)5, 0}};
static const struct burg_pat_node_t burg_pat_375[] = {{0, (int)6, 0}};
static const struct burg_pat_node_t burg_pat_376[] = {{0, (int)7, 0}};

const struct burg_rule_row_t burg_rule_table[] = {
    {1, 2, 2, 1, burg_pat_1, 0},  /* i32: LoadConst */
    {2, 3, 2, 1, burg_pat_2, 0},  /* i64: LoadLongConst */
    {3, 4, 5, 1, burg_pat_3, 0},  /* f32: LoadFloatConst */
    {4, 5, 9, 1, burg_pat_4, 0},  /* f64: LoadDoubleConst */
    {5, 2, 2, 1, burg_pat_5, 1},  /* i32: LoadLocal */
    {6, 3, 2, 1, burg_pat_6, 1},  /* i64: LoadLocal */
    {7, 4, 2, 1, burg_pat_7, 1},  /* f32: LoadLocal */
    {8, 5, 2, 1, burg_pat_8, 1},  /* f64: LoadLocal */
    {9, 6, 2, 1, burg_pat_9, 1},  /* ref: LoadLocal */
    {10, 7, 2, 1, burg_pat_10, 1},  /* v128: LoadLocal */
    {11, 6, 5, 1, burg_pat_11, 0},  /* ref: LoadThis */
    {12, 6, 2, 1, burg_pat_12, 0},  /* ref: LoadNull */
    {13, 6, 2, 1, burg_pat_13, 0},  /* ref: LoadClass */
    {14, 2, 1, 3, burg_pat_14, 0},  /* i32: Add(i32, i32) */
    {15, 2, 1, 3, burg_pat_15, 0},  /* i32: Sub(i32, i32) */
    {16, 2, 1, 3, burg_pat_16, 0},  /* i32: Mul(i32, i32) */
    {17, 2, 1, 3, burg_pat_17, 0},  /* i32: Div(i32, i32) */
    {18, 2, 1, 3, burg_pat_18, 0},  /* i32: Rem(i32, i32) */
    {19, 2, 1, 3, burg_pat_19, 0},  /* i32: And(i32, i32) */
    {20, 2, 1, 3, burg_pat_20, 0},  /* i32: Or(i32, i32) */
    {21, 2, 1, 3, burg_pat_21, 0},  /* i32: Xor(i32, i32) */
    {22, 2, 1, 3, burg_pat_22, 0},  /* i32: Shl(i32, i32) */
    {23, 2, 1, 3, burg_pat_23, 0},  /* i32: Shr(i32, i32) */
    {24, 2, 1, 3, burg_pat_24, 0},  /* i32: Ushr(i32, i32) */
    {25, 2, 3, 2, burg_pat_25, 0},  /* i32: Neg(i32) */
    {26, 2, 1, 2, burg_pat_26, 0},  /* i32: LogNot(i32) */
    {27, 3, 1, 3, burg_pat_27, 0},  /* i64: Add(i64, i64) */
    {28, 3, 1, 3, burg_pat_28, 0},  /* i64: Sub(i64, i64) */
    {29, 3, 1, 3, burg_pat_29, 0},  /* i64: Mul(i64, i64) */
    {30, 3, 1, 3, burg_pat_30, 0},  /* i64: Div(i64, i64) */
    {31, 3, 1, 3, burg_pat_31, 0},  /* i64: Rem(i64, i64) */
    {32, 3, 1, 3, burg_pat_32, 0},  /* i64: And(i64, i64) */
    {33, 3, 1, 3, burg_pat_33, 0},  /* i64: Or(i64, i64) */
    {34, 3, 1, 3, burg_pat_34, 0},  /* i64: Xor(i64, i64) */
    {35, 3, 1, 3, burg_pat_35, 0},  /* i64: Shl(i64, i64) */
    {36, 3, 1, 3, burg_pat_36, 0},  /* i64: Shr(i64, i64) */
    {37, 3, 1, 3, burg_pat_37, 0},  /* i64: Ushr(i64, i64) */
    {38, 3, 3, 2, burg_pat_38, 0},  /* i64: Neg(i64) */
    {39, 4, 1, 3, burg_pat_39, 0},  /* f32: Add(f32, f32) */
    {40, 4, 1, 3, burg_pat_40, 0},  /* f32: Sub(f32, f32) */
    {41, 4, 1, 3, burg_pat_41, 0},  /* f32: Mul(f32, f32) */
    {42, 4, 1, 3, burg_pat_42, 0},  /* f32: Div(f32, f32) */
    {43, 4, 1, 2, burg_pat_43, 0},  /* f32: Neg(f32) */
    {44, 5, 1, 3, burg_pat_44, 0},  /* f64: Add(f64, f64) */
    {45, 5, 1, 3, burg_pat_45, 0},  /* f64: Sub(f64, f64) */
    {46, 5, 1, 3, burg_pat_46, 0},  /* f64: Mul(f64, f64) */
    {47, 5, 1, 3, burg_pat_47, 0},  /* f64: Div(f64, f64) */
    {48, 5, 1, 2, burg_pat_48, 0},  /* f64: Neg(f64) */
    {49, 2, 1, 3, burg_pat_49, 0},  /* i32: Eq(i32, i32) */
    {50, 2, 1, 3, burg_pat_50, 0},  /* i32: Ne(i32, i32) */
    {51, 2, 1, 3, burg_pat_51, 0},  /* i32: Lt(i32, i32) */
    {52, 2, 1, 3, burg_pat_52, 0},  /* i32: Le(i32, i32) */
    {53, 2, 1, 3, burg_pat_53, 0},  /* i32: Gt(i32, i32) */
    {54, 2, 1, 3, burg_pat_54, 0},  /* i32: Ge(i32, i32) */
    {55, 2, 1, 3, burg_pat_55, 0},  /* i32: GeU(i32, i32) */
    {56, 2, 1, 3, burg_pat_56, 0},  /* i32: Eq(i64, i64) */
    {57, 2, 1, 3, burg_pat_57, 0},  /* i32: Ne(i64, i64) */
    {58, 2, 1, 3, burg_pat_58, 0},  /* i32: Lt(i64, i64) */
    {59, 2, 1, 3, burg_pat_59, 0},  /* i32: Le(i64, i64) */
    {60, 2, 1, 3, burg_pat_60, 0},  /* i32: Gt(i64, i64) */
    {61, 2, 1, 3, burg_pat_61, 0},  /* i32: Ge(i64, i64) */
    {62, 2, 1, 3, burg_pat_62, 0},  /* i32: Eq(f32, f32) */
    {63, 2, 1, 3, burg_pat_63, 0},  /* i32: Ne(f32, f32) */
    {64, 2, 1, 3, burg_pat_64, 0},  /* i32: Lt(f32, f32) */
    {65, 2, 1, 3, burg_pat_65, 0},  /* i32: Le(f32, f32) */
    {66, 2, 1, 3, burg_pat_66, 0},  /* i32: Gt(f32, f32) */
    {67, 2, 1, 3, burg_pat_67, 0},  /* i32: Ge(f32, f32) */
    {68, 2, 1, 3, burg_pat_68, 0},  /* i32: Eq(f64, f64) */
    {69, 2, 1, 3, burg_pat_69, 0},  /* i32: Ne(f64, f64) */
    {70, 2, 1, 3, burg_pat_70, 0},  /* i32: Lt(f64, f64) */
    {71, 2, 1, 3, burg_pat_71, 0},  /* i32: Le(f64, f64) */
    {72, 2, 1, 3, burg_pat_72, 0},  /* i32: Gt(f64, f64) */
    {73, 2, 1, 3, burg_pat_73, 0},  /* i32: Ge(f64, f64) */
    {74, 2, 1, 3, burg_pat_74, 0},  /* i32: Eq(ref, ref) */
    {75, 2, 2, 3, burg_pat_75, 0},  /* i32: Ne(ref, ref) */
    {76, 2, 1, 3, burg_pat_76, 1},  /* i32: Eq(i32, LoadConst) */
    {77, 2, 1, 3, burg_pat_77, 1},  /* i32: Eq(LoadConst, i32) */
    {78, 2, 2, 3, burg_pat_78, 1},  /* i32: Ne(i32, LoadConst) */
    {79, 2, 2, 3, burg_pat_79, 1},  /* i32: Ne(LoadConst, i32) */
    {80, 2, 1, 3, burg_pat_80, 1},  /* i32: Eq(i64, LoadLongConst) */
    {81, 2, 1, 3, burg_pat_81, 1},  /* i32: Eq(LoadLongConst, i64) */
    {82, 2, 2, 3, burg_pat_82, 1},  /* i32: Ne(i64, LoadLongConst) */
    {83, 2, 2, 3, burg_pat_83, 1},  /* i32: Ne(LoadLongConst, i64) */
    {84, 2, 1, 3, burg_pat_84, 0},  /* i32: Eq(ref, LoadNull) */
    {85, 2, 1, 3, burg_pat_85, 0},  /* i32: Eq(LoadNull, ref) */
    {86, 2, 2, 3, burg_pat_86, 0},  /* i32: Ne(ref, LoadNull) */
    {87, 2, 2, 3, burg_pat_87, 0},  /* i32: Ne(LoadNull, ref) */
    {88, 2, 0, 3, burg_pat_88, 1},  /* i32: Add(i32, LoadConst) */
    {89, 2, 0, 3, burg_pat_89, 1},  /* i32: Add(LoadConst, i32) */
    {90, 2, 0, 3, burg_pat_90, 1},  /* i32: Sub(i32, LoadConst) */
    {91, 2, 0, 3, burg_pat_91, 1},  /* i32: Mul(i32, LoadConst) */
    {92, 2, 0, 3, burg_pat_92, 1},  /* i32: Mul(LoadConst, i32) */
    {93, 2, 0, 3, burg_pat_93, 1},  /* i32: Div(i32, LoadConst) */
    {94, 2, 0, 3, burg_pat_94, 1},  /* i32: Or(i32, LoadConst) */
    {95, 2, 0, 3, burg_pat_95, 1},  /* i32: Or(LoadConst, i32) */
    {96, 2, 0, 3, burg_pat_96, 1},  /* i32: Xor(i32, LoadConst) */
    {97, 2, 0, 3, burg_pat_97, 1},  /* i32: Xor(LoadConst, i32) */
    {98, 2, 0, 3, burg_pat_98, 1},  /* i32: And(i32, LoadConst) */
    {99, 2, 0, 3, burg_pat_99, 1},  /* i32: And(LoadConst, i32) */
    {100, 2, 0, 3, burg_pat_100, 1},  /* i32: Shl(i32, LoadConst) */
    {101, 2, 0, 3, burg_pat_101, 1},  /* i32: Shr(i32, LoadConst) */
    {102, 2, 0, 3, burg_pat_102, 1},  /* i32: Ushr(i32, LoadConst) */
    {103, 3, 0, 3, burg_pat_103, 1},  /* i64: Add(i64, LoadLongConst) */
    {104, 3, 0, 3, burg_pat_104, 1},  /* i64: Add(LoadLongConst, i64) */
    {105, 3, 0, 3, burg_pat_105, 1},  /* i64: Sub(i64, LoadLongConst) */
    {106, 3, 0, 3, burg_pat_106, 1},  /* i64: Mul(i64, LoadLongConst) */
    {107, 3, 0, 3, burg_pat_107, 1},  /* i64: Mul(LoadLongConst, i64) */
    {108, 3, 0, 3, burg_pat_108, 1},  /* i64: Div(i64, LoadLongConst) */
    {109, 3, 0, 3, burg_pat_109, 1},  /* i64: Or(i64, LoadLongConst) */
    {110, 3, 0, 3, burg_pat_110, 1},  /* i64: Or(LoadLongConst, i64) */
    {111, 3, 0, 3, burg_pat_111, 1},  /* i64: Xor(i64, LoadLongConst) */
    {112, 3, 0, 3, burg_pat_112, 1},  /* i64: Xor(LoadLongConst, i64) */
    {113, 3, 0, 3, burg_pat_113, 1},  /* i64: And(i64, LoadLongConst) */
    {114, 3, 0, 3, burg_pat_114, 1},  /* i64: And(LoadLongConst, i64) */
    {115, 3, 0, 3, burg_pat_115, 1},  /* i64: Shl(i64, LoadLongConst) */
    {116, 3, 0, 3, burg_pat_116, 1},  /* i64: Shr(i64, LoadLongConst) */
    {117, 3, 0, 3, burg_pat_117, 1},  /* i64: Ushr(i64, LoadLongConst) */
    {118, 2, 2, 3, burg_pat_118, 0},  /* i32: Add(LoadConst, LoadConst) */
    {119, 2, 2, 3, burg_pat_119, 0},  /* i32: Sub(LoadConst, LoadConst) */
    {120, 2, 2, 3, burg_pat_120, 0},  /* i32: Mul(LoadConst, LoadConst) */
    {121, 2, 2, 3, burg_pat_121, 0},  /* i32: And(LoadConst, LoadConst) */
    {122, 2, 2, 3, burg_pat_122, 0},  /* i32: Or(LoadConst, LoadConst) */
    {123, 2, 2, 3, burg_pat_123, 0},  /* i32: Xor(LoadConst, LoadConst) */
    {124, 2, 2, 3, burg_pat_124, 0},  /* i32: Shl(LoadConst, LoadConst) */
    {125, 2, 2, 3, burg_pat_125, 0},  /* i32: Shr(LoadConst, LoadConst) */
    {126, 2, 2, 3, burg_pat_126, 0},  /* i32: Ushr(LoadConst, LoadConst) */
    {127, 2, 2, 2, burg_pat_127, 0},  /* i32: Neg(LoadConst) */
    {128, 2, 2, 3, burg_pat_128, 1},  /* i32: Div(LoadConst, LoadConst) */
    {129, 2, 2, 3, burg_pat_129, 1},  /* i32: Rem(LoadConst, LoadConst) */
    {130, 3, 2, 3, burg_pat_130, 0},  /* i64: Add(LoadLongConst, LoadLongConst) */
    {131, 3, 2, 3, burg_pat_131, 0},  /* i64: Sub(LoadLongConst, LoadLongConst) */
    {132, 3, 2, 3, burg_pat_132, 0},  /* i64: Mul(LoadLongConst, LoadLongConst) */
    {133, 3, 2, 3, burg_pat_133, 0},  /* i64: And(LoadLongConst, LoadLongConst) */
    {134, 3, 2, 3, burg_pat_134, 0},  /* i64: Or(LoadLongConst, LoadLongConst) */
    {135, 3, 2, 3, burg_pat_135, 0},  /* i64: Xor(LoadLongConst, LoadLongConst) */
    {136, 3, 2, 3, burg_pat_136, 0},  /* i64: Shl(LoadLongConst, LoadLongConst) */
    {137, 3, 2, 3, burg_pat_137, 0},  /* i64: Shr(LoadLongConst, LoadLongConst) */
    {138, 3, 2, 3, burg_pat_138, 0},  /* i64: Ushr(LoadLongConst, LoadLongConst) */
    {139, 3, 2, 2, burg_pat_139, 0},  /* i64: Neg(LoadLongConst) */
    {140, 3, 2, 3, burg_pat_140, 1},  /* i64: Div(LoadLongConst, LoadLongConst) */
    {141, 3, 2, 3, burg_pat_141, 1},  /* i64: Rem(LoadLongConst, LoadLongConst) */
    {142, 2, 2, 3, burg_pat_142, 1},  /* i32: Mul(i32, LoadConst) */
    {143, 2, 2, 3, burg_pat_143, 1},  /* i32: Mul(LoadConst, i32) */
    {144, 3, 2, 3, burg_pat_144, 1},  /* i64: Mul(i64, LoadLongConst) */
    {145, 3, 2, 3, burg_pat_145, 1},  /* i64: Mul(LoadLongConst, i64) */
    {146, 2, 2, 3, burg_pat_146, 1},  /* i32: Div(i32, LoadConst) */
    {147, 8, 0, 1, burg_pat_147, 0},  /* cond: i32 */
    {148, 9, 1, 1, burg_pat_148, 0},  /* ncond: i32 */
    {149, 8, 0, 2, burg_pat_149, 0},  /* cond: LogNot(ncond) */
    {150, 9, 0, 2, burg_pat_150, 0},  /* ncond: LogNot(cond) */
    {151, 8, 0, 3, burg_pat_151, 1},  /* cond: Ne(i32, LoadConst) */
    {152, 8, 0, 3, burg_pat_152, 1},  /* cond: Ne(LoadConst, i32) */
    {153, 9, 1, 3, burg_pat_153, 1},  /* ncond: Ne(i32, LoadConst) */
    {154, 9, 1, 3, burg_pat_154, 1},  /* ncond: Ne(LoadConst, i32) */
    {155, 8, 1, 3, burg_pat_155, 1},  /* cond: Eq(i32, LoadConst) */
    {156, 8, 1, 3, burg_pat_156, 1},  /* cond: Eq(LoadConst, i32) */
    {157, 9, 0, 3, burg_pat_157, 1},  /* ncond: Eq(i32, LoadConst) */
    {158, 9, 0, 3, burg_pat_158, 1},  /* ncond: Eq(LoadConst, i32) */
    {159, 8, 1, 3, burg_pat_159, 1},  /* cond: Eq(i64, LoadLongConst) */
    {160, 8, 1, 3, burg_pat_160, 1},  /* cond: Eq(LoadLongConst, i64) */
    {161, 9, 2, 3, burg_pat_161, 1},  /* ncond: Eq(i64, LoadLongConst) */
    {162, 9, 2, 3, burg_pat_162, 1},  /* ncond: Eq(LoadLongConst, i64) */
    {163, 8, 2, 3, burg_pat_163, 1},  /* cond: Ne(i64, LoadLongConst) */
    {164, 8, 2, 3, burg_pat_164, 1},  /* cond: Ne(LoadLongConst, i64) */
    {165, 9, 1, 3, burg_pat_165, 1},  /* ncond: Ne(i64, LoadLongConst) */
    {166, 9, 1, 3, burg_pat_166, 1},  /* ncond: Ne(LoadLongConst, i64) */
    {167, 8, 1, 3, burg_pat_167, 0},  /* cond: Eq(ref, LoadNull) */
    {168, 8, 1, 3, burg_pat_168, 0},  /* cond: Eq(LoadNull, ref) */
    {169, 9, 2, 3, burg_pat_169, 0},  /* ncond: Eq(ref, LoadNull) */
    {170, 9, 2, 3, burg_pat_170, 0},  /* ncond: Eq(LoadNull, ref) */
    {171, 8, 2, 3, burg_pat_171, 0},  /* cond: Ne(ref, LoadNull) */
    {172, 8, 2, 3, burg_pat_172, 0},  /* cond: Ne(LoadNull, ref) */
    {173, 9, 1, 3, burg_pat_173, 0},  /* ncond: Ne(ref, LoadNull) */
    {174, 9, 1, 3, burg_pat_174, 0},  /* ncond: Ne(LoadNull, ref) */
    {175, 3, 1, 2, burg_pat_175, 0},  /* i64: I2L(i32) */
    {176, 4, 1, 2, burg_pat_176, 0},  /* f32: I2F(i32) */
    {177, 5, 1, 2, burg_pat_177, 0},  /* f64: I2D(i32) */
    {178, 2, 1, 2, burg_pat_178, 0},  /* i32: L2I(i64) */
    {179, 4, 1, 2, burg_pat_179, 0},  /* f32: L2F(i64) */
    {180, 5, 1, 2, burg_pat_180, 0},  /* f64: L2D(i64) */
    {181, 5, 1, 2, burg_pat_181, 0},  /* f64: F2D(f32) */
    {182, 4, 1, 2, burg_pat_182, 0},  /* f32: D2F(f64) */
    {183, 2, 2, 2, burg_pat_183, 0},  /* i32: F2I(f32) */
    {184, 2, 2, 2, burg_pat_184, 0},  /* i32: D2I(f64) */
    {185, 3, 2, 2, burg_pat_185, 0},  /* i64: F2L(f32) */
    {186, 3, 2, 2, burg_pat_186, 0},  /* i64: D2L(f64) */
    {187, 2, 1, 2, burg_pat_187, 0},  /* i32: MoveF2I(f32) */
    {188, 4, 1, 2, burg_pat_188, 0},  /* f32: MoveI2F(i32) */
    {189, 3, 1, 2, burg_pat_189, 0},  /* i64: MoveD2L(f64) */
    {190, 5, 1, 2, burg_pat_190, 0},  /* f64: MoveL2D(i64) */
    {191, 5, 1, 2, burg_pat_191, 0},  /* f64: F64Sqrt(f64) */
    {192, 5, 1, 2, burg_pat_192, 0},  /* f64: F64Floor(f64) */
    {193, 5, 1, 2, burg_pat_193, 0},  /* f64: F64Ceil(f64) */
    {194, 5, 1, 2, burg_pat_194, 0},  /* f64: F64Nearest(f64) */
    {195, 2, 1, 2, burg_pat_195, 0},  /* i32: I2B(i32) */
    {196, 2, 1, 2, burg_pat_196, 0},  /* i32: I2S(i32) */
    {197, 2, 5, 2, burg_pat_197, 0},  /* i32: I2C(i32) */
    {198, 2, 1, 2, burg_pat_198, 0},  /* i32: S2B(i32) */
    {199, 2, 0, 2, burg_pat_199, 0},  /* i32: S2I(i32) */
    {200, 1, 1, 2, burg_pat_200, 0},  /* stmt: Return(i32) */
    {201, 1, 1, 2, burg_pat_201, 0},  /* stmt: Return(i64) */
    {202, 1, 1, 2, burg_pat_202, 0},  /* stmt: Return(f32) */
    {203, 1, 1, 2, burg_pat_203, 0},  /* stmt: Return(f64) */
    {204, 1, 1, 2, burg_pat_204, 0},  /* stmt: Return(ref) */
    {205, 1, 1, 2, burg_pat_205, 0},  /* stmt: Return(v128) */
    {206, 1, 1, 1, burg_pat_206, 0},  /* stmt: ReturnVoid */
    {207, 1, 0, 2, burg_pat_207, 0},  /* stmt: Return(tail) */
    {208, 1, 2, 2, burg_pat_208, 0},  /* stmt: Throw(ref) */
    {209, 1, 2, 2, burg_pat_209, 0},  /* stmt: StoreLocal(i32) */
    {210, 1, 2, 2, burg_pat_210, 0},  /* stmt: StoreLocal(i64) */
    {211, 1, 2, 2, burg_pat_211, 0},  /* stmt: StoreLocal(f32) */
    {212, 1, 2, 2, burg_pat_212, 0},  /* stmt: StoreLocal(f64) */
    {213, 1, 2, 2, burg_pat_213, 0},  /* stmt: StoreLocal(ref) */
    {214, 1, 2, 2, burg_pat_214, 0},  /* stmt: StoreLocal(v128) */
    {215, 7, 2, 3, burg_pat_215, 1},  /* v128: SimdBin(v128, v128) */
    {216, 7, 3, 3, burg_pat_216, 1},  /* v128: SimdBin(v128, v128) */
    {217, 7, 2, 2, burg_pat_217, 1},  /* v128: SimdUn(v128) */
    {218, 7, 3, 2, burg_pat_218, 1},  /* v128: SimdUn(v128) */
    {219, 7, 2, 3, burg_pat_219, 1},  /* v128: SimdShift(v128, i32) */
    {220, 7, 3, 3, burg_pat_220, 1},  /* v128: SimdShift(v128, i32) */
    {221, 7, 2, 4, burg_pat_221, 1},  /* v128: SimdTern(v128, v128, v128) */
    {222, 7, 3, 4, burg_pat_222, 1},  /* v128: SimdTern(v128, v128, v128) */
    {223, 2, 2, 2, burg_pat_223, 1},  /* i32: SimdTestI(v128) */
    {224, 2, 3, 2, burg_pat_224, 1},  /* i32: SimdTestI(v128) */
    {225, 7, 2, 2, burg_pat_225, 1},  /* v128: SimdSplatI(i32) */
    {226, 7, 3, 2, burg_pat_226, 1},  /* v128: SimdSplatI(i32) */
    {227, 7, 2, 2, burg_pat_227, 1},  /* v128: SimdSplatL(i64) */
    {228, 7, 3, 2, burg_pat_228, 1},  /* v128: SimdSplatL(i64) */
    {229, 7, 2, 2, burg_pat_229, 1},  /* v128: SimdSplatF(f32) */
    {230, 7, 3, 2, burg_pat_230, 1},  /* v128: SimdSplatF(f32) */
    {231, 7, 2, 2, burg_pat_231, 1},  /* v128: SimdSplatD(f64) */
    {232, 7, 3, 2, burg_pat_232, 1},  /* v128: SimdSplatD(f64) */
    {233, 2, 3, 2, burg_pat_233, 1},  /* i32: SimdExtractI(v128) */
    {234, 2, 4, 2, burg_pat_234, 1},  /* i32: SimdExtractI(v128) */
    {235, 3, 3, 2, burg_pat_235, 1},  /* i64: SimdExtractL(v128) */
    {236, 3, 4, 2, burg_pat_236, 1},  /* i64: SimdExtractL(v128) */
    {237, 4, 3, 2, burg_pat_237, 1},  /* f32: SimdExtractF(v128) */
    {238, 4, 4, 2, burg_pat_238, 1},  /* f32: SimdExtractF(v128) */
    {239, 5, 3, 2, burg_pat_239, 1},  /* f64: SimdExtractD(v128) */
    {240, 5, 4, 2, burg_pat_240, 1},  /* f64: SimdExtractD(v128) */
    {241, 7, 3, 3, burg_pat_241, 1},  /* v128: SimdReplaceI(v128, i32) */
    {242, 7, 4, 3, burg_pat_242, 1},  /* v128: SimdReplaceI(v128, i32) */
    {243, 7, 3, 3, burg_pat_243, 1},  /* v128: SimdReplaceL(v128, i64) */
    {244, 7, 4, 3, burg_pat_244, 1},  /* v128: SimdReplaceL(v128, i64) */
    {245, 7, 3, 3, burg_pat_245, 1},  /* v128: SimdReplaceF(v128, f32) */
    {246, 7, 4, 3, burg_pat_246, 1},  /* v128: SimdReplaceF(v128, f32) */
    {247, 7, 3, 3, burg_pat_247, 1},  /* v128: SimdReplaceD(v128, f64) */
    {248, 7, 4, 3, burg_pat_248, 1},  /* v128: SimdReplaceD(v128, f64) */
    {249, 7, 18, 1, burg_pat_249, 0},  /* v128: SimdConst */
    {250, 7, 18, 3, burg_pat_250, 0},  /* v128: SimdShuffle(v128, v128) */
    {251, 7, 4, 2, burg_pat_251, 1},  /* v128: SimdMemLoad(i32) */
    {252, 7, 5, 2, burg_pat_252, 1},  /* v128: SimdMemLoad(i32) */
    {253, 7, 5, 3, burg_pat_253, 1},  /* v128: SimdMemLoadLane(i32, v128) */
    {254, 7, 6, 3, burg_pat_254, 1},  /* v128: SimdMemLoadLane(i32, v128) */
    {255, 2, 3, 2, burg_pat_255, 0},  /* i32: MemLoadI(i32) */
    {256, 3, 3, 2, burg_pat_256, 0},  /* i64: MemLoadL(i32) */
    {257, 4, 3, 2, burg_pat_257, 0},  /* f32: MemLoadF(i32) */
    {258, 5, 3, 2, burg_pat_258, 0},  /* f64: MemLoadD(i32) */
    {259, 2, 2, 1, burg_pat_259, 0},  /* i32: MemSize */
    {260, 2, 2, 2, burg_pat_260, 0},  /* i32: MemGrow(i32) */
    {261, 1, 5, 2, burg_pat_261, 1},  /* stmt: Inc(i32) */
    {262, 1, 6, 2, burg_pat_262, 1},  /* stmt: Inc(i32) */
    {263, 1, 10, 2, burg_pat_263, 1},  /* stmt: Inc(i32) */
    {264, 1, 5, 2, burg_pat_264, 0},  /* stmt: Inc(i64) */
    {265, 1, 8, 2, burg_pat_265, 0},  /* stmt: Inc(f32) */
    {266, 1, 12, 2, burg_pat_266, 0},  /* stmt: Inc(f64) */
    {267, 1, 0, 2, burg_pat_267, 1},  /* stmt: ExprEffect(i32) */
    {268, 1, 0, 2, burg_pat_268, 1},  /* stmt: ExprEffect(i64) */
    {269, 1, 0, 2, burg_pat_269, 1},  /* stmt: ExprEffect(f32) */
    {270, 1, 0, 2, burg_pat_270, 1},  /* stmt: ExprEffect(f64) */
    {271, 1, 0, 2, burg_pat_271, 1},  /* stmt: ExprEffect(ref) */
    {272, 1, 0, 2, burg_pat_272, 1},  /* stmt: ExprEffect(v128) */
    {273, 1, 1, 2, burg_pat_273, 1},  /* stmt: ExprEffect(i32) */
    {274, 1, 1, 2, burg_pat_274, 1},  /* stmt: ExprEffect(i64) */
    {275, 1, 1, 2, burg_pat_275, 1},  /* stmt: ExprEffect(f32) */
    {276, 1, 1, 2, burg_pat_276, 1},  /* stmt: ExprEffect(f64) */
    {277, 1, 1, 2, burg_pat_277, 1},  /* stmt: ExprEffect(ref) */
    {278, 1, 1, 2, burg_pat_278, 1},  /* stmt: ExprEffect(v128) */
    {279, 6, 5, 1, burg_pat_279, 0},  /* ref: New */
    {280, 2, 6, 2, burg_pat_280, 0},  /* i32: ClassInstantiable(ref) */
    {281, 6, 9, 2, burg_pat_281, 0},  /* ref: ClassConstruct(ref) */
    {282, 6, 3, 1, burg_pat_282, 0},  /* ref: CloneCopy */
    {283, 2, 4, 2, burg_pat_283, 1},  /* i32: GetField(ref) */
    {284, 2, 4, 2, burg_pat_284, 1},  /* i32: GetField(ref) */
    {285, 2, 4, 2, burg_pat_285, 1},  /* i32: GetField(ref) */
    {286, 3, 4, 2, burg_pat_286, 1},  /* i64: GetField(ref) */
    {287, 4, 4, 2, burg_pat_287, 1},  /* f32: GetField(ref) */
    {288, 5, 4, 2, burg_pat_288, 1},  /* f64: GetField(ref) */
    {289, 6, 4, 2, burg_pat_289, 1},  /* ref: GetField(ref) */
    {290, 7, 4, 2, burg_pat_290, 1},  /* v128: GetField(ref) */
    {291, 1, 4, 3, burg_pat_291, 0},  /* stmt: PutField(ref, i32) */
    {292, 1, 4, 3, burg_pat_292, 0},  /* stmt: PutField(ref, i64) */
    {293, 1, 4, 3, burg_pat_293, 0},  /* stmt: PutField(ref, f32) */
    {294, 1, 4, 3, burg_pat_294, 0},  /* stmt: PutField(ref, f64) */
    {295, 1, 4, 3, burg_pat_295, 0},  /* stmt: PutField(ref, ref) */
    {296, 1, 4, 3, burg_pat_296, 0},  /* stmt: PutField(ref, v128) */
    {297, 1, 4, 3, burg_pat_297, 0},  /* stmt: SetHeader(ref, ref) */
    {298, 2, 2, 1, burg_pat_298, 1},  /* i32: GetStatic */
    {299, 3, 2, 1, burg_pat_299, 1},  /* i64: GetStatic */
    {300, 4, 2, 1, burg_pat_300, 1},  /* f32: GetStatic */
    {301, 5, 2, 1, burg_pat_301, 1},  /* f64: GetStatic */
    {302, 6, 2, 1, burg_pat_302, 1},  /* ref: GetStatic */
    {303, 7, 2, 1, burg_pat_303, 1},  /* v128: GetStatic */
    {304, 1, 2, 2, burg_pat_304, 0},  /* stmt: PutStatic(i32) */
    {305, 1, 2, 2, burg_pat_305, 0},  /* stmt: PutStatic(i64) */
    {306, 1, 2, 2, burg_pat_306, 0},  /* stmt: PutStatic(f32) */
    {307, 1, 2, 2, burg_pat_307, 0},  /* stmt: PutStatic(f64) */
    {308, 1, 2, 2, burg_pat_308, 0},  /* stmt: PutStatic(ref) */
    {309, 1, 2, 2, burg_pat_309, 0},  /* stmt: PutStatic(v128) */
    {310, 1, 4, 3, burg_pat_310, 1},  /* stmt: SimdMemStore(i32, v128) */
    {311, 1, 5, 3, burg_pat_311, 1},  /* stmt: SimdMemStore(i32, v128) */
    {312, 1, 5, 3, burg_pat_312, 1},  /* stmt: SimdMemStoreLane(i32, v128) */
    {313, 1, 6, 3, burg_pat_313, 1},  /* stmt: SimdMemStoreLane(i32, v128) */
    {314, 1, 3, 3, burg_pat_314, 0},  /* stmt: MemStoreI(i32, i32) */
    {315, 1, 3, 3, burg_pat_315, 0},  /* stmt: MemStoreL(i32, i64) */
    {316, 1, 3, 3, burg_pat_316, 0},  /* stmt: MemStoreF(i32, f32) */
    {317, 1, 3, 3, burg_pat_317, 0},  /* stmt: MemStoreD(i32, f64) */
    {318, 1, 3, 4, burg_pat_318, 0},  /* stmt: MemFill(i32, i32, i32) */
    {319, 1, 4, 4, burg_pat_319, 0},  /* stmt: MemCopy(i32, i32, i32) */
    {320, 6, 3, 2, burg_pat_320, 0},  /* ref: NewArray(i32) */
    {321, 6, 8, 1, burg_pat_321, 0},  /* ref: ArrayNewData */
    {322, 2, 3, 3, burg_pat_322, 1},  /* i32: ArrayLoad(ref, i32) */
    {323, 2, 3, 3, burg_pat_323, 1},  /* i32: ArrayLoad(ref, i32) */
    {324, 2, 3, 3, burg_pat_324, 1},  /* i32: ArrayLoad(ref, i32) */
    {325, 3, 3, 3, burg_pat_325, 1},  /* i64: ArrayLoad(ref, i32) */
    {326, 4, 3, 3, burg_pat_326, 1},  /* f32: ArrayLoad(ref, i32) */
    {327, 5, 3, 3, burg_pat_327, 1},  /* f64: ArrayLoad(ref, i32) */
    {328, 7, 3, 3, burg_pat_328, 1},  /* v128: ArrayLoad(ref, i32) */
    {329, 1, 3, 4, burg_pat_329, 0},  /* stmt: ArrayStore(ref, i32, i32) */
    {330, 1, 3, 4, burg_pat_330, 0},  /* stmt: ArrayStore(ref, i32, i64) */
    {331, 1, 3, 4, burg_pat_331, 0},  /* stmt: ArrayStore(ref, i32, f32) */
    {332, 1, 3, 4, burg_pat_332, 0},  /* stmt: ArrayStore(ref, i32, f64) */
    {333, 1, 3, 4, burg_pat_333, 0},  /* stmt: ArrayStore(ref, i32, v128) */
    {334, 2, 2, 2, burg_pat_334, 0},  /* i32: ArrayLength(ref) */
    {335, 6, 3, 2, burg_pat_335, 0},  /* ref: NewRefArray(i32) */
    {336, 6, 6, 3, burg_pat_336, 1},  /* ref: ArrayLoad(ref, i32) */
    {337, 1, 3, 4, burg_pat_337, 0},  /* stmt: ArrayStore(ref, i32, ref) */
    {338, 1, 4, 6, burg_pat_338, 0},  /* stmt: ArrayCopy(ref, i32, ref, i32, i32) */
    {339, 2, 2, 1, burg_pat_339, 1},  /* i32: InvokeStatic */
    {340, 3, 2, 1, burg_pat_340, 1},  /* i64: InvokeStatic */
    {341, 4, 2, 1, burg_pat_341, 1},  /* f32: InvokeStatic */
    {342, 5, 2, 1, burg_pat_342, 1},  /* f64: InvokeStatic */
    {343, 6, 2, 1, burg_pat_343, 1},  /* ref: InvokeStatic */
    {344, 7, 2, 1, burg_pat_344, 1},  /* v128: InvokeStatic */
    {345, 2, 2, 2, burg_pat_345, 1},  /* i32: InvokeSpecial(ref) */
    {346, 3, 2, 2, burg_pat_346, 1},  /* i64: InvokeSpecial(ref) */
    {347, 4, 2, 2, burg_pat_347, 1},  /* f32: InvokeSpecial(ref) */
    {348, 5, 2, 2, burg_pat_348, 1},  /* f64: InvokeSpecial(ref) */
    {349, 6, 2, 2, burg_pat_349, 1},  /* ref: InvokeSpecial(ref) */
    {350, 7, 2, 2, burg_pat_350, 1},  /* v128: InvokeSpecial(ref) */
    {351, 2, 20, 2, burg_pat_351, 1},  /* i32: InvokeVirtual(ref) */
    {352, 3, 20, 2, burg_pat_352, 1},  /* i64: InvokeVirtual(ref) */
    {353, 4, 20, 2, burg_pat_353, 1},  /* f32: InvokeVirtual(ref) */
    {354, 5, 20, 2, burg_pat_354, 1},  /* f64: InvokeVirtual(ref) */
    {355, 6, 20, 2, burg_pat_355, 1},  /* ref: InvokeVirtual(ref) */
    {356, 7, 20, 2, burg_pat_356, 1},  /* v128: InvokeVirtual(ref) */
    {357, 2, 23, 2, burg_pat_357, 1},  /* i32: InvokeInterface(ref) */
    {358, 3, 23, 2, burg_pat_358, 1},  /* i64: InvokeInterface(ref) */
    {359, 4, 23, 2, burg_pat_359, 1},  /* f32: InvokeInterface(ref) */
    {360, 5, 23, 2, burg_pat_360, 1},  /* f64: InvokeInterface(ref) */
    {361, 6, 23, 2, burg_pat_361, 1},  /* ref: InvokeInterface(ref) */
    {362, 7, 23, 2, burg_pat_362, 1},  /* v128: InvokeInterface(ref) */
    {363, 10, 2, 1, burg_pat_363, 0},  /* tail: InvokeStatic */
    {364, 10, 2, 2, burg_pat_364, 0},  /* tail: InvokeSpecial(ref) */
    {365, 10, 20, 2, burg_pat_365, 0},  /* tail: InvokeVirtual(ref) */
    {366, 10, 23, 2, burg_pat_366, 0},  /* tail: InvokeInterface(ref) */
    {367, 2, 3, 2, burg_pat_367, 0},  /* i32: InstanceOf(ref) */
    {368, 6, 3, 2, burg_pat_368, 0},  /* ref: CheckCast(ref) */
    {369, 1, 2, 1, burg_pat_369, 0},  /* stmt: ExceptionEntry */
    {370, 1, 0, 1, burg_pat_370, 0},  /* stmt: Nop */
    {371, 1, 0, 1, burg_pat_371, 0},  /* stmt: i32 */
    {372, 1, 0, 1, burg_pat_372, 0},  /* stmt: i64 */
    {373, 1, 0, 1, burg_pat_373, 0},  /* stmt: f32 */
    {374, 1, 0, 1, burg_pat_374, 0},  /* stmt: f64 */
    {375, 1, 0, 1, burg_pat_375, 0},  /* stmt: ref */
    {376, 1, 0, 1, burg_pat_376, 0},  /* stmt: v128 */
};
const int burg_rule_table_len = 376;

int burg_rule_guard(int rule, BURG_NODE_TYPE node, burg_ctx_t* ctx) {
    (void)node;
    (void)ctx;
    switch (rule) {
    case 5: return (DT_IS_I32(ll_dt(node))) ? 1 : 0;
    case 6: return (ll_dt(node) == SIR_DTLONG) ? 1 : 0;
    case 7: return (ll_dt(node) == SIR_DTFLOAT) ? 1 : 0;
    case 8: return (ll_dt(node) == SIR_DTDOUBLE) ? 1 : 0;
    case 9: return (ll_dt(node) == SIR_DTREF) ? 1 : 0;
    case 10: return (ll_dt(node) == SIR_DTV128) ? 1 : 0;
    case 76: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 77: return (CV(KID(node,0)) == 0) ? 1 : 0;
    case 78: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 79: return (CV(KID(node,0)) == 0) ? 1 : 0;
    case 80: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 81: return (LV(KID(node,0)) == 0) ? 1 : 0;
    case 82: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 83: return (LV(KID(node,0)) == 0) ? 1 : 0;
    case 88: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 89: return (CV(KID(node,0)) == 0) ? 1 : 0;
    case 90: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 91: return (CV(KID(node,1)) == 1) ? 1 : 0;
    case 92: return (CV(KID(node,0)) == 1) ? 1 : 0;
    case 93: return (CV(KID(node,1)) == 1) ? 1 : 0;
    case 94: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 95: return (CV(KID(node,0)) == 0) ? 1 : 0;
    case 96: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 97: return (CV(KID(node,0)) == 0) ? 1 : 0;
    case 98: return (CV(KID(node,1)) == -1) ? 1 : 0;
    case 99: return (CV(KID(node,0)) == -1) ? 1 : 0;
    case 100: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 101: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 102: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 103: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 104: return (LV(KID(node,0)) == 0) ? 1 : 0;
    case 105: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 106: return (LV(KID(node,1)) == 1) ? 1 : 0;
    case 107: return (LV(KID(node,0)) == 1) ? 1 : 0;
    case 108: return (LV(KID(node,1)) == 1) ? 1 : 0;
    case 109: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 110: return (LV(KID(node,0)) == 0) ? 1 : 0;
    case 111: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 112: return (LV(KID(node,0)) == 0) ? 1 : 0;
    case 113: return (LV(KID(node,1)) == -1) ? 1 : 0;
    case 114: return (LV(KID(node,0)) == -1) ? 1 : 0;
    case 115: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 116: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 117: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 128: return (JF_DIVOK32(CV(KID(node,0)), CV(KID(node,1)))) ? 1 : 0;
    case 129: return (JF_DIVOK32(CV(KID(node,0)), CV(KID(node,1)))) ? 1 : 0;
    case 140: return (JF_DIVOK64(LV(KID(node,0)), LV(KID(node,1)))) ? 1 : 0;
    case 141: return (JF_DIVOK64(LV(KID(node,0)), LV(KID(node,1)))) ? 1 : 0;
    case 142: return (JF_POW2(CV(KID(node,1)))) ? 1 : 0;
    case 143: return (JF_POW2(CV(KID(node,0)))) ? 1 : 0;
    case 144: return (JF_POW2(LV(KID(node,1)))) ? 1 : 0;
    case 145: return (JF_POW2(LV(KID(node,0)))) ? 1 : 0;
    case 146: return (JF_POW2(CV(KID(node,1)))
                                  && jf_pub_nonneg_i32(ctx->facts, KID(node,0))) ? 1 : 0;
    case 151: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 152: return (CV(KID(node,0)) == 0) ? 1 : 0;
    case 153: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 154: return (CV(KID(node,0)) == 0) ? 1 : 0;
    case 155: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 156: return (CV(KID(node,0)) == 0) ? 1 : 0;
    case 157: return (CV(KID(node,1)) == 0) ? 1 : 0;
    case 158: return (CV(KID(node,0)) == 0) ? 1 : 0;
    case 159: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 160: return (LV(KID(node,0)) == 0) ? 1 : 0;
    case 161: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 162: return (LV(KID(node,0)) == 0) ? 1 : 0;
    case 163: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 164: return (LV(KID(node,0)) == 0) ? 1 : 0;
    case 165: return (LV(KID(node,1)) == 0) ? 1 : 0;
    case 166: return (LV(KID(node,0)) == 0) ? 1 : 0;
    case 215: return (ew_op_width((wasm_op_t)node->simd_bin.op) == 2) ? 1 : 0;
    case 216: return (ew_op_width((wasm_op_t)node->simd_bin.op) == 3) ? 1 : 0;
    case 217: return (ew_op_width((wasm_op_t)node->simd_un.op) == 2) ? 1 : 0;
    case 218: return (ew_op_width((wasm_op_t)node->simd_un.op) == 3) ? 1 : 0;
    case 219: return (ew_op_width((wasm_op_t)node->simd_shift.op) == 2) ? 1 : 0;
    case 220: return (ew_op_width((wasm_op_t)node->simd_shift.op) == 3) ? 1 : 0;
    case 221: return (ew_op_width((wasm_op_t)node->simd_tern.op) == 2) ? 1 : 0;
    case 222: return (ew_op_width((wasm_op_t)node->simd_tern.op) == 3) ? 1 : 0;
    case 223: return (ew_op_width((wasm_op_t)node->simd_test_i.op) == 2) ? 1 : 0;
    case 224: return (ew_op_width((wasm_op_t)node->simd_test_i.op) == 3) ? 1 : 0;
    case 225: return (ew_op_width((wasm_op_t)node->simd_splat_i.op) == 2) ? 1 : 0;
    case 226: return (ew_op_width((wasm_op_t)node->simd_splat_i.op) == 3) ? 1 : 0;
    case 227: return (ew_op_width((wasm_op_t)node->simd_splat_l.op) == 2) ? 1 : 0;
    case 228: return (ew_op_width((wasm_op_t)node->simd_splat_l.op) == 3) ? 1 : 0;
    case 229: return (ew_op_width((wasm_op_t)node->simd_splat_f.op) == 2) ? 1 : 0;
    case 230: return (ew_op_width((wasm_op_t)node->simd_splat_f.op) == 3) ? 1 : 0;
    case 231: return (ew_op_width((wasm_op_t)node->simd_splat_d.op) == 2) ? 1 : 0;
    case 232: return (ew_op_width((wasm_op_t)node->simd_splat_d.op) == 3) ? 1 : 0;
    case 233: return (ew_op_width((wasm_op_t)node->simd_extract_i.op) == 2) ? 1 : 0;
    case 234: return (ew_op_width((wasm_op_t)node->simd_extract_i.op) == 3) ? 1 : 0;
    case 235: return (ew_op_width((wasm_op_t)node->simd_extract_l.op) == 2) ? 1 : 0;
    case 236: return (ew_op_width((wasm_op_t)node->simd_extract_l.op) == 3) ? 1 : 0;
    case 237: return (ew_op_width((wasm_op_t)node->simd_extract_f.op) == 2) ? 1 : 0;
    case 238: return (ew_op_width((wasm_op_t)node->simd_extract_f.op) == 3) ? 1 : 0;
    case 239: return (ew_op_width((wasm_op_t)node->simd_extract_d.op) == 2) ? 1 : 0;
    case 240: return (ew_op_width((wasm_op_t)node->simd_extract_d.op) == 3) ? 1 : 0;
    case 241: return (ew_op_width((wasm_op_t)node->simd_replace_i.op) == 2) ? 1 : 0;
    case 242: return (ew_op_width((wasm_op_t)node->simd_replace_i.op) == 3) ? 1 : 0;
    case 243: return (ew_op_width((wasm_op_t)node->simd_replace_l.op) == 2) ? 1 : 0;
    case 244: return (ew_op_width((wasm_op_t)node->simd_replace_l.op) == 3) ? 1 : 0;
    case 245: return (ew_op_width((wasm_op_t)node->simd_replace_f.op) == 2) ? 1 : 0;
    case 246: return (ew_op_width((wasm_op_t)node->simd_replace_f.op) == 3) ? 1 : 0;
    case 247: return (ew_op_width((wasm_op_t)node->simd_replace_d.op) == 2) ? 1 : 0;
    case 248: return (ew_op_width((wasm_op_t)node->simd_replace_d.op) == 3) ? 1 : 0;
    case 251: return (ew_op_width((wasm_op_t)node->simd_mem_load.op) == 2) ? 1 : 0;
    case 252: return (ew_op_width((wasm_op_t)node->simd_mem_load.op) == 3) ? 1 : 0;
    case 253: return (ew_op_width((wasm_op_t)node->simd_mem_load_lane.op) == 2) ? 1 : 0;
    case 254: return (ew_op_width((wasm_op_t)node->simd_mem_load_lane.op) == 3) ? 1 : 0;
    case 261: return (node->inc.data_type == SIR_DTINT) ? 1 : 0;
    case 262: return (node->inc.data_type == SIR_DTBYTE || node->inc.data_type == SIR_DTSHORT) ? 1 : 0;
    case 263: return (node->inc.data_type == SIR_DTCHAR) ? 1 : 0;
    case 267: return (node->expr_effect.is_void) ? 1 : 0;
    case 268: return (node->expr_effect.is_void) ? 1 : 0;
    case 269: return (node->expr_effect.is_void) ? 1 : 0;
    case 270: return (node->expr_effect.is_void) ? 1 : 0;
    case 271: return (node->expr_effect.is_void) ? 1 : 0;
    case 272: return (node->expr_effect.is_void) ? 1 : 0;
    case 273: return (!node->expr_effect.is_void) ? 1 : 0;
    case 274: return (!node->expr_effect.is_void) ? 1 : 0;
    case 275: return (!node->expr_effect.is_void) ? 1 : 0;
    case 276: return (!node->expr_effect.is_void) ? 1 : 0;
    case 277: return (!node->expr_effect.is_void) ? 1 : 0;
    case 278: return (!node->expr_effect.is_void) ? 1 : 0;
    case 283: return (node->get_field.data_type == SIR_DTINT) ? 1 : 0;
    case 284: return (node->get_field.data_type == SIR_DTBYTE || node->get_field.data_type == SIR_DTSHORT) ? 1 : 0;
    case 285: return (node->get_field.data_type == SIR_DTCHAR) ? 1 : 0;
    case 286: return (node->get_field.data_type == SIR_DTLONG) ? 1 : 0;
    case 287: return (node->get_field.data_type == SIR_DTFLOAT) ? 1 : 0;
    case 288: return (node->get_field.data_type == SIR_DTDOUBLE) ? 1 : 0;
    case 289: return (node->get_field.data_type == SIR_DTREF) ? 1 : 0;
    case 290: return (node->get_field.data_type == SIR_DTV128) ? 1 : 0;
    case 298: return (DT_IS_I32(node->get_static.data_type)) ? 1 : 0;
    case 299: return (node->get_static.data_type == SIR_DTLONG) ? 1 : 0;
    case 300: return (node->get_static.data_type == SIR_DTFLOAT) ? 1 : 0;
    case 301: return (node->get_static.data_type == SIR_DTDOUBLE) ? 1 : 0;
    case 302: return (node->get_static.data_type == SIR_DTREF) ? 1 : 0;
    case 303: return (node->get_static.data_type == SIR_DTV128) ? 1 : 0;
    case 310: return (ew_op_width((wasm_op_t)node->simd_mem_store.op) == 2) ? 1 : 0;
    case 311: return (ew_op_width((wasm_op_t)node->simd_mem_store.op) == 3) ? 1 : 0;
    case 312: return (ew_op_width((wasm_op_t)node->simd_mem_store_lane.op) == 2) ? 1 : 0;
    case 313: return (ew_op_width((wasm_op_t)node->simd_mem_store_lane.op) == 3) ? 1 : 0;
    case 322: return (node->array_load.data_type == SIR_DTINT) ? 1 : 0;
    case 323: return (node->array_load.data_type == SIR_DTBYTE || node->array_load.data_type == SIR_DTSHORT) ? 1 : 0;
    case 324: return (node->array_load.data_type == SIR_DTCHAR) ? 1 : 0;
    case 325: return (node->array_load.data_type == SIR_DTLONG) ? 1 : 0;
    case 326: return (node->array_load.data_type == SIR_DTFLOAT) ? 1 : 0;
    case 327: return (node->array_load.data_type == SIR_DTDOUBLE) ? 1 : 0;
    case 328: return (node->array_load.data_type == SIR_DTV128) ? 1 : 0;
    case 336: return (node->array_load.data_type == SIR_DTREF) ? 1 : 0;
    case 339: return (DT_IS_I32(node->invoke_static.return_type)) ? 1 : 0;
    case 340: return (node->invoke_static.return_type == SIR_DTLONG) ? 1 : 0;
    case 341: return (node->invoke_static.return_type == SIR_DTFLOAT) ? 1 : 0;
    case 342: return (node->invoke_static.return_type == SIR_DTDOUBLE) ? 1 : 0;
    case 343: return (node->invoke_static.return_type == SIR_DTREF) ? 1 : 0;
    case 344: return (node->invoke_static.return_type == SIR_DTV128) ? 1 : 0;
    case 345: return (DT_IS_I32(node->invoke_special.return_type)) ? 1 : 0;
    case 346: return (node->invoke_special.return_type == SIR_DTLONG) ? 1 : 0;
    case 347: return (node->invoke_special.return_type == SIR_DTFLOAT) ? 1 : 0;
    case 348: return (node->invoke_special.return_type == SIR_DTDOUBLE) ? 1 : 0;
    case 349: return (node->invoke_special.return_type == SIR_DTREF) ? 1 : 0;
    case 350: return (node->invoke_special.return_type == SIR_DTV128) ? 1 : 0;
    case 351: return (DT_IS_I32(node->invoke_virtual.return_type)) ? 1 : 0;
    case 352: return (node->invoke_virtual.return_type == SIR_DTLONG) ? 1 : 0;
    case 353: return (node->invoke_virtual.return_type == SIR_DTFLOAT) ? 1 : 0;
    case 354: return (node->invoke_virtual.return_type == SIR_DTDOUBLE) ? 1 : 0;
    case 355: return (node->invoke_virtual.return_type == SIR_DTREF) ? 1 : 0;
    case 356: return (node->invoke_virtual.return_type == SIR_DTV128) ? 1 : 0;
    case 357: return (DT_IS_I32(node->invoke_interface.return_type)) ? 1 : 0;
    case 358: return (node->invoke_interface.return_type == SIR_DTLONG) ? 1 : 0;
    case 359: return (node->invoke_interface.return_type == SIR_DTFLOAT) ? 1 : 0;
    case 360: return (node->invoke_interface.return_type == SIR_DTDOUBLE) ? 1 : 0;
    case 361: return (node->invoke_interface.return_type == SIR_DTREF) ? 1 : 0;
    case 362: return (node->invoke_interface.return_type == SIR_DTV128) ? 1 : 0;
    default: return 1;   /* unguarded */
    }
}
