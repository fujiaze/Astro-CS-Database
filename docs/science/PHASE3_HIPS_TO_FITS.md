# Phase3 HiPS → 平面 WCS FITS 科学合同 (SCI-P3)

> ID: SCI-P3-001  范围: SCI-P3-001..020  状态: FROZEN (V6 SCI-008, 2026-09-16；V5 SCI-007 2026-08-28)  上游: SCI-WCS/SCI-DRZ/SCI-SCOPE  下游 ALG: ALG-P3-001..  模块: phase3 (未实现,本合同为施工边界)
>
> **V6 SCI-008（SCI-FIX-PROJ，2026-09-16）**：按 FITS WCS Paper I/II 与 astropy/WCSLIB
> 可复跑实验订正 CAR/AIT 的 CRVAL2 语义、tile 宽/leaf_order 一般式、coverage 真值表、
> 权重和与常数场容差表述（逐条内注）；claim 登记 =
> 工程控制/PROJECT-GOVERNANCE-01/SCIENCE_CORRECTNESS.md SC-001。

> **[DATA-UNC-001 更新块, 2026-09-09；权威依据订正 2026-09-16]** 本合同 §9a-10
> "variance/weight/support 输入：不支持→显式拒绝"与 §1 非目标中
> "variance/weight/ivar 输入产品"的 alpha 拒绝面，就 **variance/ivar 子产品
> 输入**一项由下列**在役**权威修订（原引「宪章 ASTROCS-CONSTITUTION-001
> §7.1/§7.3」在现行活动树中无载体，R-1 §1.4 全文核查，故改为实质条款）：
> 计算与语义唯一权威 = docs/science/UNCERTAINTY_AND_COVARIANCE.md（Phase3 节，
> DATA-P3-UNC-001）+ docs/contracts/DATA_SEMANTICS.md §30.4。Phase3
> 输入 HiPS 含 variance/ivar 子产品时**必须显式消费传播**（输出 VARIANCE/
> IVAR 扩展 HDU），两者皆无时显式 unavailable（不静默）。flux-per-pixel/
> weight/多通道/lossy 等其余拒绝项**全部不变**。
> **[SCI-FIX-PROJ 订正注记 2026-09-16]** 本合同其余条款按 ENGINEERING_SPEC
> §3 以独立证据（FITS WCS Paper I/II、astropy/WCSLIB 7.0.1 可复跑实验）判定后
> **已订正**：§2 leaf_order 一般式、§4/§5 tile 宽 W 与 order 公式、§5 跨 tile
> 邻接与权重和容差、§5/§8 coverage 真值表、§9a-3 投影清单与 registry 面、
> §9a-7 权重和、§14 文献锚。证据与逐条改前/改后见
> reports/PROJECT-GOVERNANCE-01/research/R-1_投影WCS数学正确性.md 与
> run/PROJECT-GOVERNANCE-01/SCI-FIX-PROJ/logs/；claim 登记 =
> 工程控制/PROJECT-GOVERNANCE-01/SCIENCE_CORRECTNESS.md SC-001。

## 1 目的与非目标

- **目的**：把符合支持子集的图像 HiPS（HEALPix 层次球面 tile）重投影为用户指定天区/投影/像元尺度/宽高的二维 FITS image，带合法 FITS-WCS header、coverage 与可追溯 metadata（控制包 13 §2 冻结定义）。
- **非目标（alpha 显式拒绝项）**：多通道/RGBA HiPS；JPEG/PNG 等 lossy/display-stretch tile；int+BLANK tile；weight/support 输入产品；flux-per-pixel 输入模式；SIN/CAR 等非 TAN 投影；极近极点视场；GUI。**（variance/ivar 子产品输入为例外：按 §9a-10 必须显式消费传播，不属拒绝项——DATA-UNC-001 更新块。）**

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

- `S`: ADU（面亮度语义，GLOSSARY `signal/surface_brightness`；tile 值=每像素面亮度，**非积分通量**）；`s_out`: deg/px；`center/CRVAL/RA,Dec`: deg（ICRS）；坐标: px；coverage: 无量纲 {0,1}。

## 3a 坐标 frame

- 输入: ICRS celestial HiPS（NESTED ordering 唯一，GLOSSARY `healpix_ordering`）；其他 frame（galactic/ecliptic）**显式拒绝**（alpha 范围，不做旋转）。
- 输出: FITS-WCS **TAN**（CTYPE1=`RA---TAN`, CTYPE2=`DEC--TAN`, CUNIT=deg）；像素 1-based FITS 约定（GLOSSARY `pixel_coordinate`）。
  **会话面收窄声明（2026-09-16）**：alpha 会话只接受 TAN（§1/§9a-3），而 projection
  registry 冻结集合 = ASTROCS_DESIGN §5.3 **八投影**（TAN/SIN/CAR/AIT/STG/MOL/CEA/
  ZEA，v3 已实现 4/8）；两者不冲突——registry 面已登记、会话未接线（ALG-P3-PROJ-IMPL-001
  §15.1/§15.5）。CRVAL 语义按 Paper I §2.1.1：CRPIX 处 world == CRVAL（含 CRVAL2）。

## 4 输入有效域

- properties 必需键存在且合法: `hips_order`(int≥0), `hips_tile_width`(**支持子集 W=512**；其他 2 的幂见下), `hips_frame`='icrs'（**IVOA REC-HIPS-1.0 §4.4.1 值域 = {icrs, galactic, ecliptic}；本管道只产/只收 ICRS；M1a-B-005 已把写出侧非标准值 "equatorial" 废止，读侧仅作旧产品兼容别名**）, `hips_order`≥0, 数据属性含 float FITS tiles；**合法转换=仅恒等 ICRS**。
  - **tile 宽收窄（SCI-FIX-PROJ 2026-09-16）**：M7-A-117 指出 `leaf_order=+9`
    与 `hips_tile_width∈{2^k}` 值域冲突。一般式 = `leaf_order = order_sel +
    log2(W)`（tile 内 leaf 数 = W²，见 §5），故 `W=512` 时才是 +9。alpha 支持
    子集**收窄为 W=512**（与生产 `p3_resample.cpp:43 kTileWidth=512` 一致）；
    `W≠512`（含 256/1024）→ **显式拒绝**（不静默按 512 处理），扩宽须补跨 tile
    邻接映射与 Oracle 后另行 claim。
- 视场约束: `abs(dec)<=85°`（距极点 ≥5°；TAN 极点退化显式拒，单一条件，SCI/API/session 三处一致；`dec` 即 CRVAL2，**参与映射**）；`W_out,H_out∈[1,20000]`；`s_out>0`；输出四角与中心同半球（TAN 半球约束，越界显式拒）。
- 拒绝项（§1）逐一显式错误，**无静默默认**。

## 5 连续定义

```text
order 选择 (Project-defined, 基于HiPS tile 几何; W=hips_tile_width=512 支持子集):
  s_out_rad    = s_out · π/180                                 # s_out 单位 deg/px（§3 量纲）
  order_needed = ceil( log2( sqrt(π/3) / (W · s_out_rad) ) )   # tile 像素角尺度 ≤ s_out 的最小 order
  order_sel    = min(hips_order, max(0, order_needed))         # 上下限夹紧; 过采样允许, 禁止插值发明细节
  tile 像素角尺度 ≈ sqrt(π/3) / (2^(order_sel) · W) rad        # leaf_order = order_sel + log2(W)（W=512 ⇒ +9）

逐输出像素 (x,y) 反向映射:
  world = TAN^{-1}( CD · ((x+1)−CRPIX1, (y+1)−CRPIX2); CRVAL )  → (RA,Dec)   # Paper I/II 语义
         # TAN 参考点含 CRVAL2（θ0=CRVAL2，Paper II §2.2）；CRPIX 处 world == CRVAL（Paper I §2.1.1）
  leaf_order = order_sel + log2(W)                              # 一般式（W=512 ⇒ +9）
  ipix  = ang2pix_NESTED(nside=2^leaf_order, RA, Dec)          # 像素级 leaf
  tile  = ipix >> (2·log2(W))                                   # **索引位移**（不是 order 偏移）; DATA_SEMANTICS §3 (W=512 → >>18)
  local = ipix & ((1<<(2·log2(W)))−1)                           # tile 内 leaf 索引
  采样: nearest → tile[local]                                    # nearest 为合同行为（不涉跨面邻接）
        bilinear → tile 邻域 4 像素（跨 tile 读相邻 tile）; **Σw = 1 ± k·ULP**（k 由 dtype 决定，不作逐位断言）

coverage（SCI-FIX-PROJ 订正：与 §8「tile 内 NaN」行的互斥解除）:
  C(x,y)=1 ⇔ 采样足迹内**存在 tile 像素**（其值可为 NaN；NaN 只进 S/NaN 传播，不改 C）
  否则（tile 缺失/足迹无任何 tile 像素）C=0 且 S=NaN
  真值表: tile 缺失 → (C=0, S=NaN, provenance missing)
          tile 存在且值有限 → (C=1, S=值)
          tile 存在但值 NaN → (C=1, S=NaN)   # mask 语义 = coverage ∧ NaN 判定
```

## 6 假设

- HiPS tiles 为 float FITS、与 `hips_order/hips_tile_width` 自洽、frame=ICRS；输出 FITS 读者独立于实现（SYN-007）；TAN 视场受限（§12）。

## 7 独立不变量

- **WCS 往返不变量**：`pixel→world→pixel` 误差 `<1e-6 px`（FP64）。
- **常数场不变量**：常数球面面亮度场 `B0` → 有效区输出恒 `B0`（nearest 与 bilinear 均）。
- **bilinear 权重和**：4 邻域权重和 = 1 ± k·ULP（FP64 累加；**不作逐位/精确断言**——
  IEEE-754 下「恒为 1」不可满足，M7-F-201 家族；k 由累加 dtype 决定，测试以相对容差判）。
- **coverage 单调性**：视场/分辨率不变时，增加可用 tile 只增不减 coverage。
- **order 单调性**：`s_out` 变小时 `order_sel` 不减。

## 8 极端/退化条件

| 条件 | 行为 |
|---|---|
| 缺 tile（目录存在但文件缺失） | 该足迹 coverage=0, S=NaN，provenance 记录 missing，不中断 |
| tile 内 NaN | 传播为输出 NaN（**C=1**，值 NaN；与 §5 coverage 定义一致：C 只判「足迹内有无 tile 像素」），mask 语义经 coverage+NaN 判定 |
| 跨 `RA=0/360` | RA 归一 [0,360) wrap，采样按球面角差，无接缝跳变 |
| 极区 tile/输出中心 `abs(dec)>85°` | 显式拒绝（TAN 退化；`abs(dec)<=85°` 单一条件） |
| properties 非法/缺失键 | 显式错误（无 silent default） |
| JPEG/PNG/int+BLANK/多通道 tile | 显式拒绝（alpha 范围外） |

## 9 精度策略

- 反向映射/球面计算 FP64；输出 float32/64（用户选）；双线性插值误差 O(h²)（h=tile 像素角尺度），经 `order_sel` 公式约束 h≤s_out；nearest 无插值误差。

## 9a 专属问题回答（控制包 13 §3 十二项逐项冻结）

1. **HiPS 类型/properties/tile**：单通道 image HiPS；必需 keys=`hips_order, hips_tile_width, hips_frame, dataproduct_type=image`（子集见 §4）；NESTED 唯一；tile=HEALPix cell @hips_order 的 W×W FITS float tile；frame=ICRS。
2. **输入坐标系/合法转换**：仅 ICRS 恒等；galactic/ecliptic 显式拒。
3. **输出投影清单**：**alpha 会话仅 TAN**（`p3_wcs_validate_request` 单值接受，B2-A4 冻结）；
   **registry 冻结集合 = ASTROCS_DESIGN §5.3 八投影**（TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA），
   v3 已实现 4/8（STG/MOL/CEA/ZEA 归 P3-001）。会话面与 registry 面分离：注册面已登记、
   alpha 会话未接线（ALG-P3-PROJ-IMPL-001 §15.1）；新增投影必须落在冻结集合内并附独立
   往返 Oracle（禁止"支持所有"）。
4. **像素中心/CRPIX/CD/经度方向**：FITS 1-based；CRPIX=`((W_out+1)/2,(H_out+1)/2)`（中心像元整数偏置由 s_out/center 显式参数决定，冻结公式见 ALG-P3-002）；**CD-only**（禁 PC+CDELT 混用）；经度方向=用户显式 parity（`east_left` 默认 ⇒ CD1_1<0，`east_right` ⇒ CD1_1>0）。
5. **order 选择公式**：§5 `order_needed`（Project-defined，tile 像素角尺度≤s_out 的最小 order），上下限 `0 ≤ order_sel ≤ hips_order`；过采样降采样允许，欠采样（被 hips_order 截断）按 survey 原生分辨率输出并记录。
6. **seam/极点/0-360/跨 tile**：NESTED leaf 索引连续保证 tile 内无缝；跨 tile bilinear
   读相邻 tile（HEALPix NESTED **面邻接含轴翻转/镜像映射**，实现按 3×3 切平面四象限邻域取；
   缺角 tile 时该象限权重退化，足迹无 tile ⇒ C=0）；RA wrap 球面角差；极点拒绝（§4/§8）；
   **无人工接缝=SYN-007 连续场判据**。
7. **重采样器与 alpha 默认**：`nearest`（无插值）与 `bilinear`（一阶，**Σw = 1 ± k·ULP**，
   不作逐位断言）两种；**alpha 默认=bilinear**。
8. **SB/flux/未知输入**：tile 值=面亮度（HiPS image 语义）；**flux-per-pixel 输入不支持→显式拒绝**（不做面积换算，禁止默认混淆 flux 与 SB）。
9. **NaN/missing/coverage/mask/alpha channel/blank**：§5/§8——coverage 二值 mask；NaN 传播；missing tile=无覆盖+provenance；alpha channel HiPS 与 BLANK int tile 显式拒绝。
10. **variance/ivar/support 输入（DATA-UNC-001 更新块口径）**：Phase3 输入 HiPS **含 variance/ivar 子产品时必须显式消费传播**（输出 `VARIANCE`/`IVAR` 扩展 HDU），两者皆无时显式 `unavailable`（不静默）；唯一权威 = `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（Phase3 节，DATA-P3-UNC-001）+ `docs/contracts/DATA_SEMANTICS.md` §30.4。**weight/support tile 输入仍不支持→显式拒绝**（不得静默丢弃）；flux-per-pixel 输入仍显式拒绝（§9a-8）。
11. **FITS 关键字**：`BITPIX=-32/-64`；`BSCALE=1,BZERO=0`；`BUNIT` 按 properties（缺省 'ADU'）；WCS=`CRPIX/CRVAL/CD1_1,1_2,2_1,2_2/CTYPE=TAN/CUNIT=deg`；`HISTORY+provenance`（源 HiPS 标识/order_sel/sampler/软件版本/manifest hash）必写。
    `CRVAL=(center.RA, center.Dec)` 且**两个分量都进映射**；LONPOLE 取 Paper II 标准默认
    （δ0≥θ0 ⇒ 0° 否则 180°），读方无需额外关键字即可复现。
12. **插值误差/投影畸变/容差/FOV**：nearest 无插值误差，bilinear O(h²) 且 h≤s_out；TAN 畸变随 FOV 增长——**alpha 适用 FOV ≤20°** 冻结（中心距极点 ≥5°）；容差：WCS roundtrip 1e-6 px、解析场容差由 SYN-007 **预冻结**。

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

权威文件: 本文件（SCI-P3-001）；实现: 待建（CLI-006/P3-001..004）；测试: SYN-007 五件套（`tools/validation/phase3`，SYN-007 任务建立）。

## 14 Primary literature（引用定位声明）

1. Fernique et al. 2015, A&A 578, A114（DOI 10.1051/0004-6361/201526075，[A&A 全文](https://www.aanda.org/articles/aa/full_html/2015/06/aa26075-15/aa26075-15.html)）：§2 层级索引方案与目录/文件结构实现、§3 HiPS↔MOC 关系（节主题经全文页印证）；HiPS=HEALPix tile 映射的定义语义。
2. [IVOA HiPS 1.0 (PR-HiPS-1.0-20161122)](http://www.ivoa.net/documents/HiPS/20161122/PR-HiPS-1.0-20161122.pdf)：§3、§4.1、§4.2.1、§4.4.1、§6.3.1（控制包 13 §2 指定清单；主题——tile/properties/all-sky map/客户端绘制——经全文检索印证；逐行标题核验留 ALG-P3-001 施工时复核）。
3. Górski et al. 2005, ApJ 622, 759（bibcode 2005ApJ...622..759G）：NESTED/`nside=2^order`/ang2pix 语义——文章级（逐式核验留 ALG-P3-003）。
4. Greisen & Calabretta 2002, A&A 395, 1061（Paper I，DOI 10.1051/0004-6361:20021326）§2.1.1（CRPIX↔CRVAL 定义性不变量）与 Calabretta & Greisen 2002, A&A 395, 1077（Paper II，[A&A 全文](https://www.aanda.org/articles/aa/full_html/2002/45/aah3860/aah3860.right.html)）§2.1/§2.2（native↔celestial 三 Euler 角旋转、LONPOLE 默认规则、θ0/φ0 参考点）——CRPIX/CRVAL（含 CRVAL2）/CD/CTYPE 语义与 TAN 参考点即出此（SCI-FIX-PROJ 2026-09-16 按 §2.2 订正 CAR/AIT 的 CRVAL2 语义）。
5. **可执行标准（替代口径）**：astropy 7.0.1（WCSLIB）作 Paper I/II 的可执行标准逐点对拍
   （R-1 §2/§3：22 组配置 max 6.854e-13°；AIT 椭圆半轴实测 162.0560°/81.0280° vs 标准
   162.0569°/81.0285°）。用于判定文档与标准不符的三项（CRVAL2 进映射、AIT A≤1、CAR 极行）。
6. `sqrt(π/3)` cell 宽度近似、`order_needed` 公式、`leaf_order = order_sel + log2(W)`：
   **Project-defined**（一般式与 DATA_SEMANTICS §3 冻结公式一致；W=512 ⇒ +9，支持子集收窄
   见 §4）。

## 14b 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §5/§9a 任何公式、投影域与容差。

- **HiPS 规范**：IVOA HiPS 1.0 Recommendation（https://www.ivoa.net/documents/HiPS/；最终 REC 2017-05-31，草案 PR-HiPS-1.0-20170406）。**§4.4.1 properties 键值（本轮核验）**：hips_version=1.4、hips_tile_width=512、hips_order；**hips_frame 标准值域 = {equatorial, galactic, ecliptic}**（真实 CDS 产品 DSS/2DSSColor、2MASS 实测 hips_frame=equatorial；本轮 web_fetch 逐字核验 2026-09-17）。**注**：本文件 §4 现行文本写“值域 {icrs, galactic, ecliptic} 且 equatorial 已废止”，方向与标准原文相反，已在 run/RELEASE-01/science/SCI-S2-topics.md §3-7 登记，待变更 claim 处理（公式/域条款本任务未改）。
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
- `tools/science_contract_lint.py` PASS（15 节+claim ID+锚点）；
- alpha 最小范围=13 §4 候选清单（单通道/ICRS/NESTED/TAN/显式 center-scale-W-H/nearest+bilinear/coverage/float32+64），**收窄不扩大**。
