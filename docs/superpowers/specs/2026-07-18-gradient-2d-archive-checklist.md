# checklist: 归档 GRADIENT_2D 节点 + stage1 重排为 7 节点

> 配套 spec: 2026-07-18-gradient-2d-archive.md

## A. 代码归档

- [ ] A1. 创建 `lib/photometric_calib/archive/` 目录（如不存在）
- [ ] A2. 移动 `lib/photometric_calib/cpp/gradient_2d/` → `lib/photometric_calib/archive/gradient_2d/`
- [ ] A3. 验证归档目录包含原全部文件（include/gradient_2d.h + src/gradient_fitter.h + src/gradient_2d_api.cpp + src/image_corrector.h + src/image_corrector.cpp + build.ps1）

## B. orchestrator 代码修改

- [ ] B1. `cpp/include/orchestrator.h`：
  - 删除 `GRADIENT_2D = 5` 枚举行
  - 重排注释：SNR 5, DRIZZLE 6, GRADIENT_SPHERE 7, STACK 8
  - 删除 `bool run_stage_gradient_2d(TaskResult& result);` 声明及注释
- [ ] B2. `cpp/include/dll_loader.h`：删除 `GRADIENT_2D,` 枚举行及注释
- [ ] B3. `cpp/src/dll_loader.cpp`：
  - 删除 `case ModuleId::GRADIENT_2D: return "GRADIENT_2D";`
  - 删除 `case ModuleId::GRADIENT_2D: return "gradient_2d.dll";`
  - 删除 `case ModuleId::GRADIENT_2D: sub = "lib/photometric_calib/cpp/gradient_2d/"; break;`
  - 删除 `init(ModuleId::GRADIENT_2D);`
  - 删除 `all_ok = load_module(ModuleId::GRADIENT_2D, lib_base_dir) && all_ok;`
  - 删除 `unload_module(ModuleId::GRADIENT_2D);`
  - 删除两处 `case ModuleId::GRADIENT_2D:` 分支（约 429/469 行）
- [ ] B4. `cpp/src/orchestrator.cpp`：
  - 删除 `#include "gradient_2d.h"`
  - 删除 `case PipelineStageV2::GRADIENT_2D: return "GRADIENT_2D";`
  - 删除 `run_stage1` 中 DLL 加载列表的 `ModuleId::GRADIENT_2D`（约 663/684 行）
  - 删除 `run_stage_gradient_2d` 整个函数实现（约 2334-2550 行）
  - 删除 `run_v2_with_timing(PipelineStageV2::GRADIENT_2D, ...)` 调用（约 2947-2948 行）
  - 评估"为 GRADIENT_2D 预计算 Gaia F_syn 数组"辅助函数（约 1045-1170 行）是否仅此处使用，若是则删除
- [ ] B5. `cpp/Makefile`：删除 `-I../../photometric_calib/cpp/gradient_2d/include` 行
- [ ] B6. `configs/stage1_config.json`：从 modules 数组删除 `"gradient_2d"` 行

## C. 编译验证

- [ ] C1. orchestrator.exe 编译通过（PowerShell 7 + mingw64）
- [ ] C2. `Grep "GRADIENT_2D|gradient_2d|run_stage_gradient_2d"` 在 lib/orchestrator/cpp/ 下无残留（README.md/memory.md 历史记录除外）

## D. 文档修改

- [ ] D1. `docs/PROJECT_OVERVIEW.md`：
  - 第3节 stage1 表格：删除 GRADIENT_2D 行，stage 5/6/7 重排为 SNR/DRIZZLE + stage2 重排 GRADIENT_SPHERE 7 / STACK 8
  - 第3节标题"两段流水线 10 节点"改"两段流水线 9 节点"
  - 第4节数据流：删除 `GRADIENT_2D → data(梯度校正后)` 行
  - 第6节模块清单：photometric_calib/cpp/gradient_2d 行标注"已归档至 archive/gradient_2d/"
  - 第7节依赖图：photometric_calib 框中删除 "+ gradient_2d" 标注
  - 第2节核心架构原则表："两段流水线 stage1 8 节点 + stage2 2 节点"改"7 节点 + 2 节点"
  - 第10节待办：检查是否需调整（GAP-014 涉及 stage1 节点拆分文档，本次部分修复）
- [ ] D2. `docs/DESIGN_IMPL_GAP.md`：新增 GAP-021 记录归档决策（含发现日期、类型、描述、影响、修复方式、批复意见、复核记录）
- [ ] D3. `docs/PIPELINE_OVERVIEW.md`：核查并同步更新（如描述了 8 节点或 GRADIENT_2D）
- [ ] D4. `lib/photometric_calib/memory.md`：追加归档决策记录
- [ ] D5. 根 `memory.md`：追加本次进度记录（若存在）

## E. 收尾

- [ ] E1. 更新 lib/orchestrator/memory.md 记录本次重构（不改历史条目，追加新条目）
- [ ] E2. 自检报告：列出所有修改文件 + 编译结果 + 残留检查结果
