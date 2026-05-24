CXX = g++
CXXFLAGS = -O2 -march=native -Wall -std=c++17
INCLUDES = -Iinclude -Isrc
SRC_DIR = src
SRCS = $(SRC_DIR)/aio_fits.cpp $(SRC_DIR)/aio_xisf.cpp \
       $(SRC_DIR)/aio_api.cpp $(SRC_DIR)/aio_log.cpp

.PHONY: all clean

all: astro_image_io.dll

astro_image_io.dll: $(SRCS)
	$(CXX) $(CXXFLAGS) -shared -o $@ $^ $(INCLUDES) -static-libgcc -static-libstdc++ -lm

clean:
	del /Q *.o *.dll 2>nul
