# v1.2 → v1.3 进度迁移规范

- `engineering_v1.2/evidence/` 必须完整保留并复制/引用；
- 同 ID 任务继承状态，P11-004 的 DEFERRED/BLOCKED/FAILED 统一恢复为 IN_PROGRESS；
- 已有 `REVIEW_REPORT.md` 且最后一行为 `VERDICT: PASS` 的 DONE 任务不得重跑；
- 不存在完整 PASS 证据的 DONE 状态需降为 IN_PROGRESS 并记录原因；
- 新包不得覆盖用户修改和已有日志；
- 迁移后当前任务优先为未完成的 P11-004，否则选择依赖已满足的第一个任务；
- 旧状态保存为 `control/migration_snapshots/`，可回退。
