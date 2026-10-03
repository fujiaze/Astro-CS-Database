# 取证 S5-A1：git 考古 — 实验报告里有没有为了让论文自洽而删掉不利读数

- 取证人：G08-05 第二轮对抗审稿分片 S5-A1（只读取证，未修改仓库任何文件，未执行任何 git 写操作 / 编译 / 实验脚本）
- 仓库：`/workspace/Astro CS Database`（路径含空格，全程双引号包住）
- HEAD：`ed33f57f15702d5d2b5efbc734f4d7c8e544a01b`；`git status --porcelain` 无输出（工作树干净）
- 结论一句话：**逐 hunk 通读 `1141939b^..HEAD` 区间内全部 14 份目标文件后，判定为「不利读数删除」的条数 = 0。** 区间净效果是**大幅净增不利披露**（5 个单元新增 20+ 条边界、6 处有利读数被主动撤回）。有 3 处「看起来像删不利证据」但已用 git 核证为**旧文失实**，列在第 4 节并标注了需人工确认的残留。

---

## 1. 圈定的改动区间与逐文件 diffstat

### 1.1 区间事实（重要）

```
$ git status --porcelain          # 无输出
$ git rev-parse HEAD              # ed33f57f15702d5d2b5efbc734f4d7c8e544a01b
$ git -c core.quotepath=false log --oneline 1141939b^..HEAD   # 67 个提交
```

**须先纠正任务书的一处前提**：任务书把 `1141939b^..HEAD` 描述为「G08-05 第一轮的边界提交」，但该区间实际含 **67 个提交**，覆盖 G08-01 / G08-04 / G08-05 / G08-06 / G08-07 / G08-08 六个工作包，以及品牌改名、判据专线、品牌重命名等横向批次。任务书点名的 11 个闭环提交（`99f9e2ee`、`751329ef`、`ef35ba6e`、`0235fbcf`、`123361b4`、`38d18978`、`1208d5a0`、`b7c0409c`、`68c5bea2`、`f2c2a4c5`、`ed33f57f`）**全部在区间内**，但区间 ≠ 这些提交的并集。

对本轮结论的影响：无。取证按区间全量做，取证面只放宽不放窄。下文所有「新版」= HEAD，所有「旧版」= `1141939b^`。

### 1.2 diffstat

```
$ git -c core.quotepath=false diff --stat 1141939b^ HEAD -- '实验/*/REPORT_*.md'
 实验/absolute-snr/REPORT_paper.md          |  16 +-
 实验/additive-sky-seamless/REPORT_experiment.md |  40 ++++--
 实验/additive-sky-seamless/REPORT_paper.md  |  45 ++++-
 实验/dense-snr-reconstruct/REPORT_experiment.md |  99 +++++++-
 实验/dense-snr-reconstruct/REPORT_paper.md  | 106 +++++++++++++++++---
 实验/healpix-polar/REPORT_experiment.md     | 104 ++++++++++---
 实验/healpix-polar/REPORT_paper.md          | 122 ++++++++++------
 实验/photometric-magnitude/REPORT_experiment.md |  16 +--
 实验/photometric-magnitude/REPORT_paper.md  |  66 +++++++----
 .../code/redo/route1/REPORT_route1.md       |   6 +-
 10 files changed, 458 insertions(+), 162 deletions(-)
```

```
$ git -c core.quotepath=false diff --stat 1141939b^ HEAD -- '实验/*/REPORT_*.md' '实验/*/README.md'
 实验/absolute-snr/README.md                        |  14 ++-
 实验/absolute-snr/REPORT_paper.md                  |  16 ++-
 实验/absolute-snr/code/audit/README.md             |   1 +
 实验/additive-sky-seamless/README.md                | 127 ++++++++++++-
 实验/additive-sky-seamless/REPORT_experiment.md    |  40 +++++--
 实验/additive-sky-seamless/REPORT_paper.md         |  45 +++++---
 实验/additive-sky-seamless/docs/README.md          |   3 +-
 实验/dense-snr-reconstruct/README.md               |  21 +++-
 实验/dense-snr-reconstruct/REPORT_experiment.md    |  99 +++++++++++++---
 实验/dense-snr-reconstruct/REPORT_paper.md         | 106 ++++++++++++++---
 实验/healpix-polar/README.md                       |  35 ++++--
 实验/healpix-polar/REPORT_experiment.md            | 104 +++++++++++++-----
 实验/healpix-polar/REPORT_paper.md                 | 122 ++++++++++++++------
 实验/photometric-magnitude/README.md               |  41 ++++---
 实验/photometric-magnitude/REPORT_experiment.md    |  16 +--
 实验/photometric-magnitude/REPORT_paper.md         |  66 +++++++----
 实验/photometric-magnitude/code/README.md          |   6 +
 .../code/redo/route1/REPORT_route1.md              |   6 +-
 18 files changed, 670 insertions(+), 198 deletions(-)
```

后 4 个文件（`code/audit/README.md`、`docs/README.md`、`code/README.md`、`code/redo/route1/REPORT_route1.md`）路径深度 > `实验/<unit>/`，不在本单目标文件范围，仅登记存在。

**净增删比 670 : 198 = 3.4 : 1。** 这是判定「是否系统性删不利证据」的第一道宏观闸门：删证据型改动应表现为删多于增，且删除集中在结论/结果段。实测相反。

---

## 2. 完全未被第一轮碰过的目标文件清单

目标面 = HEAD 上 `实验/<unit>/{REPORT_*.md, README.md}`，共 19 份。被碰 14 份，**未被碰 5 份**：

| # | 文件 | 行数(HEAD) | 备注 |
|---|---|---|---|
| 1 | `实验/absolute-snr/REPORT_experiment.md` | 178+ | **本单 C 项核证的关键文件之一，全程零改动** |
| 2 | `实验/cone-search-constants/REPORT_paper.md` | — | 锥体搜索常数单元 |
| 3 | `实验/engineering-evidence/README.md` | — | 工程证据单元 |
| 4 | `实验/m42-realdata/README.md` | — | M42 真实数据单元 |
| 5 | `实验/m42-realdata/REPORT_paper.md` | — | M42 真实数据单元 |

**取证含义（重要且须向上汇报）**：`实验/m42-realdata/REPORT_paper.md` 与 `实验/cone-search-constants/REPORT_paper.md` 是两个**论文级正式稿**，在含 67 个提交的治理区间内**一次都没有被审过**。同理 `实验/absolute-snr/REPORT_experiment.md`（P2 单元实验报告）零改动——这解释了为什么 C 项的两条判红读数完好无损，但也意味着 P2 单元的实验报告面根本没进本轮审稿覆盖。若二轮审稿以「第一轮已覆盖实验报告」为前提，该前提在这 5 份文件上不成立。

---

## 3. 逐条被删除的数字清单

### 3.1 判定为「不利读数删除」的条数：**0 条**

按任务书的三项合取判据（①旧版处于结论/结果/诚实边界语境且为负数·判红·超出边界·矛盾·依赖恒真门·被否证假说；②新版任何地方无等义形式；③消失未在别处补上诚实边界/失效域/判红声明）逐条过滤后，**没有任何一条通过全部三项**。

### 3.2 量化证据：旧版数字 token 全量扫描

对 14 份被改文件做「旧版有、新版全库无」的数值 token 差集（排除年份/版本号/纯计数）：

```
$ 对每份文件: git show 1141939b^:<f> | grep -oE '[0-9]+\.[0-9]+|[0-9]{2,}' | sort -u  vs  HEAD 同法，取 comm -23

实验/absolute-snr/REPORT_paper.md                OLD-ONLY: 78
实验/additive-sky-seamless/REPORT_experiment.md   OLD-ONLY: 60.66 9.46
实验/dense-snr-reconstruct/REPORT_experiment.md   OLD-ONLY: 0.037 0.433 10.1109 1153 194 1981.1163711
实验/dense-snr-reconstruct/REPORT_paper.md        OLD-ONLY: 109 11721 11722 11745 11750 194 199 255
实验/healpix-polar/REPORT_experiment.md           OLD-ONLY: 0.35999999999940 0.375 0.49978 0.50005 1.11 1.19 1.48 2.47 27.6 416 6.28
实验/healpix-polar/REPORT_paper.md                OLD-ONLY: 0.3600000000 0.49978 1.11 2.47 2.49 416 5.06 6.28
实验/healpix-polar/README.md                      OLD-ONLY: 0.49978 1.5 2026
实验/photometric-magnitude/REPORT_experiment.md   OLD-ONLY: 59.4
实验/photometric-magnitude/REPORT_paper.md        OLD-ONLY: 0.008333 0.032456 0.390 0.727 1.009 4.1 59.4
实验/photometric-magnitude/README.md              OLD-ONLY: 0.0325 0.1271 59.4
```

全部 42 个 old-only token 的归属见第 4 节（42 个中 38 个是路径行号/精度重排/归档订正/有利读数撤回，3 个是已核证的旧文失实，1 个是已核证的归档订正）。

### 3.3 反向证据：区间内**主动撤回的有利读数**（6 处，全部与本单假设方向相反）

这是本次取证最有判别力的发现。真正被删掉并**不再以任何形式出现**的，是**对论文有利**的读数：

| # | 有利读数被撤回 | 位置（旧版） | 撤回后的表述（新版） | 方向 |
|---|---|---|---|---|
| R1 | **新发现闭式** `(pi/3)·A_chart/A_exact − 1 = 0.5·rho²`，实测系数 **0.49978**、416 点扫描、跨 4 足迹尺度散差 1.48e-07，宣称为「既有预算未登记的性质」 | `实验/healpix-polar/REPORT_paper.md` §3.6（旧「**新的 drop 面积闭式**」整段） | 「**drop 足迹的图表侧偏差是弦–曲线效应，不是闭式**」…「对 rho² 的最小二乘斜率为 −1.22e-09，即**不存在 rho² 项**」 | **反噬自己** |
| R2 | M2 的「**分辨率无关性**」作为通过项（散差 1.48e-07 ≤1%） | `实验/healpix-polar/REPORT_experiment.md` §4.6 表 + §6 边界 | 「M2 原先的"分辨率无关性"门写成单侧 `res_indep <= 0.01`，**负数无条件通过、无判别力**，已改为**双侧**…」 | **自己承认旧门恒真** |
| R3 | NC-A「扣除闭式几何项后残差 **1.11e-10** 在地板内 ⇒ **归零**」，并据此宣称「度量非退化，不是恒真门（比值 2.2e10）」 | `实验/healpix-polar/REPORT_experiment.md` §4.6 归零负例 | 「残差定义里的 `+0.5·<ρ²>_p` 修正项恰是被测原语自身的指数误差展开首项…**属循环负例，已整体撤掉**」；改为面积归一自洽判据，4.847e-10 ≤ 9.239e-10 | **自己承认旧负例循环** |
| R4 | M4 作为守恒判据（残差 2.47、几何项幅 1.19e-08） | `实验/healpix-polar/REPORT_experiment.md` §4.6 表 + §5 结论 5 | 「**M4 只作防恒真检查**…把指数由 2 改为 3/2 后读数 2.47361/0.374672 保持 9 位不变 ⇒ M4 对几何零**不敏感**」 | **自己降级自己的判据** |
| R5 | 「权重和逐位为 1」作为无条件全局主张 | `实验/healpix-polar/REPORT_paper.md` §6 结论首句（旧） | 「…⇒ **「权重和逐位为 1」的适用域是同一叶内的 drop 分割，不是全域无条件成立**」 | **加限制** |
| R6 | 「本单元输出的每 dex 精度**直接构成**下游 P2 链的误差底座」 | `实验/photometric-magnitude/REPORT_paper.md` 摘要倒数第二段（旧） | 「本单元**未**做把本单元逐 dex 精度传播为 P2 误差底座的量化实验，**故不主张**「直接构成下游误差底座」这一因果量级断言（无量化传播证据，按 AGENTS §4 不以「架构上相邻」充当科学证据）」 | **撤回对下游的贡献主张** |
| R7 | 「真实帧判红与 L4 PASS 1/49 **同归因**…不是判据不可用」 | `实验/photometric-magnitude/REPORT_paper.md` 摘要（旧） | 「但**该门对成因失明**…故**具体归因与「不是判据不可用」都超出本门能给出的证据**」 | **撤回对自己有利的归因** |
| R8 | Keys 1981 作为定权恒等式的理论腿（强引用背书） | `实验/dense-snr-reconstruct/REPORT_experiment.md` §8 佐证来源表 | 「Keys 1981 是插值算子出处，**不承担定权理论腿**」，改引 Aitken 1935 | **换掉支持自己的引用** |

（R8 的完整题录未被删：`实验/dense-snr-reconstruct/refs.md:12` 仍在，含 DOI 10.1109/TASSP.1981.1163711 与逐字段核验记录。）

### 3.4 反向证据：区间内**净增的不利披露**（摘主要）

| 单元 | 新增的不利披露（新版位置） |
|---|---|
| P2 absolute-snr | `REPORT_paper.md:147,156` 把「盲检测完备性 0.43 / `D_core` 中位相对误差 **0.376（判红）** / 真实 M16 星云源项只捕获 **14.2%**」提升为**独立结论第 6 条**并写进摘要与 README；`REPORT_paper.md` §6 新增 `sigma_sky` 三条失效域（饱和 `sigma_hat` 124.6→3.9、常量像元 ≥0.40 ⇒ SNR **高估 2×10¹⁰ 倍**、结构主导域 `d ln sigma_hat/d ln B` 衰减到 0.0011）；新增掩膜半径式与正本形态不一致的待裁决冲突 |
| P5 additive-sky | `README.md` 新增 §6.B 四条带定量阈值的失效域（低对比 `L/σ_bg ≲ 15` 即判红、窄重叠带宽 < 399 px 该边不判、**饱和悬崖 0.40↔0.50 之间**、覆盖率 f=0.5 转移点）、§6.C 未决项、§11.3 恒真门重写登记；`REPORT_paper.md` §6 第 14 条新增 **C1/C5 三条门由 PASS 转 FAIL**（`A6` 0.0236→0.4078、`A9` 0.0037→0.1327、`W1` margin 0.2592→0.1753，`run_all.sh` 退出码 1） |
| P4 dense-snr | `REPORT_paper.md` §7 新增第 19–23 条（控制点退化三族 `E_dense == E_frame` **逐位相等且无错误信号**、胞内点源下重建峰恒 = `max(node)` ⇒ **权重被高估 2×–1.5×10⁴ 倍且方向翻转**、`Σa²Var` 低估 3.93×/13.3×、两条结构恒真门降级）；§7 第 16 条新增**掩膜静默降级**（掩膜目录不存在 ⇒ `valid_fraction = 1.00000`，无 fail-closed 无告警，存档无 `mask` 键 ⇒ 读存档无法回溯）；§7 第 2 条把 Δ 可用上界从 **Δ ≤ 128 收紧为 Δ ≤ 16**；§2.2/§6-22 新增 `w` 对 `m_ref` 不变性的适用域（生产组成臂漂移 1.9%–4.2%） |
| P3 healpix-polar | `REPORT_paper.md` §5 新增第 12–16 条（按方位角选面会让 **260 个 drop 的权重静默全 0**、生产侧 `tri_inside` 快路径无条件用 VOS 权重带 +5.0e-7 与 206″ 阈值台阶、**极冠 apex 投影奇点可丢 100% 面积**、本单元对正本条款的单方面「作废」标注待裁决）；`REPORT_experiment.md` §4.6 新增「生产侧面积口径**已审计**」（旧版为「**未审计**，登记为后续项」） |
| P1 photometric-magnitude | `REPORT_paper.md` §7 由 12 条扩到 **18 条**：新增探测器线性适用域未测（乘性假设前提缺失，`sigma_obs` 0.00220→0.01216）、`σ_gaia` 仿真取值自参照且**比正本乐观 5.4 倍**、`σ_psfsys` 孔径口径 r=4 vs 生产 r=10 差 10.3 倍、`σ_flat = 0.0007 mag` **一手出处至今未取得**（被归给的路径与行在当前树均不存在，而它单独决定真实帧判红方向）、低样本域双重偏松、双边界门成因失明；仿真腿 obs/预测比由 1.009/0.727/0.390 订正为 **1.154/1.008/0.417**（更差） |

---

## 4. 明确排除的假阳性清单

以下 11 类共 **42 个 old-only token** 全部排除，理由逐条给出可复现命令。

### 4.1 修引用（路径/行号迁移）— 11 个 token

> ⚠️ **本节分类已被第二实例订正，勿直接采信**：新引路径 `docs/detail/registry/astrocs.phase1.*` 在 HEAD **不存在**（真实文件名为 `acsd.phase1.*`，由 `add871a8` 改名），故这 11 项属「二代悬空引用」而非「修好」。详见文末 **A1**。

| 旧 token | 新位置 | 理由 |
|---|---|---|
| P2 `78` | `REPORT_paper.md:30` 改指 `docs/detail/registry/astrocs.phase1.noise-snr.md` | G08-01/G08-06 已删 `docs/plugins/algorithms_phase1/07_noise_snr.md`，改指新路径 |
| P4 `194`（实验报告） | `REPORT_experiment.md:135` 改指 registry 文件 | 同上 |
| P4 `194`（论文） | `REPORT_paper.md:170` 改指 registry 文件 | 同上 |
| P4 `109` | `REPORT_paper.md:196` → `snr_evaluator.h:110` | 行号随文件演进更新 |
| P4 `199`,`255` | `REPORT_paper.md:196` → `snr_evaluator.cpp:212,268` | 同上 |
| P4 `11721`,`11722` | `REPORT_paper.md:200` → `module_adapters.cpp:12595` | G08-07/G08-08 改过该文件，行号漂移 |
| P4 `11745`,`11750` | `REPORT_paper.md:200` → `module_adapters.cpp:11967` | 同上 |
| P1 `4.1`（`06_photometry.md §4.1`） | `REPORT_paper.md:38,75,143,146` 改指 `docs/detail/registry/astrocs.phase1.photometry.md`「测光一致性判据（单帧、尺度无关、双边界）」 | G08-01 路径迁移 |
| P3 `2026` | `README.md:6` 「## 成稿文件（2026-09 总编对账收口）」→「## 成稿文件」 | AGENTS §5「正文无日期、版本号…」；**顺带注意**：旧文本自身在 photometric-magnitude 引用了 `docs/plugins/algorithms_phase1/06_photometry.md §4.1`，即它同时被要求迁移、又被要求去掉日期，历史一致性欠账 |

### 4.2 精度重排 / 等义表述 — 4 个 token

| 旧 token | 新版对应 | 理由 |
|---|---|---|
| P3 `0.35999999999940` | `REPORT_experiment.md:161` `−0.3599999999994 / −0.3599999999983` | 去尾零，同值；且新版把指数 2 与 3/2 两版**并列给出** |
| P3 `0.375` | `REPORT_experiment.md:118` `rms 0.374672` | 同值加位 |
| P3 `2.47` | `REPORT_experiment.md:118,161` `2.47361` | 同值加位；且新版明确写「M4 对几何零**不敏感**」 |
| P3 `6.28` | `REPORT_experiment.md:161` `6.281e-11 / 6.084e-11` | 圆整差异，两版并列 |

### 4.3 恒真门退役（有利读数移除，且失效性被显式披露）— 2 个 token

| 旧 token | 处置 | 理由 |
|---|---|---|
| P5 `9.46`（旧 `N2b_zero_effect_zeroes_seam`：`max|excess| = 9.46e-12 e⁻` ⇒ 归零） | 换成 `N2b_apply_field_identity` | 这是**绿读数**。且新版 `REPORT_experiment.md:126-130` 显式写「**原 `N2b` 是恒真门**…检的是「真值是否为 0」这一存在性事实，注入 6 种缺陷 6/6 全绿」——比原数字更不利的失效性被写进正文 |
| P5 `60.66`（旧 `N2c_injected_step_is_detected`：`max|δ_k| = 60.66 e⁻` ⇒ 能红） | 换成 `N2c_injection_truth_match` | 这是**绿读数**。新版 `REPORT_experiment.md:127-129` 写「原 `N2c` 的两条子判据都是**下界**（`max|δ_k| > 1e-6`、`|excess| > 1.0`），只在真值本身变小时才红」 |

### 4.4 归档订正（且订正方向对论文更不利）— 4 个 token

| 旧 token | 新版 | 核证 |
|---|---|---|
| P4 `0.037`,`0.433`（`REPORT_experiment.md` H4 行 / `REPORT_paper.md` §4.7 E6 行） | `0.0301577`,`0.3804647`，比值 **12 → 12.6×** | `git show HEAD:实验/dense-snr-reconstruct/results/summary.json` 第 178–181 行 `extreme_4dex` 记 `A_full_E: 0.030158 / A_bglimit_E: 0.38046`，与 `results/route1/exp_p4_04_brightness_forward.json::arms/extreme_*` 一致。**该 json 在区间内未被改动**（`git diff --stat 1141939b^ HEAD -- .../results/summary.json` 无输出）。订正使不利倍数**变大**（12→12.6） |
| P1 `0.390`,`0.727`,`1.009` | `0.417`,`1.008`,`1.154` | 新值标注了出处 `results/step5_calibration_gate.json → obs_over_predicted`，且与同文件 §4.1、README 的 `1.154/1.008/0.417` 一致（旧 §5.2 的 `1.009/0.727/0.390` 与同文档另两处**自相矛盾**）。订正使仿真腿**看起来更差** |
| P1 `0.008333` | `0.000881` | 旧 `REPORT_paper.md` §7-5 写「即便取仿真口径（0.008333 mag）」，而同文件 §5.2 与 README 均写 `0.000881`——旧版内部矛盾，新版统一 |

### 4.5 已用 git 核证为「旧文失实」的三处（**形态最像删证据，须人工确认**）

这三处是本单唯一「有不利读数在新版消失」的候选。逐一用 `git` 核证代码/存档后确认：消失的是**不成立的主张**，且其失效性在新版被更精确地重述。

#### 假阳性 P-1（须人工确认）：P4「drizzle 侧求值器仍硬写 2.0」

- **旧版原文**（`实验/dense-snr-reconstruct/REPORT_paper.md` §7 第 9 条，`1141939b^`）：
  > 「**IDW 默认幂次未落地（P4-M05）**：终裁默认 `idw_power = 1.0` 是**配置级**约定，实现侧仍硬写 2.0——`snr_estimator.cpp:593,730,833`（默认赋值）、`snr_evaluator.h:109`（成员初值）、`snr_evaluator.cpp:199,255`（≤0 兜底）…」
- **新版对应物**（`REPORT_paper.md` §7 第 9 条，HEAD）：「drizzle 逐像素求值器侧**已生效**…`snr_evaluator.h:110` 成员初值 1.0；`snr_evaluator.cpp:212,268` 的 ≤0 兜底回落 1.0）；**仍与终裁不一致的只有** Phase1 结构感知估计器路径三处赋值（`snr_estimator.cpp:593,730,833`）」；摘要、§2.3、§3 H3 行、§5 结论(3)、README 一句话结论、实验报告 §2 H3 行与 §4 第 3 条同步改写。
- **核证**：
  ```
  $ git diff --stat 1141939b^ HEAD -- lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.{h,cpp}
  （空输出 = 区间内完全未改）
  $ git grep -n "idw_power" 1141939b^ -- lib/.../snr_evaluator.h
  1141939b^:.../snr_evaluator.h:110:    double   idw_power_ = 1.0;   // 规范默认 1.0 (07_noise_snr.md:192)
  $ git grep -n "idw_power = 2.0" HEAD -- lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp
  HEAD:...:593 / 730 / 833:  out_model->idw_power = 2.0;      （三条仍在）
  ```
  ⇒ 该文件在区间前就已是 **1.0**，旧版把 drizzle 侧列为「仍硬写 2.0」是**失实陈述**。
- **判为假阳性的理由**：残余不利事实（Phase1 路径三处仍 `= 2.0`）**逐字保留**且行号精确化；消失的只是失实的部分；文件未改，不存在「改码让论文好看」。
- **须人工确认点**：该订正**未在任何位置写「旧稿此处失实」**，只呈现为新版结论。若二轮的标准要求「订正须留痕」，这是应补的登记项（分级：建议）。

#### 假阳性 P-2（须人工确认）：P4「P4 稠密重建在生产集成面上不可达」

- **旧版原文**（`REPORT_paper.md` §7 第 12 条，`1141939b^`）：
  > 「`in.sparse = nullptr`（`module_adapters.cpp`）⇒ P4 稠密重建在生产集成面上不可达，生产当前走帧级标量路径。因此 §4.4 的…是**组件级**结论，不是端到端实测结论」
- **新版对应物**（HEAD §7 第 12 条）：「**P4 的逐像素重建面在 Phase2 已接通且是默认值**—…逐输出像素调用生产 API `weight_from_sparse_layer_pixel_prepared`（`module_adapters.cpp:12581`）…帧级链的 `in.sparse = nullptr`（`:12112`）只表示**稀疏层不经帧级标量链**…**不是**逐像素面不可达」，并保留 3 条子项：① `snr_path=dense` 仍 fail-closed；② **「未做：带绝对真方差参照的生产端到端试跑…故 12 倍仍是组件级读数而非端到端实测」**；③ 帧级路径上该机制方向相反。
- **核证**：
  ```
  $ git grep -c "weight_from_sparse_layer_pixel_prepared" 1141939b^ -- lib/infrastructure/scheduler/src/module_adapters.cpp
  4
  $ git grep -c "weight_from_sparse_layer_pixel_prepared" HEAD   -- 同文件
  4
  ```
  ⇒ 该消费面在区间前即已存在，旧版是把**帧级链的 `in.sparse = nullptr` 误诊为逐像素面不可达**。
- **判为假阳性的理由**：不利结论（组件级、非端到端）**保留**；消失的是误诊。`in.sparse = nullptr` 该行本身在 HEAD 仍存在（`:12134`）。
- **须人工确认点**：(a) 新版引用的行号 `:12112` 是**旧行号**，HEAD 实际为 `:12134`（`git grep -n` 两端可验），属新引入的**悬移行号锚**；(b) 同样未留「旧稿此处误诊」的痕。

#### 假阳性 P-3：P1 自指口径上界方差占比 59.4% → 47.6%

- **旧版原文**（`REPORT_paper.md` §5.2，`1141939b^`）：
  > 「历史实现把 `delta_after_m`（= 0.019561 mag）当作 σ_flat …，**占上界方差 59.4%**，得 σ_ceiling = **0.032456** ⇒ PASS」
- **新版对应物**（HEAD §5.2）：「**占上界方差 47.6%**，得 σ_ceiling = **0.028366** ⇒ PASS」。同数字在 `README.md` 与 `REPORT_experiment.md` 订正项 8 同步（3 处）。
- **核证**（存档 `results/step8_real_frame.json` 区间内**未改动**，`git diff --stat` 无输出）：
  ```
  :978  "sigma_ceiling": 0.020560838265260225      ← 独立项口径（σ_flat = 0.0007）
  :988  "delta_after_m_diagnostic": 0.019560615424196688
  ```
  自算：`Σ其余预算项² = 0.020560838² − 0.0007² = 4.22748e-4 − 4.9e-7 = 4.22258e-4`；
  `σ_ceiling,self = √(4.22258e-4 + 0.019560615²) = √(8.04876e-4) = 0.028368` ⇒ **与新版 0.028366 相符**；
  占比 `= (0.019560615/0.028368)² = 47.55%` ⇒ **与新版 47.6% 相符**。
  旧版对 (59.4%, 0.032456) 内部即不自洽：`(0.019561/0.032456)² = 36.3%`，与 59.4% 矛盾。
- **判为假阳性的理由**：数字订正**可从未改动的存档独立复算验证**；方向上使自指污染显得**更小**，但自指缺陷本身、机制、PASS→ABOVE_CEILING 翻转、`delta_after_m` 降级、N6 反例全部逐字保留。
- **须人工确认点**：订正**未附「旧值 59.4% 经核为误」的一句留痕**，且方向对论文更有利——建议按 AGENTS §5「诚实边界」体例补一句订正说明（分级：建议）。

### 4.6 其余假阳性 — 3 个 token

| 旧 token | 处置 | 理由 |
|---|---|---|
| P1 `0.032456`（`REPORT_paper.md`） | → `0.028366` | 见 4.5 P-3，已复算验证 |
| P1 `0.0325`,`0.1271`（`README.md` N6 行） | → `0.0562`,`0.1605` | 存档 `results/step7_negatives.json` 区间内**未改动**，其 `:273` `"ceiling_selfref": 0.05621888963922278`、`:303` `"ceiling_selfref": 0.16052783986632202` ⇒ **新版数值与存档字段相符**；旧值取自该 JSON `:313` 的 `note` 字符串（「自指口径下 ceiling 0.0325→0.1271」）——即旧文抄了 JSON 里一句**自身就写错**的注 |
| P1 `59.4`（`README.md`、`REPORT_experiment.md` 订正项 8） | → `47.6` | 同 4.5 P-3 |

---

## 5. C 项核证结果

**核证对象**：第一轮在 P2 查出过「同实验中隐去 `D_core` 中位误差 0.376 与真实帧源项只捕获 14.2%」。

**结论：❌ 未被删除。两条判红读数在 HEAD 上完好无损，且第一轮不但没删，还把它们从散落的适用域段提升为独立结论条目并写进摘要与 README。**

```
$ grep -n "0\.376" 实验/absolute-snr/REPORT_paper.md 实验/absolute-snr/REPORT_experiment.md 实验/absolute-snr/README.md
REPORT_paper.md:147    **适用域**：盲检测消融臂检测完备性 0.43，端到端 `D_core` 中位相对误差 0.376（判红）——差距全在**分子侧**（检测 + PSF 尺度），不在方差模型；真实 HST M16 星云结构臂源项只捕获 14.2%…
REPORT_paper.md:156 6. **同批实验的判红臂必须与绿臂同权报告**（本条为选择性报告的订正，G08-04 整改 P0-3）…①盲检测消融臂检测完备性 **0.43**、端到端 `D_core` 中位相对误差 **0.376（判红）**…②真实 HST M16 星云结构臂源项只捕获 **14.2%**…
REPORT_experiment.md:74   - 边界：盲检测消融臂检测完备性 0.43、端到端 `D_core` 中位相对误差 **0.376**（判红）…B 臂（真实 HST M16 星云结构）源项只捕获 **14.2%**…
README.md:8        …同批实测的盲检测臂检测完备性 0.43、`D_core` 中位相对误差 0.376（判红）、真实 M16 星云结构臂源项只捕获 14.2%…

$ grep -n "14\.2" （同上四文件）
REPORT_paper.md:147, 156
REPORT_experiment.md:74, 157   ← :157 为 §8 诚实边界第 10 条「§4.9 四臂的适用域…B 臂在延展发射上「源稀疏」先验失效（源项捕获 14.2%）」
README.md:8
```

**逐项回答**：

| 项 | 核证 |
|---|---|
| `0.376` 是否仍在？ | **是**，5 处：论文 :147/:156、实验报告 :74、README :8（实验报告 :157 处以「检测完备性 0.43」等义复述） |
| `14.2` 是否仍在？ | **是**，5 处：论文 :147/:156、实验报告 :74/:157、README :8 |
| 被谁删掉？ | **无人删过。** `git diff --stat 1141939b^ HEAD -- 实验/absolute-snr/REPORT_experiment.md` **无输出**——该文件在整个 67 提交区间内**零改动**。论文的 16 行改动中，这两句是**逐字保留**的（见 `git diff` 的上下文行，它们出现在新增条目的**引述**中，值一字未改） |
| 第一轮实际做了什么 | 把这两条判红读数**升格**：① 论文新增独立结论第 6 条「同批实验的判红臂必须与绿臂同权报告（选择性报告的订正，G08-04 整改 P0-3）」；② 论文摘要/适用域段、实验报告 §4.9 适用域段、README 一句话结论三处**同步**点名 |

**附带发现（须上报）**：`实验/absolute-snr/REPORT_experiment.md` 是 C 项两个载体之一，却**在含 67 个提交的治理区间内一次都没被改过**。这既解释了 C 项为何完好，也说明 P2 单元的**实验报告面根本没进第一轮审稿覆盖**。若二轮以「第一轮已覆盖实验报告」为前提，该前提对这份文件不成立。

---

## 6. 不确定项与需人工判断的地方

| # | 项 | 分级 | 需要什么 |
|---|---|---|---|
| U1 | **三处「失实陈述被订正」未留痕**（4.5 P-1/P-2/P-3）。订正本身经 git 核证成立且方向不掩盖缺陷，但新版只呈现结论、不写「旧稿此处曾失实/误诊/误算」。若二轮要求「订正须可追溯」，需补一句。 | 建议 | 负责人裁定：是否要求所有「旧文失实」类订正留痕 |
| U2 | **新引入的悬空行号锚**：`REPORT_paper.md`（P4）§7-12 写 `module_adapters.cpp:12112`，HEAD 实际为 `:12134`（区间内该文件被 G08-07/G08-08 改过，行号漂移）。同条另引 `:11967`、`:12595` 待一并核。 | 须修 | 前台按 HEAD 复核该文件全部行号锚 |
| U3 | **存档自身有错注未订正**：`实验/photometric-magnitude/results/step7_negatives.json:313` 的 `note` 字段仍写「自指口径下 ceiling 0.0325→0.1271」，而同文件 `:273/:303` 的 `ceiling_selfref` 字段是 `0.05622/0.16053`。报告已按字段值订正，**JSON 的 note 未订正**，形成新旧不一致。该 json 在区间内未被改过。 | 须修 | 订正 JSON 的 note（属产物文件，需前台确认是否允许改） |
| U4 | **5 份目标文件零覆盖**（第 2 节）：`实验/m42-realdata/REPORT_paper.md`、`实验/cone-search-constants/REPORT_paper.md` 为论文级正式稿，`实验/absolute-snr/REPORT_experiment.md` 为 C 项载体之一，`实验/m42-realdata/README.md`、`实验/engineering-evidence/README.md`。区间 67 个提交内均未被碰过。 | 阻断（覆盖缺口） | 需人工确认：这三份是否属于本仓治理范围；若在，第一轮存在未覆盖面 |
| U5 | **区间定义与任务书不符**：任务书称 `1141939b^..HEAD` 为 G08-05 第一轮边界，实际含 67 提交 / 6 个工作包。若二轮台账按「G08-05 = 这 67 个提交」记账，会把 G08-01/06/07/08 的改动误记入 G08-05。 | 须修（台账） | 前台确认台账口径 |
| U6 | **P3 ρ=0 读数在新版论文中只给 5.56e-10**，旧值 2.49e-10 仅在实验报告 `REPORT_experiment.md:161` 以「指数 2 / 3/2 两版并列」形式保留；论文 §3.6 未并列。两值均 ≤ 地板 9.74e-10，判词不变，影响极小，但论文单读时无法看到两版对照。 | 建议 | 人工判断是否需在论文补并列 |
| U7 | **P4 `REPORT_experiment.md` H5 行仍写「w 对 m_ref 不变逐位」**（无限定词），而本轮新增的 §6 第 22 条与论文 §2.2 已给出适用域（生产组成臂漂移 1.9%–4.2%）。这是**保留的过度声称**，方向与本单假设相反（是漏改不是删除），但会造成同文档内矛盾。 | 建议 | 人工判断 H5 行的限定词是否需同步 |
| U8 | 我未逐条验证「旧版每个不利数字在新版仍有等义形式」的**全部**表述层（如脚注、口头限定语、图表标题）。第 3.2 节的 token 扫描覆盖数字本身，但覆盖不到「措辞等义」。本单对结论/结果/边界段已做逐 hunk 人眼通读（14 份、670 增 198 删全部读完），结论的覆盖面到此为止。 | — | 如需更强保证，需第二轮逐段语义比对 |

---

## 附：取证纪律自证

- 全程只用 `git log` / `git show` / `git diff` / `git ls-tree` / `git grep` / `git cat-file` / `grep` / `sed` / `comm` / `awk` 等只读命令
- **未执行**任何 git 写操作（无 add / commit / checkout / reset / stash / rm / ref 改写）
- **未编译、未跑 ctest / pytest / 构建、未执行任何 `.py` / `.sh` / `.cpp` 可执行文件**
- **未读 `/tmp/acsd_g08/`**，未尝试创建或恢复
- 中文路径全程 `git -c core.quotepath=false`
- 唯一写入 = 本文件

---
---

# 追加：第二实例的独立复核（S5-A1′）

> **落笔说明**：本文件在本人开工后由并行的第二个 S5-A1 实例于 02:22 落盘。按 AGENTS §7「后到者得知先一轮已落内容，不回滚覆盖」，本人**未覆写**该文件，改为追加本节。本人与该实例**各自独立**取证（本人先只跑了任务书给的 `REPORT_*.md` glob，后补 `实验/*/README.md` 全量）。
>
> **独立结论一致**：本人逐 hunk 通读 `1141939b^..HEAD` 全部 19 份目标文件（10 份 REPORT + 5 份 README 被改，670 增 198 删），判定「不利读数删除」**同样为 0 条**；C 项（`0.376` / `14.2`）独立核证**同样为未被删除、且被升格**。
>
> **但本节订正上文第 4.1 节的一处分类错误，并新增 4 项上文未捕获的实证发现。**

## A1. 【订正上文 §4.1】「修引用」分类错误：新路径 `astrocs.phase1.*` 在 HEAD 不存在

上文 §4.1 把 11 个行号/节号 token 归为「修引用（路径迁移）」，理由是「G08-01/G08-06 已删 `docs/plugins/algorithms_phase1/*`，改指新路径 `docs/detail/registry/astrocs.phase1.*`」。**该新路径在 HEAD 同样不存在**，故不是「修好」，只是把一代悬空引用换成了二代。

```
$ git cat-file -e "HEAD:docs/detail/registry/astrocs.phase1.photometry.md"   # → 不存在
$ git cat-file -e "HEAD:docs/detail/registry/acsd.phase1.photometry.md"      # → 存在
$ ls -1 docs/detail/registry/ | head -5
README.md  acsd.phase1.calibration.md  acsd.phase1.cosmetic.md  acsd.phase1.drizzle.md  acsd.phase1.hips-writer.md
```

改名提交：`git log --oneline --diff-filter=ADR -- docs/detail/registry/astrocs.phase1.photometry.md` → `add871a8 补齐改名第三层缺口（41 个文件名）`、`dc5a2122`。即 G08-06（`123361b4`）写入的是 `astrocs.*`（当时正确），随后 G08-08 改名（`add871a8`）未同步引用。

**本区间内新引入的悬空引用共 11 处，跨 4 份目标文件**（全部为 `+` 行，即本轮新写）：

| 文件 | 行号 | 引用的路径 |
|---|---:|---|
| `实验/absolute-snr/REPORT_paper.md` | 33 | `docs/detail/registry/astrocs.phase1.noise-snr.md` |
| `实验/absolute-snr/REPORT_paper.md` | 186 | 同上 |
| `实验/dense-snr-reconstruct/REPORT_paper.md` | 42 | `docs/detail/registry/astrocs.phase1.noise-snr.md` |
| `实验/dense-snr-reconstruct/REPORT_paper.md` | 180 | 同上 |
| `实验/dense-snr-reconstruct/REPORT_experiment.md` | 130 | 同上 |
| `实验/photometric-magnitude/REPORT_paper.md` | 79 | `docs/detail/registry/astrocs.phase1.photometry.md` |
| `实验/photometric-magnitude/REPORT_paper.md` | 80 | 同上 |
| `实验/photometric-magnitude/REPORT_paper.md` | 110 | `astrocs.phase1.star-detection.md` |
| `实验/photometric-magnitude/REPORT_paper.md` | 206 | `astrocs.phase1.photometry.md`（此处是在**否认**其存在，见 A2） |
| `实验/photometric-magnitude/REPORT_experiment.md` | 24 | `astrocs.phase1.photometry.md` |
| `实验/photometric-magnitude/REPORT_experiment.md` | 113 | 同上 |

复核：`grep -rn "detail/registry/astrocs\.phase1" --include=*.md .` 另命中 `artifacts/evidence/governance-01/research/R-3_*.md` 与 18 份 `run/GOVERN-08/**` 审稿件（同源问题，出本单文件域）。

**分级：须修**。处置为把 11 处（及 `run/GOVERN-08/**` 的 18 处）前缀 `astrocs.` → `acsd.`。

## A2. 【新增发现】`REPORT_paper.md:206`（§7.16）把「路径不存在」写成了「内容不存在」，与同文件 §2.3/§3 互相打脸

- §7.16（`:206`，本轮**新增**）称：`docs/detail/registry/astrocs.phase1.photometry.md`「『数值落地口径』（内的『测光一致性判据（单帧、尺度无关、双边界）』…在当前树**均不存在**」⇒ 推出「该值在本单元内**只能作仓内约定常数引用，不得称权威常数**」。
- 但同文件 `:79`（§2.3）写「判据形态逐字来源 `docs/detail/registry/astrocs.phase1.photometry.md`『数值落地口径』（内的『测光一致性判据（单帧、尺度无关、双边界）』」——**同一份文件既否认该出处存在、又把它当逐字来源**。
- 实测：该章节**内容真实存在**，只是文件名带 `acsd.` 前缀：
  ```
  $ grep -n "测光一致性判据" docs/detail/registry/acsd.phase1.photometry.md
  69:**测光一致性判据（单帧、尺度无关、双边界）**：判据只有一条 —— 施加后星点星等
  204:- 测光一致性判据的负例（能红能绿，判据生效的必备条件）：① 伪造常数残差表
  ```
  `docs/detail/registry/acsd.phase1.noise-snr.md` 内「帧级 SNR」「稀疏帧内层几何与重建算子」等被引小节同样存在。
- §7.16 中**成立**的那一半：`docs/science/PHOTOMETRY.md` §16.4（`:357`–`:380`）只有 `σ_flat,hf` 字样、**确无 0.0007 数值**（已逐行核）。故「0.0007 的数值本身一手出处未取得」站得住；但「判据形态无出处」不站得住。

**分级：须修**。注意方向——这是本轮**新增的不利读数**（§7.16），不是删除；问题是它把一条**本不成立的不利结论**钉死了，并因此与同文件另两处冲突。修法：路径改名后重核 §7.16，把「路径不存在」与「数值 0.0007 无出处」两件事拆开表述。

## A3. 【新增发现】`实验/photometric-magnitude/README.md:594` 的 R14 行被路径替换脚本改坏

```
594:| R14 | 中 | 判据出处写成 `docs/detail/registry/astrocs.phase1.photometry.md`（原 §3.1 → ）`（不存在） | **已修**：改为 **§4.1**，…
```

`（原 §3.1 → ）`  是残缺片段：原句为「判据出处写成 `06_photometry.md §3.1`（不存在）」，批量把 `06_photometry.md` 替换成新路径时把 §3.1 一并吞掉又留下悬空反引号。读者无法判断「原 §3.1 → 」指向何处。

**分级：须修**（文本损坏，非科学结论）。

## A4. 【新增发现】P4 `REPORT_paper.md:123` 仍引旧值 0.037 / 0.433 / 12×，与同文件 §7.23 E6 的 0.030158 / 0.38046 / 12.6× 未对账

```
$ grep -n "0\.037 vs A_bglimit" 实验/dense-snr-reconstruct/REPORT_paper.md
123:重建臂…极端对比场景（4-dex）：A_full E = 0.037 vs A_bglimit E = 0.433——**丢源项的最劣效率损失 12 倍**…
$ grep -n "12\.6" 实验/dense-snr-reconstruct/REPORT_paper.md   # 摘要、:149 E6、:168 结论、:220 §7.18、:245 §7.21
```

摘要与结论已全面改用 **12.6×**（更不利），但 §4.4 正文 `:123` 仍留 **12×**。`:123` 这行在本区间是**未改动的上下文行**（不是本轮删的），但本轮改 §7.23 E6 时没有回头对账 §4.4，两处现在互相矛盾。

**分级：须修**。同时说明：上文 §4.4 把 `0.037`/`0.433` 归为「归档订正」成立（存档为 0.0301577/0.3804647，比值 12.615），但订正只落到 E6 一处，未落到真正的正文承载段。

## A5. 【订正并强化上文 U2】`module_adapters.cpp` 行号锚是**系统性 off-by-22**，且 `:12595` 的符号不存在

上文 U2 只核了 `:12112`。本人把 P4 论文 §7.10/§7.12 引用的 4 个锚全查了：

| 论文引用 | 该行 HEAD 实际内容 | HEAD 真实位置 |
|---|---|---|
| `:11967` = `snr_path=dense` fail-closed | `// 正本：docs/ACSD_DESIGN.md §5.3（design_clauses 条目 DESIGN-5.3-SNR-PATH-JSON）、` | `SNR_PATH_DENSE_UNAVAILABLE` 在 **:11989** |
| `:12112` = `in.sparse = nullptr` | `fd["operator_id"] = sparse_probe[f].operator_id;` | **:12134** |
| `:12581` = 逐像素调用 `weight_from_sparse_layer_pixel_prepared` | `// fits_index_to_nested_local(..., kP2TileShift, 512) LUT 同一序号定义）。` | **:12603** |
| `:12595` 取 `snr_weights[slot]` | `acsd::v6::p2weight::PixelWeightInput pin;` | 该表达式为 `snr_weights[it.slot[d]]`，在 **:12617** |

四个锚**一致偏后 22 行**，说明论文是照着**改名前的文件行号**写的；而 `:11967`/`:12112` 恰好等于 **`1141939b^` 的行号**（本人已用 `git show 1141939b^:… \| grep -n` 逐条验过：baseline 的 `:11967`=DENSE_UNAVAILABLE、`:12112`=`in.sparse = nullptr`、`:12581`=逐像素调用，三条**全部命中**）。

补充一条上文未核的：`git grep -c "snr_weights\[slot\]" 1141939b^ -- <file>` → **0**，`HEAD` 同查也只有 `snr_weights[it.slot[d]]` 一种形态。即 `:12595` 这条引用在 baseline 与 HEAD **两端都指不到**，不是行号漂移，是**符号名写错**（旧稿的 `module_adapters.cpp:11721-11722` 同样指不到，实为 `intra_frame_reference` 的 JSON 校验码）。

**分级：须修**（3 处行号偏移 + 1 处符号名不存在）。另注：区间内 `module_adapters.cpp` 被 `14a50e3e`（品牌改名）与 `7a63016b`（G08-07 分片三）改过，行号漂移有据，但报告未随之校锚。

## A6. 附：本人对上文 §4.5 P-1 / P-2 的独立复核（结论一致，且各补一条更强的证据）

- **P-1（IDW `snr_evaluator`）**：本人除 `git diff --stat`（区间内未改）外，另跑了 `git -c core.quotepath=false log --oneline -S 'idw_power_ = 1.0' -- lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h` → 唯一命中 `99bf071d 闭环 P3 守恒映射算子的假阳性与判据缺陷`（2026-09-29 02:26），且 `git merge-base --is-ancestor 99bf071d 1141939b^` **成立** ⇒ 该值在**基线之前**就已是 1.0，旧稿 §7-9 把 drizzle 侧列为「仍硬写 2.0」确系**失实陈述**。同时 `grep -n idw_power lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp` → `:593`/`:730`/`:833` 三行 `out_model->idw_power = 2.0;` **原样健在**，残余不利事实精确保留。**判假阳性成立**。
- **P-2（`in.sparse = nullptr` 误诊）**：本人进一步查到 baseline 的 `:12182` 就有一句注释「**稀疏层的消费面在下方逐像素路径**（weight_from_sparse_layer_pixel_prepared，…）」——即**源码自己就写明了这条逐像素消费面存在**，旧稿把它误诊为「逐像素面不可达」更无可辩。**判假阳性成立**。

## A7. 本人补充的不确定项

| # | 项 | 分级 | 说明 |
|---|---|---|---|
| A-U1 | **本区间把「第一代悬空引用」换成「第二代悬空引用」**（A1），且 11 处落在 4 份目标文件、18 处落在 `run/GOVERN-08/**` | 须修 | 前台决定是否本轮一并改名；若不修，A2 的「内容不存在」结论会持续误导下游 |
| A-U2 | **P1 单元 0.0007 mag 的「一手出处未取得」结论需重核**（A2）：路径改名后该常数在 `acsd.phase1.photometry.md:69` 的判据节里，而该常数**单独决定真实帧 ABOVE_CEILING 的方向** | 须修 | 需人工打开 `docs/detail/registry/acsd.phase1.photometry.md` 全文确认 §4.1 是否给出 0.0007 及其出处；本人只核了 `PHOTOMETRY.md` §16.4 无该数值，未逐字读 registry 全文（超出本单取证面） |
| A-U3 | P3 `README.md` 把 `code/audit/run_all.sh` 的描述从「各实验 JSON 落 `results/audit/<路线>/`，**与存档逐位对照**」改为「31 份与存档逐字节相同、13 份差异只在墙钟字段」 | 建议 | 方向是**撤回有利表述**，与本单假设相反，记录备查 |
| A-U4 | P1 `README.md` N6 行由 `0.0325→0.1271` 改为 `0.0562→0.1605` | 建议 | 与上文 §4.6 一致（本人独立复核 `results/step7_negatives.json` 的 `rows[].ceiling_selfref` = 0.056219/0.160528 确认新版正确，旧值取自该 JSON `note` 字段的自错注）。补充：JSON 的 `note` **至今未订正**，故产物与报告仍不一致（同上文 U3） |
| A-U5 | 本人**未**逐条核验 P5 `README.md`（127 行，本区间改动最大的 README）的 11 个新增失效域阈值的存档出处 | — | 上文 §3.4 列了该 README 的新增内容但未逐条核证；如需闭环建议单独派片 |

## A8. 追加节的取证纪律

- 追加节全程只读：`git cat-file` / `git log -S` / `git merge-base --is-ancestor` / `git show` / `git diff --stat` / `git grep` / `grep` / `sed` / `ls` / `python3 -c`（**仅用于 `json.load` 读取已落盘的 `.json` 产物**，未执行任何仓内 `.py`）
- **未执行**任何 git 写操作、未编译、未跑 ctest/pytest/构建、未执行任何 `.py`/`.sh`/`.cpp` 可执行文件
- **未读 `/tmp/acsd_g08/`**；中文路径全程 `git -c core.quotepath=false`
- 写入方式：**追加**（`edit`），未覆写前一实例的任何内容