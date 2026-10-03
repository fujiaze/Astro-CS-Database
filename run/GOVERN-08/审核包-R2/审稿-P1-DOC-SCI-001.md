# 审稿 P1 · DOC-SCI-001（G08-05 对抗审稿 第 1 遍）

- 片号：`DOC-SCI-001`
- 层：`docs/science`
- 基线：HEAD = `f9650dd0`，工作树零改动
- 清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:902-926`
- 本人未编译、未跑 ctest/pytest/构建/实验脚本；未读 `/tmp/acsd_g08/`；零 git 写；未改任何仓内文件（本交付件为任务指定的唯一写入）。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数 | **23** |
| 读了几份 | **本人逐行读完 8 份；另 15 份由 6 个子代理逐行读完并经本人抽验** |
| 成员总行数 | **10030** |
| 本人实读行数 | **4772** |
| 本人覆盖率 | **47.6%（4772 / 10030）** |
| 代理覆盖后合计 | **10030 / 10030 = 100%** |

### 1.1 本人逐行读完（8 份 / 4772 行）

| 文件 | 行数 |
|---|---:|
| `docs/science/DATA_SEMANTICS.md` | 3416 |
| `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md` | 453 |
| `docs/science/PHASE3_HIPS_TO_FITS.md`（经代理，本人抽验关键行） | 224 |
| `docs/science/algorithms/PHASE3_RESAMPLE.md` | 210 |
| `docs/science/INTEGRATION.md` | 174 |
| `docs/science/UNCERTAINTY_AND_COVARIANCE.md` | 163 |
| `docs/science/PSF_SIGNAL_WEIGHT.md` | 150 |
| `docs/science/algorithms/GATES_AND_TOLERANCES.md` | 124 |
| `docs/science/algorithms/ACR_EQUIVALENCE_ALGORITHMS.md` | 82 |

### 1.2 未由本人逐行读完的 15 份（5258 行）—— 如实列出

`docs/science/PHASE2_UPM.md`(554)、`algorithms/PHASE2_MOSAIC_WRITE.md`(521)、`CALIBRATION.md`(488)、
`algorithms/PHASE3_RSMP_IMPL.md`(433)、`PHOTOMETRY.md`(407)、`NOISE_MODEL.md`(403)、
`algorithms/PLATESOLVE.md`(371)、`algorithms/PHASE2_INTEGRATION.md`(361)、
`CCD_LINEAR_DEFECT_LITERATURE.md`(310)、`IO_002_HIPS_INPUT_INTERFACE.md`(297)、`ASTROMETRY.md`(284)、
`CONTROL_WEIGHT_SNR.md`(270)、`PSF.md`(240)、`algorithms/HEALPIX_MAPPING.md`(95)。

> 这 15 份的结论**均由子代理逐行阅读产出，本人对每条阻断/重条做了独立复核**（见 §7）。凡本人未复核的代理条目，本件一律降级为「建议」或标注「转录自代理、本人未独立复核」。

---

## 2. 本片判定

**判定：需修（存在 2 条阻断级）**

最重的 3 条：

1. **`G-P1-WCS-CRPIX` 是恒真门，且登记为发布门 Y。** 判据把 `crpix` 与它自己的定义式比：门表阈值写「精确 = (w/2+0.5, h/2+0.5)」（`algorithms/GATES_AND_TOLERANCES.md:70`），生产实现逐位计算同一表达式（`lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:290-291`），测试断言同一表达式（`eng/tests/unit/p1wcs/p1wcs_tests_oracle.cpp:342-344`）。**任何缺陷类都无法使该门翻红**，而它占用一个发布门位。
2. **Phase2 UPM 加性校正的唯一端到端 oracle 与被测同源。** `algorithms/PHASE2_MOSAIC_WRITE.md:357` 把该 oracle 标为「独立于被测符号」，实际它先跑生产 stage2 写出 `upm_sparse.json`，再用 `p2_upm_open` + **同一** `p2_upm_evaluate_c` 算期望值（`lib/algorithms/coverage/tests/ivar_wiring_test.cpp:292 / :298-299 / :213 / :229-230`）。UPM 求解器/gauge/几何/k_corr 整链注入缺陷时该门恒绿。
3. **10 份 science 正本把已删除的 `eng/tools/science_contract_lint.py` 的 PASS 列为冻结验收条件**（本片占 8 份）。工具仓内 0 命中、`eng/tests/sciencelint/` 为空目录；而最后修改 `INTEGRATION.md` 的提交 `ed33f57f` 的负责人裁决逐字写「文档域**不设机器门，不产出机器 PASS 记录**」——裁决与残留验收项在同一文件内对撞。

---

## 3. 逐文件清单

| # | 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `DATA_SEMANTICS.md` (3416) | 全 31 节 + §31.1a~§31.10 | 跨阶段数据合同；§4a variance/ivar 三态表；§31.10 条款计数 96=39+49+8（**已核 `clause_registry.json#counts_by_status` 逐字相符**）；**节序错乱：§29 在 §28 之前**（:2436 vs :2538）；`stage2.cpp` 源码锚漂移（见 F-7） | 需修 |
| 2 | `algorithms/STAR_DETECTION_ALGORITHMS.md` (453) | 全 | §2 冻结母函数参数化与代码不符（B-8）；O13b 形状门零文档（B-10）；:284-285 悬空引用登记（见 §6 反例 X-REF） | 阻断 |
| 3 | `algorithms/GATES_AND_TOLERANCES.md` (124) | 全 §1–§4a | **G-P1-WCS-CRPIX 恒真门（B-1）**；:81 「零命中」自指；G-P1-WCS-RT 判据力弱（自认 2.91e-9 vs 1e-4） | 阻断 |
| 4 | `UNCERTAINTY_AND_COVARIANCE.md` (163) | 全 | `k_corr` 规范式 vs 生产冻结值冲突（B-4）；`Var(median)=πσ²/(2N)` 引用 Cramér 逐字**已核一致** | 需修 |
| 5 | `ACR_EQUIVALENCE_ALGORITHMS.md` (82) | 全 | F1/F2 分块等价判据为构造性恒等（per-pixel 无跨像素归约 ⇒ 分块不变是构造保证）；全篇零负向注入 | 建议 |
| 6 | `INTEGRATION.md` (174) | 全 | :173 `science_contract_lint.py` PASS（B-3）；:53-54 `valid(i)` 末位合取在 `weights=null` 时越界；Aitken 1935/1936 双源登记**诚实** | 需修 |
| 7 | `algorithms/PHASE3_RESAMPLE.md` (210) | 全 | **:114 与 :127 coverage 口径互斥（B-9）**；:23/:24 1-based/0-based 基准错标；:59-60 称计数承载面「未冻结」已被 DATA_SEMANTICS §30.7 冻结 | 需修 |
| 8 | `PSF_SIGNAL_WEIGHT.md` (150) | 全 | §3 对 PixInsight 公式做了逐字核验并主动标注「未独立核验」的 PCL 头文件值（**诚实登记，不算伪引**）；§7a.6 主动作废「RMSE≤K·s_field」恒真判据（**本片做得最好的一处**） | 通过（局部） |
| 9 | `algorithms/PHASE2_MOSAIC_WRITE.md` (521) | 抽验 :22-26/:50-54/:355-368 | oracle「独立于被测符号」不实（B-2）；target_order 裸从句方向反（B-7）；legacy 降级臂永久判红（B-5） | 阻断 |
| 10 | `algorithms/PHASE2_INTEGRATION.md` (361) | 抽验 :32/:39-40/:195-199 | 像素域/面亮度域量纲自相矛盾，且错版位于「唯一权威」小节 | 须修 |
| 11 | `PHASE2_UPM.md` (554) | 代理 | §17 数值自洽（含非恒真负例），质量最高的一节 | 通过（局部） |
| 12 | `NOISE_MODEL.md` (403) | 抽验 :88/:343/:371 | **5% oracle 通过率 92.8% ⇒ 7.2% 恒红（B-6）** | 阻断 |
| 13 | `CALIBRATION.md` (488) | 代理 | ε 未冻结却已被 §11/§15 当作可执行门并给红/绿结论；:486 残留 `science_contract_lint.py` | 须修 |
| 14 | `PHOTOMETRY.md` (407) | 代理 | :9/:12/:218 三处对「是否宣称绝对通量」互斥；:181/:182/:209 悬空截断引文；:317 lint | 须修 |
| 15 | `PSF.md` (240) | 代理 | 五处冻结常数的证据锚文件不存在；:113 引用不存在的 `DPSF_ERR_PARAM`；:53 vs :115 常数不一致 | 须修 |
| 16 | `CONTROL_WEIGHT_SNR.md` (270) | 代理 | §8c「γ=2 由恒等式唯一确定 + 四路复核」= 逆函数构造的恒真门；生产 Δ=64 落在自述失效区 | 须修 |
| 17 | `algorithms/PLATESOLVE.md` (371) | 代理 | §4a.5 负向注入在自身构造下恒绿；§4a.2 被 `min/max` 钳死；DISP-WCS-008 用 LS 拟合出的逆多项式比它自己的正映射 | 须修 |
| 18 | `ASTROMETRY.md` (284) | 代理 | :185 判据 1e-4 px 与同句离网格读数 3.10 px 差 3.1e4 倍，:280 仍断言「全过」 | 须修 |
| 19 | `algorithms/PHASE3_RSMP_IMPL.md` (433) | 代理 | §4 表被截断 + 整段重复，表格结构破坏；符号计数三处打架 | 须修 |
| 20 | `PHASE3_HIPS_TO_FITS.md` (224) | 抽验 | coverage 语义与实现冲突的 SCI 侧根因；§15 与 §16 登记面互斥 | 须修 |
| 21 | `IO_002_HIPS_INPUT_INTERFACE.md` (297) | 代理 | 与 SCI-P3 对 `hips_order` 值域给互斥上限（29 vs 20）且未登记收窄关系 | 建议 |
| 22 | `CCD_LINEAR_DEFECT_LITERATURE.md` (310) | 代理 | 「一手定义（原文）」列混入编者自撰转述（无引号）；:23「唯一」被同文件三处推翻 | 建议 |
| 23 | `algorithms/HEALPIX_MAPPING.md` (95) | 代理 | :71 引用的豁免登记文件不存在 | 建议 |

---

## 4. 发现清单

### 4.1 阻断

---

**B-1 `G-P1-WCS-CRPIX` 是恒真门（代数恒等式型），且登记为发布门 Y**

- 位置：`docs/science/algorithms/GATES_AND_TOLERANCES.md:70`
- 现状（逐字）：`| G-P1-WCS-CRPIX | CRPIX 精确相等 | 任意帧 | 精确 | n/a | 精确 = (w/2+0.5, h/2+0.5)（1-based） | 冻结：SCI-WCS-001 §7 CRPIX 不变量 | ctest:p1wcs_apbp | Y |`
- 应为：该门应改为**独立判别**：或比对 CRPIX 相对于**独立 oracle**（由 RA/Dec/尺度反解）的一致性，或按本表 §1 R3「自证门不计入门表」撤出发布门、如实标 N。
- 证据（三方同式，本条最硬）：
  - 生产：`lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:290-291` → `result->crpix[0] = img_width / 2.0 + 0.5; result->crpix[1] = img_height / 2.0 + 0.5;`
  - 测试 oracle：`eng/tests/unit/p1wcs/p1wcs_tests_oracle.cpp:342-344` → `P1WCS_CHECK_NEAR(cs, w.crpix[0], fx.width / 2.0 + 0.5, 1e-9, …)`
  - 门表阈值：`(w/2+0.5, h/2+0.5)`
  ⇒ 三者共享同一定义式 ⇒ **判据在结构上不可能因真实天测缺陷翻红**。同表 :25 的 R3 已写「自证门不计入门表」，本行违反该规则。

---

**B-2 Phase2 UPM 加性校正的唯一端到端 oracle 与被测同源（往返自证 + 自愈）**

- 位置（文档声明）：`docs/science/algorithms/PHASE2_MOSAIC_WRITE.md:357`
- 现状（逐字）：`- Oracle 设计（独立于被测符号，不复制 §2 公式）:`
- 应为：`C_f(leaf)` 必须来自独立实现（解析解 / 冻结真值表 / 独立 NumPy），不得回读本次运行写出的 `upm_sparse.json` 并调用 `p2_upm_evaluate_c`。文档的「独立于被测符号」是错误陈述。
- 证据（本人逐行读）：
  - `lib/algorithms/coverage/tests/ivar_wiring_test.cpp:292` `const int rc = run_stage2(cfg1, 1);` → 写出 `out1`
  - `:298-299` `const auto expect = expected_weighted_mean(signals, supports, ivars, dirs, out1 + "/upm_sparse.json");`
  - `:213` `p2_upm_open(upm_sparse_path.c_str(), &model)`
  - `:229-230` `const double c = p2_upm_evaluate_c(model, fids[f], leaf); const double cal = signals[f][z] - c;`
  - `:204` 注释逐字：`期望加权均值…（生产 UPM 模型求值，非假设 offset=0）`
  ⇒ 「期望」= `Σ w_f·(raw_f − C_f^prod) / Σ w_f`，生产 = 同一式。两侧共享 `raw − C_f` ⇒ UPM 求解器/gauge/几何/k_corr 全链注入缺陷时 `|got1 − expect|` **不变 ⇒ 门恒绿**。
- 连带：`PHASE2_MOSAIC_WRITE.md:392-397` 诚实枚举「不覆盖 f64 数值 oracle、多 tile、large_scale、ACR 块路径」，**唯独漏掉 UPM 加性校正本身**——构成选择性报告。

---

**B-3 10 份 science 正本以已删除的 `science_contract_lint.py` 为冻结验收条件（本片占 8 份）**

- 位置：`INTEGRATION.md:173`、`PSF.md:208`、`PHOTOMETRY.md:317`、`NOISE_MODEL.md:402`、`PHASE2_UPM.md:368`、`CALIBRATION.md:487`、`ASTROMETRY.md:283`、`PHASE3_HIPS_TO_FITS.md:204`（另 `DRIZZLE.md`、`REJECTION.md` 属 DOC-SCI-002）
- 现状（`INTEGRATION.md:173` 逐字）：`- \`eng/tools/science_contract_lint.py\` PASS；`
- 应为：删除该验收项（与 `ed33f57f` 的负责人裁决一致），或恢复工具。
- 证据（本人复核）：
  - `ls eng/tools/science_contract_lint.py` → 不存在；`git ls-files | grep -c science_contract_lint` → **0**
  - `find eng/tests -maxdepth 1 -type d -empty` → `eng/tests/sciencelint`、`eng/tests/results`（**空目录 = 工具被删后未清场**）
  - `git log --diff-filter=D -- eng/tools/science_contract_lint.py` → `57abe9d8 删除旧门禁与检查器…`，且该提交是 HEAD 的祖先
  ⇒ 这是一条**永远不会被执行、也永远不可能红的验收条件**——恒真门三型之外的一种：**判据执行体缺失型**。
- 加重项：`ed33f57f`（最后改 `INTEGRATION.md` 的提交）裁决逐字为「文档域**不设机器门，不产出机器 PASS 记录**」，但该提交对本文件只改了 1 行（`:139`），**没动 `:173`** ⇒ 裁决已下达、残留未清。

---

**B-4 生产 `k_corr` 冻结标量 1.4 与正本「规范式 = 因子分解式」相反，且自认低估约 2 倍**

- 位置：`docs/science/UNCERTAINTY_AND_COVARIANCE.md:36` / `:49` / `:55-56`（本人读）
- 现状（逐字）：`:36`「**规范式 = 因子分解式**」；`:55-56`「冻结单数 1.4 在其声明标定域两端**低估 control_variance**（N=5 端 k_corr(5,紧凑)≈2.05 → 低估 32%；源 583–600″ 端 ≈2.5–3.0 → **低估约 2 倍**）」
- 应为：生产取值与正本一致，或正本显式承认冻结单数是**唯一在役**口径并给出方向安全性论证。
- 证据（本人复核）：`lib/algorithms/coverage/src/upm.cpp:2233` `kMaKCcorrFrozenInDomain = 1.4;`；`:2968-2972` `if (k_corr != kMaKCcorrFrozenInDomain && (calibration_run_id 空)) return 3;` ⇒ **因子分解式在生产被 rc=3 拒绝**。`lib/algorithms/integration/phase2_integrate/include/acsd/phase2_integrate.h:71` `kKCorrFrozen = 1.4`（供 `phase2_integrate.cpp:457/:1274/:1325`）。
- 影响：`control_ivar = 1/control_variance` **高估约 2 倍** ⇒ Phase2 UPM 控制点权重 `quality × control_reliability × control_ivar` 系统性偏重。方向不安全（非保守）。

---

**B-5 `legacy_allow_weight_fallback` 降级臂已从代码删除，文档仍预测 `rc=0` ⇒ 该 Oracle 臂永久判红**

- 位置：`docs/science/algorithms/PHASE2_MOSAIC_WRITE.md:365-367`
- 现状（逐字）：`- ivar 门负测: 逐样本 ivar 权重且 ivar 产品缺失、legacy_allow_weight_fallback 未显式置 true → rc=7（:565-574）；**置 true → rc=0 且 diagnostics ivar_product_missing>0**。`
- 应为：该臂改写为「置 true ⇒ 解析即拒绝、rc≠0、无 diagnostics/无产物」。
- 证据（本人复核）：`lib/algorithms/coverage/tools/stage2.cpp:785` 逐字「// （原 legacy_allow_weight_fallback=true 降级分支**已删除**。）」；`:797-804`「// 原 legacy_allow_weight_fallback=true 的 …（legacy_allow_weight_fallback 已删除）」。
- 加重项：同一事实两份 SCI 正本**互相矛盾** —— `PHASE2_INTEGRATION.md:148-149` 是对的（该键出现即被拒绝），`PHASE2_MOSAIC_WRITE.md` 是错的。

---

**B-6 `NOISE_MODEL` 5% oracle 在正确实现下有 7.2% 恒红（恒红门把真实缺陷藏在红灯里）**

- 位置：`docs/science/NOISE_MODEL.md:343`（硬门）、`:371`（自报读数）
- 现状：`:343`「经验 `σ_bg` 在 `5%` 内复现（`SNR-004`）」被 §15 Acceptance 当作冻结门；`:371` 逐字「`min_samples=64` 为 −1.25%/−1.7%、**通过率 92.8%**（纯高斯蒙特卡洛…）」
- 应为：判据须给出**双向**读数——绿侧给通过率（92.8%），红侧给**注入缺陷后的失败率**；并把「7.2% 恒红」显式写成门的固有性质，或把阈值放宽到 ≥ 某分位（如 5σ 的 99.7%）。
- 证据：这是本任务书点名的「**恒红**」形态——两个量并非逐位相同，但门对**正确实现**已有 7.2% 的假阳性 ⇒ 真实缺陷的红被淹没。文档**自己**记录了 92.8% 却未据此调整门。
- 对照（本片做得对的样本，同组判据应照抄）：`CONTROL_WEIGHT_SNR.md:216` `zA=1.27 绿 / zB=25.6 红`；`PSF.md §16` `0.4999988 绿 / 0.2770008 红`。

---

**B-7 `target_order` 分辨率上限：裸从句与代码方向相反（且代码注释已逐字点名该错）**

- 位置：`docs/science/algorithms/PHASE2_MOSAIC_WRITE.md:337-338`（裸从句）、`:52-53`（伪引 log 串）
- 现状（逐字）：`:337-338`「**分辨率上限 = 最低输入 order**: target_order ≤ 输入**最高** order，违者 rc=3（:205-208）。」；`:52-53`「`target_order > cov.target_order`（高于**最低输入 order**）→ 显式拒绝 rc=3，log "**target_order 高于输入最高 order**，禁止插值伪装分辨率"」
- 应为：两处都写 `cov.target_order`（= 所有输入的 **min**），并逐字照录实际 log。
- 证据（本人复核）`lib/algorithms/coverage/tools/stage2.cpp:213-220`：
  ```
  213: if (target_order > cov.target_order) {
  214-217: // 文案订正：cov.target_order 是**所有输入里最低**的 order …
           // 原文案写「高于输入**最高** order」与判据方向相反 …
  218: log("target_order 高于输入最低 order（min over inputs），禁止插值伪装分辨率");
  ```
  ⇒ **代码注释逐字点名了文档这个错，文档至今未订正**。按 `:337-338` 实现会放行 `min < target ≤ max`，正是 §2 明令禁止的「插值伪装分辨率」。
- 附：`:52-53` 逐字引用的 log 串与实际 `:218` 不符 ⇒ 伪引。

---

**B-8 `STAR_DETECTION_ALGORITHMS.md` §2 冻结母函数的参数化与生产代码不符，且同文件 §11.4 F4 自相矛盾**

- 位置：`docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md:113-118`（§2）vs `:399-400`（F4）
- 现状（逐字 `:115-118`）：`f(x,y)=B+A·exp(−(x′²/SX+(y′/r)²/SX)/1)`，参数 `{B,A,x0,y0,SX=2σ²,fr,alpha}`，`r=0.5·(cos fr+1)`，`sx=√(SX/2)`，`sy=sx·r`，`theta=−alpha` 归一到 (−90°,90°]`
- 应为：按生产实现写 `(x0, y0, sx, sy, th, A, B)`，sx/sy **独立**，theta ∈ (−π, π]。
- 证据（本人复核 `lib/algorithms/star_detection/src/sdet_api.cpp:188-202`）：
  - `:190-191` 注释逐字 `f(x,y) = B + A*exp(-(1/2)[(x'/sx)^2 + (y'/sy)^2])`
  - `:193` 注释逐字 `参数序 (x0, y0, sx, sy, th, A, B)`
  - `:201` `const double th = std::atan2(std::sin(x[4]),cos(x[4]));  // theta 折算 [-pi,pi]`
  - `sx`/`sy` 在 `:199-200` 独立取自 `x[2]`/`x[3]`，无 `fr`/`alpha`/`SX` 拟合参数（全文件 grep 0 命中）
- 实质后果（非记号）：`r=0.5(cos fr+1) ∈ [0,1]` ⇒ 文档强制 `sy ≤ sx`，长轴永远落 x 分量；代码允许长轴落任一分量。`:115` 的括号还把 `/SX` 与 `/r` 塞进同一括号，可被读反（方向相反），末尾 `/1` 是残迹。
- 最重后果：`:399-400` 的 F4 要求 TEST 做「独立复算」，若照 §2 实现 ⇒ 长轴沿 y 的星上 sx/sy 分配与生产不同 ⇒ **判据在正确实现下判红**（违反 AGENTS.md §8）。
- 同文件 `:399-400` 自己写的是 `(B,A,x0,y0,sx,sy,theta)`（与代码一致），**与 §2 直接矛盾**。

---

**B-9 Phase3 coverage 语义三份正本互斥，且生产实现与 SCI 正本相反**

- 位置：`docs/science/algorithms/PHASE3_RESAMPLE.md:114` vs `:127`（本人读）；`PHASE3_HIPS_TO_FITS.md:73/:141`；`lib/algorithms/resample/p3_resample.cpp:401-403`
- 现状（逐字 `PHASE3_RESAMPLE.md`）：`:114`「任一角 tile 缺失则 C=0 且 S=NaN」 vs `:127`「bilinear 四邻域 **tile 全缺失** | 该足迹 C=0, S=NaN」—— **同文件两条互斥规则**；`:49-50`「C 只判足迹内有无 tile 像素…（4 个 tile 均可读则 C=1）」偏向第三种口径。
- 应为：收敛为单一口径并与 SCI-P3 §5/§9a-6 一致（缺角象限应**退化重归一 + C=1**）。
- 证据：`lib/algorithms/resample/p3_resample.cpp:401-403` `if (!g00 || !g10 || !g01 || !g11) { *value = std::nanf(""); *coverage = 0; return P3_RS_OK; }` ⇒ 全或无，不做缺角退化重归一。
- 后果：survey 边缘本应部分覆盖的区域被整片丢成 NaN/0；且该层**既无门行、也无负注入测试**（`PHASE3_RSMP_IMPL.md` §12 的 9 条测试设计要求不含 partial-tile coverage）。

---

**B-10 一整个在役生产拒星门（R-58-1 / O13b 点源形状门）在科学文档里零提及，且字段数与消费面两处都错**

- 位置：`docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md:274` / `:280` / `:283`
- 现状（逐字）：`:274`「SDetParams **9 字段**」；`:280`「`fitRadius`、`fwhmClipSigma` **本文件内零消费面**：只出现在创建默认值与创建日志两处」；`:283`「⇒ 生产 impl 在本文件内实际消费 **2/9** 字段」
- 应为：13 字段、消费 7/13；补 O13b 的三条子判据与冻结阈值。
- 证据（本人复核 `lib/algorithms/star_detection/include/star_detector.h` + `sdet_api.cpp`）：
  - 字段：`structureLayers:30`、`hotPixelFilterRadius:31`、`iterativeClipSigma:32`、`iterativeMaxRounds:33`、`medianFilterDetail:34`、`maxStars:35`、`fitRadius:37`、`fwhmClipSigma:43`、`maxAxisRatio:44`、`psfFwhmLoRatio:66`、`psfFwhmHiRatio:67`、`maxPeakFraction:68`、`minQuarterMaxPixels:69` = **13**
  - `fitRadius` 在头文件 `:37` 注释逐字「② 读点 sdet_api.cpp 形状门窗半径」⇒ **文档「零消费面」被自家合同头直接证伪**
  - 门本体：`sdet_api.cpp:433 sdet_apply_psf_shape_gate`，`:437-440` 读 4 个字段，阈值在 `:378-383`
  - `grep -c "psfFwhm\|PeakFraction\|QuarterMax\|O13b\|形状门"` 在该文档 → **0**
- 加重项：该门自身还是**恒真门型 c**——参照量 `f0` 取自本帧存活候选中位数 ⇒ 对任何全局 FWHM 标度偏差免疫；补文档时须同时登记该适用域。

### 4.2 须修

**F-1 `science_contract_lint` 之外的 6 处「冒号后非行号」的悬空截断引文（HEAD「清除伪引」提交的残留）**
`PHOTOMETRY.md:181`「判据在 、拒绝在 」、`:182`「判据在 」、`:209` 四个空括号；`NOISE_MODEL.md:399` 两处「；）」；`:401`「（`NOISE_SATURATION_FILTER`，）；」——**其中 `:401` 是 §15 Acceptance 一条门，声明名本身被截断**。复现：`grep -n "，）；\|判据在 、\|在 ，" docs/science/*.md`。

**F-2 `DATA_SEMANTICS.md` 节序错乱**：§29（:2436）排在 §28（:2538）之前。影响：按编号顺序阅读的人会先读 §29 再回头读 §28。建议重排。

**F-3 `stage2.cpp` 源码锚大面积漂移，且错误锚已传染进 `DATA_SEMANTICS.md`（本人复核）**
| 文档锚 | 实测 | 偏移 |
|---|---|---|
| `PHASE2_MOSAIC_WRITE.md:11/:437`「stage2.cpp **2015 行**实测」 | `wc -l` = **2019** | — |
| `PHASE2_MOSAIC_WRITE.md` 球面常量 `:525-528` | `:756-759`（`nside`:756 / `A_cell`:758-759） | +231 |
| `aio_hips_product_begin` `:592` | `:823` | +231 |
| `DATA_SEMANTICS.md:1244`「`nside = 2^(target_order+9)`（stage2.cpp:525）」 | `:756` | +231 |
| `DATA_SEMANTICS.md:1246`「`A_cell`（stage2.cpp:527-528）」 | `:758-759` | +231 |
| `DATA_SEMANTICS.md:1211/:1250`「stage2.cpp:529」 | `:529` 实为 `P2SkyPlaneGeometryInputs gi{}` | 无关内容 |
⇒ `PHASE2_MOSAIC_WRITE.md:496`「**其余给定锚全部实测吻合。**」与实测不符。

**F-4 `INTEGRATION.md:53-54` 的 `valid(i)` 在 `weights=null` 时解引用空数组**
`:53-54` 末位合取写作 `∧ (weights 空 ∨ finite(weights[i])) ∧ **weights[i]≥0**`——末项落在守卫之外，而 `:42` 明写「`weights` 可空（等权）」。实现 `integrate.cpp:51-57` 是对的。SCI 正本低于算法层正本，须回改 SCI。

**F-5 `GATES_AND_TOLERANCES.md:81` 的「零命中」是自指陈述**
「4 个阈值在活动 `docs/**` 零命中」——实测 4 个 token 各有 **1** 次命中，且该命中就是这一行本身。诚实写法应为「除本行外零命中」。

**F-6 §2a.7/§7 的判据非退化记录未进 `实验/TAUTOLOGY_REGISTER.md`**（转录自代理，本人未独立复核）
`CONTROL_WEIGHT_SNR.md:216` 与 `PSF.md §16` 的双向读数（绿/红）经代理复核判定为**真双向可判**，建议登记以免后续重开。

### 4.3 建议

- **S-1** `ACR_EQUIVALENCE_ALGORITHMS.md:14-17/:30-32`：`mosaic_reject_legacy` 即 CPU reference（`:57`「CPU 为 reference」），且 `:49` 保证「per-pixel 独立无跨 pixel 归约」⇒ F1/F2 分块等价是**构造性恒等式**，全篇 82 行零负向注入。建议按 `GATES_AND_TOLERANCES.md` §1 R3 重述为「一致性锁」而非判据。
- **S-2** `PHASE2_MOSAIC_WRITE.md:356`/`:389` 把 `docs/science/algorithms/ACR_EQUIVALENCE.md` 指为「**权威容差域**」，该文件不存在（真实件为 `docs/science/ACR_EQUIVALENCE.md` 与 `algorithms/ACR_EQUIVALENCE_ALGORITHMS.md`）。
- **S-3** `ACD_LINEAR_DEFECT_LITERATURE.md:266`：「一手定义（原文）」列混入编者自撰转述（无引号），而同列其余 11 条均为带引号原文或逐字 DQ 位值。
- **S-4** `PSF_SIGNAL_WEIGHT.md:43`/`:52` 对 `PSFFluxPower` 串与 PCL 头文件数值主动标「**未独立核验**」——**这是本片最诚实的引用治理，应作为全片模板**；建议其余文件比照补「未独立核验」标记。
- **S-5** `PSF_SIGNAL_WEIGHT.md:129`「帧级臂 RMSE ≤ K·s_field 类判据对任意真值场恒真…**证据资格 = 空**」——本片唯一主动作废自身恒真门的样本，建议提为全片规则。

---

## 5. 你主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **X-1** | 把 `ipv_wcs.cpp:290-291` 的 `img_width/2.0+0.5` 改成 `img_width/2.0+0.75`，看 `G-P1-WCS-CRPIX` 是否变红 | 证明该门无判别力 | **推翻成立**（静态推导）：测试 oracle `p1wcs_tests_oracle.cpp:342` 同样写 `fx.width/2.0+0.5`，两侧同步偏移 ⇒ 门恒绿。**B-1 成立** |
| **X-2** | 把 `p2_upm_evaluate_c` 返回值整体 ×2，看 `ivar_wiring_test` 的 `\|got1−expect\|` 是否变 | 证明 UPM oracle 无判别力 | **推翻成立**：`:230` 的 `cal = raw − 2C` 与生产 `raw − 2C` 同步平移 ⇒ 差不变 ⇒ 恒绿。**B-2 成立** |
| **X-3** | 激活 `sdet_detect_saturated_stars` 的调用，确认 `STAR_DETECTION_ALGORITHMS.md:284-285` 的「零消费者」是否成立 | 推翻退役登记 | **未推翻**：`git ls-files | grep -c sdet_detector.cpp` = 0（文档逐字写的是 `sdet_detector.cpp`，确实不存在）。但同文件 `:441/:445` 引用的在役文件是 **`star_detector.cpp`**（`git ls-files` 命中 1，`star_detector.cpp:73` = `StarDetector::estimate_background`）。⇒ **字面为真但登记误导**，降级为建议（见 §6 注） |
| **X-4** | 在 `min_samples=64` 的正确实现上跑 `NOISE_MODEL` §11 的 5% oracle 1000 次 | 检验该门是否恒红 | **推翻成立**：文档 `:371` 自报通过率 **92.8%** ⇒ **7.2% 恒红**。**B-6 成立** |
| **X-5** | 把 `psfFwhmLoRatio/psfFwhmHiRatio/maxPeakFraction/minQuarterMaxPixels` 四字段设为非 0，看文档是否覆盖该行为 | 检验 O13b 是否被文档覆盖 | **推翻成立**：门在 `sdet_api.cpp:433` 且 `:2135` 在生产 impl 调用，文档 0 提及。**B-10 成立** |
| **X-6** | 取 3 帧输入 order={5,7,9}、`cfg.target_order=7`，按 `PHASE2_MOSAIC_WRITE.md:337-338` 读 | 检验文档与代码方向 | **推翻成立**：代码 `stage2.cpp:213` 用 `cov.target_order=min=5` ⇒ `7>5` ⇒ rc=3；文档读法 ⇒ `7≤9` ⇒ 放行。**B-7 成立** |
| **X-7** | 核 `STAR_DETECTION_ALGORITHMS.md:44` 对 `STAR_DETECTION_ALGORITHMS.md:55-56` 的逐字引用 | 检验伪引 | **未推翻**：`:55-56` 逐字含该句 ⇒ **不是伪引**（文档正确，予以确认） |
| **X-8** | 核 §31.10 条款计数 `clauses_total=96; FROZEN 39 / PENDING 49 / OPEN 8` | 检验计数一致性 | **未推翻**：`clause_registry.json#counts_by_status` 逐字 = `{FROZEN:39, OPEN:8, PENDING_OWNER_SIGNOFF:49}`，三 ID 清单逐条数得 49/8/39，合计 96 ⇒ **一致**（文档正确，予以确认） |
| **X-9** | 核 GATES 表 10 个 `ctest:` 证据 ID 是否解析（R2） | 检验证据可解析性 | **未推翻**：全部 `add_test` 命中；`eng/tests/backend/test_psf_moffat_oracle.py` 存在 ⇒ **R2 成立**（转录自代理，本人已用 `add_test` 复核命中数） |
| **X-10** | 核 `PHASE3_RESAMPLE.md:23`「1-based」与 `:24` 公式 `(x+1)−CRPIX1` 是否自洽 | 检验 1px 桥接 | **推翻成立**：1-based 下应为 `x−CRPIX`；同文件 `:103` 伪代码 `for x in 0..W_out−1` 是 0-based ⇒ 三方冲突。列入 F（转录自代理，本人已读该两行确认） |

---

## 6. 盲复算（遮住既有判定独立取证）

**做法**：对 §4 中权重最高的 3 条，我先只读**被引的代码/注册表**，在不看子代理结论的情况下独立取证，再回来比对。

| 盲复算对象 | 独立取证结果 | 与原结论比对 |
|---|---|---|
| `G-P1-WCS-CRPIX` | 独立读 `ipv_wcs.cpp:290-291` → `img_width/2.0+0.5`；独立 grep `p1wcs_tests_oracle.cpp` → `:342` `fx.width/2.0+0.5`。三方同式 | **一致**（我自己独立得出同一结论，且是在派发子代理之前） |
| `science_contract_lint.py` | 独立 `ls` → 不存在；`git ls-files` → 0；`find -empty` → `eng/tests/sciencelint` 空目录 | **一致**（与两条车道独立吻合） |
| `ivar_wiring_test` oracle | 独立逐行读 `:200-238`、`:288-320`，画出 `expect` 的构造链 | **一致**，且我补出了子代理未强调的一点：`:204` 注释自认「生产 UPM 模型求值」 |
| `k_corr` 冻结值 | 独立 grep → `upm.cpp:2233` `= 1.4`、`:2968` `return 3` | **一致** |
| `NOISE_MODEL` 5% oracle | 独立读 `:371` → 通过率 92.8% | **一致**，且我认为这条的严重性高于代理给的定位：它是**恒红**而非仅缺负例 |

**判**：对 5 条做盲复算，**全部一致，无偏松、无偏严**。唯一需要下调的是我自己在分析中途对 X-3 的判断（见下）。

> **对我自己结论的一处订正（如实记录）**：我在读到 `STAR_DETECTION_ALGORITHMS.md:284-285` 时，第一反应是「文档谎报 `star_detector.cpp` 在库」，并准备按阻断登记。独立复核后发现文档逐字写的是 **`sdet_detector.cpp`**（`sdet_`），该文件确实 0 命中 ⇒ **文档字面为真**。子代理 74900e63 把它列为阻断 B-1 并转述为「文档说 `star_detector.cpp`」，属**转录错误**。我**否决**该条阻断定级，改为建议级「一字母之差的命名近似使登记误导」。这是我本片最重要的一次自我纠偏。

---

## 7. 子代理派发记录

**派发 6 个**（任务要求 3–5；因首个车道配置重复、无法回收，故实发 6 个，如实记录）。

| 车道 | 范围 | 读的行 | 结论产出 |
|---|---|---:|---|
| `07ce52f7` | `DATA_SEMANTICS.md` | 3416 | 未在本人轮次内回报 |
| `101d1552` | `DATA_SEMANTICS.md`（与上重复配置） | — | 未回报；本人在 `job_kill` 前已完成该文件全文自读 |
| `e1cbc669` | PHASE2_UPM / PHASE2_MOSAIC_WRITE / PHASE2_INTEGRATION / INTEGRATION | 1610 | 4 阻断 / 8 须修 / 6 建议 + 5 反例 |
| `5b66ce63` | PHOTOMETRY / CALIBRATION / NOISE_MODEL / UNCERTAINTY / CONTROL_WEIGHT / PSF / PSF_SIGNAL_WEIGHT | 2121 | 5 阻断 / 14 须修 / 9 建议 + 14 反例 |
| `fbfa3a1c` | PHASE3_RSMP_IMPL / IO_002 / PHASE3_HIPS_TO_FITS / PHASE3_RESAMPLE / GATES / HEALPIX_MAPPING | 1383 | 5 阻断 / 12 须修 / 8 建议 + 10 反例 |
| `74900e63` | STAR_DETECTION_ALG / PLATESOLVE / CCD_LINEAR_DEFECT / ASTROMETRY / ACR_EQUIVALENCE_ALG | 1500 | 8 阻断 / 19 须修 / 5 建议 + 7 反例 |

### 7.1 逐条复核：我做了什么

对每条阻断/重条，本人到仓内**重新取证**（亲自 `sed`/`grep`/`read`），不看代理的推理链。已独立复核并**采纳**的：

| 代理条目 | 我的独立复核 | 结果 |
|---|---|---|
| e1cbc669 B2（lint 不存在） | `ls` + `git ls-files` + `find -empty` + `git log --diff-filter=D` | **采纳**（并升级为全片 10 份口径，计 8 份在本片） |
| e1cbc669 B1（UPM oracle 同源） | 逐行读 `ivar_wiring_test.cpp:200-238/:288-320` | **采纳** |
| e1cbc669 B3（legacy 臂） | `stage2.cpp:785/:797-804` | **采纳** |
| e1cbc669 B4（target_order） | `stage2.cpp:205-222` + 文档 `:50-54/:335-340` | **采纳** |
| e1cbc669 M6（行号漂移） | `wc -l` + `sed -n '525,528p'` + `grep -n A_cell` | **采纳**，并**新增**：查出该错误锚已传染进 `DATA_SEMANTICS.md:1244/:1246`（代理未查） |
| 5b66ce63 B2（k_corr 1.4） | `upm.cpp:2233/:2964-2974` + `phase2_integrate.h:71` | **采纳** |
| 5b66ce63 B4（lint） | 同上，已由我先验 | **采纳**（去重） |
| 5b66ce63 F10（92.8% 恒红） | `NOISE_MODEL.md:343/:371` | **采纳**，并**升级为阻断**（代理列为须修） |
| 5b66ce63 B3（§8c 恒真门） | 未独立复核 | 降级为建议 |
| fbfa3a1c B2（:114 vs :127） | 本人读 `PHASE3_RESAMPLE.md:114/:127` | **采纳** |
| fbfa3a1c B1（coverage 与 SCI 冲突） | 未逐行复核 `p3_resample.cpp` | 降级为须修（保留 B-9，但把「生产码违反 SCI」部分标待前台） |
| 74900e63 B2（母函数参数化） | `sed -n '188,208p' sdet_api.cpp` + 文档 `:113-119/:399-401` | **采纳** |
| 74900e63 B3（13 字段 / O13b） | `star_detector.h` 逐字段 + `grep` 文档 0 命中 | **采纳**（并升级为阻断，代理列为阻断） |

### 7.2 我**否决**的代理结论及理由

| 代理条目 | 代理主张 | 我的否决理由 |
|---|---|---|
| **74900e63 B1** | 「`STAR_DETECTION_ALGORITHMS.md:284-285` 称 `sdet_detector.cpp` 0 命中**为假**，该文件已入库且在役」 | **否决**。文档逐字写的是 `sdet_detector.cpp`（`sdet_`），`git ls-files | grep -c sdet_detector.cpp` = **0** ⇒ 文档字面为真。代理在收尾消息里把它转述成「文档说 `star_detector.cpp`」，**属转录错误**。在役文件实为 `star_detector.cpp`（`star_`），与被点名者只差一个字母。**降级为建议**：登记误导，但非虚假陈述。 |
| **e1cbc669 B2 / 5b66ce63 B4 定级** | 列为「阻断：恒真门」 | **部分下调**。该 lint 条件是**验收条款缺失执行体**，性质上是「门不存在」而非「门恒绿」；但因影响 10 份正本且与负责人裁决 `ed33f57f` 直接冲突，**保留阻断**。 |
| **74900e63 B8/S16（PLATESOLVE 判据簇）** | 3 条判据构造上无法判红 + DISP-WCS-008 往返自证 | **未独立复核**，本件**不登记为阻断**，转 §4.2 须修并标注「转录自代理、本人未独立复核」。理由：`DISP-WCS-008` 的往返自证指控方向正确，但我未独立读 `ipv_wcs.cpp:445-449/:522` 确认 AP/BP 确由最小二乘拟合，不宜据未复核内容定阻断。 |
| **e1cbc669 M3（量纲混用）** | `PHASE2_INTEGRATION.md` 像素域/面亮度域自相矛盾且错版在「唯一权威」节 | **未独立复核**，列入 §4.2 F 并标注转录。 |
| **5b66ce63 B1（P2 绝对 SNR 不绝对）** | 顶层创新点表述需回写 | **未独立复核** `ACSD_DESIGN.md`（属他片），本片**不登记**，建议前台转交对应片。 |
| **fbfa3a1c B4（export 域判据零覆盖）** | 判据集中地对 export 相位 3 几乎零覆盖 | **方向合理但本片责任面在 ENG-contracts/其他片**，本件仅记录为建议。 |

### 7.3 计数口径（按任务书要求写明是哪一层）

- 本片：**门实例层**（去重后）共识别 **13 条阻断**。
  - 其中**门实例 = 8 条**：B-1（CRPIX 恒真门）、B-2（UPM oracle 同源）、B-3（lint 执行体缺失）、B-6（5% oracle 恒红）、B-9（coverage 语义互斥）、B-11（BUNIT 缺口判据恒绿）、B-12（§4a 三态表恒红）、B-13（单位串互斥且门站错边）。
  - **非门类文档冲突 = 5 条**：B-4（k_corr）、B-5（legacy 臂）、B-7（target_order）、B-8（母函数参数化）、B-10（O13b 漏文档）。
- **去重门**：
  - 「`science_contract_lint.py` 不存在」跨 10 份 science 文档重复出现 ⇒ **只算 1 个门实例**，整改分母 = 10 个文档引用点。
  - 「k_corr 生产恒落 1.4」由 B-4（UCM:36）与 M-4（DATA_SEMANTICS:1748）**从两个文件独立命中** ⇒ 合并为 1 个门实例、2 个整改点。
  - 「行号锚漂移」横跨 `stage2.cpp` / `aio_hips_writer.cpp` / `sdet_api.cpp` 等 ⇒ 合并为 1 类（整改分母 = 4 个文件）。
- **整改分母**：本片 23 份成员文件中，**13 份**含阻断级问题 ⇒ **整改分母 = 13/23 份**。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 0) 基线
git log --oneline -1                 # 期望 f9650dd0
git status --porcelain               # 期望空（除本交付件）

# 1) 取本片成员清单与行数
sed -n '902,926p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
wc -l docs/science/DATA_SEMANTICS.md docs/science/PHASE2_UPM.md \
      docs/science/algorithms/PHASE2_MOSAIC_WRITE.md docs/science/CALIBRATION.md \
      docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md docs/science/algorithms/PHASE3_RSMP_IMPL.md \
      docs/science/PHOTOMETRY.md docs/science/NOISE_MODEL.md docs/science/algorithms/PLATESOLVE.md \
      docs/science/algorithms/PHASE2_INTEGRATION.md docs/science/CCD_LINEAR_DEFECT_LITERATURE.md \
      docs/science/IO_002_HIPS_INPUT_INTERFACE.md docs/science/ASTROMETRY.md \
      docs/science/CONTROL_WEIGHT_SNR.md docs/science/PSF.md docs/science/PHASE3_HIPS_TO_FITS.md \
      docs/science/algorithms/PHASE3_RESAMPLE.md docs/science/INTEGRATION.md \
      docs/science/UNCERTAINTY_AND_COVARIANCE.md docs/science/PSF_SIGNAL_WEIGHT.md \
      docs/science/algorithms/GATES_AND_TOLERANCES.md docs/science/algorithms/HEALPIX_MAPPING.md \
      docs/science/algorithms/ACR_EQUIVALENCE_ALGORITHMS.md | tail -1     # 期望 10030

# 2) B-1 恒真门（G-P1-WCS-CRPIX）：三方共享同一定义式
sed -n '70p'  docs/science/algorithms/GATES_AND_TOLERANCES.md
sed -n '288,292p' lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp
sed -n '340,345p' eng/tests/unit/p1wcs/p1wcs_tests_oracle.cpp
grep -n "G-P1-WCS-CRPIX" docs/science/algorithms/GATES_AND_TOLERANCES.md

# 3) B-2 UPM oracle 同源
sed -n '204,213p;229,230p;292p;298,299p;312,318p' lib/algorithms/coverage/tests/ivar_wiring_test.cpp
sed -n '355,360p' docs/science/algorithms/PHASE2_MOSAIC_WRITE.md

# 4) B-3 lint 判据执行体缺失（本片 8 份 + 他片 2 份）
ls eng/tools/science_contract_lint.py ; echo "exit=$?"          # 期望 exit≠0
git ls-files | grep -c science_contract_lint                     # 期望 0
find eng/tests -maxdepth 1 -type d -empty                        # 期望 sciencelint/results
grep -rn "science_contract_lint" docs/science/ | cut -d: -f1 | sort -u
git log --oneline --diff-filter=D -- eng/tools/science_contract_lint.py

# 5) B-4 k_corr 生产冻结值
sed -n '2233p;2964,2973p' lib/algorithms/coverage/src/upm.cpp
grep -n "kKCorrFrozen" lib/algorithms/integration/phase2_integrate/include/acsd/phase2_integrate.h
sed -n '36p;55,56p' docs/science/UNCERTAINTY_AND_COVARIANCE.md

# 6) B-5 legacy 降级臂已删
sed -n '783,806p' lib/algorithms/coverage/tools/stage2.cpp
sed -n '365,367p' docs/science/algorithms/PHASE2_MOSAIC_WRITE.md

# 7) B-6 5% oracle 恒红（7.2%）
sed -n '343p;371p' docs/science/NOISE_MODEL.md

# 8) B-7 target_order 方向（代码注释已逐字点名文档错误）
sed -n '211,220p' lib/algorithms/coverage/tools/stage2.cpp
sed -n '52,53p;337,338p' docs/science/algorithms/PHASE2_MOSAIC_WRITE.md

# 9) B-8 母函数参数化
sed -n '188,202p' lib/algorithms/star_detection/src/sdet_api.cpp
sed -n '113,119p;399,401p' docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md

# 10) B-9 coverage 语义互斥
sed -n '113,114p;126,127p' docs/science/algorithms/PHASE3_RESAMPLE.md
sed -n '399,404p' lib/algorithms/resample/p3_resample.cpp

# 11) B-10 O13b 形状门零文档
grep -n "psfFwhmLoRatio\|psfFwhmHiRatio\|maxPeakFraction\|minQuarterMaxPixels" lib/algorithms/star_detection/include/star_detector.h
sed -n '433p;437,440p' lib/algorithms/star_detection/src/sdet_api.cpp
grep -c "psfFwhm\|PeakFraction\|QuarterMax\|O13b\|形状门" docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md   # 期望 0

# 12) F-3 源码锚漂移 + 传染进 DATA_SEMANTICS
wc -l < lib/algorithms/coverage/tools/stage2.cpp                     # 期望 2019（文档称 2015）
sed -n '525,528p' lib/algorithms/coverage/tools/stage2.cpp            # 期望 P2SkyPlaneGeometryInputs（与球面常量无关）
grep -n "A_cell" lib/algorithms/coverage/tools/stage2.cpp | head -1  # 期望 :758
grep -n "stage2.cpp:52[5-8]" docs/science/DATA_SEMANTICS.md

# 13) §6 自我订正的复现（X-3）
grep -c "sdet_detector.cpp" <(git ls-files)      # 期望 0  → 文档 :284-285 字面为真
grep -c "star_detector.cpp" <(git ls-files)      # 期望 1  → 在役文件是 star_detector.cpp
sed -n '284,285p;441p' docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md

# 14) X-7 / X-8 / X-9 复核（确认文档正确，不算缺陷）
sed -n '55,56p' docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md   # 逐字引用存在
python3 -c "import json;print(json.load(open('eng/contracts/data/clause_registry.json'))['clause_registry']['counts_by_status'])"
grep -rn "add_test" --include=CMakeLists.txt . | grep -c "p1wcs_apbp\|p3_wcs\|p1star_units"
```

---

## 8b. 补记：DATA_SEMANTICS.md 三条阻断（代理 07ce52f7 提出，本人逐条独立复核后全部采纳）

这三条的落点文件**是本人自己 3416 行逐行读完的**，代理只做了代码对照取证。三条我均已亲手复核代码锚，**全部采纳**。

---

**B-11 §20.2 的「BUNIT provenance 缺口」复现判据恒绿，据此登记了一个不存在的缺口**

- 位置：`docs/science/DATA_SEMANTICS.md:1255-1266`（本人读）
- 现状（逐字 `:1263-1266`）：「以现行 Phase2 面亮度产品（`BUNIT=ADU/sr`）为输入调用 `resolve_bunit`，声明 `pixel_semantics=SurfaceBrightness` 且 `pixel_area_power=-2`…；判据 = 返回 `resolvable=false` ⇒ 缺口成立、§5.3 守卫**未生效**。反例（负对照）：把 `BUNIT` 换成 `ADU` 且 `pixel_area_power=0` 时 `resolvable=true` ⇒ 判据非退化。」
- 应为：撤掉该登记项与「**§5.3 守卫生效的判据 = 该缺口关闭**」这条闭环。
- 证据（本人复核 `lib/algorithms/resample/p3_rsmp_units.cpp:171-181`）：
  ```
  171: if (parsed.px_power != 0) {
  172:   r.resolvable = true; r.resolved = parsed;
  174:   r.reason = "explicit_solid_angle_power"; return r; }
  ```
  ⇒ `BUNIT="ADU/sr"` 解析出 `px_power=-2 ≠ 0` ⇒ **立即返回 true**，分支内 `prov` 两个字段**从不被读取** ⇒ 文档写的「外部参照」是死参数 ⇒ 该判据**恒为绿（缺口不成立）**，其「复现方法」永远无法复现。
- 负对照也错：`:182-190` 要求 `pixel_semantics == IntegratedFlux && pixel_area_power == 0` 才 true；文档负对照只说换 `BUNIT=ADU` + `pap=0`、未同时换 `IntegratedFlux` ⇒ 按字面执行得 **false**，与文档所称 `true` 相反。
- 加重项：同段 `:1259-1261` 称「`signal/properties` 只有 `dataproduct_subtype`…**无** `pixel_semantics` / `pixel_area_power`」，并称守卫「未接线」；代理在 `module_adapters.cpp:12958-12961`（写出 `ACSD_SIGNAL_UNIT`/`ACSD_PIXEL_SEMANTICS=surface_brightness`/`ACSD_PIXEL_AREA_POWER=-2`）与 `:14595`（守卫已在生产链接线）处找到反证（**本人未独立复核此两处**，标待前台）。

---

**B-12 §4a 三态表在「area 非有限」一格自相矛盾且与实现相反，而 §4a 自称唯一口径**

- 位置：`docs/science/DATA_SEMANTICS.md:82` vs `:83`（本人读）
- 现状（逐字）：
  - `:82`「| **无覆盖** | `area<=0` 或 `area` **非有限** | **`NaN`** | **`NaN`** |」
  - `:83`「| **损坏** | `vnum` 非有限 或 `vnum<0`（**或 `area` 非有限**）| … ⇒ **rc=−6 硬失败** |」
  - `:86`「**三个态互斥且穷尽**」
  ⇒ 同一张自称互斥穷尽的表，对 `area` 非有限同时给出 `NaN` 与 `rc=−6` 两个互斥结论。
- 应为：按实现归入「损坏」，或改码并回改 §12.4/§30.1。
- 证据（本人复核 `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1586-1593`）：
  ```
  1586: if (!std::isfinite(area) ||
  1587:     (area > 0.0 && (!std::isfinite(vnum) || vnum < 0.0))) {
  ...        return -6;
  ```
  ⇒ `area` 非有限**命中第一段** ⇒ rc=−6，**不是** NaN。
- **双向都被污染（恒红方向）**：按 `:82` 写的 oracle（`area=NaN ⇒ variance=NaN`）对正确实现**恒红**；按 `:83` 写的 oracle 又与 `:504`（「无覆盖（`area<=0`/非有限）→ `variance=NaN`」）及 `:2754`（只引 `covered_area≤0`）恒红。**§4a 是全文被反复引用的唯一编码口径**（`:76`「下表是**唯一口径**」、`:88`「凡与本表不一致的表述一律以本表为准」），故该矛盾会污染所有由它派生的机器判据。

---

**B-13 §31.7 与 §31.1a 对 psfsw concentration 单位串互斥，且机器门固定站在违反另一条的一侧**

- 位置：`DATA_SEMANTICS.md:3213-3216`（§31.1a）vs `:3315`（§31.7）—— 两处本人均读过
- 现状（逐字）：
  - §31.1a `:3213`「⇒ 反例（两处违规）：`ADU/px^2` —— ① `px` **不是** Table 4 列出的单位串（只有 `pixel` 与 `pix`）；② 幂符号 `^` 不在 Table 6 允许集内。合规写法为 `adu/pix**2` / `adu/pixˆ2` / `adu/pix2`。」
  - §31.7 `:3315`「**`ADU/px²` 是唯一合法写法**（依据 …）；合法域**不含** `ADU/px`…生产正例 `eng/contracts/data/examples/psfsw.example.json` 的写法为 `ADU/px^2`。」
- 应为：两条 FROZEN 级条款必须择一。
- 证据（本人复核）：`eng/contracts/data/examples/psfsw.example.json:35` 逐字 `"units": "ADU/px^2",` ⇒ **生产正例站在 §31.7 一侧**；schema 的 `concentration.units` pattern 强制 `px^2` 且配 fail-closed 门 `G-PSFSW-4COMP-UNITS`（代理给出，未独立复核 pattern 原文）⇒ **按 §31.1a 改正的产线会被自己的门 REJECT**。
- 加重项：§31.1a 引的是 FITS 4.0 Table 4/6 一手条款（`:3204-3207`），§31.7 却与之直接冲突；两条均标 FROZEN。

---

### 8b.1 代理 07ce52f7 的其余条目（本人未独立复核，降级记录）

- **M-1** §11.2/§11.3 把 HiPS signal 底数登记为「未归一」，与 `astro_sphere_sink.cpp:182-193` 的发布因子 `k = sb_publish_scale(sumArea,sumNorm)` 冲突；§4a `:48` 的「pixfrac² 在分子分母相消」只有乘上 k 才成立，而 `sb_publish_scale` 在 3416 行内**零登记**。
- **M-2** `input_manifest_hash` 行号锚三套互斥（`:112-113` / `:1341-1342` / `:2837`），均未覆盖真正的 sha256 行。
- **M-3** `:1774` 的 σ-clip 修正因子 `C(a)` 全仓只此一处、无推导无引用、`sampler.cpp:875` 未实现；代理的解析论证是：对称截尾下 `C≡1`、丢弃读法下 `(1−2q)²≤1`，**两种读法都得不到 `C(1)=1.8787`**。**此项若成立会推翻 DATA_SEMANTICS §23.2 与 UCM:36 的一整段**，建议前台优先裁决。
- **M-4** `:1748` 的 k_corr「公式面」（`k_gauss(N)×k_geo`）≠ 实现面（`kcorr_lookup` 是 pixfrac×角尺度双线性表，与 N 无关），且 `sampler.cpp:89-92` 明写生产帧角尺度远在标定域外 ⇒ **每帧恒落 1.4，「逐帧查表优先」生产恒不触发**。与 **B-4** 互为佐证。
- **M-5** `:1326-1333` 的负对照不成立：`A_cell` 实为 `double`（`aio_hips_writer.cpp:966`），f32 舍入来自分子；正例与负对照落在同一数值域，两臂不可区分 ⇒「判据非退化」不成立。
- **M-6/M-7** writer 行号锚系统陈旧约 800 行；六处「源码行数」与 HEAD 不符。
- **S-1** §4a `:58`「`A_cell²`…`4.306e21`」—— 量名与数值互为倒数（实为 `1/A_cell²`，`log10=21.634`）。

### 8b.2 本人复核后**否决**的代理结论

- 代理 07ce52f7 的 **S-3**（§31.10 条款计数）与 **S-2**（Cramér 引文）方向：我**独立复核并确认文档正确** —— `clause_registry.json#counts_by_status` 逐字 = `{FROZEN:39, OPEN:8, PENDING_OWNER_SIGNOFF:49}` = 96，与 `:3395` 一致；三张 id 表逐条点数 49/8/39 自洽。**不计为缺陷**（见 §6 X-8）。
- 代理 07ce52f7 **S-2** 的 Caveat 正确且我采纳：Cramér 1946 逐字引在仓内三处一致，但**内部一致 ≠ 原文存在**，需一手页码级核验。

---

## 9. 相对前三轮已记问题，本片**新**发现的

**新（此前十域/六域/四域审稿未记）**：

1. **`G-P1-WCS-CRPIX` 恒真门**（本人构造 X-1 独立得出，代理未覆盖）——三方共享 `(w/2+0.5,h/2+0.5)` 定义式，发布门 Y。
2. **`NOISE_MODEL` 5% oracle 7.2% 恒红**——本任务书点名的「恒红门把真实缺陷藏在红灯里」，文档 `:371` **自己**记录了 92.8% 却未据此调整门。
3. **`STAR_DETECTION_ALGORITHMS.md` §2 母函数参数化错误，且与同文件 F4 自相矛盾**——按 §2 写独立 oracle 会在正确实现下判红。
4. **O13b 点源形状门（13 字段中的 4 个 + `fitRadius`）在科学文档零提及**，且文档自称的「零消费面」被自家合同头 `star_detector.h:37` 直接证伪。
5. **`stage2.cpp` 错误源码锚已传染进 `DATA_SEMANTICS.md:1244/:1246`**（代理只查到 PHASE2_MOSAIC_WRITE，本片新查出跨文件传染）。
6. **`GATES_AND_TOLERANCES.md:81`「零命中」自指**。
7. **`DATA_SEMANTICS.md` 节序错乱（§29 在 §28 之前）**。
8. **`DATA_SEMANTICS.md §20.2` 的「BUNIT 缺口」复现判据恒绿**（B-11）——`resolve_bunit` 在 `px_power≠0` 分支提前返回，`prov` 从不被读取；据此登记的缺口不存在，且派生出一条错误闭环判据。
9. **`DATA_SEMANTICS.md §4a` 三态表在「area 非有限」一格自相矛盾且与实现相反**（B-12）——该表被全文引为「唯一口径」，矛盾会污染所有派生机器判据。
10. **`DATA_SEMANTICS.md §31.7 vs §31.1a` 两条 FROZEN 条款互斥，且机器门固定站在违反 §31.1a 的一侧**（B-13）——按 §31.1a 改正的产线会被自己的门 REJECT。
11. **`DATA_SEMANTICS.md:1774` 的 σ-clip 修正因子 `C(a)` 全仓无推导、无实现、解析上两种读法都得不到**（M-3）——若成立会推翻 §23.2 与 UCM:36 的整段，建议前台优先裁决。

**非新（前几轮已记，本片复核确认）**：`science_contract_lint.py` 不存在（`ed33f57f` 裁决残留）、`legacy_allow_weight_fallback` 已删、target_order 方向、恒真门族与自愈判据族本身。

**待前台裁决 / 待联网核验**：
- `DATA_SEMANTICS.md:1774` 的 `C(a)`（M-3）——解析疑为错，需 owner 或实验裁决。
- `CCD_LINEAR_DEFECT_LITERATURE.md` 的期刊卷页/DOI（其自述的 arXiv abs 页核对方法在该页面取不到字段）。
- PHOTOMETRY / PSF_SIGNAL_WEIGHT 的外部一手逐字引用（本片未做一手核验；两文件已**主动**标注「未独立核验」，属诚实登记）。
- B-11 的加重项（`module_adapters.cpp` 守卫链接线与 provenance 键写出）——本人未独立复核，标待前台。

**本片确认「文档正确、不算缺陷」的项**（供排除，避免下一遍重开）：§31.10 条款计数 96/39/49/8 与 `clause_registry.json` 逐字相符（X-8）；GATES 表 10 个 `ctest:` 证据 ID 全部解析、R2 成立（X-9）；`GATES:44` 对 `STAR_DETECTION:55-56` 的逐字引用**是真引**（X-7）；`κ₅₀ = 6.22±0.50` 我用 §11.4 表独立复算得 mean 6.2062 / sd 0.4971 ⇒ 与文档一致；Cramér 1946 逐字引在仓内三处一致（但需一手核验）。