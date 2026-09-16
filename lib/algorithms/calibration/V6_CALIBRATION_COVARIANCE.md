# V6 Phase1 校准 covariance（ALG-P1-CAL-COV-001）实现说明

> 任务：`IMPL-P1-CAL-001`（write_scope = `lib/algorithms/calibration/`, `tests/unit/v6_p1_cal/`）。
> 状态：实现完成，**未接线 Phase session**（`lib/phase1/`、`lib/phase1_session/` 未改）。
> 生产入口：`lib/algorithms/calibration/src/v6_calibration_covariance.cpp`；公共头：
> `lib/algorithms/calibration/include/astrocs/calibration/v6_calibration_covariance.h`。

## 1. 权威锚（冻结，逐条符合）

| 条款 | 本实现落点 |
|---|---|
| `ALG-P1-001 §2.1` 信号 | `calibrate_pixel`：`dark_opt=0 → y=(r−d)/max(f,0.1)`；`dark_opt=1 → y=(r−b−α(d−b))/max(f,0.1)`，`α=t_light/t_dark`；无 flat 不除法 |
| `ALG-P1-001 §2.2` 线性化 | `J=[1/f,−(1−α)/f,−α/f,−y/f]`；`C_cal=J C_in J`；逐像素 `v_cal=J C_in Jᵀ` |
| `FZ-FORMULA-COV-PROP` | `v_cal` 只由实际系数 `J` 与输入方差二次型得到；`CovarianceRecord.propagation="C_out = R C_in R^T"`，`variance_from="actual_combination_coefficients"`；`validate_covariance_record` 拒绝权重/诊断反推 |
| `ADJ-OBS-01` / `FZ-PROV-SHARED-SYSTEMATIC` | 三类划分：独立随机项进对角（`v_cal_independent`）；共享系统项进 covariance 面（低秩 / 相关核 / 共同 master_id 三通道，`evaluate_shared_gate` 要求 joint/naive>1）；模型偏差不进随机 ivar |
| 同一 bias master 只计一次（`OI-02` 精确形式） | 光/暗路 bias 的 `master_id` 相同 → 系数折叠 `−(1−α)/f`；不同 → `−1/f` 与 `+α/f` 两独立项（即 DESIGN §4.2 的 `V_b+α²V_b` 情形） |
| `FZ-CAL-FLOOR` | `0<f<0.1` 用 `max(f,0.1)` 且 `flat_floor_applied=true`；`f≤0`/非有限 → REJECT，不静默 floor 造值 |
| `FZ-CAL-QUANTUM-DEFAULT` | `V_q=q_adu²/12`，`q_adu` 缺省 1 ADU 并置 `quantum_default_applied` |
| `CAL-UNIT` | `calibration_unit_law()` = signal `ADU` / variance `ADU²` / ivar `ADU⁻²`；`unit_law_consistent` 强制 `variance=signal²` |
| `CAL-NO-CLIP` | 信号与方差通路无 clamp、无 pedestal；负值原样保留 |
| `FZ-UNITS` 独立项 | read_noise `(read_noise_e/gain)²`；photon_light `max(r,0)/gain`；dark_photon `max(d,0)/gain`（均 ADU²） |

## 2. fail-closed（`ALG-P1-001 §2.4`）

| 条件 | 处置 |
|---|---|
| 缺 gain 或 read_noise 且未声明 `empirical_mad_fallback` | `unavailable / kMissingGainOrReadNoise` |
| 声明 fallback 但无有效经验方差 | `unavailable / kMissingGainOrReadNoise` |
| 使用的 master 缺 `master_id` / 归一版本 / 单位 | `unavailable / kMissingMasterIdentity` |
| `f≤0` 或非有限 | `rejected / kNonPositiveFlat` |
| 共享项不可表示且无系统误差预算 | `unavailable / kSharedUnrepresentable` |
| 共享项存在但 joint/naive ≤ 1 | `kSharedNotDetected`（`evaluate_shared_gate` 红） |

## 3. 共享系统项三通道

- **lowrank**：`C_shared=L Lᵀ`，因子 `L`（n_pix×rank 行主序）由调用方物化 → `low_rank_variance`；
- **correlation_kernel**：`σ+kernel`，需 `kernel_id/version/scale`；核函数数值形式属 `DI-03`（**OPEN**），
  生产不发明核，仅接受已物化相关矩阵 `K`，否则 fail-closed（unavailable / 已声明系统误差预算兜底）；
- **common_master_id**：`master_id+α_m+V_master` → `(Σc_p α_p)²V_master`。

三通道均进入传播链并给出非负对角与联合/朴素比。

## 4. 测试与证据

- 共址测试：`tests/unit/v6_p1_cal/v6_cal_covariance_test.cpp`（85 用例，含 8 条负向注入必红）、
  独立 Oracle `v6_cal_oracle.hpp`（显式 4×4 矩阵 + 定种子 MC）、断言框架 `v6_cal_test_support.hpp`；
- CMake：`tests/unit/v6_p1_cal/CMakeLists.txt`（自包含，不依赖 legacy DLL；根构建面注册归控制器 C-004.4）；
- 证据：`run/v6/IMPL-P1-CAL-001/logs/`（构建/ctest/警告/ASan 日志）。

## 5. 未决（只登记，不擅改）

- `DI-03` 共享低秩/相关核数据面实例化 = OPEN（owner 含 IMPL-P1-CAL-001）：本实现不冻结秩上限/核函数形式；
- `OI-02`（同 master 折叠 vs DESIGN §4.2）/ `OI-03`（SCI-CAL-001 §9a 取代）只登记，归 W4/负责人；
- 故障注入钩子 `ASTROCS_V6_CAL_FAULT` 仅为测试用；未设置时行为严格正确。
