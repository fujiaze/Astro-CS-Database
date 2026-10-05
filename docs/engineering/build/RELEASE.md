# 发布与版本合同

上游：最高设计的版本与发布权一章 [1]。

版本唯一源、生成接口、构建指纹合同、版本一致性判据、交付产物与留存、发布候选门槛。

- 版本记录：**版本唯一源 = 根 `VERSION`**（见「唯一版本源」）；
  发布状态记录 = 本文件；版本变更历史由 **`VERSION` + git 历史** 承载。
  根目录条目须先登记并经负责人确认（`AGENTS.md` / `../../ACSD_DESIGN.md` 的 I/O 与原子产品一章 [1]）。
- 交付包：ACSD_Review_<主题>_<YYYYMMDD>.zip，SHA256SUMS.txt。
- 包内容：README、reports/、evidence/、self_review/、
 source/full_first_party_after.zip + manifest、docs_snapshot/。
- 包内容面：source archive 仅 first-party；build/vendor/data 保留在包外。
- Gate 字面量如实：PRE_RELEASE_ENGINEERING_FOUNDATION=PASS 仅当
 AUTHORITATIVE_DOC_CHAIN/SCIENCE/ALGORITHM/ARCHITECTURE/
 IMPLEMENTATION_STANDARDS 全 PASS；FINAL_REAL_DATA_VALIDATION=PENDING
 如实标注，完成状态只由验收签发。
- 保留清单：ACR 控制包/审核包、外部数据目录。

## 唯一版本源

- 仓库根 `VERSION` 是产品版本号的**唯一来源**（`../../ACSD_DESIGN.md` 的版本与发布权一章 [1]），内容一行：`MAJOR.MINOR.PATCH-alpha.N`；
 本文件不复制版本数值（取值现场读根 `VERSION`）。
- 大版本号的变更权 = 用户/外部审核指令。
- `MAJOR/MINOR/PATCH` 只能由用户/外部审核指令变更；`alpha.N` 只能在**最终外部审核通过后**提升或打 tag，Agent 无权自行发布。

## 生成接口（单一来源生成）

- `eng/tools/gen_version.py`：读 `VERSION` + git HEAD/dirty → 输出合同对象与版本串。
 - 开发构建：`X.Y.Z-alpha.N+g<commit12>.dirty`（工作树有未提交修改）。
 - 正式 alpha 包：必须来自 clean main，显示 `X.Y.Z-alpha.N+g<commit12>`。
  - 注入时机与构建入口（preset 合同）见 `BUILD_NODES.md` [2]。
- `--version --json` 输出至少：`version, prerelease(=alpha), commit, dirty, build_id, abi_version, cli_schema_version`，schema 见 `schemas/version.schema.json`。
- `abi_version` / `cli_schema_version` 的唯一定义点在 `eng/tools/gen_version.py`；C ABI 版本字段与 CLI schema 版本字段冻结时置 1。

## 构建指纹合同

- **configure 期采样**：`ACSD_COMMIT_SHA`（`version_generated.h`）由 **CMake configure 期** 的
 `git rev-parse HEAD` 采样一次（`CMakeLists.txt`）。Ninja 的 `RERUN_CMAKE` 规则只依赖 CMake 输入，
 **不含任何 `.cpp/.h`** ⇒ 改源码不重跑 configure ⇒ 二进制里编进去的是新代码，记录里留的是 configure
 时刻的 SHA。故 `run_context.json` / `provenance` 的 `source_sha` **只表示 configure 时刻的 HEAD，
 不是构建指纹**；两份产物的 `source_sha` 相等**不蕴含**同一二进制。验证面 = 本节构建溯源判据。
- **构建指纹**（判「同一代码 / 同一二进制」的唯一依据）由 `eng/tools/gen_build_stamp.py`
 在**构建期**采样、经 `build_stamp_generated.h` 烙进产物，随 `run_context.json` 与
 `run_manifest.provenance` 落盘：

 | 字段 | 含义 | 能否单独判「同一二进制」 |
 |---|---|---|
 | `build_source_digest` | 声明源集（`lib/**` 非测试 + `eng/cmake/**` + 根级构建输入）逐文件内容对象 id 的 sha256（64hex） | **能**（唯一依据） |
 | `build_head_sha` | 构建期 HEAD（40hex） | 否（纯文档提交也会前进） |
| `build_dirty` | 构建期工作树是否有未提交改动 | 否 |
 | `configure_head_sha` | configure 期 HEAD（对照 `source_sha` 的来历） | 否 |

- **消费规则**：凡「同一二进制 / 同一代码」的比较、逐位 A/B 对比、跨运行归因，一律以
 `build_source_digest` 相等为前提；`source_sha` 相等**不构成**前提。
- **代价约束**：指纹只由 `git ls-files` 给出的显式清单 + 改动文件的对象 id 决定
 （干净工作树零读盘，判据 = 指纹计算只用显式清单；耗时读数落 `实验/engineering-evidence/`）；生成头内容不变时不落盘，故不触发下游重编译；
 生成头只被一个 TU（`build_stamp.cpp`）消费，指纹变化的重编译面 = 1 个小 TU。
- **构建溯源判据** —— 记录指纹 ≠ 当前工作树重算 ⇒
 具名判红（`SOURCE_DIGEST_MISMATCH`，有构建树逐文件清单时精确点名差异文件）；
 产物缺指纹 ⇒ `BUILD_STAMP_ANCHOR_MISSING`，**不可锚定 ≠ 通过**。

## 同步矩阵（判据覆盖点）

| 消费点 | 同步方式 | 判据 |
|---|---|---|
| CLI `--version`/`--version --json` | 构建期由 gen_version 注入 | 版本一致性判据 |
| alpha 包名/清单 | 打包脚本必须调用 gen_version | 版本一致性判据 |
| run_manifest.json | 运行期调 gen_version | 版本一致性判据；manifest 字段口径见「产物清单」 |
| run_manifest.provenance / run_context.json 的构建指纹 | 构建期由 gen_build_stamp.py 烙入 | 构建溯源判据 |
| 文档 | 只允许出现当前基础号 | 版本一致性判据 |

## 版本一致性判据

判据由人读对抗性审核逐条裁决、实验复核取证；判据文本不含执行器。
- `VERSION` 格式必须为 `X.Y.Z-alpha.N`；出现 `stable/rc/beta` 预发布标记即判红。
- 扫描 `docs/ schemas/ eng/tools/ launch/ eng/tests/` 与根级 README/CHANGELOG/build.sh/toolchain.ps1：任何 `X.Y.Z` 字面量必须等于唯一源（豁免：hips_version、DatabaseVersion、schema_version、外部组件版本、`X.Y.Z`/`MAJOR.MINOR.PATCH` 占位写法）。
- **mutation 合同：任何一处伪造/漂移版本字面量必须使本判据判红**。该合同的固定试金石用例原在 `eng/tests/version/**`，该目录在现行文件树中不存在 ⇒ 本判据现无可复跑的 mutation 用例，注入前须先补用例。

## 豁免清单（非产品版本的三元组）

`hips_version=1.4`（HiPS 格式版本）、`DatabaseVersion=1.0.0`（Gaia 库标识）、`schema_version`（schema 自身版本）、OpenCL/GPU driver 能力串（ACR dormant 域）、`X.Y.Z`、`MAJOR.MINOR.PATCH` 等占位表述。

## 产物清单

每次构建留存：

| 产物 | 打包器 | 内容 | 用途 |
|---|---|---|---|
| `ACSD-Linux-amd64-<VERSION>.tar.zst` | `eng/tools/make_linux_release.py` | 解包根目录 `acsd/`：单用户可执行文件、`MANIFEST.json`、`backends.manifest.json`、`SBOM.spdx.json`、`LICENSES/`、`VERSION`、`SHA256SUMS` | 复验/发布候选 |
| `ACSD-Linux-amd64-<VERSION>.tar.gz` | 同上；`--tar-gz` 或运行环境无 zstd 时 | 同上 | 复验/发布候选 |
| `ACSD-Windows-amd64-<VERSION>.zip` | `eng/tools/make_windows_release.py` | 解包根目录 `acsd/`：`acsd.exe`、私有运行时 DLL、`MANIFEST.json`、`backends.manifest.json`、`SBOM.spdx.json`、`LICENSES/NOTICE.txt`、`VERSION`、`SHA256SUMS` | 复验/发布候选 |
| `<归档文件>.sha256` | 两个打包器各自写出 | 归档外层的单独哈希 | 传输校验 |
| `logs/` | 构建与复验过程侧 | 构建与复验过程日志 | 失败诊断 |

`<VERSION>` 取根 `VERSION` 的整行内容，不作形态裁剪；两个打包器在读不到 `VERSION` 文件时各自回落到
内建的字面量默认值，因此包名的版本位是否与根 `VERSION` 一致须由版本一致性判据核对。commit 信息由包内的
`VERSION` 行承载，不进文件名。

验收证据不进产物树：判定的读数由实验与代码产生，按各自实验单元的证据形态保存
（见 `../governance/TRACEABILITY.md` [3] 的证据层与各 `EVID-*` 登记项）。

## 命名与哈希

- 产物名含版本位，取根 `VERSION` 的整行内容；commit SHA 不进文件名；
- 每个归档内附 `SHA256SUMS`（覆盖除自身外的全文件），归档外另有 `<归档文件>.sha256`；
- 发布候选与普通构建分栏存放；落点登记面 = `../../ACSD_DESIGN.md` 的 I/O 与原子产品一章 [1]。

## 留存策略

- 普通构建：保留最近 N 次（可配）；
- 发布候选：长期保留 + 关联验收记录；
- 真实数据/复验证据：长期保留，关联到发布记录。

## 产物验证

- 安装目录可自检：`acsd doctor` rc=0；
- 模块可独立加载、验证、卸载；
- 打包白名单/哈希/版本/provenance 校验通过。

这几项是**人工可核的产品行为**，不是自动判决：核验者执行后记录读数，
读数即依据，不存在产出判红判绿的门。

## 发布候选门槛

发布候选至少满足：合同冻结无冲突、追踪无断链、模块可独立加载/验证/卸载、ACR 生产不可达、heavy 无硬编码线程/单线程长计算/持续低利用率/无界内存增长、Linux 真实数据终验 + Windows 复验、图像有量化证据+Agent 初审+Owner 终审、P0/P1=0、发布包白名单/哈希/版本/provenance 通过。

双平台**编译**由前台在本地验证能编过，「零警告」是前台自验标准并记入交付件；
它不转化为任何自动门，也不得以「某道门通过」作为通过依据。

**产物只是材料；最终发布决定只属项目负责人。**

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md`，最高设计的版本与发布权一章、I/O 与原子产品一章。

[2] 内部文档 `docs/engineering/build/BUILD_NODES.md`，构建节点、工具链与构建入口。

[3] 内部文档 `docs/engineering/governance/TRACEABILITY.md`，追溯规范与证据层登记。
