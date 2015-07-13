CC = gcc
CFLAGS = -Wall -g

mrcrc: mrcrc.o worker.o crc16.o

clean:
	rm -f *.o
