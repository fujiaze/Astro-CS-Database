# 检查-S11 · standards / development / quality / operations / diagnostics 全量排查

- 切片：S11 = docs/standards/（14 份标准 + checks/check_standards_registry.py）+ docs/development/（CODE_STYLE、CONFIG_SCHEMA、TESTING）+ docs/quality/（complexity_baseline_v1、coverage_baseline_v1）+ docs/operations/TOOLCHAIN_AGENT_HOST + docs/diagnostics/TROUBLESHOOTING，共 21 份文档。
- 方法：纯静态只读（未编译、未跑测试、未执行任何检查器）；21 份文档逐份全文通读；约 60 处 file:line 锚逐条实开文件核对；配置死键专项（defaults.json / filters.json / stage2 fence 三向）；与《检查-修复验证》PASS 表、《分歧台账》D-01…D-11 及 §2 A-* 裁决做去重（NUMERIC:87 D_p² 等已判合法项不重报）。
- 判级：红 = 必须改（其中科学公式/默认容差/冻结口径类只登记证据、不给改法，上呈裁决）；黄 = 应改；绿 = 登记备改。
- 计数：红 5、黄 19、绿 2（共 26 条）。

---

## 红

### 红1 ｜ 红 ｜ docs/standards/NUMERIC_STANDARD.md:45-48 ｜ 权重偏差公式与同句实测值相差约 26×，科学数值两说（上呈，不给改法）｜ 面①

- 问题：:46 自报「α 实测跨 5.28× ⇒ α² 跨 27.84×」，:47 随即写「标度未声明时归一化权重最大相对偏差**应为** `max_k (α_max/α_k)² − 1`；**实测 1.0336**」。按其自带公式与自带 α 跨度代入：max 出现在 k=α_min，值 = 5.28² − 1 ≈ 26.9（与其 :46 的 α² 跨度 27.84 同量级），与同句「实测 1.0336」相差约 26 倍；:48 的 ΔE=0.2184 亦无法从两者同时导出。
- 证据（含反方核验）：原文见 :45-49（数字与公式同句绑定，非分句可脱钩）；反方核验——若把 1.0336 解释为「归一化权重另一量（如 p_k/⟨p⟩−1）」，则 :47 的「应为 … − 1」公式句必须同时改错，两者仍不能同真；α 上下界 [1.1387e-17, 6.0083e-17] 比值 5.276、α² 比值 27.84 与 :46 数字自洽（即矛盾不在 α 数据，在公式—实测绑定处）。证据文件 `run/RELEASE-05/...`（:45 指）为 run/ 瞬态，未复算其内部。
- 建议改法：不给（科学公式/数值类，上呈负责人裁决：要么订正 :47 公式句、要么核对 1.0336 的量定义）。

### 红2 ｜ 红 ｜ docs/standards/NUMERIC_STANDARD.md:108-109 ｜ DISP-DRZ-004 状态与注册表正反两处及代码行为三方矛盾（上呈，不给改法）｜ 面③④

- 问题：NUMERIC:108-109 称「`DISP-DRZ-004`「NaN 经 F_p 传播、不掩膜」为 **TRACKED/OPEN** 偏差，见 STANDARDS_REGISTRY D.drizzle 偏差表与 §3 索引」。
- 证据（含反方核验）：STANDARDS_REGISTRY.md:182 偏差表行 = 「DISP-DRZ-004 | **高** | … | **CLOSED**：*」；:270 §3 索引行 = 「DISP-DRZ-004 | … | 第 5 行 | **CLOSED** | —（口径 = 样本级掩膜，rule_id NAN-SAMPLE-MASK-COVERAGE-NAN…）」——正本两处均 CLOSED，无 TRACKED/OPEN。行为面：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:2040-2063` 为样本级掩膜 + `rejected_nonfinite_value/variance` 计数（:2241-2245 聚合），与「NaN 经 F_p 传播、不掩膜」相反。反方核验——检索 D-01…D-11 与 A-* 裁决均无对 DISP-DRZ-004 状态的翻案记录；NUMERIC:110-111 自称「本文件、DATA_SEMANTICS §2a、STANDARDS_REGISTRY D.drizzle 三处必须逐字同口径」⇒ 其自身声明被 :108 违反。
- 建议改法：不给（涉及冻结 NaN 口径的状态定性，上呈：确认以注册表 CLOSED + 现行掩膜行为为准后，NUMERIC:108 的整句如何处置）。

### 红3 ｜ 红 ｜ docs/standards/STANDARDS_REGISTRY.md:156 / :267 / :268 vs :169 ｜ 同注册表 drizzle 核两说：A_pixel 形式与 A_drop 形式并存（上呈，不给改法）｜ 面②③①

- 问题：drizzle 归一核的写法在同一注册表内出现两个互斥版本。
- 证据（含反方核验）：① :156 CLAUSES 字段「§3（线性重建与权重 **w_jp=a_jp/A_pixel**）」；② :267 §3 索引「§3（线性重建 **w_jp = a_jp / A_pixel** 与面亮度语义）」；③ :268 DISP-DRZ-009 CLOSED 行「§3（… **w_jp = a_jp / A_pixel** …）| … | —（面亮度保持口径 **w=a/A_pixel**）」。反面：:169 同域清单行条款名 = 「§3（线性重建 **w_jp = a_jp / A_drop** 与面亮度语义）」，且与科学正本一致——docs/science/DRIZZLE.md:21/:45/:87/:224 规定 `w_jp = a_jp/A_drop,j`，:129/:158 明文把 `a_jp/A_pixel,j` 写法判红（通量偏 pixfrac²）。反方核验——:87 的 D_p² 等价参数化已被 PASS 表判合法（不涉此条，此条是分母 A_drop vs A_pixel，非方差传播式）；:169 行内 DISP-DRZ-009 处置语亦写「正向核 w=a/A_drop 配分母 N_p、禁用把正向分母换掉」，与 :268 的「面亮度保持口径 w=a/A_pixel」直接互斥。
- 建议改法：不给（冻结核口径，上呈裁决哪一版是注册表正字面）。

### 红4 ｜ 红 ｜ docs/development/CONFIG_SCHEMA.md:35 ｜ fence 键 `upm_weight_source` 代码不存在、注记自相矛盾（文档说有、代码没接）｜ 面④②

- 问题：fence 行 `support_power(1.0) robust_loss(huber) **upm_weight_source(snr2_normalized)**  # 原 snr_weight_mode，（已按 §9.73 A44 作废：键不存在；权重是派生量）`——键名、旧名、作废注记三者错配。
- 证据（含反方核验）：① 代码唯一接受的键是 `snr_weight_mode`：`lib/algorithms/coverage/src/stage2_common.cpp:163-167` `m.value("snr_weight_mode", "snr2_normalized")`（并对非默认值硬报错）；消费点 `upm.cpp:1396/:1540`、声明 `upm.h:73` 同键。② `upm_weight_source` 在 lib/eng 全量 grep = **0 命中**（仅文档自身及 docs 内转述）。③ 注记声称「upm_weight_source 原 snr_weight_mode、已作废键不存在」——事实相反：snr_weight_mode 是**现行活键**，upm_weight_source 才是不存在的键；且 A44 的作废对象是 `integration.weight_mode`/`legacy_allow_weight_fallback`（stage2_common.cpp:447-450 拒收面可证），与本行无关。④ 反方核验：stage2 model 段无 unknown-key 拒收（nlohmann value/contains 语义）⇒ 读者按文档写 `upm_weight_source` 会被静默忽略、拿到默认值，不报错——死文档键成立。
- 建议改法：把 fence 键名改回 `snr_weight_mode(snr2_normalized)`，删除错挂的 A44 作废注记（A44 注记只保留给 weight_mode 行）；同步清理域外 5 处同型残缺句（docs/contracts/PUBLIC_API.md:2208、DATA_SEMANTICS.md:1969/2079/2178、docs/plugins … PHASE2_SESSION:131、phase2_upm:168 中「upm_weight_source（原 snr_weight_mode，）」样式，域外移交见文末）。

### 红5 ｜ 红 ｜ docs/standards/LOGGING_DIAGNOSTICS_STANDARD.md:5 + docs/diagnostics/TROUBLESHOOTING.md:25 ｜ 运行日志落点与最高设计 §7.3 硬要求及 :723 条款冲突（需裁决统一口径）｜ 面③

- 问题：标准写「日志**统一**写 `run/logs/<module>/<YYYYMMDD>/`」，排障第一步同款路径；与最高设计两处硬条款冲突。
- 证据（含反方核验）：docs/ASTROCS_DESIGN.md:554-557（§7.3 运行日志·**硬要求**）「每次运行…在块级 `output_dir` 下产出完整运行日志，日志目录默认 `<output_dir>/logs`（**日志落点的唯一来源 = 块级 output_dir**）」；:723「产品与**运行日志**只落块级 `output_dir`（§7.3）；`run/` **只放开发/CI 过程产物与过程日志**，与运行日志不互替」。实现侧跟随文档：`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:573` `config_.log_dir = "run/logs/orchestrator"`、:1966-1967「运行产物统一写 run/logs/」。反方核验：AGENTS.md §7 末「日志一律落 run/<task>/logs/」、IO_STANDARD:23「运行产物统一 run/」——即仓内「run/ 派」有三处、「L0 output_dir 派」是最高权威两处，**链内本身自相矛盾**，非措辞可两全；「日志」对象（stage 运行日志 vs 开发过程日志）在 LOGGING:5 的「统一」二字下无区分。
- 建议改法：上呈裁决（若维持 L0：标准侧须区分「运行日志→output_dir/logs」与「过程日志→run/」，且实现日志落点属域外需一并处置；若 L0 条款另有解释，须在 §7.3 或标准侧写明豁免口径）。

---

## 黄

### 黄1 ｜ 黄 ｜ docs/standards/API_STANDARD.md:16-18 ｜ 三个示例 API ID 在所指两份登记表中 0 命中 ｜ 面④

- 证据（含反方核验）：`API-AIO-001`、`API-P2-REJECT-001`、`API-P2-UPM-001` 全仓（docs/lib/eng + csv/json）grep 仅命中审计台账与 findings_base（记录同一缺陷），在 `docs/contracts/API_CONTRACTS.csv`（id 形态 = `API-<符号>`，首行 `API-aio_hcsd_read`）与 `docs/traceability/TRACEABILITY_MATRIX.json`（id 形态 `API-ABI-001`/`API-P2-001` 族，无 AIO/REJECT/UPM 变体）均 0 命中。已见 01_文档 台账 DB-10-补 登记，工作树未修。
- 建议改法：示例改为登记表真实 ID（如 `API-aio_hcsd_read`），或登记这三个 ID。

### 黄2 ｜ 黄 ｜ 12 份标准 :3（API/BENCHMARK/C_ABI/CODE/COMMENT/CONCURRENCY/DOCUMENTATION/ERROR_HANDLING/IO/LOGGING/RELEASE/TEST，及经 DOCUMENT_INDEX 上游登记的 STANDARDS_REGISTRY） ｜ 上游标「§8.4（模块与 ABI）」号名错位 ｜ 面④

- 证据（含反方核验）：docs/ASTROCS_DESIGN.md:645 = `### 8.4 顶层结构`、:688 = `### 8.5 模块与 ABI`（12 份标准 :3 均写 §8.4（模块与 ABI），括题与 §8.5 相符、号是 §8.4；仅 NUMERIC_STANDARD:3 标 §8.5 正确）。DOCUMENT_INDEX.yaml:1436 对 STANDARDS_REGISTRY 亦登记 `upstream: docs/ASTROCS_DESIGN.md §8.4（模块与 ABI）`。01_文档 台账 DB-10 已集体登记，未修。
- 建议改法：统一改 §8.5（各文档主题真实上位按台账 DB-10 建议），并同步 DOCUMENT_INDEX。

### 黄3 ｜ 黄 ｜ docs/standards/CODE_STANDARD.md:6 / :11 ｜ 引用不存在的 `docs/ASTROCS_DESIGN §10.2` ｜ 面④

- 证据：docs/ASTROCS_DESIGN.md 全文无「10.2」（§10「I/O 与原子产品」:711 起无子节）；被引原文「Windows 官方工具链为 MSVC。」实为 :736、属 §11。反方核验：其所述内容本身正确（ENGINEERING_SPEC §1「MSVC v143（Windows）/ GCC 或 Clang（Linux）」逐字相符），错的只是锚。
- 建议改法：§10.2 → §11（或改为 ENGINEERING_SPEC §1 单源引用，删重复锚）。

### 黄4 ｜ 黄 ｜ docs/standards/RELEASE_STANDARD.md:7 ｜ 根目录登记规则引 AGENTS §6，实为 §7 ｜ 面④

- 证据：AGENTS.md §6（:116-133）是硬禁令清单，无「根目录条目先登记」；该规则在 §7 目录落位速查末（:161「根目录固定条目见 ENGINEERING_SPEC §7；新根目录条目先登记并经负责人确认」）。同句 `ENGINEERING_SPEC.md §7` 半边正确。
- 建议改法：AGENTS.md §6 → §7。

### 黄5 ｜ 黄 ｜ docs/standards/NUMERIC_STANDARD.md:3 / :13 / :15 ｜ 「§3.3:185」行锚失效（实文 :266）｜ 面④

- 证据：docs/ASTROCS_DESIGN.md「每个科学量写清五件事…」现于 :266（实开核对）；:185 是 §2.2 句「并经 §12.3 的独立审稿」。01_文档台账曾记 :239，行号再漂——裸行锚无防漂机制。
- 建议改法：去掉行号改指节号（§3.3），或由检查器维护行锚。

### 黄6 ｜ 黄 ｜ docs/standards/NUMERIC_STANDARD.md:30 / :31 ｜ scale-class 表两处代码锚与所述内容完全无关 ｜ 面④

- 证据：:30 称 `module_adapters.cpp:5648-5653` = 「帧级噪声节点 p1_op_noise 的消费面」——实开 :5645-5656 为 WCS 拟合注释块（P1-PHOT-BROKEN），p1_op_noise 定义在 :6752；:31 称 `module_adapters.cpp:6457-6460` = 「drizzle data/variance 块」——实开 :6455-6462 为 `noise.patch_grid`/`clip_sigma` 解析。反方核验：两行号区间在文件内存在（非越界），是**内容错位**而非行漂微偏。
- 建议改法：重锚到真实消费点（p1_op_noise :6752 起；photoapplied 路径 :1545 起），或删行锚改符号名引用。

### 黄7 ｜ 黄 ｜ docs/standards/STANDARDS_REGISTRY.md:252/:254/:257/:260/:263/:264/:272/:274 ｜ §3 索引「注册表清单行 第 N 行」8 处指向不含该 ID 的清单行 ｜ 面②④

- 证据（逐条实开清单行偏差列）：DISP-P3PROJ-001→spherical 第 3 行（:65 偏差列=「无（…）」）；DISP-HIPS-001→hips 第 7 行（:103 列内=DISP-HIPS-009）；DISP-HIPS-004→第 5 行（:101 列内=DISP-HIPS-005）；DISP-HIPS-007/010→第 3 行（:99 列内=STD-F4）；DISP-HIPS-011→第 5 行（:101 仅 HIPS-005）；DISP-DRZ-006→drizzle 第 1 行（:168 列内=DISP-DRZ-003）；DISP-DRZ-008→第 4 行（:171 列内=「无（9003 例…）」）。对照正确例：HIPS-009→第 7 行 ✓、HIPS-005→第 5 行 ✓、DRZ-003→第 1 行 ✓（证明编号规则本身可读、只是这 8 个指向未在该行登记）。
- 反方核验（机器面）：C7 只查「行存在 + 该行域/条款真实 + 域 DEVIATION 字段含 ID」（check_standards_registry.py:19-21 C7 文案），不查该行**偏差列是否列了 ID**；C9 只覆盖清单列→DEVIATION 反向。故 8 处可整体绿，违反 §3.1 登记纪律（:401-403 声明的登记一致性）。
- 建议改法：或把 ID 补进所指清单行偏差列、或把「第 N 行」改指实际含该 ID 的行；并给 C7 加「所指行偏差列含该 ID」断言。

### 黄8 ｜ 黄 ｜ docs/standards/STANDARDS_REGISTRY.md:249 / :253（声明 :401-403） ｜ §3 状态列越出冻结域 {TRACKED,CLOSED}，且所称机器校验不存在 ｜ 面②④

- 证据：:249 STD-F1 状态 = 「**CONFORMANT（…）**」、:253 STD-F4 = 「**OPEN（下一轮域任务）**」；:402-403 冻结声明「§3 偏差索引的 `状态`（`TRACKED`/`CLOSED`）= 偏差处置登记，**定义域与索引一致性由 C6/C7 现场判**」。反方核验：check_standards_registry.py 全文 grep `TRACKED|CLOSED` = **0 命中**——C6/C7 从未校验状态列取值；且 CONFORMANT 是「符合状态」轴词、OPEN 不属于任一声明域，两行同时混淆两轴（:401 明言两轴独立）。
- 建议改法：:249/:253 状态改填 TRACKED/CLOSED（或按处置语义改）；:403 的机器合同要么落实（C7 补状态域断言）要么删去。

### 黄9 ｜ 黄 ｜ docs/quality/complexity_baseline_v1.md:10 / :25 / :30-33 ｜ 基线口径含两个不存在的测量根 + 死锚 + 不可执行的对齐句 ｜ 面④

- 证据：:10 `--paths lib,cli,include`——仓根无 `cli/`、`include/`（glob 实核）；`eng/ci/checks.json` DEEP-COMPLEXITY 实为 `--paths lib`；check_complexity.py 文档串明示 fail-closed「--paths 里任一测量根不存在即 exit 1…COMPLEXITY_MISSING_INPUT」——即**按本文档跑必红**。:25 引 `cli/commands.cpp` 为死路径。:30-33「数值应与本基线一致」——基线口径含死根，不可执行。反方核验：检查器侧已修（GAP-027 fail-closed 在位），错在文档口径未随迁。
- 建议改法：:10 改 `--paths lib` 与 checks.json 对齐、删 :25 死路径、:30-33 注明数值口径仅对 lib 根有效。

### 黄10 ｜ 黄 ｜ docs/quality/coverage_baseline_v1.md:18-21 ｜ 产物路径与驱动描述只覆盖 PY 腿，scope 却含 DEEP-COV-CPP ｜ 面④

- 证据：:20-21 产物 `run/ci/coverage/coverage.xml…` vs checks.json 实参：DEEP-COV-PY → `--output-dir run/ci/coverage-py`、DEEP-COV-CPP → `deep_ci_driver.py coverage-cpp --output-dir run/ci/coverage-cpp`（实核命令数组）；:18-19 驱动只写 `ci_coverage_runner.py`（=PY 腿），:6 scope 却列 DEEP-COV-CPP。反方核验：:13 `threshold: null`、:19 cov 参数、:22-23 `prerequisite_tools=["pytest"]` 与 runner/checks.json 相符（这三处无问题）。
- 建议改法：产物行拆成 coverage-py/coverage-cpp 两路径；驱动行补 DEEP-COV-CPP = deep_ci_driver.py coverage-cpp。

### 黄11 ｜ 黄 ｜ docs/development/TESTING.md:9 / :15 / :26 ｜ 不存在的测试二进制、坏运行命令、无源外部编号 ｜ 面④

- 证据：:9 `v13_synth_test.exe` 全仓（lib/eng/docs 的 CMake/cpp/py/md）0 命中（仅本文自身）；:26 命令 `py -3.12 lib/algorithms/coverage/build/phase2_synthetic_gate.exe`——`lib/algorithms/coverage/build/` 不存在（目标 `phase2_synthetic_gate` 是 CMake gtest 可执行文件，CMakeLists.txt:103，产物在构建树）、用 Python 启动器跑 C++ exe、`.exe` 为 Windows 产物；:15 Hipsgen oracle「205625/205627」全仓 0 命中且无项目/版本标注。反方核验：:8 `lib/infrastructure/aio/tests/*` ✓ 存在（hips_mapping_oracle.py 等）、:11 toolchain.ps1 ✓、CTest 发现前缀 phase2_synthetic_gate.* ✓（CMakeLists:182-183）——非整篇虚构，是 3 个点位失实。01_文档 DB-11:434 已记其写法违规，未记存在性。
- 建议改法：删或补 `v13_synth_test` 实体；运行命令改为 ctest/CMake 目标调用；oracle 编号补出处。

### 黄12 ｜ 黄 ｜ docs/operations/TOOLCHAIN_AGENT_HOST.md:8/:28/:38/:44/:60 ｜ 快照与自身证据源多处对不上，无 as-of 标注 ｜ 面④③

- 证据：① :44 policy SHA256 `7c66e1a5…` vs 实测 `sha256sum eng/ci/toolchain.policy.json` = `2fb81389…`，lock 内嵌 `policy_file_sha256 = 650986a3…`——三个哈希互不相等（policy mtime 9-24 晚于文档 9-21）；② :28/:60「missing 9 项全部登记于 lock 的 missing_tools」vs 现 lock `missing_tools = ['ccache']`，且 `command -v` 实测 cmake/ninja/gcc/g++/clang/make/pytest 全部在位（lock 记 cmake 3.31.6、gcc 14.2.0、inventory_source 标注 V81-ADOPT-004 post-install 重采）；③ :8 盘点基准 SHA `98c2354f…` vs lock `git_head_sha = 52e71804…`；④ :38 AGENTS 行引文「执行与控制节点/经远程节点执行/离线不阻塞」AGENTS.md grep 0 命中（AGENTS 实文 :39 仅「正式平台 Windows x64 与 Linux amd64…」）。反方核验：DOCUMENT_INDEX.yaml:1042 登记 status = ACTIVE_INFORMATIVE、notes「V81-ADOPT-004 实测快照」——但正文无 as-of/过期标注，表格以现在时陈述「本机可用/缺失」。
- 建议改法：全文加「快照时点（生成 UTC + 锁定 SHA）+ 后续已刷新」标注并更新四处数字，或降级为带日期归档件。

### 黄13 ｜ 黄 ｜ docs/development/CONFIG_SCHEMA.md:102 / :104-105 ｜ 排异档位权威指错 + 「内核同值」锚反证自己 ｜ 面③④

- 证据：:102 写「生产科学路由**唯一权威** = docs/ASTROCS_DESIGN §5.5：1≤N≤3 none / 4≤N≤5 percentile / N≥6 winsorized（M3：原 N≥16 linear fit 档改投）」——ASTROCS §5.5 实文（:449）明言「逐像素档位表（档界与算法名）的**唯一正本 = docs/plugins/algorithms_phase2/12_rejection.md §9**（本节不复制档界与取值）」，§5.5 无任何档界数字。:104-105「内核同值见 rejection.cpp:**1139** kPixelSmallNPolicy」——实开 :1138-1141 注释为**四档**映射「6≤N≤15→winsorized / N≥16→**linear_fit**」，与 :103 三档句相反；kPixelSmallNPolicy 枚举实际在 :1144-1146；生产三档的实现在 astrocs_n_map_method/resolver（:1264-1278「**N≥16 → winsorized（M3）**」）。反方核验：档位**数值**与正本 §9 三档一致 ✓（12_rejection §9 表 + M3 注 + 对照档仍 linear 语义 ✓），故错的是出处与锚，不是档界本身。
- 建议改法：:102 出处改指 12_rejection §9（§5.5 为转授权）；:104 锚改 astrocs_n_map_method 决策点（或 :1264-1278），避免引四档注释自证。

### 黄14 ｜ 黄 ｜ docs/diagnostics/TROUBLESHOOTING.md:13 ｜ oracle ID `ALG-DRZ-CAND-001` 全仓无此号 ｜ 面④

- 证据：`grep -rn "ALG-DRZ-CAND-001" docs lib eng` 仅本文 1 处；仓库实际候选枚举 oracle ID = `TEST-DRZ-CAND-001`（docs/contracts/TEST_MATRIX.md:40 关联 candidate_oracle_test.cpp）。反方核验：同表其余指针实存——:17 `p2_validate_candidate_weights`（module_adapters.cpp:28/:11368/:11942）、:12 `docs/modules/gaia_xpsd_client.md` ✓、:20 CACHE_POLICY = docs/architecture/CACHE_POLICY.md ✓（DOCUMENT_INDEX:335 收录）。
- 建议改法：ID 改 `TEST-DRZ-CAND-001`（或补登记 ALG- 族号）。

### 黄15 ｜ 黄 ｜ docs/development/CONFIG_SCHEMA.md:25 ｜ fence 键 `patch_radius_leaf` 非 JSON 键名（死文档键）｜ 面④

- 证据：fence `model: control_grid_per_tile(8) patch_radius_leaf(2) …`；JSON 侧键 = `patch_radius_pixels`（stage2_common.cpp:44-70 `m.value("patch_radius_pixels", …)` → `cfg.patch_radius_leaf`）；model 段无 unknown-key 拒收 ⇒ 按文档名写配置会被静默忽略。反方核验：默认值 2 两侧一致 ✓、`patch_radius_leaf` 是 C++ 字段名（stage2_common.h）——即文档把**字段名**当**键名**写。同段其余键（control_grid_per_tile/min_samples/support_power/robust_loss/…）逐一在 parser 命中 ✓。
- 建议改法：fence 改 `patch_radius_pixels(2)`，或注明「键名 / 字段名」两列。

### 黄16 ｜ 黄 ｜ docs/development/CONFIG_SCHEMA.md:58 ｜ 一注释盖两键，把活键 acr_route 标成作废 ｜ 面②

- 证据：:58 `weight_mode(auto) acr_route(cpu/auto)   # （已按 §9.73 A44 作废：键不存在；权重是派生量）`——weight_mode 确已死（stage2_common.cpp:447-450 `in.contains("weight_mode")` 硬错误 ✓ 注记对它成立）；但同句 `acr_route` 是活键（:466-468 `in.value("acr_route","auto")` 仅收 auto/cpu）。反方核验：:58 注记仅一句、无键名限定 ⇒ 读者会把 acr_route 也当死键丢掉（而 fence 上一行与 B4-28 注释均要求它可用）。
- 建议改法：作废注记只挂 weight_mode（拆行或写明「weight_mode 已作废」），acr_route 行改为现状说明。

### 黄17 ｜ 黄 ｜ docs/standards/DOCUMENTATION_STANDARD.md:5-9 ｜ 转录 §0 权威链漏 ACCEPTANCE_SPEC ｜ 面③

- 证据：:5「权威链（§0，唯一）：docs/ASTROCS_DESIGN → AGENTS → ENGINEERING_SPEC → CONTROL_PACK_SPEC → docs/ci/ → docs/plugins/」；ASTROCS §0 mermaid（:15-22）节点序为 ①本文→②AGENTS→③ENGINEERING→④CONTROL_PACK→**⑤ACCEPTANCE_SPEC**→⑥docs/ci→⑦docs/plugins（ACC 为链上一环）。同文 L0 框（ASTROCS/AGENTS/ENGINEERING/CONTROL_PACK 四件）同样缺 ACCEPTANCE_SPEC。反方核验：science/algorithms/UNIFIED_MODEL 三个旁挂权威该文均有列（:9）✓，漏的恰是链上唯一一件。
- 建议改法：链补 `ACCEPTANCE_SPEC`（CONTROL_PACK 与 docs/ci 之间），L0 清单同步。

### 黄18 ｜ 黄 ｜ docs/development/CODE_STYLE.md:5 / :13 ｜ 工具链与警告策略两说，且版本号无任何登记源 ｜ 面③④

- 证据：:5「C++17；**MSYS2 MinGW64 g++ 16.1.0**」、:13「警告策略：**-Wall -Wextra**」被写为风格正文；CODE_STANDARD:10-13 明确「正式 toolchain：Windows = MSVC v143；Linux = GCC 或 Clang；**MinGW64 定位 = 本地开发/兼容性验证**」，ENGINEERING_SPEC §1 同。版本 `16.1.0` 全仓 grep 仅本文件命中（toolchain.lock 无 g++、TOOLCHAIN_AGENT_HOST 记 agent 主机 g++ missing、hosted CI 为 gcc-14、toolchain.ps1 只写 MSYS2 路径无版本）。反方核验：`.clang-format`/`.editorconfig` 首行确为「V14 G5」✓（:6 的 V14 说法有据），故仅 :5/:13 两点。
- 建议改法：:5 按 CODE_STANDARD 分层表述（正式/本地），版本号删除或落到 lock/ps1 可核处；:13 注明 GCC 旗标适用域（MSVC 正式侧对应 /W4 策略另写）。

### 黄19 ｜ 黄 ｜ docs/standards/STANDARDS_REGISTRY.md:169 ｜ `DRZ-FLUX-FIX-01` 在清单偏差列，但不在注册表任何定义域 ｜ 面②④

- 证据：:169 偏差列含 `DRZ-FLUX-FIX-01（口径订正…）`；grep 注册表全文仅这 1 处——不在 D.drizzle DEVIATION 字段（:159 仅 DISP-DRZ-001..009）、不在 §3.2 跨域表、不在 §3 索引；其定义仅见 `docs/validation/v6/QA_MATRIX.md:323`（注册表 C6 闭包 = 域偏差表 ∪ §3.2 ∪ 外部 findings 登记册，QA_MATRIX 不在其中）。反方核验：C9 只收 `STD-F*/DISP-*` 词元 ⇒ 该号形态恰落在检查器盲区，可整体绿；违反 §1.5/§3.1 自述的引用闭包纪律。
- 建议改法：在 D.drizzle DEVIATION 字段或 §3.2 登记该号（并挂 QA_MATRIX 指针），或改写为已有 ID。

---

## 绿

### 绿1 ｜ 绿 ｜ lib/algorithms/coverage/include/astro/phase2/stage2_common.h:23 / :53-54 ｜ 源码注释指 CONFIG_SCHEMA 行号漂移 ｜ 面④

- 证据：:23 注 `CONFIG_SCHEMA.md:19`（该 token 现在 :33）、:53-54 注 `CONFIG_SCHEMA.md:3-4`（上游标头现 :5-6）。内容均在、行号微偏。建议下轮顺手改。

### 绿2 ｜ 绿 ｜ docs/standards/IO_STANDARD.md:19 ｜ 引 `findings.csv` 文件名与仓内实名差一字 ｜ 面④

- 证据：仓内无 findings.csv；ID `F-V19R2-IO-001` 在 `独立审计/批次清单/findings_base.csv` 3 处命中（指针可解析）。另 :18 该 finding 现状描述（aio_upm_write_sparse 直接 trunc 写）与 S6/backlog 挂账一致、未见反证。建议文件名写全。

---

## 已查无问题面

### 面① 科学性（除红1 外）

- 抽样方法：NUMERIC_STANDARD 全 131 行逐条重算 + CONFIG_SCHEMA 全 115 行 fence 键逐一比 parser + 与分歧台账 D-01…D-11/§2 A-* 逐条避让。
- 结果：① NUMERIC 公式重算通过——线性标度律 Var(x′)=α²·Var(x) / ivar(x′)=ivar(x)/α²（:38，GUM §5.1.2 式(10) 线性特例一致）；A_cell=4π/(12·2^18²)=1.5239e-11 sr、1/A_cell²=4.306e21（21.63 dex）复算 ✓（:54/:59/:93 三处同值）；float32 最小正规数 1.1755e-38、次正规判据 ✓（:69）；:87 D_p² 为 PASS 表判「合法等价参数化」，不重报。② 排异档位：CONFIG_SCHEMA:102-103 三档 = 正本 12_rejection §9 三档 = resolver 代码（N≥16→winsorized M3）✓；min/max 不用于生产有拒收面（rejection.cpp:1288-1296）✓。③ 非退化负例在位：NUMERIC:60（A_cell=1 归零）、:71（α=1 例）、:44（α=1 度量恰 0）✓。④ 默认容差无两说：CONFIG_SCHEMA tolerance 1e-6 / variance_floor 1e-12 与 stage2_common.h 默认一致；GATES 分层门（1e-4/1e-8/1e-6 px、κ_iter·τ）各自标源、未见与 docs/science 冻结值冲突的第二版本。⑤ D 系列裁决零触碰（D_p²、F&H 权重来源、k_gauss 记号等均按 PASS 表回避）。

### 面② 行文逻辑（除红3、黄7、黄8、黄16、黄19 外）

- 抽样方法：21 份全文通读，专查「同文两说 / 断链 / 订正注闭合 / UNRESOLVED 充当结论」。
- 结果：① 订正注闭合——ERROR_HANDLING:22「原引 DIAGNOSTICS_STANDARD 仓内不存在，已订正为实际文件名」实核正确（LOGGING_DIAGNOSTICS_STANDARD 存在）✓；CONFIG_SCHEMA:93-94 V17 删除说明与 parser 硬错误面一致 ✓。② 无任何一份以「UNRESOLVED/待定」作为结论收口（STANDARDS_REGISTRY 偏差一律 TRACKED/CLOSED 具名披露，除黄8 两行越域）。③ 引用清单抽查：CODE/BENCHMARK/C_ABI/COMMENT/IO/RELEASE 的「关联/引用」所列文件全部实存（各标准互链、API_CONTRACTS.csv、TRACEABILITY_MATRIX.json、astrocs_diagnose.py 等）✓。

### 面③ 跨文档冲突（除红2、红3、红5、黄12、黄13、黄17、黄18 外）

- 抽样方法：以 ASTROCS §0/§3.3/§5.5/§7.2/§7.3/§8.4/§8.5/§10/:723/§11/§12.4 为节点，对 21 份文档的上游标与实质条款双向核对，再下沉 checks.json / defaults.json / DOCUMENT_INDEX 三个注册面。
- 结果：① ERROR_HANDLING 三层语义与 ASTROCS §7.2 退出码表（0/2/3/4/5/6/7/8）及 rc 单一语义一致 ✓。② CODE_STANDARD:5/:10 正式工具链与 ENGINEERING_SPEC §1 逐字一致 ✓。③ TESTING/CONFIG/CODE_STYLE 上游标「§8（软件架构）」标题存在 ✓（内容承载问题在 01_文档 另册）。④ registry 注册面：checks.json 含 STD-REG（跑本片检查器）、DEEP-COMPLEXITY、DEEP-COV-CPP/PY、CON-CONFIG-CONTRACTS、CON-COMMENTS（→eng/tools/quality/contracts/check_comments.py，COMMENT_STANDARD:46 的「run_checks.py 注释卫生检查项」可解析）、DOC-INDEX；STANDARDS_REGISTRY 在 DOCUMENT_INDEX:1433 = ACTIVE_NORMATIVE（C1 可过）；14 份标准 + development/quality/operations/diagnostics 条目全部登记在册。⑤ 与代码一致：C_ABI rc 语义、排异拒收面、acr_route 取值域、stage2 其余 fence 键 vs parser 全部相符 ✓。⑥ check_standards_registry.py C1–C9 与注册表 §5 合同逐条对齐（含 C5′/C9、退出码 0/1/2、负向注入 verdict 恒 0），未发现合同-实现偏差。

### 面④ 幻觉与锚（除黄1-6、黄9-12、黄14、黄15、黄18、黄19、绿1、绿2 外）

- 抽样方法：约 60 处 file:line 锚逐条实开（含越界核对：drizzle_engine.cpp 全文 2634 行，被引 >2634 行的锚即判越界）；config-dead-key 三向专项（CONFIG_SCHEMA fence ↔ stage2 parser ↔ C++ 字段；defaults.json ↔ 全仓消费者；filters.json ↔ 消费者）；测试/工具引用路径逐一 stat。
- 结果：① defaults.json field_count=59 与实际 59 键相等，逐键 grep lib/cli/eng 均有消费者，**0 死键**；filters.json 有消费者 ✓。② stage2 fence 其余 17 键全部在 parser 命中（control_grid_per_tile/min_samples/support_power/robust_loss/tolerance/max_irls_iterations/acr_route/…），死键仅红4、黄15 两处。③ 工具/测试引用实存：CMakePresets cmakeMinimumRequired 3.31.0 + presets version 10 + 三 preset 名 ✓、CMakeLists 3.24/C++17 ✓、verify_toolchain.py ✓、astrocs_diagnose.py ✓、toolchain.ps1 ✓、aio/tests 目录 ✓、p2_upm_open 符号 ✓。④ 第三方（nanoflann/cfitsio/nlohmann）仅接口语义核对（nlohmann `m.value`/`contains` 未命中键不报错），未展开实现。⑤ NUMERIC:43 证据 json（run/SCI-FIX-SEMANTICS-01/…）实存 ✓；NUMERIC:67 M42 平面证据为 run/ 瞬态未复算（红1 独立成立、不依赖该文件）。

---

## 域外移交（本片验证到、但文件不在 S11 范围）

1. docs/contracts/DATA_SEMANTICS.md:371 把「NaN 经传播」写为现行行为并挂失效代码锚，与 drizzle_engine.cpp:2040-2063（掩膜+计数）及 DRIZZLE_GEOMETRY:339 相反——与红2 同根，需 contracts 域处置。
2. docs/science/algorithms/DRIZZLE_GEOMETRY.md:43 `w = a_jp/A_pixel,j` 与其 :344 及 DRIZZLE 正本相反——与红3 同根，需 algorithms 域处置（D 系列裁决面外）。
3. upm_weight_source 残缺句 5 处：docs/contracts/PUBLIC_API.md:2208、DATA_SEMANTICS.md:1969/2079/2178、docs/plugins … PHASE2_SESSION:131、phase2_upm:168——随红4 一并清理。
4. ASTROCS §7.3/:723 与实现（orchestrator run/logs）的冲突治理面属最高设计/代码域（红5 的另一半）。
