%{
#include "def.h"
#include "y.tab.hh"
#include <string.h>
#include <stdlib.h>
#define YY_USER_ACTION yylloc.first_line = yylineno;
%}

%option yylineno

/* Regular definitions */

string  \"[^\n\"]*\"
delim   [ \t]
ws      {delim}+
letter  [A-Za-z]
digit   [0-9]
id      {letter}({letter}|{digit}|[_])*
num     {digit}+
nl      [\n]+


%%

"document.write"    return WR;
"</script>"         return FT;
"<script type=\"text/JavaScript\">" return HD;
"var"               return VR;
"if"                return IF;
"else"              return EL;
"while"             return WH;
"do"                return DO;
"break"             return BR;
"continue"          return CO;
"true"              return TR;
"false"             return FA;
">="                return GE;
"<="                return LE;
"!="                return NE;
"=="                return EQ;
"||"                return OR;
"&&"                return AN;
{id}                yylval.str = strdup(yytext); return ID;
{num}               yylval.num = atoi(yytext); return NM;
{nl}                return NL;
{string}            {/*remove quotes */ 
                        yylval.str = strdup(yytext+1);
                        yylval.str[strlen(yytext)-2] = '\0';
                        return ST;
                    }   
{ws}                /* ignore whitespace */;
.                   return (int) yytext[0];

%%

