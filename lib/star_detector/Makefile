# Star Detector 模块 Makefile
# 编译 star_detector.dll, 提供 Python ctypes 调用的星点检测 API
#
# 依赖: GSL (gsl_multifit_nlinear) trust-region LM 求解器, OpenMP
#
# 编译选项:
#   make          默认编译 (O3, 无 LTO)
#   make lto      启用 LTO 优化 (链接时优化, 运行时更快, 编译较慢)
#   make pch      启用预编译头 (PCH, 重复编译更快)
#   make clean    清理产物

CXX = g++

# 编译标志
# -O3 最高优化
# -march=native 启用本机 CPU 指令集 (AVX2 等)
# -ffast-math 允许 IEEE 不精确优化 (浮点加速, 天文图像可接受)
# -fopenmp OpenMP 并行
# -funroll-loops 循环展开
CXXFLAGS_COMMON = -Wall -std=c++17 -fopenmp -march=native \
                  -ffp-contract=fast -funroll-loops \
                  -Iinclude -Isrc

# 默认 Debug/Optimized 配置 (无 LTO, 编译快)
CXXFLAGS = $(CXXFLAGS_COMMON) -O3
LDFLAGS = -static-libgcc -static-libstdc++ -shared -fopenmp
LDLIBS = -lgsl -lgslcblas -lm

# LTO 配置 (运行时更快, 编译较慢)
CXXFLAGS_LTO = $(CXXFLAGS_COMMON) -O3 -flto
LDFLAGS_LTO = -static-libgcc -static-libstdc++ -shared -fopenmp -flto

# PCH 配置
PCH_HEADER = src/stardetector_common.h
PCH_FILE = src/stardetector_common.h.gch

SRCDIR = src
INCDIR = include
TARGET = star_detector.dll

SOURCES = $(SRCDIR)/sdet_api.cpp $(SRCDIR)/sdet_detector.cpp $(SRCDIR)/sdet_image.cpp $(SRCDIR)/sdet_log.cpp $(SRCDIR)/sdet_background.cpp
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
