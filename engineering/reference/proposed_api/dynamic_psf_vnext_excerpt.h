#ifndef ASTROCS_DYNAMIC_PSF_VNEXT_EXCERPT_H
#define ASTROCS_DYNAMIC_PSF_VNEXT_EXCERPT_H

/* Reference proposal. Keep the existing uint16 API for one compatibility cycle. */
int dpsf_fit_batch_f32(
    const float* image,
    int width,
    int height,
    const double* cx,
    const double* cy,
    int count,
    const DPSFFitParams* params,
    DPSFFitResult** out_results);

#endif
