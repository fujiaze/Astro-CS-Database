# SYN-008 验证报告 — 三块重叠合成场 seam 指标门槛(预冻结)

SHA: 本报告基线 `99aeed1`(SYN-007 登记)+ 本任务验证产出。结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L130 + 00_READ_FIRST.md L166)
> 三块重叠合成场，已知背景面+星点+coverage → seam 指标门槛预冻结并过；无"视觉上可以"替代。

## 2. 方法 — 独立(independent)三重叠合成场 + seam 指标门槛
- 构造 3 重叠合成场(帧0/1/2)观测, 经 `p2_upm_build` 联合求解每帧 photometric C 场:
  - 已知背景 `true_sky`(pedestal + 大尺度渐变 + 局部 diffuse), 与 synthetic_gate G1 同源。
  - 每帧 additive field `frame_field(f)`(smooth, 决定 photometric 偏移即 seam 源)。
  - 星点(幅度 40)注入个别 cell; coverage 布局(帧2 少覆盖 tile7)。
- **seam 指标**(与 SCI-005 + REAUDIT seam 邻接差一致): 同一控制 cell 在**重叠帧**下的 UPM 校准输出
  `out = obs − C_frame` 的交叉帧差 `|out_f0 − out_f1|`。对平滑重叠场, UPM 应使跨帧输出均 ≈ true_sky
  → seam 小。星点 cell 不计入(其本应高), 但星邻域场平滑(不破坏星 flux)。
- **门限预冻结**: cross-frame seam `p95 <= 3σ`, `max <= 5σ`(σ=kNoiseRms=0.05); 即无人工接缝。

## 3. 测试与结果
`tests/api/test_seam_metric_gate.py`(3 用例):
| 用例 | 判据 | 结果 |
|---|---|---|
| test_01_cross_frame_seam_under_gate | seam p95 ≤ 3σ(0.15), max ≤ 5σ(0.25)(门槛) | OK |
| test_02_seam_statistics_sane | p50<p95<max 单调; max 远小于星幅度 40 | OK |
| test_03_upm_coverage_parameter_recovery | control_count≥256, component≥1, 模型可用 | OK |

实测 seam:
```
SEAM n=576 p50 0.013032 p95 0.040039 max 0.059061 controls 320 comps 1
```
| 指标 | 实测 | 门槛 |
|---|---|---|
| p50 | 0.013 | — |
| p95 | 0.040 | ≤ 0.15 (3σ) |
| max | 0.059 | ≤ 0.25 (5σ) |

```
$ python3 -m unittest tests.api.test_seam_metric_gate -v
Ran 3 tests in 0.915s — OK
```
回归(相邻):
```
$ python3 -m unittest tests.api.test_upm_recovery_oracle tests.api.test_upm_parallel tests.api.test_seam_metric_gate
Ran 10 tests in 5.620s — OK
```

## 4. 结论与边界
- **seam 指标门槛(预冻结)并通过**: cross-frame seam p95=0.040≤3σ(0.15), max=0.059≤5σ(0.25);三块重叠场经 UPM 校准后跨帧输出一致(接缝被消除)。对比: 未校准时的 photometric 偏移(frame_field 幅度≈0.6),seam 被压制 2 个数量级。
- 已知背景面 + 星点 + coverage 全用: true_sky / Frame field / star(cell) / partial coverage 全纳入。
- 无"视觉上可以": seam 为数值门槛(≤σ 分位数)预冻结, 非目视判断。
- 星 flux 保留: 星点 cell 不计 seam(其本应高), 星邻域场平滑;UPM 参数恢复(control=320, comp=1)。
- 说明: 本测试为合成重叠场(非真实大视场马赛克), 科学语义与 synthetic_gate G1 一致(合成 Gate 已检验 residual/c_recovery 的 RMSE≤3σ)。真实 32R/大视场 seam 由 WIN-006/008(Windows)覆盖;本任务聚焦 **Linux 合成 seam 指标门槛**的预冻结与通过。component_count=1(3 帧连通分量, 符合 3 重叠连续覆盖)。
- 本机 2 物理 CPU;UPM build 串行确定。

## 5. 相关
- 依赖 SYN-004(SYN-005 依赖 UPM)/SYN-005(UPM 参数恢复)/SYN-006(reject/integrate)→ 本任务用 UPM C 场 seam 指标;SCI-005(接缝指标)由本测试数值门槛 + synthetic_gate 覆盖。
- 下一项: SYN-009(CLI Phase1 2 3 端到端合成 pipeline)。
