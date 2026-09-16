# 任务：REAL-001 执行最终 SHA 跨平台真实数据与图像验收

状态：`NOT_STARTED`
层：L8　依赖：QA-001, PKG-001　文件域互斥组：S10

## 目标

对同一 commit/包 hash，在 Linux 与 Windows 分别跑 normalize/mosaic/export 真实数据流，核验 manifest/provenance/资源门，并完成图像科学初审与 Windows（Fatduck）复验。

## 基线状态（编制时实测）

- ASTROCS_DESIGN §11.2 的验证层级末端：Linux 真实数据流终验 → Windows/Fatduck 复验 → 图像审核 Agent 初审 → Owner 终审。
- 编制时未见新文档集对应的真实数据证据（GAP-020）。

## 权威依据
- ASTROCS_DESIGN.md §11.1（科学正确性验证项）、§11.2（验证层级）、§11.3（合成测试不等于真实数据 VERIFIED）
- CONTROL_PACK_SPEC.md §9（真实数据终验先于完成声明）

## 改动范围（文件域）
### 允许改
- run/PROJECT-GOVERNANCE-01/REAL-001/**
- reports/PROJECT-GOVERNANCE-01/**
- ACCEPTANCE.md 对应行

### 禁止改
- 为过验收改代码
- 使用不同 SHA/包
- 宣布发布
- 以合成测试替代真实数据

## 步骤
1. 锁定候选：记录 commit SHA、Linux/Windows 包 hash、工具链版本。
2. Linux 真实数据：分别运行三个命令，核验输出 manifest/provenance/hash、资源门结果、产品的独立可读性。
3. Windows/Fatduck 同 SHA 复验；不可达时如实登记 AWAITING_WINDOWS_VALIDATION，不得标记通过。
4. 图像审核：给出量化证据（接缝/背景/星形/排异/黑洞/预测 vs 实测噪声），Agent 初审结论明确 PASS/FAIL，Owner 终审留白。
5. 任何失败回到对应实现任务补修；本任务不改代码。

## 验收门（前台独立复跑）
- [ ] Linux 与 Windows 证据的 commit SHA 与包 hash 完全一致（列表对照）
- [ ] 三个命令的真实产品均可由独立工具读取（给出工具、命令与关键字段）
- [ ] 资源门与 provenance 核验通过（给出报告路径）
- [ ] 图像审核有明确 PASS/FAIL 结论与量化证据；Windows 未完成时状态为 AWAITING_WINDOWS_VALIDATION

## 证据命令
```bash
mkdir -p run/PROJECT-GOVERNANCE-01/REAL-001/logs
git rev-parse HEAD | tee run/PROJECT-GOVERNANCE-01/REAL-001/logs/sha.txt
sha256sum <linux-pkg> <win-pkg> | tee run/PROJECT-GOVERNANCE-01/REAL-001/logs/pkg-hashes.txt
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
