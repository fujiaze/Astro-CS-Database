# Master Agent Instructions

## 目标优先级

1. 先建立事实与接口约定，再修改代码；
2. 先修 WCS/测光/SNR，再生成正式银心 HISS；
3. 马赛克科学验证与浏览器底层优化可在接口冻结后并行；
4. 最终以真实数据结果和浏览器可视化闭环，而不是任务表完成率为准。

## 每任务输出

每个任务在 `evidence/<TASK_ID>/` 至少生成：

- `TASK_REPORT.md`
- `TEST_REPORT.md`
- `EVIDENCE_INDEX.md`
- `REVIEW_REPORT.md`
- 原始命令、超时、退出码、stdout/stderr 文件；
- 受影响文件 diff 或 commit；
- 机器可读报告（任务要求时）。

复核报告最后一行必须为 `VERDICT: PASS`、`VERDICT: FAIL` 或 `VERDICT: BLOCKED`。

## 实施范围

- 一次只允许一个主代码修改任务；调查、数据整理、性能采样和独立复核可并行。
- 修改接口、数据格式、CLI 事件或文件格式时，必须同步更新契约、测试、兼容性说明和回滚方案。
- Git 已配置：保留用户未提交修改，不强推，不重写历史。每个通过任务使用原子提交，默认不 push。

## 禁止“通过”方式

- 删除失败样本；
- 缩小 TestData 范围后仍称全量；
- 把警告改成成功；
- 自动降级后不在结果中声明；
- 用截图肉眼判断代替数值证据；
- 用人工调参只适配一张帧而不验证 T1–T4；
- 为浏览器预先生成低清图片替代科学数据浏览。


## P11-004 专项规则

- 先执行权威星对Gate v2，不得根据旧kd-tree p68直接改WCS；
- 不允许只使用IPV内部RMS，必须让最终Header/PipelineFrame WCS独立回投；
- 权威闭环通过时，`NO_CODE_CHANGE_REQUIRED` 是合法且优先的工程结论；
- blind catalog匹配失败应转入Photometric匹配诊断，不得自动归因于WCS；
- P11恢复完成后继续执行所有迁移进来的未完成v1.2任务。
