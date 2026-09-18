> **19:58 刷新注**：死片补做 agent C_ALG_IMPL_a（35 条，全 P1/P2）于 19:58 交二次复验版（证据按迁移后现树重锚、6 条「原某腿被现树否证」以订正标注降级论证但未强改四态），主控重跑 merge：分布不变（OPEN 727/RESOLVED 42/VOID 13/UNVERIFIABLE 3；785 行、差集空、rc=0，日志 merge_r3.log）。此后无未交分片。

# ROOT-004 收尾 SUMMARY · 旧 bug 清单按最新权威重定版（接手轮）

- 接手开工基线：HEAD = main = origin/main = 2c328348304d033aecfa81faf79d1c6cd802b30a（15:03 实测三 SHA 一致）。
- 收口时基线：HEAD/main 由并行执行线持续推进（ROOT-001/002/003/006/007、TEST-GREEN-001 等已落入 main 并推送；收口实测 HEAD=d414c3e0、origin/main=b4afc135——属并行线正常节奏，本任务零 git 写、不动其域）。
- 口径唯一来源：问题扫描/REBASE.md（四态/10 列/覆盖度/禁令）；分片与判定共用 _tools/SHARD_BRIEF.md。本轮不重造口径。
- 交付物：问题扫描/REBASE_TABLE.md（785 行×10 列）、本 SUMMARY、P0_RECHECK.md（93 条主控逐条复核）、shards/**（26 片）、_gen/{idmap.csv,stats.json,p0_rerun.json}、_tools/merge_rebase.py（新写：校验+合并+统计）、audit/**（扩展审计 10 轴，负责人加派）。

## 一、覆盖度证明（三条同时成立）

| 证据 | 数值 | 复跑命令 |
|---|---|---|
| 原始条目分母 N | 785 = FIX_LEDGER.csv 行数 | python3 -c "import csv;print(len(list(csv.DictReader(open('问题扫描/账本/FIX_LEDGER.csv',encoding='utf-8-sig')))))" |
| REBASE_TABLE 数据行 | 785（ID 双向差集为空） | python3 reports/PROJECT-GOVERNANCE-01/root-scan/_tools/merge_rebase.py（WROTE 785 rows；missing_id_count 0；error_count 0；rc=0） |
| ID→锚文件 | 785/785 有锚、0 落空；说明性标题 34 不计入 | python3 _tools/make_idmap.py（HEADINGS 821；ids with 0 heading: 0） |
| 分片 | 26/26 齐（死片补做：C_ALG_IMPL_a、E_TRACE_P2_J_FS、G_GOV_GATE_P2；在跑片未重派，自然收齐） | _gen/shard_index.json + merge 校验 |
| 零删除 | 开工 1096（=前任基线 1095+前任后补的 REBASE.md 本体）→收口 1097（+REBASE_TABLE.md）；`findings/**/*.md` 327→327 逐字不减 | find 问题扫描 -type f / find 问题扫描/findings -name *.md 前后对比 |

注（不掩盖差异）：REBASE.md §2 的「findings md 318/条目文档 309」为前任口径时点值；当前实测 findings/**/*.md=327（含 36 份各级 README）。分母不受影响：口径=账本行 785，本轮复跑 make_idmap 仍 785/785、821 标题、34 说明性标题，类别×优先级矩阵与 §2 逐项一致。

## 二、四态统计（785 条）

| 结论 | 条数 | 占比 |
|---|---|---|
| OPEN | 728 | 92.7% |
| RESOLVED | 41 | 5.2% |
| VOID | 13 | 1.7% |
| UNVERIFIABLE | 3 | 0.4% |

**OPEN 优先级分布：P0 73 / P1 428 / P2 225 / P? 2**。P0 复核后 93 条 = 73 OPEN + 20 RESOLVED，无 VOID/UNVERIFIABLE。

> 定版注：G_GOV_GATE_P2 分片在首次合并（16:26）后被其执行 agent 以复验版覆盖（16:36），主控重跑 merge 吸收：OPEN 733→728、VOID 7→13、UNVERIFIABLE 4→3（V19-N-12 由 UNVERIFIABLE 定版为 VOID「负结果不立条」；W5-N-11 由 OPEN 定版为 VOID「旧 standards 条文下链」）。P0 分布不受影响。以本表数字为最终版。

- VOID 13 条全部写明旧判据+最新替代（V12-N-14、V10-N-09、V7-N-09、M5b-C-07、M5b-G-07、FD-G-003、M7-H-104、M2a-G-3、M6b-G-007、M8a-G-009、V19-N-12、V5-N-02、W5-N-11）：旧宪章/旧标准条文独判（已下链到现行替代）、负结果自述不立条、最新权威反向要求、宿主为历史证据面 四类。
- UNVERIFIABLE 3 条：V12-N-17（缺宿主指针：V12.md §2.17 不存在）、M5a-I-001 与 M7-I-101（需逐式重推/语义级冲突裁决）。
- 与 GAP_AUDIT 重叠 52 条标「与 GAP-0xx 重复」（OPEN 50），全部保留原 ID 不合并。

## 三、P0 主控复核（93/93，不抽样）

详见 reports/PROJECT-GOVERNANCE-01/root-scan/P0_RECHECK.md。要点：
1. 26 片抽出的全部 93 条 P0 逐条在当前树重跑证据命令（_tools/p0_rerun.py → _gen/p0_rerun.json）；前任 78 命令批（p0_verify_A2/B.sh）整体复跑留痕 logs/*_recheck.out。
2. 19 条（C_DOC_CODE_GAP_P0 18 + F_TEST_GAP_P0 1）证据列为「描述语」不满足字面可复跑，主控逐条重新取证改写为字面命令，并修正 5 类路径漂移（star_matcher→cpp/src、gaia_client.c 符号→module_entry.c、p2_session→stage2_common/upm、windows 清单→cmake/astrocs.product.windows.json.in、ivar 计数→phase2/tools/stage2.cpp）；改写后逐条重跑通过。
3. 账本 fix_state=FIXED/PARTIAL 的 21 条 P0 全部按当前树复跑后才认 RESOLVED（例：M5a-G-001/002 利用率门已改 85%×已分配容量+回归锁；M2a-C-1 编码容量按 n_coords 并回显；M7-C-001 grid 消费装配值+硬门；M4-C-03 默认 fail-closed+降级计数入诊断）。
4. 复核结论：93 条全部【维持】，无改判；三条禁令逐条核验通过。

## 四、OPEN 最该优先整改前 10（全部为 P0，主控复核维持）

| # | ID | 一句判词 | 归属 |
|---|---|---|---|
| 1 | V11-N-01 | HiPS SNR 目录 Python 镜像落后 C 头两字段，越界读 24B ⇒ 交付目录第 2/3 条记录是邻堆垃圾 | AIO-001 + CI-001（镜像一致性门） |
| 2 | V11-N-04 | dpsf_fit_batch_f32 两份 Python 镜像少传第 9 参 out_status ⇒ 栈垃圾地址可写、「成功星集合」由垃圾决定；现行 gate2 PSF 证据产生于坏 ABI 之上 | P1-001 + QA-001 |
| 3 | V11-N-05 | ABI 协商/自检 6 个合同函数生产面零实现零调用，唯一实现住在测试自家探针里 ⇒ 门在自证 | ARCH-001 + CI-001 |
| 4 | M2b-A-01 | HiPS 写入口 nside 无 2 的幂/上界校验，ilog2 静默夹逼非法值成合法层级 | AIO-001 |
| 5 | M2b-A-02 | NSIDE 语义三源互斥（写 2^(k+9)/文 2^k/读端 h->nside）⇒ 自产 tile 必被自家读端判 TILE_INVALID | AIO-001 + DATA-001 |
| 6 | M5b-G-01 | 命令树/退出码/golden 全打在 COMPATIBILITY 二进制（build/cli/astrocs）且缺文件时静默零检查 ⇒ 交付物 CLI 符合性证据无效 | CI-001 + CLI-001 |
| 7 | M6b-G-001 | 追溯矩阵把「同名文件存在」当 VERIFIED（TEST 18/22 锚在 docs；EVIDENCE 层结构上不可核；CI 无 --strict）⇒ 发布门禁无判据 | DATA-001 + CI-001 |
| 8 | V13-N-03 | 审计状态铸造 713/713 VERIFIED + findings_p0..3=0 + 7 门 PASS，且铸造产物打进审核包（交付面） | FINAL-001 + CI-001 |
| 9 | M3b-G-01 | PSF ~0.5px 未闭合 BLOCKER 只活在归档日志与测试脚本注释，规范层三处零登记且 SCI 正面宣称「无 0.5px 量化损失」 | P1-001 + DOC-001 |
| 10 | M3-C-003 | SCI FROZEN min_samples=5 vs 代码 64（差 12.8 倍），ALG 以「不改 SCI、以代码为准」单方裁决 ⇒ 权威层级倒置未纠 | P1-002 + GOV-001 |

同批建议：M5b-G-02（追溯门恒 return 0 fail-open）、M6a-G-001（CON-COMMENTS 空壳门+「冻结」二字永久豁免）、M6b-G-002（4 份 RELEASE_STATUS 并存无单一事实源）、M5a-G-003/G-005（run_monitored 零应用点；THREAD-BUDGET 现行违例 2 处 rc 见日志）。注：V2-N-08 复核判 RESOLVED（psf_mode 已改真实模式派生式，见 module_adapters.cpp:1610-1614），残余「parity 锁」并入 NP-B。

## 五、归属映射与下一轮候选任务

- 映射 TASK_LIST（31→现 33 任务）：OPEN 728 中 686 条映射成功（94.2%）。TOP：P1-001 102、DOC-001 89、CI-001 86、P2-001 40、MOD-001 37、P1-002 36、DATA-001 34、AIO-001 31、GOV-001 26、RT-001 24、QA-001 20、P2-002 19、PKG-001 19、P3-001 17、CPU-001 16、P3-002 23、OBS-001 22……
- 映射不上：42 条 OPEN 落 9 个 NEXT-PACK 原始组，去重归并为 7 个候选任务（V13-N-05/07 收编 CI-001 后 NP-C 缩至 2 条）：

| 候选 | 覆盖条目 | 建议文件域 | 验收门（可机械复跑） |
|---|---|---|---|
| NP-A ABI 镜像与内存安全回归门 | NP-01 组 9 条（V11-N-03、M9-H-3、M8-H-001、M7-A-210…） | lib/gaia_xpsd_client、lib/astro_image_io、tools/quality 新 AST 检查器、ci/checks.json | CHK-ABI-MIRROR：ctypes 镜像 vs C 头逐字段/sizeof 一致 + 原型 arity 检查 rc=0 |
| NP-B 科学边界守卫与冻结测试设计回写 | NP-02 组 4 条（V12-N-10 锥触极回退、M7-T-102/103/104） | lib/phase2、tests/**、docs 偏差登记 | 每个冻结设计项有在册可执行用例（ctest 注册面 grep 命中）且负例存在 |
| NP-C 登记 ID 生命周期与宿主指针补全 | 2 条（V12-N-17、M5a-F-001） | docs/**、contracts/schemas、tools/traceability | 「先登记后使用」条文入 docs；铸造 ID 零宿主重算=0 脚本 rc=0 |
| NP-D vendored 许可与哈希覆盖面 | NP-04 组 4 条（M8a-G-009/010、M2a-F-2/3） | third_party/**、packaging/dependencies.json、tests TSAN 注册 | 每 vendored 目录 LICENSE 全文 + 逐文件 sha256 锁；竞态用例注册进 CI |
| NP-E 注释锚点漂移治理 | NP-DC-01/NP-DC1 组 12 条（L28b-D-001/002/005、V8-N-01/04、M2a-D-3/4…） | lib/**、providers/** 注释面 | DOC-SYMBOLS 门扩展：(line N) 型锚全部可解析指向被标注量；裸行锚计数=0 |
| NP-F 头-实现语义互斥清零 | NP-DC-02 组 15 条 + NP-DC-03 1 条（M1a-D-001、M3-D-001..003、M6a-D-002..006…） | 各公共头+对应实现（逐条裁改哪侧） | 每条「文档锚+实现锚+判定命令」三件套门 rc=0 |
| NP-G Gaia XPSD 客户端可靠性 | NP-GAIA-01 3 条（M9-G-1/2/3 句柄泄漏+memset 失联、部分树静默空、活性挂死/int 溢出） | lib/gaia_xpsd_client/** | 三类路径各有负例注册；「加载失败≠无星」可断言 |

凭据外泄面：GAP 线已立 GAP-028/ROOT-006（CHK-SECRET-HYGIENE）并落了打包侧 DENY；ROOT-004 视角保留独立条目（§六-3）。

## 六、三份根目录存疑文档（本轮只读实测更新）

1. **CHANGELOG.md**：已被 ROOT-007 清除（提交 01db973b，D 253 行；裁决语「旧发布流水，改由 git 历史承担」）。残留：tools/pack_audit_package.py:19 ROOT_FILES 仍含该死引用（PKG-8 同源）；VERSION_NAMESPACES 等指回项需 DOC-001 顺路清。**建议负责人确认「删除即终态」；若认为裁决登记通道不可缺，请裁「移入 docs/governance/ 重建登记面」——本任务不代改。**
2. **VISUAL_CHECK_README.md**：维持「归档 docs/archive/」建议，本轮复验全部成立——根目录仍 tracked（72 行）；launch/ 不存在；run/phase2/v9 不存在（ROOT-003 清运后更不可能）；DESIGN:69/:380/:438 三处明言 HiPS Browser 未来组件、不进产品 manifest、Alpha 不含 GUI。归档保史实、消根条目；若保留活动面必须改指真实入口（验收门：文档内每条命令路径 test -e 全通过）。
3. **FATDUCK_ACCESS.md**：打包面已闭合、git 面仍敞口（独立新发现，需裁决）。本轮实测（全程未读内容，只模式计数）：TRACKED=是；git check-ignore→NOT-IGNORED；tools/pack_audit_package.py:27 已有 DENY_PATHS={FATDUCK_ACCESS.md}+DENY_NAME_RE（*_ACCESS.md 形态）先于白名单判定（前任所见「ROOT_FILES 收录」已被 ROOT-006 修复）；ci/impact_map.json:565 仍登记（仅影响映射，可留）。残余风险=凭据仍在 git 历史与工作树跟踪。待裁：①git rm --cached + .gitignore；②凭据是否轮换；③是否退出 impact_map。归属 GOV-001（登记面）+ ROOT-006 收尾。

## 七、扩展审计（负责人加派，10 轴）

产物 reports/PROJECT-GOVERNANCE-01/root-scan/audit/<AXIS>.md（ARCH/CLI/SCI1/SCI2/CI/GOV/AIO/RT/QA/PKG）＋索引 audit/AUDIT_INDEX.md。终版：24 轴全交——第一波 10 维轴 107 条（P0×13）+ 第二波 14 文件级轴 137 条（P0×7，计划 2398 文件 100% 穷尽，见 audit/COVERAGE_MATRIX.md 终判）= 244 条 / P0×20。索引终版见 audit/AUDIT_INDEX.md。插队级 9 条（安全 3：fatduck-admin 命令注入、FATDUCK git tracked、**ACR 生产触达（根 CMakeLists:397-400 编译 acr stub 进产品目标，主控亲验）**；科学 3：饱和位写死 50000、CAR/AIT CRVAL2 恒等、order_sel 不驱动层级；门失效群 3）。

## 八、验收门逐条自证（本轮真跑）

| 门 | 命令 | rc | 关键输出 |
|---|---|---|---|
| G1 覆盖度 | python3 _tools/merge_rebase.py | 0 | WROTE 问题扫描/REBASE_TABLE.md 785 rows；missing_id_count 0；error_count 0 |
| G2 P0 三要素 | python3 _tools/p0_rerun.py | 0 | P0 rows found: 93 missing: []；state dist OPEN 73 / RESOLVED 20 |
| G3 OPEN 100% 映射 | _gen/stats.json attribution_open | 0 | 686 task + 42 NEXT-PACK = 728，无空归属 |
| G4 VOID/注记完备 | merge 内建 quality_gate（G_P2 重写后复跑 rc=0）| 0 | 四个违规清单全空 |
| G5 零删除 | find 问题扫描 -type f | wc -l | 0 | 1096→1097（仅 +REBASE_TABLE.md）；findings md 327→327；原始 findings 785/785 锚在位 |
| G6 越界核对 | git -c core.quotepath=false status --porcelain=v1（过滤本任务三域）| 0 | 域外 60 条全部属并行线（含其窗口内 lib/phase2_rej→lib/algorithms 移动）；本任务写域外零改动。注：前台并行提交（e7f33817/b4afc135）已收编早期 REBASE 口径与分片，工作树版本为最终版 |

## 九、需要负责人裁决的事项

1. FATDUCK 凭据 git 面：是否批准 git rm --cached + .gitignore + 轮换（ROOT-006 已管打包面；git 历史敞口只有您能裁）。
2. CHANGELOG.md：ROOT-007 已删——确认「删除即终态」还是「重建 docs/governance 登记通道」。
3. VISUAL_CHECK_README.md：批准归档 docs/archive/，或限期改指真实入口。
4. （V19-N-12 已定版 VOID「负结果不立条」，无需再裁）
5. P? 3 条（如 V7-N-09）：请定版优先级或确认「说明性条目不计门」。
6. M3-C-003 权威倒置：SCI 改 64、代码回 5、还是登记偏差 DISP——需科学裁决。
7. NP-E/NP-F 注释两族共 28 条：并入 TASK_LIST 现有任务顺路修，还是立独立「注释与声明卫生包」。
8. 【终版新增】ACR 生产构建触达（W2-LIB3-6，主控复验 CMakeLists:397-400 属实且守卫自我豁免）：与 DESIGN §1.3「纯 CPU 生产、ACR dormant」冲突——移交 INT/ARCH-001 线处置（stub 摘除或改 dormant 实证）。
9. 【终版新增】本地分支 ci-fix（69ec459f）违反 AGENTS §5——请您定夺处置。
10. 【终版新增】定版注：M5b-G-01 在收口窗口被 CI-001 线自修改判 RESOLVED（OPEN 728→727，P0 OPEN 73→72）；CI/TOOL 轴记录的 checks.json 147→38 父项 ID 收敛属窗口内自愈，订正表证据按取证时点有效。
8. 【SCI2-02 · AGENTS §8 升级】DESIGN §5.3 八投影 vs 实现/ALG §15/在册测试四投影——补四投影还是改 DESIGN，须您裁（不得由实现侧单方结案）；同时 ALG 文档自身 §1 与 §15 并存自相矛盾。
9. 【SCI2-03】docs/science/CONTROL_WEIGHT_SNR.md §4 把 support×snr² 标为 weight_mode=2 与实现（mode2=ivar）及节点链判非法互斥——公式权威需走变更流程订正。
10. 【安全】.github/workflows/fatduck-admin.yml 把 dispatch 输入原样注入 FATDUCK self-hosted 节点 shell:cmd（任意命令执行+证据链可篡改）——建议即刻禁用；另 tools/astro_toolkit+tools/README 内置子 Agent git_add/commit/push 通道且自述绕沙箱确认，违 AGENTS §5 硬禁令，需专项处置。
11. 定版注：M5b-G-01 在收口窗口内被 CI-001 线自修（API-DOCS 门改产品图优先+fail-closed，代码点名本条），已按实况改判 RESOLVED（OPEN 728→727、RESOLVED 41→42、OPEN P0 73→72）；SCI2-05/06/07 与订正表 M2b 家族交叉同源已在各轴标注。

## 十、过程留痕与陷阱（供下一任/隔壁线参考）

- 前任舰队未全停：接手时 12 DONE、5 在跑、9 假死；假死片补做完成，在跑片未重派；其后原舰队又复活交齐 E/G_P0、F_P0、A_P1b 等——未互踩，但出现 3 类格式违规：F_TEST_GAP_P1 表头简写（已规范）、A_SCI_DEF_P2 一行优先级误写 P3（按账本改回 P2）、C_DOC_CODE_GAP_P0 18 行证据为描述语（已重建）。merge 校验（列数/四态/优先级/类别/去重/双向差集）全过才写表。
- 账本 fix_state=FIXED/PARTIAL 不可直信：21 条 P0 及若干 P1/P2 均按当前树复跑后才采 RESOLVED。
- 行号漂移普遍（前任 1283 锚 EXACT 仅 105）：判定一律以 path::symbol 重定位为准。
- 并发写域教训：docs/contracts/**、ci/checks.json、根条目在窗口内被他线改动——所有证据引用附「取证时刻+命令+输出」，不做裸行号断言。
- 基线窗口内前进 8+ 提交：REBASE_TABLE 抬头同时记录接手 SHA 与写出时 HEAD（merge_rebase.py 动态注入）。