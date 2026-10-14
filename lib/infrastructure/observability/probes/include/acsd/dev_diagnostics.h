// lib/infrastructure/observability/probes/include/acsd/dev_diagnostics.h
// ============================================================================
// ACSD 开发诊断 sink —— 被动科学快照 + debug manifest
// ----------------------------------------------------------------------------
// 正本（只读， Agent 不得自行改写）:
//   - docs/engineering/architecture/DEV_MODE.md
//   - docs/engineering/standards/DIAGNOSTIC_PROBES.md
//
// 硬边界（与正本逐条对应）:
//   1. **唯一编译开关**：本头的一切能力只在 ACSD_DEV_MODE=1 时存在。
//      ACSD_DEV_MODE=0 ⇒ 所有宏展开为 `((void)0)`，且**传入表达式不求值**
//      （禁止为 dump 额外构造/拷贝完整中间数组）。
//   2. **只读旁路**：调用方只在"已有输出块完成且仍有效"时发出只读 view。
//      快照绝不写回、不改有效位、不改算法返回值；**任何下游科学模块都不得
//      读取本 sink 写出的文件**。快照不是 HiPS 的第二个科学图像层。
//   3. **落盘统一走 aio**：本基础设施经 aio_atomic 原语（临时名 + fsync +
//      原子 rename）落盘；业务模块不持 FILE*、不建路径、不起线程。
//   4. **失败域隔离**：诊断写盘失败只记录诊断缺失（IO_ERROR 计数），不改变
//      科学结果、退出码与正式产物的原子发布。正式科学文件/正式日志写失败
//      仍按既有 I/O 错误合同处理。
//   5. **不阻塞**：预算耗尽 ⇒ SKIPPED_BUDGET 登记后丢弃，绝不让快照队列
//      阻塞科学计算。
//
// 性能计时**不**在本头另起一套：细分计时唯一入口是 acsd/probe.h 的
// ACSD_PROBE_SCOPE / ACSD_PROBE_COUNT / ACSD_PROBE_GAUGE（由同一
// ACSD_DEV_MODE 派生编译）。
// ============================================================================
#pragma once

// ── 编译开关（由 CMake 从唯一主开关 ACSD_DEV_MODE 派生下发）─────────────────
#if defined(ACSD_DEV_MODE) && (ACSD_DEV_MODE)
#define ACSD_DEV_DIAGNOSTICS 1
#else
#define ACSD_DEV_DIAGNOSTICS 0
#endif

#if ACSD_DEV_DIAGNOSTICS

#include <cstddef>

namespace acsd {
namespace devdiag {

// ── 探针登记词面（DIAGNOSTIC_PROBES.md §2）────────────────────────────────
enum class ProbeKind : int {
  SnapshotImage = 0,
  SnapshotTable = 1,
  SnapshotModel = 2,
  PerfSpan = 3,
  PerfCounter = 4,
  PerfGauge = 5,
};

enum class CoordSpace : int {
  DetectorPixel = 0,
  Icrs = 1,
  HealpixNested = 2,
  HipsTileFitsRowmajor = 3,
  TargetWcsPixel = 4,
  None = 5,
};

const char* coord_space_token(CoordSpace c);

// 业务模块只填 POD 数据与标签；文件名/目录/编码由本基础设施决定。
// 字符串指针按**借用**保存：其生命周期必须覆盖本次调用（调用方通常传字面量
// 或比本调用活得更久的 std::string::c_str()）。
struct SnapshotMeta {
  // ── 必填（注册表字段，缺一即元数据错误）─────────────────────────────────
  const char* probe_id = "";      // 白名单枚举，见 dev_diagnostics.cpp 注册表
  const char* stage = "";         // "normalize" | "mosaic" | "export"
  const char* producer = "";      // 实际生产函数/节点身份（非行号）
  const char* semantic = "";      // 被估计的物理量（禁只写 "weight"）
  const char* unit = "";          // 原样 BUNIT / 单位词
  CoordSpace coord_space = CoordSpace::None;
  const char* dtype = "";         // "float32" / "float64" / ...
  const char* shape = "";         // "[rows,cols]" / "[n_records]" / ...
  // ── 可选 ────────────────────────────────────────────────────────────────
  const char* observation_boundary = nullptr;  // 前/后背景扣除、Drizzle 前后…
  const char* valid_support = nullptr;         // 覆盖/NaN/拒绝样本/空洞语义
  const char* validity_policy = nullptr;      // 掩膜是真实输入模型还是派生统计
  const char* frame_id = nullptr;             // 标签，不是 ID 本身
  const char* tile_note = nullptr;            // 如 "order=10 ipix=1234"
  const char* upstream_product = nullptr;     // 上游产品/数据对象指纹
  const char* extra_json = nullptr;           // 附加 JSON 片段（可空）
  /* ROI（大数组取指定窗口; rows/cols < 0 = 全幅）。确定性选择，不按线程完成
   * 先后采样（否则破坏两次运行对照）。偏移与幅值必须在缓冲范围内，越界记
   * INVALID_VIEW 而非静默裁剪。 */
  int roi_row0 = 0;
  int roi_col0 = 0;
  int roi_rows = -1;
  int roi_cols = -1;

  SnapshotMeta& boundary(const char* v) { observation_boundary = v; return *this; }
  SnapshotMeta& support(const char* v) { valid_support = v; return *this; }
  SnapshotMeta& policy(const char* v) { validity_policy = v; return *this; }
  SnapshotMeta& frame(const char* v) { frame_id = v; return *this; }
  SnapshotMeta& tile(const char* v) { tile_note = v; return *this; }
  SnapshotMeta& upstream(const char* v) { upstream_product = v; return *this; }
  SnapshotMeta& extra(const char* v) { extra_json = v; return *this; }
  SnapshotMeta& roi(int r0, int c0, int rn, int cn) {
    roi_row0 = r0; roi_col0 = c0; roi_rows = rn; roi_cols = cn;
    return *this;
  }
};

// ── 运行期接口（仅在 ACSD_DEV_MODE=1 编译）─────────────────────────────────
// work_root  = 一次运行的正式工作根（诊断目录挂在其下，绝不放进 signal/
//              Norder*/ 等正式科学产品集合内）。
// run_id     = 稳定运行标识（决定目录名与跨运行对照）。
// 返回 false = 诊断工作区建立失败（此后所有快照记 IO_ERROR，科学照常）。
bool begin_run(const char* work_root, const char* run_id,
               const char* git_commit, const char* source_digest);

// 收尾：bounded flush + 写 debug_manifest.json。可安全重复调用。
void finish_run();

// 该 probe_id 当前是否被选中（选择器决定"观测多少"，不决定科学逻辑）。
bool selected(const char* probe_id);

// 快照写入。返回状态串（稳定词面，逐字写入 debug_manifest）：
//   "WRITTEN" | "NOT_SELECTED" | "SKIPPED_BUDGET" | "IO_ERROR" |
//   "INVALID_VIEW" | "NOT_SEEN"
// 任一失败都**不抛异常**、不改科学结果。
const char* snapshot_image_f32(const SnapshotMeta& meta,
                               int rows, int cols, const float* data);
const char* snapshot_image_f64(const SnapshotMeta& meta,
                               int rows, int cols, const double* data);
// 稀疏点/拟合残差/模型系数等小 JSON/表结构（body 为完整 JSON 文本）。
const char* snapshot_text(const SnapshotMeta& meta, const char* body);

// 诊断缺失计数（debug_manifest 汇总面；开发运行日志可读）。
unsigned long long diag_error_count();
unsigned long long diag_written_count();

}  // namespace devdiag
}  // namespace acsd

// ── 宏（OFF 时传入表达式不求值）────────────────────────────────────────────
#define ACSD_DEV_SNAPSHOT_IMAGE_F32(meta, rows, cols, ptr)                 \
  (void)::acsd::devdiag::snapshot_image_f32((meta), (rows), (cols), (ptr))
#define ACSD_DEV_SNAPSHOT_IMAGE_F64(meta, rows, cols, ptr)                 \
  (void)::acsd::devdiag::snapshot_image_f64((meta), (rows), (cols), (ptr))
#define ACSD_DEV_SNAPSHOT_TEXT(meta, body)                                 \
  (void)::acsd::devdiag::snapshot_text((meta), (body))

#else  // ACSD_DEV_MODE off —— 零开销空语句；参数表达式不求值

#define ACSD_DEV_SNAPSHOT_IMAGE_F32(meta, rows, cols, ptr) ((void)0)
#define ACSD_DEV_SNAPSHOT_IMAGE_F64(meta, rows, cols, ptr) ((void)0)
#define ACSD_DEV_SNAPSHOT_TEXT(meta, body) ((void)0)

#endif  // ACSD_DEV_DIAGNOSTICS
