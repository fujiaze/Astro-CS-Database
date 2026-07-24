# ============================================================================
# 稀疏 HEALpix 堆栈存储模块 Makefile
# 编译为 healpix_stack.dll, 供 Python ctypes 调用
#
# 构建目标:
#   make          - 带压缩库构建 (zstd + lz4)
#   make no-comp  - 不带压缩库构建 (fallback 到不压缩)
#   make clean    - 清理构建产物
#
# 依赖:
#   1. astro_image_io.dll (AIO C API: aio_compress / aio_decompress)
#   2. healpix_io.dll     (.hiss/.hcsd 读写: hiss_read / hcsd_write / hio_free)
# ============================================================================

CXX = g++
CXXFLAGS = -O3 -march=native -ffast-math -funroll-loops -Wall -std=c++17 -fopenmp

# astro_image_io 目录 (提供 AIO C API 与 aio_pipeline.h)
# healpix_stack 位于 lib/healpix_db/healpix_stack, astro_image_io 位于 lib/astro_image_io
AIO_DIR ?= ../../astro_image_io
# healpix_io 目录 (提供 hiss_read/hcsd_write/hio_free 与 healpix_io.h)
HIO_DIR ?= ../healpix_io
INCLUDES = -I. -I$(AIO_DIR)/include -I$(AIO_DIR)/src -I$(HIO_DIR)/include

SRCS = healpix_core.cpp ahps_reader.cpp ahps_writer.cpp stack_db.cpp stack_engine.cpp hp_stack_api.cpp hp_stack_hiss.cpp

# 默认: 带压缩库构建
# -DHAS_ZSTD / -DHAS_LZ4 保留 (stack 模块自身可能需要的压缩定义)
# 实际压缩调用通过 astro_image_io.dll 的 AIO C API 完成
DEFS = -DHAS_ZSTD -DHAS_LZ4

# 链接 astro_image_io.dll (提供 aio_compress / aio_decompress / aio_compress_bound)
# 链接 healpix_io.dll (提供 hiss_read / hcsd_write / hio_free)
LDFLAGS = -L$(AIO_DIR) -lastro_image_io -L$(HIO_DIR) -lhealpix_io
LIBS = -lzstd -llz4 -lm

.PHONY: all no-comp clean

all: healpix_stack.dll

# 带压缩库构建 (链接 astro_image_io.dll + healpix_io.dll)
healpix_stack.dll: $(SRCS) $(AIO_DIR)/astro_image_io.dll $(HIO_DIR)/healpix_io.dll
	$(CXX) $(CXXFLAGS) -shared -o $@ $(SRCS) $(INCLUDES) $(DEFS) -static-libgcc -static-libstdc++ $(LDFLAGS) $(LIBS)

# 不带压缩库构建 (fallback 到不压缩)
no-comp:
	$(CXX) $(CXXFLAGS) -shared -o healpix_stack.dll $(SRCS) $(INCLUDES) -static-libgcc -static-libstdc++ $(LDFLAGS) -lm

clean:
	del /Q *.o *.dll 2>nul
