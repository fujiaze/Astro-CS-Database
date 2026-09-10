# AstroCS 项目交接文档（HANDOVER）

> 更新：2026-09-11（ASTROCS-CONSTITUTION-ALIGNMENT-V1 控制包执行前台交接快照；
> 历史轮次交接内容见 CHANGELOG 历史节与 `docs/archive/history/`）。
> 分支：main ｜ 本交接基线 HEAD = `991e3e2e`（HEAD=main=origin/main 已核对一致；
> 接手后以 `git rev-parse HEAD` 为准）。
> 权威：根 `ASTROCS_PROJECT_CONSTITUTION.md`（冻结宪章，GOV-001 FROZEN，
> §18 四项负责人裁决）＞ AGENTS.md ＞ memory.md；`AstroCS_ENGINEERING_CONSTRAINTS.md`
> 已降级 ARCHIVED_NON_NORMATIVE 历史参照。本文件只做交接定位，不复制权威内容。

## 1. 项目总览

AstroCS 是天文 CCD 图像校准与标准化数据库系统：Phase1 把单帧 FITS 经校准/
定标/星点/WCS/测光/噪声/SNR/Drizzle 建成标准化 IVOA HiPS（signal/variance/
ivar/snr/support + manifest）；Phase2 把一组合同兼容 HiPS 经 UPM 联合光度模型
叠加为马赛克 HiPS；Phase3 把任一合同兼容 HiPS 投影为平面 FITS。三 Phase 是
隔离命令，跨 Phase 仅磁盘产品交换（DATA-002）。模块化外壳（C ABI v1 DLL，
唯一导出 astrocs_module_query_v1）按控制包任务逐域迁移。

## 2. 当前执行主线：ASTROCS-CONSTITUTION-ALIGNMENT-V1 控制包

- 位置：`工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/`（rev3，30 任务，
  CHANGELOG.md 记录 rev1→rev3 演进；`04_OWNER_DECISIONS_20260910.md` 为负责人真实数据验收指令）。
- CP 运行 id：`Rmtucy2cqced995`（框架 rev56）。**接手者注意：force register 重建运行图会重置
  passed 状态，须先对账台账（TASK_LEDGER.csv）再决定**。
- 台账快照：PASSED×9 + PASSED_FINDING_OPEN×1（WCS-002）+ IN_FLIGHT（P2-001 已实际
  完成入库需刷新）+ NOT_STARTED×19。

### 2.1 已闭环（前台机器验收 + 原子 commit 在 git log）

| 任务 | commit | 交付 |
|---|---|---|
| BASE-001 | （只读冻结） | BASE_SHA 789c5b6c 三方一致；index 指纹精确复现；dirty 基准 aeb2d470（porcelain v1） |
| GOV-001 | d8c821db | 宪章 FROZEN + §18 四项裁决（投影 TAN+SIN+CAR+AIT；CPU 门禁 85%/60%/单线程即失败；Alpha 允许 Phase3 unavailable；禁第三方模块目录）；工程约束降级 ARCHIVED |
| GOV-002 | 8e03d7da + d1ac4dfc | V8.1 包归档（SHA 零差异）+ ACTIVITY_STATE.md 唯一活动源 + reconcile V71_LEDGER 改指 archive |
| WCS-001 | 86af39d8 | 独立 WCS Oracle 量纲收口 + oracle-1b 双路径交叉断言 |
| WCS-002 | 04556761 | SIP 逆模型 42px→7.64px + 空对确定性翻锚；**科学 finding OPEN（见 2.3）** |
| PSF-001 | ef1ccfcc | dpsf_fit_batch 批量 ABI 边界（DISP-PSF-007 整改，三形态 SIGABRT 收口） |
| DATA-001 | 99713034 | DATA-UNC-001 冻结合同（§30.1-30.5 + 五 schema 行 + 科学公式两节） |
| AIO-001 | 00b73fc1 | 唯一 AIO C ABI v1 + 内容哈希复核（17 状态码双断言 + FIPS oracle） |
| AIO-002 | 36f2edfa | HiPS 原子发布四原语 + tmpfs 残留自愈（RAII）+ double-free 隐患消除 |
| P1-001 | 9e09941a + 8545d70a(F4 补录) | Phase1 三域真实化：真实 Plate Solve（ipv 链+gaia 句柄，Linux stub fail-closed）+ 真实 PSF 拟合（Moffat4 FP64）+ IVOA 1.4 标准单帧 HiPS writer；complete 门 fail-closed |
| P2-001 | 439f9f20 | Phase2 7 节点唯一真实 operation 委托（整改每子节点跑完整 p2_session_run×7 违规）+ §30.1 落地 |
| P2-002 | 9e0fa3a8 | §30.2/30.3 合同唯一事实源 JSON + kernel 直调对拍测试 |
| P0/F-P2-002-01 | 991e3e2e | rejection gather 三重错位（stride/buffer/dtype）整改，mismatch 122414→0，asan 越界读归零 |

### 2.2 环境设施变更（本轮新增）

- **/tmp 已迁移为磁盘 bind**：`mount --bind /workspace/disk_tmp /tmp`（vdb1 SSD 503G），
  fstab 持久化条目已加；原因 tmpfs 24G 过度挂载（物理内存 15Gi）且两度填满致假败。
- GC：`run/local/tmp_gc.sh`（mtime>2h 清理+满载 30min 应急），crontab 每 15 分钟，
  日志 /workspace/disk_tmp/tmp_gc.log。
- `cprun-v4-pack-template` 收录于 `工程控制/cprun-v4-pack-template/`（CP 插件模板）。

### 2.3 OPEN finding 清单（接手者处置）

| ID | 级别 | 内容 | 归属 |
|---|---|---|---|
| WCS-002 F1 | owner 已裁决（选 B） | 冻结 1e-4px 在高畸变 fixture 下不可达 → WCS-003 节点：AP/BP 布局扩展+消费方迭代反演，保留高畸变 fixture+增低中高三档，恢复冻结门后 G-SCI 才开 | WCS-003 |
| F-P2-002-02 | P1 | integrate 不剔除 kernel 拒绝样本，§30.2 n_ineligible 恒等式部分拒绝场景不成立（需逐样本掩码持久化或 integrate 重算） | 待 owner 节点化 |
| F-P2-002-03 | AIO 域 | writer int32 位 32/64 通道 + ASTROCS_* properties 键通道未实现（诊断面承载，合同 JSON 为唯一事实源输入） | AIO 域任务 |
| F-AIO-001 | 预存 | p1_noise_adapter 全量唯一 FAIL（V7 残留组合态；V7 残留不收编纪律见 memory.md item 9/10） | owner 决策 |
| F-AIO-002 | 测试域 | tree_digest 多 HDU 第二头区缺口 → P1-HIPS-DIGEST-001 节点已建待执行 | 节点待执行 |
| F-AIO-003 | 接线 | aio_abi.cpp 编入 astrocs_aio target / ArtifactStore read_verified | MOD-001/DATA-003 |
| F-P1001-005 | 诊断 | utilization_p75_low 归因（owner 裁决 2 选 A）→ BASE-UTIL-001 节点已建 | 节点待执行 |

### 2.4 未完成任务（NOT_STARTED，19 项）与恢复派发顺序建议

P3-001（中断态见 §4）→ ARCH-AUDIT-P1 / BASE-UTIL-001（read-only 可并行）→
P1-HIPS-DIGEST-001 → WCS-003（G-SCI 关键路径）→ P3-002 → RT-001 → REAL-000 →
REAL-001 → VIS-001/WIN-001 → DOC-001 → AUD-001 → PACK-001。

REAL 线 owner 指令（04_OWNER_DECISIONS_20260910.md）：T1 显式空集 PASS；T2/T3 300s
走冻结 dark 缩放策略逐帧记 K；T2 Lum 15 帧=UNAVAILABLE(NO_LUM_FLAT) 登记 finding；
count_enumerated==count_classified 零静默丢弃；Phase2 消费 Phase1 产品必须过
phase_product_exchange schema 与 matrix 拒绝码；GaiaDR3 用于解析、GaiaDR3SP 银心完整
分支；testdata/Gaia 目录只读。遇数据事实与索引不符先修匹配计划与索引（REAL-000 域）。

## 3. 目录规范（强制，详见 AGENTS.md）

仓库根目录固定条目清单见 AGENTS.md「目录规范」节（2026-09-09 基线 + 本轮
schemas→contracts/schemas、launch→packaging/launch、graph→artifacts/graph 收敛）。
新增产物必须落位对应目录。

## 4. 工作树未提交状态（交接时点，接手者第一优先甄别）

- **P3-001 中断遗留（归属待甄别，不得覆盖）**：`lib/phase3_proj/{README.md,memory.md,module.yaml}`、
  `docs/algorithms/PHASE3_PROJ_IMPL.md`、`tests/unit/CMakeLists.txt` 与根 `CMakeLists.txt`
  的 phase3 相关 hunk。SubAgent `ac055ed4` 被停止未交付。甄别：git diff 逐文件归类 →
  按 P3-001 任务规格重派 attempt 2 续作，或回滚该面（保留预存 dirty）。
- **预存 dirty（GOV-002 起冻结在案，不覆盖不收编）**：artifacts/prerelease_v5/ISA-00{1,2,3}/
  MEASUREMENTS.csv、docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv、
  evidence/v6_1_rework/tasks/CHK-001/PROD_REACHABILITY.{dot,json}、
  lib/photometric_calib/cpp/test/filter_qe_provenance.json（用户域）、reports/v19r2 两文件、
  工程控制 rev3 后续微改（若再现）。
- **V7 残留（锁死不收编）**：`lib/snr_estimator/{CMakeLists.txt,include/,src/}`、
  `tests/unit/p1_noise/` untracked（详见 memory.md item 9/10 与 ACTIVITY_STATE.md）。
- **过期产物**：`artifacts/AstroCS_AUDIT_REVIEWPACK_20260909T133841Z.zip`（77MB，不含
  P1/P2 提交）；重打用 `run/local/make_audit_pack.sh`（zip 缺失已内置 python zipfile 回退）。

## 5. 执行纪律（对 SubAgent 与前台一视同仁）

- 前台=调度+简审+机器验收+精确暂存+一任务一原子 commit+push 后核对三 SHA；
  SubAgent 零 git 写操作（P2-001 自行 push 为个例已追认并补流程说明）。
- 禁 branch/worktree/clone、reset/stash/clean/rebase/amend/force push。
- 未运行不得写 PASS；P0/P1、科学容差、Windows build/test/package、manifest/hash、
  真实数据完整性不可 waiver。
- WCS-003 未达冻结 1e-4px 前：G-SCI 不开放，Phase1/2/3 不做科学验收结论。
- 全量回归已知豁免：p1_noise_adapter FAIL=F-AIO-001（BASE 冻结快照口径）；
  pytest 13 失败=BASE 预存集（含 utilization_p75_low×6，BASE-UTIL-001 归因中，
  不得凭预存豁免进入审核包）。
- 最终只能报告 READY_FOR_OWNER_REVIEW / NOT_READY / BLOCKED_EXTERNAL / PACKAGE_INVALID；
  发布裁定权在项目负责人。

## 6. 记忆与登记索引

- `memory.md`：item 9（旧 V7 包作废 42/140 + 挂账清单）、item 10（GOV-002 归档统一）、
  P1-001/P2-001 attempt 段。
- `工程控制/ACTIVITY_STATE.md`：活动控制包唯一登记源。
- 证据目录：`.dsh/control-pack-runs/Rmtucy2cqced995/evidence/<TASK>/1.json`（框架）、
  `run/local/agent_*/`（旧包）、`run/p2003_fix/`（P0 修复）、`run/aio-001/`、`run/psf-001/`。
