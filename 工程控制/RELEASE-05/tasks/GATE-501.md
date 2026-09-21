# 任务：GATE-501 门禁合理性审计与修复（B 线）

## 目标
门禁必须"有牙"：违规必红、指标真实、字段语义一致；修复 RELEASE-04 发现的恒真门与失效字段。

## 权威依据
最高设计 §9；ENGINEERING_SPEC §10；CONTRACT-501 性能门判据与监控字段语义。

## 改动范围
- 允许改：`eng/ci/**`、`docs/ci/**`
- 禁止改：lib/（发现代码缺陷登记派单 A 线）、科学测试断言口径（GATE-502 域）

## 步骤
1. **L2 性能门改真判红**：enforcement=record_and_justify 改为 fail-closed；四条冻结判据（平均利用率 ≥0.85、p50 ≥0.90、达标样本占比 ≥0.70、无连续 142s 低利用窗且无积压）任一违规 verdict=red；配"违规数据→红"的红绿自测（--self-test）；
2. **worker_balance 指标修复**：utilization_pct 恒 50.00% 的计算错误定位修复，按 CONTRACT-501 正确算法实现，合成负载验证非恒定；
3. **requires_monitor / mutates_workspace 语义落地**（run.py:109-119 等）：按合同二选一——真强制（声明即必须监控，缺失判红）或改名 monitor_capable（纯能力声明，不影响 verdict）；mutates_workspace 同理；8 个声明逐个核对执行语义；
4. **D-14 陈旧路径**：test_isa_variants.py:79、pack_audit_package.py:30 输出统一到 artifacts/evidence/prerelease-v5/，陈旧 artifacts/prerelease_v5 路径删除；
5. **fail-closed 普查**：全部门禁逐项验证"缺失证据/坏证据/无输出"三种情况均判红（当前重点：性能门、合同门、注册表门），出普查表；
6. **每门正负例**：检查器 --self-test 覆盖红/绿双向，无自测能力的检查器补齐；
7. CI 增量/并行化收尾（G2-9）：并行 runner 等价性证据、g++ 预编译、SECRET-HYGIENE 并发读、RESOURCE-GATE 合并窗口、构建图反查、指纹缓存；fast 档空载时间继续受控。

## 验收门
- [ ] L2 门对 RELEASE-04 归档的违规数据复跑判红（用历史证据回放）；
- [ ] 全门禁 fail-closed 普查表 100% 覆盖，每门有红绿自测；
- [ ] requires_monitor 8 声明语义一致；陈旧路径清零（grep）；
- [ ] fast 档全绿且不放宽任何判据；并行等价性 99/99 保持。

## 禁止
- 不通过删检查/放宽阈值求绿；判据变更必须有 CONTRACT-501 依据。
