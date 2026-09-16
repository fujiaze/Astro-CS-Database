# GOV — 治理与权威链收敛审计（ROOT-004 扩展轴）

- **轴名**：GOV（治理与权威链收敛）
- **基线核对**：**失败 → 审计中止**。指定基线 HEAD=main=origin/main=`2c328348304d033aecfa81faf79d1c6cd802b30a`；
  实测（timeout 30 git rev-parse HEAD main origin/main）：HEAD=`4fc3e898e3394d6e64e353ef5dad8c99716e8c88`、main=`4fc3e898…`、origin/main=`4511712b345a2548aad6230ad61cc705b44ef7bd`。
  三者互不相等且均≠基线；`2c328348` 为 HEAD 祖先（HEAD 已含其后 7 个提交），main 领先 origin/main 2 个未推送提交。
- **方法**：按开工指令先核基线；不一致即触发「立即停止并回报」硬门，权威文档精读与全轴普查未执行。
- **摘要**（≤10 行）：
  1. 基线三重不符（HEAD/main/origin/main 各不一致），审计对象「当前树」不可锚定。
  2. 工作区另有 155 处脏路径（18 个已跟踪修改 + 137 未跟踪），其中含本审计对照面 `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md` 本身——对照面正在被并发改动。
  3. 基线之后新增提交含 ROOT-001/002/003、GAP-026/027/028、设计大纲删除（344 文件），本轴多个预设取证点（根白名单、旧路径引用）前提已变。
  4. 诊断中顺带可见 main 外本地分支 `ci-fix@69ec459f`（AGENTS.md §5 禁开分支），列为待复核诊断，不立条。
  5. 依「宁缺毋滥、禁止编造证据」条款，本轮零发现；建议控制包以新基线（并先处置脏树/未推送/旁支）重派 GOV 轴。
- **发现计数**：P0=0 ｜ P1=0 ｜ P2=0（审计未执行，非「无问题」）

## 发现表

| ID | 定位 | 违反条款 | 证据 | 严重度 | 影响 | 整改建议 | 文件域 | 验收门 | GAP/任务关系 | 旧清单同源 |
|---|---|---|---|---|---|---|---|---|---|---|
| — | — | — | 本轮无条目（基线门中止） | — | — | — | — | — | — | — |

## 诊断观察（非审计发现，供调度参考，均有本轮真实输出为证）

- D-1 基线漂移：HEAD/main=4fc3e898，origin/main=4511712b，基线 2c328348 落后 7 提交（含设计大纲整体删除、ROOT-001/002/003 落账）——GOV 轴根白名单与旧路径引用普查的前提文件集已变化。
- D-2 脏树：18 个 tracked 修改（GAP_AUDIT.md、docs/contracts/DATA_ARTIFACTS.md、docs/contracts/INDEX.yaml、问题扫描/ 多篇、artifacts/prerelease_v5/**）+137 untracked——审计「当前树」≠任何提交态。
- D-3 旁支存在：本地分支 ci-fix（69ec459f，2026-09-08），与 AGENTS.md §5「不在 main 外开分支」相悖，建议前台核对处置。
- D-4 推送不一致：main 领先 origin/main 2 提交未推，违反 AGENTS.md §7「每次 push 后核对三 SHA 一致」所指向的收敛态。

完整命令与逐字输出见 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/GOV.log`。
