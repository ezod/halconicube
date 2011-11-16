CC= gcc
CFLAGS= -m32 -fPIC -O3 -Wall
LDFLAGS= -m32

H_INCLUDE=$(HALCONROOT)/include
H_LIB=$(HALCONROOT)/lib/$(HALCONARCH)

all: hAcqICube.so

hAcqICube.so: hAcqICube.o
	$(CC) $(LDFLAGS) -s -shared -o $@ $^ -L$(H_LIB) -lhalcon -lNETUSBCAM -lpthread

hAcqICube.o: hAcqICube.c netusbcamextra.h
	$(CC) $(CFLAGS) -I$(H_INCLUDE) -c $<

clean:
	rm -f core *.o hAcqICube.so
