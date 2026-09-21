# 任务：CLEAN-401 死代码与退役实现处置

## 目标
按 ENGINEERING_SPEC §2 处置全部退役/未接入实现：生产路径外代码直接删除；确需暂留的加统一注释块（是什么/为什么/现状/删除或接入条件/权威依据）。最高设计 §8.2 的阶段内命名块内存管线在此任务接线落地，随后删除旧编排目录。

## 权威依据
- ENGINEERING_SPEC.md §2（历史实现处置）、§9（仓库整洁）
- ASTROCS_DESIGN.md §8.2（阶段内 PipelineFrame 命名块内存管线、阶段间落盘）
- GAP_AUDIT G2-1..G2-3、G3 相关项

## 改动范围（文件域，互斥）
- 允许改：`lib/pipeline/`、`lib/orchestrator/`（或现状对应目录）、`lib/algorithms/**/v6*`、`lib/algorithms/coverage/**`（psfsw 残留分支）、噪声模型 B 文件、`star_detection/wrapper_phase1/star_detector.cpp` 第三 σ 估计器、相关 CMake/注册表/测试
- 禁止改：docs（DOC 任务域）、aio（CLEAN-403/FIX-401 域）、phase3 接线（FIX-402 域）

## 步骤
1. 全仓扫描候选：未被任何生产 target/注册表引用的源文件、被注释代码块、含 deprecated/retired/v6/legacy/old 标记的符号、无测试无文档的模块；出清单（文件:行 + 现状分类）；
2. **命名块内存管线接线**：把已实现未接入的编排能力（lib/pipeline/orchestrator 约 1.6 万行，先审计其与最高设计 §8.2 的符合度）接入 scheduler 生产路径——阶段内 PipelineFrame 命名块（AioBlock：形状/类型/单位/可缺性冻结）传递、阶段间磁盘产品；接线后用现有 Phase 内 Pipeline 测试与合成全链对拍数值等价；
3. 接线验证通过后删除旧 orchestrator/v6 编排目录与 p3_v6_export 等 v6 家族（V6 合同层若 DOC-402 保留为设计档案，代码侧不保留生产不可达实现）；
4. psfsw 残留分支（coverage.cpp 约 376/425/430 行）：统一对象已是 13 个，删除残留分支；
5. 噪声模型 B（实验证明偏差 +104%~231%）删除文件、注册表项、测试与文档引用（文档由 DOC-402 配合）；
6. 第三 σ 估计器（star_detector.cpp 约 30-70 行，增益 +71.8%）：用合成数据复核增益与偏差，成立则接入并补测试/文档，不成立则删除并注释结论去向；
7. v6 SIN 内核容差问题转 FIX-406，不在本任务删（若内核随 v6 家族删除，确认 export TAN 路径不依赖它）；
8. 每项删除在任务回执列清单（路径 + 行数 + 依据）；CMake、MODULE_MAP、产品清单、测试同步移除；
9. 全量构建 + ctest + `ci --all` 全绿。

## 验收门（可机器复跑）
- [ ] 机器扫描：生产 target 内无未引用源文件；无被注释的旧逻辑块；保留件 100% 有统一格式注释块（新增 `retired-code` 检查器，红绿双向测试）；
- [ ] 阶段内命名块管线在三阶段生产路径实际执行（运行图 trace 佐证），与接线前合成结果数值等价（冻结容差）；
- [ ] 噪声模型 B、psfsw 残留、v6 家族符号全仓 grep 为零（档案文档除外）；
- [ ] 删除清单回执完整；ctest 全绿、零 waiver；
- [ ] 二进制体积/符号表无退役实现残留（nm 抽查）。

## 禁止
- 不删 testdata、实验数据、docs/science 公式；
- 不用"先注释留着以后看"代替二选一处置；
- 不在数值等价验证前删除旧编排。
