# ISA-001（TRUTHFUL-CONCLUSION-01）：本文件**不得**自带宿主 ISA 旗标（原 -march=native 已移除）。
# 依据 AGENTS.md §6「不硬编码线程/ISA/block」+ BLD-002「根 CMakeLists.txt 是唯一产品事实源」：
# ISA 姿态由根构建图与机器 profile 决定，模块级独立构建路径不得另行自定 ISA。
CFLAGS = -O2 -Wall -Wextra -Wpedantic
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
