# eng

本目录是工程支撑面：机器门、合同 schema、测试、CMake 模块、构建脚本、打包配置与工程工具集中在这里。

## 职责边界

- 放：可被机器执行的工程事实源——门禁注册表与检查器（ci/）、机器校验的合同 schema（contracts/）、测试套件（tests/）、CMake 模块（cmake/）、构建脚本（build/）、打包与全局配置（packaging/）、工具与质量检查器（tools/）、本地运行工作区（run/）。
- 不放：产品科学代码（lib/）、合同条款正文（docs/engineering/，与本目录 schema 双向对应）、CI 与验收的产物和证据（artifacts/、run/）。
- CMake 入口本体（根 CMakeLists.txt、CMakePresets.json）留在仓库根，本目录只提供模块与脚本。

## 内容

- ci/ —— 机器门注册表 checks.json 与确定性执行器 run_checks.py，以及各检查器脚本。
- contracts/ —— 机器校验的合同 schema，唯一事实源。
- tests/ —— 测试套件（单元/合同/集成/科学 Oracle 等），按主题分目录，test_index.csv 为套件索引。
- cmake/ —— CMake 模块、平台工具链片段与安装布局。
- build/ —— Linux 构建入口 build.sh 与 Windows 工具链入口 toolchain.ps1。
- packaging/ —— 产品清单、程序全局配置（config/）、依赖锁、许可证与安装树校验。
- tools/ —— 工具与质量检查器（quality/、doccheck/、arch/、astrometry/、monitoring/ 等）。
- run/ —— eng 侧本地运行工作区目录骨架。

## 上游

上游：docs/ACSD_DESIGN.md §8.4（顶层结构·eng 树）；ENGINEERING_SPEC.md §7（目录规范）、§10（机器一致性检查）。