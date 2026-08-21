# Dependency Rules

- 唯一 I/O 依赖方向：上层模块 → astro_image_io；astro_image_io 不依赖
  上层科学模块。
- common 可被任何模块依赖，不得反向依赖。
- healpix_drizzle 依赖 common/healpix_core，禁止自带重复实现（B4-01 去重，DRZ-01）。
- phase2 依赖 common/healpix + astro_image_io（aio_upm/aio_hips_reader）
  + acr（kernel_registry/cuda_bridge_loader/device_executor）。
- orchestrator 依赖所有模块头文件，通过 DllLoader 动态加载 DLL（不静态链接）。
- 禁止循环依赖；禁止模块直接写 testdata/run 之外目录。
- healpix_stack 冻结禁止修改（依赖已归档 healpix_io.dll，不重建）。
- Python 仅限带 NON_PRODUCTION_TOOL_ONLY 标记的测试/研究脚本。
