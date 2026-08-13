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
    case BURG_Sig_v128_f64_to_v128:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[5] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 706;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[5] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 707;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[32] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 708;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 709;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 710;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 711;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 712;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 713;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 714;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 715;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 716;
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
                p->rule[6] = 717;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[6] + 7;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 718;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[40] + 7;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 719;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[40] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 720;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[42] + p->children[2]->cost[40] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 721;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[42] + p->children[2]->cost[40] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 722;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 723;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 724;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 725;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 726;
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
                p->rule[6] = 691;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[4] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 692;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[24] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 693;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 694;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 695;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 696;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 697;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 698;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 699;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 700;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 701;
                closure_v128_reg0(p, c, node);
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
                p->rule[4] = 687;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 688;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 689;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 690;
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
                p->rule[3] = 672;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 673;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 674;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 675;
                closure_i64_reg0(p, c, node);
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
                p->rule[6] = 657;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 658;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 659;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 660;
                closure_v128_reg0(p, c, node);
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
                p->rule[2] = 653;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 654;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 655;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 656;
                closure_i32_reg0(p, c, node);
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
                p->rule[6] = 649;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 650;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 651;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 652;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_v128_to_void:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 623;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 624;
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
                p->rule[1] = 621;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 622;
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
                p->rule[6] = 617;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 618;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 619;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 620;
                closure_v128_reg0(p, c, node);
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
                p->rule[6] = 613;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 614;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 615;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 616;
                closure_v128_reg0(p, c, node);
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
                p->rule[1] = 612;
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
                p->rule[1] = 609;
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
                p->rule[2] = 607;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 608;
                closure_i32_reg0(p, c, node);
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
                p->rule[1] = 611;
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
                p->rule[2] = 605;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 606;
                closure_i32_reg0(p, c, node);
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
                p->rule[1] = 602;
            }
        }
        if (p->child_count >= 5 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7] && p->children[3]->rule[2] && p->children[4]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + p->children[3]->cost[2] + p->children[4]->cost[8] + 5;
            for (int _ci = 5; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 603;
            }
        }
        if (p->child_count >= 5 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7] && p->children[3]->rule[9] && p->children[4]->rule[8] && (JAV_TNEED(node,4) <= 7)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + p->children[3]->cost[9] + p->children[4]->cost[8] + 4;
            for (int _ci = 5; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 604;
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
                p->rule[1] = 596;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[5] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[5] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 597;
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
                p->rule[1] = 586;
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
                p->rule[1] = 585;
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
                p->rule[1] = 584;
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
                p->rule[1] = 583;
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
                p->rule[1] = 592;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[3] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[3] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 593;
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
                p->rule[1] = 582;
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
                p->rule[1] = 610;
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
                p->rule[1] = 579;
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
                p->rule[1] = 577;
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
                p->rule[1] = 576;
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
                p->rule[6] = 567;
                closure_v128_mem(p, c, node);
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
                p->rule[3] = 561;
                closure_i64_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 562;
                closure_i64_reg0(p, c, node);
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
                p->rule[2] = 559;
                closure_i32_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 560;
                closure_i32_reg0(p, c, node);
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
                p->rule[1] = 556;
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
                p->rule[6] = 627;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 628;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[40] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 629;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[40] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 630;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 631;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 632;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 633;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 634;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 635;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 636;
                closure_v128_reg0(p, c, node);
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
                p->rule[5] = 565;
                closure_f64_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 566;
                closure_f64_reg0(p, c, node);
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
                p->rule[1] = 554;
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
                p->rule[1] = 552;
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
                p->rule[1] = 551;
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
                p->rule[7] = 549;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[7] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 550;
                closure_ref_mem(p, c, node);
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
                p->rule[4] = 539;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[4] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 540;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[4] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 541;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[4] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 542;
                closure_f32_reg0(p, c, node);
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
                p->rule[3] = 535;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 536;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 537;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 538;
                closure_i64_reg0(p, c, node);
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
                p->rule[5] = 527;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 528;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 529;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 530;
                closure_f64_reg0(p, c, node);
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
                p->rule[4] = 523;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 524;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 525;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 526;
                closure_f32_reg0(p, c, node);
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
                p->rule[3] = 519;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 520;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 521;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 522;
                closure_i64_reg0(p, c, node);
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
                p->rule[3] = 515;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 516;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 517;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 518;
                closure_i64_reg0(p, c, node);
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
                p->rule[2] = 495;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 496;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 497;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 498;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 499;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 500;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 501;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 502;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 503;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 504;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 505;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 506;
                closure_i32_reg0(p, c, node);
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
                p->rule[1] = 458;
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
                p->rule[3] = 260;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 261;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 262;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 263;
                closure_i64_reg0(p, c, node);
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
                p->rule[2] = 390;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 391;
                closure_i32_reg0(p, c, node);
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
                p->rule[3] = 264;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 265;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 266;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 267;
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
                p->rule[2] = 240;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 241;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 242;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 243;
                closure_i32_reg0(p, c, node);
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
                p->rule[1] = 580;
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
                p->rule[1] = 141;
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
                p->rule[4] = 459;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 460;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 461;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 462;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 463;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 464;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 465;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 466;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 467;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 468;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 469;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 470;
                closure_f32_reg0(p, c, node);
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
                p->rule[1] = 210;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 211;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[9] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 212;
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
                p->rule[4] = 571;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 572;
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
                p->rule[3] = 272;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 273;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 274;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 275;
                closure_i64_reg0(p, c, node);
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
                p->rule[1] = 206;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 207;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[9] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 208;
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
                p->rule[7] = 205;
                closure_ref_mem(p, c, node);
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
                p->rule[6] = 204;
                closure_v128_mem(p, c, node);
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
                p->rule[3] = 408;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 409;
                closure_i64_reg0(p, c, node);
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
                p->rule[6] = 200;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 201;
                closure_v128_mem(p, c, node);
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
                p->rule[1] = 198;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 199;
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
                p->rule[3] = 569;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 570;
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
                p->rule[5] = 340;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 341;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 342;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 343;
                closure_f64_reg0(p, c, node);
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
                p->rule[1] = 196;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 197;
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
                p->rule[1] = 194;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 195;
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
                p->rule[7] = 362;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 363;
                closure_ref_mem(p, c, node);
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
                p->rule[6] = 575;
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
                p->rule[1] = 190;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 191;
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
                p->rule[5] = 430;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 431;
                closure_f64_reg0(p, c, node);
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
                p->rule[1] = 188;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 189;
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
                p->rule[1] = 140;
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
                p->rule[1] = 186;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 187;
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
                p->rule[5] = 180;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 181;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 182;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 183;
                closure_f64_reg0(p, c, node);
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
                p->rule[7] = 368;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 369;
                closure_ref_mem(p, c, node);
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
                p->rule[1] = 214;
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
    case BURG_Sig_i64_f32_i64_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[4] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[4] + p->children[2]->cost[3] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 454;
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
                p->rule[5] = 702;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 703;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 704;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 705;
                closure_f64_reg0(p, c, node);
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
                p->rule[6] = 133;
                closure_v128_mem(p, c, node);
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
                p->rule[6] = 637;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 638;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[40] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 639;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 640;
                closure_v128_reg0(p, c, node);
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
                p->rule[3] = 127;
                closure_i64_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 128;
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
                p->rule[3] = 268;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 269;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 270;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 271;
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
                p->rule[3] = 276;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 277;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 278;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 279;
                closure_i64_reg0(p, c, node);
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
                p->rule[6] = 547;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[8] + 7;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 548;
                closure_v128_mem(p, c, node);
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
                p->rule[6] = 440;
                closure_v128_mem(p, c, node);
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
                p->rule[1] = 135;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 136;
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
                p->rule[4] = 150;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 151;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 152;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 153;
                closure_f32_reg0(p, c, node);
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
                p->rule[4] = 563;
                closure_f32_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 564;
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
                p->rule[7] = 364;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 365;
                closure_ref_mem(p, c, node);
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
                p->rule[5] = 176;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 177;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 178;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 179;
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
                p->rule[1] = 594;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[4] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[4] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 595;
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
                p->rule[6] = 439;
                closure_v128_mem(p, c, node);
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
                p->rule[6] = 661;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 662;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 663;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 664;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 665;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 666;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 667;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 668;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 669;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 670;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 671;
                closure_v128_reg0(p, c, node);
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
                p->rule[6] = 625;
                closure_v128_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 626;
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
                p->rule[1] = 213;
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
                p->rule[1] = 137;
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
                p->rule[5] = 131;
                closure_f64_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 132;
                closure_f64_reg0(p, c, node);
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
    case BURG_Sig_f64_i32_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 236;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 237;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 238;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 239;
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
    case BURG_Sig_i64_i32_to_i32:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 228;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 229;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 230;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 231;
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
                p->rule[1] = 139;
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
                p->rule[7] = 447;
                closure_ref_mem(p, c, node);
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
                p->rule[1] = 455;
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
                p->rule[1] = 587;
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
                p->rule[4] = 172;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 173;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 174;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 175;
                closure_f32_reg0(p, c, node);
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
                p->rule[5] = 573;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 574;
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
                p->rule[2] = 160;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 161;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 162;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 163;
                closure_i32_reg0(p, c, node);
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
                p->rule[1] = 588;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[2] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[2] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 589;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[9] && p->children[3]->rule[8] && (JAV_TNEED(node,3) <= 7)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[9] + p->children[3]->cost[8] + 3;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 590;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[10] && p->children[2]->rule[9] && p->children[3]->rule[8] && (JAV_TNEED(node,2) <= 7 && JAV_TNEED(node,3) <= 6)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[10] + p->children[2]->cost[9] + p->children[3]->cost[8] + 2;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 591;
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
                p->rule[2] = 483;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 484;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 485;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 486;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 487;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 488;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 489;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 490;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 491;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 492;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 493;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 494;
                closure_i32_reg0(p, c, node);
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
                p->rule[3] = 146;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 147;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 148;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 149;
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
    case BURG_Sig_i64_i32_i64_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[2] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + p->children[2]->cost[3] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 209;
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
                p->rule[3] = 248;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 249;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 250;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 251;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 252;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 253;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 254;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 255;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 256;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 257;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 258;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 259;
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
                p->rule[2] = 216;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 217;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 218;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 219;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 220;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 221;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 222;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 223;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 224;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 225;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 226;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 227;
                closure_i32_reg0(p, c, node);
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
                p->rule[1] = 138;
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
                p->rule[1] = 192;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 193;
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
                p->rule[6] = 441;
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
                p->rule[2] = 531;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 532;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 533;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 534;
                closure_i32_reg0(p, c, node);
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
                p->rule[2] = 244;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 245;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 246;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 247;
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
                p->rule[6] = 442;
                closure_v128_mem(p, c, node);
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
                p->rule[7] = 159;
                closure_ref_mem(p, c, node);
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
                p->rule[4] = 129;
                closure_f32_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 130;
                closure_f32_reg0(p, c, node);
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
                p->rule[1] = 578;
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
                p->rule[5] = 543;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[5] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 544;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[5] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 545;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[5] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 546;
                closure_f64_reg0(p, c, node);
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
                p->rule[6] = 158;
                closure_v128_mem(p, c, node);
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
                p->rule[2] = 232;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 233;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 234;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 235;
                closure_i32_reg0(p, c, node);
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
                p->rule[7] = 448;
                closure_ref_mem(p, c, node);
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
                p->rule[2] = 142;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 143;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 144;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 145;
                closure_i32_reg0(p, c, node);
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
                p->rule[5] = 154;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 155;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 156;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 157;
                closure_f64_reg0(p, c, node);
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
                p->rule[7] = 134;
                closure_ref_mem(p, c, node);
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
                p->rule[4] = 420;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 421;
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
                p->rule[5] = 432;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 433;
                closure_f64_reg0(p, c, node);
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
                p->rule[2] = 511;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 512;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 513;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 514;
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
                p->rule[3] = 164;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 165;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 166;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 167;
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
                p->rule[1] = 555;
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
                p->rule[6] = 347;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 348;
                closure_v128_mem(p, c, node);
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
                p->rule[1] = 581;
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
                p->rule[4] = 168;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 169;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 170;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 171;
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
                p->rule[6] = 645;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 646;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 647;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 648;
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
                p->rule[4] = 280;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 281;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 282;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 283;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 284;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 285;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 286;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 287;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 288;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 289;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 290;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 291;
                closure_f32_reg0(p, c, node);
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
                p->rule[4] = 292;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 293;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 294;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 295;
                closure_f32_reg0(p, c, node);
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
                p->rule[4] = 296;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 297;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 298;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 299;
                closure_f32_reg0(p, c, node);
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
                p->rule[4] = 300;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 301;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 302;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 303;
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
                p->rule[7] = 568;
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
                p->rule[4] = 304;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 305;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 306;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 307;
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
                p->rule[1] = 598;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[6] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[6] + p->children[3]->cost[8] + 5;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 599;
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
                p->rule[5] = 324;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 325;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 326;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 327;
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
                p->rule[6] = 353;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 354;
                closure_v128_mem(p, c, node);
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
                p->rule[4] = 414;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 415;
                closure_f32_reg0(p, c, node);
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
                p->rule[5] = 328;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 329;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 330;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 331;
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
                p->rule[1] = 457;
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
                p->rule[5] = 332;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 333;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 334;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 335;
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
                p->rule[1] = 451;
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
                p->rule[5] = 336;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 337;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 338;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 339;
                closure_f64_reg0(p, c, node);
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
                p->rule[7] = 357;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 358;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 359;
                closure_ref_mem(p, c, node);
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
                p->rule[6] = 344;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 345;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 346;
                closure_v128_mem(p, c, node);
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
                p->rule[6] = 676;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 677;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 678;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 679;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 680;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 681;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 682;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 683;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 684;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 685;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 686;
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
                p->rule[5] = 471;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 472;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 473;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 474;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 475;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 476;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 477;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 478;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 479;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 480;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 481;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 482;
                closure_f64_reg0(p, c, node);
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
                p->rule[7] = 202;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 203;
                closure_ref_mem(p, c, node);
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
                p->rule[6] = 349;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 350;
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
                p->rule[2] = 507;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 508;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 509;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 510;
                closure_i32_reg0(p, c, node);
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
                p->rule[6] = 351;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 352;
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
                p->rule[2] = 384;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 385;
                closure_i32_reg0(p, c, node);
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
                p->rule[7] = 445;
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
                p->rule[3] = 394;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 395;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 396;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 397;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 398;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 399;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 400;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 401;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 402;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 403;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 404;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 405;
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
                p->rule[7] = 360;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 361;
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
                p->rule[5] = 428;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 429;
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
                p->rule[7] = 366;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 367;
                closure_ref_mem(p, c, node);
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
                p->rule[3] = 410;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 411;
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
                p->rule[4] = 424;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 425;
                closure_f32_reg0(p, c, node);
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
                p->rule[1] = 456;
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
                p->rule[2] = 372;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 373;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 374;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 375;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 376;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 377;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 378;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 379;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 380;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 381;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 382;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 383;
                closure_i32_reg0(p, c, node);
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
                p->rule[2] = 386;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 387;
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
                p->rule[6] = 355;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 356;
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
                p->rule[2] = 388;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 389;
                closure_i32_reg0(p, c, node);
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
                p->rule[3] = 392;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 393;
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
                p->rule[6] = 438;
                closure_v128_mem(p, c, node);
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
                p->rule[1] = 600;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 601;
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
                p->rule[3] = 406;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 407;
                closure_i64_reg0(p, c, node);
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
                p->rule[3] = 412;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 413;
                closure_i64_reg0(p, c, node);
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
                p->rule[4] = 416;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 417;
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
                p->rule[5] = 436;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 437;
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
                p->rule[4] = 418;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 419;
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
                p->rule[1] = 553;
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
                p->rule[2] = 370;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 371;
                closure_i32_reg0(p, c, node);
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
                p->rule[1] = 184;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 185;
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
                p->rule[4] = 422;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 423;
                closure_f32_reg0(p, c, node);
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
                p->rule[1] = 215;
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
                p->rule[5] = 426;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 427;
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
                p->rule[5] = 312;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 313;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 314;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 315;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 316;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 317;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 318;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 319;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 320;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 321;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 322;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 323;
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
                p->rule[5] = 434;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 435;
                closure_f64_reg0(p, c, node);
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
                p->rule[6] = 443;
                closure_v128_mem(p, c, node);
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
                p->rule[6] = 641;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 642;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[40] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 643;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 644;
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
                p->rule[7] = 444;
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
                p->rule[7] = 446;
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
                p->rule[7] = 449;
                closure_ref_mem(p, c, node);
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
                p->rule[4] = 308;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 309;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 310;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 311;
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
                p->rule[1] = 450;
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
                p->rule[1] = 452;
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
                p->rule[2] = 557;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 558;
                closure_i32_reg0(p, c, node);
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
                p->rule[1] = 453;
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
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 126: { // i32_reg0: Sig_to_i32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 127: { // i64_mem: Sig_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 128: { // i64_reg0: Sig_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 129: { // f32_mem: Sig_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 130: { // f32_reg0: Sig_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 131: { // f64_mem: Sig_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 132: { // f64_reg0: Sig_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 133: { // v128_mem: Sig_to_v128_pw
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 134: { // ref_mem: Sig_to_ref
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 135: { // stmt: Sig_i32_to_void(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 136: { // stmt: Sig_i32_to_void(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 137: { // stmt: Sig_i64_to_void(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 138: { // stmt: Sig_f32_to_void(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 139: { // stmt: Sig_f64_to_void(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 140: { // stmt: Sig_v128_to_void_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 141: { // stmt: Sig_ref_to_void(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 142: { // i32_mem: Sig_i32_to_i32(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 143: { // i32_reg0: Sig_i32_to_i32(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 144: { // i32_mem: Sig_i32_to_i32(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 145: { // i32_reg0: Sig_i32_to_i32(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 146: { // i64_mem: Sig_i64_to_i64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 147: { // i64_reg0: Sig_i64_to_i64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 148: { // i64_mem: Sig_i64_to_i64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 149: { // i64_reg0: Sig_i64_to_i64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999991u);
        break;
    }
    case 150: { // f32_mem: Sig_f32_to_f32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 151: { // f32_reg0: Sig_f32_to_f32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 152: { // f32_mem: Sig_f32_to_f32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 153: { // f32_reg0: Sig_f32_to_f32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999992u);
        break;
    }
    case 154: { // f64_mem: Sig_f64_to_f64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 155: { // f64_reg0: Sig_f64_to_f64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 156: { // f64_mem: Sig_f64_to_f64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 157: { // f64_reg0: Sig_f64_to_f64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999993u);
        break;
    }
    case 158: { // v128_mem: Sig_v128_to_v128_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 159: { // ref_mem: Sig_ref_to_ref(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 160: { // i32_mem: Sig_i64_to_i32(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 161: { // i32_reg0: Sig_i64_to_i32(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 162: { // i32_mem: Sig_i64_to_i32(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 163: { // i32_reg0: Sig_i64_to_i32(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999990u);
        break;
    }
    case 164: { // i64_mem: Sig_i32_to_i64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 165: { // i64_reg0: Sig_i32_to_i64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 166: { // i64_mem: Sig_i32_to_i64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 167: { // i64_reg0: Sig_i32_to_i64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 168: { // f32_mem: Sig_i32_to_f32(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 169: { // f32_reg0: Sig_i32_to_f32(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 170: { // f32_mem: Sig_i32_to_f32(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 171: { // f32_reg0: Sig_i32_to_f32(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 172: { // f32_mem: Sig_i64_to_f32(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 173: { // f32_reg0: Sig_i64_to_f32(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 174: { // f32_mem: Sig_i64_to_f32(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 175: { // f32_reg0: Sig_i64_to_f32(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999992u);
        break;
    }
    case 176: { // f64_mem: Sig_i32_to_f64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 177: { // f64_reg0: Sig_i32_to_f64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 178: { // f64_mem: Sig_i32_to_f64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 179: { // f64_reg0: Sig_i32_to_f64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 180: { // f64_mem: Sig_i64_to_f64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 181: { // f64_reg0: Sig_i64_to_f64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 182: { // f64_mem: Sig_i64_to_f64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 183: { // f64_reg0: Sig_i64_to_f64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999993u);
        break;
    }
    case 184: { // stmt: Sig_i32_i32_to_void(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 185: { // stmt: Sig_i32_i32_to_void(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 186: { // stmt: Sig_i64_i32_to_void(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 187: { // stmt: Sig_i64_i32_to_void(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 188: { // stmt: Sig_i32_i64_to_void(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 189: { // stmt: Sig_i32_i64_to_void(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 190: { // stmt: Sig_i64_i64_to_void(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 191: { // stmt: Sig_i64_i64_to_void(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 192: { // stmt: Sig_i32_f32_to_void(i32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 193: { // stmt: Sig_i32_f32_to_void(i32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 194: { // stmt: Sig_i64_f32_to_void(i64_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 195: { // stmt: Sig_i64_f32_to_void(i64_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 196: { // stmt: Sig_i32_f64_to_void(i32_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 197: { // stmt: Sig_i32_f64_to_void(i32_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 198: { // stmt: Sig_i64_f64_to_void(i64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 199: { // stmt: Sig_i64_f64_to_void(i64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 200: { // v128_mem: Sig_i32_to_v128_pw(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 201: { // v128_mem: Sig_i32_to_v128_pw(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 202: { // ref_mem: Sig_i32_to_ref(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 203: { // ref_mem: Sig_i32_to_ref(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 204: { // v128_mem: Sig_i64_to_v128_pw(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 205: { // ref_mem: Sig_i64_to_ref(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 206: { // stmt: Sig_i32_i32_i32_to_void(i32_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 207: { // stmt: Sig_i32_i32_i32_to_void(i32_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 208: { // stmt: Sig_i32_i32_i32_to_void(i32_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 209: { // stmt: Sig_i64_i32_i64_to_void(i64_mem, i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 210: { // stmt: Sig_i64_i32_i32_to_void(i64_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 211: { // stmt: Sig_i64_i32_i32_to_void(i64_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 212: { // stmt: Sig_i64_i32_i32_to_void(i64_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 213: { // stmt: Sig_i32_i64_i32_to_void(i32_mem, i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 214: { // stmt: Sig_i64_i64_i64_to_void(i64_mem, i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 215: { // stmt: Sig_to_void
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 216: { // i32_mem: Sig_i32_i32_to_i32(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 217: { // i32_reg0: Sig_i32_i32_to_i32(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 218: { // i32_mem: Sig_i32_i32_to_i32(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 219: { // i32_reg0: Sig_i32_i32_to_i32(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 220: { // i32_mem: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 221: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999990u);
        break;
    }
    case 222: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999900u, 0x99999990u);
        break;
    }
    case 223: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999900u, 0x99999990u);
        break;
    }
    case 224: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999900u, 0x99999990u);
        break;
    }
    case 225: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999900u, 0x99999990u);
        break;
    }
    case 226: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999900u, 0x99999990u);
        break;
    }
    case 227: { // i32_reg0: Sig_i32_i32_to_i32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999900u, 0x99999990u);
        break;
    }
    case 228: { // i32_mem: Sig_i64_i32_to_i32(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 229: { // i32_reg0: Sig_i64_i32_to_i32(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 230: { // i32_mem: Sig_i64_i32_to_i32(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 231: { // i32_reg0: Sig_i64_i32_to_i32(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 232: { // i32_mem: Sig_f32_i32_to_i32(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 233: { // i32_reg0: Sig_f32_i32_to_i32(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 234: { // i32_mem: Sig_f32_i32_to_i32(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 235: { // i32_reg0: Sig_f32_i32_to_i32(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 236: { // i32_mem: Sig_f64_i32_to_i32(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 237: { // i32_reg0: Sig_f64_i32_to_i32(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 238: { // i32_mem: Sig_f64_i32_to_i32(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 239: { // i32_reg0: Sig_f64_i32_to_i32(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 240: { // i32_mem: Sig_v128_i32_to_i32_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 241: { // i32_reg0: Sig_v128_i32_to_i32_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 242: { // i32_mem: Sig_v128_i32_to_i32_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 243: { // i32_reg0: Sig_v128_i32_to_i32_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 244: { // i32_mem: Sig_ref_i32_to_i32(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 245: { // i32_reg0: Sig_ref_i32_to_i32(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 246: { // i32_mem: Sig_ref_i32_to_i32(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 247: { // i32_reg0: Sig_ref_i32_to_i32(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 248: { // i64_mem: Sig_i32_i32_to_i64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 249: { // i64_reg0: Sig_i32_i32_to_i64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 250: { // i64_mem: Sig_i32_i32_to_i64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 251: { // i64_reg0: Sig_i32_i32_to_i64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 252: { // i64_mem: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 253: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999991u);
        break;
    }
    case 254: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999900u, 0x99999991u);
        break;
    }
    case 255: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999900u, 0x99999991u);
        break;
    }
    case 256: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999900u, 0x99999991u);
        break;
    }
    case 257: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999900u, 0x99999991u);
        break;
    }
    case 258: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999900u, 0x99999991u);
        break;
    }
    case 259: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999900u, 0x99999991u);
        break;
    }
    case 260: { // i64_mem: Sig_i64_i32_to_i64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 261: { // i64_reg0: Sig_i64_i32_to_i64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 262: { // i64_mem: Sig_i64_i32_to_i64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 263: { // i64_reg0: Sig_i64_i32_to_i64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 264: { // i64_mem: Sig_f32_i32_to_i64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 265: { // i64_reg0: Sig_f32_i32_to_i64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 266: { // i64_mem: Sig_f32_i32_to_i64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 267: { // i64_reg0: Sig_f32_i32_to_i64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 268: { // i64_mem: Sig_f64_i32_to_i64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 269: { // i64_reg0: Sig_f64_i32_to_i64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 270: { // i64_mem: Sig_f64_i32_to_i64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 271: { // i64_reg0: Sig_f64_i32_to_i64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 272: { // i64_mem: Sig_v128_i32_to_i64_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 273: { // i64_reg0: Sig_v128_i32_to_i64_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 274: { // i64_mem: Sig_v128_i32_to_i64_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 275: { // i64_reg0: Sig_v128_i32_to_i64_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 276: { // i64_mem: Sig_ref_i32_to_i64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 277: { // i64_reg0: Sig_ref_i32_to_i64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 278: { // i64_mem: Sig_ref_i32_to_i64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 279: { // i64_reg0: Sig_ref_i32_to_i64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 280: { // f32_mem: Sig_i32_i32_to_f32(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 281: { // f32_reg0: Sig_i32_i32_to_f32(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 282: { // f32_mem: Sig_i32_i32_to_f32(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 283: { // f32_reg0: Sig_i32_i32_to_f32(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 284: { // f32_mem: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 285: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999992u);
        break;
    }
    case 286: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999900u, 0x99999992u);
        break;
    }
    case 287: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999900u, 0x99999992u);
        break;
    }
    case 288: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999900u, 0x99999992u);
        break;
    }
    case 289: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999900u, 0x99999992u);
        break;
    }
    case 290: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999900u, 0x99999992u);
        break;
    }
    case 291: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999900u, 0x99999992u);
        break;
    }
    case 292: { // f32_mem: Sig_i64_i32_to_f32(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 293: { // f32_reg0: Sig_i64_i32_to_f32(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 294: { // f32_mem: Sig_i64_i32_to_f32(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 295: { // f32_reg0: Sig_i64_i32_to_f32(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 296: { // f32_mem: Sig_f32_i32_to_f32(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 297: { // f32_reg0: Sig_f32_i32_to_f32(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 298: { // f32_mem: Sig_f32_i32_to_f32(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 299: { // f32_reg0: Sig_f32_i32_to_f32(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 300: { // f32_mem: Sig_f64_i32_to_f32(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 301: { // f32_reg0: Sig_f64_i32_to_f32(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 302: { // f32_mem: Sig_f64_i32_to_f32(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 303: { // f32_reg0: Sig_f64_i32_to_f32(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 304: { // f32_mem: Sig_v128_i32_to_f32_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 305: { // f32_reg0: Sig_v128_i32_to_f32_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 306: { // f32_mem: Sig_v128_i32_to_f32_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 307: { // f32_reg0: Sig_v128_i32_to_f32_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 308: { // f32_mem: Sig_ref_i32_to_f32(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 309: { // f32_reg0: Sig_ref_i32_to_f32(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 310: { // f32_mem: Sig_ref_i32_to_f32(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 311: { // f32_reg0: Sig_ref_i32_to_f32(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 312: { // f64_mem: Sig_i32_i32_to_f64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 313: { // f64_reg0: Sig_i32_i32_to_f64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 314: { // f64_mem: Sig_i32_i32_to_f64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 315: { // f64_reg0: Sig_i32_i32_to_f64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 316: { // f64_mem: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 317: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999993u);
        break;
    }
    case 318: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999900u, 0x99999993u);
        break;
    }
    case 319: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999900u, 0x99999993u);
        break;
    }
    case 320: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999900u, 0x99999993u);
        break;
    }
    case 321: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999900u, 0x99999993u);
        break;
    }
    case 322: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999900u, 0x99999993u);
        break;
    }
    case 323: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999900u, 0x99999993u);
        break;
    }
    case 324: { // f64_mem: Sig_i64_i32_to_f64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 325: { // f64_reg0: Sig_i64_i32_to_f64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 326: { // f64_mem: Sig_i64_i32_to_f64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 327: { // f64_reg0: Sig_i64_i32_to_f64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 328: { // f64_mem: Sig_f32_i32_to_f64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 329: { // f64_reg0: Sig_f32_i32_to_f64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 330: { // f64_mem: Sig_f32_i32_to_f64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 331: { // f64_reg0: Sig_f32_i32_to_f64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 332: { // f64_mem: Sig_f64_i32_to_f64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 333: { // f64_reg0: Sig_f64_i32_to_f64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 334: { // f64_mem: Sig_f64_i32_to_f64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 335: { // f64_reg0: Sig_f64_i32_to_f64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 336: { // f64_mem: Sig_v128_i32_to_f64_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 337: { // f64_reg0: Sig_v128_i32_to_f64_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 338: { // f64_mem: Sig_v128_i32_to_f64_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 339: { // f64_reg0: Sig_v128_i32_to_f64_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 340: { // f64_mem: Sig_ref_i32_to_f64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 341: { // f64_reg0: Sig_ref_i32_to_f64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 342: { // f64_mem: Sig_ref_i32_to_f64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 343: { // f64_reg0: Sig_ref_i32_to_f64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 344: { // v128_mem: Sig_i32_i32_to_v128_pw(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 345: { // v128_mem: Sig_i32_i32_to_v128_pw(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 346: { // v128_mem: Sig_i32_i32_to_v128_pw(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 347: { // v128_mem: Sig_i64_i32_to_v128_pw(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 348: { // v128_mem: Sig_i64_i32_to_v128_pw(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 349: { // v128_mem: Sig_f32_i32_to_v128_pw(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 350: { // v128_mem: Sig_f32_i32_to_v128_pw(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 351: { // v128_mem: Sig_f64_i32_to_v128_pw(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 352: { // v128_mem: Sig_f64_i32_to_v128_pw(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 353: { // v128_mem: Sig_v128_i32_to_v128_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 354: { // v128_mem: Sig_v128_i32_to_v128_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 355: { // v128_mem: Sig_ref_i32_to_v128_pw(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 356: { // v128_mem: Sig_ref_i32_to_v128_pw(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 357: { // ref_mem: Sig_i32_i32_to_ref(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 358: { // ref_mem: Sig_i32_i32_to_ref(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 359: { // ref_mem: Sig_i32_i32_to_ref(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 360: { // ref_mem: Sig_i64_i32_to_ref(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 361: { // ref_mem: Sig_i64_i32_to_ref(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 362: { // ref_mem: Sig_f32_i32_to_ref(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 363: { // ref_mem: Sig_f32_i32_to_ref(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 364: { // ref_mem: Sig_f64_i32_to_ref(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 365: { // ref_mem: Sig_f64_i32_to_ref(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 366: { // ref_mem: Sig_v128_i32_to_ref_pw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 367: { // ref_mem: Sig_v128_i32_to_ref_pw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 368: { // ref_mem: Sig_ref_i32_to_ref(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 369: { // ref_mem: Sig_ref_i32_to_ref(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 370: { // i32_mem: Sig_i32_i64_to_i32(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 371: { // i32_reg0: Sig_i32_i64_to_i32(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 372: { // i32_mem: Sig_i64_i64_to_i32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 373: { // i32_reg0: Sig_i64_i64_to_i32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 374: { // i32_mem: Sig_i64_i64_to_i32(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 375: { // i32_reg0: Sig_i64_i64_to_i32(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999990u);
        break;
    }
    case 376: { // i32_mem: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999911u, 0x99999999u);
        break;
    }
    case 377: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999911u, 0x99999990u);
        break;
    }
    case 378: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999911u, 0x99999990u);
        break;
    }
    case 379: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999911u, 0x99999990u);
        break;
    }
    case 380: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999911u, 0x99999990u);
        break;
    }
    case 381: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999911u, 0x99999990u);
        break;
    }
    case 382: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999911u, 0x99999990u);
        break;
    }
    case 383: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999911u, 0x99999990u);
        break;
    }
    case 384: { // i32_mem: Sig_f32_i64_to_i32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 385: { // i32_reg0: Sig_f32_i64_to_i32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 386: { // i32_mem: Sig_f64_i64_to_i32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 387: { // i32_reg0: Sig_f64_i64_to_i32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 388: { // i32_mem: Sig_v128_i64_to_i32_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 389: { // i32_reg0: Sig_v128_i64_to_i32_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 390: { // i32_mem: Sig_ref_i64_to_i32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 391: { // i32_reg0: Sig_ref_i64_to_i32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 392: { // i64_mem: Sig_i32_i64_to_i64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 393: { // i64_reg0: Sig_i32_i64_to_i64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 394: { // i64_mem: Sig_i64_i64_to_i64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 395: { // i64_reg0: Sig_i64_i64_to_i64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 396: { // i64_mem: Sig_i64_i64_to_i64(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 397: { // i64_reg0: Sig_i64_i64_to_i64(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999991u);
        break;
    }
    case 398: { // i64_mem: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999911u, 0x99999999u);
        break;
    }
    case 399: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999911u, 0x99999991u);
        break;
    }
    case 400: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999911u, 0x99999991u);
        break;
    }
    case 401: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999911u, 0x99999991u);
        break;
    }
    case 402: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999911u, 0x99999991u);
        break;
    }
    case 403: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999911u, 0x99999991u);
        break;
    }
    case 404: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999911u, 0x99999991u);
        break;
    }
    case 405: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999911u, 0x99999991u);
        break;
    }
    case 406: { // i64_mem: Sig_f32_i64_to_i64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 407: { // i64_reg0: Sig_f32_i64_to_i64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 408: { // i64_mem: Sig_f64_i64_to_i64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 409: { // i64_reg0: Sig_f64_i64_to_i64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 410: { // i64_mem: Sig_v128_i64_to_i64_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 411: { // i64_reg0: Sig_v128_i64_to_i64_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 412: { // i64_mem: Sig_ref_i64_to_i64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 413: { // i64_reg0: Sig_ref_i64_to_i64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 414: { // f32_mem: Sig_i32_i64_to_f32(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 415: { // f32_reg0: Sig_i32_i64_to_f32(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 416: { // f32_mem: Sig_i64_i64_to_f32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 417: { // f32_reg0: Sig_i64_i64_to_f32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 418: { // f32_mem: Sig_f32_i64_to_f32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 419: { // f32_reg0: Sig_f32_i64_to_f32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 420: { // f32_mem: Sig_f64_i64_to_f32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 421: { // f32_reg0: Sig_f64_i64_to_f32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 422: { // f32_mem: Sig_v128_i64_to_f32_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 423: { // f32_reg0: Sig_v128_i64_to_f32_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 424: { // f32_mem: Sig_ref_i64_to_f32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 425: { // f32_reg0: Sig_ref_i64_to_f32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 426: { // f64_mem: Sig_i32_i64_to_f64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 427: { // f64_reg0: Sig_i32_i64_to_f64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 428: { // f64_mem: Sig_i64_i64_to_f64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 429: { // f64_reg0: Sig_i64_i64_to_f64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 430: { // f64_mem: Sig_f32_i64_to_f64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 431: { // f64_reg0: Sig_f32_i64_to_f64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 432: { // f64_mem: Sig_f64_i64_to_f64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 433: { // f64_reg0: Sig_f64_i64_to_f64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 434: { // f64_mem: Sig_v128_i64_to_f64_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 435: { // f64_reg0: Sig_v128_i64_to_f64_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 436: { // f64_mem: Sig_ref_i64_to_f64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 437: { // f64_reg0: Sig_ref_i64_to_f64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 438: { // v128_mem: Sig_i32_i64_to_v128_pw(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 439: { // v128_mem: Sig_i64_i64_to_v128_pw(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 440: { // v128_mem: Sig_f32_i64_to_v128_pw(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 441: { // v128_mem: Sig_f64_i64_to_v128_pw(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 442: { // v128_mem: Sig_v128_i64_to_v128_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 443: { // v128_mem: Sig_ref_i64_to_v128_pw(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 444: { // ref_mem: Sig_i32_i64_to_ref(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 445: { // ref_mem: Sig_i64_i64_to_ref(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 446: { // ref_mem: Sig_f32_i64_to_ref(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 447: { // ref_mem: Sig_f64_i64_to_ref(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 448: { // ref_mem: Sig_v128_i64_to_ref_pw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 449: { // ref_mem: Sig_ref_i64_to_ref(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 450: { // stmt: Sig_i32_f32_i32_to_void(i32_mem, f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 451: { // stmt: Sig_i32_f64_i32_to_void(i32_mem, f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 452: { // stmt: Sig_i32_v128_i32_to_void_pw(i32_mem, v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 453: { // stmt: Sig_i32_ref_i32_to_void(i32_mem, ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 454: { // stmt: Sig_i64_f32_i64_to_void(i64_mem, f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 455: { // stmt: Sig_i64_f64_i64_to_void(i64_mem, f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 456: { // stmt: Sig_i64_v128_i64_to_void_pw(i64_mem, v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 457: { // stmt: Sig_i64_ref_i64_to_void(i64_mem, ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 458: { // stmt: Sig_stk_to_void
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 459: { // f32_mem: Sig_f32_f32_to_f32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 460: { // f32_reg0: Sig_f32_f32_to_f32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 461: { // f32_mem: Sig_f32_f32_to_f32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 462: { // f32_reg0: Sig_f32_f32_to_f32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999992u);
        break;
    }
    case 463: { // f32_mem: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999922u, 0x99999999u);
        break;
    }
    case 464: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999922u, 0x99999992u);
        break;
    }
    case 465: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999922u, 0x99999992u);
        break;
    }
    case 466: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999922u, 0x99999992u);
        break;
    }
    case 467: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999922u, 0x99999992u);
        break;
    }
    case 468: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999922u, 0x99999992u);
        break;
    }
    case 469: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999922u, 0x99999992u);
        break;
    }
    case 470: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999922u, 0x99999992u);
        break;
    }
    case 471: { // f64_mem: Sig_f64_f64_to_f64(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 472: { // f64_reg0: Sig_f64_f64_to_f64(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 473: { // f64_mem: Sig_f64_f64_to_f64(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 474: { // f64_reg0: Sig_f64_f64_to_f64(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999993u);
        break;
    }
    case 475: { // f64_mem: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999933u, 0x99999999u);
        break;
    }
    case 476: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999933u, 0x99999993u);
        break;
    }
    case 477: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999933u, 0x99999993u);
        break;
    }
    case 478: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999933u, 0x99999993u);
        break;
    }
    case 479: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999933u, 0x99999993u);
        break;
    }
    case 480: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999933u, 0x99999993u);
        break;
    }
    case 481: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999933u, 0x99999993u);
        break;
    }
    case 482: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999933u, 0x99999993u);
        break;
    }
    case 483: { // i32_mem: Sig_f32_f32_to_i32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 484: { // i32_reg0: Sig_f32_f32_to_i32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 485: { // i32_mem: Sig_f32_f32_to_i32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 486: { // i32_reg0: Sig_f32_f32_to_i32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999990u);
        break;
    }
    case 487: { // i32_mem: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999922u, 0x99999999u);
        break;
    }
    case 488: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999922u, 0x99999990u);
        break;
    }
    case 489: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999922u, 0x99999990u);
        break;
    }
    case 490: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999922u, 0x99999990u);
        break;
    }
    case 491: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999922u, 0x99999990u);
        break;
    }
    case 492: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999922u, 0x99999990u);
        break;
    }
    case 493: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999922u, 0x99999990u);
        break;
    }
    case 494: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999922u, 0x99999990u);
        break;
    }
    case 495: { // i32_mem: Sig_f64_f64_to_i32(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 496: { // i32_reg0: Sig_f64_f64_to_i32(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 497: { // i32_mem: Sig_f64_f64_to_i32(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 498: { // i32_reg0: Sig_f64_f64_to_i32(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999990u);
        break;
    }
    case 499: { // i32_mem: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999933u, 0x99999999u);
        break;
    }
    case 500: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999933u, 0x99999990u);
        break;
    }
    case 501: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999933u, 0x99999990u);
        break;
    }
    case 502: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999933u, 0x99999990u);
        break;
    }
    case 503: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999933u, 0x99999990u);
        break;
    }
    case 504: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999933u, 0x99999990u);
        break;
    }
    case 505: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999933u, 0x99999990u);
        break;
    }
    case 506: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999933u, 0x99999990u);
        break;
    }
    case 507: { // i32_mem: Sig_f32_to_i32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 508: { // i32_reg0: Sig_f32_to_i32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 509: { // i32_mem: Sig_f32_to_i32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 510: { // i32_reg0: Sig_f32_to_i32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999990u);
        break;
    }
    case 511: { // i32_mem: Sig_f64_to_i32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 512: { // i32_reg0: Sig_f64_to_i32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 513: { // i32_mem: Sig_f64_to_i32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 514: { // i32_reg0: Sig_f64_to_i32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999990u);
        break;
    }
    case 515: { // i64_mem: Sig_f32_to_i64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 516: { // i64_reg0: Sig_f32_to_i64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 517: { // i64_mem: Sig_f32_to_i64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 518: { // i64_reg0: Sig_f32_to_i64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999991u);
        break;
    }
    case 519: { // i64_mem: Sig_f64_to_i64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 520: { // i64_reg0: Sig_f64_to_i64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 521: { // i64_mem: Sig_f64_to_i64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 522: { // i64_reg0: Sig_f64_to_i64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999991u);
        break;
    }
    case 523: { // f32_mem: Sig_f64_to_f32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 524: { // f32_reg0: Sig_f64_to_f32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 525: { // f32_mem: Sig_f64_to_f32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 526: { // f32_reg0: Sig_f64_to_f32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999992u);
        break;
    }
    case 527: { // f64_mem: Sig_f32_to_f64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 528: { // f64_reg0: Sig_f32_to_f64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 529: { // f64_mem: Sig_f32_to_f64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 530: { // f64_reg0: Sig_f32_to_f64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999993u);
        break;
    }
    case 531: { // i32_mem: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 532: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 533: { // i32_mem: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 534: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 535: { // i64_mem: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 536: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 537: { // i64_mem: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 538: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 539: { // f32_mem: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 540: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 541: { // f32_mem: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 542: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 543: { // f64_mem: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 544: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 545: { // f64_mem: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 546: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 547: { // v128_mem: Sig_v128_v128_i32_to_v128_pw(v128_mem, v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 548: { // v128_mem: Sig_v128_v128_i32_to_v128_pw(v128_mem, v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 549: { // ref_mem: Sig_ref_ref_i32_to_ref(ref_mem, ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 550: { // ref_mem: Sig_ref_ref_i32_to_ref(ref_mem, ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 551: { // stmt: Sig_stk_i32_to_void(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 552: { // stmt: Sig_stk_i64_to_void(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 553: { // stmt: Sig_stk_f32_to_void(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 554: { // stmt: Sig_stk_f64_to_void(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 555: { // stmt: Sig_stk_v128_to_void_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 556: { // stmt: Sig_stk_ref_to_void(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 557: { // i32_mem: Sig_ref_to_i32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 558: { // i32_reg0: Sig_ref_to_i32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 559: { // i32_mem: Sig_stk_to_i32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 560: { // i32_reg0: Sig_stk_to_i32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 561: { // i64_mem: Sig_stk_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 562: { // i64_reg0: Sig_stk_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 563: { // f32_mem: Sig_stk_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 564: { // f32_reg0: Sig_stk_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 565: { // f64_mem: Sig_stk_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 566: { // f64_reg0: Sig_stk_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 567: { // v128_mem: Sig_stk_to_v128_pw
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 568: { // ref_mem: Sig_stk_to_ref
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 569: { // i64_mem: Sig_ref_to_i64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 570: { // i64_reg0: Sig_ref_to_i64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 571: { // f32_mem: Sig_ref_to_f32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 572: { // f32_reg0: Sig_ref_to_f32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 573: { // f64_mem: Sig_ref_to_f64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 574: { // f64_reg0: Sig_ref_to_f64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 575: { // v128_mem: Sig_ref_to_v128_pw(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 576: { // stmt: Sig_ref_i32_to_void(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 577: { // stmt: Sig_ref_i64_to_void(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 578: { // stmt: Sig_ref_f32_to_void(ref_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 579: { // stmt: Sig_ref_f64_to_void(ref_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 580: { // stmt: Sig_ref_v128_to_void_pw(ref_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 581: { // stmt: Sig_ref_ref_to_void(ref_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 582: { // stmt: Sig_ref_i32_i32_to_void(ref_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 583: { // stmt: Sig_ref_i32_i64_to_void(ref_mem, i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 584: { // stmt: Sig_ref_i32_f32_to_void(ref_mem, i32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 4, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 585: { // stmt: Sig_ref_i32_f64_to_void(ref_mem, i32_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 5, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 586: { // stmt: Sig_ref_i32_v128_to_void_pw(ref_mem, i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 587: { // stmt: Sig_ref_i32_ref_to_void(ref_mem, i32_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 588: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 589: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 590: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 591: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999000u, 0x99999999u);
        break;
    }
    case 592: { // stmt: Sig_ref_i32_i64_i32_to_void(ref_mem, i32_mem, i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 593: { // stmt: Sig_ref_i32_i64_i32_to_void(ref_mem, i32_mem, i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 594: { // stmt: Sig_ref_i32_f32_i32_to_void(ref_mem, i32_mem, f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 595: { // stmt: Sig_ref_i32_f32_i32_to_void(ref_mem, i32_mem, f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 596: { // stmt: Sig_ref_i32_f64_i32_to_void(ref_mem, i32_mem, f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 597: { // stmt: Sig_ref_i32_f64_i32_to_void(ref_mem, i32_mem, f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 598: { // stmt: Sig_ref_i32_v128_i32_to_void_pw(ref_mem, i32_mem, v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 599: { // stmt: Sig_ref_i32_v128_i32_to_void_pw(ref_mem, i32_mem, v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 600: { // stmt: Sig_ref_i32_ref_i32_to_void(ref_mem, i32_mem, ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 601: { // stmt: Sig_ref_i32_ref_i32_to_void(ref_mem, i32_mem, ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 8, ctx);
        for (int _ci = 4; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 602: { // stmt: Sig_ref_i32_ref_i32_i32_to_void(ref_mem, i32_mem, ref_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 4), state->children[4], 2, ctx);
        for (int _ci = 5; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 603: { // stmt: Sig_ref_i32_ref_i32_i32_to_void(ref_mem, i32_mem, ref_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 4), state->children[4], 8, ctx);
        for (int _ci = 5; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 604: { // stmt: Sig_ref_i32_ref_i32_i32_to_void(ref_mem, i32_mem, ref_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 3), state->children[3], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 4), state->children[4], 8, ctx);
        for (int _ci = 5; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 605: { // i32_mem: Sig_ref_ref_to_i32(ref_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 606: { // i32_reg0: Sig_ref_ref_to_i32(ref_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 607: { // i32_mem: Sig_v128_to_i32_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 608: { // i32_reg0: Sig_v128_to_i32_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 609: { // stmt: Sig_i32_v128_to_void_pw(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 610: { // stmt: Sig_i32_ref_to_void(i32_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 611: { // stmt: Sig_i64_v128_to_void_pw(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 612: { // stmt: Sig_i64_ref_to_void(i64_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 613: { // v128_mem: Sig_i32_to_v128(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 614: { // v128_reg0: Sig_i32_to_v128(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 615: { // v128_mem: Sig_i32_to_v128(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 616: { // v128_reg0: Sig_i32_to_v128(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 617: { // v128_mem: Sig_i64_to_v128(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 618: { // v128_reg0: Sig_i64_to_v128(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 619: { // v128_mem: Sig_i64_to_v128(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 620: { // v128_reg0: Sig_i64_to_v128(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 621: { // stmt: Sig_i32_v128_to_void(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 622: { // stmt: Sig_i32_v128_to_void(i32_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 623: { // stmt: Sig_i64_v128_to_void(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 624: { // stmt: Sig_i64_v128_to_void(i64_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 625: { // v128_mem: Sig_to_v128
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 626: { // v128_reg0: Sig_to_v128
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 627: { // v128_mem: Sig_v128_v128_to_v128(v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 628: { // v128_reg0: Sig_v128_v128_to_v128(v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 629: { // v128_mem: Sig_v128_v128_to_v128(v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 630: { // v128_reg0: Sig_v128_v128_to_v128(v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 631: { // v128_mem: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99994444u, 0x99999999u);
        break;
    }
    case 632: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99994444u, 0x99999944u);
        break;
    }
    case 633: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99994444u, 0x99999944u);
        break;
    }
    case 634: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99994444u, 0x99999944u);
        break;
    }
    case 635: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99994444u, 0x99999944u);
        break;
    }
    case 636: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99994444u, 0x99999944u);
        break;
    }
    case 637: { // v128_mem: Sig_i32_v128_to_v128(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 638: { // v128_reg0: Sig_i32_v128_to_v128(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 639: { // v128_mem: Sig_i32_v128_to_v128(i32_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 640: { // v128_reg0: Sig_i32_v128_to_v128(i32_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 641: { // v128_mem: Sig_i64_v128_to_v128(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 642: { // v128_reg0: Sig_i64_v128_to_v128(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 643: { // v128_mem: Sig_i64_v128_to_v128(i64_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 644: { // v128_reg0: Sig_i64_v128_to_v128(i64_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 645: { // v128_mem: Sig_f32_to_v128(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 646: { // v128_reg0: Sig_f32_to_v128(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 647: { // v128_mem: Sig_f32_to_v128(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 648: { // v128_reg0: Sig_f32_to_v128(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999944u);
        break;
    }
    case 649: { // v128_mem: Sig_f64_to_v128(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 650: { // v128_reg0: Sig_f64_to_v128(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 651: { // v128_mem: Sig_f64_to_v128(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 652: { // v128_reg0: Sig_f64_to_v128(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999944u);
        break;
    }
    case 653: { // i32_mem: Sig_v128_to_i32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 654: { // i32_reg0: Sig_v128_to_i32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 655: { // i32_mem: Sig_v128_to_i32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 656: { // i32_reg0: Sig_v128_to_i32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999990u);
        break;
    }
    case 657: { // v128_mem: Sig_v128_to_v128(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 658: { // v128_reg0: Sig_v128_to_v128(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 659: { // v128_mem: Sig_v128_to_v128(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 660: { // v128_reg0: Sig_v128_to_v128(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 661: { // v128_mem: Sig_v128_i32_to_v128(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 662: { // v128_reg0: Sig_v128_i32_to_v128(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 663: { // v128_mem: Sig_v128_i32_to_v128(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 664: { // v128_reg0: Sig_v128_i32_to_v128(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 665: { // v128_mem: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999440u, 0x99999999u);
        break;
    }
    case 666: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999440u, 0x99999944u);
        break;
    }
    case 667: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999440u, 0x99999944u);
        break;
    }
    case 668: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999440u, 0x99999944u);
        break;
    }
    case 669: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999440u, 0x99999944u);
        break;
    }
    case 670: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999440u, 0x99999944u);
        break;
    }
    case 671: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999440u, 0x99999944u);
        break;
    }
    case 672: { // i64_mem: Sig_v128_to_i64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 673: { // i64_reg0: Sig_v128_to_i64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 674: { // i64_mem: Sig_v128_to_i64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 675: { // i64_reg0: Sig_v128_to_i64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999991u);
        break;
    }
    case 676: { // v128_mem: Sig_v128_i64_to_v128(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 677: { // v128_reg0: Sig_v128_i64_to_v128(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 678: { // v128_mem: Sig_v128_i64_to_v128(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 679: { // v128_reg0: Sig_v128_i64_to_v128(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 680: { // v128_mem: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999441u, 0x99999999u);
        break;
    }
    case 681: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999441u, 0x99999944u);
        break;
    }
    case 682: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999441u, 0x99999944u);
        break;
    }
    case 683: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999441u, 0x99999944u);
        break;
    }
    case 684: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999441u, 0x99999944u);
        break;
    }
    case 685: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999441u, 0x99999944u);
        break;
    }
    case 686: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999441u, 0x99999944u);
        break;
    }
    case 687: { // f32_mem: Sig_v128_to_f32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 688: { // f32_reg0: Sig_v128_to_f32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 689: { // f32_mem: Sig_v128_to_f32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 690: { // f32_reg0: Sig_v128_to_f32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999992u);
        break;
    }
    case 691: { // v128_mem: Sig_v128_f32_to_v128(v128_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 692: { // v128_reg0: Sig_v128_f32_to_v128(v128_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 693: { // v128_mem: Sig_v128_f32_to_v128(v128_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 694: { // v128_reg0: Sig_v128_f32_to_v128(v128_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999992u, 0x99999944u);
        break;
    }
    case 695: { // v128_mem: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999442u, 0x99999999u);
        break;
    }
    case 696: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999442u, 0x99999944u);
        break;
    }
    case 697: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999442u, 0x99999944u);
        break;
    }
    case 698: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999442u, 0x99999944u);
        break;
    }
    case 699: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999442u, 0x99999944u);
        break;
    }
    case 700: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999442u, 0x99999944u);
        break;
    }
    case 701: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999442u, 0x99999944u);
        break;
    }
    case 702: { // f64_mem: Sig_v128_to_f64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 703: { // f64_reg0: Sig_v128_to_f64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 704: { // f64_mem: Sig_v128_to_f64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 705: { // f64_reg0: Sig_v128_to_f64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999993u);
        break;
    }
    case 706: { // v128_mem: Sig_v128_f64_to_v128(v128_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 707: { // v128_reg0: Sig_v128_f64_to_v128(v128_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 708: { // v128_mem: Sig_v128_f64_to_v128(v128_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 709: { // v128_reg0: Sig_v128_f64_to_v128(v128_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 1, 0x99999993u, 0x99999944u);
        break;
    }
    case 710: { // v128_mem: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999443u, 0x99999999u);
        break;
    }
    case 711: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 3, 0x99999443u, 0x99999944u);
        break;
    }
    case 712: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99999443u, 0x99999944u);
        break;
    }
    case 713: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 5, 0x99999443u, 0x99999944u);
        break;
    }
    case 714: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99999443u, 0x99999944u);
        break;
    }
    case 715: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99999443u, 0x99999944u);
        break;
    }
    case 716: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99999443u, 0x99999944u);
        break;
    }
    case 717: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 718: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 719: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 720: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 721: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_mem, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99994444u, 0x99999999u);
        break;
    }
    case 722: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_mem, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 4, 0x99994444u, 0x99999944u);
        break;
    }
    case 723: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99444444u, 0x99999999u);
        break;
    }
    case 724: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 6, 0x99444444u, 0x99999944u);
        break;
    }
    case 725: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 7, 0x99444444u, 0x99999944u);
        break;
    }
    case 726: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_tile_pick(node, 8, 0x99444444u, 0x99999944u);
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
