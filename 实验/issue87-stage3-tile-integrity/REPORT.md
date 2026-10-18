# Issue #87 / E2：Stage3 HiPS Tile 读取完整性与故障传播 —— 施工与取证报告

本报告由 `make_report.py` 从 `evidence/**/*.json`（真实 CLI / 生产 C API 实测）生成，不手抄。

## 1. 身份与工具链

| 项 | 值 |
|---|---|
| 主任务 | Issue #87 |
| 关联 | #91（已完成）、#94、#9、#79、#95（独立任务，不在本轮范围） |
| 开工基线 `main` | `476c023d395b224518aa1831f730d64822250289` |
| 修复前 A/B 源码 | `476c023`（`git clone` + detached checkout，未建分支） |
| 修复前 acsd SHA-256 | `66614b2995bfcf8332c5aae22b8e17887159f8f69a4bc9e2dd6c3d44537cf1a6` |
| 修复后 acsd SHA-256 | `586000f5c62497be88d64242f2bbbf1627da6da312870abfb80780122705a6f7` |
| 输入夹具清单哈希 | `313f2e38da592e3148dbe7da17944ac15f365881964de86807eae55b120f2ad6` |

证据二进制构建于**提交前工作树**（`build_dirty=true`，版本串 `+g476c023…`）；提交后源码同一，重建的 `build_source_digest` 会变化，因此二进制字节 SHA 可能不同。运行溯源以产物内 `run_context.json` 的 `build_source_digest` 为准。

机器校验：`evidence/verify.json` 67 条判据，0 条失败。

## 2. 缺陷（修复前 A/B 实测）与修复后 fail-closed

同一受控 HiPS 矩阵：修复前基线（`476c023`）与修复后二进制仅二进制不同。修复前：声明内缺失/损坏/校验失败/I/O/身份不足全部 `exit 0` 并发布 complete FITS（缺陷本体）；修复后：`exit 3`（输入域）或 `exit 7`（I/O 域），无 FITS/bin/resampled.json，manifest=incomplete。

| 用例 | 修复前 exit/FITS/manifest | 修复前 signal 空洞 | 修复后 exit/FITS/manifest | 修复后机器码 | 修复后坏 tile |
|---|---|---|---|---|---|
| checksum_bad | 0 / Y / complete | nan=0 zero=0 | 3 / n / incomplete | P3-TILE-CHECKSUM-FAILED | [4] |
| declared_missing | 0 / Y / complete | nan=4096 zero=0 | 3 / n / incomplete | P3-TILE-DECLARED-MISSING | [5] |
| invalid_bitpix | 0 / Y / complete | nan=4096 zero=0 | 3 / n / incomplete | P3-TILE-INVALID | [4] |
| invalid_dims | 0 / Y / complete | nan=4096 zero=0 | 3 / n / incomplete | P3-TILE-INVALID | [4] |
| io_error | 0 / Y / complete | nan=4096 zero=0 | 7 / n / incomplete | P3-TILE-IO-ERROR | [4] |
| multi_bad | 0 / Y / complete | nan=3226 zero=0 | 3 / n / incomplete | P3-TILE-DECLARED-MISSING | [0, 4, 5, 8] |
| same_bad_multi_worker | 0 / Y / complete | nan=0 zero=0 | 3 / n / incomplete | P3-TILE-CHECKSUM-FAILED | [4] |
| same_bad_multi_worker | 0 / Y / complete | nan=0 zero=0 | 3 / n / incomplete | P3-TILE-CHECKSUM-FAILED | [4] |
| same_bad_multi_worker | 0 / Y / complete | nan=0 zero=0 | 3 / n / incomplete | P3-TILE-CHECKSUM-FAILED | [4] |
| truncated | 0 / Y / complete | nan=4096 zero=0 | 3 / n / incomplete | P3-TILE-INVALID | [4] |
| undetermined_coarse_moc | 0 / Y / complete | nan=4096 zero=0 | 3 / n / incomplete | P3-TILE-FOOTPRINT-UNDETERMINED | [5] |
| undetermined_manifest_only | 0 / Y / complete | nan=4096 zero=0 | 3 / n / incomplete | P3-TILE-FOOTPRINT-UNDETERMINED | [5] |

## 3. 合法/真实输入逐位回归（修复前 = 修复后）

| 用例 | 修复前 signal sha(16) | 修复后 signal sha(16) | 修复前 bin sha(16) | 修复后 bin sha(16) | 逐位相同 |
|---|---|---|---|---|---|
| checksum_absent | 6d1ca50083d9fad8 | 6d1ca50083d9fad8 | 3adaf1dfdfd978fa | 3adaf1dfdfd978fa | 是 |
| checksum_ok | 6d1ca50083d9fad8 | 6d1ca50083d9fad8 | 3adaf1dfdfd978fa | 3adaf1dfdfd978fa | 是 |
| legal_full_bilinear | a11ee210352d1ff7 | a11ee210352d1ff7 | 4e3469dc86e672d6 | 4e3469dc86e672d6 | 是 |
| legal_full_nearest | f78e175e943113a7 | f78e175e943113a7 | b8c9dd2101e08986 | b8c9dd2101e08986 | 是 |
| real_covered | d515d58870a398f9 | d515d58870a398f9 | d5ee2149e925c574 | d5ee2149e925c574 | 是 |
| real_partial | 8e1d36ab34666908 | 8e1d36ab34666908 | 1d028f4b6bf00e12 | 1d028f4b6bf00e12 | 是 |
| sparse_moc_covered | 01558e2bb86ea6a0 | 01558e2bb86ea6a0 | f4b545ffb9827ac4 | f4b545ffb9827ac4 | 是 |
| sparse_moc_outside | 372ef8aab241d43a | 372ef8aab241d43a | 019ac46f4f136dda | 019ac46f4f136dda | 是 |
| unhinted_covered | 35ba8f45257d18e6 | 35ba8f45257d18e6 | f1eaecd8dd57b99c | f1eaecd8dd57b99c | 是 |
| unhinted_outside | 372ef8aab241d43a | 372ef8aab241d43a | 019ac46f4f136dda | 019ac46f4f136dda | 是 |

全部合法/真实用例 signal 与 bin 逐位一致：**True**。

## 4. 多 worker 故障聚合（确定性）

| 用例 | tile_ipix | 机器码 | worker_min | worker_max | worker_count | read_failures | access_hits |
|---|---|---|---|---|---|---|---|
| multi_bad | 0 | P3-TILE-DECLARED-MISSING | 8 | 15 | 8 | 2 | 1178 |
| multi_bad | 4 | P3-TILE-INVALID | 3 | 15 | 6 | 3 | 870 |
| multi_bad | 5 | P3-TILE-CHECKSUM-FAILED | 0 | 12 | 6 | 6 | 870 |
| multi_bad | 8 | P3-TILE-IO-ERROR | 0 | 7 | 8 | 4 | 1178 |
| same_bad r0 | 4 | P3-TILE-CHECKSUM-FAILED | 0 | 15 | 16 | 16 | 65536 |
| same_bad r1 | 4 | P3-TILE-CHECKSUM-FAILED | 0 | 15 | 16 | 16 | 65536 |
| same_bad r2 | 4 | P3-TILE-CHECKSUM-FAILED | 0 | 15 | 16 | 16 | 65536 |

聚合按 `tile_ipix` 升序（`std::map`）；同一 tile 多 worker 访问合并为一条，`worker_min/max/count` 为确定性统计量。`read_failures` 为真实失败读取次数（并发首次 miss 竞争可 >1，观测值）；`access_hits` 为采样路径遇到该错误的次数。

## 5. FITS 校验规则（正例 / bitflip 反例 / 缺字段）

| 用例 | exit | checksum_present_reads | checksum_absent_reads | 机器码 |
|---|---|---|---|---|
| checksum_ok | 0 | 1 | 0 | - |
| checksum_absent | 0 | 0 | 1 | - |
| checksum_bad | 3 | 0 | 0 | P3-TILE-CHECKSUM-FAILED |

DATASUM/CHECKSUM 存在时以已读字节算 1's-complement 校验和（不额外重读）；缺校验字段**不是失败**，但显式计入 `checksum_absent_reads`；BSCALE/BZERO/BLANK/压缩等改变原始字节语义时回退 cfitsio 全 HDU 校验。

## 6. 正式 session 入口（`p3_session_run`）

| 用例 | ACS 状态 | 发布 FITS | 期望机器码 | 判定 |
|---|---|---|---|---|
| legal_full | ACS_OK | True | - | PASS |
| sparse_moc_outside | ACS_OK | True | - | PASS |
| declared_missing | ACS_ERR_PARAM | False | P3-TILE-DECLARED-MISSING | PASS |
| checksum_bad | ACS_ERR_PARAM | False | P3-TILE-CHECKSUM-FAILED | PASS |
| io_error | ACS_ERR_IO | False | P3-TILE-IO-ERROR | PASS |
| undetermined_coarse_moc | ACS_ERR_PARAM | False | P3-TILE-FOOTPRINT-UNDETERMINED | PASS |
| same_bad_multi_worker | ACS_ERR_PARAM | False | P3-TILE-CHECKSUM-FAILED | PASS |

session 用例 7 个，失败 0 个。合法无覆盖仍 `ACS_OK` + FITS；完整性错误 `ACS_ERR_PARAM`；I/O 故障 `ACS_ERR_IO`；`last_error` 携带 `P3-TILE-*` 与 tile ipix。

## 7. 自动化测试

- `eng/tests/unit/p3_tile_semantics_test.cpp` + `run_p3_tile_semantics_test.py`：**115/115 checks passed**（真实生产库；分类/缓存/聚合/身份）。
- `实验/issue87-stage3-tile-integrity/run_matrix.py`：受控矩阵 22 例（修复后 0 fail）。
- `实验/issue87-stage3-tile-integrity/run_session.py`：session 入口 7 例（0 fail）。
- `实验/issue87-stage3-tile-integrity/stale_output_test.py`：同 output_dir 陈旧产物失效（修复后 PASS）。
- 提交前工作树**从零构建**（`/workspace/.e87-verify-build`）成功；该 clean 二进制跑同一受控矩阵 22 例 0 红（`evidence/clean/results.json`）。

## 8. 既有回归

| 回归项 | 用例数 | 失败 | 结论 |
|---|---|---|---|
| #91 22 例 CLI（e87_regress） | 22 | 0 | 0/22 红 |
| #91/E1-R 故障注入（e87_regress） | 5 | 0 | 0/5 红 |
| 立项 30 条单位裁决单测 | 30 | 0 | 30/30 passed |

22 例退出码 / 机器码 / FITS data sha256 / finite·NaN 计数与 #91 已提交证据逐例一致（见 `verify_matrix.py` 的 bitwise 段落与本报告 §3 的同源方法）。

## 9. 性能与共享 Tile Cache 量化对照

基线 acsd `66614b2995bfcf83…`，修复后 acsd `586000f5c62497be…`；同一真实 M42 writer 产品（`/workspace/Astro CS Database/实验/issue87-stage3-tile-integrity/work/real/hips`，只读单位归一化视图）。交错执行 prefix/fixed（同一 `sub_block_px` 背靠背）以减小页缓存/系统漂移。

| 套件 | 二进制 | sub_block_px | nw(计划) | wall min(s) | wall 中位(s) | wall max(s) | RSS(MB) | checksum reads | misses | signal sha(16) |
|---|---|---|---|---|---|---|---|---|---|---|
| cache_resident | fixed | 96 | 16 | 0.668 | 1.875 | 2.478 | 16.2 | 42 | 42 | 4262787159723b4e |
| cache_resident | fixed | 192 | 4 | 0.669 | 2.852 | 3.590 | 34.5 | 30 | 30 | 4262787159723b4e |
| cache_resident | fixed | 384 | 1 | 0.679 | 3.029 | 4.034 | 12.2 | 24 | 24 | 4262787159723b4e |
| cache_resident | prefix | 96 | 16 | 0.677 | 3.254 | 4.178 | 44.1 | None | None | 4262787159723b4e |
| cache_resident | prefix | 192 | 4 | 0.668 | 2.959 | 3.883 | 12.7 | None | None | 4262787159723b4e |
| cache_resident | prefix | 384 | 1 | 0.668 | 2.944 | 3.314 | 29.9 | None | None | 4262787159723b4e |
| real_stress | fixed | 384 | 16 | 3.234 | 3.253 | 3.264 | 154.3 | 2260 | 7813 | 81895b360b26c69d |
| real_stress | fixed | 768 | 4 | 23.218 | 23.998 | 66.286 | 72.4 | 35822 | 52792 | 81895b360b26c69d |
| real_stress | prefix | 384 | 16 | 3.228 | 3.516 | 9.891 | 146.8 | None | None | 81895b360b26c69d |
| real_stress | prefix | 768 | 4 | 22.805 | 23.730 | 34.043 | 71.8 | None | None | 81895b360b26c69d |

reader 层微基准（同源 `checksum_probe.cpp` 分别链接修复前/后 `libacsd_hips`，遍历该产品 MOC 声明的 523 个叶级 tile）：修复前 372227 ns/tile，修复后 434683 ns/tile，差 +62456 ns/tile (+16.8%)。

结论：所有合法/真实用例修复前后 FITS signal 逐位一致；reader 层每 tile 开销 +63 µs 量级（DATASUM/CHECKSUM 的 1's-complement 求和，以已读字节计算、不二次读盘），对 523 个唯一 tile 约 +33 ms。cache-resident 小工作集端到端 wall/RSS 与修复前同量级；real_stress 大工作集（工作集远大于默认 `max_tiles` 导致缓存抖动）端到端 wall 受 I/O 抖动支配，min 值同量级、RSS 不增。修复前 manifest 的 `tile_cache` 恒为 null/0（旧实现 close 后才取 stats 的缺陷），修复后随产物落盘真实计数。

## 10. 失败 run 的输出目录完整性

同一 output_dir 先成功后失败：失败 run 退出 `3`，成功后 `无残留成功 artifact`，`p3_tile_failures.json`=True，manifest=incomplete，tmp=[]。

## 11. NOT_RUN / 实际边界

1. **真实权限拒绝（EACCES）**：本环境以有权限用户运行，`open` 权限故障以“tile 路径被目录占位”构造受控 I/O 失败（`IO_ERROR`，exit 7）；未构造独立 EACCES 实测。
2. **磁盘满/句柄耗尽**：未注入；属资源域独立故障（既有 disk_full 分类未改）。
3. **HTTP/远程 HiPS、压缩 tile（fpack）**：reader 的本地路径校验和缺失字段规则已实现并回退 cfitsio 全 HDU 校验；未做远程/压缩实测。
4. **父级覆盖归约（#95）**：MOC 仅低阶父级条目判 `FOOTPRINT_UNDETERMINED`，不做归约；`undetermined_coarse_moc` 已实测。
5. **同名多次 run 的多个 run manifest**：CLI 每次运行写一个 `acsd_run_*.json`；陈旧成功产物已由本修复失效，但多 manifest 的选择由消费方按最新 mtime/状态判定，本轮未改该约定。
6. **原始真实观测数据**：仓库内无未处理的原始 M42 FITS；使用 M42 管线的 writer 产品 `run/HIPS-PACK-01/corpus_big` 的只读 symlink 单位归一化视图（原产品只读，未改写 tile/MOC 字节）。

## 12. 复算命令

```bash
python3 实验/issue87-stage3-tile-integrity/make_fixtures.py
python3 实验/issue87-stage3-tile-integrity/run_matrix.py --acsd build/linux-dev/acsd --tag fixed --expect fixed
python3 实验/issue87-stage3-tile-integrity/run_matrix.py --acsd /tmp/acsd-e87-base-build/acsd --tag prefix --expect prefix
python3 实验/issue87-stage3-tile-integrity/verify_matrix.py
python3 实验/issue87-stage3-tile-integrity/run_session.py --build build/linux-dev
python3 实验/issue87-stage3-tile-integrity/stale_output_test.py --acsd build/linux-dev/acsd --tag fixed
python3 实验/issue87-stage3-tile-integrity/perf_matrix.py --baseline /workspace/.e87-base-build/acsd --fixed build/linux-dev/acsd
python3 实验/issue87-stage3-tile-integrity/checksum_bench.py --baseline /workspace/.e87-base-build --fixed build/linux-dev
python3 eng/tests/unit/run_p3_tile_semantics_test.py
# 回归
python3 实验/issue91-stage3-bunit/run_e2e.py --acsd build/linux-dev/acsd --tag e87_regress
python3 实验/issue91-stage3-bunit/run_session_entry.py --src "$PWD" --build build/linux-dev --tag e87_regress
python3 eng/tests/unit/run_p3_input_unit_guard_test.py
python3 实验/issue91-e1r-worker-open/run_e1r.py --acsd build/linux-dev/acsd --tag e87_regress --expect fixed
```

## 13. 固定输入哈希

| 夹具 | properties | manifest | MOC | tile 集合 |
|---|---|---|---|---|
| legal_full_nearest | 501518dae13ef2ac | 43c0318d9a8148f8 | 7b9574a3eed86167 | 12 tiles / a78093dbe7a75349 |
| legal_full_bilinear | 501518dae13ef2ac | 43c0318d9a8148f8 | 7b9574a3eed86167 | 12 tiles / a78093dbe7a75349 |
| sparse_moc_covered | 501518dae13ef2ac | 848f437540ac75f1 | 6f3dbcb8c069dae9 | 3 tiles / ca316a746fc79867 |
| sparse_moc_outside | 501518dae13ef2ac | 848f437540ac75f1 | 6f3dbcb8c069dae9 | 3 tiles / ca316a746fc79867 |
| unhinted_covered | 501518dae13ef2ac | f3a6eca8653f50ab | - | 1 tiles / 8721ee540ecc0e7a |
| unhinted_outside | 501518dae13ef2ac | f3a6eca8653f50ab | - | 1 tiles / 8721ee540ecc0e7a |
| declared_missing | 501518dae13ef2ac | 81374b7b542f1c3b | d35f0ee3acf40133 | 1 tiles / 43970d9ee68112a1 |
| invalid_dims | 501518dae13ef2ac | 03fbbe63129dfe2e | ebebcd62847e0eff | 1 tiles / a788dce978dbdf25 |
| invalid_bitpix | 501518dae13ef2ac | 03fbbe63129dfe2e | ebebcd62847e0eff | 1 tiles / b62123204a70a070 |
| truncated | 501518dae13ef2ac | 03fbbe63129dfe2e | ebebcd62847e0eff | 1 tiles / 5efadc257e823c1e |
| checksum_ok | 501518dae13ef2ac | 03fbbe63129dfe2e | ebebcd62847e0eff | 1 tiles / 1b68775c63454357 |
| checksum_bad | 501518dae13ef2ac | 03fbbe63129dfe2e | ebebcd62847e0eff | 1 tiles / b6cbd0ffa984678a |
| checksum_absent | 501518dae13ef2ac | 03fbbe63129dfe2e | ebebcd62847e0eff | 1 tiles / c8a590a5e23315e4 |
| io_error | 501518dae13ef2ac | 81374b7b542f1c3b | d35f0ee3acf40133 | 1 tiles / 323c562ebd4cf24a |
| undetermined_coarse_moc | 1dd8c7d1759db511 | 341c1702bb72d7c4 | 7b9574a3eed86167 | 1 tiles / 736832266d07ad45 |
| undetermined_manifest_only | 501518dae13ef2ac | 43c0318d9a8148f8 | - | 1 tiles / 1d5f2c95aac7c51a |
| multi_bad | 501518dae13ef2ac | 43c0318d9a8148f8 | 7b9574a3eed86167 | 2 tiles / 250c69aa049ecb6d |
| same_bad_multi_worker | 501518dae13ef2ac | 03fbbe63129dfe2e | ebebcd62847e0eff | 1 tiles / 0487effe7e126e40 |
| real_covered | a78b9928e12dbe3d | 35fae66b4790aff0 | f48f06c37f337878 | 0 tiles / e3b0c44298fc1c14 |
| real_partial | a78b9928e12dbe3d | 35fae66b4790aff0 | f48f06c37f337878 | 0 tiles / e3b0c44298fc1c14 |

## 14. 原始证据

- `evidence/prefix/results.json`、`evidence/fixed/results.json`（CLI 矩阵）
- `evidence/verify.json`（67 条机器判据）
- `evidence/session/results.json`（session 入口）
- `evidence/stale/results_fixed.json`（陈旧产物）
- `evidence/perf/results.json`（性能）
- 逐例原始日志 `evidence/<tag>/*.log`；受控夹具 `work/fixtures/`
