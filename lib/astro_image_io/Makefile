CXX = g++
CXXFLAGS = -O2 -march=native -Wall -std=c++17 -fopenmp -DAIO_ENABLE_FITS -DAIO_ENABLE_XISF
INCLUDES = -Iinclude -Isrc
SRC_DIR = src
# R05-B12: ahpx 已废弃归档, 从构建中移除
SRCS = $(SRC_DIR)/aio_fits.cpp $(SRC_DIR)/aio_xisf.cpp \
       $(SRC_DIR)/aio_api.cpp $(SRC_DIR)/aio_log.cpp \
       $(SRC_DIR)/aio_compressor.cpp \
       $(SRC_DIR)/hiss_codec.cpp \
       $(SRC_DIR)/hiss_common.cpp \
       $(SRC_DIR)/hiss_tile_model.cpp \
       $(SRC_DIR)/hiss_transform.cpp \
       $(SRC_DIR)/hiss_writer.cpp \
       $(SRC_DIR)/hiss_stream_writer.cpp \
       $(SRC_DIR)/hiss_reader.cpp \
       $(SRC_DIR)/aio_pipeline.cpp \
       $(SRC_DIR)/aio_pipeline_engine.cpp

.PHONY: all clean

all: astro_image_io.dll

astro_image_io.dll: $(SRCS)
	$(CXX) $(CXXFLAGS) -shared -o $@ $^ $(INCLUDES) -static-libgcc -static-libstdc++ -lm

clean:
	del /Q *.o *.dll 2>nul
