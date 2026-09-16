# lib/phase3_rsmp — astrocs.p3.resample（HiPS 重采样域合同）

> P3-RSMP-DOC 冻结（2026-09-12，SA-P3-R）。本目录为 Phase3 HiPS 重采样域
> 迁移合同落位（三件套：README + module.yaml + memory.md），照
> lib/phase3_proj→phase3_fits 迁移目录先例新建。**现状**：生产源实际
> 位于 lib/phase3_session/（p3_resample.h 58 行 + p3_resample.cpp 239 行，
> astrocs_phase3_session 静态库成员，根 CMakeLists.txt:460-465）；
> dll_target=astrocs_p3_resample.dll 为矩阵合同值，尚未存在
> （entrypoint=MISSING，DISP-P3RSMP-005，由 P3-RSMP-IMPL 建立，
> 禁止声明 IMPLEMENTED）。

## 1 身份

- module_id: **astrocs.p3.resample**（MODULE_MIGRATION_MATRIX P3-RSMP 行
  权威值；registry descriptor 占位 module_id=astrocs.phase3.resample2
  由 P3-RSMP-INT 对齐，不作冻结依据）。
- registry 行: MOD-astrocs-phase3-resample2；dll_target:
  astrocs_p3_resample.dll（合同值，未建）。
- 域: HiPS tile 重采样（NEAREST 精确 cell / BILINEAR 一阶插值显式两核；
  第三种采样核非本合同）。

## 2 合同链（ID 唯一权威落位）

| 层 | ID | 权威落位 | 状态 |
|---|---|---|---|
| SCI | SCI-P3-001 | docs/science/PHASE3_HIPS_TO_FITS.md（共享 FROZEN，映射声明 SCI-P3-RES-001⇒SCI-P3-001 见 ALG-P3-RSMP-IMPL-001 §5） | FROZEN |
| ALG | ALG-P3-RSMP-IMPL-001 | docs/algorithms/PHASE3_RSMP_IMPL.md（本域实现级合同，兼承接 ALG-P3-003 本域子面 G3/G4） | CONTRACT_READY |
| DATA | DATA-P3-RES | docs/contracts/DATA_SEMANTICS.md §29 | CONTRACT_READY |
| API | API-P3-RSMP-001 | docs/contracts/PUBLIC_API.md（Phase3 重采样公共消费面节） | CONTRACT_READY |
| ARCH | ARCH-001 | docs/architecture/cpu/ARCH_CONTRACTS.md | VERIFIED |
| API(镜像) | API-P3-001 | docs/contracts/PUBLIC_API.md（p3_session 五段编排面 FROZEN 镜像，不变） | FROZEN 镜像 |
| TEST | TEST-P3-RES-001 | 登记面=TEST-P3-RSMP-DESIGN-001（设计冻结 VERIFIED，ALG-P3-RSMP-IMPL-001 §12 + registry 页 §9 双重陈述）；现状执行证据=tests/backend/p3_resample_probe_main.cpp + test_p3_resample.py（156 行）+ test_p3003_parallel_resampler.py + tests/unit/p3_interp_test.cpp / p3_coverage_test.cpp；验收升级归 P3-RSMP-TEST | 见右 |
| EVID | EVID-MISSING | 待 P3-RSMP-INT/验收补 | MISSING |

## 3 生产源（冻结实测，2026-09-12）

- 唯一权威签名头: lib/phase3_session/p3_resample.h（58 行）——
  P3ResampleStatus（h:12-17）/p3_order_select（h:21）/
  p3_resample_check_mode（h:25）/P3SamplerImpl 前置声明（h:28）/
  P3Sampler（h:29-31）/p3_sampler_open（h:32-33）/p3_sampler_open_ex
  （h:36-38）/p3_sampler_set_max_tiles（h:42）/p3_sample_nearest
  （h:46-47）/p3_sample_bilinear（h:51-52）/p3_sampler_close（h:54）。
- 实现: lib/phase3_session/p3_resample.cpp（239 行；kTileWidth=512 :18、
  TileCache FIFO :22-36、read_leaf :49-80、order_select :82-93、
  check_mode :95-107、open/open_ex :109-159、set_max_tiles :161-168、
  close :170-180、bilinear :196-230、nearest :232-239）。
- 会话消费点: p3_session.cpp:16（include）/:167-178（主 sampler open_ex :171
  open_ex，暴露实际 order/BUNIT）/:179-194（max_tiles 守卫，可降
  不可升）/:196-199（p3_order_select，max_order=输入实际 order）/
  :217-244（每 worker 独立 sampler+cache，nearest/bilinear 分派）/
  :258-263（行级取消）/:265-277（provenance）。
- 逐符号源码锚、冻结公式（G3 order 选择/G4 采样核）、错误语义、
  并发/确定性合同：ALG-P3-RSMP-IMPL-001 §2-§10。

## 4 职责与非职责

- 职责: sampler 生命周期（open/open_ex/set_max_tiles/close）、G3
  order 选择、输入模式守卫（surface_brightness 唯一合法）、
  NEAREST/BILINEAR 逐输出像素采样（含跨 tile 邻域读取）、tile 缓存
  （有界 FIFO）、coverage 二值语义。
- 非职责: 不做投影/WCS 构造（ALG-P3-PROJ-IMPL-001 归 phase3_proj 域）、
  不做 FITS 原子写（p3_output 域）、不做会话编排与请求解析
  （p3_session 域）、不修改 SCI 公式（SCI-P3 FROZEN 零改动）、
  不引入 variance/weight/ivar/flux-per-pixel 输入（SCI §9a-8/10
  显式拒）。

## 5 co-located 合同链接

- SCI: [docs/science/PHASE3_HIPS_TO_FITS.md](../../docs/science/PHASE3_HIPS_TO_FITS.md)（FROZEN）
- ALG: [docs/algorithms/PHASE3_RSMP_IMPL.md](../../docs/algorithms/PHASE3_RSMP_IMPL.md)
- ALG(承接): [docs/algorithms/PHASE3_RESAMPLE.md](../../docs/algorithms/PHASE3_RESAMPLE.md)（ALG-P3-003 施工规格，公式零改动）
- DATA: [docs/contracts/DATA_SEMANTICS.md](../../docs/contracts/DATA_SEMANTICS.md) §29
- API: [docs/contracts/PUBLIC_API.md](../../docs/contracts/PUBLIC_API.md)
- 模块页: [docs/modules/phase3_rsmp.md](../../docs/modules/phase3_rsmp.md)；
  registry 手写页: docs/modules/registry/astrocs.phase3.resample2.md
- 迁移模板: 工程控制/…/tasks/MODULE_MIGRATION_TEMPLATE.md（P3-RSMP-DOC）

## 6 实测偏差登记（不改码，详见 ALG-P3-RSMP-IMPL-001 §11）

- DISP-P3RSMP-001: bilinear 为切平面四象限最近中心双线性（cpp:196-230）；
  G4 施工规格写"面积重叠分数（投影线性化）"——同族一阶插值、Σw=1
  不变量一致，离散化方案不同。
- DISP-P3RSMP-002: tile cache 逐出 FIFO（cpp:22-36）；ALG §3 伪代码
  写 "LRU"。
- DISP-P3RSMP-003: p3_resample_check_mode 会话编排层无调用点
  （能力在内核，探针消费）。
- DISP-P3RSMP-004: provenance.missing_tiles 恒 nullptr（缺 tile 聚合
  上报未接线，p3_session.cpp:265-277）。
- DISP-P3RSMP-005: astrocs_p3_resample.dll 未建（entrypoint 缺失）。
