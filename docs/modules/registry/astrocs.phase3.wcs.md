---
id: MOD-astrocs-phase3-wcs
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-P3-WCS-001, ALG-P3-002, API-P3-001]
downstream: [TEST-P3-WCS-001]
---

# 模块 astrocs.phase3.wcs（P3-PROJ-DOC 手写合同页，2026-09-11）

> Registry 行 ID 沿用 MOD-astrocs-phase3-wcs；矩阵权威 module_id=
> **astrocs.p3.projection**（MODULE_MIGRATION_MATRIX P3-PROJ 行）。
> frontmatter upstream/downstream（SCI-P3-WCS-001/ALG-P3-002/API-P3-001/
> TEST-P3-WCS-001）为 descriptor 占位词汇，保留不改动，由 P3-PROJ-INT
> 对齐（映射声明=ALG-P3-PROJ-IMPL-001 §5；占位 ID 不入合同）。

## 1 身份与合同落位

- 模块: astrocs.p3.projection（dll_target=astrocs_p3_projection.dll
  为迁移合同值，尚未存在——MISSING 语义，由 P3-PROJ-IMPL 建立，
  禁止声明 IMPLEMENTED；现状构建=astrocs_phase3_session 静态库成员，
  根 CMakeLists.txt:460-465，p3_wcs.cpp 为五源文件之一）。
- 合同落位: lib/phase3_proj/ 三件套（README r1 + module.yaml
  CONTRACT_READY entrypoint=MISSING + memory.md，迁移目标目录按
  lib/phase2_upm→phase2_samp→phase2_rej→phase2_int→hips_p2→
  phase3_fits 先例新建；lib/phase3_session/ 为会话编排域共享源，
  不整目录归属）。
- 生产源: lib/phase3_session/p3_wcs.cpp（165 行）+ 唯一权威签名头
  p3_wcs.h（50 行），实测 2026-09-11。
- 合同链: SCI-P3-001（共享 FROZEN，docs/science/PHASE3_HIPS_TO_FITS.md，
  V5 SCI-007 2026-08-28）→ ALG-P3-PROJ-IMPL-001
  （docs/algorithms/PHASE3_PROJ_IMPL.md，兼承接 ALG-P3-002 本域
  子面 G1/G2）→ DATA-P3-WCS（DATA_SEMANTICS §28）+ API-P3-PROJ-001
  （PUBLIC_API.md Phase3 投影公共消费面节）→ TEST-P3-WCS-001
  （登记面=TEST-P3-WCS-DESIGN-001 设计冻结 VERIFIED，见 §9 双重
  陈述）；编排面 API-P3-001（p3_session 五段 FROZEN）镜像不变。
- 上游依赖: astrocs_phase3_session（采样/重采样/写出编排域同库）；
  depends_on_int=ABI-005;DATA-004;RT-006（矩阵行；ABI-005=模块
  C ABI 承接、DATA-004=WCS descriptor 数据面、RT-006=线程泄漏守卫
  由纯函数无状态结构性满足，ALG §10）。

## 2 职责与明确非职责

- 职责: TAN(gnomonic) 投影域——descriptor 构造（G1：CRPIX/CRVAL/
  CD、parity、PA 推广 CD）、像素↔天球正反映射（G2）、FITS 关键词
  文本输出、极点（|dec|≤85° 单一条件）/TAN 半球/参数守卫。
- 非职责: 不做重采样与 tile 读取（ALG-P3-001/003，phase3_resample2
  域）、不做 FITS 文件读写（ALG-P3-FITS-IMPL-001 域）、不做请求
  解析与编排（p3_session run 段）、不实现 SIN/ZEA/CAR/AIT（矩阵
  notes 扩展清单，SCI §9a-3 须独立测试+新 claim）、不改 SCI 公式
  （SCI-P3 FROZEN 零改动）。

## 3 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标/dtype |
|---|---|---|---|---|
| `props` | `DATA-P3-PROPS`（descriptor 词汇；HiPS properties 面） | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::HEALPIX`，文本键值 |
| `wcs_plan` | `DATA-P3-WCS`（§28 实际承载=构造入参/映射面） | 可 | `UnitId::DEGREE` | `CoordinateFrame::ICRS` deg + 0-based px + CD deg/px，FP64 |

- invalid 唯一权威=DATA-P3-WCS §28：parity 非法/|dec|>85°/scale≤0/
  W,H∉[1,20000]→P3_WCS_PARAM（p3_wcs.cpp:39-43）；映射空指针→
  PARAM（:95/:122）；world2pix |dec|>85°→PARAM（:123）、|det|<
  1e-300→PARAM（:137）；r≥π/2（:104）/denom≤0（:130）→
  P3_WCS_HEMISPHERE；make 四角守卫失败码透传（:80-88）。
- 坐标冻结: 天球=ICRS deg（RA 归一 [0,360)）；像素=0-based 入参/
  出参（FITS 1-based=+1，:97-98/:140-141），crpix 本身 FITS
  1-based pixel-center=(W+1)/2。
- 端口词汇（props/wcs_plan、DATA-P3-PROPS）为 descriptor 派生
  （module_adapters.cpp:406-423），由 P3-PROJ-INT 对齐 DATA-P3-WCS，
  不作冻结依据。

## 4 公共 header、核心 symbol 与生命周期

- 内核消费面=API-P3-PROJ-001（p3_wcs.h 唯一权威签名头）:
  `p3_wcs_make`（h:31-34，实现 :30-90）、`p3_wcs_pix2world`
  （h:38-39，:93-118）、`p3_wcs_world2pix`（h:42-43，:120-143）、
  `p3_wcs_fits_keywords`（h:46，:145-163）+ 数据结构
  P3WcsDescriptor（h:11-20）/P3WcsStatus（h:22-27，OK=0/PARAM=1/
  UNSUPPORTED=2 无产生点/HEMISPHERE=3）。
- 生命周期: 无状态纯函数（无 create/destroy；descriptor 由调用方
  持有，make 一次、映射 N 次）；会话编排面 API-P3-001 FROZEN 五段
  create→validate→run→inspect→destroy 不变（p3_session.h:16-28），
  run 内 make（p3_session.cpp:160）→worker 逐像素 pix2world（:232）。
- 域际: DATA-P3-FITS 写路径 wcs 字段承载本 descriptor
  （p3_output.cpp:148-169 关键词写、API-P3-FITS-001 消费面）。
- 不新增/不修改任何 C 头/C ABI（本页为既有符号展开冻结）。

## 5 Registry descriptor 与配置 schema

- descriptor（module_adapters.cpp:406-423 p3_wcs_descriptor，
  编排层占位词汇）: module_id=`astrocs.phase3.wcs`；
  execution_class=cpu_heavy；parallel_ok=true（纯函数 const-only
  并发安全，与 §7 结构性一致）；ports props(DATA-P3-PROPS 必)
  +wcs_plan(DATA-P3-WCS 可)；sci_id=SCI-P3-WCS-001、alg_id=
  ALG-P3-002、data_id=DATA-P3-WCS、api_id=API-P3-001、test_id=
  TEST-P3-WCS-001——占位 ID/端口由 P3-PROJ-INT 对齐本页与
  lib/phase3_proj/module.yaml，不作冻结依据。
- 注册序: module_adapters.cpp:816 起序列（phase3_descriptor→
  p3_wcs_descriptor→p3_resample2_descriptor→p3_writer_descriptor，
  :797-798 收尾）；配置=phase config JSON（按 PHASE API 文档）。

## 6 冻结公式（G1/G2 摘要；唯一权威=ALG-P3-PROJ-IMPL-001 §6/§7）

- G1（p3_wcs.cpp:51-78）: CRPIX=((W+1)/2,(H+1)/2)；PA=0 对角
  east_left diag(−s,+s)/east_right diag(+s,−s)；PA≠0 推广
  CD=R(−PA)·diag(sgn_x·s, sgn_y·s)，展开式 CD1_1=sgn_x·s·cosPA/
  CD1_2=sgn_y·s·sinPA/CD2_1=−sgn_x·s·sinPA/CD2_2=sgn_y·s·cosPA；
  det(CD)=−s²<0 手性冻结；P0 修复 bughunt_p0_wcs（:65-68）已合并。
- G2 正向（:93-118）: (ξ,η)=CD·(pix−CRPIX)→θ=atan(1/r)→球面角
  （Calabretta & Greisen 2002 形式）→RA wrap [0,360)。
- G2 反向（:120-143）: gnomonic (ξ,η)→δ=CD⁻¹·(ξ,η)→0-based 像素。
- 容差: roundtrip <1e-6 px（SCI §7 冻结）；FOV≤20° 适用域
  （SCI §9a-12）。

## 7 执行类、并行轴、ThreadBudget lease、确定性

- execution_class=cpu_heavy；内核纯函数无内部并行轴（0 处
  thread/mutex/omp/全局可变量，p3_wcs.cpp:12-28）——const-only
  入口多线程并发安全；RT-006 线程泄漏守卫结构性满足。
- 并行仅上游 worker 池（p3_session.cpp:247-253，worker=
  ThreadBudget.max_workers，禁 hardware_concurrency）；本域逐像素
  调用（:232）失败 continue（半球外像素 NaN）。
- 确定性: 同入参 bitwise（无求和序）；双平台数值合同由测试层
  承载（§9 T6）。

## 8 实测偏差与整改（不修码，唯一权威=ALG-P3-PROJ-IMPL-001 §11）

- PA 未接线: p3_session.cpp:160 rotation_pa_deg 恒 0.0（内核能力
  无会话消费方）→ P3-PROJ-IMPL/INT。
- kMaxSide=20000 可 ASTROCS_P3_MAX_SIDE 编译期覆盖（:18-22）——
  默认值语义如实冻结。
- projection 硬编码 "TAN"（:36/:89），UNSUPPORTED 枚举无产生点；
  SIN/ZEA/CAR/AIT 扩展 TODO。
- astrocs_p3_projection.dll 未建（entrypoint=MISSING）；探针/
  回归现状内联编译，DLL 挂载归 P3-PROJ-IMPL。
- 本域无 DISP 缺陷登记；其余见 docs/KNOWN_LIMITATIONS.md 与
  ALG-P3-PROJ-IMPL-001 §13 合同边界。

## 9 验证与测试面（TEST-P3-WCS-DESIGN-001 设计冻结 VERIFIED）

- 设计冻结: ALG-P3-PROJ-IMPL-001 §12 T1-T7（T1 正向解析解/T2
  roundtrip 1e-6 px 冻结容差/T3 G1 精确断言/T4 手性极性关键词/
  T5 负面清单/T6 oracle=现状独立解析解→验收级 WCSLIB/T7 不变量
  回归）。
- 双重陈述（C7 锚）: 本节承载 TEST-P3-WCS-001 登记面；可执行面
  升级归 P3-PROJ-TEST（验收级 oracle=WCSLIB，矩阵 notes）。
- 现状执行测试（相邻证据，引用不冒认）:
  tests/unit/p3_wcs_test.cpp（90 行，WCS 完整性/溢出检查）+
  tests/backend/test_p1002_gaps.py（独立解析解回归 :115-138）+
  tests/backend/p3_wcs_main.cpp（探针 make/p2w/w2p/kw）。
- EVIDENCE: EVID-MISSING（归 P3-PROJ-INT/验收补）。

## 10 合同链接

- SCI: docs/science/PHASE3_HIPS_TO_FITS.md（FROZEN）
- ALG: docs/algorithms/PHASE3_PROJ_IMPL.md；承接:
  docs/algorithms/PHASE3_RESAMPLE.md（ALG-P3-002 G1/G2 施工规格，
  零改动）
- DATA: docs/contracts/DATA_SEMANTICS.md §28
- API: docs/contracts/PUBLIC_API.md（API-P3-PROJ-001 节）
- 模块总页: docs/modules/phase3_proj.md；合同三件套:
  lib/phase3_proj/{README.md,module.yaml,memory.md}
