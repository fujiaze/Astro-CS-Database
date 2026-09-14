# 前台自证条目（producer=FD，负责人裁定：保留现状、仅登记为问题）

> 本文件的条目由**前台自己**取证并定稿（全部 M 域已收工，无人承接），编号 `FD-*`，与 `M*`/`L*` 同级引用。
> 取证手段：会话内一次 agent 指令注入收到全文 + 当场只读 `find`/`git ls-files` 清点，未修改任何被扫描文件。

### FD-G-001 影子树内的 agent 指令文件与冻结宪章在「唯一入口」上正面冲突，且对全部机器门永久不可见

- **ID**：FD-G-001
- **类别**：G_GOV_GATE
- **优先级**：**P1**，带「**依它提交代码即升 P0**」条件升级标签（与 M9 §6 同手法）
- **负责人裁定（本轮）**：**不做改名处置**，仅登记为问题；影子文件一律原地保留。
- **位置**：`run/reaudit_v3/run001/A/AGENTS.md::正式运行只有 orchestrator.exe`（207 行、UNTRACKED）；同文本第二份 `run/reaudit_v3/run001/B/AGENTS.md`（207 行）；另有 `run/arch_audit_p1/shadow_base`(78)、`run/arch_audit_p1/shadow`(57)、`run/scif2001/shadow`(57)、`run/rt001/base_shadow`(57)、`run/release-rescue/**` 七份 clean/clean-check*/base_src/mut/src/a4_src(各 78)、`工程控制/AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3/templates/AGENTS.md`(5)
- **问题说明**：全仓 agent 指令文件（AGENTS.md/CLAUDE.md/*.rules/.cursorrules）**18 份，仅根 `AGENTS.md`（78 行）1 份被 git 跟踪**，其余 **17 份 UNTRACKED** ⇒ 对 CI、对文档门、对 `git ls-files` 类闭包判据**同时不可见**。长度有 **207/78/57/5 四种**，影子 207 行版含根版没有的整节内容，构成**另一套规范**而非快照。其原文与本审计绝大多数条目的判定基线直接互斥：
  ①「正式运行只有 `orchestrator.exe <stage1.json>`」+「`.\toolchain.ps1 run <stage1.json>` 运行 orchestrator（**唯一正式入口**）」vs 宪章 §3.1（每平台一个用户可见入口）与 §8.1（CLI 只暴露一个薄的 `astrocs` 入口、不实现科学公式），根 AGENTS.md「单入口」要素同；
  ②「Python 生产层已删除」vs 根构建仍产 `astrocs_python_abi3`、`lib/astrocs_py/` 在 `add_subdirectory` 8 项内、`packaging/astrocs.product.json` 列 `bin/_astrocs.pyd` 为 required 单元；
  ③「项目**唯一权威文档**维护在 GitHub Wiki 仓库」vs §1.1 权威分层（宪章 > docs/science·algorithms·contracts > 其余），且根目录规范把 `AstroCS.wiki/` 列为**用户/资料区、禁改禁删的本地克隆**；
  ④隐含第二套状态与退出码面（DLL 由其相对路径加载、orchestrator 静态链接），与已定稿的 M8a-C-001（`lib/orchestrator/README.md` 固化第二套 `ASTROCS_*` 码表）同型不同主体。
- **影响**：它是**活的指令注入**（本会话即收到该文件全文，抬头为「These instructions apply to work under `run/reaudit_v3/run001/A`」）。任何在该路径下启动的 agent 会取得与根 AGENTS.md **相反**的"唯一正式入口"要素，并按 §8.1 明令禁止的方式实现入口，**且自认合规**；采信 ③ 则 §1.1 分层被文件系统的偶然性覆盖，本审计约 180 条依赖 §1.1 的条目在其视角下全部失去依据。**没有任何门会因两份 AGENTS.md 互斥而变红。**
- **依据条款**：宪章 §1.1（权威分层）、§3.1、§8.1、§12.3-10（状态与声明须同源）、§14.1；根 `AGENTS.md`「目录规范（强制）」与「单入口」要素
- **建议处置**（负责人已选"仅登记"，以下为留档建议）：①**不改名、不删除**（已裁定）；②改由**门**处理：新增机器门校验"被跟踪的 agent 指令文件必须与根 AGENTS.md 十要素 + §1.1 一致"，并对免报区出现的指令文件**只报告不阻断**（`check_comment_hygiene.py` 那类"写 json/md 但恒 return 0"的形态即 C-11 要治的病，新门不得重犯）；③在 `AGENTS.md` 目录规范中明文登记 `run/**` 内 `AGENTS.md` 的**非权威**地位，消除注入歧义（登记属条文修改，须 §1.2 流程）
- **取证口径**：`find`（含免报区）+ `git ls-files --error-unmatch` 逐份判 TRACKED/UNTRACKED + `wc -l` 计数；引文均为注入原文当场读取，非二手转述
- **置信度**：高（18 份清点、4 种长度、UNTRACKED 判定、四段引文全部一手）
- **related**：M8a-C-001（同型不同主体：README 面向人且被跟踪 vs AGENTS 面向 agent 且在免报区，**不并档**）、M6b-G-002（状态声明无单一事实源）、M6a §10（wiki 权威性待裁，本条提供实证）、M4-F-01/M2a-F-1/L23-004（构建孤儿形态，本条为**指令面同族**）、L25-005（豁免按字面目录名切分 = 换目录即逃检，与本条同根因）、C-10/C-07、F00-11
- **状态**：登记为问题，**不处置**（负责人 2026 裁定）；R 层不得因"未修复"重复计为残余风险之外的新条目

### FD-G-002 唯一执行「禁止运行产物落项目根目录」的门被永久容忍为红，且该检查自身声明 `waivable=False`

- **ID**：FD-G-002 ｜ **类别**：G_GOV_GATE ｜ **优先级**：**P1**（负责人裁定前不下 P0）
- **位置**：`ci/known_failures.json::failures[0]`（`check_id=UT-CLI`、`kind=check`、`category=WORKSPACE_HYGIENE`、`expected=fail`、`owner=SA-CLI`）；
  `ci/checks.json::UT-CLI`（同一 id 的登记项，实测字段 `waivable=False`、`profiles=["linux-main"]`、`mutates_workspace`、`dirty_ignore_exact`、`dirty_ignore_prefixes`）；
  根因面 `cli/commands.cpp`（基线 reason 字段自述产物默认落点）
- **问题说明（三条同时成立）**：
  ① 基线条目的 `reason` **逐字承认**：`astrocs graph` 产 `graph/*.json`、`memory-report` 产 `alloc_report.json`/`alloc_samples.csv`，
     「**根因在 cli/commands.cpp 产物默认落点（域外，非 tests/cli 断言面）**」——而根 AGENTS.md「目录规范（强制）」明文「**禁止将运行产物产出到项目根目录**」并规定     「发现根目录散落产物，整理归位…」。⇒ **一条强制禁止性条款的唯一执行门，处于版本化的常容忍红态**。
  ② **自相矛盾的登记**：同一 id 在 `checks.json` 里 `waivable=False`（声明不可豁免），却在 `known_failures.json` 里以 `expected=fail` 被长期容忍；
     而 AGENTS.md「状态机」要素明文「放行一律走 waiver 登记…且**严禁用 waiver 掩盖红灯（裁决 R-05/R-13）**」。
     基线合同的 `allowed_categories` 又把 `WORKSPACE_HYGIENE` 列为**可登记类** ⇒ **仓内两份权威文本对"这一类红灯能不能容忍"给出相反答案**，无仲裁条文。
  ③ 基线 reason 末尾还有一句被低估的话：「**UT-CLI 的 17 项过时断言修复前该检查不可绿**」⇒ 即使产物落点被修好，     该门仍因自身断言过时而不可绿。**一道"永不可绿 + 已被容忍"的门，同时充当着根目录污染的唯一防线**。
- **影响**：AGENTS.md 的目录规范在本仓库**没有任何有效机器执行**（这不是新增猜测，是基线文件的自述）；任何 agent 往根目录写产物都不会让 CI 变红，  因为该检查的红已被版本化接纳为 KNOWN。与本审计簇 1 的关系：它不是「结构性不会红」（那是 discover 0 用例、恒 return 0），  而是**「会红，但红已被制度化豁免」——同一失守的第二种机制**，且这一种更难被发现，因为它看起来完全合规（有 owner、有 reason、有 first_seen_commit、有 reproducer）。
- **依据条款**：AGENTS.md「目录规范（强制）」末段与「状态机」要素（R-05/R-13）；宪章 §12.3-10（声明与执行须同源）、§14.5（放行与 waiver）
- **建议处置**：①由负责人裁定 `WORKSPACE_HYGIENE` 到底可不可登记（可 → 删 AGENTS.md R-05/R-13 中与之冲突的一半；不可 → 把该条目移出基线并修 `cli/commands.cpp` 落点）；  ②无论哪向，**同一提交内**必须同时修 UT-CLI 的 17 项过时断言，否则该门仍在"永不可绿"状态；③建议新增机器门：`checks.json` 中 `waivable=False` 的 id 出现在 `known_failures.json` 即 FAIL（零假阳、可当天入门）
- **取证口径**：JS `JSON.parse` 直读 `ci/checks.json` **失败**（`read` 对 >2000 字符单行截断，position 42665/line 2000）⇒ 改用 `python3 json.load` 才拿到字段；  本报告内所有 checks/基线数字均由 python3 解析得出。**这是 H-1 陷阱对前台自己的第二次命中**（第一次是 M6b 的 CSV），已记入 R 层规则：机器文件一律用解释器解析，禁以 read 行文本解析结构化文件。
- **置信度**：高（两份 JSON 字段当场解析；引文为文件逐字内容）｜**related**：M5b-G-01（同名第二二进制，同族「门验错对象」）、M8-F-004（在册但不执行）、簇 1 机制补全、L25-005（豁免按字面量切分）、C-11/C-12

### FD-G-004 同一检查项在 `checks.json` 写 `waivable=false`、在证据里被记为 `SKIPPED(waivable)`：豁免状态无单一事实源（第二实例）
- **ID**：FD-G-004 ｜ **类别**：G_GOV_GATE ｜ **优先级**：**P1** ｜ producer=FD（由 E3 上报、前台当场复验）
- **位置**：`ci/checks.json::UT-CPU-AVX512`（前台当场 python3 解析：`waivable=False`；同族 `UT-CPU-BASELINE`/`UT-CPU-DISPATCH`/`UT-CPU-AVX2` **四项全 `waivable=False`**）；
  `evidence/**` 的 aud-ci-live 转述把 `UT-CPU-AVX512` 记作 `SKIPPED(waivable)`；跳过合同在 `tests/unit/CMakeLists.txt::cpu001_provider_selftest_avx512`（`SKIP_RETURN_CODE 77`，**按符号引用，行号已漂移**）
- **问题说明**：avx512 自测在缺指令集的机器上以退出码 77 主动跳过，`SKIP_RETURN_CODE 77` 使它在 CI 眼里是"正常的 Skipped"；
  而证据层把它写成"waivable"，配置层它明明是 `waivable=false`。⇒ **"这项到底被豁免了、还是合理地跳过了、还是仍然必须绿"三件事在两份文本里说法不同**，
  且跳过是**静默的**（Skipped 不计入失败），所以一台永远没有 avx512 的 runner 会永远"通过"CPU-001 的 avx512 面。
  这条与 FD-G-002 同一族（声明与执行面不符）但机制不同：FD-G-002 是"不可豁免项被基线容忍为红"，本条是"**可跳过被记账成可豁免**"，两者都是**豁免语义没有单一事实源**。
- **影响**：CPU provider 的 ISA 分派面（§10.2/§10.3）在无该指令集的环境上**没有任何有效验证**，而账面显示已覆盖；这与 M9 §4「平台盲区」是同一形态的 **ISA 维度版本**。
- **依据条款**：宪章 §10.2、§10.3、§12.3-10（状态与声明须同源）、§15.4（验证职责矩阵）；AGENTS.md「状态机」要素（waiver 登记唯一入口）
- **建议处置**：①统一豁免语义：`SKIP_RETURN_CODE` 类"环境性跳过"必须在 checks/registry 里有一个区别于 `waivable` 与 `PASS` 的第四态并计入覆盖率；
  ②机器门：`checks.json` 的 `waivable` 与证据层记账值不一致即 FAIL（零假阳、可当天入门）；③CPU-001 的 avx512 面须显式声明"仅在某 runner 上有效"，否则不得计为已覆盖
- **置信度**：高（四项 waivable 值当场解析；跳过合同按符号定位）｜ **related**：FD-G-002（同族第一实例）、M9.md §4（平台盲区总述，本条补 ISA 维度）、M5b-G-01（门验错对象）、簇 1 第三种机制、A-31 同批
