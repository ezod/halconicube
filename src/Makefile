CC= gcc
CFLAGS= -m32 -fPIC -O3 -Wall
LDFLAGS= -m32

H_INCLUDE=/opt/halcon/include
H_LIB=/opt/halcon32/lib/x86sse2-linux2.4-gcc40

all: hAcqICube.so

hAcqICube.so: hAcqICube.o
	$(CC) $(LDFLAGS) -s -shared -o $@ $^ -L$(H_LIB) -lhalcon -lNETUSBCAM

hAcqICube.o: hAcqICube.c NETUSBCAM_API.h
	$(CC) $(CFLAGS) -I$(H_INCLUDE) -c $<

clean:
	rm -f core *.o hAcqICube.so
