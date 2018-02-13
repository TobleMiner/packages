CP?=cp -r
LN?=ln -s
FPIC?=-fPIC

all: libmeshneighbour

libmeshneighbour:
	$(CC) -c ${CFLAGS} $(FPIC) -o libmeshneighbour.o libmeshneighbour.c -Wall
	$(CC) -shared $(FPIC) -Wl,-soname,libmeshneighbour.so.0 -o libmeshneighbour.so.0 libmeshneighbour.o -lc -lubox -lubus -lgluonutil

clean:
	rm libmeshneighbour.so.0 libmeshneighbour.o
