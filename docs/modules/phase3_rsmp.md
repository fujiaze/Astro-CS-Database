# Phase3 HiPS 重采样模块（astrocs.p3.resample）

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 模块总页；registry 登记页 = docs/modules/registry/astrocs.phase3.resample2.md；
> 合同三件套 = lib/algorithms/resample/（落位规则见 docs/modules/README.md）。

## 1 身份

- module_id: astrocs.p3.resample；registry 行 MOD-astrocs-phase3-resample2；
  dll_target=astrocs_p3_resample.dll（合同值，尚未存在，entrypoint 未落地，
  IMPLEMENTED 只由验收签发）；现状构建=astrocs_phase3_session 静态库
  成员（p3_resample.cpp 为其五源文件之一）。
- owner: SA-P3-S26；language: c++17；abi_version: 1；
  phase_scope: phase3；resource_class: cpu_heavy；
  threading_model: host_executor_lease（迁移目标合同值；现状=内核
  无内部线程，并行由会话 worker 池每 worker 独立 sampler 组织）。
- 相邻占位行注记: registry 行 MOD-astrocs-phase3-resample
  （phase3_descriptor 端口词汇）不属本域页，其对齐属迁移目标（未落地），
  本页不触碰。

## 2 合同链

| 层 | ID | 权威落位 | 状态 |
|---|---|---|---|
| SCI | SCI-P3-001 | docs/science/PHASE3_HIPS_TO_FITS.md（共享 FROZEN；映射声明 SCI-P3-RES-001⇒SCI-P3-001 见 ALG §5） | FROZEN |
| ALG | ALG-P3-RSMP-IMPL-001 | docs/science/algorithms/PHASE3_RSMP_IMPL.md（兼承接 ALG-P3-003 G3/G4 本域子面） | CONTRACT_READY |
| DATA | DATA-P3-RES | docs/contracts/DATA_SEMANTICS.md §29 | CONTRACT_READY |
| API | API-P3-RSMP-001 | docs/contracts/PUBLIC_API.md（Phase3 重采样公共消费面节） | CONTRACT_READY |
| API(镜像) | API-P3-001 | PUBLIC_API.md（p3_session 五段编排面 FROZEN 镜像） | FROZEN 镜像 |
| ARCH | ARCH-001 | docs/contracts/ARCH-001.md | VERIFIED |
| TEST | TEST-P3-RES-001 | 登记面=TEST-P3-RSMP-DESIGN-001（ALG §12 + registry 页 §9 双重陈述 VERIFIED）；矩阵 test_status=DORMANT，可执行面待落地 | 见左 |
| EVID | 执行证据 | 验收证据待补（未取得） | NOT_VERIFIED |

## 3 职责

- HiPS tile 重采样（G4）：NEAREST 精确 cell / BILINEAR 一阶插值
  （3×3 邻域四象限最近中心、切平面权重、Σw=1 冻结不变量）；G3
  order 选择（p3_order_select，pixel_resolution_arcsec 对拍等价）；
  sampler 生命周期（open/open_ex/set_max_tiles/close）；输入模式
  守卫（surface_brightness 唯一合法，flux/variance/weight 显式拒）；
  有界 tile 缓存（FIFO）；coverage 二值语义（SCI §5）。
- 非职责: 投影/WCS（phase3_proj 域）、FITS 文件读写（phase3_fits
  域）、会话编排（p3_session 域）、第三种采样核（SCI §9a-7 之外
  显式拒）、variance/weight/ivar/flux-per-pixel 输入与 alpha
  channel/BLANK int tile（SCI §9a-8/-9/-10 显式拒）。

## 4 生产源

- lib/algorithms/resample/p3_resample.h（202 行，签名头正本）+
  lib/algorithms/resample/p3_resample.cpp（586 行）——十符号:
  P3ResampleStatus/p3_order_select/p3_resample_check_mode/
  P3SamplerImpl/P3Sampler/p3_sampler_open/p3_sampler_open_ex/
  p3_sampler_set_max_tiles/p3_sample_nearest/p3_sample_bilinear/
  p3_sampler_close。
- 会话消费点: p3_session.cpp :16/:167-178（open_ex :171）/:179-194
  （max_tiles 守卫）/:196-199（order 选择）/:217-244（worker 闭包
  每 worker 独立 sampler+cache）/:236-237（nearest/bilinear 分派）/
  :258-263（cancel）/:265-277（provenance）；域际消费:
  lib/algorithms/shared/healpix（leaf_to_tile_nest/tile_to_leaf_nest/
  nested_local_to_fits_index/ang2pix/pix2ang 权威函数）。
- 执行面: eng/tests/backend/p3_resample_probe_main.cpp（探针六模式）+
  eng/tests/backend/test_p3_resample.py（seam/NaN/无静默默认）+
  eng/tests/backend/test_p3003_parallel_resampler.py +
  eng/tests/unit/p3_interp_test.cpp / p3_coverage_test.cpp（独立参考实现）。

## 5 端口与 DATA

| 端口 | DATA | 必/可 | 说明 |
|---|---|---|---|
| wcs_plan | DATA-P3-WCS | 必 | §28 权威源（输出平面几何上游） |
| hips | DATA-HIPS-001 | 必 | HiPS tile/properties 输入面（tile 读路径权威=DATA_SEMANTICS §3） |
| resampled | DATA-P3-RES | 可 | §29 权威源（value/coverage/单位/dtype/invalid） |

端口词汇为 descriptor 派生（p3_resample2_descriptor，
module_id=astrocs.phase3.resample2），其对齐属迁移目标（未落地），不作
冻结依据。

## 6 实测偏差与现行语义（权威源 = ALG-P3-RSMP-IMPL-001 §11）

- bilinear 离散化为四象限最近中心（cpp:196-230），G4 施工规格写「面积重叠
  分数」——同族一阶插值、Σw=1 一致。
- tile cache 逐出为 FIFO（cpp:22-36），ALG §3 写「LRU」。
- p3_resample_check_mode 会话层未接线（仅探针消费）。
- provenance.missing_tiles 恒 nullptr（:265-277），缺 tile 聚合上报未接线。
- DLL/入口未建（迁移目标未落地）。
- 其余见 ALG-P3-RSMP-IMPL-001 §11/§13 合同边界。

## 7 链接

- registry 登记页: docs/modules/registry/astrocs.phase3.resample2.md
- ALG: docs/science/algorithms/PHASE3_RSMP_IMPL.md
- DATA: docs/contracts/DATA_SEMANTICS.md §29
- API: docs/contracts/PUBLIC_API.md（API-P3-RSMP-001）
- SCI: docs/science/PHASE3_HIPS_TO_FITS.md（FROZEN，零改动）
- 同域: docs/modules/phase3_proj.md（WCS 域）、
  docs/modules/phase3_fits.md（写出域）、
  phase3_session 模块页未建（会话编排层不单独成模块）；会话编排面现行权威 =
  `docs/contracts/RT-001.md` + `docs/modules/registry/astrocs.phase3.*`。
