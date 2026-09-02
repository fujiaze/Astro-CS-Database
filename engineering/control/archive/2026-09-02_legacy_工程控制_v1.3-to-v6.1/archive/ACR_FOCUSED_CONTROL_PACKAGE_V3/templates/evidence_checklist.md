# ACR聚焦版 Evidence 检查表

- [ ] Evidence在仓库外生成
- [ ] 源码HEAD、evidence/head.txt、git log tip一致
- [ ] 工作区干净，path guard PASS
- [ ] 原始Benchmark尺寸/重复/候选块记录齐全
- [ ] Profile Schema与内部validator均PASS
- [ ] 成本单位回归与交叉点实测验证PASS
- [ ] qualified Operation的Auto路由结果与传输计数已记录
- [ ] Dispatcher真实resident：一次upload、多token、一次materialize
- [ ] CPU/GPU/Auto/ForcedMixed正确性结果齐全
- [ ] partial容量、retry清零、merge测试齐全
- [ ] RAM/VRAM/staging动作证据齐全
- [ ] compute-sanitizer与可用CPU sanitizer日志齐全
- [ ] 0 failed、0 timeout；SKIPPED原因明确
- [ ] SHA清单可在解压后完整复核
