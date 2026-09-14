# 隔壁工单 · 静态已确证项（前台汇总，按"能否当天做完"排序）

> 判据全部来自 V1–V15 复核层与前台自验，**均可静态复算、每条带锚点**；本单不含"需运行期"项（那些在 `V4.md §4`、`V2.md §4`、`V1.md §4`）。
> 规则：**每改一处须在 `问题扫描/账本/FIX_LEDGER.csv` 填处置列**（`fix_state`/`fix_commit`/`fix_date`/`regression_test`/`fix_note`），**`regression_test` 留空 = 未加锁 = R 层降 PARTIAL**。

## A. 让 CI 变绿的三处（当前静态必红，均为 `waivable:false`）
| # | 门 | 根因锚点 | 当天动作 |
|---|---|---|---|
| A1 | `CON-TRACEABILITY` | `c3452d48` 从 `docs/TRACEABILITY.csv` 删了 `SCI-PSF-001`/`SCI-REJ-001`/`SCI-INT-001`/`SCI-ACR-EQUIV-001` ⇒ PSF/REJ 两关键词在 63 个 id 中零命中（V6 逐 rev 复算 67→PASS、63→FAIL） | **改锚恢复四行**（`test_ids`/`test_files` 指 `p1psf`/`p1star`/`p1phot` 真目标）。**不得以删行处理不可追溯项** |
| A2 | `TRACEABILITY-MATRIX` | `c3452d48` 给 `TRACEABILITY_MATRIX.csv` 注入 UTF-8 BOM ⇒ `_check_csv_parity:380-390` 见 `\ufeffmodule_id` 直接 return，**整条 JSON↔CSV 同构校验从未执行**；剥 BOM 后仍有 5 行发散（CSV 改了、权威 JSON 三版一字未动仍 `EVID-MISSING`） | ①删 BOM ②**同一订正回写 `TRACEABILITY_MATRIX.json`** ③CSV 由 `gen_traceability_csv.py` 生成（否则下次重跑静默冲回） |
| A3 | `CTEST-REGISTRATION` | 新增 ctest 目标未登记，逐 rev 复算 UNREG **0→5→10→12**（`p1snr_science_*`5、`p1snr_linux_*`5、`p1star_angle_guard`、`ipv_triangle_budget`） | 补 `ci/ctest_baseline.json` 源清单 + `ci/checks.json` 的 `CTEST-*`（`waivable:false`），**command 必须带 `-R`**，否则触 C6「登记但未带名」 |
| A4 | 疑 `UT-BACKEND`（需一次 CI 定性） | `test_p3006:126` 要 `verdict==ok` 而自记实测 1.66–1.68 核 < 新阈值 `0.85×2=1.7`；`test_p2007:259-262` 要 `≥90/≥85` 而按 16 核归一为 7.1%；无 skip | 若确认红：同步 `tests/backend` 三文件的期望到新口径（**不是回退阈值**，阈值是 §18.2 冻结值） |

## B. 内存安全与静默数据损失（P0/P1，成本都是行级）
| # | 条目 | 锚点 | 动作 |
|---|---|---|---|
| B1 | **V2-N-01** | `IpvParams` 改布局（352→424）而 `lib/plate_solve/tools/diag_gaia_psf_projection.py:61-80` ctypes 镜像未同步 ⇒ `memset(sizeof)` **72 字节越界写**；无 `struct_size/abi_version`（§8.6）；该工具在 ci/tools/tests **零引用** ⇒ 门不可见 | 同步镜像 + 给结构体加 `struct_size` 首字段并两侧断言 + 把该工具纳入采集 |
| B2 | **V5-N-03** | `gaia_client.c:1201-1203` 裸 `atof` 解析 `magnitudeRange` → `:2054` 整文件剪枝谓词 ⇒ 畸形声明即**整 shard 静默漏星**；同批 `07eb229b` 刚立 `parse_bounded_int` 四重校验 ⇒ 自相矛盾 | 加 `isfinite` + `low<=high` + 值域窗；**失败则不置 `has_magnitude_range`**（宁可不剪不可漏星） |
| B3 | **V2-N-08** | FAST 前提（`psf_params` 零消费者）被同批 `35c85f53` 推翻 ⇒ `psf.max_stars=5000` 静默决定交付 `median_snr`/`snr_phot`/`m_5`；`psf_mode` 恒字面量 `"fast"` | 二选一：SNR 目录改回全量成功星；或在 `p1_snr.json` 记 `psf_mode`/`n_fit_input`/`n_sources`+截断位并写进 `DATA-P1-SNR`；补一条 `max_stars=0` 与 `5000` 的差异 parity 锁 |
| B4 | **V4-N-10** | 回收门回退分支**非单调**：`peak==last`（全程不还页）判 PASS、释放 40% 判 FAIL；**Windows/MSVC 恒走此分支** ⇒ §17.6 在发布平台对单调增长恒通过；新锁③还把旧判定钉成期望 | 修语义（残留量用 `peak-last` 侧、回落量另名）+ 撤锁③的"旧判定不变"期望 + 阈值随口径重标定（见 A-42） |
| B5 | **V3-N-01/02** | 新失败码 `-14` 未入 `docs/contracts/PUBLIC_API.md:233-239`；plugin `drz_legacy_status:862-866` 把 `-14` 折叠进默认域 | 补合同行 + plugin 显式 case 给 DATA/精度域 |
| B6 | **V6-N-06** | 自铸 `EVID-P1-PSF-001`/`EVID-P1-STAR-001` 各仅 1 处引用（自己那行）即抬 VERIFIED；**V1-N-09** oracle 脚本失踪而 `EVIDENCE_MANIFEST.json:180` 仍指向它 | 证据须有第二处消费者或改判；缺失脚本入库或删除引用 |

## C. 文档/注释与合同同源（P1/P2，多为"改一半"）
- **C1 `V2-N-03`（本轮唯一反向钉死）**：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:111` 仍写 `RMSE=mad·1.4826/A<=0.2`，代码已改对 ⇒ **按文档复现会把 RMSE 塞回 `mad` 字段**；同病 `lib/star_detector/README.md:132`。改一行即闭环，并把阈值放宽 **+48.26%** 与 `0.2` 出处补 `DISP-*`。
- **C2 `V1-N-04`**：`CONTROL_WEIGHT_SNR.md:44`「当前实现不产出 `sigma_F`」与同批 `snr_science.cpp:180-181`、`module_adapters.cpp:2544-2546` 直接互斥；`§7` 红线与 `:2520 snr_phot=median(SNR_F)` 冲突 ⇒ 改写为现行事实并绑 `snr_schema`。
- **C3 §6.3 红线残留**：`CONTROL_WEIGHT_SNR.md:66/71/40/§2a` 仍把 `weights[s]=support[s]×snr_v²` 标为 `weight_mode=2`（=`M3-A-002`/A-02 本体，**授权编辑改了一半**）。
- **C4 `V1-N-02`**：`snr_format=1` 未升版而物理量已换 ⇒ 新旧 `.hiss` 同标签不同量，下游静默错解读；`aio_healpix_io.h:52/58/64` 仍注旧定义；`DATA-P1-SNR/2` 在 `DATA_SEMANTICS` 零登记。
- **C5 `V6-N-05`**：`lib/phase1/noise/README.md:54-58` 称"本批未改仓库树 lib/core"，而同一 `35c85f53` 给 `module_adapters.cpp` 加了 **162 行** ⇒ 改实况描述。
- **C6 `V6-N-03`**：`star_matcher.h:6-7` 把 `PC_QF_*`/`SNR_QF_*` 宿主指向不含这些符号的 `photometric_calib.h`；`p1phot_fixgates.cpp:258-260` 自称"证明 6 个导出均有 `quality_flags` 形参"而 6 枚形参一个都没有。
- **C7 `V4-N-02`**：`resource_gate.h:170/:171/:395/:432` 文案滞留 `0.75/0.50/min(workers,cpus)`（进 verdict JSON 与 stderr，属交付面）⇒ 与实现同源订正。
- **C8 `V1-N-05`/`V2-N-07`**：`DATA-P1-FLUX` 三口径（registry ADU vs registry ELECTRON vs 实现 ELECTRON）；`median_*`/`n_valid` 口径改为"拟合子集成功星"未登 `DATA_SEMANTICS §15`；`PHOTOMETRY.md:82`/`:38` 的行锚漂移（重新制造 `M3-E-001`）。
- **C9 `V6-N-07`**：`PUBLIC_API.md` 的 9 个 snr 行锚 **9/9 漂移**，而 `anchor_contract.json` 的 `doc_globs` 不含 `docs/contracts/**` ⇒ 门盲区；另禁裸行号续锚（`PHOTOMETRY.md:38/:40` 的 `(552-559)` 正则不匹配、永不校验）。

## D. 挂账纪律（本单最容易被忽略但最要紧）
- **D1 `V6-N-10`**：本轮 9 个真改代码的提交（`35c85f53`/`8dc0220e`/`b858b76d`/`80c32b19`/`b0353303`/`23a7f665`/`6d74046d`/`bd4bc23b`/`c1959436`）在账本 **ref=0**，账本 20 条只覆盖 4 个批次 ⇒ **请为每个改生产代码的提交补一行**。
- **D2**：`M3-A-001`/`M3-A-002`（本域两 P0）**已改代码未挂账**（仍 OPEN）。
- **D3 `V4-N-05`/`V4-N-04`**：锁名要写**可注册定位到的名字**（宿主套件名 `p2_workers`/`io_ownership`/`mon002_gate`/`UT-ABI`/`UT-CLI`，而非注释标签）；且**锁必须断言被修的那条表达式/分支**（`M8-F-002` 现在锁的是注入分支，不是主退出路径）。

## 附：C-15 与 C-9 的规模实测（前台自跑，脚本 `问题扫描/_verify/_front_clause_scale.py`、`_front_runref_classify.py`）
- **原始计数**：tracked 文件里 `run/**` 形态引用 **573 处 / 194 个文件**（远超 V6 报的"本批新增 50 处/14 文件"——V6 只扫了 diff，我扫的是全仓存量）。
- **但我不按 573 报缺陷**：脚本分类后大部分是**合法的输出落位声明与 `dirty_ignore` 引用**（`ci/checks.json` 独占 158 处，主要是资源产物路径）。真正危险的是 **A 类：把 `run/**` 当证据/权威/期望值锚** 的行——见上方分类输出，此类才进 C-15 的收口范围。
- **诚实限定**：我的分类靠关键词启发（`证据|依据|推导|实测|oracle|REPORT|manifest|EVID|期望值|基准|见 `…``），**A/C 边界需人工过一遍**；C 类中性行我没有判据支撑，宁可不报。**结论强度：A 类的每一条都可逐行复算，A 类总数只是上界口径**。
- **C-9 裸行号续锚**：全仓 **13 文件 / 22 处**（`dpsf_psf.cpp` 4、`STAR_PSF_ALGORITHMS.md` 3、`e17_model_fit.cpp` 3、`test_query_pixel.cpp` 2、`cpu005_route_decision_test.cpp` 2…）。规模不大但**全部门都扫不到**（`ANCHOR_RE` 要求 `文件名:行号`），属"零成本可关死的盲区"。
- **未决标记存量**：`TODO`/`XXX`/`OWNER-nn` 等合计上百处（含标记文件数十个），**本批 09-13 之后仍被改动的文件里还有若干** ⇒ 说明"待办"在被搬运而非收敛；`OWNER-07` 未落裁决档只是其中一例（见 A-42）。

## E. 本轮沉淀的七条**机器门**建议（拦的是"机制"而非单点，按性价比排序）
| # | 门 | 判据（一句可写实现） | 拦住的本轮实例 |
|---|---|---|---|
| E1 | **合同↔追溯 ID 集对账** | `INDEX.yaml` 中 `status=ACTIVE` 的合同 ID 必须出现在 `docs/TRACEABILITY.csv`，缺失即红 | `V13-N-06`（四合同被删而 INDEX 仍 ACTIVE，永久分叉恒不红）、`V6-N-01` |
| E2 | **`target 存在 ⇒ 白名单必含行`** | CMake 里可安装的交付单元必须能在 product.json/contract 白名单找到行 | `V14-N-05`（`if(TARGET)` 自动复活安装而两份清单零登记） |
| E3 | **`cpu_heavy ⇒ 有并行轴证据`** | 声明 heavy 的单元须在源内命中 omp parallel/target 或 work-unit 循环 | `V14-N-06`（单发 JSON 读写挂 heavy，叠加 `V4-N-07` 成吞吐陷阱） |
| E4 | **`VERIFIED ⇒ 可解析锚 + refs>=2`** | 声明 VERIFIED 的行须提供可解析证据锚，且其 ID 在登记面外至少 2 处宿主 | `V13-N-01/03/04/05`、`V6-N-06`、`A-36`、`L28e-E-001` |
| E5 | **`run/**` 作权威即红** | tracked 文件的证据/权威/期望值字段出现 `run/**` 形态路径即红；waiver 的 `evidence`/`reason` 同判 | `V6-N-04`、**`V16-N-01`**、`V1-N-09` |
| E6 | **`struct_size` 统一语义 + 镜像逐字段比对** | 跨语言消费的结构体首字段必为 `struct_size`；AST 比对 C 头与所有 ctypes 镜像，字段数/序/尺寸不一致即红 | **`V11-N-01`（交付 HiPS 记录被污染）**、`V11-N-02/03`、`V2-N-01` |
| E7 | **在册计数禁字面量钉死** | 测试/门中形如 `assertEqual(len(units),N)` / `assertGreaterEqual(entries,1)` 的断言须由权威清单派生 | `V14-N-05`（补登记反而假红）、`V4-N-16` |
- **共同特征**：这七条全部**静态可实现**（多数是一行判据 + 现成解析器），且每条都对应至少一个"当前恒不红"的实例 ⇒ 建议隔壁**先落 E1/E2/E4**（三条覆盖本轮绝大多数结构性失守），其余随 R 层统一提。

## F. `E8`/`S-4①` 的具体形态已由 V12 出稿（`问题扫描/_verify/DESIGN_check_numeric_constants.md`，91 行）——隔壁可直接照此实现
- **建议检查项 id `CON-NUMERIC-CONSTANTS`**，三 profile、`waivable=false`、timeout 120s；退出码 `0` 全绿 / `1` `VALUE_MISMATCH` 或 `derivation` 复算不符 / `2` `NO_AUTHORITY` / `3` `BAD_REGISTRATION`（登记面自身坏，fail-fast）/ **`4` `OBSERVE_ONLY`** / `5` `SELFTEST_FAILED`。⇒ **观察期走退出码 4，不走 waiver**（R-05/R-13「严禁用 waiver 掩盖红灯」）。
- **两级判定是核心（防"位数洁癖"误报）**：先归一为数值再比 ⇒ 红；位数只在展示层记 `PRECISION` ⇒ 黄。真实对照：**`kLn10` 一处 30 位、一处 19 位，归一后同为 2.302585092994046（0 ulp）⇒ 只黄**；而 `1.4826` 对 15 位 rel −1.496359e−06 ⇒ 红。**位数不同 ≠ 值不同。**
- **为何禁用子串在场（两条实测包含关系，值得抄进 PR 描述）**：`'1.4826022185' ⊂ '1.482602218505602'` ⇒ **门永不红**；`'0.7316728' ⊄ '0.7316727929211932'`（实测 False）⇒ **把 `PSF.md` 订正到全精度反而红** ⇒ 子串判据会把截断串**固化为过门必要条件**（即 `V12-N-02`）。
- **三类族分层**：显式族（有 id 绑定）／**影子族**（命中但无 id 绑定 ⇒ `NO_AUTHORITY`，本门主要产出）／**共源族**（oracle 与生产同字面 ⇒ `CO_SOURCE`，**不得当"正向钉死"采信**——本轴三例：`p1phot_oracle.hpp:68` 用 `0.6745`、`noise_model_science_test.cpp:363`+`p1snr_science_test.cpp:45` 用 16 位串、`p1psf_oracle.hpp` 用 `1.230310`）。**19 条族正则已列全**（含 `NC-MAG-DECODE` 的 `0.001/-1.5/+24`、`NC-GATE-CPU-*`、`NC-GAIA-CAP-PER-FILE`、`NC-XPSD-WLGRID`、`NC-BBOX-MARGIN` 等），并给出**不扫为常数**的负样本面（下标/位移/位宽、`row[9]` 与 40 字节头、rng seed 与 SplitMix64、日志格式串数字）。
- **三条必进 `ci/checks.json` 的理由（全用在册事实，不是主张）**：①未接线即等于门不存在——`docs_machine_consistency.py` 实测**不在 `checks.json` 的 126 项内**，于是 ACTIVE 文档里的「PASS 9/9」成为无据证据（`M6b-G-003`）；②它**无只读模式、无条件覆盖写 `reports/`** ⇒ "未接线 + 有副作用"互相锁死永远进不了 CI（`ci/INVENTORY_REPORT.md:73` 就是它未入册的理由）⇒ **本门须默认零副作用（`--out` 才写且路径限 `run/`）**；③自证三件套：`--selftest` 坏例集逐条红、打印输入 SHA + 命令 + UTC、机器可读 jsonl 证据。
- **误报抑制要点（隔壁照抄可少走弯路）**：合法截断只走登记面 `display` 列（**不给 waiver 口子**，截断是事实不是例外）；FP32 按 `(value, precision)` 二元组比 `float32(host)` vs `float32(value)`（容 1 ulp）并要求 `digits<=7` 否则报伪精度（`1.4826f` rel −1.514e−06 红 / `1.482602218505602f` +1.36e−08 绿）；注释/markdown 命中只记 `DOC_DRIFT` 不占退出码；fixture 须 `kind=synthetic` + `reference_id`（`spectrumStep=1`、测试星等窗、`cap_value=200000.0`）；**禁行号锚，按 文件+符号 定位**。
- **接入节奏**：阶段 0 只报（`--observe-only`）跑一轮出「族 × 宿主」红绿矩阵 → 按 §1 十八行预判 **红 ≥13 族 / 绿 1 族（π 族）/ 黄 3 族（`kLn10`、4GB 同值两义、eps 族）**；阶段 1 **按族补权威点（禁止逐文件改位数——`A-12` 型局部修复正是本轮缺陷成因）**；阶段 2 升非豁免门并把红项与 `A-12`/`A-39`/`A-42`/`A-43` 对齐；阶段 3 拆 `snr_constants` 旧判据。**每阶段验收 = 三类坏例各红（位数不一致型／倒数式型／伪精度型）。**
- **一条支撑「禁行号锚 / `C-18`」的新实测**：复核期间 HEAD 由 `521095b` 推进到 `d2493d55`，`gaia_client.c` **同文件内漂移不等量（+1 到 +60）**——`WL_COUNT :27→:28`、`MAX_STARS_RESULT :31→:32`、`QUERY_CACHE_CAPACITY :113→:127`、`cos_dec` 守卫 `:922→:935`、**1.2 裕量 `:923→:936`**、`AE_C45_FACTOR :956→:969`、**0.25 剪枝 `:2054→:2114`** ⇒ 任何按行号写的锚/门在同一天内就会失效。**这不是观点，是实测数字。**
- **`E5` 扩展（V8-N-04）**：现判据「串里含 `run/` 即红」**抓不到裸文档名形态** ⇒ 须并判「**`REPORT.md`/`*.md` + `§N` 引用且在仓内无可解析宿主** 即红」；实测 15 处（12 处无前缀），且全仓唯一在册 `REPORT.md` 在 `lib/plate_solve/cpp/ipv/`，**其节名与被引内容不对应** ⇒ 会把复核者指向错模块。
- **`E10` 新增（V8-N-07 / `A-44`）**：凡注释/文档/合同出现「负责人裁(决|定|授权|要求)」+ 日期，**必须在 `40_OWNER_DECISIONS.md` 或 `CHANGELOG.md` 有同日期条目**，否则红。实测 39 行援引中两族 0 登记（P5-SNR 那族已登记＝合规正例可照抄）。该门同时封住"以裁决名义自证"与"合同自指导致成环"两种路径。

## §A 工单订正（V9 实测后由前台改写，**邻站以此为准，勿照旧文施工**）
- **A3 `CTEST-REGISTRATION` 关闭（已转绿）**：`scripts_v9_ctestreg.py` 现值 `TARGETS_TOTAL 206 / BASELINE_TOTAL 160 / EXPLICIT 46 / STRUCTURAL 0 / **UNREGISTERED 0** / STALE_BASELINE 0 / DANGLING 0 / PATTERN_NOT_IN_COMMAND 0 / VERDICT: PASS`（前台 `efde9ef6`/`23ffd698` 补登记 5 门所致）。⇒ **旧 A3 行作废，照它施工是无效工。**
- **A1 改写（根因换掉）**：不是「文档缺 PSF 关键词」，而是 **`c3452d48` 净删三行 frozen 科学门**（`SCI-PSF-001`/`SCI-REJ-001`/`SCI-ACR-EQUIV-001`）。⇒ **修法=回滚这三行**；**严禁往文档补词凑子串过门**（那是 `A-38`/`E1` 要拦的行为）。命中表现已含 **REJ 0**（旧工单漏报）。
- **A2 改写（两条一起修）**：①BOM 在 **HEAD blob**（非本机脏，`git show` 前三字节 `ef bb bf`）⇒ 去 BOM 并提交；②`check_traceability_matrix.py:387-390` **早退遮蔽 5 行发散** ⇒ 去早退、同批报全；只去 BOM 仍不够。
- **新增 A5 `CON-API-CONTRACTS`**（非豁免三 profile，必红）：`API_CONTRACTS.csv:371` 的 `estimate_mag_lim_by_density` 已被 `6d74046d` 从 `ipv_select.h` 删除 ⇒ 删该行或改 `RETIRED` 并登记，**注释行不算声明**。
- **新增 A6 `DOC-LINE-ANCHORS`**（非豁免，必红）：13 条 `C4`（PLATESOLVE 11 + P3RSMP-DESCRIPTOR + NOISE-MINPATCH + NOISE-CMAKE）⇒ 订正锚；**建议同批改「文件::符号」定位**（行号已实测同日漂 +1..+60）。
- **D 节改数**：`known_failures.json` **现 1 条**（`p1_noise_adapter`），check 面豁免 **0** ⇒ 旧「35 条 / 29 条过期」作废。
- **施工顺序提示**：实体红 4-5 个经 `CON-FULL-INTEGRATION`（聚合 10 checker）与 `KNOWN-FAILURES-BASELINE-CHECK`（未登记新红即红）扇出成 **6-7 个红门** ⇒ 先修根因门再跑聚合门，否则"修一个冒两个"。
- **给邻站的一条本机环境警告**：未跟踪的 `设计大纲/_evidence` 影子树会让锚类工具在**本机**多报 35 条 `C2 ambiguous`（13 → 48）⇒ **以 CI 口径（仅 tracked）为准**。

## §B-2 门修复的第二半（V9 片3-4；**只修必红门不够，这些门永远不会红**）
- **修 4 道 `--selftest` 门**（`V9-N-07` P0）：`ISA-LEAK`/`PROD-REACH`/`PRODUCTION-GRAPH`/`LOG-CONTRACT` 增传交付物参数并令缺产物即 `FAIL`，或改名 + 另立真门。
- **`CON-COMMENTS` 去 `re.S`**（`V9-N-08`）：否则删那行注释后门依旧永不红。
- **`_ctest_argv` 无条件加 `--fail-if-no-tests`**（`V9-N-09`）+ 补 20 个零证据门的执行面 + `CTEST-PHASE2-GATES` 补 `ctest_targets`。
- **exit 77 限定 `waivable=true` 可用**（`V9-N-10`）：`UT-CPU-AVX512(waivable=false)` 已实测以 77 隐身一次。
- **`CI-BINDING-TESTS` 采集面 9% → 全量**（`V9-N-14`）：`-p "test_ci001b_*.py"` 改 `-p "test_*.py"`；给 `validate_registry.py`/`check_registration_timeouts.py` 各立一门。
- **`UT-CLI` 的 `dirty_ignore` 去掉根目录裸名**（`V9-N-15`）：改 `run/cli_runs/**` 并把 56 个根目录 `astrocs_run_*.json` 归位。
- **两条自证规则（写进 RQS 协议，适用于你我双方）**：①**"定向复跑全绿"不构成 fast 绿**（74/130 门不可达，含必红的 `DOC-LINE-ANCHORS` 与已转绿的 `CTEST-REGISTRATION`）⇒ 定向复跑后必须补跑不可达清单；②**改 checker 必须同批改其 `changed_paths`**，否则该类复跑看不到新行为。
- **新门建议 `E11`**：`waivable=false` 的门的 command 里含 `--selftest`/`--selfcheck` ⇒ 红。
