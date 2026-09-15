# AstroCS V6 并行科学与工程实施包

## 运行角色

控制器/主 Agent 在控制包运行期间只做：识别 READY、并行派发、接收返回、检查写域/证据、作 pass/retry 简审、精确集成提交。不得亲自执行任何任务正文。任务正文由子代理完成。

## 并行策略

- 每轮把所有依赖已 PASS 且 write_scope 互斥的 READY 任务同时派发。
- Wave 1 可并行 5 项，Wave 3 可并行 7 项，Wave 5 可并行 9 项，Wave 10 可并行 2 项。
- 子代理读宪章和任务卡，不再派生子代理，不 commit/push，不扩写域。
- 返回进入 REVIEW；控制器只根据任务卡验收。失败 retry，不用控制器代做。
- 审核通过后控制器按任务逐一精确 add/commit/push main，再解锁依赖。执行本包时，应先由控制器把本轮已批准的设计/控制包基线作为单独治理提交推送；本包交付者本轮不替执行 Agent 做提交。

## 科学模式

Phase2 正式支持 `point_information`、`surface_gls`、`psfsw_robust`；`psf_snr_power` 只有在 SCI-PSFW 冻结后才可进入生产。PSFSW 是正式可选集成权重，不是 QA-only，但无量纲复合权重不得冒充 ivar/Fisher information。
