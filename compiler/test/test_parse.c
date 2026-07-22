// test_parse.c — Section 1 (parser): Java source -> AST.
// Plain-C, exit-code based (javelina wasm/test convention). Enumerated coverage
// of the grammar surface, construct family by family — conformance CONFIRMS the
// faithful Java.peg/Java.asdl port, it does not discover it. Assertions read the
// generated ast_* tagged unions directly.
#include "java_parser.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <string.h>

#include "javelina_test.h"

static ast_program_t* do_parse(const char* src) {
    java_parse_ctx_t pctx;
    bbq_arena_init(&pctx.arena, 4096);
    pctx.result = NULL;
    pctx.file = NULL;
    peg_state p;
    java_parser_init(&p, src, (int)strlen(src));
    p.user_data = &pctx;
    if (!java_parser_parse(&p)) return NULL;
    return pctx.result;
}

// Expect a source to FAIL parsing (negative test).
static int parse_fails(const char* src) { return do_parse(src) == NULL; }

static ast_type_decl_t* one_class(const char* src, const char* what) {
    ast_program_t* prog = do_parse(src);
    if (!prog) { printf("  FAIL  %s: parse returned NULL\n", what); TEST_FAILED(); return NULL; }
    if (prog->types_count < 1) { printf("  FAIL  %s: no types\n", what); TEST_FAILED(); return NULL; }
    return prog->types[0];
}

static ast_member_t* member(ast_type_decl_t* c, int i) {
    if (!c || c->tag != AST_CLASSDECL || i >= c->class_decl.members_count) return NULL;
    return c->class_decl.members[i];
}

// Parse `class T { <ret> f() { <body> } }` and return f's body block.
static ast_stmt_t* method_body(const char* ret, const char* body, const char* what) {
    char src[1024];
    snprintf(src, sizeof(src), "class T { %s f() { %s } }", ret, body);
    ast_type_decl_t* c = one_class(src, what);
    ast_member_t* m = member(c, 0);
    if (!m || m->tag != AST_METHODDECL || !m->method_decl.body) {
        printf("  FAIL  %s: no method body\n", what); TEST_FAILED(); return NULL;
    }
    return m->method_decl.body;
}

// Parse `return <expr>;` inside a method and return the expression.
static ast_expr_t* expr_of(const char* e, const char* what) {
    char body[512];
    snprintf(body, sizeof(body), "return %s;", e);
    ast_stmt_t* blk = method_body("int", body, what);
    if (!blk || blk->block.stmts_count < 1) return NULL;
    ast_stmt_t* s = blk->block.stmts[0];
    if (s->tag != AST_RETURN || !s->return_.value) { printf("  FAIL  %s: not a return-expr\n", what); TEST_FAILED(); return NULL; }
    return s->return_.value;
}

// ── Declarations ────────────────────────────────────────────────────────────
static void t_declarations(void) {
    ast_program_t* p = do_parse(
        "import java.util.Foo;\n"
        "import java.lang.*;\n"
        "public abstract class T extends B implements I, J {\n"
        "  public static final int N = 5;\n"
        "  protected int a, b;\n"
        "  private byte[] buf;\n"
        "  T(int x) { this.a = x; }\n"
        "  public abstract int m(int x);\n"
        "  int g() throws E { return 0; }\n"
        "}\n"
        "interface I extends K { int c(); }\n");
    CHECK(p != NULL, "decls: parses");
    if (!p) return;
    CHECK(p->imports_count == 2, "two imports");
    CHECK(p->imports_count == 2 && p->imports[0]->tag == AST_SINGLEIMPORT, "single import");
    CHECK(p->imports_count == 2 && p->imports[1]->tag == AST_WILDCARDIMPORT, "wildcard import");
    CHECK(p->types_count == 2, "class + interface");
    ast_type_decl_t* c = p->types[0];
    CHECK(c->tag == AST_CLASSDECL, "T is class");
    CHECK(c->tag == AST_CLASSDECL && c->class_decl.super_class != NULL, "extends B");
    CHECK(c->tag == AST_CLASSDECL && c->class_decl.interfaces_count == 2, "implements I,J");
    CHECK(c->tag == AST_CLASSDECL && c->class_decl.mods_count == 2, "public abstract mods");
    CHECK(c->tag == AST_CLASSDECL && c->class_decl.members_count == 6, "6 members");
    ast_member_t* f0 = member(c, 0);
    CHECK(f0 && f0->tag == AST_FIELDDECL && f0->field_decl.mods_count == 3, "N: public static final");
    ast_member_t* f1 = member(c, 1);
    CHECK(f1 && f1->tag == AST_FIELDDECL && f1->field_decl.decls_count == 2, "int a, b -> 2 decls");
    ast_member_t* f2 = member(c, 2);
    CHECK(f2 && f2->tag == AST_FIELDDECL && f2->field_decl.ty->tag == AST_ARRAYTYPE, "byte[] buf");
    ast_member_t* ctor = member(c, 3);
    CHECK(ctor && ctor->tag == AST_CONSTRUCTORDECL, "constructor");
    ast_member_t* am = member(c, 4);
    CHECK(am && am->tag == AST_METHODDECL && am->method_decl.body == NULL, "abstract method has no body");
    ast_member_t* gm = member(c, 5);
    CHECK(gm && gm->tag == AST_METHODDECL && gm->method_decl.throws__count == 1, "g throws E");
    CHECK(p->types[1]->tag == AST_INTERFACEDECL, "I is interface");
    CHECK(p->types[1]->tag == AST_INTERFACEDECL && p->types[1]->interface_decl.extends__count == 1, "interface extends K");
}

// ── Primitive + array types ─────────────────────────────────────────────────
static void t_types(void) {
    struct { const char* kw; ast_type_t_tag tag; } prims[] = {
        {"byte", AST_BYTETYPE}, {"short", AST_SHORTTYPE}, {"int", AST_INTTYPE},
        {"boolean", AST_BOOLTYPE},
    };
    for (size_t i = 0; i < sizeof(prims)/sizeof(prims[0]); i++) {
        char src[128];
        snprintf(src, sizeof(src), "class T { %s x; }", prims[i].kw);
        ast_type_decl_t* c = one_class(src, prims[i].kw);
        ast_member_t* m = member(c, 0);
        CHECK(m && m->tag == AST_FIELDDECL && m->field_decl.ty->tag == prims[i].tag, prims[i].kw);
    }
    // void return type
    ast_stmt_t* b = method_body("void", "return;", "void-method");
    CHECK(b != NULL, "void method parses");
    // class type, qualified
    ast_type_decl_t* c = one_class("class T { java.lang.Object o; }", "qualified-class-type");
    ast_member_t* m = member(c, 0);
    CHECK(m && m->field_decl.ty->tag == AST_CLASSTYPE, "qualified class type");
}

// ── Statements ──────────────────────────────────────────────────────────────
static void t_statements(void) {
    ast_stmt_t* b = method_body("void",
        "int i = 0;"
        "if (i == 0) i = 1; else i = 2;"
        "while (i < 10) i = i + 1;"
        "do { i = i - 1; } while (i > 0);"
        "for (int j = 0; j < 3; j = j + 1) { i = i + j; }"
        "switch (i) { case 0: break; case 1: i = 9; break; default: i = -1; }"
        "try { i = 1; } catch (E e) { i = 2; } finally { i = 3; }"
        "lbl: while (true) { break lbl; }"
        "return;",
        "statements");
    if (!b) return;
    ast_stmt_t** s = b->block.stmts;
    int n = b->block.stmts_count;
    CHECK(n == 9, "nine statements");
    if (n != 9) return;
    CHECK(s[0]->tag == AST_LOCALVARDECL, "local var decl");
    CHECK(s[1]->tag == AST_IF && s[1]->if_.else_ != NULL, "if/else");
    CHECK(s[2]->tag == AST_WHILE, "while");
    CHECK(s[3]->tag == AST_DOWHILE, "do/while");
    CHECK(s[4]->tag == AST_FOR && s[4]->for_.init != NULL && s[4]->for_.update_count == 1, "for");
    CHECK(s[5]->tag == AST_SWITCH && s[5]->switch_.cases_count == 3, "switch with 3 cases");
    CHECK(s[6]->tag == AST_TRY && s[6]->try_.catches_count == 1 && s[6]->try_.finally_ != NULL, "try/catch/finally");
    CHECK(s[7]->tag == AST_LABELED, "labeled stmt");
    CHECK(s[7]->tag == AST_LABELED && s[7]->labeled.body->tag == AST_WHILE, "labeled wraps while");
    CHECK(s[8]->tag == AST_RETURN, "return");
    // break-with-label nested in the labeled while
    ast_stmt_t* lw = s[7]->labeled.body;            // while
    ast_stmt_t* br = lw->while_.body->block.stmts[0];
    CHECK(br->tag == AST_BREAK && br->break_.label != NULL, "break with label");
}

// ── Expression operators + precedence/associativity ─────────────────────────
static void t_operators(void) {
    // Precedence: + below *, so a + b * c == Add(a, Mul(b,c))
    ast_expr_t* e = expr_of("a + b * c", "prec-mul-over-add");
    CHECK(e && e->tag == AST_BINARY && e->binary.op == AST_ADD, "top Add");
    CHECK(e && e->tag == AST_BINARY && e->binary.rhs->tag == AST_BINARY && e->binary.rhs->binary.op == AST_MUL, "Mul nested under Add");
    // Left-assoc: a - b - c == Sub(Sub(a,b),c)
    e = expr_of("a - b - c", "sub-left-assoc");
    CHECK(e && e->binary.op == AST_SUB && e->binary.lhs->tag == AST_BINARY && e->binary.lhs->binary.op == AST_SUB, "subtraction left-assoc");
    // Shift below add: a + b << c == Shl(Add(a,b), c)
    e = expr_of("a + b << c", "shift-below-add");
    CHECK(e && e->tag == AST_BINARY && e->binary.op == AST_SHL, "shift lowest of the three");
    // Relational/equality/logical chain: a < b == c  -> Eq(Lt(a,b), c)
    e = expr_of("a < b == c", "rel-below-eq");
    CHECK(e && e->binary.op == AST_EQ && e->binary.lhs->tag == AST_BINARY && e->binary.lhs->binary.op == AST_LT, "== below <");
    // && below ||, both below ?:  a || b && c  -> Or(a, And(b,c))
    e = expr_of("a || b && c", "and-below-or");
    CHECK(e && e->binary.op == AST_OR && e->binary.rhs->binary.op == AST_AND, "&& binds tighter than ||");
    // Ternary, right-assoc:  a ? b : c ? d : e -> Ternary(a,b,Ternary(c,d,e))
    e = expr_of("a > 0 ? 1 : b > 0 ? 2 : 3", "ternary-right-assoc");
    CHECK(e && e->tag == AST_TERNARY, "ternary");
    CHECK(e && e->tag == AST_TERNARY && e->ternary.else_->tag == AST_TERNARY, "ternary right-assoc");
    // Bitwise precedence: a | b & c -> Or-bits(a, And-bits(b,c))
    e = expr_of("a | b & c", "bitand-below-bitor");
    CHECK(e && e->binary.op == AST_BITOR && e->binary.rhs->binary.op == AST_BITAND, "& binds tighter than |");
    // instanceof
    e = expr_of("a instanceof T", "instanceof");
    CHECK(e && e->tag == AST_INSTANCEOF, "instanceof expr");
    // Unary forms
    CHECK((e = expr_of("-a", "neg")) && e->tag == AST_UNARY && e->unary.op == AST_NEG, "unary neg");
    CHECK((e = expr_of("!a", "lognot")) && e->tag == AST_UNARY && e->unary.op == AST_LOGNOT, "logical not");
    CHECK((e = expr_of("~a", "bitnot")) && e->tag == AST_UNARY && e->unary.op == AST_BITNOT, "bitwise not");
    CHECK((e = expr_of("++a", "preinc")) && e->tag == AST_UNARY && e->unary.op == AST_PREINC, "pre-inc");
    CHECK((e = expr_of("a--", "postdec")) && e->tag == AST_UNARY && e->unary.op == AST_POSTDEC, "post-dec");
    // Cast
    CHECK((e = expr_of("(short) a", "cast")) && e->tag == AST_CAST, "cast expr");
}

// ── Primary expressions ─────────────────────────────────────────────────────
static void t_primaries(void) {
    ast_expr_t* e;
    CHECK((e = expr_of("42", "int-lit")) && e->tag == AST_INTLIT && e->int_lit.value == 42, "int literal");
    CHECK((e = expr_of("0x1F", "hex-lit")) && e->tag == AST_INTLIT && e->int_lit.value == 31, "hex literal");
    CHECK((e = expr_of("true", "bool-lit")) && e->tag == AST_BOOLLIT && e->bool_lit.value, "bool literal");
    CHECK((e = expr_of("null", "null-lit")) && e->tag == AST_NULLLIT, "null literal");
    CHECK((e = expr_of("x", "ident")) && e->tag == AST_IDENT, "ident");
    CHECK((e = expr_of("o.field", "field-access")) && e->tag == AST_FIELDACCESS, "field access");
    CHECK((e = expr_of("a[i]", "array-access")) && e->tag == AST_ARRAYACCESS, "array access");
    CHECK((e = expr_of("g(1, 2)", "call")) && e->tag == AST_METHODCALL && e->method_call.args_count == 2, "method call");
    CHECK((e = expr_of("o.m(x)", "method-call-recv")) && e->tag == AST_METHODCALL && e->method_call.obj != NULL, "receiver method call");
    CHECK((e = expr_of("new T(1)", "new")) && e->tag == AST_NEW, "new object");
    CHECK((e = expr_of("new int[n]", "new-array")) && e->tag == AST_NEWARRAY, "new array");
    CHECK((e = expr_of("this", "this")) && e->tag == AST_THIS, "this");
    // assignment + compound assignment as statement-expressions
    ast_stmt_t* b = method_body("void", "int x = 0; x = 1; x += 2;", "assigns");
    if (b && b->block.stmts_count == 3) {
        CHECK(b->block.stmts[1]->expr_stmt.e->tag == AST_ASSIGN, "plain assign");
        CHECK(b->block.stmts[2]->expr_stmt.e->tag == AST_COMPOUNDASSIGN, "compound assign");
    } else CHECK(0, "assign statements parse");
}

// ── Full Java 1.0 types + modifiers (the extension over the JC subset) ──────
static void t_java10_types(void) {
    struct { const char* kw; ast_type_t_tag tag; } prims[] = {
        {"long", AST_LONGTYPE}, {"char", AST_CHARTYPE},
        {"float", AST_FLOATTYPE}, {"double", AST_DOUBLETYPE},
    };
    for (size_t i = 0; i < sizeof(prims)/sizeof(prims[0]); i++) {
        char src[128];
        snprintf(src, sizeof(src), "class T { %s x; }", prims[i].kw);
        ast_type_decl_t* c = one_class(src, prims[i].kw);
        ast_member_t* m = member(c, 0);
        CHECK(m && m->tag == AST_FIELDDECL && m->field_decl.ty->tag == prims[i].tag, prims[i].kw);
    }
    // char is its OWN type now, not aliased to short.
    ast_type_decl_t* cc = one_class("class T { char c; short s; }", "char-not-short");
    CHECK(member(cc,0) && member(cc,0)->field_decl.ty->tag == AST_CHARTYPE, "char is CharType");
    CHECK(member(cc,1) && member(cc,1)->field_decl.ty->tag == AST_SHORTTYPE, "short still ShortType");
    // multi-dimensional array: int[][] -> ArrayType(ArrayType(IntType))
    ast_type_decl_t* ca = one_class("class T { int[][] g; }", "multidim-array");
    ast_member_t* ma = member(ca, 0);
    CHECK(ma && ma->field_decl.ty->tag == AST_ARRAYTYPE, "int[][] is array");
    if (ma && ma->field_decl.ty->tag == AST_ARRAYTYPE) {
        ast_type_t* inner = ma->field_decl.ty->array_type.element;
        CHECK(inner->tag == AST_ARRAYTYPE, "outer array of array");
        CHECK(inner->tag == AST_ARRAYTYPE && inner->array_type.element->tag == AST_INTTYPE, "element is int");
    }
}

static void t_java10_modifiers(void) {
    ast_type_decl_t* c = one_class(
        "class T { transient int x; volatile int y; synchronized void f() {} native int g(); }",
        "java10-modifiers");
    if (!c) return;
    ast_member_t* mx = member(c, 0);
    CHECK(mx && mx->tag == AST_FIELDDECL && mx->field_decl.mods_count == 1
              && mx->field_decl.mods[0] == AST_TRANSIENT, "transient field");
    ast_member_t* my = member(c, 1);
    CHECK(my && my->field_decl.mods[0] == AST_VOLATILE, "volatile field");
    ast_member_t* mf = member(c, 2);
    CHECK(mf && mf->tag == AST_METHODDECL && mf->method_decl.mods_count == 1
              && mf->method_decl.mods[0] == AST_SYNCHRONIZED, "synchronized method (parses; sema rejects later)");
    ast_member_t* mg = member(c, 3);
    CHECK(mg && mg->tag == AST_METHODDECL && mg->method_decl.mods[0] == AST_NATIVE, "native method");
    // long/float/double now reserved -> can't be used as identifiers
    CHECK(parse_fails("class T { int long; }"), "'long' reserved (not an identifier)");
}

// ── Java 1.0 literals (JLS §3.10: long/float/double/char) ───────────────────
static int near(double a, double b) { double d = a - b; return d < 1e-9 && d > -1e-9; }

static void t_java10_literals(void) {
    ast_expr_t* e;
    // plain int still IntLit
    CHECK((e = expr_of("42", "int")) && e->tag == AST_INTLIT && e->int_lit.value == 42, "42 is IntLit");
    // long literals (l/L suffix), decimal + hex
    CHECK((e = expr_of("100L", "long-dec")) && e->tag == AST_LONGLIT && e->long_lit.value == 100, "100L is LongLit");
    CHECK((e = expr_of("0xFFl", "long-hex")) && e->tag == AST_LONGLIT && e->long_lit.value == 255, "0xFFl is LongLit 255");
    // double literals (no suffix, d/D suffix, exponent, leading dot)
    CHECK((e = expr_of("1.5", "double")) && e->tag == AST_DOUBLELIT && near(e->double_lit.value, 1.5), "1.5 is DoubleLit");
    CHECK((e = expr_of("1.5d", "double-d")) && e->tag == AST_DOUBLELIT && near(e->double_lit.value, 1.5), "1.5d is DoubleLit");
    CHECK((e = expr_of(".5", "double-leading-dot")) && e->tag == AST_DOUBLELIT && near(e->double_lit.value, 0.5), ".5 is DoubleLit");
    CHECK((e = expr_of("1e3", "double-exp")) && e->tag == AST_DOUBLELIT && near(e->double_lit.value, 1000.0), "1e3 is DoubleLit");
    CHECK((e = expr_of("2.0e-3", "double-exp-neg")) && e->tag == AST_DOUBLELIT && near(e->double_lit.value, 0.002), "2.0e-3 is DoubleLit");
    // float literals (f/F suffix)
    CHECK((e = expr_of("1.5f", "float")) && e->tag == AST_FLOATLIT && near(e->float_lit.value, 1.5), "1.5f is FloatLit");
    CHECK((e = expr_of("3F", "float-int")) && e->tag == AST_FLOATLIT && near(e->float_lit.value, 3.0), "3F is FloatLit");
    // char literals: plain, escape, octal escape (\101 == 'A' == 65)
    CHECK((e = expr_of("'a'", "char")) && e->tag == AST_CHARLIT && e->char_lit.value == 97, "'a' is CharLit 97");
    CHECK((e = expr_of("'\\n'", "char-nl")) && e->tag == AST_CHARLIT && e->char_lit.value == 10, "'\\n' is CharLit 10");
    CHECK((e = expr_of("'\\101'", "char-octal")) && e->tag == AST_CHARLIT && e->char_lit.value == 65, "'\\101' octal is CharLit 65");
    // a long-typed local initialized by a long literal, end to end
    ast_stmt_t* b = method_body("void", "long x = 9999999999L;", "long-local");
    CHECK(b && b->block.stmts_count == 1 && b->block.stmts[0]->tag == AST_LOCALVARDECL, "long local var decl");
    if (b && b->block.stmts_count == 1) {
        ast_var_decl_t* vd = b->block.stmts[0]->local_var_decl.decls[0];
        CHECK(vd->init && vd->init->tag == AST_LONGLIT && vd->init->long_lit.value == 9999999999LL, "long literal exceeds int range");
    }
}

// ── Package declaration (captured, not discarded) ───────────────────────────
static void t_package(void) {
    ast_program_t* p = do_parse("package com.example.app; class T {}");
    CHECK(p != NULL, "package: parses");
    CHECK(p && p->package_ != NULL, "package name captured");
    CHECK(p && p->package_ && p->package_->tag == AST_QUALIFIEDNAME, "package is a qualified name");
    CHECK(p && p->package_ && p->package_->tag == AST_QUALIFIEDNAME
            && strcmp(p->package_->qualified_name.id, "app") == 0, "package tail is 'app'");
    ast_program_t* q = do_parse("class T {}");
    CHECK(q && q->package_ == NULL, "no package decl -> NULL");
}

// ── Negative cases (JLS rejections the grammar enforces) ────────────────────
static void t_negatives(void) {
    CHECK(parse_fails("class T { void f() { 5 + 3; } }"), "non-statement-expression rejected");
    CHECK(parse_fails("class T {"), "unterminated class rejected");
    CHECK(parse_fails("class T { int f() return 0; }"), "missing method braces rejected");
}

int main(void) {
    t_declarations();
    t_types();
    t_statements();
    t_operators();
    t_primaries();
    t_java10_types();
    t_java10_modifiers();
    t_java10_literals();
    t_package();
    t_negatives();
    return TEST_SUMMARY("test_parse");
}
