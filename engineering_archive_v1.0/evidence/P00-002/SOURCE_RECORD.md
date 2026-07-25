# healpix_drizzle 源码来源记录

## 独立仓库信息
- 远端: https://github.com/fujiaze/Healpix-Drizzle-Cpp.git
- 分支: main
- Commit: ecf8758affaf0caf0eb12faed7d2ed32623886e7
- 日期: 2026-07-16T12:27:28+08:00
- 最后提交: refactor(drizzle): link aio.dll instead of healpix_io.dll (spec G1 Phase 1)

## 纳入主仓库操作
- 日期: 2026-07-24
- 操作: 删除嵌套 .git，清理编译产物（*.dll/*.exe/__pycache__/.pytest_cache），git add 纳入主仓库
- 源码文件未被修改（仅移除 .git 和编译产物）

## 源码文件清单（16 个 + 2 个测试）
- drizzle_engine.cpp/.h
- hp_drizzle_api.cpp/.h
- fits_reader.cpp/.h
- poly_clip.cpp/.h
- wcs_sip.cpp/.h
- healpix_drizzle.py
- pipeline_adapter.py
- Makefile
- README.md
- memory.md
- .gitignore
- tests/test_drizzle.py
- tests/test_pipeline_adapter.py

## 已清理（不入库）
- healpix_drizzle.dll (1,273,688 bytes)
- _test_compile.exe (504,472 bytes)
- __pycache__/ (6 个 .pyc 文件)
- .pytest_cache/
- .git/ (嵌套仓库)
