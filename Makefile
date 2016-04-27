CC? =
PREFIX?=/usr/local/
CPPFLAGS=-I$(PREFIX)/include -I/usr/local/opt/openssl/include
CFLAGS=-Wall -Wextra -g -O2 -pipe -funroll-loops -ffast-math -fno-strict-aliasing
CFLAGS+=$(CPPFLAGS)
LDFLAGS=-L$(PREFIX)/lib
LDLIBS=

CFLAGS+=$(shell pkg-config --cflags libldns openssl)
LDFLAGS+=$(shell pkg-config --libs libldns openssl)

PROGS = dnsping

all: $(PROGS)

dnsping: dnsping.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS) dnsping.c -o dnsping

clean:
	rm -f *.BAK *.log *.o *.a a.out core temp.* $(PROGS)
	rm -fr *.dSYM

