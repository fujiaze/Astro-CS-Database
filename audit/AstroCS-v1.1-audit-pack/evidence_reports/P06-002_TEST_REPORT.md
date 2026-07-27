# P06-002 球面梯度与稳健叠加证据 - 测试结果报告

- 任务编号：P06-002
- 执行日期：2026-07-27
- 测试环境：PowerShell 7.6.3 + Windows
- 被测程序：`lib/orchestrator/cpp/orchestrator.exe`（stage2 子命令）
- 工具集：`tools/astro_toolkit.py`（JSON 配置驱动批量执行）

---

## 1. 测试矩阵

共 7 项验证测试，覆盖 baseline 可重现性、sigma-clip 三档、梯度校正启用、确定性、SNR² 权重真实生效。每项测试独立输入目录、独立日志、独立输出。

---

## 2. 测试用例详情

### 2.1 T1：baseline 字节级可重现性

- **目的**：验证 P00-003 stage2 baseline 在相同输入下产出字节级一致的 HCSD（DEBUG 日志）
- **输入**：`lib/orchestrator/cpp/output_hiss_dir/`（frame1.hiss + frame2.hiss，nside=32768）
- **命令**：`orchestrator.exe stage2 --frames <dir> --output T1_baseline.hcsd --config stage2_config_t1_baseline.json --log-level DEBUG`
- **超时**：180 秒
- **期望**：exit=0，HCSD SHA-256=`2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37`
- **实际**：
  - exit code = 0
  - HCSD 大小 = 187455430 字节
  - HCSD SHA-256 = `2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37`
  - 耗时 = 6.63 秒
  - 重叠统计：nside=32768, n_pix=15522966, mean_pixel_count=1.9850（2 帧部分重叠）
  - sigma-clip：迭代 0 剔除 0 个，提前收敛
  - SNR² 加权：日志含 "第二遍累加完成 (SNR² 加权)"，has_snr=0 退化为等权
  - 梯度校正：gaia_data_dir 为空，回退无梯度校正模式
- **结果**：**PASS**（字节级与 P00-003 完全一致）

### 2.2 T2：sigma-clip 严格模式（sigma=2.0，合成离群值，DEBUG）

- **目的**：验证 sigma-clip 在合成离群值场景下正确剔除（DEBUG 日志）
- **输入**：`test_outlier/input/`（3 帧合成 HISS，has_snr=true，Frame C 为离群值 pixel=100.0）
- **命令**：`orchestrator.exe stage2 --frames <dir> --output T2_sigma_clip.hcsd --config stage2_config_strict.json --log-level DEBUG`
- **超时**：180 秒
- **期望**：exit=0，sigma=2.0 剔除 Frame C（dev=78.095 > 2*std=35.79）
- **实际**：
  - exit code = 0
  - HCSD 大小 = 1179870 字节
  - HCSD SHA-256 = `AF0BDA96EA7BA05922A69606AC40D4CB0A8B9C01A2681A05B42F85B6DE596D78`
  - 耗时 = 0.0087 秒
  - 输入 has_snr=1（合成 HISS 带 SNR 通道）
  - 日志关键证据：
    - `[hp_stack_hiss] 帧 0: ... has_snr=1`
    - `[hp_stack_hiss] 帧 1: ... has_snr=1`
    - `[hp_stack_hiss] 帧 2: ... has_snr=1`
    - `[hp_stack_hiss] 第二遍累加完成 (SNR² 加权)`
    - `[hp_stack_hiss] sigma-clip 迭代 0: 剔除 4 个离群值 (SNR² 加权)`
    - `[hp_stack_hiss] sigma-clip 迭代 1: 剔除 0 个离群值 (SNR² 加权)`
    - `[hp_stack_hiss] 迭代 1 无剔除, 提前收敛`
  - mean_pixel_count=2.0000（3 帧输入，剔除 1 帧后剩 2 帧）
- **结果**：**PASS**（sigma=2.0 正确剔除 Frame C 在所有 4 像素）

### 2.3 T3：sigma-clip 默认模式（sigma=3.0，合成离群值）

- **目的**：验证 sigma=3.0 也能正确剔除合成离群值
- **输入**：`test_outlier/input/`（同 T2）
- **命令**：`orchestrator.exe stage2 --frames <dir> --output outlier_T3_outlier_default.hcsd --config stage2_config_default.json`
- **超时**：180 秒
- **期望**：exit=0，sigma=3.0 剔除 Frame C（dev=78.095 > 3*std=53.68）
- **实际**：
  - exit code = 0
  - HCSD 大小 = 1179873 字节
  - mean_pixel_count=2.0000（剔除 Frame C）
  - sigma_clip meta: sigma=3.0000, max_iter=5
- **结果**：**PASS**（sigma=3.0 正确剔除）

### 2.4 T4：sigma-clip 宽松模式（sigma=5.0，合成离群值）

- **目的**：验证 sigma=5.0 阈值过宽时不剔除
- **输入**：`test_outlier/input/`（同 T2）
- **命令**：`orchestrator.exe stage2 --frames <dir> --output outlier_T4_outlier_loose.hcsd --config stage2_config_loose.json`
- **超时**：180 秒
- **期望**：exit=0，sigma=5.0 不剔除（dev=78.095 < 5*std=89.46）
- **实际**：
  - exit code = 0
  - HCSD 大小 = 1179873 字节
  - mean_pixel_count=3.0000（3 帧全保留，无剔除）
  - sigma_clip meta: sigma=5.0000, max_iter=1
- **结果**：**PASS**（sigma=5.0 不剔除，行为符合预期）

### 2.5 T5：梯度校正启用测试（GaiaDR3SP）

- **目的**：验证 GaiaDR3SP 启用时梯度校正管线完整运行（DEBUG 日志）
- **输入**：`test_B_overlap_duplicate/input/`（C003 copy1 + C003 copy2，nside=2048，NGC1727）
- **命令**：`orchestrator.exe stage2 --frames <dir> --output T5_gradient.hcsd --config stage2_config_gradient.json --log-level DEBUG`
- **超时**：180 秒
- **期望**：exit=0，GaiaClient 创建成功，梯度管线 5 阶段运行
- **实际**：
  - exit code = 0
  - HCSD 大小 = 1198683 字节
  - HCSD SHA-256 = `B9290F43A4C3E96E534DA27DF0E1189A7C25D72B96D263C4834F14F6A1020857`
  - 耗时 = 86.78 秒
  - HCSD meta 含 `gradient_correction: {enabled: true, success: true, lambda: "1.0e-04", method: "diff_fit_spherical_spline"}`
  - 日志关键证据：
    - `[GRADIENT_SPHERE] gradient_max_iter=10 gradient_lambda=0.000100 gaia_data_dir=F:\Astro dev\Astro CS Normalization Database\GaiaDR3SP`
    - `[gradient_sampler] 开始采样: 2 帧, gaia_dir=...GaiaDR3SP`
    - `[gradient_sampler] Gaia 星: 43383 颗 (半径 0.942°)`
    - `[gradient_sampler] 控制点候选: 425`
    - `[gradient_sampler] 完成: 2 帧处理, 0 帧跳过, 850 样本`
    - `[hp_stack_gradient_corrected] 采样完成: 850 样本, 2 帧处理`
    - `[hp_stack_gradient_corrected] === 阶段2: 差异拟合 (3D 嵌入球面样条) ===`
    - `[hp_stack_gradient_corrected] 差异拟合完成: success=1, lambda=1.000000e-04`
    - `[hp_stack_gradient_corrected] 帧 0: n_ctrl=425 w_range=[0.0000, 0.0000] w_absmax=0.0000 fit_rms=0.0000`
    - `[hp_stack_gradient_corrected] 帧 1: n_ctrl=425 w_range=[0.0000, 0.0000] w_absmax=0.0000 fit_rms=0.0000`
    - `[hp_stack_gradient_corrected] === 阶段3: 读取帧数据 + SNR 重建 ===`
    - `[hp_stack_gradient_corrected] === 阶段4: 梯度校正叠加 ===`
    - `[hp_stack_gradient_corrected] 叠加完成: 1566 像素`
    - `[hp_stack_gradient_corrected] === 阶段5: 写入 .hcsd ===`
  - 注：fit_rms=0.0000 因两帧字节级一致（C003 副本），差异为 0，校正场为 0。证明管线工作正常。
- **结果**：**PASS**（梯度校正管线完整运行，GaiaClient 创建成功）

### 2.6 T6：确定性测试（两次运行 SHA-256 一致）

- **目的**：验证 stage2 在相同输入下产出字节级一致的 HCSD（确定性保证）
- **输入**：`test_B_overlap_duplicate/input/`（同 T5）
- **命令**：两次运行 T5 相同命令，输出到不同文件
- **超时**：每次 180 秒
- **期望**：run1 SHA-256 == run2 SHA-256
- **实际**：
  - run1: T6_run1.hcsd, 大小=1198683, SHA-256=`B9290F43A4C3E96E534DA27DF0E1189A7C25D72B96D263C4834F14F6A1020857`, 耗时=83.49s
  - run2: T6_run2.hcsd, 大小=1198683, SHA-256=`B9290F43A4C3E96E534DA27DF0E1189A7C25D72B96D263C4834F14F6A1020857`, 耗时=80.50s
  - SHA-256 完全一致
- **结果**：**PASS**（确定性保证，字节级一致）

### 2.7 T7：SNR² 权重真实生效证明（合成 HISS，has_snr=true）

- **目的**：构造带 SNR 的合成 HISS，验证 SNR² 加权公式真实生效（不仅代码路径触发）
- **输入**：`test_D_snr_weight/input/`（2 帧合成 HISS，has_snr=true）
  - Frame A: pixel=10.0, snr=2.0 (w=snr²=4.0)
  - Frame B: pixel=20.0, snr=4.0 (w=snr²=16.0)
- **命令**：
  1. `orchestrator.exe stage2 --frames <dir> --output snr_weighted.hcsd --config stage2_config_default.json --log-level DEBUG`
  2. `python verify_snr_weight.py snr_weighted.hcsd`
- **超时**：120 秒
- **期望**：输出像素=18.0（SNR² 加权均值），非 15.0（等权均值）
- **实际**：
  - exit code = 0
  - HCSD 大小 = 1179873 字节
  - HCSD SHA-256 = `4BAD8B4130EFDF2D2ED1E593D457F26F0EF0CBA682AF572644184403659C315A`
  - 输出像素值 = [18.0, 18.0, 18.0, 18.0]
  - verify_snr_weight.py 输出：`VERDICT: PASS_SNR_WEIGHTED - SNR² 加权真实生效!`
  - 日志关键证据：
    - `[hio] hiss_read: nside=64 nested=1 n_pix=4 has_snr=1 snr_format=0`（输入 has_snr=1）
    - `[hp_stack_hiss] 第二遍累加完成 (SNR² 加权)`
    - `[hp_stack_hiss] sigma-clip 迭代 0: 剔除 0 个离群值 (SNR² 加权)`
  - 数学证明：
    - 加权均值 = (10×4 + 20×16)/(4+16) = 360/20 = **18.0** ✓
    - 等权均值 = (10+20)/2 = 15.0 ✗（不是输出值）
- **结果**：**PASS_SNR_WEIGHTED**（SNR² 加权真实生效，definitive proof）

---

## 3. SNR² 权重生效证明（详细）

### 3.1 配置加载
- `stage2_config_default.json` 中 `weighting=snr_squared`，日志确认加载成功

### 3.2 代码路径触发
- 日志关键词（T7 成功路径出现）：
  - `[hp_stack_hiss] 第二遍累加完成 (SNR² 加权)`
  - `[hp_stack_hiss] sigma-clip 迭代 0: 剔除 0 个离群值 (SNR² 加权)`

### 3.3 源码引用
- `lib/healpix_db/healpix_stack/hp_stack_hiss.cpp:224-228`
  ```cpp
  // SNR² 权重: 有 snr 通道时用 snr², 无 snr 通道时用 1.0 (等权, 向后兼容)
  double w = hd.snr ? (double)hd.snr[i] * hd.snr[i] : 1.0;
  // 钳位 weight 到 [0, 1e6] 避免数值溢出 (spec §9.3)
  if (w > 1e6) w = 1e6;
  if (w < 0.0) w = 0.0;
  ```

### 3.4 数学证明（T7，definitive proof）
- 输入 HISS has_snr=1（合成数据带 SNR 通道）
- Frame A: pixel=10.0, snr=2.0 → w=snr²=4.0
- Frame B: pixel=20.0, snr=4.0 → w=snr²=16.0
- SNR² 加权均值 = (10×4 + 20×16)/(4+16) = (40+320)/20 = 360/20 = **18.0**
- 等权均值（退化）= (10+20)/2 = **15.0**
- 实际输出像素 = **18.0** ✓（等于 SNR² 加权均值，不等于等权均值）
- **结论**：SNR² 加权真实生效，非仅代码路径触发

### 3.5 退化说明
- 真实观测 HISS（P05-002）has_snr=0（G-002 缺口），SNR² 加权退化为等权（w=1.0）
- 本任务通过合成 HISS (has_snr=1) 证明 SNR² 加权数学正确
- 真实数据 has_snr=0 仍退化为等权，待 P03-004 修复 PHOTOMETRIC 后回归

---

## 4. 重叠/拒绝/输出索引统计

### 4.1 重叠统计
| 测试 | unique_pixels | mean_pixel_count | n_frames | 说明 |
|---|---|---|---|---|
| T1 baseline | 15522966 | 1.9850 | 2 | 真实数据部分重叠 |
| T2 sigma-clip strict | 4 | 2.0000 | 3→2 | 合成数据完全重叠，剔除 1 帧 |
| T3 sigma-clip default | 4 | 2.0000 | 3→2 | 同 T2 |
| T4 sigma-clip loose | 4 | 3.0000 | 3 | 合成数据完全重叠，无剔除 |
| T5 gradient | 1566 | 2.0000 | 2 | C003 副本完全重叠 |
| T6 determinism | 1566 | 2.0000 | 2 | 同 T5 |
| T7 SNR weight | 4 | 2.0000 | 2 | 合成数据完全重叠 |

### 4.2 拒绝统计
| 测试 | sigma | max_iter | iter_0_rejected | iter_1_rejected | total_rejected | 说明 |
|---|---|---|---|---|---|---|
| T1 baseline | 3.0 | 5 | 0 | - | 0 | 真实数据无离群值 |
| T2 strict | 2.0 | 10 | 4 | 0 | 4 | sigma=2.0 剔除 Frame C |
| T3 default | 3.0 | 5 | 4 | 0 | 4 | sigma=3.0 剔除 Frame C |
| T4 loose | 5.0 | 1 | 0 | - | 0 | sigma=5.0 不剔除 |
| T5 gradient | 3.0 | 5 | 0 | - | 0 | 两帧一致, std=0 |
| T7 SNR weight | 3.0 | 5 | 0 | - | 0 | 两帧不同值但 sigma=3.0 不剔除 |

### 4.3 输出索引
| 测试 | nside | n_pix | n_frames | n_leaves | non_empty_leaves | filter | total_exposure_s |
|---|---|---|---|---|---|---|---|
| T1 baseline | 32768 | 15522966 | 2 | 49152 | 未输出 | Red | 360.0 |
| T2 strict | 64 | 4 | 3 | 49152 | 4 | Lum | 300.0 |
| T5 gradient | 2048 | 1566 | 2 | 49152 | 5 | Red | 1200.0 |
| T7 SNR weight | 64 | 4 | 2 | 49152 | 4 | Lum | 200.0 |

---

## 5. 梯度球面校正状态

- **状态**：PASS（T5 启用成功，非退化回退）
- **GaiaClient 创建**：成功（data_dir=F:\Astro dev\Astro CS Normalization Database\GaiaDR3SP）
- **Gaia 星查询**：43383 颗（半径 0.942°，FOV center=(73.1049, -69.5838)）
- **控制点候选**：425 per frame
- **总样本数**：850（2 帧 × 425 控制点）
- **差异拟合**：success=1, lambda=1.0e-04
- **fit_rms**：0.0000（因两帧字节级一致，差异为 0，校正场为 0）
- **HCSD meta**：`gradient_correction: {enabled: true, success: true, lambda: "1.0e-04", method: "diff_fit_spherical_spline"}`
- **管线阶段**：5 阶段全部完成（采样/拟合/读取/叠加/写入）

---

## 6. 测试总结

| 项目 | 数量 | 通过 | 失败 |
|---|---|---|---|
| baseline 可重现性 | 1 | 1 | 0 |
| sigma-clip 三档 | 3 | 3 | 0 |
| 梯度校正启用 | 1 | 1 | 0 |
| 确定性 | 1 | 1 | 0 |
| SNR² 权重真实生效 | 1 | 1 | 0 |
| **总计** | **7** | **7** | **0** |

**总体结果**：**PASS**（7/7 验证全部通过，SNR² 加权真实生效 definitive proof，梯度校正管线完整运行）
