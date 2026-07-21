// test_immix_pool.c — the ported Immix block pool: OS-acquire, reuse via the free
// list, alignment, trim back to the OS, and the lifetime counters.
#include "immix_block_allocator.h"
#include <stdio.h>

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-46s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

int main(void){
    imx_block_pool_t p; imx_block_pool_init(&p);

    imx_block_t* a = imx_block_pool_acquire(&p);
    imx_block_t* b = imx_block_pool_acquire(&p);
    CK(a && b && a != b, "acquire two distinct blocks");
    CK(((uintptr_t)a % IMX_BLOCK_SIZE)==0 && ((uintptr_t)b % IMX_BLOCK_SIZE)==0, "blocks are BLOCK_SIZE-aligned");
    CK(imx_block_pool_allocated(&p)==2 && imx_block_pool_live(&p)==2 && imx_block_pool_free(&p)==0,
       "counts: 2 allocated, 2 live, 0 free");

    /* release b, then re-acquire -> reuse it (no new OS alloc) */
    imx_block_pool_release(&p, b);
    CK(imx_block_pool_live(&p)==1 && imx_block_pool_free(&p)==1, "after release: 1 live, 1 free");
    imx_block_t* c = imx_block_pool_acquire(&p);
    CK(c == b && p.total_os_allocs==2, "re-acquire reuses the freed block, no new OS alloc");

    /* release both, trim back to 0 -> frees to the OS */
    imx_block_pool_release(&p, a);
    imx_block_pool_release(&p, c);
    size_t freed = imx_block_pool_trim(&p, 0);
    CK(freed==2 && imx_block_pool_allocated(&p)==0 && p.total_os_frees==2, "trim(0): frees both to the OS");

    /* trim keeps `keep` pooled blocks — acquire two DISTINCT, then release both */
    imx_block_t* x = imx_block_pool_acquire(&p);
    imx_block_t* y = imx_block_pool_acquire(&p);
    imx_block_pool_release(&p, x);
    imx_block_pool_release(&p, y);
    CK(imx_block_pool_trim(&p, 1)==1 && imx_block_pool_free(&p)==1, "trim(1): keeps one pooled block");

    imx_block_pool_destroy(&p);
    printf("\nimmix block pool (ported C): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
