# AstroCS V6 科学产品 I/O（IMPL-AIO-001）

> 任务: `IMPL-AIO-001`（Wave 5，depends_on = CONTRACT-FREEZE-001）
> 写域: `lib/infrastructure/aio/v6/`、`tests/unit/v6_aio/`
> 状态: 新科学层基础能力实现；**不接线任何 Phase session**（任务正文要求）。

本目录是 V6 科学层的产品写盘基础：原生 FITS 流式写出 + 标准 DATASUM/CHECKSUM、
原子发布（tmp + fsync + rename + 重开独立验证）、provenance 最小集 fail-closed 门、
BUNIT 量纲可判与二次律、HiPS properties/manifest 渲染与校验。

## 1. 文件

| 文件 | 语义 |
|---|---|
| `include/astro/aio/v6_sha256.h` / `src/v6_sha256.cpp` | FIPS 180-4 SHA-256（output_hash / input_product_hashes 完整性锚） |
| `include/astro/aio/v6_bunit.h` / `src/v6_bunit.cpp` | 冻结单位表、量纲代数、BUNIT 可判性、二次律 |
| `include/astro/aio/v6_fits.h` / `src/v6_fits.cpp` | 原生 FITS 流式写出 + DATASUM/CHECKSUM + 重开验证 |
| `include/astro/aio/v6_atomic_publish.h` / `src/v6_atomic_publish.cpp` | 文件/目录原子发布事务（失败/取消无半成品） |
| `include/astro/aio/v6_provenance.h` / `src/v6_provenance.cpp` | provenance 最小集对象与 fail-closed 门 |
| `include/astro/aio/v6_hips_manifest.h` / `src/v6_hips_manifest.cpp` | HiPS properties / manifest 渲染与校验 |
| `include/astro/aio/v6_product_io.h` / `src/v6_product_io.cpp` | 多 HDU 产品原子发布 + 产品记录级汇总门 |
| `CMakeLists.txt` | 独立静态库 `astrocs_v6_aio`（不修改根/公共 CMakeLists，C-004.4） |

## 2. 冻结节点映射（实现逐条对应）

| 冻结 id | 实现点 | 门 |
|---|---|---|
| `ALG-P3-008` §7.3 | `atomic_publish_file/directory`：同目录 tmp → fsync → rename → 重开验证；失败/取消删除 tmp，rename 不发生 | `G-ATOMIC-PUBLISH`（无 tmp 残留） |
| `ALG-P3-008` §7.1/§7.3 | `FitsStreamWriter`：HDU 布局、2880 对齐、DATASUM/CHECKSUM（FITS 4.0 §4.4.2.5） | `G-FITS-DATASUM` / `G-FITS-CHECKSUM` |
| `ALG-P3-008` §7.3 第 5 步 | `verify_fits_file`：shape / WCS 关键字 / 层完整性 / BUNIT 重开验证 | `G-FITS-LAYER` / `G-FITS-KEYWORD` / `G-FITS-BUNIT` |
| `FZ-PROV-MINIMAL-SET` | `provenance_required_keys()` + `validate_provenance_json` | `G-PROV-MINIMAL-SET` |
| `FZ-BUNIT-SEMANTICS` | `bunit_dimension_decidable`（显式 px 幂次 或 ADU+surface_brightness+-2+面积） | `G-BUNIT-SEMANTICS` / `G-PIXEL-AREA-POWER` |
| `FZ-P3-BUNIT-QUADRATIC` | `quadratic_law_holds`：variance=signal²、ivar=1/variance | `G-BUNIT-QUADRATIC` |
| `FZ-UNIT-SIGNAL-SB / VAR-IN / VAR-SB / IVAR-SB / WINFO / Q / FLUX / PSFSW` | `frozen_unit_string` + `unit_matches_frozen`（量纲幂次等价） | `G-UNIT-TABLE` |
| `FZ-COND-FLUX-CONSERV` | pixfrac<1 必须有正 `flux_conservation_factor` | `G-FLUX-CONSERV-FACTOR` |
| `FZ-PROV-KCORR` | `k_corr` 定义/适用域/值/标定必填；`k_corr=1` 判红 | `G-KCORR-DOMAIN` / `G-KCORR-CALIBRATION` |
| `FZ-PROV-SHARED-SYSTEMATIC` / `FZ-GATE-PARENT-VAR` | `correlation_summary` 表示域；对角-only 不得声明不可用相关 | `G-SHARED-SYSTEMATIC` |
| `FZ-DEGRADE-SCALAR` | 降级记录 p05≤p50≤p95 + 双门 | `G-DEGRADE-SCALAR` |
| `ADJ-GEN-03` | `unavailable.{flag,reason,scope}`，占位原因判红 | `G-UNAVAILABLE-REASON` |

科学纪律：本模块不实现任何科学公式（Drizzle/PSF/GLS 属其他任务）；不把
`median(SNR_F)`/support/coverage/FWHM 当权重；psfsw 单位冻结为无量纲 `1`；
covariance 与 variance 只做单位/键位校验，不重算科学值。

## 3. 构建与测试

```bash
# 独立构建（不依赖根 CMake）
cmake -S tests/unit/v6_aio -B build/v6_aio -DCMAKE_BUILD_TYPE=Release
cmake --build build/v6_aio -j 8
ctest --test-dir build/v6_aio --output-on-failure
```

ctest 用例：
- `v6_aio_units` / `v6_aio_provenance` / `v6_aio_manifest` / `v6_aio_fits` / `v6_aio_atomic`（共址正/负例）
- `v6_aio_artifacts`（fixture：生成 product.fits / provenance.json / manifest.json / properties）
- `v6_aio_oracle`（独立 Oracle：astropy + hashlib + 合同 JSON + 独立 1 补码实现）
- `v6_aio_oracle_negative`（10 项产物级 mutation，独立检查必须判红）
- `v6_aio_impl_mutations`（12 项实现级 mutation：注入违反冻结的实现 → 测试必红）

### 外部真值（Oracle 独立性）
- FITS DATASUM/CHECKSUM：`astropy.io.fits verify_datasum/verify_checksum` 与
  独立转写的 FITS 4.0 §4.4.2.5 1 补码算法双真值交叉；
- provenance 最小集：required 键由 `contracts/proposals/v6/data/astrocs.v6.provenance.v1.schema.json`
  独立反查，单位表由 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` 反查；
- SHA-256：`hashlib`。

## 4. 边界

- 未修改根 `CMakeLists.txt` / `tests/unit/CMakeLists.txt`（C-004.4：根构建面注册由控制器/W9 统一处理）。
- 未接线 Phase session；未修改任何 `docs/`、`ci/`、其他 `lib/` 模块。
- `PENDING_OWNER_SIGNOFF` 条款（FZ-BUNIT-SEMANTICS、FZ-UNIT-VAR-IN/VAR-SB/IVAR-SB、
  FZ-GATE-PARENT-VAR 等）按 fail-closed 实现，不放宽、不自行定值；OPEN 项不取值。
