%{
#include "y.tab.h"

%}

%option yylineno

/* Regular definitions */

string  \"[^\n\"]*\"
delim   [ \t]
ws      {delim}+
letter  [A-Za-z]
digit   [0-9]
id      {letter}({letter}|{digit})*
num     {digit}+
nl      [\n]+


%%

"document.write"    return WR;
"</script>"         return FT;
"<script type=\"text/JavaScript\">" return HD;
"var"               return VR;
{id}                return ID;
{num}               return NM;
{nl}                return NL;
{string}            return ST;
{ws}                /* ignore whitespace */;
.                   return (int) yytext[0];
%%

