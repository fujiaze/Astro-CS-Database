# ACSD P1–P5 科学实验 · 回传报告（SCIENCE_HANDOFF_REPORT）

> **状态：DRAFT / 未定稿。** 本文件在 S05（P2）与 S07（P5 乘性增益）执行者仍在产出时起草，
> 供 ChatGPT 提前审阅结构与已确证结论；**定稿版将在 S08/S09 收口后用同一路径覆盖**，
> 并在定稿时把本文件的 `HEAD`、commit 列表、逐篇主张状态与未决清单刷新到最终值。
>
> - 分支：`audit/acsd-science-docs-p5`
> - 交接基线（工作包 pinned）：`e31b2be66ffe37d547e67a4ba007e7257e49789d`
> - 起草时 HEAD：**`d025932d1`**（完整 SHA 见表 A）
> - 仓库：`https://github.com/fujiaze/Astro-CS-Database`
> - 科学正本：`docs/ACSD_DESIGN.md`、`docs/science/**`（本报告不改这些文件）
> - 原始 FITS 一律未上传（仅索引、哈希与头部摘要）；`testdata/` 零改写

---

## 0 一页结论

1. **两处结构性生产缺陷已复现、修复、验证并推送**：
   P3 逐 drop 完备性 `ε_j`（两处近简并叉积条件数）与 P5 断连重叠图逐分量 gauge。
2. **五篇论文的正文（`REPORT_paper.md`）本轮**未改**——`S08` 尚未执行**。本报告只登记实验读数与
   证据级别，不改写论文，也不改 `docs/science/**` 判据文字。
3. **每个已跑项目都带阴性/阳性对照**；无法跑的一律 `NOT_RUN`；无外部证据的一律 `UNVERIFIED`。
4. **仍阻断发布的项目**见 §7；**需负责人裁决**的见 §8。

---

## 1 提交清单（表 A，按时间序）

| # | commit | 主题 | 主要变更面 |
|---|---|---|---|
| 1 | `6e0aa96ff` | science(S00)：真实帧轻量索引与哈希清单 | `实验/shared/data_catalog.json` |
| 2 | `183b83c73` | fix(drizzle)：P3 `ε_j` 的两处近简并叉积缺陷 | `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp` |
| 3 | `9f1e99f2a` | test(p3)：drop 边法向精度律的独立 Oracle | `实验/healpix-polar/code/s02_p3_normal_precision_oracle.py` |
| 4 | `f679a34bb` | test(p3)：1500 格独立并行回归矩阵 + 微小多边形面积误差源订正 | `实验/healpix-polar/{REPORT_experiment.md,code,docs}` |
| 5 | `449278205` | refs(S01)：修文献登记件字面量换行缺陷 + 机器可核验引文审计 | `实验/shared/references/**` |
| 6 | `120e75c28` | fix(coverage)：P5 断连重叠图改为逐分量 gauge 并加逐分量判据 | `lib/algorithms/coverage/src/sky_plane.cpp` |
| 7 | `535f891e7` | fix(evidence)：订正 S03 逐分量诊断过期读数 + 全图/逐分量判据分裂对照 | `实验/additive-sky-seamless/**` |
| 8 | `061271253` | test(p1)：S04 复验专节 + 可复跑脚本 | `实验/photometric-magnitude/**` |
| 9 | `fd52161d9` | test(p5)：S03 执行者 canonical 证据与回归脚本 | `实验/additive-sky-seamless/**` |
| 10 | `a73f577b0` | docs(p5)：S03-exec 附录 + Lead 七条裁决 | `实验/additive-sky-seamless/REPORT_experiment.md` |
| 11 | `4a0a3dc4a` | test(S09)：两处生产补丁的测试集零回归基线对照 | `实验/engineering-evidence/acsd-science-docs-p5/S09_TEST_REGRESSION_BASELINE.md` |
| 12 | `d025932d1` | test(p4)：S06 复验专节 + P4 证据 JSON + 可复跑脚本 | `实验/dense-snr-reconstruct/**` |

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
| S05 | P2 噪声/协方差/真实 coverage | **执行中** | `run/ACSDSDOC-02/S05/**`（尚无报告与章节） |
| S06 | P4 重建统计目标与域外失败 | **完成** | `run/ACSDSDOC-02/S06/REPORT_S06.md` + `实验/dense-snr-reconstruct/**`（§9） |
| S07 | P5 乘性增益研究 | **执行中** | `run/ACSDSDOC-02/S07/**`（本轮启动） |
| S08 | 五篇论文结果/跨篇传递 | **未开始** | —（论文正文本轮**未改**） |
| S09 | 构建/端到端/回传 | **部分** | 零回归对照已落；`normalize→mosaic→export` 端到端与回传定稿 **未做** |

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

## 5 逐篇证据级别（本轮，**不改论文**）

| 篇 | 被估量 | 本轮新增证据 | 当前证据级别 | 阻断未闭合项 |
|---|---|---|---|---|
| **P1** | `F_syn` vs `F_instr`、逐帧零点、颜色/空间残差 | 合规 8 帧 `σ_obs` 0.0253–0.0421 mag；色轴 `−0.0534 mag/mag(R−B)`（−10.05σ），跨仪器留出中位降 25.0%；P1-D 5/5 判红 | `ORACLE_CHECKED` + `EXPERIMENT_CHECKED`（真帧） | 线性区 `NOT_MEASURED`；`gain/RN/饱和电平` `UNVERIFIED`；论文数与 HEAD 代码版本不符（§8 裁决 4） |
| **P2** | 条件 `SNR_ref`、协方差、真实 coverage | **执行中（S05）** | `UNVERIFIED` | 117 星 coverage、`k_corr` 单位与标定域、`sparse_snr_layer` 落地状态 |
| **P3** | `w_jp=a_jp/A_drop,j`、逐 drop 完备性 | 两处缺陷修复；1500 格矩阵 218→0 破门；连通图/叶内逐位不变 | `ORACLE_CHECKED` + `PRODUCTION_CHECKED`（负例判红） | `60″–290″` 叶弦/分支残差未修；FP32 未复测；`docs/science` 判据文字未改 |
| **P4** | 固定 `F_ref` 的 `V_ref(x)`、支持域重建 | 过冲 `2.011325955375069`（越节点 `+1.1326e-02`、下冲 `−0.307920`）；域外 `55.5556%`/`43.7500%`；clamp 非无偏；NaN 静默填充；`idw_power` 生产 `2.0` vs 规范 `1.0` | `ORACLE_CHECKED` + `PRODUCTION_CHECKED`（探针） | `idw_power` 分裂未修（§8 裁决 5）；真实控制点链未跑 |
| **P5** | `y_i=g_i S+a_i+noise`、重叠连通分量 | 逐分量 gauge 修复（214×）；判据分裂对照；乘性**研究臂执行中（S07）** | 加性：`IMPLEMENTATION_CHECKED` + `PRODUCTION_CHECKED`；乘性：`UNVERIFIED` | 跨分量绝对零点需外部锚；乘性默认恒等**未验证**；边界 114/196 未审计 |

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

1. **S05（P2）未完成** ⇒ 条件 `SNR_ref` 与真实 coverage 的关系未闭合，`k_corr` 仍 `UNCALIBRATED`。
2. **S07（P5 乘性）未完成** ⇒ 乘性增益必须保持**默认恒等**；阴性对照历史「8/10 也改善」未推翻。
3. **P5 跨分量绝对零点无外部锚** ⇒ 产品不得声称跨分量定标。
4. **P1 探测器线性区 `NOT_MEASURED`**，且论文 §5 数字对应旧版代码。
5. **P3 大 drop 叶弦/分支残差未修**（不破 `1e-6`，但属未声明适用域）。
6. **S08 未做** ⇒ 五篇论文正文尚未更新结果/图表/证据级别；`docs/science/**` 判据文字未同步。
7. **端到端未跑** ⇒ `normalize→mosaic→export`、C++ 单测（仓库无 `add_test` 注册）、
   FP32、Windows 构建一律 `NOT_RUN`。
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
| 9 | **S01 逐页精读** | 仅书目级 + 一致性审计 | 哪些引用需要 `MATH_CHECKED`（目前正文无式号/页码断言） |

---

## 9 `NOT_RUN` 清单（不得由其他读数推断）

FP32 生产实例复测（P3/P5）；Windows 构建与 MSVC 复测；`normalize→mosaic→export` 端到端；
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

## 11 定稿前待补

1. 写入 S05（P2）与 S07（P5 乘性）的报告与章节，并刷新 §3、§5、§7、§8。
2. 执行 S08：按 `03_SCIENTIFIC_ACCEPTANCE.md` 的最低图表集更新五篇 `REPORT_paper.md`
   （Table 1–4、Figure 1–4；无实测数据的图只留图题与待跑说明）。
3. 执行 S09：`normalize→mosaic→export` 端到端、C++ 侧回归、本文件定稿并去掉 DRAFT 标记。
