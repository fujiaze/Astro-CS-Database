# lib/algorithms/integration/v6_phase1 — Phase1 单帧产品装配（P1-INTEGRATE-001 / Wave 7）

> 状态：IMPLEMENTED（本任务只做**接线**，不新增科学公式、不改已归属底层模块）。
> 写域：`lib/phase1/`。根/公共 CMake 注册归 RUNTIME-CI-001/W9（控制包 C-004.4）。

## 职责

把 Wave 5 交付的 Phase1 科学实现装配为**一个可落盘、可重开、可被 Phase2 消费**
的单帧产品：

1. 校准 covariance（`astrocs::calibration::v6`，IMPL-P1-CAL-001）
2. PSF 归一 / `A_NEA` / effective PSF（`astrocs::v6::p1psfw` psf_information，IMPL-P1-PSFW-001）
3. `W_info` / `Q` / `F_hat`（information_weight，IMPL-P1-PSFW-001）
4. PSFSW 四分量 + 未归一复合（psfsw，IMPL-P1-PSFW-001）
5. 球面 Drizzle 面亮度/方差/相关（`astrocs::v6::drizzle`，IMPL-P1-DRZ-001）
6. FITS 写出 / provenance / BUNIT / 原子发布 / HiPS manifest（`astrocs::aio`，IMPL-AIO-001）

## 磁盘产物（原子发布，无可见半成品）

```
<target_dir>/
  science.fits          # PRIMARY=signal_sb(ADU/px^2) + SUPPORT(px^2)
                        # + VARIANCE(ADU^2/px^4) + IVAR(px^4/ADU^2)
  phase1_product.json   # 复合记录（见下）
```

`phase1_product.json` 内嵌记录逐条对齐生产 schema：

| 键 | 生产 schema |
|---|---|
| `point_information` | `astrocs.v6.point-information.v1.schema.json` |
| `psfsw` | `astrocs.v6.psfsw.v1.schema.json` |
| `calibration_covariance` / `drizzle_covariance` | `astrocs.v6.covariance.v1.schema.json` |
| `effective_psf` | `astrocs.v6.effective-psf.v1.schema.json` |
| `provenance` | `astrocs.v6.provenance.v1.schema.json` |

原子发布序列 = `tmp → fsync → CHECKSUM → rename → 重开验证`（AIO
`atomic_publish_directory`）；失败/取消时 staging 整树删除，目标不出现。

## 冻结口径（逐条）

- 单位：`signal_sb=ADU/px^2`、`pixel_variance_in=ADU^2`、`sb_variance_out=ADU^2/px^4`、
  `sb_ivar_out=px^4/ADU^2`、`W_info=ADU^-2`、`Q=ADU^-1`、`flux=ADU`、`psfsw=1`。
- BUNIT 量纲可判 + **二次律** `variance = signal^2`、`ivar = 1/variance`。
- 权重词表单源（W6 canonical）：`weight.kind="psfsw_robust_weight"`、`units="1"`、
  `group_normalized=true`、`normalization.scope="group"`、`median_target=1.0`、
  `constants_version="PSFSW-COMPOSITE-V1"`。**禁**写成 `ivar`/`Fisher`/`1/W`。
- **OI-01（OPEN）**：Phase1 单帧无法形成帧组 → 只出四分量 + **未归一 Wt** +
  归一契约；`weight.weight_value=null`，组内 `median=1` 归一归 Phase2。
  `phase1_extensions.group_normalization_deferred_to="phase2"` 显式登记。
- **P33 撤销（C-004.2）**：帧级 `median(SNR_F)` 系数与任何诊断权重形态不得重新
  引入；重开门对 `snr_frame_coefficient`/`snr_coefficient`/`support_x_snr*`/
  `psf_snr_power` 等**键**做递归 fail-closed 扫描。
- 共享 master 系统项按 `FZ-PROV-SHARED-SYSTEMATIC` 三通道之一进 covariance 链；
  只出对角时另存相关核/可重建算子摘要（`diagonal_approximation.is_lower_bound=true`、
  `use_for_aperture=false`）。

## API

- `write_phase1_product(inputs, target_dir) -> Phase1WriteResult`
- `open_phase1_product(target_dir) -> Phase1OpenResult`（磁盘重开独立验证）
- `consume_phase1_group_for_psfsw(dirs) -> Phase1GroupConsumption`（Phase2 消费面：
  仅由磁盘重开产品组成帧组并产组内归一权重）

## 构建 / 测试

自包含（不需根 CMake 注册）：

```bash
cmake -S tests/integration/v6_p1 -B build/v6_p1_int -DCMAKE_BUILD_TYPE=Release
cmake --build build/v6_p1_int -j 8
ctest --test-dir build/v6_p1_int --output-on-failure
```
