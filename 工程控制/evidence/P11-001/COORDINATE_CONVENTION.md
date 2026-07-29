# AstroCS 坐标约定冻结文档（P11-001）

> 状态：**FROZEN**（2026-07-27）
> 依据：`docs/05_WCS_COORDINATE_CONVENTION_AND_CLOSURE_SPEC.md` + 代码审计事实
> 约束：本文件冻结后，任何坐标相关变更必须先提 ADR，禁止"先改符号"

## 1. 坐标系统定义

| 系统 | 原点 | Y 方向 | 索引基准 | 单位 | 用途 |
|------|------|--------|----------|------|------|
| **S1 图像数组坐标 (detector)** | 左上角 (0,0) | Y 向下（行号递增） | 0-based | 像素 | `star_detector` DLL 输出，numpy `arr[y,x]` |
| **S2 内部 U 坐标 (plate_solve)** | 图像中心 `(w/2, h/2)` | Y 向上 | 0-based | 像素 | PlateSolve 内部匹配/拟合 |
| **S3 FITS WCS 像素坐标** | 左上角参考像素 | Y 向下（FITS 行号递增） | **1-based** | 像素 | CRPIX/CD/SIP 写入 FITS header |
| **S4 切平面中间坐标 (xi, eta)** | 投影中心 | Y 向上（与 Dec 同向） | N/A | **角秒**（plate_solve 内部） | Gnomonic 投影输出 |
| **S5 天球坐标 (ra, dec)** | ICRS 历元 J2000 | Dec 向北为正 | N/A | 度 | Gaia 星表、CRVAL、Photometric/SNR/Drizzle |
| **S6 HEALPix 球面像素** | NESTED 序号 | N/A | 0-based | 像素索引 | Drizzle 累加、HISS/HCSD 存储 |
| **S7 浏览器笛卡尔坐标** | 球心 (0,0,0) | Z=北天极 | 右手系 | 单位向量 | X=春分点(RA=0), Y=RA=90° |

## 2. 关键转换函数

### 2.1 S1 → S2（detector → U）

**位置**：`lib/plate_solve/cpp/ipv/src/ipv_select.cpp:682-687`

```cpp
double cx = img_w / 2.0, cy = img_h / 2.0;       // 中心点（无 +0.5）
output.U[i].x = (det_x[idx] - cx);               // 平移到中心
output.U[i].y = -(det_y[idx] - cy);              // Y 反转（down → up）
```

- 输入：detector 0-based 数组坐标
- 输出：U 坐标（原点=图像中心，Y 向上）
- **不乘 s0**（避免 FOCALLEN 标称误差影响匹配）

### 2.2 S2 → S4（U → xi,eta via TRANS）

**位置**：`lib/plate_solve/cpp/ipv/include/ipv_itertrans.h:121-138` (`apply_trans`)

- 输入：U 坐标（像素，原点中心，Y 向上）
- 输出：(xi, eta) 角秒，原点投影中心
- 线性项单位 = 角秒/像素，常数项 = 角秒
- 阶数 1/2/3 对应 6/12/20 参数

### 2.3 S5 → S4（天球 → 切平面，Gnomonic 正投影）

**位置**：`lib/plate_solve/cpp/ipv/src/ipv_select.cpp:517-545` (`gnomonic_forward_proj`)

- 输入：(ra, dec) 度，切点 (ra0, dec0) 度
- 输出：(xi, eta) **角秒**
- 退化保护：`cosc ≤ 1e-10` 返回 valid=false
- 标准 Calabretta & Greisen 2002 公式

### 2.4 S4 → S5（切平面 → 天球，Gnomonic 反投影）

**位置**：`lib/plate_solve/cpp/ipv/src/ipv_solver.cpp:65-100` (`gnomonic_inverse_proj`)

- 输入：(xi, eta) 角秒，切点 (ra0, dec0) 度
- 输出：(ra, dec) 度

### 2.5 S2 → S3（U → FITS WCS，Y-up → Y-down 转换）

**位置**：`lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp:540-581`

转换规则（在 `extract_wcs_sip` 出口处统一应用）：

| 元素 | 转换规则 | 推导 |
|------|---------|------|
| `CD.cd12` | 取反 | Y 列翻转 |
| `CD.cd22` | 取反 | Y 列翻转 |
| `SIP A[i][j]` | × `(-1)^j` | 仅输入 y 翻转 |
| `SIP B[i][j]` | × `-(-1)^j` | 输入+输出 y 翻转 |
| `SIP AP[i][j]` | × `(-1)^j` | 同 A 规则 |
| `SIP BP[i][j]` | × `-(-1)^j` | 同 B 规则 |
| `CRVAL1/CRVAL2` | 不变 | 中心点对称 |
| `CRPIX1/CRPIX2` | 不变 | 中心点对称 |

### 2.6 S3 → S5（FITS WCS 像素 → 天球，标准 WCS）

**位置**：`lib/photometric_calib/cpp/src/wcs_transform.cpp:173-188`（Photometric）、`lib/healpix_db/healpix_drizzle/wcs_sip.cpp`（Drizzle）

```cpp
double dx = x - (m_crpix1 - 1.0);     // CRPIX 1-based → 0-based 偏移
double dy = y - (m_crpix2 - 1.0);
// 前向 SIP A/B（若有）
double xi  = m_cd[0] * dx + m_cd[1] * dy;   // CD 直接应用，不显式乘 cos(Dec)
double eta = m_cd[2] * dx + m_cd[3] * dy;
tanIntermediateToWorld(xi, eta, ra, dec);   // 标准 TAN 反投影
```

- 输入：(x, y) 0-based 像素坐标
- 输出：(ra, dec) 度
- **CD 矩阵直接应用，不显式乘 cos(Dec)**（cos(Dec) 已由 plate solver 在 CD 求解时隐含）

### 2.7 S5 → S3（天球 → FITS WCS 像素，标准 WCS 逆向）

**位置**：同 2.6 的 `skyToPixel` 函数

- TAN 投影 → CD⁻¹ → 逆向 SIP AP/BP → + CRPIX（转 1-based）

## 3. 关键变量冻结

### 3.1 CRPIX（参考像素）

- **基准**：1-based（FITS WCS Paper I 标准）
- **计算公式**：`CRPIX1 = width/2.0 + 0.5`，`CRPIX2 = height/2.0 + 0.5`
- **位置**：`lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp:285-288`
- **Python 写入**：`lib/plate_solve/python/solve_and_write_wcs.py:443-444`（直接使用 C++ 返回值，不再 +1）
- **读取**：`lib/astro_image_io` 模块原样读写，不做 0-based/1-based 转换

### 3.2 CD 矩阵

- **格式**：标准 WCS（含 cos(Dec) 缩放，无独立 1/cos(Dec) 因子）
- **单位**：度/像素
- **生成位置**：`lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp:273-276`（TRANS 线性项 / 3600）
- **消费方约定**：Photometric/SNR/Drizzle 直接应用 CD，**不显式乘 cos(Dec)**
- **V40PrototypeSolver (Python) 例外**：输出 CD 是标准 WCS 格式，需乘 cos(Dec)（仅 Python 原型，C++ 已修正）

### 3.3 SIP 系数

- **索引约定**：`A[i*6+j]` 对应 `dx^i * dy^j`，i+j ≤ order，最大 5 阶（36 项）
- **存储位置**：`lib/plate_solve/cpp/ipv/include/ipv_types.h`（`SipCoeffs` 结构）
- **前向 A/B**：pixelToSky 使用，由 `extract_wcs_sip` 解析公式提取
- **逆向 AP/BP**：skyToPixel 使用，由网格反变换法（NB_GRID_POINTS=7）生成
- **归一化**：`ipv_sip.cpp:470-488` 中 `A[i][j] = coeff[k] / scale^(i+j)`

### 3.4 (RA - CRVAL) × cos(Dec)

- **标准 WCS 公式**中 RA 方向需乘 `cos(dec)`（注释见 `ipv_sip.h:27-28`）
- C++ TRANS 已通过 Gnomonic 投影隐含处理，CD 输出不含独立因子
- 下游 astropy/Siril 读取标准 WCS 时自行应用

### 3.5 图像数组 shape

- **单通道**：`(height, width)`（numpy 行主序）
- **多通道**：`(channels, height, width)`（CHW，与 PCL 约定一致）
- **NAXIS1 = width, NAXIS2 = height**（FITS 标准）
- **像素存储**：行主序 `data[y*width + x]`

### 3.6 has_wcs 判定

- **条件**：`CTYPE1/CTYPE2 非空 且 CD 矩阵至少一个元素 |val| > 1e-15`
- **未检查 CRVAL**：CRVAL=0 也认为有 WCS（事实，可能需后续讨论）
- **三处实现一致**：C++ FITS、C++ XISF、Python

### 3.7 pixel_scale / rotation_deg

- `pixel_scale = sqrt(|det(CD)|) * 3600`（角秒/像素，小视场近似）
- `rotation_deg = atan2(cd2_1, cd1_1)`（度，仅旋转×缩放时准确）

## 4. 模块间一致性

| 模块 | 文件 | CRPIX | 像素 | 天球 | 投影 | CD cos(Dec) |
|------|------|-------|------|------|------|-------------|
| PlateSolve (输出) | `ipv_wcs.cpp` | 1-based | 0-based 内部 | 度 | TAN+SIP | 标准 WCS（无独立因子） |
| Photometric | `wcs_transform.cpp` | 1-based | 0-based 输入 | 度 | TAN+SIP | 不显式乘 |
| SNR | `snr_estimator.cpp` | 1-based | 0-based | 度 | TAN+SIP（前向 A/B） | 不显式乘 |
| Drizzle | `wcs_sip.cpp` | 1-based | 0-based | 度 | TAN+SIP | 不显式乘 |
| astro_image_io | `astro_image_io.py` | 原样 1-based | shape=(h,w) | N/A | N/A | 原样读写 |

**结论**：四个 WCS 消费模块约定完全一致。

## 5. Y 轴反转链

反转在两个环节发生：

1. **输入侧**（detector → U）：`ipv_select.cpp:687`，`U.y = -(det_y - cy)`，图像 Y 向下转内部 Y 向上
2. **输出侧**（U → FITS WCS）：`ipv_wcs.cpp:540-581`，CD.cd12/cd22 取反 + SIP 符号调整，内部 Y-up 转 FITS Y-down

**中间环节**（solver/itertrans/robust_refine）统一使用 Y-up，不在内部做翻转。

## 6. 球面浏览器独立坐标系

- **坐标系**：右手笛卡尔，X=春分点(RA=0,Dec=0)，Y=RA=90°，Z=北天极
- **球面→笛卡尔**：`x=cos(dec)cos(ra), y=cos(dec)sin(ra), z=sin(dec)`
- **相机位置**：球心 (0,0,0)，向外看
- **yaw（左右拖动）**：forward 绕 up 旋转，up 不变（画面不旋转）
- **pitch（上下拖动）**：forward + up 都绕 right = forward × up 旋转
- **up vector**：north-up，从 ra/dec 重算，**绝不携带**（防 roll）
- **MAX_FOV = 50°**（防球面投影畸变）
- **极区兜底**：dec=±90° 时 north 退化，用 (0,1,0) 替代
- **与 WCS 像素坐标不直接交换**，仅通过 HEALPix 像素的 (ra, dec) 交互

## 7. Gaia 客户端约定

- **坐标系**：ICRS，历元 J2000
- **单位**：ra/dec 度（double），magG/magBP/magRP mag，parallax mas，pmra/pmdec mas/yr
- **Cone search**：输入 (ra, dec, radius_deg) 度
- **Gnomonic 投影**：输出 (xi, eta) **角秒**（仅 plate_solve 内部使用）
- **缓存**：星表 60s TTL，区域索引保留至程序关闭

## 8. 禁止事项

依据 `docs/05_WCS_COORDINATE_CONVENTION_AND_CLOSURE_SPEC.md` 修复原则：

1. **不得先改符号**：Y 轴和 SIP 奇次项符号是待验证假设，必须由诊断确定确切转换
2. **不得只在 Photometric 中补偿**：最终修复必须统一服务于 Photometric、SNR、Drizzle 和浏览器
3. **不得用旧路径 A/B 一致替代标准 WCS 闭环验证**
4. **不得在 Photometric 内部偷偷翻转 Y 来掩盖错误的 WCS 生产端**

## 9. 接口注释要求

以下接口需在头文件中保持坐标约定注释（已存在，本次冻结确认）：

- `lib/plate_solve/cpp/ipv/include/ipv_itertrans.h:80`（U 坐标定义）
- `lib/plate_solve/cpp/ipv/include/ipv_sip.h:27-28`（标准 WCS 公式含 cos(dec)）
- `lib/plate_solve/cpp/ipv/include/ipv_solver.h:69`（CRPIX 1-based）
- `lib/plate_solve/cpp/ipv/include/ipv_api.h:42`（CRPIX 1-based）
- `lib/plate_solve/cpp/ipv/include/ipv_types.h:102`（crpix 1-based）
- `lib/photometric_calib/cpp/src/wcs_transform.h:9-12`（坐标约定注释块）
- `lib/healpix_db/healpix_browser_qt/widgets/sphere_view.cpp:1-9`（浏览器坐标系）

## 10. 变更控制

本文件冻结后，任何坐标相关变更（CRPIX 基准、CD cos(Dec) 处理、SIP 符号、Y 方向、shape 约定）必须：

1. 先提 ADR 记录变更原因与影响
2. 通过 P11-002（标准 WCS 真实星对闭环诊断工具）验证
3. 在 P11-003（T1-T4 代表帧复现 WCS 闭环缺陷）中证明缺陷
4. 在 P11-004（WCS 生产端统一修正）中统一修复
5. 在 P11-005（PlateSolve 710 全量回归）中验证无回归

**禁止跳过闭环验证直接改符号。**

---

## 附录 A：关键文件清单

### C++ 实现
- `lib/plate_solve/cpp/ipv/src/ipv_select.cpp`（中心点、U 坐标、Gnomonic 正投影）
- `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp`（WCS/SIP 提取、Y-up→Y-down 转换）
- `lib/plate_solve/cpp/ipv/src/ipv_solver.cpp`（Gnomonic 反投影、iterative_reproject）
- `lib/plate_solve/cpp/ipv/src/ipv_sip.cpp`（SIP 拟合、归一化）
- `lib/plate_solve/cpp/ipv/src/ipv_robust_refine.cpp`（鲁棒精化，Y-up 内部）
- `lib/photometric_calib/cpp/src/wcs_transform.cpp`（Photometric WCS 转换）
- `lib/healpix_db/healpix_drizzle/wcs_sip.cpp`（Drizzle WCS 转换）
- `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`（Drizzle 引擎，像素四角 ±0.5）
- `lib/healpix_db/healpix_drizzle/poly_clip.cpp`（Gnomonic 投影，与 wcs_sip 一致）
- `lib/snr_estimator/cpp/src/snr_estimator.cpp`（SNR 像素坐标 + 控制点 ra/dec）
- `lib/gaia_xpsd_client/src/gaia_client.h`（Gaia 星表结构）
- `lib/astro_image_io/src/aio_fits.cpp`（FITS 读写）
- `lib/astro_image_io/src/aio_xisf.cpp`（XISF 读写）

### C++ 头文件
- `lib/plate_solve/cpp/ipv/include/ipv_itertrans.h`（TRANS + apply_trans）
- `lib/plate_solve/cpp/ipv/include/ipv_sip.h`（SIP 模型 + cos(dec) 注释）
- `lib/plate_solve/cpp/ipv/include/ipv_solver.h`（extract_wcs_sip 接口）
- `lib/plate_solve/cpp/ipv/include/ipv_api.h`（CRPIX 1-based 注释）
- `lib/plate_solve/cpp/ipv/include/ipv_types.h`（crpix 1-based 注释）
- `lib/photometric_calib/cpp/src/wcs_transform.h`（坐标约定注释块）
- `lib/astro_image_io/include/astro_image_io.h`（结构体定义）

### Python
- `lib/plate_solve/python/solve_and_write_wcs.py`（WCS 关键字写入 FITS）
- `lib/plate_solve/python/verify_y_orientation.py`（Y 方向验证）
- `lib/plate_solve/python/ipv_solver.py`（Python 绑定）
- `lib/plate_solve/python/debug_alignment.py`（CRPIX 1-based↔0-based 验证）
- `lib/plate_solve/python/visualize_reproject.py`（astropy 0-based 像素坐标）
- `lib/astro_image_io/python/astro_image_io.py`（WCSKeywordsPy + ImageData）
- `lib/astro_image_io/astro_image_io/wcs_keywords.py`（WCSKeywords 纯 Python）
- `lib/astro_image_io/astro_image_io/image_data.py`（ImageGeometry.shape）
- `lib/astro_image_io/astro_image_io/writer.py`（FITSWriter）
- `lib/astro_image_io/astro_image_io/fits_reader.py`（FITSReader）

### 规范文档
- `engineering_v1.2/docs/05_WCS_COORDINATE_CONVENTION_AND_CLOSURE_SPEC.md`（WCS 坐标约定与闭合性规范）
- `lib/plate_solve/IPV_PIPELINE.md`（管线说明，CD 无 1/cos(Dec)）
- `lib/plate_solve/cpp/ipv/SIRIL_COMPARISON.md`（与 Siril 对比）
