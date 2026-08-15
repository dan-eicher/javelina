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
                p->rule[1] = 994;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 995;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 996;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 997;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 998;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 999;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 1000;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 1001;
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
                p->rule[1] = 985;
            }
        }
        break;
    case BURG_Sig_i32_v128_to_void_aw:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 982;
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
                p->rule[1] = 979;
            }
        }
        if (p->child_count >= 5 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7] && p->children[3]->rule[2] && p->children[4]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + p->children[3]->cost[2] + p->children[4]->cost[8] + 5;
            for (int _ci = 5; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 980;
            }
        }
        if (p->child_count >= 5 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7] && p->children[3]->rule[9] && p->children[4]->rule[8] && (JAV_TNEED(node,4) <= 7)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + p->children[3]->cost[9] + p->children[4]->cost[8] + 4;
            for (int _ci = 5; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 981;
            }
        }
        break;
    case BURG_Sig_ref_i32_v128_i32_to_void_aw:
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[6] && p->children[3]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[6] + p->children[3]->cost[2] + 6;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 975;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[6] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[6] + p->children[3]->cost[8] + 5;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 976;
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
                p->rule[1] = 973;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[5] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[5] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 974;
            }
        }
        break;
    case BURG_Sig_ref_i32_v128_to_void_aw:
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[6]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[6] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 963;
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
                p->rule[1] = 962;
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
                p->rule[1] = 960;
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
                p->rule[1] = 969;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[3] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[3] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 970;
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
                p->rule[1] = 959;
            }
        }
        break;
    case BURG_Sig_ref_v128_to_void_aw:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 957;
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
                p->rule[1] = 983;
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
                p->rule[1] = 956;
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
                p->rule[1] = 953;
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
                p->rule[1] = 952;
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
                p->rule[1] = 947;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 948;
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
                p->rule[1] = 945;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 946;
            }
        }
        break;
    case BURG_Sig_v128_to_void_aw:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 944;
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
                p->rule[1] = 943;
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
                p->rule[1] = 937;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[5] && p->children[2]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[5] + p->children[2]->cost[16] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 938;
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
                p->rule[1] = 935;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[4] && p->children[2]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[4] + p->children[2]->cost[16] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 936;
            }
        }
        break;
    case BURG_Sig_i32_v128_i32_to_void_aw:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[6] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[6] + p->children[2]->cost[2] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 931;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[6] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[6] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 932;
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
                p->rule[1] = 929;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[5] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[5] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 930;
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
                p->rule[1] = 917;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[16] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 918;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[17] && p->children[2]->rule[16] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[17] + p->children[2]->cost[16] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 919;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 920;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 5)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 921;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 922;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 923;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 3 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 924;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 2 && JAV_TNEED(node,2) <= 1)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 925;
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
                p->rule[1] = 890;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[2] && p->children[2]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + p->children[2]->cost[16] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 891;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[9] && p->children[2]->rule[16] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[9] + p->children[2]->cost[16] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 892;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 893;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 5)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 894;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 895;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 896;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 3 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 897;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[16] && (JAV_TNEED(node,1) <= 2 && JAV_TNEED(node,2) <= 1)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[16] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 898;
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
                p->rule[1] = 872;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 873;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 874;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 875;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 876;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 877;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 878;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 879;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 880;
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
                p->rule[1] = 863;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 864;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 865;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 866;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 867;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 868;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 869;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 870;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 871;
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
                p->rule[1] = 854;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 855;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 856;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 857;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 858;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 859;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 860;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 861;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 862;
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
                p->rule[1] = 836;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 837;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 838;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 839;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 840;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 841;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 842;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 843;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 844;
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
                p->rule[1] = 827;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 828;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 829;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 830;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 831;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 832;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 833;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 834;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 835;
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
                p->rule[1] = 818;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 819;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 820;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 821;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 822;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 823;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 824;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 825;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 826;
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
                p->rule[1] = 808;
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
                p->rule[1] = 800;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 801;
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
                p->rule[1] = 798;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 799;
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
                p->rule[7] = 797;
                closure_ref_mem(p, c, node);
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
                p->rule[7] = 795;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[7] && p->children[1]->rule[7] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 796;
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
                p->rule[7] = 793;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 794;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i64_to_ref_aw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 791;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 792;
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
                p->rule[7] = 789;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 790;
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
                p->rule[7] = 785;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 786;
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
                p->rule[7] = 781;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 782;
                closure_ref_mem(p, c, node);
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
                p->rule[1] = 806;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 807;
            }
        }
        break;
    case BURG_Sig_v128_i32_to_ref_aw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 779;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 780;
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
                p->rule[7] = 775;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 776;
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
                p->rule[7] = 768;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 769;
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
                p->rule[7] = 765;
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
                p->rule[7] = 764;
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
                p->rule[6] = 743;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[5] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 744;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[32] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 745;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 746;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 747;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 748;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 749;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 750;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 751;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 752;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 753;
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
                p->rule[6] = 754;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[6] + 7;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 755;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[40] + 7;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 756;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[40] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 757;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[42] + p->children[2]->cost[40] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 758;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[42] + p->children[2]->cost[40] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 759;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 760;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 761;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 762;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[44] && p->children[1]->rule[42] && p->children[2]->rule[40] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[44] + p->children[1]->cost[42] + p->children[2]->cost[40] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 763;
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
                p->rule[6] = 732;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[4] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 733;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[24] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 734;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 735;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 736;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 737;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 738;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 739;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 740;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 741;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 742;
                closure_v128_reg0(p, c, node);
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
                p->rule[6] = 706;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 707;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 708;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 709;
                closure_v128_reg0(p, c, node);
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
                p->rule[6] = 702;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 703;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 704;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 705;
                closure_v128_reg0(p, c, node);
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
                p->rule[6] = 698;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 699;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 700;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 701;
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
                p->rule[4] = 373;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 374;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 375;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 376;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 377;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 378;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 379;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 380;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 381;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 382;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 383;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 384;
                closure_f32_reg0(p, c, node);
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
                p->rule[1] = 958;
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
                p->rule[4] = 365;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 366;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 367;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 368;
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
                p->rule[3] = 355;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 356;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 357;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 358;
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
                p->rule[1] = 964;
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
                p->rule[4] = 369;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 370;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 371;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 372;
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
                p->rule[3] = 351;
                closure_i64_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 352;
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
                p->rule[3] = 334;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 335;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 336;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 337;
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
                p->rule[3] = 330;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 331;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 332;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 333;
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
                p->rule[6] = 627;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 628;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[16] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 629;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 630;
                closure_v128_reg0(p, c, node);
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
                p->rule[6] = 587;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 588;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 589;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 590;
                closure_v128_reg0(p, c, node);
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
                p->rule[1] = 949;
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
                p->rule[1] = 809;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 810;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 811;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 812;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 813;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 814;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 815;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 816;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 817;
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
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 169;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 170;
                closure_i32_reg0(p, c, node);
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
                p->rule[3] = 326;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 327;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 328;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 329;
                closure_i64_reg0(p, c, node);
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
                p->rule[1] = 986;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 987;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 988;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 989;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 990;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 991;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 992;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 993;
            }
        }
        break;
    case BURG_Sig_v128_i64_to_i32_aw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 191;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 192;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 193;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 194;
                closure_i32_reg0(p, c, node);
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
                p->rule[7] = 787;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 788;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i32_to_i64_aw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 290;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 291;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 292;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 293;
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
                p->rule[3] = 294;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 295;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 296;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 297;
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
                p->rule[3] = 286;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 287;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 288;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 289;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_to_v128_aw:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 5;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 642;
                closure_v128_mem(p, c, node);
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
                p->rule[6] = 643;
                closure_v128_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 644;
                closure_v128_reg0(p, c, node);
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
                p->rule[4] = 389;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 390;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 391;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 392;
                closure_f32_reg0(p, c, node);
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
                p->rule[2] = 248;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[7]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[7] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 249;
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
                p->rule[3] = 278;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 279;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 280;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 281;
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
                p->rule[6] = 660;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 661;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 662;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 663;
                closure_v128_reg0(p, c, node);
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
                p->rule[7] = 773;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 774;
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
                p->rule[3] = 302;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 303;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 304;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 305;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 306;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 307;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 308;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 309;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 310;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 311;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 312;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 313;
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
    case BURG_Sig_v128_i32_to_i32_aw:
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
    case BURG_Sig_v128_to_i32:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 252;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 253;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 254;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 255;
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
                p->rule[3] = 256;
                closure_i64_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 257;
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
                p->rule[6] = 676;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 677;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[40] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 678;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 679;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 680;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 681;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 682;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 683;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 684;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 685;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[10] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 686;
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
                p->rule[4] = 385;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 386;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 387;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 388;
                closure_f32_reg0(p, c, node);
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
                p->rule[1] = 804;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 805;
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
                p->rule[6] = 617;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 618;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[16] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 619;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 620;
                closure_v128_reg0(p, c, node);
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
                p->rule[6] = 631;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[2] + 6;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 632;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[8] + 7;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 633;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[6] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + p->children[2]->cost[8] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 634;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[41] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[41] + p->children[2]->cost[8] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 635;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[6] && p->children[1]->rule[41] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[41] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 636;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[43] && p->children[1]->rule[41] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[43] + p->children[1]->cost[41] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 637;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[43] && p->children[1]->rule[41] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[43] + p->children[1]->cost[41] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 638;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[43] && p->children[1]->rule[41] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[43] + p->children[1]->cost[41] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 639;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[43] && p->children[1]->rule[41] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[43] + p->children[1]->cost[41] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 640;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[43] && p->children[1]->rule[41] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 3 && JAV_TNEED(node,2) <= 1)) {
            int c = p->children[0]->cost[43] + p->children[1]->cost[41] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 641;
                closure_v128_reg0(p, c, node);
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
                p->rule[1] = 802;
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 803;
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
                p->rule[3] = 266;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 267;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 268;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 269;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 270;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 271;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 272;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 273;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 274;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 275;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 276;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 277;
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
                p->rule[1] = 845;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 846;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 847;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 848;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 849;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 850;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 851;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 852;
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 853;
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
                p->rule[6] = 621;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 622;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[16] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 623;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 624;
                closure_v128_reg0(p, c, node);
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
                p->rule[3] = 353;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 354;
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
                p->rule[5] = 508;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 509;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 510;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 511;
                closure_f64_reg0(p, c, node);
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
                p->rule[3] = 338;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 339;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 340;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[3] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 341;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[17] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 342;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[17] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 343;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 344;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 345;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 5)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 346;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 347;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 348;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 3 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 349;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 2 && JAV_TNEED(node,2) <= 1)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 350;
                closure_i64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_to_v128_aw:
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 649;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 650;
                closure_v128_mem(p, c, node);
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
    case BURG_Sig_v128_to_i32_aw:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 250;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 251;
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
                p->rule[4] = 361;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 362;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 363;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 364;
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
                p->rule[4] = 359;
                closure_f32_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 360;
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
                p->rule[2] = 246;
                closure_i32_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 247;
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
                p->rule[3] = 282;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 283;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 284;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 285;
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
                p->rule[2] = 195;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 196;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 197;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 198;
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
    case BURG_Sig_i64_v128_i64_to_void_aw:
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[6] && p->children[2]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[6] + p->children[2]->cost[3] + 5;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 939;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[6] && p->children[2]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[6] + p->children[2]->cost[16] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 940;
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
                p->rule[1] = 899;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 900;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[9] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 901;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 902;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 5)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 903;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 904;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 905;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 3 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 906;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[18] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 2 && JAV_TNEED(node,2) <= 1)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 907;
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
                p->rule[4] = 429;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 430;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 431;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 432;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 433;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 434;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 435;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 436;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 437;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 438;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 439;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 440;
                closure_f32_reg0(p, c, node);
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
                p->rule[4] = 460;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 461;
                closure_f32_reg0(p, c, node);
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
                p->rule[2] = 231;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 232;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 233;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 234;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[9] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 235;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[9] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 236;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 237;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 238;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 5)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 239;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 240;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 241;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 3 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 242;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 2 && JAV_TNEED(node,2) <= 1)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 243;
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
                p->rule[3] = 318;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 319;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 320;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 321;
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
                p->rule[6] = 583;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 584;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 585;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 586;
                closure_v128_reg0(p, c, node);
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
                p->rule[2] = 183;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 184;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 185;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 186;
                closure_i32_reg0(p, c, node);
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
                p->rule[2] = 223;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 224;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 225;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 226;
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
                p->rule[6] = 599;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 600;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 601;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 602;
                closure_v128_reg0(p, c, node);
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
                p->rule[1] = 977;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[7] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[7] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 978;
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
                p->rule[3] = 314;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 315;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 316;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 317;
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
                p->rule[2] = 227;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 228;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 229;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 230;
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
                p->rule[3] = 262;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 263;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 264;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 265;
                closure_i64_reg0(p, c, node);
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
                p->rule[6] = 591;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 592;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 593;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 594;
                closure_v128_reg0(p, c, node);
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
                p->rule[1] = 965;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[2] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[2] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 966;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[9] && p->children[3]->rule[8] && (JAV_TNEED(node,3) <= 7)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[9] + p->children[3]->cost[8] + 3;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 967;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[10] && p->children[2]->rule[9] && p->children[3]->rule[8] && (JAV_TNEED(node,2) <= 7 && JAV_TNEED(node,3) <= 6)) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[10] + p->children[2]->cost[9] + p->children[3]->cost[8] + 2;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 968;
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
                p->rule[3] = 258;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 259;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 260;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 261;
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
                p->rule[2] = 199;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[4]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 200;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 201;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[24]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 202;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 203;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 204;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 205;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 206;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 207;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 208;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 209;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[25] && p->children[1]->rule[24] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[25] + p->children[1]->cost[24] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 210;
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
                p->rule[2] = 211;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 212;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 213;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 214;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 215;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 216;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 217;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 218;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 219;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 220;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 221;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 222;
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
                p->rule[4] = 417;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 418;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 419;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 420;
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
                p->rule[5] = 524;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 525;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 526;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 527;
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
                p->rule[2] = 171;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 172;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 173;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 174;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 175;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 176;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 177;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 178;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 179;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 180;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 181;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[17] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[17] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 182;
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
    case BURG_Sig_i32_ref_i32_to_void:
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[7] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 933;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[7] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[7] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 934;
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
                p->rule[2] = 244;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 245;
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
                p->rule[4] = 393;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 394;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 395;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 396;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i32_to_f32_aw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 397;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 398;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 399;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 400;
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
                p->rule[4] = 405;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 406;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 407;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 408;
                closure_f32_reg0(p, c, node);
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
                p->rule[5] = 492;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 493;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 494;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 495;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_stk_v128_to_void_aw:
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 951;
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
                p->rule[4] = 409;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 410;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 411;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 412;
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
                p->rule[5] = 532;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 533;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 534;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 535;
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
                p->rule[4] = 413;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 414;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 415;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 416;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_to_v128_aw:
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 654;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 655;
                closure_v128_mem(p, c, node);
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
                p->rule[1] = 927;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[4] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[4] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 928;
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
                p->rule[4] = 401;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 402;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 403;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 404;
                closure_f32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i64_to_f32_aw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 421;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 422;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 423;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 424;
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
                p->rule[5] = 548;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[4]) {
            int c = p->children[0]->cost[4] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 549;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 550;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[24]) {
            int c = p->children[0]->cost[24] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 551;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i64_to_i64_aw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 322;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 323;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 324;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 325;
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
                p->rule[4] = 425;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 426;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 427;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 428;
                closure_f32_reg0(p, c, node);
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
                p->rule[4] = 441;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 442;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 443;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 444;
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
                p->rule[4] = 445;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[4] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 446;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[4] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 447;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[4] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[4] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 448;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[25] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[25] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 449;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[4] && p->children[1]->rule[25] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[25] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 450;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[26] && p->children[1]->rule[25] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[26] + p->children[1]->cost[25] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 451;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[26] && p->children[1]->rule[25] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[26] + p->children[1]->cost[25] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 452;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[26] && p->children[1]->rule[25] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 5)) {
            int c = p->children[0]->cost[26] + p->children[1]->cost[25] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 453;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[26] && p->children[1]->rule[25] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[26] + p->children[1]->cost[25] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 454;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[26] && p->children[1]->rule[25] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[26] + p->children[1]->cost[25] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 455;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[26] && p->children[1]->rule[25] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 3 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[26] + p->children[1]->cost[25] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 456;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[26] && p->children[1]->rule[25] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 2 && JAV_TNEED(node,2) <= 1)) {
            int c = p->children[0]->cost[26] + p->children[1]->cost[25] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 457;
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
                p->rule[7] = 777;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 778;
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
                p->rule[4] = 458;
                closure_f32_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 459;
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
                p->rule[7] = 766;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 767;
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
                p->rule[6] = 721;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 722;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 723;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 724;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 725;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 726;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 727;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 728;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 729;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 730;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[16] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[16] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 731;
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
                p->rule[5] = 536;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[5]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 537;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 538;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[32]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 539;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 540;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 541;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 542;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 543;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 544;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 545;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 546;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[33] && p->children[1]->rule[32] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[33] + p->children[1]->cost[32] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 547;
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
                p->rule[6] = 595;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 596;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 597;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 598;
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
                p->rule[4] = 462;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 463;
                closure_f32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[4]) {
                p->cost[4] = c;
                p->rule[4] = 464;
                closure_f32_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[24]) {
                p->cost[24] = c;
                p->rule[24] = 465;
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
                p->rule[5] = 466;
                closure_f64_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 467;
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
                p->rule[5] = 468;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[5]) {
            int c = p->children[0]->cost[5] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 469;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 470;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[32]) {
            int c = p->children[0]->cost[32] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 471;
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
                p->rule[5] = 472;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 473;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 474;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 475;
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
                p->rule[1] = 971;
            }
        }
        if (p->child_count >= 4 && p->children[0]->rule[7] && p->children[1]->rule[2] && p->children[2]->rule[4] && p->children[3]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + p->children[2]->cost[4] + p->children[3]->cost[8] + 4;
            for (int _ci = 4; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 972;
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
                p->rule[6] = 613;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 614;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 615;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 616;
                closure_v128_reg0(p, c, node);
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
                p->rule[5] = 476;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[3]) {
            int c = p->children[0]->cost[3] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 477;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 478;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[16]) {
            int c = p->children[0]->cost[16] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 479;
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
                p->rule[5] = 480;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 481;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 482;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 483;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 484;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 485;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 486;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 487;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 488;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 489;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 490;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 491;
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
                p->rule[5] = 496;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 497;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 498;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 499;
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
                p->rule[1] = 941;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[3] && p->children[1]->rule[7] && p->children[2]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[7] + p->children[2]->cost[16] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 942;
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
                p->rule[5] = 500;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 501;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 502;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 503;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i32_to_f64_aw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 504;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 505;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 506;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 507;
                closure_f64_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_i64_v128_to_void_aw:
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 984;
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
                p->rule[1] = 926;
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
                p->rule[5] = 512;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 513;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 514;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 515;
                closure_f64_reg0(p, c, node);
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
                p->rule[5] = 516;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 517;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 518;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 519;
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
                p->rule[5] = 520;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 521;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 522;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[4] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[4] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 523;
                closure_f64_reg0(p, c, node);
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
                p->rule[1] = 961;
            }
        }
        break;
    case BURG_Sig_v128_i64_to_f64_aw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 528;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 529;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 530;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 531;
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
                p->rule[1] = 955;
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
                p->rule[6] = 575;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 576;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 577;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 578;
                closure_v128_reg0(p, c, node);
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
                p->rule[5] = 552;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[5] && p->children[2]->rule[2]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + p->children[2]->cost[2] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 553;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[5] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + p->children[2]->cost[8] + 4;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 554;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[5] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[5] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 555;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[33] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[33] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 556;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[5] && p->children[1]->rule[33] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[33] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 557;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[34] && p->children[1]->rule[33] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[34] + p->children[1]->cost[33] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 558;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[34] && p->children[1]->rule[33] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[34] + p->children[1]->cost[33] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 559;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[34] && p->children[1]->rule[33] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 5)) {
            int c = p->children[0]->cost[34] + p->children[1]->cost[33] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 560;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[34] && p->children[1]->rule[33] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[34] + p->children[1]->cost[33] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 561;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[34] && p->children[1]->rule[33] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[34] + p->children[1]->cost[33] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 562;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[34] && p->children[1]->rule[33] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 3 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[34] + p->children[1]->cost[33] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 563;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[34] && p->children[1]->rule[33] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 2 && JAV_TNEED(node,2) <= 1)) {
            int c = p->children[0]->cost[34] + p->children[1]->cost[33] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 564;
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
                p->rule[5] = 567;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 568;
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
                p->rule[6] = 573;
                closure_v128_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 574;
                closure_v128_reg0(p, c, node);
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
                p->rule[5] = 569;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[6]) {
            int c = p->children[0]->cost[6] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 570;
                closure_f64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[5]) {
                p->cost[5] = c;
                p->rule[5] = 571;
                closure_f64_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[40]) {
            int c = p->children[0]->cost[40] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[32]) {
                p->cost[32] = c;
                p->rule[32] = 572;
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
                p->rule[3] = 298;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 299;
                closure_i64_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[3]) {
                p->cost[3] = c;
                p->rule[3] = 300;
                closure_i64_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[16]) {
                p->cost[16] = c;
                p->rule[16] = 301;
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
                p->rule[6] = 609;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 610;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 611;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 612;
                closure_v128_reg0(p, c, node);
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
                p->rule[6] = 579;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 580;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 581;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 582;
                closure_v128_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i32_to_v128_pw_aw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 603;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 604;
                closure_v128_mem(p, c, node);
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
                p->rule[6] = 605;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 606;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 607;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 608;
                closure_v128_reg0(p, c, node);
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
                p->rule[2] = 187;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[3] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 188;
                closure_i32_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[2]) {
                p->cost[2] = c;
                p->rule[2] = 189;
                closure_i32_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[5] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[5] + p->children[1]->cost[16] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[8]) {
                p->cost[8] = c;
                p->rule[8] = 190;
                closure_i32_reg0(p, c, node);
            }
        }
        break;
    case BURG_Sig_v128_i64_to_v128_pw_aw:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[3]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[3] + 6;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 625;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[16] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 626;
                closure_v128_mem(p, c, node);
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
                p->rule[1] = 881;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[2] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 882;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[9] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 883;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 884;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 5)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 885;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 886;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 887;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 3 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 888;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[9] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 2 && JAV_TNEED(node,2) <= 1)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[9] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 889;
            }
        }
        break;
    case BURG_Sig_ref_to_v128_aw:
        if (p->child_count >= 1 && p->children[0]->rule[7]) {
            int c = p->children[0]->cost[7] + 4;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 645;
                closure_v128_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_ref_i32_to_v128_aw:
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 646;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[7] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[7] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 647;
                closure_v128_mem(p, c, node);
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
                p->rule[1] = 954;
            }
        }
        break;
    case BURG_Sig_to_v128_aw:
        {
            int c = 0 + 3;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 648;
                closure_v128_mem(p, c, node);
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
                p->rule[7] = 770;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 771;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 772;
                closure_ref_mem(p, c, node);
            }
        }
        break;
    case BURG_Sig_i32_i32_to_v128_aw:
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[2] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 651;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[8] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 652;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[9] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[9] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 653;
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
                p->rule[6] = 656;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[2]) {
            int c = p->children[0]->cost[2] + 2;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 657;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 3;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 658;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 1 && p->children[0]->rule[8]) {
            int c = p->children[0]->cost[8] + 0;
            for (int _ci = 1; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 659;
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
                p->rule[1] = 908;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[3] && p->children[2]->rule[8]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[3] + p->children[2]->cost[8] + 3;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 909;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[2] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,2) <= 7)) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[17] + p->children[2]->cost[8] + 2;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 910;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 7 && JAV_TNEED(node,2) <= 6)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 911;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 6 && JAV_TNEED(node,2) <= 5)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 912;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 5 && JAV_TNEED(node,2) <= 4)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 913;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 4 && JAV_TNEED(node,2) <= 3)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 914;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 3 && JAV_TNEED(node,2) <= 2)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 915;
            }
        }
        if (p->child_count >= 3 && p->children[0]->rule[10] && p->children[1]->rule[17] && p->children[2]->rule[8] && (JAV_TNEED(node,1) <= 2 && JAV_TNEED(node,2) <= 1)) {
            int c = p->children[0]->cost[10] + p->children[1]->cost[17] + p->children[2]->cost[8] + 0;
            for (int _ci = 3; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[1]) {
                p->cost[1] = c;
                p->rule[1] = 916;
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
                p->rule[6] = 710;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[2]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[2] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 711;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 712;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[8]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 713;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 714;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 715;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 716;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 717;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 718;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 719;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[41] && p->children[1]->rule[8] && (JAV_TNEED(node,1) <= 1)) {
            int c = p->children[0]->cost[41] + p->children[1]->cost[8] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 720;
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
                p->rule[6] = 664;
                closure_v128_mem(p, c, node);
            }
        }
        {
            int c = 0 + 0;
            for (int _ci = 0; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 665;
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
                p->rule[1] = 950;
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
    case BURG_Sig_v128_v128_to_v128:
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 7;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 666;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[6] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 667;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[40] + 5;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 668;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[6] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[6] + p->children[1]->cost[40] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 669;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 670;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 671;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 672;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 673;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 674;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[42] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[42] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 675;
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
                p->rule[7] = 783;
                closure_ref_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[2] && p->children[1]->rule[16]) {
            int c = p->children[0]->cost[2] + p->children[1]->cost[16] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[7]) {
                p->cost[7] = c;
                p->rule[7] = 784;
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
                p->rule[6] = 687;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[6]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[6] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 688;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[40] + 4;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 689;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[3] && p->children[1]->rule[40]) {
            int c = p->children[0]->cost[3] + p->children[1]->cost[40] + 2;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 690;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 3;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[6]) {
                p->cost[6] = c;
                p->rule[6] = 691;
                closure_v128_mem(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 7)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 692;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 6)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 693;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 5)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 694;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 4)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 695;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 3)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 696;
                closure_v128_reg0(p, c, node);
            }
        }
        if (p->child_count >= 2 && p->children[0]->rule[18] && p->children[1]->rule[40] && (JAV_TNEED(node,1) <= 2)) {
            int c = p->children[0]->cost[18] + p->children[1]->cost[40] + 0;
            for (int _ci = 2; _ci < p->child_count; _ci++)
                c += p->children[_ci]->cost[1];
            if (c < p->cost[40]) {
                p->cost[40] = c;
                p->rule[40] = 697;
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
    case 159: { // i32_mem: Sig_v128_i32_to_i32_aw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 160: { // i32_reg0: Sig_v128_i32_to_i32_aw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 161: { // i32_mem: Sig_v128_i32_to_i32_aw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 162: { // i32_reg0: Sig_v128_i32_to_i32_aw(v128_mem, i32_reg0)
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
    case 169: { // i32_mem: Sig_i32_i64_to_i32(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 170: { // i32_reg0: Sig_i32_i64_to_i32(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999990u);
        break;
    }
    case 171: { // i32_mem: Sig_i64_i64_to_i32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 172: { // i32_reg0: Sig_i64_i64_to_i32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 173: { // i32_mem: Sig_i64_i64_to_i32(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 174: { // i32_reg0: Sig_i64_i64_to_i32(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999990u);
        break;
    }
    case 175: { // i32_mem: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999911u, 0x99999999u);
        break;
    }
    case 176: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999911u, 0x99999990u);
        break;
    }
    case 177: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999911u, 0x99999990u);
        break;
    }
    case 178: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999911u, 0x99999990u);
        break;
    }
    case 179: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999911u, 0x99999990u);
        break;
    }
    case 180: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999911u, 0x99999990u);
        break;
    }
    case 181: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999911u, 0x99999990u);
        break;
    }
    case 182: { // i32_reg0: Sig_i64_i64_to_i32(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999911u, 0x99999990u);
        break;
    }
    case 183: { // i32_mem: Sig_f32_i64_to_i32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 184: { // i32_reg0: Sig_f32_i64_to_i32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 185: { // i32_mem: Sig_f32_i64_to_i32(f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 186: { // i32_reg0: Sig_f32_i64_to_i32(f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999990u);
        break;
    }
    case 187: { // i32_mem: Sig_f64_i64_to_i32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 188: { // i32_reg0: Sig_f64_i64_to_i32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 189: { // i32_mem: Sig_f64_i64_to_i32(f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 190: { // i32_reg0: Sig_f64_i64_to_i32(f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999990u);
        break;
    }
    case 191: { // i32_mem: Sig_v128_i64_to_i32_aw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 192: { // i32_reg0: Sig_v128_i64_to_i32_aw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 193: { // i32_mem: Sig_v128_i64_to_i32_aw(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 194: { // i32_reg0: Sig_v128_i64_to_i32_aw(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999990u);
        break;
    }
    case 195: { // i32_mem: Sig_ref_i64_to_i32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 196: { // i32_reg0: Sig_ref_i64_to_i32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 197: { // i32_mem: Sig_ref_i64_to_i32(ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 198: { // i32_reg0: Sig_ref_i64_to_i32(ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999990u);
        break;
    }
    case 199: { // i32_mem: Sig_f32_f32_to_i32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 200: { // i32_reg0: Sig_f32_f32_to_i32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 201: { // i32_mem: Sig_f32_f32_to_i32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 202: { // i32_reg0: Sig_f32_f32_to_i32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999990u);
        break;
    }
    case 203: { // i32_mem: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999922u, 0x99999999u);
        break;
    }
    case 204: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999922u, 0x99999990u);
        break;
    }
    case 205: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999922u, 0x99999990u);
        break;
    }
    case 206: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999922u, 0x99999990u);
        break;
    }
    case 207: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999922u, 0x99999990u);
        break;
    }
    case 208: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999922u, 0x99999990u);
        break;
    }
    case 209: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999922u, 0x99999990u);
        break;
    }
    case 210: { // i32_reg0: Sig_f32_f32_to_i32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999922u, 0x99999990u);
        break;
    }
    case 211: { // i32_mem: Sig_f64_f64_to_i32(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 212: { // i32_reg0: Sig_f64_f64_to_i32(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 213: { // i32_mem: Sig_f64_f64_to_i32(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 214: { // i32_reg0: Sig_f64_f64_to_i32(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999990u);
        break;
    }
    case 215: { // i32_mem: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999933u, 0x99999999u);
        break;
    }
    case 216: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999933u, 0x99999990u);
        break;
    }
    case 217: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999933u, 0x99999990u);
        break;
    }
    case 218: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999933u, 0x99999990u);
        break;
    }
    case 219: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999933u, 0x99999990u);
        break;
    }
    case 220: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999933u, 0x99999990u);
        break;
    }
    case 221: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999933u, 0x99999990u);
        break;
    }
    case 222: { // i32_reg0: Sig_f64_f64_to_i32(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999933u, 0x99999990u);
        break;
    }
    case 223: { // i32_mem: Sig_f32_to_i32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 224: { // i32_reg0: Sig_f32_to_i32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 225: { // i32_mem: Sig_f32_to_i32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 226: { // i32_reg0: Sig_f32_to_i32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999990u);
        break;
    }
    case 227: { // i32_mem: Sig_f64_to_i32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 228: { // i32_reg0: Sig_f64_to_i32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 229: { // i32_mem: Sig_f64_to_i32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 230: { // i32_reg0: Sig_f64_to_i32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999990u);
        break;
    }
    case 231: { // i32_mem: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 232: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 233: { // i32_mem: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 234: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999990u);
        break;
    }
    case 235: { // i32_mem: Sig_i32_i32_i32_to_i32(i32_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 236: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999990u);
        break;
    }
    case 237: { // i32_mem: Sig_i32_i32_i32_to_i32(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999000u, 0x99999999u);
        break;
    }
    case 238: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999000u, 0x99999990u);
        break;
    }
    case 239: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999000u, 0x99999990u);
        break;
    }
    case 240: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999000u, 0x99999990u);
        break;
    }
    case 241: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999000u, 0x99999990u);
        break;
    }
    case 242: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999000u, 0x99999990u);
        break;
    }
    case 243: { // i32_reg0: Sig_i32_i32_i32_to_i32(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999000u, 0x99999990u);
        break;
    }
    case 244: { // i32_mem: Sig_ref_to_i32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 245: { // i32_reg0: Sig_ref_to_i32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 246: { // i32_mem: Sig_stk_to_i32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 247: { // i32_reg0: Sig_stk_to_i32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 248: { // i32_mem: Sig_ref_ref_to_i32(ref_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 249: { // i32_reg0: Sig_ref_ref_to_i32(ref_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 250: { // i32_mem: Sig_v128_to_i32_aw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 251: { // i32_reg0: Sig_v128_to_i32_aw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 252: { // i32_mem: Sig_v128_to_i32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 253: { // i32_reg0: Sig_v128_to_i32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999990u);
        break;
    }
    case 254: { // i32_mem: Sig_v128_to_i32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 255: { // i32_reg0: Sig_v128_to_i32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999990u);
        break;
    }
    case 256: { // i64_mem: Sig_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 257: { // i64_reg0: Sig_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 258: { // i64_mem: Sig_i64_to_i64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 259: { // i64_reg0: Sig_i64_to_i64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 260: { // i64_mem: Sig_i64_to_i64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 261: { // i64_reg0: Sig_i64_to_i64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999991u);
        break;
    }
    case 262: { // i64_mem: Sig_i32_to_i64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 263: { // i64_reg0: Sig_i32_to_i64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 264: { // i64_mem: Sig_i32_to_i64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 265: { // i64_reg0: Sig_i32_to_i64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 266: { // i64_mem: Sig_i32_i32_to_i64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 267: { // i64_reg0: Sig_i32_i32_to_i64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 268: { // i64_mem: Sig_i32_i32_to_i64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 269: { // i64_reg0: Sig_i32_i32_to_i64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 270: { // i64_mem: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 271: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999991u);
        break;
    }
    case 272: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999900u, 0x99999991u);
        break;
    }
    case 273: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999900u, 0x99999991u);
        break;
    }
    case 274: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999900u, 0x99999991u);
        break;
    }
    case 275: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999900u, 0x99999991u);
        break;
    }
    case 276: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999900u, 0x99999991u);
        break;
    }
    case 277: { // i64_reg0: Sig_i32_i32_to_i64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999900u, 0x99999991u);
        break;
    }
    case 278: { // i64_mem: Sig_i64_i32_to_i64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 279: { // i64_reg0: Sig_i64_i32_to_i64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 280: { // i64_mem: Sig_i64_i32_to_i64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 281: { // i64_reg0: Sig_i64_i32_to_i64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 282: { // i64_mem: Sig_f32_i32_to_i64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 283: { // i64_reg0: Sig_f32_i32_to_i64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 284: { // i64_mem: Sig_f32_i32_to_i64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 285: { // i64_reg0: Sig_f32_i32_to_i64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 286: { // i64_mem: Sig_f64_i32_to_i64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 287: { // i64_reg0: Sig_f64_i32_to_i64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 288: { // i64_mem: Sig_f64_i32_to_i64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 289: { // i64_reg0: Sig_f64_i32_to_i64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 290: { // i64_mem: Sig_v128_i32_to_i64_aw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 291: { // i64_reg0: Sig_v128_i32_to_i64_aw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 292: { // i64_mem: Sig_v128_i32_to_i64_aw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 293: { // i64_reg0: Sig_v128_i32_to_i64_aw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 294: { // i64_mem: Sig_ref_i32_to_i64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 295: { // i64_reg0: Sig_ref_i32_to_i64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 296: { // i64_mem: Sig_ref_i32_to_i64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 297: { // i64_reg0: Sig_ref_i32_to_i64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 298: { // i64_mem: Sig_i32_i64_to_i64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 299: { // i64_reg0: Sig_i32_i64_to_i64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 300: { // i64_mem: Sig_i32_i64_to_i64(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 301: { // i64_reg0: Sig_i32_i64_to_i64(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999991u);
        break;
    }
    case 302: { // i64_mem: Sig_i64_i64_to_i64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 303: { // i64_reg0: Sig_i64_i64_to_i64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 304: { // i64_mem: Sig_i64_i64_to_i64(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 305: { // i64_reg0: Sig_i64_i64_to_i64(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999991u);
        break;
    }
    case 306: { // i64_mem: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999911u, 0x99999999u);
        break;
    }
    case 307: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999911u, 0x99999991u);
        break;
    }
    case 308: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999911u, 0x99999991u);
        break;
    }
    case 309: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999911u, 0x99999991u);
        break;
    }
    case 310: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999911u, 0x99999991u);
        break;
    }
    case 311: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999911u, 0x99999991u);
        break;
    }
    case 312: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999911u, 0x99999991u);
        break;
    }
    case 313: { // i64_reg0: Sig_i64_i64_to_i64(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999911u, 0x99999991u);
        break;
    }
    case 314: { // i64_mem: Sig_f32_i64_to_i64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 315: { // i64_reg0: Sig_f32_i64_to_i64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 316: { // i64_mem: Sig_f32_i64_to_i64(f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 317: { // i64_reg0: Sig_f32_i64_to_i64(f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999991u);
        break;
    }
    case 318: { // i64_mem: Sig_f64_i64_to_i64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 319: { // i64_reg0: Sig_f64_i64_to_i64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 320: { // i64_mem: Sig_f64_i64_to_i64(f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 321: { // i64_reg0: Sig_f64_i64_to_i64(f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999991u);
        break;
    }
    case 322: { // i64_mem: Sig_v128_i64_to_i64_aw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 323: { // i64_reg0: Sig_v128_i64_to_i64_aw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 324: { // i64_mem: Sig_v128_i64_to_i64_aw(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 325: { // i64_reg0: Sig_v128_i64_to_i64_aw(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999991u);
        break;
    }
    case 326: { // i64_mem: Sig_ref_i64_to_i64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 327: { // i64_reg0: Sig_ref_i64_to_i64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 328: { // i64_mem: Sig_ref_i64_to_i64(ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 329: { // i64_reg0: Sig_ref_i64_to_i64(ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999991u);
        break;
    }
    case 330: { // i64_mem: Sig_f32_to_i64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 331: { // i64_reg0: Sig_f32_to_i64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 332: { // i64_mem: Sig_f32_to_i64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 333: { // i64_reg0: Sig_f32_to_i64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999991u);
        break;
    }
    case 334: { // i64_mem: Sig_f64_to_i64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 335: { // i64_reg0: Sig_f64_to_i64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 336: { // i64_mem: Sig_f64_to_i64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 337: { // i64_reg0: Sig_f64_to_i64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999991u);
        break;
    }
    case 338: { // i64_mem: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 339: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 340: { // i64_mem: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 341: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_mem, i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999991u);
        break;
    }
    case 342: { // i64_mem: Sig_i64_i64_i32_to_i64(i64_mem, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999910u, 0x99999999u);
        break;
    }
    case 343: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_mem, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999910u, 0x99999991u);
        break;
    }
    case 344: { // i64_mem: Sig_i64_i64_i32_to_i64(i64_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999110u, 0x99999999u);
        break;
    }
    case 345: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999110u, 0x99999991u);
        break;
    }
    case 346: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999110u, 0x99999991u);
        break;
    }
    case 347: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999110u, 0x99999991u);
        break;
    }
    case 348: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999110u, 0x99999991u);
        break;
    }
    case 349: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999110u, 0x99999991u);
        break;
    }
    case 350: { // i64_reg0: Sig_i64_i64_i32_to_i64(i64_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999110u, 0x99999991u);
        break;
    }
    case 351: { // i64_mem: Sig_stk_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 352: { // i64_reg0: Sig_stk_to_i64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 353: { // i64_mem: Sig_ref_to_i64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 354: { // i64_reg0: Sig_ref_to_i64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 355: { // i64_mem: Sig_v128_to_i64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 356: { // i64_reg0: Sig_v128_to_i64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999991u);
        break;
    }
    case 357: { // i64_mem: Sig_v128_to_i64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 358: { // i64_reg0: Sig_v128_to_i64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999991u);
        break;
    }
    case 359: { // f32_mem: Sig_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 360: { // f32_reg0: Sig_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 361: { // f32_mem: Sig_f32_to_f32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 362: { // f32_reg0: Sig_f32_to_f32(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 363: { // f32_mem: Sig_f32_to_f32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 364: { // f32_reg0: Sig_f32_to_f32(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999992u);
        break;
    }
    case 365: { // f32_mem: Sig_i32_to_f32(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 366: { // f32_reg0: Sig_i32_to_f32(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 367: { // f32_mem: Sig_i32_to_f32(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 368: { // f32_reg0: Sig_i32_to_f32(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 369: { // f32_mem: Sig_i64_to_f32(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 370: { // f32_reg0: Sig_i64_to_f32(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 371: { // f32_mem: Sig_i64_to_f32(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 372: { // f32_reg0: Sig_i64_to_f32(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999992u);
        break;
    }
    case 373: { // f32_mem: Sig_i32_i32_to_f32(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 374: { // f32_reg0: Sig_i32_i32_to_f32(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 375: { // f32_mem: Sig_i32_i32_to_f32(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 376: { // f32_reg0: Sig_i32_i32_to_f32(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 377: { // f32_mem: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 378: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999992u);
        break;
    }
    case 379: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999900u, 0x99999992u);
        break;
    }
    case 380: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999900u, 0x99999992u);
        break;
    }
    case 381: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999900u, 0x99999992u);
        break;
    }
    case 382: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999900u, 0x99999992u);
        break;
    }
    case 383: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999900u, 0x99999992u);
        break;
    }
    case 384: { // f32_reg0: Sig_i32_i32_to_f32(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999900u, 0x99999992u);
        break;
    }
    case 385: { // f32_mem: Sig_i64_i32_to_f32(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 386: { // f32_reg0: Sig_i64_i32_to_f32(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 387: { // f32_mem: Sig_i64_i32_to_f32(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 388: { // f32_reg0: Sig_i64_i32_to_f32(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 389: { // f32_mem: Sig_f32_i32_to_f32(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 390: { // f32_reg0: Sig_f32_i32_to_f32(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 391: { // f32_mem: Sig_f32_i32_to_f32(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 392: { // f32_reg0: Sig_f32_i32_to_f32(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 393: { // f32_mem: Sig_f64_i32_to_f32(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 394: { // f32_reg0: Sig_f64_i32_to_f32(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 395: { // f32_mem: Sig_f64_i32_to_f32(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 396: { // f32_reg0: Sig_f64_i32_to_f32(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 397: { // f32_mem: Sig_v128_i32_to_f32_aw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 398: { // f32_reg0: Sig_v128_i32_to_f32_aw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 399: { // f32_mem: Sig_v128_i32_to_f32_aw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 400: { // f32_reg0: Sig_v128_i32_to_f32_aw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 401: { // f32_mem: Sig_ref_i32_to_f32(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 402: { // f32_reg0: Sig_ref_i32_to_f32(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 403: { // f32_mem: Sig_ref_i32_to_f32(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 404: { // f32_reg0: Sig_ref_i32_to_f32(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 405: { // f32_mem: Sig_i32_i64_to_f32(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 406: { // f32_reg0: Sig_i32_i64_to_f32(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 407: { // f32_mem: Sig_i32_i64_to_f32(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 408: { // f32_reg0: Sig_i32_i64_to_f32(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999992u);
        break;
    }
    case 409: { // f32_mem: Sig_i64_i64_to_f32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 410: { // f32_reg0: Sig_i64_i64_to_f32(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 411: { // f32_mem: Sig_i64_i64_to_f32(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 412: { // f32_reg0: Sig_i64_i64_to_f32(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999992u);
        break;
    }
    case 413: { // f32_mem: Sig_f32_i64_to_f32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 414: { // f32_reg0: Sig_f32_i64_to_f32(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 415: { // f32_mem: Sig_f32_i64_to_f32(f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 416: { // f32_reg0: Sig_f32_i64_to_f32(f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999992u);
        break;
    }
    case 417: { // f32_mem: Sig_f64_i64_to_f32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 418: { // f32_reg0: Sig_f64_i64_to_f32(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 419: { // f32_mem: Sig_f64_i64_to_f32(f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 420: { // f32_reg0: Sig_f64_i64_to_f32(f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999992u);
        break;
    }
    case 421: { // f32_mem: Sig_v128_i64_to_f32_aw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 422: { // f32_reg0: Sig_v128_i64_to_f32_aw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 423: { // f32_mem: Sig_v128_i64_to_f32_aw(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 424: { // f32_reg0: Sig_v128_i64_to_f32_aw(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999992u);
        break;
    }
    case 425: { // f32_mem: Sig_ref_i64_to_f32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 426: { // f32_reg0: Sig_ref_i64_to_f32(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 427: { // f32_mem: Sig_ref_i64_to_f32(ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 428: { // f32_reg0: Sig_ref_i64_to_f32(ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999992u);
        break;
    }
    case 429: { // f32_mem: Sig_f32_f32_to_f32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 430: { // f32_reg0: Sig_f32_f32_to_f32(f32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 431: { // f32_mem: Sig_f32_f32_to_f32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 432: { // f32_reg0: Sig_f32_f32_to_f32(f32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999992u);
        break;
    }
    case 433: { // f32_mem: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999922u, 0x99999999u);
        break;
    }
    case 434: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999922u, 0x99999992u);
        break;
    }
    case 435: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999922u, 0x99999992u);
        break;
    }
    case 436: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999922u, 0x99999992u);
        break;
    }
    case 437: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999922u, 0x99999992u);
        break;
    }
    case 438: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999922u, 0x99999992u);
        break;
    }
    case 439: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999922u, 0x99999992u);
        break;
    }
    case 440: { // f32_reg0: Sig_f32_f32_to_f32(f32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999922u, 0x99999992u);
        break;
    }
    case 441: { // f32_mem: Sig_f64_to_f32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 442: { // f32_reg0: Sig_f64_to_f32(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 443: { // f32_mem: Sig_f64_to_f32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 444: { // f32_reg0: Sig_f64_to_f32(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999992u);
        break;
    }
    case 445: { // f32_mem: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 446: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 447: { // f32_mem: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 448: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_mem, f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999992u);
        break;
    }
    case 449: { // f32_mem: Sig_f32_f32_i32_to_f32(f32_mem, f32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999920u, 0x99999999u);
        break;
    }
    case 450: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_mem, f32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999920u, 0x99999992u);
        break;
    }
    case 451: { // f32_mem: Sig_f32_f32_i32_to_f32(f32_reg2, f32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 26, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999220u, 0x99999999u);
        break;
    }
    case 452: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_reg2, f32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 26, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999220u, 0x99999992u);
        break;
    }
    case 453: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_reg2, f32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 26, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999220u, 0x99999992u);
        break;
    }
    case 454: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_reg2, f32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 26, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999220u, 0x99999992u);
        break;
    }
    case 455: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_reg2, f32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 26, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999220u, 0x99999992u);
        break;
    }
    case 456: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_reg2, f32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 26, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999220u, 0x99999992u);
        break;
    }
    case 457: { // f32_reg0: Sig_f32_f32_i32_to_f32(f32_reg2, f32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 26, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 25, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999220u, 0x99999992u);
        break;
    }
    case 458: { // f32_mem: Sig_stk_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 459: { // f32_reg0: Sig_stk_to_f32
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 460: { // f32_mem: Sig_ref_to_f32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 461: { // f32_reg0: Sig_ref_to_f32(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 462: { // f32_mem: Sig_v128_to_f32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 463: { // f32_reg0: Sig_v128_to_f32(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999992u);
        break;
    }
    case 464: { // f32_mem: Sig_v128_to_f32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 465: { // f32_reg0: Sig_v128_to_f32(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999992u);
        break;
    }
    case 466: { // f64_mem: Sig_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 467: { // f64_reg0: Sig_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 468: { // f64_mem: Sig_f64_to_f64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 469: { // f64_reg0: Sig_f64_to_f64(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 470: { // f64_mem: Sig_f64_to_f64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 471: { // f64_reg0: Sig_f64_to_f64(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999993u);
        break;
    }
    case 472: { // f64_mem: Sig_i32_to_f64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 473: { // f64_reg0: Sig_i32_to_f64(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 474: { // f64_mem: Sig_i32_to_f64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 475: { // f64_reg0: Sig_i32_to_f64(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 476: { // f64_mem: Sig_i64_to_f64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 477: { // f64_reg0: Sig_i64_to_f64(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 478: { // f64_mem: Sig_i64_to_f64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 479: { // f64_reg0: Sig_i64_to_f64(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999993u);
        break;
    }
    case 480: { // f64_mem: Sig_i32_i32_to_f64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 481: { // f64_reg0: Sig_i32_i32_to_f64(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 482: { // f64_mem: Sig_i32_i32_to_f64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 483: { // f64_reg0: Sig_i32_i32_to_f64(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 484: { // f64_mem: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 485: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999993u);
        break;
    }
    case 486: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999900u, 0x99999993u);
        break;
    }
    case 487: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999900u, 0x99999993u);
        break;
    }
    case 488: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999900u, 0x99999993u);
        break;
    }
    case 489: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999900u, 0x99999993u);
        break;
    }
    case 490: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999900u, 0x99999993u);
        break;
    }
    case 491: { // f64_reg0: Sig_i32_i32_to_f64(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999900u, 0x99999993u);
        break;
    }
    case 492: { // f64_mem: Sig_i64_i32_to_f64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 493: { // f64_reg0: Sig_i64_i32_to_f64(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 494: { // f64_mem: Sig_i64_i32_to_f64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 495: { // f64_reg0: Sig_i64_i32_to_f64(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 496: { // f64_mem: Sig_f32_i32_to_f64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 497: { // f64_reg0: Sig_f32_i32_to_f64(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 498: { // f64_mem: Sig_f32_i32_to_f64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 499: { // f64_reg0: Sig_f32_i32_to_f64(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 500: { // f64_mem: Sig_f64_i32_to_f64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 501: { // f64_reg0: Sig_f64_i32_to_f64(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 502: { // f64_mem: Sig_f64_i32_to_f64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 503: { // f64_reg0: Sig_f64_i32_to_f64(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 504: { // f64_mem: Sig_v128_i32_to_f64_aw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 505: { // f64_reg0: Sig_v128_i32_to_f64_aw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 506: { // f64_mem: Sig_v128_i32_to_f64_aw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 507: { // f64_reg0: Sig_v128_i32_to_f64_aw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 508: { // f64_mem: Sig_ref_i32_to_f64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 509: { // f64_reg0: Sig_ref_i32_to_f64(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 510: { // f64_mem: Sig_ref_i32_to_f64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 511: { // f64_reg0: Sig_ref_i32_to_f64(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 512: { // f64_mem: Sig_i32_i64_to_f64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 513: { // f64_reg0: Sig_i32_i64_to_f64(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 514: { // f64_mem: Sig_i32_i64_to_f64(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 515: { // f64_reg0: Sig_i32_i64_to_f64(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999993u);
        break;
    }
    case 516: { // f64_mem: Sig_i64_i64_to_f64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 517: { // f64_reg0: Sig_i64_i64_to_f64(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 518: { // f64_mem: Sig_i64_i64_to_f64(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 519: { // f64_reg0: Sig_i64_i64_to_f64(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999993u);
        break;
    }
    case 520: { // f64_mem: Sig_f32_i64_to_f64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 521: { // f64_reg0: Sig_f32_i64_to_f64(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 522: { // f64_mem: Sig_f32_i64_to_f64(f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 523: { // f64_reg0: Sig_f32_i64_to_f64(f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999993u);
        break;
    }
    case 524: { // f64_mem: Sig_f64_i64_to_f64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 525: { // f64_reg0: Sig_f64_i64_to_f64(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 526: { // f64_mem: Sig_f64_i64_to_f64(f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 527: { // f64_reg0: Sig_f64_i64_to_f64(f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999993u);
        break;
    }
    case 528: { // f64_mem: Sig_v128_i64_to_f64_aw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 529: { // f64_reg0: Sig_v128_i64_to_f64_aw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 530: { // f64_mem: Sig_v128_i64_to_f64_aw(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 531: { // f64_reg0: Sig_v128_i64_to_f64_aw(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999993u);
        break;
    }
    case 532: { // f64_mem: Sig_ref_i64_to_f64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 533: { // f64_reg0: Sig_ref_i64_to_f64(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 534: { // f64_mem: Sig_ref_i64_to_f64(ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 535: { // f64_reg0: Sig_ref_i64_to_f64(ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999993u);
        break;
    }
    case 536: { // f64_mem: Sig_f64_f64_to_f64(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 537: { // f64_reg0: Sig_f64_f64_to_f64(f64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 538: { // f64_mem: Sig_f64_f64_to_f64(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 539: { // f64_reg0: Sig_f64_f64_to_f64(f64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999993u);
        break;
    }
    case 540: { // f64_mem: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999933u, 0x99999999u);
        break;
    }
    case 541: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999933u, 0x99999993u);
        break;
    }
    case 542: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999933u, 0x99999993u);
        break;
    }
    case 543: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999933u, 0x99999993u);
        break;
    }
    case 544: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999933u, 0x99999993u);
        break;
    }
    case 545: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999933u, 0x99999993u);
        break;
    }
    case 546: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999933u, 0x99999993u);
        break;
    }
    case 547: { // f64_reg0: Sig_f64_f64_to_f64(f64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999933u, 0x99999993u);
        break;
    }
    case 548: { // f64_mem: Sig_f32_to_f64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 549: { // f64_reg0: Sig_f32_to_f64(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 550: { // f64_mem: Sig_f32_to_f64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 551: { // f64_reg0: Sig_f32_to_f64(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999993u);
        break;
    }
    case 552: { // f64_mem: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 553: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 554: { // f64_mem: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 555: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_mem, f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999993u);
        break;
    }
    case 556: { // f64_mem: Sig_f64_f64_i32_to_f64(f64_mem, f64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999930u, 0x99999999u);
        break;
    }
    case 557: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_mem, f64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999930u, 0x99999993u);
        break;
    }
    case 558: { // f64_mem: Sig_f64_f64_i32_to_f64(f64_reg2, f64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 34, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999330u, 0x99999999u);
        break;
    }
    case 559: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_reg2, f64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 34, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999330u, 0x99999993u);
        break;
    }
    case 560: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_reg2, f64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 34, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999330u, 0x99999993u);
        break;
    }
    case 561: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_reg2, f64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 34, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999330u, 0x99999993u);
        break;
    }
    case 562: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_reg2, f64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 34, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999330u, 0x99999993u);
        break;
    }
    case 563: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_reg2, f64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 34, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999330u, 0x99999993u);
        break;
    }
    case 564: { // f64_reg0: Sig_f64_f64_i32_to_f64(f64_reg2, f64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 34, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 33, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999330u, 0x99999993u);
        break;
    }
    case 565: { // f64_mem: Sig_stk_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 566: { // f64_reg0: Sig_stk_to_f64
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 567: { // f64_mem: Sig_ref_to_f64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 568: { // f64_reg0: Sig_ref_to_f64(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 569: { // f64_mem: Sig_v128_to_f64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 570: { // f64_reg0: Sig_v128_to_f64(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999993u);
        break;
    }
    case 571: { // f64_mem: Sig_v128_to_f64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 572: { // f64_reg0: Sig_v128_to_f64(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999993u);
        break;
    }
    case 573: { // v128_mem: Sig_to_v128_pw
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 574: { // v128_reg0: Sig_to_v128_pw
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 575: { // v128_mem: Sig_v128_to_v128_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 576: { // v128_reg0: Sig_v128_to_v128_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 577: { // v128_mem: Sig_v128_to_v128_pw(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 578: { // v128_reg0: Sig_v128_to_v128_pw(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 579: { // v128_mem: Sig_i32_to_v128_pw(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 580: { // v128_reg0: Sig_i32_to_v128_pw(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 581: { // v128_mem: Sig_i32_to_v128_pw(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 582: { // v128_reg0: Sig_i32_to_v128_pw(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 583: { // v128_mem: Sig_i64_to_v128_pw(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 584: { // v128_reg0: Sig_i64_to_v128_pw(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 585: { // v128_mem: Sig_i64_to_v128_pw(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 586: { // v128_reg0: Sig_i64_to_v128_pw(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 587: { // v128_mem: Sig_i32_i32_to_v128_pw(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 588: { // v128_reg0: Sig_i32_i32_to_v128_pw(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 589: { // v128_mem: Sig_i32_i32_to_v128_pw(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 590: { // v128_reg0: Sig_i32_i32_to_v128_pw(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 591: { // v128_mem: Sig_i64_i32_to_v128_pw(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 592: { // v128_reg0: Sig_i64_i32_to_v128_pw(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 593: { // v128_mem: Sig_i64_i32_to_v128_pw(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 594: { // v128_reg0: Sig_i64_i32_to_v128_pw(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 595: { // v128_mem: Sig_f32_i32_to_v128_pw(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 596: { // v128_reg0: Sig_f32_i32_to_v128_pw(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 597: { // v128_mem: Sig_f32_i32_to_v128_pw(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 598: { // v128_reg0: Sig_f32_i32_to_v128_pw(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 599: { // v128_mem: Sig_f64_i32_to_v128_pw(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 600: { // v128_reg0: Sig_f64_i32_to_v128_pw(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 601: { // v128_mem: Sig_f64_i32_to_v128_pw(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 602: { // v128_reg0: Sig_f64_i32_to_v128_pw(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 603: { // v128_mem: Sig_v128_i32_to_v128_pw_aw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 604: { // v128_mem: Sig_v128_i32_to_v128_pw_aw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 605: { // v128_mem: Sig_ref_i32_to_v128_pw(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 606: { // v128_reg0: Sig_ref_i32_to_v128_pw(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 607: { // v128_mem: Sig_ref_i32_to_v128_pw(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 608: { // v128_reg0: Sig_ref_i32_to_v128_pw(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 609: { // v128_mem: Sig_i32_i64_to_v128_pw(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 610: { // v128_reg0: Sig_i32_i64_to_v128_pw(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 611: { // v128_mem: Sig_i32_i64_to_v128_pw(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 612: { // v128_reg0: Sig_i32_i64_to_v128_pw(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 613: { // v128_mem: Sig_i64_i64_to_v128_pw(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 614: { // v128_reg0: Sig_i64_i64_to_v128_pw(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 615: { // v128_mem: Sig_i64_i64_to_v128_pw(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 616: { // v128_reg0: Sig_i64_i64_to_v128_pw(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 617: { // v128_mem: Sig_f32_i64_to_v128_pw(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 618: { // v128_reg0: Sig_f32_i64_to_v128_pw(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 619: { // v128_mem: Sig_f32_i64_to_v128_pw(f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 620: { // v128_reg0: Sig_f32_i64_to_v128_pw(f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 621: { // v128_mem: Sig_f64_i64_to_v128_pw(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 622: { // v128_reg0: Sig_f64_i64_to_v128_pw(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 623: { // v128_mem: Sig_f64_i64_to_v128_pw(f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 624: { // v128_reg0: Sig_f64_i64_to_v128_pw(f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 625: { // v128_mem: Sig_v128_i64_to_v128_pw_aw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 626: { // v128_mem: Sig_v128_i64_to_v128_pw_aw(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 627: { // v128_mem: Sig_ref_i64_to_v128_pw(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 628: { // v128_reg0: Sig_ref_i64_to_v128_pw(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 629: { // v128_mem: Sig_ref_i64_to_v128_pw(ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 630: { // v128_reg0: Sig_ref_i64_to_v128_pw(ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 631: { // v128_mem: Sig_v128_v128_i32_to_v128_pw(v128_mem, v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 632: { // v128_reg0: Sig_v128_v128_i32_to_v128_pw(v128_mem, v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 633: { // v128_mem: Sig_v128_v128_i32_to_v128_pw(v128_mem, v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 634: { // v128_reg0: Sig_v128_v128_i32_to_v128_pw(v128_mem, v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 635: { // v128_mem: Sig_v128_v128_i32_to_v128_pw(v128_mem, v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999440u, 0x99999999u);
        break;
    }
    case 636: { // v128_reg0: Sig_v128_v128_i32_to_v128_pw(v128_mem, v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999440u, 0x99999944u);
        break;
    }
    case 637: { // v128_mem: Sig_v128_v128_i32_to_v128_pw(v128_reg3, v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 43, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99944440u, 0x99999999u);
        break;
    }
    case 638: { // v128_reg0: Sig_v128_v128_i32_to_v128_pw(v128_reg3, v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 43, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99944440u, 0x99999944u);
        break;
    }
    case 639: { // v128_reg0: Sig_v128_v128_i32_to_v128_pw(v128_reg3, v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 43, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99944440u, 0x99999944u);
        break;
    }
    case 640: { // v128_reg0: Sig_v128_v128_i32_to_v128_pw(v128_reg3, v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 43, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99944440u, 0x99999944u);
        break;
    }
    case 641: { // v128_reg0: Sig_v128_v128_i32_to_v128_pw(v128_reg3, v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 43, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99944440u, 0x99999944u);
        break;
    }
    case 642: { // v128_mem: Sig_v128_to_v128_aw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 643: { // v128_mem: Sig_stk_to_v128_pw
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 644: { // v128_reg0: Sig_stk_to_v128_pw
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 645: { // v128_mem: Sig_ref_to_v128_aw(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 646: { // v128_mem: Sig_ref_i32_to_v128_aw(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 647: { // v128_mem: Sig_ref_i32_to_v128_aw(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 648: { // v128_mem: Sig_to_v128_aw
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 649: { // v128_mem: Sig_i32_to_v128_aw(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 650: { // v128_mem: Sig_i32_to_v128_aw(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 651: { // v128_mem: Sig_i32_i32_to_v128_aw(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 652: { // v128_mem: Sig_i32_i32_to_v128_aw(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 653: { // v128_mem: Sig_i32_i32_to_v128_aw(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 654: { // v128_mem: Sig_i64_to_v128_aw(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 655: { // v128_mem: Sig_i64_to_v128_aw(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 656: { // v128_mem: Sig_i32_to_v128(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 657: { // v128_reg0: Sig_i32_to_v128(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 658: { // v128_mem: Sig_i32_to_v128(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 659: { // v128_reg0: Sig_i32_to_v128(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 660: { // v128_mem: Sig_i64_to_v128(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 661: { // v128_reg0: Sig_i64_to_v128(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 662: { // v128_mem: Sig_i64_to_v128(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 663: { // v128_reg0: Sig_i64_to_v128(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 664: { // v128_mem: Sig_to_v128
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 665: { // v128_reg0: Sig_to_v128
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 666: { // v128_mem: Sig_v128_v128_to_v128(v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 667: { // v128_reg0: Sig_v128_v128_to_v128(v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 668: { // v128_mem: Sig_v128_v128_to_v128(v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 669: { // v128_reg0: Sig_v128_v128_to_v128(v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 670: { // v128_mem: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99994444u, 0x99999999u);
        break;
    }
    case 671: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99994444u, 0x99999944u);
        break;
    }
    case 672: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99994444u, 0x99999944u);
        break;
    }
    case 673: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99994444u, 0x99999944u);
        break;
    }
    case 674: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99994444u, 0x99999944u);
        break;
    }
    case 675: { // v128_reg0: Sig_v128_v128_to_v128(v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99994444u, 0x99999944u);
        break;
    }
    case 676: { // v128_mem: Sig_i32_v128_to_v128(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 677: { // v128_reg0: Sig_i32_v128_to_v128(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 678: { // v128_mem: Sig_i32_v128_to_v128(i32_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 679: { // v128_reg0: Sig_i32_v128_to_v128(i32_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 680: { // v128_mem: Sig_i32_v128_to_v128(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999044u, 0x99999999u);
        break;
    }
    case 681: { // v128_reg0: Sig_i32_v128_to_v128(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999044u, 0x99999944u);
        break;
    }
    case 682: { // v128_reg0: Sig_i32_v128_to_v128(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999044u, 0x99999944u);
        break;
    }
    case 683: { // v128_reg0: Sig_i32_v128_to_v128(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999044u, 0x99999944u);
        break;
    }
    case 684: { // v128_reg0: Sig_i32_v128_to_v128(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999044u, 0x99999944u);
        break;
    }
    case 685: { // v128_reg0: Sig_i32_v128_to_v128(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999044u, 0x99999944u);
        break;
    }
    case 686: { // v128_reg0: Sig_i32_v128_to_v128(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999044u, 0x99999944u);
        break;
    }
    case 687: { // v128_mem: Sig_i64_v128_to_v128(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 688: { // v128_reg0: Sig_i64_v128_to_v128(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 689: { // v128_mem: Sig_i64_v128_to_v128(i64_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 690: { // v128_reg0: Sig_i64_v128_to_v128(i64_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 691: { // v128_mem: Sig_i64_v128_to_v128(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999144u, 0x99999999u);
        break;
    }
    case 692: { // v128_reg0: Sig_i64_v128_to_v128(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999144u, 0x99999944u);
        break;
    }
    case 693: { // v128_reg0: Sig_i64_v128_to_v128(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999144u, 0x99999944u);
        break;
    }
    case 694: { // v128_reg0: Sig_i64_v128_to_v128(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999144u, 0x99999944u);
        break;
    }
    case 695: { // v128_reg0: Sig_i64_v128_to_v128(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999144u, 0x99999944u);
        break;
    }
    case 696: { // v128_reg0: Sig_i64_v128_to_v128(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999144u, 0x99999944u);
        break;
    }
    case 697: { // v128_reg0: Sig_i64_v128_to_v128(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999144u, 0x99999944u);
        break;
    }
    case 698: { // v128_mem: Sig_f32_to_v128(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 699: { // v128_reg0: Sig_f32_to_v128(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 700: { // v128_mem: Sig_f32_to_v128(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 701: { // v128_reg0: Sig_f32_to_v128(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999944u);
        break;
    }
    case 702: { // v128_mem: Sig_f64_to_v128(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 703: { // v128_reg0: Sig_f64_to_v128(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 704: { // v128_mem: Sig_f64_to_v128(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 705: { // v128_reg0: Sig_f64_to_v128(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999944u);
        break;
    }
    case 706: { // v128_mem: Sig_v128_to_v128(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 707: { // v128_reg0: Sig_v128_to_v128(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 708: { // v128_mem: Sig_v128_to_v128(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 709: { // v128_reg0: Sig_v128_to_v128(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 710: { // v128_mem: Sig_v128_i32_to_v128(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 711: { // v128_reg0: Sig_v128_i32_to_v128(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 712: { // v128_mem: Sig_v128_i32_to_v128(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 713: { // v128_reg0: Sig_v128_i32_to_v128(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999944u);
        break;
    }
    case 714: { // v128_mem: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999440u, 0x99999999u);
        break;
    }
    case 715: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999440u, 0x99999944u);
        break;
    }
    case 716: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999440u, 0x99999944u);
        break;
    }
    case 717: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999440u, 0x99999944u);
        break;
    }
    case 718: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999440u, 0x99999944u);
        break;
    }
    case 719: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999440u, 0x99999944u);
        break;
    }
    case 720: { // v128_reg0: Sig_v128_i32_to_v128(v128_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999440u, 0x99999944u);
        break;
    }
    case 721: { // v128_mem: Sig_v128_i64_to_v128(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 722: { // v128_reg0: Sig_v128_i64_to_v128(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 723: { // v128_mem: Sig_v128_i64_to_v128(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 724: { // v128_reg0: Sig_v128_i64_to_v128(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999944u);
        break;
    }
    case 725: { // v128_mem: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999441u, 0x99999999u);
        break;
    }
    case 726: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999441u, 0x99999944u);
        break;
    }
    case 727: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999441u, 0x99999944u);
        break;
    }
    case 728: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999441u, 0x99999944u);
        break;
    }
    case 729: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999441u, 0x99999944u);
        break;
    }
    case 730: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999441u, 0x99999944u);
        break;
    }
    case 731: { // v128_reg0: Sig_v128_i64_to_v128(v128_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999441u, 0x99999944u);
        break;
    }
    case 732: { // v128_mem: Sig_v128_f32_to_v128(v128_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 733: { // v128_reg0: Sig_v128_f32_to_v128(v128_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 734: { // v128_mem: Sig_v128_f32_to_v128(v128_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 735: { // v128_reg0: Sig_v128_f32_to_v128(v128_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999944u);
        break;
    }
    case 736: { // v128_mem: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999442u, 0x99999999u);
        break;
    }
    case 737: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999442u, 0x99999944u);
        break;
    }
    case 738: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999442u, 0x99999944u);
        break;
    }
    case 739: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999442u, 0x99999944u);
        break;
    }
    case 740: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999442u, 0x99999944u);
        break;
    }
    case 741: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999442u, 0x99999944u);
        break;
    }
    case 742: { // v128_reg0: Sig_v128_f32_to_v128(v128_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999442u, 0x99999944u);
        break;
    }
    case 743: { // v128_mem: Sig_v128_f64_to_v128(v128_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 744: { // v128_reg0: Sig_v128_f64_to_v128(v128_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 745: { // v128_mem: Sig_v128_f64_to_v128(v128_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 746: { // v128_reg0: Sig_v128_f64_to_v128(v128_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999944u);
        break;
    }
    case 747: { // v128_mem: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999443u, 0x99999999u);
        break;
    }
    case 748: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999443u, 0x99999944u);
        break;
    }
    case 749: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999443u, 0x99999944u);
        break;
    }
    case 750: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999443u, 0x99999944u);
        break;
    }
    case 751: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999443u, 0x99999944u);
        break;
    }
    case 752: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999443u, 0x99999944u);
        break;
    }
    case 753: { // v128_reg0: Sig_v128_f64_to_v128(v128_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 41, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999443u, 0x99999944u);
        break;
    }
    case 754: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 755: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999944u);
        break;
    }
    case 756: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 757: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_mem, v128_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999944u);
        break;
    }
    case 758: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_mem, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99994444u, 0x99999999u);
        break;
    }
    case 759: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_mem, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99994444u, 0x99999944u);
        break;
    }
    case 760: { // v128_mem: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99444444u, 0x99999999u);
        break;
    }
    case 761: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99444444u, 0x99999944u);
        break;
    }
    case 762: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99444444u, 0x99999944u);
        break;
    }
    case 763: { // v128_reg0: Sig_v128_v128_v128_to_v128(v128_reg4, v128_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 44, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 42, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 40, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99444444u, 0x99999944u);
        break;
    }
    case 764: { // ref_mem: Sig_to_ref
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 765: { // ref_mem: Sig_ref_to_ref(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 766: { // ref_mem: Sig_i32_to_ref(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 767: { // ref_mem: Sig_i32_to_ref(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 768: { // ref_mem: Sig_i64_to_ref(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 769: { // ref_mem: Sig_i64_to_ref(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 770: { // ref_mem: Sig_i32_i32_to_ref(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 771: { // ref_mem: Sig_i32_i32_to_ref(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 772: { // ref_mem: Sig_i32_i32_to_ref(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 773: { // ref_mem: Sig_i64_i32_to_ref(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 774: { // ref_mem: Sig_i64_i32_to_ref(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 775: { // ref_mem: Sig_f32_i32_to_ref(f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 776: { // ref_mem: Sig_f32_i32_to_ref(f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 777: { // ref_mem: Sig_f64_i32_to_ref(f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 778: { // ref_mem: Sig_f64_i32_to_ref(f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 779: { // ref_mem: Sig_v128_i32_to_ref_aw(v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 780: { // ref_mem: Sig_v128_i32_to_ref_aw(v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 781: { // ref_mem: Sig_ref_i32_to_ref(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 782: { // ref_mem: Sig_ref_i32_to_ref(ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 783: { // ref_mem: Sig_i32_i64_to_ref(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 784: { // ref_mem: Sig_i32_i64_to_ref(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 785: { // ref_mem: Sig_i64_i64_to_ref(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 786: { // ref_mem: Sig_i64_i64_to_ref(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 787: { // ref_mem: Sig_f32_i64_to_ref(f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 788: { // ref_mem: Sig_f32_i64_to_ref(f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 789: { // ref_mem: Sig_f64_i64_to_ref(f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 790: { // ref_mem: Sig_f64_i64_to_ref(f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 791: { // ref_mem: Sig_v128_i64_to_ref_aw(v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 792: { // ref_mem: Sig_v128_i64_to_ref_aw(v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 793: { // ref_mem: Sig_ref_i64_to_ref(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 794: { // ref_mem: Sig_ref_i64_to_ref(ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 795: { // ref_mem: Sig_ref_ref_i32_to_ref(ref_mem, ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 796: { // ref_mem: Sig_ref_ref_i32_to_ref(ref_mem, ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 797: { // ref_mem: Sig_stk_to_ref
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 798: { // stmt: Sig_i32_to_void(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 799: { // stmt: Sig_i32_to_void(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 800: { // stmt: Sig_i64_to_void(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 801: { // stmt: Sig_i64_to_void(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 802: { // stmt: Sig_f32_to_void(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 803: { // stmt: Sig_f32_to_void(f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 24, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 804: { // stmt: Sig_f64_to_void(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 805: { // stmt: Sig_f64_to_void(f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 32, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 806: { // stmt: Sig_v128_to_void_pw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 807: { // stmt: Sig_v128_to_void_pw(v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 40, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 808: { // stmt: Sig_ref_to_void(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 809: { // stmt: Sig_i32_i32_to_void(i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 810: { // stmt: Sig_i32_i32_to_void(i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 811: { // stmt: Sig_i32_i32_to_void(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 812: { // stmt: Sig_i32_i32_to_void(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999900u, 0x99999999u);
        break;
    }
    case 813: { // stmt: Sig_i32_i32_to_void(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999900u, 0x99999999u);
        break;
    }
    case 814: { // stmt: Sig_i32_i32_to_void(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999900u, 0x99999999u);
        break;
    }
    case 815: { // stmt: Sig_i32_i32_to_void(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999900u, 0x99999999u);
        break;
    }
    case 816: { // stmt: Sig_i32_i32_to_void(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999900u, 0x99999999u);
        break;
    }
    case 817: { // stmt: Sig_i32_i32_to_void(i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999900u, 0x99999999u);
        break;
    }
    case 818: { // stmt: Sig_i64_i32_to_void(i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 819: { // stmt: Sig_i64_i32_to_void(i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 820: { // stmt: Sig_i64_i32_to_void(i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999910u, 0x99999999u);
        break;
    }
    case 821: { // stmt: Sig_i64_i32_to_void(i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999910u, 0x99999999u);
        break;
    }
    case 822: { // stmt: Sig_i64_i32_to_void(i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999910u, 0x99999999u);
        break;
    }
    case 823: { // stmt: Sig_i64_i32_to_void(i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999910u, 0x99999999u);
        break;
    }
    case 824: { // stmt: Sig_i64_i32_to_void(i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999910u, 0x99999999u);
        break;
    }
    case 825: { // stmt: Sig_i64_i32_to_void(i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999910u, 0x99999999u);
        break;
    }
    case 826: { // stmt: Sig_i64_i32_to_void(i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 8, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999910u, 0x99999999u);
        break;
    }
    case 827: { // stmt: Sig_i32_i64_to_void(i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 828: { // stmt: Sig_i32_i64_to_void(i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 829: { // stmt: Sig_i32_i64_to_void(i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999901u, 0x99999999u);
        break;
    }
    case 830: { // stmt: Sig_i32_i64_to_void(i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999901u, 0x99999999u);
        break;
    }
    case 831: { // stmt: Sig_i32_i64_to_void(i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999901u, 0x99999999u);
        break;
    }
    case 832: { // stmt: Sig_i32_i64_to_void(i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999901u, 0x99999999u);
        break;
    }
    case 833: { // stmt: Sig_i32_i64_to_void(i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999901u, 0x99999999u);
        break;
    }
    case 834: { // stmt: Sig_i32_i64_to_void(i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999901u, 0x99999999u);
        break;
    }
    case 835: { // stmt: Sig_i32_i64_to_void(i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999901u, 0x99999999u);
        break;
    }
    case 836: { // stmt: Sig_i64_i64_to_void(i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 837: { // stmt: Sig_i64_i64_to_void(i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 838: { // stmt: Sig_i64_i64_to_void(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999911u, 0x99999999u);
        break;
    }
    case 839: { // stmt: Sig_i64_i64_to_void(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999911u, 0x99999999u);
        break;
    }
    case 840: { // stmt: Sig_i64_i64_to_void(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999911u, 0x99999999u);
        break;
    }
    case 841: { // stmt: Sig_i64_i64_to_void(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999911u, 0x99999999u);
        break;
    }
    case 842: { // stmt: Sig_i64_i64_to_void(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999911u, 0x99999999u);
        break;
    }
    case 843: { // stmt: Sig_i64_i64_to_void(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999911u, 0x99999999u);
        break;
    }
    case 844: { // stmt: Sig_i64_i64_to_void(i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 16, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999911u, 0x99999999u);
        break;
    }
    case 845: { // stmt: Sig_i32_f32_to_void(i32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 846: { // stmt: Sig_i32_f32_to_void(i32_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 847: { // stmt: Sig_i32_f32_to_void(i32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999902u, 0x99999999u);
        break;
    }
    case 848: { // stmt: Sig_i32_f32_to_void(i32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999902u, 0x99999999u);
        break;
    }
    case 849: { // stmt: Sig_i32_f32_to_void(i32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999902u, 0x99999999u);
        break;
    }
    case 850: { // stmt: Sig_i32_f32_to_void(i32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999902u, 0x99999999u);
        break;
    }
    case 851: { // stmt: Sig_i32_f32_to_void(i32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999902u, 0x99999999u);
        break;
    }
    case 852: { // stmt: Sig_i32_f32_to_void(i32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999902u, 0x99999999u);
        break;
    }
    case 853: { // stmt: Sig_i32_f32_to_void(i32_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999902u, 0x99999999u);
        break;
    }
    case 854: { // stmt: Sig_i64_f32_to_void(i64_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 855: { // stmt: Sig_i64_f32_to_void(i64_mem, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999992u, 0x99999999u);
        break;
    }
    case 856: { // stmt: Sig_i64_f32_to_void(i64_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999912u, 0x99999999u);
        break;
    }
    case 857: { // stmt: Sig_i64_f32_to_void(i64_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999912u, 0x99999999u);
        break;
    }
    case 858: { // stmt: Sig_i64_f32_to_void(i64_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999912u, 0x99999999u);
        break;
    }
    case 859: { // stmt: Sig_i64_f32_to_void(i64_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999912u, 0x99999999u);
        break;
    }
    case 860: { // stmt: Sig_i64_f32_to_void(i64_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999912u, 0x99999999u);
        break;
    }
    case 861: { // stmt: Sig_i64_f32_to_void(i64_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999912u, 0x99999999u);
        break;
    }
    case 862: { // stmt: Sig_i64_f32_to_void(i64_reg1, f32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 24, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999912u, 0x99999999u);
        break;
    }
    case 863: { // stmt: Sig_i32_f64_to_void(i32_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 864: { // stmt: Sig_i32_f64_to_void(i32_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 865: { // stmt: Sig_i32_f64_to_void(i32_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999903u, 0x99999999u);
        break;
    }
    case 866: { // stmt: Sig_i32_f64_to_void(i32_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999903u, 0x99999999u);
        break;
    }
    case 867: { // stmt: Sig_i32_f64_to_void(i32_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999903u, 0x99999999u);
        break;
    }
    case 868: { // stmt: Sig_i32_f64_to_void(i32_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999903u, 0x99999999u);
        break;
    }
    case 869: { // stmt: Sig_i32_f64_to_void(i32_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999903u, 0x99999999u);
        break;
    }
    case 870: { // stmt: Sig_i32_f64_to_void(i32_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999903u, 0x99999999u);
        break;
    }
    case 871: { // stmt: Sig_i32_f64_to_void(i32_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999903u, 0x99999999u);
        break;
    }
    case 872: { // stmt: Sig_i64_f64_to_void(i64_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 873: { // stmt: Sig_i64_f64_to_void(i64_mem, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999993u, 0x99999999u);
        break;
    }
    case 874: { // stmt: Sig_i64_f64_to_void(i64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999913u, 0x99999999u);
        break;
    }
    case 875: { // stmt: Sig_i64_f64_to_void(i64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999913u, 0x99999999u);
        break;
    }
    case 876: { // stmt: Sig_i64_f64_to_void(i64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999913u, 0x99999999u);
        break;
    }
    case 877: { // stmt: Sig_i64_f64_to_void(i64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999913u, 0x99999999u);
        break;
    }
    case 878: { // stmt: Sig_i64_f64_to_void(i64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999913u, 0x99999999u);
        break;
    }
    case 879: { // stmt: Sig_i64_f64_to_void(i64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999913u, 0x99999999u);
        break;
    }
    case 880: { // stmt: Sig_i64_f64_to_void(i64_reg1, f64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 32, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999913u, 0x99999999u);
        break;
    }
    case 881: { // stmt: Sig_i32_i32_i32_to_void(i32_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 882: { // stmt: Sig_i32_i32_i32_to_void(i32_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 883: { // stmt: Sig_i32_i32_i32_to_void(i32_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 884: { // stmt: Sig_i32_i32_i32_to_void(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999000u, 0x99999999u);
        break;
    }
    case 885: { // stmt: Sig_i32_i32_i32_to_void(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999000u, 0x99999999u);
        break;
    }
    case 886: { // stmt: Sig_i32_i32_i32_to_void(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999000u, 0x99999999u);
        break;
    }
    case 887: { // stmt: Sig_i32_i32_i32_to_void(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999000u, 0x99999999u);
        break;
    }
    case 888: { // stmt: Sig_i32_i32_i32_to_void(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999000u, 0x99999999u);
        break;
    }
    case 889: { // stmt: Sig_i32_i32_i32_to_void(i32_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999000u, 0x99999999u);
        break;
    }
    case 890: { // stmt: Sig_i64_i32_i64_to_void(i64_mem, i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 891: { // stmt: Sig_i64_i32_i64_to_void(i64_mem, i32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 892: { // stmt: Sig_i64_i32_i64_to_void(i64_mem, i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999901u, 0x99999999u);
        break;
    }
    case 893: { // stmt: Sig_i64_i32_i64_to_void(i64_reg2, i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999101u, 0x99999999u);
        break;
    }
    case 894: { // stmt: Sig_i64_i32_i64_to_void(i64_reg2, i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999101u, 0x99999999u);
        break;
    }
    case 895: { // stmt: Sig_i64_i32_i64_to_void(i64_reg2, i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999101u, 0x99999999u);
        break;
    }
    case 896: { // stmt: Sig_i64_i32_i64_to_void(i64_reg2, i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999101u, 0x99999999u);
        break;
    }
    case 897: { // stmt: Sig_i64_i32_i64_to_void(i64_reg2, i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999101u, 0x99999999u);
        break;
    }
    case 898: { // stmt: Sig_i64_i32_i64_to_void(i64_reg2, i32_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999101u, 0x99999999u);
        break;
    }
    case 899: { // stmt: Sig_i64_i32_i32_to_void(i64_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 900: { // stmt: Sig_i64_i32_i32_to_void(i64_mem, i32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 901: { // stmt: Sig_i64_i32_i32_to_void(i64_mem, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999900u, 0x99999999u);
        break;
    }
    case 902: { // stmt: Sig_i64_i32_i32_to_void(i64_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999100u, 0x99999999u);
        break;
    }
    case 903: { // stmt: Sig_i64_i32_i32_to_void(i64_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999100u, 0x99999999u);
        break;
    }
    case 904: { // stmt: Sig_i64_i32_i32_to_void(i64_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999100u, 0x99999999u);
        break;
    }
    case 905: { // stmt: Sig_i64_i32_i32_to_void(i64_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999100u, 0x99999999u);
        break;
    }
    case 906: { // stmt: Sig_i64_i32_i32_to_void(i64_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999100u, 0x99999999u);
        break;
    }
    case 907: { // stmt: Sig_i64_i32_i32_to_void(i64_reg2, i32_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 9, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999100u, 0x99999999u);
        break;
    }
    case 908: { // stmt: Sig_i32_i64_i32_to_void(i32_mem, i64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 909: { // stmt: Sig_i32_i64_i32_to_void(i32_mem, i64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 910: { // stmt: Sig_i32_i64_i32_to_void(i32_mem, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999910u, 0x99999999u);
        break;
    }
    case 911: { // stmt: Sig_i32_i64_i32_to_void(i32_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999010u, 0x99999999u);
        break;
    }
    case 912: { // stmt: Sig_i32_i64_i32_to_void(i32_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999010u, 0x99999999u);
        break;
    }
    case 913: { // stmt: Sig_i32_i64_i32_to_void(i32_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999010u, 0x99999999u);
        break;
    }
    case 914: { // stmt: Sig_i32_i64_i32_to_void(i32_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999010u, 0x99999999u);
        break;
    }
    case 915: { // stmt: Sig_i32_i64_i32_to_void(i32_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999010u, 0x99999999u);
        break;
    }
    case 916: { // stmt: Sig_i32_i64_i32_to_void(i32_reg2, i64_reg1, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999010u, 0x99999999u);
        break;
    }
    case 917: { // stmt: Sig_i64_i64_i64_to_void(i64_mem, i64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 918: { // stmt: Sig_i64_i64_i64_to_void(i64_mem, i64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 919: { // stmt: Sig_i64_i64_i64_to_void(i64_mem, i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999911u, 0x99999999u);
        break;
    }
    case 920: { // stmt: Sig_i64_i64_i64_to_void(i64_reg2, i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999111u, 0x99999999u);
        break;
    }
    case 921: { // stmt: Sig_i64_i64_i64_to_void(i64_reg2, i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999111u, 0x99999999u);
        break;
    }
    case 922: { // stmt: Sig_i64_i64_i64_to_void(i64_reg2, i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999111u, 0x99999999u);
        break;
    }
    case 923: { // stmt: Sig_i64_i64_i64_to_void(i64_reg2, i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999111u, 0x99999999u);
        break;
    }
    case 924: { // stmt: Sig_i64_i64_i64_to_void(i64_reg2, i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999111u, 0x99999999u);
        break;
    }
    case 925: { // stmt: Sig_i64_i64_i64_to_void(i64_reg2, i64_reg1, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 17, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999111u, 0x99999999u);
        break;
    }
    case 926: { // stmt: Sig_to_void
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 927: { // stmt: Sig_i32_f32_i32_to_void(i32_mem, f32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 928: { // stmt: Sig_i32_f32_i32_to_void(i32_mem, f32_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 929: { // stmt: Sig_i32_f64_i32_to_void(i32_mem, f64_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 930: { // stmt: Sig_i32_f64_i32_to_void(i32_mem, f64_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 931: { // stmt: Sig_i32_v128_i32_to_void_aw(i32_mem, v128_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 932: { // stmt: Sig_i32_v128_i32_to_void_aw(i32_mem, v128_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 933: { // stmt: Sig_i32_ref_i32_to_void(i32_mem, ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 934: { // stmt: Sig_i32_ref_i32_to_void(i32_mem, ref_mem, i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 8, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 935: { // stmt: Sig_i64_f32_i64_to_void(i64_mem, f32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 936: { // stmt: Sig_i64_f32_i64_to_void(i64_mem, f32_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 937: { // stmt: Sig_i64_f64_i64_to_void(i64_mem, f64_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 938: { // stmt: Sig_i64_f64_i64_to_void(i64_mem, f64_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 939: { // stmt: Sig_i64_v128_i64_to_void_aw(i64_mem, v128_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 940: { // stmt: Sig_i64_v128_i64_to_void_aw(i64_mem, v128_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 941: { // stmt: Sig_i64_ref_i64_to_void(i64_mem, ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 942: { // stmt: Sig_i64_ref_i64_to_void(i64_mem, ref_mem, i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 16, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 943: { // stmt: Sig_stk_to_void
        for (int _ci = 0; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 944: { // stmt: Sig_v128_to_void_aw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 945: { // stmt: Sig_stk_i32_to_void(i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 946: { // stmt: Sig_stk_i32_to_void(i32_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 8, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999990u, 0x99999999u);
        break;
    }
    case 947: { // stmt: Sig_stk_i64_to_void(i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 948: { // stmt: Sig_stk_i64_to_void(i64_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 16, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 1, 0x99999991u, 0x99999999u);
        break;
    }
    case 949: { // stmt: Sig_stk_f32_to_void(f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 4, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 950: { // stmt: Sig_stk_f64_to_void(f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 5, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 951: { // stmt: Sig_stk_v128_to_void_aw(v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 6, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 952: { // stmt: Sig_stk_ref_to_void(ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        for (int _ci = 1; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 953: { // stmt: Sig_ref_i32_to_void(ref_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 954: { // stmt: Sig_ref_i64_to_void(ref_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 3, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 955: { // stmt: Sig_ref_f32_to_void(ref_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 4, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 956: { // stmt: Sig_ref_f64_to_void(ref_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 5, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 957: { // stmt: Sig_ref_v128_to_void_aw(ref_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 958: { // stmt: Sig_ref_ref_to_void(ref_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 959: { // stmt: Sig_ref_i32_i32_to_void(ref_mem, i32_mem, i32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 2, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 960: { // stmt: Sig_ref_i32_i64_to_void(ref_mem, i32_mem, i64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 3, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 961: { // stmt: Sig_ref_i32_f32_to_void(ref_mem, i32_mem, f32_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 4, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 962: { // stmt: Sig_ref_i32_f64_to_void(ref_mem, i32_mem, f64_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 5, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 963: { // stmt: Sig_ref_i32_v128_to_void_aw(ref_mem, i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 6, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 964: { // stmt: Sig_ref_i32_ref_to_void(ref_mem, i32_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 7, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 2), state->children[2], 7, ctx);
        for (int _ci = 3; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 965: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_mem, i32_mem, i32_mem)
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
    case 966: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_mem, i32_mem, i32_reg0)
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
    case 967: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_mem, i32_reg1, i32_reg0)
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
    case 968: { // stmt: Sig_ref_i32_i32_i32_to_void(ref_mem, i32_reg2, i32_reg1, i32_reg0)
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
    case 969: { // stmt: Sig_ref_i32_i64_i32_to_void(ref_mem, i32_mem, i64_mem, i32_mem)
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
    case 970: { // stmt: Sig_ref_i32_i64_i32_to_void(ref_mem, i32_mem, i64_mem, i32_reg0)
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
    case 971: { // stmt: Sig_ref_i32_f32_i32_to_void(ref_mem, i32_mem, f32_mem, i32_mem)
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
    case 972: { // stmt: Sig_ref_i32_f32_i32_to_void(ref_mem, i32_mem, f32_mem, i32_reg0)
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
    case 973: { // stmt: Sig_ref_i32_f64_i32_to_void(ref_mem, i32_mem, f64_mem, i32_mem)
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
    case 974: { // stmt: Sig_ref_i32_f64_i32_to_void(ref_mem, i32_mem, f64_mem, i32_reg0)
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
    case 975: { // stmt: Sig_ref_i32_v128_i32_to_void_aw(ref_mem, i32_mem, v128_mem, i32_mem)
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
    case 976: { // stmt: Sig_ref_i32_v128_i32_to_void_aw(ref_mem, i32_mem, v128_mem, i32_reg0)
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
    case 977: { // stmt: Sig_ref_i32_ref_i32_to_void(ref_mem, i32_mem, ref_mem, i32_mem)
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
    case 978: { // stmt: Sig_ref_i32_ref_i32_to_void(ref_mem, i32_mem, ref_mem, i32_reg0)
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
    case 979: { // stmt: Sig_ref_i32_ref_i32_i32_to_void(ref_mem, i32_mem, ref_mem, i32_mem, i32_mem)
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
    case 980: { // stmt: Sig_ref_i32_ref_i32_i32_to_void(ref_mem, i32_mem, ref_mem, i32_mem, i32_reg0)
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
    case 981: { // stmt: Sig_ref_i32_ref_i32_i32_to_void(ref_mem, i32_mem, ref_mem, i32_reg1, i32_reg0)
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
    case 982: { // stmt: Sig_i32_v128_to_void_aw(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 983: { // stmt: Sig_i32_ref_to_void(i32_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 984: { // stmt: Sig_i64_v128_to_void_aw(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 985: { // stmt: Sig_i64_ref_to_void(i64_mem, ref_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 7, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 986: { // stmt: Sig_i32_v128_to_void(i32_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 987: { // stmt: Sig_i32_v128_to_void(i32_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 2, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 988: { // stmt: Sig_i32_v128_to_void(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999044u, 0x99999999u);
        break;
    }
    case 989: { // stmt: Sig_i32_v128_to_void(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999044u, 0x99999999u);
        break;
    }
    case 990: { // stmt: Sig_i32_v128_to_void(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999044u, 0x99999999u);
        break;
    }
    case 991: { // stmt: Sig_i32_v128_to_void(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999044u, 0x99999999u);
        break;
    }
    case 992: { // stmt: Sig_i32_v128_to_void(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999044u, 0x99999999u);
        break;
    }
    case 993: { // stmt: Sig_i32_v128_to_void(i32_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 10, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999044u, 0x99999999u);
        break;
    }
    case 994: { // stmt: Sig_i64_v128_to_void(i64_mem, v128_mem)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 6, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 0, 0x99999999u, 0x99999999u);
        break;
    }
    case 995: { // stmt: Sig_i64_v128_to_void(i64_mem, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 3, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 2, 0x99999944u, 0x99999999u);
        break;
    }
    case 996: { // stmt: Sig_i64_v128_to_void(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 3, 0x99999144u, 0x99999999u);
        break;
    }
    case 997: { // stmt: Sig_i64_v128_to_void(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 4, 0x99999144u, 0x99999999u);
        break;
    }
    case 998: { // stmt: Sig_i64_v128_to_void(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 5, 0x99999144u, 0x99999999u);
        break;
    }
    case 999: { // stmt: Sig_i64_v128_to_void(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 6, 0x99999144u, 0x99999999u);
        break;
    }
    case 1000: { // stmt: Sig_i64_v128_to_void(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 7, 0x99999144u, 0x99999999u);
        break;
    }
    case 1001: { // stmt: Sig_i64_v128_to_void(i64_reg2, v128_reg0)
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 0), state->children[0], 18, ctx);
        jav_tile_burg_reduce(BURG_NODE_CHILD(node, 1), state->children[1], 40, ctx);
        for (int _ci = 2; _ci < state->child_count; _ci++) {
            jav_tile_burg_reduce(BURG_NODE_CHILD(node, _ci), state->children[_ci], 1, ctx);
        }
        jav_t2_stamp(node, 8, 0x99999144u, 0x99999999u);
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
