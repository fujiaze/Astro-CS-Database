# ============================================================================
# LOD 金字塔模块 Makefile
# 编译为 healpix_lod.dll, 供 Python ctypes 调用
#
# 构建目标:
#   make          - 带压缩库构建 (zstd + lz4)
#   make no-comp  - 不带压缩库构建 (fallback 到不压缩)
#   make clean    - 清理构建产物
#
# 依赖:
#   - astro_image_io (AIO C API, 动态链接 astro_image_io.dll)
#   - healpix_stack/healpix_core (HEALpix 运算, 静态编译)
#   - healpix_stack/ahps_reader (读取数据层 .ahps tile, 静态编译, 已用 AIO API)
# ============================================================================

AIO_DIR ?= ../../astro_image_io

CXX = g++
CXXFLAGS = -O3 -march=native -ffast-math -funroll-loops -Wall -std=c++17 -fopenmp
INCLUDES = -I. -I$(AIO_DIR)/include -I$(AIO_DIR)/src -I../healpix_stack

# LOD 模块源文件
SRCS = lod_downsample.cpp lod_manager.cpp hp_lod_api.cpp

# 依赖模块源文件 (静态编译进来, 避免跨 dll 依赖)
# ahps_reader.cpp 已改用 AIO C API, 不再需要 ahpx_io/compressor.cpp
DEPS = ../healpix_stack/healpix_core.cpp \
       ../healpix_stack/ahps_reader.cpp

# 默认: 带压缩库构建 (压缩能力由 astro_image_io.dll 提供)
DEFS = -DHAS_ZSTD -DHAS_LZ4
LIBS = -lm
LDFLAGS = -L$(AIO_DIR) -lastro_image_io

.PHONY: all no-comp clean

all: healpix_lod.dll

# 带压缩库构建 (链接 astro_image_io.dll)
healpix_lod.dll: $(SRCS) $(DEPS)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(SRCS) $(DEPS) $(INCLUDES) $(DEFS) -static-libgcc -static-libstdc++ $(LIBS) $(LDFLAGS)

# 不带压缩库构建 (fallback 到不压缩, 但仍需链接 astro_image_io.dll, 因 ahps_reader 依赖 AIO)
no-comp:
	$(CXX) $(CXXFLAGS) -shared -o healpix_lod.dll $(SRCS) $(DEPS) $(INCLUDES) -static-libgcc -static-libstdc++ -lm $(LDFLAGS)

clean:
	del /Q *.o *.dll 2>nul
