# 工程控制 / RELEASE-03 验收记录（ACCEPTANCE）

> **规范依据**：`CONTROL_PACK_SPEC.md` §6.3（任务状态：`NOT_STARTED → IN_PROGRESS → PASS / FAIL / BLOCKED`；
> **PASS 仅由前台验收后写入**）、§7.1（验收三层：机器门 / 证据核验 / 文档-代码一致性）、
> §7.2（本文件表格格式）、§7.3（禁止事项）。
>
> **当前状态**：控制包**待负责人批准**（§3.3），**尚未执行** ⇒ 全部任务为 `NOT_STARTED`。

## 1. 验收三层（§7.1，缺一不可）

1. **机器门**：CI 检查项 / 单测 / 合同检查器，全部 `rc=0`；
2. **证据核验**：前台**独立复跑**关键验收命令，**不依赖 SubAgent 自述**；
3. **文档-代码一致性**：改动与任务声明的文件域一致，**未越界**。

## 2. 逐任务结论（§7.2 格式）

| 任务 | 状态 | 机器门结果 | 证据路径 | 前台结论 |
|---|---|---|---|---|
| DOC-201 详细层「权重模式」整套作废 | NOT_STARTED | — | — | — |
| DOC-202 详细层其余 🔴 订正 | NOT_STARTED | — | — | — |
| DOC-203 同批同步订正 + 4 前置裁决 | NOT_STARTED | — | — | — |
| DOC-204 权威链与状态字段清理 | NOT_STARTED | — | — | — |
| FIX-201 aio 文件级唯一 I/O 边界 | NOT_STARTED | — | — | — |
| FIX-202 HiPS 格式内部权重枚举作废 | NOT_STARTED | — | — | — |
| FIX-203 提升键落地 | NOT_STARTED | — | — | — |
| FIX-204 排异按 N 自动选择 | NOT_STARTED | — | — | — |
| FIX-205 投影未实现显式报不支持 | NOT_STARTED | — | — | — |
| FIX-206 原子性缺口闭合 | NOT_STARTED | — | — | — |
| FIX-207 三命令同构 schema/模板统一 | NOT_STARTED | — | — | — |
| FIX-208 事件流默认输出 + 磁盘门收窄 | NOT_STARTED | — | — | — |
| EXP-201 天光采样点权重 | NOT_STARTED | — | — | — |
| EXP-202 NaN 处置 | NOT_STARTED | — | — | — |
| EXP-203 Phase2 signal 量纲 | NOT_STARTED | — | — | — |
| EXP-204 排异 N≤3 | NOT_STARTED | — | — | — |
| EXP-205 SNR 三口径精度 | NOT_STARTED | — | — | — |
| EXP-206 噪声模型两套 | NOT_STARTED | — | — | — |
| BLD-201 全量构建与机器门 | NOT_STARTED | — | — | — |
| E2E-201 端到端重跑 | NOT_STARTED | — | — | — |
| ACC-201 验收 | NOT_STARTED | — | — | — |

## 3. 差距清单闭合追踪（对照 `GAP_AUDIT.md`）

> §9：**真实数据终验先于控制包完成声明**；未过终验不得写「完成」。

| 差距组 | 条目数 | 承担任务 | 闭合状态 |
|---|---|---|---|
| **违规** V01–V22 | 22 | DOC-201/202/203/204、FIX-201/202/205/206 | 未闭合 |
| **缺口** G01–G12 | 12 | DOC-203/204、FIX-201/203/207/208、EXP-201..206 | 未闭合 |
| **过时** O01–O04 | 4 | DOC-201/203、FIX-204 | 未闭合 |
| **漂移** D01–D07 | 7 | D01 ✅ 已闭合（`d519a67a`）；其余未闭合 | 部分闭合 |
| **无主** N01–N04 | 4 | N01 ✅ 已登记（`d519a67a`）；其余未闭合 | 部分闭合 |
| **UNRESOLVED** U01–U10 | 10 | U04/U05 ✅ 已定；其余未闭合 | 部分闭合 |

## 4. 本包开始前的已闭合项（留痕，非本包任务）

| 项 | 证据 | 提交 |
|---|---|---|
| 最高设计补齐（995 → 1,159 行，26 条条文订正 + 18 组新要点条款） | `ASTROCS_DESIGN.md` | `26cb79d9` |
| 配置锚点失效修复（38 处行漂移 + 6 处 defaults 锚点 + 3 未登记键 + totals 重算） | CFG002 11/11 PASS + selftest PASS；`tests/config` 58 passed | `d519a67a` |
| §9.73 A44 配置面摘除（help/模板/白名单） | `mosaic --help \| grep -c weight_mode` = 0；块内出现即 rc=3 | `444c220b` |
| §9.68 多数据块 JSON 落地（normalize `blocks[]`） | `tests/cli/test_multiblock_normalize.py` 9 例；`tests/config/test_cfg003_multiblock.py` | `ac75f3f3` |
| 全量 `ctest` 467/467 | `/var/tmp/astrocs/ctest_after_split.log` | — |
| `AGENTS.md` §1.1 铁律（任何任务先去最高文档确定规范） | `AGENTS.md` | 本包同批 |

## 5. 禁止事项（§7.3）

- **不得用 waiver 掩盖红灯**（最高设计不允许放宽的条款不可豁免）；
- **不得以「工具问题/环境问题」掩盖失败而不给复现**；
- **不得把「能编译」当「验收过」**——必须跑断言/测试。
