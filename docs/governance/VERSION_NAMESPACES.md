# AstroCS 版本命名空间与产品版本事实源（GOV-003）

> 本文档是 AstroCS 版本治理的机器可校验权威（authoring_task: GOV-003，
> owner: SA-GOV-01）。它把"版本"拆成互不干扰的命名空间，并规定产品版本
> 的唯一来源与生成链。任何把 ABI/模块/数据 schema/文档修订号误当作产品
> 版本、或在非定义点手抄产品版本的行为，都视为违反本合同的漂移。

状态: ACTIVE_NORMATIVE  版本: 1

## 1. 五个版本命名空间（总表）

AstroCS 的"版本号"不是一个量，而是五个生命周期独立的命名空间。每个
命名空间有自己的定义点（canonical definition point）与消费方式，禁止
跨命名空间借用号段。

| 命名空间 | 权威定义点 | 当前值 | 递增规则/生命周期 |
|---|---|---|---|
| product | 仓库根 `VERSION`（唯一事实源，单行 `MAJOR.MINOR.PATCH-alpha.N`） | `0.11.0-alpha.1` | 产品发布语义；`MAJOR.MINOR.PATCH` 只由负责人指令变更，`alpha.N` 只在外部审核通过后提升；禁止 stable/rc/beta |
| module | 各模块 `module.yaml`（module_version 字段；DATA-001 建立类型化产物合同时登记于 module manifest） | 逐模块独立（当前以 manifest 为准） | 模块接口/产物变更时由模块 owner 递增，与产品版本无关 |
| ABI | `include/astrocs/common_abi_v1.h` 的 `ACS_ABI_VERSION_V1`（C ABI 冻结）与 `cli/version_generated.h` 暴露的 `abi_version` | ABI v1（头常量 `1u`；gen_version 输出 `abi_version=0` 表示 CLI 侧尚未对 ABI-001 冻结置 1） | ABI-001 冻结后 CLI 报告从 0 → 1；任何破坏二进制兼容的变更必须递增 |
| data-schema | `contracts/data/artifact_types.registry.json` 的 `schema_version`（type_id 数据产品 schema）与各 schema 文件 `$schema` 版本 | type_id schema_version = 1（DATA-001 冻结） | 数据产品结构变更时按 registry 递增 |
| doc-revision | 每份治理/规范文档 front matter 的 `状态: … 版本: N`（文档自身修订号） | 逐文档独立（本文档 = 1） | 文档内容修订时递增；与产品版本、数据 schema 无换算关系 |

## 2. 历史轮次命名空间（不参与以上五类）

历史工程轮次（V6.1、V19R8、控制包 ASTROCS-ALPHA3-MODULAR-REFOUNDATION-V7
等）不是"当前版本"。它们只允许出现在 `CHANGELOG.md`（历史节）与
`docs/archive/**`、`engineering/control/archive/**`（ARCHIVED_NON_NORMATIVE
文档）中，禁止把旧轮次数字冒充当前产品状态（约束文件 §F.8、审计 P1 §4.11）。
因此治理上把 history 视作第五个受管命名空间（"历史轮次"），规则是：
**只进 archive/CHANGELOG，不进 active 文档。**

| 命名空间 | 允许出现的位置 | 禁止出现的位置 |
|---|---|---|
| history（历史工程轮次） | `CHANGELOG.md` 历史节、`docs/archive/**`、`engineering/control/archive/**`、`REVIEW.md` 供追溯的历史引用 | active 文档正文、`docs/governance`、`docs/architecture` 现状描述、README 当前状态节、CLI/打包产物 |

> 注：任务书列举 "product/module/ABI/data-schema/doc-revision/history"
> 五个命名空间。其中前四个是活动版本空间，history 是"只归档不激活"的
> 受管空间，规则上不携带可递增的"当前值"。

## 3. 产品版本唯一事实源（禁止手抄）

- 根 `VERSION` 是产品版本的唯一权威：内容一行 `MAJOR.MINOR.PATCH-alpha.N`。
- 机器检查豁免表内的数字三元组不是产品版本：FITS 4.0（格式规范）、
  HiPS 1.0/1.4（IVOA 格式版本）、DatabaseVersion（Gaia 库标识）、
  schema_version / ABI v1（见 §1 各命名空间定义点）、外部组件版本
  （CFITSIO 4.6.4、gcc/cmake 等）、`X.Y.Z` / `MAJOR.MINOR.PATCH` 占位表述。
- 禁止把上述非产品版本当作产品版本扫描命中，也禁止反方向：产品版本
  只能来自根 `VERSION`，不得在 CMake/CLI/打包脚本内手抄字面量。

## 4. 生成链（CMake/CLI/L0 从根 VERSION 生成，禁止手抄）

| 消费点 | 生成方式 | 状态 |
|---|---|---|
| CMake 主版本串 | 根 `CMakeLists.txt` `file(READ …/VERSION)` + `git rev-parse HEAD` → `ASTROCS_VERSION_STRING`（`X.Y.Z-alpha.N+g<sha>`）→ `configure_file` 生成 `cli/version_generated.h` | 已接线（VER-001 遗留）；`project(astrocs VERSION …)` 字面量字段需 BLD-002 改读生成值（见 known_limits） |
| CLI `astrocs --version[ --json]` | 编译期注入 `ASTROCS_VERSION_STRING`；`--json` 输出 `{"schema_version":"1","name":"astrocs","version":"<生成串>"}` | 已接线；验收需重建 CLI 验证 |
| CLI JSON 报告 `astrocs_version` 字段 | `ASTROCS_VERSION_STRING`（doctor/hardware/verify 等共用） | 已接线 |
| L0 文档（REVIEW.md 等） | 只引用当前产品基础号 `0.11.0-alpha.1`（由本任务与 GOV-005 收敛） | 本任务更新允许路径内引用 |
| 打包脚本 | `tools/gen_version.py` 读 VERSION + git HEAD → 版本串与 build_id | 已接线（`make_linux_release.py`/`make_windows_release.py` 调用）；硬编码回退串属他人路径清理项 |
| `tools/gen_version.py --json` | 输出 version/prerelease/commit/dirty/build_id/abi_version/cli_schema_version 合同对象（schemas/version.schema.json） | 已接线 |

校验规则（机器检查）：
1. 根 `VERSION` 必须匹配 `^(\d+)\.(\d+)\.(\d+)-alpha\.(\d+)$`，禁 stable/rc/beta；
2. 扫描 active 文档/配置，任何其他产品版本（`X.Y.Z-alpha.N` 或与源
   基础号不同的裸 `X.Y.Z`）必须 FAIL —— 但 FITS 4.0、HiPS 1.0/1.4、
   ABI v1、schema_version、DatabaseVersion 等豁免项不得被误报；
3. 产品版本不得出现在 active 文档中与源不一致的形态（同一文档允许
   出现当前源值本身）。

实现：`tools/doccheck/check_version_namespaces.py`（见 §6）；历史旧检查器
`tools/check_version_consistency.py` 为 VER-001 遗留，其扫描口径与本命名
空间文档的差异（含对 docs/VERSIONING.md 等他人路径的硬编码）在
known_limits 登记，待 GOV-005/前台统一收敛。

## 5. 当前产品版本值

- 根 `VERSION`：`0.11.0-alpha.1`
- 生成串形态（clean main）：`0.11.0-alpha.1+g<commit12>`
- 生成串形态（dirty 工作树）：`0.11.0-alpha.1+g<commit12>.dirty`
- CLI `astrocs --version --json` 预期（schema_version="1" 顶层对象）：
  `{"schema_version":"1","name":"astrocs","version":"0.11.0-alpha.1+g<commit12>"}`
- `tools/gen_version.py --json` 预期（schemas/version.schema.json）：
  `{"version":"0.11.0-alpha.1+g<commit12>[.dirty]","prerelease":"alpha",
  "commit":"<sha>","dirty":<bool>,"build_id":"g<commit12>[.dirty]",
  "abi_version":"0","cli_schema_version":"0"}`

## 6. 机器检查入口

- 本任务校验器：`tools/doccheck/check_version_namespaces.py`
  （exit 0 = PASS；对 FITS 4.0 / HiPS 1.0 / ABI v1 等豁免项做反误报断言）。
- 既有版本扫描：`tools/check_version_consistency.py`（VER-001 遗留，
  0.11.0 更新需前台协调他人路径硬编码后统一 PASS）。
- 文档索引/归档边界：`tools/doccheck/check_doc_index.py`（GOV-002）。

---
authoring_task: GOV-003
authoring_owner: SA-GOV-01
base_main_sha: b7b2dea70dbcdacdcf6eb762609a908abdeab697
product_version: 0.11.0-alpha.1
