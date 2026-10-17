# ALG-HIPS-001..005｜统一 AIO HiPS Writer 的球面信号、层级、精度状态和产品提交

> 上游：ACSD_DESIGN.md [D-1]「节点流程」一节（Phase1 节点流程）、[D-1]「I/O 与原子产品」一节（I/O 与原子产品）

> SCI 上游: SCI-DRZ-001（docs/science/drizzle/DRIZZLE.md，FROZEN，共享引用不改动；
> 实现锚 finalize_tile 方差语义与 support=D_p 归一语义两处）
> 与 SCI-SCOPE-001（docs/science/unified/科学范围 [S-2]「处理链」一节，产品目标）；读侧消费合同
> SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，只读引用）。
> 实现源（逐公式锚定）: lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
> （合同头 lib/infrastructure/aio/include/aio_hips.h）。
> 数据语义权威: docs/detail/infrastructure/hips_writer.md（DATA-P1-HIPS；上游 docs/detail/normalize/modules.md 的 DATA-P1-DRZ、
> [S-5]「输入输出端口」一节 DATA-HIPS-VAR-001/DATA-HIPS-IVAR-001、[S-6]「输入输出端口」一节 FITS 局部像素映射、[S-6]「输入输出端口」一节 帧身份）。
> HiPS 1.0/1.4 外部参照: IVOA HiPS 推荐（Fernique et al. 2015）、HEALPix 算法
> （Górski et al. 2005）——经 SCI-P3-001 收录的文献锚，本文件不另立外部断言。
> 本文件为逐公式"算法+源码锚点"登记：凡 SCI 层无覆盖而实现自带的语义（HiPS 写出
> 合同细节），以实现为准登记并标注；凡实现与 SCI 语义冲突处，登记 DISP- 条目。
> **权威订正原则** = `docs/ACSD_DESIGN.md` [D-1]「文档权威与索引」一节引）「科学正确性优先」：独立证据（外部标准 /
> 文献 / 可复跑实验）证明文档与事实不符时，**订正文档是义务**（SCI 层订正走变更流程并
> 记录证据与影响面）；文档已被证明正确而实现不符时改实现。
>
> **源码锚声明**：`aio_hips_writer.cpp` 经原子写与流式 hierarchy 重构后主函数行域整体推移，
> 函数内部结构亦有多处重排（如 write_moc_fits 分层出
> write_moc_fits_raw、finalize_hierarchy 分层出 write_hierarchy_cell）。本文件全文的
> 锚一律给到**符号名 + 文件**（不再用行号定位），读法以**符号名与语义**为准。
> `eng/contracts/anchors/anchor_contract.json` 未对 aio 文件声明 symbol binding，
> 故 C4 机器门不覆盖此漂移；子锚批量重标归全库重锚工程。

## 0. 范围界定

覆盖：aio_hips_writer.cpp 全部写出算法路径——product_begin 校验与产品状态
（ALG-HIPS-001）、叶级 tile 信号/支撑写出与归一（ALG-HIPS-002）、方差/逆方差
产品（ALG-HIPS-003）、层级聚合 hierarchy tiles（ALG-HIPS-004）、MOC/properties/
SNR/manifest 收尾（ALG-HIPS-005）。不覆盖：tile 累加产生 AstroSphereTileView 的
drizzle 引擎公式（ALG-DRZ-001 权威）；HiPS 读侧（aio_hips_reader.cpp，P3 链）；
IO-003 Python 发布层（原子 rename/manifest COMPLETE 语义在
docs/engineering/contracts/ATOMIC_PUBLISH.md，本文件仅登记对齐边界）。

## 1. ALG-HIPS-001 — 产品生命周期与叶级几何基数

**源码锚**: `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` 的 `aio_hips_product_begin`。

- (1a) 叶级阶 K 与 tile 阶：`leaf_order = ilog2(nside)`，
  `tile_order = leaf_order − 9`（tile 宽恒 512=2^9），叶级
  nside = 2^K ≥ 512（拒绝 nside<512）、tile_width==512（同文件）。
  与 `docs/science/unified/DATA_SEMANTICS.md`「坐标语义」一节（leaf_order=tile_order+9）一致；与 SCI-P3-001 读侧
  默认一致。注意 hiss 侧 `compute_tile_depth`（`lib/infrastructure/aio/src/hiss_common.cpp`，
  d=clamp(log2(nside)−4,0,9)）是独立中间容器语义，与本公式不同（不混用）。
- (1b) 单叶球面元面积 `A_cell = 4π / (12·nside²)`，即 Górski 2005
  NESTED 等面积基数 4π/(12·4^K)；无 SCI 独立条目，按实现+文献登记。
- (1c) MOC 阶：入参 moc_order>0 时 `moc_order = min(moc_order, tile_order)`
  ；=0 时自动取 tile_order（finalize 路径）。读侧
  `lib/infrastructure/aio/src/hips/aio_hips_reader.cpp` 仅保留 order==K 的 UNIQ——moc_order<K 时
  低阶父 cell 对自家 reader 无效（见 DISP-HIPS-005）。
- (1d) 产品集校验：现行 flags 位域 {1,2,4,8,16,32,64}，ALL=7/ALL_V19=31/ALL_V20=127
  （`lib/infrastructure/aio/include/aio_hips.h`；begin 拒绝越位）；dtype∈{0=float32,1=float64}
  （同文件）；违规 → 返回 NULL + last_error。
- (1e) 缺省元数据：creator_did="ivo://acsd/phase1"、
  obs_title="Astro Celestial Sphere Database（ACSD） Phase1"（可入参覆盖）。
- (1f) 状态句柄 ProductSet 持有：nside/tile_width/data_type/flags/out_dir/
  properties 元数据、moc_cells（叶级 cell 集）、leaf_ipix_list（写序）、
  hier[k]（map<ipix,AncestorAcc>，resize(tile_order)）、
  SNR 点缓存、prov 键（默认 unset）、六段 profile
  计时（同文件）。

## 2. ALG-HIPS-002 — 叶级 tile 信号/支撑归一与 FITS 写出

**源码锚**: `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` 的 `aio_hips_write_signal_support_tile`
（视图校验；缓冲；NESTED→FITS 序；归一；
无效规则；FITS 写；MOC/面积登记，各段均在同一文件）。

- (2a) 叶级局部索引映射：`fits_index(i) = nested_local_to_fits_index(i, 9, 512)`
  （三处 scatter 写点全部同式）。权威实现
  `lib/algorithms/shared/healpix/healpix_core.cpp`：NESTED 交错位解（x=偶位、
  y=奇位，nested_local_to_xy→nest_to_xy）后
  `fits = (511−x)·512 + y`——即 `docs/science/unified/DATA_SEMANTICS.md`「坐标语义」一节冻结的 (511−x)·512+y
  （CDS Hipsgen MAPTILES 对拍冻结，`lib/algorithms/shared/healpix/healpix_core.h` 头注释）。tile 内
  512×512，与父 cell parent_ipix（NESTED，Norder K）拼接出全局 HEALPix 索引。
- (2b) 归一（现行实际是先由上游 sink 把 Drizzle F/N 转换成 AIO 可消费 F'/D；support 非真实面积并集，详见D7-C正文；
  `docs/science/unified/DATA_SEMANTICS.md`「面亮度单位的推导」一节）：对 tile 内每局部像素 p（512²）：
  `signal[p] = flux_sum[p] / covered_area[p]`，
  `support[p] = covered_area[p]/A_cell`，`>1 钳 1.0`（`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`）。
  **量纲（逐项）**：flux_sum [ADU]、covered_area [sr] ⇒ `signal` **[ADU/sr]**
  （面亮度，`FZ-UNIT-SIGNAL-SB` FROZEN；推导见 `docs/science/unified/DATA_SEMANTICS.md`「面亮度单位的推导」一节量纲链表）；
  `support` **无量纲**（sr/sr，∈[0,1]）。**`signal` 的读法 = 面亮度 [ADU/sr]**；另一种读数是"每像素计数"
  （裸 ADU）：两者相差 1/A_cell 的立体角因子，且跨像元尺度不可比。
- (2b-上游) Phase1 编排（`lib/infrastructure/scheduler/src/module_adapters.cpp` 的 p1_op_writer）必须按
  uint8 支持度面连续缩放 `covered_area = (support/255)·A_cell`，并置 `valid_mask`=
  本 parent 实际触及叶像素（未覆盖偏移一律取未触及态，与上一 parent 缓冲无关）；
  **禁用** `support>0 ? A_cell : 0` 这一写法——它把任意部分覆盖塌缩为满覆盖
  （AIO 侧 `support=area/A_cell` 恒 1，负例判据）；provenance
  `covered_area_model="support_ratio_x_A_cell"`。
- (2c) 无效规则（`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`）：当 `valid[p] && area[p]>0 && isfinite(flux[p])
  && isfinite(area[p])` 为假 → signal=NaN、support=0；
  signal_min/signal_max 遍历有限值更新（供 properties
  hips_data_range）。IEEE NaN 填充（无 FITS BLANK 整型卡）。
- (2d) FITS tile：`write_fits_image`（vendored CFITSIO 4.6.4）当前是单文件**私有tmp→内容/checksum验证→fsync→原子rename/目录fsync**的发布壳；其内部`write_fits_image_raw`确有先remove(tmp)再fits_create_file(tmp)，但**不是在正式tile路径上直写**。多文件完整revision仍未实现，详见D7-C及DISP-HIPS-004历史纠偏。cards
  PIXTYPE=HEALPIX / ORDERING=NESTED / COORDSYS=C +
  OBJECT/FILTER/EXPTIME/DATE-OBS + NSIDE/FIRSTPIX="0"/
  LASTPIX="262143"（调用方三处同文件；声明性头卡，
  DISP-HIPS-012）；`fits_write_pix` 行主序一次写 512×512（
  bitpix −32/−64 随 data_type）；`fits_write_chksum`（DATASUM/
  CHECKSUM 完整性）。路径 `Norder{K}/Dir{(ipix/10000)*10000}/Npix{ipix}.fits`
  （IVOA REC-HIPS-1.0 §4.1；**禁用** Dir=商/Npix=余数这一与标准相反的映射；
  tile_rel_path 位于本文件 aio_hips_writer.cpp 的匿名命名空间内）。
- (2e) 登记副作用：FITS 写失败 rc=−4（signal）/−5（support）；
  MOC 叶级 cell（moc_cells.insert）、moc_area_sr += A_cell(K)、
  covered_area_sr += tile_covered、leaf_ipix_list 追加（均在同文件）
  （写序即 leaf 顺序）。

## 3. ALG-HIPS-003 — 方差/逆方差产品

**源码锚**: `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` 的 `aio_hips_write_variance_tile`
（参数校验；缓冲；主循环；全无效；
FITS 写；hierarchy 登记，各段同文件）。SCI 锚：SCI-DRZ-001 的 finalize_tile 段
（finalize_tile 方差传播，方差分子已由 P1-DRZ 链生产，本模块只做除法归一）；
`docs/science/unified/DATA_SEMANTICS.md`「方差与逆方差的三态编码」一节（DATA-HIPS-VAR-001/DATA-HIPS-IVAR-001）。

- (3a) CURRENT 四态：有覆盖且vnum>0有限时variance=vnum/area²、ivar=1/variance；有覆盖且vnum==0时variance=0、ivar=0是**精度未知的显式哨兵态，不是零噪声**；无覆盖时两者NaN；area非有限或有覆盖的vnum负/非有限则返回损坏rc=-6。见下方D7-C独立状态表。
  var_n[i] 缓存（供 hierarchy）取 `vnum` 当 `valid && area>0 && vnum>0`
  （同文件，不含 isfinite——与写盘条件略有差，如实登记）。var_num_sum
  语义=Σ v_j·w_jp²（drizzle 侧逐像素分子，`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp` 传入），
  本模块不改传播公式（与 DISP-DRZ-007 的行漂移说明相接）。
- (3b) 全无效 tile：该 tile 无任何有效样本 → 返回 −5 **不写文件**
  （显式失败，不静默产空 tile；调用方须自行预判跳过——sink 侧对 −5 跳过
  该 tile 计数 n_variance_skipped，`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`，不因此
  abort）。参数校验 rc=−2（var_num_sum 缺失）/−3（视图不匹配）
  /−4（parent_ipix 越界）（均在同文件）。
- (3c) FITS 写同 (2d)：`variance` 目录 tile 写失败 rc=−6、`ivar`
  rc=−7。
- (3d) hierarchy 登记：`acc.add_var(z, var_n[i])`（add_var
  直接累加方差分子，权重恒 0 不另乘；均在同文件）。

## 4. ALG-HIPS-004 — hierarchy 低阶 tile 聚合

**源码锚**: `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` 叶级同步累加（signal tile 内与 variance tile 内
同构段，`leaf_ipix_list.push_back`）（AncestorAcc 与
add_var 同文件）；finalize 落盘 `finalize_hierarchy`
（逐层循环内经 `write_hierarchy_cell` 写各阶 FITS，流式写出共用同一函数）。IVOA HiPS hierarchy
（上采样满足任意浏览器）的写出侧实现合同；SCI 层零覆盖（读侧 SCI-P3-001
只规定消费），按实现+IVOA 语义登记。

- (4a) 父索引下降：对叶 tile（阶 K，tile 阶 T=K−9）内局部 NESTED 索引 l
  （512×512 展平 2^18），对 k=T−1..0 降序：`z = ((s<<18)|l) >> 2(T−k)`
  （同文件，s=tile 内 NESTED 段），得阶 k 的父 cell 索引——标准 HEALPix
  NESTED 4 分叉父子关系（Górski 2005）；叶 tile→父 cell 分离
  `leaf_to_tile_nest = leaf_ipix >> 2(L−K)`（`lib/algorithms/shared/healpix/healpix_core.h`）。
- (4b) 逐父 cell 面积加权层级归约（现行采用**稀疏f64分块+完备即写出释放**；原后续f32累计描述是历史实现，默认f32_step仅在故障注入时可达）：
  `flux_n += sig·a`、`area_n += a`，其中 **`a` = 未钳制真实覆盖面积**
  （叶级 `area_true`→`scratch_area_n`），`sig` = 发布面叶级
  signal（f32 产品取 float 截断后的值）。父级面积恒等式为 `area_parent = Σ area_child`（未钳制真实覆盖面积之和），发布面再取 `support = min(area_parent/A_cell_k, 1)`；钳后值不可反推真实面积，Σa>A_cell_k 的倍数信息在发布面丢失并按计数键登记（见 (4c)）。**归约权重取自未钳制真实覆盖面积；钳后的 support 只作发布值，不进入
  权重**：support 是 a/A_cell 的钳后发布值，用它反乘等于把父级面亮度降为
  sup 加权均值；异质覆盖（a>A_cell）下父级通量出现**本可避免的损失**
  （M2a-H-3；R-7 实测 sb=10@c=4 与 sb=0.1@c=1 混合域：面积加权 8.02 vs
  旧式 5.05，损失 37.032%→0）。方差分子直接累加
  `var_n += var_num`（同文件，var_num 本身已是
  Σv_j w_jp² 分子，不另乘权）。AncestorAcc 保持 f32/f64 双轨（is_f32
  ；**当前正常生产的累计均为double，即使最终产品位深f32也只在写出时转换**；历史float累加风险已有实现修复，DISP-HIPS-009只保留历史/故障注入对照）。同一父 cell 按叶写序单线程顺序累加（map 条目
  首次遇到时创建）→ fixed_reduction_order（无并行求和漂移）。
- (4c) 落盘：finalize 时对每阶 k<T：nside_k=2^(k+9)、
  A_cell_k=4π/(12·nside_k²)；cell 归一 signal=Σflux/Σarea、
  support=min(Σarea/A_cell_k,1)（层级面钳制点，`finalize_hierarchy` 内
  `sup = area/A_cell_k; if (sup>1.0) sup=1.0`；叶级另有各自发布面的钳制点
  （同文件）⇒ **每个发布面各钳制一次**，两处计数键不同：叶级
  `acsd_support_clamped_pixels`、层级 `acsd_coverage_gt1_pixels`；
  Σflux/Σarea 用的是 (4b) 的未钳制面积）、variance/ivar 同 (3a) 用
  Σarea——与叶级公式同构；对齐 IVOA"低阶像素=子像素聚合"。**编码限
  （M2a-H-3）**：support 是 a/A_cell_k 的钳后值，Σa>A_cell_k 时父级真实覆盖
  面积**不可由产物复原**（sup=1 丢失倍数信息）；该情形逐像素计数写入
  properties 与 manifest（`acsd_support_clamped_pixels` 叶级钳制像素数
  （叶级与层级两处计数键，均在同文件）；`docs/detail/mosaic/modules.md` 同款声明）
  约定。FITS cards ORDERING=NESTED + NSIDE=2^(k+9)（k = 该 hierarchy 阶，
  nside_k = 2^(k+9) 与叶级 ilog2 语义一致；**不是 tile 阶**——tile 宽恒 512=2^9，
  故 k 阶的 NSIDE 比叶级 nside 低 2(K−k) 个数量级）。三处 scatter 同 (2a) 式
  （同文件）。
- (4d) 边界：hier 仅对**被写过的父 cell** 建立条目（map 语义），但已建
  条目的空 acc（count 全 0）仍会写全 NaN tile（不检查 count），
  且 hierarchy 目录无独立 MOC（如实登记，DISP-HIPS-011）。tile_depth≠9
  的视图在 sink 层拒绝（`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`），本模块对
  width≠512 视图 rc=−2（同文件）。hier 每层 finalize 重新分配
  sigF/supF/sigD/supD 四个 262144 缓冲（未入句柄 scratch，
  内存峰值点）。

## 5. ALG-HIPS-005 — MOC、properties、SNR 产品与 manifest 收尾

**源码锚**: `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` 的 `write_moc_fits`（原子包装 + raw 体）、
`write_properties`、`finalize_image_product`、
`finalize_hierarchy`、`finalize_snr_product`、
`aio_hips_set_drizzle_provenance`、`aio_hips_finalize`、
`aio_hips_abort`（均在同一文件）。

- (5a) MOC：叶级 moc_cells（2^K cell）→ **UNIQ 编码
  `uniq = 4·4^moc_order + (c >> 2·(tile_order−moc_order))`**（同文件，
  SNR 同构），排序去重后 `write_moc_fits`（BINTABLE
  列 "UNIQ" TFORM=K；MOCORDER/PIXCOUNT 头键；checksum；
  空集不写；remove+create）。moc_sky_fraction =
  moc_area_sr/4π（finalize）。
- (5b) properties：当前`write_properties`组装key=value文本后通过`aio_atomic::write_file_atomic`写出并传播失败；原裸fopen静默失败已修。其余键定义见下：IVOa 关键字
  creator_did（缺省 ivo://acsd/phase1）/ obs_title / obs_creator=
  "ACSD"/ **hips_version="1.4"** / hips_order=K /
  hips_tile_width="512" / hips_frame="equatorial"（IVOA REC-HIPS-1.0 §4.4.1 标准值域 {equatorial, galactic, ecliptic}；
  M1a-B-005 前写非标准值 "equatorial"）/ dataproduct_type="image" /
  dataproduct_subtype（signal=surface brightness、support=coverage fraction、
  variance=variance、ivar=inverse variance，由 finalize 传入）/ hips_tile_format="fits" / **hips_status="private master"（恒值，DISP-HIPS-003）** / hips_creator / hips_builder="ACSD
  aio_hips_writer (CFITSIO 4.6.4)" / **hips_estsize="1000000" 硬编码（DISP-HIPS-002）** / hips_release_date+hips_creation_date=真实 UTC
  （utc_now_date/utc_now_iso，gmtime_r 固定格式不依赖时区；
  META-001 禁伪造）/ obs_description / prov_progenitor=ivo://acsd/
  phase1/drizzle/ [prov_set 时] ACSD_DRIZZLE_PIXFRAC（%.6f）/
  ACSD_DRIZZLE_SCALE_ARCSEC（%.4f）（同文件；`set_drizzle_
  provenance` 亦同文件，**M9-F-3 合法域**：pixfrac∈(0,1] rc=2、
  scale 必须有限且 ∈(0, ACS_HIPS_MAX_FRAME_SCALE_ARCSEC=824.5167388361774″]
  rc=2，**每次拒绝都 set_error 点名原因**；通道全或无——接受即两键齐备落盘，
  不再有 "scale>0 才写" 的静默省略分支。上界推导：叶级 nside≥512 ⇒ 最粗叶像素
  412.258369″，经 SCI-DRZ-001 冻结的 1–2× 过采样 ⇒ 帧尺度 ≤2×412.258369″；
  该界是物理/表示域，**不是** k_corr 查表域 [300,600]″）/ obs_regime
  / hips_hierarchy / **hips_pixel_scale = (180/π)·√(π/3)/nside deg（IVOA REC-HIPS-1.0
  §4.4.1 单位=度；**禁用**多乘 3600 写角秒。帧尺度上界常量
  ACS_HIPS_MAX_FRAME_SCALE_ARCSEC 仍是角秒内部量，不改）
  （同文件，%.6f）** / **hips_initial_fov="60" 硬编码（DISP-HIPS-002）** /
  **moc_sky_fraction（与 manifest.json 共用唯一格式化函数
  fmt_sky_fraction = %.17g（三处共用，同文件）；DBL_DECIMAL_DIG ⇒
  strtod回程精确，[S-1]「判据与误差」一节的 <1e-9 绝对容差与键值精确两条无条件成立；**禁用**
  properties 与 manifest 各用一套格式化（std::to_string 6dp / %.8f）——
  双面字面量必然分叉，违反逐字符相等）** /
  acsd_covered_sky_fraction / acsd_signal_dtype /
  **acsd_support_clamped_pixels + acsd_coverage_gt1_pixels（properties 与 manifest 两面，
  同文件；M2a-H-3 钳制计数，见 (4c) 编码限）** /
  [非空] hips_data_range（sig_min≤sig_max 才写，同文件）/ **hips_ordering=
  "NESTED" 恒写（消费侧 coverage 断言 NESTED）** / **obs_filter
  恒写（含空值；空串=显式"无 filter"声明，**禁用**"键缺失"写法）** /
  [exposure>0] obs_exptime / [obs_date] obs_date +
  t_min+t_max（iso_to_mjd 固定纪元换算，t_max=t_min+exposure/86400，
  ；同文件）。另写极简 metadata.fits（PIXTYPE/ORDERING=NESTED/NSIDE/
  HIPSTILEWIDTH=512/DATAPRODTYPE，remove+create；同文件）。
- (5c) SNR Catalogue HiPS：`finalize_snr_product`——无点整体不写；
  点 (ra_deg,dec_deg) 经共享 HEALPix 权威
  `acsd::healpix::ang2pix_nest(2^tile_order, ra, dec)` 落 cell
  （radec_to_xyz→xyz_to_hp→xy_to_nest，`lib/algorithms/shared/healpix/healpix_core.cpp`，
  源 astrometry.net healpix.c，astropy-healpix 百万点 oracle 已对拍）；
  按 cell 聚类写 TSV tile（NorderK/DirD/NpixN.tsv，同文件）：头注释行 +
  列 star_id(ra 格 %lld)/ra/dec（%.12f deg）/snr/quality_flags/
  photometric_status，**SNR-PREC-001 精度合同：FP32 %.9g、FP64 %.17g
  （round-trip 无损）**；SNR properties（同文件）：
  dataproduct_type=catalog / dataproduct_subtype=snr / hips_tile_format=
  tsv / moc_sky_fraction=cells·A_cell(K)/4π / **hips_cat_nrows=点数** /
  **hips_initial_ra/hips_initial_dec=真实源位置中位数（注释
  明示非伪造）**；metadata.xml=VOTable 1.3（Hipsgen LINT[4.4.3] 要求根
  元素 votable；FIELD 含 datatype/unit/ucd，同文件）；SNR Moc.fits
  （同文件）。
- (5d) manifest.json（当前由`aio_atomic::write_file_atomic_stream`在**全部异步tile发布池两次排空且子产品完成**后最后写出，读侧仅在manifest声明产品时消费）：format_version=1、hips_version="1.4"、
  nside/tile_width/data_type、products 列表、n_leaf_tiles=leaf_ipix_list
  .size()、moc_sky_fraction、acsd_covered_sky_fraction、signal_dtype。
  **无 COMPLETE 状态字、无 sha256 树哈希**——发布侧语义在 IO-003
  （lib/infrastructure/aio/io/hips_output_store.py 临时写→fsync→fitsverify→sha256→原子
  rename→manifest COMPLETE），C++ writer 与发布层的对齐边界=DATA-P1-HIPS
  [S-1]「判据与误差」一节，两合同均如实登记（writer 无事务、发布层有；tree hash 仅存在于
  发布层）。
- (5e) 终止语义：`finalize`——重复 finalize rc=−2（finalized
  位）；逐产品失败 rc=−3（signal 子产品）/−4（support）
  /−7（variance）/−8（ivar）/−5（hierarchy）/
  −6（snr）；最后写 manifest、delete ps（均在同文件）。
  abort **仅 delete ps，不删除已写文件**——`lib/infrastructure/aio/include/aio_hips.h`
  注释"清理已写部分(尽力)"与实现不符，DISP-HIPS-001；部分失败残留处置归
  调用方/IO-003 发布层）。每子产品独立 properties/Moc/低阶 hierarchy
  （文件头注释，variance/ivar 各自独立 hierarchy tiles）。
- (5f) last_error：thread_local g_hips_error（同文件，每次入口 clear 于三处公开入口；
  `aio_hips_last_error` 同文件），跨 ABI 无异常抛出；
  错误码无集中枚举（DISP-HIPS-007）。

## 6. 与 SCI 层的对应与缺口（不反向修改）

- SCI-DRZ-001 的 finalize_tile 方差实现锚与 support=D_p 锚为本模块
  唯二 SCI 锚；(2b)/(3a) 与之严格一致（support=min(D_p/A_cell,1) 的集合级
  截断与无效规则为实现语义，`docs/science/unified/DATA_SEMANTICS.md`「三个基本对象的语义」一节冻结）。
- **SCI 缺口（如实登记）**：HiPS 写出合同（tile 切分/hierarchy 聚合/properties
  键集/publish 协议）在 docs/science/ 无 SCI 级条目——由本文件 ALG-HIPS 承接；
  `docs/science/unified/科学范围 [S-2]「处理链」一节 仅产品级目标，SCI-P3-001 为读侧消费合同。SCI 化候选
  变更走 SCI 变更流程（`docs/ACSD_DESIGN.md` [D-1]「文档权威与索引」一节：独立证据证明文档有误时订正文档是义务，
  SCI 层订正须记录证据与影响面并做一致性回归）。
- `docs/science/drizzle/DRIZZLE.md` 指向的 DISP-DRZ-007（方差行漂移）涉 `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`
  与本文件 (3a) 接口，本模块不改传播公式。
- 上游 drizzle 计算为 OpenMP 行级并行（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`，
  schedule(static)，线程数不污染 ICV），线程私有 tile 累加 + 线程 0
  串行合并后单线程调 writer（`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`）——
  writer 自身单线程串行消费，与 [S-1]「参考文献与参考代码」一节 确定性一致。

## 7. 确定性

fixed_reduction_order：(4b) hierarchy 单线程按叶写序逐父 cell 累加（同一父
cell 依写入顺序累计、finalize 按 map 序写出）、(2e) leaf_ipix_list 记录
写序、FITS checksum 内嵌可复现校验。properties 的 UTC 时间戳（(5b)）按
META-001 使用真实时间 → properties/manifest 字节不跨运行复现（合同本身
如此，非缺陷）；科学 tile 字节（signal/support/variance/ivar/MOC/TSV）同
输入同序逐字节可复现。数据类型双轨（f32/f64）由 data_type 一次性决定
（begin 固化），不混写；hips 精度分层见 §8。

## 8. 精度与 dtype（以当前 f64 稀疏父阶与明确precision状态为准）

产品BITPIX受begin中data_type控制：f32/f64在写tile时分别采用-32/-64，归一比例先以double计算再存储到相应精度。**CURRENT正常AncestorAcc是稀疏double块累加Σflux/Σarea/Σvar_num**，旧版“f32产品用float累加”的风险已有实现修正；f32_step只是测试故障注入。父阶将源叶已存精度的面亮度乘未钳制area累加，不能宣称比输入已量化值凭空获得新精度。面积分母趋近0、NaN值资格、var=0/ivar=0编码未知精度应分别处理；signal=flux/area与variance=var_num/area²仅在正有限的同一面积和同一物理估计量下成立。HiPS data_range只统计有限signal。SNR TSV用FP32 %.9g、FP64 %.17g保留数值round-trip；真正Catalogue不是canonical精度模型，更多状态详见D7-C §12。

## 9. TEST-HIPS-DESIGN-001 — 冻结测试设计（可执行测试归 P1-HIPS-TEST）

- **fixture**：F1 合成小天区（K=9, nside=512, 单 tile 单元 12 cell 数量级；
  flux/area/var_num 解析可控）；F2 多 tile 覆盖同父（hierarchy 聚合闭合）；
  F3 含 NaN/0-area/负 flux 边界像素；F4 SNR 点集（已知 ra/dec→cell）；
  F5 prov 键完整/缺省两态；F6 f32/f64 双 dtype；**G 异质覆盖 c>1
  （p1hips_fixtures.hpp `fix_hips_g_hetero_coverage_tile`：逐像素交替
  sb=10@c=4 与 sb=0.1@c=1 ⇒ 面积加权真值 8.02 vs 旧 support 加权 5.05，
  是 (4b) 归约式的**判别面**——c≤1 全域两式仅差浮点重结合，无判别力）**。
  生成器入 tests（testkit
  规则：fixture generator，不内嵌生产算法；现有 e2e fixture 生成器
  属 e2e fixture——注意其照抄 estsize/fov 占位值的两处，P1-HIPS-TEST
  须以解析值替代）。
- **oracle**：独立朴素实现（不调用生产 symbol，不复制源码公式——用
  HEALPix 独立库或解析解）逐像素重算 signal/support/variance/ivar 与
  FITS 局部索引（`docs/science/unified/DATA_SEMANTICS.md`「坐标语义」一节 (511−x)·512+y；外部对拍可取 CDS
  Hipsgen 样例）；FITS 头键精确匹配；MOC UNIQ 精确（式见 (5a)）；
  hierarchy 父像素=子像素精确聚合（NESTED 4 分叉）；**层次闭合 oracle 的权重
  必须取 fixture 输入的未钳制覆盖面积（权重面唯一来源）；产物 support（钳后值）只作被检对象，不参与反乘
  ——否则 oracle 与实现同源、对 (4b) 型错误恒绿（负例判据：同源 oracle
  对等价缺陷注入必须判红）**；moc_sky_fraction 的序列化
  判别点必须取十进制非有限小数（oracle 组 O6 用 N=1,K=0 ⇒ 1/12：
  6dp 偏 3.333e-7 = §9 容差的 333 倍，且 properties↔manifest 字面量必须
  逐字符相等）。独立复检生态参考：
  HIPS_VERIFY（orchestrator 的相关段，经 aio_hips_reader）与
  gate7_hips_validate.py（lib/algorithms/photometry/.../gate7_hips_validate.py，
  astropy 独立复检 tile 头/DATASUM/support∈[0,1]/F=signal×support×A_cell/
  MOC↔叶级一一对应）——其不变量与 I1-I8 相容，可作 oracle 参照实现。
- **不变量**：I1 逐像素 signal=flux/area；I2 support=min(area/A_cell,1)≤1；
  I3 无效规则→NaN/0；I4 variance/ivar 互为倒数（有限域）；I5 MOC cells=
  非空叶 cells（order K）；I6 moc_sky_fraction·4π=moc_area_sr；
  **I7 hierarchy 逐阶聚合闭合 = 面积加权（权重为未钳制真实覆盖面积 a；
  发布面 support=min(Σa/A_cell_k,1) 是编码限，只作发布值，不回流当归约权重），
  f64 域 bitwise（c>1 亦然）；f32 路径按 §9 容差（accumulator 漂移）**；I8
  manifest 计数字段=实际文件数；I9 double finalize rc=−2；I10 abort 后
  句柄不可复用；I11 历史式 F=signal×support×A_cell 仅当support未钳制、被估计量/源面积和分母一致时有意义；**科学signal允许负值**，且当真实area>A_cell后support被钳为1，该乘积不能复原F；
  I12 SNR TSV 数值 round-trip 精确（SNR-PREC-001）。
- **负面矩阵**：nside<512、tile_width≠512、非法 dtype、越位 flags、
  parent_ipix≥12·4^K、width≠512、var_num NULL、全无效 variance tile（−5）、
  FITS 路径不可写（−4/−5/−6/−7）、重复 finalize（−2）、SNR metadata.xml
  不可写；**prov 合法域（M9-F-3）：pixfrac∉(0,1] rc=2、scale 非有限/≤0/
  >824.5167388361774″ rc=2，每次拒绝 last_error 必须点名原因（N5）；
  跨边界结构 ABI（V11-N-01）：struct_size/abi_version 与调用方编译期布局不符
  ⇒ 入口 rc=−9 且 last_error 点名 ABI（view/variance/diag/snr 数组逐元素/
  兼容入口 tile 数组，N11）**。
- **冻结容差**：f64 通路逐像素 bitwise（同序确定性）；f32 存储 rtol=1e-7
  （单次乘除舍入界）；hierarchy f64 累加对 oracle bitwise、f32 累加路径
  rtol=1e-6（f32 累加器漂移界；DISP-HIPS-009）；sky fraction
  绝对误差 <1e-9 **且字面量必须 strtod 回程精确（%.17g；判别点取 1/12 这类
  十进制非有限小数，见[S-1]「判据与误差」一节oracle）**；MOC/properties 键值精确相等
  （moc_sky_fraction 在 properties 与 manifest 两面**逐字符**相等）；checksum
  字段由 CFITSIO 重算一致。
- **精度分层**：f32/f64 两 data_type 全矩阵重跑；f32 输入 + f64 累加器
  路径断言无交叉污染。
- 性能/资源：单 tile 写耗与内存水位 smoke 记录（非冻结容差，登记即可）。

## 10. DISP-HIPS-001..012 — 历史实现偏差清单（已修与未闭合须按D7-C现行源码重新判定）

| ID | 严重度 | 描述 | 锚 | 处置建议 |
|---|---|---|---|---|
| DISP-HIPS-001 | 高 | CURRENT abort会先排空发布池、再删除ps，不回滚已经落盘的子产品，残骸没有成功manifest；`lib/infrastructure/aio/include/aio_hips.h` 注释称"清理已写部分(尽力)"——合同与实现不符，部分失败产品残留无 rollback。matrix 专项"partial failure rollback"如实登记为缺口 | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（abort 段）；`lib/infrastructure/aio/include/aio_hips.h` | IMPL 事务化（写临时目录+发布切换）或头注释降级声明+文档化调用方清理责任（与 IO-003 对齐） |
| DISP-HIPS-002 | 中 | properties `hips_estsize="1000000"`、`hips_initial_fov="60"`（image 与 SNR 两处）硬编码占位，无真实估算/校验 | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（image 与 SNR 两处） | 按产品目录真实字节数与天区极值估算；e2e fixture 生成器照抄处需同步 |
| DISP-HIPS-003 | 低 | properties `hips_status` 恒 "private master"，无公开/克隆状态参数化 | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（image 与 SNR 两处） | 参数化或确认产品定位恒私有 |
| DISP-HIPS-004 | 高 | **旧原子缺陷已部分修复，仍无统一多文件revision**：tile/MOC使用私有tmp+checksum/fsync+atomic rename，properties与manifest采用aio_atomic；但metadata.fits与SNR TSV仍存在局部直写，abort不清理整棵目录，P1 HiPS和frame_science不共用完成指针。历史表述“所有tile都在正式路径直写”作废。finalize 中途失败（−3..−8）已写子产品残留；同 out_dir 重跑与旧运行残留混合。原子语义由 IO-003 Python 发布层承接（临时写→fsync→fitsverify→sha256→原子 rename→manifest COMPLETE）——两合同边界在DATA-P1-HIPS [S-1]「判据与误差」一节登记对齐，writer 层不冒认已原子。对照：HISS 容器有 .partial/.tmppool+atomic_replace（`lib/infrastructure/aio/src/hiss_stream_writer.cpp`）但 writer 未采用 | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`；`docs/engineering/contracts/ATOMIC_PUBLISH.md` | INT 层接线（writer 写 staging 由 IO-003 消费）或 writer 内嵌事务；tree hash 归属待定 |
| DISP-HIPS-005 | 低/中 | 入参 moc_order 静默 clamp（min 与 tile_order）无告警；且 moc_order<K 时 Moc.fits 含低阶 UNIQ，而自家读侧 `lib/infrastructure/aio/src/hips/aio_hips_reader.cpp` 仅保留 order==K——低阶 MOC 对自家 reader 无效（Moc.fits 为 optional hint，不影响覆盖判定） | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`；`aio_hips_reader.cpp` | 强制 moc_order=K 或 reader 兼容低阶 UNIQ |
| DISP-HIPS-006 | 中 | CFITSIO 裸调未包装进程级互斥锁（同库 `lib/infrastructure/aio/src/aio_fits.cpp` 与 `aio_hips_reader.cpp` 均用 aio::cfitsio_io_mutex）——writer 写路径完全无锁；单句柄串行使用无影响，未来多句柄/多线程写同进程将静默竞争（现生产链=drizzle 合并后单线程写，`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`，暂无并发场景） | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` 全文件无 mutex | 统一包装 mutex 或显式登记单句柄使用约束 |
| DISP-HIPS-007 | 低 | 错误码无集中枚举且正负混用（write/finalize 负码 −1..−9（含 ABI 不匹配 −9，V11-N-01）vs provenance 正码 1/2；语义仅注释）——ABI 演进风险；last_error 每入口 clear，跨调用不可追溯 | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（各公开入口与各子产品写出/收尾段） | 集中枚举 + 头文件公开 |
| DISP-HIPS-008 | 低 | `aio_hips_write` 兼容入口旧语义：signal=F、support=uint8 0..255 → covered_area=su/255·A_cell、flux_sum=signal·su/255（8bit 量化损失），flags 固定 ALL（无 variance/ivar），与新 AstroSphereTileView 连续 support 语义并存易混用 | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（兼容入口与其转换段） | 标记为非推荐入口或内部转换告警 |
| DISP-HIPS-009 | 已修/回归 | **正常父阶稀疏double累计**；历史float运算仅f32_step故障注入用于负例检测，不是当前的正常f32路径 | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（AncestorAcc 定义与逐父累加段） | f32 产品仍用 double 累加（存储时再截断）或登记精度边界（TEST-HIPS-DESIGN-001 容差已按 1e-6 冻结） |
| DISP-HIPS-010 | 部分已修 | fits_str超长静默截断与properties文本值转义仍需控制；**write_properties现在用aio_atomic并向finalize传回失败**，SNR TSV仍为fopen逐tile，不能把已修错误传播再记未修 | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（fits_str/write_properties/SNR tile 三段） | 截断告警；properties 值转义/校验；失败传播到返回码 |
| DISP-HIPS-011 | 部分已修 | hierarchy使用稀疏double分块、完备即流式写释放；write_hierarchy_cell仍为每cell分配输出缓冲、空acc/稀疏覆盖域需要独立文件数对照；父层无独立MOC的限制另验 | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（finalize_hierarchy 缓冲分配段与空 acc 写出段） | count==0 跳过；缓冲入句柄 scratch 复用 |
| DISP-HIPS-012 | 低 | FIRSTPIX/LASTPIX 声明性头卡（恒 "0"/"262143"）全仓无消费方（仅 fixture 断言字面值）——冗余契约，语义漂移风险低 | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（三处 tile 写调用点） | 保留并文档化或删除 |

登记级事项（非缺陷，如实登记）：逐像素标量 scatter 调用
nested_local_to_fits_index（prof_transform 热点段之一，性能优化归
IMPL）；全无效 variance tile 以 rc=−5 报错、调用方须预判（设计张力）；
UTC 时间戳致 properties/manifest 字节不跨运行复现（合同，§7）；无取消/
检查点（长 finalize 不可中断，README §7）。

## 11. 关联

- 上游：SCI-DRZ-001（docs/science/drizzle/DRIZZLE.md，共享引用）、SCI-SCOPE-001；ALG-DRZ-001
  （tile 累加上游，DRIZZLE_GEOMETRY.md）；ALG-HEALPIX-001（NESTED 核心，
  HEALPIX_MAPPING.md 索引卡，lib/algorithms/shared/healpix 权威实现——healpix_drizzle
  内 healpix_core.h 为 DEPRECATED shim，healpix_stack 系列函数全仓零调用
  已死代码化）。
- 下游/合同：DATA-P1-HIPS（`docs/detail/infrastructure/hips_writer.md`）、API-HIPS-001
  （PUBLIC_API.md）、API-P1-007（编排级 hp_drizzle_run_hips 区间）、
  ARCH-001、IO-003（发布合同，对齐不越权）、SCI-P3-001（读侧消费）。
- 模块：MOD-acsd-phase1-hips-writer（lib/algorithms/drizzle/hips/README.md、module.yaml、
  registry acsd.phase1.hips-writer.md）；traceability 行
  MOD-acsd-phase1-hips-writer。
- 相邻（不改）：aio_hips_reader.cpp（P3 读链，HIPS_VERIFY 后端）、
  astro_sphere_sink.cpp（P1-DRZ 写通道 sink）、hiss_codec/hiss_stream_writer
  （独立中间容器，legacy_hiss_compare 开关封闭，CFG-002 关闭 HISS_VERIFY
  `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp`）、orchestrator.cpp（stage 编排）、
  `lib/algorithms/coverage/tools/stage2.cpp`（Phase2 写方）。

> 本文引用上游正本（论文式编号，正文引用处均已改为自然语言节名，不再使用跨文档 §N 跳转）：
> - [D-1] docs/ACSD_DESIGN.md（最高设计）。
> - [S-1] docs/science/drizzle/DRIZZLE.md（守恒映射科学正本）。
> - [S-2] docs/science/unified/科学范围 [S-2]「处理链」一节。
> - [S-3] docs/science/projection/HIPS_TO_FITS.md（投影导出科学正本）。
> - [S-5] docs/detail/infrastructure/hips_writer.md（相关科学正本）。
> - [S-6] docs/detail/normalize/modules.md（相关科学正本）。

## 参考文献与参考代码库（含许可证）

- HiPS 规范：IVOA HiPS 1.0 Recommendation（https://www.ivoa.net/documents/HiPS/；§3/§4.1/§4.2.1/§4.4.1/§6.3.1）。
- HiPS 层级/目录：Fernique et al. 2015, A&A 578, A114（DOI 10.1051/0004-6361/201526075）。
- HEALPix 几何：Górski et al. 2005, ApJ 622, 759。
- FITS 写出/关键字：FITS Standard 3.0；CFITSIO（宽松许可）。
- MOC/properties：IVOA MOC 1.0（https://www.ivoa.net/documents/MOC/）。
- dtype/精度与 hierarchy 聚合：Project-defined（本文件 §2/§4/§8）。

## 12. D7-C 正式重构：公共 HiPS Writer 的现行科学量、接口、精度、事务及独立实验

**原文保全与分层**：以`63eccbb6133aef56389641d43c55df6e243819c5`的427行原版、blob`ae917e6510915845b7de97f1fc5367d838ca7ff6`完整精读为基础，保留历史ALG-HIPS-001..005、测试fixture G、面积加权父层、标准文件布局、MOC/TSV、负例判据及标准出处。本文的旧 §1–11 中与2026-10-17最新版冲突的“直接覆盖正式tile、f32累计、仅flags≤31、NaN精度处理、无原子properties、历史P1权重链”等，应以本节的**CURRENT**取代；仍有效的原数学、参数和实验设计保留作为可执行研究内容。事实源码：`lib/infrastructure/aio/include/aio_hips.h`（405行）、`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（3138行）、`astro_sphere_sink.cpp`、`module_adapters.cpp`、`aio_hips_reader.cpp`与`hips_properties.cpp`。科学正本：[Drizzle](../../drizzle/DRIZZLE.md)、[统一模型](../../unified/UNIFIED_SCIENCE_MODEL.md)、[数据语义](../../unified/DATA_SEMANTICS.md)；接口详见[共用HiPS写入](../../../detail/infrastructure/hips_writer.md)、[Normalize正式数据合同](../../../engineering/contracts/NORMALIZE_DATA.md)、[原子发布](../../../engineering/contracts/ATOMIC_PUBLISH.md)。

### 12.1 真球面 leaf/tile/FITS/MOC 四种“阶”和三个面积

对 HEALPix NESTED，真实leaf阶`K=log2(nside)`，合法leaf `nside=2^K`，当前AIO begin要求2的幂且512≤nside≤2^29。一个标准512×512 tile含`4^9=2^18`个叶；tile目录阶`T=K-9`。在同一个base face内的tile索引`t`和tile局部Morton索引`l`构成全局leaf `leaf_ipix=(t<<18)|l`；其天区身份来自`pix2ang_nest(nside,leaf_ipix)`，而不是tile本地二维数组下标本身。共享`nested_local_to_fits_index(l,9,512)`经低位交错反解(x,y)，转换为`fits_index=(511-x)*512+y`；NorderT/DirD/NpixN.fits，其中`D=floor(N/10000)*10000`，Npix文件名为完整的无符号64bit tile号。AIO reader仅在标准路径缺时尝试旧商/余数目录（只读兼容）。必须在base face缝、极区和大索引上用独立astropy-healpix/HiPS FITS对拍，这一公式不是简单图像旋转即可解释。

真实叶球面立体角`Omega_leaf=4*pi/(12*nside²)` sr；tile覆盖的**整块名义球面足迹**为`4^9*Omega_leaf`。源像元缩小drop面积`A_drop,j`、真实单次交叠`a_jp=area(D_j intersect H_p)`、HiPS叶几何足迹面积Ω_p，以及Drizzle累计归一分母`N_p`、覆盖分配累计`D_p`是**不同量**。由#15已证实的高nside四角geodesic叶近似`Htilde_p`不等于HEALPix真曲边`H_p`；`sum_p a_tilde/A_drop`可内部闭合但真叶membership仍错。极冠示例的约−9.968%是**局部弦边形状面积差**，不是已测最终星点测光损失。修正需真实曲边交叠/等面积chart独立积分和分区完备性，不是writer元数据或sidecar改名能解决。

### 12.2 Drizzle → AIO 写入所估计的量及正确的 k 因子

定义原生每源像元线性积分值`x_j`（物理单位`L`，可能native ADU或应用后的模型单位），实际未收缩球面面积`Omega_j`，drop面积`A_drop,j`，对叶p交叠`a_jp`，权`w_jp=a_jp/A_drop,j`。上游 Drizzle 可用以下等价累计表示：
\[
 F_p=\sum_j w_{jp} x_j,\qquad
 N_p=\sum_j w_{jp}\Omega_j,\qquad
 D_p=\sum_j a_{jp},\qquad
 B_p=F_p/N_p.
\]
`F_p`是按drop分配积分中间量（单位L），`N_p`是将其换算成面亮度的归一面积（sr）；`D_p`是像元/drop footprint重叠的面积累计（sr），**不一定等于叶真实覆盖并集**，多帧/重叠可超过Ω_leaf。一个共同factor`k_p=D_p/N_p`将上游转给AIO：
\[
 {\rm view.flux\_sum}=F_p\,k_p,\quad
 {\rm view.covered\_area}=D_p,\quad
 {\rm view.var\_num\_sum}=V_{F,p}\,k_p^2.
\]
这里`V_F,p`专指**该输出累计分子F_p的条件随机方差**，与#8点源flux估计`V_ref`必须异名区分；若原始随机项独立，`V_F,p=sum_j w_jp² v_j`，一般为`w_p^T C_source w_p`。AIO按`view.flux_sum/view.covered_area`得到`B_p=F_p/N_p`（L/sr），在误差通道上按`view.var_num_sum/D_p²`得到`Var(B_p)=V_F,p/N_p²`（L²/sr²）。**这正是`astro_sphere_sink.cpp::write_hips_phase1`实际使用的`sb_publish_scale(sumArea,sumNorm)`与`k²`修正**；原文若把原`sumFlux/sumArea`当成最终面亮度，在pixfrac<1时会有尺度错误，pixfrac=1作为弱阳性恰可能掩盖缺陷。实验须用pixfrac=0.5/0.8/1及单位D_p/N_p、方差平方比例同时区分。

**上游科学条件**：`x_j`若是native ADU，且由`B=F/N`得出才有`BUNIT=ADU/sr`；若实际 P1 photometry apply由有量纲alpha将x变为另种L，writer虽只做相同数值除法，BUNIT必须跟随`L/sr`、variance跟随`L²/sr²`。#9生产`p1_stack.json`和`declare_hips_surface_brightness_units`仍有固定ADU/sr声明，不能因“源值被乘因子但形式仍是面亮度”否认单位变动；也不能把无量纲相对零点标度误当SI绝对标定。TARGET必须把native/applied/response/scale_state的实际单位和数值变换由`frame_science`与产品properties一致追踪，图像单位不因FITS HDU数减少而消失。

### 12.3 coverage、valid、signal、未知precision与数据损坏五种状态

AIO当前`aio_hips_write_signal_support_tile`每leaf判断`covered = valid_mask_allowed && isfinite(area) && area>0`；已覆盖时无论`flux_sum`是否有限，都写`support=min(area/Omega_leaf,1)`，而`signal=flux_sum/area`仅当flux有限，否则NaN；无覆盖S=NaN/support=0。有限零与负signal均正确。若`D_p>Omega_leaf`会clamp support为1，原始D累计倍数无法由product support逆推；真实层级必须继续用未clamp的area并在`acsd_support_clamped_pixels`记录发生次数。**support不是SNR、inverse variance，也不是并集覆盖面积**。

`aio_hips_write_variance_tile`现行4态需在同一产品metadata声明，而不是把物理0方差解释成最大信息：

| 几何/数据状态 | Signal/support | variance/ivar产物 | 解释与处理 |
|---|---|---|---|
| area≤0或valid=0且area有效 | S=NaN, support=0 | NaN / NaN | 无观测足迹，不可用零亮度填 |
| area>0且signal有限、vnum>0有限 | S可为任意有限正/零/负，support>0 | vnum/area² 与其倒数 | 条件随机精度可用；需真实被估计量与单位配对 |
| area>0且vnum==0 | support>0（S视flux有效性） | **0 / 0** | **库内编码为 precision unavailable**，不是无随机误差、也不得令1/0=∞；必须使用产品及逐像元状态避免歧义 |
| area>0但flux非有限 | S=NaN, support>0 | vnum依实际是否有效单独声明 | 科学signal无效与几何覆盖分离；下游须保留实际原因 |
| area非有限，或有覆盖且vnum负/非有限 | 不可消费为合格产品 | **写出返回负码（variance路径rc=-6）** | **DATA_CORRUPT**，不是unknown precision；不得clamp或把脏模型退化成valid数据 |

上表“0/0”表示两张图的两个**数值零哨兵**，并非有理数`0/0`的运算定义；遇到真实确定性零方差需要一个明确物理模型及另外状态，而不是自动把其视为unknown。全块无`any_covered`时方差函数返回−5（sink对特定情况会跳过并记录），需确认被声明子产品的tile存在集与signal/MOC一致；不能以writer“返回成功”自行判定所有叶方差都有证据。CURRENT库通过flags决定是否尝试VARIANCE/IVAR，不能据公共ABI有四通道就称Mosaic最终多科学图像：Normalize单帧**可有辅助**signal/support/variance/ivar（及Catalogue），而`p2_op_write`真正只选SIGNAL，P2最终single scientific image channel。

### 12.4 hierarchy：数值真相与稀疏f64流式策略

对低一层`k<T`的tile，祖先tile id`A=t>>2(T-k)`，其局部索引`z=((s<<18)|l)>>2(T-k)`（s是t在祖先下面的后缀子tile位）；同一细leaf支撑的低阶目标由父层累加决定。CURRENT `AncestorAcc`使用稀疏按需64个double子块记录flux/area/var-num，而不是历史f32父阶求和；`f32_step`仅用于显式故障注入。因不同tile的local leaf经过完整NESTED父映射不会给同一目标z重复独立贡献，历史“因tile到达序浮点重结合必不可复现”的推断已被源码结构性修正，但不同输入科学帧源本身的融合仍须另按权重重算。`stream_flush_ready_cells`在祖先cell所有预期leaf slots（含方差槽配对）到达后直接`write_hierarchy_cell`、释放子块；`finalize_hierarchy`写剩余尚未完成的cell，属于MEM-DESIGN-01的真实改进而非未来架构设想。

当前父层按正有效信号贡献计算`flux_parent=sum(B_leaf_stored*D_leaf_unclamped)`、`area_parent=sum(D_leaf_unclamped)`、`signal_parent=flux_parent/area_parent`，support仍先除父叶Ω再钳到[0,1]；方差分子从子叶累计后按`V_parent=sum var_num/(sum area)²`。该方差等式仅在其原生成分子及条件协方差/同一光度计量成立时有意义，**跨孩子像元共享原始曝光/Drizzle贡献时不能仅相加孩子方差得到真实跨叶相关合成方差**。不要把派生父层当成独立观测，也不得从被clamp的support乘回真实D或原始F。

**D7-C 静态新风险**：叶级`covered && !isfinite(flux)`会保留support>0但S=NaN；在`aio_hips_write_signal_support_tile`累加父层时读取`flux=sig_n[i]*area_n[i]`，随`if (!isfinite(flux)) continue`把**同一leaf的面积贡献也跳过**。这使某些真实有footprint但信号不可用的位置，在父阶被计为面积0/支持域减小。它是生产条件路径的SOURCE_CONFIRMED而非真实E2E已触发。TARGET需按面积、有限信号与有效估计器分开聚合，决定“几何covered但科学invalid”在父层的表现；外部独立构造一个area>0、signal NaN及旁边正常leaf，要求各阶面积并集和信号状态分开核验。此问题不同于旧DISP-HIPS-009已修的f32累计。

### 12.5 公共C ABI、所有权、失败码与实际两个生产者

`aio_hips.h`是公开C ABI，不是STL跨DLL接口，句柄`AioHipsProductSet*`唯一拥有会话状态、scratch/祖先map/叶MOC/来源。`aio_hips_product_begin`输入out_dir、nside、tile_width、data_type、flags、creator_did、obs_title、obs_filter、exposure_s、obs_date、moc_order，失败返回NULL并通过`aio_hips_last_error`读取最近错误。产品flag数值 SIGNAL1/SUPPORT2/SNR4/VARIANCE8/IVAR16/NREJ32/NUSED64，ALL7、ALL_V19=31、ALL_V20=127；f32=0/f64=1，diag nrej/nused是int32无父层聚合。`AstroSphereTileView`由caller持有并传入，其`struct_size`/`abi_version`必须使用`aio_hips_tile_view_abi_init`（其他SNR/diag/test镜像用各自initializer）；字段顺序parent_ipix:uint64、leaf_order:uint32、width:uint32、data_type:int32、指向NESTED局部序flux_sum/covered_area/valid_mask/var_num_sum。错误布局返回`AIO_HIPS_ABI_MISMATCH=-9`，不得按调用者自己猜sizeof。

生产流程为`aio_hips_product_begin`→`aio_hips_write_signal_support_tile`（必要时`aio_hips_write_variance_tile`/`aio_hips_write_diag_tile`/`aio_hips_write_snr_points`及provenance/set_geometry）→`aio_hips_finalize`，失败时`aio_hips_abort`。调用方保留view数据至少到同步写返回，库不可跨线程用同一ps同时修改scratch/hierarchy。虽然AIO有单文件I/O发布池，并不意味着同一产品集数学归约可被多个线程同时无锁写。返回负码各函数具体不同（参数、未覆盖、FITS写、发布池或ABI），正码的setter通常为无效参数；不能以全局“0成功+其它任意可忽略”替代逐函数具名状态及最后错误。异常跨C API在实现中有try/catch障壁，错误文本不得作为程序分支唯一依据。

**两个真实消费者**：

- **Normalize Phase1**：`astro_sphere_sink.cpp::write_hips_phase1`消费Drizzle累加与`sb_publish_scale`，按真实输入是否有方差决定SIGNAL|SUPPORT及VARIANCE|IVAR，按视图逐tile同步调用并把模型/Frame SNR部分证据写到HiPS；`p1_op_writer`另负责检查正式产品、在完成tile后写/验证`frame_science.json`和模型，产品root/final摘要不等同AIO writer内部完成标志。若precision模型未写出，signal tiles可以存在，但不能叫它“完整且precision unavailable的v2 frame-science产品”。
- **Mosaic Stage2**：`p2_op_write`从真正integrate中间层取得signal/coverage，转为writer要求的NESTED local布局，在`staging`中`aio_hips_product_begin`，当前flags只选SIGNAL，不把P2内部ivar/reject/support变成最终多图层；`aio_hips_finalize`后继续独立校验并发布整体mosaic root。多个相机、波段按自定义混合规则可以参与，但标称滤镜标签/mixed不能伪装为同一物理响应。

### 12.6 科学Identity：#9/#10/#13/#19/#74/#75与产品读侧

#10 **原字段问题的当前变化**：`p1_op_drizzle`如今设置`fdoc.signal.leaf_order=log2(frame_nside)`，不再直接写K−9；但真正`write_frame_science_document/read_frame_science_document`与机器schema仍为v1。`NORMALIZE_DATA` §9冻结的 **唯一frame-science/v2是TARGET / NOT_RUN**，不能把新文档既定schema当已编码上线；其leaf_order=K、tile_order=K−9、球面控制点、64bit ipix十进制字符串、模型hash和source ID必须作为同一次产品revision验证。

#13 **Canonical Precision未被真正消费**：P1`frame_science.json`现在可能写出球面PSF/参考source条件模型，但Stage2`p2_sparse_layer_probe`仍依赖`ACSD_SPARSE_SNR_LAYER`的legacy像素规整层，未读取Canonical v2模型；点源`I_F=(SNR_ref/F_ref)²`并不直接是Mosaic面亮度方差逆，要有同估计量同噪声条件下`T=V_B/V_F`。未知precision是有完成sidecar和具名reason的科学能力状态，而读取sidecar失败/哈希损坏属于DATA_CORRUPT/INCOMPLETE，不能假装unavailable；无sidecar的未完成私有staging又是NOT_APPLICABLE或未提交revision。这样才能使写库的API“可写SNR TSV”不被冒称Canonical精度已贯通。

#19 **跨输入阶**：Stage2部分coverage元数据可合并不同T，但reject/integrate仍可能将同一个tile_ipix/局部index在不同nside下当成同天区，没有真实重映射，造成坐标错配。必须给每帧显式FrameGrid并**同grid才能直接按leaf合并**；否则先按球面面积/有效核重映射signal与相关统计，完整功能前对混阶fail-closed。writer输出最低T的产品不能修复源叠加前坐标错配。

#74 已有实际观测：自动nside最高档写出/sidecar成功与显式512/2048写tile后长时仍不出frame_science有显著差别，但根因**未知**（不能凭近端 writer API推断死循环或归因线程）；复核需分离Drizzle、AIO层级finalize、球面精度序列化和p1_writer每段timestamp/RSS/perf。#75 增量构建`ACSD_COMMIT_SHA`可能陈旧，`software_sha`不能独立证明当前加载CLI/DLL身份；TARGET记录真实构建依赖及二进制hash、dirty/source revision与模型版本，不从同名文件覆盖推断匹配。

HiPS properties包括`hips_order=T`,`hips_tile_width=512`,`hips_frame=equatorial`（ICRS含义）、`hips_ordering=NESTED`,`hips_pixel_scale`单位是**度**，`obs_filter`在image properties即使空也写；`hips_estsize`与`hips_initial_fov`仍可能是历史硬编码占位。MOC用UNIQ整数编码，较粗`moc_order<T`产物读取时`aio_hips_reader.cpp::load_tiles_from_moc`只识别T对应的UNIQ，不会自动展开低阶父cell ⇒ tile列表缺失，须严格配对MOC order或升级读侧。Catalogue TSV点源SNR只为浏览/legacy消费，不能替代frame_science canonical。目录及属性检验由`hips_product_validate`与`aio_hips_open`各有侧重：它们不能仅凭manifest存在证明元数据所有语义已由机器schema验证。

### 12.7 原子文件 vs 多文件完整提交：已有修复与尚缺事务

**旧§2/§5/§10“全部FITS正式路径直接覆盖写、properties fopen失败静默、manifest非原子”已被当前源码否定。** `write_fits_image` 与`write_moc_fits`现在经`write_fits_atomic`使用同目录私人tmp，write/close与可复现FITS checksum、独立verify、fsync、可选经读回验证的全零打洞、单文件原子rename并尽力目录fsync；发布池可后台并行文件尾部并在`aio_hips_finalize`入口和完成标记前两次`publish_drain`。`write_properties`使用`aio_atomic::write_file_atomic`且失败上行；最终`manifest.json`经`aio_atomic::write_file_atomic_stream`在最后发布。`write_chksum_deterministic`使用CFITSIO标准数值校验和但避免UTC变动注释污染文件字节确定性。仍有少量辅助`metadata.fits`或Catalogue TSV通过局部直接create/fopen路径，不能概括为“所有文件原子且都经统一验收”。

**单文件发布和完整science revision是不同事务**：`aio_hips_product_begin`先移除旧manifest/选中子产品目录，启动后失效的对象对AIO reader会因为无manifest而被拒；`aio_hips_abort`等待在途发布后只是删除句柄，**没有自动删除全部已写tile/MOC/目录**。新manifest写出之前失败可留未提交残骸但读侧fail-closed。更高层P2使用自己的run-private staging后再完整发布；P1 HiPS最终清单与`p1_op_writer::publish_staged_frame_science`并非共同commit pointer。旧版本完成目标覆盖与目录fsync失败的可恢复性还需单独实测，不能把单文件atomic_replace等同于所有子产品+model的ACID事务。ARS multi-file revision target是`private staging -> all tiles/model hashes/geometry/BUNIT validated -> durable status -> CAS/serial COMPLETE pointer`；pointer可见但持久化未证应COMMIT_UNCERTAIN，不能盲删正式目标。CURRENT是否真把这套ARS接到三个CLI须逐一查证；本批文档只给实施接口和验证路线。

### 12.8 算法复杂度、内存预算与测试/外部Oracle（D7-C设计，不冒充实验完成）

叶tile信号+support散射每tile固定`512²`次，分子/denominator和FITS输出至少O(Tleaf·512²)时间与实际tile数成正比的I/O，层级若T较大每叶涉及O(T)祖先映射；稀疏hier驻留按当前未释放祖先cell数×已分配`hier_sparse`子块增长，不能表述为严格常数。scratch按dtype、方差和诊断flags按需申请；单文件发布池持有路径/状态、失败先排空，CPU生产写库自身序列化句柄而非共享可写。真实峰值还含源tile集合、压缩/磁盘策略、AIO publish workers、合法SNR点数和临时文件；低nside的#74停滞未有根因剖析。

| ID | 输入/独立Oracle | 正反例与验收条件 | 证据 |
|---|---|---|---|
| H-W01 | nside512/2048/8192、12face/极区、标准Dir/legacy读取、外部healpy或astropy-healpix | 叶K/T映射、NESTED Morton/FITS扇区、MOC UNIQ、目录及checksum逐项一致；伪K−9被识别而非自动修补 | STATIC，E2E NOT_RUN |
| H-W02 | pixfrac0.5/0.8/1、人工F_p/N_p/D_p、含L标度 | 核对k=D/N、S=F/N、Var=V_F/N²与support；故障漏k时值偏差/方差平方项必须红 | 数学/调用链，MC NOT_RUN |
| H-W03 | 负/零signal，D>Ω clamp，D>0但flux NaN，方差0/0、-1、Inf | 三态几何/科学值/精度及DATA_CORRUPT独立读回，任何var=0不进无限逆权重；新父级NaN面积丢失反例需能判红 | SOURCE_CONFIRMED条件风险，注入NOT_RUN |
| H-W04 | 不同叶数/同父、多层与不同输入tile写序、f32/f64 | 未clamp面积加权恢复8.02对5.05旧错、稀疏与稠密oracle同科学值；开启f32_step fault应判红；测真实峰RSS与磁盘吞吐 | 旧oracle设计保留，新NOT_RUN |
| H-W05 | 原子tmp、kill、ENOSPC、fsync、checksum、dir sync、重跑/覆盖 | 正式单tile无半写；根manifest最后发布、失败读侧不可当成功产品；旧产品恢复和P1 sidecar统一revision分别判断，不能凭单个rename自证 | SOURCE路径，故障NOT_RUN |
| H-W06 | frame-science/v1/v2、错误K/T、坏model/hash、unknown precision | v1拒为UNSUPPORTED_SCHEMA，v2完整且precision unavailable为合法信号，损坏为DATA_CORRUPT；P2消费Canonical而非legacy时核实T和单位 | TARGET schema未上线，NOT_RUN |
| H-W07 | 混合阶T0+T1反例（同tip不同sky）和角色交换 | 不同阶输入未重映射时直接fail-closed；完成后用独立天空真值验证不出现ghost、PSF/area正确 | #19 SOURCE_CONFIRMED，E2E NOT_RUN |
| H-W08 | 实测M42低nside显式512/2048 vs auto、增量构建变SHA | 各阶段CPU/RSS/I/O剖面定位#74真实根因，软件SHA用二进制/版本与实际源头对拍#75，不猜循环 | #74/#75既有观察，D7-C NOT_RUN |

参考：IVOA HiPS 1.0 https://www.ivoa.net/documents/HiPS/ 与MOC https://www.ivoa.net/documents/MOC/；Fernique et al. 2015 A&A 578 A114 DOI 10.1051/0004-6361/201526075；Górski et al. 2005 ApJ622 759 DOI10.1086/427976；Fruchter & Hook 2002 PASP114 144 DOI10.1086/338393；FITS4.0 https://fits.gsfc.nasa.gov/fits_standard.html 与CFITSIO https://heasarc.gsfc.nasa.gov/fitsio/；独立 astropy-healpix与Astropy FITS/WCS（BSD-3）。上述论文/标准提供方法与参照，**本D7-C未新下载逐段核对外部文献版本/具体软件许可，亦未执行可编译单测、生产CLI、HST/M42、MC/RSS或跨平台测试**；旧 §9 的test ID只代表设计及历史证据，不是D7-C新增PASS。

