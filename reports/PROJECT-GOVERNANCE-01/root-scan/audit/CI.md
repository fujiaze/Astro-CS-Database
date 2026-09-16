# CI 轴审计报告（ROOT-004 扩展轴 · 代号 CI）

- **轴名**：CI 注册表与机器门有效性审计（ci/checks.json、docs/ci/01-03、tools/ 与 tools/quality 检查器 fail-open 形态、三个红着的旧门、criterion→CHK 映射）
- **基线 SHA**：**核对失败 —— 未建立有效基线**。任务书声明基线 `2c328348304d033aecfa81faf79d1c6cd802b30a`；本轮实测 `HEAD=main=4fc3e898e3394d6e64e353ef5dad8c99716e8c88`、`origin/main=4511712b345a2548aad6230ad61cc705b44ef7bd`。三者两两不等。
- **方法**：仅执行开工第一步 `git rev-parse HEAD main origin/main` 基线核对；按指令「不一致立即停止并回报」，**本轴审计未开工**（未读 docs/ci 正文、未跑任何检查器、未做负例抽样）。下列为核对过程中以只读 git 命令取得的终止诊断证据。
- **摘要（≤10 行）**：
  1. 三 SHA 不一致：HEAD 与 main 同为 `4fc3e898`，origin/main 为 `4511712b`，与声明基线 `2c328348` 均不符。
  2. `2c328348` 确实是本仓库一个合法 commit，且是 HEAD 的祖先（`ANCESTOR_OF_HEAD`），即基线已过期而非拼写错误。
  3. 自声明基线以来新增 **6 个提交**（c5392daa → 939d3f6c → c44adc08 → 4511712b → e5fba371 → 4fc3e898），其中 3 个是 governance/GAP 登记、2 个是 ROOT-002/001/003 根目录门与保留策略、1 个是删除 `设计大纲/`（344 个 tracked 文件）。
  4. HEAD 比 origin/main **超前 2 个提交**（`git rev-list --left-right --count HEAD...origin/main` = `2  0`）：本地未 push，违反 AGENTS.md §7「每次 push 后 fetch 并核对三 SHA 一致」的基线前提。
  5. 工作树极脏：`git status --porcelain` 共 **158** 条目，其中 **138** 为未跟踪（`??`），含对 `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md`、`docs/contracts/**`、`artifacts/**`、`evidence/**`、`reports/**`、`问题扫描/**` 的已跟踪文件修改。
  6. 本轴审计对象亦受污染：`tools/quality/` 下存在 3 个未跟踪新检查器（`check_module_map.py`、`check_secret_hygiene.py`、`fixtures/module_map_fixture.py`），属未提交态；`ci/`、`docs/ci/` 相对 HEAD 干净。
  7. 脏树 + 未提交新门并存时，「当前树」行号与注册表条目均不稳定，任何发现表都无法作为可复跑验收门的判据。
  8. 另见只读观测：本地存在分支 `ci-fix`（69ec459f）。本轴不就此立条（属越权判定），仅作为基线漂移旁证记录，交由 ROOT/governance 轴核对 AGENTS.md §5「不在 main 外开分支」。
  9. **结论：本轴发现数为 0（未开展）**，不产出任何严重度判定、CHK-* 迁移建议或负例注入建议；待基线重设后重跑。
- **发现计数**：P0 = 0 ｜ P1 = 0 ｜ P2 = 0（本轴未开展审计，非「查无问题」，而是「未取证」）

## 发现表

| ID | 定位 | 违反的最新权威条款 | 当前证据 | 建议严重度 | 影响 | 整改建议 | 建议文件域 | 验收门 | GAP/任务关系 | 旧清单同源 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| （无） | — | — | — | — | — | — | — | — | — | — |

> 说明：按任务书「不一致立即停止并回报」，本轮不立任何发现条，避免以过期/脏树基线铸造不可复跑的假证据（宁缺毋滥）。

## 终止诊断证据（逐字，本轮真跑，全部只读）

```text
$ timeout 30 git rev-parse HEAD main origin/main
4fc3e898e3394d6e64e353ef5dad8c99716e8c88
4fc3e898e3394d6e64e353ef5dad8c99716e8c88
4511712b345a2548aad6230ad61cc705b44ef7bd
```

```text
$ timeout 30 git cat-file -t 2c328348304d033aecfa81faf79d1c6cd802b30a
commit
$ timeout 30 git merge-base --is-ancestor 2c328348304d033aecfa81faf79d1c6cd802b30a HEAD && echo ANCESTOR_OF_HEAD || echo NOT_ANCESTOR_OF_HEAD
ANCESTOR_OF_HEAD
$ timeout 30 git rev-list --count 2c328348304d033aecfa81faf79d1c6cd802b30a..HEAD
6
```

```text
$ timeout 30 git rev-list --left-right --count HEAD...origin/main
2	0
$ timeout 30 git log --oneline 2c328348304d033aecfa81faf79d1c6cd802b30a..HEAD
4fc3e898 docs(root): ROOT-001 根目录账本与 ROOT-003 run/ 保留策略（前台复跑一致）
e5fba371 feat(root): ROOT-002 根目录长效机器门（清单 + 检查器 + 测试 + 注册）
4511712b chore(root): 删除 设计大纲/（344 tracked 文件，负责人判定已无用）
c44adc08 docs(governance): 立 ROOT-006（凭据入仓 SECURITY-URGENT）并登记 GAP-028
939d3f6c docs(governance): 登记 GAP-027（build/ 清运暴露 API-DOCS 恒绿）并入 CI-001 口径
c5392daa docs(governance): 立 WIKI-001/ROOT-005 并登记 GAP-026（Wiki 权威自称与漂移）
```

```text
$ timeout 30 sh -c 'git status --porcelain=v1 | wc -l'
158
$ timeout 30 sh -c 'git status --porcelain=v1 | grep -c "^??"'
138
$ timeout 30 sh -c 'git status --porcelain=v1 -- ci tools docs/ci'
?? tools/quality/check_module_map.py
?? tools/quality/check_secret_hygiene.py
?? tools/quality/fixtures/module_map_fixture.py
```

```text
$ timeout 30 git branch -a --format="%(refname:short) %(objectname:short)"
ci-fix 69ec459f
main 4fc3e898
origin/main 4511712b
```

## 重跑前提（交回前台 / ROOT 轴）

1. 由前台完成未提交改动的归属判定与原子提交或清理，使 `git status --porcelain` 中已跟踪文件修改归零；
2. push 后 `git fetch`，确立 `HEAD == main == origin/main` 的新单一基线 SHA；
3. 以该新 SHA 重新下发本轴任务书（CI 注册表与机器门有效性审计：147 项旧注册 vs 27 个 CHK-*、waivable 滥用、`ci/exemptions.json` 只减不增、`EMPTY_OUTPUT_SILENCE_EXEMPT` 白名单、≥12 个检查器 fail-open 抽样、三个红着的旧门、criterion→CHK 映射缺口、CHK-* 迁移与负例注入建议）。

## 合规声明

本轮零修复、零 git 写（未执行 commit/push/branch/stash/reset/clean/checkout/add；git 仅只读查询）；除本报告与 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/CI.log` 外未写任何路径；未读取/打印/复制 `FATDUCK_ACCESS.md`。
