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
// P2-001: Phase2 7 类节点唯一真实 operation 委托（ARCH-P0-001 Phase2 侧整改;
//   原工厂委托 P2Api session adapter = 子节点调用完整 p2_session_run 违规）:
//   coverage   → p2_coverage_build         (lib/phase2 C ABI, 两阶段容量协议)
//   sample     → p2_frame_id + p2_sample_controls_cached (两阶段查询/回填)
//   upm-fit    → p2_upm_build_geo + p2_upm_save          (production ivar 权重)
//   upm-apply  → p2_upm_open + p2_upm_calibrate_block    (帧 HiPS signal 真读)
//   reject     → p2_reject_plan_resolve + p2_collect_candidate_stack
//                + p2_reject_stack_ex (AUTO 只在 planning 层解析)
//   integrate  → p2_validate_candidate_weights + p2_integrate_pixel
//                (weight_mode=2 逆方差无 fallback, fail-closed DATA-UNC-001 §30)
//   write      → aio_hips_product_begin/write_signal_support_tile/
//                write_variance_tile/finalize (AIO-002 原子发布内建;
//                variance/ivar 按 DATA-P2-VAR-001 §30.1 合成公式)
// 子节点一律不调用完整 phase_session_run; operation/entry 名与
// runtime/pipeline/module_ports.registry.json 冻结绑定表一致。
// 节点间 typed artifact 经 output_dir 文件约定传递（p2_coverage.json →
// p2_samples.json → p2_upm_model.{bin,json} → p2_corrected.{json,bin} →
// p2_rejection.{json,bin} → p2_integrated.{json,bin} → mosaic HiPS + p2_final.json）。
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
#include "aio_hips_reader.h" // lib/astro_image_io: HiPS 读面(P2 帧数据消费)

// P2-001: Phase2 真实节点生产头（lib/phase2 冻结 C ABI + HEALPix 单一实现 +
// 输入 manifest hash 共享 SHA-256; 模块库零 diff 只读调用）
#include "astro/phase2/coverage.h"
#include "astro/phase2/sampler.h"
#include "astro/phase2/upm.h"
#include "astro/phase2/rejection.h"
#include "astro/phase2/integrate.h"
#include "healpix/healpix_core.h"  // fits_index_to_nested_local (NESTED LUT 单一权威)
#include "crypto/sha256.h"         // astrocs::crypto::sha256_hex (input_manifest_hash)

#include "photometer.h"
#include "noise_model.h"
#include "wcs_tan.h"

// P3-002: Phase3 唯一真实 operation 节点生产头（lib/phase3_session 冻结 C++
// 内核, 静态库 astrocs_phase3_session 已在 astrocs_module_adapters 链接闭包;
// 相对路径 include 同 "../../phase1/stars/star_detector.h" 先例, 根 CMake
// 零改动）
#include "../../phase3_session/p3_resample.h"
#include "../../phase3_session/p3_output.h"
#include "../../phase3_session/p3_wcs.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

// ── RT-001: 唯一 Executor 生产接入（编译归属注记, 详见 executor_runtime.h）──
// RT-004 冻结合同实现 lib/core/src/executor.cpp 此前未编入任何生产 target
// （根 CMakeLists.txt 不在 RT-001 写入白名单）, 唯一池为死代码。本文件经
// #include 将其编入 astrocs_module_adapters —— 全仓唯一的 executor 池 worker
// 创建点仍是 executor.cpp（合同语句不变）, 仅编译归属临时迁移。整改归位
// （根 CMakeLists 为 astrocs_module_adapters 追加 executor.cpp 并删除此
// include）已登记 finding F-RT-001-04; 在此之前, 任何 target 不得把
// executor.cpp 与 astrocs_module_adapters 编入同一二进制（重定义 → 链接期
// 显式失败, 无静默重复）。
#include "executor.cpp"
#include "executor_runtime.h"

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

// ── RT-001: Runtime 唯一 work-unit executor 注册点（合同见 executor_runtime.h）──
namespace astrocs::core::rt {

std::shared_ptr<CpuHeavyExecutor> shared_work_executor(
    const std::shared_ptr<ThreadBudget>& budget) {
  static std::mutex mu;
  // 强引用注册表: 池随预算源进程驻留（"唯一 executor 池" 语义）; 析构路径
  // join 全部 worker（executor.cpp RT-004 生命周期合同, 无 detach/UAF）。
  static std::vector<std::pair<std::weak_ptr<ThreadBudget>,
                               std::shared_ptr<CpuHeavyExecutor>>>
      registry;
  if (!budget || budget->budget() == 0) return nullptr;  // 无预算上下文 → 串行降级
  std::lock_guard<std::mutex> lock(mu);
  for (auto it = registry.begin(); it != registry.end();) {
    // 防御性回收: 若池实现不再持有预算强引用, 预算消亡后池一并回收。
    if (it->first.expired()) {
      it = registry.erase(it);
    } else {
      ++it;
    }
  }
  for (const auto& entry : registry) {
    if (entry.first.lock() == budget) return entry.second;  // 同一预算源 → 同一池
  }
  auto created = create_cpu_heavy_executor(budget);
  if (!created.ok()) return nullptr;
  registry.emplace_back(budget, std::move(created.value()));
  return registry.back().second;
}

}  // namespace astrocs::core::rt

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

// ══ P2-001: Phase2 真实节点 operation 实现 ═══════════════════════════════════
// 每节点唯一真实 operation 委托（见文件头映射表）；子节点禁止调用完整
// p2_session_run。manifest 携带 operation/entry 标记（与
// runtime/pipeline/module_ports.registry.json 冻结绑定表一致），typed
// artifact 落盘 config.output_dir。节点间数据流 = output_dir 文件约定
// （上游 artifact 缺失 → DATA fail-closed，指向上游节点未执行）。

// ── P2 共用工具 ──────────────────────────────────────────────────────────────
bool p2_write_text(const std::string& path, const std::string& text) {
  std::ofstream f(std::filesystem::u8path(path), std::ios::binary);
  if (!f) return false;
  f << text;
  return f.good();
}

bool p2_read_json(const std::string& path, Json* out) {
  std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
  if (!f) return false;
  try {
    f >> *out;
  } catch (const Json::parse_error&) {
    return false;
  }
  return true;
}

// 裸数值 bin 写/读（typed artifact 数据面; JSON manifest 记 offset/count）
template <typename T>
bool p2_write_bin(const std::string& path, const std::vector<T>& v) {
  std::ofstream f(std::filesystem::u8path(path), std::ios::binary);
  if (!f) return false;
  if (!v.empty())
    f.write(reinterpret_cast<const char*>(v.data()),
            static_cast<std::streamsize>(v.size() * sizeof(T)));
  return f.good();
}

template <typename T>
bool p2_read_bin_range(const std::string& path, uint64_t offset_elems,
                       uint64_t count_elems, std::vector<T>* out) {
  std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
  if (!f) return false;
  f.seekg(static_cast<std::streamoff>(offset_elems * sizeof(T)));
  if (!f.good()) return false;
  out->assign(static_cast<size_t>(count_elems), T{});
  if (count_elems == 0) return true;
  f.read(reinterpret_cast<char*>(out->data()),
         static_cast<std::streamsize>(count_elems * sizeof(T)));
  return f.good() || f.gcount() == static_cast<std::streamsize>(count_elems * sizeof(T));
}

// tile 内 leaf 数（512×512 标准 HiPS tile = 2^18 leaf）
constexpr uint64_t kP2TileLeafSpan = 512ULL * 512ULL;
constexpr uint32_t kP2TileShift = 9;  // leaf order − tile order 差（512=2^9）

// coverage artifact → P2CoverageResult 重建（sample 节点消费上游 typed artifact;
// inputs/union_cells 由调用方 vector 持有, 仅视图指向）
struct P2CoverageView {
  P2CoverageResult cov{};
  std::vector<P2HipsInputInfo> inputs;
  std::vector<P2MocCell> cells;
  std::vector<std::string> hips_paths;
  std::vector<const char*> path_ptrs;
};

bool p2_coverage_from_artifact(const Json& cov_doc, P2CoverageView* view,
                               std::string* err) {
  if (!cov_doc.is_object() || cov_doc.value("schema", "") != "DATA-P2-COV") {
    *err = "coverage artifact schema mismatch";
    return false;
  }
  const auto& paths = cov_doc["hips_paths"];
  const auto& frames = cov_doc["frames"];
  const auto& cells = cov_doc["union_cells"];
  if (!paths.is_array() || !frames.is_array() || !cells.is_array() ||
      paths.size() != frames.size() || paths.empty()) {
    *err = "coverage artifact fields invalid";
    return false;
  }
  view->hips_paths.clear();
  for (const auto& p : paths) view->hips_paths.push_back(p.get<std::string>());
  view->inputs.resize(view->hips_paths.size());
  for (size_t i = 0; i < view->hips_paths.size(); ++i) {
    const auto& fr = frames[i];
    P2HipsInputInfo& info = view->inputs[i];
    std::memset(&info, 0, sizeof(info));
    const std::string hp = fr.value("hips_path", view->hips_paths[i]);
    std::snprintf(info.hips_path, sizeof(info.hips_path), "%s", hp.c_str());
    std::snprintf(info.frame_id, sizeof(info.frame_id), "%s",
                  fr.value("frame_id", "").c_str());
    info.max_leaf_order = fr.value("max_leaf_order", 0);
    info.n_tiles = fr.value("n_tiles", 0);
    std::snprintf(info.filter_passband, sizeof(info.filter_passband), "%s",
                  fr.value("filter_passband", "").c_str());
    std::snprintf(info.frame_type, sizeof(info.frame_type), "%s",
                  fr.value("frame_type", "").c_str());
  }
  view->cells.clear();
  for (const auto& c : cells) {
    P2MocCell mc{};
    mc.order = c.value("order", 0ull);
    mc.ipix = c.value("ipix", 0ull);
    view->cells.push_back(mc);
  }
  view->path_ptrs.clear();
  for (const auto& p : view->hips_paths) view->path_ptrs.push_back(p.c_str());
  std::memset(&view->cov, 0, sizeof(view->cov));
  view->cov.n_inputs = view->hips_paths.size();
  view->cov.inputs = view->inputs.data();
  view->cov.n_union_cells = view->cells.size();
  view->cov.union_cells = view->cells.empty() ? nullptr : view->cells.data();
  view->cov.target_order = cov_doc.value("target_order", 0);
  view->cov.status = 0;
  return true;
}

// DATA-FRAME-ID-001 内容稳定帧标识（真实 p2_frame_id 生产符号; 0=失败哨兵）
uint64_t p2_node_frame_id(const std::string& hips_path, std::string* err) {
  const uint64_t fid = p2_frame_id(hips_path.c_str());
  if (fid == 0 && err)
    *err = "p2_frame_id failed (hash/open): " + hips_path;
  return fid;
}

// §20.3 input_manifest_hash = canonical(frame identity + 关键元数据)（stage2
// 同一公式: 按 frame_id 升序拼接 "fid|filter=..;order=..;frame=..;" 后 sha256;
// frame_id 为真实 p2_frame_id 数值（DATA-FRAME-ID-001）, 16hex 大端文本排序键）
std::string p2_input_manifest_hash(const P2CoverageView& view,
                                   const std::vector<uint64_t>& frame_ids) {
  std::vector<std::pair<std::string, std::string>> hex_entries;
  for (size_t i = 0; i < view.hips_paths.size(); ++i) {
    std::string meta;
    meta += std::string("filter=") + view.inputs[i].filter_passband + ";";
    meta += "order=" + std::to_string(view.inputs[i].max_leaf_order) + ";";
    meta += std::string("frame=") + view.inputs[i].frame_type + ";";
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx",
                  static_cast<unsigned long long>(frame_ids[i]));
    hex_entries.emplace_back(std::string(hex), meta);
  }
  std::sort(hex_entries.begin(), hex_entries.end());
  std::string payload;
  for (const auto& e : hex_entries) payload += e.first + "|" + e.second + ";";
  return astrocs::crypto::sha256_hex(payload.data(), payload.size());
}

// ── op: compute_coverage（唯一真实入口 p2_coverage_build; 两阶段容量协议）──
Result<void> p2_op_coverage(const Json& doc, Json* man) {
  std::vector<std::string> paths;
  for (const auto& p : doc["hips_paths"]) paths.push_back(p.get<std::string>());
  const std::string out_dir = doc.value("output_dir", std::string("."));
  std::vector<const char*> ptrs;
  for (const auto& p : paths) ptrs.push_back(p.c_str());

  (*man)["stages"] = Json::array();
  Json& st = (*man)["stages"].emplace_back(Json{{"name", "coverage"}, {"status", "running"}});
  P2CoverageResult cov{};
  cov.n_inputs = ptrs.size();
  int rc = p2_coverage_build(ptrs.data(), ptrs.size(), &cov);   // 查询容量
  if (rc != 0 && cov.n_union_cells == 0) {
    st["status"] = "fail";
    return Result<void>::fail(Error(ErrorDomain::DATA,
        std::string("p2_coverage_build failed: ") + cov.error));
  }
  std::vector<P2HipsInputInfo> infos(paths.size());
  cov.inputs = infos.data();
  std::vector<P2MocCell> cells(cov.n_union_cells > 0 ? cov.n_union_cells : 1);
  cov.union_cells = cells.data();
  rc = p2_coverage_build(ptrs.data(), ptrs.size(), &cov);       // 回填
  if (rc != 0) {
    st["status"] = "fail";
    return Result<void>::fail(Error(ErrorDomain::DATA,
        std::string("p2_coverage_build failed (fill): ") + cov.error));
  }
  st["status"] = "ok";
  st["n_inputs"] = cov.n_inputs;
  st["n_union_cells"] = cov.n_union_cells;
  st["target_order"] = cov.target_order;

  Json frames = Json::array();
  for (size_t i = 0; i < paths.size(); ++i) {
    frames.push_back(Json{{"hips_path", paths[i]},
                          {"frame_id", std::string(infos[i].frame_id)},
                          {"max_leaf_order", infos[i].max_leaf_order},
                          {"n_tiles", infos[i].n_tiles},
                          {"filter_passband", std::string(infos[i].filter_passband)},
                          {"frame_type", std::string(infos[i].frame_type)}});
  }
  Json cells_j = Json::array();
  for (const auto& c : cells)
    cells_j.push_back(Json{{"order", c.order}, {"ipix", c.ipix}});

  const std::string out_path = out_dir + "/p2_coverage.json";
  Json artifact = Json{{"schema", "DATA-P2-COV"},
                       {"entry", "p2_coverage_build"},
                       {"n_inputs", cov.n_inputs},
                       {"target_order", cov.target_order},
                       {"n_union_cells", cov.n_union_cells},
                       {"hips_paths", paths},
                       {"frames", frames},
                       {"union_cells", cells_j}};
  if (!p2_write_text(out_path, artifact.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed: " + out_path));
  (*man)["coverage_artifact"] = out_path;
  (*man)["n_union_cells"] = cov.n_union_cells;
  (*man)["target_order"] = cov.target_order;
  return Result<void>::success();
}

// ── op: sample_frames（唯一真实入口 p2_frame_id + p2_sample_controls_cached;
//      两阶段查询/回填; 消费上游 coverage artifact, 缺失即 DATA fail-closed）──
Result<void> p2_op_sample(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  Json cov_doc;
  const std::string cov_path = out_dir + "/p2_coverage.json";
  if (!p2_read_json(cov_path, &cov_doc))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream coverage artifact missing: " + cov_path +
        " (sample consumes coverage output)"));
  P2CoverageView view;
  std::string err;
  if (!p2_coverage_from_artifact(cov_doc, &view, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "coverage artifact rebuild failed: " + err));

  // 内容稳定帧标识缓存（DATA-FRAME-ID-001; 0 = p2_frame_id 失败哨兵 → 拒绝）
  std::vector<uint64_t> frame_ids(view.hips_paths.size());
  for (size_t i = 0; i < view.hips_paths.size(); ++i) {
    frame_ids[i] = p2_node_frame_id(view.hips_paths[i], &err);
    if (frame_ids[i] == 0)
      return Result<void>::fail(Error(ErrorDomain::DATA, err));
  }
  const std::string manifest_hash = p2_input_manifest_hash(view, frame_ids);

  P2SamplerConfig sc = p2_sampler_default_config();
  // CON-004: cpu_workers = Runtime lease 权威（P2NodeModule execute 注入
  // __workers; budget 唯一权威, 禁硬编码; 1=串行 reference）
  sc.cpu_workers = std::max(1, doc.value("__workers", 1));

  uint64_t n_obs = 0, n_controls = 0;
  P2SampleStats stats{};
  int rc = p2_sample_controls_cached(&view.cov, view.path_ptrs.data(),
                                     frame_ids.data(), &sc, nullptr, 0,
                                     &n_obs, &n_controls, &stats,
                                     nullptr, 0, nullptr, 0);
  if (rc != 0 && n_obs == 0)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        std::string("p2_sample_controls_cached(query) failed rc=") + std::to_string(rc)));
  std::vector<P2ControlObservation> obs(n_obs > 0 ? n_obs : 1);
  std::vector<P2ControlNode> nodes(n_controls > 0 ? n_controls : 1);
  char errbuf[512] = {0};
  rc = p2_sample_controls_cached(&view.cov, view.path_ptrs.data(),
                                 frame_ids.data(), &sc, obs.data(), n_obs,
                                 &n_obs, &n_controls, &stats, nodes.data(),
                                 n_controls, errbuf, sizeof(errbuf));
  if (rc != 0)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        std::string("p2_sample_controls_cached failed rc=") + std::to_string(rc) +
        ": " + errbuf));

  Json obs_j = Json::array();
  for (uint64_t i = 0; i < n_obs; ++i) {
    const auto& o = obs[static_cast<size_t>(i)];
    obs_j.push_back(Json{{"frame_id", o.frame_id},
                         {"control_id", o.control_id},
                         {"leaf_ipix", o.leaf_ipix},
                         {"ra_deg", o.ra_deg},
                         {"dec_deg", o.dec_deg},
                         {"value", o.value},
                         {"uncertainty", o.uncertainty},
                         {"snr", o.snr},
                         {"ivar", o.ivar},
                         {"control_variance", o.control_variance},
                         {"control_ivar", o.control_ivar},
                         {"snr_available", o.snr_available},
                         {"support", o.support},
                         {"quality_flags", o.quality_flags}});
  }
  Json nodes_j = Json::array();
  for (uint64_t i = 0; i < n_controls; ++i) {
    const auto& n = nodes[static_cast<size_t>(i)];
    nodes_j.push_back(Json{{"control_id", n.control_id},
                           {"tile_ipix", n.tile_ipix},
                           {"gx", n.gx}, {"gy", n.gy},
                           {"ra_deg", n.ra_deg}, {"dec_deg", n.dec_deg},
                           {"leaf_ipix", n.leaf_ipix}});
  }
  Json fid_j = Json::array();
  for (uint64_t f : frame_ids) fid_j.push_back(f);

  const std::string out_path = out_dir + "/p2_samples.json";
  Json artifact = Json{{"schema", "DATA-P2-SMP"},
                       {"entry", "p2_sample_controls_cached"},
                       {"input_manifest_hash", manifest_hash},
                       {"target_order", view.cov.target_order},
                       {"frame_ids", fid_j},
                       {"n_obs", n_obs},
                       {"n_controls", n_controls},
                       {"stats", Json{{"candidate_observations", stats.candidate_observations},
                                      {"accepted_observations", stats.accepted_observations},
                                      {"rejected_insufficient_support", stats.rejected_insufficient_support},
                                      {"rejected_insufficient_retained", stats.rejected_insufficient_retained},
                                      {"rejected_bright_tolerance", stats.rejected_bright_tolerance},
                                      {"rejected_high_contamination", stats.rejected_high_contamination},
                                      {"rejected_catalog_veto", stats.rejected_catalog_veto},
                                      {"rejected_lt_two_clean_frames", stats.rejected_lt_two_clean_frames},
                                      {"accepted_controls", stats.accepted_controls},
                                      {"overlap_controls", stats.overlap_controls}}},
                       {"controls", nodes_j},
                       {"observations", obs_j}};
  if (!p2_write_text(out_path, artifact.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed: " + out_path));
  (*man)["samples_artifact"] = out_path;
  (*man)["n_obs"] = n_obs;
  (*man)["n_controls"] = n_controls;
  (*man)["overlap_controls"] = stats.overlap_controls;
  return Result<void>::success();
}

// ── op: fit_upm（唯一真实入口 p2_upm_build_geo + p2_upm_save; production
//      control_ivar 权重（SCI-UPM-WEIGHT-001, use_ivar_weight=1 冻结;
//      control_ivar<=0/非有限 → 生产模式显式拒绝, 禁静默回退））──
Result<void> p2_op_upm_fit(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  Json smp_doc;
  const std::string smp_path = out_dir + "/p2_samples.json";
  if (!p2_read_json(smp_path, &smp_doc))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream samples artifact missing: " + smp_path +
        " (upm-fit consumes sample output)"));
  const auto& obs_j = smp_doc["observations"];
  const auto& nodes_j = smp_doc["controls"];
  if (!obs_j.is_array() || !nodes_j.is_array())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "samples artifact observations/controls invalid"));
  std::vector<P2ControlObservation> obs;
  obs.reserve(obs_j.size());
  for (const auto& o : obs_j) {
    P2ControlObservation x{};
    x.frame_id = o.value("frame_id", 0ull);
    x.control_id = o.value("control_id", 0ull);
    x.leaf_ipix = o.value("leaf_ipix", 0ull);
    x.ra_deg = o.value("ra_deg", 0.0);
    x.dec_deg = o.value("dec_deg", 0.0);
    x.value = o.value("value", 0.0);
    x.uncertainty = o.value("uncertainty", 0.0);
    x.snr = o.value("snr", 0.0);
    x.ivar = o.value("ivar", 0.0);
    x.control_variance = o.value("control_variance", 0.0);
    x.control_ivar = o.value("control_ivar", 0.0);
    x.snr_available = o.value("snr_available", 0);
    x.support = o.value("support", 0.0);
    x.quality_flags = o.value("quality_flags", 0u);
    obs.push_back(x);
  }
  std::vector<P2ControlNode> nodes;
  nodes.reserve(nodes_j.size());
  for (const auto& n : nodes_j) {
    P2ControlNode x{};
    x.control_id = n.value("control_id", 0ull);
    x.tile_ipix = n.value("tile_ipix", 0ull);
    x.gx = n.value("gx", 0);
    x.gy = n.value("gy", 0);
    x.ra_deg = n.value("ra_deg", 0.0);
    x.dec_deg = n.value("dec_deg", 0.0);
    x.leaf_ipix = n.value("leaf_ipix", 0ull);
    nodes.push_back(x);
  }

  P2UpmBuildConfig uc{};
  uc.robust_loss = 0;              // huber(首版冻结)
  uc.snr_weight_mode = 0;          // snr2_normalized
  uc.huber_delta = 1.345;
  uc.max_iterations = 100;
  uc.tolerance = 1e-6;
  // target_order = coverage 实测值（p2_session 同款; 空间 UPM 显式 control
  // leaf 层级 order=target+9 由模型内部展开）
  uc.target_order = smp_doc.value("target_order", -1);
  uc.sigma_floor = 1e-3;
  uc.support_power = 1.0;
  uc.use_ivar_weight = 1;          // production（SCI-UPM-WEIGHT-001 冻结）
  uc.control_reliability = 1.0;
  // CON-005: cpu_workers = Runtime lease 权威（execute 注入 __workers）
  uc.cpu_workers = std::max(1, doc.value("__workers", 1));
  const std::string manifest_hash = smp_doc.value("input_manifest_hash", std::string());
  std::string manifest_hold = manifest_hash;
  uc.input_manifest_hash = manifest_hold.empty() ? nullptr : manifest_hold.c_str();
  const Json& upm_cfg = doc.contains("upm") ? doc["upm"] : Json::object();
  if (upm_cfg.contains("max_iterations"))
    uc.max_iterations = upm_cfg["max_iterations"].get<int>();
  if (upm_cfg.contains("huber_delta"))
    uc.huber_delta = upm_cfg["huber_delta"].get<double>();
  if (upm_cfg.contains("smoothing_lambda"))
    uc.smoothing_lambda = upm_cfg["smoothing_lambda"].get<double>();

  void* model = nullptr;
  const int rc = p2_upm_build_geo(obs.data(), obs.size(),
                                  nodes.empty() ? nullptr : nodes.data(),
                                  nodes.size(), &uc, &model);
  if (rc != 0 || !model)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        std::string("p2_upm_build_geo failed rc=") + std::to_string(rc) +
        " (production control-ivar weight: missing/invalid control ivar is an"
        " explicit error, no silent fallback)"));

  P2ModelInfo info{};
  char geometry_hash[128] = {0};
  std::vector<uint64_t> gauges;
  uint64_t n_components = 0;
  if (p2_upm_info(model, &info) != 0)
    std::memset(&info, 0, sizeof(info));
  if (p2_upm_geometry_hash(model, geometry_hash, sizeof(geometry_hash)) != 0)
    geometry_hash[0] = '\0';
  if (p2_upm_component_gauges(model, &n_components, nullptr) == 0 &&
      n_components > 0) {
    gauges.resize(static_cast<size_t>(n_components));
    if (p2_upm_component_gauges(model, &n_components, gauges.data()) != 0)
      gauges.clear();
  }
  const std::string bin_path = out_dir + "/p2_upm_model.bin";
  if (p2_upm_save(model, bin_path.c_str()) != 0) {
    p2_upm_close(model);
    return Result<void>::fail(Error(ErrorDomain::IO,
        "p2_upm_save failed: " + bin_path));
  }
  p2_upm_close(model);   // 所有权合同 §1: 调用方持有, p2_upm_close 释放

  Json gauges_j = Json::array();
  for (uint64_t g : gauges) gauges_j.push_back(g);
  const std::string out_path = out_dir + "/p2_upm_model.json";
  Json artifact = Json{{"schema", "DATA-P2-UPM"},
                       {"entry", "p2_upm_build_geo/p2_upm_save"},
                       {"model_hash", std::string(info.model_hash)},
                       {"geometry_hash", std::string(geometry_hash)},
                       {"control_count", info.control_count},
                       {"observation_count", info.observation_count},
                       {"component_count", info.component_count},
                       {"target_order", info.target_order},
                       {"precision", info.precision},
                       {"gauges", gauges_j},
                       {"input_manifest_hash", manifest_hash},
                       {"artifact_bin", bin_path},
                       {"use_ivar_weight", 1}};
  if (!p2_write_text(out_path, artifact.dump(2))) {
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(bin_path), ec);
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed: " + out_path));
  }
  (*man)["upm_model_artifact"] = out_path;
  (*man)["upm_model_bin"] = bin_path;
  (*man)["model_hash"] = std::string(info.model_hash);
  (*man)["observation_count"] = info.observation_count;
  return Result<void>::success();
}

// ── upm-apply 共用: 读单帧全部 tile 的 signal/support, 生成 leaf 升序
//    (tile_ipix 升序 × tile 内 NESTED local 升序) 的 valid 像素校正集。
//    唯一真实入口 p2_upm_calibrate_block（W2 冻结核心接口）; 每帧每 tile
//    恰好一次块调用; support<=0/非 finite 像素不进校正（保留 NaN 占位,
//    下游 integrate 面按 §30 invalid policy 收敛 NaN）。──
struct P2FrameTiles {
  // 每 tile: {tile_ipix, leaves(valid), corrected(valid 位置回填)}; bin 内
  // 布局 = tile 升序 × 262144 leaf（含 NaN 占位, offset 按 tile 序推进）。
  struct TileData {
    uint64_t tile_ipix;
    uint64_t offset;                 // bin 内元素偏移（tile 序 × kP2TileLeafSpan）
  };
  uint64_t frame_id = 0;
  std::vector<TileData> tiles;
  std::vector<double> data;          // 全 tile leaf-major（NaN=无效）
};

Result<void> p2_op_upm_apply(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  Json model_doc;
  const std::string model_path = out_dir + "/p2_upm_model.json";
  if (!p2_read_json(model_path, &model_doc))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream upm model artifact missing: " + model_path +
        " (upm-apply consumes upm-fit output)"));
  const std::string bin_path = model_doc.value("artifact_bin",
                                               out_dir + "/p2_upm_model.bin");
  void* model = nullptr;
  if (p2_upm_open(bin_path.c_str(), &model) != 0 || !model)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "p2_upm_open failed: " + bin_path));
  struct ModelGuard {
    void* m;
    ~ModelGuard() { if (m) p2_upm_close(m); }
  } model_guard{model};

  std::vector<std::string> paths;
  for (const auto& p : doc["hips_paths"]) paths.push_back(p.get<std::string>());
  // leaf 布局 LUT（单一权威 healpix fits_index_to_nested_local; tile 512²）
  std::vector<uint64_t> local_lut(kP2TileLeafSpan);
  for (uint64_t i = 0; i < kP2TileLeafSpan; ++i)
    local_lut[i] = astrocs::healpix::fits_index_to_nested_local(i, kP2TileShift, 512u);

  Json frames_j = Json::array();
  uint64_t total_pixels = 0;
  for (size_t f = 0; f < paths.size(); ++f) {
    const std::string& path = paths[f];
    std::string err;
    const uint64_t fid = p2_node_frame_id(path, &err);
    if (fid == 0)
      return Result<void>::fail(Error(ErrorDomain::DATA, err));
    AioHipsDataset* sig = aio_hips_open(path.c_str(), AIO_HIPS_RD_SIGNAL);
    AioHipsDataset* sup = aio_hips_open(path.c_str(), AIO_HIPS_RD_SUPPORT);
    if (!sig || !sup) {
      if (sig) aio_hips_close(sig);
      if (sup) aio_hips_close(sup);
      return Result<void>::fail(Error(ErrorDomain::IO,
          "aio_hips_open failed (frame " + std::to_string(f) + "): " + path +
          " -- " + aio_hips_reader_last_error()));
    }
    P2FrameTiles ft;
    ft.frame_id = fid;
    const int n_tiles = aio_hips_tile_count(sig);
    if (n_tiles <= 0) {
      aio_hips_close(sig);
      aio_hips_close(sup);
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "frame has no signal tiles: " + path));
    }
    std::vector<uint64_t> tile_ipix(static_cast<size_t>(n_tiles));
    for (int t = 0; t < n_tiles; ++t)
      aio_hips_tile_ipix(sig, t, &tile_ipix[static_cast<size_t>(t)]);
    std::sort(tile_ipix.begin(), tile_ipix.end());
    std::vector<float> sig_buf(kP2TileLeafSpan), sup_buf(kP2TileLeafSpan);
    std::vector<double> in_v(kP2TileLeafSpan), out_v(kP2TileLeafSpan);
    std::vector<uint64_t> leaves(kP2TileLeafSpan);
    uint64_t tile_offset = 0;
    for (int t = 0; t < n_tiles; ++t) {
      const uint64_t tip = tile_ipix[static_cast<size_t>(t)];
      if (aio_hips_read_tile_f32(sig, tip, sig_buf.data()) != 0 ||
          aio_hips_read_tile_f32(sup, tip, sup_buf.data()) != 0) {
        aio_hips_close(sig);
        aio_hips_close(sup);
        return Result<void>::fail(Error(ErrorDomain::IO,
            "aio_hips_read_tile_f32 failed (frame " + std::to_string(f) +
            " tile " + std::to_string(tip) + "): " + path));
      }
      // valid 像素集（support>0 且 finite）→ 块校正; 无效位置保留 NaN
      uint64_t n_valid = 0;
      for (uint64_t i = 0; i < kP2TileLeafSpan; ++i) {
        const double sv = static_cast<double>(sup_buf[static_cast<size_t>(i)]);
        const double xv = static_cast<double>(sig_buf[static_cast<size_t>(i)]);
        out_v[static_cast<size_t>(i)] = std::numeric_limits<double>::quiet_NaN();
        if (std::isfinite(sv) && sv > 0.0 && std::isfinite(xv)) {
          const uint64_t leaf = (tip << (2 * kP2TileShift)) | local_lut[static_cast<size_t>(i)];
          leaves[static_cast<size_t>(n_valid)] = leaf;
          in_v[static_cast<size_t>(n_valid)] = xv;
          ++n_valid;
        }
      }
      if (n_valid > 0) {
        p2_upm_calibrate_block(model, fid, leaves.data(), in_v.data(),
                               out_v.data(), n_valid);   // 唯一真实校正入口
        // 回填 valid 位置（calibrate_block 按输入序输出; 重新扫描映射）
        uint64_t k = 0;
        for (uint64_t i = 0; i < kP2TileLeafSpan; ++i) {
          const double sv = static_cast<double>(sup_buf[static_cast<size_t>(i)]);
          const double xv = static_cast<double>(sig_buf[static_cast<size_t>(i)]);
          if (std::isfinite(sv) && sv > 0.0 && std::isfinite(xv)) {
            ft.data.push_back(out_v[static_cast<size_t>(k)]);
            ++k;
          } else {
            ft.data.push_back(std::numeric_limits<double>::quiet_NaN());
          }
        }
      } else {
        for (uint64_t i = 0; i < kP2TileLeafSpan; ++i)
          ft.data.push_back(std::numeric_limits<double>::quiet_NaN());
      }
      ft.tiles.push_back(P2FrameTiles::TileData{tip, tile_offset});
      tile_offset += kP2TileLeafSpan;
      total_pixels += kP2TileLeafSpan;
    }
    aio_hips_close(sig);
    aio_hips_close(sup);
    // per-frame corrected bin（typed artifact 数据面; NaN=无覆盖）
    char fid_hex[17];
    std::snprintf(fid_hex, sizeof(fid_hex), "%016llx",
                  static_cast<unsigned long long>(fid));
    const std::string data_file = out_dir + "/p2_corrected_f" + fid_hex + ".bin";
    if (!p2_write_bin(data_file, ft.data))
      return Result<void>::fail(Error(ErrorDomain::IO,
          "corrected bin write failed: " + data_file));
    Json tiles_j = Json::array();
    for (const auto& td : ft.tiles)
      tiles_j.push_back(Json{{"tile_ipix", td.tile_ipix},
                             {"n_pixels", kP2TileLeafSpan},
                             {"offset", td.offset}});
    frames_j.push_back(Json{{"frame_id", fid},
                            {"hips_path", path},
                            {"data_file", data_file},
                            {"n_tiles", ft.tiles.size()},
                            {"tiles", tiles_j}});
  }

  const std::string out_path = out_dir + "/p2_corrected.json";
  Json artifact = Json{{"schema", "DATA-P2-COR"},
                       {"entry", "p2_upm_open/p2_upm_calibrate_block"},
                       {"model_hash", model_doc.value("model_hash", "")},
                       {"n_pixels_total", total_pixels},
                       {"tile_leaf_span", kP2TileLeafSpan},
                       {"frames", frames_j}};
  if (!p2_write_text(out_path, artifact.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed: " + out_path));
  (*man)["corrected_artifact"] = out_path;
  (*man)["n_pixels_total"] = total_pixels;
  return Result<void>::success();
}

// ── op: reject_outliers（唯一真实入口 p2_reject_plan_resolve +
//      p2_collect_candidate_stack + p2_reject_stack_ex; AUTO 只在 planning
//      层解析（wbpp_current group-level 一次解析, nominal=全链帧数）;
//      kernel 永不执行 AUTO）。rejected_low/high 语义 = threshold 侧
//      计数（禁原始值符号）; n<=underdetermined_n → UNDERDETERMINED
//      全接受并记录。──
Result<void> p2_op_reject(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  Json cor_doc;
  const std::string cor_path = out_dir + "/p2_corrected.json";
  if (!p2_read_json(cor_path, &cor_doc))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream corrected artifact missing: " + cor_path +
        " (reject consumes upm-apply output)"));
  const auto& frames = cor_doc["frames"];
  if (!frames.is_array() || frames.empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "corrected artifact frames invalid"));
  const uint64_t tile_span = cor_doc.value("tile_leaf_span", kP2TileLeafSpan);

  // wbpp_current group-level 一次解析（kernel 域禁 AUTO）
  P2RejectionPlanRequest req{};
  req.request = P2_REJECT_AUTO;
  req.nominal_contributors = static_cast<std::uint32_t>(frames.size());
  const std::string profile = doc.value("reject_profile", std::string("wbpp_current"));
  req.profile = profile.c_str();
  req.underdetermined_n = 2;
  P2RejectionPlan plan{};
  char perr[256] = {0};
  if (p2_reject_plan_resolve(&req, &plan, perr, sizeof(perr)) != 0)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        std::string("p2_reject_plan_resolve failed: ") + perr));

  // 跨帧 tile 对齐: union tile = 各帧 tile 并集（升序）; 帧序 = corrected
  // manifest 帧序（稳定）。bin 布局 per tile: accepted u8 | nrej u16 | candidates u16
  struct TileRef { uint64_t tile_ipix; const Json* frame; uint64_t offset; };
  std::map<uint64_t, std::vector<TileRef>> union_tiles;   // ipix → 每帧 ref
  for (const auto& fr : frames) {
    for (const auto& t : fr["tiles"]) {
      const uint64_t tip = t.value("tile_ipix", 0ull);
      const uint64_t off = t.value("offset", 0ull);
      union_tiles[tip].push_back(TileRef{tip, &fr, off});
    }
  }
  std::vector<uint8_t> accepted_bin;
  std::vector<uint16_t> nrej_bin, cand_u16;
  uint64_t acc_total = 0, rej_low_total = 0, rej_high_total = 0, undet_total = 0;
  uint64_t n_pixels_processed = 0;
  Json tiles_j = Json::array();
  uint64_t out_offset = 0;
  std::vector<double> compact_vals;  // kernel 候选栈（工作缓冲）
  std::vector<uint32_t> src_idx;
  std::vector<uint8_t> reasons;
  for (const auto& [tip, refs] : union_tiles) {
    const uint64_t depth = refs.size();
    if (depth > 255)
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "tile depth > 255 exceeds u16 rejection counters"));
    // 读各帧 tile 数据并拼接为 frame-major 平面（valid 面完整性: 文件/
    // offset/count 一致性）。
    // [F-P2-002-01 修复] gather 契约: values 必须是 frame-major 平面且
    // value_stride = 每帧元素跨度（rejection.h:229 契约注释;
    // rejection.cpp:1185 索引公式 vd[s*stride+pixel] 为契约权威;
    // stage2.cpp:1086 传 chunk_pixels / acr_kernels.cpp:121 传 n_px 同证）。
    // 原实现 value_stride=sizeof(double) 且以仅 depth 元素的逐像素 vals
    // 缓冲为基址, kernel 按 s*stride+pixel 寻址 → depth≥3 堆越界读 +
    // 垃圾栈 → rejection bins 失真且非确定。现改为各帧 tile 数据
    // （tile_span 元素/帧）连续拼入 depth×tile_span 平面后整体传入,
    // 缓冲覆盖全部契约寻址范围（s*stride+pixel ≤ depth*tile_span-1）,
    // 零堆越界。corrected 数据面为 fp64（p2_corrected bin 由 double 写出）
    // → value_dtype=1 显式声明（零初始化默认 0=fp32 会使 kernel 以 float
    // 宽度错误解释 double 位模式并错位跨帧寻址, ASAN "READ of size 4" 同证）。
    std::vector<double> frame_major(static_cast<size_t>(depth) *
                                    static_cast<size_t>(tile_span));
    {
      std::vector<double> scratch;
      for (size_t d = 0; d < depth; ++d) {
        const auto& ref = refs[d];
        if (!p2_read_bin_range<double>(ref.frame->value("data_file", ""),
                                       ref.offset, tile_span, &scratch))
          return Result<void>::fail(Error(ErrorDomain::IO,
              "corrected bin read failed (tile " + std::to_string(ref.tile_ipix) +
              " frame slot " + std::to_string(d) + ")"));
        std::memcpy(frame_major.data() + d * tile_span, scratch.data(),
                    tile_span * sizeof(double));
      }
    }
    const uint64_t base = out_offset;
    for (uint64_t p = 0; p < tile_span; ++p) {
      // 资格收集（生产 strided 单一路径）: frame-major values, valid/support/
      // quality 传 nullptr（corrected 数据面已保证 support>0; NaN 由 finite 过滤）
      P2EligibilityGatherInput gin{};
      gin.values = frame_major.data();
      gin.value_stride = tile_span;   // 每帧元素跨度（frame-major 契约）
      gin.value_dtype = 1;            // corrected bin 为 fp64（double 写出）
      gin.count = static_cast<std::uint32_t>(depth);
      gin.pixel = static_cast<std::uint32_t>(p);
      P2EligibilityGatherOutput gout{};
      compact_vals.assign(depth, 0.0);
      gout.values = compact_vals.data();
      gout.weights = nullptr;
      gout.support = nullptr;
      gout.frame_ids = nullptr;
      src_idx.assign(depth, 0u);
      gout.source_indices = src_idx.data();
      uint32_t eligible_count = 0;
      gout.eligible_count = &eligible_count;
      const int grc = p2_collect_candidate_stack(&gin, &gout);
      if (grc != 0)
        return Result<void>::fail(Error(ErrorDomain::INTERNAL,
            std::string("p2_collect_candidate_stack failed rc=") + std::to_string(grc)));
      uint8_t acc = 1;
      uint16_t nrej = 0;
      const uint16_t cand = static_cast<uint16_t>(eligible_count);
      if (eligible_count > 0 &&
          eligible_count > plan.underdetermined_n &&
          eligible_count >= static_cast<uint32_t>(plan.minimum_n)) {
        P2CandidateStack stack{};
        stack.values = compact_vals.data();
        stack.weights = nullptr;
        stack.frame_ids = nullptr;
        stack.count = eligible_count;
        stack.data_type = 1;
        P2RejectionDecision dec{};
        reasons.assign(eligible_count, 0);
        dec.reasons = reasons.data();
        const int krc = p2_reject_stack_ex(&stack, &plan, &dec);
        if (krc != 0)
          return Result<void>::fail(Error(ErrorDomain::INTERNAL,
              std::string("p2_reject_stack_ex failed rc=") + std::to_string(krc)));
        // per-sample reason 权威（kernel 冻结语义）: reason ∈ {ACCEPTED,
        // UNDERDETERMINED} 视为接受; rejected_low/high 只认 threshold 侧计数
        acc = 0;
        for (uint32_t s = 0; s < eligible_count; ++s) {
          if (reasons[s] == P2_REASON_ACCEPTED ||
              reasons[s] == P2_REASON_UNDERDETERMINED) { acc = 1; break; }
        }
        nrej = static_cast<uint16_t>(dec.rejected_low + dec.rejected_high);
        rej_low_total += dec.rejected_low;
        rej_high_total += dec.rejected_high;
      }
      // 候选不足/空栈 → UNDERDETERMINED（全接受并记录, 禁偷换算法）
      accepted_bin.push_back(acc);
      nrej_bin.push_back(nrej);
      cand_u16.push_back(cand);
      acc_total += acc;
      if (cand > 0 && (cand <= plan.underdetermined_n ||
                       cand < static_cast<uint32_t>(plan.minimum_n))) ++undet_total;
      ++n_pixels_processed;
    }
    tiles_j.push_back(Json{{"tile_ipix", tip},
                           {"n_pixels", tile_span},
                           {"offset", base}});
    out_offset += tile_span;
  }

  const std::string acc_file = out_dir + "/p2_rejection_accepted.bin";
  const std::string nrej_file = out_dir + "/p2_rejection_nrej.bin";
  const std::string cand_file = out_dir + "/p2_rejection_candidates.bin";
  if (!p2_write_bin(acc_file, accepted_bin) || !p2_write_bin(nrej_file, nrej_bin) ||
      !p2_write_bin(cand_file, cand_u16))
    return Result<void>::fail(Error(ErrorDomain::IO, "rejection bin write failed"));

  const std::string out_path = out_dir + "/p2_rejection.json";
  Json artifact = Json{{"schema", "DATA-P2-REJ"},
                       {"entry", "p2_reject_plan_resolve/p2_collect_candidate_stack/p2_reject_stack_ex"},
                       {"profile", profile},
                       {"plan", Json{{"method", plan.method},
                                     {"semantic_id", p2_rejection_semantic_id(plan.method)},
                                     {"minimum_n", plan.minimum_n},
                                     {"underdetermined_n", plan.underdetermined_n},
                                     {"normalization", plan.normalization}}},
                       {"tile_leaf_span", tile_span},
                       {"n_pixels", n_pixels_processed},
                       {"tiles", tiles_j},
                       {"stats", Json{{"accepted_pixels", acc_total},
                                      {"rejected_low", rej_low_total},
                                      {"rejected_high", rej_high_total},
                                      {"underdetermined_pixels", undet_total}}},
                       {"files", Json{{"accepted", acc_file},
                                      {"nrej", nrej_file},
                                      {"candidates", cand_file}}}};
  if (!p2_write_text(out_path, artifact.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed: " + out_path));
  (*man)["rejection_artifact"] = out_path;
  (*man)["reject_semantic_id"] = p2_rejection_semantic_id(plan.method);
  (*man)["n_pixels"] = n_pixels_processed;
  return Result<void>::success();
}

// ── op: integrate_frames（唯一真实入口 p2_validate_candidate_weights +
//      p2_integrate_pixel; 权重面 = DATA-UNC-001 §30.1 目标态合同:
//      weight_mode=2（科学默认）逐样本 ivar 逆方差, ivar 产品缺失 →
//      fail-closed（禁 support/snr²/常量 0 伪 variance; 唯一显式出口 =
//      legacy_allow_weight_fallback=true 的等权降级 + uncertainty_available=
//      false）; weight_mode=1 等权 + uncertainty_available=false）。
//      ivar_mosaic = Σ ivar_i（帧索引序, 正权样本）; variance = 1/W。──
Result<void> p2_op_integrate(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  Json cor_doc;
  const std::string cor_path = out_dir + "/p2_corrected.json";
  if (!p2_read_json(cor_path, &cor_doc))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream corrected artifact missing: " + cor_path +
        " (integrate consumes upm-apply output)"));
  Json rej_doc;
  const std::string rej_path = out_dir + "/p2_rejection.json";
  if (!p2_read_json(rej_path, &rej_doc))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream rejection artifact missing: " + rej_path +
        " (integrate consumes reject output)"));
  const auto& frames = cor_doc["frames"];
  if (!frames.is_array() || frames.empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "corrected artifact frames invalid"));
  const uint64_t tile_span = cor_doc.value("tile_leaf_span", kP2TileLeafSpan);

  int weight_mode = 2;
  if (doc.contains("weight_mode")) {
    if (!doc["weight_mode"].is_number_integer())
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "weight_mode must be integer (1=equal, 2=ivar)"));
    weight_mode = doc["weight_mode"].get<int>();
  }
  if (weight_mode != 1 && weight_mode != 2)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "weight_mode 0 (legacy SNR) is not a science variance surface in the"
        " node chain; only 1 (equal) or 2 (ivar) are legal (DATA-UNC-001 §30.1)"));
  const bool allow_fallback = doc.value("legacy_allow_weight_fallback", false);

  // ivar 产品读取（weight_mode=2 必须; 缺失 → fail-closed 或显式降级）
  struct IvarSet {
    AioHipsDataset* ds = nullptr;
    std::string path;
  };
  std::vector<IvarSet> ivar(frames.size());
  bool uncertainty_available = false;
  bool fallback = false;
  if (weight_mode == 2) {
    uint64_t ivar_missing = 0;
    for (size_t f = 0; f < frames.size(); ++f) {
      const std::string p = frames[f].value("hips_path", "");
      ivar[f].ds = aio_hips_open(p.c_str(), AIO_HIPS_RD_IVAR);
      ivar[f].path = p;
      if (!ivar[f].ds) ++ivar_missing;
    }
    if (ivar_missing > 0) {
      if (!allow_fallback) {
        for (auto& iv : ivar) if (iv.ds) aio_hips_close(iv.ds);
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "weight_mode=2 requires per-frame ivar products; " +
            std::to_string(ivar_missing) + "/" + std::to_string(frames.size()) +
            " frames missing ivar (DATA-UNC-001 §30.1: no silent fallback;"
            " set legacy_allow_weight_fallback=true for explicit equal-weight"
            " degradation with uncertainty_available=false)"));
      }
      fallback = true;   // §30.1 unavailable 规则 2（显式降级路径）
      for (auto& iv : ivar) { if (iv.ds) { aio_hips_close(iv.ds); iv.ds = nullptr; } }
    } else {
      uncertainty_available = true;
    }
  } else {
    uncertainty_available = false;   // mode 1: 等权非 ivar 语义面
  }
  struct IvarGuard {
    std::vector<IvarSet>* v;
    ~IvarGuard() { for (auto& iv : *v) if (iv.ds) aio_hips_close(iv.ds); }
  } ivar_guard{&ivar};

  // 帧 signal/support 重读（integrate 面资格权威: finite ∧ support>0）
  struct FrameDs { AioHipsDataset* sig = nullptr; AioHipsDataset* sup = nullptr; };
  std::vector<FrameDs> fds(frames.size());
  for (size_t f = 0; f < frames.size(); ++f) {
    const std::string p = frames[f].value("hips_path", "");
    fds[f].sig = aio_hips_open(p.c_str(), AIO_HIPS_RD_SIGNAL);
    fds[f].sup = aio_hips_open(p.c_str(), AIO_HIPS_RD_SUPPORT);
  }
  struct DsGuard {
    std::vector<FrameDs>* v;
    ~DsGuard() { for (auto& d : *v) { if (d.sig) aio_hips_close(d.sig);
                                        if (d.sup) aio_hips_close(d.sup); } }
  } ds_guard{&fds};

  // rejection bins（accepted u8 / nrej u16, tile 序拼接）
  const std::string acc_file = rej_doc["files"].value("accepted", "");
  const std::string nrej_file = rej_doc["files"].value("nrej", "");
  std::vector<uint8_t> acc_all;
  std::vector<uint16_t> nrej_all;
  if (!p2_read_bin_range<uint8_t>(acc_file, 0, rej_doc.value("n_pixels", 0ull), &acc_all) ||
      !p2_read_bin_range<uint16_t>(nrej_file, 0, rej_doc.value("n_pixels", 0ull), &nrej_all))
    return Result<void>::fail(Error(ErrorDomain::IO,
        "rejection bin read failed: " + acc_file));

  std::vector<double> sig_bin, sup_bin, wsum_bin;
  std::vector<int32_t> nused_bin, nrej_plane;
  Json tiles_j = Json::array();
  uint64_t zero_weight_pixels = 0, invalid_pixels = 0, nrej_total = 0;
  uint64_t nrej_pix_cursor = 0;
  uint64_t out_offset = 0;
  std::vector<float> ivar_buf(kP2TileLeafSpan), sup_buf(kP2TileLeafSpan);
  std::vector<double> vals, weights, supports;
  std::vector<uint8_t> accs;
  // corrected tile 查找索引（tile_ipix+frame → data_file（frame 级键）/offset）
  std::map<std::pair<uint64_t, uint64_t>, std::pair<std::string, uint64_t>> cor_index;
  for (size_t f = 0; f < frames.size(); ++f) {
    const std::string fdata = frames[f].value("data_file", "");
    for (const auto& t : frames[f]["tiles"]) {
      const uint64_t tip = t.value("tile_ipix", 0ull);
      cor_index[{tip, f}] = {fdata, t.value("offset", 0ull)};
    }
  }
  const auto& rej_tiles = rej_doc["tiles"];
  if (!rej_tiles.is_array() || rej_tiles.empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "rejection artifact tiles invalid"));
  for (const auto& rt : rej_tiles) {
    const uint64_t tip = rt.value("tile_ipix", 0ull);
    const uint64_t rej_off = rt.value("offset", 0ull);
    vals.clear(); weights.clear(); supports.clear(); accs.clear();
    // per-frame corrected tile 独立缓冲（tile 生存期; 禁共享 static 缓冲）
    std::vector<std::vector<double>> tile_bufs;
    std::vector<size_t> slot;
    for (size_t f = 0; f < frames.size(); ++f) {
      const auto it = cor_index.find({tip, f});
      if (it == cor_index.end()) continue;   // 该帧无此 tile → 不入栈
      std::vector<double> buf;
      if (!p2_read_bin_range<double>(it->second.first, it->second.second,
                                     tile_span, &buf))
        return Result<void>::fail(Error(ErrorDomain::IO,
            "corrected bin read failed (tile " + std::to_string(tip) +
            " frame " + std::to_string(f) + ")"));
      tile_bufs.push_back(std::move(buf));
      slot.push_back(f);
    }
    std::vector<const std::vector<double>*> tile_v;
    tile_v.reserve(tile_bufs.size());
    for (const auto& b : tile_bufs) tile_v.push_back(&b);
    const uint64_t depth = tile_v.size();
    // 该 tile 各帧 support/ivar tile（read_tile_f32; 缺失 → 该像素零权/无支持）
    std::vector<bool> has_sup(depth, false), has_ivar(depth, false);
    std::vector<std::vector<float>> sup_v(static_cast<size_t>(depth));
    std::vector<std::vector<float>> ivar_v(static_cast<size_t>(depth));
    for (size_t d = 0; d < depth; ++d) {
      const size_t f = slot[d];
      if (fds[f].sup &&
          aio_hips_read_tile_f32(fds[f].sup, tip, sup_buf.data()) == 0) {
        sup_v[d] = sup_buf;
        has_sup[d] = true;
      }
      if (ivar[f].ds &&
          aio_hips_read_tile_f32(ivar[f].ds, tip, ivar_buf.data()) == 0) {
        ivar_v[d] = ivar_buf;
        has_ivar[d] = true;
      }
    }
    for (uint64_t p = 0; p < tile_span; ++p) {
      vals.clear(); weights.clear(); supports.clear(); accs.clear();
      const uint64_t rej_pix = rej_off + p;
      if (rej_pix >= acc_all.size())
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "rejection plane index out of range (tile " + std::to_string(tip) + ")"));
      for (size_t d = 0; d < depth; ++d) {
        const double v = (*tile_v[d])[static_cast<size_t>(p)];
        const double sp = has_sup[d]
            ? static_cast<double>(sup_v[d][static_cast<size_t>(p)]) : 0.0;
        const bool acc = acc_all[static_cast<size_t>(rej_pix)] != 0;
        // 调用方资格（SCI-INT §5 valid ∧ W>0 面）: finite ∧ support>0 ∧ accepted;
        // 先资格过滤后权重面 —— 无覆盖像素（support=0 → corrected NaN → 过滤）
        // 不进入权重检查（ivar 产品在无覆盖像素 = NaN 同态, §30.1 表注 F-UNC-001）
        if (!std::isfinite(v) || !std::isfinite(sp) || sp <= 0.0 || !acc) continue;
        double w = 1.0;
        if (weight_mode == 2 && !fallback) {
          // 入栈样本的 ivar 契约检查（§20.1 读侧: ivar==0 合法零权重,
          // nonfinite/负 = 产品损坏 hard fail, 禁 clamp/skip）
          if (!has_ivar[d])
            return Result<void>::fail(Error(ErrorDomain::DATA,
                "ivar tile read failed where corrected data exists (frame " +
                std::to_string(slot[d]) + " tile " + std::to_string(tip) + ")"));
          w = static_cast<double>(ivar_v[d][static_cast<size_t>(p)]);
          if (!std::isfinite(w) || w < 0.0)
            return Result<void>::fail(Error(ErrorDomain::DATA,
                "non-finite/negative input ivar at frame " +
                std::to_string(slot[d]) + " tile " + std::to_string(tip) +
                " pixel " + std::to_string(p) +
                " (DATA-UNC-001 §30.1: p2_validate_candidate_weights hard"
                " fail, no clamp/no skip)"));
        }
        vals.push_back(v);
        weights.push_back(w);
        supports.push_back(sp);
        accs.push_back(1);
      }
      P2PixelStack stk{};
      stk.values = vals.data();
      stk.weights = weights.empty() ? nullptr : weights.data();
      stk.support = supports.empty() ? nullptr : supports.data();
      stk.accepted = accs.empty() ? nullptr : accs.data();
      stk.count = static_cast<std::uint32_t>(vals.size());
      P2PixelResult pr{};
      const int irc = p2_integrate_pixel(&stk, &pr);
      if (irc != 0)
        return Result<void>::fail(Error(ErrorDomain::INTERNAL,
            std::string("p2_integrate_pixel failed rc=") + std::to_string(irc)));
      // 权重资格守卫（构建后 p2_validate_candidate_weights; 合同要求）
      if (p2_validate_candidate_weights(weights.empty() ? nullptr : weights.data(),
                                        static_cast<std::uint32_t>(weights.size())) != 0)
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "candidate weights validation failed (tile " + std::to_string(tip) +
            " pixel " + std::to_string(p) + ")"));
      double wsum = 0.0;
      for (size_t i = 0; i < vals.size(); ++i) {
        if (weights[i] > 0.0 && std::isfinite(weights[i])) wsum += weights[i];
      }
      double signal = pr.signal;
      if (pr.status == P2_INTEGRATE_OK) {
        if (!std::isfinite(wsum) || wsum <= 0.0) {
          signal = std::numeric_limits<double>::quiet_NaN();   // 病态 → NaN
          wsum = std::numeric_limits<double>::quiet_NaN();
          ++invalid_pixels;
        }
      } else {
        // 无有效样本: NaN/NaN 同态（§30.1 invalid policy 第 1 行, 禁 0/±Inf 伪装）
        signal = std::numeric_limits<double>::quiet_NaN();
        wsum = std::numeric_limits<double>::quiet_NaN();
        if (pr.status == P2_INTEGRATE_ZERO_VALID_WEIGHT) ++zero_weight_pixels;
      }
      sig_bin.push_back(signal);
      sup_bin.push_back(pr.support);   // canonical reducer max(accepted support)
      wsum_bin.push_back(wsum);        // ivar_mosaic（§30.1: ivar = W）
      nused_bin.push_back(static_cast<int32_t>(pr.n_used));
      const int32_t nrej_p = static_cast<int32_t>(nrej_all[static_cast<size_t>(rej_pix)]);
      nrej_plane.push_back(nrej_p);
      nrej_total += static_cast<uint64_t>(nrej_p);
      ++nrej_pix_cursor;
    }
    tiles_j.push_back(Json{{"tile_ipix", tip},
                           {"n_pixels", tile_span},
                           {"offset", out_offset}});
    out_offset += tile_span;
  }

  const std::string sig_file = out_dir + "/p2_integrated_signal.bin";
  const std::string sup_file = out_dir + "/p2_integrated_support.bin";
  const std::string wsum_file = out_dir + "/p2_integrated_wsum.bin";
  const std::string nused_file = out_dir + "/p2_integrated_nused.bin";
  const std::string nrej_file_out = out_dir + "/p2_integrated_nrej.bin";
  if (!p2_write_bin(sig_file, sig_bin) || !p2_write_bin(sup_file, sup_bin) ||
      !p2_write_bin(wsum_file, wsum_bin) || !p2_write_bin(nused_file, nused_bin) ||
      !p2_write_bin(nrej_file_out, nrej_plane))
    return Result<void>::fail(Error(ErrorDomain::IO, "integrated bin write failed"));

  const std::string out_path = out_dir + "/p2_integrated.json";
  Json artifact = Json{{"schema", "DATA-P2-INT"},
                       {"entry", "p2_validate_candidate_weights/p2_integrate_pixel"},
                       {"weight_mode", weight_mode},
                       {"fallback", fallback},
                       {"uncertainty_available", uncertainty_available},
                       {"tile_leaf_span", tile_span},
                       {"n_pixels", nrej_pix_cursor},
                       {"tiles", tiles_j},
                       {"diagnostics", Json{{"zero_valid_weight_pixels", zero_weight_pixels},
                                            {"nonfinite_result_pixels", invalid_pixels},
                                            {"nrej_total", nrej_total}}},
                       {"files", Json{{"signal", sig_file},
                                      {"support", sup_file},
                                      {"wsum", wsum_file},
                                      {"nused", nused_file},
                                      {"nrej", nrej_file_out}}}};
  if (!p2_write_text(out_path, artifact.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed: " + out_path));
  (*man)["integrated_artifact"] = out_path;
  (*man)["weight_mode"] = weight_mode;
  (*man)["uncertainty_available"] = uncertainty_available;
  return Result<void>::success();
}

// ── op: write_mosaic（唯一真实入口 aio_hips_product_begin /
//      aio_hips_write_signal_support_tile / aio_hips_write_variance_tile /
//      aio_hips_finalize — AIO-002 原子发布原语内建于 aio_hips 落盘路径;
//      AIO 唯一 writer, 禁手写 FITS）。
//      variance/ivar = DATA-P2-VAR-001 §30.1: ivar_mosaic=W=Σivar_i、
//      variance=1/W 经 writer 通道（var_num_sum = variance×cov²,
//      writer 归约 variance = var_num_sum/covered_area²）; n_used=0 →
//      cov=0 → NaN/NaN 同态（writer 通道 §12.4 冻结合同）。nused/nrej
//      子产品位（DATA-P2-REJ-001 §30.2, 位 32/64）待 AIO 域实现, 现以
//      integrated bins 诊断面承载（artifact 如实登记, 不冒充子产品）。──
Result<void> p2_op_write(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  Json int_doc;
  const std::string int_path = out_dir + "/p2_integrated.json";
  if (!p2_read_json(int_path, &int_doc))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream integrated artifact missing: " + int_path +
        " (write consumes integrate output)"));
  Json cov_doc;
  const std::string cov_path = out_dir + "/p2_coverage.json";
  if (!p2_read_json(cov_path, &cov_doc))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream coverage artifact missing: " + cov_path));
  const int target_order = cov_doc.value("target_order", 0);
  if (target_order < 0 || target_order > 20)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "coverage target_order out of range (got " +
        std::to_string(target_order) + ")"));
  // target_order 语义 = coverage MOC/tile 父单元 order（AIO properties
  // hips_order 同面, P2MocCell.order 同值）; mosaic 叶级 nside =
  // 2^(target_order+9)（tile 512² = 2^9 leaf）→ nside>=512 IVOA 标准恒满足
  // （target_order>=0）; nside<512 仅当上游 HiPS tile_width<512 —— coverage
  // 层 tw==512 硬校验已拦截。
  const uint32_t nside =
      1u << static_cast<uint32_t>(target_order + 9);
  const uint64_t tile_span = int_doc.value("tile_leaf_span", kP2TileLeafSpan);
  const bool uncertainty_available = int_doc.value("uncertainty_available", false);
  const int weight_mode = int_doc.value("weight_mode", 0);
  const double a_cell = 4.0 * 3.14159265358979323846 /
                        (12.0 * static_cast<double>(nside) * static_cast<double>(nside));

  // provenance 面（§30.3 键值来源; 键写入 properties 的 AIO 通道缺口见 artifact）
  std::string manifest_hash, model_hash, reject_profile;
  {
    Json smp_doc;
    if (p2_read_json(out_dir + "/p2_samples.json", &smp_doc))
      manifest_hash = smp_doc.value("input_manifest_hash", "");
    Json umd_doc;
    if (p2_read_json(out_dir + "/p2_upm_model.json", &umd_doc))
      model_hash = umd_doc.value("model_hash", "");
    Json rej_doc;
    if (p2_read_json(out_dir + "/p2_rejection.json", &rej_doc))
      reject_profile = rej_doc.value("profile", "");
  }

  int flags = AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT;
  if (uncertainty_available) flags |= AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR;
  std::string obs_filter;
  {
    const auto& frames = cov_doc["frames"];
    if (frames.is_array() && !frames.empty())
      obs_filter = frames[0].value("filter_passband", "");
  }
  AioHipsProductSet* ps = aio_hips_product_begin(
      out_dir.c_str(), nside, 512, AIO_HIPS_FLOAT32, flags,
      "ivo://astrocs/phase2", "AstroCS Phase2 mosaic",
      obs_filter.empty() ? nullptr : obs_filter.c_str(),
      0.0, nullptr, 0);
  if (!ps)
    return Result<void>::fail(Error(ErrorDomain::IO,
        std::string("aio_hips_product_begin failed: ") + aio_hips_last_error()));

  const auto& files = int_doc["files"];
  const auto& tiles = int_doc["tiles"];
  // integrated bins 像素序 = 标准 tile FITS row-major（upm-apply/integrate
  // 同一序）; writer view 合同 = NESTED local 序（write_variance_tile 内
  // nested_local_to_fits_index 归约, healpix_core 单一权威逆映射）。单一路径:
  // fits→nested LUT 一次构建, 三通道同序重排。
  std::vector<uint32_t> fits_to_local(kP2TileLeafSpan);
  for (uint64_t i = 0; i < kP2TileLeafSpan; ++i)
    fits_to_local[static_cast<size_t>(i)] = static_cast<uint32_t>(
        astrocs::healpix::fits_index_to_nested_local(i, kP2TileShift, 512u));
  std::vector<float> flux_buf(kP2TileLeafSpan), cov_buf(kP2TileLeafSpan),
      varnum_buf(kP2TileLeafSpan);
  int64_t n_tiles_written = 0;
  for (const auto& t : tiles) {
    const uint64_t tip = t.value("tile_ipix", 0ull);
    const uint64_t off = t.value("offset", 0ull);
    std::vector<double> sig_v, sup_v, wsum_v;
    if (!p2_read_bin_range<double>(files.value("signal", ""), off, tile_span, &sig_v) ||
        !p2_read_bin_range<double>(files.value("support", ""), off, tile_span, &sup_v) ||
        !p2_read_bin_range<double>(files.value("wsum", ""), off, tile_span, &wsum_v))
      return Result<void>::fail(Error(ErrorDomain::IO,
          "integrated bin read failed (tile " + std::to_string(tip) + ")"));
    for (uint64_t i = 0; i < tile_span; ++i) {
      const size_t local = fits_to_local[static_cast<size_t>(i)];   // NESTED local
      const double sup = sup_v[static_cast<size_t>(i)];
      const double cov = (std::isfinite(sup) && sup > 0.0) ? sup * a_cell : 0.0;
      const double sig = sig_v[static_cast<size_t>(i)];
      // P1 writer 同构: flux_sum = signal × cov（writer 归约 signal = flux/area）
      flux_buf[local] = static_cast<float>(sig * cov);
      cov_buf[local] = static_cast<float>(cov);
      // §30.1: var_num_sum = variance_mosaic × cov² = cov²/W（writer 归约
      // variance = var_num_sum/cov²）; W 病态/无样本 → NaN（writer NaN 同态）
      if (uncertainty_available) {
        const double w = wsum_v[static_cast<size_t>(i)];
        varnum_buf[local] =
            static_cast<float>((std::isfinite(w) && w > 0.0)
                                   ? (cov * cov) / w
                                   : std::numeric_limits<double>::quiet_NaN());
      }
    }
    AstroSphereTileView view;
    std::memset(&view, 0, sizeof(view));
    view.parent_ipix = tip;
    view.leaf_order = static_cast<uint32_t>(target_order + 9);   // == ps->leaf_order
    view.width = 512;
    view.data_type = AIO_HIPS_FLOAT32;
    view.flux_sum = flux_buf.data();
    view.covered_area = cov_buf.data();
    view.valid_mask = nullptr;
    view.var_num_sum = uncertainty_available ? varnum_buf.data() : nullptr;
    const int wr = aio_hips_write_signal_support_tile(ps, &view);
    if (wr != 0) {
      aio_hips_abort(ps);
      return Result<void>::fail(Error(ErrorDomain::IO,
          std::string("aio_hips_write_signal_support_tile failed: ") +
          aio_hips_last_error()));
    }
    if (uncertainty_available) {
      const int wv = aio_hips_write_variance_tile(ps, &view);
      if (wv != 0) {
        aio_hips_abort(ps);
        return Result<void>::fail(Error(ErrorDomain::IO,
            std::string("aio_hips_write_variance_tile failed: ") +
            aio_hips_last_error()));
      }
    }
    ++n_tiles_written;
  }
  if (aio_hips_finalize(ps) != 0) {
    aio_hips_abort(ps);
    return Result<void>::fail(Error(ErrorDomain::IO,
        std::string("aio_hips_finalize failed: ") + aio_hips_last_error()));
  }
  std::error_code ec;
  const std::string props = out_dir + "/signal/properties";
  if (!std::filesystem::exists(std::filesystem::u8path(props), ec))
    return Result<void>::fail(Error(ErrorDomain::IO,
        "HiPS properties missing after finalize: " + props));

  // 产品面回读校验（variance/ivar tile 数一致; HIPS_VERIFY 目标态 §30.1 扩展）
  Json products = Json::array({"signal", "support"});
  if (uncertainty_available) {
    products.push_back("variance");
    products.push_back("ivar");
    AioHipsDataset* dv = aio_hips_open(out_dir.c_str(), AIO_HIPS_RD_VARIANCE);
    AioHipsDataset* di = aio_hips_open(out_dir.c_str(), AIO_HIPS_RD_IVAR);
    const bool ok = dv && di &&
                    aio_hips_tile_count(dv) == aio_hips_tile_count(di);
    if (dv) aio_hips_close(dv);
    if (di) aio_hips_close(di);
    if (!ok)
      return Result<void>::fail(Error(ErrorDomain::IO,
          "variance/ivar product tile count mismatch after finalize (HIPS_VERIFY)"));
  }

  const std::string out_path = out_dir + "/p2_final.json";
  Json final_out = Json{{"schema", "DATA-P2-RES"},
                        {"entry", "aio_hips_product_begin/write_signal_support_tile/write_variance_tile/finalize"},
                        {"hips_root", out_dir},
                        {"nside", nside},
                        {"target_order", target_order},
                        {"n_tiles_written", n_tiles_written},
                        {"products", products},
                        {"covered_area_model", "support_x_A_cell"},
                        {"uncertainty_available", uncertainty_available},
                        {"weight_mode", weight_mode},
                        {"provenance", Json{
                            {"ASTROCS_INPUT_MANIFEST_HASH", manifest_hash},
                            {"ASTROCS_MODEL_HASH", model_hash},
                            {"ASTROCS_UNCERTAINTY_AVAILABLE",
                             uncertainty_available ? "true" : "false"},
                            {"ASTROCS_WEIGHT_MODE", weight_mode},
                            {"ASTROCS_REJECT_PROFILE", reject_profile}}},
                        {"properties", props},
                        {"pending_contracts", Json{
                            {"nused_nrej_planes",
                             "DATA-P2-REJ-001 §30.2 AIO bits 32/64: writer"
                             " channels not implemented in AIO domain; planes"
                             " carried by p2_integrated_n{used,rej}.bin"
                             " (diagnostic surface, not science planes)"},
                            {"properties_provenance_channel",
                             "DATA-P2-PROV-001 §30.3: AIO writer provenance"
                             " channel (aio_hips_set_drizzle_provenance) only"
                             " exposes pixfrac/scale; ASTROCS_* property keys"
                             " pending AIO-domain contract registration"}}}};
  if (!p2_write_text(out_path, final_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["mosaic_root"] = out_dir;
  (*man)["final_artifact"] = out_path;
  (*man)["n_tiles_written"] = n_tiles_written;
  (*man)["uncertainty_available"] = uncertainty_available;
  return Result<void>::success();
}
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

// ── P2NodeModule: Phase2 唯一真实 operation 节点适配器（IModule）────────────
enum class P2NodeOp { Coverage, Sample, UpmFit, UpmApply, Reject, Integrate, Write };
struct P2NodeSpec {
  P2NodeOp op;
  const char* operation;  // module_ports.registry.json 冻结 operation 名
  const char* entry;      // 冻结唯一真实入口名（节点 manifest 可审计标记）
};

struct P2NodeModule : public IModule {
  ModuleDescriptor desc_;
  P2NodeSpec spec_;
  std::string config_;
  std::string manifest_;
  uint32_t workers_ = 2;

  P2NodeModule(ModuleDescriptor d, P2NodeSpec s)
      : desc_(std::move(d)), spec_(s) {}

  const ModuleDescriptor& descriptor() const noexcept override { return desc_; }

  // config 合同: hips_paths（非空 string 数组）+ output_dir（string）必填;
  // upm/reject 对象可选; weight_mode ∈ {1,2}（0=legacy SNR 非科学方差面,
  // DATA-UNC-001 §30.1 目标态）; 其余科学参数无 silent default（op 内
  // fail-closed 校验, 不提前消费缺省值）。
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
    for (const char* req : {"hips_paths", "output_dir"}) {
      if (!doc.contains(req))
        return Result<void>::fail(Error(ErrorDomain::DATA,
            std::string("missing key '") + req + "'"));
    }
    if (!doc["hips_paths"].is_array() || doc["hips_paths"].empty() ||
        !doc["output_dir"].is_string())
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "hips_paths must be non-empty array; output_dir must be string"));
    for (const auto& p : doc["hips_paths"])
      if (!p.is_string())
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "hips_paths items must be strings"));
    if (doc.contains("upm") && !doc["upm"].is_object())
      return Result<void>::fail(Error(ErrorDomain::DATA, "upm must be object"));
    if (doc.contains("reject") && !doc["reject"].is_object())
      return Result<void>::fail(Error(ErrorDomain::DATA, "reject must be object"));
    if (doc.contains("weight_mode") && !doc["weight_mode"].is_number_integer())
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "weight_mode must be integer (1=equal, 2=ivar)"));
    if (doc.contains("legacy_allow_weight_fallback") &&
        !doc["legacy_allow_weight_fallback"].is_boolean())
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "legacy_allow_weight_fallback must be boolean"));
    return Result<void>::success();
  }

  Result<ModulePlan> plan(const std::string& node_id,
                          const std::string& config_json) override {
    config_ = config_json;
    ModulePlan p;
    p.node_id = node_id;
    p.work_units = 1;
    p.parallel_axes = {"tile-leaf-band"};
    p.cpu_heavy = desc_.execution_class == "cpu_heavy";
    return Result<ModulePlan>::ok(std::move(p));
  }

  Result<void> execute(RunContext& ctx) override {
    const uint32_t host_workers =
        ctx.budget() ? ctx.budget()->budget() : workers_;
    ThreadLease lease = ctx.acquire_lease(host_workers);
    const uint32_t cap = lease.acquired() ? lease.size() : 1u;
    HostSession hs;
    if (!hs.init(cap)) {
      return Result<void>::fail(Error(ErrorDomain::RESOURCE,
          desc_.module_id + ": host services init failed"));
    }
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
    Json man = Json{{"kind", "astrocs.phase2.node"},
                    {"module_id", desc_.module_id},
                    {"operation", spec_.operation},
                    {"entry", spec_.entry},
                    {"artifact_type", desc_.data_id},
                    {"availability", "available"},
                    {"status", "running"}};
    Result<void> r = Result<void>::success();
    try {
      Json doc = Json::parse(config_);
      // typed artifact 落盘面: output_dir 由节点幂等创建（执行面职责;
      // CLI/测试调用方不预建目录, p2001 pytest out1/outN 同面）
      {
        const std::string out_dir = doc.value("output_dir", std::string("."));
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::u8path(out_dir), ec);
        if (ec && !std::filesystem::exists(std::filesystem::u8path(out_dir), ec)) {
          man["error"] = "cannot create output_dir: " + out_dir;
          r = Result<void>::fail(Error(ErrorDomain::IO, man["error"].get<std::string>()));
        }
      }
      if (r.ok()) {
      // sampler/upm cpu_workers = Runtime lease 权威（CON-004/005; budget 唯一
      // 权威, 禁硬编码; 节点 op 内以 doc 键覆盖注入）。
      Json cfg2 = doc;
      cfg2["__workers"] = cap;
      switch (spec_.op) {
        case P2NodeOp::Coverage:  r = p2_op_coverage(cfg2, &man); break;
        case P2NodeOp::Sample:    r = p2_op_sample(cfg2, &man); break;
        case P2NodeOp::UpmFit:    r = p2_op_upm_fit(cfg2, &man); break;
        case P2NodeOp::UpmApply:  r = p2_op_upm_apply(cfg2, &man); break;
        case P2NodeOp::Reject:    r = p2_op_reject(cfg2, &man); break;
        case P2NodeOp::Integrate: r = p2_op_integrate(cfg2, &man); break;
        case P2NodeOp::Write:     r = p2_op_write(cfg2, &man); break;
      }
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

std::unique_ptr<IModule> make_p2_node_module(ModuleDescriptor desc, P2NodeSpec spec) {
  return std::make_unique<P2NodeModule>(std::move(desc), spec);
}

// ══ P3-002: Phase3 唯一真实 operation 节点适配器（IModule）═══════════════════
// 五节点链 source→properties→wcs→resample→writer→verify; 每节点唯一真实
// operation 委托（宪章 §7.2 "投影规划和重采样是独立算法节点, 不得重复调用
// 完整 phase3_session_run()" + §8.2 每节点唯一 entrypoint/call count;
// P1-001/P2-001 整改同构——原工厂委托 P3Api session adapter = 每个子节点
// 调用完整 p3_session_run, 5 节点链重复执行全链 5 次, 违规）:
//   properties → p3_sampler_open_ex + p3_uncertainty_open探测 (ALG-P3-001)
//   wcs        → p3_wcs_make + p3_wcs_fits_keywords (ALG-P3-002)
//   resample   → p3_order_select + p3_sample_{nearest,bilinear}_ex +
//                p3_uncertainty_propagate (ALG-P3-003 + DATA-P3-UNC-001 §30.4)
//   writer     → p3_output_write_atomic_ex (ALG-P3-004 + §30.4 VARIANCE/IVAR
//                HDU 目标态)
//   verify     → p3_output_verify_ex (独立重开; unavailable 双向防占位)
// 节点间 typed artifact 经 output_dir 文件约定传递（P2 先例同构）:
//   p3_props.json → p3_wcs.json → p3_resampled.{json,bin} →
//   output_phase3.fits + p3_writer.json → p3_verify.json。
// operation/entry 名与 runtime/pipeline/module_ports.registry.json 冻结绑定
// 表一致; manifest 携带标记供 trace/审计。

namespace {

using Json = nlohmann::json;

// ── 共用 helper ──────────────────────────────────────────────────────────────
// 请求几何段 (与 p3_session parse_request 同合同面; 值域在 op 内 fail-closed)
struct P3nGeom {
  std::string hips_dir;
  std::string out_dir;
  double ra = 0, dec = 0;
  double scale = 0;
  int w = 0, h = 0;
  std::string sampler = "bilinear";
  std::string parity = "east_left";
  int bitpix = -32;
};

bool p3n_geom(const Json& doc, P3nGeom* g, std::string* err) {
  auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
  if (!doc.contains("source") || !doc["source"].is_object() ||
      !doc["source"].contains("hips_dir") || !doc["source"]["hips_dir"].is_string())
    return fail("missing source.hips_dir");
  if (!doc.contains("center") || !doc["center"].is_object() ||
      !doc["center"].contains("ra_deg") || !doc["center"].contains("dec_deg"))
    return fail("missing center.ra_deg/dec_deg");
  if (!doc.contains("output_dir") || !doc["output_dir"].is_string())
    return fail("missing output_dir");
  g->hips_dir = doc["source"]["hips_dir"].get<std::string>();
  g->out_dir = doc["output_dir"].get<std::string>();
  g->ra = doc["center"]["ra_deg"].get<double>();
  g->dec = doc["center"]["dec_deg"].get<double>();
  g->scale = doc.value("scale_deg_per_px", 0.0);
  g->w = doc.value("width_px", 0);
  g->h = doc.value("height_px", 0);
  g->sampler = doc.value("sampler", std::string("bilinear"));
  g->parity = doc.value("longitude_parity", std::string("east_left"));
  g->bitpix = doc.value("bitpix", -32);
  // 值域 (与 p3_session 同款; 无 silent default 非法值)
  if (!(g->scale > 0.0)) return fail("scale_deg_per_px must be > 0");
  if (g->w < 1 || g->w > 20000 || g->h < 1 || g->h > 20000)
    return fail("width_px/height_px must be in [1,20000]");
  if (std::fabs(g->dec) > 85.0) return fail("abs(center.dec_deg) must be <= 85");
  if (g->sampler != "nearest" && g->sampler != "bilinear")
    return fail("sampler must be nearest|bilinear");
  if (g->parity != "east_left" && g->parity != "east_right")
    return fail("longitude_parity must be east_left|east_right");
  if (g->bitpix != -32 && g->bitpix != -64) return fail("bitpix must be -32|-64");
  return true;
}

// 上游 artifact 读取 (fail-closed: 缺失/损坏 = DATA 拒, DAG 断链不静默)
bool p3n_read_json(const std::string& path, Json* out, std::string* err) {
  std::ifstream f(path, std::ios::binary);
  if (!f) { if (err) *err = "upstream artifact missing: " + path; return false; }
  std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  try {
    *out = Json::parse(s);
  } catch (const Json::parse_error& e) {
    if (err) *err = "artifact parse: " + path + ": " + e.what();
    return false;
  }
  return true;
}

bool p3n_wcs_from_json(const Json& j, astrocs::phase3::P3WcsDescriptor* d,
                       std::string* err) {
  using namespace astrocs::phase3;
  auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
  if (j.value("schema", std::string()) != "DATA-P3-WCS")
    return fail("wcs_plan schema mismatch (expected DATA-P3-WCS)");
  *d = P3WcsDescriptor{};
  d->crval_ra_deg = j.value("crval_ra_deg", 0.0);
  d->crval_dec_deg = j.value("crval_dec_deg", 0.0);
  d->crpix_x = j.value("crpix_x", 0.0);
  d->crpix_y = j.value("crpix_y", 0.0);
  d->cd[0][0] = j.value("cd11", 0.0);
  d->cd[0][1] = j.value("cd12", 0.0);
  d->cd[1][0] = j.value("cd21", 0.0);
  d->cd[1][1] = j.value("cd22", 0.0);
  d->width_px = j.value("width_px", 0);
  d->height_px = j.value("height_px", 0);
  d->projection = "TAN";
  return true;
}

// ── op: properties (ALG-P3-001 唯一真实入口 = 严格 properties 校验 + 实测
//    order/BUNIT + uncertainty 子产品探测) ────────────────────────────────────
Result<void> p3_op_properties(const Json& doc, Json* man) {
  P3nGeom g;
  std::string err;
  if (!p3n_geom(doc, &g, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  using namespace astrocs::phase3;
  P3Sampler samp{};
  int order = -1;
  std::string bunit, serr;
  const P3ResampleStatus st =
      p3_sampler_open_ex(g.hips_dir.c_str(), &samp, &order, &bunit, &serr);
  if (st != P3_RS_OK) {
    const ErrorDomain dom = (st == P3_RS_IO) ? ErrorDomain::IO : ErrorDomain::DATA;
    return Result<void>::fail(Error(dom, "p3_sampler_open_ex: " + serr));
  }
  p3_sampler_close(&samp);
  // uncertainty 子产品探测 (properties/order 非法 = 产品损坏显式拒, §30.4)
  P3UncertaintySource src = P3_UNC_NONE;
  const P3ResampleStatus ust = p3_uncertainty_open(g.hips_dir.c_str(), order, &src, nullptr);
  if (ust == P3_RS_PARAM)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "uncertainty sub-product corrupt (properties/order mismatch)"));
  if (ust != P3_RS_OK)
    return Result<void>::fail(Error(ErrorDomain::IO, "uncertainty sub-product open failed"));

  const std::string path = g.out_dir + "/p3_props.json";
  Json props{{"schema", "DATA-P3-PROPS"},
             {"hips_dir", g.hips_dir},
             {"hips_order", order},
             {"tile_width", 512},
             {"bunit", bunit},
             {"variance_available", src == P3_UNC_VARIANCE},
             {"ivar_available", src == P3_UNC_IVAR},
             {"uncertainty_source",
              src == P3_UNC_VARIANCE ? "variance"
                                     : (src == P3_UNC_IVAR ? "ivar" : "none")}};
  std::ofstream f(path, std::ios::binary);
  if (!f) return Result<void>::fail(Error(ErrorDomain::IO, "cannot write p3_props.json"));
  f << props.dump(2) << "\n";
  f.close();
  if (!f.good()) return Result<void>::fail(Error(ErrorDomain::IO, "p3_props.json write failed"));
  (*man)["props_artifact"] = path;
  (*man)["hips_order"] = order;
  (*man)["bunit"] = bunit;
  (*man)["uncertainty_source"] = props["uncertainty_source"];
  return Result<void>::success();
}

// ── op: wcs (ALG-P3-002 唯一真实入口 = WCS plan 构造 + FITS 关键字文本) ─────
Result<void> p3_op_wcs(const Json& doc, Json* man) {
  P3nGeom g;
  std::string err;
  if (!p3n_geom(doc, &g, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  // 上游 props artifact fail-closed (DAG 端口语义)
  Json props;
  if (!p3n_read_json(g.out_dir + "/p3_props.json", &props, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  using namespace astrocs::phase3;
  P3WcsDescriptor wcs{};
  const P3WcsStatus wst = p3_wcs_make(g.ra, g.dec, g.scale, g.w, g.h,
                                      g.parity.c_str(), 0.0, &wcs);
  if (wst != P3_WCS_OK) {
    return Result<void>::fail(
        Error(ErrorDomain::DATA,
              std::string("p3_wcs_make rejected: ") +
                  (wst == P3_WCS_UNSUPPORTED ? "projection unsupported"
                   : wst == P3_WCS_HEMISPHERE ? "output crosses TAN hemisphere"
                                              : "parameter out of range")));
  }
  const std::string path = g.out_dir + "/p3_wcs.json";
  Json plan{{"schema", "DATA-P3-WCS"},
            {"crval_ra_deg", wcs.crval_ra_deg},
            {"crval_dec_deg", wcs.crval_dec_deg},
            {"crpix_x", wcs.crpix_x},
            {"crpix_y", wcs.crpix_y},
            {"cd11", wcs.cd[0][0]},
            {"cd12", wcs.cd[0][1]},
            {"cd21", wcs.cd[1][0]},
            {"cd22", wcs.cd[1][1]},
            {"width_px", wcs.width_px},
            {"height_px", wcs.height_px},
            {"fits_keywords", p3_wcs_fits_keywords(&wcs)}};
  std::ofstream f(path, std::ios::binary);
  if (!f) return Result<void>::fail(Error(ErrorDomain::IO, "cannot write p3_wcs.json"));
  f << plan.dump(2) << "\n";
  f.close();
  if (!f.good()) return Result<void>::fail(Error(ErrorDomain::IO, "p3_wcs.json write failed"));
  (*man)["wcs_plan_artifact"] = path;
  return Result<void>::success();
}

// ── op: resample (ALG-P3-003 唯一真实入口 = order 选择 + 反向映射采样 +
//    DATA-P3-UNC-001 §30.4 不确定度传播; 重计算面, 行带 work unit 经 Runtime
//    唯一 executor 执行 — RT-001) ─
Result<void> p3_op_resample(const Json& doc, Json* man, uint32_t cap,
                            RunContext* ctx) {
  P3nGeom g;
  std::string err;
  if (!p3n_geom(doc, &g, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  Json props, plan;
  if (!p3n_read_json(g.out_dir + "/p3_props.json", &props, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  if (!p3n_read_json(g.out_dir + "/p3_wcs.json", &plan, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  using namespace astrocs::phase3;
  P3WcsDescriptor wcs{};
  if (!p3n_wcs_from_json(plan, &wcs, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  if (wcs.width_px != g.w || wcs.height_px != g.h)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "wcs_plan geometry mismatch with config"));

  P3Sampler samp{};
  int input_order = 20;
  std::string bunit, serr;
  const P3ResampleStatus sst =
      p3_sampler_open_ex(g.hips_dir.c_str(), &samp, &input_order, &bunit, &serr);
  if (sst != P3_RS_OK) {
    const ErrorDomain dom = (sst == P3_RS_IO) ? ErrorDomain::IO : ErrorDomain::DATA;
    return Result<void>::fail(Error(dom, "p3_sampler_open_ex: " + serr));
  }
  P3UncertaintySource src = P3_UNC_NONE;
  P3Sampler u_samp{};
  {
    const P3ResampleStatus ust =
        p3_uncertainty_open(g.hips_dir.c_str(), input_order, &src, &u_samp);
    if (ust == P3_RS_PARAM) {
      p3_sampler_close(&samp);
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "uncertainty sub-product corrupt (properties/order mismatch)"));
    }
    if (ust != P3_RS_OK) {
      p3_sampler_close(&samp);
      return Result<void>::fail(Error(ErrorDomain::IO,
          "uncertainty sub-product open failed"));
    }
  }
  const bool unc_available = (src != P3_UNC_NONE);
  // props artifact 的 uncertainty 声明与实测一致性 (上游/下游不漂移)
  const std::string props_src = props.value("uncertainty_source", std::string("none"));
  const std::string live_src =
      src == P3_UNC_VARIANCE ? "variance" : (src == P3_UNC_IVAR ? "ivar" : "none");
  if (props_src != live_src) {
    p3_uncertainty_close(&u_samp);
    p3_sampler_close(&samp);
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "uncertainty_source drift between p3_props.json and live scan"));
  }

  // max_tiles 内存守卫 (ARCH-P3 §3; 请求可降不可升, 同 p3_session)
  {
    const int64_t wh = (int64_t)g.w * g.h;
    const int64_t per_tile = 512 * 512;
    const int64_t need = (wh + per_tile - 1) / per_tile + 16;
    const int64_t default_max = std::min<int64_t>(1024, std::max<int64_t>(8, need));
    int64_t mt = doc.value("max_tiles", (int)default_max);
    if (mt > default_max) {
      p3_uncertainty_close(&u_samp);
      p3_sampler_close(&samp);
      return Result<void>::fail(Error(ErrorDomain::RESOURCE,
          "max_tiles above default memory guard (可降不可升)"));
    }
    p3_sampler_set_max_tiles(&samp, (int)std::max<int64_t>(1, mt));
  }

  int order_sel = -1;
  if (p3_order_select(input_order, g.scale, &order_sel) != P3_RS_OK) {
    p3_uncertainty_close(&u_samp);
    p3_sampler_close(&samp);
    return Result<void>::fail(Error(ErrorDomain::DATA, "p3_order_select failed"));
  }

  const long nelem = (long)g.w * g.h;
  std::vector<float> sig((size_t)nelem, std::nanf(""));
  std::vector<float> cov((size_t)nelem, 0.0f);
  std::vector<float> var_plane, ivar_plane;
  if (unc_available) {
    var_plane.assign((size_t)nelem, std::nanf(""));
    ivar_plane.assign((size_t)nelem, std::nanf(""));
  }
  std::atomic<long long> missing_px{0};
  std::atomic<int> corrupt{-1};          // 行号 (u 产品损坏 §30.4-3)
  const int npts = (g.sampler == "nearest") ? 1 : 4;

  auto worker = [&](int y0, int y1) {
    P3Sampler w_samp{};
    std::string wserr;
    if (p3_sampler_open_ex(g.hips_dir.c_str(), &w_samp, nullptr, nullptr, &wserr) !=
        P3_RS_OK)
      return;
    P3Sampler w_u{};
    P3UncertaintySource w_src = P3_UNC_NONE;
    if (unc_available &&
        p3_uncertainty_open(g.hips_dir.c_str(), input_order, &w_src, &w_u) != P3_RS_OK) {
      p3_sampler_close(&w_samp);
      corrupt.store(-2);
      return;
    }
    P3WcsDescriptor w_wcs = wcs;
    for (int y = y0; y < y1 && corrupt.load() == -1; ++y) {
      for (int x = 0; x < g.w; ++x) {
        double px_ra = 0, px_dec = 0;
        if (p3_wcs_pix2world(&w_wcs, (double)x, (double)y, &px_ra, &px_dec) !=
            P3_WCS_OK)
          continue;   // 半球外像素保持 NaN/0
        const long i = (long)y * g.w + x;
        float v = 0;
        int c = 0;
        double w[4] = {0, 0, 0, 0};
        uint64_t lf[4] = {0, 0, 0, 0};
        const P3ResampleStatus rst =
            (g.sampler == "nearest")
                ? p3_sample_nearest_ex(&w_samp, px_ra, px_dec, &v, &c, &lf[0])
                : p3_sample_bilinear_ex(&w_samp, px_ra, px_dec, &v, &c, w, lf);
        if (rst != P3_RS_OK) continue;
        sig[(size_t)i] = (c == 1) ? v : std::nanf("");
        cov[(size_t)i] = (c == 1) ? 1.0f : 0.0f;
        if (!unc_available) continue;
        if (c == 0) {   // 无覆盖 → var/ivar=NaN + C=0 (signal NaN 同态)
          var_plane[(size_t)i] = std::nanf("");
          ivar_plane[(size_t)i] = std::nanf("");
          continue;
        }
        double u_out = 0;
        P3UncPixelState u_st = P3_U_OK;
        const P3ResampleStatus urst =
            p3_uncertainty_propagate(&w_u, w, lf, npts, &u_out, &u_st);
        if (urst == P3_RS_PARAM) { corrupt.store(y); break; }   // 产品损坏
        if (urst != P3_RS_OK) continue;
        if (u_st == P3_U_MISSING) {
          missing_px.fetch_add(1);
          var_plane[(size_t)i] = std::nanf("");
          ivar_plane[(size_t)i] = std::nanf("");
          continue;
        }
        var_plane[(size_t)i] =
            std::isnan(u_out) ? std::nanf("") : static_cast<float>(u_out);
        ivar_plane[(size_t)i] =
            std::isnan(u_out)
                ? std::nanf("")
                : (u_out > 0.0 ? static_cast<float>(1.0 / u_out)
                               : (u_out == 0.0 ? 0.0f : std::nanf("")));
      }
    }
    p3_uncertainty_close(&w_u);
    p3_sampler_close(&w_samp);
  };

  // 行带执行 (RT-001): 行带 = work unit, 提交到 Runtime 唯一 executor
  // (rt::shared_work_executor, 每任务经 ThreadBudget acquire(1,1) 恰租 1 槽,
  // Σactive ≤ budget 与全部模块租约同一预算源; 禁 hardware_concurrency)。
  // 池不可得（无预算上下文）或 cap<2 → 调用线程串行（极小任务允许串行, §10.4;
  // 与旧行为 bitwise 一致）。节点整预算租约由 execute() 在提交前显式归还,
  // 否则池任务抢不到槽会自等待（见 P3NodeModule::execute 注记）。
  // fail-closed: 每个行带完成计数; wait_all 后已执行数 ≠ 提交数（取消丢弃/
  // 注入缺陷）→ 节点显式失败, 不落任何伪产物。实测观测: work_units（提交数）、
  // band_active_peak（行带并发峰值 = 实测 active threads）、band_executed。
  const uint32_t nw =
      (cap >= 2 && g.h >= 2)
          ? static_cast<uint32_t>(std::min<uint32_t>(cap, static_cast<uint32_t>(g.h)))
          : 1u;
  std::atomic<uint32_t> band_active{0};
  std::atomic<uint32_t> band_active_peak{0};
  std::atomic<uint32_t> band_executed{0};
  std::shared_ptr<CpuHeavyExecutor> band_pool;
  if (ctx && nw >= 2) band_pool = rt::shared_work_executor(ctx->budget());
  auto band_task = [&](int y0, int y1, RunContext&) {
    // 故障注入 (ASTROCS_RT001_FAULT): resample_drop_band 模拟"行带任务被取消
    // 丢弃"缺陷 → fail-closed 路径必须拒绝 (P2-002/P3-002 注入先例同构)。
    const char* fault = std::getenv("ASTROCS_RT001_FAULT");
    if (fault && std::strcmp(fault, "resample_drop_band") == 0 && y0 == 0) {
      return;  // 首行带(y0==0)被吞: 不执行、不计数 (确定性: 无时序竞争)
    }
    const uint32_t cur = band_active.fetch_add(1) + 1;
    uint32_t p = band_active_peak.load();
    while (cur > p && !band_active_peak.compare_exchange_weak(p, cur)) {}
    worker(y0, y1);
    band_active.fetch_sub(1);
    band_executed.fetch_add(1);
  };
  RunContext no_ctx;  // 池外调用面的空观测上下文（串行/防御路径专用）
  if (nw >= 2) {
    const int rows = g.h / (int)nw;
    for (uint32_t k = 0; k < nw; ++k) {
      const int y0 = (int)k * rows;
      const int y1 = (k == nw - 1) ? g.h : y0 + rows;
      if (band_pool) {
        band_pool->enqueue([&band_task, y0, y1](RunContext& c) {
          band_task(y0, y1, c);
        });
      } else {
        // 防御路径（池不可得且 cap>=2, 生产不可达）: 串行执行全部行带,
        // 不自建线程池（禁止回退到调用点 spawn）—— 完整性与确定性优先。
        band_task(y0, y1, no_ctx);
      }
    }
    if (band_pool) band_pool->wait_all();
  } else {
    band_task(0, g.h, no_ctx);
  }
  p3_uncertainty_close(&u_samp);
  p3_sampler_close(&samp);
  if (band_executed.load() != nw) {
    return Result<void>::fail(Error(ErrorDomain::CANCELLED,
        "resample row-band work units dropped (executed " +
            std::to_string(band_executed.load()) + "/" + std::to_string(nw) +
            "): cancelled or fault-injected; fail-closed, no partial product"));
  }
  if (man) {
    (*man)["work_units"] = nw;                      // 提交的 work unit 数（计划面）
    (*man)["band_executed"] = band_executed.load(); // 实测完成数
    (*man)["band_active_peak"] = band_active_peak.load();  // 实测并发峰值
  }
  if (corrupt.load() != -1) {
    if (corrupt.load() == -2)
      return Result<void>::fail(Error(ErrorDomain::IO,
          "uncertainty sampler open failed in worker"));
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "uncertainty product corrupt (negative/inf variance pixel) at row " +
            std::to_string(corrupt.load())));
  }

  // typed artifact: p3_resampled.bin = 平面连续拼接 (f32: sig, cov[, var, ivar])
  const std::string bin_path = g.out_dir + "/p3_resampled.bin";
  {
    std::ofstream bf(bin_path, std::ios::binary);
    if (!bf) return Result<void>::fail(Error(ErrorDomain::IO, "cannot write p3_resampled.bin"));
    bf.write(reinterpret_cast<const char*>(sig.data()),
             (std::streamsize)sizeof(float) * nelem);
    bf.write(reinterpret_cast<const char*>(cov.data()),
             (std::streamsize)sizeof(float) * nelem);
    if (unc_available) {
      bf.write(reinterpret_cast<const char*>(var_plane.data()),
               (std::streamsize)sizeof(float) * nelem);
      bf.write(reinterpret_cast<const char*>(ivar_plane.data()),
               (std::streamsize)sizeof(float) * nelem);
    }
    bf.close();
    if (!bf.good())
      return Result<void>::fail(Error(ErrorDomain::IO, "p3_resampled.bin write failed"));
  }
  // 完整性锚: bin 流式 sha256 (禁前缀/假哈希; 大图流式不整载)
  astrocs::crypto::Sha256 bh;
  {
    std::ifstream bf(bin_path, std::ios::binary);
    char hbuf[64 * 1024];
    while (bf.good()) {
      bf.read(hbuf, sizeof(hbuf));
      bh.update(hbuf, static_cast<size_t>(bf.gcount()));
    }
  }
  const std::string bin_sha = bh.final_hex();
  Json planes = Json::array();
  planes.push_back("signal");
  planes.push_back("coverage");
  if (unc_available) { planes.push_back("variance"); planes.push_back("ivar"); }
  const std::string json_path = g.out_dir + "/p3_resampled.json";
  Json res{{"schema", "DATA-P3-RES"},
           {"width_px", g.w},
           {"height_px", g.h},
           {"order_sel", order_sel},
           {"sampler", g.sampler},
           {"bitpix", g.bitpix},
           {"bunit", bunit},
           {"planes", planes},
           {"uncertainty_available", unc_available},
           {"uncertainty_source", live_src},
           {"uncertainty_missing_pixels", missing_px.load()},
           {"bin", "p3_resampled.bin"},
           {"bin_sha256", bin_sha}};
  std::ofstream f(json_path, std::ios::binary);
  if (!f) return Result<void>::fail(Error(ErrorDomain::IO, "cannot write p3_resampled.json"));
  f << res.dump(2) << "\n";
  f.close();
  if (!f.good())
    return Result<void>::fail(Error(ErrorDomain::IO, "p3_resampled.json write failed"));
  (*man)["resampled_artifact"] = json_path;
  (*man)["order_sel"] = order_sel;
  (*man)["uncertainty_available"] = unc_available;
  (*man)["uncertainty_source"] = live_src;
  (*man)["uncertainty_missing_pixels"] = missing_px.load();
  return Result<void>::success();
}

// ── op: writer (ALG-P3-004 唯一真实入口 = 原子流式 FITS 发布) ────────────────
Result<void> p3_op_writer(const Json& doc, Json* man) {
  P3nGeom g;
  std::string err;
  if (!p3n_geom(doc, &g, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  Json res, plan;
  if (!p3n_read_json(g.out_dir + "/p3_resampled.json", &res, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  if (!p3n_read_json(g.out_dir + "/p3_wcs.json", &plan, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  using namespace astrocs::phase3;
  P3WcsDescriptor wcs{};
  if (!p3n_wcs_from_json(plan, &wcs, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  const long nelem = (long)g.w * g.h;
  std::vector<float> sig((size_t)nelem), cov((size_t)nelem);
  const bool unc = res.value("uncertainty_available", false);
  std::vector<float> var_p, ivar_p;
  if (unc) { var_p.resize((size_t)nelem); ivar_p.resize((size_t)nelem); }
  {
    std::ifstream bf(g.out_dir + "/p3_resampled.bin", std::ios::binary);
    if (!bf) return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream artifact missing: p3_resampled.bin"));
    bf.read(reinterpret_cast<char*>(sig.data()), (std::streamsize)sizeof(float) * nelem);
    bf.read(reinterpret_cast<char*>(cov.data()), (std::streamsize)sizeof(float) * nelem);
    if (unc) {
      bf.read(reinterpret_cast<char*>(var_p.data()), (std::streamsize)sizeof(float) * nelem);
      bf.read(reinterpret_cast<char*>(ivar_p.data()), (std::streamsize)sizeof(float) * nelem);
    }
    if (!bf.good())
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "p3_resampled.bin truncated (planes vs manifest drift)"));
  }
  P3Provenance prov{};
  prov.hips_id = "ivo://astrocs/phase3";
  prov.manifest_hash = nullptr;
  prov.missing_tiles = nullptr;
  prov.missing_count = 0;
  const std::string version_str = "astrocs-phase3-node";
  const std::string run_id_str = "p3-node";
  const std::string order_sel_str = std::to_string(res.value("order_sel", -1));
  const std::string sampler_str = res.value("sampler", std::string("bilinear"));
  prov.software_version = version_str.c_str();
  prov.run_id = run_id_str.c_str();
  prov.order_sel_used = order_sel_str.c_str();
  prov.sampler_used = sampler_str.c_str();
  const std::string unc_src = res.value("uncertainty_source", std::string("none"));
  prov.uncertainty_source = (unc_src == "variance" || unc_src == "ivar")
                                ? unc_src.c_str() : nullptr;
  prov.uncertainty_missing_pixels =
      (long)res.value("uncertainty_missing_pixels", 0ll);
  const std::string fits_path = g.out_dir + "/output_phase3.fits";
  P3OutputResult ores{};
  const P3OutputStatus ost = p3_output_write_atomic_ex(
      sig.data(), cov.data(), unc ? var_p.data() : nullptr,
      unc ? ivar_p.data() : nullptr, g.w, g.h, &wcs,
      res.value("bunit", "ADU").c_str(), fits_path.c_str(), &prov, g.bitpix, -1,
      &ores);
  if (ost != P3_OUT_OK)
    return Result<void>::fail(Error(ErrorDomain::IO,
        "p3_output_write_atomic_ex failed (status " +
            std::to_string((int)ost) + ")"));
  long covn = 0;
  for (long i = 0; i < nelem; ++i) if (cov[(size_t)i] > 0.5f) ++covn;
  const std::string json_path = g.out_dir + "/p3_writer.json";
  Json wr{{"schema", "DATA-P3-WRITER-MANIFEST"},
          {"output_fits", fits_path},
          {"sha256", std::string(ores.sha256)},
          {"reopen_ok", ores.reopen_ok},
          {"coverage_stats", {{"covered_px", covn}, {"total_px", nelem}}},
          {"uncertainty_available", unc},
          {"uncertainty_source", unc_src},
          {"uncertainty_missing_pixels",
           (long long)res.value("uncertainty_missing_pixels", 0ll)}};
  std::ofstream f(json_path, std::ios::binary);
  if (!f) return Result<void>::fail(Error(ErrorDomain::IO, "cannot write p3_writer.json"));
  f << wr.dump(2) << "\n";
  f.close();
  (*man)["output_fits"] = fits_path;
  (*man)["writer_artifact"] = json_path;
  (*man)["sha256"] = std::string(ores.sha256);
  (*man)["uncertainty_available"] = unc;
  return Result<void>::success();
}

// ── op: verify (ALG-P3-005 唯一真实入口 = 独立重开验证) ──────────────────────
Result<void> p3_op_verify(const Json& doc, Json* man) {
  P3nGeom g;
  std::string err;
  if (!p3n_geom(doc, &g, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  Json res, wr, plan;
  if (!p3n_read_json(g.out_dir + "/p3_resampled.json", &res, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  if (!p3n_read_json(g.out_dir + "/p3_writer.json", &wr, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  if (!p3n_read_json(g.out_dir + "/p3_wcs.json", &plan, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  using namespace astrocs::phase3;
  P3WcsDescriptor wcs{};
  if (!p3n_wcs_from_json(plan, &wcs, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  const long nelem = (long)g.w * g.h;
  std::vector<float> sig((size_t)nelem), cov((size_t)nelem);
  const bool unc = res.value("uncertainty_available", false);
  std::vector<float> var_p, ivar_p;
  if (unc) { var_p.resize((size_t)nelem); ivar_p.resize((size_t)nelem); }
  {
    std::ifstream bf(g.out_dir + "/p3_resampled.bin", std::ios::binary);
    if (!bf) return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream artifact missing: p3_resampled.bin"));
    bf.read(reinterpret_cast<char*>(sig.data()), (std::streamsize)sizeof(float) * nelem);
    bf.read(reinterpret_cast<char*>(cov.data()), (std::streamsize)sizeof(float) * nelem);
    if (unc) {
      bf.read(reinterpret_cast<char*>(var_p.data()), (std::streamsize)sizeof(float) * nelem);
      bf.read(reinterpret_cast<char*>(ivar_p.data()), (std::streamsize)sizeof(float) * nelem);
    }
    if (!bf.good())
      return Result<void>::fail(Error(ErrorDomain::DATA, "p3_resampled.bin truncated"));
  }
  P3OutputResult vres{};
  const P3OutputStatus vst = p3_output_verify_ex(
      wr.value("output_fits", std::string()).c_str(), &wcs, sig.data(), cov.data(),
      unc ? var_p.data() : nullptr, unc ? ivar_p.data() : nullptr, g.w, g.h,
      &vres);
  if (vst != P3_OUT_OK)
    return Result<void>::fail(Error(ErrorDomain::IO, "p3_output_verify_ex failed"));
  const std::string json_path = g.out_dir + "/p3_verify.json";
  Json ver{{"schema", "DATA-P3-VER"},
           {"output_fits", wr.value("output_fits", std::string())},
           {"reopen_ok", vres.reopen_ok},
           {"coverage_ok", vres.coverage_ok},
           {"sha256", std::string(vres.sha256)},
           {"coverage_stats",
            {{"covered_px", vres.covered_px}, {"total_px", vres.total_px}}},
           {"uncertainty_available", unc}};
  std::ofstream f(json_path, std::ios::binary);
  if (!f) return Result<void>::fail(Error(ErrorDomain::IO, "cannot write p3_verify.json"));
  f << ver.dump(2) << "\n";
  f.close();
  (*man)["verified_artifact"] = json_path;
  (*man)["reopen_ok"] = vres.reopen_ok;
  return Result<void>::success();
}

}  // namespace (p3 node ops)

// ── P3NodeModule: Phase3 唯一真实 operation 节点适配器（IModule）────────────
enum class P3NodeOp { Properties, Wcs, Resample, Writer, Verify };
struct P3NodeSpec {
  P3NodeOp op;
  const char* operation;  // module_ports.registry.json 冻结 operation 名
  const char* entry;      // 冻结唯一真实入口名（节点 manifest 可审计标记）
};

struct P3NodeModule : public IModule {
  ModuleDescriptor desc_;
  P3NodeSpec spec_;
  std::string config_;
  std::string manifest_;
  uint32_t workers_ = 2;

  P3NodeModule(ModuleDescriptor d, P3NodeSpec s)
      : desc_(std::move(d)), spec_(s) {}

  const ModuleDescriptor& descriptor() const noexcept override { return desc_; }

  // config 合同: source.hips_dir + center.ra_deg/dec_deg + scale_deg_per_px +
  // width_px/height_px + output_dir 必填 (类型面); 科学值域在 op 内
  // fail-closed 校验 (不提前消费科学缺省值; 冻结缺省 sampler=bilinear/
  // parity=east_left/bitpix=-32 为 SCI §9a 合同值)。
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
    if (!doc.contains("source") || !doc["source"].is_object() ||
        !doc["source"].contains("hips_dir") || !doc["source"]["hips_dir"].is_string())
      return Result<void>::fail(Error(ErrorDomain::DATA, "missing source.hips_dir"));
    if (!doc.contains("center") || !doc["center"].is_object() ||
        !doc["center"].contains("ra_deg") || !doc["center"].contains("dec_deg"))
      return Result<void>::fail(Error(ErrorDomain::DATA, "missing center.ra_deg/dec_deg"));
    if (!doc.contains("output_dir") || !doc["output_dir"].is_string())
      return Result<void>::fail(Error(ErrorDomain::DATA, "missing output_dir"));
    if (doc.contains("sampler") && !doc["sampler"].is_string())
      return Result<void>::fail(Error(ErrorDomain::DATA, "sampler must be string"));
    if (doc.contains("longitude_parity") && !doc["longitude_parity"].is_string())
      return Result<void>::fail(Error(ErrorDomain::DATA, "longitude_parity must be string"));
    if (doc.contains("bitpix") && !doc["bitpix"].is_number_integer())
      return Result<void>::fail(Error(ErrorDomain::DATA, "bitpix must be integer"));
    if (doc.contains("max_tiles") && !doc["max_tiles"].is_number_integer())
      return Result<void>::fail(Error(ErrorDomain::DATA, "max_tiles must be integer"));
    return Result<void>::success();
  }

  Result<ModulePlan> plan(const std::string& node_id,
                          const std::string& config_json) override {
    config_ = config_json;
    ModulePlan p;
    p.node_id = node_id;
    p.work_units = 1;
    p.parallel_axes = {"row-band"};
    p.cpu_heavy = desc_.execution_class == "cpu_heavy";
    return Result<ModulePlan>::ok(std::move(p));
  }

  Result<void> execute(RunContext& ctx) override {
    const uint32_t host_workers =
        ctx.budget() ? ctx.budget()->budget() : workers_;
    // RT-001: 租约只用于 cap 授权观测（trace workers/granted）——本节点重计算
    // 面行带已改为 Runtime 唯一 executor 的 work unit（每任务 acquire(1,1) 恰
    // 租 1 槽）。节点若持有整预算租约再提交行带, 池任务将抢不到槽而自等待
    // 死锁; 故租约在此显式归还后再进入 op 执行（ThreadLease::release 幂等,
    // 析构兜底, 异常路径安全）。其余 P1/P2 session 节点保持整租约执行模型
    // 不变（其内部池为租约驱动 per-call worker, ARCH-THREAD-001 §1 登记形态;
    // work-unit 化待后续任务, 见 F-RT-001-05）。
    ThreadLease lease = ctx.acquire_lease(host_workers);
    const uint32_t cap = lease.acquired() ? lease.size() : 1u;
    lease.release();
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
    Json man = Json{{"kind", "astrocs.phase3.node"},
                    {"module_id", desc_.module_id},
                    {"operation", spec_.operation},
                    {"entry", spec_.entry},
                    {"artifact_type", desc_.data_id},
                    {"availability", "available"},
                    {"status", "running"}};
    Result<void> r = Result<void>::success();
    try {
      Json doc = Json::parse(config_);
      // typed artifact 落盘面: output_dir 由节点幂等创建 (P2 同款)
      {
        const std::string out_dir = doc.value("output_dir", std::string("."));
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::u8path(out_dir), ec);
        if (ec && !std::filesystem::exists(std::filesystem::u8path(out_dir), ec)) {
          man["error"] = "cannot create output_dir: " + out_dir;
          r = Result<void>::fail(Error(ErrorDomain::IO, man["error"].get<std::string>()));
        }
      }
      if (r.ok()) {
        switch (spec_.op) {
          case P3NodeOp::Properties: r = p3_op_properties(doc, &man); break;
          case P3NodeOp::Wcs:        r = p3_op_wcs(doc, &man); break;
          case P3NodeOp::Resample:   r = p3_op_resample(doc, &man, cap, &ctx); break;
          case P3NodeOp::Writer:     r = p3_op_writer(doc, &man); break;
          case P3NodeOp::Verify:     r = p3_op_verify(doc, &man); break;
        }
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

std::unique_ptr<IModule> make_p3_node_module(ModuleDescriptor desc, P3NodeSpec spec) {
  return std::make_unique<P3NodeModule>(std::move(desc), spec);
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
  // coverage→sample→upm_fit→upm_apply→reject→integrate→write。
  // P2-001 (ARCH-P0-001 Phase2 侧整改): 7 子节点全部唯一真实 operation 委托
  // （原工厂委托 P2Api session adapter = 每个子节点调用完整 p2_session_run,
  //  7 节点链重复执行全链 7 次, 违反 RT-001 每 node 唯一真实 operation 绑定）;
  // 各节点 operation/entry 与 module_ports.registry.json 冻结绑定表一致,
  // manifest 携带标记供 trace/审计。
  const std::pair<ModuleDescriptor, P2NodeSpec> p2_nodes[] = {
      {p2_coverage_descriptor(),   {P2NodeOp::Coverage,  "compute_coverage", "astrocs_phase2_coverage_v1"}},
      {p2_sample_descriptor(),     {P2NodeOp::Sample,    "sample_frames",    "astrocs_phase2_sample_v1"}},
      {p2_upm_fit_descriptor(),    {P2NodeOp::UpmFit,    "fit_upm",          "astrocs_phase2_upmfit_v1"}},
      {p2_upm_apply_descriptor(),  {P2NodeOp::UpmApply,  "apply_upm",        "astrocs_phase2_upmapply_v1"}},
      {p2_reject_descriptor(),     {P2NodeOp::Reject,    "reject_outliers",  "astrocs_phase2_reject_v1"}},
      {p2_integrate_descriptor(),  {P2NodeOp::Integrate, "integrate_frames", "astrocs_phase2_integrate_v1"}},
      {p2_write_descriptor(),      {P2NodeOp::Write,     "write_mosaic",     "astrocs_phase2_write_v1"}},
  };
  for (const auto& [d, spec] : p2_nodes) {
    auto rr = registry.register_module(d);
    if (rr.failed()) return rr;
    auto ff = registry.register_factory(
        d.module_id, [d, spec]() { return make_p2_node_module(d, spec); });
    if (ff.failed()) return ff;
  }
  // P3-002 (宪章 §7.2/§8.2): Canonical Phase3 IR 5 节点链子模块注册
  // (source→properties→wcs→resample→writer→verify)。五子节点全部唯一真实
  // operation 委托（P3-002 整改: 原工厂委托 P3Api session adapter = 每个子
  // 节点调用完整 p3_session_run, 5 节点链重复执行全链 5 次, 违反 RT-001 每
  // node 唯一真实 operation 绑定）; 各节点 operation/entry 与
  // runtime/pipeline/module_ports.registry.json 冻结绑定表一致, manifest
  // 携带标记供 trace/审计; 节点间 typed artifact 经 output_dir 文件约定
  // 传递 (p3_props.json → p3_wcs.json → p3_resampled.{json,bin} →
  // output_phase3.fits → p3_verify.json)。
  // 顶层 astrocs.phase3.resample 占位 descriptor（P2 模板复制残留）不在本
  // 任务触碰, 由 P3-RSMP-INT 处理 (lib/phase3_rsmp/README.md 实测登记)。
  const std::pair<ModuleDescriptor, P3NodeSpec> p3_nodes[] = {
      {p3_properties_descriptor(), {P3NodeOp::Properties, "read_properties",      "astrocs_phase3_properties_v1"}},
      {p3_wcs_descriptor(),        {P3NodeOp::Wcs,        "build_wcs",            "astrocs_phase3_wcs_v1"}},
      {p3_resample2_descriptor(),  {P3NodeOp::Resample,   "resample_projection",  "astrocs_phase3_resample_v1"}},
      {p3_writer_descriptor(),     {P3NodeOp::Writer,     "write_fits",           "astrocs_phase3_writer_v1"}},
      {p3_verify_descriptor(),     {P3NodeOp::Verify,     "verify_output",        "astrocs_phase3_verify_v1"}},
  };
  for (const auto& [d, spec] : p3_nodes) {
    auto rr = registry.register_module(d);
    if (rr.failed()) return rr;
    auto ff = registry.register_factory(
        d.module_id, [d, spec]() { return make_p3_node_module(d, spec); });
    if (ff.failed()) return ff;
  }

  return Result<void>::success();
}

}  // namespace astrocs::core
