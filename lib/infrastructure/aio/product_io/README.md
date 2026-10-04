# Astro Celestial Sphere Database（ACSD） V6 科学产品 I/O（IMPL-AIO-001）

> 任务: `IMPL-AIO-001`（Wave 5，depends_on = CONTRACT-FREEZE-001）
> 写域: `lib/infrastructure/aio/product_io/`、`eng/tests/unit/aio/`
> 状态: 新科学层基础能力实现；**不接线任何 Phase session**（任务正文要求）。

本目录是 V6 科学层的产品写盘基础：原生 FITS 流式写出 + 标准 DATASUM/CHECKSUM、
原子发布（tmp + fsync + rename + 重开独立验证）、provenance 最小集 fail-closed 门、
BUNIT 量纲可判与二次律、HiPS properties/manifest 渲染与校验。

## 1. 文件

| 文件 | 语义 |
|---|---|
| `include/astro/aio/sha256.h` / `src/sha256.cpp` | FIPS 180-4 SHA-256（output_hash / input_product_hashes 完整性锚） |
| `include/astro/aio/bunit.h` / `src/bunit.cpp` | 冻结单位表、量纲代数、BUNIT 可判性、二次律 |
| `include/astro/aio/fits.h` / `src/fits.cpp` | 原生 FITS 流式写出 + DATASUM/CHECKSUM + 重开验证 |
| `include/astro/aio/atomic_publish.h` / `src/atomic_publish.cpp` | 文件/目录原子发布事务（失败/取消无半成品） |
| `include/astro/aio/provenance.h` / `src/provenance.cpp` | provenance 最小集对象与 fail-closed 门 |
| `include/astro/aio/hips_manifest.h` / `src/hips_manifest.cpp` | HiPS properties / manifest 渲染与校验 |
| `include/astro/aio/product_io.h` / `src/product_io.cpp` | 多 HDU 产品原子发布 + 产品记录级汇总门 |
| `CMakeLists.txt` | 独立静态库 `acsd_product_io`（根构建面注册由控制器统一处理，C-004.4） |

路径相对本目录 `lib/infrastructure/aio/product_io/`。

## 2. 冻结节点映射（实现逐条对应）

| 冻结 id | 实现点 | 门 |
|---|---|---|
| `ALG-P3-008` §7.3 | `atomic_publish_file/directory`：同目录 tmp → fsync → rename → 重开验证；失败/取消删除 tmp，rename 不发生 | `G-ATOMIC-PUBLISH`（无 tmp 残留） |
| `ALG-P3-008` §7.1/§7.3 | `FitsStreamWriter`：HDU 布局、2880 对齐、DATASUM/CHECKSUM（FITS 4.0 §4.4.2.5） | `G-FITS-DATASUM` / `G-FITS-CHECKSUM` |
| `ALG-P3-008` §7.3 第 5 步 | `verify_fits_file`：shape / WCS 关键字 / 层完整性 / BUNIT 重开验证 | `G-FITS-LAYER` / `G-FITS-KEYWORD` / `G-FITS-BUNIT` |
| `FZ-PROV-MINIMAL-SET` | `provenance_required_keys()` + `validate_provenance_json` | `G-PROV-MINIMAL-SET` |
| `FZ-BUNIT-SEMANTICS` | `bunit_dimension_decidable`（显式立体角幂次 或 ADU+surface_brightness+-2+面积） | `G-BUNIT-SEMANTICS` / `G-PIXEL-AREA-POWER` |
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

本库由根 `CMakeLists.txt` 注册（`add_subdirectory(lib/infrastructure/aio/product_io)`），
随根构建产出 `acsd_product_io`。本目录的 `CMakeLists.txt` 可被独立 configure，但仓内
**当前没有任何测试 harness 接入**：下文这组 `aio_*` 用例与变异 harness（原
`eng/tests/unit/aio/`）目录均已不存在，故不给出无法复现的命令。这些用例曾是：

- `aio_units` / `aio_provenance` / `aio_manifest` / `aio_fits` / `aio_atomic`（共址正/负例）
- `aio_artifacts`（fixture：生成 product.fits / provenance.json / manifest.json / properties）
- `aio_oracle`（独立 Oracle：astropy + hashlib + 合同 JSON + 独立 1 补码实现）
- `aio_oracle_negative`（10 项产物级 mutation，独立检查必须判红）
- `aio_impl_mutations`（12 项实现级 mutation：注入违反冻结的实现 → 测试必红）

⚠ 由此产生的真实缺口：本模块的门目前**没有仓内机检覆盖**，改动门逻辑后没有用例能证明
它转红或转绿。重新接入前，本模块的门改动须由前台补负例验证。

### 外部真值（Oracle 独立性）
- FITS DATASUM/CHECKSUM：`astropy.io.fits verify_datasum/verify_checksum` 与
  独立转写的 FITS 4.0 §4.4.2.5 1 补码算法双真值交叉；
- provenance 最小集：required 键由 `eng/contracts/schemas/product_family_field_constraints.schema.json#/$defs/provenance`
  独立反查，单位表由 `eng/contracts/data/clause_registry.json` 反查；
- SHA-256：`hashlib`。

## 4. 边界

- 写域限于本目录（`lib/infrastructure/aio/product_io/`）；根 `CMakeLists.txt` 的 target
  注册由控制器统一处理（C-004.4），本目录的 `CMakeLists.txt` 可被独立 configure。
- 未接线 Phase session；不依赖 `eng/tests/unit/`（该目录已不存在）。
- `PENDING_OWNER_SIGNOFF` 条款（FZ-BUNIT-SEMANTICS、FZ-UNIT-VAR-IN/VAR-SB/IVAR-SB、
  FZ-GATE-PARENT-VAR 等）按 fail-closed 实现，不放宽、不自行定值；OPEN 项不取值。
