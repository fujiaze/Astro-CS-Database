# 冻结门判别力口径与盲区登记

> 上游：`docs/ACSD_DESIGN.md` §12.1（科学正确性）、`docs/engineering/SCIENCE_FREEZE.md`（冻结基线）、
> `docs/science/algorithms/GATES_AND_TOLERANCES.md` §1 R1/R2（表内唯一来源、证据必需）

本文件给出两样东西：判定一道门**是否具备判别力**的口径，以及本仓几处具体判定点的**已知盲区**。

判定由人读对抗性审核逐条给出：审核者构造反例、确认注入真的生效，再据结果定性。
判定结果不产出自动判红判绿，也不进流水线；本文件因此不描述任何注册或执行流程。

## 0 判据（盘点口径）

| 问 | 怎么判 |
|---|---|
| **是否真的在跑** | 只认执行证据：判定点确实被触达、确实执行了比较、结论确实被读到。写代码≠在跑；被登记≠在跑；写在产物里无人读≠在跑 |
| **谁被通知** | 读取方必须具名：`product_log` / `run_artifact` / 读到这条结论并据其行动的审核者；写 `none` 就要说明 |
| **有没有出口** | 出口 = 触发后是否有**制度化**的登记/处置面，不指技术上能不能绕开。`none` 必须带 `remediation` |

## 1 判定点登记

| 判定点 | 执行证据 | 通知面 | 出口 |
|---|---|---|---|
| **F9-WCS-ABS**（DISP-WCS-001 / RESCUE F-9） | `lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:727`：残差 `rms_px>0.5` 或内点 `n_pairs<12` 即拒 | product_log | 停工 + 档位判词记录 |
| **G-RES-01**（L2 资源四条） | 判定域 = `docs/detail/infrastructure/21_observability.md` §8；阈值唯一数值源 = `eng/contracts/resource_gate_v1.json`；判定实现 = `eng/tools/monitoring/run_monitored.py::evaluate_frozen_gate`，采样 = `eng/tools/quality/resource_monitor.py`（导入期 fail-closed） | run_artifact | record_and_justify |
| **G-P1-GATETABLE**（P1 检测/PSF/WCS 门表） | 阈值正本 = `docs/science/algorithms/GATES_AND_TOLERANCES.md`；本仓无执行该表的判定面 | 无 | 降级为诊断 |
| **A6-GATE-PHOT**（P1 测光 σ 双边界） | 判定序在 `实验/photometric-magnitude/code/`；正本落在该单元实验报告正文 | **none** | **none** |
| **CPU-PROFILE-BIND**（画像↔构建指纹） | `lib/infrastructure/benchmark/backend_host/profile_store.cpp`；`source_commit` 不符归入 `stale_build`（失效分类正本 = `profile_store.h`） | product_log | **none** |

## 2 已知盲区与处置

### 2.1 F9-WCS-ABS —— 停工型，登记面 = 档位判词记录

触发形态：帧被拒 → 块级中止（帧序执行，首帧失败即中止）→ 整块不出产品。
触发事件在轮次报告里有记，**产品面（manifest / 档位判词记录）无记录**，等价于「停工且无登记面」：
只看产品面的人无法区分本档是「首帧中止」还是「全批失败」。

登记面定义见 TV-03 / TV-06 / TV-12（档位判词记录面）。

**处置方案**：
1. 登记面 = 档位判词记录（TV-03/TV-06/TV-12）；
2. 拒绝记录必带判定点 ID / 原文原因 / 分类（产品行为 or 缺陷）/ 出口 / 确定性复现；
3. 存在未消命中时，档位判词写 `VERIFIED` 之前必须先由对抗性审核确认该命中成立 ——
   写 `VERIFIED` 本身不构成确认，不得作为通过依据。

### 2.2 A6-GATE-PHOT —— 真正的「无出口」

σ 判红（G7 = `ABOVE_CEILING`）在实验域不留运行结果归档，其正本落在实验报告正文：
`实验/photometric-magnitude/README.md` §4.7 与该单元 `REPORT_experiment.md` / `REPORT_paper.md`
的真实帧腿段（`σ_obs = 0.026520` > `σ_ceiling = 0.020561`）。该判红**不通知、不阻断**，
对产品零影响：这是「判红但没人被通知」的最典型形态。

**处置方案**：
1. **确认面**：该判红须由对抗性审核重新推导结论并构造反例确认——从 `实验/photometric-magnitude/code/`
   复跑判定序，在固定 seed 下重建 `σ_obs` 与 `σ_ceiling`，并构造正反算例确认判定在真值无效应时归零、
   在乘性空间残差注入下单调上升并判红。**判定自身绿不构成该判红的证据**。
2. **口径面**：真实帧腿的 `σ_color` / `σ_gaia` / 大尺度平场三项不可自算 ⇒ `σ_ceiling` 不完整（偏严方向），
   收口时须二分这条红的成因：「预算不完整导致的判红」vs「已消系统项超预算」，两者不可混谈。

### 2.3 CPU-PROFILE-BIND —— 处置未进验收流程

画像失效（`build.source_commit` 与画像记录不符）会被归类为 `stale_build` 而非静默采用；
无画像则走最小可用基线。两类事实分流是对的，
但「画像过期时谁重跑 benchmark、何时算阻塞」没有落进验收流程。

**处置方案**：档位判词记录必填 `cpu_profile_provenance = {path, source_commit, rewritten_this_round}`；
档位证据的二进制与画像绑定逐档登记，否则该档结论的「同二进制性」不可复核。

## 3 复核方式

本文件所记盲区的定性一律走对抗性审核：先复跑判定序重建读数，再构造反例确认判定有牙齿。
「写代码≠在跑」「被登记≠在跑」在删除登记面之后尤其成立 ——
一个判定点是否具备判别力，只能由它对注入缺陷的反应给出，不能由它的存在给出。

## 4 边界

- 不改任何阈值、不改任何冻结定义；
- 不重跑全量端到端、不跑重计算批次；
- 本文件只登记判定点与盲区，不承担阈值正本职责（阈值正本见 `docs/science/algorithms/GATES_AND_TOLERANCES.md`）。
