#pragma once
#define P2_API __attribute__((visibility("default")))
P2_API int p2_integrate_pixel(const struct P2PixelStack* in);
