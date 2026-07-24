# spec: 归档 GRADIENT_2D 节点 + stage1 重排为 7 节点

> 创建日期: 2026-07-18
> 任务类型: 工程重构
> 来源: 用户审阅 PROJECT_OVERVIEW.md 后纠正——GRADIENT_2D 不应在 stage1 做曲面拟合和图像亮度修正（那是 stage2 马赛克阶段的事），stage1 在 PSF 后只做测光坐标系校准（PHOTOMETRIC 已完成），然后直接算 SNR。

---

## 1. 背景

PROJECT_OVERVIEW.md 第3节描述 stage1 为 8 节点：READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/**GRADIENT_2D**/SNR/DRIZZLE，其中 GRADIENT_2D 节点调用 `gradient_2d_calibrate` 做"乘性梯度曲面拟合 + 图像校正（IRLS+Tukey+Ridge+LOOCV）"。

用户纠正：
- **stage1 不做曲面拟合和图像亮度修正**——那是 stage2 马赛克阶段的事
- **PSF 后只做测光坐标系校准**：通过 PSF 流量 vs Gaia 积分流量的线性拟合，把图像转换到测光坐标系（这已经是 PHOTOMETRIC 节点做的事——IRLS+Tukey 求全局 scale 并应用到图像）
- **然后直接算 SNR**：使用乘法模型 SNR(ra,dec) = SNR_phot × (IDW(snr_psf 控制点) / median_snr)，结合测光不确定度（sigma_residual）和 PSF SNR（snr_psf = (A-B)/mad）

结论：**GRADIENT_2D 节点应归档**，stage1 重排为 7 节点。

## 2. 范围

### 2.1 in scope（本次必做）

- **代码归档**：`lib/photometric_calib/cpp/gradient_2d/` 整目录移至 `lib/photometric_calib/archive/gradient_2d/`
- **orchestrator 代码修改**：
  - `cpp/include/orchestrator.h`：删除 `PipelineStageV2::GRADIENT_2D = 5` 枚举值；stage 序号重排（SNR 6→5, DRIZZLE 7→6, GRADIENT_SPHERE 8→7, STACK 9→8）；删除 `run_stage_gradient_2d` 声明
  - `cpp/include/dll_loader.h`：删除 `ModuleId::GRADIENT_2D` 枚举值
  - `cpp/src/dll_loader.cpp`：删除所有 GRADIENT_2D 相关 case（name/dll_name/subdir/init/load/unload/case 分支）
  - `cpp/src/orchestrator.cpp`：删除 `#include "gradient_2d.h"`；删除 `stage_name_v2` 中的 GRADIENT_2D case；删除 `run_stage1` 中 DLL 加载列表的 GRADIENT_2D；删除 `run_stage_gradient_2d` 函数实现（约 2300-2550 行）；删除 `run_v2_with_timing` 调用；删除"为 GRADIENT_2D 预计算 Gaia F_syn 数组"辅助函数（如确认仅此处使用）
  - `cpp/Makefile`：删除 `-I../../photometric_calib/cpp/gradient_2d/include` 编译路径
  - `configs/stage1_config.json`：从 modules 列表删除 "gradient_2d"
- **文档修改**：
  - `docs/PROJECT_OVERVIEW.md`：第3节 stage1 改 7 节点 + 归档标注；第4节数据流删除 GRADIENT_2D 行；第6节模块清单标注归档；第7节依赖图移除 gradient_2d；第10节待办更新（移除/调整相关条目）
  - `docs/DESIGN_IMPL_GAP.md`：新增 GAP-021 记录归档决策
  - `docs/PIPELINE_OVERVIEW.md`：同步更新（若描述了 GRADIENT_2D 或 8 节点结构）
  - `lib/photometric_calib/memory.md`：记录归档决策
  - 根 `memory.md`：记录本次进度（若存在）
- **编译验证**：orchestrator.exe 编译通过，无 GRADIENT_2D 引用残留

### 2.2 out of scope（本次不做）

- stage2 GRADIENT_SPHERE 节点设计核查（用户明确"本次只处理 stage1"）
- GAP-006/008/014/015/018/019/020 等其他未处理 GAP
- photometric_calib C API 内部实现修改（PHOTOMETRIC 阶段已正确做 IRLS+Tukey 求 scale 并应用到图像）

## 3. 技术方案

### 3.1 stage 序号重排（删除 GRADIENT_2D 后）

| 旧 stage | 旧节点 | 新 stage | 新节点 |
|---|---|---|---|
| 0 | READ_FITS | 0 | READ_FITS |
| 1 | CALIBRATE | 1 | CALIBRATE |
| 2 | PLATESOLVE | 2 | PLATESOLVE |
| 3 | PSF | 3 | PSF |
| 4 | PHOTOMETRIC | 4 | PHOTOMETRIC |
| 5 | **GRADIENT_2D**（删除） | — | — |
| 6 | SNR | **5** | SNR |
| 7 | DRIZZLE | **6** | DRIZZLE |
| 8 | GRADIENT_SPHERE | **7** | GRADIENT_SPHERE |
| 9 | STACK | **8** | STACK |

### 3.2 PHOTOMETRIC 阶段职责确认

PHOTOMETRIC 节点（stage 4）已经完成用户期望的"测光坐标系校准"：
- 输入：PSF 流量（psf 块）+ Gaia 积分流量 F_syn = ∫ S(λ)·T(λ)·Q(λ) dλ
- 算法：IRLS + Tukey biweight 稳健回归求全局 scale
- 输出：scale_factor（标量）+ sigma_residual（残差）+ 图像像素应用 scale（I_cal = I × scale）
- 写入 photo_stats 命名块供 SNR 阶段读取

无需新增节点替代 GRADIENT_2D。

### 3.3 SNR 模型保持不变

SNR(ra,dec) = SNR_phot × (IDW_球面(控制点, query) / median_snr)
- SNR_phot = 1/(ln10 × sigma_residual)：来自 PHOTOMETRIC 的测光不确定度
- 控制点 snr_psf = (A-B)/mad：PSF SNR
- 已符合用户期望"乘法模型结合测光不确定度和 psf snr"

### 3.4 代码归档策略

`lib/photometric_calib/cpp/gradient_2d/` → `lib/photometric_calib/archive/gradient_2d/`

保留全部历史代码（include/ + src/ + build.ps1），不删改文件内容，仅移动目录。后续 stage2 马赛克阶段若需要曲面拟合可参考。

## 4. 验收标准

- [ ] `lib/photometric_calib/archive/gradient_2d/` 存在且包含原 `cpp/gradient_2d/` 全部文件
- [ ] `lib/photometric_calib/cpp/gradient_2d/` 目录不存在
- [ ] orchestrator.h 中无 `GRADIENT_2D = 5` 枚举值，stage 序号已重排
- [ ] dll_loader.h/.cpp 中无 `ModuleId::GRADIENT_2D` 引用
- [ ] orchestrator.cpp 中无 `run_stage_gradient_2d` 函数、无 `#include "gradient_2d.h"`、无 `[GRADIENT_2D]` 日志
- [ ] Makefile 中无 gradient_2d/include 路径
- [ ] stage1_config.json 中 modules 列表无 "gradient_2d"
- [ ] orchestrator.exe 编译通过
- [ ] PROJECT_OVERVIEW.md 第3节 stage1 表格为 7 节点
- [ ] DESIGN_IMPL_GAP.md 新增 GAP-021
- [ ] PIPELINE_OVERVIEW.md 同步更新
- [ ] lib/photometric_calib/memory.md 记录归档决策

## 5. 回滚策略

- 代码归档：`mv archive/gradient_2d/ cpp/gradient_2d/` 即可恢复
- orchestrator 代码：git revert 本次 commit
- 文档：git revert 本次 commit

## 6. 风险与缓解

| 风险 | 缓解 |
|---|---|
| stage 序号重排可能影响检查点文件 | 检查点文件按 frame_name 存储阶段，未硬编码 stage 数字；如有影响，fresh_start 重跑 |
| 配置文件中 stage 序号引用 | 已核查 stage1_config.json 仅 modules 列表引用模块名，无 stage 数字 |
| orchestrator.exe 二进制兼容性 | 本次为破坏性重构，需重新编译；无外部消费者依赖 stage 数字 |
