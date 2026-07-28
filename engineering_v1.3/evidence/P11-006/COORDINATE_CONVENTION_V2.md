# AstroCS 坐标约定文档 v2（P11-006）

> 版本：**v2**
> 日期：2026-07-28
> 升级任务：P11-006（更新坐标契约、CLI capabilities、provenance）
> 前一版本：v1 FROZEN（2026-07-27，P11-001，见 `evidence/P11-001/COORDINATE_CONVENTION.md`）
> 依据：`docs/05_WCS_COORDINATE_CONVENTION_AND_CLOSURE_SPEC.md` + `docs/24_WCS_VALIDATION_V2_SPEC.md` + 代码审计事实
> 约束：本文件发布后，任何坐标相关变更必须先提 ADR，禁止"先改符号"

---

## 1. v1 → v2 变更摘要

| # | 变更项 | v1 | v2 | 触发任务 |
|---|--------|----|----|----------|
| 1 | CRPIX 计算一致性 | L165 与 L287 实现冲突（cx+1.0 vs cx+0.5） | 统一为 `width/2.0 + 0.5`（1-based） | P11-006 修复 `ipv_wcs.cpp:165-167` |
| 2 | WCS+SIP 传递方式 | 默认写入 FITS header | 默认作为管线内存块（PipelineFrame 字段）传递；仅在需持久化时写 FITS header | P11-005 用户确认 |
| 3 | WCS 验证架构 | 单层 WCS 闭环 | A/B/C 三层（Solver Fit / Serialized WCS / Blind Catalog） | `docs/24_WCS_VALIDATION_V2_SPEC.md` |
| 4 | B 层硬 Gate 阈值 | 未量化 | 明确 7 项量化阈值（见 §6） | P11-002 / P11-004 |
| 5 | 权威星对 schema | 无 | 引用 `wcs_authoritative_pairs.schema.json` v1.0（含 provenance 扩展） | P11-004 / P11-006 |
| 6 | SIP 序列化要求 | 仅 A/B | A/B + AP/BP 逆向（V4.20+）+ CTYPE TAN-SIP | P11-004 根因修复 |
| 7 | offset_px 检查 | `validate_wcs` 含 `offset_px < 250` 检查 | 移除（望远镜 pointing 抖动不应作为 WCS Gate） | P11-006 修复 `run_ipv_baseline.py` |
| 8 | CLI capabilities | 未声明 schema_versions | 声明 `wcs_authoritative_pairs:"1.0"`、`wcs_closure_report:"1.0"`、`coordinate_convention:"2"` | P11-006 更新 `cli_command.cpp` |

**兼容性**：v2 与 v1 在坐标系定义、Y 轴反转链、CD cos(Dec) 处理上完全一致；v2 仅修复实现冲突、量化阈值、明确传递方式，不改变坐标语义。

---

## 2. 坐标系统定义（从 v1 继承）

| 系统 | 原点 | Y 方向 | 索引基准 | 单位 | 用途 |
|------|------|--------|----------|------|------|
| **S1 图像数组坐标 (detector)** | 左上角 (0,0) | Y 向下（行号递增） | 0-based | 像素 | `star_detector` DLL 输出，numpy `arr[y,x]` |
| **S2 内部 U 坐标 (plate_solve)** | 图像中心 `(w/2, h/2)` | Y 向上 | 0-based | 像素 | PlateSolve 内部匹配/拟合 |
| **S3 FITS WCS 像素坐标** | 左上角参考像素 | Y 向下（FITS 行号递增） | **1-based** | 像素 | CRPIX/CD/SIP 写入 FITS header 或 PipelineFrame |
| **S4 切平面中间坐标 (xi, eta)** | 投影中心 | Y 向上（与 Dec 同向） | N/A | **角秒**（plate_solve 内部） | Gnomonic 投影输出 |
| **S5 天球坐标 (ra, dec)** | ICRS 历元 J2000 | Dec 向北为正 | N/A | 度 | Gaia 星表、CRVAL、Photometric/SNR/Drizzle |
| **S6 HEALPix 球面像素** | NESTED 序号 | N/A | 0-based | 像素索引 | Drizzle 累加、HISS/HCSD 存储 |
| **S7 浏览器笛卡尔坐标** | 球心 (0,0,0) | Z=北天极 | 右手系 | 单位向量 | X=春分点(RA=0), Y=RA=90° |

**核心约定**：
- **Solver U 坐标**：图像中心原点，Y 轴向上（S2）
- **FITS 像素坐标**：左下角原点（实际为左上角参考像素），Y 轴向下，0-based（S3 的 detector 输入侧）
- **FITS CRPIX**：1-based，`width/2.0 + 0.5`（S3 的参考像素）
- **天球坐标**：RA/Dec（度），ICRS J2000（S5）

---

## 3. 坐标转换规则

### 3.1 U → FITS 0-based（detector 输入侧）

```
px = U.x + (width-1)/2.0
py = (height-1)/2.0 - U.y
```

- 输入：U 坐标（原点=图像中心，Y 向上）
- 输出：FITS 0-based 像素坐标（Y 向下）
- 实现：`ipv_select.cpp:682-687`（U 生成）+ `ipv_wcs.cpp:540-581`（Y 反转）

### 3.2 U → FITS 1-based CRPIX

```
CRPIX1 = width/2.0 + 0.5
CRPIX2 = height/2.0 + 0.5
```

- 1-based 参考 pixel，FITS WCS Paper I 标准
- 实现：`ipv_wcs.cpp:285-288`（L287 一直正确）+ `ipv_wcs.cpp:165-167`（P11-006 修复后一致）
- Python 写入：`solve_and_write_wcs.py:443-444`（直接使用 C++ 返回值，不再 +1）

### 3.3 FITS 0-based → 1-based

```
FITS_1based = FITS_0based + 1
```

- 通用偏移规则，适用于 CRPIX 及任意像素坐标转换
- 消费方（Photometric/SNR/Drizzle）在 `skyToPixel` / `pixelToSky` 内部统一应用 `crpix - 1.0`

### 3.4 S2 → S4（U → xi,eta via TRANS）

- 位置：`ipv_itertrans.h:121-138` (`apply_trans`)
- 输入：U 坐标（像素，原点中心，Y 向上）
- 输出：(xi, eta) 角秒，原点投影中心
- 阶数 1/2/3 对应 6/12/20 参数

### 3.5 S5 ↔ S4（天球 ↔ 切平面，Gnomonic 正/反投影）

- 正投影：`ipv_select.cpp:517-545` (`gnomonic_forward_proj`)，(ra,dec) 度 → (xi,eta) 角秒
- 反投影：`ipv_solver.cpp:65-100` (`gnomonic_inverse_proj`)，(xi,eta) 角秒 → (ra,dec) 度
- 标准 Calabretta & Greisen 2002 公式

### 3.6 S3 ↔ S5（FITS WCS 像素 ↔ 天球，标准 WCS）

- 位置：`photometric_calib/cpp/src/wcs_transform.cpp:173-188`、`healpix_db/healpix_drizzle/wcs_sip.cpp`
- 前向（pixel→sky）：`dx = x - (crpix1 - 1.0); dy = y - (crpix2 - 1.0); xi = cd1*dx + cd2*dy; eta = cd3*dx + cd4*dy; tanIntermediateToWorld(xi,eta,ra,dec)`
- 逆向（sky→pixel）：TAN 投影 → CD⁻¹ → 逆向 SIP AP/BP → + CRPIX（转 1-based）
- **CD 矩阵直接应用，不显式乘 cos(Dec)**（cos(Dec) 已由 plate solver 在 CD 求解时隐含）

---

## 4. A/B/C 三层验证架构

> 来源：`docs/24_WCS_VALIDATION_V2_SPEC.md`
> 目标：同时验证三件不同的事——求解器拟合质量、标准 WCS 序列化质量、盲目录匹配健康度。三者不得混成单一残差。

### 4.1 A 层 — Solver Fit

- **来源**：IPV 最终 RANSAC/Umeyama inliers
- **报告**：`n_inliers`、`solver_rms_px`、内部残差分布、TRANS/SIP 阶数
- **作用**：参考基线，**不能单独证明 Header/PipelineFrame WCS 正确**

### 4.2 B 层 — Serialized WCS（P11 硬 Gate）

- **方法**：固定 A 层的对应关系，仅使用最终 Header/PipelineFrame WCS/SIP 回投 Gaia 星
- **输出**：
  - `external_to_detector`：WCS 预测像素 vs detector 质心
  - `external_to_internal_prediction`：WCS 预测像素 vs 求解器内部预测
  - median/p68/p90/p99/RMS/max
  - X/Y mean、median、MAD
  - 四象限和边缘分布
  - 每个 pair 的稳定 ID
- **作用**：P11 硬 Gate，必须通过

### 4.3 C 层 — Blind Catalog（二级诊断）

- **用途**：发现检测质量、拥挤场、饱和、proper motion、星等选择和 Photometric 匹配问题
- **要求**：
  - 排除饱和、边缘、严重 blend
  - Gaia proper motion 可用时传播到观测历元
  - 一对一匹配，不允许多个 Gaia 指向同一 detection
  - 候选半径由 B 层误差与 PSF FWHM 决定
  - 优先 mutual nearest neighbor，必要时最小代价分配
  - 分星等/FWHM/饱和/边缘报告
- **作用**：**非硬 Gate**；若 C 失败而 B 通过，应进入 Photometric 匹配任务，不得回头改 WCS

---

## 5. B 层硬 Gate 阈值

> 来源：`docs/24_WCS_VALIDATION_V2_SPEC.md` §B Gate + P11-004 实践

| # | 指标 | 阈值 | 说明 |
|---|------|------|------|
| 1 | 星对数一致性 | = 求解器最终 inlier 数 | 若剔除非有限值，必须逐项列出原因 |
| 2 | `external_to_internal_prediction` median | ≤ 0.05 px | 旧 API 暂不能导出内部预测时，可临时使用差分 RMS Gate，但必须在 P11-006 补齐接口 |
| 2 | `external_to_internal_prediction` p99 | ≤ 0.20 px | 同上 |
| 3 | `external_to_detector_rms` | ≤ max(0.35 px, 2 × solver_rms_px + 0.05 px) | 兜底 0.35 px，避免低 RMS 求解器过严 |
| 4 | `external_to_detector` median | ≤ 0.50 px | — |
| 4 | `external_to_detector` p90 | ≤ 1.00 px | — |
| 4 | `external_to_detector` p99 | ≤ 2.00 px | — |
| 5 | \|X mean\| | ≤ 0.25 px | 且无象限翻转、尺度漂移、90/180° 旋转 |
| 5 | \|Y mean\| | ≤ 0.25 px | 同上 |
| 6 | WCS 对象数值闭环 median | ≤ 1e-6 px | pixelToSky(skyToPixel(p)) ≈ p |
| 7 | 代表帧全部通过 | — | 才可进入 710 回归 |

**阈值调整原则**：门限若被真实权威星对证明不合理，必须用分布和 ADR 调整，**禁止为了当前帧临时放宽**。

---

## 6. SIP 序列化要求

> 来源：P11-004 根因分析（6/16 帧因 SIP 未序列化导致 B 层失败）+ V4.20 实现

### 6.1 前向 SIP（A/B）

- `A_ORDER` / `B_ORDER`：前向 SIP 阶数
- `A_i_j` / `B_i_j`：前向 SIP 系数（pixel → intermediate）
- 索引约定：`A[i*6+j]` 对应 `dx^i * dy^j`，i+j ≤ order，最大 5 阶（36 项）
- 归一化：`ipv_sip.cpp:470-488` 中 `A[i][j] = coeff[k] / scale^(i+j)`

### 6.2 逆向 SIP（AP/BP，V4.20+）

- `AP_ORDER` / `BP_ORDER`：逆向 SIP 阶数
- `AP_i_j` / `BP_i_j`：逆向 SIP 系数（intermediate → pixel）
- 生成方式：网格反变换法（`NB_GRID_POINTS=7`）
- **必须序列化**：消费方 `skyToPixel` 依赖逆向 SIP 实现高精度反向投影

### 6.3 CTYPE 约定

- `CTYPE1 = RA---TAN-SIP`
- `CTYPE2 = DEC--TAN-SIP`
- 未带 SIP 时退化为 `RA---TAN` / `DEC--TAN`

### 6.4 Y 轴反转符号规则（U → FITS WCS 出口）

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

---

## 7. 权威星对 schema

> 引用：`engineering_v1.3/contracts/wcs_authoritative_pairs.schema.json` v1.0（P11-006 扩展 provenance）

### 7.1 顶层字段

- `schema_version`（integer, min=1）：schema 版本
- `frame_id`（string）：帧标识
- `input_sha256`（string）：输入哈希
- `solver_version`（string）：求解器版本（扁平字段，向后兼容）
- `detection_hash`（string）：检测结果哈希（扁平字段，向后兼容）
- `solver_rms_px`（number）：A 层求解器 RMS
- `n_inliers`（integer, min=1）：A 层 inlier 数
- `pairs`（array）：权威星对列表

### 7.2 pairs 元素

每对包含：`pair_id`、`gaia_source_id`、`gaia_ra_deg`、`gaia_dec_deg`、`detector_x_px`、`detector_y_px`、`internal_pred_x_px`、`internal_pred_y_px`、`internal_residual_px`、`saturated`、`edge`、`blend`

### 7.3 provenance 对象（P11-006 新增，可选）

```json
{
  "provenance": {
    "solver_version": "ipv_v4.30",
    "solver_commit": "<git commit hash>",
    "gaia_catalog_version": "DR3SP",
    "detection_hash": "<sha256 前16位>",
    "observation_epoch": "J2000.0",
    "wcs_closure_summary": {
      "layer_a_solver_rms_px": 0.285,
      "layer_b_external_p68_px": 0.158,
      "layer_b_external_p90_px": 0.42,
      "layer_b_external_p99_px": 1.20,
      "gate_passed": true
    },
    "software_commit_config_hash": "<sha256>"
  }
}
```

- `required`：`["solver_version", "gaia_catalog_version"]`
- `additionalProperties`: false
- 与顶层扁平字段 `solver_version` / `detection_hash` 并存（向后兼容）

---

## 8. WCS 传递方式

> P11-005 用户确认：WCS+SIP 作为管线内存块传递，不写入 FITS header 是设计如此

### 8.1 管线内存块（默认方式）

- **载体**：PipelineFrame 结构体字段（WCS + SIP 系数对象）
- **生命周期**：求解器输出 → Photometric / SNR / Drizzle 消费，全程内存传递
- **优点**：
  - 无 FITS header 序列化/反序列化开销
  - 避免 1-based/0-based 转换误差
  - 避免 SIP 系数精度损失（FITS keyword 为浮点字符串）
  - 支持 PipelineFrame 携带完整 5 阶 SIP + 逆向 AP/BP
- **适用场景**：管线内部所有模块间传递

### 8.2 FITS header（持久化方式）

- **载体**：FITS / XISF header keywords（CRPIX、CD、CRVAL、CTYPE、A_ORDER、A_i_j、AP_ORDER、AP_i_j 等）
- **触发条件**：
  - 交付给外部工具（astropy、Siril、DS9）
  - 持久化到磁盘供离线分析
  - 与第三方管线交换数据
- **要求**：必须完整序列化 CRPIX（1-based）+ CD + CRVAL + CTYPE TAN-SIP + 前向 A/B + 逆向 AP/BP

### 8.3 一致性保证

无论哪种传递方式，WCS 对象的数值表示必须等价：
- CRPIX 一致（1-based, `width/2.0 + 0.5`）
- CD 矩阵一致（度/像素，含 cos(Dec) 隐含）
- SIP 系数一致（前向 A/B + 逆向 AP/BP，Y 翻转符号规则统一）
- CTYPE 一致（`RA---TAN-SIP` / `DEC--TAN-SIP`）

---

## 9. 模块间一致性（从 v1 继承）

| 模块 | 文件 | CRPIX | 像素 | 天球 | 投影 | CD cos(Dec) |
|------|------|-------|------|------|------|-------------|
| PlateSolve (输出) | `ipv_wcs.cpp` | 1-based | 0-based 内部 | 度 | TAN+SIP | 标准 WCS（无独立因子） |
| Photometric | `wcs_transform.cpp` | 1-based | 0-based 输入 | 度 | TAN+SIP | 不显式乘 |
| SNR | `snr_estimator.cpp` | 1-based | 0-based | 度 | TAN+SIP（前向 A/B） | 不显式乘 |
| Drizzle | `wcs_sip.cpp` | 1-based | 0-based | 度 | TAN+SIP | 不显式乘 |
| astro_image_io | `astro_image_io.py` | 原样 1-based | shape=(h,w) | N/A | N/A | 原样读写 |

**结论**：四个 WCS 消费模块约定完全一致。

---

## 10. Y 轴反转链（从 v1 继承）

反转在两个环节发生：

1. **输入侧**（detector → U）：`ipv_select.cpp:687`，`U.y = -(det_y - cy)`，图像 Y 向下转内部 Y 向上
2. **输出侧**（U → FITS WCS）：`ipv_wcs.cpp:540-581`，CD.cd12/cd22 取反 + SIP 符号调整，内部 Y-up 转 FITS Y-down

**中间环节**（solver/itertrans/robust_refine）统一使用 Y-up，不在内部做翻转。

---

## 11. 关键变量冻结

### 11.1 CRPIX（参考像素）

- **基准**：1-based（FITS WCS Paper I 标准）
- **计算公式**：`CRPIX1 = width/2.0 + 0.5`，`CRPIX2 = height/2.0 + 0.5`
- **位置**：`ipv_wcs.cpp:285-288`（L287 一直正确）+ `ipv_wcs.cpp:165-167`（P11-006 修复后与 L287 一致）
- **Python 写入**：`solve_and_write_wcs.py:443-444`（直接使用 C++ 返回值，不再 +1）
- **读取**：`astro_image_io` 模块原样读写，不做 0-based/1-based 转换

### 11.2 CD 矩阵

- **格式**：标准 WCS（含 cos(Dec) 缩放，无独立 1/cos(Dec) 因子）
- **单位**：度/像素
- **生成位置**：`ipv_wcs.cpp:273-276`（TRANS 线性项 / 3600）
- **消费方约定**：Photometric/SNR/Drizzle 直接应用 CD，**不显式乘 cos(Dec)**

### 11.3 SIP 系数

- **索引约定**：`A[i*6+j]` 对应 `dx^i * dy^j`，i+j ≤ order，最大 5 阶（36 项）
- **存储位置**：`ipv_types.h`（`SipCoeffs` 结构）
- **前向 A/B**：pixelToSky 使用，由 `extract_wcs_sip` 解析公式提取
- **逆向 AP/BP**：skyToPixel 使用，由网格反变换法（`NB_GRID_POINTS=7`）生成
- **归一化**：`ipv_sip.cpp:470-488` 中 `A[i][j] = coeff[k] / scale^(i+j)`

### 11.4 图像数组 shape

- **单通道**：`(height, width)`（numpy 行主序）
- **多通道**：`(channels, height, width)`（CHW，与 PCL 约定一致）
- **NAXIS1 = width, NAXIS2 = height**（FITS 标准）
- **像素存储**：行主序 `data[y*width + x]`

### 11.5 has_wcs 判定

- **条件**：`CTYPE1/CTYPE2 非空 且 CD 矩阵至少一个元素 |val| > 1e-15`
- **三处实现一致**：C++ FITS、C++ XISF、Python

---

## 12. CLI capabilities 声明（P11-006 新增）

> 位置：`lib/orchestrator/cpp/src/cli_command.cpp` L1696, L1715

ipv_solver capabilities 新增：
- `export_authoritative_pairs`：导出权威星对 JSON
- `wcs_sip_serialization`：完整 SIP 序列化（A/B + AP/BP）

schema_versions 新增：
- `wcs_authoritative_pairs: "1.0"`
- `wcs_closure_report: "1.0"`
- `coordinate_convention: "2"`

---

## 13. 禁止条款

依据 `docs/05_WCS_COORDINATE_CONVENTION_AND_CLOSURE_SPEC.md` 修复原则 + P11-006 新增约束：

1. **不得先改符号**：Y 轴和 SIP 奇次项符号是待验证假设，必须由诊断确定确切转换
2. **不得只在 Photometric 中补偿**：最终修复必须统一服务于 Photometric、SNR、Drizzle 和浏览器
3. **不得用旧路径 A/B 一致替代标准 WCS 闭环验证**
4. **不得在 Photometric 内部偷偷翻转 Y 来掩盖错误的 WCS 生产端**
5. **禁止多套 WCS 变换**：Photometric / SNR / Drizzle 必须使用同一 WCS 实现（共用 `wcs_transform` / `wcs_sip`）
6. **禁止未经 ADR + 闭环验证就改坐标符号**：任何坐标相关变更必须先提 ADR + 通过 B 层闭环验证
7. **禁止用全星表 kd-tree p68 作为唯一 WCS Gate**：C 层盲目录匹配仅作诊断，WCS 硬 Gate 以 B 层权威星对为准
8. **禁止用望远镜 pointing 偏差作为 WCS Gate**：`offset_px` 检查已在 P11-006 移除（pointing 抖动是正常现象）

---

## 14. 变更控制

本文件发布后，任何坐标相关变更（CRPIX 基准、CD cos(Dec) 处理、SIP 符号、Y 方向、shape 约定、B 层阈值、SIP 序列化要求、WCS 传递方式）必须：

1. 先提 ADR 记录变更原因与影响
2. 通过 B 层权威星对闭环验证（`wcs_closure_diagnostic_v3.py`）
3. 在代表帧上证明无回归
4. 更新本契约文档版本（v2 → v3）
5. 通过 P12+ 后续任务验证

**禁止跳过闭环验证直接改符号。**

---

## 附录 A：v1 → v2 代码变更清单

| 文件 | 行 | 变更 | 任务 |
|------|----|------|------|
| `lib/plate_solve/python/siril_compare/run_ipv_baseline.py` | validate_wcs | 移除 `offset_px < 250` 检查 | P11-006 |
| `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp` | L165-167 | `crpix[0] = cx + 1.0` → `crpix[0] = cx + 0.5`（与 L287 一致） | P11-006 |
| `lib/orchestrator/cpp/src/cli_command.cpp` | L1696 | ipv_solver capabilities 新增 `export_authoritative_pairs`, `wcs_sip_serialization` | P11-006 |
| `lib/orchestrator/cpp/src/cli_command.cpp` | L1715 | schema_versions 新增 `wcs_authoritative_pairs:"1.0"`, `wcs_closure_report:"1.0"`, `coordinate_convention:"2"` | P11-006 |
| `engineering_v1.3/contracts/wcs_authoritative_pairs.schema.json` | properties | 新增可选 `provenance` 对象 | P11-006 |

---

## 附录 B：关键文件清单（从 v1 继承）

### C++ 实现
- `lib/plate_solve/cpp/ipv/src/ipv_select.cpp`（中心点、U 坐标、Gnomonic 正投影）
- `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp`（WCS/SIP 提取、Y-up→Y-down 转换、CRPIX 统一）
- `lib/plate_solve/cpp/ipv/src/ipv_solver.cpp`（Gnomonic 反投影、iterative_reproject）
- `lib/plate_solve/cpp/ipv/src/ipv_sip.cpp`（SIP 拟合、归一化）
- `lib/plate_solve/cpp/ipv/src/ipv_robust_refine.cpp`（鲁棒精化，Y-up 内部）
- `lib/photometric_calib/cpp/src/wcs_transform.cpp`（Photometric WCS 转换）
- `lib/healpix_db/healpix_drizzle/wcs_sip.cpp`（Drizzle WCS 转换）
- `lib/orchestrator/cpp/src/cli_command.cpp`（CLI capabilities 声明）
- `lib/astro_image_io/src/aio_fits.cpp`（FITS 读写）
- `lib/astro_image_io/src/aio_xisf.cpp`（XISF 读写）

### Python
- `lib/plate_solve/python/solve_and_write_wcs.py`（WCS 关键字写入 FITS）
- `lib/plate_solve/python/siril_compare/run_ipv_baseline.py`（IPV 基线验证，offset_px 检查已移除）
- `lib/plate_solve/python/verify_y_orientation.py`（Y 方向验证）
- `lib/plate_solve/python/ipv_solver.py`（Python 绑定）
- `lib/astro_image_io/python/astro_image_io.py`（WCSKeywordsPy + ImageData）

### 规范文档
- `engineering_v1.3/docs/05_WCS_COORDINATE_CONVENTION_AND_CLOSURE_SPEC.md`（WCS 坐标约定与闭合性规范）
- `engineering_v1.3/docs/24_WCS_VALIDATION_V2_SPEC.md`（WCS 验证架构 v2，A/B/C 三层）
- `lib/plate_solve/IPV_PIPELINE.md`（管线说明，CD 无 1/cos(Dec)）
- `lib/plate_solve/cpp/ipv/SIRIL_COMPARISON.md`（与 Siril 对比）

### 契约
- `engineering_v1.3/contracts/wcs_authoritative_pairs.schema.json`（权威星对 schema v1.0，含 provenance 扩展）
- `engineering_v1.3/evidence/P11-001/COORDINATE_CONVENTION.md`（v1 FROZEN 前序文档）
