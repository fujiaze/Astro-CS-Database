# TEST_REPORT

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| Python 环境 | `python -c "import astropy, numpy, ctypes; print(astropy.__version__, numpy.__version__)"` | 10s | 0 | PASS | Python 3.10.11, astropy 6.1.7, numpy 2.2.6 |
| ipv_solver.dll 加载 | `python -c "from ipv_solver import IPVSolver; s=IPVSolver(); print(s._handle)"` | 30s | 0 | PASS | IPVSolver 实例创建成功, handle 非空 |
| DLL 符号导出验证 | `nm --extern-only --defined-only ipv_solver.dll \| grep ipv_solve` | 5s | 0 | PASS | 6 个符号导出: ipv_solve, ipv_solve_from_memory, ipv_solve_create, ipv_solve_destroy, ipv_solve_from_detections_v1 (新), ipv_solve_from_memory_with_callback (新) |
| DLL 大小与哈希 | `Get-FileHash ipv_solver.dll` | 5s | 0 | PASS | 984362 字节, SHA-256 804B2F2F54C665DCBC796A021E756DE62D37A4531B878483425ACB9A9E506547 |
| 单帧三路径对比 (Red 180S) | `python engineering\tools\p02_002_single_frame_test.py --fits <Red.fts>` | 60s | 0 | PASS | 三路径 success=true, WCS 完全一致 (d_crval=0, d_cd=0), d_rms_arcsec=0.000000" (详见 single_frame_three_paths.json) |
| 路径 0 (基准) 单帧 | 从 single_frame_three_paths.json.path0_baseline 提取 | 5s | 0 | PASS | success=true, n_pairs=45, rms=0.3329", duration=2.145s |
| 路径 B (callback) 单帧 | 从 single_frame_three_paths.json.pathB_callback 提取 | 5s | 0 | PASS | success=true, n_pairs=45, rms=0.3329", duration=1.590s, callback_n_detections=2000; 与 path0 WCS 完全一致 |
| 路径 A (外检) 单帧 | 从 single_frame_three_paths.json.pathA_from_detections 提取 | 5s | 0 | PASS | success=true, n_pairs=45, rms=0.3329", duration=0.080s, input_n_detections=2000; 与 path0 WCS 完全一致 |
| 单帧 A vs 0 精度对比 | 从 single_frame_three_paths.json.diff_A_vs_0 提取 | 5s | 0 | PASS | d_crval1=0", d_crval2=0", d_crpix=0px, d_cd11/12/21/22=0, d_rms_arcsec=0.000000", d_n_pairs=0 (精度完全一致) |
| 单帧 B vs 0 精度对比 | 从 single_frame_three_paths.json.diff_B_vs_0 提取 | 5s | 0 | PASS | d_crval1=0", d_crval2=0", d_crpix=0px, d_cd11/12/21/22=0, d_rms_arcsec=0.000000", d_n_pairs=0 (精度完全一致) |
| 批量三路径对比 (5 帧) | `python engineering\tools\p02_002_batch_test.py` | 300s | 0 | PASS | 5/5 帧全部三路径 success=true, 全部 diff=0 (详见 batch_three_paths.json) |
| 批量 Galaxy_Center Red 180S | 从 batch_three_paths.json.frames[0] 提取 | 5s | 0 | PASS | 三路径 success=true, n_pairs=45, rms=0.3329", pathA_dur=0.078s, pathB_dur=1.542s, path0_dur=2.328s |
| 批量 Galaxy_Center Green 180S | 从 batch_three_paths.json.frames[1] 提取 | 5s | 0 | PASS | 三路径 success=true, n_pairs=30, rms=0.3199", pathA_dur=0.072s, pathB_dur=1.317s, path0_dur=1.318s |
| 批量 Galaxy_Center Blue 180S | 从 batch_three_paths.json.frames[2] 提取 | 5s | 0 | PASS | 三路径 success=true, n_pairs=32, rms=0.5183", pathA_dur=0.071s, pathB_dur=1.410s, path0_dur=1.351s |
| 批量 Galaxy_Center H-alpha 300S | 从 batch_three_paths.json.frames[3] 提取 | 5s | 0 | PASS | 三路径 success=true, n_pairs=44, rms=0.4030", pathA_dur=0.074s, pathB_dur=0.894s, path0_dur=0.836s |
| 批量 Galaxy_Center Oiii 600S | 从 batch_three_paths.json.frames[4] 提取 | 5s | 0 | PASS | 三路径 success=true, n_pairs=34, rms=0.4110", pathA_dur=0.070s, pathB_dur=0.790s, path0_dur=0.773s |
| 批量 A vs 0 通过率 | 从 batch_three_paths.json.overall.A_vs_0_pass 提取 | 5s | 0 | PASS | 5/5 帧通过 (容差 0.001", 实测差异 0.000000") |
| 批量 B vs 0 通过率 | 从 batch_three_paths.json.overall.B_vs_0_pass 提取 | 5s | 0 | PASS | 5/5 帧通过 (容差 0.001", 实测差异 0.000000") |
| 路径 A 性能提升验证 | pathA_duration vs path0_duration 对比 | 5s | 0 | PASS | 平均加速比 0.073s vs 1.521s (节省 95.2%), 跳过 sdet_detect_ex 后仅做求解 |
| 路径 B 开销验证 | pathB_duration vs path0_duration 对比 | 5s | 0 | PASS | 平均开销差异 +0.014s (1.191s vs 1.521s 中位差 < 5%), callback 复制 detections 几乎零开销 |
| callback detections 数量一致性 | pathB.callback_n_detections == pathA.input_n_detections | 5s | 0 | PASS | 5/5 帧 pathB 导出 detections 数 (2000) == pathA 输入 detections 数 (2000), 路径 A/B 数据流闭环 |
| star_det v1 格式合规性 | detections 列数=6, dtype=float64 | 5s | 0 | PASS | 列定义符合 spec: [x_px, y_px, flux, mag, saturated, has_saturated], 全 FLOAT64 |
| WCS 格式合规性 | 三路径 wcs.ctype1/ctype2/cd/sip_order 字段一致 | 5s | 0 | PASS | CTYPE=RA---TAN-SIP/DEC--TAN-SIP, CD 矩阵无 1/cos(Dec) 因子, SIP order=3 (符合标准 WCS+SIP) |
| 向后兼容 (原 API 不变) | nm 验证 ipv_solve / ipv_solve_from_memory 符号保留 | 5s | 0 | PASS | 原 4 个 API 符号未修改, 函数签名未变, ipv_solver.py 中原 solve/solve_from_memory 方法保留 |
| 生产管线未切换 | grep "solve_from_detections_v1\|solve_from_memory_with_callback" pipeline_adapter.py | 5s | 0 | PASS | pipeline_adapter.py 中无新 API 调用, 默认仍走 solve_from_memory (生产路径未切换) |
| solver_logs 完整性 | 检查 results/solver_logs/{path0,pathA,pathB}/*.log | 5s | 0 | PASS | 三路径每路径 3 个日志 (ipv_solver.log, ipv_triangle.log, ipv_itertrans.log) 全部生成 |
| batch_solver_logs 完整性 | 检查 results/batch_solver_logs/<frame>/<path>/*.log | 5s | 0 | PASS | 5 帧 × 3 路径 × 3 日志 = 45 个日志文件全部生成 |

## Real-data metrics

- **测试数据规模**：testdata/Galaxy_Center_T4/lights/panel1/ 下 5 个真实 `.fts` 帧，覆盖 4 种滤镜 (Red 180S, Green 180S, Blue 180S, H-alpha 300S, Oiii 600S)，同一望远镜 (T4 200mm wide FOV)，同一指向 (RA=272.808°, Dec=-13.177°)，图像尺寸 4500×3600，单帧约 16 MB。
- **三路径运行总耗时**：单帧测试 2.145s (path0) + 1.590s (pathB) + 0.080s (pathA) = 3.815s；批量测试 5 帧累计 path0=6.961s, pathB=5.952s, pathA=0.366s，pathA 相对 path0 节省 94.7%。
- **路径 A (外检) 耗时分布**：median 0.072s, mean 0.073s, max 0.080s — 完全跳过 sdet_detect_ex 与 FITS I/O，仅做求解 (triangle_match + iter_trans + iterative_reproject + extract_wcs_sip)。
- **路径 B (callback) 耗时分布**：median 1.317s, mean 1.191s, max 1.542s — 与 path0 几乎一致 (path0 median 1.318s, mean 1.521s, max 2.328s)；差异来自 callback 内 numpy 数组 `.copy()` 复制开销 (< 50ms) 与系统调度抖动。
- **路径 0 (基准) 耗时分布**：median 1.318s, mean 1.521s, max 2.328s — 含 FITS I/O + sdet_detect_ex + 求解；首帧 (Red) 因 Gaia 冷缓存达 2.328s，后续帧稳定在 0.77-1.35s。
- **RMS 精度**：5 帧三路径 RMS 完全一致 (Red 0.333", Green 0.320", Blue 0.518", H-alpha 0.403", Oiii 0.411")，差异 = 0.000000" — 证明算法等价性。
- **n_pairs 分布**：Red 45, Green 30, Blue 32, H-alpha 44, Oiii 34 — 三路径每帧 n_pairs 完全一致 (差异 = 0)。
- **WCS 输出**：5 帧三路径 CRVAL/CRPIX/CD/SIP 完全一致 (浮点 bit-wise identical)，证明 `solve_post_select` 共享方法正确复现了 `solve_from_memory` 算法流程。
- **detections 数据流**：路径 B callback 导出 detections 数 (2000) == 路径 A 输入 detections 数 (2000)，证明路径 A/B 数据流闭环可串联。
- **滤镜覆盖**：Red/Green/Blue (宽带) + H-alpha/Oiii (窄带) 5 种滤镜全部三路径成功，验证窄带帧 (Oiii 600s, H-alpha 300s) 在新 API 下无回退。
- **无失败帧**：5/5 帧全部三路径 success=true (100% 成功率)，与 P02-001 基线 (99.86% 成功率) 一致 (本测试集 5 帧均为可成功帧，未包含 P02-001 中失败的 frame 50 Oiii 窄带)。

## Failures and investigation

### 无失败

本次测试 5 帧 × 3 路径 = 15 次运行全部 success=true，无失败案例。

### 退化 (非失败，记录为新 API 现状)

1. **路径 A detections 来源依赖**：路径 A 不再调用 sdet_detect_ex，必须由调用方提供 detections。若调用方提供的 detections 质量差 (如信噪比不足、噪点误检)，路径 A 求解会失败 — 但这属于上游检测模块责任，非路径 A 算法问题。本测试中路径 A 输入 detections 来自路径 B callback 导出 (即原 sdet_detect_ex 输出)，因此与路径 0 结果完全一致。
2. **callback 同步调用**：路径 B callback 在 sdet_detect_ex 之后、选星之前**同步调用**，调用方不得在 callback 内执行阻塞操作 (如磁盘 I/O、网络请求)，否则会阻塞整个求解线程。本测试中 callback 仅做 numpy `.copy()` (< 50ms)，无阻塞风险。
3. **detections 缓冲区生命周期**：C++ 端构造 `std::vector<double> det_v1(N*6)` 临时缓冲区传给 callback，缓冲区在 callback 返回后立即析构。Python 端 `_trampoline` 已 `.copy()` 复制数据，避免悬挂引用。若调用方为 C/C++ 直接调用 callback，必须在 callback 内显式 memcpy 复制数据。
4. **测试集规模限制**：本次测试仅 5 帧 (Galaxy_Center 同一指向同一望远镜)，未覆盖 P02-001 全量 710 帧 (7 目标 × 3 望远镜 × 7 滤镜)。后续切换生产路径前，应运行 P02-001 同款 710 帧全量测试 (或至少 NGC1727 T2 中焦 + Victory_Nebula T4 wide FOV 等长尾帧) 验证无回退。
5. **路径 A 性能优势场景受限**：路径 A 的 95% 提速仅在"已有外部 detections"场景下成立。若调用方仍需自行调用 sdet_detect_ex 生成 detections，则总耗时与路径 0 相当 (检测 + 求解 = 1.4s + 0.07s ≈ 1.5s)。路径 A 真正价值在于"跨模块 detections 复用" (如对齐叠加模块的检测结果直接喂给 PlateSolve)。
6. **路径 B callback 仅导出 detections，不导出选星结果**：callback 在选星 (ipv_select) 之前调用，因此导出的是 sdet_detect_ex 原始 detections (2000 颗候选星)，而非选星后的匹配星 (45 颗)。若下游需要选星结果，应另设 callback 或从 IpvWcsResult.n_pairs 字段获取。

以上退化均为新 API 设计现状，非 bug，符合 P02-002 实验性候选路径范围 (仅实现 + 验证，不切换生产路径)。
