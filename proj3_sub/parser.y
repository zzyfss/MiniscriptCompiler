%{

#include "def.h"
#include "parser.h"
#include<stdio.h>
#include<list>
#include<string.h>
#include<string>
#include<iostream>

using namespace std;
/*
// Symbol table
map<string, STentry*> stable;


// Stack for STentry objects
stack<STentry*> fields;
*/

Node* root;

extern FILE *yyin;

extern int yylineno;

extern char* yytext;
   extern "C" 
   {

        
       void yyerror(const char *s)
       {
            // fprintf(stderr, "error: %s, line: %d, token:%s\n", s, yylineno, yytext);
            fprintf(stderr, "syntax error\n");
        }
       int yylex(void);
       int yyparse(void);

       // adjust line number
       int adjust_lineno(char lookahead){
            
            // printf("%d:%c,", lookahead, lookahead);
           // newline is represented as 'tab' internally?
           if(lookahead == 9 || lookahead == 10){
                return yylineno - 1;
           }
           else{
               return yylineno;
           }
       }

   } // end extern

%}

%error-verbose
%locations

%debug 

%token <str> ID ST
%token <num> NM
%token WR FT HD VR NL
%token IF EL WH DO BR CO
%token TR FA GE LE NE EQ OR AN
%type<node> program block statement assign declare write loop
%type<node> if el_r cond break continue
%type <node> expr or_exp not_exp than_exp eq_exp
%type <node> aexpr mul_exp atom var para field_init
%type<spec> plist plist_r object_init array_init array_init_r object_init_r

%start program

%%


program : sop newlines block eop 
    {
        root = $3;
    }; 

/* Multiple new lines 
  in case that there are wonky white space */

newlines: 
    | newlines NL; 

/* Start of program */
sop: newlines HD NL;

/* End of program */
eop: FT newlines;

/* End of statement */
eos: NL newlines
     | ';' newlines; 
        
/* Block of statement */
block: {
            Node *bl = new Node;
            bl->type = N_BL;
            
            // instantiate a list to store statement nodes
            list<Node*> *slist = new list<Node*>;

            // Set slist to be special field of block node
            bl->spec.ptr = slist;

            $$ = bl;
        }
    | block statement {
        Node *bl = $1;
        list<Node*> *slist = (list<Node*> *) bl->spec.ptr;
        slist->push_back($2);

        $$ = bl;
    };

statement:  assign eos { $$ = $1; }
            | declare eos { $$ = $1; }
            | write eos { $$ = $1; }
            | if NL newlines { $$  = $1; }
            | loop { $$ = $1;}
            | break eos { 
                $$ = $1;
            }
            | continue eos {
                $$ = $1;
            };

break: BR
{
    Node *br = new Node;
    br->type = N_BR;
    br->lineno = adjust_lineno(yychar) ;
    $$ = br;

};

continue: CO
{
    Node *co = new Node;
    co->type = N_CO;
    co->lineno = adjust_lineno(yychar) ;

    $$ = co;

};

/* Write statement */
write: WR '(' plist newlines ')' {
        Node* wr = new Node;
        wr->type = N_WR;
        wr->spec.ptr = $3;
        wr->lineno = yylineno;

        $$ = wr;
    };

/* Parameter list */
plist: 
    { $$ = NULL; }
    |
    plist_r { $$ = $1; };

plist_r: para   {
            list<Node*> *plist = new list<Node*>;
            plist->push_back($1);
            $$= plist;
        }
        | plist_r ',' newlines para {
            list<Node*> *plist = (list<Node*> *) $1;
            plist->push_back($4);
            $$ = $1;
        };

para: expr { $$ = $1; };

/* if statement */
if: IF '(' cond ')' '{' NL newlines 
        block
    '}' el_r 
{
    Node* ifstmt = new Node();
    ifstmt->type = N_IF;
    ifstmt->left = $8; // if branch
    ifstmt->right = $10; // else branch
    ifstmt->spec.ptr = $3; // condition
    
    $$ = ifstmt;
}

/* else branch */
el_r: /* empty */
    { $$ = NULL; }
    | 
    /* else if */
    EL if
    { $$ = $2; }
    | 
    /* else */
    EL '{' NL newlines 
            block
    '}' 
    {
        $$ = $5;
    };

/* loop statement */
loop:
    /* while-do */
    WH '(' cond ')' '{' NL newlines
        block
    '}' NL newlines
    {
        Node* lp = new Node;
        lp->type = N_WD;
        lp->left = $3;
        lp->right = $8;

        $$ = lp;
    }
    |
    /* do-while */
    DO '{' NL newlines
        block
    '}' NL
    WH '(' cond ')' do_while_end newlines
    {
        Node* lp = new Node;
        lp->type = N_DW; 
        lp->left = $10; // Condition
        lp->right = $5;

        $$ = lp;
    };
do_while_end:
    ';' NL
    | NL;

/* Condition */
cond: expr { $$ = $1; }; 

expr: or_exp { $$ = $1; };

/* Boolean expression */
or_exp: or_exp OR eq_exp
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar);
        exp->left = $1;
        exp->right = $3;
        exp->spec.etype = E_OR;

        $$ = exp;
    }
    | or_exp  AN eq_exp
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $1;
        exp->right = $3;
        exp->spec.etype = E_AND;

        $$ = exp;
    }
    | eq_exp { $$ = $1; };

eq_exp: eq_exp EQ than_exp 
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $1;
        exp->right = $3;
        exp->spec.etype = E_EQ;

        $$ = exp;
    }
    | eq_exp NE than_exp
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $1;
        exp->right = $3;
        exp->spec.etype = E_NE;

        $$ = exp;
    }
    | than_exp { $$ = $1; };

than_exp: than_exp '<' aexpr
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $1;
        exp->right = $3;
        exp->spec.etype = E_LT;

        $$ = exp;
    }
    | than_exp LE aexpr
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $1;
        exp->right = $3;
        exp->spec.etype = E_LE;

        $$ = exp;
    }
    | than_exp '>' aexpr
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $1;
        exp->right = $3;
        exp->spec.etype = E_GT;

        $$ = exp;
    }
    | than_exp GE aexpr
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $1;
        exp->right = $3;
        exp->spec.etype = E_GE;

        $$ = exp;
    }
    | aexpr { $$ = $1; };


/* Arithmetic expression */
aexpr: aexpr '+' mul_exp
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $1;
        exp->right = $3;

        exp->spec.etype = E_ADD;

        $$ = exp;
    }
    | aexpr  '-' mul_exp 
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $1;
        exp->right = $3;
        exp->spec.etype = E_SUB;

        $$ = exp;
    }
    | mul_exp { $$ = $1; };

mul_exp: mul_exp '*' not_exp
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $1;
        exp->right = $3;
        exp->spec.etype = E_MUL;

        $$ = exp;
    }
    | mul_exp '/' not_exp 
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $1;
        exp->right = $3;
        exp->spec.etype = E_DIV;

        $$ = exp;
    }
    | not_exp { $$ = $1; };

not_exp: '!' atom 
    {
        Node* exp = new Node;
        exp->type = N_EX;
        exp->lineno = adjust_lineno(yychar) ;
        exp->left = $2;
        exp->spec.etype = E_NOT;

        $$ = exp;
    }
    | atom { $$ = $1; };

atom:
    TR  
    {   // true value
        Node* atom = new Node;
        atom->type = N_VA;
        atom->lineno = adjust_lineno(yychar) ;
        
        Value* v = new Value;
        v->val.v_int = 1;
        v->type = V_BOL;

        atom->spec.ptr = v;

        $$ = atom;
    }
    | FA
    {   // false value
        Node* atom = new Node;
        atom->type = N_VA;
        atom->lineno = adjust_lineno(yychar) ;
        
        Value* v = new Value;
        v->val.v_int = 0;
        v->type = V_BOL;

        atom->spec.ptr = v;

        $$ = atom;
    }
    | ST
    {   // str value
        Node* atom = new Node;
        atom->type = N_VA;
        atom->lineno = adjust_lineno(yychar) ;
        
        Value* v = new Value;
        v->val.v_str = $1;
        v->type = V_STR;

        atom->spec.ptr = v;

        $$ = atom;
    }
    | var   { $$ = $1; } 
    | NM    
    { 
        Node* atom = new Node;
        atom->type = N_VA;
        atom->lineno = adjust_lineno(yychar) ;
        
        Value* v = new Value;
        v->val.v_int = $1;
        v->type = V_INT;

        atom->spec.ptr = v;

        $$ = atom;
    }
    | '(' expr ')' 
    { $$ = $2; };

/* Varialbe could be scalar or object field or an array member */
var: ID  {
        // identifier node
        Node* id = new Node;
        id->type = N_ID;
        id->lineno = adjust_lineno(yychar) ;
        id->spec.ptr = $1;

        $$ = id;
    }
    | ID '.' ID {
        Node* field = new Node;
        field->type = N_FD;
        field->left = (Node*) $1;
        field->right = (Node*) $3;
        field->lineno = adjust_lineno(yychar) ;

        $$= field;
    }
    | ID '[' expr ']' {
        Node* arr = new Node;
        arr->type = N_AR;
        arr->left = $3;
        arr->spec.ptr = $1;
        arr->lineno = adjust_lineno(yychar) ;

        $$ = arr;
    };

/* Assign statement */
assign: var '=' expr {
            // assignment node
            Node* assign = new Node;
            assign->type = N_AS;
            assign->lineno = $1->lineno;
            assign->left = $1;
            assign->right = $3;
            
            // adjust expr lineno
            $3->lineno = adjust_lineno(yychar);

            $$ = assign;
        };      

/* Declare statement */
declare: VR ID '=' expr {
            
            Node* declare = new Node;
            declare->type = N_SDE;
            declare->lineno = adjust_lineno(yychar);
            declare->spec.ptr = $2;
            declare->right = $4;
            $4->lineno = adjust_lineno(yychar);

            $$ = declare;
        }
        | VR ID
        {
            Node* declare = new Node;
            declare->type = N_DE;
            declare->lineno = adjust_lineno(yychar);
            declare->spec.ptr = $2;

            $$ = declare;
        }
        | /* Object declaration */
        VR ID  '=' '{' object_init '}'
        {
            Node* obj = new Node;
            obj->type = N_ODE;
            obj->lineno = adjust_lineno(yychar);
            obj->spec.ptr = $2;
            obj->right = (Node*) $5;
            
            $$ = obj;
        }
        | /* Array declaration */
        VR ID '=' '[' array_init ']'
        {
            Node* arr = new Node;
            arr->type = N_ADE;
            arr->lineno = adjust_lineno(yychar);
            arr->spec.ptr = $2;
            
            // list of nodes
            arr->right = (Node*) $5;

            $$ = arr;   
        };

/* Array initialization */
array_init: 
    { 
        $$ = NULL;
    }
    | array_init_r newlines {
        /* no trailing \n */
        $$ = $1;
    };

array_init_r: newlines expr {
        list<Node*> *alist = new list<Node*>;
        alist->push_back($2);

        $$ = alist;
    }
    | array_init_r ',' newlines expr {
        list<Node*> *alist = (list<Node*> *)$1;
        alist->push_back($4);
        
        $$ = alist;
    }

/* Object initialization */
object_init: {
        $$ = NULL;
    }
    | object_init_r newlines {
        $$ = $1;
    };

object_init_r: newlines field_init
    {
        list<Node*> *flist = new list<Node*>;
        flist->push_back($2);

        $$ = flist;
    }
    |   object_init_r ',' newlines field_init {
     
        list<Node*> *flist = (list<Node*>*) $1; 
        flist->push_back($4);

        $$ = flist;
    }; 

field_init: ID ':' expr {
        
        /* initialize field */
        Node* ifd = new Node;
        ifd->type = N_IFD;
        ifd->lineno = adjust_lineno(yychar) ;
        ifd->spec.ptr = $1;
        ifd->right = $3;

        $$ = ifd;
    };


%%

main(int argc, char *argv[])
{

    //Node* root = yyparse();

    if (argc == 2){
        FILE *file;
        
        file = fopen(argv[1], "r");
        if( !file){
            fprintf(stderr, "Failed to open %s\n", argv[1]);
        }
        else {
            yyin = file;

            // Parse input and build AST
            yyparse();

            // Interpret AST
            exec(root, NULL);
        }
    }
    else {
        fprintf(stderr, "format: %s [filenmae]\n", argv[0]);
    }

}
