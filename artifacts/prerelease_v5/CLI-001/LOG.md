# CLI-001 执行日志 (2026-08-28, vm-bj + Fatduck)

## 输入
03_TASK_DETAILS CLI-001 行(建单一 target 和 Windows/Linux main;只接通 help/version stub;验收=两平台可编译+发布安装规则只选一个 exe); API-002 命令树; ARCH-003(禁全局 -march/arch:AVX)。

## 动作
1. 新建 cli/main.cpp: 严格 C++17 无平台宏分支;--help/-h(13 行=API-002 §1 命令树逐行)/--version[--json](恰一 JSON 文档)/未知命令 exit 2+help 到 stderr/已知子命令 stub(CLI-002 前接线, exit 2);exit_codes 表内联单源(内部映射, API-002 §2)。
2. 新建 cli/CMakeLists.txt: 单 target `astrocs`;版本单源 VERSION+git rev-parse --short=12+dirty 探测 → X.Y.Z-alpha.N+g<hash12>[.dirty](tools/gen_version.py 同式, configure_file 注入);编译旗标 GNU/Clang: -Wall -Wextra -fno-exceptions;MSVC: /W4 /EHs-c- /utf-8;**无任何全局 -march=/arch:AVX**(ARCH-003 baseline 规);install 规则恰一 exe → bin/。
3. 两平台编译落锤(实测):
   - Linux vm-bj: g++ 14.2.0+cmake 3.31.6, Release 编译通过, 运行: --version → 0/`astrocs 0.9.0-alpha.1+g<hash>.dirty`;--version --json → 恰一 JSON;--help → 13 行;bogus → 2;无参数 → 2。
   - Windows Fatduck(在线窗内, MSVC 14.44 BuildTools vcvars64+cl /O2 /W4 /utf-8 /std:c++17): BUILD_OK;--help → exit 0 且恰 13 行;--version → 0;--version --json → 同式 JSON;bogus → 2;phase1 run stub → 2;无参数 → 2。与 Linux 逐项一致(跨平台同码)。修复过程: 首跑缺 version_generated.h 生成步→补 Set-Content;C4819(代码页 936)→加 /utf-8。
   - Fatduck 临时文件已清理。
4. 机器门 tests/cli/test_cli_build.py 6 用例(Linux cmake+g++ 真编译): 版本串格式/JSON 恰一文档/help 与 API-002 命令树逐行断言/未知命令 exit 2/恰一 add_executable+install/禁全局 arch 旗标(跳过注释行, 避免禁令注释自命中)。

## 验证
- Linux 全量回归 unittest **82/82 OK**(新增 6)。
- 双平台编译+运行证据如上(Windows help/exit 逐项与 Linux 一致)。

## 限制与说明
- .dirty 后缀为构建时工作树未提交的预期行为(与 gen_version.py 约定一致);本提交后为干净构建。
- benchmark cpu 等子命令为 stub(API-002 §1 树已登记), 接线属 CLI-002;不另发 benchmark exe。
- Windows cmake 未安装(空缺), 用 cl 直编等价验证;CLI-002 前 PlayBook 保持脚本化复验。

## 产物
cli/main.cpp; cli/CMakeLists.txt; cli/version_generated.h.in; tests/cli/test_cli_build.py; 本日志。

## PASS 判定
单一 target/双平台可编译/发布规则只装一个 exe/help+version stub 接通且 golden 化。CLI-001 = PASS。
