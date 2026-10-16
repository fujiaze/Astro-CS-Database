# ROUND9：`sed_response` 参考链的数据合同、已可复用部分与缺失输入

本文件回答子任务 D 要求的三件事：(a) 给出可实现的输入/积分合同；
(b) 实施**能从现有有效数据无歧义求值**的部分并独立验证；
(c) 对仍缺定义/未提供的数据保持具名 unavailable，并列出前台裁决点。

## 1. 现行状态（如实登记）

- 配置入口存在：`parse_reference_config`（`lib/infrastructure/scheduler/src/normalize_precision.cpp:124-134`）
  解析 `sed_id`/`response_id`/`flux_unit`/`normalization` 后**恒置 unavailable**
  （reason = "requires evaluable response integration (SED curve + zero point)"）。
- 因此 canonical 参考精度当前唯一可用路径是 `explicit_linear_flux`
  （用户给正有限 F_ref + 单位 + reference ID）。本仓实跑的参考是
  `explicit_linear_flux` F_ref = 1000 ADU（`native_calibrated` 或 applied 面换算）。
- **积分核已经存在且是生产路径**（不是本轮新写）：`lib/algorithms/photometry/cpp/src/spectrum_integrator.{h,cpp}`

## 2. 可实现的积分合同（与科学正本一致）

PHOTOMETRY.md §2 的光子计数合成响应：

\[
Q_{ks}=\frac{1}{hc}\int f_{\lambda,s}(\lambda)\,R_k(\lambda)\,\lambda\,d\lambda
\]

生产实现（`spectrum_integrator.h` 头部逐字，claim `PHOT-FSYN-CANON-001`）：

```
F_syn = ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ        [W·m⁻²·nm]
F_λ(λ_i) = byte_i·flux_mul + flux_min   [W·m⁻²·nm⁻¹]   (XPSD 官方解码)
```

- `T(λ)`：滤镜透过率 `[0,1]`，Akima 重采样到谱网格，区间外 0；
- `Q(λ)`：CCD QE `[0,1]`，可缺省为 1（显式未建模项）；
- `λ` 因子 = 光子计数权重（`1/(hc)` 被零点吸收，故 `F_syn = hc·N_γ`）；
- 与 Gaia DR3 文档式 5.41 的差别只是去掉与星无关的归一化分母
  `∫TQλdλ`（逐帧常数，被 `location/ZP_syn` 吸收）。

**曲线本体的读/缓存/输出链**：运行时扫描 `<可执行文件目录>/config/response_curves/`
（`filter_curve_json.h`，唯一相对路径，无覆盖键）；实际装载的**身份**（库键、文件、
`n_points`、`wl/n` 范围）落进 `p1_phot.json.passband_identity`（含 `qe` 子对象）。
曲线**数值本体**当前不落盘为产品——这是可复核性缺口（见 §4 裁决点 2）。

### 2.1 独立数值复核（本轮实施并留证）

`code/verify_round9_fsyn_integral.cpp`（编译+运行命令见 `evidence/verify_round9_fsyn_integral.log`）
只调用生产核，期望值由解析式与**本程序内独立实现**（1 nm 细网格 + 复合求积）给出：

| 判据 | 实测 |
|---|---|
| 常量 SED、T=Q=1 ⇒ `F_syn = F_λ(λ₂²−λ₁²)/2`（λ 权重与单位） | rel `0.00e+00` |
| 幂律 SED × 平滑窗 × 线性 QE vs 独立 1 nm 细网格求积 | rel `1.31e-15` |
| T≡0 ⇒ `F_syn ≡ 0`（退化，不给数值） | `0.0` |
| XPSD 解码 `F = byte·mul + flux_min`（对 flux_min 线性，核 = `∫λdλ`） | rel `1.77e-16` |
| `F_syn ≡ hc·N_γ`（显式 h、c 换算） | rel `2.85e-16` |

⇒ 「能从现有有效数据无歧义求值的部分」（积分核 + 官方解码 + 通带/QE 重采样）
**已实现、已独立验证**，可作为 `sed_response` 的积分段复用，不需要新写第二份实现。

## 3. 从积分到 `F_ref`：两种自洽约定与其所需输入

设参考 SED 为 `f_ref(λ)`（其 XPSD 记录给 `byte/flux_mul/flux_min`），帧系统响应
`R_k = T_k·Q_k`，则

\[
F_{\rm syn}^{\rm ref}=\int f_{\rm ref}(\lambda)T_k(\lambda)Q_k(\lambda)\lambda\,d\lambda
\quad[\mathrm{W\,m^{-2}\,nm}]
\]

`F_ref` 是该参考源在**帧 applied 面**的总通量。把 `F_syn` 换成阵列通量需要一条
**零点/归一约定**，只有两种可核对写法：

### 约定 A（帧相对、由本帧拟合吸收零点）— 推荐
测光拟合已给出逐帧 `a_k` 与空间场 `m_k(x,y)`，满足
`f_obs ≈ a_k·m_k(x_s,y_s)·F_syn,s`。取

\[
F_{\rm ref}(u)=a_k\,m_k(u)\,F_{\rm syn}^{\rm ref},
\]

则 F_ref 与帧自身拟合同单位、零误差传播来自同一 `a_k`；对应
`photometry.status = relative_only`（现网真实状态）。**不需要任何绝对零点**，
但需要：
1. 明确的**参考 SED 选择**（`sed_id` 指向哪一个可解析的谱：某颗星？某合成谱？
   什么颜色/温度？）与其 `byte/flux_mul/flux_min` / 波长网格；
2. 参考 SED 与观测帧使用**同一** `T_k/Q_k`（同一 `response_id`）与同一 XPSD 解码；
3. `m_k(u)` 的求值位置 u 声明为控制点（本实现已如此）；
4. `flux_unit`/`normalization` 声明为 applied 面单位（`photo_applied`）。

### 约定 B（绝对零点、需独立标定）
若要绝对标度，需要外部参考星等的**真实零点**（如 Gaia XP 官方合成星等 +
`ZP_syn`）与口径/曝光/总透过率的独立测量。正本 PHOTOMETRY.md §2 明写：组成因子
无独立测量时**只能拟合乘积** ⇒ 约定 B 目前不具备仓内可核对的输入。

## 4. 缺失输入与前台裁决点（精确清单）

`sed_response` 当前保持具名 unavailable。要在生产中启用，需要前台给定：

1. **参考 SED 身份与来源**：`sed_id` 的解析路径（哪份星表/哪个文件字段/哪个合成谱），
   以及该谱的按星 `flux_mul/flux_min` 与波长网格来源（`sed_id` 目前只是字符串）。
2. **响应曲线是否作为产品落盘**：现只落身份（库键/文件/n_points/范围），不落数值；
   参考链要独立复核 `F_syn^ref` 必须能读到曲线本体 ⇒ 需要 data_id/位置/版本，
   或明确接受「按运行时扫描集重算」并声明其可复核边界。
3. **零点/归一约定**：选约定 A（帧相对，推荐，无需绝对零点）还是约定 B
   （需绝对零点）；若选 A，还需确认 `F_ref = a_k·m_k(u)·F_syn^ref` 的
   单位与 `reference.normalization` 的取值（`photo_applied`）。
4. **不确定度口径**：参考 SED 的外定标误差、拟合 `a_k` 的协方差（现网
   `parameter_uncertainty = unmodelled`）、以及其在 `V_F` 中的传播是否需要显式项。
5. **单位与星等换算**：是否允许把参考星等换为通量；零点常数（如 GaiaXPy 的
   `ZP_syn = −26.4899`，仅对**其官方通带**成立）的适用范围。

在这些给出之前，本实现不伪造绝对零点、不把未知曲线当已知：
`sed_response ⇒ unavailable`，`explicit_linear_flux` 仍是唯一 canonical 路径。

## 5. 本轮代码改动

`lib/infrastructure/scheduler/src/normalize_precision.cpp` 的 `sed_response`
分支把原来的单句原因细化为**逐项缺失清单**（机器可读地列出上述 1–5），
使 unavailable 的原因可审计、可直接对应本节裁决点；不改变任何数值行为。

## 6. 与其它子任务的关系

- 积分核复核是 D 的「可复用段」证据，与 A/B/C/E 无耦合。
- 约定 A 一旦被前台采纳，`F_ref` 将由 `a_k`（photometry fit）与 `m_k`（本仓已有
  唯一实现）给出，可直接接进 A 子任务建立的总通量口径
  （`h_j=(α_j/α_*)P_j`、`F_ref(u)=α_*(u)F_native` 的同一位置参考语义）。
