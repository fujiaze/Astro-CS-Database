CXX = g++
CXXFLAGS = -O2 -std=c++17 -fopenmp -fPIC -Wall
LDFLAGS = -shared -fopenmp

SRCDIR = src
INCDIR = include
TARGET = photometric_calib.dll

SOURCES = $(SRCDIR)/pc_api.cpp \
          $(SRCDIR)/star_matcher.cpp \
          $(SRCDIR)/image_corrector.cpp \
          $(SRCDIR)/wcs_transform.cpp

HEADERS = $(INCDIR)/photometric_calib.h \
          $(SRCDIR)/wcs_transform.h \
          $(SRCDIR)/star_matcher.h \
          $(SRCDIR)/image_corrector.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SOURCES) -I$(INCDIR) -I$(SRCDIR) -static -lm

clean:
	del /f /q $(TARGET) 2>nul
	del /f /q $(SRCDIR)\*.o 2>nul
