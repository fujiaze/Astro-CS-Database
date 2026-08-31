# VER-001 PLAN — 版本推进为 0.10.0-alpha.2

## 需求 (04_TASK_SPECIFICATIONS.md VER-001)
- 根 `VERSION` 改为 `0.10.0-alpha.2`。
- CMake project 数字版本可为 `0.10.0`；显示版本、CLI、provider ABI manifest、profile、FITS provenance、release docs 和包名必须从 VERSION+commit 派生。
- 扫描自有生产源码中的 `0.1.0`、旧 alpha.1、手写 commit/run_id；测试 fixture 可登记豁免。

## 现状证据
- VERSION=0.10.0-alpha.1；lib/phase3_session/p3_session.cpp 硬编码 provenance `"0.1.0"`/`"phase3-run"`（F-012）。
- cli/CMakeLists.txt 与根 CMakeLists.txt 从 VERSION 派生 version_generated.h（ASTROCS_VERSION_STRING + ASTROCS_COMMIT_SHA）。

## 修改
1. `VERSION` → `0.10.0-alpha.2`
2. `lib/phase3_session/p3_session.cpp`：provenance 用 `ASTROCS_VERSION_STRING` 与 `p3-<commit>` 替代硬编码 `0.1.0`/`phase3-run`（F-012 部分闭环）
3. `CMakeLists.txt`：`astrocs_phase3_session` 增加 `${CMAKE_CURRENT_BINARY_DIR}` include（version_generated.h）
4. 文档/测试/工具中 `0.10.0-alpha.1` 字面量 → `0.10.0-alpha.2`
5. `tools/check_version_consistency.py`：修复其自身从未通过的问题（合同文档 front matter 修订号、YAML version 键、探针文件豁免；SKIP_DIRS 修正）
6. `tests/version/test_version_consistency.py`：mutation 测试改用实时 base/alpha

## 影响文件
VERSION、CMakeLists.txt、lib/phase3_session/p3_session.cpp、docs/VERSIONING.md、docs/review/*、docs/refactor/CLI_COMMAND_LAYER.md、tests/version/test_version_consistency.py、tests/unit/{p3_output_test,cpu_profile_test,p3_assembly_test}.cpp、tests/backend/{profile_gen_main.cpp,test_cpu_profile.py}、tools/{check_version_consistency.py,make_linux_release.py,make_windows_release.py,check_final_traceability.py,check_release_consistency.py,check_reproducible_build.py}

## 科学影响
无（版本字符串与 provenance 派生；不改科学算法）。

## 风险
- p3_session 编译依赖 version_generated.h；CMake include 已加，构建验证通过（CLI 0.10.0-alpha.2+g<commit>）。

## 验收命令
1. `python3 tools/check_version_consistency.py` → VERSION_CONSISTENCY_PASS base=0.10.0-alpha.2
2. `python3 -m unittest tests/version/test_version_consistency.py` → OK (5 tests)
3. fresh cmake configure 显示 `AstroCS 0.10.0-alpha.2+g<commit>`；CLI `--version` 一致
4. `grep -rn '0\.1\.0' lib/phase3_session/ cli/ include/ lib/backend_host/ lib/core/` → 无残留（生产源码）
