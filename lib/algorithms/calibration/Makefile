# ISA-001（TRUTHFUL-CONCLUSION-01）：本文件**不得**自带宿主 ISA 旗标（原 -march=native 已移除）。
# 依据 AGENTS.md §6「不硬编码线程/ISA/block」+ BLD-002「根 CMakeLists.txt 是唯一产品事实源」：
# ISA 姿态由根构建图与机器 profile 决定，模块级独立构建路径不得另行自定 ISA。
CXX = g++
CXXFLAGS = -O3 -funroll-loops -fopenmp -Wall -std=c++17
INCLUDES = -Icpp
SRC_DIR = cpp
SRCS = $(SRC_DIR)/cosmetic_corrector.cpp

.PHONY: all clean

all: cosmetic_corrector.dll

cosmetic_corrector.dll: $(SRCS) $(SRC_DIR)/cosmetic_corrector.h
	$(CXX) $(CXXFLAGS) -shared -o $@ $(SRCS) $(INCLUDES) -static-libgcc -static-libstdc++ -lm

clean:
	del /Q *.o *.dll 2>nul
