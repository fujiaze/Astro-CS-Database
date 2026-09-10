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
- registry（P1-001 后更新，2026-09-10）：8 个 Phase1 descriptor 工厂已
  改为 P1NodeModule 唯一真实 operation 委托（module_adapters.cpp
  p1_nodes[]；子节点禁调 phase_session_run，ARCH-P0-001 整改）；
  P1Api/SessionModule 保留兼容面。complete 门：run 成功 status="partial"+
  availability 8 域（PROD-P0-001 Phase1 侧；p1_session.cpp run 成功路径）。
  测试锚：tests/unit/p1001_real_nodes_test.cpp + p1sess 五组（断言已迁移
  partial）。
- P1-001 attempt 2 三域真实化（2026-09-10 口径更新落地）：star-psf=
  sdet(lib/star_detector 生产源)+dpsf_fit_batch_f64(lib/dynamic_psf
  Moffat4 FP64，DATA-P1-PSF 携 psf_params:FLOAT64[N,9])；wcs=ipv 求解链
  (ipv_solve_from_memory_with_callback_d，sdet+gaia_client 句柄注入，
  Linux=源内 stub fail-closed 报平台限制、Windows=真实求解，缺求解参数
  DATA 拒绝)；writer=p1_stack.hiss→aio_hiss_inspect/read_tile_*→
  AstroSphereTileView→aio_hips_product_begin/write/finalize（NESTED 聚合
  IVOA 1.4 标准 512×512 HiPS，covered_area_model=support_x_A_cell，
  nside>=512 合同）。主链测试 7 节点（wcs 旁支平台化单测）+ 下游零调用
  负向用例（口径⑨）。构建面：astrocs_p1_dpsf/astrocs_p1_ipv/
  astrocs_p1_sdet 三个 STATIC + GSL(gsl gslcblas) + AIO_ENABLE_HEALPIX
  编译定义；cli/CMakeLists.txt compat 面同步（源集+include+链接 gsl）。
  CLI 键白名单 parser.cpp 补 drizzle/wcs。
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


## 2026-09-10 · P1-001 真实节点化与 complete 门（attempt 2）

### 任务

- 控制包任务 P1-001（ASTROCS-CONSTITUTION-ALIGNMENT-V1，ARCH-P0-001 +
  PROD-P0-001 Phase1 侧整改）：B 线 registry 8 个 Phase1 节点唯一真实
  operation 化 + complete 门 fail-closed。

### 现状变更要点

- module_adapters.cpp：P1Api 8 工厂委托 → P1NodeModule 8 域唯一真实
  operation 委托（p1_nodes[]；子节点零 phase_session_run 调用）。
  operation/entry 名与 runtime/pipeline/module_ports.registry.json 一致。
- p1_session.cpp：run 成功 manifest status="partial" + availability 8 域
  （链不完整禁写 complete，complete 语义冻结待链完整后按门禁恢复）。
- cli/runtime_client.cpp：phase1 IR 由单 cal 节点扩为 cal→cosmetic 链
  （cosmetic enabled=false 时 0 帧直通，产物语义不变）。
- cli/commands.cpp：phase1 成功路径补接 write_run_graphs（对齐 phase3
  先例；observed trace 现含 phase1 节点观测）。
- 根 CMakeLists.txt（白名单越界登记）：astrocs_phase1_session PUBLIC
  链接追加 4 个 lib/phase1 静态库（wcs/phot/noise/stars，源零修改只读
  委托）+ astrocs_module_adapters 追加 astrocs_drizzle（hp_drizzle_run
  直链）；V7 残留追加块零触碰。
- 测试：tests/unit/p1001_real_nodes_test.cpp 新增（RED 锚定→GREEN；
  operation/entry/artifact/call_count=1/complete 门/负向/确定性）；
  p1sess 五组 + p1_session_manifest 断言迁移 partial。

### 如实边界

- rt005_registry / rt009_node_trace / p1_noise_adapter（F-AIO-001）与
  tools/arch/check_thread_budget.py（3× module_entry.cpp 未登记宏）、
  tools/check_p1_symbol_map.py（docs/refactor 旧路径断链）为 BASE 预存
  失败/断链，本任务零触碰，登记 findings 移交前台。
- wcs-platesolve 为配置/初始 WcsTan 标定语义（真实求解器接线归各 IMPL
  任务）；star-psf PSF 特性=StarSource fwhm_px/ellipticity（不接
  dpsf_fit_batch，A 线 DPSF 生产链另行承载）。
