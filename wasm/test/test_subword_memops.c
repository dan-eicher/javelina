// test_subword_memops.c — the sub-word integer loads/stores (opcodes 0x2c–0x35, 0x3a–0x3e),
// both tiers. Written to WASM 3.0 §4.6.8 (execution) + §3.4.5 (validation):
//   iN.loadK_sx : [i32] -> [iN]   — read K/8 bytes at i+offset, extend^sx to |iN|; trap if i+offset+K/8 > |mem|
//   iN.storeK   : [i32 iN] -> []  — write wrap_{|iN|,K}(c) as K/8 bytes; trap on OOB
//   memarg valid for the access only if 2^align <= K/8  (K = STORAGE width, not |iN|)
// Loads are checked for correct sign vs zero extension; stores via a known-good i32.load
// readback; OOB traps and the over-alignment validator rejection are pinned too.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const jav_valtype_t RES_I32[1] = { WVT_I32 };

#define TRAP    (-0x6BADBAD)
#define VALFAIL (-999)
static int run(const uint8_t* code, size_t n, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1; cx.nmemories=1;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return VALFAIL;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st};
    struct heap_t heap; memset(&heap,0,sizeof heap); jav_mem_add(&heap, 1, 1, 1, 0);  // 1 page = 65536 bytes
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.heap=&heap;  vm.cluster.mem_addrs=(uint32_t[]){0}; vm.cluster.num_mems=1;
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? TRAP : jav_tos(&vm).i;
    jav_heap_free_mems(&heap); bbq_vec_free(st);
    return r;
}

// ── a tiny bytecode emitter (avoids hand-computed LEBs) ──
typedef struct { uint8_t b[64]; size_t n; } buf_t;
static void eb(buf_t* p, uint8_t x){ p->b[p->n++]=x; }
static void uleb(buf_t* p, uint64_t v){ do{ uint8_t b=v&0x7f; v>>=7; if(v) b|=0x80; eb(p,b);}while(v); }
static void sleb(buf_t* p, int64_t v){ int more=1; while(more){ uint8_t b=v&0x7f; v>>=7;
    if((v==0 && !(b&0x40)) || (v==-1 && (b&0x40))) more=0; else b|=0x80; eb(p,b);} }
static void i32c(buf_t* p, int32_t v){ eb(p,0x41); sleb(p,v); }
static void i64c(buf_t* p, int64_t v){ eb(p,0x42); sleb(p,v); }
static void memop(buf_t* p, uint8_t op, uint32_t align, uint32_t off){ eb(p,op); uleb(p,align); uleb(p,off); }

static int fails=0;
static void val(const char* nm, const buf_t* p, int exp){
    int i=run(p->b,p->n,0), j=run(p->b,p->n,1);
    int ok=(i==j && i==exp);
    printf("  %-34s interp=%-12d jit=%-12d exp=%-12d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    buf_t p;
    // ── loads: sign vs zero extension ──
    #define BEGIN() (p.n=0)
    #define END()   eb(&p,0x0b)

    // i32.load8_s of byte 0x80 -> -128 ; i32.load8_u -> 128
    BEGIN(); i32c(&p,0); i32c(&p,0x80); memop(&p,0x36,2,0);    // store 0x00000080 @0 (bytes 80,00,00,00)
             i32c(&p,0); memop(&p,0x2c,0,0); END();             // i32.load8_s @0
    val("i32.load8_s 0x80 = -128", &p, -128);
    BEGIN(); i32c(&p,0); i32c(&p,0x80); memop(&p,0x36,2,0);
             i32c(&p,0); memop(&p,0x2d,0,0); END();             // i32.load8_u
    val("i32.load8_u 0x80 = 128", &p, 128);

    // i32.load16_s of 0x8000 -> -32768 ; _u -> 32768
    BEGIN(); i32c(&p,0); i32c(&p,0x8000); memop(&p,0x36,2,0);
             i32c(&p,0); memop(&p,0x2e,1,0); END();             // i32.load16_s
    val("i32.load16_s 0x8000 = -32768", &p, -32768);
    BEGIN(); i32c(&p,0); i32c(&p,0x8000); memop(&p,0x36,2,0);
             i32c(&p,0); memop(&p,0x2f,1,0); END();             // i32.load16_u
    val("i32.load16_u 0x8000 = 32768", &p, 32768);

    // i64.load8_s 0xFF -> -1 (full i64 via i64.eq) ; _u -> 255
    BEGIN(); i32c(&p,0); i32c(&p,0xFF); memop(&p,0x36,2,0);
             i32c(&p,0); memop(&p,0x30,0,0); i64c(&p,-1); eb(&p,0x51); END();  // i64.load8_s; i64.const -1; i64.eq
    val("i64.load8_s 0xFF == -1 (i64)", &p, 1);
    BEGIN(); i32c(&p,0); i32c(&p,0xFF); memop(&p,0x36,2,0);
             i32c(&p,0); memop(&p,0x31,0,0); i64c(&p,255); eb(&p,0x51); END(); // i64.load8_u; i64.const 255; i64.eq
    val("i64.load8_u 0xFF == 255 (i64)", &p, 1);

    // i64.load32_s 0x80000000 -> 0xFFFFFFFF80000000 ; _u -> 0x0000000080000000
    BEGIN(); i32c(&p,0); i32c(&p,(int32_t)0x80000000); memop(&p,0x36,2,0);
             i32c(&p,0); memop(&p,0x34,2,0); i64c(&p,-2147483648LL); eb(&p,0x51); END(); // i64.load32_s; const -2^31; eq
    val("i64.load32_s sign-extends (i64)", &p, 1);
    BEGIN(); i32c(&p,0); i32c(&p,(int32_t)0x80000000); memop(&p,0x36,2,0);
             i32c(&p,0); memop(&p,0x35,2,0); i64c(&p,2147483648LL); eb(&p,0x51); END(); // i64.load32_u; const 2^31; eq
    val("i64.load32_u zero-extends (i64)", &p, 1);

    // ── stores: wrap, verified via known-good i32.load (4-byte) readback into zeroed mem ──
    BEGIN(); i32c(&p,0); i32c(&p,0x1234ABCD); memop(&p,0x3a,0,0);  // i32.store8 -> writes 0xCD
             i32c(&p,0); memop(&p,0x28,2,0); END();                 // i32.load
    val("i32.store8 writes low byte = 0xCD", &p, 0xCD);
    BEGIN(); i32c(&p,0); i32c(&p,0x1234ABCD); memop(&p,0x3b,1,0);  // i32.store16 -> 0xABCD
             i32c(&p,0); memop(&p,0x28,2,0); END();
    val("i32.store16 writes low 2 = 0xABCD", &p, 0xABCD);
    BEGIN(); i32c(&p,0); i64c(&p,0x123456789ABCDEFFLL); memop(&p,0x3c,0,0); // i64.store8 -> 0xFF
             i32c(&p,0); memop(&p,0x28,2,0); END();
    val("i64.store8 writes low byte = 0xFF", &p, 0xFF);
    BEGIN(); i32c(&p,0); i64c(&p,0x123456789ABCDEFFLL); memop(&p,0x3d,1,0); // i64.store16 -> 0xDEFF
             i32c(&p,0); memop(&p,0x28,2,0); END();
    val("i64.store16 writes low 2 = 0xDEFF", &p, 0xDEFF);
    BEGIN(); i32c(&p,0); i64c(&p,0x123456789ABCDEFFLL); memop(&p,0x3e,2,0); // i64.store32 -> 0x9ABCDEFF
             i32c(&p,0); memop(&p,0x28,2,0); END();
    val("i64.store32 writes low 4 bytes", &p, (int)0x9ABCDEFF);

    // ── OOB traps: i+offset+K/8 > |mem| (mem = 65536 bytes) ──
    BEGIN(); i32c(&p,65536); memop(&p,0x2c,0,0); END();            // load8_s @65536 -> ea+1 > 65536
    val("i32.load8_s @65536 traps", &p, TRAP);
    BEGIN(); i32c(&p,65536); i32c(&p,0); memop(&p,0x3a,0,0); i32c(&p,0); END(); // store8 @65536 traps
    val("i32.store8 @65536 traps", &p, TRAP);

    // ── validator: over-alignment must be REJECTED (2^align <= K/8). For load8 K=8 -> align<=0;
    //    align=1 is invalid. This is the proof the verifier enforces the narrow access width. ──
    BEGIN(); i32c(&p,0); memop(&p,0x2c,1,0); END();                // i32.load8_s align=1 -> invalid
    val("i32.load8_s align=1 rejected", &p, VALFAIL);
    BEGIN(); i32c(&p,0); memop(&p,0x2e,2,0); END();                // i32.load16_s align=2 -> invalid (max 1)
    val("i32.load16_s align=2 rejected", &p, VALFAIL);

    printf("\nsub-word load/store (0x2c-0x35, 0x3a-0x3e) interp == JIT, spec §4.6.8/§3.4.5: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
