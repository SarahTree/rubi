#if _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "parser.h"
#include "expr.h"
#include "codegen.h"

void* jit_buf;
size_t jit_sz;

int npc;
static int main_address, mainFunc;

struct {
    Variable var[0xFF];
    int count;
    int data[0xFF];
} gblVar;

struct {
    Variable var[0xFF][0xFF]; // var[ "functions.now" ] [ each variable ]
    int count, size[0xFF];
} locVar;

// strings embedded in native code
struct {
    char *text[0xff];
    int *addr;
    int count;
} strings;

struct {
    func_t func[0xff];
    int count, inside, now;
} functions;

#define NON 0

void jit_init() {
    dynasm_init_jit();
}

void* jit_finalize() {
    dynasm_link_jit(&jit_sz);
#ifdef _WIN32
    jit_buf = VirtualAlloc(0, jit_sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    jit_buf = mmap(0, jit_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    dynasm_encode_jit(jit_buf);
#ifdef _WIN32
    {DWORD dwOld; VirtualProtect(jit_buf, jit_sz, PAGE_EXECUTE_READWRITE, &dwOld); }
#else
    mprotect(jit_buf, jit_sz, PROT_READ | PROT_WRITE | PROT_EXEC);
#endif
    return jit_buf;
}

char* getString()
{
    strings.text[ strings.count ] =
        calloc(sizeof(char), strlen(tok.tok[tok.pos].val) + 1);
    strcpy(strings.text[strings.count], tok.tok[tok.pos++].val);
    return strings.text[strings.count++];
}

Variable *getVar(char *name)
{
    /* local variable */
    for (int i = 0; i < locVar.count; i++) {
        if (!strcmp(name, locVar.var[functions.now][i].name))
            return &locVar.var[functions.now][i];
    }
    /* global variable */
    for (int i = 0; i < gblVar.count; i++) {
        if (!strcmp(name, gblVar.var[i].name))
            return &gblVar.var[i];
    }
    return NULL;
}

static Variable *appendVar(char *name, int type)
{
    if (functions.inside == IN_FUNC) {
        int32_t sz = 1 + ++locVar.size[functions.now];
        strcpy(locVar.var[functions.now][locVar.count].name, name);
        locVar.var[functions.now][locVar.count].type = type;
        locVar.var[functions.now][locVar.count].id = sz;
        locVar.var[functions.now][locVar.count].loctype = V_LOCAL;

        return &locVar.var[functions.now][locVar.count++];
    } else if (functions.inside == IN_GLOBAL) {
        /* global varibale */
        strcpy(gblVar.var[gblVar.count].name, name);
        gblVar.var[gblVar.count].type = type;
        gblVar.var[gblVar.count].loctype = V_GLOBAL;
        gblVar.var[gblVar.count].id = (int)&gblVar.data[gblVar.count];
        return &gblVar.var[gblVar.count++];
    }
    return NULL;
}

func_t *getFunc(char *name)
{
    for (int i = 0; i < functions.count; i++) {
        if (!strcmp(functions.func[i].name, name))
            return &functions.func[i];
    }
    return NULL;
}

static func_t *appendFunc(char *name, int address, int espBgn, int args)
{
    functions.func[functions.count].address = address;
    functions.func[functions.count].espBgn = espBgn;
    functions.func[functions.count].args = args;
    strcpy(functions.func[functions.count].name, name);
    return &functions.func[functions.count++];
}

static int32_t make_break()
{
    uint32_t lbl = npc++;
    dynasm_growpc_jit(npc);
    d_make_break(lbl);
    brks.addr = realloc(brks.addr, 4 * (brks.count + 1));
    brks.addr[brks.count] = lbl;
    return brks.count++;
}

static int32_t make_return()
{
    compExpr(); /* get argument */

    int lbl = npc++;
    dynasm_growpc_jit(npc);

    d_make_return(lbl);

    rets.addr = realloc(rets.addr, 4 * (rets.count + 1));
    if (rets.addr == NULL) error("no enough memory");
    rets.addr[rets.count] = lbl;
    return rets.count++;
}

int32_t skip(char *s)
{
    if (!strcmp(s, tok.tok[tok.pos].val)) {
        tok.pos++;
        return 1;
    }
    return 0;
}

int32_t error(char *errs, ...)
{
    va_list args;
    va_start(args, errs);
    printf("error: ");
    vprintf(errs, args);
    puts("");
    va_end(args);
    exit(0);
    return 0;
}

static int eval(int pos, int status)
{
    while (tok.pos < tok.size)
        if (expression(pos, status)) return 1;
    return 0;
}

static Variable *declareVariable()
{
    int32_t npos = tok.pos;
    if (isalpha(tok.tok[tok.pos].val[0])) {
        tok.pos++;
        if (skip(":")) {
            if (skip("int")) {
                --tok.pos;
                return appendVar(tok.tok[npos].val, T_INT);
            }
            if (skip("string")) {
                --tok.pos;
                return appendVar(tok.tok[npos].val, T_STRING);
            }
            if (skip("double")) {
                --tok.pos;
                return appendVar(tok.tok[npos].val, T_DOUBLE);
            }
        } else {
            --tok.pos;
            return appendVar(tok.tok[npos].val, T_INT);
        }
    } else error("%d: can't declare variable", tok.tok[tok.pos].nline);
    return NULL;
}

static int ifStmt()
{
    compExpr(); /* if condition */
    uint32_t end = npc++;
    dynasm_growpc_jit(npc);
    // didn't simply 'jz =>end' to prevent address diff too large
    d_make_ifStmt(end);
    return eval(end, NON);
}

static int whileStmt()
{
    uint32_t loopBgn = npc++;
    dynasm_growpc_jit(npc);
    d_whileStmt_1(loopBgn);
    compExpr(); /* condition */
    uint32_t stepBgn[2], stepOn = 0;
    if (skip(",")) {
        stepOn = 1;
        stepBgn[0] = tok.pos;
        for (; tok.tok[tok.pos].val[0] != ';'; tok.pos++)
            /* next */;
    }
    uint32_t end = npc++;
    dynasm_growpc_jit(npc);
    d_whileStmt_2(end);

    if (skip(":")) expression(0, BLOCK_LOOP);
    else eval(0, BLOCK_LOOP);

    if (stepOn) {
        stepBgn[1] = tok.pos;
        tok.pos = stepBgn[0];
        if (isassign()) assignment();
        tok.pos = stepBgn[1];
    }
    d_whileStmt_3(loopBgn,end);
    
    for (--brks.count; brks.count >= 0; brks.count--)
        d_whileStmt_4(brks.addr[brks.count]);

    brks.count = 0;

    return 0;
}

static int32_t functionStmt()
{
    int32_t argsc = 0;
    char *funcName = tok.tok[tok.pos++].val;
    functions.now++; functions.inside = IN_FUNC;
    if (skip("(")) {
        do {
            declareVariable();
            tok.pos++;
            argsc++;
        } while(skip(","));
        if (!skip(")"))
                error("%d: expecting ')'", tok.tok[tok.pos].nline);
    }
    int func_addr = npc++;
    dynasm_growpc_jit(npc);

    int func_esp = npc++;
    dynasm_growpc_jit(npc);

    appendFunc(funcName, func_addr, func_esp, argsc); // append function

    d_functionStmt_1(func_addr,func_esp);

    d_functionStmt_2(argsc);

    eval(0, BLOCK_FUNC);

    for (--rets.count; rets.count >= 0; --rets.count) {
        d_functionStmt_3(rets.addr[rets.count]);
    }
    rets.count = 0;

    d_functionStmt_4();

    return 0;
}

int expression(int pos, int status)
{
    int isputs = 0;

    if (skip("$")) { // global varibale?
        if (isassign()) assignment();
    } else if (skip("def")) {
        functionStmt();
    } else if (functions.inside == IN_GLOBAL &&
               strcmp("def", tok.tok[tok.pos+1].val) &&
               strcmp("$", tok.tok[tok.pos+1].val) &&
               (tok.pos+1 == tok.size || strcmp(";", tok.tok[tok.pos+1].val))) { // main function entry
        functions.inside = IN_FUNC;
        mainFunc = ++functions.now;

        int main_esp = npc++;
        dynasm_growpc_jit(npc);

        appendFunc("main", main_address, main_esp, 0); // append function

        d_expression_1(main_address,main_esp);

        eval(0, NON);
        d_expression_2();

        functions.inside = IN_GLOBAL;
    } else if (isassign()) {
        assignment();
    } else if ((isputs = skip("puts")) || skip("output")) {
        do {
            int isstring = 0;
            if (skip("\"")) {
                char *str=getString();
                d_expression_3(str);
                isstring = 1;
            } else {
                compExpr();
            }
            d_expression_4();

            if (isstring) {
                d_expression_5();
            } else {
                d_expression_6();
            }
            d_expression_7();

        } while (skip(","));
        /* new line ? */
        if (isputs) {
            d_expression_8();
        }
    } else if(skip("printf")) {
        // support maximum 5 arguments for now
        if (skip("\"")) {
            char *str=getString();
            d_expression_9(str);
        }
        if (skip(",")) {
            uint32_t a = 4;
            do {
                compExpr();
                d_expression_10(a);
                a += 4;
            } while(skip(","));
        }
        d_expression_16();
    } else if (skip("for")) {
        assignment();
        if (!skip(","))
            error("%d: expecting ','", tok.tok[tok.pos].nline);
        whileStmt();
    } else if (skip("while")) {
        whileStmt();
    } else if(skip("return")) {
        make_return();
    } else if(skip("if")) {
        ifStmt();
    } else if(skip("else")) {
        int32_t end = npc++;
        dynasm_growpc_jit(npc);
        d_expression_11(pos,end);
        eval(end, NON);
        return 1;
    } else if (skip("elsif")) {
        int32_t endif = npc++;
        dynasm_growpc_jit(npc);
        d_expression_12(pos,endif);
        compExpr(); /* if condition */
        uint32_t end = npc++;
        dynasm_growpc_jit(npc);
        d_expression_13(end);
        eval(end, NON);
        d_expression_15(endif);
        return 1;
    } else if (skip("break")) {
        make_break();
    } else if (skip("end")) {
        if (status == NON) {
            d_expression_14(pos);
        } else if (status == BLOCK_FUNC) functions.inside = IN_GLOBAL;
        return 1;
    } else if (!skip(";")) { compExpr(); }

    return 0;
}

static char *replaceEscape(char *str)
{
    char escape[12][3] = {
        "\\a", "\a", "\\r", "\r", "\\f", "\f",
        "\\n", "\n", "\\t", "\t", "\\b", "\b"
    };
    for (int32_t i = 0; i < 12; i += 2) {
        char *pos;
        while ((pos = strstr(str, escape[i])) != NULL) {
            *pos = escape[i + 1][0];
            memmove(pos + 1, pos + 2, strlen(pos + 2) + 1);
        }
    }
    return str;
}

int (*parser())(int *, void **)
{
    jit_init();

    tok.pos = 0;
    strings.addr = calloc(0xFF, sizeof(int32_t));
    main_address = npc++;
    dynasm_growpc_jit(npc);
    d_parser_1(main_address);

    eval(0, 0);

    for (int i = 0; i < strings.count; ++i)
        replaceEscape(strings.text[i]);

    uint8_t* buf = (uint8_t*)jit_finalize();

    for (int i = 0; i < functions.count; i++)
        *(int*)(buf + dasm_getpclabel_jit(functions.func[i].espBgn) - 4) = (locVar.size[i+1] + 6)*4;

    dasm_free_jit();
    return ((int (*)(int *, void **))dynasm_get_start_entry());
}

int32_t isassign()
{
    char *val = tok.tok[tok.pos + 1].val;
    if (!strcmp(val, "=") || !strcmp(val, "++") || !strcmp(val, "--")) return 1;
    if (!strcmp(val, "[")) {
        int32_t i = tok.pos + 2, t = 1;
        while (t) {
            val = tok.tok[i].val;
            if (!strcmp(val, "[")) t++; if (!strcmp(val, "]")) t--;
            if (!strcmp(val, ";"))
                error("%d: invalid expression", tok.tok[tok.pos].nline);
            i++;
        }
        if (!strcmp(tok.tok[i].val, "=")) return 1;
    } else if (!strcmp(val, ":") && !strcmp(tok.tok[tok.pos + 3].val, "=")) {
        return 1;
    }
    return 0;
}

int32_t assignment()
{
    Variable *v = getVar(tok.tok[tok.pos].val);
    int32_t inc = 0, dec = 0, declare = 0;
    if (v == NULL) {
        declare++;
        v = declareVariable();
    }
    tok.pos++;

    int siz = (v->type == T_INT ? sizeof(int32_t) :
               v->type == T_STRING ? sizeof(int32_t *) :
               v->type == T_DOUBLE ? sizeof(double) : 4);

    if (v->loctype == V_LOCAL) {
        if (skip("[")) { // Array?
            compExpr();
            d_assignment_1();
            if (skip("]") && skip("=")) {
                compExpr();
                d_assignment_2(siz,v->id);
                if (v->type == T_INT) {
                    d_assignment_3();
                } else {
                    d_assignment_4();
                }
            } else if ((inc = skip("++")) || (dec = skip("--"))) {
                // TODO
            } else 
                error("%d: invalid assignment", tok.tok[tok.pos].nline);
        } else { // Scalar?
            if(skip("=")) {
                compExpr();
            } else if((inc = skip("++")) || (dec = skip("--"))) {
                d_assignment_5(siz,v->id);
                if (inc)
                    d_assignment_6();
                else if(dec)
                    d_assignment_7();
            }
            d_assignment_8(siz,v->id);
            if (inc || dec)
                d_assignment_9();
        }
    } else if (v->loctype == V_GLOBAL) {
        if (declare) { // first declare for global variable?
            // assignment only int32_terger
            if (skip("=")) {
                unsigned *m = (unsigned *) v->id;
                *m = atoi(tok.tok[tok.pos++].val);
            }
        } else {
            if (skip("[")) { // Array?
                compExpr();
                d_assignment_10();
                if(skip("]") && skip("=")) {
                    compExpr();
                    d_assignment_11(v->id);
                    if (v->type == T_INT) {
                        d_assignment_12();
                    } else {
                        d_assignment_13();
                    }
                } else error("%d: invalid assignment",
                             tok.tok[tok.pos].nline);
            } else if (skip("=")) {
                compExpr();
                d_assignment_14(v->id);
            } else if ((inc = skip("++")) || (dec = skip("--"))) {
                d_assignment_15(v->id);
                if (inc)
                    d_assignment_16();
                else if (dec)
                    d_assignment_17();
                d_assignment_18(v->id);
            }
            if (inc || dec)
                d_assignment_19();
        }
    }
    return 0;
}
