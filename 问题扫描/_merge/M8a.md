# 合并档案 M8a — 第二层合并验证（**实际范围 = L25 + L27**）

- 代理码: M8a（第二层合并验证代理）
- **范围更正记录**: 原派发范围 L21+L22 → 前台两次更正后锁定 **M8a = L25 + L27**（令文：「【范围更正】你改做 M8a = L25 + L27，不再做 L21/L22」）。象限锁：M7=L19+L20+L21、M8=L22+L23、M8a=L25+L27、M9=L24+L26。
- 输入: `_cache/L25.md`（12 条，P0:2 / P1:5 / P2:5）、`_cache/L27.md`（17 条，P0:0 / P1:10 / P2:7）、`_merge/00_COORDINATION.md`、`_cache/F00_FRONT_SPOTCHECKS.md`、宪章 §1.1/§8.4/§12.2/§12.3/§13.1/§14.2/§16.2/§16.3/§17.10/§17.11/§19。
- 模式: **纯只读**（read / grep / glob；web 仅用于核对上游许可事实，附 URL）。未跑 shell/构建/测试/脚本/git。
- 输出: `findings/<类别>/p<N>/M8a_L25_L27.md`（每「类别×优先级」一份）+ 本档案。
- **计数与比对纪律（本档案全部数字自证）**: 凡"两份结构化文件是否同构/等值"的判断一律不用 read 的行文本作判据（read 对超长单行截断），改用 ①元素计数 ②键集合 ③逐列 grep 三件套，并在每条定稿正文写明所用口径；凡前台/他人转述的数字一律作**待复核线索**，定稿自行重算。

---

## 0. 前台指令执行确认（逐条落点）

| 令 | 内容 | M8a 执行落点 |
|---|---|---|
| 范围更正 | 改做 L25+L27，不再做 L21/L22；旧证据不得丢弃 | 本档案 §7「已作废范围」整节保留 L21/L22 已取证内容并交回 M7/M8 |
| 必核① | GSL + Siril 派生双缺口复验（是否进交付、有无许可登记、著作权声明是否覆盖派生），基线 §16.2，发布阻断级自定 P0 | **M8a-G-001**（一条三面：许可传染 / 清单零登记 / 派生署名，定 **P0**；并写明判定边界：不判法律衍生、不断言必为静态并入） |
| 必核② | 豁免表按字面目录名切分 = L25-005 根因，写成机器门建议（按 LICENSE 文件存在性判定） | **M8a-G-003**（含 4 项机器门建议：按出处事实判定的 `check_third_party_registry.py`、豁免集改由登记面派生、反向存在性门、审核包同源收录） |
| 必核③ | L27 三条最重要复验：orchestrator README「只有一条命令」+ 第二套退出码；五源三态滞后簇；MODULE-READMES 门实覆 5/43、8 要素核验 0、放行错锚、gen_module_readmes 不在 checks.json | **M8a-C-001**、**M8a-C-002**、**M8a-G-006**、**M8a-G-007**（全部按当前树重读命中，见 §2 复算台账 R3/R4/R5/R6） |
| 必核④ | L27-002/003 是否升 P0 由本代理裁决；若维持 P1 须写出与三家 P0 的实质差别 | 裁决**维持 P1**，实质差别三条已写入 M8a-G-006「问题说明」末段（判据可否为假 / 条文是否存在 / 后果面是否阻断），另附**条件升级条款**（比照 R-4「接线即升级」） |
| 必核⑤ | 「entrypoint」两义：先找权威定义，找不到登记为术语缺口交负责人裁决，不按猜测定档 | 全量检索后确认**仓内无权威定义**（GLOSSARY 0 命中、14 份标准 0 定义、schema 只有另一键名 `entrypoint_abi`、字段规范 11 号标准不在仓内）→ 立 **M8a-I-006** 术语缺口条，含二选一裁决建议，**不定档谁对谁错** |
| 必核⑥ | L27 覆盖面分母原样收录 | 本档案 §5（92 目录单元 / 43 README = 46.7%、43 份逐要素打分、21 module.yaml、86 SCI/ALG 中 2 未注册、93 DISP 中 1 未登记、10 悬空路径、§2 四源矩阵 23 模块），并附本代理独立复算口径 |
| 必核⑦ | 保持「有要求无门 = G_GOV_GATE」vs「标准缺要求 = I_DOC_HYGIENE」二分；owner 的「每子库 README」无条文支撑 → 入 SUMMARY「建议新增规范条款」；同法处理 L27-011，不与 L14-002 重复计数 | **M8a-I-001**（含建议条款 **R-01**）+ §6；L27-011 → **M8a-I-005**，只补 L2 层实例，related 指回 **L14-002（主条）**，不重复计 |
| 必核⑧ | 边界：L25 对 M5b 版本簇（G-04）只作 related；L27 与 M6b 分工——README/manifest 一致性归本代理，索引/追溯断链归 M6b | 全部 related 化：M5b G-01/G-02/G-03（fail-open 三家）、M5b 版本簇 G-04、M6b-E-001/-E-002/-G-001、M2b-G-04、L18-001、L12-016、L13-013、L03/L04、F00-01/-03；本档案不写「系统性」级判词 |
| 方法论令 | 禁用 read 行文本作等值判据；转述数字一律重算；SUMMARY 总述条目归属既定，本域条目 related 指回 | 本档案 §2 台账逐条标口径；§8「数字口径替换表」列出所有被替换的转述数字 |
| 补令（M2b 许可证面） | 把「DISP-HIPS 系文档写 Healpix_3.83 GPL-2+ 而交付清单/LICENS面另有说法」并入同族，并成一条「依赖与许可登记面三处失真」并引用 C-07 条款 | **已并为一条**（M8a-G-001 三面结构 + 引用 `问题扫描/40_OWNER_DECISIONS.md:59` 的 **C-07 依赖与许可登记门**）；HEALPix 面经查**主落已是 M2b-G-04**（源自 L03-007，非 L15-002），本条只作同族引用不重复登记，详见 §4 转述复核表 |
| 通则确认 | 「问题扫描/** 自写文件不计入真源统计」——43/92 分母须确认不含我方档案 | **已确认剔除**：本代理口径显式排除 `问题扫描/**` 下 **37** 份 README（全仓 glob 共 951 份，其中 `run/**` 847 份影子树亦剔除）；在范围标准命名 README = 42 + 1 份异形 = **43**，详见 §5.2 |

---

## 1. 逐条处置表（L25 12 条 + L27 17 条）

### 1.1 L25（依赖 / 许可 / vendored / 派生 / 豁免表）

| 源条目 | 一句话 | 四态判定 | 定稿编号 | 类别 | 级 | 处置理由（当前树复验） |
|---|---|---|---|---|---|---|
| L25-001 | Siril 派生 ipv 挂整模块 MIT、逐文件零署名 | 仍成立 | **M8a-G-001**（面①③） | G_GOV_GATE | **P0** | 与 L25-002 并成一条三面定稿（前台指令）；ipv 13 TU 在 STATIC 闭包、AT_MATCH_ 50 命中、SPDX/Copyright 0 命中均复算命中 |
| L25-002 | GSL 经 PUBLIC 链入产品 exe，锁与文档零登记 | 仍成立 | **M8a-G-001**（面①②） | G_GOV_GATE | **P0** | 链接闭包三段 PUBLIC 复核命中；登记面 4 处 0 命中；CI 面反证（知道 libgsl-dev）已写入 |
| L25-003 | nanoflann / healpix_core 编入交付但许可零采集 | 仍成立 | M8a-G-005 | G_GOV_GATE | P1 | `third_party/` 实树仅 nlohmann；`lib/drizzle/CMakeLists.txt:95-98` 的 EXISTS 守卫恒假（幻影 vendored 路径） |
| L25-004 | vendored json-schema-validator 无 LICENSE 全文 | 仍成立（**降级**） | M8a-G-009 | G_GOV_GATE | P1→**P2** | 该目录不在根交付图（add_subdirectory 全集无 `lib/orchestrator`），义务面止于源码分发；附"接线即回升"条款 |
| L25-005 | 豁免/登记按字面目录名切分（根因） | 仍成立 | M8a-G-003 | G_GOV_GATE | P1 | 12 个工具的字面判据逐条复核 + CSV 键集合复算（license_hint 值域只有 cfitsio） |
| L25-006 | 三项"机器校验入口"无接线 + lock↔文档单向 | 仍成立 | M8a-G-004 | G_GOV_GATE | P1 | `ci/checks.json` grep `gen_sbom|BLD-004|dependency-lock|verify_actions_lock` = 0 命中；`cmake/toolchain/verify_toolchain.py` 存在但 0 引用 |
| L25-007 | RCR 版本口径互斥 + oracle 冒名 | **部分修复**（两断言不成立） | M8a-B-001 | B_STD_MISMATCH | P1→**P2** | 全仓 13 处一致写 2.4.7（grep 复核）；`rcr_oracle_compare.py:14` 真 import rcr → "astropy 冒名"不成立；残留 = SCI 以包版本为语义权威且无推导位 |
| L25-008 | healpix 出处 URL 损坏 + NOTICE 无上游原文 | 仍成立 | M8a-I-004 | I_DOC_HYGIENE | P2 | `healpix_core.cpp:6` 的 https:// 后含空格；31 行 NOTICE 为转述未贴原文 |
| L25-009 | 锁与报告两把尺子互斥 | **前提翻转**（修复未回写） | M8a-I-007 | I_DOC_HYGIENE | P2 | 锁已刷新（`missing_tools` 现仅 ccache），失真侧变为 `ci/INVENTORY_REPORT.md:15`；原"多工具缺失/相差一天"数字不继承 |
| L25-010 | zlib 降级叙述与根图实路相反 | 仍成立 | M8a-C-007 | C_DOC_CODE_GAP | P2 | `zcompress.c:6` 无条件 include；根图无 else 剔除；降级实现只在 `cli/CMakeLists.txt:204-215` |
| L25-011 | cfitsio 哈希覆盖面 + 上游 4.7.0 | 仍成立（部分**无法判定**） | M8a-G-010 | G_GOV_GATE | P2 | 60 源 + 1 头的覆盖面成立；"168 追踪文件/树未漂移"依赖锁自述，规程禁 git → 标无法判定；上游 4.7.0 本代理独立抓取核实 |
| L25-012 | test-only oracle 依赖群未入锁 | 仍成立 | M8a-C-009 | C_DOC_CODE_GAP | P2 | `test_only_oracles` 实有 4 条，无 astropy-healpix/scipy/rcr；policy 装 python3-scipy 而 `tests/` 0 引用 |

### 1.2 L27（子库 README 完备性与内容正确性）

| 源条目 | 一句话 | 四态判定 | 定稿编号 | 类别 | 级 | 处置理由 |
|---|---|---|---|---|---|---|
| L27-001 | README-DOCS 改写 §12.2 分层 + Wiki 置于权威链首 | 仍成立 | M8a-G-008 | G_GOV_GATE | P1 | §12.2:425-427 vs README-DOCS:8/:11/:16-20 字面冲突；DOCUMENTATION_STANDARD:3 把权威外链 |
| L27-002 | MODULE-READMES 门实覆 5/43、8 要素 0 核验、放行错锚 | 仍成立（**不升 P0**） | M8a-G-006 | G_GOV_GATE | P1 | 覆盖面/判据/错锚三重确数；与三家 P0 的实质差别 + 条件升级条款见该条 |
| L27-003 | gen_module_readmes 自称 checker 源但不在 checks.json | 仍成立（**不升 P0**） | M8a-G-007 | G_GOV_GATE | P1 | 注册表 0 命中；26 registry 页 vs 22 descriptor 集合差 4；`DOCUMENT_INDEX.yaml` 19 条"手写/须保留"自然语言叮嘱即无门证据 |
| L27-004 | orchestrator README 称"正式科学运行只有一条命令" + 第二套退出码 | 仍成立 | M8a-C-001 | C_DOC_CODE_GAP | P1 | 不在根图/安装清单/产品单元；`ASTROCS_MODULE_MISSING` 在 `cli/` 0 命中；ARCH-001:88 反向自证 |
| L27-005 | astro_image_io README 四重脱钩 | 仍成立 | M8a-C-003 | C_DOC_CODE_GAP | P1 | "零外部依赖(不依赖 cfitsio)" vs 同目录 vendored cfitsio 60 源入图；外部 GitHub commit 当"当前版本" |
| L27-006 | healpix_db README 称"独立仓库本地副本，.gitignore 忽略" | 仍成立 | M8a-C-004 | C_DOC_CODE_GAP | P1 | `.gitignore` grep healpix = 0 命中；根图直接编译并安装该目录 |
| L27-007 | 五源三态滞后簇（hips/cosmetic/calibration/drizzle） | 仍成立 | M8a-C-002 | C_DOC_CODE_GAP | P1 | 新证：cosmetic/hips 两模块 README"无源码/无该 target"对 `add_library(... SHARED` + install + product.json IMPLEMENTED；calibration README 抄反自家 yaml |
| L27-008 | module_id 三套词汇（README/yaml/descriptor） | 仍成立（**本代理加重**） | M8a-C-005 | C_DOC_CODE_GAP | P1 | 值集合精确比对：21 份 manifest 的 module_id 与 22 个 descriptor 的 module_id **精确交集 = 0**（强于原报"三向 1 + 双轨 19/21"） |
| L27-009 | browser_qt 设计文档三份全悬空 | 仍成立 | M8a-E-002 | E_TRACE_BREAK | P1 | glob `docs/superpowers/**` = **0 文件** |
| L27-010 | 未注册 SCI/ALG ID 被 L2 引用 | 仍成立 | M8a-E-001 | E_TRACE_BREAK | P1 | `ALG-P2-COV-001`/`SCI-F3-001` 在 `docs/` = 0 命中；L1 登记的是 `ALG-COV-001`（INDEX.yaml:364） |
| L27-011 | V7 编号标准当现行权威 | 仍成立（**只补 L2 实例**） | M8a-I-005 | I_DOC_HYGIENE | P2 | 21/21 lib manifest + 2 份 conformance + 多份 README 引用仓内不存在的编号制标准；主条 L14-002 不重复计 |
| L27-012 | 「每子库 README」要求无条文 + 35/92 缺失 | 仍成立（标准缺口） | M8a-I-001 | I_DOC_HYGIENE | P2 | 加新证：§8.4 义务范围目录 `lib/algorithms/` **仓内不存在**（glob 0 文件、CMake/docs 0 命中）→ 该硬义务字面不成立 |
| L27-013 | 5 份薄 README 8 要素缺 4 | 仍成立（**锚剔除**） | M8a-I-002 | I_DOC_HYGIENE | P2 | 剔除原报的 `DOCUMENTATION_STANDARD.md:31` / `README-DOCS.md:24` "MODULE_README_TEMPLATE.md" 锚：该文件仅 14 行、`docs/` grep MODULE_README = 0 命中 |
| L27-014 | tools/README 覆盖面与名实 | 部分成立（**数字全部替换**） | M8a-I-003 | I_DOC_HYGIENE | P2 | 原报「README:95 称 quality/ 34 个检查器」不成立（文件 83 行、无 34/检查器字样）→ 改立为"无索引 + 无边界声明 + 与工具链政策相悖" |
| L27-015 | DISP-PSF-007 未登记 | 仍成立 | M8a-E-003 | E_TRACE_BREAK | P2 | 四处使用（README/yaml/测试锚/`HANDOVER.md:38`），登记表 §11.3 只到 006；`docs/` grep 0 命中 |
| L27-016 | providers README 符号表缺 `acs_cap_classify_v1` | 仍成立 | M8a-C-008 | C_DOC_CODE_GAP | P2 | 头 :127 有该导出、测试消费；`FROZEN` 不在 §0 词表；同文件其余为**全仓最佳样本**（正面事实已写入） |
| L27-017 | README 路径/行锚小簇 4 处 | 仍成立（缩窄） | M8a-E-004 | E_TRACE_BREAK | P2 | cosmetic 签名头指不存在的本目录路径、hips 测试引用需目录相对解析、calibration 行锚落在邻近 target、两页把**不存在的** ARCH_CONTRACTS 标 VERIFIED |
| —（M8a 新立） | 发布 NOTICE/SBOM 由脚本内硬编码字面量生成 | 新证 | **M8a-C-006** | C_DOC_CODE_GAP | P1 | `tools/make_windows_release.py:101-108` + `make_linux_release.py:87-93`；cfitsio 标 BSD 而仓内文本为 NASA/USG；SBOM 单一容器 `NOASSERTION` |
| —（M8a 新立） | entrypoint 两义无仓内权威 → 术语缺口 | 新证 | **M8a-I-006** | I_DOC_HYGIENE | P2 | 见必核⑤；作为 C-002/G-006 建议门的前置条件 |
| —（M8a 新立） | 两处并发修复无回归断言 | 新证 | **M8a-F-001** | F_TEST_GAP | P2 | 13 处版本串 / 8 文件与"引用锁的文档"两面均 0 断言（`ci/checks.json` grep rcr = 0） |

**定稿统计**: 29 源条目 → **29 条定稿**（1:1 承接 26 条 + 合并 2 条为 1 条 + 新立 3 条中的… 见下行精确数）
- 精确数：**P0 = 1**（M8a-G-001，一条三面，吸收 L25-001 + L25-002 两个源 P0）；**P1 = 14**；**P2 = 16**；合计 **31 条**。
- 覆盖类别：G_GOV_GATE(9) / C_DOC_CODE_GAP(9) / I_DOC_HYGIENE(7) / E_TRACE_BREAK(4) / B_STD_MISMATCH(1) / F_TEST_GAP(1)；**A_SCI_DEF / D_COMMENT / H_NUMERIC 本域 0 条**（L25/L27 两轴无该型事实，未强行凑条）。
- 新立 3 条：M8a-C-006、M8a-I-006、M8a-F-001（均为合并层复验时自行发现，非源条目）。

---

## 2. 复算台账（本代理亲自复算，逐条标明口径；不接受任何转述数字）

| # | 复算对象 | 口径（三件套中的哪一种） | 结果 | 用在哪条定稿 |
|---|---|---|---|---|
| R1 | GSL 是否进入交付可执行闭包 | 逐列 grep 链接关键字 + 符号锚：`CMakeLists.txt::astrocs_p1_sdet`(546)→`::astrocs_module_adapters`(646-649)→`::astrocs_cli_runtime`(665-666)→`::add_executable(astrocs)`(588-609)；三段均 PUBLIC | 闭包成立（传染结构事实；未判定 .so/.a 解析方式） | M8a-G-001 面① |
| R2 | 派生代码与登记面是否相交 | 元素计数 + 键集合：`lib/plate_solve/cpp/ipv/src/*.cpp` = **13**；`**/*.h` = 15；grep `SPDX|Copyright|GPL|MIT` 于 `lib/plate_solve` = **1 命中（即整模块 MIT）**、子树 = **0**；grep `AT_MATCH_` = **50 命中 / 4 生产 .cpp**；`third_party/**` 实树 = **仅 nlohmann/json.hpp** | 派生 13 TU 零逐文件署名；`third_party/` 并非真实 vendored 落点 | M8a-G-001 面③、M8a-G-005 |
| R3 | MODULE-READMES 门实覆与放行正确性 | 元素计数（glob `**/README.md` = 951，剔除清单见 §5.2 → 在范围 42+1）+ 判据逐条读 `tools/check_module_readmes.py`（断言集 = 文件存在 + ID 字样正则 + 3 个文件名出现且存在 + `"L2" in t`）+ 对 `docs/contracts/INDEX.yaml` 的 `- id:` 键集合 grep | 门覆 **5** 份；8 要素核验 **0** 项；`P1-003/004/005` 在 INDEX **0 命中**而门仍 PASS（放行错锚实证） | M8a-G-006、M8a-I-002 |
| R4 | registry 页与生产 descriptor 是否同构 | 键集合比对（glob `docs/modules/registry/*.md` = **26** vs grep `d.module_id = "` 于 `lib/core/src/module_adapters.cpp` = **22**）；差集两侧都算 | 页面多 **4**（`phase2.session`/`phase1.session`/`phase1.star-detection`/`phase1.hips-writer`，全为手写页）；descriptor 无缺页 | M8a-G-007 |
| R5 | manifest 与 descriptor 的 module_id 是否可比 | 值集合精确字符串比对（grep `^module_id:` 于 `lib` = **21** vs R4 的 **22**） | **精确交集 = 0**（manifest 用 `astrocs.p1./p2./p3./catalog.`，descriptor 用 `astrocs.phaseN.*`；唯一 `astrocs.phase1.session` 落在 manifest 侧而 registry 无该 descriptor）→ 强于 L27-008 原报 | M8a-C-005 |
| R6 | 五源状态滞后是否仍在当前树 | 逐 target grep：`lib/hips/CMakeLists.txt:43 add_library(astrocs_p1_hips_writer SHARED` + install_layout `:104-105` + product.json `:16 IMPLEMENTED` vs `lib/hips/README.md:24/:27/:28`；`lib/cosmetic/src/module_entry.cpp` 实存 + `lib/cosmetic/CMakeLists.txt:1` vs `lib/cosmetic/README.md:9-10`；`lib/calibration/module.yaml:21` vs `lib/calibration/README.md:163` | 四例全部仍成立（其中 cosmetic/hips 是本轮新增硬证据） | M8a-C-002 |
| R7 | 门注册表覆盖哪些依赖/许可/manifest 检查 | 单文件多模式 grep `ci/checks.json`：`gen_sbom|SBOM|dependency-lock|DEPENDENCIES|BLD-004|verify_actions_lock|check_release_layout|make_windows_release|make_linux_release|module.yaml|module_status|gen_module_readmes` 全 = **0 命中**；对照命中项 `MODULE-READMES:415-437`、`check_thread_budget:564`、`check_comments:782`、`verify_toolchain:1209`；`.github/` 同模式亦 0 | 依赖/许可/manifest 三面**无门** | M8a-G-004、M8a-G-006/-007 |
| R8 | RCR 版本串散写点与断言 | 分域 grep `2\.4\.7`：docs **5** / lib **7** / tools **1** = **13 命中 / 11 文件**；`ci/checks.json` grep `rcr|REJECTION` = **0**；`tests/` grep `2\.4\.7` = **0** | 手工同步、零断言 | M8a-B-001、M8a-F-001 |
| R9 | 工具链锁与报告谁失真 | 逐键 grep `"present"` 于 `ci/toolchain.lock.json`（10 项 true、仅 `ccache:83 false`）+ `missing_tools` 数组 | 锁已刷新（仅 ccache 缺），`ci/INVENTORY_REPORT.md:15` 的"九类全缺（见 lock missing_tools）"归因反转为失真 | M8a-I-007、M8a-F-001 |
| R10 | 「每子库 README」条文与义务范围 | 条文穷尽检索：`docs/standards/` = **14 份命名制标准全列**、无"每子库 README"要求；宪章 §8.4 义务范围目录 glob `lib/algorithms/**` = **0 文件**、grep 于 `CMakeLists.txt`/`docs/` = **0 命中** | 要求无条文 + §8.4 义务范围目录不存在（宪章↔树不一致，须 §1.2 流程） | M8a-I-001 |
| R11 | entrypoint 是否有权威定义 | 检索面：`docs/GLOSSARY.md`（0）、`docs/standards/`（仅 STANDARDS_REGISTRY 的描述性命中）、宪章（3 处用词无字段定义）、schema（只有另一键名 `entrypoint_abi`） | 无仓内权威定义 → 登记术语缺口，不定档 | M8a-I-006 |
| R12 | tools/ 规模与"检查器"叙述 | 元素计数：glob `tools/**` = **218**，剔 `__pycache__` 后 **160**；`tools/quality/` = **97**；文件名匹配 `(check|verify|audit)_*` = **63**（quality 内 24）；`tools/README.md` = **83 行**，grep `34|检查器` = **0 命中** | L27 的「README:95 称 34 个检查器」不成立；改立为无索引 + 无边界声明 | M8a-I-003 |
| R13 | 悬空引用与未注册 ID | 单/多文件 grep 命中数：`docs/superpowers/**` glob = **0 文件**；`ALG-P2-COV-001`、`SCI-F3-001`、`DISP-PSF-007` 在 `docs/` = **0 命中** | 三组悬空/未注册成立 | M8a-E-002、E-001、E-003 |
| R14 | 上游许可事实（外部核实，附 URL） | web 抓取 | Siril = **GPL-3.0-or-later**（<https://gitlab.com/free-astro/siril/-/raw/master/README.md> 「## License」）；GSL = **GNU GPL**（<https://www.gnu.org/software/gsl/doc/html/gpl.html>）；CFITSIO 上游现势 **4.7.0**（<https://heasarc.gsfc.nasa.gov/fitsio/> 「Download the latest 4.7.0 version of CFITSIO」）；**注意** L25 原引的 `…/docs/software/fitsio/fitsio.html` 本代理抓取返回 **404**，事实由现势 URL 复核成立 | M8a-G-001 面①、M8a-G-010 |
| R15 | README 分母是否含我方档案（前台通则） | 同 §5.2 剔除清单逐条计数 | **`问题扫描/**` 的 37 份 README 全部剔除**；`run/**` 847 份影子树亦剔除 | §5.2 |

---

## 3. 已修复表（不进 findings/，仅作修复未固化判定）

| 源条目 | 原结论 | 当前树复验 | 修复证据（path::符号） | 回归保护 | 是否另立条目 |
|---|---|---|---|---|---|
| L25-007 前半 | `rejection.h:27` 写 RCR **2.4.1**，与 SCI/README 的 2.4.7 互斥 | **已修复** | 现 `rejection.h::注释 Oracle 段`(26-27) 与 `rejection.cpp:100` 均 2.4.7；全仓 grep "rcr 2.4.1" = 0 命中 | **无**（13 处散写零断言） | 是 → **M8a-F-001**；残留语义权威面 → **M8a-B-001** |
| L25-007 后半 | RCR oracle 实为 `astropy.sigma_clipped_stats`，"官方 RCR"名不副实 | **不成立（本代理否证）** | `lib/phase2/tools/rcr_oracle_compare.py:14` 直接 `from rcr import RCR, SS_MEDIAN_DL`；`sigma_clipped_stats` 只在 `lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/` 两脚本（另一方法学） | 不适用（原断言有误） | 该断言在 M8a-B-001 四态判定段显式剔除，不继承 |
| L25-009 | 锁与报告互斥、锁列 13 工具缺失且与实况不符 | **锁侧已被并发刷新**，失真侧翻转到报告 | `ci/toolchain.lock.json:4` `generated_utc 2026-09-06T10:14:47Z`、`:133-135` `missing_tools: ["ccache"]`（原多工具清单已不在锁中） | **无**（引用锁的文档无刷新门） | 是 → **M8a-I-007**（事实面）+ **M8a-F-001**（断言面） |
| L27-002 相关面（推测项） | 原疑 `check_module_readmes.py` 可能"生成器覆盖其余模块" | **推测排除** | `tools/quality/gen_module_readmes.py::OUT_DIR` = `docs/modules/registry`（生成的是 registry 页，不是子库 README） | 不适用 | 否（结论并入 M8a-G-006/-007 正文） |

---

## 4. 剔除 / 降级 / 转述复核表

### 4.1 剔除或缩窄的断言

| 项 | 原锚/原数 | 复验结论 | 处置 |
|---|---|---|---|
| L27-013 模板锚 | `DOCUMENTATION_STANDARD.md:31`、`README-DOCS.md:24`「点名 MODULE_README_TEMPLATE.md」 | 该标准全文仅 **14 行**；`docs/` 全域 grep `MODULE_README` = **0 命中**；模板只存在于控制包解压区并被 `tools/quality/gen_module_readmes.py:5` 引用 | **子断言剔除**，其余事实入 M8a-I-002 |
| L27-014 数字 | 「tools/README.md:95 称 quality/ 34 个检查器」 | 文件 **83 行**，grep `34|检查器` = **0 命中** → 属转述未复核线索 | **整句剔除**，改立 M8a-I-003 为"无索引 + 无边界声明 + 与工具链政策相悖"，数字全部换成 R12 口径 |
| L27-014 附注 | 「`docs/governance/` 只有 VERSION_NAMESPACES.md」 | 事实成立（glob 1 文件）但与本条主张无因果 | 降为背景事实，不入定稿 |
| L25-009 数字 | 「13 个工具缺失」「相差一天」 | 锁已刷新，两数不再复现 | 数字不继承，改写为 M8a-I-007 |
| L25-007 两断言 | 2.4.1 互斥 / astropy 冒名 | 见 §3 行 1-2 | 剔除，缩窄为 M8a-B-001 并降 P2 |
| 陈旧锚（多处） | `tools/backend/rcr_oracle_compare.py`、`tools/quality/rcr_oracle_compare.py`、`lib/phase2_rej/include/astro/phase2/rejection.h`、`tools/quality/check_release_consistency.py`、`lib/phase1/noise/README.md` 的 descriptor 行区间等 | 均已不存在或已改路径 | 一律以当前路径 + 符号锚替换（见各条位置行）；行号漂移按纪律**不作移除理由** |

### 4.2 降级（P1→P2）与不升级裁决

| 项 | 原级 | 定稿 | 理由（须写出实质差别） |
|---|---|---|---|
| L25-004 → M8a-G-009 | P1 | **P2** | 不在根交付图/安装树/产品单元 → 义务面止于源码分发；附"遗留通道接线即回升"条款 |
| L25-007 → M8a-B-001 | P1 | **P2** | 两个硬断言被否证/修复，残留为引用形式与可复现性结构问题，非成批一致性失效 |
| L27-002 → M8a-G-006 | 建议升 P0 | **维持 P1** | 与三家 P0 的**实质差别**：①那三条（注释门空壳 / 线程预算门 0 对象自证 / AST-API 恒真）的断言在结构上**永不可能为假**，属"检查器不存在"级；本门断言**可为假**（删 `lib/phase1/wcs/wcs_tan.cpp` 或改 README 不含 "L2" 即变红），是"窄门"不是"假门"。②那三条守护的是宪章 §12.3 **明列**的机器一致性项（-5/-8），条文存在而门恒真 → "已验证声明"直接失据 = 发布阻断；本门守护的"模块 README 8 要素完备性"在 §12.3 十二项中**无对应条文**（§8.4 只要求文件存在 + ID 链接）→ 缺的是"更强的门"。③后果面：本门失效只让文档类问题不变红，产品不合规的证据链由其他门承担。④仍附**条件升级条款**（比照 R-4）：若负责人把 §12.3-1/-3 判为"README 声明的合同锚必须可解析到 INDEX.yaml"，则本门对现存 5 份给出绿灯即构成对交付物的假验证声明 → 升 P0 并与 M8a-E-001 并档 |
| L27-003 → M8a-G-007 | 建议升 P0 | **维持 P1** | 同族同理：生成器不是"恒真门"而是**根本没接进门**（0 命中）+ 声明失实（自述被 checker 引用）；缺的是"接线 + diff 门"，其失效后果面为文档漂移而非交付物合规误判。与 M8a-C-002 合看仍不阻断发布，故 P1；若负责人把 §12.3-1 的"注册表一致"判为要求 registry 页必须与 descriptor diff 为空，则同样按条件升级条款升 P0 |
| L27-009 → M8a-E-002 | P1 | **P1（维持）** | 不降级的理由：该 README 的「相关文档」要素**三份全悬空**（glob 0 文件），是要素级完全失效而非个别锚漂移；与 M8a-E-004（4 处小项，降 P2）的差别在"整要素为空 vs 个别路径写法" |

### 4.3 转述内容复核（前台/M2b 转来的两条）

| 转述 | 复验结果 | 处置 |
|---|---|---|
| 「M2b 的 **L15-002 许可证面**（L25-001/002 同型）：DISP-HIPS 系**文档**把外部依赖写成 Healpix_3.83 GPL-2+」 | ①L15-002 在 `_cache/L15.md:56` 与 `_merge/M2b.md:34/:98` 的定身是 **hips_pixel_scale 以角秒写入度键（3600 倍，P0/B_STD，主落 M2b-B-03）**，不是许可证面；②许可/派生面的实际定稿是 **L03-007 → M2b-G-04**（`_merge/M2b.md:20`，NOTICE :19/:22 与 `healpix_core.cpp:338/:418` 的 Healpix_3.83 移植声明互斥）；③"Healpix_3.83 … GPL-2+ 参考" 字样出现在**生产源码注释**（`healpix_core.cpp:338-339`、:418）与测试注释，**不在 docs 的 DISP-HIPS 文档**——`docs/` 全域 grep `Healpix_3\.83|GPL-2|healpix_cxx` = **0 命中** | 采纳其**族属**判断（与 L25-001/002 同族），但按复算改挂到正确的源与主落：**M8a-G-001 只作同族引用，主落仍为 M2b-G-04，不重复登记**；文档/源码位置之辨写入 M8a-G-001 四态判定段与 M8a-I-004 |
| 「请并成一条『依赖与许可登记面三处失真』定稿，引用 C-07 建议条款」 | 已执行：`findings/G_GOV_GATE/p0/M8a_L25_L27.md` 重写为**一条三面**（面①许可传染 / 面②清单零登记 / 面③派生署名），文首登记**别名规则**（其余文件中的 `M8a-G-002` 一律读作 `M8a-G-001②`，不另立条、不重复计数），并引用 `问题扫描/40_OWNER_DECISIONS.md:59` 的 **C-07 依赖与许可登记门**作为处置落点 | 完成；P0 数由"2 条源 P0"变"1 条定稿 P0（含 3 面 + 1 个交付末端 P1 面 M8a-C-006）" |
| 「M2b 已确立通则：`问题扫描/**` 自写文件不计入真源统计」 | 已按该通则重跑分母（R15）：`问题扫描/**` 37 份 README、`run/**` 847 份影子树 README 全部剔除 | 完成；见 §5.2 |

---

## 5. 覆盖面分母

### 5.1 L27 原报口径（依必核⑥**原样收录**，不作改写）

- 目录单元分母：**92** 个应具文档的目录单元；有 `README.md` 者 **43** 份 → 覆盖率 **46.7%**。
- 逐要素打分：**43 份全部逐要素打分**（负责人 8 要素：功能/定位/相关文档/接口/构建安装/测试/DISP 偏差/状态词）。
- manifest：**21 份 `lib/**/module.yaml`**；registry 页 **26 份**。
- ID 引用面：**86** 个 SCI/ALG 引用中 **2** 个未注册；**93** 个 DISP 引用中 **1** 个未登记（DISP-PSF-007）。
- 悬空路径引用：**10** 处。
- §2 四源矩阵：**23 模块** × README / module.yaml / registry 页 / descriptor 四源对照。
- L27 自述的缺口二分：「有要求无门」→ G_GOV_GATE；「标准缺要求」→ I_DOC_HYGIENE（本档案完整保留该二分）。

### 5.2 M8a 独立复算（依前台方法论令 + M2b 通则重算，逐条可复现）

- 口径：glob `**/README.md` 全仓 = **951** 路径；按下列清单逐条剔除后得**在范围标准命名 README = 42 份**，另 1 份异形命名 = **43 份文档面**（与 L27 的 43 数值一致，但口径已显式化）：
  - `问题扫描/**` **37**（本扫描自写档案，依 M2b 通则**不计真源**）
  - `run/**` **847**（影子树，规程免报区）
  - `.pytest_cache/**` 5、`engineering/control/**` 9、`docs/archive/**` 2、`工程控制/**` 4、`reports/**` 1、`BASS DR3/**` 1（用户/资料区）、vendored cfitsio 1、根 `README.md` 1、`lib/healpix_db/archive/legacy/` 1
  - 异形命名 1：`runtime/core/phase_lifecycle.README.md`（不计入"每目录 README.md"，单列为 M8a-I-001 建议条款第④项的处置对象）
- 与 L27 的差异说明：L27 的 43 与本代理的 42(+1) 之间的 1 份差 = 是否把**根 README** 计入；两份数字都不含 `问题扫描/**`（前台通则已核实）。
- 门实覆：**5 / 43 = 11.6%**（对 92 单元 = 5.4%）→ 用于 M8a-G-006。
- 目录单元数 92 **沿用 L27 且未由本代理重算**（重算需目录枚举，本代理只做了文件级 glob）；如需以 92 为验收分母，建议由前台用目录枚举一次性钉死口径。

---

## 6. 规范缺口二分与建议新增规范条款（供 SUMMARY 录入）

### 6.1 二分保留（不混淆）

| 形态 | 定义 | 类别 | 本域条目 |
|---|---|---|---|
| **有要求、无门 / 假门** | 条文存在（宪章或标准）但无机器门，或门恒真/覆面极窄 | G_GOV_GATE | M8a-G-003、-004、-006、-007、-001 |
| **标准本身缺该要求 / 缺定义** | 无条文可依，故无法建门、无法验收 | I_DOC_HYGIENE | M8a-I-001（每子库 README + 3 项要素 + §8.4 义务范围目录不存在）、M8a-I-005（字段值域标准在仓外）、M8a-I-006（entrypoint 无定义） |

### 6.2 建议新增/订正条款清单

| 编号 | 内容（一句话） | 事实依据 | 归属 |
|---|---|---|---|
| **C-07**（已由负责人登记于 `问题扫描/40_OWNER_DECISIONS.md:59`） | **依赖与许可登记门**：PUBLIC 链接的非 vendored 库（如 GSL）必须出现在 `DEPENDENCIES.md` 与 `dependency-lock.json` 并带许可证；派生目录的 LICENSE 必须覆盖上游著作权 | L25-001/002 → 本域 **M8a-G-001**（一条三面） | 落地为 `check_third_party_registry.py` + lock 反向门（M8a-G-003/-004 已给机器门设计） |
| **R-01（本代理新提）** | **模块 L2 文档义务与最少要素**：交付 target 所在目录与治理目录必须有 `README.md`，含 功能/定位/相关文档/接口/构建安装/测试锚/偏差登记/状态词 八项；`DOCUMENTATION_STANDARD.md` 要素表补「构建安装/偏差登记/状态词」三项并规定落点；目录枚举为门的数据源，豁免须登记 owner+理由 | L27-012 → **M8a-I-001**（含 §8.4 义务范围目录 `lib/algorithms/` 在仓内**不存在**这一硬事实） | 需先裁决 §8.4 的路径表述（宪章改动权仅在负责人，§1.2） |
| **R-02（本代理新提）** | **术语定义**：`entrypoint` 二选一（导出符号名 / registry 接线状态），第二义另起键名（建议 `registry_descriptor_wired`），写入 `docs/GLOSSARY.md` 并把字段规范迁入仓内标准 | L27 §6 → **M8a-I-006**（义 A/义 B 混用实例已举证的 6 处） | 负责人/GOV 裁决；未裁决前 M8a-C-002 的门只能做"存在性"不能做"一致性" |
| **R-03（本代理新提）** | **状态词唯一口径**：L2 文档/manifest 的状态字面量只能取 `docs/owner/RELEASE_STATUS.md` §0 阶梯；生成器模板禁写 blanket「已验/确定性」断言 | L27-016（`FROZEN`）、L27-003（26/26 registry 页一律 `status: ACTIVE`）→ **M8a-C-008**、**M8a-G-007** | 并入 R-01 的要素③ |
| **R-04（本代理新提）** | **交付许可面必须由登记面派生**：发布包 `LICENSES/NOTICE.txt` 与 SBOM 禁止脚本内字面量，必须由 `dependency-lock.json` 生成 | **M8a-C-006** 自证（NOTICE 漏列 GSL/Siril、cfitsio 误标 BSD、HEALPix 归属误指 `lib/plate_solve/LICENSE`） | 与 C-07 同批落地 |

---

## 7. 已作废范围（原 L21+L22 取证，交回 M7 / M8，不随范围更正丢弃）

> 本代理收到「改做 L25+L27」更正时，对 L21/L22 只完成了**取证阶段**（未产定稿、未写 findings）。以下按原证据原样登记，供 M7（L19+L20+L21）与 M8（L22+L23）直接续用；本代理不对这些条目做任何定档。

### 7.1 交回 M7：L21（跨文档物理量口径矩阵）

- 源规模：14 条定稿候选（P0:1 / P1:9 / P2:4）+ 种子扩展实例 8 + 待复核 6；矩阵本体 = `问题扫描/_cache/L21.md::附录 A`（量 × 出处 × 口径），**逐行原样可续用**：宪章 §4.1 十量（signal/variance/ivar/snr/quality/support/coverage/validity/rejection_mask·count/provenance）+ 常数·单位行 14 条（K、pixfrac、min_samples、sigma 因子/IRLS、MAD→σ、mag 零点、FOV、nside/order、CRPIX 基、deg/rad、sr/px²、ADU/e⁻、weight 族）。
- 符号含义（沿用 L21 自定）：✓=一致，⚡=该轴定稿，🌱=种子已登记/扩展实例，○=无权威条目。
- M7 需知的两个本代理顺带核实的事实：①`snr`、`quality`、`validity`、`provenance`、`rejection_mask/count` 在 `docs/GLOSSARY.md` 无条目（L21-002 的 5/10 缺项与本域 **M8a-I-006** 的 `entrypoint` 缺项是同一文件的同一问题，可并档）；②L21-009 已按 `00_COORDINATION.md` 的改判令降级（不得写「互操作被锁死」，须先找实际校验点）。
- 待复核 6 项（L21 §4，本代理未动）：UPM 归一化 Σ 域、`snr_weight_mode=0` 是否有消费点、交换校验器是否强检 planes↔磁盘、stage2 读帧 sr↔px 换算、`hips_frame` 全链阻断点（禁执行，标"禁执行"未验）、`GLOSSARY:17 bad_mask` 行锚命中性。
### 7.2 交回 M8：L22（并行确定性与 bitwise 声明证伪）

- 源规模：10 条定稿候选（P0:0 / P1:7 / P2:3）+ §1 声明面 **A1-A12** 逐条判定表 + §2 算子面清点 + §6 待复核 7 + §8 反证 3。
- **A1-A12 判定表原样交回**（M8 可直接引用），其中本代理读取时确认的三条"正面样本/不成立项"最需注意：A5（PHOTOMETRIC_FIT + DATA_SEMANTICS:484-485 bitwise，代码侧核验**成立**，属"对未构建模块的现行口吻"家族，L22 自判不另立）、A6（cosmetic bitwise **成立**；dynamic 版不在根图）、A7（star-detection bitwise **有真跨线程证据**：p1star properties F3 显式 set 1/2/4 + memcmp，ctest 注册进根图 = 正面标杆）。
- §8 三条反证/改判原样交回（M8 定档时必须吸收）：①`reduction(+:…)` 类**不是**科学浮点归约子族（16 处全为 int/long long 计数或 prof 计时），真危害在合并折叠树（drizzle `threadTiles`、upm `tsums`）→ M4/M8 若按"reduction 危害"定档即打错靶；②L12 种子「SparseEqualsDense 无注册执行面」在当前树**已被 B3-A4 批修复**，残留只有 GTest QUIET 静默零注册面；③F00-03 对 `pc_api.cpp` 的量化（14 处裸 omp / 2 处 dynamic+reduction）与现树（19 处 / 3 处）不一致 = 并发编辑漂移，引用前须按符号重计票。
- 「声明 = 证据」六项正面标杆（供 SUMMARY 证明"问题非能力缺失而是纪律未一致执行"）：p1star F3、p1wcs F5、p1drz properties 组、UT-CPU 三 oracle、dpsf B2-A2、ipv 顺序安全论证 —— 证据行仍在 `_cache/L22.md §1/§4`，本代理未复算（非本域），故不填判定列，避免越权定档。
- 并发修改实证（L22 §7）：扫描期间 `docs/algorithms/UPM_SOLVER.md` 两次读取总行数 98→100；本档案全部沿用**符号锚**，行号仅作复核时点附注。

---

## 8. 数字口径替换表（凡转述数字，定稿一律改用本代理口径）

| 转述数字（来源） | 本代理定稿口径与结果 | 用在 |
|---|---|---|
| 「43 份 README / 92 目录单元」（L27） | 951 → 在范围 42 标准命名 + 1 异形 = 43 文档面；92 单元沿用未重算 | §5 |
| 「门实覆 5/43」（L27） | 5 / 43 = 11.6%（对 92 单元 5.4%） | M8a-G-006 |
| 「`tools/README.md:95` 称 quality/ 34 个检查器」（L27-014） | **该句不存在**（83 行）；改立：tools 218/160、quality 97、check/verify/audit 脚本 63（quality 内 24） | M8a-I-003 |
| 「三向一致 1 + 双轨 19/21」（L27-008） | 值集合精确比对：**21 份 manifest 与 22 个 descriptor 的 module_id 精确交集 = 0** | M8a-C-005 |
| 「26 registry 页与 22 descriptor 大体对齐」（L27-003 语气） | 集合差 = 页面多 4（全为手写页），descriptor 无缺页 | M8a-G-007 |
| 「toolchain.lock 列 13 工具缺失」（L25-009） | 当前锁 `missing_tools` = **1**（仅 ccache）；失真侧翻转为 INVENTORY 报告 | M8a-I-007 |
| 「rcr 2.4.1 与 2.4.7 互斥」（L25-007） | 全树 13 处一致 2.4.7（docs 5 / lib 7 / tools 1，11 文件） | M8a-B-001 / M8a-F-001 |
| 「`healpix_stack`/ 活跃（独立仓库）」（L27-006） | 实际在 `lib/healpix_db/archive/legacy/healpix_stack/`，且 .gitignore 无 healpix 条目 | M8a-C-004 |
| 「第三方 = `third_party/` 目录」（全仓脚本口径） | `third_party/**` 实树仅 1 文件；真实 vendored/派生有三处在别的目录 | M8a-G-003 / -005 / -001 |
