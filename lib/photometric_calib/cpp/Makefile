CXX = g++
CXXFLAGS = -O2 -std=c++17 -fopenmp -fPIC -Wall
LDFLAGS = -shared -fopenmp

SRCDIR = src
INCDIR = include
GAIA_DIR = ../../gaia_xpsd_client
GAIA_INCDIR = $(GAIA_DIR)/src
GAIA_DLL = $(GAIA_DIR)/gaia_client.dll
TARGET = photometric_calib.dll

SOURCES = $(SRCDIR)/pc_api.cpp \
          $(SRCDIR)/star_matcher.cpp \
          $(SRCDIR)/image_corrector.cpp \
          $(SRCDIR)/wcs_transform.cpp \
          $(SRCDIR)/spectrum_integrator.cpp

HEADERS = $(INCDIR)/photometric_calib.h \
          $(SRCDIR)/wcs_transform.h \
          $(SRCDIR)/star_matcher.h \
          $(SRCDIR)/image_corrector.h \
          $(SRCDIR)/spectrum_integrator.h

.PHONY: all clean

all: $(TARGET)

# 链接 gaia_client.dll (MinGW ld 可直接接受 .dll 作为输入, 运行时需同目录)
# 复制 gaia_client.dll 到当前目录, 保证 Python 加载 photometric_calib.dll 时能找到依赖
$(TARGET): $(SOURCES) $(HEADERS) $(GAIA_DLL)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SOURCES) -I$(INCDIR) -I$(SRCDIR) -I$(GAIA_INCDIR) $(GAIA_DLL) -static -lm
	-copy /Y "$(GAIA_DLL)" . 2>nul

clean:
	del /f /q $(TARGET) 2>nul
	del /f /q $(SRCDIR)\*.o 2>nul
	del /f /q gaia_client.dll 2>nul
