# Dependency Rules

> 上游：docs/ASTROCS_DESIGN.md §8（软件架构）、§12.5（状态阶梯（唯一口径））

- 唯一 I/O 依赖方向：上层模块 → aio（文件级唯一 I/O 边界）；aio 不依赖
  上层科学模块（最高设计 §10）。
- 共享面（`astrocs_common`，含 SHA-256 与共享 HEALPix 核心）可被任何模块依赖，依赖方向单向。
- healpix_drizzle 依赖共享 HEALPix 核心，实现单源 = `lib/algorithms/shared/healpix/healpix_core.cpp`（CMake target `astrocs_common`）。
- phase2 依赖共享 HEALPix 核心 + aio 的产品读写面（`aio_upm` / `aio_hips_reader`）。
  ⛔ **生产构建采用无 ACR/CUDA 的链接面**（最高设计 §1.4 非目标：ACR/GPU 生产路由
  生产不可达）。ACR 为 `DORMANT`：保留源码与隔离测试，
  **不进生产构建/加载/路由/benchmark/发布**（最高设计 §1.4；状态词唯一口径 = 最高设计 §12.5）。
- 唯一动态加载路径 = 安全 loader：加载前过固定检查（CPU 特征、OS 可安全执行状态、manifest、哈希、ABI），
  只认清单授权的绝对路径，失败只报错、不回退搜索（最高设计 §8.4）。
- 依赖图为有向无环图；模块产品输出落块级 `output_dir`（最高设计 §10），开发/CI 过程产物落 `run/`，testdata/ 只读。
- 归档面（`lib/infrastructure/aio/healpix_db/`）不进生产构建、不重建；其内容改动一律走变更流程。
- Python 仅限带 NON_PRODUCTION_TOOL_ONLY 标记的测试/研究脚本。
