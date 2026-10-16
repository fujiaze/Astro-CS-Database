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
