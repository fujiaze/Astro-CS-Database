# R10 已知问题报告

日期: 2026-08-04

## 1. 非阻断问题

### K-001: orchestrator 日志路径解析 bug
- 现象: 运行时在 `cpp/` 下创建 `lib/orchestrator/logs/` 嵌套目录
- 根因: 日志目录解析未固定绝对路径/`run/logs/orchestrator/`
- 状态: 待修复（非阻断，日志内容正确）
- 位置: `lib/orchestrator/cpp/src/`

### K-002: 源码树生成产物未清理
- 现象: `lib/` 下散落 .exe/.dll/.o、`logger_*` 测试临时目录、`cpp/lib/orchestrator/` 嵌套目录
- 状态: 阶段 8c 交付前清理（本报告随证据归档时同步处理）

### K-003: 根目录散落旧交付/产物
- 现象: 根目录多个旧 ZIP、orchestrator.exe/dll、测试文件
- 状态: 按 HANDOVER §6.1 清理

### K-004: data_pipeline 模块与 astro_image_io 重叠
- 状态: 待 ADR 明确（未触碰）

## 2. 已知精度例外（科学合理性说明）

### P-001: 星点检测固定 FLOAT32
- FP64 模式 PLATESOLVE 星点检测前显式转换 FLOAT64→FLOAT32
- 原因: 检测器 ABI/性能约束（星检测串行），WCS 拟合仍为 double
- 影响: 对 PlateSolve 最终 WCS 精度影响可忽略（检测只提供初始坐标）

### P-002: 非线性拟合内部使用 double
- PSF Moffat 拟合、测光最小二乘在两种模式下内部均用 double
- FP32 模式输入 float32；FP64 模式用专用 f64 API 输入 float64（不降级）

## 3. 控制包缺陷登记对应状态

以下 BLOCKER/MAJOR 缺陷已闭合（对应修复见 `implementation_report.md`）:

- CFG-001/002/003/004/005/006/007/008/009/010/011/012（编排器/配置）
- PREC-001/002/003/004/005/006/007/008/009/010/011/012/014/015（双精度）
- PY-001/002/003/004/005（Python 边界）
- TEST-001/002/003/004/005/006/010（验证）
- SNR-001/002（SNR 核算）
- HISS-001/002/003（HISS 双 dtype）
- DOC-001/002/003/004（文档）
- PKG-001/002/003（交付，阶段 8c 处理）

未闭合（超出 R10 范围或需用户决策）:

- PREC-013（逐模块 ULP/误差实验，需独立精度研究）
- PREC-016（第三方库精度能力审计）
- TEST-007（FP32 误差/ULP 门限实验）
- TEST-008/009（性能对比证据，需可比较运行环境）
- HISS-004（真实文件 SNR/metadata/occupancy roundtrip 全量证据）
- SNR 丢弃点 INVALID_PSF 的科学优化（PSF 质量改进）
