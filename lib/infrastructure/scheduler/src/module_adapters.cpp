// RT-005 可执行模块适配器：Phase1/2/3 session → IModule 工厂
// P1-001 (attempt 2): Phase1 8 类节点唯一真实 operation 委托（ARCH-P0-001 整改）:
//   calibration    → ac_calibrate_frame     (lib/algorithms/calibration C ABI)
//   cosmetic       → ac_correct_frame       (lib/algorithms/calibration C ABI)
//   star-psf       → astrocs::phase1::StarDetector::detect  (lib/algorithms/star_detection/wrapper_phase1
//                                        的 P1-003 桥接类, **不是** lib/algorithms/star_detection
//                                        sdet) → star_det v1 [N,6] →
//                                        dpsf_fit_batch_f64 (lib/algorithms/psf Moffat4
//                                        FP64); P2/PSF-FAST-001 起只对最亮
//                                        psf.max_stars 颗拟合（默认 5000）
//   wcs-platesolve → WcsTan::pix2sky         (lib/algorithms/platesolve/wrapper_phase1; 配置/初始 WCS
//                    像素→天球投影, 标定语义; 真实求解器接线归各 IMPL 任务)
//   photometry     → Photometer::measure     (lib/algorithms/photometry/wrapper_phase1)
//   noise-snr      → NoiseModel::estimate    (lib/algorithms/noise_snr/wrapper_phase1)
//   drizzle        → hp_drizzle_run          (lib/algorithms/drizzle 静态库)
//   writer         → aio_write_fits          (lib/infrastructure/aio)
// P2-001: Phase2 7 类节点唯一真实 operation 委托（ARCH-P0-001 Phase2 侧整改;
//   原工厂委托 P2Api session adapter = 子节点调用完整 p2_session_run 违规）:
//   coverage   → p2_coverage_build         (lib/algorithms/coverage C ABI, 两阶段容量协议)
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

// B2-A10: 构建期版本单源（与 CLI 共用同一生成头）——节点 manifest 自报
// module build ID 需要 ASTROCS_VERSION_STRING。
#include "version_generated.h"

#include "astrocs/common_abi_v1.h"
#include "astrocs/core/context.h"

#include "astro_calibration.h"
#include "astro_image_io.h"
#include "aio_fits.h"        // AIOImageData 完整布局: 写出前归一化样本格式为 FP32
#include "hp_drizzle_api.h"

// P1-001 口径更新: 真实求解器/拟合器/HiPS writer 生产头（模块库零 diff 只读调用）
#include "dynamic_psf.h"     // lib/algorithms/psf: dpsf_fit_batch_f64 (Moffat4 FP64)
#include "ipv_api.h"         // lib/algorithms/platesolve/cpp/ipv: 真实 plate-solve 求解链
#include "gaia_client.h"     // lib/infrastructure/gaia_xpsd_client: Gaia XPSD cone-search 客户端
#include "star_detector.h"   // lib/algorithms/star_detection: sdet_create/sdet_detect_ex_f64 句柄
// 注: C++ StarDetector 类头与 sdet C 头同名——裸名 include 命中 lib/algorithms/star_detection
// (lib/algorithms/star_detection/include 先于 lib/algorithms/star_detection/wrapper_phase1), C++ 类头以相对路径显式引入。
#include "../../../algorithms/star_detection/wrapper_phase1/star_detector.h"  // astrocs::phase1::StarDetector (C++)
#include "aio_hips.h"        // lib/infrastructure/aio: IVOA HiPS 标准写链
#include "aio_hips_reader.h" // lib/infrastructure/aio: HiPS 读面(P2 帧数据消费)

// P2-001: Phase2 真实节点生产头（lib/algorithms/coverage 冻结 C ABI + HEALPix 单一实现 +
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
// P8-SNR-LINUX: 逐源 SNR 帧级聚合 (lib/algorithms/noise_snr/wrapper_phase1), 其公式实现为
// lib/algorithms/noise_snr/cpp/src/snr_science.cpp (已编入 astrocs_phase1_noise)。
#include "snr_frame_science.h"
#include "wcs_tan.h"

// P7-UTIL-001: 节点级 OpenMP 并行度注入的保存/恢复需要 ICV 访问器。
#ifdef _OPENMP
#include <omp.h>
#endif

// P3-002: Phase3 唯一真实 operation 节点生产头（lib/phase3_session 冻结 C++
// 内核, 静态库 astrocs_phase3_session 已在 astrocs_module_adapters 链接闭包;
// 相对路径 include 同 "../../../algorithms/star_detection/wrapper_phase1/star_detector.h" 先例, 根 CMake
// 零改动）
#include "../../../phase3_session/p3_resample.h"
#include "../../../phase3_session/p3_output.h"
#include "../../../phase3_session/p3_wcs.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>    // P10-UTIL2-006: 节点执行窗口观测 (ASTROCS_NODE_TRACE)
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>   // P7-UTIL-001: std::getenv (ASTROCS_LEASE_TRACE 观测开关)
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <mutex>
#include <vector>

// CORE-RACE-001: 临时文件命名需要进程号（见 p1_staging_path）
#ifdef _WIN32
#include <process.h>
#define P1_NODE_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define P1_NODE_GETPID static_cast<long>(::getpid())
#endif

// ── RT-001: 唯一 Executor 生产接入（编译归属注记, 详见 executor_runtime.h）──
// RT-004 冻结合同实现 lib/infrastructure/scheduler/src/executor.cpp 此前未编入任何生产 target
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

// ── P7-UTIL-001: 节点级 OMP 并行度注入 (作用域 RAII, 不污染进程/线程 ICV) ──
//
// 缺陷 (实测): 原实现直接用 ac_set_num_threads(hs.host.budget.max_workers) 把
// 节点租约大小写进**当前线程的 OpenMP nthreads-var ICV 且永不恢复**。当 Scheduler
// 并发派发同一 DAG 层内的多个 cpu_heavy 节点时, 唯一 ThreadBudget 是"先到先得整份"
// (RunContext::acquire_lease → ThreadBudget::acquire(1, want, NONBLOCK) 取 min(want,
// available)), 落败节点的 cap=1 于是被写进该调度线程的 ICV; 该线程上**之后所有节点**
// 的 OpenMP 代码 (ipv triangle_match / star_detector 检测等) 一律退化为 1 线程, 且
// 该线程被池复用时污染持续存在 —— 违反宪章 §10.5「任何连续 10 s 低于 60% 或只有
// 一个活跃计算线程均失败」。
//
// 证据: LD_PRELOAD 拦截 libgomp omp_set_num_threads + addr2line, 命中两次, 栈为
//   std::thread(Scheduler::run pool) → Scheduler::run lambda → std::function invoke
//   → RuntimeImpl::load_pipeline lambda → P1NodeModule::execute → omp_set_num_threads(1)
// 随后 ipv_triangle.cpp 的 omp_get_max_threads() 返回 1 (日志 "[并行 1 线程]")。
//
// 修复语义: **仅**把租约值注入限制在节点 execute 的作用域内, 退出 (正常/异常/取消
// 路径) 恢复进入前的 ICV。不改变 lease 的申请/释放、lease.acquired() 判定与
// "拿不到整份预算就降级" 的行为 (降级只影响该节点自身的并行度, 不再外溢)。
// 线程数仍唯一来自 host budget (宪章 §10.4), 无任何硬编码。
class ScopedOmpWorkerInjection {
 public:
  explicit ScopedOmpWorkerInjection(int workers) {
#ifdef _OPENMP
    prev_ = omp_get_max_threads();
    if (workers > 0) omp_set_num_threads(workers);
#else
    (void)workers;
#endif
  }
  ~ScopedOmpWorkerInjection() {
#ifdef _OPENMP
    if (prev_ > 0) omp_set_num_threads(prev_);
#endif
  }
  ScopedOmpWorkerInjection(const ScopedOmpWorkerInjection&) = delete;
  ScopedOmpWorkerInjection& operator=(const ScopedOmpWorkerInjection&) = delete;

 private:
#ifdef _OPENMP
  int prev_ = -1;
#endif
};

// P7-UTIL-001 观测: ASTROCS_LEASE_TRACE=1 时逐节点输出租约/预算快照
// (默认零输出零开销; 供 §10.5 资源门禁与后续排期定位"节点级降级").
void trace_node_lease(const char* module_id, uint32_t host_workers,
                      bool acquired, uint32_t cap, uint32_t available) {
  static const bool on = [] {
    const char* v = std::getenv("ASTROCS_LEASE_TRACE");
    return v && v[0] == '1';
  }();
  if (!on) return;
  std::fprintf(stderr,
               "[lease] %s host_workers=%u acquired=%d cap=%u budget_available=%u\n",
               module_id ? module_id : "?", host_workers, acquired ? 1 : 0, cap,
               available);
}

// P10-UTIL2-006 观测: ASTROCS_NODE_TRACE=1 时逐节点输出**执行窗口**的单调时钟
// 边界 (BEGIN/END) —— 仅观测, 不改变调度/并行度/科学; 默认零输出零开销。
// 用途: 把进程级 /proc CPU 采样精确归因到节点。注意 [lease] 行打印在**取租约**
// 时 (heavy 节点可能在预算闸门处等待), 不能当作执行起点; 本窗口才是真实执行区间。
double p10_monotonic_s() {
  return std::chrono::duration<double>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

struct P10NodeTraceGuard {
  const char* mod;
  double t0;
  ~P10NodeTraceGuard() {
    if (std::getenv("ASTROCS_NODE_TRACE"))
      std::fprintf(stderr, "[nodetrace] END %s %.6f\n", mod ? mod : "?",
                   p10_monotonic_s() - t0);
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

// ── B2-A1 (AUD-COORD F-01/F-06): 独立前向 TAN 参考解 ──
// 用途: p1_op_wcs 的绝对交叉门锚点。旧门 sky2pix(pix2sky(x,y)) 是同一对
// 变换自洽求残差, 对"ξ/η 度当弧度"这类成对单位错零鉴别力 (AUD-COORD O6:
// 绝对偏差 14306″ 时 roundtrip 仍 1e-12 px) ⇒ 恒真。
// 本函数是与 WcsTan::pix2sky **不同源**的推导: 切点单位向量 r0 及其东向/
// 北向正交基 (e, n = r0×e) 上做 gnomonic 投影 r ∝ r0 + ξ·e + η·n, 再
// 归一化取 asin/atan2。ξ/η 在此显式由 deg 转 rad, 任何"deg 当弧度"回归
// 会以 180/π 量级偏差被检出。
// 权威: FITS-WCS Paper I §2.2 (pixel → intermediate world) + Paper II
// (Calabretta & Greisen 2002) TAN/gnomonic; 像素输入为 FITS 1-based (与
// WcsTan 头契约一致), CD 单位 deg/px。
constexpr double kP1D2R = 0.01745329251994329577;   // π/180
constexpr double kP1R2D = 57.29577951308232087680;  // 180/π

void p1_tan_forward_reference(double crpix1, double crpix2, double crval1,
                              double crval2, double cd11, double cd12,
                              double cd21, double cd22, double x, double y,
                              double* ra, double* dec) {
  const double u = x - crpix1;
  const double v = y - crpix2;
  const double xi = (cd11 * u + cd12 * v) * kP1D2R;   // rad
  const double eta = (cd21 * u + cd22 * v) * kP1D2R;  // rad
  const double a0 = crval1 * kP1D2R;
  const double d0 = crval2 * kP1D2R;
  const double ca = std::cos(a0), sa = std::sin(a0);
  const double c0 = std::cos(d0), s0 = std::sin(d0);
  // r0(切点), e(东向), n = r0 × e (北向)
  const double r0x = c0 * ca, r0y = c0 * sa, r0z = s0;
  const double ex = -sa, ey = ca;
  const double nx = -s0 * ca, ny = -s0 * sa, nz = c0;
  // gnomonic: 天球方向 ∝ r0 + ξ·e + η·n (ξ,η 为切平面偏移的 tan 量)
  double px = r0x + xi * ex + eta * nx;
  double py = r0y + xi * ey + eta * ny;
  double pz = r0z + eta * nz;
  const double norm = std::sqrt(px * px + py * py + pz * pz);
  if (std::isfinite(norm) && norm > 0.0) {
    px /= norm;
    py /= norm;
    pz /= norm;
  }
  double sdec = pz > 1.0 ? 1.0 : (pz < -1.0 ? -1.0 : pz);
  if (dec) *dec = std::asin(sdec) * kP1R2D;
  double ra_deg = std::atan2(py, px) * kP1R2D;
  if (ra_deg > 180.0) ra_deg -= 360.0;
  if (ra_deg < -180.0) ra_deg += 360.0;
  if (ra) *ra = ra_deg;
}

// 两天球坐标的角距 (deg, haversine; 经度环绕安全)。B2-A1 交叉门度量。
double p1_angular_sep_deg(double ra1, double dec1, double ra2, double dec2) {
  const double d1 = dec1 * kP1D2R, d2 = dec2 * kP1D2R;
  double dra = (ra2 - ra1) * kP1D2R;
  while (dra > 3.14159265358979323846) dra -= 2.0 * 3.14159265358979323846;
  while (dra < -3.14159265358979323846) dra += 2.0 * 3.14159265358979323846;
  const double sh = std::sin((d2 - d1) / 2.0);
  const double sn = std::sin(dra / 2.0);
  double s = sh * sh + std::cos(d1) * std::cos(d2) * sn * sn;
  if (s > 1.0) s = 1.0;
  if (s < 0.0) s = 0.0;
  return 2.0 * std::asin(std::sqrt(s)) / kP1D2R;
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
      // F-8 (RESCUE): drz 的真实 header KV 来源是 wcs 节点产物 p1_wcs.json;
      // 声明为 typed 输入端口使 IR 依赖边可绑定（artifact:p1_wcs）, 调度器据此
      // 保证 wcs 先落盘再 drizzle, 不再依赖并发文件约定。
      {"wcs", "DATA-P1-WCS", true, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
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
// AIOImageData 无部分读标志 ABI; 以真实磁盘布局为准——每像素字节数取自 aio
// 读入的真实样本布局（FITS BITPIX / XISF sample format; BSCALE/BZERO 只做
// 数值缩放、不改变文件体积）, 头域长度取 FITS 主头实测的 2880 字节块数
//（非 FITS 沿用单块 2880 的保守下界）。要求 头字节 + w*h*(|BITPIX|/8) 全部落盘。
// 溯源: 旧实现硬编码 4 B/px（隐含 BITPIX=-32），而真实亮场全为 BITPIX=16
//（2 B/px）→ 全部被误判"不可读"（RESCUE F-7, 源自 9e09941a P1-001，非本包
// 引入）。本判据不放松截断检测: 32/64 位浮点需求反而更大; 位深/头域不可得即
// fail-closed（与 U6/U3 "truncated input must not complete" 语义同源, 不留伪产物）。
bool p1_is_fits_file(const std::string& path) {
  std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
  if (!f) return false;
  char magic[6] = {0};
  f.read(magic, 6);
  return f.gcount() == 6 && std::strncmp(magic, "SIMPLE", 6) == 0;
}

uint64_t p1_fits_primary_header_bytes(const std::string& path) {
  std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
  if (!f) return 0;
  char blk[2880];
  // 上限 100 块（288 KB）避免无 END 的病态头无限读
  for (uint64_t blocks = 1; blocks <= 100; ++blocks) {
    if (!f.read(blk, sizeof(blk))) return 0;
    for (int i = 0; i < 36; ++i) {
      const char* c = blk + i * 80;
      if (std::strncmp(c, "END", 3) == 0 && (c[3] == ' ' || c[3] == '\0'))
        return blocks * 2880ull;
    }
  }
  return 0;  // 主头无 END → fail-closed
}

bool p1_image_sane(const P1Image& im, const std::string& path) {
  if (!im.ok() || im.w() <= 0 || im.h() <= 0 || im.px() == nullptr) return false;
  const AIOImageOptions opt = aio_get_options(im.p);
  if (opt.bits_per_sample <= 0) return false;  // 位深不可得 → fail-closed
  const uint64_t bpp = static_cast<uint64_t>(opt.bits_per_sample) / 8ull;
  if (bpp == 0) return false;
  const uint64_t a = static_cast<uint64_t>(im.w());
  const uint64_t b = static_cast<uint64_t>(im.h());
  if (a > UINT64_MAX / b) return false;
  const uint64_t pixels = a * b;
  if (pixels > UINT64_MAX / bpp) return false;
  const uint64_t data = pixels * bpp;
  // FITS: 主头实测块数; 非 FITS（XISF 等）: 单块保守下界, 与旧实现同口径
  const uint64_t header = p1_is_fits_file(path) ? p1_fits_primary_header_bytes(path)
                                                : 2880ull;
  if (header == 0 || data > UINT64_MAX - header) return false;
  const uint64_t need = header + data;
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

// ── CORE-RACE-001: 原子发布原语（禁就地覆写共享产物路径）────────────────────
// 缺陷（修复前）: 节点链 cos 与 drz 同为 cal 下游且节点声明 resources.parallel=true
// ⇒ create_runtime(2) 下并发; cosmetic 读取 artifact:cal 路径后 *就地覆写同一
// 路径*, 而 aio_write_fits 为 fopen("wb") 截断 + 增量写（非原子）⇒ 并发消费者
// (drz / 其他) 可观察到半写文件（CI-REG-002: 200 次复跑 pass=176 fail=24, 12%）。
// 修复两条同时成立:
//   1) 每节点写独立产物路径（cos → cleaned_<base>, 不再覆写 artifact:cal）;
//   2) 所有落盘走"同目录临时文件 + rename 原子发布", 消费者只可能看到完整文件
//      （POSIX rename 原子; 目标已存在时覆盖。崩溃残留的 .tmp 不污染产物面）。
// 注: 不改科学公式/默认容差, 只改落盘与路径语义。
std::string p1_staging_path(const std::string& final_path) {
  static std::atomic<uint64_t> seq{0};
  const uint64_t s = seq.fetch_add(1, std::memory_order_relaxed);
  return final_path + ".tmp." + std::to_string(P1_NODE_GETPID) + "." +
         std::to_string(s);
}

bool p1_atomic_publish(const std::string& staging, const std::string& final_path,
                       std::string* err) {
  std::error_code ec;
  std::filesystem::rename(std::filesystem::u8path(staging),
                          std::filesystem::u8path(final_path), ec);
  if (ec) {
    // 兜底（Windows 部分实现 rename 不覆盖已存在目标）: 先删目标再 rename。
    // 该窗口内目标短暂缺失, 但任一时刻观察到的都是"旧完整文件"或"新完整文件",
    // 不存在半写状态（原子发布的核心不变式）。
    std::error_code ec_rm;
    std::filesystem::remove(std::filesystem::u8path(final_path), ec_rm);
    std::error_code ec2;
    std::filesystem::rename(std::filesystem::u8path(staging),
                            std::filesystem::u8path(final_path), ec2);
    if (ec2) {
      if (err) *err = "atomic publish failed: " + ec2.message();
      std::error_code ec_drop;
      std::filesystem::remove(std::filesystem::u8path(staging), ec_drop);
      return false;
    }
  }
  return true;
}

// FITS 原子落盘（临时文件在目标同目录 → rename 不跨文件系统）
// RESCUE 真实链路修复: 节点计算缓冲恒为 FP32（aio 的 float* 域）。若复用源图
// 句柄（源为 BITPIX=16/float_sample=0），aio_write_fits 会按源元数据写 int16 并
// 对越界值回绕，产出损坏产品（实测 cal 产物与源相关性仅 0.065，真实 16 位数据
// 的 wcs 求解因此稳定失败）。写出前把样本格式归一为 FP32, 与缓冲真实类型一致。
bool p1_write_fits_atomic(const P1Image& im, const std::string& final_path,
                          std::string* err) {
  if (im.ok() && im.p != nullptr) {
    im.p->bits_per_sample = 32;
    im.p->float_sample = 1;
  }
  const std::string staging = p1_staging_path(final_path);
  if (aio_write_fits(im.p, staging.c_str()) != 0) {
    if (err) *err = "write failed: " + final_path;
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(staging), ec);
    return false;
  }
  if (!p1_atomic_publish(staging, final_path, err)) return false;
  return true;
}

// 节点输入帧路径: 优先 cal 节点产物 calibrated_<base>（节点链约定）, 无则原帧
std::string p1_calibrated_path(const Json& doc, const std::string& light) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  const std::string cand = out_dir + "/calibrated_" + p1_base_name(light);
  std::error_code ec;
  if (std::filesystem::exists(std::filesystem::u8path(cand), ec)) return cand;
  return light;
}

// cosmetic 节点产物路径（DATA-P1-COSMETIC / artifact:cos）: cleaned_<base>。
// 独立于 artifact:cal —— cos 不再就地覆写上游产物（CORE-RACE-001）。
std::string p1_cosmetic_path(const Json& doc, const std::string& light) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  return out_dir + "/cleaned_" + p1_base_name(light);
}

// cosmetic 下游节点的输入帧路径: 优先 cos 节点产物 cleaned_<base>, 无则退回
// cal 产物/原帧（节点单独运行时缺上游产物 = 确定性回退, 不 silent 造数据）。
std::string p1_cleaned_input_path(const Json& doc, const std::string& light) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  const std::string cand = out_dir + "/cleaned_" + p1_base_name(light);
  std::error_code ec;
  if (std::filesystem::exists(std::filesystem::u8path(cand), ec)) return cand;
  return p1_calibrated_path(doc, light);
}

bool p1_write_text(const std::string& path, const std::string& text) {
  // 原子发布（同目录临时文件 + rename）: 并发消费者不会读到半写 JSON
  const std::string staging = p1_staging_path(path);
  {
    std::ofstream f(std::filesystem::u8path(staging), std::ios::binary);
    if (!f) return false;
    f << text;
    if (!f.good()) {
      f.close();
      std::error_code ec;
      std::filesystem::remove(std::filesystem::u8path(staging), ec);
      return false;
    }
  }
  return p1_atomic_publish(staging, path, nullptr);
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

// ── FIX-E2E B1-A3: 空必填输入 fail-closed ─────────────────────────────────
// §14.4 最小充分校验: 只做数组非空/元素类型校验, 不扩成存在性防御堆叠。
// 返回 DATA（CLI rc=2）; 不设 error_kind —— "缺文件"仍走原 error_kind=input→3 语义。
Result<void> p1_require_lights(const Json& doc) {
  if (!p1_has(doc, "input_lights") || !doc["input_lights"].is_array() ||
      doc["input_lights"].empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "input_lights must be non-empty array"));
  for (const auto& l : doc["input_lights"])
    if (!l.is_string() || l.get<std::string>().empty())
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "input_lights items must be non-empty strings"));
  return Result<void>::success();
}

// ── B2-A6: master flat 数值有效性 fail-closed（消费边界）──────────────────
// SCI-CAL-001 §3/§4/§8 单位表与 flat 语义：calibrate 以 max(flat,0.1) 为除数。
// master flat 若全零/中位数<=0/非有限，除法退化为常数放大（max(0,0.1)=0.1 →
// 恒 ×10）或传播 NaN/Inf，产出看似正常的伪科学产品（AUD-CI-LIVE F3 的 ×10
// 根因之一）。宪章 §14.4 fail-fast / §11 不留半成品：消费边界确定性拒绝。
// 只做整帧退化判定，不裁切合法负像素/近零像素（floor 语义仍在 calibrate 内）。
bool p1_master_flat_valid(const P1Image& flat, std::string* why) {
  if (!flat.ok() || flat.px() == nullptr || flat.w() <= 0 || flat.h() <= 0) {
    if (why) *why = "unreadable";
    return false;
  }
  const int64_t n =
      static_cast<int64_t>(flat.w()) * static_cast<int64_t>(flat.h());
  const float* px = flat.px();
  bool all_zero = true;
  for (int64_t i = 0; i < n; ++i) {
    const float v = px[i];
    if (!std::isfinite(v)) { if (why) *why = "non-finite pixel"; return false; }
    if (v != 0.0f) all_zero = false;
  }
  if (all_zero) { if (why) *why = "all-zero frame"; return false; }
  // 中位数（口径与 master_generator.cpp median_of 一致：偶数取中间两值平均）
  std::vector<float> vals(px, px + static_cast<size_t>(n));
  const size_t mid = static_cast<size_t>(n) / 2;
  std::nth_element(vals.begin(), vals.begin() + mid, vals.end());
  float med = vals[mid];
  if (n % 2 == 0) {
    const float lower = *std::max_element(vals.begin(), vals.begin() + mid);
    med = (lower + med) * 0.5f;
  }
  if (!(med > 0.0f)) { if (why) *why = "median<=0"; return false; }
  return true;
}

// ── op: calibrate（唯一真实入口 ac_calibrate_frame; 语义对齐 p1_session calibrate 阶段）──
Result<void> p1_op_calibrate(const Json& doc, Json* man) {
  auto p1_lights_rc = p1_require_lights(doc);
  if (p1_lights_rc.failed()) return p1_lights_rc;
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
  // B2-A6: 非法 master flat 消费边界 fail-closed（median<=0 / 全零 / 非有限）。
  // 公式与 floor 语义零改动；仅在进入 calibrate 前拒绝整帧退化输入。
  if (flat.ok()) {
    std::string flat_why;
    if (!p1_master_flat_valid(flat, &flat_why)) {
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "master_flat invalid (" + flat_why +
              "): degenerate flat must not be consumed (SCI-CAL-001 §8)"));
    }
  }
  const bool dark_opt = doc.value("dark_optimization", false);
  const float k_fixed = doc.value("dark_scale_factor", 1.0f);
  // ── B2-A13: dark_opt=1 的 K 必须由 FITS EXPTIME 推导 (K=t_light/t_dark) ──
  // SCI-CAL-001 §5 / DATA_SEMANTICS §9.1 K 行: K 由调用方从 FITS EXPTIME 计算
  // 后传入 ac_calibrate_frame。仅当 bias+dark 均在位（calibrator 的 K 分支真正
  // 生效）时要求 EXPTIME；缺 bias/dark 时 calibrator 按合同回退标准分支
  // (K=1.0, P2-11 另行处置)，此处不越界。缺失/非正/不匹配 → DATA fail-closed。
  const bool k_branch = dark_opt && bias.ok() && dark.ok();
  double dark_exptime = 0.0;
  if (k_branch) {
    const AIOImageMetadata dmeta =
        aio_read_metadata(doc["master_dark"].get<std::string>().c_str());
    dark_exptime = dmeta.calibration.exptime;
    if (!std::isfinite(dark_exptime) || dark_exptime <= 0.0) {
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "master_dark FITS EXPTIME missing/<=0; K=t_light/t_dark requires EXPTIME"));
    }
  }
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
    float k_use = k_fixed;
    if (k_branch) {
      const AIOImageMetadata lmeta = aio_read_metadata(lp.c_str());
      const double t_light = lmeta.calibration.exptime;
      if (!std::isfinite(t_light) || t_light <= 0.0) {
        st_cal["status"] = "fail";
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "light FITS EXPTIME missing/<=0 (required for K=t_light/t_dark): " + lp));
      }
      const double k_expo = t_light / dark_exptime;
      if (!std::isfinite(k_expo) || k_expo <= 0.0) {
        st_cal["status"] = "fail";
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "K=t_light/t_dark invalid (non-finite/<=0) for " + lp));
      }
      // 显式配置标量不得与 EXPTIME 比不一致（禁配置冒充科学输入; 只做
      // fail-closed 门, 不改变 K 的推导公式与单位）。
      if (p1_has(doc, "dark_scale_factor")) {
        const double cfg_k = p1_num(doc, "dark_scale_factor", 1.0);
        if (std::fabs(cfg_k - k_expo) > 1e-6 * std::max(1.0, std::fabs(k_expo))) {
          st_cal["status"] = "fail";
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "dark_scale_factor (" + std::to_string(cfg_k) +
              ") disagrees with FITS EXPTIME ratio K=" + std::to_string(k_expo)));
        }
      }
      k_use = static_cast<float>(k_expo);
    }
    const int rc = ac_calibrate_frame(
        light.px(), W, H,
        dark.ok() ? dark.px() : nullptr,
        flat.ok() ? flat.px() : nullptr,
        bias.ok() ? bias.px() : nullptr,
        out.data(), dark_opt ? 1 : 0, k_use, &actual_k);
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
    // CORE-RACE-001: 原子发布（临时文件 + rename）——本路径是 drz/wcs 等并发
    // 消费者的共享输入, 任何时刻只允许存在完整文件。
    std::string werr;
    if (!p1_write_fits_atomic(wim, outp, &werr)) {
      (*man)["error_kind"] = "output";
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO, werr));
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
  auto p1_lights_rc = p1_require_lights(doc);
  if (p1_lights_rc.failed()) return p1_lights_rc;
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
    // 输入 = artifact:cal（cal 节点产物 calibrated_<base>, 无则原帧）
    const std::string in_path = p1_calibrated_path(doc, l.get<std::string>());
    P1Image im = p1_read_image(in_path);
    if (!im.ok()) {
      (*man)["error_kind"] = "input";
      st["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + in_path));
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
    // 输出 = artifact:cos（cleaned_<base>, 独立于上游 artifact:cal）+ 原子发布。
    // CORE-RACE-001: 修复前此处就地覆写 artifact:cal 路径 —— 与并发下游 drz
    // 读同一路径竞争, aio_write_fits 非原子 ⇒ 撕裂读（P1 数据完整性缺陷）。
    const std::string out_path = p1_cosmetic_path(doc, l.get<std::string>());
    std::string werr;
    if (!p1_write_fits_atomic(im, out_path, &werr)) {
      (*man)["error_kind"] = "output";
      st["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO, "cosmetic " + werr));
    }
    hot_total += hot;
    cold_total += cold;
    ++frames;
    artifacts.push_back(out_path);
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

// ── op: detect_sources（真实检测+拟合链: lib/algorithms/star_detection/wrapper_phase1 的
//      phase1::StarDetector 检测（局部峰 + 5×5 质心/二阶矩 + sigma-clip
//      背景, 5σ）→ star_det v1 [N,6] → lib/algorithms/psf dpsf_fit_batch_f64
//      Moffat4 批量 PSF 拟合（生产源零 diff; DPSF-PREC-105/FP64 双精度）;
//      输出 DATA-P1-SOURCES + DATA-P1-PSF(psf_params:FLOAT64[N,9])。
//
//      ⚠ 注释订正 (P2, 2026-09-14): 本节点检测器是 lib/algorithms/star_detection/wrapper_phase1 的
//      P1-003 桥接类 astrocs::phase1::StarDetector（d3af6ffa 引入），
//      **不是** lib/algorithms/star_detection 的 sdet —— sdet（sdet_create /
//      sdet_detect_ex_f64, 带 maxStars 截断）只被 wcs-platesolve 节点使用
//      （module_adapters.cpp:1995 sp.maxStars=2000）。旧注释误标为
//      "lib/algorithms/star_detection sdet 检测"; 误述源自 9e09941a 的提交信息。
//
//      PSF-FAST-001 (负责人裁决 2026-09-14): 生产路径只跑 FAST ——
//      DATA-P1-SOURCES 全量检测**不动**（p1_sources.json 与下游孔径测光
//      p1_flux.json 逐字节不变），只把 Moffat4 拟合限制到**最亮 N_fit 颗**
//      （配置 psf.max_stars, 默认 5000, 禁编译期硬编码; 0 = 不截断=全量精确）。
//      依据: psf_params 在仓库内零消费者、psf 端口为死边、测光正式口径=孔径
//      测光（负责人 2026-09-14 裁决）; 全量 145,884 颗拟合实测 156.7 s/帧,
//      最亮 5000 颗 ~4 s（REPORT.md §4）。完整精确路径保留为
//      p1_op_star_psf_precise（inactive, kPrecisePsfEnabled=false）。
Result<void> p1_op_star_psf_impl(const Json& doc, Json* man, int n_fit_limit) {
  auto p1_lights_rc = p1_require_lights(doc);
  if (p1_lights_rc.failed()) return p1_lights_rc;
  const std::string out_dir = doc.value("output_dir", std::string("."));
  // ── P14-N-08 (RQS V2-N-08): psf_mode 必须是**真实模式**, 不得为字面量 ──────
  // 模式由生效的拟合上限派生: n_fit_limit>0 ⇒ "fast"(只拟合最亮 N 颗);
  // n_fit_limit==0 ⇒ "precise"(全量无截断)。生产 dispatch (p1_op_star_psf) 依
  // kPrecisePsfEnabled + 节点配置决定传入值; 精确直调路径传 0。
  const std::string psf_mode = (n_fit_limit > 0) ? std::string("fast")
                                                 : std::string("precise");
  const astrocs::phase1::StarDetector det(5.0);
  Json frames = Json::array();
  std::vector<double> fwhm_xs, fwhm_ys, ells;
  int64_t n_valid_total = 0, n_total_total = 0, n_fit_total = 0;
  for (const auto& l : doc["input_lights"]) {
    // IR 输入端口 = artifact:cos → 消费 cosmetic 节点产物（CORE-RACE-001 接线）
    const std::string path = p1_cleaned_input_path(doc, l.get<std::string>());
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
    // PSF-FAST-001: 拟合输入 = 最亮 N_fit 颗（检测全量 N 保持不变）。
    // 选取: flux 降序 partial_sort, 同 flux 按检测下标升序 tie-break（确定性）;
    // 选完**按检测下标升序重排**, 使 star_id ↔ psf 行的 compact 映射口径与
    // B2-A2 完全一致（逐星真值索引仍由 psf_status 承载）。
    std::vector<int> fit_idx(N);
    for (size_t i = 0; i < N; ++i) fit_idx[i] = static_cast<int>(i);
    size_t N_fit = N;
    if (n_fit_limit > 0 && static_cast<size_t>(n_fit_limit) < N) {
      std::partial_sort(fit_idx.begin(),
                        fit_idx.begin() + n_fit_limit, fit_idx.end(),
                        [&](int a, int b) {
                          const double fa = cat.sources[static_cast<size_t>(a)].flux;
                          const double fb = cat.sources[static_cast<size_t>(b)].flux;
                          if (fa != fb) return fa > fb;   // 亮度降序
                          return a < b;                    // tie-break: 检测序
                        });
      fit_idx.resize(static_cast<size_t>(n_fit_limit));
      std::sort(fit_idx.begin(), fit_idx.end());
      N_fit = static_cast<size_t>(n_fit_limit);
    }
    std::vector<double> dets(N_fit * 6, 0.0);
    for (size_t k = 0; k < N_fit; ++k) {
      const auto& s = cat.sources[static_cast<size_t>(fit_idx[k])];
      dets[k * 6 + 0] = s.x;
      dets[k * 6 + 1] = s.y;
      dets[k * 6 + 2] = s.flux;
      dets[k * 6 + 3] = (s.flux > 0.0)
          ? -2.5 * std::log10(s.flux) : 99.0;
      dets[k * 6 + 4] = (s.quality & 1) ? 1.0 : 0.0;   // saturated
      dets[k * 6 + 5] = (cat.n_saturated > 0) ? 1.0 : 0.0;
    }
    // 真实 PSF 拟合: dpsf_fit_batch_f64（float32 检测帧 → double 全链拟合,
    // 数据保真升精度; 默认拟合参数）
    std::vector<double> psf_params(N_fit * 9, 0.0);
    // B2-A2 (RESCUE-P0-05): 逐星拟合状态 out_status[k] 按【拟合输入下标 k】
    // 报告结果（k → 检测下标 fit_idx[k]）; 成功行在 psf_params 中顺序 compact
    // 存放。消费方必须按状态映射, 禁止按 k < n_valid 前缀截断。
    std::vector<int> psf_status(N_fit, DPSF_PSF_STATUS_FIT_FAILED);
    int n_valid = 0;
    if (N_fit > 0) {
      std::vector<double> dbuf(static_cast<size_t>(im.w()) * static_cast<size_t>(im.h()));
      for (size_t i = 0; i < dbuf.size(); ++i) dbuf[i] = static_cast<double>(im.px()[i]);
      const int drc = dpsf_fit_batch_f64(
          dbuf.data(), im.w(), im.h(), dets.data(), static_cast<int>(N_fit),
          nullptr, psf_params.data(), &n_valid, psf_status.data());
      if (drc != 0) {
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "dpsf_fit_batch_f64 failed rc=" + std::to_string(drc)));
      }
      if (n_valid <= 0) {
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "dpsf_fit_batch_f64: 0/" + std::to_string(N_fit) + " fits converged"));
      }
      // 成功行按拟合输入序（== 检测序）compact; 逐星按真值索引取行 (row 只读)
      int row = 0;
      for (size_t k = 0; k < N_fit; ++k) {
        if (psf_status[k] != DPSF_PSF_STATUS_OK) continue;
        // [7]=fwhm_x [8]=fwhm_y; sx=sigma_x → fwhm=2.3548*sx（由 9 列取 [7]/[8] 权威值）
        fwhm_xs.push_back(psf_params[static_cast<size_t>(row) * 9 + 7]);
        fwhm_ys.push_back(psf_params[static_cast<size_t>(row) * 9 + 8]);
        const double sx = psf_params[static_cast<size_t>(row) * 9 + 4];
        const double sy = psf_params[static_cast<size_t>(row) * 9 + 5];
        const double mx = std::max(sx, sy), mn = std::min(sx, sy);
        ells.push_back(mx > 0.0 ? 1.0 - mn / mx : 0.0);
        ++row;
      }
      n_valid_total += n_valid;
    }
    n_total_total += static_cast<int64_t>(N);
    n_fit_total += static_cast<int64_t>(N_fit);
    Json sources = Json::array();
    for (const auto& s : cat.sources) {
      sources.push_back(Json{{"id", s.id}, {"x", s.x}, {"y", s.y},
                             {"flux", s.flux}, {"fwhm_px", s.fwhm_px},
                             {"ellipticity", s.ellipticity}, {"snr", s.snr},
                             {"quality", s.quality}});
    }
    // B2-A2: star_id ↔ PSF 行按逐星真值状态映射 (row 顺序 compact)
    Json psf_rows = Json::array();
    {
      int row = 0;
      for (size_t k = 0; k < N_fit; ++k) {
        if (psf_status[k] != DPSF_PSF_STATUS_OK) continue;
        const size_t i = static_cast<size_t>(fit_idx[k]);   // PSF-FAST-001: 子集映射
        psf_rows.push_back(Json{{"star_id", cat.sources[i].id},
                                {"B", psf_params[static_cast<size_t>(row)*9+0]},
                                {"A", psf_params[static_cast<size_t>(row)*9+1]},
                                {"cx", psf_params[static_cast<size_t>(row)*9+2]},
                                {"cy", psf_params[static_cast<size_t>(row)*9+3]},
                                {"sx", psf_params[static_cast<size_t>(row)*9+4]},
                                {"sy", psf_params[static_cast<size_t>(row)*9+5]},
                                {"theta", psf_params[static_cast<size_t>(row)*9+6]},
                                {"fwhm_x", psf_params[static_cast<size_t>(row)*9+7]},
                                {"fwhm_y", psf_params[static_cast<size_t>(row)*9+8]}});
        ++row;
      }
    }
    frames.push_back(Json{{"file", p1_base_name(path)},
                          {"n_detected", cat.n_detected},
                          {"n_saturated", cat.n_saturated},
                          {"n_edge", cat.n_edge},
                          {"background", cat.background},
                          {"noise_sigma", cat.noise_sigma},
                          {"n_psf_valid", n_valid},
                          // P14-N-08 (RQS V2-N-08): 交付样本真实性 provenance——
                          // psf_mode=真实模式; n_sources=可用源总数(全量检测);
                          // n_fit_input=真正送入 Moffat4 拟合的星数(受性能上限);
                          // psf_fit_truncated=拟合输入是否被 psf.max_stars 截断。
                          {"psf_mode", psf_mode},
                          {"n_sources", static_cast<int64_t>(N)},
                          {"n_fit_input", static_cast<int64_t>(N_fit)},
                          {"fit_limit", n_fit_limit},
                          {"psf_fit_truncated", N_fit < N},
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
                      // B2-A2: 星↔参数行映射权威 = 逐星状态 (parsed rows 已按
                      // DPSF_PSF_STATUS_OK compact; 失败星不入 psf_params)
                      {"status_schema", DPSF_PSF_STATUS_SCHEMA},
                      {"entry", "dpsf_fit_batch_f64"},
                      // PSF-FAST-001 (负责人裁决 2026-09-14) + P14-N-08:
                      // psf_mode = 真实模式（n_fit_limit>0 ⇒ "fast"/"precise",
                      // 由 p1_op_star_psf_impl 派生, 不再是字面量）。
                      // n_sources = 全量检测星数（与 p1_sources.n_detected 一致,
                      // 未截断）; n_fit_input = 实际送入拟合的最亮星数（配置
                      // psf.max_stars）。median_* 统计口径 = **拟合子集**的成功星
                      // （旧口径 = 全量 145,884 颗含 54% 失败星的混合集）。
                      // truncated = 拟合输入被 psf.max_stars 截断（性能开关, 只
                      // 影响拟合成本; 交付 SNR/深度不读该子集, 见 p1_op_noise）。
                      // 精确路径保留但 inactive（见 p1_op_star_psf_precise）。
                      {"psf_mode", psf_mode},
                      {"n_sources", n_total_total},
                      {"n_fit_input", n_fit_total},
                      {"fit_limit", n_fit_limit},
                      {"truncated", n_fit_total < n_total_total},
                      {"n_valid", n_valid_total},
                      {"median_fwhm_x_px", median(fwhm_xs)},
                      {"median_fwhm_y_px", median(fwhm_ys)},
                      {"median_ellipticity", median(ells)}};
  if (!p1_write_text(src_path, cat_out.dump(2)) || !p1_write_text(psf_path, psf_out.dump(2))) {
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  }
  (*man)["frames"] = static_cast<uint64_t>(frames.size());
  (*man)["n_sources"] = n_total_total;
  (*man)["n_fit_input"] = n_fit_total;      // PSF-FAST-001
  (*man)["psf_mode"] = psf_mode;            // P14-N-08: 真实模式 (非字面量)
  (*man)["fit_limit"] = n_fit_limit;        // P14-N-08
  (*man)["psf_fit_truncated"] = n_fit_total < n_total_total;  // P14-N-08
  (*man)["n_psf_valid"] = n_valid_total;
  (*man)["sources_artifact"] = src_path;
  (*man)["psf_artifact"] = psf_path;
  (*man)["artifacts"] = Json::array({src_path, psf_path});
  return Result<void>::success();
}

// ── PSF-FAST-001: 拟合星数上限（节点级配置, 禁硬编码, 宪章 §10.4）────────────
// 与 wcs 节点 sp.maxStars=2000（同文件 wcs-platesolve 段）同为节点配置口径。
// psf.max_stars: 送入 Moffat4 拟合的**最亮星数**; 0 = 不截断（全量精确路径）。
int p1_psf_fit_limit(const Json& doc) {
  const Json psf = (p1_has(doc, "psf") && doc["psf"].is_object())
                       ? doc["psf"] : Json::object();
  return p1_int(psf, "max_stars", 5000);
}

// ── PSF-FAST-001 / INACTIVE: 完整精确 PSF 路径（保留实现, 生产路径不调用）──────
// 负责人裁决 2026-09-14: (a) 测光正式口径 = 孔径测光（现状实现）; (b) psf 端口
// 为死边、psf_params 在仓库内零消费者, 故 PSF 测光本轮及后续都不作为要求。
// ⇒ 精确 PSF 生产上不启用; 但按裁决**完整实现予以保留**（不删算法代码）。
// 唯一启用开关（恒 false ⇒ 生产路径永不进入精确分支）:
constexpr bool kPrecisePsfEnabled = false;

// 节点级精确路径: 与 FAST 同链, 唯一差别 = n_fit_limit=0（**全量**检测星不截断）。
// 直调测试: tests/unit/p1001_real_nodes_test.cpp::test_starpsf_precise_inactive_
// direct_call（证明本路径仍可编译且能跑出结果, 防止被当作死代码清理）。
Result<void> p1_op_star_psf_precise(const Json& doc, Json* man) {
  return p1_op_star_psf_impl(doc, man, /*n_fit_limit=*/0);
}

// 生产入口（p1_nodes 表绑定 operation=detect_sources）: FAST 模式。
Result<void> p1_op_star_psf(const Json& doc, Json* man) {
  // 唯一 dispatch 点。kPrecisePsfEnabled 恒 false ⇒ 编译期丢弃精确分支
  // （"保留实现但不工作"）; 未来若改口径, 只改此开关 + 合同登记。
  if constexpr (kPrecisePsfEnabled) {
    return p1_op_star_psf_precise(doc, man);
  }
  return p1_op_star_psf_impl(doc, man, p1_psf_fit_limit(doc));
}

// ── B2-A17 (AUD-COORD F-03): SIP 系数桥接 ─────────────────────────────────────────
// 缺陷: 解算结果 IpvWcsResult 携带 sip_a/sip_b/sip_ap/sip_bp (ipv_api.h:41-48,
// 36 项 i*6+j 布局) 与 ctype1/ctype2 ("RA---TAN-SIP"), 但 p1_op_wcs 只把
// sip_order 写进 p1_wcs.json (从不落盘系数), p1_op_drizzle 也只透传 7 个线性
// WCS 参数到 frame header → drizzle 侧永远走无 SIP 的线性分支, A/B/AP/BP
// 全线丢失 (WcsSip 支持 SIP 但生产链从不喂它)。
// 桥接契约 (FITS SIP 约定 + WcsSip 消费口径):
//   p1_wcs.json: wcs.{crpix1..cd22, ctype1, ctype2, sip:{order, ap_order,
//                a[36], b[36], ap[36], bp[36]}} (系数存在才写 sip 对象)
//   frame header: CTYPE1/CTYPE2 + A_ORDER/B_ORDER/AP_ORDER/BP_ORDER +
//                A_i_j/B_i_j/AP_i_j/BP_i_j (hp_drizzle_api.cpp:586-645 读面)
// 系数按 i*6+j 存 (与 hp_drizzle_api 读侧同构), 逐项有限性校验; order 超出
// drizzle 正式支持域 [0,5] → DATA 拒绝 (禁静默截断)。
struct P1SipCoeffs {
  int order = 0;
  int ap_order = 0;
  double a[36] = {0};
  double b[36] = {0};
  double ap[36] = {0};
  double bp[36] = {0};
  bool present = false;
};

P1SipCoeffs p1_parse_sip(const Json& wc, bool* ok, std::string* err) {
  P1SipCoeffs out;
  *ok = true;
  const Json& sip = wc.contains("sip") && wc["sip"].is_object() ? wc["sip"] : Json::object();
  if (sip.empty()) return out;  // 无 SIP = 合法 (无畘变路径, 基线逐字节等价)
  auto rd_order = [&](const char* k, int dflt) -> int {
    if (!p1_has(sip, k)) return dflt;
    const Json& v = sip[k];
    if (!v.is_number_integer()) { *ok = false; *err = std::string(k) + " must be integer"; return 0; }
    return v.get<int>();
  };
  const int order = rd_order("order", 0);
  const int ap_order = rd_order("ap_order", 0);
  if (!*ok) return out;
  if (order < 0 || order > 5 || ap_order < 0 || ap_order > 5) {
    *ok = false;
    *err = "sip order out of drizzle contract [0,5] (order=" + std::to_string(order) +
           ", ap_order=" + std::to_string(ap_order) + ")";
    return out;
  }
  auto rd_arr = [&](const char* k, double* dst) -> bool {
    if (!p1_has(sip, k)) return true;  // 允许缺省 (全零)
    const Json& v = sip[k];
    if (!v.is_array() || v.size() != 36) return false;
    for (std::size_t i = 0; i < 36; ++i) {
      if (!v[i].is_number()) return false;
      const double d = v[i].get<double>();
      if (!std::isfinite(d)) return false;
      dst[i] = d;
    }
    return true;
  };
  if (!rd_arr("a", out.a) || !rd_arr("b", out.b) || !rd_arr("ap", out.ap) ||
      !rd_arr("bp", out.bp)) {
    *ok = false;
    *err = "sip coefficient array must be 36 finite numbers (a/b/ap/bp)";
    return out;
  }
  out.order = order;
  out.ap_order = ap_order;
  out.present = true;
  return out;
}

// SIP 前向修正 (FITS paper IV §2.1: U = dx + A(dx,dy), V = dy + B(dx,dy))。
// 独立于 WcsSip (与 drizzle 生产实现不同翻译单元; 交叉门用)。
void p1_sip_poly(const double* c, double dx, double dy, int order, double* out) {
  double acc = 0.0;
  for (int i = 0; i <= order; ++i) {
    for (int j = 0; i + j <= order; ++j) {
      acc += c[i * 6 + j] * std::pow(dx, i) * std::pow(dy, j);
    }
  }
  *out = acc;
}

void p1_tan_forward_reference_sip(const P1SipCoeffs& sip, double crpix1,
                                  double crpix2, double crval1, double crval2,
                                  double cd11, double cd12, double cd21,
                                  double cd22, double x, double y, double* ra,
                                  double* dec) {
  const double dx = x - crpix1;
  const double dy = y - crpix2;
  double A = 0.0, B = 0.0;
  if (sip.present && sip.order > 0) {
    p1_sip_poly(sip.a, dx, dy, sip.order, &A);
    p1_sip_poly(sip.b, dx, dy, sip.order, &B);
  }
  p1_tan_forward_reference(crpix1, crpix2, crval1, crval2, cd11, cd12, cd21,
                           cd22, crpix1 + (dx + A), crpix2 + (dy + B), ra, dec);
}

// 把 SIP 系数写入 frame header KV (drizzle 读面 hp_drizzle_api.cpp:586-645)。
// 返回 false 表示 kv_set 失败。CTYPE 由调用方按是否含 SIP 选择 -SIP 后缀。
bool p1_sip_write_header_frame(void* frame, const P1SipCoeffs& sip,
                               bool (*kv_set)(void*, const char*, const char*),
                               std::string* err) {
  auto set = [&](const char* k, const std::string& v) -> bool {
    if (!kv_set(frame, k, v.c_str())) { *err = std::string("kv_set failed: ") + k; return false; }
    return true;
  };
  if (!sip.present || sip.order <= 0) return true;  // 无 SIP: 不写任何 SIP 键
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%d", sip.order);
  if (!set("A_ORDER", buf) || !set("B_ORDER", buf)) return false;
  for (int i = 0; i <= sip.order; ++i) {
    for (int j = 0; i + j <= sip.order; ++j) {
      if (i + j == 0) continue;
      char key[24];
      std::snprintf(key, sizeof(key), "A_%d_%d", i, j);
      std::snprintf(buf, sizeof(buf), "%.17g", sip.a[i * 6 + j]);
      if (!set(key, buf)) return false;
      std::snprintf(key, sizeof(key), "B_%d_%d", i, j);
      std::snprintf(buf, sizeof(buf), "%.17g", sip.b[i * 6 + j]);
      if (!set(key, buf)) return false;
    }
  }
  if (sip.ap_order > 0) {
    std::snprintf(buf, sizeof(buf), "%d", sip.ap_order);
    if (!set("AP_ORDER", buf) || !set("BP_ORDER", buf)) return false;
    for (int i = 0; i <= sip.ap_order; ++i) {
      for (int j = 0; i + j <= sip.ap_order; ++j) {
        if (i + j == 0) continue;
        char key[24];
        std::snprintf(key, sizeof(key), "AP_%d_%d", i, j);
        std::snprintf(buf, sizeof(buf), "%.17g", sip.ap[i * 6 + j]);
        if (!set(key, buf)) return false;
        std::snprintf(key, sizeof(key), "BP_%d_%d", i, j);
        std::snprintf(buf, sizeof(buf), "%.17g", sip.bp[i * 6 + j]);
        if (!set(key, buf)) return false;
      }
    }
  }
  return true;
}

// B2-A17: SIP 系数 JSON 序列化 (落盘/下发共用同一形状; 无 SIP → 不写键)。
Json p1_sip_to_json(const P1SipCoeffs& sip) {
  if (!sip.present) return Json();
  Json sa = Json::array(), sb = Json::array(), sap = Json::array(), sbp = Json::array();
  for (int k = 0; k < 36; ++k) {
    sa.push_back(sip.a[k]); sb.push_back(sip.b[k]);
    sap.push_back(sip.ap[k]); sbp.push_back(sip.bp[k]);
  }
  return Json{{"order", sip.order}, {"ap_order", sip.ap_order},
              {"a", sa}, {"b", sb}, {"ap", sap}, {"bp", sbp}};
}

// ── P9 (F-10): 帧自有关键字读取 —— 初始指向/板尺度只从非 WCS 关键字推导 ───
// 负责人裁定: 帧头 WCS（CRVAL1/2、PLTSOLVD、CD/PC、SIP）未授权, 不得作为
// 初始指向或任何解算输入。下面两个 helper 只读 OBJCTRA/OBJCTDEC/RA/DEC/
// FOCALLEN/XPIXSZ 一类观测关键字, 不触碰任何 WCS 关键字。
// 板尺度常量: 206.265 = (180×3600)/π × 1e-3, 把 (XPIXSZ μm)/(FOCALLEN mm)
// 直接转为 角秒/像素 —— 与求解器唯一权威常量 ipv_select.cpp:57
// IPV_ARCSEC_PER_UM_PER_MM(=206.265) 及 DATA_SEMANTICS §18.1 逐位一致。
// （206264.806 是 asec/rad 常量, 只有当 XPIXSZ 以 mm 记时才成立; FITS 头
//  XPIXSZ 以 μm 记, 故统一用 206.265, 避免 1e-3 量纲错。）
static constexpr double kP9AsecPerUmPerMm = 206.265;
std::string p1_header_kw_str(const P1Image& im, const char* key) {
  if (!im.ok()) return std::string();
  const int n = aio_get_keyword_count(im.p);
  for (int i = 0; i < n; ++i) {
    const AIOFITSKeyword kw = aio_get_keyword(im.p, i);
    if (std::strcmp(kw.name, key) == 0) return std::string(kw.value);
  }
  return std::string();
}

// 六进制/十进制角度解析: "18 11 14.00" / "18:11:14" / "18h11m14s" / "272.8"。
// is_ra=true 时结果 ×15（OBJCTRA/RA 约定为小时）并归一化到 [0,360)。
// 解析失败返回 false（调用方 fail-closed, 禁 silent default）。
bool p1_parse_ra_dec_deg(const std::string& raw, bool is_ra, double* out_deg) {
  std::string t;
  t.reserve(raw.size());
  bool neg = false;
  for (char c : raw) {
    switch (c) {
      case 'h': case 'H': case 'd': case 'D':
      case 'm': case 'M': case 's': case 'S':
      case ':': case '/': case '\'': case '"':
        t.push_back(' '); break;
      case '-':
        neg = true; t.push_back('-'); break;
      default: t.push_back(c);
    }
  }
  std::istringstream is(t);
  std::vector<double> v;
  double x = 0.0;
  while (is >> x) v.push_back(x);
  if (v.empty()) return false;
  double mag = 0.0, div = 1.0;
  for (size_t k = 0; k < v.size() && k < 3; ++k) {
    if (v[k] < 0.0) neg = true;
    mag += std::fabs(v[k]) / div;
    div *= 60.0;
  }
  if (!std::isfinite(mag)) return false;
  double deg = is_ra ? mag * 15.0 : mag;
  if (is_ra) {
    deg = std::fmod(deg, 360.0);
    if (deg < 0.0) deg += 360.0;
  } else if (neg) {
    deg = -deg;
  }
  *out_deg = deg;
  return true;
}

// ── P9: header_pointing 初始指向 + 板尺度（帧自有关键字, 非帧头 WCS）──────
// 中心: OBJCTRA/OBJCTDEC（六进制, RA 小时×15）, 回退 RA/DEC;
// 板尺度: s0 = 206.265 * XPIXSZ / FOCALLEN（FOCALLEN 单位 mm, XPIXSZ 单位 μm;
// 常量与求解器 ipv_select.cpp:57 唯一权威一致, 见 kP9AsecPerUmPerMm 注释）。
// 任一不可得 → 返回 false 并写 *why（调用方 DATA fail-closed, 禁 silent default）。
bool p1_header_pointing(const P1Image& im, double* ra0, double* dec0,
                        double* focal_mm, double* pixel_um, double* s0,
                        std::string* src, std::string* why) {
  const AIOImageMetadata meta = aio_get_metadata(im.p);
  std::string ra_s = p1_header_kw_str(im, "OBJCTRA");
  std::string de_s = p1_header_kw_str(im, "OBJCTDEC");
  const char* used = nullptr;
  if (!ra_s.empty() && !de_s.empty()) {
    used = "OBJCTRA/OBJCTDEC";
  } else {
    ra_s = p1_header_kw_str(im, "RA");
    de_s = p1_header_kw_str(im, "DEC");
    if (!ra_s.empty() && !de_s.empty()) used = "RA/DEC";
  }
  if (used == nullptr) {
    *why = "frame header has no pointing keywords (OBJCTRA/OBJCTDEC, RA/DEC)";
    return false;
  }
  double ra = 0.0, dec = 0.0;
  if (!p1_parse_ra_dec_deg(ra_s, true, &ra) ||
      !p1_parse_ra_dec_deg(de_s, false, &dec)) {
    *why = std::string("frame pointing keyword unparseable: ") + used +
           "='" + ra_s + "'/'" + de_s + "'";
    return false;
  }
  if (!meta.observation.has_focallen || !meta.observation.has_xpixsz ||
      !(meta.observation.focallen > 0.0) || !(meta.observation.xpixsz > 0.0)) {
    *why = "frame header lacks valid FOCALLEN/XPIXSZ "
           "(needed for s0=206.265*XPIXSZ/FOCALLEN, XPIXSZ in um)";
    return false;
  }
  *ra0 = ra;
  *dec0 = dec;
  *focal_mm = meta.observation.focallen;
  *pixel_um = meta.observation.xpixsz;
  *s0 = kP9AsecPerUmPerMm * meta.observation.xpixsz / meta.observation.focallen;
  *src = std::string(used) + "+FOCALLEN/XPIXSZ";
  return true;
}

// ── op: plate_solve（真实求解器链: lib/algorithms/platesolve ipv——sdet 句柄 +
//      gaia_client 句柄注入 IPVSolver → ipv_solve_from_memory_with_callback_d
//      FP64 全链解算 → IpvWcsResult(CD/CRVAL/CRPIX/RMS) → WcsTan roundtrip
//      自检。生产源零 diff（FD-05 后 Linux amd64 与 Windows 经同一组 C API
//      静态/动态直连真实求解器; 源内已无平台 stub, 两平台均为真实求解）。
//      F-9 起求解器输出边界另加 parity/尺度绝对合理性闸门（见 ipv extract_wcs_sip）──
Result<void> p1_op_wcs(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  if (!p1_has(doc, "input_lights") || doc["input_lights"].empty()) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "input_lights required"));
  }
  const Json& wc = doc.contains("wcs") && doc["wcs"].is_object() ? doc["wcs"] : Json::object();
  // FIX-E2E B1-A1: 显式 WCS 配置路径（调用方给定线性 WCS 时的校验/透传通道, 不伪造求解）。
  // 当配置显式给出线性 WCS 8 参数时, 本节点做"显式 WCS 校验 + 透传": 校验有限性/
  // 可逆性(det!=0) + WcsTan roundtrip 自检 + B2-A1 独立前向交叉绝对门, 写 p1_wcs.json 并标记
  // wcs_source="explicit_config"、solver="none(explicit_config)"; 不调用 ipv, 也不
  // 冒充求解结果。未提供显式参数时保持原真实 ipv 求解链（Windows/有求解器平台）。
  const bool explicit_wcs = p1_has(wc, "crpix1") && p1_has(wc, "crpix2") &&
                            p1_has(wc, "crval1") && p1_has(wc, "crval2") &&
                            p1_has(wc, "cd11") && p1_has(wc, "cd12") &&
                            p1_has(wc, "cd21") && p1_has(wc, "cd22");
  if (explicit_wcs) {
    astrocs::phase1::WcsTan wcs;
    wcs.crpix1 = p1_num(wc, "crpix1", 0.0); wcs.crpix2 = p1_num(wc, "crpix2", 0.0);
    wcs.crval1 = p1_num(wc, "crval1", 0.0); wcs.crval2 = p1_num(wc, "crval2", 0.0);
    wcs.cd11 = p1_num(wc, "cd11", 0.0); wcs.cd12 = p1_num(wc, "cd12", 0.0);
    wcs.cd21 = p1_num(wc, "cd21", 0.0); wcs.cd22 = p1_num(wc, "cd22", 0.0);
    const double det = wcs.cd11 * wcs.cd22 - wcs.cd12 * wcs.cd21;
    const double vals[8] = {wcs.crpix1, wcs.crpix2, wcs.crval1, wcs.crval2,
                            wcs.cd11, wcs.cd12, wcs.cd21, wcs.cd22};
    for (double v : vals)
      if (!std::isfinite(v))
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "explicit wcs params must be finite (crpix/crval/cd)"));
    if (!std::isfinite(det) || det == 0.0)
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "explicit wcs CD matrix is singular (det==0)"));
    // B2-A17: 显式配置路径同样支持 SIP 畸变系数 (无畸变 = 不写 sip 对象)。
    bool sip_ok = true;
    std::string sip_err;
    const P1SipCoeffs sip = p1_parse_sip(wc, &sip_ok, &sip_err);
    if (!sip_ok)
      return Result<void>::fail(Error(ErrorDomain::DATA, "explicit wcs " + sip_err));
    const char* ctype1 = sip.present ? "RA---TAN-SIP" : "RA---TAN";
    const char* ctype2 = sip.present ? "DEC--TAN-SIP" : "DEC--TAN";
    const std::string frame0 = p1_calibrated_path(doc, doc["input_lights"][0].get<std::string>());
    P1Image im = p1_read_image(frame0);
    if (!im.ok()) {
      (*man)["error_kind"] = "input";
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + frame0));
    }
    const int W = im.w(), H = im.h();
    std::vector<std::pair<double, double>> pts;
    const int step = std::max(1, std::max(W, H) / 8);
    for (int y = 0; y < H; y += step)
      for (int x = 0; x < W; x += step)
        pts.emplace_back(static_cast<double>(x), static_cast<double>(y));
    Json samples = Json::array();
    double max_rt = 0.0;
    double max_cross_deg = 0.0;
    // B2-A17: SIP A/B 前向修正叠加在 WcsTan 线性 pix2sky 之上 (WcsSip 同式,
    // pixelToSkyT: dx' = dx + A(dx,dy), dy' = dy + B(dx,dy))。
    for (const auto& [x, y] : pts) {
      double ra = 0.0, dec = 0.0, bx = 0.0, by = 0.0;
      if (sip.present && sip.order > 0) {
        double A = 0.0, B = 0.0;
        p1_sip_poly(sip.a, x - wcs.crpix1, y - wcs.crpix2, sip.order, &A);
        p1_sip_poly(sip.b, x - wcs.crpix1, y - wcs.crpix2, sip.order, &B);
        wcs.pix2sky(x + A, y + B, &ra, &dec);
      } else {
        wcs.pix2sky(x, y, &ra, &dec);
      }
      wcs.sky2pix(ra, dec, &bx, &by);
      const double rt = std::sqrt((bx - x) * (bx - x) + (by - y) * (by - y));
      if (rt > max_rt) max_rt = rt;
      // B2-A1/B2-A17: 绝对门 —— 与独立 gnomonic 前向参考解的角度残差
      // (独立参考解同样施加 SIP A/B; 与 sky2pix/pix2sky 自洽无关)。
      double ra_ref = 0.0, dec_ref = 0.0;
      p1_tan_forward_reference_sip(sip, wcs.crpix1, wcs.crpix2, wcs.crval1,
                                   wcs.crval2, wcs.cd11, wcs.cd12, wcs.cd21,
                                   wcs.cd22, x, y, &ra_ref, &dec_ref);
      const double cross = p1_angular_sep_deg(ra, dec, ra_ref, dec_ref);
      if (cross > max_cross_deg) max_cross_deg = cross;
      samples.push_back(Json{{"x", x}, {"y", y}, {"ra", ra}, {"dec", dec},
                             {"ra_ref", ra_ref}, {"dec_ref", dec_ref},
                             {"forward_cross_deg", cross},
                             {"roundtrip_px", rt}});
    }
    // roundtrip 保留为次级不变量 (正反互逆; 对成对单位错零鉴别力)。无 SIP
    // 时线性正反互逆仍按原 1e-6 px 契约; 有 SIP 时 sky2pix 走逆向多项式,
    // 收敛性取决于 AP/BP 是否与 A/B 严格互逆 (生产 ipv 保证), 显式配置面
    // 不做逆多项式存在性假设 —— 该面绝对正确性由前向交叉门独立保证。
    if (!sip.present && (!std::isfinite(max_rt) || max_rt >= 1e-6)) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "explicit WcsTan roundtrip " + std::to_string(max_rt) + " px exceeds 1e-6 contract"));
    }
    // B2-A1 绝对正确性门: 与独立前向参考解的角距 ≤1e-9 deg (≈3.6e-6")。
    // 阈值依据: 两路径均为 FP64 且本尺度 (|xi,eta| <= ~0.1 deg) 下舍入
    // ~1e-15 deg; 1e-9 deg 高出舍入 6 个量级; 旧缺陷偏差 ~5.6e-1 deg、
    // +0.5px 注入 ~5.5e-5 deg 均远大于该门 => 真正可失败, 非恒真。存在 SIP
    // 时交叉残差含 SIP 多项式求值差异 (<1e-12 deg), 阈值不变。
    if (!std::isfinite(max_cross_deg) || max_cross_deg > 1e-9) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "explicit WcsTan forward cross " + std::to_string(max_cross_deg) +
          " deg exceeds 1e-9 absolute contract"));
    }

    const std::string out_path = out_dir + "/p1_wcs.json";
    Json wcs_obj = Json{{"crpix1", wcs.crpix1}, {"crpix2", wcs.crpix2},
                        {"crval1", wcs.crval1}, {"crval2", wcs.crval2},
                        {"cd11", wcs.cd11}, {"cd12", wcs.cd12},
                        {"cd21", wcs.cd21}, {"cd22", wcs.cd22},
                        {"ctype1", ctype1}, {"ctype2", ctype2}};
    // B2-A17: SIP 系数落盘 (消费方 = p1_op_drizzle frame header 桥接)。
    if (const Json sj = p1_sip_to_json(sip); !sj.is_null()) wcs_obj["sip"] = sj;
    Json wcs_out = Json{{"schema", "DATA-P1-WCS"},
                        {"solver", "none(explicit_config)"},
                        {"wcs_source", "explicit_config"},
                        {"initial", false},
                        {"wcs", wcs_obj},
                        {"n_samples", samples.size()},
                        {"max_roundtrip_px", max_rt},
                        {"max_forward_cross_deg", max_cross_deg},
                        {"forward_cross_ref", "p1_tan_forward_reference"},
                        {"samples", samples}};
    if (!p1_write_text(out_path, wcs_out.dump(2)))
      return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
    (*man)["wcs_source"] = "explicit_config";
    (*man)["n_samples"] = samples.size();
    (*man)["max_roundtrip_px"] = max_rt;
    (*man)["max_forward_cross_deg"] = max_cross_deg;
    (*man)["wcs_artifact"] = out_path;
    (*man)["artifacts"] = Json::array({out_path});
    return Result<void>::success();
  }
  // ── F-10 / P9: 初始化指向来源策略显式化（禁 silent 用错误指向）───────────
  // 负责人裁定（P9）: 帧头 WCS **未授权** —— 本节点不得读取/使用帧头的
  // CRVAL1/2、PLTSOLVD、CD/PC 或 SIP 作为初始指向或任何解算输入。
  // 初始指向与板尺度只允许以下三种来源, 由 wcs.init_source 显式声明, 默认
  // 首选 header_pointing（帧自身关键字总能推出中心与板尺度）:
  //   header_pointing : 帧自有关键字 —— 中心 OBJCTRA/OBJCTDEC（六进制, RA 小时
  //                     ×15; 回退 RA/DEC）; 板尺度 s0=206.265*XPIXSZ/FOCALLEN
  //                     （FOCALLEN mm, XPIXSZ μm）。**不读帧头 WCS**。
  //   config          : config.wcs.ra0/dec0 与 config focal_length_mm/pixel_size_um
  //                     （调用方给定的数据集指向; 与帧头 WCS 无关）。
  //   neighbor_crval  : config.wcs.neighbor_ra0/neighbor_dec0 —— 该来源必须来自
  //                     **我们自己已解出的产物**（本管线 p1_wcs.json 的 crval, 由
  //                     调用方从历史产物回填）, 明确不是帧头 WCS。
  // 非法值 / 来源不可得 → DATA fail-closed（不给错解, 禁 silent default）。
  const std::string init_source = wc.value("init_source", std::string("header_pointing"));
  if (init_source != "header_pointing" && init_source != "config" &&
      init_source != "neighbor_crval") {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "wcs.init_source must be one of header_pointing|config|neighbor_crval "
        "(header_crval removed: 帧头 WCS 未授权)"));
  }
  // Gaia 数据目录是真实求解链必需的数据参数（不可从帧推出, 仍须显式给出）
  const std::string gaia_dir = wc.value("gaia_data_dir", std::string());
  if (gaia_dir.empty()) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "wcs config requires gaia_data_dir (real ipv solve chain; no silent defaults)"));
  }
  double focal_mm = std::numeric_limits<double>::quiet_NaN();
  double pixel_um = std::numeric_limits<double>::quiet_NaN();
  double ra0 = std::numeric_limits<double>::quiet_NaN();
  double dec0 = std::numeric_limits<double>::quiet_NaN();
  double s0_arcsec_px = std::numeric_limits<double>::quiet_NaN();
  std::string init_center_src;  // 指向来源（配置键或帧关键字, F-10 审计）
  if (init_source == "config" || init_source == "neighbor_crval") {
    focal_mm = p1_num(wc, "focal_length_mm", std::numeric_limits<double>::quiet_NaN());
    pixel_um = p1_num(wc, "pixel_size_um", std::numeric_limits<double>::quiet_NaN());
    if (std::isnan(focal_mm) || std::isnan(pixel_um)) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "wcs.init_source=" + init_source +
          " requires config focal_length_mm/pixel_size_um (no silent defaults)"));
    }
    s0_arcsec_px = kP9AsecPerUmPerMm * pixel_um / focal_mm;
    if (init_source == "config") {
      ra0 = p1_num(wc, "ra0", ra0);
      dec0 = p1_num(wc, "dec0", dec0);
      init_center_src = "config.wcs.ra0/dec0";
    } else {
      ra0 = p1_num(wc, "neighbor_ra0", ra0);
      dec0 = p1_num(wc, "neighbor_dec0", dec0);
      init_center_src = "config.wcs.neighbor_ra0/neighbor_dec0(own_solved_product)";
    }
  }
  const std::string frame0 = p1_calibrated_path(doc, doc["input_lights"][0].get<std::string>());
  P1Image im = p1_read_image(frame0);
  if (!im.ok()) {
    (*man)["error_kind"] = "input";
    return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + frame0));
  }
  if (init_source == "header_pointing") {
    std::string why, src;
    if (!p1_header_pointing(im, &ra0, &dec0, &focal_mm, &pixel_um,
                            &s0_arcsec_px, &src, &why)) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "wcs.init_source=header_pointing but " + why +
          " (fail-closed; frame " + frame0 + ")"));
    }
    init_center_src = src;
  }
  if (std::isnan(ra0) || std::isnan(dec0)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "wcs init pointing unavailable for init_source=" + init_source +
        " (ra0/dec0 missing; 禁 silent default)"));
  }
  if (!std::isfinite(focal_mm) || !std::isfinite(pixel_um) ||
      !(focal_mm > 0.0) || !(pixel_um > 0.0)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "wcs init plate scale unavailable for init_source=" + init_source +
        " (FOCALLEN/XPIXSZ invalid; 禁 silent default)"));
  }
  // F-10: 初始指向来源逐帧登记（含中心与 s0）, 便于事后审计
  (*man)["wcs_init_source"] = init_source;
  (*man)["wcs_init_center_src"] = init_center_src;
  (*man)["wcs_init_ra0_deg"] = ra0;
  (*man)["wcs_init_dec0_deg"] = dec0;
  (*man)["wcs_init_s0_arcsec_px"] = s0_arcsec_px;
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
        " (ipv 真实求解器链: 求解失败或解被 parity/尺度合理性闸门拒绝)"));
  }
  // 5) 解算结果 → WcsTan 自检: 次级 roundtrip (<1e-6 px) + B2-A1 绝对
  //    前向交叉门 (<=1e-9 deg, 独立 gnomonic 参考解)
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
  double max_cross_deg = 0.0;
  for (const auto& [x, y] : pts) {
    double ra = 0.0, dec = 0.0, bx = 0.0, by = 0.0;
    wcs.pix2sky(x, y, &ra, &dec);
    wcs.sky2pix(ra, dec, &bx, &by);
    const double rt = std::sqrt((bx - x) * (bx - x) + (by - y) * (by - y));
    if (rt > max_rt) max_rt = rt;
    // B2-A1: 绝对门 (同 explicit 路径; 独立前向参考解)
    double ra_ref = 0.0, dec_ref = 0.0;
    p1_tan_forward_reference(wcs.crpix1, wcs.crpix2, wcs.crval1, wcs.crval2,
                             wcs.cd11, wcs.cd12, wcs.cd21, wcs.cd22,
                             x, y, &ra_ref, &dec_ref);
    const double cross = p1_angular_sep_deg(ra, dec, ra_ref, dec_ref);
    if (cross > max_cross_deg) max_cross_deg = cross;
    samples.push_back(Json{{"x", x}, {"y", y}, {"ra", ra}, {"dec", dec},
                           {"ra_ref", ra_ref}, {"dec_ref", dec_ref},
                           {"forward_cross_deg", cross},
                           {"roundtrip_px", rt}});
  }
  if (max_rt >= 1e-6) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "WcsTan roundtrip " + std::to_string(max_rt) + " px exceeds 1e-6 contract"));
  }
  // B2-A1 绝对正确性门 (阈值依据同 explicit 路径): <=1e-9 deg。
  if (!std::isfinite(max_cross_deg) || max_cross_deg > 1e-9) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "WcsTan forward cross " + std::to_string(max_cross_deg) +
        " deg exceeds 1e-9 absolute contract"));
  }
  const std::string out_path = out_dir + "/p1_wcs.json";
  // B2-A17: 解算器 SIP 系数 (IpvWcsResult sip_a/sip_b/sip_ap/sip_bp, 36 项
  // i*6+j 布局) 落盘到 p1_wcs.json 的 wcs.sip; 无 SIP (order==0) 不写该键,
  // 下游 drizzle 因此走原线性路径 (无畸变产物与基线逐字节等价)。
  P1SipCoeffs sip;
  sip.order = r.sip_order;
  sip.ap_order = r.sip_ap_order;
  for (int k = 0; k < 36; ++k) {
    sip.a[k] = r.sip_a[k]; sip.b[k] = r.sip_b[k];
    sip.ap[k] = r.sip_ap[k]; sip.bp[k] = r.sip_bp[k];
  }
  sip.present = r.sip_order > 0;
  if (r.sip_order < 0 || r.sip_order > 5 || r.sip_ap_order < 0 || r.sip_ap_order > 5)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "solver SIP order out of drizzle contract [0,5] (order=" +
        std::to_string(r.sip_order) + ", ap_order=" + std::to_string(r.sip_ap_order) + ")"));
  Json wcs_obj = Json{{"crpix1", wcs.crpix1}, {"crpix2", wcs.crpix2},
                      {"crval1", wcs.crval1}, {"crval2", wcs.crval2},
                      {"cd11", wcs.cd11}, {"cd12", wcs.cd12},
                      {"cd21", wcs.cd21}, {"cd22", wcs.cd22},
                      {"ctype1", std::string(r.ctype1)},
                      {"ctype2", std::string(r.ctype2)}};
  if (const Json sj = p1_sip_to_json(sip); !sj.is_null()) wcs_obj["sip"] = sj;
  Json wcs_out = Json{{"schema", "DATA-P1-WCS"},
                      {"solver", "ipv_solve_from_memory_with_callback_d"},
                      {"wcs_source", "ipv"},
                      // F-10: 初始化指向来源逐帧登记（header_pointing|config|
                      // neighbor_crval）, 含中心与板尺度 s0, 便于事后审计。
                      {"wcs_init_source", init_source},
                      {"wcs_init_center_src", init_center_src},
                      {"wcs_init_ra0_deg", ra0},
                      {"wcs_init_dec0_deg", dec0},
                      {"wcs_init_s0_arcsec_px", s0_arcsec_px},
                      {"initial", false},
                      {"wcs", wcs_obj},
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
                      {"max_forward_cross_deg", max_cross_deg},
                      {"forward_cross_ref", "p1_tan_forward_reference"},
                      {"samples", samples}};
  if (!p1_write_text(out_path, wcs_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_pairs"] = r.n_pairs;
  (*man)["rms_px"] = r.rms_px;
  (*man)["n_samples"] = samples.size();
  (*man)["max_roundtrip_px"] = max_rt;
  (*man)["max_forward_cross_deg"] = max_cross_deg;
  (*man)["wcs_artifact"] = out_path;
  (*man)["artifacts"] = Json::array({out_path});
  return Result<void>::success();
}

// ── op: measure_flux（唯一真实入口 Photometer::measure; 无源 → 如实空输出）──
Result<void> p1_op_photometry(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  const std::string src_path = out_dir + "/p1_sources.json";
  // ── B2-A16: 上游 DATA-P1-SOURCES 是本节点唯一输入 ─────────────────────
  // 旧实现: 文件打不开/解析失败 → cat=[] → 空循环 → 仍写 p1_flux.json 并
  // success（fail-open）；帧缺失仅记 {"error":"frame not found"} 后继续。
  // 宪章 §14.4 fail-fast / §11 不留貌似成功产品: 上游 artifact 缺失/不可解析
  // 或已被 sources 引用的帧缺失 → DATA 失败（CLI rc=2），不写任何产物。
  Json cat = Json::array();
  {
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::u8path(src_path), ec))
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "p1_sources.json missing (upstream star-psf artifact required): " + src_path));
    std::ifstream f(std::filesystem::u8path(src_path), std::ios::binary);
    if (!f)
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot open: " + src_path));
    try {
      const Json j = Json::parse(std::string((std::istreambuf_iterator<char>(f)),
                                             std::istreambuf_iterator<char>()));
      if (!j.is_object() || !j.contains("frames") || !j["frames"].is_array())
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "p1_sources.json must be an object with a 'frames' array: " + src_path));
      cat = j["frames"];
    } catch (const std::exception& e) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          std::string("p1_sources.json parse failed: ") + e.what()));
    }
  }
  const astrocs::phase1::Photometer phot;
  Json frames = Json::array();
  uint64_t missing_frames = 0;
  std::string first_missing;
  for (const auto& fr : cat) {
    const std::string file = fr.value("file", std::string());
    // sources.json 的 file 是 calibrated 基名; 逐帧读取。
    if (file.empty()) {
      ++missing_frames;
      if (first_missing.empty()) first_missing = "(empty file name)";
      continue;
    }
    std::string path = out_dir + "/" + file;
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::u8path(path), ec)) {
      // B2-A16: 帧缺失按合同计数, 循环后显式上抛（不再静默跳过记 error）。
      ++missing_frames;
      if (first_missing.empty()) first_missing = file;
      continue;
    }
    P1Image im = p1_read_image(path);
    if (!im.ok()) {
      (*man)["error_kind"] = "input";
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + path));
    }
    // ── P10-UTIL2-003 (2026-09-14): 逐源并行化 ─────────────────────────
    // 并行轴 = 源 (work unit = 该帧 sources 数组的一行)。Photometer::measure
    // 是 const 且无共享可变状态 (每源只读 image、只写自己的结果槽), 因此逐源
    // 并行线程安全且**无归约**: 结果按下标写入定长数组, 再按**原顺序**串行
    // 组装 JSON -> 产物与串行逐位一致, 与线程数/调度顺序无关。
    // 失败语义: 原实现遇到首个失败源立即返回其错误; 并行版记录每源失败并按
    // 下标升序扫描, 返回**下标最小**的失败 (与串行首个失败一致)。
    // 调度: dynamic + 小 chunk; work unit 数 = 源数, 与线程数无关。
    const Json empty_sources = Json::array();
    const Json& srcs =
        fr.contains("sources") && fr["sources"].is_array() ? fr["sources"] : empty_sources;
    const std::size_t nsrc = srcs.size();
    std::vector<astrocs::phase1::PhotometryResult> prows(nsrc);
    std::vector<unsigned char> pok(nsrc, 0);
    std::vector<std::string> perr(nsrc);
    std::vector<int> perr_dom(nsrc, 0);
    if (nsrc > 0) {
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic, 8)
#endif
      for (long long i = 0; i < static_cast<long long>(nsrc); ++i) {
        const std::size_t si = static_cast<std::size_t>(i);
        const Json& s = srcs[si];
        const double cx = s.value("x", 0.0), cy = s.value("y", 0.0);
        auto r = phot.measure(im.px(), im.w(), im.h(), cx, cy);
        if (r.failed()) {
          perr[si] = r.error().message();
          perr_dom[si] = static_cast<int>(r.error().domain());
          continue;
        }
        prows[si] = r.value();
        pok[si] = 1;
      }
    }
    Json results = Json::array();
    for (std::size_t si = 0; si < nsrc; ++si) {
      if (!pok[si])
        return Result<void>::fail(
            Error(static_cast<ErrorDomain>(perr_dom[si]), perr[si]));
      const Json& s = srcs[si];
      const double cx = s.value("x", 0.0), cy = s.value("y", 0.0);
      const astrocs::phase1::PhotometryResult& pr = prows[si];
      results.push_back(Json{{"id", s.value("id", std::string())},
                             {"x", cx}, {"y", cy},
                             {"flux", pr.flux}, {"flux_error", pr.flux_error},
                             {"background", pr.background}, {"snr", pr.snr},
                             {"valid", pr.valid}, {"failure_reason", pr.failure_reason}});
    }
    frames.push_back(Json{{"file", file}, {"results", results}});
  }
  if (missing_frames != 0)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        std::to_string(missing_frames) + " frame(s) referenced by p1_sources.json "
        "not found (first: " + first_missing + ")"));
  const std::string out_path = out_dir + "/p1_flux.json";
  Json flux_out = Json{{"schema", "DATA-P1-FLUX"}, {"frames", frames}};
  if (!p1_write_text(out_path, flux_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  // ── B2-A14: 真实测光 provenance sidecar (DATA-P1-PHOTPROV-001) ─────────────
  // 本节点 (measure_flux) 只测量孔径通量, 不对像素施加测光缩放（§02_FROZEN §7
  // I_photo=k_photo·I_cal 由 pc_calibrate/simple 类节点承担），故如实声明
  // photometry_applied=false、photscal=1.0（中性）。drizzle 消费本产物决定
  // PHOTSCAL/PHOTAPPL；禁止再硬编码 1。
  const std::string prov_path = out_dir + "/p1_phot.json";
  const Json prov = Json{{"schema", "DATA-P1-PHOTPROV-001"},
                         {"node", "astrocs.phase1.photometry"},
                         {"operation", "measure_flux"},
                         {"entry", "astrocs_phase1_photometry_v1"},
                         {"photometry_applied", false},
                         {"photscal", 1.0},
                         {"pixel_scaling", "none"},
                         {"n_frames", frames.size()}};
  if (!p1_write_text(prov_path, prov.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_frames"] = frames.size();
  (*man)["flux_artifact"] = out_path;
  (*man)["photometry_provenance_artifact"] = prov_path;
  (*man)["photometry_applied"] = false;
  (*man)["artifacts"] = Json::array({out_path, prov_path});
  return Result<void>::success();
}

// ── op: estimate_snr（唯一真实入口 NoiseModel::estimate; SCI-NOISE-001 公式）──
//
// P8-SNR-LINUX (2026-09-14): 本节点的 SNR 输出改为**逐源科学 SNR**
//   (lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.h -> lib/algorithms/noise_snr/cpp/src/snr_science.cpp
//    唯一权威实现, 已编入 astrocs_phase1_noise):
//     snr_phot == median_snr == median_source_snr == median(SNR_F),
//     SNR_F = F/sigma_F (Horne 1986 最优提取; sigma_F^-2 = sum_i P_i^2/sigma_i^2)。
//   帧级科学基准改为 5sigma 点源深度 frame_depth_flux5_adu / frame_depth_m5_mag;
//   **不再输出任何"整帧 SNR 标量"**。local_snr/frame_snr 按 SCI-CW-001 §2a/§4
//   重定义为相对质量权重场 / 5sigma 深度 (非校准信噪比)。
// 输入: 上游 star-psf 节点产物 p1_sources.json (逐源 flux/fwhm_px + 帧级
//   noise_sigma + psf_params 的 PSF 有效星集合; 与 measure_flux 同源)。
//   上游缺失/不可解析 -> DATA 失败 (fail-fast; 不写貌似成功的 p1_snr.json)。
// 配置 (可选): doc["snr"] = {gain_e_per_adu, read_noise_e, zero_point_mag,
//   aperture_radius_px, n_sky, profile_half_px, sigma_logflux_dex, n_matches,
//   reference_flux_adu}; 缺省 = 未知 gain/ZP (天空受限最优提取, m_5 = NaN)。
Result<void> p1_op_noise(const Json& doc, Json* man) {
  auto p1_lights_rc = p1_require_lights(doc);
  if (p1_lights_rc.failed()) return p1_lights_rc;
  const std::string out_dir = doc.value("output_dir", std::string("."));
  const astrocs::phase1::NoiseModel model;

  // ── P8: 上游 DATA-P1-SOURCES = 逐源 SNR 目录的唯一来源 (fail-fast) ──
  const std::string src_path = out_dir + "/p1_sources.json";
  Json src_frames = Json::array();
  {
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::u8path(src_path), ec))
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "p1_sources.json missing (upstream star-psf artifact required): " + src_path));
    std::ifstream sf(std::filesystem::u8path(src_path), std::ios::binary);
    if (!sf) return Result<void>::fail(Error(ErrorDomain::IO, "cannot open: " + src_path));
    try {
      const Json sj = Json::parse(std::string((std::istreambuf_iterator<char>(sf)),
                                             std::istreambuf_iterator<char>()));
      if (!sj.is_object() || !sj.contains("frames") || !sj["frames"].is_array())
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "p1_sources.json must be an object with a 'frames' array: " + src_path));
      src_frames = sj["frames"];
    } catch (const std::exception& e) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          std::string("p1_sources.json parse failed: ") + e.what()));
    }
  }

  // ── P8: SNR 科学配置 (可选; 缺省 = 无 gain/read-noise/ZP) ──
  // P14-N-08: snr.max_sources = **交付样本上限** (默认 0 = 不限)。这是唯一
  // 允许影响交付 SNR 样本大小的显式开关, 且生效时必须置 truncated=true;
  // 与性能开关 psf.max_stars 完全解耦（后者只影响 PSF 拟合, 不进 SNR 样本）。
  int snr_max_sources = 0;
  astrocs::phase1::SnrFrameScienceConfig sci_cfg;
  if (doc.contains("snr") && doc["snr"].is_object()) {
    const Json& sc = doc["snr"];
    sci_cfg.gain_e_per_adu = sc.value("gain_e_per_adu", 0.0);
    sci_cfg.read_noise_e = sc.value("read_noise_e", 0.0);
    sci_cfg.zero_point_mag = sc.value("zero_point_mag", 0.0);
    sci_cfg.aperture_radius_px = sc.value("aperture_radius_px", 0.0);
    sci_cfg.n_sky = sc.value("n_sky", 0.0);
    sci_cfg.profile_half_px = sc.value("profile_half_px", 0);
    sci_cfg.sigma_logflux_dex = sc.value("sigma_logflux_dex", 0.0);
    sci_cfg.n_matches = sc.value("n_matches", 0);
    sci_cfg.reference_flux_adu = sc.value("reference_flux_adu", 0.0);
    snr_max_sources = sc.value("max_sources", 0);
    if (snr_max_sources < 0) snr_max_sources = 0;   // <0 非法 -> 视为不限
  }

  Json frames = Json::array();
  for (const auto& l : doc["input_lights"]) {
    // cosmetic 下游（cos → psf → phot → snr）: 消费 artifact:cos 产物
    const std::string path = p1_cleaned_input_path(doc, l.get<std::string>());
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
    const std::string base = p1_base_name(path);
    Json frame = Json{{"file", base},
                      {"variance", nr.variance}, {"ivar", nr.ivar},
                      {"sigma", nr.sigma}, {"background", nr.background},
                      {"valid", nr.valid}, {"reason", nr.reason}};
    // ── P8: 逐源科学 SNR (仅当上游目录含同帧时附加) ──
    frame["snr_schema"] = "DATA-P1-SNR/2";
    frame["snr_definition"] =
        "SNR_F = F/sigma_F (Horne 1986 optimal extraction; "
        "sigma_F^-2 = sum_i P_i^2/sigma_i^2); frame-level science benchmark = "
        "5-sigma point-source depth (SCI-CW-001 2a)";
    const Json* src_frame = nullptr;
    for (const auto& fr : src_frames) {
      if (fr.is_object() && fr.value("file", std::string()) == base) {
        src_frame = &fr;
        break;
      }
    }
    if (src_frame == nullptr) {
      frame["snr_catalogue_status"] = "unavailable_no_upstream_frame";
      frame["snr_phot"] = nullptr;
      frame["median_snr"] = nullptr;
      frame["median_source_snr"] = nullptr;
      frame["frame_depth_flux5_adu"] = nullptr;
      frame["frame_depth_m5_mag"] = nullptr;
      frame["sigma_location_se_dex"] = nullptr;
      frame["sigma_location_se_mag"] = nullptr;
      frame["truncated"] = false;
      frame["psf_mode"] = "unavailable";
    } else {
      astrocs::phase1::SnrFrameScienceConfig cfg = sci_cfg;
      cfg.sigma_sky_adu = src_frame->value("noise_sigma", 0.0);
      // ── P14-N-08/N-09 (RQS V2-N-08 + V2-N-09): 交付 SNR 样本真实性 ──────
      // 交付样本 = DATA-P1-SOURCES.sources 的**全部测光有效源** (flux>0 且
      // fwhm_px>0), **不再**取 psf_params —— 后者是受性能开关 psf.max_stars
      // (默认 5000) 截断的「最亮子集」, 会被静默当成 SNR 目录 ⇒ 交付的
      // median_snr / frame_depth_m5_mag 由最亮 ≤5000 颗决定, 系统性偏乐观。
      // 本实现与 psf.max_stars 解耦: 后者不得改变交付的 SNR/深度数值。
      const Json all_sources =
          (src_frame->contains("sources") && (*src_frame)["sources"].is_array())
              ? (*src_frame)["sources"] : Json::array();
      std::vector<astrocs::phase1::SnrSourceRow> rows;
      rows.reserve(all_sources.size());
      for (const auto& s : all_sources) {
        astrocs::phase1::SnrSourceRow row;
        row.id = s.value("id", std::string());
        row.flux_adu = s.value("flux", 0.0);
        row.fwhm_px = s.value("fwhm_px", 0.0);
        // 测光有效判据与 compute_snr_frame_science 内部一致 (flux>0, fwhm>0);
        // 此处先剔除不可计算行, 使 n_sources 如实反映可用样本。
        if (!(std::isfinite(row.flux_adu) && row.flux_adu > 0.0)) continue;
        if (!(std::isfinite(row.fwhm_px) && row.fwhm_px > 0.0)) continue;
        rows.push_back(std::move(row));
      }
      const std::size_t n_snr_available = rows.size();
      // 显式交付样本上限 (snr.max_sources; 默认 0 = 不限): 一旦生效即如实置
      // truncated=true（下游可读）; 默认路径**不截断** ⇒ 样本 = 全量有效源。
      bool snr_sample_truncated = false;
      if (snr_max_sources > 0 &&
          rows.size() > static_cast<std::size_t>(snr_max_sources)) {
        std::partial_sort(
            rows.begin(),
            rows.begin() + static_cast<std::ptrdiff_t>(snr_max_sources),
            rows.end(),
            [](const astrocs::phase1::SnrSourceRow& a,
               const astrocs::phase1::SnrSourceRow& b) {
              if (a.flux_adu != b.flux_adu) return a.flux_adu > b.flux_adu;
              return a.id < b.id;   // tie-break: id 升序 (确定性)
            });
        rows.resize(static_cast<std::size_t>(snr_max_sources));
        std::sort(rows.begin(), rows.end(),
                  [](const astrocs::phase1::SnrSourceRow& a,
                     const astrocs::phase1::SnrSourceRow& b) {
                    return a.id < b.id;
                  });
        snr_sample_truncated = true;
      }
      const astrocs::phase1::SnrFrameScienceResult sci =
          astrocs::phase1::compute_snr_frame_science(rows, cfg);
      frame["snr_catalogue_status"] = sci.valid ? "ok" : "degenerate";
      frame["snr_catalogue_reason"] = sci.reason;
      frame["n_snr_input"] = sci.n_input;
      frame["n_snr_catalogue"] = sci.n_used;
      // ── P14-N-08: 交付样本 provenance（下游可读, 截断不再静默）──────────
      // snr_sample = 所用样本定义; n_sources = 上游可用源总数;
      // truncated = 该交付样本是否被样本上限 (snr.max_sources) 截断;
      // psf_mode/n_fit_input/psf_fit_truncated = 上游 PSF 拟合 provenance
      // （只作如实登记, 不影响本帧 SNR/深度数值）。
      frame["snr_sample"] =
          "all photometrically valid sources from DATA-P1-SOURCES.sources "
          "(flux>0, fwhm_px>0); independent of psf.max_stars / psf_params";
      frame["n_sources"] = static_cast<int64_t>(all_sources.size());
      frame["n_snr_available"] = static_cast<int64_t>(n_snr_available);
      frame["truncated"] = snr_sample_truncated;
      frame["snr_max_sources"] = snr_max_sources;
      frame["psf_mode"] = src_frame->value("psf_mode", std::string("unavailable"));
      frame["n_fit_input"] =
          src_frame->value("n_fit_input", static_cast<int64_t>(-1));
      frame["psf_fit_truncated"] = src_frame->value("psf_fit_truncated", false);
      frame["snr_phot"] = sci.snr_phot;
      frame["median_snr"] = sci.median_snr;
      frame["median_source_snr"] = sci.median_source_snr;
      frame["frame_depth_flux5_adu"] = sci.frame_depth_flux5_adu;
      frame["frame_depth_m5_mag"] = sci.frame_depth_m5_mag;
      frame["sigma_location_se_dex"] = sci.sigma_location_se_dex;
      frame["sigma_location_se_mag"] = sci.sigma_location_se_mag;
      frame["sigma_location_se_status"] =
          (cfg.sigma_logflux_dex > 0.0 && cfg.n_matches > 0)
              ? std::string("ok")
              : std::string("unavailable_no_calibration_residual");
      frame["snr_reference"] = Json{
          {"profile", "median_fwhm_of_catalogue_sky_limited"},
          {"flux_adu", sci.reference_flux_adu},
          {"fwhm_px", sci.reference_fwhm_px},
          {"snr_f", sci.reference_snr_f},
          {"sigma_f_adu", sci.reference_sigma_f_adu}};
      Json vals = Json::array();
      Json sarr = Json::array();
      for (std::size_t i = 0; i < rows.size(); ++i) {
        vals.push_back(sci.local_snr[i]);
        sarr.push_back(Json{{"id", rows[i].id},
                            {"flux_adu", rows[i].flux_adu},
                            {"fwhm_px", rows[i].fwhm_px},
                            {"snr_f", sci.snr_f[i]},
                            {"sigma_f_adu", sci.sigma_f_adu[i]},
                            {"local_snr", sci.local_snr[i]}});
      }
      frame["local_snr"] = Json{
          {"definition",
           "relative quality weight = SNR_F/median(SNR_F) (SCI-CW-001 4 "
           "quality_weight; NOT a calibrated signal-to-noise ratio)"},
          {"units", "1"},
          {"values", vals}};
      frame["frame_snr"] = Json{
          {"definition",
           "5-sigma point-source depth = F_5 [ADU] / m_5 [mag] (SCI-CW-001 2a); "
           "NOT a whole-frame scalar SNR"},
          {"flux5_adu", sci.frame_depth_flux5_adu},
          {"m5_mag", sci.frame_depth_m5_mag},
          {"zero_point_mag", cfg.zero_point_mag}};
      frame["sources"] = sarr;
    }
    frames.push_back(frame);
  }
  const std::string out_path = out_dir + "/p1_snr.json";
  Json snr_out = Json{{"schema", "DATA-P1-SNR"},
                      {"schema_version", "2"},
                      {"frames", frames}};
  if (!p1_write_text(out_path, snr_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_frames"] = frames.size();
  (*man)["snr_schema"] = "DATA-P1-SNR/2";
  (*man)["snr_artifact"] = out_path;
  (*man)["artifacts"] = Json::array({out_path});
  return Result<void>::success();
}

// ── op: drizzle_stack（唯一真实入口 hp_drizzle_run; nside 科学参数无缺省）──
Result<void> p1_op_drizzle(const Json& doc, Json* man) {
  auto p1_lights_rc = p1_require_lights(doc);
  if (p1_lights_rc.failed()) return p1_lights_rc;
  const std::string out_dir = doc.value("output_dir", std::string("."));
  // FIX-E2E B1-A1: header KV 来源 = 上游 wcs 节点产物 p1_wcs.json 透传优先
  // （真实节点产物; 显式配置路径下该产物带 wcs_source=explicit_config），
  // 回退到 config.wcs。两者都无 → DATA fail-closed（禁 silent default）。
  Json wj_storage = Json::object();
  {
    const std::string wcs_prod_path = out_dir + "/p1_wcs.json";
    std::ifstream wf(std::filesystem::u8path(wcs_prod_path), std::ios::binary);
    Json wcs_prod;
    bool have_prod = false;
    if (wf) {
      try {
        wcs_prod = Json::parse(std::string((std::istreambuf_iterator<char>(wf)),
                                           std::istreambuf_iterator<char>()));
        have_prod = true;
      } catch (...) { have_prod = false; }
    }
    if (have_prod && wcs_prod.is_object() && wcs_prod.contains("wcs") &&
        wcs_prod["wcs"].is_object()) {
      wj_storage = wcs_prod["wcs"];
    } else if (p1_has(doc, "wcs") && doc["wcs"].is_object()) {
      wj_storage = doc["wcs"];
    } else {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "drizzle requires upstream p1_wcs.json or config 'wcs' (HP DRIZZLE"
          " header KV source; 禁 silent default)"));
    }
  }
  const Json& wj = wj_storage;
  // B2-A17: 上游/配置 SIP 系数 (p1_wcs.json wcs.sip) 解析 —— 有则下发到 frame
  // header (CTYPE*-SIP + A/B/AP/BP_i_j), 供 hp_drizzle_api.cpp:586-645 读入
  // WcsSip 并逐叶像素施加畸变修正; 无则保持原线性路径 (基线等价)。
  bool sip_ok = true;
  std::string sip_err;
  const P1SipCoeffs sip = p1_parse_sip(wj, &sip_ok, &sip_err);
  if (!sip_ok)
    return Result<void>::fail(Error(ErrorDomain::DATA, "drizzle wcs " + sip_err));
    const std::string ctype1_kv =
        p1_has(wj, "ctype1") && wj["ctype1"].is_string()
            ? wj["ctype1"].get<std::string>()
            : (sip.present ? std::string("RA---TAN-SIP") : std::string("RA---TAN"));
    const std::string ctype2_kv =
        p1_has(wj, "ctype2") && wj["ctype2"].is_string()
            ? wj["ctype2"].get<std::string>()
            : (sip.present ? std::string("DEC--TAN-SIP") : std::string("DEC--TAN"));
  const bool has_drz = p1_has(doc, "drizzle") && doc["drizzle"].is_object();
  if (!has_drz)
    return Result<void>::fail(Error(ErrorDomain::DATA, "drizzle config required"));
  const Json& dj = doc["drizzle"];
  // ── P17-NSIDE: nside 采样率合规 (采样率等价 drizzle 1x-2x, 负责人裁定) ──
  // 语义:
  //  * drizzle.nside 缺省 / null / 0 / "" ⇒ **自动**: 从帧 WCS/SIP 调
  //    hp_drizzle_compute_auto_nside (最细局部输入像素尺度 → 最小 2 次幂 nside
  //    使 hp_res <= finest, 即 1~2× 线性过采样, nside 钳位 [16, 2^22])。
  //    这是**合规默认**, 不是 silent default: 决策依据 (finest/hp_res/过采样倍率)
  //    与 nside_source=auto 全部写入产物与节点 manifest。
  //  * drizzle.nside > 0 ⇒ **显式强制输入 (另当别论)**: 原样使用, 记
  //    nside_source=explicit; 若显式值使 hp_res > finest (欠采样), 则**必须可见**:
  //    高亮 stderr 告警 + nside_conflict=undersampled + 欠采样倍率写产物/manifest
  //    (宪章 §17.6 禁静默降级; 不由本节点静默改写用户显式值)。
  //  * drizzle.nside_mode 若显式给出则优先并校验: "1x_to_2x_drizzle"|"auto" ⇒
  //    自动 (此时 nside 必须缺省/0/空, 否则语义冲突 fail-closed); "explicit" ⇒
  //    必须同时给 nside>0。合同外取值 → DATA fail-closed。
  const bool nside_mode_given = p1_has(dj, "nside_mode");
  std::string nside_mode = "unspecified";  // 未给出 ≠ auto: 由 nside 是否存在决定
  if (nside_mode_given) {
    if (!dj.at("nside_mode").is_string())
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "drizzle.nside_mode must be a string"));
    nside_mode = dj.at("nside_mode").get<std::string>();
    if (nside_mode == "1x_to_2x_drizzle") nside_mode = "auto";
    if (nside_mode != "auto" && nside_mode != "explicit")
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "drizzle.nside_mode must be '1x_to_2x_drizzle'|'auto'|'explicit', got: '"
          + nside_mode + "'"));
  }
  // nside 解析: 缺省/null/0/"" = 自动; 整数 >0 = 显式; 其余类型拒绝 (禁隐式转换)
  int nside = 0;
  bool nside_given = false;
  if (p1_has(dj, "nside")) {
    const Json& nv = dj.at("nside");
    if (nv.is_string()) {
      if (!nv.get<std::string>().empty())
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "drizzle.nside string must be empty (''=auto); non-empty strings rejected"));
    } else if (nv.is_number_unsigned() || nv.is_number_integer()) {
      nside = p1_int(dj, "nside", 0);
      if (nside < 0)
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "drizzle.nside must be >= 0 (0=auto)"));
      nside_given = (nside > 0);
    } else if (nv.is_number_float()) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "drizzle.nside must be an integer (0=auto); float rejected (no truncation)"));
    } else {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "drizzle.nside must be integer|null|'' (empty=auto)"));
    }
  }
  // 冲突/缺参只对"显式给出 mode"成立 (未给出 mode 时 nside 存在 = explicit,
  // 缺省 = auto, 与负责人裁定一致)。
  if (nside_mode == "explicit" && !nside_given)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "drizzle.nside_mode='explicit' requires drizzle.nside > 0"));
  if (nside_mode == "auto" && nside_given)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "drizzle.nside_mode=1x_to_2x_drizzle conflicts with explicit drizzle.nside>0; "
        "omit nside (auto) or set nside_mode='explicit'"));
  const bool auto_nside = (nside_mode == "auto") ||
                          (!nside_mode_given && !nside_given);
  const std::string nside_source = auto_nside ? "auto" : "explicit";
  // B1-A9: HiPS NESTED 合同缺省 nested=1（旧缺省 0 被 drizzle 引擎直接拒绝, 链不可达）。
  const int nested = p1_int(dj, "nested", 1);
  const double pixfrac = p1_num(dj, "pixfrac", 1.0);
  // ── B2-A12: precision_mode 科学精度门（无 silent default）─────────────
  // 宪章 §5.3: Drizzle 采用 float64 累积。B1-A9 已关闭 silent 缺省（缺失即
  // DATA 拒绝，E2E 可达性面）；本动作收紧类型（必须整数 0|1，禁真值/浮点截断
  // 冒充）并把实际累积精度写入产物 provenance（p1_stack.json + 节点 manifest
  // + 帧头 PRECISION KV），禁止再写死 "0"。
  if (!p1_has(dj, "precision_mode"))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "drizzle requires 'drizzle.precision_mode' (0=FP32, 1=FP64;"
        " missing precision_mode is rejected, no silent default)"));
  if (!dj.at("precision_mode").is_number_integer())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "precision_mode must be integer 0 (FP32) or 1 (FP64);"
        " boolean/float/string are rejected (no silent coercion)"));
  const int precision_mode = p1_int(dj, "precision_mode", -1);
  if (nested != 0 && nested != 1)
    return Result<void>::fail(Error(ErrorDomain::DATA, "nested must be 0 or 1"));
  if (precision_mode != 0 && precision_mode != 1)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "precision_mode must be 0 (FP32) or 1 (FP64)"));
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
  // ── B2-A14: PHOTSCAL/PHOTAPPL 由真实测光 provenance 决定（禁硬编码 1）──
  // 上游 p1_phot.json (DATA-P1-PHOTPROV-001) 由 p1_op_photometry (measure_flux) 产出，
  // 声明是否已对像素施加测光缩放。本节点只透传该事实；未执行/未应用测光时
  // PHOTAPPL=0 + PHOTDEGRADE=1（在 drizzle 显式降级为 ADU，绝不伪造
  // RELATIVE_FLUX）。配置标量 photscal 不再是科学输入来源。
  bool photometry_applied = false;
  double photscal = 1.0;
  bool have_phot_prov = false;
  {
    const std::string prov_path = out_dir + "/p1_phot.json";
    std::error_code pec;
    if (std::filesystem::exists(std::filesystem::u8path(prov_path), pec)) {
      std::ifstream pf(std::filesystem::u8path(prov_path), std::ios::binary);
      if (!pf)
        return Result<void>::fail(Error(ErrorDomain::IO, "cannot open: " + prov_path));
      try {
        const Json pj = Json::parse(std::string((std::istreambuf_iterator<char>(pf)),
                                                std::istreambuf_iterator<char>()));
        if (!pj.is_object() || pj.value("schema", std::string()) != "DATA-P1-PHOTPROV-001")
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "p1_phot.json schema mismatch (expect DATA-P1-PHOTPROV-001): " + prov_path));
        photometry_applied = pj.value("photometry_applied", false);
        photscal = pj.value("photscal", 1.0);
        if (!std::isfinite(photscal) || photscal <= 0.0)
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "p1_phot.json photscal must be finite and > 0"));
        have_phot_prov = true;
      } catch (const std::exception& e) {
        return Result<void>::fail(Error(ErrorDomain::DATA,
            std::string("p1_phot.json parse failed: ") + e.what()));
      }
    }
  }
  // PipelineFrame: data [H,W] + header KV（hp_drizzle_run 合同: dims[0]=H, dims[1]=W）。
  // M2a-H-1: data 块 dtype 必须与 drizzle.precision_mode 一致（FP64 请求 →
  // FLOAT64 块，走真 binary64 累加域）；否则元数据声称 FP64 而实际 binary32。
  PipelineFrame* frame = aio_pipeline_frame_create();
  if (!frame) return Result<void>::fail(Error(ErrorDomain::RESOURCE, "frame create failed"));
  const int dims[2] = {im.h(), im.w()};
  const uint64_t n_px = (uint64_t)im.w() * (uint64_t)im.h();
  std::vector<double> px64;
  const void* px_ptr = im.px();
  AioBlockType blk_type = AIO_BLOCK_FLOAT32;
  if (precision_mode == 1) {
    if (aio_get_dtype(im.p) == 1) {
      const double* s = aio_get_pixel_data_f64(im.p);
      px64.assign(s, s + (size_t)n_px);
    } else {
      const float* s = im.px();
      px64.resize((size_t)n_px);
      for (uint64_t i = 0; i < n_px; ++i) px64[i] = (double)s[i];
    }
    px_ptr = px64.data();
    blk_type = AIO_BLOCK_FLOAT64;
  }
  int rc = aio_frame_add_block(frame, "data", blk_type, const_cast<void*>(px_ptr),
                               (int64_t)n_px, dims, 2,
                               precision_mode == 1
                                   ? "p1 drizzle node input plane (FP64)"
                                   : "p1 drizzle node input plane (FP32)");
  if (rc == 0) {
    // 显式一致性自检（无 silent 缺省）：块 dtype 必须匹配 precision_mode。
    const AioBlock* db = aio_frame_get_block(frame, "data");
    const AioBlockType want =
        (precision_mode == 1) ? AIO_BLOCK_FLOAT64 : AIO_BLOCK_FLOAT32;
    if (!db || db->type != want) {
      aio_pipeline_frame_destroy(frame);
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "drizzle precision consistency: data block dtype != precision_mode"));
    }
  }
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
    // B2-A17: CTYPE 由上游 WCS 的 SIP 存在性决定（有 SIP → "*-SIP"）。

    const KV kvs[] = {
        {"CRPIX1", ""}, {"CRPIX2", ""}, {"CRVAL1", ""}, {"CRVAL2", ""},
        {"CD1_1", ""}, {"CD1_2", ""}, {"CD2_1", ""}, {"CD2_2", ""},
        {"CDELT1", ""}, {"CDELT2", ""}, {"CROTA1", "0"}, {"CROTA2", "0"},
        {"CTYPE1", ""}, {"CTYPE2", ""}, {"PRECISION", ""},
        {"PHOTSCAL", ""}, {"PHOTAPPL", ""}, {"PHOTDEGRADE", ""},
    };
    (void)b1; (void)b2; (void)b3; (void)b4; (void)b5; (void)b6; (void)b7; (void)b8;
    // PHOTSCAL: 来自真实测光 provenance（未应用测光时=中性 1.0）；PHOTAPPL 由
    // provenance 决定；PHOTDEGRADE=1 表示本节点显式降级为未测光 ADU（B2-A14）。
    char b9[64];
    fmt(photscal, b9, sizeof(b9));
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
        if (std::strcmp(kv.k, "CTYPE1") == 0) return ctype1_kv;
        if (std::strcmp(kv.k, "CTYPE2") == 0) return ctype2_kv;
        if (std::strcmp(kv.k, "PRECISION") == 0)
          return std::string(precision_mode == 1 ? "fp64" : "fp32");
        if (std::strcmp(kv.k, "PHOTSCAL") == 0) return std::string(b9);
        if (std::strcmp(kv.k, "PHOTAPPL") == 0)
          return std::string(photometry_applied ? "1" : "0");
        if (std::strcmp(kv.k, "PHOTDEGRADE") == 0)
          return std::string(photometry_applied ? "0" : "1");
        return std::string(b8);  // CDELT2
      }();
      if (aio_frame_kv_set(frame, "header", kv.k, val.c_str()) != 0) {
        aio_pipeline_frame_destroy(frame);
        return Result<void>::fail(Error(ErrorDomain::IO,
            std::string("kv_set failed: ") + kv.k));
      }
    }
    // B2-A17: SIP 系数 → frame header (hp_drizzle_api.cpp 读面 A_ORDER/B_ORDER/
    // AP_ORDER/BP_ORDER + A_i_j/B_i_j/AP_i_j/BP_i_j)。无 SIP 时不写任何键。
    {
      std::string sip_hdr_err;
      auto kv_cb = [](void* f, const char* k, const char* v) -> bool {
        return aio_frame_kv_set(static_cast<PipelineFrame*>(f), "header", k, v) == 0;
      };
      if (!p1_sip_write_header_frame(frame, sip, kv_cb, &sip_hdr_err)) {
        aio_pipeline_frame_destroy(frame);
        return Result<void>::fail(Error(ErrorDomain::IO,
            std::string("drizzle frame SIP header: ") + sip_hdr_err));
      }
    }
  }
  if (rc != 0) {
    aio_pipeline_frame_destroy(frame);
    return Result<void>::fail(Error(ErrorDomain::IO, "add data block failed"));
  }
  // ── P17-NSIDE: 最终 nside 决策 + 采样率 provenance/告警 ─────────────────
  // 自动 (nside 缺省/0/空 或 nside_mode=1x_to_2x_drizzle): 必须成功, 否则 DATA
  // fail-closed。显式 (nside>0): 原样使用, 但 best-effort 复核采样率并把任何
  // 欠采样标记为 nside_conflict=undersampled + 高亮 stderr 告警 (禁静默降级)。
  const double HEALPIX_SCALE_PER_NSIDE_ARCSEC =
      std::sqrt(M_PI / 3.0) * (180.0 / M_PI) * 3600.0;  // ≈ 211034.6 "/nside
  HpAutoNsideResult auto_res;
  std::memset(&auto_res, 0, sizeof(auto_res));
  const int auto_rc = hp_drizzle_compute_auto_nside(frame, &auto_res);
  if (auto_nside) {
    if (auto_rc != 0 || auto_res.nside <= 0) {
      aio_pipeline_frame_destroy(frame);
      return Result<void>::fail(Error(ErrorDomain::DATA,
          std::string("auto nside (1x_to_2x_drizzle) failed: ") +
          (auto_res.error_msg[0] ? auto_res.error_msg : "(no detail)")));
    }
    nside = auto_res.nside;
  }
  // 采样率合规复核: oversample_factor = finest_input / hp_res ∈ [1,2) 即合规;
  // < 1 表示输出像素比输入粗 (欠采样)。
  std::string nside_conflict = "unknown";
  double finest_input_arcsec = 0.0, hp_res_arcsec = 0.0, oversample_factor = 0.0;
  if (auto_rc == 0 && auto_res.finest_arcsec > 0.0 && nside > 0) {
    finest_input_arcsec = auto_res.finest_arcsec;
    hp_res_arcsec = HEALPIX_SCALE_PER_NSIDE_ARCSEC / static_cast<double>(nside);
    oversample_factor = finest_input_arcsec / hp_res_arcsec;
    nside_conflict = (hp_res_arcsec > finest_input_arcsec) ? "undersampled" : "none";
  }
  if (nside_conflict == "undersampled") {
    const double under = (finest_input_arcsec > 0.0)
                             ? hp_res_arcsec / finest_input_arcsec : 0.0;
    fprintf(stderr,
        "[drizzle_node][P17-NSIDE][WARN] 显式 drizzle.nside=%d 欠采样: "
        "hp_res=%.4f\" 粗于 finest_input=%.4f\" (欠采样 %.2fx); "
        "合规 1x-2x 应取 nside=%d。本次按用户显式值执行 (nside_source=explicit), "
        "该降级已在 p1_stack.json / manifest 标记 nside_conflict=undersampled。\n",
        nside, hp_res_arcsec, finest_input_arcsec, under, auto_res.nside);
  }
  HpDrizzleResult res;
  std::memset(&res, 0, sizeof(res));
  // P23 一级: 生产末端直写标准 HiPS, 不再落中间容器
  // (hp_drizzle_run_phase1_hips -> write_hips_phase1, 与旧 writer 节点产物
  // 逐字节等价)。观测 passband 身份由 phase_config 提供 (与旧 writer 同源)。
  const std::string filter_passband = doc.value("filter_passband", std::string());
  if (filter_passband.find('\n') != std::string::npos ||
      filter_passband.find('\r') != std::string::npos)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "filter_passband must not contain newline"));
  rc = hp_drizzle_run_phase1_hips(frame, nside, nested, pixfrac, out_dir.c_str(),
                                  filter_passband.c_str(), &res, precision_mode);
  aio_pipeline_frame_destroy(frame);
  if (rc != 0) {
    return Result<void>::fail(Error(ErrorDomain::IO,
        std::string("hp_drizzle_run_phase1_hips failed: ") +
        (res.error_msg[0] ? res.error_msg : "(no detail)")));
  }
  const std::string out_path = out_dir + "/p1_stack.json";
  // P17-NSIDE: 采样率 provenance (nside 来源 + 决策依据 + 合规判定) —— 每个
  // 产物与节点 manifest 都带, 使 1x-2x 合规性与任何显式降级完全可机检。
  const std::string nside_mode_out = auto_nside ? "1x_to_2x_drizzle" : "explicit";
  const int auto_nside_value = (auto_rc == 0) ? auto_res.nside : 0;
  Json stack_out = Json{{"schema", "DATA-P1-STACK"},
                        {"nside", res.nside}, {"nested", res.nested},
                        {"pixfrac", res.pixfrac}, {"precision_mode", precision_mode},
                        {"sip_present", sip.present},
                        {"sip_order", sip.present ? sip.order : 0},
                        {"sip_ap_order", sip.present ? sip.ap_order : 0},
                        {"ctype1", ctype1_kv}, {"ctype2", ctype2_kv},
                        {"nside_source", nside_source},
                        {"nside_mode", nside_mode_out},
                        {"auto_nside", auto_nside_value},
                        {"finest_input_arcsec", finest_input_arcsec},
                        {"hp_res_arcsec", hp_res_arcsec},
                        {"oversample_factor", oversample_factor},
                        {"nside_conflict", nside_conflict},
                        {"nside_clamped", auto_res.clamped != 0},
                        {"n_healpix_pixels", static_cast<int64_t>(res.n_healpix_pixels)},
                        {"n_source_pixels", static_cast<int64_t>(res.n_source_pixels)},
                        {"elapsed_sec", static_cast<double>(res.elapsed_sec)},
                        {"bunit", photometry_applied ? "ASTROCS_RELATIVE_FLUX" : "ADU"},
                        {"photappl", photometry_applied ? 1 : 0},
                        {"photscal", photscal},
                        {"photometry_provenance", have_phot_prov ? "p1_phot.json" : "absent"},
                        {"artifact", "signal/ + support/ (标准 HiPS 树)"},
                        {"entry", "hp_drizzle_run_phase1_hips"}};
  if (!p1_write_text(out_path, stack_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_healpix_pixels"] = static_cast<int64_t>(res.n_healpix_pixels);
  (*man)["stack_artifact"] = out_path;
  (*man)["precision_mode"] = precision_mode;
  (*man)["sip_present"] = sip.present;
  (*man)["sip_order"] = sip.present ? sip.order : 0;
  (*man)["nside"] = res.nside;
  (*man)["nside_source"] = nside_source;
  (*man)["nside_mode"] = nside_mode_out;
  (*man)["auto_nside"] = auto_nside_value;
  (*man)["finest_input_arcsec"] = finest_input_arcsec;
  (*man)["hp_res_arcsec"] = hp_res_arcsec;
  (*man)["oversample_factor"] = oversample_factor;
  (*man)["nside_conflict"] = nside_conflict;
  (*man)["nside_clamped"] = auto_res.clamped != 0;
  (*man)["photometry_applied"] = photometry_applied;
  (*man)["photscal"] = photscal;
  (*man)["photometry_provenance"] = have_phot_prov ? "p1_phot.json" : "absent";
  (*man)["artifacts"] = Json::array({out_path});
  return Result<void>::success();
}

// ── op: write_hips（Phase1 生产末端校验节点）。P23 一级后, 上游
//      drizzle 节点已经由 hp_drizzle_run_phase1_hips 直写标准 HiPS 树
//      (signal/ + support/ = NorderK/DirD/NpixN.fits, Moc.fits, metadata.fits,
//      properties); 本节点不再消费任何中间容器, 只做产物事实面校验并落
//      p1_final.json (逐节点 typed artifact 合同不变)。──
Result<void> p1_op_writer(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  std::error_code ec;
  const std::string props = out_dir + "/signal/properties";
  if (!std::filesystem::exists(std::filesystem::u8path(props), ec)) {
    (*man)["error_kind"] = "input";
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream HiPS product missing: " + props +
        " (writer validates drizzle direct HiPS output)"));
  }
  // 上游 provenance: p1_stack.json (drizzle 节点落盘, 携带 nside 决策依据)
  const std::string stack_json = out_dir + "/p1_stack.json";
  int nside = 0;
  {
    std::ifstream f(std::filesystem::u8path(stack_json), std::ios::binary);
    Json sj;
    try { if (f) f >> sj; } catch (...) { sj = Json::object(); }
    if (sj.is_object()) nside = sj.value("nside", 0);
  }
  // 叶片 Norder = log2(nside) - 9 (标准 512 叶 tile)。只统计叶片 tile, 排除
  // finalize 额外写出的上层 hierarchy NorderK (K < 叶片 order) 汇总 tile。
  int leaf_order = 0;
  for (int n = nside; n > 1; n /= 2) ++leaf_order;
  const int leaf_norder = (nside >= 512) ? leaf_order - 9 : -1;
  // 统计标准 HiPS 事实面 (signal/ 叶片 tile 数 + support/ 一致性)
  int64_t n_tiles_written = 0, n_support_tiles = 0;
  for (const std::string prod : {std::string("signal"), std::string("support")}) {
    const std::string root = out_dir + "/" + prod;
    int64_t c = 0;
    std::error_code it_ec;
    for (std::filesystem::recursive_directory_iterator it(
             std::filesystem::u8path(root), it_ec), end;
         it != end; it.increment(it_ec)) {
      if (!it->is_regular_file(it_ec)) continue;
      const std::filesystem::path p = it->path();
      const std::string fn = p.filename().string();
      if (fn == "Moc.fits" || fn == "metadata.fits" || fn == "properties") continue;
      if (p.extension() != ".fits") continue;
      if (leaf_norder >= 0) {
        const std::string nord =
            p.parent_path().parent_path().filename().string();
        if (nord != ("Norder" + std::to_string(leaf_norder))) continue;
      }
      ++c;
    }
    if (prod == "signal") n_tiles_written = c; else n_support_tiles = c;
  }
  const std::string filter_passband = doc.value("filter_passband", std::string());
  const std::string final_path = out_dir + "/p1_final.json";
  Json final_out = Json{{"schema", "DATA-P1-HIPS"},
                        {"entry", "hp_drizzle_run_phase1_hips"},
                        {"hips_root", out_dir},
                        {"nside", nside},
                        {"tile_nside", 512},
                        {"n_tiles", n_tiles_written},
                        {"n_tiles_written", n_tiles_written},
                        {"n_support_tiles", n_support_tiles},
                        {"products", Json::array({"signal", "support"})},
                        {"filter_passband", filter_passband},
                        {"covered_area_model", "support_ratio_x_A_cell"},
                        {"properties", props}};
  if (!p1_write_text(final_path, final_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_tiles"] = n_tiles_written;
  // P21 复杂度不变量: 聚合已由上游 sink 的 write_hips_phase1 单趟完成 (O(T)),
  // 本节点只做产物计数, 结构上不存在 parent-span 整表扫描。aggregation_* 字段
  // 保留为机器可判定的回归面 (scan_steps ≈ n_tiles << parent_span × n_tiles)。
  int64_t parent_span = 0;
  if (nside >= 512 && leaf_order <= 20)
    parent_span = static_cast<int64_t>(12) *
                  (static_cast<int64_t>(1) << (2 * (leaf_order - 9)));
  (*man)["aggregation_mode"] = "sink_single_pass";
  (*man)["aggregation_parent_span"] = parent_span;
  (*man)["aggregation_parents_visited"] = n_tiles_written;
  (*man)["aggregation_scan_steps"] = n_tiles_written;
  (*man)["hips_root"] = out_dir;
  (*man)["final_artifact"] = final_path;
  (*man)["artifacts"] = Json::array({final_path, props});
  // B2-A10（宪章 §4.3）: 单位/坐标系/观测 passband 随节点 manifest 上报。
  (*man)["bunit"] = "ADU";
  (*man)["coordinate_frame"] = "equatorial";
  (*man)["filter_passband"] = filter_passband;
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
    // FIX-E2E B1-A7: HiPS 输入不可打开 = 输入缺失 → error_kind=input,
    // CLI 退出码映射 rf=3(INPUT) 而非 2(DATA)（CLI_PROTOCOL_V1 §2）。
    (*man)["error_kind"] = "input";
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
  // RESCUE-FD-08(观测链): 恢复 CLI-002 入口迁移后丢失的 session stage 观测证据
  // —— IR 通道的 coverage 节点把实测 union cell 数写 stderr(p2_session 旧通道
  // 的唯一实现已不在生产调用面上)。数值来自真实 p2_coverage_build 结果, 不造假。
  std::fprintf(stderr, "stage coverage ok: cells=%llu\n",
               (unsigned long long)cov.n_union_cells);
  std::fflush(stderr);

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
  (*man)["artifacts"] = Json::array({out_path});
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
                       {"target_order", view.cov.target_order}, {"control_grid_per_tile", sc.control_grid_per_tile},
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
  (*man)["artifacts"] = Json::array({out_path});
  (*man)["n_obs"] = n_obs;
  (*man)["n_controls"] = n_controls;
  // B1-A2/A10: session 摘要需 n_inputs（帧数）——coverage 有而 sample manifest 缺，
  // cmd_phase2_run 摘要扫描命中 sample（含 n_obs）后读不到 n_inputs 会报 0。
  (*man)["n_inputs"] = static_cast<uint64_t>(view.hips_paths.size());
  (*man)["overlap_controls"] = stats.overlap_controls;
  // RESCUE-FD-08(观测链): 同 coverage —— 恢复 sample 阶段实测统计的 stderr 证据
  // (obs/overlap_controls 均来自真实 p2_sample_controls_cached 结果)。
  std::fprintf(stderr, "stage sample ok: obs=%llu overlap_controls=%llu\n",
               (unsigned long long)n_obs,
               (unsigned long long)stats.overlap_controls);
  std::fflush(stderr);
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
  uc.sigma_floor = 1e-3; uc.zero_anchor_weight = 1e-3; uc.grid = smp_doc.value("control_grid_per_tile", 8);  // SCI-UPM-001 §9a:133; M7-C-001 G
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
  // M4-C-02: 与 stage2_common 对称的显式覆盖面；缺省保持 SCI §9a:133 λ0=1e-3。
  if (upm_cfg.contains("zero_anchor_weight"))
    uc.zero_anchor_weight = upm_cfg["zero_anchor_weight"].get<double>();

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
  // A4: upm_save_path/persist_upm 是已登记 session 键（p2_session 语义）; 正式
  // 节点链为唯一写者, 故在此按其语义把模型落盘到指定路径（键可达, 非 silent 忽略）。
  Json upm_arts = Json::array({out_path, bin_path});
  if (doc.value("persist_upm", false) && doc.contains("upm_save_path") &&
      doc["upm_save_path"].is_string() && !doc["upm_save_path"].get<std::string>().empty()) {
    const std::string save_path = doc["upm_save_path"].get<std::string>();
    std::error_code cec;
    std::filesystem::copy_file(std::filesystem::u8path(bin_path),
                               std::filesystem::u8path(save_path),
                               std::filesystem::copy_options::overwrite_existing, cec);
    if (cec)
      return Result<void>::fail(Error(ErrorDomain::IO,
          "upm_save_path copy failed: " + save_path + " (" + cec.message() + ")"));
    upm_arts.push_back(save_path);
  }
  (*man)["artifacts"] = upm_arts;
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
  Json cor_arts = Json::array({out_path});
  for (const auto& fr : frames_j) cor_arts.push_back(fr.value("data_file", std::string()));
  (*man)["artifacts"] = cor_arts;
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
  // frame_idx = corrected manifest 帧序（稳定帧身份; 掩码/帧 slot 一致性校验
  // 的权威, 禁以指针距离/compact 下标反推）
  struct TileRef { uint64_t tile_ipix; size_t frame_idx; uint64_t offset; };
  std::map<uint64_t, std::vector<TileRef>> union_tiles;   // ipix → 每帧 ref
  for (size_t f = 0; f < frames.size(); ++f) {
    for (const auto& t : frames[f]["tiles"]) {
      const uint64_t tip = t.value("tile_ipix", 0ull);
      const uint64_t off = t.value("offset", 0ull);
      union_tiles[tip].push_back(TileRef{tip, f, off});
    }
  }
  std::vector<uint8_t> accepted_bin;
  std::vector<uint16_t> nrej_bin, cand_u16;
  // [F-P2-002-02 / B2-A3] 逐样本接受掩码持久化（tile 序拼接; 每 tile
  // depth×tile_span 字节, 索引 [s*tile_span+p], s=原始帧 slot）。像素级
  // accepted(u8) 无法表达部分拒绝像素内逐样本的接受/拒绝; §30.2 完备划分
  // （n_ineligible = depth − nused − nrej）要求 integrate 按原始样本索引
  // 逐样本剔除（01_SCIENCE_AUTHORITY_BASELINE §4: 拒绝掩码按原始样本索引传递）。
  std::vector<uint8_t> sample_mask;
  uint64_t acc_total = 0, rej_low_total = 0, rej_high_total = 0, undet_total = 0;
  uint64_t n_pixels_processed = 0, rej_samples_total = 0;
  Json tiles_j = Json::array();
  uint64_t out_offset = 0, mask_offset = 0;
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
        if (!p2_read_bin_range<double>(frames[ref.frame_idx].value("data_file", ""),
                                       ref.offset, tile_span, &scratch))
          return Result<void>::fail(Error(ErrorDomain::IO,
              "corrected bin read failed (tile " + std::to_string(ref.tile_ipix) +
              " frame slot " + std::to_string(d) + ")"));
        std::memcpy(frame_major.data() + d * tile_span, scratch.data(),
                    tile_span * sizeof(double));
      }
    }
    const uint64_t base = out_offset;
    // [F-P2-002-02 / B2-A3] 该 tile 的逐样本掩码块（[s*tile_span+p], 帧 slot 序
    // 与 refs 同序）。默认 0 = 未入栈/未接受; kernel 逐样本 reason 只映射到
    // eligible 样本的原始 slot（src_idx 权威, compact→original）。
    std::vector<uint8_t> tile_mask(
        static_cast<size_t>(depth) * static_cast<size_t>(tile_span), 0);
    std::vector<uint32_t> frame_slots(depth, 0);
    for (size_t d = 0; d < depth; ++d)
      frame_slots[d] = static_cast<uint32_t>(refs[d].frame_idx);
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
        // UNDERDETERMINED} 视为接受; rejected_low/high 只认 threshold 侧计数。
        // [F-P2-002-02 / B2-A3] 逐样本掩码必须落在**原始帧 slot**（src_idx
        // 为 eligible→original 映射; rejection.h:252-255 合同禁用 compact
        // index 反推）。像素级 acc 仅作冗余投影, 不再是 integrate 的资格权威。
        acc = 0;
        for (uint32_t s = 0; s < eligible_count; ++s) {
          const bool ok_s = (reasons[s] == P2_REASON_ACCEPTED ||
                             reasons[s] == P2_REASON_UNDERDETERMINED);
          if (ok_s) acc = 1;
          else ++rej_samples_total;
          const uint32_t slot_s = src_idx[s];
          if (slot_s < depth)
            tile_mask[static_cast<size_t>(slot_s) * tile_span + p] = ok_s ? 1 : 0;
        }
        nrej = static_cast<uint16_t>(dec.rejected_low + dec.rejected_high);
        rej_low_total += dec.rejected_low;
        rej_high_total += dec.rejected_high;
      } else {
        // 候选不足/空栈 → UNDERDETERMINED（全接受并记录, 禁偷换算法）:
        // 逐样本掩码对全部 eligible 样本置 1（与 accepted_bin=1 同语义）;
        // eligible_count==0（无资格样本）→ 掩码块全 0（无样本可入栈）。
        for (uint32_t s = 0; s < eligible_count; ++s) {
          const uint32_t slot_s = src_idx[s];
          if (slot_s < depth)
            tile_mask[static_cast<size_t>(slot_s) * tile_span + p] = 1;
        }
      }
      accepted_bin.push_back(acc);
      nrej_bin.push_back(nrej);
      cand_u16.push_back(cand);
      acc_total += acc;
      if (cand > 0 && (cand <= plan.underdetermined_n ||
                       cand < static_cast<uint32_t>(plan.minimum_n))) ++undet_total;
      ++n_pixels_processed;
    }
    sample_mask.insert(sample_mask.end(), tile_mask.begin(), tile_mask.end());
    // [F-P2-002-02 / B2-A3] tile 记录承载逐样本掩码定位三键: depth（该 tile
    // 覆盖帧数）、frame_slots（掩码 slot d ↔ corrected 帧索引）、
    // sample_mask_offset（掩码块在 p2_rejection_sample_mask.bin 的字节偏移）。
    tiles_j.push_back(Json{{"tile_ipix", tip},
                           {"n_pixels", tile_span},
                           {"offset", base},
                           {"depth", depth},
                           {"frame_slots", frame_slots},
                           {"sample_mask_offset", mask_offset}});
    out_offset += tile_span;
    mask_offset += static_cast<uint64_t>(tile_mask.size());
  }

  const std::string acc_file = out_dir + "/p2_rejection_accepted.bin";
  const std::string nrej_file = out_dir + "/p2_rejection_nrej.bin";
  const std::string cand_file = out_dir + "/p2_rejection_candidates.bin";
  // [F-P2-002-02 / B2-A3] 逐样本掩码落盘（§30.2 完备划分的唯一可判据载体;
  // 缺失/错位时 integrate fail-closed, 禁退化为像素级 accepted）。
  const std::string mask_file = out_dir + "/p2_rejection_sample_mask.bin";
  if (!p2_write_bin(acc_file, accepted_bin) || !p2_write_bin(nrej_file, nrej_bin) ||
      !p2_write_bin(cand_file, cand_u16) || !p2_write_bin(mask_file, sample_mask))
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
                                      {"rejected_samples", rej_samples_total},
                                      {"underdetermined_pixels", undet_total}}},
                       {"files", Json{{"accepted", acc_file},
                                      {"nrej", nrej_file},
                                      {"candidates", cand_file},
                                      {"sample_mask", mask_file}}}};
  if (!p2_write_text(out_path, artifact.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed: " + out_path));
  (*man)["artifacts"] = Json::array({out_path, acc_file, nrej_file, cand_file, mask_file});
  (*man)["rejection_artifact"] = out_path;
  (*man)["sample_mask"] = mask_file;
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

  // [F-P2-002-02 / B2-A3] 逐样本接受掩码（§30.2 完备划分的唯一权威面）:
  // 缺失即 fail-closed（禁退化为像素级 accepted —— 那正是部分拒绝像素
  // nused+nrej>depth、n_ineligible<0 的根因）。整文件按字节读入, tile 定位
  // 由 tiles[].sample_mask_offset 提供（不依赖文件总长推断）。
  const std::string smask_file = rej_doc["files"].value("sample_mask", "");
  if (smask_file.empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "rejection artifact missing per-sample acceptance mask"
        " (files.sample_mask; DATA-P2-REJ §30.2: integrate must drop"
        " kernel-rejected samples per original sample slot, not per pixel)"));
  std::vector<uint8_t> sample_mask_all;
  {
    std::error_code ec;
    const auto msz = std::filesystem::file_size(std::filesystem::u8path(smask_file), ec);
    if (ec || msz == 0 ||
        !p2_read_bin_range<uint8_t>(smask_file, 0,
                                    static_cast<uint64_t>(msz), &sample_mask_all))
      return Result<void>::fail(Error(ErrorDomain::IO,
          "rejection sample mask read failed: " + smask_file));
  }

  std::vector<double> sig_bin, sup_bin, wsum_bin;
  std::vector<int32_t> nused_bin, nrej_plane;
  Json tiles_j = Json::array();
  uint64_t zero_weight_pixels = 0, invalid_pixels = 0, nrej_total = 0;
  uint64_t nrej_pix_cursor = 0;
  uint64_t sample_rejected_skipped = 0;   // 因 kernel 逐样本拒绝而剔除的样本实例数
  // 掩码块在文件中必须**按 tile 序连续无洞**（reject 以 union tile 升序拼接）:
  // 游标核对使任何 offset 错位/重叠/空洞立即被检出（禁信任可自洽的伪造 offset）。
  uint64_t sm_cursor = 0;
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
    // [F-P2-002-02 / B2-A3] 该 tile 逐样本掩码定位/序校验（尺寸/depth/帧 slot
    // 三重一致, 任一不符 fail-closed；禁按文件长度/compact 下标猜测布局）。
    // reject 与 integrate 的 slot 均为 corrected 帧升序（cor_index 同源）。
    const uint64_t sm_off = rt.value("sample_mask_offset", ~0ull);
    const uint64_t sm_depth = rt.value("depth", ~0ull);
    if (sm_off == ~0ull || sm_off != sm_cursor || sm_depth != depth ||
        sm_off + depth * tile_span > sample_mask_all.size())
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "rejection sample mask layout mismatch (tile " + std::to_string(tip) +
          "): reject depth=" + std::to_string(sm_depth) + " integrate depth=" +
          std::to_string(depth) + " offset=" + std::to_string(sm_off) +
          " expected_offset=" + std::to_string(sm_cursor) +
          " mask_size=" + std::to_string(sample_mask_all.size())));
    if (!rt.contains("frame_slots") || !rt["frame_slots"].is_array() ||
        rt["frame_slots"].size() != depth)
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "rejection sample mask missing frame_slots (tile " +
          std::to_string(tip) + ")"));
    for (size_t d = 0; d < depth; ++d) {
      const uint64_t fs = rt["frame_slots"][d].get<uint64_t>();
      if (fs != static_cast<uint64_t>(slot[d]))
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "rejection sample mask frame-slot order mismatch (tile " +
            std::to_string(tip) + " d=" + std::to_string(d) +
            " reject_slot=" + std::to_string(fs) + " integrate_slot=" +
            std::to_string(slot[d]) + ")"));
    }
    sm_cursor += depth * tile_span;
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
        // [F-P2-002-02 / B2-A3] 逐样本接受权威: kernel reason 掩码（索引
        // [d*tile_span+p], d=原始帧 slot）。像素级 acc 仅作冗余守卫, 不再是
        // 资格权威; 非 0/1 掩码值 → 产品损坏 fail-closed（禁 clamp/推断）。
        const uint8_t sm =
            sample_mask_all[static_cast<size_t>(sm_off + d * tile_span + p)];
        if (sm > 1)
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "rejection sample mask must be 0/1 (tile " + std::to_string(tip) +
              " frame " + std::to_string(slot[d]) + " pixel " + std::to_string(p) +
              " value=" + std::to_string(static_cast<int>(sm)) + ")"));
        // 调用方资格（SCI-INT §5 valid ∧ W>0 面）: finite ∧ support>0 ∧
        // 逐样本 accepted; 先资格过滤后权重面 —— 无覆盖像素（support=0 →
        // corrected NaN → 过滤）不进入权重检查（ivar 产品在无覆盖像素 =
        // NaN 同态, §30.1 表注 F-UNC-001）
        if (!std::isfinite(v) || !std::isfinite(sp) || sp <= 0.0 || !acc ||
            sm == 0) {
          if (sm == 0 && std::isfinite(v) && std::isfinite(sp) && sp > 0.0 && acc)
            ++sample_rejected_skipped;
          continue;
        }
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
  // [F-P2-002-02 / B2-A3] 掩码文件必须被 tile 块恰好铺满（无尾随/截断/空洞）:
  // 与游标核对共同保证"逐样本掩码与 reject 产物同源同序"。
  if (sm_cursor != sample_mask_all.size())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "rejection sample mask size mismatch: consumed=" +
        std::to_string(sm_cursor) + " file_size=" +
        std::to_string(sample_mask_all.size())));

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
                       {"sample_mask_consumed", true},
                       {"diagnostics", Json{{"zero_valid_weight_pixels", zero_weight_pixels},
                                            {"nonfinite_result_pixels", invalid_pixels},
                                            {"nrej_total", nrej_total},
                                            {"rejected_samples_skipped",
                                             sample_rejected_skipped}}},
                       {"files", Json{{"signal", sig_file},
                                      {"support", sup_file},
                                      {"wsum", wsum_file},
                                      {"nused", nused_file},
                                      {"nrej", nrej_file_out}}}};
  if (!p2_write_text(out_path, artifact.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed: " + out_path));
  (*man)["artifacts"] = Json::array({out_path, sig_file, sup_file, wsum_file,
                                     nused_file, nrej_file_out});
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
      // B2-A8: coverage union 已验证全帧 filter 身份（含显式空声明），mosaic
      // 恒透传该身份；properties 写侧恒写 obs_filter 键（空值也是声明）。
      obs_filter.c_str(),
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
  (*man)["artifacts"] = Json::array({out_path, props});
  (*man)["mosaic_root"] = out_dir;
  (*man)["final_artifact"] = out_path;
  (*man)["n_tiles_written"] = n_tiles_written;
  (*man)["uncertainty_available"] = uncertainty_available;
  // B2-A10（宪章 §4.3）: 单位/坐标系/输入产品哈希随节点 manifest 上报，
  // 供 run manifest provenance 汇总（Phase2 mosaic 单位 = ADU，
  // 坐标系 = equatorial，与 coverage.h / DATA-HIPS-SIGNAL-001 合同一致）。
  (*man)["bunit"] = "ADU";
  (*man)["coordinate_frame"] = "equatorial";
  (*man)["input_manifest_hash"] = manifest_hash;
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

  // P10-UTIL2-005: 声明**真实**工作量 / 并行轴 / worker 需求（取代 runtime 对所有
  // 节点填死的 (1, budget) 占位）。声明只表达"需求量"，不含任何具体线程数（§10.4）：
  //   - cal/cos/star-psf/photometry/noise-snr/wcs/drizzle: 帧内逐像素/逐源/逐块可
  //     并行，需求 = 可用预算（max_workers=0 -> runtime 按 budget 回退）；
  //   - writer: 产物为串行 JSON + HiPS 写盘（I/O），真实需求 = 1。
  Result<ModulePlan> plan(const std::string& node_id,
                          const std::string& config_json) override {
    config_ = config_json;
    ModulePlan p;
    p.node_id = node_id;
    p.cpu_heavy = desc_.execution_class == "cpu_heavy";
    // 工作量 = 输入帧数（config.input_lights），最少 1（config 已在 validate 校验）。
    uint64_t n_frames = 1;
    try {
      const Json doc = Json::parse(config_json);
      if (doc.is_object() && doc.contains("input_lights") &&
          doc["input_lights"].is_array() && !doc["input_lights"].empty())
        n_frames = static_cast<uint64_t>(doc["input_lights"].size());
    } catch (...) {
      n_frames = 1;  // plan 不因 config 解析失败而 fail（validate 已先行）
    }
    switch (spec_.op) {
      case P1NodeOp::Calibrate:
      case P1NodeOp::Cosmetic:
        p.parallel_axes = {"pixel"};
        break;
      case P1NodeOp::StarPsf:
        p.parallel_axes = {"pixel", "source"};
        break;
      case P1NodeOp::Photometry:
        p.parallel_axes = {"source"};
        break;
      case P1NodeOp::NoiseSnr:
        p.parallel_axes = {"pixel", "source"};
        break;
      case P1NodeOp::WcsSolve:
        p.parallel_axes = {"source", "triangle"};
        break;
      case P1NodeOp::Drizzle:
        p.parallel_axes = {"tile", "row-band"};
        break;
      case P1NodeOp::Writer:
        p.parallel_axes = {"io"};
        break;
    }
    p.work_units = n_frames;
    p.min_workers = 1;
    // writer 是 I/O 串行写盘：显式声明 1（不为它占用整份预算）。
    p.max_workers = (spec_.op == P1NodeOp::Writer) ? 1u : 0u;
    return Result<ModulePlan>::ok(std::move(p));
  }

  Result<void> execute(RunContext& ctx) override {
    // P10-UTIL2-006: 节点真实执行窗口观测 (env-gated; 默认零开销)。
    const double p10_node_t0 = p10_monotonic_s();
    P10NodeTraceGuard p10_node_guard{desc_.module_id.c_str(), p10_node_t0};
    if (std::getenv("ASTROCS_NODE_TRACE"))
      std::fprintf(stderr, "[nodetrace] BEGIN %s %.6f\n", desc_.module_id.c_str(),
                   p10_node_t0);
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
    // P7-UTIL-001: 作用域内注入节点租约并行度, 退出 (含异常/取消) 自动恢复 ——
    // 不再把降级节点的 cap 永久写进调度线程的进程 ICV。
    trace_node_lease(desc_.module_id.c_str(), host_workers, lease.acquired(), cap,
                     ctx.budget() ? ctx.budget()->available() : 0u);
    ScopedOmpWorkerInjection omp_worker_injection(
        static_cast<int>(hs.host.budget.max_workers));
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
                    {"status", "running"},
                    // B2-A10（宪章 §4.3）: 每个真实节点自报 algorithm ID / module
                    // build ID / provider；run manifest provenance 由此汇总，
                    // 不在 CLI 侧造占位。
                    {"algorithm_id", desc_.alg_id},
                    {"module_build_id", desc_.module_id + "@" + ASTROCS_VERSION_STRING},
                    {"provider", "baseline"}};
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
    // P7-UTIL-001: 作用域内注入节点租约并行度, 退出 (含异常/取消) 自动恢复 ——
    // 不再把降级节点的 cap 永久写进调度线程的进程 ICV。
    trace_node_lease(desc_.module_id.c_str(), host_workers, lease.acquired(), cap,
                     ctx.budget() ? ctx.budget()->available() : 0u);
    ScopedOmpWorkerInjection omp_worker_injection(
        static_cast<int>(hs.host.budget.max_workers));
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
                    {"status", "running"},
                    // B2-A10（宪章 §4.3）: 节点自报 algorithm ID / module build ID /
                    // provider；run manifest provenance 由此汇总。
                    {"algorithm_id", desc_.alg_id},
                    {"module_build_id", desc_.module_id + "@" + ASTROCS_VERSION_STRING},
                    {"provider", "baseline"}};
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
  // B2-A4/A5: 请求层字段 (缺省 = p3_session parse_request 冻结值)
  std::string projection = "TAN";
  std::string frame = "icrs";
  std::string coverage_output = "mask";
};

// B2-A4/A5: 请求层 projection/frame/coverage_output 的类型 + 值域校验。
// 唯一语义源 = astrocs::phase3::p3_wcs_validate_request (CLI 配置面与节点面共用);
// 未实现/未注册投影在此显式拒绝, 绝不放行也不静默改写为 TAN。
bool p3n_check_request_fields(const Json& doc, std::string* err) {
  auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
  for (const char* k : {"projection", "frame", "coverage_output"}) {
    if (doc.contains(k) && !doc[k].is_string())
      return fail(std::string(k) + " must be string");
  }
  const std::string proj = doc.value("projection", std::string("TAN"));
  const std::string frame = doc.value("frame", std::string("icrs"));
  const std::string cov = doc.value("coverage_output", std::string("mask"));
  std::string why;
  const astrocs::phase3::P3WcsStatus st = astrocs::phase3::p3_wcs_validate_request(
      doc.contains("projection") ? proj.c_str() : nullptr,
      doc.contains("frame") ? frame.c_str() : nullptr,
      doc.contains("coverage_output") ? cov.c_str() : nullptr, &why);
  if (st != astrocs::phase3::P3_WCS_OK) return fail(why);
  return true;
}

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
  // B2-A4/A5: 投影/帧/覆盖率先于数值面显式校验 (与 p3_session parse_request 同序)
  if (!p3n_check_request_fields(doc, err)) return false;
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
  g->projection = doc.value("projection", std::string("TAN"));
  g->frame = doc.value("frame", std::string("icrs"));
  g->coverage_output = doc.value("coverage_output", std::string("mask"));
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

// B2-A10: 输入 HiPS 产品清单哈希（宪章 §4.3「输入产品哈希」）。
// 组成 = signal/properties 与 signal/Moc.fits 的字节（有 Moc.fits 时）。
// 两者共同定义该产品声明的帧身份与几何覆盖，是该产品被消费时的身份锚；
// 大图不参与（tile 内容由各自 FITS CHECKSUM 保护，本哈希只需标识产品声明面）。
std::string p3n_input_manifest_hash(const std::string& hips_dir) {
  std::string blob;
  const char* parts[] = {"/signal/properties", "/signal/Moc.fits"};
  for (const char* rel : parts) {
    std::ifstream f(hips_dir + rel, std::ios::binary);
    if (!f) continue;
    std::string s((std::istreambuf_iterator<char>(f)),
                  std::istreambuf_iterator<char>());
    if (s.empty()) continue;
    blob += rel;
    blob += '\n';
    blob.append(s);
    blob += '\n';
  }
  if (blob.empty()) return std::string();
  return astrocs::crypto::sha256_hex(blob.data(), blob.size());
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
  // B2-A4: 上游 wcs_plan 的投影只允许已实现值 (篡改/漂移 → DATA fail-closed,
  // 不得静默按 TAN 消费)。
  {
    if (j.contains("projection") && !j["projection"].is_string())
      return fail("wcs_plan projection must be string");
    const std::string pj = j.value("projection", std::string("TAN"));
    std::string why;
    if (astrocs::phase3::p3_wcs_validate_request(
            j.contains("projection") ? pj.c_str() : nullptr, nullptr, nullptr,
            &why) != astrocs::phase3::P3_WCS_OK)
      return fail("wcs_plan: " + why);
  }
  d->projection = "TAN";   // 校验通过后携带的字面量 (仅 TAN 实现)
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
  (*man)["artifacts"] = Json::array({path});
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
                                      g.parity.c_str(), 0.0, &wcs,
                                      g.projection.c_str());
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
            {"projection", wcs.projection},
            {"fits_keywords", p3_wcs_fits_keywords(&wcs)}};
  std::ofstream f(path, std::ios::binary);
  if (!f) return Result<void>::fail(Error(ErrorDomain::IO, "cannot write p3_wcs.json"));
  f << plan.dump(2) << "\n";
  f.close();
  if (!f.good()) return Result<void>::fail(Error(ErrorDomain::IO, "p3_wcs.json write failed"));
  (*man)["wcs_plan_artifact"] = path;
  (*man)["artifacts"] = Json::array({path});
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
    // P30: 修复前 max_tiles 只设到主 sampler; 每个行带 worker 新建的 sampler
    // 回落默认 cap=8 (工作集 523 tile 时逐行抖动重复解码)。此处同样约束
    // uncertainty sampler (它有自己的缓存, 键同为 tile ipix, 禁与 signal 共享)。
    p3_sampler_set_max_tiles(&u_samp, (int)std::max<int64_t>(1, mt));
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
  std::atomic<bool> cancelled{false};    // P30: 协作取消 (行带循环安全点)
  const int npts = (g.sampler == "nearest") ? 1 : 4;

  auto worker = [&](int y0, int y1) {
    P3Sampler w_samp{};
    std::string wserr;
    if (p3_sampler_open_ex(g.hips_dir.c_str(), &w_samp, nullptr, nullptr, &wserr) !=
        P3_RS_OK)
      return;
    // P30: 与本节点唯一的有界 tile 缓存共享 (容量 = max_tiles 总量, 与 worker
    // 数无关 → 内存有界; 同一 tile 全节点只解码一次)。只读数据 + 缺失负缓存,
    // 不改变任何像素值; 每个 worker 仍各有独立 AioHipsDataset/句柄。
    p3_sampler_attach_cache(&w_samp, &samp);
    P3Sampler w_u{};
    P3UncertaintySource w_src = P3_UNC_NONE;
    if (unc_available &&
        p3_uncertainty_open(g.hips_dir.c_str(), input_order, &w_src, &w_u) != P3_RS_OK) {
      p3_sampler_close(&w_samp);
      corrupt.store(-2);
      return;
    }
    if (unc_available) p3_sampler_attach_cache(&w_u, &u_samp);   // P30: 同上
    P3WcsDescriptor w_wcs = wcs;
    for (int y = y0; y < y1 && corrupt.load() == -1; ++y) {
      // P30: 协作取消安全点 (与 p3_session 路径同款语义)。此前本循环不检查
      // 取消位 → SIGINT/SIGTERM 无法中止运行中的 phase3 重采样 (实测只能
      // SIGKILL); 现在每个输出行检查一次, 取消后立即置位并跳出, 由下方统一
      // fail-closed 返回, **不落任何半成品** (bin/fits 均在行带循环之后才写)。
      if (ctx && ctx->cancelled()) { cancelled.store(true); break; }
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
  // P30: 取消优先于"行带被丢弃"判定 (两者都 fail-closed; 取消更具体)。
  // 取消时 p3_resampled.bin/json 尚未写出 → 无半成品产物。
  if (cancelled.load() || (ctx && ctx->cancelled())) {
    return Result<void>::fail(Error(ErrorDomain::CANCELLED,
        "resample cancelled by cancellation token: fail-closed, no partial product"));
  }
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
    // P30 (§10.5 证据): tile 缓存命中率 / 真实 tile 读次数 / 失败 open 次数。
    P3CacheStats cs{};
    p3_sampler_cache_stats(&samp, &cs);
    (*man)["tile_cache"] = Json{{"cap_tiles", cs.cap_tiles},
                                {"resident_tiles", cs.resident_tiles},
                                {"hits", cs.hits},
                                {"misses", cs.misses},
                                {"absent_reads", cs.absent_reads},
                                {"evictions", cs.evictions}};
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
  (*man)["artifacts"] = Json::array({json_path, bin_path});
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
  // B2-A10（宪章 §4.3）：provenance 必须携带真实来源，而非节点占位串。
  //   ① manifest_hash = 实际消费的输入 HiPS 产品清单哈希（properties +
  //      Moc.fits 的 SHA-256；P3 可读任意合同兼容 HiPS，故按实际源计算）；
  //   ② run_id / software_version = CLI 运行上下文（run_context.json，由
  //      cmd_phaseN_run 在会话启动前写入 out_dir）。上下文缺失 = 显式 IO 失败
  //      （fail-closed），不再静默写 "p3-node"/"astrocs-phase3-node" 占位。
  const std::string input_manifest_hash = p3n_input_manifest_hash(g.hips_dir);
  Json run_ctx;
  if (!p3n_read_json(g.out_dir + "/run_context.json", &run_ctx, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "run context missing (required for §4.3 provenance): " + err));
  const std::string run_id_str = run_ctx.value("run_id", std::string());
  const std::string version_str = run_ctx.value("software_version", std::string());
  const std::string source_sha_str = run_ctx.value("source_sha", std::string());
  if (run_id_str.empty() || version_str.empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "run_context.json must carry non-empty run_id/software_version (§4.3)"));
  if (input_manifest_hash.empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "input HiPS manifest hash unavailable: " + g.hips_dir));
  P3Provenance prov{};
  prov.hips_id = "ivo://astrocs/phase3";
  prov.manifest_hash = input_manifest_hash.c_str();
  prov.missing_tiles = nullptr;
  prov.missing_count = 0;
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
           (long long)res.value("uncertainty_missing_pixels", 0ll)},
          // B2-A10（宪章 §4.3）: 真实 provenance（非占位）随节点 manifest 落盘。
          {"run_id", run_id_str},
          {"software_version", version_str},
          {"input_manifest_hash", input_manifest_hash},
          {"hips_id", std::string(prov.hips_id)},
          {"source_sha", source_sha_str},
          {"product_sha256", std::string(ores.sha256)},
          {"coordinate_frame", "icrs"},
          {"bunit", res.value("bunit", "ADU")},
          {"algorithm_id", "ALG-P3-004"},
          {"provider", "baseline"}};
  std::ofstream f(json_path, std::ios::binary);
  if (!f) return Result<void>::fail(Error(ErrorDomain::IO, "cannot write p3_writer.json"));
  f << wr.dump(2) << "\n";
  f.close();
  // A2 单点键名: 节点 manifest 与 CLI 收集端统一用 output_fits_path
  //（旧节点写 output_fits、CLI 只读 output_fits_path → phase3_output role 恒缺）。
  (*man)["output_fits_path"] = fits_path;
  (*man)["writer_artifact"] = json_path;
  (*man)["artifacts"] = Json::array({fits_path, json_path});
  (*man)["sha256"] = std::string(ores.sha256);
  (*man)["uncertainty_available"] = unc;
  // B2-A10（宪章 §4.3）: writer 节点 manifest 携带真实 provenance，供 CLI
  // run manifest 汇总（input_product_hashes / units / coordinate_frames 等）。
  (*man)["run_id"] = run_id_str;
  (*man)["software_version"] = version_str;
  (*man)["source_sha"] = source_sha_str;
  (*man)["input_manifest_hash"] = input_manifest_hash;
  (*man)["product_sha256"] = std::string(ores.sha256);
  (*man)["coordinate_frame"] = "icrs";
  (*man)["bunit"] = res.value("bunit", "ADU");
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
           {"uncertainty_available", unc},
           // B2-A10: verify 侧同源透传 writer 的真实 provenance（禁 CLI 侧再猜）。
           {"run_id", wr.value("run_id", std::string())},
           {"software_version", wr.value("software_version", std::string())},
           {"input_manifest_hash", wr.value("input_manifest_hash", std::string())},
           {"source_sha", wr.value("source_sha", std::string())},
           {"product_sha256", wr.value("product_sha256", std::string())},
           {"coordinate_frame", wr.value("coordinate_frame", std::string())},
           {"bunit", wr.value("bunit", std::string())},
           {"algorithm_id", wr.value("algorithm_id", std::string())},
           {"module_build_id", wr.value("module_build_id", std::string())},
           {"provider", wr.value("provider", std::string())}};
  std::ofstream f(json_path, std::ios::binary);
  if (!f) return Result<void>::fail(Error(ErrorDomain::IO, "cannot write p3_verify.json"));
  f << ver.dump(2) << "\n";
  f.close();
  (*man)["verified_artifact"] = json_path;
  (*man)["artifacts"] = Json::array({json_path});
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
    // B2-A4/A5: projection/frame/coverage_output 值域 (validate/plan/run 一致拒绝面;
    // 未实现投影不得登记为可运行配置)。
    {
      std::string rerr;
      if (!p3n_check_request_fields(doc, &rerr))
        return Result<void>::fail(Error(ErrorDomain::DATA, rerr));
    }
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
                    {"status", "running"},
                    // B2-A10（宪章 §4.3）: 节点自报 algorithm ID / module build ID /
                    // provider；run manifest provenance 由此汇总。
                    {"algorithm_id", desc_.alg_id},
                    {"module_build_id", desc_.module_id + "@" + ASTROCS_VERSION_STRING},
                    {"provider", "baseline"}};
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

// B2-A10（宪章 §4.3）: run_context.json 唯一生成路径（CLI run 与 node 级测试
// 夹具共用）。原子写(tmp+rename)；run_id/software_version 必须非空（fail-closed，
// 消费端 p3_op_writer 依赖其真实性，禁占位）。
Result<void> write_run_context(const std::string& out_dir, const std::string& run_id,
                               const std::string& software_version,
                               const std::string& source_sha) {
  if (run_id.empty() || software_version.empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "run_context requires non-empty run_id/software_version (§4.3)"));
  Json ctx = {{"schema_version", "1"},
              {"kind", "astrocs_run_context"},
              {"run_id", run_id},
              {"software_version", software_version},
              {"source_sha", source_sha}};
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::u8path(out_dir), ec);
  const std::string final_path = out_dir + "/run_context.json";
  const std::string tmp_path = final_path + ".tmp";
  {
    std::ofstream f(std::filesystem::u8path(tmp_path),
                    std::ios::binary | std::ios::trunc);
    if (!f)
      return Result<void>::fail(Error(ErrorDomain::IO,
          "cannot write run context tmp: " + tmp_path));
    f << ctx.dump(2) << "\n";
    if (!f.good())
      return Result<void>::fail(Error(ErrorDomain::IO, "run context write failed"));
  }
  std::filesystem::rename(std::filesystem::u8path(tmp_path),
                          std::filesystem::u8path(final_path), ec);
  if (ec)
    return Result<void>::fail(Error(ErrorDomain::IO,
        "cannot finalize run context: " + ec.message()));
  return Result<void>::success();
}

// PSF-FAST-001 / INACTIVE: 精确 PSF 路径的**直调测试钩子**（声明见
// include/astrocs/core/module_adapters.h）。生产注册表（register_phase_modules）
// **不注册**本路径; 本钩子只供测试证明精确实现仍可编译、仍能跑出结果,
// 防止 inactive 代码被当作死代码清理（负责人裁决 2026-09-14）。
// 入参/出参用 std::string 承载 JSON: core 公共头不引入 nlohmann 实现依赖。
// n_fit_limit=0 ⇒ 全量星 Moffat4 拟合（= PSF-FAST-001 之前的旧口径）。
Result<void> p1_op_star_psf_precise_json(const std::string& config_json,
                                         std::string* manifest_json) {
  Json doc;
  try {
    doc = Json::parse(config_json);
  } catch (const std::exception& e) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        std::string("p1_op_star_psf_precise_json: bad config json: ") + e.what()));
  }
  if (!doc.is_object())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "p1_op_star_psf_precise_json: config must be a JSON object"));
  Json man = Json::object();
  auto rc = p1_op_star_psf_impl(doc, &man, /*n_fit_limit=*/0);
  if (manifest_json) *manifest_json = man.dump();
  return rc;
}

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
  // 任务触碰, 由 P3-RSMP-INT 处理 (lib/algorithms/resample/README.md 实测登记)。
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
