# 全项目问题搜寻作业规程（READ-ONLY SWEEP PROTOCOL）

- 代号：`RQS-PROTOCOL-001`
- 模式：**只读研究**。本文件对所有子代理具有约束力，不得自行放宽。
- 权威顺序：`ASTROCS_PROJECT_CONSTITUTION.md`（FROZEN，最高）> `AGENTS.md` >
  `docs/science/`（SCI 权威）> `docs/algorithms/`（ALG 权威）>
  `docs/standards/STANDARDS_REGISTRY.md`（国际标准冻结注册表）>
  `docs/contracts|interfaces|architecture`（L1）> 模块 README / module.yaml（L2）> 代码。
  根 `AstroCS_ENGINEERING_CONSTRAINTS.md` 为 ARCHIVED_NON_NORMATIVE 历史参照，与宪章冲突一律以宪章为准。

---

## 0. 硬性纪律（违反即该代理档案作废）

1. **禁止任何执行**：不得调用 shell/bash，不得构建、不得跑测试、不得跑脚本、不得执行 git 任何子命令
   （含只读子命令，因其可能触碰 `index.lock` 干扰并发 agent）。允许的工具只有：`read`、`grep`、`glob`、
   `write`（仅限写自己的档案）、`edit`（仅限改自己的档案）、`web_search`/`web_fetch`（仅限核对国际标准原文）。
2. **只写自己的档案**：叶子代理只能写 `问题扫描/_cache/<你的代理码>.md`；合并代理另可写
   `问题扫描/findings/**` 与 `问题扫描/_merge/**`。禁止修改仓库任何既有文件、代码、文档、测试、CI 配置。
3. **不改科学定义**：只登记问题，不修复、不重写公式、不调整容差。
4. **并发现场免报**：以下路径属其他 agent 正在作业的运行/构建现场，**一律不得作为问题登记**：
   `run/**`、`build/**`、`out/**`、`worktrees/**`、`Testing/**`、`logs/**`、`artifacts/**`、`evidence/**`、
   `reports/**`、`工程控制/**`、`GaiaDR3/**`、`GaiaDR3SP/**`、`BASS DR3/**`、`AstroCS.wiki/**`、
   `**/__pycache__/**`、`.pytest_cache/**`、`astrocs_p1sess_*/**`、`问题扫描/**`，以及根目录
   `astrocs_run_*.json`、`alloc_*`、`resource_*`、`worker_balance.csv`、`CS/`、`Database/`、`graph/`。
   影子树内 `run/**/lib/...` 之类的旧副本与真源不一致，也不得当作代码问题，只可在 §7 一句话备注。
   审计对象只有仓库真源：`lib/ include/ cli/ runtime/ providers/ modules/ tests/ tools/ scripts/
   contracts/ ci/ cmake/ packaging/ docs/ .github/` + 根治理文档 + `CMakeLists.txt`。
5. **禁止臆测**：无 `文件:行号` 证据 + 原文摘录的判断不得进入 finding，改放 §6「待复核」。

## 1. 问题类别（9 类；合并时按此建目录）

| 代码 | 类别 | 覆盖的问题 |
|---|---|---|
| `A_SCI_DEF` | 科学定义与推导 | 定义含糊/自相矛盾、推导跳步或错误、量纲/单位错误、统计假设缺失、误差传播不闭合、边界与奇点未定义、与所引文献定义不符、默认容差无出处 |
| `B_STD_MISMATCH` | 国际标准偏离 | 与 FITS WCS Paper I/II + SIP、IVOA HiPS 1.0 + properties 1.4、HEALPix（Górski 2005 NESTED）、Drizzle（Fruchter & Hook 2002）、Gaia DR3 data model、FITS 4.0 的实质偏离；偏离未登记偏差 ID；登记与实际不符；错误声称 COMPLIANT |
| `C_DOC_CODE_GAP` | 科学/算法文档 ↔ 代码 ↔ 接口割裂 | 文档写 A 实现做 B；签名/参数/默认值/返回语义与 ALG 不符；README/module.yaml/registry 与实现不符；错误码与状态字面量不一致；单位或坐标系在接口处丢失 |
| `D_COMMENT` | 注释不清晰 | 注释缺失或含糊、复述代码、堆积任务号/版本号/审计流水（违宪章 §12.2）、注释与代码矛盾、注释中的单位或公式错误、公共头缺前后置条件/所有权/线程安全/生命周期、注释掉的死代码 |
| `E_TRACE_BREAK` | 追溯与锚点断裂 | SCI/ALG/DATA/API/ARCH/SRC/TEST/EVIDENCE 断链或未显式 MISSING；文档行锚点漂移；TRACEABILITY 矩阵与真实文件不符；悬空引用（指向不存在的文件/符号/章节）；ID 重复或未经定义即被引用 |
| `F_TEST_GAP` | 测试与验证缺口 | 核心合同无独立 Oracle（宪章 §12.3-4、§13）；断言空洞/名不副实；测试容差与 SCI 冻结容差不一致；性质测试缺失；确定性与回归缺口；以跳过或豁免掩盖验证缺失 |
| `G_GOV_GATE` | 治理与门禁 | 违反宪章条款（§10.4 硬编码 workers/私有线程池、§10.1 ACR 生产不可达、§8.1 单入口、§3.2 三 Phase 不得隐式串接、§14 目录与原地演进、§16 版本、§17 门禁）；机器门禁项无实现或形同虚设；AGENTS.md 条款映射与宪章不一致 |
| `H_NUMERIC` | 数值与稳定性 | 科学路径精度降级（未达 float64 要求）、溢出下溢、NaN/Inf 传播未定义、静默夹紧或裁切、非确定性归约与随机种子、迭代不收敛处理、奇点与极点分支错误 |
| `I_DOC_HYGIENE` | 文档陈旧与可维护性 | 陈旧版本号或历史状态冒充当前状态、活动文档内部矛盾、缺单位/坐标系/像素语义声明、术语不统一、交叉引用断裂、复制粘贴残留与同名文档分叉 |

一个 finding 只归一个主类别（最贴近根因者），次要关联写入 `related`。

## 2. 优先级定义（P0/P1/P2）

- **P0（发布阻断；宪章 §17.10 要求 P0/P1 为零）**：会导致**科学结果错误或产品不合规**，或使科学声明失去可信证据。
  典型：公式或推导错误；单位/量纲错误；与冻结 SCI 或国际标准实质冲突且未登记偏差；文档与实现的行为差异会改变
  输出数值或产品结构；「已验证」声明无测试/Oracle/证据支撑；追溯链断裂使「科学定义=算法=接口=代码=测试」不成立；
  数值语义错误（NaN 静默、错误归一化、把 support/coverage 当科学权重）；门禁形同虚设导致上述问题无法被发现。
- **P1（须尽快修，暂不直接改变数值输出）**：一致性或可验证性破坏。典型：接口/契约/默认值与文档不符；容差与文档
  不符；缺独立 Oracle；锚点漂移或悬空引用；涉及科学量的误导性注释；并发与资源合同与实现不符；状态字面量与门禁
  工具冲突；模块 README/module.yaml 与实现不符。
- **P2（可维护性与表述）**：注释含糊或缺失、术语不统一、文档结构、陈旧叙述混入活动文档、交叉引用与拼写问题。

判定纪律：**类别不等于优先级**。注释错误若会误导科学实现可按 P1/P0；纯表述问题不得为凑数抬成 P0。
优先级由「是否改变科学结论或产品合规」决定。

## 3. 每条 finding 的必填 schema

```
### <代理码>-<三位序号> <一句话标题（≤40字，具体说清问题，禁止"存在问题"式空话）>
- 类别: A_SCI_DEF | B_STD_MISMATCH | C_DOC_CODE_GAP | D_COMMENT | E_TRACE_BREAK | F_TEST_GAP | G_GOV_GATE | H_NUMERIC | I_DOC_HYGIENE
- 建议优先级: P0 | P1 | P2
- 位置: <path>:<line>（多处逐行列出，必须真实可定位）
- 证据摘录:
  > 原文 1–6 行，逐字引用
- 权威依据: <宪章 §x.y / SCI-XXX-nnn / ALG-XXX-nnn / 标准条款（如 Paper I §2.1.1、HiPS 1.0 §4.2.1）/ DATA 合同 ID>
- 问题说明: <错在哪；与权威定义差在哪；写出正确的形式或应满足的条款>
- 影响: <影响哪个产品/数值/结论；真实数据路径是否受影响>
- 建议处置: <方向性建议，不改代码>
- 置信度: 高 | 中 | 低（低必须同时进 §6）
- related: <可选：其他 finding ID / SCI-ID / 偏差 ID>
```

## 4. 深度与覆盖要求

- 每条 slice 必须走完：读权威 SCI/ALG 文档全文 → 读对应实现与公共头 → 读对应测试 → 三方交叉比对。
- 至少 10 条 finding；确实不足时须在 §5「覆盖声明」逐条列出已检查文件与「未见问题」的判定依据。
  不得凑数造 finding，也不得少查。
- 必查六类典型割裂：注释不清晰；算法与文档割裂；科学定义与实现割裂；科学定义推导不当；与国际标准定义不同；
  声明与证据割裂（称已验证但无证据指针）。
- 特别关注：数值常数（容差、系数、阈值）是否有 SCI/标准出处；同一公式在 SCI、ALG、代码三处是否字面一致；
  文档中的 VERIFIED / COMPLIANT / PASS 结论是否有可定位证据。

## 5. 缓存档案文件头（必填）

```
# 扫描缓存 <代理码> — <slice 名称>
- 代理码: L03
- 覆盖文件: <逐条列出实际读过的文件>
- 未覆盖(原因): <文件 + 原因（体量/需运行/二进制）>
- 判定基线: 宪章 §... ；SCI-... ；ALG-... ；STD 域 ...
- finding 数: N（P0:x / P1:y / P2:z）
```

## 6. 待复核清单（不计入 finding 统计）

`- 线索 | 涉及文件 | 为何暂不能定性 | 上级应如何验证`

## 7. 现场冲突备注（可选，≤3 行）

只记「不作为 finding」的影子树或并发产物观察。

## 8. 交付与汇报

- 叶子代理：只写 `_cache/<代理码>.md`；最终回复报告：代理码、finding 数（P0/P1/P2）、最重要 3 条 P0 标题、
  待复核条数。不要把 finding 全文贴进回复。
- 合并代理：对每条 finding 重新 `read` 原文，核对 `文件:行号` 与摘录是否逐字吻合；不吻合者剔除或降级并写明理由；
  去重、改判优先级；最终写 `findings/<类别代码>/p<0|1|2>/<slice码>.md` 与 `_merge/<合并代理码>.md`
  （含剔除/降级/改判清单与二次验证结论）。
- 全程中文。任何代理不得改动仓库既有文件。

## 8. 执行层（E 层）授权边界 —— **负责人已授权 B-14（一次性）**

- **允许**：`git ls-files` / `git --no-optional-locks status` / `git rev-parse` 等**不改索引**的查询；对**已存在**产物做 `nm` / `objdump` / `readelf` / `strings` 只读检查；`ctest -N`（**仅列出**用例名）；读取既有 `CMakeCache.txt` / `CTestTestfile.cmake`；`find` / `grep` / `wc`；`python3` 做纯文本分析与本地重算。
- **输出**：只允许写入 `问题扫描/**` 与 **`run/审计执行层/E*/**`**（`run/*` 全 gitignore，符合根目录规范「禁止将运行产物产出到项目根目录」）。
- **禁止（一律不得触碰）**：任何编译器/链接器/make/ninja 调用；`cmake -B` / `--build` / 任何 configure；**运行**测试、二进制、CLI、脚本产生的构建；`git add/commit/checkout/stash/reset/clean`（提交只由前台在本目录路径上做）；写入或修改 `run/审计执行层/` 与 `问题扫描/` 之外的任何路径；连接 Fatduck/Windows 节点；删除或改名任何文件（含 `run/**` 内的 AGENTS.md —— **负责人已裁定不改名**）。
- **仍属无法判定、不得强行定档**：需 Windows/UCRT 真实行为者（B-05 rename 语义、B-08 longPathAware 的运行时效果、B-06 的 Windows 侧选择集）、需新构建 ASan/UBSan 后实跑者（B-09 —— **本轮只允许在既有 `build/asan/**` 与既有日志里找证据，不得新建构建**）、需真实数据端到端者（B-13）、需外网原文者（B-12）。
- **纪律**：E 层每条结论必须写「命令 + 输出摘录」；凡与静态推演结论不符，**以实测为准并标"改判"**；E 层**不改级别定义、不新增总述条目**，只做「无法判定 → 四态」的落地。