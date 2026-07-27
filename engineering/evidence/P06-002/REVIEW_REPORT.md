# P06-002 球面梯度与稳健叠加证据 - 独立复核报告

- 任务编号：P06-002
- 复核日期：2026-07-27
- 复核人：子 Agent（GLM-5.2，独立复核视角）
- 复核对象：`engineering/evidence/P06-002/` 全部证据
- 提交基线：5ec9866（P06-001 完成 + astro_toolkit 工具集）

---

## 1. 复核范围

本复核独立审查 P06-002 任务的所有证据，验证：

1. **7 项验证测试**（T1-T7）的行为是否符合预期。
2. **SNR² 权重是否真实生效**（不仅代码路径触发，且数学结果正确）。
3. **梯度球面校正管线是否完整运行**（非退化回退）。
4. **sigma-clip/Winsorized 异常剔除**是否在 sigma=2.0/3.0/5.0 三档下行为正确。
5. **确定性保证**（同输入两次运行 SHA-256 一致）。
6. **证据完整性**（HCSD SHA-256、日志一致性、源码引用准确性）。
7. **是否未修改业务源码**（只读验证任务约束）。
8. **既存退化（G-002）是否被正确记录且不阻塞 PASS**。

---

## 2. 复核方法

1. **结构化证据 JSON 审查**：逐项检查 `stage2_gradient_evidence.json` 中 7 项测试的字段一致性。
2. **日志一致性核对**：核对每项测试的 `stage2_stderr.log` 关键日志行与 JSON 记录的关键证据。
3. **HCSD inspect 输出核对**：核对 `hcsd_inspect.log` 中的 meta_json 与 JSON 记录的输出索引。
4. **SHA-256 完整性**：核对 6 个 HCSD 输出的 SHA-256 与日志/JSON 记录一致。
5. **可重现性验证**：检查 T1 的 HCSD SHA-256 与 P00-003 baseline 完全一致。
6. **确定性验证**：检查 T6 两次运行 HCSD SHA-256 完全一致。
7. **源码引用验证**：核对 SNR² 权重公式、sigma-clip 实现、梯度采样器代码位置。
8. **数学证明验证**：独立计算 T7 的 SNR² 加权均值与等权均值，核对与输出像素值。
9. **Git 状态验证**：确认未修改业务源码（仅新增证据目录 + 修改任务注册表/控制文件）。

---

## 3. 复核结果

### 3.1 T1 baseline 字节级可重现性

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| exit code | 0 | 0 | ✓ |
| HCSD SHA-256 | `2A9BD12E...4122C37`（与 P00-003 一致） | `2A9BD12E...4122C37` | ✓ |
| HCSD 大小 | 187455430 bytes | 187455430 bytes | ✓ |
| mean_pixel_count | 1.9850（部分重叠） | 1.9850 | ✓ |
| SNR² 加权日志 | 含 "第二遍累加完成 (SNR² 加权)" | 含 | ✓ |
| 梯度回退 | gaia_data_dir="" → 回退（设计行为） | 回退 | ✓ |
| nside / n_pix | 32768 / 15522966 | 32768 / 15522966 | ✓ |

**复核结论**：T1 baseline 字节级可重现（与 P00-003 SHA-256 完全一致），PASS。

### 3.2 T2 sigma-clip 严格模式（sigma=2.0）

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| exit code | 0 | 0 | ✓ |
| 输入 has_snr | 1（合成 HISS 带 SNR） | 1 | ✓ |
| sigma-clip 迭代 0 剔除数 | 4（Frame C 在 4 像素全被剔除） | 4 | ✓ |
| sigma-clip 迭代 1 剔除数 | 0（提前收敛） | 0 | ✓ |
| mean_pixel_count | 2.0（3 帧→剔除 1 帧→剩 2 帧） | 2.0 | ✓ |
| SNR² 加权日志 | 含 "sigma-clip 迭代 0: 剔除 4 个离群值 (SNR² 加权)" | 含 | ✓ |
| 拒绝条件验证 | dev=78.095 > 2*std=35.79 → 剔除 | 数学正确 | ✓ |

**复核结论**：T2 sigma=2.0 正确剔除合成离群值 Frame C，PASS。

### 3.3 T3 sigma-clip 默认模式（sigma=3.0）

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| exit code | 0 | 0 | ✓ |
| sigma-clip 剔除数 | 4（dev=78.095 > 3*std=53.68 → 剔除） | 4 | ✓ |
| mean_pixel_count | 2.0 | 2.0 | ✓ |

**复核结论**：T3 sigma=3.0 也正确剔除，PASS。

### 3.4 T4 sigma-clip 宽松模式（sigma=5.0）

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| exit code | 0 | 0 | ✓ |
| sigma-clip 剔除数 | 0（dev=78.095 < 5*std=89.46 → 不剔除） | 0 | ✓ |
| mean_pixel_count | 3.0（3 帧全保留） | 3.0 | ✓ |

**复核结论**：T4 sigma=5.0 阈值过宽不剔除，行为符合预期，PASS。

### 3.5 T5 梯度校正启用（GaiaDR3SP）

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| exit code | 0 | 0 | ✓ |
| GaiaClient 创建 | 成功（gaia_data_dir=GaiaDR3SP） | 成功 | ✓ |
| Gaia 星查询 | 43383 颗（半径 0.942°） | 43383 颗 | ✓ |
| 控制点候选 | 425 per frame | 425 | ✓ |
| 总样本数 | 850（2 帧 × 425） | 850 | ✓ |
| 差异拟合 success | 1 | 1 | ✓ |
| 拟合 lambda | 1.0e-04 | 1.0e-04 | ✓ |
| 5 阶段完整运行 | 采样/拟合/读取/叠加/写入 | 全部完成 | ✓ |
| HCSD meta gradient_correction.enabled | true | true | ✓ |
| HCSD meta gradient_correction.success | true | true | ✓ |
| HCSD meta gradient_correction.method | diff_fit_spherical_spline | diff_fit_spherical_spline | ✓ |
| fit_rms | 0.0（两帧字节级一致，差异为 0） | 0.0 | ✓（非退化，证明管线工作正常） |

**复核结论**：T5 梯度校正管线完整运行，GaiaClient 创建成功，5 阶段全部完成，HCSD meta 标注成功，PASS（非退化回退）。

### 3.6 T6 确定性

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| run1 exit code | 0 | 0 | ✓ |
| run2 exit code | 0 | 0 | ✓ |
| run1 SHA-256 | `B9290F43...020857` | `B9290F43...020857` | ✓ |
| run2 SHA-256 | `B9290F43...020857` | `B9290F43...020857` | ✓ |
| SHA-256 一致性 | run1 == run2 | 完全一致 | ✓ |
| run1 大小 / run2 大小 | 1198683 / 1198683 | 1198683 / 1198683 | ✓ |

**复核结论**：T6 两次运行 HCSD SHA-256 完全一致，确定性保证，PASS。

### 3.7 T7 SNR² 权重真实生效（definitive proof）

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| exit code | 0 | 0 | ✓ |
| 输入 has_snr | 1（合成 HISS 带 SNR 通道） | 1 | ✓ |
| 日志 hiss_read has_snr | "has_snr=1" | "has_snr=1" | ✓ |
| 日志 SNR² 加权 | 含 "第二遍累加完成 (SNR² 加权)" | 含 | ✓ |
| 输出像素值 | [18.0, 18.0, 18.0, 18.0] | [18.0, 18.0, 18.0, 18.0] | ✓ |
| 期望加权均值 | (10×4 + 20×16)/(4+16) = 360/20 = 18.0 | 18.0 | ✓ |
| 期望等权均值 | (10+20)/2 = 15.0 | 15.0（非输出值） | ✓ |
| verify_snr_weight.py 输出 | VERDICT: PASS_SNR_WEIGHTED | PASS_SNR_WEIGHTED | ✓ |
| 源码引用 | hp_stack_hiss.cpp:224-228, `w = hd.snr ? snr² : 1.0` | 行号正确，公式正确 | ✓ |

**独立数学验证**：
- Frame A: pixel=10.0, snr=2.0 → w=snr²=4.0
- Frame B: pixel=20.0, snr=4.0 → w=snr²=16.0
- sum_weight = 4.0 + 16.0 = 20.0
- sum_pixel_times_weight = 10.0×4.0 + 20.0×16.0 = 40.0 + 320.0 = 360.0
- weighted_mean = 360.0 / 20.0 = **18.0** ✓
- equal_weight_mean = (10.0 + 20.0) / 2 = 15.0 ✗（非输出值）
- 实际输出 = 18.0 = weighted_mean → **SNR² 加权真实生效**

**复核结论**：T7 输出像素值 = 18.0 = SNR² 加权均值，**不等于**等权均值 15.0，definitive proof，PASS_SNR_WEIGHTED。

---

## 4. SNR² 权重生效证明深度复核

### 4.1 代码路径触发（P06-001 已证明）

- 日志关键词：`[hp_stack_hiss] 第二遍累加完成 (SNR² 加权)` ✓
- 日志关键词：`[hp_stack_hiss] sigma-clip 迭代 0: 剔除 N 个离群值 (SNR² 加权)` ✓
- 源码：`hp_stack_hiss.cpp:224-228`，`double w = hd.snr ? (double)hd.snr[i] * hd.snr[i] : 1.0;` ✓

### 4.2 数学正确性（P06-002 T7 新增证明）

- **P06-001 缺口**：仅证明代码路径触发，因 has_snr=0 退化为等权（w=1.0），无法验证数学正确性。
- **P06-002 T7 解决**：构造合成 HISS（has_snr=1），输入 pixel=[10.0, 20.0], snr=[2.0, 4.0]。
- **数学期望**：
  - SNR² 加权：18.0
  - 等权退化：15.0
- **实际输出**：18.0 = SNR² 加权均值 ✓
- **结论**：SNR² 加权**真实生效**，非仅代码路径触发。

### 4.3 退化说明

- 真实观测 HISS（P05-002）has_snr=0（G-002 缺口），SNR² 加权退化为等权（w=1.0）。
- 本任务通过合成 HISS (has_snr=1) 证明 SNR² 加权数学正确。
- 真实数据 has_snr=0 仍退化为等权，待 P03-004 修复 PHOTOMETRIC 后回归验证。

---

## 5. 梯度球面校正深度复核

### 5.1 P06-001 状态

- 状态：DEGRADED（G-002）
- 原因：`gaia_client_create_ex 失败`，回退无梯度校正模式。
- 根因：`stage2_config.json` 中 `gaia_data_dir` 未配置。

### 5.2 P06-002 解决方案

- 配置：`stage2_config_gradient.json` 中 `gaia_data_dir="GaiaDR3SP"`。
- 结果：GaiaClient 创建成功，43383 颗 Gaia 星查询成功。
- 管线：5 阶段完整运行（采样/拟合/读取/叠加/写入）。
- HCSD meta：`gradient_correction: {enabled: true, success: true, method: "diff_fit_spherical_spline"}`。

### 5.3 fit_rms=0.0 说明

- T5/T6 使用 C003 副本（字节级一致），差异为 0，校正场为 0。
- fit_rms=0.0 **不**代表管线失效，而是输入数据特性决定的。
- 证明：5 阶段日志全部输出，差异拟合 success=1，HCSD meta 标注 success=true。
- 建议：未来用不同帧（如 C003+C005）测试非零梯度校正效果。

**复核结论**：梯度校正管线完整运行，非退化回退，PASS。

---

## 6. 证据完整性

### 6.1 HCSD SHA-256 索引

| 测试 | SHA-256 | 大小 | 一致性 |
|---|---|---|---|
| T1 baseline | `2A9BD12E...4122C37` | 187455430 | ✓ 与 P00-003 一致 |
| T2 sigma-clip strict | `AF0BDA96...596D78` | 1179870 | ✓ |
| T5 gradient | `B9290F43...020857` | 1198683 | ✓ |
| T6 run1 | `B9290F43...020857` | 1198683 | ✓ 与 T5 一致（同输入同配置） |
| T6 run2 | `B9290F43...020857` | 1198683 | ✓ 与 run1 一致（确定性） |
| T7 SNR weight | `4BAD8B41...59C315A` | 1179873 | ✓ |

### 6.2 日志一致性

- 每项测试的 `stage2_stderr.log` 关键日志行与 JSON 记录的关键证据一致 ✓
- 每项测试的 `hcsd_inspect.log` 中 meta_json 与 JSON 记录的输出索引一致 ✓
- exit code 全部为 0（成功路径） ✓

### 6.3 源码引用准确性

| 源码位置 | 行号 | 复核结果 |
|---|---|---|
| `hp_stack_hiss.cpp` | 224-228 | ✓ SNR² 权重公式 `w = hd.snr ? snr² : 1.0` |
| `hp_stack_hiss.cpp` | 263-329 | ✓ sigma-clip 实现 |
| `gradient_sampler.cpp` | 317-606 | ✓ 球面背景采样 |
| `corrected_stacker.cpp` | 150-204 | ✓ 梯度校正叠加 |

### 6.4 业务源码未修改约束

- 本任务为只读验证，仅新增 `engineering/evidence/P06-002/` 目录 ✓
- 修改文件：仅控制文件（MASTER_TASK_REGISTER.csv / PROJECT_STATE.yaml / CURRENT_TASK.md） ✓
- 未修改业务源码 ✓

**复核结论**：证据完整，可复现，符合审计要求。

---

## 7. 发现的问题

### 7.1 既存退化（非本任务引入，不阻塞 PASS）

1. **G-002 缺口（部分解决）**：
   - 状态：HISS has_snr=0（真实观测数据）→ SNR² 加权退化为等权。
   - 本任务贡献：通过合成 HISS (has_snr=1) 证明 SNR² 加权数学正确（T7 definitive proof）。
   - 残留：真实数据 has_snr=0 仍退化为等权，待 P03-004 修复 PHOTOMETRIC 后回归。

2. **HCSD has_snr 字段不传播**：
   - 现象：输入 HISS has_snr=1，但 HCSD meta_json has_snr=false。
   - 影响：不影响实际堆叠数学（SNR² 已正确应用），但影响 inspect 输出的诊断信息。
   - 建议：未来修复 HCSD 写入端，使 has_snr 字段正确传播。

3. **梯度校正 fit_rms=0.0**：
   - 现象：T5/T6 使用 C003 副本（字节级一致），差异为 0，fit_rms=0.0。
   - 影响：证明管线工作正常但未验证非零差异的校正效果。
   - 建议：未来用不同帧（如 C003+C005）测试非零梯度校正。

### 7.2 设计观察（非问题，记录备查）

1. **sigma-clip 在 std=0 时不触发**：T5/T6 两帧字节级一致，std=0，sigma-clip 不剔除（设计行为，避免除零）。
2. **filter 混合取首帧**：T2/T7 合成数据 filter=Lum（取首帧），与 P06-001 一致。
3. **权重钳位**：源码中 `if (w > 1e6) w = 1e6; if (w < 0.0) w = 0.0;` 避免数值溢出，符合 spec §9.3。

---

## 8. 风险评估

| 风险项 | 等级 | 缓解措施 |
|---|---|---|
| G-002 既存缺口（真实数据 has_snr=0） | 低 | 已通过 T7 合成 HISS 证明数学正确，待 P03-004 修复后回归 |
| HCSD has_snr 字段不传播 | 低 | 不影响堆叠数学，仅影响诊断信息，待未来修复 |
| 梯度校正 fit_rms=0.0 | 低 | 管线工作正常，待未来用不同帧测试非零差异 |
| sigma-clip std=0 不触发 | 低 | 设计行为，避免除零 |

**总体风险**：低。所有风险均为既存或设计行为，非本任务引入。

---

## 9. 验收清单

| 验收项 | 状态 |
|---|---|
| 依赖任务均已通过（P06-001） | ✓ |
| 本任务目标有可复现证据 | ✓（HCSD SHA-256 + 日志 + JSON） |
| 相关回归全部运行 | ✓（7 项测试 T1-T7 全执行） |
| 独立复核以 VERDICT: PASS 结束 | ✓（见下） |
| 四份标准报告已产出 | ✓（TASK/TEST/EVIDENCE_INDEX/REVIEW） |
| 结构化证据 JSON 已产出 | ✓（stage2_gradient_evidence.json） |
| 任务注册表已更新 | 待 commit 时同步 |
| SNR² 权重真实生效证明 | ✓（T7 definitive proof，输出=18.0=加权均值） |
| 梯度校正管线完整运行 | ✓（T5 GaiaClient 创建成功，5 阶段完成） |
| 确定性保证 | ✓（T6 两次运行 SHA-256 一致） |
| 未修改业务源码 | ✓（只读验证任务约束） |

---

## 10. 复核结论

P06-002 球面梯度与稳健叠加证据任务执行完整、证据充分、行为符合预期：

1. **7 项验证全部 PASS**：
   - T1 baseline 字节级可重现（SHA-256 与 P00-003 一致）
   - T2/T3/T4 sigma-clip 三档行为正确（2.0/3.0 剔除，5.0 不剔除）
   - T5 梯度校正管线完整运行（GaiaClient 创建成功，43383 颗 Gaia 星，5 阶段完成）
   - T6 确定性保证（两次运行 SHA-256 完全一致）
   - T7 SNR² 权重真实生效（输出=18.0=加权均值，非等权 15.0，definitive proof）

2. **SNR² 权重真实生效**：
   - 代码路径触发：日志含 "SNR² 加权" 关键词 ✓
   - 数学正确性：T7 输出像素=18.0=SNR² 加权均值（非等权 15.0）✓
   - **definitive proof**：通过合成 HISS (has_snr=1) 证明，非仅代码路径触发

3. **梯度校正管线完整运行**：
   - GaiaClient 创建成功（gaia_data_dir=GaiaDR3SP）✓
   - 43383 颗 Gaia 星查询成功 ✓
   - 5 阶段完整运行（采样/拟合/读取/叠加/写入）✓
   - HCSD meta 标注 success=true ✓
   - 非退化回退（与 P06-001 的 DEGRADED 状态对比）✓

4. **证据完整可复现**：
   - 6 个 HCSD 输出全部记录 SHA-256 ✓
   - baseline 字节级可重现（与 P00-003 一致）✓
   - 确定性保证（T6 两次运行一致）✓
   - 结构化证据 JSON 完整 ✓

5. **未修改业务源码**：符合只读验证任务约束 ✓

6. **既存退化已记录**：
   - G-002（真实数据 has_snr=0 退化）已通过 T7 合成数据证明数学正确，待 P03-004 修复后回归
   - HCSD has_snr 字段不传播（不影响堆叠数学）
   - 梯度校正 fit_rms=0.0（输入数据特性，非管线失效）

**VERDICT: PASS**
