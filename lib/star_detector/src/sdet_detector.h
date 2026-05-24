#pragma once

#include "sdet_image.h"
#include "../include/star_detector.h"
#include <vector>
#include <cmath>

struct ConnectedComponent {
    int x0, y0, x1, y1;
    int count;
    std::vector<int> px;
    std::vector<int> py;
};

struct DetectedStarInternal {
    double cx, cy;
    float peak, background, sigma, flux;
    int area;
    float kurtosis, snr;
    int rect_x0, rect_y0, rect_x1, rect_y1;
};

struct StarDetectorInternal {
    SDetParams params;
    int width = 0, height = 0;
    float* raw_detail = nullptr;
};

void sdet_get_structure_map(StarDetectorInternal* sd, const float* image, int w, int h, float* out_map);
void sdet_get_local_maxima_map(const float* map, float* out_maxima, int w, int h, float upper_limit);
int sdet_find_connected_components(const float* binary_map, int w, int h, ConnectedComponent** out_components, int* out_count);
void sdet_get_star_parameters(const float* image, int w, int h, ConnectedComponent* cc, DetectedStarInternal* star);
int sdet_apply_filters(DetectedStarInternal* stars, int count, int w, int h);
int sdet_compute_auto_min_structure_size(DetectedStarInternal* stars, int count);
int sdet_deduplicate_stars(DetectedStarInternal* stars, int count, int min_size);
