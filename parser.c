#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "parser.h"
#include "expr.h"

enum{
    keywordAndOther, //do,| ,end, under, alias, if, while, unless, until, BEGIN, {, }, =, return, yield, and, or, not, !, ., ::, super, (, ), .., ..., +, -, *, /, %, **, +, -, ^, &, <=>, >, >=, <, <=, ==, ===, !=, =~, !~, ~, <<, >>, &&, ||, defined?, elsif, else, when, in, for,begin,rescue,ensure,class,module,def,&, ',' , nil, self,';','/n'
    variable,
    numeric,

    PROGRAM,
    COMPSTMT,
    STMT,
    EXPR,
    CALL,
    COMMAND,
    FUNCTION,
    ARG,
    PRIMARY,
    WHEN_ARGS,
    THEN,
    DO,
    BLOCK_VAR,
    MLHS,
    MLHS_ITEM,
    LHS,
    MRHS,
    CALL_ARGS,
    ARGS,
    ARGDECL,
    ARGLIST,
    SINGLETON,
    ASSOCS,
    ASSOC,
    VARIABLE,
    LITERAL,
    TERM,
    OP_ASGN,
    SYMBOL,
    FNAME,
    OPERATION,
    VARNAME,
    GLOBAL,
    STRING,
    STRING2,
    HERE_DOC,
    REGEXP,
    IDENTIFIER
};

typedef struct tree{
    int type;
    char data[32];
    struct tree* next;
    struct tree* childs;
    int child_count;
} PTNode;

PTNode *head,*tail;
int buf_count;

char checkkeyword[70][8]={"do","|" ,"end", "under", "alias", "if","\"", "while", "unless", "until", "BEGIN", "{", "}", "=", "return", "yield", "and", "or", "not", "!", ".", "::", "super", "(", ")", "..", "...", "+", "-", "*", "/", "%", "**", "+", "-", "^", "&", "<=>", ">", ">=", "<", "<=", "==", "===", "!=", "=~", "!~", "~", "<<", ">>", "&&", "||", "defined", "elsif", "else", "when", "in", "for","begin","rescue","ensure","class","module","def","&", "," , "nil", "self",";","/n"};

//sarah 12/15/2016 end

int32_t skip(char *s)
{
    if (!strcmp(s, tok.tok[tok.pos].val)) {
        tok.pos++;
        return 1;
    }
    return 0;
}

//sarah 12/15/2016 begin
int CheckType(char * str){
    if(str[0]>'0' && str[0]<'9'){
    	int i=1;
    	while(str[i]!='\0'){	
    		if(str[i]<'0' || str[i] >'9'){
    			printf("error: number mixed words");
    			exit(1);		
    		}
            i++;
    	}
        return numeric;
    }
    else if (!(str[0]>'0' && str[0]<'9')){
        for(int32_t j= 0; j < 70; j ++){  //sizeof(keywordAndOther)/8
            if(strcmp(str,checkkeyword[j])==0){
                return keywordAndOther;
            }
        }
        return variable;
    }

    return -1;
}
//sarah 12/15/2016 end

void printBuffer() {
    printf("\n\n");
	PTNode *p;
	p = head;
	while(p != tail) {
		printf("%s ,%d\n",p->data,p->type);
		p = p -> next;
	}
	printf("%s ",p->data);
}

void pushToBuffer(char *str){
   

	PTNode *p;
    p=(PTNode *)malloc(sizeof(PTNode));
	if(p == NULL){  
	   printf("Out of memory\n");  
	   exit(-1);  
	}

    if(buf_count==0) {				// 初始化head,tail
		tail = p;
        head = p;
	}
	if (strlen(str) >= 32) {
		printf("error : too big size for identifier !\n");
		exit(-1);
	}
	strcpy(p->data,str);			// head <- p1 <- p2 <- p3 <- tail
	
	if (tail == NULL)
		printf("warning : something got wrong with building linked list !\n");
	p->next = head;
    head = p;

    buf_count++;

//...

    //strcpy(p.data,tok.tok[tok.pos].val);
    p->type=CheckType(str);	// 回傳什麼type sarah
  //  printf("\n\n");
}
int reduce(){
    if(head != tail && (head->type==variable || head->type == numeric) && (head->next->type==keywordAndOther && (strcmp(head->next->data,"\"")==0)) ){
        PTNode *p;
        p=(PTNode *)malloc(sizeof(PTNode));
        p->type=STRING;
        p->child_count=2;
        strcpy(p->data,"STRING");
        int nextc;
        PTNode *nextp=head;
        for(nextc=0;nextc<p->child_count;nextc++)
            nextp=nextp->next;
        p->next=nextp;
        head=p;
        return 1;

    }
    else
        return 0;
}
static int eval()
{
    PTNode *p;
    while (tok.pos < tok.size){
        int succ=0;
        pushToBuffer(tok.tok[tok.pos].val);
        do{
            succ=reduce();
        }while(succ);
        tok.pos++;
        //if (expression(pos, status)) return 1;
    }
	printBuffer();

    printf("\n%s\t%s xxxxxxxxxx \n",head->data,head->next->data);
    return 0;
}

int (*parser())(int *, void **)
{
    tok.pos = 0;
    buf_count=0;
    eval();
    return NULL;
}

