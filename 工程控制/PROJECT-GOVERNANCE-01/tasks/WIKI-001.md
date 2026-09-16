# 任务：WIKI-001 从权威文档生成项目 Wiki 并同步

状态：NOT_STARTED
层：L0　依赖：GOV-001（权威收敛后才有稳定的生成源）　文件域互斥组：S1-W

## 目标

把项目 Wiki（`https://github.com/fujiaze/Astro-CS-Database.wiki.git`）从「手工维护、已漂移」改为**由权威文档生成**：

1. 生成器把仓库内权威文档渲染为 Wiki 页面（导航 + 内容 + 交叉链接）；
2. 消除 Wiki 的**权威自称**——现状 `Home.md` 第 3 行写「本 Wiki 是项目唯一权威标准」，与 ASTROCS_DESIGN.md §0 的权威链**直接冲突**（见 GAP-026）；
3. 建立可复跑的同步流程（dry-run 产物 + 人工确认后再推送），杜绝再次漂移。

## 基线状态（编制时实测 2026-09-16）

- 远端 wiki 可达：`git ls-remote` 返回 `HEAD = refs/heads/master = 901725847ded7d1185a98b995df8fce9a7e20a1e`；
- 内容：**34 个页面 / 148K**，最近提交 `9017258 docs(wiki): Phase2 V1 — one UPM, dynamic block, rejection, HiPS mosaic; HICS removed from Phase2 design`；
- **已漂移**：22 页提到 `HISS`（已废止）、4 页 `HICS`、2 页 `Phase3`；当前三命令名 normalize/mosaic/export 中 `normalize` 与 `export` 在 Wiki 里**零命中**（`mosaic` 仅 3 页，且属旧口径）；
- 本地 `AstroCS.wiki/` 目录**为空**且被 `.gitignore:122` 的 `/AstroCS.wiki/` 忽略；
- **成因（已查清，非本轮清运所致）**：`25259880`（2026-09-06）移除了索引里的失效 gitlink（当时无 `.gitmodules`，导致 actions/checkout 失败），该提交说明写明「磁盘 AstroCS.wiki/ 目录保留（owner 本地 wiki clone，转为未跟踪）」——即它**曾是本地手工 clone**，后被清掉；
- 结论：**没有内容丢失**（wiki 内容一直在 wiki 仓库里），本地那份从来不是生成物。

## 权威依据

- ASTROCS_DESIGN.md §0（权威链：ASTROCS_DESIGN > AGENTS.md > ENGINEERING_SPEC > CONTROL_PACK_SPEC > docs/ci > docs/plugins；**Wiki 不在链上**）、§11（状态口径）
- AGENTS.md §5（不在 main 外开分支/worktree/额外 clone；不宣布发布）、§6（目录落位）
- ENGINEERING_SPEC.md §7（仓库根固定条目——**未列 Wiki**，属该清单自身缺口，见 GAP-026）、§8（机器门）
- CONTROL_PACK_SPEC.md（任务卡与验收格式）

## 改动范围（文件域）

### 允许改
- 新建 `tools/wiki_export/**`（生成器脚本 + 模板 + 单元测试）
- 新建 `ci` 检查项：Wiki 与权威文档的一致性门（注册进 `ci/checks.json`，新增 `CHK-WIKI-SYNC`）
- 生成的页面产物落 `run/AstroCS.wiki-export/**`（不入库）
- **仅**在负责人批准后：写入 Wiki 仓库并 push（`git -C <wiki-clone> push`）

### 禁止改
- 不改 wiki 之外的任何既有文档内容（生成器只读权威文档）；
- 不改 `docs/science/**`、`docs/algorithms/**` 的公式与推导；
- 不在本任务里重建 `AstroCS.wiki/` 本地 clone 的跟踪关系（保持 `.gitignore` 忽略；要不要登记为根条目属 GOV-001 的目录决策）；
- 不在任务内删除或改写 Wiki 的既有页面（删除/覆盖由「推送」动作一次性完成，须批准）。

## 步骤

1. 用只读 clone 取得 wiki 现状（放 `run/AstroCS.wiki-restore/`，已存在于本次核查），逐页盘点上表；
2. 设计 Wiki 结构（建议）：`Home.md` 改为「导航入口 + 权威链声明（明确 Wiki 是**派生物**，唯一权威是仓库文档）」；按权威层级生成页面组——设计总纲 / 工程规范 / 控制包规范 / CI 规范 / 科学（docs/science）/ 算法（docs/algorithms）/ 插件（23 篇）/ 数据对象（UNIFIED_MODEL）/ 常见工作流（AGENTS.md）；
3. 写生成器：输入权威文档路径清单，输出 wiki 页面（Markdown，重写交叉链接，附「生成自 <路径>@<SHA>，请勿手工编辑」页脚）；
4. 产出 **dry-run**：把生成结果落 `run/AstroCS.wiki-export/**`，给出「新增/修改/删除页面清单 + 差异统计」，**先交负责人审阅**；
5. 新增机器门 `CHK-WIKI-SYNC`：校验 Wiki 当前内容与生成器输出一致（不一致即红），并要求生成器的页脚 SHA 与 HEAD 一致；
6. 负责人批准后：在 wiki clone 内提交并 push（这是**发布类动作**，AGENTS.md §8 必须授权）；push 后 `git ls-remote` 核对新 SHA 并记录到证据。

## 验收门（前台独立复跑）

- [ ] 生成器可复跑：给定 HEAD SHA，输出确定（同 SHA 两次生成逐字节一致）
- [ ] dry-run 页清单与「权威文档 → Wiki 页面」映射表完整（每个权威文档都有归属页面或明确「不发布」理由）
- [ ] `Home.md` 不再自称唯一权威；含权威链声明与「派生物」标注
- [ ] Wiki 中旧世代名词（HISS/HICS/Phase3 旧指代）零残留（给出 grep 计数前后对比）
- [ ] `CHK-WIKI-SYNC` 能红能绿（正例 = 同步后绿；负例 = 手工改一页后变红）
- [ ] push 后 `git ls-remote` 的 master SHA == 本次提交，且仓库内 `python3 ci/run_checks.py --check CHK-WIKI-SYNC` rc=0

## 禁止

- 未经负责人明确批准**不得 push 到 Wiki 仓库**；
- 不得把密钥/凭据/内部路径写进 Wiki 页面（含 FATDUCK 凭据、本机绝对路径）；
- 不得让 Wiki 成为第二事实源（任何页面不得声明自己是权威）；
- 不得把大型生成产物入库（页面产物落 `run/`，不入库）。

## 交付物

1. `tools/wiki_export/**` 生成器 + 测试；
2. `ci/checks.json` 新增 `CHK-WIKI-SYNC`；
3. dry-run 产物与页清单（`run/AstroCS.wiki-export/**`）；
4. 自证摘要（含 push 后的远端 SHA 证据）。