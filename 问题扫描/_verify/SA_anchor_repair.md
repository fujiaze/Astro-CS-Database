# SA 轴 · 悬空锚归因与「文件::符号」替代锚修复表（终稿）

- **时点**：扫描起点 HEAD `a3a343a4`（负责人任务时点，2026-09-15 10:2x 本地）；交付时点 HEAD `1ac569b3`（11:29）。期间并发推进 `a3a343a4→a20a1db8→1ac569b3`，`git diff --name-only a3a343a4 HEAD` 共 210 文件、**问题扫描/ 外 0 文件**（命令与输出见 §2-6）⇒ 真源与既有 findings 未被改名/删除，本表判读不受影响；findings 计数 294（第一轮定稿）→296→**297**（本轴终跑时点），并发新增均为审计档案，不构成锚宿主。
- **范围**：`问题扫描/findings/**/*.md` = 297 份，锚 token 全量 **6964 出现**；其中字面命中 HEAD `git ls-files` 集 3829 出现 / 736 种（不在本表）；**悬空 3135 出现 / 1008 种**全部入表，逐种给动作，不留悬空。
- **纪律**：纯静态（read/glob/grep + 问题扫描内只读 `python3 -B` + `git --no-optional-locks` 只读子命令）。未跑任何编译器/cmake/ctest（含 -N）/测试/二进制/ci/run.py；未连 Fatduck；未做任何 git 写/删除/改名；结构化文件（recheck_round1.json、FIX_LEDGER.jsonl）一律解释器解析。**未改 findings/ 与 账本/ 任何字节**；产物仅本 `.md` 与同名 `.json`。

## §0 口径声明与「421」复现

1. **锚 token 判据**：路径形（≥1 斜杠）与裸文件名形，扩展名白名单 `jsonl|tsv|csv|cpp|hpp|md|txt|yaml|yml|cmake|ps1|sh|ini|json|py|c|h`，允许 `:NN`、`:NN-MM`、`::符号` 后缀，**尾部加否定后查**（禁止把 `astrocs.p1.calibration` 截断成 `astrocs.p1.c` 计为锚——第一轮口径未做此界，见下）。
2. **「421」精确复现**：按第一轮宽松口径（裸文件名形 token；真源索引 = 全仓 walk，排除前缀全清单：`./build/ ./run/ ./out/ ./logs/ ./.git/ ./__pycache__/ ./.pytest_cache/ ./GaiaDR3/ ./GaiaDR3SP/ ./BASS DR3/ ./AstroCS.wiki/ ./问题扫描/ ./设计大纲/ ./worktrees/`；另排除 `.o/.d/.obj/.so/.a`；字面路径与 basename 双 0 命中）= **421 出现 / 206 种**，与前台公布数一致（该数按出现次数计，非锚种数）。注：`问题扫描/_cache/recheck_round1.json` 实为「变更文件 × 494 条 verify id」清单（键 head/base/changed/verify/gone/moved/changed_ids，解释器解析核对），**不含 421 锚明细**，故以本轴复算为准。
3. **421 拆解（本轴终态判读）**：
   - **截断伪命中 149 occ / 70 种**：`astrocs.p1.c`←`astrocs.p1.calibration`(5)、`result.c`←`result.coverage_ok`(4)、`CI-BINDING-TESTS.c`←`CI-BINDING-TESTS.command`(2) 等——**不是悬空锚，是第一轮抽取器把普通文本截断成锚**，剔除即消；
   - **成员访问/通配噪声 28 occ / 11 种**：`img.wcs.c`、`path_n.c`（← `path_n.c_str()`）、`o.c`/`D.c`/`s.c` 与 `*_test.cpp` 残段——同上，改判非锚；
   - **实际可救回 86 occ / 53 种**：裸名唯一命中 55 + 指向我方档案（第一轮把问题扫描/整目录排除所以看不见）16 + 近似名 6 + 同名多处 5 + 历史删除 4；
   - **真无宿主 158 occ / 72 种**：其中 27 occ 系 `findings/*/**/README.md:4` 的**命名规则示例**「如 `L01_L02.md`」（**29 份 README 逐字重复该句**，非锚，见 §2-1），余 **131 occ / 71 种**逐条给 reword/删判（见 §1 细分行）。注：终态全量裸名真无宿主为 191 occ/85 种——421 集与终态之差（33 occ/13 种）来自并发新增第 297 份 findings 的新 token，均已在表内。
4. **本轴主口径（悬空 = 1008 种/3135 出现）**：以 HEAD `git ls-files` 集为宿主唯一判源（`./问题扫描/` 与 `./设计大纲/` 也在集内，按其角色另归类），工作树存在未入库者单独成桶（B1/B2/B5），**不跟随第一轮「真源排除」把 owner/审计档案判为失踪**。
5. **三级复核法（R_TIER 规则三）**：判「缺失」前依次——①整 token grep（findings 内取逐字上下文）→ ②basename 检索（HEAD ls-files + 工作树全量 walk）→ ③git 全历史 `--diff-filter=DR` 映射（重命名/删除链）。命令与输出见 §2。

## §1 归因分类计数（种 / 出现；宿主与动作见 json 行级）

| 类 | 含义 | 种 | occ | 动作 |
|---|---|---:|---:|---|
| A1 | 裸文件名（无目录前缀；basename 全仓唯一/择一命中） | 570 | 2278 | rewrite_path（高置信 685 种可自动回写） |
| A2 | 模块相对路径笔误/前缀错（含 `src/`、`.../`、构建目录混入） | 138 | 185 | rewrite_path；跨目录同名换目录者标「需签名确认」 |
| A3 | 已重命名（git 改名链/近似名） | 47 | 85 | rewrite_path（链终点命中 HEAD）或 needs_human |
| A4 | 已删除（HEAD 无、全历史 D 命中） | 10 | 12 | **不得改锚现行宿主**：改述「历史文件（commit X 删除）」或删 |
| A5 | 指向我方自身文档（审计档案，可解析但禁作仓内事实证据宿主） | 94 | 272 | rewrite_to_canonical（补 `问题扫描/` 前缀）；VERIFIED 证据位需人工换真宿主或降为交叉引用 |
| B1 | 工作树存在但未入库（影子残留/待提交） | 13 | 34 | needs_human：提交入库或改锚已跟踪宿主 |
| B2 | 指向 gitignored 运行区 run/build/out/logs（C-15 红线） | 22 | 26 | delete_or_reword：宣称留、锚面删（脚本须回拷 `_verify/` 才可作锚） |
| B5 | 控制包/设计大纲未入库工作区副本（工程控制/） | 8 | 21 | needs_human：入库 `engineering/control/` 或改述「控制包文本，非仓内」 |
| B3 | 外部 URL | 1 | 1 | reword：改外链引用格式 |
| C1-产物 | 运行时产物名/待新增 fixture 名（`p1_wcs.json`、`product.json`、建议中的测试夹具…） | 34 | 75 | reword：标「运行时产物/待新增」，禁以产物名冒充源宿主 |
| C1-宣称不存在 | 宣称本体即「该文件/符号不存在」（`PHASE_OVERVIEW.md`、`DATA-001_*`…） | 8 | 20 | reword：事实留，锚改普通文本+检索口径，禁悬空路径形态 |
| C1-外链 | 上游源文件名（Siril `star_finder.c`/astrometry.net `healpix.c`…） | 7 | 18 | reword：显式上游归属（仓库+版本） |
| C1 | 真无宿主（HEAD+全历史+工作树三判据 0 命中） | 28 | 35 | **应删或整句改述**（含 `run_abi_checks.sh` 4 occ、`tests/abi/run_abi_checks.sh`——HEAD 与全历史 0 命中，见 §2-4） |
| C1-near | 真无宿主但有 HEAD 近似名候选 | 7 | 8 | needs_human（候选已给） |
| C2 | 正则噪声/示例名（`L01_L02.md` 27 occ 等，非锚） | 21 | 65 | reword_text：还原为普通文本 |

按动作汇总：**rewrite_path 702 种/2435 occ ＋ rewrite_to_canonical 53 种/208 occ**（合计 843 种可回写，其中 `auto_safe=true` **687 种/2382 occ** 经 2380 个建议锚子串级校验零失败）；needs_human 122 种/240 occ（候选已列）；reword 50 种/114 occ；delete_or_reword 60 种/73 occ；reword_text 21 种/65 occ。**1008 种全部有判定，零悬空遗留。**

## §2 三级复核取证（命令逐字 + 输出）

**2-1 `L01_L02.md`（27 occ）判「示例名非锚」**：
```
$ grep -rn 'L01_L02.md' findings | head -3   # 工作树内检索
findings/D_COMMENT/p1/README.md:4:一个 slice 一个 `.md`（文件名 = slice 码，如 `L01_L02.md`）。尚未定稿的原始记录在 `../../../_cache/`。
(另有 p0、p2 同句——grep -rl 该句 = 29 份 README 逐字重复)
$ git --no-optional-locks -c core.quotepath=false log --all --no-merges --diff-filter=DR --name-only --format= -- '*L01_L02.md' | grep -x 'L01_L02.md'; echo rc=$?
rc=1   # 全历史无此精确 basename（在册者为 M1a_L01_L02.md 等 18 份）
```
**2-2 控制包 `12_DLL_ABI_AND_LOADER_STANDARD.md`（7 occ）判 B5 未入库副本**：
```
$ find . -name '12_DLL_ABI*' -not -path './.git/*'
./工程控制/AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3/12_DLL_ABI_AND_LOADER_STANDARD.md
$ git --no-optional-locks -c core.quotepath=false ls-files | grep -c '12_DLL_ABI'
0   # HEAD 无此文件 ⇒ 锚在 HEAD 不可解析
```
**2-3 run 区审计脚本 `E4/collapse_recompute.py`（及 E1/E3 同族 5 种）判 B2/C-15**：
```
$ find . -path '*E4/collapse_recompute.py' -not -path './.git/*'
./run/审计执行层/E4/collapse_recompute.py
$ git --no-optional-locks ls-files | grep -c 'collapse_recompute'
0   # 实体存在但在 gitignored run/ 区 ⇒ 宣称可留、锚面必须删/回拷入库
```
**2-4 凭空符号/文件两例（`build_fits_wcs_from_solution`、`tests/abi/run_abi_checks.sh`）**：
```
$ grep -rn 'build_fits_wcs_from_solution' lib/ include/ tests/ cli/ | wc -l ; git log --no-merges -S 'build_fits_wcs_from_solution' --format='%h' HEAD | wc -l
0
4   # 4 个 commit 全为问题扫描/docs 审计提交，代码面从未出现 ⇒ 符号凭空
$ git --no-optional-locks ls-files | grep -c 'run_abi_checks'
0   # 文件与全历史均 0 命中 ⇒ 引用它的 4 occ 句走应删/改述
```
**2-5 改名链 `schemas/*.schema.json`（13 occ）必须追链到 HEAD**：`git log` 示两段改名：64c1e988 `schemas/ → contracts/schemas_tmp/`，009ee419「schemas/(7 个合同 schema)→contracts/schemas/」；`contracts/schemas_tmp/` 现已不存在（`ls` 报无此目录），现宿主：`git ls-files contracts/schemas/ | head` → `contracts/schemas/traceability_matrix.schema.json` 等 4+ 命中。**一跳改名即改锚会把锚写进已消失的中间态**。
**2-6 并发增量边界（本表有效性前提）**：`git -c core.quotepath=false diff --name-only a3a343a4 HEAD | grep -cv '^问题扫描/'` → **0**（210 文件全在问题扫描/）⇒ 宿主解析所依赖的真源 HEAD 文件集全程不变。

## §3 判定条（15 条，宁窄而实；均为对「我方前台自身锚债」的修复判定，非新缺陷主题；免重报核对过 SUMMARY 簇表与 INDEX §三）

1. **421 复现成立但口径虚高**：421 = 裸名形×真源 0 命中出现数（本轴精确复现）；其中 177 occ/81 种是抽取器截断/成员访问伪命中，86 occ/53 种实可救回，**真无宿主仅 131 occ/71 种（剔除示例句后）**。前台按本表 §4 步骤回写即可全数落定。（related：INDEX §三「行锚系统性失效 M6b-E-002」同族不同事实）
2. **悬空主体是裸文件名**：全口径 1008 种悬空中 A1 占 570 种/2278 occ（72.7%）——违反 E4「锚必须可解析」的主体形态；685 种有唯一宿主，可批量补前缀（表内 `auto_safe`）。
3. **禁行号锚的本轴实测支撑（C-18）**：悬空锚中带 `:NN` 行号者 **1466 occ**；其中宿主已给、做了符号推算的 1370 occ 里，仅 27 occ 行文本印证、1127 occ 位推算低置信、216 occ 行号位无定义可提取（余 96 occ 宿主本身缺失未处理）——**行号锚即使路径修对也不可信**；本表逐 occ 给 `proposed_anchor`（`文件::符号`），未印证者一律标人工过目，不冒充可自动。
4. **两处凭空符号**：`ipv_wcs.cpp::build_fits_wcs_from_solution` 与 `aio_hips_writer.cpp::write_tile_core`——HEAD 代码面+全历史 0 命中（§2-4）；其所在句（RC3-399 型「防清理凭据是不存在的符号」宣称）**事实本身成立**，但锚必须改述为「0 命中（检索口径同 §2-4）」而非悬空 `::符号`。另有 5 occ 属 SYMBOL_ELSEWHERE（符号在他处）：**禁撤条，换宿主**（R_TIER 规则一）。
5. **改名类 47 种 85 occ 中 13 occ 是两段链**（`schemas/*` → `schemas_tmp` → `contracts/schemas/`，§2-5）；前台回写脚本必须按 json 的 `proposed_host`（已追链到 HEAD 命中），**禁止**按 `_tools/anchor_repair*.js` 的一跳映射回写。
6. **已删除类 10 种/12 occ**（`docs/stage1_fix/00_COMMON_CONTRACTS.md` 3、`lib/healpix_db/ahpx_io/ahpx_writer.cpp`、`lib/photometric_calib/.../synthetic_photometry.py` 等，各带删除 commit）：宣称若指历史事实→改述「曾存在，commit X 删」（C-18 ③退役登记式）；若指现行状态→整句撤（本表 action=delete_or_reword）。
7. **A5 自指 94 种/272 occ**：`_merge/M7.md` 30、`40_OWNER_DECISIONS.md` 14、`_cache/L23.md` 12 等——补 `问题扫描/` 前缀后字面可解析（第一轮判失踪因真源排除本目录），但**作为 VERIFIED/证据列宿主一律不合格**（M6b-E-001 同机制第 N 实例：TEST/证据锚写在文档）；处置分两栏写入 json：路径规范化（自动）+ 证据角色（人工，needs_human 标注行已给）。
8. **B1 影子残留 13 种/34 occ**：`lib/snr_estimator/src/`（module_entry.cpp 3、noise_model.cpp 等——HEAD 只有 `cpp/` 面，`src/` 为未跟踪旧布局残留，§2 类推 + `ls-files|grep -c 'lib/snr_estimator/src'`=0）、`lib/phase1/tests/p1phot/` 两件、根级资源 csv/json（真源在册版在 `evidence/v6_1_rework/tasks/MON-001/logs/`，裸名锚应改指该版）。**锚到未入库文件 = 对任何 clean checkout 不可解析**，等同悬空；要么提交、要么换宿主。
9. **B2 run 区引用 22 种/26 occ**：E1/E3/E4 取证脚本、`build/CMakeCache.txt`、`run/release-rescue/*` 等，违 C-15（仓内文件权威依据禁指 gitignored 区）——宣称证据要留在 findings，**必须把脚本回拷 `问题扫描/_verify/` 入库或整句改述**；本轴不得越权回拷（只读纪律），已列全清单。
10. **B5 控制包文本 8 种/21 occ**：`12_DLL_ABI...`(7)、`11_MODULE_SOURCE...`(4)、`15_CPU_PROVIDER...`(3) 等权威条款引用只存在于 `工程控制/…FINAL3/` 未入库副本（§2-2）——**科学/门禁裁决的权威文本本身在 HEAD 不可解析**，是前台侧真实治理缺口：入库 `engineering/control/archive/` 或改述为裁决登记（40 文件）条款号；挂 related A-31/M6b-E-002 族。
11. **C1-产物 34 种/75 occ**（`product.json` 9、`p1_wcs.json` 6、`stage1.json` 5、`oracle.jsonl` 5、待新增 fixture `test_anchor_is_doc.json` 等）：运行时产物名与建议夹具名不是宿主文件——改述为「产物名（运行期生成，宿主=生成/消费代码 `文件::符号`）」；生成/消费侧真宿主锚多数已在同句，补链即可。
12. **C1-宣称不存在 8 种/20 occ + C1 硬悬空 28 种/35 occ**：`PHASE_OVERVIEW.md`（宪章 §12.1 点名、全仓不存在= M5b-G-07 事实本身）、`check_cli_protocol.py`（近似名 needs_human，同名域现役为 `tools/check_cli_command_layer.py` 等）等——前者留事删锚，后者按表逐条走；**两类合计 48 种/55 occ 是本轴「不得留悬空」的硬底线清单**。
13. **外部上游 7 种/18 occ**（`star_finder.c` 7、`healpix.c` 6、`atpmatch.c`、`astrometry_solver.c`、`gsl_multifit_nlinear.h`、siril raw URL）：这些是「依赖上游」宣称的正确对象，但**锚形态必须显式外部化**（`外部: siril/src/libstars/star_finder.c@<ver>`），否则任何锚门会永远判漂。（related：簇 7 权威外包、INDEX「许可与依赖登记面 M8a-G-001」同域不同事实）
14. **前台批量回写操作规程（唯一安全序列）**：①只执行 `auto_safe=true` 的 687 种（动作 rewrite_path/rewrite_to_canonical；替换键=species[].anchor_path → proposed_host；**先长后短排序，替换带路径字符边界**——anchor_repair v2 的教训）；②needs_human 122 种逐条（候选与上下文择一建议已写入 json `candidates`/`proposed_host`）；③reword/delete 类 131 种按行处置，处置后在 `账本/FIX_LEDGER.csv` 对应 id 行留 fix_note（本轴无权代填）；④回写后复跑 `node 问题扫描/_tools/verify_anchors.js` 与本轴 `/tmp` 口径（脚本随 json meta 声明，可请前台重导出）断言：字面未命中 occ → 仅允许残留在 needs_human 白名单内；**建议把该断言并入 L16-008 指出的「界内错锚零检测」缺口修好的新门**（related）。
15. **本轴不判「旧文档口径 vs 现行权威」归属**：所有替代锚仅声明「文件+符号在 HEAD 在位」这一机械事实；凡涉及宣称本身对错（如 weight_mode、MAD 口径），仍归原条目域裁决——按准绳顺序①负责人意图②冻结文本③代码自洽，本表不回改判词。**需运行期方能定论的锚**（如仅在 ctest 运行时生成的 `compile_commands.json`、`run/cli_runs/*`）一律标「需运行期，未判」，不冒充静态可修。

## §4 json 交付表字段说明（`SA_anchor_repair.json`）

- `species[]`：1008 行，按锚的 file_part（物种）聚合。字段：`anchor_path`（档案里的写法，回写键）、`cat`/`action`/`conf`/`note`（§1 图例）、`proposed_host`（唯一建议宿主）、`candidates`（多解候选）、`auto_safe`（前台可机执行）、`n_occ`、`occurrences[]`。
- `occurrences[]`：`at`（findings 内 文件:行）、`as_written`（含 `:NN`/`::sym` 的原文 token）、`proposed_anchor`（**「文件::符号」形态建议**，可空）、`flags`（符号在位/行文本印证/位推算低置信/仅改路径/SYMBOL_ELSEWHERE/SYMBOL_ABSENT…）。
- 规模 1.02 MB；QA：auto_safe 2380 个建议 occ 锚，宿主存在+符号子串双校验 **0 失败**。
- 校验命令（python 解析，勿用行文本读）：`python3 -B -c "import json;d=json.load(open('问题扫描/_verify/SA_anchor_repair.json'));print(len(d['species']), d['meta']['counts'])"`

## §5 合规声明

- 未修改 `findings/`、`账本/`、`_merge/` 任何文件；产物仅 `_verify/SA_anchor_repair.{json,md}` 两份（骨架先建后回读，与本节同步完成）。
- git 一律 `--no-optional-locks` 只读子命令（ls-files / log / diff / rev-parse / status）；python 只读（唯一写入为 `/tmp` 工作脚本与本 _verify 两份产物）。
- 全部裸数字带口径与时点（§0 主口径、§2 时点、421 复现口径单列）。

