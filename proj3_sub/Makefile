all: parser

parser: y.tab.cc lex.yy.o exec.o
	g++ -g -std=c++11 y.tab.cc lex.yy.o exec.o -o parser -lfl

exec.o: exec.cc
	g++ -g -std=c++11 -c exec.cc

y.tab.cc: parser.y
	bison -y -d -g --verbose parser.y -o y.tab.cc

lex.yy.o: lex.yy.c
	gcc -c lex.yy.c

lex.yy.c: parser.l
	lex parser.l

debug: y.tab.cc lex.yy.o
	g++ -DYYDEBUG=1 y.tab.cc lex.yy.o -o parser -lfl

test_a:	clean parser
	ruby ./test/test.rb

test_b: clean parser
	./harness.py

test_syntax: clean parser
	./harness.py syntax

loc: 	clean
	ls *.cc *.h *.y *.l Makefile | grep -v "^\s*$$" | xargs  wc -l
	
clean:
	rm -f lex.yy.c y.tab.cc *.o parser y.*
