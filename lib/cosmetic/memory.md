# cosmetic - 模块开发memory

## 模块职责
Phase1 坏点修复（cosmetic correction）：热/冷像素全局阈值检测
（median ± sigma·1.4826·MAD）、8 连通结构过滤、5×5 中值 / 4 方向 IDW
插值修复、修复计数输出。现行生产实现位于 lib/calibration
（cosmetic_corrector.cpp，经 ac_correct_frame(+_f64) 导出）；本目录
lib/cosmetic/ 是 astrocs.p1.cosmetic / astrocs_p1_cosmetic.dll 的
迁移目标（P1-COS-IMPL 落码）。

## 当前版本
- 版本号：0.11.0-alpha.2（module_version，随仓库 VERSION）
- 模块状态：CONTRACT_READY（P1-COS-DOC 冻结，2026-09-07）

## 关键决策记录
- **独立模块合同（不与 P1-CAL 合并）**：MODULE_MIGRATION_MATRIX.csv 将
  cosmetic 登记为独立模块（astrocs.p1.cosmetic，legacy_paths=
  "lib/calibration;lib/phase1_session"）；SCI 层共享 SCI-CAL-001
  （SCI §12 仅将坏点检测登记为 ALG-CAL-004 摘要），ALG 层独立冻结为
  ALG-COS-001..005（ALG-CAL-004 与之描述同一现行实现，重叠界定见
  COSMETIC_ALGORITHMS.md §0）。
- **生产调用现状如实登记（no fabrication of valid coverage）**：
  p1_session.cpp:294-307 cosmetic stage 传 master_dark/master_bias=
  nullptr → 检测全禁用、恒等 pass（DISP-COS-009）；检测从未在生产
  生效，母版接线归 P1-COS-IMPL/编排层。文档不美化该事实。
- **method=1 名义 bilinear 实为 4 方向 1/dist IDW**（DISP-COS-003）：
  作为冻结现状行为登记；修正需 SCI/控制包变更，DOC 阶段禁止改码。
- **threading_model=host_executor_lease** 为迁移目标合同值（11 号
  标准 §4）；现状 OpenMP 默认 team + ac_set_num_threads 全局 ICV 是
  迁移整改点（与 DISP-CAL-002 同根）。

## 进度日志

### 2026-09-07 P1-COS-DOC 冻结合同与 README（W1, SA-P1-COS）
- 任务: P1-COS-DOC（依赖 GOV-001/ARC-001/DATA-001/DOC-001 CLOSED；
  与 P1-CAL-DOC 产物共存）。
- 产物: 新建 docs/algorithms/COSMETIC_ALGORITHMS.md（ALG-COS-001..005
  逐公式源码锚定 + TEST-COS-DESIGN-001 测试设计与冻结容差
  rtol=1e-6/atol=1e-7 + DISP-COS-001..011 缺陷清单）；
  docs/contracts/DATA_SEMANTICS.md §10（DATA-P1-COS）；docs/contracts/
  PUBLIC_API.md（API-COS-001，3 符号）；docs/modules/calibration.md
  与 docs/modules/registry/astrocs.phase1.cosmetic.md 事实修订；
  本 README 新建 + module.yaml 冻结（MOD-astrocs-phase1-cosmetic，
  CONTRACT_READY，entrypoint=MISSING，7 个生产符号）；
  TRACEABILITY_MATRIX json/csv 追加行 MOD-astrocs-phase1-cosmetic
  （SCI-CAL-001/ALG-COS-001/DATA-P1-COS/API-COS-001/SRC-COS-001/
  TEST-COS-DESIGN-001 → VERIFIED，EVID-MISSING 待 INT/验收）；
  docs/DOCUMENT_INDEX.yaml 登记新算法文档。
- 源码核对结论（一切以源码为准）:
  - 唯一生产实现 lib/calibration/src/cosmetic_corrector.cpp:61-265
    （ac::{filter_by_structure_size,detect_hot_pixels,
    detect_cold_pixels,interpolate_pixels,correct_frame}）+ C API
    ac_api.cpp:108-122,228-263；签名权威 astro_calibration.h:97-103,
    142-148。
  - median 偶数样本 = (hi+lo)*0.5f（hi=上中位，lo=下半区 max_element，
    cosmetic_corrector.cpp:34-43）；MAD = median(|x−med|)，
    σ=1.4826f·mad。
  - 8 连通过滤保留 <max_size 连通域（≥max_size 清零）；背景 label 0
    size=max_size 防全图误标（遗留通道泄漏 bug 已在现行实现修复）。
  - IDW: 4 正交方向步进跳过连续坏点，1/dist 反比加权，全坏/出界回退
    原值；median: 5×5 镜像反射边界，仅收好像素，空域回退原值。
  - ac_correct_frame_f64 内部降级 float32（ac_api.cpp:228-263）；
    AC_ERR_MEMORY/INTERNAL 定义但从未返回；无 extern "C" 异常屏障。
  - 缺陷清单 DISP-COS-001..011 登记（COSMETIC_ALGORITHMS.md §10）。
    **未改任何生产代码**（git diff lib/*.c/*.h/*.cpp 为空）。
- 验收: check_traceability_matrix rc=0、pytest traceability 全过、
  check_contract_graph rc=0、check_doc_index rc=0、符号存在性自检
  7/7 PASS；生产源码与 docs/science 零改动。
- 日志: run/local/agent_p1_cos_doc/。
