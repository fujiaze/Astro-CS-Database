CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -fPIC
INCDIR = include
SRCS = src/healpix_io.cpp
TARGET = healpix_io.dll

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -shared -o $(TARGET) $(SRCS) -lzstd -static

clean:
	del /Q $(TARGET) 2>nul
	del /Q src\*.o 2>nul
