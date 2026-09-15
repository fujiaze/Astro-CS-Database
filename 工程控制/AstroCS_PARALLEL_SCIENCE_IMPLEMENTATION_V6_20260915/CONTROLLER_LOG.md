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
