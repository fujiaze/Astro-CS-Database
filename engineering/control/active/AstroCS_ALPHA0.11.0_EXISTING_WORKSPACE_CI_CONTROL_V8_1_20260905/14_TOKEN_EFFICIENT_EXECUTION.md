# 14｜低 Token、少人工执行

## Agent 每次只加载

1. `START_PROMPT.txt`；
2. `validators/next_tasks.py` 输出的一个任务；
3. 对应 `tasks/*.md` 小节；
4. 该模块当前 SCI/ALG/API 合同。

不要每轮重读全部历史日志、旧控制包和完整源树。前台只在首次原地接管、门禁失败和最终打包时读取全局文件。

## 机器替代文字流程

- 下一任务：DAG 脚本计算；
- 影响测试：`impact_map` 计算；
- PASS：exit code + schema 计算；
- 版本/API/追踪/运行图：从源码生成后对比；
- 资源利用：原始采样重算；
- 审核包：白名单生成；
- Owner 提醒：单一 Issue 自动更新。

Agent 不写长篇计划和重复汇报。正常任务只返回：task ID、SHA、changed files、检查计数、耗时、PASS/FAIL。失败只返回 failing check、最小复现和下一修复 task。

## 禁止低效防御

- 不为未复现风险增加 fallback/retry/兼容层；
- 不重复校验同一不可变输入；
- 不设置中间人工审批；
- 不用多份状态台账；
- 不用自然语言复制机器 JSON；
- 不跑历史版本全链路；
- 不因 Fatduck 离线重复询问 Owner。

只有科学/API 合同冲突、权限缺失和最终发布决定需要人介入。
