CC 	= gcc

CFLAGS  = -Wall -g -I .

LD 	= gcc

#LDFLAGS  = -Wall -g -L/home/pn-cs453/Given/Asgn2
LDFLAGS  = -Wall -g 

PUBFILES =  README  hungrymain.c  libPLN.a  libsnakes.a  lwp.h\
	    numbersmain.c  snakemain.c  snakes.h

TARGET =  pn-cs453@hornet:Given/asgn2

PROGS	= snakes nums hungry liblwp.so liblwp.a

SNAKEOBJS  = snakemain.o 

HUNGRYOBJS = hungrymain.o 

NUMOBJS    = numbersmain.o

OBJS	= $(SNAKEOBJS) $(HUNGRYOBJS) $(NUMOBJS) 

SRCS	= snakemain.c numbersmain.c

HDRS	= 

EXTRACLEAN = core $(PROGS)

all: 	$(PROGS)

allclean: clean
	@rm -f $(EXTRACLEAN)

clean:	
	rm -f *.o $(OBJS) *~ TAGS

liblwp.so: lwp.o magic64.o
	$(CC) $(CFLAGS) -shared -o $@ lwp.o magic64.o

snakes: snakemain.o liblwp.a libsnakes.a
	$(LD) $(LDFLAGS) -o snakes snakemain.o -L. -lsnakes -llwp -lncurses

hungry: hungrymain.o liblwp.a libsnakes.a
	$(LD) $(LDFLAGS) -o hungry hungrymain.o -L. -lsnakes -llwp -lncurses

nums: numbersmain.o liblwp.a 
	$(LD) $(LDFLAGS) -o nums numbersmain.o -L. -llwp

hungrymain.o: lwp.h snakes.h

snakemain.o: lwp.h snakes.h

numbersmain.o: lwp.h

lwp.o: lwp.c
	$(CC) $(CFLAGS) -fpic -c -o $@ lwp.c

magic64.o: magic64.S
	$(CC) -o magic64.o -c magic64.S

liblwp.a: lwp.o magic64.o
	ar r $@ lwp.o magic64.o

#libPLN.a: ../Publish/lwp.c
#	gcc -c ../Publish/lwp.c
#	ar r libPLN.a lwp.o
#	rm lwp.o

pub:
	scp $(PUBFILES) $(TARGET)

