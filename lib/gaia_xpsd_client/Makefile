CFLAGS = -O2 -march=native -Wall
SRC = src/gaia_client.c

.PHONY: all clean

all: libgaia_client.a gaia_client_demo

libgaia_client.a: $(SRC)
	ar rcs $@ $^

gaia_client.dll: $(SRC)
	gcc $(CFLAGS) -shared -o $@ $^ -Isrc -lz -fopenmp

gaia_client_demo: example/demo.c libgaia_client.a
	gcc $(CFLAGS) -o $@ $^ -Isrc -lz -fopenmp -lm

clean:
	rm -f libgaia_client.a gaia_client.dll gaia_client_demo
