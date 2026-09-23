# Drizzle Geometry Algorithms (P1-DRZ)

> 上游：ASTROCS_DESIGN.md §5.2（固定科学流程）、§6.3（投影算法）

> ID 覆盖: ALG-DRZ-001  状态: CONTRACT_READY  上游: SCI-DRZ-001  下游: DATA-P1-DRZ / API-DRZ-001 / TEST-DRZ-DESIGN-001
> 本文档由源码逐函数核对后登记。实现唯一生产源 =
> `lib/algorithms/drizzle/healpix_drizzle/`（CMake 目标 `astrocs_drizzle`，
> CMakeLists.txt:356-366；C ABI 导出 `lib/algorithms/drizzle/healpix_drizzle/
> hp_drizzle_api.h:43,62,70,130,139,140`）；迁移目标目录 `lib/algorithms/drizzle/`。科学定义见
> `docs/science/DRIZZLE.md`（SCI-DRZ-001，FROZEN，集合 SCI-DRZ-001/014/015/016）。
> 本文档只登记离散算法与实现事实；源码与 SCI 的差异全部登记于 §10（DISP-DRZ-*）。
> **权威订正原则** = `ENGINEERING_SPEC.md` §3「科学正确性优先」：独立证据（外部标准 /
> 文献 / 可复跑实验）证明文档与事实不符时，**订正文档是义务**（SCI 层订正走变更流程并
> 记录证据与影响面）；文档已被证明正确而实现不符时改实现。禁止声明 IMPLEMENTED。

## 0 范围界定

本文档以单一 **ALG-DRZ-001**（drizzle 几何与累加合同）覆盖现行唯一
生产 tiled 实现全链路
（`drizzleTiled[_f64]` → `processPixelSharedTiled/processPixelTiled` →
球面几何 → tile 累加器），实现与测试均以本文档为准：

- 候选缓冲/几何合同一律引用 ALG-DRZ-001 与 TEST-DRZ-DESIGN-001；
  源码注释中出现的 `ALG-DRZ-GEOM-CACHE-001`、`ALG-DRZ-VAR`、`TEST-ALG-DRZ-*`
  为同义登记名，不另立合同。
- S_p=F_p/D_p 面亮度归一与 variance/ivar finalize 属 astro_image_io
  下游（astro_sphere_sink.cpp:100 传出原始累加量后由
  aio_hips_writer finalize_tile 完成，见 DISP-DRZ-007），不在本模块。

## 1 ALG-DRZ-001 核心累加公式（与 SCI-DRZ-001 §5 对照）

- 源锚: `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`
  （`processPixelTiled` / `processPixelSharedTiled` 累加段 1538-1638；
  **符号名优先于行号**，行号仅作导航）。
- 记号: 源像素 j（值 x_j [ADU]、方差 v_j [ADU²]、权重面）、目标
  HEALPix NESTED leaf p；drop = 源像素按 pixfrac 收缩的球面 footprint；
  A_pixel,j = **未收缩**源像素球面总面积 [sr]（`spherical::polygon_area_consistent`）；
  A_drop,j = drop 球面总面积 [sr]
  （S-H 裁剪前，双精度角点累积，<1e-20 拒绝，
  `build_drop_geometry_into` / `DropGeometryT::drop_area`）。
- 离散公式（逐条源码锚；**单位逐项**与 `docs/contracts/DATA_SEMANTICS.md` §31.1a
  的量纲链逐段一致，`FZ-UNIT-SIGNAL-SB` FROZEN）:
  - 权重: `w_jp = a_jp / A_pixel,j`，**量纲 = 1（无量纲）**（a_jp 与 A_pixel,j 同为 sr）。
    **面亮度保持口径**（SCI-DRZ-001 §5 目标态面亮度保持权重），a_jp = drop ∩ target p
    球面交叠面积 [sr]；A_pixel,j = **未收缩**源像素球面面积 [sr]，由
    `spherical::polygon_area_consistent`（与 drop_area 同一分支同一例程）
    对未收缩四角求值。`pixfrac==1` 时未收缩四角 ≡ drop 四角 ⇒ 分母直接取
    `drop_area`（**逐位不变**，默认 `drizzle.pixfrac=1.0` 零回归）；
    `pixfrac<1` 时多 4 次 `pixelToSky`（审核量化 +3%）。w≤0 拒绝。
    **适用域（分母口径不可替换）**：`A_pixel,j` 必须由未收缩四角在同一球面面积例程上
    求值；`A_drop,j = pixfrac²·A_pixel,j` 只在平面（仿射）极限下精确，球面残差
    `δ = A_drop/(pixfrac²·A_pixel) − 1 = (1−pixfrac²)·θ²·[0.25/(1+r_c²) − 0.625·ξ_c²/(1+r_c²)²] + O(θ⁴)`
    （θ = 源像素角尺度 [rad]，(ξ_c,η_c) = 像素中心的 gnomonic 平面坐标 [rad]；
    推导与实测见 §10 DISP-DRZ-009 段）。θ=2″/px、pixfrac=0.8、r_c=0 时 δ=8.46e-12；
    θ=300″/px 时 δ=1.90e-7（与 DISP-009 注入产物实测 1.48e-7 同阶）。故该替换**不得**采用。
    **禁用**按 `A_drop,j` 归一（`w_jp=a_jp/A_drop,j`）：会使 `S_p` 偏 `1/pixfrac²`
    （pf=0.8 → +56.25%），负例判据 `1/pf²−1`。
  - 通量: `F_p = Σ_j x_j · w_jp`，**单位 = ADU**（x_j 为帧平面线性计数 [ADU]，
    w 无量纲；`acc.sumFlux += Scalar(pixelValue * weight)`，:1628）。
  - 支撑面积: `D_p = Σ_j a_jp`，**单位 = sr**（`acc.sumArea += Scalar(overlap_area)`，
    :1629；**绝对球面面积**，非无量纲覆盖分数；support = D_p/A_cell 无量纲，语义不变）。
  - 方差分子: `sumVarNum_p = Σ_j v_j · w_jp²`，**单位 = ADU²**（v_j 为输入源像素方差
    [ADU²]，`FZ-UNIT-VAR-IN`；`(double)v · (double)w²` 中转再转 Scalar，
    仅当 v>0 累加，:1630-1634）——"仅 v>0"即"方差不可用样本不进入方差项"，
    依据 DATA_SEMANTICS §4a「无方差信息 ⇒ variance=0 ∧ ivar=0（显式不可用）」
    与 §20.1「ivar==0 = 合法零权重」。
    **α² 缩放律（代数恒等）**：`v_j → α²·v_j` ⇒ `sumVarNum_p → α²·sumVarNum_p`
    ⇒ `variance_p → α²·variance_p`（sumVarNum 对 v_j 线性、D_p 与 v 无关），
    ivar 按二次律 `→ α⁻²`；该律是恒等式而非近似，故回归门取逐像素 `worst_rel < 1e-4`
    只受浮点重结合限制（§9 容差行）。
  - 贡献计数: `nContrib_p = Σ_j 1`（**合格样本计数**，计样本级贡献，非权重级；
    与 §5 的 `n_rejected_nonfinite` 是两个不同的量，不得混用）。
- **估计量类别（与一手文献对照）**：`S_p = F_p/D_p = Σ_j B_j·a_jp / Σ_j a_jp`
  （B_j = x_j/A_pixel,j [ADU/sr]）是**输入面亮度的 a_jp 加权平均**。Fruchter & Hook 2002
  （PASP 114, 144；arXiv:astro-ph/9808087v2 §2 式(4)(5)）的 drizzle 输出为
  `I = Σ_i d_i·a_i·w_i·s² / Σ_i a_i·w_i`（"a factor of s² is introduced to conserve
  surface intensity"），同属"输入值的加权平均"族：两者对常量面亮度场均与 pixfrac 无关，
  差别在权重口径——F&H 用 `a·w`（w = 用户输入权重）并把像素尺度比以 `s²` 显式换算，
  本模块用 `a`（几何交叠面积）并把绝对面亮度写进分子 `a/A_pixel,j`（⇒ 无需 s² 因子），
  分母 D_p 兼作 support 的分子（`support = D_p/A_cell`）。该差异为 Project-defined
  选择，须与 `FZ-UNIT-SIGNAL-SB` 的量纲链一并阅读，不得按 F&H 原式替换。
- **S_p = F_p/D_p 归一不在本模块**: sumFlux/sumArea/sumVarNum 原始和
  逐 tile 传出（astro_sphere_sink.cpp:99-104 dense 化），归一在
  aio_hips_writer finalize_tile（variance = var_num_sum/area²，
  lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:566-631）——与
  drizzle_engine.cpp:2-3 锚注释一致。
- 单位/dtype: 累加器 Scalar = float（precision_mode=0）或 double（=1）
  显式模板双实例（drizzle_engine.cpp:2177-2184）；a_jp/面积几何全程
  double，FP32 仅发生在累加存储层（逐项舍入，容差门见 §9）。

## 2 几何管线（drop footprint → 候选 → 交叠面积）

- drop 四角: `half = 0.5·pixfrac`，`(x±half, y±half)` 经 WCS/SIP
  `pixelToSky` → 单位 Vec3（drizzle_engine.cpp:1306-1311 四角构造；
  pixfrac==1.0 时行级顶点共享缓存，:1666,1689-1713）。
- 收缩语义: 球面 slerp 不做显式插值，四角直接按收缩后平面坐标取
  WCS 映射（pixfrac 收缩在源平面完成）；pixfrac∈(0,1] 严格校验，
  ≤0 或 >1 拒绝不夹逼（:1570-1577；文件通道 API 层接受 0.0 的双轨
  差异见 DISP-DRZ-003）。
- 三层候选缓冲（spherical_overlap.cpp）:
  1. quick-reject: `lim = max_angle + 1.25·hp_res`
     （HP_CIRCUMRADIUS_FACTOR=1.25，:42）。**系数依据（可判定上界）**：HEALPix 单元的
     外接半径 `r_circ`（中心→最远边界点）与等面积尺度 `hp_res = sqrt(A_cell)` 之比在
     全天上界为 **1.0442**（本仓实测 `run/SCI-FIX-DRZGEOM-01/evidence/
     exp_c_realdata.json` C3 段：nside=512/1024 × 181 档纬度扫描，最坏 1.0441/1.0442，
     出现在 |dec|≈42°，**不是极区**；阴性对照等经纬网格同度量达 1.669）⇒ 1.25 相对
     该上界留 19.7% 余量，且覆盖跨 face 边界（面内畸变已由 `hp_res` 的球面定义吸收）。
     零漏选由 9003 例全枚举 oracle 兜底（§9）。
  2. 保守查询圆: `query_radius = max_angle + 3.0·hp_res`
     （queryDisc 回退）。**系数依据**：候选完备性要求
     `query_radius ≥ max_angle + r_circ,max = max_angle + 1.0442·hp_res`（任一与 drop 相交
     的 cell，其中心必落在 drop 的 `max_angle` 邻域加该 cell 外接半径之内）⇒
     3.0·hp_res 比 1.0442·hp_res 宽 2.87×，属**保守过覆盖**（代价是候选数，不是正确性）；
  3. fast 路径: 面 delta×1.15 面内畸变系数 + 极冠/跨 face 边界回退
     queryDisc（:1667-1702）。
- 交叠面积: 球面多边形裁剪（clip_normals_d，内部 double）——逐边裁剪的球面推广（Sutherland–Hodgman 1974 原文只处理平面多边形，球面形式为本模块推广）
  + **Van Oosterom & Strackee 扇形三角剖分**有向面积 + 半球包含检查
  （max_ang ≥ π/2−1e-12 → NAN，:186-239）。本模块面积算法的唯一命名 =
  Sutherland–Hodgman 球面逐边裁剪 + **Van Oosterom & Strackee 扇形三角剖分**；
  **禁用** "Girard 定理"（内角和式）这一命名——本模块无该实现。
- 目标几何缓存: per-thread LRU 8192（TargetGeomCache），hit 复用
  center+boundary4；per-run generation 原子递增清空
  （drizzle_engine.cpp:1662-1663 `s_target_cache_gen.fetch_add`；
  **禁用**裸 static 可变状态——会引入 data race）。
- HEALPix 地址: 仅 NESTED；`parent = ipix >> 2·d`、
  `local = ipix & (4^d − 1)` 位分解（:1515-1516）；候选枚举 Morton
  spread 位交织（spherical_overlap.cpp:1639-1653）；shim
  healpix_core.h 转发 astrocs::healpix::ang2pix_nest，RING 直接拒绝。

## 3 auto nside 决策（compute_auto_nside）

- 源锚: `drizzle_engine.cpp` `compute_auto_nside`（:624-745；符号名优先，行号仅作导航）。
- `HEALPIX_SCALE_PER_NSIDE_ARCSEC = sqrt(π/3)·(180/π)·3600 = 211076.28514206142″`
  （:706-708；**该常数是 HEALPix 等面积基数的精确开方**，不是近似：
  `sqrt(4π/(12·nside²))·(180/π)·3600 ≡ sqrt(π/3)/nside·(180/π)·3600`，逐位恒等，
  依据 Górski et al. 2005, ApJ 622, 759 §4「all pixels have exactly equal area
  `4π/(12·nside²)`」；本仓实验锚 `run/SCI-FIX-DRZGEOM-01/evidence/exp_a_geometry.json`
  A3 段：三档 nside 的恒等相对差 = 0.0）。
  **数值纪律**：常数按上式**求值**，不得抄写十进制近似值；`211034.6` 与上式相差
  −1.97e-4（相对），会使 `nside` 决策在每个倍频程的
  `finest ∈ (211034.6/2^k, 211076.285/2^k)` 窗口内**少取一阶**
  （窗口相对宽度 1.97e-4，20 个倍频程全部存在；实测例：finest=105527.7″ 时
  上式给 nside=4、抄写值给 nside=2 ⇒ 像素尺度差 2 倍，破坏 1–2× 过采样目标）。
- 决策: 最小 2 次幂 ≥ 211076.28514206142/finest_arcsec（判据式
  `hp_res_arcsec = C/nside ≤ finest_arcsec`，:710-724）；无效输入（finest≤0 等）
  → 0 由调用方兜底（:697-701）；钳位 `[NSIDE_MIN, NSIDE_MAX] = [16, 4194304=2^22]`
  （:726-730）。**钳位下界 16 的适用域**：nside=16 对应等面积尺度
  `211076.285/16 = 13192.3″ ≈ 3.66°`，即该函数可表达的最粗叶级；更粗的请求被钳到 16，
  此时 `oversample = hp_res/finest > 1`（欠采样）——消费方必须读 `oversample` 字段判欠采样，
  不得只看 `nside`（:732-740 输出该字段）。
  **适用域**：判据量是**等面积等效线尺度** `sqrt(A_cell)`（面积是常数），
  而 HEALPix 单元的**局部采样步长**（邻元中心角距）随纬度与方向变化：
  nside=512 实测邻元步长 ∈ [0.63, 0.71]×`sqrt(A_cell)`（共边邻元）与
  [1.95, 2.94]×`sqrt(A_cell)`（对角邻元），各向异性最大 ≈4.6×
  （实验锚同上 A4 段，nside=512/1024 双档）。故"过采样 1–2×"是按等面积尺度
  定义的**面平均**口径；要求**方向性**分辨率保证的消费方必须按局部步长另加余量。
- orchestrator 侧策略 `1x_to_2x_drizzle`/`fixed` 最终映射到本函数
  （orchestrator.cpp:3298-3311）。

## 4 反向 drizzle（Sphere→Plane，REV-101..107）

- 源锚: reverse_drizzle.h:26-70、reverse_drizzle.cpp:255-334。
- 语义: 每 source leaf 构造球面 footprint（边界自适应细分），pixfrac
  沿球面向 leaf 中心收缩；目标平面像素经 WCS/SIP 映射为球面
  footprint；重叠面积 = 球面面积（禁止平面 2D 面积作权重）；signal
  按球面面积比例分摊；coverage 以"覆盖在 leaf 内均匀分布"假设输出。
- 严格校验（reverse_drizzle.cpp:255-334）: nside 2 的幂 [1,2^22]；
  仅 NESTED；宽高>0；pixfrac∈(0,1]；CD 行列式有限且 |det|≥1e-30、
  |cd[k]|≤1 deg/px；crval dec∈[-90,90]；f32/f64 signal 二选一严格；
  support∈[0,1]（空=全 1.0）；ipix 越界/重复拒绝。
- 能力/版本: hp_drizzle_reverse_capability 位 0x01|0x02|0x04|0x08|
  0x10|0x20（api.cpp:145-147）；version "1.0.0"（:149-151）。

## 5 输入校验与 NaN/Inf/invalid 边界

| 条件 | 行为 | 锚 |
|---|---|---|
| pixfrac ≤0 或 >1（引擎层） | 拒绝（不夹逼） | drizzle_engine.cpp:1570-1577 |
| pixfrac==0.0（文件通道 API 层） | 放行后进引擎再拒（双轨） | api.cpp:191（DISP-DRZ-003） |
| RING（nested=0） | 硬拒绝 | :1578-1582；shim throw |
| channels≠1 多通道 | 拒绝 | :1583-1591 |
| 缺 WCS（CD 与 CDELT+CROTA2 均无） | 拒绝（帧通道返回 -9） | api.cpp:541-545 |
| 尺寸/空指针非法 | 拒绝 | drizzle_engine.cpp:1597-1606 |
| **值像素 NaN/Inf** | 按 `rule_id NAN-SAMPLE-MASK-COVERAGE-NAN` 处置 = **样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数**（唯一口径文字 = `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md` §2a）：不合格样本从 `F_p`、分母、方差三项一并剔除并重新归一，仅零合格样本输出 `NaN ∧ support≤0`，每个输出像素必须暴露被剔除样本计数 `n_rejected_nonfinite`（按原因分类、互斥可加）。**实现锚**：`!std::isfinite(pixelValue) → ++tc.rejected_nonfinite_value; continue`（`DrizzleEngine::drizzleTiledImpl` 主循环，:2000-2005）；分类计数聚合为 `DrizzleStats::n_rejected_nonfinite{,_value,_variance,_nonpositive_weight}`（:2203-2208）。 | :2000-2005 / :2203-2208 |
| SNR 面非有限 | 计入 `rejected_nonfinite_value` 同族掩膜路径（SNR 面参与权重/有效性判定，禁止静默跳过） | :2000-2005 |
| 权重面非有限或 ≤0 | 计入 `rejected_nonpositive_weight`（原因 3）后剔除该样本（**必须计数**，禁静默） | :2013-2018 |
| variance 面非有限（NaN/Inf） | 计入 `rejected_nonfinite_variance`（原因 2）后剔除该样本（**必须计数**，禁静默） | :2022-2027 |
| **variance 面 = 0（方差不可用）** | **不是无效像素**：样本合格性只判 `isfinite(x_j)`（DATA-002 §2a）；variance=0 按 DATA_SEMANTICS §4a「无覆盖/无方差信息像素写 variance=0 且 ivar=0（显式不可用）」与 §20.1「ivar==0 = 合法零权重、variance==0 = 无信息」处理 ⇒ **只令方差项为 0，不丢信号、不丢几何支撑**。`V_j ≤ 0` 不构成掩膜授权（ASTROCS_DESIGN §5.5:379 只授权对 NaN 做样本级掩膜） | `drizzle_engine.cpp` V≤0 分支（现行 :2033-2038 置 0 不 continue）；订正依据与实测见 `run/VARIANCE-SEMANTICS-01/REPORT.md` |
| 几何 NaN（ra/dec 非有限） | 显式拒绝该像素 | :1322-1323 |
| 半球检查失败（max_ang≥π/2） | 返回 NAN 面积 | spherical_overlap.cpp:196-216 |
| A_drop<1e-20 / w≤0 | 拒绝 | drizzle_engine.cpp:1439-1441,1509-1512 |
| nside 非法（shim 构造） | 容忍不抛，校验责任在调用方（缺陷候选） | healpix_core.h:25-27 |
| reverse 输入非法 | 非 0 返回码，逐项校验 | reverse_drizzle.cpp:255-334 |

## 6 确定性与归约（1/N 合同）

> **归约结合树是*输入*的函数**：现行实现把归约结合树定义为输入的函数，跨线程预算
> **bitwise 一致**；下列条目按 `drizzle_engine.cpp` 现行符号（`drizzleTiledImpl` /
> `drizzle_deterministic_stripe_count` / `merge_tile_map_into`）逐条对应。

- 并行（P15a DRIZZLE-DET-001 + P22 池化归约流水线）: y 行按
  `drizzle_deterministic_stripe_count(img.height) = min(ceil(height/16), 256)`
  划分为**固定 stripe 数**（**仅依赖 `img.height`，与线程预算无关**）；
  每个 stripe 由**唯一**线程按 (y,x) 行主序累加进池中 scratch map
  （无 atomic、无竞争）；stripe 边界 `y0 = stripe*height/n_stripes` 只依赖
  stripe 索引。worker 数唯一来自 `config.threads`（>0）否则
  `omp_get_max_threads`（`drizzleTiledImpl`），不改全局
  omp_set_num_threads；无 _OPENMP 退化串行（tid=0）。
- 累加结构: 归约池中至多 `kScratchPoolCap` 个 scratch map（P22 前为
  per-thread map）+ per-thread 计数器；行级顶点缓存 thread_local
  （`shared_vertices = (config.pixfrac == 1.0)`）。
- 合并: 任意线程按 `merge_cursor` **升序左折叠**归约
  （`merge_tile_map_into`：按 `touched` 插入序逐 leaf 累加，字段序
  sumFlux→sumArea→sumVarNum→nContrib）；输出 `tiles` 按 `parent_ipix` 排序
  ⇒ tile 写盘顺序也与线程数无关。
- **1/N 确定性成立（跨线程数 bitwise）**: 任一 leaf 的浮点加法结合树
  = "stripe 0..N_s−1 升序左折叠"，只是**输入与 `img.height`** 的函数
  ⇒ 同输入在不同 worker 预算下产物科学载荷逐字节一致。
  回归锁（sha256 逐位比对）: `p1drz_taskset_invariance`（P15a，正方形 256²，
  taskset 1/2/4/8/16）与 `p1drz_merge_pipeline_lock`（P22，高瘦 256×1024
  = 64 stripe，强制池耗尽/归还与跨线程归约归属，FP32/FP64 × 两轮重复）。
  **禁用** per-thread map + 按线程序 `t=1..N−1` 合并的归约：其结合树依赖
  worker 预算，跨线程数不再 bitwise 一致（负例判据）。
- ThreadLease: 模块内零命中；omp 为模块内部通道，生产调度走 Runtime
  lease（CMakeLists.txt:379-382 注释）；ThreadLease 迁移必须保持本节合并序。

## 7 复杂度与内存

- 时间: O(n_source · avg_candidates)，avg≈3.5（小图实测，9003 例
  oracle 枚举）；fast 路径圆心距预过滤降低 S-H 调用。
- 内存: tile 累加器 leaf 连续数组 O(1) 寻址（禁 per-leaf 全局 map，
  :1525 注释）；target 缓存 per-thread bounded 8192 LRU；单交集
  O(顶点数≤8) 无整帧副本；SNR 控制点 RAII vector（api.cpp:609-613，
  已消除 free 泄漏）。

## 8 数据布局

- Tile: `TileAccumulatorT<Scalar>{parent_ipix, touched[], pixels[4^d]}`
  ；`TileLeafAccumulatorT<Scalar>{sumFlux, sumArea, sumVarNum,
  nContrib}`（drizzle_engine.h:64-71；release 注释"3 字段"与实际 4
  字段不符，DISP-DRZ-006）。
- HiPS 直写: tile_depth 必须 =9、nside≥512（astro_sphere_sink.cpp:36-51），
  leaf tile 512×512 1:1，产品 SIGNAL/SUPPORT + V19 variance/ivar
  （有 variance 输入时）；provenance 写 pixfrac/源像素尺度。
- HissWriter 流式（writeHisTilesT，drizzle_engine.cpp:1265 finalize），
  测光 gate :1950-1956；operation_counts.json 剖面
  （api.cpp:1091-1134）。
- **B2-A12 精度 provenance（无 silent 缺省）**: 累加精度由
  `drizzle.precision_mode`（整数 0=FP32 / 1=FP64）显式给出；缺失或非整数
  → DATA 拒绝（不 silent 降 FP32）。节点写 p1_stack.json `precision_mode`、
  帧头 `PRECISION`=fp32/fp64 实际值（module_adapters.cpp:2023-2049）。
- **RESCUE-FD-02 库边界精度缺省（无 silent FP32）**: `hp_drizzle_run` 参数
  `precision_mode==-1` 且帧头无 `PRECISION` KV 时按本条 `RESCUE-FD-02` 取 **FP64**
  （原引「宪章 §5.3」已废止，现行载体 = 本条；数据面同源 =
  `DATA_SEMANTICS.md` §11.1「PRECISION」行；**语义不变**）
  （不再静默 FP32）；帧头显式 `fp32`/`fp64` 生效；未知 KV 值或参数
  非 -1/0/1 → 显式拒绝（返回非零，不写产物）。生产节点仍由
  `drizzle.precision_mode`（0|1）显式门把关（B2-A12）。
  （hp_drizzle_api.cpp:956-991；回归 eng/tests/unit/drizzle_precision_default_test.cpp）
- **B2-A14 测光 provenance（禁硬编码 PHOTAPPL=1）**: PHOTSCAL/PHOTAPPL
  由真实测光 provenance `p1_phot.json`（DATA-P1-PHOTPROV-001，
  `p1_op_photometry` 产出）决定；未应用测光 → PHOTAPPL=0 + 帧头
  `PHOTDEGRADE=1`，引擎在显式声明时降级写 BUNIT=ADU（photappl=0），
  未显式声明仍按 02_FROZEN §7 拒绝（drizzle_engine.cpp:1950-1956；
  hp_drizzle_api.cpp:935-937；module_adapters.cpp:2054-2083）。
- 输入通道: PipelineFrame "data"（f32/f64 二选一，bzero=0/bscale=1
  固定，api.cpp:486-503）+ header WCS/SIP KV + 可选 "snr_model" 块
  （KD-tree IDW 重建逐像素 SNR，snr_evaluator.h）。
- **B2-A17 SIP 桥接**（AUD-COORD F-03）：编排 drizzle 节点从上游
  `p1_wcs.json`（DATA-P1-WCS §18）读回 `wcs.sip`（order/ap_order/a/b/ap/bp，
  i*6+j），经 `p1_sip_write_header_frame` 写入 frame header 的 FITS 键：

  ```text
  CTYPE1/2 带 "-SIP" 后缀
  A_ORDER / B_ORDER
  A_i_j / B_i_j
  AP_* / BP_*
  ```

  无 SIP → CTYPE 不含 `-SIP` 且不写任何 SIP 键。reverse 通道 sip_order
  校验 [0,5]（DISP-DRZ-001）；`p1_stack.json` 记录 sip_present/sip_order/
  ctype1/ctype2 作为"实际下发"证据（module_adapters.cpp p1_op_drizzle）。

## 9 TEST-DRZ-DESIGN-001（测试设计冻结，可执行 TEST-P1-DRZ-001 由 P1-DRZ-TEST 建立）

- fixture（全合成、离线、零真实数据）:
  - FIX-DRZ-A 常量场（面亮度语义 oracle：每像素常量 ADU ⇒ S=C/A_drop
    ≠C，检验 D_p 语义）；FIX-DRZ-B 点源高斯（总通量守恒）；
    FIX-DRZ-C 梯度场；FIX-DRZ-D 脉冲单像素；FIX-DRZ-E NaN/Inf 注入面；
    FIX-DRZ-F SIP 畸变边缘 patch（15° 宽场）。
- 冻结容差（预冻结，源自既有测试门，不得放宽）:
  - FP64 通量闭合 <1e-6（主域）；FP32/FP64 逐 leaf <1e-5
    （drizzle_freeze_test.cpp 硬门，:211 附近；l0 小图 FP64<1e-10）；
  - 方差 α² 缩放律逐像素 worst_rel <1e-4（variance_propagation_test，
    SCI-DRZ-014）；
  - 候选零漏选: 9003 例 false_negative=0（candidate_oracle_test，
    4 pixfrac × 5 尺度 × 7 nside 全枚举，RA 跨 0/极区/face 边界）；
  - reverse: false_hole=false_fill=0、coverage∈[0,1]、常数场
    uniform_rel_std<1e-4、半图/全图 total_in 比 <1e-6；
  - 面积误差预算: float 面积 0.05% ≪ 候选保守性（零漏选）≪ 门禁
    容差；arc-chord 1e-6·hp_res（旧 §9/§12 冻结值照抄）。
- 不变量: 常量场均匀性（无接缝/系统性亮斑，drizzle_freeze_test:422）；
  NESTED 地址往返；1/2/4 线程一致性；HISS 写读往返；负值保持；
  multi-FWHM 孔径测光 4σ 相对差 <1e-3。
- 负面矩阵: §5 表逐行断言（pixfrac 0/负/>1、RING、多通道、缺 WCS、
  NaN 面、reverse 二选一/越界/重复 ipix）。

## 10 DISP-DRZ-001..009（SCI/文档 vs 源码口径清单；修复不得反向改 SCI）

| # | 文档声称 | 源码实际 | 双方锚 |
|---|---|---|---|
| DISP-DRZ-001 | hp_drizzle_api.h:93 注释 sip_order "0..4" | hp_drizzle_api.cpp:98-103 校验 [0,5]（6×6 系数组支持 5 阶下标） | hp_drizzle_api.h:93 vs hp_drizzle_api.cpp:98-103 |
| DISP-DRZ-002 | 源码注释 `spherical_overlap.h:15,77` / `spherical_overlap.cpp:11` 写 "Girard 定理" | 面积实现 = S-H 球面裁剪 + Van Oosterom & Strackee 扇形三角剖分，无 Girard 实现；文档侧命名已与实现一致，**禁用** "Girard 定理" 命名 | spherical_overlap.h:15,77; spherical_overlap.cpp:11 vs spherical_overlap.cpp:186-239 |
| DISP-DRZ-003 | pixfrac∈(0,1] 单一边界 | 文件通道 API 层接受 0.0（<0 才拒），引擎层拒绝——两层双轨 | api.cpp:191 vs drizzle_engine.cpp:1570 |
| DISP-DRZ-004 | 值像素 NaN 按 `rule_id NAN-SAMPLE-MASK-COVERAGE-NAN` 处置 = 样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数（`DRIZZLE.md:116`）：不合格样本剔除并重归一、仅零合格样本输出 `NaN ∧ support≤0`、必须暴露 `n_rejected_nonfinite` | **约束**：主循环按原因分类计数（值/方差/权重三分类，:2000-2027）并聚合暴露 `DrizzleStats::n_rejected_nonfinite*`（:2203-2208）；**禁用**把非有限样本传播进 `F_p`/分母/方差——会污染整像素信号与几何支撑（负例判据：零合格样本必须输出 `NaN ∧ support≤0` 且分类计数非零） | DRIZZLE.md:116 vs drizzle_engine.cpp:2000-2027 / :2203-2208 |
| DISP-DRZ-005 | `max_angle < 1e-3` 切平面分支是**实际执行路径，必须保留**：微小 drop（角跨度 < 1e-3 rad ≈ 206″）用切平面面积 | 三处活分支：:1091 `g.drop_area` 微小 drop 用切平面面积、:1288-1289 nb=4 重叠 `<1e-3` 用 `planar_polygon_area_n`（否则球面 `spherical_polygon_area_n`）、:1331-1332 三角形扇重叠同策略（与 g.drop_area 表示一致，避免 weight 偏差） | spherical_overlap.cpp:1091,1288-1289,1331-1332（注释 :1001-1007,:1078-1079）；θ<1e-3 时切平面偏差 <4e-8，球面 double 相消噪声 ~1e-4~5e-5 |
| DISP-DRZ-006 | TileLeafAccumulatorT release 仅 3 字段（drizzle_engine.h:62-63 注释） | 实际 4 字段（sumVarNum 为正式产品） | drizzle_engine.h:62-63 vs 64-71 |
| DISP-DRZ-007 | SCI §13 方差锚 drizzle_engine.cpp:100/736-762 | 行号漂移：现行方差锚 astro_sphere_sink.cpp:100 + aio_hips_writer finalize_tile | DRIZZLE.md:132 vs drizzle_engine.cpp:2-3 |
| DISP-DRZ-008 | poly_clip.h 自述生产重叠面积用途 | PolyClip（平面 S-H/Shoelace）生产 tiled 路径零调用 | poly_clip.h:4-15 vs drizzle_engine.cpp 全文 |
| DISP-DRZ-009 | SCI-DRZ-001 §5 目标态面亮度保持权重 `w_SB=a_jp/A_pixel,j`（`S_p=Σ_j B_j a_jp/Σ_j a_jp`） | **约束（覆盖正向与反向两条路径）**。**正向** `processPixelSharedTiled`：`weight = overlap_area / pixel_area`（分母 = 未收缩像素面积 A_pixel,j）。**反向** `reverse_drizzle.cpp`：`S_p = Σ_j B_j·a_jp / Σ_j a_jp`（面亮度加权平均，分子分母各自累加后相除）。**两条路径都禁用** `w_jp=a_jp/A_drop,j`：正向 `pixfrac<1` 时偏 `1/pixfrac²`（pf=0.8→+56.25%，与 `1/pf²−1` 逐位吻合）；反向则使输出随**源 nside** 变化（实测 nside=16/64/256 偏 −99.98%/−99.70%/−95.23%，`S_p` 与 `B0·A_pixel/A_leaf` 逐位吻合）——两者均为负例判据 | DRIZZLE.md:40-54 vs drizzle_engine.cpp `processPixelSharedTiled`（`pixel_area` 段）与 reverse_drizzle.cpp（`weight` 累加 + 输出归一）/ `spherical_overlap.cpp:polygon_area_consistent`；契约 `FZ-FORMULA-DRIZZLE-SB`（docs/contracts/DATA_SEMANTICS.md §31.1、§11.2）；回归门 `p1drz_disp009` |

无差异项（核对通过）: F/D/sumVarNum 结构、HP_CIRCUMRADIUS
_FACTOR=1.25、三层缓冲语义、NESTED 统一、按线程序合并确定性。

**面亮度保持权重的实现口径**：
`drizzle_engine.cpp` 的 `processPixelSharedTiled` 内权重恒为
`weight = overlap_area / pixel_area`，其中 `pixel_area` = **未收缩**源像素球面面积
（新增 `spherical::polygon_area_consistent`，与 `build_drop_geometry_into` 的
`drop_area` 同一分支同一例程）；`pixfrac<1` 时由调用方
（`processPixelTiled` 的 Step 2b）补 4 次未收缩四角 `pixelToSky`；
`pixfrac==1` 时传 nullptr ⇒ 分母直接取 `drop_area`，产物**逐字节不变**
（sha256 实证：`pixfrac==1` 默认路径的 `.norm.hiss`/`.canon` 与分母取 `drop_area` 时逐字节相同
⇒ 默认路径零回归）。`sumArea`（D_p=Σa_jp，
support 语义）与 `sumVarNum`/`variance=sumVarNum/sumArea²` 语义不变。
**注意**：近似写法 `overlap_area·pixfrac²/drop_area` **不得采用** ——
恒等式 `A_drop,j = pixfrac²·A_pixel,j` 只在平面（仿射）极限下精确。**残差标度律**
（二阶展开 + Gauss-Legendre 求积独立复算，锚 `run/SCI-FIX-DRZGEOM-01/evidence/
exp_a_geometry.json` A2 段；两条数值路径与解析式在 |δ|>1e-13 域内相对差 <2.4e-3，
小尺度档 <2e-5）：

```text
δ(pixfrac, θ, ξ_c, η_c) = A_drop/(pixfrac²·A_pixel) − 1
                        = (1−pixfrac²)·θ²·[ 0.25/(1+r_c²) − 0.625·ξ_c²/(1+r_c²)² ] + O(θ⁴)
   θ     = 源像素角尺度 [rad]      r_c² = ξ_c²+η_c²   (ξ_c,η_c) = 像素中心 gnomonic 平面坐标 [rad]
```

**量级与适用域**（pixfrac=0.8、像素中心在参考点 r_c=0）：θ=0.2″→8.5e-14；
θ=2″→8.46e-12；θ=6.3″→8.40e-11；θ=60″→7.62e-9；θ=300″→1.90e-7。
⇒ θ ≲ 10″/px 时该替换的系统偏差 ≲ 1e-10（远低于本模块 1e-5/1e-6 通量门），
但 θ ≳ 100″/px 时进入 1e-7–1e-6 量级，与门禁容差同阶 ⇒ **全域禁用**该替换。
**与精度效应分离**：float32 存储角点另引入面积相对误差（实测 θ=2″→7.6e-8、
θ=300″→4.4e-8，随角点舍入符号变化），量级与 δ 不同源；文档与实现不得把两者
合并成单一常数。**真实产物实证**（θ=300″/px 常量场
B0=1000、nside=512、W=H=16）：注入态（分母取 A_drop）逐 leaf `S_p/B0` 实测
1.562500231（pixfrac=0.8）/2.777777366（0.6）/3.999998942（0.5），与解析
`1/pixfrac²` 的相对差 +1.48e-7/−1.4e-7/−2.6e-7（与 δ(θ=300″)=1.90e-7 同阶，
差异来自本仓 fixture 的独立几何口径）；分母取 `A_pixel,j` 时同四档 `max|S_p/B0−1|` ≤ 4.8e-12。
验证：回归门 `p1drz_disp009`（pixfrac∈{1.0,0.8,0.6,0.5} 常量面亮度
`|S_p/B0−1|<1e-3` + "分母取 A_drop 必判红"的负例控制）与 1/N 逐位锁
`p1drz_taskset_invariance` / `p1drz_merge_pipeline_lock`。

**DISP-DRZ-005 负面用例建议（仅注记，用例实现不在本批）**：θ < 1e-3 rad
的微小 drop 两侧对拍——同一输入分别走切平面分支（`planar_polygon_area_n`）
与球面 Van Oosterom 分支（`spherical_polygon_area_n`），断言面积/weight 差
落在注释论证的偏差带内（切平面 vs 球面 <4e-8 量级，见 :996-1001），
守护该分支在后续迁移中不被误删或语义漂移。

## 11 关联

- SCI: SCI-DRZ-001（docs/science/DRIZZLE.md，FROZEN；§5 公式、§7
  不变量、§15 Acceptance；集合 SCI-DRZ-014 方差传播 / 015 支撑 /
  016 协方差）。
- DATA: DATA-P1-DRZ（docs/contracts/DATA_SEMANTICS.md §11）；上游
  DATA-P1-CAL（§9）；编排现状引用 DATA-P1-STACK（descriptor）。
- API: API-DRZ-001（docs/contracts/PUBLIC_API.md）；API-P1-007
  （docs/api/PHASE1_API_V1.md，区间 API-P1-001..010 编排合同）。
- MOD/SRC: MOD-astrocs-phase1-drizzle（lib/algorithms/drizzle/module.yaml，
  CONTRACT_READY；lib/algorithms/drizzle/README.md 实现事实）；SRC-DRZ-001
  （lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h 等签名源）。
- TEST: TEST-DRZ-DESIGN-001（本文档 §9）；可执行 TEST-P1-DRZ-001
  由 P1-DRZ-TEST 建立（既有 lib 内 eng/tests/*.cpp 为科学门基线）。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- Drizzle 线性重建/drop/pixfrac：Fruchter & Hook 2002, PASP 114, 144（DOI 10.1086/338393）。**差异**：原式在切平面，本模块在球面 HEALPix 上实施（Project-defined 迁移）。
- Drizzle 实践：DrizzlePac Handbook（STScI）；drizzlepac（BSD-3-Clause）。
- HEALPix 几何：Górski et al. 2005, ApJ 622, 759（DOI 10.1086/427976）；astropy-healpix（BSD-3-Clause）、healpy（GPL-2.0）。
- 球面三角面积：Van Oosterom & Strackee 1983, IEEE TBME 30, 125（DOI 10.1109/TBME.1983.325207）。
- 多边形裁剪：Sutherland & Hodgman 1974, “Reentrant Polygon Clipping”, Comm. ACM 17, 32（DOI 10.1145/360767.360802）——**平面**算法（原文摘要为 “plane-faced volumes”）；本模块的球面逐边裁剪是它的推广，不属原文结论。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

