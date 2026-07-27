# TASK_REPORT

- Task ID: P08-001
- Commit/base: 29cb2912affda58b5387371b65cc9a636f365f58 (main)
- Objective: 生成自包含运行包、版本清单、hash、默认配置与验证命令。发布包不得依赖用户安装 Python/PowerShell, 从干净目录验证 capabilities/smoke/inspect, 生成版本与 SHA-256 清单。

## Changes

本任务为发布打包任务, 未修改任何业务源码。仅创建发布包文件和证据文档。

### 发布包 (dist/AstroCS-CLI-v1/) - 22 个文件

**二进制文件 (17 个, 不提交 git, 由 .gitignore 排除)**:
- lib/orchestrator/cpp/orchestrator.exe (4126364 bytes) - CLI 编排器主程序
- lib/astro_image_io/astro_image_io.dll (2993875 bytes) - I/O 模块
- lib/calibration/astro_calibration.dll (997878 bytes) - 校准模块
- lib/plate_solve/cpp/ipv/ipv_solver.dll (984362 bytes) - PlateSolve 模块
- lib/dynamic_psf/dynamic_psf.dll (334677 bytes) - PSF 模块
- lib/photometric_calib/cpp/photometric_calib.dll (1081805 bytes) - 测光校准
- lib/photometric_calib/cpp/gaia_client.dll (281990 bytes) - Gaia 客户端
- lib/snr_estimator/cpp/snr_estimator.dll (973907 bytes) - SNR 估算
- lib/healpix_db/healpix_drizzle/healpix_drizzle.dll (1273688 bytes) - Drizzle
- lib/healpix_db/healpix_stack/healpix_stack.dll (1471655 bytes) - 堆栈
- bin/libgcc_s_seh-1.dll (151732 bytes) - GCC 运行时
- bin/libgomp-1.dll (330358 bytes) - OpenMP 运行时
- bin/liblz4.dll (158649 bytes) - LZ4 压缩
- bin/libstdc++-6.dll (2667040 bytes) - C++ 标准库
- bin/libwinpthread-1.dll (64930 bytes) - POSIX 线程
- bin/libzstd.dll (1218273 bytes) - Zstandard 压缩
- bin/zlib1.dll (128488 bytes) - zlib 压缩

**文本文件 (5 个, 提交 git)**:
- config/default_stage1.json - Stage1 默认配置 (34 参数, 基于 config_parameter_registry.csv)
- config/default_stage2.json - Stage2 默认配置 (15 参数)
- VERSION.txt - 版本信息 (v1.1.0, git commit 29cb291, 构建环境, 组件清单)
- README.txt - 快速开始说明 (系统要求, 目录结构, CLI 命令, 退出码, 配置参数)
- verify.bat - Windows 原生验证脚本 (不依赖 Python/PowerShell, 用 certutil 算 hash)
- SHA256SUMS.txt - SHA-256 清单 (22 个文件)

### 发布包结构决策

orchestrator.exe 的 dll_loader 通过 GetModuleFileNameA 向上 4 级推导 lib_base_dir, 然后在 lib_base_dir/lib/<module>/ 下查找模块 DLL。因此发布包保持 lib/<module>/ 目录结构, orchestrator.exe 放在 lib/orchestrator/cpp/ (向上 4 级 = 发布包根目录)。MinGW 运行时 DLL 放在 bin/, verify.bat 将 bin/ 加入 PATH 供 Windows DLL 搜索。

## Files

- dist/AstroCS-CLI-v1/ (22 文件, 二进制不提交)
- engineering/evidence/P08-001/release_manifest.json
- engineering/evidence/P08-001/TASK_REPORT.md
- engineering/evidence/P08-001/TEST_REPORT.md
- engineering/evidence/P08-001/EVIDENCE_INDEX.md
- engineering/evidence/P08-001/REVIEW_REPORT.md

## Compatibility

- 发布包自包含, 不依赖用户安装 Python/PowerShell/.NET
- 不依赖 Visual C++ Runtime (orchestrator.exe 用 -static 编译)
- MinGW-w64 运行时 DLL 已包含在 bin/
- 向后兼容: 所有配置参数默认值与 config_parameter_registry.csv 一致
- DLL 接口未变更, 与 P07-002 基线完全一致

## Rollback

- 删除 dist/AstroCS-CLI-v1/ 目录即可完全回滚
- 无业务源码修改, 无需代码回滚
- 控制文件回滚: 将 MASTER_TASK_REGISTER.csv 中 P08-001 状态改回 IN_PROGRESS

## Remaining risks

1. GaiaDR3SP 数据库 (~50GB) 不包含在发布包中, 需单独获取
2. 测试数据 (testdata/, ~73GB) 不包含在发布包中
3. 南天天区 (如 C003) 内存需求 32-35 GB, 需 64 GB RAM
4. HISS 文件非字节级可重现 (zstd 压缩含时间戳), 但数据一致
5. find_mingw_bin() 在无 MinGW 环境下回退到 C:\msys64\mingw64\bin, 若不存在则 SetDllDirectory 不设置; 但 verify.bat 将 bin/ 加入 PATH, Windows 仍能找到运行时 DLL
6. 模块 DLL 版本号大多 "unknown" (capabilities 输出), 留待未来版本补充
