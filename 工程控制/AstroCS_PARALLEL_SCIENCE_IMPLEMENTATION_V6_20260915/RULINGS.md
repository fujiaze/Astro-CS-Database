# V6 裁决

1. 执行 Agent 允许并应使用子代理并行处理 READY 任务。
2. 控制器在控制包运行中只派发、简审、集成提交，不亲自执行任务正文。
3. PSF Signal Weight 双轨：W_info 为严格点源信息；psfsw_robust 为正式可选 conventional integration 权重。
4. PSFSW 四分量、共同星集、selection function、归一和 validity 必须可审计。
5. psfsw_robust 不写入 ivar；输出 covariance 从实际组合系数传播。
6. 同波并发必须 write_scope 互斥；共享接线由后续 integration 任务独占。
7. 子代理不 commit，控制器审核后按任务原子 commit/push main。
