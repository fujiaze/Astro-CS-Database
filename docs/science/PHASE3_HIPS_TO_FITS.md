# Phase3 HiPS → 平面 WCS FITS 科学合同 (SCI-P3)

> ID: SCI-P3-001  范围: SCI-P3-001..020  状态: FROZEN (V5 SCI-007, 2026-08-28)  上游: SCI-WCS/SCI-DRZ/SCI-SCOPE  下游 ALG: ALG-P3-001..  模块: phase3 (未实现,本合同为施工边界)

## 1 目的与非目标

- **目的**：把符合支持子集的图像 HiPS（HEALPix 层次球面 tile）重投影为用户指定天区/投影/像元尺度/宽高的二维 FITS image，带合法 FITS-WCS header、coverage 与可追溯 metadata（控制包 13 §2 冻结定义）。
- **非目标（alpha 显式拒绝项）**：多通道/RGBA HiPS；JPEG/PNG 等 lossy/display-stretch tile；int+BLANK tile；variance/weight/ivar 输入产品；flux-per-pixel 输入模式；SIN/CAR 等非 TAN 投影；极近极点视场；GUI。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `hips_order` | HiPS 层级 order（properties） | 输入 |
| `hips_tile_width` | tile 宽 `W`（默认 512=2⁹） | 输入 |
| `leaf_order` | tile 内像素 order = `tile_order+9` | DATA_SEMANTICS §3 |
| `s_out` | 输出像元角尺度 deg/px | 用户显式 |
| `center` | 输出中心 (RA,Dec) ICRS deg | 用户显式 |
| `W_out, H_out` | 输出宽高 px | 用户显式 |
| `order_sel` | 采样 order = min(hips_order, order_needed) | §5 公式 |
| `C(x,y)` | 输出 coverage ∈{0,1} | 输出 |
| `S(x,y)` | 输出面亮度 float32/64 | 输出 |

## 3 物理量和单位

- `S`: ADU（面亮度语义，GLOSSARY `signal/surface_brightness`；tile 值=每像素面亮度，**非积分通量**）；`s_out`: deg/px；`center/CRVAL/RA,Dec`: deg（ICRS）；坐标: px；coverage: 无量纲 {0,1}。

## 3a 坐标 frame

- 输入: ICRS celestial HiPS（NESTED ordering 唯一，GLOSSARY `healpix_ordering`）；其他 frame（galactic/ecliptic）**显式拒绝**（alpha 范围，不做旋转）。
- 输出: FITS-WCS **TAN**（CTYPE1=`RA---TAN`, CTYPE2=`DEC--TAN`, CUNIT=deg）；像素 1-based FITS 约定（GLOSSARY `pixel_coordinate`）。

## 4 输入有效域

- properties 必需键存在且合法: `hips_order`(int≥0), `hips_tile_width`(2 的幂，默认 512), `hips_frame`='icrs'?/equatorial, `hips_order`≥0, 数据属性含 float FITS tiles；**合法转换=仅恒等 ICRS**。
- 视场约束: `abs(dec)<=85°`（距极点 ≥5°；TAN 极点退化显式拒，单一条件，SCI/API/session 三处一致）；`W_out,H_out∈[1,20000]`；`s_out>0`；输出四角与中心同半球（TAN 半球约束，越界显式拒）。
- 拒绝项（§1）逐一显式错误，**无静默默认**。

## 5 连续定义

```text
order 选择 (Project-defined, 基于HiPS tile 几何):
  order_needed = ceil( log2( sqrt(π/3) / (W · s_out_rad) ) )   # tile 像素角尺度 ≤ s_out 的最小 order
  order_sel    = min(hips_order, order_needed), 0 ≤ order_sel  # 过采样允许(bilinear 降采样),禁止插值发明细节
  tile 像素角尺度 ≈ sqrt(π/3) / (2^(order_sel) · W) rad        # leaf_order=order_sel+9

逐输出像素 (x,y) 反向映射:
  world = TAN^{-1}( CD · ((x+1)−CRPIX1, (y+1)−CRPIX2) + CRVAL )  → (RA,Dec)   # Paper I/II 语义
  ipix  = ang2pix_NESTED(nside=2^(order_sel+9), RA, Dec)        # 像素级 leaf (leaf_order=order_sel+9)
  tile  = ipix >> (2·log2(W))                                   # DATA_SEMANTICS §3 (W=512 → >>18)
  采样: nearest → tile[ipix_local]; bilinear → tile 邻域 4 像素(跨 tile 读相邻 tile, 权重和=1)

coverage:
  C(x,y)=1 ⇔ 采样足迹内存在有限 tile 像素; 否则 C=0 且 S=NaN
```

## 6 假设

- HiPS tiles 为 float FITS、与 `hips_order/hips_tile_width` 自洽、frame=ICRS；输出 FITS 读者独立于实现（SYN-007）；TAN 视场受限（§12）。

## 7 独立不变量

- **WCS 往返不变量**：`pixel→world→pixel` 误差 `<1e-6 px`（FP64）。
- **常数场不变量**：常数球面面亮度场 `B0` → 有效区输出恒 `B0`（nearest 与 bilinear 均）。
- **bilinear 权重和**：4 邻域权重和恒为 1（无增益/衰减伪影）。
- **coverage 单调性**：视场/分辨率不变时，增加可用 tile 只增不减 coverage。
- **order 单调性**：`s_out` 变小时 `order_sel` 不减。

## 8 极端/退化条件

| 条件 | 行为 |
|---|---|
| 缺 tile（目录存在但文件缺失） | 该足迹 coverage=0, S=NaN，provenance 记录 missing，不中断 |
| tile 内 NaN | 传播为输出 NaN（C=1，值 NaN），mask 语义经 coverage+NaN 判定 |
| 跨 `RA=0/360` | RA 归一 [0,360) wrap，采样按球面角差，无接缝跳变 |
| 极区 tile/输出中心 `abs(dec)>85°` | 显式拒绝（TAN 退化；`abs(dec)<=85°` 单一条件） |
| properties 非法/缺失键 | 显式错误（无 silent default） |
| JPEG/PNG/int+BLANK/多通道 tile | 显式拒绝（alpha 范围外） |

## 9 精度策略

- 反向映射/球面计算 FP64；输出 float32/64（用户选）；双线性插值误差 O(h²)（h=tile 像素角尺度），经 `order_sel` 公式约束 h≤s_out；nearest 无插值误差。

## 9a 专属问题回答（控制包 13 §3 十二项逐项冻结）

1. **HiPS 类型/properties/tile**：单通道 image HiPS；必需 keys=`hips_order, hips_tile_width, hips_frame, dataproduct_type=image`（子集见 §4）；NESTED 唯一；tile=HEALPix cell @hips_order 的 W×W FITS float tile；frame=ICRS。
2. **输入坐标系/合法转换**：仅 ICRS 恒等；galactic/ecliptic 显式拒。
3. **输出投影清单**：alpha 仅 **TAN**；SIN/CAR 等增加须独立测试+新 claim（禁止"支持所有"）。
4. **像素中心/CRPIX/CD/经度方向**：FITS 1-based；CRPIX=`((W_out+1)/2,(H_out+1)/2)`（中心像元整数偏置由 s_out/center 显式参数决定，冻结公式见 ALG-P3-002）；**CD-only**（禁 PC+CDELT 混用）；经度方向=用户显式 parity（`east_left` 默认 ⇒ CD1_1<0，`east_right` ⇒ CD1_1>0）。
5. **order 选择公式**：§5 `order_needed`（Project-defined，tile 像素角尺度≤s_out 的最小 order），上下限 `0 ≤ order_sel ≤ hips_order`；过采样降采样允许，欠采样（被 hips_order 截断）按 survey 原生分辨率输出并记录。
6. **seam/极点/0-360/跨 tile**：NESTED leaf 索引连续保证 tile 内无缝；跨 tile bilinear 读相邻 tile；RA wrap 球面角差；极点拒绝（§4/§8）；**无人工接缝=SYN-007 连续场判据**。
7. **重采样器与 alpha 默认**：`nearest`（无插值）与 `bilinear`（一阶，权重和=1）两种；**alpha 默认=bilinear**。
8. **SB/flux/未知输入**：tile 值=面亮度（HiPS image 语义）；**flux-per-pixel 输入不支持→显式拒绝**（不做面积换算，禁止默认混淆 flux 与 SB）。
9. **NaN/missing/coverage/mask/alpha channel/blank**：§5/§8——coverage 二值 mask；NaN 传播；missing tile=无覆盖+provenance；alpha channel HiPS 与 BLANK int tile 显式拒绝。
10. **variance/weight/support 输入**：**不支持→显式拒绝**（HiPS-var/权重 tile 不得静默丢弃）；输出仅 `S+coverage`（+provenance）。
11. **FITS 关键字**：`BITPIX=-32/-64`；`BSCALE=1,BZERO=0`；`BUNIT` 按 properties（缺省 'ADU'）；WCS=`CRPIX/CRVAL/CD1_1,1_2,2_1,2_2/CTYPE=TAN/CUNIT=deg`；`HISTORY+provenance`（源 HiPS 标识/order_sel/sampler/软件版本/manifest hash）必写。
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
4. Greisen & Calabretta 2002, A&A 395, 1061（Paper I，DOI 10.1051/0004-6361:20021326）与 Calabretta & Greisen 2002, A&A 395, 1077（Paper II，[A&A 全文](https://www.aanda.org/articles/aa/full_html/2002/45/aah3860/aah3860.right.html)）：CRPIX/CRVAL/CD/CTYPE 与 TAN 语义——文章级（SCI-002 已核验存在性与 DOI）。
5. `sqrt(π/3)` cell 宽度近似、`order_needed` 公式、`leaf_order=tile_order+9`：**Project-defined**（后者与 DATA_SEMANTICS §3 冻结公式一致）。

## 15 Acceptance

- §11 Oracle 十二项全过且 Oracle 独立性成立（不调生产路径）；
- `UNRESOLVED-SCIENCE=0`（§9a 十二项全部冻结，无 TBD/二选一）；
- `tools/science_contract_lint.py` PASS（15 节+claim ID+锚点）；
- alpha 最小范围=13 §4 候选清单（单通道/ICRS/NESTED/TAN/显式 center-scale-W-H/nearest+bilinear/coverage/float32+64），**收窄不扩大**。
