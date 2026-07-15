# ============================================================================
# orchestrator - C++ CLI 编排器 Makefile
# 功能: 编译 orchestrator.exe (交互式 REPL + 单次命令执行)
#       编译 test_dll_loader.exe (DllLoader 单元测试)
#       编译 test_checkpoint.exe (CheckpointManager 单元测试)
# 编译器: g++ (MSYS2), C++17, 静态链接 C++ 运行时
# 注: 暂不链接任何 DLL (动态加载由 DllLoader 运行时完成)
# ============================================================================

CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -fopenmp
INCLUDES = -Iinclude
LDFLAGS = -static

SRC_DIR = src
INCDIR = include
TEST_DIR = tests

# 源文件列表
SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/orchestrator.cpp \
       $(SRC_DIR)/dll_loader.cpp \
       $(SRC_DIR)/checkpoint.cpp \
       $(SRC_DIR)/logger.cpp \
       $(SRC_DIR)/cli_repl.cpp \
       $(SRC_DIR)/cli_command.cpp

# 头文件列表
HEADERS = $(INCDIR)/orchestrator.h \
          $(INCDIR)/dll_loader.h \
          $(INCDIR)/checkpoint.h \
          $(INCDIR)/logger.h \
          $(INCDIR)/cli_repl.h \
          $(INCDIR)/cli_command.h

# 输出目标
TARGET = orchestrator.exe
TEST_DLL_LOADER = $(TEST_DIR)/test_dll_loader.exe
TEST_CHECKPOINT = $(TEST_DIR)/test_checkpoint.exe
TEST_LOGGER = $(TEST_DIR)/test_logger.exe
TEST_ORCHESTRATOR_CLI = $(TEST_DIR)/test_orchestrator_cli.exe

.PHONY: all clean test_dll_loader test_checkpoint test_logger test_orchestrator_cli run_test run_integration_test

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(INCLUDES) $(LDFLAGS) -lm

# ============================================================================
# DllLoader 单元测试
# ============================================================================
test_dll_loader: $(TEST_DLL_LOADER)

$(TEST_DLL_LOADER): $(TEST_DIR)/test_dll_loader.cpp $(SRC_DIR)/dll_loader.cpp $(INCDIR)/dll_loader.h
	@if not exist $(TEST_DIR) mkdir $(TEST_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_dll_loader.cpp $(SRC_DIR)/dll_loader.cpp $(INCLUDES) $(LDFLAGS) -lm

# ============================================================================
# CheckpointManager 单元测试
# ============================================================================
test_checkpoint: $(TEST_CHECKPOINT)

$(TEST_CHECKPOINT): $(TEST_DIR)/test_checkpoint.cpp $(SRC_DIR)/checkpoint.cpp $(INCDIR)/checkpoint.h
	@if not exist $(TEST_DIR) mkdir $(TEST_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_checkpoint.cpp $(SRC_DIR)/checkpoint.cpp $(INCLUDES) $(LDFLAGS) -lm

# ============================================================================
# Logger 单元测试
# ============================================================================
test_logger: $(TEST_LOGGER)

$(TEST_LOGGER): $(TEST_DIR)/test_logger.cpp $(SRC_DIR)/logger.cpp $(INCDIR)/logger.h
	@if not exist $(TEST_DIR) mkdir $(TEST_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_logger.cpp $(SRC_DIR)/logger.cpp $(INCLUDES) $(LDFLAGS) -lm

# ============================================================================
# 阶段1集成测试 (Task 5): test_orchestrator_cli
# 测试 REPL 命令、单次命令、断点续传、DLL 加载降级、日志集成
# ============================================================================
test_orchestrator_cli: $(TEST_ORCHESTRATOR_CLI)

$(TEST_ORCHESTRATOR_CLI): $(TEST_DIR)/test_orchestrator_cli.cpp \
                          $(SRC_DIR)/orchestrator.cpp \
                          $(SRC_DIR)/dll_loader.cpp \
                          $(SRC_DIR)/checkpoint.cpp \
                          $(SRC_DIR)/logger.cpp \
                          $(INCDIR)/orchestrator.h \
                          $(INCDIR)/dll_loader.h \
                          $(INCDIR)/checkpoint.h \
                          $(INCDIR)/logger.h
	@if not exist $(TEST_DIR) mkdir $(TEST_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_orchestrator_cli.cpp \
	    $(SRC_DIR)/orchestrator.cpp $(SRC_DIR)/dll_loader.cpp \
	    $(SRC_DIR)/checkpoint.cpp $(SRC_DIR)/logger.cpp \
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
