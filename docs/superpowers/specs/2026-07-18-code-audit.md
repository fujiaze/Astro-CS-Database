# 代码审计 spec - 2026-07-18

## 1. 背景与目的

GRADIENT_2D 归档重构完成后，stage1 由 8 节点缩减为 7 节点。需逐模块细致扫描活跃 C++ 代码，验证实现是否符合最新架构文档要求，发现并记录所有层级问题，作为后续修复决策的依据。

## 2. 范围

### 2.1 扫描模块（9 个，仅活跃 C++ 代码，归档不扫）

| # | 模块 | 活跃 C++ 代码路径 | 不扫描 |
|---|------|------------------|--------|
| 1 | astro_image_io | `lib/astro_image_io/src/aio_*.cpp` + `src/healpix/aio_healpix_io.cpp` + `include/*.h` | `src/ahpx/` (deprecated) |
| 2 | calibration | `lib/calibration/src/*.cpp` + `include/astro_calibration.h` | `cpp/cosmetic_corrector.*` (重复) |
| 3 | plate_solve | `lib/plate_solve/cpp/ipv/src/ipv_*.cpp` + `include/ipv_*.h` | `archive/vector_method/` |
| 4 | dynamic_psf | `lib/dynamic_psf/src/dpsf_*.cpp` + `include/dynamic_psf.h` | — |
| 5 | photometric_calib | `lib/photometric_calib/cpp/src/*.cpp` + `cpp/include/*.h` | `archive/` |
| 6 | snr_estimator | `lib/snr_estimator/cpp/src/snr_estimator.cpp` + `include/snr_estimator.h` | — |
| 7 | healpix_drizzle | `lib/healpix_db/healpix_drizzle/*.cpp` + `*.h` | — |
| 8 | healpix_stack | `lib/healpix_db/healpix_stack/*.cpp` + `gradient/*.cpp` + 对应 `.h` | — |
| 9 | orchestrator | `lib/orchestrator/cpp/src/*.cpp` + `include/*.h` | `archive/` |

### 2.2 对照基准（4 文档 + project_memory 硬约束）

- `docs/PROJECT_OVERVIEW.md`（最新架构总览）
- `docs/ARCHITECTURE.md`（架构详细）
- `docs/PIPELINE_OVERVIEW.md`（流水线概述）
- `docs/DESIGN_IMPL_GAP.md`（GAP-001~021 已批复修复要求）
- `c:\Users\fujia\.trae-cn\memory\projects\-f-Astro-dev-Astro-CS-Normalization-Database\project_memory.md` 中的硬约束

## 3. 检查重点（4 个方面）

### 3.1 stage handler 与 DLL API 契约
- orchestrator.cpp 中各 `run_stage_*` 调用 DLL 时参数传递是否正确
- PipelineFrame 命名块读写：块名/数据类型/形状是否符合架构文档
- 错误处理：DLL 调用失败时是否正确传播、日志是否清晰
- 资源生命周期：handle 创建/复用/销毁是否无泄漏

### 3.2 算法核心逻辑
- **PLATESOLVE**：gnomonic 投影 + 向量匹配法 + Umeyama SVD 迭代精化 + s 限制在初始值 ±10% + 动态 inlier 阈值 (3.0×1.4826×MAD, min 1.0×s0) + RANSAC 同时校验欧氏距离+向量叉积 + 5 次 MAD outlier 移除 (max(5", 3×1.4826×MAD))
- **PHOTOMETRIC**：F_syn=∫S(λ)·T(λ)·Q(λ)dλ 积分（含 CCD QE）+ IRLS+Tukey biweight 稳健回归求全局 scale + **应用 scale 到图像**（测光坐标系校准，I_cal = I×scale）+ 输出 photo_stats（scale_factor/n_matched/sigma_residual）
- **SNR**：乘法模型 SNR = SNR_phot × (IDW_球面(控制点, query) / median_snr)，结合测光不确定度（sigma_residual）和 PSF SNR（snr_psf）；稀疏控制点 + 区域 SNR 拟合 + Winsorized sigma clip + SNR² 加权叠加（stage2）
- **DRIZZLE**：标准 WCS+SIP，CD 矩阵无 1/cos(Dec) 因子，nside 自适应计算
- **STACK**：球面梯度校正 + Winsorized sigma clip + SNR² 加权叠加

### 3.3 数据格式与接口
- `.hiss` 格式（含 snr 通道）读写一致性
- `.hcsd` 格式（含子叶块索引 nside=64）读写一致性
- WCS+SIP 标准性（CD 矩阵 / CRVAL / CRPIX / CTYPE / A/B/AP/BP 多项式）
- PipelineFrame 命名块契约（FLOAT32[H,W] / FLOAT32[N,4] / FLOAT64[N,3] / FLOAT64[N,9] / KV）

### 3.4 运维质量
- 日志输出：每个模块有日志目录，日志分级 (INFO/WARN/ERROR)，关键参数落盘
- 配置文件：JSON 参数完整性与默认值合理性
- 错误码：DLL API 错误返回一致（成功=1/0 还是 0/-1）
- 资源释放：析构顺序、文件句柄、内存释放

## 4. 输出格式

每个模块输出问题清单，按严重度分级：

| 级别 | 定义 | 例子 |
|------|------|------|
| **Critical** | 架构不一致、算法错误、数据损坏风险 | PHOTOMETRIC 未应用 scale 到图像但文档说应用了 |
| **High** | 硬约束违反、API 契约不符、错误处理缺失 | RANSAC 未同时做距离+向量叉积校验 |
| **Medium** | 日志缺失、配置默认值不合理、资源释放顺序错 | 缺少关键参数日志、句柄销毁顺序反 |
| **Low** | 代码风格、注释不全、命名不一致 | 变量命名与文档不一致 |

每个问题包含：
- 模块名 / 文件路径 / 行号
- 问题描述（事实陈述，不含推测）
- 对照基准（哪条文档/硬约束）
- 影响评估
- 建议修复方式（A/B 选项）

## 5. 执行方法

- 9 个子代理（subagent_type=search）并行扫描，每个负责一个模块
- 每个子代理读取：4 个文档相关章节 + project_memory.md 硬约束 + 模块 memory.md + 模块 C++ 代码
- 输出：按问题清单格式返回（不返回代码片段，只引行号）
- 主代理汇总 9 份问题清单 → 总报告
- 用户审阅后决定修复范围

## 6. 验收标准

- 9 个模块全部扫描完成
- 每模块问题清单格式完整（模块/文件/行号/描述/基准/影响/建议）
- 总报告按严重度排序，含统计表（Critical/High/Medium/Low 各多少）
- 不修改任何代码（本轮纯审计）

## 7. 不包含

- Python 代码扫描（用户明确只扫 C++）
- 归档代码扫描
- 测试代码扫描（tests/ 目录）
- 第三方库扫描（Eigen/nanoflann 等）
- 修复实施（用户审阅问题清单后再决定）
