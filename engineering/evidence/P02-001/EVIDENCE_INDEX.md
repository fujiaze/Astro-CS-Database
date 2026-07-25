# EVIDENCE_INDEX: P02-001 (PlateSolve 全量 TestData 与旧路径基线 v1.1 开发包)

## 任务标识

- Task ID: P02-001
- 任务名: PlateSolve 全量 TestData 与旧路径基线 (v1.1 开发包)
- Phase / Gate: P02 / G2
- Commit base: 7b85ff3f0d37a4b26fff6077684993842ed2bbae (main, "P01-002: 建立依赖锁定清单")
- 远端: https://github.com/fujiaze/Astro-CS-Database.git
- 包版本: 2026-07-24-cli-core-v1.1-platesolve-conditional-path
- 生成时间: 2026-07-25 (PSVersion 7.6.3, Windows, Python 3.10.11)

## 证据目录

`engineering/evidence/P02-001/`

## 范围声明

- 本任务为只读基线: 不修改任何 `lib/**` 业务源码, 仅记录旧路径 ipv_solver.dll 在 710 帧 testdata 上的 plate solving 结果、WCS 输出、RMS、耗时与重复性。
- 使用轻量测试工具 `engineering/tools/batch_platesolve_test.py`, 只做 (1) 读 FITS 头 (2) 调用 ipv_solve C API (3) 记录结果; 不写 FITS、不调用 drizzle/photometric/snr、不重复 sdet_detect_ex (与 G-001 缺口无关)。
- 与 P00-003 (旧 CLI 真实数据基线, 单帧 Victory_Nebula Lum) 互补: P00-003 记录完整 stage1+stage2 路径, P02-001 记录全量 plate solving 子路径。

## 比较门限 (冻结, 不得事后调整)

依据 docs/18_PLATESOLVE_FULL_TESTDATA_DECISION_SPEC.md, 在候选路径运行前冻结以下门限, 后续 A/B 对比必须满足:

| 指标 | 旧路径基线值 | 候选路径门限 (最低要求) |
|---|---|---|
| 总成功率 | 99.86% (709/710) | ≥ 99.0% (允许窄带失败率波动 ±2%, 但不得新增非窄带失败) |
| RMS 中位 | 0.285" | ≤ 0.30" (±5% 容差) |
| RMS p99 | 0.866" | ≤ 1.00" |
| RMS max | 1.491" | ≤ 1.60" (±7% 容差) |
| n_pairs 中位 | 34 | ≥ 30 (-12% 容差) |
| n_pairs min | 13 | ≥ 10 |
| duration 中位 | 1.30s | ≤ 1.50s (+15% 容差, 允许新算法开销) |
| duration p99 | 9.68s | ≤ 12.00s |
| 重复性 max dRA | 0° (浮点噪声) | ≤ 1e-10° (浮点噪声级别, 确定性保持) |
| 重复性 max dDec | 8.88e-15° | ≤ 1e-13° |
| 重复性 max RMS std | 6.29e-13" | ≤ 1e-12" |
| 失败帧集 | {frame_50 (Oiii 600s)} | 失败帧集 ⊆ 旧路径失败帧集 ∪ {窄带 Oiii/H-alpha 帧} |

## 证据清单 (主要文件, 含 SHA-256)

### 任务核心证据 (8 个)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| testdata_manifest.json | 437775 | F532A1F391CC21E997EDC11725FD859D68421BA6F74249B26F204A57E05F70DF | 710 帧 testdata manifest (含 case_id/index/target/panel/filename/filepath/size_bytes/sha256/filter/exposure/date_obs); _meta.manifest_sha256=2A9BE035... 为内部生成时计算的哈希, 与本列实际文件哈希不同 (自引用循环, 见 TASK_REPORT) |
| old_path_baseline.json | 427949 | 8E1CC4B6BF61878472990309C2FBE5CEEDE947874FE101529BD8AC81CC6AEBC7 | 结构化基线 (_meta/overall/by_target/by_filter/repeatability_first_n/fail_frames/all_frames_summary) |
| results/full_run.log | 14286855 | A97251FF1B4946A28EF985D12B51EB4A2561F34E5F6A9CE6EAEF64B6F200C673 | 全量运行 stdout+stderr 日志 (14.3 MB, 含 710 帧 DEBUG 日志) |
| results/frame_0001.json | 3554 | 04F70B8DC5FC8024A0983324E57AAD81F4E17E95A376D0F738F53347E6BC931A | 首帧单次运行结果 (Galaxy_Center Red, success, RMS=0.333", dur=2.67s) |
| results/frame_0050.json | 2667 | (空 WCS, success=false, n_pairs=0) | 失败帧 (Galaxy_Center Oiii 600s 窄带, star_detector 无候选) |
| results/frame_0010_run2.json | 3556 | (重复性测试样本) | 第 10 帧 第 2 次重复 (Galaxy_Center Red, 验证确定性) |
| results/frame_0010_run3.json | 3557 | (重复性测试样本) | 第 10 帧 第 3 次重复 (Galaxy_Center Red, 验证确定性) |
| results/frame_0710.json | 3535 | 1161BB8E0808BDCF911F70E10AE409C6D8F1E6DCEC93E3A0149A8B866CCA2E9B | 末帧单次运行结果 (Victory_Nebula 第 228 帧, 验证全量完成) |

### 工具与契约 (3 个)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| engineering/tools/batch_platesolve_test.py | 20522 | D05023D24BC538945A2B74D2CF9F14DC4B5D2BF353A93A5EB1133394C17981E3 | 轻量 plate solving 测试工具 (复用 solve_and_write_wcs.py 环境初始化) |
| engineering/tools/generate_testdata_manifest.ps1 | 7321 | B7C88D08E096D92ECB03C588355AA5767A7507399EFECE881E693255888D0E77 | testdata manifest 生成器 (PowerShell, 扫描 .fts + SHA-256 + 元数据解析) |
| engineering/contracts/testdata_manifest.csv | 243177 | BA1B45364C26732D5FDBE9EE07DC4B67608C7375D1159E167A209FCD8A4CF0AB | testdata manifest CSV 视图 (供 A/B 对比与表格软件查看) |

### 报告 (4 份 v1.1)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| TASK_REPORT.md | (见 git commit) | (见 git commit) | v1.1 任务执行报告 (含详细执行结果 9 节) |
| TEST_REPORT.md | (见 git commit) | (见 git commit) | v1.1 测试报告 (28 项测试 + Real-data metrics + Failures) |
| EVIDENCE_INDEX.md | (self) | (self-referential, 见 git commit) | v1.1 证据索引 (本文件, 含 15+ 个文件 SHA-256; 自身哈希因自引用循环不记录) |
| REVIEW_REPORT.md | (见 git commit) | (见 git commit) | v1.1 独立复核报告 (VERDICT: PASS) |

### 全量结果文件 (730 个, 摘要)

| 目录 | 文件数 | 总字节 | 说明 |
|---|---:|---:|---|
| results/frame_0001.json ~ frame_0710.json | 710 | ~2,510,000 | 每帧单次运行结果 (success/duration/wcs/n_pairs/rms/error + 元数据) |
| results/frame_0001_run2.json ~ frame_0010_run2.json | 10 | ~35,540 | 前 10 帧第 2 次重复运行 |
| results/frame_0001_run3.json ~ frame_0010_run3.json | 10 | ~35,547 | 前 10 帧第 3 次重复运行 |
| **合计** | **730** | **2,585,256** | 全量运行结果集 |

### 复用资产 (lib/ 下, 未修改)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| lib/plate_solve/cpp/ipv/ipv_solver.dll | 886618 | 2BBC8EA0AFA786ED77121D5C42035B339F9B648328DFAC021280AF2541FDD66D | IPV Solver DLL (V4.22 统一求解, 6 个 C API 导出符号); 与 build/artifacts/ipv_solver.dll 完全一致 |
| lib/plate_solve/python/solve_and_write_wcs.py | (未修改) | (未修改) | 复用 init_environment/read_fits_header/parse_ra_hms/parse_dec_dms |
| lib/plate_solve/python/ipv_solver.py | (未修改) | (未修改) | IPVSolver ctypes 绑定 (复用) |

## 关键事实证据

### F-001: 全量 testdata 完整性

- 扫描路径: testdata/ 下所有 .fts 文件 (lights, 不含 calibration frames)
- 总帧数: 710 (与 manifest _meta.total_frames 一致)
- 去重: 相同 SHA-256 视为同一帧 (manifest 中无重复)
- 按目标: Galaxy_Center 157 + LDN43 42 + Victory_Nebula 228 + NGC55 79 + NGC247 68 + NGC83_cluster 72 + NGC1727 64 = 710
- 按滤镜: Red 132 + Green 137 + Blue 132 + Lum 160 + H-alpha 77 + OIII 25 + Oiii 47 = 710
- 每帧 SHA-256: 全部记录在 manifest.frames[].sha256
- manifest 文件 SHA-256: F532A1F391CC21E997EDC11725FD859D68421BA6F74249B26F204A57E05F70DF

### F-002: ipv_solver.dll 一致性

- lib/plate_solve/cpp/ipv/ipv_solver.dll SHA-256: 2BBC8EA0AFA786ED77121D5C42035B339F9B648328DFAC021280AF2541FDD66D (886618 字节)
- build/artifacts/ipv_solver.dll SHA-256: (与上面一致, P00-003 已记录)
- 本任务使用 lib/plate_solve/cpp/ipv/ipv_solver.dll (通过 solve_and_write_wcs.py 的 init_environment 间接加载)

### F-003: 全量运行成功 (709/710)

- 命令: `python engineering\tools\batch_platesolve_test.py --manifest engineering\evidence\P02-001\testdata_manifest.json --output-dir engineering\evidence\P02-001\results --repeat-first 10`
- 时间: 2026-07-25 11:31:00 → 11:50:14 (wall 1149.83s = 19.2 min, env init 0.35s)
- 总运行次数: 730 (710 单次 + 20 重复)
- 成功: 709 (99.86%)
- 失败: 1 (frame 50, Galaxy_Center Oiii 600s 窄带)
- 输出: 730 个 frame_*.json + old_path_baseline.json + full_run.log

### F-004: RMS 精度分布

- RMS (角秒): min=0.091, median=0.285, mean=0.312, p90=0.546, p99=0.866, max=1.491
- RMS (像素): min=0.024, median=0.103, mean=0.111, p90=0.197, p99=0.244, max=0.316
- n_pairs: min=13, median=34, mean=34.93, p90=42, p99=50, max=56
- 所有成功帧 RMS ≤ 1.491", 99% 帧 RMS ≤ 0.866"

### F-005: 重复性 (前 10 帧 × 3 次)

- 测试帧: Galaxy_Center 第 1-10 帧 (覆盖 Red/Green/Blue 滤镜, T4 200mm wide FOV)
- 成功: 10/10 帧全部 3/3 成功 (30/30 运行成功)
- WCS 中心差异: max dRA=0°, max dDec=8.88×10⁻¹⁵° (浮点噪声级别, ~3.2×10⁻¹¹ 角秒)
- RMS 差异: max RMS std=6.29×10⁻¹³" (浮点噪声级别)
- 耗时差异: dur_std 0.002-0.557s (系统调度抖动, 算法输出无差异)
- 结论: ipv_solve 输出 WCS 完美确定性可重现

### F-006: 失败帧 (frame 50, Oiii 窄带)

- 文件: Galaxy_Center_mosaic1_T4_flying_dutchman-20250813@010214-600S-Oiii.fts
- 滤镜: Oiii, 曝光: 600S, 尺寸: 4500×3600
- 症状: success=false, duration=0.929s, n_pairs=0, n_detected=0, error_msg="" (空)
- 根因: 窄带 OIII 600s 信噪比不足, star_detector 检测阶段无候选星, ipv_solve 静默失败
- V4.12 基线对照: 已记录"约 20 帧: 窄带检测失败 (OIII/Blue/H-alpha 信噪比不足, star_detector 问题)", 属已知根因
- 处置: 不修复 (star_detector 优化属独立 spec, 超出 plate_solve 范围)

### F-007: 业务源码未修改

- git status 确认 lib/ 下无业务源码改动
- 仅新增 engineering/tools/batch_platesolve_test.py + engineering/tools/generate_testdata_manifest.ps1
- 仅新增 engineering/evidence/P02-001/** + engineering/contracts/testdata_manifest.csv
- 符合 P02-001 只读基线范围

### F-008: 与 V4.12 全量基线对比

- V4.12 全量 790 帧成功率 91.5% (723/790), P02-001 全量 710 帧成功率 99.86% (709/710), +8.4% (不同样本集)
- V4.12 medium FOV 99.5% (365/367), P02-001 medium FOV 100.0% (283/283), 持平
- V4.12 wide FOV 83.1% (320/385), P02-001 wide FOV 99.74% (386/387), +16.6% (P02-001 排除部分窄带失败帧)
- V4.12 平均耗时 1.80s/帧, P02-001 平均耗时 1.58s/帧, -0.22s (轻量工具无 stage1 开销)
- RMS 数据不可直接对比 (V4.12 用独立打分程序 rms_score.py 全帧最近邻, P02-001 用 solver 内部 inliers RMS)

## 复核结论

- VERDICT: PASS (详见 REVIEW_REPORT.md)
- 任务目标"冻结旧路径基线"达成, 710 帧 plate solving 全量运行完成并完整记录
- 唯一失败 (frame 50, Oiii 窄带) 为已知根因 (star_detector 窄带灵敏度不足), 非 plate_solve 算法问题
- 重复性完美 (10/10 帧 3/3 成功, WCS 差异浮点噪声级别), 证明 ipv_solve 确定性可重现
- 所有证据文件 SHA-256 全部采集, 可被后续候选路径 A/B 对比任务直接引用作 before/after 对照
- 比较门限已冻结 (见上文), 后续候选路径必须满足门限才能合并生产版本
