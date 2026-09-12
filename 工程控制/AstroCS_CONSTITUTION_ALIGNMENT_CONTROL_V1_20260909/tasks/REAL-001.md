# REAL-001｜Linux最终SHA全量真实数据终验

## 目标
三层验收（详见 tasks/REAL-001.md rev3.1 版）：A 全量 906 帧 parse/校准/solve 三级≥99% 硬门；B M42+银心 353 帧 Phase1 单帧 HiPS+9 个 Phase2 分滤镜马赛克（ivar/五键齐全，STD-F2/F3 修复后）+9 个 Phase3 平面导出+拉伸预览；C 断点续跑+资源门+机器证据。GaiaDR3 全量 solve，GaiaDR3SP 银心分支。

## 依赖
`CI-002|REAL-000|STD-F1-ADJ`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `run/`
- `artifacts/`
- `reports/`

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
