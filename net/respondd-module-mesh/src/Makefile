CFLAGS         += -std=c99 -Wall

all: respondd.so

%.c: %.h

respondd.so: mesh.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -shared -fPIC -D_GNU_SOURCE -lmnl -lgluonutil -lubus -lubox -lm -o $@ $^ $(LDLIBS)

clean:
	rm -rf *.so
