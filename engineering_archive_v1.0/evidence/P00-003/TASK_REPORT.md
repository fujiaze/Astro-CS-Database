# TASK_REPORT: P00-003 恢复并固定 healpix_stack 源码

## 任务信息
- **Task ID**: P00-003
- **Phase**: P00
- **状态**: IN_REVIEW
- **执行时间**: 2026-07-24

## 来源记录
- **远端仓库**: https://github.com/fujiaze/Healpix-Mosaic-Cpp.git
- **分支**: main
- **Commit**: 027b64f51ec365a223816faf3ca9801499e2db9f
- **日期**: 2026-07-16T12:29:24+08:00
- **最后提交**: refactor(stack): link aio.dll for HEALPix I/O (spec G1 Phase 1)

## 第三方依赖
- nanoflann.hpp (BSD 许可证, header-only, Jose Luis Blanco et al.) — 位于 gradient/nanoflann.hpp

## 执行步骤
1. 记录独立仓库 commit、远端 URL、源码文件 SHA-256
2. 检查 nanoflann.hpp 许可证（BSD，可保留）
3. 删除嵌套 .git
4. 清理编译产物（healpix_stack.dll + 4 个 .exe + __pycache__ + .pytest_cache + compile_err/out.txt）
5. `git add lib/healpix_db/healpix_stack` 纳入主仓库
6. 验证 38 个文件已暂存、无编译产物、哈希 MATCH

## 纳入文件清单（38 个）
- 根目录 21 个：ahps_format.h, ahps_reader.cpp/.h, ahps_writer.cpp/.h, build.ps1, healpix_core.cpp/.h, healpix_stack.py, hp_stack_api.cpp/.h, hp_stack_hiss.cpp/.h, Makefile, memory.md, README.md, simple_test.cpp, stack_db.cpp/.h, stack_engine.cpp/.h, .gitignore
- gradient/ 14 个：corrected_stacker.cpp/.h, gradient_fitter.cpp/.h, gradient_sampler.cpp/.h, nanoflann.hpp, snr_evaluator.cpp/.h, spherical_spline.cpp/.h, test_gradient_sampler.cpp, test_snr_evaluator.cpp
- tests/ 3 个：test_gradient_synthetic.py, test_healpix_stack.py, test_hp_stack_hiss.py

## 已清理（未入库）
- healpix_stack.dll (1,471,655 bytes)
- test_corrected_stacker.exe, test_gradient_fitter.exe, test_gradient_sampler.exe, test_spherical_tps.exe
- tests/__pycache__/, tests/.pytest_cache/
- compile_err.txt, compile_out.txt (0 bytes)
- .git/ (嵌套仓库)

## 变更文件
- `lib/healpix_db/healpix_stack/**`（38 个文件新增）
- `engineering/evidence/P00-003/**`（新增）

## 允许范围遵守
- 仅修改了 `lib/healpix_db/healpix_stack/**`（删除 .git 和编译产物）、`engineering/evidence/P00-003/**`
- 未修改源码内容（哈希验证 MATCH）
- 未修改其他模块

## 下一任务
P00-004 依赖 P00-002 + P00-003（均已 DONE），P00-005 依赖 P00-001（DONE）。
按注册表顺序，P00-004 优先。
