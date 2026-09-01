// RT-005 可执行模块适配器：Phase1/2/3 session → IModule 工厂
#include "astrocs/core/module_adapters.h"

#include "astrocs/common_abi_v1.h"
#include "astrocs/core/context.h"

#include <cstring>

// session C ABI（与 lib/phaseN_session/*.h 一致；避免把会话头拉进 core 依赖图）
extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out);

acs_status p1_session_create(const astrocs_host_services_v1* host, acs_handle* out);
acs_status p1_session_validate(acs_handle h, const acs_span_u8 config_json);
acs_status p1_session_run(acs_handle h, const acs_span_u8 config_json, int async_io_depth);
acs_status p1_session_inspect(acs_handle h, acs_span_u8* out_manifest_json);
acs_status p1_session_destroy(acs_handle h);

acs_status p2_session_create(const astrocs_host_services_v1* host, acs_handle* out);
acs_status p2_session_validate(acs_handle h, const acs_span_u8 config_json);
acs_status p2_session_run(acs_handle h, const acs_span_u8 config_json);
acs_status p2_session_inspect(acs_handle h, acs_span_u8* out_manifest_json);
acs_status p2_session_destroy(acs_handle h);

acs_status p3_session_create(const astrocs_host_services_v1* host, acs_handle* out);
acs_status p3_session_validate(acs_handle h, const acs_span_u8 request_json);
acs_status p3_session_run(acs_handle h, const acs_span_u8 request_json);
acs_status p3_session_inspect(acs_handle h, acs_span_u8* out_result_json);
acs_status p3_session_destroy(acs_handle h);
}

// session C++ 辅助（last_error 保留错误细节；RT-008 CLI 合同需要）
namespace astrocs::phase1 { std::string last_error(acs_handle h); }
namespace astrocs::phase2 { std::string last_error(acs_handle h); }
namespace astrocs::phase3 { std::string last_error(acs_handle h); }

namespace astrocs::core {

namespace {

struct HostSession {
  astrocs_host_services_v1 host{};
  void* state = nullptr;
  bool valid = false;

  bool init(uint32_t workers) {
    if (astrocs_host_services_default_v1(&host, &state) != 0) return false;
    astrocs_host_state_set_budget_v1(state, workers, workers, &host);
    valid = true;
    return true;
  }
  ~HostSession() {
    if (valid && state) astrocs_host_services_destroy_state_v1(state);
  }
};

std::string status_str(acs_status st) {
  switch (st) {
    case ACS_OK: return "OK";
    case ACS_ERR_PARAM: return "PARAM";
    case ACS_ERR_ABI_MISMATCH: return "ABI_MISMATCH";
    case ACS_ERR_NOMEM: return "NOMEM";
    case ACS_ERR_IO: return "IO";
    case ACS_ERR_UNSUPPORTED: return "UNSUPPORTED";
    case ACS_ERR_CANCELLED: return "CANCELLED";
    case ACS_ERR_STATE: return "STATE";
    case ACS_ERR_BUDGET: return "BUDGET";
    case ACS_ERR_SELFTEST: return "SELFTEST";
    case ACS_ERR_INTERNAL: return "INTERNAL";
    default: return "UNKNOWN(" + std::to_string(static_cast<int>(st)) + ")";
  }
}

Result<void> to_result(acs_status st, const char* what) {
  if (st == ACS_OK) return Result<void>::success();
  ErrorDomain dom = ErrorDomain::INTERNAL;
  switch (st) {
    case ACS_ERR_PARAM: dom = ErrorDomain::DATA; break;
    case ACS_ERR_ABI_MISMATCH: dom = ErrorDomain::DATA; break;
    case ACS_ERR_IO: dom = ErrorDomain::IO; break;
    case ACS_ERR_CANCELLED: dom = ErrorDomain::CANCELLED; break;
    case ACS_ERR_BUDGET: dom = ErrorDomain::RESOURCE; break;
    case ACS_ERR_NOMEM: dom = ErrorDomain::RESOURCE; break;
    case ACS_ERR_UNSUPPORTED: dom = ErrorDomain::BACKEND; break;
    default: dom = ErrorDomain::INTERNAL;
  }
  return Result<void>::fail(Error(dom, std::string(what) + ": " + status_str(st)));
}

// ── 通用 session 模块适配器 ──
struct SessionModule : public IModule {
  ModuleDescriptor desc_;
  std::string config_;
  std::string manifest_;  // RT-008: 最近一次 execute 的 session inspect 摘要
  // 会话函数族
  acs_status (*fn_create)(const astrocs_host_services_v1*, acs_handle*);
  acs_status (*fn_validate)(acs_handle, acs_span_u8);
  acs_status (*fn_run)(acs_handle, acs_span_u8);
  acs_status (*fn_inspect)(acs_handle, acs_span_u8*);
  acs_status (*fn_destroy)(acs_handle);
  std::string (*fn_last_error)(acs_handle);  // RT-008: 会话 last_error（保留错误细节）
  uint32_t workers_ = 2;

  SessionModule(ModuleDescriptor desc,
                acs_status (*create)(const astrocs_host_services_v1*, acs_handle*),
                acs_status (*validate)(acs_handle, acs_span_u8),
                acs_status (*run)(acs_handle, acs_span_u8),
                acs_status (*inspect)(acs_handle, acs_span_u8*),
                acs_status (*destroy)(acs_handle),
                std::string (*last_error)(acs_handle))
      : desc_(std::move(desc)), fn_create(create), fn_validate(validate),
        fn_run(run), fn_inspect(inspect), fn_destroy(destroy),
        fn_last_error(last_error) {}

  const ModuleDescriptor& descriptor() const noexcept override { return desc_; }

  Result<void> validate_config(const std::string& config_json) override {
    HostSession hs;
    if (!hs.init(workers_)) {
      return Result<void>::fail(Error(ErrorDomain::RESOURCE,
          desc_.module_id + ": host services init failed"));
    }
    acs_handle h = nullptr;
    acs_status st = fn_create(&hs.host, &h);
    if (st != ACS_OK) return to_result(st, "session create");
    acs_span_u8 cfg;
    cfg.head.struct_size = sizeof(cfg);
    cfg.head.abi_version = ACS_ABI_VERSION_V1;
    cfg.count = static_cast<uint64_t>(config_json.size());
    cfg.data = const_cast<uint8_t*>(
        reinterpret_cast<const uint8_t*>(config_json.data()));
    st = fn_validate(h, cfg);
    if (st != ACS_OK) { fn_destroy(h); return to_result(st, "session validate"); }
    fn_destroy(h);
    return Result<void>::success();
  }

  Result<ModulePlan> plan(const std::string& node_id,
                          const std::string& config_json) override {
    config_ = config_json;  // RT-008: 保存 config，execute 用真实配置驱动 session
    ModulePlan p;
    p.node_id = node_id;
    p.work_units = 1;
    p.parallel_axes = {"tile"};
    p.cpu_heavy = desc_.execution_class == "cpu_heavy";
    return Result<ModulePlan>::ok(std::move(p));
  }

  Result<void> execute(RunContext& ctx) override {
    HostSession hs;
    if (!hs.init(workers_)) {
      return Result<void>::fail(Error(ErrorDomain::RESOURCE,
          desc_.module_id + ": host services init failed"));
    }
    acs_handle h = nullptr;
    acs_status st = fn_create(&hs.host, &h);
    if (st != ACS_OK) return to_result(st, "session create");
    acs_span_u8 cfg;
    cfg.head.struct_size = sizeof(cfg);
    cfg.head.abi_version = ACS_ABI_VERSION_V1;
    cfg.count = static_cast<uint64_t>(config_.size());
    cfg.data = const_cast<uint8_t*>(
        reinterpret_cast<const uint8_t*>(config_.data()));
    // RT-008: execute 先 validate（与旧 CLI 流程一致；拒绝面在 validate 层:
    // phase3 projection/center/scale 等 → PARAM/UNSUPPORTED，CLI 映射 ARGS(2)）
    st = fn_validate(h, cfg);
    if (st != ACS_OK) {
      // validate 阶段失败 = 配置/请求错 → DATA(CLI → 2)；UNSUPPORTED 也是显式拒(2)
      std::string why = fn_last_error ? fn_last_error(h) : "";
      if (why.empty()) why = "session validate: " + status_str(st);
      fn_destroy(h);
      return Result<void>::fail(Error(ErrorDomain::DATA, why));
    }
    st = fn_run(h, cfg);
    // RT-008: destroy 前捕获 session manifest（inspect 摘要；成功/失败都捕获，
    // 失败时 manifest 含 error_kind 供 CLI 按 04 合同映射退出码）
    {
      acs_span_u8 man{};
      if (fn_inspect(h, &man) == ACS_OK && man.data) {
        manifest_ = std::string(reinterpret_cast<char*>(man.data),
                                static_cast<size_t>(man.count));
        hs.host.allocator.free(hs.host.allocator.user_data, man.data);
      }
    }
    if (st != ACS_OK) { fn_destroy(h); return to_result(st, "session run"); }
    ctx.log(LogLevel::INFO, desc_.module_id, "execute OK");
    fn_destroy(h);
    return Result<void>::success();
  }

  Result<std::string> inspect() override {
    HostSession hs;
    if (!hs.init(workers_)) {
      return Result<std::string>::fail(Error(ErrorDomain::RESOURCE,
          desc_.module_id + ": host services init failed"));
    }
    acs_handle h = nullptr;
    acs_status st = fn_create(&hs.host, &h);
    if (st != ACS_OK) {
      return Result<std::string>::fail(
          Error(ErrorDomain::INTERNAL, std::string("session create: ") + status_str(st)));
    }
    acs_span_u8 out{};
    st = fn_inspect(h, &out);
    std::string result;
    if (st == ACS_OK && out.data) {
      result.assign(reinterpret_cast<char*>(out.data),
                    static_cast<size_t>(out.count));
      hs.host.allocator.free(hs.host.allocator.user_data, out.data);
    }
    fn_destroy(h);
    if (st != ACS_OK) return Result<std::string>::fail(
        to_result(st, "session inspect").error());
    return Result<std::string>::ok(std::move(result));
  }

  // RT-008: 返回 execute 时捕获的 session manifest（不再重新 create session）
  Result<std::string> last_manifest() override {
    if (manifest_.empty()) {
      return Result<std::string>::fail(Error(ErrorDomain::DATA,
          desc_.module_id + ": no manifest captured (execute not run)"));
    }
    return Result<std::string>::ok(manifest_);
  }
};

ModuleDescriptor phase1_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase1.calibration";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"frames", "DATA-P1-FRAME", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"calibrated", "DATA-P1-CAL", false, UnitId::ADU, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P1-CAL-001";
  d.alg_id = "ALG-P1-CAL-001";
  d.data_id = "DATA-P1-CAL";
  d.api_id = "API-P1-001";
  d.test_id = "TEST-P1-CAL-001";
  return d;
}

ModuleDescriptor phase2_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.resample";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"calibrated", "DATA-P1-CAL", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"resampled", "DATA-P2-RES", false, UnitId::ADU, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P2-RES-001";
  d.alg_id = "ALG-P2-RES-001";
  d.data_id = "DATA-P2-RES";
  d.api_id = "API-P2-001";
  d.test_id = "TEST-P2-RES-001";
  return d;
}

ModuleDescriptor phase3_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase3.resample";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"hips", "DATA-HIPS-001", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"tile", "DATA-TILE-001", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::HEALPIX},
  };
  d.sci_id = "SCI-P3-RES-001";
  d.alg_id = "ALG-P3-RES-001";
  d.data_id = "DATA-TILE-001";
  d.api_id = "API-P3-001";
  d.test_id = "TEST-P3-RES-001";
  return d;
}

// ---- P3-006 (G6): Canonical Phase3 IR 链子模块 descriptor ----
// source→properties→wcs→resample→writer→verify; 端口 DATA/单位/Artifact ID 完整。
// 工厂委托 P3Api session adapter(一站式执行)。

ModuleDescriptor p3_properties_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase3.properties";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"hips", "DATA-HIPS-001", true, UnitId::ADU, CoordinateFrame::HEALPIX},
      {"props", "DATA-P3-PROPS", false, UnitId::DIMENSIONLESS, CoordinateFrame::HEALPIX},
  };
  d.sci_id = "SCI-P3-PROPS-001";
  d.alg_id = "ALG-P3-001";
  d.data_id = "DATA-P3-PROPS";
  d.api_id = "API-P3-001";
  d.test_id = "TEST-P3-PROPS-001";
  return d;
}

ModuleDescriptor p3_wcs_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase3.wcs";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"props", "DATA-P3-PROPS", true, UnitId::DIMENSIONLESS, CoordinateFrame::HEALPIX},
      {"wcs_plan", "DATA-P3-WCS", false, UnitId::DEGREE, CoordinateFrame::ICRS},
  };
  d.sci_id = "SCI-P3-WCS-001";
  d.alg_id = "ALG-P3-002";
  d.data_id = "DATA-P3-WCS";
  d.api_id = "API-P3-001";
  d.test_id = "TEST-P3-WCS-001";
  return d;
}

ModuleDescriptor p3_resample2_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase3.resample2";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"wcs_plan", "DATA-P3-WCS", true, UnitId::DEGREE, CoordinateFrame::ICRS},
      {"hips", "DATA-HIPS-001", true, UnitId::ADU, CoordinateFrame::HEALPIX},
      {"resampled", "DATA-P3-RES", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P3-RES-001";
  d.alg_id = "ALG-P3-003";
  d.data_id = "DATA-P3-RES";
  d.api_id = "API-P3-001";
  d.test_id = "TEST-P3-RES-001";
  return d;
}

ModuleDescriptor p3_writer_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase3.writer";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "io";
  d.parallel_ok = false;
  d.ports = {
      {"resampled", "DATA-P3-RES", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
      {"fits", "DATA-P3-FITS", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P3-WR-001";
  d.alg_id = "ALG-P3-004";
  d.data_id = "DATA-P3-FITS";
  d.api_id = "API-P3-001";
  d.test_id = "TEST-P3-WR-001";
  return d;
}

ModuleDescriptor p3_verify_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase3.verify";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "io";   // 读回校验非计算 heavy(heavy+serial 资源门禁止)
  d.parallel_ok = false;
  d.ports = {
      {"fits", "DATA-P3-FITS", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
      {"verified", "DATA-P3-VER", false, UnitId::DIMENSIONLESS, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P3-VER-001";
  d.alg_id = "ALG-P3-005";
  d.data_id = "DATA-P3-VER";
  d.api_id = "API-P3-001";
  d.test_id = "TEST-P3-VER-001";
  return d;
}

ModuleDescriptor p1_cosmetic_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase1.cosmetic";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"calibrated", "DATA-P1-CAL", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"cleaned", "DATA-P1-COSMETIC", false, UnitId::ADU, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P1-COS-001";
  d.alg_id = "ALG-P1-COS-001";
  d.data_id = "DATA-P1-COSMETIC";
  d.api_id = "API-P1-002";
  d.test_id = "TEST-P1-COS-001";
  return d;
}

ModuleDescriptor p1_star_psf_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase1.star-psf";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"cleaned", "DATA-P1-COSMETIC", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"sources", "DATA-P1-SOURCES", false, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
      {"psf", "DATA-P1-PSF", false, UnitId::DIMENSIONLESS, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P1-PSF-001";
  d.alg_id = "ALG-002";            // wcs-psf-batch kernel
  d.data_id = "DATA-P1-SOURCES";
  d.api_id = "API-P1-003";
  d.test_id = "TEST-P1-PSF-001";
  return d;
}

ModuleDescriptor p1_wcs_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase1.wcs-platesolve";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"sources", "DATA-P1-SOURCES", true, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
      {"wcs", "DATA-P1-WCS", false, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
  };
  d.sci_id = "SCI-P1-WCS-001";
  d.alg_id = "ALG-002";            // wcs-psf-batch kernel
  d.data_id = "DATA-P1-WCS";
  d.api_id = "API-P1-004";
  d.test_id = "TEST-P1-WCS-001";
  return d;
}

ModuleDescriptor p1_photometry_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase1.photometry";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"psf", "DATA-P1-PSF", true, UnitId::DIMENSIONLESS, CoordinateFrame::PIXEL},
      {"sources", "DATA-P1-SOURCES", true, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
      {"fluxes", "DATA-P1-FLUX", false, UnitId::ELECTRON, CoordinateFrame::ICRS},
  };
  d.sci_id = "SCI-P1-PHOT-001";
  d.alg_id = "ALG-002";            // wcs-psf-batch kernel
  d.data_id = "DATA-P1-FLUX";
  d.api_id = "API-P1-005";
  d.test_id = "TEST-P1-PHOT-001";
  return d;
}

ModuleDescriptor p1_noise_snr_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase1.noise-snr";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"fluxes", "DATA-P1-FLUX", true, UnitId::ELECTRON, CoordinateFrame::ICRS},
      {"snr", "DATA-P1-SNR", false, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
  };
  d.sci_id = "SCI-P1-SNR-001";
  d.alg_id = "ALG-004";            // noise-snr-reductions kernel
  d.data_id = "DATA-P1-SNR";
  d.api_id = "API-P1-006";
  d.test_id = "TEST-P1-SNR-001";
  return d;
}

ModuleDescriptor p1_drizzle_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase1.drizzle";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"calibrated", "DATA-P1-CAL", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"stacked", "DATA-P1-STACK", false, UnitId::ADU, CoordinateFrame::ICRS},
  };
  d.sci_id = "SCI-P1-DRIZ-001";
  d.alg_id = "ALG-005";            // drizzle-* kernels
  d.data_id = "DATA-P1-STACK";
  d.api_id = "API-P1-007";
  d.test_id = "TEST-P1-DRIZ-001";
  return d;
}

ModuleDescriptor p1_writer_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase1.writer";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "io";
  d.parallel_ok = false;
  d.ports = {
      {"stacked", "DATA-P1-STACK", true, UnitId::ADU, CoordinateFrame::ICRS},
      {"fits", "DATA-P1-FITS", false, UnitId::ADU, CoordinateFrame::ICRS},
  };
  d.sci_id = "SCI-P1-WR-001";
  d.alg_id = "ALG-P1-WR-001";
  d.data_id = "DATA-P1-FITS";
  d.api_id = "API-P1-008";
  d.test_id = "TEST-P1-WR-001";
  return d;
}

// ---- P2-006 (G5): Canonical Phase2 IR 7 节点链子模块 descriptor ----
// 节点链: coverage → sample → upm_fit → upm_apply → reject → integrate → write。
// 各端口 DATA/单位/Artifact ID 完整；工厂委托 P2Api session adapter(一站式执行)。
// 静态图语义：IR 每节点描述一个 pipeline 阶段；运行时执行委托同一 P2 session。

ModuleDescriptor p2_coverage_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.coverage";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"calibrated", "DATA-P2-CAL", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"coverage", "DATA-P2-COV", false, UnitId::DIMENSIONLESS, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P2-COV-001";
  d.alg_id = "ALG-P2-COV-001";
  d.data_id = "DATA-P2-COV";
  d.api_id = "API-P2-001";
  d.test_id = "TEST-P2-COV-001";
  return d;
}

ModuleDescriptor p2_sample_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.sample";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"coverage", "DATA-P2-COV", true, UnitId::DIMENSIONLESS, CoordinateFrame::PIXEL},
      {"samples", "DATA-P2-SMP", false, UnitId::ADU, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P2-SMP-001";
  d.alg_id = "ALG-P2-SMP-001";
  d.data_id = "DATA-P2-SMP";
  d.api_id = "API-P2-001";
  d.test_id = "TEST-P2-SMP-001";
  return d;
}

ModuleDescriptor p2_upm_fit_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.upm-fit";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"samples", "DATA-P2-SMP", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"upm_model", "DATA-P2-UPM", false, UnitId::ADU, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P2-UPM-001";
  d.alg_id = "ALG-P2-UPM-001";
  d.data_id = "DATA-P2-UPM";
  d.api_id = "API-P2-001";
  d.test_id = "TEST-P2-UPM-001";
  return d;
}

ModuleDescriptor p2_upm_apply_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.upm-apply";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"upm_model", "DATA-P2-UPM", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"calibrated_frames", "DATA-P2-CAL", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"corrected", "DATA-P2-COR", false, UnitId::ADU, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P2-UPM-002";
  d.alg_id = "ALG-P2-UPM-002";
  d.data_id = "DATA-P2-COR";
  d.api_id = "API-P2-001";
  d.test_id = "TEST-P2-UPM-002";
  return d;
}

ModuleDescriptor p2_reject_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.reject";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"corrected", "DATA-P2-COR", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"accepted_mask", "DATA-P2-REJ", false, UnitId::DIMENSIONLESS, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P2-REJ-001";
  d.alg_id = "ALG-P2-REJ-001";
  d.data_id = "DATA-P2-REJ";
  d.api_id = "API-P2-001";
  d.test_id = "TEST-P2-REJ-001";
  return d;
}

ModuleDescriptor p2_integrate_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.integrate";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  d.ports = {
      {"accepted_mask", "DATA-P2-REJ", true, UnitId::DIMENSIONLESS, CoordinateFrame::PIXEL},
      {"corrected", "DATA-P2-COR", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"integrated", "DATA-P2-INT", false, UnitId::ADU, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P2-INT-001";
  d.alg_id = "ALG-P2-INT-001";
  d.data_id = "DATA-P2-INT";
  d.api_id = "API-P2-001";
  d.test_id = "TEST-P2-INT-001";
  return d;
}

ModuleDescriptor p2_write_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.write";
  d.version = "1.0.0";
  d.abi = "c++17";
  d.execution_class = "io";
  d.parallel_ok = false;
  d.ports = {
      {"integrated", "DATA-P2-INT", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"mosaic", "DATA-P2-RES", false, UnitId::ADU, CoordinateFrame::PIXEL},
  };
  d.sci_id = "SCI-P2-WR-001";
  d.alg_id = "ALG-P2-WR-001";
  d.data_id = "DATA-P2-RES";
  d.api_id = "API-P2-001";
  d.test_id = "TEST-P2-WR-001";
  return d;
}


template <typename T>
std::unique_ptr<IModule> make_session_module(ModuleDescriptor desc) {
  return std::make_unique<SessionModule>(std::move(desc), T::create, T::validate,
                                         T::run, T::inspect, T::destroy, T::last_error);
}

struct P1Api {
  static acs_status create(const astrocs_host_services_v1* h, acs_handle* o) { return p1_session_create(h, o); }
  static acs_status validate(acs_handle h, acs_span_u8 c) { return p1_session_validate(h, c); }
  static acs_status run(acs_handle h, acs_span_u8 c) { return p1_session_run(h, c, 0); }
  static acs_status inspect(acs_handle h, acs_span_u8* o) { return p1_session_inspect(h, o); }
  static acs_status destroy(acs_handle h) { return p1_session_destroy(h); }
  static std::string last_error(acs_handle h) { return phase1::last_error(h); }
};
struct P2Api {
  static acs_status create(const astrocs_host_services_v1* h, acs_handle* o) { return p2_session_create(h, o); }
  static acs_status validate(acs_handle h, acs_span_u8 c) { return p2_session_validate(h, c); }
  static acs_status run(acs_handle h, acs_span_u8 c) { return p2_session_run(h, c); }
  static acs_status inspect(acs_handle h, acs_span_u8* o) { return p2_session_inspect(h, o); }
  static acs_status destroy(acs_handle h) { return p2_session_destroy(h); }
  static std::string last_error(acs_handle h) { return phase2::last_error(h); }
};
struct P3Api {
  static acs_status create(const astrocs_host_services_v1* h, acs_handle* o) { return p3_session_create(h, o); }
  static acs_status validate(acs_handle h, acs_span_u8 c) { return p3_session_validate(h, c); }
  static acs_status run(acs_handle h, acs_span_u8 c) { return p3_session_run(h, c); }
  static acs_status inspect(acs_handle h, acs_span_u8* o) { return p3_session_inspect(h, o); }
  static acs_status destroy(acs_handle h) { return p3_session_destroy(h); }
  static std::string last_error(acs_handle h) { return phase3::last_error(h); }
};

}  // namespace

// RT-008: cfitsio 首次初始化 shim（core 不 include cfitsio 头，避免依赖图污染）
extern "C" void astrocs_cfitsio_ensure_initialized(void);

Result<void> register_phase_modules(ModuleRegistry& registry) {
  // RT-008: cfitsio 首次初始化在单线程阶段完成（Runtime 并行 worker 并发首用会数据竞争）。
  astrocs_cfitsio_ensure_initialized();

  // Phase1
  auto d1 = phase1_descriptor();
  auto r1 = registry.register_module(d1);
  if (r1.failed()) return r1;
  auto f1 = registry.register_factory(
      d1.module_id, [d1]() { return make_session_module<P1Api>(d1); });
  if (f1.failed()) return f1;

  // Phase2
  auto d2 = phase2_descriptor();
  auto r2 = registry.register_module(d2);
  if (r2.failed()) return r2;
  auto f2 = registry.register_factory(
      d2.module_id, [d2]() { return make_session_module<P2Api>(d2); });
  if (f2.failed()) return f2;

  // Phase3
  auto d3 = phase3_descriptor();
  auto r3 = registry.register_module(d3);
  if (r3.failed()) return r3;
  auto f3 = registry.register_factory(
      d3.module_id, [d3]() { return make_session_module<P3Api>(d3); });
  if (f3.failed()) return f3;

  // P1-001 (G4): 8 类 Phase1 模块注册。工厂委托 p1_session 兼容 adapter
  // (Session 仅兼容 adapter, 内部委托 Runtime, 无第二调度顺序)。
  // calibration 已注册; 补 cosmetic/star-psf/wcs-platesolve/photometry/noise-snr/
  // drizzle/writer 共 7 个。
  const ModuleDescriptor p1_more[] = {
      p1_cosmetic_descriptor(),   p1_star_psf_descriptor(),
      p1_wcs_descriptor(),        p1_photometry_descriptor(),
      p1_noise_snr_descriptor(),  p1_drizzle_descriptor(),
      p1_writer_descriptor(),
  };
  for (const auto& d : p1_more) {
    auto rr = registry.register_module(d);
    if (rr.failed()) return rr;
    auto ff = registry.register_factory(
        d.module_id, [d]() { return make_session_module<P1Api>(d); });
    if (ff.failed()) return ff;
  }

  // P2-006 (G5): Canonical Phase2 IR 7 节点链子模块注册。
  // coverage→sample→upm_fit→upm_apply→reject→integrate→write; 工厂委托 P2Api
  // session adapter(与 phase2.resample 同一调度, 无第二调度顺序)。
  const ModuleDescriptor p2_chain[] = {
      p2_coverage_descriptor(),   p2_sample_descriptor(),
      p2_upm_fit_descriptor(),    p2_upm_apply_descriptor(),
      p2_reject_descriptor(),     p2_integrate_descriptor(),
      p2_write_descriptor(),
  };
  for (const auto& d : p2_chain) {
    auto rr = registry.register_module(d);
    if (rr.failed()) return rr;
    auto ff = registry.register_factory(
        d.module_id, [d]() { return make_session_module<P2Api>(d); });
    if (ff.failed()) return ff;
  }
  // P3-006 (G6): Canonical Phase3 IR 链子模块注册(source→properties→wcs→resample→writer→verify)
  const ModuleDescriptor p3_chain[] = {
      p3_properties_descriptor(), p3_wcs_descriptor(),
      p3_resample2_descriptor(),  p3_writer_descriptor(),
      p3_verify_descriptor(),
  };
  for (const auto& d : p3_chain) {
    auto rr = registry.register_module(d);
    if (rr.failed()) return rr;
    auto ff = registry.register_factory(
        d.module_id, [d]() { return make_session_module<P3Api>(d); });
    if (ff.failed()) return ff;
  }

  return Result<void>::success();
}

}  // namespace astrocs::core
