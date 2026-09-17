# Astrometry / WCS Science (SCI-WCS)

> ID: SCI-WCS-001 (SCI-AST-001 别名)  状态: FROZEN (T102 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-WCS-001..  模块: plate_solve (IPV)
> 变更: 新增 §11a 天测精度外部闭环指标 v1（claim「天测精度外部闭环口径冻结」（编号待前台集中分配），2026-09-17；§1–§15 的公式/不变量/容差零改动）

## 1 目的与非目标

- **目的**：为每帧建立世界坐标系 WCS（TAN 投影 + SIP 多项式畸变），使像素 `(x,y)` ↔ 天球 `(RA,Dec)` 可追溯，用于 Drizzle 重投影、Gaia 交叉匹配与 UPM 几何。
- **非目标**：不保证超出 SIP 阶数的畸变残差（QA 报告残差）；不做大气折射/色差高阶修正；不提供超越单次 gnomonic 投影的广视场拼接模型。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `x,y` | 像素坐标（0-based `x∈[0,w-1]`） | 求解器内部 |
| `xp,yp` | FITS 1-based 像素 `xp=x+1` | FITS 头 |
| `CRPIX1/2` | 参考像素（1-based， `w/2+0.5, h/2+0.5`） | `ipv_wcs.cpp:154,276` |
| `CRVAL1/2` | 参考天球坐标 RA/Dec (deg) | 同上 |
| `CD` | 线性变换 `deg/pixel`（`CD=trans.linear/3600`） | WCS 头 |
| ξ,η | TAN 投影中间坐标（deg） | §5 前向式 |
| `cd_inv` | `CD^{-1}` 线性逆 `pixel/arcsec`（仅 SIP 换算） | `ipv_wcs.cpp:328-331` |
| `trans` | IPV 求解的畸变多项式（含 `x_ij,y_ij`） | 求解器 |
| `A/B` | SIP 前向畸变系数 `1/pixel^{i+j-1}` | `ipv_wcs.cpp:340-356` |
| `AP/BP` | SIP 逆向系数 | `ipv_wcs.cpp:463-464` |
| `RA,Dec` | 天球坐标 J2000 deg | 输出 |
| `θ_q, radius` | 查询锥球心距/半径 | `gaia_client.c` |
| `C, C45` | 极区平面 Lipschitz `π/2≈1.5708`, `π/(2√2)≈1.1107` | 极区 prune |

## 3 物理量和单位

- `x,y,xp,CRPIX`: pixel；`CD`: deg/pixel；`cd_inv`: pixel/arcsec；`SIP A/B/AP/BP`: `1/pixel^{i+j-1}`；`RA`: deg `[0,360)`, `Dec`: deg `[-90,90]`；`σ` 质心误差: pixel；`rms`: pixel/arcsec。

## 3a 坐标 frame

- 天球 frame：**ICRS/J2000**（Gaia DR3 星表同系）；`RA∈[0,360)`, `Dec∈[-90,90]`（GLOSSARY `ra_dec`）。
- 像素约定：内部 0-based `x,y`，FITS 输出 1-based `xp=x+1`，`CRPIX` 1-based 恒为 `(w/2+0.5, h/2+0.5)`（§7 不变量）；FITS 输出执行 Y-up→Y-down 翻转（§5），`|det(CD)|` 不变。
- **口径边界与责任方**（STD-F1 / 前台裁决 R-02 方案 b，2026-09-12）：ipv 求解器内部保持 0-based 自洽约定；**FITS 1-based 的唯一桥接点 = Phase3 导出边界**（§5a），桥接责任方为 `lib/phase3_session/`。除该边界外任何一侧不得再施加一次 `+1`（双重桥接 = 恒定 1px 系统偏移）。

## 4 输入有效域

- 维度 `w>0,h>0`，星点列表非空；`CRPIX` 按 `w/2+0.5` 冻结；`trans.order` 2–3 阶；**逆向拟合采样网格 ≥7×7**（实现值：AP/BP 41×41、拟合阶 5；APx/BPx 81×81、阶 7，DISP-WCS-008）。
- 极区阈值 `|dec|>45°` 进入极区分支，`|dec|>85°` 仍保守；`dra>180°` 时 `dra=360°−dra` 并以 `cos(dec)` 缩放判相交。
- 越界 `rect`/空星点 → `DPSF/IPV_ERR_PARAM`，不产生伪 WCS。

## 5 连续定义

```text
前向 WCS (像素→天球):
  xp = x+1,  yp = y+1
  (ξ,η) = CD · [ (xp−CRPIX) + SIP_A/B(xp−CRPIX) ]
  # SIP 自变量 = 像素偏移 dPix = xp−CRPIX (px)；A/B 单位 1/px^{i+j-1}；
  # CD 单位 deg/px ⇒ (ξ,η) 单位 deg；(ξ,η) 为 TAN 投影中间坐标
  (RA,Dec) = TAN^{-1}(ξ,η; CRVAL)

SIP 前向 (解析公式):
  A[i][j] = cd_inv · trans.x_ij
  B[i][j] = cd_inv · trans.y_ij    # cd_inv = inv(trans 线性项)

SIP 逆向 (最小二乘):
  UV = cd_inv · IWC                # IWC 为世界坐标逆投影
  AP/BP = argmin ||UV − [ (u,v) + SIP_A/B(u,v) ]||²   on ≥7×7 grid
  约定 AP[1,0]-=1, BP[0,1]-=1  (剔除单位线性)

Y-up → Y-down 转换 (FITS 1-based 输出):
  cd12, cd22 取反
  A'[i][j] = A[i][j]·(−1)^j,  B'[i][j] = −B[i][j]·(−1)^j
  AP/BP 同规则；CRVAL/CRPIX 不变, |det(CD)| 不变
```

与 `ipv_wcs.cpp::extract_wcs_sip / wcs_sky_to_pixel_iterative` 及 `ipv_select.cpp::select_image_stars` 的像素域 SIP 形式一致（SIP 标准形：自变量为像素偏移，Shupe et al. 2005 §A）。

### 5a 导出边界桥接条款（STD-F1 合同条款；前台裁决 R-02 方案 b）

- **内部口径（求解器）**：`lib/algorithms/platesolve` 迭代反演与 `trans`/SIP 拟合全程 0-based 自洽约定，
  收敛像素输出为 `x = u + CRPIX`（即 `u = x − CRPIX`，与 oracle 前向/逆向同口径，
  `lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:869-872`）。该约定对自身 roundtrip 免疫，故
  与标准口径的差异**是纯原点平移（常量 1px），不是数学内容差**。
- **导出口径（FITS）**：以 FITS WCS Paper I §2.1.1 为基础 —— `CRPIX` 为 **1-based** 参考像素，
  像素坐标 `xp = x + 1`，中间坐标 `(u,v) = CD·(xp − CRPIX) + SIP`（§5）；
  逆向 `x = CD⁻¹·(ξ,η) + CRPIX − 1`（0-based 回程）。
- **桥接点（单一，形状明确）**：`lib/algorithms/projection/p3_wcs.cpp` 的 `fits_pixel_1based`
  （`xp = x + kFitsPixelOrigin`，`kFitsPixelOrigin = 1.0`）与逆桥接 `fits_pixel_0based`；
  `p3_wcs_pix2world` 与 `p3_wcs_world2pix` **只经该函数对换算**，
  该文件内再无第二处像素 `+1`。
- **责任方**：**Phase3 导出边界**（`lib/phase3_session/`）。求解器内部、消费方与诊断工具
  均按各自既有口径使用，不得再叠加一次 `+1`；第三方工具按 Paper I §2.1.1 以 1-based
  参考像素配对使用即可与本链混用。
- **冻结门不变**：Paper I 第三方交叉门 `1e-4 px`（§11）与冻结不变量（§7）**均不变、不放宽**；
  本条不改变任何科学公式、CD/SIP/CRVAL/CRPIX 数值或默认容差。

**实测证据（2026-09-12，STD-F1-ADJ；证据目录 `run/std_f1_adj/`）**

| 桥接配对（导出边界） | 第三方 astropy 交叉（测试面独立实现） | 判定 |
|---|---|---|
| `+1`（合同值，`xp = x+1`） | 前向最大偏差 `5.7e-14 deg`（≈`3.5e-10 px`）；逆向 `< 3.1e-9 px` | 通过（机器精度） |
| 移除桥接（`xp = x`，负向注入） | `1.414 px`（=√2 px：x/y 各差 1px） | 必败（对拍 FAIL） |
| 错置桥接（`xp = x+2`，负向注入） | `1.414 px` | 必败（对拍 FAIL） |

九宫格显式判定（中心 1 格 + 四角 4 格 + 四边中点 4 格，9×100×100 px，1024×1024 帧逐像素；
`east_left` 与 `east_right` 两个 parity 各 9 格，共 18 格）：逐格 roundtrip ≤ `3.2e-10 px`、
第三方 astropy 前向最大偏差 ≤ `7.7e-14 deg`（≤ `5.6e-10 px`）、逆向 ≤ `3.3e-10 px`，
**无 1px 偏移**；同帧内「移除桥接 / 双重桥接」在每一格均产生 `1.414 px` 偏差
（x/y 各 1px）⇒ 桥接不是恒真装饰。

## 6 假设

- 视场内可用单次 gnomonic 投影 + 低阶 SIP 刻画光学畸变；星表为 Gaia DR3（J2000）；视场弧分~度级，极区仍可用专用 prune 保守处理。

## 7 独立不变量

- **CRPIX 不变量**：`CRPIX = w/2+0.5, h/2+0.5` (1-based) 恒成立，与求解结果无关。
- **行列式不变量**：`Y-up → Y-down` 转换前后 `|det(CD)|` 不变（仅符号重排）。
- **SIP 逆一致性**：逆映射以**迭代反演**为准；不变量在**独立于拟合采样的密集域**（中心 90% 区域 + 四边 + 四角 + ≥1000 随机点，FP64）上测量，判据 `max‖(x,y)−WCS^{-1}(WCS(x,y))‖ ≤ 1e-4 px`（与 ALG-WCS-001 §11.4 F2 同值）。
- **极区保守性**：`|dec|≤85°` 时平面盘 `B(q,C·radius)` 与节点矩形不相交 ⇒ 安全剪枝，`false_negative=0`（`polar_plane_intersects`）。
- **导出边界桥接不变量（STD-F1）**：FITS 导出侧恒满足 `xp = x + 1`（Paper I §2.1.1），该桥接**只发生一次**——位于 Phase3 导出边界的单一函数（§5a）；ipv 内部 0-based 输出 `u = x − CRPIX` 不变。桥接缺失或重复，两者都会产生恒定 1px 系统偏移，并必须被第三方对拍检出（§11）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 星点不足/几何退化 | 返回 `IPV_NO_SOLUTION`，不写 WCS | `ipv_entry.cpp` |
| `trans` 线性奇异 (`det→0`) | 拒 SIP 推导，返回参数错误 | `ipv_wcs.cpp:322 inv_det` |
| `rect` 空/越界 | `LOG_WARN empty rect` | `dpsf_psf.cpp:446` 类比 |
| 极区 `θ_q+radius>90°` 或跨 `±45°` | 保守不剪枝，遍历极冠树 | `gaia_client.c` |
| RA 环绕 `±180°` | `dra=360−dra` 并 `cos(dec)` 缩放判交 | `bbox_intersects` |

## 9 精度策略

- FP64 全链路；`CD` 与 SIP 系数以 `double` 写入 FITS 头；AP/BP 以**≥7×7 采样网格**（实现 41×41，DISP-WCS-008）迭代反演/最小二乘拟合，残差 `rms_px` 写入诊断。

## 9a 专属问题回答（SCI-002 指定问题逐项）

- **WCS frame/pixel convention**：ICRS/J2000；CRPIX 1-based 冻结公式（§7）；`xp=x+1`；`CD` deg/px；SIP 系数单位 `1/px^{i+j-1}`；FITS 输出 Y-down 翻转（§5）。
- **PSF 参数**：椭圆 Moffat4 七参数 `B,A,x0,y0,sx,sy,θ`，`FWHM≈1.230310·σ`，解析通量 `flux=2πA·sxsy/3`（PSF.md §2/§5），供星点建模与剔星。
- **aperture/flux/background**：本链为 PSF 拟合通量域，非孔径测光；PSF 背景 `B` 在模型内联合拟合（dpsf §5）；aperture 孔径测光不在 SCI-002 范围，若引入必须新 claim 冻结。
- **photometric scale 与不确定度**：`scale=10^{−location}`，`sigma_mag=2.5·sigma_residual`，`sigma_cal_rel=ln10·sigma_residual`（PHOTOMETRY.md §5/§3）；WCS 残差以 `rms_px` 诊断表达，不并入光度不确定度。

## 10 不可接受变化

- 改变 `CRPIX` 冻结公式或 `1-based ↔ 0-based` 换算；
- 在 Phase3 导出边界（§5a）之外再施加一次、或在该边界处缺失 `+1` 桥接——两种情形都产生恒定 1px 系统偏移；
- 将 `cd_inv` 单位误写为 `deg/pixel`；
- 省略 `Y-down` 的 `cd12/cd22` 符号翻转或 `A/B` 的 `(-1)^j` 因子；
- 在极区移除 `polar_plane_intersects` 的 `C=π/2` 保守盘剪枝。

## 11 验证 Oracle

- **Astropy WCS 参考**：同 `CD/CRPIX/CRVAL/SIP` 的 `astropy.wcs.WCS` 前向/逆向在 100 随机像素上 `‖Δx‖<1e-4 px`。
- **往返不变量**：像素→天球→像素往返在**独立密集域**（中心 90% + 四边/四角 + ≥1000 随机点）上 `max_abs ≤ 1e-4 px`（FP64）；原「7×7 网格 <1e-6 px」口径是拟合域插值条件，鉴别力为零（自网格 1.8e-12 px vs 离网格 3.10 px，R-3 §2.7），已废止。
- **极区保守门**：对 `|dec|>45°` 人工锥与全量 Gaia 节点暴力比对，`false_negative=0`。
- **导出边界桥接门（STD-F1，§5a）**：九宫格（中心 1 格 + 四角 4 格 + 四边中点 4 格，9×100×100 px）
  逐像素 roundtrip `< 1e-6 px`，且与 Paper I §2.1.1 独立第三方参考（`xp = x0+1`）差 `< 1e-6 px`；
  负向注入（移除桥接 `xp = x0`、错置桥接 `xp = x0+2`）必须产生 `≥1 px` 偏差并使对拍 FAIL。
- **失败注入**：空星表/奇异线性/越界 `rect` 显式错误码。
- **外部闭环指标门**：§11a 的 G-P1-WCS-CLOSURE v1 口径与 G-P1-WCS-CLOSURE-REPRO 可复现门。

## 11a 天测精度外部闭环指标（G-P1-WCS-CLOSURE v1，冻结口径）

> 口径冻结 2026-09-17（claim「天测精度外部闭环口径冻结」（编号待前台集中分配）；E2E-001 §4 实测证伪旧口径的可复现性）。
> 唯一可执行实现 = `tools/astrometry/closure_metric.py`（`compute` 出记录 /
> `check` 判门 / `selftest` 负例注入）；门行登记在
> `docs/algorithms/GATES_AND_TOLERANCES.md` §3（G-P1-WCS-CLOSURE 口径、
> G-P1-WCS-CLOSURE-REPRO 可复现门）。**本节的数值参数是唯一事实源，别处不得重述。**

**定义（一句话）**：一帧真实数据的检出星子样本 `S`，经 WCS 前向映射到天球后，
与星表 1-最近邻星的**真实大圆角距** `d_i`（角秒）；指标
`M = median{ d_i : d_i ≤ 1.0″ }`，且**必须同报** `n_matched`、
`match_rate = n_matched/|S|`、`p95`、`max`。

| 项 | 冻结值 | 依据（逐条） |
|---|---|---|
| 检出星样本 `S` | `p1_sources.json` 中 `x,y` 有限 **且 `snr > 20`** | ① 低 SNR 检出星在 1″ 内主要是错配（E2E-001 §4.3 结论 3：全样本只有 4–28% 能找到对应体）；② 与同域位置门 G-P1-CENTROID-SCI 的 `SNR_peak ≥ 20` 域同源（GATES_AND_TOLERANCES §3）；③ 实测（`run/PROJECT-GOVERNANCE-01/E2E-FIX-001/probe_sample_def.py`）：LDN43 T2 中 `snr ≤ 20` 却进入 legacy「flux 前 20000」样本的 13,861 颗贡献了 1497 个 1″ 匹配中的 **813 个（54%）**，median 0.4486″，比 SNR 合格子集的 0.4059″ **更差** ⇒ 不设 SNR 门则半数「匹配」不可解释 |
| 样本上限 | `flux` 降序前 **20000**（超限时记录 `sample_capped=true`，并同时记录 `n_det_snr_pass`） | 运行时间上界（T4 场 n_detected 1.46e5、星表锥内 3.4e6）；**上限必须随记录报告**，否则 |S| 不可复现（E2E §4.3 方法学） |
| 星表样本 | 本仓 Gaia DR3 XPSD 视场单锥搜索，**G < 18** | 与 legacy P11-002 工具同口径（`lib/algorithms/platesolve/memory.md:49-63`）；mag<18 与 T2/T3/T4 探测深度匹配，避免暗端错配主导 |
| 匹配半径 | **1.0″（唯一值）**，必须写入记录 `params.match_radius_arcsec` | ① 1″ 在 T2/T3 原生采样（0.9586/0.9669″/px）≈1 px，把残差锚回像素尺度（legacy `0.897 px` 的本意）；② 1″ 是 E2E 四档扫描（1/2/3/5″）中最严的一档，错配污染最小；③ **不冻结半径即不可复现**：仅把半径 1″→5″，T3 median 即从 0.5410″ 漂到 0.6140″（**+13.5%**，E2E §4.2） |
| 统计量 | **median**（必须同时报 p95 / max / n_matched / match_rate） | ① legacy 口径用 median（P11-002），保持可比；② median 对错配长尾稳健；③ p95/max 几乎贴住半径上限正是「错配主导」的特征（E2E §4.3 结论 3）⇒ 三者必须同报，禁止单选 |
| 匹配率 | `match_rate = n_matched / \|S\|`，**必须与 median 同报** | median 单独不可解释：`M` 只描述「已匹配的那些星」，匹配率回答「多少星根本没匹配上」；实测 4.9%（T3）/3.5%（T2）/28.0%（T4，legacy 样本） |
| WCS 口径 | `wcs_flavor` ∈ {`solved_cd_sip`（产物 `p1_wcs.json` 的 CD+SIP）, `frame_header`（输入帧头，当前为仪器 PinPoint）}，**分别报告、禁止合并** | ① D08：求解结果当前不写回 FITS 头，头域仍是未授权的外部解 ⇒ 两种口径并存；② E2E §4.3.1 实测二者统计等价（T3 solved 0.5644 px / 0.5410″ vs header 0.5051 px / 0.4841″；T2 0.4202 px / 0.4062″ vs 0.3627 px / 0.3507″）⇒ 任一方都不能代表另一方；③ legacy `0.897 px` 是 **frame_header** 口径 |
| 像素换算 | `median_px = median_arcsec / s0`，`s0 = 3600·sqrt\|det(CD)\|`（**线性 CD 标度**；SIP 的局部标度不参与），s0 必须同报 | legacy 以 px 报值；s0 定义与 E2E §4.3 一致，缺 s0 则 px 值不可复现 |
| 残差定义 | 1-最近邻（tangent 平面 KD-tree 选邻居）→ 最终用**真大圆角距**（不用平面近似代替） | E2E §4.3 方法学；与独立 astropy 对拍通过（E2E §3 逐点 100% 相等） |
| 独立性 | 只用 astropy（≥7.0.1）从产物 JSON/FITS 头重建 WCS + 独立星表解码；**不导入 AstroCS 代码、不读 `wcs_result.*`** | ENGINEERING_SPEC §5.1「不调用生产实现的独立 Oracle」；GATES_AND_TOLERANCES §1 R3 |
| 复现容差 | 同输入同口径两次运行：**median 完全相等（容差 0 px）**、`n_matched` 完全相等 | E2E §5.1/§5.3 实测：全链科学面产物逐字节 EQUAL、规范哈希全等 ⇒ 同输入同口径的指标漂移实测 = 0；非 0 即说明样本/口径/输入哈希有未记录变化 ⇒ 判红。记录内的 `1e-9 px` 只是 JSON 浮点往返护栏，**不是科学容差** |

**判读纪律（与门同读，禁止单独引用 median）**：

1. 本指标**不是**「天测精度」本身：它同时含检出星质心误差、星表系统差、
   投影/SIP 模型残差与错配长尾；`median` 与 `match_rate`、内部解 `rms_arcsec`/`n_pairs`
   （§9a）必须联合判读。**该指标不得单独作科学门。**
2. 阈值按**像素尺度分档**给定：T2/T3 档（s0≈0.96″/px）与 T4 档（s0≈6.31″/px）不可互推。
   当前分档阈值 **UNJUSTIFIED（发布门=N）**：E2E 实测值是在 UNIT-001（XISF 母版单位）
   未修复的输入上取得的，修复后必须整体复跑才可标定阈值。
3. 星表锥内星数、`sample_capped`、`s0`、星表/p1_sources 的 sha256 必须随记录落盘
   （`tools/astrometry/closure_metric.py` 已强制），否则该记录不可复现、不得引用。

**历史值（不再作为门，仅存档）**：

| 历史值 | 出处 | 旧口径 | 废止理由 |
|---|---|---|---|
| `Galaxy_Center rms_arcsec = 0.1431″` | `docs/algorithms/PLATESOLVE.md` §11.4、`lib/algorithms/platesolve/memory.md:32`（2026-07-12） | v1.2 `ipv_solve_from_memory` 单次记录，无 s0/样本/域信息 | 在当前版本 T4 帧上**不可复现**（实测 0.2803–0.3588″，2.0–2.5×）；只在 T2/T3 档场复现（0.1472/0.1584″）⇒ 标签与实测域不一致，见 claim「天测精度外部闭环口径冻结」（编号待前台集中分配） |
| `0.897 px`（全帧匹配星 median，frame-header 口径） | `lib/algorithms/platesolve/memory.md:55`（legacy v1.2 工具 × 2 帧） | 匹配半径/星选/统计量**均未冻结** | 单值不可复现：仅改匹配半径 1″→5″ median 漂 14%；量级可复现（当前 T3 0.564–0.641 px） |

## 12 关联 ALG ID

- `ALG-WCS-001` IPV 求解与 `trans` 估计
- `ALG-WCS-002` SIP `A/B` 解析与 `AP/BP` 网格拟合 + Y-down 转换

## 13 追溯与测试

- 权威文件: `docs/science/ASTROMETRY.md` (SCI-WCS-001 / SCI-AST-001)
- 实现: `lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp` (`build_wcs, CRPIX, cd_inv, SIP A/B/AP/BP, Y-down`), `lib/algorithms/platesolve/cpp/ipv/src/ipv_entry.cpp` (`ipv_solve_from_detections_v1`), `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c` (`polar_plane_intersects, bbox_intersects`)
- 公开 API: `lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h` (`ipv_solve_from_detections_v1`), `lib/algorithms/platesolve/cpp/ipv/include/ipv_wcs.h` (`build_wcs`)
- 测试: `TST-WCS-001` Astropy 比对、`TST-WCS-INV-001` 往返/`TST-WCS-FAIL-001` 参数拒（新增/映射见 `docs/TRACEABILITY.csv`）

## 14 Primary literature（引用定位声明）

1. Greisen & Calabretta 2002, A&A 395, 1061（Paper I，DOI 10.1051/0004-6361:20021326，[A&A 全文](https://www.aanda.org/articles/aa/full/2002/45/aah3859/aah3859.right.html)）：WCS 关键词体系（CRPIX/CRVAL/CD）与广义坐标映射方法——文章级定位，TAN/SIP 公式号未逐式核验，不得以其覆盖本合同 §5。
2. Calabretta & Greisen 2002, A&A 395, 1077（Paper II，[A&A 全文](https://www.aanda.org/articles/aa/full/2002/45/aah3860/aah3860.right.html)）：天球坐标实现与 TAN 投影——文章级定位（§5 TAN 语义为 Project-defined）。
3. Shupe et al. 2005, ASPC 347, 491（SIP畸变约定，bibcode 2005ASPC..347..491S）：SIP A/B/AP/BP 来源——文章级定位（bibcode 级，未逐页核验）。

## 14a 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §5/§5a/§11a 任何公式、常数与容差。

- **WCS 框架与 1-based CRPIX/CRVAL**：Greisen, E. W. & Calabretta, M. R. 2002, A&A 395, 1061（Paper I；DOI 10.1051/0004-6361:20021326，arXiv:astro-ph/0207407 逐字核验 2026-09-17）§2.1.1 式(1) q_i=Σ_j m_ij(p_j−r_j)（r_j=CRPIX_j）与 §2.1.4（整数像素号=像素中心，首像素 0.5→1.5）。**据此，CRPIX 处 world==CRVAL 在 1-based 下无需额外 +1**；本文件 §5a 的“0-based/常量 1px 平移”叙述与 Paper I 及实现（ipv_wcs.cpp:944-945 out.x=u+CRPIX）不符，已在 run/RELEASE-01/science/SCI-S2-topics.md §3-2 登记（finding WCS-003-F1），待变更 claim 处理。
- **TAN 投影与 celestial↔native 旋转/LONPOLE**：Calabretta, M. R. & Greisen, E. W. 2002, A&A 395, 1077（Paper II）§2.1/§2.2/Table 1。
- **SIP A/B/AP/BP 约定**：Shupe, D. L. et al. 2005, ASP Conf. Ser. 347, 491（bibcode 2005ASPC..347..491S）。**核验状态**：bibcode 级。
- **可执行标准与独立 Oracle**：WCSLIB（LGPL-3.0，官方 https://www.atnf.csiro.au/people/mcalabre/WCS/ ；镜像 Punzo/wcslib SPDX=LGPL-3.0）；astropy.wcs（BSD-3-Clause，https://github.com/astropy/astropy）≥7.0.1；ERFA（BSD-3-Clause 类，liberfa/erfa）。
- **多帧天体/光度联合校准实践**：SCAMP（GPL-3.0，https://github.com/astromatic/scamp；论文 Bertin 2006, ASPC 351, 112）；Astrometry.net（https://astrometry.net，许可证**需网络核验**）。
- **Gaia 参考星表与 ICRS/J2000**：Gaia Collaboration et al. 2016, A&A 595, A1（DR1）；2018, A&A 616, A1（DR2）；2021, A&A 649, A1（EDR3）；2023, A&A 674, A1（DR3）。**核验状态**：文章级；本合同只消费星表数值。
- **极区保守剪枝/球面 bbox**：Project-defined（§8/§11）；球面几何基元可对照 astropy/ERFA 的独立实现。
- **Y-up↔Y-down 与 |det(CD)| 不变量**：Paper I §2.1.1 的像素/世界定义 + Project-defined 符号规则（§5）。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

## 15 Acceptance

- §11 Oracle 全过：Astropy 前向/逆向 `‖Δx‖<1e-4 px`、往返 `≤1e-4 px`（独立密集域，§11）、极区 `false_negative=0`；
- §11 导出边界桥接门（STD-F1）过：九宫格两 parity 共 18 格全过、astropy 四向桥接扫描
  正向达机器精度且三向负向注入全部检出（证据 `run/std_f1_adj/std_f1_bridge_cross.json`）；
- §7 四不变量门全过（CRPIX/行列式/SIP 逆一致/极区保守）；
- §11a 外部闭环指标可复现门（G-P1-WCS-CLOSURE-REPRO）过：`ctest:p1wcs_closure_metric_gate`
  （合成场同输入两跑 median 完全相等 + 7 类负例注入全部判红 + 2 类缺输入 fail-closed 判红）；
- `tools/science_contract_lint.py` PASS（15 节+claim ID+锚点）；
- 解析不变量→SYN-002 转换：已知 WCS 星场（解析 TAN+SIP 场）、往返不变量、RA wrap/极区用例登记 SYN-002；WCS roundtrip 亦入 SYN-007/009。
