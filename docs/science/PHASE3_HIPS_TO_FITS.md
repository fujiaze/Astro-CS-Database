# Phase3 HiPS → 平面 WCS FITS 科学合同 (SCI-P3)

> 上游：ASTROCS_DESIGN.md §6.2（export 流程）、§6.3（投影算法）

> ID: SCI-P3-001  范围: SCI-P3-001..020  状态: FROZEN  上游: SCI-WCS/SCI-DRZ/SCI-SCOPE  下游 ALG: ALG-P3-001..  模块: phase3 (未实现,本合同为施工边界)

## 1 目的与非目标

- **目的**：把符合支持子集的图像 HiPS（HEALPix 层次球面 tile）重投影为用户指定天区/投影/像元尺度/宽高的二维 FITS image，带合法 FITS-WCS header、coverage 与可追溯 metadata（控制包 13 §2 冻结定义）。
- **非目标（alpha 显式拒绝项）**：多通道/RGBA HiPS；JPEG/PNG 等 lossy/display-stretch tile；int+BLANK tile；weight/support 输入产品；flux-per-pixel 输入模式；SIN/CAR 等非 TAN 投影；极近极点视场（请求域收窄，理由见 §4）；GUI。**（variance/ivar 子产品输入为例外：按 §9a-10 必须显式消费传播，不属拒绝项。）**

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `hips_order` | HiPS 层级 order（properties） | 输入 |
| `hips_tile_width` | tile 宽 `W`（默认 512=2⁹） | 输入 |
| `leaf_order` | leaf 像素 order = `tile_order + log2(W)`（`W=hips_tile_width`；**支持子集 W=512 ⇒ +9**） | DATA_SEMANTICS §3 |
| `s_out` | 输出像元角尺度 deg/px | 用户显式 |
| `center` | 输出中心 (RA,Dec) ICRS deg；**= FITS CRVAL（CRVAL1 与 CRVAL2 均进入映射）** | 用户显式 |
| `W_out, H_out` | 输出宽高 px | 用户显式 |
| `order_sel` | 采样 order = min(hips_order, order_needed) | §5 公式 |
| `C(x,y)` | 输出 coverage ∈{0,1} | 输出 |
| `S(x,y)` | 输出面亮度 float32/64 | 输出 |

## 3 物理量和单位

- `S`: **`ADU/sr`**（面亮度语义：计数按**立体角**归一，与输出像元尺度无关 ⇒ 跨像元尺度可比；写端口单位 = `SURFACE_BRIGHTNESS`，BUNIT 透传输入 properties，canonical 串 `ADU/sr`，单位口径唯一权威 = `docs/contracts/DATA_SEMANTICS.md` §31.1a；**非积分通量**，禁 flux-per-pixel 解释）；`s_out`: deg/px；`center/CRVAL/RA,Dec`: deg（ICRS）；坐标: px；coverage: 无量纲 {0,1}。
  **输入单位的判定来源（正向约束）**：导出侧的单位语义**只由输入 `signal/properties` 的 `BUNIT` 承载**，不做任何推断。合法形态有两种，其余一律显式拒绝（`exit 3/4`，无静默默认）：① 逐字等于 canonical 串 `ADU/sr`；② 逐字等于 `ADU` **且**同时声明 `ASTROCS_PIXEL_SEMANTICS=surface_brightness` 与 `ASTROCS_PIXEL_AREA_POWER=-2`。缺 `BUNIT` ⇒ `P3-INPUT-BUNIT-MISSING`；`BUNIT` 存在但不可判或非面亮度语义 ⇒ `P3-INPUT-BUNIT-UNDECIDABLE` / `P3-INPUT-NOT-SURFACE-BRIGHTNESS` / `P3-INPUT-UNIT-UNSUPPORTED`。
  **`ADU/px^2` 属拒绝项而非面亮度别名**：FITS 约定下 `pix` 已是**面积**单位，故 `ADU/px^2` 读作「ADU 每（像素面积）²」，既非面亮度量纲，也与写盘数值（按 sr 归一）不符（`docs/contracts/DATA_SEMANTICS.md` §31.1a 反例条）。实现锚：`lib/infrastructure/scheduler/src/module_adapters.cpp` 的 `p3n_guard_input_units`。

## 3a 坐标 frame

- 输入: ICRS celestial HiPS（NESTED ordering 唯一，GLOSSARY `healpix_ordering`）；其他 frame（galactic/ecliptic）**显式拒绝**（alpha 范围，不做旋转）。
- 输出: FITS-WCS **TAN**（CTYPE1=`RA---TAN`, CTYPE2=`DEC--TAN`, CUNIT=deg）；像素 1-based FITS 约定（GLOSSARY `pixel_coordinate`）。
  **会话面收窄声明**：alpha 会话只接受 TAN（§1/§9a-3），而 projection
  registry 冻结集合 = ASTROCS_DESIGN §5.3 **八投影**（TAN/SIN/CAR/AIT/STG/MOL/CEA/
  ZEA，v3 已实现 4/8）；两者不冲突——registry 面已登记、会话未接线（ALG-P3-PROJ-IMPL-001
  §15.1/§15.5）。CRVAL 语义按 Paper I §2.1.1：CRPIX 处 world == CRVAL（含 CRVAL2）。

## 4 输入有效域

- properties 必需键存在且合法: `hips_order`(int≥0), `hips_tile_width`(**支持子集 W=512**), `hips_frame`='equatorial', 数据属性含 float FITS tiles；**合法转换=仅恒等 ICRS**。
  - **`hips_frame` 的值域与扩展条款**：IVOA REC-HiPS-1.0 §4.4.1 逐字给出标准值域「Format: word “equatorial” (ICRS), “galactic”, “ecliptic”」；同节另逐字声明「The vocabulary associated to some keywords are not exhaustive and may be extended if required」。⇒ **值域 = {equatorial, galactic, ecliptic}**；本管道只产/只收 `equatorial`（ICRS 坐标由该值承载），这是**本管道的范围收窄**，不是对规范的限制；读侧另接受非标准别名 `icrs` 作只读兼容。
  - **tile 宽支持子集**：一般式 = `leaf_order = order_sel + log2(W)`（tile 内 leaf 数 = W²，见 §5），故 `W=512` 时才是 +9。alpha 支持子集**收窄为 W=512**（与生产 `kTileWidth=512` 一致，`p3_resample.cpp:48`）；`W≠512`（含 256/1024）→ **显式拒绝**（不静默按 512 处理），扩宽须补跨 tile 邻接映射与 Oracle 后另行冻结。
  - **W 必须是 2 的幂（该前提由本管道要求，不来自规范）**：规范对 `hips_tile_width` 的约束逐字只有「positive integer」（默认 512），**未要求 2 的幂**；而 §5 的位移恒等式 `tile = ipix >> (2·log2(W))` 只在 `W = 2^S` 时成立。⇒ 实现必须**显式断言** `W & (W-1) == 0`（当前由 properties 解析器以 `hips_tile_width must be 512` 收窄实现，`lib/algorithms/coverage/hips_properties.cpp:120-121`）；非 2 的幂的 W 在规范内合法但**不在本算法适用域**，必须显式拒绝而非静默计算。
- 视场约束: `abs(dec)<=85°`（距极点 ≥5°；单一条件，SCI/API/session 三处一致；`dec` 即 CRVAL2，**参与映射**）。**该界限是请求域收窄，不是投影奇点**：TAN 的唯一数学奇点在距 CRVAL **90°** 的大圆（`cos c = 0`）；投影中心在极点处 `cos c = 1`、完全良态（FITS WCS Paper II §5.1 式 (11) 与 §5.1.3 式 (54) `R_θ = (180°/π)·cot θ`；§2.3 明确 CRVAL 即 native 极点；勘误 2.1 明言默认 LONPOLE 规则**对 θ₀ = 90° 同样适用**）。收窄理由是**极点邻域的球面面积路径尚未达冻结门**（实测闭合破门）；放宽须经变更流程，且**必须与该路径达门同时进行**，否则会把显式拒绝变成静默错值。`W_out,H_out∈[1,20000]`；`s_out>0`；输出四角与中心同半球（TAN 半球约束，越界显式拒）。
- 拒绝项（§1）逐一显式错误，**无静默默认**。

## 5 连续定义

```text
order 选择 (Project-defined, 基于HiPS tile 几何; W=hips_tile_width=512 支持子集):
  s_out_rad    = s_out · π/180                                 # s_out 单位 deg/px（§3 量纲）
  order_needed = ceil( log2( sqrt(π/3) / (W · s_out_rad) ) )   # 等面积等效角尺度 θ_pix ≤ s_out 的最小 order
  order_sel    = min(hips_order, max(0, order_needed))         # 上下限夹紧; 过采样允许, 禁止插值发明细节
  θ_pix(leaf)  = sqrt(π/3) / (2^(order_sel) · W) rad           # 等面积等效角尺度（精确）; leaf_order = order_sel + log2(W)
  h_max(leaf)  = 2.13794  / (2^(order_sel) · W) rad            # 局部采样间隔上界（安全界, 见下）

逐输出像素 (x,y) 反向映射:
  world = TAN^{-1}( CD · ((x+1)−CRPIX1, (y+1)−CRPIX2); CRVAL )  → (RA,Dec)   # Paper I/II 语义
         # TAN 参考点含 CRVAL2（θ0=CRVAL2，Paper II §2.2）；CRPIX 处 world == CRVAL（Paper I §2.1.1）
  leaf_order = order_sel + log2(W)                              # 一般式（W=512 ⇒ +9）
  ipix  = ang2pix_NESTED(nside=2^leaf_order, RA, Dec)          # 像素级 leaf
  tile  = ipix >> (2·log2(W))                                   # **索引位移**（不是 order 偏移）; W=2^S 时成立; W=512 → >>18
  local = ipix & ((1<<(2·log2(W)))−1)                           # tile 内 leaf 索引
  采样: nearest → tile[local]                                    # nearest 为合同行为（不涉跨面邻接）
         # 双线性权重和上界的推导: 每个 w 由 1 次减法 + 1 次乘法构成 ⇒ |Δw| ≤ 3u|w|,
         # Σ|w| = 1; 3 次加法各 ≤ u·(部分和) ≤ u ⇒ |Σw − 1| ≤ 4·3u + 3u = 15u (u = 2⁻⁵³)
        bilinear → tile 邻域 4 像素（跨 tile 读相邻 tile）; **|Σw − 1| ≤ 15·u**（u = 2⁻⁵³）

coverage:
  C(x,y)=1 ⇔ 采样足迹内**存在 tile 像素**（其值可为 NaN；NaN 只进 S/NaN 传播，不改 C）
  否则（tile 缺失/足迹无任何 tile 像素）C=0 且 S=NaN
  §4 视场约束（`abs(dec)<=85°`、`W_out,H_out∈[1,20000]`、`s_out>0`）在此同样成立；真值表:
          tile 存在且值有限 → (C=1, S=值)
          tile 存在但值 NaN → (C=1, S=NaN)   # mask 语义 = coverage ∧ NaN 判定
```

**`θ_pix` 的地位与适用域（正向约束）**

- **`θ_pix = sqrt(π/3)/nside_leaf` 是精确的等面积等效角尺度**：HEALPix 全部像素面积恒为 `Ω_pix = 4π/(12·nside²) = π/(3·nside²)`，故 `θ_pix ≡ sqrt(Ω_pix)` 对每个像素逐位相等（数值复核 nside=1…1024，相对差 0）。文献口径：Górski et al. 2005, ApJ 622, 759 §5 式(23)(24) 定义 `θpix ≡ √Ωpix` 为「the angular resolution」，理由逐字为「Since all pixels have the same surface area but slightly different shape」；HiPS REC 附录把同一式标注为「(global average)」。
- **它不是像素角向展宽的上界**。实测（像素直径 = 4 角点两两最大大圆角距；nside ≤ 1024 全量扫描，极区加密）：
  - 下界 `min/θ_pix → √2 = 1.4142136`（极值像素在赤道带，nside ≥ 16）；
  - 上界 `max/θ_pix → 2.0892036`（闭式 `√(16/5 + 5π²/36)/√(π/3)`，与数值外推 2.0892037 吻合到 7 位有效数字；nside=256 实测 2.0861）。极值像素恒在**极冠边界**（`|z| = 2/3`，纬度 ≈ 41.81°），**不在极点**。
  - ⇒ 安全上界 **`h_max = 2.13794 / nside_leaf` rad**（= 2.0892036·θ_pix），对全部像素、全部 nside 成立。
- **适用域**：`θ_pix ≤ s_out` 是「输出网格比输入采样更细」的过采样条件；在 `order_needed ≤ hips_order` 时按**等面积尺度精确成立**。但 bilinear 的 `O(h²)` 误差界必须取 `h = h_max`（局部最大采样间隔）；用 `θ_pix` 会把该界低估最多 `2.089² = 4.36` 倍。
- **夹紧域（必须显式声明）**：`order_needed > hips_order` 时 `order_sel = hips_order`，此时 `θ_pix > s_out`，**`h ≤ s_out` 不成立**。例：`hips_order=3, W=512, s_out=0.001°/px` ⇒ `order_needed=7`、`order_sel=3`、`θ_pix = 0.01431° = 51.5″/px = 14.31×s_out`。该情形按 §9a-5「按 survey 原生分辨率输出并记录」处理，**过采样主张的成立条件 = `θ_pix ≤ s_out`**。
- 证据：`run/SCI-FIX-PHOTFIT-01/evidence/e1_synthetic_and_realdata.json → E9_order_selection`；`run/SCI-FIX-PHOTFIT-01/evidence/e4_healpix_tan.json → equal_area_exactness / pixel_extent`。

**位移恒等式的地位（正向约束）**：`tile = ipix >> (2·log2(W))` 与 `local = ipix & ((1<<(2·log2(W)))−1)` 由 HEALPix NESTED 的层级性质（父索引 = 子索引 `>> 2`，HiPS REC §4.1 逐字「The tile index N at order K corresponds to the 4 tile indices Nx4, Nx4+1, Nx4+2 and Nx4+3 at order K+1」）与 tile/leaf 的 S 阶差（REC §4.2.1 逐字「HiPS image tile hierarchy is S orders less deep than the original HEALPix resampled data, packaging the 2^S x 2^S HEALPix cell values」）合成；**规范未逐字写出该位移式**，它是推论，且**要求 W = 2^S**。
**可执行判据**：`ang2pix_nested(W·nside_parent,·) >> 2·log2(W) == ang2pix_nested(nside_parent,·)`，对 `W ∈ {256,512,1024}` × `nside_parent ∈ {1,2,8,64}` 共 12 组、每组 200 008 点（含极区、`lat=±90°`、`lon=0/360` 接缝）**全量零不一致**，另有 4 组穷举全天空比对（最大 12 582 912 像素）**零不一致**；负例（位移 ±1 位、W 差一档）不一致数 1.76×10⁵–2.00×10⁵，比偶然命中水平（~3）高 4–5 个数量级 ⇒ 判据有判别力。证据：`run/SCI-FIX-PHOTFIT-01/evidence/e1_synthetic_and_realdata.json → E8_healpix`。

## 6 假设

- HiPS tiles 为 float FITS、与 `hips_order/hips_tile_width` 自洽、frame=ICRS；输出 FITS 读者独立于实现（SYN-007）；TAN 视场受限（§12）。

## 7 独立不变量

- **WCS 往返不变量**：`pixel→world→pixel` 误差 `<1e-8 px`（FP64）。
  **单一事实源**：数值、适用域与门选择全部由 `lib/algorithms/projection/p3_wcs.cpp` 的 `kTanApplicability`（:212-224：`roundtrip_tol_px=1e-8`、`roundtrip_tol_global_px=1e-6`、`min_scale_arcsec=0.9`、`max_fov_deg=20.0`、`max_abs_crval_dec_deg=85.0`）与 `p3_wcs_roundtrip_gate()`（:266-290）给出；本节引用其值，不另立第二套数。
  **适用域**：该紧门仅在 `scale ≥ min_scale_arcsec = 0.9″/px` 时有保守性证据；尺度低于下限时紧门**不适用**（报「超出适用域」而非判红），退回全域保守门 `1e-6 px`。
  **两门不合并**：紧门判据力强但域窄（`0.9″/px` 处余量 2.98×）；全域门覆盖所有真实仪器但判据力弱（`0.18″/px` 处余量 101×）。
  **包络式**：`envelope_px = C_env·u·sec²Δ/s_rad`，`u = 2⁻⁵³`、`C_env = 128`（一阶包络实测 max = 78）、`sec²Δ = 1 + (FOV_rad/2)²`——该式成立是因为 TAN 的平面半径 `ξ = (180°/π)·tan γ`，故 `ξ_max = FOV_rad/2 ⇒ sec²γ_max = 1 + tan²γ_max = 1 + (FOV_rad/2)²`（`p3_wcs.cpp:253-263`）。
  门的适用域判定 = `p3_wcs_roundtrip_gate()`（单一事实源）；见 `docs/algorithms/GATES_AND_TOLERANCES.md` §3。
- **常数场不变量**：常数球面面亮度场 `B0` → 有效区输出恒 `B0`（nearest 与 bilinear 均）。
- **bilinear 权重和**：4 邻域权重和 = 1 ± k·ULP（FP64 累加；**不作逐位/精确断言**——
  IEEE-754 下「恒为 1」不可满足；k 由累加 dtype 决定，测试以相对容差判）。
- **coverage 单调性**：视场/分辨率不变时，增加可用 tile 只增不减 coverage。
- **order 单调性**：`s_out` 变小时 `order_sel` 不减。

## 8 极端/退化条件

| 条件 | 行为 |
|---|---|
| 缺 tile（目录存在但文件缺失） | 该足迹 coverage=0, S=NaN，provenance 记录 missing，不中断 |
| tile 内 NaN | 传播为输出 NaN（**C=1**，值 NaN；与 §5 coverage 定义一致：C 只判「足迹内有无 tile 像素」），mask 语义经 coverage+NaN 判定 |
| 跨 `RA=0/360` | RA 归一 [0,360) wrap，采样按球面角差，无接缝跳变 |
| 极区 tile/输出中心 `abs(dec)>85°` | 显式拒绝（**请求域收窄，非投影奇点**，理由见 §4；`abs(dec)<=85°` 单一条件） |
| properties 非法/缺失键 | 显式错误（无 silent default） |
| JPEG/PNG/int+BLANK/多通道 tile | 显式拒绝（alpha 范围外） |

## 9 精度策略

- 反向映射/球面计算 FP64；输出 float32/64（用户选）；双线性插值误差 O(h²)，其中 **h = 局部最大采样间隔 ≤ `h_max = 2.13794/nside_leaf` rad**（§5）；`order_sel` 公式按**等面积等效角尺度** `θ_pix = sqrt(π/3)/nside_leaf` 约束 `θ_pix ≤ s_out`（仅在 `order_needed ≤ hips_order` 时成立，见 §5 夹紧域）；nearest 无插值误差。

## 9a 专属问题回答（控制包 13 §3 十二项逐项冻结）

1. **HiPS 类型/properties/tile**：单通道 image HiPS；必需 keys=`hips_order, hips_tile_width, hips_frame, dataproduct_type=image`（子集见 §4）；NESTED 唯一；tile=HEALPix cell @hips_order 的 W×W FITS float tile；frame=`equatorial`（ICRS）。
2. **输入坐标系/合法转换**：仅 ICRS 恒等；galactic/ecliptic 显式拒。
3. **输出投影清单**：**alpha 会话仅 TAN**（`p3_wcs_validate_request` 单值接受）；
   **registry 冻结集合 = ASTROCS_DESIGN §5.3 八投影**（TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA），
   v3 已实现 4/8（STG/MOL/CEA/ZEA 归 P3-001）。会话面与 registry 面分离：注册面已登记、
   alpha 会话未接线（ALG-P3-PROJ-IMPL-001 §15.1）；新增投影必须落在冻结集合内并附独立
   往返 Oracle（支持面 = 冻结集合内的投影）。
4. **像素中心/CRPIX/CD/经度方向**：FITS 1-based；CRPIX=`((W_out+1)/2,(H_out+1)/2)`（中心像元整数偏置由 s_out/center 显式参数决定，冻结公式见 ALG-P3-002）；**CD-only**（禁 PC+CDELT 混用）；经度方向=用户显式 parity（`east_left` 默认 ⇒ CD1_1<0，`east_right` ⇒ CD1_1>0）。实现锚：CRPIX 计算 `p3_wcs.cpp:114-115`、校验 :463-469；parity 与 `det(CD)<0` :102-138；CD 写出 `p3_output.cpp:267/:710`。
5. **order 选择公式**：§5 `order_needed`（Project-defined，等面积等效角尺度 `θ_pix ≤ s_out` 的最小 order），上下限 `0 ≤ order_sel ≤ hips_order`；过采样降采样允许，欠采样（被 `hips_order` 夹紧）按 survey 原生分辨率输出并记录。
   **夹紧的可判定条件与记录面**：夹紧 ⇔ `order_needed > hips_order` ⇔ `order_sel == hips_order` 且按 §5 复算的 `order_needed` 更大。记录面 = FITS 关键字 `ORDERSEL`（`p3_output.cpp:290/:724`）与 manifest 的 `order_sel_used`（`p3_session.cpp:413`）；**该分支不产生独立布尔标记**，消费方须以 `order_sel` + 复算 `order_needed` 判定欠采样。
6. **seam/极点/0-360/跨 tile**：NESTED leaf 索引连续保证 tile 内无缝；跨 tile bilinear
   读相邻 tile（HEALPix NESTED **面邻接含轴翻转/镜像映射**，实现按 3×3 切平面四象限邻域取；
   缺角 tile 时该象限权重退化，足迹无 tile ⇒ C=0）；RA wrap 球面角差；极点拒绝（§4/§8）；
   **无人工接缝=SYN-007 连续场判据**。
7. **重采样器与 alpha 默认**：`nearest`（无插值）与 `bilinear`（一阶，**Σw = 1 ± k·ULP**，
   不作逐位断言）两种；**alpha 默认=bilinear**。
8. **SB/flux/未知输入**：tile 值=面亮度（HiPS image 语义）；**flux-per-pixel 输入不支持→显式拒绝**（不做面积换算，flux 与 SB 各自具名）。
9. **NaN/missing/coverage/mask/alpha channel/blank**：§5/§8——coverage 二值 mask；NaN 传播；missing tile=无覆盖+provenance；alpha channel HiPS 与 BLANK int tile 显式拒绝。
10. **variance/ivar/support 输入（DATA-UNC-001 更新块口径）**：Phase3 输入 HiPS **含 variance/ivar 子产品时必须显式消费传播**（输出 `VARIANCE`/`IVAR` 扩展 HDU），两者皆无时显式 `unavailable`（不静默）；唯一权威 = `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（Phase3 节，DATA-P3-UNC-001）+ `docs/contracts/DATA_SEMANTICS.md` §30.4。**weight/support tile 输入仍不支持→显式拒绝**（拒绝逐条登记）；flux-per-pixel 输入仍显式拒绝（§9a-8）。
11. **FITS 关键字**：`BITPIX=-32/-64`；`BSCALE=1,BZERO=0`；`BUNIT` 按 properties，缺省为 canonical `ADU/sr`（**不是**裸 `ADU`——裸 `ADU` 无法量纲可判且与写盘数值不符，见 §3）；WCS=`CRPIX/CRVAL/CD1_1,1_2,2_1,2_2/CTYPE=TAN/CUNIT=deg`；`HISTORY+provenance`（源 HiPS 标识/order_sel/sampler/软件版本/manifest hash）必写。实现锚：缺省串 `p3_resample.cpp:293`、`p3_output.cpp:284-285`；variance/ivar 的 BUNIT 由二次律 canonical 推导（`p3_output.cpp:55-90/:348-372`），不在冻结单位表内的串显式拒绝。
    `CRVAL=(center.RA, center.Dec)` 且**两个分量都进映射**；LONPOLE 取标准默认（`δ0 ≥ θ0 ⇒ φ0`，否则 `φ0+180°`；`φ0` 由 `PVi_1a` 给出、通常为 0。该口径是 Paper II §2.2 的**现行有效形式**，与 FITS Standard 4.0 §8.3 及官方勘误件「Corrections and clarifications for FITS WCS papers I, II, & III」一致。TAN 为天顶投影 ⇒ `θ0 = 90°`，本合同 `|CRVAL2| ≤ 85°` 排除 `δ0 = θ0` ⇒ **默认恒为 `φ0+180° = 180°`**）
    （δ0≥θ0 ⇒ 0° 否则 180°），读方无需额外关键字即可复现。
12. **插值误差/投影畸变/容差/FOV**：nearest 无插值误差，bilinear O(h²) 且 `h ≤ h_max`（§5）；TAN 畸变随 FOV 增长——**alpha 适用 FOV ≤20°** 冻结。该门**只依赖 FOV**（`max sec²c = 1 + (FOV_rad/2)²`），与中心赤纬无关；中心距极点 ≥5° 是**独立**的请求域收窄（§4），两者不同源。
    **FOV 上限的定量含义**：TAN（gnomonic，Paper II §5.1.3 式(54) `Rθ = (180°/π)·cot θ`，θ 为 native latitude、γ = 90°−θ 为离参考点角距）的**径向**尺度因子为 `sec²γ`、**切向**为 `secγ`（⇒ TAN **不是 conformal**）。以本合同的平面 FOV 约定（`FOV = s_out·sqrt(W_out²+H_out²)`，`ξ_half = FOV_rad/2 = tan γ_max`，`p3_wcs.cpp:238-261`）：平面 FOV=20° ⇒ `γ_max = 9.9023°`、径向放大率 `sec²γ_max = 1.030462`（**+3.046%**）、切向 `+1.511%`、像元角尺度比 `cos²γ_max = 0.970441`（**−2.956%**）；FOV=30° ⇒ 径向 **+7.180%**；FOV=40° ⇒ **+13.247%**。⇒ 20° 冻结的适用域 = **边缘与中心的尺度差 ≤3.1%**；超出该域必须显式拒绝（`p3_wcs.cpp:456-461`）。证据：`run/SCI-FIX-PHOTFIT-01/evidence/e4_healpix_tan.json → tan_plane_fov_convention`。
    容差：WCS roundtrip 1e-8 px（单一事实源 `kTanApplicability`，`p3_wcs.cpp:212-224`）、解析场容差由 SYN-007 **预冻结**。

## 10 不可接受变化

- 以空命令/复制 tile/改 header/no-op 冒充重投影（控制包 01 §3）；
- 静默支持超出 §1 的输入（多通道/lossy/variance/flux 模式）或静默降级；
- 改变 `order_needed` 公式/`east_left` 默认 parity/`bilinear` 默认 sampler 而无 SCI 冻结变更；
- Oracle 调用生产 lookup/WCS wrapper（13 §5 独立性）。

## 11 验证 Oracle（13 §5 全集）

常数球面场恒定；解析球面函数经 WCS 反变换逐像素比对；HEALPix tile 边界连续场无人工接缝；RA 0/360、极区、旋转 CD；缺 tile/NaN/mask/coverage；SB 保持；WCS round-trip；baseline/ISA、1/N worker、双平台数值合同；FITS header 以**独立** WCS/FITS 读取器验证。Oracle 不调用生产 HiPS lookup/resampler；小规模允许高精度直接球面计算作 reference。

## 12 关联 ALG ID

`ALG-P3-001` HiPS properties/tile 安全读取；`ALG-P3-002` 输出 FITS-WCS 描述与 pixel↔world；`ALG-P3-003` order 选择/跨 tile 采样/coverage；`ALG-P3-004` FITS 原子写+provenance。

## 13 追溯与测试

权威文件: 本文件（SCI-P3-001）；实现: 待建（CLI-006/P3-001..004）；测试: SYN-007 五件套（`eng/tools/validation/phase3`，SYN-007 任务建立）。

## 14 Primary literature（引用定位声明）

1. Fernique et al. 2015, A&A 578, A114（DOI 10.1051/0004-6361/201526075，[A&A 全文](https://www.aanda.org/articles/aa/full_html/2015/06/aa26075-15/aa26075-15.html)）：§2 层级索引方案与目录/文件结构实现、§3 HiPS↔MOC 关系（节主题经全文页印证）；HiPS=HEALPix tile 映射的定义语义。
2. [IVOA HiPS 1.0 (PR-HiPS-1.0-20161122)](http://www.ivoa.net/documents/HiPS/20161122/PR-HiPS-1.0-20161122.pdf)：§3、§4.1、§4.2.1、§4.4.1、§6.3.1（控制包 13 §2 指定清单；主题——tile/properties/all-sky map/客户端绘制——经全文检索印证；逐行标题核验留 ALG-P3-001 施工时复核）。
3. Górski et al. 2005, ApJ 622, 759（bibcode 2005ApJ...622..759G）：NESTED/`nside=2^order`/ang2pix 语义——文章级（逐式核验留 ALG-P3-003）。
4. Greisen & Calabretta 2002, A&A 395, 1061（Paper I，DOI 10.1051/0004-6361:20021326）§2.1.1（CRPIX 为参考点像素坐标：`p_j = CRPIX_j ⇒ q_i = 0`；CRPIX 可非整数、可在图像外）与 Calabretta & Greisen 2002, A&A 395, 1077（Paper II，DOI 10.1051/0004-6361:20021327）§2.2（**Reference point of the projection**：native↔celestial 三 Euler 角旋转、θ0/φ0 参考点、LONPOLE 默认规则）与 §5.1.3 式(54)(55)（TAN = gnomonic，`Rθ = (180°/π)·cot θ`）——CRPIX/CRVAL（含 CRVAL2）/CD/CTYPE 语义与 TAN 参考点即出此。
   **引用边界**：① `CRVAL` 作为「参考点世界坐标」只在**线性** CTYPE 下是 Paper I 的直接结论；对 `RA---TAN` 该语义由 Paper II §2.2 的投影方程构造（`(φ0,θ0) → (ξ,η)=(0,0)`）保证，归属面 = Paper II §2.2。② LONPOLE 默认值的**现行口径**为 `φ0/φ0+180°`，依据 = 官方勘误件「Corrections and clarifications for FITS WCS papers I, II, & III」§2「Corrections and clarifications for Paper II」第 1 条逐字：「For δ0 ≥ θ0 , the default for LONPOLE a is φ0 . For δ0 < θ0 , the default for LONPOLE a is φ0 + 180° .」；FITS Standard 4.0 §8.3 同此（逐字：「default: φ0 if δ0 ≥ θ0 , φ0 + 180° otherwise」）。**引用 Paper II 正文的 `0/180°` 形式不构成有效依据**，实现与文档一律取 `φ0/φ0+180°`。③ Paper II 未给出 TAN 畸变的**定量**表述（§5.1 的「conformality is a local property」针对 conformal 投影，TAN 不是 conformal，其引用域限于 conformal 投影）；§9a-12 的百分比由式(54) 自行求导。
5. **可执行标准（替代口径）**：astropy 7.0.1（WCSLIB）作 Paper I/II 的可执行标准逐点对拍
   （22 组配置 max 6.854e-13°；AIT 椭圆半轴实测 162.0560°/81.0280° vs 标准
   162.0569°/81.0285°）。
6. `θ_pix = sqrt(π/3)/nside`、`order_needed` 公式、`leaf_order = order_sel + log2(W)`：
   `order_needed` 与 `leaf_order` 为 **Project-defined**（一般式与 DATA_SEMANTICS §3 冻结公式一致；W=512 ⇒ +9，支持子集收窄见 §4）。
   `θ_pix ≡ sqrt(Ω_pix) = sqrt(π/3)/nside` **不是 Project-defined**：它是 HEALPix 等面积性质的精确推论（Górski et al. 2005 §5 式(23)(24) 逐字「Since all pixels have the same surface area but slightly different shape, the angular resolution is defined as θpix ≡ √Ωpix」；面积等式同节逐字「A HEALPix map has Npix = 12 N²side pixels of the same area Ωpix = π/(3 N²side)」）。**该式是等面积等效尺度，不是像素角向展宽的上界**；上界见 §5（`h_max = 2.13794/nside_leaf`）。

## 14b 参考文献与参考代码库（含许可证）

> 本节只补出处与参考实现，不改动 §5/§9a 任何公式、投影域与容差。

- **HiPS 规范**：IVOA HiPS 1.0 Recommendation，**REC-HiPS-1.0-20170519**（https://www.ivoa.net/documents/HiPS/20170519/REC-HiPS-1.0-20170519.pdf；同文档的 Proposed Recommendation 版为 2017-04-06）。**§4.4.1 properties 键值**（逐字）：`hips_version` 值域为「word “1.4” corresponds to this document specification」（注意：**规范文档版本是 1.0，而 `hips_version` 的值是 `1.4`**）；`hips_tile_width` 为「positive integer – Default : 512」（**未约束为 2 的幂**）；`hips_order` 为「Deepest HiPS order – Format: positive integer」；`hips_frame` 值域 =「“equatorial” (ICRS), “galactic”, “ecliptic”」且同节声明该词汇表可扩展。§4.1 逐字要求「HiPS must use the NESTED numbering scheme only」；§4.2.1 逐字给出 tile 内 leaf 数 = `2^S × 2^S`（`S = log2(hips_tile_width)`）与 `nside_leaf = tileWidth × 2^order`。真实 CDS 产品 DSS/2DSSColor、2MASS 实测 `hips_frame=equatorial`；本管道只产/只收 `equatorial`（ICRS 坐标），读侧另接受非标准别名 `icrs` 作只读兼容（§4）。
- **MOC**：IVOA MOC 1.0/2.0 Recommendation（https://www.ivoa.net/documents/MOC/）§4.3.1：uniq=4×4^order+index（write_moc_fits 的 ORDERING=NUNIQ/MOCORDER 依据）。
- **HiPS 层级与目录**：Fernique, P. et al. 2015, A&A 578, A114（DOI 10.1051/0004-6361/201526075）§2/§3。
- **HEALPix 几何**：Górski, K. M. et al. 2005, ApJ 622, 759（DOI 10.1086/427976）；独立实现 astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）、healpy（GPL-2.0，只对照不复制）。
- **FITS WCS**：Greisen & Calabretta 2002, A&A 395, 1061（Paper I）§2.1.1；Calabretta & Greisen 2002, A&A 395, 1077（Paper II）§2.1/§2.2/Table 1；可执行标准 astropy 7.0.1（BSD-3-Clause）/WCSLIB（LGPL-3.0）。
- **FITS 独立读取器**：CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- **order_needed/leaf_order=order_sel+log2(W)/sqrt(π/3) cell 宽度**：Project-defined（§5/§14 第 6 条），与 DATA_SEMANTICS §3 冻结公式一致。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- WCSLIB（LGPL-3.0）/ CFITSIO（宽松许可，NASA/HEASARC）：WCS 与 FITS 独立读取器。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

## 15 Acceptance

- §11 Oracle 十二项全过且 Oracle 独立性成立（不调生产路径）；
- `UNRESOLVED-SCIENCE=0`（§9a 十二项全部冻结，无 TBD/二选一）；
- `eng/tools/science_contract_lint.py` PASS（15 节+合同 ID+锚点）；
- alpha 最小范围=13 §4 候选清单（单通道/ICRS/NESTED/TAN/显式 center-scale-W-H/nearest+bilinear/coverage/float32+64），**收窄不扩大**。

## 16 登记面：§5.3 输入语义守卫与产品 provenance 现状

> 本节是**如实登记**（`ASTROCS_DESIGN.md` §12.5 负向状态如实标注 + §5.3），
> **不改动**本文件任何公式、阈值、容差与冻结锚点。证据 = 本节与 `docs/algorithms/PHASE3_PROJ_IMPL.md` §16
> （判据 sha256 `562d9f7447b91f650d547ce0a322fc519e91de270956da9c49094b7f163d5de0`）。

| # | 项 | 实测现状 | 结论 |
|---|---|---|---|
| C4 | §5.3 输入语义守卫在**生产路径**的接线面 | **调度节点面已接线、会话面未接线**。已接线：守卫内核 `lib/algorithms/resample/p3_rsmp_units.cpp`（`parse_bunit_string`/`resolve_bunit`）随 `astrocs_p3_rsmp` **进生产链接闭包**（`lib/algorithms/resample/CMakeLists.txt:22`，经根 `CMakeLists.txt:1011` 的 `add_subdirectory`）；`module_adapters.cpp` 的 `p3n_guard_input_units`（:11440-11535）在 `p3_op_properties`（:11561）与重采样节点（:11729）两处**先于任何像素读取**调用，失败即 `p3n_guard_fail`（:11541）fail-closed。未接线：`lib/phase3_session/p3_session.cpp:166-172,396` 只透传 BUNIT、不做语义判定；`lib/phase3_session/p3_v6_export.cpp` 未进构建（`grep -c p3_v6_export CMakeLists.txt` = **0**） | **生效面 = 调度节点面**；会话面归 Phase3 export 域 |
| C5 | 守卫对仓内 HiPS 产品的实测裁决 | **分布而非单一结论**（扫描仓内全部 340 份 `*/signal/properties`）：`BUNIT` 缺失 **132**、`ADU/sr` **114**、`ADU/px^2` **93**、`ADU/px^-2` **1**。按守卫分支裁决：`ACCEPT` **114**、`P3-INPUT-BUNIT-MISSING`（exit 3）**132**、`P3-INPUT-NOT-SURFACE-BRIGHTNESS`（exit 4）**94**。产品侧单位声明节点 `declare_hips_surface_brightness_units`（`module_adapters.cpp:10113-10160`，标记键 `ASTROCS_SIGNAL_UNIT`）已对 **206** 份产品写入 canonical `ADU/sr` + `ASTROCS_PIXEL_SEMANTICS=surface_brightness` + `ASTROCS_PIXEL_AREA_POWER=-2`（其中 114 份为 canonical 串、92 份仍带旧串 `ADU/px^2`）；未经该节点的 **132** 份（`aio_hips_writer` 直出）无任何单位键 | **产品侧 provenance 分布性缺口**（不是语义错）：未经声明节点的产品缺 `BUNIT`；带 `ADU/px^2` 的旧产品按 §3 属**正确拒绝**（`pix` 已是面积单位）。证据：`run/SCI-FIX-PHOTFIT-01/evidence/e3_realdata_hips_bunit.json` |
| C6 | 上游 P1 产品 | 真实 Phase1 `signal` 含 `±1e14–1e15` 量级值（低覆盖像素 `S=F/D` 分母退化） | 归 P1 域单独处理 |
| — | Phase2 `signal` 量纲 | 实测 = **面亮度**（分辨率不变密度算子：常量场 `R_cross = 1.0`、真实 testdata `0.999999972724`；链内零单位换算，FLUX-IN 负例逐像元 ×Ω） | 与 §5.3「导出只接受面亮度语义输入」**一致** |

- **口径**：§5.3 的**科学语义已冻结且调度节点面已按 fail-closed 执行**；缺的是会话面接线（C4）与部分产品的单位声明（C5），**不是**科学语义。
- **代码侧缺口（只登记，本文件不改）**：C1a（`module_adapters.cpp:1040-1057` `p2_write_descriptor` 的 `mosaic` 端口仍为 `UnitId::ADU`，应为 `UnitId::SURFACE_BRIGHTNESS`）、C4 的会话面部分、C5 的未声明产品部分、
  C9（HiPS hierarchy 归约用 **f32** 累加器：dk=1 逐位精确、dk=9 偏差 **2.5e-3**（合成）/ **3.95e-4**（真实）；修法 = 用已存在的 `sumFluxD/sumAreaD` 分支或 Kahan/分块补偿求和）。
  另登记：`p3_order_select`（`p3_resample.cpp:206-218`）在无 `k ≤ max_order` 满足 `res_deg ≤ s_out` 时直接返回 `max_order` 且状态 `P3_RS_OK`，**不区分「恰好等于」与「被夹紧」**（欠采样倍率可由 `θ_pix(order_sel)/s_out` 复算，实测最大 14.31×，见 §5 夹紧域）。
  实现侧完整清单与归属见 `docs/algorithms/PHASE3_PROJ_IMPL.md` §16（同一实验单元，避免两套文字）。
