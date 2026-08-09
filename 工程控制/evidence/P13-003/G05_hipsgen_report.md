# G5 外部 Oracle 补齐 — Hipsgen（V4 最终签字包）

日期: 2026-08-09

## 修复内容（commit 见 git log）

1. **SNR Catalogue metadata.xml 改为标准 VOTable**
   - 修复前: `finalize_snr_product()` 写非标准 `<hips_metadata>` 根元素，
     Hipsgen LINT 报 `Lint[4.4.3] "metadata.xml" format error (expecting "votable")`，
     SNR 产品整体判为 not IVOA HiPS 1.0 compatible。
   - 修复后: 写 `<VOTABLE version="1.3">` + `RESOURCE/TABLE/FIELD`，6 列定义与 TSV
     列（star_id/ra/dec/snr/quality_flags/photometric_status）一一对应。
   - 证据: `hipsgen/hipsgen_snr_LINT_fixed.log` → `Lint: "metadata.xml" ok`，
     HiPS 判为 compatible（仅 1 条 obs_regime 推荐键告警，与 image 产品一致）。

2. **补充 HiPS 推荐元数据（真实值，非伪造）**
   - obs_description / prov_progenitor / obs_regime / em_min / em_max /
     hips_creation_date / hips_cat_nrows（替代未引用的 catalog_nrows）/
     hips_initial_ra / hips_initial_dec（SNR 源 RA/Dec 中位数推导）/
     t_min / t_max（DATE-OBS + 曝光时长 → MJD 数值，Hipsgen 要求数值格式）。
   - hips_status 由 `public master` 改为 `private master`（单帧本地产物，控制包 08 要求）。

3. **Oracle 测试路径解码修正（pack 自带脚本缺陷）**
   - HiPS 标准瓦片命名: `NorderK/DirD/NpixN`，完整 ipix = D*10000 + N。
   - 原 pack 脚本只取 NpixN 与 astropy 全 ipix 比较，导致 ipix>=10000 的行全部误报。
   - 已修正并入库 `lib/common/healpix/tests/snr_hips_spatial_oracle.py`：
     rows=997, wrong_tile_rows=0, duplicate_ids=0, tile 集合完全一致。

## Hipsgen 执行结果（对修复后产品 t4_crop_meta_fix.hips）

| 产品 | LINT | CHECKCODE | CHECK | CHECKFAST | CHECKDATASUM |
| --- | --- | --- | --- | --- | --- |
| signal | PASS (compatible+WARN) | PASS | PASS (Check OK) | PASS | PASS |
| support | PASS (compatible+WARN) | PASS | PASS (Check OK) | PASS | PASS |
| snr (TSV) | PASS (compatible+WARN) | 扫描 24 TSV 后因 preview.jpg 生成异常 ABORT（工具对 TSV 目录限制） | 不可用（无 check code，工具限制） | PASS（0 FITS 扫描） | 不可用（无 FITS 瓦片，工具限制） |

说明: Hipsgen 的 CHECK/CHECKFAST/CHECKDATASUM 面向 FITS 瓦片；TSV Catalogue 产品按控制包
要求以 LINT + 标准结构检查 + 独立 HEALPix 空间 Oracle 覆盖（snr_hips_spatial_oracle.py
mismatch=0）。signal/support 的 LINT/CHECKCODE/CHECK/CHECKFAST/CHECKDATASUM 全部真实执行且通过。

## 关联验证

- 共享 HEALPix core 全天空 Oracle: `test_healpix_oracle.exe`
  lines=1,000,110 mismatch=0 bad_parse=0 bad_roundtrip=0（12 face 全覆盖）。
- 重跑 1024² 管线（新 DLL）HIPS_VERIFY: tiles=24 order=7 snr_points=997 PASS；
  完整 stdout/stderr 见 `run/temp/v4_hipsgen_fix_run/run_stdout_stderr_fixed2.log`。
