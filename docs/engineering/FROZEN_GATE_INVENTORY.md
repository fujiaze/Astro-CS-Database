# 冻结门处置面盘点（FROZEN GATE INVENTORY）

> 上游：`docs/ACSD_DESIGN.md` §12.1（科学正确性）、`docs/engineering/SCIENCE_FREEZE.md`（冻结基线）、
> `docs/science/algorithms/GATES_AND_TOLERANCES.md` §1 R1/R2（表内唯一来源、证据必需）
> 机器判据：FG-01..FG-06（裁决器无在位载体）
> 盘点表（机器可读）：`eng/tools/acceptance/frozen_gate_inventory.json`
> 盘点时点 commit：`e9fa20d9`

## 0 判据（盘点口径）

| 问 | 怎么判 |
|---|---|
| **是否真的在跑** | 只认机器证据：已注册 ctest 目标名（`ctest -N` 实测 585 个）。写代码≠在跑 |
| **谁被通知** | 通知面必须具名：`product_log` / `ci_step` / `run_artifact`；写 `none` 就要说明 |
| **有没有出口** | 出口 = 触发后是否有**制度化**的登记/处置面，不指技术上能不能绕开。`none` 必须带 `remediation` |

## 1 逐条盘点（7 条）

| 门 | 在跑？ | 机器证据（详见盘点表） | 通知面 | 出口 |
|---|---|---|---|---|
| **F9-WCS-ABS**（DISP-WCS-001 / RESCUE F-9） | **是** | `lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp`；ctest `p1wcs_negative` / `p1wcs_apbp` | product_log | 停工 + 登记面=档位判词记录 |
| **G-RES-01**（L2 资源四条） | **裁决器在跑，当轮不跑** | 资源门判定域 = `docs/detail/infrastructure/21_observability.md` §8；CI 侧只登记 `L2-FROZEN-GATE-SELFTEST` / `L2-FROZEN-GATE-REPLAY` | run_artifact | record_and_justify |
| **G-P1-GATETABLE**（P1 检测/PSF/WCS 20 行门表） | **是** | `CHK-GATES-AND-TOLERANCES(-SELFTEST)`；20 行中 18 行的 `ctest:` 证据 ID 实测全部解析成功 | ci_step | 降级为诊断 |
| **A6-GATE-PHOT**（P1 测光 σ 双边界） | **不在跑**（已声明缺口） | `checks.json` 161 条中 `实验/` 只出现在 `changed_paths`，0 条作为执行面；插件文档自述「判据尚未在代码中生效」 | **none** | **none** |
| **L4-SEAM-FOOTPRINT**（接缝门） | 注册在跑，零产品时跳过 | `CHK-L4-SEAM-FOOTPRINT`；其 PRODUCT 步的 `optional_inputs` 实测不存在 | ci_step | 降级为诊断 |
| **CHK-REALDATA-E2E**（真实数据 E2E） | 只在 linux-deep / prerelease | `checks.json`：`waivable=true`、前置缺失 rc=77 | run_artifact | 可豁免 |
| **CPU-PROFILE-BIND**（画像↔构建指纹） | **是**（产品面） | `lib/infrastructure/cli/commands.cpp`（rc=5） | product_log | **none** |

## 2 「触发即永久卡死且无出口」的判定与处置

### 2.1 F9-WCS-ABS —— 停工型，登记面 = 档位判词记录

触发形态：帧被拒 → 块级中止（帧序执行，首帧失败即中止）→ 整块不出产品。
触发事件在轮次报告里有记，**产品面（manifest / 判词记录）无记录**，等价于「停工且无登记面」：
只看产品面的人无法区分本档是「首帧中止」还是「全批失败」。

登记面定义见 TV-03 / TV-06 / TV-12（档位判词记录面）。

**处置方案（本轮已落地一半）**：
1. 登记面 = 档位判词记录（TV-03/TV-06/TV-12）；
2. 拒绝记录必带门 ID / 原文原因 / 分类（产品行为 or 缺陷）/ 出口 / 确定性复现；
3. 存在未消命中时，档位判词写 `VERIFIED` 即判红（TV-12）。

### 2.2 A6-GATE-PHOT —— 真正的「无出口」（已声明缺口 + 处置方案）

σ 判红（G7 = `ABOVE_CEILING`）在实验域不留运行结果归档，其正本落在实验报告正文：
`实验/photometric-magnitude/README.md` §4.7 与该单元 `REPORT_experiment.md` / `REPORT_paper.md`
的真实帧腿段（`σ_obs = 0.026520` > `σ_ceiling = 0.020561`）。该判红**不进门、不通知、不阻断**，
对产品零影响：这是「判红但没人被通知」的最典型形态。

**处置方案**：
1. **确认面**：该判红须由对抗性审核重新推导结论并构造反例确认——从 `实验/photometric-magnitude/code/`
   复跑判定序，在固定 seed 下重建 `σ_obs` 与 `σ_ceiling`，并构造正反算例确认门在真值无效应时归零、
   在乘性空间残差注入下单调上升并判红。**判据自身绿不构成该判红的证据**；该确认由人读对抗审核承担，
   不为它注册读取归档的常驻机器门（本仓不为此新建常驻门）。
2. **口径面**：真实帧腿的 `σ_color` / `σ_gaia` / 大尺度平场三项不可自算 ⇒ `σ_ceiling` 不完整（偏严方向），
   L1 收口时须二分这条红的成因：「预算不完整导致的判红」vs「已消系统项超预算」，两者不可混谈。

### 2.3 CPU-PROFILE-BIND —— 处置未进验收流程（已登记 + 待落）

`build.source_commit` 变更 ⇒ rc=5 拒绝执行；无画像走最小可用基线 rc=0。两类事实分流是对的，
但「画像过期时谁重跑 benchmark、何时算阻塞」没有落进验收流程（只存在于 R-58 轮次报告 §7.2）。

**处置方案**：档位判词记录新增必填 `cpu_profile_provenance = {path, source_commit, rewritten_this_round}`；
档位证据的二进制与画像绑定逐档登记，否则该档结论的「同二进制性」不可复核。
（机器侧已由 `rel790_pack_check.py` RP-08 覆盖。）

## 3 可执行证据

裁决入口由门禁执行器提供；本仓无在位的门禁注册面与执行器（`--self-test` 契约 = 正例 1/1 + 注入负例 6/6）。

实测：`FROZEN_GATE_EXIT_PASS: gates=7 无出口=2 已声明未运行缺口=1 red_rules=0`。
「无出口=2」= A6-GATE-PHOT + CPU-PROFILE-BIND，二者均已带 `remediation`；
「已声明未运行缺口=1」= A6-GATE-PHOT（`open_gap=true`）。

## 4 本盘点不做的事

- 不改任何阈值、不改任何冻结定义（本轮硬约束）；
- 不重跑全量端到端、不跑重计算批次；
- 「是否在跑」只做静态取证（读 `checks.json` + `ctest -N`），未实跑各门。