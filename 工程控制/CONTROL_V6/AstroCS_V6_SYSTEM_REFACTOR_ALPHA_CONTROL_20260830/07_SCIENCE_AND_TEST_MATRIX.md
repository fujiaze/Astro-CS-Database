# 科学推导与数值验证矩阵

## 1. 验收逻辑

科学正确性不由旧版本输出决定。每个模块按以下顺序验收：

1. SCI 给连续/统计定义、单位和适用边界；
2. ALG 从 SCI 推导离散式、近似和误差；
3. DATA/API 把每个量映射到字段、类型、坐标和所有权；
4. independent oracle/解析解验证实现；
5. 性质测试验证不变量；
6. scalar/parallel/AVX2/AVX-512 在同一合同下等价；
7. 少量真实数据只用于发现模型覆盖不到的场景；
8. 最终 32R 验证产品级结果。

任何公式若在现有正式 SCI 文档中有不同定义，执行 Agent不得自行选择“看起来合理”的版本：记录冲突和两套结果，停止该科学模块，继续不依赖它的架构任务。

## 2. 通用误差规则

- 元数据、mask、计数、索引、端口和选择结果：精确一致。
- 可逐元素解析的 FP64 非归约：默认 `rtol=1e-12`，`atol=1e-13 × scale`。
- FP32 产品非归约：默认 `rtol=5e-6`，`atol=1e-6 × scale`。
- 归约不使用一个任意大常数。ALG 必须给求和项数 `n`、machine epsilon `u` 和误差模型。使用
  `gamma_n = n*u/(1-n*u)`，门限为 `C*gamma_n*sum(abs(terms)) + atol`，`C` 必须在测试前冻结且通常不大于 4。
- 并行归约若需要 deterministic，使用固定分块/树形合并；否则文档明确 reproducible-within-bound，不能声称 bitwise deterministic。
- NaN/Inf/missing 的位置和语义精确一致，不允许仅比较 finite pixels。
- 容差调整必须是独立 SCI/TEST commit，附失败分布和推导；不能与实现修复同 commit。

## 3. Phase1

| 模块 | 科学定义/推导必须回答 | 合成数据与独立 Oracle | 不变量/失败门 |
|---|---|---|---|
| Calibration | master bias/dark 是否已去 bias；曝光缩放；flat normalization；ADU↔electron；负值 | 常量/梯度 bias、dark、flat；解析逐像素公式；多 bit depth | 不裁切负值；flat=0/masked 拒绝或标 invalid；单位正确 |
| Cosmetic | 坏点定义、邻域/PSF 假设、mask 传播 | 已知孤立坏点/列、天体结构、纯噪声 | 不把真实点源当坏点；mask/provenance 保留 |
| Star detect | 背景/噪声阈值、连通域、饱和/边界 | Gaussian/Moffat 星场 + 可控噪声；已知 truth catalog | completeness/false positives；deterministic tie |
| PSF | PSF 模型、参数单位、拟合权重、质量 | 已知 centroid/FWHM/ellipticity 的 analytic PSF | 参数误差门；失败不输出伪有效值；重复 PSF 不发生 |
| Plate solve | pixel/WCS 坐标、projection/distortion、匹配/残差 | 从已知 WCS 生成星场再求解 | pixel→sky→pixel roundtrip；错误初值和无解明确失败 |
| Photometry | aperture/PSF flux、背景、曝光/增益、误差 | 已知 flux 星场与背景；解析 aperture reference | flux conservation；“解析成功但积分失败”回归 |
| Noise/SNR | shot/read/dark/sky variance；variance/ivar；SNR 定义 | Poisson+Gaussian fixed-seed Monte Carlo + analytic moments | variance 非负且单位平方；ivar=0 对 invalid；blank sky/低信号 |
| NSIDE | 输入角分辨率、过采样、tile 限制 | 多 pixel scale 和 projection | 结果在冻结的 1–2× 过采样范围；不硬编码 |
| Drizzle | drop footprint、pixfrac、权重、support、输出是 flux 还是 surface brightness | constant field、impulse、subpixel shift、rotation、mask、RA wrap | constant/flux/centroid/support；FP64 accumulator；无摩尔纹异常放大 |
| HiPS write | HEALPix ordering、tile hierarchy、properties、units | 小 NSIDE 完整球/局部 patch | tile tree、coverage、hash、重新打开一致 |

### Calibration 公式门

SCI-001 必须从现有 master 定义中选择并明确唯一公式。例如 master dark 已 bias-subtracted 时：

`C = (R - B - alpha*D) / F_norm`

若 master dark 含 bias，则不能套用该式，必须在 SCI 写出正确展开式并让 DATA 标记 master 的生成语义。代码中不得通过文件名猜是哪一种。

### Noise/SNR 最低推导

若以 electron 表示，独立像素最小模型应显式区分：

`Var[e-] = source_e + sky_e + dark_e + N_read * sigma_read_e^2`

实际实现可有更完整的校准/重采样协方差项，但每一项必须注明来源、单位和是否忽略。`SNR = signal / sqrt(variance)` 中 signal 和 variance 必须处于一致单位。质量权重不得冒充 inverse variance。

### Drizzle 最低推导

对输入样本 `s_i` 与输出像素 `o` 的重叠系数 `a_io`、统计权重 `w_i`，至少分别定义累计 numerator、denominator/support：

`N_o = Σ a_io w_i s_i`, `W_o = Σ a_io w_i`, `I_o = N_o/W_o (W_o>0)`。

若产品需要总 flux 而非归一化 surface-brightness，必须是另一明确 DATA 产品和公式，不能靠 writer 临时改变归一化。

## 4. Phase2

| 模块 | 合成场景 | 核心指标 | 硬失败条件 |
|---|---|---|---|
| Coverage | 三 patch：连通、断连、只边接触 | overlap graph 与解析几何一致 | 漏边/伪边；空覆盖被当有效 |
| Sampler/control | 背景+星/星云+mask+不同 SNR | control 分布、质量、determinism | 明亮结构污染背景控制点；workers=1 生产路由 |
| Additive UPM | 各 patch 注入常数/线性/低阶平滑加性面 | overlap median/RMS/gradient 降低；源 flux ratio≈1 | 乘性改变；黑洞；断图漂移；过拟合真实结构 |
| Block plan/apply | 同一小问题 full 与分块 | elementwise error bound；峰值 RAM | 重复/漏算；稠密全局缓存；边界接缝 |
| Rejection | Gaussian noise + hot pixel/cosmic/streak；真实星核 | precision/recall；source preservation | 自动方法无 reason；低样本非法运行；卫星线全保留 |
| Integration | 已知常量/梯度/variance/support | mean/weighted mean/variance 解析值 | frame identity 丢失；权重单位混淆；zero support 造值 |
| HiPS output | mosaic+support+UPM+reject map | tree/header/hash/coverage | 只输出最终图而无诊断；partial tree 误标成功 |

### 加性接缝模型最低推导

重叠区域控制点 `x_k` 对图像 `i,j` 提供差值约束。拟合每图加性背景模型 `b_i(x)`：

`min_b Σ_(i,j,k) w_ijk [(y_i(x_k)-b_i(x_k))-(y_j(x_k)-b_j(x_k))]^2 + λR(b)`

并施加 gauge（固定一幅或零和）以消除整体加性退化。断连覆盖图按连通分量独立处理并标 provenance。`b_i` 只校正背景，不参与 integration weight。合成验证必须把恒星/扩展结构加入重叠区，证明其 flux 和形态未被背景拟合吞掉。

## 5. Phase3

| 测试 | 输入 | Oracle/性质 | 门禁 |
|---|---|---|---|
| Constant sphere | 所有有效 HEALPix sample 常数 c | coverage 内输出 c | 无条纹/接缝；边界不造值 |
| Analytic sky | `f(lon,lat)` 平滑解析函数 | output WCS pixel center 反算 sky 后直接求 f | 插值误差在 ALG 界内 |
| Impulse | 单/小簇 HEALPix 非零 | centroid、support、kernel footprint | 不复制/丢失；RA wrap 正确 |
| WCS roundtrip | 多 projection/rotation/scale | pixel→sky→pixel | 误差门内；FITS keywords 完整 |
| Pole/wrap | 跨 0/360 与极区 patch | 连续性与 coverage | 不越界/翻转/空洞 |
| Units | ADU、electron、surface brightness、unknown | identity resample 不改变单位；显式转换才改变 | 禁止硬编码 Jy/beam |
| Parallel | 同一 tiling 1-worker test reference 与 N-worker production | deterministic 或 error-bound | production large task active workers≥2 |

## 6. 后端与并行等价

每个注册为 CPU-heavy 的 kernel 必须使用同一 fixture 依次运行：

1. high-precision/analytic oracle；
2. baseline 1-worker（仅测试参考）；
3. baseline Runtime N-worker；
4. AVX2 N-worker（支持时）；
5. AVX-512 N-worker（支持且实现时）。

先比较正确性，错误则不计性能。每份报告记录 backend id/build id、ISA feature、worker count、block、编译 flags、误差分布、非 finite、wall/cpu time 和 resource summary。

## 7. 真实数据策略

- Linux：每块最多一帧 R + 必需 masters，作为解析、WCS、内存和 pipeline smoke；不做全量科学结论。
- Windows 小真实：三块各一帧，验证 overlap、writer 和产品结构。
- Windows 最终：冻结的 32R 一次全链，检查每帧贡献、接缝、黑洞、条纹、卫星线、support、UPM、rejection 和 Phase3。
- 发现问题后回到最小合成 fixture 复现；不要靠反复 32R 调参。

## 8. 参考依据

正式 SCI/ALG 应引用并说明采用/未采用的部分：

- Fruchter & Hook, *Drizzle: A Method for the Linear Reconstruction of Undersampled Images*, PASP 114, 144–152 (2002), arXiv:astro-ph/9808087。
- Górski et al., *HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere*, ApJ 622, 759–771 (2005), arXiv:astro-ph/0409513。
- Greisen & Calabretta (2002) 与 Calabretta & Greisen (2002), FITS WCS Papers I/II。
- Jacob et al., *Montage: a grid portal and software toolkit for science-grade astronomical image mosaicking* (2010), arXiv:1005.4454；用于模块化 mosaic 和背景 rectification 参考，不代表可直接复制其科学选择。
- Maples et al., *Robust Chauvenet Outlier Rejection* (2018), arXiv:1807.05276；只有当 AstroCS 正式实现 RCR 时才作为算法依据，不能只借名称。

链接和底层并发规范统一见 `14_PRIMARY_REFERENCES.md`。
