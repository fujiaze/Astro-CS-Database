# ACSD P1–P5 科学实验 · 回传报告（SCIENCE_HANDOFF_REPORT）

> **状态：交接定稿快照（FINAL AS SUBMITTED；后经独立复核补正）**。S00–S09 的规定执行件已经交付，五篇论文均有结果章节；“执行已完成”不等于科学结论已通过或完整 E2E 已验收。以下记录的 SHA 为历史快照，当前分支可能因审查提交而前移；独立复核与尚待处置的完整清单见 [INDEPENDENT_REVIEW_8ecaac427.md](INDEPENDENT_REVIEW_8ecaac427.md)。
>
> - 分支：`audit/acsd-science-docs-p5`
> - 交接基线（工作包 pinned）：`e31b2be66ffe37d547e67a4ba007e7257e49789d`
> - 五篇论文最后一次更新提交：**`bab607107`**（表 A 第 38 项）；首次 FINAL 报告提交快照：**`8ecaac427d8d4466b5213d0dd6a6ad1a9`**（第 39 项）。它们不是本文件再次修改后的动态 HEAD。
> - 仓库：`https://github.com/fujiaze/Astro-CS-Database`
> - 科学正本：`docs/ACSD_DESIGN.md`、`docs/science/**`（本报告不改这些文件）
> - 原始 FITS 一律未上传（仅索引、哈希与头部摘要）；`testdata/` 零改写

---

## 0 一页结论

1. **两处结构性生产缺陷已复现、修复、验证并推送**：
   P3 逐 drop 完备性 `ε_j`（两处近简并叉积条件数）与 P5 断连重叠图逐分量 gauge。
2. **五篇 `REPORT_paper.md` 已按 S08 更新**，分别在 `09b14b13d`（P1/P3/P4）及 `bab607107`（P2/P5）提交；各篇的**跨篇误差传播实跑仍为 `NOT_RUN`**，不能因论文表格齐备就宣称五篇科学真值闭合。本任务没有修改 `docs/science/**` 正本。
3. **每个已跑项目都带阴性/阳性对照**；无法跑的一律 `NOT_RUN`；无外部证据的一律 `UNVERIFIED`。
4. **仍阻断发布的项目**见 §7；**需负责人裁决**的见 §8。

---

## 1 提交清单（表 A，按时间序）

| # | commit | 主题 |
|---|---|---|
| 1 | `6e0aa96ff` | science(S00): 建立 P1-P5 实验用的真实帧轻量索引与哈希清单 |
| 2 | `183b83c73` | fix(drizzle): 修 P3 逐 drop 完备性 epsilon_j 的两处近简并叉积条件数缺陷 |
| 3 | `9f1e99f2a` | test(p3): 补 drop 边法向精度律的独立 Oracle（不链接生产库） |
| 4 | `f679a34bb` | test(p3): 补 S02 独立并行回归矩阵（1500 格）与微小多边形面积误差源订正 |
| 5 | `449278205` | refs(S01): 修文献登记件字面量换行缺陷并加机器可核验引文审计 |
| 6 | `120e75c28` | fix(coverage): P5 断连重叠图改为逐分量 gauge 并加逐分量判据 |
| 7 | `535f891e7` | fix(evidence): 订正 S03 逐分量诊断的过期读数并补全图/逐分量判据分裂对照 |
| 8 | `061271253` | test(p1): S04 第二轮复验专节 + 可复跑脚本（P1 真实帧光度与颜色误差归因） |
| 9 | `fd52161d9` | test(p5): 补 S03 执行者的 canonical 证据与回归脚本 |
| 10 | `a73f577b0` | docs(p5): 提交 S03-exec 附录并落 Lead 对 S03 的七条裁决 |
| 11 | `4a0a3dc4a` | test(S09): 记录两处生产补丁的测试集零回归基线对照 |
| 12 | `d025932d1` | test(p4): S06 复验专节 + P4 证据 JSON + 可复跑脚本（重建统计目标与域外失败） |
| 13 | `448b76163` | docs(S09): 起草 SCIENCE_HANDOFF_REPORT（DRAFT，含 12 个 commit 清单与逐篇证据级别） |
| 14 | `eca227f10` | chore(p4): 采纳 S06 台账刷新并入库实测图（bookkeeping-only 变更） |
| 15 | `f35e4d120` | docs(S09): 记录端到端 9/9 判据静态跳过的原因与阻塞 |
| 16 | `4bb8ae219` | docs(S09): normalize->mosaic->export 真实帧端到端实跑证据（三命令 rc=0） |
| 17 | `b894e9048` | docs(S09): 刷新回传报告 DRAFT（端到端已跑通、P2 中间判红读数、E2E.* 静态跳过） |
| 18 | `c93b4a6f9` | test(p2): 入库 S05 可复跑脚本与实测图（P2 证据的可复算面） |
| 19 | `0ddd5a710` | docs(S08): Lead 复核记录——H3 论文表 4 第 6 行 pre/post 非同构型（必须修正） |
| 20 | `c825ac5c9` | docs(S08): 复核记录追加 R-4（P4 流程叙事）与 R-5（逐篇来源列须真含所列数值） |
| 21 | `021858c7e` | test(p2): S05 复验专节 + P2 证据 JSON（四臂 Oracle / k_corr 未标定 / 117 星真实复核） |
| 22 | `1b7a61ea8` | docs(p2): 入库 S05_REPORT.md（逐字镜像，唯一新增未提交件） |
| 23 | `92416c767` | docs(S09): 回传报告补入 P2 两项阻断裁决（k_corr 方向性缺陷、2.7928 表述分歧） |
| 24 | `09b14b13d` | docs(S08): 更新 P1/P3/P4 三篇论文结果·图表·证据级别（含 Lead 复核修正） |
| 25 | `cec31850d` | test(p5): S07 乘性增益研究证据（status=FAIL，生产默认须保持恒等） |
| 26 | `35de8385b` | docs(S09): 刷新回传报告提交清单至 25 项并声明证据级别命名映射 |
| 27 | `48e5041e9` | docs(S08): FIVE_PAPER_EXPERIMENT_PLAN 补第九节验收对照表（逐篇对 G-SCI/G-FAIL/G-REPRO/G-RELEASE） |
| 28 | `e1d80dcd7` | docs(p5): 提交 S07-exec 附录（接线审计/研究臂/假阳性判红/次序发现/边界分原因/诚实边界） |
| 29 | `9c38a1908` | chore(p5): 刷新 S07 证据台账的 head_at_bundle_time（bookkeeping-only） |
| 30 | `e07c67267` | docs(S09): 状态表刷新 S05/S07 为完成、S08 为部分完成 |
| 31 | `de8ce0dda` | docs(S09): 回传报告 §5 填写 P2/P5 两行（依据已提交的 S05/S07 证据） |
| 32 | `0b3727335` | docs(S09): 登记高风险未修项——P5 论文仍含被裁决禁止的 12.8% 表述（8d） |
| 33 | `6b5070c7e` | fix(p5): 订正论文摘要中被裁决禁止的 12.8% 接缝表述（Lead 直接修正，风险优先） |
| 34 | `912b9f98b` | docs(S09): 标记 8d 已解决（P5 的 12.8% 禁止表述已由 6b5070c7e 订正） |
| 35 | `dda7c813c` | docs(p5): 入库 S07_REPORT.md（从被 .gitignore 忽略的 run/ 镜像） |
| 36 | `fd3d6ad15` | docs(S09): 刷新回传报告 §11 待补清单并补 §12 交付纪律核对（刷新至 dda7c813c） |
| 37 | `2334ddce4` | docs(S09): 增补 RESUME_NOTES（断点续做说明） |
| 38 | `bab607107` | docs(S08): 更新 P2/P5 两篇论文结果·图表·证据级别（S08 五篇完成） |
| 39 | `8ecaac427` | docs(S09): 回传报告由 DRAFT 转为 FINAL（本次交接快照） |

`lib/` 全局改动面仅两个文件：`spherical_overlap.cpp`（P3）与 `sky_plane.cpp`（P5）。
**未合并 `main`、未开 PR、未 force-push。**

---

## 2 环境与构建

| 项 | 值 |
|---|---|
| OS / 内核 | Linux 6.12.107+deb13-amd64 x86_64 |
| 编译器 | g++ 14.2.0（Debian） |
| CMake / Ninja | 3.31.6 / 1.12.1 |
| Python / 科学栈 | 3.13.5；numpy 2.2.4、scipy 1.15.3、astropy 7.0.1、healpy 1.20.1 |
| 硬件 | 16 核 / 23 GiB |
| 生产构建 | `cmake -S . -B build/linux-sci -G Ninja -DCMAKE_BUILD_TYPE=Release`，版本串 `0.1.0-alpha.1+ge31b2be66ffe37d547e67a4ba007e7257e49789d` |
| 独立构建（交叉核对） | `build/linux-sci-p5`（P5）、`build/linux-sci-s05`（P2）、`build/linux-sci-s07`（P5 乘性） |

**测试集零回归（`4a0a3dc4a`）**：同一子集在基线 `e31b2be66` 与 HEAD 上均为 **29 failed / 152 passed**，
差异计数 **0**。29 项失败全部是既存 `C2/ANCHOR_STALE` 违约，指向
`lib/infrastructure/scheduler/src/module_adapters.cpp`（**不在本轮任何补丁改动面内**）。

---

## 3 任务状态总表

| ID | 主题 | 状态 | 证据落点 |
|---|---|---|---|
| S00 | 基线/环境/真实数据索引 | **完成** | `实验/shared/data_catalog.json`（906 帧 + 3 HST + 27 母版；31 帧缺 WCS 逐帧登记） |
| S01 | 文献主张与公式双向审查 | **部分完成** | 引文一致性机器审计（`S01_CITATION_AUDIT.json`）；**全文逐页 MATH_CHECKED 未做** |
| S02 | P3 几何/守恒 Oracle 与修复 | **完成** | `run/ACSDSDOC-02/S02/REPORT_S02.md` + `实验/healpix-polar/**`（附录 A/B/C） |
| S03 | P5 连通分量 gauge 与回归 | **完成** | `run/ACSDSDOC-02/S03/REPORT_S03.md` + `实验/additive-sky-seamless/**`（附录 S03/S03-exec/S03-裁决） |
| S04 | P1 真实帧光度与误差归因 | **完成** | `run/ACSDSDOC-02/S04/REPORT_S04.md` + `实验/photometric-magnitude/**`（§11） |
| S05 | P2 噪声/协方差/真实 coverage | **完成** | `实验/absolute-snr/docs/{S05_P2_evidence.json,S05_REPORT.md}` + `code/s05/` + `figures/`（4 图）+ `REPORT_experiment.md` 追加 6 节（`021858c7e`、`1b7a61ea8`） |
| S06 | P4 重建统计目标与域外失败 | **完成** | `run/ACSDSDOC-02/S06/REPORT_S06.md` + `实验/dense-snr-reconstruct/**`（§9） |
| S07 | P5 乘性增益研究 | **完成（结论 `FAIL`）** | `实验/additive-sky-seamless/docs/S07_P5_GAIN_evidence.json` + `docs/S07_results/`（5 件）+ `code/s07_*.py` + `REPORT_experiment.md` 附录 S07（`cec31850d`、`e1d80dcd7`） |
| S08 | 五篇论文结果/跨篇传递 | **完成** | P1/P3/P4（`09b14b13d`）、P2/P5（`bab607107`）；验收对照表 `48e5041e9`；跨篇反例五篇均如实 `NOT_RUN` |
| S09 | 构建/端到端/回传 | **执行与回传完成；完整科学验证未完成** | 子集测试基线与 HEAD 同为 `29 failed / 152 passed`（仅数量对照，缺逐用例 nodeid diff）；真实帧 **CLI 三命令 rc=0**（只验证链路，冒烟 master_scale 不能作科学定标）；`eng/tests/e2e` 的 9 条 **静态跳过**；报告已定稿但须结合独立审查补正阅读 |

---

## 4 两处生产修复（本轮的科学与工程核心）

### 4.1 P3：逐 drop 完备性 `ε_j` 的两处近简并叉积缺陷（`183b83c73`）

**被估量**：`ε_j = Σ_p a_jp / A_drop,j − 1`（无量纲；`a_jp` = drop∩叶球面交叠面积 [sr]）。
生产探针 1×1 帧 `custom_x≡1` 下 `sumw_m1 ≡ ε_j`（`drizzle_engine.cpp:1402/1424` 逐行核对），
故与正本 `DRIZZLE.md` 的完备性判据**同物量**。

**独立 Oracle（不复用生产裁剪）**：精确切平面 2D 裁剪 `ε = +1.33e-15`；drop 内 4001² 网格对四叶
四角弦做半空间测试，四叶分数和 **`1.000000`** ⇒ **叶边界弦表示恰铺满 drop，不是成因**。

**根因（两处，缺一不可）**

| 臂（顶点构型：RA=45°、dec=asin(2/3)、nside 8192、0.04″/px、pixfrac 0.8） | `ε` |
|---|---|
| 修前 HEAD | **`−3.209422e-03`** |
| 只修 P3-NORM-01（drop 边法向） | `+5.499704e-05` |
| **P3-NORM-01 + P3-NORM-02（S-H 交点）** | **`−8.370272e-10`** |

- **NORM-01**：`n = normalize(cross(C_j,C_{j+1}))`，`|cross|≈θ` 而分量是 O(1) 乘积之差 ⇒ 平面位置
  相对误差 `~2ε_mach/θ²`。修法用恒等式 `P1×P2 ∝ c×(α2−α1) + α1×α2`（`α_i = P_i/(P_i·c) − c`）。
  实测四条边 `n·c` 由最大 0.45% 偏差变为四个**逐位相同**值。
- **NORM-02（该构型主残差）**：`compute_intersection` 的 `cross(cross(S,E), n_clip)` 在 S-H 迭代出
  `~1e-7 rad` 短边时同样近简并。改为弦-平面零点 `(d_S·E − d_E·S)/(d_S − d_E)`（调用点恒有异号 ⇒ 分母无相消）。

**全矩阵（1500 格：24 位置类 × nside{512,4096,8192,16384} × pixfrac{0.5,0.8,1} × 尺度{0.04″,0.5″,2″,10″,60″}）**

| 臂 | `max\|ε_j\|` | 破 `1e-6` 格 |
|---|---|---|
| 修前 | `1.5649e-02` | **218 / 1500** |
| 只修 NORM-01 | `2.3783e-03` | 172 / 1500 |
| **修后** | **`1.3755e-08`** | **0 / 1500** |

**误差律**：修前 `ε ∝ ε_mach/θ²`（翻转阈值 `θ≈1.5″`）；修后 `ε ∝ ε_mach/θ`，
`θ_1e-6 = (5.8–8.3)e-05″`，**低于产品已声明失效域**（`A_drop<1e-20 sr`）⇒ 全受检域可达 `1e-6` 门。

**负例判红**：`ACSD_DRZ_P3_FAULT=legacy_cross_normal`（同时回退两处，静态缓存零开销）与修前臂
**1537 条记录逐位相同**，注入路径 218 格破门 ↔ 默认 0 格。

**未修**：`60″–290″` 档 `~2e-7`（峰值 `3.29e-7`，3 倍余量）由叶四角弦表示与切平面/球面分支
硬跳变支配，**不破门、未修、不得外推**。生产 FP32 实例未单独复测。

### 4.2 P5：断连重叠图逐分量 gauge（`120e75c28`）

**缺陷**：前向模型 `y_k = B_ref + δ_k` 只约束组合；每个连通分量内存在规范自由
`B_ref → B_ref + f`、`δ_k → δ_k − f`。生产原用 `sky_plane.cpp:1506 model->ref_frame = 0` 的
**全局单一**参考帧，只固定分量 0 的规范，compB 漂移被 `B_ref` 吸收进产品，而 `rms_weighted`/`χ²_red`
**结构性失明**。

**修法**：以**实际有效控制点**（`used` 样本共享 `control_id`）建帧重叠图 → 并查集枚举连通分量 →
**每分量取分量内最小 `frame_id` 作锚帧，`δ_anchor ≡ 0`**；`ref_frame` 降为兼容字段。新增
逐分量状态旁路 `ACSD_SKY_COMPONENT_DUMP`、逐分量数据部分秩判据、求解扰动敏感性、
显式规范约束核对；负例开关 `ACSD_SKY_P5_FAULT=global_anchor`。

| 场景 | 修前 rc | 修后 rc | 关键读数 |
|---|---|---|---|
| `S9b`（两分量各 2 帧） | 0 | 0 | compB `1.281497171821252e-1 → 5.981665755358275e-4`（**214×**）；全图 κ `3.89e9 → 3.66e4` |
| `S12_chain4` / `S14_thin_bridge_1cell`（连通） | 0 | 0 | **`model_hash` 逐字相同** ⇒ 补丁只作用于断连图 |
| `S0`（两单帧分量） | 7 | 7 | 保守方向保持 |
| `S9`（两帧分量 + 单帧分量） | 0 | **7** | **有意行为变更**（见 §8 裁决 1） |
| `S13_thin_bridge`（零有效边） | 0 | **7** | 修前「全局单锚」把零有效边的两分量**静默桥接**成一个产品 |

**逐分量判据分裂（本修复的关键价值）**：故障注入下

| 注入臂 | comp0 | comp1 | 全图 |
|---|---|---|---|
| `rank/n_free` | `160/160` | **`118/127`** | `256/256` |
| `n_unidentified` | 0 | **9** | **0** |
| `identifiable` | 绿 | **红** | **`identifiable=1`（绿）** |
| `κ` | `1.33e4` | — | **`3.89e9`（仍 < `1/τ=1e10`，绿）** |

⇒ **全图秩、`identifiable`、条件数全部读绿，只有逐分量判据抓到缺陷**，实证 `REVIEW_DECISIONS`
D-02 §3/§4 的「不得仅用 `κ < 1/τ` 判绿」。

**产品级限制（必须随结论引用）**：`S9b` 两分量间 `x∈(150,170)` 无重叠，修后电平差
`18.84 ADU（6.07%）`＝两分量锚帧加性偏置之差，属**不可辨识的跨分量绝对零点** ⇒ 修后产品
**不得**被引用为「已跨分量定标」；逐分量 `identifiable=1` 只声明**分量内**可辨识；
仍**需要外部锚**（**未实现**）。

---

## 5 逐篇证据级别（**五篇论文已在 S08 更新，科学结论仍按本表分级**）

> **证据级别命名统一（本任务发现的口径不一致，在此声明映射）**：`03_SCIENTIFIC_ACCEPTANCE.md` 定义六级为
> `BIB_VERIFIED / SELF_CHECK / ORACLE_CHECKED / IMPLEMENTATION_CHECKED / EXPERIMENT_CHECKED / RELEASE_VERIFIED`。
> 但部分单元证据件（如 `实验/healpix-polar/docs/S02_P3_*.json` 的 `status`、本报告早期草稿）曾使用
> **`PRODUCTION_CHECKED`** 这一**不在六级内**的名称。本文档与五篇论文一律按**六级**书写，其中
> **`PRODUCTION_CHECKED` ≡ `IMPLEMENTATION_CHECKED`**（生产源码 + 可追溯调用链 + 可执行负例）。
> 历史证据件**不改写**（AGENTS §13.2：过程记录不得覆盖），仅在此声明映射；**五篇论文中不得出现 `PRODUCTION_CHECKED`**。

| 篇 | 被估量 | 本轮新增证据 | 当前证据级别 | 阻断未闭合项 |
|---|---|---|---|---|
| **P1** | `F_syn` vs `F_instr`、逐帧零点、颜色/空间残差 | 合规 8 帧 `σ_obs` 0.0253–0.0421 mag；色轴 `−0.0534 mag/mag(R−B)`（−10.05σ），跨仪器留出中位降 25.0%；P1-D 5/5 判红 | `ORACLE_CHECKED` + `EXPERIMENT_CHECKED`（真帧） | 线性区 `NOT_MEASURED`；`gain/RN/饱和电平` `UNVERIFIED`；论文数与 HEAD 代码版本不符（§8 裁决 4） |
| **P2** | 条件 `SNR_ref`、协方差、真实 coverage | **已完成（S05）**。四臂独立 numpy Oracle（不链接生产库）20000 次 MC + Wilson 95% 区间；真实帧 **2195 星对**覆盖率 nominal `0.6827` → 实测 **`0.4752`**（去趋势 `0.4639`），`contains_nominal=false`；多余散度由**与 SNR 无关的系统项**支配（SNR 分层地板 `0.036–0.070 mag`）；空间共模超额 `37.9×`、有效独立样本 **64 块**；`k_corr` 单位已由 `hp_drizzle_api.cpp:1112-1113` 原文闭合为**源帧像元角尺度 `[arcsec/px]`**，但生产几何 `rho≈0.83` 下真值 `2.05–2.33` 而冻结回退仅 `1.4`| `ORACLE_CHECKED` + `IMPLEMENTATION_CHECKED`（探针）+ `EXPERIMENT_CHECKED`（真实帧）；**`k_corr` = `UNCALIBRATED`**；`sparse_snr_layer` = `NOT_IMPLEMENTED` | `k_corr` 三选一处置；`2.7928` 表述须改为分层读数；真实帧无平场/暗场改正；`tile_order=12` 域 `k>38` |
| **P3** | `w_jp=a_jp/A_drop,j`、逐 drop 完备性 | 两处缺陷修复；1500 格矩阵 218→0 破门；连通图/叶内逐位不变 | `ORACLE_CHECKED` + `IMPLEMENTATION_CHECKED`（生产负例判红） | `60″–290″` 叶弦/分支残差未修；FP32 未复测；`docs/science` 判据文字未改 |
| **P4** | 固定 `F_ref` 的 `V_ref(x)`、支持域重建 | 过冲 `2.011325955375069`（越节点 `+1.1326e-02`、下冲 `−0.307920`）；域外 `55.5556%`/`43.7500%`；clamp 非无偏；NaN 静默填充；`idw_power` 生产 `2.0` vs 规范 `1.0` | `ORACLE_CHECKED` + `IMPLEMENTATION_CHECKED`（生产探针） | `idw_power` 分裂未修（§8 裁决 5）；真实控制点链未跑 |
| **P5** | `y_i=g_i S+a_i+noise`、重叠连通分量 | **已完成（S03+S07）**。加性：逐分量 gauge 修复，`S9b` compB `1.281497171821252e-1 → 5.981665755358275e-4`（`214×`），连通图 `S12_chain4`/`S14_thin_bridge_1cell` 的 `model_hash` 修前修后**逐字相同**，负例 `ACSD_SKY_P5_FAULT=global_anchor` 判红；**判据分裂**——故障注入下全图 `rank 256/256`/`identifiable=1`/`κ=3.89e9` 全绿，只有逐分量判据抓到 comp1 `rank 118/127`、`n_unidentified=9`。乘性：阴性世界（无真实增益，10 seeds）联合 A+G 仍给统计显著「改善」（`+0.00968`，CI `[0.00681,0.01740]`，`frac_pos=1.0`）而 **oracle 增益给恰 `0.0`** ⇒ 假阳性 ⇒ 预声明 C3 不成立 ⇒ **`status=FAIL`**；接线审计 `P1_kphoto_mxy=APPLIED`、`P5_MA_scalar_solver=NOT_IMPLEMENTED_ON_PRODUCTION_PATH`、`P5_multiplicative_gain=IDENTITY` | 加性：`ORACLE_CHECKED` + `IMPLEMENTATION_CHECKED`（已上线）+ 逐分量诊断；乘性：**`FAIL`**（不得默认启用） | 跨分量绝对零点不可辨识、**需外部锚**（未实现）；乘性**次序缺陷**已登记未修；边界 196 条仅 114 条有效；真实帧乘性腿 `NOT_RUN` |

**S01 状态**：引文一致性为机器核验（15 条登记、五篇引用零悬空、13 条 DOI 一一对应、
**正文无任何式号/页码断言** ⇒ 无未核验的 MATH_CHECKED 主张）；**逐页全文精读未做**，
`R12`（PixInsight LocalNormalization）已登记但五篇正文**无一篇引用**。

---

## 6 数据与可复现性

- 真实帧索引：`实验/shared/data_catalog.json`（sha256 `14a1231f…`），906 亮场 + 3 HST drz + 27 母版，
  逐帧 SHA/头/WCS/滤镜/曝光。**31 帧头内无任何 WCS**（M42 30 + Galaxy_Center 1）逐帧登记为待查。
- HST `*_drz.fits` 仅作**结构模板**，其噪声不得当作未重采样独立原始探测器噪声真值；
  `CDELT1=1.0` 判为无效占位，另记由 CD 矩阵导出的 `0.0400004″/px`。
- **原始 FITS 零上传**；`testdata/` 零改写；大体积二进制中间产物留在 `run/`（gitignored）。
- 每项读数的命令、seed、输入哈希、构建 SHA 见各自的 `run/ACSDSDOC-02/S0*/REPORT_S0*.md`。

---

## 7 仍阻断发布的项目（`RELEASE_VERIFIED` 不成立）

1. **S05（P2）已执行，但科学校准未完成** ⇒ 真实 coverage 尚未在正确统计被估量下校准到标称；`k_corr` 仍 `UNCALIBRATED`，`sparse_snr_layer` = `NOT_IMPLEMENTED`。
2. **S07（P5 乘性）研究已执行，结论是 `FAIL`** ⇒ 阴性 `g=1` 时 10/10 seeds 误检增益，生产必须保持**默认恒等**；P5 MA 未接入生产。
3. **P5 跨分量绝对零点无外部锚** ⇒ 产品不得声称跨分量定标。
4. **P1 探测器线性区 `NOT_MEASURED`**，且论文 §5 数字对应旧版代码。
5. **P3 大 drop 叶弦/分支残差未修**（不破 `1e-6`，但属未声明适用域）。
6. **S08 五篇论文正文已更新结果/图表索引/证据级别**，但跨篇传播反例均 `NOT_RUN`，远端 Git 树无跟踪的 PNG/SVG 图件，P1/P3 部分结果文件仍只在被忽略的 `run/`；科学正本需要对剩余实现缺陷作证据支持的同步订正。
7. **端到端**：`normalize→mosaic→export` 已在真实帧上跑通（三命令 `rc=0`，产物经 astropy 独立读回，
   导出 FITS `1024×1024 >f4`、`RA---TAN`、`coverage 1048576/1048576`、`p3_verify.canonical_match=true`），
   但**该跑只验证链路与合同**：`master_scale` 为冒烟测试值、母版实际已归一化 ⇒ 导出绝对量级不可信。
   仍 **`NOT_RUN`**：C++ 单测（仓库无 `add_test` 注册）、FP32 端到端、Windows 构建、多 block/多帧相对定标、
   GUI 视觉验收、`acsd benchmark` 全量、`doctor` 的 avx512 腿（本机 ISA 不支持）。
   另：`eng/tests/e2e/` 的 9 条 `E2E.*` 判据**全部静态 `pytest.mark.skip`**（`requires: build/acsd`
   这一不存在路径 + 过期车道约束），其自带 A 类自洽守卫禁止改 skip 打开 ⇒ 不得引用为已通过。
8. **29 项既存测试失败**（`module_adapters.cpp` 锚点漂移）未修，不在本任务写面内。

---

## 8 需负责人/后续轮次裁决的事项

| # | 事项 | 现状 | 需要的裁决 |
|---|---|---|---|
| 1 | **P5 `S9`/`S13` 失败关闭** | 修后 `rc=7`（D-02 §2 明许「独立产品或显式失败/未可定标状态」） | 是否实现「单帧/未连分量作**独立产品**输出、其余分量照常成功」的替代分支 |
| 2 | **P5 跨分量绝对零点** | 需外部锚，未实现 | 是否引入外部测光锚；在此之前产品措辞边界 |
| 3 | **P4 `idw_power` 生产-规范分裂** | 生产有效 `2.0`、规范默认 `1.0` | 是否统一（改 `lib/**` 需单独授权） |
| 4 | **P1 论文数与代码版本不符** | `0.0265198375` 对应 `410f05f13` **之前**的代码；HEAD 给 `0.0268730673` | 论文 §5 补口径声明；`ABOVE_CEILING` 不得写作「科学不合格」（正本明示其为**下包络**） |
| 5 | **P1 `σ_flat_independent` 的 `N_eff` 折算** | 在相关平场上低估约 8 倍（直接口径 `0.0018–0.0023`） | 是否改用直接口径；`0.0007` 的登记措辞 |
| 6 | **31 帧缺 WCS 与 `index.json` 标 `available` 的语义冲突** | 逐帧登记未猜补 | 索引是否改标 |
| 7 | **P3 `docs/science` 判据/适用域文字** | 未改 | 是否按 `60″–290″` 残差与 `θ_1e-6` 边界同步订正 |
| 8 | **逐分量常驻状态** | 仅 env 旁路 + `save` 三键 | `P2SkyPlaneInfo` ABI 常驻化是否立项 |
| 8b | **P2 `k_corr` 方向性缺陷（阻断项）** | `scale_arcsec` 量纲核对**正确**（源帧像元角尺度 `[arcsec/px]`，D-05 红线未触碰），但物理驱动量是 `rho=叶/源`，而查表只收 `(pixfrac, scale_arcsec)`、**不收 nside**；标定件把输出固定在 `tile_order=0`。生产默认帧 `tile_order=9`（叶 `0.805″`）、真帧源 `0.9666″/px` ⇒ `rho≈0.83`。独立 MC + 生产球面权重探针给 **模型 `2.33` / 生产权重 `2.054–2.058`**，而**冻结回退只有 `1.4`** | 三选一：(a) 按 `rho` 标定（需把 nside 传入查表）；(b) 该域显式标 `UNCALIBRATED` 并降级权重语义；(c) 两因子 `k_gauss(N)·k_geo(rho,pf)` 落配置。**后果**：控制点方差被低估、逆方差权重被高估约 `1.5×`；保守域仅 `rho≳1.3` |
| 8c | **P2 与 `REPORT_paper.md` 的 `2.7928` 表述分歧** | 论文摘要写「117 颗…`2.7928`」；S05 独立重测 **2195 星**、比值 `3.37`（shot-only）/`2.73`（empirical），且证明多余散度由**与 SNR 无关的系统项**支配（亮端 `z_rms 5.1–10.3`） | 论文须改为**分层读数 + 经验地板层 + 适用域**，**不得**写成「独立像素噪声低估 `2.79` 倍」 |
| 8d | ~~P5 论文含已被裁决禁止的 `12.8%` 表述~~ **【已解决，见提交 `6b5070c7e`】** | `实验/additive-sky-seamless/REPORT_paper.md` 摘要（第 7 行）仍写「S9b 断连图第二分量的接缝约为 **12.8%**」；而 `REPORT_experiment.md` 的**附录 S03-裁决 2** 已明令该数对外一律写「**规范失控的读数症状**」，**不得**称「12.8% 物理接缝」（同位置真实不连续量仅 `−2.56/−3.99`，剖面斜率 `≈89.9/px`，分母是失控背景 `−5988.917`，分子 `step_abs=−767.478` 几乎是斜坡） | 已由 Lead 直接订正为「规范失控的读数症状」口径（并补入 S07 的阴性假阳性与字面次序两条实测结论），该禁止表述不再存在于论文中 |
| 9 | **S01 逐页精读** | 仅书目级 + 一致性审计 | 哪些引用需要 `MATH_CHECKED`（目前正文无式号/页码断言） |

---

## 9 `NOT_RUN` 清单（不得由其他读数推断）

FP32 生产实例复测（P3/P5）；Windows 构建与 MSVC 复测；**完整科学定标验证的** `normalize→mosaic→export`（**CLI 三命令冒烟已运行且 rc=0；9 个 E2E pytest 仍跳过**）；
C++ 单测（无注册）；`HP_CIRCUMRADIUS_FACTOR=1.25` 在 nside≥512 的全天穷举；
P3 `290″–3600″` 之外更粗像元域；P4 留一边残差重拟合、二维 footprint 的环、`gauge_mode=1` 多分量；
P5 乘法臂的二维 `g_i(x)`；P1 绝对透过率/消光、母版施加后残余平场、线性区拐点；
S01 逐页全文核对。

---

## 10 复现入口

```bash
# 生产构建（基线 pinned）
cmake -S . -B build/linux-sci -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux-sci -j 14

# P3：独立 Oracle + 生产探针（顶点构型；scale pixfrac nside 可调）
python3 run/ACSDSDOC-02/S02/code/p3_oracle.py
python3 实验/healpix-polar/code/s02_p3_oracle.py /tmp/s02.json
g++ -O2 -std=c++17 -D HP_DRIZZLE_STATIC -D AIO_ENABLE_FITS -D AIO_ENABLE_HEALPIX \
  -I lib/algorithms/drizzle/healpix_drizzle -I lib/infrastructure/aio/include \
  -I lib/infrastructure/aio/src -I lib/algorithms/shared -I lib/algorithms/shared/healpix \
  run/ACSDSDOC-02/S02/code/probe_p3_detail.cpp -o /tmp/probe_p3_detail \
  build/linux-sci/libacsd_drizzle.a build/linux-sci/libacsd_hips.a build/linux-sci/libacsd_aio.a \
  build/linux-sci/libacsd_common.a -lzstd -lz -fopenmp
/tmp/probe_p3_detail /tmp/new.json 0.04 0.8 8192
ACSD_DRZ_P3_FAULT=legacy_cross_normal /tmp/probe_p3_detail /tmp/legacy.json 0.04 0.8 8192

# P5：逐分量 gauge 回归 + 负例
python3 实验/additive-sky-seamless/code/s03_p5_component_regression.py \
    --probe run/ACSDSDOC-02/S03/code/p2_sky_joint_probe --tag post
ACSD_SKY_P5_FAULT=global_anchor python3 实验/additive-sky-seamless/code/s03_p5_component_regression.py \
    --probe run/ACSDSDOC-02/S03/code/p2_sky_joint_probe --tag fault_global_anchor S9b_two_components

# P4
python3 实验/dense-snr-reconstruct/code/s06/s06_b_overshoot.py
python3 实验/dense-snr-reconstruct/code/s06/s06_c_holdout_cv.py

# P1
python3 实验/photometric-magnitude/code/s04/s04_measure.py

# 测试集零回归对照（基线 worktree）
git worktree add -f /tmp/acsd-base e31b2be66ffe37d547e67a4ba007e7257e49789d
(cd /tmp/acsd-base && python3 -m pytest eng/tests/unit eng/tests/synthetic eng/tests/module \
    eng/tests/integration eng/tests/pipeline -q)     # 29 failed / 152 passed
python3 -m pytest eng/tests/unit eng/tests/synthetic eng/tests/module \
    eng/tests/integration eng/tests/pipeline -q      # 29 failed / 152 passed（HEAD，同）
```

---

## 11 定稿说明与后续审查

1. **S08 已完成**：五篇论文均按 `03_SCIENTIFIC_ACCEPTANCE.md` 的最低图表集更新并提交
   （P1/P3/P4 于 `09b14b13d`，P2/P5 于 `bab607107`），验收对照表见
   `实验/shared/FIVE_PAPER_EXPERIMENT_PLAN.md` §九。
2. **本报告已定稿**：表 A 提交清单、§3 任务状态、§5 逐篇证据级别、§7 阻断项、§8 裁决项均已刷新。
3. **跨篇误差传递反例五篇均 `NOT_RUN`**（§九已列缺口），**未**以单篇内部反例冒充。
4. **不随本任务闭合的登记项**：`k_corr` 三选一处置、`idw_power` 统一、乘性增益次序缺陷修复、
   跨分量外部锚、P3 未跑像元域、29 项既存测试失败（`module_adapters.cpp` 锚点漂移）、
   `eng/tests/e2e/` 的 9 条静态跳过判据——均**只登记未动**，需负责人另开任务。

