/* structurer.c — SIR-spine scope identification (see structurer.h).
 *
 * A single iterative depth-first walk (the recursive form would blow the C
 * stack on long methods) with white/gray/black colouring, exactly the
 * gray==on-stack back-edge test the optimizer uses for its reachability walk.
 * A successor found GRAY is a retreating edge → its target is a loop header;
 * BLACK is a forward/cross edge. Postorder is recorded as nodes blacken;
 * reversing it gives reverse-postorder (RPO), in which — for the reducible
 * CFGs structured Java produces — every forward edge runs low→high. */
#include "javelina/compiler/structurer.h"
#include <string.h>

enum { ST_WHITE = 0, ST_GRAY = 1, ST_BLACK = 2 };

/* Mix a pointer to a 32-bit htree key (same finalizer the optimizer uses). */
static uint32_t st_ptr_hash(const void* p) {
    uintptr_t x = (uintptr_t)p;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    return (uint32_t)x;
}

/* Key identifying one out-edge (source node + successor slot). */
static uint32_t st_edge_key(const sir_node_t* from, int slot) {
    return st_ptr_hash(from) + (uint32_t)slot * 0x9e3779b1u;
}

int wasm_structure_index(const wasm_structure_t* s, const sir_node_t* n) {
    void* v = bbq_htree_search(s->index, st_ptr_hash(n));
    return v ? (int)((uintptr_t)v - 1) : -1;
}

void wasm_structure_build(wasm_structure_t* s, bbq_arena* arena, sir_node_t* entry) {
    s->arena           = arena;
    s->rpo             = NULL;
    s->index           = bbq_htree_create();
    s->is_loop_header  = NULL;
    s->is_merge        = NULL;
    s->fwd_preds       = NULL;
    s->back_edges      = NULL;
    s->back_edge_count = 0;
    s->count           = 0;
    if (!entry) return;

    bbq_htree* color = bbq_htree_create();

    typedef struct { sir_node_t* node; int succ; } frame_t;
    frame_t*     stack = NULL;
    sir_node_t** post  = NULL;   /* postorder (blackened) order */

    /* Back-edges are discovered before RPO indices exist, so record the raw
     * endpoints now and resolve to indices once the order is fixed. */
    typedef struct { sir_node_t* from; sir_node_t* to; int slot; } raw_be_t;
    raw_be_t* raw = NULL;

    frame_t f0 = { entry, 0 };
    bbq_vec_push(stack, f0);
    bbq_htree_insert(color, st_ptr_hash(entry), (void*)(uintptr_t)ST_GRAY);

    while (bbq_vec_len(stack) > 0) {
        frame_t* f  = &stack[bbq_vec_len(stack) - 1];
        int      sc = sir_succ_count(f->node);
        if (f->succ < sc) {
            int         slot = f->succ++;
            sir_node_t* t    = sir_succ(f->node, slot);
            if (!t) continue;
            uintptr_t c = (uintptr_t)bbq_htree_search(color, st_ptr_hash(t));
            if (c == ST_WHITE) {
                bbq_htree_insert(color, st_ptr_hash(t), (void*)(uintptr_t)ST_GRAY);
                frame_t fn = { t, 0 };
                bbq_vec_push(stack, fn);
            } else if (c == ST_GRAY) {
                raw_be_t be = { f->node, t, slot };
                bbq_vec_push(raw, be);
            }
            /* ST_BLACK: forward / cross edge — no scope marker here. */
        } else {
            bbq_htree_insert(color, st_ptr_hash(f->node), (void*)(uintptr_t)ST_BLACK);
            bbq_vec_push(post, f->node);
            bbq__vec_hdr(stack)->len--;   /* pop the finished frame */
        }
    }

    /* RPO = reverse(postorder); assign indices. */
    bbq_vec_reverse(post);
    s->rpo   = post;
    s->count = bbq_vec_len(post);
    for (int i = 0; i < s->count; i++)
        bbq_htree_insert(s->index, st_ptr_hash(s->rpo[i]), (void*)(uintptr_t)(i + 1));

    size_t n = (size_t)s->count;
    s->is_loop_header = (bool*)bbq_arena_alloc(arena, n * sizeof(bool));
    s->is_merge       = (bool*)bbq_arena_alloc(arena, n * sizeof(bool));
    s->fwd_preds      = (int*) bbq_arena_alloc(arena, n * sizeof(int));
    memset(s->is_loop_header, 0, n * sizeof(bool));
    memset(s->is_merge,       0, n * sizeof(bool));
    memset(s->fwd_preds,      0, n * sizeof(int));

    /* Resolve back-edges to indices, flag headers, and record each one's
     * edge key so the forward-predecessor pass can exclude it. */
    bbq_htree* be_set = bbq_htree_create();
    for (int i = 0; i < (int)bbq_vec_len(raw); i++) {
        raw_be_t* be = &raw[i];
        wasm_back_edge_t e = {
            wasm_structure_index(s, be->from),
            wasm_structure_index(s, be->to),
            be->slot,
        };
        bbq_vec_push(s->back_edges, e);
        if (e.to >= 0) s->is_loop_header[e.to] = true;
        bbq_htree_insert(be_set, st_edge_key(be->from, be->slot), (void*)(uintptr_t)1);
    }
    s->back_edge_count = (int)bbq_vec_len(s->back_edges);

    /* Forward in-degree: count every out-edge that is not a back-edge. */
    for (int i = 0; i < s->count; i++) {
        sir_node_t* u  = s->rpo[i];
        int         sc = sir_succ_count(u);
        for (int slot = 0; slot < sc; slot++) {
            sir_node_t* v = sir_succ(u, slot);
            if (!v) continue;
            if (bbq_htree_search(be_set, st_edge_key(u, slot))) continue;
            int vi = wasm_structure_index(s, v);
            if (vi >= 0) s->fwd_preds[vi]++;
        }
    }
    for (int i = 0; i < s->count; i++)
        s->is_merge[i] = s->fwd_preds[i] >= 2;

    bbq_htree_destroy(be_set);
    bbq_htree_destroy(color);
    bbq_vec_free(stack);
    bbq_vec_free(raw);
}

bool wasm_edge_is_back(const wasm_structure_t* s, int from, int succ_slot) {
    for (int i = 0; i < s->back_edge_count; i++)
        if (s->back_edges[i].from == from && s->back_edges[i].succ_slot == succ_slot)
            return true;
    return false;
}

wasm_transfer_t wasm_classify_edge(const wasm_structure_t* s, int from, int succ_slot) {
    sir_node_t*     v = sir_succ(s->rpo[from], succ_slot);
    wasm_transfer_t r = { WT_BR_FORWARD, wasm_structure_index(s, v) };

    /* Back-edge takes precedence: a retreating edge is always a br to its loop,
     * never a fall-through (its target is laid out earlier). */
    if (wasm_edge_is_back(s, from, succ_slot)) { r.kind = WT_BR_LOOP; return r; }

    /* Layout-adjacent forward edge: the successor is emitted next, so the
     * transfer is implicit — even when the successor is the return node
     * (it emits its own terminator). */
    if (r.to == from + 1) { r.kind = WT_FALLTHROUGH; return r; }

    /* A non-adjacent edge whose continuation is the function return: inline the
     * terminator rather than branching forward to a shared return block. */
    if (v && (v->tag == SIR_RETURN || v->tag == SIR_RETURNVOID)) {
        r.kind = WT_RETURN; return r;
    }

    /* Otherwise a forward branch out to the successor's enclosing block end. */
    r.kind = WT_BR_FORWARD;
    return r;
}

void wasm_structure_free(wasm_structure_t* s) {
    if (s->index) { bbq_htree_destroy(s->index); s->index = NULL; }
    bbq_vec_free(s->rpo);
    bbq_vec_free(s->back_edges);
    s->count = s->back_edge_count = 0;
}
