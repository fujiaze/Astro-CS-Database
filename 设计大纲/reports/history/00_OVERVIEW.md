# 00_OVERVIEW — AstroCS 项目历史溯源总览

> 任务一只读取证产物。覆盖率、阶段划分依据、模块演进主线、编号族-阶段对照、模块起止索引、分片报告指针。全程不含源码/diff；每条断言给 seq+sha8 指针，可回指 _evidence/ 或分片报告。

## 0. 覆盖率声明
- 提交总数 1988（拓扑 seq 1..1988，日期 2026-05-24 至 2026-09-14；seq 为谱系聚合拓扑序，非日期序）。
- 分片 37（S01..S37），每片主责 55 条（末片 8 条），相邻片重叠复核 10 条。
- 主责提交覆盖 1988/1988，缺失 0，覆盖率 100.00%（脚本 设计大纲/_tools/04_check_slice_coverage.py → 设计大纲/reports/history/coverage.csv）。
- 正文条目组合计 1568（同编号连续提交合并；每组列全成员 seq 与 sha8）。

## 0a. 重叠区核对结果（36 对 × 10 = 360 条）
全量回核见 设计大纲/reports/history/overlap_recheck.md（三组核对员分段完成，逐条保留双方原文引述）：
- 一致 350 ／ 口径差-可兼容 8 ／ 不一致 2（不一致率 0.56%）。
- 不一致 2 条及裁定：seq 161 | ed145a70（S04 计数 16 vs S03 计数 19+1，以 index.jsonl 文件清单裁为 19+1，采 S03）；seq 1150 | 163e9e56（S21 称 ASYNC_IO_CONTRACT.md"新增"，S22 核实为修改 M +19/-0，采 S22）。
- 口径差 8 条：seq 49、660、821、929、1371、1427、1429、1922（均为列举粒度/措辞/条目粒度差，定性判断两侧一致）。
- 系统性发现：证据卡"目录聚合行数"与"每文件变更行数"两口径对含新增文件的目录固有差异（1373/1375/1421/1532/1538/1540 等）。本套报告的行数引用未统一二选一，引用具体行数时以 设计大纲/_evidence/commits/cards/ 目录聚合行或 git numstat 为准。

## 0b. 独立抽查结果（30 条随机提交，回读 git show 复算）
见 设计大纲/reports/history/spotcheck.md：
- 误读率 0/30 = 0.0%；相符 27/30 = 90.0%；部分相符 3/30 = 10.0%（seq 349 落点目录前缀错置、seq 1549 组条目漏本条最大代码落点 ci/tests +141、seq 1898 漏新增 P0 定稿条目最大落点）；最严口径误读上界 2/30 = 6.7%。
- 数字面：28/30 的文件数/增删行/清单与 git 三方一致；2 处行数字面误差可追溯至证据卡"每文件行"清单偏小（非报告编造）。
- 交叉引用：父提交/前后 seq/被引短哈希逐条反查，仅 1 处不成立（seq 791 条目将 seq 771 并入预告）；"引用哈希不存在"类反向存疑标记（seq 824 的 4bc2a38）核实为真。
- 结论：分片报告无编造式误读；主要风险模式为"以消息自述代替变更面"的组条目归纳残留，建议后续引用时对组条目逐 seq 反查一次 numstat。
- 归纳层注记 1：H-S03 对 seq 129 的"710 帧 × 3 次重复"表述经抽查复核应为"710 帧各 1 次 + 21 个 _run2/_run3 文件"（与其自报 740 文件总数矛盾）；引用该条时以 numstat 复算为准，原条目文本保留不改。
- 归纳层注记 2：seq 528（lib/acr 休眠非目标域）与 seq 1074（tools/ 静态 checker）被条目归入"生产代码"未标语境，跨阶段统计"生产代码量"时应对此类条目作语义换算；另 seq 791 组条目的 crypto 单源预告不应包含 seq 771（后者为 HEALPix 映射权威 [B2-08]）。
- 归纳层注记 3：证据卡"每文件变更行数"清单存在系统性偏小（compact 卡继承），本套报告凡引用行数处以 cards/ 目录聚合行或 git numstat 为最终口径（对应可复核命令已在 spotcheck.md 末节给出）。

## 1. 阶段划分（8 大阶段）

判据仅用可核对信号：控制包引入/废止（PACKNEW/PACKDEL）、目录谱系重构（RENAME50/DELETE/NEWDIR）、CI 门禁体系变化、任务编号族更替、根谱系边界（主仓初始化 seq 100）、明显时间断档（SILENT）。一手来源 设计大纲/_evidence/commits/structural_events.csv（1001 事件）。

| # | 阶段 | seq | 日期 | 提交数 | 主判据（seq | sha8 | 事件） |
|---|---|---|---|---|---|
| 1 | 上游模块起源与第一代工程控制包 | 1–271 | 05-24→08-02 | 271 | 13 根→主仓初始化 seq 100 c384de17(SILENT 6天)；PACKNEW engineering v1.0 seq 118 1ac74251→v1.1 seq 128→v1.2 seq 162→v1.3 seq 175；权威包 seq 204 75b05f72(A..I Gate)；编号族 无→P00-P13→A..I Gate |
| 2 | ACR 底座与 Phase1 HiPS 换代 | 272–550 | 08-02→08-10 | 279 | seq 272 f8d749e4 建 lib/acr+ADR-001~009；ACR_FOCUSED V1→V4 四连 PACKNEW(406/429/438/460)；DELETE Python 生产层 310/322；HiPS 直写+CFITSIO vendored 498；ACR 休眠并入 merge 550 198d69e0 |
| 3 | Phase2 开工与 v6→v19 快速迭代 | 551–715 | 08-10→08-16 | 165 | seq 552 Phase2 控制包 V1、lib/phase2 首现(553)；浏览器 616；版本前缀 v6→v17→v18→v19→v19r2/r3；docs/standards 与 docs/science/algorithms 首现 700 |
| 4 | V19R4-R8 质量闭合与 REAUDIT_V3 | 716–990 | 08-16→08-27 | 275 | 5 天空窗 729→730；QA-V19R7 控制包 735(95 任务)；V19R8 立项 816-820；CON-001 首现 965→966；reports 版本化 980 |
| 5 | REAUDIT 收口、V4 更替与 V5 单 CLI | 991–1260 | 08-28→08-30 | 270 | V5 起点 PACKNEW RELEASE_V5 1042 f99e80d8；cli/main.cpp 首现 1088；编号族 V5 (TASK-ID) 括号族；V4 自造→归位→更替 1037-1039 |
| 6 | V6 重构、V6.1 返工与 V7 合同-实现 | 1261–1520 | 08-30→09-05 | 260 | PACKNEW CONTROL_V6 1261 4b1b948e；V6→V6.1 空窗 575 分 1360→1361；RENAME50 归档 1480；V7 三重切换 1474→1475 |
| 7 | V8.1 CI 控制包与 W1 合同冻结波 | 1521–1785 | 09-05→09-09 | 265 | PACKNEW V8.1 注册 1521 a4fdee3f；evidence/v8_1_ci_control 引入；控制包 k/140 冻结波；台账规则换轨 1668 b05288b8；记账线终结 1780 |
| 8 | 宪章定纲、真实数据、CI 转绿与 RQS 普查 | 1786–1988 | 09-10→09-14 | 203 | 宪章冻结入库 1786 d8c821db(GOV-001)；RESCUE-V3 1850；问题扫描 1859 RQS；CI 转绿 last-step 1944；FIX_LEDGER 1945；账本完备性 1988 |

各阶段详述见 STAGE-01.md … STAGE-08.md（固定七段结构）。

## 2. 模块演进主线一览
- 科学算法内核：上游 7 个 lib 模块（H-S01）→ Python/C++ DLL 双栈→C++（STAGE-01）→ Phase1 冻结+HiPS（STAGE-02）→ Phase2 叠加/rejection（STAGE-03）→ B 批锚点级微调（STAGE-04）→ SCI/ALG 合同冻结不改语义（STAGE-05）→ P1/P2/P3 迁移（STAGE-06）→ 模块独立化 IMPL（STAGE-07）→ 宪章逐面委托落地（STAGE-08）。
- CLI：无→orchestrator.exe 单入口（seq 320）→ astrocs-stage2（563）→ 单一 astrocs target（1088 cli/main.cpp）→ 入口壳+parser/commands/runtime_client（1379）→ 删 run --phases 三 Phase 隔离（1519）→ 单一 astrocs（宪章 §8.1）。
- 构建与 CI：无门禁（STAGE-01）→ path_guard/ACR_BUILD_SANITIZER（296/308）→ tools/quality 52 件（965）→ GOV-001 AGENTS 机器门（1043）→ ci/checks.json（1527）+ ci/tests（1528）→ 台账 reconcile 稳态（1668）→ CI Batch3 真启用（1872）→ known_failures 清空转绿（1944）。
- 控制包与治理：engineering v1.0→v1.3→工程控制→权威 v2.0（203/204）→ tasks/acr 无版本包（293）→ ACR_FOCUSED V1-V4（406-460）→ QA-V19R7/R8（735/816）→ V4/V5/V6/V6.1/V7/V8.1→宪章对齐包（1796）→ RESCUE-V3（1850）→ RQS 只读审计（1859）。
- 文档与报告：逐模块 memory.md/README（49-55）→ docs 六分层（624）→ L0-L5 权威层级（700）→ reports 版本化归档（980）→ docs/science|algorithms|governance → 宪章为 SSOT（1786）。
- 测试：模块 ctest→p1noise 六域验证面（1769 等）→契约 pytest 同构套件（1486+）→独立 Oracle 方法学（440 CAND-001）→V19R7 锚点（B 批）→ V5 五门机器复跑+胶囊。

## 3. 编号族与阶段对照表
| 阶段 | 主编号族 |
|---|---|
| 1 | 无（上游）→ P00..P13 → A-001..I-002 Gate → R04..R07（审查轮） |
| 2 | ADR-001..010；R07..R13；Commit A–G/F-fix；IMG/CFG-001；P13-003 |
| 3 | v6..v19（版本前缀）；R；HOLE/SEAM；G4..G7；QA-*/self_review；v19r2-s*/v19r3-s* |
| 4 | V19R4/R6R2-W1；QA-V19R7 A/B/C/D；V19R8 S0-S6；Stage A/B/C；P1..P5；CON-001..010 |
| 5 | G2..G7；V4 C0-00x；V5 (TASK-ID) 括号族（SCI/ALG/ARCH/API/ABI/CLI/ISA/BENCH/PAR/SYN/LNX/WIN） |
| 6 | V6 G0..G11 + DOC/SCI/DATA/RT/CPU/MON/P1..P3/QA/REL；V7 GOV/LOG/ARC/BLD/DATA/ABI/RT |
| 7 | V81-ADOPT/V8-CI-0xx/CIQA；W1 控制包 k/140；R3..R13 批次字母；CPU/MON 治理号 |
| 8 | 宪章 §x.y；AIO/P1/P2/WCS/P3/RT/MOD/CLI/CI/REAL；R-01..14 裁决；RESCUE/FD；RQS/L 轴·域；FIX_LEDGER V{n}-N-xx；P1..P11 批次 |

注：紧凑卡"消息中的编号"字段对 SCI-*/ALG-*/R-CPU 等存在系统性误拆（H-S24 警告），统计任务量不可依赖该字段；需以标题+正文实现编号为准。

## 4. 模块起止索引（25–40 条，取自 module_provenance.csv）
| 路径 | 引入(seq | sha8 | 日期) | 末改(seq | sha8 | 日期) | 触及 | 现存 |
|---|---|---|---|---|
| lib/astro_image_io | C2 802becc2 05-24 | C1888 23f96b9c 09-14 | 123 | 是 |
| lib/healpix_db | C31 0c08aeb5 07-11 | C1951 dce8abd4 09-14 | 157 | 是 |
| lib/star_detector | C3 850973a7 05-24 | C1971 8dc0220e 09-14 | 37 | 是 |
| lib/dynamic_psf | C18 12c55ed6 05-24 | C1957 23a7f665 09-14 | 28 | 是 |
| lib/calibration | C24 a88d9789 07-10 | C1884 1dce779d 09-13 | 35 | 是 |
| lib/photometric_calib | C35 f75ae3f0 07-12 | C1958 b0353303 09-14 | 49 | 是 |
| lib/data_pipeline | C45 e27ff13d 07-12 | C680 25a6479b 08-15 | 5 | 否（已消失/改名） |
| lib/plate_solve | C94 75835599 07-16 | C1972 b858b76d 09-14 | 36 | 是 |
| lib/snr_estimator | C75 8399f940 07-15 | C1961 35c85f53 09-14 | 32 | 是 |
| lib/orchestrator | C76 127c4a96 07-15 | C1690 1efef060 09-08 | 105 | 是 |
| lib/common | C327 bd036f3e 08-04 | C1694 9f85b422 09-08 | 15 | 是 |
| lib/acr | C272 f8d749e4 08-02 | C1690 1efef060 09-08 | 137 | 是 |
| lib/phase2 | C553 1a38f400 08-10 | C1951 dce8abd4 09-14 | 144 | 是 |
| lib/phase3_session | C1117 9fb12dc0 08-29 | C1857 66906609 09-13 | 28 | 是 |
| lib/phase3_proj | C1722 de15efc9 09-08 | C1806 9953f103 09-11 | 2 | 是 |
| lib/backend_host | C1094 7718a2f8 08-28 | C1743 aceca50c 09-09 | 31 | 是 |
| lib/core | C1277 3e98a22d 08-31 | C1980 0e4cee72 09-14 | 55 | 是 |
| lib/io | C1285 2f39880c 08-31 | C1716 90680db9 09-08 | 3 | 是 |
| include/astrocs | C1094 7718a2f8 08-28 | C1977 57c04256 09-14 | 37 | 是 |
| cli/main.cpp | C1088 07bc18b9 08-28 | C1719 47ac0d40 09-08 | 21 | 否（已改名/搬家） |
| engineering/evidence | C118 1ac74251 07-24 | C203 036a3bb5 07-29 | 40 | 否（搬家到工程控制） |
| engineering/control | C118 1ac74251 07-24 | C1787 8e03d7da 09-10 | 31 | 是 |
| docs/science | C700 8ab2efbd 08-15 | C1958 b0353303 09-14 | 39 | 是 |
| docs/algorithms | C700 8ab2efbd 08-15 | C1958 b0353303 09-14 | 74 | 是 |
| docs/contracts | C624 5a9eadcf 08-13 | C1980 0e4cee72 09-14 | 58 | 是 |
| docs/architecture | C624 5a9eadcf 08-13 | C1951 dce8abd4 09-14 | 52 | 是 |
| docs/api | C1078 e08512ba 08-28 | C1872 c21eec50 09-13 | 14 | 是 |
| docs/governance | C1482 39e77317 09-02 | C1526 b4f923cc 09-05 | 2 | 是 |
| tests/unit | C1275 338eecc2 08-30 | C1980 0e4cee72 09-14 | 150 | 是 |
| tests/realdata | C1815 9f6b72b5 09-11 | C1815 9f6b72b5 09-11 | 1 | 是 |
| tools/quality | C698 729bc3b9 08-15 | C1959 80c32b19 09-14 | 57 | 是 |
| evidence/refactor | C1261 4b1b948e 08-30 | C1594 4f6e2b65 09-07 | 96 | 是 |
| evidence/v6_1_rework | C1361 a3d16d85 08-31 | C1754 acceab68 09-09 | 110 | 是 |
| evidence/v8_1_ci_control | C1521 a4fdee3f 09-05 | C1780 9690c1da 09-09 | 189 | 是 |
| ci/checks.json | C1527 0acfb514 09-05 | C1956 6d74046d 09-14 | 22 | 否 |
| ci/known_failures.json | C1822 778fe98e 09-12 | C1944 ccc7a933 09-14 | 2 | 否 |
| reports/REAUDIT_V3 | C979 ac2ced53 08-27 | C1036 6a947f51 08-28 | 41 | 是 |
| contracts/schemas | C1782 009ee419 09-09 | C1782 009ee419 09-09 | 1 | 是 |

"现存=否"即历史上存在后消失/改名的路径，为阶段结束去向交代的一手证据（如 cli/main.cpp→入口壳搬家、data_pipeline 外迁、engineering/evidence→工程控制、ci 配置末态归位）。

## 5. 分片报告指针索引
H-S01 … H-S37 见 设计大纲/reports/history/slices/。逐片：开篇 seq/日期关系声明 → 主责逐条正文（六/七字段，带 seq+sha8）→ 重叠区核对 → 本片主题归纳 / 重叠一致与不一致 / 遗留与证据缺口。coverage.csv 为逐片覆盖机器校验。structural_events.csv/.md、module_provenance.csv 为阶段边界与模块起止一手底稿。
