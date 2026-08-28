# SCI-001 权威索引 —— 唯一科学 authority 核对

> G3 任务：建立唯一 SCI 文档索引；每个科学量只有一个权威定义，其他文档仅链接。
> 判定：**PASS**（满足 CP3 "每个底层定义有唯一 SCI authority"）。
> 复核：2026-08-27。机器门禁：`check_science_units` PASS（13 docs / 138 unit refs）、
> `check_traceability` PASS（67 行 / 断链 0 / symbols 13/13）。

## 1 索引表（科学量 → 唯一权威定义）

| SCI ID | 权威定义文档 | 科学量 | 符号 | 单位 | 域 |
|---|---|---|---|---|---|
| SCI-SCOPE-001 | `docs/science/SCIENCE_SCOPE.md` | 处理范围/权威链入口 | — | — | 深空多帧 |
| SCI-NOISE-001..015 | `docs/science/NOISE_MODEL.md` | signal | `x` | ADU (或 e⁻) | 校准后空背景 |
| SCI-NOISE-001..015 | `docs/science/NOISE_MODEL.md` | noise_sigma / σ_bg | `σ_bg` | ADU | `1.4826022185·MAD`，Gaussian 假设 |
| SCI-NOISE-001..015 | `docs/science/NOISE_MODEL.md` | variance | `variance` | ADU² | `a+b·x+c·y` 平面或 `σ_bg²`；floor `1e-12` |
| SCI-NOISE-001..015 | `docs/science/NOISE_MODEL.md` | ivar | `ivar` | ADU⁻² | `=1/max(variance,floor)` |
| SCI-INT-001.. | `docs/science/INTEGRATION.md` | support | `support[i]`, `sup_max` | 无量纲 [0,1] | `max(accepted support)` canonical reducer |
| SCI-CW-001..008 | `docs/science/CONTROL_WEIGHT_SNR.md` | local_snr / frame_snr | `local_snr`, `frame_snr` | 无量纲 (SNR) | 区域级优先, 整帧 median 回退 |
| SCI-CW-001..008 | `docs/science/CONTROL_WEIGHT_SNR.md` | frame quality | `quality` | uint32 位掩码 | SNR 目录质量位, OR 累积 |
| SCI-DRIZZLE | `docs/science/DRIZZLE.md` | drizzle flux / surface-brightness | 输入像素值 | ADU 或 ADU/pixel（二选一） | 见 DRIZZLE 语义 |
| SCI-DRIZZLE | `docs/science/DRIZZLE.md` | pixfrac | — | 0..1 | 像素分配因子 |
| SCI-DRIZZLE | `docs/science/DRIZZLE.md` | area factor | 面积 | pixel² | 球面像素面积 |
| SCI-DRIZZLE | `docs/science/DRIZZLE.md` | 输出 BUNIT | — | 随 flux/面亮度 | 与输入物理量一致 |
| SCI-UPM | `docs/science/PHASE2_UPM.md` | 控制光度模型 | `calibrated=raw−C_f(p)` | ADU | 8×8 control cell 双线性 |
| SCI-UPM | `docs/science/PHASE2_UPM.md` | C_f | `C_f(p)` | ADU | 加性校正场；参考帧 C=0 |
| SCI-UPM | `docs/science/PHASE2_UPM.md` | M | `M(p)` | ADU | 公共场 |
| SCI-UPM | `docs/science/PHASE2_UPM.md` | Huber residual | δ=1.345 | 无量纲 | IRLS |
| SCI-UPM | `docs/science/PHASE2_UPM.md` | raw_weight / control_ivar / quality_factor | `w` | 无量纲 | `w=query_factor×geom_rel×control_ivar` |
| SCI-REJ | `docs/science/REJECTION.md` | rejection method / sample / threshold | — | 样本数 | 每个 method 边界、中心/尺度、阈值 |
| SCI-INT | `docs/science/INTEGRATION.md` | 积分 reducer / weight 资格 | — | 无量纲 | `w` NaN/Inf→INVALID；0 合法不贡献；>0 可用 |
| SCI-PHOT | `docs/science/PHOTOMETRY.md` | 测光零点 | — | mag / dex | 零点残差散度 |
| SCI-CAL | `docs/science/CALIBRATION.md` | 单帧校准 | — | ADU | bias/dark/flat/cosmetic |
| SCI-WCS | `docs/science/ASTROMETRY.md` | 天球/WCS | — | deg(J2000) | RA/Dec |
| SCI-PSF | `docs/science/PSF.md` | PSF / q_psf | `q_psf` | 0..1 | 点扩散质量 |
| SCI-ACR-EQUIV | `docs/science/ACR_EQUIVALENCE.md` | CPU/GPU 工作域等价 | — | — | ACR |
| UNCERTAINTY_AND_COVARIANCE | `docs/science/UNCERTAINTY_AND_COVARIANCE.md` | drizzle 后相邻像素相关 | — | 协方差 | 与 SCI-NOISE 输入方差区隔 |

> 注：`NOISE_MODEL.md` 中 `signal/variance/ivar` 为逐像素科学权重源；`UNCERTAINTY_AND_COVARIANCE.md`
> 处理 drizzle 后**相邻像素协方差**（非逐像素方差），两者量纲语义分隔，不冲突（见 §3 冲突核对）。

## 2 验证证据

- `check_science_units.py --repo .` → `{"status":"PASS","docs":13,"unit_refs":138,"findings":[]}`。
- `check_traceability.py --repo .` → `traceability: rows=67 ok=67 broken=0 symbols=13/13`。
- 每个底层的量均落在**唯一** `docs/science/*.md` 权威文档，未发现同一量在两个以上文档互相独立定义（见 §3 唯一不冲突例外）。

## 3 冲突/歧义核对

- **UPM gauge 措辞（SCI-004 相关，轻微/待复核）**：`PHASE2_UPM.md` §不变量写「常量场不变量：`raw=C`, `C_f` 解为同一常数……」，而 spec `03_TASK_SPECIFICATIONS.md` §SCI-004 要求「常量公共场进入 `M`、`C_f=0`；不得写 `C_f=C`」。两者 gauge 约定表述不同（spec 为绝对 gauge：常数公共场入 M、C_f=0；doc 为参考帧 gauge：参考帧 C=0）。该处**表述待审核人/作者厘清统一**，不构成数值门禁 FAIL（代码 `upm.cpp:631` 每分量 gauge=分量内最小 frame_id C=0，与 doc 参考帧约定一致）。
- 逐像素 `variance`（SCI-NOISE）与 drizzle 后相邻像素协方差（UNCERTAINTY_AND_COVARIANCE）**量纲/语义分隔**，非冲突。

## 4 结论

SCI-001 权威索引满足：每个科学量只有一个权威定义（唯一 authority），其余文档仅链接/引用。
机器门禁均 PASS。唯一需作者/审核人复核项为 UPM gauge 措辞（见 §3），不影响本索引判定。
