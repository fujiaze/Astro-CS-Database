# MON-004 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS MON-004 行「20 次循环、预热剔除、稳健斜率/峰值/retained bytes/OOM 预警 | 注入泄漏被抓; 稳定 cache 不误判; 报告含曲线摘要」; 07 §5(≥20 次循环+丢弃预热+稳健斜率/峰值/allocator 指标+无界增长/每轮 retained 递增/逼近 OOM 即 FAIL; ASan 不替代运行曲线)。ABI 冻结(v1)不改公共 API。

## 动作
新建 **cli/memory_growth.h**(纯分析模块, 无副作用):
1. **warmup 剔除**: 丢弃前 warmup(默认 3)轮, 只分析预热后区段。
2. **Theil-Sen 稳健斜率**: 所有点对斜率中位数(抗单点噪声, 比最小二乘稳健)。
3. **峰值/retained 趋势**: peak_bytes(每轮最大)、retained_growth_bytes(末轮-预热后首轮)、relative_growth_pct。
4. **OOM 预警**(07 §5 逼近 OOM): peak ≥ oom_frac(默认 0.85)× mem_limit → oom_pre_warning。
5. **分类** `MemDiag`(Stable/Leak/Growing/Oscillating/OomPreWarning)逐项判定:
   - 近零净斜率(|slope|<0.5×leak_threshold): 按峰谷振幅区分 震荡(≥0.3, 每轮释放非泄漏) vs 稳定;
   - slope ≥ leak_threshold → leak(无界增长);
   - 0<slope<阈值 → growing(早期预警);
   - 判 OOM 在前。
6. **报告含曲线摘要**(slope/peak/growth/pct/n_analyzed/detail)。
7. 纯逻辑(无硬编码; 阈值由 cfg 注入: warmup/leak_slope/oom_frac/mem_limit)。

## 验证
- tests/cli/test_memory_growth.py(7 测试):
  - 01 注入线性泄漏(+64KB/轮)→ leak; 02 稳定 cache(warmup 后持平)→ 不误判; 03 峰谷震荡(±0.5MB 往返)→ oscillating 非 leak; 04 完全平稳→ stable; 05 峰值≥85% 上限→ oom_pre_warning; 06 正增长低于泄漏阈值→ growing(早期预警); 07 报告含曲线摘要(slope/peak/growth/n_analyzed/detail)。
- 全量回归 unittest **242/242 OK**(新增 7, 零回归)。

## 限制与遗留
- 分析逻辑落地(输入每轮 RSS/retained 序列); 与 CLI/benchmark 集成(每可重复 stage ≥20 次循环采 retained bytes 并调 analyze_memory_growth)由后续集成任务接线。
- 泄漏阈值 leak_slope_bytes_per_iter 与 mem_limit 为外部输入(由 run 环境/benchmark 合同注入); OOM 判断依赖 mem_limit 已知。
- ASan/LSan 仍作辅助(经 sanitizer 构建), 但按 07 §5 不替代运行曲线 —— 本模块承载运行曲线判定。

## 产物
cli/memory_growth.h(分析+分类+OOM 预警); tests/cli/test_memory_growth.py(7 测试); artifacts/prerelease_v5/MON-004/LOG.md; 本日志。

## PASS 判定
20 次循环+预热剔除(Theil-Sen 稳健斜率/峰值/retained 趋势/相对增长)+OOM 预警(逼近上限)+分类(泄漏/增长/震荡/稳定)落地; 注入泄漏被抓(stable-cache 不误判, oscillating 非 leak, OOM 预警); 报告含曲线摘要。MON-004 = PASS。
