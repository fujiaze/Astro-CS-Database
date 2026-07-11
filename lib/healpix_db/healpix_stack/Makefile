# ============================================================================
# 稀疏 HEALpix 堆栈存储模块 Makefile
# 编译为 healpix_stack.dll, 供 Python ctypes 调用
#
# 构建目标:
#   make          - 带压缩库构建 (zstd + lz4)
#   make no-comp  - 不带压缩库构建 (fallback 到不压缩)
#   make clean    - 清理构建产物
#
# 依赖: 复用 ahpx_io 的 compressor (../ahpx_io/compressor.h)
# ============================================================================

CXX = g++
CXXFLAGS = -O3 -march=native -ffast-math -funroll-loops -Wall -std=c++17 -fopenmp
INCLUDES = -I. -I../ahpx_io
SRCS = healpix_core.cpp ahps_reader.cpp ahps_writer.cpp stack_db.cpp stack_engine.cpp hp_stack_api.cpp

# 默认: 带压缩库构建
# 通过 -DHAS_ZSTD / -DHAS_LZ4 启用压缩支持
# compressor.cpp 来自 ahpx_io (静态编译进来, 避免跨 dll 依赖)
DEFS = -DHAS_ZSTD -DHAS_LZ4
LIBS = -lzstd -llz4 -lm

.PHONY: all no-comp clean

all: healpix_stack.dll

# 带压缩库构建 (链接 ahpx_io 的 compressor.cpp)
healpix_stack.dll: $(SRCS) ../ahpx_io/compressor.cpp
	$(CXX) $(CXXFLAGS) -shared -o $@ $(SRCS) ../ahpx_io/compressor.cpp $(INCLUDES) $(DEFS) -static-libgcc -static-libstdc++ $(LIBS)

# 不带压缩库构建 (fallback 到不压缩)
no-comp:
	$(CXX) $(CXXFLAGS) -shared -o healpix_stack.dll $(SRCS) ../ahpx_io/compressor.cpp $(INCLUDES) -static-libgcc -static-libstdc++ -lm

clean:
	del /Q *.o *.dll 2>nul
