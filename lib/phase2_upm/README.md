# astrocs.p2.upm — Phase2 联合加性光度模型模块（P2-UPM）

> P2-UPM-DOC（2026-09-10，SA-P2-U21）新建模块页。合同三件套落位
> `lib/phase2_upm/`（README r1 + module.yaml + memory.md，CONTRACT_READY，
> entrypoint=MISSING）——迁移目标目录按 `lib/phase2_samp/`（P2-SAMP-DOC，
> 其按 `lib/phase2_int/` → `lib/phase2_rej/` → `lib/hips_p2/`
> P2-HIPS-DOC 先例链）先例新建；`lib/phase2/` 一目录一套三件套已被
> P2-COV（astrocs.p2.coverage）占用（lib/phase2/README.md r1，不可
> 覆盖）。生产源 `lib/phase2/src/upm.cpp`（1565 行，根 CMakeLists
> astrocs_phase2 静态库成员 :337-346，:338 编入 upm.cpp）+ 唯一权威
> 签名头 `lib/phase2/include/astro/phase2/upm.h`（184 行）；会话消费方
> `lib/phase2_session/p2_session.cpp`（upm_build 段 :180-233），apply
> 面消费 `lib/phase2/tools/stage2.cpp`（p2_upm_calibrate_block
> :927/:1272，无独立 apply 段）；持久化后端
> `lib/astro_image_io/src/aio_upm.cpp`（aio_upm_write_sparse :66，
> dense 格式 astrocs-upm-dense-v2 :223/:407）。

## 身份与合同

- MOD ID：`MOD-astrocs-phase2-upm-fit` 与 `MOD-astrocs-phase2-upm-apply`
  （registry 两行，domain 内两个 descriptor；本域含 fit+apply 两个
  descriptor，故一个域两个 MOD ID，module.yaml 单 manifest 以
  MOD-astrocs-phase2-upm-fit 为 id，apply 对应 ID 见其文件头注释与
  本文 §fit/apply descriptor 现状）；module_id：`astrocs.p2.upm`
  （MODULE_MIGRATION_MATRIX P2-UPM 行；descriptor 编排层占位
  astrocs.phase2.upm-fit / astrocs.phase2.upm-apply 由 P2-XX-INT
  对齐，不反向作冻结依据）；dll_target：`astrocs_p2_upm.dll`
  （合同值，尚未存在，迁移归 P2-UPM-IMPL）。
- owner SA-P2-U21；depends_on_int=P2-SAMP-INT;CPU-005；
  legacy_paths="lib/phase2 upm sources"（均以
  MODULE_MIGRATION_MATRIX.csv P2-UPM 行为权威）。
- 合同链：SCI-UPM-001（docs/science/PHASE2_UPM.md，FROZEN T106
  2026-08-23，集合 SCI-UPM-001..010 + SCI-UPM-WEIGHT-001 +
  SCI-UPM-PERSIST-001，页头明示模块 "phase2 (upm/sampler)"；
  descriptor 占位 SCI-P2-UPM-001/002⇒SCI-UPM-001 映射声明于
  docs/algorithms/PHASE2_UPM_IMPL.md §映射节，占位 ID 不入合同）
  → ALG-P2-UPM-IMPL-001（docs/algorithms/PHASE2_UPM_IMPL.md，
  P2-UPM-DOC 新建，兼承接 ALG-UPM-001 权威推导（docs/algorithms/
  UPM_SOLVER.md）与 ALG-UPM-CONTROL-IVAR-001 control-ivar 权重子面）
  → DATA-P2-UPM（DATA_SEMANTICS §25，fit 侧）+ DATA-P2-COR
  （DATA_SEMANTICS §26，apply 侧 corrected 输出）/
  API-P2-UPM-001（PUBLIC_API "Phase2 UPM 公共消费面"节）+
  API-P2-001（PHASE2_API_V1 编排上游，引用不改动）→
  TEST-P2-UPM-001（fit）/ TEST-P2-UPM-002（apply）（登记面=
  DORMANT MISSING；可执行测试归 P2-UPM-TEST，不冒认；设计冻结面=
  ALG-P2-UPM-IMPL-001 TEST-DESIGN 节）；arch=ARCH-001。

## 职责（摘要，权威=ALG-P2-UPM-IMPL-001）

### fit 侧（build/save）

- 多帧控制点观测（P2ControlObservation，upm.h:31-57）上构建唯一
  联合加性光度模型 UPM：huber IRLS 迭代（默认 huber_delta=1.345、
  max_iterations=100、tolerance=1e-6，:212-244 默认面），
  relative光度消逐帧背景/零点差；单帧区由 Laplacian 延拓
  （upm.h:93-99 语义）。
- 科学权重 production 公式唯一实现：raw_w = quality_factor ×
  control_ivar（p2_upm_raw_weight :1292，h:126-133 冻结；
  control_ivar≤0/非有限 → rc=2 显式 INVALID，禁止静默回退
  support/SNR；ALG-UPM-CONTROL-IVAR-001 子面）。
- gauge 冻结：每连通分量参考帧=分量内最小 frame_id，C=0
  （:651/:789-829；component_ref_frame upm.h:85；无观测几何节点
  sentinel SIZE_MAX :216/:417-421 不参与数据图/gauge；
  p2_upm_component_gauges :1369 查询）。
- 持久化：p2_upm_save :940-1006（format astrocs-upm-v2，json 序列化
  controls/cell_index/C 稀疏行 :972-1002，唯一 AIO
  aio_upm_write_sparse :1003-1005）；p2_upm_open :1008（format 校验
  :1027）；模型 hash=SHA-256（model_hash[65]，upm.h:67）。

### apply 侧（calibrate/evaluate/dense）

- p2_upm_calibrate_block :1240：per-frame 批量 corrected =
  raw − C(frame,leaf)；未知 frame_id → rc=1（:1249-1252）。
- p2_upm_evaluate_c :1271：单 leaf 求值；未知 frame_id 返回 NaN
  （显式不可用，:1271-1276，禁止用 frame 0 参数伪装有效结果）。
- dense cache：p2_upm_materialize_dense_n :1390（kChunk=16 :1407）/
  p2_upm_dense_info :1523 / p2_upm_dense_read_block :1542（未知帧
  :1546-1550 显式失败禁回退 frame0；rc=2=stale-cache，source_hash
  不匹配）。
- 权重/几何查询面：p2_upm_normalized_weights :1325、
  p2_upm_geometry_hash :1346-1367（order/grid/cell/cell 拓扑/邻接，
  不含 SNR/quality/support——权重变化不得改变几何 hash）。

## 明确非职责

- 不做控制点采样（上游 P2-SAMP 域，DATA-P2-SMP 输入）；不做
  coverage union 计算（P2-COV 域）；不做最终加权积分与排异判定
  （SCI-INT/SCI-REJ）；不处理乘性尺度差（已撤销，SCI-UPM 非目标）；
  不产 per-pixel 科学场产品（dense cache 为光度模型 C 场缓存，
  非积分产品）。

## 实现现状与迁移差距

- CONTRACT_READY 语义：实现存在且生产在用（upm.cpp 1565 行 +
  upm.h 184 行，行号 grep/read 实测），模块化迁移（独立
  astrocs_p2_upm.dll / C ABI adapter / plan-execute-cancel-inspect /
  ThreadLease 接线）由 P2-UPM-IMPL 执行。
- dll_target=astrocs_p2_upm.dll 尚未存在：现状构建=根
  CMakeLists.txt:337-346 astrocs_phase2 STATIC（:338 编入
  upm.cpp）+ :454 astrocs_phase2_session STATIC（:458 链接
  astrocs_phase2）。
- entrypoint=MISSING：registry 入口未接（registry 页
  docs/modules/registry/astrocs.phase2.upm-fit.md /
  astrocs.phase2.upm-apply.md 已存在， descriptor 现状为占位词汇，
  见下节）。
- threading_model=host_executor_lease 为迁移目标合同值（11 号标准
  §4 值域）；现状实现=worker 数取 cfg.cpu_workers（Runtime lease
  唯一来源，p2_session.cpp:155/:195 传 host->budget.max_workers；
  模块内 std::thread 池，无 hardware_concurrency，无 #pragma omp
  ——2026-09-10 实测 0 处）；并行段=compute_raw :515-559、IRLS
  w 更新 :615-637、materialize_dense_n :1479-1497；规约=worker-local
  tsums 按 tid 升序合并（:515-559，D1 worker 数无关）→
  determinism=fixed_reduction_order。

## 唯一生产源符号清单（lib/phase2/src/upm.cpp，16 个导出符号）

| 符号 | 行号 | 说明 |
|---|---|---|
| p2_upm_build | :929 | obs-only 构建（兼容入口） |
| p2_upm_build_geo | :934 | 全几何节点构建（含无观测 sentinel 节点） |
| p2_upm_save | :940 | 稀疏模型持久化（astrocs-upm-v2，唯一 AIO） |
| p2_upm_open | :1008 | 重开（format 校验 :1027） |
| p2_upm_info | :1233 | P2ModelInfo 查询（model_hash[65]） |
| p2_upm_calibrate_block | :1240 | per-frame 批量 corrected 输出 |
| p2_upm_evaluate_c | :1271 | 单 leaf C 求值（未知帧 NaN） |
| p2_upm_raw_weight | :1292 | production 权重公式唯一实现（rc=2 缺 control_ivar） |
| p2_upm_normalized_weights | :1325 | per-control 归一化权重 |
| p2_upm_geometry_hash | :1346 | 几何拓扑 SHA-256（不含权重输入） |
| p2_upm_component_gauges | :1369 | 每分量参考帧查询 |
| p2_upm_materialize_dense_n | :1390 | dense cache 物化（workers 显式版） |
| p2_upm_materialize_dense | :1518 | dense 物化（既有签名，转调 _n） |
| p2_upm_dense_info | :1523 | dense cache 信息查询 |
| p2_upm_dense_read_block | :1542 | dense 读块（rc=2 stale-cache） |
| p2_upm_close | :1559 | 释放 |

构建入口 build_impl :212（默认值修补面 ：224-244，huber_delta=1.345 /
max_iterations=100 / tolerance=1e-6 / sigma_floor=1e-3 /
zero_anchor_weight=1e-3 / smoothing_lambda=0.0 默认关 /
use_ivar_weight=1 / control_reliability=1.0 / cpu_workers=1）。

## fit/apply descriptor 现状（占位词汇声明）

- lib/core/src/module_adapters.cpp:599-632：p2_upm_fit_descriptor
  （:599-613，module_id=astrocs.phase2.upm-fit，ports samples in →
  upm_model out（fit 行标可选输出，:609-612））与
  p2_upm_apply_descriptor（:614-632，module_id=
  astrocs.phase2.upm-apply，ports upm_model in（apply 行标必选输入）
  → corrected out）；占位 sci_id=SCI-P2-UPM-001/002、
  alg_id=ALG-P2-UPM-001/002、data_id=DATA-P2-SMP/DATA-P2-CAL、
  api_id=API-P2-001、test_id=TEST-P2-UPM-001/002。
- 【DISP-P2UPM-004】端口语义占位与真实数据流不符：真实数据流=
  fit 产模型经 persist → apply reload（upm_model 并非 fit 的
  optional 产物直连 apply 输入）；对齐归 P2-XX-INT，占位词汇
  （含 module_id/alg_id/data_id 端口拼写）不反向作冻结依据。
- 本 manifest 与本页冻结的合同词汇：module_id=astrocs.p2.upm、
  data=DATA-P2-UPM（fit 侧）/DATA-P2-COR（apply 侧）、
  api=API-P2-UPM-001；descriptor data_id 端口词汇 DATA-P2-SMP/
  DATA-P2-CAL 为上游域合同引用。

## 测试现状

- 可执行 TEST-P2-UPM-001（fit）/ TEST-P2-UPM-002（apply）均
  MISSING（P2-UPM-TEST 建立，不冒认）；登记面=DORMANT MISSING；
  设计冻结面=ALG-P2-UPM-IMPL-001 TEST-DESIGN 节（SA-B 写）。
- 现状相邻证据（既有测试，引用不冒认，不是 TEST-P2-UPM-001/002）：
  tests/api/test_upm_recovery_oracle.py（4 test）、
  tests/api/test_upm_parallel.py（3 test）、
  tests/unit/p2_upm_synthetic_test.cpp（synthetic gate）、
  tests/backend/test_p2002_parallel_upm.py。

## 链接

- README/module.yaml/memory.md：`lib/phase2_upm/`（本目录）
- SCI：docs/science/PHASE2_UPM.md（SCI-UPM-001，FROZEN T106
  2026-08-23，零改动）
- ALG：docs/algorithms/PHASE2_UPM_IMPL.md（ALG-P2-UPM-IMPL-001，
  P2-UPM-DOC 新建）；承接 docs/algorithms/UPM_SOLVER.md
  （ALG-UPM-001）
- DATA：docs/contracts/DATA_SEMANTICS.md §25（DATA-P2-UPM）/
  §26（DATA-P2-COR）
- API：docs/contracts/PUBLIC_API.md API-P2-UPM-001；
  API-P2-001（docs/api/PHASE2_API_V1.md，编排上游）
- 模块页：docs/modules/registry/astrocs.phase2.upm-fit.md /
  astrocs.phase2.upm-apply.md
- ARCH：ARCH-001

## 已知限制（DISP-P2UPM，登记不改码，整改归 P2-UPM-IMPL/TEST/SESSION-IMPL）

| ID | 摘要 | 源锚 |
|---|---|---|
| DISP-P2UPM-001 | upm.h:154-156 与 :173-175 p2_upm_materialize_dense 重复声明（复制粘贴遗留，C++ 合法非 ODR 违例） | upm.h:154-156/:173-175 |
| DISP-P2UPM-002 | upm.h:89-91 cpu_workers 注释漂移（"仅 P2_ENABLE_OPENMP 且>1 时并行" vs 实现纯 std::thread 段、无 OpenMP 条件） | upm.h:89-91 vs upm.cpp:515-559/:615-637/:1479-1497 |
| DISP-P2UPM-003 | p2_session upm 覆盖键仅 {max_iterations,huber_delta,smoothing_lambda}，zero_anchor_weight/tolerance 等无 config 键 | p2_session.cpp:196-204 |
| DISP-P2UPM-004 | descriptor 端口语义占位（fit 行 upm_model 标可选输出、apply 行标必选输入，与 persist→reload 真实数据流不符） | module_adapters.cpp:599-632 |
