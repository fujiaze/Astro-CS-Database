# ============================================================================
# HEALpix Drizzle 模块 Makefile
# 编译为 healpix_drizzle.dll, 供 Python ctypes 调用
#
# 本模块职责:
#   - 读取 FITS 投影图像 (含 SIP 畸变多项式)
#   - 将 WCS 投影像素重投影 (drizzle) 到 HEALpix 等积网格上
#   - 多边形裁剪 (poly_clip) 处理像素与 HEALpix cell 的交叠
#   - 支持 OpenMP 多线程加速 (默认 16 线程, 由运行时设置)
#   - 输出 .hiss 格式 (通过 healpix_io.dll 的 hiss_write)
#
# 构建目标:
#   make          - 默认构建 (链接 astro_image_io.dll + healpix_io.dll)
#   make no-comp  - 不带压缩库构建 (链接 astro_image_io.dll, 同 all)
#   make clean    - 清理构建产物
#
# 依赖:
#   - astro_image_io/astro_image_io.dll (提供 PipelineFrame + KV 读取 C API)
#   - healpix_io/healpix_io.dll (提供 .hiss 读写 C API: hiss_write)
#   - healpix_stack/healpix_core (HEALpix 运算, 静态编译)
#
# 日志:
#   所有诊断/进度日志输出到 stderr, 不污染 stdout 便于 Python 解析
# ============================================================================

CXX = g++
# R05-B06: 禁用 -ffast-math (PRECISE/reference/test 不得使用)
# -ffast-math 会破坏 IEEE 754 语义, 影响球面几何精度和通量守恒
# FAST 实验可作为独立变量启用, 但生产构建必须禁用
CXXFLAGS = -O2 -march=native -funroll-loops -Wall -std=c++17 -fopenmp

# astro_image_io 路径变量 (提供 AIO C API + PipelineFrame + HEALPix I/O)
# 注: healpix_io 已合并入 aio (2026-07-16 按 spec G1), aio.dll 统一提供 FITS/XISF/HEALPix I/O
AIO_DIR ?= ../../astro_image_io
INCLUDES = -I. -I$(AIO_DIR)/include -I$(AIO_DIR)/src -I../healpix_stack

# Drizzle 模块源文件
#   fits_reader.cpp        - FITS 图像读取与头解析
#   wcs_sip.cpp            - WCS 投影与 SIP 畸变多项式求值
#   poly_clip.cpp          - 多边形裁剪 (Sutherland-Hodgman)
#   spherical_overlap.cpp  - 球面 HEALPix 边界 + Girard 定理 + 候选查询 (02_FROZEN §8)
#   drizzle_engine.cpp     - drizzle 重投影核心算法 (调用 hiss_write 输出 .hiss)
#   hp_drizzle_api.cpp     - 对外 C ABI 接口
SRCS = fits_reader.cpp wcs_sip.cpp poly_clip.cpp spherical_overlap.cpp drizzle_engine.cpp hp_drizzle_api.cpp

# 依赖模块源文件 (静态编译进来, 避免跨 dll 依赖)
# 注: .hiss 读写通过链接 astro_image_io.dll 获得 (healpix_io 已合并入 aio)
# 注: snr_evaluator.cpp (KD-tree IDW) 静态编译进来, 用于 drizzle 端 SNR 重建
DEPS = ../healpix_stack/healpix_core.cpp ../healpix_stack/gradient/snr_evaluator.cpp

# 链接 astro_image_io.dll (PipelineFrame/KV + HEALPix I/O: hiss_write)
# zstd/lz4 由 astro_image_io.dll 内部静态链接, 此处不再需要
LDFLAGS = -L$(AIO_DIR) -lastro_image_io
# AIO_ENABLE_HEALPIX: 启用 aio_healpix_io.h 中的 HEALPix I/O 声明 + 向后兼容宏
DEFS = -DAIO_ENABLE_HEALPIX
LIBS = -lm

.PHONY: all no-comp clean

all: healpix_drizzle.dll

# 默认构建: 链接 astro_image_io.dll (含 HEALPix I/O)
# -shared          生成 Windows DLL
# -static-libgcc   静态链接 libgcc, 避免运行时依赖
# -static-libstdc++ 静态链接 libstdc++
# -Wl,--stack,8388608  设置 DLL 栈大小为 8MB (Windows 默认 1MB 太小,
#                      nanoflann divideTree 在偏斜 3D 数据上递归深度可达数百层,
#                      每帧 1-2KB 栈 → 1MB 不够, P13-002 修复栈溢出 0xC00000FD)
healpix_drizzle.dll: $(SRCS) $(DEPS)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(SRCS) $(DEPS) $(INCLUDES) $(DEFS) -static-libgcc -static-libstdc++ -Wl,--stack,8388608 $(LDFLAGS) $(LIBS)

# 不带压缩库构建 (链接 astro_image_io.dll, 与 all 一致)
no-comp:
	$(CXX) $(CXXFLAGS) -shared -o healpix_drizzle.dll $(SRCS) $(DEPS) $(INCLUDES) $(DEFS) -static-libgcc -static-libstdc++ -Wl,--stack,8388608 $(LDFLAGS) -lm

clean:
	del /Q *.o *.dll 2>nul
