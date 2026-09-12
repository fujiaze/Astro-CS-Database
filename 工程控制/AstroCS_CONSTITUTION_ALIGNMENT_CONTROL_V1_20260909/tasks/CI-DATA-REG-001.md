# CI-DATA-REG-001｜DATA 产物语义登记对齐（DATA-ARTIFACTS 转绿）

## 目标
修复 `DATA-ARTIFACTS`（fast/linux-main/windows-main 全红，非豁免门）。前台本地实测失败原文：

```
DATA_ARTIFACTS_FAIL (1):
  DATA_SEMANTICS 声明但未登记: ['DATA-HIPS-001', 'DATA-TILE-001']
```

即 `docs/contracts/DATA_ARTIFACTS.md`（或等价 DATA 合同面）中**声明**了 `DATA-HIPS-001`、`DATA-TILE-001` 两个 DATA 语义项，但未在检查器要求的登记表中登记。

处置要求：
1. 先读取 `tools/check_data_artifacts.py` 判定规则，确认"登记"的机器可读形态；
2. 到 `docs/contracts/` 与 `docs/science/` 核实这两个 ID **是否真实存在对应合同正文**：
   - **存在** → 按既有登记表形态补齐登记行（保持与既有条目同构）；
   - **不存在（悬空声明）** → 这是**更高层文档断链**，按宪章 §1.1「不得选择方便的一层继续工作，必须建立 finding」登记 finding 并如实上报，**不得**为过检查而删除声明、也不得伪造合同正文。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `docs/contracts/`
- `docs/science/`

## 裁决依据（前台 rev6，见 `07_FRONT_DESK_RULINGS_20260912.md`）
- 裁决 **R-04**：以实测红灯为唯一事实源逐条落任务。
- 宪章 §1.1 权威分层与断链处置；§4.1「不得混淆的量」与 §4.3 产品 Manifest 要求 DATA 合同写明单位/坐标系/像素语义/精度/shape/无效值/所有权/生命周期。

## 非目标与禁令
- 不顺手修复域外问题；发现后登记 finding（05 号登记册续写）。
- 不修改/放宽科学公式、默认容差、冻结门或负责人裁决。
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone；SubAgent 不 git add/commit/push。
- **严禁以"放宽检查规则 / 加入 known-failures 基线 / 改 waivable / 跳过测试"的方式让红灯消失**（裁决 R-05、R-13）。
- 新增/修改测试必须同提交注册 CI 检查项（宪章 §17 + 负责人 CI 指令）；运行产物禁止落根目录。

## 必须动作
1. 读取冻结宪章相关条款、`docs/standards/STANDARDS_REGISTRY.md`、本任务域 SCI/ALG/DATA/ARCH 文档。
2. 测试设计先行：先红后绿、负向注入必败、1/N worker parity（适用时）、确定性 bitwise（适用时）。
3. 执行并保存命令证据（timeout/cwd/argv/起止/rc/stdout/stderr/SHA）；heavy 同 run ID 资源监控。
4. 同步订正 CI：新测试目标→`ci/checks.json` 显式检查项；漂移锚→复测工具。
5. 修复后必须**本地复现 CI 同构条件**验证（不得只在本机默认环境跑绿即报 PASS）。

## 验收
- write_scope 零越界；预存 dirty 零覆盖；所有新测试故障注入必败。
- 本任务对应的 CI 检查项在本地同构条件下 **exit 0**，并给出命令与日志路径。
- 一个任务一个原子 commit 并 push main；fetch 后核对 HEAD/main/origin/main 三 SHA。

## 返回证据
scope/acceptance/provenance 三检查 + changed_files + 命令日志 + 任务特化产物。
