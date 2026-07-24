/* wasm_module.c — assemble a compiled program into a .wasm module.
 *
 * Built the way `water` builds one: the burg + structured emit produce each
 * function's body BYTES (locals vec + instrs + 0x0B), and `wasm_types` produces
 * the type-section BYTES (the GC rec group). Both are decoded with the shared
 * jav readers (`jav_func_body_read` / `jav_type_section_read`) — the same grammar
 * the VM uses, so a malformed opcode/immediate/LEB/type is rejected here, at the
 * source — then parked in a `jav_module_t` and serialized with the one shared
 * `jav_module_write` (no hand-rolled framing, no second binary-layout authority).
 *
 * Reference types are concrete throughout (`(ref null $T)`): struct/array/func
 * types from `wasm_types`, function params/results from sema, and body-local
 * valtypes from the SIR reference descriptors the ddcg carries (ClassRef /
 * ArrayRef / PrimArray) — no `eqref` placeholder. */
#include "javelina/compiler/wasm_module.h"
#include "javelina/compiler/sir_support.h"
#include "javelina/compiler/type_lattice.h"     /* lat_value_class */
#include "javelina/compiler/codegen_method.h"   /* burg_ctx_t, codegen_method_structured */
#include "javelina/compiler/sir_optimizer.h"    /* sir_optimize (cctx->optimize) */
#include "javelina/compiler/descriptor.h"        /* desc_from_method — overloaded-export disambiguation */
#include "bbq_vec.h"
#include "bbq_read.h"                            /* bbq_ctx_t, bbq_ctx_init/free */
#include "jav_types.h"
#include "jav_reader.h"                          /* jav_func_body_read, jav_type_section_read */
#include "jav_writer.h"                          /* jav_module_write */
#include "jav_validate_module.h"                 /* jav_module_wf — §5.5.1 audit */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* SIR primitive datatype → WASM valtype byte (references are concrete and
 * emitted separately via their reference descriptor). The class decision
 * is the lattice's (lat_dt_valtype); this only picks the encoding byte. */
static uint8_t prim_valtype(sir_datatype_t dt) {
    switch (lat_dt_valtype(dt)) {
        case LAT_VT_I64:  return W_VT_I64;
        case LAT_VT_F32:  return W_VT_F32;
        case LAT_VT_F64:  return W_VT_F64;
        case LAT_VT_V128: return W_VT_V128;
        default:          return W_VT_I32;
    }
}

/* A SIR reference descriptor (ClassRef/ArrayRef/PrimArray) → the concrete WASM
 * heap type index, or -1 if it isn't a (recognised) reference descriptor. */
static int32_t resolve_ref_typeidx(wasm_types_t* wt, const sir_node_t* ref) {
    return wasm_types_ref_typeidx(wt, ref);   /* the shared descriptor→typeidx authority */
}

/* Per-slot type info gathered from the SIR: the datatype and (for references)
 * the resolved heap type index (-1 = primitive / not yet known). */
typedef struct { sir_datatype_t dt; int32_t typeidx; } slot_info_t;

/* DFS the SIR graph (data children + spine successors), recording each
 * Load/StoreLocal's datatype and reference type by slot. Memoized on visited
 * pointers — the SIR is a graph (loop back-edges, shared merges). A slot's
 * reference type is READ off the proven ClassRef/ArrayRef/PrimArray descriptor
 * the ddcg carried (ident loads via sema; spill temps via the `locref`
 * destination) — never recovered from the value here. */
static void collect_slots(wasm_types_t* wt, const sir_node_t* n,
                          const sir_node_t*** seen, slot_info_t* slots, int total) {
    if (!n) return;
    for (int i = 0; i < (int)bbq_vec_len(*seen); i++)
        if ((*seen)[i] == n) return;
    bbq_vec_push(*seen, n);

    if (n->tag == SIR_LOADLOCAL || n->tag == SIR_STORELOCAL) {
        int s = sir_local_slot(n);
        bool is_load = (n->tag == SIR_LOADLOCAL);
        sir_datatype_t dt = is_load ? n->load_local.data_type : n->store_local.data_type;
        const sir_node_t* ref = is_load ? n->load_local.ref_type : n->store_local.ref_type;
        if (s >= 0 && s < total) {
            slots[s].dt = dt;
            if (dt == SIR_DTREF) {
                int32_t ti = resolve_ref_typeidx(wt, ref);
                if (ti >= 0) slots[s].typeidx = ti;
            }
        }
    } else if (n->tag == SIR_EXCEPTIONENTRY) {
        /* The catch landing-pad binds its slot to a reference of the catch type
         * even when the catch body never reads the variable (no LoadLocal to
         * carry the descriptor) — type it from the ExceptionEntry's catch class
         * so the backend's `ref.cast`→`local.set` validates. */
        int s = n->exception_entry.local_slot;
        if (s >= 0 && s < total) {
            slots[s].dt = SIR_DTREF;
            int32_t ti = wasm_types_class_typeidx(wt, lat_handler_landing_class(wt->sema,
                                            n->exception_entry.catch_class_id));
            if (ti >= 0) slots[s].typeidx = ti;
        }
    }
    for (int i = 0; i < sir_arity(n); i++)
        collect_slots(wt, sir_child(n, i), seen, slots, total);
    for (int i = 0; i < sir_succ_count(n); i++)
        collect_slots(wt, sir_succ(n, i), seen, slots, total);
}

/* Emit the §5.4.5 locals vec for `method`'s body-local slots (those past the
 * `param_region` leading param/`this` slots), one run per local, concrete
 * valtypes. */
/* Emit the §5.4.5 locals vec for `method`'s body-local slots. Slot types come from
 * the descriptors the DDCG THREADED onto the SIR (collect_slots reads them off the
 * FINAL graph — final because Click's slot-pack renumbers slots, so the map can only
 * be authoritative post-optimization; this is a reader of the threaded facts, not a
 * second type authority). Returns false (fail-closed) if a reference slot reached
 * here with no threaded descriptor: that is a DDCG destination-threading bug, and we
 * reject the module rather than guess (ref Object) and mis-type a downstream consumer. */
static bool emit_locals_vec_pr(wasm_types_t* wt, const sir_method_t* method,
                               int param_region, emit_wasm_ctx* fb) {
    int total = method->max_locals;
    int nlocals = total - param_region;
    if (nlocals <= 0) { ew_u32(fb, 0); return true; }

    slot_info_t* slots = (slot_info_t*)calloc((size_t)total, sizeof *slots);
    for (int i = 0; i < total; i++) { slots[i].dt = SIR_DTINT; slots[i].typeidx = -1; }
    const sir_node_t** seen = NULL;
    collect_slots(wt, method->entry, &seen, slots, total);
    bbq_vec_free(seen);

    bool ok = true;
    ew_u32(fb, (uint32_t)nlocals);
    for (int s = param_region; s < total; s++) {
        ew_u32(fb, 1);                                          /* one run per local */
        if (slots[s].dt != SIR_DTREF) {
            ew_byte(fb, prim_valtype(slots[s].dt));
        } else if (slots[s].typeidx >= 0) {
            wasm_types_emit_ref(fb, slots[s].typeidx);
        } else {
            const sema_class_t* mc = method->class_id >= 0 ? sema_get_class(wt->sema, method->class_id) : NULL;
            fprintf(stderr, "wasm_assemble: %s.%s — body-local slot %d holds a reference whose type "
                    "was never threaded through its DDCG destination (a codegen bug, not a default)\n",
                    mc ? mc->name : "<clinit>",
                    (mc && method->method_id >= 0) ? mc->methods[method->method_id].name : "<clinit>", s);
            ok = false;
            wasm_types_emit_ref(fb, wasm_types_class_typeidx(wt, wasm_root_class(wt))); /* keep bytes well-formed; ok=false discards the module */
        }
    }
    free(slots);
    return ok;
}

static bool emit_locals_vec(wasm_types_t* wt, const sir_method_t* method,
                            const sema_method_t* m, emit_wasm_ctx* fb) {
    bool is_static = (m->modifiers & ACC_STATIC) != 0;
    return emit_locals_vec_pr(wt, method, m->param_count + (is_static ? 0 : 1), fb);
}

/* True if class `class_id` has more than one EXPORTED (non-private, non-ctor) method
 * named `name` — its "Class.name" export would then be a §2.5.10 duplicate, so the
 * export disambiguates by appending the JVM method descriptor. */
static bool export_name_overloaded(const sema_ctx_t* s, int class_id, const char* name) {
    const sema_class_t* cls = sema_get_class(s, class_id);
    int cnt = 0;
    for (int i = 0; i < (int)bbq_vec_len(cls->methods); i++) {
        const sema_method_t* m = &cls->methods[i];
        if (m->is_constructor || (m->modifiers & ACC_PRIVATE)) continue;
        if (strcmp(m->name, name) == 0) cnt++;
    }
    return cnt > 1;
}

/* ── Canonical INTERNAL linking names (jre.wasm ⇄ plugin). The jre RUNTIME export and the
 * plugin import build IDENTICAL bytes here, so a name resolves by construction. A method
 * name ALWAYS carries its JVM descriptor → every (class, method, signature) is unique with
 * no overload detection; globals use '#' so a method `out` and a static field `out` can't
 * collide. (Host-facing USER exports keep the clean "Class.method" scheme above.) Caller
 * frees the returned bytes. */
static uint8_t* canon_method_name(const sema_ctx_t* s, int cid, const sema_method_t* m, uint32_t* len_out) {
    /* FULLY QUALIFIED class name (§6.7): two classes may share a simple name
     * across packages (§7), and a simple-name link name would collide. */
    const char* cn = sema_get_class(s, cid)->fq_name;
    const char* desc = desc_from_method(s->arena, m->param_types, m->param_count, m->return_type, s);
    int cl = (int)strlen(cn), ml = (int)strlen(m->name), dl = (int)strlen(desc);
    int L = cl + 1 + ml + dl;
    uint8_t* nb = (uint8_t*)malloc((size_t)L);
    memcpy(nb, cn, (size_t)cl); nb[cl] = '.';
    memcpy(nb + cl + 1, m->name, (size_t)ml);
    memcpy(nb + cl + 1 + ml, desc, (size_t)dl);
    *len_out = (uint32_t)L; return nb;
}
static uint8_t* canon_hash_name(const char* cn, const char* member, uint32_t* len_out) {
    int cl = (int)strlen(cn), ml = (int)strlen(member);
    int L = cl + 1 + ml;
    uint8_t* nb = (uint8_t*)malloc((size_t)L);
    memcpy(nb, cn, (size_t)cl); nb[cl] = '#'; memcpy(nb + cl + 1, member, (size_t)ml);
    *len_out = (uint32_t)L; return nb;
}

/* A decoded §5.3.1 valtype struct for a global-import desc: emit the valtype bytes (the one
 * authority, `wasm_types_emit_valtype`) then read them back into the section struct. */
static jav_val_type_t plugin_valtype(wasm_types_t* wt, const sema_ctx_t* s, java_type_t t) {
    (void)s;
    emit_wasm_ctx vc = {0};
    wasm_types_emit_valtype(wt, &vc, t);
    jav_val_type_t vt; memset(&vt, 0, sizeof vt);
    bbq_ctx_t rc; bbq_ctx_init(&rc, vc.code, (size_t)bbq_vec_len(vc.code));
    (void)jav_val_type_read(&rc, &vt);
    bbq_ctx_free(&rc); bbq_vec_free(vc.code);
    return vt;
}

bool wasm_assemble_program(compiler_ctx_t* cctx, const sema_ctx_t* sctx,
                           wasm_types_t* wt, sir_method_t** methods, int mc,
                           emit_wasm_ctx* out) {
    /* The function-index authority is sema's emitted-function table (sema_func_*),
     * which the InvokeStatic immediate (wasm_func_index), the type section, and
     * this assembler all read — one authority, no re-derivation. */
    int nf = sema_func_count(sctx);
    /* The module initializer (if any) is one extra function appended past the
     * table at index nf — present in the function & code sections (and pointed at
     * by the start section), but NOT exported. */
    /* <clinit> is ALWAYS emitted now: it runs the reflection bootstrap fixup (set each
     * Class singleton's field0 = Class.class) at module start, plus any user static inits. */
    bool user_clinit = (cctx->clinit != NULL);
    int has_clinit = 1;
    wt->has_clinit = true;                  /* the type section appends its ()->() */
    wt->has_exceptions = sema_uses_exceptions(sctx);  /* sema's flag → tag section + tag functype */
    /* The iface_instanceof helper is emitted iff the program has any interface (the
     * only thing interface instanceof/checkcast can target). */
    wt->has_iface_helper = false;
    for (int ci = 0; ci < wt->num_classes; ci++)
        if (sema_get_class(sctx, ci)->is_interface) { wt->has_iface_helper = true; break; }
    int has_iface = wt->has_iface_helper ? 1 : 0;
    int nfuncs = nf + has_clinit + has_iface;
    int fcap = nfuncs ? nfuncs : 1;
    /* RUNTIME (jre.wasm) also exports every defined func + one global per Class singleton
     * + one per static field; size the export array for that. WHOLE/PLUGIN export ≤ nf. */
    int ecap = 1 + ((sctx->mode == SEMA_MODE_RUNTIME)   /* +1 for the exported I/O memory */
        ? nf + wt->nimports + wt->num_classes + wasm_global_count(wt) + 4   /* funcs + primitive natives + globals */
        : (nf ? nf : 1));

    uint32_t*         fidxs   = (uint32_t*)calloc((size_t)fcap, sizeof *fidxs);
    jav_export_t*     exports = (jav_export_t*)calloc((size_t)ecap, sizeof *exports);
    jav_code_entry_t* entries = (jav_code_entry_t*)calloc((size_t)fcap, sizeof *entries);
    int  nexp = 0;                              /* count of emitted exports (≤ nf) */
    bool ok = true;

    /* Run the per-method Click fixpoint in the call graph's REVERSE-TOPOLOGICAL
     * order — a callee before its caller — so a callee's stage-5 summary is available when
     * its caller is analyzed. Hoisted OUT of the assembly loop, which now only emits.
     *
     * Today no summary is consumed, so this is a pure REORDERING OF THE ANALYSIS: each
     * method is optimized exactly once, each method's emitted bytes are independent of that
     * order, and wasm_func_index (built pre-optimization from class_id/method_id, never from
     * iteration order) is untouched — so the module is BYTE-IDENTICAL to the old in-loop
     * order. That equality is the gate. The iterate-to-convergence WRAPPER (for recursion,
     * once a summary can change) does not exist yet — its convergence quantity would be the
     * summary, which does not exist either, so there is nothing to converge and the single
     * pass is exact.
     * <clinit> is not a call-graph node (it is instantiation-time); it stays optimized at its
     * own emit site below. */
    if (cctx->optimize) {
        /* Choi §4: converge the interprocedural summaries FIRST (summarize-only, iterated over the
         * reverse-topological order until stable — so a recursive/mutually-recursive callee stops
         * reading as a bottom method), THEN rewrite once per method in the same order with the
         * converged callee summaries in hand. (compiler_summarize_to_convergence builds the call
         * graph itself.) */
        compiler_summarize_to_convergence(cctx);
        int* order = (int*)bbq_arena_alloc(cctx->arena,
                                           (size_t)(mc > 0 ? mc : 1) * sizeof(int));
        int no = compiler_analysis_order(cctx, order);
        const char* only = getenv("JAVELINA_CLICK_ONLY");   /* the bisection hook, preserved */
        for (int oi = 0; oi < no; oi++) {
            int m = order[oi];
            bool match = !only;
            if (only) {
                const sema_class_t* mcl = sema_get_class(sctx, methods[m]->class_id);
                const char* qcn = mcl ? mcl->name : "?";
                const char* qmn = mcl ? mcl->methods[methods[m]->method_id].name : "?";
                char qual[256]; snprintf(qual, sizeof qual, "%s.%s", qcn ? qcn : "?",
                                         qmn ? qmn : "?");
                char toks[512]; snprintf(toks, sizeof toks, "%s", only);
                for (char* t = strtok(toks, ","); t && !match; t = strtok(NULL, ","))
                    if (strstr(qual, t)) match = true;
            }
            if (match) sir_optimize(cctx, m);
        }
    }

    for (int ai = 0; ai < mc && ok; ai++) {
        int cid = methods[ai]->class_id;
        /* pos = defined-function position (indexes the func/code/export arrays);
         * fi = funcidx (offset past the import range) used in the export desc and
         * by callers. A native method has pos < 0 (it is an import, not emitted). */
        int pos = sema_func_index(sctx, cid, methods[ai]->method_id);
        if (pos < 0) continue;                /* native/host import — emitted in the import section */
        int fi  = wasm_func_index(wt, cid, methods[ai]->method_id);
        const sema_class_t*  cls = sema_get_class(sctx, cid);
        const sema_method_t* sm  = &cls->methods[methods[ai]->method_id];
        const char* cn = cls->name; const char* mn = sm->name;

        /* Export the USER-class call surface, under "Class.method": non-constructor,
         * NON-PRIVATE methods. The host calls user entry points; a fused module
         * reaches everything else by funcidx, never by name —
         *   • PRIVATE methods are internal (JLS §6.6: inaccessible outside the class),
         *     never a host export surface;
         *   • CONSTRUCTORS are §12.5 init code invoked by `new`/super();
         *   • LIBRARY (import_pkg>=0) members are the JRE internals.
         * Same-named methods across classes stay distinct via the "Class." prefix.
         * A class's OWN overloaded methods (same name, different signature) would
         * collide on "Class.method" (§2.5.10 duplicate export name), so an overloaded
         * name disambiguates by appending its JVM method descriptor — "T.p" becomes
         * "T.p(I)I" / "T.p(J)I". Non-overloaded methods keep the clean "Class.method". */
        if (sm->is_synthetic_main) {
            /* E7.1a: the program entry, exported as the plain "$main" the runner invokes
             * (argc, argv-offset) -> exit-code — no "Class." prefix, so the runner needn't
             * know the main class's name. */
            uint8_t* nb = (uint8_t*)malloc(5); memcpy(nb, "$main", 5);
            jav_export_t ex; memset(&ex, 0, sizeof ex);
            ex.name.count = 5; ex.name.bytes.data = nb; ex.name.bytes.length = 5;
            ex.kind = 0; ex.idx = (uint32_t)fi;
            exports[nexp++] = ex;
        }
        else if (cls->import_pkg < 0 && !sm->is_constructor && !(sm->modifiers & ACC_PRIVATE)) {
            const char* desc = export_name_overloaded(sctx, cid, mn)
                ? desc_from_method(sctx->arena, sm->param_types, sm->param_count, sm->return_type, sctx)
                : "";
            int clen = (int)strlen(cn), mlen = (int)strlen(mn), dlen = (int)strlen(desc);
            int L = clen + 1 + mlen + dlen;
            uint8_t* nb = (uint8_t*)malloc((size_t)L);
            memcpy(nb, cn, (size_t)clen); nb[clen] = '.';
            memcpy(nb + clen + 1, mn, (size_t)mlen);
            memcpy(nb + clen + 1 + mlen, desc, (size_t)dlen);
            jav_export_t ex; memset(&ex, 0, sizeof ex);
            ex.name.count = (uint32_t)L; ex.name.bytes.data = nb; ex.name.bytes.length = (size_t)L;
            ex.kind = 0; ex.idx = (uint32_t)fi;
            exports[nexp++] = ex;
        }

        /* RUNTIME (jre.wasm): export EVERY defined func under its canonical internal name
         * "<Class>.<method><descriptor>" so a plugin imports the complete java.lang surface
         * (ctors, overlays, forwarders — all of it) by name. No user classes exist here. */
        if (sctx->mode == SEMA_MODE_RUNTIME) {
            uint32_t L; uint8_t* nb = canon_method_name(sctx, cid, sm, &L);
            jav_export_t ex; memset(&ex, 0, sizeof ex);
            ex.name.count = L; ex.name.bytes.data = nb; ex.name.bytes.length = (size_t)L;
            ex.kind = 0; ex.idx = (uint32_t)fi;
            exports[nexp++] = ex;
        }

        /* Click ran in the reverse-topological driver above (before this loop); the
         * assembly loop now only emits. The census below reads each method's post-optimize
         * facts, which are all final by here. */

        /* JAVELINA_GUARD_CENSUS=1: report how many §15 implicit-exception guards
         * the DDCG emitted — the baseline the optimizer's guard elimination is
         * measured against. Free, because the DDCG records each guard as it
         * emits it (it is the stage that knows). */
        if (getenv("JAVELINA_GUARD_CENSUS")) {
            static int total = 0, gone = 0;
            int nf = 0;
            const compiler_fact_t* fs = compiler_get_facts(cctx, ai, &nf);
            int ng = 0, g_gone = 0;
            /* A guard the optimizer proved dead had its Branch re-tagged as a
             * Nop by cp_rewrite_branch_fold. The sidecar makes measuring this
             * free: we recorded the Branch, so we can just look at it. */
            static int by_kind[COMPILER_GUARD_KIND_COUNT] = {0};
            static int by_kind_gone[COMPILER_GUARD_KIND_COUNT] = {0};
            for (int fi2 = 0; fi2 < nf; fi2++) {
                if (fs[fi2].kind != COMPILER_FACT_GUARD) continue;
                ng++;
                int k = fs[fi2].a;                       /* compiler_guard_kind_t */
                if (k >= 0 && k < COMPILER_GUARD_KIND_COUNT) by_kind[k]++;
                /* A guard the optimizer proved dead was re-tagged away from the node it
                 * was emitted as. That is a Branch for every kind EXCEPT the array-store
                 * check, which is a call in an ExprEffect (JLS §10.10 — see the enum). */
                int emitted_tag = (k == COMPILER_GUARD_ARRAY_STORE) ? SIR_EXPREFFECT
                                                                    : SIR_BRANCH;
                if (fs[fi2].key && (int)fs[fi2].key->tag != emitted_tag) {
                    g_gone++;
                    if (k >= 0 && k < COMPILER_GUARD_KIND_COUNT) by_kind_gone[k]++;
                }
            }
            total += ng; gone += g_gone;
            /* One name per kind — the enum's count sizes it, so a new kind that is not
             * named here fails to compile instead of vanishing from the census. */
            static const char* kn[COMPILER_GUARD_KIND_COUNT] = {
                "NPE", "IDX_LOW", "IDX_HIGH", "NEG_SIZE", "DIV_ZERO", "CLASS_CAST",
                "DIV_OVERFLOW", "ARRAY_STORE" };
            fprintf(stderr, "guard-census: %s.%s emitted=%d eliminated=%d "
                    "(totals: %d / %d)", cn ? cn : "?", mn ? mn : "?",
                    ng, g_gone, gone, total);
            for (int k = 0; k < COMPILER_GUARD_KIND_COUNT; k++)
                fprintf(stderr, " | %s %d/%d", kn[k], by_kind_gone[k], by_kind[k]);
            fprintf(stderr, " | devirt %d", cctx->devirt_total);
            fprintf(stderr, " | noescape %d/%d (struct %d, array %d) | scalar %d",
                    cctx->noescape_total, cctx->alloc_total,
                    cctx->noescape_struct, cctx->noescape_array,
                    cctx->scalar_total);
            fprintf(stderr, "\n");
        }

        /* code: locals vec (concrete valtypes) + burg/structurer body bytes;
         * decode via the shared reader into a jav_func_body (the grammar gate). */
        int nsc = 0; const compiler_fact_t* sc = compiler_get_facts(cctx, ai, &nsc);
        burg_ctx_t bc = {0}; burg_ctx_init(&bc); bc.types = wt;
        codegen_method_structured(methods[ai], sc, nsc, &bc);
        if (burg_has_error(&bc)) {
            fprintf(stderr, "wasm_assemble: %s.%s (func %d) — %s (%d); body truncated, "
                            "refusing to ship it\n",
                    cn, mn, fi, burg_get_error(&bc), burg_get_error_arg(&bc));
            ok = false;                       /* fail-loud: never ship a half-emitted body */
        }
        emit_wasm_ctx fb = {0};
        if (!emit_locals_vec(wt, methods[ai], sm, &fb)) ok = false;
        for (int k = 0; k < (int)bbq_vec_len(bc.emit.code); k++) ew_byte(&fb, bc.emit.code[k]);
        bbq_vec_free(bc.emit.code);             /* caller owns the burg emit buffer (codegen_method.h) */
        burg_ctx_free(&bc);

        jav_code_entry_t e; memset(&e, 0, sizeof e);
        bbq_ctx_t rc; bbq_ctx_init(&rc, fb.code, (size_t)bbq_vec_len(fb.code));
        if (!jav_func_body_read(&rc, &e.body)) {
            fprintf(stderr, "wasm_assemble: %s.%s (func %d) — backend emitted bytes the "
                            "spec grammar rejects (codegen bug)\n", cn, mn, fi);
            ok = false;                       /* fail-loud: never ship a bad body */
        }
        bbq_ctx_free(&rc);
        bbq_vec_free(fb.code);
        entries[pos] = e;
    }

    /* Native marshaling forwarders: each ref-carrying import has a defined slot
     * (sema appended it past the compiled methods). Emit its forwarder body —
     * generated any↔extern marshaling around the externref import, not SIR — into
     * that slot. */
    for (int i = 0; ok && i < sema_import_count(sctx); i++) {
        sema_func_ent_t fe = sema_import_at(sctx, i);
        int pos = sema_func_index(sctx, fe.class_id, fe.method_id);
        if (pos < 0) continue;                /* primitive native: direct import, no forwarder */
        emit_wasm_ctx fb = {0};
        wasm_emit_forwarder_body(wt, sctx, fe.class_id, fe.method_id, &fb);
        jav_code_entry_t e; memset(&e, 0, sizeof e);
        bbq_ctx_t rc; bbq_ctx_init(&rc, fb.code, (size_t)bbq_vec_len(fb.code));
        if (!jav_func_body_read(&rc, &e.body)) {
            const sema_class_t* icls = sema_get_class(sctx, fe.class_id);
            fprintf(stderr, "wasm_assemble: %s.%s forwarder — emitted bytes the spec "
                            "grammar rejects (forwarder bug)\n",
                    icls->name, icls->methods[fe.method_id].name);
            ok = false;
        }
        bbq_ctx_free(&rc);
        bbq_vec_free(fb.code);
        entries[pos] = e;
        /* RUNTIME: export the forwarder (a ref-carrying native's natural-typed defined func)
         * so a plugin importing that java.lang native by name reaches it. */
        if (sctx->mode == SEMA_MODE_RUNTIME) {
            const sema_method_t* im = &sema_get_class(sctx, fe.class_id)->methods[fe.method_id];
            uint32_t L; uint8_t* nb = canon_method_name(sctx, fe.class_id, im, &L);
            jav_export_t ex; memset(&ex, 0, sizeof ex);
            ex.name.count = L; ex.name.bytes.data = nb; ex.name.bytes.length = (size_t)L;
            ex.kind = 0; ex.idx = (uint32_t)wasm_func_index(wt, fe.class_id, fe.method_id);
            exports[nexp++] = ex;
        }
    }

    /* Function-type indices: stable regardless of codegen order — they sit past
     * structs + vtable + the SIGNATURE arrays (frozen before codegen), and the
     * body-local arrays trail PAST the func types, so a body registering a new
     * array never shifts a func-type index a call_ref already baked. */
    for (int ai = 0; ai < mc; ai++) {
        int cid = methods[ai]->class_id;
        int pos = sema_func_index(sctx, cid, methods[ai]->method_id);
        if (pos < 0) continue;
        fidxs[pos] = (uint32_t)wasm_functype_idx(wt, cid, methods[ai]->method_id);
    }
    for (int i = 0; i < sema_import_count(sctx); i++) {    /* forwarders: natural func type */
        sema_func_ent_t fe = sema_import_at(sctx, i);
        int pos = sema_func_index(sctx, fe.class_id, fe.method_id);
        if (pos < 0) continue;
        fidxs[pos] = (uint32_t)wasm_functype_idx(wt, fe.class_id, fe.method_id);
    }

    /* The module initializer: appended as function index nf with the trailing
     * ()->() type. Its body is structured like any method (no scopes/try — field
     * initializers are expressions), decoded through the shared grammar gate. */
    if (ok) {
        emit_wasm_ctx fb = {0};
        burg_ctx_t bc = {0};
        if (user_clinit) {                      /* locals vec: the user <clinit>'s (else none) */
            if (cctx->optimize) sir_optimize(cctx, SIR_OPT_CLINIT);
            burg_ctx_init(&bc); bc.types = wt;
            codegen_method_structured(cctx->clinit, cctx->clinit_facts,
                                      cctx->clinit_fact_count, &bc);
            if (burg_has_error(&bc)) {
                fprintf(stderr, "wasm_assemble: <clinit> — %s (%d); body truncated, "
                                "refusing to ship it\n",
                        burg_get_error(&bc), burg_get_error_arg(&bc));
                ok = false;
            }
            if (!emit_locals_vec_pr(wt, cctx->clinit, 0, &fb)) ok = false;
        } else {
            ew_u32(&fb, 0);                     /* no locals */
        }
        wasm_types_emit_reflect_fixup(wt, sctx, &fb);   /* run the field0 fixup FIRST */
        if (sctx->mode == SEMA_MODE_RUNTIME) {
            /* Eager static init for the shared runtime: a plugin links against an ALREADY-
             * instantiated jre, so the jre must initialize its OWN static state at instantiation
             * (§4.5.4 — the start function runs at the end of instantiation). Otherwise a plugin
             * reading a jre static object field (e.g. System.out) through its imported global
             * observes it before that field's lazy $ensure_init has run. Call every needs_init
             * class's $ensure_init here: the §12.4.2 barriers self-order (each ensures its super /
             * dependencies first) and are idempotent, so loop order is irrelevant. WHOLE and PLUGIN
             * stay lazy — a program's OWN classes init on first active use (§12.4.1). */
            for (int ci = 0; ci < wt->num_classes; ci++) {
                if (!sema_class_needs_init(sctx, ci)) continue;
                int em = sema_ensure_init_cp(sctx, ci);
                if (em < 0) continue;
                ew_emit(&fb, WOP_CALL); ew_u32(&fb, (uint32_t)wasm_func_index(wt, ci, em));
            }
        }
        if (user_clinit) {                      /* then the user static-init body (ends with `end`) */
            for (int k = 0; k < (int)bbq_vec_len(bc.emit.code); k++) ew_byte(&fb, bc.emit.code[k]);
            bbq_vec_free(bc.emit.code);
            burg_ctx_free(&bc);
        } else {
            ew_byte(&fb, 0x0B);                 /* terminating end (no user body) */
        }

        jav_code_entry_t e; memset(&e, 0, sizeof e);
        bbq_ctx_t rc; bbq_ctx_init(&rc, fb.code, (size_t)bbq_vec_len(fb.code));
        if (!jav_func_body_read(&rc, &e.body)) {
            fprintf(stderr, "wasm_assemble: <clinit> — backend emitted bytes the spec "
                            "grammar rejects (module-init codegen bug)\n");
            ok = false;
        }
        bbq_ctx_free(&rc);
        bbq_vec_free(fb.code);
        entries[nf] = e;
        fidxs[nf] = (uint32_t)wasm_clinit_functype_idx(wt);
    }

    /* The iface_instanceof helper: a synthesized function past <clinit>, its body
     * hand-emitted by wasm_types (a scan of obj.ClassDesc.interfaces), decoded via the
     * shared grammar gate like any other function. */
    if (has_iface && ok) {
        emit_wasm_ctx fb = {0};
        wasm_types_emit_iface_helper(wt, &fb);
        jav_code_entry_t e; memset(&e, 0, sizeof e);
        bbq_ctx_t rc; bbq_ctx_init(&rc, fb.code, (size_t)bbq_vec_len(fb.code));
        if (!jav_func_body_read(&rc, &e.body)) {
            fprintf(stderr, "wasm_assemble: iface_instanceof — emitted bytes the spec "
                            "grammar rejects (helper codegen bug)\n");
            ok = false;
        }
        bbq_ctx_free(&rc);
        bbq_vec_free(fb.code);
        int pos = nf + has_clinit;
        entries[pos] = e;
        fidxs[pos]   = (uint32_t)wasm_iface_helper_functype_idx(wt);
    }

    /* RUNTIME (jre.wasm): export the java.lang GLOBALS a plugin references — one Class
     * singleton "<Class>#class" per class (the object-header identity + vtable carrier) and
     * one "<Class>#<field>" per static field. Vtable globals are NOT exported: a plugin
     * reaches a java.lang vtable only through an object's imported Class singleton. */
    if (ok && sctx->mode == SEMA_MODE_RUNTIME) {
        /* Primitive-only natives are host imports (no forwarder) — export them ALIASED at
         * their import funcidx so a plugin importing them from jre reaches jre's host func.
         * Ref-carrying natives were exported via their forwarders in the method loop above. */
        for (int i = 0; i < wt->nimports; i++) {
            sema_func_ent_t fe = sema_import_at(sctx, i);
            const sema_method_t* im = &sema_get_class(sctx, fe.class_id)->methods[fe.method_id];
            bool refc = !(im->modifiers & ACC_STATIC)
                     || im->return_type.tag == JT_CLASS || im->return_type.tag == JT_ARRAY;
            for (int p = 0; !refc && p < im->param_count; p++)
                refc = im->param_types[p].tag == JT_CLASS || im->param_types[p].tag == JT_ARRAY;
            if (refc) continue;                          /* forwarded → already exported */
            uint32_t L; uint8_t* nb = canon_method_name(sctx, fe.class_id, im, &L);
            jav_export_t ex; memset(&ex, 0, sizeof ex);
            ex.name.count = L; ex.name.bytes.data = nb; ex.name.bytes.length = (size_t)L;
            ex.kind = 0; ex.idx = (uint32_t)wasm_func_index(wt, fe.class_id, fe.method_id);
            exports[nexp++] = ex;
        }
        for (int ci = 0; ci < wt->num_classes; ci++) {
            const sema_class_t* c = sema_get_class(sctx, ci);
            uint32_t L; uint8_t* nb = canon_hash_name(c->fq_name, "class", &L);
            jav_export_t ex; memset(&ex, 0, sizeof ex);
            ex.name.count = L; ex.name.bytes.data = nb; ex.name.bytes.length = (size_t)L;
            ex.kind = 0x03; ex.idx = (uint32_t)wasm_class_singleton_global_index(wt, ci);
            exports[nexp++] = ex;
            for (int lf = 0; lf < (int)bbq_vec_len(c->fields); lf++) {
                if (!(c->fields[lf].modifiers & ACC_STATIC)) continue;
                uint32_t L2; uint8_t* nb2 = canon_hash_name(c->fq_name, c->fields[lf].name, &L2);
                jav_export_t ex2; memset(&ex2, 0, sizeof ex2);
                ex2.name.count = L2; ex2.name.bytes.data = nb2; ex2.name.bytes.length = (size_t)L2;
                ex2.kind = 0x03; ex2.idx = (uint32_t)wasm_global_index(wt, ci, lf);
                exports[nexp++] = ex2;
            }
        }
        if (wt->has_exceptions) {                    /* export the exn tag → plugins import it so a jre
                                                      * throw and a plugin catch share ONE store tag id */
            jav_export_t ext; memset(&ext, 0, sizeof ext);
            uint8_t* nb = (uint8_t*)malloc(3); memcpy(nb, "exn", 3);
            ext.name.count = 3; ext.name.bytes.data = nb; ext.name.bytes.length = 3;
            ext.kind = 0x04; ext.idx = 0;            /* tag export; tagidx 0 (jre's sole defined tag) */
            exports[nexp++] = ext;
        }
    }

    /* The type section: the complete GC rec group as bytes from wasm_types,
     * decoded via the shared reader into the module's type section. */
    jav_type_section_t typesec; memset(&typesec, 0, sizeof typesec);
    if (ok) {
        emit_wasm_ctx tc = {0};
        wasm_types_emit_typesec_content(wt, sctx, &tc);
        bbq_ctx_t trc; bbq_ctx_init(&trc, tc.code, (size_t)bbq_vec_len(tc.code));
        if (!jav_type_section_read(&trc, &typesec)) {
            fprintf(stderr, "wasm_assemble: type section — wasm_types emitted bytes the "
                            "spec grammar rejects (type-layout bug)\n");
            ok = false;
        }
        bbq_ctx_free(&trc);
        bbq_vec_free(tc.code);
    }

    /* The global section: one module global per static field (default-inited),
     * followed by one vtable instance global per class (populated funcrefs),
     * decoded from wasm_types bytes via the shared reader. */
    jav_global_section_t globalsec; memset(&globalsec, 0, sizeof globalsec);
    bool has_globals = ok && wasm_total_global_count(wt) > 0;
    if (has_globals) {
        emit_wasm_ctx gc = {0};
        wasm_types_emit_globals_content(wt, sctx, &gc);
        bbq_ctx_t grc; bbq_ctx_init(&grc, gc.code, (size_t)bbq_vec_len(gc.code));
        if (!jav_global_section_read(&grc, &globalsec)) {
            fprintf(stderr, "wasm_assemble: global section — wasm_types emitted bytes the "
                            "spec grammar rejects (static-field layout bug)\n");
            ok = false; has_globals = false;
        }
        bbq_ctx_free(&grc);
        bbq_vec_free(gc.code);
    }

    /* The import section (id 2): one FUNCTION import per referenced native method,
     * occupying funcidx [0, nimports) (§2.5.1 — imports precede defined functions
     * in the funcidx space). module = declaring class name, field = method name;
     * desc = the method's functype index. The host supplies these at instantiation. */
    jav_import_section_t impsec; memset(&impsec, 0, sizeof impsec);
    jav_import_t* imps = NULL;
    int nimp = wt->nimports;
    int gimp = wasm_imported_global_count(wt);   /* PLUGIN: library static-field + singleton globals; 0 otherwise */
    int timp = (sctx->mode == SEMA_MODE_PLUGIN && wt->has_exceptions) ? 1 : 0;   /* PLUGIN: import jre's "exn" tag */
    int mimp = (sctx->mode == SEMA_MODE_PLUGIN) ? 1 : 0;   /* PLUGIN: import jre's shared I/O staging memory */
    if (ok && (nimp + gimp + timp + mimp) > 0) {
        imps = (jav_import_t*)calloc((size_t)(nimp + gimp + timp + mimp), sizeof *imps);
        for (int i = 0; i < nimp; i++) {
            sema_func_ent_t fe = sema_import_at(sctx, i);
            const sema_class_t*  icls = sema_get_class(sctx, fe.class_id);
            const sema_method_t* im   = &icls->methods[fe.method_id];
            uint8_t *mb, *fbn; int cl, ml;
            if (sctx->mode == SEMA_MODE_PLUGIN && icls->import_pkg >= 0 && icls->ast_node) {
                /* java.lang funcs come from the jre.wasm runtime, resolved by canonical name.
                 * A USER-declared native (import_pkg<0) is NOT jre's — it falls to the host branch. */
                cl = 3; mb = (uint8_t*)malloc(3); memcpy(mb, "jre", 3);
                uint32_t fl; fbn = canon_method_name(sctx, fe.class_id, im, &fl); ml = (int)fl;
            } else {
                /* host natives: module = declaring class, field = method name. */
                cl = (int)strlen(icls->name); mb = (uint8_t*)malloc((size_t)cl); memcpy(mb, icls->name, (size_t)cl);
                ml = (int)strlen(im->name);   fbn = (uint8_t*)malloc((size_t)ml); memcpy(fbn, im->name, (size_t)ml);
            }
            imps[i].module.count = (uint32_t)cl; imps[i].module.bytes.data = mb;  imps[i].module.bytes.length = (size_t)cl;
            imps[i].field.count  = (uint32_t)ml; imps[i].field.bytes.data  = fbn; imps[i].field.bytes.length  = (size_t)ml;
            imps[i].desc.kind = 0x00;                  /* function import */
            imps[i].desc.body.tag = 0;                 /* case_0 = idx_imm (typeidx) */
            imps[i].desc.body.u.case_0.x = (uint32_t)wasm_import_functype_idx(wt, fe.class_id, fe.method_id);
        }
        /* PLUGIN: import the java.lang GLOBALS from jre in the [0, G_imp) authority order —
         * library static fields (var) first, then one Class singleton (const, (ref null Class))
         * per library class. Emitted past the func imports (funcidx/globalidx are per-kind). */
        if (sctx->mode == SEMA_MODE_PLUGIN) {
            int gi = nimp;
            /* Import every SHARED class's globals from jre, in the [0, G_imp) authority order:
             * shared static fields (var) first, then one Class singleton (const) per shared class.
             * SHARED = NOT user-source (import_pkg>=0 OR synthesized) — a predicate, not a ci range,
             * since synthesized shared classes carry higher ids than user-source ones. */
            #define JAV_SHARED(c) (!((c)->import_pkg < 0 && (c)->ast_node))
            for (int ci = 0; ci < wt->num_classes; ci++) {        /* shared static-field globals */
                const sema_class_t* c = sema_get_class(sctx, ci);
                if (!JAV_SHARED(c) || sema_array_class_overlay(sctx, ci) >= 0) continue;  /* array Classes are plugin-local */
                for (int lf = 0; lf < (int)bbq_vec_len(c->fields); lf++) {
                    if (!(c->fields[lf].modifiers & ACC_STATIC)) continue;
                    uint32_t fl; uint8_t* fn = canon_hash_name(c->fq_name, c->fields[lf].name, &fl);
                    uint8_t* mb = (uint8_t*)malloc(3); memcpy(mb, "jre", 3);
                    imps[gi].module.count = 3; imps[gi].module.bytes.data = mb; imps[gi].module.bytes.length = 3;
                    imps[gi].field.count = fl; imps[gi].field.bytes.data = fn; imps[gi].field.bytes.length = (size_t)fl;
                    imps[gi].desc.kind = 0x03; imps[gi].desc.body.tag = 3;
                    imps[gi].desc.body.u.case_3.type = plugin_valtype(wt, sctx, c->fields[lf].type);
                    imps[gi].desc.body.u.case_3.mut  = 0x01;     /* static field = var (matches jre) */
                    gi++;
                }
            }
            java_type_t class_ty = jt_class(sema_class_reflect_id(sctx));
            for (int ci = 0; ci < wt->num_classes; ci++) {        /* shared Class singletons */
                const sema_class_t* c = sema_get_class(sctx, ci);
                if (!JAV_SHARED(c) || sema_array_class_overlay(sctx, ci) >= 0) continue;  /* array Classes are plugin-local */
                uint32_t fl; uint8_t* fn = canon_hash_name(c->fq_name, "class", &fl);
                uint8_t* mb = (uint8_t*)malloc(3); memcpy(mb, "jre", 3);
                imps[gi].module.count = 3; imps[gi].module.bytes.data = mb; imps[gi].module.bytes.length = 3;
                imps[gi].field.count = fl; imps[gi].field.bytes.data = fn; imps[gi].field.bytes.length = (size_t)fl;
                imps[gi].desc.kind = 0x03; imps[gi].desc.body.tag = 3;
                imps[gi].desc.body.u.case_3.type = plugin_valtype(wt, sctx, class_ty);
                imps[gi].desc.body.u.case_3.mut  = 0x00;         /* singleton = const (matches jre) */
                gi++;
            }
            if (wt->has_exceptions) {            /* import jre's "exn" tag → this plugin's tagidx 0, so its
                                                  * throw/catch use jre's store tag id (link_imports §4.2 shares it) */
                uint8_t* mb = (uint8_t*)malloc(3); memcpy(mb, "jre", 3);
                uint8_t* fn = (uint8_t*)malloc(3); memcpy(fn, "exn", 3);
                imps[gi].module.count = 3; imps[gi].module.bytes.data = mb; imps[gi].module.bytes.length = 3;
                imps[gi].field.count  = 3; imps[gi].field.bytes.data  = fn; imps[gi].field.bytes.length  = 3;
                imps[gi].desc.kind = 0x04; imps[gi].desc.body.tag = 4;
                imps[gi].desc.body.u.case_4.attr = 0x00;
                imps[gi].desc.body.u.case_4.type = (uint32_t)wasm_tag_functype_idx(wt);
                gi++;
            }
            {   /* Import jre's I/O staging memory as this plugin's memidx 0 — so a Mem.store8/load8
                 * (i32.store8/load8_u) in shared stdlib OR plugin code targets the ONE shared buffer the
                 * host reads via wasm_memory_data. Byte-matches jre's defined memory (flag 0, min 1). */
                uint8_t* mb = (uint8_t*)malloc(3); memcpy(mb, "jre", 3);
                uint8_t* fn = (uint8_t*)malloc(6); memcpy(fn, "memory", 6);
                imps[gi].module.count = 3; imps[gi].module.bytes.data = mb; imps[gi].module.bytes.length = 3;
                imps[gi].field.count  = 6; imps[gi].field.bytes.data  = fn; imps[gi].field.bytes.length  = 6;
                imps[gi].desc.kind = 0x02; imps[gi].desc.body.tag = 2;
                imps[gi].desc.body.u.case_2.flag = 0; imps[gi].desc.body.u.case_2.min = 1;
                gi++;
            }
            #undef JAV_SHARED
        }
        impsec.count = (uint32_t)(nimp + gimp + timp + mimp);
        impsec.imports.items = imps; impsec.imports.count = (size_t)(nimp + gimp + timp + mimp);
    }

    /* Assemble the module: magic/version + type(1)/[import(2)]/function(3)/
     * [global(6)]/export(7)/[start(8)]/code(10)/[tag(13)] in ascending section
     * order, each a jav_section, then serialize with the one shared writer. */
    jav_module_t mod; memset(&mod, 0, sizeof mod);
    mod.magic = 0x6d736100u; mod.version = 1u;

    jav_section_t secs[10];
    memset(secs, 0, sizeof secs);
    int ns = 0;
    secs[ns].id = 1;  secs[ns].body.tag = 1;  secs[ns].body.u.case_1 = typesec; ns++;
    if (nimp + gimp > 0) {
        secs[ns].id = 2; secs[ns].body.tag = 2; secs[ns].body.u.case_2 = impsec; ns++;
    }
    secs[ns].id = 3;  secs[ns].body.tag = 3;
    secs[ns].body.u.case_3.count = (uint32_t)nfuncs;
    secs[ns].body.u.case_3.type_indices.items = fidxs; secs[ns].body.u.case_3.type_indices.count = (size_t)nfuncs; ns++;
    if (sctx->mode != SEMA_MODE_PLUGIN) {       /* RUNTIME/WHOLE DEFINE the memory; PLUGIN imports jre's */
        /* One linear memory (1 page = 64 KiB) as the I/O staging buffer — the standard GC↔host bridge
         * (§7.1 gives the embedder mem_read/mem_write; a byte[] crosses by copying to/from it). */
        jav_mem_entry_t* iomem = (jav_mem_entry_t*)malloc(sizeof *iomem);   /* jav_module_free frees mems.items */
        memset(iomem, 0, sizeof *iomem);
        iomem->limits.flag = 0; iomem->limits.min = 1;   /* flag 0 = min only, no max */
        secs[ns].id = 5;  secs[ns].body.tag = 5;   /* memory section (§5.5.8) — the I/O staging buffer */
        secs[ns].body.u.case_5.count = 1;
        secs[ns].body.u.case_5.mems.items = iomem; secs[ns].body.u.case_5.mems.count = 1; ns++;
    }
    /* Tag section (id 13): one exception tag → its functype. PLUGIN imports jre's
     * tag instead of defining one, so a cross-boundary throw/catch shares one tag
     * id. Heap-allocated: jav_module_free owns + frees tags.items.
     *
     * §5.5.17: sections "must occur at most once and in the prescribed order",
     * and the module grammar puts tagsec between memsec and globalsec — NOT at
     * the position its id would suggest (§5.5.2 notes ids do not follow the
     * encoding order). Emitting it last, after codesec, produces a module every
     * conforming runtime rejects. */
    if (wt->has_exceptions && sctx->mode != SEMA_MODE_PLUGIN) {
        jav_tag_type_t* exn_tag = (jav_tag_type_t*)malloc(sizeof *exn_tag);
        exn_tag->attr = 0x00;                /* attribute 0 = exception tag */
        exn_tag->type = (uint32_t)wasm_tag_functype_idx(wt);
        secs[ns].id = 13; secs[ns].body.tag = 13;
        secs[ns].body.u.case_13.count = 1;
        secs[ns].body.u.case_13.tags.items = exn_tag; secs[ns].body.u.case_13.tags.count = 1; ns++;
    }
    if (has_globals) {
        secs[ns].id = 6;  secs[ns].body.tag = 6;  secs[ns].body.u.case_6 = globalsec; ns++;
    }
    /* Export the I/O staging memory as "memory" so the embedder reaches its bytes via wasm_memory_data
     * (and a PLUGIN links its memory import to it). Only the module that DEFINES it exports it. */
    if (sctx->mode != SEMA_MODE_PLUGIN) {
        uint8_t* mnb = (uint8_t*)malloc(6); memcpy(mnb, "memory", 6);   /* jav_module_free frees export names */
        jav_export_t ex; memset(&ex, 0, sizeof ex);
        ex.name.count = 6; ex.name.bytes.data = mnb; ex.name.bytes.length = 6;
        ex.kind = 2;   /* export kind 2 = memory */ ex.idx = 0;
        exports[nexp++] = ex;
    }
    secs[ns].id = 7;  secs[ns].body.tag = 7;
    secs[ns].body.u.case_7.count = (uint32_t)nexp;
    secs[ns].body.u.case_7.exports.items = exports;  secs[ns].body.u.case_7.exports.count = (size_t)nexp; ns++;
    if (has_clinit) {                        /* start section → the module initializer (funcidx past imports) */
        secs[ns].id = 8;  secs[ns].body.tag = 8;  secs[ns].body.u.case_8.func = (uint32_t)(wt->nimports + nf); ns++;
    }
    secs[ns].id = 10; secs[ns].body.tag = 10;
    secs[ns].body.u.case_10.count = (uint32_t)nfuncs;
    secs[ns].body.u.case_10.entries.items = entries; secs[ns].body.u.case_10.entries.count = (size_t)nfuncs; ns++;

    jav_section_t* secarr = (jav_section_t*)malloc((size_t)ns * sizeof(jav_section_t));
    memcpy(secarr, secs, (size_t)ns * sizeof(jav_section_t));
    mod.sections.items = secarr; mod.sections.count = (size_t)ns;

    /* §5.5.1 structural audit of the FINISHED module, before it is serialized.
     * The per-body jav_func_body_read above is construction — it decodes the
     * emitted bytes into the entry so the tree can be assembled — not a check.
     * This is the check: the cross-section invariants no single section's
     * grammar can express (section order and duplication, function/code count
     * agreement, datacount/data agreement, the locals limit). We are the only
     * producer of an owning jav_module_t, so if the emitter gets one of these
     * wrong nothing else in the pipeline would catch it before the VM. */
    if (ok) {
        const char* wf_err = NULL;
        if (!jav_module_wf(&mod, &wf_err)) {
            fprintf(stderr, "wasm_assemble: emitted module is malformed: %s\n",
                    wf_err ? wf_err : "(no reason given)");
            ok = false;
        }
    }

    if (ok) {
        bbq_write_ctx_t w;
        bbq_write_ctx_init_growable(&w, 256);
        bbq_write_set_endian(&w, true);
        if (jav_module_write(&w, &mod)) {
            for (size_t i = 0; i < w.pos; i++) ew_byte(out, w.data[i]);
        } else {
            fprintf(stderr, "wasm_assemble: jav_module_write failed\n");
            ok = false;
        }
        bbq_write_ctx_free(&w);
    }

    jav_module_free(&mod);   /* frees sections + all nested (types, names, bodies) */
    return ok;
}
