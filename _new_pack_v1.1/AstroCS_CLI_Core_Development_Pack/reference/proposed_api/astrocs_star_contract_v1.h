#ifndef ASTROCS_STAR_CONTRACT_V1_H
#define ASTROCS_STAR_CONTRACT_V1_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AstroCSDetectedStarV1 {
    double x_px;
    double y_px;
    float flux;
    float mag;
    uint32_t flags;
} AstroCSDetectedStarV1;

enum {
    ASTROCS_STAR_SATURATED = 1u << 0,
    ASTROCS_STAR_HAS_SATURATED = 1u << 1
};

#ifdef __cplusplus
}
#endif
#endif
