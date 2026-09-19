# RELEASE-02 PHOT-VERIFY — Phase1 星等归一化核实 + 平场/渐晕空间乘性残留测量

- 任务：独立复核「Phase1 星等归一化是否正确」，并量化 L4 数据的空间乘法/加性残留。
- 核实人：PHOT-VERIFY（SubAgent）。**零 git 写权限；未改任何生产代码/文档；未跑 ninja/cmake/ctest。**
- 日期：2026-09-19。环境：`TMPDIR=/dev/shm/astrocs_phot`（用完已清空）。
- 证据/脚本：`run/RELEASE-02/phot-verify/`（见 §10）。
- 数据：`run/RELEASE-02/L4-rebuild/norm/*/`（12 板块 / 49 帧，RELEASE-02 L4 重跑产物）。

---

## 0 TL;DR（结论先行）

| 问题 | 结论 |
|---|---|
| **Phase1 星等归一化是否正确？** | **算法定义正确，但生产链路根本没有执行它** —— RELEASE-02 L4 的 `photometry_applied=false`、`photscal=1.0`、`BUNIT=ADU`，帧是**未做任何测光归一的原始 ADU**。负责人的「所有帧已归一化到测光坐标系」这一前提**在当前实现下不成立**。 |
| `k_photo` 是标量吗？ | **是单一全局标量**：`k_photo = scale = 10^(−location)`，`location` = 全体匹配星 `r_i=log10(F_instr,i/F_syn,i)` 的 Tukey-IRLS 稳健位置。**无空间项、无颜色项**（`photometry_apply.cpp:63` 单一循环；`star_matcher.cpp:589`）。 |
| L4 实测帧间差异多大？ | 固定孔径(r=6px)星点比值：同指向不同夜 **0.105 mag**（t2_m1 f0/f1）；跨望远镜同板块 **0.03–0.12 mag**；相邻板块 **0.06–0.14 mag**。用生产实际口径（检测等照度 flux）测得更大（跨望远镜可达 ~0.5 mag），说明该 flux 估计量本身有强视宁度/阈值偏差。 |
| 空间乘性残留多大？ | 扣掉帧级标量后，干净天区 overlap 内 `log10(y_A/y_B)` 的 8×8 分箱峰峰值 **≈0.05–0.08 mag**（xtel_m1 0.067、xtel_m4 0.078、xtel_m5 0.052）。单帧天光径向探测（扣平面后）残差 **±2–3%**（≈0.02–0.03 mag），峰峰值 ≈4–6%。 |
| 渐晕型还是梯度型？ | overlap 内二者**退化不可分**（`rA²−rB²` 与天球位置强相关）；同指向小 dither 对**无显著空间结构**（|corr|<0.03）⇒ 观测到的跨板块图案更符合**探测器固定的平场/渐晕残差**在两天区采样点不同所致，而非天空梯度（天光梯度在比值中相消）。 |
| 加性 vs 乘性谁主导接缝？ | **乘性帧级项主导**：M42 星云信号达数千 ADU/px，1–10% 乘性差 = 大绝对台阶；实测加性差在干净天区**空间上近似常数**（同天球位置 `b_A−b_B` 中位 20–35 ADU/px，MAD 仅 1.4–3.2）。 |
| 是否需 mosaic 阶段补充测光校准？ | **需要**。①Phase1 归一化缺席，帧级乘性差完全未修；②即使补上帧级标量，空间乘性残留（≈0.05–0.08 mag PTP）仍在。方案见 §8。 |
| 现有 `g_k`/UPM 够用吗？ | 冻结生产模型（`PHASE2_UPM.md §5`）是**纯加性** `calibrated=raw−C_f(p)`，**没有 g_k**；设计（`11_upm.md:26`）的 `g_k` 是**帧级标量**。帧级 g_k 必要但**不充分**（去不掉空间残留）；且冻结文档 §6 假设「帧间无乘性尺度差」已被本次实测**证伪**。 |

---

## 1 权威依据

- `docs/science/PHOTOMETRY.md`（SCI-PHOT-001, FROZEN）：§5 公式、§4 有效域、§8 退化。
- `lib/algorithms/photometry/cpp/include/photometric_calib.h`：C ABI 6 导出符号。
- `lib/algorithms/calibration/src/photometry_apply.h/.cpp`：`I_photo = k_photo·I_cal`。
- `docs/science/PHASE2_UPM.md`（SCI-UPM-001, FROZEN）：现行纯加性模型 + §6 假设。
- `docs/plugins/algorithms_phase2/11_upm.md`：设计 `y_k(x)=g_k·s(x)+b_k(x)`。
- `工程控制/RELEASE-02/GAP_AUDIT.md §9.21–9.24`：负责人原话与前台初步核实。

---

## 2 ① 链路与 `k_photo` 定义

### 2.1 完整链路（星提取 → Gaia 匹配 → 光谱积分 → 拟合 → 应用）

1. **星点提取（F_instr）**：`lib/algorithms/star_detection/src/sdet_detector.cpp:281-285`
   对连通域内 `image−bkg>0` 的像素求和 ⇒ `star->flux`；由
   `lib/infrastructure/scheduler/src/module_adapters.cpp:2102-2106` 原样写入
   `p1_sources.json` 的 `sources[].flux`。
   **注意：这是"检测等照度通量"（阈值/连通域相关），不是 PSF 总通量、也不是固定孔径通量。**
   （`module_adapters.cpp:1997` 明文："psf_params 在仓库内零消费者、psf 端口为死边、测光正式口径=孔径"。）

2. **WCS 投影 + 双向 KD-tree 匹配**：`star_matcher.cpp:185-369`。
   Gaia 星投到像素坐标 → 对 Gaia 建 KD-tree（正向 PSF→Gaia）+ 对 PSF 有效星建 KD-tree（反向 Gaia→PSF），
   只保留互为最近邻的唯一对；匹配半径 `2.0 px`（`pc_api.cpp:131,390`）。

3. **光谱积分（F_syn）**：`pc_api.cpp:340-356` 调 `compute_f_syn_cached_xpsd`；
   被积函数与积分见 `spectrum_integrator.cpp:266-274`：
   ```
   F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ     # Akima 插值 + 复合 Simpson 1/3
   flux[j] = byte[j]*flux_mul + flux_min   # XPSD 官方解码 (PCL)
   ```
   （`pc_api.cpp:349-351`；滤光片/QE 缓存见 `spectrum_integrator.cpp:285+`。）

4. **拟合 `k_photo`**：`star_matcher.cpp:445-590`
   ```
   r_i     = log10(F_instr,i / F_syn,i)                         # dex   (:455)
   delta_i = −2.5·log10(F_instr,i) − G_Gaia,i                   # 星等预过滤 (:457-459)
   预拒绝  若 |delta_i − median(delta)| > 3.0 mag                # (:486-501)
   |r_consistent| < 3 ⇒ NO_DATA, scale=1.0                      # (:518-536)
   location = median(r); S = MAD(r)/0.6744897501960817          # (:540-545)
   Tukey IRLS: u=(r−location)/(4.685·S), w=(1−u²)², 迭代≤50, tol=1e-6   # (:552-586)
   k_photo = scale = 10^(−location)                             # (:589)
   sigma_residual = MAD(r_inliers)/0.6744897501960817           # (:616-627)
   ```
   实际传入点：`pc_api.cpp:137-142`（float 路径）/ `:396-401`（with_gaia 路径），
   `mag_tolerance=3.0`。

5. **应用**：`photometry_apply.cpp:59-65` —— 单一 `for` 循环 `out[i]=(float)((double)light[i]*photscal)`。

### 2.2 定义式与"是否单一标量"

```
k_photo ≡ scale = 10^(−location),
location = argmin_θ Σ_i w_i·(r_i − θ)²   (Tukey biweight IRLS, r_i = log10(F_instr,i/F_syn,i))
```

- **是单一标量**：`apply_photometry(const float*, int, int, double photscal, float*)` 只有一个 `double`（`photometry_apply.h:37`），对全帧 16M 像素乘同一数（`photometry_apply.cpp:63`）。
- **不含空间项、不含颜色项**：拟合目标是 `r` 的**单一稳健位置**，无 (x,y) 基函数、无 (BP−RP) 基函数（`star_matcher.cpp:538-590`）。SCI-PHOT §1 亦明文"非目标：不处理带通外颜色项高阶效应"。
- **无 χ²**：拟合质量只用 `sigma_residual = MAD(r_inliers)/0.6745`（`:616-627`），**不计算 χ²/χ²_red**。
- **离群剔除**：两级 —— ① 星等一致性 3.0 mag 预过滤；② Tukey IRLS（`c=4.685`，权重为 0 的判为 outlier）。

### 2.3 代码级缺陷（仅当该拟合被接线时才生效）

- `pc_api.cpp:137-142` 与 `:396-401` 调用 `cleanAndScale(raw, &rows, nullptr, 3.0, …)`，
  第 3 参数 `quality_flags=nullptr` ⇒ `star_matcher.cpp:428-438` 的饱和/质量位过滤**不生效**
  （`saturated = (quality_flags != nullptr && …)` 恒 false）。这与 SCI-PHOT §4
  「饱和星不进入匹配与定标」**不一致**，属实现缺陷（DISP/负例应锁定）。
  另外 `matchWithKdTree` 只用 `psf_status==0` 过滤（`:235-239`），而 `psf_status` 是 PSF 拟合状态，
  不是饱和标志。

---

## 3 ★ 关键发现：生产链路根本没有执行 Phase1 测光归一化

### 3.1 生产 photometry 节点只"测通量"，不"应用比例"

`lib/infrastructure/scheduler/src/module_adapters.cpp:2964-3098` `p1_op_photometry`（operation=`measure_flux`）：

- 逐帧逐源调 `astrocs::phase1::Photometer::measure`（固定 r=4px 孔径 + 6–10px 天光环，`wrapper_phase1/photometer.cpp`）；
- 写 `p1_flux.json`；
- **写死** `photometry_applied=false, photscal=1.0, pixel_scaling="none"` 的
  `p1_phot.json`（`:3076-3091`），节点自述："本节点只测量孔径通量，不对像素施加测光缩放"（`:3077-3080`）。

drizzle 节点读该 provenance（`:3536-3563`），据此写 `PHOTAPPL=0 / PHOTSCAL=1.0 / PHOTDEGRADE=1 / BUNIT=ADU`
（`:3697-3720, 3818-3820`）。

### 3.2 L4 产物事实（12/12 板块）

`run/RELEASE-02/phot-verify/evidence_wiring.txt`：

```
t2_m1_red: False 1.0 none        ... (全部 12 个板块)
```

- `norm/*/p1_phot.json`：`photometry_applied=false, photscal=1.0`；
- `norm/*/*/p1_stack.json`：`photappl=0, bunit="ADU", photscal=1.0`；
- `norm/*/calibrated_*.fts`：**帧头无任何 PHOT* 关键字**（实测 `[k for k in h if 'PHOT' in k]` 为空）。

### 3.3 会做归一化的代码为何没跑

- `pc_calibrate_simple*` 的**唯一生产调用者**是 `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:2572 run_stage_photometric`（经 DLL loader 加载 `photometric_calib.dll`）。
  但：**`build/` 下无 `photometric_calib.dll`**（`module.yaml` 自述"未编入根 CMake 主构建"）；
  `astrocs` CLI 可执行文件链接的是 `p1_session.cpp + module_adapters.cpp`，**不含 orchestrator**（`cli/CMakeLists.txt:216`）；
  L4 的 `graph/l0_graph.json` 节点是 `astrocs.phase1.photometry / measure_flux`。
  ⇒ 归一化路径在 `astrocs normalize` 上**不可达**。
- `apply_photometry`（`photometry_apply.cpp`）在**活跃代码树内 0 调用者**（`evidence_wiring.txt [2]`）。
- `module.yaml`：`entrypoint: MISSING`、`node_operations: []`、`dll_target` 尚未存在。

### 3.4 后果

L4 的 49 帧**不在同一测光坐标系**：帧间保留完整的乘性零点差（透明度/消光/口径差）。
这直接证伪了 `PHASE2_UPM.md §6`「帧间无乘性尺度差」的假设，也证伪了负责人
「phase1 已把所有帧归一化到测光坐标系」的前提。

---

## 4 ② L4 实测：帧间乘性差与"拟合质量"（独立复算）

> 因为生产没有 k_photo，无法直接读"每帧 k_photo"（它们按构造恒为 1.0）。
> 因此本核实**独立**用 L4 原始像素 + p1_sources 星表复算帧间乘性比，
> 以回答"若做归一化，会得到什么、残差多大"。

### 4.1 方法

- 星位置来自各帧 `p1_sources.json`，用 `p1_wcs.json` 的 TAN+SIP（`wcs_lib.py` 逐行移植
  `wcs_transform.cpp`，前向与 astropy 独立实现比对 ≤0.16″）投到天球，KD-tree 互匹配（≤0.7″，质量位=0）。
- 在 `calibrated_*.fts` 上做**固定孔径** r=6px、天光环 10–16px 的测光（`pixel_measure.py`），
  3 轮 4σ 剔除。固定孔径避免检测等照度 flux 的阈值/视宁度偏差。

### 4.2 帧级乘性比（干净天区，单位 mag；正= A 比 B 暗）

| 对 | 类型 | n | 帧级比 (mag) | 逐星散度 (mag) |
|---|---|---|---|---|
| t2_m1 f0/f1 | 同指向·异夜 | 1296 | **+0.105** | 0.016 |
| t2_m2 f0/f1 | 同指向·异夜（星云核） | 2627 | +0.118 | 0.150 |
| t2_m2/t2_m3 | 相邻板块 | 104 | +0.089 | 0.026 |
| t2_m5/t2_m6 | 相邻板块 | 73 | −0.060 | 0.025 |
| t3_m2/t3_m5 | 相邻板块（星云） | 2286 | −0.003 | 0.298 |
| t2_m1/t3_m1 | 跨望远镜 | 1114 | −0.029 | 0.023 |
| t2_m4/t3_m4 | 跨望远镜 | 409 | −0.125 | 0.034 |
| t2_m5/t3_m5 | 跨望远镜（星云） | 2689 | −0.013 | 0.227 |

**孔径敏感性**（`radius_test.py`）：
- t2_m1 f0/f1：r=4→10px 比值 0.905→0.908（**稳定**）⇒ 0.105 mag 是真实响应/透明度差，非视宁度；
- t2_m1/t3_m1：r=3→10px 比值 1.102→1.021（**变 0.08 mag**）⇒ 跨望远镜因 PSF 形状不同，
  帧级比有 ~0.05–0.08 mag 的孔径/PSF 系统不确定度。

### 4.3 用生产实际口径（检测等照度 flux）的对照

若把 `p1_sources[].flux`（即拟合链路里的 F_instr）直接做帧间比，帧级差被**放大**：
跨望远镜可达 **0.1–0.5 mag**，逐星散度 0.01–0.13 dex（`pair_results.json`）。
⇒ **该 flux 估计量有强视宁度/阈值偏差**，若将来接线 k_photo，其 `F_instr` 需换成
固定孔径/PSF 总通量，否则 `sigma_residual` 会被这一项主导。

### 4.4 "拟合质量"结论

- 匹配星数：L4 每帧 PSF 有效星 1633–5000（`p1_sources.json n_psf_valid`）；固定孔径可匹配星数千级。
- 拟合残差：干净天区逐星 `log10(ratio)` MAD ≈ **0.006–0.014 dex（0.016–0.034 mag）**
  ⇒ 若接线，`sigma_residual` 量级 ~0.01 dex（0.03 mag）是可达的；星云场 ~0.06–0.36 dex，被背景系统误差主导。
- χ²：**不存在**（拟合只出 MAD-based sigma，`star_matcher.cpp:616-627`）。
- 离群剔除：有（3.0 mag 预过滤 + Tukey IRLS）；但**饱和/质量位过滤未生效**（§2.3）。
- 是否拉到同一测光坐标系：**否**（生产未执行）。若执行，帧级标量能把干净天区拉到 ~0.02 mag 以内，
  但空间残留（§5）仍在。

---

## 5 ③ 空间乘性残留量化（重点）

### 5.1 overlap 内 `y_A/y_B` 的空间结构（扣帧级标量后）

固定孔径 r=6px，8×8 分箱中位数，干净天区：

| 对 | n | 帧级比 (mag) | plane 拟合 PTP (mag) | 8×8 分箱 PTP (mag) | 分箱 MAD (mag) |
|---|---|---|---|---|---|
| within t2_m1 f0/f1 | 1296 | +0.105 | 0.003 | 0.029 | 0.004 |
| adj t2_m2/t2_m3 | 104 | +0.089 | 0.039 | (分箱不足) | — |
| adj t2_m5/t2_m6 | 73 | −0.060 | 0.070 | (分箱不足) | — |
| xtel t2_m1/t3_m1 | 1114 | −0.029 | 0.053 | **0.067** | 0.013 |
| xtel t2_m4/t3_m4 | 409 | −0.125 | 0.075 | **0.078** | 0.022 |
| xtel t2_m5/t3_m5 | 2689 | −0.013 | 0.056 | 0.052 | 0.020 |

⇒ **扣掉帧级标量后，overlap 内空间乘性残留峰峰值 ≈0.05–0.08 mag（0.02–0.03 dex）。**
（星云主导的板块：t2_m4/t2_m5 0.53 mag、t3_m2/t3_m5 0.24 mag —— 这些数被真实星云背景
系统误差污染，**不作为可靠的空间乘性估计**。）

### 5.2 单帧天光径向探测（独立、更强证据）

`sky_vignette.py`：对干净帧 `t2_m1`、`t2_m4`、`t2_m6`、`t3_m1`，用检测星表掩星（r=8px）后
取天光中位数，先扣平面（真实天光大尺度梯度），再看随探测器半径的残差：

- `t2_m1 f0`（sky≈194 ADU/px）：r=300–700px 残差 **−2.4…−1.8%**，r=1700–2100px **−0.1…+1.1%**；
- `t2_m1 f1`：同形（−2.6% → +1.4%）；
- `t2_m4 f0`：+1.4% → −3.4%；
- `t2_m6 f0`：−0.8% → −1.4%。

⇒ 单帧平场/渐晕残差 **±2–3%（≈0.02–0.03 mag）**，峰峰值 **≈4–6%**。与 §5.1 的 overlap 结果同量级。

### 5.3 渐晕型 vs 梯度型

- 设计矩阵对比（`vignette_test.py`）：模型 G（天球平面 `z=c0+c1·x_sky+c2·y_sky`）与
  模型 V（`z=c0+k·(rA²−rB²)`）对 overlap 残差的降低量**几乎相同**；
  两者相关系数都可达 |0.3–0.5|，且 `rA²−rB²` 与天球位置**强退化** ⇒ **无法从 overlap 唯一区分**。
- 但：**同指向小 dither 对**（`within t2_m1`）无显著空间相关（corr x=−0.02, y=−0.03, radial=+0.01），
  即探测器位置几乎不变时空间图案≈0；而跨板块（探测器采样点差异大）图案出现。
- ⇒ 观测到的跨板块空间乘性图案**更符合探测器固定的平场/渐晕残差**（不同板块把不同探测器区域
  投到同一天区），**而不是天光梯度**（天光是两天区共有的真实信号，在 `y_A/y_B` 中相消）。

---

## 6 ④ 加性 vs 乘性：谁主导接缝

- **加性**：同一批天球位置上，两帧局部天光差 `b_A−b_B`：
  - 干净天区：中位 **20–35 ADU/px**（帧级天光亮度差），空间 MAD 仅 **1.4–3.2 ADU/px**
    （占天光 ~1–2%）⇒ **空间上近似常数**（是一帧级零点差，不是梯度）。
  - 星云场：中位可达 ±250 ADU/px、MAD ~80 —— 那是真实星云结构，不是天光梯度。
  - 逐箱联合 `b_A=a·b_B+c`：`a`≈帧级比，`c` 的分箱散布在干净天区 ~10–30 ADU/px。
- **乘性**：帧级 **0.01–0.14 mag（干净天区）**，空间残留 **0.05–0.08 mag PTP**。
- **接缝主导项**：M42 星云信号达 `10³–10⁴ ADU/px`，1–10% 的**乘性**差 → `10¹–10³ ADU/px` 绝对台阶；
  而实测加性差在干净天区仅 `1–3 ADU/px` 空间变化。⇒ **接缝的乘性项在星云区主导**；
  加性项在纯天光区主导（与 SEAM 报告一致：SEAM 是在**已做加性天光面**之后仍见 1–3% 台阶）。
- 两者**不重复**：乘性来自星点（背景扣除后），加性来自天光/背景；PMM 正是这样分离
  （`run/RELEASE-02/pmosaic/PhotometricMosaic/lib/SampleGrid.js:39` 乘性 scale 作用在
  target 中位数；`Gradient.js:206` 对 `tgt·scale−ref` 做加性曲面样条）。

---

## 7 ⑤ 结论：Phase1 星等归一化是否正确？

**分两层回答：**

1. **算法层面：定义与实现正确**（在 SCI-PHOT-001 的"模型通带相对刻度"语义内）。
   `k_photo=10^(−location)`、IRLS/Tukey、星等预过滤、星数门、退化语义都与冻结文档一致；
   合成 Oracle（§11）与不变量（§7）设计成立。
2. **工程/生产层面：不正确 —— 它没有运行。**
   RELEASE-02 L4 的 49 帧 `photometry_applied=false / photscal=1.0 / BUNIT=ADU`，
   归一化路径（`pc_calibrate_simple*` / `apply_photometry`）在 `astrocs normalize` 上**不可达**，
   `photometric_calib.dll` 未构建，`module.yaml entrypoint=MISSING`。
   ⇒ **当前产物没有公共测光坐标系**，帧间乘性差 0.01–0.14 mag 原样保留。

**缺陷锚点（file:line）与最小修复：**

| # | 缺陷 | 锚点 | 最小修复 |
|---|---|---|---|
| D1 | photometry 节点不计算也不应用 k_photo | `module_adapters.cpp:3076-3091` | 在 `p1_op_photometry` 内增加"星测光拟合 + 乘到像素"步骤（或新增独立节点），产出真实 `photscal` 并置 `photometry_applied=true` |
| D2 | 归一化实现未编入主构建 | `lib/algorithms/photometry/module.yaml`（entrypoint=MISSING；无 CMake 目标） | 把 `cpp/src/*.cpp` 编入根 CMake 并接 registry 入口 |
| D3 | `apply_photometry` 无调用者 | `photometry_apply.cpp`（活跃树 0 caller） | 由 D1 的节点调用 |
| D4 | 饱和/质量位过滤未生效 | `pc_api.cpp:138,397`（传 `nullptr`） | 传入 quality_flags（或改 `cleanAndScale` 签名强制） |
| D5 | F_instr 用检测等照度 flux（阈值/视宁度偏差） | `sdet_detector.cpp:281-285` → `p1_sources.flux` | 改用固定孔径/PSF 总通量做测光拟合输入 |
| D6 | 拟合无 χ² | `star_matcher.cpp:616-627` | 可选：补 χ²_red/outlier_rate 进 diag |

> D1 是"归一化缺席"的根因；D2–D3 是使其可达的必要条件；D4–D5 影响一旦接线的拟合质量。

---

## 8 ⑥ 是否需 mosaic 阶段补充测光校准？——需要，方案如下

### 8.1 为什么需要

1. **帧级乘性差完全未修**（Phase1 缺席）：0.01–0.14 mag（干净天区），星云区造成大绝对台阶。
2. **即使接上帧级标量，空间乘性残留仍在**：≈0.05–0.08 mag PTP（overlap），
   单帧平场/渐晕残差 ±2–3%。帧级标量按定义无法修。

### 8.2 建议方案（PMM 式乘/加严格分离，N 帧适配）

```
① 乘性（来自星测光）：对每帧 k 求乘性因子
     - 优先：Phase1 修复后直接消费每帧 k_photo（帧级标量）；
     - 补充：若空间残留显著，用 overlap 内"星点背景扣除后通量比"对每帧拟合低阶空间乘性面
       m_k(x,y)（探测器坐标；阶数由残差判据定，建议 ≤2 阶），乘到像素上。
   对应 PMM: SampleGrid.js:39 applyScaleToSamplePairs(targetMedian*scale)
② 加性（来自天光/背景）：对每帧拟合天光面 b_k(x)（现有 UPM 8×8 control-cell 双线性 C_f，
   或天空面样条），叠加时相减 corrected = pixel − b_k(x)。
   对应 PMM: Gradient.js:206 calcSurfaceSpline(tgt*scale − ref)
③ 分工：乘性只用"星"（背景已扣），加性只用"天光/背景"（星已掩膜）⇒ 信号不相交、不会重复扣。
④ 顺序：先乘后加（PMM 顺序：scale 先作用，再对差值拟合加性梯度）。
```

- **与加性梯度是否重复？不重复**：乘性项对"点源通量/星云亮度"成比例，加性项对"天光基底"平移；
  两者在 overlap 内的可辨识性来自"星 vs 天光"两类样本（PMM 的 sample grid + 星拒绝正是为此）。
- **N 帧（L4 每板块 8–9 帧）**：PMM 是 2 帧（ref+tgt），AstroCS 需把 ref 换成"其他帧的 SNR 加权组合"
  （`GAP_AUDIT.md §9.22` 已登记为待研究项）。
- **与现有 `g_k`/UPM 冲突**：现行冻结模型没有 g_k（§9）。要落地本方案，必须走 SCI 变更流程
  解决 `PHASE2_UPM.md §9.21/§9.22 登记的 UNRESOLVED`，把 `g_k`（必要时加空间乘性面）纳入模型。

### 8.3 优先级

1. **P0**：修复 D1–D3（让 Phase1 帧级归一化真正运行）。这一项就能消掉最大的帧间乘性差。
2. **P1**：解决 UPM 模型的 UNRESOLVED，把帧级 `g_k` 纳入（若 P0 已完成，等价于"双重保险"并处理
   Phase1 未覆盖的帧）。
3. **P2**：评估是否需要**空间**乘性面 m_k(x,y)。判据：接缝验收阈值 vs 实测 0.05–0.08 mag PTP。
   M42 星云区 5–8% 台阶肉眼可见，倾向需要低阶空间乘性面。

---

## 9 ⑦ 与现有 `g_k` / UPM 的关系

- **冻结生产模型**（`docs/science/PHASE2_UPM.md §5`，SCI-UPM-001 FROZEN）：
  `calibrated_f(p) = raw_f(p) − C_f(p)`，`C_f` = 8×8 control-cell 双线性 —— **纯加性、无乘性项**；
  §6 明文假设「帧间无乘性尺度差（乘性 photometric scale 已撤销）」。
- **设计模型**（`docs/plugins/algorithms_phase2/11_upm.md:26`）：
  `y_k(x) = g_k·s(x) + b_k(x) + ε_k(x)`，其中 **`g_k` 是帧级标量乘性响应**（`:5,:18,:26`），
  `b_k(x)` 是加性天光面。**不是空间乘性场**。
- **实现**：活跃代码树**没有 g_k 实现**（`grep g_k lib/...` 仅命中注释 `module_adapters.cpp:4942`
  "不除 g_k"）。
- **本次实测判定**：
  - `PHASE2_UPM.md §6` 的假设**被证伪**（帧间确实有 0.01–0.14 mag 乘性差）；
  - 设计里的**帧级 `g_k` 是必要但不充分**：它消掉帧级差，**消不掉 0.05–0.08 mag 的空间乘性残留**；
  - `PHASE2_UPM.md:182` 登记的 UNRESOLVED（纯加性 vs `g_k·s+b_k`）**必须裁决**，
    证据（本报告 §4–§5）支持"加入 `g_k`"，并视接缝验收要求决定是否再加低阶空间乘性面。

---

## 10 证据与脚本清单（`run/RELEASE-02/phot-verify/`）

| 文件 | 内容 |
|---|---|
| `wcs_lib.py` | `wcs_transform.cpp` 的 TAN+SIP 逐行 Python 移植（与 astropy 前向比对 ≤0.16″） |
| `pair_ratio.py` | 星表匹配 + `log10(F_A/F_B)` 空间分析（plane/quad/radial） |
| `run_all_pairs.py` / `pair_results.json` | 全部 111 对（同板块/相邻板块/跨望远镜）结果 |
| `pixel_measure.py` / `run_pixel.py` / `pixel_results.json` | 固定孔径像素测光 + 8×8 分箱 + 加性联合拟合 |
| `px_*.npz` | 逐对匹配星数组（位置/比值/天光/探测器坐标） |
| `vignette_test.py` / `vignette_test.json` | 天球梯度 vs 探测器径向（渐晕）模型对比 |
| `sky_vignette.py` / `sky_vignette.json` | 单帧天光径向残差（平场/渐晕残差独立探测） |
| `radius_test.py` | 孔径半径敏感性（约束视宁度系统差） |
| `evidence_wiring.txt` | "归一化未执行"的接线证据（12 板块 provenance、0 caller 等） |
| `evidence_summary.json` | 汇总表 |

**复现**：
```bash
export TMPDIR=/dev/shm/astrocs_phot
cd run/RELEASE-02/phot-verify
python3 run_all_pairs.py      # ~4 min
python3 run_pixel.py          # ~2 min
python3 sky_vignette.py       # ~2 min
python3 vignette_test.py
python3 radius_test.py
```

---

## 11 局限与未决（诚实登记）

1. **星云场无法可靠测空间乘性**：t2_m2/m3/m5/m6 与 t3_m2/m3/m5/m6 大部分 overlap 落在 M42 星云，
   逐星散度 0.06–0.36 dex，被背景系统误差主导；本报告的空间乘性结论**只基于干净天区**
   （t2_m1、t2_m4、t2_m6、t3_m1、t3_m6 附近），样本对有限（n=104–1296）。
2. **渐晕 vs 梯度不可唯一分离**：overlap 内两模型退化；"更符合探测器固定残差"是基于
   同指向对小 dither 无空间结构的**推断**，非直接证明。
3. **跨望远镜帧级比有 0.05–0.08 mag 的孔径/PSF 形状系统差**（r=3→10px），故跨望远镜的
   帧级比绝对值不确定；空间 PTP 相对可靠。
4. **未独立复算 Gaia XP F_syn**：本核实没有解码 64GB XPSD DB（无 Python reader，且不允许
   自行编译），因此没有独立重跑完整的 k_photo 拟合；帧间乘性差是用**纯内部星点比**测的
   （不需要 Gaia）。若要独立验证 F_syn 链路，需要现成的 `photometric_calib` 可执行/so。
5. **没有 χ² 可报**：拟合实现本身不产出 χ²；本报告的"拟合质量"用逐星散度（MAD）代替。
6. **UNRESOLVED 需负责人裁决**：`PHASE2_UPM.md:182` 的模型冲突（纯加性 vs g_k·s+b_k）
   超出本核实权限，本报告只提供证据。
