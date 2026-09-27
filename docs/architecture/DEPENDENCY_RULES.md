# Dependency Rules

> 上游：ASTROCS_DESIGN.md §8（软件架构）

- 唯一 I/O 依赖方向：上层模块 → astro_image_io；astro_image_io 不依赖
  上层科学模块。
- common 可被任何模块依赖，依赖方向单向。
- healpix_drizzle 依赖 common/healpix_core，实现只有该一份（B4-01 去重，DRZ-01）。
- phase2 依赖 common/healpix + astro_image_io（aio_upm/aio_hips_reader）。
  ⛔ **生产构建采用无 ACR/CUDA 的链接面**（最高设计 §1.4 非目标：ACR/GPU 生产路由
  生产不可达）。ACR 是 `DORMANT`：保留源码与隔离测试，
  **不进生产构建/加载/路由/benchmark/发布**（最高设计 §1.4）。
- orchestrator（历史保留）依赖所有模块头文件，通过 DllLoader 动态加载 DLL（不静态链接）。
- 依赖图为有向无环图；模块产品输出落块级 `output_dir`（最高设计 §10），开发/CI 过程产物落 `run/`，testdata/ 只读。
- healpix_stack 为冻结件，改动一律走变更流程（依赖已归档 healpix_io.dll，不重建）。
- Python 仅限带 NON_PRODUCTION_TOOL_ONLY 标记的测试/研究脚本。
