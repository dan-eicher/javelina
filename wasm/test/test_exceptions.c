// test_exceptions.c — try_table / throw / throw_ref: same-frame catch, catch_all,
// uncaught (unwinds out), CROSS-FRAME (throw in a callee, caught in the caller), and
// catch_all_ref + throw_ref rethrow. Both tiers (interp == JIT) — the validator runs
// the full §7.6 pass; throw/throw_ref resync like br_on_null, try_table's catch vector
// is skipped by the JIT walk like br_table.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* tag $0 : [i32] -> ε  (an exception carrying one i32) */
static const jav_valtype_t TAGP[1] = { WVT_I32 };
static const jav_functype_t TAGS[1] = { { TAGP, 1, NULL, 0 } };
static const u4 TAG_IDS[1] = { 0 };   // §4.2 the tag's store identity (one tag → id 0); throw/catch match on this
static const jav_valtype_t RES_I32[1] = { WVT_I32 };

#define UNCAUGHT (-0x5EEE)

/* Validate + run ONE function (no calls). Returns its i32 result, UNCAUGHT if a throw
 * escaped, or -999 on validation failure. */
static int run1(const uint8_t* code, size_t n, int jit){
    jav_st_entry_t* st; unsigned nst; jav_try_t* tt; unsigned ntt;
    jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1; cx.tags=TAGS; cx.ntags=1;
    if (!jav_typecheck_ex(code,n,&cx,&st,&nst,&tt,&ntt,NULL)) return -999;
    jav_func_t f[1]; memset(f,0,sizeof f);
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st,.trytable=tt,.ntry=ntt};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.tags=TAGS; vm.cluster.num_tags=1; vm.cluster.tag_ids=TAG_IDS;
    struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap;
    jav_heap_gc_init(&heap,&vm);   // an exn is a managed GC object → the collector must be live
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st; vm.frame.trytable=tt; vm.frame.ntry=ntt;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.unwinding ? UNCAUGHT : jav_tos(&vm).i;
    jav_heap_gc_destroy(&heap); jav_vm_free(&vm); bbq_vec_free(st); bbq_vec_free(tt);
    return r;
}

static int emit_uleb(uint8_t* p, uint32_t v){ int n=0; do{ uint8_t b=v&0x7f; v>>=7; if(v) b|=0x80; p[n++]=b; }while(v); return n; }

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int exp){   /* interp == JIT == exp */
    int i=run1(c,n,0), j=run1(c,n,1);
    int ok=(i==j && i==exp);
    printf("  %-44s interp=%-8d jit=%-8d exp=%-8d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    /* same-frame: block $o(i32){ try_table(i32) catch tag0->$o { const 99; throw 0 } }
     * throw is caught, branches to $o carrying 99 -> function returns 99. */
    static const uint8_t a[]={ 0x02,0x7f,
                                 0x1f,0x7f, 0x01, 0x00,0x00,0x00,   /* try_table(i32) catch(0) tag0 label0 */
                                   0x41,0x2a, 0x08,0x00,            /* const 42; throw 0 */
                                 0x0b,                              /* end try_table */
                               0x0b, 0x0b };                        /* end $o; end fn */
    val("same-frame catch carries the value", a,sizeof a, 42);

    /* catch_all (kind 2, no tag): catches; label $o is void here, so wrap to return 7.
     * block $o(i32){ try_table(void) catch_all->$o2 ... }  — simpler: catch_all to a
     * void block, then const 7. */
    static const uint8_t b[]={ 0x02,0x7f,                            /* block $o (i32) */
                                 0x02,0x40,                          /* block $h (void) */
                                   0x1f,0x40, 0x01, 0x02,0x00,       /* try_table(void) catch_all label0 ($h) */
                                     0x41,0x05, 0x08,0x00,           /* const 5; throw 0 */
                                   0x0b,                             /* end try_table */
                                 0x0b,                               /* end $h (caught lands here) */
                                 0x41,0x07,                          /* const 7 */
                               0x0b, 0x0b };                         /* end $o; end fn */
    val("catch_all then returns 7", b,sizeof b, 7);

    /* uncaught: const 42; throw 0 with no handler -> unwinds out of the function. */
    static const uint8_t c[]={ 0x41,0x2a, 0x08,0x00, 0x0b };
    val("uncaught throw unwinds out", c,sizeof c, UNCAUGHT);

    /* CROSS-FRAME: func0 wraps `call 1` in a try_table catching tag0; func1 throws
     * tag0(33). The throw escapes func1, unwinds through jav_call, and func0 catches
     * it -> returns 33. */
    static const uint8_t f0[]={ 0x02,0x7f,                          /* block $o (result i32) */
                                  0x1f,0x40, 0x01, 0x00,0x00,0x00,  /* try_table(void) catch tag0 label0 */
                                    0x10,0x01,                      /* call 1 */
                                  0x0b,                             /* end try_table */
                                  0x41,0x7f,                        /* const -1 (no-throw path) */
                                0x0b, 0x0b };                       /* end $o; end fn */
    static const uint8_t f1[]={ 0x41,0x21, 0x08,0x00, 0x0b };       /* const 33; throw 0; end */
    {
        static const jav_functype_t SIG0 = { NULL,0, RES_I32,1 };
        static const jav_functype_t SIG1 = { NULL,0, NULL,0 };
        static const jav_functype_t FSIGS[2] = { SIG0, SIG1 };
        jav_valtype_t VOID0;  (void)VOID0;
        /* validate func0 (calls func1) and func1 (throws) */
        jav_st_entry_t *st0,*st1; unsigned n0,n1; jav_try_t *tt0,*tt1; unsigned k0,k1;
        jav_vctx_t cx0={0}; cx0.results=RES_I32; cx0.nresults=1; cx0.tags=TAGS; cx0.ntags=1;
                            cx0.func_sigs=FSIGS; cx0.nfuncs=2;
        jav_vctx_t cx1={0}; cx1.tags=TAGS; cx1.ntags=1; cx1.func_sigs=FSIGS; cx1.nfuncs=2;
        int ok = jav_typecheck_ex(f0,sizeof f0,&cx0,&st0,&n0,&tt0,&k0,NULL)
              && jav_typecheck_ex(f1,sizeof f1,&cx1,&st1,&n1,&tt1,&k1,NULL);
        int gi=-999, gj=-999;
        if (ok) for (int jit=0; jit<2; jit++) {
            jav_func_t fns[2]; memset(fns,0,sizeof fns);
            fns[0]=(jav_func_t){.code=f0,.code_len=sizeof f0,.num_results=1,.sidetable=st0,.trytable=tt0,.ntry=k0};
            fns[1]=(jav_func_t){.code=f1,.code_len=sizeof f1,.num_results=0,.sidetable=st1,.trytable=tt1,.ntry=k1};
            vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=fns; vm.cluster.num_functions=2;
            vm.cluster.tags=TAGS; vm.cluster.num_tags=1; vm.cluster.tag_ids=TAG_IDS;
            struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
            bbq_ctx_init(&vm.frame.code,f0,sizeof f0); vm.frame.sidetable=st0; vm.frame.trytable=tt0; vm.frame.ntry=k0;
            if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
            int g = vm.unwinding ? UNCAUGHT : jav_tos(&vm).i; if (jit) gj=g; else gi=g;
            jav_heap_gc_destroy(&heap); jav_vm_free(&vm);
        }
        int cok=(gi==gj && gi==33);
        printf("  %-44s interp=%-8d jit=%-8d exp=%-8d [%s]\n","cross-frame: callee throws, caller catches",gi,gj,33,cok?"PASS":"FAIL");
        fails+=!cok;
        bbq_vec_free(st0); bbq_vec_free(st1); bbq_vec_free(tt0); bbq_vec_free(tt1);   // free both functions' side/try tables
    }

    /* RETHROW: inner try catch_all_ref binds the exnref; throw_ref re-raises it; the
     * outer try catches tag0 -> returns its value (7). Exercises catch_all_ref + throw_ref. */
    {
        static const jav_valtype_t EXNR[1] = { WVT_REF };
        static const uint32_t      EXNR_T[1] = { (uint32_t)HT_EXN };
        static const jav_functype_t TYPES[1] = { { NULL,0, EXNR,1, NULL, EXNR_T } };   /* $0 : [] -> [exnref] */
        static const uint8_t r[]={ 0x02,0x7f,                              /* block $o (i32) */
                                     0x1f,0x7f, 0x01, 0x00,0x00,0x00,      /* outer try(i32) catch tag0 -> $o */
                                       0x02,0x00,                          /* block $i (type $0 -> exnref) */
                                         0x1f,0x00, 0x01, 0x03,0x00,       /* inner try(type$0) catch_all_ref -> $i */
                                           0x41,0x07, 0x08,0x00,           /* const 7; throw 0 */
                                         0x0b,                             /* end inner try */
                                       0x0b,                               /* end $i -> exnref on stack */
                                       0x0a,                               /* throw_ref (re-raise) */
                                     0x0b,                                 /* end outer try (-> i32, polymorphic) */
                                   0x0b, 0x0b };                           /* end $o; end fn */
        jav_st_entry_t* st; unsigned nst; jav_try_t* tt; unsigned ntt;
        jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1; cx.tags=TAGS; cx.ntags=1; cx.types=TYPES; cx.ntypes=1;
        int gi=-999, gj=-999;
        if (jav_typecheck_ex(r,sizeof r,&cx,&st,&nst,&tt,&ntt,NULL)) {
            for (int jit=0; jit<2; jit++) {
                jav_func_t f[1]; memset(f,0,sizeof f);
                f[0]=(jav_func_t){.code=r,.code_len=sizeof r,.num_results=1,.sidetable=st,.trytable=tt,.ntry=ntt};
                vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1; vm.cluster.tags=TAGS; vm.cluster.num_tags=1; vm.cluster.tag_ids=TAG_IDS;
                struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
                bbq_ctx_init(&vm.frame.code,r,sizeof r); vm.frame.sidetable=st; vm.frame.trytable=tt; vm.frame.ntry=ntt;
                if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
                int g = vm.unwinding ? UNCAUGHT : jav_tos(&vm).i; if (jit) gj=g; else gi=g;
                jav_heap_gc_destroy(&heap); jav_vm_free(&vm);
            }
            bbq_vec_free(st); bbq_vec_free(tt);
        }
        int rok=(gi==gj && gi==7);
        printf("  %-44s interp=%-8d jit=%-8d exp=%-8d [%s]\n","catch_all_ref + throw_ref rethrow",gi,gj,7,rok?"PASS":"FAIL");
        fails+=!rok;
    }

    /* HANDLER STACK has NO cap (was vm->handlers[256], silently not installing #257+). 300 nested
     * try_tables where ONLY the innermost (handler #300) catches the thrown tag0 — the outer 299 catch
     * tag1, which never matches. Pre-fix, handler #300 wasn't installed → tag0 escaped every tag1
     * handler → UNCAUGHT; now it's caught → 42. (Both tiers; the handler stack is a bbq_vec.) */
    {
        const int N = 300;
        static const jav_valtype_t TP[1] = { WVT_I32 };
        static const jav_functype_t T2[2] = { { TP,1,NULL,0 }, { TP,1,NULL,0 } };   /* tag0, tag1 : [i32]->ε */
        static const u4 IDS2[2] = { 0, 1 };
        uint8_t* code = malloc((size_t)N*8 + 64); size_t n = 0;
        code[n++]=0x02; code[n++]=0x7f;                                /* block $o (result i32) */
        for (int i=0;i<N;i++) {                                        /* N nested try_table(i32) catch <tag> -> $o */
            code[n++]=0x1f; code[n++]=0x7f; code[n++]=0x01; code[n++]=0x00;   /* try_table(i32), 1 catch, kind 0 */
            code[n++]=(uint8_t)((i==N-1)?0:1);                         /* innermost catches tag0; the rest tag1 */
            n += emit_uleb(&code[n], (uint32_t)i);                     /* label i = the outer block $o */
        }
        code[n++]=0x41; code[n++]=0x2a; code[n++]=0x08; code[n++]=0x00;/* const 42; throw 0 (tag0) */
        for (int i=0;i<N;i++) code[n++]=0x0b;                          /* end each try_table */
        code[n++]=0x0b; code[n++]=0x0b;                                /* end $o; end fn */
        jav_st_entry_t* st; unsigned nst; jav_try_t* tt; unsigned ntt;
        jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1; cx.tags=T2; cx.ntags=2;
        int gi=-999, gj=-999;
        if (jav_typecheck_ex(code,n,&cx,&st,&nst,&tt,&ntt,NULL)) {
            for (int jit=0; jit<2; jit++) {
                jav_func_t f[1]; memset(f,0,sizeof f);
                f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st,.trytable=tt,.ntry=ntt};
                vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1; vm.cluster.tags=T2; vm.cluster.num_tags=2; vm.cluster.tag_ids=IDS2;
                struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
                bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st; vm.frame.trytable=tt; vm.frame.ntry=ntt;
                if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
                int g = vm.unwinding ? UNCAUGHT : jav_tos(&vm).i; if (jit) gj=g; else gi=g;
                jav_heap_gc_destroy(&heap); jav_vm_free(&vm);
            }
            bbq_vec_free(st); bbq_vec_free(tt);
        }
        int dok=(gi==gj && gi==42);
        printf("  %-44s interp=%-8d jit=%-8d exp=%-8d [%s]\n","300 nested try_tables (no handler cap)",gi,gj,42,dok?"PASS":"FAIL");
        fails+=!dok; free(code);
    }

    printf("\nexceptions (try_table / throw / throw_ref, same + cross-frame): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
