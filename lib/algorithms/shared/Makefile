# Common 公共类型库 Makefile
# ----------------------------------------------------------------------------
# 说明:
#   lib/common 是 header-only 公共类型库 (astro_scalar.h / precision_context.h),
#   本身不产出 .dll/.so. 本 Makefile 仅用于编译运行单元测试, 验证头文件可被
#   编译器找到并通过 static_assert / 运行期检查.
#
# 编译选项:
#   make          编译并运行测试 (默认)
#   make build    仅编译测试
#   make run      仅运行已编译的测试
#   make clean    清理产物
#
# 其他模块引用 common 头文件时, 在自身 Makefile 的 INCLUDES 中添加:
#   -I../common/include   (相对路径, 按模块位置调整)
# 或在 orchestrator 的全局编译命令中统一注入.

CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17 -Iinclude -I.

TEST_SRC = tests/test_precision.cpp
TEST_BIN = test_precision.exe

# 共享 HEALPix core (Phase1 Final Signoff V4): 静态源码, 被 AIO/Drizzle/Browser 引用
HEALPIX_SRC = healpix/healpix_core.cpp
HEALPIX_TEST_SRC = healpix/tests/test_healpix_oracle.cpp
HEALPIX_TEST_BIN = test_healpix_oracle.exe

.PHONY: all build run clean healpix-oracle

all: build run

build: $(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) include/astro_scalar.h include/precision_context.h
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_SRC)

run: $(TEST_BIN)
	./$(TEST_BIN)

# HEALPix 全天空 Oracle: test_healpix_oracle.exe <oracle.jsonl>
healpix-oracle: $(HEALPIX_TEST_BIN)

$(HEALPIX_TEST_BIN): $(HEALPIX_TEST_SRC) $(HEALPIX_SRC) healpix/healpix_core.h
	$(CXX) $(CXXFLAGS) -o $@ $(HEALPIX_TEST_SRC) $(HEALPIX_SRC)

clean:
	del /f /q $(TEST_BIN) 2>nul
	del /f /q $(HEALPIX_TEST_BIN) 2>nul
