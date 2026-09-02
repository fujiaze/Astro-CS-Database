# G06 Sanitizer 报告 — Phase1 Final Signoff V4

- 日期: 2026-08-09
- 分支: main (commit 见 git log)
- 工具链: WSL Ubuntu (gcc 15.2.0), `-fsanitize=address,undefined`, LSan via `detect_leaks=1`
- 脚本: `lib/astro_image_io/tests/sanitize_wsl_v4.sh`
- 全部步骤以 `timeout` 强制限时，超时即失败。

## 阶段与结果

| # | 覆盖 | 结果 |
| - | --- | --- |
| 1 | 共享 HEALPix core (ang2pix/pix2ang) vs 控制包 1,000,000 全天随机 Oracle | PASS: lines=1000110 mismatch=0 bad_parse=0 bad_roundtrip=0，12 base face 均有覆盖 (83972/83146/83013/83673/83355/83215/83690/83223/83315/83205/83245/83058) |
| 2 | HiPS writer/reader FP32 可移植核心 (CFITSIO) | PASS: HIPS_SANITIZE_OK tiles=3 |
| 3 | HiPS robustness: FP32 + FP64 双精度写读、VOTable metadata.xml 根元素、corrupt TSV 行/ properties / Moc.fits 输入 | PASS: 两 dtype 均 `HIPS_ROBUST_SANITIZE_OK`；损坏 TSV 注入 8 行后 catalog 仍读回 2 个有效行 (id 1001/1002)；损坏 properties 后优雅返回 NULL；损坏 Moc 后优雅降级 |
| 4 | DR3SP parser (gaia_client) 对 GaiaDR3SP 目录实际解析 | PASS: files=20 sources=219165266 GAIA_SANITIZE_OK matched=1 flux_probe=4.52e-17 |
| 5 | Catalogue spatial fuzz: order 7 (NSIDE=128) 100k 随机 + 10k 对抗点 (极区/RA 跨界/face seam jitter)，astropy-healpix 外部 Oracle | PASS: lines=109976 mismatch=0 bad_parse=0 bad_roundtrip=0，12 face 全覆盖 |

总体: **ALL_SANITIZE_V4_PASS**

## G6 过程中发现并修复的问题（均属测试驱动，非生产代码）

1. `hips_robust_sanitize_driver.cpp` 损坏 TSV 注入函数在 `std::getline` 循环结束后复用 `line` 变量：
   C++ 的 `getline` 在最后一次失败读取时会先 `erase()` 目标串，导致循环后 `line` 为空，
   原始数据行被误写为空行。修复：改用 `lines[i]`，并加损坏前 catalog 读回断言。
2. 同一驱动 FP64 分支把 `std::vector<float>` 传给按 `double*` 解释的 `flux_sum/covered_area`，
   ASan 立即报 heap-buffer-overflow（aio_hips_writer.cpp:398）。修复：按 dtype 分配
   `float`/`double` 两种 buffer。生产链 `astro_sphere_sink.cpp` 使用 `std::vector<Scalar>`
   与 dtype 一致，无此问题；本次修复后 FP64 HiPS 写读路径首次在 ASan 下完整验证。

## 超时清单

| 阶段 | timeout |
| - | - |
| 1M oracle 编译+运行 | 1200s |
| HiPS writer/reader 编译 | 900s |
| HiPS 运行 | 600s |
| robustness 编译 | 900s |
| robustness 运行 (每 dtype) | 600s |
| DR3SP 编译 | 900s |
| DR3SP 运行 | 600s |
| fuzz 运行 | 600s |

## 证据文件

- sanitize_wsl_v4_full.log — 完整 stdout/stderr
- healpix_oracle_1m.out / healpix_spatial_fuzz_order7.out — 阶段 1/5 结果
- hips_sanitize_f32.out / hips_robust_f32.out / hips_robust_f64.out — 阶段 2/3 结果
- gaia_dr3sp_sanitize.out — 阶段 4 结果
- spatial_fuzz_order7_oracle.jsonl — 阶段 5 Oracle 向量 (可复现, 脚本 gen_spatial_fuzz.py)
- cfitsio_cc.log — CFITSIO 编译告警记录

## 结论

G6 全部子门 PASS：
- ASan/UBSan/LSan 覆盖共享 HEALPix core + HiPS writer/reader + snr v3（含 FP32/FP64）✅
- Catalogue spatial fuzz mismatch=0 ✅
- corrupt TSV/metadata 优雅处理 ✅
- 全部进程 timeout 强制 ✅
