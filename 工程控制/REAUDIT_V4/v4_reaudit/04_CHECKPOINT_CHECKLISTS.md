# V4 检查点清单（C0–C9）

每个检查点打包核验：按本清单逐项勾验，生成 `CHECKPOINT_RESULTS.csv`（模板）并附证据索引；
未全过的检查点不得跨关。

## C0 控制与起点
- [ ] 控制包 validate_control.py CONTROL_PASS（98 行、C0–C9 完整）
- [ ] origin/main 起点 SHA 冻结并记录（C0-002）
- [ ] 双平台工具链盘点（C0-003）
- [ ] V3 继承债务五项登记（C0-004）

## C1 Linux 静态分析
- [ ] 六组机器检查器 0 FAIL（C1-001..005）
- [ ] 硬编码扫描清单（核心/worker/block/AVX）输出（C1-006）
- [ ] V3 TSan race 债务分类清单（C1-007）
- [ ] C1 快照（C1-008）

## C2 SCI
- [ ] Phase3 SCI 五项定义（输入输出/覆盖/核与通量/NaN/精度）（C2-001..005）
- [ ] Phase1/2 复核冻结记录（C2-006/007）
- [ ] C2 快照（C2-008）

## C3 ALG
- [ ] Phase3 几何/WCS/映射/插值/边界推导（C3-001..005）
- [ ] backend 选择/动态线程/benchmark/资源门禁算法（C3-006..009）
- [ ] SCI-ALG-ARCH 交叉引用自洽（C3-010）

## C4 ARCH/API
- [ ] 单一 CLI 合同（Win/Linux）（C4-001）
- [ ] 进程内架构 + DLL 退役方案（C4-002/003）
- [ ] backend/profile/监控/门禁 API（C4-004..008）
- [ ] Phase3 API 与数据合同（C4-009/010）
- [ ] 错误/退出码合同（C4-011）；C4 快照（C4-012）

## C5 CODE
- [ ] CLI/进程内/Phase3 骨架/backend/baseline 实现（C5-001..005）
- [ ] 动态线程 + 硬编码清零 + AVX 治理（C5-006..008）
- [ ] Phase3 五项核心实现 + FITS 写出（C5-009..013）
- [ ] 监控/门禁/benchmark 实现（C5-014..016）
- [ ] upm race 修复 + TSan 回归（C5-017/018）
- [ ] 串行段复核清零（C5-019）；alpha 脚本 + Windows 适配（C5-020/021）；快照（C5-022）

## C6 测试与 Oracle
- [ ] 独立合成 Oracle + 对拍阈值判定（C6-001/002）
- [ ] CLI/进程内/backend/线程/benchmark/监控/门禁测试（C6-003..009）
- [ ] Phase3 端到端 + 全链合成（C6-010/011）
- [ ] 既有套件回归 + TSan 全门禁干净（C6-012/013）
- [ ] C6 快照（C6-014）

## C7 Linux 构建 alpha
- [ ] build 0 错 0 警（C7-001）；alpha 包 + SHA/manifest（C7-002/003）
- [ ] Linux 全验证汇总（C7-004）；review capsule（C7-005）；C7 包（C7-006）

## C8 Windows 正式验证（Fatduck 在线）
- [ ] Windows 构建 exit 0（C8-001）；全测试 FAIL=0（C8-002）
- [ ] full benchmark + 资源监控全开（C8-003）
- [ ] 合成验证（C8-004）；2R 小真实数据（C8-005）
- [ ] 资源门禁判定（C8-006）；hash 登记（C8-007）；C8 包（C8-008）

## C9 32R 与发布候选
- [ ] 银心 32R 当前候选唯一一次 + 资源门禁（C9-001/002）
- [ ] 发布审核包（C9-003）；终态冻结（C9-004）
- [ ] 输出 AWAITING_EXTERNAL_RELEASE_REVIEW（C9-005，唯一允许终态）
