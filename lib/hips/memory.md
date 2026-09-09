# lib/hips — 模块记忆（P1-HIPS-DOC）

> 生命周期: P1-HIPS-DOC（本文档建立）→ P1-HIPS-IMPL（落码）→ P1-HIPS-TEST
> （可执行测试）→ P1-HIPS-INT。追加式日志，不删改历史段落。

## 2026-09-07 · P1-HIPS-DOC 合同冻结（CONTRACT_READY）

### 任务

- 控制包任务 P1-HIPS-DOC：冻结合同与 README（函数/单位/dtype/shape/
  错误/并发/原子发布/rollback/tree hash 如实登记，以源码为准，不信任
  旧 README）。
- 依据 MODULE_MIGRATION_TEMPLATE.md `<prefix>-DOC` 节执行；矩阵行
  P1-HIPS：module_id=astrocs.p1.hips_writer、target=astrocs_p1_hips_writer.dll、
  legacy_paths="lib/healpix_db;lib/phase1_session"、
  depends_on_int=P1-DRZ-INT;IO-003。

### 产物

- 本目录三件套：README.md（模块合同 10 节）、module.yaml（11 号标准
  §4 manifest，module_status=CONTRACT_READY，entrypoint=MISSING）、
  memory.md（本文件）。
- docs/algorithms/HIPS_WRITER.md 新建：ALG-HIPS-001..005 逐公式源码
  锚定 + TEST-HIPS-DESIGN-001（§9）+ DISP-HIPS-001..008（§10）。
- docs/contracts/DATA_SEMANTICS.md 追加 §12（DATA-P1-HIPS）。
- docs/contracts/PUBLIC_API.md 追加 API-HIPS-001 节（9 符号现状 C API）。
- docs/modules/registry/astrocs.phase1.hips-writer.md 新建。phase3 registry
  页 `DATA-HIPS-001` 引用**经查非悬空**：该 ID 是既有自洽语义（matrix
  services-io 行 data=DATA-HIPS-001 VERIFIED、TRACEABILITY_SPEC.md:74
  示例、生产源码 lib/core/src/module_adapters.cpp:300/:323 端口表在用，
  Phase3 通用 HiPS 语义），不属本模块合同、不在本任务域——不改（改动
  会扩 diff 并触碰生产源码）；本模块用 DATA-P1-HIPS（§12），两者并存
  合法。
- docs/traceability/TRACEABILITY_MATRIX.{json,csv} 追加
  MOD-astrocs-phase1-hips-writer 行（七层 VERIFIED + EVID-MISSING，
  追加不重排）。
- docs/DOCUMENT_INDEX.yaml 登记 docs/algorithms/HIPS_WRITER.md 与
  registry 页；lib/hips/ 三件套入索引。
- 源码事实采集底稿（不入库）：run/local/agent_p1_hips_doc/
  （my_own_facts.md 主核 + subagent 侦察报告）。

### 源码核对结论（摘要，行号以实测为准）

- 唯一生产 writer=lib/astro_image_io/src/hips/aio_hips_writer.cpp（1222 行，
  合同头 aio_hips.h 9 符号）；编译于 astrocs_hips 静态库
  （CMakeLists.txt:298-309）；调用方=astro_sphere_sink.cpp:97（drizzle
  sink 合并后单线程写）+ stage2.cpp:592（Phase2）+ tests/unit/
  p1_hips_writer_test.cpp:55。lib/phase1_session 零 HiPS 引用（grep rc=1）；
  healpix_stack 系列函数全仓零调用（死代码化）；HISS 为独立中间容器
  （legacy_hiss_compare 开关封闭，HISS_VERIFY=CFG-002 关闭）。
- 归一公式与 DATA_SEMANTICS §4/§4a、DRIZZLE.md:130/:145 一致：
  signal=flux_sum/covered_area、support=min(area/A_cell,1)（>1 钳 1.0）、
  A_cell=4π/(12·nside²)（:413）、variance=var_num/area²、ivar=1/variance
  （有限+正值域）；无效→signal=NaN/support=0/variance=NaN（IEEE NaN，
  无 FITS BLANK）。全无效 variance tile rc=−5 不写文件。
- FITS tile 索引=nested_local_to_fits_index(i,9,512)（三处 scatter
  :464/:604/:809；共享 lib/common/healpix 权威 core，(511−x)·512+y，
  CDS Hipsgen 冻结，DATA_SEMANTICS §3）；tile 布局
  NorderK/Dir{ipix/10000}/Npix{ipix%10000}.fits（万进制分片，
  tile_rel_path :135-142）。
- hierarchy 聚合（:536-557,:664-679）：k=K−1..0 降序，z=((s<<18)|l)>>2(K−k)，
  AncestorAcc 逐 cell 确定性累加（f32 产品 float 累加——DISP-HIPS-009），
  无并行归约漂移；落盘 :791-885。
- MOC：UNIQ=4·4^moc_order+(c>>2(K−moc_order))（:781-785，SNR 同构）；
  moc_sky_fraction=moc_area_sr/4π；空 MOC 不写；moc_order<K 时低阶 UNIQ
  对自家 reader 无效（DISP-HIPS-005）。
- SNR Catalogue：ang2pix_nest(2^tile_order)（共享权威 :896-899）→ 逐 cell
  TSV（SNR-PREC-001 FP32 %.9g/FP64 %.17g round-trip）+ VOTable 1.3
  metadata.xml + hips_cat_nrows + hips_initial_ra/dec（真实中位数）。
- **原子发布/rollback 现状（matrix 专项如实登记）**：writer 层**无**
  事务语义——abort 仅 delete ps 不删文件（:1133-1137；aio_hips.h:151
  注释与实现不符，DISP-HIPS-001）；FITS/MOC/metadata 先 remove 后 create
  （:185-186/:251/:770）；finalize 中途失败已写子产品残留；manifest.json
  （:1086-1128）无 COMPLETE 状态字。原子发布语义层由 IO-003
  （docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md：临时写→fsync→
  fitsverify→sha256→原子 rename→manifest COMPLETE）在 Python 发布层
  承接，两合同对齐边界登记于 DATA-P1-HIPS §12.5。
- **tree hash/provenance 现状**：C++ writer 无整树哈希——仅 per-tile FITS
  DATASUM/CHECKSUM（fits_write_chksum :230，MOC :275）；sha256 清单在
  IO-003 发布层（runtime/io/hips_output_store.py）；properties 级
  provenance=creator_did/obs_*/prov_progenitor/ASTROCS_DRIZZLE_PIXFRAC/
  SCALE_ARCSEC（:727-736）+ frame_id 输入白名单含 signal/support tile
  像素与关键 properties（DATA_SEMANTICS §5，DATA-FRAME-ID-001）。
- SCI 覆盖缺口（不反向改 SCI）：无 SCI 文档定义 HiPS 写出合同（tile
  切分/properties/publish 协议零 SCI 覆盖）；唯一 SCI 锚=DRIZZLE.md:130
  （finalize_tile 方差）+:145（support 归一）。SCI 变更候选走 SCI 流程，
  不在本任务范围。

### 纪律记录

- 未改动：生产源码（lib/** 源文件）、docs/science/**（SCI-DRZ-001 等只读
  引用）、TRACEABILITY_SPEC.md。禁止根据代码缺陷反向修改 SCI——差异全部
  登记 DISP-HIPS-001..012（ALG-HIPS-001 §10），修复归 P1-HIPS-IMPL/INT
  或 SCI 变更流程。
- 无 git commit/push（SubAgent 纪律；前台统一验证提交）。
- r1 修订（P1-HIPS-DOC 修正轮）：HIPS_WRITER.md/README.md 整体重写——
  采纳二轮深度侦察行号锚（estsize :721、hips_initial_fov :745/:954、
  hips_status :718、prov_progenitor :729、remove :185、add_var :327-333、
  finalize rc :1036-1072、SNR :887-1005），UNIQ 公式按 :783 修正，
  DISP 清单 001..008→001..012（新增 009 f32 累加器、010 fits_str 截断/
  properties 无转义、011 hierarchy 空 tile/缓冲重分配、012 FIRSTPIX/
  LASTPIX 声明性头卡），补 SNR Catalogue 产品结构（TSV+VOTable+
  hips_cat_nrows+中位数 ra/dec）与 SNR-PREC-001 精度合同、
  astro_sphere_sink.cpp:97 调用点、make_hips_fixture.py:168/:170 占位值
  提醒；module.yaml 注释同步 DISP 引用。

### 验收

五项自检 rc（run/local/agent_p1_hips_doc/ 日志在案，2026-09-07 实测）：
①check_traceability_matrix.py rc=0（modules=27 errors=0 warns=4，warns 逐
条与基线 4 条相同）；②pytest tests/traceability rc=0（4 passed）；
③check_contract_graph.py rc=0（contracts=37）；④check_doc_index.py rc=0
（DOC_INDEX_PASS）；⑤run/local/agent_p1_hips_doc/selfcheck.py rc=0
（PASS 87 FAIL 0）；⑥git status 域核查：lib/** 生产源码与 docs/science/**
零改动（git diff 空列表断言入 selfcheck），traceability 验收日志不暂存。
另：git 工作区存在**预先存在**的非本任务改动（AGENTS.md、artifacts/*、
控制包 zip 删除等），非 P1-HIPS-DOC 所为，未触碰。

### 待后续任务（不阻塞本任务）

- P1-HIPS-IMPL：astrocs_p1_hips_writer.dll、adapter/plan/execute/cancel/
  inspect、ThreadLease、CFITSIO mutex 包装（DISP-HIPS-006）、abort 事务化
  （DISP-HIPS-001/004，或显式降级注释）、错误码集中化（DISP-HIPS-007）、
  hips_estsize/hips_initial_fov 真实估算（DISP-HIPS-002）、moc_order
  严格化（DISP-HIPS-005）、f32 hierarchy 累加器（DISP-HIPS-009）、
  fits_str 截断/properties 转义（DISP-HIPS-010）、hierarchy 空 tile/缓冲
  复用（DISP-HIPS-011）。
- P1-HIPS-TEST：TEST-HIPS-DESIGN-001 → 可执行 TEST-P1-HIPS-001。
- P1-HIPS-INT：与 IO-003 发布合同对齐接线；tree hash 归属（writer 侧
  或发布侧）裁决记录。
- SCI 层候选变更（走 SCI 变更流程，不在本任务范围）：HiPS 写出合同
  SCI 化（tile 切分/properties/publish 协议现零 SCI 覆盖，ALG-HIPS 承接）。

## 2026-09-09 · P1-HIPS-IMPL 交付（模块迁移完成）

### 任务

- 控制包任务 P1-HIPS-IMPL（queue 40, lock-P1-HIPS；依赖 P1-HIPS-TEST
  c19b4a59 / ABI-005 / RT-004 / DATA-003）：迁移到独立模块（C ABI adapter、
  typed artifact、plan() 真实 work_units）。验收关键词: DLL builds;
  scope scientific_change=false。

### 交付物

- lib/hips/CMakeLists.txt: SHARED 目标 astrocs_p1_hips_writer —— 生产闭包
  从源 PIC 重编译 2 TU（aio_hips_writer.cpp + healpix_core.cpp）+ cfitsio 源
  （ASTROCS_CFITSIO_SOURCES 相对根路径前缀变换）；version-script/DEF 唯一
  导出 astrocs_module_query_v1；C/CXX_VISIBILITY_PRESET hidden（Linux 面）。
- lib/hips/src/module_entry.cpp: C ABI v1 九操作 adapter
  （query/describe/validate_config/plan/create/execute/inspect/
  request_cancel/destroy）。单事务 op=write_product（product_begin → 逐 tile
  write_signal_support_tile/write_variance_tile → write_snr_points →
  set_drizzle_provenance → finalize，与 astro_sphere_sink.cpp:52-167 同构）。
  plan 真实 work_units（work_units=1 单事务 + nside/n_products/memory
  hierarchy_accumulator + io_out 估算，元数据推导禁空转）；host executor
  硬租约 acquire(1)（cpu_heavy，executor 缺失/acquire 失败 → ACS_ERR_BUDGET
  detail 105 禁无租约运行）；entry cancel + tile 间 cancel 安全点
  （DISP-HIPS-001: abort 不清理已写文件，处置归调用方/IO-003 层）；
  strbuf 两阶段（尺寸探测/BUFFER_TOO_SMALL）；manifest 输入顶层平铺 v1
  （base64 平面 native 字节序 + per-tile 位图 + SNR 六平面 SoA + provenance）。
- lib/hips/include/astrocs/hips/types.h + src/module_exports.map +
  src/astrocs_p1_hips_writer.def + module.yaml（交付态: entrypoint=
  astrocs_module_query_v1, node_operations=[write_product]）。
- tests/unit/p1_hips/adapter_test.c + adapter_entry_impl.cpp + tests/unit/
  CMakeLists.txt 注册块: hips_writer_adapter（9 case: direct reference/
  adapter 全生命周期/direct-vs-plugin 产物树逐文件 size 对拍/alloc_fail/
  schema_reject/budget 105×2/cancel_not_begun/strbuf 探针/dlsym 导出面探针）。
- 根 CMakeLists.txt: add_subdirectory(lib/hips) + RPATH foreach 追加。

### 关键事实（实证）

- 依赖闭包: writer TU 仅依赖 astrocs::healpix 两符号
  （nested_local_to_fits_index + ang2pix_nest, healpix_core.cpp）+ cfitsio 源；
  aio_fits/aio_log/aio_api/aio_healpix_io/hiss_*/aio_hips_reader/aio_upm
  零依赖剔除（g++ -fsyntax-only 实证）。
- 生产源零改动: lib/astro_image_io/** git diff 空 —— scientific_change=false。
  manifest.json/properties 无路径键（两目录 size 对拍成立）；FITS tile 数据
  位级一致（DATASUM 同值），字节级差异仅 cfitsio CHECKSUM/DATASUM 注释
  的 wall-clock 秒级时间戳（as-built 实测: diff 4 字节全在头部注释）。
- 导出面: nm -D --defined-only 唯一 T astrocs_module_query_v1；dlsym 九
  legacy aio_hips_* 全 NULL（ctypes + 测试 case8 双实证，主树 DLL 1.79MB）。
- 无 OMP: writer/healpix_core 零 #pragma omp，DLL 无 libgomp 依赖 →
  58d20223 OMP 教训本案不触发（测试 TU 亦无 OMP 符号）。

### 验证（run/local/agent_p1hips_impl/ 日志在案）

- 影子树（工作树快照隔离验证）: 构建 + ctest 七组（p1hips 6 + adapter）
  循环 5/5 全绿（loop_round1..5）；asan 树（ASTROCS_ENABLE_SANITIZERS=ON）
  构建 + 6/7 绿，p1hips_properties 失败为 TEST 域断言设计面: tree_digest
  全字节 FNV 含 cfitsio CHECKSUM 注释秒级时间戳，-O0 慢速跨秒必败
  （字节级实证见上），非本任务回归；科学数据位级零差异。
- 主树: STAR-TEST 注册块补齐后 configure 通过，ninja 构建 + ctest
  循环 5 轮 3/5 全绿; 2 轮 flaky：p1hips_performance parity4=0.24x
  （哨兵线 0.25, t1w≈0.10s 噪声主导; 根上复测 5/5 PASS parity4=0.26-0.29x）
  与 p1hips_properties 跨秒（同 asan 机理）。环境: /tmp tmpfs 7.9G 曾
  100% 满（历史调试残留）→ asan-repro 1.1G 迁移至
  run/local/agent_p1hips_impl/relocated_tmp/（symlink 保留路径）并
  remount size=24G 后测试面稳定。
- adapter 9 case 全 PASS（asan/主树双树实测; direct-vs-plugin 产物树
  文件集合相等 + 每文件 size 相等）。

### 待后续任务（不阻塞本任务）

- P1-HIPS-INT: integration descriptor/typed ports/调用计数/registry 注册
  （module.yaml 端口注册面同步归 INT）。
- TEST 域登记（P1-HIPS-TEST 域产物零改动，如实移交）: ①tree_digest 对
  绝对路径与 cfitsio CHECKSUM 注释时间戳敏感（properties determinism 断言
  在慢树/跨秒下 flaky，建议 digest 排除 CHECKSUM 卡或归一化时间注释）;
  ②performance parity4 下界 0.25 与实测 0.24-0.29 贴线（t1w 基线过小）,
  建议 t1w 增大 rep/规模或放宽 trend 线。
- DISP-HIPS-006 CFITSIO mutex 包装 / 002 estsize / 005 moc_order 严格化 /
  009 f32 accumulator / 010 fits_str / 011 缓冲复用: 未在本任务消化
  （ scientific_change=false 边界，保持登记）。

## 2026-09-10 · AIO-002 交付（原子发布与临时目录清理，模块事务面）

### 任务

- 控制包任务 AIO-002（ASTROCS-CONSTITUTION-ALIGNMENT-V1）：staging→校验→
  fsync→原子 promote；正常/取消/ENOSPC/kill 后无 partial；临时目录 RAII。
- 写域：lib/hips/ lib/hips_p2/ tests/。生产 writer（lib/astro_image_io）零改动
  （scientific_change=false；git diff 实证空）。

### 交付物

- lib/hips/include/astrocs/hips/publish.h：原子发布原语 v1 合同头（4 原语；
  aio_publish_status_v1 数值 0..15/70/71 与 aio_abi_v1.h aio_status 全域一致
  （_Static_assert 编译期对齐）；ASTROCS_HIPS_STAGE_BASENAME=".hips_staging.tmp"；
  故障注入 env=ASTROCS_HIPS_PUBLISH_FAULT 注册名
  p1_stage_create_fail/p1_fsync_fail/p1_promote_fail/p1_discard_noop/
  p1_stage_slow_write）。
- lib/hips/src/aio_publish.cpp：唯一实现（stage_create 兄弟 staging+残留自愈/
  stage_discard 递归删除幂等 RAII/tree_fsync 后序文件+目录 fsync（ENOSPC→
  DISKFULL 收敛点）/promote rename 整树原子+目标非空拒绝+父目录 fsync）。
  DLL 内部隐藏符号，唯一导出面 astrocs_module_query_v1 不变。
- lib/hips/src/module_entry.cpp：write_product 事务化——writer 写 staging；
  cancel 检查点抽取 hips_cancel_requested；发布门（fail/cancel → abort+discard
  → out_dir 无 partial；成功 → fsync 树 → promote → 输出 manifest 报 out_dir）；
  legacy 所有权修正：finalize 成功=句柄内部已释放（README §5），成功路径禁
  abort（旧代码成功路径 abort 是 double-free 隐患，本次实证修复——原语义
  finalize 后从未走 abort，未暴露）。诊断码 HIPS_ECODE_PUBLISH_STAGE=122/
  HIPS_ECODE_PUBLISH_REJECT=123（types.h）。
- lib/hips/CMakeLists.txt：aio_publish.cpp 编入 DLL；tests/unit/CMakeLists.txt：
  hips_publish_atomic_test 注册（units/atomic 两组 ctest）+ adapter 测试补
  aio_publish.cpp。
- tests/unit/p1_hips/publish_atomic_test.c：TEST-P1-HIPS-PUBLISH-001 可执行面
  （4 tile 确定性流；正向/注入必败/cancel/kill+自愈/非空目标拒绝/残留垃圾
  自愈/units 原语正负幂等）；tests/unit/p1_hips/adapter_test.c 补 hips_rm_rf
  幂等（重复 root 旧树清零——原子发布"非空目标拒绝"使旧残树合法拒发）。
- module.yaml：source_symbols 补 4 publish 原语 + test_ids 补
  TEST-P1-HIPS-PUBLISH-001；README §9 补 AIO-002 验证与限制注记。
- lib/hips_p2/：本任务零改动（P2 写编排 stage2.cpp 直写 out_hips 属
  DISP-P2HIPS-003 登记面，域外不改；见 lib/hips_p2/memory.md）。

### 关键决策（防后续误读）

- staging=兄弟目录（<parent>/.<base>.hips_staging.tmp）而非 out_dir 内嵌：
  rename(stage→out) 整树原子，out_dir 根要么完整旧态要么完整新树；且取消/
  失败 discard 不触碰 out_dir 本体（零态/旧态均收敛无 partial）。
- out_dir 根语义升级为"发布目标"：writer manifest.json（:1086-1128，无
  COMPLETE 字）作为树内完成标记随树原子出现；树级 COMPLETE manifest +
  sha256 tree_hash 归 IO-003 Python 层（DATA-P1-HIPS §12.5 边界不变）。
- ENOSPC 语义：writer 写文件走 stdio 缓冲，ENOSPC 在 fsync 树遍历时确定性
  暴露（DISKFULL=13），发布门 discard → 无 partial；测试经 p1_fsync_fail
  注入覆盖（真实盘满注入不可移植，错误码数值域已冻结）。
- F-AIO-002（p1hips Moc.fits 第二头区 CHECKSUM/DATASUM）：本任务写入面
  （publish/fsync/discard）不涉及 FITS 字节 digest，无交叉整改窗口，维持
  AIO-001 登记移交（p1hips 测试域/IMPL）。
- 前台补充上下文 ② 的 fits_header_end 多 HDU 扫描（p1hips 测试域）同样
  不在本任务写入面，不触碰。

### 验证（run/local/agent_aio002/ 日志在案；test 时间 2026-09-10）

- 主树：hips_publish_atomic_units + hips_publish_atomic + hips_writer_adapter
  + p1_hips_writer（4/4）循环 5 轮全绿（幂等实证）；p1hips 全组 6/6。
- asan 树（ASTROCS_ENABLE_SANITIZERS=ON）：4/4 两轮零报告（asan 下 kill
  用例改轮询等待 staging 出现——固定 250ms 在 -O0 慢 10x 下不确定）。
- 注入必败动态验证（asan tests/unit 直接运行）：p1_stage_create_fail/
  p1_fsync_fail/p1_promote_fail → rc=1 且 0 PASS-line；p1_discard_noop →
  units_discard 断言翻红（红锚口径：假清后残留断言）；p1_stage_slow_write →
  kill 用例时序锚（PASS 合法：非断言翻转型）。
- RED 锚定：临时反转 staging 重定向（writer 直写 out_dir）后
  hips_publish_atomic 翻红（fsync_fail 注入下 partial 断言失败）→ 恢复
  转绿——证明新增断言真实锚定旧行为。
- adapter 测试 5 轮幂等全绿；发布测试 3 轮幂等全绿。

### 待后续任务（不阻塞本任务）

- P1-HIPS-INT / IO-003 集成：stage2/astro_sphere_sink 等生产消费方接入
  publish 事务面（writer 核心 remove+create 直写语义未动，DISP-HIPS-004/
  DISP-P2HIPS-003 维持登记）；树级 COMPLETE manifest + sha256 tree_hash。
- hips_writer_adapter direct-vs-plugin 产物树对拍已在 staging 语义下自然
  成立（writer 输出在 stage、promote 后即发布树）；无新增差异面。
