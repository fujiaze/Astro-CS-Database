# CI-REPAIR-001｜建立CI修复常驻线

## 目标
1. 创建 run/ci_repair/ 轮报机制：每轮结束回拉 GitHub Checks API 全量结果（gh api 或 python urllib，带 timeout）+ run/ci/monitor/*.json；2. 生成 <round>/FINDINGS.json（失败 job/检查/日志摘录/归因域）；3. 本轮修复对象=上轮 e6254d4d run 的全部红灯；4. 修复须在本轮 GitHub 双虚拟机复跑绿后闭环。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `run/ci_repair/`
- `ci/`
- `tools/quality/`

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
- 按上方目标逐项通过，前台机器复跑后才可 PASS。
- 一个任务一个原子 commit 并 push main；fetch 后核对 HEAD/main/origin/main 三 SHA。

## 返回证据
scope/acceptance/provenance 三检查 + changed_files + 命令日志 + 任务特化产物。
