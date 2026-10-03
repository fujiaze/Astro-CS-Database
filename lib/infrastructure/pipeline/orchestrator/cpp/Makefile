# ============================================================================
# orchestrator - C++ CLI 编排器 Makefile
# 功能: 编译 orchestrator.exe (Phase1 JSON 入口: orchestrator.exe <stage1.json>)
# 编译器: g++ (MSYS2), C++17, 静态链接 C++ 运行时
# 注: 暂不链接任何 DLL (动态加载由 DllLoader 运行时完成)
# 注: nlohmann/json 已通过 MSYS2 pacman 安装在 mingw64/include, 默认搜索路径
# ============================================================================

CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -fopenmp -DAIO_ENABLE_HEALPIX
# -Iinclude: orchestrator 自身头文件
# 其余 -I 路径: 各 DLL 模块头文件 (orchestrator.cpp stage handler 调用所需)
# Makefile 位于 lib/orchestrator/cpp/, ../../ 解析到 lib/
# -I../../common/include: PrecisionContext / AstroScalarType 双精度 ABI 头文件
# -Ithird_party/json-schema-validator: nlohmann-json-schema-validator v2.4.0 (vendor, 正式 Schema validator)
# -I../../../third_party: 根 vendor nlohmann/json.hpp (Linux 下无 pacman 系统头;
#   Windows MSYS2 同样命中 vendor 头, 双平台一致, 修复 R5 批次 B 回归)
INCLUDES = -Iinclude \
           -I../../common \
           -I../../common/include \
           -I../../astro_image_io/include \
           -I../../plate_solve/cpp/ipv/include \
           -I../../dynamic_psf/include \
           -I../../photometric_calib/cpp/include \
           -I../../snr_estimator/cpp/include \
           -I../../healpix_db/healpix_drizzle \
                     -I../../gaia_xpsd_client/src \
           -I../../star_detector/include \
           -Ithird_party/json-schema-validator \
           -I../../../third_party
# -Wl,--stack,33554432: 设置 EXE 主线程栈为 32MB
# (P13-002: nanoflann divideTree 递归在偏斜 3D 数据上深度可达数百层,
#  DLL 中的代码使用调用线程的栈, -Wl,--stack 对 DLL 无效, 必须设置 EXE 栈)
# BLD-003/004: 平台条件化 — Windows(MinGW) 用 -static + --stack;
#   Linux 用 GNU ld 的 -Wl,-z,stacksize (32MB), 不支持 -static 全静态(依赖 glibc 动态解析)。
ifeq ($(OS),Windows_NT)
LDFLAGS = -static -Wl,--stack,33554432
else
LDFLAGS = -Wl,-z,stacksize=33554432
endif

SRC_DIR = src
INCDIR = include

# 源文件列表 (Phase1 JSON 入口: 删除 cli_repl, 新增 json_config)
SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/orchestrator.cpp \
       ../../common/healpix/healpix_core.cpp ../../common/crypto/sha256.cpp \
       $(SRC_DIR)/dll_loader.cpp \
       $(SRC_DIR)/checkpoint.cpp \
       $(SRC_DIR)/logger.cpp \
       $(SRC_DIR)/cli_command.cpp \
       $(SRC_DIR)/json_config.cpp

# nlohmann-json-schema-validator v2.4.0 (vendor: third_party/json-schema-validator)
JSV_DIR = third_party/json-schema-validator
JSV_SRCS = $(JSV_DIR)/json-uri.cpp \
           $(JSV_DIR)/json-validator.cpp \
           $(JSV_DIR)/json-patch.cpp \
           $(JSV_DIR)/smtp-address-validator.cpp \
           $(JSV_DIR)/string-format-check.cpp \
           $(JSV_DIR)/json-schema-draft7.json.cpp

# 头文件列表
HEADERS = $(INCDIR)/orchestrator.h \
          $(INCDIR)/dll_loader.h \
          $(INCDIR)/checkpoint.h \
          $(INCDIR)/logger.h \
          $(INCDIR)/cli_command.h \
          $(INCDIR)/json_config.h

# 输出目标
TARGET = orchestrator.exe

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(JSV_SRCS) $(INCLUDES) $(LDFLAGS) -lm

# ============================================================================
# 清理
# ============================================================================
clean:
	del /f /q $(TARGET) 2>nul
	del /f /q $(SRC_DIR)\*.o 2>nul