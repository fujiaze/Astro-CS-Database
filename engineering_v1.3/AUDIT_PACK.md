# AstroCS v1.3 审计交付包 (AUDIT_PACK)

> 生成时间: 2026-07-29
> 用途: 用户审计当前开发状态，给出下一段开发指南
> 当前 Gate: G12 (Stage1 科学链)
> 当前任务: P13-002 (IN_PROGRESS，用户调整范围)

---

## 1. 项目状态总览

### 1.1 项目阶段

| 项 | 值 |
| --- | --- |
| 项目 | AstroCS CLI Core + Spherical Browser |
| 版本 | v1.3 (P13 Stage1 全量验证阶段) |
| 状态 | ACTIVE |
| 当前 Gate | G12 |
| 当前任务 | P13-002 (IN_PROGRESS) |
| 上一完成任务 | P13-001 (2026-07-29) |
| 工程包根 | `engineering_v1.3/` |

### 1.2 任务进度总览

| 阶段 | 任务数 | DONE | IN_PROGRESS | TODO |
| --- | --- | --- | --- | --- |
| P09 (基线冻结) | 3 | 3 | 0 | 0 |
| P10 (TestData 与校准) | 6 | 6 | 0 | 0 |
| P11 (WCS 与测光) | 6 | 6 | 0 | 0 |
| P12 (测光修复) | 6 | 6 | 0 | 0 |
| P13 (Stage1 全量验证) | 4 | 1 | 1 | 2 |
| P14 (银心马赛克) | 8 | 0 | 0 | 8 |
| P15 (浏览器异步 I/O) | 8 | 0 | 0 | 8 |
| P16 (浏览器 GPU) | 6 | 0 | 0 | 6 |
| P17 (统一回归) | 3 | 0 | 0 | 3 |
| **合计** | **50** | **22** | **1** | **27** |

### 1.3 已完成 Gate

| Gate | 描述 | 完成日期 |
| --- | --- | --- |
| G9 | 基线冻结 | 2026-07-28 |
| G10 | TestData 与校准 | 2026-07-28 |
| G11 | WCS 与测光 | 2026-07-28 |
| G12 | Stage1 科学链 | **进行中** |

---

## 2. P13-002 会话交付物（本次审计重点）

### 2.1 已完成工作

| # | 工作项 | commit | 验证 |
| --- | --- | --- | --- |
| 1 | snr_evaluator 栈溢出修复（DLL 栈 8MB + leaf_max_size 32） | 61d49fa | 3/3 失败帧 PASS |
| 2 | 浏览器 CLI 后台调试工具 + 部署修复 | e5aeaac | 9/9 DLL OK + 启动正常 |
| 3 | orchestrator.exe 栈 32MB（根因修正：DLL 用调用线程栈） | 04226a9 | 综合验证 PASS |
| 4 | Stage2 银心 5 帧验证 | - | galaxy_center_stacked.hcsd success=true |
| 5 | Stage2 胜利 20 帧 LUM 验证 | - | victory_lum_stacked.hcsd success=true |
| 6 | Stage1 批量运行（用户调整范围） | - | 281/385 T4 帧 PASS |

### 2.2 关键修复根因

#### 栈溢出 (0xC00000FD)

- **症状**: Victory_Nebula mosaic1 RED@045711 + BLUE@060603/071753 在 DRIZZLE 阶段栈溢出
- **根因**: nanoflann `divideTree` 在偏斜 3D 笛卡尔控制点数据上递归数百层（理想 ~11 层），每帧栈使用 1-2KB，超过 Windows DLL 默认 1MB 栈
- **关键认知**: `-Wl,--stack` 对 DLL 无效，DLL 代码使用调用线程栈，必须设置 EXE 栈
- **修复**: healpix_drizzle.dll 栈 8MB + snr_evaluator leaf_max_size 10→32 + orchestrator.exe 栈 32MB
- **验证**: 3/3 失败帧 exit=0，HISS 正常生成（85KB-87KB）

#### 浏览器 STATUS_DLL_NOT_FOUND

- **症状**: 双击启动 exit=-1073741515
- **根因**: astro_image_io.dll 依赖 libgomp-1.dll（OpenMP）+ liblz4.dll（压缩）未部署
- **修复**: deploy.ps1 添加这两个 DLL

### 2.3 性能基线（浏览器 CLI 实测）

| 场景 | 指标 | 值 |
| --- | --- | --- |
| .hiss 加载 | 耗时 | 3.4 ms |
| .hcsd 加载 | 耗时 | 1.4 ms |
| .hcsd 子叶加载 | avg | 0.42 ms/叶 |
| 球面模式 FPS | - | 55-63 FPS |
| 内存占用 | - | 8 MB |

---

## 3. 用户方向调整记录（2026-07-29）

用户明确指示：**不在前面就批量跑 710 帧**（严重拖慢进度）。改为分阶段：

1. ✅ 先修复栈溢出
2. ✅ 只用银心+胜利两组数据跑通全流程（Stage1+Stage2+浏览器）
3. ✅ 修好浏览器性能（CLI 工具 + 部署修复）
4. ⏳ **用户验证全流程工作没问题**（待本次审计）
5. ⏳ 优化 Stage1 性能（80s/帧太慢）
6. ⏳ 全部验证好后，再跑最终全流程测试

---

## 4. 已知问题与待优化项

### 4.1 性能问题（用户明确要求优化）

| 阶段 | 耗时 | 占比 | 备注 |
| --- | --- | --- | --- |
| PLATESOLVE | ~13s | 16% | 求解器大头 |
| DRIZZLE | 15-24s | 19-30% | 球面重投影 |
| 其他 | 40-50s | 50-60% | READ/CAL/PSF/PHOT/SNR |
| **总计** | **70-80s/帧** | 100% | 用户认为太慢 |

### 4.2 数据依赖风险

- 栈溢出修复对当前数据集足够（32MB），但极端偏斜数据可能仍需更大栈
- 算法层修复（nanoflann middleSplit_ 退化分割）可作后续改进

### 4.3 未完成任务

| 任务 ID | 描述 | 状态 |
| --- | --- | --- |
| P13-002 | 全量 710 帧 Stage1 | IN_PROGRESS（281/710，用户调整范围） |
| P13-003 | Stage1 负面恢复与资源验证 | TODO |
| P13-004 | Stage1 科学链独立 Gate | TODO |
| P14-* | 银心三片 32 帧马赛克（8 任务） | TODO |
| P15-* | 浏览器异步 I/O（8 任务） | TODO |
| P16-* | 浏览器 GPU Tile Renderer（6 任务） | TODO |
| P17-* | 统一回归与发布（3 任务） | TODO |

---

## 5. 审计入口文档导航

### 5.1 工程控制文件

| 文档 | 路径 | 用途 |
| --- | --- | --- |
| 项目状态 | [engineering_v1.3/control/PROJECT_STATE.yaml](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/control/PROJECT_STATE.yaml) | 当前项目状态 |
| 任务注册表 | [engineering_v1.3/control/MASTER_TASK_REGISTER.csv](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/control/MASTER_TASK_REGISTER.csv) | 50 任务状态 |
| 当前任务 | [engineering_v1.3/control/CURRENT_TASK.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/control/CURRENT_TASK.md) | 当前任务详情 |
| 基线事实 | [engineering_v1.3/control/BASELINE_FACTS.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/control/BASELINE_FACTS.md) | 已冻结基线 |
| 决策登记 | [engineering_v1.3/control/DECISION_REGISTER.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/control/DECISION_REGISTER.md) | 关键决策记录 |
| 风险登记 | [engineering_v1.3/control/RISK_REGISTER.csv](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/control/RISK_REGISTER.csv) | 风险跟踪 |

### 5.2 P13-002 证据 4 件套

| 文档 | 路径 |
| --- | --- |
| 任务报告 | [engineering_v1.3/evidence/P13-002/TASK_REPORT.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/evidence/P13-002/TASK_REPORT.md) |
| 测试报告 | [engineering_v1.3/evidence/P13-002/TEST_REPORT.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/evidence/P13-002/TEST_REPORT.md) |
| 证据索引 | [engineering_v1.3/evidence/P13-002/EVIDENCE_INDEX.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/evidence/P13-002/EVIDENCE_INDEX.md) |
| 独立复核 | [engineering_v1.3/evidence/P13-002/REVIEW_REPORT.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/evidence/P13-002/REVIEW_REPORT.md) |

### 5.3 自治入口与规格

| 文档 | 路径 | 用途 |
| --- | --- | --- |
| 自治入口 | [engineering_v1.3/AUTONOMOUS_ENTRY.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/AUTONOMOUS_ENTRY.md) | Agent 自治执行入口 |
| 阶段边界 | [engineering_v1.3/docs/00_PHASE_GOAL_AND_BOUNDARIES.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/docs/00_PHASE_GOAL_AND_BOUNDARIES.md) | 阶段目标与边界 |
| Stage1 Spec | [engineering_v1.3/docs/08_STAGE1_REAL_DATA_FULL_VALIDATION_SPEC.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/docs/08_STAGE1_REAL_DATA_FULL_VALIDATION_SPEC.md) | Stage1 全量验证规格 |
| 银心马赛克 Spec | [engineering_v1.3/docs/09_GALAXY_CENTER_THREE_PANEL_MOSAIC_SPEC.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/engineering_v1.3/docs/09_GALAXY_CENTER_THREE_PANEL_MOSAIC_SPEC.md) | 银心三片马赛克规格 |

### 5.4 根目录组织

| 文档 | 路径 | 用途 |
| --- | --- | --- |
| 根目录清单 | [ROOT_INVENTORY.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/ROOT_INVENTORY.md) | 根目录每个目录/文件的用途与归档状态 |
| 项目 README | [README.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/README.md) | 项目说明（待更新到 v1.3） |
| 项目记忆 | [memory.md](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/memory.md) | 工程记忆 |

---

## 6. 审计建议关注点

### 6.1 本次会话技术亮点

1. **栈溢出根因分析深入**：从现象（exit code）→ 模块定位（snr_evaluator）→ 算法根因（nanoflann 递归深度）→ 系统根因（DLL 栈限制 + -Wl,--stack 对 DLL 无效）→ 修复策略（EXE 栈 32MB）
2. **浏览器 CLI 工具填补空白**：之前无后台调试手段，现在可非交互式诊断 DLL 依赖 + 性能基准 + 模拟交互
3. **Stage2 全流程闭环验证**：银心 5 滤镜 + 胜利 20 帧 LUM 均生成有效 HCSD

### 6.2 待用户决策项

1. **Stage1 性能优化方向**：
   - PLATESOLVE 13s：是否优化 Gaia 查询/候选过滤/求解器迭代？
   - DRIZZLE 15-24s：是否优化球面投影/并行度/内存布局？
   - 其他 40-50s：哪些子阶段可优化？

2. **全量测试时机**：
   - 优化后立即跑 710 帧？
   - 还是先完成 P14（银心马赛克）再统一回归？

3. **浏览器后续路径**：
   - P15（异步 I/O）+ P16（GPU Renderer）是否调整优先级？
   - 当前 55-63 FPS 是否满足交互需求，还是需要 GPU 加速？

4. **栈溢出算法层修复**：
   - 当前 32MB EXE 栈对当前数据足够
   - 是否需要算法层修复（nanoflann middleSplit_ 退化分割）作为长期方案？

---

## 7. 下一段开发候选方向

基于当前状态和未完成任务，候选方向（待用户审计后决定）：

| 方向 | 内容 | 依赖 |
| --- | --- | --- |
| A. Stage1 性能优化 | PLATESOLVE + DRIZZLE 优化，目标 <30s/帧 | P13-002 当前交付 |
| B. 全量 710 帧 Stage1 | 性能优化后跑全量 | A 完成 |
| C. 银心马赛克 P14 | 三片 32 帧梯度校正 + 叠加 | P13-002 完成 |
| D. 浏览器异步 I/O P15 | Tile 缓存 + 异步解码 | P13-002 完成 |
| E. 浏览器 GPU P16 | R32F Tile Renderer | P15 完成 |

**建议优先级**：A → B → C → D → E（性能优化是用户明确要求，且阻塞全量测试）

---

## 8. VERDICT

```
本次交付包 VERDICT: IN_PROGRESS (已交付部分均 PASS)
等待用户审计，给出下一段开发指南。
```
