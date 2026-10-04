# T01b ACR 清理执行 — 交付件

依据：`run/GOVERN-08/审核包-R2/T01-ACR彻底删除.md`（710 行，前车道清点与定性）
基线 HEAD：`b4f37def`（"ACR 文档退场：清掉仍带 ACR 的六处引用"）
状态：**READY_FOR_OWNER_REVIEW**（未 git 写、未编译、未跑测试、未跑构建）

---

## 摘要

| 项 | 结果 |
|---|---|
| 甲档删了几处 | **20 个文件**，覆盖 T01 §3 的 A1/A2/A3/A5/A6/A7/A8/A9/A10/A12/A14 + §4.1 + §4.3 + §4.4 + §2.1 删脚本 |
| 丙档顺序对不对 | **对**。先删 3 份配置 JSON + 3 个生成方并验证，再动 parser；且顺序**优于** T01 §5.5 的建议（见 §7.2） |
| 计数不变量 | **实测三个数一致**：`counts=9` / 顶层数组 `len=9` / 镜像数组 `len=9` |
| 索引解析器验证 | **通过**。`yaml.safe_load` 解析 OK；178 条 `path` 全存在，**零悬空** |
| 清零命令 | **253 → 210 行（−43）**；受保护树仍零命中；补门 1/2 残留不变 |
| 残留 | 210 行，**逐条理由见 §6.2**；其中 **10 行是本单新增的「已删除」退场标记**，非缺陷 |
| 是否留下悬空引用 | **本单未引入任何新悬空**（证据 §2.3）；本单造成的 4 处悬空已由前台当场修掉（§5.4） |

改动总量：`29 files changed, 80 insertions(+), 240 deletions(-)`，另删除 1 文件（124 行）。

---

## 1. 分派与复核

按任务书派发 **5 个执行子代理**，文件集两两不相交（开工前逐一比对，防撞车）：

| 车道 | 覆盖面 | 结果 |
|---|---|---|
| L1 | `eng/contracts/data/{contract_index.yaml,clause_registry.json}` | ✅ 4/4 assert 通过 |
| L2 | 3 份在仓配置 + 3 个生成方 + `stage2_common.cpp`（丙档） | ✅ 4 项自检全绿 |
| L3 | `docs/engineering/{ARCH-001,CONFIG_SCHEMA,MODULE_MAP,RELEASE_STATUS,PUBLIC_API}.md` | ✅ 表格/顺延核查通过 |
| L4 | 工具/构建/配置/模块文档 + 删 `memory_acr_compare.py` | ✅ 4 项自检全绿 |
| L5 | `docs/detail/registry/*.md` + 机器索引校验 | ✅ 索引 178/178 零悬空 |

**前台独立复核（非子代理结论，全部重跑）**：§2 乙档核对、§3 计数不变量、§4 索引、§5 顺序与悬空、§6 清零命令。

**撞车处置**：L1 契约车道被误派发两次，`interrupt_agent` 在其写入前中止第二个（`58f3e9b1`），`git status` 验证零写入痕迹，保留首个（`92f2fae4`）。

**子代理否决项**（详见 §7.3）：`clause_registry.json:3815/3939` 两处历史沿革串不删；`v19r3_traceability.py:368` ID 校验正则保留；`update_audit_status.py` 不扩大范围；rejection 四处保留 `acr_kernels` 作已删历史指称；`memory_acr_compare.py` 不走「摘 ACR 分支再改名」路线。

---

## 2. 乙档确认未动 —— 逐项核对结果

### 2.1 两条「删则编译失败」红线的认定

任务书点名的两条，**均不在 T01 §3/§4 的清单里**，任务书未指名。实测认定如下：

| 红线 | 认定 | 性质证据 | 删则后果 |
|---|---|---|---|
| **符号名** | `p2_acr_block_eligible` | 声明 `lib/algorithms/coverage/include/astro/phase2/stage2_common.h:195`；定义 `src/stage2_common.cpp:700`；调用 `tools/stage2.cpp:968`。是**真实函数符号**，三处同现 | 删 `acr` 段 ⇒ 声明/定义/调用三方不匹配 ⇒ **编译失败** |
| **诊断类型名** | `GaiaCreateDiagnostics` | `lib/infrastructure/gaia_xpsd_client/src/gaia_client.h:190` 的 `} GaiaCreateDiagnostics;` —— 是 **typedef 后的 struct 类型名**；用作变量类型 `gaia_client.c:77`、参数 `:87`、出参 `:198` | 删 `aCr` 段 ⇒ 类型名与其三处用法不匹配 ⇒ **编译失败** |

补充实测：`GaiaCreateDiagnostics` 被终检正则 `(?<![A-Za-z0-9])(?:acr\|ACR)(?![A-Za-z0-9])\|(?<![A-Z_])Acr(?=[A-Z])` **零命中**（`printf 'GaiaCreateDiagnostics' | grep -P "$PAT"` ⇒ rc=1），因为它的大小写是 `aCr`（小写 a）。它落在**子串巧合 431 行**那个乙档桶里。

> ⚠️ 终检正则**抓不到**这条红线。任何以正则计数为门禁的验收都不会替我们守住它 —— 只能靠类型定义本身守住。已单列。

### 2.2 乙档 13 项 `git diff` 核对（应全为 0）

```
0 处改动  <- lib/infrastructure/gaia_xpsd_client          （诊断类型名，红线）
0 处改动  <- lib/algorithms/coverage/tools/stage2.cpp     （16 行 ACR 命中全未动）
0 处改动  <- lib/algorithms/coverage/include              （8+2+2+1 行全未动）
0 处改动  <- lib/infrastructure/scheduler/src/module.cpp   （:57-59 拒绝谓词）
0 处改动  <- lib/include/acsd/core/runtime.h              （:35 ACR 不注册不链接）
0 处改动  <- lib/algorithms/integration                    （4 行先例车道，保留）
0 处改动  <- docs/engineering/UNRESOLVED_REGISTER.md      （35 行 append-only 台账）
0 处改动  <- 实验                                          （历史证据 314 行）
0 处改动  <- artifacts                                      （历史证据）
```

**冻结公开键面**（`accelerator_fallback.h:22-29` 逐字声明「不得改写、不得改名、不得合并」）零改动：
`acr_requested_route` / `acr_effective_route` / `acr_workers` / `acr_fallback_reason` 四键及其在 `stage2.cpp:1709-1712` 的写入，一字未动。

**M_ACR**（`block.h:8,46`，内存预算公式注释）未动。

### 2.3 「本单未引入新悬空」的证明

本单新增行里**凡含** `acr_kernels` / `mosaic_reject_legacy` 的，逐条检查均带删除标记：

```
+`acr_kernels.cpp` 已随 ACR 子树退场删除（`383088f2`），该像素间并行面不再存在。
+ACR↔CPU 等价组（ACR 侧随 `acr_kernels.cpp` 一并退场，`383088f2`），
+schedule(static)；rejection.cpp 无任何线程原语）。原并列调用方 acr_kernels.cpp
+并列的 acr_kernels.h 已随 ACR 子树退场删除（`383088f2`），该冻结接口成员不再存在。
-ACR 子树已退场（`383088f2`）：其仅有的 CPU launcher `mosaic_reject_legacy`
+  （**无 CUDA kernel**）随 `lib/algorithms/coverage/src/acr_kernels.cpp` 一并删除，
+> 曾并列的 `lib/algorithms/coverage/src/acr_kernels.cpp` 已随 ACR 子树退场删除
```

而活动面里**仍**把已删文件当现行组件陈述的残留，**全部在 HEAD 上就已存在**（本单未引入）：

```
HEAD 中 docs/engineering/PUBLIC_API.md            含 acr_kernels.cpp 行数 = 3
HEAD 中 docs/science/DATA_SEMANTICS.md            含 acr_kernels.cpp 行数 = 4
HEAD 中 docs/science/algorithms/PHASE2_INTEGRATION.md 含 acr_kernels.cpp 行数 = 2
HEAD 中 docs/science/algorithms/PHASE2_REJECTION.md    含 acr_kernels.cpp 行数 = 2
HEAD 中 lib/infrastructure/scheduler/src/module_adapters.cpp 含 acr_kernels.cpp 行数 = 1
```

⇒ 这些是**前车遗留**，登记入 §6.2 残留表，需另派单，**不在 T01 §3/§4 清单内**。

---

## 3. 计数不变量实测（三个数一致）

删 `SUP-06` 若不同步计数，注册表自相矛盾。真解析器实测：

```
CONTRACTS OK  (4/4 assert passed)
contracts 总数 = 114
counts=9  顶层数组len=9  镜像数组len=9  三者一致=True
```

| 项 | 改前 | 改后 |
|---|---|---|
| `counts.superseded_sections` | 10 | **9** |
| `len(j['superseded_sections'])` | 10 | **9** |
| `len(j['clause_registry']['superseded_sections'])`（镜像） | 10 | **9** |
| `contracts` 数组总数 | 116 | **114**（恰好 −2，与删 2 个 ACR 条目吻合） |

⚠️ **第三个数是本单新增的不变量，T01 与前台派单都只提到前两个。**
`clause_registry` 子对象是顶层表的**精确副本索引**（实测制式：`镜像 signoff_items == 顶层`、`total == len(clauses)` 均为 True）。只删顶层不同步镜像，会在同一文件内留下 `counts=9` vs `镜像=10` 的打架。
它逃过一切按 ACR 字符串扫描的审计，**因为 `"SUP-06"` 这一行不含 "ACR" 子串**。L1 发现后按派单边界未擅改，**前台当场补改**（`clause_registry.json:3346`）。

自检脚本（T01 §8.4 原样，4 条 assert 全过）：

```python
y = yaml.safe_load(open('eng/contracts/data/contract_index.yaml', encoding='utf-8'))
assert not [c['id'] for c in y['contracts'] if 'ACR' in c['id']]
assert 'SCI-ACR-EQUIV-001' not in json.dumps(y, ensure_ascii=False)
j = json.load(open('eng/contracts/data/clause_registry.json', encoding='utf-8'))
assert j['counts']['superseded_sections'] == len(j['superseded_sections'])
assert not [s for s in j['superseded_sections'] if 'ACR' in json.dumps(s, ensure_ascii=False)]
```

附加：`contracts` 全 114 条的 `upstream`/`downstream` 全量扫描 ⇒ 悬空引用 `[]`、重复 id `[]`、含 ACR 引用 `[]`。

---

## 4. 索引解析器验证结果

⚠️ 本项目出过错：用文本替换破坏索引结构导致解析失败。**改前改后各跑一次真解析器**（`yaml.safe_load`，PyYAML 6.0.2）。

| | 改前（前台基线） | 改后 |
|---|---|---|
| 解析 | `BASELINE PARSE OK` | **`DOCUMENT_INDEX.yaml PARSE OK`** |
| 顶层键 | `['doc_index']` | `['doc_index']` |
| `path` 字段总数 | 178 | 178 |
| 不存在的 `path` | **0** | **0** |

**是否需要同步索引：不需要，且已证明非假设。**
本单**不删除任何 `.md` 文档**（唯一删除项是 `memory_acr_compare.py`，已实测索引零命中），三个 detail/registry `.md` 只做就地改写、`path` 值不变。⇒ `docs/DOCUMENT_INDEX.yaml` **零改动**。

> 方法论留档：L5 第一版脚本把 `upstream`/`downstream` 也当路径校验，误报 **311 条「悬空」**，L5 自行判定为假阳性并主动撤回 —— 那些字段是散文引用列表（含「等 N 处」尾缀与 `§章节` 锚）。**该 311 不可采信**，口径应收敛到索引自己承诺的 `path:` 字段。

---

## 5. 丙档 `acr_route` 退场 —— 执行顺序证据

### 5.1 顺序机制

`stage2_common.cpp` 的 `reject_unknown_keys(in,"integration.",{...})` 是**未知键白名单**：白名单外的键出现即 parser **fail-closed 拒收整份配置**。仓内**已有先例** —— 同函数内已退役的 `integration.weight_mode`（:599-606）与 `integration.legacy_allow_weight_fallback`（:611-621）。其模式是：注释块 + `if (in.contains(k)) { *err = "...已删除…请删除该键。"; return false; }`，且**拒绝面必须在未知键门之前**（源码注释原文：「位置在两道退役键拒绝面之后」）。

### 5.2 顺序证据（硬证据）

**第一步（配置侧，6 文件）全部落地并验证通过后，才第一次写 `stage2_common.cpp`（第 7 个文件）。**
L2 在第一步验证时打的快照：

```
 configs/stage2_gc_3panel_red.json      |  3 +--
 configs/stage2_real_overlap_cpu.json   |  3 +--
 configs/stage2_t4_true_overlap.json    |  3 +--
 tools/controlled_rejection_metrics.py  |  2 +-
 tools/controlled_rejection_truth.py    |  2 +-
 tools/satellite_gate_build.py          |  1 -
```

—— 列表里**没有 `stage2_common.cpp`**。parser 是在配置侧 100% 落地并验证之后才被改的。

### 5.3 终态位置核验

```
600:  *err = "integration.weight_mode 已删除：…
612:  *err = "integration.legacy_allow_weight_fallback 已删除："
630:  *err = "integration.acr_route 已删除："          ← 新增拒绝面
637:  // integration 面的未知键门。位置在三道退役键拒绝面**之后**
639:  if (!reject_unknown_keys(in, "integration.",
640:                           {"precision", "memory_limit_mb", "rejection"},
641:                           err)) {
```

**白名单 = `{precision, memory_limit_mb, rejection}`，`acr_route` 已摘除。** 拒绝面在门之前，与两处先例同侧。
连带修正：未知键门注释「**两道**」→「**三道**」（加第三道后原注释即成假陈述）。

拒绝面新增内容逐字：

```cpp
// integration.acr_route **已删除** ——
// 该键曾在 auto/cpu 之间选择「集成执行路由」，把一个已整体退场的加速后端
// 重新接回配置面；生产计算后端恒为纯 CPU。
//   · docs/ACSD_DESIGN.md §1.3（非目标）首条：「GPU 与 CPU/GPU 混合生产路由」；
//   · docs/ACSD_DESIGN.md §9（CPU 后端与资源）首条：「生产仅纯 CPU」。
// ⇒ 该键**既不能被设、也不能被读**：出现即 fail-closed 拒绝（退役对象的
//   拒绝面必须存活，不得静默忽略或静默取默认值）。
if (in.contains("acr_route")) {
    *err = "integration.acr_route 已删除："
           "该键只用于在 auto/cpu 之间选择集成执行路由，"
           "而生产计算后端恒为纯 CPU（docs/ACSD_DESIGN.md §9（CPU 后端与资源）："
           "生产仅纯 CPU）；GPU 与 CPU/GPU 混合生产路由属非目标"
           "（docs/ACSD_DESIGN.md §1.3（非目标））。请删除该键。";
    return false;
}
```

引用的两条规范条款**已逐字核对原文**（`docs/ACSD_DESIGN.md:79-81` §1.3 非目标首条、`:529` §9 首条），不是凭节号推断。

### 5.4 顺序的执行效果（本单造成的 4 处文档漂移，已当场修掉）

丙档落地后，4 处仍把该键当活字段登记的文档与代码不符。**前台全部当场修掉**（不改即违反「不得留下悬空引用」）：

| 文件:行 | 改后 |
|---|---|
| `docs/science/DATA_SEMANTICS.md:1217` | 改为 `\| acr_route（键不存在） \| — \| — \| **禁用键**：…配置中出现该键 ⇒ 具名 fail-closed 拒绝（§31.8） \|` —— 照抄同表 `:1215-1216` 两行既有退役键的「禁用键」体例 |
| `docs/science/DATA_SEMANTICS.md:1360` | 「浮点积分确定性受 ~~acr_route/~~execution 预算…控制」 |
| `docs/detail/registry/acsd.phase2.write.md:113` | 删去 `acr_route=auto、` |
| `lib/algorithms/coverage/hips_p2/README.md:123,125` | `:123` 退役键清单加 `acr_route`；`:125` 删去 `acr_route(auto)、` |

另有 L3 的 ARCH-001 顺延造成 1 处悬空，同样当场修掉：
`docs/engineering/BUILD_GRAPH.md:3`「ARCH-001.md §8 不变量 1」→ **§7**（§8 顺延后已变成「平台与发布形态」，「不变量」已顺延为 §7）。

### 5.5 未做（登记为后续代码债）

- **未删** `stage2_common.h:164` 的 `std::string acr_route = "auto";` 字段 —— 仍被 `stage2.cpp:849` 消费并写入冻结公开键 `acr_requested_route`；改字段需编译验证。
- **未删** `stage2_common.cpp:644-648` 的 `cfg->acr_route = in.value(...)` + auto/cpu 校验 —— 加了拒绝面后该键永不出现、恒取默认 `"auto"`，**永不可达但无害**。
  ⚠️ L3 提出异议：这是「以为能关其实不生效的第二层假承诺」。已核实括号平衡（HEAD braces 净差 0 / NOW braces 净差 0；本次编辑净贡献 braces 0、parens 0；`parens=1` 在 HEAD 上就已存在），**不会编译失败**。判为**代码债**，随「删字段 + 删消费点」一并处理，需前台跑编译。
- **未动** `tools/stage2.cpp`、冻结诊断键面（4 键）。

---

## 6. 清零命令：改前 / 改后

命令取自 T01 §8.4，口径未改。

### 6.1 改前 / 改后

```bash
cd "/workspace/Astro CS Database"
PAT='(?<![A-Za-z0-9])(?:acr|ACR)(?![A-Za-z0-9])|(?<![A-Z_])Acr(?=[A-Z])'

# 主门（活动面 + 构建面）
git -c core.quotepath=false grep -InP "$PAT" -- \
  lib eng docs CMakeLists.txt CMakePresets.json \
  ':(exclude)lib/third_party' \
  ':(exclude)lib/infrastructure/aio/third_party' \
  ':(exclude)lib/infrastructure/aio/healpix_db/archive' \
  ':(exclude)lib/algorithms/drizzle/healpix_drizzle' | wc -l
```

| 门 | 改前 | 改后 |
|---|---|---|
| **主门** | **253** | **210**（−43） |
| 受保护树（7 棵，须无输出） | rc=1 干净 | **rc=1 干净** |
| 补门 1（`acr[a-z]{3,}` 排除 across） | 1 | **1**（不变，`实验/` 历史证据） |
| 补门 2（`[Ll]ibacr`） | 1 | **1**（不变，`实验/` 历史证据） |

逐文件净变化：

```
lib/README.md                                          1 ->   0   (-1)
docs/engineering/MODULE_MAP.md                         1 ->   0   (-1)
eng/cmake/install_layout.cmake                         1 ->   0   (-1)
eng/packaging/config/config_registry.json              1 ->   0   (-1)
lib/algorithms/coverage/tools/satellite_gate_build.py  1 ->   0   (-1)
lib/algorithms/coverage/configs/stage2_gc_3panel_red.json    1 -> 0 (-1)
lib/algorithms/coverage/configs/stage2_t4_true_overlap.json  1 -> 0 (-1)
lib/algorithms/coverage/tools/controlled_rejection_truth.py  1 -> 0 (-1)
lib/algorithms/coverage/configs/stage2_real_overlap_cpu.json 1 -> 0 (-1)
lib/algorithms/coverage/tools/controlled_rejection_metrics.py 1 -> 0 (-1)
docs/science/DATA_SEMANTICS.md                       10 ->   9   (-1)
docs/engineering/RELEASE_STATUS.md                    2 ->   0   (-2)
eng/tools/quality/update_audit_status.py               2 ->   0   (-2)
docs/engineering/CONFIG_SCHEMA.md                      4 ->   1   (-3)
eng/contracts/data/clause_registry.json                5 ->   2   (-3)
eng/tools/quality/v19r3_traceability.py               6 ->   1   (-5)
eng/contracts/data/contract_index.yaml                7 ->   0   (-7)
docs/engineering/ARCH-001.md                           7 ->   1   (-6)
lib/algorithms/coverage/tools/memory_acr_compare.py    8 ->   0   (-8, 文件已删)
lib/algorithms/coverage/src/stage2_common.cpp        13 ->  15   (+2, = 新增拒绝面)
docs/detail/registry/acsd.phase2.reject.md             1 ->   2   (+1, = 退场标记)
docs/detail/registry/acsd.phase2.write.md              8 ->   9   (+1, = 退场标记)
```

三个文件 **11 → 11 / 7 → 7 / 3 → 3 不变**：`PUBLIC_API.md`（`:1126` 改写后仍含该键名）、`hips_p2/README.md`（`:125` 的键移进 `:123` 退役清单）、`acsd.phase2.integrate.md`。

### 6.2 残留 210 行 —— 逐条理由

**没有任何一行是默默留着的。** 残留分五类：

**① 乙档 · 历史台账 35 行** —— `docs/engineering/UNRESOLVED_REGISTER.md`
该文件 §0 自述 append-only 台账（「不重新执行…不是指向现行文件的指针」）。T01 §8.5 称它是「唯一需前台逐条判读后落刀之处」，但同一份 T01 的 §3-A11 明写：`:1044`(R9)、`:1048`(R14) 两条属**治理动作**须交负责人裁决，其余 **33 条属 append-only 台账 ⇒ 不动**。
⚠️ 且 `:1856` 自带红线逐字写着「清扫须避开 `ACK-ACR-001..007` 与 ADR 引用等**合法注册 ID**」。
**判定：全 35 行不动**（前台裁定「历史证据（314 行）」同族）。

**② 乙档 · 同名但代码仍活着 / 冻结键面 54 行** ——
`lib/algorithms/coverage/tools/stage2.cpp`(16)、`stage2_common.cpp`(15)、`stage2_common.h`(8)、`block.h`(2)、`rejection.h`(2)、`integrate.h`(1)、`accelerator_fallback.h`(1)、`module.cpp`(3)、`module_adapters.cpp`(1)、`runtime.h`(1)、`coverage/memory.md`(10)。
含**两条红线**（§2.1）、**四个冻结公开诊断键**（§5.1 逐字禁止改名）、**调度器拒绝谓词** `module.cpp:57-59`（活代码，删除会改变运行时错误路径，需编译验证）、**`M_ACR`**（注释）。
**判定：不动。** 本单 §5.5 已把这部分登记为代码债。

**③ 乙档 · integration/rejection/coverage 先例车道的「已删除」措辞 13 行** ——
`rejection/{README.md(2),memory.md(1),module.yaml(1)}`、`integration/{README.md(2),memory.md(1),module.yaml(1)}`、`coverage/README.md(2)`、`detail/registry/{integrate(3),reject(2),write(9)}`。
仓内既有做法即**保留已删文件名作历史指称**（`integration` 车道至今 4 行命中即此形态）。A9 的目的就是让两个车道**措辞对等** —— 实测 rejection 现 4 条 vs integration 现 4 条，**1:1 对齐，自相矛盾已消除**。
**判定：不动，这是仓内既定体例，不是残留缺陷。**

**④ 丙档的「拒绝面必须存活」4 行** ——
`stage2_common.cpp:622,629,630,646`、`CONFIG_SCHEMA.md:58`、`PUBLIC_API.md:1126`。
先例源码注释原文：「**退役对象的拒绝面必须存活，不得静默忽略或静默取默认值**」。删掉拒绝面 = 让拼错的键静默取默认值。
**判定：不动，这是设计要求的资产。** 同理 `DATA_SEMANTICS.md:1217` 的「禁用键」行。

**⑤ 前车遗留的活组件陈述（需另派单）26 行** ——
| 文件 | 行数 | 内容 |
|---|---|---|
| `docs/science/algorithms/PHASE2_MOSAIC_WRITE.md` | 19 | ACR 块路径/CUDA bridge/route 的完整推导叙述 |
| `docs/engineering/PUBLIC_API.md` | 11 | `:41,:76,:1162-1164,:1241,:1248,:1253,:1265,:1367` |
| `docs/science/DATA_SEMANTICS.md` | 9 | `:1406,:1424,:1457,:1482,:1483,:1655` 等 |
| `docs/science/algorithms/PHASE2_INTEGRATION.md` | 7 | `:165,:172,:178,:225,:289,:335,:336` |
| `docs/science/algorithms/PHASE2_REJECTION.md` | 5 | `:462,:597,:735,:775,:799` |
| `docs/engineering/{EXECUTION_MODEL(3),DEPENDENCY_RULES(2),execution_options_contract(2),04_ARTIFACTS(1),PHASE2_API_V1(1),TRACEABILITY_SPEC(1),PROJECT_SPEC(1),RELEASE_STANDARD(1),RT-001(1),TOOLCHAIN_AGENT_HOST(1),VERSIONING(1),CONTROL_WEIGHT_SNR(1)}` | 16 | 多为「生产面不含 ACR/CUDA」的正确正面陈述 |
| `eng/contracts/data/clause_registry.json` | 2 | `:3815,:3939` 历史沿革串（`migration_map`/`reanchor_map` 段） |
| `eng/cmake/ARCH-001-migration-manifest.md` | 2 | 迁移清单，`32 \| lib/acr | … | DONE` 是**已完成迁移的留痕** |
| `eng/tools/quality/v19r3_traceability.py` | 1 | `:368` ID 校验正则，T01 §4.4 明确「建议留」 |

**为什么不动**：T01 §3 只逐条判定了 A1–A16（其中 A15/A16/A11 本身就标为「交负责人裁决」），§4 只判定了 5 组。前台甲档定义是「**清单里判为「属于 ACR 组件」的文档改动**」。上表 26 行**不在清单里**，且大部分落在 `docs/science/` 一级正本与「生产面不含 ACR」的正面陈述上 —— 机械改写属越权。
⚠️ 其中 **11 行（`PUBLIC_API.md` 4 + `DATA_SEMANTICS.md` 4 + `PHASE2_*` 3 等）确实仍把 `acr_kernels.cpp` 当现行组件陈述**，是**真悬空**。已在 §2.3 证明它们**在 HEAD 上就存在**，非本单引入。**建议另派「科学正本 ACR 段退场」单。**

---

## 7. 推翻的前车判定

### 7.1 T01 §4.1 行锚漂移（实锤）
T01 写 `ALG-ACR-EQUIV-001` 块在 `:631–637`。**实测是 `:631–638`** —— T01 的 `:637` 截在 `upstream:` 行，**漏掉 `:638 downstream: []`**。照 `:631–637` 字面删，该行会变成 `ALG-GAIA-001` 的**重复 key**（其 `:630` 已有 `downstream: []`），正是任务书警告的结构破坏。依「删整块」按 8 行整删，并用**自建重复键检测 SafeLoader** 复核通过。

### 7.2 T01 §5.5 的退场顺序有断裂窗口（推翻）
T01 §5.5 给的顺序是「**先加 fail-closed 拒绝** → 再从白名单摘除 → **再删三份 JSON**」。
该顺序存在断裂窗口：**第 1 步落下后、删 JSON 前，三份现存配置会被新拒绝面 fail-closed 拒收。**
前台裁定的顺序（**先删配置侧、再改 parser**）**全程零断裂窗口**：改完配置侧时白名单仍含该键 → 配置照常解析；拒绝面落地时已无任何配置携带该键 → 无配置被拒。两者终态相同，但前台顺序在**任一中间态都自洽**。**照前台顺序执行。**

### 7.3 T01 §3-A5 「`mosaic_reject_legacy` 全仓零命中」不成立
T01 写「`git grep -c mosaic_reject_legacy -- lib` → 零命中（仅存于文档与本文件自身）」。
**实测**：该限定词 `-- lib` 成立，但**全仓并非零命中** —— `docs/science/algorithms/PHASE2_REJECTION.md:462`、`eng/tools/quality/v19r3_traceability.py:93` 仍在。
准确口径是「**`lib/` 下零命中、定义侧随 `acr_kernels.cpp` 一并删除**」。改写按此口径写，不写「全仓零命中」。

### 7.4 T01 §4.2 漏了镜像索引（本单新增不变量）
`clause_registry.json:3346` 的 `clause_registry.superseded_sections` 是顶层表的精确副本索引，仍列 `SUP-06`。T01 与前台派单都只提到 `counts` 与顶层数组**两个**数。
**逃过审计的原因很具体：`"SUP-06"` 这行不含 `ACR` 子串**，任何按 ACR 字符串扫的审计都扫不到。前台已补改，改为**三个数一致**。

### 7.5 T01 §3-A12 的口径已被丙档作废
A12 写「此项**不删行**（键仍在用），只订正值域为 `auto`/`cpu`」。
丙档退场该键后，「键仍在用」的前提不成立 —— 照 A12 原口径改值域，等于在一级正本里**继续把一个已被 parser 拒绝的键登记为活键**。已改用同文件 `:1125` 的退役键口径（行不删，内容改为「描述已删除的键」）。

### 7.6 T01 §4.3 `:70` 删 `"acr_kernels"` 的语义复核
派单要求先确认 L4 实测：`batch_of(path)` 的入参来自 `reports/v19r2/file_audit_inventory.csv` 的 `path` 列，判据是**对文件路径做子串匹配**。删 `"acr_kernels"` 后唯一会掉批的是「路径含 acr_kernels 且不含 rejection/integrate/block」的路径。穷举证明**零命中**（`git ls-files -- 'lib/algorithms/coverage/**' | grep -ci acr_kernels` → 0；`find` 无输出）⇒ **无任何现存路径失去识别**，未扩大范围。
附带实测：`update_audit_status.py` **本身已退役**（`:78-87 RETIRED_NOTICE`，`main()` 无条件 `return 2`），输入 `reports/` 目录都不存在 ⇒ 该编辑是一致性清理，**不改变任何活行为**。

### 7.7 审核包 §8.4 的「执行完清单后期望 0」不成立
T01 预期主门清零。实测 **210 行残留**，且其中至少 54 行是**乙档明令不得触碰**的（两条红线、冻结公开键面、调度器谓词、历史台账）。**「清零」这一期望本身与乙档定义冲突**，不应作为验收判据。真实判据是「甲档与丙档处置完毕、悬空引用为零、计数与索引不变量成立」。

---

## 8. 自证段（可复跑）

### 8.1 基线
```bash
cd "/workspace/Astro CS Database"
git log -1 --oneline                       # b4f37def ACR 文档退场：清掉仍带 ACR 的六处引用
git tag -l | grep pre-acr-removal           # pre-acr-removal（仍在，指向 4e682ffe，本单未动）
ls -d lib/infrastructure/acr 2>&1           # 不存在
```

### 8.2 本单体量
```bash
git -c core.quotepath=false diff --shortstat # 29 files changed, 80 insertions(+), 240 deletions(-)
git -c core.quotepath=false status --porcelain | grep -E '^ ?D'
#  D lib/algorithms/coverage/tools/memory_acr_compare.py
```

### 8.3 计数不变量（三数一致）
```bash
python3 -c "
import json
j=json.load(open('eng/contracts/data/clause_registry.json',encoding='utf-8'))
t=j['superseded_sections']; m=j['clause_registry']['superseded_sections']
print(j['counts']['superseded_sections'], len(t), len(m), j['counts']['superseded_sections']==len(t)==len(m))
print([s['id'] for s in t])"
# 9 9 9 True
# ['SUP-01','SUP-02','SUP-03','SUP-04','SUP-05','SUP-07','SUP-08','SUP-09','SUP-10']
```

### 8.4 索引真解析器
```bash
python3 -c "
import yaml,os
d=yaml.safe_load(open('docs/DOCUMENT_INDEX.yaml',encoding='utf-8')); print('PARSE OK',list(d.keys()))
p=[x for x in d['doc_index']['active'] if x.get('path')]
print('active 条目 =',len(d['doc_index']['active']),' 不存在 path =',[q['path'] for q in p if not os.path.exists(q['path'])])"
# PARSE OK ['doc_index'] ; 不存在 path = []
```

### 8.5 `acr_route` 顺序终态
```bash
grep -n '已删除：\|未知键门\|reject_unknown_keys(in, "integration' \
  lib/algorithms/coverage/src/stage2_common.cpp | sed -n '1,6p'
# 600 weight_mode / 612 legacy_allow_weight_fallback / 630 acr_route / 637 未知键门注释 / 639 门
sed -n '640p' lib/algorithms/coverage/src/stage2_common.cpp
#                           {"precision", "memory_limit_mb", "rejection"},
```
配置侧与生成方已零持有：
```bash
grep -rn 'acr_route' lib/algorithms/coverage/configs/ \
  lib/algorithms/coverage/tools/controlled_rejection_*.py \
  lib/algorithms/coverage/tools/satellite_gate_build.py   # 无输出
```

### 8.6 乙档未动
```bash
git -c core.quotepath=false diff --numstat -- \
  lib/infrastructure/gaia_xpsd_client lib/algorithms/coverage/tools/stage2.cpp \
  lib/algorithms/coverage/include lib/infrastructure/scheduler/src/module.cpp \
  lib/include/acsd/core/runtime.h lib/algorithms/integration \
  docs/engineering/UNRESOLVED_REGISTER.md 实验 artifacts
# 无输出
```

### 8.7 C++ 结构完整性（未编译，用静态计数）
```
braces 净差 = 0   （HEAD 亦为 0，本次编辑净贡献 0）
parens 净差 = 1   （HEAD 亦为 1，本次编辑净贡献 0；为 HEAD 上既有的注释/字符串内半个括号）
```

### 8.8 未执行事项（如实登记）
- **未跑编译、构建、测试**（遵守任务书与 AGENTS.md §9）。所有「活代码」判定基于符号级枚举与路径存在性。
- **未做任何 git 写操作**；未 add/commit/checkout/reset/stash/tag；`pre-acr-removal` 保持在 `4e682ffe` 未动。
- 唯一写入 = 本交付件 + T01 清单所列被处置文件。

---

## 9. 登记交负责人（不在本单授权范围）

| # | 事项 | 证据 |
|---|---|---|
| 1 | `实验/reviews/g08_residency_config_traits_review.md` 整篇 280 行审的是已删 ACR，15 条裁决无一适用当前树，却以现行断言口吻置于 `实验/reviews/` 下 | T01 §3-A15；仓内已记越权写入并请求删除 |
| 2 | `UNRESOLVED_REGISTER.md:1044`(R9) / `:1048`(R14) 两条**活待裁项**主体已失效 | T01 §3-A11 |
| 3 | `artifacts/.../RETIREMENT_LEDGER.md:49,85` 保留理由（依赖已不存在的 `test_iso_acr_gpu_isolation.py`）已失效 | T01 §3-A16 |
| 4 | **科学正本 ACR 段退场**：§6.2 ⑤ 类中 11 行仍把 `acr_kernels.cpp` 当现行组件 | §2.3 HEAD 比对 |
| 5 | **代码债**：`stage2_common.h:164` 字段 + `stage2_common.cpp:644-648` 不可达残留 + `module.cpp:57-59` 不可达谓词删除，均需编译验证 | §5.5 |
| 6 | `missing_unit_file` 现在全仓零命中，本身也是悬空词 | L4 报告 |
| 7 | `CONFIG_SCHEMA.md:6` 称一致性由 `eng/tools/config_consistency_check.py` 校验，该文件**不存在** ⇒ 改 `:58` 无机器门 | L3 报告 |
| 8 | `build/acr/` 未跟踪陈旧构建树仍在磁盘，会使「ACR 已从本机消失」的断言为假 | T01 §5.4 |
| 9 | 行锚更新提示：`v19r3_traceability.py` ID 校验正则由 `:378` 前移至 **`:368`**；`p2_acr_block_eligible` 定义由 `:685` 后移至 **`:700`** | L4 / 本单实测 |