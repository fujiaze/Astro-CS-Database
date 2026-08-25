# Calibration Science (SCI-CAL)

> ID: SCI-CAL-001  状态: FROZEN (T100 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-CAL-001..  模块: calibration

## 1 目的与非目标

- **目的**：去除仪器签名（bias/dark/flat/cosmetic），使多帧信号在同一物理标度下可比较，为后续 PSF/astrometry/photometry、Drizzle、Phase2 UPM/rejection 产出提供校准后帧。
- **非目标**：不校正非线性/电子增益（测光定标层处理）；不估计科学噪声方差（`snr_estimator` 独立估计）；不做天光背景扣除（Phase2 控制采样/UPM 处理）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `raw`/`light` | 单帧观测亮场 | `ac_calibrate_frame` 输入 |
| `bias` | 零曝光本底帧 | `ac_generate_master_bias` |
| `dark` | 暗电流帧（含 bias） | `ac_generate_master_dark` |
| `flat` | 平场帧 | `ac_generate_master_flat` |
| `flat_norm` | 归一后平场 `median=1.0, floor 0.1` | `normalize_flat` |
| `K` | 暗场缩放因子 `K=t_light/t_dark` | `calibrate: k_init` |
| `cal` | 校准后信号 | 输出 `out` |
| `t_light`, `t_dark` | Light/Dark 曝光时长 (EXPTIME) | 调用方 FITS 头 |
| `sigma_low/high`, `max_iter`, `combine` | sigma-clip 阈值/迭代/合并方式 | `generate_master` |
| `hot_sigma/cold_sigma/method/max_structure_size` | 坏点检测/修复参数 | `ac_correct_frame` |

## 3 物理量和单位

- `raw/bias/dark/flat/cal`: ADU（同滤镜/增益下标度）；`t_expo`: s；`K`: 无量纲；`flat_norm`: 无量纲（median=1.0, floor 0.1）；`sigma`: 无量纲倍数（以 MAD 转 sigma）；像素坐标无量纲。

## 4 输入有效域

- 维度: `w>0, h>0, n_frames>=1`；数组指针非空（空指针返回 `AC_ERR_PARAM`）。
- 母版分组: 按曝光时长/滤镜已由 orchestrator 层分组，calibration 层仅接收已分组母版；`t_light` 在母版覆盖范围内。
- 数值: 输入含 NaN 时该像素的 median/MAD 计算跳过 NaN（`generate_master` 工作缓冲仅纳非 NaN），输出不传播 NaN 为伪有效值（见 §9）。
- 平场: `flat` 可为 NULL（跳过除法）；非 NULL 时 `normalize_flat` 将 median<=0 的平场保持原样不归一（避免除零）。

## 5 连续定义

```text
dark_opt=0 (默认, Dark 已含 Bias):
  cal = (raw − dark) / flat_norm

dark_opt=1 (显式 Bias/Dark 分离, K=t_light/t_dark):
  cal = (raw − bias − K·(dark − bias)) / flat_norm

flat_norm = max(flat / median(flat), 0.1)   # median→1.0, 逐像素 floor 0.1
                                            # median<=0 时不归一，保持原样
```

与 `lib/calibration/src/calibrator.cpp:104-136,147-179` 及 `lib/calibration/src/master_generator.cpp:222-234` 一致。`dark_opt=1` 仅在 `bias && dark` 均非 NULL 时生效，否则回退 `dark_opt=0` 语义且 `k→1.0`。

## 6 假设

- `bias` 与曝光无关；`dark` 与曝光线性（`dark_opt=1` 时经 `K` 线性缩放）；
- `flat` 光谱形状与 Light 滤镜匹配；
- 坏点稀疏且与天体源不混淆（连通域大小过滤可分离）。

## 7 独立不变量

- **常量场不变量**：常数输入 `raw=C, dark=D, flat=1.0` 时 `cal = C−D`（`dark_opt=0`）在全帧恒定，无空间调制。
- **空平场不变量**：`flat=NULL` 时退化为减法校准，不引入除法伪影。
- **幂等归一不变量**：对同一 `flat` 连续两次 `normalize_flat` 结果一致（median 已为 1.0，二次归一不变）。
- **确定性不变量**：相同 `raw/bias/dark/flat/K` 输入顺序改变不改变 `cal`（逐像素独立算术，无跨像素归约）。
- **median 鲁棒性**：`generate_master` 在单帧 `n_frames=1` 时直接拷贝，不做 sigma-clip（ `master_generator.cpp: single frame copy`）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| `w<=0`/`h<=0`/空指针 | 返回 `AC_ERR_PARAM`，不写 `out` | `ac_generate_master_bias` 参数校验 |
| `median(flat)<=0` | `normalize_flat` 不归一，保持原样 | `calibrator.cpp:84 if(!(med>0)) return` |
| `flat_norm` 过小 | `max(...,0.1)` floor 避免极大放大 | `calibrator.cpp:78-93,120,130,164,173` |
| `MAD=0` (无离散度) | sigma-clip 提前终止，不再剔除 | `master_generator.cpp: sigma<=0 break` |
| `t_light/t_dark` 极端 | `K` 仍按比值应用，溢出由 FP32 饱和语义界定，不静默 clamp | `calibrate: k=k_init` 直通 |
| 坏点全帧 | `cc_correct_median` 仅修复 `bad_mask=1` 像素，其余不变 | `cosmetic_corrector.cpp` |

## 9 精度策略

- 母版算术与校准核心为 FP32（`float`）；FP64 双精度 ABI `*_f64` 在像素级算术路径使用 `double` 不降级（`calibrator.cpp:147-179 calibrate_d`），其余统计/mask 路径经 `float` 中转（注释已明示）。
- `flat` 除法前 `max(...,0.1)` 保证除数下界 0.1，避免 `1e-7` 量级噪声放大。
- 中位数用 `std::nth_element` O(n)，MAD 转 sigma 系数 `1.4826`（高斯假设）。
- 不传播母版方差至 `cal` 的方差项（ivar 由 `snr_estimator` 独立估计，见 NOISE_MODEL）。

## 10 不可接受变化

- 改变 `flat_norm` 的 `median=1.0` 或 `floor 0.1` 语义而无 SCI 冻结变更；
- 将 `K` 改为优化搜索值而非 `t_light/t_dark` 比值；
- 在 `cal` 层引入非线性/增益校正或背景扣除；
- 使 `normalize_flat` 在 `median<=0` 时仍归一导致除零/Inf 传播。

## 11 验证 Oracle

- **解析解**：常数场 `raw/dark/flat` 组合验证公式精确性（`max_abs==0`）。
- **Python 参考**：NumPy 对同一 `raw/bias/dark/flat/K` 的双分支公式逐像素比对，FP32 `rtol=1e-6, atol=1e-7`。
- **不变性门**：常量场、空平场、幂等归一、确定性四门（见 `TST-CAL-INV-*`）。
- **失败注入**：空指针/零维度/NaN 输入显式错误码 `AC_ERR_PARAM`（见 `TST-CAL-FAIL-*`）。

## 12 关联 ALG ID

- `ALG-CAL-001` MasterBias/Dark 生成（sigma-clip+合并）
- `ALG-CAL-002` MasterFlat 生成（减 Bias→逐帧归一→sigma-clip+mean→再归一）
- `ALG-CAL-003` 单帧校准 `calibrate/calibrate_d`（双分支除法+floor）
- `ALG-CAL-004` 坏点检测/修复 `cc_detect_hot/cold + cc_correct_median`

## 13 追溯与测试

- 权威文件: `docs/science/CALIBRATION.md` (SCI-CAL-001)
- 实现: `lib/calibration/src/calibrator.cpp` (`normalize_flat, calibrate, calibrate_d`), `lib/calibration/src/master_generator.cpp` (`generate_master`), `lib/calibration/cpp/cosmetic_corrector.cpp`
- 公开 API: `lib/calibration/include/astro_calibration.h` (`ac_generate_master_bias/dark/flat, ac_calibrate_frame, ac_correct_frame` 及其 `_f64` 变体)
- 测试: `TST-CAL-001` 常量场、`TST-CAL-INV-001` 幂等归一、`TST-CAL-FAIL-001` 参数校验（新增/映射见 `docs/TRACEABILITY.csv`）
