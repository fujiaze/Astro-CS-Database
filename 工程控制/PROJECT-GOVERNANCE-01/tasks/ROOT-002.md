# 任务：ROOT-002 根目录整洁的长效规则与机器门

状态：`NOT_STARTED`
层：L0　依赖：ROOT-001　文件域互斥组：S2-R

## 目标

让根目录**不会再次被污染**：把允许的根条目写成机器可校验的单一清单，补齐 `.gitignore` 缺口，并新增一个能绿能红的根目录整洁检查项。

## 基线状态（编制时实测）

- `.gitignore` 已有根目录整洁约束段（astrocs_run_*.json、/Testing/、/out/、/AstroCS.wiki/、resource_samples.csv、resource_summary.json、worker_balance.csv、alloc_report.json、alloc_samples.csv、/graph/），但 ① 无 p*-files.patch、run_context.json 兜底；② 无机器门验证根目录整洁。
- ENGINEERING_SPEC §7 的白名单是散文式代码块，机器无法直接消费（现无任何检查器读取它）。
- `ci/checks.json` 145 项中没有根目录整洁项（GAP-016）。

## 权威依据
- ENGINEERING_SPEC.md §7（根固定条目白名单）、§8（ci/checks.json 唯一注册表；每项能绿能红）
- docs/ci/01_CHECKS.md §2（CHK-* 检查项清单）、§4（新增检查项流程）
- ASTROCS_DESIGN.md §6.3（运行产物不得落进程 CWD）

## 改动范围（文件域）
### 允许改
- `.gitignore`
- 新增 `ci/root_manifest.json` 与检查器 `tools/quality/check_root_cleanliness.py`
- `ci/checks.json` 中本任务对应登记项
- `tests/quality/**`
- 必要的 `docs/ci/01_CHECKS.md` 条目补充（若需登记新 CHK-*）

### 禁止改
- 改动任何产品源码与权威文档内容
- 放宽或删除既有检查项
- 把白名单写成只增不减的豁免表（新增根条目必须走负责人确认流程）

## 步骤
1. 把 ENGINEERING_SPEC §7 白名单落成机器可读清单 `ci/root_manifest.json`（allowed_files / allowed_dirs / ignored_patterns），并与文档逐条对照（给出对照表，防止清单比文档更宽）。
2. 实现检查器：① 顶层条目 ∉ 白名单且未被登记为本地保留 → 红；② 根目录出现 astrocs_run_* 类运行产物 → 红；③ 文档要求存在但实际缺失的条目 → 红；④ 输出机器 JSON 便于 CI 消费。
3. 补齐 `.gitignore`：把「本地保留但不入库」的根条目全部登记（p*-files.patch、run_context.json 等），并写清每条的保留理由与清理条件。
4. 注册到 `ci/checks.json`（ID 形如 CHK-ROOT-CLEAN，P0）并给正例与三类负例（多出条目 / 运行产物落根 / 白名单条目缺失）。

## 验收门（前台独立复跑）
- [ ] 检查器正例 rc=0；三类负例各自 rc≠0（给出注入方式与失败输出）
- [ ] 白名单与 ENGINEERING_SPEC §7 逐条对照，无「比文档更宽」的条目（给出对照表）
- [ ] `python3 -m unittest discover -s tests/quality -t tests/quality` → rc=0
- [ ] 在根目录人为新建一个未登记文件后检查器必红（给出实验证据，探针随后删除）

## 证据命令
```bash
mkdir -p run/PROJECT-GOVERNANCE-01/ROOT-002/logs
python3 tools/quality/check_root_cleanliness.py --json-out run/PROJECT-GOVERNANCE-01/ROOT-002/logs/root.json; echo "rc=$?"
# 负例：新建探针文件后复跑（应 rc!=0），随后删除探针
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

1. 限「改动范围」内的改动；
2. `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/` 下的命令日志与机器输出；
3. 自证摘要（字段：任务ID / 改动清单 / 逐条验收命令与退出码 / 未决项）。
