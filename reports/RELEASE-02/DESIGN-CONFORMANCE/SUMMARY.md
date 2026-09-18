# RELEASE-02 设计符合性审计 — 总结（SUMMARY）

> 审计对象：`ASTROCS_DESIGN.md`（最高权威）**逐条 vs 实际实现行为**。
> 审计员：设计符合性审计 SubAgent（独立验证者）+ 4 个并行子审计（SUB-A §4 / SUB-B §5+§9 / SUB-C §7·§10·§12·§1·§2 / SUB-D §11+§8+测试可信度）。
> 时点：2026-09-18，审计基线 = 已提交 HEAD `1292c7e` + RELEASE-01 运行证据。证据标准见 REGISTER.md §0。
> **并发注意**：审计期间（18:21）前台正在工作树实施 P0-21 修复（`module_adapters.cpp` 新增 `p1_frame_key`/`p1_frame_dir`/唯一性门）；
> 该修复不在本审计基线内，DC-305/SUB-D-33 的判定基于 HEAD 行为，修复完成后须复核闭合。
> **「配置/文档声明」≠「实际执行」；ctest 绿灯不作为符合性证据。**

---

## 1. 计数

| 判定 | 行数 |
|---|---|
| **DIVERGENT** | **106** |
| CONFORMANT | 31 |
| UNVERIFIABLE | 5 |
| 合计条款行 | 142 |
| **其中违反 fail-closed** | **38** |
| **其中阻断级** | **25 行（去重后约 14 个独立根因）** |

> 去重说明：同一根因跨章节重复登记，例如 frame_snr 冒充 depth_m5 = DC-201/DC-307/DC-704；
> 帧级 SNR 未入 HiPS 头 = DC-308/DC-705；权重链断裂 = DC-203/DC-405/DC-1103/SUB-D-22；
> L1 证据缺失 = DC-1102/SUB-D-17；L4 未跑 = DC-1101/SUB-D-20。按独立根因计约 **89 条**。

## 2. 按 § 分布（DIVERGENT / 条款行）

| 章节 | DIVERGENT | CONFORMANT | UNVERIFIABLE | 备注 |
|---|---|---|---|---|
| §3 normalize | 17 | 0 | 0 | 最上游、影响最大；含 P0-21 |
| §4 mosaic | 21 | 5 | 0 | 逐像素排异/权重链/天光面 |
| §5 export + §9 I/O | 13 | 7 | 1 | 输出模式/投影/原子性 |
| §6 CLI 合同 | 5 | 4 | 1 | 版本/退出码/预检 |
| §7 架构 · §10 双平台 · §12 版本 · §1 · §2 | 18 | 3 | 0 | 模块边界/入口名/版本 |
| §8 资源 | 2 | 1 | 0 | 线程预算/单位文案 |
| §11 验证体系（主审） | 5 | 0 | 1 | 四层验收 |
| §11 + §8 + 测试可信度（SUB-D） | 22 | 11 | 2 | 假绿/空证据/Oracle 缺口 |
| **合计** | **106** | **31** | **5** | 另有跨章重复 |

## 3. 阻断级清单（25 行 / ~14 个独立根因）

| # | 根因 | 编号 | 一句话 |
|---|---|---|---|
| B1 | **normalize N 进 1 出（P0-21）** | DC-305、SUB-D-33 | 81 帧输入只 drizzle 15 帧、只写 15 个 HiPS（每配置 1），静默丢弃 66 帧且 rc=0；测试反而断言 `call_count==1` 把错误固化为期望 |
| B2 | **frame_snr 冒充 depth_m5 + 帧级 SNR 未入 HiPS 头** | DC-201、DC-307、DC-308、DC-704、DC-705 | `p1_snr.json` 的 `frame_snr` 是 5σ 深度对象；properties 无任何 SNR 键；违反 §2 对象禁止冒充 + §3.4 唯一承载 |
| B3 | **Phase2 逆方差权重链断裂** | DC-203、DC-405、DC-1103、SUB-D-22 | 稀疏 SNR 层零实现；`w=SNR²/F_ref²` 无实现；L4 用等权、L3 用 `legacy_allow_weight_fallback=true` 静默走通 |
| B4 | **稀疏 SNR 层完全未实现** | DC-403 | `sparse_snr_layer` 在 `lib/` 零命中（§4.3 标准行为） |
| B5 | **逐像素排异自适应未实现** | DC-414、DC-418、DC-419、DC-424 | AUTO 按整组帧数一次解析；`algorithm_rejection_method` 无消费者且被 CLI 判 unknown key；L4 74% 像素 n=2 全接受 |
| B6 | **天光稀疏面未接入生产链** | DC-412 | `sky_plane` 只接独立 `astrocs-stage2`；生产 UPM component_count=1；FIX-A 报告「生产默认开启」不成立 |
| B7 | **export 输出模式死合同** | DC-503、DC-504 | `output_mode` 零消费者（rc=3 unknown key）；恒按 surface_brightness，从不拒绝/unavailable |
| B8 | **原子性只覆盖 FITS/manifest** | DC-508、DC-509 | p3_props/p3_wcs/p3_resampled/p3_verify 原地直写，无 tmp/fsync/rename，失败残留 |
| B9 | **唯一事实源 schema/template 不可运行** | DC-514 | `export --json config/templates/export.phase_config.json` → rc=2；schema 与 CLI flat 格式互斥 |
| B10 | **版本纪律失守且被 CI 反向固化** | DC-718 | `--version` 输出 0.11.0-alpha.2；产物/代码/run 全含版本；CI `VERSION-CONSISTENCY` 反而强制版本存在 |
| B11 | **L1 合成科学性验收证据完全缺失** | DC-1102、SUB-D-17 | `artifacts/acceptance/` 无 `l1_science/`，无任何 L1 文件 |
| B12 | **L4 真实视觉验收全量未运行** | DC-1101、SUB-D-20 | L4 STATUS 自述 NOT RUN；仅 L3 demo |
| B13 | **预检 fail-open（文件找不到判 correct）** | DC-310、DC-408 | 不存在的输入/校准路径判 `[correct]`，`-y` 后进入运行；§3.5 明列为 error |
| B14 | **测试假绿** | SUB-D-30、SUB-D-33 | 任何可查 ctest 记录都非全绿（3 failed / 12 skipped）；多帧基数合同无测试 |

## 4. 最严重三条

1. **P0-21 同族：normalize 基数合同 + 测试固化**（DC-305 / SUB-D-33）。
   设计要求「一组 light → 一组 HiPS，每帧一个」（§3.4:153-157）；实测 15 个 L4 配置声明 **81 帧**，只 drizzle **15 次**、写 **15 个 HiPS**，**静默丢弃 66 帧且 rc=0**；根因 `module_adapters.cpp:3343`（只取 `input_lights[0]`）、`:3385`（建 1 个 frame）、`:3541`（drizzle 一次）。
   更严重的是测试把该行为固化为正确：`tests/unit/p1001_real_nodes_test.cpp:324,589` 用 2 个 light 却断言每节点 `call_count==1`，全仓无「N 帧进 → N 产物出」计数断言。这正是缺陷能长期漏网的根因。

2. **Phase2 权重链与设计脱节**（DC-403/405/412 + DC-203/1103/SUB-D-22）。
   §4.3「逆方差叠加是标准行为」端到端从未验证：稀疏 SNR 层零实现、`w=SNR²/F_ref²` 无实现、生产改读 Phase1 `ivar` 而该产品缺失；L4 配置显式 `weight_mode=1`（等权），L3 自述 `weight_chain="BLOCKED ... completed via explicit legacy_allow_weight_fallback=true"`。天光稀疏面（§4.4）只接独立工具、未进生产节点链。

3. **逐像素排异三点全违**（DC-414/418/419/424）。
   §4.5 核心要求：AUTO 按该输出像素的 n 自适应、显式指定不被覆盖、不得静默改算法。实测 AUTO 在 `module_adapters.cpp:4412-4423` 用 `nominal_contributors=frames.size()` **整组一次解析**；`algorithm_rejection_method` 在 `lib/` 零消费者且顶层写被 CLI 判 `unknown key`（rc=3），`reject:{method:"bogus"}` 预检判 `[correct]` 被静默忽略；L4 `p2_rejection.json` 74.14% 像素 n=2 全接受（`accepted_pixels==n_pixels`），卫星线残留。

## 5. 「绿灯不可信」清单（按可信度排序）

| 绿灯 | 为什么不可信 | 证据 |
|---|---|---|
| **ctest 460/460 全绿** | 任何可查运行记录都非全绿：2026-09-18 16:47 轮 460 tests / 443 passed / 12 skipped / **3 failed**（`p1hips_selfcheck`、`p1noise_abi_layout`、`v6_aio_impl_mutations`）；2026-09-16 轮 414 tests / 1 failed。且 12 条 SKIP 含负例 `Phase2Coverage.FilterMismatchRejected`；`test_golden_parity` 环境未设即静默 PASS；mutation 类 ctest 仅 1 条（`v6_p3_rsmp` mutation driver 默认 OFF） | `run/RELEASE-02/DESIGN-CONFORMANCE/evidence/LastTestsFailed.log`、`LastTestsDisabled.log`、`ctest-status.log`；SUB-D-30/31/34 |
| **多帧/normalize 基数绿灯** | 测试把「只 drizzle 一帧」固化为期望；无 N→N 计数断言与多帧正例 | `tests/unit/p1001_real_nodes_test.cpp:324,589,2843`；SUB-D-33 |
| **CI 版本/一致性绿灯** | `ci/check_version.py` + `VERSION-CONSISTENCY`/`VERSION-NAMESPACES` **反向强制**版本串存在与一致；无「不得含版本信息」的负例检查器 ⇒ CI 绿 = 必然违反 §12 | `ci/checks.json:6446-6480`；`ci/check_version.py`；DC-718 |
| **L3/L4 端到端绿灯** | L4 输入只有代表性的 24.5%（12/49 R 帧）且受 P0-21 污染；L3 权重链靠 `legacy_allow_weight_fallback=true` 静默走通；L4 全量根本没跑 | `artifacts/acceptance/L3/EVIDENCE.json`、`L4/STATUS.json`；GAP_AUDIT §8.3 |
| **排异绿灯** | L4 74.14% 像素 n=2 UNDERDETERMINED 全接受、`accepted_pixels==n_pixels`，仍 exit 0；无任何测试覆盖生产 AUTO 路由/per-pixel n/配置键消费 | `l4__p2_m42/p2_rejection.json`；A-23/24 |
| **预检绿灯** | 不存在的输入/校准路径判 `[correct]`（normalize/mosaic/export 三命令同）；无资源/磁盘预估；无 warn 级；`-y` 不显示页面；`-force` 仍请求确认且不越结构错 | 实测（evidence-cli.txt）；DC-310~314/408~411 |
| **资源门绿灯** | 默认 record_only；唯一切到 enforced（exit 10）的开关 `--strict-resource-gate`/`--on-resource-gate` 被解析器判 unknown flag ⇒ exit 10 在 CLI 面不可达，L2「零 enforce 违约」不可证伪 | `commands.cpp:565-567` vs `parser.cpp:36-42`；DC-801 |
| **验收证据绿灯** | 四层证据仅 3 个 JSON、无 L1、无 `ACCEPTANCE_REPORT.md`；1/N 一致性目录 6 个 0 字节文件；L4 全量未跑 | `artifacts/acceptance/`；`find run/RELEASE-01/e2e/evidence -size 0`；SUB-D-32 |
| **Python 测试绿灯** | `tests/test_index.csv` 多套件含 errors/failures/skip（api 2 error+19 skip、backend 29 error+50 skip、cli 1 fail+9 error+72 skip、io 3 error、version 2 fail、abi 9 error）仍被当通过证据 | `tests/test_index.csv:2,3,5,6,7,15,23`；SUB-D-35 |
| **FIX-A 报告「生产默认开启」** | `sky_plane`/`star_mask`/`p2_sample_sky` 在 `module_adapters.cpp` 零引用，只对独立 `astrocs-stage2` 工具成立 | A-12；DC-412 |
| **配置/schema 绿灯** | `config/templates/*.phase_config.json` 与 CLI flat 格式互斥、`algorithm_*` 零消费者；`contracts/schemas/phase_config_*.schema.json` 只在 `tests/config` 孤立校验，从不校验真实 CLI 配置 | dead-keys.md；DC-514 |
| **CTest 锁死违规** | `rt008` 断言注册表 size==22（含两个整阶段 Session 适配器）；`p1hips_tests_units.cpp:288-289` 断言 `hips_frame=icrs` 为标准（违反 IVOA） | C-12；GAP_AUDIT P0-19 |

### 可信面（经复核，可作为后续基线）
有独立 Oracle 且进 ctest 的核心模块（calibration/cosmetic/star/psf/photometry/noise_snr/drizzle/resample/projection/upm/rejection）、`v6_p1_psfw` 的 `σ_F=1/√(ΣW)` MC 门、`v6_p3_rsmp_oracle` 的 `C_out=RC_inRᵀ` 独立对拍、G-RES-01 阈值唯一源 `contracts/resource_gate_v1.json`（C++ 生成头 + Python 共读）、天光面内存流式性用例、三命令 CLI 层独立性。

## 6. 需要上呈负责人裁决的口径冲突（非 agent 可自决）

1. **§4.5.2（最高设计要求 per-pixel n 路由）vs `docs/science/REJECTION.md:18,32,78`（冻结，明写「n 一次解析，禁止 per-pixel effective 路由」）** —— 二者直接冲突；当前实现与测试站在 SCI 文档一侧。按 AGENTS §8/§9 须走变更 claim 裁决（SUB-A 未改任何文档）。
2. **§3.5 `-force`「跳过全部检查（连 error）」vs 实现「结构错不可 force」** —— 需裁决收紧设计还是放开实现（DC-313/411）。
3. **§12 版本纪律 vs `ci/check_version.py` 的版本一致性门** —— 需裁决版本纪律的生效时点与 CI 检查方向（DC-718；控制包已登记 P0-15，负责人裁决「全部验收通过后再改」）。
4. **§9.1 aio 唯一 I/O 边界 vs `algorithms/fits_output` 直调 CFITSIO** —— 需裁决是否修订 §9.1（DC-515）。

## 7. 交付物与证据索引

- 主台账：`reports/RELEASE-02/DESIGN-CONFORMANCE/REGISTER.md`（142 行）
- 死键清单：`reports/RELEASE-02/DESIGN-CONFORMANCE/dead-keys.md`
- 本总结：`reports/RELEASE-02/DESIGN-CONFORMANCE/SUMMARY.md`
- 子审计全文：`run/RELEASE-02/DESIGN-CONFORMANCE/SUB-{A,B,C,D}-*.md`
- 主审员证据：`run/RELEASE-02/DESIGN-CONFORMANCE/evidence-p021.txt`（N→1 全表）、`evidence-cli.txt`（CLI 只读实测）
- SUB-D 运行时证据：`run/RELEASE-02/DESIGN-CONFORMANCE/evidence/{LastTestsFailed.log,LastTestsDisabled.log,ctest-status.log}`
- 关键运行时来源：`run/RELEASE-01/e2e/{l4/configs,l4/logs,evidence}/`、`run/v6/performance/`、`artifacts/acceptance/`、`工程控制/RELEASE-02/{GAP_AUDIT,ACCEPTANCE}.md`

> 审计员未改 `docs/`、`lib/`、`tests/`、`contracts/`、`config/`、`ci/`、`ASTROCS_DESIGN.md`；零 git 写；未跑 ninja/cmake/ctest。
