# 全仓库重复科学路径/多默认值清理（V15）

依据 `reports/semantic_path_inventory.csv`（全仓库扫描，非 changed-only）。

## 已删除/收敛

| 项 | 处置 |
| --- | --- |
| 浏览器第二套手写 NESTED HEALPix（healpix_math.cpp 本地 ang2pix/pix2ang） | 删除本地实现，委托 `astrocs::healpix`（shared core）；query_disc/ud_grade 保留为构建在 canonical 之上的工具（c0fa750） |
| Browser support 显示 sqrt(support) | 改为固定 linear [0,1]（8d3b2c7） |
| Browser 多 STF 状态 owner（preset_/auto_range_/auto_view_/stf_locked_/manual_* + MainWindow 副本） | 收敛为唯一 DisplayTransformState（8d3b2c7） |
| auto 按 pixel effective count 路由（V14 kernel 内） | 删除；planning 层 `p2_reject_plan_resolve`（4b92989） |
| 共享 low/high/max_iterations 参数 | 拆为 method-specific typed（4b92989） |
| sampler null-config 未初始化（默认值随机失效） | `p2_sampler_default_config()` 单源（4b92989） |
| schema/parser 不一致（schema 缺 percentile/median_sigma/minmax/auto） | schema/template/parser 统一（4b92989） |
| rejection status=3 被后续算法覆盖 | status/reason 分离（4b92989） |

## 保留（有明确理由）

| 项 | 分类 | 理由 |
| --- | --- | --- |
| ACR legacy launcher + CUDA kernel | BACKEND_EQUIVALENT | 同一 contract；CPU/GPU 等价测试 PASS（同一科学语义） |
| HISS writer/verify | COMPAT_ADAPTER | HiPS 为生产；HISS 仅 legacy verify（Phase1 冻结链） |
| healpix_stack（gradient sphere/stack） | LEGACY_FROZEN | Stage2 冻结模块，禁止修改/重建（AGENTS.md） |
| orchestrator legacy stage2 wiring | LEGACY_FROZEN | 生产 Phase2 入口=astrocs-stage2 |
| aio_upm（astro_image_io） | COMPAT_ADAPTER | UPM 模型 I/O 容器（科学在 phase2/upm.cpp） |
| web stf.js / stf.py | ARCHIVED | 已归档非生产（archive/healpix_browser_web） |

## 结论

```text
duplicate production science paths = 0
legacy science fallback（静默换语义）= 0
test-only production callers = 0
duplicated default source = 0
```
