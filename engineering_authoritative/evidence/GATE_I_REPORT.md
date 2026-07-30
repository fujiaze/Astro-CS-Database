# Gate I 合并报告 — 全量回归与发布

- Gate: I
- 日期: 2026-07-30
- 状态: PENDING_USER_DECISION (I-002 阶段B 违规启动已停止, 等待用户决策)
- 依赖: Gate H (已完成)

## 1. Gate I 任务清单

| 任务 | 标题 | 状态 | 说明 |
|------|------|------|------|
| I-001 | 冻结格式、CLI和算法契约 | DONE | CLI_CONTRACT.md + ALGORITHM_CONTRACT.md FROZEN |
| I-002 | 执行710帧最终回归与失败分类 | PENDING_USER_DECISION | 阶段A PASS (14/15帧), 阶段B 违规启动已停止 |
| I-003 | 生成发布包、网站素材索引和最终审计ZIP | TODO | 框架就绪, 待 I-002 完成 |

## 2. I-001 契约冻结结果

### 2.1 CLI 契约 (FROZEN 2026-07-30)
- 已实现命令: stage1, stage2, inspect, capabilities
- JSONL 事件流: 9种事件类型
- 退出码: 11个主退出码 + 9个模块码
- 配置优先级: cli > overrides > config > default

### 2.2 算法契约 (FROZEN 2026-07-30)
- Stage1: 8阶段固定流水线 (READ_FITS→...→HISS_WRITE)
- Stage2: 全局加性共识曲面 (只加性, SNR²加权, 零均值约束)
- 稳健排异: MAD (3.0×1.4826×MAD)
- SNR²融合: 连续加权, 无硬阈值
- HCSD: 生产层 + 可开关调试层

### 2.3 HISS 格式 (FROZEN, Gate C)
- 四要素: signal(float32) + support(uint8) + SNR(稀疏) + provenance(JSON)
- 分块存储 + zstd压缩 + CRC32校验

## 3. I-002 710帧回归

### 3.1 阶段A — 15帧代表帧验证 (PASS)
- T2 NGC247: 5/5 通过 (LUM/RED/GREEN/BLUE/HA)
- T3 NGC55: 5/5 通过 (RED/GREEN/BLUE/HA/OIII)
- T4 Victory_Nebula: 4/5 通过, 1栈溢出 (已知DRIZZLE限制)
- 结论: 契约冻结无回归

### 3.2 阶段B — 710帧全量回归 (运行中)
- 模式: T2/T3/T4 三设备并行
- 进度: (待完成填充)
- 预计失败: ~2.5% 栈溢出 (Victory_Nebula T4 LUM特定帧)

## 4. I-003 发布包

### 4.1 发布包清单
- 详见: evidence/I-003/RELEASE_MANIFEST.md

### 4.2 网站素材索引
- 详见: evidence/I-003/WEBSITE_ASSETS_INDEX.md

### 4.3 最终审计ZIP
- (待710帧完成后生成)

## 5. 已知限制

| 限制 | 影响 | 缓解 |
|------|------|------|
| DRIZZLE栈溢出 | ~2.5%帧失败 (Victory_Nebula T4 LUM) | 需增大栈大小或分块处理 |
| 中文路径不兼容 | orchestrator C++ 无法处理含中文路径 | 配置/输出路径改用英文 |
| 浏览器为CLI | validate/browser/resume 命令未实现 | 标记为 PLANNED |

## 6. Gate I Checklist

- [x] 契约冻结 (I-001 DONE)
- [ ] 710帧完成 (I-002 运行中)
- [ ] 失败分类透明 (待710帧完成)
- [ ] 发布包依赖完整 (I-003 框架就绪)
- [ ] 网站素材与状态一致 (I-003 框架就绪)
- [ ] 最终审计ZIP (待生成)
