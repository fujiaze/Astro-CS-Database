# Fix Review Evidence Checklist

- [ ] `git status --porcelain`为空
- [ ] result/source/evidence/log tip为同一完整SHA
- [ ] path guard原始命令、exit code和完整输出已保存
- [ ] 每个命令记录明确timeout
- [ ] 无TIMEOUT被计为PASS
- [ ] GPU/Mixed日志包含真实device id和完成工作量
- [ ] 资源控制报告包含连续原始采样、平均、P95和动作
- [ ] Sanitizer构建日志证明编译与运行时实际启用
- [ ] UTF-8 SHA-256验证全部通过
