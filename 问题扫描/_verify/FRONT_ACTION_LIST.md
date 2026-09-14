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

## E. 本轮沉淀的六条**机器门**建议（拦的是"机制"而非单点，按性价比排序）
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