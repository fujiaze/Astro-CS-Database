# lib/drizzle — 模块记忆（P1-DRZ-DOC）

> 生命周期: P1-DRZ-DOC（本文档建立）→ P1-DRZ-IMPL（落码）→ P1-DRZ-TEST
> （可执行测试）→ P1-DRZ-INT。追加式日志，不删改历史段落。

## 2026-09-07 · P1-DRZ-DOC 合同冻结（CONTRACT_READY）

### 任务

- 控制包任务 P1-DRZ-DOC：冻结合同与 README（函数/单位/dtype/shape/
  错误/并发/确定性如实登记，以源码为准，不信任旧 README）。
- 依据 MODULE_MIGRATION_TEMPLATE.md `<prefix>-DOC` 节执行；矩阵行
  P1-DRZ：module_id=astrocs.p1.drizzle、target=astrocs_p1_drizzle.dll、
  depends_on_int=P1-WCS-INT;P1-NOISE-INT;CPU-005。

### 产物

- 本目录三件套：README.md（模块合同 10 节）、module.yaml（11 号标准
  §4 manifest，module_status=CONTRACT_READY，entrypoint=MISSING）、
  memory.md（本文件）。
- docs/algorithms/DRIZZLE_GEOMETRY.md 重写：ALG-DRZ-001 逐公式源码锚定
  + TEST-DRZ-DESIGN-001（§9）+ DISP-DRZ-001..008（§10）。
- docs/contracts/DATA_SEMANTICS.md 追加 §11（DATA-P1-DRZ）。
- docs/contracts/PUBLIC_API.md 追加 API-DRZ-001 节。
- docs/modules/healpix_drizzle.md、docs/modules/registry/
  astrocs.phase1.drizzle.md 事实修订。
- docs/traceability/TRACEABILITY_MATRIX.{json,csv} P1-DRZ 行原地更新
  （七层 VERIFIED + EVID-MISSING，行序不变）。
- 源码事实采集底稿（不入库）：run/local/agent_p1_drz_doc/
  source_facts.md（11 节，全行号实测）。

### 源码核对结论（摘要，行号以 source_facts.md 为准）

- 核心公式与 SCI-DRZ-001 §5 一致（w=overlap_area/drop_area、
  sumFlux+=x·w、sumArea+=a、sumVarNum+=v·w²）；S_p=F_p/D_p 归一不在
  drizzle 层，在 astro_sphere_sink.cpp:100 + aio_hips_writer
  finalize_tile。
- SCI vs 源码差异 8 条（DISP-DRZ-001..008）：Girard→实为 Eriksson、
  值像素 NaN 静默跳过（vs SCI:96 传播语义）、pixfrac=0.0 API/引擎
  双轨、1e-3 切平面分支不存在、sip_order 注释/校验 ±1、TileLeaf
  注释 3 字段 vs 实际 4、方差锚点行号漂移、poly_clip legacy 零调用。
- 1/N 确定性：schedule(static) reduction + per-thread 累加器 + 按
  线程序合并（drizzle_engine.cpp:1670-1671,1762-1785）；ThreadLease
  全模块零命中（生产调度走 Runtime lease，CMakeLists.txt:379-382）。
- 生产接线现状：orchestrator DLL 通道（orchestrator.cpp:3256-3371）；
  registry descriptor（module_adapters.cpp:508-525）未接节点；
  p1_session 无 drizzle stage。入口=MISSING 如实登记。

### 纪律记录

- 未改动：生产源码（lib/** 源文件）、docs/science/**（SCI-DRZ-001
  只读引用）、TRACEABILITY_SPEC.md 示例行。禁止根据代码错误反向修改
  SCI——差异全部登记 DISP-DRZ-*，修复归 P1-DRZ-IMPL/INT 或 SCI 变更。
- 无 git commit/push（SubAgent 纪律；前台统一验证提交）。

### 验收

五项自检 rc（详见任务报告）：①check_traceability_matrix.py rc=0
（warns=基线 4 条不新增）；②pytest tests/traceability rc=0 全过；
③check_contract_graph.py rc=0；④check_doc_index.py rc=0；
⑤run/local/agent_p1_drz_doc/selfcheck.py ALL PASS；⑥git status 域
核查：生产源码与 docs/science 零改动。

### 待后续任务（不阻塞本任务）

- P1-DRZ-IMPL：astrocs_p1_drizzle.dll、ThreadLease 接线（替换 omp
  遗留通道）、DISP-DRZ 消化、pixfrac 边界统一、错误码集中化。
- P1-DRZ-TEST：TEST-DRZ-DESIGN-001 → 可执行 TEST-P1-DRZ-001。
- SCI 层候选变更（走 SCI 变更流程，不在本任务范围）：DRIZZLE.md:63
  Girard→Eriksson 措辞、:96 NaN 传播语义 vs 实测静默跳过、:98/:131
  锚点行号失效。
