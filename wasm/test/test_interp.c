// test_interp.c — interpreter-tier gate, driven off the c-lite zero-copy load
// path: the .wasm is indexed once, jav_view_nav recovers the code-body span from
// the overlay, and the opgen-generated handlers run over it in place. add(3,5)==8.
// The owning tree is never built — the span index alone feeds the interpreter.

#include "jav_view_nav.h"
#include "jav_view_reader.h"
#include "bbq_arena.h"
#include "interp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    const bbq_field_capture *cs = jav_view_find_section(m.root, 10, buf);   // code section
    if (!cs) { fprintf(stderr, "no code\n"); return 2; }
    bbq_bytes_t body = jav_view_code_entry_bytes(cs, 0, buf);

    // Set up the frame. The code cursor reads the locals-declaration vector
    // (skeleton: 0 entries), leaving pos at the first instruction. Params are
    // locals[0..1] = (3, 5).
    vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm);
    bbq_ctx_init(&vm.frame.code, body.data, body.length);
    uint32_t nlocal_decls = 0;
    bbq_read_uleb128_u32(&vm.frame.code, &nlocal_decls);   // 0 for the add function
    vm.frame.locals[0].i = 3; vm.frame.local_types[0] = T_INT;
    vm.frame.locals[1].i = 5; vm.frame.local_types[1] = T_INT;
    vm.frame.num_locals = 2;

    jav_status_t s = interp_run(&vm, NULL);

    int ok = (s == JAV_RETURN && jav_tos_type(&vm) == T_INT && jav_tos(&vm).i == 8);
    printf("interp  local.get 0; local.get 1; i32.add; end  (3,5) -> %d  status=%d  [%s]\n",
           jav_tos(&vm).i, (int)s, ok ? "PASS" : "FAIL");

    bbq_arena_free(&ar);
    free(buf);
    printf("\n1E (interp half) gate: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
