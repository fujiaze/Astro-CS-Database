# 模块 acsd.phase3.wcs

> 上游：docs/ACSD_DESIGN.md §8.5（模块与 ABI）、§6.2（export 流程）、
> §6.3（投影算法：内置多种）
> 科学正本：docs/science/PHASE3_HIPS_TO_FITS.md（SCI-P3-001，FROZEN，零改动）
> 算法正本：docs/science/algorithms/PHASE3_PROJ_IMPL.md（ALG-P3-PROJ-IMPL-001；
> §5 映射声明、§6/§7 公式、§11 实测偏差、§12 测试设计 T1–T7、§13 合同边界、
> §15 registry 冻结表 + 逐投影六要素）；承接
> docs/science/algorithms/PHASE3_RESAMPLE.md（ALG-P3-002 G1/G2 施工规格，零改动）
> 数据正本：docs/detail/registry/acsd.phase3.wcs.md（DATA-P3-WCS 端口表，本页输入输出端口表）、
> eng/contracts/schemas/projection_registry.schema.json（registry 冻结集合，
> 机器可校验导出 schema）
> API 正本：docs/engineering/api/PUBLIC_API.md（API-P3-PROJ-001，Phase3 投影公共消费面节；
> API-P3-001 = p3_session 五段编排面 FROZEN 镜像）
> 架构正本：docs/engineering/architecture/ARCHITECTURE.md
> 门表事实源：docs/science/algorithms/GATES_AND_TOLERANCES.md §3
> 落地设计：docs/detail/PHASE3_DETAILED_DESIGN.md §2
> 引用文献：见文末「参考文献」（角标用全角 `［N］`，因本文正文的半角 `[...]` 已被
> 数值域区间与数组下标占用）

模块 = `acsd.p3.projection`（module_id 合同值）；registry 行
MOD-acsd-phase3-wcs；dll_target = `acsd_p3_projection.dll`（迁移合同值，
未落地，IMPLEMENTED 只由验收签发）；现状构建 = `acsd_phase3_session` 静态库
成员，`p3_wcs.cpp` 为其五源文件之一。owner = SA-P3-P25；language = c++17；
abi_version = 1；phase_scope = phase3；resource_class = cpu_heavy；
threading_model = `host_executor_lease`（迁移目标合同值；现状 = 内核纯函数无内部
线程）。descriptor 词汇（module_id=`acsd.phase3.wcs`、SCI-P3-WCS-001 /
ALG-P3-002 / API-P3-001 / TEST-P3-WCS-001）与 module_id 合同值
`acsd.p3.projection` 的对齐属迁移目标（未落地）。

## 1 身份与合同落位

- 合同落位: lib/algorithms/projection/ 三件套（CONTRACT_READY）。
- 生产源: lib/algorithms/projection/p3_wcs.cpp + 同目录签名头正本 p3_wcs.h。
- 合同链: SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，FROZEN；映射声明
  SCI-P3-WCS-001 ⇒ SCI-P3-001 见 ALG §5）→ ALG-P3-PROJ-IMPL-001（兼承接
  ALG-P3-002 本域子面 G1/G2）→ DATA-P3-WCS（本页输入输出端口表）+
  API-P3-PROJ-001 → TEST-P3-WCS-001（设计冻结 = TEST-P3-WCS-DESIGN-001，见 §9）；
  编排面 API-P3-001（p3_session 五段 FROZEN）镜像不变；ARCHITECTURE（VERIFIED）。
- 上游依赖: acsd_phase3_session（采样/重采样/写出编排域同库）；
  depends_on_int=ABI-005;DATA-004;RT-006（ABI-005=模块 C ABI 承接、
  DATA-004=WCS descriptor 数据面、RT-006=线程泄漏守卫由纯函数无状态
  结构性满足，ALG §10）。

## 2 职责与明确非职责

- 职责: TAN(gnomonic) 投影域——descriptor 构造（G1：CRPIX/CRVAL/
  CD、parity、PA 推广 CD）、像素↔天球正反映射（G2）、FITS 关键词
  文本输出、极点（|dec|≤85° 单一条件）/TAN 半球/参数守卫。
- 非职责: 不做重采样与 tile 读取（ALG-P3-001/003，phase3_resample2
  域）、不做 FITS 文件读写（ALG-P3-FITS-IMPL-001 域）、不做请求
  解析与编排（p3_session run 段）、**除 TAN 以外的投影**、不改 SCI 公式
  （SCI-P3 FROZEN 零改动）。
- **投影集口径（最高设计 §6.3）**：**设计冻结 8 种**
  （`TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA`）；每种投影必须声明六要素：适用域、奇点、
  经度 wrap、轴手性、CRPIX/CRVAL/CD/PC/CDELT、CTYPE。**当前登记：仅 `TAN` 已
  实现**（声明集 D = 实现集 I = `{TAN}`，`p3_projection_registry.h`；
  **在役 registry = `p3_proj.cpp`**）；**支持声明以注册表登记为准：未实现者报
  「不支持」**（`p3_proj_declare` → `P3_WCS_UNSUPPORTED` + 请求码 + 原因 +
  已支持清单）；`registry_find` 未命中返回 nullptr = fail-closed。

  新增投影须同时进实现集与声明集（`p3_proj_registry_selfcheck` 判红）+ 独立
  往返 Oracle + 追溯条目；**注册面与会话面分离**。

  **实现集口径冲突登记**：文档面曾记「已实现 TAN/SIN/CAR/AIT（4/8），STG/MOL/
  CEA/ZEA 待实现」，与在役注册表不符 —— 口径**以在役注册表为准**（当前
  D = I = {TAN}）。

  registry 冻结要点（FITS WCS Paper II）：CAR / AIT 把 CRVAL2（含 LONPOLE 默认
  0/180）纳入三 Euler 角映射；AIT 椭圆域要求半长轴 ≤ 1；CAR native 极行
  |θ| ≥ 90° fail-closed。冻结表与逐投影六要素的正本 = ALG-P3-PROJ-IMPL-001 §15
  与 `lib/algorithms/projection/` 的 `Spec` 六要素字段与 `registry_frozen_set()`
  导出（schema 不复制公式）。

- **输入输出数据合同补充**：输入 = 用户 WCS 计划（中心、尺度、shape、旋转、
  投影或足够约束）；输出 = 注册的投影定义（CTYPE、正反变换、适用域、奇点处理、
  CRPIX/CRVAL/CD/PC/CDELT）。FITS 1-based 关键字与内部 0-based 像素中心的转换
  唯一；正反变换必须互逆（误差 < 合同阈值）。

## 3 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标/dtype |
|---|---|---|---|---|
| `props` | `DATA-P3-PROPS`（descriptor 词汇；HiPS properties 面） | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::HEALPIX`，文本键值 |
| `wcs_plan` | `DATA-P3-WCS`（§28 实际承载=构造入参/映射面） | 可 | `UnitId::DEGREE` | `CoordinateFrame::ICRS` deg + 0-based px + CD deg/px，FP64 |

- invalid 权威源=DATA-P3-WCS §28：parity 非法/|dec|>85°/scale≤0/
  W,H∉[1,20000]→P3_WCS_PARAM（p3_wcs.cpp）；映射空指针→
  PARAM（p3_wcs.cpp 两处）；world2pix |dec|>85°→PARAM（p3_wcs.cpp）、|det|<
  1e-300→PARAM（p3_wcs.cpp）；r≥π/2/denom≤0→
  P3_WCS_HEMISPHERE（均 p3_wcs.cpp）；make 四角守卫失败码透传（p3_wcs.cpp）。
- 坐标冻结: 天球=ICRS deg（RA 归一 [0,360)）；像素=0-based 入参/
  出参（FITS 1-based=+1，见 p3_wcs.cpp），crpix 本身 FITS
  1-based pixel-center=(W+1)/2。
- 端口词汇（props/wcs_plan、DATA-P3-PROPS）为 descriptor 派生
  （p3_wcs_descriptor），其与 DATA-P3-WCS 的对齐属迁移目标（未落地），
  不作冻结依据。

## 4 公共 header、核心 symbol 与生命周期

- 内核消费面=API-P3-PROJ-001（p3_wcs.h 签名头正本）:
  `p3_wcs_make`（p3_wcs.h 声明，p3_wcs.cpp 实现）、`p3_wcs_pix2world`
  、`p3_wcs_world2pix`、
  `p3_wcs_fits_keywords`（均在 p3_wcs.h 声明 / p3_wcs.cpp 实现）+ 数据结构
  P3WcsDescriptor（p3_wcs.h）/P3WcsStatus（p3_wcs.h，OK=0/PARAM=1/
  UNSUPPORTED=2 无产生点/HEMISPHERE=3）。
- 生命周期: 无状态纯函数（无 create/destroy；descriptor 由调用方
  持有，make 一次、映射 N 次）；会话编排面 API-P3-001 FROZEN 五段
  create→validate→run→inspect→destroy 不变（p3_session.h），
  run 内 make（p3_session.cpp）→worker 逐像素 pix2world（p3_session.cpp）。
- 域际: DATA-P3-FITS 写路径 wcs 字段承载本 descriptor
  （p3_output.cpp 关键词写、API-P3-FITS-001 消费面）。
- 不新增/不修改任何 C 头/C ABI（本页为既有符号展开冻结）。

## 5 Registry descriptor 与配置 schema

- descriptor（p3_wcs_descriptor，编排层口径）: module_id=`acsd.phase3.wcs`；
  execution_class=cpu_heavy；parallel_ok=true（纯函数 const-only
  并发安全，与 §7 结构性一致）；ports props(DATA-P3-PROPS 必)
  +wcs_plan(DATA-P3-WCS 可)；sci_id=SCI-P3-WCS-001、alg_id=
  ALG-P3-002、data_id=DATA-P3-WCS、api_id=API-P3-001、test_id=
  TEST-P3-WCS-001——descriptor 派生的占位 ID/端口与
  lib/algorithms/projection/module.yaml 的对齐属迁移目标（未落地），不作冻结依据。
- 注册序: phase3_descriptor→p3_wcs_descriptor→p3_resample2_descriptor→
  p3_writer_descriptor（registry 注册序列）；配置=phase config JSON（键集 = API-P3-001）。
- 用户 WCS 计划面：

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `projection` | `tan` | —— | tan / sin / car / ait / stg / mol / cea / zea |
| `crpix` | 中心 | px | 参考像素 |
| `crval` | —— | deg | 参考天球坐标 |
| `cd_matrix` / `cdelt` | —— | deg/px | 尺度 |
| `rotation` | 0 | deg | 旋转（用 CD 时） |

## 6 冻结公式（G1/G2 摘要；权威源=ALG-P3-PROJ-IMPL-001 §6/§7）

- G1（p3_wcs.cpp）: CRPIX=((W+1)/2,(H+1)/2)；PA=0 对角
  east_left diag(−s,+s)/east_right diag(+s,−s)；PA≠0 推广
  CD=R(−PA)·diag(sgn_x·s, sgn_y·s)，展开式 CD1_1=sgn_x·s·cosPA/
  CD1_2=sgn_y·s·sinPA/CD2_1=−sgn_x·s·sinPA/CD2_2=sgn_y·s·cosPA；
  det(CD)=−s²<0 手性冻结；P0 缺陷修复已合并（p3_wcs.cpp）。
- G2 正向（p3_wcs.cpp）: (ξ,η)=CD·(pix−CRPIX)→θ=atan(1/r)→球面角
  （Calabretta & Greisen 2002［2］ 形式）→RA wrap [0,360)。
- G2 反向（p3_wcs.cpp）: gnomonic (ξ,η)→δ=CD⁻¹·(ξ,η)→0-based 像素。
- 容差: roundtrip **紧门** <1e-8 px（SCI §7 冻结；适用域与门限由
  `p3_wcs_applicability()` 单一事实源给出，`kTanApplicability`）——
  **适用域 `scale ≥ min_scale_arcsec = 0.9″/px`**，
  低于该尺度紧门不适用（报「超出适用域」而非判红），退回**全域保守门** 1e-6 px
  （`roundtrip_tol_global_px`）；机器可读判定 = `p3_wcs_roundtrip_gate()`；FOV≤20° 适用域
  （SCI §9a-12）。门表事实源 = `docs/science/algorithms/GATES_AND_TOLERANCES.md` §3
  （G-P1-WCS-BRIDGE / -GLOBAL / -RT-ITER / -RT-APBP）。

## 7 执行类、并行轴、ThreadBudget lease、确定性

- execution_class=cpu_heavy；内核纯函数无内部并行轴（0 处
  thread/mutex/omp/全局可变量，p3_wcs.cpp）——const-only
  入口多线程并发安全；RT-006 线程泄漏守卫结构性满足。
- 并行仅上游 worker 池（p3_session.cpp，worker=
  ThreadBudget.max_workers，禁 hardware_concurrency）；本域逐像素
  调用（p3_session.cpp）失败 continue（半球外像素 NaN）。
- 确定性: 同入参 bitwise（无求和序）；双平台数值合同由测试层
  承载（§9 T6）。

## 8 实测偏差与现行语义（权威源 = ALG-P3-PROJ-IMPL-001 §11）

- 未注册投影 → 拒绝；超适用域（极点 / 奇点）→ 明确处理（wrap 或拒绝），错位
  一律显式登记；轴手性 / CRPIX 单位错误 → fail-closed。
- PA 未接线: p3_session.cpp rotation_pa_deg 恒 0.0（内核能力无会话消费方）。
- kMaxSide=20000 可 ACSD_P3_MAX_SIDE 编译期覆盖（p3_wcs.cpp）——默认值语义
  如实冻结。
- 产品声明门 `p3_proj_declare` 对非 TAN 码显式返回 `P3_WCS_UNSUPPORTED`
  （含已支持清单），`p3_wcs.cpp` 经 `p3_proj_is_implemented` 产生该状态；
  8 冻结码 = `TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA`，声明/实现集当前 = `{TAN}`。
- acsd_p3_projection.dll 未建（entrypoint 未落地）；探针/回归为内联编译；
  WCSLIB 验收 oracle 与可执行测试待建。
- 合同边界与缺陷登记 = ALG-P3-PROJ-IMPL-001 §11/§13；全局限制登记 =
  artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。

## 9 验证与测试面（TEST-P3-WCS-DESIGN-001 设计冻结 VERIFIED）

- 设计冻结: ALG-P3-PROJ-IMPL-001 §12 T1-T7（T1 正向解析解/T2
  roundtrip 1e-8 px 冻结容差/T3 G1 精确断言/T4 手性极性关键词/
  T5 负面清单/T6 oracle=现状独立解析解→验收级 WCSLIB/T7 不变量
  回归）。
- 本节承载 TEST-P3-WCS-001 登记面；可执行面待建（验收级 oracle=WCSLIB）。
- 已取证但载体不在仓内的相邻结论（不冒认）: WCS 完整性/溢出检查读数、
  独立解析解回归读数，以及 make/p2w/w2p/kw 探针读数。这些读数不在本仓可复算路径上，
  引用时只作背景。
- Oracle 面补充：Astropy / WCSLIB 独立正负投影**绝对对拍**（不只用往返 —— 往返
  对 CRVAL2 类缺陷零区分力）+ 往返（中心、边、wrap、极点、奇点）+ CRPIX ↔ CRVAL
  定义性不变量 + `dec0 ≠ 0` 用例；**八投影全覆盖测试**（每投影必须带独立 Oracle
  才可注册）。
- 执行证据：NOT_VERIFIED（验收证据待补）。

## 10 合同链接

- SCI: docs/science/PHASE3_HIPS_TO_FITS.md（FROZEN）
- ALG: docs/science/algorithms/PHASE3_PROJ_IMPL.md；承接:
  docs/science/algorithms/PHASE3_RESAMPLE.md（ALG-P3-002 G1/G2 施工规格，
  零改动）
- DATA: docs/detail/registry/acsd.phase3.wcs.md（本页输入输出端口表）
- API: docs/engineering/api/PUBLIC_API.md（API-P3-PROJ-001 节）
- 合同三件套: lib/algorithms/projection/
- 会话编排面现行权威 = docs/engineering/contracts/RUNTIME.md + docs/detail/registry/acsd.phase3.*

## NaN 与输出语义

- NaN 规则（权威 = `ACSD_DESIGN.md` §5.5）：**样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数**；
  剔除项逐条进场级计数。无覆盖/无数据 = NaN；0 与 ±Inf 不作有效值。
- 输出语义守卫：只接受**面亮度**语义输入，端口 `UnitId::SURFACE_BRIGHTNESS`；输出模式
  `surface_brightness` / `point_source_flux` / `visualization` 显式声明（最高设计 §6.3）。

## 参考文献

- ［1］ Greisen, E. W.; Calabretta, M. R. (2002). "Representations of World Coordinates in
  FITS". *Astronomy and Astrophysics* 395, 1061–1075（FITS WCS Paper I）.
  DOI [10.1051/0004-6361:20021326](https://doi.org/10.1051/0004-6361:20021326)
- ［2］ Calabretta, M. R.; Greisen, E. W. (2002). "Representations of Celestial Coordinates
  in FITS". *Astronomy and Astrophysics* 395, 1077–1122（FITS WCS Paper II）.
  DOI [10.1051/0004-6361:20021327](https://doi.org/10.1051/0004-6361:20021327)
