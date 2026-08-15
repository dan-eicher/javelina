#include "wat_layout.h"

void wat_layout_burg_ctx_init(wat_layout_burg_ctx_t* ctx) {
    bbq_arena_init(&ctx->arena, 4096);
    ctx->state_cache = bbq_htree_create();
    ctx->burg_error_msg = NULL;
    ctx->burg_error_arg = 0;
}

void wat_layout_burg_ctx_free(wat_layout_burg_ctx_t* ctx) {
    bbq_arena_free(&ctx->arena);
    bbq_htree_destroy(ctx->state_cache);
    ctx->state_cache = NULL;
}

bool wat_layout_burg_has_error(const wat_layout_burg_ctx_t* ctx) {
    return ctx->burg_error_msg != NULL;
}

const char* wat_layout_burg_get_error(const wat_layout_burg_ctx_t* ctx) {
    return ctx->burg_error_msg;
}

int wat_layout_burg_get_error_arg(const wat_layout_burg_ctx_t* ctx) {
    return ctx->burg_error_arg;
}

void wat_layout_burg_clear_error(wat_layout_burg_ctx_t* ctx) {
    ctx->burg_error_msg = NULL;
    ctx->burg_error_arg = 0;
}

void wat_layout_burg_set_error(const char* msg, int arg, wat_layout_burg_ctx_t* ctx) {
    if (ctx->burg_error_msg == NULL) {
        ctx->burg_error_msg = msg;
        ctx->burg_error_arg = arg;
    }
}


static BURG_UNUSED void* arena_alloc(size_t size, wat_layout_burg_ctx_t* ctx) {
    return bbq_arena_alloc(&ctx->arena, size);
}

static BURG_UNUSED void arena_reset(wat_layout_burg_ctx_t* ctx) {
    bbq_arena_reset(&ctx->arena);
}

static BURG_UNUSED burg_state_t* burg_cache_lookup(uint32_t id, wat_layout_burg_ctx_t* ctx) {
    return (burg_state_t*)bbq_htree_search(ctx->state_cache, id);
}

static BURG_UNUSED void burg_cache_store(uint32_t id, burg_state_t* state, wat_layout_burg_ctx_t* ctx) {
    bbq_htree_insert(ctx->state_cache, id, state);
}

static BURG_UNUSED void burg_cache_clear(wat_layout_burg_ctx_t* ctx) {
    bbq_htree_clear(ctx->state_cache);
}

static void closure_externdesc(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_typeuse(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_comptype(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_subtype(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_instr(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_decl(burg_state_t* p, int c, BURG_NODE_TYPE node);

static void closure_externdesc(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 529;
    }
}

static void closure_typeuse(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 528;
    }
}

static void closure_comptype(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 527;
    }
}

static void closure_subtype(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 526;
    }
}

static void closure_instr(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 525;
    }
}

static void closure_decl(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 524;
    }
}


static void burg_dp(burg_state_t* p, BURG_NODE_TYPE node, wat_layout_burg_ctx_t* ctx) {
    (void)node;
    (void)ctx;
    int op = p->op;
    switch (op) {
    case BURG_W_op_nil:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 523;
            }
        }
        break;
    case BURG_I_f64x2_relaxed_max:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 518;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_relaxed_min:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 517;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_relaxed_laneselect:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 511;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_relaxed_madd:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 507;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_relaxed_trunc_f64x2_s_zero:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 34;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 505;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_convert_low_i32x4_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 500;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_trunc_sat_f64x2_u_zero:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 30;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 499;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_convert_i32x4_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 23;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 497;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_trunc_sat_f32x4_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 25;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 495;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_trunc_sat_f32x4_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 25;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 494;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_pmax:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 493;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_max:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 491;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_min:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 490;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_mul:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 488;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_pmin:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 481;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_max:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 480;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_pmax:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 482;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_mul:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 477;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_add:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 475;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_sqrt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 474;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_abs:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 472;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_extmul_low_i32x4_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 470;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_extmul_low_i32x4_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 468;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_ge_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 467;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_le_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 466;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_lt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 464;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_ne:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 463;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_op_cons:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 522;
            }
        }
        break;
    case BURG_I_i64x2_mul:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 461;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_add:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 459;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_shl:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 456;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_extend_high_i32x4_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 453;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_extend_low_i32x4_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 452;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_bitmask:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 451;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_all_true:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 450;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_extmul_low_i16x8_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 446;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_relaxed_madd:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 509;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_extmul_high_i16x8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 445;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_max_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 442;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_min_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 440;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_min_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 439;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_mul:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 438;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_extend_low_i32x4_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 454;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_sub:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 437;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_add:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 436;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_div:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 478;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_extend_low_i16x8_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 431;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_extend_high_i16x8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 430;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_bitmask:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 428;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_abs:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 425;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extmul_high_i8x16_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 424;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_min_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 417;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_mul:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 415;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_extend_high_i16x8_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 432;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_nearest:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 414;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_sub_sat_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 413;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_add_sat_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 410;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_add:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 408;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_shl:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 405;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extend_high_i8x16_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 404;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_bitmask:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 398;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_all_true:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 397;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_q15mulr_sat_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 396;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_neg:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 395;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_abs:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 394;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_sub_sat_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 412;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_extadd_pairwise_i16x8_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 31;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 393;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_extadd_pairwise_i16x8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 31;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 392;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_trunc:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 388;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_min_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 385;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_sub_sat_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 380;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_add_sat_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 377;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_add:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 376;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_shr_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 374;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_nearest:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 372;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_extmul_high_i32x4_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 469;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_trunc:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 371;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_relaxed_laneselect:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 512;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_floor:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 370;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_ceil:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 369;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_narrow_i16x8_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 22;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 368;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_ceil:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 382;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_narrow_i16x8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 22;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 367;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_all_true:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 365;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_popcnt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 364;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_neg:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 363;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_abs:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 362;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_store64_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 357;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_relaxed_dot_i8x16_i7x16_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 33;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 520;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_store8_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 18;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 354;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load64_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 18;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 353;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load8_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 350;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_any_true:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 349;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_xor:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 347;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_not:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 343;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_ge:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 342;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_le:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 341;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_gt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 340;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_lt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 339;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_le:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 335;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_neg:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 426;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_ne:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 332;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_narrow_i32x4_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 22;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 399;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_eq:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 331;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_ge_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 330;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_ge_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 329;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_le_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 328;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_le_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 327;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_gt_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 326;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_gt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 325;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_lt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 323;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_sub:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 460;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_ge_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 320;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_le_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 318;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_eq:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 462;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_gt_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 316;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_ne:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 312;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_eq:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 311;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_gt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 305;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_lt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 303;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_ne:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 302;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_eq:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 301;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_extract_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 299;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_max_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 419;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_max_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 387;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_store32_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 356;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_extract_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 297;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_replace_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 296;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_or:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 346;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_extract_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 293;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extract_lane_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 22;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 291;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extract_lane_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 22;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 290;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_extract_lane_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 22;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 288;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extend_low_i8x16_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 403;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_splat:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 286;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extend_low_i8x16_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 401;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_splat:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 285;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_splat:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 283;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_splat:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 282;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_splat:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 281;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_swizzle:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 280;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_shuffle:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 279;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_relaxed_nmadd:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 508;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_store:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 277;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load64_splat:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 276;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load32_splat:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 275;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load32_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 18;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 352;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load16_splat:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 274;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_div:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 489;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load8_splat:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 18;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 273;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_min_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 416;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load32x2_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 272;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load16x4_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 270;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load8x8_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 268;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load8x8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 267;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 266;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_table_grow:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 263;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_memory_fill:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 259;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_extmul_low_i16x8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 444;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_splat:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 284;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_memory_copy:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 258;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_gt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 315;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_replace_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 289;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_mul:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 121;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_trunc_f32_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 181;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_ctz:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 117;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_neg:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 473;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_le:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 114;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_ceil:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 168;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_new_fixed:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 225;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_eq:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 110;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_loop:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 6;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 28;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_sub:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 379;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_le_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 101;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_start:
        {
            int c = 0 + 7;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 10;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_W_custom:
        {
            int c = 0 + 9;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 13;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_I_i64_gt_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 99;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_eqz:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 82;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_rotr:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 133;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_trunc_sat_f64x2_s_zero:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 30;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 498;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_ref_is_null:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 211;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_shr_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 375;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_ne:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 95;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_gt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 87;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_table_get:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 51;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_store:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 68;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_eq:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 321;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_trunc_sat_f32_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 252;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_neg:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 484;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_rem_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 124;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_table_fill:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 265;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_fill:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 233;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_const:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 81;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_eq:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 104;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_max_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 418;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_and:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 126;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_ne:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 111;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_replace_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 292;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_const:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 80;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_demote_f64x2_zero:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 25;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 360;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_table_copy:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 262;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_shl:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 147;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_relaxed_min:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 515;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_const:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 78;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_relaxed_q15mulr_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 25;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 519;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_div_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 123;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_memory_size:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 76;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_extmul_high_i16x8_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 447;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_shl:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 129;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_shr_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 406;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extend_high_i8x16_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 402;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_lt_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 324;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_store16:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 72;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_extract_lane_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 22;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 287;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_le:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 108;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_clz:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 134;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_store:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 70;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_memory_init:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 256;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load32_zero:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 18;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 358;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_ge_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 310;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_lt_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 86;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_load16_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 64;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_or:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 127;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_return_call:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 38;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_load16_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 63;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extmul_low_i8x16_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 423;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_load16_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 60;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_ne:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 338;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_ge_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 319;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_ge_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 102;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_ge_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 309;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_load8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 61;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_copysign:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 179;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_ed_tag:
        {
            int c = 0 + 5;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 24;
                closure_externdesc(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_shl:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 373;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_return_call_indirect:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 22;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 39;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_le_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 308;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_nop:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 5;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 26;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_replace_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 294;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_min:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 177;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_load8_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 62;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_ed_func:
        {
            int c = 0 + 6;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 20;
                closure_externdesc(p, c, node);
            }
        }
        break;
    case BURG_I_f32_ne:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 105;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_shr_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 148;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_nearest:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 157;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_typeuse:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 19;
                closure_typeuse(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_gt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 465;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_min_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 384;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_return:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 35;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_max_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 441;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_le_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 100;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_abs:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 166;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_get_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 229;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_sub:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 487;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_and:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 344;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_load:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 56;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_min:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 163;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_br_on_null:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 215;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_abs:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 483;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_data:
        {
            int c = 0 + 6;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 12;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_max_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 386;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_type:
        {
            int c = 0 + 6;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 1;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_I_br_table:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 34;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_lt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 313;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_ref_cast:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 239;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load16_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 18;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 351;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_ge:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 115;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_sqrt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 485;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_local_tee:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 48;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load16x4_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 269;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_const:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 79;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_shr_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 457;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_demote_f64:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 195;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_gt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 113;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_load:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 53;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_dot_i16x8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 443;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_replace_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 298;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_global:
        {
            int c = 0 + 8;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 7;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_W_comp_func:
        {
            int c = 0 + 6;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 16;
                closure_comptype(p, c, node);
            }
        }
        break;
    case BURG_I_i32_xor:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 128;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_memory:
        {
            int c = 0 + 8;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 6;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_W_sub:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 14;
                closure_subtype(p, c, node);
            }
        }
        break;
    case BURG_I_i64_lt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 96;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_relaxed_dot_i8x16_i7x16_add_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 37;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 521;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_eq:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 83;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_struct_get:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 219;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extmul_low_i8x16_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 421;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_if:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 29;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_shr_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 434;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_convert_i32_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 196;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_shr_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 130;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_drop:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 6;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 42;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_unreachable:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 25;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_popcnt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 118;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_add_sat_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 409;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_comp_struct:
        {
            int c = 0 + 8;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 17;
                closure_comptype(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_shr_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 435;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_ne:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 84;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_throw:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 7;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 30;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_max:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 178;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_sub:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 120;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_elem:
        {
            int c = 0 + 6;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 11;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_gt_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 306;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_ref_eq:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 213;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_lt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 112;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_try_table:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 45;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_br_if:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 7;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 33;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_struct_new:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 217;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_ed_table:
        {
            int c = 0 + 7;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 21;
                closure_externdesc(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_le_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 307;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_block:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 7;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 27;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_func:
        {
            int c = 0 + 6;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 4;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_I_i64_ctz:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 135;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_memory_grow:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 77;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_max:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 164;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_reinterpret_f32:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 201;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_export:
        {
            int c = 0 + 8;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 9;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_convert_low_i32x4_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 501;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_clz:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 116;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_trunc_f64_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 189;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_ref_cast_x17:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 240;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_eq:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 337;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_table_set:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 52;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_table:
        {
            int c = 0 + 7;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 5;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_I_f32_copysign:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 165;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_lt_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 314;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_rem_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 125;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_tag:
        {
            int c = 0 + 5;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 8;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_W_ed_global:
        {
            int c = 0 + 8;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 23;
                closure_externdesc(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_shr_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 407;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_comp_array:
        {
            int c = 0 + 7;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 18;
                closure_comptype(p, c, node);
            }
        }
        break;
    case BURG_I_i64_trunc_f64_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 190;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_eqz:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 93;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_store16:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 74;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_ref_as_non_null:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 214;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_load32_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 65;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_gt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 107;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_throw_ref:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 31;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_add:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 486;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_load:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 54;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_add:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 173;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_andnot:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 345;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_add:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 119;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_br:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 32;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_min:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 479;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_add_sat_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 378;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i31_get_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 247;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_div_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 141;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_div_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 122;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_select_x1c:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 44;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_avgr_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 420;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_eq:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 94;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_call:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 6;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 36;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_relaxed_laneselect:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 513;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_store:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 67;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_call_indirect:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 37;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_shr_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 458;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_narrow_i32x4_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 22;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 400;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_lt_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 97;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_ref_i31:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 245;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_all_true:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 427;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_lt_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 304;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_load8_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 58;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_load8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 57;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_lt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 106;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_neg:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 167;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extmul_high_i8x16_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 422;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_convert_i32_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 191;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_store16_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 355;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_data_drop:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 257;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_len:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 232;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_store8:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 73;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_store8:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 71;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_call_ref:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 40;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_trunc_sat_f32_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 249;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_lt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 85;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_ge:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 109;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_le_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 90;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_abs:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 152;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_local_set:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 47;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_store32:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 75;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_import:
        {
            int c = 0 + 8;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 3;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_shl:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 433;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_local_get:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 46;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_shr_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 131;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_promote_low_f32x4:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 25;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 361;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_global_get:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 49;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_any_convert_extern:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 243;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_relaxed_nmadd:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 510;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_rotl:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 132;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_sqrt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 172;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_trunc_f64_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 184;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_pmin:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 492;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_load:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 55;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_trunc_sat_f64_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 255;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_extmul_high_i32x4_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 471;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_popcnt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 136;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_ge_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 103;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_add:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 137;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_relaxed_trunc_f32x4_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 29;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 503;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_load32_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 66;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_sub:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 138;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_store:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 69;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_mul:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 139;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_extend_high_i32x4_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 27;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 455;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_div_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 140;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_select:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 43;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_rem_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 142;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extadd_pairwise_i8x16_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 31;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 390;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_new_default:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 224;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_sub:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 411;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_return_call_ref:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 41;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_and:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 144;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_add:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 159;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_sub:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 174;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_xor:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 146;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_shr_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 149;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_load16_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 59;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_rotl:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 150;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_bitmask:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 366;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_rotr:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 151;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_neg:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 153;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_ceil:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 154;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_relaxed_swizzle:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 23;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 502;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_elem_drop:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 261;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_ed_mem:
        {
            int c = 0 + 8;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 22;
                closure_externdesc(p, c, node);
            }
        }
        break;
    case BURG_I_f32_floor:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 155;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_relaxed_max:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 516;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_le_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 89;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_trunc:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 156;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_sqrt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 158;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_gt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 334;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_subx:
        {
            int c = 0 + 5;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 15;
                closure_subtype(p, c, node);
            }
        }
        break;
    case BURG_I_f32_sub:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 160;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_or:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 8;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 145;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i31_get_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 246;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_gt_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 98;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_mul:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 161;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_convert_i64_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 198;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_relaxed_trunc_f32x4_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 29;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 504;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_W_rec:
        {
            int c = 0 + 5;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 2;
                closure_decl(p, c, node);
            }
        }
        break;
    case BURG_I_f32_div:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 162;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_extend_low_i16x8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 429;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_table_init:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 260;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_floor:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 169;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_bitselect:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 348;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_reinterpret_i64:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 204;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_trunc:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 170;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_nearest:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 171;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_copy:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 234;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_mul:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 175;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_replace_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 300;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_ref_func:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 212;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_wrap_i64:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 180;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_trunc_f64_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 183;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_ge_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 91;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_extend_i32_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 18;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 186;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_sub_sat_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 381;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_global_set:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 50;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_ref_null:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 210;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_struct_new_default:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 218;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_table_size:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 264;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_extend_i32_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 18;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 185;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_trunc_f32_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 187;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load64_zero:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 18;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 359;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_ne:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 322;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_trunc_f32_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 188;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_convert_i32_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 192;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_convert_i64_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 194;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_rem_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 143;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_convert_i32_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 197;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_convert_i64_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 199;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_promote_f32:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 200;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_reinterpret_f64:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 202;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_extend8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 205;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_extend16_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 206;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_le_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 317;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_convert_i64_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 19;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 193;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_extend16_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 208;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_convert_i32x4_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 23;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 496;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_lt:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 333;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_br_on_non_null:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 216;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_extract_lane:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 295;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_struct_get_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 221;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_trunc_f32_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 182;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_struct_set:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 222;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32x4_relaxed_trunc_f64x2_u_zero:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 34;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 506;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64_div:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 9;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 176;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_extend32_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 209;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_new:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 223;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_new_data:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 226;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_neg:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 449;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i16x8_extadd_pairwise_i8x16_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 31;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 391;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f64x2_floor:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 383;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_new_elem:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 16;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 227;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_gt_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 88;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_get:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 228;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i8x16_avgr_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 389;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_struct_get_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 14;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 220;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_get_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 13;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 230;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_extend8_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 15;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 207;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_set:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 231;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_init_data:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 235;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_sub:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 476;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_array_init_elem:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 236;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32x4_ge:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 336;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_const:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 278;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_v128_load32x2_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 271;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_ref_test:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 237;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_ref_test_x15:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 238;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_relaxed_laneselect:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 26;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 514;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_br_on_cast:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 12;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 241;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64x2_abs:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 11;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 448;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_br_on_cast_fail:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 17;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 242;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_extern_convert_any:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 20;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 244;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_trunc_sat_f64_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 251;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_trunc_sat_f32_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 248;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_f32_reinterpret_i32:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 203;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_trunc_sat_f64_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 250;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i32_ge_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 10;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 92;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_trunc_sat_f32_u:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 253;
                closure_instr(p, c, node);
            }
        }
        break;
    case BURG_I_i64_trunc_sat_f64_s:
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 21;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 254;
                closure_instr(p, c, node);
            }
        }
        break;
    default:
        wat_layout_burg_set_error("burg: unknown opcode in match", op, ctx);
        break;
    }
}

static burg_state_t* burg_label_tree(BURG_NODE_TYPE node, wat_layout_burg_ctx_t* ctx) {
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

static burg_state_t* burg_label(BURG_NODE_TYPE node, wat_layout_burg_ctx_t* ctx) {
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

int wat_layout_burg_rule(burg_state_t* state, int goalnt) {
    if (!state || goalnt < 1 || goalnt > BURG_MAX_NT) return 0;
    return state->rule[goalnt];
}

int wat_layout_burg_cost(burg_state_t* state, int goalnt) {
    if (!state || goalnt < 1 || goalnt > BURG_MAX_NT) return BURG_MAX_COST;
    return state->cost[goalnt];
}

burg_state_t* wat_layout_burg_label_root(BURG_NODE_TYPE root, wat_layout_burg_ctx_t* ctx) {
    if (wat_layout_burg_has_error(ctx)) return NULL;
    arena_reset(ctx);
    return burg_label_tree(root, ctx);
}

const char* wat_layout_burg_nt_name(int nt) {
    static const char* names[] = {
        "<invalid>",
        "grp",
        "decl",
        "subtype",
        "comptype",
        "typeuse",
        "externdesc",
        "instr",
        "operands",
    };
    if (nt >= 1 && nt <= BURG_MAX_NT) return names[nt];
    return names[0];
}

void wat_layout_burg_reduce(BURG_NODE_TYPE node, burg_state_t* state, int goalnt, wat_layout_burg_ctx_t* ctx) {
    if (wat_layout_burg_has_error(ctx)) return;
    int rule = wat_layout_burg_rule(state, goalnt);
    switch (rule) {
    case 1: { // decl: W_type
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 1, ctx);
        break;
    }
    case 2: { // decl: W_rec
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 2, ctx);
        break;
    }
    case 3: { // decl: W_import
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 3, ctx);
        break;
    }
    case 4: { // decl: W_func
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 4, ctx);
        break;
    }
    case 5: { // decl: W_table
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 5, ctx);
        break;
    }
    case 6: { // decl: W_memory
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 6, ctx);
        break;
    }
    case 7: { // decl: W_global
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 7, ctx);
        break;
    }
    case 8: { // decl: W_tag
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 8, ctx);
        break;
    }
    case 9: { // decl: W_export
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 9, ctx);
        break;
    }
    case 10: { // decl: W_start
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 10, ctx);
        break;
    }
    case 11: { // decl: W_elem
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 11, ctx);
        break;
    }
    case 12: { // decl: W_data
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 12, ctx);
        break;
    }
    case 13: { // decl: W_custom
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 13, ctx);
        break;
    }
    case 14: { // subtype: W_sub
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 14, ctx);
        break;
    }
    case 15: { // subtype: W_subx
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 15, ctx);
        break;
    }
    case 16: { // comptype: W_comp_func
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 16, ctx);
        break;
    }
    case 17: { // comptype: W_comp_struct
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 17, ctx);
        break;
    }
    case 18: { // comptype: W_comp_array
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 18, ctx);
        break;
    }
    case 19: { // typeuse: W_typeuse
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 19, ctx);
        break;
    }
    case 20: { // externdesc: W_ed_func
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 20, ctx);
        break;
    }
    case 21: { // externdesc: W_ed_table
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 21, ctx);
        break;
    }
    case 22: { // externdesc: W_ed_mem
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 22, ctx);
        break;
    }
    case 23: { // externdesc: W_ed_global
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 23, ctx);
        break;
    }
    case 24: { // externdesc: W_ed_tag
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 24, ctx);
        break;
    }
    case 25: { // instr: I_unreachable(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 25, ctx);
        break;
    }
    case 26: { // instr: I_nop(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 26, ctx);
        break;
    }
    case 27: { // instr: I_block(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 27, ctx);
        break;
    }
    case 28: { // instr: I_loop(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 28, ctx);
        break;
    }
    case 29: { // instr: I_if(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 29, ctx);
        break;
    }
    case 30: { // instr: I_throw(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 30, ctx);
        break;
    }
    case 31: { // instr: I_throw_ref(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 31, ctx);
        break;
    }
    case 32: { // instr: I_br(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 32, ctx);
        break;
    }
    case 33: { // instr: I_br_if(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 33, ctx);
        break;
    }
    case 34: { // instr: I_br_table(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 34, ctx);
        break;
    }
    case 35: { // instr: I_return(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 35, ctx);
        break;
    }
    case 36: { // instr: I_call(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 36, ctx);
        break;
    }
    case 37: { // instr: I_call_indirect(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 37, ctx);
        break;
    }
    case 38: { // instr: I_return_call(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 38, ctx);
        break;
    }
    case 39: { // instr: I_return_call_indirect(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 39, ctx);
        break;
    }
    case 40: { // instr: I_call_ref(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 40, ctx);
        break;
    }
    case 41: { // instr: I_return_call_ref(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 41, ctx);
        break;
    }
    case 42: { // instr: I_drop(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 42, ctx);
        break;
    }
    case 43: { // instr: I_select(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 43, ctx);
        break;
    }
    case 44: { // instr: I_select_x1c(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 44, ctx);
        break;
    }
    case 45: { // instr: I_try_table(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 45, ctx);
        break;
    }
    case 46: { // instr: I_local_get(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 46, ctx);
        break;
    }
    case 47: { // instr: I_local_set(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 47, ctx);
        break;
    }
    case 48: { // instr: I_local_tee(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 48, ctx);
        break;
    }
    case 49: { // instr: I_global_get(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 49, ctx);
        break;
    }
    case 50: { // instr: I_global_set(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 50, ctx);
        break;
    }
    case 51: { // instr: I_table_get(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 51, ctx);
        break;
    }
    case 52: { // instr: I_table_set(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 52, ctx);
        break;
    }
    case 53: { // instr: I_i32_load(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 53, ctx);
        break;
    }
    case 54: { // instr: I_i64_load(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 54, ctx);
        break;
    }
    case 55: { // instr: I_f32_load(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 55, ctx);
        break;
    }
    case 56: { // instr: I_f64_load(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 56, ctx);
        break;
    }
    case 57: { // instr: I_i32_load8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 57, ctx);
        break;
    }
    case 58: { // instr: I_i32_load8_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 58, ctx);
        break;
    }
    case 59: { // instr: I_i32_load16_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 59, ctx);
        break;
    }
    case 60: { // instr: I_i32_load16_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 60, ctx);
        break;
    }
    case 61: { // instr: I_i64_load8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 61, ctx);
        break;
    }
    case 62: { // instr: I_i64_load8_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 62, ctx);
        break;
    }
    case 63: { // instr: I_i64_load16_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 63, ctx);
        break;
    }
    case 64: { // instr: I_i64_load16_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 64, ctx);
        break;
    }
    case 65: { // instr: I_i64_load32_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 65, ctx);
        break;
    }
    case 66: { // instr: I_i64_load32_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 66, ctx);
        break;
    }
    case 67: { // instr: I_i32_store(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 67, ctx);
        break;
    }
    case 68: { // instr: I_i64_store(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 68, ctx);
        break;
    }
    case 69: { // instr: I_f32_store(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 69, ctx);
        break;
    }
    case 70: { // instr: I_f64_store(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 70, ctx);
        break;
    }
    case 71: { // instr: I_i32_store8(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 71, ctx);
        break;
    }
    case 72: { // instr: I_i32_store16(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 72, ctx);
        break;
    }
    case 73: { // instr: I_i64_store8(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 73, ctx);
        break;
    }
    case 74: { // instr: I_i64_store16(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 74, ctx);
        break;
    }
    case 75: { // instr: I_i64_store32(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 75, ctx);
        break;
    }
    case 76: { // instr: I_memory_size(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 76, ctx);
        break;
    }
    case 77: { // instr: I_memory_grow(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 77, ctx);
        break;
    }
    case 78: { // instr: I_i32_const(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 78, ctx);
        break;
    }
    case 79: { // instr: I_i64_const(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 79, ctx);
        break;
    }
    case 80: { // instr: I_f32_const(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 80, ctx);
        break;
    }
    case 81: { // instr: I_f64_const(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 81, ctx);
        break;
    }
    case 82: { // instr: I_i32_eqz(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 82, ctx);
        break;
    }
    case 83: { // instr: I_i32_eq(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 83, ctx);
        break;
    }
    case 84: { // instr: I_i32_ne(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 84, ctx);
        break;
    }
    case 85: { // instr: I_i32_lt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 85, ctx);
        break;
    }
    case 86: { // instr: I_i32_lt_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 86, ctx);
        break;
    }
    case 87: { // instr: I_i32_gt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 87, ctx);
        break;
    }
    case 88: { // instr: I_i32_gt_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 88, ctx);
        break;
    }
    case 89: { // instr: I_i32_le_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 89, ctx);
        break;
    }
    case 90: { // instr: I_i32_le_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 90, ctx);
        break;
    }
    case 91: { // instr: I_i32_ge_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 91, ctx);
        break;
    }
    case 92: { // instr: I_i32_ge_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 92, ctx);
        break;
    }
    case 93: { // instr: I_i64_eqz(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 93, ctx);
        break;
    }
    case 94: { // instr: I_i64_eq(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 94, ctx);
        break;
    }
    case 95: { // instr: I_i64_ne(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 95, ctx);
        break;
    }
    case 96: { // instr: I_i64_lt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 96, ctx);
        break;
    }
    case 97: { // instr: I_i64_lt_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 97, ctx);
        break;
    }
    case 98: { // instr: I_i64_gt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 98, ctx);
        break;
    }
    case 99: { // instr: I_i64_gt_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 99, ctx);
        break;
    }
    case 100: { // instr: I_i64_le_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 100, ctx);
        break;
    }
    case 101: { // instr: I_i64_le_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 101, ctx);
        break;
    }
    case 102: { // instr: I_i64_ge_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 102, ctx);
        break;
    }
    case 103: { // instr: I_i64_ge_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 103, ctx);
        break;
    }
    case 104: { // instr: I_f32_eq(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 104, ctx);
        break;
    }
    case 105: { // instr: I_f32_ne(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 105, ctx);
        break;
    }
    case 106: { // instr: I_f32_lt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 106, ctx);
        break;
    }
    case 107: { // instr: I_f32_gt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 107, ctx);
        break;
    }
    case 108: { // instr: I_f32_le(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 108, ctx);
        break;
    }
    case 109: { // instr: I_f32_ge(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 109, ctx);
        break;
    }
    case 110: { // instr: I_f64_eq(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 110, ctx);
        break;
    }
    case 111: { // instr: I_f64_ne(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 111, ctx);
        break;
    }
    case 112: { // instr: I_f64_lt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 112, ctx);
        break;
    }
    case 113: { // instr: I_f64_gt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 113, ctx);
        break;
    }
    case 114: { // instr: I_f64_le(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 114, ctx);
        break;
    }
    case 115: { // instr: I_f64_ge(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 115, ctx);
        break;
    }
    case 116: { // instr: I_i32_clz(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 116, ctx);
        break;
    }
    case 117: { // instr: I_i32_ctz(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 117, ctx);
        break;
    }
    case 118: { // instr: I_i32_popcnt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 118, ctx);
        break;
    }
    case 119: { // instr: I_i32_add(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 119, ctx);
        break;
    }
    case 120: { // instr: I_i32_sub(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 120, ctx);
        break;
    }
    case 121: { // instr: I_i32_mul(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 121, ctx);
        break;
    }
    case 122: { // instr: I_i32_div_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 122, ctx);
        break;
    }
    case 123: { // instr: I_i32_div_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 123, ctx);
        break;
    }
    case 124: { // instr: I_i32_rem_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 124, ctx);
        break;
    }
    case 125: { // instr: I_i32_rem_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 125, ctx);
        break;
    }
    case 126: { // instr: I_i32_and(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 126, ctx);
        break;
    }
    case 127: { // instr: I_i32_or(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 127, ctx);
        break;
    }
    case 128: { // instr: I_i32_xor(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 128, ctx);
        break;
    }
    case 129: { // instr: I_i32_shl(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 129, ctx);
        break;
    }
    case 130: { // instr: I_i32_shr_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 130, ctx);
        break;
    }
    case 131: { // instr: I_i32_shr_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 131, ctx);
        break;
    }
    case 132: { // instr: I_i32_rotl(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 132, ctx);
        break;
    }
    case 133: { // instr: I_i32_rotr(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 133, ctx);
        break;
    }
    case 134: { // instr: I_i64_clz(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 134, ctx);
        break;
    }
    case 135: { // instr: I_i64_ctz(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 135, ctx);
        break;
    }
    case 136: { // instr: I_i64_popcnt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 136, ctx);
        break;
    }
    case 137: { // instr: I_i64_add(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 137, ctx);
        break;
    }
    case 138: { // instr: I_i64_sub(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 138, ctx);
        break;
    }
    case 139: { // instr: I_i64_mul(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 139, ctx);
        break;
    }
    case 140: { // instr: I_i64_div_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 140, ctx);
        break;
    }
    case 141: { // instr: I_i64_div_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 141, ctx);
        break;
    }
    case 142: { // instr: I_i64_rem_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 142, ctx);
        break;
    }
    case 143: { // instr: I_i64_rem_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 143, ctx);
        break;
    }
    case 144: { // instr: I_i64_and(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 144, ctx);
        break;
    }
    case 145: { // instr: I_i64_or(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 145, ctx);
        break;
    }
    case 146: { // instr: I_i64_xor(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 146, ctx);
        break;
    }
    case 147: { // instr: I_i64_shl(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 147, ctx);
        break;
    }
    case 148: { // instr: I_i64_shr_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 148, ctx);
        break;
    }
    case 149: { // instr: I_i64_shr_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 149, ctx);
        break;
    }
    case 150: { // instr: I_i64_rotl(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 150, ctx);
        break;
    }
    case 151: { // instr: I_i64_rotr(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 151, ctx);
        break;
    }
    case 152: { // instr: I_f32_abs(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 152, ctx);
        break;
    }
    case 153: { // instr: I_f32_neg(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 153, ctx);
        break;
    }
    case 154: { // instr: I_f32_ceil(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 154, ctx);
        break;
    }
    case 155: { // instr: I_f32_floor(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 155, ctx);
        break;
    }
    case 156: { // instr: I_f32_trunc(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 156, ctx);
        break;
    }
    case 157: { // instr: I_f32_nearest(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 157, ctx);
        break;
    }
    case 158: { // instr: I_f32_sqrt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 158, ctx);
        break;
    }
    case 159: { // instr: I_f32_add(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 159, ctx);
        break;
    }
    case 160: { // instr: I_f32_sub(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 160, ctx);
        break;
    }
    case 161: { // instr: I_f32_mul(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 161, ctx);
        break;
    }
    case 162: { // instr: I_f32_div(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 162, ctx);
        break;
    }
    case 163: { // instr: I_f32_min(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 163, ctx);
        break;
    }
    case 164: { // instr: I_f32_max(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 164, ctx);
        break;
    }
    case 165: { // instr: I_f32_copysign(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 165, ctx);
        break;
    }
    case 166: { // instr: I_f64_abs(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 166, ctx);
        break;
    }
    case 167: { // instr: I_f64_neg(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 167, ctx);
        break;
    }
    case 168: { // instr: I_f64_ceil(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 168, ctx);
        break;
    }
    case 169: { // instr: I_f64_floor(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 169, ctx);
        break;
    }
    case 170: { // instr: I_f64_trunc(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 170, ctx);
        break;
    }
    case 171: { // instr: I_f64_nearest(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 171, ctx);
        break;
    }
    case 172: { // instr: I_f64_sqrt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 172, ctx);
        break;
    }
    case 173: { // instr: I_f64_add(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 173, ctx);
        break;
    }
    case 174: { // instr: I_f64_sub(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 174, ctx);
        break;
    }
    case 175: { // instr: I_f64_mul(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 175, ctx);
        break;
    }
    case 176: { // instr: I_f64_div(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 176, ctx);
        break;
    }
    case 177: { // instr: I_f64_min(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 177, ctx);
        break;
    }
    case 178: { // instr: I_f64_max(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 178, ctx);
        break;
    }
    case 179: { // instr: I_f64_copysign(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 179, ctx);
        break;
    }
    case 180: { // instr: I_i32_wrap_i64(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 180, ctx);
        break;
    }
    case 181: { // instr: I_i32_trunc_f32_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 181, ctx);
        break;
    }
    case 182: { // instr: I_i32_trunc_f32_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 182, ctx);
        break;
    }
    case 183: { // instr: I_i32_trunc_f64_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 183, ctx);
        break;
    }
    case 184: { // instr: I_i32_trunc_f64_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 184, ctx);
        break;
    }
    case 185: { // instr: I_i64_extend_i32_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 185, ctx);
        break;
    }
    case 186: { // instr: I_i64_extend_i32_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 186, ctx);
        break;
    }
    case 187: { // instr: I_i64_trunc_f32_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 187, ctx);
        break;
    }
    case 188: { // instr: I_i64_trunc_f32_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 188, ctx);
        break;
    }
    case 189: { // instr: I_i64_trunc_f64_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 189, ctx);
        break;
    }
    case 190: { // instr: I_i64_trunc_f64_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 190, ctx);
        break;
    }
    case 191: { // instr: I_f32_convert_i32_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 191, ctx);
        break;
    }
    case 192: { // instr: I_f32_convert_i32_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 192, ctx);
        break;
    }
    case 193: { // instr: I_f32_convert_i64_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 193, ctx);
        break;
    }
    case 194: { // instr: I_f32_convert_i64_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 194, ctx);
        break;
    }
    case 195: { // instr: I_f32_demote_f64(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 195, ctx);
        break;
    }
    case 196: { // instr: I_f64_convert_i32_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 196, ctx);
        break;
    }
    case 197: { // instr: I_f64_convert_i32_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 197, ctx);
        break;
    }
    case 198: { // instr: I_f64_convert_i64_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 198, ctx);
        break;
    }
    case 199: { // instr: I_f64_convert_i64_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 199, ctx);
        break;
    }
    case 200: { // instr: I_f64_promote_f32(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 200, ctx);
        break;
    }
    case 201: { // instr: I_i32_reinterpret_f32(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 201, ctx);
        break;
    }
    case 202: { // instr: I_i64_reinterpret_f64(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 202, ctx);
        break;
    }
    case 203: { // instr: I_f32_reinterpret_i32(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 203, ctx);
        break;
    }
    case 204: { // instr: I_f64_reinterpret_i64(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 204, ctx);
        break;
    }
    case 205: { // instr: I_i32_extend8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 205, ctx);
        break;
    }
    case 206: { // instr: I_i32_extend16_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 206, ctx);
        break;
    }
    case 207: { // instr: I_i64_extend8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 207, ctx);
        break;
    }
    case 208: { // instr: I_i64_extend16_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 208, ctx);
        break;
    }
    case 209: { // instr: I_i64_extend32_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 209, ctx);
        break;
    }
    case 210: { // instr: I_ref_null(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 210, ctx);
        break;
    }
    case 211: { // instr: I_ref_is_null(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 211, ctx);
        break;
    }
    case 212: { // instr: I_ref_func(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 212, ctx);
        break;
    }
    case 213: { // instr: I_ref_eq(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 213, ctx);
        break;
    }
    case 214: { // instr: I_ref_as_non_null(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 214, ctx);
        break;
    }
    case 215: { // instr: I_br_on_null(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 215, ctx);
        break;
    }
    case 216: { // instr: I_br_on_non_null(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 216, ctx);
        break;
    }
    case 217: { // instr: I_struct_new(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 217, ctx);
        break;
    }
    case 218: { // instr: I_struct_new_default(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 218, ctx);
        break;
    }
    case 219: { // instr: I_struct_get(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 219, ctx);
        break;
    }
    case 220: { // instr: I_struct_get_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 220, ctx);
        break;
    }
    case 221: { // instr: I_struct_get_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 221, ctx);
        break;
    }
    case 222: { // instr: I_struct_set(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 222, ctx);
        break;
    }
    case 223: { // instr: I_array_new(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 223, ctx);
        break;
    }
    case 224: { // instr: I_array_new_default(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 224, ctx);
        break;
    }
    case 225: { // instr: I_array_new_fixed(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 225, ctx);
        break;
    }
    case 226: { // instr: I_array_new_data(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 226, ctx);
        break;
    }
    case 227: { // instr: I_array_new_elem(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 227, ctx);
        break;
    }
    case 228: { // instr: I_array_get(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 228, ctx);
        break;
    }
    case 229: { // instr: I_array_get_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 229, ctx);
        break;
    }
    case 230: { // instr: I_array_get_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 230, ctx);
        break;
    }
    case 231: { // instr: I_array_set(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 231, ctx);
        break;
    }
    case 232: { // instr: I_array_len(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 232, ctx);
        break;
    }
    case 233: { // instr: I_array_fill(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 233, ctx);
        break;
    }
    case 234: { // instr: I_array_copy(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 234, ctx);
        break;
    }
    case 235: { // instr: I_array_init_data(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 235, ctx);
        break;
    }
    case 236: { // instr: I_array_init_elem(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 236, ctx);
        break;
    }
    case 237: { // instr: I_ref_test(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 237, ctx);
        break;
    }
    case 238: { // instr: I_ref_test_x15(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 238, ctx);
        break;
    }
    case 239: { // instr: I_ref_cast(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 239, ctx);
        break;
    }
    case 240: { // instr: I_ref_cast_x17(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 240, ctx);
        break;
    }
    case 241: { // instr: I_br_on_cast(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 241, ctx);
        break;
    }
    case 242: { // instr: I_br_on_cast_fail(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 242, ctx);
        break;
    }
    case 243: { // instr: I_any_convert_extern(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 243, ctx);
        break;
    }
    case 244: { // instr: I_extern_convert_any(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 244, ctx);
        break;
    }
    case 245: { // instr: I_ref_i31(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 245, ctx);
        break;
    }
    case 246: { // instr: I_i31_get_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 246, ctx);
        break;
    }
    case 247: { // instr: I_i31_get_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 247, ctx);
        break;
    }
    case 248: { // instr: I_i32_trunc_sat_f32_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 248, ctx);
        break;
    }
    case 249: { // instr: I_i32_trunc_sat_f32_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 249, ctx);
        break;
    }
    case 250: { // instr: I_i32_trunc_sat_f64_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 250, ctx);
        break;
    }
    case 251: { // instr: I_i32_trunc_sat_f64_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 251, ctx);
        break;
    }
    case 252: { // instr: I_i64_trunc_sat_f32_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 252, ctx);
        break;
    }
    case 253: { // instr: I_i64_trunc_sat_f32_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 253, ctx);
        break;
    }
    case 254: { // instr: I_i64_trunc_sat_f64_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 254, ctx);
        break;
    }
    case 255: { // instr: I_i64_trunc_sat_f64_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 255, ctx);
        break;
    }
    case 256: { // instr: I_memory_init(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 256, ctx);
        break;
    }
    case 257: { // instr: I_data_drop(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 257, ctx);
        break;
    }
    case 258: { // instr: I_memory_copy(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 258, ctx);
        break;
    }
    case 259: { // instr: I_memory_fill(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 259, ctx);
        break;
    }
    case 260: { // instr: I_table_init(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 260, ctx);
        break;
    }
    case 261: { // instr: I_elem_drop(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 261, ctx);
        break;
    }
    case 262: { // instr: I_table_copy(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 262, ctx);
        break;
    }
    case 263: { // instr: I_table_grow(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 263, ctx);
        break;
    }
    case 264: { // instr: I_table_size(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 264, ctx);
        break;
    }
    case 265: { // instr: I_table_fill(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 265, ctx);
        break;
    }
    case 266: { // instr: I_v128_load(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 266, ctx);
        break;
    }
    case 267: { // instr: I_v128_load8x8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 267, ctx);
        break;
    }
    case 268: { // instr: I_v128_load8x8_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 268, ctx);
        break;
    }
    case 269: { // instr: I_v128_load16x4_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 269, ctx);
        break;
    }
    case 270: { // instr: I_v128_load16x4_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 270, ctx);
        break;
    }
    case 271: { // instr: I_v128_load32x2_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 271, ctx);
        break;
    }
    case 272: { // instr: I_v128_load32x2_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 272, ctx);
        break;
    }
    case 273: { // instr: I_v128_load8_splat(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 273, ctx);
        break;
    }
    case 274: { // instr: I_v128_load16_splat(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 274, ctx);
        break;
    }
    case 275: { // instr: I_v128_load32_splat(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 275, ctx);
        break;
    }
    case 276: { // instr: I_v128_load64_splat(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 276, ctx);
        break;
    }
    case 277: { // instr: I_v128_store(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 277, ctx);
        break;
    }
    case 278: { // instr: I_v128_const(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 278, ctx);
        break;
    }
    case 279: { // instr: I_i8x16_shuffle(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 279, ctx);
        break;
    }
    case 280: { // instr: I_i8x16_swizzle(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 280, ctx);
        break;
    }
    case 281: { // instr: I_i8x16_splat(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 281, ctx);
        break;
    }
    case 282: { // instr: I_i16x8_splat(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 282, ctx);
        break;
    }
    case 283: { // instr: I_i32x4_splat(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 283, ctx);
        break;
    }
    case 284: { // instr: I_i64x2_splat(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 284, ctx);
        break;
    }
    case 285: { // instr: I_f32x4_splat(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 285, ctx);
        break;
    }
    case 286: { // instr: I_f64x2_splat(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 286, ctx);
        break;
    }
    case 287: { // instr: I_i8x16_extract_lane_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 287, ctx);
        break;
    }
    case 288: { // instr: I_i8x16_extract_lane_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 288, ctx);
        break;
    }
    case 289: { // instr: I_i8x16_replace_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 289, ctx);
        break;
    }
    case 290: { // instr: I_i16x8_extract_lane_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 290, ctx);
        break;
    }
    case 291: { // instr: I_i16x8_extract_lane_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 291, ctx);
        break;
    }
    case 292: { // instr: I_i16x8_replace_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 292, ctx);
        break;
    }
    case 293: { // instr: I_i32x4_extract_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 293, ctx);
        break;
    }
    case 294: { // instr: I_i32x4_replace_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 294, ctx);
        break;
    }
    case 295: { // instr: I_i64x2_extract_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 295, ctx);
        break;
    }
    case 296: { // instr: I_i64x2_replace_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 296, ctx);
        break;
    }
    case 297: { // instr: I_f32x4_extract_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 297, ctx);
        break;
    }
    case 298: { // instr: I_f32x4_replace_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 298, ctx);
        break;
    }
    case 299: { // instr: I_f64x2_extract_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 299, ctx);
        break;
    }
    case 300: { // instr: I_f64x2_replace_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 300, ctx);
        break;
    }
    case 301: { // instr: I_i8x16_eq(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 301, ctx);
        break;
    }
    case 302: { // instr: I_i8x16_ne(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 302, ctx);
        break;
    }
    case 303: { // instr: I_i8x16_lt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 303, ctx);
        break;
    }
    case 304: { // instr: I_i8x16_lt_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 304, ctx);
        break;
    }
    case 305: { // instr: I_i8x16_gt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 305, ctx);
        break;
    }
    case 306: { // instr: I_i8x16_gt_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 306, ctx);
        break;
    }
    case 307: { // instr: I_i8x16_le_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 307, ctx);
        break;
    }
    case 308: { // instr: I_i8x16_le_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 308, ctx);
        break;
    }
    case 309: { // instr: I_i8x16_ge_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 309, ctx);
        break;
    }
    case 310: { // instr: I_i8x16_ge_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 310, ctx);
        break;
    }
    case 311: { // instr: I_i16x8_eq(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 311, ctx);
        break;
    }
    case 312: { // instr: I_i16x8_ne(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 312, ctx);
        break;
    }
    case 313: { // instr: I_i16x8_lt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 313, ctx);
        break;
    }
    case 314: { // instr: I_i16x8_lt_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 314, ctx);
        break;
    }
    case 315: { // instr: I_i16x8_gt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 315, ctx);
        break;
    }
    case 316: { // instr: I_i16x8_gt_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 316, ctx);
        break;
    }
    case 317: { // instr: I_i16x8_le_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 317, ctx);
        break;
    }
    case 318: { // instr: I_i16x8_le_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 318, ctx);
        break;
    }
    case 319: { // instr: I_i16x8_ge_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 319, ctx);
        break;
    }
    case 320: { // instr: I_i16x8_ge_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 320, ctx);
        break;
    }
    case 321: { // instr: I_i32x4_eq(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 321, ctx);
        break;
    }
    case 322: { // instr: I_i32x4_ne(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 322, ctx);
        break;
    }
    case 323: { // instr: I_i32x4_lt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 323, ctx);
        break;
    }
    case 324: { // instr: I_i32x4_lt_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 324, ctx);
        break;
    }
    case 325: { // instr: I_i32x4_gt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 325, ctx);
        break;
    }
    case 326: { // instr: I_i32x4_gt_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 326, ctx);
        break;
    }
    case 327: { // instr: I_i32x4_le_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 327, ctx);
        break;
    }
    case 328: { // instr: I_i32x4_le_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 328, ctx);
        break;
    }
    case 329: { // instr: I_i32x4_ge_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 329, ctx);
        break;
    }
    case 330: { // instr: I_i32x4_ge_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 330, ctx);
        break;
    }
    case 331: { // instr: I_f32x4_eq(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 331, ctx);
        break;
    }
    case 332: { // instr: I_f32x4_ne(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 332, ctx);
        break;
    }
    case 333: { // instr: I_f32x4_lt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 333, ctx);
        break;
    }
    case 334: { // instr: I_f32x4_gt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 334, ctx);
        break;
    }
    case 335: { // instr: I_f32x4_le(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 335, ctx);
        break;
    }
    case 336: { // instr: I_f32x4_ge(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 336, ctx);
        break;
    }
    case 337: { // instr: I_f64x2_eq(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 337, ctx);
        break;
    }
    case 338: { // instr: I_f64x2_ne(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 338, ctx);
        break;
    }
    case 339: { // instr: I_f64x2_lt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 339, ctx);
        break;
    }
    case 340: { // instr: I_f64x2_gt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 340, ctx);
        break;
    }
    case 341: { // instr: I_f64x2_le(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 341, ctx);
        break;
    }
    case 342: { // instr: I_f64x2_ge(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 342, ctx);
        break;
    }
    case 343: { // instr: I_v128_not(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 343, ctx);
        break;
    }
    case 344: { // instr: I_v128_and(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 344, ctx);
        break;
    }
    case 345: { // instr: I_v128_andnot(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 345, ctx);
        break;
    }
    case 346: { // instr: I_v128_or(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 346, ctx);
        break;
    }
    case 347: { // instr: I_v128_xor(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 347, ctx);
        break;
    }
    case 348: { // instr: I_v128_bitselect(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 348, ctx);
        break;
    }
    case 349: { // instr: I_v128_any_true(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 349, ctx);
        break;
    }
    case 350: { // instr: I_v128_load8_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 350, ctx);
        break;
    }
    case 351: { // instr: I_v128_load16_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 351, ctx);
        break;
    }
    case 352: { // instr: I_v128_load32_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 352, ctx);
        break;
    }
    case 353: { // instr: I_v128_load64_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 353, ctx);
        break;
    }
    case 354: { // instr: I_v128_store8_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 354, ctx);
        break;
    }
    case 355: { // instr: I_v128_store16_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 355, ctx);
        break;
    }
    case 356: { // instr: I_v128_store32_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 356, ctx);
        break;
    }
    case 357: { // instr: I_v128_store64_lane(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 357, ctx);
        break;
    }
    case 358: { // instr: I_v128_load32_zero(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 358, ctx);
        break;
    }
    case 359: { // instr: I_v128_load64_zero(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 359, ctx);
        break;
    }
    case 360: { // instr: I_f32x4_demote_f64x2_zero(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 360, ctx);
        break;
    }
    case 361: { // instr: I_f64x2_promote_low_f32x4(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 361, ctx);
        break;
    }
    case 362: { // instr: I_i8x16_abs(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 362, ctx);
        break;
    }
    case 363: { // instr: I_i8x16_neg(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 363, ctx);
        break;
    }
    case 364: { // instr: I_i8x16_popcnt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 364, ctx);
        break;
    }
    case 365: { // instr: I_i8x16_all_true(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 365, ctx);
        break;
    }
    case 366: { // instr: I_i8x16_bitmask(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 366, ctx);
        break;
    }
    case 367: { // instr: I_i8x16_narrow_i16x8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 367, ctx);
        break;
    }
    case 368: { // instr: I_i8x16_narrow_i16x8_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 368, ctx);
        break;
    }
    case 369: { // instr: I_f32x4_ceil(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 369, ctx);
        break;
    }
    case 370: { // instr: I_f32x4_floor(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 370, ctx);
        break;
    }
    case 371: { // instr: I_f32x4_trunc(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 371, ctx);
        break;
    }
    case 372: { // instr: I_f32x4_nearest(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 372, ctx);
        break;
    }
    case 373: { // instr: I_i8x16_shl(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 373, ctx);
        break;
    }
    case 374: { // instr: I_i8x16_shr_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 374, ctx);
        break;
    }
    case 375: { // instr: I_i8x16_shr_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 375, ctx);
        break;
    }
    case 376: { // instr: I_i8x16_add(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 376, ctx);
        break;
    }
    case 377: { // instr: I_i8x16_add_sat_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 377, ctx);
        break;
    }
    case 378: { // instr: I_i8x16_add_sat_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 378, ctx);
        break;
    }
    case 379: { // instr: I_i8x16_sub(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 379, ctx);
        break;
    }
    case 380: { // instr: I_i8x16_sub_sat_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 380, ctx);
        break;
    }
    case 381: { // instr: I_i8x16_sub_sat_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 381, ctx);
        break;
    }
    case 382: { // instr: I_f64x2_ceil(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 382, ctx);
        break;
    }
    case 383: { // instr: I_f64x2_floor(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 383, ctx);
        break;
    }
    case 384: { // instr: I_i8x16_min_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 384, ctx);
        break;
    }
    case 385: { // instr: I_i8x16_min_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 385, ctx);
        break;
    }
    case 386: { // instr: I_i8x16_max_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 386, ctx);
        break;
    }
    case 387: { // instr: I_i8x16_max_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 387, ctx);
        break;
    }
    case 388: { // instr: I_f64x2_trunc(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 388, ctx);
        break;
    }
    case 389: { // instr: I_i8x16_avgr_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 389, ctx);
        break;
    }
    case 390: { // instr: I_i16x8_extadd_pairwise_i8x16_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 390, ctx);
        break;
    }
    case 391: { // instr: I_i16x8_extadd_pairwise_i8x16_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 391, ctx);
        break;
    }
    case 392: { // instr: I_i32x4_extadd_pairwise_i16x8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 392, ctx);
        break;
    }
    case 393: { // instr: I_i32x4_extadd_pairwise_i16x8_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 393, ctx);
        break;
    }
    case 394: { // instr: I_i16x8_abs(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 394, ctx);
        break;
    }
    case 395: { // instr: I_i16x8_neg(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 395, ctx);
        break;
    }
    case 396: { // instr: I_i16x8_q15mulr_sat_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 396, ctx);
        break;
    }
    case 397: { // instr: I_i16x8_all_true(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 397, ctx);
        break;
    }
    case 398: { // instr: I_i16x8_bitmask(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 398, ctx);
        break;
    }
    case 399: { // instr: I_i16x8_narrow_i32x4_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 399, ctx);
        break;
    }
    case 400: { // instr: I_i16x8_narrow_i32x4_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 400, ctx);
        break;
    }
    case 401: { // instr: I_i16x8_extend_low_i8x16_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 401, ctx);
        break;
    }
    case 402: { // instr: I_i16x8_extend_high_i8x16_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 402, ctx);
        break;
    }
    case 403: { // instr: I_i16x8_extend_low_i8x16_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 403, ctx);
        break;
    }
    case 404: { // instr: I_i16x8_extend_high_i8x16_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 404, ctx);
        break;
    }
    case 405: { // instr: I_i16x8_shl(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 405, ctx);
        break;
    }
    case 406: { // instr: I_i16x8_shr_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 406, ctx);
        break;
    }
    case 407: { // instr: I_i16x8_shr_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 407, ctx);
        break;
    }
    case 408: { // instr: I_i16x8_add(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 408, ctx);
        break;
    }
    case 409: { // instr: I_i16x8_add_sat_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 409, ctx);
        break;
    }
    case 410: { // instr: I_i16x8_add_sat_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 410, ctx);
        break;
    }
    case 411: { // instr: I_i16x8_sub(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 411, ctx);
        break;
    }
    case 412: { // instr: I_i16x8_sub_sat_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 412, ctx);
        break;
    }
    case 413: { // instr: I_i16x8_sub_sat_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 413, ctx);
        break;
    }
    case 414: { // instr: I_f64x2_nearest(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 414, ctx);
        break;
    }
    case 415: { // instr: I_i16x8_mul(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 415, ctx);
        break;
    }
    case 416: { // instr: I_i16x8_min_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 416, ctx);
        break;
    }
    case 417: { // instr: I_i16x8_min_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 417, ctx);
        break;
    }
    case 418: { // instr: I_i16x8_max_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 418, ctx);
        break;
    }
    case 419: { // instr: I_i16x8_max_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 419, ctx);
        break;
    }
    case 420: { // instr: I_i16x8_avgr_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 420, ctx);
        break;
    }
    case 421: { // instr: I_i16x8_extmul_low_i8x16_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 421, ctx);
        break;
    }
    case 422: { // instr: I_i16x8_extmul_high_i8x16_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 422, ctx);
        break;
    }
    case 423: { // instr: I_i16x8_extmul_low_i8x16_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 423, ctx);
        break;
    }
    case 424: { // instr: I_i16x8_extmul_high_i8x16_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 424, ctx);
        break;
    }
    case 425: { // instr: I_i32x4_abs(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 425, ctx);
        break;
    }
    case 426: { // instr: I_i32x4_neg(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 426, ctx);
        break;
    }
    case 427: { // instr: I_i32x4_all_true(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 427, ctx);
        break;
    }
    case 428: { // instr: I_i32x4_bitmask(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 428, ctx);
        break;
    }
    case 429: { // instr: I_i32x4_extend_low_i16x8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 429, ctx);
        break;
    }
    case 430: { // instr: I_i32x4_extend_high_i16x8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 430, ctx);
        break;
    }
    case 431: { // instr: I_i32x4_extend_low_i16x8_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 431, ctx);
        break;
    }
    case 432: { // instr: I_i32x4_extend_high_i16x8_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 432, ctx);
        break;
    }
    case 433: { // instr: I_i32x4_shl(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 433, ctx);
        break;
    }
    case 434: { // instr: I_i32x4_shr_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 434, ctx);
        break;
    }
    case 435: { // instr: I_i32x4_shr_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 435, ctx);
        break;
    }
    case 436: { // instr: I_i32x4_add(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 436, ctx);
        break;
    }
    case 437: { // instr: I_i32x4_sub(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 437, ctx);
        break;
    }
    case 438: { // instr: I_i32x4_mul(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 438, ctx);
        break;
    }
    case 439: { // instr: I_i32x4_min_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 439, ctx);
        break;
    }
    case 440: { // instr: I_i32x4_min_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 440, ctx);
        break;
    }
    case 441: { // instr: I_i32x4_max_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 441, ctx);
        break;
    }
    case 442: { // instr: I_i32x4_max_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 442, ctx);
        break;
    }
    case 443: { // instr: I_i32x4_dot_i16x8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 443, ctx);
        break;
    }
    case 444: { // instr: I_i32x4_extmul_low_i16x8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 444, ctx);
        break;
    }
    case 445: { // instr: I_i32x4_extmul_high_i16x8_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 445, ctx);
        break;
    }
    case 446: { // instr: I_i32x4_extmul_low_i16x8_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 446, ctx);
        break;
    }
    case 447: { // instr: I_i32x4_extmul_high_i16x8_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 447, ctx);
        break;
    }
    case 448: { // instr: I_i64x2_abs(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 448, ctx);
        break;
    }
    case 449: { // instr: I_i64x2_neg(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 449, ctx);
        break;
    }
    case 450: { // instr: I_i64x2_all_true(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 450, ctx);
        break;
    }
    case 451: { // instr: I_i64x2_bitmask(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 451, ctx);
        break;
    }
    case 452: { // instr: I_i64x2_extend_low_i32x4_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 452, ctx);
        break;
    }
    case 453: { // instr: I_i64x2_extend_high_i32x4_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 453, ctx);
        break;
    }
    case 454: { // instr: I_i64x2_extend_low_i32x4_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 454, ctx);
        break;
    }
    case 455: { // instr: I_i64x2_extend_high_i32x4_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 455, ctx);
        break;
    }
    case 456: { // instr: I_i64x2_shl(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 456, ctx);
        break;
    }
    case 457: { // instr: I_i64x2_shr_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 457, ctx);
        break;
    }
    case 458: { // instr: I_i64x2_shr_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 458, ctx);
        break;
    }
    case 459: { // instr: I_i64x2_add(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 459, ctx);
        break;
    }
    case 460: { // instr: I_i64x2_sub(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 460, ctx);
        break;
    }
    case 461: { // instr: I_i64x2_mul(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 461, ctx);
        break;
    }
    case 462: { // instr: I_i64x2_eq(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 462, ctx);
        break;
    }
    case 463: { // instr: I_i64x2_ne(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 463, ctx);
        break;
    }
    case 464: { // instr: I_i64x2_lt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 464, ctx);
        break;
    }
    case 465: { // instr: I_i64x2_gt_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 465, ctx);
        break;
    }
    case 466: { // instr: I_i64x2_le_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 466, ctx);
        break;
    }
    case 467: { // instr: I_i64x2_ge_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 467, ctx);
        break;
    }
    case 468: { // instr: I_i64x2_extmul_low_i32x4_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 468, ctx);
        break;
    }
    case 469: { // instr: I_i64x2_extmul_high_i32x4_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 469, ctx);
        break;
    }
    case 470: { // instr: I_i64x2_extmul_low_i32x4_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 470, ctx);
        break;
    }
    case 471: { // instr: I_i64x2_extmul_high_i32x4_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 471, ctx);
        break;
    }
    case 472: { // instr: I_f32x4_abs(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 472, ctx);
        break;
    }
    case 473: { // instr: I_f32x4_neg(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 473, ctx);
        break;
    }
    case 474: { // instr: I_f32x4_sqrt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 474, ctx);
        break;
    }
    case 475: { // instr: I_f32x4_add(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 475, ctx);
        break;
    }
    case 476: { // instr: I_f32x4_sub(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 476, ctx);
        break;
    }
    case 477: { // instr: I_f32x4_mul(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 477, ctx);
        break;
    }
    case 478: { // instr: I_f32x4_div(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 478, ctx);
        break;
    }
    case 479: { // instr: I_f32x4_min(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 479, ctx);
        break;
    }
    case 480: { // instr: I_f32x4_max(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 480, ctx);
        break;
    }
    case 481: { // instr: I_f32x4_pmin(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 481, ctx);
        break;
    }
    case 482: { // instr: I_f32x4_pmax(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 482, ctx);
        break;
    }
    case 483: { // instr: I_f64x2_abs(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 483, ctx);
        break;
    }
    case 484: { // instr: I_f64x2_neg(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 484, ctx);
        break;
    }
    case 485: { // instr: I_f64x2_sqrt(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 485, ctx);
        break;
    }
    case 486: { // instr: I_f64x2_add(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 486, ctx);
        break;
    }
    case 487: { // instr: I_f64x2_sub(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 487, ctx);
        break;
    }
    case 488: { // instr: I_f64x2_mul(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 488, ctx);
        break;
    }
    case 489: { // instr: I_f64x2_div(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 489, ctx);
        break;
    }
    case 490: { // instr: I_f64x2_min(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 490, ctx);
        break;
    }
    case 491: { // instr: I_f64x2_max(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 491, ctx);
        break;
    }
    case 492: { // instr: I_f64x2_pmin(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 492, ctx);
        break;
    }
    case 493: { // instr: I_f64x2_pmax(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 493, ctx);
        break;
    }
    case 494: { // instr: I_i32x4_trunc_sat_f32x4_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 494, ctx);
        break;
    }
    case 495: { // instr: I_i32x4_trunc_sat_f32x4_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 495, ctx);
        break;
    }
    case 496: { // instr: I_f32x4_convert_i32x4_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 496, ctx);
        break;
    }
    case 497: { // instr: I_f32x4_convert_i32x4_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 497, ctx);
        break;
    }
    case 498: { // instr: I_i32x4_trunc_sat_f64x2_s_zero(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 498, ctx);
        break;
    }
    case 499: { // instr: I_i32x4_trunc_sat_f64x2_u_zero(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 499, ctx);
        break;
    }
    case 500: { // instr: I_f64x2_convert_low_i32x4_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 500, ctx);
        break;
    }
    case 501: { // instr: I_f64x2_convert_low_i32x4_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 501, ctx);
        break;
    }
    case 502: { // instr: I_i8x16_relaxed_swizzle(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 502, ctx);
        break;
    }
    case 503: { // instr: I_i32x4_relaxed_trunc_f32x4_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 503, ctx);
        break;
    }
    case 504: { // instr: I_i32x4_relaxed_trunc_f32x4_u(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 504, ctx);
        break;
    }
    case 505: { // instr: I_i32x4_relaxed_trunc_f64x2_s_zero(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 505, ctx);
        break;
    }
    case 506: { // instr: I_i32x4_relaxed_trunc_f64x2_u_zero(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 506, ctx);
        break;
    }
    case 507: { // instr: I_f32x4_relaxed_madd(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 507, ctx);
        break;
    }
    case 508: { // instr: I_f32x4_relaxed_nmadd(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 508, ctx);
        break;
    }
    case 509: { // instr: I_f64x2_relaxed_madd(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 509, ctx);
        break;
    }
    case 510: { // instr: I_f64x2_relaxed_nmadd(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 510, ctx);
        break;
    }
    case 511: { // instr: I_i8x16_relaxed_laneselect(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 511, ctx);
        break;
    }
    case 512: { // instr: I_i16x8_relaxed_laneselect(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 512, ctx);
        break;
    }
    case 513: { // instr: I_i32x4_relaxed_laneselect(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 513, ctx);
        break;
    }
    case 514: { // instr: I_i64x2_relaxed_laneselect(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 514, ctx);
        break;
    }
    case 515: { // instr: I_f32x4_relaxed_min(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 515, ctx);
        break;
    }
    case 516: { // instr: I_f32x4_relaxed_max(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 516, ctx);
        break;
    }
    case 517: { // instr: I_f64x2_relaxed_min(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 517, ctx);
        break;
    }
    case 518: { // instr: I_f64x2_relaxed_max(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 518, ctx);
        break;
    }
    case 519: { // instr: I_i16x8_relaxed_q15mulr_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 519, ctx);
        break;
    }
    case 520: { // instr: I_i16x8_relaxed_dot_i8x16_i7x16_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 520, ctx);
        break;
    }
    case 521: { // instr: I_i32x4_relaxed_dot_i8x16_i7x16_add_s(operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 521, ctx);
        break;
    }
    case 522: { // operands: W_op_cons(instr, operands)
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        wat_layout_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 522, ctx);
        break;
    }
    case 523: { // operands: W_op_nil
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            wat_layout_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        wat_rec(node, 523, ctx);
        break;
    }
    case 524: { // grp: decl
        wat_layout_burg_reduce(node, state, 2, ctx);
        break;
    }
    case 525: { // grp: instr
        wat_layout_burg_reduce(node, state, 7, ctx);
        break;
    }
    case 526: { // grp: subtype
        wat_layout_burg_reduce(node, state, 3, ctx);
        break;
    }
    case 527: { // grp: comptype
        wat_layout_burg_reduce(node, state, 4, ctx);
        break;
    }
    case 528: { // grp: typeuse
        wat_layout_burg_reduce(node, state, 5, ctx);
        break;
    }
    case 529: { // grp: externdesc
        wat_layout_burg_reduce(node, state, 6, ctx);
        break;
    }
    default:
        wat_layout_burg_set_error("burg: no rule for goal nonterminal", goalnt, ctx);
        break;
    }
}

void wat_layout_burg_rewrite(BURG_NODE_TYPE root, wat_layout_burg_ctx_t* ctx) {
    if (wat_layout_burg_has_error(ctx)) return;
    arena_reset(ctx);

    if (BURG_NODE_SUCC_COUNT(root) == 0) {
        burg_state_t* state = burg_label_tree(root, ctx);
        if (!state->rule[1])
            wat_layout_burg_set_error("burg: start nonterminal has no rule at root", (int)BURG_NODE_OP(root), ctx);
        else
            wat_layout_burg_reduce(root, state, 1, ctx);
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
                wat_layout_burg_set_error("burg: start nonterminal does not cover graph node", (int)BURG_NODE_OP(rpo[_i]), ctx);
            else
                wat_layout_burg_reduce(rpo[_i], s, 1, ctx);
        }
    }
    bbq_vec_free(rpo);
}
