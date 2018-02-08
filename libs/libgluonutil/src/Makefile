CP?=cp -r
LN?=ln -s
FPIC?=-fPIC

all: libgluonutil

libgluonutil:
	$(CC) -c ${CFLAGS} $(FPIC) -o libgluonutil.o libgluonutil.c -Wall
	$(CC) -shared $(FPIC) -Wl,-soname,libgluonutil.so.0 -o libgluonutil.so.0 libgluonutil.o -lc

install:
	@echo Running install target
	$(CP) libgluonutil.h /usr/include/libgluonutil-0
	$(CP) libgluonutil.so.0 /usr/lib
	$(LN) /usr/lib/libgluonutil.so.0 /usr/lib/libgluonutil.so

clean:
	rm libgluonutil.so.0 libgluonutil.o
