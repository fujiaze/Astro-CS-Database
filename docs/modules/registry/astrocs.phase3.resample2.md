---
id: MOD-astrocs-phase3-resample2
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-P3-RES-001, ALG-P3-003, API-P3-001]
downstream: [TEST-P3-RES-001]
---

# 模块 astrocs.phase3.resample2（P3-RSMP-DOC 手写合同页，2026-09-12）

> Registry 行 ID 沿用 MOD-astrocs-phase3-resample2；矩阵权威 module_id=
> **astrocs.p3.resample**（MODULE_MIGRATION_MATRIX P3-RSMP 行）。
> frontmatter upstream/downstream（SCI-P3-RES-001/ALG-P3-003/API-P3-001/
> TEST-P3-RES-001）为 descriptor 占位词汇，保留不改动，由 P3-RSMP-INT
> 对齐（映射声明=ALG-P3-RSMP-IMPL-001 §5：SCI-P3-RES-001⇒SCI-P3-001；
> 占位 ID 不入合同）。

## 1 身份与合同落位

- 模块: astrocs.p3.resample（dll_target=astrocs_p3_resample.dll
  为迁移合同值，尚未存在——MISSING 语义（DISP-P3RSMP-005），由
  P3-RSMP-IMPL 建立，禁止声明 IMPLEMENTED；现状构建=
  astrocs_phase3_session 静态库成员，根 CMakeLists.txt:460-465，
  p3_resample.cpp 为五源文件之一）。
- 合同落位: lib/phase3_rsmp/ 三件套（README r1 + module.yaml
  CONTRACT_READY entrypoint=MISSING + memory.md，迁移目标目录按
  lib/phase3_proj→phase3_fits 先例新建；lib/phase3_session/ 为会话
  编排域共享源，不整目录归属）。
- 生产源: lib/phase3_session/p3_resample.cpp（239 行）+ 唯一权威
  签名头 p3_resample.h（58 行），实测 2026-09-12。
- 合同链: SCI-P3-001（共享 FROZEN，docs/science/PHASE3_HIPS_TO_FITS.md，
  V5 SCI-007 2026-08-28；矩阵 science_id 占位 SCI-P3-RES-001 映射
  声明⇒SCI-P3-001）→ ALG-P3-003（docs/algorithms/PHASE3_RESAMPLE.md
  G3/G4 施工规格，公式零改动）+ ALG-P3-RSMP-IMPL-001
  （docs/algorithms/PHASE3_RSMP_IMPL.md 实现级合同）→ DATA-P3-RES
  （DATA_SEMANTICS §29）+ API-P3-RSMP-001（PUBLIC_API.md Phase3
  重采样公共消费面节）→ TEST-P3-RES-001（登记面=
  TEST-P3-RSMP-DESIGN-001 设计冻结 VERIFIED，见 §9 双重陈述；
  矩阵 test_status=DORMANT）；编排面 API-P3-001（p3_session 五段
  FROZEN）镜像不变。
- 上游依赖: astrocs_phase3_session（properties 校验经
  p3_sampler_open 间接消费 + lib/common/healpix 权威球面函数）；
  depends_on_int=P3-PROJ-INT;IO-003;CPU-005（矩阵行；P3-PROJ-INT=
  上游 WCS 域对齐、IO-003=tile 文件读路径、CPU-005=worker 池并行
  由每 worker 独立 sampler 结构性满足，ALG-P3-RSMP-IMPL-001 §7）。
- 相邻占位行注记: MOD-astrocs-phase3-resample（module_adapters.cpp:
  300-318 phase3_descriptor，P2 模板复制残留）不属本页，由
  P3-RSMP-INT 处理，本任务零触碰。

## 2 职责与明确非职责

- 职责: HiPS tile 重采样域——sampler 生命周期（open/open_ex/
  set_max_tiles/close）、G3 order 选择（p3_order_select）、输入模式
  守卫（p3_resample_check_mode，surface_brightness 唯一合法）、
  NEAREST（精确 cell）/BILINEAR（一阶插值）逐输出像素采样（含跨
  tile 3×3 邻域读取）、有界 tile 缓存（FIFO）、coverage 二值语义。
- 非职责: 不做投影/WCS 构造（ALG-P3-PROJ-IMPL-001 域）、不做 FITS
  原子写（ALG-P3-FITS-IMPL-001 域）、不做请求解析与编排
  （p3_session run 段）、不实现 variance/weight/ivar/support/flux-
  per-pixel 输入与 alpha channel/BLANK int tile（SCI §9a-8/-9/-10
  显式拒）、不引入第三种采样核、不改 SCI 公式（SCI-P3 FROZEN
  零改动）。

## 3 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `wcs_plan` | `DATA-P3-WCS` | 必 | `UnitId::DEGREE` | `CoordinateFrame::ICRS` |
| `hips` | `DATA-HIPS-001` | 必 | `UnitId::ADU` | `CoordinateFrame::HEALPIX` |
| `resampled` | `DATA-P3-RES` | 可 | `UnitId::SURFACE_BRIGHTNESS` | `CoordinateFrame::PIXEL` |

- invalid 唯一权威=DATA-P3-RES §29.4：properties 语义不符→
  P3_RS_PARAM（hips_properties.cpp:113-122；order∈[0,20]、
  tile_width 必须 512、NESTED 唯一）；max_order 越界/scale≤0→
  P3_RS_PARAM（p3_resample.cpp:84-86）；input_mode 非
  surface_brightness→P3_RS_UNSUPPORTED（cpp:95-107）；properties
  解析失败/HiPS 目录或 tile 不可读→P3_RS_IO；**tile 缺失=coverage=0
  数据语义非错误**（§6.6 值语义表：tile 内 NaN→值 NaN+coverage=1
  传播；tile 缺失→NaN+coverage=0；未打开→coverage=0）。
- 坐标/单位冻结: 采样值=面亮度（BUNIT 承载，缺省 'ADU' 绝不
  Jy/beam——SCI §9a-8/-11）；coverage 无量纲二值；tile 内 leaf
  local 坐标 fits_index=(511-x)*512+y（DATA_SEMANTICS §3 CDS
  oracle 冻结，nested_local_to_fits_index 权威函数）。
- 端口词汇（wcs_plan/hips/resampled）为 descriptor 派生
  （module_adapters.cpp:363-377），由 P3-RSMP-INT 对齐 DATA-P3-RES，
  不作冻结依据。

## 4 公共 header、核心 symbol 与生命周期

- 内核消费面=API-P3-RSMP-001（p3_resample.h 唯一权威签名头）:
  `p3_sampler_open`（h:32-33，实现 :109+）、`p3_sampler_open_ex`
  （h:36-38，:130-159）、`p3_order_select`（h:21，:82-93）、
  `p3_resample_check_mode`（h:25，:95-107）、
  `p3_sampler_set_max_tiles`（h:42，:161-168）、
  `p3_sample_nearest`（h:46-47，:232-239）、`p3_sample_bilinear`
  （h:51-52，:196-230）、`p3_sampler_close`（h:54，:170-180）+
  数据结构 P3ResampleStatus（h:12-17，OK=0/PARAM=1/UNSUPPORTED=2/
  IO=3）/P3Sampler（h:29-31，impl+last_error[256]）。
- 生命周期: open(_ex)→set_max_tiles（可选，≤0 恢复默认 8）→
  逐像素 sample_nearest/sample_bilinear N 次→close（幂等）；
  sampler 自含 TileCache，**单实例非线程安全**（无内部锁），禁止
  跨线程共享——每 worker 独立实例（§7）；会话编排面 API-P3-001
  FROZEN 五段 create→validate→run→inspect→destroy 不变，run 内
  主 sampler open_ex（p3_session.cpp:171）→每 worker 独立
  open_ex（:222）→逐像素分派（:236-237）→close（:259/:262）。
- 域际: DATA-P3-FITS 写路径 resampled 平面承载本域输出
  （p3_writer_descriptor 端口词汇，API-P3-FITS-001 消费面）。
- 不新增/不修改任何 C 头/C ABI（本页为既有符号展开冻结）。

## 5 Registry descriptor 与配置 schema

- descriptor（module_adapters.cpp:363-377 p3_resample2_descriptor，
  编排层占位词汇）: module_id=`astrocs.phase3.resample2`；
  execution_class=cpu_heavy；parallel_ok=true（每 worker 独立
  sampler 的进程内并行安全，与 §7 结构性一致；单实例内无并发）；
  ports wcs_plan(DATA-P3-WCS 必)+hips(DATA-HIPS-001 必)+resampled
  (DATA-P3-RES 可)；alg_id=ALG-P3-003、data_id=DATA-P3-RES、
  api_id=API-P3-001、test_id=TEST-P3-RES-001——占位 ID/端口由
  P3-RSMP-INT 对齐本页与 lib/phase3_rsmp/module.yaml，不作冻结
  依据。
- 注册序: module_adapters.cpp:754 起序列（phase3_descriptor→
  p3_wcs_descriptor→p3_resample2_descriptor→p3_writer_descriptor，
  :797-798 收尾）；配置=phase config JSON（按 PHASE API 文档）。

## 6 冻结公式（G3/G4 摘要；唯一权威=ALG-P3-RSMP-IMPL-001 §6）

- G3 order 选择（p3_resample.cpp:82-93）: 最小 k∈[0,max_order] 使
  pixel_resolution_arcsec(512<<k)/3600 ≤ scale_deg_per_px
  （pixel_resolution_arcsec=sqrt(π/3)/nside rad，healpix_core.cpp
  权威；与 ALG-P3-003 G3 ceil 式 `order=clamp(ceil(log2(sqrt(π/3)/
  (W·s_out))), 0, hips_order)` 数学等价，nside=W·2^k）；无更细层
  →out_order=max_order（欠采样降级，SCI §9a-5）；会话 max_order=
  输入实际 order（p3_session.cpp:196-199，禁仅写 metadata）。
- G4 NEAREST（:232-239）: 输出像素中心→pix2ang→ang2pix(nside_leaf)
  精确 cell；无插值误差（SCI §9a-12）。
- G4 BILINEAR（:196-230）: leaf 3×3 邻域（healpix_neighbors）四象限
  最近中心（d² 比较）→切平面双线性（den=dx·dy2−dx2·dy≤0 跳过背面；
  |dx|>1e-300 防 0 除；u,v∈[0,1]）→权重 w00=(1−u)(1−v)/w10=u(1−v)/
  w01=(1−u)v/w11=uv（FP64，Σw=1 精确——SCI §9a-7 不变量）；离散化
  方案与 G4 施工规格差异=DISP-P3RSMP-001。
- tile 寻址: tile_ipix=leaf_ipix>>18（leaf_to_tile_nest(leaf,9)，
  W=512→shift=9，SCI §9a-1）；fits_index=(511−x)·512+y
  （DATA_SEMANTICS §3）；值语义表（NaN/C）=ALG §6.6。

## 7 执行类、并行轴、ThreadBudget lease、确定性

- execution_class=cpu_heavy；内核无内部线程（TileCache 每 sampler
  实例自含，p3_resample.cpp:22-36）；并行=会话 worker 池
  （p3_session.cpp:211-214 worker 数=ThreadBudget.max_workers，禁
  hardware_concurrency；>hpx clamp 行带不空；<2 串行；:246-255
  std::thread 池）——**每 worker 独立 sampler+cache**（:217-244
  闭包内 open_ex），HiPS tile 文件只读共享无写锁；CPU-005 禁单线程
  重计算由 worker 池结构性满足。
- 单一 P3Sampler 实例非线程安全（无内部锁）——合同禁止项跨线程
  共享（ALG-P3-RSMP-IMPL-001 §7）。
- 确定性: 逐像素计算路径固定（邻域确定→最近中心确定→权重确定）
  ⇒ 输出与 tile 装载顺序/worker 数/缓存容量无关，bitwise 一致
  （ALG-P3-RSMP-IMPL-001 §7）。

## 8 实测偏差与整改（不修码，唯一权威=ALG-P3-RSMP-IMPL-001 §11）

- DISP-P3RSMP-001: bilinear=切平面四象限最近中心双线性
  （cpp:196-230）；G4 施工规格写"面积重叠分数（投影线性化）"——
  同族一阶插值、Σw=1 不变量一致 → P3-RSMP-IMPL 决策。
- DISP-P3RSMP-002: tile cache 逐出 FIFO（cpp:22-36）；ALG §3 伪代码
  写 "LRU" → P3-RSMP-IMPL。
- DISP-P3RSMP-003: p3_resample_check_mode 会话编排层无调用点
  （仅探针 p3_resample_probe_main.cpp:31 消费）→ P3-RSMP-INT 接线。
- DISP-P3RSMP-004: provenance.missing_tiles 恒 nullptr
  （p3_session.cpp:265-277）——缺 tile 聚合上报未接线（SCI §9a-9）
  → P3-RSMP-IMPL/INT。
- DISP-P3RSMP-005: astrocs_p3_resample.dll 未建（entrypoint=
  MISSING）；探针/回归现状内联编译 → P3-RSMP-IMPL。
- 其余见 docs/KNOWN_LIMITATIONS.md 与 ALG-P3-RSMP-IMPL-001 §13
  合同边界。

## 9 验证与测试面（TEST-P3-RSMP-DESIGN-001 设计冻结 VERIFIED）

- 设计冻结: ALG-P3-RSMP-IMPL-001 §12（①order 选择等价性对拍
  pixel_resolution_arcsec/②seam 连续性 SYN-007 预冻结容差/③NaN/
  coverage 值语义全表/④缺 tile→C=0/⑤nearest 精确 cell/⑥bilinear
  Σw=1 与背面跳过/⑦max_tiles FIFO 逐出/⑧open 守卫负面清单）。
- 双重陈述（C7 锚）: 本节承载 TEST-P3-RES-001 登记面（矩阵
  test_status=DORMANT）；可执行面升级归 P3-RSMP-TEST。
- 现状执行测试（相邻证据，引用不冒认）:
  tests/backend/p3_resample_probe_main.cpp（探针六模式）+
  tests/backend/test_p3_resample.py（156 行，test_05_nan_semantics/
  test_06_no_silent_default_open/seam 域界 1e8-1..12e8+1 连续性
  1e-5°）+ tests/backend/test_p3003_parallel_resampler.py（104 行）+
  tests/unit/p3_interp_test.cpp（109 行）/p3_coverage_test.cpp
  （106 行，独立参考实现非生产自证）。
- EVIDENCE: EVID-MISSING（归 P3-RSMP-INT/验收补）。

## 10 合同链接

- SCI: docs/science/PHASE3_HIPS_TO_FITS.md（FROZEN）
- ALG: docs/algorithms/PHASE3_RSMP_IMPL.md；承接:
  docs/algorithms/PHASE3_RESAMPLE.md（ALG-P3-003 G3/G4 施工规格，
  零改动）
- DATA: docs/contracts/DATA_SEMANTICS.md §29
- API: docs/contracts/PUBLIC_API.md（API-P3-RSMP-001 节）
- 模块总页: docs/modules/phase3_rsmp.md；合同三件套:
  lib/phase3_rsmp/{README.md,module.yaml,memory.md}
