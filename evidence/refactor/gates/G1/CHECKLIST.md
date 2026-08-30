# G1 Checklist — 合同真相层

生成时间: 2026-08-30T15:46:30Z
commit: c7ec10032f7cf2773fdc200dddf1be43c519ffc5

| 项 | 状态 | 证据 |
|---|---|---|
| VERSION 唯一且目标为 0.10.0-alpha.1 | PASS | VER-001: VERSION=0.10.0-alpha.1; CMake file(READ VERSION); binary --version 验证 |
| SCI/ALG/DATA/ARCH/API/MOD/TEST ID 唯一 | PASS | DOC-001: INDEX.yaml 36 合同; CONTRACT_GRAPH_PASS; 无悬空引用 |
| Phase1/2 SCI 与冻结约束一致 | PASS | SCI-001/002: 6+4 份 FROZEN 文档确认; 公式/单位/不变量/Oracle 完整 |
| Phase3 状态是 prototype，正式 SCI 已设计 | PASS | SCI-003: SCI-P3-001 冻结(12 项); prototype 明确 NOT_IMPLEMENTED |
| 所有端口有 DATA 单位/坐标/invalid/ownership | PASS | DATA-001: 18 artifact schemas 含 unit/coordinate/invalid/ownership |
| 接缝加性模型与 integration weight 分离 | PASS | SCI-002 + DATA-001: UPM robust_control_weight vs stack weight 分离命名 |
| 测试容差事前冻结 | PASS | TEST-001: FP32 rtol=5e-6/FP64 1e-12/归约 γ_n/C≤4 事前冻结 |
| 无未登记科学冲突 | PASS | SCIENCE_OVERVIEW.md §5: 无未登记冲突 |