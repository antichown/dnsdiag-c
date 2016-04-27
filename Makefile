CC? =
PREFIX?=/usr/local/
CPPFLAGS=-I$(PREFIX)/include -I/usr/local/opt/openssl/include
CFLAGS=-Wall -Wextra -g -O2 -pipe -funroll-loops -ffast-math -fno-strict-aliasing
CFLAGS+=$(CPPFLAGS)
LDFLAGS=-L$(PREFIX)/lib
LDLIBS=

INCLUDES:=$(shell pkg-config --cflags libldns openssl)
LIBS:=$(shell pkg-config --libs libldns openssl)

CFLAGS+=$(INCLUDES)
LDFLAGS+=$(LIBS)

PROGS = dnsping

all: $(PROGS)

dnsping: Makefile dnsping.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS) dnsping.c -o dnsping

clean:
	rm -f *.BAK *.log *.o *.a a.out core temp.* $(PROGS)
	rm -fr *.dSYM

