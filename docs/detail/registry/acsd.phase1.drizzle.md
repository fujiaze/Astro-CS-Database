# 模块 acsd.phase1.drizzle

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节、「输出合同」一节（帧级 SNR 入文件头、
> 稀疏层插入）、「P2 跨帧绝对信噪比」一节与无覆盖即NaN语义口径
> 科学正本：docs/science/drizzle/DRIZZLE.md（SCI-DRZ-001/014/015/016，「参数与常数」表的
> pixfrac / 叶面积 / `flux_conservation_factor`；「主题与目标」「几何闭合」的守恒不变量）、
> docs/science/algorithms/DRIZZLE_GEOMETRY.md（ALG-DRZ-001，重采样几何、「TEST-DRZ-DESIGN-001」
> 测试设计与「DISP-DRZ-001..009」缺陷登记）、docs/science/noise_snr/NOISE_SNR.md「重采样方差传播」
> 数据正本：docs/detail/registry/acsd.phase1.drizzle.md（DATA-P1-DRZ 端口表与帧内
> "variance" 块（可选，帧内块）float32，ADU²，本页输入输出端口表）
> 落盘与索引词表：eng/contracts/schemas/hips_storage_form.schema.json、
> eng/contracts/schemas/run_manifest.schema.json
> API 正本：docs/engineering/api/PUBLIC_API.md（API-DRZ-001）、API-P1-007
> （docs/engineering/api/PUBLIC_API.md「分阶段 API 面」）
> 数据对象：`docs/detail/common/unified_model.md`「重采样线性算子」一节

合同 = SCI-DRZ-001 / ALG-DRZ-001 / TEST-DRZ-DESIGN-001（与
`lib/algorithms/drizzle/module.yaml`、registry descriptor 同口径）。事实源 =
`lib/algorithms/drizzle/README.md`、DRIZZLE_GEOMETRY.md、
`lib/algorithms/drizzle/src/`。

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp descriptor）。职责：把定标后
单帧图像按 WCS 重采样到球面 HEALPix/HiPS 格点，输出标准化单帧产品（HiPS + 结构化
JSON）：tiled 球面 drizzle（drop 收缩 footprint × HEALPix NESTED leaf 交叠，
S-H+Van Oosterom 面积加权累加 sumFlux / sumArea / sumVarNum / nContrib）、auto
nside、FP32/FP64 通道、HiPS 直写 / HISS 输出、反向 drizzle、操作计数诊断。

不做：测光 / PSF 建模；多帧统计合并（Phase2）；master / 校准 / 坏点（P1-CAL /
P1-COS）；面亮度归一与 variance finalize（astro_image_io 层）；线程授予（omp
内部通道 + Runtime lease，ThreadLease 迁移整改点）；**点源信息权重不能仅用
drizzle 后逐像素 ivar 重建而丢掉 PSF / 协方差**；**不产出外挂独立 SNR 文件**。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `calibrated` | `DATA-P1-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `wcs` | `DATA-P1-WCS` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `photprov` | `DATA-P1-PHOTPROV-001` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `sources` | `DATA-P1-SOURCES` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `snr` | `DATA-P1-SNR` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `stacked` | `DATA-P1-STACK` | 可 | `UnitId::SURFACE_BRIGHTNESS` | `CoordinateFrame::ICRS` |

数据合同：descriptor 的 data_id = DATA-P1-STACK（编排汇总语义）；模块级数据合同 =
DATA-P1-DRZ（本页输入输出端口表：tile 累加量为原始和，finalize 归一在下游），其
对齐属迁移目标（未落地）。输入帧形如「data」f32/f64 `[H][W]` ADU + header
WCS/SIP + 可选 PRECISION / snr_model；目标 NESTED tile 产品含 SIGNAL / SUPPORT /
variance / ivar，tile_depth = 9、nside ≥ 512 硬门，外加 `HpDrizzleResult` 统计
与 operation_counts.json。

**HiPS 文件内容**：signal、pixel variance/ivar、support、coverage/validity、
drizzle correlation/transfer 描述、PSF 模型、photometric response、
point_information map、psfsw、depth、**帧级 SNR 写入文件头**、**稀疏帧内 SNR 层
（控制点存绝对 SNR）作为标准层**、source catalog、manifest。

**结构化 JSON**：输出路径信息，符合 Phase2 输入格式；逐帧产品清单
`p1_products.json` 登记帧清单与 HiPS 路径（`frames` / `hips_paths` /
`filter_passband`）；产品级索引（`index_path` / `index_sha256` /
`archive_sha256`）与运行级覆盖索引（`coverage_index`，含 `path` / `sha256` /
`n_frames` / `n_blocks`）属归档形态写出侧的待实现项。

**落盘形态**：输入配置键 `storage_form` 取 `archive`（`<name>.hips.zst`，整包 tar
+ 逐成员 zstd 帧）或 `bare`（`<name>.hips/`），schema 缺省 = `archive`；键缺失
或留空 ⇒ 取默认并报 warn（不静默取默认）。**生产写出侧当前只落裸形态** ——
`storage_form` 在写出侧无读取点，归档形态的写出与读取、产品级索引与运行级覆盖
索引均属待实现项；归档形态落地时，其归档内 `properties` 必须与裸形态逐字节
一致。

**invalid**：源像元值非有限（NaN/±Inf）按**样本级掩膜**处理 —— 不合格样本从
分子、分母、方差三项中一并剔除并**重新归一**，剔除数逐叶计数（引擎计数
`rejected_nonfinite_value`，规则正本 = docs/science/drizzle/DRIZZLE.md 的「误差
来源与预算」）；只有**零合格样本**的输出像元才取 NaN 且 `support ≤ 0`
（覆盖级 NaN）；方差面非有限同样按不合格样本剔除，而方差有限但 ≤0 表示「有覆盖
但无方差信息」，此时信号与几何权重照常计入覆盖、不计入方差项（回归 finalize 层，
covered_area ≤ 0 → variance 记 NaN）。`pixfrac ∈ (0,1]` 引擎层严格拒绝（非有限、
0、负、>1 均拒绝，不夹逼）；仅 NESTED。

### 数值落地口径

面积加权累加与核归一的推导正本 = DRIZZLE.md 与 DRIZZLE_GEOMETRY.md；本页只记
落地约束与实现落点：

- 源像素积分通量先转面亮度，再按球面交叠面积加权累积；同时输出线性算子 / 足够
  方差传播信息、support、coverage、validity、相关噪声描述；
- 重采样是线性算子：输出协方差由输入协方差经该算子的双向作用得到；只存对角
  variance 时**必须**另存 correlation kernel / scale 或可重建算子摘要；
- signal 单位、源/目标像素面积、pixfrac、归一必须统一，不得隐含；
- **核按 drop 面积归一（canonical）**：交叠面积除以该 drop 的面积。依据 F&H 2002
  式(2)–(5)的累加式与Fruchter & Hook 2002式(7)后「`a + b = 1`」的定义句（双锚；原引已由
  docs/science/algorithms/DRIZZLE_GEOMETRY.md 撤换），以及 drizzlepac
  `src/cdrizzlebox.c` 的 `do_kernel_square`（`dover /= jaco`，`jaco` 为映射后的 drop
  面积）；在这一口径**且几何闭合成立**时，累加的通量总和等于源端积分
  通量总和。通量守恒的成立是**条件式**的：条件一 = 核按 drop 面积归一（换任何
  其他归一即破坏），条件二 = 几何闭合 `Σ_p a_jp = A_drop,j` 逐 drop 成立（drop
  足迹面积在其覆盖的叶上被精确分完，超额与亏损分开具名判红；`A_drop,j` 取球面
  实测值，它与 `pixfrac²·A_pixel,j` 只在平面极限相等，残差见下条 `k`）。守恒量与
  两条核心不变量的正本 = docs/science/drizzle/DRIZZLE.md 的「主题与目标」与
  「归一与分母的唯一性」，闭合判据的正本 = 同文件「几何闭合」与「正确性判据」；
  本页不作无条件断言；
- **面亮度归一分母 = 各源 drop 归一权重乘源像素面积之和**：若把分母换成覆盖
  面积，`pixfrac < 1` 时面亮度偏 `pixfrac²` 的倒数（pf = 0.8 ⇒ +56.25%，
  `DISP-DRZ-009` 负例判据）。等价参数化（同一交叠面积）：按源像素面积归一配覆盖
  面积分母；两种参数化给出同一个 `c_jp`、因而给出同一个 `S_p` 与同一个
  `variance_p`（实现侧两个直写末端共用同一分母、走同一代码路径 ⇒ 产物逐位相同；
  两套公式独立复算时差在双精度舍入量级），但通量守恒因子只有 drop 面积归一口径
  等于源端积分通量。`provenance.flux_conservation_factor` **恒为 1**（drop 面积
  归一，与 pixfrac 无关；口径正本 = docs/science/drizzle/DRIZZLE.md「参数与常数」
  表的 `flux_conservation_factor` 行）；
- **归一发布因子 `k = D_p/N_p`（`D_p = Σ_j a_jp` 为覆盖面积，
  `N_p = Σ_j w_jp·A_pixel,j` 为面亮度归一分母）**：在**平面（仿射）极限**下，
  由 `A_drop,j = pixfrac²·A_pixel,j` 与 `w_jp = a_jp/A_drop,j` 得
  `N_p = Σ_j a_jp/pixfrac² = D_p/pixfrac²`，即 `k = pixfrac²`（与各 `a_jp` 交叠面积的
  具体取值无关；`pixfrac = 1` 时 `k ≡ 1`）。**球面上该等式只近似成立**：
  `A_drop,j` 与 `A_pixel,j` 各自由球面上不同四边形量得，二者之比与 `pixfrac²`
  的相对残差按
  `δ = (1−pixfrac²)·θ²·[0.25/(1+r_c²) − 0.625·ξ_c²/(1+r_c²)²] + O(θ⁴)`
  随源像元角尺度 `θ` 二次增长（pixfrac = 0.8、中心在参考点时 θ = 2″→8.5e-12、
  θ = 6.3″→8.4e-11、θ = 60″→7.6e-9、θ = 300″→1.9e-7）。把它代回
  `N_p = Σ_j (a_jp/A_drop,j)·A_pixel,j` 作一阶展开可知：`k` 相对 `pixfrac²` 的偏离
  与 `δ` **同阶**（面积加权平均、符号相反）。由上式在 `r_c = ξ_c = 0` 下取
  `δ = 0.09·θ²`（θ 以弧度计）直接读出边界：`θ ≲ 7″/px` 时偏离 ≲ 1e-10、
  `θ ≳ 217″/px` 起进入 1e-7、`θ ≳ 688″/px` 起进入 1e-6（后两档才与门禁容差同阶）。
  残差律的推导、独立复算与实测读数正本 =
  docs/science/algorithms/DRIZZLE_GEOMETRY.md 的「DISP-DRZ-001..009」章末
  「面亮度保持权重的实现口径」段；实现侧 `k` 的取法 = `sb_publish_scale`
  （`sum_norm ≤ 0` 时退回 1.0 并计数告警），**不得**用近似式
  `overlap_area·pixfrac²/drop_area` 替代球面精确交叠面积。`k` 的幂次用法
  （signal 乘 `k`、variance 乘 `k²`，漏 `k²` 把方差压低 `pixfrac⁴` 倍）正本 =
  docs/science/noise_snr/NOISE_SNR.md 的方差传播节；
- **实现落点**：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 的
  `processPixelSharedTiled` 权重核 = 交叠面积 / drop 面积（代码内名
  `overlap_area` / `drop_area`），并累加归一分母；`pixfrac == 1` 时 pixel_area ≡
  drop_area ⇒ 归一分母 ≡ sumArea 逐位相同，默认路径产物逐字节不变；
- **适用域**：两种归一口径的差异量是 `pixfrac²`，故 `pixfrac = 1` 时同一结果；
  差异在 `pixfrac < 1` 的配置下可观测。生产默认见
  `eng/packaging/config/defaults.json`。

## 公共 header、核心 symbol 与生命周期

现状 C ABI（`lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h`，六导出，
extern "C"）：`hp_drizzle_fits_to_ahpx` / `hp_drizzle_run` / `hp_drizzle_run_hips` /
`hp_drizzle_reverse_run` / `hp_drizzle_reverse_capability` /
`hp_drizzle_reverse_version`（API-DRZ-001，PUBLIC_API.md）。球面几何接口
（`spherical_overlap.h`）：`compute_overlap_area_g_ctx_cached`、`radec_to_vec`、
`HP_CIRCUMRADIUS_FACTOR = 1.25`。编排级 API-P1-007（区间声明）。迁移生命周期
create→validate→run→inspect→destroy 为迁移目标（未落地）；现状 entrypoint 未落地
（descriptor 未接节点，p1_session 无 drizzle stage）。

entrypoint = 图像组 + WCS + 科学层 → HiPS 产品目录 + 结构化 JSON。输出原子目录，
重开独立消费（Phase2 不需回读 raw light）。

### 源文件

`lib/algorithms/drizzle/healpix_drizzle/`（编入根 CMake 静态库
`acsd_drizzle`；`poly_clip.cpp` 编入但生产路径零调用，登记见
DRIZZLE_GEOMETRY.md）。模块合同落位 `lib/algorithms/drizzle/`。

## Registry descriptor 与配置 schema

module_id=`acsd.p1.drizzle`; execution_class=`cpu_heavy`; parallel_ok=True。

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `order` | —— | —— | HEALPix order（或 HiPS 尺度） |
| `pixfrac` | 0.8 | —— | drop 收缩因子 ∈ (0,1]（CFG-001）。**数值默认唯一来源 = `eng/packaging/config/defaults.json` 的 `drizzle.pixfrac`**，本表不另立取值 |
| `pixel_scale` | —— | arcsec/px | 输出像素尺度（HiPS tile） |
| `nside` | —— | —— | HEALPix nside（与 order 等价）；`nside_mode` / `nside_value` 走 auto nside |
| `ordering` | `nested` | —— | HEALPix ordering（仅 NESTED） |
| `precision` | —— | —— | FP32 / FP64（经 header KV `PRECISION`） |
| `storage_form` | `archive` | —— | `archive`（zstd 归档包）/ `bare`（裸目录）；键缺失或留空取默认并报 warn |
| `sparse_snr_layer` | true | —— | 是否将稀疏帧内 SNR 层插入 HiPS（来自 noise-snr；控制点值 = 绝对通量型 SNR，与帧级同口径、同一个配置缺省的参考星等档 `m_ref`，可被输入 JSON 覆盖）。配置缺省为**请求产出**；该层的生产侧产者落地状态见 `docs/detail/registry/acsd.phase1.noise-snr.md` 的输出面表 |

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`。并行轴 = 源图像行（schedule(static) 条带 + per-thread tile 累加器，
drizzle_engine.cpp）；按线程序合并 touched leaf。ThreadLease 零命中（omp 内部
通道）—— 迁移整改。

**1/N 确定性** = 同输入同线程数 bitwise 可复现；跨线程数浮点和序不同，不保证
bitwise。geometry cache：per-thread LRU + per-run generation 原子清空；
同进程多 run 并发安全。

复杂度与三层候选缓冲（1.25 / 3.0·hp_res + fast 1.15 畸变系数）按 run 生成。
候选零漏选的判据见 DRIZZLE_GEOMETRY.md。

## 内存/cache/I-O/所有权

tile 累加器为 leaf 连续数组（**禁** per-leaf 全局 map）。I-O = HiPS 直写（AIO
API）+ `.hiss` 输出 + operation_counts.json；stdout 无日志（全部 stderr）。

**有界 target-ipix 几何缓存**：`TargetGeomCache`（LRU，默认 8192，线程私有，
run generation 切换清空，原子化替换）。计数新增 `target_boundary_builds` /
`target_geometry_builds` / `geometry_cache_hits` / `geometry_cache_misses`
（DrizzleStats + `[ops]` 行）。缓存的科学等价由 candidate oracle、freeze 闭合门
与 MC 复算结果给出（判据见 DRIZZLE_GEOMETRY.md）；`k_corr` 的规范式 = 因子分解式
`k_corr = k_gauss(N_retained) × k_geo(几何)`、逐帧查表标定
（`docs/science/noise_snr/NOISE_SNR.md`「定权」一节`k_corr`因子分解式），标定域两端的冻结单数 1.4 低估，
不作规范取值。

所有权 = 调用方分配 frame / result / 输出缓冲；模块内 RAII（SNR 控制点 vector，
以符号名为准）；HiPS 目录树由模块写入、编排层负责 overwrite 清理。

## 错误、日志、指标、取消和 checkpoint

错误码：几何退化 / 无 WCS / 非法参数 → 拒绝。文件通道正值 1..12（+12 = C 边界
内部异常）；帧通道正负混用 −1..−8 / −9 / −12 / −13，**无集中枚举 —— 登记缺陷**。
`error_msg[512]`。

日志 = stderr，前缀 `[hp_drizzle_api]` / `[drizzle_engine]` / `[sink]`。指标 =
`DrizzleOpCounters`（`operation_counts.json`，全字段见该剖面；计数含
`METRIC-P1-DRZ-CANDIDATES` 等）。

- WCS 缺失 / 不完整 → fail-closed；
- 相关噪声不存描述 → variance 完备性的宣称以该描述在盘为前提；
- 缺 tile / 非有限 → validity 标记，不以零填充；
- **无覆盖 / 无数据 = NaN**（与支撑度 ≤ 0 一致），不用 0 或 ±Inf 冒充无效。NaN
  采用**样本级掩膜**：被掩除的样本不参与该输出像素，剩余样本权重**重归一**；
  整个输出像素无有效覆盖则置 NaN（**覆盖级 NaN**）并**强制计数**（最高设计
  「公式与推导」一节，规则见DRIZZLE.md）；
- **逐像素方差/ivar 产品面**：生产调度路径**已挂** `variance` 帧内命名块 ——
  module_adapters.cpp（`p1_op_drizzle`）按噪声模型 A 的 blank-sky variance 填面后
  `aio_frame_add_block(frame, "variance", AIO_BLOCK_FLOAT32, …)`；引擎侧按权重
  平方累加，sink/writer finalize 出 variance/ivar 子产品；`uncertainty_available`
  为 provenance 判定结果（`true` ⇒ variance|ivar 位同时置位，`false` ⇒ 两位均不
  置位，禁占位子产品），**由磁盘事实给出，禁硬编码**。**显式降级（非静默，带
  `var_status` / `var_reason`）**：noise model 退化（rc=1）⇒
  `skipped_degenerate_empty_support`；填充面含非有限 / 非正值 ⇒
  `skipped_fill_failed`（全零方差面会让引擎整像素跳过，抹掉 signal / support，
  故 fail-closed）。凡「逐像素方差已由生产路径产出」的主张**必须**附方差 tile
  数大于 0 的磁盘证据；双实现分裂项见 registry/acsd.phase1.noise-snr.md。

取消 = 模块内无检查点（登记限制）；checkpoint 无（HiPS 由编排层 overwrite 清理）。

## 独立 synthetic 验证命令与容差

`TEST-DRZ-DESIGN-001`（DRIZZLE_GEOMETRY.md 的「TEST-DRZ-DESIGN-001」章）：FP64 通量闭合 < 1e-6（主域）/
逐 leaf < 1e-5、方差缩放律 worst_rel < 1e-4、候选零漏选（合成负例，全命中）、
reverse false_hole / false_fill = 0。可执行 `TEST-P1-DRZ-001` 待建。

已取证的科学门读数（载体不在本仓可复算路径上，引用时只作背景）：candidate /
overlap / variance oracle（证据产物为测试期 JSONL）、freeze / l0 闭合门、缩放律、
reverse false_hole / false_fill；Monte Carlo 方差（SNR-011/012）。

已取证的回归门读数：

- `p1drz_disp009`：常量面亮度 `|S_p/B0 − 1| < 1e-3` 覆盖 `pixfrac ∈ (0,1]` +
  「分母取覆盖面积必判红」负例控制；
- `drizzle_acceptance`：`Σ_p sumFlux = Σ_j x_j`，`pixfrac ∈ {0.1, 0.5, 0.8, 1.0}`
  + `--inject-legacy-pixfrac2` 负例注入。

Oracle 面：常量面亮度、积分通量、variance、correlation oracle 全过；重采样
协方差与高精度矩阵 oracle 对比；不同 pixfrac / order 下科学值一致性；产品从
磁盘独立重开后足以执行 Phase2（不依赖进程内状态）；1 worker vs N worker 一致。

## 已知限制

- DISP-DRZ 清单（正本 = docs/science/algorithms/DRIZZLE_GEOMETRY.md 的
  「DISP-DRZ-001..009」章；另见 `lib/algorithms/drizzle/README.md` 的
  「构建与已知限制」段）：值 NaN 掩膜与计数已按 DRIZZLE.md 的规则落地并在引擎
  分类计数；仍未闭合的是 `pixfrac` 值域双轨（文件通道 API 层接受 0.0、引擎层
  拒绝）、帧/文件两通道错误码混用且无集中枚举、无取消检查点、static 条带负载
  不均、`poly_clip` 生产路径零调用；
- 归档形态（`storage_form = archive`）的写出与读取、产品级索引与运行级覆盖索引
  未落地，生产只落裸形态；
- 面亮度归一与 variance finalize 在下游 astro_image_io 层，模块与编排层的
  DATA-P1-DRZ / DATA-P1-STACK 对齐属迁移目标（未落地）；
- ThreadLease 零命中，跨线程数不保证 bitwise；迁移 entrypoint 未落地（descriptor
  未接节点，p1_session 无 drizzle stage）；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
