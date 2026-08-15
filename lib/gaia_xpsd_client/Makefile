CFLAGS = -O2 -march=native -Wall -Wextra -Wpedantic
SRC = src/gaia_client.c
OBJ = gaia_client.o

.PHONY: all clean dll

all: gaia_client.dll libgaia_client.a gaia_client_demo

gaia_client.o: $(SRC)
	gcc $(CFLAGS) -fopenmp -c $< -o $@ -Isrc

libgaia_client.a: $(OBJ)
	ar rcs $@ $^
	ranlib $@

gaia_client.dll: $(SRC)
	gcc $(CFLAGS) -shared -o $@ $^ -Isrc -lz -fopenmp

gaia_client_demo: example/demo.c libgaia_client.a
	gcc $(CFLAGS) -o $@ example/demo.c -Isrc -L. -lgaia_client -lz -fopenmp -lm

dll: gaia_client.dll

clean:
	rm -f libgaia_client.a gaia_client.dll gaia_client_demo gaia_client.o
