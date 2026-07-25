#ifndef ASTROCS_IPV_API_VNEXT_EXCERPT_H
#define ASTROCS_IPV_API_VNEXT_EXCERPT_H

#include "astrocs_star_contract_v1.h"

/* Reference proposals. Integrate into the real ipv_api.h only after ABI review. */

/* Candidate path A. It may become production only after all TestData pass. */
int ipv_solve_from_detections_v1(
    void* solver,
    const AstroCSDetectedStarV1* stars,
    int star_count,
    int image_width,
    int image_height,
    double ra0,
    double dec0,
    double focal_length_mm,
    double pixel_size_um,
    const IpvParams* params,
    IpvWcsResult* result);

/* Conservative path B. The pointers are borrowed and valid only during callback. */
typedef int (*IpvDetectionSinkV1)(
    void* user,
    const AstroCSDetectedStarV1* stars,
    int star_count);

/*
 * This entry must preserve the original ipv_solve_from_memory data path.
 * It performs the original internal detection exactly once, invokes sink with
 * the exact full detector output, and then continues the unchanged solver path.
 */
int ipv_solve_from_memory_with_detection_sink_v1(
    void* solver,
    const float* image,
    int image_width,
    int image_height,
    double ra0,
    double dec0,
    double focal_length_mm,
    double pixel_size_um,
    const IpvParams* params,
    IpvDetectionSinkV1 sink,
    void* sink_user,
    IpvWcsResult* result);

#endif
