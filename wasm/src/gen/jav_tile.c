#include "jav_tile.h"

void jav_tile_burg_ctx_init(jav_tile_burg_ctx_t* ctx) {
    bbq_arena_init(&ctx->arena, 4096);
    ctx->state_cache = bbq_htree_create();
    ctx->burg_error_msg = NULL;
    ctx->burg_error_arg = 0;
}

void jav_tile_burg_ctx_free(jav_tile_burg_ctx_t* ctx) {
    bbq_arena_free(&ctx->arena);
    bbq_htree_destroy(ctx->state_cache);
    ctx->state_cache = NULL;
}

bool jav_tile_burg_has_error(const jav_tile_burg_ctx_t* ctx) {
    return ctx->burg_error_msg != NULL;
}

const char* jav_tile_burg_get_error(const jav_tile_burg_ctx_t* ctx) {
    return ctx->burg_error_msg;
}

int jav_tile_burg_get_error_arg(const jav_tile_burg_ctx_t* ctx) {
    return ctx->burg_error_arg;
}

void jav_tile_burg_clear_error(jav_tile_burg_ctx_t* ctx) {
    ctx->burg_error_msg = NULL;
    ctx->burg_error_arg = 0;
}

void jav_tile_burg_set_error(const char* msg, int arg, jav_tile_burg_ctx_t* ctx) {
    if (ctx->burg_error_msg == NULL) {
        ctx->burg_error_msg = msg;
        ctx->burg_error_arg = arg;
    }
}


static BURG_UNUSED void* arena_alloc(size_t size, jav_tile_burg_ctx_t* ctx) {
    return bbq_arena_alloc(&ctx->arena, size);
}

static BURG_UNUSED void arena_reset(jav_tile_burg_ctx_t* ctx) {
    bbq_arena_reset(&ctx->arena);
}

static BURG_UNUSED burg_state_t* burg_cache_lookup(uint32_t id, jav_tile_burg_ctx_t* ctx) {
    return (burg_state_t*)bbq_htree_search(ctx->state_cache, id);
}

static BURG_UNUSED void burg_cache_store(uint32_t id, burg_state_t* state, jav_tile_burg_ctx_t* ctx) {
    bbq_htree_insert(ctx->state_cache, id, state);
}

static BURG_UNUSED void burg_cache_clear(jav_tile_burg_ctx_t* ctx) {
    bbq_htree_clear(ctx->state_cache);
}

static void closure_v128_reg5(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_v128_reg3(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_v128_reg6(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_v128_reg2(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_v128_reg1(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_v128_reg0(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f64_reg5(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f64_reg3(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f64_reg1(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f64_reg0(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f32_reg7(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i32_reg6(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i32_reg5(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f64_reg6(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i32_reg4(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f32_reg4(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i32_reg2(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i64_reg2(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i32_reg1(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_v128_reg4(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f64_mem(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i32_reg0(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f32_reg3(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i64_mem(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i64_reg7(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_v128_mem(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f64_reg4(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i64_reg4(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i32_mem(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f32_reg0(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i32_reg3(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i64_reg0(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f32_reg6(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i32_reg7(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i64_reg3(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_ref_mem(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i64_reg1(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i64_reg5(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f32_reg5(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f64_reg2(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_i64_reg6(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f32_reg1(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f64_reg7(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f32_mem(burg_state_t* p, int c, BURG_NODE_TYPE node);
static void closure_f32_reg2(burg_state_t* p, int c, BURG_NODE_TYPE node);

static void closure_v128_reg5(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 6 < p->cost[6]) {
        p->cost[6] = c + 6;
        p->rule[6] = 120;
        closure_v128_mem(p, c + 6, node);
    }
    if (c + 0 < p->cost[46]) {
        p->cost[46] = c + 0;
        p->rule[46] = 122;
        closure_v128_reg6(p, c + 0, node);
    }
}

static void closure_v128_reg3(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 6 < p->cost[6]) {
        p->cost[6] = c + 6;
        p->rule[6] = 114;
        closure_v128_mem(p, c + 6, node);
    }
    if (c + 0 < p->cost[44]) {
        p->cost[44] = c + 0;
        p->rule[44] = 116;
        closure_v128_reg4(p, c + 0, node);
    }
}

static void closure_v128_reg6(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 6 < p->cost[6]) {
        p->cost[6] = c + 6;
        p->rule[6] = 123;
        closure_v128_mem(p, c + 6, node);
    }
}

static void closure_v128_reg2(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 6 < p->cost[6]) {
        p->cost[6] = c + 6;
        p->rule[6] = 111;
        closure_v128_mem(p, c + 6, node);
    }
    if (c + 0 < p->cost[43]) {
        p->cost[43] = c + 0;
        p->rule[43] = 113;
        closure_v128_reg3(p, c + 0, node);
    }
}

static void closure_v128_reg1(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 6 < p->cost[6]) {
        p->cost[6] = c + 6;
        p->rule[6] = 108;
        closure_v128_mem(p, c + 6, node);
    }
    if (c + 0 < p->cost[42]) {
        p->cost[42] = c + 0;
        p->rule[42] = 110;
        closure_v128_reg2(p, c + 0, node);
    }
}

static void closure_v128_reg0(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 6 < p->cost[6]) {
        p->cost[6] = c + 6;
        p->rule[6] = 105;
        closure_v128_mem(p, c + 6, node);
    }
    if (c + 0 < p->cost[41]) {
        p->cost[41] = c + 0;
        p->rule[41] = 107;
        closure_v128_reg1(p, c + 0, node);
    }
}

static void closure_f64_reg5(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[5]) {
        p->cost[5] = c + 5;
        p->rule[5] = 97;
        closure_f64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[38]) {
        p->cost[38] = c + 0;
        p->rule[38] = 99;
        closure_f64_reg6(p, c + 0, node);
    }
}

static void closure_f64_reg3(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[5]) {
        p->cost[5] = c + 5;
        p->rule[5] = 91;
        closure_f64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[36]) {
        p->cost[36] = c + 0;
        p->rule[36] = 93;
        closure_f64_reg4(p, c + 0, node);
    }
}

static void closure_f64_reg1(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[5]) {
        p->cost[5] = c + 5;
        p->rule[5] = 85;
        closure_f64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[34]) {
        p->cost[34] = c + 0;
        p->rule[34] = 87;
        closure_f64_reg2(p, c + 0, node);
    }
}

static void closure_f64_reg0(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[5]) {
        p->cost[5] = c + 5;
        p->rule[5] = 82;
        closure_f64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[33]) {
        p->cost[33] = c + 0;
        p->rule[33] = 84;
        closure_f64_reg1(p, c + 0, node);
    }
}

static void closure_f32_reg7(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[4]) {
        p->cost[4] = c + 5;
        p->rule[4] = 80;
        closure_f32_mem(p, c + 5, node);
    }
}

static void closure_i32_reg6(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[2]) {
        p->cost[2] = c + 5;
        p->rule[2] = 31;
        closure_i32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[15]) {
        p->cost[15] = c + 0;
        p->rule[15] = 33;
        closure_i32_reg7(p, c + 0, node);
    }
}

static void closure_i32_reg5(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[2]) {
        p->cost[2] = c + 5;
        p->rule[2] = 28;
        closure_i32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[14]) {
        p->cost[14] = c + 0;
        p->rule[14] = 30;
        closure_i32_reg6(p, c + 0, node);
    }
}

static void closure_f64_reg6(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[5]) {
        p->cost[5] = c + 5;
        p->rule[5] = 100;
        closure_f64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[39]) {
        p->cost[39] = c + 0;
        p->rule[39] = 102;
        closure_f64_reg7(p, c + 0, node);
    }
}

static void closure_i32_reg4(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[2]) {
        p->cost[2] = c + 5;
        p->rule[2] = 25;
        closure_i32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[13]) {
        p->cost[13] = c + 0;
        p->rule[13] = 27;
        closure_i32_reg5(p, c + 0, node);
    }
}

static void closure_f32_reg4(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[4]) {
        p->cost[4] = c + 5;
        p->rule[4] = 71;
        closure_f32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[29]) {
        p->cost[29] = c + 0;
        p->rule[29] = 73;
        closure_f32_reg5(p, c + 0, node);
    }
}

static void closure_i32_reg2(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[2]) {
        p->cost[2] = c + 5;
        p->rule[2] = 19;
        closure_i32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[11]) {
        p->cost[11] = c + 0;
        p->rule[11] = 21;
        closure_i32_reg3(p, c + 0, node);
    }
}

static void closure_i64_reg2(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[3]) {
        p->cost[3] = c + 5;
        p->rule[3] = 42;
        closure_i64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[19]) {
        p->cost[19] = c + 0;
        p->rule[19] = 44;
        closure_i64_reg3(p, c + 0, node);
    }
}

static void closure_i32_reg1(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[2]) {
        p->cost[2] = c + 5;
        p->rule[2] = 16;
        closure_i32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[10]) {
        p->cost[10] = c + 0;
        p->rule[10] = 18;
        closure_i32_reg2(p, c + 0, node);
    }
}

static void closure_v128_reg4(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 6 < p->cost[6]) {
        p->cost[6] = c + 6;
        p->rule[6] = 117;
        closure_v128_mem(p, c + 6, node);
    }
    if (c + 0 < p->cost[45]) {
        p->cost[45] = c + 0;
        p->rule[45] = 119;
        closure_v128_reg5(p, c + 0, node);
    }
}

static void closure_f64_mem(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 4;
    }
    if (c + 5 < p->cost[32]) {
        p->cost[32] = c + 5;
        p->rule[32] = 83;
        closure_f64_reg0(p, c + 5, node);
    }
    if (c + 5 < p->cost[33]) {
        p->cost[33] = c + 5;
        p->rule[33] = 86;
        closure_f64_reg1(p, c + 5, node);
    }
    if (c + 5 < p->cost[34]) {
        p->cost[34] = c + 5;
        p->rule[34] = 89;
        closure_f64_reg2(p, c + 5, node);
    }
    if (c + 5 < p->cost[35]) {
        p->cost[35] = c + 5;
        p->rule[35] = 92;
        closure_f64_reg3(p, c + 5, node);
    }
    if (c + 5 < p->cost[36]) {
        p->cost[36] = c + 5;
        p->rule[36] = 95;
        closure_f64_reg4(p, c + 5, node);
    }
    if (c + 5 < p->cost[37]) {
        p->cost[37] = c + 5;
        p->rule[37] = 98;
        closure_f64_reg5(p, c + 5, node);
    }
    if (c + 5 < p->cost[38]) {
        p->cost[38] = c + 5;
        p->rule[38] = 101;
        closure_f64_reg6(p, c + 5, node);
    }
    if (c + 5 < p->cost[39]) {
        p->cost[39] = c + 5;
        p->rule[39] = 104;
        closure_f64_reg7(p, c + 5, node);
    }
}

static void closure_i32_reg0(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[2]) {
        p->cost[2] = c + 5;
        p->rule[2] = 13;
        closure_i32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[9]) {
        p->cost[9] = c + 0;
        p->rule[9] = 15;
        closure_i32_reg1(p, c + 0, node);
    }
}

static void closure_f32_reg3(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[4]) {
        p->cost[4] = c + 5;
        p->rule[4] = 68;
        closure_f32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[28]) {
        p->cost[28] = c + 0;
        p->rule[28] = 70;
        closure_f32_reg4(p, c + 0, node);
    }
}

static void closure_i64_mem(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 2;
    }
    if (c + 5 < p->cost[16]) {
        p->cost[16] = c + 5;
        p->rule[16] = 37;
        closure_i64_reg0(p, c + 5, node);
    }
    if (c + 5 < p->cost[17]) {
        p->cost[17] = c + 5;
        p->rule[17] = 40;
        closure_i64_reg1(p, c + 5, node);
    }
    if (c + 5 < p->cost[18]) {
        p->cost[18] = c + 5;
        p->rule[18] = 43;
        closure_i64_reg2(p, c + 5, node);
    }
    if (c + 5 < p->cost[19]) {
        p->cost[19] = c + 5;
        p->rule[19] = 46;
        closure_i64_reg3(p, c + 5, node);
    }
    if (c + 5 < p->cost[20]) {
        p->cost[20] = c + 5;
        p->rule[20] = 49;
        closure_i64_reg4(p, c + 5, node);
    }
    if (c + 5 < p->cost[21]) {
        p->cost[21] = c + 5;
        p->rule[21] = 52;
        closure_i64_reg5(p, c + 5, node);
    }
    if (c + 5 < p->cost[22]) {
        p->cost[22] = c + 5;
        p->rule[22] = 55;
        closure_i64_reg6(p, c + 5, node);
    }
    if (c + 5 < p->cost[23]) {
        p->cost[23] = c + 5;
        p->rule[23] = 58;
        closure_i64_reg7(p, c + 5, node);
    }
}

static void closure_i64_reg7(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[3]) {
        p->cost[3] = c + 5;
        p->rule[3] = 57;
        closure_i64_mem(p, c + 5, node);
    }
}

static void closure_v128_mem(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 5;
    }
    if (c + 6 < p->cost[40]) {
        p->cost[40] = c + 6;
        p->rule[40] = 106;
        closure_v128_reg0(p, c + 6, node);
    }
    if (c + 6 < p->cost[41]) {
        p->cost[41] = c + 6;
        p->rule[41] = 109;
        closure_v128_reg1(p, c + 6, node);
    }
    if (c + 6 < p->cost[42]) {
        p->cost[42] = c + 6;
        p->rule[42] = 112;
        closure_v128_reg2(p, c + 6, node);
    }
    if (c + 6 < p->cost[43]) {
        p->cost[43] = c + 6;
        p->rule[43] = 115;
        closure_v128_reg3(p, c + 6, node);
    }
    if (c + 6 < p->cost[44]) {
        p->cost[44] = c + 6;
        p->rule[44] = 118;
        closure_v128_reg4(p, c + 6, node);
    }
    if (c + 6 < p->cost[45]) {
        p->cost[45] = c + 6;
        p->rule[45] = 121;
        closure_v128_reg5(p, c + 6, node);
    }
    if (c + 6 < p->cost[46]) {
        p->cost[46] = c + 6;
        p->rule[46] = 124;
        closure_v128_reg6(p, c + 6, node);
    }
}

static void closure_f64_reg4(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[5]) {
        p->cost[5] = c + 5;
        p->rule[5] = 94;
        closure_f64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[37]) {
        p->cost[37] = c + 0;
        p->rule[37] = 96;
        closure_f64_reg5(p, c + 0, node);
    }
}

static void closure_i64_reg4(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[3]) {
        p->cost[3] = c + 5;
        p->rule[3] = 48;
        closure_i64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[21]) {
        p->cost[21] = c + 0;
        p->rule[21] = 50;
        closure_i64_reg5(p, c + 0, node);
    }
}

static void closure_i32_mem(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 1;
    }
    if (c + 5 < p->cost[8]) {
        p->cost[8] = c + 5;
        p->rule[8] = 14;
        closure_i32_reg0(p, c + 5, node);
    }
    if (c + 5 < p->cost[9]) {
        p->cost[9] = c + 5;
        p->rule[9] = 17;
        closure_i32_reg1(p, c + 5, node);
    }
    if (c + 5 < p->cost[10]) {
        p->cost[10] = c + 5;
        p->rule[10] = 20;
        closure_i32_reg2(p, c + 5, node);
    }
    if (c + 5 < p->cost[11]) {
        p->cost[11] = c + 5;
        p->rule[11] = 23;
        closure_i32_reg3(p, c + 5, node);
    }
    if (c + 5 < p->cost[12]) {
        p->cost[12] = c + 5;
        p->rule[12] = 26;
        closure_i32_reg4(p, c + 5, node);
    }
    if (c + 5 < p->cost[13]) {
        p->cost[13] = c + 5;
        p->rule[13] = 29;
        closure_i32_reg5(p, c + 5, node);
    }
    if (c + 5 < p->cost[14]) {
        p->cost[14] = c + 5;
        p->rule[14] = 32;
        closure_i32_reg6(p, c + 5, node);
    }
    if (c + 5 < p->cost[15]) {
        p->cost[15] = c + 5;
        p->rule[15] = 35;
        closure_i32_reg7(p, c + 5, node);
    }
}

static void closure_f32_reg0(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[4]) {
        p->cost[4] = c + 5;
        p->rule[4] = 59;
        closure_f32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[25]) {
        p->cost[25] = c + 0;
        p->rule[25] = 61;
        closure_f32_reg1(p, c + 0, node);
    }
}

static void closure_i32_reg3(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[2]) {
        p->cost[2] = c + 5;
        p->rule[2] = 22;
        closure_i32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[12]) {
        p->cost[12] = c + 0;
        p->rule[12] = 24;
        closure_i32_reg4(p, c + 0, node);
    }
}

static void closure_i64_reg0(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[3]) {
        p->cost[3] = c + 5;
        p->rule[3] = 36;
        closure_i64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[17]) {
        p->cost[17] = c + 0;
        p->rule[17] = 38;
        closure_i64_reg1(p, c + 0, node);
    }
}

static void closure_f32_reg6(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[4]) {
        p->cost[4] = c + 5;
        p->rule[4] = 77;
        closure_f32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[31]) {
        p->cost[31] = c + 0;
        p->rule[31] = 79;
        closure_f32_reg7(p, c + 0, node);
    }
}

static void closure_i32_reg7(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[2]) {
        p->cost[2] = c + 5;
        p->rule[2] = 34;
        closure_i32_mem(p, c + 5, node);
    }
}

static void closure_i64_reg3(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[3]) {
        p->cost[3] = c + 5;
        p->rule[3] = 45;
        closure_i64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[20]) {
        p->cost[20] = c + 0;
        p->rule[20] = 47;
        closure_i64_reg4(p, c + 0, node);
    }
}

static void closure_ref_mem(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 6;
    }
}

static void closure_i64_reg1(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[3]) {
        p->cost[3] = c + 5;
        p->rule[3] = 39;
        closure_i64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[18]) {
        p->cost[18] = c + 0;
        p->rule[18] = 41;
        closure_i64_reg2(p, c + 0, node);
    }
}

static void closure_i64_reg5(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[3]) {
        p->cost[3] = c + 5;
        p->rule[3] = 51;
        closure_i64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[22]) {
        p->cost[22] = c + 0;
        p->rule[22] = 53;
        closure_i64_reg6(p, c + 0, node);
    }
}

static void closure_f32_reg5(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[4]) {
        p->cost[4] = c + 5;
        p->rule[4] = 74;
        closure_f32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[30]) {
        p->cost[30] = c + 0;
        p->rule[30] = 76;
        closure_f32_reg6(p, c + 0, node);
    }
}

static void closure_f64_reg2(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[5]) {
        p->cost[5] = c + 5;
        p->rule[5] = 88;
        closure_f64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[35]) {
        p->cost[35] = c + 0;
        p->rule[35] = 90;
        closure_f64_reg3(p, c + 0, node);
    }
}

static void closure_i64_reg6(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[3]) {
        p->cost[3] = c + 5;
        p->rule[3] = 54;
        closure_i64_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[23]) {
        p->cost[23] = c + 0;
        p->rule[23] = 56;
        closure_i64_reg7(p, c + 0, node);
    }
}

static void closure_f32_reg1(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[4]) {
        p->cost[4] = c + 5;
        p->rule[4] = 62;
        closure_f32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[26]) {
        p->cost[26] = c + 0;
        p->rule[26] = 64;
        closure_f32_reg2(p, c + 0, node);
    }
}

static void closure_f64_reg7(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[5]) {
        p->cost[5] = c + 5;
        p->rule[5] = 103;
        closure_f64_mem(p, c + 5, node);
    }
}

static void closure_f32_mem(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 0 < p->cost[1]) {
        p->cost[1] = c + 0;
        p->rule[1] = 3;
    }
    if (c + 5 < p->cost[24]) {
        p->cost[24] = c + 5;
        p->rule[24] = 60;
        closure_f32_reg0(p, c + 5, node);
    }
    if (c + 5 < p->cost[25]) {
        p->cost[25] = c + 5;
        p->rule[25] = 63;
        closure_f32_reg1(p, c + 5, node);
    }
    if (c + 5 < p->cost[26]) {
        p->cost[26] = c + 5;
        p->rule[26] = 66;
        closure_f32_reg2(p, c + 5, node);
    }
    if (c + 5 < p->cost[27]) {
        p->cost[27] = c + 5;
        p->rule[27] = 69;
        closure_f32_reg3(p, c + 5, node);
    }
    if (c + 5 < p->cost[28]) {
        p->cost[28] = c + 5;
        p->rule[28] = 72;
        closure_f32_reg4(p, c + 5, node);
    }
    if (c + 5 < p->cost[29]) {
        p->cost[29] = c + 5;
        p->rule[29] = 75;
        closure_f32_reg5(p, c + 5, node);
    }
    if (c + 5 < p->cost[30]) {
        p->cost[30] = c + 5;
        p->rule[30] = 78;
        closure_f32_reg6(p, c + 5, node);
    }
    if (c + 5 < p->cost[31]) {
        p->cost[31] = c + 5;
        p->rule[31] = 81;
        closure_f32_reg7(p, c + 5, node);
    }
}

static void closure_f32_reg2(burg_state_t* p, int c, BURG_NODE_TYPE node) {
    (void)node;
    if (c + 5 < p->cost[4]) {
        p->cost[4] = c + 5;
        p->rule[4] = 65;
        closure_f32_mem(p, c + 5, node);
    }
    if (c + 0 < p->cost[27]) {
        p->cost[27] = c + 0;
        p->rule[27] = 67;
        closure_f32_reg3(p, c + 0, node);
    }
}


static void burg_dp(burg_state_t* p, BURG_NODE_TYPE node, jav_tile_burg_ctx_t* ctx) {
    (void)ctx;
    int op = p->op;
    switch (op) {
    case BURG_Sig_i64_v128_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 725;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 726;
            }
        }
        break;
    case BURG_Sig_i32_v128_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 723;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 724;
            }
        }
        break;
    case BURG_Sig_i64_ref_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[7] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 722;
            }
        }
        break;
    case BURG_Sig_i32_v128_to_void_pw:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 719;
            }
        }
        break;
    case BURG_Sig_ref_i32_ref_i32_i32_to_void:
        if (p->child_count >= 5 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7] && p->children[3]->rule[2] && p->children[4]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + p->children[3]->cost[2] + p->children[4]->cost[2] + 6;
            for (int _ci = 5; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 716;
            }
        }
        if (p->child_count >= 5 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7] && p->children[3]->rule[2] && p->children[4]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + p->children[3]->cost[2] + p->children[4]->cost[8] + 5;
            for (int _ci = 5; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 717;
            }
        }
        if (p->child_count >= 5 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7] && p->children[3]->rule[9] && p->children[4]->rule[8] && (JAV_TNEED(node,4) <= 7)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + p->children[3]->cost[9] + p->children[4]->cost[8] + 4;
            for (int _ci = 5; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 718;
            }
        }
        break;
    case BURG_Sig_ref_i32_f64_i32_to_void:
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[5] && p->children[3]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[5] + p->children[3]->cost[2] + 5;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 710;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[5] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[5] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 711;
            }
        }
        break;
    case BURG_Sig_ref_i32_v128_to_void_pw:
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[6]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[6] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 700;
            }
        }
        break;
    case BURG_Sig_ref_i32_f64_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[5]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[5] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 699;
            }
        }
        break;
    case BURG_Sig_ref_i32_f32_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[4]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[4] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 698;
            }
        }
        break;
    case BURG_Sig_ref_i32_i64_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[3] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 697;
            }
        }
        break;
    case BURG_Sig_ref_i32_i64_i32_to_void:
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[3] && p->children[3]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[3] + p->children[3]->cost[2] + 5;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 706;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[3] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[3] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 707;
            }
        }
        break;
    case BURG_Sig_ref_i32_i32_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 696;
            }
        }
        break;
    case BURG_Sig_i32_ref_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 720;
            }
        }
        break;
    case BURG_Sig_ref_f64_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[5] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 693;
            }
        }
        break;
    case BURG_Sig_ref_i64_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 691;
            }
        }
        break;
    case BURG_Sig_ref_i32_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 690;
            }
        }
        break;
    case BURG_Sig_stk_ref_to_void:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 689;
            }
        }
        break;
    case BURG_Sig_stk_i64_to_void:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 685;
            }
        }
        break;
    case BURG_Sig_stk_i32_to_void:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 684;
            }
        }
        break;
    case BURG_Sig_stk_to_void:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 683;
            }
        }
        break;
    case BURG_Sig_i64_v128_i64_to_void_pw:
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[6] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[6] + p->children[2]->cost[3] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 681;
            }
        }
        break;
    case BURG_Sig_i64_f64_i64_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[5] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[5] + p->children[2]->cost[3] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 680;
            }
        }
        break;
    case BURG_Sig_i64_f32_i64_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[4] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[4] + p->children[2]->cost[3] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 679;
            }
        }
        break;
    case BURG_Sig_i32_v128_i32_to_void_pw:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[6] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[6] + p->children[2]->cost[2] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 677;
            }
        }
        break;
    case BURG_Sig_i64_i64_i64_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[3] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 673;
            }
        }
        break;
    case BURG_Sig_i64_i32_i64_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[2] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + p->children[2]->cost[3] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 668;
            }
        }
        break;
    case BURG_Sig_i32_i32_i32_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 665;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 666;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[9] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 667;
            }
        }
        break;
    case BURG_Sig_i64_f64_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[5] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 663;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 664;
            }
        }
        break;
    case BURG_Sig_i32_f64_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[5] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 661;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 662;
            }
        }
        break;
    case BURG_Sig_i64_f32_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[4] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 659;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 660;
            }
        }
        break;
    case BURG_Sig_i32_i64_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 653;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 654;
            }
        }
        break;
    case BURG_Sig_i64_i32_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 651;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 652;
            }
        }
        break;
    case BURG_Sig_ref_v128_to_void_pw:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 694;
            }
        }
        break;
    case BURG_Sig_ref_to_void:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 648;
            }
        }
        break;
    case BURG_Sig_v128_to_void_pw:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 647;
            }
        }
        break;
    case BURG_Sig_i64_to_void:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 644;
            }
        }
        break;
    case BURG_Sig_i32_to_void:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 642;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 643;
            }
        }
        break;
    case BURG_Sig_ref_ref_i32_to_ref:
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[7] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + p->children[2]->cost[2] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 639;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[7] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 640;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i64_to_ref:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 638;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i64_to_ref_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 637;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_i64_to_ref:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 636;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_i64_to_ref:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 635;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_i64_to_ref:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 634;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i32_to_ref:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 631;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 632;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_i32_to_ref:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 625;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 626;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i32_to_ref:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 620;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 621;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 622;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_to_ref:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 619;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_to_ref:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 616;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_to_ref:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 615;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_f64_to_v128:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[5] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 594;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[5] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 595;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[32] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 596;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 597;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 598;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 599;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 600;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 601;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 602;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 603;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 604;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_ref_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 695;
            }
        }
        break;
    case BURG_Sig_i32_to_f32:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 327;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 328;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 329;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 330;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_to_i64:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 317;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 318;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 319;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 320;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i32_ref_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 701;
            }
        }
        break;
    case BURG_Sig_i64_to_f32:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 331;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 332;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 333;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 334;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_stk_to_i64:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 313;
                closure_i64_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 314;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_to_i64:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 305;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 306;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 307;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 308;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_to_i32_pw:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 231;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 232;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_to_i64:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 301;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 302;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 303;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 304;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i64_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 516;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i64_to_i64:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 299;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 300;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i32_to_i64:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 275;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 276;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 277;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 278;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_i32_to_i64:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 267;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 268;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 269;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 270;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_stk_to_v128_pw:
        {
            int c = 0 + 3;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 519;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_i32_to_f32:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 351;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 352;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 353;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 354;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_v128_to_void_pw:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 721;
            }
        }
        break;
    case BURG_Sig_ref_ref_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 229;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 230;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_i32_to_i64:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 259;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 260;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 261;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 262;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_i32_to_ref:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 623;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 624;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_i64_to_i64:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 281;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 282;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 283;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 284;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 285;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 286;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 287;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 288;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 289;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 290;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 291;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 292;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Carried_f32:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 9;
                closure_f32_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_to_i32:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 233;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 234;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 235;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 236;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Carried_i32:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 7;
                closure_i32_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_to_i64:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 237;
                closure_i64_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 238;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_v128_to_v128:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[6] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 541;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 542;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[40] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 543;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 544;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_i64_i32_to_i64:
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[2] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 309;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 310;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 311;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 312;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Carried_i64:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 8;
                closure_i64_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_to_v128:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 525;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 526;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 527;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 528;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_i32_to_f32:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 347;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 348;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 349;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 350;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_i64_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 183;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 184;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_to_void:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 646;
            }
        }
        break;
    case BURG_Sig_i64_i32_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 147;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 148;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 149;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 150;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_i64_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 513;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_v128_i32_to_v128_pw:
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[2] + 8;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 517;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[8] + 7;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 518;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_to_void:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 645;
            }
        }
        break;
    case BURG_Sig_i32_i32_to_i64:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 247;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 248;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 249;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 250;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 251;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 252;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 253;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 254;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 255;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 256;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 257;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 258;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i32_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 135;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 136;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 137;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 138;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 139;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 140;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 141;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 142;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 143;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 144;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 145;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 146;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Carried_v128:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 11;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_f32_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[4] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 657;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 658;
            }
        }
        break;
    case BURG_Sig_f64_i64_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 514;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_to_i64:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 315;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 316;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i32_to_f64:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 449;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 450;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 451;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 452;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_to_i32:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 127;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 128;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 129;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 130;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Carried_ref:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 12;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_to_v128:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 5;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 557;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 558;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 559;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 560;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_i32_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 155;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 156;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 157;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 158;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_to_f32:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 323;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 324;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 325;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 326;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_to_f32:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 321;
                closure_f32_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 322;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_stk_to_i32:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 227;
                closure_i32_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 228;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i64_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 187;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 188;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_to_i32:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 125;
                closure_i32_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 126;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_i32_i32_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 669;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 670;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[9] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 671;
            }
        }
        break;
    case BURG_Sig_f32_f32_to_f32:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 379;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 380;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 381;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 382;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 383;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 384;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 385;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 386;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 387;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 388;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 389;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 390;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i32_to_i64_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 271;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 272;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 273;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 274;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_to_f32:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 401;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 402;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_i32_to_i64:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 263;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 264;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 265;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 266;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i32_to_i32_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 159;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 160;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 161;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 162;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_to_v128_pw:
        {
            int c = 0 + 3;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 493;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_to_f64:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 489;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 490;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 491;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 492;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i64_to_i64:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 279;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 280;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i64_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 511;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i32_i32_to_i32:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[2] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 221;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 222;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 223;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 224;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i64_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 515;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i32_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 163;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 164;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 165;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 166;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_i64_to_i64:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 295;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 296;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_to_v128_pw:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 497;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_i64_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 181;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 182;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i32_ref_i32_to_void:
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7] && p->children[3]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + p->children[3]->cost[2] + 5;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 714;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 715;
            }
        }
        break;
    case BURG_Sig_f32_i64_to_i64:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 293;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 294;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_to_i32:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 217;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 218;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 219;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 220;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_to_i64:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 243;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 244;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 245;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 246;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_stk_v128_to_void_pw:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 688;
            }
        }
        break;
    case BURG_Sig_i64_i32_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 501;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 502;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i32_i32_i32_to_void:
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[2] && p->children[3]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[2] + p->children[3]->cost[2] + 5;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 702;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[2] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[2] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 703;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[9] && p->children[3]->rule[8] && (JAV_TNEED(node,3) <= 7)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[9] + p->children[3]->cost[8] + 3;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 704;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[10] && p->children[2]->rule[9] && p->children[3]->rule[8] && (JAV_TNEED(node,2) <= 7 && JAV_TNEED(node,3) <= 6)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[10] + p->children[2]->cost[9] + p->children[3]->cost[8] + 2;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 705;
            }
        }
        break;
    case BURG_Sig_i64_to_i64:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 239;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 240;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 241;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 242;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_f32_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 189;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 190;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 191;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 192;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 193;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 194;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 195;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 196;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 197;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 198;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 199;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 200;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_f64_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 201;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 202;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 203;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 204;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 205;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 206;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 207;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 208;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 209;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 210;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 211;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 212;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_i64_to_f32:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 373;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 374;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_i64_to_f64:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 459;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 460;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_i64_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 169;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 170;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 171;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 172;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 173;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 174;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 175;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 176;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 177;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 178;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 179;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 180;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Carried_f64:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 10;
                closure_f64_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_i32_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 505;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 506;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_to_i32:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 213;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 214;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 215;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 216;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i32_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 498;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 499;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 500;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_ref_i32_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[7] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 678;
            }
        }
        break;
    case BURG_Sig_ref_to_i32:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 225;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 226;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_i32_to_f32:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 355;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 356;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 357;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 358;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_stk_to_ref:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 641;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i32_to_f32_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 359;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 360;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 361;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 362;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_f32_i32_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[4] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[4] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 675;
            }
        }
        break;
    case BURG_Sig_ref_i32_to_f32:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 363;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 364;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 365;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 366;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i64_to_f32:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 367;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 368;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i32_v128_i32_to_void_pw:
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[6] && p->children[3]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[6] + p->children[3]->cost[2] + 6;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 712;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[6] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[6] + p->children[3]->cost[8] + 5;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 713;
            }
        }
        break;
    case BURG_Sig_i64_i32_to_f64:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 433;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 434;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 435;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 436;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i32_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 507;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 508;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_i64_to_f32:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 369;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 370;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i64_to_f64:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 463;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 464;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_i64_to_f32:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 371;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 372;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_stk_f32_to_void:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 686;
            }
        }
        break;
    case BURG_Sig_i32_i32_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 649;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 650;
            }
        }
        break;
    case BURG_Sig_i32_i64_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 167;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 168;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i64_to_f32_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 375;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 376;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_to_f64:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 477;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 478;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 479;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 480;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i64_to_i64_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 297;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 298;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i64_to_f32:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 377;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 378;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_to_v128:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 553;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 554;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 555;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 556;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_to_f32:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 391;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 392;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 393;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 394;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_f32_i32_to_f32:
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[4] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + p->children[2]->cost[2] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 395;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[4] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 396;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[4] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 397;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[4] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 398;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_i32_to_ref:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 627;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 628;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_stk_to_f32:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 399;
                closure_f32_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 400;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_to_ref:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 617;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 618;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i64_to_v128:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 572;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 573;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 574;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 575;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 576;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 577;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 578;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 579;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 580;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 581;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 582;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_f64_to_f64:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 465;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 466;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 467;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 468;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 469;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 470;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 471;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 472;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 473;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 474;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 475;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 476;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_i32_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 503;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 504;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_to_f32:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 403;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 404;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 405;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 406;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_to_f64:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 407;
                closure_f64_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 408;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_to_f64:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 409;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 410;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 411;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 412;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_to_f64:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 413;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 414;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 415;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 416;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i32_f32_i32_to_void:
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[4] && p->children[3]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[4] + p->children[3]->cost[2] + 5;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 708;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[4] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[4] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 709;
            }
        }
        break;
    case BURG_Sig_i64_i64_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 512;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_to_f64:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 417;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 418;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 419;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 420;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_i32_to_f64:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 437;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 438;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 439;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 440;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_ref_i64_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[7] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[7] + p->children[2]->cost[3] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 682;
            }
        }
        break;
    case BURG_Sig_f64_i32_to_f64:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 441;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 442;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 443;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 444;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_f64_i32_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[5] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[5] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 676;
            }
        }
        break;
    case BURG_Sig_v128_i32_to_f64_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 445;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 446;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 447;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 448;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_to_void:
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 674;
            }
        }
        break;
    case BURG_Sig_i32_i64_to_f64:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 453;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 454;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i32_to_ref_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 629;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 630;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_i64_to_f64:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 455;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 456;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_i64_to_f64:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 457;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 458;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i32_to_f64:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 421;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 422;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 423;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 424;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 425;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 426;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 427;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 428;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 429;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 430;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 431;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 432;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i64_to_f64_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 461;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 462;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_f32_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[4] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 692;
            }
        }
        break;
    case BURG_Sig_f32_i32_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 151;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 152;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 153;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 154;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_to_v128_pw:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 5;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 494;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_f64_f64_i32_to_f64:
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[5] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + p->children[2]->cost[2] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 481;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[5] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 482;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[5] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 483;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[5] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 484;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_to_i32:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 131;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 132;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 133;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 134;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_to_f64:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 487;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 488;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_to_v128_pw:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 495;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 496;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i64_to_i32_pw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 185;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 186;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i32_to_v128_pw:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 509;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 510;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_i64_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 655;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 656;
            }
        }
        break;
    case BURG_Sig_ref_to_v128_pw:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 520;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_to_v128:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 521;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 522;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 523;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 524;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_stk_f64_to_void:
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 687;
            }
        }
        break;
    case BURG_Sig_stk_to_f64:
        {
            int c = 0 + 2;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 485;
                closure_f64_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 486;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_v128_to_v128:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 7;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 531;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 532;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[40] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 533;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[40] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 534;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 535;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 536;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 537;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 538;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 539;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 540;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i64_to_ref:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 633;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_v128_to_v128:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[6] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 545;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 546;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[40] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 547;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 548;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i32_to_f32:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 335;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 336;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 337;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 338;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 339;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 340;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 341;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 342;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 343;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 344;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 345;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 346;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_f32_to_v128:
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 549;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 550;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 551;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 552;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i64_i32_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[3] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 672;
            }
        }
        break;
    case BURG_Sig_to_v128:
        {
            int c = 0 + 3;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 529;
                closure_v128_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 530;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i32_to_v128:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 561;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 562;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 563;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 564;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 565;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 566;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 567;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 568;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 569;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 570;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 571;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_v128_v128_to_v128:
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[6] + 9;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 605;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[6] + 7;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 606;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[40] + 7;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 607;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[40] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 608;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[42] + p->children[2]->cost[40] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 609;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[42] + p->children[2]->cost[40] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 610;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 611;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 612;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 613;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 614;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_f32_to_v128:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[4] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 583;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[4] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 584;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[24] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 585;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 586;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 587;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 588;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 589;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 590;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 591;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 592;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 593;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    default:
        jav_tile_burg_set_error("burg: unknown opcode in match", op, ctx);
        break;
    }
}

static burg_state_t* burg_label_tree(BURG_NODE_TYPE node, jav_tile_burg_ctx_t* ctx) {
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

static burg_state_t* burg_label(BURG_NODE_TYPE node, jav_tile_burg_ctx_t* ctx) {
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

int jav_tile_burg_rule(burg_state_t* state, int goalnt) {
    if (!state || goalnt < 1 || goalnt > BURG_MAX_NT) return 0;
    return state->rule[goalnt];
}

int jav_tile_burg_cost(burg_state_t* state, int goalnt) {
    if (!state || goalnt < 1 || goalnt > BURG_MAX_NT) return BURG_MAX_COST;
    return state->cost[goalnt];
}

burg_state_t* jav_tile_burg_label_root(BURG_NODE_TYPE root, jav_tile_burg_ctx_t* ctx) {
    if (jav_tile_burg_has_error(ctx)) return NULL;
    arena_reset(ctx);
    return burg_label_tree(root, ctx);
}

const char* jav_tile_burg_nt_name(int nt) {
    static const char* names[] = {
        "<invalid>",
        "stmt",
        "i32_mem",
        "i64_mem",
        "f32_mem",
        "f64_mem",
        "v128_mem",
        "ref_mem",
        "i32_reg0",
        "i32_reg1",
        "i32_reg2",
        "i32_reg3",
        "i32_reg4",
        "i32_reg5",
        "i32_reg6",
        "i32_reg7",
        "i64_reg0",
        "i64_reg1",
        "i64_reg2",
        "i64_reg3",
        "i64_reg4",
        "i64_reg5",
        "i64_reg6",
        "i64_reg7",
        "f32_reg0",
        "f32_reg1",
        "f32_reg2",
        "f32_reg3",
        "f32_reg4",
        "f32_reg5",
        "f32_reg6",
        "f32_reg7",
        "f64_reg0",
        "f64_reg1",
        "f64_reg2",
        "f64_reg3",
        "f64_reg4",
        "f64_reg5",
        "f64_reg6",
        "f64_reg7",
        "v128_reg0",
        "v128_reg1",
        "v128_reg2",
        "v128_reg3",
        "v128_reg4",
        "v128_reg5",
        "v128_reg6",
    };
    if (nt >= 1 && nt <= BURG_MAX_NT) return names[nt];
    return names[0];
}

void jav_tile_burg_reduce(BURG_NODE_TYPE node, burg_state_t* state, int goalnt, jav_tile_burg_ctx_t* ctx) {
    if (jav_tile_burg_has_error(ctx)) return;
    int rule = jav_tile_burg_rule(state, goalnt);
    switch (rule) {
    case 1: { // stmt: i32_mem
        jav_tile_burg_reduce(node, state, 2, ctx);
        break;
    }
    case 2: { // stmt: i64_mem
        jav_tile_burg_reduce(node, state, 3, ctx);
        break;
    }
    case 3: { // stmt: f32_mem
        jav_tile_burg_reduce(node, state, 4, ctx);
        break;
    }
    case 4: { // stmt: f64_mem
        jav_tile_burg_reduce(node, state, 5, ctx);
        break;
    }
    case 5: { // stmt: v128_mem
        jav_tile_burg_reduce(node, state, 6, ctx);
        break;
    }
    case 6: { // stmt: ref_mem
        jav_tile_burg_reduce(node, state, 7, ctx);
        break;
    }
    case 7: { // i32_mem: Carried_i32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 8: { // i64_mem: Carried_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 9: { // f32_mem: Carried_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 10: { // f64_mem: Carried_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 11: { // v128_mem: Carried_v128
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 12: { // ref_mem: Carried_ref
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        break;
    }
    case 13: { // i32_mem: i32_reg0
        jav_tile_burg_reduce(node, state, 8, ctx);
        break;
    }
    case 14: { // i32_reg0: i32_mem
        jav_tile_burg_reduce(node, state, 2, ctx);
        break;
    }
    case 15: { // i32_reg1: i32_reg0
        jav_tile_burg_reduce(node, state, 8, ctx);
        break;
    }
    case 16: { // i32_mem: i32_reg1
        jav_tile_burg_reduce(node, state, 9, ctx);
        break;
    }
    case 17: { // i32_reg1: i32_mem
        jav_tile_burg_reduce(node, state, 2, ctx);
        break;
    }
    case 18: { // i32_reg2: i32_reg1
        jav_tile_burg_reduce(node, state, 9, ctx);
        break;
    }
    case 19: { // i32_mem: i32_reg2
        jav_tile_burg_reduce(node, state, 10, ctx);
        break;
    }
    case 20: { // i32_reg2: i32_mem
        jav_tile_burg_reduce(node, state, 2, ctx);
        break;
    }
    case 21: { // i32_reg3: i32_reg2
        jav_tile_burg_reduce(node, state, 10, ctx);
        break;
    }
    case 22: { // i32_mem: i32_reg3
        jav_tile_burg_reduce(node, state, 11, ctx);
        break;
    }
    case 23: { // i32_reg3: i32_mem
        jav_tile_burg_reduce(node, state, 2, ctx);
        break;
    }
    case 24: { // i32_reg4: i32_reg3
        jav_tile_burg_reduce(node, state, 11, ctx);
        break;
    }
    case 25: { // i32_mem: i32_reg4
        jav_tile_burg_reduce(node, state, 12, ctx);
        break;
    }
    case 26: { // i32_reg4: i32_mem
        jav_tile_burg_reduce(node, state, 2, ctx);
        break;
    }
    case 27: { // i32_reg5: i32_reg4
        jav_tile_burg_reduce(node, state, 12, ctx);
        break;
    }
    case 28: { // i32_mem: i32_reg5
        jav_tile_burg_reduce(node, state, 13, ctx);
        break;
    }
    case 29: { // i32_reg5: i32_mem
        jav_tile_burg_reduce(node, state, 2, ctx);
        break;
    }
    case 30: { // i32_reg6: i32_reg5
        jav_tile_burg_reduce(node, state, 13, ctx);
        break;
    }
    case 31: { // i32_mem: i32_reg6
        jav_tile_burg_reduce(node, state, 14, ctx);
        break;
    }
    case 32: { // i32_reg6: i32_mem
        jav_tile_burg_reduce(node, state, 2, ctx);
        break;
    }
    case 33: { // i32_reg7: i32_reg6
        jav_tile_burg_reduce(node, state, 14, ctx);
        break;
    }
    case 34: { // i32_mem: i32_reg7
        jav_tile_burg_reduce(node, state, 15, ctx);
        break;
    }
    case 35: { // i32_reg7: i32_mem
        jav_tile_burg_reduce(node, state, 2, ctx);
        break;
    }
    case 36: { // i64_mem: i64_reg0
        jav_tile_burg_reduce(node, state, 16, ctx);
        break;
    }
    case 37: { // i64_reg0: i64_mem
        jav_tile_burg_reduce(node, state, 3, ctx);
        break;
    }
    case 38: { // i64_reg1: i64_reg0
        jav_tile_burg_reduce(node, state, 16, ctx);
        break;
    }
    case 39: { // i64_mem: i64_reg1
        jav_tile_burg_reduce(node, state, 17, ctx);
        break;
    }
    case 40: { // i64_reg1: i64_mem
        jav_tile_burg_reduce(node, state, 3, ctx);
        break;
    }
    case 41: { // i64_reg2: i64_reg1
        jav_tile_burg_reduce(node, state, 17, ctx);
        break;
    }
    case 42: { // i64_mem: i64_reg2
        jav_tile_burg_reduce(node, state, 18, ctx);
        break;
    }
    case 43: { // i64_reg2: i64_mem
        jav_tile_burg_reduce(node, state, 3, ctx);
        break;
    }
    case 44: { // i64_reg3: i64_reg2
        jav_tile_burg_reduce(node, state, 18, ctx);
        break;
    }
    case 45: { // i64_mem: i64_reg3
        jav_tile_burg_reduce(node, state, 19, ctx);
        break;
    }
    case 46: { // i64_reg3: i64_mem
        jav_tile_burg_reduce(node, state, 3, ctx);
        break;
    }
    case 47: { // i64_reg4: i64_reg3
        jav_tile_burg_reduce(node, state, 19, ctx);
        break;
    }
    case 48: { // i64_mem: i64_reg4
        jav_tile_burg_reduce(node, state, 20, ctx);
        break;
    }
    case 49: { // i64_reg4: i64_mem
        jav_tile_burg_reduce(node, state, 3, ctx);
        break;
    }
    case 50: { // i64_reg5: i64_reg4
        jav_tile_burg_reduce(node, state, 20, ctx);
        break;
    }
    case 51: { // i64_mem: i64_reg5
        jav_tile_burg_reduce(node, state, 21, ctx);
        break;
    }
    case 52: { // i64_reg5: i64_mem
        jav_tile_burg_reduce(node, state, 3, ctx);
        break;
    }
    case 53: { // i64_reg6: i64_reg5
        jav_tile_burg_reduce(node, state, 21, ctx);
        break;
    }
    case 54: { // i64_mem: i64_reg6
        jav_tile_burg_reduce(node, state, 22, ctx);
        break;
    }
    case 55: { // i64_reg6: i64_mem
        jav_tile_burg_reduce(node, state, 3, ctx);
        break;
    }
    case 56: { // i64_reg7: i64_reg6
        jav_tile_burg_reduce(node, state, 22, ctx);
        break;
    }
    case 57: { // i64_mem: i64_reg7
        jav_tile_burg_reduce(node, state, 23, ctx);
        break;
    }
    case 58: { // i64_reg7: i64_mem
        jav_tile_burg_reduce(node, state, 3, ctx);
        break;
    }
    case 59: { // f32_mem: f32_reg0
        jav_tile_burg_reduce(node, state, 24, ctx);
        break;
    }
    case 60: { // f32_reg0: f32_mem
        jav_tile_burg_reduce(node, state, 4, ctx);
        break;
    }
    case 61: { // f32_reg1: f32_reg0
        jav_tile_burg_reduce(node, state, 24, ctx);
        break;
    }
    case 62: { // f32_mem: f32_reg1
        jav_tile_burg_reduce(node, state, 25, ctx);
        break;
    }
    case 63: { // f32_reg1: f32_mem
        jav_tile_burg_reduce(node, state, 4, ctx);
        break;
    }
    case 64: { // f32_reg2: f32_reg1
        jav_tile_burg_reduce(node, state, 25, ctx);
        break;
    }
    case 65: { // f32_mem: f32_reg2
        jav_tile_burg_reduce(node, state, 26, ctx);
        break;
    }
    case 66: { // f32_reg2: f32_mem
        jav_tile_burg_reduce(node, state, 4, ctx);
        break;
    }
    case 67: { // f32_reg3: f32_reg2
        jav_tile_burg_reduce(node, state, 26, ctx);
        break;
    }
    case 68: { // f32_mem: f32_reg3
        jav_tile_burg_reduce(node, state, 27, ctx);
        break;
    }
    case 69: { // f32_reg3: f32_mem
        jav_tile_burg_reduce(node, state, 4, ctx);
        break;
    }
    case 70: { // f32_reg4: f32_reg3
        jav_tile_burg_reduce(node, state, 27, ctx);
        break;
    }
    case 71: { // f32_mem: f32_reg4
        jav_tile_burg_reduce(node, state, 28, ctx);
        break;
    }
    case 72: { // f32_reg4: f32_mem
        jav_tile_burg_reduce(node, state, 4, ctx);
        break;
    }
    case 73: { // f32_reg5: f32_reg4
        jav_tile_burg_reduce(node, state, 28, ctx);
        break;
    }
    case 74: { // f32_mem: f32_reg5
        jav_tile_burg_reduce(node, state, 29, ctx);
        break;
    }
    case 75: { // f32_reg5: f32_mem
        jav_tile_burg_reduce(node, state, 4, ctx);
        break;
    }
    case 76: { // f32_reg6: f32_reg5
        jav_tile_burg_reduce(node, state, 29, ctx);
        break;
    }
    case 77: { // f32_mem: f32_reg6
        jav_tile_burg_reduce(node, state, 30, ctx);
        break;
    }
    case 78: { // f32_reg6: f32_mem
        jav_tile_burg_reduce(node, state, 4, ctx);
        break;
    }
    case 79: { // f32_reg7: f32_reg6
        jav_tile_burg_reduce(node, state, 30, ctx);
        break;
    }
    case 80: { // f32_mem: f32_reg7
        jav_tile_burg_reduce(node, state, 31, ctx);
        break;
    }
    case 81: { // f32_reg7: f32_mem
        jav_tile_burg_reduce(node, state, 4, ctx);
        break;
    }
    case 82: { // f64_mem: f64_reg0
        jav_tile_burg_reduce(node, state, 32, ctx);
        break;
    }
    case 83: { // f64_reg0: f64_mem
        jav_tile_burg_reduce(node, state, 5, ctx);
        break;
    }
    case 84: { // f64_reg1: f64_reg0
        jav_tile_burg_reduce(node, state, 32, ctx);
        break;
    }
    case 85: { // f64_mem: f64_reg1
        jav_tile_burg_reduce(node, state, 33, ctx);
        break;
    }
    case 86: { // f64_reg1: f64_mem
        jav_tile_burg_reduce(node, state, 5, ctx);
        break;
    }
    case 87: { // f64_reg2: f64_reg1
        jav_tile_burg_reduce(node, state, 33, ctx);
        break;
    }
    case 88: { // f64_mem: f64_reg2
        jav_tile_burg_reduce(node, state, 34, ctx);
        break;
    }
    case 89: { // f64_reg2: f64_mem
        jav_tile_burg_reduce(node, state, 5, ctx);
        break;
    }
    case 90: { // f64_reg3: f64_reg2
        jav_tile_burg_reduce(node, state, 34, ctx);
        break;
    }
    case 91: { // f64_mem: f64_reg3
        jav_tile_burg_reduce(node, state, 35, ctx);
        break;
    }
    case 92: { // f64_reg3: f64_mem
        jav_tile_burg_reduce(node, state, 5, ctx);
        break;
    }
    case 93: { // f64_reg4: f64_reg3
        jav_tile_burg_reduce(node, state, 35, ctx);
        break;
    }
    case 94: { // f64_mem: f64_reg4
        jav_tile_burg_reduce(node, state, 36, ctx);
        break;
    }
    case 95: { // f64_reg4: f64_mem
        jav_tile_burg_reduce(node, state, 5, ctx);
        break;
    }
    case 96: { // f64_reg5: f64_reg4
        jav_tile_burg_reduce(node, state, 36, ctx);
        break;
    }
    case 97: { // f64_mem: f64_reg5
        jav_tile_burg_reduce(node, state, 37, ctx);
        break;
    }
    case 98: { // f64_reg5: f64_mem
        jav_tile_burg_reduce(node, state, 5, ctx);
        break;
    }
    case 99: { // f64_reg6: f64_reg5
        jav_tile_burg_reduce(node, state, 37, ctx);
        break;
    }
    case 100: { // f64_mem: f64_reg6
        jav_tile_burg_reduce(node, state, 38, ctx);
        break;
    }
    case 101: { // f64_reg6: f64_mem
        jav_tile_burg_reduce(node, state, 5, ctx);
        break;
    }
    case 102: { // f64_reg7: f64_reg6
        jav_tile_burg_reduce(node, state, 38, ctx);
        break;
    }
    case 103: { // f64_mem: f64_reg7
        jav_tile_burg_reduce(node, state, 39, ctx);
        break;
    }
    case 104: { // f64_reg7: f64_mem
        jav_tile_burg_reduce(node, state, 5, ctx);
        break;
    }
    case 105: { // v128_mem: v128_reg0
        jav_tile_burg_reduce(node, state, 40, ctx);
        break;
    }
    case 106: { // v128_reg0: v128_mem
        jav_tile_burg_reduce(node, state, 6, ctx);
        break;
    }
    case 107: { // v128_reg1: v128_reg0
        jav_tile_burg_reduce(node, state, 40, ctx);
        break;
    }
    case 108: { // v128_mem: v128_reg1
        jav_tile_burg_reduce(node, state, 41, ctx);
        break;
    }
    case 109: { // v128_reg1: v128_mem
        jav_tile_burg_reduce(node, state, 6, ctx);
        break;
    }
    case 110: { // v128_reg2: v128_reg1
        jav_tile_burg_reduce(node, state, 41, ctx);
        break;
    }
    case 111: { // v128_mem: v128_reg2
        jav_tile_burg_reduce(node, state, 42, ctx);
        break;
    }
    case 112: { // v128_reg2: v128_mem
        jav_tile_burg_reduce(node, state, 6, ctx);
        break;
    }
    case 113: { // v128_reg3: v128_reg2
        jav_tile_burg_reduce(node, state, 42, ctx);
        break;
    }
    case 114: { // v128_mem: v128_reg3
        jav_tile_burg_reduce(node, state, 43, ctx);
        break;
    }
    case 115: { // v128_reg3: v128_mem
        jav_tile_burg_reduce(node, state, 6, ctx);
        break;
    }
    case 116: { // v128_reg4: v128_reg3
        jav_tile_burg_reduce(node, state, 43, ctx);
        break;
    }
    case 117: { // v128_mem: v128_reg4
        jav_tile_burg_reduce(node, state, 44, ctx);
        break;
    }
    case 118: { // v128_reg4: v128_mem
        jav_tile_burg_reduce(node, state, 6, ctx);
        break;
    }
    case 119: { // v128_reg5: v128_reg4
        jav_tile_burg_reduce(node, state, 44, ctx);
        break;
    }
    case 120: { // v128_mem: v128_reg5
        jav_tile_burg_reduce(node, state, 45, ctx);
        break;
    }
    case 121: { // v128_reg5: v128_mem
        jav_tile_burg_reduce(node, state, 6, ctx);
        break;
    }
    case 122: { // v128_reg6: v128_reg5
        jav_tile_burg_reduce(node, state, 45, ctx);
        break;
    }
    case 123: { // v128_mem: v128_reg6
        jav_tile_burg_reduce(node, state, 46, ctx);
        break;
    }
    case 124: { // v128_reg6: v128_mem
        jav_tile_burg_reduce(node, state, 6, ctx);
        break;
    }
    case 125: { // i32_mem: Sig_to_i32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 126: { // i32_reg0: Sig_to_i32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 127: { // i32_mem: Sig_i32_to_i32(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 128: { // i32_reg0: Sig_i32_to_i32(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 129: { // i32_mem: Sig_i32_to_i32(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 130: { // i32_reg0: Sig_i32_to_i32(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 131: { // i32_mem: Sig_i64_to_i32(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 132: { // i32_reg0: Sig_i64_to_i32(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 133: { // i32_mem: Sig_i64_to_i32(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 134: { // i32_reg0: Sig_i64_to_i32(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999990u);
        break;
    }
    case 135: { // i32_mem: Sig_i32_i32_to_i32(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 136: { // i32_reg0: Sig_i32_i32_to_i32(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 137: { // i32_mem: Sig_i32_i32_to_i32(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 138: { // i32_reg0: Sig_i32_i32_to_i32(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 139: { // i32_mem: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 140: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999990u);
        break;
    }
    case 141: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999900u, 0x99999990u);
        break;
    }
    case 142: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999900u, 0x99999990u);
        break;
    }
    case 143: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999900u, 0x99999990u);
        break;
    }
    case 144: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999900u, 0x99999990u);
        break;
    }
    case 145: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999900u, 0x99999990u);
        break;
    }
    case 146: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999900u, 0x99999990u);
        break;
    }
    case 147: { // i32_mem: Sig_i64_i32_to_i32(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 148: { // i32_reg0: Sig_i64_i32_to_i32(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 149: { // i32_mem: Sig_i64_i32_to_i32(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 150: { // i32_reg0: Sig_i64_i32_to_i32(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 151: { // i32_mem: Sig_f32_i32_to_i32(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 152: { // i32_reg0: Sig_f32_i32_to_i32(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 153: { // i32_mem: Sig_f32_i32_to_i32(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 154: { // i32_reg0: Sig_f32_i32_to_i32(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 155: { // i32_mem: Sig_f64_i32_to_i32(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 156: { // i32_reg0: Sig_f64_i32_to_i32(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 157: { // i32_mem: Sig_f64_i32_to_i32(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 158: { // i32_reg0: Sig_f64_i32_to_i32(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 159: { // i32_mem: Sig_v128_i32_to_i32_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 160: { // i32_reg0: Sig_v128_i32_to_i32_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 161: { // i32_mem: Sig_v128_i32_to_i32_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 162: { // i32_reg0: Sig_v128_i32_to_i32_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 163: { // i32_mem: Sig_ref_i32_to_i32(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 164: { // i32_reg0: Sig_ref_i32_to_i32(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 165: { // i32_mem: Sig_ref_i32_to_i32(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 166: { // i32_reg0: Sig_ref_i32_to_i32(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 167: { // i32_mem: Sig_i32_i64_to_i32(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 168: { // i32_reg0: Sig_i32_i64_to_i32(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 169: { // i32_mem: Sig_i64_i64_to_i32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 170: { // i32_reg0: Sig_i64_i64_to_i32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 171: { // i32_mem: Sig_i64_i64_to_i32(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 172: { // i32_reg0: Sig_i64_i64_to_i32(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999990u);
        break;
    }
    case 173: { // i32_mem: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999911u, 0x99999999u);
        break;
    }
    case 174: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999911u, 0x99999990u);
        break;
    }
    case 175: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999911u, 0x99999990u);
        break;
    }
    case 176: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999911u, 0x99999990u);
        break;
    }
    case 177: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999911u, 0x99999990u);
        break;
    }
    case 178: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999911u, 0x99999990u);
        break;
    }
    case 179: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999911u, 0x99999990u);
        break;
    }
    case 180: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999911u, 0x99999990u);
        break;
    }
    case 181: { // i32_mem: Sig_f32_i64_to_i32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 182: { // i32_reg0: Sig_f32_i64_to_i32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 183: { // i32_mem: Sig_f64_i64_to_i32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 184: { // i32_reg0: Sig_f64_i64_to_i32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 185: { // i32_mem: Sig_v128_i64_to_i32_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 186: { // i32_reg0: Sig_v128_i64_to_i32_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 187: { // i32_mem: Sig_ref_i64_to_i32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 188: { // i32_reg0: Sig_ref_i64_to_i32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 189: { // i32_mem: Sig_f32_f32_to_i32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 190: { // i32_reg0: Sig_f32_f32_to_i32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 191: { // i32_mem: Sig_f32_f32_to_i32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 192: { // i32_reg0: Sig_f32_f32_to_i32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999990u);
        break;
    }
    case 193: { // i32_mem: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999922u, 0x99999999u);
        break;
    }
    case 194: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999922u, 0x99999990u);
        break;
    }
    case 195: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999922u, 0x99999990u);
        break;
    }
    case 196: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999922u, 0x99999990u);
        break;
    }
    case 197: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999922u, 0x99999990u);
        break;
    }
    case 198: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999922u, 0x99999990u);
        break;
    }
    case 199: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999922u, 0x99999990u);
        break;
    }
    case 200: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999922u, 0x99999990u);
        break;
    }
    case 201: { // i32_mem: Sig_f64_f64_to_i32(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 202: { // i32_reg0: Sig_f64_f64_to_i32(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 203: { // i32_mem: Sig_f64_f64_to_i32(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 204: { // i32_reg0: Sig_f64_f64_to_i32(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999990u);
        break;
    }
    case 205: { // i32_mem: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999933u, 0x99999999u);
        break;
    }
    case 206: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999933u, 0x99999990u);
        break;
    }
    case 207: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999933u, 0x99999990u);
        break;
    }
    case 208: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999933u, 0x99999990u);
        break;
    }
    case 209: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999933u, 0x99999990u);
        break;
    }
    case 210: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999933u, 0x99999990u);
        break;
    }
    case 211: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999933u, 0x99999990u);
        break;
    }
    case 212: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999933u, 0x99999990u);
        break;
    }
    case 213: { // i32_mem: Sig_f32_to_i32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 214: { // i32_reg0: Sig_f32_to_i32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 215: { // i32_mem: Sig_f32_to_i32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 216: { // i32_reg0: Sig_f32_to_i32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999990u);
        break;
    }
    case 217: { // i32_mem: Sig_f64_to_i32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 218: { // i32_reg0: Sig_f64_to_i32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 219: { // i32_mem: Sig_f64_to_i32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 220: { // i32_reg0: Sig_f64_to_i32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999990u);
        break;
    }
    case 221: { // i32_mem: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 222: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 223: { // i32_mem: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 224: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 225: { // i32_mem: Sig_ref_to_i32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 226: { // i32_reg0: Sig_ref_to_i32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 227: { // i32_mem: Sig_stk_to_i32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 228: { // i32_reg0: Sig_stk_to_i32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 229: { // i32_mem: Sig_ref_ref_to_i32(ref_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 230: { // i32_reg0: Sig_ref_ref_to_i32(ref_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 231: { // i32_mem: Sig_v128_to_i32_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 232: { // i32_reg0: Sig_v128_to_i32_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 233: { // i32_mem: Sig_v128_to_i32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 234: { // i32_reg0: Sig_v128_to_i32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 235: { // i32_mem: Sig_v128_to_i32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 236: { // i32_reg0: Sig_v128_to_i32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999990u);
        break;
    }
    case 237: { // i64_mem: Sig_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 238: { // i64_reg0: Sig_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 239: { // i64_mem: Sig_i64_to_i64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 240: { // i64_reg0: Sig_i64_to_i64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 241: { // i64_mem: Sig_i64_to_i64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 242: { // i64_reg0: Sig_i64_to_i64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999991u);
        break;
    }
    case 243: { // i64_mem: Sig_i32_to_i64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 244: { // i64_reg0: Sig_i32_to_i64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 245: { // i64_mem: Sig_i32_to_i64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 246: { // i64_reg0: Sig_i32_to_i64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 247: { // i64_mem: Sig_i32_i32_to_i64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 248: { // i64_reg0: Sig_i32_i32_to_i64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 249: { // i64_mem: Sig_i32_i32_to_i64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 250: { // i64_reg0: Sig_i32_i32_to_i64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 251: { // i64_mem: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 252: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999991u);
        break;
    }
    case 253: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999900u, 0x99999991u);
        break;
    }
    case 254: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999900u, 0x99999991u);
        break;
    }
    case 255: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999900u, 0x99999991u);
        break;
    }
    case 256: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999900u, 0x99999991u);
        break;
    }
    case 257: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999900u, 0x99999991u);
        break;
    }
    case 258: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999900u, 0x99999991u);
        break;
    }
    case 259: { // i64_mem: Sig_i64_i32_to_i64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 260: { // i64_reg0: Sig_i64_i32_to_i64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 261: { // i64_mem: Sig_i64_i32_to_i64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 262: { // i64_reg0: Sig_i64_i32_to_i64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 263: { // i64_mem: Sig_f32_i32_to_i64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 264: { // i64_reg0: Sig_f32_i32_to_i64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 265: { // i64_mem: Sig_f32_i32_to_i64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 266: { // i64_reg0: Sig_f32_i32_to_i64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 267: { // i64_mem: Sig_f64_i32_to_i64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 268: { // i64_reg0: Sig_f64_i32_to_i64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 269: { // i64_mem: Sig_f64_i32_to_i64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 270: { // i64_reg0: Sig_f64_i32_to_i64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 271: { // i64_mem: Sig_v128_i32_to_i64_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 272: { // i64_reg0: Sig_v128_i32_to_i64_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 273: { // i64_mem: Sig_v128_i32_to_i64_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 274: { // i64_reg0: Sig_v128_i32_to_i64_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 275: { // i64_mem: Sig_ref_i32_to_i64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 276: { // i64_reg0: Sig_ref_i32_to_i64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 277: { // i64_mem: Sig_ref_i32_to_i64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 278: { // i64_reg0: Sig_ref_i32_to_i64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 279: { // i64_mem: Sig_i32_i64_to_i64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 280: { // i64_reg0: Sig_i32_i64_to_i64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 281: { // i64_mem: Sig_i64_i64_to_i64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 282: { // i64_reg0: Sig_i64_i64_to_i64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 283: { // i64_mem: Sig_i64_i64_to_i64(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 284: { // i64_reg0: Sig_i64_i64_to_i64(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999991u);
        break;
    }
    case 285: { // i64_mem: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999911u, 0x99999999u);
        break;
    }
    case 286: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999911u, 0x99999991u);
        break;
    }
    case 287: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999911u, 0x99999991u);
        break;
    }
    case 288: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999911u, 0x99999991u);
        break;
    }
    case 289: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999911u, 0x99999991u);
        break;
    }
    case 290: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999911u, 0x99999991u);
        break;
    }
    case 291: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999911u, 0x99999991u);
        break;
    }
    case 292: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999911u, 0x99999991u);
        break;
    }
    case 293: { // i64_mem: Sig_f32_i64_to_i64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 294: { // i64_reg0: Sig_f32_i64_to_i64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 295: { // i64_mem: Sig_f64_i64_to_i64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 296: { // i64_reg0: Sig_f64_i64_to_i64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 297: { // i64_mem: Sig_v128_i64_to_i64_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 298: { // i64_reg0: Sig_v128_i64_to_i64_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 299: { // i64_mem: Sig_ref_i64_to_i64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 300: { // i64_reg0: Sig_ref_i64_to_i64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 301: { // i64_mem: Sig_f32_to_i64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 302: { // i64_reg0: Sig_f32_to_i64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 303: { // i64_mem: Sig_f32_to_i64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 304: { // i64_reg0: Sig_f32_to_i64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999991u);
        break;
    }
    case 305: { // i64_mem: Sig_f64_to_i64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 306: { // i64_reg0: Sig_f64_to_i64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 307: { // i64_mem: Sig_f64_to_i64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 308: { // i64_reg0: Sig_f64_to_i64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999991u);
        break;
    }
    case 309: { // i64_mem: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 310: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 311: { // i64_mem: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 312: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 313: { // i64_mem: Sig_stk_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 314: { // i64_reg0: Sig_stk_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 315: { // i64_mem: Sig_ref_to_i64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 316: { // i64_reg0: Sig_ref_to_i64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 317: { // i64_mem: Sig_v128_to_i64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 318: { // i64_reg0: Sig_v128_to_i64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 319: { // i64_mem: Sig_v128_to_i64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 320: { // i64_reg0: Sig_v128_to_i64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999991u);
        break;
    }
    case 321: { // f32_mem: Sig_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 322: { // f32_reg0: Sig_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 323: { // f32_mem: Sig_f32_to_f32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 324: { // f32_reg0: Sig_f32_to_f32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 325: { // f32_mem: Sig_f32_to_f32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 326: { // f32_reg0: Sig_f32_to_f32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999992u);
        break;
    }
    case 327: { // f32_mem: Sig_i32_to_f32(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 328: { // f32_reg0: Sig_i32_to_f32(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 329: { // f32_mem: Sig_i32_to_f32(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 330: { // f32_reg0: Sig_i32_to_f32(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 331: { // f32_mem: Sig_i64_to_f32(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 332: { // f32_reg0: Sig_i64_to_f32(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 333: { // f32_mem: Sig_i64_to_f32(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 334: { // f32_reg0: Sig_i64_to_f32(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999992u);
        break;
    }
    case 335: { // f32_mem: Sig_i32_i32_to_f32(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 336: { // f32_reg0: Sig_i32_i32_to_f32(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 337: { // f32_mem: Sig_i32_i32_to_f32(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 338: { // f32_reg0: Sig_i32_i32_to_f32(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 339: { // f32_mem: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 340: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999992u);
        break;
    }
    case 341: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999900u, 0x99999992u);
        break;
    }
    case 342: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999900u, 0x99999992u);
        break;
    }
    case 343: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999900u, 0x99999992u);
        break;
    }
    case 344: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999900u, 0x99999992u);
        break;
    }
    case 345: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999900u, 0x99999992u);
        break;
    }
    case 346: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999900u, 0x99999992u);
        break;
    }
    case 347: { // f32_mem: Sig_i64_i32_to_f32(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 348: { // f32_reg0: Sig_i64_i32_to_f32(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 349: { // f32_mem: Sig_i64_i32_to_f32(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 350: { // f32_reg0: Sig_i64_i32_to_f32(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 351: { // f32_mem: Sig_f32_i32_to_f32(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 352: { // f32_reg0: Sig_f32_i32_to_f32(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 353: { // f32_mem: Sig_f32_i32_to_f32(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 354: { // f32_reg0: Sig_f32_i32_to_f32(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 355: { // f32_mem: Sig_f64_i32_to_f32(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 356: { // f32_reg0: Sig_f64_i32_to_f32(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 357: { // f32_mem: Sig_f64_i32_to_f32(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 358: { // f32_reg0: Sig_f64_i32_to_f32(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 359: { // f32_mem: Sig_v128_i32_to_f32_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 360: { // f32_reg0: Sig_v128_i32_to_f32_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 361: { // f32_mem: Sig_v128_i32_to_f32_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 362: { // f32_reg0: Sig_v128_i32_to_f32_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 363: { // f32_mem: Sig_ref_i32_to_f32(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 364: { // f32_reg0: Sig_ref_i32_to_f32(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 365: { // f32_mem: Sig_ref_i32_to_f32(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 366: { // f32_reg0: Sig_ref_i32_to_f32(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 367: { // f32_mem: Sig_i32_i64_to_f32(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 368: { // f32_reg0: Sig_i32_i64_to_f32(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 369: { // f32_mem: Sig_i64_i64_to_f32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 370: { // f32_reg0: Sig_i64_i64_to_f32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 371: { // f32_mem: Sig_f32_i64_to_f32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 372: { // f32_reg0: Sig_f32_i64_to_f32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 373: { // f32_mem: Sig_f64_i64_to_f32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 374: { // f32_reg0: Sig_f64_i64_to_f32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 375: { // f32_mem: Sig_v128_i64_to_f32_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 376: { // f32_reg0: Sig_v128_i64_to_f32_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 377: { // f32_mem: Sig_ref_i64_to_f32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 378: { // f32_reg0: Sig_ref_i64_to_f32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 379: { // f32_mem: Sig_f32_f32_to_f32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 380: { // f32_reg0: Sig_f32_f32_to_f32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 381: { // f32_mem: Sig_f32_f32_to_f32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 382: { // f32_reg0: Sig_f32_f32_to_f32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999992u);
        break;
    }
    case 383: { // f32_mem: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999922u, 0x99999999u);
        break;
    }
    case 384: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999922u, 0x99999992u);
        break;
    }
    case 385: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999922u, 0x99999992u);
        break;
    }
    case 386: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999922u, 0x99999992u);
        break;
    }
    case 387: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999922u, 0x99999992u);
        break;
    }
    case 388: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999922u, 0x99999992u);
        break;
    }
    case 389: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999922u, 0x99999992u);
        break;
    }
    case 390: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999922u, 0x99999992u);
        break;
    }
    case 391: { // f32_mem: Sig_f64_to_f32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 392: { // f32_reg0: Sig_f64_to_f32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 393: { // f32_mem: Sig_f64_to_f32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 394: { // f32_reg0: Sig_f64_to_f32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999992u);
        break;
    }
    case 395: { // f32_mem: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 396: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 397: { // f32_mem: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 398: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 399: { // f32_mem: Sig_stk_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 400: { // f32_reg0: Sig_stk_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 401: { // f32_mem: Sig_ref_to_f32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 402: { // f32_reg0: Sig_ref_to_f32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 403: { // f32_mem: Sig_v128_to_f32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 404: { // f32_reg0: Sig_v128_to_f32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 405: { // f32_mem: Sig_v128_to_f32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 406: { // f32_reg0: Sig_v128_to_f32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999992u);
        break;
    }
    case 407: { // f64_mem: Sig_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 408: { // f64_reg0: Sig_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 409: { // f64_mem: Sig_f64_to_f64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 410: { // f64_reg0: Sig_f64_to_f64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 411: { // f64_mem: Sig_f64_to_f64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 412: { // f64_reg0: Sig_f64_to_f64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999993u);
        break;
    }
    case 413: { // f64_mem: Sig_i32_to_f64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 414: { // f64_reg0: Sig_i32_to_f64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 415: { // f64_mem: Sig_i32_to_f64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 416: { // f64_reg0: Sig_i32_to_f64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 417: { // f64_mem: Sig_i64_to_f64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 418: { // f64_reg0: Sig_i64_to_f64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 419: { // f64_mem: Sig_i64_to_f64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 420: { // f64_reg0: Sig_i64_to_f64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999993u);
        break;
    }
    case 421: { // f64_mem: Sig_i32_i32_to_f64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 422: { // f64_reg0: Sig_i32_i32_to_f64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 423: { // f64_mem: Sig_i32_i32_to_f64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 424: { // f64_reg0: Sig_i32_i32_to_f64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 425: { // f64_mem: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 426: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999993u);
        break;
    }
    case 427: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999900u, 0x99999993u);
        break;
    }
    case 428: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999900u, 0x99999993u);
        break;
    }
    case 429: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999900u, 0x99999993u);
        break;
    }
    case 430: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999900u, 0x99999993u);
        break;
    }
    case 431: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999900u, 0x99999993u);
        break;
    }
    case 432: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999900u, 0x99999993u);
        break;
    }
    case 433: { // f64_mem: Sig_i64_i32_to_f64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 434: { // f64_reg0: Sig_i64_i32_to_f64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 435: { // f64_mem: Sig_i64_i32_to_f64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 436: { // f64_reg0: Sig_i64_i32_to_f64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 437: { // f64_mem: Sig_f32_i32_to_f64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 438: { // f64_reg0: Sig_f32_i32_to_f64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 439: { // f64_mem: Sig_f32_i32_to_f64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 440: { // f64_reg0: Sig_f32_i32_to_f64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 441: { // f64_mem: Sig_f64_i32_to_f64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 442: { // f64_reg0: Sig_f64_i32_to_f64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 443: { // f64_mem: Sig_f64_i32_to_f64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 444: { // f64_reg0: Sig_f64_i32_to_f64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 445: { // f64_mem: Sig_v128_i32_to_f64_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 446: { // f64_reg0: Sig_v128_i32_to_f64_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 447: { // f64_mem: Sig_v128_i32_to_f64_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 448: { // f64_reg0: Sig_v128_i32_to_f64_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 449: { // f64_mem: Sig_ref_i32_to_f64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 450: { // f64_reg0: Sig_ref_i32_to_f64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 451: { // f64_mem: Sig_ref_i32_to_f64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 452: { // f64_reg0: Sig_ref_i32_to_f64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 453: { // f64_mem: Sig_i32_i64_to_f64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 454: { // f64_reg0: Sig_i32_i64_to_f64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 455: { // f64_mem: Sig_i64_i64_to_f64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 456: { // f64_reg0: Sig_i64_i64_to_f64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 457: { // f64_mem: Sig_f32_i64_to_f64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 458: { // f64_reg0: Sig_f32_i64_to_f64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 459: { // f64_mem: Sig_f64_i64_to_f64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 460: { // f64_reg0: Sig_f64_i64_to_f64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 461: { // f64_mem: Sig_v128_i64_to_f64_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 462: { // f64_reg0: Sig_v128_i64_to_f64_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 463: { // f64_mem: Sig_ref_i64_to_f64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 464: { // f64_reg0: Sig_ref_i64_to_f64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 465: { // f64_mem: Sig_f64_f64_to_f64(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 466: { // f64_reg0: Sig_f64_f64_to_f64(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 467: { // f64_mem: Sig_f64_f64_to_f64(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 468: { // f64_reg0: Sig_f64_f64_to_f64(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999993u);
        break;
    }
    case 469: { // f64_mem: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999933u, 0x99999999u);
        break;
    }
    case 470: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999933u, 0x99999993u);
        break;
    }
    case 471: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999933u, 0x99999993u);
        break;
    }
    case 472: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999933u, 0x99999993u);
        break;
    }
    case 473: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999933u, 0x99999993u);
        break;
    }
    case 474: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999933u, 0x99999993u);
        break;
    }
    case 475: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999933u, 0x99999993u);
        break;
    }
    case 476: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999933u, 0x99999993u);
        break;
    }
    case 477: { // f64_mem: Sig_f32_to_f64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 478: { // f64_reg0: Sig_f32_to_f64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 479: { // f64_mem: Sig_f32_to_f64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 480: { // f64_reg0: Sig_f32_to_f64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999993u);
        break;
    }
    case 481: { // f64_mem: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 482: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 483: { // f64_mem: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 484: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 485: { // f64_mem: Sig_stk_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 486: { // f64_reg0: Sig_stk_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 487: { // f64_mem: Sig_ref_to_f64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 488: { // f64_reg0: Sig_ref_to_f64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 489: { // f64_mem: Sig_v128_to_f64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 490: { // f64_reg0: Sig_v128_to_f64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 491: { // f64_mem: Sig_v128_to_f64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 492: { // f64_reg0: Sig_v128_to_f64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999993u);
        break;
    }
    case 493: { // v128_mem: Sig_to_v128_pw
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 494: { // v128_mem: Sig_v128_to_v128_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 495: { // v128_mem: Sig_i32_to_v128_pw(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 496: { // v128_mem: Sig_i32_to_v128_pw(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 497: { // v128_mem: Sig_i64_to_v128_pw(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 498: { // v128_mem: Sig_i32_i32_to_v128_pw(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 499: { // v128_mem: Sig_i32_i32_to_v128_pw(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 500: { // v128_mem: Sig_i32_i32_to_v128_pw(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 501: { // v128_mem: Sig_i64_i32_to_v128_pw(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 502: { // v128_mem: Sig_i64_i32_to_v128_pw(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 503: { // v128_mem: Sig_f32_i32_to_v128_pw(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 504: { // v128_mem: Sig_f32_i32_to_v128_pw(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 505: { // v128_mem: Sig_f64_i32_to_v128_pw(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 506: { // v128_mem: Sig_f64_i32_to_v128_pw(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 507: { // v128_mem: Sig_v128_i32_to_v128_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 508: { // v128_mem: Sig_v128_i32_to_v128_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 509: { // v128_mem: Sig_ref_i32_to_v128_pw(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 510: { // v128_mem: Sig_ref_i32_to_v128_pw(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 511: { // v128_mem: Sig_i32_i64_to_v128_pw(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 512: { // v128_mem: Sig_i64_i64_to_v128_pw(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 513: { // v128_mem: Sig_f32_i64_to_v128_pw(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 514: { // v128_mem: Sig_f64_i64_to_v128_pw(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 515: { // v128_mem: Sig_v128_i64_to_v128_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 516: { // v128_mem: Sig_ref_i64_to_v128_pw(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 517: { // v128_mem: Sig_v128_v128_i32_to_v128_pw(v128_mem, v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 518: { // v128_mem: Sig_v128_v128_i32_to_v128_pw(v128_mem, v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 519: { // v128_mem: Sig_stk_to_v128_pw
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 520: { // v128_mem: Sig_ref_to_v128_pw(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 521: { // v128_mem: Sig_i32_to_v128(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 522: { // v128_reg0: Sig_i32_to_v128(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 523: { // v128_mem: Sig_i32_to_v128(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 524: { // v128_reg0: Sig_i32_to_v128(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 525: { // v128_mem: Sig_i64_to_v128(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 526: { // v128_reg0: Sig_i64_to_v128(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 527: { // v128_mem: Sig_i64_to_v128(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 528: { // v128_reg0: Sig_i64_to_v128(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 529: { // v128_mem: Sig_to_v128
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 530: { // v128_reg0: Sig_to_v128
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 531: { // v128_mem: Sig_v128_v128_to_v128(v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 532: { // v128_reg0: Sig_v128_v128_to_v128(v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 533: { // v128_mem: Sig_v128_v128_to_v128(v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 534: { // v128_reg0: Sig_v128_v128_to_v128(v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 535: { // v128_mem: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99994444u, 0x99999999u);
        break;
    }
    case 536: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99994444u, 0x99999944u);
        break;
    }
    case 537: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99994444u, 0x99999944u);
        break;
    }
    case 538: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99994444u, 0x99999944u);
        break;
    }
    case 539: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99994444u, 0x99999944u);
        break;
    }
    case 540: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99994444u, 0x99999944u);
        break;
    }
    case 541: { // v128_mem: Sig_i32_v128_to_v128(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 542: { // v128_reg0: Sig_i32_v128_to_v128(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 543: { // v128_mem: Sig_i32_v128_to_v128(i32_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 544: { // v128_reg0: Sig_i32_v128_to_v128(i32_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 545: { // v128_mem: Sig_i64_v128_to_v128(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 546: { // v128_reg0: Sig_i64_v128_to_v128(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 547: { // v128_mem: Sig_i64_v128_to_v128(i64_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 548: { // v128_reg0: Sig_i64_v128_to_v128(i64_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 549: { // v128_mem: Sig_f32_to_v128(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 550: { // v128_reg0: Sig_f32_to_v128(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 551: { // v128_mem: Sig_f32_to_v128(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 552: { // v128_reg0: Sig_f32_to_v128(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999944u);
        break;
    }
    case 553: { // v128_mem: Sig_f64_to_v128(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 554: { // v128_reg0: Sig_f64_to_v128(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 555: { // v128_mem: Sig_f64_to_v128(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 556: { // v128_reg0: Sig_f64_to_v128(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999944u);
        break;
    }
    case 557: { // v128_mem: Sig_v128_to_v128(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 558: { // v128_reg0: Sig_v128_to_v128(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 559: { // v128_mem: Sig_v128_to_v128(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 560: { // v128_reg0: Sig_v128_to_v128(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 561: { // v128_mem: Sig_v128_i32_to_v128(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 562: { // v128_reg0: Sig_v128_i32_to_v128(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 563: { // v128_mem: Sig_v128_i32_to_v128(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 564: { // v128_reg0: Sig_v128_i32_to_v128(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 565: { // v128_mem: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999440u, 0x99999999u);
        break;
    }
    case 566: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999440u, 0x99999944u);
        break;
    }
    case 567: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999440u, 0x99999944u);
        break;
    }
    case 568: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999440u, 0x99999944u);
        break;
    }
    case 569: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999440u, 0x99999944u);
        break;
    }
    case 570: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999440u, 0x99999944u);
        break;
    }
    case 571: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999440u, 0x99999944u);
        break;
    }
    case 572: { // v128_mem: Sig_v128_i64_to_v128(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 573: { // v128_reg0: Sig_v128_i64_to_v128(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 574: { // v128_mem: Sig_v128_i64_to_v128(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 575: { // v128_reg0: Sig_v128_i64_to_v128(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 576: { // v128_mem: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999441u, 0x99999999u);
        break;
    }
    case 577: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999441u, 0x99999944u);
        break;
    }
    case 578: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999441u, 0x99999944u);
        break;
    }
    case 579: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999441u, 0x99999944u);
        break;
    }
    case 580: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999441u, 0x99999944u);
        break;
    }
    case 581: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999441u, 0x99999944u);
        break;
    }
    case 582: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999441u, 0x99999944u);
        break;
    }
    case 583: { // v128_mem: Sig_v128_f32_to_v128(v128_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 584: { // v128_reg0: Sig_v128_f32_to_v128(v128_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 585: { // v128_mem: Sig_v128_f32_to_v128(v128_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 586: { // v128_reg0: Sig_v128_f32_to_v128(v128_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999944u);
        break;
    }
    case 587: { // v128_mem: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999442u, 0x99999999u);
        break;
    }
    case 588: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999442u, 0x99999944u);
        break;
    }
    case 589: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999442u, 0x99999944u);
        break;
    }
    case 590: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999442u, 0x99999944u);
        break;
    }
    case 591: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999442u, 0x99999944u);
        break;
    }
    case 592: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999442u, 0x99999944u);
        break;
    }
    case 593: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999442u, 0x99999944u);
        break;
    }
    case 594: { // v128_mem: Sig_v128_f64_to_v128(v128_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 595: { // v128_reg0: Sig_v128_f64_to_v128(v128_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 596: { // v128_mem: Sig_v128_f64_to_v128(v128_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 597: { // v128_reg0: Sig_v128_f64_to_v128(v128_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999944u);
        break;
    }
    case 598: { // v128_mem: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999443u, 0x99999999u);
        break;
    }
    case 599: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999443u, 0x99999944u);
        break;
    }
    case 600: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999443u, 0x99999944u);
        break;
    }
    case 601: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999443u, 0x99999944u);
        break;
    }
    case 602: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999443u, 0x99999944u);
        break;
    }
    case 603: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999443u, 0x99999944u);
        break;
    }
    case 604: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999443u, 0x99999944u);
        break;
    }
    case 605: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 606: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 607: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 608: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 609: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_mem, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99994444u, 0x99999999u);
        break;
    }
    case 610: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_mem, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99994444u, 0x99999944u);
        break;
    }
    case 611: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99444444u, 0x99999999u);
        break;
    }
    case 612: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99444444u, 0x99999944u);
        break;
    }
    case 613: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99444444u, 0x99999944u);
        break;
    }
    case 614: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99444444u, 0x99999944u);
        break;
    }
    case 615: { // ref_mem: Sig_to_ref
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 616: { // ref_mem: Sig_ref_to_ref(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 617: { // ref_mem: Sig_i32_to_ref(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 618: { // ref_mem: Sig_i32_to_ref(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 619: { // ref_mem: Sig_i64_to_ref(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 620: { // ref_mem: Sig_i32_i32_to_ref(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 621: { // ref_mem: Sig_i32_i32_to_ref(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 622: { // ref_mem: Sig_i32_i32_to_ref(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 623: { // ref_mem: Sig_i64_i32_to_ref(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 624: { // ref_mem: Sig_i64_i32_to_ref(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 625: { // ref_mem: Sig_f32_i32_to_ref(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 626: { // ref_mem: Sig_f32_i32_to_ref(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 627: { // ref_mem: Sig_f64_i32_to_ref(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 628: { // ref_mem: Sig_f64_i32_to_ref(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 629: { // ref_mem: Sig_v128_i32_to_ref_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 630: { // ref_mem: Sig_v128_i32_to_ref_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 631: { // ref_mem: Sig_ref_i32_to_ref(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 632: { // ref_mem: Sig_ref_i32_to_ref(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 633: { // ref_mem: Sig_i32_i64_to_ref(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 634: { // ref_mem: Sig_i64_i64_to_ref(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 635: { // ref_mem: Sig_f32_i64_to_ref(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 636: { // ref_mem: Sig_f64_i64_to_ref(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 637: { // ref_mem: Sig_v128_i64_to_ref_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 638: { // ref_mem: Sig_ref_i64_to_ref(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 639: { // ref_mem: Sig_ref_ref_i32_to_ref(ref_mem, ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 640: { // ref_mem: Sig_ref_ref_i32_to_ref(ref_mem, ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 641: { // ref_mem: Sig_stk_to_ref
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 642: { // stmt: Sig_i32_to_void(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 643: { // stmt: Sig_i32_to_void(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 644: { // stmt: Sig_i64_to_void(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 645: { // stmt: Sig_f32_to_void(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 646: { // stmt: Sig_f64_to_void(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 647: { // stmt: Sig_v128_to_void_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 648: { // stmt: Sig_ref_to_void(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 649: { // stmt: Sig_i32_i32_to_void(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 650: { // stmt: Sig_i32_i32_to_void(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 651: { // stmt: Sig_i64_i32_to_void(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 652: { // stmt: Sig_i64_i32_to_void(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 653: { // stmt: Sig_i32_i64_to_void(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 654: { // stmt: Sig_i32_i64_to_void(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 655: { // stmt: Sig_i64_i64_to_void(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 656: { // stmt: Sig_i64_i64_to_void(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 657: { // stmt: Sig_i32_f32_to_void(i32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 658: { // stmt: Sig_i32_f32_to_void(i32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 659: { // stmt: Sig_i64_f32_to_void(i64_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 660: { // stmt: Sig_i64_f32_to_void(i64_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 661: { // stmt: Sig_i32_f64_to_void(i32_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 662: { // stmt: Sig_i32_f64_to_void(i32_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 663: { // stmt: Sig_i64_f64_to_void(i64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 664: { // stmt: Sig_i64_f64_to_void(i64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 665: { // stmt: Sig_i32_i32_i32_to_void(i32_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 666: { // stmt: Sig_i32_i32_i32_to_void(i32_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 667: { // stmt: Sig_i32_i32_i32_to_void(i32_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 668: { // stmt: Sig_i64_i32_i64_to_void(i64_mem, i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 669: { // stmt: Sig_i64_i32_i32_to_void(i64_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 670: { // stmt: Sig_i64_i32_i32_to_void(i64_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 671: { // stmt: Sig_i64_i32_i32_to_void(i64_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 672: { // stmt: Sig_i32_i64_i32_to_void(i32_mem, i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 673: { // stmt: Sig_i64_i64_i64_to_void(i64_mem, i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 674: { // stmt: Sig_to_void
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 675: { // stmt: Sig_i32_f32_i32_to_void(i32_mem, f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 676: { // stmt: Sig_i32_f64_i32_to_void(i32_mem, f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 677: { // stmt: Sig_i32_v128_i32_to_void_pw(i32_mem, v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 678: { // stmt: Sig_i32_ref_i32_to_void(i32_mem, ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 679: { // stmt: Sig_i64_f32_i64_to_void(i64_mem, f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 680: { // stmt: Sig_i64_f64_i64_to_void(i64_mem, f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 681: { // stmt: Sig_i64_v128_i64_to_void_pw(i64_mem, v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 682: { // stmt: Sig_i64_ref_i64_to_void(i64_mem, ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 683: { // stmt: Sig_stk_to_void
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 684: { // stmt: Sig_stk_i32_to_void(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 685: { // stmt: Sig_stk_i64_to_void(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 686: { // stmt: Sig_stk_f32_to_void(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 687: { // stmt: Sig_stk_f64_to_void(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 688: { // stmt: Sig_stk_v128_to_void_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 689: { // stmt: Sig_stk_ref_to_void(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 690: { // stmt: Sig_ref_i32_to_void(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 691: { // stmt: Sig_ref_i64_to_void(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 692: { // stmt: Sig_ref_f32_to_void(ref_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 693: { // stmt: Sig_ref_f64_to_void(ref_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 694: { // stmt: Sig_ref_v128_to_void_pw(ref_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 695: { // stmt: Sig_ref_ref_to_void(ref_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 696: { // stmt: Sig_ref_i32_i32_to_void(ref_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 697: { // stmt: Sig_ref_i32_i64_to_void(ref_mem, i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 698: { // stmt: Sig_ref_i32_f32_to_void(ref_mem, i32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 4, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 699: { // stmt: Sig_ref_i32_f64_to_void(ref_mem, i32_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 5, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 700: { // stmt: Sig_ref_i32_v128_to_void_pw(ref_mem, i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 701: { // stmt: Sig_ref_i32_ref_to_void(ref_mem, i32_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 702: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 703: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 704: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 705: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999000u, 0x99999999u);
        break;
    }
    case 706: { // stmt: Sig_ref_i32_i64_i32_to_void(ref_mem, i32_mem, i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 707: { // stmt: Sig_ref_i32_i64_i32_to_void(ref_mem, i32_mem, i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 708: { // stmt: Sig_ref_i32_f32_i32_to_void(ref_mem, i32_mem, f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 709: { // stmt: Sig_ref_i32_f32_i32_to_void(ref_mem, i32_mem, f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 710: { // stmt: Sig_ref_i32_f64_i32_to_void(ref_mem, i32_mem, f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 711: { // stmt: Sig_ref_i32_f64_i32_to_void(ref_mem, i32_mem, f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 712: { // stmt: Sig_ref_i32_v128_i32_to_void_pw(ref_mem, i32_mem, v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 713: { // stmt: Sig_ref_i32_v128_i32_to_void_pw(ref_mem, i32_mem, v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 714: { // stmt: Sig_ref_i32_ref_i32_to_void(ref_mem, i32_mem, ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 715: { // stmt: Sig_ref_i32_ref_i32_to_void(ref_mem, i32_mem, ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 716: { // stmt: Sig_ref_i32_ref_i32_i32_to_void(ref_mem, i32_mem, ref_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 4), state->children[4], 2, ctx);
        for (int _ci = 5; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 717: { // stmt: Sig_ref_i32_ref_i32_i32_to_void(ref_mem, i32_mem, ref_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 4), state->children[4], 8, ctx);
        for (int _ci = 5; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 718: { // stmt: Sig_ref_i32_ref_i32_i32_to_void(ref_mem, i32_mem, ref_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 4), state->children[4], 8, ctx);
        for (int _ci = 5; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 719: { // stmt: Sig_i32_v128_to_void_pw(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 720: { // stmt: Sig_i32_ref_to_void(i32_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 721: { // stmt: Sig_i64_v128_to_void_pw(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 722: { // stmt: Sig_i64_ref_to_void(i64_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 723: { // stmt: Sig_i32_v128_to_void(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 724: { // stmt: Sig_i32_v128_to_void(i32_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 725: { // stmt: Sig_i64_v128_to_void(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 726: { // stmt: Sig_i64_v128_to_void(i64_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    default:
        jav_tile_burg_set_error("burg: no rule for goal nonterminal", goalnt, ctx);
        break;
    }
}

void jav_tile_burg_rewrite(BURG_NODE_TYPE root, jav_tile_burg_ctx_t* ctx) {
    if (jav_tile_burg_has_error(ctx)) return;
    arena_reset(ctx);

    if (BURG_NODE_SUCC_COUNT(root) == 0) {
        burg_state_t* state = burg_label_tree(root, ctx);
        if (!state->rule[1])
            jav_tile_burg_set_error("burg: start nonterminal has no rule at root", (int)BURG_NODE_OP(root), ctx);
        else
            jav_tile_burg_reduce(root, state, 1, ctx);
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
                jav_tile_burg_set_error("burg: start nonterminal does not cover graph node", (int)BURG_NODE_OP(rpo[_i]), ctx);
            else
                jav_tile_burg_reduce(rpo[_i], s, 1, ctx);
        }
    }
    bbq_vec_free(rpo);
}
