CXX = g++
CXXFLAGS = -O2 -march=native -Wall -std=c++17 -fopenmp \
           -DAIO_ENABLE_FITS -DAIO_ENABLE_XISF -DAIO_ENABLE_AHPX \
           -DAIO_ENABLE_HEALPIX -DAIO_ENABLE_COMPRESSOR -DAIO_ENABLE_PIPELINE \
           -DHAS_ZSTD -DHAS_LZ4
CC = gcc
CFLAGS = -O2 -march=native -Wall
INCLUDES = -Iinclude -Isrc -I../common
SRC_DIR = src
AHPX_DIR = $(SRC_DIR)/ahpx

# CFITSIO 4.6.4 (vendored, Phase1 Final Closure V3): 静态编进唯一 AIO DLL
# 源文件清单 = 上游 libcfitsio_la_SOURCES (Makefile.in:817), 排除 f77_wrap*/drvrgsiftp
CFITSIO_DIR = third_party/cfitsio
CFITSIO_SRC = \
    $(CFITSIO_DIR)/buffers.c $(CFITSIO_DIR)/cfileio.c $(CFITSIO_DIR)/checksum.c \
    $(CFITSIO_DIR)/drvrfile.c $(CFITSIO_DIR)/drvrmem.c $(CFITSIO_DIR)/drvrnet.c \
    $(CFITSIO_DIR)/drvrsmem.c $(CFITSIO_DIR)/editcol.c $(CFITSIO_DIR)/edithdu.c \
    $(CFITSIO_DIR)/eval_l.c $(CFITSIO_DIR)/eval_y.c $(CFITSIO_DIR)/eval_f.c \
    $(CFITSIO_DIR)/fitscore.c $(CFITSIO_DIR)/getcol.c $(CFITSIO_DIR)/getcolb.c \
    $(CFITSIO_DIR)/getcold.c $(CFITSIO_DIR)/getcole.c $(CFITSIO_DIR)/getcoli.c \
    $(CFITSIO_DIR)/getcolj.c $(CFITSIO_DIR)/getcolk.c $(CFITSIO_DIR)/getcoll.c \
    $(CFITSIO_DIR)/getcols.c $(CFITSIO_DIR)/getcolsb.c $(CFITSIO_DIR)/getcoluk.c \
    $(CFITSIO_DIR)/getcolui.c $(CFITSIO_DIR)/getcoluj.c $(CFITSIO_DIR)/getkey.c \
    $(CFITSIO_DIR)/group.c $(CFITSIO_DIR)/grparser.c $(CFITSIO_DIR)/histo.c \
    $(CFITSIO_DIR)/iraffits.c $(CFITSIO_DIR)/modkey.c $(CFITSIO_DIR)/putcol.c \
    $(CFITSIO_DIR)/putcolb.c $(CFITSIO_DIR)/putcold.c $(CFITSIO_DIR)/putcole.c \
    $(CFITSIO_DIR)/putcoli.c $(CFITSIO_DIR)/putcolj.c $(CFITSIO_DIR)/putcolk.c \
    $(CFITSIO_DIR)/putcoluk.c $(CFITSIO_DIR)/putcoll.c $(CFITSIO_DIR)/putcols.c \
    $(CFITSIO_DIR)/putcolsb.c $(CFITSIO_DIR)/putcolu.c $(CFITSIO_DIR)/putcolui.c \
    $(CFITSIO_DIR)/putcoluj.c $(CFITSIO_DIR)/putkey.c $(CFITSIO_DIR)/region.c \
    $(CFITSIO_DIR)/scalnull.c $(CFITSIO_DIR)/swapproc.c $(CFITSIO_DIR)/wcssub.c \
    $(CFITSIO_DIR)/wcsutil.c \
    $(CFITSIO_DIR)/imcompress.c $(CFITSIO_DIR)/quantize.c $(CFITSIO_DIR)/ricecomp.c \
    $(CFITSIO_DIR)/pliocomp.c $(CFITSIO_DIR)/fits_hcompress.c \
    $(CFITSIO_DIR)/fits_hdecompress.c $(CFITSIO_DIR)/simplerng.c \
    $(CFITSIO_DIR)/zcompress.c $(CFITSIO_DIR)/zuncompress.c
CFITSIO_OBJ = $(CFITSIO_SRC:.c=.o)

SRCS = $(SRC_DIR)/aio_fits.cpp $(SRC_DIR)/aio_xisf.cpp \
       $(SRC_DIR)/aio_api.cpp $(SRC_DIR)/aio_log.cpp \
       $(SRC_DIR)/aio_compressor.cpp \
       ../common/healpix/healpix_core.cpp \
       $(SRC_DIR)/aio_upm.cpp \
       ../common/crypto/sha256.cpp \
       $(SRC_DIR)/hiss_codec.cpp \
       $(SRC_DIR)/hiss_common.cpp \
       $(SRC_DIR)/hiss_tile_model.cpp \
       $(SRC_DIR)/hiss_transform.cpp \
       $(SRC_DIR)/hiss_writer.cpp \
       $(SRC_DIR)/hiss_stream_writer.cpp \
       $(SRC_DIR)/hiss_reader.cpp \
       $(SRC_DIR)/hips/aio_hips_writer.cpp \
       $(SRC_DIR)/hips/aio_hips_reader.cpp \
       $(SRC_DIR)/healpix/aio_healpix_io.cpp \
       $(AHPX_DIR)/aio_ahpx_writer.cpp $(AHPX_DIR)/aio_ahpx_reader.cpp $(AHPX_DIR)/aio_ahpx_api.cpp \
       $(SRC_DIR)/aio_pipeline.cpp \
       $(SRC_DIR)/aio_pipeline_engine.cpp

.PHONY: all clean

all: astro_image_io.dll

astro_image_io.dll: $(SRCS) $(CFITSIO_OBJ)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(SRCS) $(CFITSIO_OBJ) $(INCLUDES) -I$(CFITSIO_DIR) -static-libgcc -static-libstdc++ -lzstd -llz4 -lz -lm

$(CFITSIO_DIR)/%.o: $(CFITSIO_DIR)/%.c
	$(CC) $(CFLAGS) -I$(CFITSIO_DIR) -c $< -o $@

clean:
	del /Q *.o *.dll 2>nul
	del /Q $(CFITSIO_DIR)\*.o 2>nul
