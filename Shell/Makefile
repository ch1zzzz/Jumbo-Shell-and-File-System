CC=/usr/sup/bin/gcc
LD=$(CC)
CPPFLAGS=-g -std=gnu99 -Wpedantic -Wall
CFLAGS=-I.
LDFLAGS=
LDLIBS=
PROGRAM=shell

all: $(PROGRAM)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(PROGRAM): $(PROGRAM).o
	$(LD) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS) -o $@ $<

.PHONY:
clean:
	rm -f *.o $(PROGRAM)
