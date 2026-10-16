# 依赖规则

本标准规定组件依赖方向、外部库来源与工具链配置的维护位置。项目目标见[最高设计](../../ACSD_DESIGN.md)，实际构建定位见[构建图](../build/BUILD_GRAPH.md)。

## 1. 组件方向

科学与适配模块通过公开 API 依赖共享算法和统一 AIO；AIO 不依赖上层科学编排。共享 HEALPix、密码摘要和常用数值算法保留单一实现，调用者不复制内核。依赖图避免循环，模块经声明端口交换对象，禁止借私有文件或全局配置形成隐式依赖。

动态模块经安全 loader 加载，使用授权路径、manifest、哈希、ABI 和 CPU/OS 可执行能力核对，失败明确返回。是否存在可用数学等价 baseline，按[CPU 后端](../resources/cpu/BACKEND.md)与[安全加载](../api/abi/SECURE_LOADER.md)规定，不能把“禁止搜索其他路径”写成“任何可选后端失败都不能选合法 baseline”。

生产科学计算为 CPU，ACR/GPU 保留源码或实验也不自动进入生产加载、benchmark 或发行。归档代码按其现有归属保存，不重建它来冒充现行产品。Python 可用于构建、打包、诊断、测试和研究工具；这些用途不表示科学内核改成 Python，也不需要人为标签才能运行文档脚本。

## 2. 配置与锁定来源

| 对象 | 维护入口 | 核对面 |
|---|---|---|
| Windows preset 与工具链策略 | `CMakePresets.json`、`eng/packaging/schemas/preset-contract.json` | `eng/cmake/toolchain/verify_toolchain.py` 与实际 CMake cache/编译记录 |
| VS 安装组件 | `eng/packaging/windows/.vsconfig` | 实际安装与选中工具链 |
| 第三方、系统库和许可证来源 | `eng/packaging/dependency-lock.json` | 实际源码/库哈希、构建链接与 SBOM |
| 安装文件 | `eng/cmake/install_layout.cmake`、install-tree.contract.json | 全新安装树及产物依赖 |

文档解释策略，机器配置给出具体取值，实际工具链记录证明使用事实。三者不一致时保留差异并订正责任面；机器文件不能反向改写产品目标或物理定义。vendor 注记和 README 是镜像，不另设一份版本表。

## 3. 路径、CRT 与可选能力

构建输入使用可重定位源码/依赖路径，避免个人用户目录和机器盘符。安装路径由显式参数和工具链发现确定；当前 VS preset 不固定某个个人安装目录。MinGW 可作本地兼容验证，正式 MSVC 依赖来源和验证单独报告。

MSVC 动态 CRT 必须在跨边界模块与第三方间兼容；Debug 与非 Debug 的取值按 preset。显式 cache 变量选择 zlib 等外部能力，缺压缩后端只在合同允许的范围报告能力，不将不可读压缩输入静默改成成功。

引入外部依赖同时记录版本、内容哈希、许可证、平台、构建方式、公开 ABI 和实际消费者。依赖锁及 SBOM 不能只覆盖包中独立 DLL 而遗漏静态嵌入库。系统库与工具链也属于复现条件，其证据随构建保存。
