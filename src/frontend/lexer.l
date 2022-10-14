%option noyywrap
%top{
#include <math.h>
}

%{
    int chars = 0;
    int lines = 0;
%}

line    \n
char    .

%%

{line} {lines++;}
{char} {chars++;}

%%