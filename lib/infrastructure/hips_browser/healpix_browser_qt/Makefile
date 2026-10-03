# Makefile - HEALPix 浏览器 Qt 版 (healpix_browser_qt)
# 用途: 编译 libhealpix_browser_core.a 静态库
# 依赖: MSYS2/MinGW64 (g++), astro_image_io.dll (在 ../../astro_image_io/)

CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra -fopenmp -DAIO_ENABLE_HEALPIX
INCDIR = core include
AIO_DIR = ../../astro_image_io

# core 静态库源文件
CORE_SRCS = core/healpix_math.cpp core/stf_engine.cpp core/browser_backend.cpp core/gl_renderer.cpp
CORE_OBJS = $(CORE_SRCS:.cpp=.o)

.PHONY: all clean core

all: core

# 编译 core 静态库
core: libhealpix_browser_core.a

libhealpix_browser_core.a: $(CORE_OBJS)
	ar rcs $@ $^

# 编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(addprefix -I,$(INCDIR)) -I$(AIO_DIR)/include -c $< -o $@

clean:
	del /Q *.o 2>nul
	del /Q core\*.o 2>nul
	del /Q libhealpix_browser_core.a 2>nul