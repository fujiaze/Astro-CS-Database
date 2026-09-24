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
//   drizzle        → hp_drizzle_run_phase1_hips (lib/algorithms/drizzle 静态库)
//                    2026-09-20 订正 [V5 分片 5 / R-2]: 旧文 `hp_drizzle_run` 已作废 ——
//                    生产调用点本文件 :4745（A 分片报告记 :4562, 已漂移）;
//                    `hp_drizzle_run` 仅剩定义、零生产调用者（GAP_AUDIT A-04）
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
// lib/infrastructure/pipeline/module_ports.registry.json 冻结绑定表一致。
// 节点间 typed artifact 经 output_dir 文件约定传递（p2_coverage.json →
// p2_samples.json → p2_upm_model.{bin,json} → p2_corrected.{json,bin} →
// p2_rejection.{json,bin} → p2_integrated.{json,bin} → mosaic HiPS + p2_final.json）。
#include "astrocs/core/module_adapters.h"

// DET-001: 规范产品哈希（canonical product hash）—— 产品指纹口径的唯一实现
// （C++ 侧; 与 eng/tools/canonical_product_hash.py 逐字节同构）。
#include "astrocs/core/canonical_hash.h"
// UNIT-001: 母版单位/归一化消费门（纯规则；SCI-CAL-001 §3/§6/§8/§11 +
// ALG-CAL-001 §2 标度声明表 + DISP-CAL-013 + DATA-P1-CAL §9.1a）。
#include "astrocs/core/master_unit_guard.h"

// B2-A10: 构建期版本单源（与 CLI 共用同一生成头）——节点 manifest 自报
// module build ID 需要 ASTROCS_VERSION_STRING。
#include "version_generated.h"
// RUN-PROVENANCE-01: 构建期指纹（head/dirty/源集内容摘要）。run_context.json 必须
// 同时给出它，否则消费方只能拿 configure 期的 source_sha 当"同一二进制"判据 —— 那
// 正是 w02_2f/w04_2f 那次被污染的 A/B 比较的成因（run/RUN-PROVENANCE-01/REPORT.md §A）。
#include "astrocs/core/build_stamp.h"

#include "astrocs/common_abi_v1.h"
#include "astrocs/core/context.h"
// P3-STREAM-01：ARCH-504 export 子块流式调度器（生产接线点 = phase3 writer 节点）。
// 依据 ASTROCS_DESIGN §8.3 export 行「子块流式：读子块 → 投影重采样 → 写 FITS，
// 有界队列 + 背压，不整幅驻留」+ docs/contracts/SCHEDULER_CONTRACT.md §2。
#include "astrocs/core/export_stream.h"

#include "astro_calibration.h"
#include "astro_image_io.h"
#include "aio_fits.h"        // AIOImageData 完整布局: 写出前归一化样本格式为 FP32
#include "hp_drizzle_api.h"
#include "astrocs/noise/variance_plane_policy.h"   // §4a 三态表的方差面可用性判据

// P1-001 口径更新: 真实求解器/拟合器/HiPS writer 生产头（模块库零 diff 只读调用）
#include "dynamic_psf.h"     // lib/algorithms/psf: dpsf_fit_batch_f64 (Moffat4 FP64)
#include "ipv_api.h"         // lib/algorithms/platesolve/cpp/ipv: 真实 plate-solve 求解链
#include "ipv_select.h"      // STARDET-01: ipv 冻结的极限星等派生/视场密度实现
                             // (compute_fov_density / estimate_mag_lim_iterative,
                             //  唯一权威常量 206.265 与 m_lim_* 默认值)
#include "gaia_client.h"     // lib/infrastructure/gaia_xpsd_client: Gaia XPSD cone-search 客户端
#include "star_detector.h"   // lib/algorithms/star_detection: sdet_create/sdet_detect_ex_f64 句柄
// 注: C++ StarDetector 类头与 sdet C 头同名——裸名 include 命中 lib/algorithms/star_detection
// (lib/algorithms/star_detection/include 先于 lib/algorithms/star_detection/wrapper_phase1), C++ 类头以相对路径显式引入。
#include "../../../algorithms/star_detection/wrapper_phase1/star_detector.h"  // astrocs::phase1::StarDetector (C++)
#include "aio_hips.h"        // lib/infrastructure/aio: IVOA HiPS 标准写链
#include "aio_hips_reader.h" // lib/infrastructure/aio: HiPS 读面(P2 帧数据消费)
#include "aio_atomic_file.h" // lib/infrastructure/aio: §9 原子产品落盘原语(header-only)
#include "aio_file_io.h"     // CLEAN-403: aio 唯一整文件读取/摘要原语(header-only)
#include "aio_sysinfo.h"     // AIO-SYSINFO-01: 可用内存探测 (aio 边界内唯一实现)
#include "aio_disk_full.h"   // 磁盘满失败瞬间分类 (§10 + §7.2 exit 10)

// P2-001: Phase2 真实节点生产头（lib/algorithms/coverage 冻结 C ABI + HEALPix 单一实现 +
// 输入 manifest hash 共享 SHA-256; 模块库零 diff 只读调用）
#include "astro/phase2/coverage.h"
#include "astro/phase2/sampler.h"
#include "astro/phase2/upm.h"
#include "astro/phase2/rejection.h"
#include "astro/phase2/integrate.h"
#include "astro/phase2/sky_plane.h"   // FIX-A 稀疏天光面 (生产 mosaic 接线)
#include "astro/phase2/stage2_common.h"  // CONFORM-FIX-B-009: P2_SMOOTHING_LAMBDA_AUTO
                                         // 单一来源（stage2 工具与 node chain 同语义）
#include "healpix/healpix_core.h"  // fits_index_to_nested_local (NESTED LUT 单一权威)
// RELEASE-02 权重链: HiPS 头帧级 SNR → 逆方差权重 (w = SNR²/F_ref²)。
#include "astrocs/v6/weight_chain.h"
// RELEASE-02 P2b: 残差制造者 PΣPᵀ 归一化方差传播（variance_propagation.h）
#include "astrocs/v6/variance_propagation.h"
#include "crypto/sha256.h"         // astrocs::crypto::sha256_hex (input_manifest_hash)
#include "astrocs/probe.h"         // RELEASE-02 探针 (ASTROCS_PROBES=OFF 时宏为空语句)

#include "photometer.h"
#include "snr_estimator.h"   // NOISE-MODEL-CANON-001: SCI-NOISE-001 §5/§5a 唯一实现 (A)
// P8-SNR-LINUX: 逐源 SNR 帧级聚合 (lib/algorithms/noise_snr/wrapper_phase1), 其公式实现为
// lib/algorithms/noise_snr/cpp/src/snr_science.cpp (已编入 astrocs_phase1_noise)。
#include "snr_frame_science.h"
#include "wcs_tan.h"
// RELEASE-02 FIX-P1 (P1-1): Phase1 测光归一化真正接到像素。
//  - apply_photometry: I_photo = k_photo·I_cal (生产零调用者缺陷的修复;
//    astrocs_calibration 已编入 photometry_apply.cpp)
//  - fit_frame_photometry: 装配 gaia_client + filters/QE → 生产 star_matcher
//    Tukey-IRLS k_photo (astrocs_phase1_photcal)
#include "photometry_apply.h"
#include "frame_photometry_fit.h"

// P7-UTIL-001: 节点级 OpenMP 并行度注入的保存/恢复需要 ICV 访问器。
#ifdef _OPENMP
#include <omp.h>
#endif

// P3-002: Phase3 唯一真实 operation 节点生产头（冻结 C++ 内核, 静态库
// astrocs_phase3_session 已在 astrocs_module_adapters 链接闭包;
// 相对路径 include 同 "../../../algorithms/star_detection/wrapper_phase1/star_detector.h" 先例, 根 CMake
// 零改动）
// 三个 Phase3 会话内核头已按 ASTROCS_DESIGN §7.1 迁入各自算法
// 模块 —— p3_wcs.h → algorithms/projection (批次 1)、p3_resample.h →
// algorithms/resample (批次 2)、p3_output.h → algorithms/fits_output (批次 3);
// 符号与命名空间零改动, 仅 include 面改锚。
#include "../../../algorithms/resample/p3_resample.h"
// 冻结单位表 / BUNIT 可判性 / FZ-P3-MODES 模式枚举（源在
// lib/algorithms/resample/p3_rsmp_units.cpp, 已随 astrocs_p3_rsmp 进生产链接闭包）——
// 输入语义守卫与输出模式声明的唯一单位/模式词汇源, 不另发明第二套。
#include "../../../algorithms/resample/p3_rsmp.h"
#include "../../../algorithms/fits_output/p3_output.h"
#include "../../../algorithms/projection/p3_wcs.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>    // P0-21: p1_frame_key 字符白名单化
#include <chrono>    // P10-UTIL2-006: 节点执行窗口观测 (ASTROCS_NODE_TRACE)
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>   // P7-UTIL-001: std::getenv (ASTROCS_LEASE_TRACE 观测开关)
#include <cstring>
#include <exception>  // PERF-P2: 并行 worker 内异常跨线程回传
#include <stdexcept> // 守卫内 std::stoi 非法尾字符 → std::invalid_argument
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>       // P0-21: frame_key 唯一性 fail-closed
#include <thread>
#include <utility>
#include <mutex>
#include <vector>

// ── CLEAN-403: 本 TU 的文件 I/O 全部经 aio 机制原语 ─────────────────────────
// 依据 ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界：任何文件读写经 aio」+
// §9.73 裁决 U5。本命名空间只做**薄转发**(零策略/零缓存/零语义), 使调用点不再
// 出现第二处文件系统原语; 机制唯一实现在 lib/infrastructure/aio/src/**。
namespace aio_fs {
inline bool exists(const std::string& p) {
  return aio_atomic::path_exists(p, nullptr) != 0;
}
inline bool is_dir(const std::string& p) {
  int d = 0;
  return aio_atomic::path_exists(p, &d) != 0 && d != 0;
}
inline bool read_all(const std::string& p, std::string* out) {
  return aio_file::read_all(p.c_str(), out);
}
inline bool write_atomic(const std::string& p, const std::string& text) {
  return aio_atomic::write_file_atomic(p, text, nullptr) == 0;
}
inline bool make_dirs(const std::string& p) {
  return aio_atomic::make_dirs(p) == 0;
}
inline void remove(const std::string& p) {
  (void)aio_atomic::remove_file(p);
}
inline bool rename_replace(const std::string& from, const std::string& to) {
  return aio_atomic::atomic_replace(from, to) == 0;
}
inline bool file_size(const std::string& p, uint64_t* sz) {
  return aio_atomic::path_size(p, sz, nullptr) != 0;
}
// 纯字符串路径工具 (无文件系统调用)。
inline std::string base_name(const std::string& p) {
  const std::size_t s = p.find_last_of("/\\");
  return (s == std::string::npos) ? p : p.substr(s + 1);
}
inline std::string dir_name(const std::string& p) {
  const std::size_t s = p.find_last_of("/\\");
  if (s == std::string::npos) return std::string();
  if (s == 0) return p.substr(0, 1);
  return p.substr(0, s);
}
inline bool ends_with(const std::string& s, const char* suffix) {
  const std::size_t n = std::strlen(suffix);
  return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}
// 递归枚举 root 下全部条目 (先序; 机制经 aio for_each_child, 深度上限 64)。
// fn 返回非 0 ⇒ 立即中止, 本函数返回该值。
inline int walk_tree(const std::string& root,
                     const std::function<int(const std::string&, int)>& fn,
                     int depth) {
  if (depth > 64) return 0;
  int stop = 0;
  (void)aio_atomic::for_each_child(
      root,
      [&](const std::string& child, int kind) -> int {
        const int rc = fn(child, kind);
        if (rc != 0) { stop = rc; return 1; }
        if (kind == 1) {
          const int rc2 = walk_tree(child, fn, depth + 1);
          if (rc2 != 0) { stop = rc2; return 1; }
        }
        return 0;
      },
      nullptr);
  return stop;
}
}  // namespace aio_fs

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
// M1a-C-003: Δ(内部 0-based → FITS 1-based) = +1 (FITS WCS Paper I §2.1.1)
constexpr double kP1FitsPixelOrigin = 1.0;
// M1a-C-003: 内部像素坐标口径 = **0-based 数组下标 (index-is-center)**
// (SCI-WCS-001 §3a「内部 0-based x,y, FITS 输出 1-based xp=x+1」/ §5a 单一桥接点);
// WcsTan 与 p1_tan_forward_reference 的契约是 **FITS 1-based** (Paper I §2.1.1,
// wcs_tan.h:11)。⇒ 任何把内部下标喂给 WcsTan 的调用必须恰好施加一次本偏移
// (xp = x + kP1FitsPixelOrigin); 漏加 = 恒定 1px 系统偏差, 加两次 = 双重桥接。

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
      {"hips", "DATA-HIPS-001", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
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
      {"hips", "DATA-HIPS-001", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::HEALPIX},
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
      {"hips", "DATA-HIPS-001", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::HEALPIX},
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
      // 取向先验的真实来源 = 本帧已解出的 WCS 产物 <frame_dir>/p1_wcs.json。
      // 星表引导检测要把 Gaia 星表逆投影到像素域, 逆投影必须知道像面取向与镜像;
      // 由配置给出的"中心指向 + 板尺度"**不含取向**（缺省北向上/东向左），
      // 该假设下密集真实场仍有拟合通过全部质量门而落在错误天球位置。
      // 声明为 typed 输入端口 ⇒ 调度器保证 wcs 节点先落盘, 本节点直接以本帧
      // 自解 WCS 作先验, 不再要求调用方预先知道像面取向。
      {"wcs", "DATA-P1-WCS", true, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
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
      // 本节点按帧读**校准后像素**自行做星点检测与 Gaia 求解（sdet + ipv 真实链），
      // 不消费 p1_sources.json（与注册表 astrocs.phase1.wcs-platesolve 同款声明）。
      // 故本节点是 sources 的**上游**: 星检测节点排在解算之后不构成环。
      {"calibrated", "DATA-P1-CAL", true, UnitId::ADU, CoordinateFrame::PIXEL},
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
      // 校准后像素是测光的真实输入面（p1_calibrated_path 逐帧读取）。
      {"calibrated", "DATA-P1-CAL", true, UnitId::ADU, CoordinateFrame::PIXEL},
      // PSF 参数随 p1_sources.json 的 psf_params 行到达（同一产物、同一帧序），
      // 故**不**声明 artifact:p1_psf 输入端口: 该边在注册表里不存在
      // （astrocs.phase1.star-psf 的 p1_psf 端口"生产链路零消费者"）。
      {"sources", "DATA-P1-SOURCES", true, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
      // P1-PHOT-BROKEN: 本节点按 output_dir 文件约定读 <frame_dir>/p1_wcs.json
      // （回退 config.wcs）来取 CRVAL/CD 做 Gaia 投影。未声明该 typed 输入时
      // phot 与 wcs 同为 sources 下游**并发执行**, p1_wcs.json 是否存在取决于
      // 调度时序 ⇒ 同配置两次运行结果不同（实测: 16:22 冒烟跑读到缺失 WCS,
      // 以 CRVAL=(0,0)/CD=0 拟合出 0 匹配 → 占位 scale=1.0; 16:26 重跑读到
      // 真实 WCS → 939/917 匹配, location=16.20 dex）。与 F-8（drz←wcs）、
      // DET-001（drz←phot）同款处置: 声明 typed 边, 调度器保证 wcs 先落盘。
      {"wcs", "DATA-P1-WCS", true, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
      {"fluxes", "DATA-P1-FLUX", false, UnitId::ADU, CoordinateFrame::ICRS},
      // DET-001 (D5): p1_phot.json (DATA-P1-PHOTPROV-001) 是本节点的第二个真实
      // 产物, 且被 drizzle 节点按 output_dir 文件约定消费。未声明为 typed 输出
      // 时 drizzle 无依赖边、与 photometry 并发执行, 会在本文件落盘前读到
      // "不存在" 并把 photometry_provenance 记成 "absent"（非确定性 + 静默
      // ADU 降级）。声明为 typed 输出端口使依赖边可绑定（同 F-8 的 wcs 处置）。
      {"photprov", "DATA-P1-PHOTPROV-001", false, UnitId::DIMENSIONLESS,
       CoordinateFrame::ICRS},
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
      // 真实输入面（注册表 astrocs.phase1.noise-snr）: 逐源行 p1_sources.json +
      // 测光 provenance p1_phot.json + cleaned 像素。三者都是本节点实际读取的产物,
      // 声明为 typed 输入使 IR 依赖边可绑定（调度器保证上游先落盘）。
      {"sources", "DATA-P1-SOURCES", true, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
      {"photprov", "DATA-P1-PHOTPROV-001", true, UnitId::DIMENSIONLESS,
       CoordinateFrame::ICRS},
      {"cleaned", "DATA-P1-COSMETIC", true, UnitId::ADU, CoordinateFrame::PIXEL},
      // 合成 IR（单元测试构造的最小图）以本端口绑定 photometry→noise-snr 依赖;
      // 生产 IR 不声明本端口（p1_flux.json 在本节点无读取点）。
      {"fluxes", "DATA-P1-FLUX", true, UnitId::ADU, CoordinateFrame::ICRS},
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
      // DET-001 (D5): PHOTSCAL/PHOTAPPL 的真实来源是 photometry 节点产物
      // p1_phot.json。未声明该 typed 边时 drz 与 phot 同为 cal/psf 下游并发执行,
      // drz 的存在性判定可先于 phot 落盘 → photometry_provenance 在
      // "p1_phot.json"/"absent" 间翻转（14 次实测 10/4），且把「尚未产出」误记为
      // 「未应用测光」（静默 ADU 降级）。声明依赖边后调度器保证 phot 完成再执行 drz。
      {"photprov", "DATA-P1-PHOTPROV-001", true, UnitId::DIMENSIONLESS,
       CoordinateFrame::ICRS},
      // 逐星掩膜输入 = star-psf 节点产物 p1_sources.json（注册表 drizzle 的
      // p1_sources 端口同款声明）: 声明 typed 边使本节点不会被调度到 star-psf 之前。
      {"sources", "DATA-P1-SOURCES", true, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
      // 帧级 SNR 键的真实来源 = noise-snr 节点产物 p1_snr.json（astro_sphere_sink.cpp
      // write_hips_phase1 逐帧读取）。未声明该 typed 边时 drz 与 snr 并发执行,
      // 读到缺失/陈旧 p1_snr.json 时**只打印警告并跳过帧级 SNR 键**（静默降级）,
      // 产品缺少 SNR 而运行仍报成功。声明依赖边后调度器保证 snr 先落盘。
      {"snr", "DATA-P1-SNR", true, UnitId::DIMENSIONLESS, CoordinateFrame::ICRS},
      {"stacked", "DATA-P1-STACK", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::ICRS},
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
      {"stacked", "DATA-P1-STACK", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::ICRS},
      {"fits", "DATA-P1-FITS", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::ICRS},
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
      {"calibrated", "DATA-P2-CAL", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
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
      {"samples", "DATA-P2-SMP", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
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
      {"samples", "DATA-P2-SMP", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
      {"upm_model", "DATA-P2-UPM", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
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
      {"upm_model", "DATA-P2-UPM", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
      {"calibrated_frames", "DATA-P2-CAL", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
      {"corrected", "DATA-P2-COR", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
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
      {"corrected", "DATA-P2-COR", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
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
      {"corrected", "DATA-P2-COR", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
      {"integrated", "DATA-P2-INT", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
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
      //GAP_AUDIT G3-4 / ASTROCS_DESIGN §5.6「Phase2 信号为面亮度量纲」:
      // **写出端口**单位 = SURFACE_BRIGHTNESS（冻结单位表 signal_sb = ADU/sr;
      // docs/contracts/DATA_SEMANTICS.md §31.1）。integrated 输入面仍为
      // integrate 节点产出的逐像素信号面（docs/modules/registry/astrocs.phase2.write.md
      // 端口表同源: 输入 ADU / 输出 SURFACE_BRIGHTNESS）。
      {"integrated", "DATA-P2-INT", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
      {"mosaic", "DATA-P2-RES", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},
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
// manifest 携带 operation/entry 标记（与 lib/infrastructure/pipeline/module_ports.registry.json
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
  // CLEAN-403: 头部探测经 aio (aio_file::read_head), 无自持 ifstream。
  std::string magic;
  if (!aio_file::read_head(path.c_str(), 6, &magic)) return false;
  return magic.size() == 6 && std::strncmp(magic.data(), "SIMPLE", 6) == 0;
}

uint64_t p1_fits_primary_header_bytes(const std::string& path) {
  // CLEAN-403: 头部读取经 aio (aio_file::read_head, 上界 100 块 = 288 KB)。
  std::string blob;
  if (!aio_file::read_head(path.c_str(), 100ull * 2880ull, &blob)) return 0;
  const uint64_t have_blocks = static_cast<uint64_t>(blob.size()) / 2880ull;
  for (uint64_t blocks = 1; blocks <= have_blocks; ++blocks) {
    const char* blk = blob.data() + (blocks - 1) * 2880ull;
    for (int i = 0; i < 36; ++i) {
      const char* c = blk + i * 80;
      if (std::strncmp(c, "END", 3) == 0 && (c[3] == ' ' || c[3] == '\0'))
        return blocks * 2880ull;
    }
  }
  return 0;  // 主头无 END → fail-closed
}

// NOISE-MODEL-CANON-001（负责人 §9.67 定案 3）: 饱和电平读取。
// SCI NOISE_MODEL §4「饱和域」(claim SC-008) 要求：未提供电平时调用方**必须**
// 在帧产品写显式降级声明（禁止静默）。来源优先级 = cfg > SATURATE > DATAMAX，
// 与 snr_estimator.h:134-136 的声明一致。
// 返回 >0 = 有效电平（ADU）；0 = 未提供（调用方须写 DISABLED_NO_METADATA）。
double p1_fits_saturation_level(const std::string& path) {
  // CLEAN-403: 头部读取经 aio (aio_file::read_head, 上界 100 块 = 288 KB)。
  std::string blob;
  if (!aio_file::read_head(path.c_str(), 100ull * 2880ull, &blob)) return 0.0;
  double saturate = 0.0;
  double datamax = 0.0;
  auto parse_card = [](const char* c, double* out) -> bool {
    // 形如 "SATURATE=        65535.0 / comment"
    if (c[8] != '=') return false;
    char val[71];
    std::memcpy(val, c + 10, 70);
    val[70] = '\0';
    char* end = nullptr;
    const double v = std::strtod(val, &end);
    if (end == val) return false;           // 非数值（如 T/F 逻辑卡）
    if (!std::isfinite(v) || v <= 0.0) return false;
    *out = v;
    return true;
  };
  const uint64_t have_blocks = static_cast<uint64_t>(blob.size()) / 2880ull;
  for (uint64_t blocks = 1; blocks <= have_blocks; ++blocks) {
    const char* blk = blob.data() + (blocks - 1) * 2880ull;
    for (int i = 0; i < 36; ++i) {
      const char* c = blk + i * 80;
      if (std::strncmp(c, "END", 3) == 0 && (c[3] == ' ' || c[3] == '\0')) {
        if (saturate > 0.0) return saturate;
        if (datamax > 0.0) return datamax;
        return 0.0;
      }
      if (std::strncmp(c, "SATURATE", 8) == 0 && saturate <= 0.0) {
        double v = 0.0;
        if (parse_card(c, &v)) saturate = v;
      } else if (std::strncmp(c, "DATAMAX", 7) == 0 && datamax <= 0.0) {
        double v = 0.0;
        if (parse_card(c, &v)) datamax = v;
      }
    }
  }
  if (saturate > 0.0) return saturate;
  return datamax;
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
  // CLEAN-403: 文件大小经 aio (aio_atomic::path_size)。
  uint64_t sz = 0;
  return aio_fs::file_size(path, &sz) && sz >= need;
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

// ── P0-21: 一组进一组出（ASTROCS_DESIGN §3.4「输出基数」）───────────────
// 每帧输入 ⇒ 一个独立 HiPS 产品目录 output_dir/<frame_key>/。frame_key 由输入
// light 基名（去扩展名）经字符白名单派生; 同块内重复 key ⇒ fail-closed
// （两帧共用目录会互相覆盖产品与中间产物, 属"静默丢弃"）。
std::string p1_frame_key(const std::string& light) {
  const std::string base = p1_base_name(light);
  const size_t dot = base.find_last_of('.');
  const std::string stem =
      (dot == std::string::npos || dot == 0) ? base : base.substr(0, dot);
  std::string out;
  out.reserve(stem.size());
  for (unsigned char c : stem) {
    if (std::isalnum(c) || c == '_' || c == '-' || c == '.')
      out.push_back(static_cast<char>(c));
    else
      out.push_back('_');
  }
  if (out.empty() || out == "." || out == "..") out = "frame";
  return out;
}

std::string p1_frame_dir(const Json& doc, const std::string& light) {
  return doc.value("output_dir", std::string(".")) + "/" + p1_frame_key(light);
}

// 同块 frame_key 唯一性门。任何按帧派生落位的操作器（wcs/drizzle/writer）
// 必须先过此门; 重复 key 直接 DATA fail-closed, 不再静默覆盖。
Result<void> p1_require_unique_frame_keys(const Json& doc) {
  if (!p1_has(doc, "input_lights") || !doc["input_lights"].is_array())
    return Result<void>::success();
  std::set<std::string> seen;
  for (const auto& l : doc["input_lights"]) {
    if (!l.is_string()) continue;
    const std::string key = p1_frame_key(l.get<std::string>());
    if (!seen.insert(key).second)
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "input_lights frame key collision: '" + key +
          "' (two frames would share one HiPS product dir; rename inputs)"));
  }
  return Result<void>::success();
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
  // CLEAN-403: 原子替换机制经 aio 唯一实现 (aio_atomic::atomic_replace =
  // POSIX rename(2) / Windows MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH))。
  // 原"先删目标再 rename"兜底被删除: aio_atomic_file.h 冻结禁令「禁止先删目标
  // 再 rename」(删除后 rename 前崩溃 → 文件丢失), 且 MoveFileExW 已覆盖
  // REPLACE_EXISTING 语义 ⇒ 兜底既无必要又违反冻结不变式。
  if (!aio_fs::rename_replace(staging, final_path)) {
    if (err) *err = "atomic publish failed: " + final_path;
    aio_fs::remove(staging);
    return false;
  }
  aio_atomic::fsync_parent_dir(final_path);
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
    aio_fs::remove(staging);
    return false;
  }
  if (!p1_atomic_publish(staging, final_path, err)) return false;
  return true;
}

// 节点输入帧路径: 优先 cal 节点产物 calibrated_<base>（节点链约定）, 无则原帧
std::string p1_calibrated_path(const Json& doc, const std::string& light) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  const std::string cand = out_dir + "/calibrated_" + p1_base_name(light);
  if (aio_fs::exists(cand)) return cand;
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
  if (aio_fs::exists(cand)) return cand;
  return p1_calibrated_path(doc, light);
}

// ── RELEASE-02 FIX-P1 (P1-1): 测光已应用帧路径 photoapplied_<base> ──────────
// photometry 节点在 Drizzle 前把 I_photo = k_photo·I_cal 应用到 cal 产物, 写成
// 独立路径 photoapplied_<base>（不就地覆写上游产物, 同 CORE-RACE-001 语义）。
// drizzle 消费该路径; 若 provenance 声明 applied=true 而产物缺失 ⇒ 调用方
// fail-closed（不得静默退回未测光 ADU）。
std::string p1_photoapplied_path(const Json& doc, const std::string& light) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  return out_dir + "/photoapplied_" + p1_base_name(light);
}

bool p1_write_text(const std::string& path, const std::string& text) {
  // 原子发布（同目录临时文件 → fflush → fsync → rename）: 并发消费者不会读到
  // 半写 JSON。CLEAN-403: 机制经 aio 唯一实现 (aio_atomic::write_file_atomic),
  // 本 TU 不再自持 ofstream 通道。
  return aio_fs::write_atomic(path, text);
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
  // P0-21: 每帧一个产品目录 ⇒ frame_key 必须唯一（重复即 fail-closed）。
  return p1_require_unique_frame_keys(doc);
}

// ── UNIT-001: 母版单位/归一化声明解析 + 观测统计 + 声明换算 ──────────────────
// 依据: SCI-CAL-001 §3/§6/§8/§11；ALG-CAL-001 §2「标度声明」表 + DISP-CAL-013；
//       DATA-P1-CAL §9.1/§9.1a。规则本体是纯函数（astrocs/core/master_unit_guard.h），
//       本处只做 JSON 解析、像素统计与「按声明施加换算」；一句话：文件不携带 ADU 换算
//       因子（XISF bounds 只是可表示域），所以换算必须显式声明，否则 fail-closed。
namespace mu = astrocs::core::master_units;

bool p1_parse_master_units(const Json &doc, mu::Decl *d, std::string *err) {
  auto parse_unit = [&](const char *key, mu::ClassDecl *c) -> bool {
    if (!doc.contains("master_units")) return true;
    const Json &u = doc["master_units"];
    if (!u.is_object()) {
      if (err) *err = "master_units must be an object {light|bias|dark|flat: \"ADU\"|\"normalized\"}";
      return false;
    }
    if (!u.contains(key)) return true;
    const Json &v = u[key];
    if (!v.is_string()) {
      if (err) *err = std::string("master_units.") + key + " must be a string token";
      return false;
    }
    const std::string t = v.get<std::string>();
    c->has_unit = true;
    if (t == "ADU") {
      c->unit = mu::Unit::ADU;
    } else if (t == "normalized") {
      c->unit = mu::Unit::Normalized;
    } else {
      if (err) *err = std::string("master_units.") + key + "=\"" + t +
                      "\" unknown token (allowed: ADU|normalized)";
      return false;
    }
    return true;
  };
  auto parse_scale = [&](const char *key, mu::ClassDecl *c) -> bool {
    if (!doc.contains("master_scale")) return true;
    const Json &m = doc["master_scale"];
    if (!m.is_object()) {
      if (err) *err = "master_scale must be an object {light|bias|dark|flat: <number>}";
      return false;
    }
    if (!m.contains(key)) return true;
    const Json &v = m[key];
    if (!v.is_number()) {
      if (err) *err = std::string("master_scale.") + key + " must be a number (ADU factor)";
      return false;
    }
    c->has_scale = true;
    c->scale = v.get<double>();
    return true;
  };
  const char *keys[4] = {"light", "bias", "dark", "flat"};
  mu::ClassDecl *classes[4] = {&d->light, &d->bias, &d->dark, &d->flat};
  for (int i = 0; i < 4; ++i)
    if (!parse_unit(keys[i], classes[i])) return false;
  for (int i = 0; i < 4; ++i)
    if (!parse_scale(keys[i], classes[i])) return false;
  if (doc.contains("master_flat_normalize")) {
    const Json &v = doc["master_flat_normalize"];
    if (!v.is_string()) {
      if (err) *err = "master_flat_normalize must be \"none\" or \"median\"";
      return false;
    }
    const std::string t = v.get<std::string>();
    d->flat_normalize_given = true;
    if (t == "median") {
      d->flat_normalize_median = true;
    } else if (t != "none") {
      if (err) *err = "master_flat_normalize=\"" + t + "\" unknown (allowed: none|median)";
      return false;
    }
  }
  if (doc.contains("master_flat_median_range")) {
    const Json &v = doc["master_flat_median_range"];
    if (!v.is_array() || v.size() != 2 || !v[0].is_number() || !v[1].is_number()) {
      if (err) *err = "master_flat_median_range must be [lower, upper] (two numbers)";
      return false;
    }
    d->flat_band_given = true;
    d->flat_band_lo = v[0].get<double>();
    d->flat_band_hi = v[1].get<double>();
    if (!(d->flat_band_lo < d->flat_band_hi) || !std::isfinite(d->flat_band_lo) ||
        !std::isfinite(d->flat_band_hi)) {
      if (err) *err = "master_flat_median_range must satisfy lower < upper (finite)";
      return false;
    }
  }
  // dark 的 bias 约定：dark_optimization 是否显式给出（U3 判据，见 master_unit_guard.h）
  d->dark_convention_given = p1_has(doc, "dark_optimization");
  d->dark_includes_bias = d->dark_convention_given && p1_flag(doc, "dark_optimization", false);
  return true;
}

// 观测统计（中位数口径与 p1_master_flat_valid/master_generator 一致：偶数取中间两值均值）。
mu::Stats p1_image_stats(const P1Image &im) {
  mu::Stats s;
  if (!im.ok() || im.px() == nullptr || im.w() <= 0 || im.h() <= 0) {
    s.has_finite = false;
    return s;
  }
  const int64_t n = static_cast<int64_t>(im.w()) * static_cast<int64_t>(im.h());
  const float *px = im.px();
  std::vector<float> vals;
  vals.reserve(static_cast<size_t>(n));
  double mn = 0.0, mx = 0.0;
  bool first = true;
  for (int64_t i = 0; i < n; ++i) {
    const float v = px[i];
    if (!std::isfinite(v)) {
      s.all_finite = false;
      continue;
    }
    vals.push_back(v);
    if (first) {
      mn = mx = static_cast<double>(v);
      first = false;
    } else {
      if (v < mn) mn = v;
      if (v > mx) mx = v;
    }
  }
  if (vals.empty()) {
    s.has_finite = false;
    return s;
  }
  s.min_v = mn;
  s.max_v = mx;
  const size_t mid = vals.size() / 2;
  std::nth_element(vals.begin(), vals.begin() + static_cast<std::ptrdiff_t>(mid), vals.end());
  double med = static_cast<double>(vals[mid]);
  if (vals.size() % 2 == 0) {
    const float lower =
        *std::max_element(vals.begin(), vals.begin() + static_cast<std::ptrdiff_t>(mid));
    med = (static_cast<double>(lower) + med) * 0.5;
  }
  s.median = med;
  return s;
}

// 按显式声明施加线性换算（仅当 scale 有效且 != 1.0；k<=1 与 NaN 已在 U4 拒绝）。
void p1_apply_declared_scale(P1Image &im, double k) {
  if (!im.ok() || im.px() == nullptr || k == 1.0 || !std::isfinite(k) || !(k > 0.0)) return;
  const int64_t n = static_cast<int64_t>(im.w()) * static_cast<int64_t>(im.h());
  float *px = im.px();
  for (int64_t i = 0; i < n; ++i) px[i] = static_cast<float>(static_cast<double>(px[i]) * k);
}

// 显式声明 master_flat_normalize="median" 时按 SCI-CAL-001 §5 施加
// flat/median(flat)（对已归一平场幂等；floor 0.1 仍由 calibrate 施加）。
bool p1_normalize_flat_median(P1Image &flat, double *med_before, double *med_after,
                              std::string *err) {
  const mu::Stats s = p1_image_stats(flat);
  if (!s.has_finite) {
    if (err) *err = "master_flat has no finite pixels";
    return false;
  }
  if (!(s.median > 0.0)) {
    if (err) *err = "master_flat median<=0, cannot normalize";
    return false;
  }
  if (med_before) *med_before = s.median;
  const double inv = 1.0 / s.median;
  const int64_t n = static_cast<int64_t>(flat.w()) * static_cast<int64_t>(flat.h());
  float *px = flat.px();
  for (int64_t i = 0; i < n; ++i) px[i] = static_cast<float>(static_cast<double>(px[i]) * inv);
  if (med_after) *med_after = p1_image_stats(flat).median;
  return true;
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
// ── PERF-P1 (RELEASE-05): 确定性帧级并行执行器 ─────────────────────────────
// 与 p2_parallel_for（PERF-P2）**同规范**，不是另发明一套：
//   · 线程数 = Runtime lease 注入的 __workers（AGENTS §6: 禁硬编码线程数;
//     1 = 串行 reference）；
//   · 任务以原子计数动态认领（等价 dynamic schedule, 抗帧间负载不均）；
//   · 每个任务只写**自己下标**的结果槽与**自己帧**的产物路径；跨任务不做任何
//     浮点归约 ⇒ 结果与串行**逐位一致**、与线程数/调度顺序无关；
//   · 帧序归约（累加器 / 产物列表 / 错误选择）在 join 之后由调用线程**按下标升序**
//     执行 ⇒ 归约顺序冻结，与串行完全相同；
//   · worker 内异常经 exception_ptr 回传并在 join 后重抛，与串行异常语义一致。
// 线程安全前提（已核查）: 各算法入口只有不可变的 static const API 表；drizzle 的
// 错误槽与计数为 thread_local；帧间无共享可变状态。持有句柄的节点（star-psf 的
// 检测器、wcs 的 ipv/gaia/sdet）必须**按 worker 分实例**，不得跨 worker 共享。
// PERF-501 观测: ASTROCS_P1CAP_TRACE=1 时输出**并行轴分配快照**（租约 / 内存上限 /
// 帧在飞数 / 帧内 OpenMP 度）。仅观测，不改变调度、并行度与科学结果；默认零输出
// 零开销。用途: 把进程级 /proc CPU 平台（如恒 202%）精确归因到"哪个节点被压到几条
// 线程"。节点名取调度器写入的线程本地归属（context.h:170 trace_current_node），
// 无需逐调用点传参。
void trace_p1_cap(const char* what, uint32_t lease, uint32_t memory_cap,
                  uint32_t frame_workers, uint64_t n, uint32_t inner_omp) {
  static const bool on = [] {
    const char* v = std::getenv("ASTROCS_P1CAP_TRACE");
    return v && v[0] == '1';
  }();
  if (!on) return;
  std::fprintf(stderr,
               "[p1cap] %s node=%s lease=%u memory_cap=%u frame_workers=%u "
               "n_units=%llu inner_omp=%u\n",
               what, trace_current_node().c_str(), lease, memory_cap,
               frame_workers, static_cast<unsigned long long>(n), inner_omp);
}

// 标定旋钮读取（仅 P1-PARALLEL-AXIS-REDESIGN-01 受控 A/B 用；缺省 0 = 用策略值）。
// 只做无符号十进制解析；非法输入返回 0（= 不覆盖），不抛异常、不改任何数值路径。
static uint32_t p1_axis_env_u32(const char* name) {
  const char* v = std::getenv(name);
  if (!v || !v[0]) return 0;
  char* end = nullptr;
  const unsigned long long x = std::strtoull(v, &end, 10);
  if (end == v) return 0;
  return static_cast<uint32_t>(std::min<unsigned long long>(x, 4096ull));
}

template <typename Fn>
static void p1_parallel_for(uint32_t workers, uint64_t n, uint32_t thread_budget,
                            Fn&& body) {
  // ── P1-PARALLEL-AXIS-REDESIGN-01: 并行轴分配 ─────────────────────────────
  // 规范依据: docs/architecture/THREADING_MODEL.md「并行轴分配」（两轴之积 ≤ lease）;
  //   docs/contracts/SCHEDULER_CONTRACT.md:28（归约序按帧/块/窗口/像素 ID 固定、
  //   跨 worker 无共享浮点累加器）⇒ 轴形态只决定「预算怎么用」，不进数值路径。
  // 轴: 帧级 frame_w（受内存闸门约束）× 帧内 OpenMP inner_u，frame_w × inner_u ≤ budget。
  // 策略: **保留既有的「帧轴优先摊开、剩余预算转帧内轴」分配（本轮不改其行为）**。
  //   「内存闸门咬住时改为一帧独占全部预算（frame_w = 1, inner_u = budget）」这一形态
  //   已被同二进制四档受控 A/B **实测否决**（run/P1-PARALLEL-AXIS-REDESIGN-01/REPORT.md）:
  //     8 帧×2 线程（现状） 墙钟 390.7 s / 峰值 14.12 GB / CPU p50 1321%
  //     2 帧×8 线程         墙钟 532.9 s / 峰值  5.25 GB / CPU p50  701%
  //     1 帧×16 线程        墙钟 583.7 s / 峰值  3.57 GB / CPU p50   98.9%
  //     8 帧×「单帧高并行令牌」 墙钟 459.0 s / 峰值 12.43 GB / CPU p50 720%
  //   ⇒ 帧在飞数越少：墙钟单调变差、CPU 占用单调变低、内存单调变省。
  //   根因: 每帧都有**不可并行**的串行段（FITS 读、hips_write 占 drizzle 帧时 24–30%、
  //   星表查询、wcs-platesolve 无帧级并行），帧级并发正是把这些串行段互相重叠的手段；
  //   压到 1 帧会让其余 15 个核在串行段上空转 —— cpu_p50 98.9% 即「真的变成单核」，
  //   恰是本轮要消除的症状本身。四档产品**逐位相同** ⇒ 轴形态不改科学结果，
  //   故按**墙钟**选型（内存余量见 REPORT §5）。
  // 不变式: in_flight × inner_u ≤ budget，且 budget = Runtime lease（AGENTS §6:
  //   线程数唯一来自 lease，不硬编码）。
  const uint32_t budget = (thread_budget > 0) ? thread_budget : workers;
  uint32_t frame_w = (workers > 0) ? workers : 1u;
  const uint64_t in_flight =
      std::min<uint64_t>(n, static_cast<uint64_t>(frame_w));
#ifdef _OPENMP
  uint32_t inner_u =
      (in_flight > 0)
          ? std::max<uint32_t>(1u, budget / static_cast<uint32_t>(in_flight))
          : 1u;
#else
  uint32_t inner_u = 1u;
#endif
  // 标定旋钮（仅用于**同一二进制、唯一变量 = 并行轴形态**的受控 A/B；缺省 0 = 策略值）。
  // 越界（frame_w × inner_u > budget）一律**拒绝覆盖**并留痕 ⇒ fail-closed 回策略值，
  // 「总并行度 ≤ lease」不因标定而破（AGENTS §6）。
  {
    const uint32_t fw = p1_axis_env_u32("ASTROCS_P1_AXIS_FRAME_WORKERS");
    const uint32_t io = p1_axis_env_u32("ASTROCS_P1_AXIS_INNER_OMP");
    if (fw > 0 || io > 0) {
      const uint32_t nf = (fw > 0) ? fw : frame_w;
      const uint32_t ni = (io > 0) ? io : inner_u;
      if (nf >= 1u && ni >= 1u &&
          static_cast<uint64_t>(nf) * static_cast<uint64_t>(ni) <=
              static_cast<uint64_t>(budget)) {
        frame_w = nf;
        inner_u = ni;
      } else {
        std::fprintf(stderr,
                     "[p1axis] 拒绝越界标定 frame_workers=%u inner_omp=%u "
                     "budget=%u ⇒ 回策略值 frame_workers=%u inner_omp=%u\n",
                     nf, ni, budget, frame_w, inner_u);
      }
    }
  }
  // memory_cap 列在本行不适用（帧级池大小已由 workers 给出），恒填 0 = n/a；
  // 真实内存闸门值见同节点的 [p1cap] frame_workers 行。
  trace_p1_cap("parallel_for", budget, 0, frame_w, n, inner_u);
  if (frame_w <= 1 || n <= 1) {
    // 帧级宽度退化为 1 的路径（lease=1 / 内存闸门 cap=1 / 单帧 / 标定旋钮指定）。
    // **必须显式把帧内预算注入本线程 ICV** —— 缺陷（本轮修，P1-PARALLEL-AXIS-REDESIGN-01）:
    // 旧实现在本分支直接串行循环、**从不调用 omp_set_num_threads** ⇒ 帧内宽度回落到
    // **调用线程的既有 ICV**（通常是进程默认 = 硬件并发，而非 lease）：
    // 「本帧独占整个预算」既不被保证、也随调用上下文漂移（不可复现），
    // 且可能超出 lease 造成超额订阅。
    // 依据: docs/architecture/THREADING_MODEL.md「并行轴分配」不变式。
#ifdef _OPENMP
    omp_set_num_threads(static_cast<int>(inner_u));
#endif
    for (uint64_t i = 0; i < n; ++i) body(i, 0u);
    return;
  }
  // PERF-P1: 帧级并行占用了并行轴 ⇒ 帧内 OpenMP 并行度必须按**剩余预算**分配，
  // 禁止 N×N 超额订阅。依据：drizzle_engine.cpp 的 num_threads 与
  // dpsf_psf.cpp 的 omp parallel for 都取当前线程的 omp_get_max_threads()；
  // std::thread worker 的 ICV 是进程默认值（= 硬件并发），若不显式收窄就会
  // N×N 线程 + 每帧 num_threads 份线程局部缓冲（内存 ×256）。
  // 这是**并行轴分配**，不是硬编码线程数：总并行度仍由 Runtime lease 的
  // __workers 唯一决定（AGENTS §6）。
  // 注: omp_set_num_threads 只改**本线程**的 ICV，不影响主线程与进程其他阶段。
#ifdef _OPENMP
  const int inner_omp = static_cast<int>(inner_u);
#endif
  std::atomic<uint64_t> next{0};
  std::vector<std::exception_ptr> eptr(frame_w, nullptr);
  std::vector<std::thread> pool;
  pool.reserve(frame_w);
  for (uint32_t w = 0; w < frame_w; ++w) {
    pool.emplace_back([&, w]() {
#ifdef _OPENMP
      omp_set_num_threads(inner_omp);
#endif
      try {
        for (;;) {
          const uint64_t i = next.fetch_add(1);
          if (i >= n) break;
          body(i, w);
        }
      } catch (...) {
        eptr[w] = std::current_exception();
      }
    });
  }
  for (auto& th : pool) th.join();
  for (const auto& e : eptr)
    if (e) std::rethrow_exception(e);
}

// P1 节点并行度：Runtime lease 注入的 __workers（budget 唯一权威; 1 = 串行）。
static uint32_t p1_workers(const Json& doc) {
  return std::max(1u, doc.value("__workers", 1u));
}

// ── PERF-P1: 帧级并行的**内存安全**并发上限 ────────────────────────────────
// 实测约束（run/RELEASE-05/perf/logs/{ser3,par3,par11}.log；同一代码同一数据，
// 仅差并行轴）: T4 4500x3600（16.2e6 px）FP64、drizzle auto nside=65536 下
//   · 串行 3 帧峰值 RSS 4.09 GB；
//   · 3 帧并行峰值 RSS 17.5 GB（≈ 5.8 GB/帧 ≈ 358 B/px；含 signal/weight/variance
//     累积 map、确定性 stripe 归约槽与 HiPS 写出缓冲的并发放大）；
//   · 11 帧并行 645 s 后被 OOM killer 杀掉（RC=137 SIGKILL，产物只写了 27 个文件）。
// ⇒ 并发帧数必须受**可用内存**约束：这不是"环境问题"，而是内存随并发帧数近线性
//   增长的实现事实。口径（全部由可观测物理量派生）:
//      cap = floor(可用内存 × 安全系数 / (帧像素数 × 每像素字节))
//   每像素字节 = 上述实测标定；帧像素数取 FITS 头（aio_read_metadata，不读像素）。
// fail-closed: 可用内存或帧尺寸不可判定 ⇒ cap=1（退回串行，不冒险 OOM）。
// 平台: Windows 未接入可用内存 API（不在未验证平台上启用并行）⇒ 恒 cap=1，
//   待 BLD-501 Windows 腿接入 GlobalMemoryStatusEx 后放开并补平台证据。
//   AIO-SYSINFO-01: 探测机制已在 aio 侧就绪（aio_system_available_memory_bytes，
//   Windows 分支 = GlobalMemoryStatusEx.ullAvailPhys）；**是否在 Windows 放开并行
//   仍属平台策略**（需平台证据），故本函数在 Windows 保持既有取值 0 = 不可判定。
// P1-CONCURRENCY-CALIB-01 重标定（同二进制受控扫描；2026-09-23）：
//   · 旧值 358.0 的来历: par3（3 帧并行）峰值 RSS 17,946,058,752 B ÷ 3 帧 ÷ 16.2e6 px
//     （T4 4500×3600）≈ 369 B/px。该构造**隐含 base = 0**（把整幅峰值全摊给"在飞帧"），
//     而同一报告的 ser3（3 帧**串行**、1 CPU）峰值就是 4.19 GB ⇒ base ≠ 0；
//     且它是 **K = num_threads 时代**的标定（scratch 池随 worker 数线性放大）。
//   · 现口径: kScratchPoolCap = 2 把池钉在 K ≤ 2，峰值不再随 worker 数放大。
//   · **实测内存模型**（run/P1-CONCURRENCY-CALIB-01；同一二进制
//     build_source_digest=f86d60e2…、同场 8 帧 4096²/FP64/auto nside、inner_omp 恒 1、
//     只变 in_flight、峰值 RSS 由**外部**采样 /proc/<pid>/status VmHWM）:
//         RSS(F) = 0.143 GB + F × 1.6724 GB        （F = in_flight，残差 ≤ ±2.5%）
//         ⇒ 边际 99.68 B/px、base 0.143 GB
//       实测点: F=1→1.799 / F=2→3.576,3.580（两次重复）/ F=4→6.726 /
//               F=8→13.555,13.325 GB（两次重复）
//   · 旧值 358.0 对**边际**项保守 3.59×（358 / 99.68）。
//   · **原「抬帧并发会改变 HiPS 产品」的拒收理由已被证伪**（P1-CONCURRENCY-CALIB-01 §3.3）：
//     其唯一证据（t2_16f_fixA vs t2_16f_after 的 8769/11820 DATASUM 变化）是**跨二进制 +
//     跨通带曲线**的对照 —— 两跑 k_photo 差 ×2.457905（0.86 mag），signal 逐像素 A/B 比值
//     恰为该刻度比（min/p50/max = 2.4579049/2.4579051/2.4579053）⇒ 残差归零；
//     且祖先归约按 s 互斥（aio_hips_writer.cpp:771-777；HIPS-DETERMINISM-01 已证
//     "tile 到达序 +=" 形态在生产不存在）、跨帧零浮点归约（本文件 :6203-6206）。
//   · **受控扫描直接反证**：同帧集 8 帧、同一二进制、只变 in_flight(2/4/8) 与 inner_omp(1/2)
//     ⇒ **FITS 差异 0/5916、必同 JSON 差异 0**（含全部 HiPS 层级、calibrated/photoapplied
//     科学帧与组级星表聚合），仅 10 个含 output_dir 路径串的 JSON 在掩码后逐键相同。
//   · 取值 116.0 的依据: W_eff = F × min(max(1, lease/F), K) 在 F=8 取到 16（=8×2），
//     而 F∈[9,15] 是 I=1 死区（W_eff 掉到 9..15）⇒ **不是越小越好**。
//     F=8 ⟺ 0.75A/(9P) < B ≤ 0.75A/(8P)（是**窗口**不是阈值）；
//     A=20.77 GB 时 = (103.2, 116.1]。取窗口上端 116.0 ⇒ A ∈ [20.76, 23.3] GB 内
//     得 F=8 / W_eff=16；A 更低时优雅退化为 F=6–7（W_eff=12–14），
//     任何 A 下都不劣于旧值的 W_eff=4。
//   · **目标配置实测**（lease=16, F=8, inner=2, W_eff=16）: 峰值 RSS 13.940 GB
//     = 0.75A(A=20.77 GB) 的 89.5% ⇒ 余量 10.5%；同帧集墙钟 421.0 s，
//     对比旧口径的 W_eff=4（783.5 s）为 **1.86×**。
//   · 标定对象变更（drizzle 精度模式 / nside / 帧几何 / K）必须重测并更新本值。
//   · 残留风险: p1_memory_cap 是**瞬时快照**而非预留；无 swap 机器上闸门放行后若其它进程
//     再吃内存，安全垫会被吃掉（旧值峰值 3.5 GB 时风险小，13.9 GB 时不可忽略）。
static constexpr double kP1FrameBytesPerPixel = 116.0;   // 实测标定（P1-CONCURRENCY-CALIB-01）
static constexpr double kP1FrameMemSafetyFrac = 0.75;    // 留基础占用与运行波动

static uint64_t p1_available_memory_bytes() {
#ifdef _WIN32
  return 0;  // 平台策略不变（见上）: Windows 恒 cap=1
#else
  // AIO-SYSINFO-01: 可用内存探测经 aio 边界（ASTROCS_DESIGN §10「aio 是文件级
  // 唯一 I/O 边界」+ §9.73 裁决 U5）。口径（/proc/meminfo 的 MemAvailable，含
  // 可回收 page cache；读不到则回退 sysconf(_SC_AVPHYS_PAGES)×_SC_PAGESIZE）
  // 在 aio 侧唯一实现，本 TU 不再自持 fopen/fgets 通道。返回 0 = 不可判定
  // ⇒ 上层 p1_memory_cap 判 cap=1（fail-closed）。
  return aio_system_available_memory_bytes();
#endif
}

// 帧级并行的内存安全并发上限（>=1）。帧尺寸取 input_lights[0] 的头（优先逐帧产物，
// 回退原始 light），与节点实际消费的路径族一致。
static uint32_t p1_memory_cap(const Json& doc) {
  const uint64_t avail = p1_available_memory_bytes();
  if (avail == 0) return 1;
  if (!p1_has(doc, "input_lights") || !doc["input_lights"].is_array() ||
      doc["input_lights"].empty())
    return 1;
  const std::string first = doc["input_lights"][0].get<std::string>();
  std::string probe = p1_calibrated_path(doc, first);
  if (!aio_fs::exists(probe)) probe = first;
  const AIOImageMetadata meta = aio_read_metadata(probe.c_str());
  const double w = static_cast<double>(meta.geometry.width);
  const double h = static_cast<double>(meta.geometry.height);
  if (!(w > 0.0) || !(h > 0.0)) return 1;
  const double per_frame = w * h * kP1FrameBytesPerPixel;
  const double cap =
      std::floor(static_cast<double>(avail) * kP1FrameMemSafetyFrac / per_frame);
  if (!(cap >= 1.0)) return 1;
  return static_cast<uint32_t>(std::min(cap, 1024.0));
}

// P1 帧级并行的**实际**并行度 = min(Runtime lease 预算, 内存安全上限)。帧数多于该值时
// p1_parallel_for 的原子认领天然分批（同时最多 cap 帧在飞，内存受控）。
static uint32_t p1_frame_workers(const Json& doc) {
  const uint32_t lease = p1_workers(doc);
  const uint32_t mem_cap = p1_memory_cap(doc);
  const uint32_t frame_workers = std::min(lease, mem_cap);
  // PERF-501: 并行轴分配的**根因快照**（lease 是预算权威，memory_cap 是内存闸门，
  // 两者取小才是真实帧在飞数）。仅 ASTROCS_P1CAP_TRACE=1 时输出。
  trace_p1_cap("frame_workers", lease, mem_cap, frame_workers, 0, 0);
  return frame_workers;
}

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
  // ── UNIT-001: 母版单位/归一化消费门（声明 + 观测校验 + 声明换算）────────────
  // 依据 SCI-CAL-001 §3/§6/§8/§11、ALG-CAL-001 §2「标度声明」表 + DISP-CAL-013、
  // DATA-P1-CAL §9.1a。只做「声明是否给出 / 观测域是否与声明相容 / 按声明施加换算」，
  // 不改任何科学公式；违反即 DATA 拒绝（CLI rc=2）并点名文件与观测值，
  // 禁止静默按错误标度计算（XISF Float32 bounds="0:1" 是可表示域，不是 ADU）。
  mu::Decl mu_decl;
  {
    std::string mu_err;
    if (!p1_parse_master_units(doc, &mu_decl, &mu_err)) {
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "MASTER_UNIT_DECLARATION_INVALID: " + mu_err));
    }
    const mu::Verdict dv = mu::check_declarations(mu_decl);
    if (!dv.ok) {
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::DATA, dv.message));
    }
  }
  if (dark.ok()) {
    const mu::Verdict dv =
        mu::check_dark_convention(doc["master_dark"].get<std::string>(), mu_decl);
    if (!dv.ok) {
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::DATA, dv.message));
    }
  }
  // 声明换算（归一化域 → ADU）：换算因子只来自显式声明（文件不携带）。
  Json mu_applied = Json::object();
  if (bias.ok() && mu_decl.bias.has_scale) {
    p1_apply_declared_scale(bias, mu_decl.bias.scale);
    mu_applied["bias"] = mu_decl.bias.scale;
  }
  if (dark.ok() && mu_decl.dark.has_scale) {
    p1_apply_declared_scale(dark, mu_decl.dark.scale);
    mu_applied["dark"] = mu_decl.dark.scale;
  }
  if (flat.ok() && mu_decl.flat.has_scale) {
    p1_apply_declared_scale(flat, mu_decl.flat.scale);
    mu_applied["flat"] = mu_decl.flat.scale;
  }
  // 亮场域证据**逐帧**探测（P0-21 §3.4 一组进一组出: 每帧独立校验, 不得只取
  // 首帧——首帧探测会让其余帧的域错配静默通过。换算按声明施加后再取统计）。
  std::vector<mu::Stats> mu_light_probes;
  mu_light_probes.reserve(lights.size());
  for (const auto& lp : lights) {
    P1Image probe = p1_read_image(lp);
    if (!p1_image_sane(probe, lp)) {
      (*man)["error_kind"] = "input";
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read light: " + lp));
    }
    if (mu_decl.light.has_scale) p1_apply_declared_scale(probe, mu_decl.light.scale);
    mu_light_probes.push_back(p1_image_stats(probe));
  }
  const mu::Stats mu_light_probe = mu_light_probes.front();  // 溯源记首帧口径
  const mu::Stats mu_st_bias = p1_image_stats(bias);
  const mu::Stats mu_st_dark = p1_image_stats(dark);
  const mu::Stats mu_st_flat = p1_image_stats(flat);
  for (size_t fi = 0; fi < lights.size(); ++fi) {
    // [RELEASE-02 probe] 逐帧热点: calibrate
    ASTROCS_PROBE_SCOPE_CTX(_probe_cal_frame, "phase1", "calibrate.frame");
    ASTROCS_PROBE_TAG(_probe_cal_frame, "frame_key", p1_frame_key(lights[fi]).c_str());
    if (bias.ok()) {
      const mu::Verdict uv = mu::check_master_domain(
          "bias", doc["master_bias"].get<std::string>(), mu_decl.bias, mu_st_bias,
          mu_light_probes[fi]);
      if (!uv.ok) {
        st_cal["status"] = "fail";
        return Result<void>::fail(Error(ErrorDomain::DATA,
            uv.message + " (light frame: " + lights[fi] + ")"));
      }
    }
    if (dark.ok()) {
      const mu::Verdict uv = mu::check_master_domain(
          "dark", doc["master_dark"].get<std::string>(), mu_decl.dark, mu_st_dark,
          mu_light_probes[fi]);
      if (!uv.ok) {
        st_cal["status"] = "fail";
        return Result<void>::fail(Error(ErrorDomain::DATA,
            uv.message + " (light frame: " + lights[fi] + ")"));
      }
    }
  }
  double mu_flat_med_before = 0.0, mu_flat_med_after = 0.0;
  bool mu_flat_normalized = false;
  if (flat.ok()) {
    const std::string flat_path = doc["master_flat"].get<std::string>();
    const mu::Verdict fv = mu::check_flat_normalized(flat_path, mu_decl, mu_st_flat);
    if (!fv.ok) {
      st_cal["status"] = "fail";
      return Result<void>::fail(Error(ErrorDomain::DATA, fv.message));
    }
    if (mu_decl.flat_normalize_median) {
      std::string nerr;
      if (!p1_normalize_flat_median(flat, &mu_flat_med_before, &mu_flat_med_after, &nerr)) {
        st_cal["status"] = "fail";
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "MASTER_FLAT_NOT_NORMALIZED: master_flat=" + flat_path + " " + nerr));
      }
      mu_flat_normalized = true;
    }
  }
  // 溯源（记实际执行路径）: 声明 + 观测中位数 + 实际换算/归一动作。
  Json mu_man{{"spec", "UNIT-001/ALG-CAL-001-DISP-CAL-013"},
              {"declared_units", {{"light", mu::unit_token(mu_decl.light.unit)},
                                  {"bias", mu::unit_token(mu_decl.bias.unit)},
                                  {"dark", mu::unit_token(mu_decl.dark.unit)},
                                  {"flat", mu::unit_token(mu_decl.flat.unit)}}},
              {"dark_convention_declared", mu_decl.dark_convention_given},
              {"dark_includes_bias", mu_decl.dark_includes_bias},
              {"flat_normalize", mu_flat_normalized ? "median" : "none"},
              {"flat_median_range", {mu_decl.flat_band_lo, mu_decl.flat_band_hi}},
              {"flat_median_before", mu_flat_med_before},
              {"flat_median_after", mu_flat_med_after},
              {"observed_median", {{"light", mu_light_probe.median},
                                   {"bias", mu_st_bias.median},
                                   {"dark", mu_st_dark.median},
                                   {"flat", mu_st_flat.median}}},
              // P0-21: 亮场域证据逐帧覆盖数（= input_lights 帧数; 可核对）。
              {"light_frames_probed", static_cast<uint64_t>(mu_light_probes.size())},
              {"applied_scale", mu_applied}};
  st_cal["master_unit_guard"] = mu_man;
  (*man)["master_unit_guard"] = mu_man;
  const bool dark_opt = doc.value("dark_optimization", false);
  const float k_fixed = doc.value("dark_scale_factor", 1.0f);
  // ── B2-A13 + BIAS-001: K 只要 dark 在位就必须由 FITS EXPTIME 推导 ──
  // SCI-CAL-001 §5（订正后）/ DATA_SEMANTICS §9.1 K 行: K = t_light/t_dark 由调用方
  // 从 FITS EXPTIME 计算后传入 ac_calibrate_frame，**标准式与兼容式都施加**；
  // 旧实现在标准分支强制 K=1.0 属缺陷（DISP-CAL-012）。dark 不在位时 K 不进入
  // 算术，不要求 EXPTIME（bias/flat-only 配置保持可运行）。缺失/非正/不匹配 →
  // DATA fail-closed（不得静默取 K=1）。
  const bool k_branch = dark.ok();
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
  // PERF-P1: 帧级并行（与 p2_parallel_for 同规范）。每帧只写自己下标的结果槽与
  // 自己帧的产物路径；跨帧无浮点归约；帧序归约（frames_ok/per_frame/artifacts）
  // 在 join 后按下标升序执行 ⇒ 与串行逐字一致、与 worker 数无关。
  // W/H 不再由循环内共享变量推进（那是帧间共享可变状态）：以母版尺寸（若有）或
  // **首帧**尺寸为基准逐帧独立校验，判据与串行相同且确定性。
  const uint32_t cal_workers = p1_frame_workers(doc);
  const size_t n_lights = lights.size();
  const int W_base = W, H_base = H;
  std::vector<Result<void>> f_err(n_lights, Result<void>::success());
  std::vector<Json> f_row(n_lights);
  std::vector<std::string> f_out(n_lights);
  std::vector<int> f_errkind(n_lights, 0);   // 0=无 1=input 2=output
  std::vector<int> f_wh(n_lights * 2, -1);
  p1_parallel_for(cal_workers, n_lights, p1_workers(doc), [&](uint64_t fi, uint32_t) {
    const std::string lp = lights[fi];
    P1Image light = p1_read_image(lp);
    if (!light.ok()) {
      f_errkind[fi] = 1;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO, "cannot read light: " + lp));
      return;
    }
    if (W_base >= 0) {
      if (light.w() != W_base || light.h() != H_base) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "light size mismatch vs masters: " + lp));
        return;
      }
    } else if (fi == 0) {
      f_wh[0] = light.w(); f_wh[1] = light.h();
    }
    const int W = (W_base >= 0) ? W_base : f_wh[0];
    const int H = (H_base >= 0) ? H_base : f_wh[1];
    if (light.w() != W || light.h() != H) {
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
          "light size mismatch vs first frame: " + lp));
      return;
    }
    // UNIT-001: 亮场声明域（若声明 normalized + scale，按声明换算；产物合同仍为 ADU）。
    if (mu_decl.light.has_scale) p1_apply_declared_scale(light, mu_decl.light.scale);
    const uint64_t n = static_cast<uint64_t>(W) * static_cast<uint64_t>(H);
    std::vector<float> out(static_cast<size_t>(n), 0.0f);
    float actual_k = 0.0f;
    float k_use = k_fixed;
    if (k_branch) {
      const AIOImageMetadata lmeta = aio_read_metadata(lp.c_str());
      const double t_light = lmeta.calibration.exptime;
      if (!std::isfinite(t_light) || t_light <= 0.0) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "light FITS EXPTIME missing/<=0 (required for K=t_light/t_dark): " + lp));
        return;
      }
      const double k_expo = t_light / dark_exptime;
      if (!std::isfinite(k_expo) || k_expo <= 0.0) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "K=t_light/t_dark invalid (non-finite/<=0) for " + lp));
        return;
      }
      // 显式配置标量不得与 EXPTIME 比不一致（禁配置冒充科学输入; 只做
      // fail-closed 门, 不改变 K 的推导公式与单位）。
      if (p1_has(doc, "dark_scale_factor")) {
        const double cfg_k = p1_num(doc, "dark_scale_factor", 1.0);
        if (std::fabs(cfg_k - k_expo) > 1e-6 * std::max(1.0, std::fabs(k_expo))) {
          f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
              "dark_scale_factor (" + std::to_string(cfg_k) +
              ") disagrees with FITS EXPTIME ratio K=" + std::to_string(k_expo)));
          return;
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
      f_err[fi] = Result<void>::fail(Error(rc == AC_ERR_MEMORY ? ErrorDomain::RESOURCE
                                                          : ErrorDomain::INTERNAL,
          "ac_calibrate_frame failed: " + lp));
      return;
    }
    // 写出 calibrated_<base>（与 p1_session 命名约定一致; CLI 按此收集 artifact）
    P1Image wim = P1Image(aio_read_fits(lp.c_str()));
    if (!wim.ok()) {
      (*man)["error_kind"] = "input";
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO, "re-read failed: " + lp));
      return;
    }
    if (static_cast<uint64_t>(wim.w()) * static_cast<uint64_t>(wim.h()) != n) {
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA, "re-read size mismatch: " + lp));
      return;
    }
    std::memcpy(wim.px(), out.data(), out.size() * sizeof(float));
    const std::string outp = out_dir + "/calibrated_" + p1_base_name(lp);
    // CORE-RACE-001: 原子发布（临时文件 + rename）——本路径是 drz/wcs 等并发
    // 消费者的共享输入, 任何时刻只允许存在完整文件。
    std::string werr;
    if (!p1_write_fits_atomic(wim, outp, &werr)) {
      (*man)["error_kind"] = "output";
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO, werr));
      return;
    }
    f_out[fi] = outp;
    // CONFORM-FIX-A ⑤ / CONFORM-SWEEP-1-008: dark_scale 必须记录**实际施加**的 K。
    // CALIBRATION_ALGORITHMS.md F3.1/F3.2 (:149,154) 规定两个分支都令 actual_k = k,
    // 且标准式 (dark_opt=0) 的 k = k_init = 调用方给出的 t_light/t_dark (不再强制 1.0)。
    // 旧实现在标准式写 k_fixed (默认 1.0) 而算术用 k_use ⇒ K≠1 时每帧溯源字段系统性
    // 错误 (B2-A13/BIAS-001 的判据面即此 manifest 字段)。
    f_row[fi] = Json{{"input", p1_base_name(lp)},
                     {"output", "calibrated_" + p1_base_name(lp)},
                     {"dark_scale", static_cast<double>(actual_k)}};
  });
  // 帧序归约（冻结顺序：下标升序；首个失败即返回，与串行同判据）
  for (size_t fi = 0; fi < n_lights; ++fi) {
    if (!f_err[fi].ok()) {
      if (f_errkind[fi] == 1) (*man)["error_kind"] = "input";
      else if (f_errkind[fi] == 2) (*man)["error_kind"] = "output";
      st_cal["status"] = "fail";
      return f_err[fi];
    }
    ++frames_ok;
    per_frame.push_back(f_row[fi]);
    artifacts.push_back(f_out[fi]);
  }
  st_cal["status"] = "ok";
  st_cal["frames"] = frames_ok;
  // BIAS-001: 标定参与面必须显式可见（E2E-D04 期望①）。
  // dark_convention 声明 master_dark 的本底约定；bias_participated 记录 master_bias
  // 是否真正进入算术（标准式缺 bias 时为 false ⇒ 本底未去除）。
  st_cal["dark_convention"] =
      dark_opt ? "master_dark_includes_bias_explicit_separation"
               : "master_dark_bias_subtracted";
  st_cal["bias_participated"] = bias.ok();
  st_cal["dark_participated"] = dark.ok();
  st_cal["flat_participated"] = flat.ok();
  if (dark.ok() && !bias.ok()) {
    // 不阻断（真实性优先于完备性），但必须留痕：标准式的 bias 项为 0。
    st_cal["optimize"] = Json::array({"master_bias 未提供：标准式 bias 项为 0，"
                                      "本底未去除（母版须与 light 同标度，见 SCI-CAL-001 §5/§6）"});
    std::fprintf(stderr,
                 "[calibrate] WARNING (recorded): master_dark 在位但 master_bias 缺失 — "
                 "标准式 bias 项为 0，本底未去除\n");
  }
  st_cal["per_frame"] = per_frame;

  // io_write 校验（产物存在性 fail-closed）
  Json& st_wr = (*man)["stages"].emplace_back(Json{{"name", "io_write"}, {"status", "running"}});
  for (const auto& a : artifacts) {
    if (!aio_fs::exists(a.get<std::string>())) {
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
  // PERF-P1: 帧级并行（与 p2_parallel_for 同规范）。每帧只写**自己下标**的结果槽与
  // **自己帧**的产物路径；跨帧无浮点归约；帧序归约（hot/cold/frames/artifacts）在
  // join 之后按下标升序执行 ⇒ 与串行逐位一致、与 worker 数无关。
  const uint32_t cos_workers = p1_frame_workers(doc);
  const size_t n_lights = doc["input_lights"].size();
  std::vector<Result<void>> f_err(n_lights, Result<void>::success());
  std::vector<int> f_hot(n_lights, 0), f_cold(n_lights, 0);
  std::vector<std::string> f_out(n_lights);
  std::vector<int> f_errkind(n_lights, 0);   // 0=无 1=input 2=output 3=internal
  p1_parallel_for(cos_workers, n_lights, p1_workers(doc), [&](uint64_t fi, uint32_t) {
    const std::string lp = doc["input_lights"][fi].get<std::string>();
    // 输入 = artifact:cal（cal 节点产物 calibrated_<base>, 无则原帧）
    const std::string in_path = p1_calibrated_path(doc, lp);
    P1Image im = p1_read_image(in_path);
    if (!im.ok()) {
      f_errkind[fi] = 1;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + in_path));
      return;
    }
    std::vector<float> fixed(static_cast<size_t>(im.w()) * static_cast<size_t>(im.h()), 0.0f);
    int hot = 0, cold = 0;
    const int rc = ac_correct_frame(im.px(), im.w(), im.h(), nullptr, nullptr,
                                    fixed.data(), hot_sigma, cold_sigma, method,
                                    mss, &hot, &cold);
    if (rc != AC_OK) {
      f_errkind[fi] = 3;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::INTERNAL,
          std::string("ac_correct_frame failed rc=") + std::to_string(rc)));
      return;
    }
    std::memcpy(im.px(), fixed.data(), fixed.size() * sizeof(float));
    // 输出 = artifact:cos（cleaned_<base>, 独立于上游 artifact:cal）+ 原子发布。
    // CORE-RACE-001: 修复前此处就地覆写 artifact:cal 路径 —— 与并发下游 drz
    // 读同一路径竞争, aio_write_fits 非原子 ⇒ 撕裂读（P1 数据完整性缺陷）。
    const std::string out_path = p1_cosmetic_path(doc, lp);
    std::string werr;
    if (!p1_write_fits_atomic(im, out_path, &werr)) {
      f_errkind[fi] = 2;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO, "cosmetic " + werr));
      return;
    }
    f_hot[fi] = hot;
    f_cold[fi] = cold;
    f_out[fi] = out_path;
  });
  // 帧序归约（冻结顺序：下标升序；首个失败即返回，与串行同判据）
  for (size_t fi = 0; fi < n_lights; ++fi) {
    if (!f_err[fi].ok()) {
      if (f_errkind[fi] == 1) (*man)["error_kind"] = "input";
      else if (f_errkind[fi] == 2) (*man)["error_kind"] = "output";
      st["status"] = "fail";
      return f_err[fi];
    }
    hot_total += f_hot[fi];
    cold_total += f_cold[fi];
    ++frames;
    artifacts.push_back(f_out[fi]);
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
// ── F-INSTR-CONFORM-FIX: Moffat4 (β=4) 整平面解析通量 ──────────────────────
// 规范依据: docs/science/PSF.md (SCI-PSF-001 FROZEN) §2/§3/§5/§9a:
//   I(r) = B + A/(1+Q)^4,  flux = 2πA·sxsy/3,  单位 ADU。
// 与 lib/algorithms/psf/src/dpsf_psf.cpp:428 的解析式**逐字同式**（那里算出的
// flux 因 psf_params 的 9 列 ABI（docs/contracts/PUBLIC_API.md:611 layout B）
// 无处承载而被丢弃）。本函数由该 ABI 的权威列 A,sx,sy 复算同一量: 不新增列、
// 不改 PSF 模块 ABI、不改任何科学公式/容差。
// 唯一合法用途 = SCI-PHOT-001 §9a 的 F_instr（星点通量来自 PSF 拟合域）。
inline double p1_psf_analytic_flux(double A, double sx, double sy) {
  return 2.0 * M_PI * A * sx * sy / 3.0;
}

// ════════════════════════════════════════════════════════════════════════════
// STARDET-01：星表引导检测（权威路径）节点级驱动
// 规范: ASTROCS_DESIGN.md §4.2（星表引导检测：用本帧 WCS 把 Gaia 星表逆投影到
//   像素域，只对星表位置做质心/PSF 拟合；拟合成功即星点，失败直接丢弃；按亮度
//   取 top 2–5 万为上限，极限星等按焦距、画幅、曝光时间派生估计，宁多勿少）
//   + §2.1 + docs/plugins/algorithms_phase1/03_star_detection.md §4。
// 分工: 权威路径 = 本块（星表逆投影 → sdet_detect_guided_ex_f64 逐位置拟合）；
//   全图盲检测 = **诊断/初值**路径（显式 star_detection.mode=blind_diagnostic，
//   或未配置参考星表时的自动降级 —— 降级必须逐帧落 degraded_reason 且
//   authoritative=false，不得静默）。
// fail-closed（Gaia 静默部分装载事故）:
//   ① gaia_data_dir 不可解析 → DATA 拒绝（权威路径不接受"无星表"）
//   ② gaia_client_create 返回 NULL → DATA 拒绝（含装载失败原因）
//   ③ file_count<=0 / file_load_fail_count!=0 / file_count!=xpsd 条目数 → DATA 拒绝
//   ④ 近似 WCS 不可解析（含板尺度非法）→ DATA 拒绝
//   ⑤ 圆锥查询 0 颗 / 帧内 0 个预测位置 → DATA 拒绝
//   ⑥ 引导拟合 0 颗 → DATA 拒绝（**不得**回退到全图盲检测冒充成功）
// ════════════════════════════════════════════════════════════════════════════

// P9 (F-10) 帧自有关键字初始指向（定义在下方 wcs 节点段; 本段前置声明）
bool p1_header_pointing(const P1Image& im, double* ra0, double* dec0,
                        double* focal_mm, double* pixel_um, double* s0,
                        std::string* src, std::string* why);

// P1-PHOT-BROKEN 的 WCS 天测可用性判定（定义在下方 wcs/phot 段; 本段前置声明）:
// 取向先验取自本帧解算产物时必须用**同一判据**确认天测可用（有限 crval + 非退化 CD）。
bool p1_wcs_astrometry_usable(const Json& wc, std::string* why);

// top 数合同域 = 2–5 万（ASTROCS_DESIGN.md §4.2）。越界即 DATA 拒绝，禁静默夹取。
static constexpr int kP1GuidedMaxStarsLo = 20000;
static constexpr int kP1GuidedMaxStarsHi = 50000;
// 206.265 = (180×3600)/π × 1e-3，与 kP9AsecPerUmPerMm / ipv_select.cpp:57
// IPV_ARCSEC_PER_UM_PER_MM 逐位一致（s0 = 206.265·XPIXSZ[μm]/FOCALLEN[mm]）。
static constexpr double kP1GuidedAsecPerUmPerMm = 206.265;

struct P1GuidedCfg {
  // mode: auto（默认；有参考星表 ⇒ 权威路径，否则显式降级并留痕）
  //     | catalog_guided（权威路径，星表缺失即 fail-closed）
  //     | blind_diagnostic（显式声明的非权威诊断路径）
  std::string mode = "auto";
  int max_stars = kP1GuidedMaxStarsLo;
  std::string gaia_dir;
  bool have_explicit_wcs = false;
  astrocs::phase1::WcsTan explicit_wcs;
  double rotation_deg = 0.0;
  bool have_rotation = false;
  std::string parity = "pos";     // pos | neg
  double limiting_mag = std::numeric_limits<double>::quiet_NaN();
  // 星等迭代/查询参数（默认值 = IPVSolverParams 成员默认，禁另立常数）
  ipv::IPVSolverParams ipv_params;
};

// 取向先验可用性（**配置级**判定，不需要图像）: 显式 CD 或 rotation_deg+parity
// 二者之一。星表逆投影必须知道像面取向与镜像; 实测（NGC55 T3 600s 真实帧,
// 4096²）取向错 90° 时仍有 88 个拟合通过全部质量门（真实场盲检测密度 ~5.7 源/
// 千像素 ⇒ 随机预测位置常落在真星上），0 星闸门抓不住 ⇒ 缺取向先验时不得声称权威。
bool p1_guided_has_orientation(const P1GuidedCfg& cfg) {
  return cfg.have_explicit_wcs || cfg.have_rotation;
}

// star_detection 段解析（唯一配置入口；top 数禁编译期硬编码，宪章 §10.4）。
// 返回 false 且写 *why = 配置非法（调用方 DATA fail-closed）。
bool p1_guided_cfg(const Json& doc, P1GuidedCfg* out, std::string* why) {
  const Json sd = (p1_has(doc, "star_detection") && doc["star_detection"].is_object())
                      ? doc["star_detection"] : Json::object();
  const Json wc = (p1_has(doc, "wcs") && doc["wcs"].is_object()) ? doc["wcs"] : Json::object();
  out->mode = sd.value("mode", std::string("auto"));
  if (out->mode != "auto" && out->mode != "catalog_guided" &&
      out->mode != "blind_diagnostic") {
    *why = "star_detection.mode must be one of auto|catalog_guided|blind_diagnostic (got '" +
           out->mode + "')";
    return false;
  }
  // 参考星表目录: star_detection.gaia_data_dir 优先, 回退 wcs.gaia_data_dir
  out->gaia_dir = sd.value("gaia_data_dir", std::string());
  if (out->gaia_dir.empty()) out->gaia_dir = wc.value("gaia_data_dir", std::string());
  // top-N 上限: 默认 = 合同下界; 显式给出时必须落在 [2万, 5万]
  const int max_stars = p1_int(sd, "max_stars", kP1GuidedMaxStarsLo);
  if (max_stars < kP1GuidedMaxStarsLo || max_stars > kP1GuidedMaxStarsHi) {
    *why = "star_detection.max_stars=" + std::to_string(max_stars) +
           " outside contract domain [" + std::to_string(kP1GuidedMaxStarsLo) + ", " +
           std::to_string(kP1GuidedMaxStarsHi) +
           "] (ASTROCS_DESIGN.md §4.2: top 2–5 万; 禁静默夹取)";
    return false;
  }
  out->max_stars = max_stars;
  // 显式近似 WCS（star_detection.approx_wcs 优先, 回退 wcs 段的天测键）。
  // B3 fail-closed：approx_wcs **显式给出**时六键必须齐备 —— 缺键不得静默回退到
  // wcs 段（那会把用户输入静默丢弃，违 docs/contracts/LOG_AND_ERROR_CONTRACT.md §6
  // 「禁止静默回退/静默跳过校验」）。合同声明 =
  // eng/contracts/schemas/phase_config_normalize.schema.json#/$defs/star_detection_config
  // （approx_wcs.required = 六天测键）。
  if (p1_has(sd, "approx_wcs")) {
    if (!sd["approx_wcs"].is_object()) {
      *why = "star_detection.approx_wcs must be an object with the six astrometric keys "
             "crval1/crval2/cd11/cd12/cd21/cd22";
      return false;
    }
    for (const char* k : {"crval1", "crval2", "cd11", "cd12", "cd21", "cd22"}) {
      if (!p1_has(sd["approx_wcs"], k)) {
        *why = std::string("star_detection.approx_wcs missing key '") + k +
               "' (six astrometric keys must be complete; silent fallback to the wcs "
               "section is forbidden)";
        return false;
      }
    }
  }
  const Json aw = (p1_has(sd, "approx_wcs") && sd["approx_wcs"].is_object())
                      ? sd["approx_wcs"] : wc;
  if (p1_has(aw, "crval1") && p1_has(aw, "crval2") && p1_has(aw, "cd11") &&
      p1_has(aw, "cd12") && p1_has(aw, "cd21") && p1_has(aw, "cd22")) {
    out->explicit_wcs.crval1 = p1_num(aw, "crval1", 0.0);
    out->explicit_wcs.crval2 = p1_num(aw, "crval2", 0.0);
    out->explicit_wcs.cd11 = p1_num(aw, "cd11", 0.0);
    out->explicit_wcs.cd12 = p1_num(aw, "cd12", 0.0);
    out->explicit_wcs.cd21 = p1_num(aw, "cd21", 0.0);
    out->explicit_wcs.cd22 = p1_num(aw, "cd22", 0.0);
    const double det = out->explicit_wcs.cd11 * out->explicit_wcs.cd22 -
                       out->explicit_wcs.cd12 * out->explicit_wcs.cd21;
    if (!std::isfinite(out->explicit_wcs.crval1) || !std::isfinite(out->explicit_wcs.crval2) ||
        !std::isfinite(det) || !(std::fabs(det) > 0.0)) {
      *why = "star_detection approximate WCS invalid (non-finite crval or degenerate CD)";
      return false;
    }
    out->have_explicit_wcs = true;
  }
  if (p1_has(sd, "rotation_deg")) {
    out->rotation_deg = p1_num(sd, "rotation_deg", 0.0);
    out->have_rotation = true;
    if (!std::isfinite(out->rotation_deg)) {
      *why = "star_detection.rotation_deg must be finite";
      return false;
    }
  }
  out->parity = sd.value("parity", std::string("pos"));
  if (out->parity != "pos" && out->parity != "neg") {
    *why = "star_detection.parity must be pos|neg (got '" + out->parity + "')";
    return false;
  }
  if (p1_has(sd, "limiting_mag")) {
    out->limiting_mag = p1_num(sd, "limiting_mag", std::numeric_limits<double>::quiet_NaN());
    if (!std::isfinite(out->limiting_mag)) {
      *why = "star_detection.limiting_mag must be finite when given";
      return false;
    }
  }
  // 星等迭代参数可调（默认 = IPVSolverParams 成员默认值；非法值由
  // estimate_mag_lim_iterative 自身 fail-closed）
  out->ipv_params.m_lim_safety = p1_num(sd, "limiting_mag_safety", out->ipv_params.m_lim_safety);
  out->ipv_params.gaia_query_radius_factor =
      p1_num(sd, "query_radius_factor", out->ipv_params.gaia_query_radius_factor);
  out->ipv_params.m_lim_max_iter = p1_int(sd, "limiting_mag_max_iter", out->ipv_params.m_lim_max_iter);
  return true;
}

// 本帧解算产物（<frame_dir>/p1_wcs.json, DATA-P1-WCS）→ 逆投影先验。
// 只取线性部分（crpix/crval/CD）: 与显式 CD 分支同口径（预测位置的高阶 SIP 项
// 不参与, 登记为已知残差）。天测可用性用与 photometry 同一判据确认。
// 任一不可得 → false + *why（调用方按 mode 语义降级或 fail-closed）。
bool p1_guided_wcs_product_prior(const Json& wprod, astrocs::phase1::WcsTan* out,
                                 std::string* why) {
  if (!wprod.is_object() || !p1_has(wprod, "wcs") || !wprod["wcs"].is_object()) {
    *why = "p1_wcs.json carries no 'wcs' object";
    return false;
  }
  const Json& w = wprod["wcs"];
  if (!p1_wcs_astrometry_usable(w, why)) return false;
  const double crpix1 = p1_num(w, "crpix1", std::numeric_limits<double>::quiet_NaN());
  const double crpix2 = p1_num(w, "crpix2", std::numeric_limits<double>::quiet_NaN());
  if (!std::isfinite(crpix1) || !std::isfinite(crpix2)) {
    *why = "p1_wcs.json reference pixel (crpix) missing/non-finite";
    return false;
  }
  out->crpix1 = crpix1;
  out->crpix2 = crpix2;
  out->crval1 = w["crval1"].get<double>();
  out->crval2 = w["crval2"].get<double>();
  out->cd11 = w["cd11"].get<double>();
  out->cd12 = w["cd12"].get<double>();
  out->cd21 = w["cd21"].get<double>();
  out->cd22 = w["cd22"].get<double>();
  return true;
}

// 近似 WCS 解析（权威路径逆投影的锚）。来源优先级:
//   ① star_detection.approx_wcs / wcs 段的天测键（crval1/2 + CD，方向完整）——
//      调用方显式给定的先验, 最高优先（诊断/负例注入亦走此路）。
//   ② **本帧解算产物** <frame_dir>/p1_wcs.json（psf 节点排在 wcs 之后, 由 IR 的
//      typed 边 artifact:p1_wcs 保证先落盘）—— 生产默认先验: 含完整取向, 无需
//      调用方预先知道像面取向, 且经求解器 roundtrip/前向交叉绝对门。
//   ③ wcs.init_source 的初始指向（header_pointing|config|neighbor_crval）+
//      板尺度 s0=206.265·XPIXSZ/FOCALLEN + 方向（rotation_deg/parity；缺省
//      "北向上/东向左" ⇒ *assumed=true 留痕，不静默当已知）。
// 任一不可得 → false + *why（调用方 DATA fail-closed）。
// solved: 该帧已解出的 WCS（nullptr = 该帧无产物先验）。
bool p1_guided_approx_wcs(const P1GuidedCfg& cfg, const Json& doc, const P1Image& im,
                          const astrocs::phase1::WcsTan* solved,
                          astrocs::phase1::WcsTan* wcs, double* out_focal_mm,
                          double* out_pixel_um, double* out_s0, std::string* src,
                          bool* assumed, std::string* why) {
  *assumed = false;
  double ra0 = std::numeric_limits<double>::quiet_NaN();
  double dec0 = std::numeric_limits<double>::quiet_NaN();
  double focal_mm = std::numeric_limits<double>::quiet_NaN();
  double pixel_um = std::numeric_limits<double>::quiet_NaN();
  double s0 = std::numeric_limits<double>::quiet_NaN();
  std::string center_src;
  if (cfg.have_explicit_wcs) {
    *wcs = cfg.explicit_wcs;
    wcs->crpix1 = 0.5 * (double)im.w() + 0.5;   // FITS 1-based 中心
    wcs->crpix2 = 0.5 * (double)im.h() + 0.5;
    // 显式 WCS 的板尺度 = |CD 第 1 列|（deg/px → ″/px）；focal/pixel 由 s0 反演
    // 只为把同一物理量喂给 compute_fov_density（等价换算，不引入新常数）。
    s0 = std::sqrt(wcs->cd11 * wcs->cd11 + wcs->cd21 * wcs->cd21) * 3600.0;
    if (!std::isfinite(s0) || !(s0 > 0.0)) {
      *why = "star_detection approximate WCS degenerate (|CD column 1| = 0)";
      return false;
    }
    pixel_um = 1.0;
    focal_mm = kP1GuidedAsecPerUmPerMm / s0;
    *out_focal_mm = focal_mm;
    *out_pixel_um = pixel_um;
    *out_s0 = s0;
    *src = "config_astrometric_keys";
    return true;
  }
  if (solved != nullptr) {
    // ② 本帧解算产物: 完整线性 WCS（含取向与镜像）, 无需假设。
    *wcs = *solved;
    // 板尺度同显式分支口径 = |CD 第 1 列|（deg/px → ″/px）; focal/pixel 由 s0 反演,
    // 只为把同一物理量喂给 compute_fov_density（等价换算，不引入新常数）。
    s0 = std::sqrt(wcs->cd11 * wcs->cd11 + wcs->cd21 * wcs->cd21) * 3600.0;
    if (!std::isfinite(s0) || !(s0 > 0.0)) {
      *why = "solved WCS product degenerate (|CD column 1| = 0)";
      return false;
    }
    pixel_um = 1.0;
    focal_mm = kP1GuidedAsecPerUmPerMm / s0;
    *out_focal_mm = focal_mm;
    *out_pixel_um = pixel_um;
    *out_s0 = s0;
    *src = "solved_wcs_product(<frame_dir>/p1_wcs.json)";
    return true;   // 产物含取向 ⇒ *assumed 保持 false（不假设、不冒充）
  }
  const Json wc = (p1_has(doc, "wcs") && doc["wcs"].is_object()) ? doc["wcs"] : Json::object();
  const std::string init_source = wc.value("init_source", std::string("header_pointing"));
  if (init_source == "header_pointing") {
    if (!p1_header_pointing(im, &ra0, &dec0, &focal_mm, &pixel_um, &s0, &center_src, why)) {
      return false;
    }
  } else if (init_source == "config" || init_source == "neighbor_crval") {
    focal_mm = p1_num(wc, "focal_length_mm", std::numeric_limits<double>::quiet_NaN());
    pixel_um = p1_num(wc, "pixel_size_um", std::numeric_limits<double>::quiet_NaN());
    if (!std::isfinite(focal_mm) || !std::isfinite(pixel_um) ||
        !(focal_mm > 0.0) || !(pixel_um > 0.0)) {
      *why = "wcs.init_source=" + init_source +
             " requires config focal_length_mm/pixel_size_um (no silent defaults)";
      return false;
    }
    s0 = kP1GuidedAsecPerUmPerMm * pixel_um / focal_mm;
    if (init_source == "config") {
      ra0 = p1_num(wc, "ra0", ra0);
      dec0 = p1_num(wc, "dec0", dec0);
      center_src = "config.wcs.ra0/dec0";
    } else {
      ra0 = p1_num(wc, "neighbor_ra0", ra0);
      dec0 = p1_num(wc, "neighbor_dec0", dec0);
      center_src = "config.wcs.neighbor_ra0/neighbor_dec0(own_solved_product)";
    }
  } else {
    *why = "wcs.init_source must be one of header_pointing|config|neighbor_crval (got '" +
           init_source + "')";
    return false;
  }
  if (!std::isfinite(ra0) || !std::isfinite(dec0)) {
    *why = "approx WCS center unavailable (ra0/dec0 missing; 禁 silent default)";
    return false;
  }
  if (!std::isfinite(s0) || !(s0 > 0.0)) {
    *why = "approx WCS plate scale unavailable (s0=206.265*XPIXSZ/FOCALLEN invalid)";
    return false;
  }
  const double s_deg = s0 / 3600.0;
  // 缺省取向 = 北向上/东向左（本管线像素 y 沿行号向下 ⇒ CD2_2 取负）；
  // rotation_deg 为像面相对该取向的旋转（逆时针为正）。
  if (!cfg.have_rotation) *assumed = true;
  const double th = cfg.rotation_deg * M_PI / 180.0;
  const double ct = std::cos(th), st = std::sin(th);
  const double par = (cfg.parity == "neg") ? -1.0 : 1.0;
  const double cd0_11 = -s_deg, cd0_12 = 0.0, cd0_21 = 0.0, cd0_22 = -s_deg * par;
  wcs->crpix1 = 0.5 * (double)im.w() + 0.5;
  wcs->crpix2 = 0.5 * (double)im.h() + 0.5;
  wcs->crval1 = ra0;
  wcs->crval2 = dec0;
  wcs->cd11 = ct * cd0_11 - st * cd0_21;
  wcs->cd12 = ct * cd0_12 - st * cd0_22;
  wcs->cd21 = st * cd0_11 + ct * cd0_21;
  wcs->cd22 = st * cd0_12 + ct * cd0_22;
  *out_focal_mm = focal_mm;
  *out_pixel_um = pixel_um;
  *out_s0 = s0;
  *src = "init_pointing(" + center_src + ")+s0=206.265*XPIXSZ/FOCALLEN" +
         (cfg.have_rotation ? "+rotation_deg" : "+orientation_assumed_north_up_east_left") +
         (cfg.parity == "neg" ? "+parity_neg" : "");
  return true;
}

// 星表引导预测: 极限星等派生（焦距/画幅/曝光）→ Gaia 圆锥查询 →
// 逆投影到像素域 → 按亮度（G 星等升序）取 top cfg.max_stars。
// prov 逐项记录派生输入（可审计）；失败写 *why（调用方 DATA fail-closed）。
bool p1_guided_predict(const P1GuidedCfg& cfg, const P1Image& im, const std::string& frame_path,
                       const astrocs::phase1::WcsTan& wcs, double focal_mm, double pixel_um,
                       GaiaClient* gaia,
                       std::vector<double>* px, std::vector<double>* py, Json* prov,
                       std::string* why) {
  px->clear();
  py->clear();
  const AIOImageMetadata meta = aio_read_metadata(frame_path.c_str());
  const double exptime = meta.calibration.exptime;
  if (!std::isfinite(exptime) || !(exptime > 0.0)) {
    *why = "frame EXPTIME missing/<=0 (极限星等派生需要曝光时间): " + frame_path;
    return false;
  }
  // 板尺度与视场（画幅）: 复用 ipv 冻结实现 compute_fov_density
  // （206.265″/px/μm/mm、query_radius = fov_diag × factor）。n_img_bright=0:
  // 本路径不需要图像面密度（只取 s0 / fov_diag / query_radius）。
  double s0 = 0.0, fov_diag_deg = 0.0, query_radius_deg = 0.0, q_area = 0.0;
  double img_area = 0.0, rho_img = 0.0, rho_target = 0.0;
  int n_target_unused = 0;
  ipv::compute_fov_density(focal_mm, pixel_um, (double)im.w(), (double)im.h(),
                           0, 1.5, cfg.ipv_params.gaia_query_radius_factor,
                           s0, fov_diag_deg, query_radius_deg, q_area, img_area,
                           rho_img, rho_target, n_target_unused, nullptr);
  if (!std::isfinite(query_radius_deg) || !(query_radius_deg > 0.0)) {
    *why = "query radius degenerate (fov_diag=0)";
    return false;
  }
  // 极限星等派生: 割线迭代（初值 = 6 + 1.5·log10(f) + 2·log10(t) + offset，
  // N_target = max_stars × safety）—— 目标 = 帧内星表星数达到 top-N 上限，
  // 宁多勿少由 safety(默认 3.0) 承担；显式 limiting_mag 直接采用。
  std::vector<double> cat_ra, cat_dec;
  std::vector<float> cat_mag;
  double m_lim = 0.0;
  int n_query = 0;
  bool converged = false, capped = false;
  if (std::isfinite(cfg.limiting_mag)) {
    m_lim = cfg.limiting_mag;
    int n_ret = 0;
    double *ra_p = nullptr, *dec_p = nullptr;
    float *mag_p = nullptr;
    if (gaia_client_cone_search_for_solver(gaia, wcs.crval1, wcs.crval2, query_radius_deg,
                                           m_lim, &ra_p, &dec_p, &mag_p, &n_ret) != 0) {
      if (ra_p) free(ra_p);
      if (dec_p) free(dec_p);
      if (mag_p) free(mag_p);
      *why = "gaia cone search failed (explicit limiting_mag)";
      return false;
    }
    if (n_ret > 0) {
      cat_ra.assign(ra_p, ra_p + n_ret);
      cat_dec.assign(dec_p, dec_p + n_ret);
      cat_mag.assign(mag_p, mag_p + n_ret);
    }
    if (ra_p) free(ra_p);
    if (dec_p) free(dec_p);
    if (mag_p) free(mag_p);
    n_query = 1;
    converged = true;
  } else {
    auto query_fn = [&](double mag, int& n_returned) -> int {
      double *ra_p = nullptr, *dec_p = nullptr;
      float *mag_p = nullptr;
      int n_ret = 0;
      const int rc = gaia_client_cone_search_for_solver(
          gaia, wcs.crval1, wcs.crval2, query_radius_deg, mag, &ra_p, &dec_p, &mag_p, &n_ret);
      if (rc == 0) {
        n_returned = n_ret;
        cat_ra.assign(n_ret > 0 ? ra_p : nullptr, n_ret > 0 ? ra_p + n_ret : nullptr);
        cat_dec.assign(n_ret > 0 ? dec_p : nullptr, n_ret > 0 ? dec_p + n_ret : nullptr);
        cat_mag.assign(n_ret > 0 ? mag_p : nullptr, n_ret > 0 ? mag_p + n_ret : nullptr);
      }
      if (ra_p) free(ra_p);
      if (dec_p) free(dec_p);
      if (mag_p) free(mag_p);
      return rc;
    };
    const ipv::MagIterOutcome mi = ipv::estimate_mag_lim_iterative(
        query_fn, cfg.max_stars, focal_mm, exptime, cfg.ipv_params, nullptr);
    if (!mi.valid) {
      *why = "limiting magnitude derivation failed (no successful gaia query; "
             "focal=" + std::to_string(focal_mm) + "mm exptime=" + std::to_string(exptime) + "s)";
      return false;
    }
    m_lim = mi.m_lim_final;
    n_query = mi.query_count;
    converged = mi.converged;
    capped = mi.capped;
  }
  if (cat_ra.empty()) {
    *why = "gaia cone search returned 0 stars (m_lim=" + std::to_string(m_lim) +
           ", radius=" + std::to_string(query_radius_deg) + " deg)";
    return false;
  }
  // 逆投影到像素域（本帧近似 WCS 的 sky→pix），按亮度取 top-N
  std::vector<int> order(cat_ra.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = (int)i;
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    const float ma = cat_mag[(size_t)a], mb = cat_mag[(size_t)b];
    if (ma != mb) return ma < mb;   // G 星等升序 = 亮度降序
    return a < b;
  });
  int n_in_frame = 0;
  px->reserve((size_t)cfg.max_stars);
  py->reserve((size_t)cfg.max_stars);
  for (int k : order) {
    if ((int)px->size() >= cfg.max_stars) break;
    double x = 0.0, y = 0.0;
    wcs.sky2pix((double)cat_ra[(size_t)k], (double)cat_dec[(size_t)k], &x, &y);
    if (!std::isfinite(x) || !std::isfinite(y)) continue;
    const double xi = x - 1.0;   // FITS 1-based → 0-based 数组下标（索引即中心）
    const double yi = y - 1.0;
    // 边界: 距边界 <2px 允许丢弃（SCI-P1-STAR-001 §1，与引导拟合同判据）
    if (xi < 2.0 || yi < 2.0 || xi > (double)im.w() - 3.0 || yi > (double)im.h() - 3.0) continue;
    px->push_back(xi);
    py->push_back(yi);
    n_in_frame++;
  }
  if (px->empty()) {
    *why = "0 catalog stars project inside the frame (approx WCS/pointing wrong or field empty; "
           "n_catalog=" + std::to_string(cat_ra.size()) + ")";
    return false;
  }
  *prov = Json{{"catalog_stars_queried", (int64_t)cat_ra.size()},
               {"catalog_stars_in_frame", n_in_frame},
               {"limiting_mag", m_lim},
               {"limiting_mag_explicit", std::isfinite(cfg.limiting_mag)},
               {"limiting_mag_iterations", n_query},
               {"limiting_mag_converged", converged},
               {"limiting_mag_capped", capped},
               {"limiting_mag_safety", cfg.ipv_params.m_lim_safety},
               {"max_stars", cfg.max_stars},
               {"s0_arcsec_px", s0},
               {"fov_diag_deg", fov_diag_deg},
               {"query_radius_deg", query_radius_deg},
               {"exptime_s", exptime}};
  return true;
}

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
  // ── STARDET-01: 检测模式解析（权威 = 星表引导拟合）──────────────────────────
  // 依据 ASTROCS_DESIGN.md §4.2 + docs/plugins/algorithms_phase1/03_star_detection.md §4。
  // 节点级一次解析（禁逐帧重复），模式与 fail-closed 语义见 p1_guided_cfg 注释块。
  P1GuidedCfg gcfg;
  {
    std::string why;
    if (!p1_guided_cfg(doc, &gcfg, &why)) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "star_detection config invalid: " + why));
    }
  }
  // ── 取向先验来源 ②: 本帧解算产物 <frame_dir>/p1_wcs.json ────────────────────
  // psf 节点排在 wcs 节点之后（IR typed 边 artifact:p1_wcs, 机器断言见
  // eng/ci/check_registry_ir_parity.py C4–C8）, 故此处按帧读到的就是**本帧自解** WCS。
  // 逐帧读一次并缓存（节点级一次解析, 禁逐帧重复 IO）; 只在可能走权威路径时读
  // （blind_diagnostic 显式声明、或无参考星表时不可能走权威路径 ⇒ 不读）。
  // 任一帧缺产物/产物非法 ⇒ 该帧无产物先验, 由 star_detection.mode 语义决定显式
  // 降级（auto）或 fail-closed（catalog_guided）; **不得**以缺省"北向上/东向左"冒充权威。
  const size_t n_frames = doc["input_lights"].size();
  std::vector<Json> solved_wcs_doc(n_frames);
  std::vector<bool> have_solved_wcs(n_frames, false);
  std::string solved_wcs_why;
  const bool need_wcs_prior = (gcfg.mode != "blind_diagnostic") && !gcfg.gaia_dir.empty();
  for (size_t i = 0; need_wcs_prior && i < n_frames; ++i) {
    const std::string lp = doc["input_lights"][i].get<std::string>();
    const std::string wpath = p1_frame_dir(doc, lp) + "/p1_wcs.json";
    std::string wtext;
    if (!aio_fs::read_all(wpath, &wtext)) {
      solved_wcs_why = "no solved WCS product at " + wpath;
      continue;
    }
    Json wprod;
    try {
      wprod = Json::parse(wtext);
    } catch (const Json::exception& e) {
      solved_wcs_why = std::string("p1_wcs.json parse failed (") + e.what() + "): " + wpath;
      continue;
    }
    astrocs::phase1::WcsTan probe;
    std::string pwhy;
    if (!p1_guided_wcs_product_prior(wprod, &probe, &pwhy)) {
      solved_wcs_why = "p1_wcs.json unusable (" + pwhy + "): " + wpath;
      continue;
    }
    solved_wcs_doc[i] = std::move(wprod);
    have_solved_wcs[i] = true;
  }
  bool have_solved_all = need_wcs_prior && (n_frames > 0);
  for (size_t i = 0; i < n_frames; ++i) {
    if (!have_solved_wcs[i]) { have_solved_all = false; break; }
  }

  std::string eff_mode = gcfg.mode;
  std::string degrade_reason;
  const bool cfg_orient = p1_guided_has_orientation(gcfg);
  const bool have_orient = cfg_orient || have_solved_all;
  if (eff_mode == "auto") {
    if (gcfg.gaia_dir.empty()) {
      // 显式降级（**不是**静默）：逐帧落 degraded_reason + authoritative=false。
      eff_mode = "blind_diagnostic";
      degrade_reason = "no reference catalog configured (star_detection.gaia_data_dir / "
                       "wcs.gaia_data_dir absent): authoritative catalog-guided detection "
                       "unavailable, falling back to the declared non-authoritative blind path";
      std::fprintf(stderr, "[star-psf] WARNING (recorded): %s\n", degrade_reason.c_str());
    } else if (!have_orient) {
      // 有星表但**两种**取向先验都不可得: 逆投影只能假设"北向上/东向左", 而实测该
      // 假设在真实密集场上仍能产出少量通过全部质量门的假源（随机位置落在真星上），
      // 0 星闸门抓不住 ⇒ 会以错误天球位置冒充权威星表。auto 模式按"显式降级 + 留痕"处理。
      eff_mode = "blind_diagnostic";
      degrade_reason = "reference catalog configured but no orientation prior: neither the "
                       "solved WCS product <frame_dir>/p1_wcs.json (" + solved_wcs_why + ") "
                       "nor star_detection.approx_wcs {crval1,crval2,cd11,cd12,cd21,cd22} / "
                       "star_detection.rotation_deg+parity is available; catalog inverse "
                       "projection would use an assumed north-up/east-left orientation; "
                       "falling back to the declared non-authoritative blind path";
      std::fprintf(stderr, "[star-psf] WARNING (recorded): %s\n", degrade_reason.c_str());
    } else {
      eff_mode = "catalog_guided";
    }
  } else if (eff_mode == "catalog_guided") {
    if (gcfg.gaia_dir.empty()) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "star_detection.mode=catalog_guided requires gaia_data_dir "
          "(star_detection.gaia_data_dir or wcs.gaia_data_dir): refusing to degrade to "
          "full-frame blind detection (ASTROCS_DESIGN.md §4.2)"));
    }
    if (!have_orient) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "star_detection.mode=catalog_guided requires an orientation prior: the solved WCS "
          "product <frame_dir>/p1_wcs.json is unavailable for every frame (" + solved_wcs_why +
          ") and neither star_detection.approx_wcs {crval1,crval2,cd11,cd12,cd21,cd22} (or the "
          "same keys under the wcs section) nor star_detection.rotation_deg + "
          "star_detection.parity is configured; refusing to inverse-project the catalog with "
          "an assumed orientation (ASTROCS_DESIGN.md §4.2: 权威路径必须高纯度, 不得以假设取向冒充)"));
    }
  }
  const bool guided = (eff_mode == "catalog_guided");
  // Gaia 句柄（权威路径必需）。fail-closed 三道断言与 wcs 节点同款
  // （静默部分装载事故不得重演）。
  GaiaClient* gaia = nullptr;
  Json gaia_prov = Json::object();
  if (guided) {
    gaia = gaia_client_create(gcfg.gaia_dir.c_str());
    if (!gaia) {
      const char* why = gaia_client_get_last_create_error();
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "gaia_client_create failed (gaia_data_dir=" + gcfg.gaia_dir + ")" +
          (why ? std::string(": ") + why : std::string(": (no diagnostic)"))));
    }
    const int gaia_files = gaia_client_get_file_count(gaia);
    const int gaia_entries = gaia_client_get_file_entry_count(gaia);
    const int gaia_load_failed = gaia_client_get_file_load_fail_count(gaia);
    if (gaia_files <= 0) {
      gaia_client_destroy(gaia);
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "gaia catalog is empty (gaia_data_dir=" + gcfg.gaia_dir +
          ", xpsd_entries=" + std::to_string(gaia_entries) +
          "): refusing catalog-guided detection without a reference catalog"));
    }
    if (gaia_load_failed != 0 || gaia_files != gaia_entries) {
      gaia_client_destroy(gaia);
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "gaia catalog is incomplete: loaded=" + std::to_string(gaia_files) +
          " xpsd_entries=" + std::to_string(gaia_entries) +
          " load_failed=" + std::to_string(gaia_load_failed) +
          " (gaia_data_dir=" + gcfg.gaia_dir +
          "): refusing catalog-guided detection on a partial catalog"));
    }
    gaia_prov = Json{{"gaia_data_dir", gcfg.gaia_dir},
                     {"xpsd_entries", gaia_entries},
                     {"xpsd_loaded", gaia_files},
                     {"xpsd_load_failed", gaia_load_failed}};
  }
  // PERF-P1: 帧级并行（与 p2_parallel_for 同规范）。每帧只写**自己下标**的结果槽与
  // 自己帧的统计池；跨帧无浮点归约；帧序归约（frames / fwhm 池 / 计数）在 join 之后
  // 按下标升序执行 ⇒ 与串行逐字一致、与 worker 数无关。
  // StarDetector 持检测期内部状态（背景估计与临时缓冲）⇒ **必须按 worker 分实例**，
  // 不得跨 worker 共享；每个检测器只被其所属 worker 写入自己的槽。
  const uint32_t sp_workers = p1_frame_workers(doc);
  const size_t n_lights = doc["input_lights"].size();
  // 权威路径的 sdet 句柄同样按 worker 分实例（handle 级互斥, 禁跨线程共享）
  std::vector<StarDetectorHandle> ghandles(sp_workers, nullptr);
  auto gcleanup = [&]() {
    for (StarDetectorHandle& h : ghandles) {
      if (h) { sdet_destroy(h); h = nullptr; }
    }
    if (gaia) { gaia_client_destroy(gaia); gaia = nullptr; }
  };
  std::vector<Result<void>> f_err(n_lights, Result<void>::success());
  std::vector<int> f_errkind(n_lights, 0);   // 0=无 1=input
  std::vector<Json> f_frame(n_lights);
  std::vector<std::vector<double>> f_fx(n_lights), f_fy(n_lights), f_ell(n_lights);
  std::vector<int64_t> f_nvalid(n_lights, 0), f_ntotal(n_lights, 0), f_nfit(n_lights, 0);
  std::vector<std::unique_ptr<astrocs::phase1::StarDetector>> dets(sp_workers);
  p1_parallel_for(sp_workers, n_lights, p1_workers(doc), [&](uint64_t fi, uint32_t w) {
    if (!dets[w]) dets[w] = std::make_unique<astrocs::phase1::StarDetector>(5.0);
    const astrocs::phase1::StarDetector& det = *dets[w];
    // IR 输入端口 = artifact:cos → 消费 cosmetic 节点产物（CORE-RACE-001 接线）
    const std::string path =
        p1_cleaned_input_path(doc, doc["input_lights"][fi].get<std::string>());
    P1Image im = p1_read_image(path);
    if (!im.ok()) {
      f_errkind[fi] = 1;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + path));
      return;
    }
    // ── 检测：权威路径（星表引导拟合）或显式声明的诊断路径（全图盲检测）──────
    astrocs::phase1::StarCatalog cat;
    Json guided_prov = Json::object();
    std::string det_mode = eff_mode;
    bool authoritative = guided;
    std::string det_degraded = degrade_reason;
    // FP64 图像缓冲（引导检测与 PSF 拟合共用一份）
    std::vector<double> dbuf(static_cast<size_t>(im.w()) * static_cast<size_t>(im.h()));
    for (size_t i = 0; i < dbuf.size(); ++i) dbuf[i] = static_cast<double>(im.px()[i]);
    if (guided) {
      // ① 逆投影先验: 显式配置 CD > 本帧解算产物 > 初始指向(+rotation/parity)
      astrocs::phase1::WcsTan awcs;
      astrocs::phase1::WcsTan solved_prior;
      const astrocs::phase1::WcsTan* solved_ptr = nullptr;
      if (have_solved_wcs[fi]) {
        std::string pwhy;
        if (!p1_guided_wcs_product_prior(solved_wcs_doc[fi], &solved_prior, &pwhy)) {
          f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
              "catalog_guided: solved WCS product unusable (" + pwhy + "): " + path));
          return;
        }
        solved_ptr = &solved_prior;
      }
      std::string wsrc, why;
      bool orientation_assumed = false;
      double g_focal = 0.0, g_pixel = 0.0, g_s0 = 0.0;
      if (!p1_guided_approx_wcs(gcfg, doc, im, solved_ptr, &awcs, &g_focal, &g_pixel, &g_s0,
                                &wsrc, &orientation_assumed, &why)) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "catalog_guided: approx WCS unavailable (" + why + "): " + path));
        return;
      }
      // ② 星表查询 + 逆投影到像素域 + 按亮度取 top max_stars
      std::vector<double> px, py;
      Json pred_prov;
      if (!p1_guided_predict(gcfg, im, path, awcs, g_focal, g_pixel, gaia,
                             &px, &py, &pred_prov, &why)) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "catalog_guided: prediction failed (" + why + "): " + path));
        return;
      }
      // ③ 帧级背景/噪声（与盲路径同一估计器 ⇒ p1_sources.json 同列语义）
      double g_bg = 0.0, g_sigma = 0.0;
      if (!astrocs::phase1::StarDetector::estimate_background(
              im.px(), im.w(), im.h(), &g_bg, &g_sigma)) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "catalog_guided: background estimation failed: " + path));
        return;
      }
      // ④ 权威拟合：只对星表预测位置做质心/椭圆高斯拟合，失败直接丢弃
      if (!ghandles[w]) {
        SDetParams sp;
        std::memset(&sp, 0, sizeof(sp));
        sp.structureLayers = 5;
        sp.hotPixelFilterRadius = 2;
        sp.iterativeClipSigma = 5.0f;
        sp.iterativeMaxRounds = 3;
        sp.medianFilterDetail = 2;
        sp.maxStars = gcfg.max_stars;   // = top-N 合同域 [2万,5万]
        sp.fitRadius = 0;
        sp.fwhmClipSigma = 3.0f;
        sp.maxAxisRatio = 2.0f;
        ghandles[w] = sdet_create(&sp);
        if (!ghandles[w]) {
          f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA, "sdet_create failed"));
          return;
        }
      }
      const char* g_extras[4] = {"fwhm_x", "fwhm_y", "sx", "sy"};
      double *gx = nullptr, *gy = nullptr;
      float *gflux = nullptr, *gmag = nullptr;
      int *gsat = nullptr, *ghas = nullptr, g_count = 0;
      float **gex = nullptr;
      SDetGuidedStats gstats;
      std::memset(&gstats, 0, sizeof(gstats));
      const int grc = sdet_detect_guided_ex_f64(
          ghandles[w], dbuf.data(), im.w(), im.h(), px.data(), py.data(),
          static_cast<int>(px.size()),
          &gx, &gy, &gflux, &gsat, &gmag, &ghas, &g_count,
          g_extras, 4, &gex, &gstats);
      if (grc != 0) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "sdet_detect_guided_ex_f64 failed rc=" + std::to_string(grc) + ": " + path));
        return;
      }
      // ⑤ 权威路径 0 星 ⇒ fail-closed（**不得**回退全图盲检测冒充成功）
      if (g_count <= 0) {
        sdet_free_detect_ex(gx, gy, gflux, gsat, gmag, ghas, gex, 4);
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "catalog_guided: 0/" + std::to_string(gstats.n_predicted) +
            " catalog-guided fits survived (dropped=" + std::to_string(gstats.n_dropped) +
            ", fit_failed=" + std::to_string(gstats.n_fit_failed) +
            ", rejected=" + std::to_string(gstats.n_rejected) +
            "); refusing to fall back to full-frame blind detection: " + path));
        return;
      }
      cat.background = g_bg;
      cat.noise_sigma = g_sigma;
      for (int i = 0; i < g_count; ++i) {
        astrocs::phase1::StarSource s;
        s.x = gx[i];
        s.y = gy[i];
        s.flux = static_cast<double>(gflux[i]);
        s.fwhm_px = 0.5 * (static_cast<double>(gex[0][i]) + static_cast<double>(gex[1][i]));
        const double esx = static_cast<double>(gex[2][i]);
        const double esy = static_cast<double>(gex[3][i]);
        const double emx = std::max(esx, esy), emn = std::min(esx, esy);
        s.ellipticity = (emx > 0.0) ? 1.0 - emn / emx : 0.0;
        // SNR 列语义 = SNR_det（GATES_AND_TOLERANCES §2）：未平滑原图检出像素峰值
        const int pxi = (int)std::lround(gx[i]);
        const int pyi = (int)std::lround(gy[i]);
        const double peak =
            (pxi >= 0 && pxi < im.w() && pyi >= 0 && pyi < im.h())
                ? static_cast<double>(im.px()[static_cast<size_t>(pyi) * im.w() + pxi])
                : static_cast<double>(gflux[i]);
        s.snr = (peak - cat.background) / cat.noise_sigma;
        s.quality = (gsat[i] != 0) ? 1 : 0;
        s.id = "src-" + std::to_string(i);
        cat.sources.push_back(std::move(s));
        if (gsat[i] != 0) ++cat.n_saturated;
      }
      cat.n_detected = static_cast<uint32_t>(cat.sources.size());
      sdet_free_detect_ex(gx, gy, gflux, gsat, gmag, ghas, gex, 4);
      guided_prov = pred_prov;
      guided_prov["approx_wcs_source"] = wsrc;
      guided_prov["approx_wcs_orientation_assumed"] = orientation_assumed;
      // 取向先验来源可审计: 是否来自本帧解算产物（含取向）, 还是配置/初始指向。
      guided_prov["orientation_from_solved_wcs"] =
          (solved_ptr != nullptr) && !gcfg.have_explicit_wcs;
      guided_prov["approx_wcs_crval1_deg"] = awcs.crval1;
      guided_prov["approx_wcs_crval2_deg"] = awcs.crval2;
      guided_prov["approx_wcs_cd11"] = awcs.cd11;
      guided_prov["approx_wcs_cd12"] = awcs.cd12;
      guided_prov["approx_wcs_cd21"] = awcs.cd21;
      guided_prov["approx_wcs_cd22"] = awcs.cd22;
      guided_prov["n_predicted"] = gstats.n_predicted;
      guided_prov["n_dropped"] = gstats.n_dropped;
      guided_prov["n_fit_failed"] = gstats.n_fit_failed;
      guided_prov["n_rejected"] = gstats.n_rejected;
      guided_prov["n_fit_ok"] = gstats.n_fit_ok;
      guided_prov["n_output"] = gstats.n_output;
      guided_prov["fit_success_rate"] =
          (gstats.n_predicted > 0)
              ? static_cast<double>(gstats.n_output) / static_cast<double>(gstats.n_predicted)
              : 0.0;
      guided_prov["gaia"] = gaia_prov;
      det_mode = "catalog_guided";
      authoritative = true;
      det_degraded.clear();
    } else {
      auto r = det.detect(im.px(), im.w(), im.h());
      if (r.failed()) {
        f_err[fi] = Result<void>::fail(r.error());
        return;
      }
      cat = r.value();
    }
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
      // dbuf 已在本帧检测阶段按 FP64 构造（引导检测与 PSF 拟合共用一份缓冲）
      const int drc = dpsf_fit_batch_f64(
          dbuf.data(), im.w(), im.h(), dets.data(), static_cast<int>(N_fit),
          nullptr, psf_params.data(), &n_valid, psf_status.data());
      if (drc != 0) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "dpsf_fit_batch_f64 failed rc=" + std::to_string(drc)));
        return;
      }
      if (n_valid <= 0) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "dpsf_fit_batch_f64: 0/" + std::to_string(N_fit) + " fits converged"));
        return;
      }
      // 成功行按拟合输入序（== 检测序）compact; 逐星按真值索引取行 (row 只读)
      int row = 0;
      for (size_t k = 0; k < N_fit; ++k) {
        if (psf_status[k] != DPSF_PSF_STATUS_OK) continue;
        // [7]=fwhm_x [8]=fwhm_y; sx=sigma_x → fwhm=2.3548*sx（由 9 列取 [7]/[8] 权威值）
        f_fx[fi].push_back(psf_params[static_cast<size_t>(row) * 9 + 7]);
        f_fy[fi].push_back(psf_params[static_cast<size_t>(row) * 9 + 8]);
        const double sx = psf_params[static_cast<size_t>(row) * 9 + 4];
        const double sy = psf_params[static_cast<size_t>(row) * 9 + 5];
        const double mx = std::max(sx, sy), mn = std::min(sx, sy);
        f_ell[fi].push_back(mx > 0.0 ? 1.0 - mn / mx : 0.0);
        ++row;
      }
      f_nvalid[fi] = n_valid;
    }
    f_ntotal[fi] = static_cast<int64_t>(N);
    f_nfit[fi] = static_cast<int64_t>(N_fit);
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
        const double* prow = &psf_params[static_cast<size_t>(row) * 9];
        // ── F-INSTR-CONFORM-FIX (SCI-PSF-001 §2/§5/§9a; SCI-PHOT-001 §9a) ────
        // 本列 = PSF 拟合域**解析通量** flux = 2πA·sxsy/3 (β=4, 单位 ADU;
        // 与 dpsf_psf.cpp:428 同式)。它是测光 F_instr 的唯一合法来源。
        // 修复前该量在 p1_psf.json 中无列承载 ⇒ 下游只能退回检测域 5×5
        // 正性截断盒和（star_detector.cpp:151 s.flux = m00）——盒和捕获的
        // PSF 能量份额随 seeing 变化（非测光量）, 给出假帧间差。
        psf_rows.push_back(Json{{"star_id", cat.sources[i].id},
                                {"B", prow[0]}, {"A", prow[1]},
                                {"cx", prow[2]}, {"cy", prow[3]},
                                {"sx", prow[4]}, {"sy", prow[5]},
                                {"theta", prow[6]},
                                {"fwhm_x", prow[7]}, {"fwhm_y", prow[8]},
                                {"flux", p1_psf_analytic_flux(prow[1], prow[4], prow[5])}});
        ++row;
      }
    }
    f_frame[fi] = Json{{"file", p1_base_name(path)},
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
                          // STARDET-01: 检测路径溯源（权威 = catalog_guided；
                          // 非权威 = blind_diagnostic，必须带 degraded_reason）
                          {"detection_mode", det_mode},
                          {"detection_authoritative", authoritative},
                          {"detection_degraded_reason", det_degraded},
                          {"guided", guided_prov},
                          {"sources", sources},
                          {"psf_params", psf_rows}};
  });
  // STARDET-01: 释放按 worker 的 sdet 句柄与共享 Gaia 句柄（帧级并行已 join）
  gcleanup();
  // 帧序归约（冻结顺序：下标升序；首个失败即返回，与串行同判据）
  Json frames = Json::array();
  std::vector<double> fwhm_xs, fwhm_ys, ells;
  int64_t n_valid_total = 0, n_total_total = 0, n_fit_total = 0;
  for (size_t fi = 0; fi < n_lights; ++fi) {
    if (!f_err[fi].ok()) {
      if (f_errkind[fi] == 1) (*man)["error_kind"] = "input";
      return f_err[fi];
    }
    frames.push_back(f_frame[fi]);
    fwhm_xs.insert(fwhm_xs.end(), f_fx[fi].begin(), f_fx[fi].end());
    fwhm_ys.insert(fwhm_ys.end(), f_fy[fi].begin(), f_fy[fi].end());
    ells.insert(ells.end(), f_ell[fi].begin(), f_ell[fi].end());
    n_valid_total += f_nvalid[fi];
    n_total_total += f_ntotal[fi];
    n_fit_total += f_nfit[fi];
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
  // STARDET-01: 检测模式与权威性登记（ASTROCS_DESIGN.md §4.2 权威 = 星表引导拟合）
  (*man)["detection_mode"] = eff_mode;
  (*man)["detection_authoritative"] = guided;
  (*man)["detection_degraded_reason"] = degrade_reason;
  (*man)["star_detection_max_stars"] = gcfg.max_stars;
  if (guided) (*man)["gaia"] = gaia_prov;
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
// 直调测试: eng/tests/unit/p1001_real_nodes_test.cpp::test_starpsf_precise_inactive_
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

// ── P1-PHOT-BROKEN: WCS 天测可用性判定 ──────────────────────────────────────
// 判定一个 WCS 对象是否**天测可用**, 而不是"对象非空"。
// 修复前 p1_op_photometry 只判 `wj.empty()`; 而 config 的 "wcs" 段是
// {"init_source":"header_pointing","gaia_data_dir":...} —— 非空但不含任何
// 天测键。该回退一旦命中, WcsTransform 初始化为 CRVAL=(0,0)、CD=0（det=0）:
// 锥形搜索落到 (0,0)、gaia_projected_in_frame=0、匹配 0 对 ⇒ star_matcher 走
// NO_DATA 退化（scale=1.0）, 而调用方把 1.0 当作"已拟合标度"施加。
// 实测（run/RELEASE-02/logs/smoke_norm.stderr:6155-6167 与 12 板块 phot 日志）
// 11/12 板块正是这条路径。可用性判据: 有限 CRVAL1/2 + 有限非退化 CD 矩阵。
bool p1_wcs_astrometry_usable(const Json& wc, std::string* why) {
  auto bad = [why](const char* m) { if (why) *why = m; return false; };
  if (!wc.is_object() || wc.empty()) return bad("missing/empty wcs object");
  const char* rk[2] = {"crval1", "crval2"};
  for (const char* k : rk) {
    if (!p1_has(wc, k) || !wc[k].is_number()) return bad("missing/invalid crval");
    if (!std::isfinite(wc[k].get<double>())) return bad("non-finite crval");
  }
  const char* ck[4] = {"cd11", "cd12", "cd21", "cd22"};
  double cd[4] = {0.0, 0.0, 0.0, 0.0};
  for (int i = 0; i < 4; ++i) {
    if (!p1_has(wc, ck[i]) || !wc[ck[i]].is_number())
      return bad("missing/invalid CD matrix");
    cd[i] = wc[ck[i]].get<double>();
    if (!std::isfinite(cd[i])) return bad("non-finite CD matrix");
  }
  const double det = cd[0] * cd[3] - cd[1] * cd[2];
  if (!(std::fabs(det) > 0.0)) return bad("degenerate CD matrix (det=0)");
  return true;
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
  // P0-21: 每帧一个 WCS 产物目录 ⇒ frame_key 必须唯一（重复即 fail-closed）。
  {
    auto uniq = p1_require_unique_frame_keys(doc);
    if (uniq.failed()) return uniq;
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
    // M1a-C-003: pts 里的 x/y 是 **0-based 数组下标**; WcsTan/
    // p1_tan_forward_reference 的契约是 **FITS 1-based** ⇒ 在进入两者之前
    // 施加**恰好一次** xp = x + kP1FitsPixelOrigin (SCI-WCS-001 §3a/§5a)。
    // 修复前缺该桥接: samples[] 的 ra/dec 与自带 (x,y) 标签恒差 1px, 且
    // 两道门 (roundtrip / forward-cross) 与参考解共用同一 (未桥接) 原点
    // ⇒ 对原点平移零判别力 (M1a-C-003)。
    for (const auto& [x, y] : pts) {
      const double xp = x + kP1FitsPixelOrigin;   // 内部 0-based → FITS 1-based
      const double yp = y + kP1FitsPixelOrigin;
      double ra = 0.0, dec = 0.0, bx = 0.0, by = 0.0;
      if (sip.present && sip.order > 0) {
        double A = 0.0, B = 0.0;
        p1_sip_poly(sip.a, xp - wcs.crpix1, yp - wcs.crpix2, sip.order, &A);
        p1_sip_poly(sip.b, xp - wcs.crpix1, yp - wcs.crpix2, sip.order, &B);
        wcs.pix2sky(xp + A, yp + B, &ra, &dec);
      } else {
        wcs.pix2sky(xp, yp, &ra, &dec);
      }
      wcs.sky2pix(ra, dec, &bx, &by);
      const double rt = std::sqrt((bx - xp) * (bx - xp) + (by - yp) * (by - yp));
      if (rt > max_rt) max_rt = rt;
      // B2-A1/B2-A17: 绝对门 —— 与独立 gnomonic 前向参考解的角度残差
      // (独立参考解同样施加 SIP A/B, 且同样以 FITS 1-based 入参 ⇒ 与
      //  sky2pix/pix2sky 自洽无关, 但对原点平移的鉴别力来自上面的单一桥接)。
      double ra_ref = 0.0, dec_ref = 0.0;
      p1_tan_forward_reference_sip(sip, wcs.crpix1, wcs.crpix2, wcs.crval1,
                                   wcs.crval2, wcs.cd11, wcs.cd12, wcs.cd21,
                                   wcs.cd22, xp, yp, &ra_ref, &dec_ref);
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
                        // M1a-C-003: samples[] 的 (x,y) 原点必须显式声明
                        // (内部 0-based 数组下标 = index-is-center); 其 ra/dec 已
                        // 经单次 +1 桥接至 FITS 1-based 与 (x,y) 配对。消费方不得
                        // 再叠加一次 +1 (双重桥接 = 恒定 1px 系统偏移)。
                        {"pixel_origin", "0-based array index (index-is-center); "
                                         "FITS 1-based xp = x + 1"},
                        {"fits_pixel_origin", kP1FitsPixelOrigin},
                        {"samples", samples}};
    // P0-21 §3.4: 每帧独立落一个 WCS 产物（显式 WCS 对所有帧同源, 但逐帧校验
    // 输入可读性并各自落盘 —— 输入 N 帧 ⇒ N 个逐帧产物, 任一帧不可读即报错）。
    Json artifacts = Json::array();
    for (const auto& l : doc["input_lights"]) {
      const std::string lp = l.get<std::string>();
      const std::string frame_path = p1_calibrated_path(doc, lp);
      P1Image fim = p1_read_image(frame_path);
      if (!fim.ok()) {
        (*man)["error_kind"] = "input";
        return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + frame_path));
      }
      const std::string fdir = p1_frame_dir(doc, lp);
      (void)aio_fs::make_dirs(fdir);   // CLEAN-403: 目录创建经 aio
      const std::string out_path = fdir + "/p1_wcs.json";
      if (!p1_write_text(out_path, wcs_out.dump(2)))
        return Result<void>::fail(Error(ErrorDomain::IO,
            "artifact write failed: " + out_path));
      artifacts.push_back(out_path);
    }
    (*man)["wcs_source"] = "explicit_config";
    (*man)["n_samples"] = samples.size();
    (*man)["max_roundtrip_px"] = max_rt;
    (*man)["max_forward_cross_deg"] = max_cross_deg;
    (*man)["wcs_artifact"] = artifacts.front();
    (*man)["wcs_artifacts"] = artifacts;
    (*man)["n_frames"] = artifacts.size();
    (*man)["artifacts"] = artifacts;
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
  // P0-21 §3.4: 逐帧独立解算 —— 每帧读入、按 init_source 取该帧指向、真实
  // ipv 求解、落该帧 p1_wcs.json。任一帧不可读/不可解 ⇒ 整体 fail-closed。
  // （eng/packaging/config/neighbor_crval 的 config 级参数校验在循环内逐帧复核, 首帧即拒。）
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
  // FAILCLOSED-01: 星表必须**完整**才允许解算。
  // 根因 run/WCS-DETERMINISM-01/REPORT.md §1.2 —— 修复前目录里装载失败的
  // *.xpsd shard 被静默丢弃（create 仍返回非 NULL），地址空间受限时丢掉的恰是
  // 唯一含亮星的 shard ⇒ 参考星表静默变成"暗 shard 里最亮的 60 颗" ⇒
  // iter_trans_solve 全败，且失败原因完全不可诊断。现 client 侧已 fail-closed
  // （装载失败 ⇒ create 返回 NULL），此处再加两道前置条件断言：
  //   ① file_count > 0  —— 空星表不得静默降级（M42 主题；空目录仍返回空 client）;
  //   ② file_load_fail_count == 0 且 file_count == 目录 *.xpsd 条目数
  //      —— **只断言 file_count > 0 不够**：本缺陷下 file_count=2 却仍不可用。
  gaia = gaia_client_create(gaia_dir.c_str());
  if (!gaia) {
    cleanup();
    const char* why = gaia_client_get_last_create_error();
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "gaia_client_create failed (gaia_data_dir=" + gaia_dir + ")" +
        (why ? std::string(": ") + why : std::string(": (no diagnostic)"))));
  }
  {
    const int gaia_files = gaia_client_get_file_count(gaia);
    const int gaia_entries = gaia_client_get_file_entry_count(gaia);
    const int gaia_load_failed = gaia_client_get_file_load_fail_count(gaia);
    if (gaia_files <= 0) {
      cleanup();
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "gaia catalog is empty (gaia_data_dir=" + gaia_dir +
          ", xpsd_entries=" + std::to_string(gaia_entries) +
          "): refusing to solve WCS without a reference catalog"));
    }
    if (gaia_load_failed != 0 || gaia_files != gaia_entries) {
      cleanup();
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "gaia catalog is incomplete: loaded=" + std::to_string(gaia_files) +
          " xpsd_entries=" + std::to_string(gaia_entries) +
          " load_failed=" + std::to_string(gaia_load_failed) +
          " (gaia_data_dir=" + gaia_dir +
          "): refusing to solve WCS on a partial catalog"));
    }
    // 可见诊断（不阻断）: magnitudeRange 声明非法而放弃 shard 剪枝的文件数。
    const int gaia_mag_rej = gaia_client_get_magnitude_range_reject_count(gaia);
    if (gaia_mag_rej > 0) {
      std::fprintf(stderr,
                   "[wcs] warning: %d gaia shard(s) have an invalid magnitudeRange "
                   "declaration -> shard-level magnitude pruning disabled "
                   "(conservative; gaia_data_dir=%s)\n",
                   gaia_mag_rej, gaia_dir.c_str());
    }
  }
  // 3) IPVSolver + 句柄注入
  ipv = ipv_solve_create();
  if (!ipv) {
    cleanup();
    return Result<void>::fail(Error(ErrorDomain::DATA, "ipv_solve_create failed"));
  }
  ipv_set_gaia_handle(ipv, reinterpret_cast<intptr_t>(gaia));
  ipv_set_detector_handle(ipv, reinterpret_cast<intptr_t>(sdet));
  IpvParams ip;
  ipv_get_default_params(&ip);
  std::memset(ip.log_dir, 0, sizeof(ip.log_dir));  // 节点面禁写求解日志
  // ── P0-21 §3.4: 逐帧独立求解循环 ───────────────────────────────────────
  // 每帧: 读入 → 该帧指向（header_pointing 逐帧; eng/packaging/config/neighbor 同源）→ 真实
  // ipv 求解 → roundtrip/前向交叉绝对门 → 落 output_dir/<frame_key>/p1_wcs.json。
  // 任一帧不可读/不可解 ⇒ 立即 DATA/IO fail-closed（不产出部分产品却报成功）。
  Json artifacts = Json::array();
  bool have_first = false;
  for (const auto& l : doc["input_lights"]) {
    const std::string lp = l.get<std::string>();
    // [RELEASE-02 probe] 逐帧热点: wcs
    ASTROCS_PROBE_SCOPE_CTX(_probe_wcs_frame, "phase1", "wcs.frame");
    ASTROCS_PROBE_TAG(_probe_wcs_frame, "frame_key", p1_frame_key(lp).c_str());
    const std::string frame_path = p1_calibrated_path(doc, lp);
    P1Image im = p1_read_image(frame_path);
    if (!im.ok()) {
      (*man)["error_kind"] = "input";
      cleanup();
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + frame_path));
    }
    // 该帧的初始指向/板尺度（header_pointing 逐帧推导; 其余来源 = config）。
    double f_ra0 = ra0, f_dec0 = dec0, f_focal = focal_mm, f_pixel = pixel_um;
    double f_s0 = s0_arcsec_px;
    std::string f_center_src = init_center_src;
    if (init_source == "header_pointing") {
      std::string why, src;
      if (!p1_header_pointing(im, &f_ra0, &f_dec0, &f_focal, &f_pixel, &f_s0,
                              &src, &why)) {
        cleanup();
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "wcs.init_source=header_pointing but " + why +
            " (fail-closed; frame " + frame_path + ")"));
      }
      f_center_src = src;
    }
    if (std::isnan(f_ra0) || std::isnan(f_dec0)) {
      cleanup();
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "wcs init pointing unavailable for init_source=" + init_source +
          " (ra0/dec0 missing; 禁 silent default; frame " + frame_path + ")"));
    }
    if (!std::isfinite(f_focal) || !std::isfinite(f_pixel) ||
        !(f_focal > 0.0) || !(f_pixel > 0.0)) {
      cleanup();
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "wcs init plate scale unavailable for init_source=" + init_source +
          " (FOCALLEN/XPIXSZ invalid; 禁 silent default; frame " + frame_path + ")"));
    }
    // FP64 内存求解（double 图像全链不降级）
    std::vector<double> dbuf(static_cast<size_t>(im.w()) * static_cast<size_t>(im.h()));
    for (size_t i = 0; i < dbuf.size(); ++i) dbuf[i] = static_cast<double>(im.px()[i]);
    IpvWcsResult r;
    std::memset(&r, 0, sizeof(r));
    const int src = ipv_solve_from_memory_with_callback_d(
        ipv, dbuf.data(), im.w(), im.h(), f_ra0, f_dec0, f_focal, f_pixel,
        &ip, nullptr, nullptr, &r);
    if (src != 1 || r.success != 1) {
      cleanup();
      return Result<void>::fail(Error(ErrorDomain::DATA,
          // 判词只陈述**已知为真**的两件事: 求解器返回失败、以及求解器自报的原因。
          // 求解器内部的失败点有多处（星表查询返回数不足、三角形匹配不足、RANSAC、
          // 以及 parity/尺度合理性闸门），本层无法区分，故不得替它归因到其中任一处
          // —— 早先这里把「星表查询返回 0」误报成「被 parity/尺度闸门拒绝」，
          // 直接把定位方向带偏（OPEN_QUESTIONS OQ-10）。
          std::string("ipv_solve_from_memory_with_callback_d failed: ") +
          (r.error_msg[0] ? r.error_msg : "solver returned failure") +
          " (ipv 真实求解器链失败; 具体失败点以求解器自报原因与求解日志为准; frame " +
          frame_path + ")"));
    }
    // 解算结果 → WcsTan 自检: 次级 roundtrip (<1e-6 px) + B2-A1 绝对
    // 前向交叉门 (<=1e-9 deg, 独立 gnomonic 参考解)
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
    // M1a-C-003: 同 explicit 路径 —— 0-based 数组下标经**恰好一次**
    // FITS 1-based 桥接后喂 WcsTan / 独立参考解 (SCI-WCS-001 §3a/§5a)。
    for (const auto& [x, y] : pts) {
      const double xp = x + kP1FitsPixelOrigin;   // 内部 0-based → FITS 1-based
      const double yp = y + kP1FitsPixelOrigin;
      double ra = 0.0, dec = 0.0, bx = 0.0, by = 0.0;
      wcs.pix2sky(xp, yp, &ra, &dec);
      wcs.sky2pix(ra, dec, &bx, &by);
      const double rt = std::sqrt((bx - xp) * (bx - xp) + (by - yp) * (by - yp));
      if (rt > max_rt) max_rt = rt;
      // B2-A1: 绝对门 (同 explicit 路径; 独立前向参考解, 1-based 入参)
      double ra_ref = 0.0, dec_ref = 0.0;
      p1_tan_forward_reference(wcs.crpix1, wcs.crpix2, wcs.crval1, wcs.crval2,
                               wcs.cd11, wcs.cd12, wcs.cd21, wcs.cd22,
                               xp, yp, &ra_ref, &dec_ref);
      const double cross = p1_angular_sep_deg(ra, dec, ra_ref, dec_ref);
      if (cross > max_cross_deg) max_cross_deg = cross;
      samples.push_back(Json{{"x", x}, {"y", y}, {"ra", ra}, {"dec", dec},
                             {"ra_ref", ra_ref}, {"dec_ref", dec_ref},
                             {"forward_cross_deg", cross},
                             {"roundtrip_px", rt}});
    }
    if (max_rt >= 1e-6) {
      cleanup();
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "WcsTan roundtrip " + std::to_string(max_rt) + " px exceeds 1e-6 contract"));
    }
    // B2-A1 绝对正确性门 (阈值依据同 explicit 路径): <=1e-9 deg。
    if (!std::isfinite(max_cross_deg) || max_cross_deg > 1e-9) {
      cleanup();
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "WcsTan forward cross " + std::to_string(max_cross_deg) +
          " deg exceeds 1e-9 absolute contract"));
    }
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
    if (r.sip_order < 0 || r.sip_order > 5 || r.sip_ap_order < 0 || r.sip_ap_order > 5) {
      cleanup();
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "solver SIP order out of drizzle contract [0,5] (order=" +
          std::to_string(r.sip_order) + ", ap_order=" + std::to_string(r.sip_ap_order) + ")"));
    }
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
                        {"wcs_init_center_src", f_center_src},
                        {"wcs_init_ra0_deg", f_ra0},
                        {"wcs_init_dec0_deg", f_dec0},
                        {"wcs_init_s0_arcsec_px", f_s0},
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
                        // M1a-C-003: 同 explicit 路径的像素原点声明 ——
                        // samples[] 的 (x,y) 为内部 0-based 数组下标 (index-is-center),
                        // ra/dec 已经单次 +1 桥接至 FITS 1-based 与其配对。
                        {"pixel_origin", "0-based array index (index-is-center); "
                                         "FITS 1-based xp = x + 1"},
                        {"fits_pixel_origin", kP1FitsPixelOrigin},
                        {"samples", samples}};
    const std::string fdir = p1_frame_dir(doc, lp);
    (void)aio_fs::make_dirs(fdir);   // CLEAN-403: 目录创建经 aio
    const std::string out_path = fdir + "/p1_wcs.json";
    if (!p1_write_text(out_path, wcs_out.dump(2))) {
      cleanup();
      return Result<void>::fail(Error(ErrorDomain::IO,
          "artifact write failed: " + out_path));
    }
    artifacts.push_back(out_path);
    if (!have_first) {
      have_first = true;
      // F-10: 初始指向来源登记（首帧口径; 逐帧值在各自 p1_wcs.json）。
      (*man)["wcs_init_source"] = init_source;
      (*man)["wcs_init_center_src"] = f_center_src;
      (*man)["wcs_init_ra0_deg"] = f_ra0;
      (*man)["wcs_init_dec0_deg"] = f_dec0;
      (*man)["wcs_init_s0_arcsec_px"] = f_s0;
      (*man)["n_pairs"] = r.n_pairs;
      (*man)["rms_px"] = r.rms_px;
      (*man)["n_samples"] = samples.size();
      (*man)["max_roundtrip_px"] = max_rt;
      (*man)["max_forward_cross_deg"] = max_cross_deg;
    }
  }
  cleanup();
  (*man)["wcs_source"] = "ipv";
  (*man)["n_frames"] = static_cast<uint64_t>(artifacts.size());
  (*man)["wcs_artifact"] = artifacts.front();
  (*man)["wcs_artifacts"] = artifacts;
  (*man)["artifacts"] = artifacts;
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
    if (!aio_fs::exists(src_path))
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "p1_sources.json missing (upstream star-psf artifact required): " + src_path));
    std::string ftext;
    if (!aio_fs::read_all(src_path, &ftext))
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot open: " + src_path));
    try {
      const Json j = Json::parse(ftext);
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
  // PERF-P1: 帧级并行（与 p2_parallel_for 同规范）。每帧只写**自己下标**的结果槽；
  // 跨帧无浮点归约；帧序归约（缺帧计数 / 错误选择 / frames）在 join 之后按下标
  // 升序执行 ⇒ 与串行逐字一致、与 worker 数无关。帧内逐源 OpenMP 并行保持不变
  // （nested=0 时由所属 worker 串行执行该 region, 正确性与串行同）。
  const uint32_t phot_workers = p1_frame_workers(doc);
  const size_t n_cat = cat.size();
  std::vector<Result<void>> f_err(n_cat, Result<void>::success());
  std::vector<int> f_errkind(n_cat, 0);   // 1=input
  std::vector<bool> f_missing(n_cat, false);
  std::vector<std::string> f_missing_name(n_cat);
  std::vector<Json> f_frames(n_cat);
  p1_parallel_for(phot_workers, n_cat, p1_workers(doc), [&](uint64_t fi, uint32_t) {
    const Json& fr = cat[fi];
    const std::string file = fr.value("file", std::string());
    // sources.json 的 file 是 calibrated 基名; 逐帧读取。
    if (file.empty()) {
      f_missing[fi] = true;
      f_missing_name[fi] = "(empty file name)";
      return;
    }
    const std::string path = out_dir + "/" + file;
    if (!aio_fs::exists(path)) {
      // B2-A16: 帧缺失按合同计数, 循环后显式上抛（不再静默跳过记 error）。
      f_missing[fi] = true;
      f_missing_name[fi] = file;
      return;
    }
    P1Image im = p1_read_image(path);
    if (!im.ok()) {
      f_errkind[fi] = 1;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + path));
      return;
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
      if (!pok[si]) {
        f_err[fi] = Result<void>::fail(
            Error(static_cast<ErrorDomain>(perr_dom[si]), perr[si]));
        return;
      }
      const Json& s = srcs[si];
      const double cx = s.value("x", 0.0), cy = s.value("y", 0.0);
      const astrocs::phase1::PhotometryResult& pr = prows[si];
      results.push_back(Json{{"id", s.value("id", std::string())},
                             {"x", cx}, {"y", cy},
                             {"flux", pr.flux}, {"flux_error", pr.flux_error},
                             {"background", pr.background}, {"snr", pr.snr},
                             {"valid", pr.valid}, {"failure_reason", pr.failure_reason}});
    }
    f_frames[fi] = Json{{"file", file}, {"results", results}};
  });
  // 帧序归约（冻结顺序：下标升序；首个失败即返回，与串行同判据）
  for (size_t fi = 0; fi < n_cat; ++fi) {
    if (f_missing[fi]) {
      ++missing_frames;
      if (first_missing.empty()) first_missing = f_missing_name[fi];
      continue;
    }
    if (!f_err[fi].ok()) {
      if (f_errkind[fi] == 1) (*man)["error_kind"] = "input";
      return f_err[fi];
    }
    frames.push_back(f_frames[fi]);
  }
  if (missing_frames != 0)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        std::to_string(missing_frames) + " frame(s) referenced by p1_sources.json "
        "not found (first: " + first_missing + ")"));
  const std::string out_path = out_dir + "/p1_flux.json";
  Json flux_out = Json{{"schema", "DATA-P1-FLUX"}, {"frames", frames}};
  if (!p1_write_text(out_path, flux_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  // ── RELEASE-02 FIX-P1 (P1-1): 取 k_photo 并真正施加 I_photo = k_photo·I_cal ──
  // 规范依据: docs/algorithms/CALIBRATION_ALGORITHMS.md §3.6 /
  //   docs/science/PHOTOMETRY.md (SCI-PHOT-001 FROZEN):
  //   k_photo = scale = 10^(-location), location = Tukey-IRLS(r_i) (c=4.685)。
  //
  // 缺陷 (GAP_AUDIT §9.29): 本节点原只 measure_flux, 写死
  //   photometry_applied=false/photscal=1.0; apply_photometry 生产零调用者;
  //   会归一化的 pc_calibrate_simple* 只在测试 target 编译 ⇒ Phase1 测光
  //   归一化从未应用到像素。
  //
  // 修复: 在 Drizzle 前对**每一帧**施加 I_photo=k_photo·I_cal, 写独立产物
  //   photoapplied_<base>（不就地覆写 cal 产物, 同 CORE-RACE-001 语义）。
  //   k_photo 两个显式来源:
  //   (1) photometry.fit.enabled=true → 本节点直接调生产 star_matcher 链
  //       (frame_photometry_fit → pc_calibrate_simple_with_gaia_f64_v2_qf);
  //       配置 gaia_data_dir / filter / filters_json / qe_json / qe_name /
  //       max_stars（缺 gaia/filter/filters_json 即不拟合, 不造 1.0）。
  //   (2) 否则读 p1_photscale.json (DATA-P1-PHOTSCALE-001) 逐帧标量
  //       （star_matcher 外部/离线标定通道）。
  //   两者皆无 → 如实中性 (applied=false, pixel_scaling="none") +
  //   photscale_source="none"（不把 1.0 伪装成"已应用"）。
  //
  // ── FAILSEM-01 帧级失败语义（负责人裁决）────────────────────────────────
  // 负责人原话：「拟合失败这帧报 error/fail 呗，不阻塞其他帧。」
  // 逐帧**独立**判定，三种结局互不牵连：
  //   (a) 该帧产出标度 ⇒ 施加 I_photo = k_photo·I_cal 并写 photoapplied 产物；
  //   (b) 该帧拟合失败（无 PSF 星 / NO_DATA / 非物理标度 / 散度超限 / 侧车无
  //       拟合证据 / 侧车未覆盖该帧 / 该帧 WCS 不可用）⇒ **该帧 fail**：
  //       status="fail" + 机器可读 error（domain/status/message）+ 不产出产物,
  //       **其他帧照常处理**（帧间独立；SCI-PHOT-001 §1「门只有一个 = 单帧标定
  //       是否可信，与其它帧无关」）；
  //   (c) 全局性失败（星表不可读 / 配置非法 / 冻结 C 入口自身 rc!=0）⇒ 本节点
  //       **中止**（环境问题，不是数据问题；换一帧也不会好）。
  // 运行级判红：失败帧不产出产品 ⇒ 产品数 < 输入帧数，由 write_hips 节点按
  //   ASTROCS_DESIGN §4.4「任何一帧未被处理、跳过或失败都显式判红」上抛
  //   SCIENCE_PRECONDITION（exit 4）；本节点不因单帧失败而中止。
  // 逐帧判决是**唯一真相**（manifest.frame_status / p1_phot.json.frames[]）；
  //   组级 photometry_applied 只是摘要，下游不得据此替代逐帧判定。
  //
  // ── P1-PHOT-BROKEN 修复门（全部 fail-closed, 不得放宽）──────────────────
  // (0) **本节点不设星数门槛**（SCI-PHOT-001 §16.5）：标度按帧自身拟合结果判定，
  //     不以「匹配星数 >= N」作准入。SCI-PHOT-001 §4 的「|r_consistent| >= 3 才进
  //     IRLS」是**求解前提**（唯一实现在 star_matcher / frame_photometry_fit 内，
  //     常量 astrocs::photometry::kMinFitStars）：不成立时拟合**本就不产出标度**
  //     （rc<0 / fit_ok=false），本节点按**拟合失败**如实上报，而不是另立门禁去卡。
  //     外部 sidecar 通道同理：只判「有无拟合证据」，不判星数够不够。
  // (1) P1_PHOT_MAX_SIGMA_DEX: 拟合散度 QA 上限（1.0 dex = 2.5 mag）。散度更大
  //     说明匹配集不是同一测光零点, 不是标度。
  // (3) P1_PHOT_MAX_SPREAD_DEX: **组内帧间一致性**上限。SCI-PHOT-001 §3 明确
  //     scale 单位是 [F_syn 单位]/ADU, 其**绝对值**由未建模的仪器常数
  //     （口径·曝光·增益·hc, 见 §6「常数由 location 吸收」）决定, 可跨多个
  //     数量级 —— 故**不能**用绝对窗口（如 [0.1,10]）判"合理": 那会拒绝 100%
  //     的真实 ADU→F_syn 标度（实测本 L4 数据 location=16.2 dex ⇒ k=6.27e-17）。
  //     物理上受约束的是同一组内各帧的**相对**一致性（同仪器/滤光片/曝光）:
  //     负责人判据 = 同组帧间 k 峰峰 ≤ 0.05 mag ⇒ 0.02 dex（1 dex = 2.5 mag）。
  //     F-INSTR-CONFORM-FIX 收紧 (0.5 → 0.02): 旧值 0.5 dex = 1.25 mag 过松 ——
  //     实测 seeing 2.0→4.0 px 的盒和口径假帧间差 0.50 mag、孔径扫描 M_seeing
  //     达 1.35 mag 仍能过门（"1.35 mag 的假帧间差照样过门"）。旧口径下 L4 真实
  //     帧对 t2_m1 的 k 散度 0.0427 dex = 0.107 mag 亦能过门, 而它是视宁度假信号。
  //     超限 ⇒ 整组不施加 + degraded_reason（fail-closed, 不混装测光体系）。
  constexpr double P1_PHOT_MAX_SIGMA_DEX = 1.0;
  constexpr double P1_PHOT_MAX_SPREAD_DEX = 0.02;  // ≈0.05 mag 峰峰（负责人判据）
  // ── FAILSEM-01: 逐帧判决（每帧必有其一）──────────────────────────────────
  // error_domain/error_status 取 LOG 合同 §5 的 error_report 口径：domain =
  // ErrorDomain 名、status = 稳定错误码 ^[A-Z][A-Z0-9_]{0,63}$（不新造第二套
  // 码表，只给本节点可定位的稳定标识）。
  struct P1FrameVerdict {
    std::string status;           // "ok" | "fail"
    std::string error;            // fail 时的诊断（路径只含仓库内相对/绝对路径与数值）
    std::string error_domain;     // fail 时的 ErrorDomain 名
    std::string error_status;     // fail 时的稳定错误码
    std::string degraded_reason;  // 拟合自身的机器可读退化分支（可空）
    int n_matched = 0;
    double sigma_residual_dex = 0.0;
  };
  struct P1FrameScale {
    std::string key;
    double k_photo = 1.0;
    int n_matched = 0;
    double sigma_residual_dex = 0.0;
    std::string source;
    bool fitted = false;  // true ⇔ 来自真实拟合（fit_ok）, false ⇔ 外部/占位
    // F-INSTR-CONFORM-FIX: F_instr 域 provenance（SCI-PHOT-001 §9a）
    int64_t n_psf_domain = 0;   // F_instr 有效域星数（有 PSF 拟合行且 flux 有限 >0）
    int64_t n_psf_skipped = 0;  // psf_status != OK / 未进拟合子集 ⇒ 跳过, 不回退盒和
    // ── FREF-BASELINE-001: 绝对合成星等零点（帧自身测光零点）────────────
    // mag = zero_point_mag - 2.5*log10(F_adu)，由 Gaia DR3 XP 绝对谱 + 本帧
    // 滤光片/QE 曲线正向合成（frame_photometry_fit.cpp）。与帧无关 ⇒ 跨帧公共。
    bool zero_point_valid = false;
    double zero_point_mag = 0.0;
    int zero_point_n_stars = 0;
    double zero_point_scatter_mag = 0.0;
    // ── PASSBAND-IDENTITY-GATE-01: 通带身份（帧级自述）───────────────────
    // 拟合入口自报的「实际用于合成 F_syn 的曲线身份」；组级一致性由
    // passband_identity 落盘（见 provenance 段）。空 filter_key ⇒ 该帧的标度
    // 不是拟合通道产出的（侧车通道），身份不适用。
    std::string filter_key;
    std::string filter_curve_name;
    int filter_n_points = 0;
    double filter_wl_min_nm = 0.0;
    double filter_wl_max_nm = 0.0;
  };
  std::map<std::string, P1FrameScale> scales;
  // 组级通带身份（配置声明 + 实际装载的曲线身份）。拟合通道启用时由**首帧**
  // 结果落定；随后每帧必须与它一致，否则该帧判 fail（帧间不得混用不同通带）。
  bool passband_identity_valid = false;
  std::string passband_identity_declared;   // 配置声明的通带名（块级 filter_passband）
  std::string passband_identity_filter;     // FILTER 关键字（fit.filter 或 filter_passband）
  std::string passband_identity_key;        // 解析出的库键（= filters.json 对象键）
  std::string passband_identity_curve_name; // 曲线对象自述 name（身份门要求 == 库键）
  int passband_identity_n_points = 0;
  double passband_identity_wl_min_nm = 0.0;
  double passband_identity_wl_max_nm = 0.0;
  double passband_identity_val_min = 0.0;
  double passband_identity_val_max = 0.0;
  std::string passband_identity_curve_source;  // filters.json 路径（实际读的那个）
  std::string photscale_source = "none";
  const Json phot_cfg = (p1_has(doc, "photometry") && doc["photometry"].is_object())
                            ? doc["photometry"] : Json::object();
  const Json fit_cfg = (phot_cfg.contains("fit") && phot_cfg["fit"].is_object())
                           ? phot_cfg["fit"] : Json::object();
  const bool fit_enabled = !fit_cfg.empty() && p1_flag(fit_cfg, "enabled", false);
  const Json& lights = doc["input_lights"];
  const size_t n_lights = lights.size();
  // FAILSEM-01: 逐帧判决（下标 = input_lights 下标, 与 lights 同序）。每帧必有其一;
  // 判 fail 只影响该帧, 不中止本节点 —— 全局性失败另走 fail_global（立即中止）。
  std::vector<P1FrameVerdict> verdicts(n_lights);
  for (size_t vi = 0; vi < n_lights; ++vi) verdicts[vi].status = "ok";
  // 本节点是否声明了标定通道（fit 通道启用 或 侧车存在）。通道**缺席**不是
  // "某帧拟合失败", 而是"测光未配置"的显式降级路径（photscale_absent）。
  bool channel_present = false;
  // 帧级失败记录（首个判词优先: 后续检查不再覆盖已定的失败原因）。
  auto mark_frame_fail = [&](size_t i, const std::string& domain,
                             const std::string& status_code,
                             const std::string& why,
                             const std::string& degraded) {
    if (i >= n_lights || verdicts[i].status == "fail") return;
    verdicts[i].status = "fail";
    verdicts[i].error = why;
    verdicts[i].error_domain = domain;
    verdicts[i].error_status = status_code;
    verdicts[i].degraded_reason = degraded;
  };
  // 全局性失败 = 环境/配置问题（星表不可读、配置缺项、C 入口自身失败）。
  // 语义依据: AGENTS §10（环境/数据缺失不停在"猜"）+ LOG 合同 §6 D3
  // （改变科学语义的情形不是降级, 必须 fail-closed 上行）。
  auto fail_global = [&](ErrorDomain d, const std::string& status_code,
                         const std::string& why) -> Result<void> {
    (*man)["error_status"] = status_code;
    (*man)["error_kind"] = (d == ErrorDomain::CONFIG) ? "config" : "input";
    return Result<void>::fail(Error(d, why));
  };

  if (fit_enabled) {
    // (1) 生产 star_matcher 拟合通道
    std::string gaia_dir = fit_cfg.value("gaia_data_dir", std::string());
    if (gaia_dir.empty() && p1_has(doc, "wcs") && doc["wcs"].is_object())
      gaia_dir = doc["wcs"].value("gaia_data_dir", std::string());
    std::string filter_name = fit_cfg.value("filter", std::string());
    if (filter_name.empty()) filter_name = doc.value("filter_passband", std::string());
    const std::string filters_json = fit_cfg.value("filters_json", std::string());
    const std::string qe_json = fit_cfg.value("qe_json", std::string());
    const std::string qe_name = fit_cfg.value("qe_name", std::string());
    const int max_stars = p1_int(fit_cfg, "max_stars", 5000);
    // FAILSEM-01: 配置缺项是**全局性失败**（不是数据问题）⇒ 立即中止, 不把整批
    // 帧判 fail。依据: 负责人裁决「全局性失败（如星表不可读、config 非法）⇒ 仍应中止」。
    if (gaia_dir.empty()) {
      return fail_global(ErrorDomain::CONFIG, "PHOT_CONFIG_GAIA_DIR_MISSING",
          "photometry.fit.gaia_data_dir (或 wcs.gaia_data_dir) required"
          "（全局性失败: 配置缺项, 中止运行而不是把整批帧判 fail）");
    }
    if (filter_name.empty()) {
      return fail_global(ErrorDomain::CONFIG, "PHOT_CONFIG_FILTER_MISSING",
          "photometry.fit.filter (或 filter_passband) required"
          "（全局性失败: 配置缺项, 中止运行）");
    }
    if (filters_json.empty()) {
      return fail_global(ErrorDomain::CONFIG, "PHOT_CONFIG_FILTERS_JSON_MISSING",
          "photometry.fit.filters_json required (response curve file)"
          "（全局性失败: 配置缺项, 中止运行）");
    }
    {
      channel_present = true;   // FAILSEM-01: fit 通道已启用（配置齐备）
      for (size_t i = 0; i < n_lights; ++i) {
        const std::string lp = lights[i].get<std::string>();
        const std::string key = p1_frame_key(lp);
        // PSF 星: p1_sources.json 帧序 == input_lights 序 (star-psf 顺序写出)
        if (i >= cat.size() || !cat[i].is_object() ||
            !cat[i].contains("sources") || !cat[i]["sources"].is_array()) {
          // 该帧在上游星点目录里没有行 ⇒ 本帧无从拟合（帧级, 其他帧照常）。
          mark_frame_fail(i, "DATA", "PHOT_SOURCES_FRAME_MISSING",
                          "p1_sources.json frame missing sources for " + key, "");
          continue;
        }
        const Json& srcs = cat[i]["sources"];
        // ══ F-INSTR-CONFORM-FIX: F_instr 的唯一合法域 = PSF 拟合域 ══════════
        // 规范依据: SCI-PHOT-001 §9a「星点通量来自 PSF 拟合域（PSF.md）」+
        //   SCI-PSF-001 §2/§5「flux = 2πA·sxsy/3, 单位 ADU」。
        // 缺陷（修复前）: F_instr 取 srcs[].flux = 检测器 5×5 正性截断盒和
        //   (star_detector.cpp:151 s.flux = m00), 并把每颗检测星标为 status=0
        //   （p1_sources 无拟合状态列）—— 盒和捕获的 PSF 能量份额依赖 seeing,
        //   不是测光量; 且 psf_status 从未真正送达有效域门。
        // 修复: (1) 按 star_id 关联 p1_psf.json 的 psf_params 行取 PSF 域解析
        //   通量（PSF-FAST-001 子集映射: 只有拟合成功的星有行, 禁按行号关联）;
        //   (2) psf_status != OK（拟合失败 / 未进入拟合子集）的星**跳过**,
        //   显式以非零 status 送入（star_matcher matchWithKdTree 的有效域门
        //   status==0 剔除; SCI-PHOT-001 §4「饱和/质量异常不参与定标」）
        //   —— **不回退盒和**: 回退会把 seeing 依赖重新注入标度, 且与 §9a 冲突。
        //   有效星不足时由 §4/§8 冻结门给 NO_DATA（fail-closed, 不伪造标度）。
        std::map<std::string, const Json*> psf_row_by_id;
        if (cat[i].contains("psf_params") && cat[i]["psf_params"].is_array()) {
          for (const auto& r : cat[i]["psf_params"]) {
            if (r.is_object() && r.contains("star_id") && r["star_id"].is_string())
              psf_row_by_id.emplace(r["star_id"].get<std::string>(), &r);
          }
        }
        struct P1PhotCand {
          double x = 0.0, y = 0.0, flux = 0.0;
          int status = DPSF_PSF_STATUS_FIT_FAILED;
          uint32_t qf = 0u;
        };
        std::vector<P1PhotCand> cands;
        cands.reserve(srcs.size());
        int64_t n_psf_domain = 0, n_psf_skipped = 0;
        for (const auto& s : srcs) {
          const auto it = psf_row_by_id.find(s.value("id", std::string()));
          P1PhotCand c;
          c.x = s.value("x", 0.0);          // 位置口径不变（检测质心, 非本次改动）
          c.y = s.value("y", 0.0);
          if (it != psf_row_by_id.end()) {
            c.flux = (*it->second).value("flux", 0.0);   // PSF 域解析通量 (ADU)
            if (std::isfinite(c.flux) && c.flux > 0.0) {
              c.status = DPSF_PSF_STATUS_OK;
              ++n_psf_domain;
            } else {
              c.flux = 0.0;   // 非物理 PSF 通量 ⇒ 视同拟合无效, 不得进标度
              ++n_psf_skipped;
            }
          } else {
            ++n_psf_skipped;  // 拟合失败 / 未进入 PSF-FAST-001 拟合子集
          }
          // P1-2: sdet 饱和位 = quality&1 (star_detector.cpp:170); 映射到
          // PC_QF_SATURATED (star_matcher.h:11, 1u<<1) ⇒ cleanAndScale 有效域
          // 过滤 (SCI-PHOT-001 §4/§10)。
          const int64_t q = s.value("quality", int64_t{0});
          c.qf = (q & 1) ? (1u << 1) : 0u;
          cands.push_back(c);
        }
        // 排序: PSF 域有效星按 F_instr 降序在前; 无效星恒在尾部（先被 max_stars
        // 截断丢弃 —— 它们本就不在有效域内）。稳定排序保证同亮度次序确定。
        std::stable_sort(cands.begin(), cands.end(),
                         [](const P1PhotCand& a, const P1PhotCand& b) {
                           const bool av = (a.status == DPSF_PSF_STATUS_OK);
                           const bool bv = (b.status == DPSF_PSF_STATUS_OK);
                           if (av != bv) return av;
                           return a.flux > b.flux;
                         });
        if (max_stars > 0 && cands.size() > static_cast<size_t>(max_stars))
          cands.resize(static_cast<size_t>(max_stars));
        std::vector<double> pcx, pcy, pfl;
        std::vector<int> pst;
        std::vector<uint32_t> pqf;
        pcx.reserve(cands.size()); pcy.reserve(cands.size());
        pfl.reserve(cands.size());
        pst.reserve(cands.size()); pqf.reserve(cands.size());
        for (const P1PhotCand& c : cands) {
          pcx.push_back(c.x);
          pcy.push_back(c.y);
          pfl.push_back(c.flux);
          pst.push_back(c.status);
          pqf.push_back(c.qf);
        }
        // WCS: 逐帧 p1_wcs.json（回退 config.wcs）
        Json wj = Json::object();
        {
          const std::string wpath = p1_frame_dir(doc, lp) + "/p1_wcs.json";
          std::string wtext;
          if (aio_fs::read_all(wpath, &wtext)) {
            try {
              Json wprod = Json::parse(wtext);
              if (wprod.is_object() && wprod.contains("wcs") && wprod["wcs"].is_object())
                wj = wprod["wcs"];
            } catch (...) { wj = Json::object(); }
          }
          if (wj.empty() && p1_has(doc, "wcs") && doc["wcs"].is_object()) wj = doc["wcs"];
        }
        // P1-PHOT-BROKEN (a): 原判定 wj.empty() 只查"对象是否为空"。config 的
        // "wcs" 段（init_source/gaia_data_dir）非空但无天测键 ⇒ 以 CRVAL=(0,0)、
        // CD=0 拟合出 0 匹配, 静默退化为占位 1.0。改为显式天测可用性校验:
        // 不可用即记 photscale_error ⇒ 整组不施加 + degraded_reason（fail-closed,
        // 不静默、不伪造）。
        std::string wcs_why;
        if (!p1_wcs_astrometry_usable(wj, &wcs_why)) {
          // 本帧天测不可用 ⇒ 本帧的 Gaia 逆投影无从谈起（帧级; 不得以零 WCS 拟合
          // 出占位 1.0）。其他帧的 WCS 各自独立 ⇒ 不阻塞。
          mark_frame_fail(i, "SCIENCE_PRECONDITION", "PHOT_WCS_UNUSABLE",
                          "WCS unusable for " + key + ": " + wcs_why, "");
          continue;
        }
        bool sip_ok = true;
        std::string sip_err;
        const P1SipCoeffs sip = p1_parse_sip(wj, &sip_ok, &sip_err);
        if (!sip_ok) {
          mark_frame_fail(i, "DATA", "PHOT_WCS_SIP_INVALID",
                          "wcs " + sip_err + " (frame " + key + ")", "");
          continue;
        }
        const std::string src_path_i = p1_calibrated_path(doc, lp);
        P1Image fim = p1_read_image(src_path_i);
        if (!fim.ok()) {
          mark_frame_fail(i, "IO", "PHOT_FRAME_UNREADABLE",
                          "cannot read " + src_path_i, "");
          continue;
        }
        std::vector<double> dbuf(static_cast<size_t>(fim.w()) * fim.h());
        for (size_t p = 0; p < dbuf.size(); ++p) dbuf[p] = static_cast<double>(fim.px()[p]);
        astrocs::photometry::FramePhotFitRequest freq;
        freq.pixels = dbuf.data();
        freq.width = fim.w(); freq.height = fim.h();
        freq.psf_cx = pcx.data(); freq.psf_cy = pcy.data();
        freq.psf_flux = pfl.data(); freq.psf_status = pst.data();
        freq.psf_quality = pqf.empty() ? nullptr : pqf.data();
        freq.n_psf = static_cast<int>(pst.size());
        freq.crval1 = p1_num(wj, "crval1", 0.0); freq.crval2 = p1_num(wj, "crval2", 0.0);
        freq.crpix1 = p1_num(wj, "crpix1", 0.0); freq.crpix2 = p1_num(wj, "crpix2", 0.0);
        freq.cd11 = p1_num(wj, "cd11", 0.0); freq.cd12 = p1_num(wj, "cd12", 0.0);
        freq.cd21 = p1_num(wj, "cd21", 0.0); freq.cd22 = p1_num(wj, "cd22", 0.0);
        freq.sip_order = sip.present ? sip.order : 0;
        freq.sip_a = sip.a; freq.sip_b = sip.b; freq.sip_ap = sip.ap; freq.sip_bp = sip.bp;
        freq.gaia_data_dir = gaia_dir;
        freq.filter_name = filter_name;
        // PASSBAND-IDENTITY-GATE-01: 把配置**声明**的通带名送进拟合入口，由
        // 生产唯一实现核对「FILTER 关键字解析出的库键 == 声明名」；不等 ⇒
        // 拟合拒绝产出标度（具名环境作用域失败，见 frame_photometry_fit.cpp）。
        // 依据 docs/science/PHOTOMETRY.md §2a.4/§2a.5（通带形状不被零点吸收、
        // 比较 sigma_residual 必须声明所用模型通带）。
        freq.declared_filter_passband = doc.value("filter_passband", std::string());
        freq.filters_json = filters_json;
        freq.qe_json = qe_json; freq.qe_name = qe_name;
        const astrocs::photometry::FramePhotFitResult fr =
            astrocs::photometry::fit_frame_photometry(freq);
        // 拟合失败 = **拟合自身的判决**（rc / fit_ok），不是本节点另立的星数门。
        // 冻结 C 入口在 NO_DATA/退化分支（无 PSF 星 / 无光谱星 / 滤光片缓存失败 /
        // SCI-PHOT-001 §4 求解前提 |r_consistent|<3）返回 rc==0 且 scale=1.0、
        // fit_used=0；占位 1.0 不得当作"已拟合标度"施加（FIX-P1）。
        // 本节点**不设星数门槛**（SCI-PHOT-001 §16.5「星数不构成拒绝条件」）：
        // 标度是否成立只由拟合自身判决回答，本节点不复检匹配星数。
        // ── FAILSEM-01: 拟合失败按**作用域**分流 ──────────────────────────
        // 环境/配置失败（星表不可读、冻结 C 入口自身 rc!=0）⇒ **全局中止**;
        // 本帧判决（无 PSF 星 / NO_DATA / 非物理标度）⇒ **该帧 fail**, 其他帧照常。
        if (fr.rc != 0 || !fr.fit_ok) {
          const std::string fit_why =
              "photometry fit failed for " + key + ": " +
              (fr.error.empty() ? std::string("no scale produced") : fr.error) +
              " (degraded_reason=" + fr.degraded_reason +
              ", n_matched=" + std::to_string(fr.n_matched) + ")";
          if (fr.failure_scope == astrocs::photometry::FitFailureScope::kEnvironment) {
            return fail_global(ErrorDomain::IO, "PHOT_FIT_ENVIRONMENT",
                fit_why + "（全局性失败: 星表/曲线/配置或 C 入口自身的问题, 中止运行）");
          }
          mark_frame_fail(i, "SCIENCE_PRECONDITION", "PHOT_FIT_NO_SCALE", fit_why,
                          fr.degraded_reason);
          verdicts[i].n_matched = fr.n_matched;
          verdicts[i].sigma_residual_dex = fr.sigma_residual_dex;
          continue;
        }
        if (!(std::isfinite(fr.k_photo) && fr.k_photo > 0.0)) {
          mark_frame_fail(i, "SCIENCE_PRECONDITION", "PHOT_SCALE_NON_PHYSICAL",
                          "non-physical k_photo for " + key, "non_physical_scale");
          continue;
        }
        if (!std::isfinite(fr.sigma_residual_dex) ||
            fr.sigma_residual_dex > P1_PHOT_MAX_SIGMA_DEX) {
          mark_frame_fail(i, "SCIENCE_PRECONDITION", "PHOT_FIT_IMPLAUSIBLE_SCATTER",
                          "implausible fit scatter for " + key +
                              " (sigma_residual_dex=" +
                              std::to_string(fr.sigma_residual_dex) + " > " +
                              std::to_string(P1_PHOT_MAX_SIGMA_DEX) + ")",
                          "implausible_fit_scatter");
          verdicts[i].n_matched = fr.n_matched;
          verdicts[i].sigma_residual_dex = fr.sigma_residual_dex;
          continue;
        }
        P1FrameScale sc;
        sc.key = key; sc.k_photo = fr.k_photo; sc.n_matched = fr.n_matched;
        sc.sigma_residual_dex = fr.sigma_residual_dex;
        sc.source = "gaia_star_matcher_tukey_irls";
        sc.fitted = true;
        sc.n_psf_domain = n_psf_domain;    // F-INSTR-CONFORM-FIX provenance
        sc.n_psf_skipped = n_psf_skipped;
        // PASSBAND-IDENTITY-GATE-01: 帧级通带身份自述（组级一致性见下方落盘）
        sc.filter_key = fr.filter_key;
        sc.filter_curve_name = fr.filter_curve_name;
        sc.filter_n_points = fr.filter_n_points;
        sc.filter_wl_min_nm = fr.filter_wl_min_nm;
        sc.filter_wl_max_nm = fr.filter_wl_max_nm;
        // FREF-BASELINE-001: 帧自身测光零点（正向合成, 与帧噪声无关）
        sc.zero_point_valid = fr.zero_point_valid;
        sc.zero_point_mag = fr.zero_point_mag;
        sc.zero_point_n_stars = fr.zero_point_n_stars;
        sc.zero_point_scatter_mag = fr.zero_point_scatter_mag;
        // ── PASSBAND-IDENTITY-GATE-01: 通带身份（组级一致性）─────────────
        // 首帧落定组级身份；后续帧必须与首帧**逐项**一致（同一组只能有一条
        // 模型通带）。不一致 ⇒ 该帧 fail（帧间不得混装不同通带的产品）。
        if (!passband_identity_valid) {
          passband_identity_valid = true;
          passband_identity_declared = doc.value("filter_passband", std::string());
          passband_identity_filter = filter_name;
          passband_identity_key = fr.filter_key;
          passband_identity_curve_name = fr.filter_curve_name;
          passband_identity_n_points = fr.filter_n_points;
          passband_identity_wl_min_nm = fr.filter_wl_min_nm;
          passband_identity_wl_max_nm = fr.filter_wl_max_nm;
          passband_identity_val_min = fr.filter_val_min;
          passband_identity_val_max = fr.filter_val_max;
          passband_identity_curve_source = filters_json;
        } else if (fr.filter_key != passband_identity_key ||
                   fr.filter_n_points != passband_identity_n_points ||
                   fr.filter_wl_min_nm != passband_identity_wl_min_nm ||
                   fr.filter_wl_max_nm != passband_identity_wl_max_nm) {
          mark_frame_fail(i, "SCIENCE_PRECONDITION", "PHOT_PASSBAND_IDENTITY_INCONSISTENT",
                          "passband identity differs from the group's first fitted frame"
                          " for " + key + " (group key='" + passband_identity_key +
                          "' n_points=" + std::to_string(passband_identity_n_points) +
                          " range=[" + std::to_string(passband_identity_wl_min_nm) + "," +
                          std::to_string(passband_identity_wl_max_nm) + "] nm; frame key='" +
                          fr.filter_key + "' n_points=" + std::to_string(fr.filter_n_points) +
                          " range=[" + std::to_string(fr.filter_wl_min_nm) + "," +
                          std::to_string(fr.filter_wl_max_nm) + "] nm)"
                          " -- refusing to mix different model passbands in one group",
                          "passband_identity_inconsistent");
          continue;
        }
        scales[key] = sc;
      }
      // 通道已走通即如实登记来源（逐帧成败见 verdicts / photscale_fit）。
      photscale_source = "gaia_star_matcher_tukey_irls";
    }
  } else {
    // (2) 上游/外部标定通道: p1_photscale.json (DATA-P1-PHOTSCALE-001)
    const std::string sp = out_dir + "/p1_photscale.json";
    if (aio_fs::exists(sp)) {
      channel_present = true;   // FAILSEM-01: 侧车标定通道存在
      std::string stext;
      if (!aio_fs::read_all(sp, &stext))
        return Result<void>::fail(Error(ErrorDomain::IO, "cannot open: " + sp));
      try {
        const Json sj = Json::parse(stext);
        if (!sj.is_object() || sj.value("schema", std::string()) != "DATA-P1-PHOTSCALE-001")
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "p1_photscale.json schema mismatch (expect DATA-P1-PHOTSCALE-001): " + sp));
        if (!sj.contains("frames") || !sj["frames"].is_array())
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "p1_photscale.json must have a 'frames' array: " + sp));
        for (const auto& sfj : sj["frames"]) {
          const std::string file = sfj.value("file", std::string());
          const double k = sfj.value("k_photo", 0.0);
          // 条目结构非法 = 侧车产物本身违约（不是"这一帧拟合失败"）⇒ 全局中止。
          if (file.empty() || !(std::isfinite(k) && k > 0.0))
            return fail_global(ErrorDomain::DATA, "PHOT_SIDECAR_FRAME_MALFORMED",
                "p1_photscale.json frame requires file + finite k_photo>0");
          // P1-PHOT-BROKEN (b'): 外部通道不得注入"无拟合证据"的占位标度。
          // 判据 = **有无拟合证据**（SCI-PHOT-001 §16.5「星数不构成拒绝条件」），
          // 不是"星数够不够"：侧车显式声明拟合来源（source）或声明拟合产出了
          // 至少一颗内点（n_matched >= 1）即为有证据；两者皆无 = 该标度没有拟合
          // 支撑 ⇒ 按**拟合失败**硬拒绝（fail-closed，不冒充已标定）。
          const int n_matched = sfj.value("n_matched", -1);
          const bool source_declared =
              sfj.contains("source") && sfj["source"].is_string() &&
              !sfj["source"].get<std::string>().empty();
          const bool fit_evidence = source_declared || n_matched >= 1;
          // 无拟合证据 = **该帧**的拟合失败（不是侧车结构违约）⇒ 该帧 fail,
          // 其他帧照常施加（帧间独立, 负责人裁决「不阻塞其他帧」）。
          if (!fit_evidence) {
            const std::string side_key = p1_frame_key(file);
            for (size_t li = 0; li < n_lights; ++li) {
              if (li >= lights.size() || !lights[li].is_string()) continue;
              const std::string lp_i = lights[li].get<std::string>();
              if (p1_frame_key(lp_i) != side_key &&
                  p1_frame_key(p1_calibrated_path(doc, lp_i)) != side_key)
                continue;
              mark_frame_fail(li, "SCIENCE_PRECONDITION", "PHOT_SCALE_NO_FIT_EVIDENCE",
                              "p1_photscale.json frame has no fit provenance (n_matched=" +
                                  std::to_string(n_matched) +
                                  ", source undeclared): no fit produced this scale"
                                  " -- refusing to fake a calibration (frame " +
                                  side_key + ")",
                              "no_fit_evidence");
              verdicts[li].n_matched = n_matched < 0 ? 0 : n_matched;
              break;
            }
            continue;
          }
          P1FrameScale sc;
          sc.key = p1_frame_key(file);
          sc.k_photo = k;
          sc.n_matched = n_matched < 0 ? 0 : n_matched;
          sc.sigma_residual_dex = sfj.value("sigma_residual_dex", 0.0);
          sc.source = sfj.value("source", std::string("photscale_sidecar"));
          sc.fitted = fit_evidence;
          scales[sc.key] = sc;
        }
        if (!scales.empty()) photscale_source = "photscale_sidecar";
      } catch (const std::exception& e) {
        return Result<void>::fail(Error(ErrorDomain::DATA,
            std::string("p1_photscale.json parse failed: ") + e.what()));
      }
    }
  }

  // 逐帧查表: frame_key(input) 或 frame_key(calibrated_<base>) 两种键都接受。
  auto find_scale = [&](const std::string& lp) -> const P1FrameScale* {
    auto it = scales.find(p1_frame_key(lp));
    if (it != scales.end()) return &it->second;
    it = scales.find(p1_frame_key(p1_calibrated_path(doc, lp)));
    if (it != scales.end()) return &it->second;
    return nullptr;
  };
  // ── FAILSEM-01: 逐帧判决补全（**只该帧**, 不再整组连坐）──────────────────
  // 逐帧各自判定三件事:
  //   · 已判 fail 的帧（拟合失败）原样保留;
  //   · 声明了标定通道但该帧不在其中 ⇒ 该帧 fail（PHOT_SCALE_MISSING）;
  //   · 标度缺**拟合证据**（fitted=false）⇒ 该帧 fail（与门禁
  //     CHK-PROVENANCE-CONSISTENCY 的 photscale_detail.fitted 判据同一口径,
  //     生产侧与门禁侧不得分歧）。
  // 通道**缺席**（既未启用 fit 通道, 也无 p1_photscale.json）时不判 fail: 那是
  // 「测光未配置」的显式降级路径（degraded_reason=photscale_absent, 见下方）。
  std::vector<char> do_apply(n_lights, 0);
  size_t n_frames_failed = 0;
  for (size_t i = 0; i < n_lights; ++i) {
    if (verdicts[i].status == "fail") { ++n_frames_failed; continue; }
    const P1FrameScale* sc = find_scale(lights[i].get<std::string>());
    if (sc == nullptr) {
      if (channel_present) {
        mark_frame_fail(i, "SCIENCE_PRECONDITION", "PHOT_SCALE_MISSING",
                        "no photometric scale for this frame in the declared calibration"
                        " channel (photscale_source=" + photscale_source + ")", "");
        ++n_frames_failed;
      }
      continue;
    }
    if (!sc->fitted) {
      mark_frame_fail(i, "SCIENCE_PRECONDITION", "PHOT_SCALE_NO_FIT_EVIDENCE",
                      "photometry fit has no provenance for " + sc->key +
                          " (n_matched=" + std::to_string(sc->n_matched) +
                          ", source undeclared); refusing to declare it applied",
                      "no_fit_evidence");
      ++n_frames_failed;
      continue;
    }
    do_apply[i] = 1;
  }
  const size_t n_frames_ok = n_lights - n_frames_failed;
  size_t n_applied = 0;
  for (size_t i = 0; i < n_lights; ++i) n_applied += do_apply[i] ? 1u : 0u;

  // ── P1-PHOT-BROKEN (c): 帧标度收集 + **组间一致性报告字段**（非门禁）─────
  // 依据 SCI-PHOT-001 §3/§6: scale 的绝对值含未建模仪器常数（可跨数量级）。
  // **负责人 2026-09-19 裁决（GAP_AUDIT §9.49 定案 2）：组间一致性不是门禁** ——
  // 帧间独立标定，各帧只对「自己的标定是否可信」负责；不同光学系统混装不得报错。
  // 此处仍**计算并落盘** `photscale_spread_dex`（PMM warning 范式，供人工审阅）。
  double photscale_spread_dex = 0.0;
  bool photscale_spread_warn = false;
  {
    // FAILSEM-01: 散度只在**已施加**的帧集合上统计（失败帧既不参与统计, 也不
    // 因组间散度改变自己的施加与否 —— 帧间独立, 门只有一个 = 单帧标定是否可信）。
    double kmin = 0.0, kmax = 0.0;
    size_t n_k = 0;
    for (size_t i = 0; i < n_lights; ++i) {
      if (!do_apply[i]) continue;
      const P1FrameScale* sc = find_scale(lights[i].get<std::string>());
      if (sc == nullptr) continue;
      if (n_k == 0 || sc->k_photo < kmin) kmin = sc->k_photo;
      if (n_k == 0 || sc->k_photo > kmax) kmax = sc->k_photo;
      ++n_k;
    }
    // ── 负责人 2026-09-19 裁决（GAP_AUDIT §9.49 定案 2）：**删除组间 k 散度门** ──
    // 原话：「极度异常值拒绝，并抛出错误，其他的合理范围都可以接受。这玩意应该是
    // 帧间独立的，为啥要组间对比」「不同光学系统的帧混装不得报错」「门只有一个：
    // 单帧标定是否可信…与其它帧无关」。
    // 独立佐证（PMM-STUDY §Q-B）：PhotometricMosaic 亦**没有任何「拒绝帧」的跨帧
    // 一致性门**，其模型本身是相对的（scale 允许任意量级，注释显式支持 12bit vs
    // 16bit、高达 2× 尺度差）；跨帧比例只用于**星点匹配预筛**，超限的后果是
    // 「这一对星不匹配」而**不是「这一帧被拒绝」**。
    // 实证（E2E 2026-09-20）：本门曾使 L4 真实数据 **photometry_applied=false**
    // （t2_m1 两帧 k 散度 0.0311 dex = 0.078 mag > 0.02 dex）⇒ **测光归一化在
    // 生产上完全不执行**，正是本裁决要消除的故障。
    // 处置：**保留 spread_dex 的计算与落盘**（供人工审阅，PMM 的 warning 范式），
    // **删除其 fail-closed 分支**；组间一致性是**语义目标，不是门禁**。
    if (n_k > 0 && kmin > 0.0) {
      const double spread_dex = std::log10(kmax / kmin);
      photscale_spread_dex = std::isfinite(spread_dex) ? spread_dex : 0.0;
      // 仅提示，不阻断：超过参考值只在 provenance 记 warning 供审阅。
      photscale_spread_warn = !(std::isfinite(spread_dex)) ||
                              spread_dex > P1_PHOT_MAX_SPREAD_DEX;
    }
  }

  Json applied_artifacts = Json::array();
  Json photscales = Json::object();
  Json photscale_detail = Json::object();
  double photscal_rep = 1.0;
  bool photometry_applied = false;
  if (n_applied > 0) {
    // PERF-P1: 帧级并行（与 p2_parallel_for 同规范）。每帧只写**自己下标**的结果槽
    // 与**自己帧**的产物路径；跨帧无浮点归约；帧序归约（applied_artifacts /
    // photscales / photscale_detail / ks）在 join 之后按下标升序执行 ⇒ 与串行逐字
    // 一致（含 Json object 的插入顺序）、与 worker 数无关。
    const uint32_t ph_workers = p1_frame_workers(doc);
    std::vector<Result<void>> f_err(n_lights, Result<void>::success());
    std::vector<std::string> f_apath(n_lights), f_key(n_lights);
    std::vector<double> f_k(n_lights, 0.0);
    std::vector<Json> f_detail(n_lights);
    p1_parallel_for(ph_workers, n_lights, p1_workers(doc), [&](uint64_t fi, uint32_t) {
      // FAILSEM-01: 只施加**产出标度**的帧; 拟合失败的帧不写 photoapplied 产物
      // （该帧的失败已记在 verdicts / manifest / p1_phot.json）。
      if (!do_apply[fi]) return;
      const std::string lp = lights[fi].get<std::string>();
      const P1FrameScale* sc = find_scale(lp);
      const std::string key = p1_frame_key(lp);
      f_key[fi] = key;
      const std::string src_path_i = p1_calibrated_path(doc, lp);
      P1Image im = p1_read_image(src_path_i);
      if (!im.ok()) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO,
            "photometry apply: cannot read " + src_path_i));
        return;
      }
      // I_photo = k_photo · I_cal (apply_photometry, in-place; 非有限像素透传)
      const int arc = calibration::apply_photometry(im.px(), im.w(), im.h(),
                                                    sc->k_photo, im.px());
      if (arc != 0) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "apply_photometry failed rc=" + std::to_string(arc) + " for " + key));
        return;
      }
      const std::string apath = p1_photoapplied_path(doc, lp);
      std::string werr;
      if (!p1_write_fits_atomic(im, apath, &werr)) {
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO,
            "photometry apply write failed: " + werr));
        return;
      }
      f_apath[fi] = apath;
      f_k[fi] = sc->k_photo;
      // P1-PHOT-BROKEN (d): 逐帧拟合 provenance（标度之外的拟合证据）。
      // photscales 只承载标量（drz 消费口径不变）; 本对象记录该标量是否来自
      // 真实拟合（fitted/n_matched/sigma_residual_dex）, 使"applied=true"可被
      // 独立核对, 而不是只能自证。
      // F-INSTR-CONFORM-FIX: 逐帧 F_instr 域 provenance —— 使"标度取自 PSF
      // 拟合域解析通量（而非检测域盒和）"可被独立核对。
      f_detail[fi] = Json{{"k_photo", sc->k_photo},
                          {"n_matched", sc->n_matched},
                          {"sigma_residual_dex", sc->sigma_residual_dex},
                          {"fitted", sc->fitted},
                          {"source", sc->source},
                          {"f_instr_domain", "psf_analytic_flux_2pi_A_sx_sy_over_3"},
                          {"n_psf_domain", sc->n_psf_domain},
                          {"n_psf_skipped", sc->n_psf_skipped}};
    });
    // 帧序归约（冻结顺序：下标升序；首个失败即返回，与串行同判据）
    std::vector<double> ks;
    for (size_t i = 0; i < n_lights; ++i) {
      if (!do_apply[i]) continue;   // 拟合失败的帧不参与施加归约
      if (!f_err[i].ok()) return f_err[i];
      applied_artifacts.push_back(f_apath[i]);
      photscales[f_key[i]] = f_k[i];
      photscale_detail[f_key[i]] = f_detail[i];
      ks.push_back(f_k[i]);
    }
    std::sort(ks.begin(), ks.end());
    photscal_rep = ks.empty() ? 1.0 : ks[ks.size() / 2];
    photometry_applied = true;
  }

  // ── P1-PHOT-BROKEN (e): provenance 自洽硬约束 (fail-closed) ─────────────
  // applied 的帧 ⟺ 该帧有标度 + 有拟合证据 + 产物已写出。修复前 11/12 板块声明
  // applied=true + photscal=1.0, 而该 1.0 是 NO_DATA 占位值（provenance 结构
  // 完整但语义为假）。FAILSEM-01 后该约束**逐帧**成立：数量必须等于实际施加的
  // 帧数（失败帧不产出标度与产物, 也不得被计入）。
  if (photometry_applied &&
      (photscales.size() != n_applied || applied_artifacts.size() != n_applied ||
       photscale_detail.size() != n_applied)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "photometry provenance inconsistent: applied frames=" +
        std::to_string(n_applied) + " but photscales=" +
        std::to_string(photscales.size()) + " detail=" +
        std::to_string(photscale_detail.size()) + " artifacts=" +
        std::to_string(applied_artifacts.size()) + " for n_lights=" +
        std::to_string(n_lights)));
  }

  // ── FREF-BASELINE-001: 逐帧测光拟合证据表（**无条件**落盘）───────────────
  // 组间 k 散度门只决定"是否把 k 施加到像素"（photometry_applied），**不得**
  // 把已经算出的逐帧拟合结果丢弃：帧级 SNR 的绝对参考通量
  //   F_ref,k = 10^(-0.4*(m_ref - ZP_k)),  ZP_k = ZP_syn,k - 2.5*log10(k_photo,k)
  // 需要它，且按 §9.49 定案 2（帧间独立；跨帧 k 不同是正常的）必须逐帧可得。
  // 无此表时 SNR 节点无法把 F_ref 锚到固定星等 ⇒ 只能退化为块级中位数。
  Json photscale_fit = Json::object();
  for (const auto& l : lights) {
    if (!l.is_string()) continue;
    const P1FrameScale* sc = find_scale(l.get<std::string>());
    if (sc == nullptr) continue;
    photscale_fit[sc->key] =
        Json{{"k_photo", sc->k_photo},
             {"n_matched", sc->n_matched},
             {"sigma_residual_dex", sc->sigma_residual_dex},
             {"fitted", sc->fitted},
             {"source", sc->source},
             {"f_instr_domain", "psf_analytic_flux_2pi_A_sx_sy_over_3"},
             {"zero_point_valid", sc->zero_point_valid},
             {"zero_point_mag", sc->zero_point_mag},
             {"zero_point_n_stars", sc->zero_point_n_stars},
             {"zero_point_scatter_mag", sc->zero_point_scatter_mag}};
  }

  // ── FAILSEM-01: 逐帧判决的落盘形态（provenance 与节点 manifest 同源）─────
  // 每帧一条 {frame_key, status, error_domain, error_status, error, ...}。
  // 下游（drizzle）按本表逐帧决定消费哪一个输入面; 门禁按本表核对
  // 「applied 帧数 = photscales 键数」与「失败帧不产出产物」。
  Json frames_status = Json::array();
  Json frame_status_map = Json::object();
  Json frame_errors = Json::array();
  Json failed_frames = Json::array();
  for (size_t i = 0; i < n_lights; ++i) {
    const std::string fkey = p1_frame_key(lights[i].get<std::string>());
    const P1FrameVerdict& v = verdicts[i];
    Json entry = Json{{"frame_key", fkey},
                      {"input_light", lights[i].get<std::string>()},
                      {"status", v.status},
                      {"photometry_applied", do_apply[i] ? true : false}};
    if (v.status == "fail") {
      // error_report 形状（LOG 合同 §5）: domain/status/source/symbol/message/
      // node/phase/degraded。稳定错误码见 P1FrameVerdict 的取值集合。
      entry["error_domain"] = v.error_domain;
      entry["error_status"] = v.error_status;
      entry["error"] = v.error;
      if (!v.degraded_reason.empty()) entry["degraded_reason"] = v.degraded_reason;
      entry["n_matched"] = v.n_matched;
      entry["sigma_residual_dex"] = v.sigma_residual_dex;
      failed_frames.push_back(fkey);
      frame_errors.push_back(Json{{"domain", v.error_domain},
                                  {"status", v.error_status},
                                  {"source", "astrocs.phase1.photometry"},
                                  {"symbol", "p1_op_photometry"},
                                  {"message", v.error},
                                  {"node", "astrocs.phase1.photometry"},
                                  {"phase", "phase1"},
                                  {"frame_key", fkey},
                                  {"degraded", false}});
    }
    frames_status.push_back(entry);
    frame_status_map[fkey] = entry;
  }

  // ── PASSBAND-IDENTITY-GATE-01: 通带身份自述（落盘 + 判据）────────────────
  // 把「配置声明的通带」与「实际用于合成 F_syn 的曲线身份」一起落进 p1_phot.json，
  // 使二者可在产品里被**独立核对**（而不是只能信任运行日志）。gate 字段的语义：
  //   "enforced (curve identity == declared filter name)" = 生产唯一实现已在装配期
  //     核对并通过（曲线对象自述 name == 库键，且与 provenance 的 curve_stats 一致）；
  //   "not_applicable (no fitted scale)" = 无拟合标度产出（标度来自侧车或通道缺席），
  //     通带身份不适用 —— 此时**不**声称已核对。
  // 依据 docs/science/PHOTOMETRY.md §2a.4（比较 sigma_residual 必须声明所用模型
  // 通带）+ §2a.5（通带形状不被零点吸收）+ docs/contracts/CONFIG_CONTRACT.md §4
  // （滤镜名解析 = 字节精确、无别名）。
  auto passband_identity_json = [&]() -> Json {
    if (!passband_identity_valid)
      return Json{{"gate", "not_applicable (no fitted scale)"},
                  {"declared_filter_passband", doc.value("filter_passband", std::string())}};
    return Json{{"gate", "enforced (curve identity == declared filter name)"},
                {"declared_filter_passband", passband_identity_declared},
                {"filter_keyword", passband_identity_filter},
                {"resolved_library_key", passband_identity_key},
                {"curve_name", passband_identity_curve_name},
                {"n_points", passband_identity_n_points},
                {"wl_min_nm", passband_identity_wl_min_nm},
                {"wl_max_nm", passband_identity_wl_max_nm},
                {"val_min", passband_identity_val_min},
                {"val_max", passband_identity_val_max},
                {"curve_source", passband_identity_curve_source}};
  };

  // ── B2-A14: 真实测光 provenance sidecar (DATA-P1-PHOTPROV-001) ─────────────
  // drizzle 消费本产物决定 PHOTSCAL/PHOTAPPL; 禁止硬编码 1。如实声明是否已对
  // 像素施加测光缩放（施加后 applied=true, operation 记录两步）。
  const std::string prov_path = out_dir + "/p1_phot.json";
  // operation 保持冻结绑定值 measure_flux（module_ports.registry.json）; 施加
  // 步骤以 pixel_scaling/apply_entry/photometry_applied 如实登记。
  Json prov = Json{{"schema", "DATA-P1-PHOTPROV-001"},
                   {"node", "astrocs.phase1.photometry"},
                   {"operation", "measure_flux"},
                   {"entry", "astrocs_phase1_photometry_v1"},
                   {"photometry_applied", photometry_applied},
                   {"photscal", photometry_applied ? photscal_rep : 1.0},
                   // pixel_scaling: applied=全帧施加 / partial=部分帧施加（其余帧
                   // 已判 fail, 见 frames[]）/ none=未施加任何帧。
                   {"pixel_scaling", n_applied == 0
                                        ? std::string("none")
                                        : (n_applied == n_lights ? std::string("applied")
                                                                 : std::string("partial"))},
                   {"photscale_source", photscale_source},
                   // 组间一致性：**报告字段，非门禁**（负责人 GAP_AUDIT §9.49 定案 2）。
                   {"photscale_spread_dex", photscale_spread_dex},
                   {"photscale_spread_warn", photscale_spread_warn},
                   {"photscale_spread_gate", "none (owner ruling 9.49: frame-independent)"},
                   // PASSBAND-IDENTITY-GATE-01: 通带身份自述（声明 vs 实际曲线）
                   {"passband_identity", passband_identity_json()},
                   {"n_frames", static_cast<uint64_t>(n_lights)},
                   // ── FAILSEM-01: 逐帧判决（下游与门禁的**唯一真相**）──────
                   // 每帧一条: status ∈ {ok, fail}。fail 帧带 error_domain /
                   // error_status / error（LOG 合同 §5 的 error_report 口径）,
                   // 且**不产出** photoapplied 产物。下游（drizzle）按本表逐帧
                   // 决定消费哪一个输入面, 不得用组级 photometry_applied 替代。
                   {"frames", frames_status},
                   {"n_frames_ok", static_cast<uint64_t>(n_frames_ok)},
                   {"n_frames_failed", static_cast<uint64_t>(n_frames_failed)},
                   {"failed_frames", failed_frames},
                   {"n_frames_applied", static_cast<uint64_t>(n_applied)}};
  // FREF-BASELINE-001: 逐帧拟合证据表无条件落盘（见上）。photscales/
  // photscale_detail 保持原语义（只描述"已施加"的那组标度），不受影响。
  prov["photscale_fit"] = photscale_fit;
  if (photometry_applied) {
    prov["apply_entry"] = "calibration::apply_photometry";
    prov["photscales"] = photscales;
    prov["photscale_detail"] = photscale_detail;
    prov["photoapplied_artifacts"] = applied_artifacts;
  }
  if (n_frames_failed == 0 && !photometry_applied) {
    // 通道缺席且无一帧施加: 「测光未配置」的**显式降级**（不是某帧拟合失败）。
    // 依据 ASTROCS_DESIGN §4.2「该步不可用时产品显式记录 degraded_reason 并
    // fail-closed」+ LOG 合同 §6 D1/D2。拟合失败**不再**走 degraded_reason ——
    // 它是显式失败（frames[].status=fail + error_*）, 由运行级判红收敛。
    prov["degraded_reason"] = "photscale_absent";
  }
  if (!p1_write_text(prov_path, prov.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_frames"] = frames.size();
  (*man)["flux_artifact"] = out_path;
  (*man)["photometry_provenance_artifact"] = prov_path;
  (*man)["photometry_applied"] = photometry_applied;
  (*man)["photscal"] = photometry_applied ? photscal_rep : 1.0;
  (*man)["photscale_source"] = photscale_source;
  // 组间一致性报告字段（非门禁；负责人 GAP_AUDIT §9.49 定案 2：帧间独立，不设组间门）。
  (*man)["photscale_spread_dex"] = photscale_spread_dex;
  (*man)["photscale_spread_warn"] = photscale_spread_warn;
  (*man)["photscale_spread_gate"] = "none (owner ruling 9.49: frame-independent)";
  // PASSBAND-IDENTITY-GATE-01: 通带身份（与 p1_phot.json 同一对象，逐字段可核）
  (*man)["passband_identity"] = passband_identity_json();
  // F-INSTR-CONFORM-FIX (SCI-PHOT-001 §9a): 本节点 F_instr 的域 = PSF 拟合域
  // 解析通量; 修复前 = 检测域 5×5 盒和。显式登记以便独立核对。
  (*man)["f_instr_domain"] = "psf_analytic_flux_2pi_A_sx_sy_over_3";
  // ── FAILSEM-01: 逐帧失败**显式上报**（节点 manifest 是运行级判红的输入）──
  // 每帧一条 error_report 形状的记录（LOG 合同 §5: domain/status/source/symbol/
  // message/degraded）; 失败帧同时进 failed_frames（供下游/门禁快速判定）。
  (*man)["frame_status"] = frame_status_map;
  (*man)["frame_errors"] = frame_errors;
  (*man)["failed_frames"] = failed_frames;
  (*man)["n_frames_ok"] = static_cast<uint64_t>(n_frames_ok);
  (*man)["n_frames_failed"] = static_cast<uint64_t>(n_frames_failed);
  (*man)["n_frames_applied"] = static_cast<uint64_t>(n_applied);
  (*man)["photoapplied_artifacts"] = applied_artifacts;
  Json artifacts = Json::array({out_path, prov_path});
  for (const auto& a : applied_artifacts) artifacts.push_back(a);
  (*man)["artifacts"] = artifacts;
  return Result<void>::success();
}

// ── NOISE-MODEL-CANON-002（负责人 §9.67 定案 2「逐像素方差接入」+ ASTROCS_DESIGN
//    §8.2:526-527「阶段内：命名块内存管线与块生命周期」）────────────────────────────────────
// A（snr_noise_model_v1 / _f64 / _fill；docs/science/NOISE_MODEL.md:71/165 与
// docs/algorithms/NOISE_ESTIMATION.md §13.1:120「生产符号唯一源」）的**调用侧配置
// 推导**共用面。两个消费点必须同源，禁止第二份策略副本（自适应 patch 网格 / 掩膜
// 半径上界 / 天空与 patch 预算 / 饱和电平解析 / 逐星掩膜四路输入）：
//   ① p1_op_noise   —— 帧级标量 + noise_* 诊断（写 p1_snr.json）
//   ② p1_op_drizzle —— 逐像素 variance **帧内命名块**（登记面 = DATA-P1-DRZ
//      §11.1:295「variance 面（可选，帧内块）float32，随 data 布局，ADU²」；
//      消费侧 = hp_drizzle_api.cpp:1018-1052，零改动）
// data/data_is_f64: 必须与 drizzle 的 "data" 块**同一数组**（同 dtype），使逐像素
//   方差的帧身份与标度自动一致。
//   **但"同一数组"不足以免除单位换算**：模型内唯一的**绝对常数**是
//   variance_floor（SCI-NOISE §7 冻结 1e-12 **ADU²**），它不随数组线性缩放。
//   数组被上游测光归一化（x' = α·x_ADU）时 floor 必须按 SNR-002 的 α² 换算，
//   否则 max(plane, floor) 会把整帧钳到 floor（M42 实测：α²≈5.7e-34、
//   真实空背景方差≈3e-31 ⇒ 全帧恒为 1e-12，噪声信息全丢）。见 data_scale 参数。
// rc 语义同 snr_noise_model_v1：0=成功（含退化兜底）/ 1=完全退化（ivar=0）/
//   3=参数非法 / -9=ABI 失配。model 由调用方 snr_noise_model_v1_free 释放
//   （**必须成对**，否则撞 DISP-NOISE-001/009 注册表泄漏）。
struct P1NoiseFrameModel {
  NoiseWeightModelV1 model{};
  int rc = 3;
  bool cfg_default_failed = false;
  double saturation_level = 0.0;
  std::string saturation_filter = "DISABLED_NO_METADATA";
  std::string saturation_source = "unset";
  // 调用方职责内的适配事实（只增诊断，不改数值；键名与 p1_op_noise 既有
  // manifest 键一致；未适配的键不出现）。
  Json diag = Json::object();
};

P1NoiseFrameModel p1_noise_model_for_frame(const void* data, bool data_is_f64,
                                           int h, int w,
                                           const std::string& frame_path,
                                           const Json& snr_cfg,
                                           const Json* src_frame,
                                           double data_scale,
                                           bool disable_spatial_field) {
  P1NoiseFrameModel out;
  SnrNoiseModelConfig ncfg{};
  if (snr_noise_model_v1_default_config(&ncfg) != 0) {
    out.cfg_default_failed = true;
    out.rc = 3;
    return out;
  }
  // ── 单位链（SCI-NOISE §3/§7 + SNR-002 尺度律）─────────────────────────────
  // variance_floor 的冻结单位是 **ADU²**（SCI-NOISE §7「variance_floor 单位为 ADU²、
  // 默认 1e-12 属冻结项」；§3「x, σ_bg, √variance: ADU（或 e⁻，同输入标度）」）。
  // 本函数收到的数组可能已被上游测光节点按 x' = α·x_ADU 归一化（α = frame_photscal）。
  // floor 与数组必须**同标度**：α² ≈ 5.7e-34 时 1e-12 比该帧真实空背景方差
  // （≈3e-31）大 19 个数量级 ⇒ fill_impl 的 max(a+b·x+c·y, floor) 把**每个像素**
  // 钳成常数 1e-12，产品 variance 退化为纯几何量（与 signal 无关、跨帧恒定）。
  // 按 SNR-002「x'=αx ⇒ var'=α²var, ivar'=ivar/α²」把 floor 换算到数组标度；
  // ADU 面（α=1）逐位不变。**冻结默认值本身不改**（仍由 default_config 给出）。
  const double floor_adu2 = ncfg.variance_floor;
  if (std::isfinite(data_scale) && data_scale > 0.0 && data_scale != 1.0) {
    ncfg.variance_floor = floor_adu2 * data_scale * data_scale;
    out.diag["noise_variance_floor_unit_scale"] =
        Json{{"floor_adu2", floor_adu2},
             {"data_scale", data_scale},
             {"floor_array_units", ncfg.variance_floor}};
  }
  // 平面场禁用通道：调用方在**线性平面不是合法方差场**时的 fail-closed 退化
  // （退回 SCI-NOISE §4/§5 已规定的全局常量场）。默认不启用 ⇒ 数值逐位不变。
  if (disable_spatial_field) {
    ncfg.enable_spatial_field = 0;
    out.diag["noise_spatial_field_disabled"] = true;
  }
  // 自适应 patch 网格（调用方职责，非公式变更）：默认 8x8 网格要求每 patch
  // >= min_patch_samples(64) 个天空样本；小画幅帧（如测试 fixture 32x32）
  // 在 8x8 下每 patch 仅 16 px ⇒ 全部不合格 ⇒ 整帧退化（A 科学上正确）。
  // 生产按画幅收缩网格，使每 patch 仍 >= 64 样本；下界 2（cfg 声明 >=2），
  // 上界 = 配置值（默认 8，大画幅保持设计默认）。
  {
    const double px_total = static_cast<double>(w) * static_cast<double>(h);
    const double min_s = static_cast<double>(std::max(1, ncfg.min_patch_samples));
    const int g_adapt = static_cast<int>(std::floor(std::sqrt(px_total / min_s)));
    const int gx = std::max(2, std::min(ncfg.patch_grid_x, g_adapt));
    const int gy = std::max(2, std::min(ncfg.patch_grid_y, g_adapt));
    if (gx != ncfg.patch_grid_x || gy != ncfg.patch_grid_y) {
      out.diag["noise_patch_grid_adapted"] = Json{{"from_x", ncfg.patch_grid_x},
                                                  {"from_y", ncfg.patch_grid_y},
                                                  {"to_x", gx}, {"to_y", gy}};
    }
    ncfg.patch_grid_x = gx;
    ncfg.patch_grid_y = gy;
    // 掩膜半径上界与天空预算同样必须**帧内可行**（调用方职责）：
    //   rmax = source_mask_radius_px * mask_radius_scale 默认 10*6 = 60 px；
    //   在 32x32 帧上一个中心星即盖满全帧 ⇒ 全部 patch 被掩 ⇒ 整帧退化。
    //   掩膜大于画幅、预算大于画幅都是**无意义约束**，按画幅收缩。
    const double side_min = static_cast<double>(std::min(w, h));
    const double rmax_cap = std::max(2.0, side_min / 4.0);
    const double rmax_now = ncfg.source_mask_radius_px *
                            std::max(1.0, ncfg.mask_radius_scale);
    if (rmax_now > rmax_cap && ncfg.source_mask_radius_px > 0.0) {
      out.diag["noise_mask_rmax_adapted"] = Json{{"from", rmax_now}, {"to", rmax_cap}};
      ncfg.mask_radius_scale = rmax_cap / ncfg.source_mask_radius_px;
    }
    const uint64_t px_u = static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
    const uint32_t sky_cap = static_cast<uint32_t>(std::max<uint64_t>(64, px_u / 2));
    if (ncfg.mask_budget_min_sky > sky_cap) {
      out.diag["noise_mask_sky_budget_adapted"] =
          Json{{"from", ncfg.mask_budget_min_sky}, {"to", sky_cap}};
      ncfg.mask_budget_min_sky = sky_cap;
    }
    const uint32_t patch_cap = static_cast<uint32_t>(
        std::max(4, (ncfg.patch_grid_x * ncfg.patch_grid_y) / 2));
    if (ncfg.mask_budget_min_patches > patch_cap) {
      out.diag["noise_mask_patch_budget_adapted"] =
          Json{{"from", ncfg.mask_budget_min_patches}, {"to", patch_cap}};
      ncfg.mask_budget_min_patches = patch_cap;
    }
  }
  ncfg.gain_e_per_adu = snr_cfg.value("gain_e_per_adu", 0.0);
  ncfg.read_noise_e = snr_cfg.value("read_noise_e", 0.0);
  // 饱和电平：cfg 优先，其次帧头 SATURATE/DATAMAX；未提供必须**显式**声明降级
  // （SCI NOISE_MODEL §4「饱和域」claim SC-008，禁止静默）。
  double sat_level = snr_cfg.value("saturation_level", 0.0);
  std::string sat_filter = "DISABLED_NO_METADATA";
  std::string sat_source = "unset";
  if (std::isfinite(sat_level) && sat_level > 0.0) {
    sat_filter = "ENABLED";
    sat_source = "config";
  } else {
    sat_level = 0.0;
    const double hdr_sat = p1_fits_saturation_level(frame_path);
    if (std::isfinite(hdr_sat) && hdr_sat > 0.0) {
      sat_level = hdr_sat;
      sat_filter = "ENABLED";
      sat_source = "fits_header";
    }
  }
  ncfg.saturation_level = sat_level;
  out.saturation_level = sat_level;
  out.saturation_filter = sat_filter;
  out.saturation_source = sat_source;
  // 逐星掩膜四路输入（MASK-002/claim SC-009）：上游 p1_sources.json 的 sources[]
  // 已含 x/y/flux/fwhm_px ⇒ 无需新上游产物。判据与 SNR 交付样本一致
  // （flux>0 ∧ fwhm_px>0），保证掩膜与权重用同一批星。
  std::vector<double> nm_sx, nm_sy, nm_sf, nm_sw;
  if (src_frame != nullptr && src_frame->contains("sources") &&
      (*src_frame)["sources"].is_array()) {
    for (const auto& s : (*src_frame)["sources"]) {
      if (!s.is_object()) continue;
      const double x = s.value("x", std::numeric_limits<double>::quiet_NaN());
      const double y = s.value("y", std::numeric_limits<double>::quiet_NaN());
      const double fl = s.value("flux", 0.0);
      const double fw = s.value("fwhm_px", 0.0);
      if (!std::isfinite(x) || !std::isfinite(y)) continue;
      if (!(fl > 0.0) || !(fw > 0.0)) continue;
      // ── CHAIN-WIRE-ADAPT-01 / W3: 掩膜标度必须与**本帧数组**同标度 ──────────
      // §5a 的 Moffat 半径式 r_local = α_psf·sqrt((F(β−1)/(π α_psf² k σ_bg))^(1/β) − 1)
      // **只在 F 与 σ_bg 处于同一标度时**才对整体线性缩放不变（比值 F/σ_bg 无量纲）。
      // 本函数收到的数组可能是 photoapplied_<base> = α·ADU（α = data_scale =
      // frame_photscal ≈ 5.9e-17），而 p1_sources.json 的 flux 由 star-psf 节点在
      // cleaned_<base>（= ADU）上测得 ⇒ 直接传入会让 F/σ_bg 放大 1/α ≈ 1.7e16 倍，
      // 半径一律顶到硬上界 rmax = 60 px（MASK-002 的「硬上界」退化成操作默认值）。
      // 实测（run/SCI-VAR-ZERO-01/REPORT.md §3.1）: drizzle 路径四帧
      // mask_radius_p50 **恒为 60.000000**、mask_frac 0.199–0.445，而同一帧的 SNR
      // 节点（:5832-5834，cleaned 面、data_scale=1.0）给出 r_p50=4.84、
      // mask_frac=0.018。§5a 的设计意图是「逐星半径由掩膜边缘残余面亮度 ≤ k·σ_bg
      // 导出」⇒ 标度错则逐星半径从未生效，掩膜退化为统一 rmax（§5a 明确
      // 「60 px 只能作硬上界」、固定 60 px 的 worst |σ 偏差| 6.93% vs 逐星 1.11%）。
      // 处置: 把 flux 换算到数组标度（× data_scale；ADU 面 data_scale=1 时逐位不变）。
      // 注意 data_scale 的语义链已由 :5401-5408 的 variance_floor 换算确立
      // （x' = α·x ⇒ var' = α²·var），本处是同一 α 在通量上的对应换算。
      const double fl_scaled = fl * data_scale;
      if (!(fl_scaled > 0.0) || !std::isfinite(fl_scaled)) continue;
      nm_sx.push_back(x); nm_sy.push_back(y);
      nm_sf.push_back(fl_scaled); nm_sw.push_back(fw);
    }
  }
  const int n_stars = static_cast<int>(nm_sx.size());
  // W3 审计量：掩膜通量是否真的做过标度换算（data_scale≠1 时逐星输入已乘 α）。
  // 键只在换算生效时出现（ADU 面不出现，逐位不变）。
  if (n_stars > 0 && std::isfinite(data_scale) && data_scale > 0.0 &&
      data_scale != 1.0) {
    out.diag["noise_mask_flux_scale"] =
        Json{{"data_scale", data_scale},
             {"n_stars_scaled", n_stars},
             {"rule", "SCI-NOISE §5a: the per-star flux and sigma_bg must share the"
                      " frame array scale (F/sigma_bg is dimensionless)"}};
  }
  if (data_is_f64) {
    out.rc = snr_noise_model_v1_f64(
        static_cast<const double*>(data), h, w, nullptr,
        nm_sx.empty() ? nullptr : nm_sx.data(),
        nm_sy.empty() ? nullptr : nm_sy.data(),
        nm_sf.empty() ? nullptr : nm_sf.data(),
        nm_sw.empty() ? nullptr : nm_sw.data(), n_stars, &ncfg, &out.model);
  } else {
    out.rc = snr_noise_model_v1(
        static_cast<const float*>(data), h, w, nullptr,
        nm_sx.empty() ? nullptr : nm_sx.data(),
        nm_sy.empty() ? nullptr : nm_sy.data(),
        nm_sf.empty() ? nullptr : nm_sf.data(),
        nm_sw.empty() ? nullptr : nm_sw.data(), n_stars, &ncfg, &out.model);
  }
  return out;
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
  // NOISE-MODEL-CANON-001: 旧 wrapper_phase1::NoiseModel (B) 已退役 ——
  // 本函数改用 snr_noise_model_v1 (A)，配置/模型对象在逐帧循环内构造与释放。

  // ── P8: 上游 DATA-P1-SOURCES = 逐源 SNR 目录的唯一来源 (fail-fast) ──
  const std::string src_path = out_dir + "/p1_sources.json";
  Json src_frames = Json::array();
  {
    if (!aio_fs::exists(src_path))
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "p1_sources.json missing (upstream star-psf artifact required): " + src_path));
    std::string stext;
    if (!aio_fs::read_all(src_path, &stext))
      return Result<void>::fail(Error(ErrorDomain::IO, "cannot open: " + src_path));
    try {
      const Json sj = Json::parse(stext);
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
  // ── WEIGHT-SCI-001: 显式配置的组内公共 F_ref（首选路径）───────────────
  // 缺失/非有限/<=0 = 未给出 ⇒ 走下方块级两遍法；两者都不可得 ⇒ 逐帧
  // compute_snr_frame_science fail-closed（不写伪帧级 SNR 键）。
  double configured_ref_flux = 0.0;
  bool has_configured_ref_flux = false;
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
    // F_ref 由下方"组内公共 F0"逻辑统一写入 sci_cfg.reference_flux_adu。
    // 显式给出但非法（非有限/<=0）⇒ DATA fail-closed：不得静默回退到块级中位数
    // （否则用户显式值被无声忽略）。键**缺失**才走两遍法。
    if (sc.contains("reference_flux_adu")) {
      configured_ref_flux = sc.value("reference_flux_adu", 0.0);
      if (!std::isfinite(configured_ref_flux) || !(configured_ref_flux > 0.0))
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "snr.reference_flux_adu must be finite and > 0 (group-common F_ref)"));
      has_configured_ref_flux = true;
    }
    snr_max_sources = sc.value("max_sources", 0);
    if (snr_max_sources < 0) snr_max_sources = 0;   // <0 非法 -> 视为不限
  }

  // ── WEIGHT-SCI-001 (2026-09-18): 组内公共参考通量 F0 ─────────────────────
  // 依据 reports/RELEASE-02/weight-sci-ruling.md（配对性定理）:
  //   SNR_f = a_f·F0/σ_f  ⇒  SNR_f²/F0² = a_f²/σ_f² = w_f
  // 成立当且仅当分母 F0 与定义 SNR 时所用参考通量是同一个。逐帧 F_ref 回退已
  // 在 snr_frame_science.cpp 删除（缺失即 fail-closed）。因此本节点必须为
  // **整个 input_lights 数据块**选定一个公共 F0，并对所有帧传入同一值：
  //   (a) 首选: snr.reference_flux_adu 显式给出 ⇒ F0 为全局固定值（闸门恒过）;
  //   (b) 次选: 两遍法 —— 块级检出通量中位数（逐帧检出通量中位数 → 块中位数）。
  //      仅当一次 Phase1 run 的帧即 Phase2 组时闸门才过（裁定 §7.1 C2(b)）。
  // 逐帧 F_ref 禁止（丢 a_f² 且存头 SNR 混入本帧检出亮度）。
  auto find_src_frame = [&](const std::string& base) -> const Json* {
    for (const auto& fr : src_frames) {
      if (fr.is_object() && fr.value("file", std::string()) == base) return &fr;
    }
    return nullptr;
  };
  auto median_of_vec = [](std::vector<double> v) -> double {
    if (v.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n % 2 == 1) return v[n / 2];
    return 0.5 * (v[n / 2 - 1] + v[n / 2]);
  };
  auto all_sources_of = [](const Json* src_frame) -> Json {
    return (src_frame->contains("sources") && (*src_frame)["sources"].is_array())
               ? (*src_frame)["sources"]
               : Json::array();
  };
  // 交付 SNR 样本行构造（判据与 compute_snr_frame_science 内部一致:
  // flux>0 ∧ fwhm_px>0），并施加 snr.max_sources 显式上限。两遍法与主循环
  // **共用本 lambda** ⇒ F0 与逐帧样本定义严格一致（与 psf.max_stars 解耦）。
  auto build_snr_rows =
      [&](const Json& all_sources, std::size_t* n_available_out,
          bool* truncated_out) {
        std::vector<astrocs::phase1::SnrSourceRow> rows;
        rows.reserve(all_sources.size());
        for (const auto& s : all_sources) {
          astrocs::phase1::SnrSourceRow row;
          row.id = s.value("id", std::string());
          row.flux_adu = s.value("flux", 0.0);
          row.fwhm_px = s.value("fwhm_px", 0.0);
          if (!(std::isfinite(row.flux_adu) && row.flux_adu > 0.0)) continue;
          if (!(std::isfinite(row.fwhm_px) && row.fwhm_px > 0.0)) continue;
          rows.push_back(std::move(row));
        }
        if (n_available_out) *n_available_out = rows.size();
        if (truncated_out) *truncated_out = false;
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
          if (truncated_out) *truncated_out = true;
        }
        return rows;
      };

  std::string ref_flux_source = "unavailable";
  double group_ref_flux = std::numeric_limits<double>::quiet_NaN();
  if (has_configured_ref_flux) {
    group_ref_flux = configured_ref_flux;
    ref_flux_source = "config";
  } else {
    // 两遍法 pass-1: 逐帧检出通量中位数 → 块级中位数 = F0（确定性; 只读
    // 上游 p1_sources.json, 不读图像）。
    std::vector<double> frame_flux_medians;
    for (const auto& l : doc["input_lights"]) {
      if (!l.is_string()) continue;
      const std::string path = p1_cleaned_input_path(doc, l.get<std::string>());
      const Json* src_frame = find_src_frame(p1_base_name(path));
      if (src_frame == nullptr) continue;
      const Json all_sources = all_sources_of(src_frame);
      std::size_t n_avail = 0;
      bool trunc = false;
      const std::vector<astrocs::phase1::SnrSourceRow> rows =
          build_snr_rows(all_sources, &n_avail, &trunc);
      std::vector<double> fluxes;
      fluxes.reserve(rows.size());
      for (const auto& r : rows) fluxes.push_back(r.flux_adu);
      const double fm = median_of_vec(fluxes);
      if (std::isfinite(fm) && fm > 0.0) frame_flux_medians.push_back(fm);
    }
    const double block_med = median_of_vec(frame_flux_medians);
    if (std::isfinite(block_med) && block_med > 0.0) {
      group_ref_flux = block_med;
      ref_flux_source = "group_median";
    }
  }
  // 所有帧共用同一 F0（组内公共）。不可得时保持 0.0 ⇒ 逐帧 fail-closed。
  if (std::isfinite(group_ref_flux) && group_ref_flux > 0.0)
    sci_cfg.reference_flux_adu = group_ref_flux;

  // ── FREF-BASELINE-001: 固定参考星等的**绝对共同基准** ────────────────────
  // 负责人裁决（GAP_AUDIT §9.49 定案 7）: 「直接用 6 等星（或一个数值表示比较
  // 正常的星等来做基准就行）」。据此把帧级 SNR 的参考通量从"块级检出通量
  // 中位数"（数据派生、随帧集漂移）改为**同一颗参考星**在各帧的仪器通量:
  //
  //     F_ref,k = 10^(-0.4*(m_ref - ZP_k))      [ADU]
  //     ZP_k    = ZP_syn,k - 2.5*log10(k_photo,k)
  //
  // ZP_syn 由 Gaia DR3 XP **绝对**谱 + 本帧滤光片/QE 曲线正向合成
  // （frame_photometry_fit.cpp），只依赖 (filter, QE, 天区星族)。
  // 由此同时满足三条硬要求:
  //   (i)  帧间可比: 各帧报的是**同一颗**参考星的 SNR, 不再混入本帧检出亮度;
  //   (ii) 帧间独立: F_ref 只依赖本帧自身的测光标定 ⇒ **不需要"组"的概念**
  //        （§9.49 定案 2; 跨帧 k_photo 不同是正常的, 不是错误）;
  //   (iii) 配对性: 头部 ASTROCS_REFERENCE_FLUX 写**物理公共锚**
  //        F0 = 10^(-0.4*(m_ref - ZP_syn_block)), 对同波段同星场恒为同一数
  //        ⇒ 闸门恒过, 且 w = SNR_f²/F0² = a_f²/σ_f²（WEIGHT-SCI-001
  //        Convention A; 逐帧 F_ref,f 会丢掉 a_f²）。
  // ZP_syn 取块中位数: 它是 (filter, QE, 星族) 的系统常数, 逐帧差异只来自锥形
  // 边界抽样; 取中位数使 F0 成为**单一常数**（闸门要求），而 F_ref,k 仍逐帧。
  // 不可得（无 photscale_fit / zero_point_valid=false）⇒ 保持既有来源, 不伪造。
  double ref_mag = 6.0;
  if (doc.contains("snr") && doc["snr"].is_object())
    ref_mag = doc["snr"].value("reference_mag", 6.0);
  const bool ref_mag_usable = std::isfinite(ref_mag) && ref_mag > -30.0 && ref_mag < 60.0;

  std::map<std::string, double> kphoto_of_key;   // p1_frame_key -> k_photo
  std::map<std::string, double> zp_inst_of_key;  // p1_frame_key -> ZP_k [mag]
  std::vector<double> zp_syn_vals;
  bool phot_fit_available = false;
  if (ref_mag_usable) {
    const std::string pp = out_dir + "/p1_phot.json";
    if (aio_fs::exists(pp)) {
      std::string ptext;
      if (aio_fs::read_all(pp, &ptext)) {
        try {
          const Json pj = Json::parse(ptext);
          if (pj.is_object() && pj.contains("photscale_fit") &&
              pj["photscale_fit"].is_object()) {
            for (const auto& l : doc["input_lights"]) {
              if (!l.is_string()) continue;
              const std::string key = p1_frame_key(l.get<std::string>());
              if (!pj["photscale_fit"].contains(key)) continue;
              const Json& e = pj["photscale_fit"][key];
              if (!e.is_object()) continue;
              if (!e.value("zero_point_valid", false)) continue;
              const double kp = e.value("k_photo", 0.0);
              const double zps = e.value("zero_point_mag", 0.0);
              if (!(std::isfinite(kp) && kp > 0.0)) continue;
              if (!std::isfinite(zps)) continue;
              kphoto_of_key[key] = kp;
              zp_inst_of_key[key] = zps - 2.5 * std::log10(kp);
              zp_syn_vals.push_back(zps);
            }
          }
        } catch (const std::exception&) {
          kphoto_of_key.clear();
          zp_inst_of_key.clear();
          zp_syn_vals.clear();
        }
      }
    }
    phot_fit_available = !zp_syn_vals.empty();
  }

  double ref_flux_common = std::numeric_limits<double>::quiet_NaN();
  double ref_zero_point_syn = std::numeric_limits<double>::quiet_NaN();
  if (ref_mag_usable && phot_fit_available && !has_configured_ref_flux) {
    std::vector<double> zs = zp_syn_vals;
    std::sort(zs.begin(), zs.end());
    const std::size_t zn = zs.size();
    const double zp_syn = (zn % 2 == 1) ? zs[zn / 2]
                                        : 0.5 * (zs[zn / 2 - 1] + zs[zn / 2]);
    const double f0 = std::pow(10.0, -0.4 * (ref_mag - zp_syn));
    if (std::isfinite(zp_syn) && std::isfinite(f0) && f0 > 0.0) {
      ref_zero_point_syn = zp_syn;
      ref_flux_common = f0;
      ref_flux_source = "fixed_magnitude";
    }
  }

  // PERF-P1: 帧级并行（与 p2_parallel_for 同规范）。每帧只写**自己下标**的结果槽与
  // 诊断键槽；跨帧无浮点归约；帧序归约（诊断键 / 错误选择 / frames）在 join 之后
  // 按下标升序执行 ⇒ 与串行逐字一致、与 worker 数无关。
  // 线程安全前提（本轮实测核查）: noise model 的 floor 注册表原为**无锁全局 map**
  // （noise_model.cpp），已改为互斥量保护 + 热路径零加锁（PERF-P1 同批修复）；
  // find_src_frame/all_sources_of/build_snr_rows/median_of_vec 均为只读或纯函数。
  const uint32_t nz_workers = p1_frame_workers(doc);
  const size_t nz_lights = doc["input_lights"].size();
  std::vector<Result<void>> f_err(nz_lights, Result<void>::success());
  std::vector<int> f_errkind(nz_lights, 0);   // 0=无 1=input 2=noise_model_config 3=abi_or_input
  std::vector<Json> f_frame(nz_lights);
  std::vector<Json> f_diag(nz_lights);
  p1_parallel_for(nz_workers, nz_lights, p1_workers(doc), [&](uint64_t fi, uint32_t) {
    // cosmetic 下游（cos → psf → phot → snr）: 消费 artifact:cos 产物
    const std::string path =
        p1_cleaned_input_path(doc, doc["input_lights"][fi].get<std::string>());
    P1Image im = p1_read_image(path);
    if (!im.ok()) {
      f_errkind[fi] = 1;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + path));
      return;
    }
    // FP64 模式（aio_internal_is_fp64）下 aio_read 只填 data_f64、data=NULL
    // （aio_fits.cpp:875-877）⇒ 按 dtype 取指针喂 A（v1/v1_f64 同语义），取代旧
    // 实现 std::vector<float> px(im.px(), ...)（FP64 下是对空指针做构造）。
    const bool im_f64 =
        (aio_get_dtype(im.p) == 1) && (aio_get_pixel_data_f64(im.p) != nullptr);
    const void* nm_data = im_f64
        ? static_cast<const void*>(aio_get_pixel_data_f64(im.p))
        : static_cast<const void*>(im.px());
    const std::string base = p1_base_name(path);

    // ── NOISE-MODEL-CANON-001（负责人 §9.67 定案 3「选对的」）──────────────
    // 改用冻结 SCI-NOISE-001 §5/§5a 的**唯一实现**（docs/science/NOISE_MODEL.md:71/165;
    // ALG §13.1:120「生产符号唯一源」）。旧 wrapper_phase1::NoiseModel 是它的
    // **退化子集**（ALG §13.5:257-262）= 无掩膜/无 5σ 裁剪/无饱和过滤/无 patch 网格/
    // 无平面场/无天空预算门的整帧兜底支；其公式在自述域（**空白背景像素集**）内正确，
    // 但生产曾把**整帧**喂入 ⇒ 有星时整帧 MAD 偏差 +2.6%..+21.6%（超 SCI §7 的 2%
    // 与 §11 的 5% oracle），而本实现全算法 <=0.35%（独立复现, 3 seed）。已退役。
    const Json snr_cfg = (doc.contains("snr") && doc["snr"].is_object())
                             ? doc["snr"] : Json::object();
    // 逐星掩膜与 background 键共用同一上游帧条目（p1_sources.json frames[]）。
    const Json* nm_src = find_src_frame(base);
    // A 的唯一调用面（NOISE-MODEL-CANON-002）: 与 drizzle 节点的逐像素 variance
    // 帧内块共用 p1_noise_model_for_frame, 禁止第二份配置推导副本。
    // 本节点消费 cosmetic 下游的 cleaned_<base> 面（ADU 标度，见 p1_snr.json 的
    // background/frame_depth_flux5_adu 均为 ADU）⇒ 数组标度 α=1，floor 1e-12 ADU²
    // 量纲正确，逐位不变（SCI-NOISE §3/§7）。
    P1NoiseFrameModel nmc = p1_noise_model_for_frame(
        nm_data, im_f64, static_cast<int>(im.h()), static_cast<int>(im.w()),
        path, snr_cfg, nm_src, /*data_scale=*/1.0, /*disable_spatial_field=*/false);
    // 调用侧适配事实（键名与既有 manifest 键一致；未适配的键不出现）。
    // PERF-P1: 写入 man 的动作移出并行体（逐帧暂存，join 后按帧序覆盖）。
    f_diag[fi] = nmc.diag;
    if (nmc.cfg_default_failed) {
      f_errkind[fi] = 2;
      f_err[fi] = Result<void>::fail(
          Error(ErrorDomain::INTERNAL, "snr_noise_model_v1_default_config failed"));
      return;
    }
    if (nmc.rc == 3 || nmc.rc == -9) {
      // DISP-NOISE-001/009: A 的 floor 注册表以 model 指针记账 ⇒ 任何非成功返回
      // 也必须 _free 配对（A 的 malloc 失败路径已在 A 内自释放；未注册时 no-op）。
      snr_noise_model_v1_free(&nmc.model);
      f_errkind[fi] = 3;
      f_err[fi] = Result<void>::fail(
          Error(ErrorDomain::DATA, "snr_noise_model_v1 rejected input"));
      return;
    }
    NoiseWeightModelV1& nm = nmc.model;
    const double sat_level = nmc.saturation_level;
    const std::string sat_filter = nmc.saturation_filter;
    const std::string sat_source = nmc.saturation_source;
    // rc=1 = 完全退化（无合格 patch 且全局兜底也退化）⇒ ivar=0，**不传播、不伪造权重**
    // （SCI NOISE_MODEL §7:106「空 support 不传播」）。
    const bool nm_degenerate = (nmc.rc == 1) || (nm.degenerate != 0);
    const double nm_sigma = nm_degenerate ? 0.0 : nm.sigma_bg_global;
    const double nm_variance = nm_degenerate ? 0.0 : nm.variance_bg_global;
    const double nm_ivar = nm_degenerate ? 0.0 : nm.ivar_bg_global;
    Json frame = Json{{"file", base},
                      {"variance", nm_variance}, {"ivar", nm_ivar},
                      {"sigma", nm_sigma},
                      {"background", (nm_src != nullptr && nm_src->contains("background"))
                                         ? (*nm_src)["background"] : Json(nullptr)},
                      {"valid", !nm_degenerate},
                      {"reason", nm_degenerate
                                     ? std::string("DEGENERATE_EMPTY_SUPPORT")
                                     : std::string("ok")}};
    // NOISE-MODEL-CANON-001 诊断键（只增不改）：下游可据此 fail-closed。
    frame["noise_model"] = "snr_noise_model_v1";
    frame["noise_model_source"] = static_cast<int>(nm.source);
    frame["noise_degenerate"] = nm_degenerate;
    frame["noise_has_spatial_field"] = (nm.has_spatial_field != 0);
    frame["noise_n_control_points"] = nm.n_control_points;
    frame["noise_n_qualified_patches"] = nm.n_qualified_patches;
    frame["noise_n_rejected_patches"] = nm.n_rejected_patches;
    frame["noise_mask_degraded"] = nm.mask_degraded;
    frame["noise_mask_radius_p50"] = nm.mask_radius_p50;
    frame["noise_mask_frac"] = nm.mask_frac;
    frame["noise_saturation_filter"] = sat_filter;
    frame["noise_saturation_level"] = sat_level;
    frame["noise_saturation_source"] = sat_source;
    snr_noise_model_v1_free(&nm);   // 与 v1 成对，避免 DISP-NOISE-001/009 注册表泄漏
    // ── P8: 逐源科学 SNR (仅当上游目录含同帧时附加) ──
    frame["snr_schema"] = "DATA-P1-SNR/3";
    frame["snr_definition"] =
        "SNR_F = F/sigma_F (Horne 1986 optimal extraction; "
        "sigma_F^-2 = sum_i P_i^2/sigma_i^2); frame-level science benchmark = "
        "5-sigma point-source depth (SCI-CW-001 2a)";
    // ── SNR 口径（本帧 σ_F 是否含**源光子散粒项**）──────────────────────────
    // 源项 = F·P_i/g 含 1/g ⇒ 增益未知（gain<=0）时该项**不可计算**，σ_F 只剩天空项
    // ⇒ SNR 系统性**偏高**，是**上界**而不是绝对值（解析式
    // SNR_rep/SNR_true = sqrt(1 + F/σ_bg²)，φ=0.5 时偏高 41%、φ=0.95 时 347%）。
    // 必须显式发布：最高设计 §2.2 的交付物是**绝对 SNR**，若产品不记口径，消费者
    // 会把无增益上界当成绝对 SNR 用（下游定权 w = SNR²/F_ref² 直接受此影响）。
    const bool gain_known =
        (sci_cfg.gain_e_per_adu > 0.0) && std::isfinite(sci_cfg.gain_e_per_adu);
    frame["snr_gain_e_per_adu"] =
        gain_known ? Json(sci_cfg.gain_e_per_adu) : Json(nullptr);
    frame["snr_source_shot_term_included"] = gain_known;
    frame["snr_caliber"] =
        gain_known ? Json("absolute_flux_type_snr") : Json("upper_bound_no_gain");
    frame["snr_degraded_reason"] =
        gain_known
            ? Json(nullptr)
            : Json("gain_unknown: source shot-noise term F*P_i/g is not computable "
                   "without g; sigma_F is sky-limited and SNR is an UPPER BOUND");
    const Json* src_frame = find_src_frame(base);
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
      // SCI-B D1 定案 (07_noise_snr.md 4.2a): noise_sigma 来自
      // StarDetector::estimate_background 的**整帧 2 轮 median±3*1.4826*MAD 裁剪后 RMS**
      // (盲检测 detect() 与星表引导路径同用该估计器), 是**含读出噪声的经验总 rms**
      // ⇒ 声明语义, 禁止在 snr_science 里再加一次 (RN/g)^2
      // (修复前读噪双计: sigma_F 高估 +12.8%~+34.0%)。
      // 注: noise_model 的 1.4826*MAD 稳健尺度是**另一个**生产者(承载逐像素 variance /
      // variance_bg_global), 不是本处入参。
      cfg.sigma_sky_source = SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS;
      // ── FREF-BASELINE-001: 逐帧参考通量 = 固定星等 m_ref 在本帧的仪器通量 ──
      // 只读本帧自身的测光标定（k_photo, ZP_syn）⇒ 帧间独立, 无组概念。
      // 缺该帧标定 ⇒ reference_flux_adu=0 ⇒ 本帧 fail-closed（不伪造、不回退
      // 到块级中位数, 否则同一批产品里混两种参考, 帧间不可比）。
      const std::string fkey =
          p1_frame_key(doc["input_lights"][fi].get<std::string>());
      double frame_zp_inst = std::numeric_limits<double>::quiet_NaN();
      double frame_kphoto = std::numeric_limits<double>::quiet_NaN();
      if (ref_flux_source == "fixed_magnitude") {
        auto kp_it = kphoto_of_key.find(fkey);
        auto zp_it = zp_inst_of_key.find(fkey);
        if (kp_it != kphoto_of_key.end() && zp_it != zp_inst_of_key.end()) {
          frame_kphoto = kp_it->second;
          frame_zp_inst = zp_it->second;
          const double fref = std::pow(10.0, -0.4 * (ref_mag - frame_zp_inst));
          cfg.reference_flux_adu =
              (std::isfinite(fref) && fref > 0.0) ? fref : 0.0;
          // 修 G2（m_5 从不产出）: 帧自身测光零点 ⇒ m_5 = ZP_k - 2.5*log10(F_5)
          cfg.zero_point_mag = frame_zp_inst;
        } else {
          cfg.reference_flux_adu = 0.0;
        }
      }
      // ── P14-N-08/N-09 (RQS V2-N-08 + V2-N-09): 交付 SNR 样本真实性 ──────
      // 交付样本 = DATA-P1-SOURCES.sources 的**全部测光有效源** (flux>0 且
      // fwhm_px>0), **不再**取 psf_params —— 后者是受性能开关 psf.max_stars
      // (默认 5000) 截断的「最亮子集」, 会被静默当成 SNR 目录 ⇒ 交付的
      // median_snr / frame_depth_m5_mag 由最亮 ≤5000 颗决定, 系统性偏乐观。
      // 本实现与 psf.max_stars 解耦: 后者不得改变交付的 SNR/深度数值。
      // 交付 SNR 样本行构造与 snr.max_sources 截断统一走 build_snr_rows
      // （两遍法 F0 与逐帧样本定义共用同一实现; 测光有效判据 flux>0, fwhm>0，
      //  使 n_snr_available 如实反映可用样本）。
      const Json all_sources = all_sources_of(src_frame);
      std::size_t n_snr_available = 0;
      bool snr_sample_truncated = false;
      std::vector<astrocs::phase1::SnrSourceRow> rows =
          build_snr_rows(all_sources, &n_snr_available, &snr_sample_truncated);
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
      // WEIGHT-SCI-001 provenance: F_ref 的作用域与来源。scope="group" 表示
      // flux_adu 是**组内公共**参考通量 F0（snr_f 亦定义在该 F0 下，二者配对）;
      // reference_flux_source ∈ {"config","group_median","unavailable"}。
      const bool fref_fixed = (ref_flux_source == "fixed_magnitude");
      frame["snr_reference"] = Json{
          {"profile", "median_fwhm_of_catalogue_sky_limited"},
          // scope="group": flux_adu 是块级公共 F0, 且 snr_f 亦定义在该 F0 下
          // （二者配对, 见 WEIGHT-SCI-001）。
          // scope="frame_independent_fixed_magnitude": flux_adu 是**固定参考星等
          // m_ref 在本帧的仪器通量** F_ref,k（逐帧, 只依赖本帧标定）; 头部写的是
          // flux_common（物理公共锚 F0, 对同波段同星场恒为同一数）⇒ 配对性
          // w = SNR_f²/F0² = a_f²/σ_f² 成立（Convention A）。
          {"scope", fref_fixed ? "frame_independent_fixed_magnitude" : "group"},
          {"reference_flux_source", ref_flux_source},
          {"flux_adu", sci.reference_flux_adu},
          {"flux_common", fref_fixed ? Json(ref_flux_common) : Json(nullptr)},
          {"flux_common_unit",
           fref_fixed ? Json("F_syn (Gaia XPSD absolute spectral integral)")
                      : Json(nullptr)},
          {"reference_mag", fref_fixed ? Json(ref_mag) : Json(nullptr)},
          {"reference_mag_system",
           fref_fixed ? Json("gaia_g_via_synthetic_xpsd") : Json(nullptr)},
          {"reference_zero_point_syn_mag",
           fref_fixed ? Json(ref_zero_point_syn) : Json(nullptr)},
          {"frame_zero_point_mag",
           std::isfinite(frame_zp_inst) ? Json(frame_zp_inst) : Json(nullptr)},
          {"frame_k_photo",
           std::isfinite(frame_kphoto) ? Json(frame_kphoto) : Json(nullptr)},
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
    f_frame[fi] = frame;
  });
  // 帧序归约（冻结顺序：下标升序）。诊断键与 error_kind 的写入时机与串行一致：
  // 串行时本帧诊断键在失败检查**之前**写入 ⇒ 归约也先写诊断键再判失败。
  Json frames = Json::array();
  for (size_t fi = 0; fi < nz_lights; ++fi) {
    for (auto it = f_diag[fi].begin(); it != f_diag[fi].end(); ++it)
      (*man)[it.key()] = it.value();
    if (!f_err[fi].ok()) {
      if (f_errkind[fi] == 1) (*man)["error_kind"] = "input";
      else if (f_errkind[fi] == 2) (*man)["error_kind"] = "noise_model_config";
      else if (f_errkind[fi] == 3) (*man)["error_kind"] = "noise_model_abi_or_input";
      return f_err[fi];
    }
    frames.push_back(f_frame[fi]);
  }
  const std::string out_path = out_dir + "/p1_snr.json";
  const bool fref_fixed_out = (ref_flux_source == "fixed_magnitude");
  // 逐帧 snr_caliber 的聚合（口径定义见逐帧写入处的注释）。
  size_t n_abs = 0, n_ub = 0;
  for (const Json& fj : frames) {
    const std::string c = fj.value("snr_caliber", std::string("unavailable"));
    if (c == "absolute_flux_type_snr") ++n_abs;
    else if (c == "upper_bound_no_gain") ++n_ub;
  }
  const char* caliber_summary = (n_abs > 0 && n_ub == 0) ? "absolute_flux_type_snr"
                              : (n_ub > 0 && n_abs == 0) ? "upper_bound_no_gain"
                                                         : "mixed_or_unavailable";
  Json snr_out = Json{{"schema", "DATA-P1-SNR"},
                      {"schema_version", "3"},
                      // WEIGHT-SCI-001: 块级参考通量 provenance。
                      // "group" ⇒ reference_flux_adu 为块级公共 F0（ADU）;
                      // "frame_independent_fixed_magnitude" ⇒ 头部写
                      // reference_flux_common（物理公共锚）, 逐帧 flux_adu 只作
                      // 配对性核对（C1: snr_f == flux_adu/sigma_f_adu）。
                      {"snr_reference_scope",
                       fref_fixed_out ? "frame_independent_fixed_magnitude" : "group"},
                      {"reference_flux_source", ref_flux_source},
                      // fixed_magnitude 生效时 group_ref_flux（两遍法块中位数）**未被
                      // 使用** ⇒ 写 null, 避免下游把陈旧的中位数当成生效值。
                      {"reference_flux_adu",
                       fref_fixed_out ? Json(nullptr) : Json(group_ref_flux)},
                      {"reference_flux_common",
                       fref_fixed_out ? Json(ref_flux_common) : Json(nullptr)},
                      {"reference_flux_common_unit",
                       fref_fixed_out
                           ? Json("F_syn (Gaia XPSD absolute spectral integral)")
                           : Json(nullptr)},
                      {"reference_mag", fref_fixed_out ? Json(ref_mag) : Json(nullptr)},
                      {"reference_mag_system",
                       fref_fixed_out ? Json("gaia_g_via_synthetic_xpsd") : Json(nullptr)},
                      {"reference_zero_point_syn_mag",
                       fref_fixed_out ? Json(ref_zero_point_syn) : Json(nullptr)},
                      // ── SNR 口径汇总：逐帧 snr_caliber 的聚合。全帧一致时等于该值，
                      // 否则 mixed_or_unavailable。消费者据此判断本文件的 SNR 能否
                      // 当作**绝对 SNR** 使用（无增益 ⇒ 只是上界）。
                      {"snr_caliber", caliber_summary},
                      {"n_frames_absolute_snr", n_abs},
                      {"n_frames_upper_bound_no_gain", n_ub},
                      {"frames", frames}};
  if (!p1_write_text(out_path, snr_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["n_frames"] = frames.size();
  (*man)["snr_schema"] = "DATA-P1-SNR/3";
  (*man)["snr_artifact"] = out_path;
  // WEIGHT-SCI-001 / FREF-BASELINE-001: 参考通量 provenance（块级）。
  (*man)["snr_reference_scope"] =
      fref_fixed_out ? "frame_independent_fixed_magnitude" : "group";
  (*man)["reference_flux_source"] = ref_flux_source;
  (*man)["reference_flux_adu"] =
      fref_fixed_out ? Json(nullptr) : Json(group_ref_flux);
  (*man)["reference_flux_common"] =
      fref_fixed_out ? Json(ref_flux_common) : Json(nullptr);
  (*man)["reference_mag"] = fref_fixed_out ? Json(ref_mag) : Json(nullptr);
  (*man)["artifacts"] = Json::array({out_path});
  return Result<void>::success();
}

// ── op: drizzle_stack（唯一真实入口 hp_drizzle_run_phase1_hips; nside 科学参数无缺省）──
//    2026-09-20 订正 [V5 分片 5 / R-2 同源]: 旧文 hp_drizzle_run 已作废, 实调用 :4745
Result<void> p1_op_drizzle(const Json& doc, Json* man) {
  auto p1_lights_rc = p1_require_lights(doc);
  if (p1_lights_rc.failed()) return p1_lights_rc;
  const std::string out_dir = doc.value("output_dir", std::string("."));
  // P0-21 §3.4: WCS 头来源改为**逐帧** <frame_dir>/p1_wcs.json（回退 config.wcs）,
  // 在下方逐帧循环内解析（每帧独立 WCS ⇒ 每帧独立 HiPS 产品）。
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
  // ── B2-A14: PHOTSCAL/PHOTAPPL 由真实测光 provenance 决定（禁硬编码 1）──
  // 上游 p1_phot.json (DATA-P1-PHOTPROV-001) 由 p1_op_photometry (measure_flux) 产出，
  // 声明是否已对像素施加测光缩放。本节点只透传该事实；未执行/未应用测光时
  // PHOTAPPL=0 + PHOTDEGRADE=1（在 drizzle 显式降级为 ADU，绝不伪造
  // RELATIVE_FLUX）。配置标量 photscal 不再是科学输入来源。（帧无关, 循环外一次）
  bool photometry_applied = false;
  double photscal = 1.0;
  bool have_phot_prov = false;
  // FIX-P1: 逐帧 k_photo (frame_key → 标量)。p1_phot.json.photscales 缺省时
  // 退回标量 photscal（向后兼容既有 B2-A14 夹具）。
  Json photscales = Json::object();
  // FAILSEM-01: 上游**逐帧判决**（p1_phot.json.frames[] → frame_key → 记录）。
  // 逐帧真相是本节点选择输入面的唯一依据; 组级 photometry_applied 只作摘要。
  Json frame_status = Json::object();
  bool have_frame_status = false;
  {
    const std::string prov_path = out_dir + "/p1_phot.json";
    if (aio_fs::exists(prov_path)) {
      std::string ptext;
      if (!aio_fs::read_all(prov_path, &ptext))
        return Result<void>::fail(Error(ErrorDomain::IO, "cannot open: " + prov_path));
      try {
        const Json pj = Json::parse(ptext);
        if (!pj.is_object() || pj.value("schema", std::string()) != "DATA-P1-PHOTPROV-001")
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "p1_phot.json schema mismatch (expect DATA-P1-PHOTPROV-001): " + prov_path));
        photometry_applied = pj.value("photometry_applied", false);
        photscal = pj.value("photscal", 1.0);
        if (!std::isfinite(photscal) || photscal <= 0.0)
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "p1_phot.json photscal must be finite and > 0"));
        if (pj.contains("photscales") && pj["photscales"].is_object()) {
          photscales = pj["photscales"];
          for (auto it = photscales.begin(); it != photscales.end(); ++it) {
            if (!it.value().is_number())
              return Result<void>::fail(Error(ErrorDomain::DATA,
                  "p1_phot.json photscales values must be numbers"));
            const double k = it.value().get<double>();
            if (!std::isfinite(k) || k <= 0.0)
              return Result<void>::fail(Error(ErrorDomain::DATA,
                  "p1_phot.json photscales values must be finite and > 0"));
          }
        }
        // FAILSEM-01: 逐帧判决表（可选键: 兼容未携带该表的既有产物, 此时
        // 逐帧判定退化为"photscales 有无该帧 + 组级 applied"的老口径）。
        if (pj.contains("frames") && pj["frames"].is_array()) {
          for (const auto& fe : pj["frames"]) {
            if (!fe.is_object()) continue;
            const std::string fk = fe.value("frame_key", std::string());
            if (fk.empty()) continue;
            frame_status[fk] = fe;
            have_frame_status = true;
          }
        }
        have_phot_prov = true;
      } catch (const std::exception& e) {
        return Result<void>::fail(Error(ErrorDomain::DATA,
            std::string("p1_phot.json parse failed: ") + e.what()));
      }
    }
  }
  // P23 一级: 生产末端直写标准 HiPS, 不再落中间容器
  // (hp_drizzle_run_phase1_hips -> write_hips_phase1, 与旧 writer 节点产物
  // 逐字节等价)。观测 passband 身份由 phase_config 提供 (与旧 writer 同源)。
  const std::string filter_passband = doc.value("filter_passband", std::string());
  if (filter_passband.find('\n') != std::string::npos ||
      filter_passband.find('\r') != std::string::npos)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "filter_passband must not contain newline"));
  // P17-NSIDE: 采样率 provenance 判定口径（nside 来源 + 决策依据 + 合规判定）。
  const std::string nside_mode_out = auto_nside ? "1x_to_2x_drizzle" : "explicit";
  const double HEALPIX_SCALE_PER_NSIDE_ARCSEC =
      std::sqrt(M_PI / 3.0) * (180.0 / M_PI) * 3600.0;  // ≈ 211034.6 "/nside
  // ── P0-21 §3.4: 逐帧 drizzle 循环 ─────────────────────────────────────
  // 每帧: 读定标帧 → 该帧 WCS（<frame_dir>/p1_wcs.json, 回退 config.wcs）→
  // 真实 drizzle → 直写 output_dir/<frame_key>/ 标准 HiPS + 该帧 p1_stack.json。
  // 输入 N 帧 ⇒ N 个独立 HiPS 产品; 任一帧不可读/不可 drizzle ⇒ fail-closed。
  // PERF-P1: 帧级并行（与 p2_parallel_for 同规范）。每帧只写**自己下标**的结果槽；
  // 跨帧零浮点归约；全部 man 写入（诊断键 / variance 审计 / 首帧 provenance）在
  // join 之后按下标升序复现串行时序 ⇒ 与串行逐字一致、与 worker 数无关。
  // 线程安全前提（本轮核查）: drizzle 的错误槽与计数为 thread_local
  // （g_hips_error / g_tl_n_quick）; 每帧自建 PipelineFrame 与自己的 HiPS 目录;
  // 逐像素 variance 生产者（snr_noise_model_v1[_fill]）的 floor 注册表已加锁
  // （PERF-P1 同批修复 noise_model.cpp）。
  const uint32_t drz_workers = p1_frame_workers(doc);
  const size_t drz_n = doc["input_lights"].size();
  std::vector<Result<void>> f_err(drz_n, Result<void>::success());
  std::vector<int> f_errkind(drz_n, 0);   // 0=无 1=input 2=disk_full
  std::vector<int> f_stage(drz_n, 0);     // 1=var 段前失败 2=var 段内失败 3=var 段后失败
  std::vector<bool> f_var_ran(drz_n, false);
  std::vector<Json> f_var_diag(drz_n);    // 逐帧调用侧诊断键（串行时立即写 man）
  std::vector<Json> f_var_entry(drz_n);   // 逐帧 variance 审计条目
  std::vector<Json> f_var_block(drz_n);   // 逐帧 variance 块词表（shape 依赖本帧画幅）
  std::vector<Json> f_man_first(drz_n);   // 首帧 provenance 键（仅 fi==0 非空）
  std::vector<std::string> f_artifact(drz_n);
  std::vector<Json> f_frame_entry(drz_n);
  std::vector<bool> f_skip(drz_n, false);          // FAILSEM-01: 上游已判 fail 的帧
  std::vector<std::string> f_skip_reason(drz_n);
  std::vector<std::string> f_skip_status(drz_n);
  p1_parallel_for(drz_workers, drz_n, p1_workers(doc), [&](uint64_t fi, uint32_t) {
    const std::string lp = doc["input_lights"][fi].get<std::string>();
    // [RELEASE-02 probe] 逐帧热点: drizzle
    ASTROCS_PROBE_SCOPE_CTX(_probe_drz_frame, "phase1", "drizzle.frame");
    ASTROCS_PROBE_TAG(_probe_drz_frame, "frame_key", p1_frame_key(lp).c_str());
    // ── FAILSEM-01: 逐帧输入面选择（不再用组级 applied 一刀切）────────────
    //   ① 该帧在 photscales 里有标度 ⇒ 必须消费 photoapplied_<base>
    //      （缺产物 ⇒ fail-closed, 不得静默退回未测光 ADU）;
    //   ② 上游 frames[] 判该帧 fail ⇒ **跳过该帧**（不产出产品; 这是**显式**
    //      失败, 不是静默跳过 —— 原因来自 photometry 节点的 error_report）;
    //   ③ 组级 applied=true 但既无标度也无逐帧判决（旧产物/不自洽）⇒ fail-closed;
    //   ④ 其余（测光通道缺席）⇒ 按既有显式 ADU 降级消费 calibrated 面。
    const std::string calibrated_path = p1_calibrated_path(doc, lp);
    std::string frame_path = calibrated_path;
    double frame_photscal = photscal;
    bool frame_applied = false;   // 本帧像素是否已施加测光归一化（PHOTAPPL 的逐帧真相）
    auto ksit = photscales.find(p1_frame_key(lp));
    if (ksit == photscales.end()) ksit = photscales.find(p1_frame_key(calibrated_path));
    if (ksit != photscales.end()) {
      frame_applied = true;
      const std::string applied_path = p1_photoapplied_path(doc, lp);
      if (!aio_fs::exists(applied_path)) {
        f_stage[fi] = 1;
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "p1_phot.json declares a scale for this frame but the applied frame is"
            " missing: " + applied_path));
        return;
      }
      frame_path = applied_path;
      frame_photscal = ksit.value().get<double>();
    } else if (have_frame_status) {
      auto vit = frame_status.find(p1_frame_key(lp));
      if (vit != frame_status.end() &&
          vit.value().value("status", std::string()) == "fail") {
        f_skip[fi] = true;
        f_skip_reason[fi] = vit.value().value("error", std::string());
        f_skip_status[fi] = vit.value().value("error_status", std::string());
        return;
      }
      if (photometry_applied) {
        f_stage[fi] = 1;
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "p1_phot.json declares photometry_applied=true but this frame has neither"
            " a scale nor a per-frame verdict (inconsistent provenance): "
            + p1_frame_key(lp)));
        return;
      }
    } else if (photometry_applied) {
      // 旧口径产物（无逐帧判决表 frames[]）: 保持既有语义 —— applied=true ⇒ 本帧
      // 必须消费 photoapplied_<base>（缺产物 fail-closed, 不得静默退回未测光 ADU），
      // 组级标量 photscal 作本帧代表值。有 frames[] 时上面的逐帧分支已接管。
      const std::string applied_path = p1_photoapplied_path(doc, lp);
      if (!aio_fs::exists(applied_path)) {
        f_stage[fi] = 1;
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "p1_phot.json declares photometry_applied=true but applied frame missing: "
            + applied_path));
        return;
      }
      frame_path = applied_path;
      frame_applied = true;
      frame_photscal = photscal;
    }
    P1Image im = p1_read_image(frame_path);
    if (!im.ok()) {
      f_stage[fi] = 1;
      f_errkind[fi] = 1;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO, "cannot read: " + frame_path));
      return;
    }
    // 该帧 WCS: 逐帧上游产物优先, 回退 config.wcs; 两者都无 → fail-closed。
    Json wj_storage = Json::object();
    {
      const std::string wcs_prod_path = p1_frame_dir(doc, lp) + "/p1_wcs.json";
      std::string wtext;
      Json wcs_prod;
      bool have_prod = false;
      if (aio_fs::read_all(wcs_prod_path, &wtext)) {
        try {
          wcs_prod = Json::parse(wtext);
          have_prod = true;
        } catch (...) { have_prod = false; }
      }
      if (have_prod && wcs_prod.is_object() && wcs_prod.contains("wcs") &&
          wcs_prod["wcs"].is_object()) {
        wj_storage = wcs_prod["wcs"];
      } else if (p1_has(doc, "wcs") && doc["wcs"].is_object()) {
        wj_storage = doc["wcs"];
      } else {
        f_stage[fi] = 1;
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "drizzle requires upstream <frame_dir>/p1_wcs.json or config 'wcs' (HP DRIZZLE"
            " header KV source; 禁 silent default; frame " + frame_path + ")"));
        return;
      }
    }
    const Json& wj = wj_storage;
    // B2-A17: 上游/配置 SIP 系数 (p1_wcs.json wcs.sip) 解析 —— 有则下发到 frame
    // header (CTYPE*-SIP + A/B/AP/BP_i_j), 供 hp_drizzle_api.cpp:586-645 读入
    // WcsSip 并逐叶像素施加畸变修正; 无则保持原线性路径 (基线等价)。
    bool sip_ok = true;
    std::string sip_err;
    const P1SipCoeffs sip = p1_parse_sip(wj, &sip_ok, &sip_err);
    if (!sip_ok) {
      f_stage[fi] = 1;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA, "drizzle wcs " + sip_err));
      return;
    }
    const std::string ctype1_kv =
        p1_has(wj, "ctype1") && wj["ctype1"].is_string()
            ? wj["ctype1"].get<std::string>()
            : (sip.present ? std::string("RA---TAN-SIP") : std::string("RA---TAN"));
    const std::string ctype2_kv =
        p1_has(wj, "ctype2") && wj["ctype2"].is_string()
            ? wj["ctype2"].get<std::string>()
            : (sip.present ? std::string("DEC--TAN-SIP") : std::string("DEC--TAN"));
    // PipelineFrame: data [H,W] + header KV（hp_drizzle_run 合同: dims[0]=H, dims[1]=W）。
    // M2a-H-1: data 块 dtype 必须与 drizzle.precision_mode 一致（FP64 请求 →
    // FLOAT64 块，走真 binary64 累加域）；否则元数据声称 FP64 而实际 binary32。
    PipelineFrame* frame = aio_pipeline_frame_create();
    if (!frame) {
      f_stage[fi] = 1;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::RESOURCE, "frame create failed"));
      return;
    }
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
        f_stage[fi] = 1;
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            "drizzle precision consistency: data block dtype != precision_mode"));
        return;
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
      fmt(frame_photscal, b9, sizeof(b9));
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
          // FAILSEM-01: PHOTAPPL/PHOTDEGRADE 是**逐帧**事实（该帧像素是否已施加
          // 测光归一化），不得再用组级摘要 —— 同一次运行里可以有已归一化的帧与
          // 未归一化的帧（后者是测光通道缺席的显式 ADU 降级）。
          if (std::strcmp(kv.k, "PHOTAPPL") == 0)
            return std::string(frame_applied ? "1" : "0");
          if (std::strcmp(kv.k, "PHOTDEGRADE") == 0)
            return std::string(frame_applied ? "0" : "1");
          return std::string(b8);  // CDELT2
        }();
        if (aio_frame_kv_set(frame, "header", kv.k, val.c_str()) != 0) {
          aio_pipeline_frame_destroy(frame);
          f_stage[fi] = 1;
          f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO,
              std::string("kv_set failed: ") + kv.k));
          return;
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
          f_stage[fi] = 1;
          f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO,
              std::string("drizzle frame SIP header: ") + sip_hdr_err));
          return;
        }
      }
    }
    if (rc != 0) {
      aio_pipeline_frame_destroy(frame);
      f_stage[fi] = 1;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO, "add data block failed"));
      return;
    }
    // ── 定案 2（负责人 §9.67「2那就接入啊」+ ASTROCS_DESIGN §8.2:526-527）──
    // 逐像素 variance **帧内命名块**（登记面 = DATA-P1-DRZ §11.1:295 逐字
    // 「variance 面（可选，帧内块）| float32，随 data 布局 | ADU²」）。
    // 生产者 = A（snr_noise_model_v1/_fill; 与 p1_op_noise 共用
    //   p1_noise_model_for_frame, 零策略副本）; 消费侧 = hp_drizzle_api.cpp:1018-1052
    //   （既有冻结实现, 本节点零改动）→ drizzle_engine.cpp:1555-1559
    //   sumVarNum += v·w² → astro_sphere_sink.cpp:306-327 请求 VARIANCE|IVAR
    //   产品位 → DATA-P1-HIPS §12.2 variance/ivar 子产品 → Phase2
    //   aio_hips_open(..., AIO_HIPS_RD_IVAR) 逐样本逆方差权重。
    // 帧身份/标度: 输入数组 = 与 "data" 块**同一数组、同一 dtype** ⇒ 无需 SNR-002
    //   尺度律（PHOTSCAL 已由上游测光节点乘入, 本节点不二次缩放, 见 hp_drizzle_api
    //   的 apply_photometry=false 口径）。
    // fail-closed（ASTROCS_DESIGN §8.2:547「退化或无正有限值的面（如全零方差面）
    // 不挂帧，挂帧会导致积分侧清空像素」）:
    //   ① A 退化（rc=1）时 variance_bg_global 保持 0（noise_model.cpp:414-418）
    //      ⇒ _fill 写出全零平面（:739-746）; 而 drizzle_engine.cpp:1919 对
    //      varianceValue<=0 是**整像素 continue**（不只是不累加方差）⇒ 挂全零
    //      平面会清空 signal/support ⇒ 退化/平面无正有限值 ⇒ **不挂块**;
    //   ② 无星掩膜输入（p1_sources.json 缺该帧条目）⇒ **不挂块**: 未掩膜整帧
    //      MAD 偏乐观 +2.6%..+21.6%（RULING3 E7）, 不得当科学方差发布;
    //   ③ rc=3/-9（A 拒绝输入 / ABI 失配）⇒ DATA fail-closed（不静默出权重面）。
    // 降级声明 = 节点 manifest 键 + stderr; 产品面事实由 writer 节点 p1_final.json
    //   的 uncertainty_available / n_variance_tiles 从**磁盘事实**给出（禁硬编码）。
    {
      f_var_ran[fi] = true;   // 本帧 var 段已进入（归约据此复现 diag 键的写入时机）
      // 上游 p1_sources.json（DATA-P1-SOURCES）: 逐星掩膜输入的唯一来源。
      // 缺失/不可解析 ⇒ 走 ② 显式降级（本节点不因它 fail-closed: 无方差面时
      // signal/support 产品面不受影响, 见 p1_op_writer 的成对校验）。
      Json nm_src_frames = Json::array();
      {
        std::string sftext;
        if (aio_fs::read_all(out_dir + "/p1_sources.json", &sftext)) {
          try {
            const Json sj = Json::parse(sftext);
            if (sj.is_object() && sj.contains("frames") && sj["frames"].is_array())
              nm_src_frames = sj["frames"];
          } catch (const std::exception&) { nm_src_frames = Json::array(); }
        }
      }
      // 帧身份归一（去节点前缀 + 去扩展名）: p1_sources.json 由 star-psf 节点按
      // cleaned_<base> 记账, 本节点积分 photoapplied_/calibrated_<base>（帧身份
      // 差异已在 run/RELEASE-02/parallel/02-variance-wiring.md §3.2 登记）。
      auto nm_stem = [](const std::string& base) -> std::string {
        std::string s = base;
        for (const char* p : {"cleaned_", "calibrated_", "photoapplied_"}) {
          const size_t np = std::strlen(p);
          if (s.rfind(p, 0) == 0) { s = s.substr(np); break; }
        }
        const size_t dot = s.find_last_of('.');
        return (dot == std::string::npos) ? s : s.substr(0, dot);
      };
      const Json* nm_src = nullptr;
      const std::string nm_want = nm_stem(p1_base_name(frame_path));
      for (const auto& fr : nm_src_frames) {
        if (!fr.is_object()) continue;
        if (nm_stem(fr.value("file", std::string())) == nm_want) {
          nm_src = &fr;
          break;
        }
      }
      // 与 "data" 块同一数组/同一 dtype（见上「帧身份/标度」）。
      const void* nm_data = nullptr;
      const bool nm_is_f64 = (precision_mode == 1) || (aio_get_dtype(im.p) == 1);
      if (precision_mode == 1) {
        nm_data = static_cast<const void*>(px64.data());
      } else if (aio_get_dtype(im.p) == 1) {
        nm_data = static_cast<const void*>(aio_get_pixel_data_f64(im.p));
      } else {
        nm_data = static_cast<const void*>(im.px());
      }
      std::string var_status;
      std::string var_reason;
      Json var_diag = Json::object();
      bool var_degenerate = false;
      if (nm_src == nullptr) {
        var_status = "skipped_no_star_mask_input";
        var_reason = "p1_sources.json has no entry for this frame; the blank-sky"
                     " model would be unmasked (optimistic-biased) => refuse to"
                     " publish a science variance plane (DATA-P1-DRZ 11.1:295"
                     " optional frame block)";
      } else if (nm_data == nullptr) {
        var_status = "skipped_no_pixel_array";
        var_reason = "pixel array pointer unavailable for the integrated frame";
      } else {
        const Json snr_cfg = (doc.contains("snr") && doc["snr"].is_object())
                                 ? doc["snr"] : Json::object();
        // 数组标度 α（SCI-NOISE §3/§7 + SNR-002）: frame_applied ⇒ 本帧消费
        // photoapplied_<base>，像素值 = α·ADU（α = frame_photscal ≈ 1e-17）；
        // 噪声模型的 variance_floor（冻结单位 ADU²）必须按 α² 换算到同一标度，
        // 否则 max(plane, floor) 把整帧钳成常数（见 p1_noise_model_for_frame）。
        // 未施加测光（显式 ADU 降级面）时 α=1，逐位不变。
        const double nm_data_scale = frame_applied ? frame_photscal : 1.0;
        P1NoiseFrameModel nmc = p1_noise_model_for_frame(
            nm_data, nm_is_f64, im.h(), im.w(), frame_path, snr_cfg, nm_src,
            nm_data_scale, /*disable_spatial_field=*/false);
        // 调用侧适配事实（与 p1_op_noise 同键名, 只增诊断不改数值）。
        // PERF-P1: 暂存到本帧槽；写入 man 的动作在 join 后按帧序复现（并行体不写 man）。
        f_var_diag[fi] = nmc.diag;
        if (nmc.rc == 3 || nmc.rc == -9) {
          snr_noise_model_v1_free(&nmc.model);
          aio_pipeline_frame_destroy(frame);
          f_stage[fi] = 2;
          f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
              "drizzle variance block: snr_noise_model_v1 rejected input"
              " (rc=" + std::to_string(nmc.rc) + ", frame " + frame_path + ")"));
          return;
        }
        var_degenerate = (nmc.rc == 1) || (nmc.model.degenerate != 0);
        // ── SCI-NOISE-001 §5d「可观测量（必须写入 provenance）」逐项落盘 ──────────
        // 规范：docs/science/NOISE_MODEL.md §5d:270-271 —— 控制点方差的动态范围、
        // 平面系数、凸包内预测 ≤ 0 的占比、被剔除 patch 数与 R 的分布，
        // **缺任一项即视为该帧的方差面不可审计**。
        // 数据来源 = ABI v2 的 NoiseWeightModelV1 尾部字段（生产模型自身的输出，
        // 不是调用侧重算；ABI 版本 SNR_NOISE_MODEL_ABI_VERSION=2）。逐项写入本帧
        // var_diag ⇒ 同时落节点 manifest（:6976）与 variance_product_frames 条目。
        var_diag = Json{{"sigma_bg_global", nmc.model.sigma_bg_global},
                        {"variance_bg_global", nmc.model.variance_bg_global},
                        {"n_control_points", nmc.model.n_control_points},
                        {"has_spatial_field", (nmc.model.has_spatial_field != 0)},
                        {"mask_degraded", nmc.model.mask_degraded},
                        {"mask_radius_p50", nmc.model.mask_radius_p50},
                        {"mask_frac", nmc.model.mask_frac},
                        {"saturation_filter", nmc.saturation_filter},
                        {"saturation_level", nmc.saturation_level},
                        {"saturation_source", nmc.saturation_source},
                        // §5d① 控制点有效性（自校准 R = σ_MAD/σ_white-equiv）
                        {"n_structure_rejected_patches",
                         nmc.model.n_structure_rejected_patches},
                        {"n_r_unavailable_patches", nmc.model.n_r_unavailable_patches},
                        {"r_min", nmc.model.r_min},
                        {"r_median", nmc.model.r_median},
                        {"r_max", nmc.model.r_max},
                        {"r_fence", nmc.model.r_fence},
                        // §5d② 拟合平面系数与可行域审计量
                        {"plane_a", nmc.model.plane_a},
                        {"plane_b", nmc.model.plane_b},
                        {"plane_c", nmc.model.plane_c},
                        {"ctrl_variance_range", nmc.model.ctrl_variance_range},
                        {"hull_min_pred", nmc.model.hull_min_pred},
                        {"hull_nonpositive_frac", nmc.model.hull_nonpositive_frac}};
        // ── §5d + §8 新增退化行: 凸包内负区 ≠ 0 ⇒ 该帧方差面不可审计, fail-closed ──
        // 判据正本 = astrocs/noise/variance_plane_policy.h:88 variance_plane_auditable():
        // **严格等于 0**（0 是「平面在控制点凸包内恒 ≥ 0」这条约束的定义值，不是
        // 「小到可以忽略」的阈值；任何 ≠ 0 的取值——正残差、负值、NaN——一律不可审计）。
        // 与 classify_variance_plane 正交：后者判「这张已 fill 的面能不能挂」，
        // 本判据判「这张面的**拟合**是否可信」，两者都过才发布（缺一即 fail-closed）。
        // 处置口径与本节其余 fail-closed 分支一致（skipped_no_star_mask_input /
        // skipped_degenerate_empty_support / skipped_fill_failed）：**不挂 variance 块**、
        // 显式登记状态与原因、stderr 出声；signal/support 产品面不受影响
        // （§5d:267-269「凸包内出现大面积预测 ≤ 0 属拟合缺陷，必须按 §8 登记并
        //  fail-closed，不得静默出片」；§7「空 support 不传播」）。
        // 故障注入面（ENGINEERING_SPEC §8「可执行负例」；与 ASTROCS_IVAR_FAULT 同款）:
        // ASTROCS_VARPLANE_FAULT=force_not_auditable 把生效审计量置为非零，
        // 使本 fail-closed 分支可被判红 —— 判据不是恒真门（生产默认不可达）。
        double hull_nonpositive_frac_eff = nmc.model.hull_nonpositive_frac;
        {
          const char* vpf = std::getenv("ASTROCS_VARPLANE_FAULT");
          if (vpf && std::string(vpf) == "force_not_auditable") {
            hull_nonpositive_frac_eff = 1.0;
            var_diag["variance_plane_fault_injected"] = true;
            var_diag["hull_nonpositive_frac"] = hull_nonpositive_frac_eff;
          }
        }
        const bool var_plane_auditable =
            astrocs::noise::variance_plane_auditable(hull_nonpositive_frac_eff);
        if (var_degenerate) {
          var_status = "skipped_degenerate_empty_support";
          var_reason = "NoiseWeightModelV1 degenerate (rc=1): variance_bg_global=0;"
                       " attaching an all-zero plane would make drizzle_engine"
                       " skip every pixel (varianceValue<=0 => continue) and erase"
                       " signal/support (ASTROCS_DESIGN §8.2:547)";
        } else if (!var_plane_auditable) {
          // §5d/§8 fail-closed（与上一条分支同口径：显式降级、不静默、不出片）。
          // 注意本分支**不**消费 classify_variance_plane 的 n_usable 判据：那张面
          // 可能仍有大量正像素（可用），但它的**拟合**已被证明不可信（凸包内出现
          // 预测 ≤ 0 的负区 ⇒ 平面不是 §5d 要求的那个估计量）⇒ 按 §5d:270-271
          // 「缺任一项即视为该帧的方差面不可审计」不得作为科学方差面发布。
          var_diag["variance_plane_audit_failed"] = true;
          var_status = "skipped_variance_plane_not_auditable";
          var_reason =
              "SCI-NOISE-001 §5d audit failed: hull_nonpositive_frac=" +
              std::to_string(nmc.model.hull_nonpositive_frac) +
              " != 0 (plane predicts <= 0 inside the control-point convex hull =>"
              " fitting defect, not edge extrapolation); the frame's variance plane"
              " is NOT auditable => refusing to attach it (NOISE_MODEL.md §5d/§8:"
              " must be registered and fail-closed, must not silently ship)."
              " Per-pixel encoding of any published plane is unchanged"
              " (§5/§9④ variance=0 & ivar=0, no clamp)";
        } else {
          std::vector<float> var_plane(static_cast<size_t>(n_px), 0.0f);
          // 平面可用性判据 = fill 成功 **且** 无**损坏**像素（variance<0 或非有限）。
          // variance==0 是 §4a 明文规定的**合法产品态**（有覆盖但方差不可用 ⇒ 零权重）:
          // 平面预测 ≤ 0 或该像素在产品 dtype 中不可表示时，生产者写 0∧0；投影侧
          // (drizzle_engine 的 varianceValue<=0 分支) 只跳过方差累加、不丢 signal/support
          // ⇒ **不构成拒绝理由**。整面丢弃会把可用像素的**空间方差结构**一并丢掉、
          // 退化成帧级常数场, 那是纯损失。判据正本 = astrocs/noise/variance_plane_policy.h
          // （与生产者同判据；逐位中性: 全正平面只读不写）。
          std::size_t n_var_unavailable = 0;
          std::size_t n_var_corrupt = 0;
          auto fill_plane = [&](P1NoiseFrameModel& m) -> bool {
            std::fill(var_plane.begin(), var_plane.end(), 0.0f);
            if (snr_noise_model_v1_fill(&m.model, im.h(), im.w(),
                                        var_plane.data(), nullptr) != 0) {
              return false;
            }
            const astrocs::noise::VariancePlaneVerdict vp =
                astrocs::noise::classify_variance_plane(var_plane.data(),
                                                        var_plane.size());
            n_var_unavailable = vp.n_unavailable;
            n_var_corrupt = vp.n_corrupt;
            return vp.accepted;
          };
          bool plane_ok = fill_plane(nmc);
          bool plane_invalid_fallback = false;
          if (!plane_ok) {
            // ── fail-closed：平面面本身损坏（variance<0 或非有限）──────────────
            // §4a「损坏 ⇒ rc=−6 硬失败（禁 clamp/禁静默跳过）」。此时退回
            // SCI-NOISE §4/§5 **已规定的**全局常量场（variance_bg_global = 合格 patch
            // variance 的稳健中位数），并显式登记降级（不静默）。数值上：平面合法帧逐位不变。
            // 注意「平面含非正**预测**」不再是本分支的触发条件——那是生产者的合法
            // 零方差态（见上），由 §4a 三态表按像素承载。
            P1NoiseFrameModel nmc_global = p1_noise_model_for_frame(
                nm_data, nm_is_f64, im.h(), im.w(), frame_path, snr_cfg, nm_src,
                nm_data_scale, /*disable_spatial_field=*/true);
            const bool global_ok =
                (nmc_global.rc == 0) && (nmc_global.model.degenerate == 0);
            if (global_ok && fill_plane(nmc_global)) {
              snr_noise_model_v1_free(&nmc.model);   // 与 v1 成对（DISP-NOISE-001/009）
              nmc = nmc_global;                      // 所有权转移（下方统一 _free）
              f_var_diag[fi] = nmc.diag;
              var_diag["spatial_plane_invalid_fallback_global"] = true;
              var_diag["spatial_plane_corrupt_pixels"] = (std::int64_t)n_var_corrupt;
              plane_ok = true;
              plane_invalid_fallback = true;
              var_status = "attached_global_field_plane_invalid";
              var_reason = "spatial plane is not a valid variance field (fill failed or"
                           " holds a non-positive value); fell back to the SCI-NOISE"
                           " §4/§5 global constant field (variance_bg_global) —"
                           " explicit degradation, signal/support preserved";
            } else {
              snr_noise_model_v1_free(&nmc_global.model);
            }
          }
          if (!plane_ok) {
            var_diag["spatial_plane_corrupt_pixels"] = (std::int64_t)n_var_corrupt;
            var_status = "skipped_fill_failed";
            var_reason = "snr_noise_model_v1_fill failed (non-zero rc) or the plane"
                         " holds a non-positive/non-finite value, and the global"
                         " constant-field fallback was unavailable; refusing to attach"
                         " a block that would drop pixels";
          } else {
            const int vrc = aio_frame_add_block(
                frame, "variance", AIO_BLOCK_FLOAT32, var_plane.data(),
                static_cast<int64_t>(n_px), dims, 2,
                "定案2 NoiseWeightModelV1 blank-sky variance (ADU^2,"
                " DATA-P1-DRZ 11.1:295 frame block)");
            if (vrc != 0) {
              snr_noise_model_v1_free(&nmc.model);
              aio_pipeline_frame_destroy(frame);
              f_stage[fi] = 2;
              f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO,
                  "drizzle variance block add failed rc=" + std::to_string(vrc)));
              return;
            }
            if (!plane_invalid_fallback) {   // 降级声明不得被默认状态覆盖
              var_status = "attached";
              var_reason = "ok";
            }
            // §4a 三态表的产品面事实: 有多少像素是「有覆盖但方差不可用」(0∧0)。
            // 与「平面是否被采用」正交, 故两条路径都要登记。
            var_diag["variance_unavailable_pixels"] =
                (std::int64_t)n_var_unavailable;
          }
        }
        snr_noise_model_v1_free(&nmc.model);   // 与 v1 成对（DISP-NOISE-001/009）
      }
      if (var_status.rfind("attached", 0) != 0) {
        std::fprintf(stderr,
                     "[drizzle_node][variance] %s: %s (frame %s) -- variance 块不挂,"
                     " signal/support 产品面不受影响（显式降级, 非静默）\n",
                     var_status.c_str(), var_reason.c_str(), frame_path.c_str());
      }
      // 逐帧审计（§30.1「diagnostics 标红计数」面）。PERF-P1: 条目与块形状暂存到
      // 本帧槽；累积（variance_product_frames / status / 块词表键）在 join 之后按
      // 帧序执行，复现串行的"读-改-写"时序。
      Json var_entry = Json{{"frame_id", p1_frame_key(lp)},
                            {"status", var_status},
                            {"reason", var_reason},
                            {"degenerate", var_degenerate},
                            {"model", "snr_noise_model_v1"}};
      for (auto it = var_diag.begin(); it != var_diag.end(); ++it)
        var_entry[it.key()] = it.value();
      f_var_entry[fi] = std::move(var_entry);
      f_var_block[fi] = Json{{"shape", Json::array({im.h(), im.w()})}};
    }
    // ── P17-NSIDE: 该帧最终 nside 决策 + 采样率 provenance/告警 ───────────
    // 自动: 必须成功, 否则 DATA fail-closed。显式: 原样使用, 但 best-effort
    // 复核采样率并把欠采样标记为 nside_conflict=undersampled + stderr 告警。
    int frame_nside = nside;
    HpAutoNsideResult auto_res;
    std::memset(&auto_res, 0, sizeof(auto_res));
    const int auto_rc = hp_drizzle_compute_auto_nside(frame, &auto_res);
    if (auto_nside) {
      if (auto_rc != 0 || auto_res.nside <= 0) {
        aio_pipeline_frame_destroy(frame);
        f_stage[fi] = 3;
        f_err[fi] = Result<void>::fail(Error(ErrorDomain::DATA,
            std::string("auto nside (1x_to_2x_drizzle) failed: ") +
            (auto_res.error_msg[0] ? auto_res.error_msg : "(no detail)")));
        return;
      }
      frame_nside = auto_res.nside;
    }
    // 采样率合规复核: oversample_factor = finest_input / hp_res ∈ [1,2) 即合规;
    // < 1 表示输出像素比输入粗 (欠采样)。
    std::string nside_conflict = "unknown";
    double finest_input_arcsec = 0.0, hp_res_arcsec = 0.0, oversample_factor = 0.0;
    if (auto_rc == 0 && auto_res.finest_arcsec > 0.0 && frame_nside > 0) {
      finest_input_arcsec = auto_res.finest_arcsec;
      hp_res_arcsec = HEALPIX_SCALE_PER_NSIDE_ARCSEC / static_cast<double>(frame_nside);
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
          frame_nside, hp_res_arcsec, finest_input_arcsec, under, auto_res.nside);
    }
    const std::string fdir = p1_frame_dir(doc, lp);
    (void)aio_fs::make_dirs(fdir);   // CLEAN-403: 目录创建经 aio
    HpDrizzleResult res;
    std::memset(&res, 0, sizeof(res));
    // §10 + LOG_AND_ERROR_CONTRACT §5: 本帧产品写入的**帧级归因窗口**。窗口是
    // 执行流局部(thread_local)的单调计数快照 ⇒ 并发帧各自的判定互不覆盖:
    // 另一帧的 aio_hips_product_begin 不会抹掉本帧的判定, 另一帧的失败收尾也不会
    // 抢走本帧的判定 (aio_disk_full.h 头注「归因粒度=帧级」)。
    const aio_disk::FailureEpoch dsk_epoch;
    rc = hp_drizzle_run_phase1_hips(frame, frame_nside, nested, pixfrac, fdir.c_str(),
                                    filter_passband.c_str(), &res, precision_mode);
    aio_pipeline_frame_destroy(frame);
    if (rc != 0) {
      // §10 原子产品 + §7.2 退出码表「10 = 磁盘写满/写盘失败」:
      // 磁盘满必须按**失败本身**归类上抛。aio 在失败瞬间(清理临时产物之前)
      // 已判定 (aio_disk_full.h 头注: 事后探针在清理后必然 fail-open);
      // 这里按**本帧自己的**归因窗口写进失败节点 manifest, 由 CLI 的
      // pipeline_exit_code_from_error 映射为 exit 10。
      if (dsk_epoch.failed()) f_errkind[fi] = 2;   // disk_full（归约时写入 man）
      f_stage[fi] = 3;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO,
          std::string("hp_drizzle_run_phase1_hips failed: ") +
          (res.error_msg[0] ? res.error_msg : "(no detail)") +
          " (frame " + frame_path + ")"));
      return;
    }
    const std::string out_path = fdir + "/p1_stack.json";
    // P17-NSIDE: 采样率 provenance (nside 来源 + 决策依据 + 合规判定) —— 每个
    // 产物与节点 manifest 都带, 使 1x-2x 合规性与任何显式降级完全可机检。
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
                          // BUNIT 口径 = canonical 面亮度串，**与 frame_applied 无关**：
                          // 测光归一化是线性乘性标度，只改零点、不改量纲类别，标度由
                          // photappl/photscal 逐帧承载（docs/contracts/DATA_SEMANTICS.md
                          // §31.1a:2808-2810 与 :2827-2830）。同一份像素的另一个声明面
                          // <fdir>/signal/properties 的 BUNIT 由 declare_hips_surface_
                          // brightness_units 写同一串；两串不一致由该函数判红（B1 守卫）。
                          {"bunit", "ADU/sr"},
                          {"photappl", frame_applied ? 1 : 0},
                          {"photscal", frame_photscal},
                          {"photometry_provenance", have_phot_prov ? "p1_phot.json" : "absent"},
                          {"artifact", "signal/ + support/ (标准 HiPS 树)"},
                          {"entry", "hp_drizzle_run_phase1_hips"}};
    if (!p1_write_text(out_path, stack_out.dump(2))) {
      f_stage[fi] = 3;
      f_err[fi] = Result<void>::fail(Error(ErrorDomain::IO,
          "artifact write failed: " + out_path));
      return;
    }
    f_artifact[fi] = out_path;
    f_frame_entry[fi] = Json{{"frame_id", p1_frame_key(lp)},
                             {"input_light", lp},
                             {"hips_root", fdir},
                             {"stack", out_path},
                             {"nside", res.nside},
                             {"n_healpix_pixels",
                              static_cast<int64_t>(res.n_healpix_pixels)},
                             {"n_source_pixels",
                              static_cast<int64_t>(res.n_source_pixels)}};
    if (fi == 0) {
      // 串行语义: 首个**成功**帧写这批 provenance 键；任何帧失败即返回 ⇒ 首个
      // 成功帧必为下标 0（归约时按 fi==0 复现）。
      Json& mf = f_man_first[fi];
      mf["n_healpix_pixels"] = static_cast<int64_t>(res.n_healpix_pixels);
      // DET-001 (D5): drizzle 墙钟耗时是**遥测**, 不属 DATA-P1-STACK 产品面。
      // 产品面必须逐字节可复现（manifest 登记 sha256 作验收门）; 遥测保留在节点
      // manifest / resource_summary.json，需要时仍可读取。
      mf["elapsed_sec"] = static_cast<double>(res.elapsed_sec);
      mf["stack_artifact"] = out_path;
      mf["precision_mode"] = precision_mode;
      mf["sip_present"] = sip.present;
      mf["sip_order"] = sip.present ? sip.order : 0;
      mf["nside"] = res.nside;
      mf["nside_source"] = nside_source;
      mf["nside_mode"] = nside_mode_out;
      mf["auto_nside"] = auto_nside_value;
      mf["finest_input_arcsec"] = finest_input_arcsec;
      mf["hp_res_arcsec"] = hp_res_arcsec;
      mf["oversample_factor"] = oversample_factor;
      mf["nside_conflict"] = nside_conflict;
      mf["nside_clamped"] = auto_res.clamped != 0;
      mf["photometry_applied"] = photometry_applied;   // 组级摘要（至少一帧）
      mf["frame_photometry_applied"] = frame_applied;   // 本帧事实（首帧）
      mf["photscal"] = frame_photscal;
      mf["photometry_provenance"] = have_phot_prov ? "p1_phot.json" : "absent";
    }
  });
  // 帧序归约（冻结顺序：下标升序；逐段复现串行时序：diag 键 → variance 审计与块
  // 词表 → drizzle 段 → 首帧 provenance）。
  Json artifacts = Json::array();
  Json frame_entries = Json::array();
  Json var_frames_acc = Json::array();
  Json skipped_frames = Json::array();
  for (size_t fi = 0; fi < drz_n; ++fi) {
    // FAILSEM-01: 上游测光节点已判 fail 的帧 ⇒ 本节点不产出该帧产品。
    // 这是**显式**跳过（原因来自上游 error_report，逐条落在 manifest），
    // 与"读不到文件就静默跳过"不同：后者仍是 fail-closed（见上方逐帧分支）。
    if (f_skip[fi]) {
      const std::string sk = p1_frame_key(doc["input_lights"][fi].get<std::string>());
      skipped_frames.push_back(Json{{"frame_key", sk},
                                    {"input_light",
                                     doc["input_lights"][fi].get<std::string>()},
                                    {"reason", f_skip_reason[fi]},
                                    {"upstream_error_status", f_skip_status[fi]},
                                    {"source", "astrocs.phase1.photometry"}});
      (*man)["skipped_frames"] = skipped_frames;
      continue;
    }
    if (f_var_ran[fi]) {
      for (auto it = f_var_diag[fi].begin(); it != f_var_diag[fi].end(); ++it)
        (*man)[it.key()] = it.value();
    }
    if (f_stage[fi] == 1 || f_stage[fi] == 2) {
      if (f_errkind[fi] == 1) (*man)["error_kind"] = "input";
      else if (f_errkind[fi] == 2) (*man)["error_kind"] = "disk_full";
      return f_err[fi];
    }
    if (f_var_ran[fi]) {
      var_frames_acc.push_back(f_var_entry[fi]);
      (*man)["variance_product_frames"] = var_frames_acc;
      bool var_all = true, var_any = false;
      for (const auto& e : var_frames_acc) {
        if (e.value("status", std::string()) == "attached") var_any = true;
        else var_all = false;
      }
      (*man)["variance_product_status"] =
          var_all ? "attached" : (var_any ? "mixed" : "skipped");
      // 块词表登记（ASTROCS_DESIGN §8.2:526: 名字/形状/类型/单位/可缺性）。
      (*man)["variance_block_name"] = "variance";
      (*man)["variance_block_type"] = "AIO_BLOCK_FLOAT32";
      (*man)["variance_block_shape"] = f_var_block[fi]["shape"];
      (*man)["variance_block_unit"] = "ADU^2";
      (*man)["variance_block_optional"] = true;
      (*man)["variance_frame_identity"] =
          "same array/dtype as the \"data\" block (integrated frame)";
      (*man)["variance_scale_law_applied"] = false;
    }
    if (f_stage[fi] == 3) {
      if (f_errkind[fi] == 1) (*man)["error_kind"] = "input";
      else if (f_errkind[fi] == 2) (*man)["error_kind"] = "disk_full";
      return f_err[fi];
    }
    if (!f_err[fi].ok()) return f_err[fi];   // 兜底（不应到达）
    artifacts.push_back(f_artifact[fi]);
    frame_entries.push_back(f_frame_entry[fi]);
    if (fi == 0) {
      for (auto it = f_man_first[fi].begin(); it != f_man_first[fi].end(); ++it)
        (*man)[it.key()] = it.value();
    }
  }
  (*man)["n_frames"] = static_cast<uint64_t>(artifacts.size());
  (*man)["n_input_frames"] = static_cast<uint64_t>(drz_n);
  (*man)["n_frames_skipped"] = static_cast<uint64_t>(skipped_frames.size());
  (*man)["stack_artifacts"] = artifacts;
  (*man)["frames"] = frame_entries;
  (*man)["artifacts"] = artifacts;
  return Result<void>::success();
}

// ── op: write_hips（Phase1 生产末端校验节点）。P23 一级后, 上游
//      drizzle 节点已经由 hp_drizzle_run_phase1_hips 直写标准 HiPS 树
//      (signal/ + support/ = NorderK/DirD/NpixN.fits, Moc.fits, metadata.fits,
//      properties); 本节点不再消费任何中间容器, 只做产物事实面校验并落
//      p1_final.json (逐节点 typed artifact 合同不变)。──
// 冻结单位表 canonical **产品 BUNIT 串**（docs/contracts/DATA_SEMANTICS.md §31.1）: signal_sb = ADU/sr, sb_variance_out = ADU^2/sr^2,
// sb_ivar_out = sr^2/ADU^2。产品面必须逐字写冻结串。
// 注: p3rsmp::Bunit::canonical() 现按 DATA_SEMANTICS §31.1a 的同一映射写冻结串
// （"ADU/sr" / "ADU^2/sr^2" / "sr^2/ADU^2"；符号幂次 = px_power/2），与上列常量同串；
// 产品面仍以本处常量逐字写出为准（不依赖该函数）。
constexpr const char* kP3BunitSurfaceBrightness = "ADU/sr";
constexpr const char* kP3BunitSbVariance = "ADU^2/sr^2";
constexpr const char* kP3BunitSbIvar = "sr^2/ADU^2";

// HiPS 产品单位/像素语义声明（properties + manifest.json 双写）。
// 定义在 p2_read_json 之后（依赖它）; 此处前置声明供 Phase1 writer 调用。
bool declare_hips_surface_brightness_units(const std::string& product_root,
                                           bool uncertainty_available,
                                           std::string* err);

Result<void> p1_op_writer(const Json& doc, Json* man) {
  const std::string out_dir = doc.value("output_dir", std::string("."));
  if (!p1_has(doc, "input_lights") || !doc["input_lights"].is_array() ||
      doc["input_lights"].empty()) {
    (*man)["error_kind"] = "input";
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "writer requires non-empty input_lights (P0-21 §3.4: N 帧 ⇒ N 个 HiPS 产品)"));
  }
  {
    auto uniq = p1_require_unique_frame_keys(doc);
    if (uniq.failed()) return uniq;
  }
  const std::string filter_passband = doc.value("filter_passband", std::string());
  // ── FAILSEM-01: 上游逐帧失败判决 → 运行级判红（ASTROCS_DESIGN §4.4）───────
  // 「任何一帧未被处理、跳过或失败都显式判红，产品数与输入帧数可核对」。
  // 上游测光节点判 fail 的帧没有产品 ⇒ 产品数 < 输入帧数。本节点把该事实**显式
  // 判红**（SCIENCE_PRECONDITION → exit 4，ASTROCS_DESIGN §7.2「4 = 科学验证或
  // 不变量失败」），并且**不发布** p1_products.json（P0-21: 不产出部分产品却报
  // 成功）。其他帧的 HiPS 产物已在磁盘上（本节点不回滚、不删除它们的证据）。
  Json phot_failed = Json::object();      // frame_key -> 上游逐帧失败记录
  Json failed_frame_list = Json::array();
  {
    const std::string pp = out_dir + "/p1_phot.json";
    std::string ptext;
    if (aio_fs::read_all(pp, &ptext)) {
      Json pj = Json::object();
      try { pj = Json::parse(ptext); } catch (...) { pj = Json::object(); }
      if (pj.is_object() && pj.contains("frames") && pj["frames"].is_array()) {
        for (const auto& fe : pj["frames"]) {
          if (!fe.is_object()) continue;
          if (fe.value("status", std::string()) != "fail") continue;
          const std::string fk = fe.value("frame_key", std::string());
          if (fk.empty()) continue;
          phot_failed[fk] = fe;
          failed_frame_list.push_back(fk);
        }
      }
    }
  }
  // ── P0-21 §3.4: 逐帧校验 + 逐帧 p1_final.json + 聚合 p1_products.json ──
  // 每帧产品目录 = output_dir/<frame_key>/（drizzle 直写 HiPS 的落点）。
  // 任一阵列缺失/不完整 ⇒ fail-closed（不产出部分产品却报成功）。
  Json artifacts = Json::array();
  Json product_frames = Json::array();
  Json hips_paths = Json::array();
  bool have_first = false;
  for (const auto& l : doc["input_lights"]) {
    const std::string lp = l.get<std::string>();
    const std::string fdir = p1_frame_dir(doc, lp);
    const std::string props = fdir + "/signal/properties";
    // FAILSEM-01: 该帧是否已被上游判 fail（逐帧判决是唯一真相, 不看组级摘要）。
    auto pit = phot_failed.find(p1_frame_key(lp));
    if (pit != phot_failed.end()) {
      const std::string up_status = pit.value().value("error_status", std::string());
      const std::string up_err = pit.value().value("error", std::string());
      (*man)["error_kind"] = "science_precondition";
      (*man)["error_status"] =
          up_status.empty() ? std::string("PHOT_FRAME_FAILED") : up_status;
      (*man)["failed_frames"] = failed_frame_list;
      (*man)["n_frames_failed"] =
          static_cast<uint64_t>(failed_frame_list.size());
      (*man)["n_frames_expected"] = static_cast<uint64_t>(doc["input_lights"].size());
      return Result<void>::fail(Error(ErrorDomain::SCIENCE_PRECONDITION,
          "frame photometric calibration failed upstream: frame=" +
          p1_frame_key(lp) + " upstream_status=" +
          (up_status.empty() ? std::string("(none)") : up_status) + " upstream_error=" +
          (up_err.empty() ? std::string("(none)") : up_err) +
          " (ASTROCS_DESIGN §4.4: 任何一帧失败都显式判红; 该帧不产出产品,"
          " 其余帧产物已写出但数据集清单不发布)"));
    }
    if (!aio_fs::exists(props)) {
      (*man)["error_kind"] = "input";
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "upstream HiPS product missing: " + props +
          " (writer validates drizzle direct HiPS output per frame; frame " + lp + ")"));
    }
    // 上游 provenance: 该帧 p1_stack.json (drizzle 节点落盘, 携带 nside 决策依据)
    const std::string stack_json = fdir + "/p1_stack.json";
    int nside = 0;
    {
      std::string stext;
      Json sj;
      try {
        if (aio_fs::read_all(stack_json, &stext)) sj = Json::parse(stext);
      } catch (...) { sj = Json::object(); }
      if (sj.is_object()) nside = sj.value("nside", 0);
    }
    // 叶片 Norder = log2(nside) - 9 (标准 512 叶 tile)。只统计叶片 tile, 排除
    // finalize 额外写出的上层 hierarchy NorderK (K < 叶片 order) 汇总 tile。
    int leaf_order = 0;
    for (int n = nside; n > 1; n /= 2) ++leaf_order;
    const int leaf_norder = (nside >= 512) ? leaf_order - 9 : -1;
    // 统计标准 HiPS 事实面 (逐子产品叶片 tile 数)。product 集 = 磁盘事实,
    // 不再硬编码 [signal, support]: 命中 DATA-P1-HIPS §12.2 variance/ivar 子产品
    // 时如实上报 (manifest/合同登记面与产品一致)。
    int64_t n_tiles_written = 0, n_support_tiles = 0;
    int64_t n_variance_tiles = 0, n_ivar_tiles = 0;
    for (const std::string prod :
         {std::string("signal"), std::string("support"),
          std::string("variance"), std::string("ivar")}) {
      const std::string root = fdir + "/" + prod;
      int64_t c = 0;
      // CLEAN-403: 递归枚举经 aio (aio_fs::walk_tree → for_each_child),
      // 计数口径与 std::filesystem 版本逐条同义 (常规文件/排除清单/.fits/
      // 叶 Norder 目录)。
      const std::string leaf_norder_dir = "Norder" + std::to_string(leaf_norder);
      aio_fs::walk_tree(
          root,
          [&](const std::string& p, int kind) -> int {
            if (kind != 0) return 0;
            const std::string fn = aio_fs::base_name(p);
            if (fn == "Moc.fits" || fn == "metadata.fits" || fn == "properties")
              return 0;
            if (!aio_fs::ends_with(fn, ".fits")) return 0;
            if (leaf_norder >= 0) {
              const std::string nord =
                  aio_fs::base_name(aio_fs::dir_name(aio_fs::dir_name(p)));
              if (nord != leaf_norder_dir) return 0;
            }
            ++c;
            return 0;
          },
          0);
      if (prod == "signal") n_tiles_written = c;
      else if (prod == "support") n_support_tiles = c;
      else if (prod == "variance") n_variance_tiles = c;
      else n_ivar_tiles = c;
    }
    // signal/support 为无条件产品面 (§12.2 恒写); 缺失即上游未接线 → fail-closed。
    if (n_tiles_written <= 0 || n_support_tiles != n_tiles_written) {
      (*man)["error_kind"] = "input";
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "phase1 HiPS product face incomplete (frame " + lp + "): signal tiles=" +
          std::to_string(n_tiles_written) + " support tiles=" +
          std::to_string(n_support_tiles) +
          " (DATA-P1-HIPS §12.2: signal/support 恒写且叶 tile 数一致)"));
    }
    // variance/ivar 为成对产品 (§4a 互推; §12.2 同通道落盘): 任一单独存在即产品
    // 损坏 → fail-closed (禁静默丢弃/禁单边冒充)。
    if ((n_variance_tiles > 0) != (n_ivar_tiles > 0) ||
        (n_variance_tiles > 0 && n_variance_tiles != n_tiles_written)) {
      (*man)["error_kind"] = "input";
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "phase1 variance/ivar product pair inconsistent (frame " + lp +
          "): variance tiles=" + std::to_string(n_variance_tiles) + " ivar tiles=" +
          std::to_string(n_ivar_tiles) + " signal tiles=" +
          std::to_string(n_tiles_written) +
          " (DATA-P1-HIPS §12.1/§12.2 + §4a: variance/ivar 同通道成对落盘)"));
    }
    // ── CHAIN-WIRE-ADAPT-01 / W2: 方差面「真值」普查 = **实读落盘的 variance
    //    叶 tile**，不是数文件个数 ─────────────────────────────────────────────
    // 缺陷（证据 run/SCI-VAR-ZERO-01/REPORT.md §4(2)）: 上面两个 n_*_tiles 是
    // **子产品 tile 文件数**；而每个 signal tile 必写一个 variance tile
    // （aio_hips_writer.cpp:1515-1574 的三态表: 有覆盖 ∧ vnum==0 ⇒ 写 0∧0，
    // 是 §4a 明文规定的**合法产品态**）⇒「n_variance_tiles == n_tiles」是
    // 同义反复，对「方差面里到底有没有不确定度」零信息。实测反例:
    // M2_T2_20251224_033141 帧 32/115 个 tile 的 variance 面**整块全零**
    // （SCI-VAR-ZERO-01 §0/§3），而 p1_final.json 仍报
    // uncertainty_available=true（恒真门，与磁盘事实相反）。
    // 规范依据: NOISE_MODEL §7「对外产品面即 variance=0 ∧ ivar=0（禁 NaN）」
    //   + §5d「可观测量必须写入 provenance；缺任一项即视为该帧的方差面不可审计」。
    // 口径（与 DATA_SEMANTICS §4a 三态表逐项对齐；读的是**落盘后的产品值**）:
    //   usable      : isfinite(v) ∧ v > 0   —— 合法「方差可用」
    //   unavailable : v == 0                —— 合法「有覆盖但方差不可用」
    //   uncovered   : isnan(v)              —— 无覆盖（与 signal NaN 同态）
    //   corrupt     : v < 0 ∨ isinf(v)      —— 产品损坏（§12.4 本应已在写侧
    //                 fail-closed；此处见到即判红，禁静默）
    // 成本: 逐 tile 顺序读，单个 512² float32 缓冲（1 MiB）复用，不整帧驻留。
    int64_t n_var_tiles_with_data = 0;         // 至少 1 个可用方差像素的 tile 数
    int64_t n_var_tiles_all_unavailable = 0;   // 有覆盖但**整块**方差不可用（全零）
    int64_t n_var_tiles_uncovered = 0;         // 无覆盖（全 NaN）
    int64_t var_usable_px = 0, var_unavailable_px = 0, var_uncovered_px = 0,
            var_corrupt_px = 0;
    bool variance_census_available = false;
    std::string variance_census_error;
    if (n_variance_tiles > 0) {
      auto hips_rd_err = []() -> std::string {
        const char* e = aio_hips_reader_last_error();
        return e ? std::string(e) : std::string("(no detail)");
      };
      AioHipsDataset* vds = aio_hips_open(fdir.c_str(), AIO_HIPS_RD_VARIANCE);
      if (vds == nullptr) {
        variance_census_error = "aio_hips_open(AIO_HIPS_RD_VARIANCE) failed: " + hips_rd_err();
      } else {
        const int nt = aio_hips_tile_count(vds);
        std::vector<float> vbuf(512u * 512u);
        bool census_ok = (nt > 0);
        if (!census_ok) variance_census_error = "variance dataset reports no leaf tiles";
        for (int t = 0; census_ok && t < nt; ++t) {
          uint64_t ipix = 0;
          if (aio_hips_tile_ipix(vds, t, &ipix) != 0) {
            census_ok = false;
            variance_census_error = "aio_hips_tile_ipix failed at index " + std::to_string(t);
            break;
          }
          if (aio_hips_read_tile_f32(vds, ipix, vbuf.data()) != 0) {
            census_ok = false;
            variance_census_error = "aio_hips_read_tile_f32 failed at ipix " +
                                    std::to_string(ipix) + ": " + hips_rd_err();
            break;
          }
          int64_t usable = 0, unavail = 0, uncov = 0, corrupt = 0;
          for (float v : vbuf) {
            if (std::isnan(v)) { ++uncov; continue; }
            if (v == 0.0f) { ++unavail; continue; }
            if (v < 0.0f || !std::isfinite(v)) { ++corrupt; continue; }
            ++usable;
          }
          var_usable_px += usable;
          var_unavailable_px += unavail;
          var_uncovered_px += uncov;
          var_corrupt_px += corrupt;
          if (usable > 0) ++n_var_tiles_with_data;
          else if (unavail > 0) ++n_var_tiles_all_unavailable;
          else ++n_var_tiles_uncovered;
        }
        aio_hips_close(vds);
        variance_census_available = census_ok;
      }
    }
    if (variance_census_available && var_corrupt_px > 0) {
      (*man)["error_kind"] = "output";
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "phase1 variance product corrupt (frame " + lp + "): " +
          std::to_string(var_corrupt_px) + " pixel(s) hold variance<0 or non-finite"
          " (DATA_SEMANTICS §12.4/§4a: 损坏必须 fail-closed, 禁 clamp/禁静默跳过)"));
    }
    if (!variance_census_error.empty()) {
      // 读不到就不许声称可用（fail-closed on the claim，不中止本帧其余产品面）。
      std::fprintf(stderr,
                   "[writer][variance] 方差面真值普查不可用 (frame %s): %s --"
                   " uncertainty_available 按不可用登记（禁按文件数冒充）\n",
                   lp.c_str(), variance_census_error.c_str());
    }
    // 产品面「存在」判据（成对子产品在位）与「信息可用」判据必须分开:
    //   ① products / units 声明面按**产品集在位**（磁盘上确有 variance|ivar 子产品，
    //      单位必须逐字声明，否则 Phase3 输入语义守卫按「单位不可判」拒绝）；
    //   ② uncertainty_available 是**科学声明**，其真值 = 落盘方差面里确有可用像素
    //      （§7 三态表 + §5d 可审计性）。二者由同一份磁盘事实导出，不再恒真。
    const bool variance_products_present = (n_variance_tiles > 0 && n_ivar_tiles > 0);
    const bool has_uncertainty =
        variance_products_present && variance_census_available && (var_usable_px > 0);
    // 逐帧 HiPS 产品单位/像素语义声明（Phase1 Drizzle/HiPS signal 亦为
    // 面亮度 signal_sb = ADU/sr; 冻结单位表 §1 + FZ-BUNIT-SEMANTICS）。未声明
    // ⇒ Phase3 输入语义守卫按"单位不可判"拒绝（Phase1→Phase3 直连流不可用）。
    {
      std::string uerr;
      if (!declare_hips_surface_brightness_units(fdir, variance_products_present, &uerr)) {
        (*man)["error_kind"] = "output";
        return Result<void>::fail(Error(ErrorDomain::IO,
            "phase1 HiPS 单位/像素语义声明失败 (frame " + lp + "): " + uerr));
      }
    }
    const std::string final_path = fdir + "/p1_final.json";
    Json products = Json::array({"signal", "support"});
    if (variance_products_present) { products.push_back("variance"); products.push_back("ivar"); }
    Json final_out = Json{{"schema", "DATA-P1-HIPS"},
                          {"entry", "hp_drizzle_run_phase1_hips"},
                          {"hips_root", fdir},
                          {"nside", nside},
                          {"tile_nside", 512},
                          {"n_tiles", n_tiles_written},
                          {"n_tiles_written", n_tiles_written},
                          {"n_support_tiles", n_support_tiles},
                          {"n_variance_tiles", n_variance_tiles},
                          {"n_ivar_tiles", n_ivar_tiles},
                          {"uncertainty_available", has_uncertainty},
                          // ── 方差面真值普查（实读落盘 variance 叶 tile；W2）──────
                          // 「方差面存在」与「方差面里有信息」是两件事，分开登记。
                          {"n_variance_tiles_with_data", n_var_tiles_with_data},
                          {"n_variance_tiles_all_unavailable",
                           n_var_tiles_all_unavailable},
                          {"n_variance_tiles_uncovered", n_var_tiles_uncovered},
                          {"variance_usable_pixels", var_usable_px},
                          {"variance_unavailable_pixels", var_unavailable_px},
                          {"variance_uncovered_pixels", var_uncovered_px},
                          {"variance_corrupt_pixels", var_corrupt_px},
                          {"variance_census_available", variance_census_available},
                          {"variance_census_source",
                           "on_disk_variance_leaf_tiles(aio_hips_reader)"},
                          {"products", products},
                          {"filter_passband", filter_passband},
                          {"covered_area_model", "support_ratio_x_A_cell"},
                          // 单位/像素语义随 DATA-P1-HIPS manifest 落盘
                          {"bunit", kP3BunitSurfaceBrightness},
                          {"units", Json{
                              {"bunit", kP3BunitSurfaceBrightness},
                              {"pixel_semantics", "surface_brightness"},
                              {"pixel_area_power", -2},
                              {"variance_bunit", variance_products_present
                                   ? std::string(kP3BunitSbVariance) : std::string()},
                              {"ivar_bunit", variance_products_present
                                   ? std::string(kP3BunitSbIvar) : std::string()}}},
                          {"properties", props},
                          // P0-21: 帧身份随逐帧产品落盘（可枚举/可核对）。
                          {"frame_id", p1_frame_key(lp)},
                          {"input_light", lp}};
    if (!p1_write_text(final_path, final_out.dump(2)))
      return Result<void>::fail(Error(ErrorDomain::IO,
          "artifact write failed: " + final_path));
    artifacts.push_back(final_path);
    hips_paths.push_back(fdir);
    product_frames.push_back(Json{{"frame_id", p1_frame_key(lp)},
                                  {"input_light", lp},
                                  {"hips_path", fdir},
                                  {"properties", props},
                                  {"final", final_path},
                                  {"nside", nside},
                                  {"n_tiles", n_tiles_written},
                                  {"n_support_tiles", n_support_tiles},
                                  {"n_variance_tiles", n_variance_tiles},
                                  {"n_ivar_tiles", n_ivar_tiles},
                                  {"uncertainty_available", has_uncertainty},
                                  {"n_variance_tiles_with_data", n_var_tiles_with_data},
                                  {"n_variance_tiles_all_unavailable",
                                   n_var_tiles_all_unavailable},
                                  {"n_variance_tiles_uncovered", n_var_tiles_uncovered},
                                  {"variance_usable_pixels", var_usable_px},
                                  {"variance_unavailable_pixels", var_unavailable_px},
                                  {"variance_uncovered_pixels", var_uncovered_px},
                                  {"variance_corrupt_pixels", var_corrupt_px},
                                  {"variance_census_available", variance_census_available}});
    if (!have_first) {
      have_first = true;
      (*man)["n_tiles"] = n_tiles_written;
      (*man)["n_support_tiles"] = n_support_tiles;
      (*man)["n_variance_tiles"] = n_variance_tiles;
      (*man)["n_ivar_tiles"] = n_ivar_tiles;
      (*man)["uncertainty_available"] = has_uncertainty;
      // W2: 方差面真值（实读落盘 tile）；uncertainty_available 即由它导出。
      (*man)["n_variance_tiles_with_data"] = n_var_tiles_with_data;
      (*man)["n_variance_tiles_all_unavailable"] = n_var_tiles_all_unavailable;
      (*man)["n_variance_tiles_uncovered"] = n_var_tiles_uncovered;
      (*man)["variance_usable_pixels"] = var_usable_px;
      (*man)["variance_unavailable_pixels"] = var_unavailable_px;
      (*man)["variance_uncovered_pixels"] = var_uncovered_px;
      (*man)["variance_corrupt_pixels"] = var_corrupt_px;
      (*man)["variance_census_available"] = variance_census_available;
      (*man)["variance_census_source"] = "on_disk_variance_leaf_tiles(aio_hips_reader)";
      (*man)["products"] = products;
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
      (*man)["hips_root"] = fdir;
      (*man)["final_artifact"] = final_path;
    }
  }
  // ── 聚合结构化 JSON（ASTROCS_DESIGN §3.4「外加 1 个列出全部产品路径的结构化
  //    JSON」+「可串行衔接」）: hips_paths 可直接作为 mosaic 的 hips_paths 消费。
  const std::size_t n_inputs = doc["input_lights"].size();
  if (hips_paths.size() != n_inputs) {
    (*man)["error_kind"] = "input";
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "phase1 product count mismatch: products=" + std::to_string(hips_paths.size()) +
        " inputs=" + std::to_string(n_inputs) +
        " (P0-21 §3.4: 输入 N 帧 ⇒ 恰好 N 个 HiPS 产品; 禁静默丢弃)"));
  }
  const std::string products_path = out_dir + "/p1_products.json";
  Json products_out = Json{{"schema", "DATA-P1-PRODUCTS"},
                           {"entry", "hp_drizzle_run_phase1_hips"},
                           {"output_dir", out_dir},
                           {"n_frames", static_cast<uint64_t>(n_inputs)},
                           {"n_products", static_cast<uint64_t>(hips_paths.size())},
                           {"count_consistent", true},
                           {"hips_paths", hips_paths},
                           {"frames", product_frames},
                           {"filter_passband", filter_passband}};
  if (!p1_write_text(products_path, products_out.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO,
        "artifact write failed: " + products_path));
  artifacts.push_back(products_path);
  (*man)["n_frames"] = static_cast<uint64_t>(n_inputs);
  (*man)["n_products"] = static_cast<uint64_t>(hips_paths.size());
  (*man)["hips_paths"] = hips_paths;
  (*man)["products_artifact"] = products_path;
  (*man)["artifacts"] = artifacts;
  // B2-A10（宪章 §4.3）: 单位/坐标系/观测 passband 随节点 manifest 上报。
  (*man)["bunit"] = kP3BunitSurfaceBrightness;
  (*man)["pixel_semantics"] = "surface_brightness";
  (*man)["pixel_area_power"] = -2;
  // P0-19 同步: HiPS properties 的 hips_frame 按 IVOA REC-HIPS-1.0 §4.4.1
  // 写标准值 "equatorial"(ICRS); 节点 manifest 的 coordinate_frame 与之
  // 同源, 避免 CLI 汇总 coordinate_frames 出现 icrs/equatorial 两种写法。
  (*man)["coordinate_frame"] = "equatorial";
  (*man)["filter_passband"] = filter_passband;
  return Result<void>::success();
}

// ══ P2-001: Phase2 真实节点 operation 实现 ═══════════════════════════════════
// 每节点唯一真实 operation 委托（见文件头映射表）；子节点禁止调用完整
// p2_session_run。manifest 携带 operation/entry 标记（与
// lib/infrastructure/pipeline/module_ports.registry.json 冻结绑定表一致），typed
// artifact 落盘 config.output_dir。节点间数据流 = output_dir 文件约定
// （上游 artifact 缺失 → DATA fail-closed，指向上游节点未执行）。

// ── P2 共用工具 ──────────────────────────────────────────────────────────────
bool p2_write_text(const std::string& path, const std::string& text) {
  // CLEAN-403: 落盘经 aio 原子写原语 (临时文件 → fflush → fsync → 原子 rename)。
  return aio_fs::write_atomic(path, text);
}

// §9 原子文本落盘（临时文件 → fflush → fsync → 原子 rename; AIO 唯一原语）:
// 失败/取消不得留下可被误认为正式产品的半成品。用于 P3 typed artifact。
bool p2_write_text_atomic(const std::string& path, const std::string& text) {
  std::string aerr;
  const int rc = aio_atomic::write_file_atomic_stream(
      path,
      [&text](FILE* f) {
        return text.empty() ||
               std::fwrite(text.data(), 1, text.size(), f) == text.size();
      },
      &aerr);
  if (rc != 0)
    std::fprintf(stderr, "[atomic] text write failed: %s (%s)\n", path.c_str(),
                 aerr.c_str());
  return rc == 0;
}


bool p2_read_json(const std::string& path, Json* out) {
  // CLEAN-403: 整文件读取经 aio (aio_file::read_all)。
  std::string text;
  if (!aio_fs::read_all(path, &text)) return false;
  try {
    *out = Json::parse(text);
  } catch (const Json::parse_error&) {
    return false;
  }
  return true;
}

// 裸数值 bin 写/读（typed artifact 数据面; JSON manifest 记 offset/count）
template <typename T>
bool p2_write_bin(const std::string& path, const std::vector<T>& v) {
  // CLEAN-403: 顺序二进制写经 aio (write_open_trunc + append_write + append_close)。
  aio_atomic::AppendSink* f = aio_atomic::write_open_trunc(path, nullptr);
  if (f == nullptr) return false;
  if (!v.empty() &&
      aio_atomic::append_write(f, v.data(), v.size() * sizeof(T)) != 0) {
    (void)aio_atomic::append_close(f);
    return false;
  }
  return aio_atomic::append_close(f) == 0;
}

template <typename T>
bool p2_read_bin_range(const std::string& path, uint64_t offset_elems,
                       uint64_t count_elems, std::vector<T>* out) {
  // CLEAN-403: 区间读取经 aio (aio_file::read_range, 分块定位不整载)。
  out->assign(static_cast<size_t>(count_elems), T{});
  if (count_elems == 0) return true;
  const std::size_t bytes = static_cast<std::size_t>(count_elems * sizeof(T));
  std::string buf;
  if (!aio_file::read_range(path.c_str(), offset_elems * sizeof(T), bytes, &buf))
    return false;
  std::memcpy(out->data(), buf.data(), bytes);
  return true;
}

// tile 内 leaf 数（512×512 标准 HiPS tile = 2^18 leaf）
constexpr uint64_t kP2TileLeafSpan = 512ULL * 512ULL;
constexpr uint32_t kP2TileShift = 9;  // leaf order − tile order 差（512=2^9）

// ── PERF-P2 (RELEASE-02): 确定性 tile 级并行执行器 ────────────────────────────
// 线程数 = Runtime lease 注入的 __workers（AGENTS §5: 禁硬编码线程数; 1 = 串行
// reference）。任务以原子计数动态认领（等价 dynamic schedule, 抗负载不均）;
// 每个任务只写**自己下标**的结果槽 / 输出 offset，跨任务无任何浮点归约 ⇒ 结果
// 与串行逐位一致、与线程数/调度顺序无关。
// 异常语义: worker 内异常（如 bad_alloc）经 std::exception_ptr 回传并在 join 后
// 于调用线程重抛, 与串行路径的异常行为一致（外层 execute 的 catch 语义不变）。
template <typename Fn>
static void p2_parallel_for(uint32_t workers, uint64_t n, Fn&& body) {
  if (workers <= 1 || n <= 1) {
    for (uint64_t i = 0; i < n; ++i) body(i, 0u);
    return;
  }
  std::atomic<uint64_t> next{0};
  std::vector<std::exception_ptr> eptr(workers, nullptr);
  std::vector<std::thread> pool;
  pool.reserve(workers);
  for (uint32_t w = 0; w < workers; ++w) {
    pool.emplace_back([&, w]() {
      try {
        for (;;) {
          const uint64_t i = next.fetch_add(1);
          if (i >= n) break;
          body(i, w);
        }
      } catch (...) {
        eptr[w] = std::current_exception();
      }
    });
  }
  for (auto& th : pool) th.join();
  for (const auto& e : eptr)
    if (e) std::rethrow_exception(e);
}

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

// ── CONFORM-FIX-B-014: model 段 sampler 配置解析（唯一事实源 CONFIG_SCHEMA.md
//    「model:」段；键集 = P2SamplerConfig 全部 17 字段，逐字段显式赋值）。
//    旧实现只消费 __workers ⇒ node chain 采样参数钉死编译期默认值。
//    域校验与 stage2_common.cpp:40-126 同口径（fail-closed，禁静默夹取）。
//    patch_radius_leaf 为主名（CONFIG_SCHEMA），patch_radius_pixels 为 stage2
//    工具名别名（"auto" → 2，与工具同语义）。──
bool p2_sample_cfg_from_doc(const Json& doc, P2SamplerConfig* sc,
                            std::string* err) {
  if (!doc.contains("model")) return true;
  if (!doc["model"].is_object()) {
    *err = "model must be object";
    return false;
  }
  const Json& m = doc["model"];
  auto num = [&](const char* k, double* out) -> bool {
    if (!m.contains(k)) return true;
    if (!m[k].is_number()) { *err = std::string("model.") + k + " must be number"; return false; }
    *out = m[k].get<double>();
    return true;
  };
  auto i32 = [&](const char* k, int* out) -> bool {
    if (!m.contains(k)) return true;
    if (!m[k].is_number_integer()) { *err = std::string("model.") + k + " must be integer"; return false; }
    *out = m[k].get<int>();
    return true;
  };
  if (!i32("control_grid_per_tile", &sc->control_grid_per_tile)) return false;
  if (sc->control_grid_per_tile < 1 || sc->control_grid_per_tile > 64) {
    *err = "model.control_grid_per_tile 必须在 1..64";
    return false;
  }
  if (m.contains("patch_radius_leaf") || m.contains("patch_radius_pixels")) {
    const char* pk = m.contains("patch_radius_leaf") ? "patch_radius_leaf"
                                                     : "patch_radius_pixels";
    const Json& pv = m[pk];
    if (pv.is_string()) {
      if (pv.get<std::string>() != "auto") {
        *err = std::string("model.") + pk + " 只支持 'auto' 或整数";
        return false;
      }
      sc->patch_radius_leaf = 2;
    } else if (pv.is_number_integer()) {
      sc->patch_radius_leaf = pv.get<int>();
      if (sc->patch_radius_leaf < 0 || sc->patch_radius_leaf > 64) {
        *err = std::string("model.") + pk + " 必须在 0..64";
        return false;
      }
    } else {
      *err = std::string("model.") + pk + " 类型错误（'auto' 或整数）";
      return false;
    }
  }
  if (!i32("min_samples", &sc->min_samples)) return false;
  if (sc->min_samples < 1) { *err = "model.min_samples 必须 >= 1"; return false; }
  if (!num("snr_search_radius_deg", &sc->snr_search_radius_deg)) return false;
  if (sc->snr_search_radius_deg <= 0.0) {
    *err = "model.snr_search_radius_deg 必须 > 0"; return false;
  }
  if (!i32("background_patch_radius", &sc->background_patch_radius)) return false;
  if (sc->background_patch_radius < 3) {
    *err = "model.background_patch_radius 必须 >= 3"; return false;
  }
  if (!num("background_clip_sigma", &sc->background_clip_sigma)) return false;
  if (sc->background_clip_sigma <= 0.0) {
    *err = "model.background_clip_sigma 必须 > 0"; return false;
  }
  if (!i32("background_clip_iters", &sc->background_clip_iters)) return false;
  if (sc->background_clip_iters < 1) {
    *err = "model.background_clip_iters 必须 >= 1"; return false;
  }
  if (!num("background_max_contamination", &sc->background_max_contamination)) return false;
  if (sc->background_max_contamination <= 0.0 ||
      sc->background_max_contamination >= 1.0) {
    *err = "model.background_max_contamination 必须在 (0,1)"; return false;
  }
  if (!num("background_contamination_sigma", &sc->background_contamination_sigma)) return false;
  if (sc->background_contamination_sigma <= 0.0) {
    *err = "model.background_contamination_sigma 必须 > 0"; return false;
  }
  if (!num("background_min_retained_fraction", &sc->background_min_retained_fraction)) return false;
  if (sc->background_min_retained_fraction <= 0.0 ||
      sc->background_min_retained_fraction > 1.0) {
    *err = "model.background_min_retained_fraction 必须在 (0,1]"; return false;
  }
  if (!num("background_tolerance", &sc->background_tolerance)) return false;
  if (sc->background_tolerance <= 0.0) {
    *err = "model.background_tolerance 必须 > 0"; return false;
  }
  if (!i32("background_neighbor_radius", &sc->background_neighbor_radius)) return false;
  if (sc->background_neighbor_radius < 1) {
    *err = "model.background_neighbor_radius 必须 >= 1"; return false;
  }
  if (!i32("background_catalog_veto", &sc->background_catalog_veto)) return false;
  if (sc->background_catalog_veto != 0 && sc->background_catalog_veto != 1) {
    *err = "model.background_catalog_veto 必须 ∈ {0,1}"; return false;
  }
  // sampler.h:56-59 声明的 veto/掩膜口径（与 catalog veto 同源，见 005）
  if (!num("control_k_corr", &sc->control_k_corr)) return false;
  if (sc->control_k_corr <= 0.0) {
    *err = "model.control_k_corr 必须 > 0"; return false;
  }
  if (!num("star_mask_snr_factor", &sc->star_mask_snr_factor)) return false;
  if (!(sc->star_mask_snr_factor > 0.0)) {
    *err = "model.star_mask_snr_factor 必须 > 0"; return false;
  }
  if (!num("star_mask_radius_deg", &sc->star_mask_radius_deg)) return false;
  if (!(sc->star_mask_radius_deg > 0.0)) {
    *err = "model.star_mask_radius_deg 必须 > 0"; return false;
  }
  return true;
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
  // CONFORM-FIX-B-014：透传 cfg 的 sampler 配置键（CONFIG_SCHEMA.md:11-18
  // 「model:」段；键集 = P2SamplerConfig 全字段）。缺键 = 编译期默认值
  // （sampler.cpp:302-322 单一来源），非法值 = DATA fail-closed（不夹取）。
  {
    std::string sc_err;
    if (!p2_sample_cfg_from_doc(doc, &sc, &sc_err))
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "sampler config rejected (fail-closed): " + sc_err));
  }
  // CON-004: cpu_workers = Runtime lease 权威（P2NodeModule execute 注入
  // __workers; budget 唯一权威, 禁硬编码; 1=串行 reference）。恒最后赋值，
  // 不允许被 model.cpu_workers 覆盖（lease 是唯一权威）。
  sc.cpu_workers = std::max(1, doc.value("__workers", 1));
  (*man)["sampler_config_source"] =
      doc.contains("model") ? "config.model+defaults" : "compiled_defaults";

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
                       // CONFORM-FIX-B-014: 生效 sampler 配置全量落盘（调参后
                       // 重跑的可审计面；键名与 CONFIG_SCHEMA「model:」段一致）
                       {"sampler_config", Json{
                            {"control_grid_per_tile", sc.control_grid_per_tile},
                            {"patch_radius_leaf", sc.patch_radius_leaf},
                            {"min_samples", sc.min_samples},
                            {"snr_search_radius_deg", sc.snr_search_radius_deg},
                            {"background_patch_radius", sc.background_patch_radius},
                            {"background_clip_sigma", sc.background_clip_sigma},
                            {"background_clip_iters", sc.background_clip_iters},
                            {"background_max_contamination", sc.background_max_contamination},
                            {"background_contamination_sigma", sc.background_contamination_sigma},
                            {"background_min_retained_fraction", sc.background_min_retained_fraction},
                            {"background_tolerance", sc.background_tolerance},
                            {"background_neighbor_radius", sc.background_neighbor_radius},
                            {"background_catalog_veto", sc.background_catalog_veto},
                            {"control_k_corr", sc.control_k_corr},
                            {"star_mask_snr_factor", sc.star_mask_snr_factor},
                            {"star_mask_radius_deg", sc.star_mask_radius_deg},
                            {"cpu_workers", sc.cpu_workers}}},
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
  // CONFORM-FIX-B-001（合规回退）：FZ-UPM-CONVERGENCE 冻结
  // tol=1e-6（docs/algorithms/GATES_AND_TOLERANCES.md，
  // 明文「改动 tol/σ_floor → rc!=0」），PHASE2_UPM_IMPL.md:379/:400-401 与
  // DATA_SEMANTICS:1874 同值且标注「冻结面，任何修改必须走 SCI/合同变更」。
  // 上一版实现就地改为 tolerance=1e-3 + tolerance_relative=1（未走变更流程）
  // ⇒ 生产门 ≈1e-3·max|M| ≈3e12 ADU，比冻结门宽 18 个数量级，且与
  // p2_session.cpp:204（仍 1e-6）分叉。现回退到冻结值，恢复符合性；
  // 相对判据（尺度无关）的**授权路径**见变更 claim 草案
  // 工程控制/RELEASE-02/change-claims/CONFORM-FIX-B-001-tolerance-relative.md
  // （状态：草案，待负责人裁决）——未经裁决不得在实现内启用。
  // 显式 opt-in 覆盖键 upm.tolerance / upm.tolerance_relative 保留（默认 0）。
  uc.tolerance = 1e-6;
  // SCI-502 FIX-1 定案（DOC-502 / 11_upm.md 4.6 / PHASE2_UPM_IMPL 496）：
  // 收敛判据必须**无量纲**、分母用观测量尺度。绝对容差 1e-6 在 ~300 e⁻ 尺度
  // 永不收敛（300 次迭代 converged=0，SCI-C C1 A7b）⇒ 生产默认走相对判据；
  // upm.cpp 内部以 max(scale_obs, 1.0) 保留近零尺度下的绝对容差保护。
  uc.tolerance_relative = 1;
  // RELEASE-02 P2a-2/P2a-4（科学行为变更）：阻尼 α=0.5（naive α=1 在
  // 链式/二部覆盖图有特征值 -1、周期 2 振荡）；M 全帧加权；末端残差场
  // gauge 使叠加 ≡ 公共场 ⇒ 覆盖子集突变处阶跃恒 0（q2-snr-smooth §4/§5）。
  uc.gs_damping = 0.5;
  uc.m_full_frame = 1;
  uc.final_gauge = 1;
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
  // CONFORM-FIX-B-009：与 stage2 工具同一「smoothing」键语义
  // （CONFIG_SCHEMA.md:19 smoothing(auto→0.1)；"auto" 的解析值单一来源 =
  // P2_SMOOTHING_LAMBDA_AUTO）。缺键保持 upm.h:75 的编译期默认 0.0 ——
  // 该默认属 docs/algorithms/PHASE2_UPM_IMPL.md §13「冻结面」，改动须走
  // SCI/合同变更；与负责人裁决 GAP_AUDIT §9.39 A5「λ 不能为 0」的冲突已
  // 登记上呈（生产 λ 取值归 SMOOTH-LAMBDA 分片），本节点不擅自改冻结默认。
  {
    const Json model_cfg = (doc.contains("model") && doc["model"].is_object())
                               ? doc["model"] : Json::object();
    if (model_cfg.contains("smoothing")) {
      const Json& sm = model_cfg["smoothing"];
      if (sm.is_string()) {
        if (sm.get<std::string>() != "auto")
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "model.smoothing 只支持 'auto' 或 number: " +
              sm.get<std::string>()));
        uc.smoothing_lambda = P2_SMOOTHING_LAMBDA_AUTO;
      } else if (sm.is_number()) {
        uc.smoothing_lambda = sm.get<double>();
      } else {
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "model.smoothing 类型错误（'auto' 或 number）"));
      }
      if (!(uc.smoothing_lambda >= 0.0))
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "model.smoothing 必须 >= 0"));
    } else if (model_cfg.contains("smoothing_lambda")) {
      if (!model_cfg["smoothing_lambda"].is_number())
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "model.smoothing_lambda must be number"));
      uc.smoothing_lambda = model_cfg["smoothing_lambda"].get<double>();
      if (!(uc.smoothing_lambda >= 0.0))
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "model.smoothing_lambda 必须 >= 0"));
    }
  }
  if (upm_cfg.contains("smoothing_lambda"))
    uc.smoothing_lambda = upm_cfg["smoothing_lambda"].get<double>();
  (*man)["upm_smoothing_lambda"] = uc.smoothing_lambda;
  (*man)["upm_smoothing_lambda_source"] =
      (upm_cfg.contains("smoothing_lambda")
           ? "config.upm.smoothing_lambda"
           : ((doc.contains("model") && doc["model"].is_object() &&
               (doc["model"].contains("smoothing") ||
                doc["model"].contains("smoothing_lambda")))
                  ? "config.model.smoothing"
                  : "compiled_default_alg13_frozen_0.0"));
  // M4-C-02: 与 stage2_common 对称的显式覆盖面；缺省保持 SCI §9a:133 λ0=1e-3。
  if (upm_cfg.contains("zero_anchor_weight"))
    uc.zero_anchor_weight = upm_cfg["zero_anchor_weight"].get<double>();
  // RELEASE-02 P2a 显式可配置（缺省 = 上面的生产值；便于对照/回归与
  // 负责人按 c-delta-ruling 裁决切换）。
  if (upm_cfg.contains("tolerance"))
    uc.tolerance = upm_cfg["tolerance"].get<double>();
  if (upm_cfg.contains("tolerance_relative"))
    uc.tolerance_relative = upm_cfg["tolerance_relative"].get<int>();
  if (upm_cfg.contains("gs_damping"))
    uc.gs_damping = upm_cfg["gs_damping"].get<double>();
  if (upm_cfg.contains("m_full_frame"))
    uc.m_full_frame = upm_cfg["m_full_frame"].get<int>();
  if (upm_cfg.contains("final_gauge"))
    uc.final_gauge = upm_cfg["final_gauge"].get<int>();

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
  // RELEASE-02 P2a-3（收敛状态可见）：iterations/converged/objective 此前
  // 只进 .bin（模型 JSON 文本），p2_upm_model.json 契约面缺失、且本节点从不
  // 调用 p2_upm_convergence。此处显式读取并落盘到 .json + manifest，使
  // "不收敛"在数据面上可见（禁 rc=0 冒充已收敛）。
  uint64_t upm_iterations = 0;
  double upm_objective = 0.0;
  int upm_converged = 0;
  if (p2_upm_convergence(model, &upm_iterations, &upm_objective,
                         &upm_converged) != 0) {
    upm_iterations = 0;
    upm_objective = 0.0;
    upm_converged = 0;   // 读不到一律按"未证明收敛"
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
                       {"use_ivar_weight", 1},
                       // RELEASE-02 P2a-3：IRLS 收敛状态（只读访问器）
                       {"iterations", upm_iterations},
                       {"converged", upm_converged},
                       {"objective", upm_objective},
                       // RELEASE-02 P2a-2/P2a-3/P2a-4 求解器行为 provenance
                       {"tolerance", uc.tolerance},
                       {"tolerance_relative", uc.tolerance_relative},
                       {"gs_damping", uc.gs_damping},
                       {"m_full_frame", uc.m_full_frame},
                       {"final_gauge", uc.final_gauge}};
  if (!p2_write_text(out_path, artifact.dump(2))) {
    aio_fs::remove(bin_path);
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed: " + out_path));
  }
  // A4: upm_save_path/persist_upm 是已登记 session 键（p2_session 语义）; 正式
  // 节点链为唯一写者, 故在此按其语义把模型落盘到指定路径（键可达, 非 silent 忽略）。
  Json upm_arts = Json::array({out_path, bin_path});
  if (doc.value("persist_upm", false) && doc.contains("upm_save_path") &&
      doc["upm_save_path"].is_string() && !doc["upm_save_path"].get<std::string>().empty()) {
    const std::string save_path = doc["upm_save_path"].get<std::string>();
    // CLEAN-403: 复制经 aio (copy_file = 分块流式 → 临时文件 → fsync → 原子 rename,
    // overwrite=true 覆盖目标; 不出现半写副本)。
    const int crc = aio_atomic::copy_file(bin_path, save_path, true);
    if (crc != 0)
      return Result<void>::fail(Error(ErrorDomain::IO,
          "upm_save_path copy failed: " + save_path + " (errno=" +
              std::to_string(crc) + ")"));
    upm_arts.push_back(save_path);
  }
  (*man)["artifacts"] = upm_arts;
  (*man)["upm_model_artifact"] = out_path;
  (*man)["upm_model_bin"] = bin_path;
  (*man)["model_hash"] = std::string(info.model_hash);
  (*man)["observation_count"] = info.observation_count;
  // RELEASE-02 P2a-3：收敛状态进 manifest（不收敛必须对机器消费者可见）
  (*man)["upm_iterations"] = upm_iterations;
  (*man)["upm_converged"] = upm_converged;
  (*man)["upm_objective"] = upm_objective;
  (*man)["upm_tolerance"] = uc.tolerance;
  (*man)["upm_tolerance_relative"] = uc.tolerance_relative;
  (*man)["upm_gs_damping"] = uc.gs_damping;
  (*man)["upm_m_full_frame"] = uc.m_full_frame;
  (*man)["upm_final_gauge"] = uc.final_gauge;

  // ── FIX-A 天光面（P0-08/P0-09）接入生产 mosaic 链 ─────────────────────────
  // DESIGN §4.4: 星点掩膜后逐帧稀疏天光采样 → 全部帧联合建参考天光面
  // B_ref(x)+δ_k(x)（稀疏样条, 按需求值, 不建稠密栅格）。采样点直接由
  // background-clean control observations 映射（与 sampler patch estimator
  // 同源）; 成功后 save 供 upm-apply 逐像素扣除。失败显式降级（保留 UPM C
  // 场）并记日志, 不静默、不写半成品。config: doc["sky_plane"]。
  {
    const Json sp_cfg = (doc.contains("sky_plane") && doc["sky_plane"].is_object())
                            ? doc["sky_plane"] : Json::object();
    // CONFORM-FIX-B-011：默认值 = 「本次是否真的会施加 δ」。
    // 依据：① FIX-SCI-SNR-CANON-001（负责人 2026-09-19）§2/§3.1 明文
    //   「FIX-P2a 默认路径为**保留 C 去 δ**，raw − C_k，g_k ≡ 1 本期不启用」
    //   ⇒ 生产默认 additive_mode = "c"（本文件 :5503 起），δ 从不施加；
    //   ② docs/ 内**无任何**规定 sky_plane.enabled 默认值的条款
    //   （grep docs/ 仅命中 UNIFIED_MODEL.md:49 对象描述与
    //   UNIFIED_SCIENCE_MODEL.md:122 的 UNRESOLVED 登记）；
    //   ③ 唯一「默认开启」记录是 FIX-A 目标模型前提下的前台选项 a
    //   （工程控制/RELEASE-02/ACCEPTANCE.md:11、reports/RELEASE-02/FIX-A-report.md:73,140,156），
    //   而 FIX-A-UPM-001 已被 FIX-SCI-SNR-CANON-001 否决 ⇒ 该前提消失。
    // 处置：缺省 = (additive_mode ∈ {delta,both})，即「要施加才构建」；
    // 显式 sky_plane.enabled 始终优先。默认路径不再产出无消费方的
    // p2_sky_plane.bin（其稀疏样条拟合 + Schur 解是纯成本），
    // 且 additive_mode=delta 时不会静默退化为 c。
    const Json seam_pre =
        (doc.contains("seam") && doc["seam"].is_object()) ? doc["seam"]
                                                         : Json::object();
    // §9.67 定案 1：默认 "delta"（多退少补到公共天光面，保留 B_ref）
    const std::string additive_mode_pre =
        seam_pre.value("additive_mode", std::string("delta"));
    const bool delta_wanted =
        (additive_mode_pre == "delta" || additive_mode_pre == "both");
    const bool sky_enabled = sp_cfg.value("enabled", delta_wanted);
    (*man)["sky_plane_enabled"] = sky_enabled;
    (*man)["sky_plane_enabled_default_source"] =
        sp_cfg.contains("enabled")
            ? "config"
            : (delta_wanted ? "additive_mode_applies_delta" : "additive_mode_c");
    if (!sky_enabled) {
      (*man)["sky_plane_status"] = "disabled";
      (*man)["sky_plane_degraded"] = true;   // 显式登记：本次 mosaic 无天光面扣除
    } else if (obs.empty()) {
      std::fprintf(stderr, "[sky_plane] no control observations -> fallback to UPM C field\n");
      (*man)["sky_plane_status"] = "fallback_no_samples";
      (*man)["sky_plane_degraded"] = true;   // 顶层可见：天光面未生效
    } else {
      std::vector<P2SkySample> sky_samples;
      sky_samples.reserve(obs.size());
      for (const auto& o : obs) {
        P2SkySample sk{};
        sk.frame_id = o.frame_id;
        sk.control_id = o.control_id;
        sk.ra_deg = o.ra_deg;
        sk.dec_deg = o.dec_deg;
        sk.value = o.value;
        sk.variance = o.control_variance;
        sk.snr = (o.uncertainty > 0.0) ? std::fabs(o.value) / o.uncertainty : 0.0;
        sk.flags = o.snr_available ? P2_SKY_FLAG_NONE : P2_SKY_FLAG_NO_LOCAL_SNR;
        sky_samples.push_back(sk);
      }
      P2SkyPlaneConfig spc = p2_sky_plane_default_config();
      spc.spline_degree = sp_cfg.value("spline_degree", 3);
      // CHAIN-WIRE-ADAPT-01 / W4: 节点间距由输入几何导出（PHASE2_UPM 7a）。
      // 旧实现写死 sp_cfg.value("node_spacing_deg", 1.0)：默认值 1.0 是一个标定
      // 常数，7a:185-189 明文「禁止在配置里留一个『默认节点间距』标定值」；且
      // 1.0 度在 M42 上比规则 1 的上界（0.0752 度）粗 13.3 倍，B_ref 在整个视场上
      // 几乎退化成常数，帧间天光差的空间结构表示不了（run/UPM-NODE-ADAPT-01 7.4
      // 实测南行减中行电平差 +10.913%）。现改为：配置显式给出则逐字沿用；未给出
      // 则由本产品的控制点观测几何导出，不回退任何常数。
      if (sp_cfg.contains("node_spacing_deg"))
        spc.node_spacing_deg = sp_cfg["node_spacing_deg"].get<double>();
      P2SkyPlaneGeometry sp_geo{};
      P2SkyPlaneGeometryProvenance sp_geo_prov{};
      bool sp_geo_ok = false;
      {
        double px_scale = sp_cfg.value("pixel_scale_arcsec", 0.0);
        if (!(std::isfinite(px_scale) && px_scale > 0.0)) {
          if (doc.contains("hips_paths") && doc["hips_paths"].is_array() &&
              !doc["hips_paths"].empty() && doc["hips_paths"][0].is_string()) {
            const std::string st =
                doc["hips_paths"][0].get<std::string>() + "/p1_stack.json";
            std::string stext;
            if (aio_fs::read_all(st, &stext)) {
              try {
                const Json sd = Json::parse(stext);
                px_scale = sd.value("finest_input_arcsec", 0.0);
              } catch (const std::exception&) { px_scale = 0.0; }
            }
          }
        }
        std::vector<double> o_ra(obs.size()), o_dec(obs.size());
        std::vector<std::uint64_t> o_fid(obs.size()), o_cid(obs.size());
        for (std::size_t i = 0; i < obs.size(); ++i) {
          o_ra[i] = obs[i].ra_deg;
          o_dec[i] = obs[i].dec_deg;
          o_fid[i] = obs[i].frame_id;
          o_cid[i] = obs[i].control_id;
        }
        std::vector<double> c_ra(nodes.size()), c_dec(nodes.size());
        std::vector<std::uint64_t> c_tile(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
          c_ra[i] = nodes[i].ra_deg;
          c_dec[i] = nodes[i].dec_deg;
          c_tile[i] = nodes[i].tile_ipix;
        }
        P2SkyPlaneGeometryInputs gi{};
        gi.obs_ra_deg = o_ra.data();
        gi.obs_dec_deg = o_dec.data();
        gi.obs_frame_id = o_fid.data();
        gi.obs_control_id = o_cid.data();
        gi.n_obs = obs.size();
        gi.ctrl_ra_deg = c_ra.data();
        gi.ctrl_dec_deg = c_dec.data();
        gi.ctrl_tile_ipix = c_tile.data();
        gi.n_ctrl = nodes.size();
        gi.pixel_scale_arcsec = px_scale;
        char gerr[512] = {0};
        sp_geo_ok = (::p2_sky_plane_geometry_from_controls(
                         gi, &sp_geo, &sp_geo_prov, gerr, sizeof(gerr)) == 0);
        spc.geometry = sp_geo;
        (*man)["sky_plane_geometry_source"] =
            sp_geo_ok ? "derived_from_control_observations" : "unavailable";
        (*man)["sky_plane_geometry_error"] = std::string(gerr);
        if (sp_geo_ok) {
          (*man)["sky_plane_geometry"] = Json{
              {"overlap_band_width_deg", sp_geo.overlap_band_width_deg},
              {"pointing_spacing_deg", sp_geo.pointing_spacing_deg},
              {"sample_pitch_deg", sp_geo.sample_pitch_deg},
              {"pixel_scale_arcsec", sp_geo.pixel_scale_arcsec},
              {"n_obs", sp_geo_prov.n_obs},
              {"n_frames", sp_geo_prov.n_frames},
              {"n_pointings", sp_geo_prov.n_pointings},
              {"n_ctrl", sp_geo_prov.n_ctrl},
              {"frame_separation_fence_deg", sp_geo_prov.frame_separation_fence_deg},
              {"frame_extent_median_deg", sp_geo_prov.frame_extent_median_deg},
              {"pointing_spacing_min_deg", sp_geo_prov.pointing_spacing_min_deg},
              {"pointing_spacing_max_deg", sp_geo_prov.pointing_spacing_max_deg},
              {"overlap_band_trimmed_deg", sp_geo_prov.overlap_band_trimmed_deg},
              {"overlap_band_full_deg", sp_geo_prov.overlap_band_full_deg},
              {"n_overlap_pairs", sp_geo_prov.n_overlap_pairs},
              {"sample_pitch_median_deg", sp_geo_prov.sample_pitch_deg},
              {"n_pitch_samples", sp_geo_prov.n_pitch_samples},
              {"rule", "PHASE2_UPM 7a: h <= min(overlap_band, pointing_spacing)/2,"
                       " derived from the product's own input geometry (no calibrated"
                       " default)"}};
          P2SkyPlaneNodeSpacing ns{};
          char nserr[512] = {0};
          if (p2_sky_plane_derive_node_spacing(&sp_geo, &ns, nserr, sizeof(nserr)) ==
              P2_SKY_NODE_SPACING_OK) {
            (*man)["sky_plane_node_spacing_upper_deg"] = ns.upper_deg;
            (*man)["sky_plane_node_spacing_lower_deg"] = ns.lower_deg;
            (*man)["sky_plane_node_spacing_px"] = ns.node_spacing_px;
            (*man)["sky_plane_representable_scale_deg"] = ns.representable_scale_deg;
          } else {
            (*man)["sky_plane_node_spacing_derive_error"] = std::string(nserr);
          }
        } else {
          std::fprintf(stderr,
                       "[sky_plane] input geometry derivation FAILED: %s -> node"
                       " spacing cannot be derived; build will fail-closed"
                       " (PHASE2_UPM 7a forbids falling back to a calibrated constant)\n",
                       gerr);
        }
      }
      spc.frame_gradient_order = sp_cfg.value("frame_gradient_order", 1);
      spc.gauge_mode = sp_cfg.value("gauge_mode", 0);
      spc.weight_mode = sp_cfg.value("weight_mode", 0);
      spc.roughness_penalty = sp_cfg.value("roughness_penalty", 1e-3);
      if (sp_cfg.contains("huber_delta")) spc.huber_delta = sp_cfg["huber_delta"].get<double>();
      if (sp_cfg.contains("max_iterations")) spc.max_iterations = sp_cfg["max_iterations"].get<int>();
      if (sp_cfg.contains("tolerance")) spc.tolerance = sp_cfg["tolerance"].get<double>();
      if (sp_cfg.contains("kappa_max")) spc.kappa_max = sp_cfg["kappa_max"].get<double>();
      if (sp_cfg.contains("rank_rtol")) spc.rank_rtol = sp_cfg["rank_rtol"].get<double>();
      if (sp_cfg.contains("min_samples")) spc.min_samples = sp_cfg["min_samples"].get<int>();
      if (sp_cfg.contains("min_samples_per_frame"))
        spc.min_samples_per_frame = sp_cfg["min_samples_per_frame"].get<int>();
      // CHAIN-WIRE-ADAPT-01 / W4: max_nodes 护栏。7a:199-200「自适应必须真的会被
      // 触发：门控值若宽到让真实数据永远通过，则自适应等价于不存在」。算法侧默认
      // 2048 个节点会把 M42 上科学上更优的 h = 0.0375948 度（n_nodes = 4814）挡在
      // 门外，自适应 fail-closed 返回 rc=5（TOO_MANY_NODES），一个模型都不给
      // （run/UPM-NODE-ADAPT-01 7.5）。取值依据（两条，均非「拍一个更大的数」）：
      //   1) 必须容纳规则区间内实际可达的最细网格：M42 实测逐档节点数
      //      1.0->36、0.15->391、0.0752->1333、0.05->2835、0.0376->4814、
      //      0.0188->19256（run/UPM-NODE-ADAPT-01 7.2）。生产要能采纳 4814 这一档。
      //   2) 内存：求解矩阵是 n_full x n_full 稠密 FP64（n_full = n_free + 帧数 x
      //      delta 基函数数）；8192 节点档的峰值 RSS 实测见
      //      run/CHAIN-WIRE-ADAPT-01/REPORT.md W4（远低于本节点既有预算）。
      // 取 8192：容纳 4814（余量 1.70x），并与 sky_plane.h 声明的
      // 「max_nodes 默认 8192」一致（算法侧实现里的 2048 与该声明漂移；编排面按
      // 声明取值，不改算法侧）。配置显式给出时逐字沿用。
      if (sp_cfg.contains("max_nodes")) spc.max_nodes = sp_cfg["max_nodes"].get<int>();
      else spc.max_nodes = 8192;
      char sperr[512] = {0};
      void* spm = nullptr;
      // ── SCI-502 FIX-3 定案 + CHAIN-WIRE-ADAPT-01 / W4: 近奇异 + 节点间距自适应 ──
      // 旧实现只做「粗糙度惩罚逐级 x10」这一条分支（kappa 分支），节点间距被
      // 写死的 1.0 度钉死 ⇒ 7a:190-192「节点间距必须与 roughness_penalty **同级**
      // 进入自适应重试回路；条件数或残差不达门时，除提高粗糙度惩罚外，必须允许
      // 细化节点重解」在生产面上**不可达**（UPM-NODE-ADAPT-01 7.3 实测：生产数据
      // 的权重尺度下 lambda 要到 1e3 量级才影响 kappa，默认 1e-3 差 6 个数量级
      // ⇒ 惩罚分支事实上不具调节能力，真正干活的是节点细化分支）。
      // 现改用 p2_sky_plane_build_adaptive：两条分支都在同一个有界回路里，
      // 搜索区间由输入几何给（上界 = 规则 1 的值，下界 = 数据分辨率极限），
      // 逐次尝试的 (h, lambda, rc, action, adopted) 全部入 provenance。
      // 放宽 kappa_max 求绿仍属禁止项（FZ-AP2S-KAPPA-MAX 负例）。
      const double kappa_penalty0 = spc.roughness_penalty;
      P2SkyPlaneAdaptiveConfig spad = p2_sky_plane_default_adaptive_config();
      P2SkyPlaneAdaptiveReport spread{};
      int src = p2_sky_plane_build_adaptive(sky_samples.data(), sky_samples.size(),
                                            &spc, &spad, &spm, &spread,
                                            sperr, sizeof(sperr));
      // 节点间距自适应是否真的被触发过（7a:199-200：无触发记录的路径视为未实现）。
      (*man)["sky_plane_node_adaptive_used"] = (spread.node_adaptive_used != 0);
      (*man)["sky_plane_penalty_adaptive_used"] = (spread.penalty_adaptive_used != 0);
      (*man)["sky_plane_adaptive_attempts"] = spread.n_attempts;
      (*man)["sky_plane_node_refinements"] = spread.n_node_refinements;
      (*man)["sky_plane_node_coarsenings"] = spread.n_node_coarsenings;
      (*man)["sky_plane_penalty_escalations"] = spread.n_penalty_escalations;
      (*man)["sky_plane_representation_limited"] = (spread.representation_limited != 0);
      (*man)["sky_plane_clamped_to_upper"] = (spread.clamped_to_upper != 0);
      (*man)["sky_plane_adaptive_search"] = Json{
          {"node_spacing_upper_deg", spread.node_spacing_upper_deg},
          {"node_spacing_lower_deg", spread.node_spacing_lower_deg},
          {"constraining_scale_deg", spread.constraining_scale_deg},
          {"residual_improve_ratio", spread.residual_improve_ratio},
          {"kappa_max_effective", spread.kappa_max_effective},
          {"node_spacing_deg", spread.node_spacing_deg},
          {"roughness_penalty", spread.roughness_penalty}};
      {
        Json att = Json::array();
        for (int ai = 0; ai < spread.n_attempts && ai < P2_SKY_ADAPT_MAX_ATTEMPTS; ++ai) {
          const P2SkyPlaneAttempt& a = spread.attempts[ai];
          att.push_back(Json{{"node_spacing_deg", a.node_spacing_deg},
                             {"roughness_penalty", a.roughness_penalty},
                             {"rc", a.rc},
                             {"action", a.action},
                             {"adopted", a.adopted},
                             {"kappa", a.kappa},
                             {"kappa_data", a.kappa_data},
                             {"rank", a.rank},
                             {"rank_solve", a.rank_solve},
                             {"n_nodes", a.n_nodes},
                             {"chi2_red", a.chi2_red},
                             {"rms_weighted", a.rms_weighted}});
        }
        (*man)["sky_plane_adaptive_trace"] = att;
      }
      (*man)["sky_plane_kappa_adaptive_attempts"] = spread.n_penalty_escalations;
      (*man)["sky_plane_kappa_adaptive_used"] = (spread.n_penalty_escalations > 0);
      (*man)["sky_plane_roughness_penalty_configured"] = kappa_penalty0;
      (*man)["sky_plane_roughness_penalty_used"] = spread.roughness_penalty;
      // 生效门 = H_solve 的谱自身口径（1/rank_rtol），显式覆盖时即配置值（7a:196-198）。
      (*man)["sky_plane_kappa_max"] = spread.kappa_max_effective;
      if (src != P2_SKY_PLANE_OK) {
        std::fprintf(stderr,
                     "[sky_plane] build FAILED rc=%d %s -> explicit fallback to UPM C field\n",
                     src, sperr);
        (*man)["sky_plane_status"] = "fallback_build_failed";
        (*man)["sky_plane_rc"] = src;
        (*man)["sky_plane_error"] = std::string(sperr);
        // DATA-UNC-001 §30.1（unavailable 显式登记）：顶层置降级标志，机器消费者
        // 无法把本次 mosaic 读成"天光面已生效"。rc=6 是否升为硬 fail-closed 由
        // 前台裁决（见 reports/RELEASE-02/fix-sky-report.md §6）。
        (*man)["sky_plane_degraded"] = true;
        if (spm) p2_sky_plane_close(spm);
      } else {
        P2SkyPlaneInfo spinfo{};
        if (p2_sky_plane_info(spm, &spinfo) != 0) std::memset(&spinfo, 0, sizeof(spinfo));
        const std::string sp_path = out_dir + "/p2_sky_plane.bin";
        if (p2_sky_plane_save(spm, sp_path.c_str()) != 0) {
          std::fprintf(stderr, "[sky_plane] save FAILED -> explicit fallback to UPM C field\n");
          (*man)["sky_plane_status"] = "fallback_save_failed";
          (*man)["sky_plane_degraded"] = true;
          p2_sky_plane_close(spm);
        } else {
          p2_sky_plane_close(spm);
          Json arts = (*man)["artifacts"];
          arts.push_back(sp_path);
          (*man)["artifacts"] = arts;
          (*man)["sky_plane_status"] = "ok";
          (*man)["sky_plane_degraded"] = false;
          (*man)["sky_plane_artifact"] = sp_path;
          (*man)["sky_plane_model_hash"] = std::string(spinfo.model_hash);
          (*man)["sky_plane_n_nodes"] = spinfo.n_nodes;
          (*man)["sky_plane_n_frames"] = spinfo.n_frames;
          (*man)["sky_plane_n_used"] = spinfo.n_used;
          // SCI-502 FIX-3 provenance: 条件数/秩/迭代如实落盘（可观测、可审计）
          (*man)["sky_plane_kappa"] = spinfo.kappa;
          // SCI-502 FIX-3：未惩罚数据矩阵条件数（独立诊断量；λ=0 时与 kappa 逐位相等）
          (*man)["sky_plane_kappa_data"] = spinfo.kappa_data;
          (*man)["sky_plane_rank"] = spinfo.rank;
          (*man)["sky_plane_n_params"] = spinfo.n_params;
          (*man)["sky_plane_iterations"] = spinfo.iterations;
          (*man)["sky_plane_chi2_red"] = spinfo.chi2_red;
          (*man)["sky_plane_node_spacing_deg"] = spinfo.node_spacing_deg;
          std::fprintf(stderr,
                       "[sky_plane] ok n_used=%llu n_nodes=%llu n_frames=%llu rms_w=%.6g\n",
                       static_cast<unsigned long long>(spinfo.n_used),
                       static_cast<unsigned long long>(spinfo.n_nodes),
                       static_cast<unsigned long long>(spinfo.n_frames),
                       spinfo.rms_weighted);
        }
      }
    }
  }
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

// ── RELEASE-02 P2b-1: 控制级残差制造者方差表（upm-apply 逐像素方差用）──────
// 归一化把观测 y 映射到被扣除的校正场 ĝ = H y，输出 corrected = y − ĝ = P y
// （P = I − H，"残差制造者"）。p2_samples.json 的每个 control 上，各帧观测以
// control_ivar 加权耦合出公共校正场；采用 **(c) 排除自身**（H_kk=0，与 P2a
// 重构口径一致，不假设参考帧）：
//   Var(corrected_k) = Σ_j (δ_kj − H_kj)² σ_j² ,  H_kj = w_j / W_{-k}
// 逐 leaf 用 tile 内最近 control（8×8 控制格）的值，得到逐像素方差面。
// **不是** σ² + Var(ĝ)（后者漏 −HΣ−ΣHᵀ 交叉项；N=8 高估 1.29×）。
struct P2bControlVar {
  uint64_t tile_ipix = 0;
  int gx = 0, gy = 0;
  std::vector<uint64_t> frame_id;
  std::vector<double> weight;    // control_ivar（>0）
  std::vector<double> variance;  // control_variance（>0）
  std::vector<double> corr_var;  // 残差制造者方差（与 frame_id 同序；NaN=不可算）
};

// 读 p2_samples.json 并逐 control 预计算残差制造者方差。失败 → false + why。
static bool p2b_load_control_var(const std::string& out_dir,
                                 std::vector<P2bControlVar>* out,
                                 int* out_grid, bool include_self,
                                 std::string* why) {
  Json smp;
  if (!p2_read_json(out_dir + "/p2_samples.json", &smp)) {
    if (why) *why = "p2_samples.json missing (control weights for residual maker)";
    return false;
  }
  const int grid = smp.value("control_grid_per_tile", 0);
  if (grid < 2 || grid > 64) {
    if (why) *why = "control_grid_per_tile invalid in p2_samples.json";
    return false;
  }
  if (!smp.contains("controls") || !smp["controls"].is_array() ||
      !smp.contains("observations") || !smp["observations"].is_array()) {
    if (why) *why = "p2_samples.json controls/observations invalid";
    return false;
  }
  out->clear();
  out->reserve(smp["controls"].size());
  std::map<uint64_t, std::size_t> cidx;
  for (const auto& c : smp["controls"]) {
    P2bControlVar e;
    e.tile_ipix = c.value("tile_ipix", 0ull);
    e.gx = c.value("gx", 0);
    e.gy = c.value("gy", 0);
    const uint64_t cid = c.value("control_id", 0ull);
    cidx[cid] = out->size();
    out->push_back(std::move(e));
  }
  for (const auto& o : smp["observations"]) {
    const auto it = cidx.find(o.value("control_id", 0ull));
    if (it == cidx.end()) continue;
    P2bControlVar& e = (*out)[it->second];
    e.frame_id.push_back(o.value("frame_id", 0ull));
    e.weight.push_back(o.value("control_ivar", 0.0));
    e.variance.push_back(o.value("control_variance", 0.0));
  }
  if (out_grid) *out_grid = grid;
  for (P2bControlVar& e : *out) {
    const std::size_t n = e.frame_id.size();
    e.corr_var.assign(n, std::numeric_limits<double>::quiet_NaN());
    if (n < 2) continue;   // 单帧/无观测：无跨帧耦合，残差制造者无定义
    std::vector<double> w(n, 0.0), s2(n, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
      const double v = e.variance[j];
      const double wj = (std::isfinite(e.weight[j]) && e.weight[j] > 0.0)
                            ? e.weight[j]
                            : ((std::isfinite(v) && v > 0.0) ? 1.0 / v : 0.0);
      w[j] = wj;
      s2[j] = (std::isfinite(v) && v > 0.0) ? v : ((wj > 0.0) ? 1.0 / wj : 0.0);
    }
    bool ok = true;
    std::string err;
    for (std::size_t k = 0; k < n && ok; ++k) {
      astrocs::v6::p2var::HatRow row;
      ok = astrocs::v6::p2var::normalized_weight_hat_row(w.data(), n, k,
                                                         include_self, &row,
                                                         &err) &&
           astrocs::v6::p2var::residual_maker_variance(row, s2.data(), n, k,
                                                       &e.corr_var[k], &err);
    }
    if (!ok) e.corr_var.assign(n, std::numeric_limits<double>::quiet_NaN());
  }
  return true;
}

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

  // ── FIX-A 天光面 / FIX-GK 方案 B 归一化面接入生产 mosaic 链 ───────────────
  // upm-fit 成功时落盘 p2_sky_plane.bin; 此处 open 并按需逐像素取 δ_k
  // （δ_k = b_k − B_ref, 不建稠密栅格），把每帧归一化到公共面 B_ref。
  // 文件存在但 open 失败 = 产物损坏 → DATA fail-closed（禁静默跳过）。
  void* sky_model = nullptr;
  {
    const std::string sky_path = out_dir + "/p2_sky_plane.bin";
    std::error_code sec;
    if (aio_fs::exists(sky_path)) {
      if (p2_sky_plane_open(sky_path.c_str(), &sky_model) != 0 || !sky_model)
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "p2_sky_plane_open failed (corrupted sky plane artifact): " + sky_path));
    }
  }
  struct SkyGuard {
    void* m;
    ~SkyGuard() { if (m) p2_sky_plane_close(m); }
  } sky_guard{sky_model};
  // 叶级 nside（ra/dec 求值需要）: coverage target_order → nside=2^(order+9)
  uint32_t nside = 0;
  if (sky_guard.m) {
    Json cov_doc;
    if (!p2_read_json(out_dir + "/p2_coverage.json", &cov_doc))
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "coverage artifact missing (sky plane eval needs target_order): " +
          out_dir + "/p2_coverage.json"));
    const int target_order = cov_doc.value("target_order", -1);
    if (target_order < 0 || target_order > 20)
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "coverage target_order out of range for sky plane eval"));
    nside = 1u << static_cast<uint32_t>(target_order + 9);
  }

  // ── RELEASE-02 P2a-1：单次加性扣除（去掉有害的双重扣除）────────────────
  // 生产原为 corrected = (raw − C_k) − δ_k，两次逐帧加性扣除。实测帧间失配
  //   raw−C = 0.131% / raw−δ = 2.799% / raw−C−δ = 13.974%（比不校正的
  //   13.454% 还差）——C 已把每帧对齐到公共面，δ 是在已对齐场上的第二次
  //   扣除（c-delta-ruling §2）。
  // 配置 doc["seam"]["additive_mode"] ∈ {"delta"(默认), "c", "both"}：
  //   delta = raw − δ_k         （**默认**；多退少补到公共天光面，保留 B_ref）
  //   c     = raw − C_k         （全减，含 B_ref ⇒ 背景被剪掉；仅对照）
  //   both  = raw − C_k − δ_k   （legacy 双重扣除，仅供对照/回归）
  // ⚠ 默认值变更（2026-09-20，负责人 GAP_AUDIT §9.67 定案 1「**多退少补到公共天光面**」）：
  //   原默认 "c" 的依据是 §9.54 裁决 1 的**接缝**判据；该判据经前台复核**是退化的**——
  //   `raw−C` 把整张背景减掉后各帧都 ≈0，两帧相减自然 ≈0 ⇒ **接缝小是因为背景没了，
  //   不是因为对齐做好了**。用「接缝」当判据必然收敛到「全减光」。
  //   负责人定案：`calibrated_k = raw_k − δ_k`，**保留公共天光面 B_ref**。
  //   实测（旧判据口径）：raw 13.454% / raw−δ 2.799% / raw−C 0.131%；
  //   `raw−δ` 的 2.799% 是 **δ 拟合不足**（工程问题），不是概念错。
  const Json seam_cfg = (doc.contains("seam") && doc["seam"].is_object())
                            ? doc["seam"] : Json::object();
  std::string additive_mode =
      seam_cfg.value("additive_mode", std::string("delta"));
  if (additive_mode != "c" && additive_mode != "delta" &&
      additive_mode != "both")
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "seam.additive_mode invalid (expect c|delta|both): " + additive_mode));
  std::string additive_mode_effective = additive_mode;
  if (additive_mode == "delta" && !sky_guard.m) {
    // 无天光面产物 ⇒ δ 不存在；退化为单次 C 扣除并显式登记（绝不静默变成
    // raw 不校正，也绝不回退到双重扣除）。
    additive_mode_effective = "c";
  }
  const bool sub_c = (additive_mode_effective == "c" ||
                      additive_mode_effective == "both");
  const bool sub_delta = (additive_mode_effective == "delta" ||
                          additive_mode_effective == "both");

  // ── PERF-P2 S1.1 (RELEASE-02): frame 级并行（work unit = 一帧）───────────
  // 每帧独立 sig/sup 句柄、独立 p2_corrected_f<fid>.bin 输出文件; 帧间零共享写、
  // 零浮点归约; model/sky_model/local_lut 只读共享。帧结果按下标写各自槽位, join
  // 后按 paths 序组装 ⇒ 产物与串行逐位一致、与线程数/调度顺序无关。
  //
  // 上游 frame_id 复用: sample 阶段已按 coverage 路径序对每帧算过 p2_frame_id
  // （p2_samples.json.frame_ids）。仅当本节点 hips_paths 与 p2_coverage.json.
  // hips_paths **逐元素同序相等**时复用（同一函数同一路径 ⇒ 同一 fid, 产物逐位
  // 不变）; 任一不满足 → 回退现场 p2_node_frame_id（不省读、不改值）。禁按长度/
  // 位置猜测映射（DATA-FRAME-ID-001）。
  std::vector<uint64_t> up_fids(paths.size(), 0);
  {
    Json cov_doc2, smp_doc2;
    if (p2_read_json(out_dir + "/p2_coverage.json", &cov_doc2) &&
        p2_read_json(out_dir + "/p2_samples.json", &smp_doc2) &&
        cov_doc2.contains("hips_paths") && cov_doc2["hips_paths"].is_array() &&
        smp_doc2.contains("frame_ids") && smp_doc2["frame_ids"].is_array()) {
      const auto& cpaths = cov_doc2["hips_paths"];
      const auto& cfids = smp_doc2["frame_ids"];
      if (cpaths.size() == paths.size() && cfids.size() == paths.size()) {
        bool same = true;
        for (size_t i = 0; i < paths.size(); ++i)
          if (cpaths[i].get<std::string>() != paths[i]) { same = false; break; }
        if (same) {
          for (size_t i = 0; i < paths.size(); ++i)
            up_fids[i] = cfids[i].get<uint64_t>();
        }
      }
    }
  }

  struct P2FrameOut {
    bool ok = false;
    ErrorDomain dom = ErrorDomain::INTERNAL;
    std::string err;
    uint64_t fid = 0;
    std::string data_file;
    std::vector<P2FrameTiles::TileData> tiles;
    uint64_t n_pixels = 0;
    // RELEASE-02 P2b-1: 逐像素 Var(corrected) 面
    std::string var_file;
    bool var_ok = false;
    bool pixel_noise_included = false;
    uint64_t n_var_pixels = 0;
  };
  // ── RELEASE-02 P2b-1: 逐像素 Var(corrected) 输入（残差制造者 PΣPᵀ）─────
  // 控制级耦合表来自 p2_samples.json；不可得 → 如实报 variance_available=false
  // （禁伪造方差面；P2b-5 过渡期纪律）。
  std::vector<P2bControlVar> cvar_tab;
  int cvar_grid = 0;
  std::string cvar_why;
  // variance_include_self: false（默认）= (c) 排除自身（与 P2a 重构口径一致）；
  // true = UPM 含自身的加权均值（W2 口径；此时"残差制造者 vs σ²+Var(ĝ)"
  // 差异显著——朴素式对 N 帧高估 (1+1/N)/(1-1/N)）。
  const bool cvar_include_self = doc.value("variance_include_self", false);
  const bool cvar_available = p2b_load_control_var(
      out_dir, &cvar_tab, &cvar_grid, cvar_include_self, &cvar_why);
  std::map<std::pair<uint64_t, int>, std::size_t> cvar_at;
  if (cvar_available)
    for (std::size_t i = 0; i < cvar_tab.size(); ++i)
      cvar_at[{cvar_tab[i].tile_ipix, cvar_tab[i].gy * cvar_grid + cvar_tab[i].gx}] = i;

  const uint32_t workers = std::max(1u, doc.value("__workers", 1u));
  std::vector<P2FrameOut> fouts(paths.size());
  p2_parallel_for(workers, static_cast<uint64_t>(paths.size()),
                  [&](uint64_t fi, uint32_t /*w*/) {
    const size_t f = static_cast<size_t>(fi);
    const std::string& path = paths[f];
    P2FrameOut& fo = fouts[f];
    uint64_t fid = up_fids[f];
    if (fid == 0) {
      std::string err;
      fid = p2_node_frame_id(path, &err);
      if (fid == 0) { fo.dom = ErrorDomain::DATA; fo.err = err; return; }
    }
    AioHipsDataset* sig = aio_hips_open(path.c_str(), AIO_HIPS_RD_SIGNAL);
    AioHipsDataset* sup = aio_hips_open(path.c_str(), AIO_HIPS_RD_SUPPORT);
    if (!sig || !sup) {
      if (sig) aio_hips_close(sig);
      if (sup) aio_hips_close(sup);
      fo.dom = ErrorDomain::IO;
      fo.err = "aio_hips_open failed (frame " + std::to_string(f) + "): " + path +
               " -- " + aio_hips_reader_last_error();
      return;
    }
    const int n_tiles = aio_hips_tile_count(sig);
    if (n_tiles <= 0) {
      aio_hips_close(sig);
      aio_hips_close(sup);
      fo.dom = ErrorDomain::DATA;
      fo.err = "frame has no signal tiles: " + path;
      return;
    }
    std::vector<uint64_t> tile_ipix(static_cast<size_t>(n_tiles));
    for (int t = 0; t < n_tiles; ++t)
      aio_hips_tile_ipix(sig, t, &tile_ipix[static_cast<size_t>(t)]);
    std::sort(tile_ipix.begin(), tile_ipix.end());
    // per-frame corrected bin（typed artifact 数据面; NaN=无覆盖）。PERF-P2:
    // 逐 tile **顺序流式**写（tile t 在 offset t*kP2TileLeafSpan）, 与串行
    // p2_write_bin(ft.data) 的 tile 序拼接逐字节同值; 不整帧驻留内存。
    char fid_hex[17];
    std::snprintf(fid_hex, sizeof(fid_hex), "%016llx",
                  static_cast<unsigned long long>(fid));
    const std::string data_file = out_dir + "/p2_corrected_f" + fid_hex + ".bin";
    // CLEAN-403: 顺序写经 aio (write_open_trunc + append_write + append_close),
    // 本 TU 不自持 ofstream 通道。
    aio_atomic::AppendSink* df = aio_atomic::write_open_trunc(data_file, nullptr);
    if (df == nullptr) {
      aio_hips_close(sig);
      aio_hips_close(sup);
      fo.dom = ErrorDomain::IO;
      fo.err = "corrected bin write failed: " + data_file;
      return;
    }
    // P2b-1: 逐帧 Var(corrected) 输出面（cvar_available 时）；帧自身 Phase1
    // variance 产品可选（缺失 ⇒ 方差只含校正场残差制造者项，如实标记
    // pixel_noise_included=false，不得声称完整 Var(corrected)）。
    const std::string var_file = out_dir + "/p2_corrected_var_f" + fid_hex + ".bin";
    aio_atomic::AppendSink* vdf = nullptr;
    if (cvar_available) {
      vdf = aio_atomic::write_open_trunc(var_file, nullptr);
      if (vdf == nullptr) {
        aio_hips_close(sig);
        aio_hips_close(sup);
        (void)aio_atomic::append_close(df);
        fo.dom = ErrorDomain::IO;
        fo.err = "corrected variance bin write failed: " + var_file;
        return;
      }
      fo.var_file = var_file;
    }
    AioHipsDataset* vds = aio_hips_open(path.c_str(), AIO_HIPS_RD_VARIANCE);
    bool any_pixel_noise = false;
    std::vector<float> sig_buf(kP2TileLeafSpan), sup_buf(kP2TileLeafSpan);
    std::vector<float> var_buf(kP2TileLeafSpan);
    std::vector<double> in_v(kP2TileLeafSpan), out_v(kP2TileLeafSpan);
    std::vector<uint64_t> leaves(kP2TileLeafSpan);
    std::vector<double> tile_out(kP2TileLeafSpan);
    std::vector<double> var_tile(kP2TileLeafSpan);
    uint64_t tile_offset = 0;
    for (int t = 0; t < n_tiles; ++t) {
      const uint64_t tip = tile_ipix[static_cast<size_t>(t)];
      // [RELEASE-02 probe] 逐 tile: sky_plane 应用 (库层 eval_block 已计时, 此处补 tile 上下文)
      ASTROCS_PROBE_SCOPE_CTX(_probe_sky_tile, "phase2", "sky_plane.apply.tile");
      ASTROCS_PROBE_TAG(_probe_sky_tile, "tile_id", static_cast<unsigned long long>(tip));
      std::fill(var_tile.begin(), var_tile.end(),
                std::numeric_limits<double>::quiet_NaN());
      bool has_var_tile = false;
      if (vds && aio_hips_read_tile_f32(vds, tip, var_buf.data()) == 0)
        has_var_tile = true;
      if (aio_hips_read_tile_f32(sig, tip, sig_buf.data()) != 0 ||
          aio_hips_read_tile_f32(sup, tip, sup_buf.data()) != 0) {
        aio_hips_close(sig);
        aio_hips_close(sup);
        (void)aio_atomic::append_close(df);
        fo.dom = ErrorDomain::IO;
        fo.err = "aio_hips_read_tile_f32 failed (frame " + std::to_string(f) +
                 " tile " + std::to_string(tip) + "): " + path;
        return;
      }
      // 天光面求值用逐像素 ra/dec（leaf ipix 只依赖 tile 与局部 LUT, 与帧无关）
      std::vector<double> tile_ra, tile_dec;
      if (sky_guard.m) {
        tile_ra.resize(static_cast<size_t>(kP2TileLeafSpan));
        tile_dec.resize(static_cast<size_t>(kP2TileLeafSpan));
        for (uint64_t i = 0; i < kP2TileLeafSpan; ++i) {
          const uint64_t leaf =
              (tip << (2 * kP2TileShift)) | local_lut[static_cast<size_t>(i)];
          astrocs::healpix::pix2ang_nest(
              nside, leaf, tile_ra[static_cast<size_t>(i)],
              tile_dec[static_cast<size_t>(i)]);
        }
      }
      // valid 像素集（support>0 且 finite）→ 块校正; 无效位置保留 NaN
      uint64_t n_valid = 0;
      std::vector<double> valid_ra, valid_dec;
      if (sky_guard.m) {
        valid_ra.reserve(static_cast<size_t>(kP2TileLeafSpan));
        valid_dec.reserve(static_cast<size_t>(kP2TileLeafSpan));
      }
      for (uint64_t i = 0; i < kP2TileLeafSpan; ++i) {
        const double sv = static_cast<double>(sup_buf[static_cast<size_t>(i)]);
        const double xv = static_cast<double>(sig_buf[static_cast<size_t>(i)]);
        out_v[static_cast<size_t>(i)] = std::numeric_limits<double>::quiet_NaN();
        if (std::isfinite(sv) && sv > 0.0 && std::isfinite(xv)) {
          const uint64_t leaf = (tip << (2 * kP2TileShift)) | local_lut[static_cast<size_t>(i)];
          leaves[static_cast<size_t>(n_valid)] = leaf;
          in_v[static_cast<size_t>(n_valid)] = xv;
          if (sky_guard.m) {
            valid_ra.push_back(tile_ra[static_cast<size_t>(i)]);
            valid_dec.push_back(tile_dec[static_cast<size_t>(i)]);
          }
          ++n_valid;
        }
      }
      if (n_valid > 0) {
        p2_upm_calibrate_block(model, fid, leaves.data(), in_v.data(),
                               out_v.data(), n_valid);   // 唯一真实校正入口
        // RELEASE-02 P2a-1：单次加性扣除（见本函数头 seam.additive_mode）。
        // calibrate_block 已输出 raw − C（并含 P2a-2 末端公共 gauge G）。
        if (!sub_c) {
          // delta 模式：把 C 加回（raw − C → raw − G），只保留 δ 一次扣除。
          // G 是"对所有帧相同"的公共残差场（P2a-2，full_frame=1 时≈0），
          // 不产生帧间/接缝差异，故保留。
          for (uint64_t k = 0; k < n_valid; ++k) {
            const double cval = p2_upm_evaluate_c(model, fid, leaves[k]);
            if (std::isfinite(cval) &&
                std::isfinite(out_v[static_cast<size_t>(k)]))
              out_v[static_cast<size_t>(k)] += cval;
          }
        }
        // FIX-GK / 方案 B：δ_k(x) = b_k(x) − B_ref(x)（逐帧相对公共参考面的偏差）。
        // 越域/未知帧点不扣（状态非 OK → 保持当前值；与 stage2 生产接线同口径）。
        if (sub_delta && sky_guard.m) {
          std::vector<double> dvals(static_cast<size_t>(n_valid), 0.0);
          std::vector<uint8_t> dstat(static_cast<size_t>(n_valid), P2_SKY_EVAL_INVALID);
          p2_sky_plane_eval_delta_block(sky_guard.m, fid, valid_ra.data(),
                                        valid_dec.data(), n_valid, dvals.data(),
                                        dstat.data());
          for (uint64_t k = 0; k < n_valid; ++k) {
            if (dstat[static_cast<size_t>(k)] == P2_SKY_EVAL_OK &&
                std::isfinite(dvals[static_cast<size_t>(k)]) &&
                std::isfinite(out_v[static_cast<size_t>(k)]))
              out_v[static_cast<size_t>(k)] -= dvals[static_cast<size_t>(k)];
          }
        }
        // 回填 valid 位置（calibrate_block 按输入序输出; 重新扫描映射）
        // 同时算 RELEASE-02 P2b-1 逐像素 Var(corrected)：最近 control 的
        // 残差制造者方差（(c) 排除自身），加可选帧 Phase1 逐像素方差。
        const uint64_t tile_side = (1ull << kP2TileShift);
        uint64_t k = 0;
        for (uint64_t i = 0; i < kP2TileLeafSpan; ++i) {
          const double sv = static_cast<double>(sup_buf[static_cast<size_t>(i)]);
          const double xv = static_cast<double>(sig_buf[static_cast<size_t>(i)]);
          if (std::isfinite(sv) && sv > 0.0 && std::isfinite(xv)) {
            tile_out[static_cast<size_t>(i)] = out_v[static_cast<size_t>(k)];
            if (cvar_available && cvar_grid > 0) {
              const uint64_t fx =
                  tile_side - 1ull - i / tile_side;   // FITS index → (x,y)
              const uint64_t fy = i % tile_side;
              const uint64_t cs = tile_side / static_cast<uint64_t>(cvar_grid);
              const int cgx = static_cast<int>(fx / cs);
              const int cgy = static_cast<int>(fy / cs);
              const auto cit = cvar_at.find({tip, cgy * cvar_grid + cgx});
              if (cit != cvar_at.end()) {
                const P2bControlVar& ce = cvar_tab[cit->second];
                std::size_t kk = ce.frame_id.size();
                for (std::size_t z = 0; z < ce.frame_id.size(); ++z)
                  if (ce.frame_id[z] == fid) { kk = z; break; }
                if (kk < ce.frame_id.size() && std::isfinite(ce.corr_var[kk]) &&
                    ce.corr_var[kk] > 0.0) {
                  double vv = ce.corr_var[kk];
                  if (has_var_tile) {
                    const double pv =
                        static_cast<double>(var_buf[static_cast<size_t>(i)]);
                    if (std::isfinite(pv) && pv > 0.0) {
                      vv += pv;
                      any_pixel_noise = true;
                    }
                  }
                  var_tile[static_cast<size_t>(i)] = vv;
                  ++fo.n_var_pixels;
                }
              }
            }
            ++k;
          } else {
            tile_out[static_cast<size_t>(i)] =
                std::numeric_limits<double>::quiet_NaN();
          }
        }
      } else {
        for (uint64_t i = 0; i < kP2TileLeafSpan; ++i)
          tile_out[static_cast<size_t>(i)] =
              std::numeric_limits<double>::quiet_NaN();
      }
      (void)aio_atomic::append_write(
          df, tile_out.data(),
          static_cast<std::size_t>(kP2TileLeafSpan * sizeof(double)));
      if (vdf != nullptr)
        (void)aio_atomic::append_write(
            vdf, var_tile.data(),
            static_cast<std::size_t>(kP2TileLeafSpan * sizeof(double)));
      fo.tiles.push_back(P2FrameTiles::TileData{tip, tile_offset});
      tile_offset += kP2TileLeafSpan;
    }
    if (vds) aio_hips_close(vds);
    aio_hips_close(sig);
    aio_hips_close(sup);
    if (aio_atomic::append_close(df) != 0) {
      fo.dom = ErrorDomain::IO;
      fo.err = "corrected bin write failed: " + data_file;
      return;
    }
    if (vdf != nullptr) {
      if (aio_atomic::append_close(vdf) != 0) {
        fo.dom = ErrorDomain::IO;
        fo.err = "corrected variance bin write failed: " + var_file;
        return;
      }
    }
    fo.fid = fid;
    fo.data_file = data_file;
    fo.n_pixels = tile_offset;
    fo.var_ok = cvar_available && fo.n_var_pixels > 0;
    fo.pixel_noise_included = any_pixel_noise;
    fo.ok = true;
  });

  // 帧序组装（= paths 序, 与串行逐位一致）; 失败按下标升序取首个（= 串行首个失败）。
  Json frames_j = Json::array();
  uint64_t total_pixels = 0;
  bool any_var_ok = false;
  bool all_pixel_noise = true;
  uint64_t var_pixels_total = 0;
  for (size_t f = 0; f < paths.size(); ++f) {
    const P2FrameOut& fo = fouts[f];
    if (!fo.ok)
      return Result<void>::fail(Error(fo.dom, fo.err));
    Json tiles_j = Json::array();
    for (const auto& td : fo.tiles)
      tiles_j.push_back(Json{{"tile_ipix", td.tile_ipix},
                             {"n_pixels", kP2TileLeafSpan},
                             {"offset", td.offset}});
    frames_j.push_back(Json{{"frame_id", fo.fid},
                            {"hips_path", paths[f]},
                            {"data_file", fo.data_file},
                            {"var_file", fo.var_file},
                            {"variance_available", fo.var_ok},
                            {"pixel_noise_included", fo.pixel_noise_included},
                            {"n_var_pixels", fo.n_var_pixels},
                            {"n_tiles", fo.tiles.size()},
                            {"tiles", tiles_j}});
    total_pixels += fo.n_pixels;
    any_var_ok = any_var_ok || fo.var_ok;
    all_pixel_noise = all_pixel_noise && fo.pixel_noise_included;
    var_pixels_total += fo.n_var_pixels;
  }

  const std::string out_path = out_dir + "/p2_corrected.json";
  // CONFORM-FIX-B-012：provenance 自洽。sky_plane_loaded = 天光面产物被成功
  // 载入；sky_plane_applied = δ_k **真的被扣除**。旧实现把 loaded 直接当
  // applied ⇒ 同一 JSON 内 sky_plane_applied=true 与 delta_subtracted=false /
  // sky_plane_mode="none" / additive_combination="raw_minus_C" 并列为互斥声明，
  // 按「本次 mosaic 是否做了天光面扣除」取值的消费者必被误导。
  const bool sky_loaded = (sky_guard.m != nullptr);
  // RELEASE-02 P2b-5: 过渡期诚实标记。方差面 = 残差制造者 PΣPᵀ（(c) 排除自身
  // 控制级耦合）+ 可选逐像素 Phase1 噪声 + 参数协方差 J_out C_θ J_outᵀ。
  // 当前生产 W2 模型无 C_θ API（参数项缺失）且 L4 输入帧无 variance 产品
  // （逐像素噪声缺失）⇒ 不得声称完整 Var(corrected)，uncertainty_available
  // 必须为 false，权重链不得据此声称逆方差加权。
  const bool param_cov_included = false;
  const bool uncertainty_available =
      any_var_ok && all_pixel_noise && param_cov_included;
  const bool delta_applied = sub_delta && sky_loaded;
  const bool sky_applied = delta_applied;   // CONFORM-FIX-B-012: applied == δ 实扣
  // RELEASE-02 P2a-1：单次加性扣除 provenance（组合语义对机器消费者可见）
  std::string combo;
  if (sub_c && delta_applied) combo = "raw_minus_C_minus_delta(legacy)";
  else if (sub_c) combo = "raw_minus_C";
  else if (delta_applied) combo = "raw_minus_delta";
  else combo = "raw(no_additive_correction)";
  Json artifact = Json{{"schema", "DATA-P2-COR"},
                       {"entry", "p2_upm_open/p2_upm_calibrate_block" +
                                     std::string(delta_applied ? "/p2_sky_plane_eval_delta_block" : "")},
                       {"model_hash", model_doc.value("model_hash", "")},
                       // CONFORM-FIX-B-012: applied(实扣) 与 loaded(仅载入) 分离
                       {"sky_plane_applied", sky_applied},
                       {"sky_plane_loaded", sky_loaded},
                       // RELEASE-02 P2a-1：单次加性扣除（默认 raw−C；双重扣除已
                       // 证有害：13.974% vs 0.131%，c-delta-ruling §2）。
                       {"additive_mode_requested", additive_mode},
                       {"additive_mode_effective", additive_mode_effective},
                       {"additive_combination", combo},
                       {"c_subtracted", sub_c},
                       {"delta_subtracted", delta_applied},
                       // FIX-GK 方案 B: 施加的是逐帧 δ_k=b_k−B_ref（保留公共面 B_ref），
                       // 不是整个 b_k（旧口径会把背景归零并产生大量负像素）。
                       {"sky_plane_mode", delta_applied ? "delta_to_B_ref" : "none"},
                       {"sky_plane_artifact",
                        sky_loaded ? (out_dir + "/p2_sky_plane.bin") : std::string()},
                       {"n_pixels_total", total_pixels},
                       {"tile_leaf_span", kP2TileLeafSpan},
                       // ── RELEASE-02 P2b-1/5: 逐像素方差面与诚实标记 ──
                       {"variance_available", any_var_ok},
                       {"variance_model",
                        any_var_ok
                            ? (cvar_include_self
                                   ? "residual_maker_PSigmaPt_include_self_control"
                                   : "residual_maker_PSigmaPt_exclude_self_control")
                            : "unavailable"},
                       {"variance_reason", any_var_ok ? std::string() : cvar_why},
                       {"pixel_noise_included", any_var_ok && all_pixel_noise},
                       {"param_covariance_included", param_cov_included},
                       {"n_var_pixels_total", var_pixels_total},
                       {"uncertainty_available", uncertainty_available},
                       {"frames", frames_j}};
  if (!p2_write_text(out_path, artifact.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed: " + out_path));
  Json cor_arts = Json::array({out_path});
  for (const auto& fr : frames_j) {
    cor_arts.push_back(fr.value("data_file", std::string()));
    const std::string vf = fr.value("var_file", std::string());
    if (!vf.empty()) cor_arts.push_back(vf);
  }
  (*man)["artifacts"] = cor_arts;
  (*man)["corrected_artifact"] = out_path;
  (*man)["n_pixels_total"] = total_pixels;
  (*man)["sky_plane_applied"] = sky_applied;
  (*man)["sky_plane_loaded"] = sky_loaded;   // CONFORM-FIX-B-012
  (*man)["variance_available"] = any_var_ok;
  (*man)["pixel_noise_included"] = any_var_ok && all_pixel_noise;
  (*man)["param_covariance_included"] = param_cov_included;
  (*man)["uncertainty_available"] = uncertainty_available;
  // RELEASE-02 P2a-1：组合语义与降级显式登记
  (*man)["additive_mode_requested"] = additive_mode;
  (*man)["additive_mode_effective"] = additive_mode_effective;
  (*man)["additive_combination"] = combo;
  if (additive_mode == "delta" && additive_mode_effective != "delta")
    (*man)["additive_mode_degraded"] = "no_sky_plane_artifact";
  return Result<void>::success();
}

// ── n=2 档外部先验（FIX-REJ §8 方案 A; REJ-kernel §7 接线约定）──────────────
// **显式 opt-in 辅助（不在生产 AUTO 路由上）**：SD-18（2026-09-18）裁决后
// astrocs_adaptive_pixel 的 n<=3 走保守 none（不排异 + 加权积分），生产
// p2_op_reject **不再调用本函数**、不再逐样本填 prior_sigma/prior_sky
// （原逐像素 31×31 稳健统计粗估 1000-1200s 单线程, 已从生产路径移除）。
// 保留供显式指定 P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA 的调用方复用；因生产
// 路径无引用, 以 [[maybe_unused]] 标注（不产生死代码警告）。
// 对每个 eligible 候选样本, 取该样本所属帧该 tile 内以输出像素为中心的
// 31×31 邻域（clipped 到 tile 边界）稳健统计:
//     prior_sky   = 邻域中位数
//     prior_sigma = 1.4826 × MAD(邻域)
// 中位数与 MAD **同源**（同一邻域）; 按 eligibility 紧凑序写出（调用方用
// src_idx 对齐, 禁止用 compact index 猜 original slot）。
// 任一 eligible 样本邻域有效像素 < kMinValid → 返回 false（调用方不传先验
// 数组; plan.extreme_prior.center_mode=0 ⇒ kernel fail-closed 为
// UNDERDETERMINED, 绝不回退候选栈中位数 —— 那会把 n=2 单离群排异反转成
// 全接受）。tile_span 必须 = 512×512（标准 HiPS tile）。
[[maybe_unused]] static bool p2_reject_local_prior(
    const double* frame_major, uint64_t tile_span, std::uint32_t depth,
    uint64_t pixel, const std::uint32_t* src_idx, std::uint32_t eligible_count,
    double* out_sky, double* out_sigma) {
  constexpr int kHalf = 15;      // 31×31
  constexpr int kTw = 512;
  constexpr int kMinValid = 9;   // 至少 3×3 有效邻域样本
  if (frame_major == nullptr || src_idx == nullptr || out_sky == nullptr ||
      out_sigma == nullptr || tile_span != static_cast<uint64_t>(kTw) * kTw)
    return false;
  const int x0 = static_cast<int>(pixel % static_cast<uint64_t>(kTw));
  const int y0 = static_cast<int>(pixel / static_cast<uint64_t>(kTw));
  std::vector<double> buf;
  buf.reserve(static_cast<size_t>(2 * kHalf + 1) * (2 * kHalf + 1));
  for (std::uint32_t s = 0; s < eligible_count; ++s) {
    const std::uint32_t slot = src_idx[s];
    if (slot >= depth) return false;
    const double* plane = frame_major + static_cast<size_t>(slot) * tile_span;
    buf.clear();
    for (int dy = -kHalf; dy <= kHalf; ++dy) {
      const int yy = y0 + dy;
      if (yy < 0 || yy >= kTw) continue;
      const double* row = plane + static_cast<size_t>(yy) * kTw;
      for (int dx = -kHalf; dx <= kHalf; ++dx) {
        const int xx = x0 + dx;
        if (xx < 0 || xx >= kTw) continue;
        const double v = row[xx];
        if (std::isfinite(v)) buf.push_back(v);
      }
    }
    if (buf.size() < static_cast<size_t>(kMinValid)) return false;
    const size_t mid = buf.size() / 2;
    std::nth_element(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(mid),
                     buf.end());
    const double med = buf[mid];
    for (size_t i = 0; i < buf.size(); ++i) buf[i] = std::fabs(buf[i] - med);
    std::nth_element(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(mid),
                     buf.end());
    const double mad = buf[mid];
    const double sigma = 1.4826 * mad;
    if (!std::isfinite(med) || !std::isfinite(sigma) || !(sigma > 0.0))
      return false;
    out_sky[s] = med;
    out_sigma[s] = sigma;
  }
  return true;
}

// ── op: reject_outliers（唯一真实入口 p2_reject_plan_resolve_n +
//      p2_collect_candidate_stack + p2_reject_stack_ex; AUTO 只在 planning
//      层解析。**逐输出像素几何 n 路由**（DESIGN §4.5）: n = 该像素被多少帧
//      footprint 覆盖（各帧 support 层 >0 计数; coverage 覆盖图）, 按 n 缓存
//      plan; 不得用 frames.size()（整组帧数）, 不得用资格/掩膜后
//      eligible_count（n_eff）。**路由 = WBPP 一手实测
//      表**：N<6 → percentile（含 N≤3）/ 6≤N≤15 → winsorized / N>15 →
//      linear fit；禁止 min/max 与 NoRejection（AUTO 命中即 fail-closed）。
//      N≤3 的实际不排异来自 kernel underdetermined 闸（候选 ≤3 ⇒ 全接受 +
//      UNDERDETERMINED，provenance 如实记录）；「N<6 档下界是否含 N≤3」是
//      rejection.cpp 的**唯一决策点**（EXP-204 待定案）。无逐像素先验计算。
//      kernel 永不执行 AUTO）。rejected_low/high 语义 =
//      threshold 侧计数（禁原始值符号）; eligible<=underdetermined_n →
//      UNDERDETERMINED 全接受并计入 provenance underdetermined_pixels。──
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

  if (tile_span != kP2TileLeafSpan)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "corrected artifact tile_leaf_span != 512*512 (geometric n / 31x31"
        " prior require standard HiPS tiles): " + std::to_string(tile_span)));

  // ── DESIGN §4.5：按**逐输出像素几何 n** 路由（唯一路由依据）──
  // n = 该输出像素被多少帧 footprint 覆盖（各帧 support 层 >0 的帧计数;
  // coverage 覆盖图）。**不得**用 frames.size()（整组帧数, 全图一个值 ⇒
  // 等价于不按像素路由）, **不得**用资格/掩膜后的 eligible_count（n_eff）。
  // p2_reject_plan_resolve_n 是 (profile, request, n, underdetermined_n) 的
  // 纯函数 ⇒ 按 n 缓存 plan（最多 n_max 档, 1/N worker 一致）。
  const std::string profile =
      doc.value("reject_profile", std::string(P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL));
  P2RejectionPlanRequest req{};
  req.request = P2_REJECT_AUTO;   // AUTO 仅在 planning 层解析, 永不进 kernel
  req.profile = profile.c_str();
  req.underdetermined_n = 0;      // 0 = profile 默认（pixel=3: n<=3 保守 none;
                                  // wbpp/adaptive=2 冻结不变, SD-18）
  req.nominal_contributors = 0;   // 逐像素覆盖; resolve_n 传 geom_n
  const std::uint32_t n_max = static_cast<std::uint32_t>(frames.size());
  std::map<std::uint32_t, P2RejectionPlan> plan_cache;   // geom_n → plan
  for (std::uint32_t n = 0; n <= n_max; ++n) {
    P2RejectionPlan p{};
    char perr[256] = {0};
    if (p2_reject_plan_resolve_n(n, &req, &p, perr, sizeof(perr)) != 0)
      return Result<void>::fail(Error(ErrorDomain::DATA,
          std::string("p2_reject_plan_resolve_n(n=") + std::to_string(n) +
          ") failed: " + perr));
    // 显式 opt-in 保护（AUTO 默认路由永不含 extreme_prior；若调用方把
    // request 改成显式先验档, 仍强制外部 prior_sky）：center_mode=1 在
    // prior_sky 缺失时回退候选栈中位数 ⇒ 单离群使两侧同时超阈 → 全拒 →
    // n<=4 全接受容错 ⇒ 排异被反转。强制 center_mode=0：缺先验即 kernel
    // fail-closed（UNDERDETERMINED）, 绝不静默回退栈中位数。
    if (p.method == P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA)
      p.extreme_prior.center_mode = 0;
    plan_cache.emplace(n, p);
  }
  // 各帧 support 层（逐像素几何 n 的唯一来源）: corrected 数据面 NaN 无法区分
  // "无覆盖"与"覆盖但信号非有限", 故不得以 corrected finiteness 冒充覆盖。
  std::vector<AioHipsDataset*> fsup(frames.size(), nullptr);
  for (size_t f = 0; f < frames.size(); ++f) {
    const std::string p = frames[f].value("hips_path", "");
    fsup[f] = aio_hips_open(p.c_str(), AIO_HIPS_RD_SUPPORT);
    if (!fsup[f])
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "frame support product open failed (geometric n source): " + p +
          " -- " + aio_hips_reader_last_error()));
  }
  struct SupGuard {
    std::vector<AioHipsDataset*>* v;
    ~SupGuard() { for (AioHipsDataset* d : *v) if (d) aio_hips_close(d); }
  } sup_guard{&fsup};

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
  // [RELEASE-02 probe] 规模 gauge: 并集 tile 数
  ASTROCS_PROBE_GAUGE("phase2", "reject.union_tiles", static_cast<double>(union_tiles.size()));
  // ── PERF-P2 S1.2 (RELEASE-02): union-tile 级并行 ─────────────────────────
  // 每个输出 tile 的像素完全在 tile 内算完; 跨 tile 只有**整数**计数器归约
  // （结合律成立 ⇒ 顺序无关 ⇒ 逐位一致）。输出按 tile 升序写入预分配固定
  // offset（accepted/nrej/candidates: i*tile_span; sample_mask: depth 前缀和
  // ×tile_span）。kernel 是 (stack,plan,out) 纯函数、plan_cache 只读 ⇒ 每 tile
  // 结果与串行逐位相同。
  //
  // 每个 worker 使用**独立的 support 句柄**（cfitsio 同句柄并发读非线程安全,
  // 与 sampler.cpp:717-730 同款契约）; 上游串行预开的 fsup 仅承担 fail-closed
  // 校验（错误串/域与串行完全一致, 在任何 tile 处理前触发）。
  struct RejTile {
    uint64_t tip = 0;
    uint64_t out_off = 0;
    uint64_t mask_off = 0;
    size_t depth = 0;
    std::vector<size_t> frame_idx;    // union_tiles 构造序（帧升序）
    std::vector<uint64_t> frame_off;  // corrected bin 内元素偏移
  };
  std::vector<RejTile> rtiles;
  rtiles.reserve(union_tiles.size());
  {
    uint64_t out_off = 0, mask_off = 0;
    for (const auto& kv : union_tiles) {
      RejTile rt;
      rt.tip = kv.first;
      rt.out_off = out_off;
      rt.mask_off = mask_off;
      rt.depth = kv.second.size();
      for (const auto& ref : kv.second) {
        rt.frame_idx.push_back(ref.frame_idx);
        rt.frame_off.push_back(ref.offset);
      }
      out_off += tile_span;
      mask_off += static_cast<uint64_t>(rt.depth) * tile_span;
      rtiles.push_back(std::move(rt));
    }
  }
  const size_t n_tiles = rtiles.size();
  const uint32_t workers = std::max(1u, doc.value("__workers", 1u));
  std::vector<uint8_t> accepted_bin(n_tiles * static_cast<size_t>(tile_span));
  std::vector<uint16_t> nrej_bin(n_tiles * static_cast<size_t>(tile_span));
  std::vector<uint16_t> cand_u16(n_tiles * static_cast<size_t>(tile_span));
  // [F-P2-002-02 / B2-A3] 逐样本接受掩码持久化（tile 序拼接; 每 tile
  // depth×tile_span 字节, 索引 [s*tile_span+p], s=原始帧 slot）。像素级
  // accepted(u8) 无法表达部分拒绝像素内逐样本的接受/拒绝; §30.2 完备划分
  // （n_ineligible = depth − nused − nrej）要求 integrate 按原始样本索引
  // 逐样本剔除（01_SCIENCE_AUTHORITY_BASELINE §4: 拒绝掩码按原始样本索引传递）。
  std::vector<uint8_t> sample_mask(n_tiles == 0 ? 0 : rtiles.back().mask_off +
      static_cast<uint64_t>(rtiles.back().depth) * tile_span);
  // 逐 tile 整数计数与失败（join 后按 tile 升序合并/取首个失败）
  std::vector<uint64_t> t_acc(n_tiles, 0), t_rejlow(n_tiles, 0), t_rejhigh(n_tiles, 0);
  std::vector<uint64_t> t_rejsamp(n_tiles, 0), t_undetlow(n_tiles, 0), t_undettot(n_tiles, 0);
  std::vector<std::string> t_err(n_tiles);
  std::vector<int> t_errd(n_tiles, 0);

  struct P2SupportReader {
    const std::vector<std::string>* paths = nullptr;
    std::vector<AioHipsDataset*> ds;
    AioHipsDataset* get(size_t f) {
      if (ds.empty()) ds.assign(paths->size(), nullptr);
      if (!ds[f]) ds[f] = aio_hips_open((*paths)[f].c_str(), AIO_HIPS_RD_SUPPORT);
      return ds[f];
    }
    ~P2SupportReader() { for (AioHipsDataset* d : ds) if (d) aio_hips_close(d); }
  };
  std::vector<std::string> frame_paths(frames.size());
  for (size_t f = 0; f < frames.size(); ++f)
    frame_paths[f] = frames[f].value("hips_path", "");
  std::vector<std::unique_ptr<P2SupportReader>> readers(workers);
  for (uint32_t w = 0; w < workers; ++w) {
    readers[w] = std::unique_ptr<P2SupportReader>(new P2SupportReader());
    readers[w]->paths = &frame_paths;
  }
  p2_parallel_for(workers, static_cast<uint64_t>(n_tiles),
                  [&](uint64_t ti, uint32_t w) {
    const RejTile& rt = rtiles[static_cast<size_t>(ti)];
    P2SupportReader& rd = *readers[w];
    const size_t ti_s = static_cast<size_t>(ti);
    const uint64_t tip = rt.tip;
    // [RELEASE-02 probe] 逐 tile 热点: reject
    ASTROCS_PROBE_SCOPE_CTX(_probe_rej_tile, "phase2", "reject.tile");
    ASTROCS_PROBE_TAG(_probe_rej_tile, "tile_id", static_cast<unsigned long long>(tip));
    ASTROCS_PROBE_GAUGE("phase2", "reject.tile_frames", static_cast<double>(rt.depth));
    const uint64_t depth = rt.depth;
    if (depth > 255) {
      t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
      t_err[ti_s] = "tile depth > 255 exceeds u16 rejection counters";
      return;
    }
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
        if (!p2_read_bin_range<double>(frames[rt.frame_idx[d]].value("data_file", ""),
                                       rt.frame_off[d], tile_span, &scratch)) {
          t_errd[ti_s] = static_cast<int>(ErrorDomain::IO);
          t_err[ti_s] = "corrected bin read failed (tile " + std::to_string(tip) +
                        " frame slot " + std::to_string(d) + ")";
          return;
        }
        std::memcpy(frame_major.data() + d * tile_span, scratch.data(),
                    tile_span * sizeof(double));
      }
    }
    // 各帧该 tile 的 support 平面（几何 footprint 覆盖指示: support>0 = 覆盖）;
    // 逐像素几何 n 由此计数（与掩膜/资格无关）。
    std::vector<std::vector<float>> sup_tile(depth);
    for (size_t d = 0; d < depth; ++d) {
      std::vector<float> sb(static_cast<size_t>(kP2TileLeafSpan), 0.0f);
      AioHipsDataset* sds = rd.get(rt.frame_idx[d]);
      if (!sds || aio_hips_read_tile_f32(sds, tip, sb.data()) != 0) {
        t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
        t_err[ti_s] = "frame support tile read failed (tile " +
                      std::to_string(tip) + " frame slot " +
                      std::to_string(d) + ")";
        return;
      }
      sup_tile[d] = std::move(sb);
    }
    const uint64_t base = rt.out_off;
    // [F-P2-002-02 / B2-A3] 该 tile 的逐样本掩码块（[s*tile_span+p], 帧 slot 序
    // 与 rt.frame_idx 同序）。默认 0 = 未入栈/未接受; kernel 逐样本 reason 只
    // 映射到 eligible 样本的原始 slot（src_idx 权威, compact→original）。
    std::vector<uint8_t> tile_mask(
        static_cast<size_t>(depth) * static_cast<size_t>(tile_span), 0);
    uint64_t l_acc_total = 0, l_rej_low = 0, l_rej_high = 0, l_rej_samp = 0;
    uint64_t l_undet_low = 0, l_undet_tot = 0;
    std::vector<double> compact_vals;  // kernel 候选栈（工作缓冲）
    std::vector<uint32_t> src_idx;
    std::vector<uint8_t> reasons;
    for (uint64_t p = 0; p < tile_span; ++p) {
      // 该输出像素的几何 n = 覆盖它的帧 footprint 数（support>0; 与掩膜/资格
      // 无关）。plan 按几何 n 取（纯函数缓存）。
      std::uint32_t geom_n = 0;
      for (size_t d = 0; d < depth; ++d) {
        const float sv = sup_tile[d][static_cast<size_t>(p)];
        if (std::isfinite(sv) && sv > 0.0f) ++geom_n;
      }
      const P2RejectionPlan& plan = plan_cache.at(geom_n);
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
      if (grc != 0) {
        t_errd[ti_s] = static_cast<int>(ErrorDomain::INTERNAL);
        t_err[ti_s] = std::string("p2_collect_candidate_stack failed rc=") +
                      std::to_string(grc);
        return;
      }
      uint8_t acc = 1;
      uint16_t nrej = 0;
      const uint16_t cand = static_cast<uint16_t>(eligible_count);
      // 门 = **逐像素 plan** 的同式判定（几何 n 解析, 资格数判门）
      if (eligible_count > 0 &&
          eligible_count > plan.underdetermined_n &&
          eligible_count >= static_cast<uint32_t>(plan.minimum_n)) {
        P2CandidateStack stack{};
        stack.values = compact_vals.data();
        stack.weights = nullptr;
        stack.frame_ids = nullptr;
        stack.count = eligible_count;
        stack.data_type = 1;
        // SD-18（2026-09-18）：生产路径**不做逐像素 31×31 先验计算**（原
        // n=2 extreme_prior 档, 粗估 1000-1200s 单线程）。低 n（n<=3）走保守
        // none：不排异 + 直接加权积分。prior_sigma/prior_sky 保持 nullptr
        // （仅显式 opt-in 先验档才由调用方提供, 见 p2_reject_local_prior）。
        P2RejectionDecision dec{};
        reasons.assign(eligible_count, 0);
        dec.reasons = reasons.data();
        const int krc = p2_reject_stack_ex(&stack, &plan, &dec);
        if (krc != 0) {
          t_errd[ti_s] = static_cast<int>(ErrorDomain::INTERNAL);
          t_err[ti_s] = std::string("p2_reject_stack_ex failed rc=") +
                        std::to_string(krc);
          return;
        }
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
          else ++l_rej_samp;
          const uint32_t slot_s = src_idx[s];
          if (slot_s < depth)
            tile_mask[static_cast<size_t>(slot_s) * tile_span + p] = ok_s ? 1 : 0;
        }
        nrej = static_cast<uint16_t>(dec.rejected_low + dec.rejected_high);
        l_rej_low += dec.rejected_low;
        l_rej_high += dec.rejected_high;
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
      // 该像素**未做排异**的两种合法来源（如实计数, 不冒充排异成功）：
      // ① 决策点路由到 none（EXP-204 保守档：几何 N ≤ 3）；
      // ② kernel underdetermined 闸（候选数 ≤ plan.underdetermined_n；WBPP 表
      //    下几何 N ≤ 3 走此路：路由记 percentile，实际全接受 + UNDERDETERMINED）。
      // 判据只用 plan 字段（决策点/闸的唯一来源），不在此复制常量。
      // （void 无候选像素不计, 它们由 candidates=0 表达。）
      if (eligible_count > 0 &&
          (plan.method == P2_REJECT_NONE ||
           eligible_count <= plan.underdetermined_n))
        ++l_undet_low;
      accepted_bin[base + p] = acc;
      nrej_bin[base + p] = nrej;
      cand_u16[base + p] = cand;
      l_acc_total += acc;
      if (cand > 0 && (cand <= plan.underdetermined_n ||
                       cand < static_cast<std::uint32_t>(plan.minimum_n))) ++l_undet_tot;
    }
    std::memcpy(sample_mask.data() + rt.mask_off, tile_mask.data(), tile_mask.size());
    t_acc[ti_s] = l_acc_total;
    t_rejlow[ti_s] = l_rej_low;
    t_rejhigh[ti_s] = l_rej_high;
    t_rejsamp[ti_s] = l_rej_samp;
    t_undetlow[ti_s] = l_undet_low;
    t_undettot[ti_s] = l_undet_tot;
  });

  // tile 升序取首个失败（= 串行首个失败）; 整数计数按 tile 序合并（精确）。
  for (size_t i = 0; i < n_tiles; ++i)
    if (!t_err[i].empty())
      return Result<void>::fail(Error(static_cast<ErrorDomain>(t_errd[i]), t_err[i]));
  uint64_t acc_total = 0, rej_low_total = 0, rej_high_total = 0, undet_total = 0;
  uint64_t rej_samples_total = 0, undet_low_n_pixels = 0;
  // opt-in 先验路径已移出生产；保留计数恒 0（artifact schema 稳定）。
  const uint64_t prior_unavailable_pixels = 0;
  const uint64_t n_pixels_processed = static_cast<uint64_t>(n_tiles) * tile_span;
  Json tiles_j = Json::array();
  for (size_t i = 0; i < n_tiles; ++i) {
    const RejTile& rt = rtiles[i];
    std::vector<uint32_t> frame_slots(rt.depth, 0);
    for (size_t d = 0; d < rt.depth; ++d)
      frame_slots[d] = static_cast<uint32_t>(rt.frame_idx[d]);
    // [F-P2-002-02 / B2-A3] tile 记录承载逐样本掩码定位三键: depth（该 tile
    // 覆盖帧数）、frame_slots（掩码 slot d ↔ corrected 帧索引）、
    // sample_mask_offset（掩码块在 p2_rejection_sample_mask.bin 的字节偏移）。
    tiles_j.push_back(Json{{"tile_ipix", rt.tip},
                           {"n_pixels", tile_span},
                           {"offset", rt.out_off},
                           {"depth", rt.depth},
                           {"frame_slots", frame_slots},
                           {"sample_mask_offset", rt.mask_off}});
    acc_total += t_acc[i];
    rej_low_total += t_rejlow[i];
    rej_high_total += t_rejhigh[i];
    rej_samples_total += t_rejsamp[i];
    undet_low_n_pixels += t_undetlow[i];
    undet_total += t_undettot[i];
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

  // provenance: 逐几何 n 的 plan（method/semantic_id/minimum_n/underdetermined_n/
  // normalization/nominal_n）**+ 实际参数 + 合法性窗口告警码**。
  // 实际使用的方法/参数/N 必须写入排异 provenance（可追溯）；
  // §5：显式指定的合法性窗口（WBPP :1229-1293）只告警不硬阻断 ⇒ 告警码随
  // provenance 落盘（不静默、不改算法、不降级）。
  Json plans_j = Json::array();
  for (const auto& kv : plan_cache) {
    const P2RejectionPlan& p = kv.second;
    char warn_code[64] = {0};
    const bool has_warn =
        p2_rejection_applicability(p.method, kv.first, warn_code,
                                   sizeof(warn_code)) != 0;
    Json params = Json::object();
    switch (p.method) {
      case P2_REJECT_PERCENTILE:
        params = Json{{"low_fraction", p.percentile.low_fraction},
                      {"high_fraction", p.percentile.high_fraction}};
        break;
      case P2_REJECT_WINSORIZED_SIGMA:
        params = Json{{"lower_sigma", p.winsorized.lower_sigma},
                      {"upper_sigma", p.winsorized.upper_sigma},
                      {"max_iterations", p.winsorized.max_iterations}};
        break;
      case P2_REJECT_LINEAR_FIT:
        params = Json{{"lower", p.linear_fit.lower},
                      {"upper", p.linear_fit.upper},
                      {"max_iterations", p.linear_fit.max_iterations}};
        break;
      default:
        // 其它方法（显式 opt-in 先验档 / EXP-204 保守 none 档）参数不在
        // AUTO 映射值域内；method + nominal_n 已足够追溯。
        break;
    }
    plans_j.push_back(Json{{"nominal_n", kv.first},
                           {"method", p.method},
                           {"semantic_id", p2_rejection_semantic_id(p.method)},
                           {"minimum_n", p.minimum_n},
                           {"underdetermined_n", p.underdetermined_n},
                           {"normalization", p.normalization},
                           {"params", params},
                           {"applicability_warn",
                            has_warn ? std::string(warn_code) : std::string()}});
  }
  const P2RejectionPlan& plan_max = plan_cache.at(n_max);
  const std::string fallback_token =
      prior_unavailable_pixels > 0 ? "prior_sigma_unavailable" : "none";
  const std::string out_path = out_dir + "/p2_rejection.json";
  Json artifact = Json{{"schema", "DATA-P2-REJ"},
                       {"entry", "p2_reject_plan_resolve_n/p2_collect_candidate_stack/p2_reject_stack_ex"},
                       {"profile", profile},
                       // 低 n（几何 n<=3）实际不排异 = 不排异 + 加权积分。
                       {"low_n_policy", "underdetermined_no_rejection"},
                       {"low_n_max_n", 3},
                       // 小 N 策略（**唯一决策点**在 rejection.cpp 的
                       // kPixelSmallNPolicy；EXP-204 待定案）。路由按 WBPP 一手
                       // 实测表（N<6 → percentile，含 N≤3）；N ≤ 3 的**实际
                       // 执行**由 kernel underdetermined 闸决定（候选 ≤
                       // underdetermined_n ⇒ 全接受 + UNDERDETERMINED）。
                       {"small_n_policy",
                        Json{{"decision_point",
                              "lib/algorithms/coverage/src/rejection.cpp:"
                              "kPixelSmallNPolicy"},
                             {"exp_task", "EXP-204"},
                             {"percentile_band_min_n",
                              p2_rejection_percentile_band_min_n()},
                             {"routing",
                              "N<6 percentile / 6..15 winsorized_sigma / "
                              ">15 linear_fit (WBPP BPP-FrameGroup.js:1304-1312)"},
                             {"low_n_max_n", 3},
                             {"low_n_effect",
                              "underdetermined_gate_no_rejection"},
                             // 闸触发条件与原因（**显式可见，非静默降级**）：
                             // kernel 对候选数 ≤ underdetermined_n 的栈不做
                             // 排异判定，全部接受并返回 P2_STATUS_UNDERDETERMINED。
                             {"gate_trigger",
                              "eligible_count <= plan.underdetermined_n"},
                             {"gate_reason",
                              "P2_STATUS_UNDERDETERMINED: candidates "
                              "insufficient for a reliable rejection decision; "
                              "all accepted and recorded (never silent)"}}},
                       // plan = 最大几何 n 档（确定性摘要; 逐 n 全表见 plans）
                       {"plan", Json{{"nominal_n", n_max},
                                     {"method", plan_max.method},
                                     {"semantic_id", p2_rejection_semantic_id(plan_max.method)},
                                     {"minimum_n", plan_max.minimum_n},
                                     {"underdetermined_n", plan_max.underdetermined_n},
                                     {"normalization", plan_max.normalization},
                                     {"fallback", fallback_token}}},
                       {"plans", plans_j},
                       {"geometric_n_source", "frame_support_gt0"},
                       {"prior_unavailable_pixels", prior_unavailable_pixels},
                       {"tile_leaf_span", tile_span},
                       {"n_pixels", n_pixels_processed},
                       {"tiles", tiles_j},
                       {"stats", Json{{"accepted_pixels", acc_total},
                                      {"rejected_low", rej_low_total},
                                      {"rejected_high", rej_high_total},
                                      {"rejected_samples", rej_samples_total},
                                      // SD-18：因几何 n<=3 未做排异的像素数
                                      // （method=NONE, 不排异 + 加权积分）。
                                      {"underdetermined_pixels", undet_low_n_pixels},
                                      // 资格门欠定（eligible<=undet_n 或 <minimum_n）
                                      // 的像素数，独立于低 n 档。
                                      {"underdetermined_gate_pixels", undet_total}}},
                       {"files", Json{{"accepted", acc_file},
                                      {"nrej", nrej_file},
                                      {"candidates", cand_file},
                                      {"sample_mask", mask_file}}}};
  if (!p2_write_text(out_path, artifact.dump(2)))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed: " + out_path));
  (*man)["artifacts"] = Json::array({out_path, acc_file, nrej_file, cand_file, mask_file});
  (*man)["rejection_artifact"] = out_path;
  (*man)["sample_mask"] = mask_file;
  (*man)["reject_semantic_id"] = p2_rejection_semantic_id(plan_max.method);
  (*man)["reject_profile"] = profile;
  (*man)["reject_geometric_n_source"] = "frame_support_gt0";
  (*man)["reject_low_n_policy"] = "underdetermined_no_rejection";
  (*man)["reject_percentile_band_min_n"] = p2_rejection_percentile_band_min_n();
  (*man)["reject_underdetermined_pixels"] = undet_low_n_pixels;
  (*man)["reject_prior_unavailable_pixels"] = prior_unavailable_pixels;
  (*man)["n_pixels"] = n_pixels_processed;
  return Result<void>::success();
}

// HiPS properties 文本（"KEY=value\n"）浮点键解析（帧级 SNR 读取）。
static bool p2_hips_prop_double(AioHipsDataset* ds, const char* key, double* out) {
  if (!ds || !key || !out) return false;
  std::vector<char> buf(1 << 16, 0);
  if (aio_hips_get_properties(ds, buf.data(), static_cast<int>(buf.size())) != 0)
    return false;
  const std::string text(buf.data());
  const std::string k = std::string(key) + "=";
  size_t pos = 0;
  while (pos < text.size()) {
    size_t eol = text.find('\n', pos);
    if (eol == std::string::npos) eol = text.size();
    const std::string line = text.substr(pos, eol - pos);
    pos = eol + 1;
    if (line.compare(0, k.size(), k) == 0) {
      try {
        *out = std::stod(line.substr(k.size()));
      } catch (...) {
        return false;
      }
      return std::isfinite(*out);
    }
  }
  return false;
}

// ── op: integrate_frames（唯一真实入口 p2_validate_candidate_weights +
//      p2_integrate_pixel; 权重面 = DATA-UNC-001 §30.1 目标态合同 +
//      §9.73 裁决 A44 的**单一权重口径**（ASTROCS_DESIGN.md §3.1:175
//      「权重的产生链固定为两步、没有可选择项」；PSF_SIGNAL_WEIGHT.md §4）:
//      逐样本 ivar 逆方差。ivar 产品缺失 → 不再等权降级:
//      由 HiPS 头帧级 SNR 现场换算逆方差权重（w = SNR²/F_ref² = 1/σ_F²,
//      weight-chain-report §6）; 权重链未闭合 → DATA 错误 + closure token
//      （legacy_allow_weight_fallback 已按 §9.73 A44 删除：出现即拒绝）。
//      原 weight_mode∈{1,2} 整数域已删除：该键出现即 fail-closed 拒绝。
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

  // ── §9.73 裁决 A44：legacy 整数权重模式域（0/1/2）已删除 ─────────────────
  // 规范依据（权威，只读）：
  //   · ASTROCS_DESIGN.md §3.1:171「全程只有 SNR，没有"权重模式"这个概念」；
  //   · ASTROCS_DESIGN.md §3.1:175「权重的产生链固定为两步、**没有可选择项**」；
  //   · docs/science/PSF_SIGNAL_WEIGHT.md §4:62/72「单一权重口径（无模式选择）」
  //     「**没有可选择的口径**：不存在口径选择键、口径枚举、口径配置项或口径产物」；
  //   · docs/ci/01_CHECKS.md CHK-NO-WEIGHT-MODE-CODE（FZ-WEIGHT-SINGLE-PATH）。
  // 原实现读整数 doc["weight_mode"]∈{1,2}：1=equal（等权、unit_weight_mode1）、
  // 2=ivar。equal 是一个**可选择的非逆方差口径**，与「没有可选择项」直接冲突。
  // ⇒ 该键既不能被设、也不能被读：出现即 fail-closed 拒绝（退役对象的拒绝面
  //   必须存活，不得静默忽略）。唯一权重口径 = 逐样本 ivar 逆方差；ivar 缺失时
  //   走帧级 SNR 逆方差链（w = SNR²/F_ref²），权重链未闭合则 DATA 错误。
  if (doc.contains("weight_mode"))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "weight_mode 已按 §9.73 裁决 A44 删除：不存在「权重模式」"
        "（ASTROCS_DESIGN.md §3.1:175「权重的产生链固定为两步、没有可选择项」；"
        "docs/science/PSF_SIGNAL_WEIGHT.md §4「单一权重口径（无模式选择）」）。"
        "权重是阶段二按天球像素对应的输入帧集合现场算出的派生量 "
        "w = SNR^2/F_ref^2 = 1/sigma_F^2；请删除该键。"));
  // §9.73 裁决 A44（同批清理）：legacy_allow_weight_fallback **已删除**。
  // 该键曾允许「ivar 缺失 → 降级 support/equal」；support 是无量纲几何量、equal 是等权，
  // 二者都不是信号/噪声之比（ASTROCS_DESIGN.md §3.1:173/175）⇒ 既不能被设、也不能被读，
  // 出现即 fail-closed 具名拒绝。唯一降级面 = 帧级 SNR 逆方差链 w = SNR^2/F_ref^2，
  // 由数据可用性自动决定（不是用户开关）；权重链未闭合即 DATA 错误。
  if (doc.contains("legacy_allow_weight_fallback"))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "legacy_allow_weight_fallback 已按 §9.73 裁决 A44 删除：它允许用无量纲 "
        "support 或等权降级冒充逆方差权重，与 ASTROCS_DESIGN.md §3.1:173「权重只能"
        "来自纯净信号与噪声之比」及 §3.1:175「没有可选择项」冲突。唯一降级面 = "
        "帧级 SNR 逆方差链 w = SNR^2/F_ref^2；请删除该键。"));

  // ── RELEASE-02 P2b-2: 优先消费归一化逐像素方差 w = 1/Var(corrected) ──────
  // p2_corrected.json 报 uncertainty_available=true（方差完整传播：残差制造者
  // PΣPᵀ + 逐像素 Phase1 噪声 + 参数协方差项）时，权重面**优先**用逐像素
  // 1/Var(corrected)（weight_chain.h: weight_from_corrected_variance）；否则
  // 退回既有 ivar 面 / 帧级 SNR 链（后者须组内公共 F_ref 与 g_k 配对）。
  // 过渡期（方差未完整传播）不得声称逆方差加权（P2b-5）。
  std::vector<std::string> corr_var_file(frames.size());
  bool corr_var_ready = false;
  {
    const bool cor_var_avail = cor_doc.value("variance_available", false);
    const bool cor_unc_avail = cor_doc.value("uncertainty_available", false);
    bool all_files = cor_var_avail && cor_unc_avail;
    for (size_t f = 0; f < frames.size() && all_files; ++f) {
      corr_var_file[f] = frames[f].value("var_file", std::string());
      if (corr_var_file[f].empty()) all_files = false;
    }
    corr_var_ready = all_files;
  }

  // ivar 产品读取（weight_mode=2 必须）。ivar 缺失 → **不再等权降级**:
  // 由 HiPS 头帧级 SNR 现场换算逆方差权重（w = SNR²/F_ref² = 1/σ_F²;
  // weight-chain-report §6.1）; 权重链未闭合 → DATA 错误 + closure token。
  // 审计面（§30.1 + §20.1）: 逐帧记录 ivar 可用性/缺失帧索引/同帧 variance
  // 存在性（Phase1 产品面事实）, 写入 p2_integrated.json 与节点 manifest。
  struct IvarSet {
    AioHipsDataset* ds = nullptr;
    std::string path;
    bool variance_present = false;   // 仅审计事实; variance 不替代 ivar
  };
  std::vector<IvarSet> ivar(frames.size());
  std::vector<uint64_t> ivar_missing_frames;
  uint64_t variance_present_frames = 0;
  bool uncertainty_available = false;
  bool fallback = false;   // legacy 等权降级成功路径已删除（恒 false）
  std::string weight_basis = "per_sample_ivar";   // §30.1: w_i = 逐样本 ivar
  std::string weight_source = "none";             // weight-chain-report §6.1.4
  bool use_snr_chain = false;                     // ivar 缺失时走 SNR 权重链
  std::vector<double> snr_weights;                // 逐帧 w = 1/σ_F² [ADU^-2]
  std::string snr_chain_closure = "not_used";
  // 组间 F_ref 一致性：**报告字段，非门**（负责人 GAP_AUDIT §9.49 定案 2：
  // 帧间独立；配对性只要求同帧内 SNR 与 F_ref 同源，不要求跨帧相等）。
  double ref_flux_spread_rel = 0.0;
  uint64_t ref_flux_spread_frame = 0;
  bool ref_flux_spread_noncommon = false;
  // CONFORM-FIX-B-004: uncertainty_available=false 的显式原因（§30.1
  // 「diagnostics 标红计数」面）。空串 = 未降级（真值见 uncertainty_available）。
  std::string uncertainty_unavailable_reason;
  if (corr_var_ready) {
    // P2b-2 priority 1：逐像素归一化方差面（唯一科学正确的权重来源）。
    weight_basis = "per_pixel_corrected_variance";
    weight_source = "corrected_variance";
    uncertainty_available = true;
  }
  if (!corr_var_ready) {
    uint64_t ivar_missing = 0;
    for (size_t f = 0; f < frames.size(); ++f) {
      const std::string p = frames[f].value("hips_path", "");
      ivar[f].ds = aio_hips_open(p.c_str(), AIO_HIPS_RD_IVAR);
      ivar[f].path = p;
      if (!ivar[f].ds) {
        ++ivar_missing;
        ivar_missing_frames.push_back(static_cast<uint64_t>(f));
        // 同帧 variance/ 存在性只作审计上报: mode 2 读端按 §20.1 打开
        // AIO_HIPS_RD_IVAR，ivar 产品面必须显式存在（禁静默替代）。
        AioHipsDataset* vd = aio_hips_open(p.c_str(), AIO_HIPS_RD_VARIANCE);
        if (vd) {
          ivar[f].variance_present = true;
          ++variance_present_frames;
          aio_hips_close(vd);
        }
      }
    }
    if (ivar_missing > 0) {
      // ── 权重链接线（weight-chain-report §6）: HiPS 头帧级 SNR → 逆方差 ──
      // 键 ASTROCS_FRAME_SNR（通量型 F_ref/σ_F）/ ASTROCS_REFERENCE_FLUX
      // （组内公共 F_ref）; 键名与 Phase1 写入端尚未冻结（报告未闭合项）。
      // 任一帧缺键 → 权重链 fail-closed（不静默退化为等权）。
      for (auto& iv : ivar) { if (iv.ds) { aio_hips_close(iv.ds); iv.ds = nullptr; } }
      using astrocs::v6::p2weight::FrameWeightInput;
      using astrocs::v6::p2weight::FrameSnrKind;
      using astrocs::v6::p2weight::WeightChainResult;
      std::vector<FrameWeightInput> winputs(frames.size());
      // P2b-2: 归一化含乘性 /g_k 时，帧级链须 w = SNR²/F_ref²·g_k²
      // （weight_chain FrameWeightInput.gain 可空；缺一 fail-closed）。
      // 生产方案 B（加性-only, g≡1）不置 multiplicative_gain_applied ⇒
      // gain=nullptr、require_frame_gain=false，行为与旧口径逐位一致。
      const bool require_gain = cor_doc.value("multiplicative_gain_applied", false);
      std::vector<double> frame_gain(frames.size(), 1.0);
      double ref_flux = 0.0;
      bool ref_flux_set = false;
      for (size_t f = 0; f < frames.size(); ++f) {
        const std::string p = frames[f].value("hips_path", "");
        AioHipsDataset* ds = aio_hips_open(p.c_str(), AIO_HIPS_RD_SIGNAL);
        double fsnr = 0.0, fref = 0.0;
        const bool has_snr = ds &&
            p2_hips_prop_double(ds, "ASTROCS_FRAME_SNR", &fsnr) && fsnr > 0.0;
        const bool has_ref = ds &&
            p2_hips_prop_double(ds, "ASTROCS_REFERENCE_FLUX", &fref) && fref > 0.0;
        if (ds) aio_hips_close(ds);
        FrameWeightInput& in = winputs[f];
        in.frame_id = std::to_string(frames[f].value("frame_id", 0ull));
        // 唯一合法语义: 帧级未加权原始通量型 SNR（质量权重/诊断量会被拒）。
        in.kind = FrameSnrKind::kFluxTypeUnweightedSnr;
        in.has_frame_snr = has_snr;
        in.frame_snr = fsnr;
        in.sparse = nullptr;   // 稀疏 SNR 层尚未接入生产数据面
        in.x = 0.0;
        in.y = 0.0;
        in.gain = nullptr;
        if (require_gain) {
          frame_gain[f] = frames[f].value("frame_gain", 1.0);
          in.gain = &frame_gain[f];
        }
        if (has_ref) {
          // ── 负责人 GAP_AUDIT §9.49 定案 2（帧间独立）──────────────
          // 旧实现把「组内 F_ref 必须逐帧相等」当 fail-closed 闸门。该要求
          // **科学上不成立**：配对性定理（WEIGHT-SCI-001）只要求**同一帧内**
          // SNR 与 F_ref 同源（w_k = SNR_k²/F_ref,k²），**不要求跨帧相等**。
          // FREF-BASELINE-001 的 scope="frame_independent_fixed_magnitude"
          // 下 F_ref,k = 10^(−0.4(m_ref−ZP_k))，ZP_k 依赖**该帧自己的**光学
          // 系统/滤镜 ⇒ 不同指向、不同光学系统的帧**合法地**有不同 F_ref,k。
          // 旧闸门等价于「不同光学系统的帧混装即报错」，与 §9.49 定案 2
          // 直接冲突，并使 weight_mode=2 在多指向拼接上完全不可用
          // （实测：6 帧跨 t2_m1/t2_m2 两块 ⇒ 6/6 被拒，链路永不闭合）。
          // ⇒ 逐帧用自己的 F_ref,k；跨帧一致性降级为**报告字段**
          //   （reference_flux_spread_*），不再是门。
          in.ref_flux = fref;
          if (!ref_flux_set) { ref_flux = fref; ref_flux_set = true; }
          const double rel = std::fabs(fref - ref_flux) / std::fabs(ref_flux);
          if (rel > ref_flux_spread_rel) {
            ref_flux_spread_rel = rel;
            ref_flux_spread_frame = static_cast<uint64_t>(frames[f].value("frame_id", 0ull));
          }
          if (rel > 1e-9) ref_flux_spread_noncommon = true;
        }
      }
      astrocs::v6::p2weight::WeightChainPolicy wpolicy;
      wpolicy.require_frame_gain = require_gain;
      const WeightChainResult wres =
          astrocs::v6::p2weight::compute_inverse_variance_weights(
              winputs, ref_flux_set ? ref_flux : 0.0, wpolicy);
      if (!wres.ok) {
        const std::string tok =
            std::string(astrocs::v6::p2weight::weight_closure_token(wres.closure));
        const std::string detail = wres.error;
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "weight_mode=2 requires per-frame ivar products; " +
            std::to_string(ivar_missing) + "/" + std::to_string(frames.size()) +
            " frames missing ivar; frame-SNR weight chain NOT closed (" + tok +
            "): " + detail + " (DATA-UNC-001 §30.1: no silent fallback; the legacy"
            " exit legacy_allow_weight_fallback=true is deleted per §9.73 A44 and"
            " no longer produces any equal-weight degradation)"));
      }
      use_snr_chain = true;
      snr_weights = wres.weights;
      weight_source = wres.weight_source;
      weight_basis = "frame_snr_ivar";
      snr_chain_closure = astrocs::v6::p2weight::weight_closure_token(wres.closure);
      // CONFORM-FIX-B-004（fail-closed，DATA_SEMANTICS §30.1 唯一出口）：
      // 帧级 SNR 链只是**积分权重**的显式降级路径，**不是**方差产品的来源。
      // §30.1 合成公式的前提是 ivar_product_missing==0（全部输入帧 ivar 可用、
      // 无 fallback），规则 2 明文：fallback 发生 ⇒ 不写 variance/ivar 子产品 +
      // uncertainty_available=false。旧实现在此置 true ⇒ 用帧级常数权合成
      // variance = F_ref²/Σ SNR² 落盘（不含任何逐像素噪声项，且 F_ref 正是
      // CONFORM-SWEEP-1-004 的缺陷量）⇒ 违反 fail-closed。
      // 处置：权重链保留（积分仍有权重、降级显式可见），但方差面 unavailable。
      uncertainty_available = false;
      uncertainty_unavailable_reason = "ivar_product_missing_frame_snr_fallback";
      std::fprintf(stderr,
                   ("[weight_chain] weight_mode=2: " + std::to_string(ivar_missing) +
                    "/" + std::to_string(frames.size()) +
                    " frames missing ivar -> HiPS frame-SNR inverse-variance"
                    " weights (source=" + weight_source + "; closure=" +
                    snr_chain_closure + "; legacy_allow_weight_fallback deleted"
                    " per §9.73 A44)\n")
                       .c_str());
    } else {
      uncertainty_available = true;
    }
  }
  // §9.73 裁决 A44：原 `else`（weight_mode==1 → 等权、unit_weight_mode1、
  // uncertainty_unavailable_reason="weight_mode_1_equal_non_ivar"）已删除 ——
  // 等权是**可选择的非逆方差口径**，与 ASTROCS_DESIGN.md §3.1:175「没有可选择项」
  // 直接冲突；唯一口径 = 逐样本 ivar，ivar 缺失走帧级 SNR 逆方差链（fail-closed）。
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
    uint64_t msz = 0;
    if (!aio_fs::file_size(smask_file, &msz) || msz == 0 ||
        !p2_read_bin_range<uint8_t>(smask_file, 0, msz, &sample_mask_all))
      return Result<void>::fail(Error(ErrorDomain::IO,
          "rejection sample mask read failed: " + smask_file));
  }

  // ── PERF-P2 S1.3 (RELEASE-02): tile 级并行 + sm_cursor 串行预检 ─────────
  // 唯一结构改动: 原循环内的 sample_mask 连续性校验（游标 sm_cursor）与
  // slot/depth 求解抽成**串行预检 pass**（O(n_tiles), 成本可忽略）—— 它必须在
  // 任何 tile 处理前按 tile 升序推进, 且失败语义/错误串与串行逐字一致。
  // 并行体只做「读本 tile 各帧数据 + 逐像素积分 + 写本 tile 固定 offset」;
  // 每 tile 的样本栈完全来自本 tile, 跨 tile 无浮点归约 ⇒ 逐位一致。
  // 每个 worker 使用独立的 support/ivar 句柄（cfitsio 同句柄并发读非线程安全）。
  struct IntTile {
    uint64_t tip = 0;
    uint64_t rej_off = 0;
    uint64_t sm_off = 0;
    size_t depth = 0;
    std::vector<size_t> slot;            // corrected 帧索引升序（cor_index 同源）
    std::vector<std::string> data_file;  // 每帧 corrected bin 路径
    std::vector<uint64_t> data_off;      // corrected bin 内元素偏移
  };
  const auto& rej_tiles = rej_doc["tiles"];
  if (!rej_tiles.is_array() || rej_tiles.empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "rejection artifact tiles invalid"));
  // [RELEASE-02 probe] 规模 gauge: 待积分 tile 数
  ASTROCS_PROBE_GAUGE("phase2", "integrate.tiles", static_cast<double>(rej_tiles.size()));
  std::vector<IntTile> itiles;
  itiles.reserve(rej_tiles.size());
  // corrected tile 查找索引（tile_ipix+frame → data_file（frame 级键）/offset）
  std::map<std::pair<uint64_t, uint64_t>, std::pair<std::string, uint64_t>> cor_index;
  for (size_t f = 0; f < frames.size(); ++f) {
    const std::string fdata = frames[f].value("data_file", "");
    for (const auto& t : frames[f]["tiles"]) {
      const uint64_t tip = t.value("tile_ipix", 0ull);
      cor_index[{tip, f}] = {fdata, t.value("offset", 0ull)};
    }
  }
  // 掩码块在文件中必须**按 tile 序连续无洞**（reject 以 union tile 升序拼接）:
  // 游标核对使任何 offset 错位/重叠/空洞立即被检出（禁信任可自洽的伪造 offset）。
  uint64_t sm_cursor = 0;
  for (const auto& rt : rej_tiles) {
    const uint64_t tip = rt.value("tile_ipix", 0ull);
    IntTile it;
    it.tip = tip;
    it.rej_off = rt.value("offset", 0ull);
    for (size_t f = 0; f < frames.size(); ++f) {
      const auto cit = cor_index.find({tip, f});
      if (cit == cor_index.end()) continue;   // 该帧无此 tile → 不入栈
      it.slot.push_back(f);
      it.data_file.push_back(cit->second.first);
      it.data_off.push_back(cit->second.second);
    }
    const uint64_t depth = it.slot.size();
    it.depth = static_cast<size_t>(depth);
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
      if (fs != static_cast<uint64_t>(it.slot[d]))
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "rejection sample mask frame-slot order mismatch (tile " +
            std::to_string(tip) + " d=" + std::to_string(d) +
            " reject_slot=" + std::to_string(fs) + " integrate_slot=" +
            std::to_string(it.slot[d]) + ")"));
    }
    it.sm_off = sm_off;
    sm_cursor += depth * tile_span;
    itiles.push_back(std::move(it));
  }
  const size_t n_tiles = itiles.size();
  const uint32_t workers = std::max(1u, doc.value("__workers", 1u));
  const bool need_ivar = (!fallback && !use_snr_chain && !corr_var_ready);
  std::vector<double> sig_bin(n_tiles * static_cast<size_t>(tile_span));
  std::vector<double> sup_bin(n_tiles * static_cast<size_t>(tile_span));
  std::vector<double> wsum_bin(n_tiles * static_cast<size_t>(tile_span));
  std::vector<int32_t> nused_bin(n_tiles * static_cast<size_t>(tile_span));
  std::vector<int32_t> nrej_plane(n_tiles * static_cast<size_t>(tile_span));
  std::vector<uint64_t> t_zero(n_tiles, 0), t_invalid(n_tiles, 0);
  std::vector<uint64_t> t_nrej(n_tiles, 0), t_skip(n_tiles, 0);
  std::vector<std::string> t_err(n_tiles);
  std::vector<int> t_errd(n_tiles, 0);
  struct P2FrameReader {
    const std::vector<std::string>* paths = nullptr;
    std::vector<AioHipsDataset*> sup, ivar;
    AioHipsDataset* get_sup(size_t f) {
      if (sup.empty()) sup.assign(paths->size(), nullptr);
      if (!sup[f]) sup[f] = aio_hips_open((*paths)[f].c_str(), AIO_HIPS_RD_SUPPORT);
      return sup[f];
    }
    AioHipsDataset* get_ivar(size_t f) {
      if (ivar.empty()) ivar.assign(paths->size(), nullptr);
      if (!ivar[f]) ivar[f] = aio_hips_open((*paths)[f].c_str(), AIO_HIPS_RD_IVAR);
      return ivar[f];
    }
    ~P2FrameReader() {
      for (AioHipsDataset* d : sup) if (d) aio_hips_close(d);
      for (AioHipsDataset* d : ivar) if (d) aio_hips_close(d);
    }
  };
  std::vector<std::string> frame_paths(frames.size());
  for (size_t f = 0; f < frames.size(); ++f)
    frame_paths[f] = frames[f].value("hips_path", "");
  std::vector<std::unique_ptr<P2FrameReader>> readers(workers);
  for (uint32_t w = 0; w < workers; ++w) {
    readers[w] = std::unique_ptr<P2FrameReader>(new P2FrameReader());
    readers[w]->paths = &frame_paths;
  }
  p2_parallel_for(workers, static_cast<uint64_t>(n_tiles),
                  [&](uint64_t ti, uint32_t w) {
    const IntTile& it = itiles[static_cast<size_t>(ti)];
    P2FrameReader& rd = *readers[w];
    const size_t ti_s = static_cast<size_t>(ti);
    const uint64_t tip = it.tip;
    const uint64_t rej_off = it.rej_off;
    const uint64_t sm_off = it.sm_off;
    const uint64_t depth = it.depth;
    const uint64_t base = static_cast<uint64_t>(ti) * tile_span;
    // [RELEASE-02 probe] 逐 tile 热点: integrate
    ASTROCS_PROBE_SCOPE_CTX(_probe_int_tile, "phase2", "integrate.tile");
    ASTROCS_PROBE_TAG(_probe_int_tile, "tile_id", static_cast<unsigned long long>(tip));
    std::vector<float> ivar_buf(kP2TileLeafSpan), sup_buf(kP2TileLeafSpan);
    std::vector<double> vals, weights, supports;
    std::vector<uint8_t> accs;
    // per-frame corrected tile 独立缓冲（tile 生存期; 禁共享 static 缓冲）
    std::vector<std::vector<double>> tile_bufs;
    tile_bufs.reserve(static_cast<size_t>(depth));
    for (size_t d = 0; d < depth; ++d) {
      std::vector<double> buf;
      if (!p2_read_bin_range<double>(it.data_file[d], it.data_off[d],
                                     tile_span, &buf)) {
        t_errd[ti_s] = static_cast<int>(ErrorDomain::IO);
        t_err[ti_s] = "corrected bin read failed (tile " + std::to_string(tip) +
                      " frame " + std::to_string(it.slot[d]) + ")";
        return;
      }
      tile_bufs.push_back(std::move(buf));
    }
    // P2b-2: 逐像素归一化方差 tile（与 corrected tile 同布局/同 offset）
    std::vector<std::vector<double>> tile_cvar;
    if (corr_var_ready) {
      tile_cvar.reserve(static_cast<size_t>(depth));
      for (size_t d = 0; d < depth; ++d) {
        std::vector<double> buf;
        const std::string& vf = corr_var_file[it.slot[d]];
        if (vf.empty() ||
            !p2_read_bin_range<double>(vf, it.data_off[d], tile_span, &buf)) {
          t_errd[ti_s] = static_cast<int>(ErrorDomain::IO);
          t_err[ti_s] = "corrected variance bin read failed (tile " +
                        std::to_string(tip) + " frame " +
                        std::to_string(it.slot[d]) + ")";
          return;
        }
        tile_cvar.push_back(std::move(buf));
      }
    }
    std::vector<const std::vector<double>*> tile_v;
    tile_v.reserve(tile_bufs.size());
    for (const auto& b : tile_bufs) tile_v.push_back(&b);
    // 该 tile 各帧 support/ivar tile（read_tile_f32; 缺失 → 该像素零权/无支持）
    std::vector<bool> has_sup(depth, false), has_ivar(depth, false);
    std::vector<std::vector<float>> sup_v(static_cast<size_t>(depth));
    std::vector<std::vector<float>> ivar_v(static_cast<size_t>(depth));
    for (size_t d = 0; d < depth; ++d) {
      const size_t f = it.slot[d];
      AioHipsDataset* sds = rd.get_sup(f);
      if (sds && aio_hips_read_tile_f32(sds, tip, sup_buf.data()) == 0) {
        sup_v[d] = sup_buf;
        has_sup[d] = true;
      }
      if (need_ivar) {
        AioHipsDataset* ivds = rd.get_ivar(f);
        if (ivds && aio_hips_read_tile_f32(ivds, tip, ivar_buf.data()) == 0) {
          ivar_v[d] = ivar_buf;
          has_ivar[d] = true;
        }
      }
    }
    uint64_t l_zero = 0, l_invalid = 0, l_nrej = 0, l_skip = 0;
    for (uint64_t p = 0; p < tile_span; ++p) {
      vals.clear(); weights.clear(); supports.clear(); accs.clear();
      const uint64_t rej_pix = rej_off + p;
      if (rej_pix >= acc_all.size()) {
        t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
        t_err[ti_s] = "rejection plane index out of range (tile " +
                      std::to_string(tip) + ")";
        return;
      }
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
        if (sm > 1) {
          t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
          t_err[ti_s] = "rejection sample mask must be 0/1 (tile " +
                        std::to_string(tip) + " frame " +
                        std::to_string(it.slot[d]) + " pixel " +
                        std::to_string(p) + " value=" +
                        std::to_string(static_cast<int>(sm)) + ")";
          return;
        }
        // 调用方资格（SCI-INT §5 valid ∧ W>0 面）: finite ∧ support>0 ∧
        // 逐样本 accepted; 先资格过滤后权重面 —— 无覆盖像素（support=0 →
        // corrected NaN → 过滤）不进入权重检查（ivar 产品在无覆盖像素 =
        // NaN 同态, §30.1 表注 F-UNC-001）
        if (!std::isfinite(v) || !std::isfinite(sp) || sp <= 0.0 || !acc ||
            sm == 0) {
          if (sm == 0 && std::isfinite(v) && std::isfinite(sp) && sp > 0.0 && acc)
            ++l_skip;
          continue;
        }
        double w = 1.0;
        if (!fallback) {
          if (corr_var_ready) {
            // P2b-2 priority 1: w = 1/Var(corrected)（逐像素归一化方差）
            const double vv = tile_cvar[d][static_cast<size_t>(p)];
            if (!std::isfinite(vv) || !(vv > 0.0)) {
              t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
              t_err[ti_s] = "corrected variance invalid (non-finite/<=0) at frame " +
                            std::to_string(it.slot[d]) + " tile " +
                            std::to_string(tip) + " pixel " + std::to_string(p) +
                            " (P2b-2: w=1/Var(corrected) fail-closed, no clamp)";
              return;
            }
            std::string verr;
            if (!astrocs::v6::p2weight::weight_from_corrected_variance(vv, &w,
                                                                       &verr)) {
              t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
              t_err[ti_s] = "weight_from_corrected_variance failed at frame " +
                            std::to_string(it.slot[d]) + ": " + verr;
              return;
            }
          } else if (use_snr_chain) {
            // ivar 产品缺失 → 帧级 SNR 逆方差权重（w = SNR²/F_ref² = 1/σ_F²,
            // 逐帧常量; weight-chain-report §6.1）
            if (it.slot[d] >= snr_weights.size()) {
              t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
              t_err[ti_s] = "frame-SNR weight index out of range (frame " +
                            std::to_string(it.slot[d]) + ")";
              return;
            }
            w = snr_weights[it.slot[d]];
            if (!std::isfinite(w) || !(w > 0.0)) {
              t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
              t_err[ti_s] = "frame-SNR weight invalid (non-finite/<=0) at frame " +
                            std::to_string(it.slot[d]) + " (frame-SNR weight chain)";
              return;
            }
          } else {
            // 入栈样本的 ivar 契约检查（§20.1 读侧: ivar==0 合法零权重,
            // nonfinite/负 = 产品损坏 hard fail, 禁 clamp/skip）
            if (!has_ivar[d]) {
              t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
              t_err[ti_s] = "ivar tile read failed where corrected data exists (frame " +
                            std::to_string(it.slot[d]) + " tile " +
                            std::to_string(tip) + ")";
              return;
            }
            w = static_cast<double>(ivar_v[d][static_cast<size_t>(p)]);
            if (!std::isfinite(w) || w < 0.0) {
              t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
              t_err[ti_s] = "non-finite/negative input ivar at frame " +
                            std::to_string(it.slot[d]) + " tile " +
                            std::to_string(tip) + " pixel " + std::to_string(p) +
                            " (DATA-UNC-001 §30.1: p2_validate_candidate_weights hard"
                            " fail, no clamp/no skip)";
              return;
            }
          }
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
      if (irc != 0) {
        t_errd[ti_s] = static_cast<int>(ErrorDomain::INTERNAL);
        t_err[ti_s] = std::string("p2_integrate_pixel failed rc=") +
                      std::to_string(irc);
        return;
      }
      // 权重资格守卫（构建后 p2_validate_candidate_weights; 合同要求）
      if (p2_validate_candidate_weights(weights.empty() ? nullptr : weights.data(),
                                        static_cast<std::uint32_t>(weights.size())) != 0) {
        t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
        t_err[ti_s] = "candidate weights validation failed (tile " +
                      std::to_string(tip) + " pixel " + std::to_string(p) + ")";
        return;
      }
      double wsum = 0.0;
      for (size_t i = 0; i < vals.size(); ++i) {
        if (weights[i] > 0.0 && std::isfinite(weights[i])) wsum += weights[i];
      }
      double signal = pr.signal;
      if (pr.status == P2_INTEGRATE_OK) {
        if (!std::isfinite(wsum) || wsum <= 0.0) {
          signal = std::numeric_limits<double>::quiet_NaN();   // 病态 → NaN
          wsum = std::numeric_limits<double>::quiet_NaN();
          ++l_invalid;
        }
      } else {
        // signal 与 W 的口径**分开**（§30.1：ivar = W = Σw）：
        //   ZERO_VALID_WEIGHT = 样本在、**权重全 0**（=「有覆盖但方差不可用」，
        //     §4a:49 明文合法产品态）⇒ signal 无定义（NaN），但 W **就是权重和 = 0**，
        //     不得写成 NaN——写成 NaN 会让下游把「方差不可用」误判成「权重面损坏」
        //     （tile 写出侧对 cov>0 ∧ 非有限 W 判 rc=-6 硬失败）。
        //   其余（NO_CANDIDATES / ALL_REJECTED / INVALID_INPUT）= 无有效样本
        //     ⇒ NaN/NaN 同态（§30.1 invalid policy 第 1 行, 禁 0/±Inf 伪装）。
        signal = std::numeric_limits<double>::quiet_NaN();
        if (pr.status == P2_INTEGRATE_ZERO_VALID_WEIGHT) {
          wsum = 0.0;
          ++l_zero;
        } else {
          wsum = std::numeric_limits<double>::quiet_NaN();
        }
      }
      sig_bin[base + p] = signal;
      sup_bin[base + p] = pr.support;   // canonical reducer max(accepted support)
      wsum_bin[base + p] = wsum;        // ivar_mosaic（§30.1: ivar = W）
      nused_bin[base + p] = static_cast<int32_t>(pr.n_used);
      const int32_t nrej_p = static_cast<int32_t>(nrej_all[static_cast<size_t>(rej_pix)]);
      nrej_plane[base + p] = nrej_p;
      l_nrej += static_cast<uint64_t>(nrej_p);
    }
    t_zero[ti_s] = l_zero;
    t_invalid[ti_s] = l_invalid;
    t_nrej[ti_s] = l_nrej;
    t_skip[ti_s] = l_skip;
  });

  // tile 升序取首个失败（= 串行首个失败）; 整数计数按 tile 序合并（精确）。
  for (size_t i = 0; i < n_tiles; ++i)
    if (!t_err[i].empty())
      return Result<void>::fail(Error(static_cast<ErrorDomain>(t_errd[i]), t_err[i]));
  uint64_t zero_weight_pixels = 0, invalid_pixels = 0, nrej_total = 0;
  uint64_t sample_rejected_skipped = 0;   // 因 kernel 逐样本拒绝而剔除的样本实例数
  const uint64_t nrej_pix_cursor = static_cast<uint64_t>(n_tiles) * tile_span;
  Json tiles_j = Json::array();
  for (size_t i = 0; i < n_tiles; ++i) {
    tiles_j.push_back(Json{{"tile_ipix", itiles[i].tip},
                           {"n_pixels", tile_span},
                           {"offset", static_cast<uint64_t>(i) * tile_span}});
    zero_weight_pixels += t_zero[i];
    invalid_pixels += t_invalid[i];
    nrej_total += t_nrej[i];
    sample_rejected_skipped += t_skip[i];
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
  Json missing_j = Json::array();
  for (uint64_t mf : ivar_missing_frames) missing_j.push_back(mf);
  // G3-12（ASTROCS_DESIGN §3.1「全程只有 SNR，不存在『权重模式』」）:
  // 产品面**不再落** weight_mode 键。方差面状态由 corrected_variance_used /
  // snr_chain_used / uncertainty_available 三个语义键如实承载（下方均在册）。
  Json artifact = Json{{"schema", "DATA-P2-INT"},
                       {"entry", "p2_validate_candidate_weights/p2_integrate_pixel"},
                       {"weight_basis", weight_basis},
                       {"weight_source", weight_source},
                       {"corrected_variance_used", corr_var_ready},
                       {"snr_chain_closure", snr_chain_closure},
                       {"snr_chain_used", use_snr_chain},
                       // 组间 F_ref 一致性 = **报告字段，非门**（GAP_AUDIT §9.49
                       // 定案 2）。逐帧 F_ref,k 合法地可不同（不同指向/不同光学
                       // 系统 ⇒ 不同 ZP_k）；配对性只要求同帧内同源。
                       {"reference_flux_spread_rel", ref_flux_spread_rel},
                       {"reference_flux_spread_frame", ref_flux_spread_frame},
                       {"reference_flux_noncommon", ref_flux_spread_noncommon},
                       {"reference_flux_gate",
                        "none (owner ruling 9.49: frame-independent; pairing is "
                        "per-frame SNR_k^2/F_ref,k^2)"},
                       {"fallback", fallback},
                       {"ivar_product_missing_frames", missing_j.size()},
                       {"ivar_product_missing_frame_indices", missing_j},
                       {"variance_product_present_frames", variance_present_frames},
                       {"uncertainty_available", uncertainty_available},
                       {"uncertainty_unavailable_reason",
                        uncertainty_unavailable_reason},
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
  (*man)["weight_basis"] = weight_basis;
  (*man)["weight_source"] = weight_source;
  (*man)["corrected_variance_used"] = corr_var_ready;
  (*man)["snr_chain_closure"] = snr_chain_closure;
  (*man)["snr_chain_used"] = use_snr_chain;
  (*man)["reference_flux_spread_rel"] = ref_flux_spread_rel;
  (*man)["reference_flux_spread_frame"] = ref_flux_spread_frame;
  (*man)["reference_flux_noncommon"] = ref_flux_spread_noncommon;
  (*man)["reference_flux_gate"] =
      "none (owner ruling 9.49: frame-independent; pairing is per-frame "
      "SNR_k^2/F_ref,k^2)";
  (*man)["fallback"] = fallback;
  (*man)["ivar_product_missing_frames"] = static_cast<uint64_t>(missing_j.size());
  (*man)["variance_product_present_frames"] = variance_present_frames;
  (*man)["uncertainty_available"] = uncertainty_available;
  (*man)["uncertainty_unavailable_reason"] = uncertainty_unavailable_reason;
  return Result<void>::success();
}

// ══ (ASTROCS_DESIGN §10「I/O 与原子产品」/ GAP_AUDIT G3-1) ═══════════════════
// Phase2 mosaic 产品集: 运行私有暂存区 → 校验 → 统一原子发布。
// 暂存区 = output_dir 的**兄弟**路径 (同文件系统 ⇒ rename 不跨设备; 不在正式
// 目录内 ⇒ 正式目录永不出现半成品 tile), 词法与 aio_publish v1 的
// <parent>/.<base>.hips_staging.tmp 同族。
std::string p2_mosaic_staging_path(const std::string& out_dir) {
  static std::atomic<uint64_t> seq{0};
  const uint64_t s = seq.fetch_add(1, std::memory_order_relaxed);
  return out_dir + ".p2_mosaic_staging.tmp." + std::to_string(P1_NODE_GETPID) +
         "." + std::to_string(s);
}

// 把暂存区中的 mosaic 产品集统一原子发布到 out_dir:
//   1) 先摘掉旧完成清单 —— 从此刻起旧产品集不再可消费 (fail-closed);
//   2) 逐子产品「删旧 → rename 原子换入」(同文件系统内核原子);
//   3) **最后**落完成清单 manifest.json —— 它是唯一的"完成"标记, 没有它消费者
//      (aio_hips_open / aio_hips_verify_product_set) 一律拒绝;
//   4) fsync 父目录 + 清空暂存区。
// 任一步失败 ⇒ 返回 false (调用方递归删除暂存区); out_dir 此时无完成清单 ⇒
// 消费者拒绝, 不会把半发布树当成产品。
bool p2_publish_mosaic_tree(const std::string& out_dir,
                            const std::string& staging, std::string* err) {
  int is_dir = 0;
  if (!aio_atomic::path_exists(out_dir, &is_dir)) {
    if (aio_atomic::make_dirs(out_dir) != 0) {
      if (err) *err = "output_dir create failed: " + out_dir;
      return false;
    }
  } else if (!is_dir) {
    if (err) *err = "output_dir is not a directory: " + out_dir;
    return false;
  }
  if (aio_atomic::remove_file(out_dir + "/manifest.json") != 0) {
    if (err) *err = "旧完成清单不可移除: " + out_dir + "/manifest.json";
    return false;
  }
  const char* subs[] = {"signal", "support", "variance", "ivar",
                        "nrej", "nused", "snr"};
  for (const char* sub : subs) {
    const std::string sp = staging + "/" + sub;
    int sp_is_dir = 0;
    if (!aio_atomic::path_exists(sp, &sp_is_dir) || !sp_is_dir) continue;
    const std::string fp = out_dir + "/" + sub;
    if (aio_atomic::remove_tree(fp, 0) != 0) {
      if (err) *err = std::string("旧子产品目录不可移除: ") + fp;
      return false;
    }
    const int prc = aio_atomic::promote_dir(sp, fp);
    if (prc != aio_atomic::PROMOTE_OK) {
      if (err) *err = std::string("子产品原子发布失败 (promote rc=") +
                      std::to_string(prc) + "): " + sp + " -> " + fp;
      return false;
    }
  }
  const std::string sman = staging + "/manifest.json";
  if (!aio_atomic::path_exists(sman, nullptr)) {
    if (err) *err = "暂存区缺完成清单 (finalize 未成功): " + sman;
    return false;
  }
  if (aio_atomic::atomic_replace(sman, out_dir + "/manifest.json") != 0) {
    if (err) *err = "完成清单原子发布失败: " + sman;
    return false;
  }
  aio_atomic::fsync_parent_dir(out_dir + "/manifest.json");
  if (aio_atomic::remove_tree(staging, 0) != 0) {
    std::fprintf(stderr, "[hips] warning: staging 清理失败: %s\n", staging.c_str());
  }
  return true;
}

// ══ Phase2 mosaic 产品单位声明（BUNIT + 像素语义 provenance）═════════════════
// 依据: ASTROCS_DESIGN §5.6「Phase2 信号为面亮度量纲」; docs/contracts/DATA_SEMANTICS.md §31.1（signal_sb = ADU/sr; 方差/ivar 由二次律唯一导出）;
// FZ-BUNIT-SEMANTICS / FZ-P3-BUNIT-QUADRATIC。单位串 = 冻结单位表的 canonical
// 产品串（kP3Bunit* 常量），本节点不另发明第二套词表、不做任何"猜测"。
//
// 写出面 = 双写（与 AIO writer 的 properties ↔ manifest.json 双写纪律同构）:
//   * 每个 image 子产品 properties: BUNIT（signal=ADU/sr, variance=(BUNIT)^2,
//     ivar=1/(BUNIT)^2）+ ASTROCS_SIGNAL_UNIT / ASTROCS_PIXEL_SEMANTICS /
//     ASTROCS_PIXEL_AREA_POWER（canonical 幂次: signal -2 / variance -4 / ivar +4）;
//   * 产品根 manifest.json（完成清单）: units 块（键名同义, 值同源）。
// 幂等: 同名键先摘除再追加（HiPS properties 解析器禁重复键）。
bool declare_hips_surface_brightness_units(const std::string& product_root,
                                        bool uncertainty_available,
                                        std::string* err) {
  const std::string sb = kP3BunitSurfaceBrightness;
  const std::string var = kP3BunitSbVariance;
  const std::string ivar = kP3BunitSbIvar;
  struct SubUnit {
    const char* sub;
    const char* bunit;
    int pixel_area_power;
  };
  std::vector<SubUnit> subs{{"signal", sb.c_str(), -2}};
  if (uncertainty_available) {
    subs.push_back({"variance", var.c_str(), -4});
    subs.push_back({"ivar", ivar.c_str(), 4});
  }
  for (const SubUnit& su : subs) {
    const std::string path = product_root + "/" + su.sub + "/properties";
    std::string ptext;
    if (!aio_fs::read_all(path, &ptext)) {
      if (err) *err = std::string("properties 不可读: ") + path;
      return false;
    }
    std::string kept;
    {
      std::istringstream ls(ptext);
      std::string line;
      while (std::getline(ls, line)) {
        std::string key = line.substr(0, line.find('='));
        const size_t a = key.find_first_not_of(" \t\r");
        const size_t b = key.find_last_not_of(" \t\r");
        key = (a == std::string::npos) ? std::string() : key.substr(a, b - a + 1);
        if (key == "BUNIT" || key == "bunit" || key == "ASTROCS_SIGNAL_UNIT" ||
            key == "ASTROCS_PIXEL_SEMANTICS" || key == "ASTROCS_PIXEL_AREA_POWER")
          continue;   // 幂等: 摘除既有单位声明（禁重复键 / 禁静默旧值残留）
        kept += line;
        kept += '\n';
      }
    }
    kept += "BUNIT=" + std::string(su.bunit) + "\n";
    kept += "ASTROCS_SIGNAL_UNIT=" + sb + "\n";
    kept += "ASTROCS_PIXEL_SEMANTICS=surface_brightness\n";
    kept += "ASTROCS_PIXEL_AREA_POWER=" + std::to_string(su.pixel_area_power) + "\n";
    std::string werr;
    if (aio_atomic::write_file_atomic(path, kept, &werr) != 0) {
      if (err) *err = "properties 原子写失败: " + path + " (" + werr + ")";
      return false;
    }
  }
  // manifest.json（产品集完成标记）units 块: 与 properties 同源双写。
  const std::string man_path = product_root + "/manifest.json";
  Json mdoc;
  if (!p2_read_json(man_path, &mdoc) || !mdoc.is_object()) {
    if (err) *err = "manifest.json 不可读/非法 JSON: " + man_path;
    return false;
  }
  mdoc["units"] = Json{{"bunit", sb},
                       {"signal_unit", sb},
                       {"pixel_semantics", "surface_brightness"},
                       {"pixel_area_power", -2},
                       {"variance_bunit", uncertainty_available ? var : std::string()},
                       {"ivar_bunit", uncertainty_available ? ivar : std::string()}};
  std::string werr;
  if (aio_atomic::write_file_atomic(man_path, mdoc.dump(2) + "\n", &werr) != 0) {
    if (err) *err = "manifest.json 原子写失败: " + man_path + " (" + werr + ")";
    return false;
  }
  // ── B1 守卫：同一份像素的两个 BUNIT 声明面必须同串（Phase1 直写路径覆盖）──────
  // Phase1 末端由 AstroSphereSink 直写标准 HiPS 树（signal/ + support/ + properties），
  // **不经** HissWriter::open 的元数据校验 —— 这正是「两个单位串并存而运行期无信号」
  // 的成因。故在此显式核对本帧 p1_stack.json 的 bunit 键与刚写出的 properties BUNIT：
  // 不一致即判红（fail-closed，禁静默）。
  // 规范：docs/contracts/DATA_SEMANTICS.md §31.1a:2808-2810（产品 BUNIT 一律取 canonical
  // 面亮度串）+ :2827-2830（测光标度由 PHOTSCAL/PHOTAPPL 承载，不改量纲类别）。
  // 注：p1_stack.json 缺失 = 该产品族无此声明面（如 Phase2 mosaic 产品），不判。
  {
    const std::string st_path = product_root + "/p1_stack.json";
    std::string stext;
    if (aio_fs::read_all(st_path, &stext)) {
      Json sd;
      bool parsed = false;
      try { sd = Json::parse(stext); parsed = sd.is_object(); } catch (...) { parsed = false; }
      if (!parsed) {
        if (err) *err = "p1_stack.json 不可解析（BUNIT 口径核对失败）: " + st_path;
        return false;
      }
      const std::string declared = sd.value("bunit", std::string());
      if (declared != sb) {
        if (err)
          *err = "BUNIT 口径不一致（同一份像素两个单位串）: p1_stack.json bunit=\"" +
                 declared + "\" vs signal/properties BUNIT=\"" + sb +
                 "\"（docs/contracts/DATA_SEMANTICS.md §31.1a:2808-2810：产品 BUNIT 一律取 "
                 "canonical 面亮度串；测光标度由 PHOTAPPL/PHOTSCAL 承载）";
        return false;
      }
    }
  }
  return true;
}

// ── op: write_mosaic（唯一真实入口 aio_hips_product_begin /
//      aio_hips_write_signal_support_tile / aio_hips_write_variance_tile /
//      aio_hips_finalize — AIO-002 原子发布原语内建于 aio_hips 落盘路径;
//      AIO 唯一 writer, 禁手写 FITS）。
//      variance/ivar = DATA-P2-VAR-001 §30.1: ivar_mosaic=W=Σivar_i、
//      variance=1/W 经 writer 通道（var_num_sum = variance×cov²,
//      writer 归约 variance = var_num_sum/covered_area²）; n_used=0 →
//      cov=0 → 产 0（writer 按 covered_area<=0 出 NaN，§30.4 同态）。nused/nrej
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
  // 审计面（DATA-UNC-001 §30.1 规则 1 / ASTROCS_DESIGN §3.1）：集成产物必须
  // **显式**声明方差面是否科学可用；缺键 ⇒ DATA fail-closed（禁静默缺省）。
  // G3-12：原实现以整数 weight_mode∈{1,2} 承载该状态 —— 与最高设计
  // §3.1「全程只有 SNR，不存在『权重模式』这个概念」冲突，且该键随产品落盘。
  // 现改用同一 p2_integrated.json 内**已有的语义键**：uncertainty_available
  // （方差/ivar 子产品是否定义）与 corrected_variance_used / snr_chain_used
  // （逐样本 ivar 面或 SNR 权重链是否真的用上）。判据强度不变（缺键/不自洽
  // 一律 DATA fail-closed），只是不再引入「权重模式」词汇。
  if (!int_doc.contains("uncertainty_available") ||
      !int_doc["uncertainty_available"].is_boolean())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "integrated artifact missing boolean uncertainty_available (upstream"
        " integrate must declare it; DATA-UNC-001 §30.1 rule 1)"));
  const bool uncertainty_available = int_doc["uncertainty_available"].get<bool>();
  // 方差/ivar 子产品只由**逐样本权重面**定义（DATA-UNC-001 §30.1）：
  //   weight_basis = per_sample_ivar（逐帧 ivar 产品齐备）或
  //                  per_pixel_corrected_variance（逐像素归一化方差面）。
  // 帧级 SNR 链（frame_snr_ivar）只是积分权重的显式降级路径 ⇒ 该路径上
  // uncertainty_available 必须为 false（CONFORM-FIX-B-004），此处双向锁死。
  if (uncertainty_available) {
    const std::string basis = int_doc.value("weight_basis", std::string());
    const bool per_sample_surface =
        (basis == "per_sample_ivar" || basis == "per_pixel_corrected_variance");
    const bool ivar_missing =
        int_doc.value("ivar_product_missing_frames", 0) != 0;
    const bool snr_chain_used = int_doc.value("snr_chain_used", false);
    if (!per_sample_surface || ivar_missing || snr_chain_used)
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "uncertainty_available=true without a per-sample weight surface"
          " (weight_basis=" + (basis.empty() ? std::string("<missing>") : basis) +
          ", ivar_product_missing_frames=" +
          std::to_string(int_doc.value("ivar_product_missing_frames", 0)) +
          ", snr_chain_used=" + (snr_chain_used ? "true" : "false") +
          "; DATA-UNC-001 §30.1 rule 1: variance/ivar products are defined only"
          " by per-sample ivar / per-pixel corrected variance)"));
  }
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
  // §10: 全部子产品先落本次运行私有暂存区, 校验通过后统一原子发布
  // (不再直写正式 output_dir)。
  const std::string staging = p2_mosaic_staging_path(out_dir);
  if (aio_atomic::remove_tree(staging, 0) != 0)
    return Result<void>::fail(Error(ErrorDomain::IO,
        "mosaic staging 残留不可清除: " + staging));
  if (aio_atomic::make_dir(staging) != 0)
    return Result<void>::fail(Error(ErrorDomain::IO,
        "mosaic staging 创建失败: " + staging));
  AioHipsProductSet* ps = aio_hips_product_begin(
      staging.c_str(), nside, 512, AIO_HIPS_FLOAT32, flags,
      "ivo://astrocs/phase2", "AstroCS Phase2 mosaic",
      // B2-A8: coverage union 已验证全帧 filter 身份（含显式空声明），mosaic
      // 恒透传该身份；properties 写侧恒写 obs_filter 键（空值也是声明）。
      obs_filter.c_str(),
      0.0, nullptr, 0);
  if (!ps) {
    aio_atomic::remove_tree(staging, 0);
    return Result<void>::fail(Error(ErrorDomain::IO,
        std::string("aio_hips_product_begin failed: ") + aio_hips_last_error()));
  }
  // §10 + LOG_AND_ERROR_CONTRACT §5: 本节点产品写入的**归因窗口**（执行流局部的
  // 单调计数快照）。窗口覆盖 write_signal_support/write_variance/finalize 三个
  // 失败收尾点 ⇒ 判定属于本节点自己的失败，不被并发执行流覆盖或抢占
  // (aio_disk_full.h 头注「归因粒度=帧级」)。
  const aio_disk::FailureEpoch dsk_epoch;

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
      // variance = var_num_sum/cov²）。三态（DATA_SEMANTICS §4a:49 / §12.4:423 /
      // §30.4:2698）：
      //   无覆盖 (cov<=0)        → **有限 0**：writer 按 covered_area<=0 输出 NaN
      //                            （§30.4「无覆盖 variance 用 NaN，不用 §4a 的 0」）。
      //                            此处**不得**用 NaN 当哨兵 —— §4a:49「NaN/负只
      //                            表示产品损坏」，而 §12.4:423 对非有限输入是
      //                            rc=-6 硬失败（禁 clamp/禁静默跳过）。
      //   有覆盖 ∧ W 有效 (w>0)   → cov²/W（逆方差）。
      //   有覆盖 ∧ W==0          → **0 = 显式不可用**（§4a:49「有覆盖但方差不可用
      //                            ⇒ variance=0 ∧ ivar=0」，禁 NaN）。这一行是
      //                            三态表的第三态，不是损坏：W==0 的来源是全部样本
      //                            方差不可用（p1 variance=0 ⇒ ivar=0 ⇒ Σw=0）。
      //   有覆盖 ∧ W 非有限或负  → NaN = **真损坏** ⇒ writer rc=-6 硬失败（信号保留）。
      if (uncertainty_available) {
        const double w = wsum_v[static_cast<size_t>(i)];
        if (!(cov > 0.0)) {
          varnum_buf[local] = 0.0f;
        } else if (std::isfinite(w) && w > 0.0) {
          varnum_buf[local] = static_cast<float>((cov * cov) / w);
        } else if (w == 0.0) {
          varnum_buf[local] = 0.0f;
        } else {
          varnum_buf[local] = std::numeric_limits<float>::quiet_NaN();
        }
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
    aio_hips_tile_view_abi_init(&view);
    const int wr = aio_hips_write_signal_support_tile(ps, &view);
    if (wr != 0) {
      aio_hips_abort(ps);
      // §7.2: 磁盘满按失败本身归类 (aio 在清理前已判定) → 失败节点
      // manifest error_kind="disk_full" → CLI 映射 exit 10。
      if (man && dsk_epoch.failed()) (*man)["error_kind"] = "disk_full";
      aio_atomic::remove_tree(staging, 0);   // §10: 失败路径清临时产物
      return Result<void>::fail(Error(ErrorDomain::IO,
          std::string("aio_hips_write_signal_support_tile failed: ") +
          aio_hips_last_error()));
    }
    if (uncertainty_available) {
      aio_hips_tile_view_abi_init(&view);
      const int wv = aio_hips_write_variance_tile(ps, &view);
      if (wv != 0) {
        aio_hips_abort(ps);
        if (man && dsk_epoch.failed()) (*man)["error_kind"] = "disk_full";
        aio_atomic::remove_tree(staging, 0);   // §10: 失败路径清临时产物
        return Result<void>::fail(Error(ErrorDomain::IO,
            std::string("aio_hips_write_variance_tile failed: ") +
            aio_hips_last_error()));
      }
    }
    ++n_tiles_written;
  }
  if (aio_hips_finalize(ps) != 0) {
    aio_hips_abort(ps);
    if (man && dsk_epoch.failed()) (*man)["error_kind"] = "disk_full";
    aio_atomic::remove_tree(staging, 0);     // §10: 失败路径清临时产物
    return Result<void>::fail(Error(ErrorDomain::IO,
        std::string("aio_hips_finalize failed: ") + aio_hips_last_error()));
  }
  const std::string staging_props = staging + "/signal/properties";
  // §9/CLEAN-403: 存在性判定走 aio 原语 (不新增 filesystem 原语命中)
  if (!aio_atomic::path_exists(staging_props, nullptr)) {
    aio_atomic::remove_tree(staging, 0);
    return Result<void>::fail(Error(ErrorDomain::IO,
        "HiPS properties missing after finalize: " + staging_props));
  }

  // 产品面回读校验（variance/ivar tile 数一致; HIPS_VERIFY 目标态 §30.1 扩展）
  Json products = Json::array({"signal", "support"});
  if (uncertainty_available) {
    products.push_back("variance");
    products.push_back("ivar");
    AioHipsDataset* dv = aio_hips_open(staging.c_str(), AIO_HIPS_RD_VARIANCE);
    AioHipsDataset* di = aio_hips_open(staging.c_str(), AIO_HIPS_RD_IVAR);
    const bool ok = dv && di &&
                    aio_hips_tile_count(dv) == aio_hips_tile_count(di);
    if (dv) aio_hips_close(dv);
    if (di) aio_hips_close(di);
    if (!ok) {
      aio_atomic::remove_tree(staging, 0);
      return Result<void>::fail(Error(ErrorDomain::IO,
          "variance/ivar product tile count mismatch after finalize (HIPS_VERIFY)"));
    }
  }
  // 单位/像素语义声明（在暂存区内完成 → 随产品集一起原子发布;
  // properties 与 manifest.json 双写同源）。声明失败 = 产品单位面不完整 →
  // 丢弃暂存区显式拒（output_dir 不出现无单位声明的 mosaic）。
  {
    std::string uerr;
    if (!declare_hips_surface_brightness_units(staging, uncertainty_available, &uerr)) {
      aio_atomic::remove_tree(staging, 0);
      return Result<void>::fail(Error(ErrorDomain::IO,
          "mosaic 单位/像素语义声明失败: " + uerr));
    }
  }
  // 成功对象校验（发布前, 暂存区）: 完成清单 ↔ 磁盘事实双向一致
  {
    AioHipsVerifyReport rep;
    std::memset(&rep, 0, sizeof(rep));
    rep.struct_size = (uint32_t)sizeof(AioHipsVerifyReport);
    rep.abi_version = AIO_HIPS_VERIFY_REPORT_ABI_VERSION;
    const int vrc = aio_hips_verify_product_set(staging.c_str(), &rep);
    if (vrc != 0) {
      const std::string verr = aio_hips_last_error();
      aio_atomic::remove_tree(staging, 0);
      return Result<void>::fail(Error(ErrorDomain::IO,
          "mosaic 产品集校验失败 (verify rc=" + std::to_string(vrc) + ": " +
          verr + "); 暂存区已丢弃, output_dir 无完成清单"));
    }
  }
  // 统一原子发布: 先摘旧完成清单 → 逐子产品 rename 换入 → 最后落完成清单。
  {
    std::string perr;
    if (!p2_publish_mosaic_tree(out_dir, staging, &perr)) {
      aio_atomic::remove_tree(staging, 0);
      return Result<void>::fail(Error(ErrorDomain::IO,
          "mosaic 原子发布失败: " + perr + " (暂存区已丢弃)"));
    }
  }
  // 发布后回读（正式目录, 消费者视角; 完成清单已在, fail-closed 门已开）
  {
    const std::string fprops = out_dir + "/signal/properties";
    if (!aio_atomic::path_exists(fprops, nullptr))
      return Result<void>::fail(Error(ErrorDomain::IO,
          "HiPS properties missing after publish: " + fprops));
    if (uncertainty_available) {
      AioHipsDataset* dv = aio_hips_open(out_dir.c_str(), AIO_HIPS_RD_VARIANCE);
      AioHipsDataset* di = aio_hips_open(out_dir.c_str(), AIO_HIPS_RD_IVAR);
      const bool ok = dv && di &&
                      aio_hips_tile_count(dv) == aio_hips_tile_count(di);
      if (dv) aio_hips_close(dv);
      if (di) aio_hips_close(di);
      if (!ok)
        return Result<void>::fail(Error(ErrorDomain::IO,
            "variance/ivar tile count mismatch after publish (HIPS_VERIFY)"));
    }
  }

  // p2_final.json 的 properties 引用必须是**正式目录**路径
  // （staging 已随发布清理; 旧值指向被删除的暂存路径 = 悬空引用, 消费者读不到
  // 已发布的 properties/BUNIT 声明）。
  const std::string props = out_dir + "/signal/properties";

  const std::string out_path = out_dir + "/p2_final.json";
  // 权重面审计（IVAR-001）: weight_basis 与缺 ivar 帧数随 mosaic 产品面落盘,
  // 使「本次叠加用的是逐样本 ivar 还是显式等权降级」在阶段交换面可判, 不依赖
  // 上游节点目录的临时文件。
  const std::string weight_basis = int_doc.value("weight_basis", std::string());
  const uint64_t ivar_missing_frames =
      int_doc.value("ivar_product_missing_frames", 0ull);
  // CONFORM-FIX-B-004: 不可用原因随产品面落盘（§30.1「diagnostics 标红计数」），
  // 使「为何没有 variance/ivar 子产品」在阶段交换面可判（禁静默缺键）。
  const std::string uncertainty_unavailable_reason =
      int_doc.value("uncertainty_unavailable_reason", std::string());
  Json final_out = Json{{"schema", "DATA-P2-RES"},
                        {"entry", "aio_hips_product_begin/write_signal_support_tile/write_variance_tile/finalize"},
                        {"hips_root", out_dir},
                        {"nside", nside},
                        {"target_order", target_order},
                        {"n_tiles_written", n_tiles_written},
                        {"products", products},
                        {"covered_area_model", "support_x_A_cell"},
                        // 像素语义/单位随产品 manifest 落盘（与已发布的
                        // signal/properties BUNIT 声明同源同值; FZ-BUNIT-SEMANTICS）。
                        {"bunit", kP3BunitSurfaceBrightness},
                        {"units", Json{
                            {"bunit", kP3BunitSurfaceBrightness},
                            {"signal_unit", kP3BunitSurfaceBrightness},
                            {"pixel_semantics", "surface_brightness"},
                            {"pixel_area_power", -2},
                            {"variance_bunit", uncertainty_available
                                 ? std::string(kP3BunitSbVariance)
                                 : std::string()},
                            {"ivar_bunit", uncertainty_available
                                 ? std::string(kP3BunitSbIvar)
                                 : std::string()},
                            // FZ-P3-BUNIT-QUADRATIC 的串是**冻结契约值**（product_family_field_constraints
                            // .schema.json $defs/units 的 const 逐字规定），本处**逐字照抄、不得改写**。
                            // 读法消歧见 docs/contracts/DATA_SEMANTICS.md §31.1：首分句 "variance = signal^2"
                            // 是**单位传播的简写**，读作 BUNIT(variance) = BUNIT(signal)^2，**不是**数值物理律
                            // （数值上含读噪/量化时 variance != signal^2，见 EMVA 1288 R4.0 Linear §2.4 Eq.(15)）。
                            {"quadratic_law", "variance = signal^2; ivar = 1/variance; Phase3 variance BUNIT = (main HDU signal BUNIT)^2"}}},
                        {"uncertainty_available", uncertainty_available},
                        // G3-12（ASTROCS_DESIGN §3.1）：p2_final.json
                        // **不再落** weight_mode 键（「全程只有 SNR，不存在
                        // 『权重模式』这个概念」）；方差面状态由
                        // uncertainty_available + weight_basis +
                        // uncertainty_unavailable_reason 如实承载。
                        {"weight_basis", weight_basis},
                        {"ivar_product_missing_frames", ivar_missing_frames},
                        {"uncertainty_unavailable_reason",
                         uncertainty_unavailable_reason},
                        {"provenance", Json{
                            {"ASTROCS_INPUT_MANIFEST_HASH", manifest_hash},
                            {"ASTROCS_MODEL_HASH", model_hash},
                            {"ASTROCS_UNCERTAINTY_AVAILABLE",
                             uncertainty_available ? "true" : "false"},
                            // A44（GAP_AUDIT §9.73 / ASTROCS_DESIGN §2.1）：全程只有
                            // SNR，**不存在「权重模式」** ⇒ 本 provenance 面不得承载
                            // ASTROCS_WEIGHT_MODE（原键已删除；HiPS provenance 只承载
                            // 帧级 SNR 与稀疏相对 SNR 比值）。
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
  if (!p2_write_text_atomic(out_path, final_out.dump(2) + "\n"))
    return Result<void>::fail(Error(ErrorDomain::IO, "artifact write failed"));
  (*man)["artifacts"] = Json::array({out_path, out_dir + "/signal/properties"});
  (*man)["mosaic_root"] = out_dir;
  (*man)["final_artifact"] = out_path;
  (*man)["n_tiles_written"] = n_tiles_written;
  (*man)["uncertainty_available"] = uncertainty_available;
  (*man)["weight_basis"] = weight_basis;
  (*man)["ivar_product_missing_frames"] = ivar_missing_frames;
  (*man)["uncertainty_unavailable_reason"] = uncertainty_unavailable_reason;
  // B2-A10（宪章 §4.3）: 单位/坐标系/输入产品哈希随节点 manifest 上报，
  // 供 run manifest provenance 汇总（Phase2 mosaic 单位 = ADU，
  // 坐标系 = ICRS，P0-19 与 properties 的 hips_frame=equatorial 同源）。
  (*man)["bunit"] = kP3BunitSurfaceBrightness;
  (*man)["pixel_semantics"] = "surface_brightness";
  (*man)["pixel_area_power"] = -2;
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
      // PERF-P1: 与 Phase2 同规范——Runtime lease 是并行度唯一权威，节点 op 内以
      // __workers 消费（禁硬编码；budget 为空时 lease 降级为 1 ⇒ 串行 reference）。
      doc["__workers"] = cap;
      switch (spec_.op) {
        // [RELEASE-02 probe] Phase1 七阶段边界 (calibrate/cosmetic/star_psf/wcs/noise/drizzle/writer)
        case P1NodeOp::Calibrate: {
          ASTROCS_PROBE_SCOPE("phase1", "calibrate");
          r = p1_op_calibrate(doc, &man); break;
        }
        case P1NodeOp::Cosmetic: {
          ASTROCS_PROBE_SCOPE("phase1", "cosmetic");
          r = p1_op_cosmetic(doc, &man); break;
        }
        case P1NodeOp::StarPsf: {
          ASTROCS_PROBE_SCOPE("phase1", "star_psf");
          r = p1_op_star_psf(doc, &man); break;
        }
        case P1NodeOp::WcsSolve: {
          ASTROCS_PROBE_SCOPE("phase1", "wcs");
          r = p1_op_wcs(doc, &man); break;
        }
        case P1NodeOp::Photometry: {
          ASTROCS_PROBE_SCOPE("phase1", "photometry");
          r = p1_op_photometry(doc, &man); break;
        }
        case P1NodeOp::NoiseSnr: {
          ASTROCS_PROBE_SCOPE("phase1", "noise");
          r = p1_op_noise(doc, &man); break;
        }
        case P1NodeOp::Drizzle: {
          ASTROCS_PROBE_SCOPE("phase1", "drizzle");
          r = p1_op_drizzle(doc, &man); break;
        }
        case P1NodeOp::Writer: {
          ASTROCS_PROBE_SCOPE("phase1", "writer");
          r = p1_op_writer(doc, &man); break;
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
  // upm/reject 对象可选; weight_mode **已按 §9.73 裁决 A44 删除**（出现即拒绝,
  // 不存在「权重模式」）; 其余科学参数无 silent default（op 内 fail-closed
  // 校验, 不提前消费缺省值）。
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
    // §9.73 裁决 A44：不存在「权重模式」⇒ 该键既不能被设、也不能被读。
    // 出现在节点配置里即 fail-closed 拒绝（不得静默忽略，也不得做类型校验后放行）。
    if (doc.contains("weight_mode"))
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "weight_mode 已按 §9.73 裁决 A44 删除：不存在「权重模式」"
          "（ASTROCS_DESIGN.md §3.1:175「没有可选择项」；"
          "docs/science/PSF_SIGNAL_WEIGHT.md §4「单一权重口径（无模式选择）」）。"));
    // §9.73 裁决 A44（同批清理）：该键已删除 ⇒ 出现即拒绝（不再做类型校验后放行）。
    if (doc.contains("legacy_allow_weight_fallback"))
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "legacy_allow_weight_fallback 已按 §9.73 裁决 A44 删除：它允许用无量纲 "
          "support 或等权降级冒充逆方差权重（ASTROCS_DESIGN.md §3.1:173/175）。"));
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
        // CLEAN-403: 目录创建经 aio (make_dirs); 已存在视为成功 (幂等)。
        if (!aio_fs::make_dirs(out_dir) && !aio_fs::is_dir(out_dir)) {
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
        // [RELEASE-02 probe] Phase2 七阶段边界 (coverage/sample/upm_fit/upm_apply/reject/integrate/write)
        case P2NodeOp::Coverage: {
          ASTROCS_PROBE_SCOPE("phase2", "coverage");
          r = p2_op_coverage(cfg2, &man); break;
        }
        case P2NodeOp::Sample: {
          ASTROCS_PROBE_SCOPE("phase2", "sample");
          r = p2_op_sample(cfg2, &man); break;
        }
        case P2NodeOp::UpmFit: {
          ASTROCS_PROBE_SCOPE("phase2", "upm_fit");
          r = p2_op_upm_fit(cfg2, &man); break;
        }
        case P2NodeOp::UpmApply: {
          ASTROCS_PROBE_SCOPE("phase2", "upm_apply");
          r = p2_op_upm_apply(cfg2, &man); break;
        }
        case P2NodeOp::Reject: {
          ASTROCS_PROBE_SCOPE("phase2", "reject");
          r = p2_op_reject(cfg2, &man); break;
        }
        case P2NodeOp::Integrate: {
          ASTROCS_PROBE_SCOPE("phase2", "integrate");
          r = p2_op_integrate(cfg2, &man); break;
        }
        case P2NodeOp::Write: {
          ASTROCS_PROBE_SCOPE("phase2", "write");
          r = p2_op_write(cfg2, &man); break;
        }
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
// operation/entry 名与 lib/infrastructure/pipeline/module_ports.registry.json 冻结绑定
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

// ── P3-STREAM-01：子块边长（编排参数，非科学键）──────────────────────────
// 依据 docs/contracts/SCHEDULER_CONTRACT.md §3「分块/窗口/子块大小由配置/资源门
// 决定，禁止硬编码」：默认值 = ARCH-504 组件声明的配置默认（256），配置可改；
// 值域 [16, 1024] 为内存守卫（在途上界 = 2·queue_depth·sb²·8 B，与总图大小无关）。
// 与 max_tiles 同款「资源/编排键」形态（CLI 会话键白名单已登记，见 parser.cpp）。
constexpr int kP3DefaultSubBlockPx = 256;
constexpr int kP3MinSubBlockPx = 16;
constexpr int kP3MaxSubBlockPx = 1024;
constexpr int kP3DefaultQueueDepth = 4;

bool p3n_sub_block_px(const Json& doc, int* out, std::string* err) {
  auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
  const int64_t sb = doc.value("sub_block_px", (int64_t)kP3DefaultSubBlockPx);
  if (sb < kP3MinSubBlockPx || sb > kP3MaxSubBlockPx)
    return fail("sub_block_px must be in [16,1024] (编排参数内存守卫)");
  *out = (int)sb;
  return true;
}

// ── EXPORT-CROP-01：导出裁剪范围（默认不裁剪）──────────────────────────────
// 权威：docs/contracts/CONFIG_CONTRACT.md §3（export 行 crop）+ docs/design/
// PHASE3_DETAILED_DESIGN.md §8；几何唯一实现 = lib/algorithms/projection/p3_wcs.h
// （CLI 配置面与节点面共用，禁第二份）。负责人裁决 2026-09-23：默认导出不得裁剪
// 任何有效像素（允许黑边），裁剪为**可选**参数，且两种形式都要有。
// 键形（稳定、可机器生成；GUI 框选直接填）：
//   {"crop":{"crop_form":"pixels","pixels":{"x0":1,"y0":1,"x1":512,"y1":512}}}
//     —— FITS 1-based 闭区间，相对**未裁剪输出画幅**（用户到平面后手动裁剪；GUI 框选）
//   {"crop":{"crop_form":"sky","sky":{"ra_min_deg":..,"ra_max_deg":..,
//                                "dec_min_deg":..,"dec_max_deg":..}}}
//     —— ICRS deg 轴对齐矩形（GUI 的框选来自 HiPS 浏览器，框的是天区）；
//        边界加密采样后外扩取整为像素窗口（保证不切掉任何中心落在矩形内的像素）
// 两形式**互斥**：crop_form 选中其一，另一形式同时出现即具名拒绝（不比较、不取一）。
// 判别键名取 crop_form 而非 mode：cpu_profile 的 legacy_v1/kernel_v1 已占用 mode，
// 同名异义被 UNIFIED_MODEL §3 禁止（与 export 用 output_mode 避开 mode 同一处置）。
// 全部非法输入 fail-closed 具名拒绝（越界/宽高非正/两形式同时给/裁剪后为空），
// 禁静默夹取。
bool p3n_crop_shape(const Json& doc, Json* out_crop, std::string* err) {
  auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
  if (!doc.contains("crop")) return true;   // 缺省 = 不裁剪
  const Json& c = doc["crop"];
  if (!c.is_object()) return fail("crop must be an object");
  if (!c.contains("crop_form") || !c["crop_form"].is_string())
    return fail("crop.crop_form is required and must be a string (\"pixels\"|\"sky\")");
  const std::string mode = c["crop_form"].get<std::string>();
  if (mode != "pixels" && mode != "sky")
    return fail("crop.crop_form must be \"pixels\"|\"sky\" (got \"" + mode + "\")");
  const std::string other = (mode == "pixels") ? "sky" : "pixels";
  if (c.contains(other))
    return fail("crop: \"" + mode + "\" and \"" + other +
                "\" are mutually exclusive — both were given; refusing to pick one"
                " (delete the unused form)");
  if (!c.contains(mode))
    return fail("crop: crop_form=\"" + mode + "\" requires the \"" + mode + "\" object");
  for (auto it = c.begin(); it != c.end(); ++it) {
    if (it.key() != "crop_form" && it.key() != mode)
      return fail("crop: unknown key \"" + it.key() + "\" (allowed: crop_form + \"" + mode +
                  "\")");
  }
  const Json& form = c[mode];
  if (!form.is_object()) return fail("crop." + mode + " must be an object");
  const char* pk[4] = {"x0", "y0", "x1", "y1"};
  const char* sk[4] = {"ra_min_deg", "ra_max_deg", "dec_min_deg", "dec_max_deg"};
  const char** keys = (mode == "sky") ? sk : pk;
  for (int i = 0; i < 4; ++i) {
    if (!form.contains(keys[i]))
      return fail(std::string("crop.") + mode + "." + keys[i] + " is required");
    const bool ok_type = (mode == "sky") ? form[keys[i]].is_number()
                                         : form[keys[i]].is_number_integer();
    if (!ok_type)
      return fail(std::string("crop.") + mode + "." + keys[i] +
                  (mode == "sky" ? " must be a number (ICRS deg)"
                                 : " must be an integer (FITS 1-based pixel index)"));
  }
  for (auto it = form.begin(); it != form.end(); ++it) {
    bool known = false;
    for (int i = 0; i < 4; ++i) if (it.key() == keys[i]) known = true;
    if (!known) return fail("crop." + mode + ": unknown key \"" + it.key() + "\"");
  }
  if (out_crop) *out_crop = c;
  return true;
}

// crop 形状 + 几何。frame = **未裁剪**输出画幅描述符（w/h 必须与之一致）。
// 缺省（无 crop 键）⇒ win->active=false，行为与既有整幅导出逐位相同。
bool p3n_crop_window(const Json& doc, const astrocs::phase3::P3WcsDescriptor& frame,
                     int w, int h, astrocs::phase3::P3CropWindow* win,
                     std::string* mode_out, std::string* err) {
  using namespace astrocs::phase3;
  auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
  win->active = false;
  if (mode_out) mode_out->clear();
  Json c;
  if (!p3n_crop_shape(doc, &c, err)) return false;
  if (c.is_null()) return true;
  const std::string mode = c["crop_form"].get<std::string>();
  if (mode_out) *mode_out = mode;
  const Json& f = c[mode];
  std::string why;
  P3CropStatus st = P3_CROP_PARAM;
  if (mode == "pixels") {
    st = p3_crop_window_from_fits(f["x0"].get<long long>(), f["y0"].get<long long>(),
                                  f["x1"].get<long long>(), f["y1"].get<long long>(),
                                  w, h, win, &why);
  } else {
    st = p3_crop_window_from_sky(&frame, f["ra_min_deg"].get<double>(),
                                 f["ra_max_deg"].get<double>(),
                                 f["dec_min_deg"].get<double>(),
                                 f["dec_max_deg"].get<double>(), w, h, win, &why);
  }
  if (st != P3_CROP_OK)
    return fail(why.empty() ? std::string("crop: rejected") : why);
  return true;
}

// 已解析窗口 → p3_wcs.json 的 crop 块（writer/verify 的唯一来源）。
Json p3n_crop_plan_json(const astrocs::phase3::P3CropWindow& win,
                        const std::string& mode) {
  if (!win.active) return Json{{"active", false}};
  return Json{{"active", true},   {"crop_form", mode},
              {"x0", win.x0},     {"y0", win.y0},   {"x1", win.x1}, {"y1", win.y1},
              {"width_px", win.width()}, {"height_px", win.height()}};
}

// p3_wcs.json → 窗口（writer/verify 只读此处，禁各自重算；越域 = artifact 漂移拒）。
bool p3n_crop_from_plan(const Json& plan, int w, int h,
                        astrocs::phase3::P3CropWindow* win, std::string* err) {
  auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
  win->active = false;
  if (!plan.contains("crop")) return true;   // 旧 artifact：无 crop = 不裁剪
  const Json& c = plan["crop"];
  if (!c.is_object() || !c.contains("active") || !c["active"].is_boolean())
    return fail("p3_wcs.json crop block malformed (missing boolean active)");
  if (!c["active"].get<bool>()) return true;
  for (const char* k : {"x0", "y0", "x1", "y1"}) {
    if (!c.contains(k) || !c[k].is_number_integer())
      return fail(std::string("p3_wcs.json crop.") + k + " missing/not integer");
  }
  win->active = true;
  win->x0 = c["x0"].get<long long>();
  win->y0 = c["y0"].get<long long>();
  win->x1 = c["x1"].get<long long>();
  win->y1 = c["y1"].get<long long>();
  if (win->width() < 1 || win->height() < 1 || win->x0 < 0 || win->y0 < 0 ||
      win->x1 > w || win->y1 > h)
    return fail("p3_wcs.json crop window outside the output frame (artifact drift)");
  return true;
}

// 平面 → 窗口子平面（整幅驻留参考路径用；流式路径按行偏移直读）。
void p3n_window_plane(const float* src, int w, const astrocs::phase3::P3CropWindow& c,
                      std::vector<float>* dst) {
  dst->resize((std::size_t)c.width() * (std::size_t)c.height());
  for (int y = 0; y < c.height(); ++y)
    std::memcpy(dst->data() + (std::size_t)y * (std::size_t)c.width(),
                src + (std::size_t)(c.y0 + y) * (std::size_t)w + (std::size_t)c.x0,
                (std::size_t)c.width() * sizeof(float));
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
  // EXPORT-CROP-01: 裁剪范围先于数值面做**形状**校验（越界/宽高非正/两形式同时给
  // 等几何判定需要画幅 WCS，在 wcs 节点与 validate_config 处同源判；此处先拒形状）。
  if (!p3n_crop_shape(doc, nullptr, err)) return false;
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
  // CLEAN-403: 整文件读取经 aio (aio_file::read_all)。
  std::string s;
  if (!aio_fs::read_all(path, &s)) {
    if (err) *err = "upstream artifact missing: " + path;
    return false;
  }
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
    std::string s;
    if (!aio_fs::read_all(hips_dir + rel, &s)) continue;
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

// ══ Phase3 输入语义守卫（ASTROCS_DESIGN §6.3 / FZ-BUNIT-SEMANTICS）═════════════
// 生产 export 只接受**面亮度语义**输入；按输入 provenance 声明的单位分派，
// 不做任何"自动猜测单位"的宽松解析（缺声明即拒绝，禁 silent default ADU）:
//   * 面亮度（BUNIT 显式含立体角幂次 canonical "ADU/sr"，或 BUNIT=ADU +
//     ASTROCS_PIXEL_SEMANTICS=surface_brightness + ASTROCS_PIXEL_AREA_POWER=-2）→ 放行;
//     下游统一携带冻结单位表的 canonical 产品串（"ADU/sr"）。
//   * 缺 BUNIT / 空 BUNIT / 裸 ADU 而无像素语义声明（单位不可判）→ 输入缺失或
//     格式错 → error_kind=input（CLI exit 3）。
//   * 已声明但非面亮度（积分通量 ADU/px^0、方差或 ivar 面、冻结单位表外单位）→
//     科学语义违例 → ErrorDomain::SCIENCE_PRECONDITION（CLI exit 4）。
struct P3InputUnit {
  bool ok = false;
  bool input_error = false;      // true → exit 3（输入缺失/格式错）; false → exit 4
  std::string bunit_raw;
  std::string bunit_canonical;   // 通过时 = "ADU/sr"
  std::string code;              // 机器可判错误码（节点 manifest semantic_code）
  std::string reason;
};

// 读 HiPS properties 文本键（"KEY = value" 行）。重复键 → 显式拒绝（禁 silent override）。
bool p3n_properties_scalar_keys(const std::string& path,
                                std::map<std::string, std::string>* out,
                                std::string* err) {
  // CLEAN-403: 整文件读取经 aio (aio_file::read_all), 逐行解析在内存进行。
  std::string ptext;
  if (!aio_fs::read_all(path, &ptext)) {
    if (err) *err = "properties not found: " + path;
    return false;
  }
  const auto trim = [](std::string s) {
    const size_t a = s.find_first_not_of(" \t\r");
    const size_t b = s.find_last_not_of(" \t\r");
    return (a == std::string::npos) ? std::string() : s.substr(a, b - a + 1);
  };
  std::istringstream f(ptext);
  std::string line;
  while (std::getline(f, line)) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = trim(line.substr(0, eq));
    const std::string val = trim(line.substr(eq + 1));
    if (key.empty()) continue;
    if (out->count(key)) {
      if (err) *err = "duplicate properties key: " + key;
      return false;
    }
    (*out)[key] = val;
  }
  return true;
}

P3InputUnit p3n_guard_input_units(const std::string& hips_dir) {
  using namespace astrocs::p3rsmp;
  P3InputUnit g;
  std::map<std::string, std::string> kv;
  std::string perr;
  if (!p3n_properties_scalar_keys(hips_dir + "/signal/properties", &kv, &perr)) {
    g.input_error = true;
    g.code = "P3-INPUT-PROPERTIES-UNREADABLE";
    g.reason = perr;
    return g;
  }
  const auto it = kv.find("BUNIT");
  if (it == kv.end() || it->second.empty()) {
    g.input_error = true;
    g.code = "P3-INPUT-BUNIT-MISSING";
    g.reason = "input HiPS signal/properties declares no BUNIT; unit undecidable"
               " (FZ-BUNIT-SEMANTICS; 禁按 ADU 猜测)";
    return g;
  }
  g.bunit_raw = it->second;
  BunitProvenance prov;
  {
    const auto ps = kv.find("ASTROCS_PIXEL_SEMANTICS");
    if (ps != kv.end() && !ps->second.empty()) {
      if (ps->second == "surface_brightness")
        prov.pixel_semantics = PixelSemantics::SurfaceBrightness;
      else if (ps->second == "integrated_flux")
        prov.pixel_semantics = PixelSemantics::IntegratedFlux;
      else {
        g.code = "P3-INPUT-PIXEL-SEMANTICS-UNSUPPORTED";
        g.reason = "ASTROCS_PIXEL_SEMANTICS '" + ps->second +
                   "' is not a declared pixel semantics"
                   " (surface_brightness|integrated_flux)";
        return g;
      }
    }
    const auto pp = kv.find("ASTROCS_PIXEL_AREA_POWER");
    if (pp != kv.end() && !pp->second.empty()) {
      try {
        size_t used = 0;
        const int v = std::stoi(pp->second, &used);
        if (used != pp->second.size()) throw std::invalid_argument("trailing chars");
        prov.pixel_area_power_present = true;
        prov.pixel_area_power = v;
      } catch (...) {
        g.input_error = true;
        g.code = "P3-INPUT-PIXEL-AREA-POWER-UNPARSABLE";
        g.reason = "ASTROCS_PIXEL_AREA_POWER '" + pp->second + "' is not an integer";
        return g;
      }
    }
  }
  // 冻结串逐字比较（仅去空白; 禁大小写/别名/幂次"猜测"）: "ADU/sr" 是唯一
  // 显式可判的面亮度 BUNIT 串; 裸 ADU 须 provenance 声明补足 (FZ-BUNIT-SEMANTICS (b))。
  std::string norm;
  for (char c : g.bunit_raw) {
    if (c != ' ' && c != '\t') norm += c;
  }
  if (norm == kP3BunitSurfaceBrightness) {
    g.ok = true;
    g.bunit_canonical = kP3BunitSurfaceBrightness;
    return g;
  }
  const BunitResolution res = resolve_bunit(g.bunit_raw, prov);
  if (norm == "ADU") {
    if (prov.pixel_semantics == PixelSemantics::SurfaceBrightness &&
        prov.pixel_area_power_present && prov.pixel_area_power == -2) {
      // FZ-BUNIT-SEMANTICS (b): 裸 ADU + 像素语义声明 → 可判为面亮度, 归一为冻结串
      g.ok = true;
      g.bunit_canonical = kP3BunitSurfaceBrightness;
      return g;
    }
    if (prov.pixel_semantics == PixelSemantics::IntegratedFlux) {
      g.code = "P3-INPUT-NOT-SURFACE-BRIGHTNESS";
      g.reason = "BUNIT 'ADU' with ASTROCS_PIXEL_SEMANTICS=integrated_flux resolves to"
                 " integrated flux; export accepts surface-brightness inputs only"
                 " (ASTROCS_DESIGN §6.3)";
      return g;
    }
    // 裸 ADU 而无像素语义声明: 输入**未声明**可判语义 → 输入格式错 (exit 3)
    g.input_error = true;
    g.code = "P3-INPUT-BUNIT-UNDECIDABLE";
    g.reason = "input BUNIT 'ADU' without ASTROCS_PIXEL_SEMANTICS="
               "surface_brightness + ASTROCS_PIXEL_AREA_POWER=-2 is not dimensionally"
               " decidable (FZ-BUNIT-SEMANTICS; 禁按 ADU 猜测); 面亮度输入须逐字写"
               " '" + std::string(kP3BunitSurfaceBrightness) + "' 或补像素语义声明";
    return g;
  }
  // 其余（含可解析的非面亮度单位与冻结词汇表外单位）: 已声明但非面亮度语义 → exit 4
  g.code = res.resolvable ? "P3-INPUT-NOT-SURFACE-BRIGHTNESS"
                          : "P3-INPUT-UNIT-UNSUPPORTED";
  g.reason = "export accepts surface-brightness inputs only (ASTROCS_DESIGN §6.3):"
             " BUNIT '" + g.bunit_raw + "'" +
             (res.resolvable ? (" resolves to '" + res.resolved.canonical() + "'")
                             : std::string(" is outside the frozen unit vocabulary")) +
             " (expected '" + std::string(kP3BunitSurfaceBrightness) + "')";
  return g;
}

// 守卫失败 → 统一错误面（机器错误码落节点 manifest; CLI 退出码按语义:
// input_error → 3（输入缺失/格式错），否则 → 4（科学验证/不变量失败））。
Result<void> p3n_guard_fail(const P3InputUnit& g, Json* man) {
  if (man) {
    (*man)["semantic_code"] = g.code;
    (*man)["input_bunit"] = g.bunit_raw;
    if (g.input_error) (*man)["error_kind"] = "input";
  }
  const ErrorDomain dom =
      g.input_error ? ErrorDomain::DATA : ErrorDomain::SCIENCE_PRECONDITION;
  return Result<void>::fail(Error(dom, g.code + ": " + g.reason));
}

// ── op: properties (ALG-P3-001 唯一真实入口 = 严格 properties 校验 + 实测
//    order/BUNIT + uncertainty 子产品探测) ────────────────────────────────────
Result<void> p3_op_properties(const Json& doc, Json* man) {
  P3nGeom g;
  std::string err;
  if (!p3n_geom(doc, &g, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  // 输入语义守卫（先于任何像素/子产品读取; 非面亮度或单位不可判 →
  // 显式拒且不产任何半成品）
  const P3InputUnit guard = p3n_guard_input_units(g.hips_dir);
  if (!guard.ok) return p3n_guard_fail(guard, man);
  using namespace astrocs::phase3;
  P3Sampler samp{};
  int order = -1;
  std::string bunit, serr;
  const P3ResampleStatus st =
      p3_sampler_open_ex(g.hips_dir.c_str(), &samp, &order, &bunit, &serr);
  if (st != P3_RS_OK) {
    // SMOKE-001 D11: 打开失败的两条路径都指向**输入 HiPS 产品**不可用
    // （P3_RS_PARAM = signal/properties 缺失或非法，p3_resample.cpp:271-272；
    //  P3_RS_IO = 数据集打开失败）⇒ 按 §6.3「3 = 输入缺失/格式错」标记
    // error_kind=input，由 CLI 退出码映射统一收敛（runtime_client.cpp:388），
    // 与 mosaic 同类输入缺失同为 3（跨命令同失败同码）。
    (*man)["error_kind"] = "input";
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

  // BUNIT 一致性: reader 解析出的 BUNIT 必须与守卫所见逐字一致
  // （两份解析面分叉 = 产品被并发改写/解析漂移 → 显式拒，禁静默采信任一）。
  if (!bunit.empty() && bunit != guard.bunit_raw)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "BUNIT drift between properties parse ('" + bunit + "') and guard ('" +
        guard.bunit_raw + "')"));
  const std::string path = g.out_dir + "/p3_props.json";
  Json props{{"schema", "DATA-P3-PROPS"},
             {"hips_dir", g.hips_dir},
             {"hips_order", order},
             {"tile_width", 512},
             // 下游（resample/writer/FITS BUNIT）统一消费冻结单位表的
             // canonical 面亮度串; 输入原始声明另存 bunit_input 供审计。
             {"bunit", guard.bunit_canonical},
             {"bunit_input", guard.bunit_raw},
             {"pixel_semantics", "surface_brightness"},
             {"pixel_area_power", -2},
             {"variance_propagation", "C_out = R C_in R^T"},
             {"variance_available", src == P3_UNC_VARIANCE},
             {"ivar_available", src == P3_UNC_IVAR},
             {"uncertainty_source",
              src == P3_UNC_VARIANCE ? "variance"
                                     : (src == P3_UNC_IVAR ? "ivar" : "none")}};
  // §9 原子提交（临时文件 + fsync + rename）: 不留半成品
  if (!p2_write_text_atomic(path, props.dump(2) + "\n"))
    return Result<void>::fail(Error(ErrorDomain::IO, "p3_props.json atomic write failed"));
  (*man)["props_artifact"] = path;
  (*man)["artifacts"] = Json::array({path});
  (*man)["hips_order"] = order;
  (*man)["bunit"] = guard.bunit_canonical;
  (*man)["bunit_input"] = guard.bunit_raw;
  (*man)["pixel_semantics"] = "surface_brightness";
  (*man)["pixel_area_power"] = -2;
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
  // ── EXPORT-CROP-01：裁剪窗口解析（默认不裁剪）──────────────────────────
  // 画幅 WCS 已构造（**未裁剪**）⇒ 在此把 crop 解析成整数像素窗口并随 wcs plan
  // 落盘；writer/verify 只读该窗口（单一事实源，禁各自重算）。resample 仍按
  // 未裁剪画幅产出平面 ⇒ 窗口内像素与不裁剪时**逐位相同**（裁剪只能裁、不改数值）。
  astrocs::phase3::P3CropWindow crop;
  std::string crop_mode, crop_err;
  if (!p3n_crop_window(doc, wcs, g.w, g.h, &crop, &crop_mode, &crop_err))
    return Result<void>::fail(Error(ErrorDomain::DATA, crop_err));
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
            {"crop", p3n_crop_plan_json(crop, crop_mode)},
            {"fits_keywords", p3_wcs_fits_keywords(&wcs)}};
  // §9 原子提交
  if (!p2_write_text_atomic(path, plan.dump(2) + "\n"))
    return Result<void>::fail(Error(ErrorDomain::IO, "p3_wcs.json atomic write failed"));
  (*man)["wcs_plan_artifact"] = path;
  (*man)["artifacts"] = Json::array({path});
  return Result<void>::success();
}

// ── op: resample (ALG-P3-003 唯一真实入口 = order 选择 + 反向映射采样 +
//    DATA-P3-UNC-001 §30.4 不确定度传播; 重计算面, 行带 work unit 经 Runtime
//    唯一 executor 执行 — RT-001) ─
Result<void> p3_op_resample(const Json& doc, Json* man, uint32_t cap,
                            RunContext* ctx) {
  // ── FZ-P3-MODES / G-P3-MODE: output_mode 生产消费（三模式显式声明）───────
  // 缺失即拒绝（禁静默按 surface_brightness）; token 经冻结模式表解析
  // （p3rsmp::parse_mode: legacy/deferred 词一律拒）。
  if (!doc.contains("output_mode") || !doc["output_mode"].is_string() ||
      doc["output_mode"].get<std::string>().empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "export config missing output_mode (FZ-P3-MODES: 模式未声明 -> REJECT;"
        " 禁静默按 surface_brightness)"));
  const std::string omode = doc["output_mode"].get<std::string>();
  astrocs::p3rsmp::P3Mode out_mode = astrocs::p3rsmp::P3Mode::SurfaceBrightness;
  if (astrocs::p3rsmp::parse_mode(omode, &out_mode) != astrocs::p3rsmp::Status::Ok)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "output_mode '" + omode + "' is not a declared FZ-P3-MODES token"
        " (surface_brightness | point_source_flux | visualization);"
        " legacy/deferred tokens rejected (禁宽松解析)"));
  if (out_mode == astrocs::p3rsmp::P3Mode::PointSourceFlux)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "output_mode point_source_flux is not implemented in this build"
        " (needs PSF/effective PSF/Q/W recompute on the output frame;"
        " FZ-P3-QW-RECOMPUTE); refusing a silent surface_brightness downgrade"));
  // 可测量性由模式**唯一决定**（FZ-P3-MODES: visualization 恒为显示型降级,
  // surface_brightness 为唯一测量面）—— 不新造配置键（CLI 配置合同无
  // measurement_capable, 自造同义键 = 死键违规, AGENTS §6）。直调 IR/透传形态若
  // 自带该键, 只作一致性断言: 与模式推导不符即拒（禁把显示产品冒充测量产品）。
  const bool measurement_capable =
      (out_mode == astrocs::p3rsmp::P3Mode::SurfaceBrightness);
  if (doc.contains("measurement_capable")) {
    if (!doc["measurement_capable"].is_boolean())
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "measurement_capable must be boolean when present"));
    if (doc["measurement_capable"].get<bool>() != measurement_capable)
      return Result<void>::fail(Error(ErrorDomain::SCIENCE_PRECONDITION,
          std::string("measurement_capable=") +
          (measurement_capable ? "true" : "false") + " is inconsistent with"
          " output_mode '" + omode + "' (FZ-P3-MODES: 可测量性由模式唯一决定,"
          " visualization 不得冒充测量产品)"));
  }
  // surface_brightness = 唯一测量面（visualization 不产测量层）
  const bool measure_face = measurement_capable;
  P3nGeom g;
  std::string err;
  if (!p3n_geom(doc, &g, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  // 输入语义守卫（与 properties 节点同源同判; 旁路直调本节点也不放行）
  const P3InputUnit guard = p3n_guard_input_units(g.hips_dir);
  if (!guard.ok) return p3n_guard_fail(guard, man);
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
  // p3_props.json 的 canonical 单位与实时守卫必须一致（上游 artifact
  // 漂移 → 显式拒, 禁把旧声明当事实）。
  const std::string props_bunit = props.value("bunit", std::string());
  if (props_bunit != guard.bunit_canonical)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "p3_props.json bunit drift: '" + props_bunit + "' != live input '" +
        guard.bunit_canonical + "' (FIX-402 输入语义守卫)"));
  const bool input_unc_available = (src != P3_UNC_NONE);
  // FZ-P3-MODES: visualization 不产出/不消费测量层（禁写 VARIANCE/IVAR 作测量层）
  const bool unc_available = measure_face && input_unc_available;
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
  // ── P3-STREAM-01：子块流式（ASTROCS_DESIGN §8.3 export 行）──────────────
  // 驻留面 = 单个子块 × 并发任务数（**与总图大小无关**）：子块按 (by,bx) 行
  // 主序产出，经 aio 位置写直接落进 p3_resampled.bin 的平面行主序区间；
  // 平面布局与整幅实现逐字节相同（plane 连续拼接，行主序），故下游 writer/
  // verify 与既有 Oracle 不受影响。
  int sb = kP3DefaultSubBlockPx;
  if (!p3n_sub_block_px(doc, &sb, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  const int nbx = (g.w + sb - 1) / sb;
  const int nby = (g.h + sb - 1) / sb;
  const long nblocks = (long)nbx * (long)nby;
  const std::string bin_path = g.out_dir + "/p3_resampled.bin";
  const std::string bin_tmp = aio_atomic::make_tmp_path(bin_path);
  aio_atomic::make_dirs(g.out_dir);
  aio_atomic::remove_file(bin_tmp);
  aio_file::RandomWriter binw;
  {
    std::string werr;
    const uint64_t plane_bytes = (uint64_t)nelem * sizeof(float);
    const uint64_t total_bytes = plane_bytes * (unc_available ? 4u : 2u);
    if (!binw.open(bin_tmp.c_str(), &werr) || !binw.reserve(total_bytes, &werr)) {
      binw.close();
      aio_atomic::remove_file(bin_tmp);
      return Result<void>::fail(Error(ErrorDomain::IO,
          "p3_resampled.bin (tmp) open/reserve failed: " + werr));
    }
  }
  std::atomic<long long> missing_px{0};
  std::atomic<int> corrupt{-1};          // 行号 (u 产品损坏 §30.4-3)
  std::atomic<bool> cancelled{false};    // P30: 协作取消 (子块循环安全点)
  std::atomic<bool> write_failed{false}; // 位置写失败 (fail-closed, 不发布)
  const int npts = (g.sampler == "nearest") ? 1 : 4;

  // 子块位置写：平面 p 的第 y 行第 x 列 = (p*nelem + y*g.w + x) 个 float。
  // 行内连续 ⇒ 每行一次位置写（与子块 x0 对齐，行区间互不重叠）。
  auto write_block_planes = [&](int x0, int y0, int bw, int bh,
                                const float* ps, const float* pc,
                                const float* pv, const float* pi) -> bool {
    std::string werr;
    for (int yy = 0; yy < bh; ++yy) {
      const uint64_t row = (uint64_t)(y0 + yy) * (uint64_t)g.w + (uint64_t)x0;
      const std::size_t nbytes = (std::size_t)bw * sizeof(float);
      const float* planes[4] = {ps, pc, pv, pi};
      const int nplanes = unc_available ? 4 : 2;
      for (int p = 0; p < nplanes; ++p) {
        if (!planes[p]) continue;
        const uint64_t off =
            ((uint64_t)p * (uint64_t)nelem + row) * sizeof(float);
        if (!binw.write_at(off, planes[p] + (std::size_t)yy * bw, nbytes, &werr))
          return false;
      }
    }
    return true;
  };

  auto worker = [&](long b0, long b1) {
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
    std::vector<float> bsig, bcov, bvar, bivar;
    for (long b = b0; b < b1 && corrupt.load() == -1 && !write_failed.load(); ++b) {
      // P30: 协作取消安全点（与 p3_session 路径同款语义）。每个子块检查一次，
      // 取消后立即置位并跳出，由下方统一 fail-closed 返回，**不落任何半成品**
      // （bin 先写临时对象，仅在全部子块成功后才 fsync + rename）。
      if (ctx && ctx->cancelled()) { cancelled.store(true); break; }
      const int bx = (int)(b % nbx);
      const int by = (int)(b / nbx);
      const int x0 = bx * sb;
      const int y0 = by * sb;
      const int bw = std::min(sb, g.w - x0);
      const int bh = std::min(sb, g.h - y0);
      const std::size_t bn = (std::size_t)bw * (std::size_t)bh;
      bsig.assign(bn, std::nanf(""));
      bcov.assign(bn, 0.0f);
      if (unc_available) {
        bvar.assign(bn, std::nanf(""));
        bivar.assign(bn, std::nanf(""));
      }
      for (int yy = 0; yy < bh && corrupt.load() == -1; ++yy) {
        for (int xx = 0; xx < bw; ++xx) {
          const int x = x0 + xx, y = y0 + yy;
          double px_ra = 0, px_dec = 0;
          if (p3_wcs_pix2world(&w_wcs, (double)x, (double)y, &px_ra, &px_dec) !=
              P3_WCS_OK)
            continue;   // 半球外像素保持 NaN/0
          const std::size_t i = (std::size_t)yy * (std::size_t)bw + (std::size_t)xx;
          float v = 0;
          int c = 0;
          double w[4] = {0, 0, 0, 0};
          uint64_t lf[4] = {0, 0, 0, 0};
          const P3ResampleStatus rst =
              (g.sampler == "nearest")
                  ? p3_sample_nearest_ex(&w_samp, px_ra, px_dec, &v, &c, &lf[0])
                  : p3_sample_bilinear_ex(&w_samp, px_ra, px_dec, &v, &c, w, lf);
          if (rst != P3_RS_OK) continue;
          bsig[i] = (c == 1) ? v : std::nanf("");
          bcov[i] = (c == 1) ? 1.0f : 0.0f;
          if (!unc_available) continue;
          if (c == 0) {   // 无覆盖 → var/ivar=NaN + C=0 (signal NaN 同态)
            bvar[i] = std::nanf("");
            bivar[i] = std::nanf("");
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
            bvar[i] = std::nanf("");
            bivar[i] = std::nanf("");
            continue;
          }
          bvar[i] = std::isnan(u_out) ? std::nanf("") : static_cast<float>(u_out);
          bivar[i] =
              std::isnan(u_out)
                  ? std::nanf("")
                  : (u_out > 0.0 ? static_cast<float>(1.0 / u_out)
                                 : (u_out == 0.0 ? 0.0f : std::nanf("")));
        }
      }
      if (corrupt.load() != -1 || cancelled.load()) break;
      // 子块 → p3_resampled.bin 位置写（行内连续；行区间互不重叠 ⇒ 与写出
      // 顺序无关，1/N worker 逐位一致）
      if (!write_block_planes(x0, y0, bw, bh, bsig.data(), bcov.data(),
                              unc_available ? bvar.data() : nullptr,
                              unc_available ? bivar.data() : nullptr)) {
        write_failed.store(true);
        break;
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
  // fail-closed: 每个子块 work unit 完成计数; wait_all 后已执行数 ≠ 提交数（取消
  // 丢弃/注入缺陷）→ 节点显式失败, 不落任何伪产物。实测观测: work_units（提交数）、
  // band_active_peak（并发峰值 = 实测 active threads）、band_executed。
  const uint32_t nw =
      (cap >= 2 && nblocks >= 2)
          ? static_cast<uint32_t>(std::min<long>(cap, nblocks))
          : 1u;
  std::atomic<uint32_t> band_active{0};
  std::atomic<uint32_t> band_active_peak{0};
  std::atomic<uint32_t> band_executed{0};
  std::shared_ptr<CpuHeavyExecutor> band_pool;
  if (ctx && nw >= 2) band_pool = rt::shared_work_executor(ctx->budget());
  auto band_task = [&](long b0, long b1, RunContext&) {
    // 故障注入 (ASTROCS_RT001_FAULT): resample_drop_band 模拟"首个 work unit 被
    // 取消丢弃"缺陷 → fail-closed 路径必须拒绝 (P2-002/P3-002 注入先例同构)。
    const char* fault = std::getenv("ASTROCS_RT001_FAULT");
    if (fault && std::strcmp(fault, "resample_drop_band") == 0 && b0 == 0) {
      return;  // 首个 work unit 被吞: 不执行、不计数 (确定性: 无时序竞争)
    }
    const uint32_t cur = band_active.fetch_add(1) + 1;
    uint32_t p = band_active_peak.load();
    while (cur > p && !band_active_peak.compare_exchange_weak(p, cur)) {}
    worker(b0, b1);
    band_active.fetch_sub(1);
    band_executed.fetch_add(1);
  };
  // 子块区间按 [k*nblocks/nw, (k+1)*nblocks/nw) 均分（连续区间，负载均衡且
  // 与 worker 数无关的确定性划分）。
  auto block_range = [&](uint32_t k, long* b0, long* b1) {
    *b0 = (long)((uint64_t)nblocks * k / nw);
    *b1 = (long)((uint64_t)nblocks * (k + 1) / nw);
  };
  RunContext no_ctx;  // 池外调用面的空观测上下文（串行/防御路径专用）
  if (nw >= 2) {
    for (uint32_t k = 0; k < nw; ++k) {
      long b0 = 0, b1 = 0;
      block_range(k, &b0, &b1);
      if (band_pool) {
        band_pool->enqueue([&band_task, b0, b1](RunContext& c) {
          band_task(b0, b1, c);
        });
      } else {
        // 防御路径（池不可得且 cap>=2, 生产不可达）: 串行执行全部子块,
        // 不自建线程池（禁止回退到调用点 spawn）—— 完整性与确定性优先。
        band_task(b0, b1, no_ctx);
      }
    }
    if (band_pool) band_pool->wait_all();
  } else {
    band_task(0, nblocks, no_ctx);
  }
  p3_uncertainty_close(&u_samp);
  p3_sampler_close(&samp);
  // P30: 取消优先于"work unit 被丢弃"判定 (两者都 fail-closed; 取消更具体)。
  // 取消时 p3_resampled.bin 只在临时对象里 → 无半成品产物。
  if (cancelled.load() || (ctx && ctx->cancelled())) {
    binw.close();
    aio_atomic::remove_file(bin_tmp);
    return Result<void>::fail(Error(ErrorDomain::CANCELLED,
        "resample cancelled by cancellation token: fail-closed, no partial product"));
  }
  if (band_executed.load() != nw) {
    binw.close();
    aio_atomic::remove_file(bin_tmp);
    return Result<void>::fail(Error(ErrorDomain::CANCELLED,
        "resample sub-block work units dropped (executed " +
            std::to_string(band_executed.load()) + "/" + std::to_string(nw) +
            "): cancelled or fault-injected; fail-closed, no partial product"));
  }
  if (man) {
    (*man)["work_units"] = nw;                      // 提交的 work unit 数（计划面）
    (*man)["band_executed"] = band_executed.load(); // 实测完成数
    (*man)["band_active_peak"] = band_active_peak.load();  // 实测并发峰值
    // P3-STREAM-01：子块流式实测证据（判据 CHK-P3-EXPORT-STREAM-PROD 消费）。
    (*man)["sub_block_px"] = sb;
    (*man)["sub_blocks"] = nblocks;
    (*man)["sub_block_streaming"] = true;
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

  if (write_failed.load()) {
    binw.close();
    aio_atomic::remove_file(bin_tmp);
    return Result<void>::fail(Error(ErrorDomain::IO,
        "p3_resampled.bin positional write failed (fail-closed, no partial product)"));
  }
  // typed artifact: p3_resampled.bin = 平面连续拼接 (f32: sig, cov[, var, ivar])
  // —— 子块已按平面行主序区间**位置写**进临时对象；此处只做 §9 原子提交
  //（fflush → fsync → rename），不再有整幅平面的写出步骤。
  {
    std::string aerr;
    if (!binw.flush_and_sync(&aerr)) {
      aio_atomic::remove_file(bin_tmp);
      return Result<void>::fail(Error(ErrorDomain::IO,
          "p3_resampled.bin flush/fsync failed: " + aerr));
    }
    if (aio_atomic::atomic_replace(bin_tmp, bin_path) != 0) {
      aio_atomic::remove_file(bin_tmp);
      return Result<void>::fail(Error(ErrorDomain::IO,
          "p3_resampled.bin atomic replace failed: " + bin_path));
    }
  }
  // 完整性锚: bin 流式 sha256 (禁前缀/假哈希; 大图流式不整载)
  // CLEAN-403: 摘要经 aio 唯一实现 (aio_file::sha256_hex, 64 KiB 分块)。
  std::string bin_sha;
  if (!aio_file::sha256_hex(bin_path.c_str(), &bin_sha))
    return Result<void>::fail(Error(ErrorDomain::IO,
        "p3_resampled.bin sha256 failed: " + bin_path));
  Json planes = Json::array();
  planes.push_back("signal");
  planes.push_back("coverage");
  if (unc_available) { planes.push_back("variance"); planes.push_back("ivar"); }
  // BUNIT 一致性: reader 解析面与守卫面必须逐字一致。
  if (!bunit.empty() && bunit != guard.bunit_raw)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "BUNIT drift between properties parse ('" + bunit + "') and guard ('" +
        guard.bunit_raw + "')"));
  const std::string json_path = g.out_dir + "/p3_resampled.json";
  Json res{{"schema", "DATA-P3-RES"},
           {"width_px", g.w},
           {"height_px", g.h},
           {"order_sel", order_sel},
           {"sampler", g.sampler},
           {"bitpix", g.bitpix},
           // 单位/像素语义（下游 writer/FITS BUNIT 唯一来源; 禁 loose default）
           {"bunit", guard.bunit_canonical},
           {"bunit_input", guard.bunit_raw},
           {"pixel_semantics", "surface_brightness"},
           {"pixel_area_power", -2},
           // FZ-P3-MODES: 输出模式显式随产物落盘（visualization = 不可测量）
           {"output_mode", omode},
           {"measurement_capable", measure_face},
           // FZ-FORMULA-COV-PROP / docs/contracts/DATA_SEMANTICS.md §28.6:
           // variance/ivar 显式消费, 按 C_out = R C_in R^T 传播（输入 C_in 对角
           // 时逐像素 [R C R^T]_ii = Σ_k c_k² u_k; nearest: u_in）。
           {"variance_propagation", "C_out = R C_in R^T"},
           {"variance_propagation_rule",
            g.sampler == "nearest"
                ? std::string("nearest: [R C_in R^T]_ii = u_in")
                : std::string("bilinear_4quad: [R C_in R^T]_ii = sum_k c_k^2 u_k")},
           {"input_covariance_representation", "diagonal"},
           {"output_covariance_representation", "diagonal"},
           {"planes", planes},
           {"uncertainty_available", unc_available},
           {"input_uncertainty_available", input_unc_available},
           {"uncertainty_source", unc_available ? live_src : std::string("none")},
           {"input_uncertainty_source", live_src},
           {"uncertainty_not_consumed_reason",
            (!measure_face && input_unc_available)
                ? std::string("visualization: measurement layers are not produced"
                              " (measurement_capable=false)")
                : std::string()},
           {"uncertainty_missing_pixels", missing_px.load()},
           {"bin", "p3_resampled.bin"},
           {"bin_sha256", bin_sha}};
  // §9 原子提交
  if (!p2_write_text_atomic(json_path, res.dump(2) + "\n"))
    return Result<void>::fail(Error(ErrorDomain::IO, "p3_resampled.json atomic write failed"));
  (*man)["resampled_artifact"] = json_path;
  (*man)["artifacts"] = Json::array({json_path, bin_path});
  (*man)["order_sel"] = order_sel;
  (*man)["uncertainty_available"] = unc_available;
  (*man)["input_uncertainty_available"] = input_unc_available;
  (*man)["uncertainty_source"] = live_src;
  (*man)["uncertainty_missing_pixels"] = missing_px.load();
  (*man)["output_mode"] = omode;
  (*man)["measurement_capable"] = measure_face;
  (*man)["bunit"] = guard.bunit_canonical;
  (*man)["pixel_semantics"] = "surface_brightness";
  (*man)["pixel_area_power"] = -2;
  return Result<void>::success();
}

// ── P3-STREAM-01：生产 FITS 子块汇（ExportSink 唯一生产实现）───────────────
// 调度器按子块索引升序回调本汇；本汇把子块写进**当前 HDU** 的数据区
// （cfitsio fits_write_subset）。HDU 的建立与头组装由调用方在首个 write_block
// 之前完成（begin_hdu），发布（fsync + 原子 rename）在全部 HDU 写完后由调用方
// 统一执行 —— 多 HDU 的 FITS 产品只有一个发布点，原子性不变。
class P3FitsSubsetSink : public astrocs::core::ExportSink {
 public:
  explicit P3FitsSubsetSink(astrocs::phase3::P3FitsStream* w) : w_(w) {}
  bool open(std::string*) override { return true; }   // HDU 由调用方 begin_hdu
  bool write_sub_block(std::size_t, int x0, int y0, int w, int h,
                       const double* data, std::string* err) override {
    fb_.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    // f64 → f32：bin 平面本来就是 f32，double 可精确表示任何 float ⇒ 无精度损失
    for (std::size_t i = 0; i < fb_.size(); ++i)
      fb_[i] = static_cast<float>(data[i]);
    if (w_->write_block(x0, y0, w, h, fb_.data()) != astrocs::phase3::P3_OUT_OK) {
      if (err)
        *err = std::string("fits_write_subset: ") +
               astrocs::phase3::p3_output_last_error();
      return false;
    }
    return true;
  }
  bool finish(std::string*) override { return true; }   // 发布由调用方统一执行
  void abort() override {}

 private:
  astrocs::phase3::P3FitsStream* w_;
  std::vector<float> fb_;
};
// ── op: writer (ALG-P3-004 唯一真实入口 = 原子流式 FITS 发布) ────────────────
Result<void> p3_op_writer(const Json& doc, Json* man, uint32_t cap,
                          RunContext* ctx) {
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
  // ── EXPORT-CROP-01：裁剪窗口（默认不裁剪 ⇒ 与既有整幅导出逐位相同）──────
  // 写出画幅 = 窗口；输出 WCS = 未裁剪画幅 WCS 在窗口上的**精确限制**
  // （CRVAL/CD 逐位不变、CRPIX 减整数窗口原点）⇒ 窗口内像素逐位等于整幅导出的
  // 同位置像素（裁剪只能裁、不能改数值）。resample 仍按未裁剪画幅产出平面，
  // 因此本节点是裁剪的**唯一**写出点。
  astrocs::phase3::P3CropWindow crop;
  if (!p3n_crop_from_plan(plan, g.w, g.h, &crop, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  P3WcsDescriptor owcs{};
  if (astrocs::phase3::p3_crop_apply_wcs(&wcs, crop, &owcs) != astrocs::phase3::P3_CROP_OK)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "crop: failed to derive the cropped WCS from the frame WCS"));
  const int ow = owcs.width_px;
  const int oh = owcs.height_px;
  const std::string crop_mode =
      (plan.contains("crop") && plan["crop"].is_object() &&
       plan["crop"].contains("crop_form") && plan["crop"]["crop_form"].is_string())
          ? plan["crop"]["crop_form"].get<std::string>()
          : std::string();
  const long nelem = (long)g.w * g.h;        // 平面元素数（未裁剪画幅）
  const long onelem = (long)ow * (long)oh;   // 产品元素数（裁剪后画幅）
  const bool unc = res.value("uncertainty_available", false);
  // 输出面单位必须来自 resample 的 canonical 面亮度声明（禁 loose default
  // "ADU"）; 输出模式/可测量性同样必须显式随产物落盘（FZ-P3-MODES）。
  const std::string bunit_canon = res.value("bunit", std::string());
  if (bunit_canon != kP3BunitSurfaceBrightness)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "p3_resampled.json bunit '" + bunit_canon +
        "' != surface-brightness canonical '" +
        std::string(kP3BunitSurfaceBrightness) +
        "' (FZ-BUNIT-SEMANTICS; 禁 loose default)"));
  const std::string omode = res.value("output_mode", std::string());
  if (omode != "surface_brightness" && omode != "visualization")
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "p3_resampled.json output_mode '" + omode +
        "' missing/invalid (FZ-P3-MODES; 模式必须显式声明)"));
  const bool measurement_capable = res.value("measurement_capable", false);
  if (omode == "visualization" && measurement_capable)
    return Result<void>::fail(Error(ErrorDomain::SCIENCE_PRECONDITION,
        "visualization product must not be measurement_capable (FZ-P3-MODES)"));
  if (omode == "surface_brightness" && !measurement_capable)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "surface_brightness product must declare measurement_capable=true"));
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
  const std::string bin_p = g.out_dir + "/p3_resampled.bin";
  if (!aio_fs::exists(bin_p))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream artifact missing: p3_resampled.bin"));
  // ── P3-STREAM-01：导出编排参数（资源/编排键，非科学键）───────────────────
  // SCHEDULER_CONTRACT §3：分块大小与队列深度由配置/资源门决定，禁硬编码。
  int sb = kP3DefaultSubBlockPx;
  if (!p3n_sub_block_px(doc, &sb, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  const int64_t qd64 = doc.value("queue_depth", (int64_t)kP3DefaultQueueDepth);
  if (qd64 < 1 || qd64 > 64)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "queue_depth must be in [1,64] (有界队列深度守卫)"));
  const int qd = (int)qd64;
  // 故障注入（门禁负例；生产默认不可达）：ASTROCS_P3_EXPORT_FAULT=
  // whole_frame_resident ⇒ 走「整幅平面驻留 + 单次 fits_write_pix」的**改前形态**
  // 参考路径。它同时是：① 判据 CHK-P3-EXPORT-STREAM-RSS 的阳性对照（整幅驻留
  // ⇒ 必须判红）；② 产品语义逐字节对照的参考实现（与流式路径产物必须逐字节
  // 相同）。生产环境不设该变量 ⇒ 不可达（ASTROCS_RT001_FAULT 同款形态）。
  const char* export_fault = std::getenv("ASTROCS_P3_EXPORT_FAULT");
  const bool whole_frame =
      (export_fault != nullptr &&
       std::strcmp(export_fault, "whole_frame_resident") == 0);
  P3OutputResult ores{};
  long covn = 0;
  std::size_t peak_inflight = 0;
  std::size_t stream_blocks = 0;
  if (whole_frame) {
    // ── 参考路径（P3-STREAM-01 改前形态）：整幅驻留 ─────────────────────
    std::vector<float> sig((size_t)nelem), cov((size_t)nelem);
    std::vector<float> var_p, ivar_p;
    if (unc) { var_p.resize((size_t)nelem); ivar_p.resize((size_t)nelem); }
    {
      const std::size_t plane_bytes = sizeof(float) * static_cast<std::size_t>(nelem);
      std::string pbuf;
      uint64_t off = 0;
      auto read_plane = [&](void* dst) -> bool {
        if (!aio_file::read_range(bin_p.c_str(), off, plane_bytes, &pbuf)) return false;
        std::memcpy(dst, pbuf.data(), plane_bytes);
        off += plane_bytes;
        return true;
      };
      if (!read_plane(sig.data()) || !read_plane(cov.data()) ||
          (unc && (!read_plane(var_p.data()) || !read_plane(ivar_p.data()))))
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "p3_resampled.bin truncated (planes vs manifest drift)"));
    }
    // 裁剪：整幅驻留参考路径同样只写出窗口（与流式路径逐字节一致；
    // 该路径只在 ASTROCS_P3_EXPORT_FAULT 下可达，必须与生产路径同产品语义）
    std::vector<float> wsig, wcov, wvar, wivar;
    const float* psig = sig.data();
    const float* pcov = cov.data();
    const float* pvar = unc ? var_p.data() : nullptr;
    const float* pivar = unc ? ivar_p.data() : nullptr;
    if (crop.active) {
      p3n_window_plane(sig.data(), g.w, crop, &wsig);
      p3n_window_plane(cov.data(), g.w, crop, &wcov);
      if (unc) {
        p3n_window_plane(var_p.data(), g.w, crop, &wvar);
        p3n_window_plane(ivar_p.data(), g.w, crop, &wivar);
      }
      psig = wsig.data();
      pcov = wcov.data();
      pvar = unc ? wvar.data() : nullptr;
      pivar = unc ? wivar.data() : nullptr;
    }
    const P3OutputStatus ost = p3_output_write_atomic_ex(
        psig, pcov, pvar, pivar, ow, oh, &owcs,
        bunit_canon.c_str(), fits_path.c_str(), &prov, g.bitpix, -1, &ores);
    if (ost != P3_OUT_OK)
      return Result<void>::fail(Error(ErrorDomain::IO,
          "p3_output_write_atomic_ex failed (status " +
              std::to_string((int)ost) + ")"));
    for (long i = 0; i < onelem; ++i) if (pcov[(size_t)i] > 0.5f) ++covn;
  } else {
    // ── 生产路径：子块流式（ExportStreamScheduler + FITS 子集写）─────────
    // 驻留面 = 单个子块 × 在途上限（2·queue_depth），**与 W×H 无关**。
    const int nplanes = unc ? 4 : 2;
    std::atomic<bool> read_failed{false};
    P3FitsStream fs;
    if (fs.open(fits_path.c_str(), &owcs, ow, oh, g.bitpix,
                bunit_canon.c_str(), &prov) != P3_OUT_OK) {
      return Result<void>::fail(Error(ErrorDomain::IO,
          std::string("p3_stream open failed: ") + p3_output_last_error()));
    }
    for (int plane = 0; plane < nplanes; ++plane) {
      if (fs.begin_hdu(plane) != P3_OUT_OK) {
        fs.abort();
        return Result<void>::fail(Error(ErrorDomain::IO,
            std::string("p3_stream begin_hdu failed: ") + p3_output_last_error()));
      }
      // 读子块：平面 p 的第 y 行第 x 列 = (p·nelem + y·W + x) 个 float（行内连续）
      // EXPORT-CROP-01：子块坐标在**裁剪后画幅**上；平面读取按窗口原点平移
      //（裁剪只改读取起点与写出画幅，不改任何像素值 ⇒ 与不裁剪逐位相同）。
      const int gx = (int)crop.x0;
      const int gy = (int)crop.y0;
      auto read_fn = [&](int x0, int y0, int w, int h, double* out) {
        const uint64_t plane_off = (uint64_t)plane * (uint64_t)nelem;
        std::string buf;
        for (int yy = 0; yy < h; ++yy) {
          const uint64_t row = plane_off + (uint64_t)(gy + y0 + yy) * (uint64_t)g.w +
                               (uint64_t)(gx + x0);
          const std::size_t nbytes = (std::size_t)w * sizeof(float);
          if (!aio_file::read_range(bin_p.c_str(), row * sizeof(float), nbytes, &buf)) {
            read_failed.store(true);
            for (int xx = 0; xx < w; ++xx)
              out[(std::size_t)yy * w + xx] = std::nan("");
            continue;
          }
          const float* src = reinterpret_cast<const float*>(buf.data());
          for (int xx = 0; xx < w; ++xx)
            out[(std::size_t)yy * w + xx] = (double)src[xx];
        }
      };
      P3FitsSubsetSink sink(&fs);
      astrocs::core::ExportStreamConfig sc;
      sc.workers = (cap >= 1) ? (int)cap : 1;   // 线程预算来自资源门（非字面量）
      sc.sub_block_px = sb;
      sc.queue_depth = qd;
      sc.sink = &sink;
      sc.header_ready = true;   // 头已由 fs.open/begin_hdu 在首像素写出前组装
      sc.output_path = fits_path;
      sc.sub_block_fn = read_fn;
      astrocs::core::ExportStreamScheduler sched(sc);
      sched.set_image(ow, oh);   // EXPORT-CROP-01: 调度网格 = 裁剪后画幅
      const astrocs::core::ExportOutcome so = sched.run();
      if (ctx != nullptr && ctx->cancelled()) {
        fs.abort();
        return Result<void>::fail(Error(ErrorDomain::CANCELLED,
            "writer cancelled by cancellation token: fail-closed, no partial product"));
      }
      if (!so.ok) {
        fs.abort();
        return Result<void>::fail(Error(ErrorDomain::IO,
            "ExportStreamScheduler run failed (plane " + std::to_string(plane) +
                "): " + so.error));
      }
      if (read_failed.load()) {
        fs.abort();
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "p3_resampled.bin truncated (planes vs manifest drift)"));
      }
      peak_inflight = std::max(peak_inflight, so.peak_resident_bytes);
      stream_blocks += so.sub_blocks;
      if (fs.end_hdu() != P3_OUT_OK) {
        fs.abort();
        return Result<void>::fail(Error(ErrorDomain::IO,
            std::string("p3_stream end_hdu failed: ") + p3_output_last_error()));
      }
    }
    if (fs.publish(&ores) != P3_OUT_OK)
      return Result<void>::fail(Error(ErrorDomain::IO,
          std::string("p3_stream publish failed: ") + p3_output_last_error()));
    // 独立重开对拍（与整幅路径同判据；按子块流式，不整幅驻留）
    {
      P3FitsVerifyStream vs;
      if (vs.open(fits_path.c_str(), &owcs, ow, oh, unc) != P3_OUT_OK)
        return Result<void>::fail(Error(ErrorDomain::IO,
            std::string("p3_stream verify open failed: ") + p3_output_last_error()));
      std::vector<float> fb;
      for (int plane = 0; plane < nplanes; ++plane) {
        for (int y0 = 0; y0 < oh; y0 += sb) {
          for (int x0 = 0; x0 < ow; x0 += sb) {
            const int w = std::min(sb, ow - x0);
            const int h = std::min(sb, oh - y0);
            fb.resize((std::size_t)w * h);
            std::string buf;
            for (int yy = 0; yy < h; ++yy) {
              const uint64_t row = (uint64_t)plane * (uint64_t)nelem +
                                   (uint64_t)(crop.y0 + y0 + yy) * (uint64_t)g.w +
                                   (uint64_t)(crop.x0 + x0);
              if (!aio_file::read_range(bin_p.c_str(), row * sizeof(float),
                                        (std::size_t)w * sizeof(float), &buf))
                return Result<void>::fail(Error(ErrorDomain::DATA,
                    "p3_resampled.bin truncated during reopen verify"));
              std::memcpy(fb.data() + (std::size_t)yy * w, buf.data(),
                          (std::size_t)w * sizeof(float));
            }
            if (vs.check_block(plane, x0, y0, w, h, fb.data()) != P3_OUT_OK)
              return Result<void>::fail(Error(ErrorDomain::IO,
                  std::string("p3_stream verify failed: ") + p3_output_last_error()));
          }
        }
      }
      P3OutputResult vr{};
      if (vs.close(&vr) != P3_OUT_OK)
        return Result<void>::fail(Error(ErrorDomain::IO,
            std::string("p3_stream verify close failed: ") + p3_output_last_error()));
      ores.reopen_ok = vr.reopen_ok;
      ores.coverage_ok = vr.coverage_ok;
      ores.covered_px = vr.covered_px;
      ores.total_px = vr.total_px;
      covn = vr.covered_px;
    }
  }
  const std::string json_path = g.out_dir + "/p3_writer.json";
  // DET-001: 主产物指纹分两层（禁一个名字承载两个含义, UNIFIED_MODEL §字段语义）:
  //   canonical_sha256 — 规范产品哈希（像素数据 + 科学元数据; 排除易变卡/键）
  //                      => 可复现性验收判据（ACCEPTANCE_FINAL D2/E4）;
  //   integrity_sha256 — 整文件 sha256 => 完整性/防改动校验。
  const astrocs::core::CanonicalHashResult canon =
      astrocs::core::canonical_product_hash_file(fits_path);
  if (!canon.ok)
    return Result<void>::fail(Error(ErrorDomain::IO,
        "canonical product hash failed: " + canon.error));
  Json wr{{"schema", "DATA-P3-WRITER-MANIFEST"},
          {"output_fits", fits_path},
          {"canonical_sha256", canon.canonical_sha256},
          {"canonical_hash_spec", astrocs::core::kCanonicalProductHashSpec},
          {"integrity_sha256", std::string(ores.sha256)},
          {"reopen_ok", ores.reopen_ok},
          {"coverage_stats", {{"covered_px", covn}, {"total_px", onelem}}},
          // EXPORT-CROP-01：裁剪窗口随 writer manifest 落盘（下游/验收可独立核对
          // 「写出画幅 = 请求的裁剪尺寸」且像素未被改动）。
          {"crop", p3n_crop_plan_json(crop, crop_mode)},
          {"frame_width_px", g.w},
          {"frame_height_px", g.h},
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
          {"coordinate_frame", "equatorial"},
          {"bunit", bunit_canon},
          {"pixel_semantics", "surface_brightness"},
          {"pixel_area_power", -2},
          {"output_mode", omode},
          {"measurement_capable", measurement_capable},
          {"variance_propagation", res.value("variance_propagation", std::string())},
          {"variance_propagation_rule",
           res.value("variance_propagation_rule", std::string())},
          {"algorithm_id", "ALG-P3-004"},
          {"provider", "baseline"},
          // ── P3-STREAM-01：流式生效证据（判据 CHK-P3-EXPORT-STREAM-PROD/RSS 消费）
          // 生产默认路径必须经 ARCH-504 ExportStreamScheduler 子块流式执行；整幅
          // 驻留参考路径只在故障注入 ASTROCS_P3_EXPORT_FAULT=whole_frame_resident
          // 下可达（此时 whole_frame_fault=true ⇒ 门禁负例判红）。随产品落盘，
          // 供下游与验收独立核对，不依赖进程内状态。
          {"export_stream",
           {{"scheduler", "ExportStreamScheduler"},
            {"schema", "astrocs.export-stream-manifest/v1"},
            {"sub_block_px", sb},
            {"queue_depth", qd},
            {"sub_blocks", stream_blocks},
            {"peak_inflight_bytes", peak_inflight},
            {"whole_frame_fault", whole_frame},
            {"fits_hdu_streaming", !whole_frame}}},
          {"sub_block_px", sb},
          {"whole_frame_fault", whole_frame}};
  // §9 原子提交
  if (!p2_write_text_atomic(json_path, wr.dump(2) + "\n"))
    return Result<void>::fail(Error(ErrorDomain::IO, "p3_writer.json atomic write failed"));
  // A2 单点键名: 节点 manifest 与 CLI 收集端统一用 output_fits_path
  //（旧节点写 output_fits、CLI 只读 output_fits_path → phase3_output role 恒缺）。
  (*man)["output_fits_path"] = fits_path;
  (*man)["writer_artifact"] = json_path;
  (*man)["artifacts"] = Json::array({fits_path, json_path});
  (*man)["canonical_sha256"] = canon.canonical_sha256;
  (*man)["canonical_hash_spec"] = astrocs::core::kCanonicalProductHashSpec;
  (*man)["integrity_sha256"] = std::string(ores.sha256);
  (*man)["uncertainty_available"] = unc;
  // B2-A10（宪章 §4.3）: writer 节点 manifest 携带真实 provenance，供 CLI
  // run manifest 汇总（input_product_hashes / units / coordinate_frames 等）。
  (*man)["run_id"] = run_id_str;
  (*man)["software_version"] = version_str;
  (*man)["source_sha"] = source_sha_str;
  (*man)["input_manifest_hash"] = input_manifest_hash;
  (*man)["coordinate_frame"] = "equatorial";
  (*man)["bunit"] = bunit_canon;
  (*man)["pixel_semantics"] = "surface_brightness";
  (*man)["pixel_area_power"] = -2;
  (*man)["output_mode"] = omode;
  (*man)["measurement_capable"] = measurement_capable;
  // ── P3-STREAM-01：流式生效证据（判据 CHK-P3-EXPORT-STREAM-PROD 消费）──────
  // 生产默认路径必须经 ARCH-504 ExportStreamScheduler；整幅驻留参考路径只在
  // 故障注入下可达（whole_frame_fault=true ⇒ 门禁负例判红）。
  (*man)["export_stream"] = Json{
      {"scheduler", "ExportStreamScheduler"},
      {"schema", "astrocs.export-stream-manifest/v1"},
      {"sub_block_px", sb},
      {"queue_depth", qd},
      {"sub_blocks", stream_blocks},
      {"peak_inflight_bytes", peak_inflight},
      {"whole_frame_fault", whole_frame},
      {"fits_hdu_streaming", !whole_frame}};
  (*man)["sub_block_px"] = sb;
  (*man)["whole_frame_fault"] = whole_frame;
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
  // ── EXPORT-CROP-01：裁剪窗口（与 writer 同源读 p3_wcs.json；默认不裁剪）──
  // 独立重开验证必须对**写出画幅**（裁剪后）判，并逐像素与平面窗口对拍 ⇒
  // 「裁剪只裁、不改数值」在验证面同样被锁住。
  astrocs::phase3::P3CropWindow crop;
  if (!p3n_crop_from_plan(plan, g.w, g.h, &crop, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  P3WcsDescriptor owcs{};
  if (astrocs::phase3::p3_crop_apply_wcs(&wcs, crop, &owcs) != astrocs::phase3::P3_CROP_OK)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "crop: failed to derive the cropped WCS from the frame WCS"));
  const int ow = owcs.width_px;
  const int oh = owcs.height_px;
  const long nelem = (long)g.w * g.h;
  const bool unc = res.value("uncertainty_available", false);
  // verify 独立重开面同样消费 canonical 单位/模式声明（禁 loose default）;
  // resampled ↔ writer 声明分叉 → 显式拒（不把分叉当"已验证"）。
  const std::string v_bunit = res.value("bunit", std::string());
  const std::string w_bunit = wr.value("bunit", std::string());
  if (v_bunit != kP3BunitSurfaceBrightness || w_bunit != v_bunit)
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "verify: bunit drift (resampled '" + v_bunit + "', writer '" + w_bunit +
        "'; expected canonical '" +
        std::string(kP3BunitSurfaceBrightness) + "')"));
  if (res.value("output_mode", std::string()) != wr.value("output_mode", std::string()))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "verify: output_mode drift between resampled and writer artifacts"));
  if (res.value("measurement_capable", false) != wr.value("measurement_capable", false))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "verify: measurement_capable drift between resampled and writer artifacts"));
  // ── P3-STREAM-01：独立重开**子块流式**对拍（不整幅驻留）─────────────────
  // 判据与 p3_output_verify_ex 逐条同面：尺寸/WCS 关键字/HDU 面/逐像素回环
  // （NaN==NaN 同态）/COVERAGE 掩码语义；驻留面 = 单个子块。
  const std::string bin_p = g.out_dir + "/p3_resampled.bin";
  if (!aio_fs::exists(bin_p))
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "upstream artifact missing: p3_resampled.bin"));
  int sb = kP3DefaultSubBlockPx;
  if (!p3n_sub_block_px(doc, &sb, &err))
    return Result<void>::fail(Error(ErrorDomain::DATA, err));
  const std::string verify_fits = wr.value("output_fits", std::string());
  P3OutputResult vres{};
  {
    P3FitsVerifyStream vs;
    if (vs.open(verify_fits.c_str(), &owcs, ow, oh, unc) != P3_OUT_OK)
      return Result<void>::fail(Error(ErrorDomain::IO,
          std::string("p3_stream verify open failed: ") + p3_output_last_error()));
    const int nplanes = unc ? 4 : 2;
    std::vector<float> fb;
    for (int plane = 0; plane < nplanes; ++plane) {
      for (int y0 = 0; y0 < oh; y0 += sb) {
        for (int x0 = 0; x0 < ow; x0 += sb) {
          const int w = std::min(sb, ow - x0);
          const int h = std::min(sb, oh - y0);
          fb.resize((std::size_t)w * h);
          std::string buf;
          for (int yy = 0; yy < h; ++yy) {
            const uint64_t row = (uint64_t)plane * (uint64_t)nelem +
                                 (uint64_t)(crop.y0 + y0 + yy) * (uint64_t)g.w +
                                 (uint64_t)(crop.x0 + x0);
            if (!aio_file::read_range(bin_p.c_str(), row * sizeof(float),
                                      (std::size_t)w * sizeof(float), &buf))
              return Result<void>::fail(Error(ErrorDomain::DATA,
                  "p3_resampled.bin truncated (verify)"));
            std::memcpy(fb.data() + (std::size_t)yy * w, buf.data(),
                        (std::size_t)w * sizeof(float));
          }
          if (vs.check_block(plane, x0, y0, w, h, fb.data()) != P3_OUT_OK)
            return Result<void>::fail(Error(ErrorDomain::IO,
                std::string("p3_stream verify failed: ") + p3_output_last_error()));
        }
      }
    }
    if (vs.close(&vres) != P3_OUT_OK)
      return Result<void>::fail(Error(ErrorDomain::IO,
          std::string("p3_stream verify close failed: ") + p3_output_last_error()));
  }
  const std::string json_path = g.out_dir + "/p3_verify.json";
  // DET-001: verify 侧**独立重算**规范产品哈希（不采信 writer 自报值）, 并给出
  // 与 writer 声明值的一致性判定; 自报值同时保留供审计。
  const astrocs::core::CanonicalHashResult vcanon =
      astrocs::core::canonical_product_hash_file(verify_fits);
  if (!vcanon.ok)
    return Result<void>::fail(Error(ErrorDomain::IO,
        "canonical product hash failed (verify): " + vcanon.error));
  const std::string writer_canon = wr.value("canonical_sha256", std::string());
  const bool canon_match = !writer_canon.empty() &&
                           writer_canon == vcanon.canonical_sha256;
  Json ver{{"schema", "DATA-P3-VER"},
           {"output_fits", verify_fits},
           {"reopen_ok", vres.reopen_ok},
           {"coverage_ok", vres.coverage_ok},
           {"canonical_sha256", vcanon.canonical_sha256},
           {"canonical_hash_spec", astrocs::core::kCanonicalProductHashSpec},
           {"writer_canonical_sha256", writer_canon},
           {"canonical_match", canon_match},
           {"integrity_sha256", std::string(vres.sha256)},
           {"coverage_stats",
            {{"covered_px", vres.covered_px}, {"total_px", vres.total_px}}},
           {"uncertainty_available", unc},
           // B2-A10: verify 侧同源透传 writer 的真实 provenance（禁 CLI 侧再猜）。
           {"run_id", wr.value("run_id", std::string())},
           {"software_version", wr.value("software_version", std::string())},
           {"input_manifest_hash", wr.value("input_manifest_hash", std::string())},
           {"source_sha", wr.value("source_sha", std::string())},
           {"coordinate_frame", wr.value("coordinate_frame", std::string())},
           {"bunit", wr.value("bunit", std::string())},
           {"algorithm_id", wr.value("algorithm_id", std::string())},
           {"module_build_id", wr.value("module_build_id", std::string())},
           {"provider", wr.value("provider", std::string())}};
  // §9 原子提交
  if (!p2_write_text_atomic(json_path, ver.dump(2) + "\n"))
    return Result<void>::fail(Error(ErrorDomain::IO, "p3_verify.json atomic write failed"));
  (*man)["verified_artifact"] = json_path;
  (*man)["artifacts"] = Json::array({json_path});
  (*man)["reopen_ok"] = vres.reopen_ok;
  (*man)["canonical_sha256"] = vcanon.canonical_sha256;
  (*man)["canonical_hash_spec"] = astrocs::core::kCanonicalProductHashSpec;
  (*man)["writer_canonical_sha256"] = writer_canon;
  (*man)["canonical_match"] = canon_match;
  (*man)["integrity_sha256"] = std::string(vres.sha256);
  (*man)["sub_block_px"] = sb;
  (*man)["sub_block_streaming"] = true;
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
    // P3-STREAM-01: 子块边长（编排/资源键，值域 = 内存守卫；缺省 = 组件默认 256）
    if (doc.contains("sub_block_px")) {
      if (!doc["sub_block_px"].is_number_integer())
        return Result<void>::fail(Error(ErrorDomain::DATA, "sub_block_px must be integer"));
      std::string sberr;
      int sb_tmp = 0;
      if (!p3n_sub_block_px(doc, &sb_tmp, &sberr))
        return Result<void>::fail(Error(ErrorDomain::DATA, sberr));
    }
    if (doc.contains("queue_depth") && !doc["queue_depth"].is_number_integer())
      return Result<void>::fail(Error(ErrorDomain::DATA, "queue_depth must be integer"));
    // B2-A4/A5: projection/frame/coverage_output 值域 (validate/plan/run 一致拒绝面;
    // 未实现投影不得登记为可运行配置)。
    {
      std::string rerr;
      if (!p3n_check_request_fields(doc, &rerr))
        return Result<void>::fail(Error(ErrorDomain::DATA, rerr));
    }
    // EXPORT-CROP-01: 裁剪范围 fail-closed（形状 + 几何）。画幅 WCS 由配置唯一确定
    // （与 wcs 节点同源同参）⇒ 越界 / 宽高非正 / 两种形式同时给 / 裁剪后为空
    // 在 validate/plan 阶段即具名拒绝，不等到 writer 才报，也不静默夹取。
    {
      P3nGeom cg;
      std::string cerr;
      if (!p3n_geom(doc, &cg, &cerr))
        return Result<void>::fail(Error(ErrorDomain::DATA, cerr));
      astrocs::phase3::P3WcsDescriptor frame{};
      if (astrocs::phase3::p3_wcs_make(cg.ra, cg.dec, cg.scale, cg.w, cg.h,
                                       cg.parity.c_str(), 0.0, &frame,
                                       cg.projection.c_str()) == astrocs::phase3::P3_WCS_OK) {
        astrocs::phase3::P3CropWindow cwin;
        std::string cwerr;
        if (!p3n_crop_window(doc, frame, cg.w, cg.h, &cwin, nullptr, &cwerr))
          return Result<void>::fail(Error(ErrorDomain::DATA, cwerr));
      }
    }
    return Result<void>::success();
  }

  Result<ModulePlan> plan(const std::string& node_id,
                          const std::string& config_json) override {
    config_ = config_json;
    ModulePlan p;
    p.node_id = node_id;
    p.work_units = 1;
    // P3-STREAM-01：export 的调度单元 = 输出子块（ASTROCS_DESIGN §8.3 export 行）；
    // 具体切分在 op 内按 sub_block_px 与预算确定（work unit 经唯一 executor 提交）。
    p.parallel_axes = {"sub-block"};
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
        // CLEAN-403: 目录创建经 aio (make_dirs); 已存在视为成功 (幂等)。
        if (!aio_fs::make_dirs(out_dir) && !aio_fs::is_dir(out_dir)) {
          man["error"] = "cannot create output_dir: " + out_dir;
          r = Result<void>::fail(Error(ErrorDomain::IO, man["error"].get<std::string>()));
        }
      }
      if (r.ok()) {
        switch (spec_.op) {
          case P3NodeOp::Properties: r = p3_op_properties(doc, &man); break;
          case P3NodeOp::Wcs:        r = p3_op_wcs(doc, &man); break;
          case P3NodeOp::Resample:   r = p3_op_resample(doc, &man, cap, &ctx); break;
          case P3NodeOp::Writer:     r = p3_op_writer(doc, &man, cap, &ctx); break;
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
  // RUN-PROVENANCE-01: 构建期指纹 fail-closed。缺它 ⇒ 运行溯源退化为"只有
  // configure 期 HEAD"，消费方无法判"两次运行是不是同一二进制"；宁可显式失败，
  // 也不产出一份看起来有溯源、实际不可锚的上下文（§4.3 禁占位串同款纪律）。
  const BuildStamp& stamp = build_stamp();
  if (stamp.source_digest.empty() || stamp.head_sha.empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "build stamp unavailable (source_digest/head_sha empty): rebuild so that "
        "build_stamp_generated.h is regenerated (RUN-PROVENANCE-01)"));
  // 字段语义（docs/VERSIONING.md「构建指纹合同」）:
  //   source_sha          = CMake configure 期 HEAD（历史字段，保持兼容）
  //   build_source_digest = **构建期**源集内容摘要 ⇒ 判"同一代码/同一二进制"只看它
  //   build_head_sha/build_dirty = 构建期 HEAD / 工作树 dirty（人读补充）
  //   configure_head_sha  = configure 期 HEAD（对照 source_sha 的来历）
  Json ctx = {{"schema_version", "1"},
              {"kind", "astrocs_run_context"},
              {"run_id", run_id},
              {"software_version", software_version},
              {"source_sha", source_sha},
              {"build_head_sha", stamp.head_sha},
              {"build_dirty", stamp.dirty},
              {"build_source_digest", stamp.source_digest},
              {"configure_head_sha", stamp.configure_head_sha}};
  // CLEAN-403: 目录创建与原子落盘经 aio 唯一实现 (make_dirs /
  // write_file_atomic = 同目录临时文件 → fflush → fsync → 原子 rename)。
  (void)aio_fs::make_dirs(out_dir);
  const std::string final_path = out_dir + "/run_context.json";
  if (!aio_fs::write_atomic(final_path, ctx.dump(2) + "\n"))
    return Result<void>::fail(Error(ErrorDomain::IO,
        "cannot finalize run context: " + final_path));
  return Result<void>::success();
}

// PSF-FAST-001 / INACTIVE: 精确 PSF 路径的**直调测试钩子**（声明见
// lib/include/astrocs/core/module_adapters.h）。生产注册表（register_phase_modules）
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
  // lib/infrastructure/pipeline/module_ports.registry.json 冻结绑定表一致, manifest
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
