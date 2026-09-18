# W2-ROOT — 第二波·文件级审计（根 tracked 文件 + scripts/ + run/ + third_party/ + modules/）

- 分片：ROOT-004 第二波 w2；口径依据 `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/SHARD_BRIEF.md` §1/§2。
- 开工基线：`git rev-parse HEAD main origin/main` 第 1 次 = `180c8a0/180c8a0/b4afc13`（不等）→ 等 2 分钟重试 = `59981aa/59981aa/b4afc13`（origin 仍不等）→ **按简报以 HEAD=main 相等为准开工并注明**（平行线持续提交所致）。扫描中钉证据于 `23f42ff`。
- 收工基线：`01754fab8618313bc39a4da3014e78cd0ad4e2a3`，HEAD=main=origin/main **三者一致**；本报告全部行号已在 01754fab 复核。补交轮（README/memory/HANDOVER/VERSION 四行）证据另钉 `b3167608b3bd4807c12700b95cd39a2d4b7beb81`（HEAD=main 相等；四文件对 HEAD 零 diff）。
- 域内工作树状态：仅 `CMakeLists.txt` 为 WIP 未提交改动（M）；其引用行号按 01754fab 索引面计。
- 纪律自证：零修复、零 git 写；除本报告与 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/w2_ROOT.log` 外未写任何路径；未读取 `FATDUCK_ACCESS.md` 内容（仅文件名计数）。
- 结论速览：文件域 39（根 19 + scripts 2 + run 1 + third_party 1 + modules 16）；发现 17 条 = P0×0 / P1×8 / P2×9。

## ① 发现表

| ID | 定位 | 违反的最新权威条款 | 当前证据（本轮真跑） | 严重度 | 影响 | 整改建议 | 建议文件域 | 验收门(单命令) | 同源标注 |
|---|---|---|---|---|---|---|---|---|---|
| W2-ROOT-1 | AGENTS.md:36；docs/ci/01_CHECKS.md:75,77,79；ci/run_checks.py | ENGINEERING_SPEC §8（ci/ 提供确定性执行器、检查注册可复跑）；ASTROCS_DESIGN §0（CI 规范为链上权威） | 命令：`git status --porcelain -- ci/run_checks.py; git ls-files ci/run_checks.py; git log -- ci/run_checks.py`；输出：`?? ci/run_checks.py` / 空 / 空（从未入库） | P1 | 权威链文首规定的本地必跑机器门入口在 fresh clone 中不存在；实 CI 走 ci/run.py，文档与仓面脱节 | 将 run_checks.py 薄封装入库并注册进 ci/checks.json，或三处权威文本改指 `ci/run.py --profile`（后者需负责人确认） | ci/ + AGENTS.md + docs/ci | `git ls-files --error-unmatch ci/run_checks.py; echo rc=$?` → 0 | 第一波未点名；与 GAP 无重复 |
| W2-ROOT-2 | ASTROCS_DESIGN.md:323 | ASTROCS_DESIGN §6.3 自身（退出码唯一源声明）+ ENGINEERING_SPEC §8（删除/重命名无悬空引用） | 命令：`test -f include/astrocs/exit_codes.h; echo rc=$?`；输出：rc=1（不存在）；实际枚举在 cli/exit_codes.h（且 cli/ 已登记 F2 待退役，owner=ARCH-001） | P1 | 最高权威声明的退出码唯一源悬空；lib 层无公共头可引用，退出码合同面失守 | 枚举迁至 include/astrocs/exit_codes.h、cli/ 转发；或负责人批准 DESIGN 修订改指路径（agent 无权改文档绕过） | include/astrocs + cli/ | `test -f include/astrocs/exit_codes.h; echo rc=$?` → 0 | 与第一波 AIO-10 同源不删条；归属 CLI-003/ARCH-001 |
| W2-ROOT-3 | CMakeLists.txt:14,50-62；cli/version_generated.h.in；modules/conformance/{echo,noop}/module.yaml:7；modules/conformance/echo/include/astrocs/echo/types.h:26；modules/conformance/noop/src/noop_module.c:35；DEPENDENCIES.md:8 | ASTROCS_DESIGN §12（Alpha 前程序与代码不包含任何版本信息）；ENGINEERING_SPEC §7 L110 | 命令：`grep -n "project(astrocs VERSION" CMakeLists.txt; grep -rn "0.11.0-alpha" modules/`；输出：`14:project(astrocs VERSION 0.11.0 …)`、module.yaml/types.h/kVersion 共 5 处 `0.11.0-alpha.2`；L61 configure_file 注入 + cli/commands.cpp 消费 → 版本编译进产品 | P1 | 版本纪律红线在代码/元数据/二进制三面被破；发布候选清单（§12）无法如实过 | 三条路线择一由负责人裁决：(a) 撤版本链至发布时再启用；(b) §12/§7 修订批准现态；(c) 版本只留根 VERSION 助记且不进构建注入 | 根 CMakeLists + cli/ + modules/conformance + DEPENDENCIES.md | `grep -c "VERSION 0.11.0" CMakeLists.txt` → 0（或负责人批准记录在案） | 与 GAP-017 同源（其仅登记根 VERSION 文件）；归属 GOV-001 |
| W2-ROOT-4 | build.sh:26-49（入口行 26） | ENGINEERING_SPEC §1 L10（唯一根 CMake + presets，不引入第二套构建入口）；ASTROCS_DESIGN §10.2；AGENTS §3 | 命令：`sed -n 26p build.sh`；输出：`if ! cmake -S "$REPO/lib/phase2" -B "$BUILD_DIR" \`——绕过根 CMakeLists 以 lib/phase2 为源根 configure；根 CMakeLists:710 又 add_subdirectory(lib/phase2)，双入口并存 | P1 | 根级官方构建脚本只覆盖 phase2 且构成第二构建入口；按 §3/§10.2 语义产出的安装树/产品清单无从建立 | build.sh 重写为 `cmake --preset linux-control && cmake --build --preset linux && ctest --preset linux`（§7 白名单要求保留此文件，故订正而非删除） | build.sh（仓库根） | `grep -n "cmake -S" build.sh | grep -v '\\-S .\$' | wc -l` 或 `cmake --list-presets` 双入口消失：`grep -c 'lib/phase2' build.sh` → 0 | 第一波未点名；归属 CI-001/INT-001 |
| W2-ROOT-5 | toolchain.ps1:11-13,49,51,62-72,79,105-201 | ENGINEERING_SPEC §1（唯一构建入口）§6 + ASTROCS_DESIGN §6.1/§6.2/§10.1（唯一可执行 ACSD Cli/acsd_cli；命令树 normalize/mosaic/export）；AGENTS §3（不得写死服务器绝对路径）§5 | 命令：`grep -n "msys64\|Users/fujia" toolchain.ps1; git ls-files lib/astro_image_io lib/orchestrator/cpp lib/healpix_db/healpix_drizzle`；输出：11-13 三处机器绝对路径；三路径 tracked 均 0 行（build 清单 10 项已死 3+）；:179-180 打包 README 模板仍宣称「唯一运行入口 orchestrator.exe <stage1.json>」（旧命令树 stage1） | P1 | 白名单强制保留的根工具链脚本整体基于已废架构与旧命令树，误导开工链；MinGW/make 第二构建入口与 §1 冲突 | 重写 toolchain.ps1 为 Windows 侧 preset 入口封装（去除机器路径、改 doctor/benchmark 语义），或负责人批准从 §7 白名单除名归档。注：DEPENDENCIES:86 已自我登记 known_limits（承认问题但未处置） | toolchain.ps1（仓库根） | `grep -cE "msys64|Users/fujia|orchestrator" toolchain.ps1` → 0 | 与 DEPENDENCIES.md known_limits 自述同源；第一波未点名；归属 GOV-001/ROOT-001 |
| W2-ROOT-6 | DEPENDENCIES.md:4；CMakePresets.json:4,12 | ENGINEERING_SPEC §8（无悬空引用）；ASTROCS_DESIGN §0（控制包文档不在权威链顶，不能自称「唯一编译依据」） | 命令：`git ls-files | grep -i TOOLCHAIN_LOCK`；输出：空（09_WINDOWS_TOOLCHAIN_LOCK.md 全仓 tracked 零命中），但被 DEPENDENCIES/CMakePresets/cmake/toolchain/verify_toolchain.py/packaging 3 处共 6 文引用，含「唯一编译依据」措辞 | P1 | 冻结工具链合同的依据文档已删而未订正：发布工具链的权威出处悬空，packaging 面多处注释失效 | 以 packaging/schemas/preset-contract.json 为唯一在仓事实源，6 处引用改述（删「控制包为唯一编译依据」句式） | DEPENDENCIES.md + CMakePresets.json + packaging/ + cmake/toolchain | `grep -rln 09_WINDOWS_TOOLCHAIN_LOCK --include='*.md' --include='*.json' . | wc -l` → 0 | 第一波未点名（属 ROOT-007 删包后 165+ 悬空引用族的根面样本）；归属 PKG-001/GOV-001 |
| W2-ROOT-7 | DEPENDENCIES.md:42,66,68 | ENGINEERING_SPEC §8（引用有效）；DESIGN §10.1 交付形态 | 命令：`ls -d third_party/nlohmann_json; git ls-files lib/astro_image_io | wc -l`；输出：No such file；0。实际：third_party/nlohmann/json.hpp（内核对账版本 3.12.0 与 :68 声明一致✓）；cfitsio 已迁 lib/infrastructure/aio/third_party/cfitsio | P2 | 依赖锁文档两处 vendored 路径指向已消失/改名位置，按文索骥找不到锁面对象 | 两处路径改指实际位置；与 packaging/dependency-lock.json 内字段一并核对（lock 文件本身在位✓） | DEPENDENCIES.md | `grep -cE "nlohmann_json|astro_image_io/third_party" DEPENDENCIES.md` → 0 | NP-D 对账线同源；归属 PKG-001 |
| W2-ROOT-8 | .gitignore:39-41 vs lib/healpix_db/archive/** + reports/archive/**（42 tracked 文件） | .gitignore 自述（GOV-002：归档文件须受 Git 跟踪）；ENGINEERING_SPEC §8（豁免显式登记一致性） | 命令：`git ls-files | git check-ignore --no-index --stdin -v`；输出：`.gitignore:39:archive/ → lib/healpix_db/archive/legacy/healpix_stack/**(39) 与 reports/archive/**(3) 命中` | P2 | FATDUCK 同型 tracked-vs-ignore 冲突面：豁免集只覆 docs/archive 与 engineering/control/archive，两个在追归档区被声明为应忽略——误 rm 后无法经 git 找回，策略与事实互相打脸 | 补 `!lib/healpix_db/archive/` `!reports/archive/` 豁免或将两目录迁至已豁免归档位（HEAD 树实测 42 文件） | .gitignore + lib/healpix_db/archive + reports/archive | `git ls-files | git check-ignore --no-index --stdin | wc -l` → 仅剩有意豁免行 | 与第一波 GOV-7 同型不同对象（不并条）；归属 ROOT-002 |
| W2-ROOT-9 | .gitignore:80-90,110,112-113,65-66 | ASTROCS_DESIGN §10.1/§12（旧产品形态与 Alpha 前版本产物禁止）；ENGINEERING_SPEC §8 | 命令：`grep -n "AstroCS-CLI-v1\|engineering_v1.2" .gitignore; ls -d dist engineering_v1.2`；输出：83-90 行 `!dist/AstroCS-CLI-v1/VERSION.txt` 等豁免组 + 65-66 engineering_v1.2 路径；两目录均 No such | P2 | 白名单化「旧产品名+VERSION.txt 入库」的 dist 形态与 `audit/*.rar`、`_new_pack_v*/`、`*_Context.zip` 等残留规则指向已删对象，暗示已废止的交付形态仍可复现 | 删除死规则组（dist/AstroCS-CLI-v1 全组、engineering_v1.2 两行、audit/*.rar、_new_pack_v*/），保留仍在用的运行产物兜底组 | .gitignore | `grep -cE "AstroCS-CLI-v1|engineering_v1.2|audit/\*.rar" .gitignore` → 0 | 与第一波 GOV-3（注册表指向不存在文件）同族；归属 ROOT-002 |
| W2-ROOT-10 | VISUAL_CHECK_README.md:8,17 | ENGINEERING_SPEC §8（命令可复现）；ASTROCS_DESIGN §1.3/§11.2（HiPS Browser 不进产品；验收以现态构建为准） | 命令：`ls -d launch; git ls-files launch`；输出：No such file / 空——唯一启动命令 `pwsh -File .\launch\start_browser.ps1` 指向不存在目录；:17 引用 run\phase2\v9\geometry_truth.hips 为 gitignore 临时产物 | P2 | 用户视觉验收指引整体不可执行（文档头注释称自动构建但脚本从未在仓）；第一波 GOV 线建议归档，本轮逐命令复核坐实其路径全死 | 删除或整体归档（归档时正文命令须先订正）；不新建根条目 | VISUAL_CHECK_README.md（仓库根） | `test -d launch; echo rc=$?`（若保留文档则其命令路径必须实存）| 与第一波 GOV.md 归档建议 + ci/root_manifest.json F2 登记（ROOT-004 处置）同源；归属 ROOT-001 |
| W2-ROOT-11 | scripts/package_audit.py:15-19 | ENGINEERING_SPEC §8（无悬空引用）；DESIGN §12（发布/审计包白名单） | 命令：`for p in docs/refactor evidence/refactor lib/io REVIEW.md; do git ls-files "$p" | head -1; done`；输出：4 项全空；另 :19 打包 VERSION（版本入包，§12 擦边） | P2 | 旧 REL-003 打包器以 4/20 死条目白名单运行且必然「静默少打包」；与 tools/pack_audit_package.py（DENY_PATHS 防线现行版，第一波 GOV-7/PKG 认可）双头并存 | 删除 scripts/package_audit.py + validate_audit.py 或在本文件头登记替代关系并入 MOD/ROOT 台账；二选一避免白名单漂移 | scripts/ | `git ls-files scripts/package_audit.py | wc -l` → 0 或 `grep -c "docs/refactor" scripts/package_audit.py` → 0 | 与第一波 PKG/GOV-7 的 pack 侧结论同源；归属 PKG-001 |
| W2-ROOT-12 | CMakePresets.json:4,14；对照 DEPENDENCIES.md:26-27 | ENGINEERING_SPEC §8（检查器覆盖、豁免显式）；DESIGN §10.2（双平台构建以 preset 为准） | 命令：`ls packaging/schemas/; grep -c win-msvc ci/checks.json; grep -rln cmake/toolchain/verify_toolchain ci/ .github/`；输出：仅 preset-contract.json（无 .schema.json 同名物）；checks.json 命中 0；无任何调用方——「drift FAIL fast」仅为脚本自述，机器门 TOOLCHAIN-VERIFY 实为另一套 ci/verify_toolchain.py（agent-host 域） | P1 | 正式 preset 名与合同 schema 指针双错（文件名悬空），且宣称的 preset 漂移门未接任何 CI/注册表：Windows 发布工具链冻结无机器守护 | contract_schema 改 `packaging/schemas/preset-contract.json`；把 cmake/toolchain/verify_toolchain.py 注册为 CHK-*（waivable:false，windows-main profile） | CMakePresets.json + ci/checks.json | `python3 cmake/toolchain/verify_toolchain.py; echo rc=$?` 且 `grep -c "cmake/toolchain/verify_toolchain" ci/checks.json` → ≥1 | 第一波未点名；归属 CI-001/PKG-001 |
| W2-ROOT-13 | ENGINEERING_SPEC.md:110 | ENGINEERING_SPEC §7 自身（条款指向对象须存在）+ §8 无悬空 | 命令：`ls CHANGELOG.md`；输出：No such file——条款承认「VERSION/CHANGELOG.md 仅作内部助记」但该文件已被 ROOT-007 删除且未订正文本（§7 白名单亦无此两项） | P2 | 权威规范承认一个不存在的根对象，误导白名单对账与 ROOT 门（HEAD 实测 allowed_files⊖§7=∅，唯此句失真） | 该句改为「内部助记文件（如存在）不进入程序与发布产物」或经负责人订正删除；与第一波 GOV-3 的注册表清理一并处置 | ENGINEERING_SPEC.md | `grep -n "CHANGELOG" ENGINEERING_SPEC.md | wc -l` → 0 或配套存在登记在册的 CHANGELOG.md | 与第一波 GOV-3、GAP-016/027 同源；归属 GOV-001 |
| W2-ROOT-14 | build.sh:2,16,23,53；CMakeLists.txt:12,49,101-114；CMakePresets.json:4,30,60；DEPENDENCIES.md:1,8,58,81；modules/conformance/** 注释；packaging/schemas/preset-contract.json:3 | ENGINEERING_SPEC §2 L19（禁止堆积历史版本号/任务编号/审计流水/代码复述） | 命令：`grep -n "BLD-001\|TST-001" build.sh | head -2`；输出：build.sh 4 处 BLD-001/TST-001；CMakePresets 含「V8-CI-010 F-R3-02b」事故流水与「configure fail in 0.4s」叙事；同类编号遍布本轮 6+ 文件头注释 | P2 | 审计流水号进入配置/脚本头注释，语义漂移后成第二悬空源（09_WINDOWS 即先例）；违反注释红线 | 各文件注释只留约束本身（禁什么/为什么），任务号迁往 工程控制/ 台账 | 根 build.sh/CMakeLists.txt/CMakePresets.json/DEPENDENCIES.md + modules/ | `timeout 30 grep -rnoE "(BLD|TST|V8-CI|VER|REL|QA)-[0-9]+" build.sh CMakePresets.json | wc -l` → 0 | 第一波未点名；归属 DOC-001 |
| W2-ROOT-15 | .gitattributes:1,13（对照 .gitignore:57）；.editorconfig:1；.clang-format:1 | ENGINEERING_SPEC §2（行尾治理以 .gitattributes 为文；编码 UTF-8 一致） | 命令：`grep -n "xisx\|xisf" .gitattributes .gitignore`；输出：.gitattributes 有 `*.xisx binary`（全仓唯一出现、无此扩展）而缺 `*.xisf`（.gitignore:57 实存该扩展意识）；头注释自称「强制使用 LF」而规则为 `text=auto eol=lf`（auto 判定，非强制面）；.editorconfig/.clang-format 留「V14 G5」历史流水号 | P2 | 行尾/二进制治理对真实扩展（xisf 工作文件）不设防，未来 .xisf 可能以文本形态产生假 diff；注释与行为漂移 | `*.xisx`→`*.xisf`，补 `.gitignore` 对齐；头注释改准确描述；删 V14 G5 号 | .gitattributes + .editorconfig + .clang-format | `grep -c xisx .gitattributes` → 0 | 第一波未点名；归属 DOC-001 |
| W2-ROOT-17 | README.md:6,12,15,107,112；memory.md:10-13,43,111；HANDOVER.md:7-9,22 | ASTROCS_DESIGN §0（权威链唯一 DESIGN＞AGENTS＞ENG_SPEC＞…，宪章不在链上且已删）；ENGINEERING_SPEC §8（删除/重命名无悬空引用） | 命令：`grep -n "ASTROCS_PROJECT_CONSTITUTION" README.md memory.md HANDOVER.md; test -e ASTROCS_PROJECT_CONSTITUTION.md; git ls-files 工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915`；输出：README:12「FROZEN 唯一最高约束」/memory:11/HANDOVER:7 三处指宪章（absent）；V6/V1 控制包路径与 REVIEW.md、CHANGELOG.md、ACTIVITY_STATE.md 均 absent/not-tracked | P1 | 三个第一入口文档仍以已删旧宪章链自称权威（「冻结宪章之下」「REVIEW.md+docs/owner 为准」），并指向已删控制包/已删 L0——Agent 开工链首跳即悬空，b3167608 实测仍 OPEN（≠仅第一波自述） | 抬头块统一改 DESIGN §0 链；宪章/CONSTRAINTS/REVIEW/CHANGELOG/旧控制包引用删除或降级为历史注记，与第一波 GOV-1 整改合并 | README.md, memory.md, HANDOVER.md | `timeout 30 grep -lE "ASTROCS_PROJECT_CONSTITUTION|AstroCS_ENGINEERING_CONSTRAINTS|工程控制/AstroCS_" README.md memory.md HANDOVER.md | wc -l` → 0 | 与第一波 GOV-1（P1）/GOV-2、GAP-001/002/019、父点名 DOC3-2 同源不删条；归属 GOV-001/DOC-001 |
| W2-ROOT-16 | modules/conformance/{echo,noop}/module.yaml:3；echo/README.md:62；{echo,noop}/CMakeLists.txt 注释「11 §x/12 §x」 | ENGINEERING_SPEC §8（引用有效）+ §4（模块必备件引用现行规范） | 命令：`git ls-files | grep -E "MODULE_SOURCE\|_STANDARD\\.md" | head`；输出：无 `11_MODULE_SOURCE_TEST_STANDARD.md`（现行系 docs/standards/{TEST,C_ABI,…}_STANDARD.md，编号体系 11/12 无登记映射） | P2 | conformance 模块元数据自称遵循不存在的标准文件号，字段规范出处悬空（现行对应未留迁移注记） | 注释与 README 改指现行 docs/standards/ 文件名（附映射一句），或负责人确认编号系保留 | modules/conformance/** | `grep -rc "11_MODULE_SOURCE_TEST_STANDARD" modules/ | grep -v :0 | wc -l` → 0 | 与 docs/standards 换版族同源；归属 MOD-001/DOC-001 |

（config/ 未入库与 DESIGN §3.3 的冲突由 CFG-001 线立条：本轮实测 `?? config/` 且 `git ls-files config` 为空、ci/root_manifest.json allowed_dirs 无 config——不重复立条，仅在此登记事实。）

## ② 覆盖清单

```tsv
AGENTS.md	FINDING:W2-ROOT-1
ASTROCS_DESIGN.md	FINDING:W2-ROOT-2
build.sh	FINDING:W2-ROOT-4
.clang-format	FINDING:W2-ROOT-15
CMakeLists.txt	FINDING:W2-ROOT-3
CMakePresets.json	FINDING:W2-ROOT-12
CONTROL_PACK_SPEC.md	OK(已读无发现)
DEPENDENCIES.md	FINDING:W2-ROOT-6
.editorconfig	FINDING:W2-ROOT-15
ENGINEERING_SPEC.md	FINDING:W2-ROOT-13
FATDUCK_ACCESS.md	NA:credentials
.gitattributes	FINDING:W2-ROOT-15
.gitignore	FINDING:W2-ROOT-8
toolchain.ps1	FINDING:W2-ROOT-5
VISUAL_CHECK_README.md	FINDING:W2-ROOT-10
HANDOVER.md	FINDING:W2-ROOT-17
memory.md	FINDING:W2-ROOT-17
README.md	FINDING:W2-ROOT-17
VERSION	FINDING:W2-ROOT-3
scripts/package_audit.py	FINDING:W2-ROOT-11
scripts/validate_audit.py	OK(已读无发现)
run/.gitkeep	OK(已读无发现)
third_party/nlohmann/json.hpp	NA:generated(vendored单头,3.12.0与DEPENDENCIES:68版本对账一致)
modules/conformance/echo/CMakeLists.txt	FINDING:W2-ROOT-16
modules/conformance/echo/README.md	FINDING:W2-ROOT-16
modules/conformance/echo/include/astrocs/echo/types.h	FINDING:W2-ROOT-3
modules/conformance/echo/module.yaml	FINDING:W2-ROOT-3
modules/conformance/echo/src/echo_module.c	OK(已读无发现)
modules/conformance/echo/tests/unit/echo_host_callback_test.c	OK(已读无发现)
modules/conformance/noop/CMakeLists.txt	FINDING:W2-ROOT-16
modules/conformance/noop/README.md	FINDING:W2-ROOT-16
modules/conformance/noop/include/astrocs/noop/types.h	FINDING:W2-ROOT-3
modules/conformance/noop/module.yaml	FINDING:W2-ROOT-3
modules/conformance/noop/src/noop_module.c	FINDING:W2-ROOT-3
modules/conformance/noop/tests/unit/noop_handshake_test.c	OK(已读无发现)
modules/services/io/include/astrocs/io/fits_stream_v1.h	OK(已读无发现)
modules/services/io/include/astrocs/io/hips_input_v1.h	OK(已读无发现)
modules/services/io/tests/fits_core_selftest.c	OK(已读无发现)
modules/services/io/tests/hips_core_selftest.c	OK(已读无发现)
```

- files_total: 39（根 19 全量：首轮 15 + coverage_matrix 机检补交 README.md/memory.md/HANDOVER.md/VERSION 4 行；scripts 2；run 1；third_party 1；modules 16；父指令追加的 8 个根文件全部在根 19 之内）
- verdict_counts: OK=10 / FINDING=27（分布于 17 条发现）/ NA=2 —— 合计 39，每文件恰出现一次；本域（根/scripts/run/third_party/modules）下 `git status --porcelain` 无 UNTRACKED 新文件
- 枚举命令逐字：`git ls-files | awk -F/ 'NF==1' | sort`；`git ls-files scripts | sort`；`git ls-files run | sort`；`git ls-files third_party | sort`；`git ls-files modules`；`git status --porcelain | grep '^??'`

## 附：核查过但未立条（防遗漏声明）
- AGENTS.md 其余反引号路径锚（docs/ci/CI_SPEC.md、docs/plugins/**、lib/infrastructure、contracts/ 等）全部实存 → 其唯一问题即 run_checks 入口（W2-ROOT-1）。
- docs/plugins 计数：24 tracked =「23 篇」+ 00_INDEX.md，口径相符。
- CONTROL_PACK_SPEC.md：GAP_AUDIT.md/SUMMARY.md 为模板产物名非路径锚，工程控制/PROJECT-GOVERNANCE-01/ 实存 → 无发现。
- 根 CMakeLists.txt add_subdirectory 死目录 = 0；modules/conformance/echo 未接根构建系 echo/README §8 自declare「属前台集成待办」且 modules 目录已登记 F2(MOD-001) → 不重复立条。
- ci/checks.json TOOLCHAIN-VERIFY 引 ci/verify_toolchain.py（tracked 实存）→ 非悬空；问题只在 preset 合同门未接（W2-ROOT-12）。
- README.md/memory.md/HANDOVER.md/VERSION：补交轮已立条——前三者→W2-ROOT-17（旧宪章链悬空，GOV-1 同源）；VERSION=`0.11.0-alpha.2` 为 W2-ROOT-3 版本链之源（root_manifest F2/GAP-017 已登记，归属 GOV-001 裁决）。
