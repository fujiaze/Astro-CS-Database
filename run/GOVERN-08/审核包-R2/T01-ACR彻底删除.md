# T01 彻底删除 ACR — 清点与处置交付

## 摘要（先读这一段）

| 问题 | 答案 |
|---|---|
| 代码子树删干净了吗？ | **是。** `lib/infrastructure/acr` 已物理删除（`git ls-tree` → 0）。构建面**零 ACR 目标、零悬空、零孤儿**。 |
| 清点多少处？ | naive 3561 行 / 267 文件（全仓）→ 排除治理交付物 **996** → 排除子串巧合 **565** → 排除第三方(0)/历史证据(314) → **活动面 251 行**（终检正则口径 253） |
| 删了多少？ | **本单未删任何文件**（唯一可写文件即本交付件）。给出**可直接执行的删除/改写清单**：文档 16 类、配置合同注册 5 组、构建面 1 处、脚本 1 个。 |
| **配置合同那项补做了没有？** | **补做了，而且坐实了缺口。** `eng/contracts` 两份注册表共 **7 处** ACR 命中，其中 **3 处是指向已删文档的悬空注册条目**（`SCI-ACR-EQUIV-001` / `ALG-ACR-EQUIV-001` / `SUP-06`），另有 **2 处活工具里的死路径映射**、**1 处必红的追溯合同行**。 |
| 哪些没动、为什么？ | ① 子串巧合 431 行（`macro`/`across`/`PROSACResult`/`GaiaCreateDiagnostics`）；② 同名但代码仍活着 **19 组**（`acr_route` 等，删文档即制造新的文档与代码不符）；③ 历史证据 314 行；④ 第三方 343 行（已证明零 ACR）。 |
| 最大的意外发现 | **`acr_route` 是活键，且删它有 fail-closed 顺序陷阱**（§5.5）；**出厂 `acsd` 二进制里仍带 ACR 命名符号**（§5.2）；**`build/acr/` 陈旧构建树仍在磁盘上**（§5.4）。 |

---

基线 HEAD `b4f37def`（"ACR 文档退场：清掉仍带 ACR 的六处引用"），工作树 `git status --short` 为空。
`pre-acr-removal` 标签在位且未动：`git tag -l` → `astrocs-baseline-p00 / backup/main-before-precision-closure / milestone/stage1-history-consolidated / pre-acr-removal / pre-commitB-e8387dd9`。

本单**未改动任何被审文件**：唯一写入为本交付件。所有删除/改写均为「建议清单」，由前台执行并统一提交。

### 0.1 相关前序提交（已核 `git log --oneline`）

| commit | 标题 | 与本单关系 |
|---|---|---|
| `383088f2` | 退场 ACR 子树：152 份树 + 3 件树外专用接线，共 155 份 / 32,204 行 | 前序代码退场（任务书所述「已完成部分」属实） |
| `700b5a10` | ACR 文档退场（进行中批次）并入库六条并行取证交付件 | **删了 `docs/science/ACR_EQUIVALENCE.md` 与 `algorithms/ACR_EQUIVALENCE_ALGORITHMS.md`，但没动 `contract_index.yaml`** ⇒ §4.1 悬空条目的成因 |
| `11667219` | G08-03 文档去重合并（第一批）：docs 主题唯一化，悬空引用清零 | `contract_index.yaml:636` 那条是**更早遗留**的悬空（指向的实体已被并为 `ACR_EQUIVALENCE_ALGORITHMS.md`） |

---

## 1. 全仓清点结果

### 1.1 清点口径与四步收敛

| 步骤 | 命令 | 命中行 | 说明 |
|---|---|---|---|
| ① 原始子串 | `git -c core.quotepath=false grep -inI 'acr' -- . ':(exclude)run/GOVERN-08'` | **996** | 含大量巧合 |
| ② 去巧合 | 同上加 `-P '(?i)(?<![a-z])acr(?![a-z])'` | **565** | 排除 across/macro/PROSAC/GaiaCreate |
| ③ 第三方 | ②限定到 `lib/third_party` `aio/third_party` `healpix_db/archive/legacy` | **0** | 第三方内无 ACR 组件 |
| ④ 历史证据 | ②落在 `实验/` `artifacts/` | **314** | 冻结审计快照 = 历史 |
| ⑤ **活动面** | ②落在 `lib/ eng/ docs/` | **251** | **真正要处置的面** |

251 再分：`lib/`+`eng/` 共 **123** 行（其中真代码约 66 行，余为 lib 内 README/memory.md/configs），
`docs/` 共 **128** 行。

### 1.2 已确认完成的既有工作

- `lib/infrastructure/acr` 源码树已物理删除：`ls lib/infrastructure/` 无 `acr` 目录；
  `git -c core.quotepath=false ls-tree -r --name-only HEAD -- lib/infrastructure/acr | wc -l` → `0`。
- 根构建面已无 ACR：`CMakeLists.txt`、`CMakePresets.json` 对 `(?i)(?<![a-z])acr(?![a-z])` 零命中
  （`git grep -P` 退出码 1）。`ENABLE_ACR`（含 `ACSD_ENABLE_ACR`/`ASTROCS_ENABLE_ACR`）在
  `CMakeLists.txt CMakePresets.json eng/cmake/` 中零命中。
- `docs/**` 的 mermaid 代码块 0 个含 ACR（子代理逐块扫描），模块图无需处理。

### 1.3 逐类处置汇总（活动面 251 行）

| 类 | 处置 | 行数（量级） |
|---|---|---|
| A 活组件残留（文档陈述已不存在的组件/开关/符号） | **删/改写** | 见 §3 |
| B 配置与合同面（本次补做，前次遗漏） | **删** | 见 §4 |
| C 子串巧合 | **一字不动** | 996−565=431 行 |
| D 同名但代码仍活着 | **代码保留**，登记不一致 | 见 §5 |
| E 历史证据 | 不动 | 314 行 |

---

## 2. 「子串巧合」完整清单（未动，附理由）

全部由正则 `(?i)(?<![a-z])acr(?![a-z])` 排除，即"三字母两侧都不是字母"才算 ACR 组件。

| 族 | 判据 | 代表 `file:line` |
|---|---|---|
| `macro` / `macros` / `Macros` / `Macro` / `log_macros` / `LOG_MACROS_H` / `macro_sources` / `header_macros` | `mac**ro**` 含 acr | `lib/algorithms/photometry/cpp/include/log_macros.h:1` |
| `across` / `neighbor_across` / `kappa_sd_across_seeds` / `*_across_*` | `**acr**oss` | `lib/algorithms/photometry/memory.md`、`lib/algorithms/coverage/tools/controlled_rejection_metrics.py` |
| `PROSACResult` | `PROS**ACR**esult`，PROSAC 是 RANSAC 族真实算法 | `lib/algorithms/platesolve/cpp/ipv/include/ipv_types.h`、`ipv_ransac.cpp`、`ipv_ransac.h` |
| `GaiaCreateDiagnostics` | `Gai**aCr**ea` | `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c`、`gaia_client.h` |
| `macro` 变量名 | `def _macro(name)` / `macro = ...` | `eng/tools/isa_feature_bits.py:80,86,87,88,89,94,95,101,104,105,111,112,114,126,127,128`（该文件 16 处命中**全部**是此族） |
| `macros` 作为 JSON 键 | `"macro": "ACSD_BACKEND_REQUIRED_FEATURES"` | `eng/tools/quality/isa_sites.json:81,85,104,108,120,125,150,155,175,180,192` |

### 2.1 两个文件名巧合的判定

**`lib/algorithms/photometry/cpp/include/log_macros.h` — 保留，不得改名。**
纯头文件保护宏头，`acr` 仅来自 `macros`。改名会波及所有 `#include`。判定：巧合。

**`lib/algorithms/coverage/tools/memory_acr_compare.py` — 不是巧合，属 ACR，建议删。**
读内容后确认它**确实是 ACR 对比脚本**，而非"名字像 ACR 的普通内存对比脚本"：

- `:1` `# lib/algorithms/coverage/tools/memory_acr_compare.py — R9 machine-readable compare`
- `:5` `#   acr:    CPU Sigma vs CUDA Sigma`
- `:91` `# ACR：CPU Sigma vs CUDA Sigma`
- `:92` `report["acr"] = {`
- `:119` `print("MEMORY_ACR_COMPARE=" + ...)`

它一半比 memory（large/medium/tiny），一半比 ACR（CPU Sigma vs CUDA Sigma）。ACR 侧已无实现，
比较已不可能。

**终审结论：属 ACR 的脚本 → 删除候选，且删除安全。** 理由（全部实测）：
- 硬编码 `ROOT = r"F:\Astro dev\Astro CS Normalization Database"`（`:16`）——**另一个仓名**，本仓跑不起来；
- 全仓 20 处引用**全是治理记录散文**，`CMakeLists.txt`/`*.cmake`/`eng/tests`/CI **零引用**；
- ACR 侧比较已无对象。

⚠️ 但有一个**负责人裁决点**：该脚本另一半（memory 不变性 large/medium/tiny）是**仍可能有价值的门**。
若要保留 memory 侧，应先摘掉 `report["acr"]` 分支与 `cpu_sigma` 输入（`:22,:28`）再改名
（如 `memory_compare.py`），**而不是整文件删**。这是产品判据取舍，不该由机械删除代替。

---

## 3. 文档侧「活组件残留」清单（删/改写）

### A1 ★高危 `docs/engineering/ARCH-001.md` §6「ACR 隔离」整节（:151–158）

一级正本整节陈述一个**已不存在的组件**：

- `:153` 「默认构建 `ACSD_ENABLE_ACR=OFF`」——该 option 在 `CMakeLists.txt`/`CMakePresets.json` 中零命中。
- `:157` 「ACR 源码保留 dormant target，可独立构建/测试」——源码树已删，无任何 ACR target。
- `:167` 「生产构建的链接图、导出符号与运行模块表内没有 ACR」。

处置：**整节删除**（§6 及其后编号顺延），或改写为"生产面不含任何非 CPU 加速后端"。推荐前者。

### A2 ★高危 `docs/engineering/CONFIG_SCHEMA.md:144`（配置合同行）

```
| acr_routing(DORMANT，非生产) | stage2.integration.acr_route | acr/dispatcher | Dispatcher::decide / register_phase2_acr_kernels | DORMANT（保留源码与隔离测试；…） | diagnostics.route | TEST-ACR-001 |
```

逐项已证伪：

- `acr/dispatcher` 实现 —— 已删（`lib/infrastructure/acr/` 整树不存在）。
- `Dispatcher::decide` —— 全仓（排除 GOVERN-08）**零声明、零定义、零调用**。
- `register_phase2_acr_kernels` —— 全仓（排除 GOVERN-08）**零声明、零定义、零调用**；
  仅存于本行、`lib/algorithms/coverage/README.md:231`（已正确写"无 acr_kernels.h"）、
  `lib/algorithms/coverage/memory.md:82`（已正确写"acr：**已退场**"）与历史台账。
- 测试锚 `TEST-ACR-001` —— 全仓仅出现在本行自身，**无任何测试实体**。
- `:147-149` note「保留源码与隔离测试」——与已删事实直接冲突。

处置：**删整行 + 删 :147-149 的 note 块**。

### A3 ★高危 `docs/engineering/MODULE_MAP.md:147`（登记表第 29 条）

```
| 29 | `ACSD_ENABLE_ACR` | `cmake_variable` | `CMakeLists.txt:18` | … 行锚 = option() 定义行 :18。文档承载 = docs/engineering/ARCHITECTURE_OVERVIEW.md …
```

三重失效，逐项实测：

- `CMakeLists.txt` 中 `ACSD_ENABLE_ACR` 零命中。
- 行锚漂移：`sed -n '18p' CMakeLists.txt` 实际内容是
  `# QA-002: sanitizer 选项 (ASan/UBSan/LSan 或 TSan; 生产默认 OFF)`；
  真正的 `option(` 在 `:19/:20/:24`，且都是 `ACSD_ENABLE_SANITIZERS`/`ACSD_ENABLE_TSAN`/`ACSD_PROBES`，**没有 ACR**。
- 文档承载路径不存在：`ls docs/engineering/ARCHITECTURE_OVERVIEW.md` → 无此文件；
  `ls docs/owner/ARCHITECTURE_OVERVIEW.md` → 亦无此文件。

处置：**删整行**。这是"锚存活门应当判红"的一行。

### A4 `docs/engineering/UNRESOLVED_REGISTER.md`（35 处命中）

按主题分三类，逐条判读后：绝大多数为「ACR 组件退场」的未决登记，且以
`:2633` 「产品构建 0%（根 CMakeLists 从不 `add_subdirectory` ACR，`ACSD_ENABLE_ACR` 声明后从未被消费）」
这类**对已删开关的历史描述**为主。处置：随 ACR 一并清除相应条目，保留与 ACR 无关的其他条目。
（详细逐条归属见 §7 子代理复核附录。）

### A5 二级细节文档引用已删源码路径

- `docs/detail/registry/acsd.phase2.write.md:88` 列头文件集含 `acr_kernels.h`（已删）；
  `:160,166` 证据段引 `mosaic_reject_legacy`（已删）。
- `docs/detail/registry/acsd.phase2.integrate.md:160,191,194` 引 `acr_kernels.cpp`、ACR↔CPU 等价组。
- `docs/detail/registry/acsd.phase2.reject.md:164` 引 `acr_kernels.cpp`。

处置：改写为不含已删文件的现状描述。

### A6 `eng/cmake/install_layout.cmake:188`

`# missing_unit_file → 退出 5(ACR BACKEND)。` —— 构建注释里的退出码标签，ACR 已不存在。
处置：改写注释，去掉 ACR 标签（保留退出码 5 的说明）。**不影响构建行为。**

### A7 `eng/packaging/config/config_registry.json:2681`

`"reason": "全仓 grep：生产侧零 work-steal 实现（唯一命中 = lib/infrastructure/acr/**，而 ACR 生产不可达…）"`

该 `reason` 的事实前提已失效：现在**全仓零命中**，不再有"唯一命中"。
处置：改写 `reason` 句，去掉 `lib/infrastructure/acr/**` 指向。这是 `gaps[]` 的说明串，不是 ACR 配置键本身。

### A8 ★目录清单把已删子目录列为现行组件

`lib/README.md:15`：

> `- infrastructure/ —— 工程基建：cli/（…）、pipeline/（…）、scheduler/（…）、aio/（…）、benchmark/、observability/、gaia_xpsd_client/、**acr/**、hips_browser/。`

`ls -d lib/infrastructure/acr` → 不存在。这是仓内**唯一**把 `acr/` 列为 `lib/` 现存子目录的地方。
违反面：模块图。处置：从清单中摘除 `acr/`。

### A9 ★★同型缺口：上一轮清扫漏掉 rejection 车道（同车道同构却漏做）

上一轮 `700b5a10` 给 `lib/algorithms/integration/README.md:11,:65` 与 `integration/memory.md:31`
**已加**「曾并列…已随 ACR 子树退场删除」标记，但**同型**的三行至今**未加**，仍把已删文件
`lib/algorithms/coverage/src/acr_kernels.cpp` 写成**现行**合同消费者与**现行**并行轴：

| file:line | 原文摘 |
|---|---|
| `lib/algorithms/rejection/README.md:13` | 「消费链 …/tools/stage2.cpp … 与 …/src/acr_kernels.cpp（361 行，ACR 加速），均为本模块合同消费者」 |
| `lib/algorithms/rejection/README.md:85` | 「并行轴在调用方 stage2.cpp:1288 / acr_kernels.cpp:218 OMP」 |
| `lib/algorithms/rejection/memory.md:45` | 「acr_kernels.cpp（361 行）OMP :218/:228 schedule(static)」 |
| `lib/algorithms/rejection/module.yaml:27` | 「像素间并行在调用方：stage2.cpp:1288 OMP / **acr_kernels.cpp:218 OMP** schedule(static)」 |

对照：`lib/algorithms/integration/module.yaml:31-32` **已**写明「并行语义已随 ACR 退场移除：
原 `acr_kernels.cpp:218 OMP` 像素间并行面随 `383088f2` 删除该文件一并消失」。
⇒ 两个 `module.yaml` 自相矛盾。处置：按 integration 车道现有措辞补齐 rejection 车道四处。

### A10 `docs/engineering/RELEASE_STATUS.md:52,129`（死锚）

- `:52` 「ACR（CPU/GPU 异构）: DORMANT（不进生产构建/加载/路由/发布）」
- `:129` 「`| ACR | DORMANT | 根 CMakeLists.txt:17 ACR 默认 OFF；… |`」

`CMakeLists.txt` 对 ACR 零命中，且只有 **1443 行** ⇒ `:17`/`:1580` 双死锚。
处置：删分面行或改写为不含 ACR 的现状。

### A11 `docs/engineering/UNRESOLVED_REGISTER.md` 的 2 条**活**待裁项（非台账）

该文件 §0 自述为 append-only 台账（「不重新执行…不是指向现行文件树的指针」），
但 §31.4「其余待回一级项」是**活待裁项**，其中 2 条主体已失效：

| file:line | 条目 | 状态 |
|---|---|---|
| `UNRESOLVED_REGISTER.md:1044` | R9「infrastructure 17–23 编号去留（…故 acr **不接续编号**）」 | ACR 已删 ⇒ 登记项已 stale |
| `UNRESOLVED_REGISTER.md:1048` | R14「SCHEDULER_CONTRACT.md:29「跨后端等价无合同、判据面为空」…读者可能误以为 ACR↔CPU 等价有合同面」 | 指称仍有效，ACR 部分已失效 |

其余 33 条（`:1510,1511,1512,1641,1643,1647,1648,1652,1655,1664,1681,1682,1685,1688,1698,
1774,1780,1793,1798,1824,1853,1856,1970,2143,2144,2150,2152,2222,2464,2627,2633,3580,4006`）
属 append-only 台账 ⇒ **不动**。

处置：R9/R14 的关闭属**治理动作**，不应由本单静默删；**登记，交负责人裁决**。

### A12 文档值域错误（非 ACR 残留，但是同源缺陷）

`docs/engineering/PUBLIC_API.md:1126`：

```
| acr_route | "auto"（:95） | cpu/auto/cuda 族 | 集成执行路由 |
```

活 parser 只接受 `auto`/`cpu`（`stage2_common.cpp:630-632`：`if (…!= "auto" && …!= "cpu") { *err = "acr_route 只支持 auto/cpu"; return false; }`）。
`cuda` 族属**另一个字段** `execution_options.gpu_route`。⇒ 文档把两个不同字段的值域混写。
处置：订正值域为 `auto`/`cpu`。此项**不删行**（键仍在用），只订正。

### A13 陈旧 include 路径（实验脚本，可证明无害）

`实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/build.sh:13`：

```
     -Ilib/infrastructure/observability/probes/include
     -Ilib/infrastructure/acr/include -Ithird_party)
```

`ls -d lib/infrastructure/acr/include` → 不存在。

**判定为冗余而非断裂**（不靠"编译器会忽略"这类未验证推断）：该脚本 `:14-19` 实际只编译 4 个 TU
（`upm_sweep.cpp`、`lib/algorithms/coverage/src/upm.cpp`、`healpix_core.cpp`、`sha256.cpp`），
逐个核对其 `#include` **无一需要 ACR 头** ⇒ 删该 `-I` 不改变编译结果。

⚠️ 另有未证实项（不属本单）：该脚本疑似存在**与 ACR 无关**的既有断裂
（有评审称 `upm_sweep.cpp` 调用 `p2_upm_normalized_weights(...)` 而定义已删）。
本单**未编译、未独立证实**，仅登记，移交对应车道。

### A14 ⚠️ `v19r3_traceability.py` 今天运行必红（无人调用）

`:88-97` 的合同行 `ACR-IVAR-001`：

- `:92` impl 路径 `…/src/acr_kernels.cpp` —— **已删且全仓零跟踪**；
- `:93` 符号 `mosaic_reject_legacy` —— `git grep -c mosaic_reject_legacy -- lib` → **零命中**（仅存于文档与本文件自身）。

fail-closed 链完整：文件末尾实测为 `sys.exit(main())`（非裸 `main()`），
`main()` 末尾 `return 0 if not broken else 2` ⇒ **运行必得退出码 2、两条 broken**。

**但无人调用它**：全仓引用仅 `eng/tools/quality/README.md:20` 与 `docs/DOCUMENT_INDEX.yaml:303,343`
的元数据散文；`lib/phase1_session/memory.md:86` 写「禁止运行 v19r3_traceability.py」——那是会话约束，**非机器门**。

处置：**删该合同行**（或整表退役）。属决定问题：若该工具要重新启用为门，ACR 行必先删否则必红。

### A15 ⚠️ E5：`实验/reviews/g08_residency_config_traits_review.md` 整篇审的是已删 ACR

这不是"文档里带一个 ACR 词"，而是**整篇 280 行对抗式代码评审的审阅对象 100% 是 ACR**。
把它每个被引文件名在其自述 pinned HEAD `850a9ede` 上解析回原始路径，全部落在 `lib/infrastructure/acr/**`：

| 评审中的简称 | 原始路径 |
|---|---|
| `residency_manager.cpp` | `lib/infrastructure/acr/scheduler/residency_manager.cpp` |
| `task_descriptor.cpp` | `lib/infrastructure/acr/core/task_descriptor.cpp` |
| `dispatcher.cpp` | `lib/infrastructure/acr/scheduler/dispatcher.cpp` |
| `config_hot_read.cpp` | `lib/infrastructure/acr/utilization/config_hot_read.cpp` |
| `cost_estimator.cpp` | `lib/infrastructure/acr/cost/cost_estimator.cpp` |
| `acr_cuda_bridge_host.cpp` | `lib/infrastructure/acr/backends/cuda/bridge/…` |
| `weighted_integration_kernels.cpp` | `lib/infrastructure/acr/examples/weighted_integration/…` |

时序已证：`git merge-base --is-ancestor 850a9ede 383088f2` → **YES**
（评审 HEAD 早于 ACR 退场 `383088f2`）。当前树 `git ls-files | grep -iE 'residency|dispatcher|task_descriptor|cost_estimator'`
仅命中该 md 自身。

⇒ 该文 15 条裁决（含 `:231` 以 `acr_cuda_bridge_host.cpp:704-707` 支撑的 N2
「**静默错误结果，非性能问题**」）**没有一条适用于当前树**，却以**现行生产断言**口吻写成，
且置于 `实验/reviews/` 下看似当前工程证据。

**仓内已记录它系越权写入并请求删除**：
`run/GOVERN-08/审核包-R2/审稿-P1-INF-acr-004.md:6`「⚠️ 越权写：子代理 `8b4aa500` 在未授权情况下写了…
该文件不是本轮交付件，请负责人删除」，`:433` 再次要求删除。
`实验/reviews/` 至今**只有这一个文件、无 README**。

处置：**属发布权（AGENTS.md §11），交负责人裁决**（删 / 归档到 `engineering-evidence/`）。本单只登记。

### A16 `artifacts/evidence/governance-01/retire/RETIREMENT_LEDGER.md:49,85` 保留理由已失效

两条都写「仅 `tests/cli/test_iso_acr_gpu_isolation.py::test_04` 静态扫描其路径…
删除会改变测试输入面」。实测该测试**已不存在**
（`git ls-files | grep -iE 'iso_acr|acr_gpu'` → 零命中）。
⇒ 保留理由不再成立（该文件现在技术上可删）。**处置属发布权，超出本单，登记。**

---

## 4. 配置与合同面删除清单（★本次补做，前次遗漏）

### 4.1 `eng/contracts/data/contract_index.yaml` — 3 处

两条 ACR 合同条目**互相引用**且 `path` 指向**两个已删除的文档**，是确凿的悬空注册表条目。

已验证两文档均不存在：
```
docs/science/ACR_EQUIVALENCE.md            → MISSING
docs/science/algorithms/ACR_EQUIVALENCE.md → MISSING
```

| 位置 | 内容 | 处置 |
|---|---|---|
| `:19` | `SCI-SCOPE-001` 的 `downstream: […, SCI-ACR-EQUIV-001, …]` | 从列表中摘掉 `SCI-ACR-EQUIV-001` |
| `:124–131` | `id: SCI-ACR-EQUIV-001`，`status: DORMANT`，`path: docs/science/ACR_EQUIVALENCE.md`，`downstream: [ALG-ACR-EQUIV-001]` | 删整块 |
| `:631–637` | `id: ALG-ACR-EQUIV-001`，`status: DORMANT`，`path: docs/science/algorithms/ACR_EQUIVALENCE.md`，`upstream: [SCI-ACR-EQUIV-001]` | 删整块 |

安全性：`contracts` 数组共 116 项，其中 ACR 仅这 2 项（`[c['id'] for c in y['contracts'] if 'ACR' in c['id']]` 实测）。
`legacy_contract_id_map` **不含** ACR。三处同改则无残留交叉引用；若只删条目不改 `:19`，**会制造新的悬空下游引用**。

### 4.2 `eng/contracts/data/clause_registry.json` — 2 处

| 位置 | 内容 | 处置 |
|---|---|---|
| `:2727–2740` | `SUP-06`：`doc_id: SCI-ACR-EQUIV-001`，`file: docs/science/ACR_EQUIVALENCE.md`（已删），`section: §4 GPU 合同`，`lines: "33"` | 删整块 |
| `counts.superseded_sections` | 当前 `10`，数组实长 `10` | **同步改为 `9`** |

⚠️ 计数不变量：`superseded_sections` 数组实测长度 = `10`（SUP-01..SUP-10），
`counts.superseded_sections = 10`。删 SUP-06 而不同步计数即留下自相矛盾。

### 4.3 `eng/tools/quality/update_audit_status.py:51,70` — 活工具里的已删路径登记

```
:51  "lib/algorithms/shared": "B01", "lib/infrastructure/acr/api": "B14", "lib/infrastructure/acr": "B14",
:70  s in path for s in ("rejection", "integrate", "block", "acr_kernels")):
```

`lib/infrastructure/acr` 路径已不存在，这两条是**活工具中指向已删路径的注册项**。
处置：删 `:51` 两个 `"lib/infrastructure/acr…"` 映射；`:70` 子串表去掉 `"acr_kernels"`。

### 4.4 `eng/tools/quality/v19r3_traceability.py` — 悬空证据锚

- `:88–97` 追溯要求行 `ACR-IVAR-001`，`:92` 证据路径写
  `lib/algorithms/coverage/tools/stage2.cpp;lib/algorithms/coverage/src/acr_kernels.cpp`
  —— 后半段路径已删，**悬空锚**。
- `:96` `CPU_ACR_IVAR_EQUIVALENCE` 同属该行。
- `:378` ID 校验正则 `^(?:SCI|ENG|TEST|ACR|ALG|DATA|EXP|TAIL|INF|LIB)-…` 含 `ACR` 命名空间。
  仓内已无 `ACR-*` 合同 ID（§4.1 已删尽），此分支为残留。**可留（无害）亦可删**——建议留，避免动 ID 校验逻辑。

处置：`:88–97` 整行删除或改写为不指向已删路径；`:378` 见上。

### 4.5 配置字段：`acr_route`（**同名、活代码，本单不删**）

这是任务书点名的「同名但指向仍然活着的代码」。完整证据链：

| 侧 | `file:line` | 内容 |
|---|---|---|
| 结构体字段 | `lib/algorithms/coverage/include/astro/phase2/stage2_common.h:164` | `std::string acr_route = "auto";` |
| parser 读取 | `lib/algorithms/coverage/src/stage2_common.cpp:626` | 未知键白名单含 `"acr_route"` |
| parser 取值 | `lib/algorithms/coverage/src/stage2_common.cpp:629` | `cfg->acr_route = in.value("acr_route", std::string("auto"));` |
| parser 校验 | `lib/algorithms/coverage/src/stage2_common.cpp:630–631` | 只接受 `auto`/`cpu`，否则报错 |
| 消费点 | `lib/algorithms/coverage/tools/stage2.cpp:849` | `const std::string requested_route = cfg.acr_route;` |
| 配置样例 ×3 | `lib/algorithms/coverage/configs/stage2_gc_3panel_red.json:80`、`stage2_real_overlap_cpu.json:35`、`stage2_t4_true_overlap.json:35` | `"acr_route": "cpu"` |
| 生成方 ×3 | `controlled_rejection_metrics.py:147`、`controlled_rejection_truth.py:159`、`satellite_gate_build.py:202` | 写 `"acr_route": "cpu"` |
| 文档侧（**不得删**） | `docs/engineering/CONFIG_SCHEMA.md:58`（标注"**活键**"）、`PUBLIC_API.md:1126`、`DATA_SEMANTICS.md:1217`、`ARCH-001.md:155`、`docs/detail/registry/acsd.phase2.write.md:112`、`hips_p2/README.md:125`、`PHASE2_API_V1.md:43` | 描述该活键 |

处置：**代码侧与文档侧均保留**。删文档行会立即制造文档与代码不符。
**需前台裁决**：该键语义指向已删后端，是否另开一单做「配置键去 ACR 化改键名/删键」——那是代码改动，
且需编译验证，超出 T01（不跑编译）范围。本单只登记。

---

## 5. 同名但代码仍活着 —— 「代码仍在用但文档已不提/文档已提」不一致清单

| # | 标识 | 代码侧（活） | 文档侧 | 不一致性质 |
|---|---|---|---|---|
| 1 | `acr_route` | `stage2_common.h:164`；`stage2_common.cpp:626,629,630–631`；`stage2.cpp:849`；3 配置 + 3 工具 | `CONFIG_SCHEMA.md:58` 等 7 处 | 文档与代码**一致**（都还在），但二者都指向已删组件 → 需另单处置 |
| 2 | `p2_acr_block_eligible` | 声明 `stage2_common.h:195`；定义 `stage2_common.cpp:685`；调用 `stage2.cpp:968`（实参 `acr_registered=false` 硬编码） | `CONFIG_SCHEMA.md:144`（**声称 `register_phase2_acr_kernels` 在此实现，实为零命中**） | **文档超报**：文档所述实现不存在 |
| 3 | `use_acr_block` | `stage2.cpp:967,968,971`（写入 diagnostics） | 无直接文档行 | 诊断字段，代码在用；恒 false |
| 4 | `acr_workers` | `stage2.cpp:854`（定义）、`:974`（写日志）、`:1711` `diag["acr_workers"]` | `accelerator_fallback.h:23` 提 `acr_fallback_reason` 键面 | 诊断字段，代码在用 |
| 5 | `acr_fallback_reason` / `acr_requested_route` / `acr_effective_route` | `stage2.cpp:1709–1712` 逐字写入 diag | — | 诊断键，代码在用 |
| 6 | 调度器拒绝谓词 | `lib/infrastructure/scheduler/src/module.cpp:57–59`：`if (module_id.rfind("acsd.acr.",0)==0)` → 拒绝注册 | `lib/include/acsd/core/runtime.h:35`「ACR 不注册不链接」 | **门谓词仍在**：活代码，且**已编入出厂 `acsd`**（`CMakeLists.txt:417` → `acsd_core` → `acsd`）。ACR 子系统已整体删除 ⇒ 该分支永远不可达 |
| 7 | `M_ACR` | `lib/algorithms/coverage/include/astro/phase2/block.h:8,46`（内存预算公式注释与 `fixed_overhead` 注释） | 同 | 仅注释 |

**第 6 项值得前台注意**：`module.cpp:57-59` 是唯一一处**会对运行时行为真正生效**的 ACR 相关代码——
它拒绝注册 `acsd.acr.*` 前缀的模块。由于 ACR 子系统已整体删除，该分支**永远不可达**；
但删除它会改变运行时错误路径，属代码改动，需编译验证。按任务书「不跑编译」约束，本单只登记不删。

### 5.1 ⚠️ 冻结公开键：不得改名

`lib/algorithms/coverage/include/astro/phase2/accelerator_fallback.h:22-29` 逐字声明：

> `…acr_fallback_reason 键）逐字记录它们并交给下游消费，属公开键面的一部分`
> `（docs/engineering/PUBLIC_API.md 的 diagnostics.json 键集）。`
> `字符串内容不得改写、不得改名、不得合并——下游按字面比对。`

⇒ `acr_fallback_reason` 及同族 `acr_requested_route` / `acr_effective_route` / `acr_workers`
是**冻结公开键面**。任何"去 ACR 化改名"都属**破坏性公共合同变更**，不得作为清理顺手做。

### 5.2 ⚠️ `p2_acr_block_eligible` 已编入出厂二进制，但生产零调用

符号级枚举：声明 `stage2_common.h:195`、定义 `stage2_common.cpp:685`、唯一调用 `stage2.cpp:968`
（实参 `/*acr_registered=*/false` 硬编码；函数体四条 `(void)` + `return false` ⇒ **恒 false**）。

构建可达性：`stage2_common.cpp` 在 `CMakeLists.txt:682`，属 `acsd_phase2`；该库被
`CMakeLists.txt:1334-1335` `target_link_libraries(acsd PRIVATE … acsd_phase2 …)` 链入出厂 `acsd`。
但唯一调用点 `stage2.cpp` 属 `acsd-stage2`，该 target `EXCLUDE_FROM_ALL`
（`lib/algorithms/coverage/CMakeLists.txt:94,101`），且根 `CMakeLists.txt` 从不
`add_subdirectory` coverage、从不构建 `acsd-stage2`（`grep -n 'acsd-stage2' CMakeLists.txt` → 零命中）。

⇒ **出厂 `acsd` 仍带 ACR 命名符号，但该符号零生产调用者**（AGENTS.md §2.2「永远走不到的分支」形态）。
它被刻意保留为冻结守卫（`stage2.cpp:962-966` 注释言明保留理由：让「为何生产只剩 CPU canonical 路径」
这条判据在代码里仍可读、可查，而不是一句注释）。处置：**保留 + 登记**。

### 5.3 ⚠️ 陈旧注释：声称"不删 kernel"而 kernel 已删

`lib/algorithms/coverage/include/astro/phase2/stage2_common.h:193`：

```
// 保留函数与 ACR 接线（不删 kernel）：将来若重开 ACR，必须先按变更流程取得
```

`src/acr_kernels.cpp` **已被 `383088f2` 删除**（`lib/algorithms/coverage/memory.md:82` 已记「acr：**已退场**」）。
这是**活头文件里的陈旧注释**，与事实相反。处置：改写该行（纯注释，低风险）。

### 5.4 ⚠️ `build/acr/` 陈旧构建树仍在磁盘上

| 检查 | 结果 |
|---|---|
| `ls -d build/acr` | **存在** |
| `git -c core.quotepath=false ls-files build \| wc -l` | `0`（未受跟踪） |
| `git -c core.quotepath=false check-ignore -v build` | `.gitignore:20:build/`（被忽略） |

内含完整 ACR 目标图（`acr_test_*` 目标集、`lib/infrastructure/acr/**/CMakeLists.txt`、
`_deps/onetbb-build` 与 `libtbb.so`）。**它不是构建断点**（受跟踪图从不引用 `build/`），
但会使任何「ACR 已从本机消失」的断言**为假**。子代理未删除（遵守禁删规则）。
处置：**前台决定**是否清理该未跟踪目录；本单仅登记。

### 5.5 ⚠️ 删除 `acr_route` 的顺序陷阱（fail-closed）

`stage2_common.cpp:626` 是**未知键白名单**（`reject_unknown_keys(in,"integration.",{"precision",
"memory_limit_mb","rejection","acr_route"},err)`）。若只把 `"acr_route"` 从该白名单摘除、
而不同步删掉三份在仓配置
（`configs/stage2_gc_3panel_red.json:80`、`stage2_real_overlap_cpu.json:35`、`stage2_t4_true_overlap.json:35`
均含 `"acr_route": "cpu"`），
parser 会 **fail-closed 直接拒收这三份配置**（未知键即失败）。

正确退场顺序（若将来决定退它）：仿照同函数内已退役的 `weight_mode`（`stage2_common.cpp:599-606`）
与 `legacy_allow_weight_fallback`（`:611-621`）的**保留拒绝面**模式 ——
先加 fail-closed 拒绝，再从白名单摘除，再删三份 JSON，再删 header 字段与 `stage2.cpp` 消费点，
最后才改文档。**这是代码活，不是文档债。**

---

## 6. 构建面复核：无悬空、无孤儿目标

逐项枚举结果（**未跑构建**，全部为静态枚举 + 路径存在性交叉核对）：

| 检查项 | 命令 / 证据 | 结论 |
|---|---|---|
| ACR 源码树 | `ls lib/infrastructure/`；`git ls-tree -r HEAD -- lib/infrastructure/acr \| wc -l` → `0` | **已删** ✅ |
| ACR 构建目标 | 逐个打开全部 42 个 `CMakeLists.txt`/`*.cmake`/`Makefile`/`*.mk`/`CMakePresets.json`（排除第三方）grep | **零 ACR 目标** ✅ |
| 预设文件 | `grep -n -i acr CMakePresets.json` → 退出 1（零命中） | **干净** ✅ |
| 根构建 | `CMakeLists.txt` 对目标正则零命中 | **干净** ✅ |
| 悬空目标（定义存在但源已删） | 无任何 ACR 目标被定义 ⇒ 不存在悬空 ACR 目标 | **无悬空** ✅ |
| 孤儿目标（存在但无链接消费者） | 无 ACR 目标 ⇒ 无孤儿 ACR 目标 | **无孤儿** ✅ |
| 动态库 / `.so` / `dlopen` | `libacr`、`acrscheduler`、`dlopen` 对 ACR 零命中（排除 GOVERN-08 与历史台账） | **无残留** ✅ |

**唯一构建面命中**：`eng/cmake/install_layout.cmake:188` 的注释（见 §3-A6），非目标、非依赖，不影响构建。

⚠️ **前次判定被推翻**：文档 `docs/engineering/ARCH-001.md:153` 与 `docs/engineering/MODULE_MAP.md:147`
声称存在 `ACSD_ENABLE_ACR` 构建开关并给出行锚 `CMakeLists.txt:18`。实测该 option 与该行内容**均不存在**。
"代码子树已删 + 3 件树外专用接线已做" 的结论成立，但**文档侧的构建开关描述未被同步**。

---

## 7. 子代理分派与交叉复核

按任务书要求分派 **5 个只读子代理**分头清点（均禁止 git 写、禁止编译、禁止改文件）：

| # | 车道 | 覆盖面 | 状态 | 采纳要点 |
|---|---|---|---|---|
| 1 | 代码与构建面 | `lib/` `eng/` 根构建/预设，符号级枚举、悬空/孤儿目标 | ✅ 完成并复核 | 75 个 target 定义零 ACR；356 条路径字面量中 67 条未解析**全部**是变量根 `configure_file` 输出，**无一含 acr**；`build/acr/` 陈旧树；`p2_acr_block_eligible` 已入出厂二进制 |
| 2 | 配置与合同面 | CONFIG_SCHEMA/PUBLIC_API/ARCH-001/MODULE_MAP、`eng/contracts`、`eng/packaging`、module.yaml | ✅ 完成并复核 | 定位 `700b5a10` 为悬空条目成因；`acr_route` 白名单 fail-closed 顺序陷阱；schema 承诺"无悬空引用"但**无任何执行器** |
| 3 | 文档面 | `docs/**` + `lib/**/README.md` `memory.md` | ✅ 完成并复核 | 132(docs)+38(lib)=170 行逐行分桶；mermaid 零命中；S9 同型漏做缺口 |
| 4 | 实验与证据面 | `实验/` `artifacts/` `eng/tools/` `eng/tests/` | ✅ 完成并复核 | 393 命中（55 巧合 + 338 真）；**零 CI**（`.github/` 不存在）；零 ACR import；`build.sh:13` 冗余 `-I` 可证无害 |
| 5 | 子串巧合与第三方排除 | 第三方清点、巧合保护表、终检正则 | ✅ 完成并复核 | 第三方 343 行/32 文件整棵排除且**证明零 ACR**；136 个 token 全表穷举；**补集 446 行逐 token 拆解后零真 ACR** ⇒ 保护表完备性可证 |

（第 6 次派发与车道 1 重复，已中止，范围由车道 1 覆盖。）

### 7.0 各车道独立计数（互为交叉校验）

| 车道 | 口径 | 命中 | 自查更正 |
|---|---|---|---|
| 1 代码+构建 | `lib/ eng/` 排除第三方 | 194 | — |
| 2 配置+合同 | `docs/engineering`+`eng/contracts`+`eng/packaging`+`module.yaml`+`detail/registry`+`lib` 配置 | 81 | ⚠️ 曾口头报 83，作废；逐行重数为 **81**（B1=51 B2=19 B3=2 B4=9） |
| 3 文档 | `docs/` 132 + `lib` 文档车道 38 | 170 | ⚠️ 首次汇总报 149（与 170 矛盾），**自行重数更正**为 D1=96 D3=49 D4=18 D2=6 D5=1 |
| 4 实验+证据 | `实验/ artifacts/ eng/ .github/` | 393 | 55 巧合 + 338 真 ACR |
| 5 巧合保护 | 全仓 naive | 3561 行/267 文件 | 136 个去重 token，可穷举 |

⇒ 五个车道口径不同、结论互不冲突；本单 §1 的 996/565/251 是在**统一口径**（排除 GOVERN-08 与第三方）
下的收敛值，与车道 4/5 的 393/3561 因口径不同不矛盾（差异来源：是否含 `run/GOVERN-08`、是否用 naive 子串）。

### 7.1 文档车道逐行分桶（车道 3 自查更正后的计数）

车道 3 首次汇总曾报 `D1=76/D3=45/D4=19/D2=8/D5=1=149`，与实测 170 对不上；
**自行重数每一行后更正**为：

| 桶 | docs | lib 文档车道 | 合计 |
|---|---|---|---|
| **D1 活组件引用** | 80 | 16 | **96** |
| **D3 历史叙事** | 33 | 16 | **49** |
| **D4 同名/活代码仍在用** | 16 | 2 | **18** |
| **D2 子串巧合** | 2 | 4 | **6** |
| **D5 数据工件/合同引用** | 1 | 0 | **1** |
| 合计 | 132 | 38 | **170** ✓ |

（该更正由子代理主动提出，符合 AGENTS.md §2.3「数字必须来自实际运行」。）

### 7.2 被否决的既有判定 / 候选删除项

| 候选 | 原判风险 | 否决理由（实测） |
|---|---|---|
| `eng/tools/isa_feature_bits.py`（16 处命中） | 疑似枚举 ACR 站点的活工具 | **否决**。16 处**全部**是 `def _macro(name)` / `macro = …` 局部变量，纯巧合 |
| `eng/tools/quality/isa_sites.json`（11 处） | 疑似 ACR 站点登记 | **否决**。全部是 JSON 键 `"macro"` 与 `ACSD_*`/`ACS_CPU_*` ISA 特性宏名 |
| `lib/.../log_macros.h`（文件名含 acr） | 疑似需改名 | **否决**。`macros` 巧合，改名波及全部 `#include` |
| `lib/.../ipv_ransac.*` + `PROSACResult` | 疑似 ACR 符号 | **否决**。PROSAC 是 RANSAC 族算法名 |
| `lib/infrastructure/gaia_xpsd_client/*` `GaiaCreateDiagnostics` | 疑似 ACR 诊断字段 | **否决**。`Gaia`+`Create` 巧合 |
| 第三方 `cfitsio` / `nlohmann` / `nanoflann`（合计数百处） | 疑似漏清 | **否决**。目标正则下第三方命中数 = **0**；原计数全部来自 `macro`/`macros` autotools 样板与 Bison 生成文件 |
| `实验/engineering-evidence/v19r7-quality/file_audit_before.json`（221 处） | 疑似 ACR 组件清单 | **否决**。文件名 `_before_` + 内容为 `lib/acr/**` 旧路径清单 = 冻结的历史快照，非现行组件引用 |
| `docs/engineering/UNRESOLVED_REGISTER.md` 整条清空 | 粗暴批量删 | **否决**。该文件 35 处含 across/macro 巧合与与 ACR 无关条目，须逐条判读（§3-A4） |

### 7.3 我推翻的既有判定

1. **「构建开关 `ACSD_ENABLE_ACR` 仍在」** —— 推翻。实测在全部构建文件中零命中。
   （证据：`grep -rn 'ACSD_ENABLE_ACR\|ASTROCS_ENABLE_ACR\|ENABLE_ACR' CMakeLists.txt CMakePresets.json eng/cmake/` 退出 1）
2. **「`memory_acr_compare.py` 是非 ACR 的 compare 脚本巧合」** —— 推翻。读内容确认它真的比对 ACR 的
   CPU Sigma vs CUDA Sigma（`:5, :91, :92, :104, :112`），属 ACR 脚本，是删除候选而非巧合。
3. **「`MODULE_MAP.md:147` 的行锚 `CMakeLists.txt:18`」** —— 推翻。`CMakeLists.txt:18` 实为 sanitizer 注释行，
   该 option 从未存在于该位置。
4. **「配置与合同面已完成」** —— 推翻（这是任务书预判的缺口，已坐实）。
   `eng/contracts` 两份注册表共 **7 处** ACR 命中，其中 3 处是指向已删文档的**悬空注册条目**。
5. **「`G08-05-整改-退场ACR.md:28` 把 `memory_acr_compare.py` 定为『名字含 acr，非 ACR 树文件』」** ——
   推翻为**不充分**。它确实不在 ACR 树里，但 `:91-96` 构造 `report["acr"]` 做
   **CPU Sigma vs CUDA Sigma** 比较（即 ACR 的 CPU/GPU 路由比较），`:112-114` 据此判 PASS/FAIL。
   ⇒ 它是 ACR 产物。且 `:16` 硬编码 `F:\Astro dev\Astro CS Normalization Database`（**另一个仓名**），
   本仓跑不起来；全仓 20 处引用**全是治理记录散文**，零 CMake/CI/test 引用 ⇒ 直接删除安全。
6. **「`审稿-P1-ENG-tools-001.md:573` 称两个工具丢弃 `main()` 返回值 ⇒ fail-closed 不可达」** —— 推翻。
   实测 `v19r3_traceability.py` 末尾为 `sys.exit(main())`、`update_audit_status.py` 为
   `raise SystemExit(main())` ⇒ 退出码在进程边界**可达**。该工具今天运行会**返回 2**（见 §3-A14）。
7. **「`UNRESOLVED_REGISTER.md` 35 处里很多是 `across`/`macro` 巧合」** —— 推翻。实测 **35 行全部是真 ACR**
   历史审计条目，**零巧合**。其中 `:1856` 自带保护指令：`ACK-ACR-001..007` 是合法注册 ID，不得动。
8. **「`docs/ACSD_DESIGN.md` 仍以 §8 支撑 ACR DORMANT」** —— 推翻（利好）。实测 `grep -ic acr docs/ACSD_DESIGN.md` → **0**
   ⇒ 最高设计已清干净；`ARCH-001.md` / `DEPENDENCY_RULES.md` / `BUILD_GRAPH.md` / `EXECUTION_MODEL.md`
   里所有援引「最高设计 §8 / §1.4」支撑 ACR 的条文，**其上游依据已经不存在**。
9. **仓内自评的「163 份」口径** —— 推翻（见 `G08-05-整改-退场ACR.md:195` 自评为「口径误导」）。
   真实 naive 口径 = **3561 行 / 267 文件**；其中第三方 343 行、纯巧合 446 行。
   **不要再引用 163 这个数。**

---

## 8. 自证段（可复跑）

### 8.1 基线

```bash
cd "/workspace/Astro CS Database"
git log -1 --oneline                              # b4f37def ACR 文档退场：清掉仍带 ACR 的六处引用
git -c core.quotepath=false status --short | wc -l # 0
git tag -l | grep pre-acr-removal                  # pre-acr-removal（保留）
ls -d lib/infrastructure/acr 2>&1                  # 不存在
git -c core.quotepath=false ls-tree -r --name-only HEAD -- lib/infrastructure/acr | wc -l   # 0
```

### 8.2 四步清点复跑

```bash
# ① 原始子串（排除治理交付物）
git -c core.quotepath=false grep -inI 'acr' -- . ':(exclude)run/GOVERN-08' | wc -l        # 996

# ② 去巧合
git -c core.quotepath=false grep -inIP '(?i)(?<![a-z])acr(?![a-z])' -- . ':(exclude)run/GOVERN-08' | wc -l  # 565

# ③ 第三方（应为 0）
git -c core.quotepath=false grep -inIP '(?i)(?<![a-z])acr(?![a-z])' -- \
  lib/third_party lib/infrastructure/aio/third_party lib/infrastructure/aio/healpix_db/archive/legacy | wc -l  # 0

# ④ 活动面（待清零）
git -c core.quotepath=false grep -inIP '(?i)(?<![a-z])acr(?![a-z])' -- \
  lib eng docs ':(exclude)run/GOVERN-08' ':(exclude)lib/third_party' \
  ':(exclude)lib/infrastructure/aio/third_party' \
  ':(exclude)lib/infrastructure/aio/healpix_db/archive/legacy' | wc -l                    # 251
```

### 8.3 正则可信度自检（正例 / 反例）

终检采用**大小写敏感**正则（这是全部关键：`Acr` 靠大小写天然排除 `Gaia**Cr**eat`）：

```
PAT='(?<![A-Za-z0-9])(?:acr|ACR)(?![A-Za-z0-9])|(?<![A-Z_])Acr(?=[A-Z])'
```

- `(?<![A-Za-z0-9])` / `(?![A-Za-z0-9])`：把 `_` 当分隔符 ⇒ `acr_kernels` / `_ACR` /
  `p2_acr_block_eligible` / `ACR_EQUIVALENCE` **全命中**；
  `macro`（`acr` 前有 `m`）、`across`（`acr` 后有 `o`）、`PROSACResult`（`ACR` 前后皆字母）**全排除**。
- `(?<![A-Z_])Acr(?=[A-Z])`：CamelCase 段。**这条比纯 `(?i)` 版更强**——它额外命中
  `AcrCpuRouteStaysCpuNoSilentGpu`、`AcrCpuRouteEntersCpuAcrBlockForLegacyWeightMode`
  （纯大小写不敏感版会因 `acr` 后接字母 `C` 而漏掉这 2 处）。

实测对比（活动面 `lib/ eng/ docs/`，排除第三方）：
本单自建正则 **251** 行 vs 子代理正则 **253** 行 ⇒ 差 2 行正是上述 CamelCase 用例。

四棵受保护树**必须零命中**（实测 `rc=1`）：

```bash
git -c core.quotepath=false grep -IP -l "$PAT" -- \
  lib/third_party lib/infrastructure/aio/third_party \
  lib/infrastructure/aio/healpix_db/archive lib/algorithms/drizzle/healpix_drizzle \
  lib/algorithms/platesolve lib/infrastructure/gaia_xpsd_client \
  lib/algorithms/photometry 实验/healpix-polar
# → 无输出（干净）
```

已知盲点（如实登记）：`acr[a-z]` 连写形式（如 `acrscheduler`）**不被主正则覆盖**。
补跑 `git grep -nP 'acr[a-z]{3,}'` 后确认全仓只剩 `across`（多处）与**唯一 1 处真 ACR**
`acrscheduler`（`实验/engineering-evidence/v19r7-quality/audit_findings_standards.md:39`，
原文 `crate 侧 acrscheduler/dispatcher.cpp`）；另 `[Ll]ibacr` 全仓 1 处
（`实验/engineering-evidence/audit-2026-01/FIX_LEDGER.csv:261`）。**两处均在历史证据树，不在活动面。**

⚠️ **红线**：`docs/engineering/UNRESOLVED_REGISTER.md:1856` 逐字写着
「清扫须避开 `ACK-ACR-001..007` 与 ADR 引用等合法注册 ID」——这些是**合法注册 ID**，任何清扫不得动。

### 8.4 ★最终清零命令（前台执行完 §3/§4 清单后，期望输出 `0`）

```bash
cd "/workspace/Astro CS Database"
PAT='(?<![A-Za-z0-9])(?:acr|ACR)(?![A-Za-z0-9])|(?<![A-Z_])Acr(?=[A-Z])'

# 主门（活动面 + 构建面）——执行完清单后期望无输出
git -c core.quotepath=false grep -InP "$PAT" -- \
  lib eng docs CMakeLists.txt CMakePresets.json \
  ':(exclude)lib/third_party' \
  ':(exclude)lib/infrastructure/aio/third_party' \
  ':(exclude)lib/infrastructure/aio/healpix_db/archive' \
  ':(exclude)lib/algorithms/drizzle/healpix_drizzle'
# 当前：253 行；执行完本单清单后期望：0

# 补门 1：acr+小写连写（正则无法与 across 区分，须人工过目）
git -c core.quotepath=false grep -InP 'acr[a-z]{3,}' -- . \
  ':(exclude)run/GOVERN-08' | grep -v 'across'

# 补门 2：libacr* 安装目标
git -c core.quotepath=false grep -InP '[Ll]ibacr' -- . ':(exclude)run/GOVERN-08'

# 受保护树自检——必须无输出
git -c core.quotepath=false grep -IP -l "$PAT" -- \
  lib/third_party lib/infrastructure/aio/third_party \
  lib/infrastructure/aio/healpix_db/archive lib/algorithms/drizzle/healpix_drizzle \
  lib/algorithms/platesolve lib/infrastructure/gaia_xpsd_client \
  lib/algorithms/photometry 实验/healpix-polar
```

配套的**合同文件语法校验**（删条目后必跑，防悬空/破语法）：

```bash
python3 -c "
import json, yaml
y = yaml.safe_load(open('eng/contracts/data/contract_index.yaml', encoding='utf-8'))
assert not [c['id'] for c in y['contracts'] if 'ACR' in c['id']], '合同索引仍有 ACR 条目'
assert 'SCI-ACR-EQUIV-001' not in json.dumps(y, ensure_ascii=False), '仍有 ACR 交叉引用'
j = json.load(open('eng/contracts/data/clause_registry.json', encoding='utf-8'))
assert j['counts']['superseded_sections'] == len(j['superseded_sections']), '计数不变量被破坏'
assert not [s for s in j['superseded_sections'] if 'ACR' in json.dumps(s, ensure_ascii=False)], 'SUP-06 未删'
print('CONTRACTS OK')"
```

⚠️ 该脚本**已实测可运行**（语法正确），当前会如预期地失败并给出失败项
（实测：`ASSERT1` 报 `['SCI-ACR-EQUIV-001','ALG-ACR-EQUIV-001']`、`ASSERT2` 为 `True`、
`ASSERT3` 报 `['SUP-06']`；计数不变量当前**成立**（`10 == 10`））。

⚠️ **无自动校验器**：`eng/contracts/schemas/contract_index.schema.json` 的 `description`
自称保证「无悬空引用」，但 `git grep contract_index -- eng/tools eng/tests` **零命中**
⇒ 该承诺**无任何执行器**，悬空只能靠上面的手工脚本发现。

### 8.5 未执行事项（如实登记）

- **未跑编译、构建、测试**（遵守任务书与 AGENTS.md §9）。§5 中"活代码"判定基于符号级枚举与路径存在性，
  非运行期证据。
- **未做任何 git 写操作**；未 add/commit/checkout/reset/stash/tag；`pre-acr-removal` 未动。
- **未改动任何被审文件**；本单唯一写入为本交付件。所有删除/改写待前台执行。
- 子代理结论已逐条复核并采纳/否决，见 §7；**§3-A4（`UNRESOLVED_REGISTER.md` 35 处逐条归属）**为唯一
  需前台逐条判读后落刀之处，其余各项处置已可直接执行。