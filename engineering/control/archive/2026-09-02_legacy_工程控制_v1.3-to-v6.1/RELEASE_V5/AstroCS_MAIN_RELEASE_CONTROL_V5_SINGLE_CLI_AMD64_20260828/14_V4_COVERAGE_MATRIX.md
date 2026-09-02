# V4 内容继承覆盖矩阵

V5 必须包含 V4 全部有效要求；本表防止架构改版遗漏。

| V4 主题 | V5 承接位置 | 结论 |
|---|---|---|
| current-main、无历史 A/B/C | 00、08、12、BASE-001 | 完整继承 |
| 纯 CPU、ACR dormant | 00、01、ARCH/ISO Tasks | 完整继承 |
| CPU topology/ISA/autotune | 05、06、BENCH/ISA Tasks | 扩展为私有 DLL/SO 与逐 kernel profile |
| 禁止硬编码核数/AVX | 00、05、06、machine checker | 完整继承 |
| 重计算资源监控 | 07、MON Tasks | 扩展为 CLI 内置强制门禁 |
| 内存增长/泄漏曲线 | 07、MON-004、SYN/LNX/WIN | 完整继承 |
| AIO/sampler/UPM/Drizzle 并发风险 | 12、PAR Tasks | 完整继承 |
| 文档六层一致 | 08、SCI/ALG/ARCH/API/DOCCHK | 完整继承并增加 Phase3 |
| 文档导出机器检查 | 04、08、DOCCHK Tasks | 完整继承 |
| 每算法合成数据/独立 Oracle | 08、SYN Tasks | 完整继承，不跑历史版本 |
| Linux 低资源控制节点 | 09、LNX Tasks | 完整继承 |
| Fatduck Windows 重计算 | 09、WIN Tasks | 完整继承 |
| Fatduck 离线不停工 | 00、09、ledger dependency | 完整继承 |
| Windows 当前候选 32R 一次 | 09、WIN-007 | 完整继承 |
| HiPS/接缝审核 | 09、SYN-008、WIN-008 | 完整继承 |
| 原子 main commit/push | 10、全 ledger | 完整继承 |
| 每 commit review capsule | 10、REV Tasks | 完整继承 |
| 外部审核科学文档与代码抽样 | 08、10、REV Tasks | 完整继承 |
| 白名单审核包/大文件引用 | 10、package/validate scripts | 完整继承 |
| 无频繁外部停止 | 00、REV 异步规则 | 完整继承 |
| V3 历史问题逐项重验 | 12、PAR/DOCCHK/WIN Tasks | 完整继承 |

V5 新增：alpha 版本合同、单一 CLI 产品边界、稳定 JSONL/退出码、CPU backend C ABI、安全加载、Windows/Linux 双发布布局、Phase3 HiPS→平面 FITS。

