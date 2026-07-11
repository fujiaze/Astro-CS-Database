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
#   - ahpx_io/compressor (压缩层, 静态编译)
#   - healpix_stack/healpix_core (HEALpix 运算, 静态编译)
#   - healpix_stack/ahps_reader (读取数据层 .ahps tile, 静态编译)
# ============================================================================

CXX = g++
CXXFLAGS = -O3 -march=native -ffast-math -funroll-loops -Wall -std=c++17 -fopenmp
INCLUDES = -I. -I../ahpx_io -I../healpix_stack

# LOD 模块源文件
SRCS = lod_downsample.cpp lod_manager.cpp hp_lod_api.cpp

# 依赖模块源文件 (静态编译进来, 避免跨 dll 依赖)
DEPS = ../ahpx_io/compressor.cpp \
       ../healpix_stack/healpix_core.cpp \
       ../healpix_stack/ahps_reader.cpp

# 默认: 带压缩库构建
DEFS = -DHAS_ZSTD -DHAS_LZ4
LIBS = -lzstd -llz4 -lm

.PHONY: all no-comp clean

all: healpix_lod.dll

# 带压缩库构建 (链接依赖模块的 .cpp)
healpix_lod.dll: $(SRCS) $(DEPS)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(SRCS) $(DEPS) $(INCLUDES) $(DEFS) -static-libgcc -static-libstdc++ $(LIBS)

# 不带压缩库构建 (fallback 到不压缩)
no-comp:
	$(CXX) $(CXXFLAGS) -shared -o healpix_lod.dll $(SRCS) $(DEPS) $(INCLUDES) -static-libgcc -static-libstdc++ -lm

clean:
	del /Q *.o *.dll 2>nul
