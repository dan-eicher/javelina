// test_div.c — i32.div_s + the divide-by-zero trap, executed off the c-lite
// zero-copy load path: index the .wasm, recover the code-body span from the
// overlay (jav_view_nav), run the interpreter over it. No owning tree.
#include "jav_view_nav.h"
#include "jav_view_reader.h"
#include "bbq_arena.h"
#include "interp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run(const char* path, jav_status_t* st, int* res) {
    FILE* fp = fopen(path,"rb"); if(!fp){perror(path);return 0;}
    fseek(fp,0,SEEK_END); long n=ftell(fp); fseek(fp,0,SEEK_SET);
    uint8_t* buf=malloc(n); if(fread(buf,1,n,fp)!=(size_t)n){return 0;} fclose(fp);

    bbq_arena ar; bbq_arena_init(&ar, 0);
    bbq_capture_metadata m = jav_view_module(buf, (size_t)n, &ar);
    if (!m.success) { fprintf(stderr,"c-lite read failed\n"); bbq_arena_free(&ar); free(buf); return 0; }
    const bbq_field_capture* cs = jav_view_find_section(m.root, 10, buf);
    bbq_bytes_t body = jav_view_code_entry_bytes(cs, 0, buf);

    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm);
    bbq_ctx_init(&vm.frame.code, body.data, body.length);
    uint32_t nl=0; bbq_read_uleb128_u32(&vm.frame.code,&nl); vm.frame.num_locals=0;
    *st = interp_run(&vm,NULL); *res = jav_tos(&vm).i;
    bbq_arena_free(&ar); free(buf); return 1;
}
int main(void){
    jav_status_t s; int r; int fails=0;
    run("div.wasm",&s,&r);
    int ok1 = (s==JAV_RETURN && r==3);
    printf("  i32.div_s 10/3  -> %d  status=%d  [%s]\n", r, s, ok1?"PASS":"FAIL"); fails+=!ok1;
    run("div0.wasm",&s,&r);
    int ok2 = (s==JAV_TRAP);
    printf("  i32.div_s 10/0  -> status=%d (trap=%d)  [%s]\n", s, JAV_TRAP, ok2?"PASS":"FAIL"); fails+=!ok2;
    printf("\ndiv + trap execution: %s\n", fails?"FAIL":"PASS");
    return fails?1:0;
}
