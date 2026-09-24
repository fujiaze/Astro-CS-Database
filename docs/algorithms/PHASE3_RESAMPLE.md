# Phase3 HiPS→FITS Resample Algorithms (ALG-P3)

> 上游：ASTROCS_DESIGN.md §6.2（export 流程）、§6.3（投影算法）

> ID: ALG-P3-001  范围: ALG-P3-001..004  上游 SCI: SCI-P3-001  状态: DERIVED  模块: phase3 (施工规格; 重采样域实现级合同=ALG-P3-RSMP-IMPL-001 docs/algorithms/PHASE3_RSMP_IMPL.md, 投影域=ALG-P3-PROJ-IMPL-001, 写出域=ALG-P3-FITS-IMPL-001; 生产源 lib/algorithms/resample/p3_resample.cpp 586 行实测在库)

## 1 上游 SCI 与输入输出

- 上游: `SCI-P3-001`（PHASE3_HIPS_TO_FITS.md §5 连续定义 + §9a 十二项冻结）
- 输入: HiPS 目录(properties+tiles, float FITS) + 用户参数 center(RA,Dec)/s_out/W_out/H_out/sampler(nearest|bilinear)/parity(east_left|east_right)/bitpix(-32|-64)
- 输出: 单张 FITS image(S+C=coverage, 经掩膜合成或独立 COV 扩展由 API-004 冻结) + provenance(HISTORY); 落盘原子(tmp+rename)
- 前置依赖: `lib/algorithms/shared/healpix`（ALG-HEALPIX-*，round-trip ≤1e-12 deg, NESTED 父子一致）

## 2 离散公式

```text
G1 (ALG-P3-002) 输出 WCS 构造 (FITS 1-based, CD-only):
  CRPIX1=(W_out+1)/2, CRPIX2=(H_out+1)/2, CRVAL=center
  |CD1_1|=|CD2_2|=s_out, CD1_2=CD2_1=0
  east_left:  CD1_1=−s_out, CD2_2=+s_out
  east_right: CD1_1=+s_out, CD2_2=−s_out

G2 (ALG-P3-002) 反向映射 (逐输出像素 (x,y), 1-based→中间平面):
  (iwc1,iwc2) = CD · ((x+1)−CRPIX1, (y+1)−CRPIX2)   # deg 偏移；Paper I §2.1.1: 中间坐标 = CD·(p−CRPIX)
  world = proj^{-1}(iwc; CRVAL) → (RA,Dec)∈[0,360)×[−90,90]
  # 中间坐标 = CD·δp（Paper I §2.1.1）；实现锚 p3_proj_v6.cpp pix_to_plane / p3_wcs.cpp
  TAN 参考点含 CRVAL2（θ0=CRVAL2，Paper II §2.2；CRPIX 处 world == CRVAL）
  gnomonic: (ξ,η)=atan2 形式; 球面角差按 RA wrap 归一

G3 (ALG-P3-003) order 选择 (SCI-P3 §5 冻结):
  s_out_rad = s_out · π/180                         # s_out 单位 deg/px（量纲声明，M7-A-209）
  s_tile_rad(order) = sqrt(π/3) / (2^order · W)     # 叶级像素**等面积等效线尺度**（精确，非近似）
  order_needed = ceil( log2( sqrt(π/3) / (W · s_out_rad) ) )
  order_sel = clamp(order_needed, 0, hips_order)    # = min(hips_order, max(0, order_needed))

G4 (ALG-P3-003) leaf 采样:
  leaf_order = order_sel + log2(W)                  # W=hips_tile_width（支持子集 W=512 ⇒ +9）
  ipix = ang2pix_NESTED(nside=2^leaf_order, RA, Dec)
  tile = ipix >> (2·log2(W));  local = ipix & ((1<<2·log2(W))−1); (lx,ly)=nested_local_to_xy(local)
  # >>(2·log2 W) 是 **索引位移**（tile 内 leaf 数 W²），不是 order 偏移；
  # order 偏移是 log2(W)（W=512 ⇒ +9）。
  nearest: S = tile[lx,ly]（**存在判定→coverage**：tile 像素存在即 C=1，值 NaN 照传）
  bilinear: 邻域 4 leaf 权重 w=面积重叠分数(投影线性化), **Σw = 1 ± k·ULP**(k 由累加 dtype 定),
            S=Σ w·tile_value, 跨 tile 读相邻 tile（NESTED 面邻接含轴翻转/镜像）
            **邻域非有限规则（NaN 处置口径 rule_id NAN-SAMPLE-MASK-COVERAGE-NAN）**:
            不合格邻域样本（¬isfinite，含 ±Inf）按**样本级掩膜**从分子、分母、方差三项
            一并剔除，并对剩余有效邻域**重归一**；仅当零合格样本时 S=NaN
            （**覆盖级 NaN**），且每个输出像素**必须暴露**被剔除样本计数
            （**强制计数**，禁止静默剔除）；C 只判足迹内有无 tile 像素，值 NaN 不改 C
            （4 个 tile 均可读则 C=1）。
            实现锚: lib/algorithms/resample/p3_resample.cpp 的 p3_sample_bilinear_nanmask_ex
            （唯一实现；p3_sample_bilinear_ex 为其薄封装，同一数学路径）。
            已对齐项: ①样本级掩膜 + 剩余有效邻域重归一（FP64，固定 k 序）；
            ②weights[4] 暴露**生效（重归一）权重** c_k = w_k / Σ(合格 w_j)，被剔除样本
            恰为 0 —— 方差项必须消费该权重（DATA-002 §2a 规则 1「从分子、分母、方差
            三项一并剔除并重新归一」）；③强制计数经 P3SampleRejection 暴露，字段名取
            权威冻结名 n_rejected_nonfinite（按原因分类，互斥可加）；
            ④C 不变（只判足迹内有无 tile 像素）。
            未冻结项: 该计数的**产品承载面**（逐像素平面 / JSON / provenance）未见权威
            规定 → 不自行发明，登记 ALG-P3-RSMP-IMPL-001 §11 DISP-P3RSMP-006。

G5 (ALG-P3-004) FITS 写:
  BITPIX=−32/−64, BSCALE=1, BZERO=0
  BUNIT = 源 properties 的 BUNIT（**必须经面亮度量纲校验**）；
          无 BUNIT 键 ⇒ canonical 'ADU/sr'（**禁**缺省 'ADU'，禁 Jy/beam）
  # 依据 DATA_SEMANTICS §31.1/§31.1a（FZ-UNIT-SIGNAL-SB FROZEN）：重采样值是输入
  # tile 值的凸组合 ⇒ 与输入同量纲（面亮度）；'ADU/sr' 是计数按立体角归一的合法串，
  # 裸 'ADU' 是每像素计数口径（与数值不符且量纲不可判）；§31.1a 同时否定 'ADU/px^2'。
  # 实现锚：p3_resample.cpp p3_sampler_open_ex（缺省串）→ p3_session.cpp:396 透传
  # → p3_output.cpp:284/351/675（写盘缺省同串）。
  WCS: G1 全量 + CTYPE=RA---<proj>/DEC--<proj> + CUNIT=deg
  (B2-A4: <proj> 取自已校验 projection, alpha 唯一合法值 "TAN";
   未实现投影在写前 fail-closed, 不落任何 FITS)
  HISTORY: 源 HiPS 标识/order_sel/sampler/软件版本/manifest hash
  coverage: C=1 ⇔ 足迹内存在 tile 像素（值可为 NaN；NaN 只进 S 不改 C）; 无覆盖 S=NaN/C=0
```

推导来源: **SCI-P3-001 §5 连续定义与 §9a 冻结回答的离散化**（G1↔§9a-4, G2↔§5 反向映射, G3↔§9a-5, G4↔§9a-6/7, G5↔§9a-11）；实现一致性锚（非推导依据）:
`lib/algorithms/resample/p3_resample.cpp`（G3=`p3_order_select`、G4=nearest/bilinear 采样核）
与 `lib/algorithms/projection/p3_wcs.cpp`（G1/G2）。

**G3 的适用域与量纲（证据锚）**：`s_tile_rad` 是**等面积等效**线尺度——HEALPix 同 nside
下所有单元面积严格等于 `4π/(12·nside²)`（Górski et al. 2005, ApJ 622, 759 §4），故
`sqrt(π/3)/(2^order·W)` 与该面积的平方根**逐位恒等**（非近似）；本仓实验
`run/SCI-FIX-DRZGEOM-01/evidence/exp_a_geometry.json` A4 段：nside=512/1024 × 9 档纬度面积
相对偏差恒 0.0（阴性对照：等经纬网格在 dec=89.9° 偏 −20.5%）。**但它不是各向同性分辨率
上界**：单元局部采样步长（邻元中心角距）随纬度/方向变化，nside=512 实测共边邻元 ∈
[0.63,0.71]×该尺度、对角邻元 ∈ [1.95,2.94]×该尺度。要求方向性分辨率保证的消费方须按局部
步长另加余量；`order_sel` 只保证**面平均**尺度 ≥ 请求尺度。

## 3 伪代码

```text
function phase3_resample(hips_dir, params):
  props = read_properties(hips_dir)                    # ALG-P3-001: 必需键校验, 非法显式拒
  validate(params): frame=icrs, W,H∈[1,20000], s_out>0, |center.Dec|≤85°(距极点 ≥5°), pixfrac N/A
    # |center.Dec| ≤ 85°（离两极 ≥5°），与 SCI-P3 §4 的 abs(dec)<=85° 一致。
  order_sel = G3(props.hips_order, W=props.hips_tile_width, s_out)
  cd = G1(params); tiles = TileCache(order_sel)        # 有界 **LRU** 缓存（SharedTileCache：get 时 splice 到表头 = 访问序更新，超容逐出表尾；跨 worker 共享 + 负缓存 + 每 sampler 8 槽热缓存；p3_resample.cpp:63-122、p3_sampler_attach_cache cpp:235）
  parallel for row_band in rows(out):                  # worker pool by affinity, 禁硬编码线程数
    if cancelled(row_band): return CANCELLED           # 行带粒度
    for y in row_band:
      for x in 0..W_out−1:
        (RA,Dec) = G2(cd, x, y)
        ipix = G4.map(RA,Dec, leaf_order)
        vals = tiles.gather(ipix, sampler)             # nearest 1 tile / bilinear ≤4 tiles
        S[y][x], C[y][x], n_rej[y][x] = sample(vals, sampler)
        # sample 必须是什么（§2 G4 冻结口径, rule_id NAN-SAMPLE-MASK-COVERAGE-NAN）:
        #   bilinear: 邻域中 ¬isfinite（含 ±Inf）的样本必须按样本级掩膜从分子、分母、
        #     方差三项一并剔除，剩余有效邻域必须重归一；仅零合格样本时 S=NaN
        #     （覆盖级 NaN）。nearest（单样本）零合格样本同样 S=NaN。
        #   **禁**「零填」替代语义，**禁**静默剔除。
        # C 必须是什么: 只判足迹内有无 tile 像素（存在判定）—— 4 个 tile 均可读则
        #   C=1，值非有限**不改** C；任一角 tile 缺失则 C=0 且 S=NaN。
        # n_rej 必须是什么: 被剔除样本计数 n_rejected_nonfinite（强制计数；计数 0 与
        #   「字段缺失」必须可区分）。承载面见 §2 G4 实现锚注记。
  write_fits_atomic(S, C, cd, provenance)              # ALG-P3-004
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| 缺 tile 文件 | 该足迹 C=0, S=NaN, provenance 记 missing, 不中断 |
| tile 内 NaN（nearest，单样本） | 零合格样本 ⇒ S=NaN（覆盖级 NaN）, C=1(mask 语义=coverage+NaN 判定) |
| bilinear 四邻域**部分**非有限 | 不合格邻域样本按**样本级掩膜**剔除、剩余有效邻域**重归一**后求加权和，并暴露被剔除样本计数 `n_rejected_nonfinite`（**强制计数**，按原因分类、互斥可加）；C=1（4 tile 均可读）——禁「零填」替代语义。方差项随重归一权重传播（Σc'_k²u_k，c'=生效权重） |
| bilinear 四邻域 tile 全缺失 | 该足迹 C=0, S=NaN, provenance 记 missing |
| bilinear 邻域含 ±Inf | ±Inf 与 NaN 同属不合格样本，按样本级掩膜剔除并重归一；coverage 规则同上 |
| RA wrap 0/360 | 球面角差归一, 无接缝 |
| 中心距极点 <5° / 输出跨 TAN 半球 | 显式拒(G2 前) |
| properties 非法/缺键 | 显式拒(ALG-P3-001), 无 silent default |
| JPEG/PNG/int+BLANK/多通道 tile | 显式拒(alpha 范围) |
| pixfrac/单帧参数 | 不适用(N/A), 拒绝字段 |

## 5 确定性与归约

- 逐输出像素独立；bilinear 权重由 leaf 邻域几何唯一确定(无迭代、无重结合)；`order_sel`/`cd` 由参数唯一决定；tile cache 只读(值路径与 cache 命中与否无关——同一 `sample_impl`)。

## 5c SIMD 安全与取消点

- `G2` 内为逐像素标量三角算术(自动向量化安全: 无跨像素依赖)；`S/C` 写入行连续无别名；bilinear 权重和 = 1 ± k·ULP（4 权重显式归一，FP64 累加；**不作逐位/精确断言**——IEEE-754 下不可满足，M7-F-201）；样本级掩膜后该不变量在**合格集**上成立（Σc'_k=1），零合格样本时四权重全 0（此时 S=NaN，Σ=1 不适用）。
- 取消点: 输出行带粒度(ALG-P3-003 循环)；取消时**输出文件不落盘**(tmp 删除, rename 不发生)——FITS 原子性以整文件为单元(ALG-P3-004)。

## 6 时间/空间复杂度

- 时间 O(W_out·H_out·(map+sample))；map O(1)(HEALPix ang2pix), nearest O(1), bilinear O(4)+cache 命中 O(1)；
- 空间 O(W_out·H_out) 输出 + O(cache_tiles·W²) tile 缓存；manifest/provenance O(tiles_used)。

## 7 CPU-only 后端策略（V5）

- 仅 CPU：行带 worker pool（按 affinity 调度, **线程数取自 benchmark profile**）；输出与线程划分无关(逐像素独立+固定序)；无 ISA 变体分支需求(三角函数经 libm, 结果确定性由同 libm 版本冻结, 跨平台数值合同入 SYN-007)。
- **不变性的结构性前提（正向约束）**：输出与 worker 数/行带划分/tile 缓存状态无关，其成立条件是
  **每个输出像素的计算只依赖其固定邻域**（邻域确定 → 最近中心确定 → 权重确定），且
  **跨 tile / 跨像素的可变数值状态一律为空**（累加器、自适应核、依赖历史的重归一等均限于本像素邻域）。
  违反该前提时线程不变性立即失效——这是结构性约束，不是性能性质。
  tile 缓存（有界 LRU / 负缓存 / 容量）只影响 I/O 命中率，**不影响任何像素值**（tile 内容只读）。

## 8 参考实现/Oracle

- reference 实现即生产实现(首版)；Oracle=SCI-P3 §11 全集, **Oracle 不调用本模块**（独立小规模球面 reference + 独立 FITS/WCS 读取器）；容差: WCS roundtrip ≤1e-8 px（生产注册表 `p3_wcs.cpp`（`kTanApplicability`，单一事实源 `p3_wcs_applicability()`））；常数场（nearest）逐值相等；常数场（bilinear）|S−B0| ≤ k·ULP·B0（**不作 max_abs=0 逐位断言**，M7-F-201：Σw=1±k·ULP 经 S=Σw·B0 传递）；解析场容差由 SYN-007 预冻结。

- 掩膜口径 Oracle（非生产自证）：lib/algorithms/resample/tests/p3rsmp/p3_nan_mask_test.cpp ——
  在四角 leaf 逐像素注入 NaN/±Inf，期望值 = 剩余合格邻域**重归一**加权和（本文件独立复算，
  不调用生产聚合路径），并断言被剔除样本生效权重恰为 0、`n_rejected_nonfinite` 分类计数正确、
  零合格样本 S=NaN 且 C=1、方差按重归一权重传播（Σc'_k²u_k）。

## 9 容差来源

- WCS roundtrip 1e-8 px：SCI-P3 §7 不变量(FP64 反向映射+Paper I/II 语义)；
- 常数场（bilinear）：|S−B0| ≤ k·ULP·B0——由 Σw = 1 ± k·ULP 传递（不是逐位 0；M7-F-201）；
- 解析球面场容差：h≤s_out 约束下 bilinear O(h²) 误差界 → SYN-007 表冻结(任务 SYN-007 落实具体数值)；
- 样本级掩膜后的重归一为恒等 ± 舍入（全部样本合格时 c'_k = w_k/(1±k·ULP)）⇒ 不引入
  新容差项，仍落在既有 k·ULP·B0 包络内（Oracle 锚: lib/algorithms/resample/tests/p3rsmp/
  p3_nan_mask_test.cpp 以剩余合格邻域重归一加权和作解析真值，相对容差 4·eps_f32）。

## 10 关联 ARC/API/TST

- ARC: `THREADING_MODEL.md`(worker pool/取消) `PERFORMANCE_BUDGET.md`(P3 预算)
- API: `API-004`(CLI JSONL 输入/输出, 待建)
- TST: `SYN-007` 五件套(oracle 独立性)；`TST-P3-*` 编号随 API-004 建立

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- HiPS 层级/tile 与 order：IVOA HiPS 1.0（https://www.ivoa.net/documents/HiPS/）；Fernique et al. 2015, A&A 578, A114。
- HEALPix 几何/ang2pix：Górski et al. 2005, ApJ 622, 759；astropy-healpix（BSD-3-Clause）。
- 双线性插值：教科书级（Press et al. 2007, Numerical Recipes 3rd ed.）；本模块 order/邻域语义 Project-defined（SCI-P3 §5）。
- WCS 反变换：Paper I = Greisen & Calabretta 2002, A&A 395, 1061（DOI 10.1051/0004-6361:20021326）
  §2.1.1（中间坐标 = CD·(p−CRPIX)）；Paper II = Calabretta & Greisen 2002, A&A 395, 1077
  （DOI 10.1051/0004-6361:20021327）§2.2（三 Euler 角旋转核）/ Table 1（TAN: R=(180/π)·cotθ）；
  FITS Standard 4.0（2016）§4.3/§4.4；astropy 7.0.1（WCSLIB）作独立 Oracle。
- 方差传播（若涉及）：Fruchter & Hook 2002；UNCERTAINTY_AND_COVARIANCE.md。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

