# W2-DOC2 · 第二波文件级审计 — docs/plugins/**（23 篇 + 00_INDEX，共 24 文件）

- 开工基线：HEAD=main=180c8a0a，origin/main=b4afc135（三 SHA 不等；按口径等待约 2 分钟后复跑一次得 HEAD=main=59981aa ≠ origin/main，仍不等 → 以 **HEAD=main 相等**开工并注明。并行线窗口内持续推进 180c8a0→59981aa→23f42ff）。
- 收工基线：HEAD=main=3580443（写毕复核；窗口内并行线持续推进 180c8a0→59981aa→23f42ff→3580443），origin/main=b4afc135（全程未同步）。
- 必读已读：AGENTS.md、ASTROCS_DESIGN.md（全文，§0 权威链）、ENGINEERING_SPEC.md、SHARD_BRIEF.md §1/§2；对账锚：docs/architecture/MODULE_MAP.md、contracts/schemas/**、cli/**、lib/algorithms/**、lib/infrastructure/**。
- 双口径声明：工作树含并行线未提交 WIP（git status 631 行）。涉及 WIP 的判定均区分「tracked 于 HEAD」与「存在于工作树」；本域 docs/plugins/** 自身零改动、零 untracked（开工/收工各测一次均为空）。
- 8 节模板符合性：**23 篇全部符合**（python 机器扫描节号 1–8 + 标题逐字对 00_INDEX §3 模板；00_INDEX 为索引不适用）。
- 日志：run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/w2_DOC2.log

## ① 发现表

| ID | 定位 path:line | 违反的最新权威条款 | 当前证据（命令＋本轮输出，≤3 行） | 严重度 | 影响 | 整改建议 | 建议文件域 | 验收门（单命令） | 同源标注 |
|---|---|---|---|---|---|---|---|---|---|
| W2-DOC2-1 | 21 篇 §3「参考」行：01:18、02:18、03:19、04:19、05:20、06:18、07:25、08:21、09:18、10:17、11:18、12:17、13:28、14:18、15:18、16:25、18:10/16、19:17、21:11/16、22:18、23:17 | ENGINEERING_SPEC §4-7「输入/输出端口引用有效 DATA 合同（contracts/schemas/ 唯一事实源）」＋§8「无悬空引用」；DESIGN §0（插件文档=链末规范面）；UNIFIED_MODEL §2（数据对象表才是现行合同锚） | 命令：对 21 个被引 schema 名逐一 find contracts -name "<n>.schema.json"（排除 v6）；输出：data_light ABSENT / mosaic_product ABSENT / events ABSENT / catalog_query ABSENT …（21 名全空，仅 artifact_manifest 在 contracts/data/ 非所引路径）。git ls-files contracts/schemas → tracked 仅 7 份，run_*/phase_config* glob 于 HEAD 零命中 | P1 | 21 篇文档的 I/O 数据合同节整体指向不存在的 schema 文件：23 模块端口/合同对账不可机械执行，「改合同先改插件文档」（00_INDEX §5）失去锚点；真正的对象级合同链 contracts/schemas/unified/*.schema.json（16 份）已在并行 WIP 建立（untracked）却无一篇插件文档引用 | 统一改引：科学对象→contracts/schemas/unified/<对象>.schema.json（signal/variance/ivar/frame_snr/sparse_snr_layer 等），产品面→contracts/data/（artifact_manifest、phase_product_exchange），配置面→contracts/schemas/phase_config_* 与 cpu_profile；随 DATA-001 将 unified/ 入库并建「插件文档 schema 引用存在性」检查门 | docs/plugins/**、contracts/schemas/** | timeout 60 grep -rhoE "contracts/schemas/[A-Za-z0-9_.]+[.]schema[.]json" docs/plugins -r 后逐条 test -e，输出零行（整改后） | DATA-001（14 对象合同链在途）；GAP-007（contracts 子域现状）部分同源；第一波 10 轴无此条（新增面）；phase_config_*/run_manifest 属 CFG-001 未入库 WIP |
| W2-DOC2-2 | docs/plugins/infrastructure/18_cli.md:44（上位同文：DESIGN §6.3:323） | DESIGN §6.3「退出码唯一源 include/astrocs/exit_codes.h」——所声明路径在 HEAD 与工作树均不存在 | git ls-files include/astrocs/exit_codes.h → 空且盘上无此文件；sed -n "6,20p" cli/exit_codes.h → 实际唯一源在 cli/，值 0/2/3/4/5/6/7/8/9/10/70 与文档表逐值一致 | P2 | 退出码「唯一源」按文档路径不可寻址，机器对账/新人按 §6.3 取数扑空；数值面与 cli 实况一致，无行为偏差 | 码侧搬迁（cli/exit_codes.h → include/astrocs/，随 CLI-003/ARCH-001 落位）；插件文档保持转写 DESIGN，不动；DESIGN 若改判路径走文档集变更流程 | cli/**、include/**（docs/plugins 侧无需改） | test -f include/astrocs/exit_codes.h; echo rc=$? → 0 | 第一波 CLI 轴在跑（预期重叠）；GAP-005（命令面迁移）部分；文档忠实 DESIGN，属对账偏差非文档错误 |
| W2-DOC2-3 | algorithms_phase1/05_platesolve.md:10、05:12；infrastructure/22_gaia_xpsd_client.md:10；infrastructure/23_hips_browser.md:6 | ENGINEERING_SPEC §8「引用有效、无悬空」；DESIGN §0（权威节号可核）；00_INDEX §3（§2 须具体引用） | grep -c ICRS ASTROCS_DESIGN.md → 0（§3.6 硬约束列表无「WCS 为 ICRS」）；ls docs/algorithms/platesolve → 无此目录（实为 docs/algorithms/PLATESOLVE.md）；grep -rln D-010 docs/ → 仅 23 篇自身（D-010/D-014 决策账在库零出处） | P2 | §2 权威依据混入失实归引（05:10 把 ICRS 挂 §3.6）、错误路径（05:12）、语义错挂（22:10：§3.3 为 JSON 输入合同，无星表条款）、不可核验决策 ID（23:6）；按图索骥即断链 | 05:10 改引 §3.2/§3.6 并删失实括注（科学面可改引 docs/science/ASTROMETRY.md）；05:12 改 docs/algorithms/PLATESOLVE.md；22:10 改引 §7.1；23:6 决策 ID 换在库可核来源或删除 | docs/plugins/** | timeout 30 grep -rn -e "WCS 为 ICRS" -e "algorithms/platesolve/" -e "D-010" -e "输入合同：Gaia" docs/plugins → 零行 | DOC-001；与第一波 GOV-2/GOV-3 同族（悬空引用形态）不同对象 |
| W2-DOC2-4 | docs/plugins/infrastructure/23_hips_browser.md:36 | DESIGN §10.1（唯一可执行入口冻结命名 ACSD Cli.exe / acsd_cli）、§6.2（全部命令由唯一可执行 ACSD Cli 提供） | 23:36 写「不进入 astrocs 命令树」，以旧二进制名为锚；grep -n add_executable(astrocs cli/CMakeLists.txt 命中（现状代码同名）；GAP-018 实测 acsd 于 packaging/cmake/docs/ci 仅 2 处文献 URL 命中、无 ACSD 命名 | P2 | 链末文档固化旧产品名，与 §10.1 命名合同双轨并存，PKG/CLI 改名执行时易被文档反向钉死旧名 | 措辞改「不进入唯一 CLI 入口（DESIGN §10.1）的命令树」；产品名统一订正随 PKG-001/CLI-001 | docs/plugins/** | timeout 30 grep -rn "astrocs 命令树" docs/plugins → 零行 | GAP-018 同源（命名族，不删条）；PKG-001/CLI-001 |
| W2-DOC2-5 | docs/plugins/infrastructure/21_observability.md:30 | DESIGN §6.3:340「运行产物只落配置 output_dir，不得以进程 CWD 作隐式缺省写出」（§0 上位优先；AGENTS §5「或 run/」不豁免该后半句） | 21:30 配置表 event_dir 缺省 = run/<run_id>/ —— 相对路径缺省即以进程 CWD 下 run/ 写出 | P2 | 观测事件落点缺省与产物唯一落位口径冲突，OBS-001/AIO-001 实做时事件目录二义 | event_dir 缺省改「<output_dir> 下 run/<run_id>/」或删除缺省并标注必填 | docs/plugins/**、lib/infrastructure/observability/** | timeout 30 grep -c "run/<run_id>" docs/plugins/infrastructure/21_observability.md → 0（整改后为 output_dir 形态） | OBS-001；与第一波 AIO 轴产物落点族相邻不同条 |
| W2-DOC2-6 | docs/plugins/00_INDEX.md:16、:61（模块名 runtime；落位声明）；关联 infrastructure/19_runtime.md:1 | DESIGN §7.1（顶层结构唯一：16 算法并联 lib/algorithms＋基建含 scheduler/pipeline，无 runtime 目录）；ENGINEERING_SPEC §8（模块 manifest/注册表/构建 target 一致） | ls lib/algorithms → 9/16 落位（phase1 七模块未迁）；for d in lib/drizzle lib/phase3_proj lib/common; do [ -d ] 判定 → 全 GONE，而 MODULE_MAP 自称按 lib 实际目录登记、§1–§3 仍以已消失路径为证据锚；ls lib/infrastructure → benchmark/observability/pipeline 仅 PENDING.md 占位（多目录 untracked WIP） | P2 | 文档↔MODULE_MAP↔代码三轨在迁移窗口互不同步：00_INDEX/DESIGN 彼此一致，但模块名 runtime（=scheduler+pipeline 混称）无 §7.1 对应物，MODULE_MAP 锚成片失效，映射门 check_module_map.py rc=1 持续红（第一波 ARCH-4 已实测） | MOD-001 建「23 篇 ↔ module_id ↔ target ↔ 目录」唯一映射源并重写 MODULE_MAP；00_INDEX §2 增 module_id/target 列（或由映射门生成）；runtime 命名对齐 §7.1 或经负责人注记别名 | docs/architecture/**、docs/plugins/00_INDEX.md、docs/modules/** | timeout 60 python3 tools/quality/check_module_map.py; echo rc=$? → 0 | 第一波 ARCH-4 同源（注册表三轨，含 docs/plugins 域）不删条；GAP-003/GAP-008 同源；归属 MOD-001＋ARCH-001 |

**未立条的实据观察（宁缺毋滥，留档）**：14_projection 八投影 vs registry 实测四投影（p3_projection.cpp:38「四投影统一中心守卫」、:267 表 TAN/SIN/CAR/AIT）——文档忠实 DESIGN §5.3，缺口在代码，GAP-011 逐字在账，不另立；07/08 帧级 SNR 入 HiPS 文件头属目标态（GAP-009 在账）；19_runtime workers/block 行明示「由 profile、不得硬编码」，不越 DESIGN §2/§8；17_aio 与 20_benchmark 所引 glob/schema 有 tracked 命中判 OK；PHASE1/2/3_DETAILED_DESIGN 节引用抽全（01–16 各篇所引 §2–§9 逐节命中）；DESIGN 节引用（§1.3/§3.2/§3.3/§3.4/§3.6/§4.2/§4.3/§4.4/§5.1–5.4/§6/§7.1/§7.2/§8/§9/§10.1）全部存在且语义基本对应（失实括注已并入 W2-DOC2-3）。

## ② 覆盖清单

规则：每文件唯一 VERDICT，取该文件命中的最重发现 ID；W2-DOC2-2/3/4/5 亦分别命中 18、05/22/23、23、21 各篇，因这些文件同时命中系统性 P1（W2-DOC2-1），以 P1 为准，逐条锚点见发现表定位列；同型问题按简报归并为一条发现。

docs/plugins/00_INDEX.md	FINDING:W2-DOC2-6
docs/plugins/algorithms_phase1/01_calibration.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase1/02_cosmetic.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase1/03_star_detection.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase1/04_psf.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase1/05_platesolve.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase1/06_photometry.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase1/07_noise_snr.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase1/08_drizzle.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase2/09_coverage.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase2/10_sampling.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase2/11_upm.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase2/12_rejection.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase2/13_integration.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase3/14_projection.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase3/15_resample.md	FINDING:W2-DOC2-1
docs/plugins/algorithms_phase3/16_fits_output.md	FINDING:W2-DOC2-1
docs/plugins/infrastructure/17_aio.md	OK(已读无发现)
docs/plugins/infrastructure/18_cli.md	FINDING:W2-DOC2-1
docs/plugins/infrastructure/19_runtime.md	FINDING:W2-DOC2-1
docs/plugins/infrastructure/20_benchmark.md	OK(已读无发现)
docs/plugins/infrastructure/21_observability.md	FINDING:W2-DOC2-1
docs/plugins/infrastructure/22_gaia_xpsd_client.md	FINDING:W2-DOC2-1
docs/plugins/infrastructure/23_hips_browser.md	FINDING:W2-DOC2-1

files_total=24（tracked 24；untracked 0）
verdict_counts: OK=2 | FINDING:W2-DOC2-1=21 | FINDING:W2-DOC2-6=1 | NA=0（发现合计 6 条：P0=0 / P1=1 / P2=5）
枚举命令逐字：cd "/workspace/Astro CS Database" && timeout 30 git ls-files docs/plugins（24 行）；cd "/workspace/Astro CS Database" && timeout 30 git status --porcelain docs/plugins（空）
