CXX = g++
CXXFLAGS = -O3 -march=native -Wall -std=c++17 -fopenmp
LDFLAGS = -static-libgcc -static-libstdc++ -shared -fopenmp -lm

SRCDIR = src
INCDIR = include
TARGET = star_detector.dll

SOURCES = $(SRCDIR)/sdet_api.cpp $(SRCDIR)/sdet_detector.cpp $(SRCDIR)/sdet_image.cpp $(SRCDIR)/sdet_log.cpp $(SRCDIR)/sdet_background.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -I$(INCDIR) -I$(SRCDIR)

clean:
	del /f /q $(TARGET) 2>nul
	del /f /q $(SRCDIR)\*.o 2>nul

.PHONY: all clean
