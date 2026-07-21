// test_storage.c — the slot<->storage width accessor (jav_storage.h). Proves a
// value round-trips through natural-width storage for every type INCLUDING v128, and
// that jav_slot_store writes EXACTLY jav_valtype_size(t) bytes — never clobbering a
// neighbor (the over-write face of the field-stride pun this is meant to kill).
#include "runtime_api.h"     // slot_t, the value model (default backend — no -DJAVELINA_BACKEND_TYPES)
#include "jav_storage.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CK(label, cond) do { int ok=(cond); printf("  %-40s [%s]\n", label, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

// Store v of type t at offset 8 in a 0xAA-filled 48-byte buffer; verify the bytes
// before/after the written span are untouched and the value loads back identical.
static int roundtrips(const char* label, jav_valtype_t t, slot_t v) {
    unsigned char buf[48];
    memset(buf, 0xAA, sizeof buf);
    uint8_t sz = jav_valtype_size(t);
    jav_slot_store(buf + 8, t, v);
    for (int i = 0; i < 8; i++)       if (buf[i] != 0xAA)        { printf("  %s: clobbered BEFORE\n", label); return 0; }
    for (int i = 8 + sz; i < 48; i++) if (buf[i] != 0xAA)        { printf("  %s: clobbered AFTER (wrote >%u bytes)\n", label, sz); return 0; }
    slot_t r = jav_slot_load(buf + 8, t);
    return memcmp(&r, &v, sz) == 0;
}

int main(void) {
    printf("slot<->storage width accessor:\n");
    CK("size: i32=4",   jav_valtype_size(WVT_I32)==4);
    CK("size: i64=8",   jav_valtype_size(WVT_I64)==8);
    CK("size: f32=4",   jav_valtype_size(WVT_F32)==4);
    CK("size: f64=8",   jav_valtype_size(WVT_F64)==8);
    CK("size: v128=16", jav_valtype_size(WVT_V128)==16);
    CK("size: funcref=8", jav_valtype_size(WVT_REF)==8);
    CK("align: v128=16", jav_valtype_align(WVT_V128)==16);

    slot_t s; memset(&s, 0, sizeof s);
    s.i = (s4)0x11223344;                       CK("i32 round-trips, 4 bytes",  roundtrips("i32", WVT_I32, s));
    memset(&s,0,sizeof s); s.l = 0x1122334455667788LL; CK("i64 round-trips, 8 bytes",  roundtrips("i64", WVT_I64, s));
    memset(&s,0,sizeof s); s.f = 3.5f;          CK("f32 round-trips, 4 bytes",  roundtrips("f32", WVT_F32, s));
    memset(&s,0,sizeof s); s.d = 2.718281828;   CK("f64 round-trips, 8 bytes",  roundtrips("f64", WVT_F64, s));
    memset(&s,0,sizeof s); s.v.u64[0]=0xDEADBEEFCAFEF00DULL; s.v.u64[1]=0x0102030405060708ULL;
                                                CK("v128 round-trips, 16 bytes", roundtrips("v128", WVT_V128, s));
    memset(&s,0,sizeof s); s.l = 0x00000000ABCD1234LL; CK("funcref round-trips, 8 bytes", roundtrips("funcref", WVT_REF, s));

    // A narrow load must leave the slot's high bytes zero (no stale bits onto the stack).
    unsigned char b[16]; memset(b, 0xFF, sizeof b);
    b[0]=0x01; b[1]=0x00; b[2]=0x00; b[3]=0x00;
    slot_t got = jav_slot_load(b, WVT_I32);
    CK("narrow load zero-extends the slot", got.l == 0x1);

    printf("\nstorage width accessor: %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
