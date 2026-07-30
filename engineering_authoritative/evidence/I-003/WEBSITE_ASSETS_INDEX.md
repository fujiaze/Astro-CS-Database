# I-003 网站素材索引 (WEBSITE ASSETS INDEX)

- Gate: I
- 任务: I-003
- 日期: 2026-07-30
- 状态: DRAFT (待710帧完成后最终确认)

## 1. 网站页面结构

| 页面 | 素材来源 | 状态 |
|------|---------|------|
| 首页 | README.md 概述 | READY |
| 特性 | Gate A-H 能力列表 | READY |
| 工作流 | docs/01-08 specs | READY |
| HISS/HCSD | contracts/HISS_FORMAT_V2.md | READY |
| 下载 | RELEASE_MANIFEST.md | 待710帧 |
| 文档 | docs/ 全部 | READY |
| 兼容性 | 设备 T2/T3/T4 + 滤镜 | READY |
| 性能与验证 | Gate G/H 证据 | READY |
| 路线图 | docs/19_ROADMAP | READY |
| 更新日志 | git log | READY |
| 开发者接口 | CLI_CONTRACT.md | READY |
| 许可 | LICENSE | READY |

## 2. 展示素材清单

### 2.1 银心对比（Gate F/G）
| 素材 | 来源 | 说明 |
|------|------|------|
| 三片Red HISS信号 | evidence/F-001 | panel1/2/3 信号图 |
| 30帧叠加HCSD | evidence/G-004 | T4_RED_GalaxyCenter_30frame_fused.hcsd |
| 接缝诊断 | evidence/G-005 | seam diagnostic 图 |
| 连续性量化 | evidence/G-005 | 连续性突变率 12.80% |

### 2.2 SNR点与梯度控制点（Gate E）
| 素材 | 来源 | 说明 |
|------|------|------|
| SNR稀疏控制点 | evidence/E-001 | 2400控制点分布 |
| 梯度曲面 | evidence/E-003 | 全局加性共识曲面 |
| 权重分布 | evidence/E-002 | SNR²/逆方差联合权重 |

### 2.3 排异层（Gate G）
| 素材 | 来源 | 说明 |
|------|------|------|
| 排异统计 | evidence/G-001 | MAD排异率 3.43% |
| 融合结果 | evidence/G-002 | SNR²加权融合 |

### 2.4 浏览器录屏（Gate C）
| 素材 | 来源 | 说明 |
|------|------|------|
| HISS浏览器截图 | evidence/C-003 | inspector 输出 |
| HCSD球面视图 | healpix_browser_qt | Qt6+OpenGL 3.3 |

### 2.5 资源调度曲线（Gate H）
| 素材 | 来源 | 说明 |
|------|------|------|
| 内存监控 | evidence/H-001 | baseline.json |
| 准入控制 | evidence/H-002 | admission_controller |
| Spill恢复 | evidence/H-003 | spill_manager |

### 2.6 710帧回归（Gate I）—— 待完成
| 素材 | 来源 | 说明 |
|------|------|------|
| 回归汇总 | evidence/I-002 | 710帧通过率/失败分类 |
| 失败分析 | evidence/I-002 | 栈溢出根因+影响范围 |
| 代表帧HISS | output/p13-001/ | 14帧代表帧产物 |

## 3. 能力状态标注规则

| 状态 | 含义 | 适用 |
|------|------|------|
| 已发布 | 通过 Gate 验证 | Stage1/Stage2/HISS/HCSD |
| 实验性 | 功能实现但未全量验证 | 浏览器高级功能 |
| 开发中 | 部分实现 | validate/resume/benchmark CLI |
| 计划中 | 未实现 | 未列入契约的 PLANNED 命令 |

## 4. 网站与状态一致性检查

- [ ] 710帧回归结果与"已发布"标注一致
- [ ] 栈溢出限制在兼容性页面明确说明
- [ ] 下载页SHA256SUMS与发布包一致
- [ ] 更新日志包含 Gate A-I 全部里程碑
