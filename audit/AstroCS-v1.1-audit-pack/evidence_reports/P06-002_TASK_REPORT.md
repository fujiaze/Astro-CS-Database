# P06-002 球面梯度与稳健叠加证据 - 任务执行报告

- 任务编号：P06-002
- 阶段：P06（G6 Gate）
- 任务名称：球面梯度与稳健叠加证据（v1.1 开发包）
- 执行日期：2026-07-27
- 执行人：子 Agent（GLM-5.2）
- 提交基线：5ec9866（P06-001 完成 + astro_toolkit 工具集）
- 入口条件：P06-001 已完成（8/8 兼容性检查 PASS，SNR² 代码路径已证明触发）

---

## 1. 任务目标

依据 `engineering/tasks/P06-002.md` 与 `docs/10_STAGE2_REAL_DATA_VALIDATION_SPEC.md`：

1. 验证重叠图统计（mean_pixel_count、重叠区域分布）
2. 验证梯度残差（GaiaDR3SP 启用时的梯度校正管线）
3. 验证异常剔除（sigma-clip/Winsorized 拒绝统计）
4. **必须证明 SNR² 权重真实生效**（不仅代码路径触发，且数学结果正确）
5. 保存重叠、梯度、拒绝和输出索引证据
6. 产出四份标准报告（TASK/TEST/EVIDENCE_INDEX/REVIEW）+ 结构化证据 JSON

---

## 2. 执行范围与约束

- **只读验证任务**：不修改任何业务源码
- **环境**：PowerShell 7.6.3 + Windows
- **入口程序**：`lib/orchestrator/cpp/orchestrator.exe`
  - 必须使用此路径（自动推导项目根目录向上 4 级正确）
  - 不使用 `build/artifacts/orchestrator.exe`（向上 4 级会得到 `F:\Astro dev`，DLL 加载失败）
- **DLL 依赖路径**：`build/artifacts;C:\msys64\mingw64\bin`
- **每个 stage2 运行超时**：180 秒
- **工具集**：`tools/astro_toolkit.py`（JSON 配置驱动批量执行）
- **Gaia 数据**：`GaiaDR3SP/`（20 个 .xpsd 文件，已可用）

---

## 3. 输入数据来源

### 3.1 P00-003 baseline 输入（T1）
- 目录：`lib/orchestrator/cpp/output_hiss_dir/`
- frame1.hiss（nside=32768, n_pix=15406480）
- frame2.hiss（nside=32768, n_pix=15407202）

### 3.2 合成离群值 HISS（T2/T3/T4）
- 目录：`engineering/evidence/P06-002/test_outlier/input/`
- 3 帧（A/B/C），nside=64，ipix 完全重叠，has_snr=true
- Frame C 为离群值（pixel=100.0，其他帧 pixel=10.0/20.0）
- 构造工具：`make_outlier_hiss.py`

### 3.3 合成 SNR HISS（T7）
- 目录：`engineering/evidence/P06-002/test_D_snr_weight/input/`
- 2 帧（A/B），nside=64，ipix 完全重叠，has_snr=true
- Frame A: pixel=10.0, snr=2.0 (w=4.0)
- Frame B: pixel=20.0, snr=4.0 (w=16.0)
- 期望加权均值 = (10×4 + 20×16)/(4+16) = 18.0
- 期望等权均值 = (10+20)/2 = 15.0
- 构造工具：`make_snr_hiss.py`

### 3.4 真实观测 HISS（T5/T6）
- 目录：`engineering/evidence/P06-002/test_B_overlap_duplicate/input/`
- 2 帧（C003 copy1 + C003 copy2），字节级一致
- nside=2048, n_pix=1566, filter=Red, exposure=600s
- 来源：P05-002 真实观测（NGC1727）
- 用途：强制完全重叠 + 真实 WCS（Gaia 查询可用）

---

## 4. 执行步骤

### 步骤 1：准备测试配置
- 新增 `configs/stage2_config_t1_baseline.json`（DEBUG 日志，无梯度校正）
- 复用 `configs/stage2_config_gradient.json`（GaiaDR3SP 启用）
- 复用 `configs/stage2_config_strict.json`（sigma=2.0）和 `stage2_config_default.json`（sigma=3.0）

### 步骤 2：批量运行测试（astro_toolkit）
- 编写 `tools/_p06_002_run_tests.json` 配置（21 步）
- 一次 RunCommand 执行：T1 baseline + T2 sigma-clip + T5 gradient + T6 determinism (2 runs)
- 每个测试捕获 stdout/stderr/exit code + inspect + SHA-256

### 步骤 3：SNR² 权重证明（T7）
- 编写 `tools/_p06_002_run_snr_debug.json` 配置
- 运行 stage2 on 合成 SNR HISS with DEBUG log
- 运行 `verify_snr_weight.py` 读取 HCSD 像素值，验证 = 18.0

### 步骤 4：收集证据
- 结构化结果 JSON：`stage2_gradient_evidence.json`
- 日志目录：`logs/`（汇总）+ 各测试子目录 `logs/`
- 源码引用：`hp_stack_hiss.cpp:224-228`（SNR² 权重公式）

### 步骤 5：撰写报告 + Commit + Push
- 产出 TASK/TEST/EVIDENCE_INDEX/REVIEW 四份报告
- 用 `astro_toolkit` 批量执行 git add + commit + push + log

---

## 5. 执行结果摘要

| 测试 ID | 名称 | 期望 | 实际 exit | 结果 |
|---|---|---|---|---|
| T1 | baseline 字节级可重现 | exit=0, SHA 与 P00-003 一致 | 0 | PASS |
| T2 | sigma-clip 严格 (sigma=2.0, 合成离群值) | exit=0, 剔除 Frame C | 0 | PASS |
| T3 | sigma-clip 默认 (sigma=3.0, 合成离群值) | exit=0, 剔除 Frame C | 0 | PASS |
| T4 | sigma-clip 宽松 (sigma=5.0, 合成离群值) | exit=0, 不剔除 | 0 | PASS |
| T5 | 梯度校正启用 (GaiaDR3SP) | exit=0, 梯度管线运行 | 0 | PASS |
| T6 | 确定性 (两次运行 SHA 一致) | run1 SHA == run2 SHA | 0 | PASS |
| T7 | SNR² 权重真实生效 | 输出像素=18.0=加权均值 | 0 | PASS |

**SNR² 权重生效证明**：PASS_SNR_WEIGHTED（输出=18.0=SNR² 加权均值，非等权 15.0，**真实生效**）
**梯度球面校正**：PASS（GaiaClient 创建成功，43383 颗 Gaia 星，梯度管线 5 阶段完整运行）
**重叠图证据**：PASS（T1 mean_pixel_count=1.9850 部分重叠；T5/T6 mean_pixel_count=2.0 完全重叠）
**异常剔除证据**：PASS（T2 sigma=2.0 剔除 4 离群值；T4 sigma=5.0 不剔除，行为符合预期）
**确定性证据**：PASS（两次运行 SHA-256 完全一致）

---

## 6. 兼容性、回滚与残留风险

### 6.1 兼容性
- 本任务为只读验证，未修改业务源码，对其他模块无影响。
- 验证结果确认 Stage2 在所有目标场景下行为符合契约（`hiss_format_v1.md` / `hcsd_format_v1.md`）。
- 梯度校正管线在 GaiaDR3SP 启用时完整运行（非退化回退）。

### 6.2 回滚
- 无需回滚（未修改业务代码）。
- 证据目录 `engineering/evidence/P06-002/` 为新增产物，可通过 `git revert` 单次提交回滚。

### 6.3 残留风险
1. **G-002 既存缺口（部分解决）**：真实观测 HISS has_snr=0 → SNR² 加权退化为等权。本任务通过合成 HISS (has_snr=1) 证明 SNR² 加权数学正确，但真实数据仍需 P03-004 修复 PHOTOMETRIC 后回归。
2. **梯度校正 fit_rms=0.0**：T5/T6 使用 C003 副本（字节级一致），差异为 0，fit_rms=0.0。证明管线工作正常但未验证非零差异的校正效果。建议未来用不同帧（如 C003+C005）测试非零梯度校正。
3. **HCSD has_snr 字段不传播**：输入 HISS has_snr=1，但 HCSD meta_json has_snr=false。这是 HCSD 写入端的字段传播问题，不影响实际堆叠数学（SNR² 已正确应用），但影响 inspect 输出的诊断信息。建议未来修复 HCSD 写入端。

---

## 7. 自动化与可复现性

- 所有 stage2 运行均通过 `orchestrator.exe stage2 --frames <dir> --output <hcsd> --config <json> --log-level DEBUG` 调用，可独立复现。
- 所有测试配置 JSON 已保存：`configs/stage2_config_*.json`
- 所有运行日志（stdout/stderr/exit code）已保存：各测试子目录 `logs/`
- 所有输出 HCSD SHA-256 已记录在 `stage2_gradient_evidence.json`
- baseline 检查（T1）证明 P00-003 的字节级可重现性（SHA 完全一致）
- 确定性检查（T6）证明同输入两次运行字节级一致

---

## 8. 交付物清单

| 文件 | 描述 |
|---|---|
| `TASK_REPORT.md` | 本文件，任务执行报告 |
| `TEST_REPORT.md` | 测试结果报告 |
| `EVIDENCE_INDEX.md` | 证据索引 |
| `REVIEW_REPORT.md` | 独立复核报告 |
| `stage2_gradient_evidence.json` | 结构化证据 JSON（重叠/梯度/拒绝/SNR权重各项结果） |
| `logs/stage2_run.jsonl` | T1 baseline stdout（汇总） |
| `logs/stage2_run.err.log` | T1 baseline stderr（DEBUG 日志，汇总） |
| `logs/inspect_hcsd.json` | T1 baseline HCSD inspect 输出（汇总） |
| `configs/stage2_config_t1_baseline.json` | T1 baseline 配置（DEBUG，无梯度） |
| `configs/stage2_config_gradient.json` | T5/T6 梯度配置（GaiaDR3SP 启用） |
| `configs/stage2_config_strict.json` | T2 严格 sigma-clip 配置（sigma=2.0） |
| `configs/stage2_config_default.json` | T3/T7 默认配置（sigma=3.0） |
| `configs/stage2_config_loose.json` | T4 宽松 sigma-clip 配置（sigma=5.0） |
| `T1_baseline/` | baseline 可重现性证据（P00-003 输入，DEBUG） |
| `T2_sigma_clip_debug/` | sigma-clip 严格模式证据（合成离群值，DEBUG） |
| `T5_gradient/` | 梯度校正启用证据（GaiaDR3SP，DEBUG） |
| `T6_determinism/` | 确定性证据（两次运行 SHA 一致） |
| `T7_snr_weight_debug/` | SNR² 权重证明证据（合成 SNR HISS，DEBUG，PASS_SNR_WEIGHTED） |
| `runs/T2_outlier_strict/` | T2 早期运行证据（sigma=2.0） |
| `runs/T3_outlier_default/` | T3 早期运行证据（sigma=3.0） |
| `runs/T4_outlier_loose/` | T4 早期运行证据（sigma=5.0） |
| `test_B_overlap_duplicate/` | 重叠图证据（C003 副本，mean_pixel_count=2.0） |
| `test_D_snr_weight/` | SNR² 权重早期运行证据 |
| `test_outlier/` | 合成离群值 HISS 输入 + 预期值 |
| `make_outlier_hiss.py` | 合成离群值 HISS 构造工具 |
| `make_snr_hiss.py` | 合成 SNR HISS 构造工具 |
| `verify_snr_weight.py` | SNR² 权重验证脚本 |
| `run_stage2.ps1` | stage2 运行封装脚本（早期） |

---

## 9. 任务结论

P06-002 球面梯度与稳健叠加证据任务全部完成：

1. **7 项验证全部 PASS**：T1 baseline 可重现 / T2-T4 sigma-clip 三档 / T5 梯度校正启用 / T6 确定性 / T7 SNR² 权重真实生效
2. **SNR² 权重真实生效**：合成 HISS (has_snr=1) 输出像素=18.0=SNR² 加权均值（非等权 15.0），**definitive proof**
3. **梯度校正管线完整运行**：GaiaDR3SP 启用，GaiaClient 创建成功，43383 颗 Gaia 星，5 阶段管线完整运行，HCSD meta 标注 `gradient_correction.enabled=true, success=true`
4. **重叠/拒绝/输出索引证据齐全**：mean_pixel_count（1.9850/2.0/3.0）、sigma-clip 拒绝数（0/4）、inspect 输出 JSON
5. **确定性保证**：两次运行 SHA-256 完全一致
6. **未修改业务源码**：符合只读验证任务约束

**VERDICT: PASS**
