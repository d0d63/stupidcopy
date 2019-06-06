# $dodge: Makefile,v 1.4 2011/08/18 14:04:22 dodge Exp $

#CFLAGS = -ggdb -Wall -Werror -pg
CFLAGS = -ggdb -Wall -Werror
#LDFLAGS = -pg

all:	stupidcopy
clean:
	rm -f stupidcopy stupidcopy.o

stupidcopy:	stupidcopy.o
	cc -o stupidcopy $(CFLAGS) stupidcopy.o
