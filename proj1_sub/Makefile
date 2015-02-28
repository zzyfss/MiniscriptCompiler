all: parser
parser: y.tab.c lex.yy.c
	gcc y.tab.c lex.yy.c -o parser -lfl

y.tab.c: parser.y
	bison -y -d -g --verbose parser.y

lex.yy.c: parser.l
	lex parser.l

debug: y.tab.c lex.yy.c
	gcc -DYYDEBUG=1 y.tab.c lex.yy.c -o parser -lfl

test: 
	./tests/test_all

clean:
	rm -f lex.yy.c y.tab.c

