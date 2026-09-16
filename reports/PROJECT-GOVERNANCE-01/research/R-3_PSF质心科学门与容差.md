# R-3 研究报告 —— PSF / 质心 / 科学门与容差

> 研究线：`工程控制/PROJECT-GOVERNANCE-01/RESEARCH_TASKS.md` 研究线 R-3
> 覆盖议题：**D-16 + M1a-A-002（0.5px 质心门 / F1 精度门）**、**M3b-A-01（高斯 vs Moffat4）**、**M3b-F-02（curve_fit Oracle 缺失）**、**M1a-A-001（SCI 公式量纲）**、**M1a-C-001（NB_GRID 7 vs 41/81）**、**F-2（科学门/容差缺依据）**
> 纪律：零 git 写、零仓库文件改动（全部交付物落 `reports/` 与 `run/`）；不采信 `问题扫描` 的 `fix_state/verified_state`；结论均为唯一推荐方案，不以「需负责人裁决」收尾。
> 实验环境：Linux x86_64，`g++` 编译本树源码静态库，Python 3.13.5 + numpy 2.2.4 + scipy 1.15.3（仓内已装且已在用）。

---

## 0. 结论摘要（先看这一张表）

| 议题 | 门/判据本身是否正当 | 唯一推荐结论 | 置信度 |
|---|---|---|---|
| **D-16 / M3b-G-01**（0.5px 质心门 vs 实测 0.897px） | **前提不成立**：把三条互不相干的门混为一条 | 「0.5px」是 ALG-STARDET-001 §11.4 **F4 FP32/u16 量化通道**容差，实测余量 ~90x，**可达且未超标**；0.897px 是 plate solve **外部闭环全帧中位残差**（legacy 工具），不属 F4 也不属 F1 的量测域。本议题真正的缺陷不是阈值而是**端到端坐标契约**：实测 sdet 与 dpsf 对同一颗星相差**恰 0.5 px**，编排层对两条支路同施 -0.5 ⇒ PSF 支路系统性偏低 0.5px、与 fallback 支路相差 0.5000px（去重阈值恰为严格 <0.5）。⇒ **改桥，不改门值** | 高（0.5px 事实/门可达性）；中高（应改哪一支） |
| **M1a-A-002 / F1 精度门** | 判据正当、门值正当、**量测域未定义**（真缺陷） | 保留 0.5" 门值，**只补量测域**：F1 = 「合成/真场、`trans` 内点域、`rms_arcsec`、n_pairs>=12」；另立**外部闭环门**（全帧头域、与内点域正交的证据面），不作为 F1 的替代判据 | 高 |
| **M3b-A-01**（生产拟合用高斯而非 Moffat4） | **门本身错**：SCI-PSF-001 §10 的禁则适用范围被越界扩张到检测域 | **不以 SCI 为准改实现**。改为冻结「检测侧=高斯 / PSF 侧=Moffat4」**双模型**，写死不可互换的换算声明，函数与 README 改名，登记 DISP | 高 |
| **M3b-F-02**（curve_fit Oracle 缺失） | 门正当；**「需新依赖」的前提不成立** | scipy 已在仓内 5 处使用；本报告 9.5 ms 跑通 PSF.md:99 承诺的复算（位置 0.0px、FWHM 1.5e-16）。⇒ 落一个 scipy 第三方 Oracle + 证据 ID，并把矩阵 `test_path` 从文档锚改为可执行目标 | 高 |
| **M1a-A-001**（SCI 公式量纲/自变量互斥） | 门正当、**前提成立** | 以实现为准订正 SCI §5（及 §5a）为像素域式 `xi,eta = CD·[dPix + A(dPix)]`；:66 一致性声明改符号锚。**实现零改动** | 高 |
| **M1a-C-001**（NB_GRID 7 vs 41/81） | **判据本身错**：7x7 既是拟合域又是检验域 ⇒ 自证门 | 删除「NB_GRID=7」的冻结地位，SCI 改为「网格 >=7x7（实现 41x41 / APx 81x81）+ 逆映射以迭代反演为准 + 不变量在独立密集域上测」；登记 DISP-WCS-008 并把 owner 裁决 WCS-003 提回活动登记面。**「1e-4px 数学不可达」对当前实现不成立**（实测 1e-9 级） | 高 |
| **F-2**（科学门/容差缺依据） | 共性缺陷成立：**0 条门有标定依据，2/5 条连量测域都没写** | 在 SCI 侧补一张**冻结门表**（门 ID/判据/量测域/统计量/SNR 定义/依据/证据），并规定「门不得引用表外阈值」；本域必须补的量测域 = F1 精度门的域与统计量、star-det 门的 SNR 定义 | 高 |

**需负责人确认的那一句话**（详见 §5.7）：
> 「同意把『0.5px 质心门』定性为 **F4 FP32 量化通道容差（可达、未超标）**，把 D-16 的整改对象改为 **端到端坐标契约**（PSF 支路写端不再 -0.5，并新增链接 sdet+dpsf 的绝对位置门）；F1 只补量测域、不改门值。」

---

## 1. 权威原文（逐条 文件:行 + 逐字 + 在权威链中的位置）

### 1.0 权威链（ASTROCS_DESIGN.md:7-31）

```text
## 0. 权威声明
    D["(1) 本文 最高设计<br/>是什么 · 做到什么 · 顶层架构 · CLI · 发行 · 验收"]
    S["docs/science 科学公式（权威）"]
    AL["docs/algorithms 算法推导（权威）"]
- 本文与其他任何文档冲突时，**以本文为准**。
- Agent 无权放宽、重新解释或"为通过检查而改写"本文。
```

→ `docs/science/**`（SCI）与 `docs/algorithms/**`（ALG）**都是权威层**（ASTROCS_DESIGN.md:15-16 标注「权威」），二者内部冲突即为「权威打架」，必须由文档集变更流程解决；ENGINEERING_SPEC.md:25 冻结「科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、**精度与默认容差不可修改**」；ENGINEERING_SPEC.md:117-120 规定「`ci/checks.json` 是唯一检查注册表…每项检查有正例与负例（能红能绿）…核心合同有独立测试」。

### 1.1 D-16 / M1a-A-002 相关原文

| # | 位置 | 逐字原文 | 在权威链的位置 |
|---|---|---|---|
| A1 | `docs/science/STAR_DETECTION.md:12-14` | 「- subpixel centroid: 亚像素质心为连续估计（一阶导零交叉 / 二阶导零交叉 / Moffat4 GSL-LM 中心），不引入 0.5px 网格量化损失；合成场验收容差 \|Δc\|<=0.3 px（SNR>=20）见 ALG-STARDET-001 §11.4 F1。」 | SCI-P1-STAR-001（FROZEN 2026-09-07）——**权威** |
| A2 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:256-258` | 「- F1 合成高斯星场（已知中心/流量/FWHM/SNR）: 亚像素质心 \|Δc\|<=0.3 px（SNR>=20）；FWHM 相对误差 <=10%；完整性: SNR>=10 星召回 >=99%；纯噪声场虚警 <=0.1/千像素（专项=completeness/false positive synthetic fields）。」 | ALG-STARDET-001 §11.4 TEST-STAR-DESIGN-001——**权威（冻结测试设计）** |
| A3 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:263-265` | 「- F4 FP64 通道 oracle: 独立 Moffat4/Gaussian 复算（B,A,x0,y0,sx,sy,theta）\|Δ中心\|<=0.05 px、A/B 相对误差 <=1e-3（GSL 对独立实现，双精度）；FP32 通道经 uint16 量化容差独立冻结 \|Δc\|<=0.5 px（DISP-STAR-001）。」 | 同上——**「0.5 px 质心门」的唯一文本出处** |
| A4 | `docs/algorithms/PLATESOLVE.md:185-187` | 「- F1 合成线性场（order=1，已知 CD/CRVAL/CRPIX 合成星表）：求解成功且 n_pairs>=12；rms_arcsec <=0.5"（实测锚 Galaxy_Center=0.1431"，memory.md 2026-07-12）；CD 元素相对误差 <=2%（§9 尺度容差 0.002 同源）；\|ΔCRVAL\|<=1"。」 | ALG-WCS-001 §11.4 TEST-WCS-DESIGN-001——**权威；「F1 精度门」的文本出处（注意与 A2 的 F1 同名不同门）** |
| A5 | `docs/algorithms/PLATESOLVE.md:125` | 「\| RMS 统计 \| ipv_wcs.cpp:483-517 \| rms_arcsec=sqrt(Σr²/n)；rms_px=rms_arcsec/s0 \|」 | ALG-WCS-001 §11.1 逐符号锚——**只给公式，不给量测域** |
| A6 | `lib/algorithms/platesolve/memory.md:55-56` | 「  - T3: PlateSolve RMS=0.151px(31pairs) vs 独立 median=0.897px(702 matched) — 5.9x 差距 /   - T2: PlateSolve RMS=0.108px(33pairs) vs 独立 median=0.772px(1237 matched) — 7.2x 差距」 | **模块 memory（非规范层）**——0.897px 的唯一出处 |
| A7 | `run/PROJECT-GOVERNANCE-01/DOC-001/逐条处置表.md:66` | 「\| 58 \| M3b-G-01 \| P0 \| 1 \| OPEN \| 留待后续：0.5px/质心门与实测 0.897px 的差值属科学裁决面…本轮不得代裁…」 | **DOC-001 处置表**（本轮 D-16 的成形处）——把 A3 的 0.5px 与 A6 的 0.897px 直接并列 |
| A8 | `lib/infrastructure/pipeline/orchestrator/cpp/tests/test_p1_batchH_star_coord.cpp:5-9` | 「//   - sdet star_det 坐标 = "像素中心=索引+0.5" 连续系 (§17.2 权威); / //   - DPSF 拟合中心与输入 det_x 同系 (dpsf_psf.cpp 样本 dx=索引-cx、回移 cx+x0, 无 0.5 注入) -> 亦为 "像素中心=索引+0.5" 系; / //   - star_measurements 权威块 = 统一契约 index-is-center (值=连续系-0.5); / //   - ipv detections 输入 = IPV 接口契约 (像素中心=索引+0.5) (§18.1)。」 | 共址测试头注（**测试面，非规范层**）——本议题的判据前提 |
| A9 | `docs/contracts/DATA_SEMANTICS.md:760` | 「\| detections \| double [n,6] 行主序 \| 列 [0..5]=det_x,det_y,flux,mag,sat,has_sat；det_x/det_y 为 pixel（**IPV 接口契约：像素中心=索引+0.5**） \| orchestrator 由 star_measurements 统一契约（index-is-center）**+0.5 显式转换**构造（:1867）；…」 | DATA-P1-WCS——**权威合同** |
| A10 | `docs/algorithms/PLATESOLVE.md:127` | 「\| orchestrator 过滤+坐标契约 \| orchestrator.cpp:1855-1876 \| star_measurements [N,>=15] FLOAT64；status∈{0,3}、sat r[13]、fwhm r[7]∈[0.5,20]、边缘 5px；**+0.5 转换 :1867**（统一契约 index-is-center -> IPV 接口契约 center=index+0.5）；sdet fallback 坐标已是 +0.5 契约（:1878） \|」 | ALG-WCS-001 §11.1——**权威；规定 fallback 直送** |
| A11 | `docs/contracts/DATA_SEMANTICS.md:714` | 「PLATESOLVE fallback 读 star_det 块显式 DETECTOR_FALLBACK 并按「像素中心=索引+0.5」-0.5 转统一契约（orchestrator.cpp:1826-1829）」 | DATA-P1-STAR §17.2——**与 A10 及实码互斥（见 §3.4）** |

### 1.2 M3b-A-01（高斯 vs Moffat4）相关原文

| # | 位置 | 逐字原文 | 位置 |
|---|---|---|---|
| B1 | `docs/science/STAR_DETECTION.md:41-42` | 「- 基线算法=peaker 七步候选 + Moffat4 GSL trust-region LM 拟合（sdet_detect_impl 生产路径，sdet_api.cpp:1599-2353）；…」 | SCI-P1-STAR-001 §3——**权威** |
| B2 | `docs/science/STAR_DETECTION.md:29-31` | 「- SCI-PSF-001（PSF）：检测输出（cx/cy/FWHM/振幅/背景）为 PSF 拟合与 plate solve 的输入；Moffat4/Gaussian 拟合模型语义归 SCI-PSF-001 域，本模块只登记检测侧调用事实（ALG-STARDET-001 §2）。」 | 同上 §2——**范围让渡句** |
| B3 | `docs/science/PSF.md:43,50-52` | 「I(r) = B + A / (1 + Q)^4」/「各向同性 sx=sy=sigma ⇒ Q=0.5·r²/sigma² / alpha=sqrt2·sigma, FWHM=2alpha·sqrt(2^{1/4}-1)=2sqrt2·sigma·sqrt(2^{1/4}-1)≈1.230310·sigma / flux = 2piA·sxsy/3   (整平面延伸假设)」 | SCI-PSF-001 §5——**权威** |
| B4 | `docs/science/PSF.md:94` | 「- 引入未文档的高斯备选拟合路径作为主路径。」（列于 §10「不可接受变化」） | SCI-PSF-001 §10——**权威；本条禁则的范围是本议题争议焦点** |
| B5 | `docs/science/PSF.md:111` | 「- 实现: lib/algorithms/psf/src/dpsf_psf.cpp (…)」 | SCI-PSF-001 §13——SCI-PSF-001 **自我绑定 dpsf 为唯一实现面** |
| B6 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:60-64` | 「- Moffat4/Gaussian 拟合模型（sdet_lm_fit :262-437，GSL trust-region LM，7 参数）: f(x,y)=B+A·exp(-(x'²/SX+(y'/r)²/SX)/1)，参数 {B,A,x0,y0,SX=2sigma²,fr,alpha}，… fwhm=2.3548·sigma（TWO_SQRT_2_LOG2），…」 | ALG-STARDET-001 §2——**权威；此处写的已是高斯式** |
| B7 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:169-171` | 「- 亚像素质心: 一阶导/零交叉为连续估计（无 0.5px 网格量化损失），Moffat4 中心由 GSL LM（XTOL/GTOL/FTOL 编译期常量）收敛；合成高斯场 oracle 容差于 §11.4 冻结，禁止放宽。」 | 同上 §9——**同段内「Moffat4 中心」与「合成高斯场」并置** |
| B8 | `docs/algorithms/STAR_PSF_ALGORITHMS.md:52-58` | 「F1: I(r)=B+A/(1+Q)^4 … F2: 各向同性 sx=sy=sigma -> Q=0.5·r²/sigma², alpha=sqrt2 sigma, FWHM=2sqrt2 sigma·sqrt(2^{1/4}-1)=1.230310 sigma …」 | ALG-STARPSF-001 §2——**权威** |
| B9 | `lib/algorithms/star_detection/README.md:29-30` | 「+ Moffat4（GSL trust-region LM，7 参数 Gaussian 参数化）逐候选拟合 + …」 | 模块 README（合同入口）——**自相矛盾命名** |

### 1.3 M3b-F-02（Oracle）相关原文

| # | 位置 | 逐字原文 |
|---|---|---|
| C1 | `docs/science/PSF.md:98-99` | 「- **解析解**：各向同性 FWHM/sigma 与 flux 公式的解析一致性（max_abs==0）。 / - **Python 参考**：scipy / NumPy 对同参数 Moffat4 图像块做 curve_fit 复算，位置 <=0.05px、FWHM <=1%（合成无噪声谱）。」 |
| C2 | `docs/algorithms/STAR_PSF_ALGORITHMS.md:192-193` | 「- oracle：解析 Moffat4（beta=4）合成图回收 B,A,cx,cy,sx,sy,theta；flux=2piAsxsy/3 与 FWHM=1.230310·sigma 恒等复核；独立参考不调用生产 symbol（11 号标准 §5）。」 |
| C3 | `docs/algorithms/STAR_PSF_ALGORITHMS.md:201` | 「- 状态：VERIFIED（设计冻结，dpsf 套件建立后 TEST-P1-PSF-001 落 EVIDENCE）。」 |
| C4 | `docs/traceability/TRACEABILITY_MATRIX.json` ::MOD-astrocs-phase1-star-psf（复读实测） | 「"test_id": "TEST-PSF-DESIGN-001" / "test_path": "docs/algorithms/STAR_PSF_ALGORITHMS.md::TEST-PSF-DESIGN-001" / "test_status": "VERIFIED" / "evidence_id": "EVID-MISSING" / "evidence_status": "MISSING"」 |
| C5 | `ENGINEERING_SPEC.md:52-53` | 「- 确定性合成数据生成器； / - 不调用生产实现的独立 Oracle 或解析解；」 |

### 1.4 M1a-A-001 相关原文

| # | 位置 | 逐字原文 |
|---|---|---|
| D1 | `docs/science/ASTROMETRY.md:45-49` | 「前向 WCS (像素->天球): /   xp = x+1,  yp = y+1 /   (u,v) = CD · (xp-CRPIX) + SIP_A/B(u,v)   # (u,v) 为 TAN 投影中间坐标 /   (RA,Dec) = TAN^{-1}(u,v; CRVAL)」 |
| D2 | `docs/science/ASTROMETRY.md:66` | 「与 lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:13-16,153-164,274-420,530-576 及 ipv_select.cpp:723,712 一致。」 |
| D3 | `docs/algorithms/PLATESOLVE.md:16-17` | 「F4: SIP逆向 AP/BP = argmin \|\|UV-(u,v)-SIP(u,v)\|\|² on 7x7 grid, AP[1,0]-=1, BP[0,1]-=1」 |

### 1.5 M1a-C-001 相关原文

| # | 位置 | 逐字原文 |
|---|---|---|
| E1 | `docs/science/ASTROMETRY.md:39` | 「- 维度 w>0,h>0，星点列表非空；CRPIX 按 w/2+0.5 冻结；trans.order 2–3 阶；NB_GRID=7 用于逆向拟合。」 |
| E2 | `docs/science/ASTROMETRY.md:57` | 「  AP/BP = argmin \|\|UV - (u,v)-SIP(u,v)\|\|²  on 7x7 grid」 |
| E3 | `docs/science/ASTROMETRY.md:109` | 「- **SIP 逆一致性**：前向+逆向在 7x7 网格上往返误差 ‖(x,y)-WCS^{-1}(WCS(x,y))‖ < 1e-6 pixel（FP64）。」 |
| E4 | `docs/science/ASTROMETRY.md:125` | 「- FP64 全链路；CD 与 SIP 系数以 double 写入 FITS 头；NB_GRID=7 最小二乘拟合 AP/BP，残差 rms_px 写入诊断。」 |
| E5 | `docs/science/ASTROMETRY.md:145` | 「- **往返不变量**：像素->天球->像素往返 max_abs <1e-6 px（网格 7x7 拟合精度门）。」 |
| E6 | `docs/algorithms/PLATESOLVE.md:124` | 「\| SIP AP/BP 网格反变换 \| ipv_wcs.cpp:400-478 \| 7x7 网格最小二乘；AP[6]-=1、BP[1]-=1（:456-461，F4）；奇异仅 warn（:477） \|」 |
| E7 | `docs/standards/STANDARDS_REGISTRY.md:66,76` | 「\| SIP §A（A/B 前向、AP/BP 逆向与单位线性剔除） \| … \| PROJECT_DEFINED \| … \| DISP-WCS-006（AP/BP 以 7x7 网格最小二乘拟合而非标准迭代反演；SCI 层已显式冻结该口径） \|」／「\| DISP-WCS-006 \| 低 \| docs/algorithms/PLATESOLVE.md \| P1-WCS-IMPL（AP/BP 网格拟合口径与 SCI 冻结一致，维护歧义） \|」 |
| E8 | `tests/unit/p1wcs/p1wcs_tests_apbp.cpp:4-5` | 「// 合同锚: docs/algorithms/PLATESOLVE.md §11.4 F2 (SIP 前向/逆向 roundtrip / // \|Δ\|<=1e-4 px, 冻结不放宽); owner 裁决 1 选 B (2026-09-09): AP/BP 布局扩展 / // + 消费方迭代式反演, 恢复冻结门, 不接受缩小 fixture 畸变量级。」 |
| E9 | `docs/archive/HANDOVER.md:63` | 「\| WCS-002 F1 \| owner 已裁决（选 B） \| 冻结 1e-4px 在高畸变 fixture 下不可达 -> WCS-003 节点：AP/BP 布局扩展+消费方迭代反演，保留高畸变 fixture+增低中高三档，恢复冻结门后 G-SCI 才开 \| WCS-003 \|」——**唯一书面载体是 ARCHIVED 文档** |

---

## 2. 外部依据（文献/参考实现 + 我跑的实验与输出）

### 2.1 星像轮廓与 FWHM 口径（Moffat 1969）

- 轮廓来源：Moffat, A. F. J. 1969, A&A 3, 455（bibcode 1969A&A.....3.....455M），I(r) ∝ (1 + r²/alpha²)^{-beta}。[Wikipedia: Moffat distribution](https://en.wikipedia.org/wiki/Moffat_distribution)（检索命中，页面正文含该族与 beta 的讨论）；仓内 `docs/science/PSF.md:117` 自述「文章级定位（bibcode 1969A&A.....3..455M，**未逐页核验**）」，本报告不提升该引用级别。
- 参考实现：[astropy.modeling.functional_models.Moffat2D](https://docs.astropy.org/en/stable/api/astropy.modeling.functional_models.Moffat2D.html)（词条级定位；该页正文不含 FWHM 关系式，故 FWHM 关系由本报告 §2.2 自解析推导并数值复核，不以网页为据）。
- 标准关系（本报告独立推导 + 数值复核，见 §2.2 与日志 `08_exp_r3.txt` Part 1）：FWHM = 2·alpha·(2^{1/beta}-1)^{1/2}；仓约定 alpha = sqrt2·s，beta=4 ⇒ FWHM = 2·sqrt2·s·(2^{1/4}-1)^{1/2} = 1.230307652590·s。
- 参考实现（检测侧）：sextractor / photutils DAOStarFinder 族以**高斯**母函数做星像定位与形状估计（[photutils 相关文献检索命中](https://iopscience.iop.org/article/10.3847/1538-4357/ae5703)），说明「检测侧用高斯」是领域标准做法，而非实现走样。

### 2.2 实验 Part 1：FWHM/尺度系数独立复算

```text
Moffat beta=4  FWHM/s = 2*sqrt(2)*sqrt(2^(1/4)-1) = 1.230307652590
  库常数 MOFFAT4_FWHM_FACTOR (dpsf_psf.cpp:25)     = 1.230310000000  rel.err = 1.91e-06
Gaussian       FWHM/sigma = 2*sqrt(2 ln2)          = 2.3548200450309493
  库常数 TWO_SQRT_2_LOG2 (sdet_api.cpp:48)         = 2.3548200450309493  rel.err = 0.00e+00
同一 'sx' 列按两种母函数解读的宽度错率 = 1.9140 x
```

### 2.3 实验 Part 3：质心精度 vs SNR（CRLB + 蒙特卡洛，scipy 拟合）

```text
Moffat4 FWHM=3.0px -> s=2.4384px; sigma_n=8.0 ADU; 峰值 SNR := A/sigma_n
  SNR     n   median|dc|    p95|dc|    max|dc|    p95<=0.3  p95<=0.5
    5   200       0.2077     0.4283     0.6276       False      True
   10   200       0.1039     0.2270     0.3002        True      True
   20   200       0.0500     0.1085     0.1371        True      True
   50   200       0.0211     0.0411     0.0502        True      True
  100   200       0.0099     0.0198     0.0265        True      True
参考 CRLB (高斯近似, 同口径, 峰值 SNR=20): 0.0829 px
```

**结论**：以 sigma_centroid ~ FWHM/(SNR·sqrtN) 一族的理论标度为准，|Δc|<=0.3 px 在**峰值 SNR>=10–20** 即有余量地可达（FWHM=3px、21x21 窗时 SNR=20 的 p95=0.109 px）；**门不是「过严」**。但同一门在 SNR=5 时 p95=0.428 px ⇒ 超标——**门的可达性完全取决于 SNR 定义**，而仓内无任何 SNR 定义（见 §4.2）。

### 2.4 实验 Part 4：用高斯母函数拟合 Moffat4 真星（M3b-A-01 影响量化）

```text
高斯拟合 Moffat4 真星 (SNR=200, n=150):
  质心 |dc|       median=0.0047 px  p95=0.0100 px  (质心基本无偏)
  FWHM 报值/真值  median=1.0860
  解析流量比值    median=0.9024
```

**结论**：母函数错配的**主伤害不在质心**（高斯对 Moffat4 的中心无偏到 0.005px），而在**宽度列与流量列跨块不可比**（同一 sx 列两种解读差 1.914x；解析流量差 1/0.902≈1.11x）。⇒ 这条议题**不构成质心门争议**，只构成「列语义/命名」争议。

### 2.5 实验 Part 2：PSF.md:99 承诺的 scipy curve_fit Oracle 现在就能跑

```text
truth      : cx=32.37 cy=28.61 s=1.2  (FWHM=1.476372 px, 无噪声合成图 17x17)
curve_fit  : cx=32.3700000000 cy=28.6100000000 s=1.2000000000
            : |dpos| = 0.000e+00 px   (PSF.md:99 门 <=0.05 px)  -> PASS
            : FWHM rel.err = 1.504e-16  (门 <=1%)      -> PASS
            : 1-sigma 参数误差 cx 2.21e-17 cy 2.15e-17 s 4.85e-17;  wall 9.5 ms
```

**依赖事实**：scipy **已是仓内在用依赖**（`tests/unit/v6_p2_upm/oracle/upm_ma_oracle.py:148` from scipy.optimize import least_squares；`lib/algorithms/coverage/tools/rejection_oracle_compare.py:24`；`tools/quality/frame_qc_grid.py:124`；`lib/algorithms/photometry/cpp/test/test_spectrum_integrator_golden.py`；`ci/tests/test_deep_profiles.py`）。⇒ P1-001 处置表所谓「补 Oracle 需**新依赖**」不成立。

### 2.6 实验 Part 5：uint16 量化对质心的贡献（F4 FP32 门余量）

```text
n=200  SNR(peak)=20  B=100.0 noise=8.0:  uint16 量化引起的质心差
  median=0.0018 px  p95=0.0036 px  max=0.0056 px  (门 0.5 px)
```

### 2.7 实验 Part 6：SIP 逆映射 LS 拟合的网格依赖性（M1a-C-001）

```text
  NG=  7: cond=5.69e+01 | 自网格往返 max=1.820e-12 px | 离网格一步 AP=  3.0983 px | 离网格牛顿8.039e-14 px
  NG= 41: cond=2.77e+01 | 自网格往返 max=9.732e-01 px | 离网格一步 AP=  0.9682 px | 离网格牛顿1.658e-10 px
  NG= 81: cond=2.80e+01 | 自网格往返 max=1.402e+00 px | 离网格一步 AP=  0.9123 px | 离网格牛顿1.285e-10 px
```
（前向畸变模型：F(u)=u+118·(u/512)³ 各轴，等效边缘畸变 118 px，量级对齐仓内 FIX-WCS-F high 档；逆多项式阶 5、归一化坐标，与 `ipv_wcs.cpp:413-433` 同构。）

**结论**：7x7 网格下「自网格往返 1.8e-12 px」是**插值条件**而非重建精度——同一拟合的离网格误差为 **3.10 px**。⇒ SCI 冻结的「7x7 网格上往返 <1e-6 px」在**字面口径下近乎恒真**，对真实重建误差没有分辨力（§4.5）。

### 2.8 实验 Part 7：ASTROMETRY.md:48 逐字公式的量纲检验（M1a-A-001）

```text
  SIP 幅度    1.0 px @1024px, s0=0.9586"/px: 标准形=0.27293472 deg  按 :48 字面=1.27266844 deg  差=  3754.5 px 等效
  SIP 幅度   10.0 px @1024px, s0=0.9586"/px: 标准形=0.27533122 deg  按 :48 字面=10.27266844 deg  差= 37544.8 px 等效
  SIP 幅度  117.0 px @1024px, s0=0.9586"/px: 标准形=0.30382294 deg  按 :48 字面=117.27266844 deg  差=439273.8 px 等效
```

### 2.9 实验 P1：真实源码探针（sdet + dpsf 静态库，非模拟）

`run/PROJECT-GOVERNANCE-01/R-3/probe/probe_convention.cpp` 直接用本树 `build/libastrocs_p1_sdet.a` + `build/libastrocs_p1_dpsf.a` 编译，合成 Moffat4(beta=4) 星场（采样于 x+0.5 像素中心，FWHM=3px，256x256，40 星，背景 120，噪声 8）：

```text
##### SNR=300
== Part A: sdet_detect_ex_f64 rc=0 count=36 (truth=40)
   [sdet] n=36  vs truthFITS : median=0.0035 p95=0.0061 max=0.0084
   [sdet] n=36  vs truthIndex: median=0.7059 p95=0.7124 max=0.7154
== Part B0: init=182.9472 (truthFITS=182.9472 truthIndex=182.4472) rc=0 status=0 -> dpsf cx=182.447245  (cx-truthFITS=-0.5000, cx-truthIndex=+0.0000)
   [prod] vs truthFITS : median=0.7081 p95=0.7108 max=0.7111
   [prod] vs truthIndex: median=0.0031 p95=0.0063 max=0.0076
== Part D: orchestrator bridge chain (write -0.5 / read +0.5), signed dx vs truthFITS
   [D] astro_det PSF branch (= dpsf raw)   : median=-0.5000 p95(|.|)=0.5040
   [D] astro_det fallback (raw sdet)       : median=+0.0001 p95(|.|)=0.0047
   [D] same-star PSF-vs-fallback separation = 0.5001 px (dedup threshold strictly <0.5)
```
（SNR=50/100 复跑给出一致结论：sdet vs truthFITS median 0.0100–0.0202 px；dpsf cx - truthFITS = -0.4993…-0.5000；PSF 支路 -0.5002…-0.4999；分离度 0.4997–0.5004 px。SNR=20 时因 5-sigma 平滑阈值该场只检出 0 星——见 §4.2 的 SNR 定义敏感性。）

---

## 3. 实现事实（命令 + 逐字输出 + 文件:行，与 §1/§2 逐条对照）

### 3.1 sdet 与 dpsf 的坐标约定（对 A8 的判决）

`lib/algorithms/star_detection/src/sdet_api.cpp:107-135` 逐字：

```c
// 参数: x[0]=B, x[1]=A, x[2]=x0, x[3]=y0, x[4]=SX=2σ², x[5]=fr, x[6]=alpha
// tmpx = ca*(j+0.5-x0) - sa*(i+0.5-y0), IPv samples.dx 已含 +0.5
// f[k] = B + A*exp(-(tmpx²/SX + tmpy²/SY)) - y[k]
static int sdet_gaussian_f(const gsl_vector* x, void* params, gsl_vector* f) {
```

`lib/algorithms/psf/src/dpsf_psf.cpp:294-296` 逐字：

```c
            SamplePixel sp;
            sp.dx = static_cast<double>(x) - cx;
            sp.dy = static_cast<double>(y) - cy;
```

`lib/algorithms/psf/src/dpsf_psf.cpp:434-438` 逐字：

```c
    // Phase1 Final Closure (PSF-001): 样本 dx = pixel_x - cx,
    // 拟合 x0 是相对传入中心 cx 的偏移, 正确还原为 cx + x0。
    // 原实现用 rect 中心近似 cx, 对奇数宽 rect 引入 ~0.5px 系统偏差。
    double img_cx = cx + x0;
```

**实测判决（§2.9）**：对同一颗物理中心在 truthFITS = p 的星，
- sdet 返回 ~p（median 偏差 0.0035 px @SNR300）⇒ **sdet 输出「像素中心=索引+0.5」系**（与 A9/DATA-P1-STAR §17.2 一致）；
- dpsf 返回 p - 0.5000（三种初值 p / p-0.5 / p+0.5 都收敛到同一点）⇒ **dpsf 输出「索引即中心」系**，**并非** A8 所称「与输入 det_x 同系」。

### 3.2 编排层桥接（对 A10/A11 的判决）

`lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h:345-346` 逐字：

```c
    static double astro_coord_to_unified(double v) { return v - 0.5; }    // 连续系 -> 统一契约 (写端)
    static double astro_coord_from_unified(double v) { return v + 0.5; }  // 统一契约 -> IPV 接口契约 (读端)
```

`orchestrator.cpp:2494-2504` 逐字：

```c
            if (psf_valid) {
                // cx (PSF 拟合中心, "像素中心=索引+0.5" 系) -> 统一契约
                row[1] = astro_coord_to_unified(prow[3]);
                row[2] = astro_coord_to_unified(prow[4]);
            } else {
                // PSF 失败: 检测坐标 (sdet 像素中心=索引+0.5) 显式转统一契约 (索引即中心)
                row[1] = astro_coord_to_unified(cx_arr[static_cast<size_t>(i)]);
                row[2] = astro_coord_to_unified(cy_arr[static_cast<size_t>(i)]);
            }
```

`orchestrator.cpp:2334-2338`（dpsf 初值取 sdet 原值）与 `orchestrator.cpp:1851-1856`（fallback **直送**，无 -0.5）逐字：

```c
    for (int i = 0; i < n_stars; ++i) {
        cx_arr[i] = det_x[i];
        cy_arr[i] = det_y[i];
    }
...
            if (dup) continue;
            push_det(x, y, d[2], d[3], d[4], d[5]);
            ++stats.n_fallback;
```

**判决**：写端对 dpsf 输出再减 0.5（因为误信 A8），读端又加回 0.5 —— 二者在**往返意义上互相抵消**（故 `test_p1_batchH_star_coord.cpp` 的 from(to(x))=x 断言恒绿），但**绝对值错了 0.5 px**：

- astro_det 的 **PSF 支路** = dpsf 原值 = truth - 0.5000；
- astro_det 的 **fallback 支路** = sdet 原值 = truth ± 0.0002；
- 同星两来源分离度 **0.5000 px**，而 `orchestrator.cpp:1849` 的去重阈值为 `std::hypot(...) < 0.5`（严格小于）⇒ **恰 0.5 不去重**，同一颗星会以 0.5px 错位双份进入 ipv。

即：A8 的判据前提（"DPSF 拟合中心与输入 det_x 同系"）经实测**为假**，从而 test_p1_batchH_star_coord（该文件**未被任何 CMakeLists 引用**，见 §3.5）所"锁定"的修复在绝对值上未成立。

### 3.3 检测侧拟合内核（对 B1/B6 的判决）

`lib/algorithms/star_detection/src/sdet_api.cpp:110,375-376,412-413` 与 `:520,627` 逐字：

```c
static int sdet_gaussian_f(const gsl_vector* x, void* params, gsl_vector* f) {
...
    fdf.f = &sdet_gaussian_f;
    fdf.df = &sdet_gaussian_df;
...
    double fwhm_x = sx * TWO_SQRT_2_LOG2;
    double fwhm_y = sy * TWO_SQRT_2_LOG2;
...
int sdet_moffat4_fit(const T* image, int width, int height,
...
    int gsl_status = sdet_lm_fit<T>(image, width, height,
```

**判决**：生产检测内核（sdet_moffat4_fit -> sdet_lm_fit -> sdet_gaussian_f/df）是**椭圆高斯**（fwhm=2.3548·sx）；而 `lib/algorithms/psf/src/dpsf_psf.cpp:25,393-394` 的 PSF 侧是真 Moffat4（MOFFAT4_FWHM_FACTOR=1.230310）。B1/B7 的「Moffat4 GSL-LM 中心」文本**与实码互斥**；B6 的公式与 B2 的测试设计（合成高斯星场）与实码一致。

### 3.4 三条支路的坐标系（合同-实码互斥）

| 支路 | 合同说 | 实码 | 实测（§2.9） |
|---|---|---|---|
| sdet 输出 | A9「像素中心=索引+0.5」 | `sdet_api.cpp:109,131` 采样 x+0.5 | +0.5 系 ✓ |
| dpsf 输出 | A8「与输入同系（=+0.5 系）」 | `dpsf_psf.cpp:295` 采样 x | **索引系 ✗** |
| star_measurements 写端 | A10「统一契约 index-is-center」 | PSF 支路 -0.5、fallback -0.5 | **两支不同系（差 0.5px）✗** |
| astro_det 读端 | A10「fallback 已 +0.5 直送；PSF 支路 +0.5」 | PSF +0.5、fallback 直送 | **PSF 支路偏低 0.5px ✗** |
| fallback 的 -0.5 | A11「按…-0.5 转统一契约」 | 无 -0.5（直送） | **A11 已过期 ✗** |

### 3.5 门与测试的登记事实

- `ctest --test-dir build -R "p1psf_|p1star_"` -> **16/16 Passed**（日志 `01_ctest_p1psf_p1star.txt`）；`-R p1wcs_` -> **9/9 Passed**（`03_ctest_p1wcs.txt`）。
- 仓内 F1 合成场实测（`p1star_tests units` 逐字）：`[f1] truth=40 snr20=32/32 snr10=32/32 count=32 fp=0 (0.000/kpx) mean_dc=0.0059 max_dc=0.0535` ⇒ **F1 的 0.3px 门在仓内实现下达标且有 ~5.6x 余量**（`02_oracle_detail.txt`）。
- B4-5 共址锁（`p1psf_prodpath_centroid`）逐字：`T1 fit=(31.370000,27.610000) truth=(31.370000,27.610000) |dc|=0.000000 px`；`p1psf_prodpath_check.cpp:99` 以 CX+0.5 作初值（即承认 sdet 系 = 索引+0.5），而真值 CX 定义在 `moffat4_eval(...,(double)x,(double)y)`（索引系）⇒ **该锁自身即证明 dpsf 输出为索引系**。
- `lib/infrastructure/pipeline/orchestrator/cpp/tests/test_p1_batchH_star_coord.cpp` **未被任何 CMakeLists 引用**（`grep -rn "test_p1_batchH_star_coord" --include=CMakeLists.txt .` -> 0 命中；`build/` 下无该目标）⇒ 它是**未入门禁的锁**。
- `ci/checks.json` 对 `gate2_psf_oracle` **0 命中**（`grep -c 'gate2' ci/checks.json` -> 0）；`gate2_psf_oracle.py:314-318` 的 `fwhm_median_le_1pct / ell_median_le_0.005 / flux_median_le_1pct / photutils_oracle_centroid_p95_le_0.05px` 四个阈值在 `docs/**`（非 archive）**全文零命中**。
- `TRACEABILITY_MATRIX.json` 的 PSF 行：`test_path` = **文档锚** `docs/algorithms/STAR_PSF_ALGORITHMS.md::TEST-PSF-DESIGN-001`，`test_status=VERIFIED` 与 `evidence_status=MISSING` 并存；`docs/traceability/TRACEABILITY_MATRIX.csv:11-12` 两行已标 `SUPERSEDED`。

### 3.6 SIP 逆映射实现与实测（对 E1–E7 的判决）

`lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:395-414` 逐字（节选）：

```c
            // 5.3 SIP AP/BP: 网格反变换法 (WCS-002/DISP-WCS-008 整改:
            // 网格加密 7×7->41×41、拟合阶 trans.order->AP 布局上限 5、
            // 各向归一化正规方程)
...
            // 可达性 (F2 冻结 fixture 实测, 探针 run/tmp_wcs002): 边缘畸变
            // ~117px 时 5 阶最优 center90 roundtrip ~7.6 px, 为逆映射最近
            // 奇点决定的逼近极限 (order 6..15 -> 5.3..0.26 px, 收敛率~0.73);
            // 冻结 1e-4 px 在该 fixture 下数学不可达, 需负责人裁决 (finding)。
            const int NB_GRID = 41;       // 每轴网格点数 (整改: 7 -> 41)
            const int AP_FIT_ORDER = 5;   // 逆向拟合阶 (SIPCoeffs i*6+j 布局上限)
```

同文件 :537-538：`const int NB_GRID_X = 81;  // 每轴网格点数 (5.3 段 41 -> 81)` / `constexpr int APX_ORDER = 7;`。同文件 :17、:238 与 `ipv_solver.cpp:17` 仍写 NB_GRID=7（旧口径注释残余）。

**本机实测（`build/tests/unit/p1wcs/p1wcs_tests apbp` 逐字，`04_p1wcs_apbp.txt`）**：

```text
[WCS-003] low:  edge_dist=18.36px c90_max=3.299e-10 bnd_max=2.534e-10 rnd_max=2.787e-10 truth_max=3.299e-10 apx1s=3.619e-07 ap36_1s=8.774e-05
[WCS-003] mid:  edge_dist=91.81px c90_max=1.518e-09 bnd_max=1.226e-09 rnd_max=1.655e-09 truth_max=1.518e-09 apx1s=3.288e-02 ap36_1s=2.813e-01
[WCS-003] high: edge_dist=183.62px c90_max=1.655e-09 bnd_max=2.457e-09 rnd_max=2.907e-09 truth_max=1.655e-09 apx1s=3.462e+00 ap36_1s=7.601e+00
P1WCS APBP PASS
```

⇒ 「冻结 1e-4 px 数学不可达」对**当前实现（41x41 + APx 阶 7 + 迭代反演）不成立**：三档畸变下 roundtrip 实测 3.3e-10 ~ 2.9e-9 px，全门通过。代码注释里的 7.6 px 是 ap36_1s（**一步直加**配置），与生产迭代路径不是同一量。

---

## 4. ⭐ 门禁自审

### 4.1 D-16 的「0.5px 质心门」本身是否正当

| 审查项 | 判决 |
|---|---|
| (1) 有无权威依据 | **有**：ALG-STARDET-001 §11.4 F4（A3）「FP32 通道经 uint16 量化容差独立冻结 \|Δc\|<=0.5 px」；上位 SCI-P1-STAR-001 §1（A1）只冻结 0.3 px。⇒ **0.5px 是"实现通道容差"，不是科学门**，且比 SCI 门更宽。 |
| (2) 可测量/可复现 | 可测：§2.6 实测 u16 量化贡献 median 0.0018 / p95 0.0036 / max 0.0056 px；§2.9 实测端到端。**但仓内没有把 sdet+dpsf 连起来的可执行门**（B4-5 只造场直接喂 dpsf；batchH 只算桥函数算术且未注册）。 |
| (3) 是否把实现缺陷误判成文档错误（或反之） | **是（双向误判）**：DOC-001 把 M3b-G-01（登记面缺失）与 M1a-A-002（F1 量测域缺失）合并成「0.5px 门 vs 0.897px 实测」的**阈值二选一**，而两者都不是阈值问题；真正的实现缺陷（§3.2 的 0.5px 双扣）**未被任何门发现**。 |
| (4) 阈值/量测域是否定义清楚 | **阈值清楚、量测域不清楚**：F4 未定义 SNR、未定义统计量（逐星/median/p95/max）、未定义合成场参数。 |
| (5) 误报/漏报风险 | **漏报极大**：三处"通过"（gate2 的 0.3px 生产门、F4 的 0.5px 门、batchH 的往返恒等）都无法发现 0.5px 系统偏差——因为前者在 Windows 侧用合成真值直接配对（口径自定），中者不接 sdet+dpsf 链，后者只测桥算术。**误报**：若换 SNR 定义（§2.3 SNR=5 时 p95=0.428px），同一门会闪红。 |

**门应为 X**：把这条门重写为
> **G-P1-CENTROID-1（生产路径绝对位置门）**：以解析 Moffat4（beta=4）合成场为真值，**链接 sdet 检测 + dpsf 拟合 + 编排桥**三条真实实现，量测 astro_det 输出对真值的偏差；域 = SNR_peak>=20 且非饱和、非边缘；判据 = median <=0.1 px 且 p95 <=0.3 px；并对 **PSF 支路与 fallback 支路分别断言**（两条支路必须同系：\|median_PSF - median_fallback\| <= 0.05 px）。
> 原有 \|Δc\|<=0.5 px 降级为 **F4 通道量化容差**（保留，但注明「仅 u16 量化面，不构成端到端门」）。

### 4.2 §2.3 暴露的次级门缺陷：**SNR 无定义**

`grep -rn 'SNR>=' docs/ --include=*.md` 只命中 `STAR_DETECTION.md:14` 与 `STAR_DETECTION_ALGORITHMS.md:257` ——**全文没有 SNR 的定义**（峰值/总流量/孔径/5-sigma 阈值之上？）。实验证据：同一合成场在"峰值 SNR=20"下 sdet **检出 0 颗星**（平滑 sigma=2 削弱峰值后低于 median+5·bgnoise 阈值），而 SNR=50 时检出 36/40 ⇒ 门的可达性被 SNR 定义完全支配。这是 F-2 在本域最硬的实例。

### 4.3 F1 精度门（M1a-A-002）自审

| 审查项 | 判决 |
|---|---|
| (1) 依据 | 有：A4（ALG-WCS-001 §11.4 F1）+ A5（RMS 公式锚）。 |
| (2) 可测量/可复现 | 可测（rms_arcsec 是返回值字段），但**"对谁测"未写**。 |
| (3) 误判方向 | 现有表述使「内点 trans 域 RMS=0.151"」与「全帧头域 median=0.897px≈0.887"」都能自称"满足/不满足 F1"。**判据不能唯一判定 ⇒ 是判据缺陷，不是实现缺陷也不是文档笔误**。 |
| (4) 量测域 | **未定义**（唯一实质缺陷）。 |
| (5) 误报/漏报 | 漏报：内点域 RMS 天然小于全帧域（0.151 vs 0.887），门可被"内点口径"合法满足；误报：若改用全帧域则同一产品必红。 |

**门应为 X**：F1 门值 0.5" **保留不动**，补一句量测域冻结：「F1 判据 rms_arcsec 定义在 trans 拟合**内点集**（n_pairs>=12）与**合成线性场**上；它**不是**产品级天测精度门」。同时把外部闭环量（全帧头域 median/p95）另立为 G-P1-WCS-CLOSURE（证据面，非 F1 替代），其阈值需另行标定——**不把 0.897px 当作 F1 的超标证据**（该值来自 v1.2 legacy 工具、2 帧、s0≈0.989"/px 换算，其量测域与当前合同不同）。

### 4.4 M3b-A-01 自审：「禁高斯主路径」这条门是否正当

| 审查项 | 判决 |
|---|---|
| (1) 依据 | B4 是 SCI-PSF-001 §10 的禁则；但 §13（B5）把 SCI-PSF-001 的实现面绑定到 dpsf_psf.cpp。⇒ 该禁则的**规范对象是 PSF 模块**。B2（STAR_DETECTION.md §2）把它扩张到检测域。 |
| (2) 可测量 | 可测量（§3.3 指纹式验证）。 |
| (3) 误判方向 | 把「检测侧实现选择」误判成「SCI 被违反」。检测侧用高斯是领域标准（§2.1），且 B6/B7/A2 的冻结文本自身就写高斯式与"合成高斯星场"。⇒ **门本身需要改写，而不是实现需要改**。 |
| (4) 阈值/域 | 该"门"没有阈值，只有一条禁止项；域（哪个模块）没写清 ⇒ 缺陷在范围。 |
| (5) 误报 | 现状就是一次误报：一份**同时写着高斯公式**的 ALG 被用来指控实现"违反 Moffat4 冻结"。 |

**门应为 X**：`PSF.md §10` 首条改为「**在本模块（lib/algorithms/psf，SCI-PSF-001 §13）内**引入未文档的高斯备选拟合路径作为主路径」；`STAR_DETECTION.md §1/§3` 与 `STAR_DETECTION_ALGORITHMS.md §9` 改为「检测侧=椭圆高斯（本页 §2 离散公式），PSF 侧=椭圆 Moffat4（SCI-PSF-001 §5）；两列 sx/fwhm **不可跨块比较**，换算 FWHM_gauss = 1.9140 x FWHM_moffat(同 sx)」；`sdet_moffat4_fit` 更名 `sdet_gauss_fit`；`README.md:29` 去「Moffat4」。

### 4.5 M1a-C-001 自审：「7x7 网格往返」这条不变量是否正当

| 审查项 | 判决 |
|---|---|
| (1) 依据 | E1–E5 是 SCI-WCS-001 冻结文本；E7 是 PROJECT_DEFINED 偏差登记。 |
| (2) 可测量/可复现 | 「7x7 网格往返」可测量——**但它测的是拟合自身的插值条件**（§2.7：自网格 1.8e-12 px vs 离网格 3.10 px）。 |
| (3) 误判方向 | **把实现细节（网格点数）当成科学定义冻结**，导致实现（41x41/81x81 + 迭代反演，为满足**实质门 1e-4 px** 而做）在文本上永远"违规"；同时把实质门写成"数学不可达"。 |
| (4) 阈值/域 | 阈值（1e-6 px）与域（7x7 网格）都"清楚"，但**域与拟合域重合 ⇒ 空洞**。 |
| (5) 误报/漏报 | 漏报：一步 AP 的 3.10 px 全局误差在自网格上完全隐形（这正是 WCS-002/F1 事故的成因）。误报：41x41 实现在 SCI 字面下"违反 NB_GRID=7"。 |

**门应为 X**：SCI §7/§11 改为
> 「**SIP 逆一致性**：逆映射以**迭代反演**（牛顿，判据 \|F(u)-UV\|inf < tol）为准；不变量在**独立于拟合采样的密集域**（中心 90% + 四边 + 四角 + >=1000 随机点）上测量，判据 max‖·‖ <= 1e-4 px（PLATESOLVE §11.4 F2 同值）。」
> NB_GRID 从 SCI/ALG 的科学定义中移出，改为「采样网格 >=7x7（实现值：AP/BP 41x41 阶 5；APx/BPx 81x81 阶 7，登记 DISP-WCS-008）」。

### 4.6 M3b-F-02 自审

| 审查项 | 判决 |
|---|---|
| (1) 依据 | 有：C1（PSF.md §11 承诺）、C5（ENG_SPEC §5.1 独立 Oracle）。 |
| (2) 可测量 | 可测量，且**依赖已具备**（§2.5，scipy 在仓内已用）。 |
| (3) 误判方向 | 原判「补 Oracle 需**新依赖与验收口径裁决**」（P1-001 处置表第 95 行）**前提不成立**；但"VERIFIED 声明缺证据"这一半**成立**：C4 的 test_path 仍是文档锚且 evidence_status=MISSING。 |
| (4) 域 | 承诺的域（"合成无噪声谱"）写清了；缺的是**落地与登记**。 |
| (5) 误报/漏报 | 漏报：test_status=VERIFIED 与 evidence_status=MISSING 并存可长期共存，因为没有门做这两个字段的一致性检查（ENGINEERING_SPEC.md:120 要求「核心合同有独立测试」，但矩阵的 test_path 是文档锚时该检查形同虚设）。 |

**门应为 X**：保留门，另加一条**机器门**：「TRACEABILITY_MATRIX 中 test_path 必须解析到**可执行目标**（add_test 名或测试源路径），不得为 docs/**.md::ID；test_status=VERIFIED ⇒ evidence_status != MISSING」。

### 4.7 F-2 普查：本域有哪些门、哪些有冻结表

```text
门 ID                         | 权威落点                        | 阈值 | 量测域 | 统计量 | SNR 定义 | 标定依据 | 可执行门
G-P1-CENTROID-SCI  (0.3px)   | SCI-P1-STAR-001 §1 (A1)         | 有   | 合成场 | 无     | 无       | 无       | p1star_units(F1) 有
G-P1-CENTROID-U16  (0.5px)   | ALG-STARDET §11.4 F4 (A3)       | 有   | u16 通道| 无    | 无       | 无       | p1star units (f4_u16) 有
G-P1-PSF-ORACLE-F64 (0.05px) | ALG-STARDET §11.4 F4 (A3)       | 有   | 合成场 | 无     | 无       | 无       | p1star_oracle 有
G-P1-PSF-POS/FWHM (0.05px/1%)| SCI-PSF-001 §11 (C1) + ALG §9  | 有   | 图像块 | 无     | 无       | 无       | p1psf_oracle 有
G-P1-WCS-F1 (0.5")           | ALG-WCS-001 §11.4 F1 (A4)       | 有   | 未定义 | 未定义 | n/a      | 无       | 无（无可执行 F1 目标）
G-P1-WCS-F2 (1e-4px)         | ALG-WCS-001 §11.4 F2 + E6       | 有   | 中心90%| max    | n/a      | 无       | p1wcs_apbp 有
G-P1-WCS-RT (1e-6px)         | SCI-WCS-001 §7/§11 (E3/E5)      | 有   | 7x7 网格(=拟合域, 空洞) | max | n/a | 无 | p1wcs_apbp(部分)
gate2: fwhm/ell/flux/photutils| 仅 scripts/gate2_psf_oracle.py | 有   | 合成场 | median/p95 | 无  | 无       | 否（ci 未注册）
```

⇒ **没有任何一条门有"阈值来源/标定证据"字段**；G-P1-WCS-F1 连量测域都没有；G-P1-WCS-RT 的量测域与拟合域重合（空洞）；gate2 的 4 个阈值在权威面零命中且未进入 ci/checks.json。这正是 F-2 的共性：**门值都是真的，但没有一条能回答"这个数从哪来、在哪个域上测、测什么统计量"**。

### 4.8 议题前提不成立的清单（按硬约束 4 直接判定）

1. **D-16 的前提不成立**：「0.5px 门 vs 0.897px 实测」不构成一对可比的量（不同门、不同域、不同工具代）。
2. **M3b-A-01 的门不成立**：「实现必须用 Moffat4」不是 SCI-PSF-001 对检测模块的约束（范围被越界扩张）。
3. **M3b-F-02 的阻塞前提不成立**：「补 Oracle 需新依赖」为假。
4. **M1a-C-001 的"数学不可达"不成立**：ipv_wcs.cpp:408-411 的自注对当前实现为假（实测 1e-9 级通过 1e-4 门）。
5. **M1a-C-001 的"7x7 往返 <1e-6"门不成立**：自证门（域=拟合域）。
6. **A11（DATA_SEMANTICS:714）与实码互斥**：fallback 并无 -0.5 ⇒ 该合同句已过期。

---

## 5. 建议裁决（逐议题：唯一推荐 + 置信度 + 反方 + 最小改动路径 + 影响面）

### 5.1 D-16 / M3b-G-01 / M1a-A-002（合并裁决）

**唯一推荐结论**：
1. **不调任何门值**。0.3 px（SCI）与 0.5 px（F4 u16 通道）都保留；实测两门都可达且有量级余量（§2.3/§2.6/§3.5）。
2. **把「0.5px」定性为 F4 的 u16 量化通道容差**，并在 ALG-STARDET-001 §11.4 F4 补一句「本项只覆盖 u16 量化面，不构成端到端位置门」。
3. **把 D-16 的整改对象改为端到端坐标契约**：修正编排桥，使 PSF 支路与 fallback 支路处于同一坐标系（实测现状相差 0.5000 px）。修正方向以使 **astro_det 对真值无偏**为准（实测 fallback 支路已无偏、PSF 支路 -0.5000 px）⇒ **PSF 支路写端不再 -0.5（以 dpsf 原值即统一契约值）**。
4. **新增 G-P1-CENTROID-1（§4.1）** 作为真正的门：链接 sdet+dpsf+桥，双支路分别断言，median<=0.1 px 且 p95<=0.3 px，并断言两分支 \|Δmedian\|<=0.05 px。
5. **F1（plate solve）只补量测域，不动 0.5"**；0.897px 归入新的外部闭环证据面（§4.3），不作为 F1 超标证据。

**置信度**：对第 1/2/4/5 条 = **高**（均有本机可复现实验）；对第 3 条的**修正方向** = **中高**（依赖"统一契约 = 索引即中心"这一合同定义与 dpsf 采样式的联合解释；若负责人改判"统一契约应为 +0.5 系"，则改为给 fallback 支路补 -0.5 并同步改 A9/A10 与 test_p1_batchH_star_coord）。

**反方论证（最可能被反驳的点）**：
- 「dpsf 输出与输入同系」是仓内**两处独立文本**（A8 测试头注、orchestrator.cpp:2486-2488 注释）的主张，我的结论与它们相反。反驳会聚焦：我的合成场把恒星中心放在 x+0.5 采样点，是否等价于"sdet 的输出系"？——反驳可被实验消解：探针 Part B 用 p / p-0.5 / p+0.5 三种初值都得到同一 p-0.5，说明输出系由 dx = x_index - cx 唯一决定，与"我如何造场"无关；且仓内 p1psf_prodpath_check.cpp:99 的 CX+0.5 初值构造与我的结论互相印证。
- 「0.5px 是 legacy 问题、R8-A 已修」：反驳点是 git 历史；我的证据是**当前树跑出来的**，与该主张直接冲突。

**最小改动路径**：

| 动作 | 文件 | 是否需变更 claim |
|---|---|---|
| PSF 支路写端去 -0.5（或改读端只对 fallback 加 +0.5） | `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:2496-2504` | 否（实现对齐已冻结的 A10 合同） |
| 让 test_p1_batchH_star_coord 进入构建，并改为**链接 sdet+dpsf** 的绝对位置门 | 新增 CMake 条目 + 该 .cpp 改造 | 否 |
| 把 B4-5 锁升级为双支路断言 | `lib/algorithms/psf/tests/p1psf/p1psf_prodpath_check.cpp` | 否 |
| F4 补"仅量化面"限定语；F1 补量测域句 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:263-265`、`docs/algorithms/PLATESOLVE.md:185-187` | 否（澄清，不改阈值） |
| 作废/订正 A11 | `docs/contracts/DATA_SEMANTICS.md:714` | 否（订正为实码事实） |

**影响面**：`lib/infrastructure/pipeline/orchestrator`（坐标桥）、`lib/phase1/stars`/`lib/algorithms/star_detection`（下游坐标消费）、`lib/algorithms/platesolve`（输入坐标）、`contracts/` 的 DATA-P1-STAR §17.2 / DATA-P1-WCS §18.1、`docs/algorithms/PLATESOLVE.md §11.1` 的 orchestrator 行、`tests/unit/p1wcs` 与 `p1psf/p1star` 的期望值（凡以绝对坐标比对真值的用例都要重锚）。**风险**：改桥会改变已发布产品的像素坐标 0.5px ⇒ 必须走「产品面影响评估 + 版本递增」，不得静默改。

### 5.2 M3b-A-01（高斯 vs Moffat4）

**唯一推荐结论**：**保留检测侧高斯实现**（不改码），改为**冻结双模型语义**：
- SCI-P1-STAR-001 §1/§3：把「Moffat4 GSL-LM 中心」改为「连续估计（一阶导零交叉 / 二阶导零交叉 / **椭圆高斯 GSL-LM 中心**）」；§3 基线句改为「peaker 七步候选 + **椭圆高斯** GSL trust-region LM 拟合」。
- STAR_DETECTION_ALGORITHMS.md §9：改为「检测侧椭圆高斯由 GSL LM 收敛；PSF 侧 Moffat4 见 SCI-PSF-001 §5；两模型**宽度列不可互换**（FWHM_gauss/FWHM_moffat = 1.9140 @同 sx）」。
- PSF.md §10：首条加范围限定「在本模块内（§13 实现面）」。
- sdet_api.cpp 函数改名 `sdet_moffat4_fit -> sdet_gauss_fit`；`README.md:29` 去 Moffat4；`DATA_SEMANTICS §17.2:703` 的「正常星=Moffat4 振幅 A」改为「高斯峰值振幅 A」并登记 DISP。
- 追加登记 DISP-STAR-00x「检测/PSF 双母函数」到 ALG-STARDET-001 §11.3 与 registry。

**置信度**：**高**（实码指纹 + 权威自相矛盾 + 领域惯例三重支撑）。

**反方论证**：最强反驳是「SCI-PSF-001 §10 的禁则本意就是禁止**任何**高斯主路径（含检测），因为它会把 q_psf/FWHM 语义污染成两套」。回应：污染已由 §4.4 的换算声明与列改名消除，而 `q_psf=A/residual_scale` 只存在于 dpsf 侧（PSF.md:24,88），检测侧不产出 q_psf ⇒ 禁则的立法目的（保护 q_psf/PSF 语义）不受影响。第二反驳：「改 SCI 就是放宽冻结」——本改法是**缩小**禁则的适用范围至其 §13 已声明的实现面，不是放宽本模块的实现约束。

**最小改动路径**：`docs/science/STAR_DETECTION.md:12-14,41-42`、`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:60,169-171`、`docs/science/PSF.md:94`、`docs/contracts/DATA_SEMANTICS.md:703`、`lib/algorithms/star_detection/README.md:29`、`sdet_api.cpp` 函数名（含 PUBLIC_API.md:549/567/608 中出现的符号）、`docs/modules/registry/astrocs.phase1.star-psf.md`。

**影响面**：SCI 变更 claim（docs/science/STAR_DETECTION.md 是 FROZEN）；ALG-STARDET-001 符号锚表；traceability src_path 的符号名；p1star 测试注释与 extras 列名；跨块比较 FWHM 的任何下游。

### 5.3 M3b-F-02

**唯一推荐结论**：**保留门 + 落 Oracle + 加机器门**：
1. 新增 `tests/backend/test_psf_moffat_oracle.py`（scipy curve_fit，无噪声合成 Moffat4 块；断言位置 <=0.05px、FWHM <=1%；含参数不确定度与耗时上限），证据落 run/ 下证据目录，注册进 ci/checks.json 并留负例（把 beta 改成 3.9 必红）。
2. 修订 TRACEABILITY_MATRIX.json 的 MOD-astrocs-phase1-star-psf：test_path -> `lib/algorithms/psf/tests/p1psf (ctest p1psf_oracle/p1psf_prodpath_centroid) + tests/backend/test_psf_moffat_oracle.py`；evidence_id/status 落 EVID。
3. 加机器门（§4.6）：「矩阵 test_path 不得是 docs/**::ID；test_status=VERIFIED ⇒ evidence_status != MISSING」。

**置信度**：**高**（Oracle 已在 9.5 ms 内跑通）。

**反方论证**：反驳可能是「Oracle 与生产同参数化，独立性不足」——回应：ENG_SPEC §5.1 要求的是「不调用生产实现的独立 Oracle **或解析解**」；本 Oracle 用**第三方优化器 + 解析模型**，且噪声面用蒙特卡洛分布断言（而非单点值），已满足；若要更强独立性，可加「由生产系数反投影回图像再独立拟合」的双向校验。

**最小改动路径**：新增 1 个测试文件 + ci/checks.json 1 条 + 矩阵 1 行 + STAR_PSF_ALGORITHMS §11.4 状态句（C3 的「VERIFIED（设计冻结…后落 EVIDENCE）」改为「设计冻结；证据见 EVID-…」）。

**影响面**：追溯门、CI 检查表、DOC-001 遗留项。

### 5.4 M1a-A-001

**唯一推荐结论**：**以实现为准订正 SCI**（实现零改动）：
- ASTROMETRY.md:48 -> `(xi,eta) = CD · [ (xp-CRPIX) + SIP_A/B(xp-CRPIX) ]`（并显式声明自变量为**像素偏移**、A/B 单位 1/px^{i+j-1}、输出 px）。
- ASTROMETRY.md:57（§5a 重复处）同步。
- ASTROMETRY.md:66 的一致性声明由行号区间改为符号锚（`ipv_wcs.cpp::solve_pix2world_iterative`）。
- ASTROMETRY.md:168 的 Shupe 2005 条目保持「未逐页核验」标注或升级为逐页核验。
- 无需新增 claim（属订正为 SIP 标准形；若流程要求，则并入同批 SCI 变更 claim）。

**置信度**：**高**（量纲不闭合在 §2.8 已量化到 10³–10⁵ px 等效；实现侧形式与 SIP 标准一致且被 p1wcs_astropy_cross 交叉验证通过）。

**反方论证**：反驳可能是「:48 只是记号简写，读者自会理解为像素域」。回应：文中显式写 `# (u,v) 为 TAN 投影中间坐标`，且 :66 用它作"与实现一致"的证据 ⇒ 不是简写而是定义；同节 :57 的 AP/BP 公式也把 (u,v) 当像素量用 ⇒ 同一符号在同节两义。

**最小改动路径**：1 份 SCI 文档 3 处（:48/:57/:66）。**影响面**：只影响文本与第三方复算指引，不影响任何数值/测试。

### 5.5 M1a-C-001

**唯一推荐结论**：
1. SCI §4/§5/§7/§9/§11 的 NB_GRID=7 / on 7x7 grid 改为**采样下界**：「网格 >=7x7；实现值 AP/BP 41x41（阶 5）、APx/BPx 81x81（阶 7）」。
2. SCI §7/§11 的往返不变量改为**独立密集域**判据（同 §4.5「门应为 X」），阈值保持 1e-4 px（与 ALG F2 同值），删去 1e-6 px 的自网格口径（或标注为"拟合域插值条件，不作重建精度门"）。
3. ALG PLATESOLVE.md:7/17/42/55/74/124/167 同步；:124 的行号锚订正为 `ipv_wcs.cpp:395-527`。
4. 删除 ipv_wcs.cpp:408-411、:17、:238、ipv_solver.cpp:17 的旧 NB_GRID=7 与"数学不可达"注释（改为指向本机实测值 1.7e-9 px 与 p1wcs_apbp 断言）。
5. 登记 DISP-WCS-008（网格/阶扩展 + 迭代反演）到 PLATESOLVE.md §11.3 与 STANDARDS_REGISTRY，并把 docs/archive/HANDOVER.md:63 的 owner 裁决（WCS-002 F1 选 B）提回活动登记面。

**置信度**：**高**（本机 9/9 ctest 通过 + 网格对照实验）。

**反方论证**：反驳可能是「7x7 是 SCI 显式冻结、注册表已登记 DISP-WCS-006 说"与 SCI 冻结一致"，改动会破坏 registry 的 CONFORMANT 判定」。回应：E7 的注册表行自述「维护歧义」且严重度"低"；而**实码从来没有 7x7**（41/81 两处 + 三处旧注释），继续保留 7x7 只会让 registry 的 CONFORMANT 判定永久失真。另可反驳：「批注说 order 6..15 收敛率 0.73，说明提高阶比加密网格更有效」——这正是本推荐把判据重心从"网格"移到"阶 + 迭代"的理由。

**最小改动路径**：docs/science/ASTROMETRY.md（5 处）+ docs/algorithms/PLATESOLVE.md（7 处）+ docs/standards/STANDARDS_REGISTRY.md（2 处）+ ipv_wcs.cpp（3 处注释）+ ipv_solver.cpp（1 处注释）。**影响面**：SCI/ALG 文本、registry、traceability 的 ALG 锚、p1wcs 测试注释；**不影响数值**（实现不变）。

### 5.6 F-2（科学门/容差缺依据）

**唯一推荐结论**：在 SCI 侧新增一张**机器可校验的冻结门表**（建议落 `docs/science/GATES_AND_TOLERANCES.md`，或作为 SCI-PSF/STAR/WCS 的附录节），每行至少 8 列：`门ID | 判据式 | 量测域 | 统计量 | SNR/信噪定义 | 阈值 | 阈值来源(标定/解析/负责人裁决) | 证据ID`；并规定 (a) 任何检查器/门不得引用表外阈值；(b) 表内每行必须有证据 ID 或显式 UNJUSTIFIED 标记（后者不得用于发布门）。本域必须立即补齐的行：
- G-P1-CENTROID-SCI：补 SNR 定义（建议 SNR := A_fit/sigma_bg）与统计量（median/p95）。
- G-P1-CENTROID-U16：注明"仅量化面"。
- G-P1-WCS-F1：补量测域（内点 trans 域、合成线性场、n_pairs>=12）。
- gate2 的 fwhm/ell/flux/photutils 四阈值：要么在 SCI/ALG 立据并注册 ci/checks.json，要么把 gate2_psf_oracle.py 明标为**诊断脚本**并删除其门名。

**置信度**：**高**（普查表 §4.7 逐格可复现）。

**反方论证**：反驳可能是「补表就是把工程负担转移给文档」。回应：ENGINEERING_SPEC.md:117-120 已经要求"每项检查有正例与负例"，而当前 7 条门里有 4 条没有可执行检查、2 条没有量测域 ⇒ 表的成本小于门失效的成本；且表本身可被 `tools/science_contract_lint.py` 机器校验。

**最小改动路径**：新增 1 份 SCI 附录 + 各文档 1 行回指 + lint 规则 1 条。**影响面**：docs/ci/、ci/checks.json、tools/science_contract_lint.py、全部带容差的测试注释。

### 5.7 需负责人确认的那一句话

> 「同意把『0.5px 质心门』定性为 **F4 FP32 量化通道容差（可达、未超标）**，把 D-16 的整改对象改为 **端到端坐标契约**（PSF 支路写端不再 -0.5，并新增链接 sdet+dpsf 的绝对位置门）；F1 只补量测域、不改门值。」

其余五条（M3b-A-01 双模型、M3b-F-02 落 scipy Oracle、M1a-A-001 订正 SCI 公式、M1a-C-001 取消 NB_GRID=7 冻结地位、F-2 补冻结门表）都**不需要新的科学判断**，可直接转为整改任务；其中涉及 FROZEN SCI 文本的三条（M3b-A-01 范围澄清、M1a-A-001 订正、M1a-C-001 改写不变量）按文档集变更流程并批走一次变更 claim。

---

## 6. 证据清单（全部命令、退出码、产物路径）

### 6.1 环境与构建

| # | 命令 | 退出码 | 产物 |
|---|---|---|---|
| E-01 | `python3 -c "import numpy,scipy,astropy;print(...)"` -> `2.2.4 1.15.3 7.0.1`；`python3 -V` -> `Python 3.13.5`；`nproc` -> 16 | 0 | — |
| E-02 | `cd build && ninja -n p1psf_tests p1star_tests p1psf_prodpath_check_test` -> `ninja: no work to do.`（测试二进制与源码同步） | 0 | — |
| E-03 | `g++ -O2 -std=c++17 -I lib/algorithms/psf/include -I lib/algorithms/star_detection/include run/.../probe_convention.cpp build/libastrocs_p1_dpsf.a build/libastrocs_p1_sdet.a -lgsl -lgslcblas -fopenmp -lm -o run/.../probe_convention` | 0 | `run/PROJECT-GOVERNANCE-01/R-3/probe/probe_convention` |

### 6.2 仓内可执行门本机复跑

| # | 命令 | 退出码 | 逐字结果 | 日志 |
|---|---|---|---|---|
| E-04 | `ctest --test-dir build -R "p1psf_\|p1star_" --output-on-failure` | 0 | `100% tests passed, 0 tests failed out of 16` | `run/PROJECT-GOVERNANCE-01/R-3/logs/01_ctest_p1psf_p1star.txt` |
| E-05 | `build/tests/unit/p1psf/p1psf_tests oracle` / `p1psf_prodpath_check_test` / `p1star/p1star_tests oracle\|units` | 0 | `[f1] truth=40 snr20=32/32 snr10=32/32 count=32 fp=0 (0.000/kpx) mean_dc=0.0059 max_dc=0.0535`；`B4-5 PRODPATH CENTROID PASS`；`[f4] oracle found=20/20 max_dc=0.00e+00` | `…/logs/02_oracle_detail.txt` |
| E-06 | `ctest --test-dir build -R "p1wcs_" --output-on-failure` | 0 | `100% tests passed, 0 tests failed out of 9` | `…/logs/03_ctest_p1wcs.txt` |
| E-07 | `build/tests/unit/p1wcs/p1wcs_tests apbp` | 0 | `[WCS-003] low/mid/high: c90_max=3.299e-10 / 1.518e-09 / 1.655e-09 px`；`P1WCS APBP PASS` | `…/logs/04_p1wcs_apbp.txt` |

### 6.3 自建实验（真实源码探针 + 独立复算）

| # | 命令 | 退出码 | 产物 |
|---|---|---|---|
| E-08 | `run/PROJECT-GOVERNANCE-01/R-3/probe/probe_convention 20 60 42` | 0 | `…/logs/05_convention_probe_snr20.txt` |
| E-09 | `for snr in 20 50 100 300 1000; do probe_convention $snr 40 7; done` | 0 | `…/logs/06_convention_sweep.txt`（三档 SNR 的 sdet/dpsf 系判定） |
| E-10 | `for snr in 20 50 100 300; do probe_convention $snr 40 7; done`（含 Part D 编排链） | 0 | `…/logs/07_convention_chain.txt`（PSF 支路 -0.5000 px / fallback ±0.0002 px / 分离 0.5000 px） |
| E-11 | `python3 run/PROJECT-GOVERNANCE-01/R-3/exp/exp_r3.py` | 0 | `…/logs/08_exp_r3.txt` + `…/logs/exp_r3_summary.json`（Part 1–7） |
| E-12 | 源码探针：`run/PROJECT-GOVERNANCE-01/R-3/probe/probe_convention.cpp`（只读引用本树静态库，位于 run/，不入库） | — | 同上 |
| E-13 | 实验脚本：`run/PROJECT-GOVERNANCE-01/R-3/exp/exp_r3.py` | — | 同上 |

### 6.4 只读定位命令（本报告引用的 文件:行 均由这些命令复核）

```bash
grep -n "NB_GRID\|7×7\|AP_FIT_ORDER\|APX_ORDER" docs/science/ASTROMETRY.md docs/algorithms/PLATESOLVE.md
grep -n "NB_GRID\|AP_FIT_ORDER\|APX_ORDER" lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp lib/algorithms/platesolve/cpp/ipv/src/ipv_solver.cpp
grep -n "sdet_gaussian_f\|sdet_gaussian_df\|sdet_moffat4_fit\|sdet_lm_fit\|TWO_SQRT_2_LOG2" lib/algorithms/star_detection/src/sdet_api.cpp
grep -n "MOFFAT4_FWHM_FACTOR\|0\.5" lib/algorithms/psf/src/dpsf_psf.cpp
grep -n "astro_coord_to_unified\|astro_coord_from_unified" lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp
grep -rn "test_p1_batchH_star_coord" --include=CMakeLists.txt .      # -> 0 命中
grep -c "gate2" ci/checks.json                                     # -> 0
grep -rn "0\.897\|0\.151" lib/algorithms/platesolve/memory.md       # -> :55-56
grep -rn "DISP-WCS-00[6-9]" docs/ --include=*.md                   # -> 仅 006（+ REGISTRY）
python3 -c "import json;d=json.load(open('docs/traceability/TRACEABILITY_MATRIX.json'));..."  # PSF 行字段
```

### 6.5 未做/不可做的取证（如实登记）

1. `lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py` 是 **Windows 专用**（os.add_dll_directory + `ROOT = r"F:\Astro dev\..."`），本机不可运行 ⇒ 其 0.3px/FWHM/ell/flux/photutils 门值未复跑，只做**注册面与权威面**审计（§3.5）。
2. 真实帧（BASS/Galaxy_Center 等）未在本轮重跑端到端 plate solve（需 GaiaDR3SP 数据与完整 DLL 链）⇒ 0.897px 只作**台账值**引用，未在本机复现；本报告不据此下任何精度结论。
3. git 历史未读（本任务零 git 写；为避免误触也未做 git log）⇒ §3.2 的"R8-A 修复是否曾经正确"不作判断，只断言**当前树**的行为。
4. **零仓库写入核验**：本会话的全部写入均为新建文件，且仅落在 `run/PROJECT-GOVERNANCE-01/R-3/**`（gitignore）与 `reports/PROJECT-GOVERNANCE-01/research/`（新增未跟踪目录）。证据：`git --no-optional-locks status --porcelain` 中与之相关的条目只有 `?? reports/PROJECT-GOVERNANCE-01/research/`（日志 `…/logs/09_git_status.txt`）；其余 104 个 ` M ` 条目是**其它并发会话**在 `docs/`、`tools/`、`reports/PROJECT-GOVERNANCE-01/root-scan/` 等处的既有改动（mtimes 19:41–19:58，与本会话写点无交集），不是本报告的产物。

---

## 附：本报告对旧判据的三处「直接判定」（硬约束 4）

| 旧判据 | 判定 | 依据 |
|---|---|---|
| D-16「0.5px 门 vs 0.897px 实测，二选一」 | **前提失效**：两者不同门、不同域、不同工具代 | §1.1 A3/A4/A6、§2.6、§2.9 |
| `ipv_wcs.cpp:408-411`「冻结 1e-4 px 数学不可达」 | **对当前实现为假** | §3.6 实测 3.3e-10~2.9e-9 px，ctest 9/9 通过 |
| `test_p1_batchH_star_coord`「已修复 0.5px 系统错位」 | **未成立**：其前提（dpsf 与 det_x 同系）实测为假，且该测试未入门禁 | §2.9、§3.2、§3.5 |
