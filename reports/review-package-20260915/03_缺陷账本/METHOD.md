# METHOD · 口径、判定规则与复现

本文件说明 @digest-findings@ 这一包的**口径**、**处置态判定规则**、**计数复现命令**与**与 INDEX 的对账结果**。所有数字都可由本目录 @_tools/@ 下的脚本重跑得到；本包只读 @问题扫描/**@，只写 @run/review-package/digest-findings/@，不改主工作树任何既有文件，不做任何 git 操作。

## 一、目标与边界

- 目标：把 @问题扫描/@ 的缺陷体系整理成一份**逐条可落点**、**处置态有据**的账本，供项目负责人审核。
- 条目全集 = @问题扫描/findings/<类别>/p<0|1|2>/*.md@ 中全部 @##@ / @###@ 条目标题，抽取口径与官方账本生成器 @问题扫描/_tools/gen_fix_ledger.py@ 的 @ID_RE@ **逐字节相同**。
- 不重抽、不改判、不合并、不删除任何 @id@；@DEFECT_LEDGER.json@ 保留每条 @merge_state@ / @merge_sources@ / @ledger_fix_state@ / @ledger_verified_state@ / @owner_decision@ 原始列，任何人可据以重算。

## 二、权威层级（本包严格遵守）

1. **单条事实的判词**：@findings/<类别>/p<N>/*.md@ 正文 + 对应 @_merge/M*.md@ 的**逐条处置表**为准。
2. @INDEX.md@：只作导航与规模核对，**不改判**。
3. @SUMMARY.md@：只做跨域综合，**不重述、不改判**。
4. @账本/FIX_LEDGER.csv|jsonl|md@：修复台账（判定列只读；@fix_*@ / @verified_*@ 为处置列）。
5. @40_OWNER_DECISIONS.md@：需负责人裁决/授权事项（A/B/C/D 系列）。

本包**新增判定为零**：处置态是上述第 1 项（@_merge@ 四态）与第 4 项（@FIX_LEDGER@ 已填处置列）的**机械映射**，映射规则见 §四。

## 三、条目抽取口径

- 正则（与 @gen_fix_ledger.py@ 相同）：@^#{2,4}\s+((?:M\w+|L\d+b?c?d?e?|FD|V\d+|W\d+|SA)-[A-Z]{1,4}\d*-\d+)\b@。
- 去重：同 @id@ 只保留首次出现；@README.md@ 不计。
- 类别 = @findings/<类别>/@ 目录名（@path_category@）；**目录优先级** = @evidence_file@ 中的 @p0/p1/p2@（@path_priority@）；**账本优先级** = @FIX_LEDGER.priority@（判定列，决定 @release_blocker@）。@render.py@ 同时给出两套口径。
- 产出方（@producer@）= 按 @gen_fix_ledger.py@ 的规则从文件名前缀取（如 @M7.md→M7@、@V12-b.md→V12-b@）。

## 四、处置态判定（5 态）与优先级规则

处置态取值固定为：@需负责人裁决@ / @已修@ / @无法复现@ / @判定非缺陷@ / @已登记待修@。判定按以下**有序**规则（命中即停）：

| 序 | 条件 | 结果 |
|---|---|---|
| 1 | @FIX_LEDGER.verified_state ∈ {NOT_A_DEFECT, REJECT-NEVER-EXISTED}@ | 判定非缺陷 |
| 2 | @verified_state ∈ {CANNOT_REPRODUCE, CLOSED-ANCHOR-DEAD}@ | 无法复现 |
| 3 | @verified_state = MOVED@（本 id 并入他条） | 判定非缺陷 |
| 4 | @_merge@ 四态 = @判定非缺陷@（整条剔除/撤条/不另立） | 判定非缺陷 |
| 5 | @_merge@ 四态 = @无法判定@（需执行/权限） | 无法复现 |
| 6 | @owner_decision@ 非空，或条目正文（标题/影响/建议处置）提请负责人裁决 | 需负责人裁决 |
| 7 | @verified_state ∈ {FIXED, VERIFIED, FIXED-PARTIAL, FIXED-RESIDUAL}@ 或 @fix_state ∈ {FIXED, PARTIAL}@ | 已修 |
| 8 | @_merge@ 四态 = @已被修复@ | 已修 |
| 9 | 其余（@仍成立@ / @部分修复@ / @移交@ / 无处置表） | 已登记待修 |

- 规则 6 排在 7 之前：若一条既已修又挂着负责人裁决编号，本包记 @需负责人裁决@，并在 @disposition_reason@ 里注明“账本另记已修”，以免把待裁决项藏在“已修”里（仅 2 条命中此情形）。
- @部分修复@ 与 @FIXED-PARTIAL@ 一律计入 @已修@，残余事实在该条的 @merge_sources@ / @disposition_reason@ 中可见；**残余是否另立条目由 @_merge@ 原文决定**，本包不代立。

## 五、@_merge@ 处置表解析口径

- **主处置表定位**（逐文件，只取“逐条处置/判定/复验”主表，不取剔除/降级辅助表的散文）：
  @M1a/M2b/M3/M3b/M4/M5a/M6a/M6b/M8a@ = “逐条处置表”；@M2a@ = “A.1 仍成立”+“A.3 部分修复”；@M5b@ = “1A 仍成立”+“1C 部分修复”；@M7@ = “四态判定表”；@M8@ = “逐条复验表”。
- **列识别**：表头含 @定稿|最终|新条目|残留事实@ 的列取为“定稿去向”，含 @四态|判定|^态$|处置@ 的列取为“状态”。@M3@ 的“本轮新增”表以首列 @新条目@ 为 id 列。
- **表格切分**：同一段落内**忽略空行**收集 @|@ 行，按分隔行 @|---|@ 定位每张表的表头，避免把被空行切成两半的表丢掉行（@M1a.md@ §一表即被空行切成两段）。
- **四态归一化**：@①仍成立 ②已被修复 ③部分修复 ④无法判定@；文本 @仍成立/部分修复/已被修复/移交/无法判定@ 归一到同名；@前提翻转（修复未回写）@ → 部分修复；@合并/并档/归并/重锚/新增@ → 仍成立；@整条剔除/撤条/不另立/剔除表@ → 判定非缺陷。**“半边剔除/子证据剔除/锚剔除”不算整条剔除**（只认“定稿去向”列的整条判词，不认状态列里的半句）。
- **聚合**：同一 @id@ 出现在多行时按 @仍成立 > 部分修复 > 已被修复 > 移交 > 无法判定@ 取“最需处置”的一档；@merge_sources@ 保留全部来源行。
- **交叉域 id 不外借**：每个 @M*.md@ 只接收与本文件同前缀的 id，避免把行文里引用的他域 id 误挂状态。

## 六、计数复现命令

~~~bash
cd "/workspace/Astro CS Database"
# 独立核对：findings 可抽取 id 唯一数（应打印 291 份 / 785 条）
python3 run/review-package/digest-findings/_tools/count_ids.py
# 抽取 + 处置态判定（只读 问题扫描/**，只写本目录）
python3 run/review-package/digest-findings/_tools/extract.py
# 渲染 DEFECT_LEDGER.md 与 DEFECT_STATS.md
python3 run/review-package/digest-findings/_tools/render.py
~~~

@extract.py@ 打印的两行一致性自检必须为：@CSV 有 / 重抽无 = 0 ; 重抽有 / CSV 无 = 0@，且 @findings .md 文件数=291  抽取 id 数=785  FIX_LEDGER 行数=785@。

## 七、与 @INDEX.md@ 的对账（81/306/133 → 520）

- @INDEX.md@（2026-09-14 11:45）自述：@224 份定稿件 / 520 条标题@，@P0 81 / P1 306 / P2 133@，产出方仅 @M1a..M9 + FD + L28b..L28e@。
- 现树（2026-09-15）：@findings/**@ 非 README 的 @.md@ = **291 份**，可抽取 @id@ = **785 条**，类别 **11 个**（多出 @C_ALG_IMPL@、@J_FS_PUBLISH@）。
- **同产出方子集**（只取 INDEX §二列出的 19 个产出方）= **521 条**，目录优先级 **82 / 305 / 134**：与 INDEX 的 520（81/306/133）逐格对比，仅两处差异 —— @F_TEST_GAP@ P0 +1（新增 @FD-F-003@）、@G_GOV_GATE@ P1 −1/P2 +1（一条 G 条目在快照后由 @p1@ 目录移入 @p2@；INDEX 无逐条快照，**不确定**到具体 id）。
- **其余 264 条** = @V*@(188) + @W*@(71) + @SA*@(5) 的验证波，INDEX §二完全未列；该波带来两个新类别并补录既有类别。
- 结论：**INDEX 的 520 不是当前树口径，是旧快照；本账本按 785 条全量落点，520 条全部包含在内**（同口径子集 521 ⊇ 520）。详见 @DEFECT_STATS.md@ §四。

## 八、已知不确定与限制（不猜、不掩）

1. @FIX_LEDGER@ 的处置列只填了 **72/785** 条（@fix_state≠OPEN@ 22 条 + @verified_state@ 非空 71 条，二者有重叠）；因此绝大多数条目的处置态来自 @_merge@ 四态，而不是修复侧回填。
2. **无 @_merge@ 逐条处置表**的条目共 **336 条**：@V/W/SA 验证波@ 291 条 + @M9@ 26 条（M9 §1 为定稿清单、§2 为 P0 复验、§10 为剔除项，无逐条四态表）+ 合并层新增/未列 19 条。这 336 条中除 @FIX_LEDGER@ 已填的少数外一律记“已登记待修”，**明确标注“无处置表”，不代猜**。
3. “需负责人裁决”83 条中，**65 条**来自 @FIX_LEDGER.owner_decision@ 的机械匹配，**18 条**来自条目正文（标题/影响/建议处置）出现“需负责人裁决”等措辞；后者可能包含**顺带引用**他条裁决的转述，需人工二次确认（已在 @DEFECT_LEDGER.json.disposition_reason@ 区分）。
4. 优先级两套口径不合并：@FIX_LEDGER.priority@（账本，93/449/240/3）与目录 @p0/p1/p2@（100/447/235/3）在 7 条上下有差异，@DEFECT_STATS.md@ 两表并列；发布门禁数（@release_blocker=Y@）以账本口径为准。
5. @问题扫描/账本/README.md@ 自述“521 条（P0 82/P1 262/P2 118，合 462）”与现 CSV 785 行不一致，属 README 未随生成器刷新；本包以 CSV 与重抽结果为准，并在 @DEFECT_STATS.md@ 记录该不一致。
6. 本包**未消费** @_recheck/RC*.md@ 与 @_verify/@ 的最新复验（它们不是任务指定权威层），只经由 @FIX_LEDGER.verified_state@ 间接反映；若要以 RC 层为权威重判，需另开一轮。

## 九、本包不做什么

- 不修改 @问题扫描/**@ 任何既有文件（含 @账本/@、@findings/@、@_merge/@、@40_OWNER_DECISIONS.md@）。
- 不新增/删除/改判任何 @id@，不把“无法判定”写成“无法复现”以外的结论，不为凑数拆分条目。
- 不做 git 操作，不构建、不跑测试、不触发算法批次。
