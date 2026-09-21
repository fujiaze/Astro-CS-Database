# 加性天光与无接缝叠加：猜想在什么条件下成立、何时失效

**SCI-C 实验单元论文式精读报告（SCI-505）**

| 项 | 值 |
|---|---|
| 报告对象 | `实验/additive-sky-seamless/`（`README.md`、`code/`、`results/*.json`、`results/REVIEW.md`） |
| 权威依据 | ACCEPTANCE_SPEC §2.3；ASTROCS_DESIGN §2.3/§12.3；`docs/science/PHASE2_UPM.md` §7a/§16；`docs/plugins/algorithms_phase2/11_upm.md` §4.6/§9 |
| 证据口径 | 正文每个数字回溯到 `results/*.json` 的字段或 `README.md` 的小节；两者冲突时**以 JSON 为准**，冲突清单见附录 C |
| 判据分级 | 沿用结果 JSON `gates.rows[].level`：`data` = 生产代码实测；`meta` = 规范转写/代数恒等式，**不构成实现验证** |
| 本轮增量 | SCI-502 三缺陷修复（commit `d77fd11f`，2026-09-22 01:35）后的状态核对，见 §4.8 |
| 复现 | `bash 实验/additive-sky-seamless/code/run_all.sh`（固定 seed，无网络，零 git 写），见附录 B |

---

## 摘要

**问题**。测光校准后，同一片天区的真实信号在帧间是连续的；叠加产品上出现的条带状「接缝」只能来自两处：**被当成可加量处理掉的乘性帧间差**，以及**加性天光面在覆盖子集突变处不连续**。本单元的可证伪猜想是：在纯加性天光世界，统一测光模型（UPM）的四要素——排除自身的参考面、阻尼 α≈0.5、拟合/堆叠权重同源、末端残差场扣除——能让任意覆盖子集上的加权阶跃恒为零，且产品**保留公共背景** B_ref。

**结论（先行）**。

1. **猜想在可表示域内成立，且已用生产代码实测**。产品取 `calibrated_k = raw_k − δ_k`（**只扣逐帧梯度 δ_k，保留 B_ref**）时，覆盖子集突变处的背景电平接缝由 **4.80 e⁻ 压到 0.383 e⁻（12.5×）**；产品中位 **299.17 e⁻ ≈ B_ref 297.33 e⁻**，背景被保留而不是被剪掉（C1 A4、C3 B3/B7）。
2. **「全减背景」（`raw − b_k`）是退化做法，必须禁止作为无接缝证据**。它把背景减到 0.845 e⁻（C1 A5）/ 0.711 e⁻（C3 B4），在 C3 世界产生 **48.4% 的负值像素**；更危险的是它在接缝度量上**看起来更好**（C1 全减臂中位 0.275 e⁻ < 正确臂 0.383 e⁻）——因为背景没了，帧间差自然为零。本单元用一条**非退化判据**（保留背景 + off-locus 对照 + 双边检测）把这一点量化：同一注入下退化判据响应仅 **0.09σ**（恒真反证，`meta`），非退化判据分离度 **36.4σ**，样本外假阳性 **0/60**，A=10 e⁻ 检出率 0.975，5σ 检测限 **6.98 e⁻**（C4）。
3. **猜想失效的边界：无接缝 ⟺ 公共面可表示**。当帧间天光差含「参考面**不可表示**且沿 y **相干**」的分量时，残余接缝随该分量 RMS **线性增长**：slope **0.798**、Pearson **0.896**；尺度由 1600 px 扫到 50 px 使残余接缝 **×5.07**。显著区间为 **≲2× 节点间距（本单元 h=0.0355° ≈ 128 px ⇒ ≈256 px）**（C1 A10）。
4. **纯加性前提**：帧间乘性差必须先在 Phase1 吸收。星表引导的低阶 m̂ 把帧间乘性比偏离 1 从 **0.560 压到 5.89e-4**；未做 Phase1 时纯加性 UPM 后接缝 **27.37 e⁻**，做了之后 **6.31 e⁻（4.33×）**（C2 M1/M4）。
5. **权重**：`control_ivar` 的伪影漏入 **0.0245 e⁻**，比 `uniform`（0.0651）小 **2.7×**、比 `SNR²`（0.3203）小 **13×**；噪声 RMS 亦为三臂最小（1.532 < 1.674 < 2.069 e⁻）。但相对 `uniform` 的 **8.5%** 优势小于 NMC=20 的 MC 误差 ⇒ **噪声项上不可分辨**，决定性优势只在偏差（C5）。
6. **不可检验域（必须与结论同时引用）**：`smoothing_lambda=0` 时 per-(frame,cell) 自由加性场恰好定解，公共场 M 只是每 cell 的规范选择 ⇒「拟合/堆叠权重同源」与「末端残差场扣除」在该域内**不可检验**；`final_gauge` 在 `m_full_frame=1` 时近似 no-op（3.7e-3 e⁻），且**不能**修复子集依赖。这些域内的「恒真 PASS」不得充作证据（C1 A8b/A9）。
7. **工程等价性**：dense cache 与 sparse `calibrate_block` 在 1,048,576 点上 **max|Δ| = 3.1e-15**；稀疏模型 **14,001 B vs 稠密 8,389,129 B（0.167%）**（C6）。
8. **SCI-502 三缺陷已修（代码层）**：FIX-1 相对容差（生产默认 `tolerance_relative=1`，分母用观测尺度 `scale_obs`，近零尺度由 `max(scale_obs,1.0)` 保护）；FIX-2 `converged` 四态 0/1/2/3 + stalled 判据；FIX-3 κ 自适应（`roughness_penalty` 逐级 ×10、上限 6 次）+ provenance。**但**：c1–c7 归档结果**未在修复后复跑**，且状态 2/3 与 κ 自适应路径**无专项测试**——详见 §4.8 与附录 D。

**一句话回答负责人**：加性无接缝的猜想**不是普遍真理，而是一条带适用域的定理**——「接缝为零」等价于「帧间天光差落在参考面的表示空间内」；可表示域内实测 12.5×–37.5× 压缩成立，域外按 0.80×RMS 线性失效，且该前提要求 Phase1 先把乘性差吸收到 5.89e-4 量级。

---

## 1 引言

### 1.1 接缝从哪来

设第 k 帧像素为

    raw_k(x) = m_k(x)·[ s(x) + b_k(x) ] + n_k(x)

其中 s(x) 是帧间连续的真实信号（星云/星点），b_k(x) 是逐帧天光，m_k(x) 是乘性响应（平场/透明度/大气消光），n_k 是噪声。叠加产品上的接缝出现在**覆盖子集发生突变的边界**上：边界两侧参与叠加的帧集合不同，于是

    seam(x_b) ∝ Σ_k w_k(x_b) · [第 k 帧在该边界两侧的差异]

只要「校准后帧间连续」成立，上式两侧相等，接缝为零。因此**接缝不是产品缺陷，而是「帧间不连续量」的放大器**：任何未被校准吸收的帧间差，都会在覆盖子集突变处现形。

本单元把帧间差拆成两支并分别量化：

- **乘性支**（`m_k ≠ m_j`）：会与信号结构 s(x) 相乘，在星云/星点边缘制造与结构相关的接缝；
- **加性支**（`b_k ≠ b_j`）：在平缓天光上制造电平台阶。

### 1.2 猜想

> **H1（主假说）**：在纯加性天光世界，UPM 的四要素使任意覆盖子集上的加权阶跃恒为零，且产品保留公共背景 B_ref。

模型写成（`docs/plugins/algorithms_phase2/11_upm.md` §4.1）：

    y_k(x) = s(x) + C_k(x) + ε_k(x),   C_k(x) = B_ref(x) + δ_k(x)
    calibrated_k(x) = raw_k(x) − δ_k(x)          # 多退少补：只扣逐帧梯度，保留 B_ref

这里 B_ref 是**全部帧联合拟合的公共天光面**（稀疏样条），δ_k 是第 k 帧相对该面的低自由度梯度修正。关键设计是「**排除自身**」：每帧不参与自己那一份公共面的锚定，公共面由其余帧的采样点支撑，因此覆盖子集变化时公共面**不随子集跳变**。

### 1.3 为什么「全减背景」是自欺

`raw − C_k`（全减，含 B_ref）会让每帧背景都 ≈0；两帧相减自然 ≈0，接缝「消失」。这不是对齐做好了，而是**被比较的对象没了**。本单元给出了这一点的三个量化证据：

| 证据 | 数字 | 来源 |
|---|---|---|
| 全减臂产品中位 | 0.845 e⁻（C1 世界）/ 0.711 e⁻（C3 世界） | `c1_additive.json:gates.rows[A5].value`；`c3_public_plane.json:product.full_median` |
| 全减臂负值像素占比 | 48.4% | `c3_public_plane.json:product.full_neg_frac` |
| 全减臂接缝**反而更小** | C1 中位 0.275 e⁻ < 正确臂 0.383 e⁻ | `c1_additive.json:seam_summary.b_med` |

第三条最致命：**用一个退化的度量去选方案，必然选到「全减光」**。因此本单元的第一项工作不是「证明接缝变小」，而是**构造一条非退化判据**（§2.4），并证明它有鉴别力。

---

## 2 方法

### 2.1 观测方程与求解主线

生产链路（`p2_sky_plane_*` + `p2_upm_*`，静态链接 `build/libastrocs_phase2.a`，只读调用，零生产代码改动）分两步：

1. **天光面构建**：全部帧的稀疏采样点（星点掩膜后、每帧独立采样）联合加权最小二乘，解出公共面 B_ref 的样条系数与逐帧 δ_k 系数；gauge 由参考帧/和约束固定（C3 B6 实测 gauge 0↔1 只造成 δ_k 的逐帧常数偏移 5.3e-15，接缝差 0.0）。
2. **施加**：产品 = `raw_k − δ_k`，**保留 B_ref**。`additive_mode` 的生产默认已是 `delta`（`module_adapters.cpp:6512-6526`，2026-09-20 负责人定案）；无天光面产物时显式退化为 `c` 并登记 `additive_mode_degraded`，不静默变成「不校正」。

推导主线只保留这一条：**接缝 = 覆盖子集变化 × 未被公共面吸收的帧间差**；因此「无接缝」的充要条件是「帧间天光差可被公共面 + 逐帧低阶项表示」。

### 2.2 星点掩膜 + 稀疏采样 + control_ivar

- 采样：`win=9` 的局部窗口稳健背景（中位），步长 16 px，星点掩膜命中即丢弃；窗口方差用生产冻结式 `k_corr·(π/2)·σ_bg²/N_retained`（`k_corr=1.4`；`c3_public_plane.py:43-72`）。
- 权重：`control_ivar = 1/σ²`。C5 用同一批样本、同一真值、同一差分构造对照三臂（`control_ivar` / `uniform` / `SNR²`），度量对三臂公平。
- 完整链路实测（C5 W3）：1374 采样点（1001:381 / 1002:369 / 1003:368 / 1004:256，每帧 ≥4），星点掩膜覆盖最亮 0.1% 像素 **100%**，掩膜面积占比 1.19%，联合面 49 节点 / 4 帧 / rank 49 = n_nodes，χ²_red 0.771，δ_k 非零。

### 2.3 参考面表示能力（本报告的核心量）

本单元实测的参考面是 `p2_sky_plane` 的三次样条，节点间距显式取

    h = 0.0355° = 127.8 px   （像素尺度 1″/px；c3_public_plane.py:26-28）

即 **2× 8×8 control cell 的 64 px**。512² tile 上得 7×7 = 49 节点（C1/C3/C6 实测 `n_nodes=49`）。因此「可表示」的经验尺度阈值是 **≳2h ≈ 256 px**；`≲256 px` 的帧间差分量**原则上进不了 B_ref**，只能靠 δ_k 的低阶项部分吸收。

> ⚠ 口径提醒：`docs/science/PHASE2_UPM.md` §7a 把节点间距写作「≈ `hips.tile_width/8` = 64 px」，那描述的是 **UPM 的 8×8 control cell**，不是本单元实测的 sky-plane 样条节点；两者相差 2×。引用 256 px 边界时应以 h≈128 px 为准（附录 C-3）。

### 2.4 非退化接缝判据（本单元的方法学核心）

    m(x)        = nanmedian_y mosaic(x,y)                       # 沿 y 取中位，对星点/细结构稳健
    step(x_b)   = median(残差右 halfwin) − median(残差左 halfwin)  # 残差 = 去 order-2 基线
    excess(x_b) = step(x_b) − median{ step(x_b+δ) : δ∈{±32,±64,±96} }   # off-locus 对照

三条设计约束缺一不可：

1. **保留背景**：被比较的是 `raw − δ_k`（含 B_ref），不是 `raw − C_k`；
2. **off-locus 对照**：真实结构（星云、M42 核心）本身会在边界处造成系统台阶，用同一条带内偏离边界的对照线扣除，报 `excess`；
3. **红/绿双向**：注入接缝必须翻红（检出率），真值无效应必须不翻红（假阳性），阈值在**样本外**标定，且**双边**（|D − μ| > 5σ）。

汇总统计沿用 `sci_c_common.py` 口径：**对 6 条边界的 |量| 取中位**（`un_med`/`d_med`）与最大值。下文所有「中位接缝」均按此定义；`excess` 的汇总为 |excess| 的中位（C1 需由逐边界字段复算，C7 有现成字段）。

### 2.5 证据分级与可证伪设计

| 级别 | 含义 | 本单元实例 |
|---|---|---|
| `data` | 生产代码实测 | C1 A4、C3 B3/B7、C4 N2/N3、C5 W1/W2、C6 E1、C7 R3 |
| `meta` | 规范转写/代数恒等式，**非实现验证** | C3 B2（`b_k − δ_k` 与帧无关是定义式）、C4 N1（恒真反证）、C5 W4（状态机转写） |

可证伪设计（AGENTS.md §5「真值无效应 ⇒ 归零/判红」）：

- **真值无效应 ⇒ 不翻红**：C4 N5 强平滑公共梯度下假阳性 0.0；
- **真值有注入 ⇒ 必须翻红**：C4 N3 A=10 e⁻ 检出率 0.975；
- **负例登记**：C1 A5 / C3 B4 全减臂中位 ≈0（构造性对照，只作「退化定义」证据）；C2 `g_k≡1` 的 oracle 臂；C6 E4 子集求值逐位一致（近乎构造性）。

---

## 3 实验设计

### 3.1 三类数据（最高设计 §12.2）

| 类 | 内容 | 本单元用法 |
|---|---|---|
| HST 真实信号模板 | `testdata/HST_M16/...f657n_v1_drz.fits`，中心 512²，缩放到掩膜外中位 200 e⁻，σ=1.5 px PSF | **只作纯信号 s(x)**，不做物理闭合反推（声明量而非反推量） |
| 纯解析代数合成 | `raw_k = Poisson(s + b_k) + N(0, RN²)`，电子域 gain=1；`b_k` = 平缓梯度 + 二次项 + 正弦；固定 seed `SEED_BASE=20260923`，SHA-256 派生、无时间/环境随机源 | C1–C6 全部世界 |
| testdata 真实数据 | M42 M1 T3 Red 4 帧（300 s），512² 中心裁剪，WCS 预估 + FFT 相位相关对齐（实测位移 [0,0]/[0,0]/[8,6]/[10,8] px） | C7 真实互证 |

覆盖图案：4 帧 x 向条带 `A:gx0-2 / B:gx2-4 / C:gx4-6 / D:gx6-7`（8×8 control cell / 512² tile），覆盖子集在 cell 边界 `x = 128/192/256/320/384/448` 突变——**接缝判据的作用面**。

### 3.2 七个实验

| 实验 | 世界 / 对照 | 回答的问题 |
|---|---|---|
| C1 纯加性 | 纯加性 + 阶跃注入曲线 + 基外分量扫描 | 接缝压缩是否成立？失效边界在哪？ |
| C2 乘性 | 注入 `g_k={A:1.00,B:1.56,C:0.85,D:1.20}` + 低阶 m(x,y) 5% + 基外高频 1%@24 px；星表引导孔径测光 180 颗注入 Moffat4 星 | 纯加性前提是什么？不可吸收部分多大？ |
| C3 公共面 | gauge 0/1 对照、`raw−δ` vs `raw−b` vs 未校正 | 背景是否被保留？gauge 是否只差常数？ |
| C4 判据 | 120 次实现；注入落在覆盖子集突变 x=256；前 60 定阈值、后 60 测样本外假阳性 | 判据是否非退化？检测限多少？ |
| C5 权重 | 一帧被污染（5× 读出噪声 + x≥256 的 25 e⁻ 系统伪影）；clean/polluted 同 seed 差分 | 三臂权重谁优？优势是否超过 MC 误差？ |
| C6 工程 | dense cache vs sparse `calibrate_block`（1,048,576 点） | 按需求值是否等价、省内存？ |
| C7 真实 | M42 4 帧 + **人工施加**覆盖图案 | 真实数据上结论是否复现？ |

### 3.3 判据变更的披露原则

本单元经一轮独立对抗审稿（`results/REVIEW.md`）。审稿指出 6 条致命问题，其中「把文档判据换成更松的实现判据且未披露」是系统性问题。整改后的规则是：**任何「文档判据 → 实现判据」的改动必须逐条列表披露（原判据、实测、原判据判定、现判据、理由）**，未披露即计 PASS 不可接受。该表是 `README.md` §11 的一部分，本报告在 §5.5 做表格化简述。

---

## 4 结果

### 4.1 纯加性世界：接缝压缩与能力曲线（图 1）

![C1 纯加性世界：阶跃-注入曲线与失效边界](results/figs/fig_c1_additive.png)

**接缝压缩（C1 A4 / C3 B7）**

| 世界 | 未校正中位接缝 | `raw − δ_k`（保留 B_ref） | 压缩比 | 全减臂（退化对照） |
|---|---|---|---|---|
| C1 纯加性 | 4.802 e⁻（max 6.544） | **0.383 e⁻**（max 0.850） | **12.5×** | 0.275 e⁻（中位；max 8.501） |
| C3 公共面 | 5.188 e⁻ | **0.386 e⁻** | **13.4×** | 0.711 e⁻ 产品中位（`raw−b`） |

用本单元自己规定的 `excess` 度量复算（逐边界字段取 |·| 中位）：C1 **5.108 → 0.355 e⁻（14.4×）**，与 `results/REVIEW.md` 重要问题 8 的独立重跑一致。**结论不依赖度量口径的选择**。

**阶跃-注入曲线（C1 A2/A3）**：注入基内（offset+plane）幅度 0→40 e⁻ 时

| 注入 [e⁻] | 0 | 5 | 10 | 20 | 40 |
|---|---|---|---|---|---|
| 未校正臂 | 4.197 | 8.841 | 12.720 | 21.956 | 34.934 |
| 无噪真值镶嵌预言 | 4.841 | 8.932 | 13.050 | 21.475 | 37.733 |
| **`raw − δ_k` 臂** | 0.589 | 0.689 | 0.628 | 0.681 | 0.966 |
| 全减臂（退化） | 0.698 | 0.552 | 0.674 | 0.717 | 0.697 |

未校正臂斜率 0.7681 vs 预言 0.8235（比值 **0.975**）；`raw−δ_k` 臂斜率 **0.0087 e⁻**（应 ≈0）。注意全减臂同样「平坦」——**平坦性本身不是证据，保留背景才是**。这是本单元最干净的一条能力门：两个方向都有标度（未校正臂随注入上升、校正臂不随注入上升），且不是恒真。

### 4.2 失效边界：参考面不可表示的分量（C1 A10）

横轴 = 帧间天光差中「B_ref 不可表示且沿 y 相干」分量的 RMS（σ=64 px 一维高通作为**代理量**），纵轴 = 残余接缝最大值。6 个空间尺度点（`c1_additive.json:out_of_basis_sweep`）：

| 注入波长 [px] | 1600 | 800 | 400 | 200 | 100 | 50 |
|---|---|---|---|---|---|---|
| 基外 RMS [e⁻] | 0.313 | 1.347 | 3.070 | 5.491 | 6.514 | 6.643 |
| 残余接缝 max [e⁻] | 0.892 | 1.836 | 1.525 | 4.675 | **7.160** | 4.523 |
| 残余接缝中位 [e⁻] | 0.330 | 0.624 | 0.244 | 2.431 | 1.169 | 2.808 |

**标度关系**：slope **0.7976**、Pearson **0.8964**、Spearman 0.7714；短/长尺度比（100 px 与 1600 px 的 max 接缝之比）**5.072**。即

    残余接缝 ≈ 0.80 × (参考面不可表示分量的 RMS)

**如实登记**：该关系**整体正趋势成立，但单点非单调**（800 px 的 1.836 > 400 px 的 1.525；100 px 的 7.160 > 50 px 的 4.523），且每波长仅 1 次实现、无误差棒。C4 的 null 给出单边界度量 std≈0.61 e⁻，与 800→400 px 的差值同量级 ⇒ **不足以称「线性标度律」**；可以称「线性上界量级 + 显著正相关」。显著区间 ≲256 px 与「2× 节点间距」的几何预期一致。

### 4.3 乘性张力：Phase1 是纯加性的前提（C2，图 2）

![C2 乘性世界：Phase1 归一、MA 恢复与乘性残差](results/figs/fig_c2_multiplicative.png)

| 门 | 量 | 实测 |
|---|---|---|
| M1 | Phase1 后帧间乘性比偏离 1 | **5.892e-4**（未做时 **0.5599**） |
| M2 | MA 求解器自身 g_k 残差 | 0.0376（无梯度）/ 0.107（有梯度）；**Phase1 5.89e-4**（MA 比 Phase1 差 63.8×） |
| M3 | 未吸收臂 MA 恢复注入 g_k | 相对误差 0.0295（无梯度）/ 0.0477（有梯度）；oracle 底噪 0.0217 |
| M4 | 纯加性 UPM 后接缝 | 未做 Phase1 **27.367 e⁻** vs 做了 **6.314 e⁻**（**4.334×**）；oracle 臂 6.196 e⁻ |

**机制**：历史 1.56× 帧差来自「Phase1 未施加的乘性归一」（`photometry_applied=false`、`photscal=1.0`）。低阶乘性面可被 Phase1 的 m̂ 完全吸收（5.89e-4），不可吸收的基外高频分量（1%@24 px）则残留：

- **幅度量化**（匹配滤波，对模板做比）：10% 幅度下恢复 **0.693 ± 0.005（58.4σ）**；1% 幅度下恢复 **0.672 ± 0.023**，3σ 上界 **0.069**（`c2_multiplicative.json:hf_matched_filter*`）。分块（4 px）估计器对高频有 sinc 衰减、且比值是信号加权平均 ⇒ **0.69 是下界，不是无偏幅度**。
- **空间频率定位**（分块 PSD，bin=4）：峰 **k=26 vs 预期 21.33**（`psd_k_peak`/`psd_k_expected`），落在 ±6 bin 的宽窗内，但 **`psd_peak_at_injected=false`**（未落进 ±2 bin 判据），峰对应周期 19.7 px ≠ 注入 24 px ⇒ **定位是「量级相符」，不是「精确命中」**。
- **对电平接缝的贡献有界**：同 seed 扫描 hf_amp∈{0,0.002,0.005,0.01,0.02}，max|Δseam| **2.139 e⁻ < 50% 基线 17.496 e⁻**；但 Spearman 仅 0.50 ⇒ 原「线性增长」判据不成立，现判据是「有界」（§5.5）。**机制性原因**：电平接缝沿 y 取中位，对 y 向细结构本就不敏感；乘性世界的接缝由**低阶乘性残差 × 信号结构**主导（hf=0 基线已 17.50 e⁻）。

### 4.4 非退化判据：鉴别力、假阳性、检测限（C4，图 3）

![C4 判据：null 分布与红/绿双向响应](results/figs/fig_c4_criterion.png)

120 次实现；注入接缝加在帧 B 的 x≥256（左子集 {B}，右子集 {B,C}，几何预言响应斜率 ≈ w_B/(w_B+w_C) ≈ 0.5，实测 0.449）。

| 门 | 量 | 实测 | 级别 |
|---|---|---|---|
| N1 | **退化判据**（全减背景后看帧间差）对注入 50 e⁻ 的响应 | **0.092σ**（不翻红） | `meta`：恒真反证 |
| N2 | 非退化判据（保留背景 + off-locus）分离度 | **36.44σ** | `data` |
| N3 | 样本外假阳性 / A=10 e⁻ 检出率（双边、\|D−μ\|>5σ，μ=−5.102，σ=0.6266） | **0/60 = 0.0** / **0.975** | `data` |
| N4 | 5σ 检测限（响应斜率 0.449 外推，双边） | **6.975 e⁻** | `data` |
| N5 | 强平滑**公共**梯度下的假阳性 | 0.0 | `data` |
| N6 | null 分布高斯性（偏度 / Shapiro p） | 0.074 / 0.858 | `data` |

响应线性：注入 2/5/10/20/50 e⁻ → 响应 0.798 / 2.325 / 4.386 / 8.885 / 22.425 e⁻；检出率 0.0 / 0.058 / 0.975。

**这就是「非退化」的证明**：同一注入下退化判据 100% 不响应，本判据 36σ 响应；真值无效应（平滑公共梯度）时假阳性为 0。**但 N1 必须读作「退化判据的定义」，不是「排除自身参考面必要性」的经验证据**——退化臂减掉的是逐帧天光真值，注入被代数精确抵消，0.09σ 只是噪声。「保留 B_ref 必要」由 C3 B3/B4（产品 299.17 vs 全减 0.711 e⁻）与 C4 N2/N3 支撑。

### 4.5 采样权重：偏差项决定性、噪声项不可分辨（C5，图 4）

![C5 三臂权重：估计噪声与伪影漏入](results/figs/fig_c5_weights.png)

| 臂 | 真值加权 RMS [e⁻]（中位 ± std） | 伪影漏入 [e⁻]（中位） |
|---|---|---|
| **`control_ivar`** | **1.532 ± 0.698** | **0.0245** |
| `uniform` | 1.674 ± 0.639 | 0.0651（**2.66×**） |
| `SNR²` | 2.069 ± 1.044 | 0.3203（**13.1×**） |

`control_ivar` 是 RMS 点估计最优，但相对 `uniform` 的优势仅 **8.46% < NMC=20 的 MC 误差（std 0.698 e⁻）** ⇒ **噪声项上三臂不可分辨**。决定性优势在偏差漏入：用 JSON 的 mean/std/N=20 反推，`control_ivar − uniform` ≈ 5.5σ、`control_ivar − SNR²` ≈ 6.2σ（`results/REVIEW.md` 已核实为站得住 9）。这与上游 SCI-402 的独立结论一致（`实验/absolute-snr/results/*.json`）。

> 命名修正（审稿重要问题 11）：该量更准确的名称是「**污染帧对公共面的扰动**」。污染伪影 25·(x≥256) 在污染帧 D 的覆盖区（x≥384）内是常数，可被 D 自己的平面 δ_k 吸收，因此被度量的二阶效应是「5× 读出噪声 + SNR² 过加权」。

### 4.6 稀疏按需求值 vs 稠密（C6，图 5）

![C6 稀疏与稠密：持久化体积与峰值内存](results/figs/fig_c6_memory.png)

| 门 | 量 | 实测 |
|---|---|---|
| E1 | dense cache vs sparse `calibrate_block` | **max\|Δ\| = 3.109e-15**（1,048,576 点；门限 1e-12） |
| E2 | 持久化体积 | 稀疏 **14,001 B** vs 稠密 **8,389,129 B**（**0.167%**） |
| E3 | 峰值 RSS | 按需 64² 块 **11,744 kB** < 稠密物化 512² **14,520 kB** |
| E4 | 天光面节点占比 / 子集一致性 | 49/262,144 = **1.87e-4**；子集 δ_k、b_k 逐位相同（0.0） |

**工程结论**：按需现场求值不牺牲正确性（3.1e-15 等价），体积降到 0.167%，峰值内存更低。E3 的余量只有 ~19%（11,744 vs 14,520 kB）、单次测量、两臂工作量不同 ⇒ 作为「内存随分块而非像素增长」的证据**偏弱**，应改测「物化 dense 与否的峰值增量」并重复 3 次（审稿次要问题）。

### 4.7 真实数据 M42 互证（C7，图 6）

![C7 真实数据：度量最红位置裁图（未校正 | 校正），仅作互证](results/figs/fig_c7_real_crop.png)

| 门 | 量 | 实测 |
|---|---|---|
| R1 | 帧间背景电平比 `level_ratio`（**良态量**） | 中位 **0.9805**，范围 0.9386–1.0503 |
| R1 | 乘性斜率（分块中位稳健拟合，6 对） | 中位 **0.995**，范围 0.806–1.323；`corr(slope,intercept) = −0.9981`，分块动态范围仅 **8.5 ADU** ⇒ **截距/加性偏移不可辨识** |
| R2 | 生产天光面 | rc=0，n_used=841，n_nodes=49，n_params=58，rank=49，**κ=3.156e7**，χ²_red **1.0037**，15 次迭代 |
| R3 | 真实帧 + **人工**覆盖图案接缝（**excess**） | 未校正 **21.836 e⁻** → `raw−δ_k` **0.583 e⁻（37.5×）**；step 口径 20.886 → 0.460 e⁻（45.4×） |
| R5 | 相对接缝度量 | **3.90e-4**（< 1%） |

**诚实标注**：真实 M42 帧本身不构成条带覆盖，R3 是「真实帧 + 人工覆盖图案」的混合实验；斜率动态范围仅 ~10 ADU，`level_ratio` 的 ±5% 才是可用读数，「乘性失配 ≤±30%」的说法应降级。裁图（R4）只作目检互证，**不是**无接缝的科学证据。

### 4.8 SCI-502 三缺陷修复后的状态（本轮）

SCI-502（commit `d77fd11f`）已在生产代码落地三处修复。**下表区分「代码事实」与「可执行证据」**：

| 缺陷 | 修复内容（代码事实） | 位置 | 可执行证据 |
|---|---|---|---|
| FIX-1 容差 | 生产默认 `tolerance_relative = 1`；相对判据分母 = **观测量稳健尺度** `scale_obs`（`\|value\|` 中位数），阈值 = `tolerance × max(scale_obs, 1.0)`（近零尺度退化为绝对保护） | `module_adapters.cpp:5994`；`upm.cpp:707-718, 1023-1026` | 有：`synthetic_gate.cpp:5826-5904`（生产尺度下 legacy 绝对容差 converged=0、相对容差 converged=1） |
| FIX-2 状态机 | `converged` 四态 **0=max_iter / 1=converged / 2=stalled / 3=invalid**；stalled 判据 = 相对改善量 < 1e-12 连续 **5** 轮；目标非有限 ⇒ invalid；`scale_obs/rel_improve/stall_count` 随模型持久化 | `upm.cpp:96-108, 1028-1050, 1247-1248` | **无专项测试**：全仓无 `converged==2/3` 用例；仅 59 项既有套件通过 |
| FIX-3 近奇异 | `p2_sky_plane_build` 返回 `KAPPA_EXCEEDED` 时按 `roughness_penalty` **逐级 ×10** 重试，**上限 6 次总尝试**（1 初 + ≤5 重试）；provenance 落 `sky_plane_kappa`/`_rank`/`_n_params`/`_iterations`/`_chi2_red`/`_node_spacing_deg`/`_roughness_penalty_configured\|used`/`_kappa_adaptive_attempts\|used`/`_kappa_max`；不放宽 `kappa_max` 求绿 | `module_adapters.cpp:6269-6291, 6325-6331`；`sky_plane.cpp:1000-1003` | **无专项测试**：全仓无 `KAPPA_EXCEEDED`/`kappa_adaptive` 用例 |

**验收门对照（SCI-502 任务书验收门）**：

| 验收门 | 状态 | 依据 |
|---|---|---|
| M42 真实样本有限迭代 converged=1（给迭代数与残差表） | **未取证** | c7 结果产自修复前（2026-09-21 20:56），未复跑 |
| 0/1/2/3 四态各有测试；stalled 红/绿双向 | **未满足** | 无专项测试；C5 W5 的 stalled 臂原本就跑满 300 轮，不是真 stall |
| κ 处理有测试与 provenance 字段 | **部分**：字段已在代码；测试缺失 | 同上 |
| SCI-C 判据复跑全绿 | **未取证** | `results/*.json` mtime 2026-09-21 20:48–20:56；探针二进制 20:47，链接的是修复前的 `build/libastrocs_phase2.a`（现 mtime 2026-09-22 01:32） |

**两点必须写进结论的推论**：

1. 现有 c1–c7 数字全部是**修复前生产行为**的证据。按代码路径分析，探针显式传 `tolerance_relative`（0/1）与 `kappa_max=1e8`，且 κ 自适应在 `module_adapters` 层（探针不经该层）⇒ FIX-1/FIX-3 不改变这些数字；但 **C5 W5 的 stalled 臂在新四态枚举下可能由 0 变 2，属未复跑项**。
2. **FIX-3 的自适应分支在真实 M42 样本上按代码默认不会触发**：`sky_plane.cpp:414/441` 的 `kappa_max` 默认是 **1e8**，而 M42 实测 κ=3.16e7 < 1e8；科学文档 §7a 引用的冻结阈值 `FZ-AP2S-KAPPA-MAX = 1e6` 是 **UPM/GLS** 正规矩阵的上限（`upm.h:281`、`docs/algorithms/v6/frozen/01_NUMERIC_THRESHOLD_FREEZE.md:38`），不是 sky-plane 样条的上限。若按 1e6，则 C1（κ=3.36e6）、C3（κ=3.00e6）、C7（κ=3.16e7）三个样本全部超限、自适应路径会被触发。**这是一条需要在 DOC 域收敛的口径冲突**（附录 C-4）。

---

## 5 讨论

### 5.1 猜想成立的条件（清单）

在以下条件**同时**满足时，「多退少补 ⇒ 无接缝」成立，且有实测支撑：

1. **帧间天光差落在公共面的表示空间内**：可被「公共样条面 + 逐帧低阶 δ_k」表示，即特征尺度 **≳2× 节点间距（本单元 ≈256 px）**。实测 12.5×（C1）/13.4×（C3）/37.5×（C7 excess）压缩。
2. **只做加性多退少补**：`calibrated_k = raw_k − δ_k`，**保留 B_ref**（产品中位 299.17 ≈ B_ref 297.33 e⁻，负值占比 0.0）。全减背景是退化的（§1.3）。
3. **乘性差已在 Phase1 被吸收**：帧间乘性比偏离 1 降到 **5.89e-4**；否则接缝放大 4.33×（27.37 → 6.31 e⁻）。
4. **权重取自同一来源**：拟合权重 = 堆叠权重（`control_ivar`）；在含噪/污染域内其**偏差漏入**比 `uniform` 小 2.7×、比 `SNR²` 小 13×。
5. **判据本身非退化**：保留背景 + off-locus 对照 + 双边检测，36.4σ 鉴别力、样本外假阳性 0/60。

### 5.2 猜想失效的条件（清单）

1. **存在参考面不可表示且沿向相干的帧间差分量** ⇒ 残余接缝 ≈ **0.80 × 该分量 RMS**（Pearson 0.896），尺度 ≲256 px 时显著（1600→50 px 扫描 ×5.07）。工程含义：**节点间距必须 ≤ 目标可表示尺度的 1/2**；`roughness_penalty` 只能在「面确实光滑」的前提下使用，**不得用强平滑掩盖不可表示分量**。
2. **未做 Phase1 乘性归一** ⇒ 接缝 4.33×（且随信号结构增长）。
3. **基外高频乘性分量**（1%@24 px）对**电平**接缝贡献有界（<50% 基线），但会被匹配滤波检出（10% 幅度 58σ；1% 幅度 0.672±0.023、3σ 上界 0.069）⇒ 它不制造大电平台阶，但会制造与结构相关的乘性残差，属**另一个度量口径**的风险。
4. **判据被替换为「全减背景后看帧间差」** ⇒ 判据恒真（0.09σ），必然误选全减光方案。

### 5.3 不可检验域（不得充作证据）

`smoothing_lambda = 0` 时，per-(frame, cell) 自由加性场**恰好定解**（每 cell 一个自由度、观测数 ≥1），公共场 M 只是每 cell 的**规范选择**，子集不变性在 cell 内平凡成立。因此：

| 要素 | 本域内的裁决 | 证据 |
|---|---|---|
| 拟合/堆叠权重同源 | **不可检验**：异源与同源偏差 0.023619 vs 0.023174（差 1.9%，远小于门限 20%） | C1 A8b |
| 末端残差场扣除 | **近似 no-op**：`m_full_frame=1` 时 \|Δ(C+G)\| = **3.7e-3 e⁻**；且它是「每 cell 一个常数、对所有帧同减」，**不能**修复子集依赖 | C1 A9 |
| 阻尼 α≈0.5 | **本链路不需要**：α=1 亦收敛（4 次迭代，`legacy_rel_tol` converged=1） | C1 相邻门 |

**必须在产品/报告中如实标注**：这三条在本实验域内不可检验，不得把该域内的恒真 PASS 当作「四要素都必要」的证据。四要素中真正被实测支撑的是**排除自身的参考面**（由 C3 B3/B4 与 C4 N2/N3 支撑）与**多退少补保留 B_ref**；权重同源与末端扣除只在含噪/非精确拟合域才可能起作用（本单元的含噪域对照是 C5，只覆盖权重一项）。

> 补充事实：生产 `module_adapters.cpp:6020-6023` 的编译期默认是 `smoothing_lambda = 0.0`（键缺省时），`auto` 解析为 `P2_SMOOTHING_LAMBDA_AUTO = 0.1`。也就是说，**不可检验域正是当前生产的默认域**。这一点使「权重同源/末端扣除」的实证缺口从「实验设置问题」升级为「生产配置问题」，应在 L1/L4 之前决定是否给 λs 一个非零生产值（该决策归 SMOOTH-LAMBDA 分线，本报告不裁决）。

### 5.4 工程要求（可直接落到实现/配置）

1. **节点间距**：`node_spacing_deg` 必须 ≤ 目标可表示尺度的 1/2；对 1″/px 的常见采样，若要求表示 ≳256 px 的天光结构，h ≲ 0.0355°（≈128 px）。h 过大会把「不可表示分量」直接留成接缝（0.80×RMS）。
2. **正则化与 κ 的权衡**：FIX-3 的自适应重试**增大** `roughness_penalty`（面更光滑）以压条件数——这恰好**降低表示能力**，与 §5.2 的失效边界方向相反。因此自适应必须**同时**审计接缝度量与 `sky_plane_roughness_penalty_used`，并禁止把「κ 过关」当作「面正确」。
3. **阈值口径统一**：`sky_plane.kappa_max` 目前是未登记的实现默认 1e8，与冻结面 `FZ-AP2S-KAPPA-MAX=1e6`（UPM/GLS）不是同一个量。建议把 sky-plane 的 κ 上限纳入合同/配置并写明与冻结常量的关系，否则「近奇异有处理」在真实样本上可能永不生效。
4. **施加模式**：`seam.additive_mode` 生产默认 `delta`；缺天光面产物时显式降级并写 `additive_mode_degraded`；`raw − C_k`（全减）必须判红。
5. **收敛判据**：无量纲、分母用观测尺度；`tol=1e-6` 不是硬门；四态枚举可诊断（0/1/2/3），且 stalled 必须可证伪（改善量地板 + 连续轮数）。
6. **稀疏化**：天光面以稀疏系数落盘、按块现场求值（14,001 B vs 8,389,129 B，3.1e-15 等价）。

### 5.5 判据变更登记（8 条原判据 FAIL，表格化简述）

独立对抗审稿（`results/REVIEW.md` 致命问题 6）指出 11 处「文档判据 → 实现判据」的放松，其中 8 处此前未在报告中披露。整改后逐条登记如下（完整版见 `README.md` §11）：

| 门 | 原判据 | 实测 | 原判据判定 | 现判据 | 理由 |
|---|---|---|---|---|---|
| C1 A2 | 斜率 ∈ [0.8, 1.2] | 0.7681 | FAIL | 与无噪真值预言之比 ∈ [0.9, 1.1] | 硬窗口未扣散粒噪声对斜率的压低；预言比对更强 |
| C1 A10 | Pearson > 0.9 且单调 | 0.896 / Spearman 0.771 | FAIL | Pearson > 0.85 ∧ slope∈[0.3,1.5] ∧ 短长比>3 | **披露**：0.896 与 0.9 在 6 点上无统计差异；序列非单调已写入正文 |
| C2 M2 | max\|g−1\| < 5e-3 | 0.0376 / 0.107 | FAIL | < 0.15 | MA 的 g_k 受 cell 结构失配偏置，不该当第二道归一 |
| C2 M3 | 相对误差 < 2% | 0.0295 | FAIL | < 5% | 同上；oracle 底噪 2.2% |
| C2 M4 | 未归一/归一 > 5× | 4.334 | FAIL | > 3× | 阈值由 5 降到 3 |
| C2 M6 | Spearman > 0.8 | ρ=0.50 | FAIL | max\|Δseam\| < 50% 基线 | 原判据不成立；电平判据对 y 向细结构不敏感是机制性原因 |
| C4 N4 | A* ∈ [1, 5] e⁻ | 6.975 | FAIL | A* ∈ [0.5, 8] | 原区间无依据；现区间与 σ=0.627、slope 0.449 自洽 |
| C5 W1 | 比次优 ≥ 10% | vs uniform 8.45% | FAIL | 三臂最小 ∧ 比 SNR² ≥ 20% | 8.45% < NMC 误差；已作为「不可分辨」写入 §4.5/§5.3 |

另有 3 处（C1 A7 阻尼、C1 A8 权重同源、C4 N3 检出率点）由 `README.md` §5.2/§6.1 以「边界/不可检验」方式披露，不属「静默放松」。**阅读规则**：本单元结果 JSON 的 `gates.n_pass` 把能力门、缺陷登记门（A7b/W5）、边界门（A8b/A9）、恒真门（A5/B4/N1）混在一起计数；`11/11 PASS` **不等于**「11 条独立能力证据」。

### 5.6 与上游/下游的衔接

- **上游 SCI-402（绝对 SNR）**：天光只进噪声、采样权重必须用 `control_ivar`；本单元 C5 独立复现了权重结论（漏入 2.7×/13×）。
- **下游 L4 视觉验收**：本单元给出可执行的接缝度量与判据（`excess` + off-locus + 双边），建议 L4 直接复用；C7 的目检裁图是「度量与肉眼一致」的互证样本。
- **未纳入项**：Galaxy Center T4 未纳入本版（只有 M42 一路）；`code/reverse_verify/smooth_lambda/` 的 λs 扫描线在 `docs/smooth-lambda.md` §0 结论表中仍是 `X/Y/N` 占位符（`results/REVERSE_VERIFY_CANON.md` 已如实登记「从未回填」），**不构成本报告的证据**。

---

## 6 结论

1. **猜想成立的条件（可判定陈述）**：在纯加性天光世界、帧间乘性差已被 Phase1 吸收（≤5.9e-4）、且帧间天光差可被「公共稀疏样条面 + 逐帧低阶 δ_k」表示（特征尺度 ≳2× 节点间距）时，`calibrated_k = raw_k − δ_k`（**保留 B_ref**）使覆盖子集突变处的背景电平接缝压缩 **12.5×–37.5×**（C1/C3/C7），产品保留公共背景（299.17 ≈ 297.33 e⁻），且判据在 36.4σ 鉴别力、样本外假阳性 0/60、5σ 检测限 6.98 e⁻ 下非退化。
2. **猜想失效的条件（可判定陈述）**：一旦帧间天光差含参考面**不可表示且沿向相干**的分量，残余接缝 ≈ **0.80 × 该分量 RMS**（Pearson 0.896），在 ≲256 px 尺度上显著（1600→50 px 扫描 ×5.07）；未做 Phase1 归一时接缝放大 4.33×。**「无接缝」不是算法的普遍性质，而是「帧间差落在表示空间内」的同义语。**
3. **不可检验域必须与结论同时引用**：`smoothing_lambda=0`（生产编译期默认）下，「拟合/堆叠权重同源」与「末端残差场扣除」不可检验；`final_gauge` 在 `m_full_frame=1` 时近似 no-op（3.7e-3 e⁻）且不能修复子集依赖。四要素中被实测支撑的是「排除自身的参考面」与「多退少补保留 B_ref」。
4. **SCI-502 三缺陷修复后的状态**：FIX-1/FIX-2/FIX-3 的**代码实现已落地**（相对容差 + 四态收敛 + κ 自适应与 provenance，commit `d77fd11f`，既有 59 项 ctest 全过）。**但三项验收门未取证**：无 `converged==2/3` 与 κ 自适应的专项测试；c1–c7 **未在修复后复跑**（结果与探针二进制均早于修复）；真实 M42 的 κ=3.16e7 在 sky-plane 默认 `kappa_max=1e8` 下**不会触发**自适应分支。建议下一步：(a) 补 0/1/2/3 四态与 κ 自适应的红/绿双向测试；(b) 用修复后的库复跑 c1–c7 并对比 `gates.rows`；(c) 收敛 sky-plane 与 UPM 的 κ 上限口径。
5. **对 L4 视觉验收的预期**：M42 上应看到「未校正有可见条带台阶、`raw−δ_k` 后消失」（C7 裁图已给出样本），量化判据建议用 `excess` 中位 < 1% 背景电平；同时必须预期两类残留：(i) 天光差中 ≲256 px 的相干分量（按 0.80×RMS 估计）；(ii) 基外高频乘性分量（1%@24 px 量级）造成的结构相关残差。**Galaxy Center T4 与 λs 生产取值是 L4 之前需要补齐的两项。**

---

## 7 参考文献

**文献（`results/evidence_lit.json`，11/11 经 DOI handle API + Crossref/OpenAlex 元数据页交叉核对，`checked_at=2026-09-21`）**

1. Padmanabhan, N. et al. 2008, ApJ 674, 1217, *An Improved Photometric Calibration of the SDSS Imaging Data* — DOI [10.1086/524677](https://doi.org/10.1086/524677)，arXiv:astro-ph/0703454。
2. Gruen, D., Seitz, S., & Bernstein, G. M. 2014, PASP 126, 158, *Implementation of Robust Image Artifact Removal in SWarp through Clipped Mean Stacking* — DOI [10.1086/675080](https://doi.org/10.1086/675080)，arXiv:1401.4169。
3. Wild, V., & Hewett, P. C. 2005, MNRAS 358, 1083 — DOI [10.1111/j.1365-2966.2005.08844.x](https://doi.org/10.1111/j.1365-2966.2005.08844.x)，arXiv:astro-ph/0501460。
4. Soto, K. T. et al. 2016, MNRAS 458, 3210, *ZAP — enhanced PCA sky subtraction* — DOI [10.1093/mnras/stw474](https://doi.org/10.1093/mnras/stw474)，arXiv:1602.08037。
5. Wahba, G. 1990, *Spline Models for Observational Data*, SIAM — DOI [10.1137/1.9781611970128](https://doi.org/10.1137/1.9781611970128)。
6. Duchon, J. 1977, Lecture Notes in Mathematics 571, 85, *Splines minimizing rotation-invariant semi-norms in Sobolev spaces* — DOI [10.1007/BFb0086566](https://doi.org/10.1007/BFb0086566)。
7. Huber, P. J. 1964, Ann. Math. Statist. 35, 73 — DOI [10.1214/aoms/1177703732](https://doi.org/10.1214/aoms/1177703732)。
8. Holland, P. W., & Welsch, R. E. 1977, Comm. Stat. Theory Methods 6, 813 — DOI [10.1080/03610927708827533](https://doi.org/10.1080/03610927708827533)。
9. Horne, K. 1986, PASP 98, 609, *An optimal extraction algorithm for CCD spectroscopy* — DOI [10.1086/131801](https://doi.org/10.1086/131801)。
10. Regnault, N. et al. 2009, A&A 506, 999, *Photometric calibration of the SNLS fields* — DOI [10.1051/0004-6361/200912446](https://doi.org/10.1051/0004-6361/200912446)，arXiv:0908.3808。
11. Morganson, E. et al. (DES) 2018, PASP 130, 074501, *The Dark Energy Survey Image Processing Pipeline* — DOI [10.1088/1538-3873/aab4ef](https://doi.org/10.1088/1538-3873/aab4ef)，arXiv:1801.03177。

**未能核验（如实登记）**

12. Bertin, E. et al. 2002, in *ADASS XI*, ASP Conf. Ser. 281, 228（SWarp）—— **文章级/未逐页核验**：`evidence_lit.json:failed` 记录「无可解析 DOI/arXiv」，仅能定位到软件页；本报告改以 Gruen+2014（同行评议，含 SWarp 背景处理的系统误差分析）作为该实现的文献侧佐证。任务线索「Wild+2009 MNRAS 天光线」经检索**不成立**。

**开源实现（`results/evidence_code.json`，35/35 条，项目 + commit + 文件:行）**

13. SWarp 2.42.0（GPL-3.0），`astromatic/swarp @ 2f7e8b62`：背景**逐帧独立**估计（`src/makeit.c:312`），扣除为**加性** `*data -= *convert_backdata`（`src/back.c:59,317,719,801`、`src/data.c:206`），coadd 按权重叠加（`src/coadd.c:427-430,1295-1296`）。
14. SExtractor 2.29.0（GPL-3.0），`astromatic/sextractor @ 90296de1`：网格背景 + 中位滤波 + 稳健估计（`src/back.c:55,675,751,833,1096,1098`）。
15. Siril 1.4.4（GPL-3.0 only），`free-astro/siril @ GitLab 48ceaa38`：`img[i] -= background[i]; img[i] += background_mean;`（`src/algos/background_extraction.c:1025-1026`）——**减背景后加回全局均值**，即「保留一个公共面」，与本单元的多退少补同构，从工业实现侧支持「全减背景是退化做法」。
16. photutils v0.7.2（BSD-3-Clause），`astropy/photutils @ 8103fec5`：2D 背景网格 + 样条/中值滤波 + sigma 裁剪（`photutils/background/background_2d.py:64,244-246,311-312,875,735,380`）。

**仓内一手材料**

17. `实验/additive-sky-seamless/results/c1_additive.json` … `c7_realdata.json`（机器可读结果，含 `gates.rows`）；`results/REVIEW.md`（独立对抗审稿）；`实验/absolute-snr/results/*.json`（SCI-402 上游证据）。
18. 生产代码：`lib/algorithms/coverage/src/{upm.cpp,sky_plane.cpp}`、`lib/infrastructure/scheduler/src/module_adapters.cpp`、`lib/algorithms/coverage/tests/synthetic_gate.cpp`（本报告引用处均给 file:line）。

---

## 附录 A 数字索引表（关键数字 → 证据文件:字段）

> 统计口径：接缝汇总 = 6 条边界 |值| 的中位（`un_med`/`d_med`）与最大值。C1 的 `excess` 汇总由逐边界字段复算（JSON 未存汇总字段），与 `REVIEW.md` 独立重跑一致。

| # | 数字 | 值 | 证据文件:字段 |
|---|---|---|---|
| 1 | 未校正接缝中位 / max（C1） | 4.802 / 6.544 e⁻ | `results/c1_additive.json:seam_summary.un_med,un_max` |
| 2 | `raw−δ_k` 接缝中位 / max（C1） | 0.383 / 0.850 e⁻ | `results/c1_additive.json:seam_summary.d_med,d_max` |
| 3 | 全减臂产品中位（C1，退化对照） | 0.845 e⁻ | `results/c1_additive.json:gates.rows[A5].value` |
| 4 | 全减臂接缝中位（C1） | 0.275 e⁻ | `results/c1_additive.json:seam_summary.b_med` |
| 5 | 阶跃-注入比值 / 预言斜率 / δ 臂斜率 | 0.9747 / 0.8235 / 0.008678 | `results/c1_additive.json:curve_step_vs_injection.*` |
| 6 | 失效边界 slope / Pearson / Spearman / 短长比 | 0.7976 / 0.8964 / 0.7714 / 5.072 | `results/c1_additive.json:oob_prediction.*` |
| 7 | 基外扫描 6 点 (RMS, seam_max) | 见 §4.2 表 | `results/c1_additive.json:out_of_basis_sweep[]` |
| 8 | 子集不变性 max dev（同源/异源） | 0.023619 / 0.023174 e⁻ | `results/c1_additive.json:element_necessity.weights_same_source.*` |
| 9 | `final_gauge` no-op | 3.719e-3 e⁻ | `results/c1_additive.json:final_gauge_noop.max_abs_diff` |
| 10 | 星点通量守恒（相对最大） | 6.60e-6 | `results/c1_additive.json:star_flux.rel_max` |
| 11 | 绝对容差缺陷（legacy 臂） | converged=0, 300 轮 | `results/c1_additive.json:gates.rows[A7b].value` |
| 12 | Phase1 后 / 前帧间乘性比偏离 1 | 5.892e-4 / 0.55994 | `results/c2_multiplicative.json:gates.rows[M1].value` |
| 13 | 未做/做了 Phase1 的接缝 | 27.367 / 6.314 e⁻（4.334×） | `results/c2_multiplicative.json:seam_arms.*.median,seam_ratio_none_over_loworder` |
| 14 | MA g_k 残差（flat/gradient）与 Phase1 | 0.0376 / 0.1073 / 5.892e-4 | `results/c2_multiplicative.json:ma_g_errors.*` |
| 15 | 高频匹配滤波 10% / 1% | 0.6926±0.00526（58.4σ）/ 0.6721±0.02302 | `results/c2_multiplicative.json:hf_matched_filter_10pct.*,hf_matched_filter.*` |
| 16 | PSD 峰 vs 预期（bin=4） | k=26 vs 21.33（`psd_peak_at_injected=false`） | `results/c2_multiplicative.json:hf_component.psd_k_peak,psd_k_expected` |
| 17 | 高频对电平接缝 max\|Δseam\| / 基线 | 2.139 / 17.496 e⁻（Spearman 0.50） | `results/c2_multiplicative.json:hf_seam_contribution.*` |
| 18 | 产品中位 vs B_ref 中位（C3） | 299.168 / 297.331 e⁻ | `results/c3_public_plane.json:product.delta_median,Bref_median` |
| 19 | 全减臂产品中位 / 负值占比（C3） | 0.711 e⁻ / 48.4% | `results/c3_public_plane.json:product.full_median,full_neg_frac` |
| 20 | gauge 0↔1 δ_k 常数偏移 / 接缝差 | 5.33e-15 / 0.0 | `results/c3_public_plane.json:gauge.per_frame_const_spread,delta_seam` |
| 21 | 未校正→δ 臂接缝（C3） | 5.188 → 0.386 e⁻ | `results/c3_public_plane.json:seam.none_med,delta_med` |
| 22 | 退化判据响应 / 非退化分离度 | 0.0923σ / 36.443σ | `results/c4_seam_criterion.json:separation.deg_50,nondeg_50` |
| 23 | 样本外假阳性 / A=10 检出率 | 0/60 / 0.975 | `results/c4_seam_criterion.json:false_positive_rate,detect_rate.10.0` |
| 24 | 5σ 检测限 / 响应斜率 / 阈值 | 6.975 e⁻ / 0.4492 / 双边距离 3.133 | `results/c4_seam_criterion.json:detection_limit_e,response.slope,detector_calibration.*` |
| 25 | 公共梯度假阳性 / null 偏度 | 0.0 / 0.0737 | `results/c4_seam_criterion.json:false_positive_smooth_gradient,null_normality.skew` |
| 26 | 三臂 RMS（control_ivar/uniform/SNR²） | 1.532 / 1.674 / 2.069 e⁻ | `results/c5_weights.json:rms_median.*` |
| 27 | 三臂伪影漏入 | 0.0245 / 0.0651 / 0.3203 e⁻ | `results/c5_weights.json:leak_median.*` |
| 28 | 相对 uniform / SNR² 优势 | 8.46% / 25.9% | `results/c5_weights.json:margin_vs_uniform,margin_vs_snr2` |
| 29 | 完整链路采样 / 掩膜 / χ²_red | 1374 点 / 命中率 1.0 / 0.771 | `results/c5_weights.json:chain.*` |
| 30 | 生产收敛枚举（converged/maxiter/stalled 臂） | 1 / 0 / 0（轮数 4,1,300） | `results/c5_weights.json:production_convergence.*` |
| 31 | dense vs sparse max\|Δ\|（1,048,576 点） | 3.109e-15 | `results/c6_sparse_dense.json:upm_memory.dense_vs_sparse_max_abs` |
| 32 | 稀疏 / 稠密体积与比值 | 14,001 B / 8,389,129 B / 0.167% | `results/c6_sparse_dense.json:upm_memory.sparse_bytes,dense_bytes,size_ratio` |
| 33 | 峰值 RSS（64² 块 / 512² 全网格） | 11,744 / 14,520 kB | `results/c6_sparse_dense.json:upm_memory.rss_64block_kb,rss_full_grid_kb` |
| 34 | 天光面节点占比 / 子集一致性 | 1.869e-4 / 0.0 | `results/c6_sparse_dense.json:sky_plane.node_ratio,delta_subset_max_abs` |
| 35 | 真实 `level_ratio` 中位与范围 | 0.9805（0.9386–1.0503） | `results/c7_realdata.json:mismatch_summary.level_ratio.*` |
| 36 | 真实斜率中位 / corr(slope,intercept) / 杠杆臂 | 0.9946 / −0.9981 / 8.5 ADU | `results/c7_realdata.json:mismatch_summary.slope.median,corr_slope_intercept,median_bin_span_adu` |
| 37 | 真实天光面 κ / rank / χ²_red | 3.156e7 / 49 / 1.0037 | `results/c7_realdata.json:sky_build_real.kappa,rank,chi2_red` |
| 38 | 真实接缝 excess / step 与压缩比 | 21.836→0.583（37.5×）/ 20.886→0.460（45.4×） | `results/c7_realdata.json:real_seam.un_excess_med,d_excess_med,un_med,d_med` |
| 39 | 真实相对接缝度量 | 3.904e-4 | `results/c7_realdata.json:real_seam.d_rel_med` |
| 40 | C1 `excess` 汇总（复算） | 5.108 → 0.355 e⁻（14.4×） | `results/c1_additive.json:seam_native.*[].excess`（复算；同 `results/REVIEW.md` 重要问题 8） |
| 41 | 节点间距（本单元） | h=0.0355° ≈ 127.8 px | `code/c3_public_plane.py:26-28`；`run/SCI-403/sky_c1_amp_0.json:cfg.node_spacing_deg` |

---

## 附录 B 复现命令

    # 一键（编译探针 + c1..c7 + 出图；固定 seed，无网络，零 git 写）
    bash 实验/additive-sky-seamless/code/run_all.sh

    # 分步（需先有 build/libastrocs_phase2.a；build_probes.sh 缺失时会 flock 后 ninja）
    bash 实验/additive-sky-seamless/code/build_probes.sh
    python3 实验/additive-sky-seamless/code/c1_additive.py     # → results/c1_additive.json
    python3 实验/additive-sky-seamless/code/c2_multiplicative.py
    python3 实验/additive-sky-seamless/code/c3_public_plane.py
    python3 实验/additive-sky-seamless/code/c4_seam_criterion.py
    python3 实验/additive-sky-seamless/code/c5_weights.py
    python3 实验/additive-sky-seamless/code/c6_sparse_dense.py
    python3 实验/additive-sky-seamless/code/c7_realdata.py
    python3 实验/additive-sky-seamless/code/make_figures.py

环境：Python 3.13 + numpy 2.2.4 / scipy 1.15.3 / astropy 7.0.1 / matplotlib 3.11.2；探针编译 ~2 min；运行时间 C1 ~4 min、C2 ~12 min、C3 ~3 min、C4 ~6 min、C5 ~20 min、C6 ~2 min、C7 ~3 min。日志落 `run/SCI-403/logs/`。

**复现注意（审稿复现性检查结论）**：(a) `build_probes.sh` 已加「缺库先构建」；(b) 归档结果来自**多次手动分步跑**，不是一次 `run_all.sh`；(c) 归档结果与探针二进制均**早于 SCI-502 修复**，复跑前请先重建生产库并重新编译探针。

---

## 附录 C 与正式科学文档/上游材料的口径冲突

> 处理原则：**以 `results/*.json` 的实测为准**（任务要求「以 results/ 为准复核」），下列冲突登记待 DOC 域收敛。

| # | 冲突点 | 正式文档/上游写法 | results/ 实测 | 影响 |
|---|---|---|---|---|
| C-1 | 基外高频分量的 PSD 峰位 | `PHASE2_UPM.md` §16.2、`11_upm.md` §9.3 写「k=4 vs 预期 5.33」；`README.md` §9 验收门表同 | **k=26 vs 预期 21.33**（`c2_multiplicative.json:hf_component`；`bin=4`，`kexp = nb/(24/BIN)`）；`README.md` §4.2/§11.1 已改为 26/21.33 | 文档滞后一版；k=4/5.33 是「把分块数组当原始像素」的单位错位值，**不得再引用** |
| C-2 | 1% 高频分量「未检出」 | `README.md` §4.2 与 §6 第 12 条写「1% 幅度：未检出（amp −0.0004 ± 0.038）」 | JSON/日志均为 **amp1 = 0.6721 ± 0.0230**，3σ 上界 0.069；`detected_at_1pct=false` 来自把 `n_sigma=\|amp−1\|/err` 当作「检出显著性」的标签错误 | 「−0.0004±0.038」**在 results/ 中不可回溯**；1% 分量实际被匹配滤波显著检出（相对 0 约 29σ），只是恢复幅度为下界 |
| C-3 | 参考面节点间距 | `PHASE2_UPM.md` §7a：「节点间距 ≈ `hips.tile_width/8` = 64 px」 | 本单元 h = **0.0355° ≈ 127.8 px**（2× control cell），49 节点/512² tile | §7a 的 64 px 描述的是 UPM 8×8 control cell；「≲2×节点间距（≈256 px）」只有取 h≈128 px 才自洽 |
| C-4 | κ 上限 | `PHASE2_UPM.md` §7a：「`kappa_max` 默认 1e6（FZ-AP2S-KAPPA-MAX）」 | `sky_plane.cpp:414/441` 默认 **1e8**；冻结常量 1e6 属 UPM/GLS（`upm.h:281`） | 真实 M42 κ=3.16e7 在 1e8 下**不触发**自适应；口径不统一会让 FIX-3 形同虚设 |
| C-5 | 施加模式实现现状 | `11_upm.md` §5：`additive_mode`「实现现状…仍默认 `c`，须按现行口径改默认」 | `module_adapters.cpp:6512-6526`（及 `:6213-6214`）已默认 **`delta`**（2026-09-20 定案） | 文档滞后；生产已符合「多退少补」口径 |
| C-6 | FIX-1 行号 | `PHASE2_UPM.md` §16.3 写 `module_adapters.cpp:5941-5942`；`README.md` §10 写 `:5948-5949` | 当前赋值在 `:5989-5994`（`uc.tolerance=1e-6` / `uc.tolerance_relative=1`） | 行号漂移；引用生产代码应带内容锚点（本报告已改） |
| C-7 | C6 峰值 RSS | `README.md` §4.6：11,688 / 14,568 kB | JSON：**11,744 / 14,520 kB** | README 与 JSON 不一致（疑为旧一次运行）；以 JSON 为准 |
| C-8 | `smoothing_lambda` 默认 | `11_upm.md` §5 表格：「默认 0.1（生产常量 `P2_SMOOTHING_LAMBDA_AUTO`）」 | `module_adapters.cpp:6020-6023`：键缺省保持编译期默认 **0.0**；`auto` 才解析为 0.1 | 该默认值决定「不可检验域」是否就是生产域，需在 DOC 域写明 |
| C-9 | `README.md` 文本缺陷 | §4.1 末尾（第 137 行）「…（代理量 σ=64 px 高通在」句子**未写完**（悬空括号） | — | 属报告自身缺陷，结论不受影响（§6 已完整登记边界） |

---

## 附录 D 诚实边界清单（完整）

**A. 证据强度类**

1. **A10 的横轴是代理量**：σ=64 px 一维高通不是生产样条的实际零空间；它给出正确的量级与趋势（slope 0.80），但**不应读作精确的样条投影**。
2. **A10 无误差棒**：每波长仅 1 次实现；C4 null 的单边界 std≈0.61 e⁻ 与部分点间差异同量级 ⇒ 6 点差异大多不显著；**不得称「线性标度律」**，只能说「线性上界量级 + 显著正相关」。
3. **C2 M5 的恢复幅度是下界**：分块（4 px）估计器有 sinc 衰减、比值是信号加权平均 ⇒ 0.693 不是无偏幅度；1% 幅度的恢复值同样偏低。
4. **PSD 定位只到「量级相符」**：峰 k=26 vs 预期 21.33，`psd_peak_at_injected=false`（未落 ±2 bin），峰对应 19.7 px ≠ 24 px。
5. **C7 的斜率/截距不可辨识**：杠杆臂 8.5 ADU、corr=−0.9981；只有 `level_ratio`（±5%）可用，「乘性失配 ≤±30%」应降级。
6. **C7 的覆盖图案是人工施加的**：真实 M42 帧不构成条带覆盖；R3 是混合实验。
7. **C6 E3 内存证据偏弱**：余量 ~19%、单次测量、两臂工作量不同。
8. **C7 目检裁图只作互证**，不作无接缝的科学证据（判据是度量数字）。
9. **C5 的「伪影漏入」应读作「污染帧对公共面的扰动」**：伪影在污染帧覆盖区内是常数、可被其自身 δ_k 吸收；被度量的是二阶噪声/过加权效应。
10. **C5 W2 无阈值/误差棒**：只有中位比较；统计显著性由 JSON 的 mean/std/N 反推（≈5.5σ/6.2σ），非 JSON 直接字段。

**B. 判据/分级类**

11. **`meta` 级门不构成实现验证**：C3 B2（定义式）、C4 N1（恒真反证）、C5 W4（状态机转写）。
12. **恒真/构造性门与能力门混计**：A5/B4（全减臂 ≈0 是构造）、C6 E4（子集一致性近乎构造）、C7 R1（只检查有限性）、C7 R4（只检查图存在）⇒ `11/11 PASS` 不等于 11 条独立能力证据。
13. **8 处判据放松已披露但仍属放松**（§5.5）：其中 A10/M4/M6/N4 的阈值改动**没有独立依据**，只是「实测值落在哪就改到哪」；A2 的预言比对相对更强，属正当改进。
14. **C4 检测器标定在未校正产品上**，且 μ=−5.102 e⁻ 本身是真实结构造成的边界系统偏置 ⇒ 「样本外假阳性 0/60」应读作「不把既有的 ~5 e⁻ 未校正接缝判红」；真正「真值无效应」的臂是 N5。
15. **C4 检测限为双边距离外推**（3.133/0.449=6.98 e⁻），依赖响应线性假设（5 点、线性良好但未做残差检验）。

**C. 覆盖范围类**

16. **不可检验域**：`smoothing_lambda=0` 下「权重同源」与「末端残差场扣除」不可检验（§5.3）；且该域是当前生产编译期默认域。
17. **阻尼 α 非必要**：本链路 α=1 亦收敛（4 轮）⇒ 四要素中的阻尼项在本实验域内无证据价值。
18. **Galaxy Center T4 未纳入**本版，真实数据只有 M42 一路。
19. **λs 扫描线未回填**：`docs/smooth-lambda.md` §0 结论表仍是 `X/Y/N` 占位符（`results/REVERSE_VERIFY_CANON.md` 已登记「从未回填」）；`code/reverse_verify/` 各分片**不作为本报告证据**。
20. **C2 的乘性响应是注入的**（低阶多项式 + 正弦高频），非真实平场；真实平场残差功率谱形状不同。
21. **C2 M4 的绝对残差 6.31 e⁻ 是 C1 纯加性世界 0.383 e⁻ 的 16 倍**，oracle Phase1 臂也有 6.20 e⁻ ⇒ 该世界残余来自 Phase1 的 m̂ 拟合误差（resid_std 0.35%–0.52%），不是加性链路；任务原文要求的「残余归零」**实测未归零**。

**D. 工程/复现类**

22. **归档结果早于 SCI-502 修复**：c1–c7 JSON（2026-09-21 20:48–20:56）与探针二进制（20:47）都早于 commit `d77fd11f`（2026-09-22 01:35）；`build/libastrocs_phase2.a` 现为 01:32 版本。**未复跑**。
23. **FIX-2 的 stalled 分支无实验证据**：C5 W5 的「stalled」臂实际跑满 300 轮（max_iter），不是真 stall；全仓无 `converged==2/3` 测试。
24. **FIX-3 的 κ 自适应无测试且在真实样本上按代码默认不触发**（C-4）。
25. **归档结果不是一次 `run_all.sh` 的产物**（多次手动分步跑）；`README.md` §11.1 已承认。
26. **「零生产代码改动」无法用 git 证明**：`实验/additive-sky-seamless/` 整目录在本轮期间未被 git 跟踪，工作树另有大量并行任务改动；该命题只由「证据方向」（所有写操作落 `run/SCI-403/` 与 `results/`、探针只读 `p2_*`）支撑。
27. **C4 图的 null 直方图面板是合成高斯叠加**（`make_figures.py` 用 `np.random.default_rng(0)` 生成 4000 点作拟合示意），不是 120 个真实实现；读图时以 JSON 的 120 次实现统计为准。
28. **本报告自身的边界**：所有数字为对 `results/*.json` 与 `README.md` 的**只读复核**，未重跑任何实验；除 §4.2/附录 A#40 明确标注的 `excess` 中位复算外，未做新的数值推导。
