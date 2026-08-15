CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -fPIC -fopenmp
INCDIR = include
SRCS = src/snr_estimator.cpp src/noise_model.cpp
TARGET = snr_estimator.dll

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -shared -o $(TARGET) $(SRCS) -fopenmp -static

clean:
	del /Q $(TARGET) 2>nul
	del /Q src\*.o 2>nul
