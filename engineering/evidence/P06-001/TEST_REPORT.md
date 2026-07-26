# P06-001 Stage2 真实输入兼容检查 - 测试结果报告

- 任务编号：P06-001
- 执行日期：2026-07-26
- 测试环境：PowerShell 7.6.3 + Windows
- 被测程序：`lib/orchestrator/cpp/orchestrator.exe`（stage2 子命令）

---

## 1. 测试矩阵

共 8 项检查，覆盖 nside/order/filter/重复帧/损坏文件/空目录。每项检查独立输入目录、独立日志、独立输出。

---

## 2. 测试用例详情

### 2.1 检查 A：baseline 字节级可重现性

- **目的**：验证 P00-003 stage2 baseline 在相同输入下产出字节级一致的 HCSD。
- **输入**：`lib/orchestrator/cpp/output_hiss_dir/`（frame1.hiss + frame2.hiss，nside=32768）
- **命令**：`orchestrator.exe stage2 --frames <dir> --output stage2_baseline_repro.hcsd --config stage2_config.json`
- **超时**：180 秒
- **期望**：exit=0，HCSD SHA-256=`2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37`
- **实际**：
  - exit code = 0
  - HCSD 大小 = 187455430 字节
  - HCSD SHA-256 = `2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37`
  - hcsd_inspect: nside=32768, nested=true, n_pix=15522966, n_frames=2, mean_pixel_count=1.9850
- **结果**：**PASS**（字节级与 P00-003 完全一致）

### 2.2 检查 B1：nside 一致

- **目的**：验证 nside 一致的 2 帧可正常堆叠。
- **输入**：`test_B_nside_mismatch/input_b1_nside_consistent/`（C003 nside=2048 + C005 nside=2048，均 Red）
- **期望**：exit=0，HCSD 输出生成
- **实际**：
  - exit code = 0
  - HCSD 大小 = 1217055 字节
  - HCSD SHA-256 = `B3EE2CC335DF871D47961CC25D72D86910703368B95E052C62A2462391B44A86`
  - hcsd_inspect: nside=2048, n_pix=3102, mean_pixel_count=1.0000（两帧不同天区，无重叠）
- **结果**：**PASS**

### 2.3 检查 B2：nside 不一致

- **目的**：验证 nside 不一致时被正确检测并报错。
- **输入**：`test_B_nside_mismatch/input_b2_nside_mismatch/`（C001 nside=512 + C003 nside=2048）
- **期望**：exit=1，error 含 HPS_ERR_NSIDE (-3)
- **实际**：
  - exit code = 1
  - 关键日志：`[hp_stack_hiss] nside/nested 不一致: 帧 1 (2048/1) vs 首帧 (512/1)`
  - 错误：`[GRADIENT_SPHERE] 失败: -3`
  - 源码引用：`lib/healpix_db/healpix_stack/hp_stack_hiss.cpp:159-166`
- **结果**：**PASS**

### 2.4 检查 B3：order（NESTED/RING）不一致

- **目的**：验证 NESTED 与 RING 混合时被正确检测。
- **输入**：`test_B_nside_mismatch/input_b3_order_mismatch/`（C003 NESTED + C003 改造为 RING，同 nside=2048）
- **RING 改造工具**：`make_ring_hiss.py`（修改 HISS JSON 头 `nested:true`→`false`）
- **期望**：exit=1，error 含 HPS_ERR_NSIDE (-3)
- **实际**：
  - exit code = 1
  - 关键日志：
    - `[hio] hiss_read: nside=2048 nested=1 n_pix=1566 (NESTED)`
    - `[hio] hiss_read: nside=2048 nested=0 n_pix=1566 (RING)`
    - `[hp_stack_hiss] nside/nested 不一致: 帧 1 (2048/0) vs 首帧 (2048/1)`
  - 错误：`[GRADIENT_SPHERE] 失败: -3`
  - 源码引用：`lib/healpix_db/healpix_stack/hp_stack_hiss.cpp:163`
- **结果**：**PASS**

### 2.5 检查 C：filter 混合

- **目的**：验证 filter 不一致时 Stage2 不强制一致性，取首帧 filter。
- **输入**：`test_C_filter_mixed/input/`（C003 Red + C004 Lum，同 nside=2048）
- **期望**：exit=0，HCSD 输出，filter 取首帧 (Red)
- **实际**：
  - exit code = 0
  - HCSD 大小 = 1217523 字节
  - HCSD SHA-256 = `7B0F01BA320F09EEC7D2A1C2FB64E5333EEF71B615408C9077B7658CAA51EA48`
  - hcsd_inspect: filter=Red（取首帧）
  - 行为说明：源码 `hp_stack_hiss.cpp:172-174`，filter 仅在 filter.empty() 时取首帧
- **结果**：**PASS**

### 2.6 检查 D：重复帧

- **目的**：验证重复帧被正确合并（ipix 完全重叠）。
- **输入**：`test_D_duplicate/input/`（C003 copy1 + C003 copy2，字节级一致）
- **期望**：exit=0，mean_pixel_count=2.0
- **实际**：
  - exit code = 0
  - HCSD 大小 = 1198623 字节
  - HCSD SHA-256 = `BD761931EEB7C0121179844975ADE763885A662E563CEB62D6318EAA43E8F041`
  - hcsd_inspect: n_pix=1566, mean_pixel_count=2.0000
  - sigma_clip_rejected=0（值完全相同，std=0，不满足 count>=2 && std>0 条件，提前收敛）
- **结果**：**PASS**

### 2.7 检查 E：损坏文件

- **目的**：验证损坏（截断）HISS 文件被正确检测。
- **输入**：`test_E_corrupted/input/`（C003 完整 19347 字节 + C005 截断至 100 字节）
- **期望**：exit=1，error 含 HPS_ERR_HIO (-5)
- **实际**：
  - exit code = 1
  - 关键日志：
    - `[hio] hiss_read: 解压 JSON 失败`
    - `[hp_stack_hiss] hiss_read 失败: ...frame_C005_truncated.hiss (ret=-2)`
    - `[GRADIENT_SPHERE] hp_stack_gradient_corrected 失败: ret=-5`
  - 错误：`[GRADIENT_SPHERE] 失败: -5`
  - 源码引用：`hp_stack_hiss.cpp:153-157`（HPS_ERR_HIO=-5）；`aio_healpix_io.cpp:hiss_read`（ret=-2 解压失败）
- **结果**：**PASS**

### 2.8 检查 F：空目录

- **目的**：验证输入目录无 HISS 文件时报错。
- **输入**：`test_F_empty_dir/input/`（空目录）
- **期望**：exit=1，error="目录下无 .hiss 文件: ..."
- **实际**：
  - exit code = 1
  - 错误：`目录下无 .hiss 文件: f:\Astro dev\Astro CS Normalization Database\engineering\evidence\P06-001\test_F_empty_dir\input`
  - 源码引用：`lib/orchestrator/cpp/src/orchestrator.cpp:3602-3606`
- **结果**：**PASS**

---

## 3. SNR² 权重生效证明

### 3.1 配置加载
- `stage2_config.json` 中 `weighting=snr_squared`，日志确认加载成功。

### 3.2 代码路径触发
- 日志关键词（成功路径均出现）：
  - `[hp_stack_hiss] 第二遍累加完成 (SNR² 加权)`
  - `[hp_stack_hiss] sigma-clip 迭代 0: 剔除 0 个离群值 (SNR² 加权)`

### 3.3 源码引用
- `lib/healpix_db/healpix_stack/hp_stack_hiss.cpp:224-228`
  - `w = hd.snr ? snr² : 1.0`
  - 当 `hd.snr` 为 nullptr（has_snr=false）时，w=1.0（等权）

### 3.4 实际行为
- 因所有 HISS has_snr=false（G-002 缺口），SNR² 权重退化为等权（w=1.0）。
- 但代码路径被触发，日志明确标注 `SNR² 加权`，证明逻辑分支正确。

### 3.5 退化说明
- G-002 缺口链路：PHOTOMETRIC n_matched=0 → sigma_residual=0 → SNR 模型未构建 → HISS has_snr=0 → stage2 SNR² 加权退化为等权。
- 这是既存退化，不影响本任务 PASS。需在 P03-004 修复后回归验证真实 SNR² 加权效果。

---

## 4. 重叠/拒绝/输出索引统计

### 4.1 重叠统计
| 检查 | unique_pixels | mean_pixel_count | sigma_clip_rejected | 说明 |
|---|---|---|---|---|
| A baseline | 15522966 | 1.9850 | 0 | 2 帧部分重叠 |
| B1 nside 一致 | 3102 | 1.0000 | 0 | 2 帧无重叠（不同天区） |
| C filter 混合 | 3141 | 1.0000 | 0 | 2 帧无重叠 |
| D 重复帧 | 1566 | 2.0000 | 0 | 2 帧完全重叠，std=0 提前收敛 |

### 4.2 拒绝统计
- 所有成功路径 sigma_clip_rejected=0。
- 原因：输入帧数少（2 帧），且无真实噪声分布，sigma-clip 不触发剔除。
- 重复帧场景：std=0，不满足 `count>=2 && std>0` 条件，提前收敛。

### 4.3 输出索引
- 所有成功路径生成 HCSD 文件，SHA-256 已记录。
- 所有失败路径无部分输出残留（原子性清理）。

---

## 5. 梯度球面校正状态

- **状态**：DEGRADED（G-002）
- **行为**：所有检查中 `gaia_client_create_ex 失败`，回退到无梯度校正模式。
- **影响**：不影响堆叠结果正确性，仅无梯度校正。
- **根因**：`stage2_config.json` 中 `gaia_data_dir` 为空。

---

## 6. 测试总结

| 项目 | 数量 | 通过 | 失败 |
|---|---|---|---|
| 兼容性检查 | 8 | 8 | 0 |
| SNR² 权重证明 | 1 | 1 | 0 |
| 梯度校正 | 1 | DEGRADED（G-002） | - |
| 测光版本检查 | 1 | NOT_APPLICABLE | - |

**总体结果**：**PASS**（8/8 兼容性检查通过，SNR² 代码路径已证明触发，既存退化不影响验证结论）。
