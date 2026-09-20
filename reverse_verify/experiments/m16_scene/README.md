# experiments/m16_scene —— M16 场景模板的判据实验

> **STACKN32-001（2026-09-20）**：`stack_n` 1 → 32 全局统一（叠加等效读出噪声 √32×3.1 = **17.536 e⁻**）。
> 本目录三个判据脚本走**场景 JSON 默认值**，故全部已重跑。**阈值/容差一字未改**；
> **三个判据的结论全部不变**（9/9 + 9/9 + 4/4 全绿）。下表为 **stack_n=32** 的当前值，
> 括号内为 stack_n=1 交付版旧值；完整前后对照与"能红能绿"的独立对照见
> `run/RELEASE-02/paper/data/STACKN32/STACKN32-001.md`。

| 脚本 | 判据 | 结果（**stack_n=32**；括号 = stack_n=1 旧值） |
|---|---|---|
| `exp_a6_seeing_aperture.py` | **A6**：固定真值通量、只改 seeing ⇒ PSF 域口径的回收通量必须与 seeing **无关**；5×5 盒和域（生产实现 `star_detector.cpp:151 s.flux = m00`）**不**无关；负例（真值无效应）必须归零 | **4/4 PASS**：PSF 域 1.0→3.0 px 跨度 **0.00381 mag**（0.00544）；盒和域 **0.3358 mag**（0.3315，解析预测 0.3878）；负例逐位归零 / 噪声底 MAD **0.00615**（0.00548）mag。**比值 88.2×**（61×）。（更早修复前 0.0127 / 0.3429 = 27×；2×2 分解见 `run/RELEASE-02/paper/data/M16FIX/M16-SCENE-FIX-001.md` §5.1：**跨度变化主因是饱和重标定**——7e4 e⁻ 满阱下 1 px 臂最亮星核被钳位，不是底图；该结论在 stack_n=32 下重做后**不变**） |
| `exp_variance_closure.py` | 合成帧逐像素方差闭合：C1 配对差分（平场关）、C2 平场乘性增量、C3 掩膜传播、**C4 精确逐像素（体区间）** | **9/9 PASS**：C4 **+0.078 / +0.028 / +0.074%**（+0.099 / +0.099 / +0.220）；C1 **+1.92 / +0.72 / +1.03%**（+2.36 / +1.19 / −0.10，≤5%）；C2 z = **−0.10 / −0.19 / −0.06**（−0.30 / −0.10 / +0.27） |
| `exp_realbase_consistency.py` | **真实底传输**：C1 速率恒等式（由 truth_e 反解 base_rate，容差 1e-9）、C2 结构 σ 与真实帧同量级（比值 ∈ [0.5,2]）、C3 结构 Pearson r ≥ 0.30 | **9/9 PASS**：C1 残差 **≤4.3e-14**（逐位不变）；σ 比 **0.5606 / 0.8615 / 0.8659**（同）；r = **0.887 / 0.988 / 0.987**（同，剔饱和 0.963）。**修复前全红**：C1 = 0.99990（=1−1/t）、σ 比 0.0004 / 0.0141 / 0.0052、r = 0.159 / 0.009 / 0.003 |

> **为什么 C2/C3 对 `stack_n` 不敏感（已定量，不是"没生效"）**：C2/C3 建在 **3-px 高斯平滑后的结构面**上，
> 白噪声经该平滑后方差压到 `1/(4πσ²)=0.88%` ⇒ 读出项对结构 σ 的贡献只有 **7.6e-5 e/s**（nebula_core）/
> **4.6e-5 e/s**（starfield），比结构 σ（3.01 / 0.0196 e/s）低 4–5 个数量级。
> **像素级噪声确实变了**：独立对照 `data/STACKN32/stackn_sensitivity_check.py` 的 K2 量到配对差分方差增量
> **+137.2 / +130.7 / +132.1 ADU²**（解析预测 132.40），K3 帧级稳健 σ ×1.00 / ×1.09 / ×1.18，
> 且**负例**（两臂同为 stack_n=32）会让 K2 判据**翻红**。

> **M16-SCENE-FIX-001**（P9 定位的 `m16_scene.py` ×t 缺陷修复）：
> 真实底速率面被当电子数再 `/t` ⇒ 真实底在合成帧里衰减 ×t（t = 9600/14400/16000 s）。
> 修复 = `src_rate = base_rate + Σ stamp(flux_e)/t`（真实底以**速率**身份进入）；
> 配套把饱和预算从单次曝光满阱 7e4 e⁻ 重标定为**叠加满阱** NDRIZIM×7e4 = 2.24e6 e⁻
> （真实 drz 是 32 次子曝光 drizzle 合成品）。详见
> `run/RELEASE-02/paper/data/M16FIX/M16-SCENE-FIX-001.md`。

## 复跑

```bash
export TMPDIR=/dev/shm/astrocs_m16
python3 reverse_verify/experiments/m16_scene/exp_a6_seeing_aperture.py
python3 reverse_verify/experiments/m16_scene/exp_variance_closure.py
python3 reverse_verify/experiments/m16_scene/exp_realbase_consistency.py   # 真实底传输回归判据
```

前置：先建掩膜与场景帧（见 `reports/RELEASE-02/m16-scene.md` §7 复跑清单）。

## 复用关系（**不另起炉灶**）

- 物理噪声链：`reverse_verify/synthetic/noise_model.py`；
- 前向渲染接口：`reverse_verify/synthetic/m16_scene.py`；
- 测光估计器：`reverse_verify/experiments/f_instr/f_instr_lib.py`（`est_box5` / `est_psf_optimal` / `dmag`）。

## 诚实边界

- A6 的 seeing 是**解析 Moffat4 核**（与生产 canon 同约定），不是 M16 真实 PSF；
  但判据是**口径无关性**（尺度无关），不依赖 PSF 形状；
- 底图是**去噪后的期望面**（真实结构保留、真实 drz 噪声剔除），因此本实验只检验
  「口径 × seeing」的**系统项**，不检验 drizzle 相关噪声的影响；
- **σ_k = 2 px 平滑会削掉底图的小尺度结构**：3-px 平滑后，底图只保留真实帧结构方差的
  **74–81%**（σ 比 0.86–0.90；`exp_realbase_consistency` 的 C2 已按此量级写死 [0.5, 2]）。
  nebula_core 实测 σ 比 **0.561** 偏低，**全部**来自残余 141 px（0.0134%）叠加满阱饱和
  （剔饱和后 r 从 0.887 升到 0.963）—— 这些像素在**真实帧本身**就超过单次子曝光满阱；
- **C2 的预测量子式已订正**（M16-SCENE-FIX-001）：旧式用块内总方差 `Var(m−mean)` 代理
  「配对差分可见的高频方差」，而 256² 块内 tilt+vignette 的平滑分量占 33–36%（对配对差分
  不可见）⇒ 预测偏高 ~1.6×。改为与估计量同算子的 `Var_pair(m)`。**容差（3√2·σ_v）未动。**
