# P3-INTEGRATE-001 接线证据（Phase3 V6 三模式导出）

- 任务: `P3-INTEGRATE-001`（Wave 7），write_scope = `lib/phase3_session/`、`eng/tests/integration/v6_p3/`
- 基线 HEAD: `95703e639eb0056d2702e407e01f453fd353010d`（`git rev-parse HEAD` 复核）
- 接线对象（Wave 5，只调用不修改）:
  - `lib/phase3_proj/p3_proj_v6.h`（registry v2 四投影、逐像素 Ω、R/S 行列归一语义）
  - `lib/phase3_rsmp/p3_rsmp.h`（三模式传播、`C_y=R C_x R^T`、输出帧 Q/W、kernel registry、fail-closed 门）
  - `lib/infrastructure/aio/v6/include/astro/aio/*.h`（流式 FITS、原子发布、provenance、单位门）

## 1. 交付文件（全部在 write_scope 内）

| 文件 | 作用 |
|---|---|
| `lib/phase3_session/p3_v6_export.h` | 三模式导出接线 API（OutputGrid / ExportInputs / ExportResult / build_output_grid / export_product / verify_product_on_disk） |
| `lib/phase3_session/p3_v6_export.cpp` | 实现：plan+solid_angle_grid → R/S 算子 → 三模式传播 → 组层 → 流式 FITS+原子发布 → 重开验证 |
| `eng/tests/integration/v6_p3/CMakeLists.txt` | 自注册测试目标（可独立 configure，也可被根 `add_subdirectory`） |
| `eng/tests/integration/v6_p3/p3_v6_export_e2e_test.cpp` | 端点测试：三模式正例 + 16 条负向注入（独立稠密/手算 Oracle） |
| `eng/tests/integration/v6_p3/p3_v6_export_oracle.py` | 独立磁盘重开 Oracle：纯 stdlib FITS 解析 + CHECKSUM 重算 + 生产 provenance schema 门 + 可选 astropy 交叉复核 |

## 2. 接线要点与冻结节点

| 冻结节点 | 接线落点 |
|---|---|
| FZ-P3-MODES | `parse_export_mode` 仅接受 surface_brightness/point_source_flux/visualization；legacy auto/0/psf_snr_power 拒 |
| ALG-P3-001 §1.3 / FZ-P3-OMEGA-NONCONST | `build_output_grid` 调 `solid_angle_grid` 得逐像素 Ω'，替换 `p3rsmp` 邻域的常数占位 |
| FZ-P3-KERNEL-REGISTRY | `KernelRegistry::frozen().admit`；SB→ContinuousField+行归一，PSF→列归一，VIS→nearest/ExplicitUserSelection |
| FZ-FORMULA-COV-PROP | `propagate_surface_brightness` 完整 `C_y=R C_x R^T`；断言 `diag(C_y)=Σ_j R_ij² u_j`（即 var_out=Σc²u） |
| FZ-P3-QW-RECOMPUTE | `propagate_point_source_flux`：π=S p、f=S d、Q=aπ^T C_y^-1 f、W=a²π^T C_y^-1 π；`frame_is_output_recompute=true`，禁止 input_qw_resampled / W=ΣW_in / 重算上游 W_info |
| FZ-P3-FAILCLOSED | 三模式 12+ 门经 `check_failclosed_all`；visualization measurement_capable/writes_variance/ivar/point_information 任一为真即 REJECT |
| FZ-P3-BUNIT-QUADRATIC / FZ-UNIT-* | 主面 ADU/sr、VARIANCE ADU^2/sr^2、flux ADU/ADU^2、effective PSF 1；`validate_bunit_law` + 重开重建二次律 |
| FZ-PROV-MINIMAL-SET | `provenance_to_json` + `validate_provenance_json(required)`；输出哈希取磁盘 FITS 实际 sha256 |
| ALG-P3-008 §7 | `atomic_publish_file` + `FitsStreamWriter`（kRowsPerBand=2 行带/块）+ `verify_fits_file` 重开；失败不留可见半成品 |
| FZ-GATE-MEDIAN-SNR / SUPPORT-COVERAGE | `variance_from="propagated_covariance"`、weight_sources 空；不接 median SNR/support/coverage/FWHM |

## 3. 构建 / 测试命令与实测

```bash
# 独立配置 + 构建（Linux amd64, g++ 14.2, cmake 3.31）
timeout 300 cmake -S eng/tests/integration/v6_p3 -B run/v6/p3-intg/build -DCMAKE_BUILD_TYPE=Release   # rc=0
timeout 900 cmake --build run/v6/p3-intg/build -j 8                                              # rc=0
timeout 900 ctest --test-dir run/v6/p3-intg/build --output-on-failure                             # rc=0
# 1/3 v6_p3_export_positive  Passed
# 2/3 v6_p3_export_negative  Passed
# 3/3 v6_p3_export_oracle    Passed   (ORACLE_PASS 183 checks across 3 modes)
# 100% tests passed, 0 tests failed out of 3

# 负向用例计数
./run/v6/p3-intg/build/v6_p3_export_test negative run/v6/p3-intg/negcheck   # rc=0; cases=16 checks=64 fails=0

# 根构建面注册模拟（父 CMake add_subdirectory）
timeout 300 cmake -S run/v6/p3-intg/parent -B run/v6/p3-intg/parent_build -DCMAKE_BUILD_TYPE=Release  # rc=0
timeout 600 cmake --build run/v6/p3-intg/parent_build -j 8                                            # rc=0
timeout 300 ctest --test-dir run/v6/p3-intg/parent_build                                             # rc=0 (3/3)

# 独立第三方 reader 复核（astropy 7.0.1）
#   3 产品 CHECKSUM/DATASUM 通过，无 "not FITS standard" 告警
```

## 4. 负向测试（16 条，全部红）

vis_measurement_capable / psf_missing_epsf / psf_fwhm_only / psf_qw_resampled /
psf_missing_a / psf_psf_not_normalized / sb_flux_conversion_no_omega /
sb_diagonal_no_kernel / sb_variance_bunit_not_quadratic / prov_missing_kcorr /
prov_missing_flux_factor / weight_source_support /
sb_measurement_unc_unavailable_no_reason / atomic_verify_fail_removes_product /
kernel_nearest_continuous_field / kernel_unregistered_bicubic

每条断言：返回非 Ok、错误门码匹配、`product.fits` 不可见、无 staging/tmp 残留。

## 5. 未决风险（需控制器裁决）

1. **生产 schema 与 bare-ADU 通量的交叉张力**（REVIEW_REQUIRED）：
   `eng/contracts/schemas/product_family_field_constraints.schema.json#/$defs/provenance/allOf` 规定 `units.bunit="ADU"`
   必须配 `pixel_semantics=surface_brightness`/`pixel_area_power=-2`；而
   `product_family_field_constraints.schema.json#/$defs/signal` 规定 `integrated_flux => pixel_area_power=0`。
   二者叠加使纯积分通量主面在 v6 生产 schema 下不可表达；AIO `bunit_dimension_decidable`
   也仅接受 SB-ADU。本接线因此把 matched-filter 通量放扩展 HDU（`FLUX=ADU`/
   `FLUX_VARIANCE=ADU^2`），主面保持 schema 合法的面亮度。建议 owner 对该交叉张力给出裁决
   （FZ-UNIT-FLUX=ADU 与 FZ-BUNIT-SEMANTICS 的适用关系）。
2. **AIO FITS 实数卡格式**（finding，非本任务写域）：
   `FitsCard::make_real` 用 `%.12g` 产出小写 `e`，astropy `verify` 判为
   `not FITS standard`；`phase3proj::v6::fits_keywords` 亦用小写 `%.12e`。
   本层在写盘前把指数规范为大写 `E`（仅本任务产物），未改 AIO；建议 W9/后续任务修正
   `lib/infrastructure/aio/v6` 的实数卡格式化。
3. 生产主面/扩展 HDU 的最终语义命名（`SIGNAL`/`FLUX`/`EFFECTIVE_PSF`）需与
   P1/P2-INTEGRATE-001 的 HDU 命名约定对齐（当前 P1 并行任务同波交付）。
