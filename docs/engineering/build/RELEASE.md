# 版本、构建身份与发行

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


## 5. Issue #75 — 二进制与科学产品来源的冻结实施合同（2026-10-17，TARGET/NOT_RUN）

已有 `eng/tools/gen_build_stamp.py`、`acsd::core::build_stamp()` 与 CLI `build_run_provenance`，不能再重造一套 source-digest 算法。**修复对象**是构建时更新依赖、产品消费者接线及真实已加载 executable/DLL SHA-256。五层身份的数学/可审计定义与正负验收见 [实施差异地图 §6](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

- Git commit 只标识版本控制快照；dirty+内容身份只标识声明源集；`ACSD_COMMIT_SHA` 是 configure 时 HEAD，不能参与“这份科学产品由此构建”的唯一指认。
- 新构建期规则每次构建低成本检查 HEAD、Git 索引成员及受控生成输入，生成文件内容相同时不更新 mtime，只重编小型 `build_stamp.cpp`；依赖集合应与脚本计算摘要的声明集合相同，新增/删除源码能触发重新配置。明确区分 `build_source_digest` 和 `build_config_digest`，不把二者任一冒充真实二进制 hash。
- 完整已发布科学产品需要 manifest 验证实际加载的 CLI/DLL 文件内容身份；若路径或装载身份变化无法复核，则字段为 `unverified` 且不可写“verified executable”。留存 hash 算法、读取错误、外部依赖与证据采样时间。
- 旧 frame-science/v1 研发产品按 #10 拒绝、原始 FITS 重算。不设计版本戳驱动的 v1 转换或旧 schema 复原；当前尚未执行增量构建矩阵与两平台 DLL 核验：**NOT_RUN**。
