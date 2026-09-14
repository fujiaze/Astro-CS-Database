# phase1/noise — NoiseModel (L2 模块 README)

- 合同: `P1-005` / SCI-NOISE-001 (docs/contracts/INDEX.yaml)
- Header: `lib/phase1/noise/noise_model.h`
- Source: `lib/phase1/noise/noise_model.cpp`
- Test: `tests/unit/p1_noise_test.cpp` (6 组: blank sky/MC Poisson/低高信号/负值/gain 边界/ivar 不混)

## 职责
σ_bg = 1.4826·MAD → variance/ivar (1/variance)。floor=1e-12。
gain 诊断: variance = signal/gain + read_noise²/gain²; 零 gain/无效 read noise → 显式无效。
variance 与 ivar 显式不混。公式权威: SCI-NOISE-001。

## P8-SNR-LINUX 接线（2026-09-14，SNR 生产接线批）

**背景事实（改前）**：Linux 正式入口 astrocs 的 SNR 节点
（lib/core/src/module_adapters.cpp::p1_op_noise，module_adapters.cpp:2391）只把
整帧像素喂给 NoiseModel::estimate，并落盘 p1_snr.json 的
{variance, ivar, sigma, background, valid, reason} —— 即**整帧 sigma/MAD 噪声统计**，
既没有逐源 SNR，也没有 5-sigma 深度；P5-SNR 的正确实现
（lib/snr_estimator/cpp/src/snr_science.cpp）当时**不在**根 CMake 主图里。

**改后（本批）**：

| 路径 | 角色 |
|---|---|
| lib/phase1/noise/snr_frame_science.h | 帧级 SNR 聚合 API（SnrSourceRow / SnrFrameScienceConfig / SnrFrameScienceResult / compute_snr_frame_science） |
| lib/phase1/noise/snr_frame_science.cpp | 聚合实现；**零公式副本**，全部数值调 snr_science.cpp 的 C ABI |
| lib/snr_estimator/cpp/src/snr_science.cpp | 唯一权威科学实现（P5；已编入 astrocs_phase1_noise） |
| CMakeLists.txt（根）astrocs_phase1_noise | sources 增 snr_frame_science.cpp + snr_science.cpp（**仅** sources，未改 include/link/目标结构） |
| tests/unit/p1snr/p1snr_linux_test.cpp | 回归锁（链接**真实** astrocs_phase1_noise；接线缺失 → 构建期链接失败） |

**字段语义（p1_snr.json 帧块）**：

| 字段 | 定义 | 单位 |
|---|---|---|
| sources[].snr_f | SNR_F = F/sigma_F，sigma_F^-2 = sum_i P_i^2/sigma_i^2（Horne 1986 最优提取） | [1] |
| snr_phot | median(SNR_F)（**字段名冻结，语义已重定义**；旧 1/(ln10·sigma_residual) 退休） | [1] |
| median_snr / median_source_snr | = median(SNR_F)（三者同一逐位值） | [1] |
| local_snr | 相对质量权重场 SNR_F/median(SNR_F)（SCI-CW §4 quality_weight，**非**校准 SNR） | [1] |
| frame_depth_flux5_adu | F_5 = 5·sigma_F(ref)，参考轮廓 = 目录中位 FWHM + 帧 sigma_sky | [ADU] |
| frame_depth_m5_mag | m_5 = ZP − 2.5·log10(F_5)；无 ZP → null | [mag] |
| frame_snr | **对象**（5-sigma 深度 + 定义），**不是**整帧 SNR 标量 | — |
| sigma_location_se_dex / _mag | 1.253·sigma_logflux_dex/sqrt(N) 及其 ×2.5；无定标残差 → 0 + sigma_location_se_status | [dex] / [mag] |
| sigma / variance / ivar / background | 旧字段保留（整帧稳健噪声统计，**不是** SNR） | ADU / ADU² / ADU⁻² / ADU |

**禁止**：本路径不再输出任何「整帧 SNR 标量」；snr=1.0 不得作为 unknown 伪装
（退化行保持 NaN/null，n_snr_catalogue 显式计数）。

**SNR 目录成员** = 上游 p1_sources.json 的 psf_params（PSF 有效星，
p1_psf.json::n_valid 同源）∩ sources（提供 flux / fwhm_px）——
与 P5 控制点路径的有效星判据一致。

**lib/core 接线（越域，单独补丁）**：让 p1_op_noise 读入逐源目录并落盘新字段需要改
lib/core/src/module_adapters.cpp（P7 独占）。本批**未改仓库树中的 lib/core**，
改动以补丁交付：run/perf-fix/P8-snr-linux/patches/out-of-scope/lib-core-snr-wiring.patch
（+ module_adapters.cpp.patched 完整副本）；SNR 配置块 snr 的键注册同批以
cli-parser-snr-config-key.patch 交付（同属越域）。端到端验证通过用户命名空间内
bind-mount 这些副本完成（仓库文件逐字节未变，见 REPORT §6）。
