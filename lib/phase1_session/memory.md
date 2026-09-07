# lib/phase1_session — 模块记忆（P1-SESSION-DOC）

> 生命周期：P1-SESSION-DOC（本文档建立）→ P1-SESSION-IMPL（7-stage 段
> 补齐与整改）→ P1-SESSION-TEST/INT。追加式日志，不删改历史段落。

## 2026-09-07 · P1-SESSION-DOC 装配合同冻结（CONTRACT_READY）

### 任务

- 控制包任务 P1-SESSION-DOC（SA-P1-R19）：冻结 Phase1 assembly 合同与
  README——只描述装配与本阶段 DAG，不定义校准/星点/WCS/噪声/drizzle
  公式；列全节点、端口、artifact schema、外部输入；每节点指向既有
  module ID/entry/TEST；不含 Phase2/3 edge；不信任旧 README（本目录
  原无 README/module.yaml，本次新建）。

### 源码实测要点（行号锚逐条 grep，写文档前复核）

- p1_session.cpp（372 行）四段编排：io_read(:168-192) → calibrate
  (:195-280) → cosmetic(:283-317) → io_write(:319-330)；算法委托仅
  `ac_calibrate_frame`(:243) 与 `ac_correct_frame`(:294，master 实参
  nullptr→DISP-COS-009 恒等 pass)。
- 五导出 C API（p1_session.h:16-28）+ `astrocs::phase1::last_error`
  （p1_session.cpp:368-371）；create 校验 host struct_size/ABI（:91-93）；
  validate 键集 input_lights/output_dir 必需 + master_*/cosmetic/
  dark_optimization 可选（:115-143）；run 校验 async_io_depth∈{0,1,2}
  （:152）；线程注入 `ac_set_num_threads(budget.max_workers)`（:162-165）；
  取消=文件粒度（io_read :177-181）/帧粒度（calibrate :228-231、cosmetic
  :289）；manifest kind=astrocs_phase1_session（:98），stage 记
  name/status/files/frames/per_frame/artifacts（:170-192/:277-279/:274-275/
  :333-334）；inspect 输出 host alloc（:349-357）。
- registry：p1_session 五函数经 P1Api（module_adapters.cpp:693-700）被
  astrocs.phase1.calibration（:254，:728-735）+ p1_more[] 7 子模块
  （cosmetic :411 / star-psf :430 / wcs-platesolve :450 / photometry
  :469 / noise-snr :489 / drizzle :508 / writer :527，注册循环 :755-770）
  工厂委托。SessionModule.execute 先 validate 后 run（:193-201）+
  ThreadLease 租借（:156-162）+ provider=baseline（:170）。
- 构建：根 CMakeLists.txt:448-452 STATIC astrocs_phase1_session；
  :496/:514/:530 编入主可执行。
- 如实差距：API-P1-001（docs/api/PHASE1_API_V1.md FROZEN）冻结 7-stage
  序列 vs 现状 4 段——README §3 声明，不宣称 session 完成 7-stage；
  descriptor 占位 ALG-002/004/005 不作冻结依据（P1-PSF-DOC 先例）。

### 产物

- 本目录三件套：README.md（assembly 合同 10 节，双装配线 A/B 描述）、
  module.yaml（11 号标准 §4 manifest；dll_name=MISSING——迁移矩阵无
  P1-SESSION 行，不设独立 DLL；entrypoint=p1_session_run 真实符号）、
  memory.md（本文件）。
- docs/contracts/DATA_SEMANTICS.md 追加 §16（DATA-P1-SESSION：config
  键集/host services/manifest schema/artifact 命名与 dtype）。
- docs/contracts/PUBLIC_API.md 追加 API-P1-SESSION 节（五入口符号/生命周期
  时序/错误码映射/并发合同）。
- docs/contracts/INDEX.yaml 新增 DATA-P1-SESSION / API-P1-SESSION 条目
  （不设 ALG-P1-SESSION：assembly 无新算法推导，ALG 引用既有冻结 ALG；
  归档映射表 ALG-P1-SESSION-001 为 P1-001 旧词汇，不进现行 INDEX）。
- docs/modules/phase1_session.md 新建（模块页摘要，指向 co-located 合同）；
  docs/modules/registry/astrocs.phase1.session.md 新建（手写合同页；
  gen_module_readmes.py 以 module_adapters.cpp 为源会生成 calibration 等
  8 页，本页 registry 无 descriptor 源，重生成时须排除/保留）。
- docs/traceability/TRACEABILITY_MATRIX.json 新增
  MOD-astrocs-phase1-session 行（JSON/CSV 经 gen_traceability_csv.py
  同步重生成）。

### 验收锚

- 五项基线：check_traceability_matrix rc=0（基线 warns=4：conformance-noop
  EVID-BLD-003 / providers-cpu EVID-CPU-001 / services-io API-IO-STREAM-V1
  与 EVID-IO-001 的 REF_OUT_OF_SCOPE——不得新增）；pytest
  tests/traceability/test_traceability_matrix.py 全过；check_contract_graph
  rc=0；check_doc_index rc=0；自包含 selfcheck ALL PASS。
- 禁改：lib 生产源码（p1_session.cpp/.h 零 diff）、docs/science/、根科学
  公式；禁止运行 v19r3_traceability.py 与 gen_module_readmes.py。
