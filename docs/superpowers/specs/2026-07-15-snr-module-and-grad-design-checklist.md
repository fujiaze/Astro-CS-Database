# Checklist: SNR 模块设计 + §12 梯度校正设计调整 + §13 PSF 统一调整

> **对应 spec**: 2026-07-15-snr-module-and-grad-design.md
> **任务类型**: 工程设计文档更新（不写实现代码）
> **状态**: ✅ 全部完成（11 项决策全部已确认，文档更新全部落地）

## 文档更新项

### PROJECT_ARCHITECTURE.md

- [x] **新增 §14 SNR 估算模块**
  - [x] 模块定位（lib/snr_estimator/ 独立 C++ 模块）
  - [x] 管线阶段（STAGE_SNR，PHOTOMETRIC 后、DRIZZLE 前）
  - [x] 乘法模型算法（含数学推导要点）
  - [x] 输入/输出接口（psf 块 + photo_stats → snr 块）
  - [x] 边界处理（sigma_residual=0 / 无 PSF 星 / N_matched<10）
  - [x] IDW 插值参数（6 邻居，power=2）

- [x] **§12 新增 §12.15 SNR 耦合与隔离设计**（✅ 决策2 已确认）
  - [x] SNR-A（梯度拟合权重）vs SNR-B（叠加权重）定义
  - [x] SNR 不受梯度影响的严格推导（测光背景拟合+PSF B参数吸收梯度）
  - [x] SNR 单次计算 + 固定权重解耦方案
  - [x] SNR² 最优加权理论（MLE/MVUE，inverse-variance weighting）
  - [x] 双层 SNR 保护（阶段2 SNR-A 加权 + 阶段3 SNR-B² 加权）
  - [x] 叠加执行顺序（梯度迭代收敛→sigma-clip→SNR-B² 加权，串行不嵌套）
  - [x] 不耦合证明（5 点：SNR 固定/g_i 稳定/sigma-clip 一次性/校正期望行为/两层独立）
  - [x] 验证标准补强（§12.14 双层保证机制）

- [x] **§12.13 文件名修正**
  - [x] existing/sigma_clip_stack → existing/stack_engine + existing/hp_stack_hiss
  - [x] gradient/ 子目录 5 文件确认（gradient_sampler/spherical_tps/gradient_fitter/gaia_rejector/corrected_stacker）

- [x] **§13 PSF 统一调整**
  - [x] 保留 platesolve 不动（说明原因）
  - [x] psf 块生命周期延长（PHOTOMETRIC 清理 → STAGE_SNR 后清理）
  - [x] psf 块格式扩展（FLOAT64[N,6] → FLOAT64[N,9] 加 A/mad/eccentricity）
  - [x] PSF 统一核心说明（SNR 复用 PSF 的 A/B/mad）

- [x] **管线阶段调整**
  - [x] 新增 STAGE_SNR（CALIBRATE=0 → PLATESOLVE=1 → PHOTOMETRIC=2 → SNR=3 → DRIZZLE=4 → STACK=5）
  - [x] 块生命周期表更新（psf 延长、snr 新增、photo_stats 扩展）

- [x] **.hiss 格式扩展说明**
  - [x] 新增 SNR 通道（ipix + brightness + snr）
  - [x] weight 通道可选（待 stack 实现时确认）
  - [x] 引用 FORMAT_SPEC.md 需同步更新

- [x] **drizzle 累加公式问题记录**
  - [x] 记录当前公式 sqrt(sum_snr_sq/sum_weight) 多帧不提高问题
  - [x] 记录正确公式 SNR_stacked = Σsignal_i/sqrt(Σ(signal_i/SNR_i)²)
  - [x] 标注：待 stack 阶段实现时修正

- [x] **photometric_calib 接口扩展说明**
  - [x] pc_calibrate_simple 新增 out_sigma_residual 输出
  - [x] 说明 star_matcher 内部已计算，只需暴露

- [x] **hp_stack_hiss 前置依赖说明**
  - [x] 当前无权重 sigma-clip → 需扩展为 SNR-B 加权
  - [x] 标注为 §12 梯度校正集成的前置依赖

- [x] **§14.7 SNR² 加权贯通 4 处断层**（新增决策10）
  - [x] 4 处断层表格（drizzle 落盘丢弃 / .hiss 未实现 snr 通道 / Python 绑定未扩展 / hp_stack_hiss 等权）
  - [x] 复用机会说明（DrizzlePixel.sumSnrSq 已累加）
  - [x] DrizzlePixel 结构体设计（weight 字段供 stack 加权）
  - [x] 当前实现状态标注

- [x] **§14.8 数据流传递优化检查**（新增决策11的一部分）
  - [x] 4 类检查（冗余/缺失/反复计算/未导出）
  - [x] 已知问题清单

- [x] **文档职责分离重构**（新增决策11）
  - [x] §10 浏览器架构 → UI_ARCHITECTURE.md（根目录新建）
  - [x] §11 性能优化记录 → 各模块 memory.md（dynamic_psf/photometric_calib/healpix_drizzle/healpix_browser_qt）
  - [x] 已归档/废弃模块详细记录 → lib/healpix_db/memory.md
  - [x] §10 改为"文档职责分离说明"（17 行表格指向各文档位置）
  - [x] §11 保留占位说明（维持章节编号连续性）
  - [x] 文档头部更新（移除浏览器/性能描述，添加文档职责分离说明）
  - [x] 所有原 §10/§11 引用改为新位置

### UI_ARCHITECTURE.md（新建）

- [x] 原 §10 浏览器架构全部内容迁移（§1.1-§1.8）
- [x] Qt6+OpenGL 三层架构、球心相机模型、赤道仪相机导航、菱形像素渲染、STF 拉伸、LOD 动态 nside、双击启动部署

### healpix_io/FORMAT_SPEC.md

- [x] .hiss 格式新增 snr 通道说明（ipix + brightness + snr）

### 模块 memory.md 更新

- [x] lib/dynamic_psf/memory.md — §11.1 PSF 性能优化（9.26s→0.26s，-97.1%）
- [x] lib/photometric_calib/memory.md — §11.2+§11.3 photometric 性能优化（354.7s→0.881s，-99.75%）
- [x] lib/healpix_db/healpix_drizzle/memory.md — §11.4 Drizzle 黑色缝隙修复
- [x] lib/healpix_db/healpix_browser_qt/memory.md — §11.5 浏览器性能/视觉修复
- [x] lib/healpix_db/memory.md — 已归档/废弃模块详细记录

### memory.md（根目录）

- [x] 当前进度新增"SNR 模块设计 + §12/§13 设计调整 + 文档职责分离重构（2026-07-15）"条目
- [x] 记录 11 项设计决策摘要（全部已确认）
- [x] 核心设计决策 5 点（SNR 严格独立 / 单次计算 / SNR² 最优加权 / 双层保护 / 叠加执行顺序）
- [x] 文档职责分离重构说明

## 验证

- [x] 文档章节完整性（§1-§14+附录，编号连续，§11 占位说明）
- [x] 设计一致性（SNR-A/SNR-B 区分、双层 SNR 保护、SNR² 最优加权、4 处断层、叠加执行顺序、不耦合证明）
- [x] 文档职责清晰（PROJECT_ARCHITECTURE.md 聚焦数据流/后端，UI 在 UI_ARCHITECTURE.md，性能/归档在模块 memory.md）
- [x] 引用完整性（所有原 §10/§11 引用已改为新位置）
- [x] 与现有代码现状对齐（调研发现记录）
- [x] memory.md 更新

## 不包含

- 实际代码实现（后续逐步实现，每个模块独立 spec）
- drizzle 累加公式修正（待 stack 实现时）
- §12/SNR 模块代码实现（待文档确认后）
- 4 处断层修复实现（待后续独立 spec）
