%{
#include<stdio.h>
%}

%error-verbose
%debug 

%token WR FT HD VR ID NM NL ST

%start program

%%
program : sop  newlines statements eop; 

/* Multiple new lines 
  in case that there are wonky white space */
newlines: 
    | newlines NL; 

/* Start of program */
sop: newlines HD;

/* End of program */
eop: FT newlines;

/* End of statement */
eos: NL newlines | ';' newlines;
        
/* Block of statement */
statements: 
            | statements statement eos
            ;

statement:  assign
            | declare
            | WR '(' plist ')' 
            ;

/* Parameter list */
plist: 
    |
    plist_r;

plist_r: para
        | plist_r ',' para;

para: ST | aexpr;

/* Operators with lower precedence */
lp_opt: '-' | '+';

hp_opt: '*' | '/'; 

/* Arithmetic expression */
aexpr: add_exp;

add_exp:
    add_exp_r mul_exp;

add_exp_r:
    | add_exp lp_opt;

mul_exp:
    mul_exp_r atom;

mul_exp_r:
    | mul_exp hp_opt;

atom:
    ID | NM | '(' aexpr ')';

/* Assign statement */
assign: ID '=' aexpr 
    |   ID '=' ST;

/* Declare statement */
declare: VR ID '=' aexpr 
        | VR ID '=' ST
        | VR ID;

%%

FILE *yyin;
int yylineno;
yyerror(char *s)
{
    fprintf(stderr, "error: %s, line: %d\n", s, yylineno);
}

main(int argc, char *argv[])
{
    if (argc == 2){
        FILE *file;
        
        file = fopen(argv[1], "r");
        if( !file){
            fprintf(stderr, "Failed to open %s\n", argv[1]);
        }
        else {
            yyin = file;
            yyparse();
        }
    
    }
    else {
        fprintf(stderr, "format: %s [filenmae]\n", argv[0]);
    }
}
