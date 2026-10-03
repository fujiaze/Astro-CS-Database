# 审稿-P1-ENG-packaging-001（G08-05 对抗审稿 第 1 遍）

- **片号**：`ENG-packaging-001`
- **层**：`eng/packaging`
- **基线**：仓库 `/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- **口径**：本片以 `片清单-权威版.yaml:1083-1112` 的 22 份成员清单为准
- **判据口径声明**：本片成员**不含任何判据/门脚本**（`eng/packaging/` 下零 `.py`）。因此涉及判据数量时一律按「**被引用但已删除的机器入口 = 4 个**」与「**eng/ci 门注册表 = 1 个（整目录已删）**」计，**不存在**「门实例 / 去重门 / 整改分母」在本片内的计数。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 成员份数（权威清单） | **22** |
| 实际读完份数 | **22（100%）** |
| 成员总行数（`wc -l`，与权威清单 `实际行数: 4826` 一致） | **4826** |
| 实际读入行数（含 read 工具逻辑行） | **4828** |
| 覆盖率 | **100%（22/22 份，4826/4826 行）** |

**行数口径差**：`eng/packaging/config/runtime_resources.json`（`wc -l`=75 / 逻辑 76）与 `eng/packaging/launch/demo_gc.json`（`wc -l`=46 / 逻辑 47）缺行尾换行符，故逻辑行比 `wc -l` 多 2。权威清单用的是 `wc -l` 口径，两者一致。

**未读完的：无。** 本片无任何文件被跳过或部分阅读。

**⚠️ 分片范围提示（非本片责任，报给负责人）**：`eng/packaging/config/filters.json`（**14962 行**，git 已追踪）**不出现在权威清单任何一片中**。权威清单 `:428` 的 `filters.json` 是同名的 `lib/algorithms/photometry/data/response_curves/filters.json`，不是同一个文件。该文件被本片 `config_registry.json:2390` 指为 `filter_name_policy.source_of_truth`，是全层最大单文件却无人审。

---

## 2. 本片判定：**阻断**

最重的 3 条：

1. **打包校验面已整面死亡，而全部 schema/README 仍把它当现行机器入口。**
   `eng/packaging/README.md:11-13` 与 `:26-31`、`schemas/install-tree-contract.schema.json:5`、`schemas/acsd-product.schema.json:10`、`schemas/install-tree-contract.schema.json:12`、`schemas/dependency-lock.schema.json:5`、`schemas/preset-contract.json:3`、`windows/README.md:11` 共指向 **4 个** 校验器源码，**全部不存在**（`test -e` 全 false）。删除发生在 `e5f589a6 2026-09-30 "G08-01 物理删除旧门禁与 CI（344 件 / -136676 行）"`，而 `README.md` 在 **2026-10-01** 又被改过仍未清理。`eng/ci/` 整目录同样已删 ⇒ **BLD-003/BLD-004 的所有打包主张当前零机器强制力**。

2. **`acsd.product.json` 通不过自己的 schema（14 处）。** 这是「判据不可信 + 判据已删」的直接可观测后果。
   `schemas/acsd-product.schema.json:31` `additionalProperties:false` + `:20` kind enum + `:22` module_id type ⇒ 实例 `:6` `note`、`:20`/`:24` `kind:"manifest"`、`:8,9,10,17-24` 共 11 处 `module_id:null` 全部违规。对照：`install-tree-contract.schema.json:25` 的 kind enum **含** `manifest` —— 两个 schema 互相矛盾，说明 product schema 落后于实例。

3. **`config_registry.json` 119 行登记中 86 行的锚点文档已整目录删除。**
   `docs/detail/algorithms_phase{1,2,3}/` 共 16 份 `NN_*.md` 全部不存在（`ac058121 2026-09-30` 合并），但 `config_registry.json` 每一行的 `doc`、`registered_at`、`conflict.evidence`、`anchor.sha256` 仍指向它们。另有 11 行 `registered_at` 直接指向该缺失目录。

---

## 3. 逐文件清单（22 份全读）

| # | 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `eng/packaging/README.md` (46) | 全 46 行 | `:11-13` 三个 `.py` 校验器 + `:26-31` 可执行命令，全部不存在；`:36-37` 称 required 集「6→11 项」，与 `acsd.product.json:6` 的「5→10 项」矛盾，与实际 18 项也不符；`:40` 引 `lib/snr_estimator`（不存在）；`:46` 引 `10_LINUX_CONTROL_NODE.md §5 PLATFORM_SCOPE`（全仓 0 命中） | **阻断** |
| 2 | `eng/packaging/acsd.product.json` (26) | 全 26 行 | `:6` 顶层 `note` 键不在 schema properties 内；`:20,:24` `kind:"manifest"` 不在 enum；11 处 `module_id:null` 违反 `type:"string"`；`:4` `source_commit` 是合法 commit（`git cat-file -t` = commit）；`:3` 版本与根 `VERSION` 一致 | **阻断** |
| 3 | `eng/packaging/schemas/acsd-product.schema.json` (32) | 全 32 行 | `:20` kind enum 缺 `manifest`；`:22` module_id 未允许 null（同文件 `:24` 的 sha256 却正确写成 `["string","null"]`，说明是漏改不是约定）；`:10`/`:31` 漂移判据指向已删的 `check_packaging_consistency.py C1` | **阻断** |
| 4 | `eng/packaging/install-tree.contract.json` (34) | 全 34 行 | **通得过自己的 schema**（0 违规）；25 units，`required=true` 实为 **18** 项；8 个 license/schema/自指单元不在 product manifest 中，该不对称在合同内无说明 | 须修 |
| 5 | `eng/packaging/schemas/install-tree-contract.schema.json` (34) | 全 34 行 | `:5` 机器校验入口指向已删脚本；`:25` kind enum **含** `manifest`，与 #3 矛盾；`:12` 漂移判据同指向已删脚本；`:15` 引 `03_TARGET_PRODUCT_AND_ARCHITECTURE.md §4`（全仓 0 命中） | 须修 |
| 6 | `eng/packaging/schemas/dependency-lock.schema.json` (61) | 全 61 行 | 结构正确；`:5` 机器校验入口指向已删的 `gen_sbom_input.py`；`:60` `additionalProperties:false` 但无任何消费者执行它 | 建议 |
| 7 | `eng/packaging/schemas/preset-contract.json` (64) | 全 64 行 | **不是 JSON Schema**（无 `$schema`/`$id`/`type`/`properties`），是常量表，却被放在 `schemas/` 且在 `install-tree.contract.json:26` 注册为 `kind:"schema"`；`:3` 自称的读取者 `verify_toolchain.py` 不存在；**全仓无任何实例文件** | 须修 |
| 8 | `eng/packaging/dependency-lock.json` (202) | 全 202 行 | `:46-48` 称 nlohmann `UNREFERENCED`/`referenced_by:[]`/「全树零命中」—— **实测 98 个文件命中**（含 `lib/infrastructure/aio/product_io/src/hips_manifest.cpp` 等生产源）；`:196` 称 `CMakePresets.json` 固定 `CMAKE_GENERATOR_INSTANCE=C:/AstroCS/...` —— 实测 `grep -c AstroCS CMakePresets.json` = **0**，且该文件 `:4`/`:30` 明写「vswhere 自动发现 / **NOT pinned**」；`:48` 自身引用的判据已删 | **阻断** |
| 9 | `eng/packaging/licenses/LICENSE-INDEX.txt` (23) | 全 23 行 | `:14` 源路径 `lib/astro_image_io/third_party/cfitsio/...` 不存在（真实 `lib/infrastructure/aio/third_party/cfitsio/`，与锁 `:19` 自相矛盾）；`:10` 引 `16_SCIENCE...`/`18_AUDIT` 全仓 0 命中；文件清单未列自身 | 须修 |
| 10 | `eng/packaging/licenses/CFITSIO_LICENSE.txt` (25) | 全 25 行 | **干净**。逐字读完确认是真实 NASA/USG 文本（"No copyright is claimed in the United States under Title 17"），与索引 `:15` 的「NASA/USG 许可」标注一致；短是上游原样，非截断 | 通过 |
| 11 | `eng/packaging/licenses/nlohmann_json.MIT.txt` (9) | 全 9 行 | `:7-8` 称「完整 MIT 文本随第三方源保留于 `lib/third_party/nlohmann/json.hpp`」—— **实测该文件内 MIT 正文出现 0 次**（只有 SPDX 标签）；全仓无 nlohmann MIT 正文 | **阻断** |
| 12 | `eng/packaging/config/runtime_resources.json` (76) | 全 76 行 | `:24` 的水位推导：精确算术下 `high=90` **不满足**它自己写下的 `100−high ≥ 100×1.6724/B`（最坏情形 A=17.6 ⇒ 需 10.002392，实得 10.000000，**短 0.0024pp**）；`:26` 声明的重标定适用域**不含「可用内存」**这个推导主导输入；`:7,:32,:62,:64` 四处裸从句伪引（`§8.3:609-615/614/615/643/652`）；`:65` 用字符串哨兵冒充数值；`:74` 声称两个分母已登记进生成头（实测无对应占位符） | **阻断** |
| 13 | `eng/packaging/config/runtime_resources_generated.h.in` (70) | 全 70 行 | 17 个占位符中**无** `bytes_per_pixel`/`safety_frac`；`static_assert` 覆盖 6 项但**没有**任何一项约束 `memory_pressure_high/low_percent`，也没有 `high<low` 断言；`:20,:51,:53` 复述同一批漂移行锚 | 须修 |
| 14 | `eng/packaging/config/config_registry.json` (2814) | 全 2814 行 | 4 项自声明计数（`rows 119` + 3 张子表）**全部复算相符**；但 16 份锚点文档已删（86/119 行失去证据基）；`:86` 与 `defaults.json:12` 称「96 行」实际 119；`:33` 称「20 份 module.yaml」实际 24；`:613,:616` 引 `eng/ci/*` 已删且据此声称「该面门为绿」；`:2529` 的机器门同样指向已删 `eng/ci/check_test_index.py` | **阻断** |
| 15 | `eng/packaging/config/defaults.json` (995) | 全 995 行 | `field_count:59` 复算相符；53 个 `source_ref.sha256` **全部只有 16 个 hex**（sha256 应 64）；`:846` `source_ref.path` 指向已删的 `07_noise_snr.md`；`:91` 引文非逐字（`β≠4` vs 原文 `β（≠4）`）；多条引文被按字符预算截断到半个词（`:40,:57,:87,:226,:260,:277`）；`:12` 「96 行」陈旧 | **阻断** |
| 16 | `eng/packaging/launch/start_browser.ps1` (156) | 全 156 行 | `:27,:43` 指向不存在的 `lib\healpix_db\...`（真实 `lib/infrastructure/hips_browser/healpix_browser_qt`）；`:108` 指向不存在的 `lib\astro_image_io`；`:22,:34,:42,:45` 依赖 MSYS2/MinGW，而锁 `:10` 声明 `msys2_mingw: FORBIDDEN` 且 `:199` 把同类引用登记为缺陷却未登记本文件；`:61-63` 三个默认数据路径全不存在 | **阻断** |
| 17 | `eng/packaging/launch/demo_gc.json` (46) | 全 47 行 | **零消费者**；`:3` 声明 canonical 默认 `gc_32red`，而 `start_browser.ps1:61` 首选 `gc_3panel`、`:74` 的兜底正则只匹配 `gc_3panel\|real_3frame` ⇒ 其声明的默认**永不可达** | 须修 |
| 18 | `eng/packaging/windows/.vsconfig` (12) | 全 12 行 | **干净**。7 个组件与 `windows/README.md:38-44` 逐项一致 | 通过 |
| 19 | `eng/packaging/windows/README.md` (47) | 全 47 行 | `:20-30` 八行取值与 `preset-contract.json:5-18` 逐项核对**全部相符**；`:11` 校验器不存在；`:29` 复述了锁里那个不成立的 `C:/AstroCS` 固定声明；`:46-47` 的禁面在唯一事实源 `preset-contract.json` 中无对应条目 | 须修 |
| 20 | `eng/packaging/config/templates/normalize.phase_config.json` (27) | 全 27 行 | 结构合法；`:11,:22` `pixfrac 0.8` 与 `defaults.json:800` 同值 ✓；与 schema 无冲突 | 通过 |
| 21 | `eng/packaging/config/templates/export.phase_config.json` (17) | 全 17 行 | 结构合法；无跨文件数值主张 | 通过 |
| 22 | `eng/packaging/config/templates/mosaic.phase_config.json` (11) | 全 11 行 | `:8` `snr_path:"sparse_reconstruct"` 与 `defaults.json:839` 同值 ✓ | 通过 |

---

## 4. 发现清单

### 阻断（9）

| ID | 发现 | 证据 |
|---|---|---|
| **B-1** | 4 个机器校验器源码全部删除，全部 schema/README 仍当现行入口；`eng/ci/` 整目录删除 ⇒ 打包面零机器强制 | `README.md:11-13,26-31`；`install-tree-contract.schema.json:5`；`acsd-product.schema.json:10`；`install-tree-contract.schema.json:12`；`dependency-lock.schema.json:5`；`preset-contract.json:3`；`windows/README.md:11`。`test -e` ×4 全 false；`git log --diff-filter=D` → `e5f589a6 2026-09-30` |
| **B-2** | `acsd.product.json` 违反自有 schema 14 处 | 实例 `:6`(note ×1)、`:20`+`:24`(kind ×2)、`:8,9,10,17,18,19,20,21,22,23,24`(module_id ×11) vs `acsd-product.schema.json:31`/`:20`/`:22`。复算见 §8-CMD3 |
| **B-3** | 依赖锁称 nlohmann「全树零命中」而实为 98 处引用（含生产源）；且仓内无任何 nlohmann MIT 正文 | `dependency-lock.json:46,47,48` vs `git grep -lI 'nlohmann/json\.hpp'` = 98；`nlohmann_json.MIT.txt:7-8` 的断言 vs `grep -c 'hereby granted, free of charge' lib/third_party/nlohmann/json.hpp` = **0** |
| **B-4** | 配置登记册 86/119 行锚点文档整目录删除 | `config_registry.json` 16 份 doc 全 MISSING（`:93,:225,:333,:586,:668,:778,:844,:995,:1067,:1109,:1217,:1397,:1482,:1542,:1632,:1698`）；另 11 行 `registered_at` 指向缺失目录 |
| **B-5** | `runtime_resources.json` 对 `ACSD_DESIGN.md` 的 6 个行锚全部落空 + 4 处裸从句伪引 | 该文档 682 行，§8.3 实为 469-481；引用的 `:609`=P5 表行、`:614`/`:643`/`:652`=**空行**、`:615`=下级索引行、`:647`=IMPLEMENTED 状态行。`grep -cF`：「可丢弃重跑」0、「内存占用永不越界」0、「预取下一帧」0、「基于探针实测数据迭代」0（仅「可中断排队」1 命中） |
| **B-6** | 锁 `:196` 的机器路径白名单例外建立在一条与被引文件**相反**的声明上 | `dependency-lock.json:196` 称 `CMakePresets.json` 固定 `C:/AstroCS/toolchains/vs2022-17.14.39`；实测 `grep -c AstroCS CMakePresets.json` = **0**，而 `CMakePresets.json:4`/`:30` 明写「no CMAKE_GENERATOR_INSTANCE override」「VS instance is NOT pinned … a hardcoded CMAKE_GENERATOR_INSTANCE local path made hosted configure fail」 |
| **B-7** | 启动器指向已重组掉的目录，且依赖锁明令禁止的工具链；其自检 `.pyc` 还会覆写自己要校验的文件 | `start_browser.ps1:27,43`（`lib\healpix_db`）、`:108`（`lib\astro_image_io`）均不存在；`:22,34,42,45` MSYS2/MinGW vs `dependency-lock.json:10` `FORBIDDEN`；`eng/packaging/__pycache__/check_packaging_consistency.cpython-313.pyc` 仍在活动树中且其 `--self-test` 路径含 `os.link` + 原地 `write_text` |
| **B-8** | **未申报的第三方 vendored 组件 `json-schema-validator`：确实参与编译，却不在依赖锁、不在许可证索引、不在安装树** | 磁盘：`lib/infrastructure/pipeline/orchestrator/cpp/third_party/json-schema-validator/`（10 个 git 追踪文件）；真编译：`lib/infrastructure/pipeline/orchestrator/cpp/CMakeLists.txt:26-36`（`add_library(acsd_orchestrator_jsv STATIC …)`，6 个源文件）；许可：`json-validator.cpp:4` `SPDX-License-Identifier: MIT`（Copyright (c) 2016-2019 Patrick Boettcher）；申报：`grep -c` 在 `dependency-lock.json`=**0**、`LICENSE-INDEX.txt`=**0**、`install-tree.contract.json`=**0**。仓内共 3 个 vendored 组件根（cfitsio / json-schema-validator / nlohmann），依赖锁只登记了 2 个 |
| **B-9** | **唯一还活着的清单验收脚本被冻结在旧 10-unit 形态，对当前 17-unit 清单永远判红** —— 这是 M-2「11/10/18」不一致的机械成因 | `eng/tests/abi/mod001_install_load_check.py:269`：`check("S4 manifest units=10 …", len(munits) == 10, …)`；而 `acsd.product.json` 实有 **17** 个 unit。该脚本 `:254` 读 `install-tree.contract.json`，是本片合同**唯一**的活代码读者。它未被触发只是因为 `eng/ci/` 已删（B-1）⇒ 文档里的「units=10 … PASS」是冻结快照，不是现行结论 |

### 须修（14）

| ID | 发现 | 证据 |
|---|---|---|
| M-1 | 名为 `sha256` 的字段全是 16 hex（64 位），不是 sha256 | `defaults.json` 53/53、`config_registry.json` 119/119 长度直方图均为 `{16: N}`；sha256 须 64 |
| M-2 | MOD-001「required 集」三方不一致：11 / 10 / 18 | `README.md:36-37`=11；`acsd.product.json:6` note=10；`install-tree.contract.json` 实际 `required=true` 计数=18。**成因已定位**：`10` 这个数与 `mod001_install_load_check.py:269` 的 `len(munits)==10` 是同一个冻结值（见 B-9）；合同扩到 18 时无人回改脚本或 note |
| M-3 | `frame_memory_gate` 两个分母无生成头承载，`:74` 却称已登记并逐位比对 | `runtime_resources.json:71,72,74` vs `.h.in` 17 个占位符中无 `bytes_per_pixel`/`safety_frac`；比对器 `check_budget_single_source.py` 亦不存在 |
| M-4 | `memory_pressure_high_percent=90` 不满足它自己写下的不等式 | `runtime_resources.json:22,24`：精确算术下 A=17.6 ⇒ 需 `100−high ≥ 10.002392`，实得 10.000000，**余量 −0.002392pp**；「留足一帧余量」不成立 |
| M-5 | 声明的重标定适用域遗漏其推导的主导输入 | `runtime_resources.json:24` 依赖「本机 A ∈ [17.6,23.3] GB」，而 `:26` 的适用域只列帧几何/精度/nside/scratch 池，**不含可用内存**；A < 17.6042 GB 时 `high=90` 即不安全 |
| M-6 | 许可证索引的来源路径不存在 | `LICENSE-INDEX.txt:14` → `lib/astro_image_io/...`；真实 `lib/infrastructure/aio/third_party/cfitsio/licenses/License.txt`（与 `dependency-lock.json:19` 矛盾） |
| M-7 | 两处计数陈旧 | `config_registry.json:86` 与 `defaults.json:12` 称「96 行」，实际 119；`config_registry.json:33` 称「20 份 module.yaml」，实际 24 |
| M-8 | `lib/snr_estimator` 不存在 | `README.md:40`、`acsd.product.json:6`；真实为 `lib/algorithms/noise_snr` |
| M-9 | 伪引：`psf.moffat_beta` 引文非逐字 | `defaults.json:91` 引 `β≠4`，`docs/science/PSF.md:152` 实为 `β（≠4）`（括号被删） |
| M-10 | 4 个 schema 全部无活消费者；`preset-contract.json` 非 schema 却被注册为 schema 且无实例 | 仓内有可用校验器 `eng/tests/common/jsonschema_min.py`，但只接 `eng/contracts/schemas/**`，不接 `eng/packaging/schemas/**`；`CMakePresets.json:14` 还指向不存在的 `preset-contract.schema.json` |
| M-11 | 合同 25 units vs 清单 17 units 的核对门已随校验器删除 | 差异 8 项全为 license/schema/自指单元（方向合理），但核对它的 `合同↔manifest 单元集` 判据（`README.md:11`）不存在 |
| M-12 | `demo_gc.json` 零消费者，且其声明的默认不可达 | 全仓 `grep demo_gc.json` 仅命中自身；`demo_gc.json:3` 的 `gc_32red` 落在 `start_browser.ps1:74` 正则之外 |
| M-13 | 两个已发货默认值未登记为 `defaults_json` | `defaults.json:827 sparse_snr.spacing_px`、`defaults.json:838 snr.path`；登记册 `:982`/`:928-932` 分别标为 `plugin_doc`/`phase_config`，而 `:932` 自述「默认值唯一登记 = defaults.json#snr.path」 |
| M-14 | `start_browser.ps1` 有**两个互相独立**的缺陷：路径基准差一级（即使路径名正确也跑不起来） | `:26` `$repo = Split-Path -Parent $PSScriptRoot`；脚本位于 `eng/packaging/launch/` ⇒ `$PSScriptRoot`=`<root>/eng/packaging/launch`，`Split-Path -Parent` ⇒ `$repo`=`<root>/eng/packaging`，**不是仓库根**。`:27`/`:43`/`:108` 的每个 `Join-Path $repo …` 因此整体上移一层。需两次 `Split-Path -Parent` 才到根 |

### 建议（5）

| ID | 发现 | 证据 |
|---|---|---|
| S-1 | `windows/README.md:46-47` 的禁面（MFC/ATL/C++CLI/UWP/WinUI/Windows App SDK/`--includeRecommended`）在唯一事实源 `preset-contract.json:25-55` 中无对应条目，违反其 `:17` 「漂移一律以机器源为准」 | 逐项比对 |
| S-2 | `LICENSE-INDEX.txt:12-17` 的文件清单未列自身，尽管 `install-tree.contract.json:22` 将其作为 `LIC-INDEX` 登记并随包分发 | 目录实有 3 个 txt |
| S-3 | `runtime_resources.json:65` 用字符串哨兵 `"gap:未落地（…）"` 混入一组数值型编排键 | 登记册 `:2700-2709` 已按「不落死键」正确登记，但 JSON 形态对任何通用读取器是隐患 |
| S-4 | 合同 25 / 清单 17 的自指不对称未在合同中说明 | `install-tree.contract.json:7-33` vs `acsd.product.json:7-25` |
| S-5 | 4 个 `declared_negatives` 字面量无锚点，另 2 个有锚点，不对称 | `config_registry.json:2415-2434` vs `:2392-2414` |

---

## 5. 我主动构造的反例

### CE-1（推翻成功）— 用它自己写下的不等式检验它自己
`runtime_resources.json:24` 裸从句写：`需 100 − high ≥ 100 × (单帧边际 RSS) / 预算`，并称取值 90 使「最坏情形（B = 16.7 GB）也留足一帧余量」。
**期望推翻**：该结论自洽，无需检验。
**结果：推翻成功。** 精确算术下 `B = 0.95 × 17.6 = 16.72`（文中把 16.72 舍成 16.7），`100 × 1.6724 / 16.72 = 10.002392`（文中把 10.0024 舍成 10.0），于是 `100 − 90 = 10 < 10.002392`。**余量为负 0.002392 个百分点**，「留足一帧余量」不成立；恰好「凑够」靠的是两次向下取整。
这正是本轮要找的形态：**用同一段推导既当被检量又当期望量，且判据被自身的舍入动作放宽到刚好相切。**

### CE-2（推翻成功）— 换机器推导即失效，而适用域声明不覆盖这个变量
**期望推翻**：水位是通用默认值，推导应可迁移。
**结果：推翻成功。** `high=90` 仅在 `A ≥ 100×1.6724/(0.95×10) = 17.6042 GB` 时安全。A=16 GB 需 high ≤ 88.997、A=12 GB 需 ≤ 85.330、A=8 GB 需 ≤ 77.995 —— 三者都被发货值 90 违反。而 `runtime_resources.json:26` 的重标定纪律**只列**帧几何/FP64/nside/scratch 池，**未列可用内存**，读者会被误导以为换机器不必重推。

### CE-3（推翻成功）— 用 schema 自身当外部参照，14 处不过
**期望推翻**：既然仓内宣称「schema 机器校验」，实例应自洽。
**结果：推翻成功。** 手工按 draft 2020-12 逐条判：`note` 违反 `:31` `additionalProperties:false`；`kind:"manifest"` ×2 违反 `:20` enum；`module_id:null` ×11 违反 `:22` `type:"string"`（而 `:24` 的 `sha256` 正确写成 `["string","null"]`，证明这是漏改）。
**结构性观察**：姊妹 schema `install-tree-contract.schema.json:25` 的 enum **含** `manifest`。也就是说这两个 schema 对「manifest 是不是合法 kind」给出**相反**答案 —— 一个 schema 被当成另一个 schema 的外部参照时，参照系本身就自相矛盾。

### CE-4（推翻成功）— 用依赖锁自己声明的检索范围复现它的「零命中」
**期望推翻**：`dependency-lock.json:48` 列举了确切检索面（`CMakeLists.txt / eng/cmake/** / lib/** / lib/include/** / cli/**`）并断言零命中，这是可证伪的强断言。
**结果：推翻成功。** 在其自述范围内 `git grep -lI 'nlohmann/json\.hpp'` 命中 98 个文件，含 `lib/infrastructure/aio/product_io/src/hips_manifest.cpp`、`lib/algorithms/integration/phase1_product/src/phase1_product.cpp`、`lib/phase{1,2,3}_session/*.cpp` 等**随产品分发的生产源**。叠加 CE-5 ⇒ 安装树会分发 MIT 许可声明，却不携带任何 MIT 许可正文。

### CE-5（推翻成功）— 用字段名承诺的强度检验字段值
**期望推翻**：`sha256` 字段名自带承诺。
**结果：推翻成功。** 长度直方图：`defaults.json` `{16: 53}`、`config_registry.json` `{16: 119}`，无一条达到 64。这些「指纹」指向的文件大多已删除，**其可核性为零**。

### CE-6（推翻成功）— 用依赖锁自己的登记口径审依赖锁
**期望推翻**：`dependency-lock.json` 是 BLD-004 的登记面，其列出的 vendored 组件应完备。
**结果：推翻成功。** 用它自己的口径枚举：`git ls-files | grep third_party/` 折叠到组件根 ⇒ 仓内共 **3** 个 vendored 组件（cfitsio / json-schema-validator / nlohmann），而 `production_dependencies[]` 只登记 **2** 个。漏登的 `json-schema-validator` 恰恰是**真编译**的那个（`CMakeLists.txt:26-36` `add_library(acsd_orchestrator_jsv STATIC …)`）、且是 **MIT**（`json-validator.cpp:4`），却既不在锁、也不在 `LICENSE-INDEX.txt`、也不在 `install-tree.contract.json`。被判据 C4（依赖锁↔实树）与 C7（许可登记）本应拦下——而这两个判据已随 B-1 删除。

### CE-7（未能推翻 — 记为 CLEAN）
`filter_name_policy.library_facts.keys: 45` vs `filters.json` 实际 45 键；4 个 `declared_positives` 全部命中；6 个 `declared_negatives` **无一**是库键。**此处无法构造反例**，判通过。
4 项自声明计数（`field_count 59`、`rows 119`、3 张子表、`filters keys 45`）**全部复算相符** —— 这是本片唯一成体系的、确实自洽的部分。

---

## 6. 盲复算

**方法**：在读既有审稿产物（`审稿-RR*/R2*/R3*/P1-*`）**之前**，只用权威清单 + 原文 + 自写脚本独立取证。

| 项 | 我独立算出的 | 既有逐份判定口径 | 关系 |
|---|---|---|---|
| 成员份数/行数 | 22 / 4826 | 清单 `实际行数: 4826` | 一致 |
| `defaults.field_count` | 59 = 实际 59 | — | 一致 |
| `config_registry.rows` | 119 = 声明 119；3 张子表逐格相符 | — | 一致 |
| `filters.json` 键数 | 45 = 声明 45 | — | 一致 |
| `product_version` vs 根 `VERSION` | `0.1.0-alpha.1` 三处全一致 | — | 一致 |
| `.vsconfig` vs `windows/README:38-44` | 7/7 逐项一致 | — | 一致 |
| **`acsd.product.json` 自洽性** | **14 处违规** | 「默认保留（非产出面或非数据形态）」 | **偏严** |
| **锚点文档存在性** | **16 份全删，86/119 行失据** | 同上 | **偏严** |
| **依赖锁 nlohmann 断言** | **98 处引用，断言为假** | 同上 | **偏严** |
| **`runtime_resources` 水位推导** | **不满足自写不等式** | 同上 | **偏严** |

**判定：偏严。** 既有逐份判定把本片 21/22 份标为「默认保留（非产出面或非数据形态）」、1 份为「P3 手写面豁免」。该口径把**数据文件**与**合同/证据面**一视同仁地豁免，结果是：4 份 JSON Schema、1 份产品清单、1 份安装树合同、1 份依赖锁——全部是**对外分发与合规面**，却因「非数据形态」而免于任何检查。本片真正的判据是**反向**的：`acsd.product.json` 是**被校验对象**，但它自己携带的 schema 就是唯一参照系，而那个参照系已经失效（B-1）。**没有哪一份成员是「非产出面」**——它们全都进安装树。

**未发现「既有结论偏严」的项**：4 项自声明计数、`VERSION` 单源、`.vsconfig` 镜像、cfitsio 许可证文本、`install-tree.contract.json` 自洽性，全部经我独立复算确认成立，既有判定在这些点上是对的。

---

## 7. 子代理派发记录

派发 **5 次**、实际执行 **4 个不同任务**（我在同一消息中重复粘贴了一次「悬空引用审计」，产生两个同题代理；两者结论一致，互为冗余验证）。

| # | 任务 | 结论摘要 | 我的复核 |
|---|---|---|---|
| 1 | 悬空引用审计 | 4 个 `.py` 全删、`.pyc` 残留、`e5f589a6` 删除点、伪引台账 | **采纳**。我亲自复核了 `test -e` ×4、`git ls-files`、`git log --diff-filter=D`，结论一致 |
| 2 | schema↔实例一致性 | `acsd.product.json` 14 处违规；其余两对 0 违规；4 个 schema 全无活消费者 | **全盘采纳**。我先于子代理独立用自写脚本算出同样的 14 处（`note`×1、`manifest`×2、`module_id`×11），数值完全吻合，互为印证 |
| 3 | 消费者与桩分析 | `eng/ci/` 整目录不存在；`.pyc` 是自毁型 checker；`local.paths.json` 零消费者 | **采纳但降级**。`eng/ci` 不存在、`local.paths.json` 我亲自确认；**否决**其「`.pyc --self-test` 会覆写真实文件」为**阻断级**——该 `.pyc` 不在任何文档化执行路径上、`eng/packaging/` 下零 `.py`，无任何入口会调用它，故降为须修（残留产物清理），不单列阻断 |
| 4 | 内容准确性重推导 | nlohmann「零命中」被证伪（98 处）；仓内无 MIT 正文；`CMakePresets` 机器路径声明与被引文件相反 | **采纳**。我亲自复跑 `git grep -lI`（得 98，与子代理一致）、`grep -c 'hereby granted' json.hpp`（得 0）、`grep -c AstroCS CMakePresets.json`（得 0），三处全部独立复现 |

**否决清单**：
- **否决 3 号「`.pyc` 自毁 ⇒ 阻断」**，理由：无法从任何在册入口触达，定级过高，改为须修。
- **否决 3 号「`local.paths.json` 零消费者 ⇒ 配置被重定向」**，理由：该文件 gitignored 且唯一提及点是 `defaults.json:885` 的说明文本本身，不构成配置重定向。
- **部分否决 1 号「`windows/README.md` 取值表与 `preset-contract.json` 不符」**，理由：我逐项核对 8 行全部相符，判定为**通过**；真实缺陷在 `:11` 的校验器悬空与 `:29` 复述错误机器路径，属别处。
- **修正 4 号「MIT 正文缺失」的因果链**：其原表述为「因未引用而无需分发」，我核实 `lib/third_party/nlohmann/json.hpp` 被 98 处生产源引用，故**必须**分发；结论方向不变但理由更重。
- **否决 4 号「`start_browser.ps1` 因 `$repo` 差一级而不可用」作为唯一解释**，理由：路径名本身（`lib\healpix_db`、`lib\astro_image_io`）也已不存在，两个缺陷独立；已拆为 B-7 与 M-14 两条。
- **采纳 4 号 R3（`json-schema-validator` 未申报）**，我另行独立复跑 `git ls-files` 枚举组件根、`sed` 读 `CMakeLists.txt:26-36` 编译定义、`sed` 读 `json-validator.cpp:1-6` 的 MIT SPDX，并对三份登记面各跑一次 `grep -c`（皆 0）——四项全部独立复现，升级为 **B-8 阻断**。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false rev-parse HEAD   # 850a9edefd47434b9ab71bc907c3de1e0814b323

# --- B-1 四个机器校验器源码均不存在 ---
for p in eng/packaging/verify_install_tree.py eng/packaging/check_packaging_consistency.py \
         eng/packaging/gen_sbom_input.py eng/cmake/toolchain/verify_toolchain.py eng/ci; do
  [ -e "$p" ] && echo "EXISTS $p" || echo "MISSING $p"; done
git -c core.quotepath=false log --diff-filter=D --format='%h %ad %s' --date=short \
  -- eng/packaging/verify_install_tree.py
ls eng/packaging/__pycache__/          # 残留 .pyc
git -c core.quotepath=false ls-files eng/packaging | grep -c '\.py$'   # 0

# --- B-2 acsd.product.json 违反自有 schema（14 处）---
python3 - <<'PY'
import json
s=json.load(open('eng/packaging/schemas/acsd-product.schema.json',encoding='utf-8'))
i=json.load(open('eng/packaging/acsd.product.json',encoding='utf-8'))
u=s['properties']['units']['items']['properties']
print('顶层未声明键:', [k for k in i if k not in s['properties']])
print('kind 越界:', [(x['unit_id'],x['kind']) for x in i['units'] if x['kind'] not in u['kind']['enum']])
print('module_id 非 string:', sum(1 for x in i['units'] if not isinstance(x.get('module_id'),str)))
print('姊妹 schema 的 kind enum 含 manifest:', 'manifest' in
      json.load(open('eng/packaging/schemas/install-tree-contract.schema.json',encoding='utf-8'))
      ['properties']['units']['items']['properties']['kind']['enum'])
PY

# --- B-3 依赖锁「零命中」被证伪 + MIT 正文缺失 ---
git -c core.quotepath=false grep -lI 'nlohmann/json\.hpp' | wc -l          # 98
git -c core.quotepath=false grep -lI 'nlohmann/json\.hpp' -- lib | grep -vE 'test|oracle' | head
grep -c 'hereby granted, free of charge' lib/third_party/nlohmann/json.hpp  # 0

# --- B-5 ACSD_DESIGN 行锚漂移 + 裸从句伪引 ---
wc -l docs/ACSD_DESIGN.md            # 682；§8.3 实际在 469-481
for L in 609 614 615 643 647 652; do printf ':%s |%s\n' $L "$(sed -n "${L}p" docs/ACSD_DESIGN.md)"; done
for q in 可丢弃重跑 内存占用永不越界 预取下一帧 基于探针实测数据迭代 可中断排队; do
  printf '%-22s %s\n' "$q" "$(grep -cF "$q" docs/ACSD_DESIGN.md)"; done

# --- B-6 机器路径白名单例外与被引文件相反 ---
grep -c 'AstroCS' CMakePresets.json            # 0
grep -n 'GENERATOR_INSTANCE\|NOT pinned' CMakePresets.json | head -2

# --- CE-1 / CE-2 水位推导复算（最关键反例）---
python3 - <<'PY'
marg=1.6724
for A in (8.0,12.0,16.0,17.6,23.3,64.0):
    B=0.95*A; need=100*marg/B; hi_max=100-need
    print(f'A={A:>5} B={B:7.3f} 需 100-high>={need:7.3f} 允许 high<={hi_max:7.3f} 发货 90 ->',
          'OK' if hi_max>=90 else 'VIOLATED')
print('high=90 安全的最小 A =', 100*marg/(0.95*10))
PY

# --- M-1 名为 sha256 实为 16 hex ---
python3 - <<'PY'
import json,collections
d=json.load(open('eng/packaging/config/defaults.json',encoding='utf-8'))
print('defaults source_ref.sha256 长度:', dict(collections.Counter(
    len(f['source_ref']['sha256']) for f in d['fields'] if f.get('source_ref'))))
g=json.load(open('eng/packaging/config/config_registry.json',encoding='utf-8'))
print('plugin_knobs anchor.sha256 长度:', dict(collections.Counter(
    len(r['anchor']['sha256']) for r in g['plugin_knobs'])))
PY

# --- B-4 锚点文档整目录删除（86/119 行）---
python3 - <<'PY'
import json,os,collections
g=json.load(open('eng/packaging/config/config_registry.json',encoding='utf-8'))
c=collections.Counter(r['doc'] for r in g['plugin_knobs'])
miss=[d for d in c if not os.path.exists(d)]
print('缺失文档数:',len(miss),'/ 受影响行数:',sum(c[d] for d in miss),'/',len(g['plugin_knobs']))
PY

# --- B-8 / CE-6 未申报的 vendored 组件（3 个组件根，锁只登 2 个）---
git -c core.quotepath=false ls-files | grep -i 'third_party/' \
  | awk -F/ '{for(i=1;i<=NF;i++) if($i=="third_party"){print $1"/"$2"/"$3; break}}' | sort -u
git -c core.quotepath=false ls-files lib/infrastructure/pipeline/orchestrator/cpp/third_party/ | wc -l  # 10
sed -n '26,29p' lib/infrastructure/pipeline/orchestrator/cpp/CMakeLists.txt   # add_library(acsd_orchestrator_jsv STATIC
sed -n '1,6p' lib/infrastructure/pipeline/orchestrator/cpp/third_party/json-schema-validator/json-validator.cpp
for f in eng/packaging/dependency-lock.json eng/packaging/licenses/LICENSE-INDEX.txt \
         eng/packaging/install-tree.contract.json; do
  printf '%-45s 声明数=%s\n' "$f" "$(grep -c 'json-schema-validator' "$f")"; done   # 三者皆 0

# --- M-14 start_browser.ps1 路径基准差一级 ---
sed -n '26,27p' eng/packaging/launch/start_browser.ps1
# 脚本在 eng/packaging/launch/ ⇒ Split-Path -Parent 一次得到 <root>/eng/packaging，非仓库根

# --- B-9 唯一活着的清单验收脚本冻结在旧 10-unit 形态 ---
sed -n '267,270p' eng/tests/abi/mod001_install_load_check.py
python3 -c "
import json;m=json.load(open('eng/packaging/acsd.product.json',encoding='utf-8'))
c=json.load(open('eng/packaging/install-tree.contract.json',encoding='utf-8'))
print('manifest units =',len(m['units']),'| 脚本断言 == 10 ->',len(m['units'])==10)
print('contract required=true =',sum(1 for u in c['units'] if u['required']),'| README 声称 11 | note 声称 10')"

# --- CE-7 通过项（记 CLEAN）---
python3 - <<'PY'
import json
f=json.load(open('eng/packaging/config/filters.json',encoding='utf-8'))
k=f.get('lookup',{}).get('keys') or list((f.get('filters') or {}).keys())
print('filters 键数:',len(k),'(声明 45)')
PY
```

---

## 9. 建议处置顺序（供负责人裁决，本片不改任何仓内文件）

1. **B-1**：先裁定「恢复校验器」还是「正式退役并改写全部 8 处引用」。这是根因——B-2 与 M-11 都是它的直接产物。
2. **B-2**：`acsd-product.schema.json` 的 kind enum 补 `manifest`、module_id 改 `["string","null"]`、`properties` 声明 `note`；或从清单删 `note`。二者取一。
3. **B-3**：修正 `dependency-lock.json:46-48` 的 `UNREFERENCED` 断言，并把 nlohmann 移入 `UNREFERENCED` 之外的**已消费**类；补 MIT 正文（当前为分发合规风险，非文档瑕疵）。
4. **B-5 / CE-1 / M-4 / M-5**：重锚 `ACSD_DESIGN.md` 行号并补「可用内存」进适用域；水位取值改用精确算术复核（当前最坏情形余量为负）。
5. **B-4 / M-7 / M-8 / M-6 / M-9**：批量重指或退役陈旧引用。
6. **分片范围**：`eng/packaging/config/filters.json`（14962 行）当前无片覆盖，建议补片。

**本片零 git 写、零仓内文件改动、零编译、零测试执行。**