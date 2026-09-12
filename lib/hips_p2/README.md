# lib/hips_p2 — astrocs.p2.hips_writer（P2-HIPS）

> 状态: CONTRACT_READY（P2-HIPS-DOC 冻结，2026-09-09）｜doc revision: r1
> 本 README 由源码逐段核对后新建（P2-HIPS-DOC）：函数、单位、dtype、shape、
> invalid、错误、并发、内存、I/O 均以现行唯一生产实现
> `lib/phase2/tools/stage2.cpp`（1762 行实测；正式工具入口
> `astrocs-stage2`，lib/phase2/CMakeLists.txt:103-110）与 config 解析层
> `lib/phase2/include/astro/phase2/stage2_common.h`（P2Stage2Config :16-99、
> p2_stage2_parse_config :102、p2_stage2_make_upm_cfg :106、
> p2_acr_block_eligible :113）为准；行号全部 grep/sed 实测，禁止手抄他版。
> `lib/hips_p2/` 是 P2-HIPS 迁移目标目录（astrocs_p2_hips_writer.dll 落码
> 由 P2-HIPS-IMPL 建立，当前本目录仅合同文件、无源码）。落位依据：
> MODULE_MIGRATION_MATRIX.csv P2-HIPS 行 target=astrocs_p2_hips_writer.dll、
> module_id=astrocs.p2.hips_writer、legacy_paths="lib/phase2 write sources;
> lib/healpix_db"——实测 P2 马赛克写编排生产源在
> lib/phase2/tools/stage2.cpp（lib/phase2/ 目录三件套已被 P2-COV 域
> astrocs.p2.coverage 合同占用：lib/phase2/README.md r1，一目录一套
> README/module.yaml/memory.md，不可覆盖他域合同），故按 lib/hips/（P1-HIPS
> 迁移目标目录）先例落独立迁移目标目录 lib/hips_p2/，与 legacy 目录不重叠、
> 不含源码。lib/healpix_db 侧参与 HiPS 写生产的仅 P1 写通道
> lib/healpix_db/healpix_drizzle/astro_sphere_sink.cpp（P1-DRZ 域资产，
> 本模块仅作库消费者引用，不重归属）。权威合同链：SCI-UPM-001（w_UPM）+
> SCI-INT-001（signal/sup_max）+ SCI-REJ-001（排异判据）共享 FROZEN SCI
> （零 SCI 改动）→ ALG-P2-HIPS-001..004（docs/algorithms/
> PHASE2_MOSAIC_WRITE.md，P2-HIPS-DOC 冻结）→ DATA-P2-HIPS
> （DATA_SEMANTICS §20）/ API-P2-HIPS-001（PUBLIC_API Phase2 mosaic write
> 节）→ TEST-P2-HIPS-001（登记面=ALG 文档 §11.4 设计冻结 VERIFIED，
> 依 P2-COV-DOC TEST-COV-DESIGN-001 先例；可执行测试 MISSING 归 P2-HIPS-TEST）。
> 共用 writer 库 = lib/astro_image_io/src/hips/aio_hips_writer.cpp
> （P1-HIPS 域 ALG-HIPS-001..005 冻结；本模块为库消费者，叶级归一公式
> signal=flux_sum/covered_area、support=covered_area/A_cell 以
> ALG-HIPS-002 为权威，本 README 不重登记）。

## 1. 身份

| 字段 | 当前值 |
|---|---|
| MOD ID / DLL target | `MOD-astrocs-phase2-hips-writer`（矩阵行 MOD-astrocs-phase2-write 的登记延续：registry 页与追溯行沿用既有行 ID，module 词汇 astrocs.p2.hips_writer）；现状实现编入 CMake 工具目标 `astrocs-stage2`（lib/phase2/CMakeLists.txt:103-110，EXCLUDE_FROM_ALL，链 phase2 + astrocs_hips 等）；迁移目标 `astrocs_p2_hips_writer.dll`（P2-HIPS-IMPL 建立，尚未存在，全仓库无该目标） |
| module / ABI / doc revision | `astrocs.p2.hips_writer` / C++17（内部）+ C ABI 库消费（aio_hips.h extern "C"，P1 冻结面）/ r1 |
| owner / phase scope | SA-P2-I23 / phase2（matrix P2-HIPS；depends_on_int=P2-INT-INT;IO-003） |
| 文档状态 | CONTRACT_READY（实现存在于 lib/phase2/tools/stage2.cpp，模块化迁移未开始；不声明 IMPLEMENTED） |
| 构建 | lib/phase2/CMakeLists.txt:103 `add_executable(astrocs-stage2 tools/stage2.cpp)`、:104-105 链接、:107-108 P2_ENABLE_OPENMP 宏（CON-006 逐像素并行）、:110 EXCLUDE_FROM_ALL、:126 与 phase2_synthetic_gate 同列按需工具；stage2_common.cpp 编入 phase2 库（CMakeLists.txt:44、根 CMakeLists.txt:339） |

## 2. 负责范围 / 不负责

**负责**（唯一生产源 stage2.cpp `main` :112-1762 的 HIPS_WRITE 域）：

- 输入哈希链：frame_id 缓存（`p2_frame_id` :218-232，fid==0 → rc=4）→
  canonical manifest（frame_id|filter=;order=;frame=; 按 frame_id 排序拼接
  :233-243）→ `input_manifest_hash = sha256`（:243-245）→ 经
  `p2_stage2_make_upm_cfg` :427-430 进入 UPM 构建（`minfo.model_hash`
  :437-444）→ UPM 持久层与 diagnostics.json :1746。
- 马赛克编排生命周期：DISCOVER/VALIDATE/COVERAGE_UNION（:172-204，两阶段
  调用 :173-196；target_order 高于输入最高 order 拒绝 rc=3 "禁止插值伪装
  分辨率" :203-205）→ CONTROL_SAMPLE（:256-317 probe+fill 两阶段）→
  UPM_FIT（:426-444）→ UPM_PERSIST（:445-467，diagnostics=true 时）→
  BLOCK_PLAN（:515-522）→ REJECT+INTEGRATE+HIPS_WRITE（:523-1663）→
  HIPS_VERIFY（:1659-1676）。
- 马赛克写出：`aio_hips_product_begin`（out_hips，nside=1<<(target_order+9)
  :525，tile 512，dtype=cfg.precision?FLOAT64:FLOAT32 :529，flags 仅
  SIGNAL|SUPPORT :594，creator "ivo://astrocs/phase2"、title
  "AstroCS Phase2 Mosaic"、filter=infos[0].filter_passband :531/:597）→
  逐 union tile 覆盖帧探测（:663-671）→ rejection 计划解析（group-level
  wbpp_2_9_1 :641-659 / tile 级 astrocs_adaptive :674-696）→ ACR/CPU 路由
  （:733-776）→ micro-chunk 内存规划（`p2_block_plan` :778-817）→ 逐像素
  eligibility→权重→排异→积分（:1057-1538）→ flux/support 逆归一（ACR
  ACR :989-1004、CPU :1227-1228/:1531-1532）→ FITS 序→NESTED 序转换（HIPS-IMG-001 合同
  ACR :1024-1040/CPU :1606-1619，`astrocs::healpix::nested_local_to_fits_index(i,9,512)`
  调用 :1032/:1611）→ `aio_hips_write_signal_support_tile`
  :1047-1054/:1629-1636 → `aio_hips_finalize` :1638-1648。
- 权重语义门（matrix 专项）：weight_mode=2（默认）逐帧打开 ivar 产品
  （`AIO_HIPS_RD_IVAR` :557）；ivar 产品缺失且
  `legacy_allow_weight_fallback=false`（默认）→ 显式科学错误 rc=7
  （"拒绝继续，防止在非逆方差语义下冒充 ivar coadd" :565-578，rc=7 :574）；显式
  true 才降级 support 并 diagnostics 标红 :575-577。mode 0=
  support×snr²（legacy/诊断，local_snr_map 64×64 cell :1118-1128）；mode 1=
  等权。`p2_validate_candidate_weights`（:1131-1159）负/NaN/Inf hard fail。
- large_scale 两遍路径（astrocs.large_scale_rejection.v1，默认关闭）：
  `p2_large_scale_apply` connected-component grow（:1549-1551）+"拒绝 mask
  应用回原始 calibrated 科学值"二次积分 :1552-1605。
- HIPS_VERIFY 回读（AIO reader，:1659-1676；失败 rc=7 :1665）与
  diagnostics.json（out_hips/diagnostics.json :1748-1749，键集 :1697-1747）。

**不负责**（禁止越域）：

- 不做叶级归一/FITS 写盘/hierarchy/MOC/properties/manifest（writer 库
  aio_hips_writer.cpp，P1-HIPS 域 ALG-HIPS-001..005）；
- 不做 HiPS 读格式解析（IO-002 读合同 + aio_hips_reader P3 链）；
- 不做原子发布（IO-003 编排层合同；stage2 直写 cfg.out_hips :592，
  DISP-P2HIPS-003 如实登记）；
- 不做 UPM 拟合/排异判据/积分公式（SCI-UPM-001/SCI-REJ-001/SCI-INT-001
  FROZEN；w_UPM 唯一冻结式 PHASE2_UPM.md §5）；
- 不做 P3 HiPS→FITS（SCI-P3-001 域，只读引用）；
- 不跨滤镜统一（filter 取 infos[0] :531，多滤镜分组归调用方）。

## 3. 输入/输出 ports、DATA ID、单位、dtype、shape、invalid

唯一权威 = DATA-P2-HIPS（docs/contracts/DATA_SEMANTICS.md §20）。端口
词汇（module_adapters.cpp:677-694 p2_write_descriptor：integrated
DATA-P2-INT in / mosaic DATA-P2-RES out，UnitId::ADU、CoordinateFrame::
PIXEL）为编排层词汇，以 DATA-P2-HIPS 为准修订，P2-XX-INT 对齐。

- 输入：N 个 Phase1 单帧 HiPS（signal/support 子产品必读 :536-537；ivar
  子产品 weight_mode=2 必读 :557）+ control 级 local_snr_map/local_ivar_map
  （64×64 cell，:380-422）+ UPM 模型（P2-INT-INT 域输出）。
- 输出：out_hips 目录 signal + support 两个 Image HiPS（无 variance/ivar/
  snr 产品——DISP-P2HIPS-001）+ 可选 diagnostics.json；signal 单位 ADU
  （surface brightness）、dtype float32/float64（:529）；support 无量纲
  [0,1]；invalid=NaN signal + support=0（writer 库 aio_hips_writer.cpp :481-485）。
- shape：NESTED nside=2^(target_order+9)，tile 512×512，叶级序 (511−x)·512+y
  （HIPS-IMG-001）；A_cell=4π/(12·nside²) :527。

## 4. 配置 schema 与错误码

配置 = 单 JSON（`p2_stage2_parse_config` :102，rc=2 解析失败）；CLI 覆盖
--cpu-workers/--io-workers/--gpu-route cpu|auto|cuda/--deterministic 0|1
（CON-002 :155-166）。公共关键字段（stage2_common.h:16-99）：hips[]、
target_order(auto)、precision(0)、weight_mode(2)、
legacy_allow_weight_fallback(false)、reject_method(AUTO)/reject_profile
(wbpp_2_9_1)/reject_underdetermined_n(2)、reject_normalization
(astrocs_median_center_v1)、large_scale_*(false)、acr_route(auto)、
memory_limit_mb(24576)、out_hips、diagnostics(true)。权威登记：
PUBLIC_API.md Phase2 mosaic write 节（API-P2-HIPS-001）。

退出码：2=config/CLI；3=coverage/target_order；4=frame_id/sampler；
5=UPM build；6=tile 读/写/块不可行/finalize；7=ivar 门/HIPS_VERIFY。
错误经 g_log（run/logs/phase2/<YYYYMMDD>/stage2.log :161-165）与 stderr
透出；`aio_hips_last_error` 细节透传（:546-548/:598-601/:1639-1642）。

## 5. 并发/确定性/资源

- writer 单句柄串行（parallel_ok=false；CFITSIO 单流程；多句柄互斥缺口
  DISP-HIPS-006 归 P1 域）；tile 按 cov.n_union_cells 顺序 :661 串行写。
- CPU reference 逐像素并行仅内部（CON-006，OMP :1288-1317，per-worker
  scratch、thread id 固定顺序定序归并 :1315-1317）；large_scale 激活时
  强制串行 :1285（grow 缓冲跨线程覆盖防护）。
- 确定性：同输入同 config → 同 mosaic（tile 序固定、归并定序、单 writer）；
  UTC 时间戳字段（writer 合同 ALG-HIPS-005）除外。ACR 路由 fallback
  （cuda_unavailable_fallback 等 :752-766）记录于 diagnostics，不改科学语义。
- 资源：memory_limit_mb 经 `p2_block_plan`（safety_factor=0.75 :784）产出
  chunk_pixels；磁盘空间检查（UPM 持久 :464-467、product_begin 前 :581-583）。

## 6. 测试与容差

- 可执行 TEST-P2-HIPS-001：**MISSING**（P2-HIPS-TEST 建立；不冒认）。
- 矩阵/登记 VERIFIED 面=设计冻结（ALG 文档 §11.4 设计+容差，COV 先例）。
- 现状相邻测试（引用不冒认）：phase2_synthetic_gate W9 ACR
  mosaic_reject legacy↔CPU reference 等价（lib/phase2/tests/
  synthetic_gate.cpp:3021-3160）；tests/api/test_reject_integration_oracle.py
  生产 kernel oracle；tests/api/test_reject_parallel.py。马赛克端到端
  （真数据）验证记录见 lib/phase2/memory.md W11。
- TEST-DESIGN 建议与容差来源：ALG-P2-HIPS-001..004（PHASE2_MOSAIC_WRITE.md
  §8/§9）：Python NumPy 参考 signal/sup_max 复算 rtol=1e-12；序转换
  nested↔fits 恒等往返；ivar 门负测（缺 ivar → rc=7）。

## 7. 已知缺陷（DISP-P2HIPS，登记不改码，整改归 P2-HIPS-IMPL/INT）

| ID | 严重度 | 摘要 | 源码锚 |
|---|---|---|---|
| DISP-P2HIPS-001 | 中 | 输出仅 signal/support 两产品（:594 flags），消费 ivar 权重但无 variance/ivar 输出产品——方差传播止于加权积分。**SCI-F3-001 更新（2026-09-12）**：产品位 ALL_V20=127 与 nrej/nused int32 通道已在 AIO 域实现（DATA-UNC-001 §30.2 位 32/64），本行遗留 = stage2 flags 未随 §30.1/§30.2 目标态扩展 | stage2.cpp:594；aio_hips.h:ALL_V20/write_diag_tile |
| DISP-P2HIPS-002 | 中 | input_manifest_hash/model_hash 仅入 UPM 持久层与 diagnostics.json（:245/:430/:1746），未写 HiPS properties（无 aio_hips_set_drizzle_provenance 调用，全文件 grep 零命中）。**SCI-F3-001 更新（2026-09-12）**：AIO 侧通道已交付（`aio_hips_set_provenance` 五键双写 + `aio_hips_verify_product_set` 双向核验 + `aio_hips_write_diag_tile` nrej/nused int32），本行遗留部分收窄为"消费方调用点未接线"（本目录/本模块为库消费者）；写节点接线归 `lib/core/src/module_adapters.cpp` p2_op_write（域外，finding F-SCI-F3-001-01） | stage2.cpp:245,:430,:1746；aio_hips.h:set_provenance/verify_product_set/write_diag_tile |
| DISP-P2HIPS-003 | 低 | stage2 直写 out_hips（:592）无 staging；原子发布依赖 IO-003 编排层（DISP-HIPS-004 同源） | stage2.cpp:592 |
| DISP-P2HIPS-004 | 低 | 覆盖帧探测逐 tile 逐帧 probe（O(T·N) FITS 读，:663-671），无 MOC 缓存 | stage2.cpp:663-671 |

## 8. 关联

- 上游 SCI：SCI-UPM-001（PHASE2_UPM.md §5）、SCI-INT-001（INTEGRATION.md
  §5）、SCI-REJ-001（REJECTION.md）、SCI-SCOPE-001——共享 FROZEN 零改动。
- ALG：ALG-P2-HIPS-001..004（PHASE2_MOSAIC_WRITE.md）；库上游
  ALG-HIPS-001..005（HIPS_WRITER.md）、ALG-UPM-001、ALG-REJ-001..008、
  ALG-COV-001。
- 契约：DATA-P2-HIPS（DATA_SEMANTICS §20）、API-P2-HIPS-001（PUBLIC_API）、
  API-P2-001（编排）、ARCH-001；IO-002（输入读）/IO-003（原子发布）。
- 相邻（不改）：aio_hips_reader.cpp（HIPS_VERIFY 后端）、
  astro_sphere_sink.cpp（P1 写通道）、p2_session.cpp（编排 session，
  hips_paths 验证 :81-92、coverage :125/:138，不执行 HiPS 写）、
  lib/healpix_db（legacy_paths 提及侧：仅 archive/healpix_drizzle 历史
  实现与上述 P1 sink）。
- 迁移：astrocs_p2_hips_writer.dll / plan-execute-cancel-inspect /
  ThreadLease 接线归 P2-HIPS-IMPL；可执行测试归 P2-HIPS-TEST
  （可执行 TEST-P2-HIPS-001 MISSING；登记面=§11.4 设计冻结）。
