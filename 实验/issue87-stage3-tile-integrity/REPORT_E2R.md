# Issue #87 / E2-R：FP_NONE 覆盖身份与 FITS checksum 一致性 —— 施工与取证报告

本报告由 `make_report_e2r.py` 从 `evidence/**/*.json`（真实 CLI / 生产 C API 实测）生成，不手抄。

## 1. 身份与工具链

| 项 | 值 |
|---|---|
| 主任务 | Issue #87（E2-R 小范围收尾，P0-C1） |
| 开工基线 `main` | `9682b598050a24dbc4ab90d67601984d93829e6f`（已验收 E2 修复 `45bbd87a4` + 订正 `9682b5980`） |
| 修复后 acsd SHA-256 | `7638f0df56816d11af65d475229ad363ce0f4ea838d47a39776646e1c2acc1c0` |
| 修复前（E2 基线）acsd SHA-256 | `66614b2995bfcf8332c5aae22b8e17887159f8f69a4bc9e2dd6c3d44537cf1a6` |
| 隔离检出 | `/workspace/.e2r-clean`（HEAD 9682b5980 的 `--shared` 克隆；因并发 #9 Agent 同树写入，E2-R 改动在隔离检出中重放/构建/取证） |

> 二进制字节 SHA 会随 build stamp（`build_dirty`/`build_source_digest`）变化；运行溯源以产物内 `run_context.json` 的 `build_source_digest` 为准。

## 2. 合同改动（最小收尾，保留已验收 E2 修复）

1. **FP_NONE = legacy unverified coverage（正式命名 `UNVERIFIED_LEGACY`）**：无 MOC 且无覆盖计数时，缺失记新码 `UNKNOWN_LEGACY_COVERAGE`（旧行为记 `OUTSIDE_DECLARED_FOOTPRINT`，把「证据缺席」当「证据为空」）。`aio_hips_coverage_verified()` 仅对完整叶级 MOC（`MOC_LEAF`）返回 1。
2. **兼容政策**：`UNKNOWN_LEGACY_COVERAGE` 非 fail-closed（合法稀疏 HiPS 仍可 best-effort 出图，NaN/coverage=0），但 `coverage_verified=false` / `input_completeness=UNVERIFIED_LEGACY` 必须随 `p3_resampled.json`、writer/verify 节点产物、phase3 session 结果 JSON 与 FITS 头 `COVSTAT`/`COVVERIF` 发布（run manifest 只汇总节点状态/错误）；已声明覆盖下的真实缺损保持 fail-closed。
3. **校验一致性**：`fits_is_compressed_image` 判为 tile 压缩时快速路径（已读像元位样）不再使用，回退 cfitsio 全 HDU 校验（磁盘原始字节口径）；标准 tile-compressed 布局（空 PRIMARY + HDU2 ZIMAGE）由 reader 显式定位后读取。

## 3. 不可辨识世界对（同一无 MOC 产品）

世界 A = 合法稀疏（tile4 从未存在）；世界 B = 曾有 tile4 但被删除。两者输入文件集合与逐文件哈希**逐字节相同**：

- 输入文件集合：A=['manifest.json', 'signal/Norder0/Dir0/Npix7.fits', 'signal/properties']；B=['manifest.json', 'signal/Norder0/Dir0/Npix7.fits', 'signal/properties']；`identical=True`
- 输入清单哈希：`07d8c21c06947a3d78f546166b89f5afe93e54c647e5e25cc37ed925157c0532`

| 用例（世界） | exit | FITS | input_completeness | COVSTAT | unverified_absent_reads | outside_reads |
|---|---|---|---|---|---|---|
| unhinted_legacy_sparse | 0 | Y | UNVERIFIED_LEGACY | UNVERIFIED_LEGACY | 1 | 0 |
| unhinted_deleted | 0 | Y | UNVERIFIED_LEGACY | UNVERIFIED_LEGACY | 1 | 0 |

两者产出相同 NaN 面（`data_sha256` 相同）且身份都是 `UNVERIFIED_LEGACY` ⇒ 删除 tile 不再被伪装成「已证实无覆盖」；消费方据此不得把该产物当科学完整。

## 4. A/B：修复前 vs 修复后（真实生产 CLI，24 例）

- 修复后：`24` 例，`0` 失败（全绿）。
- 修复前（E2 基线）：故障类 12 例仍 `exit 0`+complete FITS（E2 缺陷复现）；另 12 个成功用例缺 `coverage_verified`/`input_completeness`（E2-R 缺陷面：身份不可机判）。

| 用例 | exit | FITS | manifest | 机器码 | input_completeness | footprint | 坏 tile | 判定 |
|---|---|---|---|---|---|---|---|---|
| checksum_absent | 0 | Y | complete | - | VERIFIED | MOC_LEAF | - | PASS |
| checksum_bad | 3 | n | incomplete | P3-TILE-CHECKSUM-FAILED | - | - | [4] | PASS |
| checksum_ok | 0 | Y | complete | - | VERIFIED | MOC_LEAF | - | PASS |
| declared_missing | 3 | n | incomplete | P3-TILE-DECLARED-MISSING | - | - | [5] | PASS |
| invalid_bitpix | 3 | n | incomplete | P3-TILE-INVALID | - | - | [4] | PASS |
| invalid_dims | 3 | n | incomplete | P3-TILE-INVALID | - | - | [4] | PASS |
| io_error | 7 | n | incomplete | P3-TILE-IO-ERROR | - | - | [4] | PASS |
| legal_full_bilinear | 0 | Y | complete | - | VERIFIED | MOC_LEAF | - | PASS |
| legal_full_nearest | 0 | Y | complete | - | VERIFIED | MOC_LEAF | - | PASS |
| multi_bad | 3 | n | incomplete | P3-TILE-DECLARED-MISSING | - | - | [0, 4, 5, 8] | PASS |
| real_covered | 0 | Y | complete | - | VERIFIED | MOC_LEAF | - | PASS |
| real_partial | 0 | Y | complete | - | VERIFIED | MOC_LEAF | - | PASS |
| same_bad_multi_worker | 3 | n | incomplete | P3-TILE-CHECKSUM-FAILED | - | - | [4] | PASS |
| same_bad_multi_worker r1 | 3 | n | incomplete | P3-TILE-CHECKSUM-FAILED | - | - | [4] | PASS |
| same_bad_multi_worker r2 | 3 | n | incomplete | P3-TILE-CHECKSUM-FAILED | - | - | [4] | PASS |
| sparse_moc_covered | 0 | Y | complete | - | VERIFIED | MOC_LEAF | - | PASS |
| sparse_moc_outside | 0 | Y | complete | - | VERIFIED | MOC_LEAF | - | PASS |
| truncated | 3 | n | incomplete | P3-TILE-INVALID | - | - | [4] | PASS |
| undetermined_coarse_moc | 3 | n | incomplete | P3-TILE-FOOTPRINT-UNDETERMINED | - | - | [5] | PASS |
| undetermined_manifest_only | 3 | n | incomplete | P3-TILE-FOOTPRINT-UNDETERMINED | - | - | [5] | PASS |
| unhinted_covered | 0 | Y | complete | - | UNVERIFIED_LEGACY | UNVERIFIED_LEGACY | - | PASS |
| unhinted_deleted | 0 | Y | complete | - | UNVERIFIED_LEGACY | UNVERIFIED_LEGACY | - | PASS |
| unhinted_legacy_sparse | 0 | Y | complete | - | UNVERIFIED_LEGACY | UNVERIFIED_LEGACY | - | PASS |
| unhinted_outside | 0 | Y | complete | - | UNVERIFIED_LEGACY | UNVERIFIED_LEGACY | - | PASS |

机器校验 `evidence/e2r_verify.json`：**96 条判据，0 失败**。

## 5. FITS checksum 快速路径 vs 独立 CFITSIO 对拍

Oracle = cfitsio `fits_verify_chksum`（`ffgcks`，按磁盘原始 header+data 字节）。`typed_equals_raw` = 快速路径公式（raw header + `fits_read_pix` 解码像元）是否等于 raw-bytes 公式；`raw_equals_cfitsio` = 本 harness 的 raw-bytes 复算是否等于 cfitsio oracle。

| 用例 | reader code | oracle dataok | oracle hduok | reader CHECKSUM_FAILED | typed==raw | raw==cfitsio | 判定 |
|---|---|---|---|---|---|---|---|
| f32_both | 0 | 1 | 1 | False | True | True | PASS |
| f32_datasum_only | 0 | 1 | 0 | False | True | True | PASS |
| f32_checksum_only | 0 | 0 | 1 | False | True | True | PASS |
| f32_none | 0 | 0 | 0 | False | True | True | PASS |
| f32_nan_payloads | 0 | 1 | 1 | False | True | True | PASS |
| f64_both | 0 | 1 | 1 | False | True | True | PASS |
| f64_nan_payloads | 0 | 1 | 1 | False | True | True | PASS |
| i32_blank_both | 0 | 1 | 1 | False | True | True | PASS |
| f32_bscale | 0 | 1 | 1 | False | False | True | PASS |
| f32_bzero | 0 | 1 | 1 | False | False | True | PASS |
| f32_compressed | 0 | 1 | 1 | False | False | False | PASS |
| f32_header_bitflip | 5 | 1 | -1 | True | True | True | PASS |
| f32_data_bitflip | 5 | -1 | -1 | True | True | True | PASS |
| f32_compressed_header_bitflip | 5 | 1 | -1 | True | False | False | PASS |
| f32_compressed_data_bitflip | 5 | -1 | -1 | True | False | False | PASS |

共 15 例，0 失败。结论：未压缩 float32/64、±0/多 NaN 位型、单/双校验字段下快速路径与磁盘原始字节逐位等价；`BSCALE`/`BZERO` 非平凡与 tile 压缩下不等价，已按 `fits_is_compressed_image`/变换关键字回退 cfitsio。

## 6. 正式 session 入口（`p3_session_run`）

| 用例 | ACS 状态 | FITS | coverage_verified | input_completeness | 判定 |
|---|---|---|---|---|---|
| legal_full | 0 | Y | True | VERIFIED | PASS |
| sparse_moc_outside | 0 | Y | True | VERIFIED | PASS |
| declared_missing | 1 | n | False | - | PASS |
| checksum_bad | 1 | n | False | - | PASS |
| io_error | 4 | n | False | - | PASS |
| undetermined_coarse_moc | 1 | n | False | - | PASS |
| same_bad_multi_worker | 1 | n | False | - | PASS |
| unhinted_deleted | 0 | Y | False | UNVERIFIED_LEGACY | PASS |

session 用例 8 个，失败 0 个。

## 7. 自动化测试与回归

| 回归项 | 用例数 | 失败 |
|---|---|---|
| #91 原有 22 例 CLI（e2r_regress） | 22 | 0 |
| #91 10 条 session 入口 | 10 | 0 |
| 立项 30 条单位裁决单测 | 30 | 0 |
| 本轮 tile 语义单测（新增 E2-R 断言） | 123 | 0 |
| #91/E1-R worker-open 注入 | 5 | 0 |
| #87 session 入口 | 8 | 0 |

与已验收 E2 证据逐例对照：20/20 例退出码、FITS signal sha256、resampled bin sha256 **逐位相同** （普通有效输入保持逐位一致）。

## 8. 性能

- reader 层微基准（同源 `checksum_probe.cpp` 链接 E2 基线/修复后 `libacsd_hips`，真实 M42 产品 523 叶级 tile）：677899 ns/tile → 641557 ns/tile。
- **如实说明**：取证期间同机有并发 Agent 重负载（load average > 10），同一微基准重复运行的delta 在 −5% ~ +29% 间摆动（`evidence/checksum_bench.json` 只保存其中一次），因此本轮**不支持**任何精确的百分比结论；只能说 reader 层仍是同量级，新增的每 tile `fits_is_compressed_image` 判定不改变复杂度。
- 端到端 wall/RSS 未重跑全 perf 矩阵（与 E2 报告同量级；合法/真实用例 signal 与 bin 逐位不变）。

## 9. NOT_RUN / 实际边界

1. **真实 EACCES/ENOSPC/EMFILE**：未注入（与 E2 相同边界）。
2. **HTTP/远程 HiPS**：未实测。
3. **#95 低阶 MOC 父级归约**：仍显式判 `FOOTPRINT_UNDETERMINED`，不归约。
4. **同一 output_dir 多 run manifest 选择约定**：未改。
5. **全仓测试/CI**：未运行；本轮只复跑指定套件（全绿），不声明全仓全绿。
6. **压缩 tile 的性能/端到端**：仅 harness 层校验一致性，未做大工作集性能矩阵。

## 10. 复算命令（隔离检出 `/workspace/.e2r-clean`）

```bash
python3 实验/issue87-stage3-tile-integrity/make_fixtures.py
python3 实验/issue87-stage3-tile-integrity/run_matrix.py --acsd build/linux-dev/acsd --tag e2r_fixed --expect fixed
python3 实验/issue87-stage3-tile-integrity/run_matrix.py --acsd /workspace/.e87-base-build/acsd --tag e2r_prefix --expect prefix
python3 实验/issue87-stage3-tile-integrity/verify_matrix.py --prefix-tag e2r_prefix --fixed-tag e2r_fixed --out evidence/e2r_verify.json
python3 实验/issue87-stage3-tile-integrity/checksum_oracle.py
python3 实验/issue87-stage3-tile-integrity/run_session.py --build build/linux-dev
python3 实验/issue87-stage3-tile-integrity/stale_output_test.py --acsd build/linux-dev/acsd --tag e2r_fixed
python3 实验/issue87-stage3-tile-integrity/checksum_bench.py --baseline /workspace/.e87-base-build --fixed build/linux-dev
python3 eng/tests/unit/run_p3_tile_semantics_test.py
python3 eng/tests/unit/run_p3_input_unit_guard_test.py
python3 实验/issue91-stage3-bunit/make_fixtures.py
python3 实验/issue91-stage3-bunit/run_e2e.py --acsd build/linux-dev/acsd --tag e2r_regress
python3 实验/issue91-stage3-bunit/run_session_entry.py --src "$PWD" --build build/linux-dev --tag e2r_regress
python3 实验/issue91-e1r-worker-open/run_e1r.py --acsd build/linux-dev/acsd --tag e2r_regress --expect fixed
```

## 11. 原始证据

- `evidence/e2r_prefix/results.json`、`evidence/e2r_fixed/results.json`（CLI 矩阵 24 例）
- `evidence/e2r_verify.json`（96 条机器判据）
- `evidence/checksum_oracle.json`（15 例 checksum 对拍）
- `evidence/session/results.json`（session 8 例）
- `evidence/unit/p3_tile_semantics_test_e2r.txt`（123/123）、`evidence/unit/p3_input_unit_guard_test_e2r.txt`（30/30）
- `evidence/checksum_bench.json`（reader 微基准）
- `实验/issue91-stage3-bunit/evidence/e2r_regress/`、`实验/issue91-e1r-worker-open/evidence/e2r_regress/`（既有回归）
