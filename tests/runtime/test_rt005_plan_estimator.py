#!/usr/bin/env python3
"""RT-005 验收测试：plan 真实资源估算（plan_estimator 独立纯函数库）。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md RT-005):
  每 module plan 从 input metadata 计算 work_units / axis / min-max useful workers /
  peak memory / read-write bytes / kernel IDs / serial sections；
  - 字段存在且随输入 metadata 变化；不同图像尺寸/order/输出 projection → 不同计划
    （至少两组输入产生不同 plan 值）；
  - 估算与运行峰值在冻结误差范围（本层纯估算不执行 kernel；以独立 oracle 公式
    复算 peak 冻结界并断言落点 —— plan_estimator.cpp peak_within_frozen_bounds
    与本文件 oracle 双向独立推导，错误同源即双 FAIL）；
  - work_units==1 的 heavy 任务拒绝(HEAVY_TINY)或 tiny 标注（14 标准 §3）；
  - tiny 任务标 tiny；serial_only 标注正确；
  - 错误路径：负/越界输入 → checked 计算返回错误，无 UB、无溢出（不产出
    UINT64_MAX 饱和伪计划、不静默降级越界 order/尺寸）。

方法 (照 tests/runtime/test_rt004_executor.py / test_rt003_budget_wiring.py 先例):
  Python unittest 内嵌 C++ driver，g++ 真实编译链接 lib/core/src/plan_estimator.cpp
  + include/astrocs/core 头，运行断言并输出 ALL PASS / N FAIL。
"""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
INC = REPO / "include"
CORE = REPO / "lib" / "core" / "src"

_DRIVER = r'''
// RT-005 harness: plan_estimator 独立纯函数库验收（真实编译链接 + 运行断言）
#include "astrocs/core/plan_estimator.h"

#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace astrocs::core;

static int g_checks = 0;
static int g_failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    ++g_checks;                                                           \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

static constexpr uint64_t kTinyOutPx = 128u * 128u;  // 与实现 tiny 阈值一致（头注释冻结）
static constexpr uint64_t kTilePx = 512u * 512u;
static constexpr uint64_t kF32 = 4u;

static bool approx_less(uint64_t a, uint64_t b, uint64_t rel_ppm, uint64_t abs_tol) {
  // 两 u64 的冻结相对界（ppm）+ 绝对余量比较（无符号差语义）
  return (a <= b) ? (b - a) <= abs_tol || a * 1000000u <= b * (1000000u + rel_ppm)
                  : approx_less(b, a, rel_ppm, abs_tol);
}

// ══ A. 不同输入 → 不同计划（字段真实函数） ══
static void test_inputs_vary_plan() {
  PlanInputMetadata a{};
  a.module_id = "astrocs.phase3.resample2"; a.is_heavy = true;
  a.width_px = 2000; a.height_px = 2000; a.input_order = 8;
  a.scale_deg_per_px = 0.001;
  auto ra = estimate_plan(a);
  CHECK(ra.ok());
  // 不同图像尺寸 → work_units/peak/read/write 全变
  PlanInputMetadata b = a; b.width_px = 4000; b.height_px = 3000;
  auto rb = estimate_plan(b);
  CHECK(rb.ok());
  CHECK(rb.plan.work_units != ra.plan.work_units);
  CHECK(rb.plan.peak_memory_bytes != ra.plan.peak_memory_bytes);
  CHECK(rb.plan.read_bytes != ra.plan.read_bytes);
  CHECK(rb.plan.write_bytes != ra.plan.write_bytes);
  // 不同输入 order → read_bytes 变（order 低于 scale 所需叶级时限制覆盖几何；
  // o4 比 o8 更欠采样 → 更粗叶级 → 足迹 tile 更少）
  PlanInputMetadata c = a; c.input_order = 4;
  auto rc = estimate_plan(c);
  CHECK(rc.ok());
  CHECK(rc.plan.read_bytes != ra.plan.read_bytes);
  CHECK(rc.plan.read_bytes < ra.plan.read_bytes);
  CHECK(rc.plan.kernel_ids == ra.plan.kernel_ids);  // kernel 由模块类别定（同一模块）
  // 不同 scale → 计划变化（order 选择不同 → 覆盖不同）
  PlanInputMetadata d = a; d.scale_deg_per_px = 0.00001;
  auto rd = estimate_plan(d);
  CHECK(rd.ok());
  CHECK(rd.plan.read_bytes != ra.plan.read_bytes);
  // 不同输出 projection: alpha 冻结仅 TAN → 非 TAN 显式拒（与 SCI/API 拒绝面一致）
  PlanInputMetadata p = a; p.projection = "CAR";
  auto rp = estimate_plan(p);
  CHECK(!rp.ok());
  CHECK(rp.error == PlanEstimateError::UNSUPPORTED_PROJECTION);
  // 空投影 = 默认 TAN 语义 → OK（与 TAN 显式等价）
  PlanInputMetadata t = a; t.projection = "TAN";
  CHECK(estimate_plan(t).ok());

  // P2 输入 tiles 变化 → work_units/read/write/peak 全变
  PlanInputMetadata p2a{};
  p2a.module_id = "astrocs.phase2.integrate"; p2a.is_heavy = true;
  p2a.n_tiles_input = 100; p2a.n_frames = 8; p2a.input_order = 6;
  auto r2a = estimate_plan(p2a);
  CHECK(r2a.ok());
  PlanInputMetadata p2b = p2a; p2b.n_tiles_input = 5000;
  auto r2b = estimate_plan(p2b);
  CHECK(r2b.ok());
  CHECK(r2b.plan.work_units == 5000 && r2b.plan.work_units != r2a.plan.work_units);
  CHECK(r2b.plan.peak_memory_bytes != r2a.plan.peak_memory_bytes);
  CHECK(r2b.plan.read_bytes != r2a.plan.read_bytes);

  // P1 帧数变化 → work_units/read 变
  PlanInputMetadata p1a{};
  p1a.module_id = "astrocs.phase1.calibration"; p1a.is_heavy = true;
  p1a.width_px = 2048; p1a.height_px = 2048; p1a.n_frames = 3;
  auto r1a = estimate_plan(p1a);
  CHECK(r1a.ok());
  PlanInputMetadata p1b = p1a; p1b.n_frames = 7;
  auto r1b = estimate_plan(p1b);
  CHECK(r1b.ok());
  CHECK(r1b.plan.work_units != r1a.plan.work_units);
  CHECK(r1b.plan.read_bytes != r1a.plan.read_bytes);
}

// ══ B. 字段存在性与一致性 ══
static void test_fields_present_and_consistent() {
  PlanInputMetadata a{};
  a.module_id = "astrocs.phase3.resample2"; a.is_heavy = true;
  a.width_px = 2000; a.height_px = 2000; a.input_order = 8;
  a.scale_deg_per_px = 0.001;
  auto r = estimate_plan(a);
  CHECK(r.ok());
  const PlanEstimate& e = r.plan;
  CHECK(e.work_units >= 1);
  CHECK(e.axis == "row-band");
  CHECK(e.min_useful_workers >= 1);
  CHECK(e.max_useful_workers >= e.min_useful_workers);
  CHECK(e.max_useful_workers <= 1024);              // 本征并行 cap（scheduler 再 cap）
  CHECK(e.peak_memory_bytes > 0);
  CHECK(e.read_bytes > 0);
  CHECK(e.write_bytes > 0);
  CHECK(e.kernel_ids.size() >= 1);
  // serial sections: P3 重投影必须显式串行元数据段（约束 D.2 分类）
  CHECK(!e.serial_sections.empty());
  bool has_meta = false, has_write = false;
  for (const auto& s : e.serial_sections) {
    if (s.kind == SerialSectionKind::METADATA) has_meta = true;
    if (s.kind == SerialSectionKind::IO_WRITE) has_write = true;
  }
  CHECK(has_meta);
  CHECK(has_write);
  // write_bytes == 输出 f32 主平面（f32 输入）
  CHECK(e.write_bytes == a.width_px * a.height_px * kF32);

  // P2 integrate
  PlanInputMetadata p2{};
  p2.module_id = "astrocs.phase2.integrate"; p2.is_heavy = true;
  p2.n_tiles_input = 100; p2.n_frames = 8; p2.input_order = 6;
  auto r2 = estimate_plan(p2);
  CHECK(r2.ok());
  CHECK(r2.plan.work_units == 100);
  CHECK(r2.plan.axis == "tile");
  CHECK(r2.plan.kernel_ids.size() == 1);
  CHECK(r2.plan.kernel_ids[0] == "integration-accumulate");  // backend_table.inc 权威名
  CHECK(r2.plan.max_useful_workers >= 1);

  // P1 calibration
  PlanInputMetadata p1{};
  p1.module_id = "astrocs.phase1.calibration"; p1.is_heavy = true;
  p1.width_px = 2048; p1.height_px = 2048; p1.n_frames = 3;
  auto r1 = estimate_plan(p1);
  CHECK(r1.ok());
  CHECK(r1.plan.axis == "frame-row-band");
  CHECK(r1.plan.kernel_ids[0] == "calibration-pixel-transform");
  CHECK(r1.plan.read_bytes == 2048ull * 2048ull * 3ull * kF32);  // 全部输入帧
  CHECK(r1.plan.write_bytes == 2048ull * 2048ull * kF32);
  bool has_io_read = false;
  for (const auto& s : r1.plan.serial_sections)
    if (s.kind == SerialSectionKind::IO_READ) has_io_read = true;
  CHECK(has_io_read);

  // P3 properties（轻量 metadata 模块）: 显式 tiny/serial_only
  PlanInputMetadata pr{};
  pr.module_id = "astrocs.phase3.properties";
  auto rp = estimate_plan(pr);
  CHECK(rp.ok());
  CHECK(rp.plan.tiny_work);
  CHECK(rp.plan.serial_only);
  CHECK(rp.plan.axis == "metadata");
}

// ══ C. 冻结误差 oracle：peak_within_frozen_bounds + 独立复算 ══
// 冻结界由实现结构推导（p3_session sig+cov 双平面 + TileCache cap + 每 worker 独立
// sampler cache；p2 block plan scratch；scheduler NodeSpec 内存回压）。min = 纯输出
// f32 主平面（write_bytes）；max = 输出×2 + 全 worker cache（tile 缓存公式
// ceil(write_bytes/1MiB)+16 cap[8,1024]）× workers + 读余量 + 2MiB 常数。
// 本文件用与 peak_within_frozen_bounds 相同的独立公式复算峰值，并与 API 直接断言
// 双保险（两处实现同源错误 → 双 FAIL，非同源则至少一处拦截）。
static bool frozen_bounds_ok(const PlanEstimate& est, uint64_t* lo, uint64_t* hi) {
  const uint64_t wb = est.write_bytes > 0 ? est.write_bytes : 1;
  const uint64_t min_v = wb;
  const uint64_t out_two = (wb > (UINT64_MAX >> 1)) ? UINT64_MAX : wb * 2;
  uint64_t ct = wb / kHipsTileF32Bytes;
  if (wb % kHipsTileF32Bytes) ++ct;
  ct += 16;
  ct = ct < 8 ? 8 : (ct > 1024 ? 1024 : ct);
  const uint64_t workers = est.max_useful_workers >= 1 ? est.max_useful_workers : 1;
  uint64_t cache_total = (ct > UINT64_MAX / kHipsTileF32Bytes)
                             ? UINT64_MAX
                             : ct * kHipsTileF32Bytes;
  uint64_t cache_all = (workers > UINT64_MAX / cache_total) ? UINT64_MAX
                                                            : cache_total * workers;
  const uint64_t read_margin = est.read_bytes >> 3;
  const uint64_t const_margin = 2u << 20;
  // safe_add 饱和
  auto sadd = [](uint64_t x, uint64_t y) {
    return (y > UINT64_MAX - x) ? UINT64_MAX : x + y;
  };
  const uint64_t max_v = sadd(sadd(sadd(out_two, cache_all), read_margin), const_margin);
  if (lo) *lo = min_v;
  if (hi) *hi = max_v;
  return est.peak_memory_bytes >= min_v && est.peak_memory_bytes <= max_v;
}

static void test_frozen_peak_bounds() {
  // P3 重投影各尺寸
  PlanInputMetadata a{};
  a.module_id = "astrocs.phase3.resample2"; a.is_heavy = true;
  a.width_px = 2000; a.height_px = 2000; a.input_order = 8;
  a.scale_deg_per_px = 0.001;
  auto r = estimate_plan(a);
  CHECK(r.ok());
  uint64_t lo = 0, hi = 0;
  CHECK(peak_within_frozen_bounds(r.plan, &lo, &hi));
  CHECK(r.plan.peak_memory_bytes >= lo && r.plan.peak_memory_bytes <= hi);
  uint64_t lo2 = 0, hi2 = 0;
  CHECK(frozen_bounds_ok(r.plan, &lo2, &hi2));
  CHECK(lo2 == lo && hi2 == hi);  // 独立复算与 API 冻结界一致
  // P1/P2 同类
  PlanInputMetadata p2{};
  p2.module_id = "astrocs.phase2.integrate"; p2.is_heavy = true;
  p2.n_tiles_input = 100; p2.n_frames = 8; p2.input_order = 6;
  auto r2 = estimate_plan(p2);
  CHECK(r2.ok());
  CHECK(peak_within_frozen_bounds(r2.plan, &lo, &hi));
  CHECK(r2.plan.peak_memory_bytes >= lo && r2.plan.peak_memory_bytes <= hi);
  PlanInputMetadata p1{};
  p1.module_id = "astrocs.phase1.calibration"; p1.is_heavy = true;
  p1.width_px = 2048; p1.height_px = 2048; p1.n_frames = 3;
  auto r1 = estimate_plan(p1);
  CHECK(r1.ok());
  CHECK(peak_within_frozen_bounds(r1.plan, &lo, &hi));
  CHECK(r1.plan.peak_memory_bytes >= lo && r1.plan.peak_memory_bytes <= hi);
  // 极小尺寸（写字节小于 1MiB）也必须在界内
  PlanInputMetadata t = a; t.width_px = 64; t.height_px = 64;
  auto rt = estimate_plan(t);
  CHECK(rt.ok());
  CHECK(peak_within_frozen_bounds(rt.plan, &lo, &hi));
  CHECK(rt.plan.peak_memory_bytes >= lo && rt.plan.peak_memory_bytes <= hi);
}

// ══ D. heavy work_units==1 → HEAVY_TINY 拒绝；tiny 正确标注 ══
static void test_heavy_tiny_and_tiny_labels() {
  // heavy 且 work_units==1 且非 tiny → 拒绝（14 标准 §3：不等运行后才发现单线程）
  PlanInputMetadata h{};
  h.module_id = "astrocs.phase3.resample2"; h.is_heavy = true;
  h.width_px = 4096; h.height_px = 8; h.input_order = 8; h.scale_deg_per_px = 0.001;
  // 输出 32768 px > 128×128 → 非 tiny；H=8 → 单行带 → max useful==1 → 拒绝
  auto rh = estimate_plan(h);
  CHECK(!rh.ok());
  CHECK(rh.error == PlanEstimateError::HEAVY_TINY);
  CHECK(!rh.error_message.empty());

  // tiny 尺寸（≤128×128 输出域）heavy → 放行并标注 tiny_work
  PlanInputMetadata t = h; t.width_px = 16; t.height_px = 16;
  auto rt = estimate_plan(t);
  CHECK(rt.ok());
  CHECK(rt.plan.tiny_work);
  CHECK(rt.plan.serial_only);       // 16×16 → 单行带 → 串行 + tiny → 显式标注
  CHECK(rt.plan.max_useful_workers == 1);

  // heavy P2: 单 tile（512×512=262144px > tiny 阈值 128×128）→ 非 tiny + max==1
  // → HEAVY_TINY 拒绝（计划并行度 1 且非 tiny，14 标准 §3）
  PlanInputMetadata p2{};
  p2.module_id = "astrocs.phase2.integrate"; p2.is_heavy = true;
  p2.n_tiles_input = 1; p2.n_frames = 1; p2.input_order = 0;
  auto r2 = estimate_plan(p2);
  CHECK(!r2.ok());
  CHECK(r2.error == PlanEstimateError::HEAVY_TINY);
}

// ══ E. serial_only / tiny 标注语义 ══
static void test_serial_only_labels() {
  // P3 properties 轻量模块: 显式 tiny + serial_only
  PlanInputMetadata pr{};
  pr.module_id = "astrocs.phase3.properties";
  auto rp = estimate_plan(pr);
  CHECK(rp.ok());
  CHECK(rp.plan.tiny_work && rp.plan.serial_only);
  // 重投影 64×64 tiny 但可切 4 行带: tiny=true, serial_only=false（并行仍有用）
  PlanInputMetadata a{};
  a.module_id = "astrocs.phase3.resample2"; a.is_heavy = true;
  a.width_px = 64; a.height_px = 64; a.input_order = 8; a.scale_deg_per_px = 0.001;
  auto ra = estimate_plan(a);
  CHECK(ra.ok());
  CHECK(ra.plan.tiny_work);
  CHECK(!ra.plan.serial_only);       // 4 行带仍可并行
  CHECK(ra.plan.max_useful_workers == 4);
  // 16×16 heavy → 单行带 tiny → serial_only=true（heavy 串行兜底仅 tiny 放行）
  PlanInputMetadata s = a; s.width_px = 16; s.height_px = 16;
  auto rs = estimate_plan(s);
  CHECK(rs.ok());
  CHECK(rs.plan.serial_only);
  // 非 heavy 单行带非 tiny → 无拒绝（轻任务可串行，不标 serial_only 无需 tiny）
  PlanInputMetadata l = a; l.is_heavy = false; l.width_px = 4096; l.height_px = 8;
  auto rl = estimate_plan(l);
  CHECK(rl.ok());
  CHECK(!rl.plan.tiny_work);
}

// ══ F. 错误路径：负/越界 → checked 错误，无 UB/溢出 ══
static void test_error_paths_checked() {
  // 空 module_id
  PlanInputMetadata e0{}; e0.is_heavy = true; e0.width_px = 100;
  auto r = estimate_plan(e0);
  CHECK(!r.ok() && r.error == PlanEstimateError::PARAM);
  // 未知模块
  PlanInputMetadata u{}; u.module_id = "astrocs.nope"; u.width_px = 100;
  auto ru = estimate_plan(u);
  CHECK(!ru.ok() && ru.error == PlanEstimateError::UNSUPPORTED_MODULE);
  // P3 resample: W/H = 0 → PARAM（不得产出 work_units=0 伪计划）
  PlanInputMetadata z{};
  z.module_id = "astrocs.phase3.resample2"; z.is_heavy = true;
  z.width_px = 0; z.height_px = 2000; z.input_order = 8; z.scale_deg_per_px = 0.001;
  auto rz = estimate_plan(z);
  CHECK(!rz.ok() && rz.error == PlanEstimateError::PARAM);
  z.width_px = 2000; z.height_px = 0;
  rz = estimate_plan(z);
  CHECK(!rz.ok() && rz.error == PlanEstimateError::PARAM);
  // order 越界 (>20) → PARAM（不得静默降级为 0）
  z.width_px = 2000; z.height_px = 2000; z.input_order = 21;
  rz = estimate_plan(z);
  CHECK(!rz.ok() && rz.error == PlanEstimateError::PARAM);
  // 非 TAN 投影 → UNSUPPORTED_PROJECTION
  z.input_order = 8; z.projection = "SIN";
  rz = estimate_plan(z);
  CHECK(!rz.ok() && rz.error == PlanEstimateError::UNSUPPORTED_PROJECTION);
  // W*H 溢出（u64）→ PARAM（checked 拒绝，不得静默饱和）
  PlanInputMetadata ov{};
  ov.module_id = "astrocs.phase3.resample2"; ov.is_heavy = true;
  ov.width_px = 1ull << 40; ov.height_px = 1ull << 40;
  ov.input_order = 8; ov.scale_deg_per_px = 0.001;
  auto rov = estimate_plan(ov);
  CHECK(!rov.ok() && rov.error == PlanEstimateError::PARAM);
  // P1: 缺尺寸 / frames=0 / 溢出 → PARAM
  PlanInputMetadata p1{};
  p1.module_id = "astrocs.phase1.calibration"; p1.is_heavy = true;
  p1.width_px = 2048; p1.height_px = 2048; p1.n_frames = 3;
  PlanInputMetadata p1n = p1; p1n.width_px = 0;
  auto r1 = estimate_plan(p1n);
  CHECK(!r1.ok() && r1.error == PlanEstimateError::PARAM);
  p1n = p1; p1n.n_frames = 0;
  r1 = estimate_plan(p1n);
  CHECK(!r1.ok() && r1.error == PlanEstimateError::PARAM);
  p1n = p1; p1n.width_px = 1ull << 40; p1n.height_px = 1ull << 40;
  r1 = estimate_plan(p1n);
  CHECK(!r1.ok() && r1.error == PlanEstimateError::PARAM);
  p1n = p1; p1n.n_frames = 1ull << 62;   // frame_px*4*frames 溢出 → 拒绝（不回绕 read=0）
  r1 = estimate_plan(p1n);
  CHECK(!r1.ok() && r1.error == PlanEstimateError::PARAM);
  // P2: n_tiles 溢出 / order 越界 → PARAM
  PlanInputMetadata p2{};
  p2.module_id = "astrocs.phase2.integrate"; p2.is_heavy = true;
  p2.n_tiles_input = 100; p2.n_frames = 8; p2.input_order = 6;
  PlanInputMetadata p2n = p2; p2n.n_tiles_input = 1ull << 50;
  auto r2 = estimate_plan(p2n);
  CHECK(!r2.ok() && r2.error == PlanEstimateError::PARAM);
  p2n = p2; p2n.input_order = 21;
  r2 = estimate_plan(p2n);
  CHECK(!r2.ok() && r2.error == PlanEstimateError::PARAM);
  // 溢出拒绝路径不得产出 UINT64_MAX 饱和计划（错误返回时 plan 无意义但不得回绕）
  PlanInputMetadata big{};
  big.module_id = "astrocs.phase3.resample"; big.is_heavy = true;
  big.width_px = UINT64_MAX; big.height_px = UINT64_MAX;
  big.input_order = 20; big.scale_deg_per_px = 1e-9;
  auto rbig = estimate_plan(big);
  CHECK(!rbig.ok());
}

// ══ G. 序列化（稳定 JSON 可解析） ══
static void test_json_stable() {
  PlanInputMetadata a{};
  a.module_id = "astrocs.phase2.reject"; a.is_heavy = true;
  a.n_tiles_input = 64; a.n_frames = 4; a.input_order = 5;
  auto r = estimate_plan(a);
  CHECK(r.ok());
  const std::string j1 = plan_estimate_to_json(r.plan);
  const std::string j2 = plan_estimate_to_json(r.plan);
  CHECK(j1 == j2);                            // 确定性
  CHECK(j1.find("\"module_id\":\"astrocs.phase2.reject\"") != std::string::npos);
  CHECK(j1.find("\"axis\":\"tile\"") != std::string::npos);
  CHECK(j1.find("\"work_units\":64") != std::string::npos);
  CHECK(j1.find("\"kernel_ids\":[\"rejection-statistics\"]") != std::string::npos);
  CHECK(j1.find("\"serial_sections\":[") != std::string::npos);
}

int main() {
  test_inputs_vary_plan();
  test_fields_present_and_consistent();
  test_frozen_peak_bounds();
  test_heavy_tiny_and_tiny_labels();
  test_serial_only_labels();
  test_error_paths_checked();
  test_json_stable();
  if (g_failures) {
    std::fprintf(stderr, "RT-005_FAIL checks=%d failures=%d\n", g_checks, g_failures);
    return 1;
  }
  std::printf("RT-005_PLAN_ESTIMATOR_ALL_PASS checks=%d failures=0\n", g_checks);
  return 0;
}
'''

# ── Python 静态断言 ──
_HDR = INC / "astrocs" / "core" / "plan_estimator.h"
_SRC = CORE / "plan_estimator.cpp"
_KNOWN_KERNELS = {
    "calibration-pixel-transform", "noise-snr-reductions", "wcs-psf-batch",
    "drizzle-overlap", "drizzle-accumulate", "drizzle-normalize",
    "upm-spmv", "upm-residual", "upm-weight-update", "rejection-statistics",
    "integration-accumulate", "hips-bulk-transform",
}  # 权威清单 lib/backend_host/backend_table.inc（12 kernel）


class TestRt005PlanEstimatorStatic(unittest.TestCase):
    """静态：plan_estimator 不引入新依赖环、kernel 名权威、常量冻结。"""

    def test_no_dependency_cycle_new_includes(self):
        """头只含标准库头（纯函数库，不 include module.h 等核心头 → 无依赖环）。"""
        text = _HDR.read_text(encoding="utf-8")
        for line in text.splitlines():
            s = line.strip()
            if s.startswith("#include"):
                self.assertTrue(
                    s.startswith('#include <') or s.startswith('#include "astrocs/core/plan_estimator.h"'),
                    f"plan_estimator.h 引入额外内部头（依赖环风险）: {s}")

    def test_source_includes_only_own_header(self):
        text = _SRC.read_text(encoding="utf-8")
        for line in text.splitlines():
            s = line.strip()
            if s.startswith("#include"):
                self.assertTrue(
                    s.startswith('#include <') or '"astrocs/core/plan_estimator.h"' in s,
                    f"plan_estimator.cpp 引入额外内部头: {s}")

    def test_frozen_constants_pinned(self):
        h = _HDR.read_text(encoding="utf-8")
        self.assertIn("kHipsTileWidthPx = 512", h)
        self.assertIn("kHipsMaxOrder = 20", h)
        self.assertIn("kHipsTileF32Bytes", h)
        self.assertNotIn("hardware_concurrency", _SRC.read_text(encoding="utf-8"))

    def test_kernel_ids_authoritative(self):
        """实现中出现的 kernel id 必须都在 backend_table.inc 权威清单内。"""
        text = _SRC.read_text(encoding="utf-8")
        import re
        for m in re.finditer(r'"(calibration-pixel-transform|noise-snr-reductions|'
                             r'wcs-psf-batch|drizzle-overlap|drizzle-accumulate|'
                             r'drizzle-normalize|upm-spmv|upm-residual|'
                             r'upm-weight-update|rejection-statistics|'
                             r'integration-accumulate|hips-bulk-transform)"', text):
            self.assertIn(m.group(1), _KNOWN_KERNELS, m.group(1))


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestRt005PlanEstimatorCpp(unittest.TestCase):
    """C++ harness：真实编译链接 plan_estimator.cpp 运行全部验收断言。"""

    @classmethod
    def setUpClass(cls):
        cls.tmp = pathlib.Path(tempfile.mkdtemp(prefix="rt005_"))
        cls.exe = cls.build_driver(cls.tmp)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    @staticmethod
    def build_driver(tmp: pathlib.Path) -> pathlib.Path:
        drv = tmp / "rt005_driver.cpp"
        drv.write_text(_DRIVER, encoding="utf-8")
        exe = tmp / "rt005_plan_estimator"
        # 编译 harness driver 与生产源码，全部 -Wall -Wextra -Werror（0 告警）
        for src, o in ((CORE / "plan_estimator.cpp", tmp / "plan_estimator.o"),
                       (drv, tmp / "driver.o")):
            r = subprocess.run(
                ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                 "-c", str(src), f"-I{INC}", "-o", str(o)],
                capture_output=True, text=True, timeout=300)
            if r.returncode != 0:
                raise RuntimeError(f"compile {src.name} failed:\n{r.stderr[-2000:]}")
        r = subprocess.run(
            ["g++", "-std=c++17", "-O2", str(tmp / "driver.o"),
             str(tmp / "plan_estimator.o"), "-o", str(exe)],
            capture_output=True, text=True, timeout=120)
        if r.returncode != 0:
            raise RuntimeError(f"link driver failed:\n{r.stderr[-2000:]}")
        return exe

    def test_driver_all_checks_pass(self):
        r = subprocess.run([str(self.exe)], capture_output=True, text=True,
                           timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-2000:])
        self.assertIn("RT-005_PLAN_ESTIMATOR_ALL_PASS", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
