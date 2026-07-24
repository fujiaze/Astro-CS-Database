#pragma once

#include <vector>
#include <cmath>

struct BackMesh {
    float mean;
    float sigma;
    float mode;
    float lcut;
    float hcut;
    int npix;
    std::vector<int> histo;
    int nlevels;
    float qzero;
    float qscale;
};

struct BackMap {
    int width, height;
    int mesh_width, mesh_height;
    int nx, ny;
    std::vector<float> back;
    std::vector<float> sigma;
    std::vector<float> dback;
    std::vector<float> dsigma;
    float backmean;
    float backsig;
};

void sex_backstat(BackMesh* mesh, const float* buf, int bufsize, int w, int bw);
void sex_backhisto(BackMesh* mesh, const float* buf, int bufsize, int w, int bw);
float sex_backguess(BackMesh* mesh, float* mean, float* sigma);
void sex_filterback(BackMap* bmap);
void sex_makeback(const float* image, int w, int h, int mesh_size, BackMap* bmap);
float sex_get_back(const BackMap* bmap, int x, int y);
float sex_get_sigma(const BackMap* bmap, int x, int y);
