// Small program used by validation/lead/healpix_oracle.py.
//
// It reads directions (unit vectors) from standard input and prints the leaf
// that the production HEALPix index assigns to each of them.  It uses only the
// public coordinates headers of the project.
#include "coordinates/healpix.hpp"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
  const acsd::U32 order = (argc > 1) ? static_cast<acsd::U32>(std::atoi(argv[1])) : 3u;
  double x = 0.0, y = 0.0, z = 0.0;
  while (std::scanf("%lf %lf %lf", &x, &y, &z) == 3) {
    const acsd::SkyICRS direction{x, y, z};
    acsd::HealpixLeaf leaf;
    if (!acsd::coordinates::healpix_lookup(order, direction, leaf)) {
      std::printf("-1\n");
      continue;
    }
    std::printf("%llu\n", static_cast<unsigned long long>(leaf.ipix));
  }
  return 0;
}
