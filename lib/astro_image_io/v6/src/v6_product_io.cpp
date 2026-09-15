/* v6_product_io.cpp — 顶层组装实现。 */
#include "astro/aio/v6_product_io.h"

#include <cstring>

namespace astrocs {
namespace aio {

std::vector<ExpectedHdu> expected_from_layers(const std::vector<FitsLayer>& layers) {
  std::vector<ExpectedHdu> out;
  out.reserve(layers.size());
  for (const auto& l : layers) {
    ExpectedHdu e;
    e.extname = l.spec.extname;
    e.bitpix = l.spec.bitpix;
    e.naxis = l.spec.naxis;
    for (const auto& c : l.spec.cards) {
      if (c.type == FitsCard::Type::kString) {
        e.string_cards.push_back({c.keyword, c.value});
        if (c.keyword == "BUNIT") {
          e.bunit = c.value;
          e.check_bunit = true;
        }
      }
    }
    out.push_back(e);
  }
  return out;
}

PublishResult publish_fits_product(const std::string& target,
                                   const std::vector<FitsLayer>& layers,
                                   const std::vector<ExpectedHdu>& expected,
                                   const PublishOptions& opts,
                                   const CancelFn& cancel) {
  // PRIMARY 后存在扩展 -> EXTEND=T（标准要求，外部校验依赖）。
  std::vector<FitsLayer> effective = layers;
  if (effective.size() > 1 && effective.front().spec.extname.empty()) {
    effective.front().spec.primary_has_extensions = true;
  }
  const FileWriterFn writer = [&effective](int fd, const CancelFn& c,
                                           std::string* err) -> bool {
    FitsStreamWriter w(fd);
    for (const auto& layer : effective) {
      if (static_cast<bool>(c) && c()) {
        if (err) *err = "cancelled";
        return false;
      }
      if (!w.begin_hdu(layer.spec, err)) return false;
      if (!layer.data.empty() &&
          !w.write_data(layer.data.data(), layer.data.size(), err)) {
        return false;
      }
      if (!w.end_hdu(err)) return false;
    }
    return w.fsync_now(err);
  };
  const VerifyFn verify = [&expected](const std::string& path,
                                      std::string* err) -> bool {
    const FitsVerifyResult vr = verify_fits_file(path, expected);
    if (!vr.ok) {
      if (err) {
        *err = vr.error.empty() ? vr.violations.empty()
                                      ? std::string("fits verify failed")
                                      : (vr.violations.front().gate + ": " +
                                         vr.violations.front().message)
                                : vr.error;
      }
      return false;
    }
    return true;
  };
  return atomic_publish_file(target, writer, verify, opts, cancel);
}

ValidationReport validate_product_record(const Provenance& prov,
                                         const HipsManifest& manifest,
                                         const HipsProperties& props) {
  ValidationReport r = validate_provenance(prov);
  r.merge(validate_hips_manifest_json(hips_manifest_to_json(manifest)));
  r.merge(validate_hips_properties(render_hips_properties(props)));
  if (manifest.provenance_sha256.empty()) {
    r.add("G-HIPS-MANIFEST", "FZ-PROV-MINIMAL-SET",
          "manifest.provenance_sha256 missing");
  }
  if (manifest.output_hash.empty()) {
    r.add("G-HIPS-MANIFEST", "FZ-PROV-MINIMAL-SET", "manifest.output_hash missing");
  }
  return r;
}

}  // namespace aio
}  // namespace astrocs
