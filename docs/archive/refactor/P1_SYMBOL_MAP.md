> **ARCHIVED_NON_NORMATIVE** — GOV-002 归档历史技术文档，不再作为当前权威。
> 替代文档：docs/architecture/MODULE_MAP.md、docs/modules/registry/

# P1-001: Phase1 old-symbol → 目标模块映射表

状态: **PASS** — HEAD=`f65fcc9`
规则: adapter 只转换数据与错误, 不复制算法 (P1-001)。
任何未映射旧能力 → blocker (当前: 0)。

## 映射表 (old symbol → module-ID → input/output → contracts)

| old symbol (p1_session API/内部) | module-ID (目标) | inputs (port → DATA) | outputs (port → DATA) | SCI | ALG | TEST |
|---|---|---|---|---|---|---|
| `p1_session_create` `p1_session_validate` `p1_session_run` `p1_session_inspect` `p1_session_destroy` | `astrocs.phase1.session` | config(DATA-CFG-P1-001) | manifest(DATA-MAN-P1-001) | SCI-SCOPE-001 | ALG-P1-SESSION-001 | TEST-P1-SESSION-001 |
| `read_image/aio_read` (内部读帧) | `astrocs.phase1.io` | raw frames(DATA-IMG-RAW-001) | image(DATA-IMG-RAW-001) | SCI-SCOPE-001 | ALG-P1-IO-001 | TEST-P1-IO-001 |
| calibration 管线 (bias/dark/flat 应用) | `astrocs.phase1.calibration` | raw+calib master(DATA-IMG-RAW-001, DATA-IMG-CAL-001) | calibrated(DATA-IMG-CAL-001) | SCI-CAL-001 | ALG-CAL-001 | TEST-CAL-001 |
| cosmetic correction (hot/dead/cosmic) | `astrocs.phase1.cosmetic` | calibrated(DATA-IMG-CAL-001) | cosmetic-corrected(DATA-IMG-CAL-001) | SCI-CAL-001 | ALG-CAL-002 | TEST-CAL-002 |
| WCS/PlateSolve (header/star) | `astrocs.phase1.wcs` | image+stars(DATA-IMG-CAL-001) | WCS(DATA-WCS-001) | SCI-WCS-001 | ALG-WCS-001 | TEST-WCS-001 |
| PSF 测量 | `astrocs.phase1.psf` | image(DATA-IMG-CAL-001) | PSF model(DATA-PSF-001) | SCI-PSF-001 | ALG-PSF-001 | TEST-PSF-001 |
| 星表抽取 (photometry 输入) | `astrocs.phase1.stars` | image(DATA-IMG-CAL-001) | star catalog(DATA-STAR-001) | SCI-PHOT-001 | ALG-PHOT-001 | TEST-PHOT-001 |
| noise/SNR 统计 | `astrocs.phase1.noise` | calibrated(DATA-IMG-CAL-001) | noise map(DATA-NOISE-001) | SCI-NOISE-001 | ALG-NOISE-001 | TEST-NOISE-001 |
| 输出落盘 (aio_write_fits/校准输出) | `astrocs.phase1.output` | calibrated(DATA-IMG-CAL-001) | FITS file(DATA-IMG-CAL-001) | SCI-SCOPE-001 | ALG-P1-OUT-001 | TEST-P1-OUT-001 |

## 能力覆盖审计 (old → mapped)

- `p1_session.h` 全部 5 API → `astrocs.phase1.session` ✅
- 内部 `read_image/map_aio_err/dispose_image/image_w/h/image_px` → `astrocs.phase1.io` ✅
- 校准管线 (apply calibration) → `astrocs.phase1.calibration` ✅
- 输出写回 (aio_write_fits) → `astrocs.phase1.output` ✅
- 未映射旧能力: **0** (无 blocker)

## 验证
- 映射表内每个 SCI-* 存在于 docs/contracts/INDEX.yaml (SCI-CAL/WCS/PHOT/PSF/NOISE/DRZ/UPM/REJ/INT/SCOPE 已登记)
- 每个 ALG-* 命名遵循 ALG-xxx 前缀 (INDEX.yaml 28 ALG 登记)
- DATA-* 引用遵循 DATA-IMG-* / DATA-CFG-* / DATA-WCS-* / DATA-PSF-* / DATA-STAR-* / DATA-NOISE-* / DATA-MAN-* 命名
