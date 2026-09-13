# 工程控制 — 控制包活动状态总表（唯一登记源）

> 建立日期：2026-09-09（GOV-002，ASTROCS-CONSTITUTION-ALIGNMENT-V1）
> 上位约束：仓库根 `ASTROCS_PROJECT_CONSTITUTION.md`（FROZEN，唯一最高约束；
> 历史文档、旧报告和旧控制包仅作线索——宪章 §1.1 权威分层第 8 条）。
> 本文件是 `工程控制/` 与 `engineering/control/` 全部控制包活动状态的
> **唯一登记源**；任何包的 ACTIVE/ARCHIVED 判定以本表为准，
> 与 `docs/DOCUMENT_INDEX.yaml`（目录级聚合）和 `memory.md`（交接锚）互相引用。

## 1. 状态枚举

- `ACTIVE`：唯一当前执行控制包；其余包一律不得执行。
- `ARCHIVED_SUPERSEDED`：已被上位文件或更新控制包替代；仅作历史线索。
- `ARCHIVED_DISARMED`：中途 disarm 终止的队列线；未完成任务状态保留作历史，不得据此续作。
- `REFERENCE_TEMPLATE`：非控制包的格式模板/规范/原件归档区。

## 2. 状态表

| 包 / 条目 | 包 ID | 状态 | 位置 | 说明 |
|---|---|---|---|---|
| AstroCS 科学重审与全流程恢复工作包 V3 | `AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912` | ACTIVE | 解压件：`工程控制/AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912/`；zip 原件：`工程控制/_control_packs/AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912.zip` | 2026-09-13 负责人指令：「使用该工程包替代以前的工程包，作为权威并执行」。不依赖 CPRun/CP 插件，前台直接以 SubAgent 派发；基线 SHA `f5f944a5`，阶段 0 `RESCUE-SNAPSHOT` 已冻结（`run/release-rescue/snapshot/STARTUP_SNAPSHOT.json`） |
| ASTROCS-CONSTITUTION-ALIGNMENT-V1 | `AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909` | ARCHIVED_SUPERSEDED | `工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/`（**原位保留**：`docs/standards/checks/check_standards_registry.py` 等多处检查器引用其 `05_FINDINGS_REGISTER_20260911.md`，不得移动/删除） | 2026-09-13 被 RESCUE-V3 取代；历史裁决 R-01..R-31 与 findings 登记册仅作追溯线索，本包不得继续派工或据此续作 |
| V8.1 CI 控制包 | `AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905` | ARCHIVED_SUPERSEDED | 解压件：`工程控制/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905/`；tracked 镜像：`engineering/control/archive/2026-09-09_superseded_V8.1_CI_CONTROL_20260905/`（56 文件原样移动，SHA-256 零差异，GOV-002） | 治理前提被宪章 supersession 替代；其 `01_FROZEN_CONSTRAINTS.md` 上级来源已降级 ARCHIVED_NON_NORMATIVE；归档说明见镜像目录 `README_ARCHIVED.md` |
| V7 MODULAR REFOUNDATION | `AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3` | ARCHIVED_DISARMED | `工程控制/AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3/`（台账 `TASK_LEDGER.csv` SHA-256 见 BASE-001 冻结） | 140 任务队列 disarm 终止于 42/140（41 submit + NOISE-IMPL 沿旧包收尾）；旧 cp 运行 `Rmtuajdhv73d430` 已 force 替换废弃；队列台账/TASK_STATE/CSV/COMMIT_LEDGER reconcile --strict rc=0；未完成任务保留 NOT_STARTED 作历史；交接锚 = `memory.md` item 9 |
| V6.1 REWORK | `AstroCS_V6_1_REWORK_CONTROL_20260831` | ARCHIVED_SUPERSEDED | `工程控制/AstroCS_V6_1_REWORK_CONTROL_20260831/`（台账 `03_REWORK_TASK_LEDGER.csv` SHA-256 见 BASE-001 冻结） | 交付面由后续轮次与宪章对齐包接管 |
| 2026-09-02 legacy 工程控制归档 | V1.3–V6.1 历史控制包集合 | ARCHIVED_SUPERSEDED | `engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/` | GOV-002（SA-GOV-01，2026-09-02）归档；目录级 README_ARCHIVED.md |
| cprun-v4 控制包模板 | — | REFERENCE_TEMPLATE | `工程控制/cprun-v4-pack-template/` | 4d3feaf4 收录的 owner 指定模板，非执行包 |
| 控制包 zip 原件 | — | REFERENCE_TEMPLATE | `工程控制/_control_packs/` | 原件归档区（目录规范），不执行 |
| 包规范 | — | REFERENCE_TEMPLATE | `工程控制/包规范.md` | 控制包/Worker 返回包/审核包通用格式（1.0） |

## 3. V7 线中断态残留登记（如实，不清理不提交）

以下残留为 V7 线 disarm 时的在制状态，GOV-002 归档**只登记、不清理、不提交、
不收编**（预存 dirty 保护，00_READ_FIRST §3）：

- 未跟踪半成品（P1-NOISE-IMPL）：`lib/snr_estimator/CMakeLists.txt`、
  `lib/snr_estimator/include/`、`lib/snr_estimator/src/`、`tests/unit/p1_noise/`；
- 根 `CMakeLists.txt` 追加块未提交（预存 8 个 tracked modified 之一）；
- 处置去向：交由当前 ACTIVE 包 `AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912`
  按其任务图（`TASK_LIST.md`）正式裁决；在此之前任何任务不得引用为当前实现或顺手删除。

## 4. V7 线未修候选与挂账去向（登记，不处置）

- DISP 未修候选：DISP-WCS-007（deg/rad 成对抵消）、DISP-WCS-008（AP/BP 逆
  42px vs 冻结 1e-4px）、`iter_trans_solve` 空对 UB、DISP-PSF-007
  （dpsf_fit_batch 无 w/h 校验 size 下溢）、p1hips `make_tmp_dir` 无清理
  tmpfs 假败风险——原始登记见 `memory.md` item 9；处置去向 = ACTIVE 包
  任务图（WCS/PSF 相关任务卡）与 finding 机制，本任务不改科学裁决。
- 挂账 10 项：全文见 `run/local/HANDOFF_CONTROL_PACK_20260909.md` §6
  （`run/*` gitignore，仅现场可读；本表不复制长文）。

## 5. 旧台账冻结参照

旧包台账/检查点共 9 文件 SHA-256 冻结于 cprun run `Rmtucy2cqced995` 的
BASE-001 证据（`evidence/BASE-001/freeze_snapshot_r1.json` `ledgers` 字段）：
本 ACTIVE 包 `TASK_LEDGER.csv`、V6.1 `03_REWORK_TASK_LEDGER.csv`、
V7 `TASK_LEDGER.csv`、V8.1 `CONTROL_TASK_LEDGER.csv`/`CURRENT_CHECKPOINT.json`/
`baseline/V7_1_STATIC_TASK_LEDGER.csv` 及 V8.1 tracked 镜像同哈希三份。
归档移动后 V8.1 镜像三文件内容 SHA-256 不变（仅路径变更，git R100 56/56 证明）。
冻结参照核对（GOV-002 attempt 1 复跑，BASE=45f80776）：`CONTROL_TASK_LEDGER.csv`
（ee6a5d7e…）与 `CURRENT_CHECKPOINT.json`（b1ebbf5c…）同 BASE-001 冻结值一致；
`baseline/V7_1_STATIC_TASK_LEDGER.csv` 镜像版（32e3e414…，自 a4fdee3f
V81-ADOPT-001 2026-09-05 入库即此值）与 BASE-001 快照登记值（524ab719…，=
`_control_packs/` zip 原件与 `工程控制/` 解压件现行内容，快照时点该 tracked
文件工作区 dirty）不同——快照为工作区口径、a4fdee3f blob 为 git 口径，两条
参照链各自完整可溯；口径差登记 F4（见 §6），不随归档改写。

## 6. 归档后域外工具影响（finding 登记，域外不顺手修）

- **F1（本归档引起，已由前台闭环）**：`ci/reconcile_state.py`（V8.1 线对账工具）
  的 `V71_LEDGER` 常量曾指向旧 active 路径
  `engineering/control/active/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905/baseline/V7_1_STATIC_TASK_LEDGER.csv`；
  归档移动后 `--current-first --strict` 的 v71_ledger_readable / v71_coverage /
  v71_no_extra_tasks 三检查 fail（rc=1），其余检查 PASS。
  修复（前台 d1ac4dfc，2026-09-10，ci/ 域外任务）：该常量已改指
  `engineering/control/archive/2026-09-09_superseded_V8.1_CI_CONTROL_20260905/baseline/V7_1_STATIC_TASK_LEDGER.csv`；
  GOV-002 attempt 1 复跑 `--current-first --strict` rc=0，9/9 checks PASS，
  断链闭环（负向注入旧路径缺失形态可复现 rc=1，见 BASE=45f80776 证据）。
- **F4（GOV-002 attempt 1 复跑登记，历史口径差，非归档动作引起）**：
  V8.1 镜像 `baseline/V7_1_STATIC_TASK_LEDGER.csv` 当前 blob 32e3e414…（自
  a4fdee3f V81-ADOPT-001 2026-09-05 入库）≠ BASE-001 freeze_snapshot_r1/r2
  （工作区口径，双遍一致）登记值 524ab719…（= `工程控制/_control_packs/`
  zip 原件与 `工程控制/` 解压件现行内容；快照时点该 tracked 文件工作区 dirty）；
  归档移动本身 git R100 56/56 零差异不受影响。处置去向：owner/域外裁决
  BASE-001 冻结口径与 a4fdee3f 入库口径的取舍，本任务不改任何域外文件。
- **F2（预存，与本任务无关）**：`ci/tests/test_impact_map.py::test_required_path_domains_covered`
  失败（schemas/** 域探针未被 impact_map 覆盖，009ee419 根目录归纳遗留）；
  `ci/impact_map.json` 与该测试文件工作区对 HEAD 零 diff，证明非本任务引起。
- **F3（历史证据保留）**：`evidence/v8_1_ci_control/`（232 个 tracked 文件）
  为 V8.1 线历史任务证据，内含当时 active 路径引用，属历史事实记录，
  不随归档改写。

## 7. 一致性自检

状态唯一性与路径存在性机器检查（只读，随验收复跑）：

```bash
python3 - <<'EOF'
import re, pathlib, sys
root = pathlib.Path('.')
text = (root / '工程控制/ACTIVITY_STATE.md').read_text(encoding='utf-8')
rows = re.findall(r'^\| ([^|]+) \| ([^|]+) \| (ACTIVE|ARCHIVED_SUPERSEDED|ARCHIVED_DISARMED|REFERENCE_TEMPLATE) \| ([^|]+) \|', text, re.M)
assert rows, '状态表解析失败'
active = [r[0].strip() for r in rows if r[2] == 'ACTIVE']
assert len(active) == 1, f'ACTIVE 必须唯一, got {active}'
for name, _pid, _status, loc in rows:
    for p in re.findall(r'[`［]?((?:工程控制|engineering)/[^`；;\s]+)', loc):
        q = pathlib.Path(p)
        assert q.exists() or (root / p).exists(), f'{name}: 路径不存在 {p}'
print(f'ACTIVITY_STATE 自检 PASS：{len(rows)} 行，唯一 ACTIVE = {active[0]}')
EOF
```
