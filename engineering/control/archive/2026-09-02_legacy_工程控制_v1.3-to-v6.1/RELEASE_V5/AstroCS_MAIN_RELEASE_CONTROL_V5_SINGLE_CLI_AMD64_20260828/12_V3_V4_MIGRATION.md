# V3/V4 继承、失效与当前已知风险

## 1. 允许继承

已在 `main` 的合理代码可保留，不要求回滚。每项必须由 V5 当前 SHA 的静态/合成/资源证据重新验证。只继承实现，不继承 PASS。

## 2. 明确失效

- 历史版本 A/B/C 对比任务；
- 重跑旧 32R；
- 用历史性能 baseline 判定当前路径；
- ACR/GPU 接线；
- CON-010 一类失败后 waiver；
- CP0/CP1 等频繁外部停工；
- 把局部 checker PASS 当作科学文档一致；
- 将大日志、pixel CSV、testdata 或 build 打进审核包。

## 3. 必须进入 FINDINGS 的已知风险

1. Phase2 历史全量运行约只使用 0.7--0.9 个等效核心；此前性能门禁被豁免。
2. AIO 曾有全局 mutex/生产队列未接线风险。
3. sampler/UPM 曾出现 sampler crash、竞态或报告互相矛盾。
4. science unit mutation 曾未被 checker 捕获；Drizzle 单位表述不冻结。
5. Windows 历史报告曾忽略 Error 1/测试失败却标 PASS。
6. 接缝问题曾解决后因大范围梳理退化。

这些不是当前失败结论，但必须由对应 V5 Task 用当前代码重验并关闭；不得删除或仅写“历史已修”。

