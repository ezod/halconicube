CC= gcc
CFLAGS= -m32 -Wall -O3 -I/opt/halcon/include
LDFLAGS= -m32

all: hAcqICube.so

hAcqICube.so: hAcqICube.o libNETUSBCAM.a
	$(CC) $(LDFLAGS) -shared -o $@ $^

hAcqICube.o: hAcqICube.c NETUSBCAM_API.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f core *.o hAcqICube.so
