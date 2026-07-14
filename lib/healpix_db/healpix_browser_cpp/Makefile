# Makefile - HEALPix 浏览器 C++ 后端 (healpix_browser_cpp)
# 用途: 编译 browser_cpp.exe, 链接 healpix_io.dll + winsock2
# 依赖: MSYS2/MinGW64 (g++), healpix_io.dll (在 ../healpix_io/)

CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra
INCDIR = include
SRCS = src/browser_backend.cpp src/http_server.cpp src/browser_main.cpp
TARGET = browser_cpp.exe

# healpix_io 依赖目录
HIO_DIR = ../healpix_io
WEB_DIR = ../healpix_browser_web

.PHONY: all clean copy_dll

all: $(TARGET)

# 链接 browser_cpp.exe
# -L$(HIO_DIR) -lhealpix_io: 链接 healpix_io.dll (MinGW 可直接链接 dll)
# -lws2_32: 链接 winsock2 (Windows 系统库)
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -I$(HIO_DIR)/include -o $(TARGET) $(SRCS) -L$(HIO_DIR) -lhealpix_io -lws2_32

# 复制 healpix_io.dll 到当前目录 (运行时需要)
copy_dll:
	copy /Y "$(HIO_DIR)\healpix_io.dll" .

clean:
	del /Q $(TARGET) 2>nul
	del /Q src\*.o 2>nul
