# 证据索引 — P11-001

- Task：P11-001 冻结内部/图像/FITS/WCS坐标约定
- Date：2026-07-27
- Reviewer：独立复核 Agent

## 交付物清单

| # | 文件 | 类型 | 大小 | 用途 |
|---|------|------|------|------|
| 1 | COORDINATE_CONVENTION.md | 冻结文档 | 13229 bytes | 坐标约定冻结主文档（10 章 + 附录） |
| 2 | scripts/verify_convention.py | 验证脚本 | ~9KB | 19 项约定验证测试 |
| 3 | TASK_REPORT.md | 任务报告 | ~6KB | 任务执行详情与结果 |
| 4 | TEST_REPORT.md | 测试报告 | ~4KB | 19 项测试矩阵与详情 |
| 5 | EVIDENCE_INDEX.md | 证据索引 | 本文件 | 交付物索引 |
| 6 | REVIEW_REPORT.md | 复核报告 | ~3KB | 独立复核结论 |

## 关键证据

### 1. 坐标约定冻结（COORDINATE_CONVENTION.md）

- **7 个坐标系统**：S1 图像数组 / S2 内部 U / S3 FITS WCS / S4 切平面 / S5 天球 / S6 HEALPix / S7 浏览器笛卡尔
- **7 个关键转换函数**：S1→S2 / S2→S4 / S5→S4 / S4→S5 / S2→S3 / S3→S5 / S5→S3
- **22 个冻结变量**：cx/cy/CRPIX/CD/SIP 索引/SIP 符号/CRVAL 不变/shape/has_wcs/pixel_scale/rotation_deg 等
- **Y 轴反转链**：输入侧（det→U）+ 输出侧（U→FITS WCS）
- **四模块一致性**：PlateSolve/Photometric/SNR/Drizzle 全部 CRPIX 1-based + 像素 0-based + 度 + TAN+SIP + 不显式乘 cos(Dec)
- **禁止事项**：不得先改符号 / 不得只在 Photometric 补偿 / 不得用旧路径替代闭环验证
- **变更控制**：任何坐标变更必须经 ADR + P11-002~P11-005 闭环验证

### 2. 验证脚本（verify_convention.py）

19 项验证测试，覆盖：
- Contract（7）：S2 中心点/Y 反转 + S3 CRPIX/CD/Y-flip/SIP 符号/CRVAL 不变
- Unit（6）：astro_image_io shape/CRPIX/has_wcs + Photometric CRPIX/no-cosdec + Drizzle CRPIX
- Consistency（4）：接口注释存在（itertrans/sip/api/wcs_transform）
- Forbidden Shortcut（1）：git diff 确认无代码修改
- Deliverable（1）：COORDINATE_CONVENTION.md 存在

### 3. 测试结果

19/19 PASS（100%）。

## 依赖关系

- **上游**：P09-002（共享检测主线与统一命名，已 DONE）
- **下游**：
  - P11-002（标准 WCS 真实星对闭环诊断工具）— 基于本冻结约定构建诊断
  - P11-003（T1-T4 代表帧复现 WCS 闭环缺陷）— 基于本冻结约定验证
  - P11-004（WCS 生产端统一修正）— 基于本冻结约定修正（若发现缺陷）
  - P11-005（PlateSolve 710 全量回归）— 基于本冻结约定回归

## 引用文档

- `engineering_v1.2/docs/05_WCS_COORDINATE_CONVENTION_AND_CLOSURE_SPEC.md`（WCS 坐标约定与闭合性规范）
- `engineering_v1.2/tasks/P11-001.md`（任务定义）
- `lib/plate_solve/IPV_PIPELINE.md`（管线说明，CD 无 1/cos(Dec)）
- `lib/plate_solve/cpp/ipv/SIRIL_COMPARISON.md`（与 Siril 对比）
