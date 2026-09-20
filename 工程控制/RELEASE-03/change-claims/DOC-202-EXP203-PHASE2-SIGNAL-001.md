# 变更 claim：DOC-202-EXP203-PHASE2-SIGNAL-001 — Phase2 signal 量纲（面亮度）与 §5.3 输入语义守卫未接线如实登记

- 控制包：RELEASE-03 / 任务 **DOC-202**
- 日期：2026-09-20
- 依据（最高权威）：`ASTROCS_DESIGN.md` §4.4（signal = 面亮度）、§5.3（导出只接受面亮度语义输入）、§9（原子产品）、§0.2
- 依据（实验定案）：`工程控制/RELEASE-03/GAP_AUDIT.md` §5.4（EXP-203 C1/C1b/C2/C4 结论与判据冻结）
- 依据（工程流程）：`ENGINEERING_SPEC.md` §3；`AGENTS.md` §1.1/§8
- 状态：**已落地**（文档侧）；**代码侧零改动**（本任务未改任何 `lib/**` 生产代码，只改 `lib/**` 内的**文档锚点**）
- 影响类：**术语/登记级**（**不改**公式、常数、默认容差、SCI/ALG 冻结定义、数值结论）

## 1 C1 —— 单位枚举订正（`UnitId::ADU` → `UnitId::SURFACE_BRIGHTNESS`）

| 文件 | 原状 | 改为 |
|---|---|---|
| `docs/modules/registry/astrocs.phase2.write.md`（`mosaic` 端口行） | `UnitId::ADU`（signal surface brightness）——**自相矛盾**（枚举与文字相反） | **`UnitId::SURFACE_BRIGHTNESS`**（signal = 面亮度）；枚举实测存在：`include/astrocs/core/artifact.h:36` `SURFACE_BRIGHTNESS = 6`，phase3 descriptor 在用（`module_adapters.cpp:648/710/728/729/747`） |
| `lib/algorithms/coverage/hips_p2/README.md` §3（`:100`） | `UnitId::ADU` | **`UnitId::SURFACE_BRIGHTNESS`** |

> 注：`integrated` 端口的 `UnitId::ADU` **保留不动**——它是 descriptor 的编排层词汇
> （积分中间量），不是产品面亮度声明；EXP-203 只点名 `mosaic`（signal surface brightness）行。

## 2 C1b —— 「writer 视图中间量」消歧（防读成「产品是通量」）

| 文件 | 补入文字 |
|---|---|
| `docs/modules/registry/astrocs.phase2.write.md`（逆归一一段，原 `:41`） | 「该 `flux` 是 **writer 视图的中间量**（`aio_hips_write_signal_support_tile` 入参口径），**落盘值仍 = `flux_sum / covered_area` = 面亮度**；**不得**读成『产品是通量』」 |
| `docs/modules/hips_p2.md`（原 `:39`） | 同上口径 |

- 依据：EXP-203 判定 —— 叶级归一 `signal = flux_sum/covered_area` 为**恒等往返**（D2 hierarchy 守恒
  中位数 R=1.0；D1 跨分辨率常量场 R=1.0；D4 测光可对接 Σsignal·Ω/F_true = 1.000062）。
  ⇒ 产品语义 = **面亮度**；「flux」只存在于 writer 内部视图。

## 3 C2 —— 行锚订正（只改文档锚点，不改代码）

| 文件 | 原锚（已漂移） | 改为（实测） |
|---|---|---|
| `docs/modules/registry/astrocs.phase2.write.md`（2 处） | `module_adapters.cpp:739-756` / `:739-752` | **`module_adapters.cpp:1040-1057`** |
| `lib/algorithms/coverage/hips_p2/README.md`（`:99`） | `module_adapters.cpp:677-694` | **`module_adapters.cpp:1040-1057`** |
| `lib/algorithms/coverage/hips_p2/module.yaml`（`:33`） | `module_adapters.cpp:677-694` | **`module_adapters.cpp:1040-1057`** |

- 实测命令：`grep -n 'p2_write_descriptor' lib/infrastructure/scheduler/src/module_adapters.cpp`
  → `:1040`（`ModuleDescriptor p2_write_descriptor() {`），函数体止于 `:1057`。
- **文件域声明（越界如实登记）**：`lib/algorithms/coverage/hips_p2/{README.md,module.yaml}`
  在 `lib/**` 下（任务书通则「禁止改 `lib/**`」），但**任务书「另两项实验驱动的订正」逐字点名这两个文件:行**
  且限定「**只改文档锚点，不改代码**」⇒ 本 claim 如实登记该**唯一**的越界面：**仅改注释/文档文本的锚点与单位枚举文字，
  零代码语义改动**。`module.yaml` 的改动保持**行数中性**（同行合并），未移动任何下游行。

## 4 C4 —— §5.3 输入语义守卫「未接线」如实登记

- **事实（实测）**：守卫内核 `lib/phase3_session/p3_rsmp_units.cpp` 与其会话层
  `lib/phase3_session/p3_v6_export.cpp` **均未进构建**：
  `grep -c p3_v6_export CMakeLists.txt` = **0**；`grep -c p3_rsmp_units CMakeLists.txt` = **0**。
- **订正**：`docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv:338` 原把 `p3_v6_export.cpp`
  标 `classification=production` / `production_reachable=yes` —— **不实**；
  改为 `NOT_WIRED(未进构建;如实登记未实现)` / `no` / phase=`Phase3`，
  并在 `risk_note` 写明「**禁止**据此行声称 §5.3 输入语义守卫已生效；接线后改回 `production/yes` 并附构建证据」。
- **连带（越界如实登记）**：该订正使 CI 台账条目 `SNI-S4-P3X-12` 的 `exit_condition` 达成
  （「清单更正 classification/production_reachable … 然后删除本条目」），检查器随即报
  `ledger_stale:SNI-S4-P3X-12（台账条目已不复现 ⇒ 必须删除）`。
  ⇒ 已从 `ci/ledgers/spec_named_impl_gaps.json` **删除该条目**（该文件不在 DOC-202 文件域；
  属「不得引入新 finding」的必要收口，已在回执登记）。
  删除后 `ci/check_spec_named_impl.py` 的 finding 数回到**基线水平**（仅剩既有 `ledger_stale:SNI-S2-NS-01`）。
- **未改**：`docs/science/**`、`docs/algorithms/**`（`docs/algorithms/PHASE3_PROJ_IMPL.md` 等）⇒ 归 **DOC-205**；
  `contracts/**`、`docs/contracts/**` ⇒ 归 **DOC-203**。

## 5 验收证据

| 命令 | 结果 |
|---|---|
| `grep -n 'SURFACE_BRIGHTNESS' docs/modules/registry/astrocs.phase2.write.md lib/algorithms/coverage/hips_p2/README.md` | 命中（原 `UnitId::ADU（signal surface brightness）` 已消除） |
| `grep -rn '739-756\|739-752\|677-694' docs/modules/registry/astrocs.phase2.write.md lib/algorithms/coverage/hips_p2/` | 无命中（锚点已订正为 `1040-1057`） |
| `grep -c p3_v6_export CMakeLists.txt` | `0`（守卫未接线的判据） |
| `python3 ci/check_spec_named_impl.py` | finding 数 = 1（基线水平；无新增） |
| `grep -n 'p3_v6_export' docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv` | `NOT_WIRED` / `no` |

## 6 残留

- 代码侧接线（把 `p3_v6_export.cpp` / `p3_rsmp_units.cpp` 编入生产 target）⇒ **FIX 系列 / Phase3 export 域主**；
  台账条目 `SNI-S4-P3X-06` 仍承载该缺口（未删）。
- `docs/algorithms/**` 的守卫「已生效」类措辞（若有）⇒ **DOC-205**。
