# 严格交付包规则

## 1. 四个逻辑交付部分

### A. Control Package

- 本权威控制包；
- ADR、依赖锁和许可证；
- API/schema/Benchmark/测试规范；
- 风险、决策和已知限制。

### B. Complete Source Snapshot

结果commit对应的完整 AstroCS 源码快照，保留原目录，足以脱离Agent仓库静态审查。不得只交diff。不得包含 `.git`、构建产物、GPU SDK、依赖缓存、旧版本副本或大型临时数据。

### C. Evidence Package

至少包含：

- 构建日志与工具链；
- 单测、经典实验、真实GPU/Mixed结果；
- Qualification原始JSON、HardwareProfile和拟合报告；
- 利用率控制报告；
- Sanitizer实际构建日志；
- 故障注入；
- path guard；
- 主线合并前后回归；
- SKIPPED/失败及原因。

### D. Merge Report

base/main/feature/merge commit、冲突、测试、dormant状态、算法目录零修改和后续集成建议。

## 2. 单一HEAD

Evidence、源码快照、summary、JSON、日志、manifest和Merge Report必须来自同一干净HEAD。生成后若发生任何提交，全部重新生成。禁止混装修复前后结果。

## 3. Manifest和哈希

每个ZIP提供：

- `package_manifest.json`；
- `SHA256SUMS.txt`；
- 路径、大小和SHA-256；
- 生成时间；
- base/result/merge commit；
- 工具版本。

生成后必须重新解压并校验。

## 4. 画像证据

必须包含：

- 原始样本，而非只有摘要；
- 设备/ISA/线程/尺寸/精度/驻留；
- 模型拟合与留出误差；
- HardwareProfile schema校验；
- 运行前后profile hash；
- 无GPU时Mixed标为SKIPPED。

## 5. 依赖

提供 dependency-lock、SPDX、NOTICE和本地补丁。不得把CUDA/ROCm/oneAPI SDK和包管理器缓存装入ZIP。

## 6. 命名

未发布项目只使用稳定权威名称：

```text
AstroCS_ACR_Control_Package.zip
```

后续直接更新这一个包，不并列V1/V2或日期包。
