# Astrometry / WCS Science (SCI-WCS)

> ID: SCI-WCS-001 (SCI-AST-001 别名)  状态: FROZEN (T102 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-WCS-001..  模块: plate_solve (IPV)

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

## 4 输入有效域

- 维度 `w>0,h>0`，星点列表非空；`CRPIX` 按 `w/2+0.5` 冻结；`trans.order` 2–3 阶；`NB_GRID=7` 用于逆向拟合。
- 极区阈值 `|dec|>45°` 进入极区分支，`|dec|>85°` 仍保守；`dra>180°` 时 `dra=360°−dra` 并以 `cos(dec)` 缩放判相交。
- 越界 `rect`/空星点 → `DPSF/IPV_ERR_PARAM`，不产生伪 WCS。

## 5 连续定义

```text
前向 WCS (像素→天球):
  xp = x+1,  yp = y+1
  (u,v) = CD · (xp−CRPIX) + SIP_A/B(u,v)   # (u,v) 为 TAN 投影中间坐标
  (RA,Dec) = TAN^{-1}(u,v; CRVAL)

SIP 前向 (解析公式):
  A[i][j] = cd_inv · trans.x_ij
  B[i][j] = cd_inv · trans.y_ij    # cd_inv = inv(trans 线性项)

SIP 逆向 (最小二乘):
  UV = cd_inv · IWC                # IWC 为世界坐标逆投影
  AP/BP = argmin ||UV − (u,v)−SIP(u,v)||²  on 7×7 grid
  约定 AP[1,0]-=1, BP[0,1]-=1  (剔除单位线性)

Y-up → Y-down 转换 (FITS 1-based 输出):
  cd12, cd22 取反
  A'[i][j] = A[i][j]·(−1)^j,  B'[i][j] = −B[i][j]·(−1)^j
  AP/BP 同规则；CRVAL/CRPIX 不变, |det(CD)| 不变
```

与 `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp:13-16,153-164,274-420,530-576` 及 `ipv_select.cpp:695,712` 一致。

## 6 假设

- 视场内可用单次 gnomonic 投影 + 低阶 SIP 刻画光学畸变；星表为 Gaia DR3（J2000）；视场弧分~度级，极区仍可用专用 prune 保守处理。

## 7 独立不变量

- **CRPIX 不变量**：`CRPIX = w/2+0.5, h/2+0.5` (1-based) 恒成立，与求解结果无关。
- **行列式不变量**：`Y-up → Y-down` 转换前后 `|det(CD)|` 不变（仅符号重排）。
- **SIP 逆一致性**：前向+逆向在 `7×7` 网格上往返误差 `‖(x,y)−WCS^{-1}(WCS(x,y))‖ < 1e-6 pixel`（FP64）。
- **极区保守性**：`|dec|≤85°` 时平面盘 `B(q,C·radius)` 与节点矩形不相交 ⇒ 安全剪枝，`false_negative=0`（`polar_plane_intersects`）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 星点不足/几何退化 | 返回 `IPV_NO_SOLUTION`，不写 WCS | `ipv_entry.cpp` |
| `trans` 线性奇异 (`det→0`) | 拒 SIP 推导，返回参数错误 | `ipv_wcs.cpp:322 inv_det` |
| `rect` 空/越界 | `LOG_WARN empty rect` | `dpsf_psf.cpp:446` 类比 |
| 极区 `θ_q+radius>90°` 或跨 `±45°` | 保守不剪枝，遍历极冠树 | `gaia_client.c` |
| RA 环绕 `±180°` | `dra=360−dra` 并 `cos(dec)` 缩放判交 | `bbox_intersects` |

## 9 精度策略

- FP64 全链路；`CD` 与 SIP 系数以 `double` 写入 FITS 头；`NB_GRID=7` 最小二乘拟合 `AP/BP`，残差 `rms_px` 写入诊断。

## 9a 专属问题回答（SCI-002 指定问题逐项）

- **WCS frame/pixel convention**：ICRS/J2000；CRPIX 1-based 冻结公式（§7）；`xp=x+1`；`CD` deg/px；SIP 系数单位 `1/px^{i+j-1}`；FITS 输出 Y-down 翻转（§5）。
- **PSF 参数**：椭圆 Moffat4 七参数 `B,A,x0,y0,sx,sy,θ`，`FWHM≈1.230310·σ`，解析通量 `flux=2πA·sxsy/3`（PSF.md §2/§5），供星点建模与剔星。
- **aperture/flux/background**：本链为 PSF 拟合通量域，非孔径测光；PSF 背景 `B` 在模型内联合拟合（dpsf §5）；aperture 孔径测光不在 SCI-002 范围，若引入必须新 claim 冻结。
- **photometric scale 与不确定度**：`scale=10^{−location}`，`sigma_mag=2.5·sigma_residual`，`sigma_cal_rel=ln10·sigma_residual`（PHOTOMETRY.md §5/§3）；WCS 残差以 `rms_px` 诊断表达，不并入光度不确定度。

## 10 不可接受变化

- 改变 `CRPIX` 冻结公式或 `1-based ↔ 0-based` 换算；
- 将 `cd_inv` 单位误写为 `deg/pixel`；
- 省略 `Y-down` 的 `cd12/cd22` 符号翻转或 `A/B` 的 `(-1)^j` 因子；
- 在极区移除 `polar_plane_intersects` 的 `C=π/2` 保守盘剪枝。

## 11 验证 Oracle

- **Astropy WCS 参考**：同 `CD/CRPIX/CRVAL/SIP` 的 `astropy.wcs.WCS` 前向/逆向在 100 随机像素上 `‖Δx‖<1e-4 px`。
- **往返不变量**：像素→天球→像素往返 `max_abs <1e-6 px`（网格 7×7 拟合精度门）。
- **极区保守门**：对 `|dec|>45°` 人工锥与全量 Gaia 节点暴力比对，`false_negative=0`。
- **失败注入**：空星表/奇异线性/越界 `rect` 显式错误码。

## 12 关联 ALG ID

- `ALG-WCS-001` IPV 求解与 `trans` 估计
- `ALG-WCS-002` SIP `A/B` 解析与 `AP/BP` 网格拟合 + Y-down 转换

## 13 追溯与测试

- 权威文件: `docs/science/ASTROMETRY.md` (SCI-WCS-001 / SCI-AST-001)
- 实现: `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp` (`build_wcs, CRPIX, cd_inv, SIP A/B/AP/BP, Y-down`), `lib/plate_solve/cpp/ipv/src/ipv_entry.cpp` (`ipv_solve_from_detections_v1`), `lib/gaia_xpsd_client/src/gaia_client.c` (`polar_plane_intersects, bbox_intersects`)
- 公开 API: `lib/plate_solve/cpp/ipv/include/ipv_api.h` (`ipv_solve_from_detections_v1`), `lib/plate_solve/cpp/ipv/include/ipv_wcs.h` (`build_wcs`)
- 测试: `TST-WCS-001` Astropy 比对、`TST-WCS-INV-001` 往返/`TST-WCS-FAIL-001` 参数拒（新增/映射见 `docs/TRACEABILITY.csv`）

## 14 Primary literature（引用定位声明）

1. Greisen & Calabretta 2002, A&A 395, 1061（Paper I，DOI 10.1051/0004-6361:20021326，[A&A 全文](https://www.aanda.org/articles/aa/full/2002/45/aah3859/aah3859.right.html)）：WCS 关键词体系（CRPIX/CRVAL/CD）与广义坐标映射方法——文章级定位，TAN/SIP 公式号未逐式核验，不得以其覆盖本合同 §5。
2. Calabretta & Greisen 2002, A&A 395, 1077（Paper II，[A&A 全文](https://www.aanda.org/articles/aa/full/2002/45/aah3860/aah3860.right.html)）：天球坐标实现与 TAN 投影——文章级定位（§5 TAN 语义为 Project-defined）。
3. Shupe et al. 2005, ASPC 347, 491（SIP畸变约定，bibcode 2005ASPC..347..491S）：SIP A/B/AP/BP 来源——文章级定位（bibcode 级，未逐页核验）。

## 15 Acceptance

- §11 Oracle 全过：Astropy 前向/逆向 `‖Δx‖<1e-4 px`、往返 `<1e-6 px`、极区 `false_negative=0`；
- §7 四不变量门全过（CRPIX/行列式/SIP 逆一致/极区保守）；
- `tools/science_contract_lint.py` PASS（15 节+claim ID+锚点）；
- 解析不变量→SYN-002 转换：已知 WCS 星场（解析 TAN+SIP 场）、往返不变量、RA wrap/极区用例登记 SYN-002；WCS roundtrip 亦入 SYN-007/009。
