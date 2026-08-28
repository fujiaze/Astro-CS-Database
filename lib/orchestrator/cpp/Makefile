# ============================================================================
# orchestrator - C++ CLI 编排器 Makefile
# 功能: 编译 orchestrator.exe (Phase1 JSON 入口: orchestrator.exe <stage1.json>)
#       编译 test_dll_loader.exe (DllLoader 单元测试)
#       编译 test_checkpoint.exe (CheckpointManager 单元测试)
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
           -Ithird_party/json-schema-validator
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
TEST_DIR = tests

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
TEST_DLL_LOADER = $(TEST_DIR)/test_dll_loader.exe
TEST_CHECKPOINT = $(TEST_DIR)/test_checkpoint.exe
TEST_LOGGER = $(TEST_DIR)/test_logger.exe
TEST_ORCHESTRATOR_CLI = $(TEST_DIR)/test_orchestrator_cli.exe

.PHONY: all clean test_dll_loader test_checkpoint test_logger test_orchestrator_cli run_test run_integration_test

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(JSV_SRCS) $(INCLUDES) $(LDFLAGS) -lm

# ============================================================================
# DllLoader 单元测试
# ============================================================================
test_dll_loader: $(TEST_DLL_LOADER)

$(TEST_DLL_LOADER): $(TEST_DIR)/test_dll_loader.cpp $(SRC_DIR)/dll_loader.cpp $(INCDIR)/dll_loader.h
	@mkdir -p $(TEST_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_dll_loader.cpp $(SRC_DIR)/dll_loader.cpp $(INCLUDES) $(LDFLAGS) -lm

# ============================================================================
# CheckpointManager 单元测试
# ============================================================================
test_checkpoint: $(TEST_CHECKPOINT)

$(TEST_CHECKPOINT): $(TEST_DIR)/test_checkpoint.cpp $(SRC_DIR)/checkpoint.cpp $(INCDIR)/checkpoint.h
	@mkdir -p $(TEST_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_checkpoint.cpp $(SRC_DIR)/checkpoint.cpp $(INCLUDES) $(LDFLAGS) -lm

# ============================================================================
# Logger 单元测试
# ============================================================================
test_logger: $(TEST_LOGGER)

$(TEST_LOGGER): $(TEST_DIR)/test_logger.cpp $(SRC_DIR)/logger.cpp $(INCDIR)/logger.h
	@mkdir -p $(TEST_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_logger.cpp $(SRC_DIR)/logger.cpp $(INCLUDES) $(LDFLAGS) -lm

# ============================================================================
# 阶段1集成测试: test_orchestrator_cli
# Phase1 JSON 入口: 测试 JSON 配置解析/验证、断点续传、DLL 加载降级、日志集成
# ============================================================================
test_orchestrator_cli: $(TEST_ORCHESTRATOR_CLI)

$(TEST_ORCHESTRATOR_CLI): $(TEST_DIR)/test_orchestrator_cli.cpp \
                          $(SRC_DIR)/orchestrator.cpp \
                          ../../common/healpix/healpix_core.cpp \
                          $(SRC_DIR)/dll_loader.cpp \
                          $(SRC_DIR)/checkpoint.cpp \
                          $(SRC_DIR)/logger.cpp \
                          $(SRC_DIR)/cli_command.cpp \
                          $(SRC_DIR)/json_config.cpp \
                          $(JSV_SRCS) \
                          $(INCDIR)/orchestrator.h \
                          $(INCDIR)/dll_loader.h \
                          $(INCDIR)/checkpoint.h \
                          $(INCDIR)/logger.h \
                          $(INCDIR)/cli_command.h \
                          $(INCDIR)/json_config.h
	@mkdir -p $(TEST_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_orchestrator_cli.cpp \
	    $(SRC_DIR)/orchestrator.cpp ../../common/healpix/healpix_core.cpp \
	    ../../common/crypto/sha256.cpp \
	    $(SRC_DIR)/dll_loader.cpp \
	    $(SRC_DIR)/checkpoint.cpp $(SRC_DIR)/logger.cpp \
	    $(SRC_DIR)/cli_command.cpp $(SRC_DIR)/json_config.cpp $(JSV_SRCS) \
	    $(INCLUDES) $(LDFLAGS) -lm

# ============================================================================
# 运行测试 (需要在项目根目录执行, 以便加载各模块 DLL)
# ============================================================================
run_test: test_dll_loader test_checkpoint test_logger
	@echo ============================================================
	@echo 运行 DllLoader 单元测试 (在项目根目录执行)
	@echo ============================================================
	cd .. && cd .. && cd .. && $(TEST_DIR)\test_dll_loader.exe
	@echo ============================================================
	@echo 运行 CheckpointManager 单元测试
	@echo ============================================================
	$(TEST_CHECKPOINT)
	@echo ============================================================
	@echo 运行 Logger 单元测试
	@echo ============================================================
	$(TEST_LOGGER)

# ============================================================================
# 运行阶段1集成测试 (Task 5)
# 需要先编译 orchestrator.exe (cli_command.cpp 修改后需重新编译)
# ============================================================================
run_integration_test: $(TARGET) test_orchestrator_cli
	@echo ============================================================
	@echo 运行阶段1集成测试 (Task 5 - 5 个 Part)
	@echo ============================================================
	$(TEST_ORCHESTRATOR_CLI)

# ============================================================================
# 清理
# ============================================================================
clean:
	del /f /q $(TARGET) 2>nul
	del /f /q $(TEST_DLL_LOADER) 2>nul
	del /f /q $(TEST_CHECKPOINT) 2>nul
	del /f /q $(TEST_LOGGER) 2>nul
	del /f /q $(TEST_ORCHESTRATOR_CLI) 2>nul
	del /f /q $(SRC_DIR)\*.o 2>nul
	del /f /q $(TEST_DIR)\*.o 2>nul
