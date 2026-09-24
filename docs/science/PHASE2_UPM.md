# Phase2 UPM (Control Photometry) Science (SCI-UPM)

> 上游：ASTROCS_DESIGN.md §5.4（天光平面与统一相对模型）

> ID: SCI-UPM-001  范围: SCI-UPM-001..010 + SCI-UPM-WEIGHT-001 + SCI-UPM-PERSIST-001  状态: FROZEN (T106 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-UPM-001..  模块: phase2 (upm/sampler)

## 1 目的与非目标

- **目的**：在多帧覆盖并集上建立唯一的联合加性光度模型 UPM，消除逐帧背景/零点差，使校准后样本 `calibrated = raw − C_f(p)` 在全域可比；提供控制权重与持久化 frame_id 绑定。
- **非目标**：不处理乘性尺度差（**本期决议：纯加性模型**，`g_k ≡ 1` 不启用，见 §14a）；不做 Drizzle 方差估计（SCI-NOISE）；不做最终加权积分与排异判定（SCI-INT/SCI-REJ）；不跨滤镜统一（filter 分组由调用方保证）。

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
| `control_reliability`（旧名 `geometric_reliability`） | per-control 相对可靠度。**实现事实 = 配置常量**（`P2UpmBuildConfig.control_reliability`，默认 1.0，`upm.cpp:289-290`），**不是**按单 control 覆盖度算出的几何量 | `upm.cpp:264`（默认 1.0）/ `:283`（越域回退 1.0） |
| `k_corr` | Drizzle 相关校正；定义域 **1 < k_corr**（`k_corr = 1` ⇔ 忽略相关，显式拒），冻结默认 1.4 | `sampler.cpp:83`（默认）/ `:874-875`（取值与消费） |
| `w_cell` | 求解器内 per-control 份额权重（无量纲，`Σ_cell w_cell = control_reliability`） | `upm.cpp:565` |
| `N_retained` | clipping 后保留样本数 | `P2ControlObservation` |
| `parameter_rows[index]` | 第 index 帧的 θ 行 | `upm.cpp:parameter_rows` |
| `frame_id_by_index[index]` | 第 index 帧的稳定 id | `frame_id_by_index` |

## 3 物理量和单位

- `C, raw, calibrated, σ_bg`: **面亮度 ADU·sr⁻¹**（与上游 Phase1 HiPS signal 层同标度，量纲依据=写盘 BUNIT 冻结集 {ADU/sr, ADU^2/sr^2, sr^2/ADU^2}，裸 ADU 判红：`lib/infrastructure/aio/src/hiss_writer.cpp:335-365`；实测闭环见 ALG-P2-SMP-001 §2）；
  `control_variance`: **(ADU·sr⁻¹)²**；`control_ivar`: **(ADU·sr⁻¹)⁻²**；
  `quality, control_reliability`: 无量纲 [0,1]；
  `w_UPM`: **(ADU·sr⁻¹)⁻²（绝对逆方差量）**——注意其量纲随上游 signal 标度而变，**不是**与仪器无关的常数；跨标度（如与 ADU 域孔径测光量）混用前必须先声明 Ω_px 换算；
  `w_cell`: **无量纲（份额式，单位元 = 无量纲）**；`N_retained`: 无量纲；`frame_id`: 无量纲 uint64；`θ`: 面亮度 ADU·sr⁻¹。

## 4 输入有效域

- 帧数 `n_frames ≥2` 且至少一 control cell 有 `≥2` 帧 clean 覆盖，否则 harmonic continuation 填单帧区；`n_control_points` 可为 0（→ NO_DATA）。
- `control_ivar` 有效要求 `use_ivar_weight=1` 时 `control_ivar>0` 且有限，否则 `p2_upm_raw_weight rc=2 → build rc=2`（`DATA-UPM-CONTROL-UNC-001`）。
- `k_corr` 为 `frames[f].kcorr>0 ? per-frame : cfg.control_k_corr`，缺省 1.4（`sampler.cpp:874`）；
  **定义域 1 < k_corr**：`p2_upm_control_variance`（`upm.cpp:2766-2790`）对 k_corr<1 返回 rc=1、
  对 k_corr=1 返回 rc=2、对非冻结值缺 `calibration_run_id` 返回 rc=3、缺 `applicability_domain` 返回 rc=4；
  `p2_upm_ma_build`（`upm.cpp:2265-2276`）对上述四类越域一律返回 rc=7。
  物理依据：k_corr 表征 Drizzle 输出像素协方差使 `N_eff ≤ N_retained`；k_corr<1 ⇔ N_eff>N_retained，
  正相关样本的有效样本量不可能大于样本数；k_corr=1 ⇔ 忽略相关。**两者都显式拒，不静默饱和**。
  边界已有判别力测试：`eng/tests/unit/v6_p2_upm/v6_p2_upm_ma_test.cpp`（0.999999→rc=1、1.0000001→rc=0）。
  **适用域（正向约束）**：k_corr 只在**其标定域内**有实证意义。生产取值 1.4 的来源是
  项目自产 MC（pixfrac=0.8、2000 次、实证 1.3883），该证据源 `control_median_mc_test`
  **未注册（构建孤儿）⇒ 不可复跑**；逐帧标定表的适用域为源像素角尺度 [300,600]″/px，
  而全部已知生产帧的源像素角尺度 ≈1″/px（M42 真帧 0.98902″/px，由 FITS 头
  FOCALLEN/XPIXSZ 换算）⇒ **逐帧标定在生产尺度上不生效，生产实际使用未在本尺度标定的
  冻结常数 1.4**。写盘门（FZ-PROV-KCORR，
  `upm.cpp:2265-2275`）要求 `k_corr_applicability_domain` 非空，且 k_corr ≠ 1.4 时
  另给 `k_corr_calibration_run_id`；任何引用 `control_variance` 的陈述必须与所用 k_corr、
  其标定域、以及「该域是否覆盖当前数据尺度」一同给出。
- 标定表 scale 维（300–600″/px）**不适用于源像素角尺度 <300″/px 的帧**：
  生产真帧源像素角尺度 ≈0.99″/px 在域外，域外一律显式回退冻结默认 1.4
  （禁 clamp 静默饱和）；域外帧保留 `kcorr=0` 并打 `[sampler] k_corr 域外回退` 标
  （`sampler.cpp:93-104` 查表域界 / `:560-583` 回退与打标）。
- `parameter_rows` 与 `frame_id_by_index` 同长、无重复，绑定仅由稳定 `frame_id` 决定，禁止容器遍历重建。

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
  #   成立条件 = iid ∧ 分布近似高斯 ∧ N ≥ 65（N=5 时渐近式低估 8.5%）；
  #   非高斯域不成立（均匀 1.91×、拉普拉斯 0.335×，实测见 ALG-P2-SMP-001 §5.4）。
  #   出处：Serfling 1980 §2.3.2（ISBN 0-471-02403-1 / DOI 10.1002/9780470316481）。
  # σ_bg = patch 内样本稳健尺度（含结构分量）；背景主导 patch 才等于噪声尺度
  #   （实测 3.78% 的 M42 真帧 patch 结构抬升 >2×，ALG-P2-SMP-001 §5.4）。
  # σ_bg_raw = 0（patch 内 ≥ 半数像素同值）⇒ 无尺度信息：control_ivar 必须为 0，
  #   禁止以数值保护量生成有限方差发布（ALG-P2-SMP-001 §5.4）。
  # control estimator = patch median (非单 leaf)
  # N_retained = clipping 后保留数 (非 n_total)；域 [min_samples, (2r+1)²] = [5, 289]
  # k_corr = 1.4 保守冻结；**1 < k_corr**（k_corr=1 与 k_corr<1 一律显式拒，§4）；实证 1.3883 来自
  #   control_median_mc_test（**未注册：构建孤儿，见 §11/§13/§15 的 MISSING 登记**）
  # N_eff = N_retained / k_corr
  # 禁 production 乘 star SNR / snr²/(1+snr²) / support^p；support 仅 eligibility/coverage
  # legacy snr²/(1+snr²)/unc² 仅 use_ivar_weight=0 ablation/诊断 (SNR-015)

求解 (UPM_SOLVER.md):
  Huber IRLS + control-ivar 感知权重 + 弱零锚 (zero_anchor_weight=0.001) + 连通分量独立 gauge
  每分量参考帧 = 最小 frame_id

收敛与容差 (SCI-UPM-CONV-001；容差随观测尺度归一，禁绝对容差):
  配置面（唯一事实源 = P2UpmBuildConfig，upm.h:77-81/:110-113）:
    tolerance（默认 1e-6，标量，同时作步长判据的容差）
    tolerance_relative（默认 0；1 = tolerance 解释为相对量）
  步长判据（converged=1 的唯一判据，upm.cpp:1021-1040）:
    tolerance_relative=0: max_dM < tolerance ∧ max_dC < tolerance
    tolerance_relative=1: max_dM < tolerance·max(scale_obs,1) ∧ max_dC < tolerance·max(scale_obs,1)
  目标判据（converged=2 的唯一判据，upm.cpp:1028-1050）:
    |Δobj| / max(|obj_old|, 1e-300) < 1e-12 连续 5 次 ⇒ stalled
  scale_obs = 观测量（control 观测值）的稳健尺度，upm.cpp:706-716；**不得**用 max|M|
  converged 状态枚举: 0 = max_iter / 1 = converged / 2 = stalled / 3 = invalid
    （只读访问器 p2_upm_convergence，upm.cpp:1577-1589；旧模型文件未记录一律读作 0）
  **适用域（正向约束）**：绝对容差只在观测尺度 ≈1 时与相对判据等价。生产观测为面亮度
  ADU·sr⁻¹（M42 真帧天空 1210 ADU/px ÷ Ω_px 2.2991e-11 sr ⇒ ≈5.26e13 ADU·sr⁻¹），
  绝对 1e-6 比该尺度上的 ULP 严若干数量级、原理上不可达 ⇒ **生产必须 tolerance_relative=1**。
  近零尺度侧由 max(scale_obs,1) 保留绝对保护（小尺度合成数据与绝对判据逐位等价）。

持久化绑定 (SCI-UPM-PERSIST-001 / ALG-UPM-FRAME-BIND-001):
  parameter_rows[index] ↔ frame_id_by_index[index]   # 同长、无重复
  绑定仅由稳定 frame_id 决定；save→close→open 保持 frame_id→θ 映射
  payload = truncated-64 canonical SHA-256 of science payload (DATA-FRAME-ID-001)
```

与 `lib/algorithms/coverage/src/upm.cpp:1-27`（冻结头注释）、`:242-1203`（build 主体）、`:1155-1175`（model_hash 序列化）、`lib/algorithms/coverage/src/sampler.cpp:257-371`（配置默认与校验）、`:689-900`（control 采样与 cvar 发布）、`lib/infrastructure/aio/src/aio_upm.cpp`（稀疏/稠密落盘）一致。

## 6 假设

- 帧间无乘性尺度差（**本期决议：纯加性模型**，`g_k ≡ 1`；乘性残留属低阶空间增益、归 Phase1，见 §14a）；控制点 SNR 与几何解耦（`snr_available` 语义 V4 R6）；控制采样 patch 足域近似高斯；Drizzle 相关可用 `k_corr>1` 表征（`k_corr=1` 表示忽略相关，§4 显式拒）。

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
  时改变估计量 ⇒ 未同步换算 λs/λ0 不得改用绝对权（§10）。
- **并行确定性容差（三档，冻结）**：(a) 同配置重复构建 = **位精确** + `model_hash` 逐字相同
  （构造保证：连续块划分 + 每 k/每帧不相交写 + 无共享浮点累加器）；(b) **跨 worker 数
  （1..N）= 相对容差 rtol 1e-12**（判据 `|ΔC| ≤ 1e-12·max(|C|, C_scale)`，
  `C_scale` = 该次构建 C 场量级），不是位精确 —— `compute_raw` 的 per-control 求和按 worker
  连续切片分块后按 worker 序合并，与串行索引序的结合顺序不同（FP 加法非结合），
  误差量级由 FP 加法非结合性决定，**与绝对标度成正比**；
  实测 ΔC_max = 2.22e-15（该算例 C 量级 ≈10，即 ≈1 ulp）；
  (c) 跨后端等价 = **不允许**（无此合同）。
  **适用域（正向约束）**：判据必须随标度归一。绝对量 1e-12 只在 `|C| ≈ 1` 时与 rtol 1e-12
  等价；生产 C 场为面亮度 ADU·sr⁻¹，量级由 Ω_px 决定（M42 真帧实测天空 ≈1.2e3 ADU/px，
  `实验/additive-sky-seamless/results/c7_realdata.json` 的 `pair_mismatch[*].level_i`；
  像素角尺度 0.98902″/px、Ω_px = 2.2991e-11 sr 由 FITS 头 FOCALLEN/XPIXSZ 换算 ⇒ 5.3e13 ADU·sr⁻¹），
  此时 ulp 量级为 1e-2，
  绝对 1e-12 比 ulp 严 10 个数量级、不可达；反之在 α² 标度（≈1e-29）上绝对 1e-12
  又比 ulp 松 14 个数量级、恒真。**禁止**在未声明 C 场标度的情况下引用绝对 1e-12。

## 7a 表示能力边界与近奇异处置（SCI-UPM-CAP-001 / SCI-UPM-KAPPA-001）

**表示能力边界（无接缝 ⟺ 公共面可表示）**：

- 参考面 `B_ref`（8×8 control cell 双线性）只能表示尺度 ≳ 2× **节点间距**的分量；节点间距 `h` 由输入几何导出
  （规则 2），**不是** tile_width/8。该实验单元取 `h = 0.0355°`（= 2× cell；见 `实验/additive-sky-seamless/code/c3_public_plane.py:26-28`
  的 `NODE_SPACING_DEG`）作为其自身网格；生产取值必须按规则 2 逐产品导出；
  **量纲与适用域（正向约束）**：`h` 的**定义域量是角尺度（度）**。换算 `h_px = h_deg × 3600 / 源像素角尺度(″/px)`：
  实验单元自身用 1″/px 网格 ⇒ `h = 127.8 px`；本仓 M42 真帧实测源像素角尺度
  0.98902″/px（FITS 头 FOCALLEN 1877 mm、XPIXSZ 9.0 µm ⇒ 206264.806×0.009/1877 = 0.98902″/px，
  复算脚本见 `实验/additive-sky-seamless/code/c7_realdata.py`）⇒ `h = 129.2 px`、`2h = 0.0710° = 258.5 px`。
  **换仪器/换像素尺度后 `h` 的度数不变、像素数按比例变**，故一切「尺度 ≲ 256 px」类的
  像素域判据必须随像素尺度重算。
- 当帧间天光差含「`B_ref` **不可表示**且沿向**相干**」的分量时，残余接缝 ≈ **0.80 × 该分量 RMS**（Pearson 0.896，`实验/additive-sky-seamless/results/c2_multiplicative.json`）；
- 空间尺度 ≲ 2× 节点间距（≈256 px）时显著：尺度 1600→50 px 扫描使残余接缝 **×5.07**；
- **工程要求（规则 1）**：节点间距须 ≤ 目标可表示尺度的 1/2（上界 = 约束帧间改正量的最小尺度 / 2）；**不得**用强平滑掩盖不可表示分量；
- 该边界是**条件结论**：在可表示域内接缝压缩 12.5× 成立，域外不成立；报告与产品必须与边界一同引用（§16.2）。
- **规则 2｜节点间距必须由输入几何导出（不得是标定常数）**：节点间距是**表示能力的唯一决定量**（规则 1），
  故它必须由输入几何**导出**，而不是取一个固定值。导出规则：**节点间距 ≤ (该产品实际约束帧间改正量的最小尺度) / 2**
  ——对「帧间加性天光差」这一目标量，该尺度是**重叠带宽度**与**指向间距**两者中的较小者（它们才是真正约束 `δ_k` 的量），
  由 manifest/输入几何逐产品算出。**禁止**在配置里留一个「默认节点间距」标定值；几何量缺失 ⇒ 显式失败，不回退常数。
- **规则 3｜自适应回路的唯一旋钮是节点间距**：节点间距**必须**进入自适应重试回路，且该回路**只有**这一条自适应方向——
  判据判红（`r_eff < n_free`：网格细过数据能约束的极限）⇒ 在规则区间内**放粗**；判据判绿且残差仍受表示能力限制
  （细化到 `h/2` 后 `χ²_red` 至少降到 `residual_improve_ratio` 倍）⇒ **细化**。两个方向由**同一条判据**（规则 4）决定，
  **不构成两条路径**。搜索区间**必须**由输入几何给出：上界 = 规则 1 的「约束尺度 / 2」，下界 = 数据自身分辨率极限
  `max(采样点角间距, 像素角尺度)`；上界 < 下界 ⇒ 无可采纳节点间距，显式失败。逐次尝试的
  `(h, λ_eff, κ(H_red), κ(H_solve), r_eff, action, adopted)` 与生效值**必须**全部写入 provenance。
- **规则 4｜可辨识性/病态判决只有一条（正向约束）**：判决**必须**落在**未正则化**的**列均衡数据信息矩阵**上：
  `H_eq = D⁻¹ H_red D⁻¹`，`D = diag(√H_ii)`（列均衡消除参数单位自由度）；记 `H_eq` 特征值 `λ_1 ≥ … ≥ λ_n`，则
  `r_eff = #{λ_i > τ·λ_1}`、`κ = λ_1/λ_n`，**判决位唯一**：`identifiable ⟺ r_eff == n_free ⟺ κ(H_red) < 1/τ`。
  「欠定」与「病态」是同一条不等式的两种读法（`λ_n` 最先跌破 `τ·λ_1`），**不得**保留两套并行判据。
  阈值**只有一个** `τ = rank_rtol`（**相对**口径，尺度不变），由浮点算术给出（精度地板 `max(m,n)·eps`），
  **不得**按数据集标定；**禁止**任何绝对条件数上限（`kappa_max` 类常数）。自由度**必须**用
  `dof = n_obs − r_eff`（Andrae, Schulze-Hartung & Melchior 2010, arXiv:1012.3754 式 (9)）；
  `χ²_red` 的分母**不得**用 `n_obs − n_params`（秩亏时后者系统性低估 `χ²_red` 并掩盖未被约束的方向数）。
- **规则 5｜不得在正则化矩阵上设门，自适应不得引入自由标定参数（正向约束）**：`H_solve = H_red + λ·P`（`P` 半正定）
  的条件数随 `λ` 增大有上界（`κ → 1`）⇒ 在 `H_solve` 上设门是**恒真门**，对「原问题是否可辨识」零信息；
  `κ(H_solve)` **只能**作为求解稳定性诊断量单独记账，**不得**进入判决。数值正则化**只能**取判据**派生**的量
  `λ_eff = τ·mean(diag(H_red))`（判据自身的分辨率下限，无自由参数），它**不参与判决**。**禁止**放宽 `τ`、
  **禁止**改回绝对上限求绿、**禁止**把粗糙度惩罚一类自由标定值当作自适应分支。自适应**必须真的会被触发**：
  门控若宽到让真实数据永远通过，则自适应等价于不存在；每条自适应路径都必须有「触发过」的记录，否则视为未实现；
  表示能力到顶（在分辨率下界上残差仍不下降）**必须**如实登记，**不得**声称已收敛。
- **不收敛时报警告不阻塞（正向约束）**：迭代未达容差（`converged != 1`）或判据判红而该路径按**报告口径**处理时，
  **必须**产出**产品级机器可检警告**（具名 `warning_codes` 随产品/manifest 落盘，如 `P2-UPM-NOT-CONVERGED`、
  `P2-UPM-NOT-IDENTIFIABLE`），**不得**改变构建 rc、**不得**静默；判红在 fail-closed 路径上（GLS/MA 求解器、
  天光面求解器）按其求解器返回码显式失败。

**近奇异（条件数）处置**：

- **判据读数必须可观测并写入 provenance（正向约束）**：列均衡数据信息矩阵的 `r_eff` 与 `κ(H_red)`、唯一阈值 `τ = rank_rtol`
  及其**生效值**（`rank_rtol_effective = max(τ, max(m,n)·eps)`）、未被约束方向数 `n_unidentified`，以及**求解稳定性诊断量**
  `κ(H_solve)` 与派生数值岭 `λ_eff`。**引用任何 κ 的陈述必须写明矩阵口径（`H_red` 还是 `H_solve`）与所用 `τ`；不同口径的 κ 不可比。**
  真实 M42 样本的实测判据读数：`rank = n_nodes = 49`、`n_params = 58`、`χ²_red = 1.004`、`κ ≈ 3.2×10⁷`（读数与矩阵口径一同引用）。
- **判红处置（正向约束）**：`r_eff < n_free`（等价 `κ(H_red) > 1/τ`）⇒ **必须**走规则 3 的节点间距自适应，并登记所走方向、
  尝试次数、生效参数与最终判据读数；**禁止**静默产出欠定解，**禁止**放宽 `τ` 求绿，**禁止**改用绝对条件数上限。
- **`H_solve` 的条件数不得进入判决（正向约束）**：`H_solve = H_red + λ·P`（`P` 半正定）的条件数随 `λ` 增大有上界
  （`λ → ∞ ⇒ κ → 1`）⇒ 在它上面设门是**恒真门**，对「原问题是否可辨识」零信息（Hansen《Regularization Tools》v4.1 手册 §1/§2.7.3：
  用 `A` 导出的良态矩阵替换 `A` 不保证得到有用的解，正则化刻画的是一个**新问题**）。`κ(H_solve)` 只作
  「求解稳定性」诊断量单独记账（`P2SkyPlaneInfo.kappa_solve` / JSON `kappa_solve`）；`λ = 0` 时两读数逐位相等，这是回归锁，不是判决依据。
- **自由度（正向约束）**：`χ²_red` 的分母**必须**是 `dof = n_obs − r_eff`（Andrae et al. 2010 式 (9)），**不得**用 `n_obs − n_params`。
- **provenance 最小集**：`gauge_mode`、每分量 `ref_frame_id`、`rank`（`r_eff`）、`rank_rtol`、`rank_rtol_effective`、
  `n_unidentified`、`kappa`（`H_red`）、`kappa_solve`（`H_solve`）、`lambda_numerical`、`node_spacing_deg` 与自适应轨迹、
  `k_corr`、`model_hash`、`any_fail_closed_reason`。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 无重叠/无 control（`n_obs=0` / 无观测几何节点） | `rc=1`（`n_obs=0`）；无观测几何节点以 sentinel 不入数据图 | `upm.cpp:215`/`:246-249`；ALG-P2-UPM-IMPL-001 §10 |
| `use_ivar_weight=1` 且 `control_ivar≤0/非有限` | `p2_upm_raw_weight rc=2 → build rc=2` | `DATA-UPM-CONTROL-UNC-001`；`upm.cpp:1665-1670` |
| `S=0`（patch 内 ≥ 半数像素同值 ⇒ 稳健尺度 0） | **无尺度信息**：`control_ivar` 必须为 0 且 `control_variance` 标为无尺度信息（非有限），**禁止**把数值保护量平方后当方差发布 | ALG-P2-SMP-001 §5.4（发布口径权威）；`sampler.cpp:864-877` 为待整改点 |
| 单帧区 | harmonic continuation 填 | `upm.cpp:1054-1090`；`upm.h:99-101` |
| 畸形模型文件 | `ERR-P2-UPM-001`（open 强校验，任何损坏 rc=1 稳定错误） | `aio_upm.cpp`；`upm.cpp:1304-1567` |
| 跨 endian payload | 理论 id 不同，当前仅 Linux x86_64 路径 | `sampler.cpp:257` |

## 9 精度策略

- FP64 求解；`dense cache` 与 `sparse` 求值 `1e-12` 等价门（`SparseEqualsDense`）；
  `k_corr` 冻结默认 `1.4`（保守取整）；其 MC 校准来源 `control_median_mc_test` **未注册
  （构建孤儿 / MISSING，见 §11/§13/§15）**——常数本身由 §5 定义冻结，但不可复跑该 MC 证据。
  **适用域（正向约束）**：该 1e-12 是**同标度下**的求值等价门；跨标度引用绝对 1e-12 的
  禁令见 §7 (b) 的适用域段。
- 并行确定性容差见 §7 三档（同配置重复位精确 + hash exact；跨 worker 数 1e-12；跨后端不允许）。

## 10 不可接受变化

- 在生产 `w_UPM` 中乘 `star SNR / support^p / obs.ivar`(已弃用)；
- 将 `control_estimator` 改为单 leaf 或 `N_total` 替代 `N_retained`；
- 改变 `k_corr` 默认 1.4 或 `parameter_rows ↔ frame_id` 绑定语义；
- 在 `calibrate_block` 中引入乘性尺度；
- 在未同步换算 λs/λ0 的前提下把求解器内的 `w_cell`（份额式）替换为 `w_UPM`（绝对式）——
  份额式丢弃跨 control 精度，换绝对权等于换估计量（隐式改科学），须先重标定并登记。

## 11 验证 Oracle

- **UPMW 硬门 7 项**：001 snr 扰动不变、002 ivar 1:4→weight 1:4、003 星群不变、004 `Var(median)≈πσ²/2N`、005 MC `k_corr≈1.3883`（**未注册：`control_median_mc_test` 是构建孤儿 MISSING，不可复跑**）、006 无 legacy SNR consumer 且缺 ivar `rc=2`、007 patch 真值恢复（`synthetic_gate`）。
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
  `control_median_mc_test.cpp` **MISSING（未注册：不在任何 CMake/ctest/CI 面，构建孤儿）**

## 3a 坐标 frame

UPM 在**像素域 control cell**（8×8 双线性网格）上工作，无 WCS/天球参与；帧绑定唯一由稳定 `frame_id`（truncated-64 SHA-256，DATA_SEMANTICS §5）决定，容器索引仅实现细节；每连通分量参考帧=最小 `frame_id`（gauge 锚，§5）。

## 9a 专属问题回答（SCI-005 指定问题逐项）

- **观测方程**：`calibrated_f(p) = raw_f(p) − C_f(p)`，`C_f(p)`=帧 f 的加性校正场（8×8 control cell 双线性插值，§5）；**纯加性模型**（`g_k ≡ 1`；乘性尺度差不属本层，§1 非目标）；`raw`=校准前样本 patch median。
- **控制点**：8×8 control cell；control estimator=patch median（非单 leaf）；`N_retained`=clipping 后保留样本数（非总数）。
- **光度面 basis**：分块常数加性背景面——每帧每 control cell 一个自由度 `θ_f`，经双线性插值成连续场；自由度 = n_frames × n_control_points。
- **正则化**：Huber IRLS 鲁棒求解 + control-ivar 感知权重 + **弱零锚** `zero_anchor_weight=0.001`（弱 Tikhonov/岭型锚定向全局零）。
- **gauge/退化**：加性场对全局常数规范自由——以连通分量独立 gauge 固定（参考帧=最小 frame_id）；退化路径：无 ≥2 帧 clean 覆盖 → harmonic continuation 填单帧区；`control_ivar≤0/非有限` → `rc=2` 显式拒（§4）。
- **接缝指标（正向约束）**：接缝 = **帧足迹边界两侧的电平有符号台阶**——沿边界法向取 `±d` 差分（`d` = 采样半距，默认 2 px），
  `rel_step(e) = median(img[+d] − img[−d]) / bg(e)`，`bg(e)` 为该边界 `±d` 采样上的**局部**背景电平。判据量是**相对**口径（无量纲、可跨产品比），
  门 `max_e |rel_step(e)| ≤ 1e-2`（依据 `ACCEPTANCE_SPEC.md` §6.2「帧间、块间无亮度/灰度阶跃」与 `ASTROCS_DESIGN.md` §2.3「接缝 = 帧集变化处的亮度阶跃」）。
  **适用域（前置约束，不是放松阈值）**：只有**两侧都在数据内部**的帧边界计入判据——要求法向 `±ctrl_shift` 处可放置平行对照线，且两侧各有 `≥ min_samples` 个有效样本；
  一侧无数据 = 画幅/足迹**外缘**，那里的台阶是数据边缘本身，不是帧间接缝。被排除的边界仍逐条落盘（`exclude` 字段 + 全部度量），**不得**静默丢弃。
  **禁止**用方差比（接缝处方差 / 块内方差）判电平接缝：电平阶跃不改变方差 ⇒ 方差比对电平接缝**原理性失明**，它只能作渲染分块伪影的粗筛。
  判据必须**能红能绿**：注入已知台阶（`amp ≠ 0`）必须判红、`amp = 0` 必须回落到基线；无法判定（输入/依赖不可用、有效边界数为 0）**不得**当作"无接缝"（fail-closed）。

## 14 Primary literature（引用定位声明）

1. **本合同为项目原创推导**（观测方程/光度面 basis/gauge/接缝语义均为 Project-defined，无外部公式依赖）。
2. Huber IRLS：Huber, P. J. 1964, "Robust Estimation of a Location Parameter", Ann. Math. Statist. 35, 73——文章级定位（bibcode 1964AnMS...35...73H，未逐页核验），仅 robust 求解框架上下文。
3. 弱零锚=弱 Tikhonov 正则：Tikhonov 解的正则化概念——教科书级，无公式引用。
4. `k_corr=1.4`（MC 实测 1.3883，pixfrac=0.8，2000 次）：**项目自产 MC 证据**（`control_median_mc_test`，**未注册/MISSING：构建孤儿，本证据当前不可复跑**），非外部文献。
5. Tukey/MAD 常数：复用 SCI-002/SCI-003 文献链（PMC6768164 实证；Φ⁻¹(3/4) 恒等式）。

## 14a 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §5 公式、§7 不变量与 §10 禁改清单。

- **多帧相对光度/天体联合定标**：SCAMP（GPL-3.0，https://github.com/astromatic/scamp；Bertin, E. 2006, ASP Conf. Ser. 351, 112，标题经 aspbooks.org 逐字核验 2026-09-17）；Padmanabhan, N. et al. 2008, ApJ 674, 1217（SDSS 重叠观测联合相对定标、gauge/连通性）。
- **马赛克逐帧背景扣除/coadd 权重**：SWarp（GPL-3.0，https://github.com/astromatic/swarp；Bertin, E. et al. 2002, ASP Conf. Ser. 281, 228）；Gruen et al. 2014, PASP 126, 158。
- **Huber IRLS**：Huber, P. J. 1964, Ann. Math. Statist. 35, 73（DOI 10.1214/aoms/1177703732）；Holland, P. W. & Welsch, R. E. 1977, Communications in Statistics A6, 813（DOI 10.1080/03610927708827533，δ=1.345 的 IRLS 出处）；Huber & Ronchetti 2009, Robust Statistics, 2nd ed., Wiley（ISBN 978-0-470-12990-6）。
- **弱零锚（弱 Tikhonov/岭正则）**：Tikhonov, A. N. 1963, Soviet Math. Dokl. 4, 1035（**核验状态**：文章级，卷页需网络核验）。
- **var(median)≈πσ²/(2N)**：正态样本中位数渐近方差的教科书结论（Hoaglin et al. 1983；Kendall & Stuart, The Advanced Theory of Statistics Vol.1）；UPMW-004 实证 ratio 0.997。
- **稀疏天光面样条（加性天光面的表示候选）**：Duchon, J. 1977, Constructive Theory of Functions of Several Variables, 85（薄板样条）；Wahba, G. 1990, Spline Models for Observational Data, SIAM（ISBN 0-89871-244-0）。§5 冻结的加性场当前实现为 8×8 control cell 双线性；稀疏样条是**同一加性场**的另一种表示候选（数据面/schema 待合同流程），**不改变** §1/§5 的纯加性模型。
- **模型形式：纯加性（正向约束）**：本文件 §1/§5 的加性模型 `calibrated_f(p)=raw_f(p)−C_f(p)` 是 Phase2 的唯一模型形式，`g_k ≡ 1` **不启用**；乘性方向 `y_k(x)=g_k·s(x)+b_k(x)` **不在本层**。`÷g²` 保持**恒等式**（`Var(corrected)=[σ_raw²+J_out C_θ J_outᵀ]/g²`，`g≡1` 时退化等价，公式保留）。**依据**：Phase1 正确归一化后，帧本身已是同一测光体系的真信号加可等效为加性的天光；残留天光无论是加性还是乘性都可用加法移除；乘性残留属低阶空间增益，归 Phase1 处理（`I_photo = k_photo·m(x,y)·I_cal`），Phase2 只做加性扣除。`10_sampling.md`/`11_upm.md`/`UNIFIED_MODEL.md` 的目标态表述与本节一致。
- **k_corr=1.4 的 MC 证据**：control_median_mc_test 未注册（MISSING，构建孤儿），常数本身按 §5/§10 冻结但当前不可复跑（§9/§11/§13/§15）。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- WCSLIB（LGPL-3.0）/ CFITSIO（宽松许可，NASA/HEASARC）：WCS 与 FITS 独立读取器。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

## 15 Acceptance

- §11 Oracle 全过（含 MC 一致性、gauge 唯一性、harmonic continuation 边界）；**例外**：
  UPMW-005 的 MC 证据源 `control_median_mc_test` **未注册（MISSING，构建孤儿）** ⇒ 该项
  只有常数冻结定义（§5/§10），没有可执行证据；收进 ctest 前不得称"已过"；
- §7 不变量门全过；
- `eng/tools/science_contract_lint.py` PASS（15 节+claim ID+锚点）；
- 解析不变量→SYN-005 转换：已知低阶光度面恢复、重叠图 gauge/退化强度扫描、接缝指标按 §9a 的**有符号台阶**口径（门 `max|rel_step| ≤ 1e-2`、适用域与 fail-closed 同 §9a），参数恢复/残差/接缝降低且不破坏星 flux 全过。

## 16 SCI-C 实验结论（RELEASE-04 / SCI-403，仓内实测）

实验单元：`实验/additive-sky-seamless/`（报告 `README.md`，机器可读结果 `results/*.json`，
一键复跑 `code/run_all.sh`）。**本节只记录已定稿的实测结论，不改变任何公式或默认容差。**

### 16.1 已证实的结论（data 级，生产代码实测）

1. **多退少补正确**：产品 `calibrated_k = raw_k − δ_k`（**保留 B_ref**）在覆盖子集突变处把
   背景电平接缝从 4.80 e⁻ 压到 **0.383 e⁻（12.5×）**；产品中位 299.17 e⁻ ≈ B_ref 297.33 e⁻。
   全减背景臂（`raw − b_k`）产品中位 0.845 e⁻ ⇒ **退化，不得用作无接缝证据**。
2. **B_ref 是规范零点**：`b_k − δ_k` 与帧无关（5.7e-14）；gauge 0↔1 只造成 δ_k 的逐帧常数
   偏移（5.3e-15），接缝度量不变（差 0.0）。
3. **权重**：`control_ivar` 的伪影漏入 0.0245 e⁻，比 uniform（0.0651）小 2.7×、比 SNR²（0.3203）
   小 13×；噪声 RMS 亦为三臂最小（1.532 < 1.674 < 2.069 e⁻）。与 SCI-402 的独立结论一致。
4. **稀疏现场求值**：dense cache 与 sparse `calibrate_block` 在 1,048,576 点上 max|Δ| = 3.1e-15；
   稀疏模型 14,001 B vs 稠密 8,389,129 B（0.167%）；按需求值 64² 块峰值 RSS 低于稠密物化 512²。

### 16.2 适用域边界（**新增，必须与结论一同引用**）

1. **无接缝 ⟺ 公共面可表示**。当帧间天光差含「B_ref 不可表示且沿 y 相干」的分量时，
   残余接缝 ≈ 0.80 × 该分量 RMS（Pearson 0.896）；空间尺度 ≲2× 节点间距（≈256 px）时显著
   （1600→50 px 扫描使残余接缝 ×5.07）。
2. **纯加性前提**：帧间乘性差必须先在 Phase1 吸收（实测 5.89e-4）。未做 Phase1 归一时
   （历史 `photometry_applied=false`/`photscal=1.0`，1.56× 帧差），纯加性 UPM 后接缝 27.37 e⁻，
   做了 Phase1 后 6.31 e⁻（**4.33×**）。基外高频乘性分量（1%@24 px）对**电平**接缝贡献有界
   （<50% 基线），但会被分块 PSD 检出（实测峰 k=26 vs 预期 21.33；`results/c2_multiplicative.json:hf_component.psd_k_peak`）。
3. **不可检验域**：`smoothing_lambda=0` 时 per-(frame,cell) 自由加性场恰好定解，公共场 M 只是
   每 cell 的规范选择 ⇒「拟合/堆叠权重同源」与「末端残差场扣除」在该域内**不可检验**；
   `final_gauge` 在 `m_full_frame=1` 时近似 no-op（3.7e-3 e⁻），且**不能**修复子集依赖。

### 16.3 生产实现现状与待满足项（逐条可核验）

| 项 | 规范要求（§5/§7a） | 实现现状（实测锚） | 状态 |
|---|---|---|---|
| 收敛判据的尺度归一 | 生产必须走相对/尺度归一判据（绝对 1e-6 在生产尺度不可达） | `module_adapters.cpp:7808-7813` 取 `tolerance=1e-6` ∧ `tolerance_relative=1`（相对判据，分母 `max(scale_obs,1)`）；`p2_session.cpp:204` 取 `tolerance=1e-6` 而**未设** `tolerance_relative`（零初始化 ⇒ 0 ⇒ 绝对判据） | **两入口口径不一致**（待统一） |
| `converged` 状态枚举 0/1/2/3 | 只读访问器暴露 `0=max_iter/1=converged/2=stalled/3=invalid`，并写入数据面 | `upm.cpp:1028-1050`（stalled 连续 5 次低改善）、`:1577-1589`（访问器）；`module_adapters.cpp:7928-7935/:7963/:7997` 读入并落 manifest | **已满足** |
| 可辨识性判决 κ | 判决取**未正则化**的列均衡数据信息矩阵：`r_eff < n_free ⟺ κ(H_red) > 1/τ`；`κ(H_solve)` 只作求解稳定性诊断 | `sky_plane.cpp:1082-1135`（判据 + fail-closed）、`identifiability.h`（天光面与 UPM 共用同一函数）、`module_adapters.cpp:8807-8864`（节点间距自适应 + provenance） | **已满足** |
| κ 口径 | **唯一阈值** `τ = rank_rtol`（相对口径、尺度不变、精度地板 `max(m,n)·eps`）；**不存在**绝对条件数上限 | `sky_plane.h:217-222`、`upm.h:293-295`（同一符号/同一值/同一实现） | **已统一** |
| 自适应重试的授权边界 | 放宽判据阈值 `τ` 求绿为禁止项；唯一自适应方向 = 节点间距（§7a 规则 3/5） | `module_adapters.cpp:7797-7808` 注释要求收敛容差回退冻结值并称相对判据待裁决，而同段 `:7809-7813` 以「定案」名义启用相对判据——**同一函数内两段注释对同一变更的授权状态表述互斥** | **待裁决**（不掩盖） |

**正向约束**：上表「待统一/待裁决」项在收敛前，任何引用 `converged` 或 `tolerance_relative` 的陈述必须写明**所用入口**；引用 κ 的陈述必须写明**矩阵口径（`H_red`/`H_solve`）与所用 `τ`**（§7a）。
