# RELEASE-02 夜间自主工作记录（2026-09-19 00:00~09:00）

负责人授权：自主执行，先视觉验收，无问题则做优化（调度/算法复杂度/利用率），不打扰。

## A. L4 重建结果（R 通道 49 帧，T2+T3 汇总）

**normalize（Phase1）：49/49 产品，全部 `count_consistent=true`** —— P0-21 修复在真实数据上验证通过。
12 个配置逐项：t2_m1=2, t2_m2=4, t2_m3=2, t2_m4=3, t2_m5=3, t2_m6=2, t3_m1=6, t3_m2=4, t3_m3=6, t3_m4=6, t3_m5=5, t3_m6=6。
Phase1 墙钟 3438s。

## B. Phase1 逐阶段性能（探针 × 资源时间序列对齐）

| 阶段 | 总耗时(s) | 占已计 | **平均核数** |
|---|---|---|---|
| **drizzle** | **2076.2** | **75.4%** | **1.21** |
| star_psf | 175.9 | 6.4% | 2.92 |
| noise | 172.0 | 6.2% | 7.53 |
| photometry | 159.1 | 5.8% | 3.34 |
| wcs | 115.0 | 4.2% | 9.62 |
| calibrate | 46.4 | 1.7% | 1.00 |
| cosmetic | 8.1 | 0.3% | 0.61 |

**结论：drizzle 是唯一主要热点，且基本单线程（1.21 核）**；逐帧耗时极均匀 36.5–49.7s（49 帧）⇒ 逐像素固定成本；
4096²/42s = **2.5 µs/像素**。`wcs`/`noise` 能到 7.5–9.6 核，说明框架并行能力可用。
详见 `reports/RELEASE-02/perf-phase1-report.md`；复现 `python3 run/RELEASE-02/L4-rebuild/stage_cpu.py`。

## C. 阻塞级缺陷：Phase2 权重链在真实数据上 fail-closed

`weight_mode=2` 报 `unclosed_invalid_reference_flux`，真实原因（stderr 中文被 locale 吞成 `?????`）：
**`ASTROCS_REFERENCE_FLUX 逐帧不一致`**。

- Phase1（HUB-B）每帧写**自己的** F_ref：实测 1728.8 ~ 11882.9，同组两帧 3492.37 vs 3366.26；
- Phase2（HUB-A）`module_adapters.cpp:5379-5380` 要求组内 F_ref **rtol 1e-9 一致** ⇒ 必然失败；
- 权重链 API 只接受**单个** group-level F_ref。

**科学疑点**：`w_f = SNR_f²/F_ref²`。逐帧 F_ref ⇒ `w_f = 1/σ_f²`（正确逆方差）；
公共 F_ref ⇒ 权重被 `F_ref,f²` 额外加权（除非帧已归一到公共通量标度）。
已派 WEIGHT-SCI 分片按 §8 查证流程裁决（文档+代码+文献三方）。

**注意：这是 fail-closed 而非静默降级 —— 行为方向正确，但生产权重链当前产出不了权重。**

## D. 视觉验收用的临时通道

为取得可视产品，用合法的 `weight_mode=1`（等权；非被删除的 `legacy_allow_weight_fallback` 假绿路径）重跑 mosaic。
**注意**：等权不影响排异（卫星线去除取决于排异，不取决于权重），故视觉验收仍有效；但**权重链本身未经真实数据验证**。

## E. 另一处需注意
`[sky_plane] build FAILED rc=6 reduced normal matrix not SPD -> explicit fallback to UPM C field` ——
天光面构建在真实数据上失败并**显式回退**（未静默）。需查 rc=6 的数值原因（法方程非正定）。
