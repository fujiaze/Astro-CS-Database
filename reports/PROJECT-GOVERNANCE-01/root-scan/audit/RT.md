# RT 轴审计报告（调度/线程/资源/观测）— 基线核对不通过，已停机

- **轴名**：RT（ROOT-004 扩展轴：调度器/线程/资源/观测）
- **要求基线**：HEAD=main=origin/main=`2c328348304d033aecfa81faf79d1c6cd802b30a`
- **实测基线**（本轮真跑，`timeout 30 git rev-parse HEAD main origin/main`）：
  - HEAD = `4fc3e898e3394d6e64e353ef5dad8c99716e8c88`
  - main = `4fc3e898e3394d6e64e353ef5dad8c99716e8c88`
  - origin/main = `4511712b345a2548aad6230ad61cc705b44ef7bd`
- **方法**：按控制指令第一步核对三 SHA；不一致 → 立即停止，未进入轴内取证（未做 DESIGN §8/§6.3、ENGINEERING_SPEC §10、docs/plugins/infrastructure/19/20/21 的读取，未做硬编码线程/双调度器/利用率门分母/观测落盘等任何扫描）。
- **补充只读事实**（`git cat-file` / `git merge-base` / `git log` / `git status`）：
  1. 期望基线 `2c328348` 对象存在，且是 HEAD 的祖先（`merge-base --is-ancestor` rc=0）；
  2. HEAD 领先期望基线 **6 个提交**（c5392daa GAP-026 / 939d3f6c GAP-027 / c44adc08 GAP-028 凭据入仓 / 4511712b 删设计大纲 / e5fba371 ROOT-002 / 4fc3e898 ROOT-001+003），多为治理文档与根目录门提交；
  3. HEAD=main 一致，但 origin/main 停在 4511712b，**本地领先远端 2 个提交未推送**，三 SHA 不相等；
  4. 工作树脏：`git status --porcelain | wc -l` = **156**（含 artifacts/prerelease_v5/ISA-00*/MEASUREMENTS.csv、docs/contracts/DATA_ARTIFACTS.md、docs/contracts/INDEX.yaml 等），不满足开审计的干净基线条件。
- **摘要（≤10 行）**：基线三重不一致（HEAD≠指令要求值；origin/main≠HEAD；156 处未提交变更）。控制指令规定「不一致立即停止并回报」，故本轮 RT 轴零取证、零立条，避免在漂移+脏树上产出行号不可复现的伪证据。注意：漂移的 6 个提交均为治理/文档类，未触及 lib/ 调度面，若控制面裁定以 4fc3e898 干净重派，RT 轴发现预计仍有效。请 ROOT-004 控制面收敛仓库状态（确认 2c328348 是否已过时、指示前台提交或清理脏项并推送）后重派 RT 轴。RT-001/CPU-001/OBS-001 收敛顺序建议随重派一并给出。
- **发现计数**：P0=0 ｜ P1=0 ｜ P2=0（审计未执行，非「当前树无问题」结论）

## 发现表

| ID | 定位 | 违反条款 | 证据 | 严重度 | 影响 | 整改建议 | 文件域 | 验收门 | GAP/任务关系 | 旧清单同源 |
|----|------|----------|------|--------|------|----------|--------|--------|--------------|------------|
| （空） | — | — | — | — | — | — | — | — | — | — |

（无条目：基线不通过即停，未对当前树立任何发现。停机事项属控制面状态，不计入 RT 轴发现数。）

## 停机证据（逐字，本轮真跑）

```text
$ timeout 30 git rev-parse HEAD main origin/main
4fc3e898e3394d6e64e353ef5dad8c99716e8c88
4fc3e898e3394d6e64e353ef5dad8c99716e8c88
4511712b345a2548aad6230ad61cc705b44ef7bd
```

```text
$ timeout 30 git status --porcelain | wc -l
156
```

```text
$ timeout 60 git rev-list --count 2c328348304d033aecfa81faf79d1c6cd802b30a..HEAD
6
```

```text
$ timeout 30 git merge-base --is-ancestor 2c328348304d033aecfa81faf79d1c6cd802b30a HEAD; echo rc=$?
rc=0
```

**状态：BLOCKED（基线不匹配，按指令停机）。下一步：等待控制面给出修正基线或恢复指令基线后重派。**
