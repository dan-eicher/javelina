// wast_sexpr.h — the shared, engine-type-free .wast S-expression reader. Included by
// BOTH test_wast.c (owning reader / validation gates) and wast_exec.c (c-lite execution
// runner): the two engine type-worlds (owning jav_types.h vs the c-lite index) cannot
// share a TU, so everything pure lives here. All static — each TU gets its own copy.
#ifndef WAST_SEXPR_H
#define WAST_SEXPR_H
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    int is_list;
    const char *tok; int tlen;       // atom text (if !is_list && !is_str)
    int is_str;                      // string literal (tok/tlen = raw incl quotes)
    const char *s0, *s1;             // raw source span of a list node: '(' .. after ')'
    struct Node **kids; int nkids;
} Node;

typedef struct { const char *p, *e; } Sc;

static void wsx_skip_ws(Sc *s) {
    for (;;) {
        while (s->p < s->e && (*s->p==' '||*s->p=='\t'||*s->p=='\n'||*s->p=='\r')) s->p++;
        if (s->p+1 < s->e && s->p[0]==';' && s->p[1]==';') { while (s->p < s->e && *s->p != '\n') s->p++; continue; }
        if (s->p+1 < s->e && s->p[0]=='(' && s->p[1]==';') {
            int depth = 0;
            while (s->p+1 < s->e) {
                if (s->p[0]=='(' && s->p[1]==';') { depth++; s->p+=2; }
                else if (s->p[0]==';' && s->p[1]==')') { depth--; s->p+=2; if (depth==0) break; }
                else s->p++;
            }
            continue;
        }
        break;
    }
}
static Node *wsx_mk(void) { Node *n = calloc(1, sizeof *n); return n; }
static void free_node(Node *n) {
    if (!n) return;
    for (int i = 0; i < n->nkids; i++) free_node(n->kids[i]);
    free(n->kids); free(n);
}
static Node *parse_value(Sc *s) {
    wsx_skip_ws(s);
    if (s->p >= s->e) return NULL;
    if (*s->p == '"') {
        const char *start = s->p++;
        while (s->p < s->e && *s->p != '"') { if (*s->p=='\\' && s->p+1<s->e) s->p++; s->p++; }
        if (s->p < s->e) s->p++;
        Node *n = wsx_mk(); n->is_str = 1; n->tok = start; n->tlen = (int)(s->p - start);
        return n;
    }
    if (*s->p == '(') {
        const char *lp = s->p; s->p++;
        Node *n = wsx_mk(); n->is_list = 1;
        for (;;) {
            wsx_skip_ws(s);
            if (s->p >= s->e || *s->p == ')') { if (s->p<s->e) s->p++; break; }
            const char *before = s->p;
            Node *k = parse_value(s);
            if (!k || s->p == before) break;
            n->kids = realloc(n->kids, (n->nkids+1)*sizeof(Node*));
            n->kids[n->nkids++] = k;
        }
        n->s0 = lp; n->s1 = s->p;
        return n;
    }
    const char *start = s->p;
    while (s->p < s->e && !strchr(" \t\n\r()\";", *s->p)) s->p++;
    if (s->p == start && s->p < s->e) s->p++;
    Node *n = wsx_mk(); n->tok = start; n->tlen = (int)(s->p - start);
    return n;
}
static int head_is(const Node *n, const char *kw) {
    return n && n->is_list && n->nkids > 0 && !n->kids[0]->is_list && !n->kids[0]->is_str &&
           (int)strlen(kw) == n->kids[0]->tlen && memcmp(n->kids[0]->tok, kw, n->kids[0]->tlen) == 0;
}
static int decode_str(const Node *s, unsigned char *out) {
    int n = 0; const char *p = s->tok+1, *e = s->tok + s->tlen - 1;
    while (p < e) {
        if (*p == '\\' && p+1 < e+1) {
            char c = p[1];
            if      (c=='t'){out[n++]='\t';p+=2;}
            else if (c=='n'){out[n++]='\n';p+=2;}
            else if (c=='r'){out[n++]='\r';p+=2;}
            else if (c=='"'){out[n++]='"'; p+=2;}
            else if (c=='\''){out[n++]='\'';p+=2;}
            else if (c=='\\'){out[n++]='\\';p+=2;}
            else { int hi = p[1], lo = p[2];
                #define WSX_HX(x) ((x)<='9'?(x)-'0':((x)|32)-'a'+10)
                out[n++] = (unsigned char)((WSX_HX(hi)<<4)|WSX_HX(lo)); p+=3; }
        } else out[n++] = (unsigned char)*p++;
    }
    return n;
}
static const Node *module_of(const Node *cmd) {
    if (head_is(cmd, "module")) return cmd;
    for (int i = 1; i < cmd->nkids; i++)
        if (head_is(cmd->kids[i], "module")) return cmd->kids[i];
    return NULL;
}
static int binary_strs_at(const Node *m) {
    for (int i = 1; i < m->nkids; i++) {
        if (m->kids[i]->is_str) return -1;
        if (!m->kids[i]->is_list && m->kids[i]->tlen==6 && memcmp(m->kids[i]->tok,"binary",6)==0) return i+1;
        if (!m->kids[i]->is_list && m->kids[i]->tlen==5 && memcmp(m->kids[i]->tok,"quote",5)==0) return -1;
    }
    return -1;
}
static int is_quote_module(const Node *m) {
    for (int i = 1; i < m->nkids; i++) {
        if (m->kids[i]->is_str) return 0;
        if (!m->kids[i]->is_list && m->kids[i]->tlen==5 && memcmp(m->kids[i]->tok,"quote",5)==0) return 1;
        if (!m->kids[i]->is_list && m->kids[i]->tlen==6 && memcmp(m->kids[i]->tok,"binary",6)==0) return 0;
    }
    return 0;
}
static const char *out_of_scope(const Node *m) {
    for (int i = 1; i < m->nkids; i++) {
        const Node *k = m->kids[i];
        if (!k->is_list && !k->is_str &&
            ((k->tlen == 10 && memcmp(k->tok, "definition", 10) == 0) ||
             (k->tlen == 8  && memcmp(k->tok, "instance",   8) == 0)))
            return "module-linking proposal (definition/instance)";
        /* §6.2.5 custom annotations `(@…)` are now SKIPPED by the wat reader (javelina's frame
         * copy's structured skip) — no longer excluded; they flow through and parse/reject. */
    }
    return NULL;
}
static int tok_is(const Node *n, const char *s) {
    int l = (int)strlen(s); return n && !n->is_list && !n->is_str && n->tlen == l && memcmp(n->tok, s, l) == 0;
}

#endif // WAST_SEXPR_H
