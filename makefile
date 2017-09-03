COMPILER = mpicc
CFLAGS = -Wall -pedantic -g
COBJS = gaussianLib.o qdbmp.o
CEXES =  gaussian gaussian_parallel parallel

all: ${CEXES}

gaussian: gaussian.c ${COBJS}
	${COMPILER} ${CFLAGS} gaussian.c ${COBJS} -o gaussian -lm

gaussian_parallel: gaussian_parallel.c ${COBJS}
	${COMPILER} ${CFLAGS} gaussian_parallel.c ${COBJS} -o gaussian_parallel -lm

parallel: parallel.c ${COBJS}
	${COMPILER} ${CFLAGS} parallel.c ${COBJS} -o parallel -lm

%.o: %.c %.h  makefile
	${COMPILER} ${CFLAGS} -lm $< -c

clean:
	rm -f *.o *~ ${CEXES}
