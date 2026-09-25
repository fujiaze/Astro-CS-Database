# ISA-001（TRUTHFUL-CONCLUSION-01）：本文件**不得**自带宿主 ISA 旗标（原 -march=native 已移除）。
# 依据 AGENTS.md §6「不硬编码线程/ISA/block」+ BLD-002「根 CMakeLists.txt 是唯一产品事实源」：
# ISA 姿态由根构建图与机器 profile 决定，模块级独立构建路径不得另行自定 ISA。
CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17 -fopenmp
LDFLAGS = -shared -lm -fopenmp

SRCDIR = src
INCDIR = include
TARGET = dynamic_psf.dll

SOURCES = $(SRCDIR)/dpsf_psf.cpp $(SRCDIR)/dpsf_image.cpp $(SRCDIR)/dpsf_log.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -I$(INCDIR) -I$(SRCDIR)

clean:
	del /f /q $(TARGET) 2>nul
	del /f /q $(SRCDIR)\*.o 2>nul

.PHONY: all clean
