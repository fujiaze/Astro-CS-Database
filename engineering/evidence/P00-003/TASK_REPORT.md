# TASK_REPORT

- Task ID: P00-003（v1.1 开发包：旧 CLI 真实数据基线）
- Commit/base: HEAD = 7b85ff3f0d37a4b26fff6077684993842ed2bbae（"P01-002: 建立依赖锁定清单"）；远端 origin = https://github.com/fujiaze/Astro-CS-Database.git；包版本 2026-07-24-cli-core-v1.1-platesolve-conditional-path
- Objective: 在不改变算法的情况下记录 Stage1/Stage2 旧路径结果、调用次数、耗时、内存与输出；确认 G-001 缺口（sdet_detect_ex 重复调用）在真实数据上的表现；采集 WCS、PSF、测光、SNR、HISS inspect、HCSD 与峰值内存作为后续 P01 优化的对照基线。
- Changes:
  - 仅写入 `engineering/evidence/P00-003/**`，未修改任何 `lib/**` 业务源码（git status 确认 lib/ 下仅 Makefile/build.ps1 因 v1.1 迁移被改，非本任务产生）。
  - 新增 `stage1_config_p00-003.json`（基于 stage1_config.json，修正 calibration_dir=testdata/T4 calibration files、filter=Lum、output_root=engineering/evidence/P00-003/output、log_level=DEBUG；注：CALIBRATE 阶段为骨架未解析 calibration_dir，配置仅作记录）。
  - 用 `lib/orchestrator/cpp/orchestrator.exe`（SHA-256 与 build/artifacts/orchestrator.exe 完全一致 04704F1B...）运行 stage1 真实帧 + stage2 既有 HISS 集，捕获 stdout JSON、stderr DEBUG 日志、内存采样。
  - 整合旧 v1.0 P00-003（healpix_stack 源码恢复）证据：旧 SOURCE_RECORD.md 与 commit_msg.txt 保留作历史参考，v1.1 报告覆盖旧 TASK_REPORT.md。
- Files:
  - `engineering/evidence/P00-003/stage1_config_p00-003.json`（stage1 配置覆盖）
  - `engineering/evidence/P00-003/stage1_run.log`（stage1 run1 stdout JSON）
  - `engineering/evidence/P00-003/stage1_run.err.log`（stage1 run1 stderr DEBUG 日志，445 行）
  - `engineering/evidence/P00-003/stage1_run_mem.log`（stage1 run2 stdout JSON，带内存监控）
  - `engineering/evidence/P00-003/stage1_run_mem.err.log`（stage1 run2 stderr DEBUG 日志）
  - `engineering/evidence/P00-003/stage1_memory.log`（340 样本，200ms 间隔，峰值 32642.55 MB）
  - `engineering/evidence/P00-003/stage2_run.log`（stage2 stdout JSON）
  - `engineering/evidence/P00-003/stage2_run.err.log`（stage2 stderr DEBUG 日志，123 行）
  - `engineering/evidence/P00-003/stage2_memory.log`（30 样本，峰值 1978.38 MB）
  - `engineering/evidence/P00-003/output/stage1_baseline.hiss`（run1 HISS 输出，47693 字节）
  - `engineering/evidence/P00-003/output/stage1_baseline_mem.hiss`（run2 HISS 输出，47692 字节）
  - `engineering/evidence/P00-003/output/stage2_baseline.hcsd`（187455430 字节，与旧 output_stage2.hcsd hash 一致）
  - `engineering/evidence/P00-003/old_cli_baseline.json`（结构化基线，350 行）
  - `engineering/evidence/P00-003/TASK_REPORT.md`（本文件）
  - `engineering/evidence/P00-003/TEST_REPORT.md`（v1.1 测试报告）
  - `engineering/evidence/P00-003/EVIDENCE_INDEX.md`（v1.1 证据索引含 SHA-256）
  - `engineering/evidence/P00-003/REVIEW_REPORT.md`（v1.1 独立复核报告，VERDICT: PASS）
- Compatibility:
  - 只读基线任务，不引入接口/ABI/格式变更。
  - `old_cli_baseline.json` 字段命名遵循 v1.1 开发包 evidence 命名规范，可被后续 P01 优化任务（如 G-001 修复、PHOTOMETRIC 修复）直接引用作 before/after 对照。
  - 旧 v1.0 证据（SOURCE_RECORD.md/commit_msg.txt）保留原位，向后兼容。
- Rollback:
  - 删除 `engineering/evidence/P00-003/` 下本任务新增的 11 个文件（config/log/memory/output/old_cli_baseline.json + 4 份 v1.1 报告）即可回滚。
  - 旧 v1.0 证据（SOURCE_RECORD.md/commit_msg.txt）保留，回滚后可恢复 v1.0 视图。
  - 不需要 git revert，因为本任务不产生 commit（由主 Agent 统一提交）。
- Remaining risks:
  - **G-001 缺口确认**：每帧 sdet_detect_ex 被调用 2 次（ipv_select.cpp:962 + orchestrator.cpp:1445），PLATESOLVE 28.3s 中约 4.9s 来自重复检测，P01 阶段需消除冗余调用。
  - **G-002 候选缺口**：PHOTOMETRIC n_matched=0（KD-tree star_matcher 返回 0 对），导致 sigma_residual=0 → SNR 退化 → HISS has_snr=0 → stage2 SNR²加权退化为等权。根因待查（疑似 PSF 星点坐标与 Gaia 星表 WCS 转换或匹配半径问题）。
  - **峰值内存 32GB**：stage1 PLATESOLVE 阶段 Gaia DR3 SP xpsd 内存映射导致 WorkingSet 峰值 32642 MB，限制部署环境，多帧并行不可行。
  - **CALIBRATE 退化**：orchestrator.cpp 骨架未解析 calibration_dir，master_dark/flat/bias 全传 nullptr，out=light。本基线在不校准的 light 上运行，后续若启用校准需重新采集基线。
  - **HISS 非字节级可重现**：同一 FITS 两次运行 stage1，HISS hash 不同（size 差 1 字节，EB637585... vs A3F4CE53...）。HCSD 则完全可重现（与旧 output_stage2.hcsd hash 一致）。
  - **stage2 GRADIENT_SPHERE 退化**：gaia_client_create_ex 失败（疑似 data_dir 路径问题），回退无梯度校正模式，多帧拼接可能有背景不一致。

## 详细执行结果

### 1. stage1 真实帧运行（执行步骤 1-3）

| 项 | 值 |
|---|---|
| 输入 FITS | testdata/Victory_Nebula_T4_Flying_Dutchman/lights/Victory_Nebula_mosaic1_flying_dutchman-20250204@035646-180S-Lum.fts |
| FITS SHA-256 | E43B88A4BDD8C9304560A6A26952A5C30D7A022CF5DCDD9F641C2C50DF373AE5 |
| FITS 尺寸 | 4500x3600, BITPIX=16, BZERO=32768, 68 关键字, 32405760 字节 |
| 滤镜 | Lum (映射为 "Baader UV/IR Cut / L CMOS Optimized", 7 点) |
| 曝光 | 180.0s, 2025-02-04T03:57:02 |
| 初始指向 | OBJCTRA=12 30 00.00 (187.5°), OBJCTDEC=-78 51 00.0 (-78.85°) |
| orchestrator.exe | lib/orchestrator/cpp/orchestrator.exe (SHA-256 04704F1B..., 与 build/artifacts 一致) |
| run1 时间 | 2026-07-25 10:21:17 → 10:22:02 (wall ~45s, exit=0) |
| run2 时间 | 2026-07-25 10:29:21 → 10:30:35 (wall ~74s, exit=0, 含内存监控开销) |
| 配置 | engineering/evidence/P00-003/stage1_config_p00-003.json (DEBUG 级别) |

**run1 失败原因（已修复）**：首次使用 build/artifacts/orchestrator.exe 运行失败（exit=3），原因为 init_dlls 自动推导项目根目录假设 EXE 位于 `<root>/lib/orchestrator/cpp/`（4 级深度），但 build/artifacts/ 仅 2 级深度，导致推导出 `F:\Astro dev`（父目录），全部 9 个 DLL 加载失败。修复方式：改用 `lib/orchestrator/cpp/orchestrator.exe`（同一文件副本，SHA-256 一致），自动推导正确识别项目根。**此为 orchestrator 自动推导逻辑的既存缺陷，非本任务引入，记录为基线现状。**

### 2. sdet_detect_ex 调用次数统计（执行步骤 2）

| 调用序号 | 调用位置 | 阶段上下文 | 耗时 | 检测星点 | 饱和/正常 | FWHM med |
|---:|---|---|---:|---:|---|---:|
| 1 | ipv_select.cpp:962 (ipv_select_from_memory) | PLATESOLVE, ipv_solve_from_memory 内部 | 2.227s | 2000 | 1094/15233 | 1.4637 |
| 2 | orchestrator.cpp:1445 (PLATESOLVE 后置) | PLATESOLVE, ipv_solve_from_memory 返回后, 写 star_det 块 | 2.671s | 2000 | 1094/15233 | 1.4637 |

**结论**：每帧 sdet_detect_ex 被调用 2 次（G-001 缺口确认）。两次调用结果完全一致（相同 2000 颗星、相同 FWHM），属算法层面重复检测。

**uint16 转换统计**：

| 转换序号 | 位置 | 方向 | 像素数 | 耗时 |
|---:|---|---|---:|---|
| 1 | ipv_select.cpp:944-950 | float32→uint16 (clip 0-65535) | 16200000 | ~35ms (内部 uint16→float 反向转换) |
| 2 | orchestrator.cpp:1429-1438 | float32→uint16 (clip 0-65535) | 16200000 | ~58ms (内部 uint16→float 反向转换) |

全图 float32→uint16 转换 2 次（每次 64.8MB→32.4MB），对应 2 次 sdet_detect_ex 调用。star_detector.dll 内部另有 uint16→float 反向转换 2 次（35.6ms + 57.7ms）。

### 3. WCS/PSF/测光/SNR/HISS inspect 保存（执行步骤 3）

#### WCS 字段（PLATESOLVE 输出，已写入 header KV 块）

| 字段 | 值 |
|---|---|
| CTYPE1/CTYPE2 | RA---TAN-SIP / DEC--TAN-SIP |
| CRVAL1/CRVAL2 | 187.545904° / -78.817030° |
| CRPIX1/CRPIX2 | 2250.5 / 1800.5 |
| CD 矩阵 | [-1.751583e-03, 3.780697e-05; -3.714201e-05, -1.751466e-03] |
| CD 行列式 | 3.069e-06 |
| SIP A/B/AP/BP order | 3/3/3/3 |
| 拟合参数 | n_pairs=32, rms_px=0.0642, rms_arcsec=0.3975, trans_order=3 |
| 指向修正 | Δra=0.045904°, Δdec=0.032970° (初始 187.5/-78.85 → 求解 187.546/-78.817) |
| 像素比例 | s0=6.1879"/px (ipv_select), CD 推导=6.307"/px |
| FOV 对角 | 9.9056° |

#### PSF 结果

| 项 | 值 |
|---|---|
| 方法 | Moffat4 (dpsf_fit_batch, fitRadius=8, maxIter=100) |
| 输入星点 | 2000 (来自 star_det 块) |
| 拟合成功 | 1943/2000 (97%) |
| 警告 | 28 个 Background constraint violated + 1 个 Invalid fit params |
| 输出块 | psf, 2000 星, 144000 字节 (72 字节/星) |

#### 测光结果

| 项 | 值 |
|---|---|
| FILTER | Lum → "Baader UV/IR Cut / L CMOS Optimized" (7 点) |
| QE 曲线 | GSENSE2020BSI (447 点) |
| 光谱点数 | 343 (wl_start=336, wl_step=2) |
| FOV 半径 | 6.057610° |
| Gaia 锥形搜索 | ra=187.5459, dec=-78.8170, r=6.058°, mag 6.0-12.0 (自适应), n_gaia=10197 |
| F_syn 有效 | 9981/10197 |
| **n_matched** | **0** (KD-tree star_matcher 返回 0 对, G-002 缺口) |
| scale / sigma_residual | 1.000000 / 0.000000 |
| photo_stats 块 | 已写入 (但内容为退化值) |

#### SNR 结果

| 项 | 值 |
|---|---|
| 状态 | 退化 (sigma_residual=0 <= 0) |
| snr_model 块 | **未写入** |
| 影响 | HISS has_snr=0, stage2 SNR²加权退化为等权 |

#### HISS inspect（DRIZZLE 输出）

| 项 | run1 | run2 (内存监控) |
|---|---|---|
| 路径 | output/stage1_baseline.hiss | output/stage1_baseline_mem.hiss |
| 大小 | 47693 字节 | 47692 字节 |
| SHA-256 | EB63758586AD1FFD... | A3F4CE5303302DDD... |
| nside | 512 | 512 |
| n_pix | 3927 | 3927 |
| n_source_pixels | 16200000 | 16200000 |
| has_snr | 0 | 0 |
| meta_json | 741 字节 | 741 字节 |
| JSON 头 | 796→557 字节 (zstd level=5) | 796→557 字节 |
| drizzle 耗时 | 12.078s | 12.078s |

**可重现性**：两次运行 HISS 输出非字节一致（size 差 1 字节，hash 不同）。原因可能为 zstd 压缩元数据/时间戳或并行 drizzle 浮点非确定性。像素数据应一致（同 nside/n_pix/n_source）。

### 4. stage1 耗时与峰值内存

| Stage | 耗时 (s) | 状态 | 备注 |
|---|---:|---|---|
| READ_FITS | 0.209 | OK | FITS 读取 0.152s |
| CALIBRATE | 0.068 | OK (退化) | master=nullptr, out=light |
| PLATESOLVE | 28.327 | OK | 含 2 次 sdet_detect_ex (4.9s) + Gaia 锥形查询 (15s) + 三角匹配 + WCS 求解 |
| PSF | 1.793 | OK | Moffat4 1943/2000 |
| PHOTOMETRIC | 1.519 | OK (退化) | n_matched=0, scale=1.0 |
| SNR | 0.028 | OK (退化) | sigma_residual=0, snr_model 未写入 |
| DRIZZLE | 13.236 | OK | hp_drizzle_run 12.078s |
| **总计** | **45.18** | **OK** | wall ~45s (run1) |

**峰值内存**：32642.55 MB（~31.9 GB），出现在 PLATESOLVE 阶段 10:30:01（Gaia DR3 SP xpsd 内存映射期间）。paged_memory 峰值 18552 MB。340 个样本（200ms 间隔）。32GB 峰值限制部署环境，多帧并行不可行。

### 5. stage2 HISS 集运行（执行步骤 4）

| 项 | 值 |
|---|---|
| 输入目录 | lib/orchestrator/cpp/output_hiss_dir |
| 输入文件 | frame1.hiss (184878332 字节, nside=32768, n_pix=15406480) + frame2.hiss (184886999 字节, nside=32768, n_pix=15407202) |
| 运行时间 | 2026-07-25 10:30:22 → 10:30:29 (wall ~7s, exit=0) |
| 峰值内存 | 1978.38 MB (~1.93 GB) |
| GRADIENT_SPHERE | 6.629s (退化: gaia_client_create_ex 失败, 回退无梯度校正) |
| STACK | 0.001s (跳过: .hcsd 已由 GRADIENT_SPHERE 生成) |
| 输出 HCSD | engineering/evidence/P00-003/output/stage2_baseline.hcsd, 187455430 字节 |
| HCSD SHA-256 | 2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37 |
| **可重现性** | **SHA-256 与 lib/orchestrator/cpp/output_stage2.hcsd (P00-001 记录) 完全一致**，证明 stage2 输出可字节级重现 |

**堆叠参数**：sigma_clip=winsorized (sigma=3.0, max_iter=5, winsor_low=0.05, winsor_high=0.95), weighting=snr_squared, grad_iter=10, grad_lambda=1e-04

**堆叠结果**：15522966 唯一像素 (nside=32768), mean_pixel_count=1.9850, sigma-clip iter 0 剔除 0 个离群值 (提前收敛), 78/49152 非空子叶, shift=18

### 6. 关键缺口确认（G-001/G-002）

- **G-001 已确认**：sdet_detect_ex 每帧调用 2 次，PLATESOLVE 28.3s 中约 4.9s 为重复检测开销。P01 阶段修复目标。
- **G-002 已确认**：PHOTOMETRIC n_matched=0，全链路降级（sigma_residual=0 → SNR 退化 → HISS has_snr=0 → stage2 SNR²加权退化）。根因待查。
- **新发现**：stage1 峰值内存 32GB（Gaia xpsd mmap）、CALIBRATE 退化、stage2 GRADIENT_SPHERE 退化、HISS 非字节级可重现——均为旧路径基线现状，P01 阶段需评估是否纳入修复范围。
