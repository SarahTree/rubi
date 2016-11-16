#include <string.h>
#include "expr.h"
#include "codegen.h"

typedef struct {
    char *name;
    int args, addr;
} std_function;

static std_function stdfunc[] = {
    {"Array", 1, 12},
    {"rand", 0, 16}, {"printf", -1, 20}, {"sleep", 1, 28},
    {"fopen", 2, 32}, {"fprintf", -1, 36}, {"fclose", 1, 40}, {"fgets", 3, 44},
    {"free", 1, 48}, {"freeLocal", 0, 52}
};

int make_stdfunc(char *name)
{
    for (size_t i = 0; i < sizeof(stdfunc) / sizeof(stdfunc[0]); i++) {
        if (!strcmp(stdfunc[i].name, name)) {
            if(!strcmp(name, "Array")) {
                compExpr(); // get array size
                	d_make_stdfunc_1();
            } else {
                if (stdfunc[i].args == -1) { // vector
                    uint32_t a = 0;
                    do {
                        compExpr();
                        d_make_stdfunc_2(a);
                        a += 4;
                    } while(skip(","));
                } else {
                    for(int arg = 0; arg < stdfunc[i].args; arg++) {
                        compExpr();
                        d_make_stdfunc_2(arg*4);
                        skip(",");
                    }
                }
                // call $function
                d_make_stdfunc_3(stdfunc[i].addr);
            }
            return 1;
        }
    }
    return 0;
}
