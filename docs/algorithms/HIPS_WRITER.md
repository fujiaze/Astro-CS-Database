# ALG-HIPS-001..005 — astrocs.p1.hips_writer HiPS 产品集写出算法

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）、§10（I/O 与原子产品）

> ID 覆盖: ALG-HIPS-001 ALG-HIPS-002 ALG-HIPS-003 ALG-HIPS-004 ALG-HIPS-005  状态: CONTRACT_READY
> SCI 上游: SCI-DRZ-001（docs/science/DRIZZLE.md，FROZEN，共享引用不改动；
> :130 实现锚 finalize_tile 方差语义、:145 support=D_p 归一语义）
> 与 SCI-SCOPE-001（docs/science/SCIENCE_SCOPE.md，产品目标）；读侧消费合同
> SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，只读引用）。
> 实现源（逐公式锚定）: lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
> （合同头 lib/infrastructure/aio/include/aio_hips.h）。
> 数据语义权威: docs/contracts/DATA_SEMANTICS.md §12（DATA-P1-HIPS；上游 §11 DATA-P1-DRZ、
> §4a DATA-HIPS-VAR-001/DATA-HIPS-IVAR-001、§3 FITS 局部像素映射、§5 帧身份）。
> HiPS 1.0/1.4 外部参照: IVOA HiPS 推荐（Fernique et al. 2015）、HEALPix 算法
> （Górski et al. 2005）——经 SCI-P3-001 :120-124 收录的文献锚，本文件不另立外部断言。
> 本文件为逐公式"算法+源码锚点"登记：凡 SCI 层无覆盖而实现自带的语义（HiPS 写出
> 合同细节），以实现为准登记并标注；凡实现与 SCI 语义冲突处，登记 DISP- 条目。
> **权威订正原则** = `ENGINEERING_SPEC.md` §3「科学正确性优先」：独立证据（外部标准 /
> 文献 / 可复跑实验）证明文档与事实不符时，**订正文档是义务**（SCI 层订正走变更流程并
> 记录证据与影响面）；文档已被证明正确而实现不符时改实现。
>
> **行号锚声明**：跨边界结构 ABI 头/校验、共用 `fmt_sky_fraction`、未钳制面积
> 归约与钳制计数使 `aio_hips_writer.cpp` 行号整体推移（+57 起，最大约 +147）。
> 本文件 §4 (4b)(4c)、§5 (5a)(5b)、§9 的锚已按修订后行号重标；§1–§3 与 §10 DISP
> 表的裸 `:NNN` 以**符号名**为准。`docs/algorithms/anchors/anchor_contract.json`
> 未对 aio 文件声明 symbol binding，故 C4 机器门不覆盖此漂移。

## 0. 范围界定

覆盖：aio_hips_writer.cpp 全部写出算法路径——product_begin 校验与产品状态
（ALG-HIPS-001）、叶级 tile 信号/支撑写出与归一（ALG-HIPS-002）、方差/逆方差
产品（ALG-HIPS-003）、层级聚合 hierarchy tiles（ALG-HIPS-004）、MOC/properties/
SNR/manifest 收尾（ALG-HIPS-005）。不覆盖：tile 累加产生 AstroSphereTileView 的
drizzle 引擎公式（ALG-DRZ-001 权威）；HiPS 读侧（aio_hips_reader.cpp，P3 链）；
IO-003 Python 发布层（原子 rename/manifest COMPLETE 语义在
docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md，本文件仅登记对齐边界）。

## 1. ALG-HIPS-001 — 产品生命周期与叶级几何基数

**源码锚**: aio_hips_writer.cpp `aio_hips_product_begin` :386-422。

- (1a) 叶级阶 K 与 tile 阶：`leaf_order = ilog2(nside)`（:411），
  `tile_order = leaf_order − 9`（:412，tile 宽恒 512=2^9），叶级
  nside = 2^K ≥ 512（:399-401 拒绝 nside<512）、tile_width==512（:402）。
  与 DATA_SEMANTICS §2（leaf_order=tile_order+9）一致；与 SCI-P3-001 读侧
  默认一致。注意 hiss 侧 `compute_tile_depth`（hiss_common.cpp:65-79，
  d=clamp(log2(nside)−4,0,9)）是独立中间容器语义，与本公式不同（不混用）。
- (1b) 单叶球面元面积 `A_cell = 4π / (12·nside²)`（:413），即 Górski 2005
  NESTED 等面积基数 4π/(12·4^K)；无 SCI 独立条目，按实现+文献登记。
- (1c) MOC 阶：入参 moc_order>0 时 `moc_order = min(moc_order, tile_order)`
  （:419）；=0 时自动取 tile_order（finalize :1022-1027 路径）。读侧
  aio_hips_reader.cpp:166-175 仅保留 order==K 的 UNIQ——moc_order<K 时
  低阶父 cell 对自家 reader 无效（见 DISP-HIPS-005）。
- (1d) 产品集校验：flags 位域 {1,2,4,8,16}，ALL=7/ALL_V19=31
  （aio_hips.h:34-43；begin :404 拒绝越位）；dtype∈{0=float32,1=float64}
  （:403）；违规 → 返回 NULL + last_error。
- (1e) 缺省元数据：creator_did="ivo://astrocs/phase1"、
  obs_title="AstroCS Phase1"（:414-415，可入参覆盖）。
- (1f) 状态句柄 ProductSet 持有：nside/tile_width/data_type/flags/out_dir/
  properties 元数据、moc_cells（叶级 cell 集 :359）、leaf_ipix_list（写序
  :360）、hier[k]（map<ipix,AncestorAcc> :361，:420 resize(tile_order)）、
  SNR 点缓存（:684-691）、prov 键（默认 unset :347-348）、六段 profile
  计时（:368-373）。

## 2. ALG-HIPS-002 — 叶级 tile 信号/支撑归一与 FITS 写出

**源码锚**: aio_hips_writer.cpp `aio_hips_write_signal_support_tile` :424-562
（视图校验 :428-437；缓冲 :442-456；NESTED→FITS 序 :463-465；归一 :476-479；
无效规则 :481-485；FITS 写 :503-521；MOC/面积登记 :528-532）。

- (2a) 叶级局部索引映射：`fits_index(i) = nested_local_to_fits_index(i, 9, 512)`
  （:463-465；三处 scatter 写点 :464/:604/:809 全部同式）。权威实现
  lib/algorithms/shared/healpix/healpix_core.cpp:287-296：NESTED 交错位解（x=偶位、
  y=奇位，nested_local_to_xy→nest_to_xy :272-275,37-54）后
  `fits = (511−x)·512 + y`——即 DATA_SEMANTICS §3 冻结的 (511−x)·512+y
  （CDS Hipsgen MAPTILES 对拍冻结，healpix_core.h:39-43 头注释）。tile 内
  512×512，与父 cell parent_ipix（NESTED，Norder K）拼接出全局 HEALPix 索引。
- (2b) 归一（与 SCI-DRZ-001 :145 support=D_p 语义一致，集合级细节由实现定，
  DATA_SEMANTICS §4）：对 tile 内每局部像素 p（512²）：
  `signal[p] = flux_sum[p] / covered_area[p]`（:1300），
  `support[p] = covered_area[p]/A_cell`，`>1 钳 1.0`（:1301-1305）。
  **量纲（逐项）**：flux_sum [ADU]、covered_area [sr] ⇒ `signal` **[ADU/sr]**
  （面亮度，`FZ-UNIT-SIGNAL-SB` FROZEN；推导见 DATA_SEMANTICS §31.1a 量纲链表）；
  `support` **无量纲**（sr/sr，∈[0,1]）。**禁止**把 `signal` 读作"每像素计数"
  （裸 ADU）：两者相差 1/A_cell 的立体角因子，且跨像元尺度不可比。
- (2b-上游) Phase1 编排（module_adapters.cpp p1_op_writer）必须按 HISS 支持度
  uint8 面连续缩放 `covered_area = (support/255)·A_cell`，并置 `valid_mask`=
  本 parent 实际触及叶像素（未覆盖偏移**不得**保留上一 parent 缓冲）；
  **禁用** `support>0 ? A_cell : 0` 这一写法——它把任意部分覆盖塌缩为满覆盖
  （AIO 侧 `support=area/A_cell` 恒 1，负例判据）；provenance
  `covered_area_model="hiss_support_ratio_x_A_cell"`。
- (2c) 无效规则（:476,:481-485）：当 `valid[p] && area[p]>0 && isfinite(flux[p])
  && isfinite(area[p])` 为假 → signal=NaN、support=0（:483-485）；
  signal_min/signal_max 遍历有限值更新（:481-482，供 properties
  hips_data_range）。IEEE NaN 填充（无 FITS BLANK 整型卡）。
- (2d) FITS tile：`write_fits_image` :171-238（vendored CFITSIO 4.6.4）——
  先 `std::remove` 后 `fits_create_file`（:185-186，覆盖式直写，无临时
  文件——原子发布属 IO-003 层，见 DISP-HIPS-004）；cards
  PIXTYPE=HEALPIX / ORDERING=NESTED / COORDSYS=C :194-196 +
  OBJECT/FILTER/EXPTIME/DATE-OBS :197-204 + NSIDE/FIRSTPIX="0"/
  LASTPIX="262143"（调用方 :505-506/:638-639/:804-805；声明性头卡，
  DISP-HIPS-012）；`fits_write_pix` 行主序一次写 512×512（:220-225，
  bitpix −32/−64 随 data_type）；`fits_write_chksum`（:230，DATASUM/
  CHECKSUM 完整性）。路径 `Norder{K}/Dir{(ipix/10000)*10000}/Npix{ipix}.fits`
  （IVOA REC-HIPS-1.0 §4.1；**禁用** Dir=商/Npix=余数这一与标准相反的映射；
  tile_rel_path 位于本文件 aio_hips_writer.cpp 的匿名命名空间内）。
- (2e) 登记副作用：FITS 写失败 rc=−4（signal，:513）/−5（support，:521）；
  MOC 叶级 cell（moc_cells.insert :528-529）、moc_area_sr += A_cell(K)
  （:530）、covered_area_sr += tile_covered（:532）、leaf_ipix_list 追加
  （写序即 leaf 顺序）。

## 3. ALG-HIPS-003 — 方差/逆方差产品

**源码锚**: aio_hips_writer.cpp `aio_hips_write_variance_tile` :571-692
（参数校验 :573-584；缓冲 :590-600；主循环 :607-628；全无效 :629-632；
FITS 写 :641-658；hierarchy 登记 :663-679）。SCI 锚：SCI-DRZ-001 :130
（finalize_tile 方差传播，方差分子已由 P1-DRZ 链生产，本模块只做除法归一）；
DATA_SEMANTICS §4a（DATA-HIPS-VAR-001/DATA-HIPS-IVAR-001）。

- (3a) 归一：条件 `valid && area>0 && vnum>0 && isfinite(area) &&
  isfinite(vnum)`（:617-621）下 `variance[p] = var_num_sum[p] / (covered_area[p])²`、
  `ivar[p] = 1 / variance[p]`（:618-621）；否则 variance=ivar=NaN（:615-616）。
  var_n[i] 缓存（供 hierarchy）取 `vnum` 当 `valid && area>0 && vnum>0`
  （:622-628，不含 isfinite——与写盘条件略有差，如实登记）。var_num_sum
  语义=Σ v_j·w_jp²（drizzle 侧逐像素分子，astro_sphere_sink.cpp:100 传入），
  本模块不改传播公式（与 DISP-DRZ-007 的行漂移说明相接）。
- (3b) 全无效 tile：该 tile 无任何有效样本 → 返回 −5（:629-632）**不写文件**
  （显式失败，不静默产空 tile；调用方须自行预判跳过——sink 侧对 −5 跳过
  该 tile 计数 n_variance_skipped，astro_sphere_sink.cpp:123-144，不因此
  abort）。参数校验 rc=−2（var_num_sum 缺失 :575-576）/−3（视图不匹配
  :579）/−4（parent_ipix 越界 :584）。
- (3c) FITS 写同 (2d)：`variance` 目录 tile 写失败 rc=−6（:648）、`ivar`
  rc=−7（:658）。
- (3d) hierarchy 登记：`acc.add_var(z, var_n[i])`（:664-679，add_var
  :327-333 直接累加方差分子，权重恒 0 不另乘）。

## 4. ALG-HIPS-004 — hierarchy 低阶 tile 聚合

**源码锚**: aio_hips_writer.cpp 叶级同步累加 :536-557 与 variance 同构段
:663-679（AncestorAcc :297-333）；finalize 落盘 `finalize_hierarchy`
:791-885（逐层循环 :797-799，:830-880 写各阶 FITS）。IVOA HiPS hierarchy
（上采样满足任意浏览器）的写出侧实现合同；SCI 层零覆盖（读侧 SCI-P3-001
只规定消费），按实现+IVOA 语义登记。

- (4a) 父索引下降：对叶 tile（阶 K，tile 阶 T=K−9）内局部 NESTED 索引 l
  （512×512 展平 2^18），对 k=T−1..0 降序：`z = ((s<<18)|l) >> 2(T−k)`
  （:552-556，s=tile 内 NESTED 段），得阶 k 的父 cell 索引——标准 HEALPix
  NESTED 4 分叉父子关系（Górski 2005）；叶 tile→父 cell 分离
  `leaf_to_tile_nest = leaf_ipix >> 2(L−K)`（healpix_core.h:65-68）。
- (4b) 逐父 cell 确定性累加（叶级缓存 :689-694、归约 :748-758）：
  `flux_n += sig·a`、`area_n += a`，其中 **`a` = 未钳制真实覆盖面积**
  （叶级 `area_true`→`scratch_area_n`，:645/:670-694），`sig` = 发布面叶级
  signal（f32 产品取 float 截断后的值，:689）。**归约不得用钳后的 support 当
  权重**：support 是 a/A_cell 的钳后发布值，用它反乘等于把父级面亮度降为
  sup 加权均值；异质覆盖（a>A_cell）下父级通量出现**本可避免的损失**
  （M2a-H-3；R-7 实测 sb=10@c=4 与 sb=0.1@c=1 混合域：面积加权 8.02 vs
  旧式 5.05，损失 37.032%→0）。方差分子直接累加
  `var_n += var_num`（:734-747，add_var :465-472，var_num 本身已是
  Σv_j w_jp² 分子，不另乘权）。AncestorAcc 保持 f32/f64 双轨（is_f32
  :444-457；**f32 产品用 float 累加 Σflux/Σarea——多子 tile 有舍入漂移
  风险，DISP-HIPS-009**）。同一父 cell 按叶写序单线程顺序累加（map 条目
  首次遇到时创建）→ fixed_reduction_order（无并行求和漂移）。
- (4c) 落盘：finalize 时对每阶 k<T：nside_k=2^(k+9)、
  A_cell_k=4π/(12·nside_k²)；cell 归一 signal=Σflux/Σarea、
  support=min(Σarea/A_cell_k,1)（层级面钳制点，`finalize_hierarchy` 内
  `sup = area/A_cell_k; if (sup>1.0) sup=1.0`；叶级另有各自发布面的钳制点
  `:1301-1305` ⇒ **每个发布面各钳制一次**，两处计数键不同：叶级
  `astrocs_support_clamped_pixels`、层级 `astrocs_coverage_gt1_pixels`；
  Σflux/Σarea 用的是 (4b) 的未钳制面积）、variance/ivar 同 (3a) 用
  Σarea——与叶级公式同构；对齐 IVOA"低阶像素=子像素聚合"。**编码限
  （M2a-H-3）**：support 是 a/A_cell_k 的钳后值，Σa>A_cell_k 时父级真实覆盖
  面积**不可由产物复原**（sup=1 丢失倍数信息）；该情形逐像素计数写入
  properties 与 manifest（`astrocs_support_clamped_pixels` 叶级钳制像素数
  :677/:1119、`astrocs_coverage_gt1_pixels` 层级 Σa>A_cell_k 像素数
  :1543-1556/:1120；DATA_SEMANTICS §20.3 同款声明）
  约定。FITS cards ORDERING=NESTED + NSIDE=2^(k+9)（k = 该 hierarchy 阶，
  nside_k = 2^(k+9) 与叶级 ilog2 语义一致；**不是 tile 阶**——tile 宽恒 512=2^9，
  故 k 阶的 NSIDE 比叶级 nside 低 2(K−k) 个数量级）。三处 scatter 同 (2a) 式
  （:809）。
- (4d) 边界：hier 仅对**被写过的父 cell** 建立条目（map 语义），但已建
  条目的空 acc（count 全 0）仍会写全 NaN tile（:797-823 不检查 count），
  且 hierarchy 目录无独立 MOC（如实登记，DISP-HIPS-011）。tile_depth≠9
  的视图在 sink 层拒绝（astro_sphere_sink.cpp:34-40），本模块对
  width≠512 视图 rc=−2（:428-431）。hier 每层 finalize 重新分配
  sigF/supF/sigD/supD 四个 262144 缓冲（:794-795，未入句柄 scratch，
  内存峰值点）。

## 5. ALG-HIPS-005 — MOC、properties、SNR 产品与 manifest 收尾

**源码锚**: aio_hips_writer.cpp `write_moc_fits` :243-283、
`write_properties` :285-293、`finalize_image_product` :697-789、
`finalize_hierarchy` :791-885、`finalize_snr_product` :887-1005、
`aio_hips_set_drizzle_provenance` :1007-1016、`aio_hips_finalize`
:1018-1131、`aio_hips_abort` :1133-1137。

- (5a) MOC：叶级 moc_cells（2^K cell）→ **UNIQ 编码
  `uniq = 4·4^moc_order + (c >> 2·(tile_order−moc_order))`**（:781-785，
  SNR 同构 :1001-1003），排序去重（:784-785）后 `write_moc_fits`（BINTABLE
  列 "UNIQ" TFORM=K，:255-258；MOCORDER/PIXCOUNT 头键 :263-264；checksum
  :275；空集不写 :246；remove+create :251）。moc_sky_fraction =
  moc_area_sr/4π（finalize :1028-1029）。
- (5b) properties（write_properties :285-293 裸 `key=value\n` 直写，fopen
  失败静默 return；键序 finalize_image_product :707-771）：IVOa 关键字
  creator_did（:707，缺省 ivo://astrocs/phase1）/ obs_title / obs_creator=
  "AstroCS"（:710）/ **hips_version="1.4"** / hips_order=K /
  hips_tile_width="512" / hips_frame="equatorial"（IVOA REC-HIPS-1.0 §4.4.1 标准值域 {equatorial, galactic, ecliptic}；
  M1a-B-005 前写非标准值 "equatorial"）/ dataproduct_type="image" /
  dataproduct_subtype（signal=surface brightness、support=coverage fraction、
  variance=variance、ivar=inverse variance，:716 由 finalize :1030-1052
  传入）/ hips_tile_format="fits" / **hips_status="private master"（恒值
  :718，DISP-HIPS-003）** / hips_creator / hips_builder="AstroCS
  aio_hips_writer (CFITSIO 4.6.4)"（:720） / **hips_estsize="1000000" 硬编码
  （:721，DISP-HIPS-002）** / hips_release_date+hips_creation_date=真实 UTC
  （utc_now_date/utc_now_iso :93-/:80-，gmtime_r 固定格式不依赖时区；
  META-001 禁伪造）/ obs_description / prov_progenitor=ivo://astrocs/
  phase1/drizzle（:1098）/ [prov_set 时] ASTROCS_DRIZZLE_PIXFRAC（%.6f）/
  ASTROCS_DRIZZLE_SCALE_ARCSEC（%.4f）（:1105-1108；set_drizzle_
  provenance :1408-1440，**M9-F-3 合法域**：pixfrac∈(0,1] rc=2、
  scale 必须有限且 ∈(0, ACS_HIPS_MAX_FRAME_SCALE_ARCSEC=824.5167388361774″]
  rc=2，**每次拒绝都 set_error 点名原因**；通道全或无——接受即两键齐备落盘，
  不再有 "scale>0 才写" 的静默省略分支。上界推导：叶级 nside≥512 ⇒ 最粗叶像素
  412.258369″，经 SCI-DRZ-001 冻结的 1–2× 过采样 ⇒ 帧尺度 ≤2×412.258369″；
  该界是物理/表示域，**不是** k_corr 查表域 [300,600]″）/ obs_regime
  / hips_hierarchy / **hips_pixel_scale = (180/π)·√(π/3)/nside deg（IVOA REC-HIPS-1.0
  §4.4.1 单位=度；**禁用**多乘 3600 写角秒。帧尺度上界常量
  ACS_HIPS_MAX_FRAME_SCALE_ARCSEC 仍是角秒内部量，不改）
  （:704-705，%.6f）** / **hips_initial_fov="60" 硬编码（:745，DISP-HIPS-002）** /
  **moc_sky_fraction（与 manifest.json 共用唯一格式化函数
  fmt_sky_fraction = %.17g，:1051/:1115/:1673；DBL_DECIMAL_DIG ⇒
  strtod 回程精确，§9 的 <1e-9 绝对容差与键值精确两条无条件成立；**禁用**
  properties 与 manifest 各用一套格式化（std::to_string 6dp / %.8f）——
  双面字面量必然分叉，违反逐字符相等）** /
  astrocs_covered_sky_fraction / astrocs_signal_dtype /
  **astrocs_support_clamped_pixels + astrocs_coverage_gt1_pixels（:1119-1120
  properties / :1668-1669 manifest；M2a-H-3 钳制计数，见 (4c) 编码限）** /
  [非空] hips_data_range（sig_min≤sig_max 才写，:1026-1029）/ **hips_ordering=
  "NESTED" 恒写（:908，B2-A8；消费侧 coverage 断言 NESTED）** / **obs_filter
  恒写（含空值，:960-964；空串=显式"无 filter"声明，**禁用**"键缺失"写法）** /
  [exposure>0] obs_exptime / [obs_date] obs_date +
  t_min+t_max（iso_to_mjd :65-79 固定纪元换算，t_max=t_min+exposure/86400，
  :762-771）。另写极简 metadata.fits（PIXTYPE/ORDERING=NESTED/NSIDE/
  HIPSTILEWIDTH=512/DATAPRODTYPE，:776-786，remove+create :770）。
- (5c) SNR Catalogue HiPS：finalize_snr_product :887-1005——无点整体不写
  （:888）；点 (ra_deg,dec_deg) 经共享 HEALPix 权威
  `astrocs::healpix::ang2pix_nest(2^tile_order, ra, dec)` 落 cell
  （:896-899；radec_to_xyz→xyz_to_hp→xy_to_nest，healpix_core.cpp:232-243，
  源 astrometry.net healpix.c，astropy-healpix 百万点 oracle 已对拍）；
  按 cell 聚类写 TSV tile（NorderK/DirD/NpixN.tsv，:904-924）：头注释行 +
  列 star_id(ra 格 %lld)/ra/dec（%.12f deg）/snr/quality_flags/
  photometric_status，**SNR-PREC-001 精度合同：FP32 %.9g、FP64 %.17g
  （round-trip 无损，:914-918）**；SNR properties（:922-975）：
  dataproduct_type=catalog / dataproduct_subtype=snr / hips_tile_format=
  tsv / moc_sky_fraction=cells·A_cell(K)/4π / **hips_cat_nrows=点数** /
  **hips_initial_ra/hips_initial_dec=真实源位置中位数（:960-973，注释
  明示非伪造）**；metadata.xml=VOTable 1.3（Hipsgen LINT[4.4.3] 要求根
  元素 votable；FIELD 含 datatype/unit/ucd，:977-1000）；SNR Moc.fits
  （:1001-1005）。
- (5d) manifest.json（:1086-1128）：format_version=1、hips_version="1.4"、
  nside/tile_width/data_type、products 列表、n_leaf_tiles=leaf_ipix_list
  .size()、moc_sky_fraction、astrocs_covered_sky_fraction、signal_dtype。
  **无 COMPLETE 状态字、无 sha256 树哈希**——发布侧语义在 IO-003
  （runtime/io/hips_output_store.py 临时写→fsync→fitsverify→sha256→原子
  rename→manifest COMPLETE），C++ writer 与发布层的对齐边界=DATA-P1-HIPS
  §12.5，两合同均如实登记（writer 无事务、发布层有；tree hash 仅存在于
  发布层）。
- (5e) 终止语义：finalize（:1018-1131）——重复 finalize rc=−2（finalized
  位 :1022-1023）；逐产品失败 rc=−3（signal 子产品，:1036）/−4（support，
  :1042）/−7（variance，:1049）/−8（ivar，:1055）/−5（hierarchy，:1065）/
  −6（snr，:1072）；最后写 manifest（:1086-1128）、delete ps（:1129）。
  abort（:1133-1137 **仅 delete ps，不删除已写文件**——aio_hips.h:151
  注释"清理已写部分(尽力)"与实现不符，DISP-HIPS-001；部分失败残留处置归
  调用方/IO-003 发布层）。每子产品独立 properties/Moc/低阶 hierarchy
  （文件头 :13 注释，variance/ivar 各自独立 hierarchy tiles）。
- (5f) last_error：thread_local g_hips_error（:48，每次入口 clear
  :398/:426/:573；aio_hips_last_error :1139-1141），跨 ABI 无异常抛出；
  错误码无集中枚举（DISP-HIPS-007）。

## 6. 与 SCI 层的对应与缺口（不反向修改）

- SCI-DRZ-001 :130（finalize_tile 方差实现锚）、:145（support=D_p）为本模块
  唯二 SCI 锚；(2b)/(3a) 与之严格一致（support=min(D_p/A_cell,1) 的集合级
  截断与无效规则为实现语义，DATA_SEMANTICS §4 冻结）。
- **SCI 缺口（如实登记）**：HiPS 写出合同（tile 切分/hierarchy 聚合/properties
  键集/publish 协议）在 docs/science/ 无 SCI 级条目——由本文件 ALG-HIPS 承接；
  SCIENCE_SCOPE.md:9 仅产品级目标，SCI-P3-001 为读侧消费合同。SCI 化候选
  变更走 SCI 变更流程（ENGINEERING_SPEC §3：独立证据证明文档有误时订正文档是义务，
  SCI 层订正须记录证据与影响面并做一致性回归）。
- DRIZZLE.md:132 指向的 DISP-DRZ-007（方差行漂移）涉 astro_sphere_sink.cpp:100
  与本文件 (3a) 接口，本模块不改传播公式。
- 上游 drizzle 计算为 OpenMP 行级并行（drizzle_engine.cpp:1673，
  schedule(static)，线程数不污染 ICV :1644），线程私有 tile 累加 + 线程 0
  串行合并（:1762-1792）后单线程调 writer（astro_sphere_sink.cpp:97）——
  writer 自身单线程串行消费，与 §7 确定性一致。

## 7. 确定性

fixed_reduction_order：(4b) hierarchy 单线程按叶写序逐父 cell 累加（同一父
cell 依写入顺序累计、finalize 按 map 序写出）、(2e) leaf_ipix_list 记录
写序、FITS checksum 内嵌可复现校验。properties 的 UTC 时间戳（(5b)）按
META-001 使用真实时间 → properties/manifest 字节不跨运行复现（合同本身
如此，非缺陷）；科学 tile 字节（signal/support/variance/ivar/MOC/TSV）同
输入同序逐字节可复现。数据类型双轨（f32/f64）由 data_type 一次性决定
（begin 固化），不混写；hips 精度分层见 §8。

## 8. 精度与 dtype

存储 dtype=入参 data_type（f32/f64 统一 tile 内外一致）；计算路径：归一在
double 域完成（:469-477 输入 double 化后运算），存储时截断到存储 dtype；
hierarchy 累加器双轨——**f64 产品全程 double；f32 产品 AncestorAcc 用
float 累加 Σflux/Σarea（:322-331），多子 tile 大和有 f32 舍入漂移风险
（DISP-HIPS-009；方差分子同理）**。signal=flux/area 与
variance=var_num/area² 的病态区域（area→0）由 (2c)/(3a) 无效规则兜底为
NaN/0。hips_data_range 只统计有限值（:481-482）。TSV 精度按 SNR-PREC-001
round-trip（(5c)）。容差冻结见 §9。

## 9. TEST-HIPS-DESIGN-001 — 冻结测试设计（可执行测试归 P1-HIPS-TEST）

- **fixture**：F1 合成小天区（K=9, nside=512, 单 tile 单元 12 cell 数量级；
  flux/area/var_num 解析可控）；F2 多 tile 覆盖同父（hierarchy 聚合闭合）；
  F3 含 NaN/0-area/负 flux 边界像素；F4 SNR 点集（已知 ra/dec→cell）；
  F5 prov 键完整/缺省两态；F6 f32/f64 双 dtype；**G 异质覆盖 c>1
  （p1hips_fixtures.hpp `fix_hips_g_hetero_coverage_tile`：逐像素交替
  sb=10@c=4 与 sb=0.1@c=1 ⇒ 面积加权真值 8.02 vs 旧 support 加权 5.05，
  是 (4b) 归约式的**判别面**——c≤1 全域两式仅差浮点重结合，无判别力）**。
  生成器入 tests（testkit
  规则：fixture generator，不内嵌生产算法；现有 eng/tests/io/make_hips_fixture.py
  属 e2e fixture——注意其 :168/:170 照抄 estsize/fov 占位值，P1-HIPS-TEST
  须以解析值替代）。
- **oracle**：独立朴素实现（不调用生产 symbol，不复制源码公式——用
  HEALPix 独立库或解析解）逐像素重算 signal/support/variance/ivar 与
  FITS 局部索引（DATA_SEMANTICS §3 (511−x)·512+y；外部对拍可取 CDS
  Hipsgen 样例）；FITS 头键精确匹配；MOC UNIQ 精确（式见 (5a)）；
  hierarchy 父像素=子像素精确聚合（NESTED 4 分叉）；**层次闭合 oracle 的权重
  必须取 fixture 输入的未钳制覆盖面积，不得用产物 support（钳后值）反乘
  ——否则 oracle 与实现同源、对 (4b) 型错误恒绿（负例判据：同源 oracle
  对等价缺陷注入必须判红）**；moc_sky_fraction 的序列化
  判别点必须取十进制非有限小数（oracle 组 O6 用 N=1,K=0 ⇒ 1/12：
  6dp 偏 3.333e-7 = §9 容差的 333 倍，且 properties↔manifest 字面量必须
  逐字符相等）。独立复检生态参考：
  HIPS_VERIFY（orchestrator :3794 起，经 aio_hips_reader）与
  gate7_hips_validate.py（lib/algorithms/photometry/.../gate7_hips_validate.py，
  astropy 独立复检 tile 头/DATASUM/support∈[0,1]/F=signal×support×A_cell/
  MOC↔叶级一一对应）——其不变量与 I1-I8 相容，可作 oracle 参照实现。
- **不变量**：I1 逐像素 signal=flux/area；I2 support=min(area/A_cell,1)≤1；
  I3 无效规则→NaN/0；I4 variance/ivar 互为倒数（有限域）；I5 MOC cells=
  非空叶 cells（order K）；I6 moc_sky_fraction·4π=moc_area_sr；
  **I7 hierarchy 逐阶聚合闭合 = 面积加权（权重为未钳制真实覆盖面积 a；
  发布面 support=min(Σa/A_cell_k,1) 是编码限，不得回流当归约权重），
  f64 域 bitwise（c>1 亦然）；f32 路径按 §9 容差（accumulator 漂移）**；I8
  manifest 计数字段=实际文件数；I9 double finalize rc=−2；I10 abort 后
  句柄不可复用；I11 F=signal×support×A_cell 有限非负（gate7 同式）；
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
  十进制非有限小数，见 §9 oracle）**；MOC/properties 键值精确相等
  （moc_sky_fraction 在 properties 与 manifest 两面**逐字符**相等）；checksum
  字段由 CFITSIO 重算一致。
- **精度分层**：f32/f64 两 data_type 全矩阵重跑；f32 输入 + f64 累加器
  路径断言无交叉污染。
- 性能/资源：单 tile 写耗与内存水位 smoke 记录（非冻结容差，登记即可）。

## 10. DISP-HIPS-001..012 — 实现偏差清单

| ID | 严重度 | 描述 | 锚 | 处置建议 |
|---|---|---|---|---|
| DISP-HIPS-001 | 高 | abort 仅 `delete ps`，不删除已写文件；aio_hips.h:151 注释称"清理已写部分(尽力)"——合同与实现不符，部分失败产品残留无 rollback。matrix 专项"partial failure rollback"如实登记为缺口 | aio_hips_writer.cpp:1140-1144; aio_hips.h:151 | IMPL 事务化（写临时目录+发布切换）或头注释降级声明+文档化调用方清理责任（与 IO-003 对齐） |
| DISP-HIPS-002 | 中 | properties `hips_estsize="1000000"`、`hips_initial_fov="60"`（image 与 SNR 两处）硬编码占位，无真实估算/校验 | :721; :745; :954 | 按产品目录真实字节数与天区极值估算；eng/tests/io/make_hips_fixture.py:168/:170 照抄需同步 |
| DISP-HIPS-003 | 低 | properties `hips_status` 恒 "private master"，无公开/克隆状态参数化 | :718; :931 | 参数化或确认产品定位恒私有 |
| DISP-HIPS-004 | 高 | C++ 写出无原子发布：FITS/MOC/metadata 先 remove 后 create 直写（:185-186/:251/:770）、make_dirs 无 fsync（:110-133）、properties/manifest 直写、manifest 无 COMPLETE 状态字/树哈希；finalize 中途失败（−3..−8）已写子产品残留；同 out_dir 重跑与旧运行残留混合。原子语义由 IO-003 Python 发布层承接（临时写→fsync→fitsverify→sha256→原子 rename→manifest COMPLETE）——两合同边界在 DATA-P1-HIPS §12.5 登记对齐，writer 层不冒认已原子。对照：HISS 容器有 .partial/.tmppool+atomic_replace（hiss_stream_writer.cpp:259-260,:644-655）但 writer 未采用 | :185-186,:251,:770,:110-133,:1086-1128; docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md | INT 层接线（writer 写 staging 由 IO-003 消费）或 writer 内嵌事务；tree hash 归属待定 |
| DISP-HIPS-005 | 低/中 | 入参 moc_order 静默 clamp（min 与 tile_order）无告警；且 moc_order<K 时 Moc.fits 含低阶 UNIQ，而自家读侧 aio_hips_reader.cpp:166-175 仅保留 order==K——低阶 MOC 对自家 reader 无效（Moc.fits 为 optional hint，不影响覆盖判定） | :419; aio_hips_reader.cpp:166-175 | 强制 moc_order=K 或 reader 兼容低阶 UNIQ |
| DISP-HIPS-006 | 中 | CFITSIO 裸调未包装进程级互斥锁（同库 aio_fits.cpp:502、aio_hips_reader.cpp:91/:140/:293 均用 aio::cfitsio_io_mutex）——writer 写路径完全无锁；单句柄串行使用无影响，未来多句柄/多线程写同进程将静默竞争（现生产链=drizzle 合并后单线程写，astro_sphere_sink.cpp:97，暂无并发场景） | aio_hips_writer.cpp 全文件无 mutex | 统一包装 mutex 或显式登记单句柄使用约束 |
| DISP-HIPS-007 | 低 | 错误码无集中枚举且正负混用（write/finalize 负码 −1..−9（含 ABI 不匹配 −9，V11-N-01）vs provenance 正码 1/2；语义仅注释）——ABI 演进风险；last_error 每入口 clear，跨调用不可追溯 | :398/:426/:573, :513/:521/:632/:648/:658/:1036-1072, :1007-1016 | 集中枚举 + 头文件公开 |
| DISP-HIPS-008 | 低 | `aio_hips_write` 兼容入口旧语义：signal=F、support=uint8 0..255 → covered_area=su/255·A_cell、flux_sum=signal·su/255（8bit 量化损失），flags 固定 ALL（无 variance/ivar），与新 AstroSphereTileView 连续 support 语义并存易混用 | :1146; :1182-1199 | 标记为非推荐入口或内部转换告警 |
| DISP-HIPS-009 | 中 | f32 产品下 AncestorAcc 以 float 累加 Σflux/Σarea/Σvar_num（f64 通道仅 f64 产品使用）——多子 tile 大和有 f32 舍入漂移风险，影响低阶 hierarchy 精度 | :322-331; :536-557 | f32 产品仍用 double 累加（存储时再截断）或登记精度边界（TEST-HIPS-DESIGN-001 容差已按 1e-6 冻结） |
| DISP-HIPS-010 | 低 | fits_str 对 FITS 头字符串 68 字符静默截断（obs_title 等超长丢尾无告警）；properties key=value 裸写无转义（值含换行/=会破坏格式，现状值域受控）；write_properties/SNR tile fopen 失败静默或仅 set_error（properties 缺失时 finalize 仍返回成功路径） | :139-157; :285-293; :906 | 截断告警；properties 值转义/校验；失败传播到返回码 |
| DISP-HIPS-011 | 低 | hierarchy 空 acc（count 全 0 的父 cell）照写全 NaN tile（:797-823 不检查 count），文件量虚增；hierarchy 目录无独立 MOC；finalize 每层重新分配 4×262144 缓冲（:794-795）内存峰值 | :794-795; :797-823 | count==0 跳过；缓冲入句柄 scratch 复用 |
| DISP-HIPS-012 | 低 | FIRSTPIX/LASTPIX 声明性头卡（恒 "0"/"262143"）全仓无消费方（仅 fixture 断言字面值）——冗余契约，语义漂移风险低 | :505-506; :638-639; :804-805 | 保留并文档化或删除 |

登记级事项（非缺陷，如实登记）：逐像素标量 scatter 调用
nested_local_to_fits_index（:464，prof_transform 热点段之一，性能优化归
IMPL）；全无效 variance tile 以 rc=−5 报错、调用方须预判（设计张力）；
UTC 时间戳致 properties/manifest 字节不跨运行复现（合同，§7）；无取消/
检查点（长 finalize 不可中断，README §7）。

## 11. 关联

- 上游：SCI-DRZ-001（DRIZZLE.md，共享引用）、SCI-SCOPE-001；ALG-DRZ-001
  （tile 累加上游，DRIZZLE_GEOMETRY.md）；ALG-HEALPIX-001（NESTED 核心，
  HEALPIX_MAPPING.md 索引卡，lib/algorithms/shared/healpix 权威实现——healpix_drizzle
  内 healpix_core.h 为 DEPRECATED shim，healpix_stack 系列函数全仓零调用
  已死代码化）。
- 下游/合同：DATA-P1-HIPS（DATA_SEMANTICS §12）、API-HIPS-001
  （PUBLIC_API.md）、API-P1-007（编排级 hp_drizzle_run_hips 区间）、
  ARCH-001、IO-003（发布合同，对齐不越权）、SCI-P3-001（读侧消费）。
- 模块：MOD-astrocs-phase1-hips-writer（lib/algorithms/drizzle/hips/README.md、module.yaml、
  registry astrocs.phase1.hips-writer.md）；traceability 行
  MOD-astrocs-phase1-hips-writer。
- 相邻（不改）：aio_hips_reader.cpp（P3 读链，HIPS_VERIFY 后端）、
  astro_sphere_sink.cpp（P1-DRZ 写通道 sink）、hiss_codec/hiss_stream_writer
  （独立中间容器，legacy_hiss_compare 开关封闭，CFG-002 关闭 HISS_VERIFY
  orchestrator:3404-3407）、orchestrator.cpp（stage 编排）、
  lib/algorithms/coverage/tools/stage2.cpp:592（Phase2 写方）。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- HiPS 规范：IVOA HiPS 1.0 Recommendation（https://www.ivoa.net/documents/HiPS/；§3/§4.1/§4.2.1/§4.4.1/§6.3.1）。
- HiPS 层级/目录：Fernique et al. 2015, A&A 578, A114（DOI 10.1051/0004-6361/201526075）。
- HEALPix 几何：Górski et al. 2005, ApJ 622, 759。
- FITS 写出/关键字：FITS Standard 3.0；CFITSIO（宽松许可）。
- MOC/properties：IVOA MOC 1.0（https://www.ivoa.net/documents/MOC/）。
- dtype/精度与 hierarchy 聚合：Project-defined（本文件 §2/§4/§8）。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

