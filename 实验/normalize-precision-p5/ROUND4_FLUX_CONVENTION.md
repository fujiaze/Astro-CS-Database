# 参考通量总通量口径（NOISE_SNR §2）订正：推导、实现与独立验证

本文记录 Normalize 参考精度链中"总通量口径 / 空间 α / 噪声模型"三者自洽性的订正：
旧实现把窗口和 `a_ref = Σ_window α_j·P_j` 当作源泊松的归一因子，同时让信号模板保持
`h = P`（不承载 α），造成 analytic_total 与窗口通量混称、空间 α 下模板与物理信号
失配。本轮按明确选定的 applied 总通量约定重写该配对，并给出独立闭式与真实世界
前向模拟的验证。

## 1. 约定与推导（原生 → applied）

符号（与 [NOISE_SNR §2](../../../docs/science/noise_snr/NOISE_SNR.md)、
[PSF.md §3/§5](../../../docs/science/psf/PSF.md) 一致）：

- `P_j`：单位**总**通量的像元积分响应，`Σ_all P = 1`（`analytic_total`，PSF.md §5）。
  有限窗口只截取一部分，`Σ_window P < 1` 是真实截断，**不重新归一**（PSF.md §3）。
- `F_native`：源的原生总通量（未施加测光前的积分量）。
- `α_j = k_photo·m(x_j,y_j)`：逐像元确定性标度（applied/native，PHOT-MXY-01）。
- `u`：参考源位置 = 控制点 `(cx_px, cy_px)`；`α_* = α(u)` 为该位置**声明的**标度。
- `F_ref(u) = α_*(u)·F_native`：参考源在 applied 面的总通量（与控制点一一对应，
  不随窗口截断变化）。

applied 面观测模型：

\[
E[y_j]=\alpha_j F_{\rm native}P_j=F_{\rm ref}\cdot h_j,
\qquad h_j=\frac{\alpha_j}{\alpha_*}P_j .
\]

源泊松（原生电子 → applied 方差，原生 gain `g`）：

\[
\operatorname{Var}[y_j]^{\rm src}=\frac{\alpha_j^2 F_{\rm native}P_j}{g}
=\frac{F_{\rm ref}\,\alpha_j^2 P_j}{g\,\alpha_*} .
\]

因为 `F_ref` 与 `h` 成对、`Var` 与同一 `h`、同一 `F_ref` 成对，三者是同一约定下的
同一个量；`α_j ≡ α` 时 `α_* = α`，模板逐位退化为 `h = P`、泊松项退化为
`α·F_ref·P_j/g` —— 即 NOISE_SNR §2 的标量公式 `V'_{src,i} = α_i F' P'_i/g_i`，
**不含任何窗口 sum 因子**。

有限窗口的完整条件方差（已知 blank 平面 `v^blank_j`，applied 面标度）：

\[
v_j=v^{\rm blank}_j+\frac{F_{\rm ref}\,\alpha_j^2P_j}{g\,\alpha_*},
\qquad
I_F=\left\|(I-\Pi_Z)\,J\,D^{-1/2}h\right\|^2,\quad V_F=I_F^{-1},
\]

其中 `J` 是 `Q = R·D^{1/2}` 的行空间投影（NOISE_SNR「数值误差怎样传到参考信息」），
`D = diag(v)`；已知背景时 `Π_Z = 0`。`Σ_j α_jP_j` 仍逐窗计算，但**只作诊断**
（`alpha_window_scale`），不参与任何归一。

### 1.1 与旧实现的差异（可测）

旧实现：

\[
h^{\rm old}_j=P_j,\qquad
v^{\rm old}_j=v^{\rm blank}_j+\frac{\alpha_j^2}{a_{\rm ref}}\frac{F_{\rm ref}P_j}{g},
\qquad a_{\rm ref}=\sum_{j\in{\rm window}}\alpha_jP_j .
\]

- **泊松归一**：新/旧泊松项之比为 `a_ref/α_*`；`α ≡ 1` 时即 `a_ref = Σ_window P`，
  旧口径把方差系统性放大 `1/ΣP` 的泊松份额。9×9 窗（`ΣP = 0.9765`）实测
  `V_F^{old}/V_F^{new} = 1.006616`（+0.66%），与 ROUND3_FIX_REPORT §5.2 登记的
  "0.55%（F=F_ref 处）"同量级；`ΣP → 1`（21×21 窗 `0.999664`）时降到 `1.000092`。
- **信号模板**：旧口径下 `E[y_j] = α_jF_nativeP_j` 不能写成 `F·P_j`（除非 α 为常量），
  旧模型对物理世界**不是**有效统计模型：实测其冻结 `V_F` 与真采样方差相对差
  0.39%（同构型），其估计量均值偏离其自身声明的 `F_ref` 1.46%（对称梯度场）、
  4.29%（非对称场）。新配对下 `V_F` 与真采样方差逐位一致（rel `2.2e-16`）。

## 2. 实现（生产调用点）

| 文件 | 改动 |
|---|---|
| `lib/algorithms/noise_snr/include/acsd/sparse_reference.h` | `PlanarPatchWindow` 增 `reference_alpha_star`（α_*）、`reference_flux`（F_ref(u)）; 删去旧"α_j²/a_ref 配对"注释, 写入总通量口径; `LocalWindow` 增 `planar_template`/`source_pixel_alpha`/`reference_window_scale`; `spherical_reference_jacobi_oracle` 增 `h_override` |
| `lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp` | 模板 `h_j=(α_j/α_*)P_j` 与源泊松 `(α_j²/α_*)F_refP_j/g` 同一 α 合同; `a_ref` 仅作 `reference_window_scale` 诊断; `h_sphere=R·h`; 每控制点 `F_ref(u)`; 空间场缺 α_* ⇒ `reference_alpha_star_missing` |
| `lib/include/acsd/normalize_precision.h` | `PrecisionFrameRequest` 增连续坐标求值器 `alpha_at_xy` |
| `lib/infrastructure/scheduler/src/normalize_precision.cpp` | 逐窗求 `α_*(u)`（连续求值优先, 最近像元回退）; `native_calibrated+applied` 时逐点 `F_ref(u)=α_*·F_native`; psf 载荷写入 α 合同; support note 增 `alpha_convention/reference_alpha_star/alpha_window_scale/per_point_reference_flux` |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | 由 `calibration::photo_spatial_m` 组装 `alpha_at_xy`（m() 唯一实现, 不复制公式） |
| `lib/.../normalize_science/{include,src}` | `LocalModelPayload` 增三字段; 模型文件写/读 `planar_template`/`source_pixel_alpha`/`reference_alpha_star` |

常量 α（`pixel_alpha` 空或 `α_j ≡ α_*`）时，`h ≡ P`、`α_* = plane_scale`，
与旧标量口径除泊松归一因子外逐位一致（verify 程序第 1 条）。

## 3. 独立验证（`code/verify_round4_flux_total.cpp`, ALL PASS）

编译与运行（真实命令）：

```bash
g++ -std=c++17 -O2 -Ilib/include -Ilib/algorithms/noise_snr/include \
  -Ilib/algorithms/psf/include \
  实验/normalize-precision-p5/code/verify_round4_flux_total.cpp \
  lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp \
  lib/algorithms/psf/src/pixel_integrated_psf.cpp -o /tmp/verify_flux_total
/tmp/verify_flux_total
```

判据与实测（expected 全部来自独立闭式/独立模拟，不由被测实现反推）：

| # | 判据 | 实测 |
|---|---|---|
| 1 | 常量 α=k: V_F == 无窗 sum 闭式; 与 `plane_scale` 常量路径逐位一致 | rel `0.0`（两条） |
| 2 | 截断窗 5/9/13/21: 新 V_F == 闭式; 旧/新比 ∈ [1, 1/ΣP] 且随 ΣP→1 收敛 | rel `3.6e-16..3.3e-15`; 比 `1.0597/1.0066/1.0012/1.0001` |
| 3a | 空间梯度+曲率场: V_F == 三维闭式（h 承载 α） | rel `2.9e-16` |
| 3b | 常量 nuisance: V_F/SNR == 显式 2×2 GLS | rel `2.2e-16` |
| 3c | 逐点 `F_ref(u)=α_*·F_native` 被 V_F/SNR 消费 | 逐位（rel `2.9e-16`/`0`） |
| 4 | 秩亏三态（R=I / 满行秩配对 / 重复行）V_F == 独立行空间投影闭式 | rank 81/41/40, rel `0`/`1.8e-16`/`1.1e-15` |
| 5 | `planar_template==(α/α_*)P`、`h_sphere==R·planar`、诊断窗标度 | 逐位; `α_*=2.0`, `ΣαP=1.95301726102` |
| 6a | 物理世界 MC（α≡1, Poisson+RN, n=200000）: 无偏、sd==√V_F、95% 覆盖 | bias `-0.001σ`; sd/√V_F `1.00275`（4σ 内）; 覆盖 `0.9490` |
| 6b | 物理世界 MC（空间梯度, applied blank=α²·原生）: 无偏、sd==√V_F、95% 覆盖 | bias `-0.000σ`; sd/√V_F `1.00165`; 覆盖 `0.9493` |
| 6c | 自洽性: 新 V_F == 真采样方差; 旧 V_F ≠ 真采样方差 | 新 rel `2.2e-16`; 旧 rel `3.9e-3` |
| 6d | 旧 h=P 估计量在非对称 α 下有偏 | 对称 +1.46% / 非对称 +4.29% 均值偏差 |
| 7 | 空间 α 场缺 α_* ⇒ 具名 unavailable, 不造 V_F | `reference_alpha_star_missing`（α_*=0 与 -1） |

日志：`evidence/verify_round4_flux_total.log`。相关程序（同步到新约定）：
`evidence/verify_round5_spatial_alpha_r4.log`、`verify_round5_snr_science_r4.log`；
真实产品读回：`evidence/verify_round6_alpha_contract.log`、
`verify_round5_sidecar_recompute_p5f.log`。

### 3.1 关键定量结论

- **物理世界与条件模型现在逐位自洽**：`V_F` 等于（不是近似等于）真采样方差
  `Σ_j w_j²Var_true(y_j)`；旧口径该差为 0.39%（本构型）。ROUND3 §5.2 登记的
  "0.55% 可测偏差"在新实现下**归零**（该偏差全部来自 1/a_ref 的泊松归一）。
- **MC 散度比 1.0017 ± 0.0016、覆盖 0.9493**（空间场）与 **1.0028 ± 0.0016、
  0.9490**（常量场）——在 4 MC se 内一致；不带任何"放宽阈值"。
- 旧口径在空间 α 下的模板失配是可证伪的：`V_F` 与真采样方差不一致（0.39%），
  估计量均值偏离其声明 `F_ref`（非对称场 4.29%）。

## 4. 生产 E2E 对照（M42 T2/M4, `run/norm-precision-test.json`, α≡1）

运行：`./build/linux-sci-p5/acsd normalize -y --json run/norm-precision-test.json`
（exit=0, 64/64 available）；日志 `evidence/run_p5f_flux_total.log`。
基线 = 同配置旧实现的实跑侧车（逐控制点 PSF/几何逐位相同，仅本轮配对公式不同）。

| 量 | 旧（基线） | 新 | 相对变化 |
|---|---|---|---|
| p0_r0 `V_F` | 5006.869056506422 | 5006.739395558223 | −2.59e-5 |
| p0_r0 `SNR_ref` | 14.132431308638235 | 14.132614303241661 | +1.29e-5 |
| p3_r0（near-null） `V_F` | 5329.586620472828 | 5329.41741967058 | −3.17e-5 |
| p5_r3（最小窗标度） `V_F` | 5288.460993955061 | 5284.0244615380725 | −8.39e-4 |
| 帧摘要 SNR | 14.029976523618295 | 14.030187080046197 | +1.50e-5 |
| 秩 / 可用性 | 627,622,614… / 64/64 | 逐点相同 / 64/64 | 0 |

**变化量已定量解释**：逐控制点窗口标度 `a_ref = Σ_window α_jP_j`（本轮起随 support note
的 `alpha_window_scale` 落盘）实测 0.997073556 .. 0.999966931（空间 PSF 下窗口是
"目标叶全部贡献像元"的并集，部分控制点小于完整 PSF 足迹）。`V_F` 相对变化
= `(1/a_ref−1)×` 信息权重下泊松份额，实测比值 −0.23..−0.29（如 p5_r3:
`−8.39e-4 / 2.94e-3 = −0.286`），方向为旧口径**偏大**（更保守）——与 §1.1 一致。

回归：schema 一致性 18/18；单元层 159/160（唯一红项 `quantization.s12` 为改动前
既有，见 RUN.md §5）；独立 sidecar 重算 64 ok / 0 failed（near-null p3_r0 秩
621 vs 622 为既有登记的数值敏感度）；α 合同读回 64 ok（`h_sphere == R·planar_template`
worst rel 3.4e-16；`alpha_window_scale == α_*·Σplanar` worst rel 5.0e-10，受 note
十进制位数限制）。

真实产品读回（`evidence/verify_round6_alpha_contract.log`）：64/64 通过；
`evidence/verify_round5_sidecar_recompute_p5f.log`：64 ok / 0 failed，
帧摘要独立重算 rel 0.00，worst `V_F` rel 6.34e-5（p3_r0 near-null，既有）。

物理世界判据（`evidence/verify_round5_snr_science_r4.log`）：F_true=F_ref 处
条件 `V_F`/精确物理方差 = **1.0000**（旧口径 0.9925）；物理世界 MC 散度比
0.99995（4 MC se 内）。

## 5. 未决与需 Codex 同步的文档差异

1. **NOISE_SNR §2 与 NORMALIZE_DATA §5 的措辞需要统一**。NOISE_SNR §2 写
   `a_ref = Σ_{完整响应域} α_iP_i`，`P'_i = α_iP_i/a_ref`，`F' = a_ref F`——
   在**完整响应域**上这自洽（`Σ_all P' = 1`）；但 NORMALIZE_DATA §5 写
   `a_ref(u)=Σ_i α_iP_i` 且未声明域，若读者按窗口 sum 实现就会变成"窗口重归一"，
   与 NOISE_SNR §2「局部窗口只截取 P'，不重算总通量归一」及 PSF.md §3
   「重新归一成窗内单位通量须同步改变 F_ref」冲突。本轮实现采用**参考位置 α_***
   约定（`h=(α/α_*)P`、`F_ref=α_*F_native`），它不需要不可评估的完整域和，
   逐控制点保存 `F_ref`（与 §5「每个控制点分别保存 F_ref」一致），常量 α 时
   与 §2 标量公式逐位一致。建议 §5 明确域并改为位置参考约定。
2. **"a_ref" 命名**：本轮把窗口量 `Σ_j α_jP_j` 保留为诊断
   `alpha_window_scale`；正本若继续使用 `a_ref` 需声明它与 `α_*` 的区别。
3. **`reference_flux` 的逐点语义**：`native_calibrated` + applied + 空间场时
   逐控制点 `F_ref(u)=α_*(u)·F_native`；`photo_applied` 声明时保持帧级常量
   （另一种自洽口径）。该分支行为需要正本一句话确认。
4. 残留 `§11.x` 交叉引用（sparse_reference.cpp 内）与现行 NOISE_SNR 分节号
   （§1–§10）不符，属文档重构遗留，待 Codex 统一。
