# 全仓排查 · 检查-S15 · 实验单元 P3 healpix-polar

**责任域**：`实验/healpix-polar/` 全部文件（find 枚举）—— `REPORT_paper.md`(157 行) / `REPORT_experiment.md`(132) / `README.md`(63) / `refs.md`(83) / `docs/DERIVATIONS-P3.md`(171) / `docs/EXP-07-POLAR.md`(795) / `docs/EXP-07-POLAR-摘要.md`(59)；`code/`、`results/` 全静态交叉核对（不执行）。
**检查面**：①科学性 ②行文逻辑 ③跨文档冲突 ④幻觉与锚。
**纪律**：只读排查，除本报告外零写入、零 git 操作、零构建/测试/试跑；科学公式、默认容差、冻结定义类问题只登记为红＋证据，不代拟改法；`独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表内已订正并验证通过者不重报；D-01…D-11、A-P3-01…12 终裁不重开。

**统计：红 2 · 黄 8 · 绿 3。**

---

## 一、红（必须改）

### 红-1 ｜面④（兼③）｜ `REPORT_paper.md:83` —— "禁抄值 5 处残留已清除"为假完成态

- **问题描述**：正文写「禁抄值 5 处残留清单已在审计中登记清除（A-P3-09）」，读者据此认为 211034.6 残留已清；工作树中 5 处全部仍在。
- **证据（反验证）**：
  - `grep -rn 211034.6`：`lib/include/hp_drizzle_api.h:114`、`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:180`、`:181`、`:198`、`lib/infrastructure/scheduler/src/module_adapters.cpp:7603`、`lib/algorithms/drizzle/README.md:93` —— 与 A-P3-09 清单的 5 处逐一对上，无一被删除或加订正注记。
  - 交叉面（③）：`独立审计/实验重做/总编对账/五单元成稿简报.md` P3 段（:105 起）「### 需订正的文档条目」（:142）第 1 条**仍把它列为待办**：「禁抄值 211034.6 残留 5 处清除（hp_drizzle_api.h:114、orchestrator.cpp:180-181/198、module_adapters.cpp:7603、drizzle/README.md:93）」。同一批对账产物中"已清除"与"待清除"两说并存。
  - 行为面排除：生产算式本身正确（`orchestrator.cpp:198` 用 `std::sqrt(PI/3)*(180/PI)*3600`、`drizzle_engine.cpp:709-710` 同源，得 211076.2851…），故残留纯属注释/文档禁抄值——但仍需真实清除，A-P3-09 登记的动作**尚未发生**。
- **建议改法**：把该句口径改为「已登记入清除清单（截至本稿 5 处仍在工作树，未清除）」，或按 A-P3-09 实际执行清除（涉及域外生产文件，走登记上呈，不由本单元代改）。
- **所属面**：④（文档声明与代码实况不符；兼 ③ 与五单元简报状态冲突）。

### 红-2 ｜面④｜ `REPORT_experiment.md:118-121`＋`:127-129`、`README.md:44-45` —— 文档字面的"一键复现"链路不可用（收编后路径未适配）

- **问题描述**：文档承诺"cd 实验/healpix-polar 后 `bash code/audit/run_all.sh`，输出与 `results/audit/<路线>/` 逐位对照，tables.md 可由 read_tables.py 重生成"。按字面执行，该链路**第一步即中止**；即使补齐参数，输出落点也全部与文档所述目录不符，对照与表重生成均不可达。
- **证据（逐条静态核对）**：
  1. **无参即中止**：`code/audit/run_all.sh:9-11` `set -u` 后紧跟 `MODE="$1"`。语义实测（通用 shell 探针，不触及本仓）：`bash -c 'set -u; MODE="$1"'` → "$1: 未绑定的变量"、立即退出，:11 的缺省 quick 分支不可达；而文档命令 `REPORT_experiment.md:120` 恰不带参数 ⇒ 第一行失败。同一模式见根 `run_all.sh:10-11`，`README.md:45` 却把参数写作可选 `[quick|full]`。
  2. **route1 写路径未随收编适配（写在打印之前，日志会全空）**：`code/audit/route1/e1:248/:279`、`e2:219/:242`、`e3:158/:170`、`e4:179/:208`、`e5:180/:209`、`e6:138/:156` 均 `open("../results/…")`（CWD 相对）。文档 CWD=单元根 ⇒ 解析为 `实验/results`（`ls` 实测：无此目录）⇒ `FileNotFoundError`；以 `e3:158` 为例，写 JSON 位于 `print(txt)`(`:172`) **之前** ⇒ 日志无输出、run() 判 [FAIL]。
  3. **route2/3 落错目录（写进存档根、非 results/audit/<路线>/）**：route2 十个脚本 `open("results/expNN_….json")`（如 `exp01:69`）、route3 八个（如 `exp01:126`）⇒ 落到 `实验/healpix-polar/results/` 根（该处现为历史探针 .out/.csv 存档），既不在 `run/healpix-polar-audit-logs/`，也不在 `results/audit/<路线>/`；`results/` 顶层实测无任何 `exp*.json`，说明从未按文档路径成功落盘。
  4. **kcorr 落在第三处**：`kcorr/run_scan.py:31-32`、`run_extra.py:24-25` `OUT = dirname(dirname(__file__))/results` + `makedirs` ⇒ 新建 `code/audit/results/`（仓库中不存在）；`direct_char.py:38-39` 同型。
  5. **read_tables 必失败**：`kcorr/read_tables.py:2` `R='results/'`（CWD 相对）、末行 `open(R+'tables.md','w')`。文档 CWD 下 `results/` 顶层无 `g0_sanity.json`（实测 `ls 实验/healpix-polar/results/*.json` → 不存在）⇒ `run_all.sh:51` 的 `k_tables` 步骤必 [FAIL]，`REPORT_experiment.md:121` 的"tables.md 可重生成"、`:129` 的单命令示例均落空。该脚本只有在**收编前原地址**（`独立审计/实验重做/P3守恒映射算子/补实验-k_corr/`，其 `results/` 实测存在且含全部 g*.json）下才成立。
  6. **脚本自述与代码冲突**：`code/audit/run_all.sh:15`「全部脚本不 import 仓库任何模块、不联网、**不写 results/（只打印 JSON）**」，与全目录 34 处 `open(..., "w")` 直接矛盾。
- **建议改法**：仅登记——按 AGENTS §5「实验单元自包含：……复现命令」，须把复现链路接通（run_all.sh 内按路线 `cd`、或统一改为脚本相对输出并落 `results/audit/<路线>/`、read_tables 改读 `results/audit/kcorr/`），同时订正 `run_all.sh:15` 的能力自述与 `README:44`/`REPORT_experiment:121` 的"逐位对照/可重生成"表述。本次只读，不代改。
- **所属面**：④（文档声称命令/能力，代码没接通）。

---

## 二、黄（建议改）

### 黄-1 ｜面③｜ `REPORT_experiment.md:58` —— "路线2『确认 0.104369』表述已改写"与源文件实况不符

- **证据**：`独立审计/实验重做/P3守恒映射算子/路线2/report.md:109`「绝对亏缺律 0.104369/N²」、`:123`「0.104386 vs 声称 0.104369」、`:129`「PASS（−9.968e-2 与 0.104369/N² 均独立确认）」**三处原文未动**；`grep '订正|D-09|0.1043885'` 该文零命中。本单元侧收编件 `results/audit/route2/exp02_polar_pixel.json:62` `claimed_constant: 0.104369`（原始键，按 PASS 先例可保留），`results/audit/KEY_RESULTS.md:23` 已按 D-09 转记——可支持"本单元索引已改写"，支持不了"路线2 表述已改写"。
- **建议改法**：措辞降为「本单元索引（KEY_RESULTS:23）已按 D-09 转记；路线2 原 report 仍为旧表述（域外，登记待改）」。
- **所属面**：③。

### 黄-2 ｜面④｜ `REPORT_paper.md:110`、`REPORT_experiment.md:93`、`README.md:40` —— "0/1089、最坏 2.79e-8"的证据腿 `[p2_algorithms.cpp T9]` 不支持

- **证据**：T9 归档 `results/p2_t9.out` 极点栏 `budget_abs = 1e-7·A_drop` → **最坏闭合 1.1220e-08、破门=否**；该行为 49 配置档（3 预算 × 7 位置），全文无 1089 计数。"0/1089 与 2.79e-8"的真源是 `docs/EXP-07-POLAR.md:40`（结论速览）、`:503`（§4.9 N=1089 负例注入 0/1089、2.789e-08）、`:615`（§6.1 推荐段）。两套测量同量级但口径不同（49 档台架 ≤1.122e-8 vs 1089 全网格注入 2.789e-08）。
- **建议改法**：证据腿改为 `[实验:EXP-07-POLAR T13/N4 注入]`（或同时列出 T9 实测 1.122e-8 与其 49 档口径）。
- **所属面**：④。

### 黄-3 ｜面②｜ `docs/EXP-07-POLAR-摘要.md:38` vs `:43` —— 同一预算两个"极点最坏闭合"未对账

- **证据**：表 `| REC-1 预算 1e-7·A_drop | 1.12e-8 | 1333 |` 与推荐句「预算 1e-7·A_drop ⇒ 最坏 **2.79e-8**（36× 门禁余量）」同文档、同预算并存；36× = 1e-6/2.79e-8（若取 1.12e-8 应为 89×）。正文同型：`EXP-07-POLAR.md:345`（T9 表 1.12e-8）vs `:615`（2.79e-8），均未标注"台架 49 档 vs 1089 全网格注入"的口径差。
- **建议改法**：表格加口径列（T9 台架 / N4 全网格注入），推荐句注明取哪一口径。
- **所属面**：②。

### 黄-4 ｜面②｜ `docs/EXP-07-POLAR.md:298` —— "T16 … 0/441、最坏 6.96e-14"与本文档表、归档日志、摘要三处冲突

- **证据**：同文档 `:522` 表（T16/HST nside=2²³）V0 行 = `0/441`、最坏 **2.148e-14**；`EXP-07-POLAR-摘要.md:22` 写 **0/121、1.90e-14**；归档 `results/p4_hst.out:16`（n=121 → 2.148e-14）、`results/p4_hst_n23.out:16`（n=121 → 1.896e-14）。`6.96e-14` 全单元 grep 零命中（孤证）；配置数 441 与 :522 表一致但最坏值不一致，121/441 两标签在三份文档间互换。
- **建议改法**：按归档日志统一 n 标签与其值，或为 6.96e-14 注明具体日志出处。
- **所属面**：②。

### 黄-5 ｜面④｜ `docs/EXP-07-POLAR-摘要.md:4` —— 门禁锚 `DRIZZLE_GEOMETRY.md:236` 指向无关行

- **证据**：该文 `:236` 实为「合并: 任意线程按 `merge_cursor` 升序左折叠归约」；门禁在 `:316`（§9 TEST-DRZ-DESIGN-001 冻结容差：「FP64 通量闭合 <1e-6（主域）」）。且摘要引文形式 `|Σa − A_drop|/A_drop < 1e-6` 在该文 grep 零命中（`1e-6` 仅出现于 :316/:323/:325/:373/:374）。**数值 1e-6 本身无误**，仅锚与引文形式失准。
- **建议改法**：锚改 `:316`（或指 §9 冻结容差小节），引文按该处原文表述。
- **所属面**：④。

### 黄-6 ｜面②（兼指错锚）｜ `README.md:56` —— "需订正清单见 REPORT_paper.md §5" 指错位置

- **证据**：`REPORT_paper.md` §5 =「## 5. 诚实边界」（:124-139，9 条边界/开放项），不含订正清单；实际含"须订正"陈述的是 §3.2（:81 注、:83 禁抄值、:89 单位/锚）与 §3.3（:97/:111 注），完整清单在 `五单元成稿简报.md:142`（P3 段「### 需订正的文档条目」）。
- **建议改法**：把 `§5` 换成 `§3.2/§3.3`（或直接只指简报 :142）。
- **所属面**：②。

### 黄-7 ｜面④｜ `refs.md:52` —— V6 文献题名 "Mapping on the HEALPix **projection**" 应为 "grid"

- **证据（外部核验）**：Crossref `10.1111/j.1365-2966.2007.12297.x` → 题名 **"Mapping on the HEALPix grid"**，MNRAS 381, 865-872（DOI/卷页与 refs 一致）；arXiv `astro-ph/0412607` 首作者 M.R. Calabretta、题名同为 **"Mapping on the HEALPix grid"**。"projection" 两处均无。`REPORT_paper.md` 参考文献第 6 条不带题名，未被波及。
- **建议改法**：题名改为 "Mapping on the HEALPix grid"。
- **所属面**：④。

### 黄-8 ｜面④｜ 两处写死绝对路径（违反 AGENTS §3"在仓库内工作，不写死服务器绝对路径"）

- **证据**：
  - `docs/EXP-07-POLAR.md:736`：`cd "/workspace/Astro CS Database" && python3 run/EXP-07-POLAR/verify/check_report2.py`——写死服务器绝对路径；且依赖 `run/` 下**未入库**的 `check_report2.py`，新克隆者无法执行该步（同段注释 :733-735 自认只能抓"数字整体过期"）。
  - `code/audit/kcorr/read_summary.py:2`：`OUT = '/workspace/Astro CS Database/独立审计/实验重做/P3守恒映射算子/补实验-k_corr/results'`——写死绝对路径且指向单元外原地址（收编未适配；该脚本也不在 `run_all.sh` 调用清单内）。
- **建议改法**：改相对仓库根/脚本相对路径；`check_report2.py` 依赖在 §8 注明"需已有 run/ 产物"。
- **所属面**：④。

---

## 三、绿（可不改，登记备查）

### 绿-1 ｜面①｜ `results/audit/KEY_RESULTS.md:54` "渐近式高估 9.6%" 三读并存

同表引用的 `var_over_asymptotic = 0.911253585449302`（`results/audit/kcorr/direct_char.json:43`）复算 ⇒ 1/0.911254 − 1 = **9.73%**；D-07 终裁精确值 **9.53%**。9.6% 与两值同向、量级一致（差在第四位），属口径/四舍五入，不构成科学性错误。

### 绿-2 ｜面①｜ `docs/DERIVATIONS-P3.md:70` sag0 "≈0.066·hp_res" 与 `:74` 的精确换算两读

`:74` 用精确系数 0.79788 ⇒ 0.08012·ρ₁ = 0.0639·hp_res；`:70` 的 0.066 与 **D-02 台账原文逐字同源**（0.8165/0.79788 = 1.0234 ⇒ 0.08094·ρ₁ ≈ 0.066）。按终裁不得改、不重开裁决；仅登记同文档两处换算精度不一致（约 2%）。

### 绿-3 ｜面③｜ `results/audit/route1/e3_circumradius_scan.txt` 尾行保留路线1 旧口径读数

尾行 `max measured = 1.13233 ; 1.25 headroom = +10.4%` 与成稿"裕量 ≥20.0%"相反，但它是收编前的机器存档；`KEY_RESULTS.md:13` 已按 D-01 转记（该值改记为极冠叶对角常数 2/√π、外接半径取 1.0415）。按"存档 JSON/TXT 不改写"政策可不改；成稿（REPORT/README/DERIVATIONS）已全部按新口径表述——**route1 旧 1.1284 框架在成稿中无残留**（专项复核通过）。

---

## 四、已查无问题面

### ①科学性 —— 已查，无新增问题（不重开 D 系裁决、不重报 PASS 项）

逐式复算全部通过：`2/√π = 1.1283792`（REPORT_paper:87/:110、DERIVATIONS、REPORT_experiment:69 与 D-01 一致）；`0.1043885 = (π/3)(1−2√2/π)`（D-09；成稿三份 REPORT/README/DERIVATIONS/KEY_RESULTS 零 `0.104369` 残留）；`depth_needed = −log₂(3.15e-6/log₂10) = 7.97 ⇒ ≥9，生产 cpp=12`（D-02）；`1.25/1.0415 = 1.2002 ≥ 20.0%`、经纬格反例 4.516（D-01）；`0.6·ulp(1″) = 6.6e-14″ / 0.05″ = 1.3e-12`（投影预算）；`2.4″² = 1.35e-10 sr ⇒ 1.4e-16/1.35e-10 = 1e-6 ⇒ 3.3e-3/1e-6 ≈ 3300×`（接缝判据）；`pf=0.8 ⇒ pf²=0.64 ⇒ 亏 36%、1/0.64−1 = +56.25%、1.5625`（负例）；决策窗 `105538.14−105517.3 = 20.84″`、`211076.28514206142″ = √(π/3)·(180/π)·3600`（A-P3-09）；`1.265/2·300 = 189.75″`、`0.8165/0.79788 = 1.0234`。F&H 双锚定（D-03）：`REPORT_paper:46`（正文）与 §3.4 明写 1.4 与 1.3883 **均非** F&H 原文，`refs.md:23-26` 记"全文无 1.3883/1.4/k_corr"；VOS 注入幅度 `+3.947e-2` 落 D-11 区间 3.95%–12% 下沿、`0.1189` 中位标"族依赖"，`REPORT_paper:131` §5.6 按 D-11 只给区间不引单值。k_corr：`k_gauss(N)×k_geo`（D-08 终裁名）、冻结 1.4 低估 32%/约 2 倍、1.3883 定位"标定几何单次 MC"，三份成稿一致。判据非退化证据齐备：`π/(4N²)` 错候选 metric=0.25、chart Jacobian 扰动 5e-4（恒 5.0e-4 判红）、lon-lat 反例 4.516、量化反方向 r→−1、原始指向负对照（25 控制源）、`route3 exp03` 恒 0 三守恒式。**未重报 PASS 项**（DRIZZLE.md:108 的 1.0415+20%、REPORT_paper:14 摘要 k_shape→k_gauss 均已带订正注记，本片复核仍成立）。

### ②行文逻辑 —— 已查，除黄-3/黄-4/黄-6 外无其他问题

订正注记闭合性抽查：`REPORT_paper.md:14`（k_shape 旧值+新值并列）、`:81/:83/:97/:111` 注、`DERIVATIONS-P3.md:62/:121` 注，均为"旧→新"两说齐备，无新旧冲突。UNRESOLVED 纪律：开放项（depth=12 收敛性、生产跨面切点分布、N=2/4 平局口径、Snyder/l'Huilier pinpoint、HST oracle 2.1e-04、接缝末环、T13-N2 恒等门）全部落在 `REPORT_paper.md:124-137` 诚实边界、`REPORT_experiment.md:104-113`、`EXP-07 §5/§7` 与台账 §4.2，**未混入结论节**；`REPORT_experiment.md:96-102` 五条结论均可回溯到上文表格；`REPORT_paper:136` 明写"UNRESOLVED 事项均不进入正文结论"。三份成稿"一句话结论"数值互不矛盾（8.2e-15、3.3e-12、36×、3300×、1.0415、0.1043885）。历史正本与订正后的判读分工有明说（`REPORT_experiment.md:89`「历史正本结论维持」、`:29`「与台账无冲突数值，维持原文」），除黄-4 单点数字外成立。`REPORT_paper.md:116` 标题「## 4.【链条位置】专节」经复核**不是**未填模板——其下内容即链条位置（上游接口/下游消费/精度约定），且该【链条位置】标题样式与 `absolute-snr:25`、`photometric-magnitude:84`、`dense-snr-reconstruct:21` 同款，不列问题。

### ③跨文档冲突 —— 已查，除红-1/黄-1 外口径全部一致

与 L1/权威链对数一致：`docs/science/DRIZZLE.md:108`（1.0415、margin ≥20.0%）、`docs/science/algorithms/DRIZZLE_GEOMETRY.md:121-137/:431`（按 D-01，旧 1.0442/19.7% 标为纬度扫描读数保留对照）、`独立审计/08_修复包/④面积交叠与分配/02_…:49/:94/:412` 与 `05_正向规格.md:150/:621`（外接半径、depth 8→12＋矢高 ≥9、亏缺律 0.1043885、单位 ρ₁、量化方向、gnomonic 区间、VOS 口径）逐项与本单元成稿同值。生产代码侧一致：`spherical_overlap.cpp:42` `HP_CIRCUMRADIUS_FACTOR = 1.25`、`:859` `WCS_ADAPTIVE_MAX_DEPTH = 12`（成稿引 `:857` 者，`:857` 注释内容即 `// max_depth = 12`，与跨文档检查既往"内容相符（±3 行）"判例一致，不列问题）、`:1001-1002` 注释 `θ < 1e-3 rad 时偏差 < 4e-8` 仍为 A-P3-04 待订正旧值——`REPORT_paper:81`「代码注释 "<4e-8" 确认错误并须订正」、`REPORT_experiment:60`「注释 "<4e-8" 错（实测 ~4.2e-7）」与简报 P3 待办第 5 条状态一致（**如实登记未完成**，与红-1 的"声称已完成"不同，不列问题）、`orchestrator.cpp:198` 自动 nside 用正确常数。`docs/science/algorithms/HEALPIX_MAPPING.md`（55 行）为算法卡（输入/输出/前置/不变量/复杂度/参考文献），`grep 1.25|外接|1.0415|0.10438|depth` 零命中 ⇒ 不含 P3 面积口径主张，无冲突面。五单元简报 P3 段的"定量主数/链条接口数值/豁免清单/UNRESOLVED 处理"与本单元三份成稿逐项同值（仅其待办第 1 条与红-1 冲突，已记红）。

### ④幻觉与锚 —— 已查，除红-1/红-2、黄-1/黄-2/黄-5/黄-7/黄-8 外全部核验通过

- **文献题录抽验（Crossref / arXiv 实取）7 条**：Górski 2005 `10.1086/427976` → ApJ 622, 759-771 ✓；Fruchter & Hook 2002 `10.1086/338393` → PASP 114, 144-152 ✓，arXiv `astro-ph/9808087` 题名 "Drizzle: A Method for the Linear Reconstruction of Undersampled Images" ✓；Van Oosterom & Strackee `10.1109/TBME.1983.325207` → IEEE Trans. Biomed. Eng. 1983 issue 2 ✓；Sutherland & Hodgman `10.1145/360767.360802` → Commun. ACM 1974 issue 1 ✓；Calabretta & Greisen `10.1051/0004-6361:20021327` → A&A 1077-1122 ✓；Calabretta & Roukema `…12297.x` 卷页✓题名✗（黄-7）；Fernique `arXiv:1505.02937` 题名 "MOC - HEALPix Multi-Order Coverage map Version 1.0" ✓。refs.md V1–V7 的 DOI/arXiv 与"标注级"表（l'Huilier 书目级、Snyder pinpoint UNRESOLVED、Kahan、Turner 2006 标未证实、C&G 1995 记"错误 DOI 已更正"）与台账口径一致，未见杜撰。
- **file:line 锚逐条开文件核验（20+ 条）**：`p1_rootcause.cpp` T2/T4/T7（:6/:8/:111/:202/:225）、`p9_review.cpp` T23/T27/T28/T29（:2/:54/:90/:127）、`p2_algorithms.cpp` T9、`e1_leaf_area_and_scale.py` 第 6 节尺度常数与 :202 起第 7 节（归档 `e1…txt:23` 往返 1.110e-16 ✓）、`DRIZZLE.md:98` → `spherical_overlap.cpp:40-42`（声明在 :42，:40-41 为同段注释，内容相符）、`DRIZZLE_GEOMETRY.md:236`（失准，黄-5）；报告中所有 `[实验:…]` 脚本名与 `results/audit` 文件名均真实存在（run_all.sh:27-51 调用清单逐一比对）。
- **"文档说行为/配置、代码没接"专项**：`WCS_ADAPTIVE_MAX_DEPTH = 12` ✓ 接通；`HP_CIRCUMRADIUS_FACTOR = 1.25` ✓ 接通；1.25 缓冲三层（quick-reject / query 3.0·hp_res / fast 枚举）✓ 与生产注释一致；auto-nside 真值常数 ✓ 接通（`orchestrator.cpp:198`、`drizzle_engine.cpp:709-710`）；REC-1 三处改动确为**伪代码未落地**且成稿明说（`README.md:55`「修复方案以伪代码形式写在 docs/EXP-07-POLAR.md §6.1，落地属于单独任务」；`REPORT_experiment.md:112` 把 leafmax_err 三口径限制列入"落地前须重算"），不算幻觉；**两处未接通且被声称已完成/可用者 = 禁抄值 5 处（红-1）与复现链路（红-2）**。
- **归档输出与成稿数字抽对**：`p9_review.out:2-3`（use_fast=1 → 376/1089、2.517e-02；use_fast=0 → 0/1089、3.823e-14）、`:44`（接缝中点 n=289，生产V0 280/289、1.7489e-04）、`:17`（0.04″/px 生产V0 最坏 3.317e-03）、`:47`（极点 生产V0 = 复刻，|Δ|/A_drop = 1.149e-13）；`p1_t7.out:6`（1.106e-13）；`p1_t2.out`（弦偏差 0.0639 hp_res）；`p3_t11.out`（极冠 d=0 弦偏差 0.06389 hp_res、`u+v=1` 交界 0.000206、三角点 0.000000）；`p2_t9.out`（49 档三预算，1.1220e-08@1e-7）；`exp03_weight_conservation.json:43/:81`（ang_over_hp_res 0.594113、Σw−1 = 0.0）；`direct_char.json:43`（0.911254）、`g3b_n5_hiprec.json`（0.9113）；`e1/e3/e4 json`（尺度常数 −1.9749e-4、往返 1.110e-16、方差二次律 9.000000、1.16e-5 → 4.4e-16）；`exp08_scale_constant.json`（0 ulp、窗宽 20.84、−1.97e-4）；`exp06/exp04`（1.0415、4.516）；`e6/exp04`（VOS +3.947e-2、l'Huilier 2.98e-7）——除黄-2/黄-4 所点两处外，逐一相符。
