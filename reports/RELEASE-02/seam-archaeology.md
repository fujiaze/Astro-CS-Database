# RELEASE-02 接缝历史考古（SEAM-ARCH）

- 任务：回答「以前的版本是怎么把接缝彻底解决掉的、为什么现在失效、怎么恢复」。
- 归因人：SEAM-ARCH。**零 git 写权限；未改任何生产代码/文档；未跑 ninja/cmake/ctest**。
- 日期：2026-09-19。环境：`TMPDIR=/dev/shm/astrocs_arch`；证据/脚本落 `run/RELEASE-02/seam-arch/`。
- 只读输入：`run/RELEASE-02/L4-rebuild/*`（生产 phase2 产物）、`build/` 已有测试二进制、git 历史。

---

## 0 结论速览（TL;DR）

| 问题 | 结论 |
|---|---|
| **①「以前」指什么** | **把所有帧归一化到一个公共面**。设计权威：`docs/science/PHASE2_UPM.md:7`「建立唯一的联合加性光度模型 UPM，消除逐帧背景/零点差，使校准后样本 `calibrated = raw − C_f(p)` 在全域可比」；`docs/plugins/algorithms_phase2/11_upm.md` §4.3「每帧在参考面之上只拟合自己的平缓梯度 `δ_k(x)`，把该帧背景梯度校准到统一大平面」。实现载体 = **UPM 加性 C 场**（`lib/algorithms/coverage/src/upm.cpp`，gauge=参考帧 C≡0）。它保留公共面、只扣帧间差（负责人「多退少补」）。 |
| **②它现在还在吗** | **不在了——生产 UPM 是空操作**。实测 `p2_upm_model.bin`：`iterations=1, converged=1, objective=0.0, M≡0, C 非零项=0`（4 个 L4 运行全部如此）。`p2_upm_calibrate_block` 逐位返回输入。 |
| **根因** | **`upm.cpp:569` 的绝对阈值**：`if (sums[ck] > 1e-12) … else raw_w[i]=0.0;`。生产 `control_ivar` 中位 **5.6e-22**，每 control 的 `Σcontrol_ivar` 中位 **5.1e-21**；**29336/29336 个 control 全部 < 1e-12 ⇒ 100% 权重被清零** ⇒ M=0、C=0、objective=0。 |
| **③现在生产做的是什么** | 因为 C≡0，`corrected = raw − C − b_k = raw − b_k` ⇒ **把整个背景面（B_ref+δ_k）剪掉**，且 δ_k 的帧子集均值跳变注入接缝（seam-attribution.md：δ_k 台阶 +4.22e11 vs 原始帧 +0.93e11）。 |
| **④`raw − b_k` 谁引入的** | `324281c2`（stage2.cpp 写 `(raw−C−b_k)/g_k`，新增 sky_plane 模块）→ `98e529ec`（`module_adapters.cpp` 生产逐像素扣 b_k）。**从未有过字面 `raw − δ_k` 的实现**；只有 δ 系数访问器 `p2_sky_plane_frame_delta`（`sky_plane.h:236`，仅测试调用）。 |
| **⑤接缝门现在怎样** | **4 道门全部 PASS**（G1–G4，见 §2.6），但**全是合成、权重尺度 O(1)**，无法覆盖生产 ~1e-22 尺度 ⇒ **门太弱，抓不住真实接缝**。 |
| **⑥裁决现状** | `affe6314` SD-22 已裁决改为 **B 口径 `corrected_k = raw_k − δ_k`（保留 B_ref）**；工作树已有未提交的 FIX-GK 改动（`stage2.cpp`/`module_adapters.cpp` 用新 API `p2_sky_plane_eval_delta`）。**但 UPM C 场仍死**（本次未改 `upm.cpp:569`）。 |

---

## 1 ①「以前」到底指什么

### 1.1 设计权威（「归一化到公共面」的原文）

- `docs/science/PHASE2_UPM.md:7`（SCI-UPM-001，FROZEN）：
  > **目的**：在多帧覆盖并集上建立唯一的联合加性光度模型 UPM，**消除逐帧背景/零点差**，使校准后样本 `calibrated = raw − C_f(p)` 在全域可比。
- `docs/science/PHASE2_UPM.md:47-50`：`calibrated_f(p) = raw_f(p) − C_f(p)`，`C_f(p)` = 8×8 control cell 双线性。
- `docs/plugins/algorithms_phase2/11_upm.md:26-31`：
  > `b_k(x) = B_ref(x) + δ_k(x)`；`B_ref(x)`：全部帧联合构建的**参考天光面**；`δ_k(x)`：第 k 帧相对参考面的平缓梯度修正。
- `docs/plugins/algorithms_phase2/11_upm.md` §4.3（`:49-50`）：
  > 全部帧的天光采样点经 WCS 投到同一球面坐标，联合拟合参考面 `B_ref(x)`；**每帧在参考面之上只拟合自己的平缓梯度 `δ_k(x)`，把该帧背景梯度校准到统一大平面，保证叠加后背景平滑连续**。

**这就是负责人说的「把所有帧归一化到平滑的公共面」「多退少补」**：公共面（UPM 的 `M` / sky_plane 的 `B_ref`）保留，只消除帧间差异。

### 1.2 实现载体：UPM 加性 C 场（历史机制）

- 模型：`y_ik = M_k + C_ik`（`M` per-control 公共潜在面；`C` per-frame per-cell 相对偏差），gauge = 分量参考帧 `C_ref ≡ 0`（`upm.cpp:801-805, 840-844`）。
- 应用：`calibrated = raw − C_f(p)`（`upm.cpp:1299` `output_signal[i] = input_signal[i] − c;`）。
- 该机制自 `32d84795`（phase2-v2，2026-08-11，"R1 spatial additive UPM (M_k + C_i(p) graph-Laplacian)"）起存在，是 RELEASE-01/02 唯一的生产背景归一化路径。
- **图拉普拉斯平滑 `smoothing_lambda`**（把单帧区/无观测节点的 C 向邻域延拓，即 SEAM-002/v13 的「全几何 Laplacian continuation」）也在该实现内（`upm.cpp:515, 592`）。

### 1.3 关键否定：**从未实现过字面的 `raw − δ_k`**

- `p2_sky_plane_eval`/`eval_block`（`sky_plane.h:224/230`）恒返回 `b_k = B_ref + δ_k + gauge_shift`；**没有 B_ref-only 或 δ_k-only 的块求值接口**。
- 唯一 δ 接口是 `p2_sky_plane_frame_delta`（`sky_plane.h:236`，返回 δ 系数），**仅被 `tests/unit/v6_p2_sky/v6_p2_sky_test.cpp:178,184,207` 调用**。
- 工作树 FIX-GK 新增了 `p2_sky_plane_eval_delta`/`eval_delta_block`（`sky_plane.h:242/248`，`sky_plane.cpp:1156/1191`），实现 `δ_k = gauge_shift + δ_k 多项式项` —— 这是**第一次**有 δ-only 求值路径（未提交）。
- 结论：负责人记忆中的「以前的方法」在**设计层**一直存在（11_upm.md §4.3），在**代码层**由 UPM C 场承担；**字面 `raw − δ_k` 是 SD-22 之后才出现的**。

### 1.4 历史时间线（git 证据）

| 日期 | 提交 | 事件 |
|---|---|---|
| 2026-08-11 | `32d84795` | phase2-v2：UPM 加性 C 场 + 图拉普拉斯；`upm.cpp:569` 的绝对 `1e-12` 守卫在此引入 |
| 2026-08-16 | `1752e8ca` | v19r3-s1：生产权重切到 `control_ivar`（SCI-UPM-WEIGHT-001） |
| 2026-08-29~09-01 | `80e6ad71`/`472e8874`/`fab26511`/`b31a3780`/`2211d0a9`/`fa677b5a` | 合成接缝门 + P2-003 生产接缝 Oracle（全部 PASS） |
| 2026-09-10 | `439f9f20` | P2-001：`upm-fit` 节点接线 `p2_upm_build_geo + use_ivar_weight=1`（生产开始吃真实 ivar） |
| 2026-09-18 | `324281c2` | FIX-A：新增 sky_plane 模块；stage2 写 `(raw−C−b_k)/g_k` |
| 2026-09-18 | `98e529ec` | sky_plane 接入生产 `module_adapters.cpp`（逐像素扣 `b_k`） |
| 2026-09-19 | `db91f264` | FIX-SKY：惩罚零空间锚+deflation，sky_plane 首次能建成（**但 UPM 仍死**） |
| 2026-09-19 | `affe6314` | SD-22..24：裁决 B 口径 `raw − δ_k`（保留 B_ref） |
| 2026-09-19 | `1a0baf51` | 9.15 归因文档：认为「生产缺 `g_k`」 |

---

## 2 ②接缝验收门判据与现状

### 2.1 P2-003 生产接缝 Oracle（`2211d0a9`）

- 文件：`tests/backend/test_p2003_seam_oracle.py` + fixture `tests/backend/phase2_fixture_main.cpp --make-seam`。
- 做什么：生成 3 块 mini HiPS（常量/线性梯度/低阶平滑背景 + 不同偏移 + 星 + mask），跑**正式生产链**（`astrocs mosaic`：sampler→UPM→persist），再用生产 apply 工具 `calibrated_pair_diag`（`p2_upm_open + p2_upm_calibrate_block`，`calibrated = raw − C`）解析**生产产物**。
- 判据（冻结容差，`test_p2003_seam_oracle.py:35-37`）：
  - `AFTER_RATIO_TOL = 0.35`（after/before RMS 上限）；
  - `STAR_NONFIT_FRAC = 0.10`（校正场空间变化 / 星幅度）；
  - 每对面板：`calibrated_median_not_worse` ∧ `calibrated_lowfreq_not_worse_5pct`；至少一对 `low_frequency_improved`（低频 p95_abs 严格下降）。
- 断言本质：**UPM C 场使跨帧重叠区低频差下降** = 「接缝下降」。

### 2.2 P2-008 单元门（`b31a3780`）与 P2-003 单元（`fab26511`）

- `tests/unit/p2_seam_gate_test.cpp`：**纯数学、不调用任何生产函数**。合成两块 32×32，偏移差 8 ADU；断言 `|before−8|<1.5`、`|after|<0.5`（seam 8→<0.5 ADU），外加资源门 `Ok/SingleThreaded` 判定。
- `tests/unit/p2_upm_synthetic_test.cpp`：同样纯数学，三块 pairwise median + gauge 闭合 + 接缝下降（`before>4`、`residual<2`）。
- **两者都没有调用 `p2_upm_*` / `p2_sky_plane_*`**，无法反映生产权重尺度。

### 2.3 P2-007 联合门（`fa677b5a`）

- `tests/backend/test_p2007_joint_gate.py`：6 块 seam workload ≥10s；`workers≥2 / CPU≥90%` 达门；科学+资源双证。属**资源门**，非接缝数值门。

### 2.4 SYN-008 合成接缝门（`80e6ad71`/`472e8874`）

- `tests/api/test_seam_metric_gate.py` + `tests/backend/syn008_seam_main.cpp`：3 重叠合成场 → `p2_upm_build` 联合求解每帧 C 场 → 同一 control cell 跨帧校准输出差 `|out_f0−out_f1|`。
- 冻结门槛：`p95 ≤ 3σ`、`max ≤ 5σ`，**σ = kNoiseRms = 0.05**（合成噪声）。实测 p95=0.0400、max=0.0591。
- 星点幅度 40、背景 O(1) ⇒ **权重尺度 O(1)**。

### 2.5 v6_p2_sky seam 用例（`324281c2`）

- `tests/unit/v6_p2_sky/v6_p2_sky_test.cpp`：合成注入面板台阶，`raw=1.49779 → calibrated=0.02355`（>20×）。**合成、尺度 O(1)**，不覆盖真实 1e13 背景。

### 2.6 复跑结果（`run/RELEASE-02/seam-arch/run_gates.log`）

| 门 | 命令 | 结果 |
|---|---|---|
| G1 | `./build/tests/unit/p2_seam_gate_test` | **PASS**（seam 8→0） |
| G2 | `./build/tests/unit/p2_upm_synthetic_test` | **PASS** |
| G3 | `./build/v6_p2_sky/v6_p2_sky_test` `[seam]` | **PASS**（1.49779→0.02355） |
| G4 | `python3 -m unittest tests.api.test_seam_metric_gate` | **PASS**（3/3） |

⇒ 按任务判据：**「能跑且通过 ⇒ 门没抓住真实接缝（门太弱）」**。四道门全部是合成、O(1) 尺度；唯一走生产链的 P2-003 Oracle 其 fixture 也写 `variance=0.01`（`phase2_fixture_main.cpp:20-21,72-73`，σ=0.1）⇒ `control_ivar=100`，`sums≈n×100 >> 1e-12`，**永远绕过生产尺度**。

### 2.7 门为什么必然抓不住

- 生产 `control_ivar` ≈ 5.6e-22（`uncertainty` ≈ 4.2e10 ADU），测试 ≈ O(1) —— **尺度差 ~1e22 倍**。
- 所有接缝门都在「权重正常」的合成档下验证「UPM 能压接缝」，**从未在真实权重尺度下验证 UPM 是否真的在工作**。
- 生产端也**没有**「模型是否退化」的门：`p2_upm_model.json` 不含 `objective/converged/M/C` 摘要，`module_adapters.cpp` 只校验 `rc==0`。

---

## 3 ③「当时」与「现在」的差异对比

### 3.1 校正公式的演化

| 时期 | 公式 | 证据 |
|---|---|---|
| RELEASE-01（设计口径） | `calibrated = raw − C_f(p)`（保留公共面 M） | `docs/science/PHASE2_UPM.md:7,47-50`；`upm.cpp:1299` |
| RELEASE-02 / FIX-A（`324281c2`） | `corrected = (raw − C_k − b_k)/g_k`，`b_k=B_ref+δ_k`（**剪掉整个背景面**） | `stage2.cpp:479-495`；`reports/RELEASE-02/FIX-A-report.md` §3.5 |
| RELEASE-02 生产（`98e529ec`） | 同上接入生产 | `module_adapters.cpp:4937-4951`（原 `p2_sky_plane_eval_block` + `out_v -= bvals`） |
| SD-22 裁决（`affe6314`）/ FIX-GK（工作树未提交） | `corrected = (raw − C_k) − δ_k`，`δ_k=b_k−B_ref`（保留 B_ref，不除 g_k） | `module_adapters.cpp:4939-4955`、`stage2.cpp:1070-1085/1443-1455`（新 API `p2_sky_plane_eval_delta`） |

### 3.2 **核心差异：UPM C 场从「归一化公共面」变成「恒等 0」**

实测（`run/RELEASE-02/seam-arch/check_upm_degeneracy.log`）：

```
[1] 生产 L4 UPM 模型状态 (p2_upm_model.bin)
  bitref_16w     iter=1 conv=1 objective=0.0 C_nz=0 M_nz=0/33472 smooth_lambda=0.0 unobs=4136 use_ivar=1
  bitref_1w      iter=1 conv=1 objective=0.0 C_nz=0 M_nz=0/33472 ...
  mosaic_out_w1  iter=1 conv=1 objective=0.0 C_nz=0 M_nz=0/33472 ...
  mosaic_out     iter=1 conv=1 objective=0.0 C_nz=0 M_nz=0/33472 ...

[2] 权重尺度 vs upm.cpp:569 绝对阈值 (sums[ck] > 1e-12)
  control_ivar_median=5.595e-22  sum(ivar)/control median=5.101e-21 min=8.88e-28 max=1.637e-20
  controls passing guard (1e-12): 0/29336   median_sum/guard=5.101e-09
```

判据链（全部 file:line）：
1. `upm.cpp:559` `p2_upm_raw_weight(use_ivar_weight=1)` 返回 `quality × control_ivar`（`upm.cpp:1348`），生产 ≈ 1e-22；
2. `upm.cpp:567-573` per-control 归一化：`if (sums[ck] > 1e-12) … else raw_w[i]=0.0;` ⇒ **全部 0**；
3. `upm.cpp:626-660` `w[i] = raw_w[i]×huber_w` ⇒ 0；
4. M 更新 `upm.cpp:710/719/744/761` `den > 1e-12` 恒假 ⇒ **M 保持 0**；
5. C 更新 `upm.cpp:846-868` rhs=0、`obs_w=0`；`cg_solve_frame`（`upm.cpp:577-613`）`pAp=0` 立即 `break`（`:600`）⇒ **C 保持 0**；
6. 收敛判据 `upm.cpp:885` `max_dM=0 ∧ max_dC=0 < tolerance=1e-6` ⇒ **converged=1, objective=0**；
7. `p2_upm_calibrate_block`（`upm.cpp:1299`）`output = input − evaluate_c_field(=0) = input` ⇒ **空操作**。

**这是「测试路径过、生产路径废」的典型尺度裂缝**：`lib/algorithms/coverage/tests/synthetic_gate.cpp:71-73` 明确「默认 control estimator 方差 1 → control_ivar=1（等价等权）」，测试权重 O(1) ⇒ 守卫不触发；生产权重 1e-22 ⇒ 守卫全灭。

历史锚：`1e-12` 守卫自 `32d84795`（phase2-v2）起就是**绝对**阈值；生产切到 `control_ivar` 在 `1752e8ca`（v19r3-s1）+ `439f9f20`（P2-001 节点接线）。RELEASE-01 VIS-001 的接缝 FAIL（`reports/RELEASE-01/vis/VIS-001-report.md:27,37`）即 C≡0 的直接后果——当时被误归因成「生产 UPM 只有帧级标量 b_k，无 b_k(x)」。

### 3.3 生产 vs 参考（stage2/tool）路径的其余缺口

系统性对比 `stage2.cpp` 与 `module_adapters.cpp` 的 `p2_*` 调用集（脚本见 `run/RELEASE-02/seam-arch/`），**只在 tool/测试有、生产没有**的天光/背景相关调用：

| 调用 | tool | 生产 | 影响 |
|---|---|---|---|
| `p2_upm_ma_build`（乘法 g_k） | `stage2.cpp:516` | **无** | 生产不做逐帧乘法响应（1a0baf51 的归因） |
| `smoothing_lambda`（全几何 Laplacian 延拓） | `stage2_common.cpp:140` `smoothing:"auto"→0.1`，`:483` 透传 | `module_adapters.cpp:4536-4537` 仅读 `doc["upm"]["smoothing_lambda"]`；L4 配置无 `upm` 键 ⇒ **λs=0** | 单帧区/无观测节点不做延拓（SEAM-002/v13 机制在生产关闭） |
| `p2_large_scale_apply` | `stage2.cpp:1744` | **无** | 拖线掩膜生长（对 TRAIL，非接缝） |
| `p2_sky_estimate_gain_dc` | `stage2.cpp:545` | **无** | g_k 直流比门只在 tool |
| `p2_upm_materialize_dense_n` | `stage2.cpp:605` | **无** | 诊断缓存 |

实测生产模型 `smoothing_lambda=0.0`、`unobserved_geometry_nodes=4136`。

### 3.4 离群剔除（负责人 SD-23）

- 设计 `11_upm.md` §4.4（`:60-65`）要求「**逆方差（SNR）加权最小二乘**，并以稳健迭代抑制离群点（M 估计/σ-clipping）」。
- sky_plane 实现：`sky_plane.cpp:933-948` 只有 **Huber 权重**（`huber_w(z, 1.345)`）；`sky_plane.cpp:1030` 的 `n_rejected` 只是 `|z|>5` 的**计数**，不参与剔除。
- σ-clipping（`clip_iters=3, clip_sigma=3.0`）位于 **patch 估计器** `P2SkyPatchConfig`（`sky_plane.cpp:256-259`，用于天光采样点），**不在天光面联合拟合内**。
- ⇒ **天光面拟合已退化为「仅 Huber 稳健权重、无离群剔除」**，与 SD-23 要求不符。

### 3.5 B_ref / δ_k 求值口径

- `p2_sky_plane_eval`/`eval_block` 恒返回 `b_k=B_ref+δ_k+gauge_shift`（`sky_plane.h:221-224`）。
- 自 `324281c2` 创建以来语义**未变**：`98e529ec` 只加探针（diff 仅 `ASTROCS_PROBE_*`）；`e55b89ca` 只加惩罚零空间锚+deflation（数值，不改科学解）。
- `gauge_mode=0`（参考帧 δ≡0）；`gauge_mode=1`（sum-zero）的平移记入 `gauge_shift`。
- **缺口**：在 SD-22 之前没有 δ-only 求值（只有系数访问器），所以「保留 B_ref、只扣 δ_k」在旧接口下**不可直接实现**——这是 FIX-GK 新增 `p2_sky_plane_eval_delta` 的原因。

### 3.6 sky_plane 拟合质量（旁证，非本次重点）

- `sky_plane.cpp:802-803` 的 `roughness_penalty=1e-3` 是绝对惩罚；生产权重 ~1e-21 ⇒ `trace(P)/trace(H_data)=1.26e17`，B_ref 被钉成 bilinear（`reports/RELEASE-02/sky-fit-diagnosis.md` §2.4）。
- `chi2_red=6268`、`rms_unweighted=7.68e13` ≈ 7.7× 天光电平 ⇒ 模型无法表达真实星云结构，结构化背景全压给逐帧 δ_k。
- 与接缝的关系：δ_k 的帧子集均值在 n 变化处跳变 ⇒ 接缝（`reports/RELEASE-02/seam-attribution.md` §2.3）。

---

## 4 ④结论与恢复方案

### 4.1 以前是怎么解决接缝的（机制 + file:line + 提交号）

**机制**：用 UPM 加性场把每一帧的局部背景/零点归一化到一个**公共面**，只扣帧间相对偏差，保留公共面。

- 设计：`docs/science/PHASE2_UPM.md:7`（`calibrated = raw − C_f(p)`，消除逐帧背景/零点差）；`docs/plugins/algorithms_phase2/11_upm.md` §4.3（`b_k=B_ref+δ_k`，每帧只拟合 δ_k，归一化到统一大平面）。
- 实现：`lib/algorithms/coverage/src/upm.cpp`，模型 `y=M+C`，gauge `C_ref≡0`（`:801-805`），应用 `raw−C`（`:1299`）。
- 提交：`32d84795`（phase2-v2 引入）；接缝验证门 `fab26511`/`b31a3780`/`80e6ad71`/`2211d0a9`。
- **但该机制在生产里已死**（§3.2），所以「以前解决过」的实际上是**合成/小尺度档**（权重 O(1)，C 正常）；真实生产从未真正归一化过（RELEASE-01 VIS-001 就 FAIL）。

### 4.2 为什么现在不行了（三条独立原因）

1. **【最深层】UPM C 场恒等 0** —— `upm.cpp:569` 绝对阈值 `1e-12` vs 生产 `Σcontrol_ivar≈5e-21`。⇒ 「归一化到公共面」机制在生产**完全没运行**（不是被改掉，是**从未生效**，被尺度裂缝隐藏）。
2. **【FIX-A 引入的错误公式】** `324281c2`/`98e529ec` 用 `raw−C−b_k`（`b_k=B_ref+δ_k`）⇒ 把整个公共背景面也剪掉（背景归零/46% 负值），且 δ_k 的帧子集跳变**主动注入**更大台阶（seam-attribution：+4.22e11 vs +0.93e11）。SD-22 已裁决否决。
3. **【工具/生产接线缺口】** 全几何 Laplacian 延拓（`smoothing_lambda`）在 stage2 默认 0.1、生产默认 0（`module_adapters.cpp:4536-4537` + L4 配置无 `upm` 键）；乘法 g_k 生产未接线。二者都削弱了公共面归一化的能力。

### 4.3 恢复方案（最小改动面，按优先级）

> 均属科学/生产变更，须走 `ENGINEERING_SPEC §3` 变更 claim + 一致性回归；**本次未改任何代码**。

1. **【必做，治本】修 `upm.cpp:569` 的绝对阈值**：改为尺度无关判据（如 `sums[ck] > 0` 且有限，或相对 `sums[ck] > eps·max_sums`）。同步检查 `upm.cpp:694/710/719/744/761/770` 的 `1e-12` 守卫与 `upm.cpp:1377` `p2_upm_normalized_weights` 的同款守卫。
   - 预期：M、C 恢复非零；`raw − C` 重新归一化帧间背景；`objective>0`、`iterations>1`。
   - **必须补门**：`p2_upm_model.json` 增 `converged/objective/C_max_abs`，并加一条「真实 ivar 尺度（~1e-22）下 C 非零」的 Oracle——现有合成门（ivar=1）无法覆盖。
2. **【SD-22 已裁决，FIX-GK 工作树已改】** 生产/参考统一 B 口径 `corrected = raw − δ_k`（保留 B_ref，不除 g_k）；配合新增 `p2_sky_plane_eval_delta`。注意：若第 1 项修好、C 复活，需明确 `raw − C − δ_k` 中 C 与 δ_k 的分工（C 管 8×8 cell 相对场、δ_k 管逐帧平缓梯度），避免重复扣除。
3. **【SD-23】天光面拟合加离群剔除**：在 `sky_plane.cpp` 的 IRLS 内加标准化残差 σ-clip（复用 patch 估计器的 `clip_iters/clip_sigma` 口径），或至少在拟合前对采样点做 MAD 剔除；判据用加权残差 RMS。
4. **【接线】生产启用全几何延拓**：`module_adapters.cpp` 读 `upm.smoothing_lambda`（与 stage2 的 `smoothing:"auto"→0.1` 对齐），恢复单帧区/无观测节点的 Laplacian 延拓（SEAM-002/v13 机制）。
5. **【门】把真实尺度纳入接缝门**：P2-003 Oracle 的 fixture 应至少有一个「生产 ivar 尺度」档；或直接对生产 `p2_upm_model.bin` 加退化检查（`C≡0 ⇒ FAIL`）。
6. **【旁证】sky_plane 惩罚按数据尺度归一**（`roughness_penalty`）——sky-fit-diagnosis 判为正确性修复，可降低 B_ref 被钉成 bilinear 导致的 δ_k 过载。

---

## 5 证据/脚本索引（`run/RELEASE-02/seam-arch/`）

| 文件 | 内容 |
|---|---|
| `check_upm_degeneracy.py` / `.log` | 生产 UPM 退化复现（M=0/C=0/objective=0 + 权重尺度 vs 1e-12） |
| `run_gates.sh` / `run_gates.log` | 4 道接缝门复跑（G1–G4 全 PASS） |
| `git_archaeology.sh` / `.log` | 关键提交、守卫/权重/公式引入点、file:line 锚 |
| `callset_diff.py` / `.log` | 生产调度路径 vs stage2 工具路径的 `p2_*` 调用集差异 |
| 只读输入 | `run/RELEASE-02/L4-rebuild/{bitref_16w,bitref_1w,mosaic_out_w1,mosaic_out}/p2_upm_model.bin`、`p2_samples.json`、`p2_sky_plane.bin` |

## 6 诚实边界

- **未改任何生产代码/文档；未跑 ninja/cmake/ctest；零 git 写操作。**（工作树中已有的 FIX-GK 改动是他人未提交的，本次只读未动。）
- RELEASE-01 的 `p2_upm_model.bin` 已不在磁盘（仅剩 `.json` 元数据），故「RELEASE-01 的 C 是否也为 0」是**推断**（依据：同样 `use_ivar_weight=1` + 同样守卫 + VIS-001 接缝 FAIL），非直接实测。
- 「以前解决过」的合成证据（SYN-008 / P2-003）确实 PASS，但其权重尺度 O(1)，**不能**证明真实生产曾归一化成功。
- 台阶绝对幅度沿用 seam-attribution/sky-fit 的方法学不确定度（±20–30%）；本报告只新增了 UPM 退化的**确定性**证据（0/29336）。
