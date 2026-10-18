# ACSD 五篇科学论文：统一实验协议与复核清单

> 目的：为五篇现行 `REPORT_paper.md` 规定共同证据、互不重复的核心科学假设与研究/生产测试界面；**不是**第五篇之外的第六篇论文，也不是已完成结果报告。文献见 [核验引文库](references/CORE_REFERENCES_VERIFIED.md)，本轮审查裁决见 `../engineering-evidence/acsd-science-docs-p5/REVIEW_DECISIONS.md`。

## 一、五篇论文和跨单元依赖

| 论文 | 科学被估量与主要方法 | 首要创新性边界 | 阻断级科学问题 | 独立 Oracle |
|---|---|---|---|---|
| [P1 光度标定](../photometric-magnitude/REPORT_paper.md) | `F_syn` vs `F_instr`、`k_photo,m(x,y)`、星等散度 | Gaia XP 合成测光已有 R01/R02 先例，贡献应是接口+独立误差预算 | 真实帧 `sigma_obs` 超旧门，`sigma_flat` 独立性和物理量纲 | SI 光子积分/解析已知谱/已知色和增益场 |
| [P2 参考 SNR](../absolute-snr/REPORT_paper.md) | `F_ref/sigma_F`、PSF Fisher/GLS、covariance | Newberry/R05 已有理论；贡献是 P1–P5 统一模型与真帧 coverage | 重复星经验散度大于条件预测、稀疏生产者缺失 | 白噪/泊松 Fisher 与自由背景 GLS、MC 覆盖 |
| [P3 HEALPix drizzle](../healpix-polar/REPORT_paper.md) | `w_jp=a_jp/A_drop,j`、局部完备性和球面面积 | Drizzle/HEALPix 均有先例，贡献是组合几何严格差错界 | apex/face-center 构型的疑似 0.32% 误差 | 独立球面曲边积分＋全候选穷举 |
| [P4 稠密 SNR](../dense-snr-reconstruct/REPORT_paper.md) | 固定 `F_ref` 的 `V_ref(x)` 与支持域重建 | 样条/IDW 已有先例，贡献是统计语义+支持域与误差传播 | 自然样条过冲、域外静默、参考/场景 SNR 混淆 | 分析真场、多尺度空洞与场景注入 |
| [P5 加乘性马赛克](../additive-sky-seamless/REPORT_paper.md) | `y_i=g_i S+a_i+noise`、重叠连通分量、接缝保真 | PMM/NSG、ubercal 已有先例，贡献需用可辨识+结构保真证明 | S9b 分量接缝、乘性没有生产接线、实际有效边界不全 | 已知增益+加性世界、分量图、HST 弥散信号真值 |

**上游误差传递原则**：P1 标度的不确定性会影响 P2/P5，但不能将其误写成 P2 的每像素独立白噪；P3 产生相关噪声会改变 P2/P4 的局部方差消费；P4 参考点源的精度场不自动成为弥散源最优像素权重；P5 从重叠估计得到的“零平均差模”不能提供绝对天空背景。

## 二、统一证据层级和实验域

对每一主结论都登记四条相互不同的证据腿：

1. **解析 Oracle**（A）：代数/积分有真值，专用于单位、恒等式、可辨识性、极限反例；可用独立 Python/高精度数值实现，但它不能证明生产代码已使用同一个公式。
2. **物理前向**（B）：HST 图像作为非平凡真实结构模板，在模型里注入已知光谱/PSF/相机噪声、WCS、响应场和 dither；输出完整真值。HST “已经叠加的 FITS”仅是固定形态参考图，**不是 HST 天区对应相机的可追溯未叠加原始噪声真值**，合成独立噪声需另注入。必须保留输入来源、滤镜/WCS、归一化和处理方式。
3. **真实数据**（C）：本地 `testdata/` 中用户提供并允许用于复现的实际校准天文帧；遵守许可与隐私，不上传原始 FITS。按逐帧唯一标识、WCS/CRVAL 约束、滤镜/曝光/相机组、文件校验哈希选帧；不能单靠路径模糊搜索。真帧无天空绝对真值，只能做重复观测/外部星表/已知物理约束的检验。
4. **生产对照**（D）：真实调用 `lib/**` 生产算子、CLI/HiPS、manifest 和合并路径，记录编译/运行环境与 SHA；对照人工注入错误能够判红。使用不链接生产实现的 Python 重演只能列 `SELF_CHECK`，不得列 `PRODUCTION_PASS`。

所有实验带至少一个**零效应阴性**与一个**已知注入阳性**。统计假说应在实验前登记，随机 seed 与参数不可在看到结果后偷偷改变；允许探索试验，但不得用探索结果冒充确认性结果。

## 三、共享实验数据清单（机器可读计划字段）

在 `testdata/` 下按原始数据未上传、仅索引的约定，构建 `实验/shared/data_catalog.json` 或已有机器清单兼容扩展，不覆写已有 schema；每条记录建议含：

```json
{
  "id": "M42-REDFRAME-EXAMPLE",
  "role": ["P1", "P2", "P5"],
  "location": "testdata/<local-relative-path>",
  "file_sha256": "to-be-calculated",
  "bytes": null,
  "detector": "reported-or-unknown",
  "filter": "Red",
  "exposure_s": null,
  "frame_id": "source-derived-stable-key",
  "shape": null,
  "wcs_center_deg": null,
  "pixel_scale_arcsec": null,
  "data_license": "internal-not-redistributable",
  "catalog_status": "INDEX_ONLY"
}
```

以上是**示例字段、不是一条真实资产记录**；没有真正扫描文件不得制造 SHA 或填虚构尺寸。真实相机偏置/暗场/平场/坏点状态也须登记。来源未知的物理参数使用 `null` 而不是零。用相对路径保证服务端可执行，原始 FITS 排除在版本控制之外。

## 四、全局实验矩阵（必须给数据、图、失败例）

| paper / task | 必跑 A | 必跑 B | 必跑 C | 必跑 D |
|---|---|---|---|---|
| P1 | 积分单位、恒星解析真值、空间场 gauge | HST 已知色/响应、PSF 与非线性 | 合规选帧、`sigma_flat` 独立测量、颜色/星等/位置残差 | 当前测光生产入口 vs 参考实现，零值/饱和负例 |
| P2 | 已知/未知背景 GLS、相关与不相关方差 | 扩展源与点源统计被估量 | 117 星等独立配对、经验 coverage | 帧头、稀疏层、生产读写与下游权重 |
| P3 | 面积分割、恒定面亮度、方差/协方差代数 | HST 结构通量/PSF 形态 | 若合规真帧可用：星通量在映射前后比较 | 极区/面中心/叶顶角独立 oracle、候选穷举 |
| P4 | IDW/样条/cell 方差真场、`[0,2,0,0]` 越界 | HST 源项注入与空间控制点疏密 | 实测控制点读回/保留验证 | 支持域外显式状态，生产现场求值 |
| P5 | 纯加性、乘性阴阳世界、断连图 S0/S9/S9b | HST 弥散结构低频功率/积分真值 | M42 真实帧、49 帧边界覆盖审计 | 加性/MA 生产调用链、manifest 的三态增益字段 |

任何一条主张在某层失败，必须在论文的“结果”里保留失败、修正定义或缩小声明适用域，不能仅把失败记录搬到私有 `run/`。不强制每篇的同一个数值误差门；各门必须由物理预算、噪声推导、外部规范或**事先保留数据**标定，禁止事后选一个恰好 PASS 的阈值。

## 五、结果和统计报告的最低格式

每个实验单元新增/更新 `EXPERIMENT_EVIDENCE.md`（或现有 `REPORT_experiment.md` 的独立新章节），逐实验记录：

- `experiment_id`、`paper_id`、H0/H1、数学被估量、单位、输入适用域、主判据及独立负例；
- 实现 SHA、编译器/依赖版本、运行命令、`seed`、输入文件列表和 SHA256、来源许可；
- 样本量及有效覆盖（帧数、星数、控制点数、边界总数/可评估数），训练/验证是否互不重合；
- 主统计量的原始分子分母、均值/中位/MAD/最大值与区间（bootstrap 或理论近似须说明）；
- 负对照是否按先验预期**判红**、零效应是否校准，运行退出码与运行日志摘要；
- `status` 严格取 `PASS`、`FAIL`、`NOT_RUN`、`UNVERIFIED`、`NOT_APPLICABLE`，另分 `selfcheck` 与 `production` 类；`NOT_RUN` 绝不能写成 `PASS`。

机器结果 JSON 建议：

```json
{
  "paper_id": "P3",
  "experiment_id": "P3-B-facecenter-apex",
  "status": "NOT_RUN",
  "evidence_level": "ORACLE",
  "source_sha": null,
  "command": null,
  "inputs_sha256": [],
  "seed": null,
  "metrics": {},
  "negative_control": "NOT_RUN",
  "limitations": ["geometry oracle not executed"]
}
```

这是**目标形态示例**，不是已产生结果。原论文之前的历史数值必须引用生成它的可再现命令与输入，无法再现标记 `HISTORICAL_ONLY` 并从当前发布主结论移出。

## 六、图表与论文最终输出目录

每篇主稿必须最终形成：Table 1 文献-方法要素对照；Table 2 数据/样本与来源；Table 3 主要实验矩阵与假说；Table 4 核心结果含区间和状态；Figure 1 模型/观测流程；Figure 2 基线与改动臂的残差分布；Figure 3 负对照和失效域（P3 改为几何位置热图，P5 另含重叠图示意）；Figure 4 性能/存储/计算精度曲线及其适用域。图只有在运行数据真实存在时才输出，不绘制虚拟“实测曲线”。

附一个跨篇误差传播表：`P1 k_photo uncertainty` → `P2 conditional sigma_F & covariance` → `P3 mapping covariance` → `P4 V_ref reconstruction` → `P5 intensity/weight/seam metric`，每个箭头至少一条既有和一条故障注入样本，记录谁消费了什么对象。

## 七、服务器执行与提交纪律

1. **先读取**五篇修订稿及 [共享文献库](references/CORE_REFERENCES_VERIFIED.md)，禁止将旧 README 的历史 PASS 覆盖成稿中的未验证结论。
2. 优先导入/建立轻量数据索引和独立几何/噪声参考测试，锁定同一原始真实帧清单和 HST 模板哈希。
3. 每个 P1..P5 分别运行 A/B/C/D，必要时子集；代码修复要与本篇 science test 的假说、被估量和红灯反例配套提交。
4. 一篇论文一次只改变一个科学因素；修过生产代码后重跑相应模拟与真帧，输出 baseline/new 的成对表。P5 的增益研究不得借修 P3 或连通 gauge 的同时变化来“证明有效”。
5. 科学论文的 `## 5 结果` 只能用新证据填充，引用全链正常运行结果需附构建 SHA；没有实际数据时只能给方法与 `UNVERIFIED`，不得捏造表格。
6. 一个实验单元的代码变动不能未经另一篇反例测试就回写“全链科学有效”；通过后再审跨篇规范、合同索引与最终产品 `manifest`。

## 八、审稿与合并门

**G-SCI**：每个 P1..P5 至少一个独立 oracle + 一个生产实例 + 一个真实/物理前向实例及负对照；若真实数据暂不可得，实证状态必是限定的 `NOT_RUN/UNVERIFIED`。

**G-CLAIM**：文学新颖性比较按 R01..R11 的“存在先例/方法变化/代码实现/实证贡献”逐条裁定；技术名称不能直接叫原创。

**G-FAIL**：P1 真帧预算、P2 coverage、P3 apex/face-center、P4 域外/过冲、P5 S9b/gain 阴性/未测边界均作为**不可豁免的必看反例**；允许最后科学主张以“指定受限域成立”收敛，不允许通过隐藏失败的方式跨门。

**G-REPRO**：每条数值有原始读数、代码 SHA、命令和输入哈希可复算；只靠报告里的“已验证”不合格。

**G-RELEASE**：在所有 P1–P5 的主张分别达到正式证据等级前不自动合并主干、不得宣布五项已达论文发表水准。

## 九、验收对照表（按 §八 五道门逐篇对照本轮实际情况）

本表只登记**实际发生**的状态；空栏一律写 `NOT_RUN`/`UNVERIFIED`，不得由同篇其它证据推断。证据级别采用
[核验层次](references/CORE_REFERENCES_VERIFIED.md) 的六级；历史证据件中若出现不在六级内的
`PRODUCTION_CHECKED`，按 `IMPLEMENTATION_CHECKED` 读。

| 篇 | G-SCI 独立 oracle | G-SCI 生产实例 | G-SCI 真实/物理前向实例 | G-FAIL 必看反例 | G-REPRO | G-RELEASE |
|---|---|---|---|---|---|---|
| P1 通量积分拟合 | `SELF_CHECK`（Python 判据复演，不链接生产 C++） | `NOT_RUN`（生产 C++ 光路路径未跑）⇒ 不主张 `IMPLEMENTATION_CHECKED` | `EXPERIMENT_CHECKED`（M42 M1 Red 300 s 合规 8 帧） | 5/5 按预声明方向判红（错通带 9.7–18.6×、G 双计 25.4×、色项截断、空间场阶数、错误 BZERO）；其中 **1.5 MAD 截尾即可把判红翻成 PASS** 已登记为禁止路径 | 是（脚本 + seed + 输入哈希） | ✗ 不成立 |
| P2 跨帧绝对信噪比 | `ORACLE_CHECKED`（四臂纯 numpy，不链接生产库；20 000 次 MC + Wilson 95% 区间） | `IMPLEMENTATION_CHECKED`（`k_corr` 生产几何探针）；**`k_corr` 状态 `UNCALIBRATED`**；`sparse_snr_layer` = `NOT_IMPLEMENTED` | `EXPERIMENT_CHECKED`（真实帧 2195 星对） | 覆盖率 nominal `0.6827` → 实测 `0.475`（去趋势 `0.464`）；SNR 分层负例、自由背景欠覆盖 24/24、重采样对角近似判红 33/35 | 是 | ✗ 不成立 |
| P3 守恒映射算子 | `ORACLE_CHECKED`（精确切平面 2D 裁剪 `+1.33e-15`；4001² 半空间网格四叶分数和 `1.000000`） | `IMPLEMENTATION_CHECKED`（生产二进制；300 格 `48/300→0/300`、1500 格 `218/1500→0/1500`；注入开关逐位复现） | `NOT_RUN`（无真实观测；P3-C/D 真帧腿未跑） | 顶点构型 `−3.2094e-03`（超门约 `3209` 倍）已修复；**`60″–290″` 叶弦/分支残差未修（峰值 `3.29e-7`，不破 `1e-6`）** | 是 | ✗ 不成立 |
| P4 重建稠密信噪比 | `ORACLE_CHECKED`（scipy 独立 oracle 与自算差 `9.96e-13`；`E_eff` 闭式对称差 `3.6e-15`） | `IMPLEMENTATION_CHECKED`（生产探针；`idw_power` 为**静态审计**，已注明无真实产品运行日志） | `NOT_RUN`（真实帧端到端未跑；C 臂为 MAD² 代理） | 自然样条越界 `+1.1326e-02`、下冲 `−0.307920`；域外 `55.5556%/43.7500%`；clamp 非无偏 `−0.04115`；NaN 静默填充 `+0.5621`；**`idw_power` 生产有效 `2.0` ≠ 规范 `1.0`（活分裂）** | 是 | ✗ 不成立 |
| P5 加性天光去除 | `ORACLE_CHECKED`（独立几何/权重 Oracle + 逐分量诊断旁路） | `IMPLEMENTATION_CHECKED`（逐分量 gauge **已上线**，`S9b` compB `214×`；连通图 `model_hash` 逐字不变）；乘性增益 = `IDENTITY` | `NOT_RUN`（真实帧乘性腿未跑） | 加性：`S9b` compB `1.2815e-1 → 5.98e-4`；**判据分裂**（全图 `identifiable=1`/`κ=3.89e9` 全绿而 comp1 `rank 118/127` 红）；乘性：**阴性世界假阳性 ⇒ `FAIL`**；边界 196 条中仅 114 条有效 | 是 | ✗ 不成立 |

### 逐篇可对外陈述的边界（不得越界）

- **P1**：`ABOVE_CEILING` 依 `docs/science/photometry/PHOTOMETRY.md` §5 只能读作「超过本帧已登记项合成的**下包络**」，
  **不是**总误差上界，**不构成**「科学不合格」；唯一跨仪器可复现的系统项是参考通带色轴
  （`−0.0534 mag/mag(R−B)`，`−10.05σ`）；线性区/残余平场/绝对透过率一律 `NOT_MEASURED`。
- **P2**：条件 `SNR_ref` 在真实帧上的覆盖率不达名义值，多余散度由**与 SNR 无关的系统项**支配
  ⇒ 须给分层读数 + 经验地板 + 适用域；`k_corr` 在生产几何下未标定（冻结回退不保守），
  **不得**据此宣称绝对权重正确。
- **P3**：修复在受检域内（`0.02″–3600″`、`nside 512…16384`、`pixfrac 0.5…1`、CPU FP64）成立；
  **不得**据未跑域（FP32、更粗像元、全天穷举）外推，**不得**称「全域精确」。
- **P4**：可辩护的贡献是「物理估计量不变 + 稀疏支持域清晰 + 显式重建偏差传播」，
  **不是**「样条或 IDW 作为新算法」；`E_eff`/`var_ratio` 为分布型判据，须成对给水平型判据。
- **P5**：加性规范已按连通分量固定；**乘性增益未经阴性世界验证、禁止默认启用**；
  修后产品**不得**称「已跨分量定标」，跨分量绝对零点仍需外部锚。

### 跨篇误差传播表（§六 要求）的当前状态

`P1 k_photo uncertainty → P2 conditional σ_F & covariance → P3 mapping covariance → P4 V_ref reconstruction
→ P5 intensity/weight/seam metric` 这条链**尚未建立端到端反例**：五篇各自的 §5 跨篇反例节
均如实登记为 `NOT_RUN`，缺口为：① 冻结的同天区/同滤镜/同相机数据链；② 上游缺陷注入后
各下游**预声明**红灯读数；③ P2 条件 SNR 与 P4 真实控制点端到端未闭合。
**禁止**用单篇内部反例冒充跨篇已闭合。

 
## 十、B09-D1：五篇论文的 Claim—Estimand—Implementation—Evidence 全局对应（2026-10-17）

本节直接作为科研正文的共享实验设计和结果溯源，不新建门禁文件。起始主线参考 `6f1410e1084c87265e59d2d9a7bc109405c9d714`；本轮已经读取五篇原始 REPORT_paper.md（176/173/188/163/189 行）、各自 REPORT_experiment.md（319/280/499/335/697 行）、源脚本与存在的固化证据、原 S1/S2 数学研究及 #8/#9/#13/#14/#80。每篇完整主稿与本节共同构成可用的论文研究材料；**本节本身不是当前 C++ 的实验证明**。

### 10.1 读数身份、独立性与估计量定义

每一条主张分别记录以下相互独立的地位：`MATH`（有明确前提的代数/反例）、`TOY_ORACLE_HIST`（旧版给定真值或独立 Python 的解析数值）、`PRODUCTION_PROBE_HIST`（按当时二进制和模型哈希构建的生产符号探针）、`REAL_HIST`（旧报告从真实FITS得出但本轮未重读）、`FORWARD_HST_HIST`（HST真实结构模板加模拟物理噪声，不是原始HST无噪真值）、`PROD_E2E_CURRENT`、`SCIENCE_FIDELITY_CURRENT`、`BIB_ONLY`、`USER_VISUAL`。最后三类**本次全部 NOT_RUN**。每次真实重验还应锁定源输入sha256、build/dll sha256、加载的真实C++函数、模型/响应/参考SED版本、配置、seed、实际有效mask、源/帧/像元各级分母、CI方法。不能把原报告的PASS填入最新main的运行状态。

P1 `F_ref(u)`是PSF模板的参考振幅，不等于像素应用位置标度后的积分总flux。P2 点源条件 Fisher 信息 `I_F=1/V_F` 的单位是 flux^-2；球面面亮度权重是 `1/V_B`，单位 `sr²/L²`，需在相同场景条件下的 transfer `T=V_B/V_F`。P3输入积分量L、面亮度L/sr、交叠面积sr和累计覆盖D不能混用；P4重建reference information field不是凭插值就自动有scientific `V_B`；P5控制patch median方差不是pixel variance，消除背景均值不消除sky shot noise，混通带不等于同一天空场。正确科学信号、精度缺失和资料损坏必须不同状态。


**Git中证据的完整性**：五个原单元的 `results/` 和 `run/` 均未被本次远端Git树追踪；P1的 S04逐星表/原始FITS/Gaia锥在仓外，P2–P5若干 `docs/S0*_evidence.json`（和脚本、图hash）在仓内。S05证据文件中还含有非标准JSON的 `Infinity` 常量，必须先单独辨识坏数值/状态，不能把它无损当标准JSON可靠精度。`evidence.json`历史记录、SHA、图像hash和生成脚本各自是不同证据层；本次没运行任何C++、数据、Monte Carlo、HST/M42或外部包。

### 10.2 全篇核心主张的证据映射表

下表每一行的“生产”指当前静态可核对的函数，历史数字始终来自对应旧REPORT或已提交小型evidence，不默许历史构建等于当前main。

| Claim及假设、创新性边界 | Estimand、单位、数学与CURRENT实现 | 旧实验、Oracle、样本量/CI、真实数据与mask | 失败、文献先例及论文可发表的限定表述 |
|---|---|---|---|
| **P1-C01** GaiaXP合成参考、逐帧相对零点；XP合成已有先例 | `q=∫Fλ TQ λ/(hc)dλ` photons m^-2 s^-1，旧`F_syn=∫FλTQλdλ`单位W m^-2 nm只是相对proxy；生产 `module_adapters.cpp::p1_op_photometry`→Photometer/`spectrum_integrator.cpp` | `code/step1_analytic.py`、`code/s04/s04_common.py`，真实XP sampled343点336–1020nm，历史827个Gaia锥源（原缓存仓外），analytic oracle和GaiaXPy本轮未跑 | 历史误将Baader R选成Antlia B已更正，响应限XP支持域，不能称新发明或绝对物理标定；一手Gaia Collaboration/Montegriffo 2023 A33/A3、Xiao2025须B10核具体式 |
| **P1-C02** 空间增益、PSF/孔径及颜色系统的非独立残差 | `d=-2.5log10(F_instr/F_syn)-µ` mag，σ_obs=2.5MAD(log残差inliers)/0.67448975，finite PSF窗η、亚像元相位/WCS影响F_instr；CURRENT p1_sources与photoapplied链 | S04真实M42 M1 Red300s，T2×2 T3×6，旧版σ_obs0.0265198375mag（n157、151内点）vs历史方差修订后0.0268730673mag（n178、171内点）；主帧171独立星色斜率-0.02467±0.00245 mag/标准轴，按星bootstrap 800–1200次seed20261014；全八帧734行只有177独立星；`code/s04/{s04_measure,s04_strata}.py`和图hash | 434候选星在各帧有效比例0.106–0.410，跨版本不同星不能写成同样本性能提升；色项存在只适用受检SED/PSF/仪器；Padmanabhan ubercal已有相对校准工作，真实现版C++E2未跑 |
| **P1-C03** 参考振幅、应用图与球面BUNIT一致性；贡献候选是防双标度和不确定度完整建模 | `F_ref=alpha_*F_native`≠`F_applied,total=F_nativeΣalpha_jP_j`，S1数学例200 vs250；`C'=Dalpha C Dalphaᵀ`仅条件协方差，边际应另含`Jθ Cθ Jθᵀ`与x/θ交叉。生产`p1_op_drizzle`真读photoapplied并缩放噪声，`p1_stack.bunit=ADU/sr`仍固定 | S04 `s04_sigmaflat.py` flat去二阶0.001766/0.002311mag、未去0.007579/0.009526mag；母版施加8帧3改善5变差，sigma_obs中位+0.00122mag，pair 1802行仅145独立星；gain=1.333e-/ADU和RN8.59e-硬编码UNVERIFIED | #9真实数值/声明错位、#51 sigma_psfsys与fit残差或双计；sigma_flat=0.0007mag未独立验证，Hα300/600斜率−3.6%/dex不能说明探测器非线性拐点。真实新E2及单位Oracle未跑 |
| **P1-C04** 负对照检出污染，不能用筛选制造成功 | 旧`ABOVE_CEILING`仅超出**已登记误差下包络**，不是实际测量错误率上界；`s04_negatives.py`并未调用新生产C++ | 历史错通带9.68/16.15/18.57×、重复Gaia G25.37×；把同一历史残差作1.5MAD再次截尾可令红翻绿；HST模板独立forward在本次NOT_RUN | 只能表述旧脚本有部分判别力且存在选择效应漏洞；不得称完整instrument noise已通过或P1→P5真实故障传播PASS |
| **P2-C01** 参考通量条件Fisher及GLS，前例已有Horne、Newberry等 | 固定PSF与噪声时`SNR_ref=F_ref/√V_F`；已知背景 `I_F=PᵀC⁻¹P`，未知背景投影后`I_F=Pᵀ(C⁻¹−C⁻¹Z(ZᵀC⁻¹Z)⁻¹ZᵀC⁻¹)P`；`noise_snr/cpp/src/{sparse_reference,information_weight}.cpp` | S05 `code/s05/s05_arms_oracle.py`、`docs/S05_P2_evidence.json`，历史白噪MC sd/理论σ 0.9929–1.0095、135个Wilson覆盖比较5不合目标、历史解析oracle；真实星不同质量需另分层 | 源Poisson、有限PSF、相关master/sky使假设失效；#8要求面亮度variance transfer，不能简单`SNR²/F_ref²`当像素权重。Horne为光谱抽取不是成像PSF逐项最优证明 |
| **P2-C02** Drizzle后真正协方差与k_corr经验定标 | `C_sphere=R C_source Rᵀ`；`k_corr`需标定源PSF/像元尺度/pixfrac和patch median统计量；生产 `sampler.cpp`、`astro_sphere_sink.cpp` | 旧S05 `s05_kcorr_units.py`,`s05_real117.py`与真实M42：MC kcorr2.33、旧探针2.054–2.058、默认回退1.4；2195最终Red配对（1108训练/1087留出；两夜Green颜色有值是样本资格），旧`sd/σ=3.3701`、留出旧条件68.27%覆盖0.4471、训练地板后0.8611过覆盖。**原脚本先逐像元clip(sub−sky,0)再孔径求和，却按线性孔径推Poisson/read σ；行号hash而非稳定Gaia ID**；本轮仅静态复核，未重读FITS/npz | 真实约0.97″/px超k_corr表300/600″域；#17/#64/#71及旧截断估计器不匹配为可证伪负例，无法把旧欠覆盖直接归因CCD随机噪声或P2 Fisher公式；S05证据含Infinity异常，不应称生产真值精度已校准；Fruchter–Hook相关噪声先例 |
| **P2-C03** Canonical precision真正供Mosaic权重 | `I_F=1/V_F=SNR_ref²/F_ref²` flux^-2；`T=V_B/V_F`，若同条件`w_B=I_F/T`单位sr²/L²；旧点源SNR不是sky patch median `control_ivar` | P1目标`frame_science/v2`尚是TARGET；Stage2 `p2_sparse_layer_probe`仍读`ACSD_SPARSE_SNR_LAYER`，S1数学Toy oracle与当前消费者分离；真实C++S1→S2未跑 | #8与#13是科学实现阻断，不能由旧sparse层的节点拟合声称SB科学权重已闭合 |
| **P3-C01** HEALPix曲面Drizzle几何守恒：F&H和HEALPix都非新算法 | `w_jp=a_jp/A_drop,j`、`epsilon_j=Σp a_jp/A_drop,j−1`、`B_p=Σwx/ΣwΩ` L/sr，`C_B=R C_in Rᵀ`。源`spherical_overlap.cpp`,`drizzle_engine.cpp`,`astro_sphere_sink.cpp` | S02历史探针`docs/S02_P3_acceptance_matrix.json`在旧二进制：1500格218/1500超1e-6→修后0/1500，修后最大1.3755e-8；故障注入旧偏差恢复。HST M16 f502n drizzled模板0.04″/px，不是独立未叠加真噪声 | #15真HEALPix曲边并非连四顶点的geodesic直线，局部−9.968%属于边形面积而非已测flux损失；原报告的自闭合PASS不能证明真实曲边，FP32和真实E2尚未跑。Górski、HiPS、Fruchter&Hook为先例 |
| **P3-C02** 同天空位置跨阶重采样而非只改tile order | NESTED真实叶K=log2(nside)，512tile目录K−9；不同输入阶必须在共同sphere grid重投或拒绝。生产`module_adapters.cpp::p2_hips_order_uniform`已fail-closed | #19历史静默跨阶问题已被同阶检查挡住；旧S02跨face/极区toy不能替代真实跨阶精度；face和apex曲边oracle需独立数值积分 | 当前是**同阶可用、异阶明确拒绝，真正跨阶重映射未实现**；也不能把P3创新点（Stage1 Drizzle）和Stage3 Export局部bilinear混写 |
| **P4-C01** 固定reference precision连续场有统计前提，非“插值越密信息越多” | `u=log V_F`或logSNR_ref；控制节点有实际σ估计误差/支持，不能在无T时叫`log V_B`；生产`weight_chain.cpp::SparseSnrReconstructor::prepare/eval`与`nearest_valid_fill` | S06 `docs/S06_P4_evidence.json`、`code/s06`：历史2×2/3×3域外占55.56%/43.75%，natural spline过冲+0.011326、下冲−0.307920；HST真实结构模拟和真实M42的MAD²代理没有真方差 | #68/69近期静态修改不能由旧S06探针盖当下PASS；#70最近邻填充可把未知精度当已知；#8/#13阻断实际V_B传播。经典splines/IDW不可当新算法 |
| **P4-C02** 三种重建表示并非一个普遍最优 | `dex-RMSE(log V)`、方差比例/efficiency必须标明真值是cell/reference/scene，实际节点有误差时和理想节点不同 | 历史解析结构场cell0.00903<dense0.577<frame1.159；HST前向cell0.206<frame0.416<<dense2.57e5；真实帧MAD²proxy cell0.171、frame11.8、dense6.46e4，旧S06 CV和6种场型；bootstrap/跨场假设需分别记录 | 0.000743理想控制点“表示上限”不是有噪end-to-end；当前idw_power2.0与旧规范1.0分裂需当代真实C++对拍 |
| **P5-C01** 公共信号/相对加性天空差、连通分量各自gauge | `y_k=g_k H_kS+a_k+epsilon`，目前生产固定g=1；每个连通分量只可辨相对additive，绝对sky需外锚；源 `sky_plane.cpp::p2_sky_plane_build_joint`、`p2_sky_plane_eval_delta_block` | S03 `docs/S03_P5_COMPONENT_evidence.json`：历史S9b规范症状0.1281497→0.00059817（214×），不是实际星云接缝改善值；低频暗云/外域未独立保真 | #82 own无支持外延与#83 floor-bin训练/bilinear应用基函数不一致，不能写每步严格LS保证收敛；P5数值来源与绝对天光本身不同 |
| **P5-C02** PMM式剩余乘性是研究设想，阴性特异性必须先成立 | 重叠matched stars估计g须分开PSF/颜色/sky/随机参数；现行生产g=1，S07 `code/s07_gain_experiment.py`并未生产接线 | 入库`docs/S07_P5_GAIN_evidence.json`：历史g=1阴性世界10seed，A+G伪改善heldout RMS中位约+0.00968，95%CI约[+0.00681,+0.01740]排除0，**FAIL**；阳性不抵消阴性 | #76 A+G+S共同解尚未生产，不能发布“自动增益科学有效”。PMM、NSG及ubercal公开先例待B10实质对拍 |
| **P5-C03** 接缝、通带可比性与低频/SNR都要同时证明 | sky subtraction不移除shot noise；不同`R_k(λ)`给不同`B_k`且混合权重决定空间变化的`R_eff`，Schur精确PSD只在有效权和模型同被估量/正规矩阵假设下成立 | 旧C1合成6条人工缝median4.80→0.383e⁻，C7真实4帧512²裁剪的6条x边是合成cell界而非真实footprint；旧49帧成品FITS已回收；真实#80 E0 2Hα+1Red观测17237/overlap5807、旧exit2、权尺度差1.4e32、Schurλmin/λmax≈−1.727e−16 | #14跨响应不能强加单公共天光，#80数值消减和单位不等于物理自洽，#83基函数仍未闭合；#76真实E2和用户本人目检为必需，不能把toy/seam RMS评为科学保真 |
| **P5-C04** 条件数值稳定性、真实共同天空和PMM可辨识性 | whitened QR/SVD /列规范的rank和相同物理预测比法向方程Schur减差更稳；有明显负特征不能仅随意ridge | S2 `innovation-closure/code/s2_oracle.py` 21×11 toy，同响应/列缩放/失秩Oracle；旧#80需回放完整真实矩阵及有量纲R_k/同响应H，而不是摘要 | 尚无HST已知谱星云真值（Hα/[N II]/连续谱分开）、同band与mixed的真实E2对照、跨帧完整联合协方差，故只`TOY_MATH + REAL_FAILURE_REVIEWED` |

### 10.3 五套报告版本身份与可用原证据

| Paper | 此次原paper blob（行） | 原实验报告blob（行） | 真正可回读资产 / 尚缺 |
|---|---|---|---|
| P1 | `fe7f9f9c5e7888e42a3b0f8733f46cd51f6b832a` (176) | `bc981668ca625a65d183bd8532beea380bfeb47c` (319) | code/s04 + Figures manifest、历史补丁/refs；S04运行树与原FITS此批未回读。**本批主稿作逐段科学修订** |
| P2 | `0f9a31f8ca97240f9199a158ae2b71ee105916d1` (173) | `5de5ec579e2ee7d75e9bde5b834ae0f6d995bbbf` (280) | docs/S05_P2_evidence.json、S05_REPORT.md、s05脚本；运行results缺失 |
| P3 | `bda5e7858a7e909784b39393d445e2dd36a1b99f` (188) | `c33b1b5b224dbcdc8c6818e02ed8251c1b99ad21` (499) | docs/S02_P3_acceptance_matrix.json、S02_P3_NORM_evidence.json、s02脚本；真实几何E2缺 |
| P4 | `312e2027439fa8ee68e93f2d5eea4bc24a2acb4b` (163) | `20a8981eb5751b29c4a56696a1c129e30869106b` (335) | docs/S06_P4_evidence.json、s06脚本；真实方差与运行树缺 |
| P5 | `37cd36497f8c456d58f4178e10e4792aedf9ad4e` (189) | `6f733a43e4ba61c05febef02b8658e941c5abcfe` (697) | docs/S03/S07 evidence、旧探针；真实49帧产品FITS已回收 |

### 10.4 下一轮P2–P5具体工作包，不冒充五篇全部完成

**P2**：以固定F_ref/PSF/sky nuisance建立白化设计矩阵及有/无已知背景GLS，对实际生产的sphere R/C/h/Z取独立SVD/QR和可证明conditional confidence；将Poisson参考源与实际source分开；回放S05各个seed、旧117星/最终2195 Red配对与当前ABI建立成对版本、独立定位clip负像元导致的通量正偏及漏计天空环带误差（原`s05_real117.py`），固定原选星和物理Gaia ID稳定holdout，报告Red-only vs双Green颜色资格及每种光度统计量配对方差，再评价effective PSF/flat covariance，重新按同估计器的pixfrac、局部patch大小、实际0.96″/px、nside多档标定k_corr。把#8 T转换和#13真实v2消费者、#64缺variance源的UNKNOWN状态纳入同一次FITS E2；B10精读Newberry、Horne、Zackay&Ofek和相关Drizzle，补原文真正求解条件。

**P3**：保留drop完备性/F/N/SNR关联方程，真正以独立HEALPix曲边数值Oracle对全部12face、极区、跨face、顶点、0.04″–更大角尺度、nside与pixfrac扫真area fraction，而不是仅查Σa/A=1；明确对比四角弦、真实曲边、源constant亮度/星点flux、PSF/MTF、源/叶cov、S02数值版本。#15局部−9.968%曲边面积构型必须单独观测，FP32和真实M42/HST有效掩膜下E2未通过不能夸大；#19当前拒绝异阶仅是安全措施，需设计实跨阶重映射及保守误差核。B10逐式核Górski、Fruchter&Hook和IVOA HiPS。

**P4**：先在#8同estimand条件下把reference_IF转实际V_B或仅保留reference诊断，再以有M（曲率）及节点观测不确定δ_i的已知logV场做解析上/下界与真实coverage，加入2×2/3×3、洞、孤岛、亮源星云、不同PSF/seeing/源泊松；比较natural spline、IDW/cell/kriging和旧dense/frame，控制点来源和运行CPU/RSS也记录。按新main重跑#68/#69修后、#70缺点合理不确定、#13正式Canonical读入；旧HST MAD²代理不能叫真方差，B10方法先例分辨Wahba/Fritsch–Carlson/Hyman。

**P5**：先以#80真实2Hα+1Red完整`D,F,G,Σ`矩阵/输入hash/源实际BUNIT/响应复核物理与数值各自原因，考虑#83 own floor-bin与bilinear eval实际基函数，检验gauge/秩与白化QR/SVD/Schur预条件；加入#82支持域，正确分同band与mixed共同光谱子空间。S07 g=1阴性十seed存在显著伪gain，必须先修阴性特异性，再做g≠1阳性、颜色/PSF/孔径/holdout matched stars、共模低频保真、sky shot噪声/参数协方差与PMM公开代码参考比较。使用HST已知SED模拟及M42真实E2单signal HiPS→FITS，独立计算point/extended flux、Hα线比、真实footprint seam、conditional/bias/CI及噪声功率谱，#76完成前必须有负责人目检E2产品。已有加性生产≠A+G联合完成。

### 10.5 B10一手 prior art 与本批证据上限

P1 Gaia Collaboration/Montegriffo A&A674 A33/A3、De Angeli A2、Xiao2025、Padmanabhan2008、Akima1970/Huber；P2 Newberry/Horne/Zackay&Ofek；P3 Górski2005/Fruchter&Hook2002/IVOA HiPS；P4 Wahba、Fritsch–Carlson、Hyman；P5 Fixsen联合标定、SDSS ubercal、PhotometricMosaic/NSG/Montage。现存 `references/CORE_REFERENCES_VERIFIED.md` 与 `references/PRIOR_ART_CLAIM_MATRIX.md` 提供书目/部分公开方法信息，**B09-D1没有逐篇下载原全文、核对原式号页码、参数/公开代码许可**。B10需把每个已声明贡献对照到一手实际estimand、假设、实现与实验，不能从DOI有效直接推出原创性。

D1审查性质：上述数学/源函数和已存历史证据映射；**NOT_RUN**现行生产C++编译、三CLI、真实/模拟FITS E2E、M42/HST新真值、外部GaiaXPy/HEALPix/WCSLIB、MC、资源与Windows、用户目检。只有P1已进入逐段正式修稿；P2–P5留下具体算法/实验包而不是假称全篇完成。等待独立复审后才开始B09-D2与B10。

 
### B09-D1并行S3研究更新（源文件在本批准备提交时入main；非生产复跑）

`实验/innovation-closure/S3_SCIENCE_REPORT_2026-10-17.md`在独立复审中给出了两条更严格的数学反例：#83 floor-bin更新与最终bilinear求值不同目标，在冻结两帧解析样本上旧拟合的平方误差1，按真实双线性基的最小二乘误差0；这只证假“严格LS”断言，不是M42真实场误差。#80针对二阶差分D的临时锚Q-only修正，二维完整矩形网格核空间含[1,x,y,xy]四维而不是假定三维。在单独构造42维SPD H、λ=0.5、α=3的受控toy上，旧Q-only修正相对原惩罚线性方程残差0.123484、精确Woodbury修正5.19e−16、增广QR约2.00e−15；`λDᵀD`与无惩罚解相距4.12577，**正则化会改变物理估计量**。研究还构造3帧×23位置和36独立匹配星、500组独立零gain激活MC；结果局限于同响应、identity PSF、已知Gaussian噪声、无bad-pixel/rejection的toy。该S3研究明确`PRODUCTION_NOT_RUN/REAL_E2_NOT_RUN/USER_VISUAL_PENDING`，不能把toy公式故障的0.123484冒充当前真实#80发生过同等偏差；旧S07 g=1阴性FAIL仍有效，S3的新star-only激活策略未经现行生产接线，不能覆盖旧失败记录。P5-D2必须新增对生产实际 fallback矩阵残差、完整四维零空间、可信penalty身份及未惩罚目标的对照。

