# Algorithm Doc Review — Stage B (V19R8)

Date: 2026-08-22  
Scope: `工程控制/docs 00/05/07/08/09/10/11/17/18/19/24/25 + PHASE2_IMPLEMENTATION/INTERFACE_FREEZE + docs/development/CONFIG_SCHEMA.md + docs/algorithms/* + memory.md`  
Tool: `tools/docs_machine_consistency.py` + `tools/config_consistency_check.py` + 人工核验算法步骤/参数/阈值/数据流/异常分支与 Stage A 科学定义对应性  
Commit: Stage B 单一目的提交

## 结论总览

| 项 | 结论 |
|---|---|
| docs_machine_consistency | PASS 9/9 |
| config_consistency_check | PASS mismatches=[] |
| P0 算法自洽性漏洞 | 0 |
| P1 可优化/待澄清 | 4 (非阻塞) |
| 与 Stage A 科学定义一一对应性 | 通过（11/11 算法文档均有 SCI→ALG→实现锚点） |
| 本阶段对 `工程控制/docs` 算法文档的修改 | 2 处最小编辑（措辞/阈值注释，不改算法） |

## 逐文档 Verdict

### 00_PHASE_GOAL_AND_BOUNDARIES — PASS
- 目标：在 CLI 下真实数据完成 校准→共享检测→PlateSolve→WCS/SIP→PSF→Gaia光谱积分→测光→SNR→Drizzle→HISS→32帧梯度/稳健叠加→HCSD→浏览器流畅查看，边界清晰（不做完整业务 GUI、不让 JS 直读 PipelineFrame、不重写无回归 PlateSolve、不先改 HCSD 格式、不扩展全目标生产先验银心）符合 V19R8 阶段目标。

### 05 WCS 坐标约定与闭环 — PASS
- 三层验证（A Solver Fit / B Serialized WCS 硬 Gate / C Blind Catalog 诊断）不混残差符合科学边界。
- B 层硬门 7 条阈值（`external_to_internal_prediction median≤0.05 p99≤0.20`、`external_to_detector_rms≤max(0.35,2×solver_rms+0.05)`、`median≤0.50 p90≤1.00 p99≤2.00`、`|mean|≤0.25` 无翻转/尺度漂移、四象限/边缘分布、WCS 数值闭环 `median≤1e-6`）与 `docs/science/ASTROMETRY.md` 坐标约定、SIP 阶数、极区分支一致；25 权威匹配对导出契约（pair_id/gaia_source_id/ra_dec/detector/internal_pred/residual/flags/epoch）与 B 层输入闭合。
- 数值阈值与 `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp` (CRPIX/Y-down/SIP 前逆)、`ipv_sip.cpp` (IRLS 15次/ε1e-6/Huber1.345)、`gaia_client.c` (polar `C=π/2`/`C45=π/(2√2)`、RA `cos(dec)` 环绕) 锚点一致。

### 07 SNR 与 HISS provenance — PASS
- 要求：Photometric 成功后生成有限 `snr_phot` + PSF 控制点 + HISS 稀疏 SNR，像素→球面用完整 WCS/SIP；HISS 记录 format_version/source_file/hash/T1-4 system_id/filter/Master hash/PlateSolve mode/WCS closure/photometry/SNR point count/nside/pixfrac/commit/config hash；正式 32 帧 `has_snr=true` 否则显式失败不静默等权。
- 与 `docs/science/NOISE_MODEL.md` (NoiseWeightModelV1 8×8 patch / 5σ≤2轮 / 平面场 / floor 1e-12 / 诊断 gain 模型不进生产)、`docs/algorithms/NOISE_ESTIMATION.md`、PHASE2_UPM `control_ivar` 链式一致。

### 08 Stage1 真实数据全量验证 — PASS
- 三层：T1-T4 代表帧 + 32 帧银心 + 全部 Light；每帧记录 CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE/HISS 成功/失败/耗时/峰值内存/关键指标；必需阶段 skipped=失败；区分算法失败/输入损坏/校准解析失败/资源超限，非仅 exit code。符合 L3 PIPELINE。

### 09 银心三片 Red 马赛克 32 帧=11+11+10 — PASS
- 冻结 panel1 11 / panel2 11 / panel3 10 共 32，不在 Spec 硬编码绝对路径由 TestData/Header 自动确认；6 阶段（单片连通→32 HISS→梯度关闭 HCSD→梯度开启 HCSD→已知梯度注入恢复→浏览器同视角同 STF）；重叠图 panel1↔panel2、panel2↔panel3 连通，panel1↔panel3 可不直连。

### 10 球面梯度科学验证 — PASS
- 证明：注入低频恢复 + 降低真实接缝 + 不损星点/扩展结构；机器指标（overlap edge 样本数、校正前后 median/MAD、RMS、幅值分位数、覆盖/外推/连通/条件数）；最低验收 4 条（峰峰恢复误差≤10%、残留 RMS≤10%、真实接缝中位差≥50%、稳定星通量比 median[0.98,1.02] p95≤5%、禁止无约束外推、无重叠必失败）；若天体与背景不可分需 mask/稳健采样+注入测试不得仅视觉。阈值明确无歧义。

### 11 稳健叠加与 HCSD — PASS
- 要求：`has_snr=true` 32 份 HISS、`SNR²` 权重可追溯、每像素/Tile 覆盖数/有效样本数/拒绝数摘要；验证覆盖 3样本以上剔除/合成卫星线热点拒绝/SNR² vs 等权符合公式/确定性或容差/HCSD leaf/sorted ipix/meta 可独立读取；Orchestrator 进度真实可拆 `MOSAIC_STACK` 或 `gradient/robust_stack/write_hcsd`，禁止空 `STACK`。

### 17 测试架构与统一全量入口 — PASS
- 10 层入口 contract/schema → unit/component → 710 PlateSolve → T1-T4 → 全 TestData → 32 HISS → gradient injection/real mosaic → HCSD round-trip → browser backend/unit → performance/visual smoke；输出 JUnit/JSON/Markdown；昂贵步骤缓存绑定 input hash/commit/config hash/工具版本。

### 18 代码修改地图 — PASS
- 重点面 8 项：`lib/plate_solve/cpp/ipv` (WCS/SIP/诊断)、`lib/orchestrator` (provenance/进度)、`lib/photometric_calib` (投影/唯一匹配)、`lib/snr_estimator`+Drizzle、 `lib/calibration` (T1-T4 Master)、`lib/astro_image_io` (HCSD 读句柄/batch leaf)、`lib/healpix_db/healpix_stack` (已归档 archived/legacy 不重建)、`lib/healpix_db/healpix_browser_qt`；警示“先冻结接口再改模块，避免三套 WCS 变换”正确。

### 19 路线、Gate 与并行计划 — PASS
- G9 v1.1基线/共享检测、G10 T1-T4、G11 WCS闭环v2/710无回归、G12 SNR/HISS/全量、G13 32梯度/叠加、G14 异步IO/LRU、G15 GPU、G16 统一回归/发布候选；P15 浏览器可在 G10 后与 P11-P13 并行，P16 视觉验收依赖 G13 HCSD，并行约束正确。

### 24 WCS 验证架构 v2 — PASS
- 同 05，但作为独立 Gate 文档详述阈值与失败处理（C 失败而 B 通过进 Photometric 不回改 WCS；门限不合理用分布+ADR 调整禁止临时放宽），边界清晰。

### 25 权威匹配对导出契约 — PASS
- 任务级 9 字段 + 每 pair 10 字段（pair_id/gaia_source_id/ra_dec/detector/internal_pred/residual/weight/inlier/flags/saturated/edge/blend/quality）+ JSONL/Parquet 含单位/基准 + 生命周期 PlateSolve 冻结后 Orchestrator evidence 存档，可归档不进 HISS。

### PHASE2_IMPLEMENTATION / PHASE2_INTERFACE_FREEZE — PASS
- W0 盘点 34A532A2…B2EB308 / W1-W12 路径、W2 接口冻结（UnifiedPhotometryConfig/Result、OperationId、Rejection C API、AioHipsMosaicInput）与当前 `lib/phase2/include/astro/phase2/*` (p2_upm_build/sampler/rejection/integrate) 对应，旧 HICS/每帧独立梯度 Gi/重叠区独有控制点/乘性 scale/新 I/O DLL 已撤销声明正确；`healpix_stack` archived/legacy 不重建与 `docs/architecture/MODULE_MAP.md` 一致。**注意**：本冻结契约中的接口签名为 W2 快照，生产以 `lib/phase2/include/astro/phase2/*.h` 为准（详见 P1-03）。

### docs/development/CONFIG_SCHEMA.md — PASS
- Stage2 model/integration/output 三段默认值与 `lib/phase2/src/stage2_common.cpp` 双实现一致；rejection 11 类型（none/sigma/winsorized/averaged/linear-fit/ESD/RCR/percentile/median_sigma/minmax/auto）+ typed params + profile `wbpp_2_9_1` (canonical) / `wbpp_current` alias / `astrocs_adaptive`、`auto` nominal 解析非 per-pixel、normalization/large_scale、percentile RCR 约束等均冻结；旧顶层 low/high/max_iterations/min_samples 已删除硬错迁 `tools/migrate_stage2_config.py` 已标注。

### docs/algorithms/* (11篇) — PASS
- CALIBRATION / PLATESOLVE / STAR_PSF / NOISE_ESTIMATION / DRIZZLE_GEOMETRY / HEALPIX_MAPPING / PHOTOMETRIC_FIT / PHASE2_SAMPLER / UPM_SOLVER / REJECTION / INTEGRATION 均含 输入/输出/前后置/不变量/伪代码/复杂度/并行/数值风险/fast-reference-oracle/ID，与科学文档一一对应：
  - NOISE_ESTIMATION：`σ_bg=1.482602218505602·MAD` + 5σ≤2轮 + fixed conservative `rmax` + `gain` 仅诊断 SNR-005 已锚点。
  - UPM_SOLVER：`raw_w=quality×control_ivar` (rc=2 缺 ivar 失败) + per-control `×geometric_reliability` 归一、`control_variance=k_corr·(π/2)·σ²/N_retained`、`k_corr=1.4` per-frame 回退、`gauge=min frame_id`、`parameter_rows↔frame_id_by_index` 同长无重复。
  - DRIZZLE_GEOMETRY：球面 S-H 裁剪 Girard 面积 + 候选包围圆 3 层缓冲 1.25/3.0/1.25×1.15 + bounded target-ipix cache LRU 8192 + operation counters 与 `05` / `NOISE_MODEL` k_corr MC 一致。
  - PHASE2_SAMPLER：patch median + MAD + `control_k_corr` + `obs.ivar` 仅诊断。
  - REJECTION/INTEGRATION：`P2_*` 全集合与头文件 name+value 全量校验、`weight==0` 合法不贡献、`ZERO_VALID_WEIGHT` vs `ALL_REJECTED` 区分、policy/reducer 分离已冻结。
  - PLATESOLVE/HEALPIX_MAPPING/STAR_PSF/PHOTOMETRIC_FIT/CALIBRATION 均与 `SCI-AST/PSF/PHOT/CAL/DRZ` 闭合。

### 马赛克叠加梯度建模计划.md — PASS (历史设计)
- 设计：背景中位数+Gaia星拒绝+球面TPS+ SNR加权 Gauss-Seidel + gauge 加权均值归零；与当前 UPM 加性场 `C_f` 同构但历史计划中 g_i 为每帧独立 TPS 字段，当前实现已收敛为单一 UPM 全局场（覆盖 union 全量控制点 + 单帧区 harmonic continuation），架构以 `PHASE2_IMPLEMENTATION` + `UPM_SOLVER` 为准，历史计划仅作设计溯源，不作当前算法权威。

## 与 Stage A 一一对应性核验

| 科学定义 | 算法文档 | 对应性 |
|---|---|---|
| SCI-CAL | CALIBRATION_ALGORITHMS | cal双分支/flat_norm median→1.0 clamp 0.1 均有锚点 |
| SCI-AST | PLATESOLVE + HEALPIX_MAPPING | 0-based↔1-based/SIP前逆/Y-down/极区 Lipschitz 均有 |
| SCI-PSF | STAR_PSF_ALGORITHMS | Moffat4 β=4 / 1.230310 / flux 2πA/3 / residual_scale/q_psf/θ消歧 均有 |
| SCI-NOISE | NOISE_ESTIMATION + PHASE2_SAMPLER | 8×8/fixed rmax/MAD/裁剪/平面场/floor/gain诊断 均有 |
| SCI-DRZ | DRIZZLE_GEOMETRY | F_p/D_p/S_p + sumVarNum/D² + 三层缓冲 + false_negative=0 均有 |
| SCI-PHOT | PHOTOMETRIC_FIT | r_i/Tukey 4.685/50/1e-6/S= MAD/0.6745/scale/sigma_residual 均有 |
| SCI-UPM | UPM_SOLVER + PHASE2_SAMPLER | C_f 双线性/Huber IRLS/control_ivar/几何可靠性/gauge/persist 均有 |
| SCI-REJ/SCI-INT | REJECTION + INTEGRATION | 7方法/typed params/eligibility/UNDERDETERMINED/large_scale + ivar/max support 均有 |
| SCI-UNC | DRIZZLE_GEOMETRY + UPM/NOISE | Cov/ρ 0.19/0.57 + k_corr·π/2/N_retained 均有 |
| 09 32=11+11+10 | 00/09/17 | 分组/6阶段/连通/哈希/进度 均有 |
| 10 梯度 | 10 + 马赛克计划(历史) + UPM_SOLVER | 注入恢复≤10%/接缝≥50%/星通量[0.98,1.02] 与 UPM 求解一致 |
| 11 叠加 | 11 + REJECTION/INTEGRATION | has_snr/SNR²/3样本剔除/确定性/HCSD 可读 均有 |

**11/11 闭合，无算法自洽性漏洞。**

## 问题清单（P0 / P1）

| ID | 级别 | 位置 | 描述 | 处置 |
|---|---|---|---|---|
| ALG-B-01 | P1 | `工程控制/docs/马赛克叠加梯度建模计划.md` vs `docs/algorithms/UPM_SOLVER.md` | 历史设计为每帧独立 g_i TPS 场，当前已收敛为单一 UPM 全局场，文档权威以 UPM_SOLVER/SCI-PHASE2_UPM 为准，历史计划未在正文顶部加“已收敛/不作权威”显式声明 | 本阶段在该计划顶部增补一行“历史设计溯源，当前权威见 SCI-PHASE2_UPM/UPM_SOLVER” |
| ALG-B-02 | P1 | `docs/algorithms/PHASE2_SAMPLER.md` | `control_k_corr` per-frame 回退 1.4 已在正文，但未显式标注“缺省 provenance 时回退”与 `sampler.cpp:672` 行锚一致 | 已满足，保持 |
| ALG-B-03 | P1 | `工程控制/docs/PHASE2_INTERFACE_FREEZE/INTERFACE_FREEZE_CONTRACT.md` | W2 快照中 `UnifiedPhotometryConfig`/`AioHipsMosaicInput` 为旧 HICS 时代快照，与现 `lib/phase2/include/astro/phase2/*.h` 已演进，文档未显式标注“以头文件为准” | 本阶段在该 contract 顶部增补权威声明 |
| ALG-B-04 | P1 | `docs/algorithms/NOISE_ESTIMATION.md` | `use_gain_model` 默认 0 empirical 优先已写，但与 `CONFIG_SCHEMA.md` 中 Stage2 model `snr_weight_mode` 命名易混（前者为 noise 增益模型开关，后者为 sampler SNR 权重模式） | 已满足，保持，入口文档区分 |

**P0 = 0**。

## 机检

```
tools/docs_machine_consistency.py  PASS 9/9
tools/config_consistency_check.py  PASS mismatches=[]
```

## 本阶段修改

- `工程控制/docs/马赛克叠加梯度建模计划.md`：顶部增补历史溯源声明（最小编辑）。
- `工程控制/docs/PHASE2_INTERFACE_FREEZE/INTERFACE_FREEZE_CONTRACT.md`：顶部增补权威以头文件为准声明（最小编辑）。

## 遗留风险

- `healpix_stack` 已归档不重建，当前 HCSD 产出由 `phase2` + `astro_image_io` (aio_hips) 承担，若未来要恢复堆栈需重新冻结接口（P1，非阻塞）。
- 浏览器异步 I/O 与 GPU Renderer (12-15) 尚未在算法文档展开，仅在 18 地图中提及，后续 Stage C 将在 `docs/ARCHITECTURE.md` 中锚定（P1）。

