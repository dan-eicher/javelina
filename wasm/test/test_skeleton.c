// test_skeleton.c — the walking skeleton's heartbeat: the SAME add.wasm function
// run through both tiers — the in-place interpreter and the copy-and-patch JIT —
// asserting interp == JIT == 8. The body is recovered off the c-lite zero-copy
// load path (jav_view_nav over the span index), never the owning tree. Both tiers
// execute the opgen-generated body of each opcode, so agreement is the differential
// check the whole design rests on.

#include "jav_view_nav.h"
#include "jav_view_reader.h"
#include "bbq_arena.h"
#include "interp.h"
#include "jit_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Set up a frame to run `body`: skip the locals-declaration vector, install
// (a,b) as locals[0..1], position the cursor at the first instruction.
static void setup(vm_t *vm, bbq_bytes_t body, int32_t a, int32_t b) {
    memset(vm, 0, sizeof *vm); jav_vm_init(vm);
    bbq_ctx_init(&vm->frame.code, body.data, body.length);
    uint32_t nlocal_decls = 0;
    bbq_read_uleb128_u32(&vm->frame.code, &nlocal_decls);   // 0 for the add function
    vm->frame.locals[0].i = a; vm->frame.local_types[0] = T_INT;
    vm->frame.locals[1].i = b; vm->frame.local_types[1] = T_INT;
    vm->frame.num_locals = 2;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "add.wasm";
    FILE *fp = fopen(path, "rb");
    if (!fp) { perror(path); return 2; }
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) { perror("fread"); return 2; }
    fclose(fp);

    bbq_arena ar; bbq_arena_init(&ar, 0);
    bbq_capture_metadata m = jav_view_module(buf, (size_t)n, &ar);
    if (!m.success) { fprintf(stderr, "c-lite read failed\n"); return 2; }
    const bbq_field_capture *cs = jav_view_find_section(m.root, 10, buf);
    bbq_bytes_t body = jav_view_code_entry_bytes(cs, 0, buf);

    vm_t vi; setup(&vi, body, 3, 5);
    jav_status_t si = interp_run(&vi, NULL);

    vm_t vj; setup(&vj, body, 3, 5);
    jav_status_t sj = jav_jit_run(&vj);

    int ri = jav_tos(&vi).i, rj = jav_tos(&vj).i;
    int ok = (si == JAV_RETURN && sj == JAV_RETURN &&
              ri == 8 && rj == 8 && ri == rj);
    printf("interp (3,5) -> %d   jit (3,5) -> %d   [%s]\n",
           ri, rj, (ri == rj) ? "agree" : "DIVERGE");
    printf("\n1E gate  interp == JIT == 8: %s\n", ok ? "PASS" : "FAIL");

    bbq_arena_free(&ar);
    free(buf);
    return ok ? 0 : 1;
}
