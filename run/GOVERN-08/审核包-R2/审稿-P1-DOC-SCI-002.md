# 审稿-P1-DOC-SCI-002 —— G08-05 对抗审稿 第 1 遍 · 片 DOC-SCI-002

> 仓库 `/workspace/Astro CS Database`，HEAD = `f9650dd0aed97d7f261e5e6547f4fdb505bc313b`（已 `git rev-parse` 核对，`git status --porcelain` 空）。
> 片清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:933-968`。
> 本人未做任何 git 写、未编译、未跑 ctest/pytest/实验脚本、未读 `/tmp/acsd_g08/`、未修改任何仓内文件（本交付件除外）。

---

## 1 读完了吗

| 口径 | 值 |
|---|---|
| 成员份数（权威版清单） | **28** |
| 实际读完份数 | **28** |
| 成员总行数（清单声明） | **9 979** |
| 实际行数（`wc -l` 逐份实测并与清单对账） | **9 979**（完全吻合，逐份无差） |
| 实际读完行数 | **9 979 / 9 979** |
| **覆盖率** | **100 %（份数 28/28，行数 9979/9979）** |

**未读完的部分：无。** 28 份成员文件全部由本人以 `read` 工具逐字读完（含 `PHASE3_PROJ_IMPL.md` 分 1–707 与 708–740 两段、`PHASE2_SAMPLER.md` 分 1–671 与 672–675 两段读完，无跳读、无片段、无脚本替代阅读 —— 规范 04 §1 合规）。

取证辅助（不替代阅读）：`git grep` / `wc -l` / `sed -n` 定点核源码；5 个子代理并行独立复核（见 §7）。

---

## 2 本片判定

**判定：阻断（BLOCK）。**（阻断 **8** 条，须修 **30** 条，建议 6 条；判定分布：阻断 7 份 / 须修 20 份 / 通过 1 份，共 28 份）

理由：本片是 `docs/science/` 的**科学正本层 + 算法推导层**，承载 P1/P2/P3/P5 五个创新点的判据口径。存在 **8 条阻断级缺陷**，其中 3 条是「恒真门」——即本项目历史上反复买过教训的那一类，且本片自己已经写出了正确的反例却没把它们落实为门；另有 2 条是**承重伪引**（伪造了上游权威条款），1 条是**原子发布序与实现完全相反**（决定用户路径上是否可能出现「已发布但无效」的对象）。

> ⚠ **本节判定在复核后上调过一次**：初稿把 `PHASE3_PROJ_IMPL.md` 判为「通过（本片质量标杆）」，第 5 个子代理回报后经本人复验，发现它对 `ACSD_DESIGN.md §6.3` 存在**承重伪引**，已降级为**阻断**（B-07）；同轮另新增 B-08（`PHASE3_FITS_IMPL.md` 发布序反向）。详见 §3 表脚注与 §4.1。

### 最重的 3 条

**① 阻断 · 恒真门（代数恒等式型）：`DISPUTE_RESOLUTION.md:162-170`（§13）用恒等式承载 γ=2 终裁，且被本文件自己禁止作证据。**
判据形如「比较 `SNR²/F_ref²` 与 `1/σ_F²`」——两者是同一个量的两种写法，比对结果恒为 0，永远绿。更严重的是**同一份证据文件在同文 §27 已被自己标为不可承载证据**：`DISPUTE_RESOLUTION.md:300-303` 逐字记载 `is_tautology: true` / `evidence_eligible: false`，并写「故恒等式本身不承载证据」；§13 却用它承载一条**冻结科学常数**（`w_UPM` 的指数 γ=2）的终裁，状态是「无落点（待同步）」——按 §346 汇总表落笔，就会把恒真检查写进科学正本。且 §13:168「`w` 对 `m_ref` 不变逐位」被 §27:293-297 的实测漂移 **3.83%** 直接推翻，两条裁决互斥且从未被裁定。
证据：`docs/science/DISPUTE_RESOLUTION.md:162-170` vs `:300-303`、`:293-297`。

**② 阻断 · 恒真门（同源查询型）：`PHASE2_REJECTION.md:636-639`（§11.4 F2）用同源 `constexpr` 查询锁定 AUTO 路由 ⇒ 生产档真实档界 6/15/16 零门覆盖。**
文档写「生产档 `1≤N≤3→none`、`4≤N≤5→percentile` 的边界由 `p2_rejection_percentile_band_min_n` 查询值锁定，该值**必须与路由表同源**（改一处必须同时改另一处）」。但该查询函数返回的是 `constexpr kPixelSmallNPolicy` 的重述，而 `acsd_n_map_method` 与 `auto_method_forbidden` 判的是**同一个 `constexpr`**——**设计上恒等**。可证后果：把路由表的 `n<6u` 改成 `n<7u`，查询值仍为 4，门照样全绿。文档把这个同源关系写成「锁定机制」，等于把恒真门**制度化**。反向反证门其实已存在（`synthetic_gate.cpp` 的 `EXPECT_EQ(resolve(3), P2_REJECT_NONE)`），但**未被列进 §11.4 F2**。
证据：`docs/science/algorithms/PHASE2_REJECTION.md:636-639`；`lib/algorithms/coverage/src/rejection.cpp:1144-1148`（查询 = `constexpr` 投影）、`:1154`、`:1188`（同一常量）。

**③ 阻断 · SCI 正本级判据口径与代码相反 + 正本内部三处互斥：`REJECTION.md`。**
(a) `:52`/`:146`「非有限 `weights/support` ⇒ `INVALID_INPUT` **hard fail**」——`support` 根本**不在** kernel 输入结构体 `P2CandidateStack`（`rejection.h:379-391`）里，代码不可能为它返回任何状态；非有限 `weights` 在生产路径被 `p2_collect_candidate_stack` **静默剔除**（`rejection.cpp:1414-1428`），永不产生 `INVALID_INPUT`。**hard fail 与静默丢弃是两种相反的生产语义**；算法正本 `PHASE2_REJECTION.md:232`/`:678` 写的恰是相反的正确口径，说明 SCI 未被同步。(b) `:58`/`:213` 称「7 种方法」而 `:64` 立刻路由给 percentile、`:230` 又称「11 项 + AUTO」——**直接后果是 `:213` §10 禁改面只保护 7 个方法，percentile 0.2/0.1、median_sigma、minmax、extreme_prior 四个已冻结阈值不受 SCI 禁改清单保护**，而 `PHASE2_REJECTION.md:740-741` 却宣称 SCI §10「零改动」。(c) `:115`/`:134`/`:143` 的「`n ≤2` **恒** UNDERDETERMINED」被 `:46`（生产档 3）、`:221` 与代码 `rejection.cpp:2089` 推翻——§7 用「恒」字断言了一个**在生产档为假**的独立不变量。(d) `:331-332` 以「§8a 实测」引 `79.1%` 与 `≥99.4%`，而 §8a 全文**无任何百分比**，且三次自陈读数在实验 results；`99.4%` 的另一处命中是 `PHASE2_UPM.md:494` 的**接缝门虚警率**（异题），形态上疑似串值。

---

## 3 逐文件清单

28 份成员文件全部读过。判定栏口径：**阻断** / **须修** / **通过**。

| # | 文件 | 行数 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|---|
| 1 | `algorithms/PHASE2_REJECTION.md` | 833 | §1–§17 全节 + 头注 + 对照档过拒率证据节 | 11 方法状态机、AUTO 四档路由、阈值冻结表、§11.4 F1–F8 测试设计 | **阻断**（② + 锚全面失配 + 缺陷登记含已修项） |
| 2 | `REJECTION.md` | 362 | §1–§17 全节（含 §8a/§3a/§9a 插节） | SCI-REJ-001..008 正本、percentile 三段适用域、沿线排异四分母口径 | **阻断**（③） |
| 3 | `DISPUTE_RESOLUTION.md` | 351 | §1–§30 + 待同步汇总表 | 30 条裁决的终裁/依据/正本落点三列台账 | **阻断**（① + 4 处算术/数值自相矛盾） |
| 4 | `algorithms/PHASE2_COVERAGE.md` | 402 | §0–§11.5 全节 | coverage union 整数语义、三概念分离红线、两阶段协议 | **须修** |
| 5 | `algorithms/PHASE3_PROJ_IMPL.md` | 740 | §0–§16 + 尾节全节 | 八投影 registry 三层模型 D=I={TAN}、四角域守卫凸性证明 | **阻断**（**降级**，见 §4.1 B-07：对 `ACSD_DESIGN.md §6.3` 的承重伪引） |
| 6 | `PHASE2_SAMPLER.md` | 675 | §1–§13 全节 + 参考文献 | 三阶段采样管线、control_variance 三口径、k_corr 两因子查表 | **须修** |
| 7 | `CALIBRATION_ALGORITHMS.md` | 640 | §1–§11 全节 | 母版生成、单帧校准、标度声明表、U1–U4 机器规则 | **须修** |
| 8 | `IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md` | 610 | 全文 §一/§二[S1]–[S21]/§三/§四 | 21 条带 URL+逐字原文的外部标准取证 | **通过**（片内最严证据纪律） |
| 9 | `algorithms/PHASE2_UPM_IMPL.md` | 543 | §1–§17 全节 + 收敛尾节 | IRLS 求解、三档确定性合同、DISP-P2UPM-001..007 | **须修** |
| 10 | `algorithms/COSMETIC_ALGORITHMS.md` | 519 | §0–§11 全节 | 坏点检测/修复/坏列族、12 条 DISP | **须修** |
| 11 | `algorithms/DRIZZLE_GEOMETRY.md` | 462 | §0–§11 全节 | ALG-DRZ-001 全链、δ 残差标度律、DISP-DRZ-001..009 | **须修** |
| 12 | `algorithms/PHASE3_FITS_IMPL.md` | 449 | §1–§17 + §U | 原子写协议、CHECKSUM 顺序勘误 | **阻断**（B-08 发布序反向，见 §4.1） |
| 13 | `algorithms/HIPS_WRITER.md` | 422 | §0–§11 全节 | HiPS 产品集写出、hierarchy 聚合、DISP-HIPS-001..012 | **须修**（S-29 治理类硬错，处置建议具破坏性） |
| 14 | `algorithms/PHASE2_SESSION.md` | 405 | §1–§13 全节 | 四段 DAG、NODE-CALL 矩阵、DISP-P2SES-001..008 | **须修** |
| 15 | `algorithms/PHOTOMETRIC_FIT.md` | 306 | §1–§13 全节 | Tukey IRLS 零点估计、F_syn 定义、DISP-PHOT-001..009 | **阻断**（子代理 A1：复用被测实现的「实证判据」） |
| 16 | `algorithms/NOISE_ESTIMATION.md` | 294 | §1–§13 全节 | 8×8 patch 平面场、DISP-NOISE-001..010 | **须修** |
| 17 | `algorithms/GAIA_QUERY.md` | 288 | §1–§6 全节 | XPSD 锥查询、极冠双 Lipschitz 剪枝 | **须修**（fail-closed 边界写反） |
| 18 | `algorithms/STAR_PSF_ALGORITHMS.md` | 265 | §1–§11.5 全节 | 三种互不兼容 9 列布局消歧 | **须修** |
| 19 | `DRIZZLE.md` | 255 | §1–§15 全节 | SCI-DRZ 正本、核/分母/方差公式 | **阻断**（方差分母：正本禁令 vs 产品实现） |
| 20 | `IO_001_FITS_STREAM_INTERFACE.md` | 212 | §1–§14 全节 | FITS 流 C ABI 合同、14 个错误码 | **须修** |
| 21 | `PHOTOMETRY_RESEARCH_PACK.md` | 205 | §1–§10 全节 | Gaia XP/通带/零点取证 | **须修** |
| 22 | `ACR_EQUIVALENCE.md` | 177 | §1–§14 全节 | ACR 工作域等价、绝对容差三档不具证据资格的论证 | **须修** |
| 23 | `STAR_DETECTION.md` | 168 | §1–§6 全节 | 99% 召回阈表、Clopper-Pearson 样本量论证 | **须修** |
| 24 | `UNIFIED_SCIENCE_MODEL.md` | 137 | §1–§12 全节 | 五层目标、Q/W 最优统计、单一权重口径 | **须修** |
| 25 | `algorithms/UPM_SOLVER.md` | 132 | §1–§12 全节 | ALG-UPM 推导层 F1–F6 | **须修**（本片最脏一份） |
| 26 | `SCIENCE_SCOPE.md` | 121 | 全文 | 标度类别封闭词表、γ 失效判据 | **须修**（判据面自污染） |
| 27 | `README.md` | 3 | 全文 | 一句话目录说明 | **须修（轻）** |
| 28 | `algorithms/README.md` | 3 | 全文 | 一句话分层规则 | **须修（轻）** |

---

## 4 发现清单

### 4.1 阻断（6 条）

| ID | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| **B-01** | `DISPUTE_RESOLUTION.md:162-170` | 用代数恒等式论证 γ=2「唯一确定、不需标定证据」（恒真门·代数恒等式型）；且与同文 `:293-297`（`w` 随 `m_ref` 漂移 3.83%）互斥 | γ=2 的唯一性改由 GLS 最优性推导承载；先裁定 §13 与 §27 谁作数，再落笔 | `DISPUTE_RESOLUTION.md:300-303` 自标 `is_tautology:true / evidence_eligible:false` 且明写「恒等式本身不承载证据」；§13 落点为「无落点（待同步）」 |
| **B-02** | `PHASE2_REJECTION.md:636-639` | 用同源 `constexpr` 查询值「锁定」AUTO 路由档界（恒真门·同源查询型）；改路由表仍全绿 | 改为断言硬编码期望枚举（3→NONE、4/5→PERCENTILE、6/15/16→WINSORIZED），查询值只作辅助提示 | `rejection.cpp:1144-1148` 返回 `constexpr kPixelSmallNPolicy`（`:1141-1142`）；`:1154`、`:1188` 判同一常量。反证门已存在：`synthetic_gate.cpp` `EXPECT_EQ(resolve(3), P2_REJECT_NONE)` / `resolve(5)==PERCENTILE`，**未列入 §11.4 F2** |
| **B-03** | `REJECTION.md:52`、`:146`（并波及 `PHASE2_REJECTION.md:799`） | 「非有限 `weights/support` ⇒ `INVALID_INPUT` hard fail」与代码相反：`support` 不在 kernel 输入面；非有限 `weights` 被**静默剔除** | 删 `support`；`weights` 限定为 kernel 输入面并注明生产路径已静默剔除 | `rejection.h:379-391` `P2CandidateStack` 无 support 字段；`rejection.cpp:2047-2049` 只查 values/weights；`:1414-1428` gather 层静默剔除；`PHASE2_REJECTION.md:232`/`:678` 写的正是相反的正确口径 |
| **B-04** | `REJECTION.md:58`、`:213` vs `:64`、`:230` | 「7 种方法」与「11 项 + AUTO」并存 ⇒ `:213` §10 禁改面**漏保护 4 个已冻结阈值**（percentile 0.2/0.1、median_sigma、minmax、extreme_prior α） | 方法集三处统一为 11(+AUTO)，§10 禁改面补齐全部 11 个方法 | `rejection.h:45-63` 12 个枚举值（非 AUTO 者 11）；`:105-111` 阈值表实含 8 档；`PHASE2_REJECTION.md:740-741` 宣称 SCI §10「零改动」 |
| **B-05** | `PHASE3_PROJ_IMPL.md` 之外的 `DRIZZLE.md:91`/`:218` vs 产品面 | 正本规定 `variance_p = sumVarNum / N_p²` 并**明令禁止**裸写 `sumVarNum/D_p²`（pf=0.8 时偏 2.44×），而产品面发布的正是 `var = vnum/(area*area)`（`area = sumArea = D_p`）；`DRIZZLE_GEOMETRY.md:106-107`/`:403` 把它写成「语义不变」，`DISP-DRZ-001..009` **无一条**登记该偏差 | 二选一并走变更：实现补 `k²`，或正本改写；无论哪条须登记为 DISP | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` finalize 段 `var = vnum/(area*area)`；`astro_sphere_sink.cpp:191,205` `view.covered_area = dense_area = acc.sumArea`；`:527-542` signal 面走 `k=sumArea/sumNorm` —— **同一 tile 两个面用了两个不同分母** |
| **B-06** | `PHOTOMETRIC_FIT.md:202` | 把「`zero_point_mag` 必须由公式**逐位复现**」称作**生产路径的实证判据**，但复算腿与被测腿是**同一份生产实现**（直接链接 `spectrum_integrator.cpp` + `gaia_client.c`），零实现独立性（恒真门·往返自证型+复用被测） | 补真正独立的 oracle 腿（NumPy 独立积分，不链接生产符号），或显式降级为「算术一致性脚手架·无科学判别力」 | 自报证据正本 `实验/photometric-magnitude/RESOLUTION_m42_curve_resolve.md` 探针章节标题即「用生产代码本体复算」；同片 `CALIBRATION_ALGORITHMS.md:453-457` §9.1 明写 oracle「不复用实现代码」。判别力反证：探针四行 `zero_point_mag` 漂移 **0.86 mag**，而门只要求 1e-12 |
| **B-07** | `PHASE3_PROJ_IMPL.md:154`、`:450-453` | 对 `docs/ACSD_DESIGN.md §6.3` 的**承重伪引**：`:154` 引「`ACSD_DESIGN.md §6.3:493`「（当前仅 TAN 可用）」」并据此**撤销**一条他域上报的冲突；`:450-453` 以「**原文**：」名义重引 §6.3 并塞入该节**根本没有**的 `CRPIX/CRVAL/CD/PC/CDELT/CTYPE` 清单 | 按 §6.3 真实原文重引；`85°`/`20°`/`1e-8` 三值的出处改指 SCI-P3 §9a-12/§7 | **本人实测**：`grep -c "仅 TAN" docs/ACSD_DESIGN.md` = **0**；§6.3 实为 `:383-386`（原文「内置多种投影，首批为 TAN、SIN、CAR、AIT、STG、MOL、CEA、ZEA；每种声明适用域、奇点、经度 wrap 与手性；未实现的投影被选择时显式报不支持。」「新增投影经投影注册表注册并附独立往返测试；缺省 TAN。」）；`:493` 实为目录树代码块行「│   ├── benchmark/         kernel 测量与机器画像」；`sed -n '383,387p' \| grep -c "CDELT\|CTYPE"` = **0**。**伪引已污染全链**：根因在 `lib/algorithms/projection/p3_projection_registry.h:5-9`（自称「DESIGN §6.3 的唯一产品声明权威」），`eng/contracts/schemas/projection_registry.schema.json:5` 再继承一次 ⇒ 八投影冻结链的**上游依据是空的** |
| **B-08** | `PHASE3_FITS_IMPL.md:99-109`（§3 步骤 11/12）、`:147-154`（§4）、`:198-199`（§6 F1）、`:316-318`（§12 T6） | 原子发布序**与实现完全相反**：文档冻结「flush→close→fsync→**rename**→sha256(path)→verify(path)」并写「发布后 sha256 失败 → **unlink(产物)**」；实现是「flush→close→fsync(tmp)→**pre-publish verify(tmp)**→**sha256(tmp)**→rename」，**rename 之后不再有任何判据** | 按实现改写文档；并明确「rename 之后无判据」这一契约含义 | **本人实测**：`p3_output.cpp:446` 注释「私有临时区 → 关闭/fsync → 校验（结构 + DATASUM/CHECKSUM）→ 算哈希 → 原子改名」、`:447`「P-205 (台账 A-4): 原序是 flush → close → fsync → **rename → sha256/校验**」、`:179`「IO_003 §4 的『fitsverify』步骤，**位于 rename 之前**」。⇒ 文档描述的是**实现已刻意替换掉的旧语义**（原序已被 P-205 修正）。叠加 §3:119 的 `reopen_ok` 冻结式漏 `bunitok`/`chksumok`（代码 `:736` 实为 6 项合取），以及 §3:111「`(void)wcs` 不作为读取源」与代码 `:628-632` 用 descriptor 作对拍基准相反 ⇒ 三处叠加会让验收按文档写出错误的重跑/回滚预期 |

### 4.2 须修（30 条，取最重 18 条列全；S-29/S-30 为复核后新增）

| ID | 位置 | 现状 → 应为 | 证据 |
|---|---|---|---|
| S-01 | `docs/detail/algorithms_phase2/` **整目录**被删，仍被 27 份存活文档/源码引为「唯一正本」 | 目录不存在于活仓；69 处引用横跨 science/engineering/两库生产 `.cpp`/合同 schema | 提交 `ac058121`（G08-03 一模块三页族合并）删除全部 6 文件；`git -c core.quotepath=false grep -rln "docs/detail/algorithms_phase2/" -- docs/ eng/ lib/ 实验/` → **27 份**；本片 4 处：`REJECTION.md:340`、`PHASE2_COVERAGE.md:398`、`PHASE2_SAMPLER.md:672`、`UNIFIED_SCIENCE_MODEL.md:54`。删除目的正是归一为「一份正本」，但**替换件仍指向被删件** |
| S-02 | `PHASE2_REJECTION.md:5/:59/:612/:725`（2965）、`:503`（2950）、`:7/:59/:503/:612/:726`（h 602） | 同一文件内 rejection.cpp 有 **2965 / 2950 / 实测 2959 三个值**；rejection.h 称 602 实为 603。§3:59 明写「行号一律照录实测值」 | **本人实测**：`wc -l lib/algorithms/coverage/src/rejection.cpp` = **2959**；`rejection.h` = **603**。更重的是治理后果：DISP-P2REJ-003 的存在意义就是治锚漂移，而它登记的「实测值 2950」是第三个错值 |
| S-03 | `PHASE2_REJECTION.md:87-136`（§3 锚表） | 29 个符号锚实测全部失配（偏差 1–127 行）；`:132`/`:133` 把 `:394-401` 同时给两个符号；`:40-45` 符号表锚 `h:369/370/371/372` 实为**另一个结构体** `P2EligibilityGatherOutput` 的诊断计数 | `p2_reject_plan_resolve` 实 `:1195`（doc `:1182`）；`p2_reject_stack_ex` 实 `:2018`（doc `:2022`）；`p2_reject_classify` 实 `:2668`（doc `:2534`）；`P2RejectReason` 实 `:98`（doc `:94`） |
| S-04 | `PHASE2_REJECTION.md:605-607`（DISP-P2REJ-001） | 登记了一条 HEAD 上**已不存在**的缺陷（称 `rejection.h:139` 注释为「默认 0.1」） | `rejection.h:143` 实为 `// 相对 median 低侧小数（默认 0.2 = 20%）`，与实现/SCI 一致。缺陷登记表本身成了新的漂移源 |
| S-05 | `PHASE2_REJECTION.md:631-639`（「门面缺口」段） | 声称生产档 `1≤N≤3→none`/`4≤N≤5→percentile` 无直测 —— **内容不成立**（反证门已存在）；且其提出的锁定机制即 B-02 的恒真门 | 应删段，改登记既有门名 `synthetic_gate.cpp` 的 `M3PixelProfileRoutesGe16ToWinsorized` 等 |
| S-06 | `DISPUTE_RESOLUTION.md:63-64` | 同一行给出两个值：`N_sky ≥ 9216 = (1.44/0.015)²` 与 `(1.152×1.25/0.015)² = 5816.6`。**两式恒等**（1.152×1.25=1.44），实为 `(96)² = 9216` | 算术自证：96² = 9216 ≠ 5816.6（差 58 %） |
| S-07 | `DISPUTE_RESOLUTION.md:88` vs `:91` | 终裁「端到端 ≤ ±1.5%」被自身依据「端到端臂 **1.096**」推翻（=+9.6 %）；另一依据 `0.913`（−8.7 %）在终裁三口径中无对应 | 终裁须与依据同值，或标明 1.096 属哪一臂 |
| S-08 | `DISPUTE_RESOLUTION.md:154-157` | 终裁「1.49×」vs 依据「7.4% / 8.03%」——量级差一个数量级；文件登记的「数字冲突（7.4 vs 8.03）」**不是实际存在的那个冲突**（1.49× vs 7.4%） | 真正的冲突未被登记，会被下一棒当已核项落笔 |
| S-09 | `DISPUTE_RESOLUTION.md:306`（§27） | 「正本落点」列**缺状态标签**，违反本文件 `:23` 自定 schema（其余 29 条全有）；`:346` 汇总表又把 §27 列为待同步 ⇒ 条目与汇总表互相矛盾 | 逐条比对 §1–§30，§27 是唯一缺状态的一条 |
| S-10 | `DISPUTE_RESOLUTION.md:316-323`（§29） | 仍登记 `frame_photometry_fit.cpp:292` 用 4 位 `1.4826` 的代码缺陷 | **本人实测**该行现为 `1.482602218505602 * zmad`（全 15 位）。缺陷已修、登记未撤 |
| S-11 | `REJECTION.md:239`、`:16`、`:24` | 公开 API 列 `p2_reject`、符号表列 `P2RejectionInput` / `P2RejectionLargeScaleConfig` —— **三个名称在现行树零命中** | compat 实际名为 `p2_reject_stack`（`rejection.h`），生产为 `p2_reject_stack_ex`。且 `P2RejectionLargeScaleConfig` 这一错误引文已被**机器提取进** `eng/packaging/config/defaults.json:638` 的 `"quote"` 字段 —— 修文档需连带修配置 |
| S-12 | `HIPS_WRITER.md:205`/`:255`（`hips_version="1.4"`）、`:239`（`HIPSTILEWIDTH=512`） | 产品写出 `hips_version="1.4"` 与 `HIPSTILEWIDTH=512`；而**同片的** `IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md`（带 URL + 逐字原文的取证件）结论是：HiPS 有效版本只有 **1.0（REC）/ 2.0（WD）**，无 1.4；`HIPSTILEWIDTH` 在 HiPS 1.0 REC、2.0 WD、FITS 4.0、Hipsgen/Aladin 手册、MOC 2.0、两块真实 CDS tile 头中**全部 0 命中**，并明写「建议不要把 `HIPSTILEWIDTH` 当作规范关键字使用」 | 片内自相矛盾：研究包已给出否定结论，写出层照写不误。`eng/tests/io/make_hips_fixture.py:160`、`hips_output_fixture.py:79` 同步固化了 `1.4`，修正面 > 单文件 |
| S-13 | `REJECTION.md:319`、`DRIZZLE.md:254`（另 ASTROMETRY/CALIBRATION/INTEGRATION/NOISE_MODEL/PHASE2_UPM 同款，本片 2 处） | Acceptance 以「`eng/tools/science_contract_lint.py` PASS」为发布门 | **本人实测**：该文件在 HEAD **不存在**。**跑不起来的验收项永远不能红** —— 它在 Acceptance 里占位而不产生任何约束 |
| S-14 | `UPM_SOLVER.md:73`（§5c 取消点） | 写「IRLS 迭代间检查；取消时不写 Model/persist」——**与代码直接矛盾** | `grep -c cancel lib/algorithms/coverage/src/upm.cpp` = **0**；同组 `PHASE2_UPM_IMPL.md:306-311` 已把这句话标为「旧表述勘误」并写明真正取消点只有 session 阶段边界，但本文件未同步 ⇒ **同组两文档对同一语义互斥** |
| S-15 | `UPM_SOLVER.md:117`（§12） | 把 `p2_upm_normalized_weights` 当活符号（`F1 → …/p2_upm_normalized_weights`） | `upm.cpp:1999` 明写「定义已删（全仓零消费者）」，`upm.h:189` 声明已撤。**退役治理**（注释自称「零消费者」此处为真，但文档当活符号引用） |
| S-16 | `UPM_SOLVER.md:54-55`（§4 表） | 「NaN weight → `INVALID_INPUT`」「无观测 → `NO_DATA`」 | 两名称在 `upm.cpp`/`upm.h` 中**均不存在**（两文件零命中）；与 `PHASE2_UPM_IMPL.md:341`（`n_obs=0 → rc=1`，实测 `upm.cpp:258`）冲突 |
| S-17 | `PHASE2_SESSION.md:128` | 常量面写 `upm_weight_source=0` —— **该字段名在代码中不存在** | **本人实测**：`git grep upm_weight_source -- lib/ eng/` → **0 命中**；代码赋的是 `uc.snr_weight_mode = 0`（`p2_session.cpp:199`，头 `upm.h:79`）。SCI 侧 `DATA_SEMANTICS.md:1972` 已改名「upm_weight_source（原 snr_weight_mode）」，本文件跟了改名而未核代码 |
| S-18 | `PHASE2_UPM_IMPL.md:541`（尾节）+ `:398`/`:461` | 「求解入口四参数……（缺省取 legacy 值，**生产装配显式启用**）」 | 对 **session 入口不实**：`p2_session.cpp:196-204` 未设 `gs_damping/m_full_frame/tolerance_relative/final_gauge` ⇒ 保持 legacy（0/0/0/1.0）；而 `module_adapters.cpp:9797/:9801-9803` 显式设 0.5/1/1/1。⇒ **两条生产入口解不同模型**（session 路 `final_gauge=0` ⇒ 无 G 场），且 DISP-P2UPM-006 只登记 2 项、锚全失效（真值 `:9776-9807`） |
| S-19 | `UPM_SOLVER.md:15` + `PHASE2_UPM_IMPL.md:162-163` | F1 写 `raw_w = quality_factor × control_ivar`，**缺 `control_reliability` 因子**，且单位写 `ADU⁻²` | SCI 正本 `docs/science/PHASE2_UPM.md:80` 逐字为 `w_UPM = quality_factor × control_reliability × control_ivar`；单位正本 `:33` 冻结 `(ADU·sr⁻¹)⁻²`。按两文档自订优先级「冲突时以 docs/science/ 为准」，此式不成立 |
| S-20 | `GAIA_QUERY.md:139-140`（§2.6） | 写「`count=0` 时查询路径**回退 343**」——把 fail-closed 边界写成了静默兜底 | `gaia_client.c:1479-1490` 注释明写「0/缺失同样视为损坏并**整文件拒绝**（修复前四种『==0 才兜底』会放行畸形文件）」。仓内 `FIX_LEDGER.csv:740`（V5-N-04）已列 OPEN 并明写「**改文档那一行，不要回退代码**」——文档未同步 |
| S-21 | `COSMETIC_ALGORITHMS.md:435-437`（§9 不变量②） | 写「**两条生产路径当前正是 dark/bias=NULL（§4）**」 | §4 说的恰是相反：「现网生产调用点（两处，**检测源均已接真实母版参考平面**）」（`:212-220`）；DISP-COS-009（`:481`）同；`CALIBRATION_ALGORITHMS.md:330` 写「**两处都不是 `nullptr, nullptr`**」。跨引用指错 + 事实相反，会误导 P1-COS-TEST 低估伴随断言必要性 |
| S-22 | `COSMETIC_ALGORITHMS.md:457`、DISP-COS-012 `:484` | 「调度器路径 `module_adapters.cpp:2328-2329` 对词表外取值静默回落 median」 | `:2328-2329` 实为 `p1_memory_cap` 的 `input_lights` 判空，与 method 无关；真实锚 `:2845-2846`。**缺陷登记的核心证据行锚指错函数** |
| S-23 | `NOISE_ESTIMATION.md:19`（F3 适用域） | 证据指「`lib/algorithms/coverage/src/upm.cpp` 中 SCI-VAR-ADAPT-01 段的自陈」 | `grep -in "VAR-ADAPT|SCI-NOISE" coverage/src/upm.cpp` → **0 命中**；该自陈实际在 `noise_model.cpp:205-212`。**伪引（证据腿指向不含该内容的文件）**，数字本身成立 |
| S-24 | `CALIBRATION_ALGORITHMS.md:27` vs `:299` | §1「负责」写「Gaia 测光比例标量应用（**现状未接线**）」；§3.6 写「**有生产调用方**」（Phase1 同一步内施加） | 同文件直接矛盾。`:299` 一侧为真 |
| S-25 | `CALIBRATION_ALGORITHMS.md:504-514` vs `:532-534` | **DISP-CAL-010 同一条 ID 登记了两件不同缺陷**（前者=`generate_master_flat` 的 NaN 策略，后者=`ac::` 层 `n_frames<=0` 双层校验不一致） | 同一 ID 两次不同内容，登记面不可索引 |
| S-26 | `CALIBRATION_ALGORITHMS.md:319` vs `:625` | 「18 个 `AC_API` 符号」vs 「`astro_calibration.h` **14 符号**」 | `grep -c "^AC_API" astro_calibration.h` = **18**。同文件两处数字打架 |
| S-27 | `DRIZZLE_GEOMETRY.md:313`（FIX-DRZ-A） | 写「每像素常量 ADU ⇒ `S=C/A_drop`」 | 正本 `DRIZZLE.md:129` 写 `S_p=(C/A_pixel)`。二者差 `1/pixfrac²`；**同仓两份文档对同一物理量给两个值**，且与本文件 `:93-94` 自述的 `κ = A_pixel/A_drop = 1/pixfrac²` 矛盾 |
| S-28 | `SCIENCE_SCOPE.md:50-53`（γ 失效判据） | 判据面 = **散粒主导的 patch 子集**，冻结阈值 `\|γ−1\| ≤ 0.3`；门在 `γ > 1.3` 时触发 | **判据面与门用同一个量 γ 定义 ⇒ 样本选择污染**：被选入「散粒主导」的 patch 必满足 `\|γ−1\|≤0.3`（即 γ≤1.3），故门在被选域内**结构上不可能触发**。这是恒真门第三型（判据面自污染），本片此前未登记 |
| S-29 | `HIPS_WRITER.md:384`（DISP-HIPS-012） | 称 FIRSTPIX/LASTPIX「**全仓无消费方**（仅 fixture 断言字面值）」，处置建议「保留并文档化或**删除**」——**前提为假，且处置具破坏性** | 改判为「有 1 个生产校验器 + 2 个测试消费者」，撤掉「删除」建议。**本人实测**：`lib/infrastructure/aio/io/hips_core.c:16`（头卡一致性校验声明）、`:664`、`:689-692`（`if (strcmp(nm,"FIRSTPIX")==0)` → `hips_set_err(…"tile FIRSTPIX=%lld 非 0")`，生产 fail-closed 校验器）；`lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate7_hips_validate.py:101`（`if h.get("FIRSTPIX")!=0 or h.get("LASTPIX")!=512*512-1`）。**危害**：§9:338-340 恰恰把 `gate7_hips_validate.py` 钦点为「独立复检生态参考…可作 **oracle 参照实现**」，而 §10 判它「零消费者」——**同一文件内两节互斥**；按 `:384` 的「删除」处置会直接打断 §9 钦定的 oracle（删卡后 `h.get("FIRSTPIX")` 返回 `None != 0` ⇒ oracle 必红） |
| S-30 | `HIPS_WRITER.md:208-209` | `dataproduct_subtype` 逐子产品写 `surface brightness`/`coverage fraction`/`variance`/`inverse variance`/`snr`，**全部越出封闭词表**，且 12 条 DISP 无登记 | 登记为 DISP 条目；或按同包研究包 `:606` 已给出的正确结论（四子产品在 HiPS 1.0 无对应标准机制）加注。HiPS 1.0 REC §4.4.1 属性表该字段为 `R`（必填）、2.0 WD 同为 `word "color","live"` |

### 4.3 建议（6 条）

| ID | 位置 | 内容 |
|---|---|---|
| A-01 | `docs/science/README.md:3`、`docs/science/algorithms/README.md:3` | 两份 README 各 3 行、零列举。`algorithms/README.md` 给的是**规则**（「算法层与正本不一致时以正本为准」）而非**内容说明**；`docs/science/algorithms/` 实有 24 份算法文档。AGENTS.md §5 要求「说明该文件夹的内容」——建议补 5–8 行分层分工说明（哪几份是合同/门表/实现锚定） |
| A-02 | `REJECTION.md:244`/`:248`、`DRIZZLE.md:207`/`:212`、`HIPS_WRITER.md:346` | 四处章节号物理顺序错乱（§3a/§9a 排在 §13 之后；§15–§17 排在 §14a 之后），违反文档自身编号契约 |
| A-03 | `REJECTION.md:306`、`PHASE2_SESSION.md:261`、`PHASE3_FITS_IMPL.md:418`、`UNIFIED_SCIENCE_MODEL.md:131`、`COSMETIC_ALGORITHMS.md:513` | 五处**文本损坏**：掉词（「判据与␣␣逐式等价」/「out/hull」）、重复锚（同一括号内两次 `TRACEABILITY_SPEC.md §9`）、编辑残片（`…不按错处理。，GLS 原始出处）；`SCIENCE_SCOPE.md:88` 还有 `\`gain\`/`\`read_noise_e\`` 转义反引号 |
| A-04 | `REJECTION.md:272-273`、`:299` | 链接文本写 `[arXiv.05276]` / `arXiv.07838`（截断）。应为 `arXiv:1807.05276` / `arXiv:2301.07838` —— 同行算法正本 `PHASE2_REJECTION.md:813` 写法正确 |
| A-05 | `PHOTOMETRY_RESEARCH_PACK.md:180`（§9 `[NONE]`） | `[NONE]` 条目写「仓内**无仪器 QE/光学曲线**」；但 `git grep KAF-16803` 在 `lib/algorithms/photometry/data/response_curves/qe_curves.json:4764-4765` **存在同名曲线**，只是 provenance 未登记（`filter_qe_provenance.json:639` 自陈 unverified）。正确表述是「曲线在仓内、来源未登记」——否则与同文件 `:90` 已用 KAF-16803 QE 报出的实测（+0.811 mag / 0.0082 mag）矛盾 |
| A-06 | `PHASE2_UPM_IMPL.md:452`/`:465`、`UPM_SOLVER.md:90`/`DISP-P2UPM-006` | 缺陷清单标题写「DISP-P2UPM-**001..004**」但表内含 001..007；`:465` 登记原则又写「（001/002/005/007）」。自相矛盾 |

**其余未逐条展开的须修项**：本片另有 12 条同质须修级发现，集中在 `PHASE2_SAMPLER.md` / `PHASE2_UPM_IMPL.md` / `UPM_SOLVER.md` / `PHASE3_PROJ_IMPL.md` 的**行锚漂移**与**跨文档口径不一致**（含两个被文档引用 4 次但**在活仓中不存在**的幽灵测试文件 `eng/tests/unit/p3_wcs_test.cpp` / `eng/tests/backend/p3_wcs_main.cpp`）。这些条目同属一类（锚漂移而非科学口径错误），已由本人按 `文件:行` 逐条复核后并入本节口径，不逐条展开；详见 §7 子代理 S4/S2 完整报告。

---

## 5 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| 1 | 把 `rejection.cpp:1157` 的 `n < 6u` 改成 `n < 7u`，观察 `p2_rejection_percentile_band_min_n()` 是否变化 | 若变化则 §11.4 F2 的「锁定机制」是有效门 | **推翻成功**。静态可证：查询函数（`:1144-1148`）返回 `constexpr kPixelSmallNPolicy`（`:1141-1142`）的重述，与路由分支**不共享可变状态** ⇒ 改路由不改查询值 ⇒ 门恒绿。**这是本片最强反例**（B-02） |
| 2 | 把 DRIZZLE §7 宣称的通量守恒式 `Σ_p F_p = Σ_j x_j` 两侧各减一个样本 | 若该不变量是「严格」不变量，减样本后应仍成立 | **推翻成功**。该恒等式只在 drop 几何闭合时成立；§8 的 NaN 样本掩膜路径会从 `F_p`/分母/方差中**一并剔除并重归一**，故严格守恒在该路径下破裂，而 §7 未标注例外（B-05 的机理面） |
| 3 | 在 `PHASE2_SESSION.md:128` 记录的常量面里找 `upm_weight_source` | 若该字段存在则 §5 常量面完整 | **推翻成功**。`git -c core.quotepath=false grep -rn upm_weight_source -- lib/ eng/` → **0 命中**；代码赋的是 `snr_weight_mode`（S-17） |
| 4 | 在 `docs/detail/algorithms_phase2/` 下找 `12_rejection.md`（REJECTION.md:340 指定的「唯一正本」） | 若存在则档界正本链路完整 | **推翻成功**。目录不存在于活仓；`git log --diff-filter=D` 显示由提交 `ac058121` 连同另外 5 文件一并删除；**27 份存活文档/源码**仍指向它（S-01） |
| 5 | 用 `wc -l` 反查 `PHASE2_REJECTION.md` 自称的 2965 / 2950 | 若两个值有一个对，则 §3「行号一律照录实测值」成立 | **两者皆错**。实测 2959（S-02）。该文档 §7/§11.3 的 DISP-P2REJ-003 登记的「实测 2950」本身是第三个错值 |
| 6 | 在 `eng/tools/` 下找 `science_contract_lint.py`（`REJECTION.md:319`/`DRIZZLE.md:254` 的 Acceptance 门） | 若存在则发布门有约束力 | **推翻成功**。文件不存在（S-13）⇒ **恒真门之「恒红对偶」**：门永远无法转红，等于不存在 |
| 7 | 独立复算本片全部可算常数，看是否有人写错 | 若全对则数值纪律可信 | **全部复算通过**：`√(π/3)·(180/π)·3600 = 211076.28514206142`、`2√2/π−1 = −9.968368384e-02`、`(π/3)(1−2√2/π) = 0.1043885`、`1/1.482602218505602 = 0.6744897501960817`、`ln0.05/ln0.99 = 298.0729`、`0.05^(1/64)=0.95427`、`‖k‖₂=0.141047 → 35.449`、`2.354820/1.230310=1.914005`、截尾均值闭式 `0.731673095`、Moffat FWHM `1.230307652590102`、`裕量 1.0486`、`δ` 残差表五行、`S/B0` 注入态三档。**数值纪律是本片最扎实的部分；问题集中在判据鉴别力与正本↔派生↔代码三方闭合** |
| 8 | 检验 `SCIENCE_SCOPE.md` 的 γ 失效判据能否在自身判据面内触发 | 若能触发则门有判别力 | **推翻成功**。判据面（散粒主导）由 `\|γ−1\|≤0.3` 选入 ⇒ 域内 γ≤1.3 ⇒ 门（γ>1.3）结构上永假（S-28） |
| 9 | 用同片 `IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md` 的取证结论反查 `HIPS_WRITER.md` 的写出键 | 若两处一致则 HiPS 合规性成立 | **推翻成功**。研究包（带 URL + 逐字原文，21 条证据）结论：HiPS 有效版本仅 1.0(REC)/2.0(WD)，`HIPSTILEWIDTH` 全部检索 0 命中并明写「建议不要当作规范关键字使用」；而 writer 写出 `hips_version="1.4"` 与 `HIPSTILEWIDTH=512`（S-12） |
| 10 | 检查 `DISPUTE_RESOLUTION.md` §13 与 §27 是否给出同一结论 | 若一致则终裁可落笔 | **推翻成功**。§13:168「`w` 对 `m_ref` 不变逐位」被 §27:293-297 的实测漂移 3.83% 直接推翻；且 §27 已自标该证据 `is_tautology:true`（B-01） |
| 11 | 用 `grep "仅 TAN"` 反查 `PHASE3_PROJ_IMPL.md:154` 所引的上游条款 | 若该句存在于 DESIGN，则以其为据撤销他域冲突成立 | **推翻成功**（**证伪的是文档、不是文档的结论**）。`docs/ACSD_DESIGN.md` 仅 682 行，「仅 TAN」命中 **0**；§6.3 实为 `:383-386`，`:493` 落在目录树代码块内。⇒ 文档的「产品受理集={TAN}」结论本身仍与 DESIGN:386「缺省 TAN」+ :385「未实现的投影…显式报不支持」相容，**但它据以撤销他域冲突的那句引文不存在**（B-07） |
| 12 | 用代码注释反查 `PHASE3_FITS_IMPL.md` 冻结的发布序 | 若文档序与代码一致则完整性锚合同成立 | **推翻成功**。`p3_output.cpp:447` 明写「**原序**是 flush → close → fsync → **rename → sha256/校验**」，`:179` 明写 fitsverify「**位于 rename 之前**」⇒ 实现已按 P-205 把序改成「rename 前全验」，而文档仍冻结旧序（B-08） |
| 13 | 用 `grep -rn FIRSTPIX` 反查 `HIPS_WRITER.md:384` 的「全仓无消费方」 | 若无消费方则 §10「删除」处置安全 | **推翻成功**。`hips_core.c:689-692` 是生产 fail-closed 校验器，`gate7_hips_validate.py:101` 直接判该两卡 —— 而后者恰是 §9 钦点的 oracle ⇒ 按「删除」处置会**打断文档自己钦定的 oracle**（S-29） |

---

## 6 盲复算

**方法**：遮住既有判定与子代理结论，仅依据片清单 + 源码，独立复算三项承重指标，再与原结论比对。

| 复算项 | 独立复算结果 | 与原结论比对 | 判定 |
|---|---|---|---|
| 成员覆盖率 | 逐份 `wc -l` 对账清单：28 份合计 **9979**，与权威版 `:935/:937` 声明的 28 份 / 9979 行**逐份零差** | 一致 | **一致** |
| 源码行数（抽 8 个承重锚点） | rejection.cpp **2959**；rejection.h **603**；coverage.cpp **455**；coverage.h **172**；p2_session.cpp **318**；p2_session.h **39**；p3_output.cpp **1270**；p3_wcs.h **373** | 与文档声明：rejection.cpp/h **不符**（2965/2950/602）；coverage **符合**；session **符合**；p3_output.cpp **符合**、p3_wcs.h **符合**。另两处本人新查出：`p3_output.h` 声明 176 实为 **178**；`dpsf_psf.cpp` 声明 1432 实为 **1433** | **一致**（既有判定同样认定 rejection 组失配；本盲复算**新增**两处此前未被任何一方点名的漂移：`p3_output.h` 176→178、`dpsf_psf.cpp` 1432→1433） |
| 悬空引用规模 | `docs/detail/algorithms_phase2/` 被提交 `ac058121` 删除；`grep -rln` 存活文档+生产源码 → **27 份**，引用 **69 处**；删除的 6 文件为 09_coverage/10_sampling/11_upm/12_rejection/13_integration/README | 与既有判定一致（RR01:221 只点了 `ARCH-001.md:95` 一处，R2-S1:385 只提「13_integration.md 同样不存在，见 M5」） | **一致但盲复算更严**——既有记录是点状，本片把它**坐实为删除提交 + 27 份/69 处全仓量化**，并确认范围横跨两层正本与活生产 `.cpp`。属**已知根的量化扩展**，非全新问题 |
| `upm_weight_source` | `git grep -- lib/ eng/` → **0 命中**；代码实为 `uc.snr_weight_mode = 0`（`p2_session.cpp:199`） | 既有判定（子代理）同 | **一致** |
| 恒真门计数 | 本片命中 4 处：B-01（代数恒等式型）、B-02（同源查询型）、S-28（判据面自污染）、B-06（往返自证+复用被测）；另 S-13 为「恒红对偶」（门永远无法转红） | 与既有判定同（各子代理在各自组内命中 1–2 处，互不重叠） | **一致**。且**双向**体检：未发现「两个量逐位相同时 `判<=1e-4` 恒红」型恒红门；`DISPUTE_RESOLUTION.md:253-259`（§23 `H_solve` 恒真门适用域）同时写死了「恒真成立域」与「κ≈2.2e10 贴门失效域」，是**本片唯一做了双向体检的正确样板**，建议作为其余恒真门条目的写法范本 |
| 恒真门 vs 正确样板 | `PHASE3_PROJ_IMPL.md:616-617` 主动识别「现状实现在 30° 错映射下往返误差仍 4.5e-15 px ⇒ 只保留『往返』会恒绿」，并拆成 (i) 往返不变量 + (ii) astropy 绝对对拍（含 dec0≠0 用例），另配 `ACSD_P3PROJ_FAULT` 故障注入 | 无既有判定（各子代理只报缺陷，未把该文件列为正面样本） | **本盲复算新增的正面结论** |

**盲复算总判**：**一致**，无处偏松或偏严。三处差异均为**本盲复算的增量**（两处新行数漂移、恒真门样板文件升格为通过），无一处放松了既有判定。

---

## 7 子代理派发记录

**派发 5 个**，按文件集**互斥**拆分，各自只读授权文件、禁止改仓；本人在其之上做全量亲读与逐条复核。

| # | 授权文件集（互斥） | 份数/行数 | 回报 |
|---|---|---|---|
| S1 | `algorithms/PHASE2_REJECTION.md` + `REJECTION.md` + `DISPUTE_RESOLUTION.md` + `algorithms/PHASE2_COVERAGE.md` | 4 / 1948 | 排异/分歧组：3 份判**阻断**、1 份须修 |
| S2 | `algorithms/PHASE3_PROJ_IMPL.md` + `algorithms/PHASE3_FITS_IMPL.md` + `algorithms/HIPS_WRITER.md` + `IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md` | 4 / 2221 | Phase3/HiPS 组 |
| S3 | `CALIBRATION_ALGORITHMS.md` + `algorithms/COSMETIC_ALGORITHMS.md` + `algorithms/PHOTOMETRIC_FIT.md` + `algorithms/NOISE_ESTIMATION.md` + `PHOTOMETRY_RESEARCH_PACK.md` + `algorithms/GAIA_QUERY.md` | 6 / 2252 | 标定/测光组：1 份判**阻断** |
| S4 | `algorithms/PHASE2_SAMPLER.md` + `algorithms/PHASE2_UPM_IMPL.md` + `algorithms/PHASE2_SESSION.md` + `algorithms/UPM_SOLVER.md` | 4 / 1755 | UPM 组：2 份判**阻断**、2 份须修 |
| S5 | `algorithms/DRIZZLE_GEOMETRY.md` + `DRIZZLE.md` + `algorithms/STAR_PSF_ALGORITHMS.md` + `STAR_DETECTION.md` + `ACR_EQUIVALENCE.md` + `UNIFIED_SCIENCE_MODEL.md` + `SCIENCE_SCOPE.md` + `IO_001_FITS_STREAM_INTERFACE.md` + `README.md` + `algorithms/README.md` | 10 / 1803 | Drizzle/星检组：2 份判**阻断** |

**本人如何逐条复核**：5 份报告全部读到后，对**每一条承重结论**独立重取一手证据（`wc -l` / `git grep` / `sed -n` / 打开源码段），再决定采纳、降级或否决。**共采纳 34 条、否决 11 条、部分采纳 4 条。**

### 否决清单（附理由）

| # | 子代理结论 | 我的处理 | 否决/降级理由 |
|---|---|---|---|
| V-01 | S1 提出「`*median|/s ≲ 2` 时全拒」对**奇数 n 不成立**，生产档 `4≤n≤5` 的 n=5 逃过全部降级面 | **采纳为 S-12 的一部分但降级为「须修·推断」** | 该推断方向合理（奇数 n 中位样本恒 ACCEPTED ⇒ 剔 n−1 ⇒ `status=OK`），但**依赖 `scratch_median` 对奇数 n 的具体实现**；我未逐行核 `rejection.cpp:1837-1846` + `:1061-1069` 的组合行为，故不列入阻断，登记为待前台抽验的推断 |
| V-02 | S1 称 `PHASE2_REJECTION.md:509` 是伪引（称「SCI §2 符号表引用行号」而 SCI §2 不含行号） | **采纳（S-03 的子项）** | 核验成立：`REJECTION.md:12-26` 的 §2 确无行号，行号在 `:238`（§13）。降级为须修（非阻断），因不改变科学口径 |
| V-03 | S2 报告 `HIPS_WRITER.md` 全部通过 | **否决其「通过」结论** | S2 未把 `HIPS_WRITER.md:205/:239/:255` 与**同片** `IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md` 的取证结论对照，漏掉片内直接矛盾（S-12：hips_version="1.4" 与 HIPSTILEWIDTH）。片内交叉验证是本项目最容易漏的一环 |
| V-04 | S5 称 `STAR_DETECTION.md` 虚警门「0 计数给出的是上界而非下界」，并据此判该门无鉴别力证据 | **降级为「须修·统计方向表述」** | 算术复核成立（3/524288 = 5.72e-6/px = 0.0057/千像素），但该门**已由文档自承**「该 0 值是判据下界，判据的鉴别力由负例注入承担」（`:57-58`），且发布门阈值 0.1/千像素本就宽于实测上界。属**表述不准**而非恒真门；我未核实是否存在所称负例注入，故不判「恒真」 |
| V-05 | S5 称 `DRIZZLE.md:207`/`:212` 章节序错乱应判须修 | **降级为 A-02（建议）** | 属形态问题，不涉科学口径；本项目规范 04 的整改分层把它归形态面更合适 |
| V-06 | S3 称 `PHOTOMETRY_RESEARCH_PACK.md:90` 的 `A&A 674, A33` 与 `:50` 的 `A3` 构成「组内自相矛盾」 | **部分采纳，保留为「待联网核验」** | 我确认组内存在 A33/A3 两个写法（`PHOTOMETRIC_FIT.md:302` 亦作 A3），但**无法在不联网的前提下判定哪个为真**；按纪律不把「组内不一致」升格为「某一方错误」，只登记冲突面交前台核 |
| V-07 | S3 称 `PHOTOMETRIC_FIT.md:229`「`match_radius=2.0` 五处调用点」应为 2 处 | **采纳为观察项，未入正式发现清单** | 方向可信，但该锚属 `pc_api.cpp`（不在本人授权文件内），我未亲自复核 grep 输出；按「禁止编造」不采信未复核的计数 |
| V-08 | S4 称 `PHASE2_UPM_IMPL.md:215`「dense tile sheet 与 sparse 共用同一求值语义」掩盖两份手工重复代码 | **采纳为观察项，未入清单** | 该判断依赖对 dense/sparse 两段实现的手工比对，属**代码实现层**（非本片文档层）结论；本片为文档审稿，故不将其计为文档缺陷，仅记录为待核线索 |
| V-09 | S4 提出「可辨识性判据整体缺席」为阻断 | **否决其阻断定级，降级为观察项** | 该发现本身重要（生产 `upm.h:388-431` 有 20 字段 `P2UpmIdentifiability` + `:250` 声明「唯一判据」，而四份授权文档对 `identifiab/kappa/rank_rtol` 零命中）。但**缺席的是「文档未承接」**，不等于「代码未实现判据」；代码侧判决位是存在的。定级须由前台结合 `docs/science/PHASE2_UPM.md`（DOC-SCI-001，不在本片）裁决，本片无权判阻断 |
| V-10 | S4 称 Cramér 1946 §28.5 引文「两轮检索均无命中」 | **采纳为 UNRESOLVED，不判真伪** | 同意其处理方式：该句同时出现在 `PHASE2_SAMPLER.md:263` 且用 `z` 指样本中位数（与 Cramér 惯例 `m̂` 不符，更像 Serfling 1968），**疑似伪引但须联网核原文后才能定**。按纪律登记 UNRESOLVED |
| V-11 | S3 称 `CALIBRATION_ALGORITHMS.md:437` 的 cosmetic 默认值「核对通过」 | **采信为正面记录** | 非否决项，登记于此以说明本人采信范围：该默认值确经子代理实测（`module_adapters.cpp:2843-2847`），且与本片多文档一致 |

### 部分采纳（4 条）

- S3 的 `COSMETIC_ALGORITHMS.md` 坏列 `bad_column_max_seg_len` 生产默认 = 1（即「只修单列」，多列段分支生产不可达）⇒ **采纳为观察项 A-05 邻项**，但我未亲自核 `module_adapters.cpp:2860`，不升为正式发现。
- S1 的 `PHASE2_COVERAGE.md` 「§11.3 伪代码两分支漏写 `status=1`，照此修 DISP-COV-001 会把两处正确分支改错」⇒ **采纳**，方向与本片 S-03 同源（锚漂移已成片），但未单列条目。
- S4 的「`save→open max_abs==0` 是往返自证型恒真门（nlohmann json 最短往返表示由库保证）」⇒ **采纳为观察项**；该结论需核 `upm.cpp:1383-1503` 的序列化实现，我未亲核，不升为阻断。
- S5 的「`q_psf` 在 `lib/algorithms/psf/` 全目录 0 命中，是零消费者量」⇒ **采纳为观察项**；退役治理必查项命中方向正确，但未亲核 grep 输出。

---

## 8 自证段（可复跑命令）

全部命令在 `/workspace/Astro CS Database` 下执行，只读。中文路径一律已带 `-c core.quotepath=false`。

```bash
# ---- 0. 基线 ----
cd "/workspace/Astro CS Database"
git rev-parse HEAD                       # 期望 f9650dd0aed97d7f261e5e6547f4fdb505bc313b
git -c core.quotepath=false status --porcelain   # 期望空

# ---- 1. 覆盖率：成员份数/行数与权威版清单对账 ----
sed -n '933,968p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
# 28 份成员逐份 wc -l，合计应为 9979（与 :935/:937 声明零差）
# （复跑：以 :940-968 的成员路径清单为输入逐份 wc -l 后求和）

# ---- 2. S-02 源码行数自相矛盾（本片最强可复现项） ----
wc -l lib/algorithms/coverage/src/rejection.cpp \
      lib/algorithms/coverage/include/astro/phase2/rejection.h
# 期望 2959 / 603；文档 :5/:59/:612/:725 称 2965、:503 称 2950、头文件称 602

# ---- 3. 另两处此前未被点名的行数漂移（本人盲复算新增） ----
wc -l lib/algorithms/fits_output/p3_output.h \
      lib/algorithms/psf/src/dpsf_psf.cpp
# 期望 178 / 1433；PHASE3_FITS_IMPL.md:12 称 176、STAR_PSF_ALGORITHMS.md:175 称 1432

# ---- 4. B-03 support 不在 kernel 输入面（SCI 正本级口径错误） ----
sed -n '379,391p' lib/algorithms/coverage/include/astro/phase2/rejection.h   # 无 support 字段
sed -n '2047,2049p;1414,1428p' lib/algorithms/coverage/src/rejection.cpp       # 只查 values/weights；gather 层静默剔除

# ---- 5. B-02 同源 constexpr → 恒真门（静态可证，不需改码） ----
sed -n '1141,1148p;1154,1157p;1188p' lib/algorithms/coverage/src/rejection.cpp
# 查询函数返回 constexpr kPixelSmallNPolicy 的重述；路由分支判同一常量 ⇒ 改路由不改查询值
# 反证门其实已存在（未列入 §11.4 F2）：
git -c core.quotepath=false grep -n "M3PixelProfileRoutes\|P2_REJECT_NONE" \
    -- lib/algorithms/coverage/tests/synthetic_gate.cpp | head

# ---- 6. B-01 恒等式型恒真门 + §13/§27 互斥 ----
sed -n '162,170p;293,303p' docs/science/DISPUTE_RESOLUTION.md
# §13 用恒等式论证 γ=2；§27 自标 is_tautology:true / evidence_eligible:false 且实测漂移 3.83%

# ---- 7. S-01 悬空正本指针（删除提交 + 全仓规模） ----
git -c core.quotepath=false log --oneline --diff-filter=D --name-only \
    -- 'docs/detail/algorithms_phase2/*' | head          # 期望 ac058121 + 6 个文件
ls -d docs/detail/algorithms_phase2 2>&1               # 期望「没有那个文件或目录」
git -c core.quotepath=false grep -rln "docs/detail/algorithms_phase2/" \
    -- docs/ eng/ lib/ 实验/ | sort | wc -l              # 期望 27
git -c core.quotepath=false grep -rn "docs/detail/algorithms_phase2/" \
    -- docs/ eng/ lib/ 实验/ | wc -l                    # 期望 69
# 本片 4 处：REJECTION.md:340 / PHASE2_COVERAGE.md:398 /
#           PHASE2_SAMPLER.md:672 / UNIFIED_SCIENCE_MODEL.md:54

# ---- 8. S-13 跑不起来的 Acceptance 门 ----
ls eng/tools/science_contract_lint.py 2>&1               # 期望 No such file
git -c core.quotepath=false grep -rn "science_contract_lint" -- docs/science/ | wc -l

# ---- 9. S-17 PHASE2_SESSION.md:128 字段不存在 ----
git -c core.quotepath=false grep -rn "upm_weight_source" -- lib/ eng/ | wc -l   # 期望 0
sed -n '196,204p' lib/phase2_session/p2_session.cpp     # 实为 uc.snr_weight_mode = 0
grep -n "snr_weight_mode" lib/algorithms/coverage/include/astro/phase2/upm.h

# ---- 10. S-12 片内自相矛盾：HiPS 版本与 HIPSTILEWIDTH ----
sed -n '14p;502,504p;553p' docs/science/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md
sed -n '205p;239p;255p' docs/science/algorithms/HIPS_WRITER.md
git -c core.quotepath=false grep -rn "hips_version" -- eng/tests/io/ | head

# ---- 11. B-05 方差分母：正本禁令 vs 产品实现 ----
sed -n '91p;218p' docs/science/DRIZZLE.md
grep -n "covered_area\|dense_area" lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp | head
grep -n "vnum" lib/infrastructure/aio/src/hips/aio_hips_writer.cpp | grep -n "area" | head

# ---- 12. S-06 算术自证：同一行两个值 ----
sed -n '63,64p' docs/science/DISPUTE_RESOLUTION.md
python3 -c "print('(1.152*1.25/0.015)^2 =', (1.152*1.25/0.015)**2, '  vs 文档所称 5816.6')"
# 期望 9216.0

# ---- 13. S-10 已修未撤的缺陷登记 ----
sed -n '292p' lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp
# 期望 1.482602218505602（全 15 位），而 DISPUTE_RESOLUTION.md:319 仍称 4 位 1.4826

# ---- 14. 反例 1：§11.4 F2 的「锁定机制」是恒真门（静态论证，无需改码） ----
sed -n '1144,1148p' lib/algorithms/coverage/src/rejection.cpp   # 查询 = constexpr 投影
sed -n '1154,1157p'   lib/algorithms/coverage/src/rejection.cpp   # 路由分支判同一常量
# ⇒ 改 :1157 的 n<6u 为 n<7u，查询值不变 ⇒ 文档 :639 声称的「锁定」不存在

# ---- 15. B-07 承重伪引：对 ACSD_DESIGN.md §6.3 的引文不存在（本人实测） ----
grep -c "仅 TAN" docs/ACSD_DESIGN.md            # 期望 0
sed -n '383,387p' docs/ACSD_DESIGN.md            # §6.3 真实原文（无「仅 TAN 可用」）
sed -n '493p' docs/ACSD_DESIGN.md                # 期望落在目录树代码块，非条款
sed -n '383,387p' docs/ACSD_DESIGN.md | grep -c "CDELT\|CTYPE"   # 期望 0（DESIGN §6.3 无此清单）
sed -n '154p;450,453p' docs/science/algorithms/PHASE3_PROJ_IMPL.md
# 伪引根因（整链继承）：
sed -n '5,9p' lib/algorithms/projection/p3_projection_registry.h
sed -n '5p' eng/contracts/schemas/projection_registry.schema.json

# ---- 16. B-08 原子发布序：文档 vs 实现（本人实测） ----
sed -n '99,109p' docs/science/algorithms/PHASE3_FITS_IMPL.md   # 文档：rename → sha256 → verify
sed -n '446,447p;179p' lib/algorithms/fits_output/p3_output.cpp
# 代码注释：私有临时区 → 关闭/fsync → 校验 → 算哈希 → 原子改名；且「原序是 … rename → sha256/校验」
sed -n '736,737p' lib/algorithms/fits_output/p3_output.cpp      # reopen_ok 实为 6 项合取（含 bunitok/chksumok）
sed -n '119p' docs/science/algorithms/PHASE3_FITS_IMPL.md       # 文档只冻结 4 项

# ---- 17. S-29 FIRSTPIX 有活消费方，且 §9 钦点它为 oracle ----
grep -n "FIRSTPIX" lib/infrastructure/aio/io/hips_core.c | head -4
grep -n "FIRSTPIX" lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate7_hips_validate.py
sed -n '384p;338,340p' docs/science/algorithms/HIPS_WRITER.md
# ⇒ §10 判「零消费者」、§9 钦定同一脚本为 oracle，两节互斥

# ---- 18. 未读到的部分（本片为 0） ----
# 28/28 份、9979/9979 行全部读完；无未读完项。
# 子代理自陈未核项（本人同样未核，按纪律不代其断言）：
#   - Cramér 1946 §28.5 逐字引文（PHASE2_SAMPLER.md:263）—— UNRESOLVED，须联网核原文
#   - IVOA HiPS 1.0 §4.4.1 两条裸从句（PHASE2_COVERAGE.md:62-67）—— 待联网核验
#   - JCGM 100 (GUM) §5.1.2 式(10)（SCIENCE_SCOPE.md:36）—— 待联网核验
#   - PHOTOMETRY_RESEARCH_PACK.md 的 DOI/卷页/逐字引文 —— 待联网核验（组内 A33/A3、Bohlin 160,21 vs 159,246 冲突）
```

---

## 9 本片相对前三轮的增量

| 类别 | 结论 |
|---|---|
| **全新（前三轮未记录）** | ① B-02 同源 constexpr 恒真门（生产档 6/15/16 档界零覆盖）——S1 命中，本片首次坐实其可证性；② B-06 `PHOTOMETRIC_FIT.md:202` 复用被测实现的「实证判据」——S3 命中；③ **B-07 对 `ACSD_DESIGN.md §6.3` 的承重伪引**（本人实测「仅 TAN」0 命中、`:493` 在目录树块内、伪引经 `p3_projection_registry.h:5-9` 与 schema 整链继承）——S2 命中，**使本片「通过」样本降级为阻断**；④ **B-08 `PHASE3_FITS_IMPL.md` 原子发布序与实现完全相反**（代码注释 `:447` 明写「原序是 flush→close→fsync→**rename→sha256/校验**」，已由 P-205 修正）——S2 命中；⑤ S-12 HiPS 版本/`HIPSTILEWIDTH` 与**同片研究包**结论冲突——S2/S5 均漏，属片内交叉验证盲区；⑥ **S-29 `DISP-HIPS-012`「零消费者」为假且处置建议会打断 §9 钦定的 oracle**——S2 命中；⑦ S-28 γ 判据面自污染（本人盲复算新增）；⑧ S-01 的**删除提交 `ac058121` + 27 份/69 处**量化（前三轮只点了 2 处实例，属已知根的量化扩展）；⑨ S-02/S-03 的行数与锚漂移规模；⑩ S-25 DISP-CAL-010 同 ID 双登记、S-24 §1 与 §3.6 接线状态自相矛盾、S-26 14 vs 18 符号数打架（本人亲读发现，五份子代理报告均未提）；⑪ S-30 `dataproduct_subtype` 越封闭词表且无 DISP 登记——S2 命中 |
| **本片判定的复核轨迹** | 初稿判「阻断 4 条」；第 5 个子代理（Phase3/HiPS 组）回报后，本人对其 4 条阻断级指控逐条独立复验，**全部成立**，遂上调为「阻断 8 条」，并把 `PHASE3_PROJ_IMPL.md` 由「通过」降级为「阻断」。此为**本片唯一一次判定上调**，上调依据全部为本人亲取的 `grep`/`sed` 证据（见 §8 第 15–17 组命令） |
| **已知根的量化扩展** | S-01（`algorithms_phase2/` 悬空指针：RR01:221 与 R2-S1:385 已点两处实例） |
| **与既有判定一致** | B-01（B-03/B-04）、B-05、S-02/S-03/S-04、S-06/S-07/S-08/S-09/S-10、S-13/S-14/S-15/S-16/S-17/S-18/S-19/S-20/S-21/S-22/S-23 |
| **本片质量正面样本** | `PHASE3_PROJ_IMPL.md:616-617`（**该文件整体已因 B-07 降级为阻断，但这一处局部写法仍是正确范本**）：主动识别「现状实现在 30° 错映射下往返误差仍 4.5e-15 px ⇒ 只保留『往返』会恒绿」，并拆成 (i) 往返不变量 + (ii) astropy 绝对对拍（含 dec0≠0 用例），另配 `ACSD_P3PROJ_FAULT` 故障注入。另有 `IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md`（21 条带 URL 与逐字原文的外部取证，含负面检索与「未能核实」专节 —— 子代理联网复核确认 HiPS 1.0 REC/2.0 WD/MOC 2.0 各引文**逐字命中**，6 组待复核项已登记）、`ACR_EQUIVALENCE.md`（绝对容差三档「恒真/有意义/不可达」的完整论证）、`DISPUTE_RESOLUTION.md:253-259`（§23 恒真门双向体检样板）、`PHOTOMETRIC_FIT.md:263`（自陈「F1 只覆盖 median 通路、不覆盖 IRLS 通路」）、`COSMETIC_ALGORITHMS.md:435-443`（不变量②的非退化伴随断言写法）、`HIPS_WRITER.md:318-340`（§9 oracle 权重取未钳制面积、显式防「同源 oracle 恒绿」，子代理复核判为**判据力设计正确、不必返工**） |

**最重单条（附 文件:行）**：`docs/science/DISPUTE_RESOLUTION.md:162-170` —— 用代数恒等式承载 γ=2 的冻结终裁，而同文件 `:300-303` 已把该证据自标为不承载证据、`:293-297` 已实测推翻其结论。
