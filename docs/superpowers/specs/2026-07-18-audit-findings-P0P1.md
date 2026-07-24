# 代码审计问题清单 - P0+P1（Critical + High，必修候选）

> 配套总报告：`2026-07-18-code-audit-report.md`
> 本文档包含 9 模块所有 Critical（19 项）+ High（31 项）= 50 项问题
> 每条问题含【问题定位】（AI 看）+ 【问题描述】（用户看，不带变量名，术语加注释）+ 【批复】（用户填写）
>
> **术语表**：
> - **PipelineFrame**：管线帧，整个流水线在内存中传递数据的容器对象
> - **命名块**：PipelineFrame 内部按名字存取的数据块（如图像数据、FITS 头、星点表等）
> - **WCS**（World Coordinate System）：世界坐标系，描述图像像素坐标 ↔ 天球坐标的映射关系
> - **SIP**（Simple Imaging Polynomial）：简单成像多项式，WCS 的多项式畸变修正
> - **CD 矩阵**：WCS 中的坐标变换矩阵，单位为度/像素
> - **TAN 投影**：gnomonic（心射切面）投影，天文图像常用
> - **HEALPix**：分层等面积等距 iso-latitude 像素化，球面分割方案
> - **NESTED 排序**：HEALPix 像素的嵌套排序方案（与 RING 排序相对）
> - **Gaia DR3SP**：Gaia 第三版光谱数据库（含光谱数据）
> - **Moffat4**：Moffat 函数（β=4）PSF 模型，比高斯更适合描述天文图像 PSF
> - **RANSAC**：随机抽样一致算法，从含异常值的数据中拟合模型
> - **SVD**：奇异值分解，用于求解最小二乘问题
> - **Umeyama 算法**：基于 SVD 的点集配准算法，求解旋转+平移+缩放
> - **MAD**（Median Absolute Deviation）：中位数绝对偏差，稳健的离散度度量
> - **IRLS**：迭代重加权最小二乘，稳健回归方法
> - **Tukey**：Tukey 双权函数，IRLS 中常用的权重函数
> - **IDW**（Inverse Distance Weighting）：反距离加权插值
> - **sigma-clip**：基于标准差的离群值剔除算法
> - **Winsorized sigma-clip**：缩尾 sigma-clip，用分位数替代极端值而非剔除，更稳健
> - **Drizzle**：图像重投影算法，将多帧图像叠加到统一坐标网格
> - **PSF**（Point Spread Function）：点扩散函数，描述恒星在图像中的光强分布
> - **FWHM**（Full Width Half Maximum）：半高全宽，PSF 宽度度量
> - **NESTED ipix**：HEALPix NESTED 排序下的像素索引
> - **npface**：HEALPix 总像素数 = 12×nside²
> - **nside**：HEALPix 分辨率参数，nside 越大分辨率越高
> - **FOV**（Field of View）：视场，图像覆盖的天区范围
> - **gnomonic projection**：心射切面投影，即 TAN 投影
> - **inlier**：内点，符合模型的匹配对（vs outlier 异常值）
> - **PipelineStage**：管线阶段枚举，标识当前执行的处理阶段

---

## 目录

- [B1 astro_image_io](#b1-astro_image_io)
- [B2 calibration](#b2-calibration)
- [B3 plate_solve](#b3-plate_solve)
- [B4 dynamic_psf](#b4-dynamic_psf)
- [B5 photometric_calib](#b5-photometric_calib)
- [B6 snr_estimator](#b6-snr_estimator)
- [B7 healpix_drizzle](#b7-healpix_drizzle)
- [B8 healpix_stack](#b8-healpix_stack)
- [B9 orchestrator](#b9-orchestrator)

---

## B1 astro_image_io

### B1-C-1: 管线阶段枚举仅 5 个，与 9 节点架构不一致

**问题定位**
- 文件：`lib/astro_image_io/include/aio_pipeline.h`
- 涉及：`enum PipelineStage { CALIBRATE, PLATESOLVE, PHOTOMETRIC, DRIZZLE, STACK }` 共 5 个值
- 编排器通过 `#define PipelineStage AioPipelineStage` 复用此枚举

**问题描述**
当前图像 I/O 模块对外暴露的"管线阶段"枚举只定义了 5 个阶段（校准、解析、测光、Drizzle、堆叠），但项目实际是 9 节点两段流水线（含读取 FITS、PSF 拟合、SNR 估算、球面梯度、独立 STACK 等）。这意味着通过编排器传递阶段信息时无法表达新增的 4 个节点，相关日志、断点续传、阶段名查询都会缺失这些节点。

**批复**
- [ ] 同意修复（扩展枚举到 9 节点）
- [ ] 刻意为之（编排器自行用 PipelineStageV2，不依赖此枚举）
- [ ] 暂不修复
- 备注：

---

### B1-H-1: 大量错误日志直接输出到 stderr，未走模块日志系统

**问题定位**
- 文件：`lib/astro_image_io/src/aio_api.cpp`、`aio_pipeline.cpp`、`aio_fits.cpp` 等多处
- 涉及：`fprintf(stderr, "...")` 调用，未调用 `aio_log_*` 系列函数
- 模块已有 `aio_log.cpp` 实现统一日志接口但未被使用

**问题描述**
图像 I/O 模块在多处直接把错误信息打印到标准错误流（stderr），没有走自己已有的日志系统。这导致日志无法按级别过滤、无法落盘到模块日志目录（应为 `lib/astro_image_io/logs/`）、无法在批量处理时归档分析。项目规范要求"每个模块建立日志目录便于分析"。

**批复**
- [ ] 同意修复（替换为 aio_log 调用）
- [ ] 刻意为之（保留 stderr 用于即时调试）
- [ ] 暂不修复
- 备注：

---

### B1-H-2: FITS 关键字段提取不完整，丢失观测元数据

**问题定位**
- 文件：`lib/astro_image_io/src/aio_fits.cpp`
- 涉及：FITS 头解析函数未提取 DATE-OBS（观测时刻）、JD-OBS（儒略日）、BUNIT（数据单位）、LONPOLE/LATPOLE（黄道极）等关键字
- 编排器读取 FITS 后未将这些字段传递给后续阶段

**问题描述**
读取 FITS 文件时，只提取了图像数据和部分头信息，遗漏了观测时刻、数据单位、黄道极坐标等关键字。这些字段对后续测光定标（需要知道曝光时刻做大气消光校正）、单位换算（ADU ↔ 电子数）、坐标系转换都是必需的。丢失后下游模块要么重新读 FITS 头（重复 I/O），要么无法做精确校正。

**批复**
- [ ] 同意修复（补全字段提取）
- [ ] 刻意为之（后续阶段按需自行读 FITS 头）
- [ ] 暂不修复
- 备注：

---

### B1-H-3: XISF 格式错误处理不完整，可能崩溃

**问题定位**
- 文件：`lib/astro_image_io/src/aio_xisf.cpp`
- 涉及：XISF 解析失败时直接 return，未释放已分配资源；部分异常分支无日志

**问题描述**
XISF（PixInsight 原生格式）解析器在遇到格式错误时直接返回，没有清理已经分配的内存和文件句柄，存在资源泄漏和潜在崩溃风险。XISF 作为可选输入格式，错误处理应当稳健——遇到坏文件应明确报错并释放资源，而不是静默失败或崩溃。

**批复**
- [ ] 同意修复（补全错误处理 + 资源释放）
- [ ] 刻意为之（XISF 实际不使用）
- [ ] 暂不修复
- 备注：

---

### B1-H-4: 命名块覆盖语义不清，可能丢失数据

**问题定位**
- 文件：`lib/astro_image_io/src/aio_pipeline_engine.cpp`
- 涉及：`set_block(name, data)` 函数在同名块已存在时的行为（覆盖 / 追加 / 报错）未在文档和代码中明确

**问题描述**
管线帧的"命名块"是模块间传递数据的载体。当两个相邻阶段都输出同名块时（如 CALIBRATE 和 PHOTOMETRIC 都修改 `data` 块），底层 API 应该是覆盖旧值还是报错？目前代码行为不明确，依赖调用方自觉。如果某阶段误用了已存在的块名，可能静默覆盖前一阶段的数据而无任何告警。

**批复**
- [ ] 同意修复（明确覆盖语义 + 加日志）
- [ ] 刻意为之（按覆盖语义使用，调用方负责）
- [ ] 暂不修复
- 备注：

---

## B2 calibration

### B2-C-1: 校准统计信息从未输出（cal_stats 命名块缺失）

**问题定位**
- 文件：`lib/calibration/src/ac_api.cpp`
- 涉及：`ac_correct_frame` 函数无任何 KV 块输出
- 架构契约：ARCHITECTURE.md §4.1 规定 CALIBRATE 输出 `cal_stats (KV)`

**问题描述**
校准模块在校正图像后，本应输出一份"校准统计报告"（包含实际应用的增益系数 K、所用主帧路径、修复的坏点数等），但代码中完全没有此输出。这意味着下游模块和监控系统无法知道校准是否真正执行、用了什么主帧、修复了多少坏点。架构文档明确要求此输出，属于契约破坏。

**批复**
- [ ] 同意修复（在 ac_correct_frame 末尾输出统计 KV）
- [ ] 刻意为之（统计信息暂不需要）
- [ ] 暂不修复
- 备注：

---

### B2-C-2: 坏点修复功能已实现但从未被调用

**问题定位**
- 文件：`lib/calibration/src/ac_api.cpp` + `cosmetic_corrector.cpp`
- 涉及：`cosmetic_corrector` 类实现完整，但 `ac_correct_frame` 主路径未调用

**问题描述**
模块内已完整实现了坏点修复功能（识别热像素、冷像素、坏像素并用邻域中位数替代），但主校准函数从未调用它。这意味着图像中的坏点会一直保留到下游，影响星点检测（坏点可能被误判为恒星）和测光精度。架构文档明确写"CALIBRATE: dark/flat/bias 校准 + 坏点修复"，与实现不符。

**批复**
- [ ] 同意修复（在 ac_correct_frame 中加入坏点修复调用）
- [ ] 刻意为之（坏点修复由其他模块承担，如 PLATESOLVE 前置）
- [ ] 暂不修复
- 备注：

---

### B2-C-3: 无主帧时静默退化，未标记状态（GAP-020 未修复）

**问题定位**
- 文件：`lib/calibration/src/calibrator.cpp`
- 涉及：当 `master_dark/flat/bias` 全为 nullptr 时直接返回原图，无任何日志或状态标记
- GAP 关联：DESIGN_IMPL_GAP.md GAP-020 已记录但未修复

**问题描述**
当用户未提供主暗场、主平场、主偏置帧时（如测试环境或新相机首次拍摄），校准模块会直接返回原图不做任何处理，但没有任何标记告知下游"本次未实际校准"。这导致下游模块（如测光定标）误以为图像已校准，可能产生错误定标结果。应当至少输出明确日志并在统计块中标记"退化模式"。

**批复**
- [ ] 同意修复（补全日志 + 状态标记）
- [ ] 刻意为之（无主帧时直接报错，禁止退化）
- [ ] 暂不修复
- 备注：

---

### B2-C-4: 两套构建脚本产物不一致

**问题定位**
- 文件：`lib/calibration/Makefile` vs `lib/calibration/build.ps1`
- 涉及：两脚本输出 DLL 路径不同、编译选项（如 -O2 vs -O3、是否 -static）不一致

**问题描述**
模块同时存在两套构建脚本（Linux 用 Makefile、Windows 用 build.ps1），但两者生成的 DLL 在路径和编译优化选项上不一致。这会导致不同平台或不同开发者构建出的 DLL 行为可能略有差异（如优化级别影响浮点运算精度），排查问题时容易混淆。应统一为单一权威构建方式。

**批复**
- [ ] 同意修复（统一构建脚本）
- [ ] 刻意为之（两平台各有优化）
- [ ] 暂不修复
- 备注：

---

### B2-H-1: 校准模块线程数硬编码 16，覆盖外部设置

**问题定位**
- 文件：`lib/calibration/src/calibrator.cpp`
- 涉及：`omp_set_num_threads(16)` 硬编码，覆盖了 `ac_set_num_threads` 的外部配置
- 编排器调用 `dll_loader_.set_num_threads(ModuleId::CALIBRATE, threads)` 但被忽略

**问题描述**
校准模块在内部强制使用 16 个 OpenMP 线程，无视外部通过 API 设置的线程数。这意味着用户在配置文件中指定的线程数（如 8 线程以节省 CPU 给其他任务）对校准模块无效。在多任务并行环境（同时处理多帧）下，所有帧都强行用 16 线程会导致 CPU 争抢和性能下降。

**批复**
- [ ] 同意修复（移除硬编码，使用外部设置）
- [ ] 刻意为之（校准必须 16 线程保证性能）
- [ ] 暂不修复
- 备注：

---

### B2-H-2: ac_set_num_threads API 形同虚设

**问题定位**
- 文件：`lib/calibration/src/ac_api.cpp`
- 涉及：`ac_set_num_threads(int n)` 函数仅保存参数到全局变量，calibrator.cpp 内部不读取

**问题描述**
校准模块对外暴露了"设置线程数"的 API，但实际只是把参数存起来不用，内部还是硬编码 16 线程。这是一个误导性 API——调用方以为生效了，实际没有。要么删除此 API，要么让它真正生效。

**批复**
- [ ] 同意修复（让 API 真正生效）
- [ ] 刻意为之（删除此 API）
- [ ] 暂不修复
- 备注：

---

### B2-H-3: 坏点修复功能日志缺失

**问题定位**
- 文件：`lib/calibration/src/cosmetic_corrector.cpp`
- 涉及：修复了多少坏点、修复前后统计等关键信息无日志

**问题描述**
坏点修复功能即使被调用，也没有输出任何日志告知用户修复了多少个坏点、坏点分布如何、修复前后图像统计有何变化。这导致无法验证修复效果，也无法在出现异常时回溯。应当输出"识别 N 个热像素、M 个冷像素，修复完成"等关键日志。

**批复**
- [ ] 同意修复（补全日志）
- [ ] 刻意为之（性能优先，省略日志）
- [ ] 暂不修复
- 备注：

---

### B2-H-4: 主帧生成无质量校验

**问题定位**
- 文件：`lib/calibration/src/master_generator.cpp`
- 涉及：生成主暗场/主平场/主偏置帧后无统计校验（如均值、标准差、坏像素比例）

**问题描述**
主帧（主暗场、主平场、主偏置）由多帧原始图像叠加生成，但生成后没有任何质量校验。例如主平场均值应在 1.0 附近（归一化后），主暗场均值应接近相机热噪声水平。如果某帧原始图像异常（如曝光错误、传感器故障），会污染主帧，但当前代码不会发现。应当输出主帧的统计信息供用户判断质量。

**批复**
- [ ] 同意修复（补全质量校验日志）
- [ ] 刻意为之（质量校验由用户人工目视）
- [ ] 暂不修复
- 备注：

---

## B3 plate_solve

### B3-C-01: 候选星数量上限硬编码为 60，违反"应为 100"的硬约束

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_kvector.cpp` 或 `ipv_select.cpp`
- 涉及：U 组（一组匹配对象）限流常量 `60`
- 违反硬约束："U组限流: max=100 (解决LDN43候选爆炸)"

**问题描述**
在向量匹配阶段，每组候选匹配对象的数量上限被硬编码为 60，但项目明确要求此上限为 100（曾因 LDN43 星云候选数量爆炸问题调整过）。当前设置会过早截断候选，可能丢失真实匹配对，导致解析失败或精度下降。

**批复**
- [ ] 同意修复（改为 100）
- [ ] 刻意为之（60 是当前最优实测值）
- [ ] 暂不修复
- 备注：

---

### B3-C-02: 候选半径用 0.55 倍视场对角线，违反"应为 0.5 倍"硬约束

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_entry.cpp`
- 涉及：候选半径计算 `0.55 * fov_diagonal`
- 违反硬约束："Candidate radius for matching must be 0.5×FOV diagonal"

**问题描述**
从 Gaia 数据库查询候选星时，搜索半径用了图像视场对角线的 0.55 倍，但项目明确要求 0.5 倍。0.55 倍会引入约 10% 的额外候选星，增加噪声匹配对，污染后续 RANSAC 算法。0.5 倍是经过验证的合理值——既能覆盖视场内所有星，又不会引入过多无关星。

**批复**
- [ ] 同意修复（改为 0.5）
- [ ] 刻意为之（0.55 是当前最优实测值）
- [ ] 暂不修复
- 备注：

---

### B3-C-03: RANSAC 缺少比例预检查，导致明显错误匹配污染结果

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp`
- 涉及：RANSAC 迭代中无 scale（缩放因子）预检查
- 违反硬约束："Scale pre-check required during RANSAC: discard pairs with |dU/dW - 1.0| > 0.05"

**问题描述**
RANSAC 算法从候选匹配对中随机抽样拟合变换模型，但当前实现没有在抽样后预检查"缩放比例是否合理"。天文图像的像素尺度与天球坐标的变换缩放应接近 1.0，若某对匹配的缩放偏离 1.0 超过 5%，几乎肯定是错误匹配。缺少此预检查会让明显错误的匹配对参与投票，可能选出错误的最佳模型。

**批复**
- [ ] 同意修复（加入 scale 预检查）
- [ ] 刻意为之（预检查影响性能）
- [ ] 暂不修复
- 备注：

---

### B3-C-04: 精化阶段使用固定阈值，违反"应使用动态阈值"硬约束

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_robust_refine.cpp`
- 涉及：内点判定用固定阈值（如 2.0 像素）
- 违反硬约束："Dynamic inlier threshold (3.0×1.4826×MAD, min 1.0×s0) must be used in refinement stage"

**问题描述**
在精化阶段（RANSAC 后的精细拟合），判定哪些匹配对是"内点"（符合模型的正确匹配）用的是固定阈值。但不同图像的匹配误差分布差异很大——有时 1 像素已是异常值，有时 3 像素仍属正常。硬约束要求用"3 倍 MAD"（中位数绝对偏差，一种稳健的离散度度量）作为动态阈值，自适应不同图像的误差分布。当前固定阈值要么过严（剔除正确匹配）要么过松（保留错误匹配）。

**批复**
- [ ] 同意修复（实现 MAD 动态阈值）
- [ ] 刻意为之（固定阈值实测更稳定）
- [ ] 暂不修复
- 备注：

---

### B3-C-05: RANSAC 内点校验只查位置，违反"应同时查方向"硬约束

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp`
- 涉及：内点校验仅用欧氏距离（位置），无向量叉积（方向）校验
- 违反硬约束："RANSAC inlier validation must use both Euclidean distance (position) and vector cross product (direction) checks"

**问题描述**
判定一对匹配是否为内点时，当前只检查"位置是否接近"（两颗星在图像和天球上的坐标差是否小于阈值），没有检查"方向是否一致"（两颗星组成的向量在图像和天球上的方向是否一致）。位置接近但方向相反的匹配对很可能是巧合的误匹配，应当用向量叉积校验剔除。缺少方向校验会让某些"位置巧合但方向错误"的误匹配被当作内点。

**批复**
- [ ] 同意修复（加入向量叉积校验）
- [ ] 刻意为之（位置校验已足够）
- [ ] 暂不修复
- 备注：

---

### B3-C-06: Umeyama 拟合缺少 5 轮 MAD 离群值剔除

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_itertrans.cpp` 或 `ipv_robust_refine.cpp`
- 涉及：Umeyama SVD 拟合后无 MAD 离群值移除循环
- 违反硬约束："Umeyama fitting must include 5 iterations of MAD outlier removal (threshold: max(5", 3×1.4826×MAD))"

**问题描述**
Umeyama 算法（基于 SVD 的点集配准）对离群值敏感，单次拟合若包含错误匹配对会拉偏结果。硬约束要求拟合后做 5 轮"剔除离群值 → 重新拟合"的迭代，每轮用 MAD（中位数绝对偏差）识别离群值，阈值取 5 角秒和 3 倍 MAD 的较大者。当前实现是一次性拟合无迭代，遇到少量错误匹配对时结果精度会显著下降。

**批复**
- [ ] 同意修复（实现 5 轮 MAD 迭代）
- [ ] 刻意为之（一次性拟合已够用）
- [ ] 暂不修复
- 备注：

---

### B3-C-07: 验证集未限制为 1000 颗最亮 Gaia 星

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_entry.cpp`
- 涉及：Gaia 锥形查询结果直接全部使用，无按星等截断
- 违反硬约束："Validation must use only the 1000 brightest Gaia stars to avoid dark star centroid inaccuracies"

**问题描述**
验证 WCS 解析结果时，使用了从 Gaia 数据库查询到的所有星（可能数万颗），包括大量暗星。暗星在图像上的质心测量不准（信噪比低），用它们验证反而会引入误差。硬约束要求只用最亮的 1000 颗 Gaia 星做验证，确保验证集本身的精度。当前实现违反此约束，验证结果可信度下降。

**批复**
- [ ] 同意修复（按星等升序排序后截断 1000 颗）
- [ ] 刻意为之（验证需要更多样本）
- [ ] 暂不修复
- 备注：

---

### B3-C-08: 解析统计字段硬编码为 0，无法反映实际匹配数

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_solver.cpp`
- 涉及：返回的 `n_detected`（检测到的星数）和 `n_catalog`（用的星表星数）字段硬编码 `0`

**问题描述**
解析模块对外返回的"检测到的星数"和"星表匹配星数"两个字段被硬编码为 0，没有填充实际值。这意味着调用方（编排器）和用户无法从返回值知道本次解析用了多少颗星、匹配了多少对，只能从日志里翻找。这些是基本统计信息，应当正确填充。

**批复**
- [ ] 同意修复（填充实际数值）
- [ ] 刻意为之（统计由日志输出即可）
- [ ] 暂不修复
- 备注：

---

### B3-H-01: 主流程用三角匹配，与文档描述的多边形匹配+PROSAC 不一致

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_entry.cpp`
- 涉及：主流程调用 `triangle_match`，文档 IPV_PIPELINE.md 描述为 `polygon_match + PROSAC`
- 调用链：ipv_solve_from_memory → triangle_match → ...

**问题描述**
解析模块的主流程实际使用的是"三角匹配"算法（用三个星组成三角形做匹配），但模块自己的算法文档（IPV_PIPELINE.md）描述的是"多边形匹配 + PROSAC 采样"算法。这两种是不同的匹配策略——三角匹配简单但易受歧义困扰，多边形匹配+PROSAC 更稳健。代码与文档不一致，让维护者困惑实际用的是哪套算法。

**批复**
- [ ] 同意修复（统一为多边形匹配+PROSAC，对齐文档）
- [ ] 同意修复（更新文档对齐代码现状）
- [ ] 刻意为之（三角匹配是当前最优）
- [ ] 暂不修复
- 备注：

---

### B3-H-02: K-vector 索引构建无验证

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_kvector.cpp`
- 涉及：K-vector 索引（一种快速范围查询数据结构）构建后无完整性校验

**问题描述**
K-vector 是一种用于快速查找特定长度向量的数据结构，构建后没有任何校验（如检查索引是否覆盖所有向量、查询结果是否正确）。如果构建过程有 bug（如排序错误、索引越界），下游匹配会静默失败或返回错误结果。应当至少做一次自检（构建后查询几个已知向量验证）。

**批复**
- [ ] 同意修复（加入自检）
- [ ] 刻意为之（构建算法已验证无 bug）
- [ ] 暂不修复
- 备注：

---

### B3-H-03: 迭代变换收敛判定不合理

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_itertrans.cpp`
- 涉及：收敛阈值固定、最大迭代次数固定、未区分"收敛"与"达到最大迭代"

**问题描述**
迭代变换（反复优化变换参数直到稳定）的收敛判定逻辑不合理：阈值固定不随图像自适应，达到最大迭代次数时直接当作收敛而不管实际是否稳定。这会导致两种问题——一是过早判定收敛（结果未真正稳定），二是无法区分"正常收敛"和"迭代到上限未收敛"（后者应当报错而非当作成功）。

**批复**
- [ ] 同意修复（改进收敛判定）
- [ ] 刻意为之（当前判定实测够用）
- [ ] 暂不修复
- 备注：

---

### B3-H-04: WCS 写回未强制 CD 矩阵无 1/cos(Dec) 因子

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp`
- 涉及：写回 CD 矩阵时未校验是否含 1/cos(Dec) 因子
- 违反硬约束："C++ output must generate standard WCS format CD matrix (without 1/cos(Dec) factor)"

**问题描述**
WCS 标准要求 CD 矩阵（坐标变换矩阵）不含"1/cos(赤纬)"因子，此因子是某些旧式非标准格式用的。当前代码写回 WCS 时没有显式校验，如果内部计算意外引入此因子（如从其他坐标系转换时），会写入非标准 WCS，导致下游模块（如 Drizzle）的坐标转换出错。

**批复**
- [ ] 同意修复（加入显式校验）
- [ ] 刻意为之（内部计算已保证无此因子）
- [ ] 暂不修复
- 备注：

---

### B3-H-05: SIP 多项式阶数自动选择缺失

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_sip.cpp`
- 涉及：SIP 多项式阶数固定或硬编码，无自动选择逻辑

**问题描述**
SIP（简单成像多项式）用于修正 WCS 的高阶畸变，阶数越高能修正越复杂的畸变但拟合越不稳定。当前代码用固定阶数，没有根据图像畸变程度自动选择（如畸变小用 2 阶，畸变大用 4 阶）。这导致简单光学系统被强拟合高阶多项式（过拟合），复杂光学系统又拟合不足。

**批复**
- [ ] 同意修复（实现自动阶数选择）
- [ ] 刻意为之（固定 3 阶实测最优）
- [ ] 暂不修复
- 备注：

---

## B4 dynamic_psf

### B4-C-1: 缺少高斯 PSF 备选方案

**问题定位**
- 文件：`lib/dynamic_psf/src/dpsf_psf.cpp`
- 涉及：仅实现 Moffat4 拟合，无高斯 fallback
- 违反硬约束："PSF 拟合（高斯/Moffat）"

**问题描述**
PSF（点扩散函数，描述恒星在图像中的光强分布）拟合模块只实现了 Moffat4 模型，没有高斯模型作为备选。当 Moffat4 拟合失败时（如恒星过亮饱和、邻近其他星、图像噪声大），没有备选方案，该星只能放弃。硬约束要求支持"高斯/Moffat"两种模型——Moffat 失败时降级用高斯（更简单更稳定），能挽救相当一部分拟合失败的星。

**批复**
- [ ] 同意修复（增加高斯 fallback）
- [ ] 刻意为之（Moffat4 已够稳健）
- [ ] 暂不修复
- 备注：

---

### B4-H-1: PSF 拟合中心位置存在系统偏差约 0.5 像素

**问题定位**
- 文件：`lib/dynamic_psf/src/dpsf_psf.cpp`
- 涉及：`img_cx` 用 patch（图像小块）几何中心，而非传入的 `cx` 参数（恒星实际位置）

**问题描述**
PSF 拟合时，从图像中切出一小块（patch）围绕目标星，然后拟合 PSF 模型。但拟合用的"中心 X 坐标"用的是 patch 的几何中心（如 patch 是 17×17，中心就是 8.5），而不是恒星在 patch 中的实际位置（可能是 8.3 或 8.7）。这导致约 ±0.5 像素的系统偏差，影响 PSF 参数（如 FWHM、振幅）的拟合精度，进而影响测光定标。

**批复**
- [ ] 同意修复（用传入的实际中心位置）
- [ ] 刻意为之（patch 已居中切出，差异可忽略）
- [ ] 暂不修复
- 备注：

---

### B4-H-2: float↔uint16 双重精度损失 + 性能开销

**问题定位**
- 文件：`lib/dynamic_psf/src/dpsf_image.cpp`
- 涉及：输入 float 图像先转 uint16 供检测，再转回 float 供拟合

**问题描述**
PSF 拟合模块在内部把输入的 32 位浮点图像先转成 16 位无符号整数（uint16）做星点检测，再转回浮点做 PSF 拟合。这造成两个问题：一是精度损失（浮点的负值和大值被截断，小数部分被量化），二是性能开销（两次大数组遍历转换）。应当全程保持浮点处理，避免无谓的格式转换。

**批复**
- [ ] 同意修复（全程浮点处理）
- [ ] 刻意为之（uint16 检测更快）
- [ ] 暂不修复
- 备注：

---

## B5 photometric_calib

### B5-H-1: 测光模块未接收 Gaia 星表块，违反架构契约

**问题定位**
- 文件：`lib/photometric_calib/cpp/src/pc_api.cpp`
- 涉及：函数从 Gaia 客户端直接查询，而非从管线帧的 `gaia_cat` 块读取
- 架构契约：ARCHITECTURE.md §4.1 规定 PHOTOMETRIC 输入含 `gaia_cat`

**问题描述**
测光定标模块本应从管线帧的"Gaia 星表"命名块（PLATESOLVE 阶段已经查询并写入）读取 Gaia 星数据，但实际是直接调用 Gaia 客户端重新查询。这导致两个问题：一是重复查询（PLATESOLVE 已查过一次，PHOTOMETRIC 又查一次），浪费时间和数据库资源；二是数据不一致（两次查询可能用不同筛选条件，得到不同星表）。

**批复**
- [ ] 同意修复（改为从 gaia_cat 块读取）
- [ ] 刻意为之（重新查询保证最新数据）
- [ ] 暂不修复
- 备注：

---

### B5-H-2: 光谱积分步长 1.0nm，与算法文档要求的 0.1nm 不一致

**问题定位**
- 文件：`lib/photometric_calib/cpp/src/spectrum_integrator.cpp`
- 涉及：积分步长硬编码 `1.0` nm
- 文档要求：algorithm.md 规定 0.1 nm

**问题描述**
计算"合成流量"（F_syn = ∫S(λ)·T(λ)·Q(λ)dλ，即恒星光谱 × 望远镜透过率 × 相机量子效率的积分）时，积分步长用了 1.0 纳米，但算法文档要求 0.1 纳米。步长越大积分越粗糙——1.0 nm 步长可能跨越多个光谱特征（如发射线、吸收线），丢失细节导致 F_syn 计算不准。0.1 nm 步长能更精细捕捉光谱特征。

**批复**
- [ ] 同意修复（改为 0.1 nm）
- [ ] 刻意为之（1.0 nm 实测精度够用）
- [ ] 暂不修复
- 备注：

---

## B6 snr_estimator

### B6-H-1: 架构文档数据流表未更新（snr 块 vs snr_model 块）

**问题定位**
- 文件：`docs/ARCHITECTURE.md` §4.1 数据流表
- 涉及：表格仍写 `snr (FLOAT32[H,W])`，实际代码已改为 `snr_model (RAW)` 块

**问题描述**
架构文档的数据流表里写 SNR 阶段输出的是"snr 浮点图像"（每个像素一个 SNR 值），但实际代码已经改成输出"snr_model 原始模型"（稀疏控制点 + IDW 插值参数）。这是之前 GAP-011 修复后的代码层变更，但文档没同步。新开发者读文档会误以为 SNR 是图像格式，实际是模型格式。

**批复**
- [ ] 同意修复（更新文档为 snr_model）
- [ ] 刻意为之（保留旧描述作历史参考）
- [ ] 暂不修复
- 备注：

---

### B6-H-2: GAP-011 状态未更新为"已修复"

**问题定位**
- 文件：`docs/DESIGN_IMPL_GAP.md` GAP-011 条目
- 涉及：状态字段仍标"待修复"，实际代码层已修复

**问题描述**
DESIGN_IMPL_GAP.md 文档记录了设计与实施割裂的问题清单，其中 GAP-011（SNR 块格式不一致）的状态仍标为"待修复"，但实际代码已经修复（snr_model 块替代 snr 浮点图像）。文档与代码状态不同步，让维护者误以为还有未修复工作。

**批复**
- [ ] 同意修复（更新 GAP-011 状态为"代码层已修复"）
- [ ] 刻意为之（保留待修复状态等更多验证）
- [ ] 暂不修复
- 备注：

---

## B7 healpix_drizzle

> 本模块所有 Critical / High 均为 0 项，硬约束全部满足。无批复项。

---

## B8 healpix_stack

### B8-C-1: 球面梯度校正在回退路径丢失 Winsorized 参数

**问题定位**
- 文件：`lib/healpix_db/healpix_stack/hp_stack_api.cpp:510-511, 534-535`
- 涉及：`hp_stack_gradient_corrected` 回退分支调用 `hp_stack_hiss` 时未透传 `use_winsorized / winsorize_low_pct / winsorize_high_pct`
- `hp_stack_hiss` 签名本身不含 Winsorized 参数

**问题描述**
球面梯度校正功能在采样失败或拟合失败时，会回退到简单的多帧堆叠路径。但回退时没有把用户配置的"Winsorized sigma-clip"（一种更稳健的离群值剔除方法）参数传递过去，导致回退路径只能用普通 sigma-clip。如果用户特意配置了 Winsorized 想要更稳健的离群值处理，一旦回退就静默降级为普通方法，与用户期望不符。

**批复**
- [ ] 同意修复（扩展 hp_stack_hiss 签名，回退时完整透传）
- [ ] 同意修复（回退时报错而非降级）
- [ ] 刻意为之（回退路径故意简化）
- [ ] 暂不修复
- 备注：

---

### B8-H-1: 梯度拟合最近控制点查找是 O(n²) 性能瓶颈

**问题定位**
- 文件：`lib/healpix_db/healpix_stack/gradient/gradient_fitter.cpp:152-204`
- 涉及：`fit` 函数为每个控制点遍历所有其他帧的所有控制点找最近邻，复杂度 O(N_frames × N_ctrl²)
- 代码注释承认"对 n=500, 10帧 ~ 2.5s"

**问题描述**
梯度曲面拟合时，需要为每个控制点找到其他帧中距离最近的控制点。当前用暴力遍历方式（两两计算距离比较），复杂度是 O(n²)——500 个控制点、10 帧时约 2.5 秒。当帧数或控制点数增加（如 50 帧 × 2000 控制点），耗时可达分钟级。应当用 KD-tree（一种空间划分数据结构，可将对数级查询）加速到 O(N log N)。

**批复**
- [ ] 同意修复（用 nanoflann KD-tree 加速）
- [ ] 刻意为之（当前数据量可接受）
- [ ] 暂不修复
- 备注：

---

### B8-H-2: 文档说"Gauss-Seidel 迭代拟合"，实际是一次性拟合

**问题定位**
- 文件：
  - `lib/healpix_db/healpix_stack/hp_stack_api.cpp:443`
  - `lib/healpix_db/healpix_stack/hp_stack_api.h:99, 108`
  - `lib/healpix_db/healpix_stack/healpix_stack.py:422, 435`
  - `lib/healpix_db/healpix_stack/gradient/gradient_sampler.h:9`
- 涉及：多处文档/注释提到 "Gauss-Seidel 拟合" 和 "gradient_max_iter: Gauss-Seidel 最大迭代"
- 实际：`gradient_fitter.h:19` 和 `gradient_fitter.cpp:8, 93` 明确"3D 嵌入球面样条拟合（一次性, 无 Gauss-Seidel 迭代）"

**问题描述**
模块的多处文档和 API 注释提到用"Gauss-Seidel 迭代"（一种迭代求解线性方程组的方法）做梯度拟合，参数 `gradient_max_iter` 描述为"Gauss-Seidel 最大迭代次数"。但实际代码是一次性球面样条拟合，根本没有任何迭代。这个参数是个死参数（传了但不被读取）。文档与实现严重不符，误导维护者。

**批复**
- [ ] 同意修复（统一文档术语为"一次性球面样条拟合"）
- [ ] 同意修复（按文档实现 Gauss-Seidel 迭代）
- [ ] 刻意为之（保留旧术语作历史参考）
- [ ] 暂不修复
- 备注：

---

### B8-H-3: STACK 阶段是空骨架，与架构文档不符（GAP-015 未修复）

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:2461-2476`
- 涉及：`run_stage_stack` 函数体仅 LOG_INFO 后 return true
- 架构契约：ARCHITECTURE.md §2 / PROJECT_OVERVIEW.md §3 将 stage 8 STACK 描述为独立阶段
- GAP 关联：DESIGN_IMPL_GAP.md GAP-015

**问题描述**
架构文档把 stage 8 STACK 描述为独立的"堆叠"阶段（负责 Winsorized sigma-clip + SNR²加权叠加输出 .hcsd），但实际代码中 STACK 是个空函数——只打印一行日志说"跳过，.hcsd 已由 GRADIENT_SPHERE 生成"。也就是说 GRADIENT_SPHERE 阶段实际承担了文档里 STACK 的全部职责，STACK 节点存在但无实际逻辑。架构与实现严重不符。

**批复**
- [ ] 同意修复（拆分函数，STACK 接管最后的 .hcsd 输出）
- [ ] 同意修复（更新文档明确 stage7+8 合并为单函数）
- [ ] 刻意为之（STACK 节点保留作未来扩展占位）
- [ ] 暂不修复
- 备注：

---

### B8-H-4: .hiss 多帧堆叠路径无 Winsorized 选项

**问题定位**
- 文件：`lib/healpix_db/healpix_stack/hp_stack_hiss.cpp:263-329`
- 涉及：sigma-clip 迭代只实现普通 mean+std，无 Winsorized 分支

**问题描述**
.hiss 多帧堆叠函数只实现了普通 sigma-clip（基于均值和标准差剔除离群值），没有 Winsorized sigma-clip（用分位数替代极端值，更稳健）的选项。当球面梯度校正回退到这个路径时（见 B8-C-1），即使用户配置 Winsorized 也无法生效。

**批复**
- [ ] 同意修复（增加 Winsorized 分支）
- [ ] 刻意为之（普通 sigma-clip 已够用）
- [ ] 暂不修复
- 备注：

---

### B8-H-5: 旧版堆叠引擎用硬编码 3 次迭代，无 Winsorized

**问题定位**
- 文件：`lib/healpix_db/healpix_stack/stack_engine.cpp:57-95, 126-128`
- 涉及：`sigmaClip` 函数迭代次数硬编码 3，无 Winsorized 分支，无 max_iter 参数

**问题描述**
旧版堆叠引擎（用于已废弃的 .ahps 格式）的 sigma-clip 实现简陋：迭代次数硬编码 3 次（不可配置），无 Winsorized 选项。虽然 .ahps 格式已废弃，但代码还在维护，若误调用会得到与新路径不一致的结果。建议标记为 deprecated 或与新版对齐。

**批复**
- [ ] 同意修复（标记 deprecated 或对齐新版）
- [ ] 刻意为之（旧代码保留作参考）
- [ ] 暂不修复
- 备注：

---

## B9 orchestrator

### B9-C-1: CALIBRATE 阶段未写入"校准统计"命名块（与 B2-C-1 同源）

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:699-787`
- 涉及：`run_stage_calibrate` 函数全程未调用 `aio_frame_kv_set(frame_, "cal_stats", ...)`
- Grep 整个 lib/orchestrator/cpp/ 无任何 `cal_stats` 字符串引用
- 架构契约：ARCHITECTURE.md §4.1 规定 CALIBRATE 输出 `cal_stats (KV)`

**问题描述**
编排器在执行 CALIBRATE 阶段后，没有把校准统计信息（如实际增益系数、所用主帧路径、修复的坏点数）写入管线帧的"校准统计"命名块。整个编排器代码中找不到 `cal_stats` 这个名字的引用。这意味着下游模块和监控系统完全无法获取校准状态，架构契约被破坏。此问题与 B2-C-1（校准模块未输出统计）是同源问题——模块没输出，编排器自然没写入。

**批复**
- [ ] 同意修复（与 B2-C-1 一并修复，编排器接收后写入 KV 块）
- [ ] 刻意为之（统计信息暂不需要）
- [ ] 暂不修复
- 备注：

---

### B9-C-2: 编排器调用 Gaia 查询未限制 1000 颗最亮星（与 B3-C-07 同源）

**问题定位**
- 文件：
  - `lib/orchestrator/cpp/src/orchestrator.cpp:1515`（PLATESOLVE: `mag_high=18.0`）
  - `lib/orchestrator/cpp/src/orchestrator.cpp:1919`（PHOTOMETRIC: `mag_min=6.0, mag_max=16.0`）
- 涉及：调用 `gaia_client_cone_search_for_solver` 和 `pc_calibrate_simple_with_gaia` 后无按星等截断逻辑
- 违反硬约束："Validation must use only the 1000 brightest Gaia stars"

**问题描述**
编排器在 PLATESOLVE 和 PHOTOMETRIC 阶段调用 Gaia 数据库查询时，没有"只取最亮的 1000 颗"的截断逻辑。对于宽视场图像（如视场 10 度），可能查询到数万颗 Gaia 星，其中包含大量暗星。暗星在图像上的质心测量不准（信噪比低），用它们做验证和定标会引入误差。硬约束明确要求只用最亮的 1000 颗。

**批复**
- [ ] 同意修复（查询后按星等升序排序截断 1000 颗）
- [ ] 刻意为之（需要更多样本保证统计意义）
- [ ] 暂不修复
- 备注：

---

### B9-C-3: 任务队列大小限制为 2 未实现

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp`（全文）
- 涉及：Grep `queue|task_queue|backpressure|max_queue` 在整个 lib/orchestrator/cpp/ 下无匹配
- 违反硬约束："Task queue size limited to 2 to control backpressure"

**问题描述**
项目硬约束要求"任务队列大小限制为 2"（控制背压，防止任务堆积），但编排器代码中完全没有任务队列抽象。当前是串行执行各阶段（一个 stage 完了接下一个），不需要队列。但约束是为了未来并行调度准备（如多帧并行执行 stage1），届时若无队列限制会导致内存爆炸。约束未实现。

**批复**
- [ ] 同意修复（实现 TaskQueue，预留并行扩展点）
- [ ] 同意修复（文档明确约束适用未来并行模式）
- [ ] 刻意为之（当前串行不需要）
- [ ] 暂不修复
- 备注：

---

### B9-C-4: PLATESOLVE/PSF 阶段像素值截断丢失精度

**问题定位**
- 文件：
  - `lib/orchestrator/cpp/src/orchestrator.cpp:1431-1438`（PLATESOLVE: pixels float→uint16）
  - `lib/orchestrator/cpp/src/orchestrator.cpp:1612-1617`（PSF: pixels float→uint16）
- 涉及：`if (v < 0.0f) v = 0.0f; if (v > 65535.0f) v = 65535.0f; pixels_u16[i] = static_cast<uint16_t>(v);`
- 根因：star_detector API 仅接受 uint16 输入（`star_detector.h:49-54` 确认无 float 版本）

**问题描述**
PLATESOLVE 和 PSF 阶段调用星点检测器时，需要把浮点图像转成 16 位无符号整数。但校准后的浮点图像可能含负值（减暗场后）或大于 65535 的值（平场放大），代码用截断方式处理（负值变 0，超 65535 截到 65535），丢失精度。更糟的是，星点检测器只接受 uint16 输入，没有浮点版本，导致精度损失不可避免。这会影响星点检测准确度和 PSF 拟合精度。

**批复**
- [ ] 同意修复（扩展星点检测器 API 支持 float 输入）
- [ ] 同意修复（转换时记录截断统计 + LOG_WARN）
- [ ] 刻意为之（uint16 精度够用）
- [ ] 暂不修复
- 备注：

---

### B9-H-1: 配置文件字段解析不完整，多个配置项无法生效

**问题定位**
- 文件：
  - `lib/orchestrator/cpp/src/orchestrator.cpp:266-323`（`load_config` 仅存原文）
  - `lib/orchestrator/cpp/src/cli_command.cpp:380-413`（`cmd_stage1` 仅注释"后续 Task 解析"）
  - 配置文件 `lib/orchestrator/configs/stage1_config.json` 含 project_root、gaia_data_dir、calibration_dir、output_root、stages、frame、drizzle、log_level、threads 等字段
- 涉及：`calibration_dir`、`filter`、`threads`、`gaia_data_dir`、`output_root` 等字段完全未读取

**问题描述**
配置文件 stage1_config.json 里定义了很多配置项（校准目录、滤光片名、线程数、Gaia 数据目录、输出根目录等），但编排器的配置加载函数只把整个 JSON 文本存起来，没有解析这些字段。后续 stage handler 用手工方式提取了少数几个字段（如 nside_strategy、sigma_clip_method），但大部分字段（如校准目录、滤光片名、线程数）完全没读取，导致这些配置项对用户无效——用户在配置文件里改了，实际不影响运行。

**批复**
- [ ] 同意修复（引入 nlohmann::json 统一解析所有字段）
- [ ] 同意修复（手工补全所有字段提取）
- [ ] 刻意为之（当前字段已够用）
- [ ] 暂不修复
- 备注：

---

### B9-H-2: PHOTOMETRIC 阶段创建未使用的星检测器/解析器句柄（资源浪费）

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:1739-1747`
- 涉及：`run_stage_photometric` 为复用 `gaia_client_handle_` 调用 `init_platesolve_env`
- `init_platesolve_env` 内部（行 1098-1153）会创建 GaiaClient + StarDetector + IPVSolver 三个实例

**问题描述**
PHOTOMETRIC 阶段需要用 Gaia 客户端查 Gaia 星，但为了复用 Gaia 客户端句柄，调用了 PLATESOLVE 的初始化函数。这个函数除了创建 Gaia 客户端，还会创建星点检测器和解析器实例——但 PHOTOMETRIC 完全不需要这两个。这造成资源浪费（每个实例都占内存）和模块耦合（PHOTOMETRIC 强依赖 PLATESOLVE DLL 已加载）。

**批复**
- [ ] 同意修复（拆分为三个独立初始化函数）
- [ ] 同意修复（懒加载，仅在实际调用时创建）
- [ ] 刻意为之（统一初始化简化代码）
- [ ] 暂不修复
- 备注：

---

### B9-H-3: Gaia API 返回值约定不统一（布尔 vs 错误码）

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:952-978`
- 涉及：`build_spectrum_wl` 中 `if (ret != 1 || count <= 0 || step_nm <= 0)`，注释说"使用布尔约定 (1=成功, 0=失败)"
- 其他 API（如 `gaia_client_cone_search_for_solver`）返回 0=成功（行 1518 `if (gaia_ret == 0 && out_count > 0 ...)`）

**问题描述**
Gaia 客户端 API 的返回值约定不统一：有的函数用"1=成功，0=失败"（布尔约定），有的用"0=成功，非0=错误码"（错误码约定）。调用方很容易误判——比如把"1=成功"的函数当成"0=成功"处理，会认为调用失败而实际成功。代码注释已意识到此问题但未根本解决。

**批复**
- [ ] 同意修复（统一为错误码约定，0=成功）
- [ ] 同意修复（在每个 API 注释中明确返回值约定）
- [ ] 刻意为之（保持现状）
- [ ] 暂不修复
- 备注：

---

### B9-H-4: STACK 阶段空骨架（与 B8-H-3 同源）

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:2462-2476`
- 涉及：`run_stage_stack` 函数体仅 `LOG_INFO("[STACK] 跳过: .hcsd 已由 GRADIENT_SPHERE 生成") + return true;`
- 架构契约：ARCHITECTURE.md §4.1 规定 STACK 输出 `.hcsd`，GRADIENT_SPHERE 输出 `—`（无输出块）

**问题描述**
（与 B8-H-3 同一问题从编排器侧描述）编排器的 STACK 阶段是空函数，只打印日志就返回成功。架构文档说 STACK 输出 .hcsd，但实际 .hcsd 是 GRADIENT_SPHERE 阶段生成的，与文档描述相反。STACK 节点存在但无逻辑，stage2 的成功标志永远为 true，无法反映 STACK 实际状态。

**批复**
- [ ] 同意修复（拆分函数，STACK 接管 .hcsd 输出）
- [ ] 同意修复（更新文档反映实际）
- [ ] 刻意为之（保留 STACK 节点作扩展占位）
- [ ] 暂不修复
- 备注：

---

### B9-H-5: 测光定标函数有 41 个参数，维护性极差

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:1880-1930`
- 涉及：`pc_calibrate_simple_with_gaia` 函数原型 41 个参数（gaia handle、FOV 中心+半径、mag 范围、filter 数组、QE 数组、spectrum 数组、image+PSF、WCS 8 参数、SIP 4 指针+order、输出 out_pixels+n_matched+scale+sigma）

**问题描述**
测光定标函数的参数多达 41 个，调用时极易传错位置（如把星等上限当成下限）。代码无运行时参数顺序校验，一旦传错难调试。这种"超长参数列表"是典型的代码坏味道，应重构为参数结构体（把相关参数分组到结构体，如 GaiaFilter、QECurve、WCSParams、SIPParams 等），调用方填充结构体后传指针。

**批复**
- [ ] 同意修复（重构为参数结构体）
- [ ] 同意修复（至少在注释中明确每参数位置和单位）
- [ ] 刻意为之（重构风险高，保持现状）
- [ ] 暂不修复
- 备注：

---

### B9-H-6: PLATESOLVE 环境清理依赖 DLL 加载状态（隐式耦合）

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:1172-1178`
- 涉及：`if (ipv_solver_handle_ != nullptr && dll_loader_.is_loaded(ModuleId::PLATESOLVE)) fn_ipv_destroy(ipv_solver_handle_);`
- `fn_ipv_destroy` 是从 PLATESOLVE DLL 通过 `dll_loader_.get_function` 获取的函数指针

**问题描述**
PLATESOLVE 环境清理函数在销毁解析器实例时，依赖 DLL 加载器的状态——如果 PLATESOLVE DLL 已被卸载，就不销毁。但当前析构顺序是"先清理 PLATESOLVE 环境，再自动析构 DLL 加载器"，正常情况下没问题。但如果未来调用顺序变更（如手动卸载 DLL 后再清理），会导致解析器实例泄漏或 use-after-free。耦合度高，鲁棒性差。

**批复**
- [ ] 同意修复（独立加载 DLL 获取销毁函数，不依赖加载器状态）
- [ ] 同意修复（用智能指针 + 自定义 deleter 自动管理）
- [ ] 刻意为之（当前调用顺序安全）
- [ ] 暂不修复
- 备注：

---

### B9-H-7: Gaia 查询返回内存用 std::free 释放，违反封装

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:1543-1545`
- 涉及：`std::free(out_ra); std::free(out_dec); std::free(out_mag);`
- 根因：`gaia_client.c:1510` 用 `malloc` 分配，但 `gaia_client.h` 无 export 专门的 free 函数

**问题描述**
编排器释放 Gaia 查询返回的内存时，直接用 `std::free`（C 标准库函数）。这之所以能工作，是因为 Gaia 客户端内部用 `malloc` 分配内存——但这个实现细节没有在头文件中明确。如果 Gaia 客户端未来改用 `new[]` 或自定义分配器，编排器立即崩溃。跨 DLL 内存管理应当封装——提供专门的释放函数，让调用方不依赖内部实现。

**批复**
- [ ] 同意修复（在 gaia_client.h 新增专门的释放函数）
- [ ] 同意修复（在头文件注释中明确"必须用 free() 释放"）
- [ ] 刻意为之（保持现状）
- [ ] 暂不修复
- 备注：

---

## 批复汇总表

> 用户审阅完上述 50 项问题后，可在下表汇总各问题的批复决定，便于后续修复决策。

| 问题 ID | 模块 | 严重度 | 批复（同意修复 / 刻意为之 / 暂不修复） | 备注 |
|---------|------|--------|----------------------------------------|------|
| B1-C-1 | astro_image_io | Critical | | |
| B1-H-1 | astro_image_io | High | | |
| B1-H-2 | astro_image_io | High | | |
| B1-H-3 | astro_image_io | High | | |
| B1-H-4 | astro_image_io | High | | |
| B2-C-1 | calibration | Critical | | |
| B2-C-2 | calibration | Critical | | |
| B2-C-3 | calibration | Critical | | |
| B2-C-4 | calibration | Critical | | |
| B2-H-1 | calibration | High | | |
| B2-H-2 | calibration | High | | |
| B2-H-3 | calibration | High | | |
| B2-H-4 | calibration | High | | |
| B3-C-01 | plate_solve | Critical | | |
| B3-C-02 | plate_solve | Critical | | |
| B3-C-03 | plate_solve | Critical | | |
| B3-C-04 | plate_solve | Critical | | |
| B3-C-05 | plate_solve | Critical | | |
| B3-C-06 | plate_solve | Critical | | |
| B3-C-07 | plate_solve | Critical | | |
| B3-C-08 | plate_solve | Critical | | |
| B3-H-01 | plate_solve | High | | |
| B3-H-02 | plate_solve | High | | |
| B3-H-03 | plate_solve | High | | |
| B3-H-04 | plate_solve | High | | |
| B3-H-05 | plate_solve | High | | |
| B4-C-1 | dynamic_psf | Critical | | |
| B4-H-1 | dynamic_psf | High | | |
| B4-H-2 | dynamic_psf | High | | |
| B5-H-1 | photometric_calib | High | | |
| B5-H-2 | photometric_calib | High | | |
| B6-H-1 | snr_estimator | High | | |
| B6-H-2 | snr_estimator | High | | |
| B8-C-1 | healpix_stack | Critical | | |
| B8-H-1 | healpix_stack | High | | |
| B8-H-2 | healpix_stack | High | | |
| B8-H-3 | healpix_stack | High | | |
| B8-H-4 | healpix_stack | High | | |
| B8-H-5 | healpix_stack | High | | |
| B9-C-1 | orchestrator | Critical | | |
| B9-C-2 | orchestrator | Critical | | |
| B9-C-3 | orchestrator | Critical | | |
| B9-C-4 | orchestrator | Critical | | |
| B9-H-1 | orchestrator | High | | |
| B9-H-2 | orchestrator | High | | |
| B9-H-3 | orchestrator | High | | |
| B9-H-4 | orchestrator | High | | |
| B9-H-5 | orchestrator | High | | |
| B9-H-6 | orchestrator | High | | |
| B9-H-7 | orchestrator | High | | |

---

**统计**：Critical 19 项 + High 31 项 = 50 项

完成批复后请返回本文件，将基于批复结果生成修复 spec + checklist。
