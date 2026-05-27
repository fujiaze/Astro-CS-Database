#ifndef STAR_DETECTOR_H
#define STAR_DETECTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define SDET_EXPORT __declspec(dllexport)
#else
#define SDET_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int structureLayers;
    int hotPixelFilterRadius;
    float iterativeClipSigma;
    int iterativeMaxRounds;
    int medianFilterDetail;
    int maxStars;
    int fitRadius;
    float fwhmClipSigma;
    float maxAxisRatio;
} SDetParams;

typedef struct StarDetectorHandle_s *StarDetectorHandle;

SDET_EXPORT StarDetectorHandle sdet_create(const SDetParams *params);
SDET_EXPORT void sdet_destroy(StarDetectorHandle handle);

SDET_EXPORT int sdet_detect(StarDetectorHandle handle,
                             const uint16_t *image, int width, int height,
                             double **out_x, double **out_y, int *out_count);

SDET_EXPORT void sdet_free_coords(double *coords);

SDET_EXPORT int sdet_detect_debug(StarDetectorHandle handle,
                                   const uint16_t *image, int width, int height,
                                   double **out_x, double **out_y, int *out_count,
                                   float **out_detail, float **out_smap, float **out_binary);

SDET_EXPORT void sdet_free_debug_maps(float *maps);

SDET_EXPORT int sdet_detect_ex(StarDetectorHandle handle,
                                const uint16_t *image, int width, int height,
                                double **out_x, double **out_y, float **out_flux, int **out_saturated, int *out_count);

SDET_EXPORT void sdet_free_detect_ex(double *x, double *y, float *flux, int *saturated);

#ifdef __cplusplus
}
#endif

#endif
