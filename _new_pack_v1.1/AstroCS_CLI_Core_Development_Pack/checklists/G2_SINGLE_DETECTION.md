# G2 PlateSolve 无退化与单次检测 Gate

- [ ] 全部 PlateSolve TestData 在候选运行前已冻结并计算 manifest hash
- [ ] 旧路径每案例至少重复运行 3 次，比较门限在候选运行前冻结
- [ ] 路径 A 已对全量 TestData 做逐例 A/B，没有事后排除样本
- [ ] 任一退化时已自动选择保守内部检测导出路径
- [ ] 最终路径已写入 ADR、capabilities、运行结果和 HISS provenance
- [ ] 选择路径 A 时全部 TestData 无成功率、WCS、匹配/RMS或稳定性退化
- [ ] 选择路径 B 时 PlateSolve 原始输入和求解数据路径保持不变
- [ ] 每帧 `sdet_detect_ex` 恰好一次
- [ ] PlateSolve 与 PSF 记录同一 `star_det` hash、count 和顺序
- [ ] PSF 使用 float32 API且无全图 uint16 缓冲
- [ ] 无消费者 `gaia_cat` 查询已删除
- [ ] 生产路径缺少选定 API 时严格失败，不静默切换另一条路径
