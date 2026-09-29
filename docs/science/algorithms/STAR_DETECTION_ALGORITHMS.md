# Star Detection Algorithms (ALG-STARDET-001)

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

> 上游 SCI: SCI-PSF-001（docs/science/PSF.md，FROZEN 共享引用不改动）；本域冻结层
> SCI-P1-STAR-001（§11.5，ALG 内冻结层，共享 SCI 不改动）
> 下游: DATA-P1-STAR（DATA_SEMANTICS §17）、API-STAR-001（PUBLIC_API）、
> MOD-astrocs-phase1-star（registry）
> 唯一权威生产源: lib/algorithms/star_detection/src/sdet_api.cpp（2497 行；源文件唯一在役副本）；合同头
> lib/algorithms/star_detection/include/star_detector.h（139 行）；取值与签名一律以本头文件为唯一来源。
> 矩阵行: docs/traceability/TRACEABILITY_MATRIX.json MOD-astrocs-phase1-star
> （matrix P1-STAR，legacy_paths=lib/algorithms/star_detection;lib/algorithms/star_detection/wrapper_phase1，
> 迁移目标 astrocs_p1_star_detection.dll）。

## 1 上游 SCI 与输入输出

- SCI-PSF-001（PSF 拟合共享 SCI）：星检测为 PSF 拟合与 plate solve 提供候选/中心，
  本文档只登记检测侧算法事实，不修改共享 SCI。
- 输入: 单帧图像（生产通道 FP64 `double` 或 FP32 经 uint16 量化，`[h·w]` 行主序，ADU）
  + SDetParams 参数（star_detector.h:14-23）。
- 输出: 逐星 `(cx,cy,flux,mag,saturated,has_saturated)` 十数组（`double*/float*/int*`，
  malloc 由调用方 `sdet_free_detect_ex` 释放）+ 可选 extras 列。
- 生产调用: orchestrator.cpp:2067 run_stage_psf（PSF/STAR_MEASURE 阶段，一帧一次
  权威检测）→ :2153-2157 sdet_detect_ex / sdet_detect_ex_f64 / sdet_free_detect_ex
  函数指针 → star_det 权威块 FLOAT64[N,6]（orchestrator.cpp:2237-2246）。
- descriptor astrocs.phase1.star-psf（module_adapters.cpp:872-897）为编排层词汇，
  不含独立 star_detection descriptor；本模块合同以 ALG-STARDET-001/DATA-P1-STAR/
  API-STAR-001 为准。

## 2 离散公式

（锚=sdet_api.cpp 实测行号；公式与源码一一对应，正文按源码照录）

- 背景噪声（行差分族 `sdet_compute_bgnoise()` :605-657；调用点 :1504）: 逐行差分 `d[y,x]=img[y,x]−img[y,x−1]` →
  3 轮 5σ clip（median/MAD 迭代，MAD 含 1.482602218505602）→ 行标准差 → 行中位 → ×0.7071
  （1/√2）。**量纲 ADU；输出量 = 单像素噪声 RMS `σ_n`。**
  **适用域（必须随引用同写）**：`Var(d) = 2σ_n²(1−ρ_adj)`，`ρ_adj` = 相邻像素
  噪声相关系数 ⇒ 估计量 `σ̂_n = σ_n·√(1−ρ_adj)`。**仅在 `ρ_adj = 0`（相邻像素噪声
  独立）时无偏**。实测（合成 AR(1) 场，4096 px/行，3 帧/档）：
  `ρ_adj` = 0 / 0.25 / 0.50 / 0.75 ⇒ `σ̂_n/σ_n` = 0.9999 / 0.8632 / 0.7031 / 0.4991，
  与解析 `√(1−ρ_adj)` = 1 / 0.8660 / 0.7071 / 0.5000 一致到 ≤0.6%。
  **测法**：合成 AR(1) 场，4096 px/行、3 帧/档，真值 `σ_n = 6.0 ADU`，`ρ_adj` 由场
  生成参数直接设定（非事后拟合）。**负对照**：`ρ_adj = 0`（独立噪声）时
  `σ̂_n/σ_n = 0.9999` 无偏（正例）；`ρ_adj = 0.75` 时 `0.4991` 低估 50.1%
  （负例 ⇒ 该估计量在相关噪声下必须判红）。
  ⇒ 对 drizzle/重采样/插值后的帧，**必须**先用独立方法标定 `ρ_adj` 并除以 `√(1−ρ_adj)`；
  `ρ_adj` 未标定时检测阈的 σ 基准 = 另行独立标定的估计量。
  纯独立高斯噪声下实测无偏：`σ̂_n/σ_n = 0.99792`（8 帧，真值 6.0 ADU，
  散布 0.42%）。
- 全局检测阈值（:1504 调 `sdet_compute_bgnoise()` 估 `bgnoise` 于**未平滑原图**；:1529 组装 `pr.thr = pr.bg + 5.0·bgnoise`；阈值作用面 = :1497-1502 的 σ=2 平滑图；候选门槛判据 :1902、对称性门 :1894-1901、饱和判定 :1816-1819）:
  `threshold = median(img) + 5.0·bgnoise`，**单位 ADU**。
  **量纲声明**：阈值**作用图像**是 `σ_smooth = 2.0 px` 的 YvV 平滑图（:1496-1502），
  而 `bgnoise` 取自未平滑原图 ⇒ 阈在平滑图噪声单位下 = `5.0/‖k‖₂ = 35.45 σ_smooth`
  （连续 2D 高斯核 `‖k‖₂ = 1/(2σ_smooth√π) = 0.14105`）。
  **`threshold_sigma` 是未平滑原图噪声的倍数，不是阈值实际作用图像上的显著性**；
  引用「5σ」时**必须**声明 σ 属于哪幅图。等价地，检出**过渡点**在
  `SNR_peak = A_fit/σ_bg` 单位下是 `SNR_peak = 5·(1 + σ_smooth²/σ_psf²)`（连续极限），
  实测系数为 `6.22 ± 0.50`（**50% 过渡点**，§11.4 F1）。该式给出的是检出概率 0.5
  的位置，**不是召回域下限**；召回域下限 = §11.4 F1 的逐档 **99% 召回阈表**
  （σ_psf = 1.0 / 1.27 / 1.5 / 2.0 / 2.5 / 3.0 px 档；数值以本文件 §11.4 表
  「99% 召回阈 `SNR_peak`」行为唯一权威，此处不复制数值以防漂移），两者**不可互换**。
- 动态范围与饱和水平（:1523-1529）: `bg=median(img)`，`maxi=max(img)`，
  `dynrange=min(maxi,65535)−bg`，`minsatlevel=0.7·dynrange`，
  `satrange=0.1·dynrange`，`locthreshold=5·bgnoise`；`norm = 65535.0f` 字面量
  （:1525，DISP-STAR-006）。**量纲 ADU；适用域 = 以 uint16 整数 ADU 读出、
  满阱=65535 ADU 的帧**。对已定标 float 帧，`dynrange` 的物理含义退化为
  「帧自身 max 与 65535 的较小者减帧中位」，**不等于探测器饱和电平**；
  该域外 `saturated` 列**必须**按 Project-defined 判据
  `A_fit > min(max(img),65535) − median(img)` 消费；该列语义 = Project-defined 判据，探测器饱和真值另立。
  实测（M42_M1_T2 Red 20251212@012404 校准帧，4096²，float32，max=80789.7 ADU）：
  `dynrange=65339.0 ADU`，`saturated=1` 的检出星 94/2474，其中 53 颗 3×3 峰值
  ≤ 65535 ADU。**测法**：生产盲检测输出逐星取 3×3 峰值与 `saturated` 列。
  **负对照**：上述 53 颗「3×3 峰值 ≤ 65535 却被标 `saturated`」即 `norm = 65535`
  不成立的反例 ⇒ 该列在 float 定标帧上**必须**按 Project-defined 判据消费。
- 8-连通组分标记（O3, `sdet_find_connected_components` :79-125）: 阈值图 `smooth > thr` 的
  非零像素按 **8 邻接 BFS** 标记连通分量（扫描序 y 主序发现、栈式 BFS ⇒ 平局 = 扫描序先到），
  每分量记 `count` 与包围盒 `x0/y0/x1/y1`（:101-104）；释放面
  `sdet_free_connected_components` :127-135。复杂度 O(N)。
- 去混叠 deblending 树（O11, `sdet_deblend_leaf` :1080-1341；设计稿 §5.11；B&A96 §4）:
  指数间隔阈值层 `t(i) = thr·(Smax/thr)^(i/30), i=1..30` 自高向低（`SDET_DEBLEND_LEVELS=30` :1058），
  首个满足「≥2 枝且枝在**当前层**阈值之上的积分流量 > `delta_c·`父组分在**检出阈值**之上的总流量」
  的层执行分裂（`SDET_DELTA_C=5e-3` :1059；判据两侧刻意不对称 = B&A96 §4）：
  存活枝 → 独立成分；其余像素按双变量高斯 **argmax 重分配**（`mu` = 叶峰位、
  `sigma²` = 叶内权重二阶矩 + 0.25 下限），平局归登记序靠前者。
  全层不满足 ⇒ 单成分（间隔 < 2σ 不可分，B&A96 §4.3，不触发即保持单星）。
  `SDET_DEBLEND_DEPTH=8` :1060 在现行**迭代**实现中无消费者（P-140 已登记，保留占位）。
  产物 `SdetLeaf{pix, peak}` :1075-1078；`peak` 现仅供树基准与调试消费
  （O8 冻结语义改由检测段 11×11 局部极大扫描承担，:1691-1726）。
- 候选饱和判定（:1796-1819）: 3×3 邻域超阈值像素 `meanhigh`、`minhigh`；
  饱和 ⇔ `meanhigh−bg ≥ minsatlevel` 且 `pixel0−minhigh ≤ satrange`（双条件）。
- 非饱和亚像素质心（一阶导数，:1821-1839）: `r0 = −0.5 − d1rl/(d1rr−d1rl)`、
  `c0 = −0.5 − d1cu/(d1cd−d1cu)`（分母 |·|<1e−20 时取 −0.5）。
- 饱和中心 edge-walking（:1840-1854；独立实现 `edge_walking_center` :736-784，
  该独立实现现仅由无调用者的 `sdet_detect_saturated_stars` :785-904 消费，见 §10）:
  从候选中心四方向走到 `pixel ≤ sat` 边缘，饱和中心=边缘盒几何中心；
  距边界 <2px 丢弃。
- 二阶导数零交叉宽度/振幅（:1856-1870；独立实现 `sdet_zero_cross_dir()` :1343-1387）: 对平滑图 smooth 逐方向找二阶导零交叉
  `Sr,Sc`（平滑图 σ 估计）与 `Ar,Ac`；平滑 PSF 估计量
  `Sr=(−srl+srr)/2`、`Ar=(Arl+Arr)/2`、`Sc=(−scu+scd)/2`、`Ac=(Acu+Acd)/2`；
  符号约定：单侧读数 `srl/srr/scu/scd`（及 `Arl/...`）记录**有符号差分读数**，
  距离量取正值为有效（实现 `sdet_zero_cross_dir()` 返回 `dist = |峰−零交叉| ≥ 1.0`，
  缺失侧回落 1.0）；对称合成取两侧均值 `Sr = 0.5·(drl + drr)`（sdet_api.cpp
  O7 段，`c.Sr = 0.5 * (drl + drr)`），同式得 `Sc`；
  `SDET_SQRT_EXP1=√e`（:1061）。
- 拟合盒半径（:1872-1876）: `s_factor=√(−2·ln 0.001)=3.7172`（`SDET_S_FACTOR` :1064）；
  `R=max(ceil(3.7172·Sr), ceil(3.7172·Sc), 5)`，钳位 `R≤200`（`SDET_MAX_BOX_RADIUS` :1057），
  边界收缩 `R=max(R,1)` 且不出帧。
- 对称性质量门（:1894-1902）: `dA=max(|Ar|,|Ac|)/min(|Ar|,|Ac|)`、`dSr`、`dSc`
  同构（分母 <1e−20 时 1e30）；拒绝条件 `dA>2 ∨ dSr>2 ∨ dSc>2 ∨
  max(|Ar|,|Ac|)<locthreshold`。
- candidate 去重（:1906-1928）: `matchradius=max(1, floor(0.2·R))`；
  曼哈顿距 `|Δx|+|Δy|≤matchradius` 视为重复，先到先留（扫描序 y 主序）。
- **椭圆高斯拟合模型（检测侧冻结母函数）**（sdet_gauss_fit :659-728 → sdet_lm_fit :387-604，
  自研信赖域 LM（nls_lm），7 参数）:
  `f(x,y)=B+A·exp(−(x′²/SX+(y′/r)²/SX)/1)`，参数 `{B,A,x0,y0,SX=2σ²,fr,alpha}`，
  `r=0.5·(cos fr+1)`，`sx=√(SX/2)`，`sy=sx·r`，`fwhm=2.3548·σ`（TWO_SQRT_2_LOG2），
  PSF 侧 = 椭圆 Moffat4（SCI-PSF-001 §5）；同 sx 下 FWHM 相差 1.9140×，两块的 FWHM 各自独立取值（DISP-STAR-007）。
  `theta=−alpha` 归一到 (−90°,90°]。残差坐标 `dx=x+0.5−cx`（像素中心=索引+0.5，
  :688-689）。
- 初始化（sdet_lm_fit :398-445）: halfA 边界搜索（`max_val=A0+bkg0`，沿中心行列向外走到
  `val−bkg0 ≤ A0/2`）→ `FWHMx=jj1−jj2`、`FWHMy=ii1−ii2`；`SX_init=FWHM²/4ln2`、
  `fr_init=acos(2·roundness−1)`、`alpha_init=0`；`bkg0`=拟合窗内下半像素截尾 MAD
  clip 后中位（sdet_gauss_fit :696-712）；`max_iter=LM_MAX_ITER_ANGLE·(饱和?3:1)`（:461；常数 :48）。
- 饱和像素 mask（:686）: 拟合采样排除 `val ≥ sat_threshold`；
  排除后 m≤8 → INVALID_PARAMS（:694，`samples.size() <= NPARAMS+1`）。
- 星等（正常星，:1990-2008）: `mag = −2.5·log10(Σ_box(pixel − B_fit))`，box=
  (2R+1)² 用候选 R（钳 [5,200]）与候选中心；`box_sum≤0 → mag=NaN`。
  饱和星：原 debug 路径（:1597）已随重写注销——`sdet_detect_debug`/`sdet_free_debug_maps`
  为**死符号**（§10），`sdet_detect_saturated_stars` :785-904 现无调用者 ⇒ 该路径的
  `mag = −2.5·log10(A_fit)`（A>0）在生产面不再产出（量纲差异登记 DISP-STAR-004）。
- 饱和标志（:1979）: `is_saturated = (A_fit > dynrange)`；`has_saturated =
  is_saturated`（:1980，DISP-STAR-004）。
- 输出排序（sdet_sort_stars :980-1037）: mag 升序 stable_sort，NaN 恒排末尾；
  dedup（sdet_dedup_stars :952-979）: 饱和星与正常星 2px 网格 d²<4.0 →
  丢正常星保饱和星；饱和星间 d²<4.0 保 r 大者；正常星间 d²≤1.0 保先者；
  最后 `maxStars>0` 截断（:1546-1547，`sdet_emit_records` 内）。

## 3 伪代码

```
sdet_detect_impl(image, w, h, params):            # sdet_api.cpp:1630（模板双实例 :2319/:2338）
  smooth   = GaussianBlur_YvV(image, sigma=2.0)   # :1497-1502（Young-van Vliet IIR）
  bgnoise  = sdet_compute_bgnoise(image)          # :1504 调用（族 :605-657，行差分+3×5σ clip）
  median   = robust_median(image)                 # :1487-1490
  thr      = median + 5·bgnoise                   # :1529（locthreshold :1528）
  dynrange = min(max(image), 65535) − median      # :1525
  binary   = (smooth > thr)                       # O3 阈值化
  comps    = label_8connected(binary)             # :1666-1667 → sdet_find_connected_components :79-125
  leaves   = deblend_tree(comps, thr, total_flux) # :1770-1776 → sdet_deblend_leaf :1080-1341（O11，设计稿 §5.11）
  for y,x in [r..h−r)×[r..w−r):                   # 候选扫描段
    if max(|Ar|,|Ac|) < locthreshold: continue    # :1902（对称性门 :1894-1901；guided 路径 :2185）
    if not local_max_11x11(smooth,y,x): x+=5; continue   # :1691-1726 平局决胜取左上唯一
    (meanhigh, minhigh) = stat3x3(image, x, y, thr)      # :1799-1815 原图 3×3
    if boundary(xx±2,yy±2) 越界: continue         # :1789
    if meanhigh−bg < 0.7·dynrange or pixel0−minhigh > 0.1·dynrange:
        (r0,c0) = first_order_centroid(smooth,xx,yy)     # :1821-1839 非饱和
    else:  sat=1; (r0,c0)=edge_walking(smooth,xx,yy,sat_level)  # :1840-1854 饱和
    (Sr,Sc,Ar,Ac) = zero_cross_2nd(smooth,xx,yy,sat)     # :1856-1870
    R = max(ceil(3.7172·Sr), ceil(3.7172·Sc), 5); clamp R ≤ 200, 帧内收缩  # :1872-1876
    if dA>2 or dSr>2 or dSc>2 or max(|Ar|,|Ac|)<5·bgnoise: continue  # :1894-1902
    if exists prior candidate with |Δx|+|Δy| ≤ max(1,⌊0.2R⌋): continue  # :1906-1928
    candidates.push({x+xx偏移, y, mag_est=meanhigh, Sr, Sc, R, sat_flag})
  sort candidates by mag_est desc（候选阶段不截断）       # :1937-1939
  parallel for c in candidates:                   # 阶段6 :1942-1958 OpenMP dynamic
    fit = NLS_TR_LM_GAUSS(image, window=2R+1, sat_mask=c.sat, init=halfA+局部截尾MAD)
  for (fit, c) in zip(fit_results, candidates):   # 阶段8 :1960-2010
    if fit.status ≠ OK: drop                      # :1961
    if max(sx,sy)/min(sx,sy) > maxAxisRatio: drop # :1966-1970
    if reject_star(fit) ≠ OK: drop                # :1973（FWHM>0.5、圆度≥0.5、
                                                  #   RMSE=mad·1.482602218505602/A≤0.2、FWHM 上限;
                                                  #   饱和豁免 RMSE :319-341）
    mag = −2.5·log10(Σ_box(pixel − B_fit))（box=R, 中心=候选中心）  # :1990-2008
    saturated = (A_fit > dynrange); has_saturated = saturated       # :1979/:1980
  dedup(stars); sort_by_mag_asc(stars, NaN last); 截断 maxStars   # 阶段10 :1544-1547
  return 10×malloc 数组 (cx,cy,flux,mag,saturated,has_saturated[,extras])  # :1537-1623
```

## 4 边界/NaN/Inf

- 候选为空（:1226-1231/1630-1638/2131-2141，分别 = sdet_detect 旧入口 / debug / impl）: 全输出指针置 NULL，`*out_count=0`，
  返回 0（不是错误；"未检测到星"由编排层判定，orchestrator.cpp:2200-2212
  det_count≤0 → STAR_DETECT_FAILED）。
- 参数无效（impl :1749）: handle/image/输出指针 NULL → −1；width/height ≤0 → −1
  （C 入口 :1030 / ex :2914 / ex_f64 :2939 同式检查）。
- 拟合失败（:621-632）: LM 状态≠Success 或非有限值或 A≤0 →
  NO_CONVERGENCE，该候选丢弃（:2278），不产出 NaN 行。
- mag NaN: 正常星 box_sum≤0（:2337）；NaN 排序恒在末尾（:972-981）。

- 饱和星拟合失败: flux/fwhm/sx/sy/theta/background/amplitude 全 0.0f 哨兵 +
  mag=NaN（:1599-1610 debug 路径语义；extras 读取端 get_extra_field :809-850）。
- 边界: peaker 扫描域 [r,h−r)×[r,w−r)（:1843-1844）；边界检查 xx±2/yy±2
  （:1892-1893）；R 收缩不出帧（:2065-2070）；edge-walking 距边界 <2px 丢弃
  （:655-657）。

## 5 确定性与归约

- OpenMP 仅三处并行: 平滑/转化 `parallel for schedule(static)`（入口转换
  :1036-1039/:2917-2920、bgnoise 行并行 :454）、候选拟合 `omp parallel +
  omp for schedule(dynamic) reduction(+:fit_ok_count)`（:2183-2184）。
- 拟合逐候选独立、结果按索引写回；dedup/sort/maxStars 截断串行
  （:2363-2385）→ 输出 bitwise 与线程数无关（1/N 等价）。
- 扫描序固定（y 主序、x 递增）+ 先到先留去重 + stable_sort → 全序确定。
- determinism=fixed_reduction_order（module.yaml 合同值）。

## 6 时间/空间复杂度

- 平滑 O(N)（YvV 递归 IIR）；bgnoise O(N·clip)；peaker O(N·r²)（r=5，11×11 窗，
  命中后 x+=5 跳步 :1868）；拟合 O(C·m·iter)（C=候选数，m=(2R+1)² 采样数）。
- 空间: smooth/map O(N) + 候选/拟合 O(C) + 输出 O(count)。
- maxStars 截断: 输出阶段截断 maxStars（:2383-2385）；impl 候选阶段仅排序
  不截断（:2170-2171）——maxStars×2 预截断仅存于 debug（:1459-1462）与
  guided（:2707-2708）路径。

## 7 CPU-only 后端策略（V5）

- 纯 CPU；自研信赖域 LM（nls_lm，替代外部 GSL gsl_multifit_nlinear，
  sdet_api.cpp:28、:375-385）；OpenMP 线程级并行；无 SIMD 内联/ISA 分派（-march=native 于
  Makefile:22，非代码分支）；V7 迁移目标 astrocs_p1_star_detection.dll
  （provider/cpu_providers 见 module.yaml）。

## 8 参考实现/Oracle

- 论文谱系: SEXTractor 族 peaker（峰值检出）与自研信赖域 LM（`nls_lm`，
  `sdet_api.cpp:28` 注释）；行为等价由既有测试与 testdata 基线锁定。
- 本文档即参考实现锚；数值 oracle 设计见 §11.4（TEST-STAR-DESIGN-001）。

## 9 容差来源

- 亚像素质心: 一阶导/零交叉为连续估计（无 0.5px 网格量化损失），**椭圆高斯**中心
  由自研信赖域 LM（nls_lm；XTOL/GTOL/FTOL 编译期常量）收敛；合成高斯场 oracle 容差于 §11.4
  冻结，取值一律按本节。检测侧高斯 / PSF 侧 Moffat4 双母函数语义与换算见 §2 与
  DISP-STAR-007。
- FP32 通道经 uint16 量化（DISP-STAR-001），其容差与 FP64 通道分别冻结。

## 10 关联 ARC/API/TST

- ARC: ARCH-001（Phase1 模块链边界）。
- API: API-STAR-001（PUBLIC_API，sdet_* 6 导出符号：sdet_create/sdet_destroy/
  sdet_detect_ex/sdet_detect_ex_f64/sdet_free_detect_ex/sdet_detect_guided_ex_f64，
  与 star_detector.h 现行 SDET_EXPORT 面一致；4 个旧 CC 路径符号
  sdet_detect/sdet_free_coords/sdet_detect_debug/sdet_free_debug_maps 已随重写
  注销为死符号）+ 常量；编排合同
  PHASE1_API_V1 §2 表行 `sdet_create/destroy/detect/detect_ex`（handle 级
  并发合同）。
- TST: TEST-STAR-DESIGN-001（§11.4 冻结测试设计）；可执行 TEST-P1-STAR-001
  由 P1-STAR-TEST 落地。
- DATA: DATA-P1-STAR（DATA_SEMANTICS §17，star_det v1 [N,6] 权威块十数组语义）。

## 11 冻结附录（SRC-STAR-001 源码实测）

### 11.1 ALG-STARDET-001 逐符号锚

| 符号 | 锚 | 角色 |
|---|---|---|
| sdet_detect_impl<T> | sdet_api.cpp:1755-2153 | 生产核心（float/double 双实例调用点 :2456/:2475） |
| YvV 平滑 σ=2.0 | sdet_api.cpp:1623-1628 | 阶段2 **调用面**（sdet_gaussian_blur_yvv/_d；**实现不在本文件**：sdet_image.cpp:176-186） |
| sdet_compute_bgnoise | :725-777 | 阶段3 行差分 FnNoise1 族 |
| threshold=median+5·bgnoise | :1655 | 阶段3 全局阈值（locthreshold :1654） |
| peaker 主扫描 | :1817-2056 | 阶段4 七步（§2 候选公式锚） |
| 候选 mag_est 降序（impl 无截断） | :2063-2065 | 阶段5 排序闸门 |
| sdet_gauss_fit | :779-848 | 阶段6 采样/饱和 mask/bkg0/初始值（检测侧母函数=椭圆高斯，DISP-STAR-007） |
| sdet_lm_fit（自研 TR-LM 7 参） | :507-724 | 阶段6 拟合主体（halfA :535） |
| reject_star | :319-341 | 阶段8 质量门（SfError 码 :297-318） |
| StarRecord 构建+mag | :2086-2136 | 阶段8（is_saturated :2105、mag :2116-2134） |
| sdet_dedup_stars | :1072-1099 | 阶段10a（语义见 §2） |
| sdet_sort_stars | :1100-1163 | 阶段10b（mag 升序 NaN 末尾） |
| 输出构造 10 数组 | :1663-1749 | `sdet_emit_records`：malloc+赋值+extras（:1679-1746） |
| sdet_detect_ex（FP32 入口） | :2442-2460 | uint16→float 转换后 impl<float>（:2456） |
| sdet_detect_ex_f64（FP64 入口） | :2467-2479 | impl<double> 全程双精度（:2475） |
| sdet_create / sdet_destroy | :1110-1160 | 句柄生命周期（默认参数 :1118-1134） |
| edge_walking_center | :856-904 | 独立饱和中心实现（**现仅由下一行消费**，见 §10） |
| sdet_detect_saturated_stars | :905-1024 | 半阈值 CC 饱和检测（**本文件内零调用者**；其 debug 入口已注销，见 §10） |
| get_extra_field / parse_extra_name | :1051-1071 / :1038-1050 | extras 列解析 |

SDetParams 9 字段（star_detector.h:14-23）生产消费面（DISP-STAR-003；实测口径，`grep params\.` 于 sdet_api.cpp）:

| 字段 | sdet_api.cpp 内的消费面 |
|---|---|
| `maxStars` | sdet_api.cpp:2150、sdet_api.cpp:2409（传入 `sdet_emit_records`）；截断段 sdet_api.cpp:1662-1673 |
| `maxAxisRatio` | :2078/:2092-2095（impl）、:2333/:2337-2340（guided）活门 |
| `fitRadius`、`fwhmClipSigma` | **本文件内零消费面**：只出现在创建默认值 :1125/:1126 与创建日志 :1141-1142（原注的「auto 半径日志推导」与「debug 入口消费」两处消费面均随重写注销） |
| `structureLayers`、`hotPixelFilterRadius`、`iterativeClipSigma`、`iterativeMaxRounds`、`medianFilterDetail` | **本文件内零消费面**：仅创建默认值 :1119-1123 |

⇒ 生产 impl 在本文件内实际消费 **2/9** 字段（`maxStars`、`maxAxisRatio`）。
**悬空引用登记**：原注点名的 `sdet_detector.cpp` 与 `sdet_get_structure_map` 在
`git ls-files` 与 `lib/` 全库均 **0 命中**（该文件从未入库），属悬空引用。

### 11.2 状态码/返回码语义

- 入口级: 0=成功（含 0 星）；−1=参数无效/内存分配失败
  （:1749/:2409-2414/:2914/:2939）。
- 拟合级 SDET_FIT_* 四码全列: OK/INVALID_PARAMS/NO_CONVERGENCE/ITERATION_LIMIT（:66-69/:621-632）；
  reject_star SfError 六码 SF_OK/SF_FWHM_NEG/SF_FWHM_TOO_SMALL/
  SF_ROUNDNESS_BELOW_CRIT/SF_RMSE_TOO_LARGE/SF_FWHM_TOO_LARGE（:198-205）。
- 编排级: 检测失败或 0 星 → 退出码 STAR_DETECT_FAILED
  （orchestrator.cpp:2200-2212）；star_det 权威块写入失败 → BLOCK_MISSING
  （:2247-2253）。
- 线程安全: handle 级互斥使用（PHASE1_API_V1 §2 表行登记 handle 级 no/no）；
  无内部锁，句柄共享面 = 单线程。

### 11.3 现状缺陷清单（DISP-STAR-001..007，登记不改码，整改归 P1-STAR-IMPL/INT）

- DISP-STAR-001 FP32 通道 uint16 量化: orchestrator.cpp:2188-2198 float clamp
  到 [0,65535] 转 uint16 后进 sdet_detect_ex；PREC-105 同族精度约束；FP64
  通道（sdet_detect_ex_f64）不降级。
- DISP-STAR-002 全局单阈值无局部背景自适应: median+5·bgnoise 全局阈值
  （:1790）对渐变背景/星云场漏检低对比星；结构图局部背景路径不在生产
  impl（仅 :992/:1281 入口保留）。**适用域判据（生产盲检测 `sdet_detect_ex_f64`，
  真实星云帧）**：逐检出星取 11×11 邻域局部背景 `bg3`，统计落在帧中位 ±2σ_n
  内的颗数——该子样本为空集 ⇒ 全局阈不构成该域帧的检出下限。
  ⇒ 全局阈的**适用域 = 背景在检出尺度上空间平坦的帧**；星云/银道面场中检出集由
  11×11 局部极大 + 3×3 邻域 + 对称性门（`dA/dSr/dSc ≤ 2` 且
  `max(|Ar|,|Ac|) ≥ 5·bgnoise`，:2072-2084）决定，判据**必须**按此域分开声明。
  **负对照**：平坦背景合成场上同一统计量的取值必须非空（正例），该子样本为空集
  ⇒ 全局阈不构成星云/银道面域的检出下限；两域**必须**分开声明。读数正本 =
  星检测实验单元 results。
- DISP-STAR-003 SDetParams 9 字段生产消费面缺口（§11.1 表后注）: 编排
  platesolve.* 传参（orchestrator.cpp:1591-1607）部分字段无效；fwhmClipSigma
  生产路径半失效。
- DISP-STAR-004 饱和星 mag 与正常星 mag 量纲不一致（振幅 vs box 流量，
  :2293 `rec.flux = (float)fit_results[i].A` vs :2337 `rec.mag = (box_sum > 0.0) ? -2.5f*log10f(box_sum) : NAN`（段 :2316-2337））；has_saturated 恒等于 is_saturated（:2342），列语义
  未分化（star_det v1 [5] 列承接归 P1-STAR-INT）。
- DISP-STAR-005 检测为单实现路径（O1–O16）；`sdet_detect`/`sdet_detect_debug` 不作为入口。
- DISP-STAR-007 检测/PSF 双母函数（列语义不可互换）：检测侧生产内核为椭圆高斯
  （`sdet_gaussian_f/df`，`fwhm=2.3548·sx`），PSF 侧为椭圆 Moffat4
  （`MOFFAT4_FWHM_FACTOR=1.230310`）；同 sx 下 FWHM 报值相差 **1.9140×**，
  解析流量比 0.902（R-3 §2.2/§2.4 实测）。因此 `star_det` 的 fwhm/flux 列与
  PSF 块同名列**只在各自块内可比**；整改（如统一母函数或列改名）归
  P1-STAR-IMPL/P1-PSF-IMPL。
- 线程数未接 ThreadBudget（#pragma omp 无 num_threads 注入，
  threading_model=host_executor_lease 为合同值，接线归 P1-STAR-IMPL）；
  取消检查点缺失（无 cancel 回调，长帧检测不可中断）并入本条整改域。

### 11.4 TEST-STAR-DESIGN-001 冻结测试设计（可执行 TEST-P1-STAR-001 由 P1-STAR-TEST 落地）

- F1 合成高斯星场（已知中心/流量/FWHM/SNR）: 亚像素质心 |Δc|≤0.3 px
  （SNR≥20）；FWHM 相对误差 ≤10%；纯噪声场虚警 ≤0.1/千像素（发布门 = `G-P1-STAR-FP`，阈值与本条一致）
  （专项=completeness/false positive synthetic fields）。
  **完整性（召回）门按 PSF 宽度分档冻结**，判据 = 逐档 99% 召回阈表（本仓实测，
  生产 `sdet_detect_ex_f64`，默认参数，256² 单星居中，**峰值对齐像素中心**，
  **1000 次/档**；`SNR_peak = A_fit/σ_bg`，σ_bg 为未平滑原图行差分背景噪声 RMS）：

  | `σ_psf` (px) | 1.0 | 1.27 | 1.5 | 2.0 | 2.5 | 3.0 |
  |---|---|---|---|---|---|---|
  | FWHM (px) | 2.35 | 2.99 | 3.53 | 4.71 | 5.89 | 7.06 |
  | 50% 过渡点 `SNR_peak` | 31.1 | 21.5 | 17.7 | 14.1 | 9.3 | 8.3 |
  | **99% 召回阈 `SNR_peak`** | **∈[52, 65]** ⚠ | **24.0** | **20.0** | **16.0** | **12.0** | **10.0** |

  **两行的定义差别（不可互换）**：50% 过渡点 = 检出概率恰为 0.5 的信噪比；
  99% 召回阈 = 使实测召回在 **1000 次/档**下达到 **1000/1000** 的最小信噪比格点。
  **召回域由 99% 行定义**，50% 行只定位过渡带中心。
  **样本量**：零失败下 Clopper-Pearson 95% 下界 = `0.05^(1/n)`，要 ≥0.99 须
  `n ≥ ln0.05/ln0.99 = 298.07`；`n = 64` 零失败只给 0.954 下界，**不足以确立
  ≥99%**，其标定值系统性偏乐观（`σ_psf` = 1.5 / 2.5 px 各低 1 个信噪比单位，
  `σ_psf` = 1.0 px 低 8 个）。本表取 `n = 1000`（零失败下界 0.99701）。
  **⚠ `σ_psf = 1.0 px` 档不给点值，只给区间 [52, 65]**：1000 次/档复测确认其
  零失败性质**非单调** —— 零失败点 65 / 66 / 71 / 74 / 76 之间夹有 999/1000、
  998/1000、997/1000，故「54.0」只是 300 次/档下首个恰好零失败的格点、**不是
  稳定阈**，已撤销。区间下端 52 = 使实测召回自该点起恒 ≥0.99 的最小信噪比
  （`SNR_peak ∈ [52, 76]` 上实测 0.9940–1.0000）；上端 65 = 首次出现零失败的格点。
  **引用该档阈必须同时报出区间**，**该档只用区间标定**。
  其余五档在 `n = 1000` 下自阈起连续 ≥4 个格点零失败，点值稳定。
  档间 `σ_psf` 取相邻两档中较严者（较大阈）；`σ_psf` 超出 `[1.0, 3.0] px`
  未标定，**本表的引用域 = `[1.0, 3.0] px`**。
  **判据可复现性（单 seed 稳定性）**：以上表为域界，对 F1 召回场换 24 个噪声 seed
  重跑，域内召回逐个 seed 均为 100%，**判据判决翻转率 0/24**；以 64 次/档旧标定
  为域界时翻转率 3/16 = 18.75% ⇒ 样本量是该判据可复现的必要条件。

  **单参数闭式（趋势模型，非验收判据）**：`SNR_peak ≥ κ₉₉·(1 + σ_smooth²/σ_psf²)`，
  `σ_smooth = 2.0 px`（:1772-1774 常量）。**单参数 κ 在六档上不成立**：含
  `σ_psf = 1.0 px`（取区间下端）时 `κ₉₉ = 7.79 ± 1.34`，逐档残差
  −25.1% / +12.9% / +8.2% / −2.6% / +6.5% / +12.5%（最大 25.1%）；取区间上端时
  `κ₉₉ = 8.22 ± 2.37`（最大 36.7%）。**剔除该档后 `κ₉₉ = 7.27 ± 0.45`**
  （`σ_psf ∈ [1.27, 3.0] px` 五档），残差
  +5.4% / +0.9% / −9.2% / −0.7% / +5.0%（**最大 9.2%**）。幂次形式
  `a·(1 + σ_smooth²/σ_psf²)^p` 在六档上最大残差仍 >10%。
  **单参数闭式不足以定义召回域**，验收一律用上表；闭式仅作档间趋势判断，
  内插结果的下界 = 相邻两档的较严者。50% 行的单参数式为
  `κ₅₀ = 6.21 ± 0.50`（六档，逐档残差 ≤12.4%）。

  **适用域**：本表在 `σ_psf ∈ [1.0, 3.0] px` 且峰值对齐像素中心时成立。
  `σ_psf = 1.0 px` 档是**唯一不服从单参数标度**的档（`κ₉₉` = 10.4–13.0 vs
  其余五档 6.9–8.0）：该档的 `σ_smooth = 2.0 px` 比其点扩散尺度**还宽**，检出由
  平滑主导、不由点扩散主导 ⇒ **它本来就是另一个物理区制**，不服从单参数标度是
  应有的，不是标定错误。该档 1000 次/档实测：检出概率在 `SNR_peak ∈ [46, 76]` 上
  在 0.9850–1.0000 之间**非单调**波动，无稳定零失败点 ⇒ 该档按**区间标定**，
  **必须**用区间标定（闭式内插只作趋势判断），且引用时必须同时报出区间。
  `σ_psf < 0.8 px` 时离散采样使过渡区进一步展宽（实测 50% 点 90 vs
  连续式给 36.3），**不适用**。亚像素相位未标定；随机相位输入的引用面 = 先标定后引用。
  **判据非退化要求（AGENTS.md §5）**：F1 的召回场**必须**包含落在过渡带内的真星
  ——过渡带 = `10 ≤ SNR_peak < 99% 召回阈(σ_psf)`——且**必须**同时报告过渡带
  负例的召回率，该召回率**必须**低于 99%（证明召回度量非恒真）。过渡带真星
  **必须**在**每一档**都有域内真星（否则该档的域内召回不被行使）；过渡带负例
  **必须**存在，总数 ≥8 且覆盖 ≥4 个 σ 档（`σ_psf = 3.0 px` 档的阈 10.0 与域下限
  重合，其过渡带结构性为空）。仅当 `snr10` 档真星数与 `snr20` 档不同（即场中
  存在 `10 ≤ SNR_peak < 20` 的真星）时，该门才算行使了其声明域；在
  `σ_psf = 1.0 px`、`SNR_peak = 10` 处召回实测 0%。
- F2 饱和/混合/边缘专项: 平台≥3px 饱和星检出且 saturated=1；饱和+正常星
  d<2px 重叠 → 保留饱和星（dedup 语义）；边界 2px 内允许丢弃（§4 边界语义）。
- F3 确定性: 同输入线程数 1/2/4 输出 bitwise 一致（§5）；mag 升序+NaN 末尾
  全序断言；maxStars 截断保留最亮（:2383-2385）。
- F4 FP64 通道 oracle: 独立 Moffat4/Gaussian 复算（B,A,x0,y0,sx,sy,theta）
  |Δ中心|≤0.05 px、A/B 相对误差 ≤1e−3（生产 nls_lm 对独立实现，双精度）；FP32 通道
  经 uint16 量化容差独立冻结 |Δc|≤0.5 px（DISP-STAR-001）—— 本项**只覆盖
  FP32→uint16 量化通道**（实测 u16 量化对质心贡献 median 0.0018 / p95 0.0036 /
  max 0.0056 px，余量约 90×，可达且未超标；R-3 §2.6），**不构成端到端位置门**；
  端到端绝对位置门 = **G-P1-CENTROID-1**（ctest `p1psf_centroid_gate`/
  `p1psf_centroid_gate_neg`，判据见 `docs/science/algorithms/GATES_AND_TOLERANCES.md`）。
- F5 状态码负例: NULL/空图/0 尺寸 → −1；空场 → count=0 且 rc=0。
- F6 回归锚: Galaxy_Center 类饱和平台场多检/漏检回归 fixture（源码教训
  :1854/:2217-2218 固化）。容差冻结: 上述数值在 TEST 落地时逐项写死，
  P1-STAR-TEST 取值一律按本节；fixture 生成器注记容差来源（本节）。

### 11.5 SCI-P1-STAR-001 状态声明

科学专项（matrix P1-STAR 行）映射：subpixel centroid=§2 一阶导/零交叉/椭圆高斯
中心（连续估计，无网格量化）；completeness/false positive synthetic fields=
§11.4 F1（检测完备性/虚警由合成场验收，非解析保证；召回域按 σ_psf 分档，
  由逐档 99% 召回阈表定义，非单一 5σ 语义）；saturation/
blend/edge=§2 饱和双条件+edge-walking+dedup 保饱和+§4 边界丢弃；deterministic
ordering=§5 全序确定（mag 升序+NaN 末尾+串行 dedup/sort）。共享 SCI（PSF/
PHOTOMETRY/ASTROMETRY）不因本附录改动；本节是唯一冻结依据（编排层词汇只作对齐对象）
（descriptor astrocs.phase1.star-psf 由 P1-PSF-INT 对齐，不作冻结依据）。

> 本域门与容差的量测域/统计量/SNR 定义/阈值来源见 `docs/science/algorithms/GATES_AND_TOLERANCES.md`（F-2 冻结门表；门值来源只此一表）。

## 参考文献与参考代码库（含许可证）


- 阈值检测/去混叠：Bertin & Arnouts 1996, A&AS 117, 393（SExtractor）；源码 SExtractor（GPL-3.0，tag 2.8.6）scan.c（扫描/阈值）/extract.c（提取）/back.c（背景）/photom.c（测光）。
- 质心估计：Stetson 1987, PASP 99, 191（DAOPHOT）；photutils（BSD-3-Clause）centroid_sources。
- 椭圆高斯 LM：Levenberg 1944, Quart. Appl. Math. 2, 164；Marquardt 1963, SIAM J. Appl. Math. 11, 431；Moré 1978, LNM 630, 105；实现对照 GSL（GPL-3.0）。
- IIR 递归高斯平滑：Young & van Vliet 1995, Signal Processing 44, 139。
- SNR_peak/门：docs/science/algorithms/GATES_AND_TOLERANCES.md §2/§3。
- 检测侧椭圆高斯与 PSF 侧 Moffat4 的 FWHM 不可跨块比较（DISP-STAR-007；Moffat 1969, A&A 3, 455）。

参考代码库（含许可证）正本 = docs/references/SCIENTIFIC_REFERENCES.md §M。


---

## 背景 σ 估计器的现行口径与实测增益

- **两级 σ 估计并存，均在役**：主路径用冻结式稳健尺度 `1.482602218505602 · MAD`；另有**第三 σ 估计器**（`star_detector.cpp` 的稳健估计路径）作为生产可达路径保留。二者不互相替代，选用由现行配置决定。
- **第三 σ 估计器实测增益**：合成星场（seed=20260919，n=210 帧池化）实测相对冻结式 `1.482602218505602·MAD` 的 `mean|rel err|` 增益 = **+78.47%**，bootstrap 95% CI **[+76.58%, +80.40%]**（不含 0）。**忠实性交叉验证（与增益是两件事，各自独立登记）**：C++ 探针直调生产实现 `StarDetector::estimate_background`（`wrapper_phase1/star_detector.cpp:30-70`）读同一批 `.f32` 帧，Python 复刻与生产实现 **270/270 帧 σ 相对偏差 = 0.0（逐位一致）**——该 270/270 描述的是「复刻↔生产」一致性，**不是**第三估计器与冻结式的一致性。**测法**：同一批帧分别用冻结式与第三估计器估计 σ，逐帧取相对误差后池化
  `mean|rel err|`，增益 = 相对降幅；CI 由帧级 bootstrap 重采样给出。**负对照
  （可推翻条件）**：两估计器若在同一批帧上逐帧相等，增益必须为 0；实测 CI 不含 0
  ⇒ 两者是可区分的不同估计量。
- **第三 σ 估计器的定义与消费面（量纲 ADU）**：`estimate_background` 的返回值 = 2 轮 `median±3σ` 裁剪后、关于裁剪中位数的 **RMS**（`star_detector.cpp:67`），写入 `p1_sources.json:frames[].noise_sigma`，并被 noise-snr 节点读作 `cfg.sigma_sky_adu`。**它与 `bgnoise`（行差分 FnNoise1）不是同一个估计量**：同帧实测 20.7384 vs 13.8148 ADU（比值 1.5012，M42_M1_T2 Red 20251212@012404）。**测法**：同一帧分别跑
  `sdet_compute_bgnoise()`（行差分 FnNoise1，`sdet_api.cpp:450-504`）与
  `StarDetector::estimate_background`（2 轮 `median±3σ` 裁剪后 RMS），两者互不调用。
  **负对照**：两套 σ 若可互换，同帧比值必须为 1 ⇒ 引用「σ_bg」而不点名估计器的
  判据不可复核（比值读数正本 = 星检测实验单元 results）。凡写「σ_bg」的判据**必须**
  点名估计器。
- **登记纪律**：本节增益数字以「度量定义 + bootstrap CI + 可复跑探针」三者齐备即引用前提；缺项数字只作过程记录。
- **NaN fail-open 已闭合**：估计器入口逐像素 `isfinite` 归约 + 返回值检查，NaN 输入不再静默通过；负例（全 NaN patch）必须判红。

