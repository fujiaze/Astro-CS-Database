// AstroCS Core Contracts — RT-005 plan 真实资源估算（独立纯函数库）
//
// 角色（规格 tasks/03_RUNTIME_DATA_IO_TASKS.md RT-005 + 14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md §4
//       + checklists/02_MODULE_ACCEPTANCE_CHECKLIST.md IMPL）：
//   每 module plan 从输入 metadata 计算执行计划：work_units、axis、min/max useful
//   workers、peak memory、read/write bytes、kernel IDs、serial sections；不同输入
//   必须产生不同计划；估算与运行峰值在冻结误差范围（本层为纯估算，不执行 kernel；
//   峰值对照冻结误差的断言在测试层以独立 oracle 公式做冻结复算）。
//
// ABI 决策（前任 agent 已定，勿改）：本估算为独立纯函数库，不改 ModulePlan 布局
// （rt001_abi_test 冻结 PortDescriptor/ModuleDescriptor/ThreadBudget offsetof/sizeof，
//   不冻结 ModulePlan；但 ModulePlan 是 RT-001 冻结合同公开结构，避免任何 ABI 连锁）。
// 本头不 include module.h（无依赖环）；估算结果独立结构 PlanEstimate，可与 ModulePlan
// 字段一一对应投影。
//
// 单位/约定（权威=docs/science、docs/algorithms、lib/phase* 实现与 api 文档，勿臆测）：
//   - 图像/输出平面尺寸一律像素(px)；角尺度 deg/px；HiPS order=properties hips_order
//     （0..20，docs/algorithms/HEALPIX_MAPPING.md + p3 hips_properties.h kMaxOrder=20）；
//   - tile 宽 W=512（lib/phase3_session/hips_properties.h kHipsTileWidth=512；P2
//     coverage.cpp 同样拒绝 tw!=512）；
//   - 每 tile FITS float 像素数 = W*W，字节 = W*W*4（f32 输入，SCALE 输入单精度）；
//   - 内存/IO 估算字节数一律 u64；无符号溢出由 checked 计算拒绝（负/越界 → 错误）。
//
// 冻结常数（不得改动；与现有实现一致）：
//   kHipsTileWidthBytes  = 512*512*4（tile f32 字节数）；
//   kHipsMaxOrder        = 20（p3 hips_properties.h kMaxOrder）；
//   peak 估算含 output 平面 + per-worker tile cache + coverage 平面 + 原子写缓冲区，
//   峰值公式的冻结误差界（见 tests）来自实现的实际结构（p3_session sig+cov 双平面 +
//   TileCache cap；p2 block plan scratch；scheduler NodeSpec 内存回压）。
#pragma once

#ifndef ASTROCS_CORE_PLAN_ESTIMATOR_H
#define ASTROCS_CORE_PLAN_ESTIMATOR_H

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs::core {

// 冻结常数（估算公式权威常数；与实现一致，禁止改动）
constexpr uint64_t kHipsTileWidthPx = 512;
constexpr uint64_t kHipsTileF32Bytes = kHipsTileWidthPx * kHipsTileWidthPx * 4;  // 1 MiB
constexpr uint64_t kHipsMaxOrder = 20;
constexpr uint64_t kPeakTilesCacheDefault = 8;   // TileCache 默认 cap（p3_resample.cpp）

// 串行段类别（14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md §4 serial sections；
// 约束 D.2：短 I/O/元数据/初始化可串行但必须显式分类；heavy kernel 不得入串行）
enum class SerialSectionKind : uint8_t {
  METADATA = 0,   // properties/header 解析、配置（短）
  IO_READ = 1,    // 短读/校验（有界 IO executor 串行标记）
  IO_WRITE = 2,   // 原子写/发布（串行 writer）
  FINALIZE = 3,   // 归并/收尾（确定性归约，短）
};

struct SerialSection {
  std::string name;             // 稳定名（"properties-parse"/"atomic-write"/...）
  SerialSectionKind kind = SerialSectionKind::METADATA;
  uint64_t estimated_us = 0;    // 短串行段估算耗时（us；仅用于 plan 分类，非观测值）
  std::string reason;           // 冻结原因（为何必须串行）
};

// 执行计划估算（RT-005 完整估算结构；命名与规格一致：
// work_units/axis/min-max useful workers/peak memory/read-write bytes/kernel ids/
// serial sections。可独立序列化，稳定字段顺序。）
struct PlanEstimate {
  std::string node_id;                 // 计划所属 node（非空）
  std::string module_id;               // 模块 ID（如 astrocs.phase3.resample2）
  std::string execution_class;         // cpu_heavy|io|cpu_light（descriptor 同源）

  uint64_t work_units = 0;             // 并行工作量（tile/row-band/像素样本数）
  std::string axis;                    // 并行轴（"row-band"/"tile"/"sample"/"frame"…）
  uint32_t min_useful_workers = 1;     // 最小有用 worker（≥1；1=串行，heavy 需标注）
  uint32_t max_useful_workers = 1;     // 最大有用 worker（cap 到 axis 可切分数）
  bool tiny_work = false;              // work_units 过小 → 标注 tiny（允许串行）
  bool serial_only = false;            // 估算仅可串行（heavy 拒绝路径之外显式标注）

  uint64_t peak_memory_bytes = 0;      // 峰值内存估算（含输入驻留 + 输出 + 缓存）
  uint64_t read_bytes = 0;             // 预计读字节（输入 metadata 推导）
  uint64_t write_bytes = 0;            // 预计写字节（输出 metadata 推导）

  std::vector<std::string> kernel_ids;         // 请求的 provider kernel ID（权威清单见
                                               // lib/backend_host/backend_table.inc）
  std::vector<SerialSection> serial_sections;  // 显式串行段（短 I/O/metadata/收尾）
};

// 输入 metadata：估算是输入形状/参数的真实函数（不执行 kernel、不读盘）。
// 各字段来源于模块 config / DataArtifactDescriptor.shape / HiPS properties
// （p3: width_px/height_px/scale_deg_per_px/source.hips order/tile_width；
//  p2: n_inputs/nside/target_order/覆盖 cell 数；p1: frames×宽高 等）。
struct PlanInputMetadata {
  std::string module_id;               // 目标模块 ID（决定 axis/kernel 映射）
  uint64_t width_px = 0;               // 输出/帧宽（px）
  uint64_t height_px = 0;              // 输出/帧高（px）
  uint64_t n_frames = 1;               // 输入帧数/样本数（≥1）
  uint64_t input_order = 0;            // 输入 HiPS order（0..20；0 表示未知/非 HiPS）
  uint64_t tile_width_px = 0;          // 输入 tile 宽（0=默认 512）
  uint64_t n_tiles_input = 0;          // 输入覆盖 tile 数（0=未知，估算保守缺省）
  uint64_t output_order = 0;           // 输出 HiPS order（0=无）
  uint64_t output_pixels = 0;          // 输出平面像素（P2 块/输出）
  double scale_deg_per_px = 0.0;       // 输出像元角尺度（deg/px；>0 才有意义）
  std::string projection;              // 输出投影（P3 alpha 冻结仅 "TAN"；
                                       // 非空且非 TAN → PARAM 拒绝，同 SCI/API 拒绝面）
  bool is_heavy = false;               // execution_class == cpu_heavy（descriptor 判定）
};

// 计划类别错误
enum class PlanEstimateError : uint8_t {
  NONE = 0,
  PARAM = 1,            // 输入 metadata 非法（0 尺寸等）
  HEAVY_TINY = 2,       // heavy 任务 work_units==1（计划并行度 1）→ 拒绝
  UNSUPPORTED_MODULE = 3,  // 未知模块 ID
  UNSUPPORTED_PROJECTION = 4,  // 非 TAN 投影（P3 alpha 显式拒）
};

// 估算结果封装（值语义；错误含稳定类别）
struct PlanEstimateResult {
  PlanEstimate plan;
  PlanEstimateError error = PlanEstimateError::NONE;
  std::string error_message;           // 机器可读；错误时非空
  bool ok() const noexcept { return error == PlanEstimateError::NONE; }
};

// 估算核心（纯函数；不执行 kernel、无 IO、线程安全=可并发调用）：
//   - 不同输入 metadata → 不同计划（确定性：同输入必同输出）；
//   - heavy 且 work_units==1（估算 max useful workers==1 且非 tiny）→
//     error=HEAVY_TINY（拒绝，不等运行后才发现单线程；14 标准 §3）；
//   - work_units==1 且 tiny（尺寸过小）→ 标注 tiny_work/serial_only（允许串行）。
// module_id 支持 astrocs.phase1.{calibration,cosmetic,star-psf,wcs-platesolve,
// photometry,noise-snr,drizzle} / astrocs.phase2.{resample,sample,upm-fit,upm-apply,
// reject,integrate,coverage} / astrocs.phase3.{resample,resample2,properties,wcs}。
PlanEstimateResult estimate_plan(const PlanInputMetadata& in);

// 便捷：峰值估算冻结误差断言辅助（返回估算是否落在冻结误差界内）。
// 冻结误差源：p3_session sig+cov 双平面+TileCache、p2 block plan（estimated_peak
// <= memory_limit 且 >= 纯输出平面）、scheduler 内存回压。实现按模块类别给出
// [min, max] 冻结界；测试断言 estimate 落在此界内（估算 vs 峰值冻结误差）。
bool peak_within_frozen_bounds(const PlanEstimate& est, uint64_t* min_allowed,
                               uint64_t* max_allowed);

// 稳定序列化（JSON 文本；字段顺序稳定；测试可解析比对）
std::string plan_estimate_to_json(const PlanEstimate& est);

}  // namespace astrocs::core

#endif  // ASTROCS_CORE_PLAN_ESTIMATOR_H
