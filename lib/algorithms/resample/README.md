# lib/algorithms/resample — astrocs.p3.resample（HiPS 重采样域合同）

> P3-RSMP-DOC 冻结（2026-09-12，SA-P3-R）。本目录为 Phase3 HiPS 重采样域
> 迁移合同落位（三件套：README + module.yaml + memory.md），照
> lib/algorithms/projection→phase3_fits 迁移目录先例新建。**现状**（行数实测
> 2026-09-16：`timeout 60 wc -l lib/algorithms/resample/p3_resample.h
> lib/algorithms/resample/p3_resample.cpp`）：生产源实际位于 lib/phase3_session/
> （p3_resample.h 150 行 + p3_resample.cpp 519 行，astrocs_phase3_session
> 静态库成员，根 CMakeLists.txt `add_library(astrocs_phase3_session …)`）；
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
| ARCH | ARCH-001 | eng/cmake/ARCH-001-migration-manifest.md | VERIFIED |
| API(镜像) | API-P3-001 | docs/contracts/PUBLIC_API.md（p3_session 五段编排面 FROZEN 镜像，不变） | FROZEN 镜像 |
| TEST | TEST-P3-RES-001 | 登记面=TEST-P3-RSMP-DESIGN-001（设计冻结 VERIFIED，ALG-P3-RSMP-IMPL-001 §12 + registry 页 §9 双重陈述）；现状执行证据=tests/backend/p3_resample_probe_main.cpp + test_p3_resample.py（162 行，2026-09-16 实测）+ test_p3003_parallel_resampler.py + tests/unit/p3_interp_test.cpp / p3_coverage_test.cpp；验收升级归 P3-RSMP-TEST | 见右 |
| EVID | EVID-MISSING | 待 P3-RSMP-INT/验收补 | MISSING |

## 3 生产源（冻结实测，2026-09-12）

- 唯一权威签名头: `lib/algorithms/resample/p3_resample.h`（150 行，2026-09-16 实测）——
  公共符号（按 `path::symbol` 定位，不冻结行号）: `P3ResampleStatus` /
  `p3_order_select` / `p3_resample_check_mode` / `P3SamplerImpl`(前置声明) /
  `P3Sampler` / `p3_sampler_open` / `p3_sampler_open_ex` /
  `p3_sampler_set_max_tiles` / `p3_sampler_attach_cache` / `P3CacheStats` /
  `p3_sampler_cache_stats` / `p3_sampler_set_absent_cache` /
  `p3_sample_nearest[_ex]` / `p3_sample_bilinear[_ex]` / `p3_sampler_close` /
  `p3_uncertainty_open` / `p3_uncertainty_close` / `p3_uncertainty_propagate`
  （uncertainty 面按 DATA-P3-UNC-001 §30.4-4 supersession 追加）。
  **注意**：ALG-P3-RSMP-IMPL-001 §4 仍记「全部公共符号（10 个）/58 行」，
  与本节实测不符——该滞后口径属 M1a-C-007，本 README 按实测记录，不复抄旧值。
- 实现: `lib/algorithms/resample/p3_resample.cpp`（519 行，2026-09-16 实测；
  按 `path::symbol` 定位）: `kTileWidth=512`、`SharedTileCache`
  （**跨 worker 共享、有界 LRU + 缺失负缓存**，P30 `955c45df`；容量 = max_tiles，
  与 worker 数无关）、`read_leaf`、`p3_order_select`、`p3_resample_check_mode`、
  `p3_sampler_open[_ex]`、`p3_sampler_set_max_tiles`、`p3_sampler_attach_cache`、
  `p3_sample_nearest_ex`、`p3_sample_bilinear_ex`、`p3_uncertainty_open`、
  `p3_uncertainty_propagate`、`p3_sampler_close`。
- 会话消费点: `lib/phase3_session/p3_session.cpp::p3_session_run` ——
  `p3_resample.h` include、`p3_sampler_open_ex`（暴露实际 order/BUNIT）、
  max_tiles 会话守卫（可降不可升）、`p3_order_select`（max_order=输入实际
  order）、`p3_sampler_attach_cache`（P30 共享缓存）、`p3_uncertainty_open`、
  行级取消、provenance `order_sel_used` 填充。
- 逐符号源码锚、冻结公式（G3 order 选择/G4 采样核）、错误语义、
  并发/确定性合同：ALG-P3-RSMP-IMPL-001 §2-§10。

## 4 职责与非职责

- 职责: sampler 生命周期（open/open_ex/set_max_tiles/close）、G3
  order 选择、输入模式守卫（surface_brightness 唯一合法）、
  NEAREST/BILINEAR 逐输出像素采样（含跨 tile 邻域读取）、tile 缓存
  （跨 worker 共享的有界 LRU + 缺失负缓存，P30 `955c45df`）、coverage 二值语义。
- 非职责: 不做投影/WCS 构造（ALG-P3-PROJ-IMPL-001 归 phase3_proj 域）、
  不做 FITS 原子写（p3_output 域）、不做会话编排与请求解析
  （p3_session 域）、不修改 SCI 公式（SCI-P3 FROZEN 零改动）、
  不把 variance/ivar 当 `input_mode` 拒绝项（DATA-P3-UNC-001 §30.4-4
  supersession：variance/ivar 转 uncertainty 子产品消费面）；
  `weight`/`flux-per-pixel` 仍显式拒（SCI §9a-8/§9a-10 不变）。

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

- DISP-P3RSMP-001: bilinear 为切平面四象限最近中心双线性
  （`p3_sample_bilinear_ex`）；G4 施工规格写"面积重叠分数（投影线性化）"
  ——同族一阶插值、Σw=1 不变量一致，离散化方案不同。
- DISP-P3RSMP-002（**已由 P30 整改取代，2026-09-16 复测**）：原记
  "tile cache 逐出 FIFO"；现状 `SharedTileCache`（`p3_resample.cpp`）为
  跨 worker 共享的有界 **LRU**（`std::list lru` front=MRU，`get` splice 提升，
  `put` 逐出 `lru.back()`）+ `std::mutex` 互斥，与 ARCH-P3 §1/§3 表述一致。
- DISP-P3RSMP-003: p3_resample_check_mode 会话编排层无调用点
  （能力在内核，探针消费）。
- DISP-P3RSMP-004: provenance.missing_tiles 恒 nullptr（缺 tile 聚合
  上报未接线，`p3_session.cpp` 中 `prov.missing_tiles` 赋值点）。
- DISP-P3RSMP-005: astrocs_p3_resample.dll 未建（entrypoint 缺失）。
