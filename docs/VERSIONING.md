# AstroCS 版本合同（VER-001）

## 1. 唯一版本源

- 仓库根 `VERSION` 是产品版本号的**唯一权威来源**，内容一行：`MAJOR.MINOR.PATCH-alpha.N`。
- 当前冻结基线：`0.10.0-alpha.1`（V6 控制包目标版本；Agent 不得自行改大版本号）。
- `MAJOR/MINOR/PATCH` 只能由用户/外部审核指令变更；`alpha.N` 只能在**最终外部审核通过后**提升或打 tag，Agent 无权自行发布。

## 2. 生成接口（禁止多处手填）

- `tools/gen_version.py`：读 `VERSION` + git HEAD/dirty → 输出合同对象与版本串。
  - 开发构建：`X.Y.Z-alpha.N+g<commit12>.dirty`（工作树有未提交修改）。
  - 正式 alpha 包：必须来自 clean main，显示 `X.Y.Z-alpha.N+g<commit12>`。
- `--version --json` 输出至少：`version, prerelease(=alpha), commit, dirty, build_id, abi_version, cli_schema_version`，schema 见 `schemas/version.schema.json`。
- `abi_version` / `cli_schema_version` 的唯一定义点在 `tools/gen_version.py`；ABI-001 / API-002 冻结时置 1。

## 3. 同步矩阵（机器检查覆盖）

| 消费点 | 同步方式 | 检查 |
|---|---|---|
| CLI `--version`/`--version --json` | 构建期由 gen_version 注入 | schema 校验 + DOCCHK |
| alpha 包名/清单（LNX-005/WIN-009） | 打包脚本必须调用 gen_version | 打包校验器 |
| run_manifest.json | 运行期调 gen_version | CLI-003 |
| 文档 | 只允许出现当前基础号 | 本 checker |

## 4. 机器检查

`tools/check_version_consistency.py`：
- `VERSION` 格式必须为 `X.Y.Z-alpha.N`；出现 `stable/rc/beta` 预发布标记即 FAIL。
- 扫描 `docs/ schemas/ tools/ launch/ tests/` 与根级 README/CHANGELOG/build.sh/toolchain.ps1：任何 `X.Y.Z` 字面量必须等于唯一源（豁免：hips_version、DatabaseVersion、schema_version、外部组件版本、`X.Y.Z`/`MAJOR.MINOR.PATCH` 占位写法）。
- **mutation 合同：任何一处伪造/漂移版本字面量必须使本 checker FAIL**（tests/version 有固定试金石）。

## 5. 豁免清单（非产品版本的三元组）

`hips_version=1.4`（HiPS 格式版本）、`DatabaseVersion=1.0.0`（Gaia 库标识）、`schema_version`（schema 自身版本）、OpenCL/GPU driver 能力串（ACR dormant 域）、`X.Y.Z`、`MAJOR.MINOR.PATCH` 等占位表述。
