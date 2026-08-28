# SCI-005 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS SCI-005 行; docs/science/PHASE2_UPM.md(T106 冻结版 122 行); UPM_SOLVER.md; DATA_SEMANTICS §5。

## 动作
1. 差距分析: 缺 4 节 + claim ID 行("SCI-UPM-001..010, WEIGHT-001, PERSIST-001")规范化。
2. 补四节: 3a frame(像素域 control cell+frame_id 绑定+最小 frame_id gauge); 9a 专属问题 6 项逐项(观测方程加性模型/控制点 8×8 patch median/光度面 basis=每帧每 cell 一自由度双线性/正则化 Huber IRLS+弱零锚 0.001/gauge=连通分量独立参考帧+退化路径/接缝指标预冻结于 SYN-005); 14 文献(项目原创声明+Huber 1964 文章级+Tikhonov 教科书级+k_corr=1.4 项目自产 MC 证据+Tukey/MAD 复用); 15 Acceptance+SYN-005 转换映射。

## 验证
- SCIENCE_CONTRACT_LINT_PASS files=1 sections=15。
- 全量回归 unittest 19/19 OK。

## 产物
docs/science/PHASE2_UPM.md(补四节+ID 规范化); 本日志。

## PASS 判定
项目原创推导完整(目标函数/参数单位/唯一性条件=连通分量 gauge+零锚明确); 接缝指标映射 SYN-005 预冻结; 无 TBD。SCI-005 = PASS。
