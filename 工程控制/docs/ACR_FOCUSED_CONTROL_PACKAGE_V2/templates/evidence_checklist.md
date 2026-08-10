# Focused ACR Evidence检查表

- [ ] 实现HEAD、源码快照HEAD、Evidence HEAD和git log tip一致
- [ ] git status clean
- [ ] 控制包SHA与仓库控制目录一致
- [ ] path guard PASS
- [ ] CPU-only/CUDA构建命令、timeout和exit code
- [ ] standard focused benchmark原始逐点数据
- [ ] CPU、GPU resident、GPU host roundtrip曲线
- [ ] 候选块测试与选择理由
- [ ] holdout真实误差
- [ ] 无交叉点路径写eligible=false和null阈值
- [ ] OperationProfile schema与完整roundtrip
- [ ] loaded Profile下small task Auto退化
- [ ] dense/reduce/drizzle CPU/GPU/ForcedMixed/Auto正确性
- [ ] 实际H2D/D2H次数和字节
- [ ] resident chain真实device buffer复用
- [ ] RAM/pinned/每GPU VRAM reservation与故障路径
- [ ] CPU sanitizer和compute-sanitizer原始日志
- [ ] 0 failed、0 timeout，SKIPPED准确
