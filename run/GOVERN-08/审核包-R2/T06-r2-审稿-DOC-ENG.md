# T06 · 第 2 轮独立审稿 · DOC-ENG（车道 `docs/engineering/**`）

审稿人：独立子代理（未参与第 1 轮审稿，也未参与第 1 轮订正）。
**只审稿，不改任何文档**；全程无 git 写操作（git 仅只读；中文路径一律 `git -c core.quotepath=false`）。

权威链依据（按顺序通读）：`AGENTS.md`（127 行全文）→ `docs/ACSD_DESIGN.md`（568 行全文）→
`standards/03_READING_AND_ADVERSARIAL_REVIEW.md`（55 行全文）→ `standards/04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md`（71 行全文）。

**审稿结论：大修。** 订正轮总体方向正确，多处高质量修复（§2 表），但**本轮最高价值产出是「订正引入的新问题」共 8 条**，且第 1 轮最重的三条（R1 构建正本不在 git、U-2 run_manifest 三方不相交、U-1 命名块载体）**均未达成闭合**。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 本车道文档总数 | **63 份 md，10 128 行**（`find docs/engineering -type f -name '*.md' \| wc -l`；`find … -exec wc -l {} + \| tail -1`）。较第 1 轮（61 份 / 9 662 行）多 2 份 = 第 1 轮新建的 `standards/ERROR_MODEL.md`、`standards/COMMENT.md` |
| **我逐行读完的** | **12 份**：`contracts/MANIFEST_VERIFY.md`(117)、`contracts/LOG_AND_ERROR.md`(199)、`standards/ERROR_MODEL.md`(167)、`contracts/CONFIG.md`(415)、`contracts/PIPELINE_BLOCK.md`(127)、`contracts/SCHEDULER.md`(83)、`UNIFIED_OBJECTS.md`(146)、`data/ARTIFACTS.md`(135)、`architecture/DATA_FLOW.md`(186)、`resources/PERFORMANCE_MODEL.md`(280)、`governance/UNRESOLVED.md`(100)、`README.md`(29) |
| **我逐段通读并逐条对读的** | **6 份**：`testing/TEST.md`(280，§1/§3/§4/§4.1/§9.5/§9.6/§9.7)、`architecture/ARCHITECTURE.md`（平台与交付形态章）、`architecture/MODULE_MAP.md`（节点表 + 科学内核表）、`build/BUILD_NODES.md`（全 100 行）、`governance/TRACEABILITY.md`（SCI-REJ-001 行 + HTML 注记扫描）、`api/PUBLIC_API.md`（§C ABI 段 :128–161 + 悬空引用扫描） |
| **我未逐行读完的** | **45 份**，逐份列出（见 §1.1） |
| 子代理分头审 | **派出 6 个**（contracts+data、standards+resources+testing、architecture+governance+build、api+UNIFIED_OBJECTS 四条车道；其中前两条**我误发了重复派单**，各有两个互相独立的上下文在审）——见 §6 的诚实声明 |

### 1.1 未逐行读完的 45 份（逐份列出，诚实登记）

`contracts/`：`ASYNC_IO.md`(88)、`ATOMIC_PUBLISH.md`(292)、`CLI_PROTOCOL.md`(185)、`HIPS_STORAGE_FORM.md`(298)、`OWNERSHIP_LIFETIME.md`(34)、`RUNTIME.md`(65)、`README.md`(3)
`data/`：`ARTIFACT_STORE.md`(123)、`PHASE_PRODUCT_EXCHANGE.md`(368)、`PROVENANCE.md`(184)、`README.md`(4)
`architecture/`：`ARCHITECTURE.md`(162，其余章节)、`MODULE_MAP.md`(114，其余章节)、`README.md`(3)
`standards/`：`CACHE.md`(40)、`CODE.md`(126)、`COMMENT.md`(56)、`COMPATIBILITY.md`(29)、`CONCURRENCY.md`(44)、`DEPENDENCY.md`(47)、`DOCUMENTATION.md`(43)、`NUMERIC.md`(208)、`OPTIMIZATION.md`(25)、`README.md`(4)
`resources/`：`BENCHMARK.md`(15)、`cpu/BACKEND.md`(59)、`cpu/AVX2_PROVIDER.md`(132)、`cpu/CAPABILITY_PROBE.md`(107)、`cpu/ISA_VARIANTS.md`(238)、`cpu/README.md`(3)、`observability/RESOURCE_MONITORING.md`(190)、`observability/RUN_GRAPH.md`(171)、`observability/STRUCTURED_LOGGING.md`(176)、`observability/README.md`(2)、`resources/README.md`(3)
`testing/`：`VALIDATION_EVIDENCE.md`(575)、`README.md`(2)
`api/`：`PUBLIC_API.md`(2321，主体未逐行)、`abi/ABI.md`(117)、`abi/SECURE_LOADER.md`(118)、`README.md`(2)、`abi/README.md`(2)
`governance/`：`DOCUMENT_GOVERNANCE.md`(254)、`DUAL_LINE.md`(98)、`TRACEABILITY.md`(339)、`README.md`(3)
`build/`：`BUILD_GRAPH.md`(119)、`RELEASE.md`(139)

**未读部分的诚实边界**：对这 45 份我不宣称「逐行读完后的独立判断」。凡我给出的结论，都附**我自己复跑过的命令**或**我自己对读过的原文**，并在条目中标注核验深度。

---

## 2. 第 1 轮问题回查表（本车道相关，逐条）

判定口径：**对** = 方向正确且新引入错误 ≤0；**不完整** = 主干改对但存在漏改面或半改产生的新分歧；**错** = 方向或结论错误。

### 2.1 我判定「对」且质量较高的（20 条，均经我独立复跑验证）

| 意见 | 订正后状态 | 我的独立核验 |
|---|---|---|
| **1-2** LOG_AND_ERROR 双文档拼接 | **对** | 拆成 `contracts/LOG_AND_ERROR.md`(9 节) + `standards/ERROR_MODEL.md`(9 节)；更关键的是**订正者主动给两篇都加了 `## 1`–`## 9` 编号**（`git diff` 显示 9 个标题全部由无编号改为编号），使「错误对象与退出码映射」正好落在 `## 5` —— 这正是活合同 `LOG_AND_ERROR_CONTRACT.md §5` 要的位置。**我 `git show 128a1b00:…` 取旧表与新表逐行比对：8 行映射表体 + 退出码列全部逐字相同，零差异。** ✅ |
| **1-6 / 1-7** PIPELINE_BLOCK 两套 C 编号 | **对** | 改为 pipeline-carrier 侧 `C1/C2/C3/C3b/PC-C4..PC-C6` + IR 侧 `IR-C4/IR-C5/IR-C5b/IR-C6/IR-C7/IR-C8`；`:108` 的范围声明同步改成 `IR-C4–IR-C8`，C8 不再排在 C7 之上 |
| **1-10** HIPS_STORAGE_FORM「四件事」对 6 条 | **对** | `:18` 现写「本合同冻结四件事」并确为 4 项编号 |
| **1-11** MODULE_MAP 节点序 | **对** | `:62` 现为「校准、修饰、天体定位、星点与 PSF、测光、噪声信噪比、球面重采样、产品写出」；`python3 -c "json.load(registry)…"` 得 `['calibration','cosmetic','wcs-platesolve','star-psf','photometry','noise-snr','drizzle','writer']` —— **逐项一致** |
| **1-12** UNRESOLVED 双 `## 5` | **对** | 现为 `1/2/3/4/5/6/7` 单调；`## 2` 自报「（22 条）」与表内 22 行一致 |
| **1-14** README 目录表漏 UNIFIED_OBJECTS | **对** | 已补为表首行 |
| **1-4** PERFORMANCE_MODEL 自指 | **对** | 两处自指已消除 |
| **2-6** MODULE_MAP 漏 `lib/algorithms/noise_snr` | **对** | `:64` 路径表已含 `lib/algorithms/noise_snr` |
| **2-11** UPM 控制点权重三套公式 | **对（且我验到底）** | `data/ARTIFACTS.md:110` 改为两阶段：`① 分子 raw_w = quality_factor × control_ivar`（**几何可靠性不在分子**）＋ `② per-control 归一化 w_cell = raw_w / Σ_cell(raw_w) × control_reliability`。**我 `sed -n '170,190p' lib/algorithms/coverage/include/astro/phase2/upm.h` 逐字对读：「production … raw_w = quality_factor × control_ivar（几何可靠性在 per-control 归一化中施加）」+「per-control 归一化权重（raw/sum_j(raw) × control_reliability）」—— 与文档逐字一致。** ✅ 方向、倍数、适用域全对 |
| **2-12** integrate 权重退役口径被登记为现行 | **对** | `:108` 改为「单一权重口径 = 调用方构造的逐样本逆方差 `w = SNR²/F_ref² = 1/σ_F²`」，并明写「无 `weight_mode` 选择键、无权重口径枚举」「`weights=nullptr` 是本 C API 的输入合同，**不是可选权重口径**」 |
| **3-5** `I = max(1, L/min(n,F))` vs `L/F` 两条公式 | **对** | `DATA_FLOW.md:123` 补齐前提与不可互换条件：「当帧单元数 `n ≥ frame_workers` 时…等价于 `I = max(1, L/F)`；`n < frame_workers` 时必须回到原式，两条不可互换」。**我重推确认**：`in_flight = min(n,F)`，仅当 `n ≥ F` 时才等于 `F` ⇒ 前提正确 |
| **3-6** `TEST.md` 的 `u` 未定义 | **对** | `TEST.md §4.1` 给出 `u` = unit roundoff 及 `2⁻⁵³ = 1.1102230246251565e-16`、`2⁻²⁴ = 5.9604644775390625e-08`。**我用 Python 复核两个数值逐位相同** ✓ |
| **4-1** 通用容差单一正本 | **对** | `TEST.md §4` 立为唯一正本并给出三档取值；`SCHEDULER.md` 已回引；`PERFORMANCE_MODEL.md:18-20` 改为「第 4 节冻结的双精度非归约档容差…容差数值不在本篇复述」 |
| **6-7** 「标定形态」指代不明 | **对** | `PERFORMANCE_MODEL.md:162-168` 补齐 `L=16, n=2, K=2` 全参数与算例 |
| **6-8** 「最近一次实测墙钟」无存放面 | **对** | `TEST.md §9.5` 新增五行表（记录面/记录者/更新时机/取不到时的行为/跨面隔离），并给出「结果件各步墙钟字段，见 §9.7」的落点 |
| **8-6** L2 enforcement 文档越权 fail-closed | **对** | `PERFORMANCE_MODEL.md:192-208` 逐键给出 enforcement（1–3 = `record_and_justify`，4 = `hard_fail`），**并把 `enforcement_note` 的实质限定写进去了**：「85% 均值门在 16-worker 真负载上实测仅 65.09%，未标定前不得硬失败」，且明写「本篇不得把这四条整体表述为 fail-closed」。✅ 质量高 |
| **8-8** 探针事件 `tags` 等价展开 | **对** | 改为「在两份 schema 中都是**独立顶层字段**（本 schema 不存在 `tags` 属性，故不存在『等价展开』这种形态）」。**我用 `json.load` 复核 `scheduler_probe_event.schema.json` 无 `tags` 属性** ✓ |
| **9-1 / 9-2** PERFORMANCE_MODEL 历史叙事段 | **对** | V14/V17/V18R2 段已删，内嵌源码行号 `(I/O 与原子产品)（:151）` 随之消失 |
| **4-4 / 4-5** `kP1FrameBytesPerPixel = 116.0` 无来源；stripe 拐点无来源 | **对（我做了独立重推）** | `:90-107` 给出拟合式 `RSS(F) = 0.143 GB + F × 1.6724 GB`、残差 ≤±2.5%、件名 `run/P1-CONCURRENCY-CALIB-01/`、四个 `summary_F*.json`，并**明写 `B` 不取实测边际 99.68 B/px，而取「使闸门放行目标 F 且需求 RSS 不越 0.75A」的可行窗口上端**，窗口式 = `(0.75·A/(9P), 0.75·A/(8P)]`。**我的独立重推**：闸门 `F = ⌊0.75A/(P·B)⌋`，放行 F=8 ⇒ `B ≤ 0.75A/(8P)`；不放行 F=9 ⇒ `B > 0.75A/(9P)` ⇒ 窗口恰为文档所写，**公式正确**。**再反解**：`B = 116` ⇒ `A = 116×8P/0.75 = 20 759 008 597 B = 19.34 GiB`，与「取可行窗口上端」（上端 `= 116.0`，下端 `= 103.1`）**逐位自洽**。✅ stripe 拐点已降为「待标定的经验拐点，不是已标定的物理常数」，并从「不可改类」移出。✅ |
| **7-1 / 10-1** 51 条死路径 | **数字成立，取向对** | 我用**加后缀边界**的修正正则独立复跑（§8.2 命令），得 **39 条**，与订正记录一致。分布：cpu 14 / api 10 / contracts 9 / build 3 / data 3 |

### 2.2 我判定「不完整 / 错」的（9 条）

| 意见 | 判定 | 依据（含我的推导） | 建议改法 |
|---|---|---|---|
| **6-1** PIPELINE_BLOCK 内部自相矛盾（`:17` 不跨节点 vs `:37` `frame`（帧内跨节点）/`:54`「全部声明消费者用完」/`:60`「随块流转」） | **不完整 —— 且订正记录的说法不成立** | 订正记录 §6 U-1 写「我**未单方面改口径**，只在工程层**消除内部自相矛盾**」。**我 `git diff 128a1b00 6934b1a1 -- contracts/PIPELINE_BLOCK.md` 过滤 `不跨节点|跨节点|随块流转|consumers|lifecycle` —— 零命中输出，即这四行本轮一字未动**。`:17` 仍写「不落盘、**不跨节点**、不跨阶段」，`:37` 仍写 `frame`（**帧内跨节点**），`:13` 仍写「模块从帧读入参块、产出新块写回、显式声明**消费**的旧块」，`:60` 仍写「随块流转，下游逐项透传」。**自相矛盾原样保留** | 要么按 `:17` 删 `frame` 档与 `:60`/`:54` 的跨节点语义，要么按 AGENTS §7 + 最高设计 8.2 改 `:17`；**在 U-1 裁决前，文档必须显式写「本条与权威链上层冲突，已登记」而不是让两套语义并存** |
| **2-1 / U-3 / A6** 平台角色与最高设计相反 | **不完整 —— 只改了一半，且现在车道内部自相矛盾** | `ARCHITECTURE.md:121-125` 已对齐最高设计（「交付平台是 Windows 10+ amd64 与 Linux amd64 两个」「开发、构建、合成与真实数据终验在 **Linux amd64** 节点上完成，随后交 Windows 复验」）。**但 `build/BUILD_NODES.md:14/:16` 原样保留**：「Linux amd64 控制节点 \| 常在线控制、静态分析、轻量编译、小合成实验 \| **Windows 发布性能结论**」「**架构塑造方向是 Windows；Linux 侧的读数不作为 Windows 发布性能结论**」。`grep -rn "架构塑造方向是 Windows\|不作为 Windows 发布性能结论" docs/` → 仅此一处。⚠️ `docs/engineering/build/` 被 `.gitignore` 排除，故 `git diff` 看不到该目录的改动，第 1 轮订正记录 §13 把 `build/BUILD_NODES.md` 列为已改但无法用 diff 验证 | 同批把 BUILD_NODES §1 的节点分工表与两条结论改为与最高设计 §11 + ARCHITECTURE 一致；或明确声明 `build/` 不受 ARCHITECTURE 管辖（但那样两份正本仍互相矛盾） |
| **2-4 / 2-5** `provenance` 精度标 `integer`；`frame_snr` 标 `float32\|float64` | **不完整 —— 只改了一侧，引入同车道两文档分歧** | `UNIFIED_OBJECTS.md:48` 现为「非数值元数据（字符串/键值）」、`:41` 现为 `float64`，并新增 `:50` 的「精度列的读法」与 `:52` 的实现缺口登记 ✅。**但 `data/ARTIFACTS.md:62` 的 `DATA-OBJ-PROVENANCE-001` scalar 仍是 `int`、`:55` 的 `DATA-OBJ-FRAME-SNR-001` 仍是 `f32\|f64`** —— 同一对象在同一车道两份一级正本里给出**互斥**精度。canonical schema 侧：`provenance.schema.json` 与 `frame_snr.schema.json` 的 `precision` 枚举均为 `["float32","float64","integer"]`（我 `json.load` 逐个解析确认） | 同批修 ARTIFACTS 两行，并按 C23 同步 `unified/*.schema.json` 的枚举；或在两处都改为「见 canonical schema」并删本车道自填值 |
| **6-3** 容差正本循环引用（SCHEDULER ↔ DATA_FLOW ↔ CONFIG） | **不完整** | `SCHEDULER.md:33/:35` 已改为回引 `../testing/TEST.md` ✅。**但 `CONFIG.md:106` 仍写「数值差异只允许落在该面事前冻结的浮点容差内（`SCHEDULER.md` ）」** —— 形成 `CONFIG → SCHEDULER → TEST` 的两跳链，而中间那篇自述「本合同不复述」 | CONFIG 直接回引 `../testing/TEST.md`，删中间跳 |
| **5-6** 交叉引用节名被替换（schema 文件名/错节名） | **不完整** | `CONFIG.md:103` 现写「合同与不变式 F0/F1..F4/M1..M4 = `HIPS_STORAGE_FORM.md` 「**滤镜名匹配语义**」一节，设计 = `../../detail/PRODUCT_STORAGE_FORM.md` 「**滤镜名匹配语义**」一节」。**我 `grep -nE '^#{1,3} '` 两文件：`HIPS_STORAGE_FORM.md` 无「滤镜名匹配语义」节（真实节为 `## 形态的输入配置与输出清单字段` :190 与 `### 运行完成清单 manifest.json#storage（加性）` :238）；`docs/detail/PRODUCT_STORAGE_FORM.md` 也**根本没有这个节**（真实为 `## 10. 形态的输入配置与清单登记` / `### 10.1 输入：Phase1 的形态切换键`）**。即两个交叉引用**都指向不存在的节**，且节名串到了 `CONFIG.md:210` 自己的 `### 滤镜名匹配语义` 标题上（批量替换的串位） | 改为 `HIPS_STORAGE_FORM.md`「形态的输入配置与输出清单字段」一节 + `PRODUCT_STORAGE_FORM.md`「形态的输入配置与清单登记」一节 |
| **5-8** 参考文献表与正文脱钩（46/48 份零引用） | **部分对，但引入新错** | 正文 `[n]` 大面积补上了（UNRESOLVED §7、MANIFEST_VERIFY 等）✅。**但见 §3 N-1/N-3：PERFORMANCE_MODEL 补 `[2][3]` 时没有同步文献表，UNRESOLVED 给 `[3]` 条目尾部加了自引 `[3]`** | 见 §3 N-1、N-3 |
| **8-13** MODULE_MAP 把状态声明混写成映射 | **对（订正者的否决正确）** | 订正者否决「照字面删 `:52-55`」，只把状态断言改写成指向声明面的映射句。我核 `MODULE_MAP.md` 现无「尚未闭合/尚未落实现」类状态断言，保留了两处真实声明面 ✅ **我认可这次否决** |
| **1-3** CONFIG.md 三文档拼接 | **降级处置正确，但节号风险已发生** | 订正者降级为「同文件内切三块顶层章节」（配置面一/二/三），未拆文件。**风险已兑现**：C5 登记的「四份机器源按 `CONFIG.md §2/§3/§5/§10` 节号引用」——切块后 §2/§3/§5 已变为 `###` 级子节，而配置面二/三被抬到 `##` 级，**`§n` 编号语义整体漂移**（现文件里 `## 配置面一` 之下是 `### 权威链`/`### 三类配置`…）。按 C5 这条需代码侧/机器源同步 | 在 CONFIG.md 顶部显式声明「本文件不保留原 `§n` 编号，四份机器源的节号引用已失效」，并把机器源改指到带标题名的锚 |
| **R1** `docs/engineering/build/` 不在版本控制 | **未修（车道外），但处置方式有问题** | `git check-ignore -v` → `.gitignore:20:build/`；`git ls-files docs/engineering/build \| wc -l` → `0`；**仍为 0**。⚠️ **订正者的做法是「在 `build/README.md` 内如实登记」——但 `build/README.md` 自身也不在 git 里**：干净克隆上整个目录不存在，**这条登记永远读不到**。这是一条不可见的自证 | 与 §3 N-9 合并处理 |

---

## 3. 本轮新发现清单

### 3.1 「订正引入的新问题」（本轮最高价值产出，8 条）

**N-1 · `resources/PERFORMANCE_MODEL.md` 参考文献表重编号后漏两条，body 引用悬空**
- **位置**：`:6`（引 `[2]` = NUMERIC.md）、`:7`（引 `[3]` = BUILD_GRAPH.md）、`:275-280`（文献表只有 `[1] [4] [5] [6]`）
- **依据**：
  ```
  python3 -c "
  t=open('docs/engineering/resources/PERFORMANCE_MODEL.md',encoding='utf-8').read()
  body,_,refs=t.rpartition('## 参考文献')
  import re
  print('正文:',sorted(set(re.findall(r'\[(\d+)\]',body)),key=int))
  print('文献表:',sorted(set(re.findall(r'^\[(\d+)\]',refs,re.M)),key=int))"
  → 正文: ['1','2','3','4','5','6'] / 文献表: ['1','4','5','6']
  git diff 128a1b00 6934b1a1 -- …/PERFORMANCE_MODEL.md   # 参考文献节
  -[1] …ACSD_DESIGN.md…  -[2] …DATA_FLOW.md…  -[3] …NUMERIC.md…
  +[1] …DATA_FLOW.md…  +[4] …TEST.md…  +[5] …NOISE_SNR.md…  +[6] …ACSD_DESIGN.md…
  ```
  旧表 `[2]` DATA_FLOW 被重编为 `[1]`，旧 `[3]` NUMERIC 消失却没有占用 `[2]`；正文新增的 NUMERIC/BUILD_GRAPH 引用被编成 `[2]/[3]` 而文献表里这两号不存在。
- **建议改法**：文献表补 `[2] 内部文档 …/standards/NUMERIC.md` 与 `[3] 内部文档 …/build/BUILD_GRAPH.md`，或把正文编号改为 `[5][6]` 之后的连续号。

**N-2 · `contracts/MANIFEST_VERIFY.md:110` 引入重复短语 + 括号失衡**
- **位置**：`:109-111`
- **依据**：`sed -n '109,111p'` 原文逐字：`… manifest verify 全组 test_01..test_06 / **manifest verify 全组 test_01..test_06** / 独立 verify 命令返回 rc=2 test_07**）**，每组断言退出码。` —— 同半句重复两遍；且 `git diff` 显示改前为 `` golden: `eng/tests/cli/test_cli_protocol.py`（… ``，改后删掉了开括号 `（` 却保留闭括号 `）` ⇒ **单行 `（`=0 / `）`=1 失衡**。这是本轮机械替换的目标文件，属新引入。
- **建议改法**：删掉重复半句，补回开括号或删掉闭括号。

**N-3 · `governance/UNRESOLVED.md:100` 参考文献条目自引**
- **位置**：`:100`
- **依据**：`sed -n '100p'` → `[3] 内部文档 \`AGENTS.md\`，总章程与查证流程 [3]。` —— 条目自身编号 `[3]` 与尾部行内引用 `[3]` 重复。`git diff` 显示改前为 `[3] 内部文档 \`AGENTS.md\`，总章程。`（无尾部引用），而同节 `[1]`/`[2]` 条目**没有**被加尾部引用 ⇒ 三条写法不一致。这是补行内 `[n]`（第 1 轮 5-8）时误加在文献表条目自身上。
- **建议改法**：删尾部 `[3]`，保留条目编号。

**N-4 · 精度列在两份一级正本间分歧（2-4/2-5 只改一侧）**
见 §2.2 第 3 行。`data/ARTIFACTS.md:55/:62` 未随 `UNIFIED_OBJECTS.md` 同步。**这是「订正只改一侧 ⇒ 车道内部从『两处都不对』变成『两处互相矛盾』」的典型。**

**N-5 · 平台角色在两份正本间矛盾（2-1 只改一侧）**
见 §2.2 第 2 行。`build/BUILD_NODES.md:14/:16` 未随 `ARCHITECTURE.md` 同步。

**N-6 · `K = num_threads` 的结论强度在两份正本间矛盾（6-6 只改一侧）**
- **位置**：`resources/PERFORMANCE_MODEL.md:145` 对 `architecture/DATA_FLOW.md:125`
- **依据**：PERFORMANCE_MODEL 已降为「**充分条件（非定理陈述）**」，并给出 `num_threads = inner_omp` 的四条来源链（`DrizzleConfig::threads` 缺省 0 = `omp_get_max_threads`；`module_adapters.cpp` 无覆盖赋值；`drizzle_engine.cpp` 取值式；帧级 worker 进入 drizzle 前 `omp_set_num_threads(inner_omp)`）✅ 质量高。**但 DATA_FLOW:125 仍原样写**：「因此 `K = num_threads` 是达成满宽的**唯一最小取值**，且总份数与轴形态无关」。`git diff` 对 DATA_FLOW 的该行无改动。
- **我的推导**：`K ≥ inner_omp` 只给必要条件 ⇒ 最小可行 `K = inner_omp`；再要落到 `num_threads` 须先证 `num_threads = inner_omp`，该恒等式在 DATA_FLOW 内**未证**（DATA_FLOW 只说线程数经宿主回调注入）。故 DATA_FLOW 的「唯一最小取值」仍是第 1 轮 6-6 原文，未订。
- **建议改法**：DATA_FLOW:125 改为与 PERFORMANCE_MODEL §「scratch 池上限的派生」同措辞并回引；`K = num_threads` 的来源链只留一处。

**N-7 · `UNIFIED_OBJECTS.md` 章节编号乱序：4 → 4b → 4a → 5**
- **位置**：`:73 ## 4`、`:97 ## 4b`、`:116 ## 4a`、`:134 ## 5`
- **依据**：`grep -nE '^#{1,3} '` → `4b` 排在 `4a` **之前**。第 1 轮无此条目；`4a` 是第 1 轮订正新增的合同 ID 映射节，插入时落在 `4b` 之后。
- **建议改法**：`4a` 提到 `4b` 之前。

**N-8 · 同一条款归属在两份文档给出两个节名**
- **位置**：`contracts/CONFIG.md:167` 对 `contracts/MANIFEST_VERIFY.md:75`
- **依据**：CONFIG.md 写「**条款归属** = `HIPS_STORAGE_FORM.md` 的「形态的输入配置与输出清单字段」一节（字段词表与不变式 **M1..M4** 的唯一正本）」；MANIFEST_VERIFY.md 写「字段词表与不变式 M1..M4 的**唯一正本** = `HIPS_STORAGE_FORM.md`「运行完成清单 `manifest.json#storage`（加性）」一节」。**两个都是真节名**（`HIPS_STORAGE_FORM.md:190` 与 `:238`），但 M1–M4 定义在**子节 `:238`**；`ATOMIC_PUBLISH.md:198` 又用第三个写法。⇒ 同一个「唯一正本」在三处指向两个不同的节。
- **建议改法**：统一到 `HIPS_STORAGE_FORM.md`「运行完成清单 `manifest.json#storage`（加性）」一节（M1..M4 的实际所在），另三处回引。

---

### 3.2 本片最重一条：run_manifest 是否被硬凑成能过 schema（前台点名必查）

**结论：没有开后门 —— 但只在一份文档里做到，另一份仍然把未接线的 schema 当成活的。**

#### 3.2.1 做对的部分（`contracts/CONFIG.md`，我逐字核过）

`CONFIG.md:159-167` 是**正确的处置**，且正是我期待的方向：

- `:161` 明写「本节描述的是 … 冻结的 run_manifest 类字段名族（**预留合同面**）」
- `:163` 明写「**显式登记（两个不相交的 run manifest 对象）**：该 schema **未被任何生产写出点消费**。生产写出点是 `lib/infrastructure/cli/commands.cpp` 的 `write_run_manifest()`，它写的是**另一套字段名族**——`schema_version`/`kind`(`acsd_run_manifest`)/`run_id`/…/`summary`（失败时另加 `error`…），落盘为 `acsd_run_<run_id>.json`…该生产字段名族除 `run_id` 外**逐字段**落在本 schema 的 `additionalProperties:false` 拒绝面内。」
- `:165` 明写「两套词表**不合并、不互相改写**；哪一套是正本需负责人裁决」

**我用生产代码独立复算验证了 `:163` 的每一个键名**：
```
sed -n '528,560p' lib/infrastructure/cli/commands.cpp   # write_run_manifest
→ schema_version / kind / run_id / acsd_version / platform{os,arch} /
  config_path / config_sha256 / cpu_profile_path(nullptr) / cpu_profile_sha256(nullptr) /
  phases / artifacts / status / started_utc / finished_utc / summary
sed -n '560,562p'
→ if (status != "complete") m["error"] = {{"message", summary}};
sed -n '568p'
→ final_path = out_dir + "/acsd_run_" + ev.run_id() + ".json";
python3 -c "import json;d=json.load(open('eng/contracts/schemas/run_manifest.schema.json'));print(d['required'])"
→ ['manifest_schema','run_id','software_sha','config_hash','manifest_input_hashes',
   'manifest_output_hashes','toolchain_version','created_utc']，additionalProperties=false
```
**交集只有 `run_id`，文档的「逐字段落在拒绝面内」成立。** ✅ **这不是替 schema 开后门，方向正确。**

#### 3.2.2 没做对的部分（`contracts/MANIFEST_VERIFY.md`）—— 三条

**M-1 · `MANIFEST_VERIFY.md` 完全没有登记「两个不相交对象 / schema 未接线」**
- **位置**：全篇 117 行
- **依据**：`grep -n "预留\|未接线\|schema_path_reserved\|不相交\|reserved" docs/engineering/contracts/MANIFEST_VERIFY.md` → **0 命中**。而本篇是**《运行清单与校验合同》**，比 `CONFIG.md` 更应当承载这一登记。订正记录 §2.2 却声称「`CONFIG.md` 与 `MANIFEST_VERIFY.md` **均**如实登记『两个不相交的对象』与『schema 为未接线的预留合同』」——**该声称对 MANIFEST_VERIFY 不成立**。
- 更严重：`:78` 仍写「CFG-001 `eng/contracts/schemas/run_manifest.schema.json` 只登记该键位与类型」，`:90` 仍写「校验序→错误码：manifest **语法/schema**(3)→…」，**把该 schema 当成 verify 路径上的活的校验器**。
- **我的反证**：`sed -n '2383,2410p' lib/infrastructure/cli/commands.cpp` 的 `cmd_verify` 只做 `m.value("kind") != "acsd_run_manifest"` 与 `m.value("schema_version") != "1"` 两个字符串比较，**全文没有加载任何 JSON Schema**；`grep -rn "run_manifest.schema.json" lib/ eng/` → 0 命中。⇒ `:90` 的「schema(3)」是对生产行为的**不实陈述**。
- **建议改法**：在 MANIFEST_VERIFY 的 run_manifest 节补与 CONFIG.md:163 同款的「两个不相交对象」登记；把 `:90` 的「语法/schema(3)」改为「语法/`kind`+`schema_version` 判别(3)」；`:78` 加「该 schema 现阶段未被任何加载方消费」。

**M-2 · 第 1 轮订正者自己「补出」的 8-2a / 8-2b 两条，本轮一条都没改**
- **8-2a（文件名）**：`:24` 标题仍是 `## run_manifest.json v1`、`:85` 仍是 `acsd doctor --json --run-manifest <manifest.json>`，而生产落盘名是 `acsd_run_<run_id>.json`（`commands.cpp:568` 已核）。`grep -n "acsd_run_\|run_manifest.json" MANIFEST_VERIFY.md` → 只有 `:24`/`:27` 的 `run_manifest.json` 与 `acsd_run_manifest`（**kind 值，不是文件名**）。⇒ **文件名字面仍与生产不符**。
- **8-2b（键漏登记）**：`grep -n "summary" MANIFEST_VERIFY.md` → **0 命中**；JSON 示例（`:27-32`）既无 `summary` 也无 `error`，而生产**无条件写 `summary`**、`status != "complete"` 时**写 `error{message}`**。按文档实现的机器消费者会漏读失败原因。
- **建议改法**：示例块补 `summary` 与条件键 `error`；标题与命令面写实际落盘名 `acsd_run_<run_id>.json`（并说明 `run_manifest.json` 只是文档简称）。

**M-3 · CONFIG.md 的判定依赖「代码自洽」，但代码自洽这一点本身在文档里没有机器锚**
`CONFIG.md:163` 称生产「由同文件的 `inspect`/`resume` 读回」。我未逐一核（时间所限），但文档对「代码写 ⊇ 代码读」这一结论**没有给可复跑命令**，而这正是「代码侧正确、schema 侧错」三分支结论的承重墙。建议附一条差集命令。

---

### 3.3 「活合同逐字引用是否被订正打断」（前台点名必查）

**结果：一保住一没保住，而订正记录对后者给了不成立的 ✅。**

| 活合同 | 代码引处 | 订正记录声称 | 我的独立核验 | 判定 |
|---|---|---|---|---|
| `contracts/LOG_AND_ERROR.md` | `runtime_client.cpp:29,548`、`aio_disk_full.h:22`（引 `docs/engineering/LOG_AND_ERROR_CONTRACT.md §5`） | 「✅ §5 仍是第 5 节，表体**逐字未动**」 | `git diff` 显示 9 个 `##` 标题**新加了 `1`–`9` 编号**，「错误对象与退出码映射」正好落在 `## 5`；`git show 128a1b00:…LOG_AND_ERROR.md` 取旧映射表与新表**逐行比对：8 行表体 + 退出码列零差异** | ✅ **声称成立，修复质量高** |
| `contracts/SCHEDULER.md` | `memory_pressure.h:14`（引 `SCHEDULER_CONTRACT.md:14/:38`）、`memory_budget.h:12`（引 `SCHEDULER_CONTRACT.md §3`） | 「✅ §1 与 §3 **一字未动**」 | 条款文字确实未动（`git diff` 只改 `:33/:35` 的容差回引与 `:62` 的 `tags`）。**但 §1/§3 这两个编号在本文件里根本不存在**：`grep -cE "^## [0-9]" contracts/SCHEDULER.md` → **0**；本车道 26 份文档中只有 `LOG_AND_ERROR.md` 与 `ERROR_MODEL.md` 两份带 `## n` 编号。**对照历史**：`git show afe139c7:docs/contracts/SCHEDULER_CONTRACT.md` 显示旧文件**有** `## 1 总则` / `## 2 三阶段调度形态` / `### 2.1 并行确定性口径` / `## 3 资源声明`，且 `memory_budget.h` 引的「内存上限（峰值工作集）……由配置/资源门决定」**正是旧 §3 的原文** ⇒ 重写时把编号丢了，代码的 `§3` 因此悬空 | ❌ **「§1 与 §3 一字未动」的核验声称不成立**：保住的是句身，没保住节号；而节号正是代码引用的东西 |

**处置建议**：与 LOG_AND_ERROR 同法给 `SCHEDULER.md` 恢复 `## 1`–`## 6` 编号（其内容与旧 `SCHEDULER_CONTRACT.md` 的 1–6 一一对应：`总则`→1、`三阶段调度形态`（含 `并行确定性口径`）→2（2.1）、`资源声明`→3、`探针事件 schema`→4、`取消与原子性`→5、`负例`→6），即可让 `§1`/`§3` 重新解析；文件名的 `*_CONTRACT.md` 悬空属 C2 代码侧，已登记。

---

### 3.4 第 1 轮漏掉的问题（新眼睛，12 条）

**L-1 · `governance/UNRESOLVED.md:45`（裁-27）整格引用被批量替换成**伪造节名**
- **依据**（逐字）：该格出现「`../../ACSD_DESIGN.md` **本节.4** 写…，**更新规则一节.3** 写…」「本节.4 与 更新规则一节.3 同指 `docs/science/noise_snr/NOISE_SNR.md`」「**验证证据标准的纪律一节a** 第 3 条与 **验证证据标准的纪律一节b** 三口径适用域」
- **反证**：`grep -rn "验证证据标准的纪律\|更新规则一节" docs/ACSD_DESIGN.md docs/engineering/testing/VALIDATION_EVIDENCE.md` → **0 命中**。真实落点是 `docs/ACSD_DESIGN.md:127`（§2.4 P4「重建以噪声信号模型为物理前提：重建量随源亮度变化」）与 `:279`（§5.3「三者都产出同一物理量 `SNR = F_ref/σ_F` 的稠密表示」）。**「本节.4」「更新规则一节.3」「验证证据标准的纪律一节a/b」在任何文档里都不是节名。**
- **是否本轮引入**：**否**（`git show 128a1b00:…UNRESOLVED.md \| grep 裁-27` 已有同样字面）⇒ **第 1 轮漏检，第 1 轮订正未处理。**
- **建议改法**：逐处改回真实节名（`最高设计第 2.4 节` / `第 5.3 节` / `VALIDATION_EVIDENCE` 的真实节名）；同格的 `〔两字符不可辨识〕`（A12 占位）保持不猜，但需在该格加一句「本条原字符不可辨识，见 A12」。

**L-2 · `UNIFIED_OBJECTS.md:130` 破引用「…一节a 表」**
- **依据**：`grep -rn "一节a" docs/` → 仅此一处。原文：「本节 「归属归一与产品族字段级约束落点」**一节a 表**与 …」。`一节a` 是 `§4a` 被批量替换后的残渣，而 `## 4a`（:116）在该句所指位置的**上方**。
- **建议改法**：改为「本文件 §4a 的表」。

**L-3 · `UNIFIED_OBJECTS.md` 开头有**元信息块** + 节名自重复**
- **依据**：`:3-5` 是三行 `> ` 引用块（`上游：` / `上位正本：` / `现行对象集 =`），违反 AGENTS §5「**开头无元信息块**」；第 1 轮 9-7 只抓了 `CLI_PROTOCOL.md` 的 `> ID:…FROZEN`，漏了这一处。且 `:3` 与 `:5` 出现「`docs/ACSD_DESIGN.md` 「数据对象」一节**（数据对象）**」——节名后重复同名词（`grep -c "（数据对象）"` → 2）。
- **建议改法**：`:3-5` 改为正文段（与 `ARTIFACTS.md` / `ERROR_MODEL.md` 的「上游：…」单行写法一致）；删「（数据对象）」。

**L-4 · `UNIFIED_OBJECTS.md:50` 用「控制点」顶替了最高设计 3.3 的「manifest」，且与本文件 :43 自相矛盾**
- **依据**：`docs/ACSD_DESIGN.md:163` 原文：「稀疏与元数据（帧级信噪比、WCS 解、星表匹配、测光定标、**manifest**）全程双精度」。`UNIFIED_OBJECTS.md:50` 写成「…测光定标、**控制点**）承载 FP64」——**把 `manifest` 换成了 `控制点`**，这是对权威链顶点的改写。
- **且由此自相矛盾**：`:50` 宣布控制点承载 FP64，而 `:43` 把存控制点的 `sparse_snr_layer` 标 `float32|float64`；`:46` `validity`、`:47` `rejection` 也仍带 `integer`，而同段把「稀疏与元数据」整体判为 FP64。
- **建议改法**：`:50` 逐字照抄最高设计 3.3 的五项；精度列要么按该表逐对象判（则 `validity`/`rejection` 的 `integer` 需说明为何是状态量例外），要么整体回引 canonical schema。

**L-5 · `data/ARTIFACTS.md` 六处被批量替换吃光的引用残渣**
- **依据（逐条定位）**：`:15` 「无方差信息 = 0（显式不可用，**a）**」；`:22` 「「DataArtifact schema 清单」一节 非目标、**a**「gain 不在本层建模」」；`:23` 「见 `docs/science/unified/DATA_SEMANTICS` **a/**」；`:24`、`:25` 「编码权威 = `DATA_SEMANTICS` **a**」；`:32` 「in-memory(不落盘, **)**」
- **是否本轮引入**：**否**（`git show 128a1b00:…ARTIFACTS.md \| grep -c` 同为 6）⇒ 第 1 轮 5-6 漏检，未订。
- **另** `:14` 与 `:22` 引「「DataArtifact schema 清单」一节 的**非目标**」——本文件 `## DataArtifact schema 清单` 下**没有** `非目标` 子节。
- **建议改法**：逐处补回真实节名（`DATA_SEMANTICS` 的实际节）与 `DATA-IMG-*` 的登记面。

**L-6 · `contracts/CONFIG.md:229` 悬空闭括号 + 节名被吃**
- **依据（逐字）**：`platform:   ../build/BUILD_NODES.md 10+ amd64 / Linux amd64）` —— **无开括号、有闭括号**；节名整体消失。真实内容是 `BUILD_NODES.md` §1 节点分工表的「Windows x64 正式工具链节点 / Linux amd64 控制节点」两行。
- **建议改法**：改为 `platform: ../build/BUILD_NODES.md 的「1 节点分工」一节（Windows x64 正式工具链节点 / Linux amd64 控制节点）`，并与 N-5 一并把该节内容改为与最高设计一致。

**L-7 · `contracts/CONFIG.md:393` 正文带任务流水编号，且「7 档」与同文 3 档矛盾**
- **依据（逐字）**：`| rejection | … | p2_reject_stack_ex (**7 档自动选择 SD-18**: 1-3 none/4-5 percentile/N>=6 winsorized) | …`
- **双重问题**：(a) `SD-18` 是任务流水编号，AGENTS §5 明禁；(b)「**7 档**」与同行括号内只有 3 档、与 `:306-307`/`:355` 的三档路由矛盾。`grep -rn "SD-18" docs/` → 仅此一处。
- **代码侧我已核**：`lib/algorithms/coverage/src/rejection.cpp:1150-1170` 的 `acsd_n_map_method` 返回 `{none, percentile, winsorized_sigma}`，`n≥16` 由 `linear_fit` 改投 `winsorized_sigma` ⇒ **三档**，「7 档」无据。
- **建议改法**：改为「三档自动路由（1-3 none / 4-5 percentile / N≥6 winsorized_sigma）」，删 `SD-18`。

**L-8 · `contracts/CONFIG.md:355` 把排异路由阈值表误挂为最高设计的「唯一权威」**
- **依据**：`docs/ACSD_DESIGN.md:291-295` 的 5.5 节原文只有「路由依据 `N` = …按 N 自动选择排异算法；**生产算法集为 none、percentile、winsorized、linear fit**」——**不含任何 N 阈值**。而 CONFIG.md:355 写「**生产科学路由唯一权威** = 最高设计的「逐像素排异」一节：`1≤N≤3` none / `4≤N≤5` percentile / `N≥6` winsorized」。
- **附带遗留**：第 1 轮 2-9/O-9 已把 TRACEABILITY 改成「生产算法集 **4 种**（…linear fit）」以对齐最高设计，但代码的 AUTO 路由**从不返回 `linear_fit`**（`rejection.cpp:1169` 对 `n≥16` 也返回 `winsorized_sigma`），CONFIG.md:307 又写「线性拟合档不参与逐像素自动路由」。⇒ **「生产算法集 4 种」与「生产路由只用 3 种」这个 set-vs-route 区分在第 1 轮订正后**仍未被任何文档讲清**。
- **建议改法**：阈值表的权威改指 `lib/algorithms/coverage/src/rejection.cpp` 的 `kPixelSmallNPolicy` / `acsd_n_map_method`；并在 TRACEABILITY 与 CONFIG 同处一句话点明「生产算法集含 `linear_fit`（可显式指定），逐像素 AUTO 路由只出 3 档」。

**L-9 · `architecture/DATA_FLOW.md:142` 与 `data/ARTIFACTS.md:110` 对 UPM 归一化第二步口径不一致**
- **依据**：ARTIFACTS:110 完整写 `w_cell = raw_w / Σ_cell(raw_w) × control_reliability`；DATA_FLOW:142 只写「`raw_w = quality_factor × control_ivar` 冻结后按控制的 `sums[ck]` 归一」，**漏掉 `control_reliability`**。代码侧 `upm.cpp:691` 注释与 `upm.h:186-187` 均含该因子（`cfg.control_reliability` 默认 1.0，`upm.cpp:373/:392`），故 DATA_FLOW 在默认配置下不算错，但作为合同陈述不完整。
- **建议改法**：DATA_FLOW:142 补一步，或明写「本条只冻结归约顺序，权重公式以 `ARTIFACTS.md` 为准」。

**L-10 · `contracts/LOG_AND_ERROR.md:120-121` 承诺的「偏差逐条登记面」不存在**
- **依据**：原文「现行实现与本表的域映射偏差**逐条登记在错误模型的稳定错误码族登记一节**（登记不改码；未登记的偏差按判红处理）」。我读完 `standards/ERROR_MODEL.md §9 稳定错误码族登记`（`:149-155`）全文，该节只给了一个 `ERR-P2-UPM-001` 的**举例**，**没有任何偏差登记条目**（也没有登记表）。⇒ 一条「未登记即判红」的规则挂在一个空的登记面上，判红条件恒真。
- **建议改法**：§9 补一张实际偏差表（当前为空也要写明「当前 0 条」），或在 LOG_AND_ERROR 侧注明该登记面尚未建立。

**L-11 · 三处单行括号失衡（第 1 轮 9-12 类「句子破碎」的残留）**
- **依据**（我跑的全车道配平脚本）：`contracts/CONFIG.md:107`（`（`=2 / `）`=3，`s_out > 0）。原 …` 结尾多一个闭括号）、`contracts/CONFIG.md:125`（6/7）、`contracts/CONFIG.md:229`（0/1，见 L-6）。另 `MANIFEST_VERIFY.md:110`（0/1，见 N-2）。其余命中均为跨行折行（开括号在上行、闭括号在下行），非缺陷。
- **建议改法**：逐处配平。

**L-12 · R1 的处置方式产生「不可见的自证」，且索引把它登记成 active**
- **依据**：`git ls-files docs/engineering/build \| wc -l` → `0`；而 `docs/DOCUMENT_INDEX.yaml` 已登记 `BUILD_GRAPH.md` / `BUILD_NODES.md` / `RELEASE.md` 三条 active。订正者的动作是「在 `build/README.md` 内如实登记」——**该文件自身也不在 git**，干净克隆上整目录不存在 ⇒ **登记读不到**；同时索引门仍把这三份当作「存在且 active」，索引与文件树的双向比对**仍然恒绿**（索引侧有、树侧在干净克隆上也没有，两侧同时缺失）。
- **建议改法**：R1 必须走 `.gitignore`（锚到 `/build/` + `!docs/engineering/build/`），文档侧登记只能是辅助，不能替代。

---

### 3.5 残留项（第 1 轮已登记，本轮实测仍未闭合）

| 项 | 实测 |
|---|---|
| R1 构建正本不在 git | `git check-ignore -v` → `.gitignore:20:build/`；`git ls-files docs/engineering/build \| wc -l` → `0` |
| R2 判据载体失联 | 我复跑修正正则 → **39 条**真死路径（cpu 14 / api 10 / contracts 9 / build 3 / data 3），与订正记录一致 |
| R3 机械锚 | `grep -ro '」一节' docs/engineering --include=*.md \| wc -l` → **383**（第 1 轮 521）。⚠️ 本车道自订的 `docs/engineering/README.md:26` 明写「不使用『见第几节』式的跳转锚」，**383/383 与之冲突** |
| R3 空锚 | `grep -ro '「」'` → **0** ✅ |
| R3 HTML 订正注记 | `grep -rn '<!--' docs/engineering --include=*.md` → 仅 `build/BUILD_GRAPH.md` 的 6 个生成器机器标记（合法）；`governance/TRACEABILITY.md` **0** ✅ |
| R4 / U-1 命名块载体 | 见 §2.2 第 1 行与 L-1 |
| U-2 run_manifest | 见 §3.2 |
| 流水编号 | `grep -rnoE "\bV1[0-9]\b\|\bV2[0-9]\b\|\bR-[0-9]{2}\b\|\bP-[0-9]{2,3}\b" docs/engineering` → 集中在本轮**无人受派**的 8 份：`api/PUBLIC_API.md` 10 处（V13/V14/V16/V17）、`resources/cpu/ISA_VARIANTS.md` 3 处（R-60）；另有 `data/PHASE_PRODUCT_EXCHANGE.md` 等带 `[ID]` 形式的契约 ID（非流水号，不计） |
| 悬空 `[n]` 文献引用 | `python3` 全车道扫：3 份命中 —— `resources/PERFORMANCE_MODEL.md`（`[2][3]` 悬空，见 N-1）、`api/abi/SECURE_LOADER.md`（`[4][5][6]` 悬空，文献表只有 `[1][2][3]`）、`api/PUBLIC_API.md`（`[6][16][32][36][64][256][512][1024]` —— **判为数组维度/位宽写法，非文献引用，不计**） |

---

## 4. 本轮是否零发现

**本轮不是零发现轮。** 十轮均有新增实质问题，且本轮的核心产出是**「订正引入的新问题」8 条**——第 1 轮订正在多处方向正确、质量很高，但**存在系统性的「只改一侧」模式**（`UNIFIED_OBJECTS`/`ARTIFACTS` 精度、`ARCHITECTURE`/`BUILD_NODES` 平台角色、`PERFORMANCE_MODEL`/`DATA_FLOW` 的 `K=num_threads` 结论强度），每一处都在本车道内部**新造了一对互相矛盾的表述**。

| 轮 | 是否有新增实质问题 | 条数（本车道确认） |
|---|---|---|
| 1 结构 | **是** | 2（L-7、L-12） |
| 2 口径 | **是** | 4（L-4、L-8、L-9、N-4/N-5 归并计） |
| 3 公式 | **是** | 2（L-6 的 §n 悬空、L-7 的「7 档」） |
| 4 证据 | **是** | 1（N-1 文献表脱钩） |
| 5 引用 | **是** | 5（L-1、L-2、L-5、L-3、§3.5 悬空 `[n]`） |
| 6 推理 | **是** | 2（L-8 权威误挂、N-6 结论强度不一致） |
| 7 负向 | **是** | 1（L-10 恒真判红面） |
| 8 一致性 | **是** | 4（M-1、M-2、§3.3 SCHEDULER、§2.2 未闭合项） |
| 9 语言 | **是** | 3（L-3、L-11、N-2） |
| 10 可复现 | **是** | 2（L-12、M-1 的 schema 谎称） |

⇒ **收敛判据（规范 03 第 2 节「连续两轮无新增实质问题」）未达成。**

---

## 5. 我推翻的既有结论

1. **推翻订正记录 §1「`SCHEDULER.md` §1 与 §3 一字未动 ✅」。**
   我保住了**句身**，但保不住**节号**：该文件无任何 `## n` 编号（`grep -cE "^## [0-9]"` → 0），而代码引的正是 `§1`/`§3`/`:14`/`:38`。对照 `git show afe139c7:docs/contracts/SCHEDULER_CONTRACT.md`，旧文件**有** `## 1 总则` / `## 3 资源声明`，且 `memory_budget.h` 引的句子逐字出自旧 §3 ⇒ **节号是重写时丢的，不是订正时丢的，但订正记录的 ✅ 掩盖了它**。同一轮的 `LOG_AND_ERROR.md` 反而正确地补回了编号 ⇒ **两条活合同的处置不一致**。

2. **推翻订正记录 §2.2「`CONFIG.md` 与 `MANIFEST_VERIFY.md` **均**如实登记『两个不相交的对象』」。**
   `grep` 证实 MANIFEST_VERIFY.md 对「预留 / 未接线 / 不相交」**零命中**，且 `:90` 仍在说 verify 走 schema——而 `cmd_verify` 全文不加载任何 schema。⇒ 该声称对 MANIFEST_VERIFY **不成立**。

3. **推翻订正记录 §0.2 的对撞表「`build/**` 已改」。**
   §13 把 `docs/engineering/build/{README,BUILD_GRAPH,BUILD_NODES,RELEASE}.md` 列入「车道改动面」，但 `git diff 128a1b00 6934b1a1 -- docs/engineering/build/` **恒为空**（`.gitignore` 排除）。这本身不是错（记录里已注明），但它意味着 **`build/` 四份的改动在本车道内无法用 diff 复核**，而 `BUILD_NODES.md` 恰恰仍保留着与最高设计相反的平台角色 ⇒ **「已改」清单里混着一条实际未改的实质内容**。

4. **推翻订正记录 §8.1 终检表「HTML 订正注记…残 6」的说法需限定。**
   我实测 `governance/TRACEABILITY.md` 已 0 命中，车道全域仅剩 `BUILD_GRAPH.md` 的 6 个**生成器机器标记**（合法，非订正注记）⇒ 实质残留为 **0**，比记录所述更干净。

5. **推翻「第 1 轮 2-9 的 `7 种` 是同一位缺陷换个位置」这一潜在判断。**
   我原以为 `CONFIG.md:393` 的「7 档」就是第 1 轮 2-9 的残留；实测 `TRACEABILITY.md:329` 已改为 4 种、`CONFIG.md:306-307`/`:355` 已改为三档 ⇒ **`CONFIG.md:393` 是独立的、位于无人受派区域的另一处残留**，且它还多带一个流水编号 `SD-18`。

---

## 6. 子代理分头审与逐条复核

**派单事实（诚实登记）**：派出 **6 个**子代理，四条车道：

| 子代理 | 车道 | 状态 |
|---|---|---|
| 93bfc592 / 63c0b24d | `contracts/**` + `data/**`（**我误发重复派单**，两个独立上下文各审一遍） | 交付未回 |
| 366e56d7 / 390920b8 | `standards/**` + `resources/**` + `testing/**`（**我误发重复派单**） | 交付未回 |
| f8dfad02 | `architecture/**` + `governance/**` + `build/**` + 车道 README | 交付未回 |
| fd85572e | `UNIFIED_OBJECTS.md` + `api/**`（含 `PUBLIC_API.md` 2321 行） | 交付未回 |

**⚠️ 必须如实说明的两件事：**

1. **本轮我又犯了第 1 轮同款错误** —— 把两个 prompt 各发了两次（contracts+data、standards+resources+testing）。与第 1 轮不同，这两次是**我自己的重复**，不是前台造成的。这四个子代理因此提供了盲复核，但没有覆盖到第四、第五条车道。
2. **六个子代理的交付件在本审稿交付时均未回传**（`job_list` 返回空，无完成通知）。因此：
   - **本报告 §2、§3 的全部结论，没有一条采信子代理**；
   - **全部由我自己逐行读完 12 份 + 逐段通读 6 份 + 复跑命令得出**，每条附可复跑依据；
   - **「我否决了哪些子代理结论」这一栏本轮为空 —— 因为没有子代理结论可否决**。这不是「子代理都对了」，是**子代理这一路在本轮事实上没有产出**。前台若需要这一栏的实质内容，须重新派单并等待回传。

**我自己独立否决/改写的**（对照第 1 轮同口径的「自行否决」栏）：

| 我在过程中形成的初判 | 复核后 | 结论 |
|---|---|---|
| `s_pixel_scale = (180/π)·√(π/3)/nside` 少了因子 2 | 我先按 `√Ω = 2√(π/3)/nside` 算，代入 nside=2^18 得 `3.37e-4 deg/px`，与文档不符 | **推翻我的初判**：文档**正确**。`√(4π/(12nside²)) = √(π/3)/nside`（4/12 = 1/3，无因子 2）。量纲与数值均成立 → 记为**已验证项** |
| `B = 116.0` 与 `0.75A/(8P)` 对不上 | 我先按 A=32 GiB 代入得 192 B/px | **推翻我的初判**：反解 `A = B×8P/0.75 = 19.34 GiB` 得窗口 `(103.1, 116.0]`，`116.0` 正是上端 ⇒ **公式与取值自洽**，且与 `run/P1-CONCURRENCY-CALIB-01` 的实测边际 99.68 B/px 是两个不同的量（文档已明写不取实测边际） → 记为**已验证项** |
| `2-15`（`coordinate.frame` icrs/equatorial）在 `data/PHASE_PRODUCT_EXCHANGE.md` 未订 | 我核 `:93-94` 现仍写「frame 必须 `icrs`（唯一允许…）」 | **保留为未闭合**（已在订正记录 C22 登记为代码侧），本轮不重复计为新发现 |
| 「全文括号失衡」 | 第一次跑配平脚本命中 60+ 行 | **收窄为 4 行**（`CONFIG.md:107/:125/:229`、`MANIFEST_VERIFY.md:110`）：其余全是跨行折行（开括号在上一行、闭括号在下一行）的 markdown 排版，非缺陷 → **不编造问题凑数** |

---

## 7. 自证段

### 7.1 我实际做了什么

- 逐行读完 12 份 + 逐段通读 6 份（§1），其余 45 份**逐份列出**（§1.1），不宣称已读。
- **用 `git diff 128a1b00 6934b1a1` 逐文件比对第 1 轮审稿状态与订正后状态**，这是判定「改对了没有 / 有没有引入新错」的主证据；订正已落为提交 `6934b1a1`。
- **自己动手重推的公式与常数**（全部可复跑）：
  - 内存闸门可行窗口 `(0.75A/(9P), 0.75A/(8P)]` —— 由 `F = ⌊0.75A/(PB)⌋` 的「放行 8 / 不放行 9」两个不等式推出，**与 `PERFORMANCE_MODEL.md:104-105` 逐字一致**；并反解 `B = 116 ⇒ A = 19.34 GiB`。
  - `s_pixel_scale = (180/π)·√(π/3)/nside` —— 由 `Ω = 4π/(12nside²)` 推出，**公式正确**（CONFIG.md:77）。
  - `density = 1/(Δ_px·s_pixel_scale)²` 的量纲 —— `deg²` → `点/度²` ✓；代生产缺省 `Δ_px = 64`、`nside = 2^16` 得 ≈ 305 点/度²，自洽。
  - `E = Var_w/Var_opt − 1` 的尺度不变性 —— 复核仍成立（第 1 轮 V-8，本轮未被改坏）。
  - 排异 AUTO 路由 —— 读 `rejection.cpp:1150-1170` 逐分支核对：`n≤3→NONE`、`n<6→PERCENTILE`、`n≤15→WINSORIZED`、`n≥16→WINSORIZED`，**三档**，「7 档」无据。
  - IEEE 754 —— `2⁻⁵³ = 1.1102230246251565e-16`、`2⁻²⁴ = 5.9604644775390625e-08`，与 `TEST.md §4.1` 逐位相同。
  - `run_manifest` 三方键集 —— 从 `commands.cpp:528-562` 逐键抄出生产词表（含 `summary`、条件 `error`、`acsd_run_<run_id>.json`），与 schema 的 8 项 `required` 求交，得 `{run_id}`，验证 `CONFIG.md:163` 的「逐字段落在 `additionalProperties:false` 拒绝面内」。
  - 死路径 39 条、括号配平、悬空 `[n]` 文献引用 —— 三条都用脚本全车道复跑，数字可独立重现。
- **我没有做的事（诚实边界）**：
  - **没有编译、没有运行任何二进制**。所有「代码为准」的判定来自源码阅读 + `grep` + `json.load` 解析 schema。
  - **没有取任何外部文献原文**。本车道唯一外部文献是 `ARTIFACTS.md` 的 Rousseeuw & Croux 1993（DOI `10.1080/01621459.1993.10476408`）与 `CONFIG.md` 的 `[1]` IVOA REC-HIPS-1.0；**二者本轮均未取原文核对** —— `CONFIG.md:77` 声称「IVOA REC-HIPS-1.0 的『瓦片像素角尺度』一节 定义的同名键单位为度」，**该节名与该键我核对不到原文**（仓内无该文献台账），按红线**记为「核对不到」，不凭印象判定**，也不计入问题数。
  - **没有逐行读完** §1.1 列出的 45 份；对它们不做独立判断。
  - **没有采信任何子代理结论**（本轮无子代理交付回传，见 §6）。

### 7.2 关键复跑命令（可直接粘贴）

```bash
cd "/workspace/Astro CS Database"

# 基线：第 1 轮审稿态 vs 订正后
git -c core.quotepath=false log --oneline -3
git -c core.quotepath=false diff 128a1b00 6934b1a1 -- docs/engineering/contracts/MANIFEST_VERIFY.md
git -c core.quotepath=false diff 128a1b00 6934b1a1 -- docs/engineering/contracts/LOG_AND_ERROR.md | grep -E '^\+#|^-## '

# 活合同：节号是否保住
grep -cE "^## [0-9]" docs/engineering/contracts/SCHEDULER.md      # 0  ← 无编号
grep -nE "^#{1,3} " docs/engineering/contracts/LOG_AND_ERROR.md    # ## 1..## 9
grep -nE "^#{1,3} " docs/engineering/standards/ERROR_MODEL.md
git -c core.quotepath=false show afe139c7:docs/contracts/SCHEDULER_CONTRACT.md | grep -nE "^#{1,3} "
sed -n '14p;38p' lib/include/acsd/core/memory_pressure.h
sed -n '12p'      lib/include/acsd/core/memory_budget.h
# 映射表逐字比对
git -c core.quotepath=false show 128a1b00:docs/engineering/contracts/LOG_AND_ERROR.md \
  | sed -n '/^| `ErrorDomain` | 退出码/,/^$/p'

# run_manifest 三方
python3 -c "import json;d=json.load(open('eng/contracts/schemas/run_manifest.schema.json'));print(d['required']);print('addProps',d['additionalProperties'])"
sed -n '528,572p' lib/infrastructure/cli/commands.cpp
sed -n '2383,2410p' lib/infrastructure/cli/commands.cpp
grep -rn "run_manifest.schema.json" lib/ eng/          # 0 命中
grep -n "预留\|未接线\|不相交\|acsd_run_\|summary" docs/engineering/contracts/MANIFEST_VERIFY.md

# 6-1 未改（PIPELINE_BLOCK 自相矛盾）
git -c core.quotepath=false diff 128a1b00 6934b1a1 -- docs/engineering/contracts/PIPELINE_BLOCK.md \
  | grep -E "^[+-].*(不跨节点|跨节点|随块流转|consumers|lifecycle)"     # 空 = 未改
sed -n '13p;17p;37p;54p;60p' docs/engineering/contracts/PIPELINE_BLOCK.md
python3 -c "import json;print(json.load(open('lib/infrastructure/pipeline/module_ports.registry.json'))['carrier_contract']['statement'][:120])"

# 2-1 只改一半（平台角色）
git -c core.quotepath=false diff 128a1b00 6934b1a1 -- docs/engineering/architecture/ARCHITECTURE.md | grep "^[+-].*平台"
grep -rn "架构塑造方向是 Windows\|不作为 Windows 发布性能结论\|常在线控制" docs/
sed -n '291,295p' docs/ACSD_DESIGN.md

# 2-4/2-5 只改一半（精度）
grep -n "DATA-OBJ-PROVENANCE-001\|DATA-OBJ-FRAME-SNR-001" docs/engineering/data/ARTIFACTS.md | cut -c1-120
sed -n '41p;43p;48p;50p' docs/engineering/UNIFIED_OBJECTS.md | cut -c1-160
python3 -c "import json;print(json.load(open('eng/contracts/schemas/unified/provenance.schema.json'))['properties']['precision'])"

# 6-6 只改一半（K = num_threads）
sed -n '125p' docs/engineering/architecture/DATA_FLOW.md | cut -c1-300
sed -n '143,160p' docs/engineering/resources/PERFORMANCE_MODEL.md | cut -c1-160

# L-1 伪造节名 / N-2 重复短语 / N-3 自引
sed -n '45p' docs/engineering/governance/UNRESOLVED.md | cut -c1-400
grep -rn "验证证据标准的纪律\|更新规则一节" docs/ACSD_DESIGN.md docs/engineering/testing/VALIDATION_EVIDENCE.md   # 0
grep -rn "一节a" docs/
sed -n '109,111p' docs/engineering/contracts/MANIFEST_VERIFY.md
sed -n '100p'    docs/engineering/governance/UNRESOLVED.md

# L-5 ARTIFACTS 残渣
grep -noE ".{18}(，a）|、a「| a/|= \`DATA_SEMANTICS\` a|不落盘, \))" docs/engineering/data/ARTIFACTS.md
git -c core.quotepath=false show 128a1b00:docs/engineering/data/ARTIFACTS.md | grep -coE "(，a）|、a「| a/|DATA_SEMANTICS\` a|不落盘, \))"   # 6 → 早已存在

# L-6 / L-7 / L-8 / L-11
sed -n '229p;393p;355p' docs/engineering/contracts/CONFIG.md | cut -c1-300
sed -n '1150,1170p' lib/algorithms/coverage/src/rejection.cpp
python3 - <<'PY'
import os
for dp,dn,fn in os.walk('docs/engineering'):
    for f in sorted(fn):
        if not f.endswith('.md'): continue
        p=os.path.join(dp,f)
        for i,ln in enumerate(open(p,encoding='utf-8').read().splitlines(),1):
            if ln.count('（')!=ln.count('）'): print(f"{p}:{i}  {ln.count('（')}/{ln.count('）')}")
PY

# L-9 UPM 归一化两阶段
sed -n '173,190p' lib/algorithms/coverage/include/astro/phase2/upm.h
sed -n '110p' docs/engineering/data/ARTIFACTS.md | cut -c1-400
sed -n '142p' docs/engineering/architecture/DATA_FLOW.md

# L-10 空的偏差登记面
sed -n '149,156p' docs/engineering/standards/ERROR_MODEL.md

# N-7 章节乱序 / L-3 元信息块
grep -nE "^#{1,3} " docs/engineering/UNIFIED_OBJECTS.md
sed -n '3,5p' docs/engineering/UNIFIED_OBJECTS.md | cut -c1-200

# 车道全域指标
grep -ro '」一节' docs/engineering --include=*.md | wc -l   # 383
grep -ro '「」'    docs/engineering --include=*.md | wc -l   # 0
grep -rn '<!--'   docs/engineering --include=*.md           # 仅 BUILD_GRAPH 生成器标记
git -c core.quotepath=false ls-files docs/engineering/build | wc -l   # 0

# 悬空 [n] 文献引用
python3 - <<'PY'
import os,re
for dp,dn,fn in os.walk('docs/engineering'):
    for f in sorted(fn):
        if not f.endswith('.md'): continue
        p=os.path.join(dp,f); t=open(p,encoding='utf-8').read()
        if '参考文献' not in t: continue
        body,_,refs=t.rpartition('参考文献')
        miss=sorted(set(re.findall(r'\[(\d+)\]',body))-set(re.findall(r'^\[(\d+)\]',refs,re.M)),key=int)
        if miss: print(p,"悬空",miss)
PY

# 39 条死路径（加后缀边界的修正正则）
python3 - <<'PY'
import re,os
root=os.path.abspath('.')
pat=re.compile(r'`((?:eng|lib|docs|run|实验)/[A-Za-z0-9_./\-]*\.(?:md|json|py|yaml|yml|h|hpp|c|cpp|cmake|txt|js))`|'
               r'`((?:\.\./|\./)[A-Za-z0-9_./\-]+\.(?:md|json|py|yaml|h|hpp|c|cpp|txt))`')
bad={}
for dp,dn,fn in os.walk('docs/engineering'):
    for f in fn:
        if not f.endswith('.md'): continue
        p=os.path.join(dp,f); base=os.path.dirname(os.path.abspath(p))
        for i,line in enumerate(open(p,encoding='utf-8',errors='replace').read().splitlines(),1):
            for a,b in pat.findall(line):
                m=(a or b).split('#')[0]
                c1=os.path.normpath(os.path.join(base,m)) if m.startswith('.') else os.path.normpath(os.path.join(root,m))
                if not os.path.exists(c1): bad.setdefault(m,[]).append(f"{p}:{i}")
print("死路径:",len(bad))
PY
```

### 7.3 收敛声明

**本车道未达成收敛。** 第 1 轮订正在 20 条上做了高质量修复（尤以 1-2 的节号恢复、2-11 的两阶段权重、8-6 的 enforcement、4-4 的 116.0 来源链为佳，我逐条独立复核确认），但**没有触及三条根因**（R1 构建正本不在 git、R2 判据载体缺失、R4/U-1 命名块载体与权威链上层相反），且**新引入 8 条实质问题**，其中 N-4/N-5/N-6 是同一种「只改一侧 ⇒ 车道内部新造矛盾」的重复模式。

**下一轮建议顺序**：
1. **收「只改一侧」的三对**（N-4 精度 / N-5 平台角色 / N-6 `K=num_threads`）——同一批改完，不要再分批；
2. **补 `SCHEDULER.md` 的 `## 1`–`## 6` 编号**（与 LOG_AND_ERROR 同法，5 分钟可闭合一条活合同）；
3. **M-1/M-2**：把 CONFIG.md:163 的「两个不相交对象」登记同步到 MANIFEST_VERIFY，并补 `summary`/`error`/真实落盘文件名；
4. **L-1/L-2/L-5/N-2/N-3/N-1**：批量替换残渣的六处清理（纯机械、可脚本化，但需人读确认每处的真实节名）；
5. **R1 走 `.gitignore`**（文档侧登记不能替代）；
6. **重新派单并等待回传**（本轮 6 个子代理交付件未回，见 §6）。

---

## 8. 交付件

唯一交付件：`/workspace/Astro CS Database/run/GOVERN-08/审核包-R2/T06-r2-审稿-DOC-ENG.md`（本文件）。
**未改动任何文档**；全程无 git 写操作。