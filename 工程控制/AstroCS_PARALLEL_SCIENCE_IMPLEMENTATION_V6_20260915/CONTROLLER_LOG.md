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
