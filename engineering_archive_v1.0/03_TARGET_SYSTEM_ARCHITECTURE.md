# 03 目标系统架构

## 1. 架构视图

```text
                    ┌──────────────────────────────┐
                    │  Configuration + Run Manifest│
                    └──────────────┬───────────────┘
                                   │
Raw FITS + Masters ──► Orchestrator/Stage 1 ──► immutable .hiss
                                   │
                                   ├─ logs / metrics / checkpoints / evidence
                                   ▼
                      Orchestrator/Stage 2
                                   │
                                   ├─ overlap graph
                                   ├─ gradient models
                                   ├─ robust rejection
                                   ├─ SNR² weighting
                                   ▼
                              versioned .hcsd
                                   │
                                   ├─ query API
                                   └─ Qt browser
```

## 2. 分层

### A. 算法模块层

每个模块只实现明确算法和 C ABI，不负责全局路径、跨阶段状态或用户交互。

### B. 数据契约层

由 `astro_image_io` 或后续明确的唯一数据层负责：

- PipelineFrame；
- HISS/HCSD；
- 序列化、压缩、版本与校验；
- 跨模块数据类型定义。

`data_pipeline` 不得继续作为第二份同名 ABI 长期存在。P02 必须决定：合并、废弃或改名。

### C. 编排层

`orchestrator` 负责：

- 加载配置并校验 schema；
- 管理模块生命周期；
- 调用顺序、超时、重试与失败分类；
- 检查点与运行清单；
- 收集指标；
- 不实现算法细节。

### D. 验证层

Python 主要承担：

- 合成数据生成；
- 结果统计；
- 与参考实现对比；
- 回归测试；
- 报告生成。

它不是永久业务胶水，但可以长期作为验证工具。

### E. 表现层

Qt 浏览器只读取稳定版本的 HISS/HCSD，不直接依赖算法内部结构。

## 3. 唯一数据流契约

PipelineFrame 标准块必须由注册表管理，不允许只靠注释约定。至少包括：

| 块名 | 类型 | 生产者 | 消费者 | 说明 |
|---|---|---|---|---|
| header | KV | READ_FITS/PLATESOLVE | 后续全部 | FITS/WCS/SIP/观测元数据 |
| data | FLOAT32[H,W] | READ_FITS/CALIBRATE/PHOTOMETRIC | 后续阶段 | 始终保持 float32，禁止无记录地截断到 uint16 |
| cal_stats | KV | CALIBRATE | 监控/报告 | 主帧、暗场优化、坏点与退化状态 |
| star_det | 版本化数组 | PLATESOLVE/检测器 | PSF | 字段顺序由 schema 定义 |
| gaia_cat | 版本化数组 | PLATESOLVE | PHOTOMETRIC | 查询范围、星等与截断策略可追溯 |
| psf | FLOAT64[N,9] | PSF | PHOTOMETRIC/SNR | 精确字段顺序固定 |
| photo_stats | KV | PHOTOMETRIC | SNR/报告 | scale、匹配数、sigma residual |
| snr_model | RAW/typed | SNR | DRIZZLE/HISS | 稀疏球面控制点模型 |

## 4. HISS 与 HCSD 角色

### HISS

- 单帧、不可变；
- 包含原始球面像素与足够的质量/SNR/观测元数据；
- 不写入 Stage 2 梯度校正后的结果；
- 每个文件可独立验证校验和、版本和坐标系统。

### HCSD

- 多帧融合产物；
- 包含生成它的输入 HISS 列表与哈希、算法配置、版本、覆盖信息；
- 支持按 nside=64 子叶读取；
- 结果值、权重、覆盖数或质量统计的保留策略必须在 P02 决定。

## 5. Stage 2 内部必须可观测

即使最终 API 仍是一条 `hp_stack_gradient_corrected()`，也必须能导出或记录：

- 输入帧清单和兼容性检查；
- overlap graph；
- 每帧梯度模型参数或摘要；
- 每轮收敛指标（如采用迭代）；
- 每个区域参与帧数、拒绝样本数、权重统计；
- 输出接缝指标与通量偏差；
- 回退路径是否发生。

## 6. 架构决策门禁

以下问题没有 ADR 不允许直接改代码：

- `data_pipeline` 与 `astro_image_io` 谁拥有 PipelineFrame；
- Stage 2 是保留两个节点还是合并成一个节点；
- HCSD 是否存 coverage/weight/variance；
- star_detector 是否增加 float32 API；
- HISS/HCSD 版本升级策略；
- 统一构建采用根 CMake 还是保留模块 Makefile 但增加 bootstrap。
