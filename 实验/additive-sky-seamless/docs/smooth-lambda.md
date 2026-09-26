# A5（SMOOTH-LAMBDA）：UPM 控制点平滑参数 λs 的合成 + 真实数据定案

> **按分歧台账订正（D-07/D-08，2026-09）**：本文为历史正本（reverse_verify 迁入件），其 §2.1 中
> `k_corr=1.4` 行按总编对账《分歧台账》D-08 终裁改写——k_corr 不是普适常数，应为
> `k_shape × k_geo` 两因子公式＋(ρ, pixfrac, 帧数/dither, patch 构成) 几何查表（查表由 P3 单元承载），
> 引用须同时声明标定元组与 N_retained 档位；1.4 属标定几何专属实测带 1.27–1.43 内的一次实现。
> 同文件 N=5 渐近式方向词按 D-07 订正为「高估 ≈9.5%（保守）」。
> 本文 N=4096 档落在 N≥65 渐近域内，公式本体不受影响。详见 `docs/control-variance-adjudication.md`。

> 分片：RELEASE-02 / A5（SMOOTH-LAMBDA）。执行者：SubAgent（UPM 平滑参数实验分片）。
> 性质：**只做实验，不改生产代码/文档**；零 git 写；未跑 ninja/cmake/ctest。
> 独立构建：`实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/build.sh`（g++ 直接编译，链接**真实** `lib/algorithms/coverage/src/upm.cpp`，非复刻）。
> 中间产物 `run/reverse_verify/smooth_lambda/`；大产物 `run/RELEASE-02/smooth-lambda/`。
> 判据**先于结果**登记在 `实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/CRITERIA.md`。

---

## 0 结论速览

| 问题 | 结论 | 关键数值 |
|---|---|---|
| λs 能否有效去除加性天光？ | **能，但存在明确上界**：λs 超过 ~0.1 后 C 场被压平，逐帧天光梯度扣不掉 | 见 §3.2 |
| λs=1000 是不是「合适值」？ | **不是**。λs=1000 已是「C 场≈逐帧常数」的**过平滑极端**（≡ λs→∞） | C 场空间梯度 RMS 下降 **N×**，天光去除率从 X 掉到 Y |
| 亮区会不会被压暗？ | 见 §3.3 / §5 | 峰区压暗 = X% |
| 负责人的缓解观察（各帧都亮 ⇒ 问题不大）成立吗？ | 见 §3.5 | 一致 vs 帧间抖动：X% vs Y% |
| 真实 L4 台阶消除？ | 见 §4 | 台阶比 X（λs=0）→ Y（推荐 λs） |
| 推荐生产默认值 | 见 §6 | `upm.smoothing_lambda` = **X**（区间 [a, b]） |
| 是否需要自适应 λs？ | 见 §7 | — |

> **单位声明（重要）**：本分片所有判据只用**尺度无关**的相对量（比值 / 相对偏差）。
> 合成场景里的 gain / read noise / dark current 是**正向物理模拟参数**，**不是**从 FITS 头
> 或数据反推的物理量；真实数据一律称「流水线标定单位」，不赋予物理量纲。
> λs 的最优值对绝对标度不变（判据 C7 数值验证）。

---

## 1 问题与既有事实

- 生产 UPM 求解（`upm.cpp`）每帧每 control 一个自由度 `C_fk`，
  目标函数（`upm.cpp:543,669`）：
  `min sum_k obs_w[f][k] (r_fk - C_fk)^2 + lambda_s * sum_{k~l} (C_fk - C_fl)^2 + lambda0 * sum C_fk^2`
- `obs_w[f][k]` = 该 (帧,control) 的 **份额式权重** `w_cell`，
  按 control **跨帧归一**：`sum_{f} w_cell(f,k) = control_reliability = 1`（`upm.cpp:645-649`，
  SCI-UPM-001 §7 两级权重性质 (iii)）。⇒ 单帧单节点的数据项权重量级 = `1/n_cov(k)`（L4 实测 `n_cov` 中位 8）。
  **⇒ λs 的"自然尺度"是 O(1/n_cov) ≈ 0.1，不是 1e3。**
- 已知实测（FIX-P2a）：`lambda_s=1000` 时覆盖交界台阶比回到 `0.96`；生产当前 `lambda_s=0`。
- 负责人指出的风险：真实亮结构（M16/M42 核心）被过平滑压暗；缓解观察：该区域各帧都亮 ⇒ 帧间一致。

---

## 2 实验设计（强制方法论：噪声必须物理模拟）

### 2.1 合成场景的物理噪声链（**噪声模型**）

对每个 (frame, control) 生成 64x64 patch 的像素，逐像素：

``
I(p)  = [ (M_base + Core_f)(k) * t_f + B_f(k) ] * (1 + tex(p))      场景单位（"标定单位"）
mu_e  = I(p) * gain                                                电子
n_e   = Poisson(mu_e) + Poisson(dark_e) + Normal(0, read_e)         源+天光散粒 / 暗电流 / 读出
adu   = n_e / gain                                                 增益量化（整数 ADU 域）
``

- `Poisson(mu_e)` 的 `mu_e` **含天光** `B_f` ⇒ 天光**确实改变信噪比**（不是只加常数）。
- control estimator = patch median；`sigma_bg` = 1.4826 x MAD；
  `control_variance = k_corr * (pi/2) * sigma_bg^2 / N_retained`，`k_corr=1.4`，`N_retained=64x64=4096`（SCI-UPM-WEIGHT-001 合同式，与生产 `sampler.cpp` 同式）。
- `tex(p)` = 固定（所有帧同一实现）的空间纹理场，模拟 patch 内真实空间结构（未分辨星/星云纹理）；
  它是**公共**结构 ⇒ 进 `M`，不制造帧间差。`tex_mad=0` 时噪声纯光子散粒主导（场景 A）。
- 参数（正向模拟，非反推）：`gain=2.0 e-/ADU`、`read_e=5 e-`、`dark_e=3 e-`、`sky_base=1.0e4` 单位。

### 2.2 几何与覆盖 = **真实 L4**

control 位置 / tile / `leaf_ipix` / 逐帧覆盖子集**全部照搬** `run/RELEASE-02/L4-rebuild/mosaic_out/p2_samples.json`
（277234 obs / 33472 control / 523 tile / 49 帧），只替换 value/uncertainty 为物理模拟值。
⇒ 覆盖子集突变的**真实拓扑**（33423 条网格边中 6433 条为覆盖子集突变边，19.2%）被完整保留。

### 2.3 真值

- `M_true`：公共天文场景 = 低阶大尺度背景 + **突兀亮度峰值** Core（高斯，`sigma=0.05 deg`，
  放在 **M42 核心真实坐标 RA 83.82 / Dec -5.39**，幅度 30x 天空）——"M16 核心式"。
- `B_f`：逐帧加性天光（offset + 一阶梯度 + 二阶项），**这是待去除的信号结构**。
- `t_f`：逐帧全局乘性透过率（UPM 纯加性模型**无法**吸收 ⇒ 真实压力源）。
- `M_target(f,k) = M_true(k) + B_f(k)`（逐帧真值观测面）；产品应恢复其覆盖加权均值。

### 2.4 场景矩阵

| 场景 | 天光 | 亮峰 | 亮峰帧间 | tex_mad | 逐帧天光幅度 | tau | 用途 |
|---|---|---|---|---|---|---|---|
| `A_base` | 有 | 有 | 一致 | 0（光子散粒主导） | 0.5% 天空 | 0 | 主场景 |
| `A_vary` | 有 | 有 | **抖动 10%** | 0 | 0.5% | 0 | 负责人观察验证 |
| `A_tau` | 有 | 有 | 一致 | 0 | 0.5% | 2% | 乘性压力 |
| `A_nosky` | **无** | 有 | 一致 | 0 | 0 | 0 | **负例 1** |
| `A_nocore` | 有 | **无** | — | 0 | 0.5% | 0 | **负例 2** |
| `A_scale` | 有 | 有 | 一致 | 0 | 0.5% | 0 | 尺度不变性（x1e9） |
| `B_prod` | 有 | 有 | 一致 | 0.18（生产实测尺度） | 14% 天空 | 2% | 生产尺度 |
| `B_nosky` | **无** | 有 | 一致 | 0.18 | 0 | 2% | **负例 1'** |
| `real49` | 真实 | 真实（含 M42 核心） | 真实 | 真实 | 真实 | 真实 | **真实数据** |

### 2.5 λs 扫描与实现

- Oracle：`upm_sweep.cpp` 链接**真实** `upm.cpp`；生产标志
  `gs_damping=0.5 / m_full_frame=1 / final_gauge=1 / tolerance_relative=1 / tol=1e-3 /
  zero_anchor=1e-3 / use_ivar_weight=1 / grid=8 / target_order=9`（与 `module_adapters.cpp:4991-5040` 一致）。
- λs 网格：`0, 1e-3, 0.01, 0.03, 0.1, 0.3, 1, 10, 1000`（log 均匀）。
- 固定迭代预算 `max_iterations=60`（生产 100）；逐点记录 `iterations/converged` 以如实反映未收敛。
- 每个 λs 落盘 C 场、校准场、`w_cell`，由 `analyze.py` 计算全部尺度无关判据量。

---

## 3 合成实验结果

{{TABLES}}

---

## 4 真实 L4 数据实测

{{REAL}}

---

## 5 负责人观察的数值验证

{{OBS}}

---

## 6 定案：推荐生产默认值与配置键

{{RECO}}

---

## 7 是否需要自适应 λs

{{ADAPT}}

---

## 8 改动面（file:line，**本分片不实现**）

{{SURFACE}}

---

## 9 局限与「待定」

{{LIMITS}}

---

## 10 复跑方法

``bash
export TMPDIR=/dev/shm/astrocs_lambda
bash 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/build.sh                       # 编译 oracle（g++，链接真实 upm.cpp）
python3 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/convert_real.py             # 真实 p2_samples.json -> UPMB
bash 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/gen_all.sh                     # 合成场景（物理噪声链）
SW_W=4 SW_MI=60 bash 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/sweep_all.sh real49 A_base A_vary B_prod
python3 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/analyze.py --upmb <...> --prefix run/reverse_verify/smooth_lambda/sw_<name> \
        --truth run/reverse_verify/smooth_lambda/<name>.truth.npz --out run/reverse_verify/smooth_lambda/sw_<name>
python3 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/report.py
``
