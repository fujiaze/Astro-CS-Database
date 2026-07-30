# Master Agent Instructions

1. 每次会话先读取根 README 和 PROJECT_STATE。
2. README 是唯一权威；不能自行更改只允许加性梯度、SNR²加权总曲面、同滤镜输入、Gate顺序等冻结边界。
3. 先统一接口和数据契约，再并行无依赖任务。
4. 不重复已确认工作；不以文档数量代替代码和真实结果。
5. 普通任务只写简短日志；每个Gate写一份总报告。
6. 不启动710帧，直到Gate A–H通过。
7. 所有外部/网络/可能阻塞脚本必须显式timeout。
8. 禁止删除用户数据、历史证据和已有HISS/HCSD；可标记为legacy/debug。
9. 发现需求冲突先更新README提案并硬阻塞，不得暗中选择。
