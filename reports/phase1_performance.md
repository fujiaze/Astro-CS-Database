# Phase1 性能（V17 True Final Freeze）

## 队列

NGC1727 T2 H-alpha 1200s × 16 独立 exposure（4096²，Phase1 全链
FITS→calibration→plate solve→PSF→photometric→SNR→drizzle→HiPS order 7）。

## 分段 profile（真实日志，orchestrator 每阶段计时）

| 阶段 | before cold median | before cold p95 | 说明 |
| --- | --- | --- | --- |
| READ_FITS | 0.10s | 0.12s | CFITSIO 读取 4096² |
| CALIBRATE | 0.49s | 0.54s | bias/dark/flat 标准校准 |
| PLATESOLVE | 15.4s | 17.6s | 星点检测 + 本地 GaiaDR3 cone + SIP 求解 |
| PSF | 1.27s | 1.41s | 动态 PSF |
| PHOTOMETRIC | 5.86s | 5.97s | 本地 GaiaDR3SP 光谱查询 + 测光 |
| SNR/NSIDE | ~0s | ~0s | 帧级 SNR catalogue |
| DRIZZLE | 77.6s | 87.3s | ← 主导（冻结热路径，本机 CPU OpenMP） |
| HIPS_VERIFY | 0.03s | 0.04s | AIO reader 回读校验 |
| 整帧 wall | ~145s | ~160s | 16 帧全 rc=0 |

## 65s vs 150s 历史差异（必须解释）

- V14 的 65s 基线：NGC55 Red 全帧（不同目标/滤镜/曝光时长），且当时
  DRIZZLE 输出规模/目录查询面与 NGC1727 H-alpha 1200s 不同；
- 本队列：NGC1727 H-alpha 1200s 全 4096²；PLATESOLVE 本地 GaiaDR3 cone
  （无网络），PHOTOMETRIC 本地 GaiaDR3SP；DRIZZLE 写 order-7 HiPS
  （14 tile + hierarchy）；
- 结论：**不是算法回归**，是数据集与输出规模的差异；V17 只以同一队列的
  before/after 为比较基准。

## Cold vs Warm（V17 优化：platesolve hint）

同一 field 相邻 16 exposure，warm 模式从上一帧已解 CRVAL 写入下一帧
`platesolve.initial_ra_deg/initial_dec_deg`（仅搜索初始化，solver 仍逐帧
独立求解+验证，不复制 WCS；`tools/phase1_e2e_bench.py` 工具层实现，
不进入生产算法）。

| 指标 | before cold | after warm | 变化 |
| --- | --- | --- | --- |
| PLATESOLVE median | _待填_ | _待填_ | _待填_ |
| DRIZZLE median | _待填_ | _待填_ | _待填_ |
| 整帧 wall median | _待填_ | _待填_ | _待填_ |

## 3-runs 重复性（before/after 各 3 次）

_待 runs 完成后填写_（run1/run2/run3 的 wall median 与 p95）。

## 结论（runs 完成后更新）

```text
PHASE1_PERFORMANCE_BASELINE = <UPDATE_AFTER_RUNS>
```

科学等价：hint 只影响搜索初始化；每帧 WCS 仍独立求解 + robust refine +
verify（HIPS_VERIFY rc=0）；Drizzle/其余阶段输入不变。
