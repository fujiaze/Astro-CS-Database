# 任务：DATA-001 建立统一数据对象合同链

状态：`NOT_STARTED`
层：L1　依赖：GOV-001, DOC-001　文件域互斥组：S2-A

## 目标

把 UNIFIED_MODEL §2 的对象表变成 `contracts/schemas/` 下的唯一事实源：14 个对象各自有 schema ID、单位、无效值语义与 provenance，并让「一个字段承载多个含义」在机器门上必败。

## 基线状态（编制时实测）

- 合同分散三处：`contracts/config/`、`contracts/data/`、`contracts/schemas/v6/`；`docs/contracts/` 下的 `INDEX.yaml`、`DATA_ARTIFACTS.md`、`DATA_SEMANTICS.md` 与 `docs/contracts/v6/data/01_units_and_bunit.md` … `10_migration_and_open_items.md` 并存两套口径。
- `docs/design/UNIFIED_MODEL.md` 已冻结 14 个对象名，但仓库内没有与之一一对应的 schema ID 清单。
- 现有质量检查器 `tools/quality/contracts/check_config_contracts.py`、`check_science_units.py`、`check_api_contracts.py` 绑定旧合同布局。

## 权威依据
- ASTROCS_DESIGN.md §2（数据对象与配置分离；对象禁止互相冒充）
- docs/design/UNIFIED_MODEL.md §1（统一线性观测模型 d_k = A_k x + n_k）
- docs/design/UNIFIED_MODEL.md §2 对象表与「可否作权重」列
- ENGINEERING_SPEC.md §3（数据对象按 UNIFIED_MODEL 区分）、§4.7（端口引用有效 DATA 合同）

## 改动范围（文件域）
### 允许改
- `contracts/schemas/**`（新建/迁移）
- `contracts/data/**`、`contracts/config/**`（迁移；只保留兼容期映射，不保留双份定义）
- `docs/contracts/**`（索引与语义文档，不改科学定义）
- `tests/contracts/**`

### 禁止改
- 科学公式、默认容差、SCI/ALG 冻结定义
- 任何 `lib/**` 实现、`cli/**`、调度器
- 为一个对象保留两个等价 schema

## 步骤
1. 逐对象落 schema：`signal`、`variance`、`ivar`、`source_snr`、`depth_m5`、`frame_snr`、`point_information`、`psfsw_robust_weight`、`sparse_snr_layer`、`support`、`coverage`、`validity`、`rejection`、`provenance`；每个给出 schema ID、单位（含 BUNIT 语义）、无效值/缺失表示、精度、可否作权重。
2. 在 schema 内禁掉模糊字段名：`weight`、`value`、`mask`、`snr` 单独出现即不合格，必须使用对象全名（UNIFIED_MODEL §2 末条）。
3. 迁移重复定义到 `contracts/schemas/` 单一事实源，并在 `docs/contracts/INDEX.yaml` 记录旧路径→新路径映射与废弃时间点。
4. 为三类配置分离留锚点（对象定义在此，配置 schema 归 CFG-001）：`phase_config` / `cpu_profile` / `run_manifest` 不得共用字段名承载不同含义。
5. 写正例与负例：正例=合法对象文档通过校验；负例=① `weight` 模糊字段 ② `snr` 冒充 variance ③ `coverage` 当 rejection ④ 跨对象错误连接（把 `source_snr` 接进要求 `variance` 的端口）。

## 验收门（前台独立复跑）
- [ ] 所有 schema 可被仓库既有 validator 加载，且 schema ID 全局唯一（给出加载清单）
- [ ] 四个负例各自必败（rc≠0 或 validator 明确报错），给出失败输出片段
- [ ] `python3 -m unittest discover -s tests/contracts -t tests/contracts` → rc=0
- [ ] `git diff --stat` 不含 `lib/**`、`cli/**`、`docs/science/**`、`docs/algorithms/**`

## 证据命令
```bash
mkdir -p run/PROJECT-GOVERNANCE-01/DATA-001/logs
ls contracts/schemas | tee run/PROJECT-GOVERNANCE-01/DATA-001/logs/schemas.txt
python3 -m unittest discover -s tests/contracts -t tests/contracts 2>&1 | tail -20; echo "rc=$?"
```

## 执行规则（每个任务都适用）

- 开工前按 `AGENTS.md §1` 读完本任务「权威依据」列出的全部条款；未读不开工。
- 只改本文件「改动范围」声明的文件域；**不顺手改无关代码**；不改 `docs/science/**` 公式与 `docs/algorithms/**` 推导。
- 科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、默认容差**一律只读**（ENGINEERING_SPEC §3）。
- 工作区在本控制包编制前已有大量预存修改与未跟踪文件（见 BASE-001）：**不得** reset/stash/clean/checkout，**不得**把它们混进本任务的改动。
- SubAgent 零 git 写权限：不 commit、不 push、不建分支；改动留在工作区并交自证材料，由前台按任务原子提交。
- 所有外部命令带 `timeout`，日志落 `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/`（`run/` 已 gitignore，不入库）。
- 报告「完成」必须附：命令、退出码、关键输出片段、产物路径。禁止用「环境问题/工具问题」掩盖失败。
- 发现文档冲突、科学歧义或无法证明的状态：登记 `BLOCKED`（控制包内）或 `UNRESOLVED`（GAP_AUDIT 内），不得自行选口径。

## 交付物

1. 限「改动范围」内的代码/合同/配置改动；
2. `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/` 下的命令日志与机器输出；
3. 自证摘要（字段：任务ID / 改动文件清单 / 逐条验收命令与退出码 / 未决项）。
