# SCI2 — Phase2/mosaic + Phase3/export 科学一致性抽样审计

- **轴名**：SCI2（ROOT-004 扩展轴 · Phase2/mosaic × Phase3/export 科学一致性抽样）
- **基线核对（开工首步，`timeout 30 git rev-parse HEAD main origin/main`）**：
  - 任务书期望基线：`2c328348304d033aecfa81faf79d1c6cd802b30a`
  - 实测 `HEAD` = `4fc3e898e3394d6e64e353ef5dad8c99716e8c88`
  - 实测 `main` = `4fc3e898e3394d6e64e353ef5dad8c99716e8c88`
  - 实测 `origin/main` = `4511712b345a2548aad6230ad61cc705b44ef7bd`
  - 期望基线状态：存在（`git cat-file -t` = commit），且为 HEAD 的祖先（`git merge-base --is-ancestor` 退出 0）；`git log --oneline 2c328348..HEAD | wc -l` = 6
  - 工作树：**dirty**（`git status --porcelain` 含 `docs/contracts/DATA_ARTIFACTS.md`、`docs/contracts/INDEX.yaml`、`工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md`、`问题扫描/**` 等修改，及 `artifacts/**` 未跟踪件）
- **方法**：先核基线 → 不一致立即停止（任务书硬规则）。本轮因此未进入「读权威文档 + 逐条对实现定位 + 真跑取证」阶段，未产出任何科学一致性发现条目。
- **摘要（≤10 行）**：
  1. 基线三条件同时破：HEAD ≠ 期望 SHA；HEAD ≠ origin/main（本地领先 2 提交）；工作树不干净。
  2. 漂移内容为纯 docs/chore：2c328348 之后 6 个提交（c5392daa / 939d3f6c / c44adc08 / 4511712b / e5fba371 / 4fc3e898），其中 4511712b 删除 `设计大纲/`（344 tracked 文件）。
  3. 中止的直接理由：任务书「不一致立即停止并回报」优先于抽样执行；在漂移且脏的树上定位行号会产生不可复现的证据。
  4. 脏区包含本轴对照面 `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md` 与合同面 `docs/contracts/**`（只读输入被并行写者改动），进一步说明当前树非稳定审计基线。
  5. 同轴兄弟 SCI1 亦因同一原因中止（`run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/SCI1.log`），两轴互证：漂移是全局的，非本轴误读。
  6. 本轴发现数 0，**不代表 Phase2/Phase3 无问题**，仅代表本轮未取证据、不立条（宁缺毋滥）。
  7. 恢复路径见文末「重跑前置条件」。
- **发现计数**：P0 = 0 ｜ P1 = 0 ｜ P2 = 0（本轮零发现：审计未启动，无证据条目）

## 发现表

| ID | 定位 | 违反的最新权威条款 | 当前证据 | 建议严重度 | 影响 | 整改建议 | 建议文件域 | 验收门 | GAP/任务关系 | 旧清单同源 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| —（无） | — | — | — | — | — | — | — | — | — | — |

## 基线异常记录（非发现条目，不占 SCI2-n 编号）

| 项 | 事实 | 证据（本轮真跑） |
| --- | --- | --- |
| B-1 基线 SHA 漂移 | HEAD/main = 4fc3e898 ≠ 期望 2c328348 | `git rev-parse HEAD main origin/main` → 三行逐字见上「基线核对」 |
| B-2 三分支不等 | HEAD=main ≠ origin/main（本地领先 2 提交：e5fba371、4fc3e898） | `git log --oneline -3` → `4fc3e898 … / e5fba371 … / 4511712b …` |
| B-3 工作树脏 | 18 项 ` M` + `?? artifacts/AstroCS_AUDIT_REVIEWPACK_*.zip`、`?? artifacts/KNOWN_FAILURES_BASELINE.json` | `git status --porcelain | head -20` |
| B-4 对照面被并发改动 | `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md`、`工程控制/PROJECT-GOVERNANCE-01/tasks/CFG-001.md` 处于 ` M` | 同上 `git status --porcelain` |

B-1..B-4 属治理/环境状态，交 ROOT-004 前台判定归口（与 GAP-024..028 治理账同域）；本轴不越权立条、不认领归属。

## 重跑前置条件（下一轮 SCI2 可直接执行）

1. 前台把基线取齐：任务书基线更新为三 SHA 一致点（需先同步使 HEAD=main=origin/main），或在本轴排期前冻结漂移窗口并清工作树。
2. 复跑单命令门（通过才开工）：
   `timeout 30 git -C "/workspace/Astro CS Database" rev-parse HEAD main origin/main | sort -u | wc -l` 期望 `1` 且该唯一 SHA 等于任务书基线；
   `timeout 30 git -C "/workspace/Astro CS Database" status --porcelain | wc -l` 期望 `0`。
3. 开工必读（本轴口径，本轮尚未开始读）：`ASTROCS_DESIGN.md` §0 权威链、`AGENTS.md`、`ENGINEERING_SPEC.md`、`docs/ci/01_CHECKS.md`、`docs/ci/03_GATES.md`，及 `docs/plugins/**` 中 mosaic/export 相关篇。
4. 抽样计划（本轮未执行，留待下一轮）：
   - 公式面：`docs/science/UPM`、`REJECTION`、`INTEGRATION`、`CONTROL_WEIGHT_SNR` §4（相对权重 ≠ 科学 SNR）、`PHASE3_PROJECTION`（八投影冻结 TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA）、`RESAMPLE`、`FITS_OUTPUT`、`HIPS`、HEALPix 合同（Nside 为 2 的幂、ipix 10000 分目录、NSIDE=2^(K+9)）。
   - 实现面：`lib/phase2*`、`lib/hips`、`lib/phase3_*`、`runtime/io`。
   - 重点 6 项：support 作权重族残留（`weights = support_v * snr^2` 类）｜ivar tile fallback 静默降级｜投影 registry 是否仍只 4 投影｜CRVAL2 与 h→nside 读写两侧是否互斥｜`creator_did` 等 IVOA 字段｜像素尺度一致性；每条给公式锚 + 实现锚 + 测试面。
   - 旧清单交叉：`reports/PROJECT-GOVERNANCE-01/root-scan/_gen/idmap.csv`（列 id/title/file，用 python 解析）。

## 本轴合规声明

- 零修复、零 git 写（仅只读 `rev-parse` / `log` / `status` / `cat-file` / `merge-base` / `branch --contains` / `show`）；未 commit/push/branch/stash/reset/clean/checkout/add。
- 本轮写入路径仅两处：本报告与 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/SCI2.log`。
- 未读取、未打印、未复制 `FATDUCK_ACCESS.md`。
- 所有外部命令带 `timeout 30~60`；`grep/find` 类扫描本轮未触发（未进入实现面扫描）。
