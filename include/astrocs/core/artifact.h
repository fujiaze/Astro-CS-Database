// AstroCS Core Contracts — CORE-002 DataArtifact + Provenance
#pragma once

#include "astrocs/core/contracts.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace astrocs::core {

// Artifact ID: 稳定标识 (跨进程/跨平台一致)
struct ArtifactId {
  std::string id;  // canonical form: "sha256:<hex64>" 或显式 schema:id
};

// Scalar type (DATA-001)
enum class ScalarType : uint8_t { F32 = 0, F64 = 1, U16 = 2, I16 = 3, U8 = 4, I32 = 5 };

constexpr const char* scalar_type_name(ScalarType t) noexcept {
  switch (t) {
    case ScalarType::F32: return "f32";
    case ScalarType::F64: return "f64";
    case ScalarType::U16: return "u16";
    case ScalarType::I16: return "i16";
    case ScalarType::U8: return "u8";
    case ScalarType::I32: return "i32";
  }
  return "unknown";
}

// Physical unit (DATA-001 unit registry; 禁止模糊 weight/value)
enum class UnitId : uint8_t {
  UNKNOWN = 0, ADU = 1, ELECTRON = 2, ADU2 = 3, ELECTRON2 = 4,
  INVERSE_VARIANCE = 5, SURFACE_BRIGHTNESS = 6, DEGREE = 7, DIMENSIONLESS = 8,
};

constexpr const char* unit_name(UnitId u) noexcept {
  switch (u) {
    case UnitId::UNKNOWN: return "UNKNOWN";
    case UnitId::ADU: return "ADU";
    case UnitId::ELECTRON: return "electron";
    case UnitId::ADU2: return "ADU^2";
    case UnitId::ELECTRON2: return "electron^2";
    case UnitId::INVERSE_VARIANCE: return "1/variance";
    case UnitId::SURFACE_BRIGHTNESS: return "surface_brightness";
    case UnitId::DEGREE: return "deg";
    case UnitId::DIMENSIONLESS: return "1";
  }
  return "UNKNOWN";
}

// Coordinate frame (DATA-001)
enum class CoordinateFrame : uint8_t { PIXEL = 0, ICRS = 1, CONTROL_CELL = 2, HEALPIX = 3 };

// Ownership (DATA-001)
enum class Ownership : uint8_t { BORROWED = 0, SHARED = 1, UNIQUE = 2, PERSISTED = 3 };

// Invalid-value policy (DATA-001: NaN/support<=0/ivar=0 显式)
enum class InvalidValuePolicy : uint8_t {
  NAN_INVALID = 0,        // NaN = invalid
  ZERO_SUPPORT_INVALID = 1,  // support<=0 invalid
  IVAR_ZERO_UNAVAILABLE = 2, // ivar=0 显式不可用
  MASK_BIT = 3,           // bitmask invalid
};

struct Shape {
  std::vector<uint64_t> dims;  // row-major; 首元素 = 最慢轴
  uint64_t rank() const { return dims.size(); }
  uint64_t num_elements() const {
    uint64_t n = 1;
    for (auto d : dims) n *= d;
    return n;
  }
};

// Provenance (CORE-002; ARCH-001 §5 冻结字段)
struct Provenance {
  std::string source_commit;     // 源码 commit
  std::string pipeline_hash;     // Pipeline IR hash
  std::string module_build_id;   // module/backend build id
  std::string config_hash;       // 运行配置 hash
  std::string input_hash;        // 输入集合 hash
  std::string created_utc;       // UTC 时间
  std::string platform;          // 平台标识
  std::string data_schema_id;    // DATA-xxx
  uint64_t schema_version = 0;

  // 稳定序列化 (machine-readable, 顺序固定)
  std::string to_json() const;
  // 稳定 hash (仅科学 payload 语义; 与序列化顺序无关)
  std::string science_hash() const;
};

// DataArtifactDescriptor: 跨模块唯一数据合同 (03 §5)
struct DataArtifactDescriptor {
  ArtifactId id;
  std::string data_schema_id;  // DATA-xxx
  uint64_t schema_version = 0;
  ScalarType scalar = ScalarType::F32;
  UnitId unit = UnitId::UNKNOWN;
  CoordinateFrame coordinate = CoordinateFrame::PIXEL;
  Shape shape;
  InvalidValuePolicy invalids = InvalidValuePolicy::NAN_INVALID;
  Ownership ownership = Ownership::UNIQUE;
  std::string storage;  // FITS/HiPS/JSON/manifest path or in-memory tag
  Provenance provenance;

  // 校验 (CORE-002: schema/units/shape 验证)
  bool validate(std::string* err) const;
  // 序列化 roundtrip
  bool to_json(std::string* out) const;
  static bool from_json(const std::string& in, DataArtifactDescriptor* out, std::string* err);
};

}  // namespace astrocs::core
