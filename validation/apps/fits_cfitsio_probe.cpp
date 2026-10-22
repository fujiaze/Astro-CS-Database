// Probe of the installed CFITSIO build.
//
// The point of this program is to establish whether the installed library can
// write and read a FITS image with the argument conventions of its header.
// The ffppx and ffgpxv entry points take the axis lengths as separate
// arguments after the axis count, and the first element of a pixel read is a
// pointer; passing the values in another order is a usage error and is not a
// property of the library.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fitsio.h>

namespace {

int fail(const char* what, int status) {
  std::fprintf(stderr, "%s failed with status %d\n", what, status);
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string path = (argc > 1) ? argv[1] : "/tmp/cfitsio_probe.fits";
  const int width = 4;
  const int height = 3;
  std::vector<double> pixels(static_cast<std::size_t>(width * height));
  for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = 0.5 * static_cast<double>(i) - 3.0;

  int status = 0;
  fitsfile* file = nullptr;
  if (fits_create_file(&file, path.c_str(), &status)) return fail("fits_create_file", status);
  const int naxes = 2;
  long naxes_values[2] = {width, height};
  if (fits_create_img(file, DOUBLE_IMG, naxes, naxes_values, &status)) {
    return fail("fits_create_img", status);
  }
  // The first element is given by pointer and the element count follows.
  long first_element = 1;
  if (fits_write_pix(file, TDOUBLE, &first_element, static_cast<long long>(pixels.size()),
                     pixels.data(), &status)) {
    return fail("fits_write_pix", status);
  }
  if (fits_close_file(file, &status)) return fail("fits_close_file", status);

  std::FILE* probe = std::fopen(path.c_str(), "rb");
  if (probe == nullptr) {
    std::fprintf(stderr, "the written file cannot be reopened\n");
    return 1;
  }
  std::fseek(probe, 0, SEEK_END);
  const long size = std::ftell(probe);
  std::fclose(probe);
  std::printf("wrote %s: %ld bytes for %d x %d float64 pixels\n", path.c_str(), size, width,
              height);

  status = 0;
  file = nullptr;
  if (fits_open_file(&file, path.c_str(), READONLY, &status)) return fail("fits_open_file", status);
  int naxis = 0;
  long axes[8] = {0};
  if (fits_get_img_size(file, 8, axes, &status)) return fail("fits_get_img_size", status);
  naxis = static_cast<int>(axes[0]);
  std::printf("header: naxis=%d axes=%ld x %ld\n", naxis, axes[1], axes[2]);
  std::vector<double> read_back(static_cast<std::size_t>(width * height), 0.0);
  // The first element is a pointer to the value, not the value itself.
  long read_first = 1;
  int any_null = 0;
  // fits_read_pix is the binary table row reader; an image is read with
  // fits_read_img, whose first element is given by value.
  if (fits_read_img(file, TDOUBLE, 1, static_cast<long long>(read_back.size()), nullptr,
                    read_back.data(), &any_null, &status)) {
    return fail("fits_read_img", status);
  }
  if (fits_close_file(file, &status)) return fail("fits_close_file", status);

  double worst = 0.0;
  for (std::size_t i = 0; i < read_back.size(); ++i) {
    const double difference = read_back[i] - pixels[i];
    if (difference < 0 ? -difference : difference > worst) worst = difference;
  }
  std::printf("read back %zu pixels, maximum difference %g\n", read_back.size(), worst);
  if (worst != 0.0) {
    std::fprintf(stderr, "the round trip is not bit exact\n");
    return 1;
  }
  std::printf("CFITSIO probe: write and read both work on this installation\n");
  return 0;
}
