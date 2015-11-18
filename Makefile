OBJS=plang.o
BIN=plang

CFLAGS=-ggdb3
CXXFLAGS=-ggdb3

${BIN}: ${OBJS}
	gcc -o $@ $? -lcurses
