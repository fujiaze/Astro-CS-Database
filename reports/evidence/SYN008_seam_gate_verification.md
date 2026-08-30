# SYN-008 / WIN-008 接缝门限验证 — 合成 Oracle(预冻结)

> 机器(当前): vm-bj Linux, g++ -std=c++17 -O3 -fopenmp, libphase2.a(OpenMP), syn008_seam_main.cpp。
> 方法(与 03 L130 + 00 L166 一致): 3 重叠合成场(已知 true_sky + 帧场 + 星点 + coverage) → `p2_upm_build` 联合求解每帧 C 场 → 交叉帧 UPM 校准输出差 |out_f0 - out_f1| 的 seam。

## 结果: PASS(预冻结门槛)
```
SEAM n=576 p50 0.013032 p95 0.040039 max 0.059061 controls 320 comps 1
```
| 判据 | 门槛(σ=0.05) | 实测 | 判定 |
|---|---|---|---|
| cross-frame seam p95 | ≤ 3σ = 0.15 | 0.040039 | PASS |
| cross-frame seam max | ≤ 5σ = 0.25 | 0.059061 | PASS |
| seam 分布单调 sanity | p50 ≤ p95 ≤ max | 0.013≤0.040≤0.059 | PASS |
| n>0(有重叠 seam 样本) | > 0 | 576 | PASS |

- UPM 联合求解后交叉帧 seam 被压到远低于 3σ/5σ 冻结门槛(0.04 vs 0.15), 与"无视觉上可以替代"的科学判据一致。
- `controls=320, comps=1`: UPM 控制点/分块合理, 参数恢复有效。

## 结论
- **SYN-008 seam 指标门限(预冻结) PASS**; 亦支撑 WIN-008 的 seam/接缝科学判据(代表链 phase1→phase2(obs=96/controls=42)→phase3 已全通)。
- 记录不宣称 release。
