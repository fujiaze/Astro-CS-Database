# healpix_stack 源码来源记录

## 独立仓库信息
- 远端: https://github.com/fujiaze/Healpix-Mosaic-Cpp.git
- 分支: main
- Commit: 027b64f51ec365a223816faf3ca9801499e2db9f
- 日期: 2026-07-16T12:29:24+08:00
- 最后提交: refactor(stack): link aio.dll for HEALPix I/O (spec G1 Phase 1)

## 第三方依赖
- nanoflann.hpp (BSD 许可证, header-only, Jose Luis Blanco et al.)
  - 位于 gradient/nanoflann.hpp
  - 用途: KD-tree 近邻搜索（梯度采样球面最近点查询）

## 纳入主仓库操作
- 日期: 2026-07-24
- 操作: 删除嵌套 .git，清理编译产物，git add 纳入主仓库
- 源码文件未被修改

## 已清理（不入库）
- healpix_stack.dll (1,471,655 bytes)
- test_corrected_stacker.exe (456,171 bytes)
- test_gradient_fitter.exe (431,018 bytes)
- test_gradient_sampler.exe (500,483 bytes)
- test_spherical_tps.exe (416,579 bytes)
- tests/__pycache__/ (1 个 .pyc)
- tests/.pytest_cache/
- compile_err.txt (0 bytes)
- compile_out.txt (0 bytes)
- .git/ (嵌套仓库)
