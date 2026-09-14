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

**交付样本定义（P14-N-08，RQS V2-N-08，2026-09-15 修订）**：
帧级 SNR / 5-sigma 深度一律来自 **DATA-P1-SOURCES.sources 的全部测光有效源**
（flux>0 且 fwhm_px>0），**与性能开关 `psf.max_stars` 完全解耦** —— 旧实现取
`psf_params`（受 `psf.max_stars` 截断的最亮 ≤5000 颗）∩ sources，交付数值由
最亮子集决定且下游读不出截断（系统性偏乐观，P8 REPORT 遗留缺陷）。唯一允许影响
交付样本大小的开关是配置 `snr.max_sources`（默认 0 = 不限），生效即
`truncated=true`。

**样本真实性 provenance（p1_snr.json 帧块，P14-N-08 新增）**：

| 字段 | 定义 |
|---|---|
| snr_sample | 所用样本定义（字符串：全部测光有效 sources，与 psf.max_stars 解耦） |
| n_sources | 上游目录可用源总数（DATA-P1-SOURCES.sources 长度） |
| n_snr_available | 其中测光有效（flux>0 且 fwhm_px>0）的源数 |
| truncated | 交付样本是否被 `snr.max_sources` 上限截断（bool） |
| snr_max_sources | 生效的交付样本上限（0 = 不限） |
| psf_mode | 上游 PSF 拟合**真实模式**（"fast"/"precise"/"unavailable"，非字面量） |
| n_fit_input | 上游真正送入 Moffat4 拟合的星数（provenance，**不**影响本帧 SNR） |
| psf_fit_truncated | 上游拟合输入是否被 `psf.max_stars` 截断（provenance） |

**lib/core 接线（P8 越域补丁 → P14 落树）**：`p1_op_noise` / `p1_op_star_psf_impl`
位于 lib/core/src/module_adapters.cpp（P8 以补丁交付、P14 直接落树）。P14 同批新增
两个**测试专用直调钩子** `p1_op_star_psf_json` / `p1_op_noise_json`（生产注册表
不变），供 tests/unit/p1snr/p1snr_frame_parity_test.cpp 端到端 parity 锁直调。
