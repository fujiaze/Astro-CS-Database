# Phase2 UPM (Control Photometry) Science (SCI-UPM)

> 上游：ASTROCS_DESIGN.md §5.4（天光平面与统一相对模型）

> 本文件条款为冻结定义，变更走变更流程。

## 1 目的与非目标

- **目的**：在多帧覆盖并集上建立唯一的联合加性光度模型 UPM，消除逐帧背景/零点差，使校准后样本 `calibrated = raw − C_f(p)` 在全域可比；提供控制权重与持久化 frame_id 绑定。
- **非目标**：不处理乘性尺度差（**本期口径：纯加性模型**，`g_k ≡ 1` 不启用，见 §14a）；不做 Drizzle 方差估计（SCI-NOISE）；不做最终加权积分与排异判定（SCI-INT/SCI-REJ）；不跨滤镜统一（filter 分组由调用方保证）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `f, frame_id` | 帧标识（稳定科学 payload uint64，`DATA-FRAME-ID-001`） | `upm.cpp:sampler.cpp` |
| `C_f(p)` | 帧 f 在像素 p 的加性校正场（8×8 control cell 双线性） | `calibrate_block` |
| `θ` | UPM 控制系数向量（每帧每 control cell 一值） | `parameter_rows` |
| `raw` | 校准前样本 patch median | `P2ControlObservation` |
| `control_variance` | `k_corr·(π/2)·σ_bg²/N_retained` | `ALG-UPM-CONTROL-IVAR-001` |
| `control_ivar` | `1/control_variance` | `p2_upm_raw_weight` |
| `quality_factor` | 质量因子（cosmic/geom 质量） | `SCI-UPM-WEIGHT-001` |
| `control_reliability`（旧名 `geometric_reliability`） | per-control 相对可靠度。**实现事实 = 配置常量**（`P2UpmBuildConfig.control_reliability`，默认 1.0，`upm.cpp`），**不是**按单 control 覆盖度算出的几何量 | `upm.cpp`（字段注释；默认 1.0 与越域回退 1.0）|
| `k_corr` | Drizzle 相关校正（`k_corr = N_retained/N_eff`，与 `UNCERTAINTY_AND_COVARIANCE.md` UPM 节同口径）；定义域 **1 < k_corr**（`k_corr = 1` ⇔ 忽略相关，显式拒）；公式面 = 两因子 `k_gauss(N)×k_geo` 几何查表，代码默认 1.4 为实现记录 | `sampler.cpp`（默认、取值与消费） |
| `w_cell` | 求解器内 per-control 份额权重（无量纲，`Σ_cell w_cell = control_reliability`） | `upm.cpp`（归一化注释） |
| `N_retained` | clipping 后保留样本数 | `P2ControlObservation` |
| `parameter_rows[index]` | 第 index 帧的 θ 行 | `upm.cpp:parameter_rows` |
| `frame_id_by_index[index]` | 第 index 帧的稳定 id | `frame_id_by_index` |

## 3 物理量和单位

- `C, raw, calibrated, σ_bg`: **面亮度 ADU·sr⁻¹**（与上游 Phase1 HiPS signal 层同标度，量纲依据=写盘 BUNIT 冻结集 {ADU/sr, ADU^2/sr^2, sr^2/ADU^2}，裸 ADU 判红：`lib/infrastructure/aio/src/hiss_writer.cpp`；实测闭环见 ALG-P2-SMP-001 §2）；
  `control_variance`: **(ADU·sr⁻¹)²**；`control_ivar`: **(ADU·sr⁻¹)⁻²**；
  `quality, control_reliability`: 无量纲 [0,1]；
  `w_UPM`: **(ADU·sr⁻¹)⁻²（绝对逆方差量）**——注意其量纲随上游 signal 标度而变，**不是**与仪器无关的常数；跨标度（如与 ADU 域孔径测光量）混用前必须先声明 Ω_px 换算；
  `w_cell`: **无量纲（份额式，单位元 = 无量纲）**；`N_retained`: 无量纲；`frame_id`: 无量纲 uint64；`θ`: 面亮度 ADU·sr⁻¹。

## 4 输入有效域

- 帧数 `n_frames ≥2` 且至少一 control cell 有 `≥2` 帧 clean 覆盖，否则 harmonic continuation 填单帧区；`n_control_points` 可为 0（→ NO_DATA）。
- `control_ivar` 有效要求 `use_ivar_weight=1` 时 `control_ivar>0` 且有限，否则 `p2_upm_raw_weight rc=2 → build rc=2`（`DATA-UPM-CONTROL-UNC-001`）。
- `k_corr` 为 `frames[f].kcorr>0 ? per-frame : cfg.control_k_corr`，缺省 1.4（`sampler.cpp`）；
  **定义域 1 < k_corr**（`k_corr = N_retained/N_eff`，正本口径见 `UNCERTAINTY_AND_COVARIANCE.md` UPM 节 / SCI-UPM §5/§6）：`p2_upm_control_variance`（`upm.cpp`）对 k_corr<1 返回 rc=1、
  对 k_corr=1 返回 rc=2、对非冻结值缺 `calibration_run_id` 返回 rc=3、缺 `applicability_domain` 返回 rc=4；
  `p2_upm_ma_build`（`upm.cpp`）对上述四类越域一律返回 rc=7。
  物理依据：k_corr 表征 Drizzle 输出像素协方差使 `N_eff ≤ N_retained`；k_corr<1 ⇔ N_eff>N_retained，
  正相关样本的有效样本量不可能大于样本数；k_corr=1 ⇔ 忽略相关。**两者都显式拒，不静默饱和**。
  边界已有判别力测试：`eng/tests/unit/p2_upm/p2_upm_ma_test.cpp`（0.999999→rc=1、1.0000001→rc=0）。
  **适用域（正向约束）**：k_corr 只在**其标定域内**有实证意义。
  1.3883 是标定几何专属（源 300″/px、nside=512→412.26″/px、pixfrac=0.8、
  全 touched patch N≈225–251、逐实现 MAD 取跨实现中位）的 MC 实测带
  1.27–1.43（中心 1.34±0.04）内的一次实现值；受控复现 1.3445±0.0416（16 相位 × 8 seed）；
  证据源 control_median_mc_test 已注册（lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt）⇒ 可复跑。
  生产公式面为**两因子 `k_gauss(N_retained) × k_geo` 几何查表**（k_gauss 表、
  k_geo 域与消费规则见 §5 注；公式与查表由 P3 单元 `实验/healpix-polar` 承载）；
  逐帧标定表的适用域为源像素角尺度 [300,600]″/px，
  而全部已知生产帧的源像素角尺度 ≈1″/px（M42 真帧 0.98902″/px，由 FITS 头
  FOCALLEN/XPIXSZ 换算）⇒ **逐帧标定在生产尺度上不生效**，生产实现现仍回退未在本尺度
  标定的代码默认 1.4（实现记录）；消费必须声明标定元组 (ρ, pixfrac,
  帧数/dither, patch 构成) 与 N 档，不一致时 **fail-closed 拒绝或现场 MC 重标**。
  写盘门（FZ-PROV-KCORR，
  `upm.cpp`）要求 `k_corr_applicability_domain` 非空，且 k_corr ≠ 1.4 时
  另给 `k_corr_calibration_run_id`；任何引用 `control_variance` 的陈述必须与所用 k_corr、
  其标定域、以及「该域是否覆盖当前数据尺度」一同给出。
- 标定表 scale 维（300–600″/px）**不适用于源像素角尺度 <300″/px 的帧**：
  生产真帧源像素角尺度 ≈0.99″/px 在域外，域外一律显式回退代码默认 1.4（实现记录；公式面见上条）
  （禁 clamp 静默饱和）；域外帧保留 `kcorr=0` 并打 `[sampler] k_corr 域外回退` 标
  （`sampler.cpp` 查表域界、回退与打标）。
- `parameter_rows` 与 `frame_id_by_index` 同长、无重复，绑定仅由稳定 `frame_id` 决定；容器遍历不构成绑定路径。

## 5 连续定义

```text
加性校正:
  calibrated_f(p) = raw_f(p) − C_f(p)
  C_f(p) = 双线性(8×8 control cell, θ_f)

科学权重 (V19R3 冻结 SCI-UPM-WEIGHT-001)：两级两个量，禁止混名——
  (1) 绝对式 w_UPM（科学定义，**(ADU·sr⁻¹)⁻²**，任何需要绝对精度的消费面：API/门/报告）
      w_UPM = quality_factor × control_reliability × control_ivar
  (2) 份额式 w_cell（求解器内归一约定，无量纲，per-control；实现消费的就是它）
      w_cell = w_UPM / Σ_cell w_UPM × control_reliability,  Σ_cell w_cell = control_reliability
  control_ivar = 1 / control_variance
  control_variance = k_corr × (π/2) × σ_bg² / N_retained
  # 精确形式 Var(median) = 1/(4·N·f(m)²)，高斯特例 = πσ²/(2N)；
  #   成立条件 = iid ∧ 分布近似高斯 ∧ 无结构分量；
  #   有限 N 修正（精确序统计量积分 + 固定 seed MC）：σ 已知的纯公式口径下渐近式对真方差
  #   **恒为高估**——κ(N)=N·Var(median)/σ² = 1.4342 (N=5) / 1.5308 (17) / 1.5604 (65) /
  #   1.5685 (289)，即公式高估 +9.5% (N=5) / +2.6% (17) / +0.7% (65) / <0.4% (≥129)，
  #   |偏差| <1% ⟺ N≥49（奇）；方向 = 发布方差偏大 ⇒ control_ivar 偏小 ⇒ 权重偏保守。
  #   生产端到端口径（σ_bg = 1.482602218505602·MAD 同一 patch plug-in）中 MAD 尺度估计器的小样本向下偏
  #   （E[σ̂²]/σ² = 0.906 (N=5) / 0.986 (17) / ≥0.997 (≥65)）与纯公式口径同量级反号抵消：
  #   端到端发布偏差 N=5 为 −0.6%、N≈11 峰 +1.5%、N≥65 ≤0.5%（奇 N 全域 |bias| ≤1.5%）。
  #   N_retained 为偶数时中位数取两中央序统计量均值（median_of），真方差低于相邻奇 N
  #   （κ(20)=1.470 vs κ(21)=1.538）而 MAD 偏置与奇偶无关 ⇒ 端到端可高估
  #   +5.0% (N=20) / +2.8% (40) / +1.3% (100)。
  #   含 3σ 亮端裁剪的生产链（裁剪后 MAD、n_retained）下：N=5 不触发裁剪（−0.8%）；
  #   N≥9 触发率 12–35%，发布 cvar 对被估量（裁剪后中位数，触发时奇偶翻转）低估 1.3–3.2%。
  #   三口径小结：纯公式口径高估（保守方向）、端到端偏差有界、
  #   生产链亮端裁剪臂低估；偶 N 奇偶效应与生产链裁剪语义的正本 = `实验/absolute-snr/`
  #   （results 与 REPORT_experiment.md）。
  #   非高斯域不成立（均匀 1.91×、拉普拉斯 0.335×，实测见 ALG-P2-SMP-001 §5.4）；
  #   结构主导 patch 的偏差比有限 N 修正大 1–2 个数量级（结构臂 b=0.3σ/样本时端到端高估 +117%）。
  #   出处：Serfling 1980 §2.3.2（ISBN 0-471-02403-1 / DOI 10.1002/9780470316481）。
  # σ_bg = patch 内样本稳健尺度（含结构分量）；背景主导 patch 才等于噪声尺度
  #   （实测 3.78% 的 M42 真帧 patch 结构抬升 >2×，ALG-P2-SMP-001 §5.4）。
  # σ_bg_raw = 0（patch 内 ≥ 半数像素同值）⇒ 无尺度信息：control_ivar 必须为 0，
  #   禁止以数值保护量生成有限方差发布（ALG-P2-SMP-001 §5.4）。
  # control estimator = patch median (非单 leaf)
  # N_retained = clipping 后保留数 (非 n_total)；域 [min_samples, (2r+1)²] = [5, 289]
  # k_corr = k_gauss(N_retained) × k_geo（两因子几何查表；公式与查表由 P3 单元
  #   实验/healpix-polar 承载；k_gauss 表（照抄正本 DERIVATIONS-P3 §D8 全表）：
  #   N=5→1.637、9→1.316、17→1.144、25→1.083、49→1.046、≥121→≈1.00
  #   k_geo：紧凑 patch 1.27±0.03 / 全 touched ≈1.43–1.45 / 远散 ≈1.00；
  #   消费规则 = 标定元组 (ρ, pixfrac, 帧数/dither, patch 构成) + N 档声明，
  #   否则 fail-closed 拒绝或现场 MC 重标，见 §4）
  #   代码默认 1.4 为实现记录（标定域两端失保守：N=5 端低估 control_variance、
  #   源 583–600″ 端低估）；标定几何专属 MC 读数正本见 `实验/healpix-polar/`，
  #   证据源 control_median_mc_test
  #   （**已注册：见 §11/§13/§15 的 EXECUTABLE 登记**）
  #   **1 < k_corr**（k_corr=1 与 k_corr<1 一律显式拒，§4）
  # N_eff = N_retained / k_corr
  # 禁 production 乘 star SNR / snr²/(1+snr²) / support^p；support 仅 eligibility/coverage
  # legacy snr²/(1+snr²)/unc² 仅 use_ivar_weight=0 ablation/诊断 (SNR-015)

求解 (UPM_SOLVER.md):
  Huber IRLS + control-ivar 感知权重 + 弱零锚 (zero_anchor_weight=0.001) + 连通分量独立 gauge
  每分量参考帧 = 最小 frame_id

收敛与容差 (SCI-UPM-CONV-001；容差随观测尺度归一，禁绝对容差):
  配置面（唯一事实源 = P2UpmBuildConfig，upm.h）:
    tolerance（默认 1e-6，标量，同时作步长判据的容差）
    tolerance_relative（默认 0；1 = tolerance 解释为相对量）
  步长判据（converged=1 的唯一判据，upm.cpp）:
    tolerance_relative=0: max_dM < tolerance ∧ max_dC < tolerance
    tolerance_relative=1: max_dM < tolerance·max(scale_obs,1) ∧ max_dC < tolerance·max(scale_obs,1)
  目标判据（converged=2 的唯一判据，upm.cpp）:
    |Δobj| / max(|obj_old|, 1e-300) < 1e-12 连续 5 次 ⇒ stalled
  scale_obs = 观测量（control 观测值）的稳健尺度，upm.cpp；**不得**用 max|M|
  converged 状态枚举: 0 = max_iter / 1 = converged / 2 = stalled / 3 = invalid
    （只读访问器 p2_upm_convergence，upm.cpp；旧模型文件未记录一律读作 0）
  **适用域（正向约束）**：绝对容差只在观测尺度 ≈1 时与相对判据等价。生产观测为面亮度
  ADU·sr⁻¹（M42 真帧天空 1210 ADU/px ÷ Ω_px 2.2991e-11 sr ⇒ ≈5.26e13 ADU·sr⁻¹），
  绝对 1e-6 比该尺度上的 ULP 严若干数量级、原理上不可达 ⇒ **生产必须 tolerance_relative=1**。
  近零尺度侧由 max(scale_obs,1) 保留绝对保护（小尺度合成数据与绝对判据逐位等价）。

持久化绑定 (SCI-UPM-PERSIST-001 / ALG-UPM-FRAME-BIND-001):
  parameter_rows[index] ↔ frame_id_by_index[index]   # 同长、无重复
  绑定仅由稳定 frame_id 决定；save→close→open 保持 frame_id→θ 映射
  payload = truncated-64 canonical SHA-256 of science payload (DATA-FRAME-ID-001)
```

与 `lib/algorithms/coverage/src/upm.cpp`（冻结头注释、build 主体、model_hash 序列化）、`lib/algorithms/coverage/src/sampler.cpp`（配置默认与校验、control 采样与 cvar 发布）、`lib/infrastructure/aio/src/aio_upm.cpp`（稀疏/稠密落盘）一致。

## 6 假设

- 帧间无乘性尺度差（**本期口径：纯加性模型**，`g_k ≡ 1`；乘性残留属低阶空间增益、归 Phase1，见 §14a）；控制点 SNR 与几何解耦（`snr_available` 语义 V4 R6）；控制采样 patch 足域近似高斯；Drizzle 相关可用 `k_corr>1` 表征（`k_corr=1` 表示忽略相关，§4 显式拒）。

## 7 独立不变量

- **常量场不变量（SCI-004 gauge 对齐）**：常数**公共**输入（各帧同值 `raw_f=C`）时 `M=C`、`C_f=0`（每分量参考帧 gauge；弱零锚微调除外），全 control cell 无空间梯度——**不写 `C_f=C`**。仅当各帧存在独立零点差时 `C_f` 才吸收 per-frame offset（参考帧之外）。
- **空 control 不传播**：无合格 control 时不产伪 `C_f`，显式 NO_DATA。
- **Huber 对称性（无量纲标准化）**：残差先标准化 `z=r/sigma_eff`，其中 `r=value−M−C`，
  `sigma_eff=max(|uncertainty|,sigma_floor)`；`Huber(δ=1.345)` 作用于无量纲 `z`：
  小残差区 `loss=0.5z²`（等价 L2），大残差区 `loss=δ|z|−0.5δ²`（L1），位置估计对称。
  **δ=1.345 的量纲与出处**：δ 无量纲（单位 = sigma_eff）；δ=1.345 是**高斯**参考分布下
  渐近效率 95% 的 Huber 阈值，出处 Huber, P. J. 1964, *Ann. Math. Statist.* **35**, 73-101,
  DOI 10.1214/aoms/1177703732（稳健位置估计的原始框架）+ Holland, P. W. & Welsch, R. E. 1977,
  *Comm. Statist.* A6, 813, DOI 10.1080/03610927708827533（IRLS 实现与 δ 取值表）+
  Huber, P. J. & Ronchetti, E. M. 2009, *Robust Statistics*, 2nd ed., Wiley,
  ISBN 978-0-470-12990-6 / DOI 10.1002/9780470434697（§4 效率表）。
  **适用域**：① z 必须无量纲（已满足）；② 参考分布为高斯时 95% 效率成立；
  ③ sigma_eff 必须携带观测的真实标度——`sigma_floor` 一旦主导（即
  `|uncertainty| < sigma_floor`），z 失去统计尺度意义，Huber 权退化为对
  `r/sigma_floor` 的固定阈值判据，此时 95% 效率的结论**不成立**。
  `sigma_floor` 的数值与量纲见 §5 常量面与 ALG-P2-SMP-001 §5.3。
- **frame_id 绑定幂等**：`save→open` 后 `parameter_rows[index]` 重开值 `max_abs==0`（`dense/sparse 1e-12` 等价门）。
- **k_corr 缩放**：`control_variance` 随 `k_corr` 线性缩放，`N_eff = N_retained / k_corr` 反比缩放；定义域 **1 < k_corr**（§4）。
- **两级权重三条性质（w_UPM 绝对式 vs w_cell 份额式）**：
  (i) 同一 control 内两观测的权比 = `w_UPM` 之比（两式在单元内一致）；
  (ii) 任何**公共**精度因子（含 `k_corr`）在份额式内严格消去（实测 model_hash 位相同）；
  (iii) 份额式**丢弃跨 control 精度**（单元总权恒 = `control_reliability`），在 λs>0
  时改变估计量 ⇒ 改用绝对权的前提 = λs/λ0 同步换算（§10）。
- **并行确定性容差（三档，冻结）**：(a) 同配置重复构建 = **位精确** + `model_hash` 逐字相同
  （构造保证：连续块划分 + 每 k/每帧不相交写 + 无共享浮点累加器）；(b) **跨 worker 数
  （1..N）= 相对容差 rtol 1e-12**（判据 `|ΔC| ≤ 1e-12·max(|C|, C_scale)`，
  `C_scale` = 该次构建 C 场量级），不是位精确 —— `compute_raw` 的 per-control 求和按 worker
  连续切片分块后按 worker 序合并，与串行索引序的结合顺序不同（FP 加法非结合），
  误差量级由 FP 加法非结合性决定，**与绝对标度成正比**；
  实测 ΔC_max = 2.22e-15（该算例 C 量级 ≈10，即 ≈1 ulp）；
  (c) 跨后端等价 = **无此合同**（判据面 = 空）。
  **适用域（正向约束）**：判据必须随标度归一。绝对量 1e-12 只在 `|C| ≈ 1` 时与 rtol 1e-12
  等价；生产 C 场为面亮度 ADU·sr⁻¹，量级由 Ω_px 决定（M42 真帧实测天空 ≈1.2e3 ADU/px，
  `实验/additive-sky-seamless/results/c7_realdata.json` 的 `pair_mismatch[*].level_i`；
  像素角尺度 0.98902″/px、Ω_px = 2.2991e-11 sr 由 FITS 头 FOCALLEN/XPIXSZ 换算 ⇒ 5.3e13 ADU·sr⁻¹），
  此时 ulp 量级为 1e-2，
  绝对 1e-12 比 ulp 严 10 个数量级、不可达；反之在 α² 标度（≈1e-29）上绝对 1e-12
  又比 ulp 松 14 个数量级、恒真。引用绝对 1e-12 的前提 = 已声明 C 场标度。

## 7a 表示能力边界与近奇异处置

**表示能力边界（无接缝 ⟺ 公共面可表示）**：

- 参考面 `B_ref`（8×8 control cell 双线性）只能表示尺度 ≳ 2× **节点间距**的分量；节点间距 `h` 由输入几何导出
  （规则 2），**不是** tile_width/8。该实验单元取 `h = 0.0355°`（= 2× cell；见 `实验/additive-sky-seamless/code/c3_public_plane.py`
  的 `NODE_SPACING_DEG`）作为其自身网格；生产取值必须按规则 2 逐产品导出；
  **量纲与适用域（正向约束）**：`h` 的**定义域量是角尺度（度）**。换算 `h_px = h_deg × 3600 / 源像素角尺度(″/px)`：
  实验单元自身用 1″/px 网格 ⇒ `h = 127.8 px`；本仓 M42 真帧实测源像素角尺度
  0.98902″/px（FITS 头 FOCALLEN 1877 mm、XPIXSZ 9.0 µm ⇒ 206264.806×0.009/1877 = 0.98902″/px，
  复算脚本见 `实验/additive-sky-seamless/code/c7_realdata.py`）⇒ `h = 129.2 px`、`2h = 0.0710° = 258.5 px`。
  **换仪器/换像素尺度后 `h` 的度数不变、像素数按比例变**，故一切「尺度 ≲ 256 px」类的
  像素域判据必须随像素尺度重算。
- 当帧间天光差含「`B_ref` **不可表示**且沿向**相干**」的分量时，残余接缝与该分量 **RMS 线性相关**（结构性主张；正本 = `实验/additive-sky-seamless/`）；
- 该线性关系的**斜率是 fixture 专属常数、不迁移**：换夹具或换像素尺度必须重新标定，具体倍数一律不外推；
- 空间尺度 ≲ 2× 节点间距（≈256 px）时效应显著：帧间相干分量尺度自 2h 收窄至 h 时残余接缝
  增长至峰值，并在 s≲h 后**非单调**回落；跨实现稳健的是**峰值/长尺度比**这一标度统计量、
  非单调形状与肘点 s≲2h。该统计量的数值为实测读数、不冻结，读数正本见 `实验/additive-sky-seamless/`
  （复数度量口径依赖电平台阶/RMS 定义、求解器实现与单帧区填充语义）。
- **工程要求（规则 1）**：节点间距须 ≤ 目标可表示尺度的 1/2（上界 = 约束帧间改正量的最小尺度 / 2）；不可表示分量的登记面 = 显式报告；
- 该边界是**条件结论**：在可表示域内接缝压缩 12.5× 成立，域外不成立；报告与产品必须与边界一同引用（§16.2）。
- **规则 2｜节点间距必须由输入几何导出（取值面 = 输入几何，非标定常数）**：节点间距是**表示能力的唯一决定量**（规则 1），
  故它必须由输入几何**导出**，而不是取一个固定值。导出规则：**节点间距 ≤ (该产品实际约束帧间改正量的最小尺度) / 2**
  ——对「帧间加性天光差」这一目标量，该尺度是**重叠带宽度**与**指向间距**两者中的较小者（它们才是真正约束 `δ_k` 的量），
  由 manifest/输入几何逐产品算出。配置面 = 无「默认节点间距」标定值；几何量缺失 ⇒ 显式失败，不回退常数。
- **规则 3｜自适应回路的唯一旋钮是节点间距**：节点间距**必须**进入自适应重试回路，且该回路**只有**这一条自适应方向——
  判据判红（`r_eff < n_free`：网格细过数据能约束的极限）⇒ 在规则区间内**放粗**；判据判绿且残差仍受表示能力限制
  （细化到 `h/2` 后 `χ²_red` 至少降到 `residual_improve_ratio` 倍）⇒ **细化**。两个方向由**同一条判据**（规则 4）决定，
  **不构成两条路径**。搜索区间**必须**由输入几何给出：上界 = 规则 1 的「约束尺度 / 2」，下界 = 数据自身分辨率极限
  `max(采样点角间距, 像素角尺度)`；上界 < 下界 ⇒ 无可采纳节点间距，显式失败。逐次尝试的
  `(h, λ_eff, κ(H_red), κ(H_solve), r_eff, action, adopted)` 与生效值**必须**全部写入 provenance。
- **规则 4｜可辨识性/病态判决只有一条（正向约束）**：判决**必须**落在**未正则化**的**列均衡数据信息矩阵**上：
  `H_eq = D⁻¹ H_red D⁻¹`，`D = diag(√H_ii)`（列均衡消除参数单位自由度）；记 `H_eq` 特征值 `λ_1 ≥ … ≥ λ_n`，则
  `r_eff = #{λ_i > τ·λ_1}`、`κ = λ_1/λ_n`，**判决位唯一**：`identifiable ⟺ r_eff == n_free ⟺ κ(H_red) < 1/τ`。
  「欠定」与「病态」是同一条不等式的两种读法（`λ_n` 最先跌破 `τ·λ_1`）；判据面 = 该单一不等式。
  阈值**只有一个** `τ = rank_rtol`（**相对**口径，尺度不变），由浮点算术给出（精度地板 `max(m,n)·eps`），
  阈值来源 = 浮点算术（数据集标定与绝对条件数上限（`kappa_max` 类常数）不属阈值面）。自由度**必须**用
  `dof = n_obs − r_eff`（Andrae, Schulze-Hartung & Melchior 2010, arXiv.3754 式 (9)）；
  `χ²_red` 的分母**取** `dof = n_obs − r_eff`；（`n_obs − n_params` 属另一口径：秩亏时以它作分母使 `χ²_red` 系统性**高估**——实测 1.0714 / 1.0581——并掩盖未被约束的方向数。）。
- **规则 5｜门设在未正则化矩阵上，自适应面 = 无自由标定参数（正向约束）**：`H_solve = H_red + λ·P`（`P` 半正定）
  的条件数随 `λ` 增大有上界（`κ → 1`）⇒ 在 `H_solve` 上设门是**恒真门**，对「原问题是否可辨识」零信息；
  `κ(H_solve)` **只能**作为求解稳定性诊断量单独记账；判决面不含该量。数值正则化**只能**取判据**派生**的量
  `λ_eff = τ·mean(diag(H_red))`（判据自身的分辨率下限，无自由参数），它**不参与判决**。自适应路径的参数面 = 冻结的 `τ`、
  **绝对上限求绿与粗糙度惩罚一类自由标定值不构成自适应分支**。自适应**必须真的会被触发**：
  门控若宽到让真实数据永远通过，则自适应等价于不存在；每条自适应路径都必须有「触发过」的记录，否则视为未实现；
  表示能力到顶（在分辨率下界上残差仍不下降）**必须**如实登记；收敛声明的前提 = 残差在下界之上仍下降。
- **不收敛时报警告不阻塞（正向约束）**：迭代未达容差（`converged != 1`）或判据判红而该路径按**报告口径**处理时，
  **必须**产出**产品级机器可检警告**（具名 `warning_codes` 随产品/manifest 落盘，如 `P2-UPM-NOT-CONVERGED`、
  `P2-UPM-NOT-IDENTIFIABLE`）；构建 rc 保持不变，警告面 = 具名 `warning_codes`；判红在 fail-closed 路径上（GLS/MA 求解器、
  天光面求解器）按其求解器返回码显式失败。

**近奇异（条件数）处置**：

- **判据读数必须可观测并写入 provenance（正向约束）**：列均衡数据信息矩阵的 `r_eff` 与 `κ(H_red)`、唯一阈值 `τ = rank_rtol`
  及其**生效值**（`rank_rtol_effective = max(τ, max(m,n)·eps)`）、未被约束方向数 `n_unidentified`，以及**求解稳定性诊断量**
  `κ(H_solve)` 与派生数值岭 `λ_eff`。**引用任何 κ 的陈述必须写明矩阵口径（`H_red` 还是 `H_solve`）与所用 `τ`；不同口径的 κ 不可比。**
  真实 M42 样本的实测判据读数：`rank = n_nodes = 49`、`n_params = 58`、`χ²_red = 1.004`、`κ ≈ 3.2×10⁷`（读数与矩阵口径一同引用）。
- **判红处置（正向约束）**：`r_eff < n_free`（等价 `κ(H_red) > 1/τ`）⇒ **必须**走规则 3 的节点间距自适应，并登记所走方向、
  尝试次数、生效参数与最终判据读数；欠定解的处置 = 规则 3 自适应并登记，判绿参数面 = 冻结 `τ`（绝对条件数上限不属判据面）。
- **判决面 = `H_red` 的条件数（`H_solve` 读数只作诊断）**：`H_solve = H_red + λ·P`（`P` 半正定）的条件数随 `λ` 增大有上界
  （`λ → ∞ ⇒ κ → 1`）⇒ 在它上面设门是**恒真门**，对「原问题是否可辨识」零信息（Hansen《Regularization Tools》v4.1 手册 §1/§2.7.3：
  用 `A` 导出的良态矩阵替换 `A` 不保证得到有用的解，正则化刻画的是一个**新问题**）。`κ(H_solve)` 只作
  「求解稳定性」诊断量单独记账（`P2SkyPlaneInfo.kappa_solve` / JSON `kappa_solve`）；`λ = 0` 时两读数逐位相等，这是回归锁，不是判决依据。
- **自由度（正向约束）**：`χ²_red` 的分母**必须**是 `dof = n_obs − r_eff`（Andrae et al. 2010 式 (9)）；`n_obs − n_params` 属另一口径。
- **provenance 最小集**：`gauge_mode`、每分量 `ref_frame_id`、`rank`（`r_eff`）、`rank_rtol`、`rank_rtol_effective`、
  `n_unidentified`、`kappa`（`H_red`）、`kappa_solve`（`H_solve`）、`lambda_numerical`、`node_spacing_deg` 与自适应轨迹、
  `k_corr`、`model_hash`、`any_fail_closed_reason`。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 无重叠/无 control（`n_obs=0` / 无观测几何节点） | `rc=1`（`n_obs=0`）；无观测几何节点以 sentinel 不入数据图 | `upm.cpp`；ALG-P2-UPM-IMPL-001 §10 |
| `use_ivar_weight=1` 且 `control_ivar≤0/非有限` | `p2_upm_raw_weight rc=2 → build rc=2` | `DATA-UPM-CONTROL-UNC-001`；`upm.cpp` |
| `S=0`（patch 内 ≥ 半数像素同值 ⇒ 稳健尺度 0） | **无尺度信息**：`control_ivar` 必须为 0 且 `control_variance` 标为无尺度信息（非有限）；发布面 = 无尺度信息（数值保护量的平方不属方差发布面） | ALG-P2-SMP-001 §5.4（发布口径权威）；`sampler.cpp` 为待整改点 |
| 单帧区 | harmonic continuation 填 | `upm.cpp`；`upm.h` |
| 畸形模型文件 | `ERR-P2-UPM-001`（open 强校验，任何损坏 rc=1 稳定错误） | `aio_upm.cpp`；`upm.cpp` |
| 跨 endian payload | 理论 id 不同，当前仅 Linux x86_64 路径 | `sampler.cpp` |

## 9 精度策略

- FP64 求解；`dense cache` 与 `sparse` 求值 `1e-12` 等价门（`SparseEqualsDense`）；
  `k_corr` 代码默认 `1.4`（实现记录）；其公式面为两因子 `k_gauss(N)×k_geo`
  几何查表（§4/§5）；MC 校准来源 `control_median_mc_test` **已注册
  （见 §11/§13/§15）**——该 MC 证据可复跑，其读数按标定几何专属实测带记账，
  数值正本见 `实验/healpix-polar/`。
  **适用域（正向约束）**：该 1e-12 是**同标度下**的求值等价门；跨标度引用绝对 1e-12 的
  禁令见 §7 (b) 的适用域段。
- 并行确定性容差见 §7 三档（同配置重复位精确 + hash exact；跨 worker 数 1e-12；跨后端 = 无此合同）。

## 10 不可接受变化

- 在生产 `w_UPM` 中乘 `star SNR / support^p / obs.ivar`(已弃用)；
- `control_estimator` 必须取 patch median、`N_retained` 必须取 clipping 后保留数（单 leaf 与 `N_total` 两条路径不属于本合同）；
- 改变 `k_corr` 公式面（两因子 `k_gauss(N)×k_geo` 查表与 fail-closed 消费规则）
  或 `parameter_rows ↔ frame_id` 绑定语义；
- 在 `calibrate_block` 中引入乘性尺度；
- 在未同步换算 λs/λ0 的前提下把求解器内的 `w_cell`（份额式）替换为 `w_UPM`（绝对式）——
  份额式丢弃跨 control 精度，换绝对权等于换估计量（隐式改科学），须先重标定并登记。

## 11 验证 Oracle

- **UPMW 硬门 7 项**：001 snr 扰动不变、002 ivar 1:4→weight 1:4、003 星群不变、004 `Var(median)≈πσ²/2N`、005 MC `k_corr`（**`control_median_mc_test` 已注册，可复跑**；读数正本见 `实验/healpix-polar/`）、006 无 legacy SNR consumer 且缺 ivar `rc=2`、007 patch 真值恢复（`synthetic_gate`）。
- **持久化门**：`UpmPersistAllPermutations/RandomStableIds/SparseDenseBinding` 等 PR#1 全排列。
- **不变量门**：常量场、空 control、Huber 对称、绑定幂等四门。
- **Python 参考**：NumPy 对同 `raw` 的 Huber IRLS + `control_ivar` 权重复算 `θ`（`rtol 1e-9`）。

## 12 关联 ALG ID

- `ALG-UPM-001` `p2_upm_build` Huber IRLS 求解
- `ALG-UPM-002` `p2_upm_calibrate_block` 双线性校正
- `ALG-UPM-003` `p2_upm_raw_weight / p2_upm_normalized_weights` 权重
- `ALG-UPM-004` `aio_upm` 持久化绑定

## 13 追溯与测试

- 权威文件: `docs/science/PHASE2_UPM.md` (SCI-UPM-001..010, SCI-UPM-WEIGHT-001, SCI-UPM-PERSIST-001)
- 实现: `lib/algorithms/coverage/src/upm.cpp` (1107-1123, 493-510), `lib/algorithms/coverage/src/sampler.cpp` (250-364, 672), `lib/infrastructure/aio/src/aio_upm.cpp` (持久化)
- 公开 API: `p2_upm_build, p2_upm_calibrate_block, p2_upm_raw_weight, p2_upm_open/save`
- 测试: `TEST-UPMW-001..007, UPMW-001..007, UpmPersist*` (`synthetic_gate.cpp`)；
  `control_median_mc_test.cpp` **EXECUTABLE（已注册：进 CMake/ctest/CI 面，可复跑）**

## 3a 坐标 frame

UPM 在**像素域 control cell**（8×8 双线性网格）上工作，无 WCS/天球参与；帧绑定唯一由稳定 `frame_id`（truncated-64 SHA-256，DATA_SEMANTICS §5）决定，容器索引仅实现细节；每连通分量参考帧=最小 `frame_id`（gauge 锚，§5）。

## 9a 口径问答：观测方程、gauge 与接缝指标

- **观测方程**：`calibrated_f(p) = raw_f(p) − C_f(p)`，`C_f(p)`=帧 f 的加性校正场（8×8 control cell 双线性插值，§5）；**纯加性模型**（`g_k ≡ 1`；乘性尺度差不属本层，§1 非目标）；`raw`=校准前样本 patch median。
- **控制点**：8×8 control cell；control estimator=patch median（非单 leaf）；`N_retained`=clipping 后保留样本数（非总数）。
- **光度面 basis**：分块常数加性背景面——每帧每 control cell 一个自由度 `θ_f`，经双线性插值成连续场；自由度 = n_frames × n_control_points。
- **正则化**：Huber IRLS 鲁棒求解 + control-ivar 感知权重 + **弱零锚** `zero_anchor_weight=0.001`（弱 Tikhonov/岭型锚定向全局零）。
- **gauge/退化**：加性场对全局常数规范自由——以连通分量独立 gauge 固定（参考帧=最小 frame_id）；退化路径：无 ≥2 帧 clean 覆盖 → harmonic continuation 填单帧区；`control_ivar≤0/非有限` → `rc=2` 显式拒（§4）。
- **接缝指标（正向约束）**：接缝 = **帧足迹边界两侧的电平有符号台阶**——沿边界法向取 `±d` 差分（`d` = 采样半距，默认 2 px），
  `rel_step(e) = median(img[+d] − img[−d]) / bg(e)`，`bg(e)` 为该边界 `±d` 采样上的**局部**背景电平。判据量是**相对**口径（无量纲、可跨产品比），
  门 `max_e |rel_step(e)| ≤ 1e-2`：**该门槛的推导与实测标定正本 = 本文件 §17**（观测量与零假设分布、虚警率、可检出下限、漏检面与实测标定；本节不再指向别处索取依据），其面向要求 = `ACCEPTANCE_SPEC.md` §6.2「帧间、块间无亮度/灰度阶跃」与 `ASTROCS_DESIGN.md` §2.3「接缝 = 帧集变化处的亮度阶跃」，判据实现与默认参数 = 机器门 `CHK-L4-SEAM-FOOTPRINT`（`eng/tools/e2e/seam_footprint.py`）。
  **适用域（前置约束，不是放松阈值）**：只有**两侧都在数据内部**的帧边界计入判据——要求法向 `±ctrl_shift` 处可放置平行对照线，且两侧各有 `≥ min_samples` 个有效样本；
  一侧无数据 = 画幅/足迹**外缘**，那里的台阶是数据边缘本身，不是帧间接缝。被排除的边界仍逐条落盘（`exclude` 字段 + 全部度量）；丢弃面 = 空（逐条落盘即其登记面）。
  **方差比的适用面 = 渲染分块伪影粗筛**：电平阶跃不改变方差 ⇒ 方差比对电平接缝**原理性失明**，电平接缝的判据面不含方差比。
  判据必须**能红能绿**：注入已知台阶（`amp ≠ 0`）必须判红、`amp = 0` 必须回落到基线；无法判定（输入/依赖不可用、有效边界数为 0）走 fail-closed（"无接缝"的判定前提 = 可判定）。

## 14 Primary literature（引用定位声明）

1. **本合同为项目原创推导**（观测方程/光度面 basis/gauge/接缝语义均为 Project-defined，无外部公式依赖）。
2. Huber IRLS：Huber, P. J. 1964, "Robust Estimation of a Location Parameter", Ann. Math. Statist. 35, 73——文章级定位（bibcode 1964AnMS...35...73H，未逐页核验），仅 robust 求解框架上下文。
3. 弱零锚=弱 Tikhonov 正则：Tikhonov 解的正则化概念——教科书级，无公式引用。
4. `k_corr` 两因子公式 `k_gauss(N)×k_geo` 与几何查表（公式、查表与读数正本由 P3 单元 `实验/healpix-polar` 承载）：**项目自产 MC 证据**（`control_median_mc_test`，**已注册，本证据可复跑**），非外部文献；k_corr 必要性的文献依据 = Fruchter & Hook 2002 §7.1（Drizzle 输出像素相关），该文不含 k_corr 数值。
5. Tukey/MAD 常数：复用 SCI-002/SCI-003 文献链（PMC6768164 实证；Φ⁻¹(3/4) 恒等式）。

## 14a 参考文献与参考代码库（含许可证）

- **多帧相对光度/天体联合定标**：SCAMP（GPL-3.0，https://github.com/astromatic/scamp，tag v2.14.0 = 控制节点实测（`scamp -v` 自述 "SCAMP version 2.14.0"）；文件位置 `src/photsolve.c`；Bertin, E. 2006, ASP Conf. Ser. 351, 112，标题经 aspbooks.org 逐字核验）；Padmanabhan, N. et al. 2008, ApJ 674, 1217（SDSS 重叠观测联合相对定标、gauge/连通性）。
- **马赛克逐帧背景扣除/coadd 权重**：SWarp（GPL-3.0，https://github.com/astromatic/swarp，tag 2.41.5 = 控制节点实测（`dpkg -s swarp` → Source `swarp (2.41.5-1)`）；文件位置 `src/resample.c`、`src/coadd.c`、`src/back.c`；Bertin, E. et al. 2002, ASP Conf. Ser. 281, 228）；Gruen et al. 2014, PASP 126, 158。
- **Huber IRLS**：Huber, P. J. 1964, Ann. Math. Statist. 35, 73（DOI 10.1214/aoms/1177703732）；Holland, P. W. & Welsch, R. E. 1977, Communications in Statistics A6, 813（DOI 10.1080/03610927708827533，δ=1.345 的 IRLS 出处）；Huber & Ronchetti 2009, Robust Statistics, 2nd ed., Wiley（ISBN 978-0-470-12990-6）。
- **弱零锚（弱 Tikhonov/岭正则）**：Tikhonov, A. N. 1963, Soviet Math. Dokl. 4, 1035（**核验状态**：文章级，卷页需网络核验）。
- **var(median)≈πσ²/(2N)**：正态样本中位数渐近方差的教科书结论（Hoaglin et al. 1983；Kendall & Stuart, The Advanced Theory of Statistics Vol.1）；UPMW-004 实证 ratio 0.997。
- **稀疏天光面样条（加性天光面的表示候选）**：Duchon, J. 1977, Constructive Theory of Functions of Several Variables, 85（薄板样条）；Wahba, G. 1990, Spline Models for Observational Data, SIAM（ISBN 0-89871-244-0）。§5 冻结的加性场当前实现为 8×8 control cell 双线性；稀疏样条是**同一加性场**的另一种表示候选（数据面/schema 待合同流程），**不改变** §1/§5 的纯加性模型。
- **模型形式：纯加性（正向约束）**：本文件 §1/§5 的加性模型 `calibrated_f(p)=raw_f(p)−C_f(p)` 是 Phase2 的唯一模型形式，`g_k ≡ 1` **不启用**；乘性方向 `y_k(x)=g_k·s(x)+b_k(x)` **不在本层**。`÷g²` 保持**恒等式**（`Var(corrected)=[σ_raw²+J_out C_θ J_outᵀ]/g²`，`g≡1` 时退化等价，公式保留）。**依据**：Phase1 正确归一化后，帧本身已是同一测光体系的真信号加可等效为加性的天光；残留天光无论是加性还是乘性都可用加法移除；乘性残留属低阶空间增益，归 Phase1 处理（`I_photo = k_photo·m(x,y)·I_cal`），Phase2 只做加性扣除。`10_sampling.md`/`11_upm.md`/`UNIFIED_MODEL.md` 的目标态表述与本节一致。
- **k_corr 的 MC 证据与公式面**：control_median_mc_test 已注册（可复跑）；公式面 = 两因子 `k_gauss(N)×k_geo` 几何查表（§5/§10），读数正本 = `实验/healpix-polar/`；k_corr 必要性的文献依据 = Fruchter & Hook 2002 §7.1（输出像素非独立、§7 式(8)–(10) 的 R），该文不含 k_corr 数值。

参考代码库（含许可证）正本 = docs/engineering/SCIENTIFIC_REFERENCES.md §M。

## 15 Acceptance

- §11 Oracle 全过（含 MC 一致性、gauge 唯一性、harmonic continuation 边界）；**例外**：
  UPMW-005 的 MC 证据源 `control_median_mc_test` **已注册可复跑** ⇒ 该项
  只有常数冻结定义（§5/§10），没有可执行证据；"已过"的成立前提 = 该 MC 证据可执行；
- §7 不变量门全过；
- `eng/tools/science_contract_lint.py` PASS（15 节+claim ID+锚点）；
- 解析不变量→SYN-005 转换：已知低阶光度面恢复、重叠图 gauge/退化强度扫描、接缝指标按 §9a 的**有符号台阶**口径（门 `max|rel_step| ≤ 1e-2`、适用域与 fail-closed 同 §9a），参数恢复/残差/接缝降低且不破坏星 flux 全过。

## 16 加性天光与无接缝的实验结论

实验单元：`实验/additive-sky-seamless/`（报告 `README.md`，机器可读结果 `results/*.json`，
一键复跑 `code/run_all.sh`）。**本节只记录结论与判据形态，不改变任何公式或默认容差；
全部读数的正本 = 该实验单元。**

### 16.1 已证实的结论（data 级，生产代码实测）

1. **多退少补正确**：产品 `calibrated_k = raw_k − δ_k`（**保留 B_ref**）在覆盖子集突变处压缩
   背景电平接缝，产品中位回到 B_ref 量级；全减背景臂（`raw − b_k`）⇒ **退化**；
   无接缝证据面 = 保留 B_ref 的臂。
2. **B_ref 是规范零点**：`b_k − δ_k` 与帧无关；gauge 0↔1 只造成 δ_k 的逐帧常数偏移，
   接缝度量不变（两臂逐位等价）。
3. **权重**：`control_ivar` 臂的伪影漏入与噪声 RMS 均为三臂最小（`uniform` 与 `SNR²` 两臂更大），
   与独立结论一致。
4. **稀疏现场求值**：dense cache 与 sparse `calibrate_block` 逐位等价；稀疏模型体积远小于稠密
   物化，按需求值块的峰值 RSS 亦低于稠密物化。

### 16.2 适用域边界（必须与结论一同引用）

1. **无接缝 ⟺ 公共面可表示**。当帧间天光差含「B_ref 不可表示且沿 y 相干」的分量时，
   残余接缝与该分量 **RMS 线性相关**（结构性主张）；**斜率是 fixture 专属常数、不迁移**，
   换夹具或换像素尺度必须重新标定。空间尺度 ≲2× 节点间距（≈256 px）时效应显著
   （帧间相干分量尺度自 2h 收窄至 h 时残余接缝增长至峰值，s≲h 后非单调回落）；
   跨实现稳健的是**峰值/长尺度比**这一标度统计量、非单调形状与肘点 s≲2h，其数值为实测读数、
   不冻结，读数正本见 `实验/additive-sky-seamless/`。

2. **纯加性前提**：帧间乘性差必须先在 Phase1 吸收。未做 Phase1 归一时，纯加性 UPM 后仍显著
   存在电平接缝；做了 Phase1 归一后接缝大幅压缩（两条臂的读数正本见 `实验/additive-sky-seamless/`）。
   基外高频乘性分量对**电平**接缝贡献有界，但会被分块 PSD 检出
   （读数正本 = `实验/additive-sky-seamless/results/` 的 `hf_component.psd_k_peak`）。

3. **不可检验域**：`smoothing_lambda=0` 时 per-(frame,cell) 自由加性场恰好定解，公共场 M 只是
   每 cell 的规范选择 ⇒「拟合/堆叠权重同源」与「末端残差场扣除」在该域内**不可检验**；
   `final_gauge` 在 `m_full_frame=1` 时近似 no-op，且**不能**修复子集依赖。

### 16.3 生产实现现状与待满足项（逐条可核验）

| 项 | 规范要求（§5/§7a） | 实现现状（实测锚） | 状态 |
|---|---|---|---|
| 收敛判据的尺度归一 | 生产必须走相对/尺度归一判据（绝对 1e-6 在生产尺度不可达） | `module_adapters.cpp` 取 `tolerance=1e-6` 与 `tolerance_relative=1`（相对判据，分母 `max(scale_obs,1)`）；`p2_session.cpp` 取 `tolerance=1e-6` 而**未设** `tolerance_relative`（零初始化 ⇒ 绝对判据） | **两入口口径待统一**（偏差登记见 ALG 层） |
| `converged` 状态枚举 0/1/2/3 | 只读访问器暴露 `0=max_iter/1=converged/2=stalled/3=invalid`，并写入数据面 | `upm.cpp`（`kStallPatience = 5`、stalled 赋值 `m->converged = 2`、数据面 JSON 暴露 `j["converged"]`）与 `p2_upm_convergence` C ABI；`module_adapters.cpp` 读入（`p2_upm_convergence`，读不到按未证明收敛）并进 provenance KV 与 manifest | **已满足** |
| 可辨识性判决 κ | 判决取**未正则化**的列均衡数据信息矩阵：`r_eff < n_free ⟺ κ(H_red) > 1/τ`；`κ(H_solve)` 只作求解稳定性诊断 | `sky_plane.cpp`（判据 + fail-closed）、`identifiability.h`（天光面与 UPM 共用同一函数）、`module_adapters.cpp`（节点间距由输入几何导出；几何 provenance 落 manifest） | **已满足** |
| κ 口径 | **唯一阈值** `τ = rank_rtol`（相对口径、尺度不变、精度地板 `max(m,n)·eps`）；**不存在**绝对条件数上限 | `sky_plane.h`、`upm.h`（同一符号/同一值/同一实现） | **已统一** |
| 自适应重试的授权边界 | 判绿参数面 = 冻结 `τ`；唯一自适应方向 = 节点间距（§7a 规则 3/5） | 收敛判据取 `tolerance_relative=1`（分母 `max(scale_obs,1)`，§4/§5）；自适应重试只作用于节点间距；实现与口径的偏差登记见 ALG 层 | **已满足** |

**正向约束**：上表「待统一」项在收敛前，任何引用 `converged` 或 `tolerance_relative` 的陈述必须写明**所用入口**；引用 κ 的陈述必须写明**矩阵口径（`H_red`/`H_solve`）与所用 `τ`**（§7a）。


## 17 接缝门槛 `max_e |rel_step(e)| ≤ 1e-2` 的推导（观测量与噪声模型）

> 本节给出 §9a 接缝指标门槛的**推导链**：观测量 → 零假设分布 → 虚警率 → 可检出下限 → 漏检面。
> **本节不改动门槛数值、判据式、默认参数与适用域**：它把门槛标定到实测噪声上，并给出它的性质。
> 证据与可复跑脚本正本：`实验/additive-sky-seamless/`（报告、`results/*.json` 与 `code/`）。

### 17.1 观测量与零假设模型

判据量（§9a；实现 = 机器门 `CHK-L4-SEAM-FOOTPRINT` 的 `edge_metric`，本节与实现逐字同源）：

    rel_step(e) = median_i seam_i(e) / bg(e)
    seam_i(e)   = I(p_i + d·n_e) − I(p_i − d·n_e),   i = 1..N_e
    bg(e)       = median( |I| over the 2·N_e 个 ±d 采样点 )      （边界自身的**局部**背景电平）

`d` = 采样半距（默认 2 px），`N_e` = 该边界折线上的有效采样数（默认 256 点；M42 实测中位 256、最小 252）。

- **零假设 H0（无接缝）下**：`seam_i = [I_true(p_i + d·n) − I_true(p_i − d·n)] + [ε₊ − ε₋]`。
  第一项是**背景梯度项** `2d·∂I/∂n + O(d²∂²I/∂n²)`（系统性偏置，逐边不同，不是噪声）；
  第二项是**采样噪声**。⇒ 判据量在真实产品上是「台阶 + 梯度」的混合量。
- **中位数估计量的标准误**（渐近、正态样本）：`Var(median) = π σ²/(2N)` ⇒
  `se(median) = √(π/2)·σ_seam/√N_e`，**中位数的渐近标准误系数 = √(π/2) = 1.2533141…**。故

      σ_rel(e) = √(π/2) · MAD(seam)(e) / ( √N_e · bg(e) )        （MAD = 1.482602218505602×中位绝对偏差）

- **`σ_seam` 必须取实测**：`σ_seam = √2·σ_pix` 只在 ±d 采样互不相关时成立；d = 2 px 的采样点
  多落在亚像素位置，双线性插值会再压一次方差（各向异性权重 ⇒ 单样本方差因子 ≈ 4/9）。
  合成夹具实测：按 `√2σ_pix` 反推得 1.186e-3，实测 `√(π/2)·MAD(seam)/(√N·bg)` = 7.45e-4，
  **解析式高估 1.59 倍** ⇒ 一律用工具落盘的 `seam_mad`。

### 17.2 门限对应的虚警率

三种零假设口径（同一合格产品的全部帧足迹边界；条数与适用域计数为实测读数，正本见
`实验/additive-sky-seamless/results/`）：

| 零假设口径 | σ(rel_step) | 1e-2 = kσ | 双边虚警（正态） | 族系虚警（114 条，双边 max\|rel_step\|，正态） |
|---|---|---|---|---|
| A 逐边解析（纯估计量噪声，逐边中位） | 1.187e-3 | **8.42** | 3.7e-17 | 6.9e-4（MC 2×10⁵ 次 max_e\|rel_step\|；由逐边最大 σ = 2.851e-3 主导） |
| B2 off-locus 对照线（同估计量、无法向 200 px 帧边界） | 1.869e-3 | **5.35** | 8.7e-8 | 1.0e-5 |
| B1 实测跨边散布（含背景梯度项与残余真实接缝） | 2.814e-3 | **3.55** | 3.8e-4 | **4.2%** |

- **A 与 B1 差 2.37 倍，方向明确**：估计量自身的采样噪声只占实测跨边散布的约 42%，其余是
  `2d·∂I/∂n` 梯度项与残余真实接缝。d 扫描判别（`|rel_step_d4x|/|rel_step|`：纯台阶 = 1、纯梯度 = 4）
  实测中位 1.73、43% 的边 > 2、27% 的边 > 3 ⇒ 真实产品上判据量以梯度成分为主。
  **故门的判读必须与 `step_net`、d 扫描一起做**（§9a 已把它们列为诊断量）。

- **正态口径在尾部失效**：对照线（= 同一估计量施加在**无缝位置**）114 条里 **1 条**
  `|ctrl_step/bg_ctrl| = 1.020e-2 > 门`，经验虚警（判据量取绝对值，双边口径）**0.88%**（Clopper–Pearson 95% 区间 [0.022%, 4.8%]）；
  同一批数据的正态预测 8.7e-8 ⇒ **差约 5 个数量级**（8.8e-3 / 8.7e-8 ≈ 1.0×10⁵）。非高斯尾部来自局域亮结构（星/星云）与**分母口径**：
  同一条对照线用边界自身 `bg` 计得 0.78e-2（判绿）、用对照线自己的 `bg_ctrl` 计得 1.02e-2（判红）
  ⇒ **比值判据在分母上就有约 30% 的口径摆动**。少数事件也说明该经验率本身极不确定（1/114）：
  族系虚警的点估计 63%、95% 区间 [2.5%, 99.6%]。

- **相对口径的分母条件（隐含适用域）**：`bg` 与单像素噪声之比实测 38.9–150.2（中位 93.4）。
  分母越小 ⇒ `σ_pix/bg` 越大 ⇒ 比值判据越病态；产品若把背景扣到近 0（`raw − C_k` 全减路径，
  §9a 已列为**退化**路径），`rel_step` 无界、门失去意义。⇒ 该门隐含前提 = **保留背景**（与 §9a 同源）。

### 17.3 门限能检出的最小真实台阶（含漏检面）

判决量是 **`max_e |rel_step(e)|`**，不是单条边的 rel_step ⇒ 检出概率带**多重性**：
同一个台阶压在受影响帧的 2–4 条适用域边界上（取 max 在检出方向上是**帮手**）；
无接缝时 114 条边界里最大的那条才决定判决（在虚警方向上是**代价**）。

- **确定性下限（无噪声硬界）**：`rel_step = Δ/(L + Δ/2)`（L = 边界一侧局部电平；`bg` 实测 = L + Δ/2）
  ⇒ `Δ/L > gate/(1 − gate/2) = 1.0050%`（约 0.0109 mag）**必判红**；小于等于 1.0050% 时无噪声下必判绿。
- **统计下限（含噪声）**：`P_detect = 1 − Π_e Φ((gate−μ_e)/σ_e)···`，`μ_e = Δ/(L_e + Δ/2)`，
  `n_s` = 受影响边数：

| σ 口径 | n_s = 1 | n_s = 2 | n_s = 4 |
|---|---|---|---|
| 逐边解析 1.187e-3：@50% / @95% | 1.005% / 1.203% | 0.940% / 1.096% | 0.885% / 1.013% |
| 实测跨边 2.814e-3：@50% / @95% | 0.997% / 1.471% | 0.844% / 1.219% | 0.717% / 1.023% |

  合成夹具实测（噪声取到与 M42 **同量级**：`σ_pix/bg = 1.0703e-2`；4 条边受影响、30 seed/点）：
  `Δ/L = 0.90% ⇒ 17%`、`0.95% ⇒ 70%`、`1.00% ⇒ 96.7%`、`1.05%` 与 `1.10%` ⇒ 100%；
  含多重性的解析预测在同点位给 31% / 66% / 92% / 99.4% / 99.99% ⇒ **实测落在解析预测的二项波动内**；
  忽略多重性的**单边预测**（8% / 23% / 47% / 72% / 90%）**系统性偏低，是错误形式**；判定以含多重性的解析预测为准。

- **漏检面（必须按此判读；这是本门的适用域边界）**：
  1. `Δ/L ≤ 1.005%`：低于确定性下限，**任何噪声下都不判红**（放行）；
  2. `1.0% < Δ/L < 1.5%`：**概率性漏检**，最坏可漏掉一半（n_s = 1 且取实测跨边 σ 时，95% 检出要 1.47%）；
  3. **过渡宽度 `w > 2d = 4 px` 的平滑接缝**：±d 差分只看到 `Δ·2d/w` ⇒ 等效门限被抬高 `w/2d` 倍
     （w = 8 px ⇒ 2.0%、16 px ⇒ 4.0%、32 px ⇒ 8.0%）⇒ 对**权重斜坡/羽化平滑过的接缝原理性不敏感**；
  4. **边界长度覆盖不足**：判据取**中位数**，边界上只有比例 `f` 的采样点承载台阶时 `f < 1/2` 恒有
     `median = 0` ⇒ 对**短于边界一半的局域接缝结构性失明**（判据量是「典型台阶」而非「最大台阶」；**该恒零仅在无噪极限下成立**——带噪时 median 由 §17.1 零假设的梯度项＋采样噪声决定，`f < 1/2` 只把判据量压到噪声水位，并非恒零失明）；
  5. **适用域之外**：足迹外缘或贴数据边界的边界**不进判据**（条数为实测读数，正本见 `实验/additive-sky-seamless/results/`），那里没有无接缝证据。

  6. **光滑法向斜坡的伪阳（判据量的梯度项，与 1–5 方向相反）**：§17.1 的梯度项相对化后
     `rel_step = 2d·ρ + Δ/bg`（ρ = 边界处**相对**法向斜率，无量纲/px），真值**无台阶**（Δ = 0）时该量
     仍非零 ⇒ ρ ≥ `gate/(2d)` = **0.25%/px 单独即把判据推过门**。合成夹具（ρ 按边界处局部电平归一）：
     ρ = 0.26%/px ⇒ rel_step = 0.0104 **判红而真值无接缝**（伪阳）；ρ = 0.10%/px ⇒ rel_step = 0.0040，
     而 d 扫描诊断量 `rel_step_d4x`（同式取 4d = 8 px）已达 0.0160，越门 1.6×（该诊断量在 ρ ≥ 0.0625%/px
     时即越门）；纯斜坡的 d 扫描比值恒 = 4、真实台阶恒 = 1，据此可判别。
     `Δ` 项与 `2d·ρ` 项在判据量里**加性耦合**，任一项单独越门即判红 ⇒ 1–5 是「真值有台阶却判绿」，
     本条是「真值无台阶却判红」，两类边界都必须随门一并陈述。

     证据（实验单元，只读调用生产门本体 `edge_metric`/`gate_decision`，seed = 20260928）：
     `实验/additive-sky-seamless/code/seam_gate_gradient_scan.py` ⇒ `实验/additive-sky-seamless/results/seam_gate_gradient_scan.json`
     （20 项断言全绿；负例 ρ = 0 ⇒ rel_step 恒 0、4000 次 MC 全绿 ⇒ 判据非恒真）。
     斜坡不敏感化的候选判据 `Δ̂ = (4·s_d − s_4d)/3` 仅作为实验证据落盘，**尚未进入判决面**：
     判据面变更须走变更流程，本条只登记适用域边界，不改变任何阈值或公式（同 §17.4「本节不放宽任何判据」）。

### 17.4 实测标定与 `1e-2` 的性质

- 合格产品在适用域内的 `max|rel_step|`、中位与 p90，以及超门边**全部**落在 `not_interior`（被适用域排除）
  这一事实，构成适用域在**数值上**必要的证据（`实验/additive-sky-seamless/results/`：没有适用域，
  同一合格产品会出现超门判红）。读数不在本节复述。

- **`1e-2` 与实测跨边散布的倍数关系由三条独立标定给出**（`实验/additive-sky-seamless/results/`）。
  ⇒ `1e-2` **不是**宽裕的工程裕度，而是「恰好压在一个合格产品的实测散布之上」。

- **`1e-2` 的性质（必须如实标注）**：该值最初来自 SCI-C 实验单元的「相对接缝度量 < 1%」**要求**
  （`实验/additive-sky-seamless/README.md` 的 R5 行；该处**实测值** 3.9e-4），由机器门实现采纳为门。
  它**不是**由误差预算反解出来的阈值——既没有先定的虚警预算，也没有先定的可检出下限。
  本节给出的是它的**事后标定**：「实测跨边散布的 3.5 倍」+「确定性地放过 `Δ/L ≤ 1.0%` 的台阶」
  +「`Δ/L` 落在 (1.0%, 1.5%) 时概率性漏检」。若要把它重新定义为误差预算导出的阈值，须先定
  (a) 可接受虚警率与 (b) 目标可检出 `Δ/L`，再按 §17.2 / §17.3 反解；在两者未定之前，
  本节结果只作**事后标定**，不构成「已按误差预算设计」的依据，也不构成放宽门的依据（本节不放宽任何判据）。

- **负例（门的判红与推导预测一致）**：按 §17.3 的临界注入幅度向真实产品注入台阶并逐档比对，
  判决翻转点落在临界值附近（约 0.8–0.9 倍临界之间），逐档判决与解析预测一致；预测与实测在
  翻转点附近的差异来自被注入多边形的足迹与**别的帧边界相交**这一二阶效应。
  逐条证据、帧标识与幅度读数正本见 `实验/additive-sky-seamless/results/`。

### 17.5 复现

```
python3 eng/tools/e2e/seam_footprint.py --self-test                                              # 判据门自检
python3 实验/additive-sky-seamless/code/audit_rework/route1/c3_seam_gate.py                     # 确定性下限 1.0050%
python3 实验/additive-sky-seamless/code/audit_rework/route2/e3_seam_gate_1e-2.py                # 门值合成复算
python3 实验/additive-sky-seamless/code/audit_rework/route3/exp01_seam_gate.py                  # 零假设与检出率
python3 实验/additive-sky-seamless/code/seam_gate_gradient_scan.py                              # 梯度项伪阳（§17.3 第 6 条）
```

读数、逐条证据与推导说明正本 = `实验/additive-sky-seamless/results/audit_rework/` 与
`实验/additive-sky-seamless/docs/seam-gate-floor.md`。
