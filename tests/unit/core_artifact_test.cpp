// CORE-002 单元测试: DataArtifact roundtrip/hash/validate
#include "astrocs/core/artifact.h"

#include <cstdio>
#include <string>

using namespace astrocs::core;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static DataArtifactDescriptor make_descriptor() {
  DataArtifactDescriptor d;
  d.id.id = "sha256:abcdef";
  d.data_schema_id = "DATA-IMG-CAL-001";
  d.schema_version = 1;
  d.scalar = ScalarType::F32;
  d.unit = UnitId::ADU;
  d.coordinate = CoordinateFrame::PIXEL;
  d.shape.dims = {64, 128};
  d.ownership = Ownership::UNIQUE;
  d.storage = "run/x/cal.fits";
  d.provenance.source_commit = "abc123";
  d.provenance.pipeline_hash = "p1";
  d.provenance.config_hash = "c1";
  d.provenance.platform = "linux-amd64";
  d.provenance.data_schema_id = "DATA-IMG-CAL-001";
  d.provenance.schema_version = 1;
  return d;
}

static void test_validate() {
  std::string err;
  CHECK(make_descriptor().validate(&err));
  DataArtifactDescriptor bad = make_descriptor();
  bad.data_schema_id = "foo";
  CHECK(!bad.validate(&err));
  DataArtifactDescriptor bad2 = make_descriptor();
  bad2.shape.dims.clear();
  CHECK(!bad2.validate(&err));
}

static void test_roundtrip() {
  DataArtifactDescriptor d = make_descriptor();
  std::string json;
  CHECK(d.to_json(&json));
  DataArtifactDescriptor out;
  std::string err;
  CHECK(DataArtifactDescriptor::from_json(json, &out, &err));
  CHECK(out.id.id == d.id.id);
  CHECK(out.data_schema_id == d.data_schema_id);
  CHECK(out.scalar == ScalarType::F32);
  CHECK(out.unit == UnitId::ADU);
  CHECK(out.shape.dims.size() == 2);
  CHECK(out.shape.dims[0] == 64 && out.shape.dims[1] == 128);
  CHECK(out.storage == d.storage);
}

static void test_science_hash_stable() {
  DataArtifactDescriptor a = make_descriptor();
  DataArtifactDescriptor b = make_descriptor();
  CHECK(a.provenance.science_hash() == b.provenance.science_hash());
  CHECK(a.provenance.science_hash().size() == 16);
  b.provenance.input_hash = "changed";
  CHECK(a.provenance.science_hash() != b.provenance.science_hash());
}

static void test_validate_rejects_bad_unit() {
  DataArtifactDescriptor d = make_descriptor();
  d.coordinate = CoordinateFrame::ICRS;
  d.unit = UnitId::UNKNOWN;  // 天球坐标必须有单位
  std::string err;
  CHECK(!d.validate(&err));
}

int main() {
  test_validate();
  test_roundtrip();
  test_science_hash_stable();
  test_validate_rejects_bad_unit();
  if (failures == 0) {
    std::printf("CORE-002 TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "CORE-002 TESTS FAIL (%d)\n", failures);
  return 1;
}
