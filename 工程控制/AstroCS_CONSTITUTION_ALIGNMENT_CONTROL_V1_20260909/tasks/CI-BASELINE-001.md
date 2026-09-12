# CI-BASELINE-001｜known-failures基线机器化

## 目标
1. known_failures_baseline.py 接入 CI：全量测试结果必须满足 失败集 ⊆ 版本化基线（基线入库，每项含首次登记 commit/原因/owner）；2. 新失败不在基线 = 检查 FAIL；3. 把 F-AIO-001(p1_noise_adapter)、UT-CLI 修复前遗留等显式登记进基线。

## 依赖
`CI-REG-002`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `tools/quality/`
- `ci/`
- `tests/`

## 非目标与禁令
- 不顺手修复域外问题；发现后登记 finding（05 号登记册续写）。
- 不修改/放宽科学公式、默认容差、冻结门或负责人裁决。
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone；SubAgent 不 git add/commit/push。
- 新增/修改测试必须同提交注册 CI 检查项（宪章 §17 + 负责人 CI 指令）；运行产物禁止落根目录。

## 必须动作
1. 读取冻结宪章相关条款、STD 注册表（如已建立）、本任务域 SCI/ALG/DATA/ARCH 文档。
2. 测试设计先行：先红后绿、负向注入必败、1/N worker parity（适用时）、确定性 bitwise（适用时）。
3. 执行并保存命令证据（timeout/cwd/argv/起止/rc/stdout/stderr/SHA）；heavy 同 run ID 资源监控。
4. 同步订正 CI：新测试目标→checks.json 显式检查项；漂移锚→复测工具。

## 验收
- write_scope 零越界；预存 dirty 零覆盖；所有新测试故障注入必败。
- 构造一个不在基线的假失败必 FAIL；基线项失败全绿。
- 一个任务一个原子 commit 并 push main；fetch 后核对 HEAD/main/origin/main 三 SHA。

## 返回证据
scope/acceptance/provenance 三检查 + changed_files + 命令日志 + 任务特化产物。
