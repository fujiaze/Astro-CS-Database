# Star Detection Algorithms (ALG-STARDET-001)

> 状态: ACTIVE（P1-STAR-DOC 冻结，2026-09-07，SA-P1-S15）
> 上游 SCI: SCI-PSF-001（docs/science/PSF.md，FROZEN 共享引用不改动）；本任务冻结层
> SCI-P1-STAR-001（§11.5，ALG 内冻结层，共享 SCI 不改动）
> 下游: DATA-P1-STAR（DATA_SEMANTICS §17）、API-STAR-001（PUBLIC_API）、
> MOD-astrocs-phase1-star（registry）
> 唯一权威生产源: lib/star_detector/src/sdet_api.cpp（2373 行实测）；合同头
> lib/star_detector/include/star_detector.h（73 行）；禁止手抄他版。
> 矩阵行: docs/traceability/TRACEABILITY_MATRIX.json MOD-astrocs-phase1-star
> （matrix P1-STAR，legacy_paths=lib/star_detector;lib/phase1/stars，
> 迁移目标 astrocs_p1_star_detection.dll）。

## 1 上游 SCI 与输入输出

- SCI-PSF-001（PSF 拟合共享 SCI）：星检测为 PSF 拟合与 plate solve 提供候选/中心，
  本文档只登记检测侧算法事实，不修改共享 SCI。
- 输入: 单帧图像（生产通道 FP64 `double` 或 FP32 经 uint16 量化，`[h·w]` 行主序，ADU）
  + SDetParams 参数（star_detector.h:13-24）。
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

- 背景噪声（FnNoise1 行差分族，:440-476）: 逐行差分 `d[y,x]=img[y,x]−img[y,x−1]` →
  3 轮 5σ clip（median/MAD 迭代，MAD 含 1.4826）→ 行标准差 → 行中位 → ×0.7071
  （1/√2）。
- 全局检测阈值（:1637-1647）: `threshold = median(img) + 5.0·bgnoise`。
- 动态范围与饱和水平（:1684-1692）: `bg=median(img)`，`maxi=max(img)`，
  `dynrange=min(maxi,65535)−bg`，`minsatlevel=0.7·dynrange`，
  `satrange=0.1·dynrange`，`locthreshold=5·bgnoise`；norm 硬编码 65535
  （:1689，DISP-STAR-006）。
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
- Moffat4/Gaussian 拟合模型（sdet_lm_fit :262-437，GSL trust-region LM，7 参数）:
  `f(x,y)=B+A·exp(−(x′²/SX+(y′/r)²/SX)/1)`，参数 `{B,A,x0,y0,SX=2σ²,fr,alpha}`，
  `r=0.5·(cos fr+1)`，`sx=√(SX/2)`，`sy=sx·r`，`fwhm=2.3548·σ`（TWO_SQRT_2_LOG2），
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
- 输出排序（sdet_sort_stars :941-956）: mag 升序 stable_sort，NaN 恒排末尾；
  dedup（sdet_dedup_stars :822-939）: 饱和星与正常星 2px 网格 d²<4.0 →
  丢正常星保饱和星；饱和星间 d²<4.0 保 r 大者；正常星间 d²≤1.0 保先者；
  最后 `maxStars>0` 截断（:2240-2242）。

## 3 伪代码

```
sdet_detect_impl(image, w, h, params):            # sdet_api.cpp:1599-2353
  smooth   = GaussianBlur_YvV(image, sigma=2.0)   # :1623-1633（Young-van Vliet IIR）
  bgnoise  = FnNoise1(image)                      # :440-476 行差分+3×5σ clip
  median   = robust_median(image)
  thr      = median + 5·bgnoise                   # :1637-1647
  dynrange = min(max(image), 65535) − median      # :1684-1692
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
    fit = GSL_TR_LM_Moffat4(image, window=2R+1, sat_mask=c.sat, init=halfA+局部截尾MAD)
  for (fit, c) in zip(fit_results, candidates):   # 阶段8 :2130-2205
    if fit.status ≠ OK: drop                      # :2139
    if max(sx,sy)/min(sx,sy) > maxAxisRatio: drop # :2143-2145
    if reject_star(fit) ≠ OK: drop                # :2148-2150（FWHM>0.5、圆度≥0.5、
                                                  #   RMSE=mad·1.4826/A≤0.2、FWHM 上限;
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

- 亚像素质心: 一阶导/零交叉为连续估计（无 0.5px 网格量化损失），Moffat4 中心
  由 GSL LM（XTOL/GTOL/FTOL 编译期常量）收敛；合成高斯场 oracle 容差于 §11.4
  冻结，禁止放宽。
- FP32 通道经 uint16 量化（DISP-STAR-001），其容差与 FP64 通道分别冻结。

## 10 关联 ARC/API/TST

- ARC: ARCH-001（Phase1 模块链边界）。
- API: API-STAR-001（PUBLIC_API，sdet_* 9 导出符号 + 常量）；编排合同
  PHASE1_API_V1 §2 表行 `sdet_create/destroy/detect/detect_ex`（handle 级
  并发合同）。
- TST: TEST-STAR-DESIGN-001（§11.4 冻结测试设计）；可执行 TEST-P1-STAR-001
  由 P1-STAR-TEST 落地。
- DATA: DATA-P1-STAR（DATA_SEMANTICS §17，star_det v1 [N,6] 权威块十数组语义）。

## 11 P1-STAR-DOC 冻结附录（2026-09-07，SRC-STAR-001 源码实测）

### 11.1 ALG-STARDET-001 逐符号锚

| 符号 | 锚 | 角色 |
|---|---|---|
| sdet_detect_impl<T> | sdet_api.cpp:1599-2353 | 生产核心（float/double 双实例，:1599-1604） |
| YvV 平滑 σ=2.0 | :1623-1633 | 阶段2（sdet_gaussian_blur_yvv/_d） |
| sdet_compute_bgnoise | :440-476 | 阶段3 行差分 FnNoise1 族 |
| threshold=median+5·bgnoise | :1637-1647 | 阶段3 全局阈值 |
| peaker 主扫描 | :1709-1974 | 阶段4 七步（§2 候选公式锚） |
| 候选 mag_est 降序+截断 | :2026-2034 | 阶段5 排序闸门 |
| sdet_moffat4_fit | :483-620 | 阶段6 采样/饱和 mask/bkg0/初始值 |
| sdet_lm_fit（GSL TR-LM 7 参） | :262-437 | 阶段6 拟合主体（halfA :289-313） |
| reject_star | :189-239 | 阶段8 质量门（SfError 五码 :177-186） |
| StarRecord 构建+mag | :2130-2205 | 阶段8（is_saturated :2159、mag :2177-2198） |
| sdet_dedup_stars | :822-939 | 阶段10a（语义见 §2） |
| sdet_sort_stars | :941-956 | 阶段10b（mag 升序 NaN 末尾） |
| 输出构造 10 数组 | :2244-2313 | malloc+赋值+extras（:2301-2311） |
| sdet_detect_ex（FP32 入口） | :2318-2333 | uint16→float 转换后 impl<float> |
| sdet_detect_ex_f64（FP64 入口） | :2343-2355 | impl<double> 全程双精度 |
| sdet_create / sdet_destroy | :954-990 | 句柄生命周期（默认参数 :963-975） |
| sdet_detect（旧 CC 入口） | :992-1274 | 旧结构图路径（非生产，DISP-STAR-005） |
| sdet_detect_debug | :1281-1593 | 诊断入口（CC 路径+平滑图导出） |
| edge_walking_center | :627-674 | 独立饱和中心实现（debug 路径） |
| sdet_detect_saturated_stars | :675-775 | 半阈值 CC 饱和检测（debug 路径） |
| get_extra_field / parse_extra_name | :778-819 | extras 列解析 |

SDetParams 9 字段（star_detector.h:13-24）生产消费面（DISP-STAR-003）:
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

### 11.3 现状缺陷清单（DISP-STAR-001..005，登记不改码，整改归 P1-STAR-IMPL/INT）

- DISP-STAR-001 FP32 通道 uint16 量化: orchestrator.cpp:2188-2198 float clamp
  到 [0,65535] 转 uint16 后进 sdet_detect_ex；PREC-105 同族精度约束；FP64
  通道（sdet_detect_ex_f64）不降级。
- DISP-STAR-002 全局单阈值无局部背景自适应: median+5·bgnoise 全局阈值
  （:1645）对渐变背景/星云场漏检低对比星；旧结构图局部背景路径已退出生产
  impl（仅 :992/:1281 旧入口保留）。
- DISP-STAR-003 SDetParams 9 字段生产消费面缺口（§11.1 表后注）: 编排
  platesolve.* 传参（orchestrator.cpp:1591-1607）部分字段无效；fwhmClipSigma
  生产路径半失效。
- DISP-STAR-004 饱和星 mag 与正常星 mag 量纲不一致（振幅 vs box 流量，
  :2175 vs :2177-2198）；has_saturated 恒等于 is_saturated（:2203），列语义
  未分化（star_det v1 [5] 列承接归 P1-STAR-INT）。
- DISP-STAR-005 双实现并存: 生产 impl（peaker 路径）与旧 sdet_detect/
  sdet_detect_debug（CC 结构图路径 :992-1274/:1281-1593）行为漂移（候选过滤
  ≤4 vs peaker 七步、dedup 半径/网格不同）；维护歧义，去留归 P1-STAR-IMPL。
- 线程数未接 ThreadBudget（#pragma omp 无 num_threads 注入，
  threading_model=host_executor_lease 为合同值，接线归 P1-STAR-IMPL）；
  取消检查点缺失（无 cancel 回调，长帧检测不可中断）并入本条整改域。

### 11.4 TEST-STAR-DESIGN-001 冻结测试设计（可执行 TEST-P1-STAR-001 由 P1-STAR-TEST 落地）

- F1 合成高斯星场（已知中心/流量/FWHM/SNR）: 亚像素质心 |Δc|≤0.3 px
  （SNR≥20）；FWHM 相对误差 ≤10%；完整性: SNR≥10 星召回 ≥99%；纯噪声场
  虚警 ≤0.1/千像素（专项=completeness/false positive synthetic fields）。
- F2 饱和/混合/边缘专项: 平台≥3px 饱和星检出且 saturated=1；饱和+正常星
  d<2px 重叠 → 保留饱和星（dedup 语义）；边界 2px 内允许丢弃（§4 边界语义）。
- F3 确定性: 同输入线程数 1/2/4 输出 bitwise 一致（§5）；mag 升序+NaN 末尾
  全序断言；maxStars 截断保留最亮（:2240-2242）。
- F4 FP64 通道 oracle: 独立 Moffat4/Gaussian 复算（B,A,x0,y0,sx,sy,theta）
  |Δ中心|≤0.05 px、A/B 相对误差 ≤1e−3（GSL 对独立实现，双精度）；FP32 通道
  经 uint16 量化容差独立冻结 |Δc|≤0.5 px（DISP-STAR-001）。
- F5 状态码负例: NULL/空图/0 尺寸 → −1；空场 → count=0 且 rc=0。
- F6 回归锚: Galaxy_Center 类饱和平台场多检/漏检回归 fixture（源码教训
  :1854/:2217-2218 固化）。容差冻结: 上述数值在 TEST 落地时逐项写死，
  P1-STAR-TEST 不得放宽；fixture 生成器注记容差来源（本节）。

### 11.5 SCI-P1-STAR-001 状态声明

科学专项（matrix P1-STAR 行）映射：subpixel centroid=§2 一阶导/零交叉/Moffat4
中心（连续估计，无网格量化）；completeness/false positive synthetic fields=
§11.4 F1（检测完备性/虚警由合成场验收，非解析保证；5σ 阈值语义）；saturation/
blend/edge=§2 饱和双条件+edge-walking+dedup 保饱和+§4 边界丢弃；deterministic
ordering=§5 全序确定（mag 升序+NaN 末尾+串行 dedup/sort）。共享 SCI（PSF/
PHOTOMETRY/ASTROMETRY）不因本附录改动；本节禁止被编排层词汇反向改写
（descriptor astrocs.phase1.star-psf 由 P1-PSF-INT 对齐，不作冻结依据）。
