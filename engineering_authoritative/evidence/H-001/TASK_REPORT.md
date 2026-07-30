# H-001 任务执行报告 — 阶段资源监测和动态成本估算

- 任务编号: H-001
- 执行日期: 2026-07-30
- 依赖: B-002 (已完成)
- 状态: **已完成**

## 1. 实现摘要

完成了 Stage1 资源监测框架和动态成本估算器, 包括:

1. **ResourceMonitor**: CPU/RAM(RSS+Commit)/Disk I/O/温度监测框架, 支持后台线程采样、滚动窗口统计、活跃阶段追踪
2. **FrameCostEstimator**: 7 个 Stage1 阶段的参数化成本模型, 基于 B-002 三帧统计校准
3. **动态成本估算器**: 输入帧参数(尺寸/星数/Gaia数/nside), 预测各阶段峰值内存、时长、CPU/IO强度
4. **B-002 基线验证**: 预测总耗时误差 < 3.4% (阈值 15%)

## 2. 关键设计决策

### 2.1 资源监测框架设计
- **可注入式采样器**: `ResourceSampler` 基类 + `PsutilSampler`(生产) + `MockSampler`(测试), 解耦采集逻辑与监测逻辑
- **滚动窗口**: 默认 60s 窗口, 自动裁剪过期样本, 提供 mean/max/p95 统计
- **线程安全**: mutex 保护采样队列和活跃阶段字典
- **不依赖 OS swap**: 显式追踪 RSS 与 Commit, 通过 `get_available_memory()` 提供 Commit Limit - Commit

### 2.2 成本模型校准 (B-002 三帧)
| 阶段 | 内存模型 | 时长模型 | 校准来源 |
|------|----------|----------|----------|
| READ_FITS | W*H*4 + 64KB | 5 ns/px | T2/T3/T4 平均 |
| CALIBRATE | 3x image (input+output+master) | 45 ns/px | T2/T3/T4 平均 |
| PLATESOLVE | image + stars + gaia + solver(50MB) | base(0.76s) + 0.146ms/gaia + wide_field_penalty | T2/T3/T4 回归 |
| PSF | image + n_stars*72B | 0.15 ms/star | T2/T3/T4 平均 |
| PHOTOMETRIC | image + gaia + spectrum(8MB) | 0.16 ms/matched | T2/T3/T4 (match率0.9) |
| SNR | 2MB + psf | 2ms (常数) | T2/T3/T4 |
| DRIZZLE | 12*nside^2*4*3 + image | 1.312 ns/src_px + 0.0267 ns/hp_px | T2/T3/T4 双变量回归 |

### 2.3 DRIZZLE 双变量时长模型
DRIZZLE 占总耗时 ~90%, 是成本估算的关键。采用双变量线性模型:
```
duration = c_src * n_source_pixels + c_hp * 12 * nside^2
```
- `c_src = 1.312 ns/源像素` (图像扫描+投影)
- `c_hp = 0.0267 ns/HEALPix像素` (地图写入)

验证结果:
- T2 (nside=2048): 预测 23.36s, 实际 24.06s, 误差 2.9%
- T3 (nside=2048): 预测 23.36s, 实际 22.55s, 误差 3.5%
- T4 (nside=512): 预测 21.34s, 实际 21.34s, 误差 0.0%

### 2.4 内存峰值分析
| 帧 | nside | DRIZZLE 峰值 | 全帧峰值 | 最坏阶段 |
|----|-------|-------------|---------|---------|
| T2 | 2048 | 671 MB | 671 MB | DRIZZLE |
| T3 | 2048 | 671 MB | 671 MB | DRIZZLE |
| T4 | 512 | 103 MB | 194 MB | CALIBRATE |

**关键发现**: T4 (宽场, nside=512) 的最坏阶段是 CALIBRATE (194MB) 而非 DRIZZLE (103MB), 因为低 nside 的 HEALPix 地图远小于高 nside。T2/T3 (nside=2048) 的 DRIZZLE 内存是 T4 的 6.5 倍。

### 2.5 不确定度模型
各阶段预测的不确定度比例 (相对 predicted_peak_bytes):
- READ_FITS: 10%, CALIBRATE: 15%, PLATESOLVE: 30% (Gaia catalog 变化大)
- PSF: 15%, PHOTOMETRIC: 20%, SNR: 50% (绝对值小), DRIZZLE: 20%

### 2.6 已知限制
- **PHOTOMETRIC 时长**: T2 误差 314.9% (预测 0.174s vs 实际 0.042s), 因 match 率(1095/1210=90%)与模型假设一致但常数偏高; T4 误差 218.7% (match率 1670/6021=28%, 远低于模型假设 90%)。绝对误差小(<0.6s), 不影响总误差 < 15%。
- **温度采集**: psutil.sensors_temperatures() 仅 Linux 可用, Windows 下返回 None。
- **3 帧校准**: 仅 3 个数据点, 模型泛化性有限, 后续 P11 批量运行后可增量校准。

## 3. 测试结果

```
H-001 单元测试: 94 PASS, 0 FAIL

Part 1: ResourceMonitor (6 测试) — 采样/滚动窗口/统计/活跃阶段/可用内存/JSON
Part 2: FrameCostEstimator (5 测试) — T2/T4估算/内存关系/高内存标记/契约符合性
Part 3: B-002 基线验证 — 3 帧总耗时误差 < 3.4% (阈值 15%), 判定 PASS
Part 4: 便捷函数 — estimate_frame 接口验证
```

## 4. 交付物

### Python 原型 (engineering_authoritative/evidence/H-001/)
- `baseline.json` — B-002 三帧基线数据 (结构化)
- `resource_monitor.py` — 资源监测框架 (ResourceMonitor + 采样器)
- `cost_estimator.py` — 动态成本估算器 (FrameCostEstimator + 7阶段模型)
- `test_h001.py` — 单元测试 (94 项断言)
- `TASK_REPORT.md` — 本报告

### C++ 头文件骨架 (lib/orchestrator/cpp/include/)
- `resource_monitor.h` — ResourceMonitor + FrameCostEstimator 接口定义 (待实现 .cpp)

### 契约符合性
- 输出符合 `resource_profile.schema.json`: frame_id / stage / predicted_peak_bytes / uncertainty_bytes / actual_peak_bytes / cpu_intensity / io_intensity

## 5. 下一任务前置条件

- H-002 (内存预约/CPU回滞/准入控制) 依赖本任务的:
  - `FrameCostEstimator.estimate()` — 提供各阶段 predicted_peak_bytes + uncertainty_bytes
  - `FrameCostEstimator.estimate_worst_next_frame()` — 提供最坏下一帧内存需求
  - `ResourceMonitor.get_available_memory()` — 提供当前可用内存
  - `ResourceMonitor.get_cpu_load()` — 提供 CPU 负载 (回滞判断)
  - `HIGH_MEMORY_STAGES` — 标记需预约的阶段 (PLATESOLVE/DRIZZLE)
