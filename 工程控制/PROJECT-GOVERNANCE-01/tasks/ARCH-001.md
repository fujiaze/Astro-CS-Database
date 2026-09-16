# 任务：ARCH-001 建立目标源码根与等价迁移骨架

状态：`NOT_STARTED`
层：L2　依赖：MOD-001　文件域互斥组：S4-A

## 目标

按 ASTROCS_DESIGN §7.1 建立 `lib/algorithms/**` 与 `lib/infrastructure/**` 两个源码根，用 `git mv` 迁移且保证科学行为等价（bitwise 或既有冻结容差内）。

## 基线状态（编制时实测）

- `ls -d lib/algorithms lib/infrastructure` → 均不存在；`lib/` 为 31 个扁平旧目录。
- 根 `cli/`、`runtime/`、`providers/`、`modules/` 未在 ENGINEERING_SPEC §7 的目标结构中。
- 目标结构含 16 个算法模块 + `shared/`；基础设施含 `cli/{normalize,mosaic,export}`、scheduler、pipeline、aio、benchmark、observability、gaia_xpsd_client、acr（DORMANT）、hips_browser（不进产品）。

## 权威依据
- ASTROCS_DESIGN.md §7.1（顶层结构唯一）、§7.3（模块与 DLL/SO 边界）
- ENGINEERING_SPEC.md §3（架构重构不得同时改科学语义；迁移必须等价）、§4、§7
- ASTROCS_DESIGN.md §1.3（禁止借架构整理修改科学公式）

## 改动范围（文件域）
### 允许改
- `lib/algorithms/**`、`lib/infrastructure/**`（新建目录与迁移落位）
- `cmake/**`（源文件清单与 target 定义，不含根 CMake 最终切换）
- 被迁移模块的 `README.md`/`module.yaml`/共址测试路径

### 禁止改
- 科学公式、容差、归约顺序、kernel 语义
- 根 `CMakeLists.txt` 的最终切换（归 INT-001）
- CLI 行为与命令树（归 CLI-001）
- 双实现/长期 facade：旧路径不得与新路径同时编译

## 步骤
1. 建目录骨架与模块边界（只建目录 + 迁移一个可独立验证的最小模块作为样板，确定迁移模式）。
2. 按 `git mv` 迁移公共头/实现/共址测试；一次迁移一个模块，保持符号名与行为不变。
3. 在 `cmake/` 建立迁移清单（旧路径 → 新路径 → 完成状态），供 INT-001 做最终切换；不为兼容旧路径留长期 facade，必要时只允许一次性的编译期断言检查。
4. 用固定输入向量证明迁移前后等价：对已迁移模块跑迁移前后同一测试二进制/同一向量，比较输出哈希或既有冻结容差内的最大偏差，结果落日志。

## 验收门（前台独立复跑）
- [ ] 目标目录结构与 ASTROCS_DESIGN §7.1 逐项一致（给出目录树对照）
- [ ] 已迁移模块的旧路径不再出现在任何构建目标中（`grep` 断言）
- [ ] 等价证据：迁移前后固定向量输出哈希一致，或最大偏差 ≤ 既有冻结容差（给出命令与数值）
- [ ] `git diff --stat` 不改变任何科学常量/公式/默认容差；`git log --follow` 显示为 rename 而非删除+新增

## 证据命令
```bash
mkdir -p run/PROJECT-GOVERNANCE-01/ARCH-001/logs
find lib -maxdepth 2 -type d | sort | tee run/PROJECT-GOVERNANCE-01/ARCH-001/logs/tree.txt
git status --porcelain=v1 | grep '^R' | tee run/PROJECT-GOVERNANCE-01/ARCH-001/logs/renames.txt
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
