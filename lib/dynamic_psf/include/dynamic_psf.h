#ifndef DYNAMIC_PSF_H
#define DYNAMIC_PSF_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define DPSF_EXPORT __declspec(dllexport)
#else
#define DPSF_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int status;
    double B;
    double A;
    double cx;
    double cy;
    double sx;
    double sy;
    double theta;
    double fwhm_x;
    double fwhm_y;
    double mad;
    double flux;
    double eccentricity;
} DPSFFitResult;

#define DPSF_FIT_OK              0
#define DPSF_FIT_NO_CONVERGENCE  1
#define DPSF_FIT_INVALID_PARAMS  2
#define DPSF_FIT_ITERATION_LIMIT 3

typedef struct {
    int fitRadius;
    int maxIter;
    double tolerance;
} DPSFFitParams;

DPSF_EXPORT int dpsf_fit(const uint16_t *image, int width, int height,
                          double cx, double cy,
                          const DPSFFitParams *params,
                          DPSFFitResult *result);

DPSF_EXPORT int dpsf_fit_batch(const uint16_t *image, int width, int height,
                                const double *cx_array, const double *cy_array, int count,
                                const DPSFFitParams *params,
                                DPSFFitResult **out_results);

DPSF_EXPORT void dpsf_free_results(DPSFFitResult *results);

#ifdef __cplusplus
}
#endif

#endif
