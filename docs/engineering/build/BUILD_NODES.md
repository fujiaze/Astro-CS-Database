# 构建节点与工具链

上游：最高设计的双平台发行一章 [1]；构建图见 `BUILD_GRAPH.md` [2]；第三方与工具链锁定的读取面见
`../standards/DEPENDENCY.md` [3]。

本文是构建档位、节点分工、冻结工具链取值与处置纪律、可复现判据的正本。取值本身以机器源为准
（preset 合同、依赖锁文件、厂商配置），本文只写不落在机器源里的判定口径。

## 1 节点分工

| 节点 | 承担 | 不承担 |
|---|---|---|
| Windows x64 正式工具链节点 | Windows 侧编译、用例执行、真实数据复验 | 开发迭代与静态分析、真实数据终验 |
| Linux amd64 控制节点 | 开发迭代、常在线控制、静态分析、轻量编译、小合成实验、合成、真实数据终验 | Windows 侧复验与发布候选终审 |

- 平台角色（哪个平台做开发、构建、合成与真实数据终验，谁复验、谁终审）的唯一正本在工程正本的
  架构面[5]；本表只登记两个具名构建节点各自执行的构建动作，属细目层，不复述也不改写平台角色。
- 两节点使用各自的构建配置与平台相关源码（I/O、线程、路径分开写），平台无关源码尽量复用；两
  节点共用同一套 C ABI 与产品 manifest。
- 节点接入实操（凭据、远程 shell 规则、同步纪律）属运行手册面，随仓库的运维说明承载，不在本文
  冻结。

## 2 编译档位

- 优化档由 `CMAKE_BUILD_TYPE` 决定，不在构建脚本里显式声明优化旗标：优化档是编译器在给定构建
  类型下的默认行为，显式声明会与构建类型形成两个取值面。
- 轻验证用 GCC，调试与静态分析用 Clang，两侧各走本平台的 preset。
- 全仓无未锁定编译旗标，无全域告警屏蔽；告警口径见 `../standards/CODE.md` [4]。
- 并行度不写死，取自线程预算（`../architecture/DATA_FLOW.md`）。

## 3 可复现判据

- 构建标识由根 `VERSION` 派生并附提交短哈希，生成模板是
  `lib/infrastructure/cli/version_generated.h.in`（版本单源条款见 `RELEASE.md`）。
- **同一提交重构建得到相同构建标识**：这是工具链面「构建输入无隐藏状态」的判据。任何把机器
  绝对路径、时间戳或未锁定旗标引入构建输入的改动都会让它转红。
- 软件物料清单的输入清单由依赖锁复算时产出，是过程产物、不入库。
- 同一代码与同一二进制的比较以 `build_source_digest` 相等为前提；构建期 HEAD 与 configure 期
  HEAD 都不能单独充当指纹（`RELEASE.md` 的构建指纹合同）。

## 4 冻结工具链取值的处置

工具链版本号属契约冻结值，处置固定为三段，顺序不可换：

1. **显式 pin**：向构建的 configure 与 build 子进程注入版本，走构建系统自身承认的覆盖入口，
   不依赖主机默认的隐式命中；
2. **构建后实测**：从构建产物自报的编译记录读出实际使用的工具链目录，与 pin 值比对；
3. **fail-closed**：未命中 pin 值或测不到证据，即判红不放行，不按「大概率是命中的那版」处理。

禁止为让判定转绿把冻结版本放宽到实测漂移值。冻结取值的唯一数值源见
`../standards/DEPENDENCY.md` 的读取面表 [3]，本文不复述版本号。

## 5 构建入口

构建入口由 preset 合同给定，两平台各一条；生成器、二进制目录与配置类型都取自
`CMakePresets.json`，不得在命令行另指定：

```bash
# Windows x64 正式工具链
cmake --preset win-msvc-17.14.39-x64
cmake --build --preset win-rel

# Linux amd64 控制节点轻验证
cmake --preset linux-control
cmake --build --preset linux
```

preset 合同的两个面（`CMakePresets.json` 的 `vendor.acsd.org/toolchain/1` 与
`configurePresets`）：

| 面 | preset | generator | binaryDir | 配置类型 |
|---|---|---|---|---|
| Windows 正式 | `win-msvc-17.14.39-x64` | Visual Studio 17 2022（x64，toolset v143,host=x64） | `${sourceDir}/build/win-msvc-17.14.39-x64` | `RelWithDebInfo`（`buildPresets.win-rel.configuration`） |
| Linux 控制节点轻验证 | `linux-control` | Unix Makefiles | `${sourceDir}/build/linux-control` | `Release`（`cacheVariables.CMAKE_BUILD_TYPE`） |

- 两个 preset 的 `condition` 互斥（`hostSystemName` 等于 / 不等于 Windows），同一台主机上
  只有一条生效；preset 名里带工具链版本，是提示不是选择依据。
- preset 合同里**没有 Ninja 生成器**：Windows 侧的 preset 描述写明不得改生成器，Linux 侧
  的 preset 描述写明不固定 Ninja/MinGW。因此 `-G Ninja` 与 `-B build` 两条命令行写法都
  不属现行构建入口，不得据此配置或构建。
- 优化档由 preset 给出的配置类型决定；`CMAKE_BUILD_TYPE` 只在 Linux 控制节点 preset 上
  有意义，Visual Studio 生成器忽略它。

产物与源集的对应关系由构建图给出 [2]；受影响的构建目标由构建图反查得出，不靠猜路径。
构建产物的留存与哈希见 `RELEASE.md`。

## 6 引用

- 架构与依赖方向见 `../architecture/ARCHITECTURE.md` 与 `../standards/DEPENDENCY.md` [3]。
- 构建图见 `BUILD_GRAPH.md` [2]；验证执行范围与分档见 `../testing/TEST.md`。
- 工具链冻结取值的机器校验实现见 `eng/cmake/toolchain/verify_toolchain.py`，
  其 schema 源见 `eng/packaging/schemas/preset-contract.json`。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md`，最高设计的双平台发行一章。

[2] 内部文档 `docs/engineering/build/BUILD_GRAPH.md`，生产构建图。

[3] 内部文档 `docs/engineering/standards/DEPENDENCY.md`，依赖规则与第三方锁定。

[4] 内部文档 `docs/engineering/standards/CODE.md`，代码标准的编译器告警口径。

[5] 内部文档 `../architecture/ARCHITECTURE.md`，平台与交付形态，平台角色（开发、构建、合成、真实数据终验、复验与终审的分派）的唯一正本。