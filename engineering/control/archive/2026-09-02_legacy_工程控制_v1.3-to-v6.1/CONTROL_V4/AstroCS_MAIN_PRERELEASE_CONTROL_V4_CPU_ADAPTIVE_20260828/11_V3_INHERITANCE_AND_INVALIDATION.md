# V3 成果继承与失效表

## 允许继承代码，不继承 PASS

- 根构建入口、Windows可移植修复；
- worker options、部分OpenMP实现；
- 有界队列类和测试；
- 文档/API/checker框架；
- 合成 Oracle 框架；
- Fatduck运行脚本。

以上都必须在当前 main 的 V4 对应 Task 中复验。

## 直接失效

- CON-006 PASS：报告自认部分实现；
- CON-008 PASS：生产未接线且全局mutex串行；
- CON-009 PASS：后续出现真实sampler崩溃和UPM race；
- CON-010豁免：利用率门禁失败不能豁免；
- CHK-006 PASS：科学单位 mutation 漏报；
- SCI-001/SCI-004完成声明：Drizzle物理量仍二选一；
- WIN-002 PASS：存在 ignored build error和测试失败；
- RUN-001..006及A/B/C所有历史运行；
- 历史接缝阈值和历史输出数值 Oracle。

## V4 当前发布阻塞项

- 重计算低利用率；
- AIO/cfitsio安全重叠管线；
- UPM/sampler竞态；
- integration内存布局；
- 所有输出层缺口；
-科学合同的值级检查；
- Windows clean build/test零失败；
- 当前候选32R和HiPS。

