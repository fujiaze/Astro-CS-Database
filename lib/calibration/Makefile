CXX = g++
CXXFLAGS = -O3 -march=native -ffast-math -funroll-loops -fopenmp -Wall -std=c++17
INCLUDES = -Icpp
SRC_DIR = cpp
SRCS = $(SRC_DIR)/cosmetic_corrector.cpp

.PHONY: all clean

all: cosmetic_corrector.dll

cosmetic_corrector.dll: $(SRCS) $(SRC_DIR)/cosmetic_corrector.h
	$(CXX) $(CXXFLAGS) -shared -o $@ $(SRCS) $(INCLUDES) -static-libgcc -static-libstdc++ -lm

clean:
	del /Q *.o *.dll 2>nul
