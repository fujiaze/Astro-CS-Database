# AUD-403 · 注释与 README / 开发者文档审计报告

审计节点：AUD-403（独立只读审查）。冻结基线 HEAD=`c8f64e9a`。
范围：`lib/**/*.cpp|*.h|*.hpp`（排除 `lib/third_party/**`，868 个跟踪源）、`eng/**/*.py|*.sh`、CMake 注释；根 `README.md`、`lib/**/README*`、`eng/**/README*`、`docs/development/**`、`docs/{TROUBLESHOOTING,DEVELOPER_GUIDE,VERSIONING}.md`、`docs/owner/RELEASE_STATUS.md`、`DEPENDENCIES.md`、`FATDUCK_ACCESS.md`、`VERSION`、`docs/DOCUMENT_INDEX.yaml`。
判据：标准 04 §5（注释）、§6（README 与开发者文档）；标准 01 §4（全文档集写法：只写现行、无元信息块、无日期/任务编号/commit/历史叙事）。
纪律：未跑构建/ctest/run_checks/任何检查器或剥离脚本（负责人裁决本阶段测试暂停，且 `v19r*_strip_comments.py`、`check_comment_hygiene.py` 会覆写跟踪件）。所有普查脚本写在工作区外 `F:\Astro dev\独立审查\_audit403_scratch\`，只读仓库。

字段固定：编号 / 对象（路径:行 + 该行内容锚）/ 权威依据 / 现状 → 应为 / 改法 / 置信 / 取证命令。

---

## AUD403-01 · 注释与现行实现直接矛盾（additive_mode 生产默认）

- **对象**：`lib/infrastructure/scheduler/src/module_adapters.cpp:9820`
  锚：`⇒ 生产默认 additive_mode = "c"（本文件 :5503 起），δ 从不施加；`
- **权威依据**：标准 04 §5「注释与代码同步更新…不保留与现行实现矛盾的注释」；§5「注释不含已撤销设计的历史叙述」。
- **现状 → 应为**：现状：该注释块（`CONFORM-FIX-B-011` 依据段，9817–9824）声明生产默认 `additive_mode="c"`、δ 从不施加；但同一函数 14 行后 `module_adapters.cpp:9834` `§9.67 定案 1：默认 "delta"`、`:9836` `seam_pre.value("additive_mode", std::string("delta"))`，且 `:10312` `seam_cfg.value("additive_mode", std::string("delta"))` 一致——**现行生产默认是 "delta"（δ 默认施加），9820 的断言已为假**。应为：删除 9817–9824 依据段里"生产默认 c / δ 从不施加"的失效陈述，只保留现行正向条款（`additive_mode ∈ {delta(默认)|c|both}`，语义见 10298–10301）。
- **改法**：删 9820 整行及其上"生产默认 additive_mode=c"依据句；`additive_mode` 语义已在 `:10298` 就地写清，无需重复；`CONFORM-FIX-B-011 / FIX-SCI-SNR-CANON-001 / §9.54 / §9.67 / FIX-A / reports/RELEASE-02/…` 等控制包章节与轮次引用一并清除（改法见 AUD403-02）。
- **置信**：CONFIRMED（代码默认值 `"delta"` 与注释 `"c"` 逐行对读；两默认点 9836/10312 一致）。
- **取证命令**：`grep -nE 'additive_mode.*value|默认 "delta"|生产默认 additive_mode' "F:/Astro dev/Astro CS Normalization Database/lib/infrastructure/scheduler/src/module_adapters.cpp"`

---

## AUD403-02 · 注释内任务编号 / 缺陷流水号 / 控制包章节 / 日期 / commit / 历史叙述（普查规模与典型位置）

- **对象（典型，非穷举）**：`lib/infrastructure/scheduler/src/module_adapters.cpp` 单文件承载最多脏注释：
  - `:2` `P1-001 (attempt 2): … ARCH-P0-001`（计划/任务编号 + "attempt 2" 过程痕迹）
  - `:16` `2026-09-20 订正 [V5 分片 5 / R-2]: 旧文 hp_drizzle_run 已作废`（日期 + 轮次 + 历史叙事"已作废"）
  - `:3242` `⚠ 注释订正 (P2, 2026-09-14): 本节点检测器是…（d3af6ffa 引入）…误述源自 9e09941a 的提交信息`（日期 + 轮次 + **两个 commit 短 sha 写进注释**）
  - `:8005` `「写进 (*man)」曾经等于「写进内存后丢弃」`（历史叙事"曾经"）
  - `:11405` `原实现读整数 doc["weight_mode"]∈{1,2}`、`:11007` `原实现 value_stride=sizeof(double)`、`:12468` `G3-12：原实现以整数 weight_mode…`（"原实现"历史叙事 + 任务号）
- **权威依据**：标准 04 §5「注释中的任务编号、缺陷流水号、控制包章节引用清理」「注释不含已撤销设计的历史叙述」；标准 01 §4「正文无日期、无负责人某日裁决、无任务编号、无 commit」。
- **现状 → 应为**：现状：仓库注释（尤其 `eng/ci`、`eng/tests` 的 docstring 与 `lib/infrastructure/scheduler`、`lib/algorithms/*`）大量内嵌 `FIX-xxx`/`RELEASE-0x`/`GATE-xxx`/`EXP-xxx`/`DOC-xxx`/`P1-00x`/`WIRING-AUDIT-01`/`V81-ADOPT-006` 等流水号、`控制包任务 …`/`负责人 §9.67 定案` 等章节裁决、`2026-09-xx` 日期、commit sha、"原实现/旧口径/作废/曾/留痕"历史叙事。应为：注释只留**现行**的"为什么"（物理依据、算法取舍、边界、非显然约束），把过程/裁决/轮次迁到 `artifacts/evidence/` 或变更 claim；行号锚（`module_adapters.cpp:1995`、`:5503`）删除（已随代码漂移失效）。
- **改法**：以 `eng/tools/quality/v19r4_strip_comments.py` 声明的轮次剥离面（`R0x-xxx`/`GAP-0xx`/控制包 SHA/轮次 token）为**下限**扩规则，覆盖 `FIX-`/`RELEASE-`/`GATE-`/`EXP-`/`DOC-`/日期/commit sha；且必须扩到 Python docstring 与 `#` 注释（现有剥离器只处理 `lib/**/*.cpp|.h` 的 `//`，见 AUD403-03）。逐条改写为正向"为什么"句，保留指向 `docs/science|algorithms` 的锚与 `实验/` 指针。
- **置信**：CONFIRMED（确定性普查 + 逐条对读；数字见覆盖率自报）。
- **取证命令**：
  `python3 "/f/Astro dev/独立审查/_audit403_scratch/census.py" < "/f/Astro dev/独立审查/_audit403_scratch/code_files.txt"`
  → `CATEGORY_COUNTS … TASK_ID=455 CONTROL_REF=70 DEFECT=64 HISTORY=241 DATE=170`；
  `grep -rnE '//.*(原实现|旧口径|作废|曾[写经]|留痕)' lib/ | wc -l`

---

## AUD403-03 · 注释卫生门的覆盖面缺口（在册门窄于自述，eng/ Python 注释无门）

- **对象**：`eng/tools/quality/contracts/check_comments.py`（在册门 `CON-COMMENTS`，注册于 `eng/ci/checks.json:2350`，随 `CHK-STALE-DOC` 步骤跑）
  锚：文件头 `"""check_comments.py — T406 comments checker … Checks: 禁止过期审计轮次、旧版本宣称、**代码复述**、错误线程/单位"""`；但 `_scan()`（62–91）实际只判两件事：(a) 字面 `V19R2/V19R3`，(b) `lib/algorithms/**` 内不变量注释需带 SCI-/ALG- 锚。
- **权威依据**：标准 04 §5、§6；AGENTS.md §9「门禁/判据本身不合理时改进门禁本身」。
- **现状 → 应为**：现状：① 该门自述禁"代码复述/错误线程/单位"，实现里一条都没判（文档-实现口径不符，属"承载判据但退化为空"）；② 扫描面写死 `lib/**/*.cpp|.h|.hpp`，`eng/**/*.py`（承载本工包最高密度的 `FIX/RELEASE/控制包/owner=…/tasks/02` 编号）与 `lib/**/*.cu` 完全不在门内；③ 覆盖面更广的 `check_comment_hygiene.py` 已**自宣退役**（其头注 12 实测"真仓库 171 条命中 / 71 文件"），并把欠账清单指向 `run/DEFECT-REPRO-01/COMMENT_HYGIENE_INVENTORY.md`——`run/` 是 gitignore 临时区，该留痕不可长期追溯。应为：把轮次/FIX/RELEASE/GAP/控制包/日期/commit-sha 判据并入在册门、扫描面扩到 `eng/**` 与 `.cu`，退役脚本的欠账清单落 `artifacts/evidence/` 而非 `run/`。
- **改法**：不放松判据、扩面即可；对 `check_comments.py` 补 `--self-test` 正负例（注入一条 `eng/*.py` docstring 里的 `FIX-9` ⇒ 必红）。
- **判准（不串轴）**：`check_comment_hygiene.py` = **已退役**（自我声明，无参 exit 2）；`check_comments.py` = **已接线但覆盖面窄**，非"不存在"、非"未接线"。
- **置信**：CONFIRMED（读 `check_comments.py` 全文 + `checks.json:2350` 注册 + `check_comment_hygiene.py` 头注退役声明）。
- **取证命令**：`grep -n "CON-COMMENTS" eng/ci/checks.json` ; `sed -n '9,16p;62,91p' eng/tools/quality/contracts/check_comments.py`

---

## AUD403-04 · 版本与状态口径单一事实源（值一致；两处副本 + 违禁元信息块）

- **对象**：
  - 版本值：`VERSION:1`=`0.1.0-alpha.1`；`CMakeLists.txt:15` `project(acsd VERSION 0.1.0 …)`（`:55` `file(READ …/VERSION)`，alpha 后缀取自 VERSION）；`docs/owner/RELEASE_STATUS.md:58/65/74`=`0.1.0-alpha.1`；`docs/DOCUMENT_INDEX.yaml:38` `base_product_version: "0.1.0-alpha.1"`；根 `README.md` 不写死版本号（`:107-108` 指 VERSION 为唯一源）。
  - 违禁元信息块：`docs/owner/RELEASE_STATUS.md:149-154`（`authoring_task: GOV-004` / `authoring_owner: SA-GOV-01` / `base_main_sha: caee3e67…`40hex / `convergence_task: DOC-CONV-001` / `convergence_run: Rmtxvlrtfa66eb7 (rev23)` / `convergence_base_sha: da3c4b4a…`40hex）；`docs/DOCUMENT_INDEX.yaml:32` `# 重写：DOC-403（2026-09-21，基线 HEAD d1ccfa1bed9c）`、`:36-38`（`version: "2.0.0"` / `authoring_task: "DOC-403"` / `base_main_sha: "…"`40hex）。
- **权威依据**：标准 04 §6「版本与状态口径以唯一事实源为准」；标准 01 §4「文档开头无元信息块…正文无日期、无任务编号、无 commit、无 sha」。
- **现状 → 应为**：现状：产品版本号**取值在全部 5 源一致**（`0.1.0-alpha.1`；README/VERSIONING 正确不复制字面量，CMake 数字三元组由 `eng/ci/check_version.py` 规则校验、非漂移）——单源结论成立。但 `0.1.0-alpha.1` 字面量被**复制**进 `RELEASE_STATUS.md`（4 处）与 `DOCUMENT_INDEX.yaml`（1 处），升版需手工同步（次级单源风险）；两份"唯一事实源"文档自身**违反标准 01 §4**（尾部/头部元信息块含 authoring_task/convergence_run/commit sha/日期）。状态口径未见跨轴串用：`README.md:99-101`→`§12.5` 为交付状态唯一词表，`RELEASE_STATUS.md:8-11` 声明"状态词清单唯一登记处=§12.5"，`DOCUMENT_INDEX.yaml:8-11` 明确其 `status` 字段「**不是** §12.5 交付状态阶梯」（文档活动分类另成轴）——三处口径自洽。应为：状态/索引文档不复制产品版本字面量（改引用"取自根 VERSION"），删除两处 authoring/convergence/sha/date 元信息块。
- **改法**：RELEASE_STATUS/DOCUMENT_INDEX 的版本行改为"= 根 `VERSION`（派生）"不留具体数字；元信息块整块删除，证据锚（commit）落 `artifacts/evidence/` 由正文引用（此二者属文档写法，主责在 AUD-101，本条登记版本/状态轴现状供其引用）。
- **置信**：CONFIRMED（5 源取值逐一 grep 对齐 + 读三份文档头尾 + CMake 派生链）。
- **取证命令**：`for f in VERSION README.md docs/VERSIONING.md docs/owner/RELEASE_STATUS.md docs/DOCUMENT_INDEX.yaml; do echo "$f:"; grep -noP '\d+\.\d+\.\d+(-alpha\.\d+)?' "$f"; done`

---

## AUD403-05 · 排障手册（常见问题）指向不存在的命令与禁用工具链

- **对象**：`docs/TROUBLESHOOTING.md`
  锚（"常用命令"段，32–44 行区）：`lib\snr_estimator\cpp\test\noise_model_science_test.exe`、`lib\healpix_db\healpix_drizzle\tests\variance_propagation_test.exe`、`lib\phase2\build\phase2_synthetic_gate.exe`、`py -3.12 tools\astrocs_diagnose.py run\logs --json diag.json`；表首行修复：`$env:Path="C:\msys64\mingw64\bin;$env:Path"; .\eng/build/toolchain.ps1 check`。
- **权威依据**：标准 04 §6「构建步骤、依赖、平台要求、常见问题在开发文档中写清」；标准 01 §1「下位文档与上位冲突以上位为准，冲突本身是缺陷」；`DEPENDENCIES.md:25/82/87`（Windows 正式工具链为 MSVC，`MinGW/MSYS2` 属禁面、`machine_absolute_path: FORBIDDEN`）。
- **现状 → 应为**：现状：① 四条测试命令的源树路径 `lib/snr_estimator`、`lib/healpix_db`、`lib/phase2`、`tools/astrocs_diagnose.py` 在本基线**全部不存在**（现行分别是 `lib/algorithms/noise_snr/`、`lib/algorithms/drizzle/healpix_drizzle/`、`lib/algorithms/integration/v6|coverage/`；诊断脚本 `astrocs_diagnose.py` 全域不存在，功能已由 `acsd doctor` 承载，见 `README.md:80`）；且把可执行写成"源目录内 .exe"，与"唯一根 CMake→build 树"的构建模型不符。② 首行让 Windows 开发者把 `C:\msys64\mingw64\bin` 加进 PATH，而 `DEPENDENCIES.md` 明列 MinGW/MSYS2 为禁面，并含硬编码机器绝对路径 `C:\msys64`（同为禁面）。应为：命令改指现行 `build/` 产物名与 `acsd doctor`；DLL 加载失败的正解走 MSVC 运行库/`toolchain.ps1 check`，删除 msys64/mingw64 PATH 建议与绝对路径。
- **改法**：逐条以 `git ls-files` 复核路径存在性后再写命令；表内"证据/阶段"列的任务码（`E100/E200/E980/CALIBRATE/PLATESOLVE/DRIZZLE`）如为现行日志域可留，若为旧轮次代号则订正。
- **置信**：CONFIRMED（四条路径 `git ls-files` 零命中 + `DEPENDENCIES.md:25/82/87` 禁面 + `README.md:80 acsd doctor` 对读）。
- **取证命令**：`for p in lib/snr_estimator lib/healpix_db lib/phase2 tools/astrocs_diagnose.py; do git ls-files "$p" | grep -q . || echo ABSENT $p; done; grep -niE 'mingw|msys|machine_absolute' DEPENDENCIES.md`

---

## AUD403-06 · 开发者指南与模块 README 完整性门覆盖面单薄

- **对象**：
  - `docs/DEVELOPER_GUIDE.md:11-14`（构建仅给 Linux `cmake -S . -B build -G Ninja` 两条，无 Windows 构建序列、无 `acsd` 运行样例）；`:24-31` 模块测试矩阵用 `SNR-001..015`/`PR-UPM-001..010` 等编号（目标名本身仍有效，已核：`phase2_synthetic_gate` 143 处、`variance_propagation_test` 14 处引用）。
  - 完整性门 `eng/tools/check_module_readmes.py`（DOC-003）：`MODULES` 列表硬编码**仅 5 个**模块（`star_detection/wcs/photometry/noise 的 wrapper_phase1 + phase3_session`），其余 `lib/algorithms/*`、`lib/infrastructure/*` 的 README 不在该门覆盖内。
- **权威依据**：标准 04 §6「每个库与关键模块有必要的说明…」「构建步骤、依赖、平台要求…写清」。
- **现状 → 应为**：现状：DEVELOPER_GUIDE 对"新读者"偏薄（Linux-only 构建、无运行/Windows 面——但 DEPENDENCIES/toolchain.ps1/windows README 已补齐 Windows 工具链，属**分工**非缺陷）；模块说明的实际质量不缺（见 AUD403-10），但**"完整性由门保证"这一层**只覆盖 5/26 顶层子模块，且 5 个 depth-1 目录（`lib/algorithms/shared`、`infrastructure/{benchmark,hips_browser,observability,pipeline}`）无同址 README（部分由子目录 README 覆盖）。应为：DOC-003 门扩到全部具名模块目录，或明确"哪些模块以子目录/父 README 覆盖"并在门里枚举。
- **改法**：DEVELOPER_GUIDE 补一段指向 `DEPENDENCIES.md`（Windows preset 构建）与 `acsd` 运行；门 MODULES 列表改为动态枚举 `lib/*/*/`（与 `docs/modules/` 对齐），补正/负例。
- **置信**：PARTIAL（覆盖面为事实；"是否算缺陷"取决于是否接受子目录/父文档替代，建议 AUD-101 与架构文档 `MODULE_MAP` 合并裁决）。
- **取证命令**：`grep -nE 'MODULES *=|\("lib/' eng/tools/check_module_readmes.py | head` ; `for d in $(git ls-files 'lib/algorithms/*/*' 'lib/infrastructure/*/*' | sed -E 's#(lib/[a-z]+/[^/]+)/.*#\1#' | sort -u); do git ls-files "$d/README*" | grep -q . || echo NO-README $d; done`

---

## AUD403-07 · 平台角色口径张力：Linux 是发行平台还是轻验证面

- **对象**：`README.md:41`「正式平台为 Windows x64（acsd.exe 与 .dll）与 Linux amd64（acsd）」 vs `DEPENDENCIES.md:27`「Linux 控制节点（轻验证；**非发布产物**）」、`:38-39`「Linux preset 仅供静态检查/轻量编译…Linux 性能不作为 Windows 发布性能结论」。
- **权威依据**：标准 01 §1（下位与上位冲突以上位为准，冲突即缺陷）；`ASTROCS_DESIGN.md §11` 发行表把 `Windows 10+ amd64 | acsd.exe` 与 `Linux amd64 | acsd（唯一 ELF + .so）`**并列为发行形态**。
- **现状 → 应为**：现状：最高设计 §11 与 README 把 Linux amd64 列为**正式发行平台**（各一套交付形态）；DEPENDENCIES 把同一 Linux 描述为"非发布产物/轻验证控制节点"。二者对"Linux 是否发行"表述不一致，新读者在 README↔DEPENDENCIES 间得到冲突答案。应为：以 §11 为准澄清——"Linux 是发行平台，但当前开发/CI 节点用 `linux-control` preset 只做轻验证，正式 Linux 发布产物面尚未产出"（区分"目标形态"与"当前实现状态"两条轴，避免把"未开工"写成"非发布"）。
- **改法**：DEPENDENCIES §27 标题与正文补一句限定"（指当前控制节点用途；Linux amd64 发行形态见 ASTROCS_DESIGN §11，属未产出的发布面）"。此为平台状态轴措辞问题，非代码缺陷。
- **置信**：PARTIAL（两处文字口径确为不一致；是否需上位裁决属负责人/AUD-101，按任务纪律不把"未开工"判为"已坏"）。
- **取证命令**：`sed -n '703,710p' ASTROCS_DESIGN.md; sed -n '27p;38,39p' DEPENDENCIES.md; sed -n '41p' README.md`

---

## AUD403-08 · 根 README 完整性判定：六要素齐备、链接零悬空（正向结论 + 轻微前瞻注记）

- **对象**：`README.md`（全 130 行）。
- **权威依据**：标准 04 §6「根 README 面向新读者：项目是什么、三个命令、科学目标、构建与运行、仓库布局、文档往哪读」。
- **现状**：逐项判：**项目是什么** 有（`:1-4`）；**三个命令** 有且与 §8.1 同口径（`:26-42` 表）；**科学目标** 有，四创新点含 `§2.1-§2.4` 且指向 `docs/science|algorithms` 正本（`:44-62`）；**构建与运行** 有，命令与 `AGENTS.md §3` 逐条一致（`:64-84`）；**仓库布局** 有（`:86-97`）；**文档往哪读** 有索引表（`:113-131`），**45 个被引路径 `git ls-files` 全部命中，零悬空**。版本口径正确不复制字面量、指向 `VERSION` 单源（`:107-108`）。判定：满足标准 04 §6，无需整改的核心缺陷。
- **轻微注记（非缺陷）**：① 构建块是 Linux 命令形态（`python3`/`cmake -G Ninja`），Windows 面靠 `:66` 指向 DEPENDENCIES——分工可接受；② `:31-32` 把 `lib/phase{1,2,3}_session` 记为现行会话层目录，与标准 04 §1"归并后独立会话目录退役"的**目标**相反，但目录当前仍在位，README 描述的是现状非错误——留待架构整治（AUD-401）时同步。
- **置信**：CONFIRMED（链接存在性逐条核 + 与 AGENTS/DESIGN 对读）。
- **取证命令**：`grep -oP '`[^`]+\.(md|yaml|json|py|sh)`|docs/[A-Za-z0-9_./-]+' README.md | tr -d '`' | sort -u | while read p; do [ -e "$p" ] || echo MISS $p; done`（无输出 = 零悬空）

---

## AUD403-09 · 注释总体在讲"为什么"，真缺陷是"为什么"被过程流水号包裹（对任务书前提的校正）

- **对象**：整体判定，样本见 `weight_chain.h:34-49`、`phase1_product.cpp:294-302`、`module_adapters.cpp` 多处、`scheduler/README`。
- **权威依据**：标准 04 §5「注释解释为什么…；任务编号/缺陷流水号/控制包章节引用清理」；§5「不保留与现行实现矛盾的注释」。
- **现状 → 应为**：现状：抽样未见"只复述代码在做什么"的空注释成灾——恰恰相反，注释承载了大量非显然约束（F&H 2002 §7.2 依据、MAD→σ 常量 `1.482602218505602`、逐帧 F_ref 同源性、`additive_mode` 语义、WCS 初值来源规则），"为什么"不缺。缺陷集中在：这些"为什么"被 `负责人 GAP_AUDIT §9.67 定案 1`、`（P2, 2026-09-14）`、`commit 9e09941a`、`旧口径/原实现/作废/留痕`、`:5503/:1995` 行号锚**包裹**（标准 01 §4 明确禁止的写法）。真正与现行实现矛盾的仅定位到 1 处硬冲突（AUD403-01）；多数 `原实现/旧口径` 是**自我标注为旧**的历史叙事（值不矛盾、但违反 §4/§5）。应为：剥离过程叙事、保留正向"为什么"；把裁决/变更/实测迁 `artifacts/evidence/`。
- **改法**：以"删历史/过程句、留正向约束句"为改写模板；对每条保留的 `SCI-/ALG-` 设计锚，改写为指向 `docs/science|algorithms` 的路径锚（现有 `check_comments.py` 已认可两种形态）。
- **置信**：CONFIRMED（精读样本 + 普查计数）。
- **取证命令**：`grep -rnE '负责人.*§[0-9].*(定案|裁决)|\(P[0-9], 20|旧口径|原实现.*作废' lib/ | wc -l`

---

## AUD403-10 · 模块 README 质量：内容达标、夹带 owner 叙事与行号锚

- **对象**：`lib/infrastructure/scheduler/README.md:1-40`（样例）。
- **权威依据**：标准 04 §6「每个库与关键模块…理解模块职责与入口」；标准 01 §4（禁 owner 裁决叙事、禁源码行号）。
- **现状 → 应为**：现状：该 README 达标良好——写清职责（运行时内核 + 三 Phase 节点适配）、入口（`src/module_adapters.cpp` 的 `p1_op_*/p2_op_*/p3_op_*`）、权威锚（`docs/contracts/DATA_SEMANTICS.md §18.5`）、非显然约束（WCS 初值来源枚举、`206.265` 常量取舍、fail-closed）。但夹带：`:15` `负责人裁定：帧头 WCS 未授权`（owner 叙事应转正向条款"帧头 WCS 不作为任何解算输入"）、`:23` `ipv_select.cpp:57 IPV_…=206.265`（源码行号，删 `:57`）、`:30` `审计登记（F-10）`（任务码）、`:32` `旧值 header_crval 已移除`（历史叙事，改"传 header_crval 即 DATA 拒绝"正向句）。
- **置信**：CONFIRMED（逐行读）。
- **改法**：把 owner 裁定改写成现行约束句；行号→符号名；`F-10`/轮次码删；"旧值已移除"→正向拒绝条款。
- **取证命令**：`sed -n '13,32p' lib/infrastructure/scheduler/README.md`

---

## 附：待清编号 → 保留指针 分类表（供 AUD-601 直接引用）

| 类别 | 代表 token / 样例位置 | 量级（注释行，普查） | 处置 | 依据 |
|---|---|---|---|---|
| 任务/计划编号 | `P1-001`/`P2-007`/`P3-006`、`ARCH-P0-001`、`GATE-501`、`WIRING-AUDIT-01`、`V81-ADOPT-006`、`T406`、`W4-A3`、`CI-001/002/007`、`CFG-001`、`LNX-005`、`WIN-009`、`owner=SA-CI-32`、`tasks/02` | 与下行合入 TASK_ID=455 | **待清**（删或改写为功能描述） | 标准04 §5 / 01 §4 |
| 缺陷流水号 | `FIX-201/402/203/207/208/401/405/406`、`DRZ-FLUX-FIX-01`、`CONFORM-FIX-B-011`、`bughunt_p1_batchI`、`bug 狩猎 R5`、`TODO/FIXME` | FIX-≈58 / DEFECT=64 | **待清** | 标准04 §5 |
| 轮次 / 控制包章节 | `V19R2/R3/R4`、`R07-M09`/`R04-B17`/`R9-A`、`GAP-0xx`、`RELEASE-02/04/05`、`控制包任务 …`、`§9.67 定案 1`、`负责人裁决`、`CONFORM-SWEEP-3` | RELEASE-≈129 / CONTROL_REF=70 | **待清** | 标准04 §5 / 01 §4 |
| 日期 | `2026-09-14`/`18`/`20`/`23` | DATE=170 | **待清** | 标准01 §4 |
| commit sha（注释/文档内） | `9e09941a`、`d3af6ffa`、`f86d60e2…`、`fb7f232a`；`RELEASE_STATUS:151/154` 40hex；`DOCUMENT_INDEX:38` 40hex | 散点 | **待清**（证据锚迁 `artifacts/evidence/`） | 标准01 §4 |
| 中文历史叙事 | `原实现`/`旧版`/`旧口径`/`作废`/`废止`/`曾`/`留痕`/`已撤销`/`订正` | HISTORY=241（留痕47/旧版38/作废26…） | **待清**（改正向现行条款） | 标准04 §5 / 01 §4 |
| 实验证据指针 | `实验/photometric-magnitude/docs/p1-spatial-gain.md` 等 | EXPT_POINTER=8（目标文件 `git ls-files` 命中） | **保留**（指向实验单元） | 标准04 §5 |
| 科学/算法设计 ID | `SCI-A/B/C`、`SCI-B D1`、`ALG-P3-xxx`、`ALG-COS-001` | SCI_ID=8 + ALG-* 若干 | **保留为锚**，宜改写为 `docs/science|algorithms` 路径 | 标准04 §5；`check_comments.py` 认可为合格权威锚 |
| 合同/条款 ID（分情形） | `DATA-001/002`、`DATA-P1-SOURCES`、`ABI-001`、`API-002`、`DATA-UNC-001` | 混合 | 若=已发布合同：保留**路径形态**（`docs/contracts/…`）、删裸号；若=工单号：待清 | 标准01 §3/§4 |
| 现行格式/组件版本 | `V6`（产品族合同名）、`aio_hips V1 子产品`、`FITS/HiPS/schema_version`、`hip 1.4`、`CFITSIO 4.6.4` | — | **保留**（非开发轮次；`check_comment_hygiene.py:14-18` 列为已知假阳性白名单） | 标准04 §5 |

---

## 覆盖率自报

- **普查（确定性，脚本在工作区外）**：扫描代码文件 1500 个（`git ls-files` lib C/C++〔排除 third_party〕+ eng/**/*.py + eng/**/*.sh + CMake，`code_files.txt` 去重 1505，third_party 过滤后 1500）。空清单守卫已验证（`printf "" | python3 census.py` → exit 3）。命中分类（**注释区提取后**，首个匹配类去重）：`TASK_ID=455 / CONTROL_REF=70 / DEFECT=64 / HISTORY=241 / DATE=170 / SCI_ID=8 / EXPT_POINTER=8 / SHA=215`；`SHA=215` 绝大多数为假阳性（`1.482602218505602` 常量、`0000000000001139` 符号地址、`20260902` seed 等纯数字命中 hex 正则），真 commit-sha-in-comment 经收紧正则复核为散点（`9e09941a`/`d3af6ffa`/`f86d60e2…` 等，见分类表）。待清类合计命中 **1000 行 / 455 文件**。分目录（注释行）：`lib`〔TASK_ID 362 / HISTORY 177 / DATE 136 / DEFECT 54 / CONTROL_REF 29〕，`eng`〔TASK_ID 93 / HISTORY 64 / DATE 34 / CONTROL_REF 41 / DEFECT 10〕。
- **精读**：逐行读约 20 个对象——`module_adapters.cpp`（6 个窗）、`weight_chain.h`、`rejection.cpp`、`phase1_product.cpp`、`calibrator/cosmetic_corrector`、`check_comments.py`〔全文〕、`check_comment_hygiene.py`〔全文〕、`v19r4_strip_comments.py`〔全文〕、`check_module_readmes.py`〔头〕、`README.md`〔全文〕、`VERSIONING.md`、`RELEASE_STATUS.md`、`DOCUMENT_INDEX.yaml`〔头〕、`DEPENDENCIES.md`、`DEVELOPER_GUIDE.md`、`TROUBLESHOOTING.md`、`scheduler/README`、`VERSION`。
- **README/开发文档批（实测）**：根 README 1 + `lib/**/README*`（非 vendored）42 + `eng/**/README*` 13 + `docs/development/**` 3 + 具名 8〔VERSION/VERSIONING/TROUBLESHOOTING/DEVELOPER_GUIDE/DEPENDENCIES/FATDUCK_ACCESS/RELEASE_STATUS/DOCUMENT_INDEX〕= 67 份实测在批（`FATDUCK_ACCESS.md` 仅登记未展开审，属操作面且涉凭据引用路径，非注释/README 缺陷面）。任务书所称"约 136 份"未在本基线复现——全仓 `README` 标题文件仅 83 份（含 vendored cfitsio 与 testdata/实验/artifacts），差异以本实测为准（见下节）。
- **待清编号条目分档**：高危害单文件 = `module_adapters.cpp`（116 脏注释行，含 1 处 live 矛盾）；中危害面 = `eng/**` docstring（`FIX/RELEASE/控制包/owner/tasks` 密集，门零覆盖）；广撒面 = `lib/algorithms/*` 的 `原实现/旧口径/EXP-xxx/DRZ-FLUX-FIX` 与 5 处 `RELEASE-0x`；保留 = 8 实验指针 + SCI-/ALG- 锚。

## 我证伪了任务书的哪个前提

1. **"抽一批'只复述代码在做什么'的注释"**——本基线的注释缺陷不是"复述/空注释成灾"，而是"过度承载过程流水号与裁决叙事"（AUD403-09）。真正与现行实现矛盾的硬冲突只逐一定位到 1 处（`module_adapters.cpp:9820`，additive_mode），余下 `原实现/旧口径/作废` 多为**自标注为旧**的历史叙事（值不矛盾、写法违规）。按"复述注释有多少"去立项会找错靶子。
2. **"README 批约 136 份"**——不可复现；全仓 README 标题文件 83，在批 67。以实测数报覆盖，未凑数。
3. **"注释卫生无人管 / 无纪律"**——前提不成立：仓库**自带**卫生工具（`v19r3/v19r4_strip_comments.py`、`check_comment_hygiene.py`）与在册门 `CON-COMMENTS`。真问题是门覆盖面窄、且只覆盖 `lib/**/*.cpp|.h` 的 `//`，`eng/**` 与 docstring 无门（AUD403-03）。

## 诚实边界

- 普查是**线索与覆盖率**，非"已审"：注释区抽取为启发式（C/C++ 抽 `//` 与 `/* */`、Python 抽 `#` 与三引号 docstring、URL 内 `//` 有跳过），可能漏抓跨行拼接注释、`--` 注释、误抓少量字符串内文本；分类"首个匹配类优先"会低估同一行多类命中。结论以精读样本与逐条 `grep` 取证为准，未把普查计数当已证。
- 未执行任何构建/ctest/run_checks/文档渲染/注释剥离或卫生检查脚本（负责人裁决测试暂停 + 部分检查器覆写跟踪件）。凡"门是否真红/真绿"的判据，我只读脚本源码与注册表推断覆盖面，未运行验证 → 相关条目置信至多 PARTIAL。
- `check_comments.py`/`check_comment_hygiene.py` 的 `--self-test` 正负例声明（"5 例 OK"、"171 命中/71 文件"）系脚本自述数字，我未跑，只作二手证据引用，不当已核。
- 版本/状态轴：确认 `0.1.0-alpha.1` 取值在 5 源一致、状态词两轴（§12.5 交付阶梯 / DOCUMENT_INDEX 文档活动分类）不互串；但 RELEASE_STATUS/DOCUMENT_INDEX 尾部元信息块、CMake `project(...VERSION)` 复制三元组的"是否构成第二事实源"，属文档整治裁决面（AUD-101），本条只登记现状。
- `docs/owner/RELEASE_STATUS.md`、`docs/VERSIONING.md`、`docs/DOCUMENT_INDEX.yaml` 的**全文写法合规性**（元信息块/日期/sha/历史）主责在 AUD-101；我仅就"版本与状态口径单一事实源"（任务项 5）取证，未做整篇文档风格审计。
- Windows 侧构建/发布面"尚未开始"按纪律**未判为缺陷**；Linux 发布面"未产出"同理（见 AUD403-07 措辞区分）。

<!-- PROGRESS: 10/10 -->
