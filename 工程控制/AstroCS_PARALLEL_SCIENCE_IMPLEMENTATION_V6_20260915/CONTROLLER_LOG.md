# V6 控制器日志（AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915）

本文件是控制器（唯一主 Agent）的集成/裁决留痕，不是任务正文，也不是任何任务的 write_scope。
任务状态唯一源仍为 `TASK_LEDGER.csv`；上位科学定义仍为 docs/ 下的目标态设计。

## C-001 基线集成（2026-09-15）
- `ce5b3a00`：负责人已批准设计（PROJECT_SPEC / 三份 Phase 详细设计 / UNIFIED_SCIENCE_MODEL /
  PSF_SIGNAL_WEIGHT / 参考文献档案）+ 本包基线（00_READ_FIRST/RULINGS/EXECUTION_GRAPH/
  TASK_MANIFEST/TASK_LEDGER + 35 任务卡）+ ACTIVITY_STATE 唯一 ACTIVE=V6。
- 边界：未触碰在途工作树回退（P33/P35/P36 相关 16 回退 + 10 删除）、审核线文档与 artifacts。

## C-002 Wave 0 集成（BASE-OWN-001）
- `4b508f28`：BASE-OWN-001 PASS（控制器复跑 build rc=0、verify 19/19 rc=0、mut1/mut2 rc=2、
  tracked 树零变更）。台账解锁 Wave 1 五项。
- 现场归属要点：dirty `-unormal`=173 / `-uall`=868（27 tracked-M + 10 tracked-D + 831 untracked）；
  工作树 16 tracked 回退到 11 个祖先提交 blob，10 tracked 删除等于其引入提交父状态。

## C-003 Wave 1 集成（5/5 PASS，全部单任务单 commit）
| 任务 | commit | 独立复跑结论 |
|---|---|---|
| SCI-P2-001 | `192fab35` | Oracle 32/32 rc=0；17 门 mutation + 3 Oracle 自我 mutation 全红 |
| AUDIT-REVIEW-001 | `a09a81f4` | Oracle 19/19 rc=0；8/8 expected-red |
| SCI-OBS-001 | `9d99fd71` | Oracle 15/15 rc=0；13 注入 mutation 全 rc=2；scope_check rc=0 |
| SCI-P3-001 | `eac43135` | Oracle 11/11 rc=0；22 mutation 全检出；doc verify rc=0（16 claims）；doc mutation 7/7 rc=1 |
| SCI-PSFW-001 | `24610e01` | Oracle selftest 76 check + 12/12 CAUGHT；w_info 13/13；doc audit 10/10 CAUGHT；scope IN-SCOPE |

## C-004 控制器裁决（本包运行期生效）
1. **Phase2 生产模式面**：本包只实现三种显式模式 `point_information` / `surface_gls` /
   `psfsw_robust`。`psf_snr_power` **本包不解冻**（保持 NOT_IMPLEMENTED/unavailable），仅在
   文档中登记为未来可选模式；其冻结条件（常数版本化、只声明优于指定基线、不得等同 Fisher 最优）
   由 ALG-P2-PSFSW-001/W3 与 CONTRACT-FREEZE-001/W4 写清，但不得进入 V6 生产路由。
2. **帧级 median(SNR_F) 系数**：现有 HEAD 中的 `lib/phase1/noise/snr_frame_coefficient.*` 只能作为
   诊断/深度表达登记，禁止进入任何权重面（点源信息权重仍以 Q=aPᵀC⁻¹d、W=a²PᵀC⁻¹P 为权威）。
   由 CONTRACT-FREEZE-001/W4 显式登记，P1-INTEGRATE-001/W7 与 RUNTIME-CI-001/W9 不得把它接入权重。
3. **权重 schema 词表统一**：SCI-PSFW-001 与 SCI-P2-001 的词表差异（weight_kind/units/scope vs
   weight.kind/units/group_normalized）由 SCHEMA-INTEGRATE-001/W6 归一为单一 schema；W3 算法规格
   必须引用 W6 词表，不得各自发明。
4. **构建面 owner 缺口（AR-033 / H-1）**：根 `CMakeLists.txt`、`tests/unit/CMakeLists.txt`、
   `tests/unit/p1snr/CMakeLists.txt` 无 V6 任务 owner。控制器裁定：W5 各 IMPL 任务只允许在自身
   write_scope 内注册测试；任何必须改根/公共 CMakeLists 的注册，由控制器在 W5 后以**独立集成提交**
   统一处理（归属 RUNTIME-CI-001/W9），不得由子代理顺手改。
5. **上位规范写保护**：Wave 1–4 只许写 `docs/**/v6/**`、`contracts/proposals/v6/**`、
   `reports/v6/**`；`docs/science/*.md`、`docs/owner/**`、`docs/design/**`、
   `docs/references/**` 在本包过程中不得改写（其被取代段由 W4 CONTRACT-FREEZE-001 登记清单，
   正式 amendment 由负责人签字）。
6. **F1 基线分歧（工作树≠HEAD）**：Wave 1–4 为 docs 写域，按 HEAD 描述；**在 W5 派发前必须裁定**。
   控制器已向负责人请示一次未获答复；若届时仍未裁决，控制器将按「工作树为 V6 基线」以独立治理提交
   固化回退态（P33/P35/P36 仍在 git 历史中，可恢复），以保证任务提交不被预存回退污染。

## C-005 转入 Wave 2
- SCI-ADJ-001 置 READY，输入 = 五个 W1 交付 + 本日志 C-004 裁决 + 各任务登记的 F-OBS-01..05 /
  F3-01..06 / AR-032..036 / S1–S4。

## C-006 Wave 3 / Wave 4 集成与 F1 局部裁定（Wave 5 放行）
- Wave 3 七项全 PASS：ALG-P2-PSFSW-001 `1c4e5f92`、ALG-P2-POINT-001 `28e0ac6c`、
  DATA-DESIGN-001 `2eab1fc1`、ALG-P2-SURF-001 `77ce7299`、ALG-P1-001 `29747831`、
  QA-MATRIX-001 `67f0456f`、ALG-P3-001 `ebefe00d`。
- Wave 4 CONTRACT-FREEZE-001 PASS `7a104485`：96 条款（FROZEN 39 / PENDING_OWNER_SIGNOFF 49 /
  OPEN 8），Oracle 2056 checks PASS，mutation 23/23 全红，SO-01..07 保持待签。
- **F1 基线分歧的裁定（局部化，取代 C-004.6 的"整体固化"）**：
  1. 事实：工作树 37 个 tracked 差异全部先于本包存在，本包 17 次集成只 `git add` 任务写域，故其
     数量始终为 37；工作树对 `spherical_overlap.{cpp,h}`、`drizzle_engine.cpp` 等文件是
     **P35/P36 提交的整体回退**（worktree blob ≠ HEAD blob，等于 `b756c7e2` 之前的状态），
     且删除 `p35_*`/`p36_*`/`p1snr_frame_*` 测试与 `cli/frame_admission.h`。
  2. 与 Wave 5 的交叉面经 `TASK_MANIFEST.write_scope` 逐条比对，**仅 IMPL-P1-DRZ-001 命中 3 个文件**
     （其余 8 个 W5 任务写域干净）。故不采用整体治理提交（会一并固化 `docs/contracts/*`、历史报告、
     audit-line 产物等异质差异），改为**按波次、按写域局部裁定**。
  3. 本次动作：把 `lib/healpix_db/healpix_drizzle/{drizzle_engine.cpp,spherical_overlap.cpp,
     spherical_overlap.h}` 的**当前工作树状态**（= P35 回退态）作为独立治理提交固化，使
     IMPL-P1-DRZ-001 从干净基线开工、其任务提交只含本任务改动。P35 原实现仍在 git 历史
     （`b756c7e2` 等）中，可恢复。
  4. 其余 34 个 tracked 差异**保持不动**：W6 SCHEMA-INTEGRATE-001（2 个 contracts）、
     W7 P1/P3-INTEGRATE-001（4 个源码）、W9 RUNTIME-CI-001（6 个）、W12 DOC-CONVERGE-001（2 个）
     各自派发前按同一规则局部裁定；未进入任何 V6 写域的 22 个差异（历史报告、`reports/v19r2/*`、
     `tools/tasks/CHK-001/*`、根/测试 CMakeLists 等）保持原状、不作为 V6 基线一部分。
  5. 若负责人对 P35/P36 回退态另有指示，可在对应任务提交前以 revert 恢复历史提交内容。

## C-007 Wave 5 集成与后续局部裁定（2026-09-15）
- Wave 5 九项全部 PASS（每项单任务单 commit，控制器均独立构建/测试复跑）：
  IMPL-P1-PSFW-001 `078bde5f`、IMPL-P1-CAL-001 `6c17e7d6`、IMPL-P2-REJ-001 `dfc1f8fc`、
  IMPL-P3-PROJ-001 `e476b90e`、IMPL-P1-DRZ-001 `684a693e`、IMPL-P2-SAMP-001 `51d5b645`、
  IMPL-P2-UPM-001 `f0502b25`、IMPL-AIO-001 `e244b284`、IMPL-P3-RSMP-001 `b5bc6af4`。
- **F1 局部裁定（Wave 6）**：SCHEMA-INTEGRATE-001 写域命中 `docs/contracts/DATA_SEMANTICS.md`
  与 `docs/contracts/PUBLIC_API.md`。二者工作树状态同样是**回退**（删除 P33-COEF §12.2 SNR 系数
  落位段落与 P27 §「生产路径不消费的 IpvParams 字段」整节；合计 -257 行），与已固化的
  P33/P35/P36 回退同源。按 C-006 规则先以独立治理提交固化这 2 个文件，使 W6 从干净基线开工。
- 其余 32 个预存 tracked 差异保持不动；W7 P1/P3-INTEGRATE-001(4)、W9 RUNTIME-CI-001(6)、
  W12 DOC-CONVERGE-001(2) 派发前同样局部裁定。
- **误报更正（留痕）**：`IMPL-P2-REJ-001` 报告"upm.cpp 被并发任务改坏（语法错误）"经复核实为
  **缺 `-I` 路径**（`aio_upm.h` 在 lib/astro_image_io/include，`crypto/sha256.h` 在
  lib/common/crypto），`upm.cpp` 完好；控制器以项目真实 compile flags 独立链接五个 UPM 测试模式
  全部 rc=0。另：控制器一度把 `aio_upm.{cpp,h}` 误判为 AIO-001 新建，实为**先前已提交的 tracked
  干净文件**（0751d34f/a8169bc0），该"跨任务依赖缺口"登记已撤回。
- **高优先级科学发现（待负责人裁决，来自 IMPL-P3-PROJ-001 的 astropy/WCSLIB 对拍）**：
  - F-CAR：legacy `lib/phase3_proj/p3_projection.cpp` CAR 用 Y=−θ，赤纬相对 FITS WCS Paper II
    反号（最大偏差 345600″=96°，legacy dec == −standard dec）。
  - F-AIT：legacy AIT 缺 Paper II γ 的 √2 因子（最大偏差 94885″、world→pix 残差 13.05 px）。
  V6 层已按 Paper II 实现并过 Oracle；legacy 文件与其逆向测试 `tests/unit/p3_projection_test.cpp`
  不属任何 V6 任务写域 → 登记为取代项，需负责人授权后才能独立修正。
- **AR-033 构建面欠账（控制器 W9 统一处理）**：Wave 5 交付的测试目标均只提供自注册
  `tests/unit/v6_*/` 与局部库目标，根 `CMakeLists.txt`/`tests/unit/CMakeLists.txt` 未改；
  待注册：`astrocs_v6_aio`、`astrocs_p3_rsmp`、9 个 `add_subdirectory(tests/unit/v6_*)`、
  以及新生产源进入生产库的接线（含 `lib/astro_image_io/v6/src/*`）。
- 台账：Wave 6（SCHEMA-INTEGRATE-001）置 READY。

## C-008 Wave 6–10 集成、AR-033 清账与负责人裁决项（2026-09-15）
### 集成链（每任务单 commit + 立即 push main，均已独立复跑）
- Wave 6 SCHEMA-INTEGRATE-001 `2ac6b758`（10 生产 schema + 词典/词表/迁移映射；词表归一单一权威）
- Wave 7 P3-INTEGRATE-001 `a689eff2`、P1-INTEGRATE-001 `960d6051`
- Wave 8 P2-INTEGRATE-001 `4c0296e0`
- Wave 9 RUNTIME-CI-001 `0d8e98f1`
- Wave 10 REAL-SCIENCE-001 `815f161f`、PERF-SCALE-001 `f8470f4c`
- F1 局部裁定：W6 `ac04289d`（docs/contracts 两处 P33/P27 回退态）、W7 `95703e63`（P33
  snr_frame_coefficient 删除 + P35 p3_resample 回退）、W9 `393db3fb`（P36 六文件回退族）、
  构建面 `c7432fa0`（3 个 CMakeLists + 8 个已删测试，与回退态自洽）
- **AR-033 清账 `3e7fbc44`**（控制器 C-004.4 集成动作）：根构建面注册 4 个 V6 生产库
  （astrocs_v6_aio、astrocs_p3_rsmp、astrocs_v6_phase1_product、astrocs_v6_phase2_integrate）
  与 13 个 add_subdirectory（9 个 tests/unit/v6_*、tests/integration/v6_{p1,p2,p3}、
  tests/system/v6_runtime），置于 lib/phase2 之后满足依赖顺序。验证：根 configure rc=0；
  定向构建 rc=0（0 error）；根图注册 **79 个 V6 ctest**；实跑 98% passed（79/81，2 项非 V6
  既有测试因未定向构建而 Not Run）。**AR-033 关闭。**
### 负责人裁决项（阻塞 READY_FOR_OWNER_REVIEW）
1. **SO-05 资源门（最高优先）**：PERF-SCALE-001 实测 16 worker 活跃窗口 CPU 均值 **65.09%**
   （冻结门 85%）、p50 87.63%（门 90%）；已按 C-007 以 record_only_pending_owner_signoff 记录，
   hard_fail=false，未自行裁决。同批记录 memory_growth_unbounded@16w（34.01 MB/s ≥ 32，斜率随
   墙钟缩短升高，疑启动工作集爬坡）与全预算 alloc_reclaim_missing。**§10.5/§17.6 门是否启用自动
   判决、以及 16w 低利用率是否判失败，须负责人签字。**
2. **schema 交叉张力**（P3-INTEGRATE-001 报告）：provenance.v1 的 allOf（bunit=ADU ⇒
   pixel_semantics=surface_brightness ∧ pixel_area_power=-2）与 signal.v1（integrated_flux ⇒
   pixel_area_power=0）使**纯积分通量主面不可表达**；AIO bunit_dimension_decidable 亦只接受 SB-ADU。
   需裁定 FZ-UNIT-FLUX 与 FZ-BUNIT-SEMANTICS 的适用关系（是否修订 schema/gate）。
3. **quantity.units 未收敛**（P1-INTEGRATE-001 报告）：生产 schema 中为自由 string，篡改
   W_info.units 仍可过 schema（C++ 重开门已补齐）；建议收紧为 const/enum。相关系数：
   covariance.v1 对角 representation 强制 operator_descriptor，而 kind 枚举缺「校准 Jacobian」。
4. **AIO FITS 实数格式**：FitsCard::make_real 用 %.12g 产生小写 e，astropy 判 not FITS standard
   （p3_proj_v6::fits_keywords 同）。P1/P3 集成任务各自在产物层规范为大写 E 绕过，**AIO 本身待修**；
   因 lib/astro_image_io/v6 已属 IMPL-AIO-001 已提交写域，未由控制器擅自改，列为待办。
5. **F-CAR / F-AIT legacy 投影错误**（C-007 登记，仍未裁决）：legacy p3_projection.cpp 的 CAR
   赤纬反号（偏差 96°）、AIT 缺 Paper II √2（残差 13.05 px）；legacy 文件与其逆向测试
   tests/unit/p3_projection_test.cpp 不属任何 V6 写域，需负责人授权后才能独立修正。
6. **Phase2 CLI 真实数据不可达**：PERF-SCALE-001 报 Phase2 三生产模式 CLI 端到端 fail-closed
   （node upm_fit: control ivar 缺失 / obs=0 overlap_controls=0）；REAL-SCIENCE-001 亦未跑真实
   FITS 产品链（write_phase1_product → run_point_information/run_psfsw_robust）。属数据流覆盖缺口。
7. Phase1 provenance.k_corr 是否豁免（Phase1 无 UPM）；施工法见 FZ-PROV-KCORR 域内继承常数 1.4。
### 诚实性登记（不得冒认）
- REAL-SCIENCE-001 的 equal/exposure/ivar 三口径为 **DOCUMENTED_BASELINE（驱动内实现）**，
  仅 W_info 与 PSFSW 走冻结库函数；该验收证明的是 V6 两条生产口径相对文档化基线的行为，
  **不是**全链生产路径端到端验收。
- 49 条 PENDING_OWNER_SIGNOFF 与 8 条条款级 OPEN 保持 fail-closed；psf_snr_power 保持 DEFERRED；
  未解冻任何冻结公式/容差/门。
- Windows/Fatduck 复验：控制器实测 Fatduck（100.104.10.71:22）**连接超时不可达**；按 §15.3
  不阻塞 Linux 工作，WIN-VERIFY-001 只能交付 Windows 验证包并置 AWAITING_WINDOWS_VALIDATION，
  不得宣称 Windows 通过、不得据此发布。
- 预存差异：本包开工前既有 868 行脏清单已逐步裁定；**剩 12 个 tracked 差异**（artifacts/prerelease_v5
  3、reports/v19r2 3、evidence/v6_1_rework 2、问题扫描 4）与本包无关，保持未提交。
- 包状态：**NOT_READY**（剩余 WIN-VERIFY-001 / DOC-CONVERGE-001 / FINAL-AUDIT-001 / OWNER-PACK-001）。

## C-009 Wave 11 结论与 CI 红线（控制器独立复核）（2026-09-15）
### WIN-VERIFY-001 = AWAITING_WINDOWS_VALIDATION（不是 PASS）
- Fatduck（100.104.10.71:22）**不可达**：ssh rc=255 Connection timed out、/dev/tcp rc=124、
  ping 100% loss、tailscale 显示 windows offline（last seen 3h ago）。按 §15.3 不阻塞，但
  **不得**宣称 Windows 通过、**不得**据此发布。
- GitHub Actions 侧（公开 REST API）：基线 SHA 的 Windows CI run 35012779853 failure，失败步
  = Run MSVC tests and package candidate（exit 1），Upload candidate skipped → **无候选**；3 次
  Fatduck Validation 全部 failure（no-candidate / CI-001 fail-closed）。
- 32/32 Windows 验证用例（同 SHA 候选安装 / ABI / 长路径 / >2GiB / 真实产品）全部 UNAVAILABLE，
  sha256 清单如实留空。
- 欠账：FD-F-003（P0 OPEN：Windows C++ 单测门长期「0 用例 PASS」）；§17.12 门 7（同 SHA
  Linux/Windows CI 通过）与门 8（Fatduck 复验）均未满足。
### AR-034 部分清账（控制器建面动作 `b7c4f35e`）
- v6_p3_rsmp 的 ctest 用例名由 p3_rsmp_*_test 改为 **v6_p3_rsmp_core/oracle/gate** 并显式 add_test，
  同时修好 ci/checks.json 里 V6-CTEST-UNIT 的 --expect-glob v6_p3_rsmp_* 覆盖缺口。
  CTEST-REGISTRATION 的 unregistered 由 3 项降为 **2 项**（余 ipv_dead_params_lock /
  ipv_dead_params_lock_selfcheck，**非 V6**）。
### 控制器独立复核的 CI 事实（§14.5 判定依据）
- **Linux CI 整条 V6 线持续红**：公共 API 逐 commit 核对，自 `ebefe00d`（Wave 3）到当前全部
  linux=failure；**AR-033 注册并非肇因**（其之前已红），但红线未消。
- 本地以 repo 自带入口复现 linux-main 关键门：
  - `THREAD-BUDGET` **FAIL**：`lib/core/src/module_adapters.cpp:245,252` 的 omp_set_num_threads
    未登记（**属 P36 回退态**；该文件工作树状态由 C-007/C-008 的 F1 裁定固化，修正需 owner 授权）。
  - `CTEST-REGISTRATION` **FAIL**：余 2 项非 V6 的 IPV 残项（见上）。
  - `VERSION-CONSISTENCY` 本地 **PASS**（与 SCHEMA-INTEGRATE-001 曾报的 4 项不同调用面：那 4 项在
    **未修改**的 docs/references 归档文件内；此处不作为 CI 红因，留待澄清）。
  - `AGENTS-GOV`、`KNOWN-FAILURES-BASELINE-VERIFY` 本地 PASS。
- **结论：§14.5 完成顺序中的「GitHub CI 通过」未满足**，故本控制包不得宣布完成；最终状态
  **NOT_READY**。W12/W13/W14 可继续（宪章不禁停工），但 FINAL-AUDIT-001 与 OWNER-PACK-001
  必须如实携带该红线，不得以 waiver 掩盖。
### 负责人裁决项（在 C-008 基础上增补）
8. **CI 红线归属**：THREAD-BUDGET（P36 回退态 module_adapters.cpp 的线程预算旁路）是否授权修正；
   2 项 IPV CTEST-REGISTRATION 残项归属；Windows MSVC「Run MSVC tests and package candidate」
   失败根因（需有 token 的环境取回 win-stage-*.log / win-package-summary.json）。

## C-010 FINAL-AUDIT-001 受理与包级覆盖缺口（2026-09-15）
### 独立审计结论：包总判定 **NOT_READY**（BLOCKER 5 / MAJOR 5 / MINOR 5 / INFO 8）
审计由独立子代理对抗性执行：自建根图、自跑 `ctest -R v6_`（79/79 全过、无 skip）、自写 numpy Oracle
17/17 复算全部关键式（**未发现符号/指数/归一错误**）、抽查 33 个任务提交 ⊆ write_scope（0 越界）、
独立复现 CI 红线与 Fatduck 不可达。控制器已独立复核并确认其最高严重度条目。
### B-04（新 BLOCKER，包级覆盖缺口 → 需后续任务）
**V6 三 Phase 生产入口在 CLI/生产中零消费者**。控制器独立复核：write_phase1_product /
run_point_information / run_surface_gls / run_psfsw_robust / open_phase1_product / p3_v6_export
的 external_refs **全为 0**；cli/commands.cpp 的 cmd_phase2_run 在 v6cli::mode_gate 校验通过后仍调用
run_with_resource_gate 走 **legacy** 路径。即：用户以 --mode point_information / psfsw_robust 调用时，
**实际执行的是 legacy 集成路径**，V6 科学链只有自测消费者。
归属：TASK_MANIFEST 中**无任何任务拥有「把 V6 链接入 cli/runtime 生产路径」**（W9 RUNTIME-CI-001 只落了
模式门与运行面契约，其任务卡亦写明「不重写科学实现」）。登记为 **AR-037（新）**，建议由后续控制包以独立
任务实现「CLI/运行时接线 V6 三 Phase 链」，在此之前 V6 科学的**生产可达性不成立**，不得对外称 V6 口径已可用。
### 其余 BLOCKER/MAJOR 处置（均已写入审计 gated 清单，交负责人）
- B-01 CI 红、B-02 真实数据终验未执行、B-03 Windows 未复验、B-05 资源门未达且 fail-open：见 C-008/C-009。
- **M-01（重要）**：49 条 PENDING 数值阈值（eps 0.05/0.20、corr_ratio 1.05、N_min 3、k_corr 1.4 等）
  已被生产代码**当作硬门执行**，但负责人签字尚未取得 → 「保持 fail-closed」只部分成立。需负责人确认这些
  阈值是**临时生效**还是**必须先签字**；未决期间不得宣称这些门已获授权。
- **M-02**：DOC-CONVERGE-001 订正了 FROZEN 算法文档 §4.2 的单位**标签**（ADU/px → ADU/px²，与冻结
  单位表一致；非公式/容差改动）。已按审计意见登记为**负责人确认项**：若认为 FROZEN 正文改动须单独签字，
  可对 3 个文件（ALG_P1_001_*.md、alg_p1_001_spec.json、verify_alg_p1_001.py）做**单 commit 回退**
  （回退后 doc-convergence 的 C4 项会转红，故建议连同 C4 规则一并改为「pending_signoff」态）。
- **M-03**：C-006/C-007/C-008 的 F1 治理提交把**包开工前既有的 P33/P35/P36 回退态**固化为 V6 基线
  （含删除 9 项 CI 检查登记、8 个测试目标、cli/frame_admission.h）。该回退**内容本身**是负责人开工前的
  决定、且控制包设计（RULINGS F1）要求按此基线推进；但审计正确指出**删除清单未获负责人逐项授权**。
  登记为负责人确认项：确认保持该基线，或指示恢复 P33/P35/P36。
- **更正 C-008 表述（m-02）**：控制器此前把根全量 ctest 的 2 项失败写成「Not Run」——那是**定向构建子集**
  的结果；审计独立跑全量为 **405/407**，2 项为**真实失败**（cpu007_profile_store 稳定 FAIL（CWD 依赖）、
  p1hips_performance 抖动）。此处以本次更正为准。
### 负责人确认/裁决项汇总（最终，共 12 项）
1 SO-05 资源门（16w 65.09% < 85%、p50 87.63% < 90%、memory_growth_unbounded@16w 未定性、全预算
alloc_reclaim_missing；是否启用自动判决/是否判失败）；2 **AR-037 V6 链未接入 CLI/生产（生产可达性不成立）**；
3 M-01 PENDING 阈值已作硬门执行的授权问题；4 M-02 FROZEN 单位标签订正确认（含回退路径）；
5 M-03 F1 回退基线逐项授权；6 模式 schema 交叉张力（bunit/allOf vs integrated_flux）；
7 quantity.units 未收敛（含 covariance operator_descriptor 枚举缺项）；8 AIO FITS 实数小写 e；
9 F-CAR/F-AIT legacy 投影错误修正授权；10 Phase2 CLI 真实数据不可达（obs=0）与 QA W10 门悬空（M-04）；
11 CI 红线归属（THREAD-BUDGET/2 项 IPV/Windows MSVC 根因）；12 FD-F-003、AR-032/SO-06（C-004.5 写保护
非 v6 SCI 正文）、memory.md 是否纳入本包。
### 包状态
**NOT_READY / NOT_RELEASED**。§14.5 六步：任务提交 NOT_MET / GitHub CI NOT_MET / Linux 真实数据终验
NOT_MET / Agent 图像初审 NOT_MET / Windows 复验 AWAITING / 汇总打包 NOT_MET。

## C-011 控制包收口（2026-09-15）
- **台账终态**：35 项 = **PASS 34 / WAITING_WINDOWS 1**（WIN-VERIFY-001 为 AWAITING_WINDOWS_VALIDATION，
  非 PASS）；无 FAIL、无 BLOCKED。
- **集成链**：全部任务一任务一 commit 并立即 push main；控制器集成动作单列（F1 局部裁定 `ac04289d`/
  `95703e63`/`393db3fb`/`c7432fa0`、AR-033 `3e7fbc44`、AR-034 `b7c4f35e`、治理日志 `8f5ef3e9`/
  `8e1e280e`/`6b459952`/本提交）。全程**未使用** git add -A、未建分支/worktree、未 stash/reset/clean、
  未 force push、未 amend。
- **包状态（最终）：NOT_READY / NOT_RELEASED** —— 即"未就绪，需负责人裁决"，**不是发布**。
  §17.12 门 6/7/8/9 未满足、门 10 未证实；AWAITING_EXTERNAL_RELEASE_REVIEW 未达成；VERSION 保持
  0.11.0-alpha.2 未提升；无任何正面 RELEASED 宣称。
- **负责人待决**：OWNER-PACK-001 的 OD-01..OD-12（见 reports/v6/owner-package/
  OWNER-PACK-001_REVIEW_PACKAGE.md 与 artifacts/v6/owner-package/owner_decisions.json）。
- **最需优先关注**：① **AR-037**——V6 三 Phase 生产入口在 CLI/生产中零消费者，用户 --mode 实际走
  legacy 路径，V6 生产可达性不成立；② **SO-05**——16w CPU 均值 65.09% < 冻结 85%，资源门 fail-open；
  ③ **CI 红线**——Linux 持续红（THREAD-BUDGET 属 P36 回退态 / CTEST-REGISTRATION 余 2 项非 V6 IPV）；
  ④ **M-01**——49 条 PENDING 阈值已被生产代码当硬门执行但未签字。
- **不得**以 waiver 掩盖上述红线；**不得**在负责人裁决前宣称本控制包完成或发布。
