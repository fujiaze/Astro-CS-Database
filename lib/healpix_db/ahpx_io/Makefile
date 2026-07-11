# ============================================================================
# .ahpx 单帧存储格式读写模块 Makefile
# 编译为 ahpx_io.dll, 供 Python ctypes 调用
#
# 构建目标:
#   make          - 尝试带压缩库构建 (zstd + lz4)
#   make no-comp  - 不带压缩库构建 (fallback 到不压缩)
#   make clean    - 清理构建产物
# ============================================================================

CXX = g++
CXXFLAGS = -O3 -march=native -ffast-math -funroll-loops -Wall -std=c++17 -fopenmp
INCLUDES = -I.
SRCS = ahpx_reader.cpp ahpx_writer.cpp ahpx_api.cpp compressor.cpp

# 默认: 尝试带压缩库构建
# 通过 -DHAS_ZSTD / -DHAS_LZ4 启用压缩支持
DEFS = -DHAS_ZSTD -DHAS_LZ4
LIBS = -lzstd -llz4 -lm

.PHONY: all no-comp clean

all: ahpx_io.dll

# 带压缩库构建
ahpx_io.dll: $(SRCS)
	$(CXX) $(CXXFLAGS) -shared -o $@ $^ $(INCLUDES) $(DEFS) -static-libgcc -static-libstdc++ $(LIBS)

# 不带压缩库构建 (fallback 到不压缩)
no-comp:
	$(CXX) $(CXXFLAGS) -shared -o ahpx_io.dll $(SRCS) $(INCLUDES) -static-libgcc -static-libstdc++ -lm

clean:
	del /Q *.o *.dll 2>nul
