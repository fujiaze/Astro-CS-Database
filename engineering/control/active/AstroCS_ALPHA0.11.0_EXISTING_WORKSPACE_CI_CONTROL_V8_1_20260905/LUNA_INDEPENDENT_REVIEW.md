# Luna 独立复审结论

复审对象：审核基线 `257ae1f4...` 与 V8 初稿方向。以下问题已进入 `CONTROL_TASK_LEDGER.csv`，不得只改文档状态：

1. 旧包要求恢复 detached worktree/WIP，与单一 main 冲突；V8.1 改为原地使用现有 main，禁止新增 worktree/分支/额外克隆，且不清理历史工作区记录。
2. Phase2 的七个 IR 节点疑似全部绑定同一 `P2Api/session_run`，artifact 只存在于元数据，可能造成重复完整计算；由 V8-RT-001/002 修复。
3. Phase2 coverage/sampler/UPM/rejection/integration 存在串行重计算；由 V8-P2-001/002 实测与并行化。
4. `module_adapters.cpp` 疑有 `workers_=2`；由 V8-CPU-001 删除并统一动态预算。
5. benchmark/profile/provider 仅有字符串或接口声明，未证明进入真实执行入口；由 V8-CPU-002/003 建闭环 trace。
6. 资源监控部分字段由模块注入，PSS/system CPU/IO/采样间隔语义不完整，短任务/mixed 可能误 PASS；由 V8-MON-001/002 改为系统观测与负向测试。
7. VERSION/CMake/README/module summary/活动文档版本漂移；由 V81-ADOPT-006、V8-RT-001、V8-DOC-001 机器生成并统一。

复审还要求：CI 不得因 IR 标记 `parallel=true`、descriptor 写 `parallel_ok` 或日志声明 worker 数就判断并行；必须以实际执行入口、调用计数和线程级资源样本为证据。
