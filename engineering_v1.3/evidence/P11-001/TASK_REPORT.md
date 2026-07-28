# 任务报告

- Task/ADR：P11-001 冻结内部/图像/FITS/WCS坐标约定
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

依据 `tasks/P11-001.md` 和 `docs/05_WCS_COORDINATE_CONVENTION_AND_CLOSURE_SPEC.md`，冻结内部/图像/FITS/WCS 坐标约定，逐函数记录原点、基准、Y 方向、1-based/0-based、SIP 变量。禁止先改符号。

## 输入与范围

- 依赖（已满足）：P09-002（共享检测主线与统一命名）
- 参考规范：`docs/05_WCS_COORDINATE_CONVENTION_AND_CLOSURE_SPEC.md`
- 审计范围：
  - lib/plate_solve/（PlateSolve C++ + Python）
  - lib/astro_image_io/（FITS/XISF 读写 C++ + Python）
  - lib/photometric_calib/（Photometric WCS 转换）
  - lib/healpix_db/healpix_drizzle/（Drizzle WCS 转换）
  - lib/snr_estimator/（SNR 像素坐标）
  - lib/gaia_xpsd_client/（Gaia 星表）
  - lib/healpix_db/healpix_browser_qt/（球面浏览器）
- 工具：`engineering_v1.2/evidence/P11-001/scripts/verify_convention.py`（19 项验证测试）

## 执行/决策

### 阶段 1：并行审计三个模块群

启动 3 个并行 search agent 分别审计：
1. **PlateSolve 代码坐标约定**（原点/Y方向/1-based/SIP）
2. **astro_image_io 坐标处理**（FITS/XISF 读写）
3. **Photometric/SNR/Drizzle 坐标使用**（消费方约定）

### 阶段 2：识别 7 个坐标系统

| 系统 | 原点 | Y 方向 | 索引基准 | 单位 | 用途 |
|------|------|--------|----------|------|------|
| S1 图像数组坐标 | 左上角 (0,0) | Y 向下 | 0-based | 像素 | detector 输出, numpy arr[y,x] |
| S2 内部 U 坐标 | 图像中心 (w/2, h/2) | Y 向上 | 0-based | 像素 | PlateSolve 内部匹配/拟合 |
| S3 FITS WCS 像素 | 左上角参考像素 | Y 向下 | 1-based | 像素 | CRPIX/CD/SIP 写入 FITS |
| S4 切平面中间坐标 | 投影中心 | Y 向上 | N/A | 角秒 | Gnomonic 投影输出（内部） |
| S5 天球坐标 | ICRS J2000 | Dec 北为正 | N/A | 度 | Gaia/CRVAL/Photometric/SNR/Drizzle |
| S6 HEALPix 球面像素 | NESTED 序号 | N/A | 0-based | 索引 | Drizzle 累加/HISS/HCSD |
| S7 浏览器笛卡尔坐标 | 球心 (0,0,0) | Z=北天极 | 右手系 | 单位向量 | X=春分点, Y=RA=90° |

### 阶段 3：确认 7 个关键转换函数

1. **S1→S2**：detector→U，`U.y = -(det_y - cy)`（Y 反转）
2. **S2→S4**：U→xi,eta via TRANS（像素→角秒）
3. **S5→S4**：天球→切平面，Gnomonic 正投影（度→角秒）
4. **S4→S5**：切平面→天球，Gnomonic 反投影（角秒→度）
5. **S2→S3**：U→FITS WCS，Y-up→Y-down 转换（CD.cd12/cd22 取反 + SIP 符号调整）
6. **S3→S5**：FITS WCS 像素→天球，标准 WCS（CD 直接应用，不显式乘 cos(Dec)）
7. **S5→S3**：天球→FITS WCS 像素，标准 WCS 逆向

### 阶段 4：冻结关键变量

| 变量 | 冻结值 | 代码位置 |
|------|--------|----------|
| 内部中心点 cx,cy | `img_w/2.0, img_h/2.0`（无 +0.5） | ipv_select.cpp:682 |
| U.y Y 反转 | `-(det_y - cy)` | ipv_select.cpp:687 |
| CRPIX 基准 | 1-based FITS | ipv_wcs.cpp:285-288 |
| CRPIX 公式 | `width/2.0 + 0.5` | ipv_wcs.cpp:287-288 |
| CD 矩阵格式 | 标准 WCS（无独立 1/cos(Dec)） | ipv_wcs.cpp:273-276 |
| CD 生成 | TRANS/3600（角秒→度） | ipv_wcs.cpp:273-276 |
| CD 消费方 | 直接应用，不显式乘 cos(Dec) | wcs_transform.cpp, wcs_sip.cpp |
| SIP 索引 | `A[i*6+j]` 对应 `dx^i*dy^j` | ipv_wcs.cpp:351-367 |
| SIP A 符号（Y-flip） | `× (-1)^j` | ipv_wcs.cpp:564 |
| SIP B 符号（Y-flip） | `× -(-1)^j` | ipv_wcs.cpp:565 |
| SIP AP 符号 | `× (-1)^j`（同 A） | ipv_wcs.cpp |
| SIP BP 符号 | `× -(-1)^j`（同 B） | ipv_wcs.cpp |
| CRVAL（Y-flip 后） | 不变 | ipv_wcs.cpp |
| CRPIX（Y-flip 后） | 不变 | ipv_wcs.cpp |
| 图像 shape | `(height, width)` 单通道 | astro_image_io.py:367-368 |
| NAXIS1/NAXIS2 | width/height | aio_fits.cpp:418-421 |
| 像素存储 | 行主序 `data[y*width+x]` | aio_fits.cpp:449 |
| has_wcs 判定 | CTYPE 非空 + CD |val|>1e-15 | aio_fits.cpp:327-329 |
| pixel_scale | `sqrt(|det(CD)|)*3600` 角秒/像素 | astro_image_io.py:71-77 |
| rotation_deg | `atan2(cd2_1, cd1_1)` 度 | astro_image_io.py:80-87 |
| Gnomonic 输出 | (xi, eta) 角秒 | ipv_select.cpp:517-545 |
| Gaia ra/dec | 度, ICRS J2000 | gaia_client.h:19-29 |

### 阶段 5：Y 轴反转链确认

反转在两个环节发生：
1. **输入侧**（detector→U）：`ipv_select.cpp:687`，`U.y = -(det_y - cy)`
2. **输出侧**（U→FITS WCS）：`ipv_wcs.cpp:540-581`，CD.cd12/cd22 取反 + SIP 符号调整

中间环节（solver/itertrans/robust_refine）统一使用 Y-up。

### 阶段 6：四模块一致性确认

| 模块 | CRPIX | 像素 | 天球 | 投影 | CD cos(Dec) |
|------|-------|------|------|------|-------------|
| PlateSolve 输出 | 1-based | 0-based 内部 | 度 | TAN+SIP | 标准 WCS |
| Photometric | 1-based | 0-based 输入 | 度 | TAN+SIP | 不显式乘 |
| SNR | 1-based | 0-based | 度 | TAN+SIP 前向 | 不显式乘 |
| Drizzle | 1-based | 0-based | 度 | TAN+SIP | 不显式乘 |
| astro_image_io | 原样 1-based | shape=(h,w) | N/A | N/A | 原样读写 |

### 阶段 7：球面浏览器独立坐标系确认

- 右手笛卡尔：X=春分点, Y=RA=90°, Z=北天极
- 相机在球心，向外看
- north-up up vector 从 ra/dec 重算，绝不携带
- MAX_FOV=50°
- 与 WCS 像素坐标不直接交换，仅通过 HEALPix 像素的 (ra,dec) 交互

### 阶段 8：禁止捷径确认

- ✅ 未修改任何代码（git diff 确认）
- ✅ 未先改符号（仅文档冻结）
- ✅ 未在 Photometric 中补偿 Y（统一由 WCS 生产端处理）
- ✅ 未用旧路径 A/B 替代闭环验证（闭环验证将在 P11-002/P11-003 执行）

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python verify_convention.py` | 60s | 0（19/19 PASS） |

## 结果与证据

### 交付物

1. **COORDINATE_CONVENTION.md** — 坐标约定冻结文档（13229 bytes，10 章 + 附录）
   - 7 个坐标系统定义
   - 7 个关键转换函数
   - 关键变量冻结表
   - 模块间一致性表
   - Y 轴反转链
   - 球面浏览器独立坐标系
   - Gaia 客户端约定
   - 禁止事项
   - 接口注释要求
   - 变更控制流程
   - 关键文件清单（C++/Python/规范文档）
2. **verify_convention.py** — 19 项验证脚本
3. **TASK_REPORT.md** — 本报告
4. **TEST_REPORT.md** — 测试报告
5. **EVIDENCE_INDEX.md** — 证据索引
6. **REVIEW_REPORT.md** — 独立复核报告

### 关键统计

| 指标 | 值 |
|------|-----|
| 坐标系统定义 | 7 |
| 关键转换函数 | 7 |
| 冻结变量 | 22 |
| 审计模块 | 7（plate_solve + astro_image_io + photometric + drizzle + snr + gaia + browser） |
| 验证测试 | 19 |
| 通过测试 | 19 (100%) |
| 代码修改 | 0（禁止捷径 PASS） |
| 接口注释确认 | 7 处 |

## 风险/回滚/残留

- **Y 轴符号假设未验证**：Y 轴和 SIP 奇次项符号是待验证假设，本任务仅冻结现状，验证将在 P11-002（标准 WCS 真实星对闭环诊断工具）和 P11-003（T1-T4 代表帧复现 WCS 闭环缺陷）中执行
- **CRVAL=0 has_wcs 判定**：当前 has_wcs 不检查 CRVAL，CRVAL=0 也认为有 WCS（事实记录，可能需后续讨论）
- **C++ XISF 路径字段差异**：XISF 路径不读取 CDELT1/CDELT2/LONPOLE/LATPOLE，与 FITS 路径有差异（事实记录，建议后续统一）
- **V40PrototypeSolver (Python) 例外**：输出 CD 需乘 cos(Dec)，仅 Python 原型，C++ 已修正（已在 project_memory 记录）
- **无代码修改**：本任务为文档冻结，不涉及代码变更，无回滚需求

## 结论

P11-001 完成。COORDINATE_CONVENTION.md 已生成并冻结（13229 bytes，10 章 + 附录）。7 个坐标系统、7 个关键转换函数、22 个冻结变量已记录。4 个 WCS 消费模块（PlateSolve/Photometric/SNR/Drizzle）约定完全一致。19/19 验证测试 PASS，覆盖 S2 内部坐标（2）+ S3 FITS WCS（5）+ astro_image_io（3）+ Photometric（2）+ Drizzle（1）+ 接口注释（4）+ 禁止捷径（1）+ 交付物（1）。禁止捷径检查通过（无代码修改、无先改符号、无 Photometric 内补偿）。后续 P11-002~P11-005 将基于本冻结约定执行闭环验证与统一修正。
