"""Archive reviewed build documents, then write their curated replacement."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
    'docs/engineering/build/BUILD_GRAPH.md': r'''# 构建图与生产可达性

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
''',
    'docs/engineering/build/BUILD_NODES.md': r'''# 构建节点与工具链

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
''',
    'docs/engineering/build/RELEASE.md': r'''# 版本、构建身份与发行

[最高设计](../../ACSD_DESIGN.md)规定 Windows CLI+Qt6 GUI 与 Linux CLI 的交付目标。本文定义版本、构建来源、包与留存；发布由项目负责人决定，文档完成和材料齐全不代表科学验收。

## 1. 版本来源

根 `VERSION` 是基础版本唯一来源，当前格式为 `MAJOR.MINOR.PATCH-alpha.N`。版本变更、tag 和正式发布按用户或负责人指令执行，本轮文档重构不提升版本。

`eng/tools/gen_version.py` 读取 VERSION、Git HEAD 和 dirty 生成显示串，开发形态为 `X.Y.Z-alpha.N+g<commit12>[.dirty]`。`--version --json` 提供 version、prerelease、commit、dirty、build_id、abi_version、cli_schema_version。实际 ABI/schema 值由生成器及对应协议声明核对，不能把“计划冻结为1”描述成当前所有协议都为1。

Git 不可用时生成器给 `GIT_UNAVAILABLE` 与非零状态，降级报告里的 base_version 不等于合法完整版本对象。打包器也必须核对实际二进制的版本；打包现场 HEAD 不能冒充该二进制的构建提交。

## 2. 四类身份

| 字段或对象 | 表示 | 不能单独证明 |
|---|---|---|
| `source_sha` / `configure_head_sha` | CMake configure 时 HEAD | 后续增量构建用到的全部源码，或同一二进制 |
| `build_head_sha` / `build_dirty` | 构建时提交与工作树状态 | 编译器、构建选项或产物内容 |
| `build_source_digest` | gen_build_stamp 对声明源集的路径/内容对象摘要 | 完整构建输入相同、二进制相同或科学结果等价 |
| 实际二进制/依赖文件 SHA-256 | 被计算文件的字节身份 | 来源、运行环境或科学正确性 |

`eng/tools/gen_build_stamp.py` 在构建时生成摘要，声明范围由 SOURCE_ROOTS/SOURCE_FILES/SOURCE_SUFFIXES 决定，包含产品源码与指定构建输入；它不是编译命令、编译器、外部库及环境的完整摘要。源摘要相同只能作为已声明源集比较的条件，不能替代产物哈希。构建头由小型 TU 消费，内容不变时避免不必要重编译。

provenance 保存上述来源字段，并与输入/配置、工具链和实际产物关联。缺摘要或重算不符明确报告。对同一二进制做 A/B 时固定实际文件和依赖；对不同构建做科学比较则说明类型、运算序和容差。旧文“源摘要能单独判断同一二进制”的断言保存在[来源档案](../../../实验/documentation-refactor/docs/BUILD_SOURCE.txt)。

## 3. 交付目标与现有工具

| 平台 | 产品交付目标 | 现有材料生成器 |
|---|---|---|
| Windows amd64 | Inno Setup 安装包，含 CLI、独立 Qt6 GUI、运行库、schema 与配置 | `eng/tools/make_windows_release.py` 当前生成 CLI zip；不等于完整 GUI 安装包 |
| Linux amd64 | 解包可用的 tar.gz；可附其他压缩或 AppImage | `eng/tools/make_linux_release.py` 生成 tar.zst 或 `--tar-gz`，内容需按实际安装树复核 |

现有打包脚本接收已有可执行文件并复制，不执行科学验收。其版本回落字面量、固定生成时间、响应曲线旧目录与当前安装规则存在差异；使用前核对，不能据脚本名宣布已实现完整正式发行。安装面由 `eng/cmake/install_layout.cmake` 和 `eng/packaging/install-tree.contract.json` 管理。

包内记录逐文件路径、大小、哈希、所需运行库与后端清单，包含许可证与 SBOM、VERSION 和配置/schema。包名含基础版本，包内保留构建身份；归档外提供单独哈希。根 CLI 布局和 Linux `bin/acsd` 布局是不同打包器选择，配置相对路径按实际可执行目录解释。

## 4. 验证及留存

分别核对全新安装、重定位、doctor、自有模块加载/卸载、CPU backend、依赖许可、版本及哈希，再记录三命令端到端科学与图像评价。Windows 和 Linux 的编译、运行、真实帧及 GUI 消费独立报告；不以测试条数、文档审批或资源利用率目标代替科学结论。

普通构建按可配置次数保留，发布候选、失败诊断及对应证据长期关联保存。研究证据保存在所属实验单元，发布包不混入原始 FITS、开发快照和任务控制包。审阅材料可以单独归档，但它不定义另一套正式产品树。

负责人确认发布后才执行发布动作。Agent 可整理并报告可审阅材料、已验证范围和缺口；本合同不授权自动提升版本、打tag或宣布正式发布。
''',
    'docs/engineering/standards/DEPENDENCY.md': r'''# 依赖规则

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
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/BUILD_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'Build source documents; commit={rev}\n'.encode())
        for name, body in DOCUMENTS.items():
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
