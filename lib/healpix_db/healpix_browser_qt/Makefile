# Makefile - HEALPix 浏览器 Qt 版 (healpix_browser_qt)
# 用途: 编译 libhealpix_browser_core.a 静态库 + 单元测试
# 依赖: MSYS2/MinGW64 (g++), astro_image_io.dll (在 ../../astro_image_io/)

CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra -fopenmp -DAIO_ENABLE_HEALPIX
INCDIR = core include
AIO_DIR = ../../astro_image_io

# core 静态库源文件
CORE_SRCS = core/healpix_math.cpp core/stf_engine.cpp core/browser_backend.cpp core/gl_renderer.cpp
CORE_OBJS = $(CORE_SRCS:.cpp=.o)

# 测试
TESTS = tests/test_healpix_math.exe tests/test_stf_engine.exe tests/test_browser_backend.exe

.PHONY: all clean tests core run_tests

all: core tests

# 编译 core 静态库
core: libhealpix_browser_core.a

libhealpix_browser_core.a: $(CORE_OBJS)
	ar rcs $@ $^

# 编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(addprefix -I,$(INCDIR)) -I$(AIO_DIR)/include -c $< -o $@

# 测试目标（单独编译每个测试）
tests/test_healpix_math.exe: tests/test_healpix_math.cpp core/healpix_math.cpp
	$(CXX) $(CXXFLAGS) $(addprefix -I,$(INCDIR)) -I$(AIO_DIR)/include -o $@ $^ -L$(AIO_DIR) -lastro_image_io

tests/test_stf_engine.exe: tests/test_stf_engine.cpp core/stf_engine.cpp
	$(CXX) $(CXXFLAGS) $(addprefix -I,$(INCDIR)) -o $@ $^

tests/test_browser_backend.exe: tests/test_browser_backend.cpp core/browser_backend.cpp core/healpix_math.cpp
	$(CXX) $(CXXFLAGS) $(addprefix -I,$(INCDIR)) -I$(AIO_DIR)/include -o $@ $^ -L$(AIO_DIR) -lastro_image_io

tests: $(TESTS)

# 运行所有测试
run_tests: tests
	@for t in $(TESTS); do echo "=== $$t ==="; ./$$t || exit 1; done

clean:
	del /Q *.o 2>nul
	del /Q core\*.o 2>nul
	del /Q libhealpix_browser_core.a 2>nul
	del /Q tests\*.exe 2>nul
