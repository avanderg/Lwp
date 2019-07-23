CC = gcc 

CFLAGS = -Wall -g -I .

LD = gcc

LDFLAGS = -Wall -g 


PROGS = snakes nums hungry liblwp.so liblwp.a

OBJ	= snakemain.o hungrymain.o numbermain.o 

all: $(PROGS)

liblwp.so: lwp.o magic64.o
	$(CC) $(CFLAGS) -shared -o $@ lwp.o magic64.o

liblwp.a: lwp.o magic64.o
	ar r $@ lwp.o magic64.o

lwp.o: lwp.c
	$(CC) $(CFLAGS) -fpic -c -o $@ lwp.c

magic64.o: magic64.S
	$(CC) -o magic64.o -c magic64.S

snakes: snakemain.o liblwp.a libsnakes.a
	$(LD) $(LDFLAGS) -o snakes snakemain.o -L. -lsnakes -llwp -lncurses

hungry: hungrymain.o liblwp.a libsnakes.a
	$(LD) $(LDFLAGS) -o hungry hungrymain.o -L. -lsnakes -llwp -lncurses

nums: numbersmain.o liblwp.a 
	$(LD) $(LDFLAGS) -o nums numbersmain.o -L. -llwp

hungrymain.o: lwp.h snakes.h


numbersmain.o: lwp.h

clean:	
	rm -f *.o $(OBJS) *~ TAGS
allclean: 
	rm -f *.o $(OBJS) $(PROGS) *~ TAGS


