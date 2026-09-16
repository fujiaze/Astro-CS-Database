# memory.md — astrocs.p2.upm（P2-UPM-DOC 冻结）

- 任务: P2-UPM-DOC（MODULE_MIGRATION_MATRIX P2-UPM 行，owner
  SA-P2-U21，2026-09-10）——合同冻结层，不改生产源码，不 commit。
  本目录 `lib/algorithms/upm/` 三件套（README r1 + module.yaml +
  memory.md）由 P2-UPM-DOC 建立。
- 落位: `lib/algorithms/upm/`（本目录）。`lib/algorithms/coverage/` 一目录一套三件套
  已被 P2-COV（astrocs.p2.coverage）占用（lib/algorithms/coverage/README.md r1），
  不可覆盖；按 `lib/algorithms/integration/`（P2-INT-DOC，其按 `lib/algorithms/coverage/hips_p2/`
  P2-HIPS-DOC 先例）→ `lib/algorithms/rejection/`（P2-REJ-DOC）→
  `lib/algorithms/sampling/`（P2-SAMP-DOC）先例新建迁移目标目录，仅合同
  文件、无源码、不与 legacy 目录重叠。生产源引用不搬家。
- 矩阵权威（P2-UPM 行，禁止编造）: owner=SA-P2-U21、
  module_id=astrocs.p2.upm、target_dll=astrocs_p2_upm.dll、
  legacy_paths="lib/algorithms/coverage upm sources"、depends_on_int=
  **P2-SAMP-INT;CPU-005**、science_specific_acceptance="control
  points;photometric surface basis/regularization/gauge;fit+apply+
  persist+reload;known plane recovery;overlap seam residual;
  ill-conditioning"。
- MOD ID 决策: registry 两行 MOD-astrocs-phase2-upm-fit /
  MOD-astrocs-phase2-upm-apply（domain 内两个 descriptor）；
  module.yaml 单 manifest id 取 MOD-astrocs-phase2-upm-fit，apply
  对应 ID 于文件头注释与 README §fit/apply descriptor 现状声明。
- 本任务 ID 决策（唯一方案，避免与矩阵/registry 占位冲突）:
  ALG=ALG-P2-UPM-IMPL-001（docs/algorithms/PHASE2_UPM_IMPL.md，
  P2-UPM-DOC 新建，SA-B 负责；兼承接 ALG-UPM-001 权威推导
  （docs/algorithms/UPM_SOLVER.md）与 ALG-UPM-CONTROL-IVAR-001
  control-ivar 权重子面）——registry 页 upstream 现占位
  ALG-P2-UPM-001/002（astrocs.phase2.upm-fit/apply.md 头部），ID
  风格与 ALG-P2-SMP-001（phase2_samp 先例）同族，取 -IMPL-1 避免
  与占位 ALG-P2-UPM-001/002 冲突；descriptor 占位 ID 不入合同。
- 生产源锚（grep/read 实测，2026-09-10）: lib/algorithms/coverage/src/upm.cpp
  （1565 行）+ lib/algorithms/coverage/include/astro/phase2/upm.h（184 行，
  唯一权威签名头）。P2ControlObservation h:31-57（control_variance/
  control_ivar 冻结注 :43-50、ivar 弃用注 :40-42、snr_available
  :51-54）；P2ModelInfo h:60-68（model_hash[65] :67）；P2UpmBuildConfig
  h:71-92（15 字段）；权重 rc=2 冻结注 h:126-133；单帧区 Laplacian
  延拓 h:93-99；materialize_dense 重复声明 h:154-156/:173-175
  【DISP-001】；cpu_workers OpenMP 漂移注 h:89-91【DISP-002】。
  P2ControlNode sampler.h:77-83（control_id/tile_ipix/gx/gy/ra_deg/
  dec_deg/leaf_ipix）。
- upm.cpp 锚（实测）: build_impl :212（默认值修补 :224-244:
  huber_delta=1.345、max_iterations=100、tolerance=1e-6、
  sigma_floor=1e-3、zero_anchor_weight=1e-3、smoothing_lambda=0.0
  默认关、use_ivar_weight=1、control_reliability=1.0、cpu_workers=1）；
  compute_raw std::thread 池 :515-559（worker-local tsums 按 tid
  升序合并，D1 worker 数无关）；IRLS w 更新 :615-637；gauge（每
  连通分量参考帧=分量内最小 frame_id，C=0）:651/:789-829（component_
  ref_frame upm.h:85）；无观测几何节点 sentinel SIZE_MAX :216/
  :417-421 不参与数据图/gauge；p2_upm_save :940-1006（format
  astrocs-upm-v2 :947，json 序列化 controls/cell_index/C 稀疏行
  :972-1002，唯一 AIO aio_upm_write_sparse :1003-1005）；p2_upm_open
  :1008（format 校验 :1027）。导出符号 16 个: p2_upm_build :929 /
  p2_upm_build_geo :934 / p2_upm_save :940 / p2_upm_open :1008 /
  p2_upm_info :1233 / p2_upm_calibrate_block :1240 /
  p2_upm_evaluate_c :1271 / p2_upm_raw_weight :1292 /
  p2_upm_normalized_weights :1325 / p2_upm_geometry_hash :1346 /
  p2_upm_component_gauges :1369 / p2_upm_materialize_dense_n :1390 /
  p2_upm_materialize_dense :1518 / p2_upm_dense_info :1523 /
  p2_upm_dense_read_block :1542 / p2_upm_close :1559。
- rc 语义锚: 0=ok；1=参数错/IO 错/未知 frame_id（calibrate_block
  :1249-1252、evaluate_c 未知帧 NaN :1271-1276、dense_read_block
  :1546-1550 显式失败禁回退 frame0）；raw_weight 2=production 缺
  control_ivar（h:126-133）；dense_read_block 2=stale-cache
  （source_hash 不匹配）。geometry_hash :1346-1367（order/grid/
  cell/cell 拓扑/邻接，不含 SNR/quality/support——权重变化不得
  改变几何 hash）。materialize_dense_n kChunk=16 :1407、并行段
  :1479-1497；无 #pragma omp（2026-09-10 实测 0 处）。
- 消费链: lib/phase2_session/p2_session.cpp（282 行）:155/:195
  cpu_workers 传 host->budget.max_workers、upm_build 段 :180-233
  （persist_upm+upm_save_path :222-233）；【DISP-003】upm 覆盖键仅
  {max_iterations,huber_delta,smoothing_lambda}（:196-204），
  zero_anchor_weight/tolerance 等无 config 键（schema 扩展归
  P2-SESSION-IMPL）。apply 面消费=lib/algorithms/coverage/tools/stage2.cpp 经
  p2_upm_calibrate_block :927/:1272（无独立 apply 段）；AIO 后端
  lib/infrastructure/aio/src/aio_upm.cpp（aio_upm_write_sparse :66、
  dense 格式 astrocs-upm-dense-v2 :223/:407）。descriptor 占位
  （不改码）: lib/infrastructure/scheduler/src/module_adapters.cpp:599-632
  p2_upm_fit_descriptor（:599-613，module_id=astrocs.phase2.upm-fit、
  ports samples in → upm_model out 可选【DISP-004】、占位 sci_id=
  SCI-P2-UPM-001/alg_id=ALG-P2-UPM-001/data_id=DATA-P2-SMP/
  api_id=API-P2-001/test_id=TEST-P2-UPM-001）与
  p2_upm_apply_descriptor（:614-632，module_id=
  astrocs.phase2.upm-apply、upm_model in 必选 → corrected out、
  占位 sci_id=SCI-P2-UPM-002/alg_id=ALG-P2-UPM-002/
  data_id=DATA-P2-CAL/test_id=TEST-P2-UPM-002）。编排层词汇，
  P2-XX-INT 对齐；真实数据流=fit 产模型经 persist → apply reload；
  映射声明 SCI-P2-UPM-001/002⇒SCI-UPM-001 = SA-B 写于
  PHASE2_UPM_IMPL.md §映射节，占位 ID 不入合同。
- known_defects（登记不改码）:
  - DISP-P2UPM-001: upm.h:154-156 与 :173-175
    p2_upm_materialize_dense 重复声明（复制粘贴遗留，C++ 合法非
    ODR 违例，整改归 P2-UPM-IMPL）。
  - DISP-P2UPM-002: upm.h:89-91 cpu_workers 注释漂移（"仅
    P2_ENABLE_OPENMP 且>1 时并行" vs 实现纯 std::thread 段
    （upm.cpp:515-559/:615-637/:1479-1497）无 OpenMP 条件，头注释
    与实现不符）。
  - DISP-P2UPM-003: p2_session.cpp:196-204 upm 覆盖键仅
    {max_iterations,huber_delta,smoothing_lambda}，
    zero_anchor_weight/tolerance 等无 config 键（schema 扩展归
    P2-SESSION-IMPL）。
  - DISP-P2UPM-004: descriptor 端口语义占位（fit 行 upm_model 标
    可选输出、apply 行标必选输入，module_adapters.cpp:599-632，
    真实数据流=fit 产模型经 persist→apply reload，由 P2-XX-INT
    对齐）。
- 验证锚（相邻证据，既有测试，引用不冒认，非 TEST-P2-UPM-001/002）:
  tests/api/test_upm_recovery_oracle.py（4 test）、
  tests/api/test_upm_parallel.py（3 test）、
  tests/unit/p2_upm_synthetic_test.cpp（synthetic gate）、
  tests/backend/test_p2002_parallel_upm.py。可执行
  TEST-P2-UPM-001（fit）/TEST-P2-UPM-002（apply）均 MISSING
  （P2-UPM-TEST 建立）；登记面=DORMANT MISSING；设计冻结面=
  ALG-P2-UPM-IMPL-001 TEST-DESIGN 节（SA-B 写）。
- 后续任务锚: P2-UPM-IMPL=迁移到独立 astrocs_p2_upm.dll（C ABI
  adapter/plan-execute-cancel-inspect/ThreadLease 接线；DISP-001/002
  整改）；P2-UPM-TEST=建立可执行 TEST-P2-UPM-001/002 与
  oracle/负面/串并行/资源测试；P2-SESSION-IMPL=upm config 键
  schema 扩展（DISP-003）；P2-UPM-INT / P2-XX-INT=descriptor 占位
  词汇与 manifest/registry 对齐（DISP-004，不改科学合同）。
- 边界: 禁改 docs/science/ 既有文件（PHASE2_UPM.md FROZEN T106）；
  禁改生产源码/测试/lib/algorithms/coverage、lib/algorithms/integration、lib/algorithms/rejection、
  lib/algorithms/sampling、lib/algorithms/coverage/hips_p2 既有文件；禁 git add/commit/push；
  run/local/ 产物不提交。
---

## 追加：V6 IMPL-P2-UPM-001（2026-09-15，Wave 5）

> 本节由 V6 任务 \`IMPL-P2-UPM-001\` 追加，记录 V6 目标态乘法/加性分离实现；
> 不改写上文 P2-UPM-DOC 历史合同文本。上位：\`ALG-P2-SURF-UPM.md\`
> （\`ALG-P2S-UPM.1..8\`）、\`docs/contracts/v6/frozen/*\`、\`docs/algorithms/v6/frozen/*\`。

- 新增公共面（\`lib/algorithms/coverage/include/astro/phase2/upm.h\`）：
  \`P2UpmMaObservation\` / \`P2UpmMaConfig\` / \`P2UpmMaInfo\` +
  \`p2_upm_ma_build/info/solution/component_of_frame/component_of_control/\`
  \`component_ref_frame/param_cov/c_out/provenance/close\` +
  \`p2_upm_control_variance\`。实现位于 \`lib/algorithms/coverage/src/upm.cpp\`
  （匿名 namespace helpers + \`extern "C"\` 段）。
- 模型：\`y_k(p) = g_k * s(p) + b_k\`（\`ALG-P2S-UPM.1\`）；g/b 分别估计，
  禁止把乘法尺度藏进加性场（原引「宪章 §6.3」已废止；现行 = `docs/science/UNCERTAINTY_AND_COVARIANCE.md`）。空间加性场 b_k(x) 的数据面
  表示属 OPEN-P2S-02（不自行定值），本实现为帧级 b_k。
- overlap graph：frame-control 二分图连通分量（确定性边序 union-find）；
  gauge 每分量 ref=min(frame_id)，\`g_ref=1, b_ref=0\`（\`ALG-P2S-UPM.2\`）。
- 秩：\`sigma_i/sigma_max > 1e-10\`（\`FZ-AP2S-RANK-RTOL\`）；σ 由 Hestenes
  一步 Jacobi 直接对 W^{1/2}J 求得（避免 (JᵀJ) 平方丢失小奇异值精度）。
- 条件数：\`kappa = cond_2(D^-1 (JᵀWJ) D^-1) <= 1e6\`（\`FZ-AP2S-KAPPA-MAX\`）。
- min_frames=2（\`FZ-AP2S-UPM-MINFRAMES\`）：单帧分量默认 rc=5；显式
  additive-only 降级（\`allow_additive_only_single_frame=1\`）后 g≡1、b_ref=0。
- 参数协方差：\`C_theta=(JᵀWJ)^-1\`（gauge 消除子空间）；
  \`C_out = C_stat + J_out C_theta J_outᵀ\`（\`FZ-FORMULA-COV-PROP\`）；
  禁止由权重/诊断量反推 variance。
- fail-closed rc：0 ok / 1 参数 / 2 control_ivar / 3 秩亏 / 4 kappa /
  5 min_frames / 6 共享系统项按独立 / 7 k_corr provenance / 8 非有限解。
- k_corr：\`control_variance = k_corr*(pi/2)*sigma_bg^2/N_retained\`
  （\`ALG-P2S-UPM.7\`）；k_corr=1.0（忽略相关）→ rc=2；非域内值须提供
  固定种子 MC run id（DI-04 未复跑标定前仅域内 1.4，\`FZ-PROV-KCORR-VALUE\`）。
- 证据：\`run/v6/IMPL-P2-UPM-001/\`（Oracle \`oracle/upm_ma_oracle.py\` +
  \`anchors.json\`；基线 \`logs/compile_run.log\`；mutation \`logs/mutations.log\`；
  CMake 注册结构验证 \`cmake_check/\`）。
- 登记（只登记不裁决）：k_corr 标定脚本/固定种子 MC 复跑=DI-04（OPEN）；
  乘法尺度生产数据面/schema=OPEN-P2S-02（OPEN）；跨帧完整 C 的低秩/
  相关核表示=OPEN-P2S-03（OPEN）。
- 上文 "不处理乘性尺度差（已撤销，SCI-UPM 非目标）" 为 V5/历史加性
  模型口径；V6 目标态以 \`ALG-P2S-UPM.1\` 新增乘法 g_k 并强制乘加分离，
  该历史句在 V6 范围内被取代（本条只登记）。

