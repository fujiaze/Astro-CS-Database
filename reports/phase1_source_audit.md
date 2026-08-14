# Phase1 源码审计（V17 True Final Freeze）

## 范围与结论

```text
PHASE1_BASE_ALGORITHMS = FROZEN（V14 审核通过后）
V17 审计结论         = NO_HIDDEN_ALTERNATE_PATH
                     = SINGLE_PRODUCTION_CALL_GRAPH
canonical_core 快照  = orchestrator + calibration + plate solve + PSF +
                      photometric + SNR + drizzle + shared HEALPix/AIO
                      （审核包 source/canonical_core/）
```

## 唯一生产调用图（cross-stage）

```text
orchestrator.exe <stage1.json>（唯一 Phase1 入口；lib/orchestrator/cpp）
  ├─ READ_FITS    → lib/astro_image_io（aio_read_fits，唯一 FITS reader）
  ├─ CALIBRATE    → lib/calibration/astro_calibration.dll（master bias/dark/flat）
  ├─ PLATESOLVE   → lib/plate_solve/cpp/ipv/ipv_solver.dll
  │                 ├─ gaia_client.dll（lib/gaia_xpsd_client，本地 GaiaDR3 cone）
  │                 └─ star_detector.dll（lib/star_detector）
  ├─ PSF          → lib/dynamic_psf/dynamic_psf.dll
  ├─ PHOTOMETRIC  → lib/photometric_calib/cpp/photometric_calib.dll
  │                 └─ gaia_client.dll（GaiaDR3SP 光谱查询）
  ├─ SNR          → lib/snr_estimator/cpp/snr_estimator.dll
  ├─ DRIZZLE      → lib/healpix_db/healpix_drizzle/healpix_drizzle.dll
  │                 └─ lib/common/healpix/healpix_core.cpp（唯一 NESTED 映射）
  ├─ HIPS_WRITE   → lib/astro_image_io（aio_hips_writer，唯一 HiPS writer）
  └─ HIPS_VERIFY  → lib/astro_image_io（aio_hips_reader 回读校验）

stage2 唯一入口：astrocs-stage2.exe（lib/phase2/tools/stage2.cpp），
orchestrator 不再持有 legacy Stage2 wiring（V17 已删除）。
```

## 无隐藏路径证据

1. **入口唯一**：`rg "run_stage_"` 显示每个阶段只有一个 handler 且由
   `PipelineStageV2` 表驱动（orchestrator.cpp:5189 起）；阶段枚举无
   GRADIENT_SPHERE/STACK（V17 删除）。
2. **模块唯一**：`tools/no_legacy_production_reference.py` PASS——生产源码
   （去注释后）无 healpix_stack/legacy Stage2 符号引用；orchestrator 运行时
   只加载 7 个 DLL（AIO/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE）。
3. **I/O 唯一**：FITS/HiPS 读写全部经 lib/astro_image_io（AIO）；HISS 仅
   DEPRECATED 产品格式，保留 reader 兼容（非 science 路径）。
4. **HEALPix 映射唯一**：NESTED local↔FITS index 只有
   lib/common/healpix/healpix_core.cpp 一套（browser healpix_math 第二套
   已在 V15 删除/委托）。
5. **Phase1 产物验证**：16 帧真实 E2E 全部 rc=0；HIPS_VERIFY 回读通过。

## 性能热点（V17 G8，真实 16 帧 NGC1727 H-alpha 1200s）

```text
READ_FITS     median 0.10s  p95 0.12s
CALIBRATE     median 0.49s  p95 0.54s
PLATESOLVE    median 15.4s  p95 17.6s   （含星点检测 + GaiaDR3 cone）
PSF           median 1.27s  p95 1.41s
PHOTOMETRIC   median 5.86s  p95 5.97s   （含 GaiaDR3SP 光谱查询）
SNR/NSIDE     ~0s
DRIZZLE       median 77.6s  p95 87.3s   ← 主导（冻结热路径）
HIPS_VERIFY   median 0.03s
整帧 wall     median ~145s
```

65s vs 150s 历史差异解释：V14 的 65s 基线为 NGC55 Red 全帧（不同
目标/滤镜/曝光；Drizzle 输出 tile 数与 catalogue 查询面不同）；本队列为
NGC1727 H-alpha 1200s 全 4096²，PLATESOLVE 做本地 GaiaDR3 cone（无
网络）、DRIZZLE 写 order-7 HiPS（14 tile + hierarchy）。非算法回归，
是数据集与输出规模的差异；V17 以同队列 before/after 为唯一比较基准。

## 冻结边界

- 本轮不修改 Phase1 算法；性能优化只允许：
  a) platesolve hint（initial_ra/dec 仅搜索初始化，逐帧求解+验证，
     config 层既有字段，不改变求解语义）；
  b) 批量运行器 warm 复用（工具层，不进入生产算法）；
  c) 经 profiler 证明后的并行核（本轮未实施，Drizzle 保持冻结）。
