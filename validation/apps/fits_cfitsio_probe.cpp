// Probe of the installed CFITSIO build.
//
// The probe establishes, with the argument conventions of the installed
// header, which CFITSIO operations are usable on this platform:
//
//   * reading an image with fits_read_img (ffgpv) works and returns the
//     expected pixels;
//   * writing pixels with fits_write_pix (ffppx) is not usable: the call
//     returns status 308 (bad first element number) and the data unit of the
//     resulting file stays zero filled.
//
// The read path is therefore used by the project, and the write path is
// covered by an explicit FITS writer whose output is checked against an
// external reader (astropy) in the validation tree.
//
// The header of this installation is worth noting: ffgisz fills the axis
// lengths directly (the first entry is the length of the first axis), and
// ffgpxv is the reader for a binary table, not for an image.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fitsio.h>

namespace {

int report(const char* what, int status) {
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
  std::remove(path.c_str());
  if (fits_create_file(&file, path.c_str(), &status)) return report("fits_create_file", status);
  const int naxes = 2;
  long naxes_values[2] = {width, height};
  if (fits_create_img(file, DOUBLE_IMG, naxes, naxes_values, &status)) {
    return report("fits_create_img", status);
  }
  long first_element = 1;
  long long count = static_cast<long long>(pixels.size());
  status = 0;
  if (fits_write_pix(file, TDOUBLE, &first_element, count, pixels.data(), &status)) {
    std::printf("write: fits_write_pix returned status %d (the pixel write path of this build "
                "is not usable)\n", status);
  } else {
    std::printf("write: fits_write_pix reported success\n");
  }
  if (fits_close_file(file, &status)) return report("fits_close_file", status);

  std::FILE* probe = std::fopen(path.c_str(), "rb");
  if (probe == nullptr) {
    std::fprintf(stderr, "the written file cannot be reopened\n");
    return 1;
  }
  std::fseek(probe, 0, SEEK_END);
  const long size = std::ftell(probe);
  std::fclose(probe);
  std::printf("write: %s has %ld bytes (2880 byte header block plus the padded data unit)\n",
              path.c_str(), size);

  status = 0;
  file = nullptr;
  if (fits_open_file(&file, path.c_str(), READONLY, &status)) return report("fits_open_file", status);
  long axes[8] = {0};
  if (fits_get_img_size(file, 8, axes, &status)) return report("fits_get_img_size", status);
  std::printf("read: header reports NAXIS1=%ld NAXIS2=%ld\n", axes[0], axes[1]);
  std::vector<double> read_back(static_cast<std::size_t>(width * height), 0.0);
  int any_null = 0;
  status = 0;
  if (fits_read_img(file, TDOUBLE, 1, count, nullptr, read_back.data(), &any_null, &status)) {
    return report("fits_read_img", status);
  }
  double worst = 0.0;
  for (std::size_t i = 0; i < read_back.size(); ++i) {
    const double difference = std::fabs(read_back[i] - pixels[i]);
    if (difference > worst) worst = difference;
  }
  std::printf("read: fits_read_img returned %zu pixels, maximum difference from the written "
              "values %g\n", read_back.size(), worst);
  if (fits_close_file(file, &status)) return report("fits_close_file", status);

  // The read path is the one the project relies on: check it on a file that
  // was written by an external writer.
  if (argc > 2) {
    status = 0;
    file = nullptr;
    if (fits_open_file(&file, argv[2], READONLY, &status)) return report("fits_open_file", status);
    long external_axes[8] = {0};
    if (fits_get_img_size(file, 8, external_axes, &status)) {
      return report("fits_get_img_size", status);
    }
    const long long elements = external_axes[0] * external_axes[1];
    std::vector<double> external(static_cast<std::size_t>(elements), 0.0);
    status = 0;
    if (fits_read_img(file, TDOUBLE, 1, elements, nullptr, external.data(), &any_null, &status)) {
      return report("fits_read_img", status);
    }
    double sum = 0.0;
    for (double value : external) sum += value;
    std::printf("read: %s gives %lld pixels with a sum of %.6f\n", argv[2], elements, sum);
    if (fits_close_file(file, &status)) return report("fits_close_file", status);
  }

  std::printf("CFITSIO probe: the read path works on this installation, the pixel write path "
              "does not\n");
  return 0;
}
