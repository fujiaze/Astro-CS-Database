#pragma once

#include "../include/astro_image_io.h"
#include <vector>
#include <string>

struct AIOImageData {
    float *data;
    int width;
    int height;
    int channels;
    int bits_per_sample;
    int float_sample;
    char source_format[16];
    char source_path[AIO_PATH_MAX];
    AIOFITSKeyword *keywords;
    int keyword_count;
    AIOImageMetadata metadata;
};

struct FITSHeader {
    std::vector<AIOFITSKeyword> keywords;
    int bitpix;
    int naxis;
    int naxis1, naxis2, naxis3;
    double bscale;
    double bzero;
    size_t data_offset;
    size_t data_size;
};

int fits_read_file(const char *path, AIOImageData *out);
int fits_read_header_only(const char *path, AIOImageData *out);
int fits_write_file(const AIOImageData *image, const char *path);
int fits_detect(const char *path);
