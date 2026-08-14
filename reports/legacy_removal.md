# Legacy 多路径移除（V17 True Final Freeze）

## 移除对象（active tree → archive/legacy）

```text
lib/healpix_db/healpix_stack/*
  gradient_fitter / gradient_sampler / corrected_stacker / stack_engine
  自有 healpix_core / hp_stack_api / AHPS/HISS writer
→ git mv 到 lib/healpix_db/archive/legacy/healpix_stack/（V17 commit 03d5d96）
```

## Orchestrator legacy Stage2 wiring

```text
dll_loader：GRADIENT_SPHERE/STACK 枚举保留但不再 load/init/version/unload
orchestrator：PipelineStageV2::GRADIENT_SPHERE/STACK 枚举删除；
              legacy handler 删除；run_stage2 硬失败并提示
              "legacy Stage2 removed in V17; use astrocs-stage2"
Makefile：healpix_stack include 移除
运行时：只加载 7 个模块（AIO/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/
        DRIZZLE）
```

## 归档约束（archive/legacy/healpix_stack）

```text
不参与 default build（Makefile 独立，默认 make 不进入）
不 link / 不 install / 不进入 production tests
production source 不 include/ref（no_legacy_production_reference.py PASS）
`.gitignore` 忽略 archive/（git mv 已从版本控制移除跟踪）
```

## 配置别名移除

```text
rejection.low / high / max_iterations / min_samples：
  parser 出现即硬错误（提示 tools/migrate_stage2_config.py）
  schema/template/consistency check 同步
migrate 工具：tools/migrate_stage2_config.py（低/高/max_iterations/
  min_samples → typed params；profile→wbpp_2_9_1；
  normalization→astrocs_*_v1）
```

## Gate

```text
no_legacy_production_reference.py = PASS（去掉注释后扫描生产源码，无
  legacy 科学逻辑引用）
make（orchestrator）= rc 0（V17 legacy 移除后的构建修复已含）
74/74 synthetic gate PASS
```

```text
DUPLICATE_PRODUCTION_SCIENCE_PATH = 0
LEGACY_REMOVAL = PASS
```
