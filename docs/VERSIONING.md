# Astro Celestial Sphere Database（ACSD） 版本合同（VER-001）

> 上游：ASTROCS_DESIGN.md §13（版本与发布权）

## 1. 唯一版本源

- 仓库根 `VERSION` 是产品版本号的**唯一来源**（`ASTROCS_DESIGN.md` §12），内容一行：`MAJOR.MINOR.PATCH-alpha.N`。
- 当前基线：以根 `VERSION`（唯一来源）为准；V6 控制包时代的冻结基线值是历史值，
  不再在本文件复制（V81-ADOPT-006 起活动文档不把旧版本写成当前值）。
- Agent 不得自行改大版本号。
- `MAJOR/MINOR/PATCH` 只能由用户/外部审核指令变更；`alpha.N` 只能在**最终外部审核通过后**提升或打 tag，Agent 无权自行发布。

## 2. 生成接口（禁止多处手填）

- `eng/tools/gen_version.py`：读 `VERSION` + git HEAD/dirty → 输出合同对象与版本串。
  - 开发构建：`X.Y.Z-alpha.N+g<commit12>.dirty`（工作树有未提交修改）。
  - 正式 alpha 包：必须来自 clean main，显示 `X.Y.Z-alpha.N+g<commit12>`。
- `--version --json` 输出至少：`version, prerelease(=alpha), commit, dirty, build_id, abi_version, cli_schema_version`，schema 见 `schemas/version.schema.json`。
- `abi_version` / `cli_schema_version` 的唯一定义点在 `eng/tools/gen_version.py`；ABI-001 / API-002 冻结时置 1。

## 2.1 构建指纹合同（RUN-PROVENANCE-01）

- **事实（一手证据 `run/RUN-PROVENANCE-01/REPORT.md` §A）**：`ASTROCS_COMMIT_SHA`
  （`version_generated.h`）由 **CMake configure 期** 的 `git rev-parse HEAD` 采样一次
  （`CMakeLists.txt`）。Ninja 的 `RERUN_CMAKE` 规则只依赖 CMake 输入，**不含任何
  `.cpp/.h`** ⇒ 改源码不会重跑 configure ⇒ 二进制里编进去的是新代码，记录里留的是旧
  SHA。故 `run_context.json` / `provenance` 的 `source_sha` **只表示 configure 时刻的
  HEAD，不是构建指纹**；两份产物的 `source_sha` 相等**不蕴含**同一二进制。
- **构建指纹**（判「同一代码 / 同一二进制」的唯一依据）由 `eng/tools/gen_build_stamp.py`
  在**构建期**采样、经 `build_stamp_generated.h` 烙进产物，随 `run_context.json` 与
  `run_manifest.provenance` 落盘：

  | 字段 | 含义 | 能否单独判「同一二进制」 |
  |---|---|---|
  | `build_source_digest` | 声明源集（`lib/**` 非测试 + `eng/cmake/**` + 根级构建输入）逐文件内容对象 id 的 sha256（64hex） | **能**（唯一依据） |
  | `build_head_sha` | 构建期 HEAD（40hex） | 否（纯文档提交也会前进） |
  | `build_dirty` | 构建期工作树是否有未提交改动（§2 的 dirty 语义） | 否 |
  | `configure_head_sha` | configure 期 HEAD（对照 `source_sha` 的来历） | 否 |

- **消费规则**：凡「同一二进制 / 同一代码」的比较、逐位 A/B 对比、跨运行归因，一律以
  `build_source_digest` 相等为前提；`source_sha` 相等**不构成**前提。
- **代价约束**：指纹只由 `git ls-files` 给出的显式清单 + 改动文件的对象 id 决定
  （干净工作树零读盘，实测 < 50ms）；生成头内容不变时不落盘，故不触发下游重编译；
  生成头只被一个 TU（`build_stamp.cpp`）消费，指纹变化的重编译面 = 1 个小 TU。
- **机器判据**：`eng/ci/check_build_provenance.py` —— 记录指纹 ≠ 当前工作树重算 ⇒
  具名判红（`SOURCE_DIGEST_MISMATCH`，有构建树逐文件清单时精确点名差异文件）；
  产物缺指纹 ⇒ `BUILD_STAMP_ANCHOR_MISSING`（rc=2，**不可锚定 ≠ 通过**）。

## 3. 同步矩阵（机器检查覆盖）

| 消费点 | 同步方式 | 检查 |
|---|---|---|
| CLI `--version`/`--version --json` | 构建期由 gen_version 注入 | schema 校验 + DOCCHK |
| alpha 包名/清单（LNX-005/WIN-009） | 打包脚本必须调用 gen_version | 打包校验器 |
| run_manifest.json | 运行期调 gen_version | CLI-003 |
| run_manifest.provenance / run_context.json 的构建指纹 | 构建期由 gen_build_stamp.py 烙入（§2.1） | CHK-BUILD-PROVENANCE |
| 文档 | 只允许出现当前基础号 | 本 checker |

## 4. 机器检查

`eng/tools/check_version_consistency.py`：
- `VERSION` 格式必须为 `X.Y.Z-alpha.N`；出现 `stable/rc/beta` 预发布标记即 FAIL。
- 扫描 `docs/ schemas/ eng/tools/ launch/ eng/tests/` 与根级 README/CHANGELOG/build.sh/toolchain.ps1：任何 `X.Y.Z` 字面量必须等于唯一源（豁免：hips_version、DatabaseVersion、schema_version、外部组件版本、`X.Y.Z`/`MAJOR.MINOR.PATCH` 占位写法）。
- **mutation 合同：任何一处伪造/漂移版本字面量必须使本 checker FAIL**（eng/tests/version 有固定试金石）。

## 5. 豁免清单（非产品版本的三元组）

`hips_version=1.4`（HiPS 格式版本）、`DatabaseVersion=1.0.0`（Gaia 库标识）、`schema_version`（schema 自身版本）、OpenCL/GPU driver 能力串（ACR dormant 域）、`X.Y.Z`、`MAJOR.MINOR.PATCH` 等占位表述。
