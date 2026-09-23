# 实验单元 05：M42 真实数据腿（m42-realdata）

本单元是四个核心创新点的**第三类数据腿**（真实数据，`ASTROCS_DESIGN.md` §12.2）在
**已产出的 M42 真实数据端到端运行**上的独立复核。**不重跑三命令**，只读
`run/RELEASE-05/vis/out/m42_p1_t2`、`m42_p1_t3`、`m42_p2`、`m42_p3` 的既有产品。

---

## 1. 可证伪假说

| 编号 | 假说 | 判据（可红） |
|---|---|---|
| H1 | 每帧独立标定后，`k_photo` 把该帧放到与其它帧同一测光星等坐标系；逐帧与 Gaia 的残差散度落在 EXP-04 已发表量级内 | C1-G1：`2.5·sigma_residual_dex ≤ 0.057457 mag`（EXP-04 三帧仿真 band 上界） |
| H2 | 噪声律 `Var(p) = sigma0² + D(p)/g` 的指数在真实数据上仍与 1 相容；权重效率损失 E 与 EXP-06 同量级 | C2-G1（产品 variance 平面）、C2-G2（跨帧配对独立测量）、C2-G3（E） |
| H3 | 加性天光被正确去除，导出图上没有接缝；合成是**加性加权和**且**保留公共天光平面** | C3-G1b（权威接缝判据 + 行块一致性）、C3-G2（加性加权和）、C3-G3（天光保留） |
| H4 | HEALPix 面坐标原生重叠分配在逐 leaf 覆盖重数上守恒；EXP-07 的极区/接缝缺陷在 M42 天区不显现 | C4-G1（逐 leaf 恒等）、C4-G2/G3（掩码与输入侧交叉核对）、C4-G5（缺陷域实测距离） |

**真值无效应 ⇒ 归零/判红**的对照臂：C1-N1（k≡1 不标定）、C2-N1（常数权重）、
C2-G1d（平坦方差）、C3-N2（算术平均/取最大/取最小）、C4-N1..N5（人为破坏守恒恒等）。

---

## 2. 方法

四个独立脚本，各自固定 seed，只读产品：

- `code/m42_common.py` —— 路径/常量/HEALPix 公式（逐字移植 `lib/algorithms/shared/healpix/healpix_core.cpp:155-226`）/统计工具/判据台账 `Gates`。
- `code/c1_photometry.py` —— SCI-A：逐帧 `k_photo`/`n_matched`/`sigma_residual_dex`/`zero_point_mag` 全量提取；跨帧乘性一致性（逐叶对 `S_i = a·S_j + b` 与中位比值两种估计器）；k≡1/置换/取中位三个对照臂；零点平移不变量的数值演示。
- `code/c2_absolute_snr.py` —— SCI-B：产品 variance 平面与信号的 log-log 斜率；**跨帧配对差** `(x_i−x_j)²/2` 给出的独立经验噪声律；权重效率损失 E（`实验/absolute-snr/code/exp05/exp05_common.py:393-408` 同一量）。
- `code/c3_seam_additive.py` —— SCI-C：权威接缝判据本体逐字副本（`code/seam_criterion.py`，附逐位 SELFTEST）+ 行块一致性判别 + 已知幅值负例注入标定检测限；马赛克重建（加性加权和 vs 算术平均/最大/最小）；天光保留检验。
- `code/c4_leaf_allocation.py` —— 创新点四：逐 leaf 覆盖重数守恒（全量 137,101,312 叶，零容差）；逐样本掩码/输入侧 support 交叉核对；层级面积守恒；EXP-07 缺陷域实测距离。

### 关键方法学决定（诚实边界，先写在前面）

1. **跨帧乘性一致性在本产物上不可判定**。帧 HiPS `signal` 平面的中位电平 ≈ 天光等效电平
   （`stored_median / (k·bg/A_pixel)` 中位 1.019，T2），即**信号平面被天光主导**；而各夜天光
   辐射亮度本身不同（是**真实加性差异**，不是标定失败）。两种估计器（OLS 斜率、中位比值）
   在"标定 vs 不标定谁更一致"上给出**相反**结论，分歧 0.049 dex（T2）/ 0.072 dex（T3），
   与待测效应同量级 ⇒ 登记为 UNDECIDABLE（C1-DIAG），不作门禁。
2. **E 需要真值方差场**。EXP-06 的真实数据臂因此**没有算 E**（`exp06_e3_real.json` 中
   `eff_loss` 出现 0 次）。本单元用"跨帧配对差"作为真值方差场的经验代理，偏差方向为
   **高估**（把帧间系统差计入噪声），已在报告中登记。
3. **导出图上的"512 块边界"不等于 HEALPix tile 边界**。p3 是 TAN 重投影，块网格与
   HiPS tile 网格不对应；本单元在同一块网格上复算权威判据，与渲染器的简化度量
   `seam_ratio` 做同几何对照，并额外用**行块一致性**把真接缝与天体结构区分开。

---

## 3. 数据来源

| 用途 | 路径 | 关键量 |
|---|---|---|
| T2 块（16 帧） | `run/RELEASE-05/vis/out/m42_p1_t2` | `p1_phot.json`/`p1_snr.json`/`p1_flux.json`/`p1_stack.json`；帧 HiPS `signal,support,variance,ivar`（Norder9，115 tile/帧） |
| T3 块（33 帧） | `run/RELEASE-05/vis/out/m42_p1_t3` | 同上 |
| 马赛克（49 帧） | `run/RELEASE-05/vis/out/m42_p2` | `p2_samples.json`/`p2_coverage.json`/`p2_rejection.json`/`p2_integrated.json`/`p2_corrected.json`/`p2_final.json`/`p2_upm_model.json`；二进制 `p2_integrated_{signal,nused,nrej}.bin`、`p2_rejection_{candidates,nrej,accepted}.bin`、`p2_rejection_sample_mask.bin`（1,470,365,696 B = 262144×5609）、`p2_corrected_*.bin`（fp64，49 个） |
| 导出图 | `run/RELEASE-05/vis/out/m42_p3/output_phase3.fits` | 4096²，4 HDU（PRIMARY/COVERAGE/VARIANCE/IVAR），1.8″/px |
| 渲染器自检 | `run/RELEASE-05/vis/out/m42_p3/vis_report.json` | `seam_ratio=1.0096`（门 1.5）、verdict FAIL（`covered_but_nonfinite_px=466515`，2.78%） |

数据规模：马赛克 union = 523 个 Norder9 tile；逐 leaf 覆盖重数中位 8、最大 33、均值 8.324；
被拒样本 17,329,578（rejected_high 13,553,303 + rejected_low 3,776,275）。

---

## 3.5 判据规模

四个脚本共产出 **49 条判据**，43 绿 / 6 红。按 `level` 分布：
`data` 18、`external-consistency` 3、`control` 2、`positive-control` 4、
`negative-control` 17、`degenerate-control` 3、`honest-boundary` 2。
每条判据的完整台账（`id/desc/value/ok/source/level/note`）在 `results/c*.json` 的
`gates.rows[]`，逐条解读在 `docs/CRITERIA.md`。

---

## 4. 结果（数字 + 不确定度 + 对照）

### 4.1 SCI-A 测光星等坐标系（C1）

- 逐帧 `sigma_residual_dex`：49 帧，`2.5·sigma_residual_dex` 中位 **0.44746 mag**、
  min 0.33280、max 0.56886（n_matched 中位 295、min 175、max 555）。
  EXP-04 已发表量级：三帧物理前向仿真 **0.045344/0.057457/0.051718 mag**、真实 testdata 帧
  **0.026520 mag（n=157）**。
  ⇒ **C1-G1 判红：49/49 帧超界，中位为 band 上界的 7.8 倍。**
- 判据能绿/能红（MC 控制）：把 MAD 注入到 EXP-04 真实帧量级 → 估计器回 0.0390 mag（绿）；
  放大 4 倍 → 0.3377 mag（红）。**说明红不是估计器造成的。**
- `k_photo` 组内散度（**如实记录，不改裁决**）：T2 `log10(kmax/kmin)=0.3710387326687012`
  （= 0.92760 mag），T3 `0.7223431688187283`（= 1.80586 mag）；独立复算与落盘值
  **逐位一致（差 0.0）**；两块的 `photscale_spread_warn=true`，`photscale_spread_gate =
  "none (owner ruling 9.49: frame-independent)"`。
- **k≡1（不标定）对照**：`sigma_residual` 对全局乘性平移**不变**
  （数值演示：r 整体平移 0.6 dex 后 MAD 差 1.11e-16），故 k≡1 时**残差散度不变**，
  但帧间有效零点差变成 **0.9276 mag（T2）/ 1.8059 mag（T3）**。这就是"真值无效应 ⇒ 归零"的对照。
- 跨帧乘性一致性：见 §2 决定 1，登记 UNDECIDABLE；对照臂可判：置换 k 臂 0.3517 dex（T2）/
  0.6769 dex（T3）严格大于标定臂 0.2631 / 0.2726 dex。

### 4.2 SCI-B 跨帧绝对 SNR（C2）

- **产品 variance 平面**：log-log 斜率 **−0.0117**（95% CI 见结果文件），
  `corr(signal, variance) = 0.0007`，逐帧相关 0.003/−0.0002/−0.0015；variance 中位
  `1.0350e9`、robust scale `3.3680e8`。信号跨 2.3 倍而方差几乎不变
  ⇒ **噪声律指数与 1 不相容（C2-G1 判红）**。
  控制：合成 `Var ∝ D` → 斜率判绿；合成平坦方差 → 判红。
- **跨帧配对独立测量**：由 `(x_i−x_j)²/2` 得到的经验噪声律 log-log 斜率
  **T2 = 0.9743**、**T3 = 2.3604**（44,653 / 72,400 个叶样本，24 个信号箱）。
  T2 与 1 相容（判绿）、T3 超线性（判红）。
- **权重效率损失 E**（本单元首次在真实数据上计算）：产品权重 `per_sample_ivar` 的
  **E = 1.143239**，与**常数权重臂完全相同**（E 比 = 1.000000），
  而 `w = 1/var_true` 臂 E = −2.2e-16。var_true 跨 49.3 倍。
  EXP-06 参考：phys_auto **7.336e-5**、naive_pixel 1.610e-2、frame_scalar 5.817e-2。
  ⇒ **C2-G3 判红：实测 E 比 EXP-06 推荐方法差 4 个数量级，且等于"不用权重"。**

### 4.3 SCI-C 加性天光与无接缝（C3）

- **权威判据本体逐位一致**：本单元 `seam_steps` 与 `sci_c_common.seam_steps` 在随机输入上
  最大绝对差 **0.0**。
- **朴素全边界判据判红**：14 条 512 块边界上最大 `|excess−mu|` = **4.5013e-6**，
  null `sigma = 2.981e-7`，阈值 1.496e-6 ⇒ 判红。
- **行块一致性判别（真接缝 vs 天体结构）**：列向仅 1/7、行向仅 1/7 边界在 4 个行块上同号；
  一致边界的最大 `|excess|` = **5.916e-7 < 1.496e-6 ⇒ C3-G1b 判绿**。
  即：朴素判据的红是 **M42 星云纤维结构造成的假阳性**，不是加性天光接缝。
- **检测限标定（负例注入）**：在最干净的边界 x=1024 注入已知加性阶跃，
  **A* = 2e-6**（阈值 1.491e-6），A=0 时判绿（3.446e-7）⇒ 判据有真实检出力。
- **加性合成**：抽样 16 个 tile，马赛克与"逐样本 ivar 加权加性和"的最大相对残差
  **0.0**；换成算术平均 **1.159**、取最大 **9.194**、取最小 **1.607** ⇒ 三个负例全判红。
- **天光保留（非全减背景）**：49 帧的 `corrected 电平 / 帧自身天光等效电平` 中位 **1.1336**、
  最小 **0.5245 > 0.3** ⇒ 判绿；负例（整体减掉公共平面）判红。

### 4.4 创新点四 HEALPix 面坐标原生重叠分配（C4）

- **HEALPix 公式自检**：本单元 `pix2ang` 与 `p2_samples.json` 的 6000 个控制点
  （`leaf_ipix → ra/dec`）最大角距 **0.00307″**（= 0.0038 hp_res，hp_res=0.805192″），中位 **0.0″**。
- **逐 leaf 覆盖重数守恒（全量、零容差）**：137,101,312 个叶，
  **违反恒等 `nused+nrej == candidates` 的叶数 = 0，最大绝对差 = 0**。
  `Σ nused = 1,141,247,607`，`Σ nrej = 17,329,578`，`Σ candidates = 1,158,577,185`。
- **逐样本掩码一致**：24 个抽样 tile 上，由 `sample_mask` 逐 slot 计数得到的 accepted 数
  与 `nused` **逐位一致（不一致叶数 0）**；抽样合计 `Σ mask_ones = Σ nused = 50,136,578`。
- **输入侧独立核对**：`candidates(p) == #{帧 k : support_k(p) > 0}` 在 24 个抽样 tile 上
  **不一致叶数 0**（`geom_total = cand_total = 50,905,958`）。
- **层级面积守恒**：`Σ_p support·A_cell(order)` 在 order 0..9 上相对差 **8.754e-9**
  （= eps_fp32 的 7.3%），order0 总面积 **3.644293e-4 sr** = 2.391468e7 个叶面积。
  阈值取 1e-6（≈8.4×eps_fp32）并附负例：把 order5 的 support 乘 (1+1e-5) → 9.991e-6 判红。
- **EXP-07 缺陷域实测距离**（M42 天区 33,472 个控制点，外扩 tile 半对角 0.0810°）：
  - `|z|` 范围 **[0.06657, 0.12223]**，极冠阈值 2/3 = 0.66667 ⇒ **极冠叶数 = 0**（RC1/RC2a 不适用）；
  - 到最近极点 **82.898°**（RC3 域为切点距极点 ≲40″）⇒ 不适用；
  - 到最近 |z|=2/3 接缝圆 **34.708°**（南）/ **45.546°**（北）⇒ 接缝地板不适用；
  - 到 z=0 线（face 4–7 的 u+v=1）**3.736°** ⇒ 只有当视场在 dec 方向跨 >7.5° 才可能触及；
  - **尺度条件却成立**：`A_drop(帧 0.967″/px) = 2.198e-11 sr ≤ 1.4e-10 sr`（接缝地板的触发尺度
    上限 2.4″/px）⇒ **"缺陷不显现"是"天区远离接缝"与"尺度满足"两个条件的合取结果**，
    不是尺度条件不满足。

---

## 5. 可判定结论

1. **C1-G1 判红（可判定）**：M42 真实数据 49 帧的测光残差散度中位 0.4475 mag，是 EXP-04
   已发表量级上界的 **7.8 倍**，49/49 帧超界。该判据经 MC 正/负例证明能绿能红。
   差异来源分析见 `docs/CRITERIA.md` §C1。
2. **C2-G1/C2-G3 判红（可判定）**：产品 variance 平面几乎与信号无关（斜率 −0.0117），
   权重效率损失 E = 1.1432，与"不用权重"完全相同，比 EXP-06 推荐方法差 4 个数量级。
3. **C2-G2 分块不一致（可判定）**：T2 经验噪声律斜率 0.9743（与 1 相容），T3 为 2.3604
   （超线性）⇒ 噪声律在真实数据上**只在部分数据块成立**。
4. **C3-G1b 判绿（可判定）**：用权威接缝判据本体 + 行块一致性判别后，M42 导出图上
   **没有可检出的加性天光接缝**；朴素全边界判据的红是星云结构假阳性。
5. **C4 全部判绿（可判定）**：逐 leaf 覆盖重数守恒零违反、掩码与输入侧三方一致、
   层级面积守恒到 fp32 表示地板；EXP-07 的 RC1/RC2a/RC3/接缝地板在 M42 天区**不适用**
   （用实测距离给出，非引用推断）。

---

## 6. 诚实边界

- **未独立验证的结论**（不得当成本单元结论引用）：
  1. "标定是否消除帧间电平差"——两估计器结论相反，登记 UNDECIDABLE；
  2. EXP-06 的五个 E 数值本身（本单元只复核"真实数据上的 E 是多少"，没有重跑 EXP-06）；
  3. EXP-07 的缺陷数值（RC1 弦偏差 6.389e-2 hp_res、接缝地板 1.4e-16 sr、RC3 3.7e-3 等）
     ——本单元只做**适用域距离**测量，未重跑任何 EXP-07 探针；
  4. 帧 manifest 的 `astrocs_support_clamped_pixels = 23,901,708`（≈99.89% 覆盖叶的
     support 被 clamp 到 1）究竟是真实几何超出还是浮点舍入，**未判定**，只登记；
  5. 帧 drizzle 总面积 3.644293e-4 sr 与 WCS 名义面积 3.6874e-4 sr 相差 ~1.17%，
     是否来自无效源像元**未判定**，只登记。
- **本单元没有做的事**：没有重跑三命令端到端；没有改 `lib/**`、`docs/**`、`eng/**`；
  没有放宽任何阈值；没有 git 写操作。
- **已知偏差方向**：C2 的真值方差代理（跨帧配对差）把帧间系统差计入噪声 ⇒ E 被**高估**；
  即真实 E 可能比 1.1432 小，但仍远大于 1e-4 的门（因为产品权重在信号维上是常数）。
- **单一天区**：只有 M42（dec −3.8°…−7.0°）一个天区，赤道带；极区/接缝结论是**适用域排除**，
  不是"在极区也正确"。

---

## 7. 复现命令

```bash
# 一键复现（四个判据脚本 + 结果快照），只读既有端到端产物
bash 实验/m42-realdata/code/run_all.sh

# 单跑
cd 实验/m42-realdata/code
python3 c1_photometry.py      # -> ../results/c1_photometry.json
python3 c2_absolute_snr.py    # -> ../results/c2_absolute_snr.json
python3 c3_seam_additive.py   # -> ../results/c3_seam_additive.json
python3 c4_leaf_allocation.py # -> ../results/c4_leaf_allocation.json

# 完整性锚
sha256sum -c 实验/m42-realdata/results/SNAPSHOT.sha256
```

日志落 `run/M42-REALDATA-01/logs/`。

---

## 8. 佐证来源（规范依据逐条锚定）

| 结论 | 规范/权威出处 |
|---|---|
| `k_photo` 是线性乘性标度 | `ASTROCS_DESIGN.md:120-128` |
| 双边界判据形态与 EXP-04 数值 | `实验/photometric-magnitude/REPORT_paper.md:71-80`、`:147-149`、`:213` |
| 组间散度参考值 0.02 dex 与"帧间独立"裁决 | `lib/infrastructure/scheduler/src/module_adapters.cpp:4377`、`:4826-4838` |
| `sigma_residual` 定义（MAD/0.6745） | `lib/algorithms/photometry/cpp/src/star_matcher.cpp:612-621` |
| 噪声律与 SNR 链 | `docs/science/PHASE2_UPM.md`、`docs/science/SNR_CHAIN.md`、`lib/algorithms/coverage/src/integrate.cpp:20-79` |
| E 的定义与参考值 | `实验/absolute-snr/code/exp05/exp05_common.py:393-408`、`实验/absolute-snr/docs/EXP-06-SUMMARY.md`、`results/exp06_e1_analytic.json` |
| 权威接缝判据本体 | `实验/additive-sky-seamless/code/sci_c_common.py:356-411` |
| 纯加性天光模型与"保留背景" | `docs/science/PHASE2_UPM.md:9-10`、`:186-190`、`ASTROCS_DESIGN.md:139-145` |
| FP64 通量闭合门 1e-6 | `docs/algorithms/DRIZZLE_GEOMETRY.md:235-237` |
| 构造闭合与 `w_jp = a_jp/A_pixel` | `ASTROCS_DESIGN.md:147-159`、`docs/algorithms/DRIZZLE_GEOMETRY.md:41-57` |
| 逐样本掩码为唯一可判据载体 | `lib/infrastructure/scheduler/src/module_adapters.cpp:8960-8962` |
| EXP-07 缺陷域与阈值 | `实验/healpix-polar/docs/EXP-07-POLAR.md:412`、`:650-657`、`:670-673`、`:677` |
| HEALPix 公式 | `lib/algorithms/shared/healpix/healpix_core.cpp:155-226`（源自 astrometry.net `healpix.c`，BSD-3） |

---

## 9. 目录

```text
实验/m42-realdata/
  README.md              本文件（八要素）
  REPORT_paper.md        论文体报告（面向发表）
  code/                  固定 seed、可独立复跑
    m42_common.py        公共路径/常量/HEALPix/统计/判据台账
    seam_criterion.py    权威接缝判据本体逐字副本
    c1_photometry.py     SCI-A 判据
    c2_absolute_snr.py   SCI-B 判据
    c3_seam_additive.py  SCI-C 判据
    c4_leaf_allocation.py 创新点四判据
    run_all.sh           一键复现
  results/               c1..c4 的 JSON 结果 + SNAPSHOT.sha256 完整性锚
  docs/
    CRITERIA.md          判据目录：规范依据 / 红绿证据 / 负例注入 / 差异来源分析
    DATA-SOURCES.md      产品字段与二进制布局清单
```
