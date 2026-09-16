#pragma once

#include "../include/astro_image_io.h"
#include "aio_fits.h"
#include <string>
#include <vector>

int xisf_read_file(const char *path, AIOImageData *out);
int xisf_read_header_only(const char *path, AIOImageData *out);
int xisf_detect(const char *path);
