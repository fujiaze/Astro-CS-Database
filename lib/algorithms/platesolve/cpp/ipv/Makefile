# ISA-001（TRUTHFUL-CONCLUSION-01）：本文件**不得**自带宿主 ISA 旗标（原 -march=native 已移除）。
# 依据 AGENTS.md §6「不硬编码线程/ISA/block」+ BLD-002「根 CMakeLists.txt 是唯一产品事实源」：
# ISA 姿态由根构建图与机器 profile 决定，模块级独立构建路径不得另行自定 ISA。
# IPV Phase I MVP Makefile
# 编译为单一 ipv_solver.dll + 测试目标

# === 编译器配置 ===
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fopenmp
INCLUDES = -Iinclude

# === DLL 导出宏 ===
# 编译 DLL 时定义 IPV_EXPORTS (dllexport)
# 使用 DLL 时不定义 (dllimport)
DLL_DEFS = -DIPV_EXPORTS

# === 源文件列表 ===
SRC_DIR = src
OBJ_DIR = obj
TEST_DIR = test

SRCS = $(SRC_DIR)/ipv_select.cpp \
  $(SRC_DIR)/ipv_kvector.cpp \
  $(SRC_DIR)/ipv_polygon.cpp \
  $(SRC_DIR)/ipv_triangle.cpp \
  $(SRC_DIR)/ipv_ransac.cpp \
  $(SRC_DIR)/ipv_itertrans.cpp \
  $(SRC_DIR)/ipv_angle.cpp \
  $(SRC_DIR)/ipv_wcs.cpp \
  $(SRC_DIR)/ipv_sip.cpp \
  $(SRC_DIR)/ipv_distortion.cpp \
  $(SRC_DIR)/ipv_robust_refine.cpp \
  $(SRC_DIR)/ipv_solver.cpp \
  $(SRC_DIR)/ipv_entry.cpp

OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

# === 目标 ===
DLL_TARGET = ipv_solver.dll
TEST_KVECTOR = test_kvector.exe

.PHONY: all clean test

# 默认目标: 编译 DLL
all: $(DLL_TARGET)

# 创建 obj 目录 (Windows cmd.exe 兼容)
$(OBJ_DIR):
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)

# 编译对象文件
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DLL_DEFS) -c $< -o $@

# 链接 DLL
$(DLL_TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -shared $^ -o $@ -lkernel32 -fopenmp
	@echo "=== DLL 编译完成: $(DLL_TARGET) ==="

# === 测试目标 ===
# k-vector 单元测试
test: $(TEST_KVECTOR)
	./$(TEST_KVECTOR)

$(TEST_KVECTOR): $(TEST_DIR)/test_kvector.cpp $(SRC_DIR)/ipv_kvector.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TEST_DIR)/test_kvector.cpp $(SRC_DIR)/ipv_kvector.cpp -o $(TEST_KVECTOR)
	@echo "=== 测试编译完成: $(TEST_KVECTOR) ==="

# === 清理 (Windows cmd.exe 兼容) ===
clean:
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist $(DLL_TARGET) del /q $(DLL_TARGET)
	@if exist $(TEST_KVECTOR) del /q $(TEST_KVECTOR)
	@echo "=== 清理完成 ==="
