CXX = g++
CXXFLAGS = -O2 -march=native -Wall -std=c++17 -fopenmp
LDFLAGS = -shared -lm -fopenmp

SRCDIR = src
INCDIR = include
TARGET = dynamic_psf.dll

SOURCES = $(SRCDIR)/dpsf_psf.cpp $(SRCDIR)/dpsf_image.cpp $(SRCDIR)/dpsf_log.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -I$(INCDIR) -I$(SRCDIR)

clean:
	del /f /q $(TARGET) 2>nul
	del /f /q $(SRCDIR)\*.o 2>nul

.PHONY: all clean
