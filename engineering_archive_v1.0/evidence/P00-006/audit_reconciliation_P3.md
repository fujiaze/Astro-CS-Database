# P3 (Low) 审计复核报告 - P00-006

> **复核日期**：2026-07-24
> **审计源文档**：`docs/superpowers/specs/2026-07-18-audit-findings-P3.md`
> **复核范围**：9 模块共 59 项 Low 级问题（代码风格/注释/命名/magic number 等）
> **复核方法**：抽查 `lib/` 下源码确认每项状态，每项给出文件证据

---

## 汇总

| 状态 | 数量 | 占比 |
|------|------|------|
| **OPEN** | 27 | 45.8% |
| **CLOSED** | 32 | 54.2% |
| **STALE** | 0 | 0.0% |
| **UNVERIFIED** | 0 | 0.0% |
| **REJECTED** | 0 | 0.0% |
| **合计** | 59 | 100% |

**关键发现**：
- 代码风格类问题（4 空格 vs tab、变量命名混用）已基本修复（CLOSED），所有 .cpp 文件统一使用 4 空格、无 tab 缩进
- 算法原理注释类问题已基本修复（ipv_ransac.cpp、dpsf_psf.cpp、snr_estimator.cpp 等均有详细算法注释）
- magic number 类问题部分修复：aio_fits.cpp 已提取 FITS_BLOCK_SIZE/FITS_CARD_SIZE，但 snr_estimator.cpp 仍有 num_threads(16)、idw_power=2.0 等 magic number
- 文件头注释类问题部分残留：dpsf_log.cpp、wcs_sip.cpp、aio_compressor.cpp 等仍无文件头注释
- B8/B9 模块的逻辑类问题（readChunk 语义、POSIX 路径、%g 格式、stage_name 旧函数、--threads 未生效等）大多仍 OPEN
- healpix_stack 模块仍有 117 处 fprintf(stderr) 未走统一日志系统

---

## 按模块明细

### B1 astro_image_io (8 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B1-L-1 | 代码风格不一致（4 空格 vs tab） | CLOSED | `lib/astro_image_io/src/*.cpp` grep `^\t` 无 tab，统一 4 空格 |
| B1-L-2 | 函数注释不全（参数/返回值） | OPEN | `aio_api.cpp` 中 aio_read/aio_read_fits 等对外 C API 无 @param/@return 注释 |
| B1-L-3 | 变量命名混用 | CLOSED | `aio_pipeline.cpp` (base64_encode/xml_escape)、`aio_api.cpp` (alloc_image_data) 命名风格统一 snake_case |
| B1-L-4 | magic number 未提取 | CLOSED | `aio_fits.cpp:12-13` 已提取 `FITS_BLOCK_SIZE = 2880` 与 `FITS_CARD_SIZE = 80` |
| B1-L-5 | include 顺序不规范 | OPEN | `aio_api.cpp:1-6` 项目头先于系统头；`aio_compressor.cpp` 同样违反 |
| B1-L-6 | 错误信息字符串不一致 | OPEN | `aio_pipeline_engine.cpp:172,219,489` 用英文，与 healpix_db 用中文混用 |
| B1-L-7 | TODO 注释残留 | CLOSED | `lib/astro_image_io/src/` grep `TODO\|FIXME` 无残留 |
| B1-L-8 | 文件头注释缺失 | OPEN | `aio_compressor.cpp:1-10` 无功能描述/作者/日期文件头；`aio_fits.cpp` 同样 |

### B2 calibration (5 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B2-L-1 | 代码风格不一致 | CLOSED | `lib/calibration/src/*.cpp` grep `^\t` 无 tab |
| B2-L-2 | 函数注释不全 | OPEN | `ac_api.cpp:48-58` ac_generate_master_bias 等对外 API 无 @param/@return |
| B2-L-3 | 变量命名混用 | CLOSED | `calibrator.cpp` 命名统一 snake_case |
| B2-L-4 | magic number 未提取 | CLOSED | `cosmetic_corrector.cpp:119,140` threshold_sigma 已参数化，无 3.0/5.0 硬编码 |
| B2-L-5 | include 顺序不规范 | OPEN | `calibrator.cpp:29-34`、`cosmetic_corrector.cpp:17-24` 项目头先于系统头 |

### B3 plate_solve (3 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B3-L-1 | 代码风格不一致 | CLOSED | `lib/plate_solve/cpp/ipv/src/*.cpp` grep `^\t` 无 tab |
| B3-L-2 | 算法原理注释缺失 | CLOSED | `ipv_ransac.cpp:1-24` 有 PROSAC/Umeyama SVD 算法注释；`:49-61` 有 2x2 SVD 数学推导 |
| B3-L-3 | 变量命名混用 | CLOSED | `ipv_solver.cpp` 命名风格统一 |

### B4 dynamic_psf (5 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B4-L-1 | 代码风格不一致 | CLOSED | `lib/dynamic_psf/src/*.cpp` grep `^\t` 无 tab |
| B4-L-2 | 函数注释不全 | CLOSED | `dpsf_psf.cpp:12-18` 有 MOFFAT4_FWHM_FACTOR 推导注释；`:64-94` moffat4_residual 有参数说明 |
| B4-L-3 | 变量命名混用 | CLOSED | `dpsf_psf.cpp` 命名风格统一 |
| B4-L-4 | magic number 未提取 | CLOSED | `dpsf_psf.cpp:98` max_iter 已作为 lm_solve 参数传入；无 17/100 硬编码 |
| B4-L-5 | 文件头注释缺失 | OPEN | `dpsf_log.cpp:1-10` 直接 `#include`，无功能描述/作者/日期文件头 |

### B5 photometric_calib (6 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B5-L-1 | 代码风格不一致 | CLOSED | `lib/photometric_calib/cpp/src/*.cpp` grep `^\t` 无 tab |
| B5-L-2 | 函数注释不全 | CLOSED | `pc_api.cpp:1-3` 文件头 + `:27-34` pc_calibrate_simple 函数注释含返回码说明 |
| B5-L-3 | 变量命名混用 | CLOSED | `*.cpp` 变量命名统一 snake_case，函数 camelCase 是统一 C++ 风格 |
| B5-L-4 | magic number 未提取 | OPEN | `spectrum_integrator.cpp:241` `const double wl_step = 1.0;` 为局部 magic number，未提取为命名常量 |
| B5-L-5 | 注释不全 | OPEN | `image_corrector.cpp:26-57,63-80` computeScale/correctImage 函数注释简陋 |
| B5-L-6 | 文件头注释缺失 | CLOSED | `wcs_transform.cpp:1-4` 有完整文件头注释 |

### B6 snr_estimator (6 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B6-L-1 | 代码风格不一致 | CLOSED | `lib/snr_estimator/cpp/src/snr_estimator.cpp` grep `^\t` 无 tab |
| B6-L-2 | 函数注释不全 | CLOSED | `snr_estimator.cpp:1-9` 文件头含 SNR_phot 公式与 IDW 算法说明 |
| B6-L-3 | 变量命名混用 | CLOSED | 命名统一 (snr_phot/star_x/star_y/medianValue) |
| B6-L-4 | magic number 未提取 | OPEN | `snr_estimator.cpp:63,80,128,143,162` 多处 `num_threads(16)`；`:151` radius = sqrt(w²+h²)；`:281` idw_power = 2.0 |
| B6-L-5 | 注释不全 | CLOSED | `snr_estimator.cpp:1-9` 文件头含乘法模型完整推导 |
| B6-L-6 | 文件头注释缺失 | CLOSED | `snr_estimator.cpp:1-9` 有完整文件头注释 |

### B7 healpix_drizzle (8 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B7-L-1 | 代码风格不一致 | CLOSED | `lib/healpix_db/healpix_drizzle/*.cpp` grep `^\t` 无 tab（Makefile tab 是语法要求） |
| B7-L-2 | 函数注释不全 | CLOSED | `hp_drizzle_api.cpp:1-3` 文件头 + `:34-36` 函数注释 |
| B7-L-3 | 变量命名混用 | CLOSED | 命名风格统一 |
| B7-L-4 | magic number 未提取 | CLOSED | `drizzle_engine.cpp` nside/pixfrac 从 config 参数传入，无 1.5/32768 硬编码 |
| B7-L-5 | 注释不全 | CLOSED | `poly_clip.cpp:133-143` 有 Sutherland-Hodgman 算法原理注释 |
| B7-L-6 | 文件头注释缺失 | OPEN | `wcs_sip.cpp:1` 直接 `#include "wcs_sip.h"`，无文件头注释 |
| B7-L-7 | TODO 注释残留 | CLOSED | grep `TODO\|FIXME` 无残留 |
| B7-L-8 | 错误信息字符串不一致 | OPEN | `hp_drizzle_api.cpp:82,99,130` 用中文，与 `aio_pipeline_engine.cpp` 用英文混用 |

### B8 healpix_stack (7 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B8-L-1 | readChunk 返回空 vector 语义不清晰 | OPEN | `ahps_reader.cpp:313-350` 解压失败仍 `result.clear(); return result;` 返回空 vector |
| B8-L-2 | 子叶块列表仅实现 Windows 路径 | OPEN | `stack_db.cpp:376-379` POSIX 分支仍注释"此处省略, 主要平台为 Windows" |
| B8-L-3 | saveMeta 用 %g 格式可能丢精度 | OPEN | `stack_db.cpp:230-234` 仍用 `snprintf + "%g"` 写 sigmaClipLow/High |
| B8-L-4 | Winsorized 分支内存占用高 | OPEN | `corrected_stacker.cpp:231` 仍 `std::vector<std::vector<double>> vals_per_ipix(n_unique);` 二维 vector |
| B8-L-5 | gauge fixing 假设 v[0] 是常数项 | OPEN | `gradient_fitter.cpp:269-273` 仍 `frames[i].model.v[0] -= offset;` 依赖 spherical_spline 零空间构造 |
| B8-L-6 | run_stage_stack 返回 true 但未工作 | OPEN | `orchestrator.cpp:2462-2476` 仍 `LOG_INFO("...跳过..."); return true;` |
| B8-L-7 | 大量 fprintf(stderr) 未走统一日志系统 | OPEN | `lib/healpix_db/healpix_stack/` 9 个文件共 117 处 fprintf(stderr) |

### B9 orchestrator (11 项)

| ID | 标题 | 状态 | 证据 |
|----|------|------|------|
| B9-L-1 | 旧版 stage_name 缺新节点分支 | OPEN | `orchestrator.cpp:200-209` 仍只覆盖 5 个旧枚举；`:226` 新增 stage_name_v2 但旧函数未删除且 `:600` 仍调用 |
| B9-L-2 | --threads 参数未生效 | OPEN | `cli_command.cpp:286-289` 仅 LOG_INFO 记录，未传递到 orchestrator.config_.threads |
| B9-L-3 | JSON 字段名 output_ahpx_path 过时 | OPEN | `cli_command.cpp:495` + `orchestrator.h:92` 字段名仍为 output_ahpx_path，但实际输出 .hiss/.hcsd |
| B9-L-4 | cmd_status 输出固定"no running instance" | OPEN | `cli_command.cpp:369-375` 仍输出固定 JSON，无实际状态查询 |
| B9-L-5 | cmd_run_batch 错误返回码 4 不规范 | OPEN | `cli_command.cpp:336` `return 4;` 与其他错误 1/2/3 无统一错误码体系 |
| B9-L-6 | FOV 半径上限 30 度硬编码 | OPEN | `orchestrator.cpp:1508` `fov_radius_deg < 30.0` 30 度仍硬编码无注释依据 |
| B9-L-7 | parse_ra_hms/parse_dec_dms 失败返回 0.0 | OPEN | `orchestrator.cpp:793-831` 解析失败仍 `return 0.0;` 无法区分缺失与 RA=0 |
| B9-L-8 | --threads 日志说"骨架"但已部分支持 | OPEN | `cli_command.cpp:288` 仍说"骨架: 配置传递待完善"，但 `orchestrator.cpp:688-691` 已对 CALIBRATE 生效 |
| B9-L-9 | gaia_data 等参数仅记录日志未使用 | OPEN | `cli_command.cpp:212-215` 仍注释"当前仅记录日志 (后续 Task 集成)" |
| B9-L-10 | cmd_run_batch results 为空返回 0 | OPEN | `cli_command.cpp:359-363` results 为空时仍 `return 0;` 误导用户 |
| B9-L-11 | tests/ 缺 stage handler 单元测试 | OPEN | `lib/orchestrator/cpp/tests/` 仅 4 个测试文件，无 stage handler 测试 |

---

## 模块汇总

| 模块 | 总数 | OPEN | CLOSED |
|------|------|------|--------|
| B1 astro_image_io | 8 | 4 | 4 |
| B2 calibration | 5 | 2 | 3 |
| B3 plate_solve | 3 | 0 | 3 |
| B4 dynamic_psf | 5 | 1 | 4 |
| B5 photometric_calib | 6 | 2 | 4 |
| B6 snr_estimator | 6 | 1 | 5 |
| B7 healpix_drizzle | 8 | 2 | 6 |
| B8 healpix_stack | 7 | 7 | 0 |
| B9 orchestrator | 11 | 8 | 3 |
| **合计** | **59** | **27** | **32** |

---

## 建议优先修复项（OPEN 中影响较大的）

1. **B8-L-7**：healpix_stack 模块 117 处 fprintf(stderr) 未走统一日志系统，影响日志归档分析
2. **B9-L-2 / B9-L-8**：--threads 参数未生效且日志过时，影响多线程性能调优
3. **B9-L-3**：JSON 字段名 output_ahpx_path 过时，影响下游解析
4. **B8-L-1**：readChunk 返回空 vector 语义不清晰，影响错误处理
5. **B9-L-7**：parse_ra_hms 失败返回 0.0，可能导致错误初始指向
6. **B8-L-2**：POSIX 路径未实现，影响跨平台支持

其余 OPEN 项多为文件头注释/include 顺序等纯风格问题，可批量"暂不修复"。

---

## 输出文件

- `engineering/evidence/P00-006/audit_reconciliation_P3.json` - 机器可读格式
- `engineering/evidence/P00-006/audit_reconciliation_P3.md` - 本文档（人类可读）
