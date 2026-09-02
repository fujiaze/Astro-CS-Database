# 当前任务

`P11-004`：在 WCS 生产端实施统一修正 — **状态：DEFERRED（审核包已导出，等待审核反馈）**

## P11-004 状态（2026-07-28，DEFERRED）

- **审核包**：`P11-004_review_bundle.zip`（38.12 MB，29 个条目，位于项目根目录）
  - 含 P11-004 evidence + 调试 PNG 样本（T2_RED_LDN43 + T3_LUM_NGC55 各 1 张）+ 前置任务上下文 + 核心库代码副本 + README
  - 不 commit（.gitignore 已加 `*_review_bundle.zip`）
- **核心矛盾**：诊断工具 kd-tree 重新匹配残差（p68≈1.0px）与 IPV 内部 RMS（0.12px）不可比
  - 匹配策略不同：IPV 用 RANSAC 选 inliers（24-34 对）vs 诊断工具用全星等 kd-tree（1942 对，含暗星误配+饱和星偏差）
- **已排除根因**：WCS 构建（等价）、WCS 闭环（正确 1e-10 px）、Gaia 查询（同一 API）、检测星点来源（callback 与 IPV 一致）、CRPIX 偏移（用户否决）
- **遗留问题详情**：`engineering_v1.2/evidence/P11-004/ISSUES_DEFERRED.md`
- **备选方案（待审核决策）**：
  - A. 用 IPV RMS 作 gate（绕过诊断工具 kd-tree 匹配）
  - B. 严格匹配+剔除（复刻 IPV RANSAC inlier 选择策略到诊断工具）
  - C. 视觉验证（visualize_reproject 投影位置准确，作为定性证据）
  - D. 查质心坐标系

## P11-003 已完成（2026-07-28）

- 任务目标：在 T1-T4 代表帧复现 WCS 闭环缺陷，量化 X/Y 偏差、象限、SIP 阶数
- 工具：P11-003 v1.0（subset driver + astropy WCS 独立诊断）
- 执行范围：16 帧代表帧
  - T2: 5 帧（LDN43 ×4 + NGC1727 ×1）
  - T3: 6 帧（NGC55 ×6）
  - T4: 5 帧（Galaxy_Center ×5）
- 执行方式：并行 Subagent（Group A + Group B 各 4 帧）+ 3 帧重跑（NTFS 压缩损坏恢复）
- 关键结果：

| 维度 | 结果 |
|------|------|
| 求解成功率 | 16/16 = 100% |
| 诊断成功率 | 16/16 = 100% |
| gate 通过率 | 8/16 = 50% |
| median solve RMS | 0.1146 px |
| median 残差中位数 | 0.736 px |

- 跨帧模式（根因结论）：

| 因素 | 模式 |
|------|------|
| 焦距/FOV（主导） | T4 (200mm 大 FOV) 100% 通过；T2/T3 (1900mm 小 FOV) 27% 通过 |
| 滤镜类型（次要） | 窄带 HA/OIII 66.7% 通过；宽带 RGB/LUM 10% 通过 |
| Y 方向系统偏差 | 15/16 帧 Y 残差 > X 残差（93.75%） |
| SIP 写入/解析损失 | 已排除（PS↔Sky 闭环 1e-10 px） |

- VERDICT: PARTIAL
  - WCS 闭环实现正确（数值闭环精度 1e-10 px，非代码缺陷）
  - gate 失败根因：SIP order=3 在小 FOV 下建模不足 + Y 方向系统偏差 + 宽带暗星质心精度
- 证据：`engineering_v1.2/evidence/P11-003/`
  - 4 份标准报告：TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md
  - 16 帧 closure_report.json + matched_pairs.json + residual_plot.png + quadrant_plot.png
  - p11_003_summary.json（16 帧汇总 + 跨帧模式 + 根因结论）
  - group_a_summary.json + group_b_summary.json + rerun_corrupted_summary.json
  - REPRESENTATIVE_FRAMES_ARCHIVE.json（16 帧设备档案 + 执行状态）
  - raw_logs/（执行日志）

## 历史任务（已完成）

- P09-001：v1.1 基线冻结 + v1.2 开发包安装（4 件套）
- P09-002：INTERNAL_DETECTION_SHARED_EXPORT 命名统一（6/6 PASS）
- P09-003：canonical_dataset_v1.2 冻结（44 文件 SHA-256，7 测光失败帧，4 HCSD 基线）
- P10-001：TestData 目录盘点（3 设备 / 49 light 组 / 49+27 Header 采样）
- P10-002：T1-T4 设备档案建立（4 profile + summary，710 lights，76/76 PASS）
- P10-003：主校准帧盘点（27 文件 CSV + summary，20/20 PASS）
- P10-004：滤镜规范名与别名冻结（52 别名映射，23/23 PASS）
- P10-005：Light 到 Master 唯一解析（587/710 resolved，123 missing_lum_flat，23/23 PASS）
- P10-006：T1-T4 真实校准代表帧验证（16/16 PASS，25/25 测试 PASS）
- P11-001：坐标约定冻结（COORDINATE_CONVENTION.md，7 坐标系 + 22 变量，19/19 PASS）
- P11-002：WCS 真实星对闭环诊断工具建立（30/30 测试 PASS，2 帧代表帧验证，VERDICT: PASS）
- P11-003：T1-T4 代表帧 WCS 闭环缺陷复现（16 帧，8/16 gate 通过，VERDICT: PARTIAL）

## 下一步：等待 P11-004 审核反馈

P11-004 当前为 DEFERRED 状态。审核包已导出至 `P11-004_review_bundle.zip`（项目根目录，不 commit）。

待用户/审核者研究审核包后，从备选方案 A/B/C/D 中决策，再继续后续工作：
- 若选 A（用 IPV RMS 作 gate）→ 修改 P11-005 全量回归的 gate 标准
- 若选 B（严格匹配+剔除）→ 修改诊断工具匹配策略，重跑 P11-003 16 帧
- 若选 C（视觉验证）→ 视为定性证据，继续 P11-005 全量回归
- 若选 D（查质心坐标系）→ 深入排查质心算法坐标系约定

后续任务（待 P11-004 决策后启动）：
- P11-005：PlateSolve 710 全量回归与 WCS 闭环 Gate
- P11-006：更新 WCS/SIP 契约和 provenance

## 已知 BLOCKED 项

- 123 Lum Light 帧（T2 25 + T4 98）缺 Lum Flat Master，需用户决策（提供 Lum Flat 或批准 flat-skip）。已在 P10-002 设备档案中记录。
