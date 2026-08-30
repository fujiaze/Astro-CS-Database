# TEST-001 — 科学测试矩阵（V6）

> 状态: ACTIVE  版本: 1.0.0  owner: AstroCS
> 上游: SCI-001..003, DATA-001  下游: 各模块 P1-*/P2-*/P3-* 测试任务, CPU-006
> 规则: 每个 SCI/ALG ID 至少一个独立 TEST; 容差事前冻结; 影响触发按 07 矩阵。

## 1. 测试类型

| 类型 | 说明 | 适用 |
|---|---|---|
| analytic oracle | 解析解逐像素/逐元素比对 | calibration/drizzle/integration |
| high-precision reference | 高精度参考(FP64/独立实现) | UPM/rejection/PSF |
| property | 不变量/性质测试 | 常数场/守恒/确定性 |
| metamorphic | 输入变换不变量 | 平移/缩放/旋转 |
| boundary | 边界/极端/NaN/Inf/空 | 所有模块 |
| parallel/backend equivalence | 1/N worker、baseline/AVX 等价 | CPU-heavy 模块 |

## 2. 通用容差规则（事前冻结，禁止跑后调阈值）

- 元数据/mask/计数/索引/端口/选择结果: 精确一致。
- FP64 非归约: `rtol=1e-12, atol=1e-13×scale`。
- FP32 产品非归约: `rtol=5e-6, atol=1e-6×scale`。
- 归约: `γ_n = n·u/(1−n·u)`, 门限 `C·γ_n·Σ|terms| + atol`, `C≤4` 事前冻结。
- 并行归约 deterministic 用固定分块/树合并; 否则 reproducible-within-bound 文档化。
- NaN/Inf/missing 位置与语义精确一致, 禁止仅比较 finite 像素。
- 容差调整必须是独立 SCI/TEST commit, 附失败分布与推导。

## 3. 每 SCI/ALG 的 TEST 映射（当前冻结）

| SCI/ALG | TEST ID（现有） | Oracle 类型 | 容差 | 测试文件 |
|---|---|---|---|---|
| SCI-CAL-001 | TEST-CAL-001 | analytic | max_abs=0 / rtol=1e-6 | lib/calibration/tests/test_photometry_apply.cpp |
| SCI-WCS-001 | TEST-IPV-001 | high-precision | WCS roundtrip 1e-6px | lib/plate_solve/cpp/ipv/test/test_synthetic.cpp |
| SCI-PHOT-001 | TEST-PHOT-001 | analytic | flux rtol=1e-6 | lib/photometric_calib/... |
| SCI-PSF-001 | TEST-PSF-001 | high-precision | centroid/FWHM 门 | lib/dynamic_psf/... |
| SCI-NOISE-001 | TEST-NOISE-001..015 | analytic+MC | 固定 seed 统计界 | lib/snr_estimator/.../noise_model_science_test.cpp |
| SCI-DRZ-001/014 | TEST-DRZ-CAND-001 / TEST-DRZ-VAR-001 | analytic/property | false_negative=0; α²v | lib/healpix_db/.../candidate_oracle_test.cpp |
| SCI-UPM-001 | TEST-UPMW-001..007 | MC+property | k_corr≈1.3883; rtol=1e-9 | lib/phase2/tests/synthetic_gate.cpp |
| SCI-REJ-001 | TEST-REJ-001..008 | property+boundary | precision/recall 门 | lib/phase2/tests/... |
| SCI-INT-001 | TEST-INT-001 | analytic | mean/weighted mean 解析 | lib/phase2/tests/... |
| SCI-CW-001 | TEST-CW-001..008 | property | snr 扰动不变 | lib/phase2/tests/... |
| SCI-P3-001 | SYN-007 五件套 | analytic+property | WCS 1e-6px; 常数场 | tools/validation/phase3 (待建, P3-006) |

## 4. 影响触发（扩大验证条件）

| 变更类型 | 必须运行 |
|---|---|
| 普通内部重构 | 模块测试 + 自动计算 downstream |
| 科学定义/离散算法 | 全模块 oracle + 受影响链 |
| 数据合同/单位/坐标 | 全模块 + cross-stage |
| Pipeline 拓扑 | 全链 IR=trace |
| 编译器/平台/ISA/后端 | 双平台等价矩阵 |
| 模块测试发现跨阶段异常 | 全链路或真实数据 |

## 5. 触发判定

- `impact_analysis`（09 规范工具）从 git diff 计算受影响模块与 downstream；
  Agent 不可手工缩小集合，只可增加。

## 6. 验收

- 每个 ACTIVE SCI/ALG 在 TRACEABILITY.csv 有 TEST 映射（DOC-004 校验断链）；
- 全部容差事前冻结于本合同；无跑后调阈值。
