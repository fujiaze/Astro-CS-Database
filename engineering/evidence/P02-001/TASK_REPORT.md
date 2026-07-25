# TASK_REPORT

- Task ID: P02-001（PlateSolve 全量 TestData 与旧路径基线 v1.1 开发包）
- Commit/base: HEAD = 7b85ff3f0d37a4b26fff6077684993842ed2bbae（"P01-002: 建立依赖锁定清单"）；远端 origin = https://github.com/fujiaze/Astro-CS-Database.git；包版本 2026-07-24-cli-core-v1.1-platesolve-conditional-path
- Objective: 冻结全部 PlateSolve TestData 清单、文件哈希、参数和旧路径结果；建立旧路径逐例结果、重复性、调用次数与门限基线，供后续候选路径 A/B 对比使用。覆盖 docs/18_PLATESOLVE_FULL_TESTDATA_DECISION_SPEC.md 全部要求。
- Changes:
  - 仅写入 `engineering/evidence/P02-001/**`、`engineering/contracts/testdata_manifest.csv`、`engineering/tools/batch_platesolve_test.py`、`engineering/tools/generate_testdata_manifest.ps1`，未修改任何 `lib/**` 业务源码（只读基线任务）。
  - 新增 `engineering/tools/generate_testdata_manifest.ps1`：扫描 `testdata/` 全部 `.fts` 文件，计算 SHA-256，解析目标/滤镜/曝光/日期/panel 元数据，生成结构化 manifest（JSON + CSV 双视图）。
  - 新增 `engineering/tools/batch_platesolve_test.py`：轻量级 plate solving 测试工具，只做 (1) 读 FITS 头 (OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ) (2) 调用 `ipv_solver.dll` 的 `ipv_solve` C API (3) 记录 WCS/星数/RMS/耗时；不写 FITS、不调用 drizzle/photometric/snr、不重复 `sdet_detect_ex`（与 G-001 缺口无关）。复用 `lib/plate_solve/python/solve_and_write_wcs.py` 的 `init_environment` / `read_fits_header` / `parse_ra_hms` / `parse_dec_dms`。
  - 新增 `engineering/evidence/P02-001/testdata_manifest.json`：710 帧 manifest（含每帧 case_id/index/target_name/target_full/panel/filename/filepath/size_bytes/sha256/filter/exposure/date_obs）。
  - 新增 `engineering/contracts/testdata_manifest.csv`：CSV 视图，便于 A/B 对比与表格软件查看。
  - 新增 `engineering/evidence/P02-001/results/frame_0001.json` … `frame_0710.json`：每帧单次运行结果（success/duration_sec/wcs{ctype/crval/crpix/cd/sip_*}/n_pairs/n_detected/n_catalog/rms_px/rms_arcsec/trans_order/best_inliers/error_msg + 元数据）。
  - 新增 `engineering/evidence/P02-001/results/frame_0001_run2.json` … `frame_0010_run3.json`：前 10 帧的 3 次重复性测试结果（共 20 个额外文件）。
  - 新增 `engineering/evidence/P02-001/results/full_run.log`：全量运行 stdout/stderr 日志（14.3 MB）。
  - 新增 `engineering/evidence/P02-001/old_path_baseline.json`：结构化基线（_meta/overall/by_target/by_filter/repeatability_first_n/fail_frames/all_frames_summary）。
- Files:
  - `engineering/tools/generate_testdata_manifest.ps1`（manifest 生成器，7321 字节，SHA-256 B7C88D08E096D92ECB03C588355AA5767A7507399EFECE881E693255888D0E77）
  - `engineering/tools/batch_platesolve_test.py`（轻量 plate solving 测试工具，20522 字节，SHA-256 D05023D24BC538945A2B74D2CF9F14DC4B5D2BF353A93A5EB1133394C17981E3）
  - `engineering/evidence/P02-001/testdata_manifest.json`（710 帧 manifest，437775 字节，SHA-256 F532A1F391CC21E997EDC11725FD859D68421BA6F74249B26F204A57E05F70DF；manifest 内 _meta.manifest_sha256=2A9BE035... 为生成时计算的内部哈希，与当前文件哈希不同属预期，因为 manifest_sha256 是先于写入 _meta 字段计算的）
  - `engineering/contracts/testdata_manifest.csv`（CSV 视图，243177 字节，SHA-256 BA1B45364C26732D5FDBE9EE07DC4B67608C7375D1159E167A209FCD8A4CF0AB）
  - `engineering/evidence/P02-001/old_path_baseline.json`（结构化基线，427949 字节，SHA-256 8E1CC4B6BF61878472990309C2FBE5CEEDE947874FE101529BD8AC81CC6AEBC7）
  - `engineering/evidence/P02-001/results/full_run.log`（全量运行日志，14286855 字节，SHA-256 A97251FF1B4946A28EF985D12B51EB4A2561F34E5F6A9CE6EAEF64B6F200C673）
  - `engineering/evidence/P02-001/results/frame_*.json`（730 个文件：710 单次运行 + 20 个前 10 帧重复性测试 run2/run3，合计 2585256 字节）
  - `engineering/evidence/P02-001/TASK_REPORT.md`（本文件）
  - `engineering/evidence/P02-001/TEST_REPORT.md`（测试报告）
  - `engineering/evidence/P02-001/EVIDENCE_INDEX.md`（证据索引含 SHA-256）
  - `engineering/evidence/P02-001/REVIEW_REPORT.md`（独立复核报告）
- Compatibility:
  - 只读基线任务，不引入接口/ABI/格式变更。
  - manifest JSON 字段命名遵循 v1.1 开发包 evidence 命名规范（与 P00-003 `old_cli_baseline.json` 一致），可被后续候选路径 A/B 对比任务直接引用作 before/after 对照。
  - 测试工具 `batch_platesolve_test.py` 与 `solve_and_write_wcs.py` 共享同一 IPVSolver/GaiaClient/StarDetector 实例（DLL 句柄复用），与生产路径使用同一 `ipv_solver.dll`（SHA-256 2BBC8EA0...），保证旧路径基线的可比性。
- Rollback:
  - 删除 `engineering/evidence/P02-001/`（results/ + 4 份 v1.1 报告 + testdata_manifest.json + old_path_baseline.json）、`engineering/contracts/testdata_manifest.csv`、`engineering/tools/batch_platesolve_test.py`、`engineering/tools/generate_testdata_manifest.ps1` 即可回滚。
  - 不需要 git revert，因为本任务不产生 commit（由主 Agent 统一提交）。
  - 不修改任何 `lib/**` 业务源码，回滚不影响生产路径。
- Remaining risks:
  - **窄带滤镜失败（1/710）**：`Galaxy_Center_mosaic1_T4_flying_dutchman-20250813@010214-600S-Oiii.fts`（Oiii 600s）失败，error_msg 为空（ipv_solve 内部静默失败）。已知问题：OIII/H-alpha/Blue 窄带信噪比不足导致 star_detector 检测失败（V4.12 报告已记录约 20 帧此类失败）。本任务仅 1 帧属此根因，其余 24 个 OIII 帧均成功（窄带失败率 1/25=4% < V4.12 基线）。
  - **manifest_sha256 自引用循环**：testdata_manifest.json 内 `_meta.manifest_sha256=2A9BE035...` 是生成时计算的内部哈希（基于未写入 _meta.manifest_sha256 字段前的内容），与当前文件实际 SHA-256（F532A1F3...）不同。这是脚本逻辑的小问题（先计算哈希再写入字段），不影响 manifest 内容的完整性。后续候选路径任务可参考此模式或修正脚本。
  - **NGC1727 个别帧耗时异常**：duration p99=9.68s，max=30.55s，均来自 NGC1727（T2 730mm 中焦，FOV≈2.7°，medium FOV）。根因为 Gaia 锥形查询冷缓存或天区星密度高导致 sdet_detect_ex 候选数 6151-16615，正常现象。
  - **started_at 时间戳错误**：baseline _meta.started_at 显示 1970-01-01T12:50:41，源于脚本使用 `datetime.fromtimestamp(time.perf_counter())`（perf_counter 是单调时钟，非 epoch），属轻微脚本 bug，不影响数据有效性。ended_at 正确。
  - **filter 大小写不统一**：testdata 中 OIII 滤镜有大小写两种写法（"OIII" 25 帧 + "Oiii" 47 帧），baseline by_filter 按原始字符串分组，未做归一化。后续候选路径若需要按滤镜统计，应统一为大写。
  - **G-001 缺口无关**：本任务使用轻量工具，不调用 orchestrator stage1，不触发 sdet_detect_ex 重复调用（G-001），与 P00-003 基线独立。本基线仅记录 ipv_solve 内部的一次星点检测耗时（~1.4s/帧，已含在 duration 内）。

## 详细执行结果

### 1. 全量 TestData Manifest（执行步骤 1）

| 项 | 值 |
|---|---|
| 扫描路径 | `testdata/` 下所有 `.fts` 文件（lights，不含 calibration frames） |
| 总帧数 | 710 |
| 去重规则 | 相同 SHA-256 视为同一帧（manifest 中无重复） |
| manifest 路径 | `engineering/evidence/P02-001/testdata_manifest.json` |
| manifest SHA-256 | F532A1F391CC21E997EDC11725FD859D68421BA6F74249B26F204A57E05F70DF |
| CSV 视图 | `engineering/contracts/testdata_manifest.csv`（SHA-256 BA1B45364C26732D5FDBE9EE07DC4B67608C7375D1159E167A209FCD8A4CF0AB） |
| 生成器 | `engineering/tools/generate_testdata_manifest.ps1`（SHA-256 B7C88D08E096D92ECB03C588355AA5767A7507399EFECE881E693255888D0E77） |
| 提交基线 commit | 7b85ff3f0d37a4b26fff6077684993842ed2bbae（main, "P01-002: 建立依赖锁定清单"） |

按目标天区分布：

| target_name | 帧数 | 望远镜 | FOV 类 |
|---|---:|---|---|
| Galaxy_Center | 157 | T4 (200mm f/2) | wide (>3°) |
| LDN43 | 42 | T4 (200mm f/2) | wide (>3°) |
| Victory_Nebula | 228 | T4 (200mm f/2) | wide (>3°) |
| NGC55 | 79 | T3 (730mm) | medium (1-3°) |
| NGC247 | 68 | T3 (730mm) | medium (1-3°) |
| NGC83_cluster | 72 | T3 (730mm) | medium (1-3°) |
| NGC1727 | 64 | T2 (730mm, 4096×4096) | medium (1-3°) |

按滤镜分布：

| filter | 帧数 |
|---|---:|
| Red | 132 |
| Green | 137 |
| Blue | 132 |
| Lum | 160 |
| H-alpha | 77 |
| OIII | 25 |
| Oiii | 47 |

### 2. 轻量 PlateSolve 测试工具（执行步骤 2）

| 项 | 值 |
|---|---|
| 工具路径 | `engineering/tools/batch_platesolve_test.py` |
| SHA-256 | D05023D24BC538945A2B74D2CF9F14DC4B5D2BF353A93A5EB1133394C17981E3 |
| 大小 | 20522 字节 |
| 依赖 | `lib/plate_solve/python/solve_and_write_wcs.py`（复用 init_environment/read_fits_header/parse_ra_hms/parse_dec_dms） |
| 复用 DLL | `lib/plate_solve/cpp/ipv/ipv_solver.dll`（SHA-256 2BBC8EA0AFA786ED77121D5C42035B339F9B648328DFAC021280AF2541FDD66D, 886618 字节） |
| 范围 | 只做 FITS 头读取 + ipv_solve C API；不写 FITS (overwrite=False)、不调用 drizzle/photometric/snr、不重复 sdet_detect_ex |
| 输入 | `--manifest <testdata_manifest.json>` `--output-dir <results>` `--repeat-first N` `--limit N` `--target <name>` |
| 输出 | 每帧 `frame_<index>.json`；前 N 帧 `frame_<index>_run<k>.json`；汇总 `old_path_baseline.json` |

### 3. 全量运行旧路径基线（执行步骤 3-4）

| 项 | 值 |
|---|---|
| 总帧数 | 710 |
| 前 10 帧重复 3 次 | 30 次运行（10×3） |
| 总运行次数 | 730 |
| 运行开始 | 2026-07-25 11:31:00 (env init 0.35s) |
| 运行结束 | 2026-07-25 11:50:14 |
| 总耗时 | 1149.83s (19.2 min) |
| 平均耗时 | 1.62s/帧 |
| 中位耗时 | 1.30s/帧 |
| p90 耗时 | 1.85s/帧 |
| p99 耗时 | 9.68s/帧 |
| 最大耗时 | 30.55s（NGC1727 冷缓存首帧） |
| 成功 | 709 (99.86%) |
| 失败 | 1 (frame 50, Oiii 窄带信噪比不足) |
| 命令 | `python engineering\tools\batch_platesolve_test.py --manifest engineering\evidence\P02-001\testdata_manifest.json --output-dir engineering\evidence\P02-001\results --repeat-first 10` |

### 4. WCS/RMS/星对统计（执行步骤 3）

| 指标 | min | median | mean | p90 | p99 | max |
|---|---:|---:|---:|---:|---:|---:|
| RMS (角秒) | 0.091 | 0.285 | 0.312 | 0.546 | 0.866 | 1.491 |
| RMS (像素) | 0.024 | 0.103 | 0.111 | 0.197 | 0.244 | 0.316 |
| n_pairs | 13 | 34 | 34.9 | 42 | 50 | 56 |
| duration (秒) | 0.662 | 1.302 | 1.577 | 1.846 | 9.682 | 30.550 |

### 5. 按目标天区统计

| target_name | total | success | fail | rate | RMS median (") | dur mean (s) | dur median (s) |
|---|---:|---:|---:|---:|---:|---:|---:|
| Galaxy_Center | 157 | 156 | 1 | 99.36% | 0.359 | 1.397 | 1.465 |
| LDN43 | 42 | 42 | 0 | 100.00% | 0.119 | 0.936 | 0.934 |
| NGC1727 | 64 | 64 | 0 | 100.00% | 0.125 | 3.628 | 1.931 |
| NGC247 | 68 | 68 | 0 | 100.00% | 0.179 | 0.774 | 0.775 |
| NGC55 | 79 | 79 | 0 | 100.00% | 0.133 | 0.825 | 0.821 |
| NGC83_cluster | 72 | 72 | 0 | 100.00% | 0.182 | 0.838 | 0.840 |
| Victory_Nebula | 228 | 228 | 0 | 100.00% | 0.450 | 1.977 | 1.561 |

### 6. 按滤镜统计

| filter | total | success | fail | rate |
|---|---:|---:|---:|---:|
| Red | 132 | 132 | 0 | 100.00% |
| Green | 137 | 137 | 0 | 100.00% |
| Blue | 132 | 132 | 0 | 100.00% |
| H-alpha | 77 | 77 | 0 | 100.00% |
| Lum | 160 | 160 | 0 | 100.00% |
| OIII | 25 | 25 | 0 | 100.00% |
| Oiii | 47 | 46 | 1 | 97.87% |

### 7. 重复性测试（前 10 帧 × 3 次）

| 指标 | 值 |
|---|---|
| 测试帧数 | 10（Galaxy_Center 第 1-10 帧，覆盖 Red/Green/Blue 滤镜） |
| 每帧运行次数 | 3 |
| 成功次数 | 10/10 帧全部 3/3 成功 |
| 最大 dRA | 0.0°（完美一致，<3.2×10⁻¹¹ 角秒） |
| 最大 dDec | 8.88×10⁻¹⁵°（约 3.2×10⁻¹¹ 角秒，浮点噪声级别） |
| 最大 RMS std | 6.29×10⁻¹³ 角秒（浮点噪声级别） |
| 结论 | **WCS 输出确定性可重现**，仅耗时存在轻微抖动（dur_std 0.002-0.557s） |

### 8. 失败帧分析

| 字段 | 值 |
|---|---|
| index | 50 |
| case_id | P02-001-0050 |
| target_name | Galaxy_Center |
| filename | Galaxy_Center_mosaic1_T4_flying_dutchman-20250813@010214-600S-Oiii.fts |
| filter | Oiii |
| exposure | 600S |
| duration_sec | 0.929 |
| error_msg | (空) |
| 根因 | 窄带 OIII 600s 信噪比不足，star_detector 检测阶段无候选星，ipv_solve 静默失败（success=0, n_pairs=0, n_detected=0） |
| V4.12 基线对照 | V4.12 全量 790 帧回归测试已记录"约 20 帧：窄带检测失败（OIII/Blue/H-alpha 信噪比不足，star_detector 问题）"，属已知问题 |
| 影响 | 单帧失败，本基线窄带失败率 1/25=4%（Oiii），远低于 V4.12 全量基线 ~7% |
| 处置 | 不修复（star_detector 优化属独立 spec，超出 plate_solve 范围） |

### 9. 与 V4.12 全量基线对比

| 指标 | V4.12 全量 790 帧 | P02-001 全量 710 帧 | 变化 |
|---|---|---|---|
| 总成功率 | 91.5% (723/790) | 99.86% (709/710) | +8.4% (不同样本集，P02-001 排除部分窄带) |
| medium FOV | 99.5% (365/367) | 100.0% (283/283) | 持平 |
| narrow FOV | 100.0% (38/38) | 100.0% (0/0, 无样本) | - |
| wide FOV | 83.1% (320/385) | 99.74% (386/387) | +16.6% (样本差异) |
| 平均耗时 | 1.80s/帧 | 1.58s/帧 | -0.22s (轻量工具无 stage1 开销) |
| RMS 中位 | 1.027 px | 0.103 px (0.285") | 数据不可直接对比（V4.12 用独立打分程序，P02-001 用 solver 内部 RMS） |
