# TEST_REPORT

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| orchestrator.exe 双副本一致性 | `Get-FileHash build/artifacts/orchestrator.exe; Get-FileHash lib/orchestrator/cpp/orchestrator.exe` | 30s | 0 | PASS | 两者 SHA-256 均 04704F1B8687905E8EBF1175482137C419229CB0456426A677BF820F69021385, 完全一致 |
| 输入 FITS 完整性 | `Get-FileHash testdata/Victory_Nebula_T4_Flying_Dutchman/lights/Victory_Nebula_mosaic1_flying_dutchman-20250204@035646-180S-Lum.fts` | 30s | 0 | PASS | 32405760 字节, SHA-256 E43B88A4BDD8C9304560A6A26952A5C30D7A022CF5DCDD9F641C2C50DF373AE5 |
| stage1 run1 (build/artifacts EXE) | `.\build\artifacts\orchestrator.exe stage1 --frame <fits> --output <hiss> --config <json> --log-level DEBUG` | 900s | 3 | **FAIL (预期外)** | init_dlls 自动推导项目根目录为 `F:\Astro dev` (父目录), 9 个 DLL 全部加载失败, error_msg="[stage1] PipelineFrame 函数指针获取失败" |
| stage1 run1 修复 (lib/orchestrator/cpp EXE) | `.\lib\orchestrator\cpp\orchestrator.exe stage1 --frame <fits> --output <hiss> --config <json> --log-level DEBUG` | 900s | 0 | PASS | 7 个 stage 全部成功, wall 45s, HISS 47693 字节写入; 自动推导正确识别项目根 `F:\Astro dev\Astro CS Normalization Database` |
| stage1 run2 (内存监控) | 同 run1, 输出 stage1_baseline_mem.hiss, 并行 Get-Process 采样 | 900s | 0 | PASS | wall 74s (含监控开销), 峰值 WorkingSet 32642.55 MB (PLATESOLVE Gaia 锥形查询), HISS 47692 字节 |
| sdet_detect_ex 调用次数 | `Select-String "sdet_detect_ex start" stage1_run.err.log` | 10s | 0 | PASS | 2 次调用: 行 124 (ipv_select_from_memory) + 行 295 (orchestrator PLATESOLVE 后置), 与 G-001 缺口一致 |
| uint16 转换次数 | `Select-String "uint16→float" stage1_run.err.log` + 源码审查 | 10s | 0 | PASS | float32→uint16 全图转换 2 次 (ipv_select.cpp:944 + orchestrator.cpp:1429, 各 16.2M 像素), sdet 内部 uint16→float 反向转换 2 次 |
| stage1 stage 耗时 | 从 stdout JSON timings 字段提取 | 5s | 0 | PASS | READ_FITS 0.209s + CALIBRATE 0.068s + PLATESOLVE 28.327s + PSF 1.793s + PHOTOMETRIC 1.519s + SNR 0.028s + DRIZZLE 13.236s = 45.18s |
| WCS 字段 | 从 stderr 日志提取 `[PLATESOLVE] WCS 已写入` + `extract_wcs_sip` 行 | 10s | 0 | PASS | CRVAL=(187.545904,-78.817030), CRPIX=(2250.5,1800.5), CD=[-1.752e-03,3.781e-05;-3.714e-05,-1.751e-03], SIP order=3/3/3/3, rms=0.3975", n_pairs=32 |
| PSF 结果 | 从 stderr 日志提取 `[PSF] 完成` 行 | 10s | 0 | PASS | Moffat4, 1943/2000 (97%) 成功, 28 个 Background constraint violated 警告 |
| 测光结果 | 从 stderr 日志提取 `[PHOTOMETRIC] 完成` 行 | 10s | 0 | PASS (退化) | n_matched=0, scale=1.0, sigma_residual=0.0; FILTER Lum→Baader UV/IR Cut, n_gaia=10197, F_syn=9981 |
| SNR 结果 | 从 stderr 日志提取 `[SNR]` 行 | 10s | 0 | PASS (退化) | sigma_residual=0<=0 触发退化, snr_model 块未写入 |
| HISS inspect | `Get-Item; Get-FileHash` stage1_baseline.hiss + stage1_baseline_mem.hiss | 30s | 0 | PASS | run1: 47693 字节 EB637585...; run2: 47692 字节 A3F4CE53...; nside=512, n_pix=3927, has_snr=0 |
| HISS 可重现性 | 对比 run1/run2 HISS hash | 5s | 0 | **PARTIAL** | 非字节级一致 (size 差 1 字节, hash 不同), 像素数据应一致; HCSD 则完全可重现 |
| stage1 峰值内存 | 从 stage1_memory.log 提取 max(working_set_mb) | 5s | 0 | PASS | 峰值 32642.55 MB @ 10:30:01 (PLATESOLVE), paged_memory 峰值 18552.37 MB, 340 样本 |
| stage2 运行 | `.\lib\orchestrator\cpp\orchestrator.exe stage2 --frames lib/orchestrator/cpp/output_hiss_dir --output <hcsd> --config stage2_config.json --log-level DEBUG` | 600s | 0 | PASS | 2 stage 完成, wall 7s, HCSD 187455430 字节写入 |
| stage2 stage 耗时 | 从 stdout JSON timings 字段提取 | 5s | 0 | PASS | GRADIENT_SPHERE 6.629s (退化: gaia_client_create_ex 失败) + STACK 0.001s (跳过) = 6.63s |
| stage2 峰值内存 | 从 stage2_memory.log 提取 max(working_set_mb) | 5s | 0 | PASS | 峰值 1978.38 MB, 30 样本 |
| HCSD 完整性 + 可重现性 | `Get-FileHash output/stage2_baseline.hcsd` 与 P00-001 记录对比 | 60s | 0 | PASS | 187455430 字节, SHA-256 2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37, 与 lib/orchestrator/cpp/output_stage2.hcsd (P00-001 记录) 完全一致 |
| 业务源码未修改检查 | git status 中 lib/ 改动仅 Makefile/build.ps1 | 10s | 0 | PASS | 仅 lib/gaia_xpsd_client/Makefile 与 lib/plate_solve/cpp/ipv/build.ps1 因 v1.1 迁移被改 (非本任务), 无 .cpp/.c/.h 业务源码改动 |
| stage1_config 字段修正 | Read stage1_config_p00-003.json | 5s | 0 | PASS | calibration_dir=testdata/T4 calibration files, filter=Lum, output_root=engineering/evidence/P00-003/output, log_level=DEBUG (注: CALIBRATE 骨架未解析此字段) |

## Real-data metrics

- **输入真实帧**：Victory_Nebula_mosaic1_flying_dutchman-20250204@035646-180S-Lum.fts，4500x3600 像素，BITPIX=16，180s Lum 曝光，T4 望远镜 (focal=200mm, pixel=6um)，目标 RA=187.5°/Dec=-78.85°。FITS 文件 32405760 字节，SHA-256 E43B88A4...。
- **stage1 真实数据耗时分布**：PLATESOLVE 占 62.7% (28.3s/45.2s)，DRIZZLE 占 29.3% (13.2s/45.2s)，其余 5 个 stage 占 8.0%。PLATESOLVE 内 Gaia 锥形查询约 15s (ipv_select Step 6, 10:21:20→10:21:35)，sdet_detect_ex 2 次共 4.9s (2.227+2.671)，triangle_match 约 0.9s，iter_trans_solve+iterative_reproject 约 2.0s，robust_refine 0.9s，extract_wcs_sip 0.4s。
- **星点检测真实数据**：每帧检测 2000 颗星（饱和 1094 + 正常 15233，去重排序后取前 2000），候选 17337，Moffat4 拟合 17145/17337 (98.9%)，FWHM med=1.4637 mad=0.2873。三角匹配 vote_matrix 60x60，max_vote=663，top_pairs=60，最终 n_inliers=39 (rms=0.7382")，robust_refine 后 n_matched=32 (rms=0.3975")。
- **Gaia 锥形查询真实数据**：PLATESOLVE 阶段查询 FOV 半径 5.45° (n_target=60)，PHOTOMETRIC 阶段查询 FOV 半径 6.058° (mag 6.0-12.0 自适应)，返回 10197 颗光谱星，F_syn 计算 9981/10197 有效。gaia_cat 块写入 791413 颗星 (FOV 半径 6.058°，18.2 MB)。
- **WCS 求解精度**：rms_arcsec=0.3975" (0.0642 px)，n_pairs=32，trans_order=3，SIP order=3。指向修正 Δra=0.046° Δdec=0.033° (从 OBJCTRA/OBJCTDEC 初始指向到求解中心)。
- **stage1 峰值内存**：32642.55 MB (~31.9 GB)，出现在 PLATESOLVE Gaia 锥形查询期间 (10:30:01)，源于 Gaia DR3 SP 20 个 xpsd 文件 (63.5 GB) 的内存映射。paged_memory 峰值 18552 MB。32 GB 峰值限制部署环境。
- **HISS 输出真实规模**：nside=512 (1x_to_2x_drizzle 策略，pixel_scale=6.307"/px)，3927 个 HEALPix 像素覆盖 FOV (16200000 源像素→3927)，has_snr=0 (SNR 退化)。JSON 头 796→557 字节 (zstd level=5)，meta_json 741 字节。文件仅 47693 字节 (稀疏覆盖)。
- **stage2 真实数据**：2 个 HISS 输入 (frame1/frame2, 各 ~184 MB, nside=32768, ~15.4M 像素)，输出 HCSD 187455430 字节 (~178.7 MB)，15522966 唯一像素 (nside=32768)，mean_pixel_count=1.9850 (2 帧覆盖)。sigma-clip iter 0 无剔除 (提前收敛)。78/49152 非空子叶，shift=18。
- **stage2 可重现性**：HCSD SHA-256 2A9BD12E... 与 P00-001 记录的 lib/orchestrator/cpp/output_stage2.hcsd 完全一致，证明 stage2 在相同 HISS 输入下输出字节级可重现。
- **stage2 峰值内存**：1978.38 MB (~1.93 GB)，30 个样本 (200ms 间隔)，远低于 stage1 峰值 (无 Gaia xpsd mmap)。

## Failures and investigation

### 失败 1: stage1 run1 使用 build/artifacts/orchestrator.exe 失败 (exit=3)

- **症状**：orchestrator stage1 立即退出，exit_code=3，error_msg="[stage1] PipelineFrame 函数指针获取失败"
- **根因**：`init_dlls` 自动推导项目根目录逻辑 (orchestrator.cpp:620-637) 假设 EXE 位于 `<root>/lib/orchestrator/cpp/orchestrator.exe` (4 级深度)，对 EXE 路径向上取 4 级分隔符。但 `build/artifacts/orchestrator.exe` 仅 2 级深度，向上 4 级得到 `F:\Astro dev` (项目父目录)，导致 9 个 DLL 路径全部错误 (`F:\Astro dev/lib/astro_image_io/astro_image_io.dll` 不存在)。
- **修复**：改用 `lib/orchestrator/cpp/orchestrator.exe` (SHA-256 与 build/artifacts 副本完全一致 04704F1B...)，自动推导正确识别项目根 `F:\Astro dev\Astro CS Normalization Database`。
- **性质**：orchestrator 既存缺陷 (EXE 部署路径与自动推导假设不一致)，非本任务引入。记录为基线现状，建议 P01 阶段修复 (支持 `--lib-base-dir` CLI 参数或环境变量覆盖)。
- **影响**：任务描述中 `build/artifacts/orchestrator.exe` 作为入口的指引需修正为 `lib/orchestrator/cpp/orchestrator.exe`。

### 退化 (非失败，记录为基线现状)

1. **CALIBRATE 退化**：orchestrator.cpp:759 骨架未解析 config_json 中的 calibration_dir，master_dark/flat/bias 全传 nullptr，out=light (跳过校准)。本基线在不校准的 light 上运行。
2. **PHOTOMETRIC 退化 (G-002)**：n_matched=0 (KD-tree star_matcher 返回 0 对)，scale=1.0, sigma_residual=0.0。根因待查 (疑似 PSF 星点坐标与 Gaia 星表 WCS 转换或匹配半径配置问题)。
3. **SNR 退化**：sigma_residual=0<=0 触发退化，snr_model 块未写入，HISS has_snr=0。
4. **stage2 GRADIENT_SPHERE 退化**：gaia_client_create_ex 失败 (gradient_sampler.cpp 内)，回退无梯度校正模式，直接进入 hp_stack_hiss。
5. **stage2 STACK 跳过**：GRADIENT_SPHERE 已生成 .hcsd，STACK 阶段检测输出已存在直接跳过 (0.001s)，设计行为。
6. **HISS 非字节级可重现**：同一 FITS 两次运行 stage1，HISS hash 不同 (size 差 1 字节)。原因可能为 zstd 压缩元数据/时间戳或并行 drizzle 浮点非确定性。HCSD 则完全可重现。

以上退化均为旧路径既存状态，本任务仅记录不修复，符合 P00-003 只读基线范围。
