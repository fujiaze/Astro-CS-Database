#pragma once
#define P2_API __attribute__((visibility("default")))
P2_API int p2_integrate_pixel(const struct P2PixelStack* in, struct P2PixelResult* out);
P2_API int p2_validate_candidate_weights(const double* w, unsigned count);
