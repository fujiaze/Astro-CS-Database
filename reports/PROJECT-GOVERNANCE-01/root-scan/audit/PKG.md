# ROOT-004 扩展轴审计报告 — PKG（打包/安装树/版本纪律）

- **轴名**：PKG（打包/安装树/版本纪律审计，对照 DESIGN §10.1–§10.2、§12，ENGINEERING_SPEC §7，PKG-001）
- **基线核对（任务要求先跑的判定）**：**不一致 → 本轴审计中止**。
  - 指定基线：`HEAD=main=origin/main=2c328348304d033aecfa81faf79d1c6cd802b30a`
  - 首核（T+0）：HEAD=main=`4fc3e898e3394d6e64e353ef5dad8c99716e8c88`，origin/main=`4511712b345a2548aad6230ad61cc705b44ef7bd`（三者互不相等，且均≠指定基线）
  - 复查（T+2min）：三者=`900916fb0dfe93e21bd36c09908a6cc679de5256`（一致，但领先指定基线 8 个提交：`2c328348..HEAD` 含 GAP-026/027/028 登记、设计大纲/删除、ROOT-001/002/003、TEST-GREEN-001 立卡、基线快照归档等 docs/chore 提交）
  - 工作树脏：19 个已跟踪文件被修改未提交（含 `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md` 本身），138 个未跟踪文件
- **方法**：`timeout 30 git rev-parse HEAD main origin/main` 等只读 git 核对（全记录见 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/PKG.log`）；核对失败后按任务卡硬指令立即停止，未进入任何文件审读。
- **摘要（≤10 行）**：
  1. 基线三重相等条件在开工时不成立（HEAD≠origin/main）。
  2. 指定基线 2c328348 仍是 HEAD 祖先，但树已在 8 提交之外且持续移动（审计窗口内前台又推送 2 提交）。
  3. 工作树大量未提交修改，其中包含本审计的对照面 GAP_AUDIT.md——即便勉强开审，对照面与"当前树"均非任务卡所锚定的对象。
  4. 依"不一致立即停止并回报"，PKG 轴全部预定检查项（install_layout.cmake 安装集、packaging/*.json 清单 sha256/source_commit、version_generated.h/--version/CHANGELOG 版本面、23 插件 DLL 边界、pack 白名单含 FATDUCK_ACCESS.md 登记项、win .in 模板 vs Linux 双平台实况）**均未执行，本报告不构成任何"已审/通过"依据**。
  5. 处置建议：由前台在稳定点重新冻结 PKG 轴基线 SHA（并先处理/提交 GAP_AUDIT.md 等 19 个脏项，或明示以含脏项的树为准），再重新派发本轴。
- **发现计数**：P0=0　P1=0　P2=0（**审计未执行，非"零问题"**）

## 发现表

| ID | 定位 | 违反条款 | 证据 | 严重度 | 影响 | 整改建议 | 文件域 | 验收门 | GAP/任务关系 | 旧清单同源 |
|---|---|---|---|---|---|---|---|---|---|---|
| （空） | — | — | — | — | — | — | — | — | 本轴挂起：待重定基线后复跑 | — |

> 备注：唯一登记事项为**流程性**中止记录，不作为发现条目；FATDUCK_ACCESS.md 白名单核查（PKG-001 关联网项，且 c44adc08 已立 ROOT-006/GAP-028 凭据入仓）在本轴复跑时必须优先执行，当前未读未判。
