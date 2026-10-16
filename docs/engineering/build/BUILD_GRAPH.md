# 构建图与生产可达性

本页说明从构建入口定位产品、模块和工具的方法。目标与平台边界由[最高设计](../../ACSD_DESIGN.md)定义；构建命令见[工具链](BUILD_NODES.md)，依赖方向见[依赖标准](../standards/DEPENDENCY.md)。

## 1. 构建入口及层次

根 `CMakeLists.txt` 声明生产 CLI `acsd`，递归纳入相应库；GUI 和 Qt 浏览组件使用独立构建。`normalize/mosaic/export` 是同一 CLI 的独立命令，各自进入 Runtime 节点图。可执行入口、链接目标、运行期动态模块和独立工具具有不同身份。

| 层次 | 当前声明中的代表目标 | 责任 |
|---|---|---|
| CLI 与运行适配 | `acsd`、`acsd_cli_runtime`、`acsd_module_adapters` | 参数、IR、Runtime 调用、节点分派 |
| 核心与 I/O | `acsd_core`、`acsd_common`、`acsd_aio`、`acsd_hips` | 对象、共享算法、统一读写与产品 |
| Normalize 科学库 | `acsd_calibration`、`acsd_p1_ipv`、`acsd_p1_sdet`、`acsd_p1_dpsf`、`acsd_phase1_photcal`、`acsd_phase1_noise`、`acsd_drizzle` | 校准、天测、科学源/PSF、测光、噪声、球面映射 |
| Mosaic 与 Export | `acsd_phase2`、`acsd_phase2_integrate`、`acsd_p3_projection_wcs`、`acsd_p3_rsmp`、`acsd_p3_fits_output` | 球面合成与平面输出；精确目标名以对应 CMake 声明为准 |
| 动态安装面 | `acsd_runtime`、`acsd_io`、`modules/*`、`providers/*` | 按安装与 loader 合同提供运行期组件 |
| 开发诊断 | `acsd_probes`、`acsd_dev_diagnostics` | `ACSD_DEV_MODE` 决定实现或仅头接口 |

该表是职责地图，不声称穷举某一次配置的传递闭包，也不以目标名存在证明代码实际执行。完整源集、条件和链接从根及子目录 CMake 查出，核对具体配置时同时检查生成构建树。

## 2. 可达性核对

从 `add_executable(acsd)` 沿 `target_link_libraries` 递归定位静态闭包；再检查 `add_dependencies`、OBJECT 源集、生成头和运行期 loader 清单。安装目录中存在 DLL 不表示 CLI 已加载它，静态链接库存在也不表示某函数被调用。反过来，运行期动态模块可以不在静态链接闭包中。

`eng/cmake/install_layout.cmake` 是安装规则入口，`eng/packaging/install-tree.contract.json` 描述预期树。`lib/infrastructure/cli/CMakeLists.txt` 提供 CLI 支撑目标；projection、resample、fits_output 和 integration 的子目录声明科学库。IPV 当前在根静态库目标中，不能继续描述成只存在独立测试工具。

共享 HEALPix、密码摘要和重复科学算子通过公开库复用；生产不引入 GPU/ACR 路由。独立工具与 dormant 源码保留各自用途，新增生产依赖以实际架构需求和合同核对，不将旧工具分类变成永久禁止依赖的审批清单。

## 3. 静态图工具的限制

`eng/tools/arch/cmake_graph.py` 提供文本解析、入口、闭包和源集路径摘要；它不是完整 CMake 解释器，尤其不能只凭文本扫描确定条件分支、生成器表达式、动态加载与实际链接命令。`source_fingerprint` 摘要的是源路径列表，不是文件内容或二进制。

`eng/tools/arch/gen_build_graph_doc.py` 可尝试生成三个机器块，但其 NONPROD 列表仍把未纳入根图的 coverage 工具登记为根图目标，可能在导出前拒绝。当前 DOC_REL 已是本页实际路径；旧正文“常量仍指向 docs/engineering/BUILD_GRAPH.md”的记录已经过时。旧表及失败叙述保存在[来源档案](../../../实验/documentation-refactor/docs/BUILD_SOURCE.txt)，不继续作为当前闭包表发布。

工具默认会写回正式页，正文重构后不直接运行它覆盖本页。需要导出时，先确认模板标记与分类，再将结果保存在所属实验；生成失败保留命令和 stderr。本轮未配置或编译产品，不将静态图核对标为双平台构建通过。

## 4. 编译定义与验证

`ACSD_DEV_MODE` 的派生宏、OFF 实现剔除及同值传递见[开发模式](../architecture/DEV_MODE.md)。OpenMP 启用条件、链接和线程预算需同时核对，不能只看到 `find_package` 就宣称内核已并行。

实际构建记录保存 preset、CMake cache、编译器/SDK/CRT、编译链接命令、目标集和产物哈希。分别报告源码可达、编译、链接、加载和真实运行；仅生成图不能证明后面四项。源文件删除或移动后重配已有构建树，防止 configure 时生成的依赖清单继续引用旧路径。
