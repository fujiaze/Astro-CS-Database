# Dependency Rules

> 上游：ASTROCS_DESIGN.md §8（软件架构）

- 唯一 I/O 依赖方向：上层模块 → astro_image_io；astro_image_io 不依赖
  上层科学模块。
- common 可被任何模块依赖，依赖方向单向。
- healpix_drizzle 依赖 common/healpix_core，实现只有该一份（B4-01 去重，DRZ-01）。
- phase2 依赖 common/healpix + astro_image_io（aio_upm/aio_hips_reader）。
  ⛔ **生产构建采用无 ACR/CUDA 的链接面**（最高设计 §8「生产目标只编译链接非 ACR 源文件；
  GPU 路由开关与第二个可执行入口不在合同面内」）。ACR 是 `DORMANT`：保留源码与隔离测试，
  **不进生产构建/加载/路由/benchmark/发布**（最高设计 §1.3/§8）。
- orchestrator 依赖所有模块头文件，通过 DllLoader 动态加载 DLL（不静态链接）。
- 依赖图为有向无环图；模块写出位置限于 testdata/run。
- healpix_stack 为冻结件，改动一律走变更流程（依赖已归档 healpix_io.dll，不重建）。
- Python 仅限带 NON_PRODUCTION_TOOL_ONLY 标记的测试/研究脚本。
