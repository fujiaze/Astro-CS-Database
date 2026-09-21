# testdata/ — 真实数据与外部只读数据集

本目录是 ENGINEERING_SPEC §7「其他固定目录」之一，存放**只读**的真实测试数据。
任何任务不得改写、移动、删除本目录内已有数据（AGENTS.md §10）。

> **2026-09-21 ROOT-CONSOLIDATION（负责人直接指令）**：依据 ENGINEERING_SPEC §7
> 「外部只读数据集……一旦被代码消费或需入库，**移入 testdata/ 或 artifacts/**」，
> 根目录两个外部只读数据集已迁入本目录：
> `BASS DR3/` → `testdata/BASS_DR3/`（目录名去空格）、`HST_M16/` → `testdata/HST_M16/`。
> 根目录条目相应删除；登记同步见 `ENGINEERING_SPEC.md §7` 与 `ci/root_manifest.json`。

## 1. 入库策略（gitignore 判据）

| 子目录 | 入库？ | 说明 |
|---|---|---|
| `index.json`、`README.md`（本文件） | ✅ 入库 | testdata 的索引与说明锚 |
| `BASS_DR3/` 的元数据面（`index.json`、`*.csv.gz`、`dates/*.json`、`tools/**`、`SHA256SUMS.txt`、`constellation_coverage.csv`、`*.tsv`、`README.md`） | ✅ 入库 | 总 ~16 MB；FITS 由工具另行下载 |
| `BASS_DR3/` 的 FITS（`*.fits` / `*.fits.fz`）、`data/`（ccdinfo 245 MB）、`index.csv`/`coords.csv`（未压缩）、`downloads/`、`logs/` | ❌ 不入库 | 可下载/可再生成的大产物 |
| `HST_M16/*.fits`（3 × 268,917,120 B = 770 MB） | ❌ 不入库 | 体积超限，公开 HLSP 可重新下载 |
| 其余望远镜原始帧（T2/T3/T4 等） | ❌ 不入库 | 历史既有策略（`testdata/*` 默认忽略） |

机器校验（可复跑）：

```bash
# 元数据面必须可入库（TRACKABLE），FITS 必须被忽略（IGNORED）
git check-ignore -q testdata/BASS_DR3/index.json      && echo BAD || echo OK
git check-ignore -q testdata/BASS_DR3/coords.csv.gz   && echo BAD || echo OK
git check-ignore -q testdata/HST_M16/*.fits           && echo OK  || echo BAD
git status --porcelain --untracked-files=all -- testdata/BASS_DR3 | wc -l   # 期望 367
```

## 2. `testdata/BASS_DR3/` — BASS DR3 单帧归档索引（元数据面）

- **数据源**：China-VO 镜像 <https://casdc.china-vo.org/archive/BASS/DR3/single_image/>
- **巡天**：BASS（Beijing–Arizona Sky Survey，2.3 m Bok + 90Prime，g/r 两波段）；
  DR3 论文 *The Third Data Release of the Beijing-Arizona Sky Survey*，
  [arXiv:1908.07099](https://arxiv.org/abs/1908.07099)
- **规模**：353 个观测日期目录（20150107–20190306）、552,300 个归档文件（science/weight/od 各 184,100），
  归档合计 ≈ 3.80 TB（`.fits.fz`，fpack 压缩）
- **本目录只读**：元数据、索引、工具、日期表入库；**0 个 FITS 文件**（FITS 用工具按需下载到 `downloads/`）
- **下载方式**（FITS 不入库，任何人可复现）：

  ```bash
  python3 testdata/BASS_DR3/tools/download_subset.py --pointing p7030g0031 --kinds science \
      --funpack --out testdata/BASS_DR3/downloads
  python3 testdata/BASS_DR3/tools/download_subset.py --dates 20151111 --kinds science,weight \
      --out testdata/BASS_DR3/downloads
  ```

- **重建索引**：`tools/crawl_bass_dr3.py`（重建 index.json/csv）、`tools/build_coords_index.py`（重建坐标索引，
  需先下载 `data/bassmzls-dr3-ccdinfo.fits` 245 MB）、`tools/verify_bass_dr3.py --sample 30`（一致性 + 抽样下载核对）
- **完整性**：`SHA256SUMS.txt` 记录全部入库产物的 SHA-256
- **详细说明**：`testdata/BASS_DR3/README.md`（数据集概览、字段表、覆盖天区、HiPS 规模估算、已知限制）

## 3. `testdata/HST_M16/` — HST WFC3/UVIS M16 三帧 drz（只读，不入库）

- **来源**：HST Heritage (HLSP) —— *Hubble Heritage* 项目 M16（鹰状星云）WFC3/UVIS 窄带镶嵌图，
  文件名 `hlsp_heritage_hst_wfc3-uvis_m16_<filter>_v1_drz.fits`（`drz` = AstroDrizzle 合成品）
- **下载**：MAST HLSP 集合 `HST Heritage`（<https://archive.stsci.edu/prepds/heritage/>）；
  三帧均为公开数据，非本仓生成、不随仓库分发
- **只读**：实验代码只以只读方式消费（`testdata/HST_M16/*.fits`），不写入、不改写、不重命名
- **体积**：3 × 268,917,120 B = 770 MB（单帧 8000×8400 float32 + 头），超 GitHub 入库上限 ⇒ gitignore
- **实测头部常量**（2026-09-21 逐帧读取，`astropy.io.fits.getheader`，BUNIT = ELECTRONS/S，NDRIZIM = 32）：

  | 文件（FILTER） | PHOTPLAM [Å] | PHOTFLAM [erg/cm²/Å/e⁻] | EXPTIME [s] | PHOTZPT | 帧型 |
  |---|---|---|---|---|---|
  | `..._f502n_v1_drz.fits` | 5009.64235 | 5.2676009e-18 | 16000.0 | −21.1 | O III |
  | `..._f657n_v1_drz.fits` | 6566.60545 | 2.2290223e-18 | 9600.0 | −21.1 | H-alpha |
  | `..._f673n_v1_drz.fits` | 6765.91580 | 2.2397195e-18 | 14400.0 | −21.1 | [S II] |

  三帧 WCS 网格逐位相同（同一天区、同输出网格）：`NAXIS1=8000`、`NAXIS2=8400`、
  `CD1_1 = −9.1016831619991e-06`、`ORIENTAT ≈ −35°`（像素尺度 0.0400″/px）。
- **消费方**：`实验/SCI-A/code/step2_hst_sim.py`（真实信号模板 → 物理前向仿真）、
  `实验/SCI-B/code/{sci_b_common.py,b3_domain_map.py}`（高对比结构适用域臂）、
  `实验/SCI-C/code/sci_c_common.py`（真实结构底图）；
  `实验/shared/synthetic/m16_*.py`（M16 合成场景渲染器）
- **校验**（可复跑，三帧齐备 + 头部常量）：

  ```bash
  python3 - <<'PY'
  import glob
  from astropy.io import fits
  fs = sorted(glob.glob("testdata/HST_M16/*.fits"))
  assert len(fs) == 3, fs
  for f in fs:
      h = fits.getheader(f, 0)
      print(h["FILTER"], h["PHOTPLAM"], h["PHOTFLAM"], h["EXPTIME"])
  PY
  ```
