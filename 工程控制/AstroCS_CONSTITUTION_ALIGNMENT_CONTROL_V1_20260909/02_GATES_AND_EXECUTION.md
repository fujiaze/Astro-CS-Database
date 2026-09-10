# 02｜任务图、门禁与执行纪律

## 门禁

- **G-GOV**：GOV-001、BASE-001、GOV-002 通过；宪章 FROZEN、四项有明确值、supersession 唯一、dirty 全登记。
- **G-SCI**：WCS/PSF 与 DATA 决策任务通过；科学公式或容差未被 Agent 放宽。
- **G-CODE**：AIO、三 Phase 节点化、complete 门、executor、DLL/loader/CLI 全通过；P0/P1=0。
- **G-CI**：当前同一 SHA Linux/Windows 不可豁免通过，Windows 候选必存在且 digest 有效。
- **G-REAL**：Linux 最终 SHA 全量真实数据+图像初审；Fatduck 同候选正式复验。
- **G-RELEASE**：追踪/L0/独审/审核包通过；只可 READY_FOR_OWNER_REVIEW，由 Owner 决定发布。

## 执行

- `repo-write` lane capacity=1；同工作区 tracked 写入严格串行。`read-only` 可并行，但不得读取正在变动的同域后声称稳定结论。
- 一个 task 一个可验证 commit。科学、架构、性能、文档不混提。
- 前台复跑验收、检查 write_scope/diff 纯度、精确 add、commit/push、fetch 后核对 HEAD/main/origin/main。
- 所有命令设 timeout 并记录 cwd/argv/start/end/rc/stdout/stderr/SHA；heavy 同 run ID 资源监控。
- P0/P1、Windows build/test/package、manifest/hash、真实数据完整性不可 waiver。
- 科学决策任务可因 Owner 未裁决进入 BLOCKED_EXTERNAL；不得选择方便方案。

## 证据最小字段

`task_id, owner_id, base_sha, final_sha, status, changed_files, commands, checks, artifacts, scope_clean, scientific_change, known_limits`。Worker PASS 仅表示申请前台验收。
