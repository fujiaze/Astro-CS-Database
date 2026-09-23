# 订正：参考通量 `F_syn` 的合成口径（判定、证据与对本实验单元的影响）

> **对象**：本实验单元（`实验/photometric-magnitude`）§2.1 所写的"冻结的合成测光约定"
> `F_syn = ∫S(λ)·T(λ)·Q(λ)·λ dλ × 10^(−0.4·magG)`。
> **性质**：订正记录（本单元只读；按任务 SCI-PHOT-FORMULA-01 的文件域，本文件是**新增**记录，
> **未修改** `REPORT_paper.md` / `README.md` / `code/*.py`）。
> **权威落点**：`docs/science/PHOTOMETRY.md` §2a（变更 claim `PHOT-FSYN-CANON-001`）。
> **一手证据目录**：`run/SCI-PHOT-FORMULA-01/`（code/ 脚本、evidence/ 结果 JSON、logs/）。

---

## 1 判定

**参考合成通量的正确口径是**

```text
F_syn = ∫ F_λ(λ) · T(λ) · Q(λ) · λ dλ            # 单位 W·m⁻²·nm
F_λ(λ_i) = byte_i · flux_mul + flux_min           # 单位 W·m⁻²·nm⁻¹（绝对谱辐照度）
```

**不含 `10^(−0.4·G)`。** `G`（Gaia G 星等）既不进入 `F_syn`，也不作归一化因子。

对本单元的含义：**§2.1 把 `× 10^(−0.4·magG)` 写成"冻结约定"是不成立的**——
它不是生产口径（生产走 XPSD 绝对解码，`compute_f_syn_cached_xpsd`），也不是合成测光的标准定义。

---

## 2 证据

### 2.1 官方定义（一手）

- **采样谱是外部定标（绝对）产品，单位 W·m⁻²·nm⁻¹**。ESA Gaia DR3 文档 §20.12.4
  `xp_sampled_mean_spectrum`：字段 `flux` 声明为 `(float[] array, Flux[W m-2 nm-1])`、
  自述 "Externally-calibrated combined BP and RP flux"；采样网格
  "343 values from 336 to 1020 nm with a step of 2 nm"。
- **合成通量的官方定义式带 `λ`、不带任何星等因子**。ESA Gaia DR3 文档 §5.4.1
  *External Calibration → Zero points* 式 (5.41)：
  `⟨f_λ⟩ = ∫ f_λ(λ) S(λ) λ dλ / ∫ S(λ) λ dλ`。
  本合同去掉的分母 `∫TQλdλ` 与常数 `1/(hc)` 都**与星无关**，被 `location`/`ZP_syn` 吸收。
- **绝对刻度上限 ≈ 1%**（同节："Thus 1 % is thought to be the current state-of-the art
  uncertainty on the 'absolute' calibration scales."）；XP 外定标精度 ±2%（λ ≳ 400 nm）、
  绝对通量标度系统误差 ~1%（Montegriffo et al. 2023, A&A 674, A3, §8.1）。
- **官方无「用 `10^(−0.4·G)` 归一化 XP 谱」这种操作**。检索范围：Gaia DR3 文档
  §20.12.3/§20.12.4/§5.3.5/§5.3.6、De Angeli et al. 2023（arXiv:2206.06143）、
  Montegriffo et al. 2023（arXiv:2206.06205）、Gaia Collaboration et al. 2023（arXiv:2208.00211）、
  GaiaXPy 官方文档、gaiaxpy 2.1.4 全源码 —— 均无。唯一出现处是 Montegriffo §8.1.1 Fig. 27 的
  **作图重标度**："the flux of the second source has been rescaled by a factor of 10^{-0.4(G_A − G_B)}
  to put both fluxes on the same scale."（把两颗亮度不同的源画到同一标度，不是产品操作）。
- **官方 Python 工具 gaiaxpy 2.1.4 不乘任何星等因子**：`src/gaiaxpy/spectrum/sampled_spectrum.py:114`
  为纯线性组合 `coefficients @ design_matrix`；端到端实测 `calibrate()` 输出 vs 官方
  `XP_SAMPLED` 产品 `flux` 的比值中位 `1.000000`、最大相对差 `1.01e−4`（float32 存储精度）。
  （该端到端复算由独立查证子代理完成，本任务未复跑。）

### 2.2 合成实验（真值已知；直接链接生产 `spectrum_integrator.cpp`）

`run/SCI-PHOT-FORMULA-01/code/probe_fsyn_analytic.cpp` + `c1_analytic_check.py`：

| 用例 | 闭式真值 | 生产输出 | 相对差 |
|---|---|---:|---:|
| A 常数谱 × 常数 `T` × 无 QE | `f0·∫λdλ = 9.2750399416e−10` | 同值 | **0**（逐位） |
| B 同上 `T=0.5` | `4.6375199708e−10` | 同值 | **0** |
| C 同上 + `Q=0.8` | `7.4200319533e−10` | 同值 | 1.4e−16 |
| D 线性 SED × 顶帽 `T` | 连续积分 `4.39569e−11` | `4.424499403e−11` | 6.55e−3 |
| E 幂律 SED × 顶帽 `T` | 连续积分 `4.5580389e−14` | `4.620319853e−14` | 1.37e−2 |

**误差分解（D/E 的差不是公式错）**：生产输出与"同一输入网格上的复合 Simpson"**逐位一致**
（rel = 0）；D/E 相对连续积分的差全部来自 **2 nm 网格离散误差**（0.66% / 1.34%），
其中 E 里 **uint8 量化**只贡献 **0.025%**。

**负例（必须归零/判红）**：

| 负例 | 结果 |
|---|---|
| 通带定义在 [1100, 1200] nm（谱网格 336–1020 nm 之外） | `F_syn = 0`；`weighted_wl` 全零（缓存非空但无通带覆盖）；真实数据 26211 星中 `F_syn>0` 者 **0** ⇒ 必须退化到"不能定标" |
| `Q` 曲线落在谱网格之外 | `F_syn = 0` |
| 解码通量处处为负（`flux_min<0`、`byte=0`） | 返回 `−4.6375e−10`（**不钳位**），由调用方 `F_syn>0` 有效域判据拒绝 |
| `flux_mul ≤ 0` / 量化参数非有限 | 显式返回 `0` |
| 旧口径乘 `10^(−0.4·G)` | 同一 uint8 谱上 `legacy/absolute` 随 `G` 变：`G=10` 时 `10^13`、`G=15` 时 `10^11`、`G=18` 时 `6.3e9`；`dlog10(ratio)/dG = 0.400` ⇒ 逐星加性项 `+0.4·G_i` (dex)，单标量零点**吸收不掉** |

### 2.3 真实数据（生产链逐位复现）

`run/SCI-PHOT-FORMULA-01/evidence/d1_zp_sigma_rederive.json`（口径见 `docs/algorithms/PHOTOMETRIC_FIT.md`）：

| 帧 | 落盘 `zero_point_mag` | 按 §1 公式复算 | Δ | 落盘 `zero_point_n_stars` | 复算 n |
|---|---:|---:|---:|---:|---:|
| T2/M1 `...20251212_012404` | −15.126346726632235 | **−15.126346726631280** | 9.5e−13 | 2338 | **2338** |
| T3/M1 `…20251212_021119` | −15.123241368129857 | **−15.123241368086541** | 4.3e−11 | 2309 | **2309** |

⇒ 生产实现与 §1 的公式**逐位一致**（差为 FP64 末位）。

**独立刻度检验**（`a1_xpsd_absolute_check.json`、`a2_bandpass_and_absolute.json`）：
26211 颗真实 XPSD 星，用官方 Gaia G 通带（Riello et al. 2021）+
GaiaXPy `Gaia_DR3_Vega` 零点 `−26.4899` 复算合成星等，与同一记录内的 `magG` 比对：
`median(m_syn − magG) = −0.0037 mag`、MAD `0.0033 mag`（G∈[6,18]、解码谱处处为正，n=11272），
`m_syn` 对 `magG` 的斜率 `0.966`。
三个互斥假设的判别：**绝对定标成立**（H1，偏移 ≈ 0）；乘 `10^(−0.4G)` 的变体偏移 **+16.34 mag**（H2）；
除 `10^(−0.4G)` 的变体偏移 **+32.76 mag**（H3）。

---

## 3 对本实验单元结论的影响（逐条）

1. **§2.1 的公式行不成立**，应读作 `F_syn = ∫F_λ·T·Q·λ dλ`（绝对谱辐照度，无 `magG` 因子）。
2. **本单元的数值结论不受影响**，原因是 `scia_common.f_syn(..., mag_g=…)` 的 `mag_g` 参数在本单元里
   被当作**星等指派重标度**使用，而不是参考通量定义的一部分：
   - `step1_analytic.py:55-62`：显式注释"XP 谱自带其源的绝对通量刻度……正确用法：只用**相对星等偏移**
     `mag_assigned − magG_source`，把谱当作 SED 形状"，即 `F_λ → F_λ·10^(−0.4(m_assigned−G_source))`
     把源谱重标到指定星等 —— 与官方 Fig. 27 的作图重标度同性质，**是合法操作**；
   - `step2_hst_sim.py:96-143`：注入与模型**使用同一个** `mag_eff − 16.0` 因子
     （`f_inj_raw` 与 `f_syn_model` 同参），该因子在 `r_i = log10(F_instr/F_syn)` 中**精确相消**
     ⇒ 前向仿真对估计量（IRLS/Tukey、`σ_obs` 双边界判据）的结论**不受口径影响**；
   - `scia_calib.py:257-260`、`step7_negatives.py:202-203`、`step8_real_frame.py:142`：**不传** `mag_g`
     ⇒ 与 §1 口径一致。
3. **需要修的是文档措辞，不是数值**：`README.md:41`、`REPORT_paper.md:56`、
   `code/scia_common.py:196` 把带 `magG` 的写法标为"冻结约定"，会被读者误当成生产口径。
   本记录即为该措辞的订正依据；上述三处**未改**（本单元只读）。

---

## 4 复现

```bash
cd "<repo root>"
python3 run/SCI-PHOT-FORMULA-01/code/a1_xpsd_absolute_check.py     # 绝对刻度三假设判别
python3 run/SCI-PHOT-FORMULA-01/code/a2_bandpass_and_absolute.py   # 分档复核 + 通带失配量级
g++ -O2 -std=c++17 -I lib/algorithms/photometry/cpp/src \
    -I lib/algorithms/photometry/cpp/include \
    run/SCI-PHOT-FORMULA-01/code/probe_fsyn_analytic.cpp \
    lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp \
    -o run/SCI-PHOT-FORMULA-01/code/probe_fsyn_analytic
./run/SCI-PHOT-FORMULA-01/code/probe_fsyn_analytic > run/SCI-PHOT-FORMULA-01/evidence/c1_probe_raw.tsv
python3 run/SCI-PHOT-FORMULA-01/code/c1_analytic_check.py          # 解析对拍 + 误差分解
python3 run/SCI-PHOT-FORMULA-01/code/c2_negative_controls.py       # 负例量级
python3 run/SCI-PHOT-FORMULA-01/code/d1_zp_sigma_rederive.py       # 生产链 ZP_syn 逐位复现
```

---

## 5 诚实边界

- 本记录**未改**本实验单元的任何文件（`REPORT_paper.md`/`README.md`/`code/*.py` 均只读）。
- `d1_zp_sigma_rederive.py` 的 `ZP_syn` **逐位复现**（Δ ≤ 4.3e−11），但 `sigma_residual_dex` 的复现
  与落盘差约 **2%**（T2/M1 复算 0.20818 vs 落盘 0.20395 dex；`n_matched` 468 vs 落盘 511），
  原因是本任务使用单个大锥 + 亚像素边界匹配，与生产逐帧锥搜索有差异；该差异已在
  `RESOLUTION_m42_curve_resolve.md` §7 登记。
- 通带曲线本身 provenance 为 `unverified`（`eng/packaging/config/filters.json → provenance.status`，
  GAP-025）：本记录的"正确通带"指**配置声明的那一支**（`Baader R`），
  **不**声称该曲线的厂商出处已被核实。
- `Q(λ)≡1` 与计入 `KAF-16803` QE 的实测散度变化为 **+12%**（0.0457 → 0.0510 mag），
  而 `Q≡1` 的跨星色项只有 **0.0082 mag** ⇒ 该 +12% **不能**全部归因于色项，
  本任务**未**做进一步归因（已登记为未决项）。
