void dynasm_init_jit();
void dynasm_link_jit(void *jit_sz);
void dynasm_encode_jit(void *jit_buf);
void dynasm_growpc_jit(size_t npc);
void dasm_free_jit();
int dasm_getpclabel_jit(int espBgn);
void* dynasm_get_start_entry();


void d_make_break(size_t lbl);
void d_make_return(size_t lbl);
void d_make_ifStmt(size_t end);

void d_whileStmt_1(size_t loopBgn);
void d_whileStmt_2(size_t end);
void d_whileStmt_3(size_t loopBgn,size_t end);
void d_whileStmt_4(size_t addr);

void d_functionStmt_1(size_t func_addr,size_t func_esp);
void d_functionStmt_2(size_t argsc);
void d_functionStmt_3(size_t addr);
void d_functionStmt_4();

void d_expression_1(size_t mainAddress,size_t mainEsp);
void d_expression_2();
void d_expression_3(char* str);
void d_expression_4();
void d_expression_5();
void d_expression_6();
void d_expression_7();
void d_expression_8();
void d_expression_9(char* str);
void d_expression_10(size_t a);
void d_expression_11(size_t pos,size_t end);
void d_expression_12(size_t pos,size_t endif);
void d_expression_13(size_t end);
void d_expression_14(size_t pos);
void d_expression_15(size_t endif);
void d_expression_16();

void d_assignment_1();
void d_assignment_2(size_t siz,unsigned int id);
void d_assignment_3();
void d_assignment_4();
void d_assignment_5(size_t siz,unsigned int id);
void d_assignment_6();
void d_assignment_7();
void d_assignment_8(size_t siz,unsigned int id);
void d_assignment_9();
void d_assignment_10();
void d_assignment_11(unsigned int id);
void d_assignment_12();
void d_assignment_13();
void d_assignment_14(unsigned int id);
void d_assignment_15(unsigned int id);
void d_assignment_16();
void d_assignment_17();
void d_assignment_18(unsigned int id);
void d_assignment_19();

void d_parser_1(size_t mainAddr);
