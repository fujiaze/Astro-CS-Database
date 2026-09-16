# lib/phase3_proj — astrocs.p3.projection（WCS/投影域合同 + 版本化 projection registry）

> P3-PROJ-DOC 冻结（2026-09-11，SA-P3-P25）。本目录为 Phase3 投影域迁移
> 合同落位（三件套：README + module.yaml + memory.md），照
> lib/phase2_upm→phase2_samp→phase2_rej→phase2_int→phase3_fits 迁移
> 目录先例新建。**P3-001 增补（2026-09-10）**：版本化 projection
> registry v1 与宪章 §18.1 冻结首批四投影（TAN/SIN/CAR/AIT）实现落位
> 本目录——`p3_projection.h`（唯一权威签名头）+ `p3_projection.cpp`
> （registry 冻结表 + 四投影统一操作面，ALG 冻结口径
> docs/algorithms/PHASE3_PROJ_IMPL.md §15 唯一权威）；TAN 逐式沿用
> lib/phase3_session/p3_wcs.cpp 冻结生产事实（bitwise 对拍承载于
> tests/unit/p3_projection_test.cpp T3）。**现状**：registry 为测试
> 目标直编面（tests/unit/CMakeLists.txt），非生产构建成员；
> dll_target=astrocs_p3_projection.dll 尚未存在（entrypoint=MISSING，
> 挂载由 P3-PROJ-IMPL/P3-002 建立，禁止声明 IMPLEMENTED）。
> legacy 生产源 lib/phase3_session/p3_wcs.h/.cpp 保留原位不搬家
> （会话消费点 p3_session.cpp 不变）。

## 1 身份

- module_id: **astrocs.p3.projection**（MODULE_MIGRATION_MATRIX P3-PROJ 行
  权威值；registry descriptor 占位 module_id=astrocs.phase3.wcs 由
  P3-PROJ-INT 对齐，不作冻结依据）。
- registry 行: MOD-astrocs-phase3-wcs；dll_target:
  astrocs_p3_projection.dll（合同值，未建）。
- 域: 天球投影/WCS。**P3-001**: registry v1 冻结四投影 TAN/SIN/CAR/AIT
  （宪章 §18.1；新增投影须经 registry 注册并附独立往返 Oracle）；
  ZEA 等其余投影未注册（registry_find 返回 nullptr，fail-closed）。

## 2 合同链（ID 唯一权威落位）

| 层 | ID | 权威落位 | 状态 |
|---|---|---|---|
| SCI | SCI-P3-001 | docs/science/PHASE3_HIPS_TO_FITS.md（共享 FROZEN，映射声明 SCI-P3-WCS-001⇒SCI-P3-001 见 ALG-P3-PROJ-IMPL-001 §5） | FROZEN |
| ALG | ALG-P3-PROJ-IMPL-001 | docs/algorithms/PHASE3_PROJ_IMPL.md（本域实现级合同，兼承接 ALG-P3-002 本域子面） | CONTRACT_READY |
| DATA | DATA-P3-WCS | docs/contracts/DATA_SEMANTICS.md §28 | CONTRACT_READY |
| API | API-P3-PROJ-001 | docs/contracts/PUBLIC_API.md（Phase3 投影公共消费面节） | CONTRACT_READY |
| ARCH | ARCH-001 | docs/architecture/cpu/ARCH_CONTRACTS.md | VERIFIED |
| API(镜像) | API-P3-001 | docs/contracts/PUBLIC_API.md（p3_session 五段编排面 FROZEN 镜像，不变） | FROZEN 镜像 |
| TEST | TEST-P3-WCS-001 | 登记面=TEST-P3-WCS-DESIGN-001（设计冻结 VERIFIED，ALG-P3-PROJ-IMPL-001 §11 + registry 页 §9 双重陈述）；可执行面=tests/unit/p3_wcs_test.cpp（90 行）+ tests/backend/test_p1002_gaps.py + tests/backend/p3_wcs_main.cpp；验收升级归 P3-PROJ-TEST | 见右 |
| EVID | EVID-MISSING | 待 P3-PROJ-INT/验收补 | MISSING |

## 2a P3-001 registry 与四投影（2026-09-10 增补）

- 冻结口径: docs/algorithms/PHASE3_PROJ_IMPL.md §15（六要素声明/共享
  旋转核/逐式公式/守卫域/测试设计）；SCI 层零改动。
- 符号: P3ProjectionSpec/P3ProjectionDescriptor/P3ProjectionStatus/
  P3ProjectionId/kP3ProjectionRegistryVersion/p3_projection_registry_
  {table,find,find_id,selfcheck}/p3_projection_{make,pix2world,
  world2pix,fits_keywords}（namespace astrocs::phase3proj）。
- 可执行测试: tests/unit/p3_projection_test.cpp（ctest
  p3_projection_units + p3_projection_fault）+ tests/backend/
  test_p3_projection_oracle.py（独立 numpy oracle/跨进程确定性）。
- 故障注入: ASTROCS_P3PROJ_FAULT=tan|sin|car|ait|registry（测试级注入，
  生产源零 getenv，P2-002 先例同构）。

## 3 生产源（冻结实测，2026-09-11）

- 唯一权威签名头: lib/phase3_session/p3_wcs.h（50 行）——
  P3WcsDescriptor（h:11-20）/P3WcsStatus（h:22-27）/
  p3_wcs_make（h:31-34）/p3_wcs_pix2world（h:38-39）/
  p3_wcs_world2pix（h:42-43）/p3_wcs_fits_keywords（h:46）。
- 实现: lib/phase3_session/p3_wcs.cpp（165 行）。
- 会话消费点: p3_session.cpp:17（include）/:160（p3_wcs_make，
  rotation_pa_deg 恒 0.0）/:163（状态映射）/:232（worker 循环
  pix2world，半球外像素 NaN）/:247-253（线程池）；backend 探针
  p3_wcs_main.cpp 与 Python 侧 test_p1002_gaps.py 编译链接 p3_wcs.cpp。
- 逐符号源码锚、冻结公式（G1 CD 构造/G2 反向映射）、错误语义、
  并发/确定性合同：ALG-P3-PROJ-IMPL-001 §2-§9。

## 4 职责与非职责

- 职责: TAN(gnomonic) 投影正/反映射、FITS WCS descriptor 构造
  （CRPIX/CRVAL/CD、parity、PA）、FITS 关键词文本输出、极点/半球
  守卫。
- 非职责: 不做重采样（ALG-P3-001/003 归 phase3_resample2 域）、
  不做 FITS 文件读写（p3_output/AIO 域）、不做会话编排与请求解析
  （p3_session 域）、不修改 SCI 公式（SCI-P3 FROZEN 零改动）。

## 5 co-located 合同链接

- SCI: [docs/science/PHASE3_HIPS_TO_FITS.md](../../docs/science/PHASE3_HIPS_TO_FITS.md)（FROZEN）
- ALG: [docs/algorithms/PHASE3_PROJ_IMPL.md](../../docs/algorithms/PHASE3_PROJ_IMPL.md)
- ALG(承接): [docs/algorithms/PHASE3_RESAMPLE.md](../../docs/algorithms/PHASE3_RESAMPLE.md)（ALG-P3-002 施工规格，公式零改动）
- DATA: [docs/contracts/DATA_SEMANTICS.md](../../docs/contracts/DATA_SEMANTICS.md) §28
- API: [docs/contracts/PUBLIC_API.md](../../docs/contracts/PUBLIC_API.md)
- 模块页: [docs/modules/phase3_proj.md](../../docs/modules/phase3_proj.md)；
  registry 手写页: docs/modules/registry/astrocs.phase3.wcs.md
- 迁移模板: 工程控制/…/tasks/MODULE_MIGRATION_TEMPLATE.md（P3-PROJ-DOC）

## 6 实测偏差登记（不改码，详见 ALG-P3-PROJ-IMPL-001 §10）

- PA 未接线: p3_session.cpp:160 rotation_pa_deg 恒 0.0（能力在内核，
  会话未消费）。
- 20000 上限可编译期覆盖: ASTROCS_P3_MAX_SIDE（p3_wcs.cpp:18-22）。
- projection 字段硬编码 "TAN"（p3_wcs.cpp:36），非 TAN 拒绝在
  p3_wcs_make 前置于 descriptor 层。
