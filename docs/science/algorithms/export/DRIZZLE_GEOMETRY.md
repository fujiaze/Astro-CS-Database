# Drizzle Geometry Algorithms (P1-DRZ)

> 上游：ACSD_DESIGN.md [D-1]「固定科学流程」一节程）、[D-1]「投影算法」一节（投影算法）

> 本文档由源码逐函数核对后登记。实现唯一生产源 =
> `lib/algorithms/drizzle/healpix_drizzle/`（CMake 目标 `acsd_drizzle`，
> 在根 `CMakeLists.txt` 中登记；C ABI 导出 `lib/algorithms/drizzle/healpix_drizzle/
> hp_drizzle_api.h`）；迁移目标目录 `lib/algorithms/drizzle/`。科学定义见
> `docs/science/drizzle/DRIZZLE.md`（SCI-DRZ-001，FROZEN，集合 SCI-DRZ-001/014/015/016）。
> 本文档只登记离散算法与实现事实；源码与 SCI 的差异全部登记于 [S-1]「判据与误差」一节（DISP-DRZ-*）。
> **权威订正原则** = `docs/ACSD_DESIGN.md` [D-1]「文档权威与索引」一节引）「科学正确性优先」：独立证据（外部标准 /
> 文献 / 可复跑实验）证明文档与事实不符时，**订正文档是义务**（SCI 层订正走变更流程并
> 记录证据与影响面）；文档已被证明正确而实现不符时改实现。状态词唯一口径 = `ACSD_DESIGN.md` [D-1]「状态词口径」一节；IMPLEMENTED 只由验收签发。

## 0 范围界定

本文档以单一 **ALG-DRZ-001**（drizzle 几何与累加合同）覆盖现行唯一
生产 tiled 实现全链路
（`drizzleTiled[_f64]` → `processPixelSharedTiled/processPixelTiled` →
球面几何 → tile 累加器），实现与测试均以本文档为准：

- 候选缓冲/几何合同一律引用 ALG-DRZ-001 与 TEST-DRZ-DESIGN-001；
  源码注释中出现的 `ALG-DRZ-GEOM-CACHE-001`、`ALG-DRZ-VAR`、`TEST-ALG-DRZ-*`
  为同义登记名，不另立合同。
- S_p=F_p/N_p 面亮度归一与 variance/ivar finalize 属 astro_image_io
  下游（`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp` 传出原始累加量后由
  aio_hips_writer finalize_tile 完成，见 DISP-DRZ-007），不在本模块。

## 1 ALG-DRZ-001 核心累加公式（与 SCI-DRZ-001 [S-1]「判据与误差」一节 对照）

- 源锚: `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`
  （`processPixelTiled` / `processPixelSharedTiled` 累加段；
  **符号名优先于行号**）。
- 记号: 源像素 j（值 x_j [ADU]、方差 v_j [ADU²]、权重面）、目标
  HEALPix NESTED leaf p；drop = 源像素按 pixfrac 收缩的球面 footprint；
  A_pixel,j = **未收缩**源像素球面总面积 [sr]（`spherical::polygon_area_consistent`）；
  A_drop,j = drop 球面总面积 [sr]
  （S-H 裁剪前，双精度角点累积，<1e-20 拒绝，
  `build_drop_geometry_into` / `DropGeometryT::drop_area`）。
- 离散公式（逐条源码锚；**单位逐项**与 `docs/science/unified/DATA_SEMANTICS.md` [U-1]「面亮度单位的推导」一节
  的量纲链逐段一致，`FZ-UNIT-SIGNAL-SB` FROZEN）:
  - 权重: `w_jp = a_jp / A_drop,j`，**量纲 = 1（无量纲）**（a_jp 与 A_drop,j 同为 sr）。
    **面亮度保持口径**（SCI-DRZ-001 [S-1]「判据与误差」一节 目标态面亮度保持权重），a_jp = drop ∩ target p
    球面交叠面积 [sr]；`A_drop,j` = drop 球面面积 [sr]（`build_drop_geometry` 的
    `g.drop_area`），由 `spherical::polygon_area_consistent`（收缩四角）求值。
    `pixfrac==1` 时未收缩四角 ≡ drop 四角 ⇒ `A_drop,j ≡ A_pixel,j`（**逐位不变**）。
    w≤0 拒绝。**核按 drop 面积归一**（F&H 2002 **双锚定**：权重更新式 `W' = a·w + W`
    与 s² 因子在 §2 式(2)–(5)；`a` 的逐字定义句 "a_xy is the fractional area overlap
    of the drop of input data pixel d_xy with the output pixel o" 在 §7 式(7) 正下方
    ——**注意：F&H 2002 全文从未陈述 partition of unity**，故 `Σ_o a_io=1` 是**本仓依据 drop
    恰覆盖输入像元、邻接输出像元无空隙这一几何事实作出的推断**，不是原文明述的定理；
    据此推出核按 drop 归一；等价于 drizzlepac
    `cdrizzlebox.c` 的 `dover /= jaco`。
    权重公式（w、a_xy、s² 因子）一手锚 = §2 式(2)–(5)＋§7 式(7) 后定义句双锚；
    §7 式(6)–(10) 的方差/相关（R = σc/σp）另行分列引用）：
    `Σ_p w_jp = 1` ⇒ `Σ_p F_p = Σ_j x_j`，**与 pixfrac 无关**。
    **归一分母（口径不可替换）**：`N_p = Σ_j w_jp·A_pixel,j`（`acc.sumNorm`，`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`）。
    若改用覆盖面积 `D_p=Σ_j a_jp` 作分母，`S_p` 偏 `1/pixfrac²`（pf=0.8 → +56.25%），
    负例判据 `1/pf²−1`（DISP-DRZ-009 回归门的 N 判据）。
    **A_pixel 分母为实现细节（代数等价参数化，不构成第二套口径）**：`w′_jp = a_jp/A_pixel,j
    = pixfrac²·w_jp`，配分母 `Σ_j w′_jp = D_p/pixfrac²` 与 canonical 给出同一个 `c_jp`；
    该构造 `Σ_p w′_jp = pixfrac²`（pf=0.8 ⇒ 0.64），**不满足通量守恒门**，故核权重口径
    一律取 drop 面积归一；`Phi_out` 只有 drop 面积归一口径 = `Σ_j x_j`。
    **适用域**：`A_pixel,j`（面亮度分母用）必须由未收缩四角在同一球面面积例程上求值；
    `A_drop,j = pixfrac²·A_pixel,j` 只在平面（仿射）极限下精确，球面残差
    `δ = A_drop/(pixfrac²·A_pixel) − 1 = (1−pixfrac²)·θ²·[0.25/(1+r_c²) − 0.625·ξ_c²/(1+r_c²)²] + O(θ⁴)`
    （θ = 源像素角尺度 [rad]，(ξ_c,η_c) = 像素中心的 gnomonic 平面坐标 [rad]；
    推导与实测见 §10 DISP-DRZ-009 段）。θ=2″/px、pixfrac=0.8、r_c=0 时 δ=8.46e-12；
    θ=300″/px 时 δ=1.90e-7（与 DISP-009 注入产物实测 1.48e-7 同阶）。故面积面**只采用球面精确式**——
    也因此 `sumNorm` 用 `overlap_area·(pixel_area/drop_area)` 逐源累加，而不是在最后乘 `pixfrac²`。
  - 通量: `F_p = Σ_j x_j · w_jp`，**单位 = ADU**（x_j 为帧平面线性计数 [ADU]，
    w 无量纲；`acc.sumFlux += Scalar(pixelValue * weight)`，`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`）。
    `Σ_p F_p = Σ_j x_j`（drop 面积归一的直接推论，见上）。
  - 面亮度归一分母: `N_p = Σ_j w_jp·A_pixel,j`，**单位 = sr**
    （`acc.sumNorm += Scalar(overlap_area * (pixel_area / drop_area))`）；
    `pixfrac==1` 时 `pixel_area` 与 `drop_area` 是同一变量 ⇒ 比值恒 1.0 ⇒ `sumNorm ≡ sumArea`
    逐位相同（默认路径零回归的代数保证）；产品面 `S_p = F_p/N_p`。
  - `.hiss` signal 面 = `F_p`（**分配通量**，Σ = Σx）；HiPS signal 面 = `F_p·k/A_cov`
    （`k = sumArea/sumNorm`，见 `astro_sphere_sink.cpp`）——两者的差别是覆盖面积口径。
  - 支撑面积: `D_p = Σ_j a_jp`，**单位 = sr**（`acc.sumArea += Scalar(overlap_area)`，
    `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`；**绝对球面面积**，非无量纲覆盖分数；support = D_p/A_cell 无量纲，语义不变）。
  - 方差分子: `sumVarNum_p = Σ_j v_j · w_jp²`，**单位 = ADU²**（v_j 为输入源像素方差
    [ADU²]，`FZ-UNIT-VAR-IN`；`(double)v · (double)w²` 中转再转 Scalar，
    仅当 v>0 累加，`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`）——"仅 v>0"即"方差不可用样本不进入方差项"，
    依据 `docs/science/unified/DATA_SEMANTICS.md`「方差与逆方差的三态编码」一节「无方差信息 ⇒ variance=0 ∧ ivar=0（显式不可用）」
    与 [U-1]「方差与逆方差的三态编码」一节。
    **α² 缩放律（代数恒等）**：`v_j → α²·v_j` ⇒ `sumVarNum_p → α²·sumVarNum_p`
    ⇒ `variance_p → α²·variance_p`（sumVarNum 对 v_j 线性、D_p 与 v 无关），
    ivar 按二次律 `→ α⁻²`；该律是恒等式而非近似，故回归门取逐像素 `worst_rel < 1e-4`
    只受浮点重结合限制（§9 容差行）。
  - 贡献计数: `nContrib_p = Σ_j 1`（**合格样本计数**，计样本级贡献，非权重级；
    与 §5 的 `n_rejected_nonfinite` 是两个不同的量，各自独立取值）。
- **估计量类别（与一手文献对照）**：`S_p = F_p/N_p = Σ_j B_j·a_jp / Σ_j a_jp`
  （分母是**面亮度归一分母** `N_p = Σ_j w_jp·A_pixel,j`，**不是**覆盖面积 `D_p = Σ_j a_jp`；
  两者相差 `κ = A_pixel,j/A_drop,j = 1/pixfrac²`，
  **用 `D_p` 会偏高 pixfrac² 倍，正是 §10 `DISP-DRZ-009` 明令禁用的那一个**）
  （B_j = x_j/A_pixel,j [ADU/sr]）是**输入面亮度的 a_jp 加权平均**。Fruchter & Hook 2002
  （PASP 114, 144；arXiv:astro-ph/9808087v2 §2 式(2)–(5)）的 drizzle 输出为
  `I = Σ_i d_i·a_i·w_i·s² / Σ_i a_i·w_i`（"a factor of s² is introduced to conserve
  surface intensity"），同属"输入值的加权平均"族：两者对常量面亮度场均与 pixfrac 无关，
  差别在权重口径——F&H 用 `a·w`（w = 用户输入权重）并把像素尺度比以 `s²` 显式换算，
  本模块用 `a_jp/A_drop,j`（几何交叠面积的 drop 归一），面亮度由分母
  `N_p=Σ_j w_jp·A_pixel,j` 归一（⇒ 无需 s² 因子），
  分母 D_p 兼作 support 的分子（`support = D_p/A_cell`）。该差异为 Project-defined
  选择，须与 `FZ-UNIT-SIGNAL-SB` 的量纲链一并阅读，实现口径只取本模块 Project-defined 定义。
- **D7-A 代数纠偏：一般情况下不能化成纯交叠面积平均**。原上文把
  S_p=F_p/N_p 无条件写为 Σ_j B_j a_jp/Σ_j a_jp；但当 A_pixel,j/A_drop,j
  随像元、畸变和 pixfrac 变化时，正确展开式是
  S_p = [Σ_j B_j a_jp (A_pixel,j/A_drop,j)] /
        [Σ_j a_jp (A_pixel,j/A_drop,j)]。
  只有所有有效 j 的 A_pixel,j/A_drop,j 对同一目标 p 为**相同常数**（例如
  局部仿射、相同 pixfrac 的受控条件）才可以约掉，成为 Σ B_j a_jp/Σ a_jp。
  因而“pixfrac 的球面面积比只进分母”的说法本身还需配对分子一起理解；
  本条不改 w_jp=a_jp/A_drop,j 与 sumNorm 的生产定义，也不允许把
  support 面的 D_p=Σa_jp 冒充 sumNorm。此纠偏同样适用于原 §10
  DISP-DRZ-009 对该等式的过强简化。
- **S_p = F_p/N_p 归一不在本模块**: sumFlux/sumArea/sumVarNum 原始和
  逐 tile 传出（`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp` dense 化），归一在
  aio_hips_writer finalize_tile（variance = var_num_sum/area²，
  `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`）——与
  `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 锚注释一致。
- 单位/dtype: 累加器 Scalar = float（precision_mode=0）或 double（=1）
  显式模板双实例（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`）；a_jp/面积几何全程
  double，FP32 仅发生在累加存储层（逐项舍入，容差门见[S-1]「判据与误差」一节）。

## 2 几何管线（drop footprint → 候选 → 交叠面积）

- drop 四角: `half = 0.5·pixfrac`，`(x±half, y±half)` 经 WCS/SIP
  `pixelToSky` → 单位 Vec3（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 四角构造；
  pixfrac==1.0 时行级顶点共享缓存，同文件）。
- 收缩语义: 球面 slerp 不做显式插值，四角直接按收缩后平面坐标取
  WCS 映射（pixfrac 收缩在源平面完成）；pixfrac∈(0,1] 严格校验，
  ≤0 或 >1 拒绝不夹逼（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`；文件通道 API 层接受 0.0 的双轨
  差异见 DISP-DRZ-003）。
- 三层候选缓冲（`lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp`）:
  1. quick-reject: `lim = max_angle + 1.25·hp_res`
     （HP_CIRCUMRADIUS_FACTOR=1.25，`lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp`）。**系数依据（可判定上界）**：HEALPix 单元的
     外接半径 `r_circ`（像元中心→像元边界最远点角距）与等面积尺度 `hp_res = sqrt(A_cell)` 之比的
     全天 sup 为 **1.0415**（N=64 全天穷举实测，随 N 自下方单调升收敛；1.25/1.0415 = 1.2002，
     即裕量 ≥20.0%；该上界与裕量的读数正本 = 实验/healpix-polar/
     （REPORT_experiment.md [S-1]「参数与常数」一节 与 docs/DERIVATIONS-P3.md [S-4]「推导补遗」一节）。**另注意区分一个几何常数**：1.1284 = √2·ρ₁/hp_res
     = 2/√π 是"极冠 apex 叶对角（极点→叶远角）/hp_res"的叶对角尺度（ρ₁ = arccos(1−1/(3N²))
     ≈ √(2/3)/N）——外接半径必须按本节定义取值（像元中心→像元边界最远点角距）；相邻 ring 纬度中点法在极冠 apex 叶失效，只作叶对角尺度的取心，不用作外接半径。
     阴性对照：等经纬网格的同度量显著高于该上界（读数与反例见 实验/healpix-polar/docs/DERIVATIONS-P3.md [S-4]「推导补遗」一节）
     ⇒ 1.25 的安全性只对 HEALPix 特化成立，相对该上界留 ≥20.0% 裕量，且覆盖跨
     face 边界（面内畸变已由 `hp_res` 的球面定义吸收）。
     零漏选由 9003 例全枚举 oracle 兜底（§9）。
  2. 保守查询圆: `query_radius = max_angle + 3.0·hp_res`
     （queryDisc 回退）。**系数依据**：候选完备性要求
     `query_radius ≥ max_angle + r_circ,max = max_angle + 1.0415·hp_res`（任一与 drop 相交
     的 cell，其中心必落在 drop 的 `max_angle` 邻域加该 cell 外接半径之内）⇒
     3.0·hp_res 比 1.0415·hp_res 宽 2.88×，属**保守过覆盖**（代价是候选数，不是正确性）；
     （3.0/1.0415 = 2.88×，属保守过覆盖）
  3. fast 路径: 面 delta×1.15 面内畸变系数 + 极冠/跨 face 边界回退
     queryDisc（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`）。
- 交叠面积: 球面多边形裁剪（clip_normals_d，内部 double）——逐边裁剪的球面推广（Sutherland–Hodgman 1974 原文只处理平面多边形，球面形式为本模块推广）
  + **Van Oosterom & Strackee 扇形三角剖分**有向面积 + 半球包含检查
  （max_ang ≥ π/2−1e-12 → NAN，`lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp`）。本模块面积算法的唯一命名 =
  Sutherland–Hodgman 球面逐边裁剪 + **Van Oosterom & Strackee 扇形三角剖分**；
  **禁用** "Girard 定理"（内角和式）这一命名——本模块无该实现。
- 目标几何缓存: per-thread LRU 8192（TargetGeomCache），hit 复用
  center+boundary4；per-run generation 原子递增清空
  （`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 的 `s_target_cache_gen.fetch_add`；
  **禁用**裸 static 可变状态——会引入 data race）。
- HEALPix 地址: 仅 NESTED；`parent = ipix >> 2·d`、
  `local = ipix & (4^d − 1)` 位分解（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`）；候选枚举 Morton
  spread 位交织（`lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp`）；shim
  `lib/algorithms/shared/healpix/healpix_core.h` 转发 acsd::healpix::ang2pix_nest，RING 直接拒绝。

## 3 auto nside 决策（compute_auto_nside）

- 源锚: `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 的 `compute_auto_nside`（符号名优先）。
- `HEALPIX_SCALE_PER_NSIDE_ARCSEC = sqrt(π/3)·(180/π)·3600 = 211076.28514206142″`
  （`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`；**该常数是 HEALPix 等面积基数的精确开方**，不是近似：
  `sqrt(4π/(12·nside²))·(180/π)·3600 ≡ sqrt(π/3)/nside·(180/π)·3600`，逐位恒等，
  依据 Górski et al. 2005, ApJ 622, 759 §4「all pixels have exactly equal area
  `4π/(12·nside²)`」；该恒等式在多档 nside 上的核对证据见 实验/healpix-polar/）。
  **数值纪律**：常数按上式**求值**，取值来源 = 求值结果；`211034.6` 与上式相差
  −1.97e-4（相对），会使 `nside` 决策在每个倍频程的
  `finest ∈ (211034.6/2^k, 211076.285/2^k)` 窗口内**少取一阶**
  （窗口相对宽度 1.97e-4，20 个倍频程全部存在；实测例：finest=105527.7″ 时
  上式给 nside=4、抄写值给 nside=2 ⇒ 像素尺度差 2 倍，破坏 1–2× 过采样目标）。
- 决策: 最小 2 次幂 ≥ 211076.28514206142/finest_arcsec（判据式
  `hp_res_arcsec = C/nside ≤ finest_arcsec`，`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`）；无效输入（finest≤0 等）
  → 0 由调用方兜底（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`）；钳位 `[NSIDE_MIN, NSIDE_MAX] = [16, 4194304=2^22]`
  （同文件）。**钳位下界 16 的适用域**：nside=16 对应等面积尺度
  `211076.285/16 = 13192.3″ ≈ 3.66°`，即该函数可表达的最粗叶级；更粗的请求被钳到 16，
  此时 `oversample = hp_res/finest > 1`（欠采样）——消费方必须读 `oversample` 字段判欠采样，
  判据输入 = `oversample` 字段，`nside` 单独不构成判据（该函数输出该字段）。
  **适用域**：判据量是**等面积等效线尺度** `sqrt(A_cell)`（面积是常数），
  而 HEALPix 单元的**局部采样步长**（邻元中心角距）随纬度与方向变化：
  nside=512 实测邻元步长 ∈ [0.63, 0.71]×`sqrt(A_cell)`（共边邻元）与
  [1.95, 2.94]×`sqrt(A_cell)`（对角邻元），各向异性最大 ≈4.6×
  （实验锚同上，nside=512/1024 双档）。故"过采样 1–2×"是按等面积尺度
  定义的**面平均**口径；要求**方向性**分辨率保证的消费方必须按局部步长另加余量。
- orchestrator 侧策略 `1x_to_2x_drizzle`/`fixed` 最终映射到本函数
  （`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp`）。

## 4 反向 drizzle（Sphere→Plane，REV-101..107）

- 源锚: `lib/algorithms/drizzle/healpix_drizzle/reverse_drizzle.h` / `lib/algorithms/drizzle/healpix_drizzle/reverse_drizzle.cpp`。
- 语义: 每 source leaf 构造球面 footprint（边界自适应细分），pixfrac
  沿球面向 leaf 中心收缩；目标平面像素经 WCS/SIP 映射为球面
  footprint；重叠面积 = 球面面积（权重一律取球面面积，平面 2D 面积只作几何参照）；signal
  按球面面积比例分摊；coverage 以"覆盖在 leaf 内均匀分布"假设输出。
- 严格校验（`lib/algorithms/drizzle/healpix_drizzle/reverse_drizzle.cpp`）: nside 2 的幂 [1,2^22]；
  仅 NESTED；宽高>0；pixfrac∈(0,1]；CD 行列式有限且 |det|≥1e-30、
  |cd[k]|≤1 deg/px；crval dec∈[-90,90]；f32/f64 signal 二选一严格；
  support∈[0,1]（空=全 1.0）；ipix 越界/重复拒绝。
- 能力/版本: hp_drizzle_reverse_capability 位 0x01|0x02|0x04|0x08|
  0x10|0x20（`lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp`）；version "1.0.0"（同文件）。

## 5 输入校验与 NaN/Inf/invalid 边界

| 条件 | 行为 | 锚 |
|---|---|---|
| pixfrac ≤0 或 >1（引擎层） | 拒绝（不夹逼） | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` |
| pixfrac==0.0（文件通道 API 层） | 放行后进引擎再拒（双轨） | `lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp`（DISP-DRZ-003） |
| RING（nested=0） | 硬拒绝 | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`；shim throw |
| channels≠1 多通道 | 拒绝 | 同上 |
| 缺 WCS（CD 与 CDELT+CROTA2 均无） | 拒绝（帧通道返回 -9） | `lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp` |
| 尺寸/空指针非法 | 拒绝 | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` |
| **值像素 NaN/Inf** | 按 `rule_id NAN-SAMPLE-MASK-COVERAGE-NAN` 处置 = **样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数**（唯一口径文字 = `docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md`「组成块」一节的聚合算子处置规则）：不合格样本从 `F_p`、分母、方差三项一并剔除并重新归一，仅零合格样本输出 `NaN ∧ support≤0`，每个输出像素必须暴露被剔除样本计数 `n_rejected_nonfinite`（按原因分类、互斥可加）。**实现锚**：`!std::isfinite(pixelValue) → ++tc.rejected_nonfinite_value; continue`（`DrizzleEngine::drizzleTiledImpl` 主循环）；分类计数聚合为 `DrizzleStats::n_rejected_nonfinite{,_value,_variance,_nonpositive_weight}`（同文件）。 | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` |
| SNR 面非有限 | 计入 `rejected_nonfinite_value` 同族掩膜路径（SNR 面参与权重/有效性判定，剔除项逐条计数登记） | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` |
| 权重面非有限或 ≤0 | 计入 `rejected_nonpositive_weight`（原因 3）后剔除该样本（**必须计数**，禁静默） | 同上 |
| variance 面非有限（NaN/Inf） | 计入 `rejected_nonfinite_variance`（原因 2）后剔除该样本（**必须计数**，禁静默） | 同上 |
| **variance 面 = 0（方差不可用）** | **不是无效像素**：样本合格性只判 `isfinite(x_j)`（DATA-002 [S-1]「判据与误差」一节）；variance=0 按 `docs/science/unified/DATA_SEMANTICS.md`「方差与逆方差的三态编码」一节「无覆盖/无方差信息像素写 variance=0 且 ivar=0（显式不可用）」与 [U-1]「方差与逆方差的三态编码」一节处理 ⇒ **只令方差项为 0，不丢信号、不丢几何支撑**。`V_j ≤ 0` 不构成掩膜授权（`ACSD_DESIGN.md` [D-1]「逐像素排异」一节只授权对NaN做样本级掩膜） | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` V≤0 分支（置 0 不 continue）；证据见 实验/absolute-snr/（方差语义面）|
| 几何 NaN（ra/dec 非有限） | 显式拒绝该像素 | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` |
| 半球检查失败（max_ang≥π/2） | 返回 NAN 面积 | `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp` |
| A_drop<1e-20 / w≤0 | 拒绝 | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` |
| nside 非法（shim 构造） | 容忍不抛，校验责任在调用方（缺陷候选） | `lib/algorithms/shared/healpix/healpix_core.h` |
| reverse 输入非法 | 非 0 返回码，逐项校验 | `lib/algorithms/drizzle/healpix_drizzle/reverse_drizzle.cpp` |

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
  lease（根 `CMakeLists.txt` 注释）；ThreadLease 迁移必须保持本节合并序。

## 7 复杂度与内存

- 时间: O(n_source · avg_candidates)，avg≈3.5（小图实测，9003 例
  oracle 枚举）；fast 路径圆心距预过滤降低 S-H 调用。
- 内存: tile 累加器 leaf 连续数组 O(1) 寻址（禁 per-leaf 全局 map，
  `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 注释）；target 缓存 per-thread bounded 8192 LRU；单交集
  O(顶点数≤8) 无整帧副本；SNR 控制点 RAII vector（`lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp`，
  已消除 free 泄漏）。

## 8 数据布局

- Tile: `TileAccumulatorT<Scalar>{parent_ipix, touched[], pixels[4^d]}`
  ；`TileLeafAccumulatorT<Scalar>{sumFlux, sumArea, sumVarNum,
  nContrib}`（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.h`；release 注释"3 字段"与实际 4
  字段不符，DISP-DRZ-006）。
- HiPS 直写: tile_depth 必须 =9、nside≥512（`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`），
  leaf tile 512×512 1:1，产品 SIGNAL/SUPPORT + V19 variance/ivar
  （有 variance 输入时）；provenance 写 pixfrac/源像素尺度。
- HissWriter 流式（writeHisTilesT，`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` finalize），
  测光 gate 同文件；operation_counts.json 剖面
  （`lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp`）。
- **累加精度 provenance（无 silent 缺省）**: 累加精度由
  `drizzle.precision_mode`（整数 0=FP32 / 1=FP64）显式给出；缺失或非整数
  → DATA 拒绝（不 silent 降 FP32）。节点写 p1_stack.json `precision_mode`、
  帧头 `PRECISION`=fp32/fp64 实际值（`lib/infrastructure/scheduler/src/module_adapters.cpp`）。
- **库边界精度缺省（无 silent FP32）**: `hp_drizzle_run` 参数
  `precision_mode==-1` 且帧头无 `PRECISION` KV 时取 **FP64**
  （现行载体 = 本条；数据面同源 =
  `docs/detail/normalize/modules.md` 的「PRECISION」行；**语义不变**）
  （不再静默 FP32）；帧头显式 `fp32`/`fp64` 生效；未知 KV 值或参数
  非 -1/0/1 → 显式拒绝（返回非零，不写产物）。生产节点仍由
  `drizzle.precision_mode`（0|1）显式门把关。
  （`lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp`；回归判据 = 精度缺省面）
- **测光 provenance（禁硬编码 PHOTAPPL=1）**: PHOTSCAL/PHOTAPPL
  由真实测光 provenance `p1_phot.json`（DATA-P1-PHOTPROV-001，
  `p1_op_photometry` 产出）决定；未应用测光 → PHOTAPPL=0 + 帧头
  `PHOTDEGRADE=1`，引擎在显式声明时降级写 BUNIT=ADU（photappl=0），
  未显式声明仍按 02_FROZEN [S-1]「参考文献与参考代码」一节 拒绝（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`；
  `lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp`；`lib/infrastructure/scheduler/src/module_adapters.cpp`）。
- 输入通道: PipelineFrame "data"（f32/f64 二选一，bzero=0/bscale=1
  固定，`lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp`）+ header WCS/SIP KV + 可选 "snr_model" 块
  （KD-tree IDW 重建逐像素 SNR，snr_evaluator.h）。
- **SIP 桥接**：编排 drizzle 节点从上游
  `p1_wcs.json`（DATA-P1-WCS定义）读回 `wcs.sip`（order/ap_order/a/b/ap/bp，
  i*6+j），经 `p1_sip_write_header_frame` 写入 frame header 的 FITS 键：

  ```text
  CTYPE1/2 带 "-SIP" 后缀
  A_ORDER / B_ORDER
  A_i_j / B_i_j
  AP_* / BP_*
  ```

  无 SIP → CTYPE 不含 `-SIP` 且不写任何 SIP 键。reverse 通道 sip_order
  校验 [0,5]（DISP-DRZ-001）；`p1_stack.json` 记录 sip_present/sip_order/
  ctype1/ctype2 作为"实际下发"证据（`lib/infrastructure/scheduler/src/module_adapters.cpp` p1_op_drizzle）。

## 9 TEST-DRZ-DESIGN-001（测试设计冻结，可执行 TEST-P1-DRZ-001 由 P1-DRZ-TEST 建立）

- fixture（全合成、离线、零真实数据）:
  - FIX-DRZ-A 常量场（面亮度语义 oracle：每像素常量 ADU ⇒ S=C/A_drop
    ≠C，检验 D_p 语义）；FIX-DRZ-B 点源高斯（总通量守恒）；
    FIX-DRZ-C 梯度场；FIX-DRZ-D 脉冲单像素；FIX-DRZ-E NaN/Inf 注入面；
    FIX-DRZ-F SIP 畸变边缘 patch（15° 宽场）。
- 冻结容差（预冻结，源自既有测试门，取值一律按本节）:
  - **逐 leaf 面积/权重相对误差门（验收主判据）**：逐 leaf（目标叶）交叠面积与核权重
    相对误差 <1e-5（drizzle_freeze_test.cpp 硬门；l0 小图 FP64<1e-10）；
  - FP64 通量闭合（辅判据，只证总量守恒）：逐 drop ≤ max(1e-6·A_drop, 1e-15 sr)、
    帧级 ≤1e-4，逐位置集分别判定，位置集必须含极点与 u+v=1 face 接缝（判据、绝对项
    来源与适用域见本节「面积守恒闭合」条；位置集与逐组判定分别出读数）；
    求和型判据对逐叶错注入无判别力，
    只作辅判据，不作验收判据；
  - 方差 α² 缩放律逐像素 worst_rel <1e-4（variance_propagation_test，
    SCI-DRZ-014）；
  - 候选零漏选: 9003 例 false_negative=0（candidate_oracle_test，
    4 pixfrac × 5 尺度 × 7 nside 全枚举，RA 跨 0/极区/face 边界）；
  - reverse: false_hole=false_fill=0、coverage∈[0,1]、常数场
    uniform_rel_std<1e-4、半图/全图 total_in 比 <1e-6；
  - 面积误差预算（按叶侧分列，只按本叶侧引用）:
    - 赤道带（等纬度小圆边）：arc-chord 预算 1e-6·hp_res —— 该预算只对
      这类边成立，且仅在自适应细分深度 ≥9（生产 max_depth=12）时成立；无细分弦
      表示下各带的亏缺量级正本 = 实验/healpix-polar/（REPORT_experiment.md）；
    - 极冠边（β_0 < 2/3 两侧的等纬度边与经线边）：**不适用**上述相对
      hp_res 预算。叶边界在 nside≥256 由 4 角弦表示（生产路径），经过
      极点 apex 的叶其"弦/真"相对亏缺 ≥(1−2√2/π)（与 N 无关，N≳64 后
      已收敛），比 1e-6·hp_res 的弦长预算大 4 个数量级；故极冠叶的
      表示误差一律按**叶面积比**度量，并按「面积守恒闭合」的逐 drop
      绝对项预算验收；1e-6·hp_res 只对赤道带这类边成立，与极冠叶是否达标无关。
    - **4 角弦四边形的亏缺闭式与系数（本节为唯一数值正本）**：相对亏缺极限
      `2√2/π − 1 = −9.968368384e-02`（**解析闭式，不随 nside 收缩**）；
      绝对亏缺 `π/3·(1−2√2/π)·N⁻² = 0.1043885·N⁻²`。
- 不变量: 常量场均匀性（无接缝/系统性亮斑，drizzle_freeze_test）；
  NESTED 地址往返；1/2/4 线程一致性；HISS 写读往返；负值保持；
  multi-FWHM 孔径测光 4σ 相对差 <1e-3。
- 负面矩阵: §5 表逐行断言（pixfrac 0/负/>1、RING、多通道、缺 WCS、
  NaN 面、reverse 二选一/越界/重复 ipix）。
- **面积守恒闭合（逐 drop 判据与位置集）**：
  - 判据：逐 drop |Σ_j a_jp − A_drop,p| ≤ max(τ_rel·A_drop,p, ε_abs)，
    τ_rel = 1e-6、ε_abs = 1e-15 sr；帧级 |Σ_p (Σ_j a_jp − A_drop,p)| /
    Σ_p A_drop,p ≤ 1e-4。A_drop,p 为 drop 四角球面多边形面积（与生产
    drop_area 同口径的独立复算，不复用被测路径）。
  - 位置集（必须齐备，缺一即门不成立）：极点与其邻域、**u+v=1 face
    接缝**（即 |z|=2/3 折线）、极冠内部、face 角点两侧、同纬度三角点、
    赤道对照；另需含 HST 真实尺度（0.04″/px、nside=2^23）。
  - 判据必须逐位置集分别判定并打印（全局汇总通过不等于各位置集通过）；
    负例（注入订正前"叶顶点全含 ⇒ 返回解析叶面积 π/(3N²)"行为）时
    极点/极冠组必须判红、赤道对照组必须仍绿。三条判据臂 =
    守恒判据 / 自检判据 / legacy 注入判据。
  - **绝对项 ε_abs 的来源与适用域**：叶边界在 nside≥256 由 4 角弦表示、
    相邻叶边界与 drop 边的近退化大圆求交在 A_drop ≲1e-16 sr 尺度上
    留下 ~1e-16 sr 的绝对残差；该残差与 drop 尺寸弱相关（实测 drop
    0.005″→0.2″/px 期间最坏 4.3e-17…1.5e-16 sr）。把同一组 double 顶点送进
    **同一裁剪算法**的 long double 版本，接缝地板不降（long double/生产 =
    0.87/0.96/0.91；赤道对照虽 15× 但其绝对量 6e-22 sr 无意义）⇒ 它是
    **几何/表示地板，不是算术精度地板**（算术项另设锁：同一组裁剪
    顶点下 double 与 long double 累加之差须 ≤1e-8，见该门）。要移动该
    地板须把叶边界生成与面积记账一并提到扩展精度表示（改变冻结口径，
    需变更流程），仅提高中间量精度无效。
    适用域：相对项在极点（实测最坏相对 2.2e-13）与赤道（2.8e-9）等
    位置可达；**相对项在接缝不可达**——u+v=1 接缝的相对闭合 ∝1/A_drop，
    0.04″/px 实测 3.9e-3、0.005″/px 实测 9.9e-2，故接缝与任何
    A_drop ≲1e-9 sr 的域一律以绝对项判，相对门对该域不适用。

## 10 DISP-DRZ-001..009（SCI/文档 vs 源码口径清单；修复方向 = 从 SCI 到实现）

| # | 文档声称 | 源码实际 | 双方锚 |
|---|---|---|---|
| DISP-DRZ-001 | `lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h` 注释 sip_order "0..4" | `lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp` 校验 [0,5]（6×6 系数组支持 5 阶下标） | 头注释 vs 实现的校验段 |
| DISP-DRZ-002 | 源码注释 `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.h` / `spherical_overlap.cpp` 写 "Girard 定理" | 面积实现 = S-H 球面裁剪 + Van Oosterom & Strackee 扇形三角剖分，无 Girard 实现；文档侧命名已与实现一致，**禁用** "Girard 定理" 命名 | 头/源注释 vs 面积实现 |
| DISP-DRZ-003 | pixfrac∈(0,1] 单一边界 | 文件通道 API 层接受 0.0（<0 才拒），引擎层拒绝——两层双轨 | `lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp` vs `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` |
| DISP-DRZ-004 | 值像素 NaN 按 `rule_id NAN-SAMPLE-MASK-COVERAGE-NAN` 处置 = 样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数（`docs/science/drizzle/DRIZZLE.md`）：不合格样本剔除并重归一、仅零合格样本输出 `NaN ∧ support≤0`、必须暴露 `n_rejected_nonfinite` | **约束**：主循环按原因分类计数（值/方差/权重三分类）并聚合暴露 `DrizzleStats::n_rejected_nonfinite*`（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`）；**禁用**把非有限样本传播进 `F_p`/分母/方差——会污染整像素信号与几何支撑（负例判据：零合格样本必须输出 `NaN ∧ support≤0` 且分类计数非零） | `docs/science/drizzle/DRIZZLE.md` vs `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` |
| DISP-DRZ-005 | `max_angle < 1e-3` 切平面分支是**实际执行路径，必须保留**：微小 drop（角跨度 < 1e-3 rad ≈ 206″）用切平面面积 | 三处活分支（均在 `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp`）：`g.drop_area` 微小 drop 用切平面面积、nb=4 重叠 `<1e-3` 用 `planar_polygon_area_n`（否则球面 `spherical_polygon_area_n`）、三角形扇重叠同策略（与 g.drop_area 表示一致，避免 weight 偏差） | 同文件的三处活分支与其注释；θ=1e-3 时切平面偏差 ≈ −θ_max²/2 = −5.0e-7（恒负、单向下偏；θ_max 按 drop 最远顶点角距约定；旧注 "<4e-8" 缺符号且偏小 12.5 倍，撤换），球面 double 相消噪声 ~1e-4~5e-5 |
| DISP-DRZ-006 | TileLeafAccumulatorT release 仅 3 字段（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.h` 注释） | 实际 4 字段（sumVarNum 为正式产品） | 同一头文件的注释 vs 结构体定义 |
| DISP-DRZ-007 | SCI[S-1]「判据与误差」一节 方差锚指向 `drizzle_engine.cpp`（该锚所在段已随引擎重构消失） | 锚漂移：现行方差锚在 `astro_sphere_sink.cpp` + aio_hips_writer finalize_tile | `docs/science/drizzle/DRIZZLE.md` vs `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 的锚注释 |
| DISP-DRZ-008 | poly_clip.h 自述生产重叠面积用途 | PolyClip（平面 S-H/Shoelace）生产 tiled 路径零调用 | poly_clip.h vs `drizzle_engine.cpp` 全文 |
| DISP-DRZ-009 | SCI-DRZ-001 [S-1]「判据与误差」一节 canonical 核 `w_jp=a_jp/A_drop,j` + 面亮度归一分母 `N_p=Σ_j w_jp·A_pixel,j`（`S_p=F_p/N_p=Σ_j B_j a_jp/Σ_j a_jp`） | **约束（覆盖正向与反向两条路径）**。**正向** `processPixelSharedTiled`：`weight = overlap_area / drop_area`（drop 面积归一，F&H 2002 §2 式(2)–(5)（原引 §7.2 撤换）/ drizzlepac `dover/=jaco`），归一分母 `acc.sumNorm`。**反向** `reverse_drizzle.cpp`：`S_p = Σ_j B_j·a_jp / Σ_j a_jp`（面亮度加权平均，分子分母各自累加后相除）。**禁用**把正向分母换成覆盖面积 `D_p=Σ_j a_jp`：`pixfrac<1` 时偏 `1/pixfrac²`（pf=0.8→+56.25%，与 `1/pf²−1` 逐位吻合）；反向路径若改用非面亮度归一，输出随**源 nside** 变化（实测 nside=16/64/256 偏 −99.98%/−99.70%/−95.23%，`S_p` 与 `B0·A_pixel/A_leaf` 逐位吻合）——两者均为负例判据 | docs/science/drizzle/DRIZZLE.md [S-1]「判据与误差」一节 vs drizzle_engine.cpp `processPixelSharedTiled`（`weight=overlap_area/drop_area` + `sumNorm`）/ `astro_sphere_sink.cpp`（`k=sumArea/sumNorm`）/ reverse_drizzle.cpp（`weight` 累加 + 输出归一）/ `spherical_overlap.cpp` 的 `polygon_area_consistent`；契约 `FZ-FORMULA-DRIZZLE-SB`（docs/science/unified/DATA_SEMANTICS.md [U-1]「面亮度单位的推导」一节 面亮度单位推导；输出面见 `docs/detail/normalize/modules.md`）；回归门 `p1drz_disp009` |

无差异项（核对通过）: F/D/sumVarNum 结构、HP_CIRCUMRADIUS
_FACTOR=1.25、三层缓冲语义、NESTED 统一、按线程序合并确定性。

**面亮度保持权重的实现口径**：
`drizzle_engine.cpp` 的 `processPixelSharedTiled` 内**核权重恒为**
`weight = overlap_area / drop_area`（即 canonical `w_jp = a_jp/A_drop,j`，drop 面积归一，同文件）。
**未收缩**源像素球面面积 `pixel_area`（`spherical::polygon_area_consistent`，与
`build_drop_geometry_into` 的 `drop_area` 同一分支同一例程）**只进面亮度归一分母**：
`sumNorm += overlap_area·(pixel_area/drop_area)`，即 `N_p = Σ_j w_jp·A_pixel,j`；
`pixfrac<1` 时由调用方（`processPixelTiled` 的 Step 2b）补 4 次未收缩四角 `pixelToSky`；
`pixfrac==1` 时传 nullptr ⇒ `pixel_area` 与 `drop_area` 为同一变量、比值恒 1.0，
产物**逐字节不变**（默认路径与分母取 `drop_area` 时逐字节相同 ⇒ 零回归）。`sumArea`（D_p=Σa_jp，
support 语义）与 `sumVarNum`/`variance=sumVarNum/sumArea²` 语义不变。
**A_pixel 归一（代数对照，不构成第二套口径）**：`w′_jp = overlap_area/pixel_area = pixfrac²·w_jp`，
`Σ_p w′_jp = pixfrac²`（pf=0.8 ⇒ 0.64），**不满足通量守恒门**。
**注意**：近似写法 `overlap_area·pixfrac²/drop_area` **只作对照，权重口径 = 球面精确交叠面积** ——
恒等式 `A_drop,j = pixfrac²·A_pixel,j` 只在平面（仿射）极限下精确。**残差标度律**
（二阶展开 + Gauss-Legendre 求积独立复算，证据见 实验/healpix-polar/
REPORT_experiment.md 对应节；两条数值路径与解析式在 |δ|>1e-13 域内的一致性由该单元给出）：

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
θ=300″→4.4e-8，随角点舍入符号变化），量级与 δ 不同源；文档与实现**把两者各自独立成项，不**
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
落在注释论证的偏差带内（切平面 vs 球面偏差按 −θ_max²/2 律：θ_max=1e-3 ⇒ −5.0e-7——旧注 4e-8 撤换；见 `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`），
守护该分支在后续迁移中不被误删或语义漂移。

## 11 关联

- SCI: SCI-DRZ-001（docs/science/drizzle/DRIZZLE.md，FROZEN；[S-1]「判据与误差」一节 公式、[S-1]「参考文献与参考代码」一节
  不变量、[S-1]「判据与误差」一节验收；集合 SCI-DRZ-014 方差传播 / 015 支撑 /
  016 协方差）。
- DATA: DATA-P1-DRZ（docs/detail/normalize/modules.md）；上游
  DATA-P1-CAL（[S-5]「输入输出端口」一节）；编排现状引用 DATA-P1-STACK（descriptor）。
- API: API-DRZ-001（docs/engineering/api/PUBLIC_API.md）；API-P1-007
  （docs/engineering/api/PUBLIC_API.md，区间 API-P1-001..010 编排合同）。
- MOD/SRC: MOD-acsd-phase1-drizzle（lib/algorithms/drizzle/module.yaml，
  CONTRACT_READY；lib/algorithms/drizzle/README.md 实现事实）；SRC-DRZ-001
  （lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h 等签名源）。
- TEST: TEST-DRZ-DESIGN-001（本文档[S-1]「公式与推导」一节）；可执行 TEST-P1-DRZ-001
  由 P1-DRZ-TEST 建立。

> 本文引用上游正本（论文式编号，正文引用处均已改为自然语言节名，不再使用跨文档 §N 跳转）：
> - [D-1] docs/ACSD_DESIGN.md（最高设计）。
> - [S-1] docs/science/drizzle/DRIZZLE.md（守恒映射科学正本 SCI-DRZ-001）。
> - [S-4] docs/DERIVATIONS-P3.md（相关科学正本）。
> - [S-5] docs/detail/normalize/modules.md（相关科学正本）。
> - [U-1] docs/science/unified/DATA_SEMANTICS.md（数据语义正本）。

## 参考文献与参考代码库（含许可证）

- Drizzle 线性重建/drop/pixfrac：Fruchter & Hook 2002, PASP 114, 144（DOI 10.1086/338393；arXiv:astro-ph/9808087**v2**——全文锚一律按 v2 打开，v1 是完全不同的短文，无 s² 项、无 §7 噪声节）。**锚分列**：权重公式（w、a_xy、s²）= §2 式(2)–(5)；方差/相关（R = σc/σp、单输出像素方差）= §7 式(6)–(10)。**差异**：原文只说 "input/output pixel plane"，**未声明其在切平面/gnomonic 平面**（v2 源码 `tangent|gnomonic|spherical|celestial` 零命中）⇒「原式在切平面」属过度声称；本模块在球面 HEALPix 上实施是 Project-defined 迁移。
- Drizzle 实践：DrizzlePac Handbook（STScI）；drizzlepac（BSD-3-Clause）。
- HEALPix 几何：Górski et al. 2005, ApJ 622, 759（DOI 10.1086/427976）；astropy-healpix（BSD-3-Clause）、healpy（GPL-2.0）。
- 球面三角面积：Van Oosterom & Strackee 1983, IEEE TBME 30, 125（DOI 10.1109/TBME.1983.325207）。
- 多边形裁剪：Sutherland & Hodgman 1974, “Reentrant Polygon Clipping”, Comm. ACM 17, 32（DOI 10.1145/360767.360802）——**平面**算法（原文摘要为 “plane-faced volumes”）；本模块的球面逐边裁剪是它的推广，不属原文结论。


---

## D7-A｜P1 真实 HEALPix 曲边与面积/通量双重真值合同（2026-10-17）

**适用阶段**：本篇即使文件路径处在 science/algorithms/export/，**它的科学对象始终是 Stage1 Normalize 的 spherical HEALPix forward Drizzle**。不得将本篇的 hp leaf 几何多边形裁剪、quad 分派与 Stage3 从单 Signal HiPS 到 FITS 的 nearest/bilinear 定义成同一个 reproject 算法。原始 63eccbb6133aef56389641d43c55df6e243819c5 完整长卷为 469 行/44423 B、blob a432fd9915e44994a5450ff2f9dac5ddac9d7c3d；本 D7-A 保留 §0–11 原 drop/面积/方差/精度/auto nside、候选完备性、SIP、反向 drizzle、九项 DISP、算术与实验设计、文献和 ABI 的技术内容，并单独订正 #15/#9/#88 等当前科学对象和真值适用域。当前源基线 main d35bfc501d1cf257eb9267a571f1061c0c1fe27a，生产锚是 drizzle_engine.cpp::processPixelSharedTiled、spherical_overlap.cpp::compute_overlap_area_g_ctx_cached / TargetGeomCache::get_or_build、hp_drizzle_api.h、astro_sphere_sink.cpp 和 aio_hips_writer.cpp；当前 P3 是 [投影/WCS合同](PHASE3_PROJ_IMPL.md)、[P3 正本](../../projection/HIPS_TO_FITS.md)。

### D7-A.1 四种不同物理命题，不能以其中一个替代另一个

记输入源像元未收缩球面 footprint P_j，面积 A_pixel,j [sr]，pixfrac 收缩 footprint D_j、面积 A_drop,j [sr]；真实 HEALPix NESTED 叶 H_p 具有**曲线边界**且精确等面积 A_H=4π/(12nside²) [sr]；真实交叠 a_jp=area(D_j∩H_p) [sr]。当前高 nside 的实现实际上是叶四角顶点大圆弧图形 Htilde_p，相交返回 atilde_jp=area(Dtilde_j∩Htilde_p)，其中 Dtilde_j 亦以源码当前 WCS/SIP 四角 footprint 表示；即使源 drop WCS 非线性，还要另计“真实源边界 vs 四角近似”的误差，不能只纠正 HEALPix 叶一侧便宣称全部精确。

| 判据 | 定义与能证明的对象 | 不能证明的内容 |
|---|---|---|
| 数值/代数闭合 | 例如 Σ_p atilde_jp≈Atilde_drop,j，或 Σ_p weight_jp≈1；含未覆盖/漏选的显式误差项 | 因为相交双方都基于替代叶，这个等式**不能说明**真正叶 H_p 归属正确 |
| 常数面亮度 | 对 B_j=B0，使用配对 F_p/N_p 保持 B0 | 不能说明尖锐星点 PSF/局部 gradient 或 leaf membership 的误差为零 |
| 真几何 | 对每个 leaf，epsilon_curve_p=(area(Htilde_p)−A_H)/A_H；每 drop 比较 a_jp 与独立真实曲边求积所得 a_ref | 不能单独推出已测星点总通量亏损比例 |
| 科学场景 | 输出 SNR、星核/积分孔径、弱扩展源、绝对 flux、噪声相关、coverage 与结构保真 | 不能由一个几何面积负例或源图单值误差简单外推 |

**#15 已知静态反例**：当 nside≥256 时，spherical_overlap.cpp::compute_overlap_area_g_ctx 的高阶分支调用 get_healpix_boundary4；**真实调用的缓存路径** TargetGeomCache::get_or_build 对所有高阶目标叶缓存 boundary4，compute_overlap_area_g_ctx_cached→overlap_area_impl(...,boundary4,4)。get_healpix_boundary_sampled 自身在 Ns≥256 同样提前返回四角。HEALPix 真叶边是投影参数曲线，不是四条大圆弧；对极冠 apex 型叶已有相对叶面积反例 epsilon_curve≈**−9.968368384%**（解析极限 2sqrt(2)/π−1），这个比例在高阶不必向零。**该读数仅对特定叶四角区域相对真实叶的面积差负责**：并非所有叶统一偏差，更不是“已证明 M42/FITS 星点通量少9.968%”。同样，旧 §9 的面积闭合 1e−6 或 absolute 1e−15 sr 是**分区自洽测试**，不能作为真实曲边 1e−5 相对 oracle 的替代；eps_abs 对 tiny drop 还会使巨大相对错配被忽略，须报告面积与真值的分母。

### D7-A.2 正确 w/F/N/D 的量纲与光度恒等式

真实科学目标的当前生产计算在给定原始图像像元通量 x_j [L]、面亮度 B_j=x_j/A_pixel,j [L/sr]、参考源几何变换之后为：

\[
w_{jp}=\frac{\tilde a_{jp}}{\tilde A_{\mathrm{drop},j}}\quad[1],\qquad
F_p=\sum_j x_jw_{jp}\quad[L],\qquad
N_p=\sum_j A_{\mathrm{pixel},j}w_{jp}\quad[\mathrm{sr}],
\]
\[
D_p=\sum_j\tilde a_{jp}\quad[\mathrm{sr}],\qquad
S_p=\frac{F_p}{N_p}\quad[L/\mathrm{sr}],\qquad
V_{F,p}^{\mathrm{ind}}=\sum_jV_{x,j}w_{jp}^{2}\quad[L^{2}].
\]
当前源码把 overlap_area/drop_area 作为权重，sumFlux、sumNorm、sumArea、sumVarNum 分别累计；后续 HiPS writer/sink 依据本层数据形成面亮度与 support。它不以 N_p=D_p 作为通用恒等式。真实（非近似）公式只需将所有带波浪号的交叠与面积换为独立真值；由相同叶/源定义给定，未覆盖或数据无效像元须单独报告。

**原文代数勘误**：
\[
 S_p=\frac{\sum_j B_j\,a_{jp}\,(A_{\rm pixel,j}/A_{\rm drop,j})}
           {\sum_j a_{jp}\,(A_{\rm pixel,j}/A_{\rm drop,j})}.
\]
仅当 \(\kappa_j=A_{\mathrm{pixel},j}/A_{\mathrm{drop},j}\) 为同一有效目标叶上的常数，才化简为 \(\sum B_ja_{jp}/\sum a_{jp}\)。原 §1 和 DISP-009 将其写为无条件恒等会让应用者误以为 “drop面积比只在分母起作用”；既定代码中 numerator 的 x_j 与 A_pixel,j 正好通过 w 形成配对。对 pixfrac<1，面积近似 \(A_{\mathrm{drop}}\simeq{\rm pixfrac}^2 A_{\mathrm{pixel}}\) 是局部仿射近似而非所有球面位置的真恒等；旧文的二阶 δ 展开应按其推导适用域阅读。**若把 N_p 改为 D_p**，在近似常 κ=1/pixfrac² 时亮度会被放大该因子（pf=.8 → 1.5625），但这个归一错误与 #15 曲边误差不是同一个问题，两者要分别做独立负例。

旧 `x_j` 的 ADU 单位只对**真正 native ADU**输入成立，P1 应用的真实 linear_unit=L 时 x_j 是 L，V_x=L²；#9 已证实应用后单位/BUNIT 问题尚未普遍闭合，所以不能把所有 P1 输入硬说成 ADU，也不能靠把最终 P3 BUNIT 写成 ADU/sr 就改变 P1 的实际光度值。P2 单 Signal 只负责最终统一面亮度图，不承诺输出 full C covariance；完整 Drizzle 相邻叶的 covariance 必须从相同输入 x_j 同时参与不同输出叶的相关项推导，不能用 \(V_F\) 的单点值冒充跨叶全相关矩阵。

### D7-A.3 真曲边面积工程方案（TARGET，**不是已实现**）

建议采用**一个具可证明预算的自适应真实几何路径**，在通过独立审计后可保留四角快速路径。此方案先作为科学规格评审，不能在 D7 文档中偷偷宣称 P1 生产已经变更。

1. **球面 chart 与精确参数边界**：以 NESTED leaf 的 (face,ix,iy,nside) 表示 H_p，通过独立的 HEALPix face→sphere 映射对每一段 ix/iy 常量的边作参数化 \(\gamma_e(t)\)，而非只拿四角后做 slerp 作为“真曲边”。源 drop 的 pixel→WCS/SIP 中间边也沿像素坐标参数化 \(d_j(t)\)，在 SIP 与大角度源 patch 上自适应划分。所有面积和 membership oracle 使用一致**真实**球面域，不假设叶边是大圆。
2. **独立真值几何 oracle**：方案A：等面积 chart 的单元裁剪＋高阶/自适应求积，对 4边非测地线精确积分，求 \(a_{jp}^{ref}\)。方案B：独立 astropy-healpix boundaries(step≫1)/healpy ang2pix 的 **球面点 membership oracle** 并做 quasi-MC/分层面积积分，两者不得共同调用被测 spherical_overlap.cpp 的四角 clip 或共用其候选列表；分别报告面积绝对误差、相对误差和真叶身份混淆率。稳定传递全 64bit ipix 和 face，不用浮点近似“近邻”猜身份。
3. **自适应细分与强制精细域**：对每条 \(\gamma_e\) 使用球面弦距和**积分差**双判断，逐段细分直到已证的面积误差上界/独立加密序列达到给定上限；在极冠 apex、|z|≈2/3、RA wrap、face 交界、drop与leaf近切处强制精细或 chart 路径。四角快速分支仅在经过真面积+最差相交构型证明可满足要求的空间域可用。不能只用弦偏离的绝对角 O(h²) 说明 tiny overlap 的**相对**误差小；当真实 a→0 时需绝对误差门，且以所有 drop 归属/错配率另验。
4. **面积与工作量可控**：候选集保守包络要包括曲边与源 WCS 非线性潜在外凸，先取有限 ring/queryDisc 超集再与真 leaf 算精细面积；动态误差预算可按 \(A_{\rm drop}\)、\(A_H\)、局部交叠 a、期望目标 flux 等分配。对超预算 drop 启动精细路径或显式标 unavailable，不悄悄用四角发布“已满足 1e−5”。线程本地 TargetGeomCache 可缓存**参数边/分段/误差证书**（key = nside+ipix+geometry_precision/model version）；内存按 bounded LRU/Runtime lease，不将无限曲线点常驻全局。
5. **独立验证后再改生产**：同一组 true-oracle 输入对比 fast4 与自适应算法，在真实 P1 Drizzle→HiPS→P2单 Signal→P3 FITS 量纲完整链测流量、FWHM、低频背景。只有独立真值给出带置信区间的误差预算及性能回退条件，才把高阶分支从 CURRENT 改成新算法，不由本文修改任何 C++、参数或机器 schema。

最小数值伪代码（TARGET，绝非已运行）：

~~~text
for each input source pixel j:
    P_j := sample_true_pixel_wcs_sip_boundary(j)
    D_j := sample_true_pixfrac_boundary(j)
    A_pixel[j], A_drop[j] := independent spherical area integrals
    candidates := conservative_spherical_envelope(D_j) + leaf_extent
    for leaf p in candidates:
        H_p := healpix_face_parametric_boundary(nside,p)
        a_ref[j,p], bound := certified_chart_curve_overlap(D_j,H_p)
        if bound > error_budget(j,p):
            refine boundaries and integration (or mark geometry_unavailable)
        weight[j,p] = a_ref[j,p]/A_drop[j]
        sumFlux[p] += x[j]*weight[j,p]
        sumNorm[p] += A_pixel[j]*weight[j,p]
        sumArea[p] += a_ref[j,p]
    audit per-drop mass closure AND leaf membership AND boundary error
publish signal only if supported valid geometry; keep precision provenance
~~~

### D7-A.4 分层实验矩阵与真实科学结果门

| 实验轴 | 必须覆盖 | 真值/证据 |
|---|---|---|
| resolution | nside=256、8192、2^18；对照低阶细分分支 | 叶面积 \(4\pi/(12n^2)\) 解析恒等；子分区和独立曲边面积比较 |
| sky domain | 12 faces、apex leaf/极冠、赤道、|z|=2/3 转折及 u+v=1 seam、RA=0±ε、跨面邻域、近平接/小交叠 | 真实曲边积分与独立 point→leaf membership；错误归属矩阵按天区分类 |
| pixfrac/size | .5/.8/1.0；source drop ≪、≈、≫ leaf；极区 subpixel tiny drop、含空洞 | 分离 relative area、absolute area、候选 false-negative、逐 drop mass closure |
| science | 正负常数、解析梯度/弱 extended structure、point/Gaussian Moffat PSF、真实M42、HST处理真值可用部分 | BUNIT、PSF FWHM、孔径通量/星等、结构低频功率、coverage、空间偏差图 |
| end-to-end | raw/native vs photometrically-applied P1，single vs multiple exposures→P2 Signal→P3 nearest/bilinear | 先验证 P1 real linear_unit，再各阶段比较误差，P3重插值核与P1交叠分开测，不互相抵扣 |
| uncertainty | shared input→neighbor HEALPix correlated noise、P2 sky-plane own、P3 repeated leaf与masked corners | 解析/独立MC测 RC R^T；#88/#89 修复的公共API 与 P3 uncertainty unavailable 分开报告 |

每组合给独立 oracle 的有效面积分母、曲边收敛率、CPU/RSS/缓存 hit、裁剪异常状态、时间、相对/绝对误差分位点与最坏叶；某一类超域不得由全球平均遮盖。原 §9 曾列逐叶相对误差 1e−5、极区弦亏、drop闭合 1e−6+1e−15sr、Gaussian aperture 1e−3 等原设计，可作为**历史准则候选**，其复合适用域与 oracle 各自独立确认，不能把“自校闭合达到1e−6”误写成“真实曲边逐叶精度达到1e−5”。

### D7-A.5 #81/#84/#88/#89 与跨阶段责任的精确状态

本批已重新读当前 main：#81 的 P2 reject adapter 已在 large_scale 后以 eligible_before_growth 为限裁剪新增 low/high mask；#84 空候选像元 acc 改为 eligible_count>0?1:0；这**纠正 D6 之前的 CURRENT 源码描述**，不代表本批跑过真实 P2 E2。#89 P3 bilinear 样本级权重输出先填0，#88 对重复 leaf identity 聚合系数再传播对角条件方差；两者只属**公开 P3 核修复**，不能把 P3 bilinear 重采样说成 P1 真实曲边求交，也不能据其把已发布的单 Signal 当成包含真实 full covariance。

**当前三阶段**：P1 输入像素的真实 footprint 面积分派→P1 HiPS signal/support/内部噪声控制；P2 统一 Signal mosaic、P5天空统计另建假设；P3 依据输出 TAN WCS 的像元中心做 nearest/bilinear 读取 HiPS，生成 FITS PRIMARY Signal（uncertainty unavailable）。只有P1曲边误差基于H_p的真实区域定义；P3插值是另一个连续场/采样重建问题，若未来要面积守恒的 P3输出像元面积交叠，需要**独立 P3 conservative area-overlap 算子**，而当前生产没实现，不能拿 P1 的面积相交代码冒充。#79 的响应身份、#9 的量纲、#14 的物理响应函数、#15 的曲边均属于不同闭合要求。

**本 D7-A 结果等级**：生产源码/历史实验与原文保全静态 VERIFIED；独立 astropy-healpix/healpy/wcslib 外部oracle、nside高阶曲边面积、M42/HST、FITS E2E、完整 covariance/噪声效率、CPU/RSS、编译测试 **NOT_RUN**，#15 继续 OPEN。
