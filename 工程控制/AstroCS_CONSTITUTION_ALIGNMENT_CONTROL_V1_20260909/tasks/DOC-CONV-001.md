# DOC-CONV-001｜L0与模块状态文档收敛

## 目标
REVIEW.md/docs/owner/RELEASE_STATUS.md/MODULE_MAP.md/lib/*/README 按当前实际收敛：三 Phase 节点化已完成、四投影已实现、RT/MOD/CLI 已落地；删除已修复项的 NOT_VERIFIED 陈述（以当前 commit 证据替换）；状态词区分 CONTRACT_READY/IMPLEMENTED/INSTALLED/VERIFIED。

## 依赖
`STD-REG-001`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `REVIEW.md`
- `docs/`
- `memory.md`

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
- doccheck 全套 PASS；无相互矛盾的 IMPLEMENTED/MISSING。
- 一个任务一个原子 commit 并 push main；fetch 后核对 HEAD/main/origin/main 三 SHA。

## 返回证据
scope/acceptance/provenance 三检查 + changed_files + 命令日志 + 任务特化产物。
