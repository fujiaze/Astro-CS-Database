# build

本目录是平台构建脚本入口：Linux 与 Windows 两侧的构建与自检脚本放在这里。

## 职责边界

- 放：Linux 构建入口 build.sh、Windows 工具链脚本 toolchain.ps1。
- 不放：CMake 配置本体（根 CMakeLists.txt 与 CMakePresets.json 为唯一 CMake 入口）、CMake 模块（eng/cmake/）。
- 脚本只用仓内 vendored 依赖与系统工具链，不引用机器绝对路径。

## 内容

- build.sh —— Linux 侧 configure/build/test 入口：在 build/ 下配置与构建，从仓库根运行测试，日志落 run/logs/。
- toolchain.ps1 —— Windows 侧统一工具链入口，提供 check、env、build、test 子命令。

## 上游

上游：ENGINEERING_SPEC.md §7（仓库根固定条目登记 eng/build/build.sh 与 eng/build/toolchain.ps1，CMake 入口留在仓库根）。