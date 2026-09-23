# Star Detection Algorithms (ALG-STARDET-001)

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

> 上游 SCI: SCI-PSF-001（docs/science/PSF.md，FROZEN 共享引用不改动）；本域冻结层
> SCI-P1-STAR-001（§11.5，ALG 内冻结层，共享 SCI 不改动）
> 下游: DATA-P1-STAR（DATA_SEMANTICS §17）、API-STAR-001（PUBLIC_API）、
> MOD-astrocs-phase1-star（registry）
> 唯一权威生产源: lib/algorithms/star_detection/src/sdet_api.cpp（2973 行实测；源文件唯一在役副本）；合同头
> lib/algorithms/star_detection/include/star_detector.h（110 行）；禁止手抄他版。
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
- descriptor astrocs.phase1.star-psf（module_adapters.cpp:496-514）为编排层词汇，
  不含独立 star_detection descriptor；本模块合同以 ALG-STARDET-001/DATA-P1-STAR/
  API-STAR-001 为准。

## 2 离散公式

（锚=sdet_api.cpp 实测行号；公式与源码一一对应，禁止改写）

- 背景噪声（FnNoise1 行差分族，:462-518；sdet_compute_bgnoise 定义 :462）: 逐行差分 `d[y,x]=img[y,x]−img[y,x−1]` →
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
  `ρ_adj` 未标定时该估计量**不得**用作检测阈的 σ 基准。
  纯独立高斯噪声下实测无偏：`σ̂_n/σ_n = 0.99792`（8 帧，真值 6.0 ADU，
  散布 0.42%）。
- 全局检测阈值（:1783 估 `bgnoise` 于**未平滑原图**；:1790 组装；:1856-1857 判 `smooth > threshold`）:
  `threshold = median(img) + 5.0·bgnoise`，**单位 ADU**。
  **量纲声明**：阈值**作用图像**是 `σ_smooth = 2.0 px` 的 YvV 平滑图（:1772-1774），
  而 `bgnoise` 取自未平滑原图 ⇒ 阈在平滑图噪声单位下 = `5.0/‖k‖₂ = 35.45 σ_smooth`
  （连续 2D 高斯核 `‖k‖₂ = 1/(2σ_smooth√π) = 0.14105`）。
  **`threshold_sigma` 是未平滑原图噪声的倍数，不是阈值实际作用图像上的显著性**；
  引用「5σ」时**必须**声明 σ 属于哪幅图。等价地，检出**过渡点**在
  `SNR_peak = A_fit/σ_bg` 单位下是 `SNR_peak = 5·(1 + σ_smooth²/σ_psf²)`（连续极限），
  实测系数为 `6.22 ± 0.50`（**50% 过渡点**，§11.4 F1）。该式给出的是检出概率 0.5
  的位置，**不是召回域下限**；召回域下限 = §11.4 F1 的逐档 **99% 召回阈表**
  （σ_psf = 1.0 / 1.27 / 1.5 / 2.0 / 2.5 / 3.0 px 对应 46.0 / 24.0 / 19.0 / 16.0 /
  10.0 / 10.0），两者**不可互换**。
- 动态范围与饱和水平（:1834-1838）: `bg=median(img)`，`maxi=max(img)`，
  `dynrange=min(maxi,65535)−bg`，`minsatlevel=0.7·dynrange`，
  `satrange=0.1·dynrange`，`locthreshold=5·bgnoise`；`norm = 65535.0f` 字面量
  （:1834，DISP-STAR-006）。**量纲 ADU；适用域 = 以 uint16 整数 ADU 读出、
  满阱=65535 ADU 的帧**。对已定标 float 帧，`dynrange` 的物理含义退化为
  「帧自身 max 与 65535 的较小者减帧中位」，**不等于探测器饱和电平**；
  该域外 `saturated` 列**必须**按 Project-defined 判据
  `A_fit > min(max(img),65535) − median(img)` 消费，**不得**读作探测器饱和真值。
  实测（M42_M1_T2 Red 20251212@012404 校准帧，4096²，float32，max=80789.7 ADU）：
  `dynrange=65339.0 ADU`，`saturated=1` 的检出星 94/2474，其中 53 颗 3×3 峰值
  ≤ 65535 ADU。**测法**：生产盲检测输出逐星取 3×3 峰值与 `saturated` 列。
  **负对照**：上述 53 颗「3×3 峰值 ≤ 65535 却被标 `saturated`」即 `norm = 65535`
  不成立的反例 ⇒ 该列在 float 定标帧上**必须**按 Project-defined 判据消费。
- 候选饱和判定（:1768-1773）: 3×3 邻域超阈值像素 `meanhigh`、`minhigh`；
  饱和 ⇔ `meanhigh−bg ≥ minsatlevel` 且 `pixel0−minhigh ≤ satrange`（双条件）。
- 非饱和亚像素质心（一阶导数，:1774-1783）: `r0 = −0.5 − d1rl/(d1rr−d1rl)`、
  `c0 = −0.5 − d1cu/(d1cd−d1cu)`（分母 |·|<1e−20 时取 −0.5）。
- 饱和中心 edge-walking（:1785-1812；独立实现 edge_walking_center :627-674）:
  从候选中心四方向走到 `pixel ≤ sat` 边缘，饱和中心=边缘盒几何中心；
  距边界 <2px 丢弃。
- 二阶导数零交叉宽度/振幅（:1856-1918）: 对平滑图 smooth 逐方向找二阶导零交叉
  `Sr,Sc`（平滑图 σ 估计）与 `Ar,Ac`；平滑 PSF 估计量
  `Sr=(−srl+srr)/2`、`Ar=(Arl+Arr)/2`、`Sc=(−scu+scd)/2`、`Ac=(Acu+Acd)/2`；
  `SQRT_EXP1=√e`（:1674）。
- 拟合盒半径（:1920-1931）: `s_factor=√(−2·ln 0.001)=3.7172`（:1678）；
  `R=max(ceil(3.7172·Sr), ceil(3.7172·Sc), r)`，钳位 `R≤200`（MAX_BOX_RADIUS），
  边界收缩 `R=max(R,1)` 且不出帧。
- 对称性质量门（:1933-1945）: `dA=max(|Ar|,|Ac|)/min(|Ar|,|Ac|)`、`dSr`、`dSc`
  同构（分母 <1e−20 时 1e30）；拒绝条件 `dA>2 ∨ dSr>2 ∨ dSc>2 ∨
  max(|Ar|,|Ac|)<locthreshold`。
- candidate 去重（:1947-1959）: `matchradius=max(1, floor(0.2·R))`；
  曼哈顿距 `|Δx|+|Δy|≤matchradius` 视为重复，先到先留（扫描序 y 主序）。
- **椭圆高斯拟合模型（检测侧冻结母函数）**（sdet_gauss_fit :520-711 → sdet_lm_fit :285-460，
  GSL trust-region LM，7 参数）:
  `f(x,y)=B+A·exp(−(x′²/SX+(y′/r)²/SX)/1)`，参数 `{B,A,x0,y0,SX=2σ²,fr,alpha}`，
  `r=0.5·(cos fr+1)`，`sx=√(SX/2)`，`sy=sx·r`，`fwhm=2.3548·σ`（TWO_SQRT_2_LOG2），
  PSF 侧 = 椭圆 Moffat4（SCI-PSF-001 §5）；同 sx 下 FWHM 相差 1.9140×，禁止跨块比较（DISP-STAR-007）。
  `theta=−alpha` 归一到 (−90°,90°]。残差坐标 `dx=x+0.5−cx`（像素中心=索引+0.5，
  :510-513）。
- 初始化（:289-346）: halfA 边界搜索（`max_val=A0+B0`，沿中心行列向外走到
  `val−bkg0 ≤ A0/2`）→ `FWHMx=jj1−jj2`、`FWHMy=ii1−ii2`；`SX_init=FWHM²/4ln2`、
  `fr_init=acos(2·roundness−1)`、`alpha_init=0`；`bkg0`=拟合窗内下半像素截尾 MAD
  clip 后中位（:530-567）；`max_iter=LM_MAX_ITER_ANGLE·(饱和?3:1)`。
- 饱和像素 mask（:505-506）: 拟合采样排除 `val ≥ sat_threshold`；
  排除后 m≤8 → INVALID_PARAMS（:520-525）。
- 星等（正常星，:2171-2198）: `mag = −2.5·log10(Σ_box(pixel − B_fit))`，box=
  (2R+1)² 用候选 R（钳 [5,200]）与候选中心；`box_sum≤0 → mag=NaN`。
  饱和星（:2175）: `mag = −2.5·log10(A_fit)`（A>0），失败=NaN
  （量纲差异登记 DISP-STAR-003）。
- 饱和标志（:2159）: `is_saturated = (A_fit > dynrange)`；`has_saturated =
  is_saturated`（:2203，DISP-STAR-004）。
- 输出排序（sdet_sort_stars :983-995）: mag 升序 stable_sort，NaN 恒排末尾；
  dedup（sdet_dedup_stars :864-982）: 饱和星与正常星 2px 网格 d²<4.0 →
  丢正常星保饱和星；饱和星间 d²<4.0 保 r 大者；正常星间 d²≤1.0 保先者；
  最后 `maxStars>0` 截断（:2240-2242）。

## 3 伪代码

```
sdet_detect_impl(image, w, h, params):            # sdet_api.cpp:1749-2499
  smooth   = GaussianBlur_YvV(image, sigma=2.0)   # :1623-1633（Young-van Vliet IIR）
  bgnoise  = FnNoise1(image)                      # :462-518 行差分+3×5σ clip
  median   = robust_median(image)
  thr      = median + 5·bgnoise                   # :1782-1792
  dynrange = min(max(image), 65535) − median      # :1835-1836
  for y,x in [r..h−r)×[r..w−r):                   # 阶段4 peaker :1709-1974
    if smooth[y,x] ≤ thr: continue                # :1711-1712
    if not local_max_11x11(smooth,y,x): x+=5; continue   # :1715-1734 平局决胜取左上唯一
    (meanhigh, minhigh) = stat3x3(image, x, y, thr)      # :1736-1756 原图 3×3
    if boundary(xx±2,yy±2) 越界: continue         # :1758-1760
    if meanhigh−bg < 0.7·dynrange or pixel0−minhigh > 0.1·dynrange:
        (r0,c0) = first_order_centroid(smooth,xx,yy)     # :1774-1783 非饱和
    else:  sat=1; (r0,c0)=edge_walking(smooth,xx,yy,sat_level)  # :1784-1812 饱和
    (Sr,Sc,Ar,Ac) = zero_cross_2nd(smooth,xx,yy,sat)     # :1856-1918
    R = max(ceil(3.7172·Sr), ceil(3.7172·Sc), 5); clamp R ≤ 200, 帧内收缩  # :1920-1931
    if dA>2 or dSr>2 or dSc>2 or max(|Ar|,|Ac|)<5·bgnoise: continue  # :1933-1945
    if exists prior candidate with |Δx|+|Δy| ≤ max(1,⌊0.2R⌋): continue  # :1947-1959
    candidates.push({x+xx偏移, y, mag_est=meanhigh, Sr, Sc, R, sat_flag})
  sort candidates by mag_est desc; 截断 maxStars×2       # :2026-2034
  parallel for c in candidates:                   # 阶段6 :2036-2068 OpenMP dynamic
    fit = GSL_TR_LM_GAUSS(image, window=2R+1, sat_mask=c.sat, init=halfA+局部截尾MAD)
  for (fit, c) in zip(fit_results, candidates):   # 阶段8 :2130-2205
    if fit.status ≠ OK: drop                      # :2139
    if max(sx,sy)/min(sx,sy) > maxAxisRatio: drop # :2143-2145
    if reject_star(fit) ≠ OK: drop                # :2148-2150（FWHM>0.5、圆度≥0.5、
                                                  #   RMSE=mad·1.482602218505602/A≤0.2、FWHM 上限;
                                                  #   饱和豁免 RMSE :189-239）
    mag = −2.5·log10(Σ_box(pixel − B_fit))（box=R, 中心=候选中心）  # :2177-2198
    saturated = (A_fit > dynrange); has_saturated = saturated       # :2159/:2203
  dedup(stars); sort_by_mag_asc(stars, NaN last); 截断 maxStars   # 阶段10 :2224-2242
  return 10×malloc 数组 (cx,cy,flux,mag,saturated,has_saturated[,extras])  # :2244-2313
```

## 4 边界/NaN/Inf

- 候选为空（:1059-1064/1246-1251/2252-2263）: 全输出指针置 NULL，`*out_count=0`，
  返回 0（不是错误；"未检测到星"由编排层判定，orchestrator.cpp:2200-2212
  det_count≤0 → STAR_DETECT_FAILED）。
- 参数无效（:1612）: handle/image/输出指针 NULL → −1；width/height ≤0 → −1
  （入口 :2325/:2340）。
- 拟合失败（:425-429/:597-609）: GSL≠SUCCESS 或非有限值或 A≤0 →
  NO_CONVERGENCE，该候选丢弃（:2139），不产出 NaN 行。
- mag NaN: 正常星 box_sum≤0（:2198）；NaN 排序恒在末尾（:947-953）。
- 饱和星拟合失败: flux/fwhm/sx/sy/theta/background/amplitude 全 0.0f 哨兵 +
  mag=NaN（:1519-1531 debug 路径语义；extras 读取端 get_extra_field :804-819）。
- 边界: peaker 扫描域 [r,h−r)×[r,w−r)（:1709-1710）；边界检查 xx±2/yy±2
  （:1758-1760）；R 收缩不出帧（:1926-1931）；edge-walking 距边界 <2px 丢弃
  （:634-636）。

## 5 确定性与归约

- OpenMP 仅三处并行: 平滑/转化 `parallel for schedule(static)`（入口转换
  :2321-2325/:2336-2340、bgnoise 行并行 :448）、候选拟合 `omp parallel +
  omp for schedule(dynamic) reduction(+:fit_ok_count)`（:2042-2044）。
- 拟合逐候选独立、结果按索引写回；dedup/sort/maxStars 截断串行
  （:2224-2242）→ 输出 bitwise 与线程数无关（1/N 等价）。
- 扫描序固定（y 主序、x 递增）+ 先到先留去重 + stable_sort → 全序确定。
- determinism=fixed_reduction_order（module.yaml 合同值）。

## 6 时间/空间复杂度

- 平滑 O(N)（YvV 递归 IIR）；bgnoise O(N·clip)；peaker O(N·r²)（r=5，11×11 窗，
  命中后 x+=5 跳步 :1734）；拟合 O(C·m·iter)（C=候选数，m=(2R+1)² 采样数）。
- 空间: smooth/map O(N) + 候选/拟合 O(C) + 输出 O(count)。
- maxStars 双闸门: 候选阶段截断 maxStars×2（:2028-2034），输出阶段截断
  maxStars（:2240-2242）。

## 7 CPU-only 后端策略（V5）

- 纯 CPU；GSL gsl_multifit_nlinear trust-region LM（trs=gsl_multifit_nlinear_trs_lm，
  :348-352）；OpenMP 线程级并行；无 SIMD 内联/ISA 分派（-march=native 于
  Makefile:22，非代码分支）；V7 迁移目标 astrocs_p1_star_detection.dll
  （provider/cpu_providers 见 module.yaml）。

## 8 参考实现/Oracle

- 历史参考: star_finder.c（SEXtractor 族 peaker，源码注释逐段对齐 :1653-1660、
  :1715、:1736、:1762、:1914-1947）；IPv 手写 LM 已被 GSL trust-region LM 取代
  （:583-585 注释）。
- 本文档即参考实现锚；数值 oracle 设计见 §11.4（TEST-STAR-DESIGN-001）。

## 9 容差来源

- 亚像素质心: 一阶导/零交叉为连续估计（无 0.5px 网格量化损失），**椭圆高斯**中心
  由 GSL LM（XTOL/GTOL/FTOL 编译期常量）收敛；合成高斯场 oracle 容差于 §11.4
  冻结，禁止放宽。检测侧高斯 / PSF 侧 Moffat4 双母函数语义与换算见 §2 与
  DISP-STAR-007。
- FP32 通道经 uint16 量化（DISP-STAR-001），其容差与 FP64 通道分别冻结。

## 10 关联 ARC/API/TST

- ARC: ARCH-001（Phase1 模块链边界）。
- API: API-STAR-001（PUBLIC_API，sdet_* 9 导出符号 + 常量）；编排合同
  PHASE1_API_V1 §2 表行 `sdet_create/destroy/detect/detect_ex`（handle 级
  并发合同）。
- TST: TEST-STAR-DESIGN-001（§11.4 冻结测试设计）；可执行 TEST-P1-STAR-001
  由 P1-STAR-TEST 落地。
- DATA: DATA-P1-STAR（DATA_SEMANTICS §17，star_det v1 [N,6] 权威块十数组语义）。

## 11 冻结附录（SRC-STAR-001 源码实测）

### 11.1 ALG-STARDET-001 逐符号锚

| 符号 | 锚 | 角色 |
|---|---|---|
| sdet_detect_impl<T> | sdet_api.cpp:1749-2499 | 生产核心（float/double 双实例调用点 :2514/:2533） |
| YvV 平滑 σ=2.0 | :1623-1633 | 阶段2（sdet_gaussian_blur_yvv/_d） |
| sdet_compute_bgnoise | :462-518 | 阶段3 行差分 FnNoise1 族 |
| threshold=median+5·bgnoise | :1782-1792 | 阶段3 全局阈值 |
| peaker 主扫描 | :1709-1974 | 阶段4 七步（§2 候选公式锚） |
| 候选 mag_est 降序+截断 | :2026-2034 | 阶段5 排序闸门 |
| sdet_gauss_fit | :520-711 | 阶段6 采样/饱和 mask/bkg0/初始值（检测侧母函数=椭圆高斯，DISP-STAR-007） |
| sdet_lm_fit（GSL TR-LM 7 参） | :285-460 | 阶段6 拟合主体（halfA :315） |
| reject_star | :189-239 | 阶段8 质量门（SfError 五码 :177-186） |
| StarRecord 构建+mag | :2130-2205 | 阶段8（is_saturated :2159、mag :2177-2198） |
| sdet_dedup_stars | :864-982 | 阶段10a（语义见 §2） |
| sdet_sort_stars | :983-995 | 阶段10b（mag 升序 NaN 末尾） |
| 输出构造 10 数组 | :2244-2313 | malloc+赋值+extras（:2301-2311） |
| sdet_detect_ex（FP32 入口） | :2318-2333 | uint16→float 转换后 impl<float> |
| sdet_detect_ex_f64（FP64 入口） | :2343-2355 | impl<double> 全程双精度 |
| sdet_create / sdet_destroy | :954-990 | 句柄生命周期（默认参数 :963-975） |
| sdet_detect（旧 CC 入口） | :1034-1323 | 旧结构图路径（非生产，DISP-STAR-005） |
| sdet_detect_debug | :1329 | 诊断入口（CC 路径+平滑图导出） |
| edge_walking_center | :627-674 | 独立饱和中心实现（debug 路径） |
| sdet_detect_saturated_stars | :675-775 | 半阈值 CC 饱和检测（debug 路径） |
| get_extra_field / parse_extra_name | :778-819 | extras 列解析 |

SDetParams 9 字段（star_detector.h:14-23）生产消费面（DISP-STAR-003）:
maxStars（:2028-2034/:2240-2242）、maxAxisRatio（:2143-2145）完整消费；
fitRadius 仅驱动 auto 半径日志推导（:2024-2026，阶段6 实际用 per-candidate R
:2039）；fwhmClipSigma 仅 debug 入口消费（:1450-1452，impl 阶段8 已移除全局
FWHM clip :2140）；structureLayers/hotPixelFilterRadius/iterativeClipSigma/
iterativeMaxRounds/medianFilterDetail 仅旧 sdet_get_structure_map 路径消费
（sdet_detector.cpp:14-56），生产 impl 不调用结构图。

### 11.2 状态码/返回码语义

- 入口级: 0=成功（含 0 星）；−1=参数无效/内存分配失败
  （:1612/:2264-2270/:2325/:2340）。
- 拟合级 SDET_FIT_*: OK/INVALID_PARAMS/NO_CONVERGENCE（:260-262/:425-429）；
  reject_star SfError 五码 SF_OK/SF_FWHM_NEG/SF_FWHM_TOO_SMALL/
  SF_ROUNDNESS_BELOW_CRIT/SF_RMSE_TOO_LARGE/SF_FWHM_TOO_LARGE（:177-186）。
- 编排级: 检测失败或 0 星 → 退出码 STAR_DETECT_FAILED
  （orchestrator.cpp:2200-2212）；star_det 权威块写入失败 → BLOCK_MISSING
  （:2247-2253）。
- 线程安全: handle 级互斥使用（PHASE1_API_V1 §2 表行登记 handle 级 no/no）；
  无内部锁，禁止跨线程共享句柄并发检测。

### 11.3 现状缺陷清单（DISP-STAR-001..007，登记不改码，整改归 P1-STAR-IMPL/INT）

- DISP-STAR-001 FP32 通道 uint16 量化: orchestrator.cpp:2188-2198 float clamp
  到 [0,65535] 转 uint16 后进 sdet_detect_ex；PREC-105 同族精度约束；FP64
  通道（sdet_detect_ex_f64）不降级。
- DISP-STAR-002 全局单阈值无局部背景自适应: median+5·bgnoise 全局阈值
  （:1790）对渐变背景/星云场漏检低对比星；旧结构图局部背景路径已退出生产
  impl（仅 :992/:1281 旧入口保留）。**实测量化（M42_M1_T2 Red 20251212@012404
  校准帧，4096²，生产盲检测 `sdet_detect_ex_f64`）**：阈值 = 265.06 ADU，
  2474 颗检出星的局部 3×3 背景 `p1/p50/p95` = 318.96 / 803.04 / 17983.63 ADU
  ⇒ **100% 的检出星所在像素在原图上本已高于全局阈**，该阈在该帧上不构成检出下限；
  同时 `bg3` 落在帧中位 ±2σ_n 内的检出星 **0 颗**（背景受限子样本为空）。
  ⇒ 全局阈的**适用域 = 背景在检出尺度上空间平坦的帧**；星云/银道面场中检出集由
  11×11 局部极大 + 3×3 邻域 + 对称性门（`dA/dSr/dSc ≤ 2` 且
  `max(|Ar|,|Ac|) ≥ 5·bgnoise`，:2089-2094）决定，判据**必须**按此域分开声明。
  **测法**：同一 M42 星云帧（4096²，float32 已定标）跑生产盲检测，逐检出星取
  11×11 邻域局部背景 `bg3`，统计其落在帧中位 ±2σ_n 内的颗数。**负对照**：该子样本
  为**空集**（0 颗）⇒ 全局阈不是该帧的检出下限；平坦背景合成场上同一统计量非空
  （正例）⇒ 两域**必须**分开声明。
- DISP-STAR-003 SDetParams 9 字段生产消费面缺口（§11.1 表后注）: 编排
  platesolve.* 传参（orchestrator.cpp:1591-1607）部分字段无效；fwhmClipSigma
  生产路径半失效。
- DISP-STAR-004 饱和星 mag 与正常星 mag 量纲不一致（振幅 vs box 流量，
  :2175 vs :2177-2198）；has_saturated 恒等于 is_saturated（:2203），列语义
  未分化（star_det v1 [5] 列承接归 P1-STAR-INT）。
- DISP-STAR-005 双实现并存: 生产 impl（peaker 路径）与旧 sdet_detect/
  sdet_detect_debug（CC 结构图路径 :1034-1323/:1329）行为漂移（候选过滤
  ≤4 vs peaker 七步、dedup 半径/网格不同）；维护歧义，去留归 P1-STAR-IMPL。
- DISP-STAR-007 检测/PSF 双母函数（列语义不可互换）：检测侧生产内核为椭圆高斯
  （`sdet_gaussian_f/df`，`fwhm=2.3548·sx`），PSF 侧为椭圆 Moffat4
  （`MOFFAT4_FWHM_FACTOR=1.230310`）；同 sx 下 FWHM 报值相差 **1.9140×**，
  解析流量比 0.902（R-3 §2.2/§2.4 实测）。因此 `star_det` 的 fwhm/flux 列与
  PSF 块同名列**禁止跨块比较**；整改（如统一母函数或列改名）归
  P1-STAR-IMPL/P1-PSF-IMPL。
- 线程数未接 ThreadBudget（#pragma omp 无 num_threads 注入，
  threading_model=host_executor_lease 为合同值，接线归 P1-STAR-IMPL）；
  取消检查点缺失（无 cancel 回调，长帧检测不可中断）并入本条整改域。

### 11.4 TEST-STAR-DESIGN-001 冻结测试设计（可执行 TEST-P1-STAR-001 由 P1-STAR-TEST 落地）

- F1 合成高斯星场（已知中心/流量/FWHM/SNR）: 亚像素质心 |Δc|≤0.3 px
  （SNR≥20）；FWHM 相对误差 ≤10%；纯噪声场虚警 ≤0.1/千像素
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
  **引用该档阈必须同时报出区间**，**禁止**用单参数闭式内插该档。
  其余五档在 `n = 1000` 下自阈起连续 ≥4 个格点零失败，点值稳定。
  档间 `σ_psf` 取相邻两档中较严者（较大阈）；`σ_psf` 超出 `[1.0, 3.0] px`
  未标定，**不得**引用本表。
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
  其内插结果**不得**低于相邻两档的较严者。50% 行的单参数式为
  `κ₅₀ = 6.21 ± 0.50`（六档，逐档残差 ≤12.4%）。

  **适用域**：本表在 `σ_psf ∈ [1.0, 3.0] px` 且峰值对齐像素中心时成立。
  `σ_psf = 1.0 px` 档是**唯一不服从单参数标度**的档（`κ₉₉` = 10.4–13.0 vs
  其余五档 6.9–8.0）：该档的 `σ_smooth = 2.0 px` 比其点扩散尺度**还宽**，检出由
  平滑主导、不由点扩散主导 ⇒ **它本来就是另一个物理区制**，不服从单参数标度是
  应有的，不是标定错误。该档 1000 次/档实测：检出概率在 `SNR_peak ∈ [46, 76]` 上
  在 0.9850–1.0000 之间**非单调**波动，无稳定零失败点 ⇒ 该档按**区间标定**，
  **必须**用区间、**禁止**用闭式内插，且引用时必须同时报出区间。
  `σ_psf < 0.8 px` 时离散采样使过渡区进一步展宽（实测 50% 点 90 vs
  连续式给 36.3），**不适用**。亚像素相位未标定；随机相位输入**不得**直接引用本表。
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
  全序断言；maxStars 截断保留最亮（:2240-2242）。
- F4 FP64 通道 oracle: 独立 Moffat4/Gaussian 复算（B,A,x0,y0,sx,sy,theta）
  |Δ中心|≤0.05 px、A/B 相对误差 ≤1e−3（GSL 对独立实现，双精度）；FP32 通道
  经 uint16 量化容差独立冻结 |Δc|≤0.5 px（DISP-STAR-001）—— 本项**只覆盖
  FP32→uint16 量化通道**（实测 u16 量化对质心贡献 median 0.0018 / p95 0.0036 /
  max 0.0056 px，余量约 90×，可达且未超标；R-3 §2.6），**不构成端到端位置门**；
  端到端绝对位置门 = **G-P1-CENTROID-1**（ctest `p1psf_centroid_gate`/
  `p1psf_centroid_gate_neg`，判据见 `docs/algorithms/GATES_AND_TOLERANCES.md`）。
- F5 状态码负例: NULL/空图/0 尺寸 → −1；空场 → count=0 且 rc=0。
- F6 回归锚: Galaxy_Center 类饱和平台场多检/漏检回归 fixture（源码教训
  :1854/:2217-2218 固化）。容差冻结: 上述数值在 TEST 落地时逐项写死，
  P1-STAR-TEST 不得放宽；fixture 生成器注记容差来源（本节）。

### 11.5 SCI-P1-STAR-001 状态声明

科学专项（matrix P1-STAR 行）映射：subpixel centroid=§2 一阶导/零交叉/椭圆高斯
中心（连续估计，无网格量化）；completeness/false positive synthetic fields=
§11.4 F1（检测完备性/虚警由合成场验收，非解析保证；召回域按 σ_psf 分档，
  由逐档 99% 召回阈表定义，非单一 5σ 语义）；saturation/
blend/edge=§2 饱和双条件+edge-walking+dedup 保饱和+§4 边界丢弃；deterministic
ordering=§5 全序确定（mag 升序+NaN 末尾+串行 dedup/sort）。共享 SCI（PSF/
PHOTOMETRY/ASTROMETRY）不因本附录改动；本节禁止被编排层词汇反向改写
（descriptor astrocs.phase1.star-psf 由 P1-PSF-INT 对齐，不作冻结依据）。

> 本域门与容差的量测域/统计量/SNR 定义/阈值来源见 `docs/algorithms/GATES_AND_TOLERANCES.md`（F-2 冻结门表；门不得引用表外阈值）。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- 阈值检测/去混叠：Bertin & Arnouts 1996, A&AS 117, 393（SExtractor）；源码 SExtractor（GPL-3.0）detect.c/scan.c。
- 质心估计：Stetson 1987, PASP 99, 191（DAOPHOT）；photutils（BSD-3-Clause）centroid_sources。
- 椭圆高斯 LM：Levenberg 1944, Quart. Appl. Math. 2, 164；Marquardt 1963, SIAM J. Appl. Math. 11, 431；Moré 1978, LNM 630, 105；实现对照 GSL（GPL-3.0）。
- IIR 递归高斯平滑：Young & van Vliet 1995, Signal Processing 44, 139。
- SNR_peak/门：docs/algorithms/GATES_AND_TOLERANCES.md §2/§3。
- 检测侧椭圆高斯与 PSF 侧 Moffat4 的 FWHM 不可跨块比较（DISP-STAR-007；Moffat 1969, A&A 3, 455）。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。


---

## 背景 σ 估计器的现行口径与实测增益

- **两级 σ 估计并存，均在役**：主路径用冻结式稳健尺度 `1.482602218505602 · MAD`；另有**第三 σ 估计器**（`star_detector.cpp` 的稳健估计路径）作为生产可达路径保留。二者不互相替代，选用由现行配置决定。
- **第三 σ 估计器实测增益**：合成星场（seed=20260919，n=210 帧池化）实测相对冻结式 `1.482602218505602·MAD` 的 `mean|rel err|` 增益 = **+78.47%**，bootstrap 95% CI **[+76.58%, +80.40%]**（不含 0）。**忠实性交叉验证（与增益是两件事，禁止混写）**：C++ 探针直调生产实现 `StarDetector::estimate_background`（`wrapper_phase1/star_detector.cpp:30-70`）读同一批 `.f32` 帧，Python 复刻与生产实现 **270/270 帧 σ 相对偏差 = 0.0（逐位一致）**——该 270/270 描述的是「复刻↔生产」一致性，**不是**第三估计器与冻结式的一致性。**测法**：同一批帧分别用冻结式与第三估计器估计 σ，逐帧取相对误差后池化
  `mean|rel err|`，增益 = 相对降幅；CI 由帧级 bootstrap 重采样给出。**负对照
  （可推翻条件）**：两估计器若在同一批帧上逐帧相等，增益必须为 0；实测 CI 不含 0
  ⇒ 两者是可区分的不同估计量。
- **第三 σ 估计器的定义与消费面（量纲 ADU）**：`estimate_background` 的返回值 = 2 轮 `median±3σ` 裁剪后、关于裁剪中位数的 **RMS**（`star_detector.cpp:67`），写入 `p1_sources.json:frames[].noise_sigma`，并被 noise-snr 节点读作 `cfg.sigma_sky_adu`。**它与 `bgnoise`（行差分 FnNoise1）不是同一个估计量**：同帧实测 20.7384 vs 13.8148 ADU（比值 1.5012，M42_M1_T2 Red 20251212@012404）。**测法**：同一帧分别跑
  `sdet_compute_bgnoise`（行差分 FnNoise1，`sdet_api.cpp:462-518`）与
  `StarDetector::estimate_background`（2 轮 `median±3σ` 裁剪后 RMS），两者互不调用。
  **负对照**：两套 σ 若可互换，同帧比值必须为 1；实测 1.5012 ≠ 1 ⇒ 引用「σ_bg」
  而不点名估计器的判据不可复核。凡写「σ_bg」的判据**必须**点名估计器。
- **登记纪律**：本节增益数字以「度量定义 + bootstrap CI + 可复跑探针」三者齐备为引用前提；缺任一项的增益数字不得引用。
- **NaN fail-open 已闭合**：估计器入口逐像素 `isfinite` 归约 + 返回值检查，NaN 输入不再静默通过；负例（全 NaN patch）必须判红。

