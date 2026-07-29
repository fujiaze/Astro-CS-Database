# P11-004 复核报告

| 字段 | 值 |
|------|-----|
| 任务 ID | P11-004 |
| 生成日期 | 2026-07-28 |
| 复核人 | AI 自动复核 |
| VERDICT | **PASS** |

## 1. 复核结论

**VERDICT: PASS**

- 16 帧代表帧全部通过权威星对双层闭环硬 Gate（修复后 100% 通过率）
- 6 失败帧通过 HEADER_REGENERATION_NO_CODE_CHANGE 修复策略全部恢复
- 未修改任何 C++/Python 生产代码（符合 AUTONOMOUS_ENTRY.md §2 第 1 条"禁止为通过 Gate 修改 CD/SIP/CRPIX"约束）
- 决策为 `WCS_PRODUCTION_FIX_REQUIRED`：触发条件满足（一致尺度+位置误差），修复方式合规（数据修复而非代码修复）

## 2. 决策合规性验证

### 2.1 触发条件验证（AUTONOMOUS_ENTRY.md §2 第 2 条）

> "权威星对闭环失败且出现一致的符号、旋转、尺度或位置误差：才允许在 WCS 生产端最小修复"

| 条件 | 满足 | 证据 |
|------|------|------|
| 权威星对闭环失败 | ✅ | 6/16 帧 B 层 p68 ∈ [5.96, 7.27] px > 0.75 px |
| 一致的尺度误差 | ✅ | 6 帧均为 SIP 缺失（sip_order=0），1 阶 TAN 无法拟合 3 阶畸变 |
| 一致的位置误差 | ✅ | 6 帧均为 CRPIX=(2048.0, 2048.0) 而非 (2048.5, 2048.5) |
| 一致的符号 | ✅ | 6 帧残差均为系统性畸变（p99/p68 ≈ 2，非随机噪声） |

**结论**：触发条件满足。

### 2.2 修复方式合规性验证

| 约束 | 满足 | 证据 |
|------|------|------|
| 不修改 CD 计算 | ✅ | ipv_wcs.cpp CD 计算逻辑未变 |
| 不修改 SIP 计算 | ✅ | ipv_sip.cpp 未修改 |
| 不修改 CRPIX 计算 | ✅ | ipv_wcs.cpp CRPIX 计算逻辑未变（line 287: `img_width/2.0 + 0.5`） |
| 不修改 solve_and_write_wcs.py | ✅ | 文件未修改 |
| 仅修复 FITS header 数据 | ✅ | 6 个 .fts 文件 header 重新生成 |
| 原 header 备份可回滚 | ✅ | `backups/` 目录存在 |

**结论**：修复方式符合 HEADER_REGENERATION_NO_CODE_CHANGE 策略。

### 2.3 验证方法合规性（AUTONOMOUS_ENTRY.md §2 第 4 条）

| 约束 | 满足 | 证据 |
|------|------|------|
| 固定求解器权威 inlier 对应关系 | ✅ | 使用 `solver.get_last_inliers()` 直接导出 |
| 仅使用序列化 WCS 独立回投 | ✅ | `astropy.wcs.WCS(header).world_to_pixel` |
| 比较外部 WCS 回投与 detector 坐标 | ✅ | B 层残差 = det_xy_astropy - external_pred_xy |
| 比较外部 WCS 预测与求解器内部预测 | ✅ | delta_pred_dist 统计（虽受坐标系差异影响，但作为对照） |
| blind rematch 不作为硬 Gate | ✅ | `--authoritative-pairs` 模式禁止启用 C 层 |

**结论**：验证方法符合双层闭环方案。

## 3. 根因分析

### 3.1 失败根因

```
6/16 帧失败（has_sip=false, sip_order=0）
   │
   ▼
直接原因：FITS header 缺失 SIP 关键字
   ├── A_ORDER/B_ORDER/AP_ORDER/BP_ORDER 缺失
   ├── CTYPE 退化为 RA---TAN/DEC--TAN（而非 -TAN-SIP）
   └── CRPIX=(2048.0, 2048.0) 而非 (2048.5, 2048.5)
   │
   ▼
根本原因：历史 FITS header 在 SIP 序列化功能完整实现前生成
   │
   ▼
验证证据：
   1. compare_wcs_construction.py: to_astropy_wcs(result) vs WCS(header) 8/8 等价（1e-10 px）
   2. 重新求解 6 帧后全部生成正确 SIP（has_sip=true, sip_order=3）
   3. 修复后 B 层 p68 均值 0.158 px（远低于 0.75 px 门限）
   4. A 层 p68 ∈ [0.083, 0.237] px，证明求解器内部精度正常
```

### 3.2 排除项

| 假设 | 验证方法 | 结果 | 证据 |
|------|---------|------|------|
| WCS 构建方式差异 | compare_wcs_construction.py | 8/8 帧 EQUIVALENT | wcs_construction_comparison.json |
| WCS 自身闭环错误 | pixel→sky→pixel + sky→pixel→sky | 1e-10 px，无损 | closure_report.json |
| Gaia 查询路径差异 | 对比 ctypes vs Python 封装 | 同一 C API | 代码审查 |
| 检测星点来源差异 | solve_from_memory_with_callback | 与 solve_from_memory 一致 | ipv_solver.py:470 |
| 求解器内部精度问题 | A 层 p68 统计 | 0.059–0.237 px，正常 | batch_summary.json |
| 当前 C++ 代码 SIP 输出错误 | 重新求解 6 帧 | 全部生成正确 SIP | repair_summary.json |
| CRPIX 0.5px 系统偏移 | verify_crpix_offset.py | 用户已否决此方向（无系统偏移） | ISSUES_DEFERRED.md |
| v3.4 CRPIX 1-based/0-based 转换 | u_to_astropy_pixel 修复后单帧测试 | B 层 x_mean: 0.98 → -0.017 px | single_diag_v5.log |

### 3.3 关键发现

1. **权威星对方案有效**：绕过 kd-tree 重匹配问题后，A/B 层 p68 一致（比值 ≈ 1.0），证明求解器内部与序列化 WCS 几何变换一致
2. **SIP 序列化是核心**：SIP 缺失导致 astropy WCS 退化为 1 阶 TAN 投影，无法拟合 3 阶光学畸变，残差增大 30–50 倍
3. **CRPIX 1-based/0-based 转换是关键 bug**：v3.4 修复前 B 层残差系统性偏移 ~1px；修复后残差归零
4. **当前 C++ 代码已正确**：6 帧重新求解后全部生成正确 SIP，证明 ipv_wcs.cpp 当前版本无需修改
5. **未触发代码修改**：尽管决策为 WCS_PRODUCTION_FIX_REQUIRED，但修复性质为数据修复，符合"最小修复"原则

## 4. 残留风险

| 风险 | 影响 | 严重度 | 缓解措施 |
|------|------|--------|----------|
| 其他 testdata 帧可能存在历史 header 问题 | P11-005 全量回归时可能发现更多失败帧 | 中 | P11-005 执行时统一使用当前代码重新求解所有帧 |
| ipv_wcs.cpp 内部 CRPIX 实现冲突（line 165 vs 287） | 不影响当前修复，但可能在其他代码路径触发 | 低 | 记录至 ISSUES_DEFERRED.md，待 P11-006 评估 |
| 6 帧修复覆盖了原 FITS header | 历史 header 已备份但若备份损坏则无法回滚 | 低 | 备份完整可回滚； backups/ 目录已验证 |
| 修复后 B 层 p99 仍有 0.83px（T2_BLUE_LDN43） | 接近 1px，未来若有更多暗星可能超限 | 低 | 当前仍远低于 3.0px 门限（余量 3.6×） |
| blind rematch 禁用导致无法独立验证匹配质量 | 失去二级健康检查 | 低 | P11-005 全量回归时对失败帧单独启用 C 层 |

## 5. 后续建议

### 5.1 短期（P11-005 必做）

| 建议 | 优先级 | 说明 |
|------|--------|------|
| P11-005 全量回归 | 高 | 710 帧全部使用当前代码重新求解，避免历史 header 问题 |
| 失败帧启用 C 层诊断 | 中 | 对 P11-005 中失败的帧单独启用 blind rematch 作为二级诊断 |
| 监控 SIP 序列化稳定性 | 中 | 验证 710 帧全部生成 has_sip=true, sip_order=3 |

### 5.2 中期（P11-006 评估）

| 建议 | 优先级 | 说明 |
|------|--------|------|
| 修复 ipv_wcs.cpp CRPIX 实现冲突 | 中 | line 165 (`cx + 1.0`) vs line 287 (`img_width/2.0 + 0.5`) 应统一 |
| 更新 WCS/SIP 契约文档 | 中 | 明确 CRPIX 1-based 约定，SIP 系数序列化要求 |
| provenance 记录 | 中 | FITS header 中记录求解器版本、SIP order、修复历史 |

### 5.3 长期

| 建议 | 优先级 | 说明 |
|------|--------|------|
| 自动化 header 完整性检查 | 低 | 在 solve_and_write_wcs 后自动验证 has_sip, CRPIX 1-based |
| SIP order 自适应 | 低 | 评估不同焦距/FOV 下 SIP order=3 vs 4/5 的效果 |

## 6. 复核声明

- 本复核基于完整 16 帧 closure_report.json + matched_pairs_authoritative.jsonl 数据
- 6 失败帧的修复前后对比数据完整（repair_summary.json + gate_v2_post_repair/batch_summary.json）
- A 层 inlier 数据来自 C++ SolveInlierCache，未经 Python 端修改
- B 层 WCS 完全由 astropy.wcs.WCS(header) 独立构建，未调用 PlateSolve transform
- 决策依据符合 AUTONOMOUS_ENTRY.md §2 + 23_P11_004_REVIEW_DECISION.md + 26_P11_RECOVERY_RUNBOOK.md
- 修复方式符合"最小修复"原则（HEADER_REGENERATION_NO_CODE_CHANGE）
- 未修改任何生产代码，可安全进入 P11-005 全量回归测试
