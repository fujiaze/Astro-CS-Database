# 当前任务

`P10-006`：T1-T4 真实校准代表帧验证。

## P10-005 已完成（2026-07-27）

- 3 个数据交付物：
  - LIGHT_TO_MASTER_RESOLUTION.csv（710 行，每行一个 Light 帧 + Bias/Dark/Flat 匹配结果 + 选择理由 + 歧义分类）
  - UNRESOLVED_CALIBRATION_REPORT.md（123 unresolved 详情，全部 missing_lum_flat）
  - RESOLUTION_SUMMARY.json（汇总统计）
- 2 个脚本交付物：
  - resolve_light_to_master.py（resolver 主脚本，460 行）
  - test_resolver.py（23 项单元/契约/e2e/forbidden 测试，499 行）
- 关键统计：
  - 710 Light 帧：587 resolved (82.7%) + 123 unresolved (17.3%)
  - Bias: 710 unique (100%)
  - Dark: 710 exact (100%)
  - Flat: 587 unique + 123 missing
  - 123 unresolved 全部为 T2/T4 Lum Light（T2 25 + T4 98），与 P10-002 设备档案 missing_flats=[Lum] 一致
- 0-resolved 根因诊断与修复：
  - 根因：P10-003 的 XISF header 解析器未提取 NAXIS1/NAXIS2（存于 Image 元素属性，非 FITSKeyword），导致 image_size_from_header 全空
  - 修复：resolver 添加统一 image_size 字段（header 优先，filename 兜底）
- 禁止捷径检查 PASS：
  - 无 first-match（多个匹配标记 ambiguous）
  - 无静默 fallback（123 unresolved 的 flat_master 字段为空字符串）
  - 无其他滤镜 flat 替代（T2/T4 缺 Lum flat 返回 missing_lum_flat）
  - Dark fallback 显式标记（fallback_longest + reason "fallback: all darks < Light exposure"）
- 23/23 测试 PASS
- 证据：engineering_v1.2/evidence/P10-005/

## 历史任务（已完成）

- P09-001：v1.1 基线冻结 + v1.2 开发包安装（4 件套）
- P09-002：INTERNAL_DETECTION_SHARED_EXPORT 命名统一（6/6 PASS）
- P09-003：canonical_dataset_v1.2 冻结（44 文件 SHA-256，7 测光失败帧，4 HCSD 基线）
- P10-001：TestData 目录盘点（3 设备 / 49 light 组 / 49+27 Header 采样）
- P10-002：T1-T4 设备档案建立（4 profile + summary，710 lights，76/76 PASS）
- P10-003：主校准帧盘点（27 文件 CSV + summary，20/20 PASS）
- P10-004：滤镜规范名与别名冻结（52 别名映射，23/23 PASS）
- P10-005：Light 到 Master 唯一解析（587/710 resolved，123 missing_lum_flat，23/23 PASS）

## 下一步：P10-006

依据 `tasks/P10-006.md`：

- T1-T4 真实校准代表帧验证
- 依赖：P10-005（已满足）
- 预期交付：待查阅 tasks/P10-006.md

## 已知 BLOCKED 项

- 123 Lum Light 帧（T2 25 + T4 98）缺 Lum Flat Master，需用户决策（提供 Lum Flat 或批准 flat-skip）。已在 P10-002 设备档案中记录。
