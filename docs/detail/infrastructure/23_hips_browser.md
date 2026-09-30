# hips_browser（HiPS 浏览器组件，astrocs.hips_browser / healpix_browser_qt）

> 上游：docs/ASTROCS_DESIGN.md §8.1（顶层结构：hips_browser 基建目录）、§8.5
> （模块与 ABI）、§1.4（非目标：Alpha 不含 GUI）、§6.3（`visualization` 模式标注
> 「不可测量」）
> 数据正本：docs/science/DATA_SEMANTICS.md §12（DATA-P1-HIPS，读侧复用；
> 依赖 DATA-HIPS-*）
> 读侧：registry/infrastructure/17_aio.md（读侧复用 aio，不复制 reader）
> 引用规范：[IVOA HiPS 1.0 Recommendation](https://www.ivoa.net/documents/HiPS/)
> （IVOA 2017；渲染与层级切分规范）

**模块名与实现名**：模块名 = `hips_browser`（基建目录
`lib/infrastructure/hips_browser/`）；实现名 = `healpix_browser_qt`（Qt6 +
OpenGL，STF 显示，可选构建），落地目录 =
`lib/infrastructure/hips_browser/healpix_browser_qt/`。两者指同一模块。

## 1. 职责与边界

- **职责**：GUI 可视化组件，浏览 HiPS 产品（渲染、缩放、导航）与工程诊断。
- **不是**：**不进产品 manifest、不参与 CLI 命令、不定义科学结果**；不参与科学
  处理链；是可选的、独立于发行清单的可视化工具（地位 = 可选组件）。无科学 ID
  （展示层）。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §1.4（非目标（明确不做）：Alpha 不含 GUI）、§8.1（顶层结构：hips_browser 基建目录）
- [IVOA HiPS 1.0 Recommendation](https://www.ivoa.net/documents/HiPS/)（IVOA 2017；渲染与层级切分规范）

## 3. 输入/输出数据合同

- **输入**：HiPS 产品（signal/variance/coverage/...，只读）。
- **输出**：可视化（渲染图像、导航状态、诊断视图）。
- 参考：`docs/science/DATA_SEMANTICS.md` §12（DATA-P1-HIPS：HiPS 产品数据合同，读侧复用）。

## 4. 算法与公式要点

- 渲染：按 HiPS order / tile 加载、LOD、颜色映射（诊断用，不冒充测量）；
- 导航：平移 / 缩放 / 坐标读出；
- 可视化允许显示型降级，**但产物只作显示面**（最高设计 §6.3：`visualization`
  模式标注「不可测量」）；
- 读侧复用 aio，不复制 reader。
- **显示变换（STF）**：显示变换状态 `DisplayTransformState{black, white,
  midtones, curve, compression, generation}`，预设模式 `STFMode`，参数结构
  `STFParams{shadows, highlights, midtones, compression}`；黑 / 白点取数据的
  0.5% / 99.5% 分位 —— `STFEngine::get_preset` 由数据 min / max 与分位裁剪点
  构造显示变换，`normalize()` 归一到 [0,1]；MTF 曲线 `STFEngine::mtf(x, m)`。
  复杂度 O(n)，用 `std::nth_element` 选分位、不整排序。**显示拉伸不改变数据**
  （只读）。
- 性能特征：tile cache + 异步 I/O。线程：Qt 主线程 + 后台 tile I/O；GL 单线程。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `lod_max_order` | —— | —— | 最大渲染 order |
| `color_map` | `inferno` | —— | 调色板 |
| `stretch` | `sqrt` | —— | 显示拉伸 |

## 6. 接口/ABI

- 独立可执行 / 组件；不进入 `acsd` 命令树；模块落点 =
  `lib/infrastructure/hips_browser/healpix_browser_qt/`（`app/` 的 `main.cpp`
  （GUI 入口 `main`）/ `browser_cli.cpp`（命令行入口 `main`）/ `main_window.cpp`；
  `core/` 的 `browser_backend.cpp` / `hips_browser_backend.cpp` /
  `hips_sky_view.cpp` / `healpix_math.cpp` / `gl_renderer.cpp`）。
- 头面 = `include/healpix_browser_core.h`（core 面）+ `core/{stf_engine.h,
  hips_sky_view.h, gl_renderer.h, browser_backend.h}` + Qt 层
  `widgets/{abstract_view, hips_view, sphere_view}.{h,cpp}`、
  `app/{main_window, stf_bar, stf_panel}.{h,cpp}`。
- 读侧走 aio（HiPS tiles 经 `astro_image_io` 读面）。
- 所有权：Qt parent-child；renderer 只读共享。

## 7. 错误与边界

- 产品不可读 / 损坏 → 明确报错（状态显示），**不渲染伪图像**；
- 显示拉伸不改变数据（只读）；
- 非产品构建（不打包进发布 manifest）。

## 8. 测试与 Oracle

- 读侧复用 aio 的测试；
- STF engine 单测（`lib/infrastructure/hips_browser/healpix_browser_qt/tests/test_stf_engine.cpp`）；
- 渲染正确性（已知产品 → 期望像素值）；
- 导航 / 坐标读出测试；
- 视觉验收按 `ACCEPTANCE_SPEC.md` 的 L4 清单执行；
- 与科学产品隔离验证（不写产品、不影响测量路径）。

## 9. 已知限制

- Alpha 不含 GUI（最高设计 §1.4）；本组件是可选工具，不进产品 manifest；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。

## 10. 源文件

`lib/infrastructure/hips_browser/healpix_browser_qt/`。
