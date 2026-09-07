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
