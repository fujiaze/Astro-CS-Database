# 构建节点与工具链

产品目标为 Windows amd64 和 Linux amd64，Windows GUI 独立构建。本文说明已声明的构建入口与取证要求；[构建图](BUILD_GRAPH.md)定位目标，[依赖标准](../standards/DEPENDENCY.md)定位锁文件，[发行合同](RELEASE.md)说明交付。

## 1. 平台与构建配置

Windows 使用 Visual Studio 17 2022、x64、v143 host=x64 和动态 CRT；Linux 当前轻验证 preset 使用 Unix Makefiles 与 Release。具体 SDK、CMake 版本及策略从 `CMakePresets.json` 与 `eng/packaging/schemas/preset-contract.json` 对应读取，文档不复制第二套版本值。

当前 Windows toolset 没有 `version=` build 号 pin，VS 实例由 CMake 发现；preset 名含旧版本提示，不能据名字声称已选择那个编译器 build。`displayName`、vendor 注记和工具链校验器若有分歧，记录实际选择与合同差异，不能在正文宣布所有版本已锁死。

两平台可承担各自可执行的开发、编译和验证工作；某个控制节点历史分工不禁止用户在 Windows 整理源码或文档。实际远程接入与凭据按运行环境管理，不在设计中写死个人主机路径。

## 2. 现有 CLI 构建命令

```powershell
cmake --preset win-msvc-17.14.39-x64
cmake --build --preset win-rel
ctest --preset win-rel
```

```sh
cmake --preset linux-control
cmake --build --preset linux
ctest --preset linux
```

这些命令来自根 preset，不代表本轮执行过。Windows 为多配置 `RelWithDebInfo`，Linux 单配置 `CMAKE_BUILD_TYPE=Release`；两者不能用同一 CMAKE_BUILD_TYPE 解释。生成器、binaryDir 和配置来自 preset，另建构建树时说明用途和配置差异。并行度根据实际预算提供，避免固定机器核数。

Debug/Release/RelWithDebInfo 决定优化与符号；开发诊断由独立的[ACSD_DEV_MODE](../architecture/DEV_MODE.md)决定。正式性能比较固定优化选项、ISA 与 worker，记录实际编译命令，不把诊断开销归给算法。

## 3. 工具链与依赖核对

依次确认声明的 preset/锁文件、构建进程实际选择、CMake cache、编译器/SDK/CRT 和产物依赖。未取得证据就报告未验证，不按目录名或预期默认值推断工具链。所有跨库对象使用兼容 CRT 与 ABI，MinGW 兼容性构建不能冒称 MSVC 正式验证。

工具链校验入口为 `eng/cmake/toolchain/verify_toolchain.py`；其存在不证明每次构建已自动调用。第三方与系统库来源、可选压缩能力和实际缺项随构建记录，不为通过检查而改锁值。

## 4. 重建身份与证据

VERSION、提交与构建源摘要见[发行合同](RELEASE.md)。同一提交可在不同工具链、配置和依赖下产生不同二进制；相同构建标识不证明字节重现。比较声明源集内容用 build_source_digest，比较交付二进制用实际文件 SHA-256，再记录工具链、选项和环境。

重建验证分别检查版本一致、源内容、ABI、功能结果、可重复二进制和实际科学等价。双平台编译、加载、合成及真实 FITS 运行分别报告。安装规则或文档检查通过不能代替实际构建；发布仍由项目负责人决定。
