#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "expr.h"
#include "rubi.h"
#include "parser.h"
#include "codegen.h"

extern int make_stdfunc(char *);

static inline int32_t isIndex() { return !strcmp(tok.tok[tok.pos].val, "["); }

static void primExpr()
{
    if (isdigit(tok.tok[tok.pos].val[0])) { // number?
		d_expr_prim1();
        //| mov eax, atoi(tok.tok[tok.pos++].val)
    } else if (skip("'")) { // char?
		d_expr_prim2();
        //| mov eax, tok.tok[tok.pos++].val[0]
        skip("'");
    } else if (skip("\"")) { // string?
        // mov eax string_address
		d_expr_prim3();
        //| mov eax, getString()
    } else if (isalpha(tok.tok[tok.pos].val[0])) { // variable or inc or dec
        char *name = tok.tok[tok.pos].val;
        Variable *v;

        if (isassign()) assignment();
        else {
            tok.pos++;
            if (skip("[")) { // Array?
                if ((v = getVar(name)) == NULL)
                    error("%d: '%s' was not declared",
                          tok.tok[tok.pos].nline, name);
                compExpr();
				d_expr_prim4();
                //| mov ecx, eax

                if (v->loctype == V_LOCAL) {
					d_expr_prim5(v);
                    //| mov edx, [ebp - v->id*4]
                } else if (v->loctype == V_GLOBAL) {
                    // mov edx, GLOBAL_ADDR
					d_expr_prim6(v);
                    //| mov edx, [v->id]
                }

                if (v->type == T_INT) {
					d_expr_prim7();
                    //| mov eax, [edx + ecx * 4]
                } else {
					d_expr_prim8();
                    //| movzx eax, byte [edx + ecx]
                }

                if (!skip("]"))
                    error("%d: expected expression ']'",
                          tok.tok[tok.pos].nline);
            } else if (skip("(")) { // Function?
                if (!make_stdfunc(name)) {	// standard function
                    func_t *function = getFunc(name);
                    char *val = tok.tok[tok.pos].val;
                    if (isalpha(val[0]) || isdigit(val[0]) ||
                        !strcmp(val, "\"") || !strcmp(val, "(")) { // has arg?
                        for (int i = 0; i < function->args; i++) {
                            compExpr();
							d_expr_prim9();
                            //| push eax
                            skip(",");
                        }
                    }
                    // call func
					d_expr_prim10(function);
                    //| call =>function->address
                    //| add esp, function->args * sizeof(int32_t)
                }
                if (!skip(")"))
                    error("func: %d: expected expression ')'",
                          tok.tok[tok.pos].nline);
            } else {
                if ((v = getVar(name)) == NULL)
                    error("var: %d: '%s' was not declared",
                          tok.tok[tok.pos].nline, name);
                if (v->loctype == V_LOCAL) {
                    // mov eax variable
					d_expr_prim11(v);
                    //| mov eax, [ebp - v->id*4]
                } else if (v->loctype == V_GLOBAL) {
                    // mov eax GLOBAL_ADDR
					d_expr_prim12(v);
                    //| mov eax, [v->id]
                }
            }
        }
    } else if (skip("(")) {
        if (isassign()) assignment(); else compExpr();
        if (!skip(")")) error("%d: expected expression ')'",
                              tok.tok[tok.pos].nline);
    }

    while (isIndex()) {
	d_expr_prim13();
        //| mov ecx, eax
        //skip("[");
        //compExpr();
        //skip("]");
        //| mov eax, [ecx + eax*4]
    }
}

static void mulDivExpr()
{
    int32_t mul = 0, div = 0;
    primExpr();
    while ((mul = skip("*")) || (div = skip("/")) || skip("%")) {
		d_expr_mulDiv1();
        //| push eax
        primExpr();
		d_expr_mulDiv2(mul,div);
    /*    | mov ebx, eax
        | pop eax
        if (mul) {
            | imul ebx
        } else if (div) {
            | xor edx, edx
            | idiv ebx
        } else { // mod 
            | xor edx, edx
            | idiv ebx
            | mov eax, edx
        }*/
    }
}

static void addSubExpr()
{
    int32_t add;
    mulDivExpr();
    while ((add = skip("+")) || skip("-")) {
		d_expr_addsub1();
        //| push eax
        mulDivExpr();
 		d_expr_addsub2(add);
    /*    | mov ebx, eax
        | pop eax
        if (add) {
            | add eax, ebx
        } else {
            | sub eax, ebx
        }*/
    }
}

static void logicExpr()
{
    int32_t lt = 0, gt = 0, ne = 0, eql = 0, fle = 0;
    addSubExpr();
	
    if ((lt = skip("<")) || (gt = skip(">")) || (ne = skip("!=")) ||
        (eql = skip("==")) || (fle = skip("<=")) || skip(">=")) {
		d_expr_logic1();
        //| push eax
        addSubExpr();
		d_expr_logic2(lt,gt,ne,eql,fle);
        /*| mov ebx, eax
        | pop eax
        | cmp eax, ebx
        if (lt)
            | setl al
        else if (gt)
            | setg al
        else if (ne)
            | setne al
        else if (eql)
            | sete al
        else if (fle)
            | setle al
        else
            | setge al

        | movzx eax, al
		*/
    }
}

void compExpr()
{
    int and = 0, or = 0;
    logicExpr();

    while ((and = skip("and") || skip("&")) ||
           (or = skip("or") || skip("|")) || (skip("xor") || skip("^"))) {
	d_expr_com1();
    //    | push eax
        logicExpr();
	d_expr_com2(and,or);
    /*    | mov ebx, eax
        | pop eax
        if (and)
            | and eax, ebx
        else if (or)
            | or eax, ebx
        else
            | xor eax, ebx
	*/
    }
}
