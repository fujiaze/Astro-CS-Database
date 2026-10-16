# 第三轮修复与 SNR 科学复核（R2-F01..F04 / §11.6 / 审查执行结论第 1 项）

分支 `audit/acsd-science-docs-p5`；基线 `f41cc1c2b`（前台第二轮审查裁决后）。
本文记录本轮按裁决执行的处理、证据与两项需要前台注意的新科学事实。
科学正本未改；若前台采纳下面第 5 节的观察, 由前台更新正本后再动实现。

## 1. 依据（前台裁决逐条）

来源：`实验/normalize-precision-p5/ROUND2_INDEPENDENT_REVIEW_633FFC31.md`、
`docs/engineering/contracts/NORMALIZE_DATA.md` §7.1B、`docs/science/noise_snr/NOISE_SNR.md`
§11.6 及执行结论第 1 条（先做针对性 SNR 科学复核, P4 结构性迁移可并行）。

| 条目 | 处置 |
|---|---|
| R2-F01 逐点 PSF 诊断 | normalize_precision.cpp 每控制点写自己的 f_enclosed/f_out/求积误差; 帧级 extrema 单列摘要 |
| R2-F02 v1/v2 版本混写 | covariance payload 的 support_note 改为真实 local-factor/v2 |
| R2-F03 线性单位 | reader 增加 base linear unit 与帧 signal 单位核对 + 三档负例 |
| R2-F04 条件数估计 | 新增 condition_estimate_method 字段随值落盘（pivot_norm_ratio_squared / retained_eigenvalue_ratio） |
| §11.6 数值稳定性 | I_F 改显式正交残差（两轮重投影）, 记录 ‖t‖/‖e‖; 禁用 ‖t‖²−‖Π_Zt‖² 相减 |
| verify_round3 假测试块 | 201×201/61×61 全平面积分**实调生产接口**并加断言（包络界 + 单调收敛） |

## 2. 代码修改点（本次提交, 未动 main）

- `lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp`
  - I_F: 显式残差 `e = t − Π_Zt`（2 轮重投影）后求 ‖e‖²（原 `t_norm2 - proj` 相减,
    近共线时相对误差实测可达 1.4e8, 见 §4.1）
  - 新增 `solver_condition_estimate_method` / `flux_response_norm` / `flux_residual_norm`
  - Jacobi 回退路径的方法名 retained_eigenvalue_ratio
- `lib/algorithms/noise_snr/include/acsd/sparse_reference.h`: 字段与 SolverDiagnostics
  口径注释（rank_tol 作用于 Q 的奇异值; 与 Jacobi 路径的 H 特征值尺度不可比）
- `lib/infrastructure/scheduler/src/normalize_precision.cpp`
  - 逐窗口保留 PixelIntegrationDiagnostics; support note 逐点 f_enclosed/f_out/
    pixel_integration_error（可机读字段同步逐点）
  - 帧级摘要 `psf_integration_summary`（min/mean/max + 最大求积误差）, 由
    module_adapters.cpp 打 stderr
  - covariance support_note: local-factor/v1 → v2, 并把三个单位声明写入 note
  - payload 透传 condition_estimate_method / flux_response_norm / flux_residual_norm
- `lib/algorithms/integration/normalize_science/{include/acsd/normalize_science.h,src/normalize_science.cpp}`
  - UnitPower 解析 base+指数; reconstruct_factor_covariance 增加 expected_linear_unit
    参数; §7.1B 三档拒绝（未知帧单位 / 因子线性单位错配 / 派生线性单位错配）
  - reader 解析并落盘 condition_estimate_method / flux_response_norm / flux_residual_norm
  - verify_frame_science_factor_models 传 doc.signal.linear_unit 并核对 linear_unit_checked
- `实验/normalize-precision-p5/code/verify_round2.cpp`: 新增用例 5b（近共线）
- `实验/normalize-precision-p5/code/verify_round3_psf_pixel_integration.cpp`: 块 (3) 实调
- `实验/normalize-precision-p5/code/verify_round4_factor_v2.cpp`: §7.1B 负例
- `实验/normalize-precision-p5/code/verify_round5_snr_science.cpp`: 新建（科学复核 1+2）
- `实验/normalize-precision-p5/code/verify_round5_sidecar_recompute.py`: 新建（科学复核 4）

## 3. 生产事实（真实重跑, run/norm-precision-test, exit=0）

```
$ ./build/linux-sci-p5/acsd normalize -y --json run/norm-precision-test.json
[drizzle_node][precision] psf_pixel_integration frame summary:
  n_controls=64 f_enclosed[min,mean,max]=[0.999889277,0.999896390,0.999904772]
  max_pixel_integration_error=0.000000
```

- 64/64 controls available（accuracy=approximate, omitted=[master_uncertainty]）
- ctrl0 `p0_r0`: V_F=5439.96408、SNR_ref=13.55819838、rank=627; 帧摘要 SNR=14.7088589524
  —— 与上一批 r8（V_F=5439.9640804091068, SNR=13.558198375223254, 帧摘要
  14.708858952409543）**逐位一致**：满列秩下显式残差与相减数学等价, 归一修复
  未改变科学数值（只改近共线稳健性与诊断字段）。
- models[]: 64 个 covariance 全为 `acsd.normalize.local-factor/v2`, 其余 192 个
  local-model/v1（与 r8 相同）。
- schema 一致性 `eng/tests/conformance/normalize_frame_science_schema_test.py`: 18/18。
- 单元层 `python3 -m eng.tests.unit.run_unit`: 159/160; 唯一红项
  `quantization.s12.statistic_vs_spec_threshold` 为本轮改动前既有红（RUN.md §5 已记录）。

## 4. 独立验证证据（五条路径, 全部 ALL PASS / exit 0）

日志在 `evidence/verify_round*.log`; 程序在 `code/`。

### 4.1 verify_round2（GLS 可辨识性 + 新增近共线用例 5b）

新用例构造 P = γ·1 + δ·u（γ=0.9, δ=1e-4, u∉span(1,x,y)）:

| 量 | 实测 |
|---|---|
| 闭式 I_F | 6.4640670652e-11 |
| 生产 I_F（显式残差） | 6.46406706515e-11（rel 7.9e-12） |
| 朴素 ‖t‖²−‖Π_Zt‖² 相减 | 0.0089539（rel err **1.385e8**） |
| ‖t‖ / ‖e‖ | 0.905 / 8.03e-6（诊断字段已落盘） |
| rank_tol=1e-12 / 1e-10 | available, I_F 逐位相同 |
| rank_tol=1e-8 | 具名 unavailable（I_F/‖t‖²=1.27e-9 < 1e-8; 不造 V_F） |

结论: 相减法的消去误差在近共线时把 I_F 抬高 8 个数量级（会被误判为"可辨识"）;
显式残差达到 QR 变换舍入底（~ε·‖t‖/‖e‖）。秩门限敏感性按 §11.6 如实登记:
该点条件 V_F≈1.5e10（SNR_ref≈8e-3）本已不可测, 容差语义随 solver 诊断落盘。

### 4.2 verify_round3（PSF 单位总通量像元积分）

第一轮被判定"只建点数组未调用"的 201×201 块现已实调:

| 窗 | f_enc | f_out | 圆盘包络(half+0.5) |
|---|---|---|---|
| 61×61 | 0.999995840983 | 4.159e-06 | 6.840e-06 |
| 201×201 | 0.999999996595960 | 3.404e-09 | 5.630e-09 |

f_enc ∈ [1−包络, 1]、f_out 单调收缩、求积阶自检误差 <1e-12、逐点响应 ∈[0,1]；
平移等变、相位敏感、Monte Carlo(4e6×5) 交叉验证均保持原状。

### 4.3 verify_round4（local-factor/v2 + §7.1B 线性单位）

新增三档负例（旧 reader 会全部放过）:
- factor=electron^2, derived=electron^2/sr^2（sr 幂次 0/-2 正确!）⇒ 拒绝
  "factor_variance_unit 'electron^2' is not adu^2 (base linear unit mismatch…)"
- 帧 signal 线性单位未知 ⇒ 拒绝（sr 指数正确不足纳）
- 原有: 单位幂次错配 / CSR 重复 (row,column) / v1 拒作 canonical
正向: write→load→reconstruct 往返, 独立第三条路径重算 C 逐位相同（max|diff|=0）。

### 4.4 verify_round5_snr_science（科学复核 1+2）

Part 1 独立像元求积（生产 GL order10 vs 复合辛普森 256 子格, 覆盖 σx≠σy、θ、
亚像元相位、欠采样 σ=1）: 逐点最大相对差 5.1e-12..1.5e-10（最坏为欠采样
各向异性; 辛普森自身离散化 ~5e-12, 128→256→512 收敛序列实测）。
Part 2 已知真值前向模拟（F_ref=1000 ADU, gain=1 e-/ADU, RN=9 e, sky=58 ADU/px,
均取自 run/norm-precision-test.json; σ=2.211089）：

| 场景 | 生产 V_F | 独立 nl 空间 [M⁻¹]_FF | MC 200k bias | MC sd/√V_F | 95% 覆盖 |
|---|---|---|---|---|---|
| R=I, 已知背景 | 4937.70 | 4937.70 (rel 3.7e-16) | −0.005σ | 0.99845±0.00016 | 0.9505 |
| R=I, 常量 nuisance | 6821.33 | 6821.33 (rel 0.0) | −0.005σ | 1.00032±0.00016 | 0.9501 |
| 稀疏 R（秩亏 41<81）, nuisance | 7377.90 | 7377.90 (rel 7.4e-16) | −0.004σ | 0.99984±0.00016 | 0.9499 |

物理世界（源泊松 = F·P/g, 总通量语义）与条件世界（生产冻结模型）的散度差
0.55%–0.72%, 与 1/a_ref−1=2.74% 的 Poisson 份额一致（见 §5.2）。
Part 3 冻结条件模型语义: F_true<F_ref 时条件 V_F 保守（0.71/0.85 比）;
F_true>F_ref 时低估（1.26 比）——条件 V_F 是 F=F_ref 处的量, 不是全域保证。

### 4.5 verify_round5_sidecar_recompute（科学复核 4, 真实产品）

从发布 sidecar 逐控制点用 **LAPACK SVD + 球面坐标白化**独立重算
（h 来自 psf payload, Z_s 与 v_j/CSR 来自 covariance/drizzle_operator payload,
与生产手写 Householder QR 是三套独立代码）:

- 64 ok / 0 failed; units（adu^2, adu^2/sr^2, sr^-1 vs signal adu）、
  representation=v2、condition_estimate_method、flux_residual_norm²==flux_information
  逐点通过
- 秩一致 63/64; V_F  worst rel 6.1e-5, 且全部来自唯一 near-null 控制点 p3_r0
  （独立 rank=621 vs 生产 rank=622, σ_min 恰在 1e-10 相对阈值上——§11.6 点名
  的敏感度, 已如实登记而非掩盖）
- 帧摘要（支持面积加权 log SNR 中位数）独立重算 14.7088589524, rel 0.00
- R2-F01 复核: 64 个控制点各自 f_enclosed ∈ [0.999889, 0.999905]（全不共享同一
  最小值; p2_r4/p3_r2 数值相同系窗口形状全等+平移等变, verify_round3 已证该性质）

## 5. 两项需要前台注意的新科学事实（不改代码, 待裁决）

### 5.1 near-null 方向的数值秩敏感度（§11.6 已预见, 现定量）

M42 生产帧 64 个控制点中, 独立 SVD 与生产 pivoted QR 在 1 个控制点上秩分歧 ±1,
V_F 差 6.1e-5; 其余 63 个秩与 V_F 全一致（worst 6.1e-5 之外 <1e-8）。
condition_number 生产值 3.5e9..7.0e19（pivot 比平方估计, R2-F04 已标注方法名）。
⇒ 跨设备产品若引用 covariance_rank 或 V_F 的低位有效数字, 必须同时引用
solver_condition_estimate_method、rank_tol 与 near-null 登记。**不需要改算法**,
需要前台在产品合同里写明这些诊断的引用口径。

### 5.2 §11.3 源泊松项的 a_ref 窗归一 vs 总通量模板语义（小窗时可测）

生产实现: 条件源泊松 = F_ref·P_j/(g·a_ref)（窗归一, a_ref=ΣP）; 而模板 P 与
F_ref 是 PSF.md §4 的 analytic_total 总通量语义（窗不重归一）。两者在 ΣP<1 时
不自洽: 条件方差系统性偏大 1/a_ref 的 Poisson 份额。定量（verify_round5
Part 2/3, 9×9 窗 ΣP=0.9734）:
- 条件 V_F 比物理世界精确方差大约 **0.55%**（F=F_ref 处; F_true=1000 比值 0.9925）
- 生产 M42 帧 64 控制点 a_ref≈0.999890..0.999905 ⇒ 效应 <1.5e-4, 本轮读数
  与上批逐位一致说明生产中可忽略
⇒ 若将来控制窗缩小（更大 σ、更小模板半径、图像边缘）, 该效应按 1/a_ref 增长。
**建议前台裁决**: 将 §11.3 的 a_ref 配对从"窗归一"改为"总通量语义"
（v_j^src = F_ref·P_j/g, α 标度不变性保持）, 或在正本中明确声明 F_ref 是
**窗内通量**并把模板同步改为 P/a_ref。实现侧不自行决定。

### 5.3 方法论留档（独立重算的第一版 bug）

第一版 sidecar 重算对 C=R diag(v) Rᵀ 直接 eigh: 特征值噪声底 ~ε·λmax ⇒ σ 尺度
~1e-8·σmax, **高于** 1e-10 秩截止 2 个数量级, 165 个零空间方向被当信号,
秩虚高 ~80、V_F 假差至 2.8e-4。改为 Q 的 thin SVD（奇异值噪声底 ~ε·σmax）后
63/64 一致。教训: 秩判定截止必须高于所用分解的数值噪声底; 与生产 QR 对拍时应
使用同尺度（σ 而非 σ²）且高于噪声底的截止。

## 6. 真实留出星复核（ROUND2 执行结论第 1 项(3), M42 T2/M4, 518 颗拟合星）

程序: `code/verify_round5_holdout_stars.py`（参数审计+响应失配+孔径测光）,
`code/verify_round5_holdout_residuals.py`、`code/verify_round5_holdout_chi2.py`、
`code/verify_round5_holdout_chi2_binned.py`（χ²/dof 分层）。日志在 evidence/。

### 6.1 生产 σ 口径核实（R2-F05 的事实基础）

`frame_psf_sigma_px` = sqrt(上中位数(sx)·上中位数(sy)) = **2.211089299**,
与生产日志 2.211089 逐位一致（Python 平均中位数给 2.209696, 差异纯属中位数
约定）。但逐星分布揭示: sx std=22.8%、sy std=19.5%、椭率中位 0.66——
生产"圆对称帧常模型"是**具名近似**（PSF.md §2.3 允许）, 不是逐位置 PSF。
flux ≡ 2π·A·sx·sy/3 逐位成立（拟合内部自洽）。

### 6.2 圆模型 vs 逐星椭圆模型的响应失配（257 颗留出星, 729 像元/星）

- 逐点相对差: median=0.470, p90=0.617, max=9.67
- 窗内包络 ΣP 差: median=4.5e-5, p90=2.4e-4, max=7.4e-3
- 高椭率半 median=0.541 vs 低椭率半=0.368（对椭星系统性失配）

⇒ per-pixel 模板形状失配很大（总通量归一由 2πsx·sy 吸收, 所以 ΣP 差小）。
这是生产参考 SNR 当前真实承受的 PSF 模型误差, 也是 P5 空间 PSF 的量化依据。

### 6.3 孔径测光与残差（40 颗最亮留出星, cleaned 帧实像素）

- 孔径测光 (r=2.5√(sx·sy)) vs 解析总通量: median −3.4%, 与圆 Moffat4 在
  2.5σ 处的窗外份额 (1+2.34)⁻³=2.7% 一致 ⇒ **通量标度无异常**, 差别是
  孔径截断（已知效应, 不是求解器缺陷）。
- χ²/dof 分层（σ_j=√(model/g+B/g+RN²), dof=n−7; 无窗内邻星, 中位 0 颗）:

| flux bin (ADU) | n | χ²/dof median | 结论 |
|---|---|---|---|
| 200–2000（含 F_ref=1000 人群） | 25 | **0.54** | 模型+噪声自洽（略保守） |
| 2000–10000 | 25 | **0.89** | 自洽 |
| 10000–50000 | 25 | 8.24 | 开始失配 |
| 50000–300000 | 24 | 63.9 | 失配 |
| ≥300000（Trapezium 核） | 14 | 274 | 严重失配（数据呈 2×2 平台结构, 混合星/丝状结构, 单 Moffat4 无法表示; 该帧无硬饱和, max=70526） |

⇒ **F_ref=1000 ADU 参考人群的 PSF 模板 + Poisson/RN 噪声模型经过真实留出
数据验证（χ²/dof≈0.5–0.9）**；亮星失配是疏散星团核的已知观测条件（复杂
结构）, 不是求解器缺陷。χ²/dof<1（ faint 端 0.54）说明噪声模型偏保守约
35%——对参考精度是安全方向, 登记待前台裁定是否需要收紧。

## 7. P4 typed Runtime 端口收敛（第一边: drz→wr frame_science）

载体合同（PIPELINE_BLOCK.md, 前台裁决 FINAL-07）: 阶段内唯一载体是
output_dir 文件约定, 无内存块传递 ⇒ typed 迁移 = descriptor/IR/registry
三处同声明 + 消费者真实读取, 不是新建传载荷机制。本轮闭合第一边:

- registry: drz `p1_frame_science` output / wr `p1_frame_science` input +
  `p1_frame_science_published` output（**此前已声明**）
- block_flow 规格 `eng/contracts/block_flow/stage_block_flow.json`: 边已存在
- descriptor（本轮补）: drz 增 output `frame_science`（DATA-P1-FRAMESCIENCE,
  DIMENSIONLESS/ICRS）; wr 增 input `frame_science` + output
  `frame_science_published`
- IR（本轮补）: drz outputs 增 `{"frame_science","artifact:p1_frame_science"}`,
  wr inputs 同步 ⇒ 调度图可见该数据流（此前只存在于注册表文本）
- 验证: `pytest eng/tests/integration/test_registry_code_parity.py` 失败集与
  基线**逐条一致**（12 项既有, 0 新增）; 生产 E2E exit=0（见 run_p5c.log）。

## 8. P5 空间 Moffat4 模型（PSF.md §4 基础完成模型; 生产已接线）

新增 `lib/algorithms/psf/{include/acsd/spatial_psf.h,src/spatial_psf.cpp}`
（Delaunay Bowyer–Watson + 重心凸组合 + 顶点规范化）, CMake 目标
`acsd_p1_dpsf` 增源; normalize_precision 按控制点优先用空间模型, 凸包外/无
顶点回退帧常 σ 并在 support note 登记 `psf_model=`; drizzle 节点从
p1_sources.json 提取本帧 psf_params 顶点（p1_sources 只含拟合成功行 ⇒ 已过
质量过滤; star_id 用行序下标）。

验证（`code/verify_round5_spatial_psf.cpp`, evidence/verify_round5_spatial_psf.log）:
- 规范化恒等: (sx,sy,θ) 与 (sy,sx,θ+π/2) 逐像元响应**逐位相同**（rel=0）;
  θ mod π 归约精确; 轴交换+π/2 后参数一致
- 顶点命中: λ=(1,0,0), 响应 == 单体响应; 质心: λ=1/3, 响应 == 三顶点平移后
  算术平均（rel 2.9e-15）
- 凸包外: 具名 outside_hull 拒绝（不外推）
- 518 颗真实星 ⇒ 1017 个三角形
- **留一法（13 颗留出星）: L1 模板失配 mean=0.135, worst=0.266** —— 对比圆对称
  帧常模型的逐点失配 median 0.470（§6.2）⇒ 模板保真度改善约 3.5 倍

生产 E2E（run_p5d_spatial_psf.log, exit=0; 64/64 available）:
- 63/64 控制点 psf_model=spatial_delaunay_barycentric; 1 个
  frame_constant_median_sigma_fallback_outside_hull（凸包外, 如实登记）
- ctrl0 `p0_r0`: V_F 5439.96409 → **5006.86906**, SNR_ref 13.55820 → **14.13243**
  （秩仍 627; 局部椭圆模型比几何平均圆模型更集中 ⇒ 信息量更高, 方向符合预期）
- 帧摘要 SNR: 14.70886 → **14.02998**; 逐点 V_F ∈ [3489, 6460],
  SNR_ref ∈ [12.44, 16.93]
- sidecar 独立重算（SVD 路径）: 64 ok / 0 failed, 秩一致 63/64（near-null
  p3_r0 同上登记）, 帧摘要 rel 0.00
- 回归: 单元层 159/160（唯一红项 quantization.s12 为既有）; schema 一致性 18/18

**参考精度口径变化（前台知悉）**: 本轮起 reference SNR 使用逐位置椭圆 Moffat4
（PSF.md §4 基础空间模型）, 不再是帧中位圆 σ 具名近似。新旧数值差（本帧
−8% V_F / +4% SNR_ref at ctrl0; 帧摘要 −4.6%）是 PSF 模型保真度变化, 不是
求解器变化; 空间模型的模板失配已达留一法定量（0.135 L1）, 但 SED 参考通带
与 master covariance 尚未闭合, 参考精度保持 approximate 定位不变。
