# ISA-001（TRUTHFUL-CONCLUSION-01）：本文件**不得**自带宿主 ISA 旗标（原 -march=native 已移除）。
# 依据 AGENTS.md §6「不硬编码线程/ISA/block」+ BLD-002「根 CMakeLists.txt 是唯一产品事实源」：
# ISA 姿态由根构建图与机器 profile 决定，模块级独立构建路径不得另行自定 ISA。
# Star Detector 模块 Makefile
# 编译 star_detector.dll, 提供 Python ctypes 调用的星点检测 API
#
# 依赖: 仓内自研 trust-region LM 求解器 (src/nls_lm.cpp), OpenMP; 无外部库依赖
#
# 编译选项:
#  make  默认编译 (O3, 无 LTO)
#  make lto  启用 LTO 优化 (链接时优化, 运行时更快, 编译较慢)
#  make pch  启用预编译头 (PCH, 重复编译更快)
#  make clean  清理产物

CXX = g++

# 编译标志
# -O3 最高优化
# 启用本机 CPU 指令集 (AVX2 等)
# 注意: 不启用 -ffast-math (R11 控制包要求, 禁止 IEEE 不精确优化)
# -fopenmp OpenMP 并行
# -funroll-loops 循环展开
CXXFLAGS_COMMON = -Wall -std=c++17 -fopenmp \
  -ffp-contract=fast -funroll-loops \
  -Iinclude -Isrc

# 默认 Debug/Optimized 配置 (无 LTO, 编译快)
CXXFLAGS = $(CXXFLAGS_COMMON) -O3
LDFLAGS = -static-libgcc -static-libstdc++ -shared -fopenmp
LDLIBS = -lm

# LTO 配置 (运行时更快, 编译较慢)
CXXFLAGS_LTO = $(CXXFLAGS_COMMON) -O3 -flto
LDFLAGS_LTO = -static-libgcc -static-libstdc++ -shared -fopenmp -flto

# PCH 配置
PCH_HEADER = src/stardetector_common.h
PCH_FILE = src/stardetector_common.h.gch

SRCDIR = src
INCDIR = include
TARGET = star_detector.dll

SOURCES = $(SRCDIR)/sdet_api.cpp $(SRCDIR)/sdet_detector.cpp $(SRCDIR)/sdet_image.cpp $(SRCDIR)/sdet_log.cpp $(SRCDIR)/sdet_background.cpp $(SRCDIR)/nls_lm.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# 默认目标: 直接编译 (无 LTO, 无 PCH, 最简单)
all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# LTO 目标: 启用链接时优化
lto: $(SOURCES)
	$(CXX) $(CXXFLAGS_LTO) $(LDFLAGS_LTO) -o $@ $^ $(LDLIBS)

# PCH 目标: 启用预编译头 (重复编译更快)
pch: $(PCH_FILE) $(SOURCES)
	$(CXX) $(CXXFLAGS) -include $(PCH_HEADER) $(LDFLAGS) -o $(TARGET) $(SOURCES) $(LDLIBS)

$(PCH_FILE): $(PCH_HEADER)
	$(CXX) $(CXXFLAGS) -x c++-header $(PCH_HEADER)

# 对象文件目标 (用于并行编译, 未来扩展)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	del /f /q $(TARGET) 2>nul
	del /f /q $(SRCDIR)\*.o 2>nul
	del /f /q $(PCH_FILE) 2>nul

.PHONY: all lto pch clean
