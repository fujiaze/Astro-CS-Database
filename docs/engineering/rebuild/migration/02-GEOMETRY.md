# 02｜球面Drizzle、自动NSIDE、信息量与协方差：高精度且保持主分支性能

**主负责人**：Geometry/Coordinates；并行只读审核可交Numerics/HEALPix/Storage。**对应Process**：geometry.overlap、geometry.drizzle、operators.action、uncertainty.covariance、information.reference、information.scene，以及coordinates.wcs的尺度探针。标准型输入/输出仅由现有 V2 公共头定义，本文不给第二套struct。所有真面积/区间与噪声公式仍以Foundation和对应Process为准。

## G-00 旧代码与新代码的实际边界

| main被生产使用/参考 | V2真正接入位置 | 核心迁移裁决 |
|---|---|---|
| `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`、`hp_drizzle_api.cpp`、`hp_drizzle_hips_api.cpp` | `src/geometry/{drizzle,drizzle_core,drizzle_grid,drizzle_process}.cpp`，Normalize workflow | 优先抽取 autoNSIDE/候选/accumulator/条带归约和性能策略；不复制第二Runtime |
| `spherical_overlap.cpp`、`poly_clip.cpp`、`wcs_sip.cpp` | `src/geometry/{overlap,refine,exact_polygon,containment,profile}.cpp` 和WCS | 旧四角/大圆近似不是现有V2严格曲边真面积证明，不得无条件替换 |
| `astro_sphere_sink.cpp`、main AIO/HiPS writer | `src/workflows/{product,product_io}.cpp` + Storage | 继承touched叶/只写有效Tile的稀疏性能与512 tile语义，V2事务为唯一持久化 |
| `shared/healpix/healpix_core.cpp` | `src/coordinates/healpix.cpp` | 保留真实NESTED位序/父子、face/极点算法与测试，只维护一套 |
| `noise_snr/cpp/src/sparse_reference.cpp`、`normalize_precision.cpp` | `src/information/`、`src/uncertainty/`、`src/operators/` | 保留稀疏参考计算思路，更正旧估计量混用、全共变随机根与后续action |

## G-01 P0：让自动NSIDE重新成为真实生产默认策略

**main 证据**：`drizzle_engine.cpp::compute_auto_nside_ex`（约711行）以WCS/SIP解析尺度、自适应四叉树局部Jacobian采样、最细尺度计算与2次幂选nside；`hp_drizzle_api.cpp::hp_drizzle_compute_auto_nside`是C入口；`module_adapters.cpp::p1_op_drizzle`的production读取auto请求并写运行追溯。**V2缺口**：`apps/cli/commands.cpp::decode_drizzle_request`缺省静态order；`src/workflows/normalize.cpp`没有统一全帧自动决策；Mosaic只接受输入leaf order相等，未给出此前统一规划。

任务 G01-a（尺度纯函数复用）：
1. 冻结main自动函数的所有输入WCS、采样深度、尺度变化触发、异常/溢出/clamp、返回值和单元，抽成可复用只读计算核，移到V2的coordinates/geometry合适文件；不把整个旧drizzle_engine拖入V2。优先保持其正确的解析导数与低分配策略。
2. V2复用真实`coordinates::WcsModel` / `solid_angle_jacobian` / 导数公开接口，**不要**让autoNSIDE私建第二个SIP solver。验证旧算法有限采样**不是严格全域最小值证书**；允许具名受界近似，但没有控制误差的情况不能假称证明，极区/畸变边缘要反例。
3. 显式订单覆盖可存在，产品保存选择策略、所有输入候选尺度、最细有效尺度、角尺度准则、nside/order、是否clamped、返回条件及计算代码哈希。过粗阶的显式请求不强行失败普通诊断产品，但**不可取得“高分辨率正式科学产品PASS”**。

任务 G01-b（**跨帧统一ResolutionPlan**，优先于任何单帧正式Normalize）：
1. 输入为全部拟执行帧的最终WCS身份、有效采样域、PSF/目标角尺度/实际通带与请求policy，先轻量probe，得每帧局部scale/合法域；在Mosaic之前选取统一目标leaf order，并显式检查内存、tile order。
2. 以最细有效角尺度为主策略（同等面积尺度 s≈sqrt(pi/3)/nside，不等同于两个方向的严格间距），同时评估WCS局部极值与用户输出目标尺度。相同目标WCS四通道可共享全局分辨率计划，但不同通道的响应/PSF允许独立声明。
3. 正式Normalize显式消费冻结的`ResolutionPlan`或适配现有类型中具名请求；不在每帧内再选择不一致阶，更不在Mosaic进行**从粗叶上采样假造细节**。若不同源必须混阶，另作数学上可证的变换任务；当前优先同阶。
4. 改计划引起geometry/R/HiPS/Mosaic/Export缓存失效，原始校准、Gaia/WCS（内容不变）仍可复用。Storage键含input-set身份、WCS revisions、policy hash与算子版本。

**尺度回归示例**：实际S1主帧约0.967″/pixel，而固定leaf order13对应约25.77″/leaf；main决策量级nside=2^18/leaf order18约0.805″/leaf，**只是示例不是硬编码**。512×512 tile时tile order=leaf order−9（在适用深度），Norder9即leaf order18；这个索引规则必须在存储和FITS层一致。

## G-02 几何Overlap：沿用性能候选，但保存V2曲边可信度

- 拆分：`footprint_provider`给连续畸变像元足迹；旧 `spherical_overlap`可提供快候选/粗剔除/本地近似初值；`geometry.overlap`的正式结果必须是对真实WCS/SIP曲边HEALPix叶的重叠、边界和面积区间。旧“四角投影后按球面大圆裁剪”的误差不能因快而无声通过。
- Candidate pruning必须**保守**：必须覆盖所有可能正交叠叶，误差用真实连续边界包络/自适应subdivision/interval排除；没有认证时保留bounded-undecided并允许refine，不可当无支持。
- 接口复用现有`GeometryBudget`、`FootprintSet`、`Overlaps`、`GeometryRevision`；工作量计费包含真实候选生成、交叠评估、区间加精、MPFR/GMP（如需要）的实际bit-length，取消和不足预算有具名状态。
- 高阶性能不能枚举全天空12*nside²叶；使用空间区域球面候选、hierarchical bounding boxes/MOC、按原像元块+目标tile分区，缓存与run版本绑定。旧thread_local bounded target-ipix geometry cache与候选向量复用是**待移植性能策略**，但不缓存未认证的旧近似当最终真面积。
- 试验：高畸变/星图边缘、HEALPix face seam/RAwrap/极区、薄sliver、部分窗/pixfrac、接触边/空支持；以独立healpy/astropy-healpix和自适应数值积分/高精几何oracle证明候选完整、支持面积误差包含、守恒与非负。记每源像元平均候选数、overlap算子调用次数、cache命中、区间细化和CPU热点行。

## G-03 Drizzle R：性能核复用优先，物理算子不可变

**main高价值优化必须逐条迁：**
1. `drizzle_engine.cpp` 逐stripe由单线程独占像元累加，**按固定stripe编号归并**：既避免多个线程对同一个浮点叶写atomic，又保持线程数变化时归约顺序可复现。
2. 每线程 `t_candidates`、drop geometry scratch向量复用；每线程按run generation失效的target-geometry **bounded LRU**；邻近像元重复访问叶形状无需每次从头构造。
3. 叶累加以parent tile定位 + `touched` 叶列表；只合并实际接触叶、只发布有支持的 tile；不创建“天区以外全NaN目录”，不做Npix全天稠密循环。
4. 分块有界：旧按stripe/TileAccumulator可改为V2 TileBlock/CSR COO spool，并用同一ProcessHost内存账本与Pipe背压；**chunk边界只影响性能，不影响物理像元→球面R**。

具体迁移：
- 先验证旧算法的 `R_pj` 与V2现有CSR/Model支持/单位公式在哪些合法物理域可等价；旧fast路径若用四角近似，保留候选与局部缓存但用V2严格Overlap求真实系数；不可要求有缺陷旧数值逐位相等。
- `src/geometry/drizzle_core.cpp` 的累积器按 leaf row/源原始像元column 建立真实稀疏映射；`src/workflows/normalize.cpp`现有source rows可能是 detector pixel block，而不是单个原像元，必须在科学operator中明确block表示/附带的row内精确分解，否则会发生细节平均丢失。
- 同一次R作用signal、PSF/response、noise/shared roots、control basis；R的CSR由非零项计数和真实row数分别管理，**nnz与叶数不同不是错误**，错误在于映射身份和高阶有效域是否真实自洽。
- 每块采用双缓冲或 bounded pipe最多在途K块；flushing后只保留immutable block元数据与必要高频cache；R/共变action按需重开，不把全部CSR/已发布tile常驻。
- 单/双帧/多帧使用同一版本几何R与CPU预算，禁止用精度降低、改变pixfrac或线程数引入不同数学模型来“提高速度”。

## G-04 Operators/Uncertainty/Information：复用稀疏核，杜绝稠密爆炸

对应代码：V2 `src/operators/{linear_action,api}.cpp`、`src/uncertainty/{covariance,api}.cpp`、`src/information/{information,internal,triangulation}.cpp`；旧main `noise_snr/cpp/src/{sparse_reference,information_weight,noise_model}.cpp` 和 `integration/normalize_science/`。

- 数值核：抽旧有效稀疏GLS、局部控点三角剖分、矩阵求解、PSF/参考信息函数的计算原理和测试。V2已有的action compose/apply/transpose、shared-root、协方差与严格区间能力不回退。
- 阻止按受支持叶数生成 dense `C`，只通过`covariance_apply`和`quadratic_form`、稀疏block influence及共享根表示。缓存系统中按真实依赖使Action children/CSR payload、support arrays与所有随机根在新进程重开；缺引用必须非零失败发布。
- reference SNR `I_F`、scene variance `V_B`、Sky控制方差 `V_ctrl`分别定义/估计；不能再用旧星点 Fisher权作为最终scene IVAR，不能将未知gain/read/QE自动补零。
- 真实数值认证需要真正interval/超越/秩证书，支持范围不满足则标conditional；**有条件估计不应被删**，但产品不能因此达到无条件SNR最佳声明。
- 可并行每个独立空间窗口、局部CG/Cholesky with fixed ordering，不能对每种作用重复展开全局稠密Gram；限制每个matrix/workspace字节与reuse/cancel，验证rank-deficient/PSD/共享母版、同帧sky同数据交叉项以及多个输出leaf共变。

## G-05 性能和科学验收（必须一起通过）

逐规模 `32²→128²→256²→512²→4096²` 的真实WCS/同科学recipe，高阶引入后记录 Nvalid_source、Nvalid_leaf、Ncandidate、nnz、Ntouched_tiles、Noutput_rows、RSS峰、disk峰、读写字节、geometry_cache_hit、CPU_seconds、wall、worker/lease，解释任何非线性突变；禁止拿低order跑基线和高order测V2宣称“差不多快”。

强制比较：
- **常值/脉冲/已知PSF星**：总光度、FWHM/ellipticity、MTF、背景低频、噪声PSD、无支持NaN与真实面缝，凭外部真值量化细节而非只看 `ΣRx=signal`。
- **同输入 main/V2**：先统一WCS、物理单位、目标叶阶、pixfrac、输出质量；记录main已知近似造成的理论差异，不强求无意义字节相等。
- **旧V2反例**：order13，122行/73151nnz不应被误认为矛盾，6.1GiB无效Tile、非法FITS头、缺LinearAction闭包、全NaN成功退出必须持续被拒绝；对高阶复发的全Npix填充/超RSS直接FAIL。
- **资源**：12GiB上限不改，目标在系统可承受范围内运行真实4096²、完整M42分块；科学order不因预算不足变粗。不可达时先按本节改稀疏结构、Spill和共享缓存，仍不能达则BLOCKED_RESOURCE，不发布假分辨率产品。细节见[06](06-PERFORMANCE.md)。
