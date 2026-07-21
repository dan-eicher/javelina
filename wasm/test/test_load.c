// test_load.c — Phase-C gate: the VM loads and runs a module through the c-lite
// zero-copy read path, on BOTH tiers. The .wasm is read once and indexed by the
// generated jav_view_reader; jav_view_nav locates the code section and recovers
// the FuncBody as a span into the SAME image; the interpreter and the JIT each
// execute off that span. The owning reader (jav_module_read) is NOT used — this
// proves the index alone drives execution. add(3,5) == 8 on interp == JIT.
#include "jav_view_nav.h"
#include "jav_view_reader.h"
#include "bbq_arena.h"
#include "interp.h"
#include "jit_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Frame the recovered body: skip the locals-declaration vector, install (a,b) as
// locals[0..1], leave the cursor at the first instruction (same as test_skeleton).
static void setup(vm_t* vm, bbq_bytes_t body, int32_t a, int32_t b) {
    memset(vm, 0, sizeof *vm); jav_vm_init(vm);
    bbq_ctx_init(&vm->frame.code, body.data, body.length);
    uint32_t nlocal_decls = 0;
    bbq_read_uleb128_u32(&vm->frame.code, &nlocal_decls);
    vm->frame.locals[0].i = a; vm->frame.local_types[0] = T_INT;
    vm->frame.locals[1].i = b; vm->frame.local_types[1] = T_INT;
    vm->frame.num_locals = 2;
}

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "add.wasm";
    FILE* fp = fopen(path, "rb");
    if (!fp) { perror(path); return 2; }
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    uint8_t* buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) { perror("fread"); return 2; }
    fclose(fp);

    // ── the c-lite load path: index the image, recover the code-body span ──
    bbq_arena ar; bbq_arena_init(&ar, 0);
    bbq_capture_metadata m = jav_view_module(buf, (size_t)n, &ar);
    if (!m.success) { fprintf(stderr, "c-lite read failed @%zu: %s\n",
                              m.error_offset, m.error_message ? m.error_message : "?"); return 1; }
    const bbq_field_capture* cs = jav_view_find_section(m.root, 10, buf);
    if (!cs) { fprintf(stderr, "no code section\n"); return 1; }
    bbq_bytes_t body = jav_view_code_entry_bytes(cs, 0, buf);
    if (!body.data || !body.length) { fprintf(stderr, "no code body span\n"); return 1; }

    // ── run the recovered span on both tiers ──
    vm_t vi; setup(&vi, body, 3, 5);
    jav_status_t si = interp_run(&vi, NULL);

    vm_t vj; setup(&vj, body, 3, 5);
    jav_status_t sj = jav_jit_run(&vj);

    int ri = jav_tos(&vi).i, rj = jav_tos(&vj).i;
    int ok = (si == JAV_RETURN && sj == JAV_RETURN && ri == 8 && rj == 8 && ri == rj);
    printf("c-lite load  interp (3,5) -> %d   jit (3,5) -> %d   [%s]\n",
           ri, rj, ok ? "PASS" : "FAIL");

    bbq_arena_free(&ar);
    free(buf);
    return ok ? 0 : 1;
}
