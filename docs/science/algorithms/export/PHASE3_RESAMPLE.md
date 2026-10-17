# Stage3 HiPS → 平面 FITS：G1–G5 科学离散与生产实现合同（ALG-P3）

> **D7-C 正式语义重构**：保全原始 63eccbb6133aef56389641d43c55df6e243819c5 全文214行（blob 7c7d894b5d31c1e2eff4041563f90b157e5674cd）的 G1–G5、公式、示例、实验和参考。以当前 main 的五节点生产 DAG 为 CURRENT；历史伪代码及旧版条件仅作设计沿革，不作为已接线功能；未来应做但未落地的是 TARGET；需要独立数值和真实观测的是 RESEARCH_OPEN。最终产品为单 PRIMARY signal FITS，不要求 variance、ivar、support 独立图层。


> 上游：ACSD_DESIGN.md [D-1]「流程」一节（export 流程）、[D-1]「投影算法」一节法）

> 施工规格（重采样域实现级合同 = `docs/science/algorithms/PHASE3_RSMP_IMPL.md`，投影域 = `docs/science/algorithms/PHASE3_PROJ_IMPL.md`，写出域 = `docs/science/algorithms/PHASE3_FITS_IMPL.md`）；生产源 `lib/algorithms/resample/p3_resample.cpp`（586 行，实测在库）。

## 1 上游 SCI 与输入输出

- 上游: `SCI-P3-001`（PHASE3_HIPS_TO_FITS.md [S-1]「判据与误差」一节 连续定义 + [S-1]「判据与误差」一节十二项冻结）
- 输入: HiPS 目录(properties+tiles, float FITS) + 用户参数 center(RA,Dec)/s_out/W_out/H_out/sampler(nearest|bilinear)/parity(east_left|east_right)/bitpix(-32|-64)
- 输出 CURRENT：仅 PRIMARY signal HDU 的普通二维 FITS，含真实 WCS、BUNIT 与可追溯来源；内部未裁剪 p3_resampled.bin 才含 signal/coverage 两个 f32 工作平面（不把 coverage 作为输出扩展）；文件原子发布，另有产品/verify 摘要。
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
  # 中间坐标 = CD·δp（Paper I §2.1.1）；实现锚 p3_proj.cpp pix_to_plane / p3_wcs.cpp
  TAN 参考点含 CRVAL2（θ0=CRVAL2，Paper II §2.2；CRPIX 处 world == CRVAL）
  gnomonic: (ξ,η)=atan2 形式; 球面角差按 RA wrap 归一

G3 (ALG-P3-003) order 选择 (SCI-P3 [S-1]「判据与误差」一节 冻结):
  s_out_rad = s_out · π/180                         # s_out 单位 deg/px（量纲声明，M7-A-209）
  s_tile_rad(order) = sqrt(π/3) / (2^order · W)     # 叶级像素**等面积等效线尺度**（精确，非近似）
  order_needed = ceil( log2( sqrt(π/3) / (W · s_out_rad) ) )
  order_sel = clamp(order_needed, 0, hips_order)    # = min(hips_order, max(0, order_needed))

G4 (ALG-P3-003) leaf 采样（桥接口径：本 G4 只承接 SCI 的连续定义到离散采样的映射声明，数值实现面以施工规格与生产源为准，本文件不另立与 SCI 冲突的采样语义）:
  leaf_order = order_sel + log2(W)                  # W=hips_tile_width（支持子集 W=512 ⇒ +9）
  ipix = ang2pix_NESTED(nside=2^leaf_order, RA, Dec)
  tile = ipix >> (2·log2(W));  local = ipix & ((1<<2·log2(W))−1); (lx,ly)=nested_local_to_xy(local)
  # >>(2·log2 W) 是 **索引位移**（tile 内 leaf 数 W²），不是 order 偏移；
  # order 偏移是 log2(W)（W=512 ⇒ +9）。
  nearest: S = tile[lx,ly]（**存在判定→coverage**：tile 像素存在即 C=1，值 NaN 照传）
  bilinear CURRENT: 4象限不规则 leaf 中心作局部矩形化权重 w，**Σw = 1 ± k·ULP**；不是面积交叠权，且一次梯度再现一般不成立（#90）。
            S=Σ w·tile_value, 跨 tile 读相邻 tile（NESTED 面邻接含轴翻转/镜像）
            **邻域非有限规则（NaN 处置口径 rule_id NAN-SAMPLE-MASK-COVERAGE-NAN）**:
            不合格邻域样本（¬isfinite，含 ±Inf）按**样本级掩膜**从分子、分母、方差三项
            一并剔除，并对剩余有效邻域**重归一**；仅当零合格样本时 S=NaN
            （**覆盖级 NaN**），且每个输出像素**必须暴露**被剔除样本计数
            （**强制计数**；静默剔除恒不接受）；C 只判足迹内有无 tile 像素，值 NaN 不改 C
            （4 个 tile 均可读则 C=1）。
            实现锚: lib/algorithms/resample/p3_resample.cpp 的 p3_sample_bilinear_nanmask_ex
            （唯一实现；p3_sample_bilinear_ex 为其薄封装，同一数学路径）。
            已对齐项: ①样本级掩膜 + 剩余有效邻域重归一（FP64，固定 k 序）；
            ②weights[4] 暴露**生效（重归一）权重** c_k = w_k / Σ(合格 w_j)，被剔除样本
            恰为 0 —— 方差项必须消费该权重（DATA-002 [S-1]「判据与误差」一节规则一「从分子、分母、方差
            三项一并剔除并重新归一」）；③强制计数经 P3SampleRejection 暴露，字段名取
            权威冻结名 n_rejected_nonfinite（按原因分类，互斥可加）；
            ④C 不变（只判足迹内有无 tile 像素）。
            未冻结项: 该计数的**产品承载面**（逐像素平面 / JSON / provenance）未见权威
            规定 → 不自行发明，登记 ALG-P3-RSMP-IMPL-001 [S-1]「判据与误差」一节DISP-P3RSMP-006。

G5 (ALG-P3-004) FITS 写:
  BITPIX=−32/−64, BSCALE=1, BZERO=0
  BUNIT 科学上应是实际 applied signal 的 L/sr 和响应标度，不凭算法单独猜单位；
          CURRENT 五节点 adapter 缺源BUNIT立即拒绝，仅认约定面亮度词汇；
          库内部的 ADU/sr fallback 不代表 CLI 允许缺失元数据（#9）。
  # 依据 `docs/science/unified/DATA_SEMANTICS.md`「单位与量纲表」与「面亮度单位的推导」两节（FZ-UNIT-SIGNAL-SB FROZEN）：重采样值是输入
  # tile 值的凸组合 ⇒ 与输入同量纲（面亮度）；'ADU/sr' 是计数按立体角归一的合法串，
  # 裸 'ADU' 是每像素计数口径（与数值不符且量纲不可判）；§31.1a 同时否定 'ADU/px^2'。
  # 实现锚：p3_resample.cpp p3_sampler_open_ex（缺省串）→ p3_session.cpp 透传
  # → p3_output.cpp 三处（写盘缺省同串）。
  WCS: G1 全量 + CTYPE=RA---<proj>/DEC--<proj> + CUNIT=deg
  (<proj> 取自已校验 projection, alpha 唯一合法值 "TAN";
   未实现投影在写前 fail-closed, 不落任何 FITS)
  HISTORY: 源 HiPS 标识/order_sel/sampler/软件版本/manifest hash
  coverage: C=1 ⇔ 足迹内存在 tile 像素（值可为 NaN；NaN 只进 S 不改 C）; 无覆盖 S=NaN/C=0
```

推导来源: **SCI-P3-001 [S-1]「判据与误差」一节连续定义与冻结回答的离散化**（G1↔[S-1]「判据与误差」一节, G2↔[S-1]「判据与误差」一节 反向映射, G3↔[S-1]「判据与误差」一节, G4↔[S-1]「判据与误差」一节, G5↔[S-1]「判据与误差」一节）；实现一致性锚（非推导依据）:
`lib/algorithms/resample/p3_resample.cpp`（G3=`p3_order_select`、G4=nearest/bilinear 采样核）
与 `lib/algorithms/projection/p3_wcs.cpp`（G1/G2）。

**G3 的适用域与量纲（证据锚）**：`s_tile_rad` 是**等面积等效**线尺度——HEALPix 同 nside
下所有单元面积严格等于 `4π/(12·nside²)`（Górski et al. 2005, ApJ 622, 759 §4），故
`sqrt(π/3)/(2^order·W)` 与该面积的平方根**逐位恒等**（非近似）；本仓实验
实验/healpix-polar/（叶面积恒等式正本 = `docs/DERIVATIONS-P3.md` [S-3]「协方差传播」一节）：nside=512/1024 × 9 档纬度面积
相对偏差恒 0.0（阴性对照：等经纬网格在 dec=89.9° 偏 −20.5%）。**但它不是各向同性分辨率
上界**：单元局部采样步长（邻元中心角距）随纬度/方向变化，nside=512 实测共边邻元 ∈
[0.63,0.71]×该尺度、对角邻元 ∈ [1.95,2.94]×该尺度。要求方向性分辨率保证的消费方须按局部
步长另加余量；`order_sel` 只保证**面平均**尺度 ≥ 请求尺度。

## 3 原始 G1–G5 实现伪代码（历史设计，需要下方 D7-C 现行调用图校正）

```text
function phase3_resample(hips_dir, params):
  props = read_properties(hips_dir)                    # ALG-P3-001: 必需键校验, 非法显式拒
  validate(params): frame=icrs, W,H∈[1,20000], s_out>0, |center.Dec|≤85°(距极点 ≥5°), pixfrac N/A
    # |center.Dec| ≤ 85°（离两极 ≥5°），与 SCI-P3 [S-1]「参数与常数」一节 的 abs(dec)<=85° 一致。
  order_sel = G3(props.hips_order, W=props.hips_tile_width, s_out)
  cd = G1(params); tiles = TileCache(order_sel)        # 有界 **LRU** 缓存（SharedTileCache：get 时 splice 到表头 = 访问序更新，超容逐出表尾；跨 worker 共享 + 负缓存 + 每 sampler 8 槽热缓存；p3_resample.cpp 与 `p3_sampler_attach_cache`）
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
  write_fits_atomic(S, C, cd, provenance)              # HISTORICAL：实际 CURRENT 为 PRIMARY signal单HDU；C仅内部统计
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| 缺 tile 文件 | 当前样本 S=NaN,C=0；聚合的 missing_tiles provenance 并未接线，已声明tile损坏/读取失败也可能同态折叠，必须额外验证 |
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

- CPU O(W_out·H_out)次像元位置映射/采样外加真实tile I/O；bilinear每像素还用3×3邻叶查找和动态vector，不是纯四次浮点运算。
- CURRENT CLI 采用 Runtime 子块写中间 p3_resampled.bin（2×W×H×sizeof(f32)磁盘字节），随后流式写单 PRIMARY FITS；RAM约为子块×在途任务、共享成功tile LRU、每worker热缓存pin、读解码及调度元数据，而不是整幅S/C必常驻。
- 成功tile LRU可配置 max_tiles，但 absent负缓存独立map没有相同cap；内存峰值与miss竞态须真实测量。原O(W×H)RAM属兼容整幅会话/历史设想，不是默认生产条件。

## 7 CPU-only 后端策略（V5）

- 仅 CPU：行带 worker pool（按 affinity 调度, **线程数取自 benchmark profile**）；输出与线程划分无关(逐像素独立+固定序)；无 ISA 变体分支需求(三角函数经 libm, 结果确定性由同 libm 版本冻结, 跨平台数值合同入 SYN-007)。
- **不变性的结构性前提（正向约束）**：输出与 worker 数/行带划分/tile 缓存状态无关，其成立条件是
  **每个输出像素的计算只依赖其固定邻域**（邻域确定 → 最近中心确定 → 权重确定），且
  **跨 tile / 跨像素的可变数值状态一律为空**（累加器、自适应核、依赖历史的重归一等均限于本像素邻域）。
  违反该前提时线程不变性立即失效——这是结构性约束，不是性能性质。
  tile 缓存（有界 LRU / 负缓存 / 容量）只影响 I/O 命中率，**不影响任何像素值**（tile 内容只读）。

## 8 参考实现/Oracle

- reference 实现即生产实现(首版)；Oracle=SCI-P3 [S-1]全集, **Oracle 不调用本模块**（独立小规模球面 reference + 独立 FITS/WCS 读取器）；容差: WCS roundtrip ≤1e-8 px（生产注册表 `p3_wcs.cpp`（`kTanApplicability`，单一事实源 `p3_wcs_applicability()`））；常数场（nearest）逐值相等；常数场（bilinear）|S−B0| ≤ k·ULP·B0（**不作 max_abs=0 逐位断言**，M7-F-201：Σw=1±k·ULP 经 S=Σw·B0 传递）；解析场容差由 SYN-007 预冻结。

- 掩膜口径 Oracle（非生产自证）：lib/algorithms/resample/tests/p3rsmp/p3_nan_mask_test.cpp ——
  在四角 leaf 逐像素注入 NaN/±Inf，期望值 = 剩余合格邻域**重归一**加权和（本文件独立复算，
  不调用生产聚合路径），并断言被剔除样本生效权重恰为 0、`n_rejected_nonfinite` 分类计数正确、
  零合格样本 S=NaN 且 C=1、方差按重归一权重传播（Σc'_k²u_k）。

## 9 容差来源

- WCS roundtrip 1e-8 px：SCI-P3 [S-1]「参考文献与参考代码」一节 不变量(FP64 反向映射+Paper I/II 语义)；
- 常数场（bilinear）：|S−B0| ≤ k·ULP·B0——由 Σw = 1 ± k·ULP 传递（不是逐位 0；M7-F-201）；
- 解析平滑场误差：矩形bilinear的O(h²)前提包含一次再现；当前不规则四角通常 Σc*x、Σc*y 非零，可产生O(h·|grad B|)主误差；原SYN-007不能充当真实HEALPix梯度误差证书（#90）。
- 样本级掩膜后的重归一为恒等 ± 舍入（全部样本合格时 c'_k = w_k/(1±k·ULP)）⇒ 不引入
  新容差项，仍落在既有 k·ULP·B0 包络内（Oracle 锚: lib/algorithms/resample/tests/p3rsmp/
  p3_nan_mask_test.cpp 以剩余合格邻域重归一加权和作解析真值，相对容差 4·eps_f32）。

## 10 关联 ARC/API/TST

- ARC: `THREADING_MODEL.md`(worker pool/取消) `PERFORMANCE_BUDGET.md`(P3 预算)
- API: `API-004`(CLI JSONL 输入/输出, 待建)
- TST: `SYN-007` 五件套(oracle 独立性)；`TST-P3-*` 编号随 API-004 建立

> 本文引用上游正本（论文式编号，正文引用处均已改为自然语言节名，不再使用跨文档 §N 跳转）：
> - [D-1] docs/ACSD_DESIGN.md（最高设计）。
> - [S-1] docs/science/projection/HIPS_TO_FITS.md（投影导出科学正本 SCI-P3-001）。
> - [S-3] docs/science/noise_snr/NOISE_SNR.md（噪声与信噪比正本）。

## 参考文献与参考代码库（含许可证）

- HiPS 层级/tile 与 order：IVOA HiPS 1.0（https://www.ivoa.net/documents/HiPS/）；Fernique et al. 2015, A&A 578, A114。
- HEALPix 几何/ang2pix：Górski et al. 2005, ApJ 622, 759；astropy-healpix（BSD-3-Clause）。
- 双线性插值：教科书级（Press et al. 2007, Numerical Recipes 3rd ed.）；本模块 order/邻域语义 Project-defined（SCI-P3 [S-1]「判据与误差」一节）。
- WCS 反变换：Paper I = Greisen & Calabretta 2002, A&A 395, 1061（DOI 10.1051/0004-6361:20021326）
  §2.1.2 式(3)（中间坐标 = CD·(p−CRPIX)）；Paper II = Calabretta & Greisen 2002, A&A 395, 1077
  （DOI 10.1051/0004-6361:20021327）§2.3 式(2)–(7)（三 Euler 角旋转核；矩阵形式见附录 A.1）/ §5.1.3 式(54)（TAN: R=(180/π)·cotθ）；
  FITS Standard 4.0（2016）§4.3/§4.4；astropy 7.0.1（WCSLIB）作独立 Oracle。
- 方差传播（若涉及）：Fruchter & Hook 2002；`docs/science/noise_snr/NOISE_SNR.md` [S-3]「协方差传播」一节。


---

## U 承接：`uncertainty_available=false`（fail-closed 唯一出口）

本层产出/消费不确定度子产品时，`uncertainty_available=false` 的处置**承接
`docs/engineering/data/ARTIFACTS.md`（DATA-UNC-001 规则面；同表显式登记面）**（合同面：输入面不含 variance/ivar 子产品
（或权重非纯逆方差、发生 fallback、合成输入非有限被拒等规则项）⇒ **不写**
variance/ivar 子产品 + manifest 写 `uncertainty_available=false` +
diagnostics 标红计数；**该键不是失败态**，是 unavailable 显式登记模式
（禁占位子产品、禁静默缺键、禁用常量 0 冒充）。
键名与取值口径以 `docs/engineering/data/ARTIFACTS.md` 为唯一权威，本层不另立第二套
（本层此前零承接，P-154/DISP-P3UNC-001 勘误）。

## 11. D7-C G1–G5：真实数学与五节点实现（CURRENT / TARGET / RESEARCH_OPEN）

**本节为现行正文的科学与工程解释**。上文保留原G1–G5构造、兼容接口、伪代码和实验判据的历史价值，但以此处明确的科学量与实际源码为消费依据。上位：[最高设计](../../../ACSD_DESIGN.md)、[统一科学模型](../../unified/UNIFIED_SCIENCE_MODEL.md)、[HiPS→FITS](../../projection/HIPS_TO_FITS.md)、[球面重采样](../../resample/RESAMPLE.md)；函数级详细设计：[Projection](PHASE3_PROJ_IMPL.md)、[Resample](PHASE3_RSMP_IMPL.md)、[FITS](PHASE3_FITS_IMPL.md)，以及[Export modules](../../../detail/export/modules.md)。

### 11.1 G1/G2 WCS、面积与裁剪

CURRENT：`p3_wcs_make` 以 ICRS中心、输出角尺度s（deg/px）、W/H、经向手性和位置角PA构造 CD-only TAN。零基输出像元中心(x,y)唯一桥接到FITS一基(x+1,y+1)，参考像元 `CRPIX=((W+1)/2,(H+1)/2)`；中间坐标向量 `xi=CD*(p-CRPIX)`。east_left未旋转CD=diag(-s,+s)，east_right=diag(+s,-s)，两者行列式均=-s²；PA非零时 `CD=R(-PA)*diag(sign_x*s,sign_y*s)`，`R(-PA)` = `[[cos(PA),sin(PA)],[-sin(PA),cos(PA)]]`。原文写CD非对角元永远为0，只是 PA=0 的子情形。`p3_wcs_pix2world` 应用TAN反投影、球面RA归一；`p3_wcs_world2pix` 做正向检验，内部0/1原点只转换一次。CURRENT只实现TAN，其他注册投影不能据枚举宣称已生效；|CRVAL Dec|≤85°、FOV≤20°和尺度分层roundtrip 1e-8 px（限s≥0.9″/px）/全域1e-6是软件适用域，不是TAN理论奇点。输出crop零基起点(x0,y0)使`CRPIX'=CRPIX-(x0,y0)`，CRVAL/CD不变，子窗只是读源像元，无新的空间插值。外部WCSLIB/Astropy与12 face/RA包角做双向对照才能证实几何绝对正确。

**被估计量**：Stage2/单帧HiPS leaf提供线性面亮度估计 `B_j`，单位`L/sr`，不是积分通量、不是星点`F_ref`，也不是SNR。输入leaf大体代表有PSF/覆盖/Drizzle算子作用的叶面积平均（不是无噪声连续天空点样本）；输出若定义为目标WCS球面足迹`Q_i`的面积平均，需要用真实球面叶`H_j`交叠面积`a_ij=area(Q_i intersect H_j)`，则已覆盖部分的面亮度估计是 `B_i=(sum_j a_ij*B_j)/(sum_j a_ij)`，几何覆盖分数可取`C_i=sum_j a_ij/area(Q_i)`。这是一种**保守面积核的研究目标**，要求真实HEALPix曲边、有效域和部分覆盖策略；**CURRENT P3没有实现它**。缺失区域不是零亮度，也不可仅靠Σ权=1声称星点积分光度守恒。积分flux还要乘该目标像元的真实sr面积，并说明参考通带/PSF。

### 11.2 G3选阶与#86：数学正确不等于执行生效

HEALPix NESTED叶阶`L=K+9`、叶nside=`512*2^K`，面积`Omega=4*pi/(12*nside²)` sr，等面积等效角宽`h_K=sqrt(pi/3)/(512*2^K)` rad。原G3式 `K_sel=clamp(ceil(log2(sqrt(pi/3)/(512*s_out_rad))),0,K_input)` 是`p3_order_select`线性扫描的精确整数等价式；这仅对应等面积线尺度，不保证所有方向的最大邻点间隔、MTF或Nyquist带限。

**#86当前源码缺陷**：`p3_op_resample` 先通过`p3_sampler_open_ex`打开 input HiPS，其`P3SamplerImpl.order=input_order`与`leaf_nside=512<<input_order`已固定；调用`p3_order_select`后只把`order_sel`写入产品和后续ORDERSEL，`p3_sample_nearest_ex`、`p3_sample_bilinear_ex`、`read_leaf`仍按 input_order读最高层。兼容`p3_session`也有同类风险。因此当`K_sel<K_input`，实际source layer不是所声明的selected layer；原G4使用`order_sel+9`计算叶级索引和`TileCache(order_sel)`的伪码**没有接入生产**。尺度粗化时用最高阶样本点不等于读面积聚合的HiPS父层，可能对棋盘格/高频星核产生aliasing；须用完全相同输出WCS、真实两级HiPS外部reader比较。TARGET可由科学裁决采用真正selected source order并将缓存、reader、source_order/provenance一致绑定；或者保留最高层做显式低通/保守目标像元积分，重命名order_sel为诊断而非“已应用”，不可只修FITS头数字。

### 11.3 G4最近邻、四象限局部bilinear及#90

CURRENT的nearest经球面`ang2pix_nest`找到包含输出中心方向的唯一叶，零阶保持该leaf面亮度；尽管无权重插值，仍有有限像元抽样/块状alias。bilinear经中心leaf和最多8个相邻leaf中心投影到目标点局部切平面，在四象限挑最近中心（距离相同时最小ipix），缺象限复用整体最近中心。算法仅据`x00,x10`计算u、`y00,y01`计算v，再用矩形形式`w=((1-u)(1-v),u(1-v),(1-u)v,u*v)`；不是原文所称“叶重叠面积分数”，也不等于不规则四边形真实形函数。

固定有效值下常量场再现需`sum_k c_k=1`；对切平面仿射场`B(x,y)=B0+bx*x+by*y`精确还必须`sum_k c_k*x_k=0`且`sum_k c_k*y_k=0`。#90给非矩形四象限反例：四角坐标`(-e,-e),(e,-e),(-2e,e),(e,e)`，矩形系数各1/4，在`B=B0+x/e`给目标误差`−0.25`。**这只是合法非矩形点集的解析反例，并非已测得某个实际HEALPix ipix误差**。正式实验需真实各face/极点/纬度转折/邻面、不同亚像元相位及PSF/扩展源，检查一阶矩与FWHM/孔径flux/低频谱。候选替代：非退化三角形barycentric（凸包+条件数）、真实四角逆双线性，或真实曲边面积交叠；分别限定目标量与资源成本。P1 #15的曲边Drizzle是**此前第一个球面投影算子**，不是P3现有bilinear。

### 11.4 sample资格、覆盖、mask、同一leaf和不确定度

nearest：tile不存在或读取失败时`C=0,S=NaN`；tile存在但值非有限时`C=1,S=NaN`。bilinear若所用任何角tile未读到`C=0,S=NaN`；若tile均存在则C=1，即便全部值NaN/Inf（无合格值）而S=NaN。部分非有限时定义`E={k:isfinite(X_leaf(k))}`，有效几何权`c_k=w_k/(sum_E w)`，剔除角的c=0，`S=sum_E c_k*X_leaf(k)`；分母非正或E为空写NaN而非补0。有限零/负线性面亮度均为合法科学值。内部coverage是读到的tile/足迹语义，不是科学信号finite/precision可用性，也不是最后FITS独立mask HDU。

`p3_sample_bilinear_nanmask_ex`有逐样本`P3SampleRejection`计数，但**CURRENT adapter调用`p3_sample_bilinear_ex`，没有消费/发布计数**；原文“必须暴露”是TARGET而非生产证据。#89已对输出`weights[4`]先初始化0，无效角权重为0。某leaf在退化邻域出现多次时归并`a_j=sum_{k:id(k)=j}c_k`；若不同leaf条件独立，其方差`V_out=sum_unique_j a_j²*V_j`，不能把重复角当不同随机变量。#88实际`p3_uncertainty_propagate`已按完整uint64 leaf_id聚合。对普遍跨leaf相关，真值仍`V_out=a^T C_leaf a`；Stage1 Drizzle、父层归约、P2光度和天空拟合会制造相关，不能由单signal反推出C。`p3_uncertainty_open/propagate`作为库能力存在，但生产不打开且最终PRIMARY无variance/ivar/权重图；不可伪造正ivars。

### 11.5 G5当前FITS输出、物理单位与#94完整性

真正CLI操作：`p3_op_properties`读取完成HiPS与BUNIT（缺BUNIT拒）、`p3_op_wcs`构造TAN/crop、`p3_op_resample`用Runtime受控块按**输入最高order**读取并位置写`p3_resampled.bin`两个f32平面（signal、coverage，原W/H行主序），`p3_op_writer`按同crop计划读取signal子窗，通过`P3FitsStream`仅写一个PRIMARY f32来源数据（BITPIX−32/−64，−64不会逆转之前的f32舍入）。`P3FitsStream::publish`执行tmp→PRIMARY DATASUM/CHECKSUM→flush→close→fsync→发布前tmp checksum验证+SHA256→atomic rename；之后`p3_op_verify`通过`P3FitsVerifyStream`逐块读回WCS/数据、另算canonical内容hash。`integrity_sha256`整文件字节hash与`canonical_sha256`产品规范科学身份不是同一对象。作crop CRPIX仅减原窗口零基原点。

#94 **SOURCE_CONFIRMED 当前真实生产控制流缺陷**：`P3FitsVerifyStream::close`可在内部数值/头不符时置`result.reopen_ok=0`但返回`P3_OUT_OK`（读回过程自身成功），`p3_op_writer`不据结果失败、`p3_op_verify`记录`reopen_ok`和`canonical_match`为false之后仍`Result<void>::success()`；“存在JSON”不能作为完整性成功/complete证书。TARGET要在真正CLI消费两个布尔，确保失败产物隔离/rollback与完成身份收敛，故障注入WCS、像元、canonical元数据false而非仅让read抛IO。流式reader与整幅verify的BUNIT/CHECKSUM检查覆盖范围不同，外部FITS读者必须独立核验。

#9 applied数值若按有量纲alpha从原ADU转换到模型L，BUNIT必须是L/sr；当前生产adapter主要识别ADU/sr字符串，绝不能以“所有乘数无量纲”省略光度标度。#79 P2的mono/unknown/mixed真实响应label没有进入P3结构化FILTER及有效响应来源，不能把mixed标签写成真正单滤镜身份。P3科学质量需要分离前级P1→HiPS、HiPS层级派生、P3输出核三段PSF/噪声影响，而非把噪声视觉变平滑称为信息增加。

### 11.6 资源、错误状态与真实计划

CPU依输出像素数O(WH)的WCS/HEALPix/邻域计算与tile I/O；bilinear每像素调用邻叶向量，不只是四次加权。磁盘工作文件是`8*W*H`字节（两个f32 plane），最终FITS数据字节为`(-BITPIX/8)*W_crop*H_crop`外加2880B FITS头/对齐；RAM含Runtime blocks、queue、每worker sampler热缓存、共享成功tile LRU、读解码和**独立无cap的absent负缓存map**，不能只估max_tiles×1MiB宣布OOM安全。调度资源由Runtime预算而非自动开满CPU；取消、未完成block和writer失败须阻止成功产品。当前合法mode为surface_brightness/visualization，point_source_flux显式不支持；P3ResampleStatus OK/PARAM/UNSUPPORTED/IO与P3OutputStatus OK/PARAM/IO/CANCELLED不能被直接当作CLI退出码。

**独立实验矩阵**：
1. WCS：even/odd画幅、PA非0、两种parity、RA跨零/高Dec/非对称crop、双重origin负例，对拍WCSLIB/Astropy的绝对坐标与CRPIX。
2. #86：实际完整两级HiPS正弦和棋盘、同WCS改变选阶；外部reader核对真正读到的Norder与宣告ORDERSEL、alias/通量偏差。常量场是弱正对照。
3. #90：全12face、pole/apex、tile seam/HEALPix四角实际几何，按Σw、Σwx、Σwy、真实非矩形barycentric oracle及PSF FWHM/MTF/孔径flux评估。
4. NaN/coverage：缺tile/已声明损坏tile、1–4无效角、重复leaf、finite0/negative；API与CLI比照，明确missing vs invalid vs corrupt。
5. 相关噪声：人工协方差含重复角[ A,A,B,C ]、跨叶共模、父阶来源相关，比较Σa²V与aᵀCa；不得凭单图生成精度层。
6. #94：合法且可读的FITS头/pixel/canonical metadata逐项篡改，定位reopen_ok=false或canonical_match=false后实际CLI必须nonzero并且不保留COMPLETE。
7. HST真值模拟+真实M42/E2，分别消除/替换三段重采样，测星点flux、PSF、低频、真实cover和用户目检；1/N workers/sub_block_px/max_tiles/大量负缓存下的真实RSS和I/O。

**证据等级**：本D7-C只做全文原文精读、源码静态函数/语义核对和文档修订；上述独立数值、C++/CLI构建、Astropy/HEALPix外部Oracle、HST/M42真实运行、RSS、Windows和故障注入均 NOT_RUN。旧文 TEST 例据保留为可复现设计，并非本轮PASS。

一手资料定位：Górski et al. 2005, ApJ 622,759 DOI 10.1086/427976；Fernique et al. 2015 A&A 578,A114 DOI 10.1051/0004-6361/201526075；IVOA HiPS REC-1.0 https://www.ivoa.net/documents/HiPS/；Greisen & Calabretta 2002 A&A 395,1061 DOI 10.1051/0004-6361:20021326；Calabretta & Greisen 2002 A&A 395,1077 DOI 10.1051/0004-6361:20021327；Fruchter & Hook 2002 PASP114,144 DOI 10.1086/338393；FITS 4.0 https://fits.gsfc.nasa.gov/fits_standard.html；CFITSIO https://heasarc.gsfc.nasa.gov/fitsio/；独立参考实现 Astropy WCS（BSD-3）、astropy-healpix（BSD-3）。本轮未逐页重新下载核实外部版本和许可证。

