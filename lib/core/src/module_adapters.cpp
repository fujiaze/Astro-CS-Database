// RT-005 可执行模块适配器：Phase1/2/3 session → IModule 工厂
// P1-001 (attempt 2): Phase1 8 类节点唯一真实 operation 委托（ARCH-P0-001 整改）:
//   calibration    → ac_calibrate_frame     (lib/calibration C ABI)
//   cosmetic       → ac_correct_frame       (lib/calibration C ABI)
//   star-psf       → StarDetector::detect    (lib/phase1/stars) + StarSource
//                    fwhm_px/ellipticity 作 PSF 特性输出（不接 dpsf_fit_batch）
//   wcs-platesolve → WcsTan::pix2sky         (lib/phase1/wcs; 配置/初始 WCS
//                    像素→天球投影, 标定语义; 真实求解器接线归各 IMPL 任务)
//   photometry     → Photometer::measure     (lib/phase1/photometry)
//   noise-snr      → NoiseModel::estimate    (lib/phase1/noise)
//   drizzle        → hp_drizzle_run          (lib/drizzle 静态库)
//   writer         → aio_write_fits          (lib/astro_image_io)
// 子节点一律不调用完整 phase_session_run; operation/entry 名与
// runtime/pipeline/module_ports.registry.json 冻结绑定表一致。
#include "astrocs/core/module_adapters.h"

#include "astrocs/common_abi_v1.h"
#include "astrocs/core/context.h"

#include "astro_calibration.h"
#include "astro_image_io.h"
#include "hp_drizzle_api.h"

// P1-001 口径更新: 真实求解器/拟合器/HiPS writer 生产头（模块库零 diff 只读调用）
#include "dynamic_psf.h"     // lib/dynamic_psf: dpsf_fit_batch_f64 (Moffat4 FP64)
#include "ipv_api.h"         // lib/plate_solve/cpp/ipv: 真实 plate-solve 求解链
#include "gaia_client.h"     // lib/gaia_xpsd_client: Gaia XPSD cone-search 客户端
#include "star_detector.h"   // lib/star_detector: sdet_create/sdet_detect_ex_f64 句柄
// 注: C++ StarDetector 类头与 sdet C 头同名——裸名 include 命中 lib/star_detector
// (lib/star_detector/include 先于 lib/phase1/stars), C++ 类头以相对路径显式引入。
#include "../../phase1/stars/star_detector.h"  // astrocs::phase1::StarDetector (C++)
#include "aio_hips.h"        // lib/astro_image_io: IVOA HiPS 标准写链
#include "aio_healpix_io.h"  // lib/astro_image_io: HISS 读面(inspect/read_tile)

#include "photometer.h"
#include "noise_model.h"
#include "wcs_tan.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

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

  // P0 修复: workers 参数 = execute 路径的调用方 budget 权威（经 ThreadLease
  // 原子预留授权，不超卖）；validate/inspect 路径传缺省值 2（非调度上下文，
  // 仅影响 host budget 上限，不影响门禁语义）。
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
    // RT-003: 模块 host API 预算源 = RunContext 注入的唯一 ThreadBudget。
    // 先原子预留（RAII：session 结束/异常/取消统一归还），再用授权数初始化
    // host services（host budget.max_workers 与 ThreadBudget lease 同源，不超卖）。
    // 预算耗尽 → 空租约 → host 以 1 worker 串行执行（不伪造 ThreadLease::make）。
    // P0 修复(budget 注入链断裂): 调用方 budget 权威优先 —— ctx.budget() 是
    // Scheduler 注入的唯一 ThreadBudget（budget=CLI cli_affinity_cpu_count 的
    // 可用核数，与 MON-001 recorder.set_workers(budget,budget) 及 gate 侧
    // available_cpus/selected_workers 同源）。workers_=2 仅作 ThreadBudget 未
    // 注入上下文（validate_config/inspect/非调度测试）的缺省，不得截断权威
    // 预算；否则 session 层 worker 恒 2，多核宿主 compute+wall≥5s 任务恒
    // LowAvgCores/CpuP50Low（gate exit 10），违反"重计算禁止单线程"注入语义。
    const uint32_t host_workers =
        ctx.budget() ? ctx.budget()->budget() : workers_;
    ThreadLease lease = ctx.acquire_lease(host_workers);
    const uint32_t cap = lease.acquired() ? lease.size() : 1u;
    HostSession hs;
    if (!hs.init(cap)) {
      return Result<void>::fail(Error(ErrorDomain::RESOURCE,
          desc_.module_id + ": host services init failed"));
    }
    // RT-006: provider 真实观测 —— host services 初始化成功 = 当前已接线的
    // 唯一 CPU 计算后端（baseline）实际可用；置位后节点 end/worker 事件携带。
    // 非 config 值冒充：仅在 host init（真实后端探测/初始化）成功路径上置位。
    ctx.set_provider("baseline");
    ctx.record_trace([&] {
      TraceEvent e;
      e.type = TraceEventType::PROVIDER_ENTER;
      e.node_id = ctx.current_node();
      e.module_id = desc_.module_id;
      e.provider = "baseline";
      e.kernel_id = desc_.alg_id;
      e.workers = cap;
      e.granted_workers = host_workers;
      return e;
    }());
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

// ══ P1-001 (attempt 2): Phase1 真实节点 operation 实现 ══════════════════════
// 唯一真实 operation 委托（见文件头映射表）；子节点禁止调用完整 phase_session_run。
// manifest 携带 operation/entry 标记（与 runtime/pipeline/module_ports.registry.json
// 冻结绑定表一致），typed artifact 落盘 config.output_dir。

using Json = nlohmann::json;

// aio 图像 RAII（IO-002 canonical deleter; 禁裸 free）
struct P1Image {
  AIOImageData* p = nullptr;
  P1Image() = default;
  explicit P1Image(AIOImageData* q) : p(q) {}
  ~P1Image() { if (p) aio_free_image_data(p); }
  P1Image(const P1Image& o) = delete;
  P1Image& operator=(const P1Image& o) = delete;
  // 转移语义（持有唯一性; 释放旧指针）
  P1Image(P1Image&& o) noexcept : p(o.p) { o.p = nullptr; }
  P1Image& operator=(P1Image&& o) noexcept {
    if (this != &o) {
      if (p) aio_free_image_data(p);
      p = o.p;
      o.p = nullptr;
    }
    return *this;
  }
  bool ok() const { return p != nullptr; }
  int w() const { return aio_get_geometry(p).width; }
  int h() const { return aio_get_geometry(p).height; }
  float* px() const { return aio_get_pixel_data(p); }
};

P1Image p1_read_image(const std::string& path) {
  return P1Image(aio_read(path.c_str()));
}

// 帧完整性守卫（fail-closed, 补 aio_read 数据缺字 WARN 放行的缝隙）:
// AIOImageData 无部分读标志 ABI; 以可落盘事实为准——文件体积必须 ≥ 头 2880
// 字节 + 像素域 BITPIX=-32 字节数（截断坏帧在此确定性拒绝; 与 U6/U3 "truncated
// input must not complete" 语义同源, fail-closed 不留伪产物）。
bool p1_image_sane(const P1Image& im, const std::string& path) {
  if (!im.ok() || im.w() <= 0 || im.h() <= 0 || im.px() == nullptr) return false;
  const uint64_t need =
      2880ull + static_cast<uint64_t>(im.w()) * static_cast<uint64_t>(im.h()) * 4ull;
  std::error_code ec;
  const auto sz = std::filesystem::file_size(std::filesystem::u8path(path), ec);
  return !ec && static_cast<uint64_t>(sz) >= need;
}

// config 值读取（对齐 p1_session validate 合同: number|bool 均合法; 错型回退默认,
// 不抛——validate 面拒绝合同外结构, run 面不因错型 terminate）
bool p1_has(const Json& c, const char* key) {
  const auto it = c.find(key);
  return it != c.end() && !it->is_null();
}
bool p1_flag(const Json& c, const char* key, bool dflt) {
  const auto it = c.find(key);
  if (it == c.end() || it->is_null()) return dflt;
  if (it->is_boolean()) return it->get<bool>();
  if (it->is_number_unsigned()) return it->get<uint64_t>() != 0;
  if (it->is_number_integer()) return it->get<int64_t>() != 0;
  if (it->is_number_float()) return it->get<double>() != 0.0;
  return dflt;
}
double p1_num(const Json& c, const char* key, double dflt) {
  const auto it = c.find(key);
  if (it == c.end() || it->is_null() || !it->is_number()) return dflt;
  return it->get<double>();
}
int p1_int(const Json& c, const char* key, int dflt) {
  const auto it = c.find(key);
  if (it == c.end() || it->is_null()) return dflt;
  if (it->is_boolean()) return it->get<bool>() ? 1 : 0;
  if (it->is_number_integer()) {
    const int64_t v = it->get<int64_t>();
    return (v >= INT64_C(-2147483648) && v <= INT64_C(2147483647))
               ? static_cast<int>(v) : dflt;
  }
  if (it->is_number_unsigned()) {
    const uint64_t v = it->get<uint64_t>();
    return v <= UINT64_C(2147483647) ? static_cast<int>(v) : dflt;
  }
  if (it->is_number_float()) {
    const double d = it->get<double>();
    return (d >= -2147483648.0 && d <= 2147483647.0)
               ? static_cast<int>(d) : dflt;  // 截断语义（p1_session 同款）
  }
  return dflt;
}

// 文件名基名（跨分隔符; p1_session 同款）
std::string p1_base_name(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

// 节点输入帧路径: 优先 cal 节点产物 calibrated_<base>（节点链约定）, 无则原帧
std::string p1_calibrated_path(const Json& doc, const std::string& light) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  const std::string cand = out_dir + "/calibrated_" + p1_base_name(light);
  std::error_code ec;
  if (std::filesystem::exists(std::filesystem::u8path(cand), ec)) return cand;
  return light;
}

bool p1_write_text(const std::string& path, const std::string& text) {
  std::ofstream f(std::filesystem::u8path(path), std::ios::binary);
  if (!f) return false;
  f << text;
  return f.good();
}

// w*h 像素数（溢出 checked）
bool p1_wh_pixels(int w, int h, uint64_t* out) {
  if (w <= 0 || h <= 0) return false;
  const uint64_t a = static_cast<uint64_t>(w);
  const uint64_t b = static_cast<uint64_t>(h);
  if (a > UINT64_MAX / b) return false;
  *out = a * b;
  return true;
}

// ── op: calibrate（唯一真实入口 ac_calibrate_frame; 语义对齐 p1_session calibrate 阶段）──
Result<void> p1_op_calibrate(const Json& doc, Json* man) {
  std::vector<std::string> masters;
  for (const char* k : {"master_bias", "master_dark", "master_flat"})
    if (p1_has(doc, k)) masters.push_back(doc[k].get<std::string>());
  std::vector<std::string> lights;
  for (const auto& l : doc["input_lights"]) lights.push_back(l.get<std::string>());
  const std::string out_dir = doc.value("output_dir", std::string("."));

  // io_read（取消点=文件粒度语义保留: 逐文件读校验; 数据缺字坏帧 → IO fail-closed）
  (*man)["stages"] = Json::array();
  Json& st_rd = (*man)["stages"].emplace_back(Json{{"name", "io_read"}, {"status", "running"}});
  for (const auto& p : masters) {
    P1Image im = p1_read_image(p);
    if (!p1_image_sane(im, p)) {
      (*man)["error_kind"] = "input";
      st_rd["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read master: " + p));
    }
  }
  for (const auto& lp : lights) {
    P1Image im = p1_read_image(lp);
    if (!p1_image_sane(im, lp)) {
      (*man)["error_kind"] = "input";
      st_rd["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read light: " + lp));
    }
  }
  st_rd["status"] = "ok";
  st_rd["files"] = static_cast<uint64_t>(masters.size() + lights.size());

  // calibrate（真实入口 ac_calibrate_frame, 每帧恰一次）
  Json& st_cal = (*man)["stages"].emplace_back(Json{{"name", "calibrate"}, {"status", "running"}});
  P1Image bias, dark, flat;
  if (p1_has(doc, "master_bias")) bias = p1_read_image(doc["master_bias"].get<std::string>());
  if (p1_has(doc, "master_dark")) dark = p1_read_image(doc["master_dark"].get<std::string>());
  if (p1_has(doc, "master_flat")) flat = p1_read_image(doc["master_flat"].get<std::string>());
  int W = -1, H = -1;
  for (const P1Image* im : {&bias, &dark, &flat}) {
    if (im->ok()) {
      if (W < 0) { W = im->w(); H = im->h(); }
      else if (im->w() != W || im->h() != H) {
        st_cal["status"] = "fail";
        return Result<void>::fail(Error(ErrorDomain::DATA, "master frame size mismatch"));
      }
    }
  }
  const bool dark_opt = doc.value("dark_optimization", false);
  const float k_fixed = doc.value("dark_scale_factor", 1.0f);
  uint32_t frames_ok = 0;
  Json per_frame = Json::array();
  Json artifacts = Json::array();
  for (const auto& lp : lights) {
    P1Image light = p1_read_image(lp);
    if (!light.ok()) {
      (*man)["error_kind"] = "input";
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read light: " + lp));
    }
    if (W >= 0 && (light.w() != W || light.h() != H)) {
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "light size mismatch vs masters: " + lp));
    }
    W = light.w(); H = light.h();
    const uint64_t n = static_cast<uint64_t>(W) * static_cast<uint64_t>(H);
    std::vector<float> out(static_cast<size_t>(n), 0.0f);
    float actual_k = 0.0f;
    const int rc = ac_calibrate_frame(
        light.px(), W, H,
        dark.ok() ? dark.px() : nullptr,
        flat.ok() ? flat.px() : nullptr,
        bias.ok() ? bias.px() : nullptr,
        out.data(), dark_opt ? 1 : 0, k_fixed, &actual_k);
    if (rc != AC_OK) {
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(rc == AC_ERR_MEMORY ? ErrorDomain::RESOURCE
                                                          : ErrorDomain::INTERNAL,
          "ac_calibrate_frame failed: " + lp));
    }
    // 写出 calibrated_<base>（与 p1_session 命名约定一致; CLI 按此收集 artifact）
    P1Image wim = P1Image(aio_read_fits(lp.c_str()));
    if (!wim.ok()) {
      (*man)["error_kind"] = "input";
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO, "re-read failed: " + lp));
    }
    if (static_cast<uint64_t>(wim.w()) * static_cast<uint64_t>(wim.h()) != n) {
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::DATA, "re-read size mismatch: " + lp));
    }
    std::memcpy(wim.px(), out.data(), out.size() * sizeof(float));
    const std::string outp = out_dir + "/calibrated_" + p1_base_name(lp);
    if (aio_write_fits(wim.p, outp.c_str()) != 0) {
      (*man)["error_kind"] = "output";
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO, "write failed: " + outp));
    }
    ++frames_ok;
    artifacts.push_back(outp);
    per_frame.push_back(Json{{"input", p1_base_name(lp)},
                             {"output", "calibrated_" + p1_base_name(lp)},
                             {"dark_scale", dark_opt ? static_cast<double>(actual_k)
                                                     : static_cast<double>(k_fixed)}});
  }
  st_cal["status"] = "ok";
  st_cal["frames"] = frames_ok;
  st_cal["per_frame"] = per_frame;

  // io_write 校验（产物存在性 fail-closed）
  Json& st_wr = (*man)["stages"].emplace_back(Json{{"name", "io_write"}, {"status", "running"}});
  std::error_code ec;
  for (const auto& a : artifacts) {
    if (!std::filesystem::exists(std::filesystem::u8path(a.get<std::string>()), ec)) {
      st_wr["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO,
          "artifact missing after write: " + a.get<std::string>()));
    }
  }
  st_wr["status"] = "ok";
  (*man)["frames"] = frames_ok;
  (*man)["artifacts"] = artifacts;
  return Result<void>::success();
}

// ── op: cosmetic_correct（唯一真实入口 ac_correct_frame; enabled=false → 0 帧如实记录）──
Result<void> p1_op_cosmetic(const Json& doc, Json* man) {
  Json stages = Json::array();
  Json artifacts = Json::array();
  if (!p1_has(doc, "cosmetic") || !p1_flag(doc["cosmetic"], "enabled", true)) {
    stages.push_back(Json{{"name", "cosmetic"}, {"status", "ok"}, {"frames", 0},
                          {"mode", "disabled"}});
    (*man)["stages"] = stages;
    (*man)["frames"] = 0;
    (*man)["artifacts"] = artifacts;
    return Result<void>::success();
  }
  const Json& c = doc["cosmetic"];
  const float hot_sigma = static_cast<float>(p1_num(c, "hot_sigma", 5.0));
  const float cold_sigma = static_cast<float>(p1_num(c, "cold_sigma", 5.0));
  const int method = p1_int(c, "method", AC_METHOD_MEDIAN) == AC_METHOD_BILINEAR
                         ? AC_METHOD_BILINEAR : AC_METHOD_MEDIAN;
  const int mss = p1_int(c, "max_structure_size", 4);
  Json& st = stages.emplace_back(Json{{"name", "cosmetic"}, {"status", "running"}});
  int hot_total = 0, cold_total = 0;
  uint32_t frames = 0;
  for (const auto& l : doc["input_lights"]) {
    const std::string path = p1_calibrated_path(doc, l.get<std::string>());
    P1Image im = p1_read_image(path);
    if (!im.ok()) {
      (*man)["error_kind"] = "input";
      st["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + path));
    }
    std::vector<float> fixed(static_cast<size_t>(im.w()) * static_cast<size_t>(im.h()), 0.0f);
    int hot = 0, cold = 0;
    const int rc = ac_correct_frame(im.px(), im.w(), im.h(), nullptr, nullptr,
                                    fixed.data(), hot_sigma, cold_sigma, method,
                                    mss, &hot, &cold);
    if (rc != AC_OK) {
      st["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::INTERNAL,
          std::string("ac_correct_frame failed rc=") + std::to_string(rc)));
    }
    std::memcpy(im.px(), fixed.data(), fixed.size() * sizeof(float));
    if (aio_write_fits(im.p, path.c_str()) != 0) {
      (*man)["error_kind"] = "output";
      st["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO, "cosmetic write failed: " + path));
    }
    hot_total += hot;
    cold_total += cold;
    ++frames;
    artifacts.push_back(path);
  }
  st["status"] = "ok";
  st["frames"] = frames;
  st["hot_fixed"] = hot_total;
  st["cold_fixed"] = cold_total;
  (*man)["stages"] = stages;
  (*man)["frames"] = frames;
  (*man)["artifacts"] = artifacts;
  return Result<void>::success();
}

// ── op: detect_sources（真实检测+拟合链: lib/star_detector sdet 检测 →
//      star_det v1 [N,6] → lib/dynamic_psf dpsf_fit_batch_f64 Moffat4 批量
//      PSF 拟合（生产源零 diff; DPSF-PREC-105/FP64 双精度）; 输出
//      DATA-P1-SOURCES + DATA-P1-PSF(psf_params:FLOAT64[N,9])）──
Result<void> p1_op_star_psf(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  const astrocs::phase1::StarDetector det(5.0);
  Json frames = Json::array();
  std::vector<double> fwhm_xs, fwhm_ys, ells;
  int64_t n_valid_total = 0, n_total_total = 0;
  for (const auto& l : doc["input_lights"]) {
    const std::string path = p1_calibrated_path(doc, l.get<std::string>());
    P1Image im = p1_read_image(path);
    if (!im.ok()) {
      (*man)["error_kind"] = "input";
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + path));
    }
    auto r = det.detect(im.px(), im.w(), im.h());
    if (r.failed()) {
      return Result<void>::fail(r.error());
    }
    const astrocs::phase1::StarCatalog& cat = r.value();
    // star_det v1 检测视图 [N,6]: x/y/flux/mag/saturated/has_saturated
    // mag 由积分通量真实换算（-2.5log10, 零/负通量 → +99 如实标记）
    const size_t N = cat.sources.size();
    std::vector<double> dets(N * 6, 0.0);
    for (size_t i = 0; i < N; ++i) {
      const auto& s = cat.sources[i];
      dets[i * 6 + 0] = s.x;
      dets[i * 6 + 1] = s.y;
      dets[i * 6 + 2] = s.flux;
      dets[i * 6 + 3] = (s.flux > 0.0)
          ? -2.5 * std::log10(s.flux) : 99.0;
      dets[i * 6 + 4] = (s.quality & 1) ? 1.0 : 0.0;   // saturated
      dets[i * 6 + 5] = (cat.n_saturated > 0) ? 1.0 : 0.0;
    }
    // 真实 PSF 拟合: dpsf_fit_batch_f64（float32 检测帧 → double 全链拟合,
    // 数据保真升精度; 默认拟合参数）
    std::vector<double> psf_params(N * 9, 0.0);
    int n_valid = 0;
    if (N > 0) {
      std::vector<double> dbuf(static_cast<size_t>(im.w()) * static_cast<size_t>(im.h()));
      for (size_t i = 0; i < dbuf.size(); ++i) dbuf[i] = static_cast<double>(im.px()[i]);
      const int drc = dpsf_fit_batch_f64(
          dbuf.data(), im.w(), im.h(), dets.data(), static_cast<int>(N),
          nullptr, psf_params.data(), &n_valid);
      if (drc != 0) {
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "dpsf_fit_batch_f64 failed rc=" + std::to_string(drc)));
      }
      if (n_valid <= 0) {
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "dpsf_fit_batch_f64: 0/" + std::to_string(N) + " fits converged"));
      }
      for (int i = 0; i < n_valid; ++i) {
        // [7]=fwhm_x [8]=fwhm_y; sx=sigma_x → fwhm=2.3548*sx（由 9 列取 [7]/[8] 权威值）
        fwhm_xs.push_back(psf_params[static_cast<size_t>(i) * 9 + 7]);
        fwhm_ys.push_back(psf_params[static_cast<size_t>(i) * 9 + 8]);
        const double sx = psf_params[static_cast<size_t>(i) * 9 + 4];
        const double sy = psf_params[static_cast<size_t>(i) * 9 + 5];
        const double mx = std::max(sx, sy), mn = std::min(sx, sy);
        ells.push_back(mx > 0.0 ? 1.0 - mn / mx : 0.0);
      }
      n_valid_total += n_valid;
    }
    n_total_total += static_cast<int64_t>(N);
    Json sources = Json::array();
    for (const auto& s : cat.sources) {
      sources.push_back(Json{{"id", s.id}, {"x", s.x}, {"y", s.y},
                             {"flux", s.flux}, {"fwhm_px", s.fwhm_px},
                             {"ellipticity", s.ellipticity}, {"snr", s.snr},
                             {"quality", s.quality}});
    }
    Json psf_rows = Json::array();
    for (int i = 0; i < n_valid; ++i)
      psf_rows.push_back(Json{{"star_id", cat.sources[static_cast<size_t>(i)].id},
                              {"B", psf_params[static_cast<size_t>(i)*9+0]},
                              {"A", psf_params[static_cast<size_t>(i)*9+1]},
                              {"cx", psf_params[static_cast<size_t>(i)*9+2]},
                              {"cy", psf_params[static_cast<size_t>(i)*9+3]},
                              {"sx", psf_params[static_cast<size_t>(i)*9+4]},
                              {"sy", psf_params[static_cast<size_t>(i)*9+5]},
                              {"theta", psf_params[static_cast<size_t>(i)*9+6]},
                              {"fwhm_x", psf_params[static_cast<size_t>(i)*9+7]},
                              {"fwhm_y", psf_params[static_cast<size_t>(i)*9+8]}});
    frames.push_back(Json{{"file", p1_base_name(path)},
                          {"n_detected", cat.n_detected},
                          {"n_saturated", cat.n_saturated},
                          {"n_edge", cat.n_edge},
                          {"background", cat.background},
                          {"noise_sigma", cat.noise_sigma},
                          {"n_psf_valid", n_valid},
                          {"sources", sources},
                          {"psf_params", psf_rows}});
  }
  auto median = [](std::vector<double> v) -> double {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
  };
  const std::string src_path = out_dir + "/p1_sources.json";
  const std::string psf_path = out_dir + "/p1_psf.json";
  Json cat_out = Json{{"schema", "DATA-P1-SOURCES"}, {"frames", frames}};
  Json psf_out = Json{{"schema", "DATA-P1-PSF"},
                      {"detection_schema", DPSF_STAR_DET_SCHEMA_V1},
                      {"params_schema", DPSF_PSF_PARAMS_SCHEMA},
                      {"entry", "dpsf_fit_batch_f64"},
                      {"n_sources", n_total_total},
                      {"n_valid", n_valid_total},
                      {"median_fwhm_x_px", median(fwhm_xs)},
                      {"median_fwhm_y_px", median(fwhm_ys)},
                      {"median_ellipticity", median(ells)}};
  if (!p1_write_text(src_path, cat_out.dump(2)) || !p1_write_text(psf_path, psf_out.dump(2))) {
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  }
  (*man)["frames"] = static_cast<uint64_t>(frames.size());
  (*man)["n_sources"] = n_total_total;
  (*man)["n_psf_valid"] = n_valid_total;
  (*man)["sources_artifact"] = src_path;
  (*man)["psf_artifact"] = psf_path;
  return Result<void>::success();
}

// ── op: plate_solve（真实求解器链: lib/plate_solve ipv——sdet 句柄 +
//      gaia_client 句柄注入 IPVSolver → ipv_solve_from_memory_with_callback_d
//      FP64 全链解算 → IpvWcsResult(CD/CRVAL/CRPIX/RMS) → WcsTan roundtrip
//      自检。生产源零 diff（ipv 非 Windows 平台为源内 stub: 求解必失败 →
//      节点 DATA fail-closed 如实报平台限制, Windows 侧即真实求解）──
Result<void> p1_op_wcs(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  if (!p1_has(doc, "input_lights") || doc["input_lights"].empty()) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "input_lights required"));
  }
  const Json& wc = doc.contains("wcs") && doc["wcs"].is_object() ? doc["wcs"] : Json::object();
  // 真实求解链必需参数: 初始指向/光学尺度/Gaia 数据目录（缺失显式拒绝, 禁 silent default）
  const double ra0 = p1_num(wc, "ra0", std::numeric_limits<double>::quiet_NaN());
  const double dec0 = p1_num(wc, "dec0", std::numeric_limits<double>::quiet_NaN());
  const double focal_mm = p1_num(wc, "focal_length_mm", std::numeric_limits<double>::quiet_NaN());
  const double pixel_um = p1_num(wc, "pixel_size_um", std::numeric_limits<double>::quiet_NaN());
  const std::string gaia_dir = wc.value("gaia_data_dir", std::string());
  if (std::isnan(ra0) || std::isnan(dec0) || std::isnan(focal_mm) ||
      std::isnan(pixel_um) || gaia_dir.empty()) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "wcs config requires ra0/dec0/focal_length_mm/pixel_size_um/gaia_data_dir "
        "(real ipv solve chain; no silent defaults)"));
  }
  const std::string frame0 = p1_calibrated_path(doc, doc["input_lights"][0].get<std::string>());
  P1Image im = p1_read_image(frame0);
  if (!im.ok()) {
    (*man)["error_kind"] = "input";
    return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + frame0));
  }
  // 资源 RAII（按 orchestrator PLATESOLVE 销毁顺序: ipv → sdet → gaia）
  StarDetectorHandle sdet = nullptr;
  GaiaClient* gaia = nullptr;
  void* ipv = nullptr;
  auto cleanup = [&]() {
    if (ipv) { ipv_solve_destroy(ipv); ipv = nullptr; }
    if (sdet) { sdet_destroy(sdet); sdet = nullptr; }
    if (gaia) { gaia_client_destroy(gaia); gaia = nullptr; }
  };
  // 1) StarDetector 句柄（A 线 orchestrator 同款默认: fitRadius=0 自动）
  SDetParams sp;
  std::memset(&sp, 0, sizeof(sp));
  sp.structureLayers = 5;
  sp.hotPixelFilterRadius = 2;
  sp.iterativeClipSigma = 5.0f;
  sp.iterativeMaxRounds = 3;
  sp.medianFilterDetail = 2;
  sp.maxStars = 2000;
  sp.fitRadius = 0;
  sp.fwhmClipSigma = 3.0f;
  sp.maxAxisRatio = 2.0f;
  sdet = sdet_create(&sp);
  if (!sdet) {
    cleanup();
    return Result<void>::fail(Error(ErrorDomain::DATA, "sdet_create failed"));
  }
  // 2) Gaia 客户端句柄（真实 XPSD 数据目录）
  gaia = gaia_client_create(gaia_dir.c_str());
  if (!gaia) {
    cleanup();
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "gaia_client_create failed (gaia_data_dir=" + gaia_dir + ")"));
  }
  // 3) IPVSolver + 句柄注入
  ipv = ipv_solve_create();
  if (!ipv) {
    cleanup();
    return Result<void>::fail(Error(ErrorDomain::DATA, "ipv_solve_create failed"));
  }
  ipv_set_gaia_handle(ipv, reinterpret_cast<intptr_t>(gaia));
  ipv_set_detector_handle(ipv, reinterpret_cast<intptr_t>(sdet));
  // 4) FP64 内存求解（double 图像全链不降级）
  std::vector<double> dbuf(static_cast<size_t>(im.w()) * static_cast<size_t>(im.h()));
  for (size_t i = 0; i < dbuf.size(); ++i) dbuf[i] = static_cast<double>(im.px()[i]);
  IpvParams ip;
  ipv_get_default_params(&ip);
  std::memset(ip.log_dir, 0, sizeof(ip.log_dir));  // 节点面禁写求解日志
  IpvWcsResult r;
  std::memset(&r, 0, sizeof(r));
  const int src = ipv_solve_from_memory_with_callback_d(
      ipv, dbuf.data(), im.w(), im.h(), ra0, dec0, focal_mm, pixel_um,
      &ip, nullptr, nullptr, &r);
  cleanup();
  if (src != 1 || r.success != 1) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        std::string("ipv_solve_from_memory_with_callback_d failed: ") +
        (r.error_msg[0] ? r.error_msg : "solver returned failure") +
        " (ipv 求解器链: 非 Windows 平台为生产源内建 stub, 平台限制如实上报)"));
  }
  // 5) 解算结果 → WcsTan roundtrip 自检（<1e-6 px 冻结合同）
  astrocs::phase1::WcsTan wcs;
  wcs.crpix1 = r.crpix[0]; wcs.crpix2 = r.crpix[1];
  wcs.crval1 = r.crval[0]; wcs.crval2 = r.crval[1];
  wcs.cd11 = r.cd[0]; wcs.cd12 = r.cd[1];
  wcs.cd21 = r.cd[2]; wcs.cd22 = r.cd[3];
  const int W = im.w(), H = im.h();
  std::vector<std::pair<double, double>> pts;
  const int step = std::max(1, std::max(W, H) / 8);
  for (int y = 0; y < H; y += step)
    for (int x = 0; x < W; x += step)
      pts.emplace_back(static_cast<double>(x), static_cast<double>(y));
  Json samples = Json::array();
  double max_rt = 0.0;
  for (const auto& [x, y] : pts) {
    double ra = 0.0, dec = 0.0, bx = 0.0, by = 0.0;
    wcs.pix2sky(x, y, &ra, &dec);
    wcs.sky2pix(ra, dec, &bx, &by);
    const double rt = std::sqrt((bx - x) * (bx - x) + (by - y) * (by - y));
    if (rt > max_rt) max_rt = rt;
    samples.push_back(Json{{"x", x}, {"y", y}, {"ra", ra}, {"dec", dec},
                           {"roundtrip_px", rt}});
  }
  if (max_rt >= 1e-6) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "WcsTan roundtrip " + std::to_string(max_rt) + " px exceeds 1e-6 contract"));
  }
  const std::string out_path = out_dir + "/p1_wcs.json";
  Json wcs_out = Json{{"schema", "DATA-P1-WCS"},
                      {"solver", "ipv_solve_from_memory_with_callback_d"},
                      {"initial", false},
                      {"wcs", Json{{"crpix1", wcs.crpix1}, {"crpix2", wcs.crpix2},
                                   {"crval1", wcs.crval1}, {"crval2", wcs.crval2},
                                   {"cd11", wcs.cd11}, {"cd12", wcs.cd12},
                                   {"cd21", wcs.cd21}, {"cd22", wcs.cd22}}},
                      {"ctype1", std::string(r.ctype1)},
                      {"ctype2", std::string(r.ctype2)},
                      {"rms_px", r.rms_px},
                      {"rms_arcsec", r.rms_arcsec},
                      {"n_pairs", r.n_pairs},
                      {"trans_order", r.trans_order},
                      {"best_inliers", r.best_inliers},
                      {"sip_order", r.sip_order},
                      {"n_samples", samples.size()},
                      {"max_roundtrip_px", max_rt},
                      {"samples", samples}};
  if (!p1_write_text(out_path, wcs_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_pairs"] = r.n_pairs;
  (*man)["rms_px"] = r.rms_px;
  (*man)["n_samples"] = samples.size();
  (*man)["max_roundtrip_px"] = max_rt;
  (*man)["wcs_artifact"] = out_path;
  return Result<void>::success();
}

// ── op: measure_flux（唯一真实入口 Photometer::measure; 无源 → 如实空输出）──
Result<void> p1_op_photometry(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  const std::string src_path = out_dir + "/p1_sources.json";
  Json cat = Json::array();
  {
    std::error_code ec;
    std::ifstream f(std::filesystem::u8path(src_path), std::ios::binary);
    if (f) {
      try {
        Json j = Json::parse(std::string((std::istreambuf_iterator<char>(f)),
                                         std::istreambuf_iterator<char>()));
        cat = j.value("frames", Json::array());
      } catch (...) { cat = Json::array(); }
    }
  }
  const astrocs::phase1::Photometer phot;
  Json frames = Json::array();
  for (const auto& fr : cat) {
    const std::string file = fr.value("file", std::string());
    // sources.json 的 file 是 calibrated 基名; 逐帧读取（找不到则跳过该帧, 如实记录）
    std::string path = out_dir + "/" + file;
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::u8path(path), ec)) {
      frames.push_back(Json{{"file", file}, {"error", "frame not found"}});
      continue;
    }
    P1Image im = p1_read_image(path);
    if (!im.ok()) {
      (*man)["error_kind"] = "input";
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + path));
    }
    Json results = Json::array();
    for (const auto& s : fr.value("sources", Json::array())) {
      const double cx = s.value("x", 0.0), cy = s.value("y", 0.0);
      auto r = phot.measure(im.px(), im.w(), im.h(), cx, cy);
      if (r.failed()) {
        return Result<void>::fail(r.error());
      }
      const astrocs::phase1::PhotometryResult& pr = r.value();
      results.push_back(Json{{"id", s.value("id", std::string())},
                             {"x", cx}, {"y", cy},
                             {"flux", pr.flux}, {"flux_error", pr.flux_error},
                             {"background", pr.background}, {"snr", pr.snr},
                             {"valid", pr.valid}, {"failure_reason", pr.failure_reason}});
    }
    frames.push_back(Json{{"file", file}, {"results", results}});
  }
  const std::string out_path = out_dir + "/p1_flux.json";
  Json flux_out = Json{{"schema", "DATA-P1-FLUX"}, {"frames", frames}};
  if (!p1_write_text(out_path, flux_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_frames"] = frames.size();
  (*man)["flux_artifact"] = out_path;
  return Result<void>::success();
}

// ── op: estimate_snr（唯一真实入口 NoiseModel::estimate; SCI-NOISE-001 公式）──
Result<void> p1_op_noise(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  const astrocs::phase1::NoiseModel model;
  Json frames = Json::array();
  for (const auto& l : doc["input_lights"]) {
    const std::string path = p1_calibrated_path(doc, l.get<std::string>());
    P1Image im = p1_read_image(path);
    if (!im.ok()) {
      (*man)["error_kind"] = "input";
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + path));
    }
    const uint64_t n = static_cast<uint64_t>(im.w()) * static_cast<uint64_t>(im.h());
    std::vector<float> px(im.px(), im.px() + static_cast<size_t>(n));
    auto r = model.estimate(px);
    if (r.failed()) return Result<void>::fail(r.error());
    const astrocs::phase1::NoiseResult& nr = r.value();
    frames.push_back(Json{{"file", p1_base_name(path)},
                          {"variance", nr.variance}, {"ivar", nr.ivar},
                          {"sigma", nr.sigma}, {"background", nr.background},
                          {"valid", nr.valid}, {"reason", nr.reason}});
  }
  const std::string out_path = out_dir + "/p1_snr.json";
  Json snr_out = Json{{"schema", "DATA-P1-SNR"}, {"frames", frames}};
  if (!p1_write_text(out_path, snr_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_frames"] = frames.size();
  (*man)["snr_artifact"] = out_path;
  return Result<void>::success();
}

// ── op: drizzle_stack（唯一真实入口 hp_drizzle_run; nside 科学参数无缺省）──
Result<void> p1_op_drizzle(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  if (!p1_has(doc, "wcs") || !doc["wcs"].is_object())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "drizzle requires 'wcs' (HP DRIZZLE header KV source; 禁 silent default)"));
  const Json& wj = doc["wcs"];
  const bool has_drz = p1_has(doc, "drizzle") && doc["drizzle"].is_object();
  if (!has_drz || !p1_has(doc["drizzle"], "nside"))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "drizzle requires 'drizzle.nside' (科学参数禁 silent default)"));
  const Json& dj = doc["drizzle"];
  const int nside = p1_int(dj, "nside", 0);
  const int nested = p1_int(dj, "nested", 0);
  const double pixfrac = p1_num(dj, "pixfrac", 1.0);
  const int precision_mode = p1_int(dj, "precision_mode", 0);
  if (nside <= 0) return Result<void>::fail(Error(ErrorDomain::DATA, "nside must be > 0"));
  if (pixfrac <= 0.0 || pixfrac > 1.0)
    return Result<void>::fail(Error(ErrorDomain::DATA, "pixfrac must be in (0,1]"));

  if (!p1_has(doc, "input_lights") || doc["input_lights"].empty())
    return Result<void>::fail(Error(ErrorDomain::DATA, "input_lights required"));
  const std::string frame_path = p1_calibrated_path(doc, doc["input_lights"][0].get<std::string>());
  P1Image im = p1_read_image(frame_path);
  if (!im.ok()) {
    (*man)["error_kind"] = "input";
    return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + frame_path));
  }
  // PipelineFrame: data [H,W] f32 + header KV（hp_drizzle_run 合同: dims[0]=H, dims[1]=W）
  PipelineFrame* frame = aio_pipeline_frame_create();
  if (!frame) return Result<void>::fail(Error(ErrorDomain::RESOURCE, "frame create failed"));
  const int dims[2] = {im.h(), im.w()};
  int rc = aio_frame_add_block(frame, "data", AIO_BLOCK_FLOAT32, im.px(),
                               static_cast<int64_t>(im.w()) * static_cast<int64_t>(im.h()),
                               dims, 2, "p1 drizzle node input plane");
  if (rc == 0) {
    // header KV: WCS 7 参数 + 派生确定性字段
    struct KV { const char* k; char v[64]; };
    auto fmt = [](double d, char* b, size_t n) { std::snprintf(b, n, "%.17g", d); };
    char b1[64], b2[64], b3[64], b4[64], b5[64], b6[64], b7[64], b8[64];
    fmt(p1_num(wj, "crpix1", 0.0), b1, sizeof(b1));
    fmt(p1_num(wj, "crpix2", 0.0), b2, sizeof(b2));
    fmt(p1_num(wj, "crval1", 0.0), b3, sizeof(b3));
    fmt(p1_num(wj, "crval2", 0.0), b4, sizeof(b4));
    fmt(p1_num(wj, "cd11", 0.0), b5, sizeof(b5));
    fmt(p1_num(wj, "cd12", 0.0), b6, sizeof(b6));
    fmt(p1_num(wj, "cd21", 0.0), b7, sizeof(b7));
    fmt(p1_num(wj, "cd22", 0.0), b8, sizeof(b8));
    const KV kvs[] = {
        {"CRPIX1", ""}, {"CRPIX2", ""}, {"CRVAL1", ""}, {"CRVAL2", ""},
        {"CD1_1", ""}, {"CD1_2", ""}, {"CD2_1", ""}, {"CD2_2", ""},
        {"CDELT1", ""}, {"CDELT2", ""}, {"CROTA1", "0"}, {"CROTA2", "0"},
        {"CTYPE1", "RA---TAN"}, {"CTYPE2", "DEC--TAN"}, {"PRECISION", "0"},
        {"PHOTSCAL", ""}, {"PHOTAPPL", "1"},  // 测光校准元数据(仅记录; 中性 1.0)
    };
    (void)b1; (void)b2; (void)b3; (void)b4; (void)b5; (void)b6; (void)b7; (void)b8;
    // PHOTSCAL: 测光缩放因子(>0, 正式 Stage1 合同); 缺省 1.0=中性(无缩放)
    char b9[64];
    fmt(p1_num(dj, "photscal", 1.0), b9, sizeof(b9));
    for (const KV& kv : kvs) {
      std::string val = kv.v[0] != '\0' ? std::string(kv.v) : [&] {
        if (std::strcmp(kv.k, "CRPIX1") == 0) return std::string(b1);
        if (std::strcmp(kv.k, "CRPIX2") == 0) return std::string(b2);
        if (std::strcmp(kv.k, "CRVAL1") == 0) return std::string(b3);
        if (std::strcmp(kv.k, "CRVAL2") == 0) return std::string(b4);
        if (std::strcmp(kv.k, "CD1_1") == 0) return std::string(b5);
        if (std::strcmp(kv.k, "CD1_2") == 0) return std::string(b6);
        if (std::strcmp(kv.k, "CD2_1") == 0) return std::string(b7);
        if (std::strcmp(kv.k, "CD2_2") == 0) return std::string(b8);
        if (std::strcmp(kv.k, "CDELT1") == 0) return std::string(b5);
        if (std::strcmp(kv.k, "PHOTSCAL") == 0) return std::string(b9);
        return std::string(b8);  // CDELT2
      }();
      if (aio_frame_kv_set(frame, "header", kv.k, val.c_str()) != 0) {
        aio_pipeline_frame_destroy(frame);
        return Result<void>::fail(Error(ErrorDomain::IO,
            std::string("kv_set failed: ") + kv.k));
      }
    }
  }
  if (rc != 0) {
    aio_pipeline_frame_destroy(frame);
    return Result<void>::fail(Error(ErrorDomain::IO, "add data block failed"));
  }
  HpDrizzleResult res;
  std::memset(&res, 0, sizeof(res));
  const std::string hiss_path = out_dir + "/p1_stack.hiss";
  rc = hp_drizzle_run(frame, nside, nested, pixfrac, hiss_path.c_str(), &res,
                      precision_mode);
  aio_pipeline_frame_destroy(frame);
  if (rc != 0) {
    return Result<void>::fail(Error(ErrorDomain::IO,
        std::string("hp_drizzle_run failed: ") +
        (res.error_msg[0] ? res.error_msg : "(no detail)")));
  }
  const std::string out_path = out_dir + "/p1_stack.json";
  Json stack_out = Json{{"schema", "DATA-P1-STACK"},
                        {"nside", res.nside}, {"nested", res.nested},
                        {"pixfrac", res.pixfrac},
                        {"n_healpix_pixels", static_cast<int64_t>(res.n_healpix_pixels)},
                        {"n_source_pixels", static_cast<int64_t>(res.n_source_pixels)},
                        {"elapsed_sec", static_cast<double>(res.elapsed_sec)},
                        {"artifact", "p1_stack.hiss"},
                        {"entry", "hp_drizzle_run"}};
  if (!p1_write_text(out_path, stack_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_healpix_pixels"] = static_cast<int64_t>(res.n_healpix_pixels);
  (*man)["stack_artifact"] = out_path;
  return Result<void>::success();
}

// ── op: write_hips（真实 HiPS writer 链: 上游 drizzle 产物 p1_stack.hiss →
//      aio_hiss_inspect/read_tile_* → AstroSphereTileView →
//      aio_hips_product_begin/write_signal_support_tile/finalize（AIO-002
//      原子发布原语内建于 aio_hips 落盘路径）→ 标准化 HiPS
//      (IVOA 1.4 NESTED: signal/ support/ properties/MOC)。单帧语义:
//      covered_area = support>0 ? A_cell : 0（stacked 单帧全或无, p1_stack.json
//      登记 covered_area_model="support_x_A_cell"）──
Result<void> p1_op_writer(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  const std::string hiss_path = out_dir + "/p1_stack.hiss";
  const std::string stack_json = out_dir + "/p1_stack.json";
  std::error_code ec;
  if (!std::filesystem::exists(std::filesystem::u8path(hiss_path), ec)) {
    (*man)["error_kind"] = "input";
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream stack artifact missing: " + hiss_path + " (writer consumes drizzle output)"));
  }
  // HISS 头与 tile 清单
  uint32_t nside = 0; uint32_t tile_nside = 0; uint32_t depth = 0;
  uint32_t n_leaf_per_tile = 0; uint64_t n_tiles = 0; uint64_t n_pix_total = 0;
  char* meta_json = nullptr; uint64_t* tile_ipix = nullptr;
  if (aio_hiss_inspect(hiss_path.c_str(), &nside, &tile_nside, &depth,
                       &n_leaf_per_tile, &n_tiles, &n_pix_total,
                       &meta_json, &tile_ipix) != 0 || n_tiles == 0) {
    if (meta_json) aio_hio_free(meta_json);
    if (tile_ipix) aio_hio_free(tile_ipix);
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "aio_hiss_inspect failed: " + hiss_path));
  }
  // IVOA HiPS 标准合同: 叶级 nside>=512（aio_hips 硬校验同款）; 上游 drizzle
  // nside 过小 = 参数错误, fail-closed 不写伪产品。HISS 内部 tile_nside 可小于
  // 标准 tile（16×16 → 每 HISS tile 256 leaf px）, 由下方 NESTED 聚合展开。
  if (nside < 512 || (nside % tile_nside) != 0 || tile_nside == 0 ||
      (tile_nside & (tile_nside - 1)) != 0) {
    if (meta_json) aio_hio_free(meta_json);
    if (tile_ipix) aio_hio_free(tile_ipix);
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "HiPS requires nside>=512 and HISS tile_nside | nside (got nside=" +
        std::to_string(nside) + ", tile_nside=" + std::to_string(tile_nside) +
        "); upstream drizzle nside too small"));
  }
  // leaf order = log2(nside); A_cell = 4π/(12·nside²) sr（单帧全或无面积模型）
  uint32_t leaf_order = 0;
  for (uint32_t n = nside; n > 1; n /= 2) ++leaf_order;
  const double a_cell = 4.0 * 3.14159265358979323846 /
                        (12.0 * static_cast<double>(nside) * static_cast<double>(nside));
  // 产品集（signal+support; CFITSIO 标准 FITS + properties/MOC 由 finalize 聚合）
  AioHipsProductSet* ps = aio_hips_product_begin(
      out_dir.c_str(), nside, 512, AIO_HIPS_FLOAT32,
      AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
      "astrocs/phase1", "AstroCS Phase1 single-frame stack", nullptr,
      0.0, nullptr, 0);
  if (!ps) {
    if (meta_json) aio_hio_free(meta_json);
    if (tile_ipix) aio_hio_free(tile_ipix);
    return Result<void>::fail(Error(ErrorDomain::IO,
        std::string("aio_hips_product_begin failed: ") + aio_hips_last_error()));
  }
  // NESTED 聚合: HISS tile (tile_nside×tile_nside, parent_ipix at Norder
  // L-log2(tile_nside)) → IVOA 标准 512×512 tile (parent at Norder L-9)。
  // 全局 leaf ipix = hiss_parent×n_leaf + local（NESTED 同构嵌套序）→
  // 标准 tile id = leaf>>18, tile 内偏移 = leaf & (2^18-1)。
  // HISS signal = 累计通量通道; covered_area = support>0 ? A_cell : 0
  // （单帧 stacked 全或无语义, p1_final.json 登记 covered_area_model）。
  const uint64_t hiss_nleaf = n_leaf_per_tile;
  const uint64_t tile_leaf_span = 512ULL * 512ULL;
  const uint64_t std_parent_count = 12ULL * (1ULL << (2 * (leaf_order - 9)));
  std::vector<float> sig_buf(tile_leaf_span, 0.0f);
  std::vector<float> cov_buf(tile_leaf_span, 0.0f);
  std::vector<uint8_t> seen(tile_leaf_span, 0);
  int64_t n_tiles_written = 0;
  for (uint64_t parent = 0; parent < std_parent_count; ++parent) {
    std::fill(seen.begin(), seen.end(), 0);
    bool touched = false;
    for (uint64_t t = 0; t < n_tiles; ++t) {
      // 该 HISS tile 是否属于本标准 tile（parent_ipix 前缀判定, NESTED 同构）
      const uint64_t base_leaf = tile_ipix[t] * hiss_nleaf;
      if ((base_leaf >> 18) != parent) continue;
      float* signal = nullptr; uint32_t n_signal = 0;
      uint8_t* support = nullptr; uint32_t n_support = 0;
      const int rs = aio_hiss_read_tile_signal(hiss_path.c_str(), tile_ipix[t],
                                               &signal, &n_signal);
      const int ru = aio_hiss_read_tile_support(hiss_path.c_str(), tile_ipix[t],
                                                &support, &n_support);
      if (rs != 0 || ru != 0 || n_signal != hiss_nleaf || n_support != hiss_nleaf) {
        if (signal) aio_hio_free(signal);
        if (support) aio_hio_free(support);
        aio_hips_abort(ps);
        if (meta_json) aio_hio_free(meta_json);
        if (tile_ipix) aio_hio_free(tile_ipix);
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "aio_hiss_read_tile failed (tile " + std::to_string(t) + ")"));
      }
      for (uint64_t i = 0; i < hiss_nleaf; ++i) {
        const uint64_t leaf = base_leaf + i;
        const uint64_t off = leaf & (tile_leaf_span - 1);
        sig_buf[off] = signal[i];
        cov_buf[off] = support[i] > 0 ? static_cast<float>(a_cell) : 0.0f;
        seen[off] = 1;
      }
      if (signal) aio_hio_free(signal);
      if (support) aio_hio_free(support);
      touched = true;
    }
    if (!touched) continue;
    AstroSphereTileView view;
    std::memset(&view, 0, sizeof(view));
    view.parent_ipix = parent;
    view.leaf_order = leaf_order;
    view.width = 512;
    view.data_type = AIO_HIPS_FLOAT32;
    view.flux_sum = sig_buf.data();
    view.covered_area = cov_buf.data();
    view.valid_mask = nullptr;
    view.var_num_sum = nullptr;
    const int wr = aio_hips_write_signal_support_tile(ps, &view);
    if (wr != 0) {
      aio_hips_abort(ps);
      if (meta_json) aio_hio_free(meta_json);
      if (tile_ipix) aio_hio_free(tile_ipix);
      return Result<void>::fail(Error(ErrorDomain::IO,
          std::string("aio_hips_write_signal_support_tile failed: ") +
          aio_hips_last_error()));
    }
    ++n_tiles_written;
  }
  if (meta_json) aio_hio_free(meta_json);
  if (tile_ipix) aio_hio_free(tile_ipix);
  if (aio_hips_finalize(ps) != 0) {
    aio_hips_abort(ps);
    return Result<void>::fail(Error(ErrorDomain::IO,
        std::string("aio_hips_finalize failed: ") + aio_hips_last_error()));
  }
  // 产物存在性校验（signal properties = 标准化 HiPS 事实面）
  const std::string props = out_dir + "/signal/properties";
  if (!std::filesystem::exists(std::filesystem::u8path(props), ec)) {
    return Result<void>::fail(Error(ErrorDomain::IO,
        "HiPS properties missing after finalize: " + props));
  }
  const std::string out_path = out_dir + "/p1_final.json";
  Json final_out = Json{{"schema", "DATA-P1-HIPS"},
                        {"entry", "aio_hips_product_begin/write/finalize"},
                        {"hips_root", out_dir},
                        {"nside", nside},
                        {"tile_nside", tile_nside},
                        {"n_tiles", n_tiles},
                        {"n_tiles_written", n_tiles_written},
                        {"n_pix_total", n_pix_total},
                        {"products", Json::array({"signal", "support"})},
                        {"covered_area_model", "support_x_A_cell"},
                        {"properties", props}};
  if (!p1_write_text(out_path, final_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_tiles"] = n_tiles;
  (*man)["hips_root"] = out_dir;
  (*man)["final_artifact"] = out_path;
  return Result<void>::success();
}

// ── P1NodeModule: 唯一真实 operation 节点适配器（IModule）──────────────────
enum class P1NodeOp { Calibrate, Cosmetic, StarPsf, WcsSolve, Photometry, NoiseSnr, Drizzle, Writer };
struct P1NodeSpec {
  P1NodeOp op;
  const char* operation;  // module_ports.registry.json 冻结 operation 名
  const char* entry;      // 冻结唯一真实入口名（节点 manifest 可审计标记）
};

struct P1NodeModule : public IModule {
  ModuleDescriptor desc_;
  P1NodeSpec spec_;
  std::string config_;
  std::string manifest_;
  uint32_t workers_ = 2;

  P1NodeModule(ModuleDescriptor d, P1NodeSpec s)
      : desc_(std::move(d)), spec_(s) {}

  const ModuleDescriptor& descriptor() const noexcept override { return desc_; }

  // 基础 config 合同（对齐 p1_session validate: 必需键/类型, 无 silent default）;
  // drizzle/wcs 科学参数在 op 内 fail-closed 校验（不提前消费科学缺省值）。
  Result<void> validate_config(const std::string& config_json) override {
    Json doc;
    try {
      doc = Json::parse(config_json);
    } catch (const Json::parse_error& e) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          std::string("config parse: ") + e.what()));
    }
    if (!doc.is_object())
      return Result<void>::fail(Error(ErrorDomain::DATA, "config must be an object"));
    for (const char* req : {"input_lights", "output_dir"}) {
      if (!doc.contains(req))
        return Result<void>::fail(Error(ErrorDomain::DATA,
            std::string("missing key '") + req + "'"));
    }
    if (!doc["input_lights"].is_array() || doc["input_lights"].empty() ||
        !doc["output_dir"].is_string())
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "input_lights must be non-empty array; output_dir must be string"));
    for (const auto& l : doc["input_lights"])
      if (!l.is_string())
        return Result<void>::fail(Error(ErrorDomain::DATA, "input_lights items must be strings"));
    if (doc.contains("cosmetic") && !doc["cosmetic"].is_object())
      return Result<void>::fail(Error(ErrorDomain::DATA, "cosmetic must be object"));
    if (doc.contains("wcs") && !doc["wcs"].is_object())
      return Result<void>::fail(Error(ErrorDomain::DATA, "wcs must be object"));
    if (doc.contains("drizzle") && !doc["drizzle"].is_object())
      return Result<void>::fail(Error(ErrorDomain::DATA, "drizzle must be object"));
    return Result<void>::success();
  }

  Result<ModulePlan> plan(const std::string& node_id,
                          const std::string& config_json) override {
    config_ = config_json;
    ModulePlan p;
    p.node_id = node_id;
    p.work_units = 1;
    p.parallel_axes = {"frame-row-band"};
    p.cpu_heavy = desc_.execution_class == "cpu_heavy";
    return Result<ModulePlan>::ok(std::move(p));
  }

  Result<void> execute(RunContext& ctx) override {
    // 预算语义与 SessionModule 一致（RT-003/P0 修复: ctx.budget 权威, lease 授权）
    const uint32_t host_workers =
        ctx.budget() ? ctx.budget()->budget() : workers_;
    ThreadLease lease = ctx.acquire_lease(host_workers);
    const uint32_t cap = lease.acquired() ? lease.size() : 1u;
    HostSession hs;
    if (!hs.init(cap)) {
      return Result<void>::fail(Error(ErrorDomain::RESOURCE,
          desc_.module_id + ": host services init failed"));
    }
    // 重计算线程注入（约束: 禁硬编码; host budget=唯一权威）
    ac_set_num_threads(static_cast<int>(hs.host.budget.max_workers));
    ctx.set_provider("baseline");
    ctx.record_trace([&] {
      TraceEvent e;
      e.type = TraceEventType::PROVIDER_ENTER;
      e.node_id = ctx.current_node();
      e.module_id = desc_.module_id;
      e.provider = "baseline";
      e.kernel_id = desc_.alg_id;
      e.workers = cap;
      e.granted_workers = host_workers;
      return e;
    }());
    Json man = Json{{"kind", "astrocs.phase1.node"},
                    {"module_id", desc_.module_id},
                    {"operation", spec_.operation},
                    {"entry", spec_.entry},
                    {"artifact_type", desc_.data_id},
                    {"availability", "available"},
                    {"status", "running"}};
    Result<void> r = Result<void>::success();
    try {
      Json doc = Json::parse(config_);
      switch (spec_.op) {
        case P1NodeOp::Calibrate:  r = p1_op_calibrate(doc, &man); break;
        case P1NodeOp::Cosmetic:   r = p1_op_cosmetic(doc, &man); break;
        case P1NodeOp::StarPsf:    r = p1_op_star_psf(doc, &man); break;
        case P1NodeOp::WcsSolve:   r = p1_op_wcs(doc, &man); break;
        case P1NodeOp::Photometry: r = p1_op_photometry(doc, &man); break;
        case P1NodeOp::NoiseSnr:   r = p1_op_noise(doc, &man); break;
        case P1NodeOp::Drizzle:    r = p1_op_drizzle(doc, &man); break;
        case P1NodeOp::Writer:     r = p1_op_writer(doc, &man); break;
      }
    } catch (const Json::exception& e) {
      man["error"] = std::string("config value type error: ") + e.what();
      r = Result<void>::fail(Error(ErrorDomain::DATA, man["error"].get<std::string>()));
    } catch (const std::bad_alloc&) {
      man["error"] = "out of memory";
      r = Result<void>::fail(Error(ErrorDomain::RESOURCE, "out of memory"));
    } catch (const std::exception& e) {
      man["error"] = std::string("node exception: ") + e.what();
      r = Result<void>::fail(Error(ErrorDomain::INTERNAL, man["error"].get<std::string>()));
    }
    if (r.failed()) {
      if (!man.contains("error")) man["error"] = r.error().message();
      man["status"] = "fail";
    } else {
      man["status"] = "ok";
      ctx.log(LogLevel::INFO, desc_.module_id,
          "execute OK (" + std::string(spec_.operation) + ")");
    }
    manifest_ = man.dump(2);
    ctx.record_trace([&] {
      TraceEvent e;
      e.type = TraceEventType::PROVIDER_LEAVE;
      e.node_id = ctx.current_node();
      e.module_id = desc_.module_id;
      e.provider = "baseline";
      e.status = r.failed() ? "FAILED" : "OK";
      return e;
    }());
    return r;
  }

  Result<std::string> inspect() override {
    if (manifest_.empty())
      return Result<std::string>::fail(Error(ErrorDomain::DATA,
          desc_.module_id + ": no manifest (execute not run)"));
    return Result<std::string>::ok(manifest_);
  }

  Result<std::string> last_manifest() override {
    if (manifest_.empty())
      return Result<std::string>::fail(Error(ErrorDomain::DATA,
          desc_.module_id + ": no manifest captured (execute not run)"));
    return Result<std::string>::ok(manifest_);
  }
};

std::unique_ptr<IModule> make_p1_node_module(ModuleDescriptor desc, P1NodeSpec spec) {
  return std::make_unique<P1NodeModule>(std::move(desc), spec);
}

}  // namespace

// RT-008: cfitsio 首次初始化 shim（core 不 include cfitsio 头，避免依赖图污染）
extern "C" void astrocs_cfitsio_ensure_initialized(void);

Result<void> register_phase_modules(ModuleRegistry& registry) {
  // RT-008: cfitsio 首次初始化在单线程阶段完成（Runtime 并行 worker 并发首用会数据竞争）。
  astrocs_cfitsio_ensure_initialized();

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

  // P1-001 (G4): 8 类 Phase1 模块注册。calibration+7 子节点全部唯一真实
  // operation 委托（P1-001 attempt 2: ARCH-P0-001 整改——子节点不再委托
  // phase_session_run; 各节点 operation/entry 与 module_ports.registry.json
  // 冻结绑定表一致, manifest 携带标记供 trace/审计）。
  const std::pair<ModuleDescriptor, P1NodeSpec> p1_nodes[] = {
      {phase1_descriptor(),        {P1NodeOp::Calibrate,  "calibrate",        "astrocs_phase1_calibrate_v1"}},
      {p1_cosmetic_descriptor(),   {P1NodeOp::Cosmetic,   "cosmetic_correct", "astrocs_phase1_cosmetic_v1"}},
      {p1_star_psf_descriptor(),   {P1NodeOp::StarPsf,    "detect_sources",   "astrocs_phase1_starpsf_v1"}},
      {p1_wcs_descriptor(),        {P1NodeOp::WcsSolve,   "plate_solve",      "astrocs_phase1_wcs_v1"}},
      {p1_photometry_descriptor(), {P1NodeOp::Photometry, "measure_flux",     "astrocs_phase1_photometry_v1"}},
      {p1_noise_snr_descriptor(),  {P1NodeOp::NoiseSnr,   "estimate_snr",     "astrocs_phase1_noisesnr_v1"}},
      {p1_drizzle_descriptor(),    {P1NodeOp::Drizzle,    "drizzle_stack",    "astrocs_phase1_drizzle_v1"}},
      {p1_writer_descriptor(),     {P1NodeOp::Writer,     "write_hips",       "astrocs_phase1_writer_v1"}},
  };
  for (const auto& [d, spec] : p1_nodes) {
    auto rr = registry.register_module(d);
    if (rr.failed()) return rr;
    auto ff = registry.register_factory(
        d.module_id, [d, spec]() { return make_p1_node_module(d, spec); });
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
