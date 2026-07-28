# 当前任务

`P11-005`：PlateSolve 710 全量回归与权威星对 WCS Gate（v1.3 修复版，紧接 P11-004 完成）。

## 状态
- 上一任务：P11-004 已 DONE（2026-07-28，WCS_PRODUCTION_FIX_REQUIRED，HEADER_REGENERATION_NO_CODE_CHANGE）
- 当前 Git HEAD：93ee025（待提交 P11-004 完成证据）
- 迁移来源：v1.2（P09-001~P11-003 全部 DONE，P11-004 已恢复并完成）

## P11-005 任务范围（按 AUTONOMOUS_ENTRY.md §2 第 5 条 + 26_P11_RECOVERY_RUNBOOK.md §8）

1. 对 710 帧 PlateSolve 全量回归测试
2. 对所有成功帧或分层抽样执行权威星对 WCS Gate（A/B 双层闭环）
3. 统计成功率、RMS 分布、失败帧根因
4. 对失败帧启用 C 层 blind kd-tree rematch 作为二级诊断
5. 输出汇总报告

## 依赖
- P11-004 已完成：诊断工具 `wcs_closure_diagnostic_v3.py` v3.4 已就绪
- P11-004 已完成：修复脚本 `repair_failed_frames.py` v3.5 已就绪（可用于批量修复历史 header 问题）

## 历史
- v1.2 P11-004：诊断工具 kd-tree 重新匹配残差与 IPV 内部 RMS 不可比 → DEFERRED
- v1.3 P11-004：按双层闭环方案重新验证，6 帧失败因 SIP 未序列化 → HEADER_REGENERATION_NO_CODE_CHANGE 修复 → 16/16 通过
- v1.3 P11-005：710 帧全量回归测试
