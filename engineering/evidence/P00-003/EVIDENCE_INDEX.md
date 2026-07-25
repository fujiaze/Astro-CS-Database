# EVIDENCE_INDEX: P00-003 (v1.1 旧 CLI 真实数据基线)

## 任务标识
- Task ID: P00-003
- 任务名: 旧 CLI 真实数据基线 (v1.1 开发包)
- Phase / Gate: P00 / G0
- Commit base: 7b85ff3f0d37a4b26fff6077684993842ed2bbae (main, "P01-002: 建立依赖锁定清单")
- 远端: https://github.com/fujiaze/Astro-CS-Database.git
- 包版本: 2026-07-24-cli-core-v1.1-platesolve-conditional-path
- 生成时间: 2026-07-25 (PSVersion 7.6.3, Windows)

## 证据目录
`engineering/evidence/P00-003/`

## 范围声明
- 本任务为只读基线: 不修改任何 `lib/**` 业务源码, 仅记录旧 Stage1/Stage2 路径在真实 Victory_Nebula Lum 帧上的运行结果、调用次数、耗时、内存与输出。
- 旧 v1.0 P00-003 证据 (healpix_stack 源码恢复) 保留原位, 作历史参考, 向后兼容。
- v1.1 报告 (TASK_REPORT / TEST_REPORT / REVIEW_REPORT / EVIDENCE_INDEX / old_cli_baseline.json) 覆盖 v1.0 视图。

## 证据清单 (19 个文件, 含 SHA-256)

### v1.1 当前任务证据 (17 个)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| stage1_config_p00-003.json | 1147 | E30B3DEBABA3110099AD77C207A52C2E7A77D69B6D65BAAF141DAFD85C5B1F31 | stage1 配置覆盖 (calibration_dir/filter/output_root/log_level) |
| stage1_run.log | 981 | E2B37933B1C539237048D8314EA923FC38E79575C24E4F32417C0BE45202D2A7 | stage1 run1 stdout JSON 结果 (7 stage timings, success=true) |
| stage1_run.err.log | 40533 | 0D7505E5A92455B7375A0BE1F617B956B38D1638F04BBB3CC31C829697CC2288 | stage1 run1 stderr DEBUG 日志 (445 行, 含完整 PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE) |
| stage1_run_mem.log | 984 | 5C4185B910646F983E93E62B6245B13DAD273F73854D80143DD8F9848CD990DB | stage1 run2 stdout JSON 结果 (内存监控运行) |
| stage1_run_mem.err.log | 40565 | 6B3C46112CF80883046F64D5B6D61BAC8318DF6D1757CBDAB7C24EDF7F07854E | stage1 run2 stderr DEBUG 日志 |
| stage1_memory.log | 12417 | 679FE5035F2B0D418DAA9BB6BAA2AD8CEC218FDF2F163493F680E30150BA74DA | stage1 内存采样 (340 样本, 200ms 间隔, 峰值 32642.55 MB) |
| stage2_run.log | 442 | 37B4AEACD2D1BFD733314BC450BCCA1C801312A63A4643CDC4B576880B46EC93 | stage2 stdout JSON 结果 (2 stage timings, success=true) |
| stage2_run.err.log | 9111 | 2CF1986F95AC21B4F8E872813F8FE6FF9CD0A4877A79003DBC481C1741468F95 | stage2 stderr DEBUG 日志 (123 行, 含 GRADIENT_SPHERE/STACK) |
| stage2_memory.log | 1121 | B1B991AC13E38AED53E6153E9CF745D39671825D1075E7AB6299E111A56C2AB3 | stage2 内存采样 (30 样本, 峰值 1978.38 MB) |
| output/stage1_baseline.hiss | 47693 | EB63758586AD1FFDF8FB4139301E2B345AD045188190E8AB9AD3E847349F98F9 | stage1 HISS 输出 run1 (nside=512, n_pix=3927, has_snr=0) |
| output/stage1_baseline_mem.hiss | 47692 | A3F4CE5303302DDDCEF0A490EC5294C1F8C0E757D0FEBE9DEB03E20817069036 | stage1 HISS 输出 run2 (内存监控运行, 与 run1 非字节一致, size 差 1 字节) |
| output/stage2_baseline.hcsd | 187455430 | 2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37 | stage2 HCSD 输出 (nside=32768, n_pix=15522966, 与 P00-001 旧 output_stage2.hcsd 字节一致) |
| old_cli_baseline.json | 15187 | 74B2CBE51C0E87FEC8519204B518F9737E72037D860DB357379FCADCECA27239 | 结构化基线 (350 行, _meta/scope/input_fits/stage1/stage2/key_findings/evidence_files) |
| TASK_REPORT.md | 13012 | EDAED5A00CF4C8722950AB410D0CF1313C949F22FF7EE6E4FB3683835D985D50 | v1.1 任务执行报告 |
| TEST_REPORT.md | 9988 | FD5EF49F800752CCAC09161FAE701A5B9DDACF70C0AD386CC99511FFB596CF56 | v1.1 测试报告 (20 项测试 + Real-data metrics + Failures) |
| REVIEW_REPORT.md | 7262 | DFA5466420648AC7D8FFD7084657A4CEF241E61F80ED85962499033F642ABC86 | v1.1 独立复核报告 (VERDICT: PASS) |
| EVIDENCE_INDEX.md | (self) | (self-referential, 见 git commit) | v1.1 证据索引 (本文件, 含 19 个文件 SHA-256; 自身哈希因自引用循环不记录) |

### v1.0 历史证据 (2 个, 保留作向后兼容)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| SOURCE_RECORD.md | 1022 | 1C3DD5BCD7717056860DBD23950C06441C3E9498ABCE0D3106FB5B1F0EC71EA0 | v1.0 P00-003 (healpix_stack 源码恢复) 来源记录, 保留作历史参考 |
| commit_msg.txt | 491 | 6E85243297F2AA13A9A528E0A61E750CB175CB0D216CBDCFC9DAA112F628682A | v1.0 P00-003 (healpix_stack 源码恢复) commit message, 保留作历史参考 |

## 关键事实证据

### F-001: 输入真实帧完整性
- FITS: testdata/Victory_Nebula_T4_Flying_Dutchman/lights/Victory_Nebula_mosaic1_flying_dutchman-20250204@035646-180S-Lum.fts
- 大小: 32405760 字节
- SHA-256: E43B88A4BDD8C9304560A6A26952A5C30D7A022CF5DCDD9F641C2C50DF373AE5
- 尺寸: 4500x3600, BITPIX=16, BZERO=32768, 68 关键字
- 滤镜/曝光: Lum / 180.0s / 2025-02-04T03:57:02
- 初始指向: OBJCTRA=12 30 00.00 (187.5°), OBJCTDEC=-78 51 00.0 (-78.85°)
- 望远镜: T4 (focal_length=200mm, pixel_size=6um)

### F-002: orchestrator.exe 双副本一致性
- build/artifacts/orchestrator.exe 与 lib/orchestrator/cpp/orchestrator.exe SHA-256 完全一致
- 哈希: 04704F1B8687905E8EBF1175482137C419229CB0456426A677BF820F69021385
- 本任务使用 lib/orchestrator/cpp/orchestrator.exe (因 build/artifacts 路径推导缺陷, 见 F-008)

### F-003: stage1 旧路径成功运行 (run1)
- 命令: orchestrator stage1 --frame <fits> --output <hiss> --config stage1_config_p00-003.json --log-level DEBUG
- 时间: 2026-07-25 10:21:17 → 10:22:02 (wall ~45s, exit=0)
- 7 stage 全部成功: READ_FITS 0.209s + CALIBRATE 0.068s + PLATESOLVE 28.327s + PSF 1.793s + PHOTOMETRIC 1.519s + SNR 0.028s + DRIZZLE 13.236s = 45.18s
- HISS 输出: 47693 字节, SHA-256 EB637585...

### F-004: stage1 内存监控运行 (run2)
- 时间: 2026-07-25 10:29:21 → 10:30:35 (wall ~74s, exit=0, 含监控开销)
- 峰值 WorkingSet: 32642.55 MB @ 10:30:01 (PLATESOLVE Gaia 锥形查询期间)
- 峰值 paged_memory: 18552.37 MB
- 340 个内存样本 (200ms 间隔)
- 32GB 峰值源于 Gaia DR3 SP 20 个 xpsd 文件 (63.5GB) 内存映射
- HISS 输出: 47692 字节, SHA-256 A3F4CE53... (与 run1 非字节一致)

### F-005: sdet_detect_ex 重复调用 (G-001 缺口确认)
- 每帧调用 2 次:
  - 调用 1: lib/plate_solve/cpp/ipv/src/ipv_select.cpp:962 (ipv_select_from_memory 内, 2.227s)
  - 调用 2: lib/orchestrator/cpp/src/orchestrator.cpp:1445 (PLATESOLVE 后置, 2.671s)
- 两次结果完全一致 (2000 颗星, 饱和 1094 + 正常 15233, FWHM med=1.4637)
- PLATESOLVE 28.3s 中约 4.9s 来自重复检测
- 全图 float32→uint16 转换 2 次 (ipv_select.cpp:944 + orchestrator.cpp:1429, 各 16.2M 像素)
- sdet 内部 uint16→float 反向转换 2 次 (35.6ms + 57.7ms)

### F-006: PHOTOMETRIC n_matched=0 (G-002 缺口确认)
- KD-tree star_matcher 返回 0 对匹配
- scale=1.0, sigma_residual=0.0
- 全链路降级: sigma_residual=0 → SNR 退化 (snr_model 块未写入) → HISS has_snr=0 → stage2 SNR²加权退化为等权
- Gaia 锥形查询: ra=187.5459, dec=-78.8170, r=6.058°, mag 6.0-12.0 (自适应), n_gaia=10197, F_syn 有效 9981

### F-007: stage2 旧路径成功运行
- 命令: orchestrator stage2 --frames lib/orchestrator/cpp/output_hiss_dir --output <hcsd> --config stage2_config.json --log-level DEBUG
- 输入: frame1.hiss (184878332 字节, nside=32768, n_pix=15406480) + frame2.hiss (184886999 字节, nside=32768, n_pix=15407202)
- 时间: 2026-07-25 10:30:22 → 10:30:29 (wall ~7s, exit=0)
- GRADIENT_SPHERE 6.629s (退化: gaia_client_create_ex 失败, 回退无梯度校正) + STACK 0.001s (跳过: .hcsd 已生成)
- 峰值内存: 1978.38 MB (30 样本)
- HCSD 输出: 187455430 字节, SHA-256 2A9BD12E...
- 15522966 唯一像素 (nside=32768), mean_pixel_count=1.9850, sigma-clip iter 0 无剔除 (提前收敛)

### F-008: orchestrator EXE 路径推导缺陷 (既存, 非本任务引入)
- 症状: build/artifacts/orchestrator.exe 运行 stage1 立即退出 (exit=3, "[stage1] PipelineFrame 函数指针获取失败")
- 根因: init_dlls 自动推导项目根目录假设 EXE 位于 <root>/lib/orchestrator/cpp/ (4 级深度), build/artifacts/ 仅 2 级深度, 向上 4 级得到 F:\Astro dev (父目录), 9 个 DLL 全部加载失败
- 修复: 改用 lib/orchestrator/cpp/orchestrator.exe (SHA-256 与 build/artifacts 一致), 自动推导正确识别项目根
- 建议: P01 阶段支持 --lib-base-dir CLI 参数或环境变量覆盖

### F-009: HCSD 输出字节级可重现, HISS 非字节级可重现
- HCSD (stage2): SHA-256 2A9BD12E... 与 lib/orchestrator/cpp/output_stage2.hcsd (P00-001 记录) 完全一致, 证明 stage2 输出可字节级重现
- HISS (stage1): 同一 FITS 两次运行 hash 不同 (run1 EB637585... / run2 A3F4CE53..., size 差 1 字节)
- HISS 非可重现原因假设: zstd 压缩元数据/时间戳或并行 drizzle 浮点非确定性 (像素数据应一致, 同 nside/n_pix/n_source)

### F-010: 业务源码未修改
- git status 确认 lib/ 下仅 Makefile/build.ps1 因 v1.1 迁移被改 (非本任务产生)
- 无 .cpp/.c/.h 业务源码改动
- 符合 P00-003 只读基线范围

### F-011: WCS 求解质量
- CTYPE1/CTYPE2: RA---TAN-SIP / DEC--TAN-SIP
- CRVAL1/CRVAL2: 187.545904° / -78.817030°
- CRPIX1/CRPIX2: 2250.5 / 1800.5
- CD: [-1.751583e-03, 3.780697e-05; -3.714201e-05, -1.751466e-03], det=3.069e-06
- SIP A/B/AP/BP order: 3/3/3/3
- n_pairs=32, rms_px=0.0642, rms_arcsec=0.3975, trans_order=3
- 指向修正: Δra=0.045904°, Δdec=0.032970° (初始 187.5/-78.85 → 求解 187.546/-78.817)
- 像素比例: s0=6.1879"/px, CD 推导=6.307"/px, FOV 对角=9.9056°

### F-012: PSF 拟合质量
- 方法: Moffat4 (dpsf_fit_batch, fitRadius=8, maxIter=100)
- 输入 2000 星, 拟合成功 1943/2000 (97%)
- 警告: 28 个 Background constraint violated + 1 个 Invalid fit params
- 输出 psf 块: 2000 星, 144000 字节 (72 字节/星)

### F-013: 既存退化路径 (本任务仅记录, 不修复)
1. CALIBRATE 退化: orchestrator.cpp:759 骨架未解析 calibration_dir, master_dark/flat/bias 全传 nullptr, out=light
2. PHOTOMETRIC 退化 (G-002): n_matched=0, scale=1.0, sigma_residual=0.0
3. SNR 退化: sigma_residual=0<=0, snr_model 块未写入, HISS has_snr=0
4. stage2 GRADIENT_SPHERE 退化: gaia_client_create_ex 失败, 回退无梯度校正
5. stage2 STACK 跳过: GRADIENT_SPHERE 已生成 .hcsd, STACK 检测输出已存在直接跳过 (设计行为)

## 复核结论
- VERDICT: PASS (详见 REVIEW_REPORT.md)
- 任务目标"记录基线"达成, stage1/stage2 均成功运行并完整记录
- 所有失败/退化均为旧路径既存状态, 本任务仅识别不修复, 符合 P00-003 只读基线范围
- 18 个证据文件 SHA-256 全部采集, 可被后续 P01 优化任务 (G-001/G-002 修复) 直接引用作 before/after 对照
