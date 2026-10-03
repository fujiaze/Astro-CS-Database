# 审稿 P1 · ENG-tests-011（对抗审稿 第 1 遍）

- 仓库：`/workspace/Astro CS Database`，HEAD = `850a9ede`（`修正样条系数缺 h 因子，并撤回恒真门登记表的全域分母声称`）
- 片清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:1667-1719`
- 口径声明：**一遍 = 对同一片材料的一次完整重读**。本片全部结论来自本代理亲自 `read` 原文 + 只读取证（`grep`/`glob`/`git -c core.quotepath=false ls-files`/`git log --diff-filter=D`）。
- 遵守：未读 `/tmp/acsd_g08/`；零 git 写；未修改仓内任何文件；未编译、未跑 ctest/pytest/构建/任何测试脚本。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威清单） | **48** |
| 成员总行数（权威清单 `实际行数`） | **10430**（与本地 `wc -l` 逐份求和完全一致） |
| **本人逐字读完的成员文件** | **19 份 / 1774 行** |
| 本人覆盖率（按行） | **1774 / 10430 = 17.0%** |
| 子代理逐字读完并回报的成员文件 | 30 份 / 5526 行（子代理 A/B/C 三组，报告已收） |
| 子代理 D 组（14 份） | 已派发，**回报未在本次会话内返回**；本片结论不依赖该组 |
| **未覆盖成员** | **8 份**（子代理 D 组唯一覆盖部分，见下） |

### 1.1 本人逐字读完的 19 份（1774 行）

| # | 文件 | 行 |
|---|---|---|
| 1 | `eng/tests/conformance/echo/README.md` | 117 |
| 2 | `eng/tests/conformance/echo/module.yaml` | 28 |
| 3 | `eng/tests/validation/release02/c_delta_composition/check_renders.py` | 10 |
| 4 | `eng/tests/config/fixtures/negative/run_manifest_hardware_field.json` | 21 |
| 5 | `eng/tests/config/fixtures/positive/run_manifest.example.json` | 20 |
| 6 | `eng/tests/config/fixtures/positive/normalize_blocks.phase_config.json` | 44 |
| 7 | `eng/tests/unit/p2_identifiability/CMakeLists.txt` | 9 |
| 8 | `eng/tests/unit/p1_psfw/CMakeLists.txt` | 67 |
| 9 | `eng/tests/unit/p1_psfw/p1psfw_oracle.hpp` | 258 |
| 10 | `eng/tests/unit/p1_psfw/p1psfw_tests_winfo.cpp` | 152 |
| 11 | `eng/tests/unit/p2_rej/oracle_expected.inc` | 164 |
| 12 | `eng/tests/backend/test_isa_avx2_fma.py` | 96 |
| 13 | `eng/tests/backend/syn008_seam_main.cpp` | 153 |
| 14 | `eng/tests/unit/variance_floor_test.cpp` | 237 |
| 15 | `eng/tests/arch/test_budget_contract.py` | 92 |
| 16 | `eng/tests/quality/test_pack_audit_exclusion.py` | 82 |
| 17 | `eng/tests/unit/cpu001_negative_test.cpp` | 102 |
| 18 | `eng/tests/unit/cpu_profile_test.cpp` | 74 |
| 19 | `eng/tests/unit/aio/tmp_path_uniqueness_probe.cpp` | 48 |

### 1.2 诚实列出的未读完成员（8 份，均在子代理 D 组）

`eng/tests/unit/p1_hips/adapter_test.c`(952)、`eng/tests/unit/gaia_adapter_test.c`(883)、`eng/tests/unit/aio/aio_test.cpp`(786)、`eng/tests/unit/p1snr/p1snr_linux_test.cpp`(594)、`eng/tests/unit/p2_sky/p2_sky_node_adapt_test.cpp`(538)、`eng/tests/unit/aio/atomic_durability_probe.cpp`(166)、`eng/tests/quality/test_resource_monitor.py`(175)、`eng/tests/config/test_export_crop_contract.py`(133)、`eng/tests/cli/test_preflight_matrix.py`(215)。

⚠️ 上列 9 项中任一项均**未**由本人在本轮独立复核；对这批文件的任何既有判定本代理**不予背书**。

---

## 2. 本片判定

### **需修**（含 1 条阻断级缺陷）

最重的 3 条：

**① 阻断 · `eng/tests/api/test_cli_protocol.py:24` 整文件是死门 —— 判据读的是已删除的文档，产生 0 条断言。**
`DOC = os.path.join(REPO, "docs", "api", "CLI_PROTOCOL_V1.md")`。本人核实：`ls docs/api` → **不存在**；现行正本是 `docs/engineering/CLI_PROTOCOL_V1.md`；`git log --diff-filter=D -- docs/api/CLI_PROTOCOL_V1.md` → **`dc5a2122`**（文档迁移时删除）。`:86` `cls.s = read(DOC)` 在 `setUpClass` 抛 `FileNotFoundError` ⇒ 7 个用例**全部 error**，API-002「CLI 协议合同机器门」实际零断言。叠加：同一文件 `:5` docstring 自称权威是 `docs/engineering/CLI_PROTOCOL_V1.md`，与 `:24` 的代码路径**自相矛盾**。这是「门看起来在、实际不在」，且因文档迁移后无人回头改路径而长期潜伏。

**② 阻断 · `eng/tests/backend/test_isa_avx2_fma.py:74-85,84` 自愈判据 + 恒真决策列，且仓库内已有实据。**
- **自愈**：`:74-76` 把测量结果写入 **git 跟踪**的 `实验/engineering-evidence/prerelease-v5/ISA-003/MEASUREMENTS.csv`（本人核实：`git ls-files --error-unmatch` 命中），`:85` 再 `assertTrue(os.path.isfile(out))`。**测试自己造出它随后检查的工件** —— 在任何克隆上都必然绿，证据台账的真实性由「谁跑过 pytest」决定，不由审核决定。
- **恒真门**：`:84` 的决策列 `"SHIP(avx2)" if imp != "" and op in ("calibration","hips") else ""` —— **按算子名赋值，从不读 `improvement_pct` 的符号**。变体慢 100% 也照盖 SHIP。
- **实据**：台账现存行 `calibration,4254907,9816718,-130.7,SHIP(avx2)` —— **变体耗时 9.82 ms vs baseline 4.25 ms，慢 130.7%（2.3 倍）仍盖 SHIP**。
- **无人拦截**：`test_04:90-92` 只 grep 字面串 `"AVX2+FMA"`/`"SHIP(avx2)"`/`"MEASUREMENTS.csv"`，**从不交叉核对数值**。
- **正本文档被自身引证的 CSV 证伪**：`docs/engineering/ISA_VARIANTS.md:21` 定 SHIP 阈值「增益 ≥ +10%」，`:33` 却称「calibration-pixel-transform 与 hips-bulk-transform 的增益**均过阈值**」—— 被 `:33` 自己引用的 `ISA-003/MEASUREMENTS.csv` 里 calibration 是 **-130.7%**。`:28` 另称该批「记为 **+39.8%**」，CSV 中无此数（实为 hips +30.9 / calibration -130.7）。

**③ 须修 · `eng/tests/monitoring/test_run_graph_render.py:155-156` 代数恒等式型恒真门，占据了一个验收项的位置。**
```python
self.assertNotIn("graphviz", src.lower().replace("graphviz", ""))  # 无强依赖
```
`x.replace("graphviz","")` 把 needle 全部从 haystack 删掉 ⇒ 右侧**对任意输入都不可能含 "graphviz"** ⇒ 断言恒真。
且这不是假想：`graphviz` 在被测工具 `eng/tools/graph/render_run_graph.py:16,18,20` **真实出现 3 次**，内容恰是「无需外部 graphviz」「主机无 Graphviz 时不假装 dot 调用成功」—— 正是该门要证的语义。作者为消假红把判据本身删空了，G5 验收项因此**零覆盖**。对照：同测试 `:152` 的 `assertNotIn("subprocess.run", src)` 是**真门**（工具中 `subprocess` 出现 0 次），故 G5 只被门了一半。

---

## 3. 逐文件清单

> 每行 = 本人读到的内容 → 看到什么 → 判定（带 `文件:行`）。

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `conformance/echo/README.md` | 模块标识/负责范围/实现事实/验证 | `:74` 称判据在 `eng/tests/unit/echo_host_callback_test.c` —— 该路径**不存在**，实际在 `eng/tests/conformance/echo/tests/unit/echo_host_callback_test.c`；`:73` `lib/include/acsd/echo/types.h` 在模块内亦无此路径（实为 `include/acsd/echo/types.h`）。同表 `:72` `src/echo_module.c` 用模块内相对约定 ⇒ 三行混用两种约定 | 建议 |
| `conformance/echo/module.yaml` | 28 行元数据 | 字段自洽；`determinism: fixed_reduction_order` 与无科学含义模块相容。`:3` 引 `11_MODULE_SOURCE_TEST_STANDARD.md §4`（裸编号，未给文档名） | 建议 |
| `validation/.../c_delta_composition/check_renders.py` | 全 10 行 | `:4` **硬编码绝对路径** `ROOT='/workspace/Astro CS Database'`；`:5-7` 引用的 3 个 FITS（`run/RELEASE-02/L4-rebuild/{p3_r_vis,p3_r_vis_fixed,p3_upmfix}/output_phase3.fits`）**本人逐一核实全部不存在**；且 `run/*` 被 `.gitignore:17` 整树忽略 ⇒ 任何干净克隆永不可再生。脚本是僵尸代码 | 须修 |
| `config/fixtures/negative/run_manifest_hardware_field.json` | 全 21 行 | 相对 `positive/run_manifest.example.json` 唯一差异是 `:20` 多一个 `"workers": 8`。**该键为「负例」的唯一载体**——但本片无任何判据消费本文件（见 §5 反例 A） | 建议 |
| `config/fixtures/positive/run_manifest.example.json` | 全 20 行 | 自洽，无 `workers` 键 | 通过 |
| `config/fixtures/positive/normalize_blocks.phase_config.json` | 全 44 行 | 两 block 结构自洽；`path/to/*` 为占位路径 | 通过 |
| `unit/p2_identifiability/CMakeLists.txt` | 全 9 行 | **证伪了子代理的怀疑**：目录未被删也未脱离构建 —— 根 `CMakeLists.txt:1497` `add_subdirectory(eng/tests/unit/p2_identifiability p2_identifiability)` 真实注册。`:6-7` 无条件链接 `acsd_phase2`/`acsd_platform_math` 与 p1_psfw 的 `if(TARGET)` 守卫不同，但本文件无独立 configure 路径，故正确 | 通过 |
| `unit/p1_psfw/CMakeLists.txt` | 全 67 行 | `:63-67` **故障注入自检** `WILL_FAIL TRUE`，注入名 `oracle_mc_var,winfo_wn,neg_w1` —— 这是本片最强的反恒真机制（判据被证明「能红」）。`:52-54` 的 `if(TARGET)` 守卫附实测依据（FINAL-07 独立 configure 退化成 `-lacsd_platform_math`），是有出处的守卫不是掩盖。`:25-28`+`:39` 把三个生产 `.cpp` 直接编进测试目标，用测试自己的 include 面 ⇒ 生产侧 include 配置错误时本测试照绿 | 通过（1 条建议） |
| `unit/p1_psfw/p1psfw_oracle.hpp` | 全 258 行 | **真独立 Oracle，非自洽**：`:36-66` Gauss-Jordan 全主元 `long double` 显式求逆 vs 生产 Cholesky/Woodbury（算法层+精度层双重独立）。`:162-179` 的 `o_cholesky_factor` 与生产同构，但**仅**被 `:227` 当 MC 数据生成器用，从不与生产对比 ⇒ 风险封闭。`:122` `1.482602218505602` 无出处注释 | 通过（2 条建议） |
| `unit/p1_psfw/p1psfw_tests_winfo.cpp` | 全 152 行 | 每个量有两条独立 Oracle 路径交叉（`:36` 闭式 1e-12 / `:41` 显式逆 1e-9）。`:134-135` 锚在**Oracle 的 `ow2`** 而非 `rep` 自身字段 ⇒ 非自指（本人逐项追踪确认）。`:121`/`:151` 是「变异见证」而非独立门，与 `:42`/`:149` 冗余 —— 但**手算证明它们是真门**（见 §5 反例 C/D），故不构成恒真门指控 | 通过 |
| `unit/p2_rej/oracle_expected.inc` | 全 164 行 | `:1` 声称由 `oracle_rej.py` 机械生成 —— 生成器**真实存在**（`eng/tests/unit/p2_rej/oracle_rej.py`），`p2_rej_test.cpp:29` 是唯一 includer。本人独立重算 case 0 的 `sigma_eff`/`z`/`reason` 全部逐位吻合 ⇒ 非快照。**但 `:135` 有零填充伪装**：case 3 `max_abs_reliability_dev = 0.0`，而同文件 `:137-139` 显示 `bins_used=0`/`bins_skipped_small=10`/`used_samples=0` —— **10 个 bin 全被跳过、一个都没算，却报出「最大可靠性偏差 = 0.0」这个完美分数** | 须修（见 §4） |
| `backend/test_isa_avx2_fma.py` | 全 96 行 | 见 §2②。自愈 + 恒真决策列 + 悬空文档路径（`:88` `docs/architecture/` 目录不存在，正本在 `docs/engineering/`） | **阻断** |
| `backend/syn008_seam_main.cpp` | 全 153 行 | `:136` `if (star) cross_seam.push_back(d * 0.0);` 把星点 cell 的真实 seam **乘 0 丢弃**；`:147` `smax = cross_seam.back()` 正是这个被剪裁分布的极值。`:8` 文件头声称「星点 cell **保留**(星 flux 不因校准爆 break)」—— 与 `:136` 直接矛盾，且全仓无任何星 flux 保持性检查。`:4` 引 `SCI-005` 在 `docs/` 中 0 命中 | 须修 |
| `unit/variance_floor_test.cpp` | 全 237 行 | **本片设计最好的判据**：`:10` 明写「每条都能红能绿，无恒 PASS 占位」；`:102-112`（V2）与 `:222-227`（V7）是**显式红锚**。本人独立复核 α 边界算术：`sqrt(1.4013e-45/1e-12)=3.743e-17 > kAlpha=2.3847e-17` ⇒ `1e-12·α²=5.687e-46 < 1.4013e-45` 确实下溢为 0，**红锚成立**。`:64-67` 主动登记旧出处已作废并给出更正证据（与「代码改了、归档没重跑」相反的正确做法）。`:141` 注释「全部 ≥ med」表述错误（`v=med·1e-2 < med`），结论仍成立 | 通过（2 条建议） |
| `arch/test_budget_contract.py` | 全 92 行 | `:79-88` 真门（真编译 driver、真调生产 `host.budget.acquire`），`max_hold ≤ budget` 且 `granted == nreq`（防饿死）双向覆盖。`:36` 的 `while(true){ if(acquire!=0) continue; ... }` 是忙等自旋但有 `sleep_for(20ms)` 让出，真并发成立。`:53` `skipUnless(g++)` 是条件声明式 | 通过 |
| `quality/test_pack_audit_exclusion.py` | 全 83 行 | `:56-62` **解耦回归哨兵**：把 `DENIED_SAMPLE` 强行加回白名单后仍必须拒 —— 这是真反恒真设计。`:66-67` `hasattr(legacy_main)` 缺失时 `self.fail()` 而非 skip ⇒ 失败关闭正确。`:76-79` 端到端 zip 条目名断言。`:27` `DENIED_SAMPLE` 仅为路径名、不读内容，符合 AGENTS.md §5 | 通过 |
| `unit/cpu001_negative_test.cpp` | 全 102 行 | `:55-81` ①② 是真门：把 `sha256` 设为占位文件的**真实哈希**使 preflight 越过 hash/ABI 直达 ISA 分支，断言 `FALLBACK_BASELINE` + reason 含 `"unsupported ISA"`。但 `:92` ③ 偏弱：`CHECK(r.decision != REJECT_SECURITY)` 是**单向否定**，且 `baseline.so` 根本不存在 ⇒ preflight 在「文件缺失」步就返回，永远到不了 ISA 检查。注释说的覆盖面比实际宽 | 建议 |
| `unit/cpu_profile_test.cpp` | 全 74 行 | **恒真门**：`:59-63` 的 `bad = "{\"kind\":\"wrong\",...}"` 是本行刚构造的局部字面量，`:62 CHECK(bad.find("acsd_cpu_profile") == npos)` **零生产代码、恒真、永不判红**，却被 `:58` 注释标为「无效 profile 回退语义（CLI validate 逻辑）」。`:53-56` 的「失效(stale)判定」正文只有一行基线 ISA 断言，**该用例不存在**；`:65-66` 自认「上面已查」，**无独立测试体**。文件首行宣称测三维度，实际只有字段完整性（`:40-51`，这部分是真的） | **须修** |
| `unit/aio/tmp_path_uniqueness_probe.cpp` | 全 48 行 | **不是僵尸判据**：本人核实 `eng/tests/unit/aio/CMakeLists.txt:124-129` 真实 `add_executable` + `add_test(NAME aio_tmp_path_uniqueness ... TIMEOUT 300)`。`:42-45` 重复即 rc=2 判红，真门。`:23` 默认 target 写死 `/tmp/p173_target.fits`（跨平台硬编码，但 ctest 调用 `:128` 显式传 threads/iters，未传 target 故走默认值） | 通过 |
| `eng/tests/unit/CMakeLists.txt`（非本片，支撑取证） | 18 处 `add_subdirectory` + 18 处 grep 命中 | 本地相对注册只有 `p1wcs`/`p1snr`；`p1_hips/adapter_test.c` 由 `:668` `add_executable(hips_writer_adapter_test p1_hips/adapter_test.c ...)` 注册 | 支撑 |
| 根 `CMakeLists.txt`（非本片） | `:345,1469-1511` | `:1489` `add_subdirectory(eng/tests/unit/p1_psfw p1_psfw)`、`:1497` `.../p2_identifiability ...` —— **证伪「测试目录未纳入构建」的怀疑** | 支撑 |

---

## 4. 发现清单

### 阻断（2）

- **B-1** `eng/tests/api/test_cli_protocol.py:24` 判据读已删除文档（`dc5a2122` 删除，现行 `docs/engineering/`），`setUpClass:86` 抛 `FileNotFoundError`，7 用例全 error，**0 条断言**。影响：API-002 协议合同机器门形同虚设。修法：`:24` 改指 `docs/engineering/CLI_PROTOCOL_V1.md`（改后 `:101/:163/:175/:184-185` 仍有 4 处必红，需一并对齐现行正本措辞）。
- **B-2** `eng/tests/backend/test_isa_avx2_fma.py:74-85` 自愈（写 git 跟踪台账再断言其存在）+ `:84` 恒真决策列（按算子名不读增益符号）。台账现存 `calibration,...,-130.7,SHIP(avx2)` 实证；`docs/engineering/ISA_VARIANTS.md:33` 声称其「增益均过阈值」被自身引证的 CSV 证伪；`test_04:90-92` 只 grep 字面串永不核数 ⇒ 该矛盾在测试面**结构性不可检出**。

### 须修（7）

- **M-1** `eng/tests/monitoring/test_run_graph_render.py:155-156` `src.lower().replace("graphviz","")` 恒真门；`graphviz` 在 `eng/tools/graph/render_run_graph.py:16,18,20` 真实存在 ⇒ G5 验收项零覆盖。
- **M-2** `eng/tests/backend/syn008_seam_main.cpp:136` 星点 seam 乘 0 丢弃 ⇒ `:147` 的 `max` 结构上无法被任何星 cell 抬高；消费方 `eng/tests/api/test_seam_metric_gate.py:81-82` 恰以 `max <= 5σ(0.25)` 为硬门，`:86` 还明写「应远小于星幅度 40」⇒ 门在设计上就对星盲。`:8` 的「星点 cell 保留」为伪声明，全仓无星 flux 保持性检查。
- **M-3** `eng/tests/unit/cpu_profile_test.cpp:59-63` 纯字面量恒真门；`:53-56`/`:65-66` 声称的用例 3/5 无测试体。文件声称的三维度中两个是空的。
- **M-4** `eng/tests/unit/p2_rej/oracle_expected.inc:135` 零填充伪装：`bins_used=0`/`bins_skipped_small=10`/`used_samples=0`（`:137-139`）却报 `max_abs_reliability_dev = 0.0`。整体 `gate=0`/`status="INSUFFICIENT_SAMPLES"`（`:131/:140`）仍正确判红，故非恒真门；但**任何只读该标量而不先查 `bins_used` 的下游会把「一个 bin 都没算」读成「完美可靠」**。
- **M-5** `eng/tests/runtime/integration/test_rt003_budget_wiring.py:140-144` 死断言：`:139` 刚断言 `available()==3`，`acquire(2,2,NONBLOCK)` 必然成功 ⇒ `:142` 的 `return` 必然执行 ⇒ `:144` 的 `CHECK` **不可达**。文件头 `:7` 的「取消 → RAII 回收」验收项无门。
- **M-6** `eng/tests/runtime/integration/test_rt003_budget_wiring.py:208-211` 判据算的是桩：`min(模块请求,预算)` 三元式是**测试自己写的**，`:211 CHECK(host_workers == 2)` 恒真且**未调用任何生产 host-cap 代码**（`:201` 验收项「ctx.budget 注入 host workers 上限」结构上不可能翻红）。
- **M-7** `eng/tests/validation/release02/c_delta_composition/check_renders.py:4-7` 硬编码绝对路径 + 引用 3 个**全部不存在**且被 `.gitignore:17 run/*` 覆盖的 FITS ⇒ 僵尸脚本。

### 建议（9）

- **S-1** `conformance/echo/README.md:74`/`:73` 判据/头文件路径与实际不符，且三行混用两种路径约定。
- **S-2** `unit/p2_identifiability/CMakeLists.txt` 虽通过，但根 `CMakeLists.txt:1502` 注释仍写退役路径名 `v6_p2_identifiability`。
- **S-3** `p1_psfw/CMakeLists.txt:25-28,39` 把生产 `.cpp` 编进测试目标并用测试自己的 include 面 ⇒ 生产 include 配置错误时本测试照绿；建议补一个走生产 target 的链接型冒烟。
- **S-4** `p1psfw_oracle.hpp:162-179` 的 `o_cholesky_factor` 与生产同构，建议加注释声明「仅用于造 MC 数据，不与生产对比」；`:122` `1.482602218505602` 补出处。
- **S-5** `p1psfw_tests_winfo.cpp:121`/`:151` 是变异见证而非独立门，建议并入 `ACSD_P1PSFW_FAULT` 故障注入面。
- **S-6** `unit/cpu001_negative_test.cpp:92` 单向否定且 `baseline.so` 不存在 ⇒ 应补具体决策断言（应为 `FALLBACK_BASELINE` + reason 含 file missing）。
- **S-7** `syn008_seam_main.cpp:8` 伪声明「星点 cell 保留」；`:4` 引 `SCI-005` 在 `docs/` 0 命中；`:21` `kLeafPerTile` 定义后从不使用；`:122` `if (fmax < 1) continue;` 不可达。
- **S-8** `variance_floor_test.cpp:141` 注释表述错误（「全部 ≥ med」实为起点 `med·1e-2 < med`）。
- **S-9** `unit/aio/tmp_path_uniqueness_probe.cpp:23` 默认 target 硬编码 `/tmp/`，与文件自身 `:31-40`（cpu001）的跨平台目录 fallback 精神不一致。

---

## 5. 你主动构造的反例

| # | 构造什么 | 期望推翻什么 | 结果 |
|---|---|---|---|
| **A** | 取负例 fixture `config/fixtures/negative/run_manifest_hardware_field.json` 的唯一差异键 `workers`，全仓 grep 找消费者 | 若无任何判据消费「含 `workers` 键 ⇒ 必拒」这条语义，则该负例是**摆件**，负例面名义存在实则无门 | **推翻成功**。全仓无判据引用该 fixture 的 `workers` 语义（`test_export_crop_contract.py` 不在本批已核范围，但本批内无任何文件引用它）。⇒ 记为 S 级：负例夹具存在但语义无门，**非恒真门，但属「名义负例」** |
| **B** | 检验 `test_isa_avx2_fma.py:84` 的 SHIP 决策列是否可能对「变体更慢」判红 | 若决策列读 `improvement_pct` 符号，则 `calibration -130.7%` 必不盖 SHIP | **推翻成功（判据被证伪为恒真）**。`:84` 条件只含 `op in ("calibration","hips")`，与 `imp` 的正负无关；台账实测 `calibration,...,-130.7,SHIP(avx2)` 已在库 ⇒ 门在真实数据上已经绿着放行了一个慢 2.3 倍的变体 |
| **C** | 对 `p1psfw_tests_winfo.cpp:151` `fabs(sumw - fe.var_f)/fe.var_f > 1e-3` 尝试按「代数恒等式恒真门」推翻 | 若该式对任意 `sumw` 恒真，则无判别力 | **推翻失败 ⇒ 是真门**。独立手算：`d_k=(k+1)P` 只影响 `q`，`w_info` 与 `d` 无关 ⇒ `sumw=4·a²·PᵀC⁻¹P`；以 C=AR(1)(var=4,ρ=0.7)、a=0.85 估 `sumw≈7.04`，`var_f≈0.142`，比值≈48.5 ≫ 1e-3。若生产把 `var_f` 误写成 `sum_w`（去掉倒数），比值→0 < 1e-3 ⇒ **判红** |
| **D** | 对 `p1psfw_tests_winfo.cpp:121` 同法尝试 | 同上 | **推翻失败 ⇒ 是真门**。`wn_misuse=a²/(4·A_NEA)≈0.1806` vs `e2.w_info≈1.759`，比值≈0.897；若稠密路径退化为白噪近似 ⇒ 判红 |
| **E** | 对 `variance_floor_test.cpp` V2/V7「判别力红锚」尝试按「恒真门」推翻 | 若红锚只由常量算术决定、无生产依赖，则不能红 | **部分推翻**。`:102-112`/`:180-186`/`:222-227` 的红锚确实**零生产代码**，只有改 `kAlpha` 才可能翻红 ⇒ 是恒真断言。**但危害有限**：真正承重的绿锚（V1/V3/V4/V5/V6）全部走真生产函数 `derive_variance_floor`/`apply_variance_floor`/`to_product_dtype_keeping_availability`，能红。故判为「命名误导」而非「掩盖缺陷」，未升级为阻断 |
| **F** | 对 `syn008_seam_main.cpp` 试图构造「星点 cell 的 seam 大到足以破门」的注入 | 若星点 seam 真被计入，`max <= 5σ=0.25` 必红 | **推翻成功（证伪门的判别力）**。`:136` 把星 cell 的 seam 乘 0 ⇒ 任何星 cell 的 seam **无论多大**都不能抬高 `:147` 的 `max` ⇒ 消费方 `test_seam_metric_gate.py:81` 的硬门对该失效模式**结构性免疫**。而星点幅度 `kStarAmp=40`（`:26`）vs σ=0.05（`:25`）为 800:1，UPM 平滑项最易在此破裂 —— 被排除的恰是最具诊断力的样本 |
| **G** | 对 `p1psfw` Oracle 试图证明其「与生产同公式 ⇒ 自洽」 | 若 Oracle 用与生产相同的 Cholesky 公式，则转置类缺陷会同现 | **推翻失败 ⇒ 非自洽**。`p1psfw_oracle.hpp:36-66` 用 Gauss-Jordan 全主元 `long double` 显式求逆，生产走 Cholesky/Woodbury `double`，算法层+精度层双重独立 |
| **H** | 对 `p2_rej/oracle_expected.inc` 独立重算 `sigma_eff`/`z`/`reason` | 若 `.inc` 是生产输出快照，则非独立 | **推翻失败 ⇒ 非自洽**。本人按 `sigma_eff=sqrt(σ₁²+upm_var)`、`z=residual/σ_eff`、阈值 `z≤-4→1, z≥+3→2` 逐项手算 case 0 的 12 项，全部逐位吻合 `.inc:21-24`；生成器 `oracle_rej.py` 仅依赖标准库 |
| **I** | 对 `api/test_cli_protocol.py` 假设「路径修好后即可绿」 | 若修路径即恢复判别力 | **推翻成功**。除 `:24` 路径死门外，`:101`「恰一 acsd」、`:163-164` 反引号 `eng/tools/*.py`、`:175`「不得留下看似完整」、`:184-185` `### 7.1 命令树（唯一）`/`### 7.2 配置、事件与退出码` 在现行正本中均 0 命中（实际 `docs/ACSD_DESIGN.md:397`=`### 7.1 命令树`、`:419`=`### 7.2 机器输出与退出码`）⇒ 单改路径仍必红 4 处 |

---

## 6. 盲复算

**方法**：遮住子代理结论与既有判定，独立取证后对比。

| 断言 | 盲复算结论 | 与既有判定对比 |
|---|---|---|
| `p1_psfw` / `p2_identifiability` 两个测试目录是否真的纳入构建 | **一致**。二者均在根 `CMakeLists.txt:1489,1497` 注册；`p2_identifiability/` 目录与 `p2_identifiability_test.cpp`(23292B) 真实存在 | 与子代理 B 一致（子代理 B 亦证伪了任务书的怀疑）。本人在子代理回报前已独立得出同一结论 |
| `oracle_expected.inc` 是否为生产快照 | **一致（判否）**。独立重算 12 项逐位吻合；生成器仅标准库 | 一致 |
| `variance_floor_test.cpp` 的 V2/V7 红锚是否恒真 | **一致（判恒真，但危害被本代理下调）**。子代理判「须修」，本代理判「通过 + 2 建议」，理由：承重绿锚全走真生产函数，恒真的只是命名有误导性的见证门，未掩盖任何实现缺陷 | **偏松一处** —— 已记录。子代理的升级理由（命名会让人误以为已验证实现）成立，但不足以脱离「核心绿锚是真的」这一事实单独定须修 |
| `test_run_graph_render.py:155-156` 是否恒真 | **一致（判恒真）**。本人核实 `graphviz` 在工具中 3 次真实出现 | 一致 |
| `syn008_seam_main.cpp:136` 是否使 `max` 失真 | **一致（判失真），并加强**。本代理补充：消费方 `test_seam_metric_gate.py:86` 明写「应远小于星幅度 40」，说明**星盲是门的设计前提而非疏漏**，但 `:8` 的「星点 cell 保留」因此成为确凿伪声明 | 一致并加强 |
| `cpu001_negative_test.cpp:92` 覆盖强度 | **一致（判偏弱）** | 一致 |
| `abi002_lifecycle_probe.c` 是否为自洽式断言 | **一致（判否）** —— 子代理以「外部 schema + 独立 Python 镜像 + 16×op 全组合对拍」三条绑定推翻自洽指控；本代理采信但标注为**未经本人逐行复核**（该文件由子代理 C 覆盖） | 一致 |

**盲复算总判**：与既有判定 **基本一致**，在 9 项核心断言上无冲突；**偏松 1 处**（`variance_floor_test.cpp` 的严重度定级，已在上表标注理由）；**未见偏严**。本代理独立推翻任务书预设的一项怀疑（`p2_identifiability` 未纳入构建），并独立发现子代理均未点名的 **M-4（`.inc:135` 零填充伪装）**。

---

## 7. 子代理派发记录

**派发 6 个，分 3 个互不重叠的子集，每子集 2 个独立副本（用于交叉复现一致性）。** 副本机制使每个子集的结论被独立重做一次，可比对分歧。

| 子代理 | 子集 | 份数/行数 | 回报 | 复核结论 |
|---|---|---|---|---|
| A（`23755a53` + `ca7cb727`） | CLI 协议 / 监控 / DAG / 原子写 / 预算接线 | 7 / 2498 | 已收 | **全部采信**，三条主项均已本人复核为真 |
| B（`d45c2f57`） | 数值 oracle / 恒真门候选（10 份） | 10 / 1337 | 已收 | **采信 2 条、降级 1 条、修正 1 条**（见下） |
| C（`1bb0ac82`） | backend / abi / cpu / validation 脚本 | 13 / 1691 | 已收 | **采信**，三条主项均已本人复核为真 |
| D（`d1c4cfb0` + `c734e235`） | 大体量 unit C/C++ + quality/arch/config | 14 | **未在本次会话内回报** | 不采信、不背书 |

### 7.1 我否决 / 降级了子代理的哪些结论

1. **否决：B 组「`variance_floor_test.cpp` 须修」→ 降级为「通过 + 2 建议」。**
   理由：子代理自己也核实了「真绿锚 V1/V3/V4/V5/V6 全部依赖真生产函数，能红」。在一个文件里，恒真的只是三处**见证型**红锚（且文件 `:10` 已诚实声明用例意图），而全部承重断言为真 ⇒ 按「是否掩盖真实缺陷」这一本轮主判据，不构成须修。已在 §6 记为**偏松一处**，供负责人裁量。

2. **否决：C 组「`loader_probe_main.cpp` 恒 return 0 = fail-open」的初始怀疑（该怀疑由 C 组自己推翻，我确认其推翻成立）。**
   理由：`:4` 显式披露「拒绝≠崩溃」的设计意图，且两个消费方按 `LOADED`/`REJECT_SECURITY`/`FALLBACK`/`SELFTEST_OK` 决策串判定而非只判 rc。**承认推翻成立**，仅保留「文件缺失未与内容非法区分」为须修。

3. **否决：B 组任务书项「`p2_identifiability/` 可能整目录已删或从未纳入构建」。**
   理由：**本人独立证伪**（根 `CMakeLists.txt:1497` 真实注册 + 目录与源码在位）。该怀疑在派发前即被我自己的取证推翻，不计入子代理有效发现。

4. **修正：C 组「`syn008_seam_main.cpp:7` 声明的 p95/max 门限无人执行」。**
   理由：C 组自行核实消费方 `test_seam_metric_gate.py:79-82` 确实断言两条阈值 ⇒ **门限真实落地**，我否决「空头支票」的表述。保留并加强其真正成立的子结论：`max` 的样本被 `:136` 预先剪裁。

5. **不采信：D 组全部结论**（未回报）。

### 7.2 本代理独立发现、子代理未点名的问题

- **M-4** `eng/tests/unit/p2_rej/oracle_expected.inc:135` 零填充伪装（`bins_used=0` 却报 `max_abs_reliability_dev=0.0`）。A/B/C 三组均判该文件「通过」。
- **`:5` 负例 fixture 语义无门**（反例 A）。
- **`p2_identifiability/CMakeLists.txt` 通过**（在派发前即独立证伪了任务书怀疑）。
- **本片未被任何一方点名的正面发现**：`test_budget_contract.py`、`test_pack_audit_exclusion.py:56-62`、`tmp_path_uniqueness_probe.cpp` 的接线与真门、`variance_floor_test.cpp` 的 α 边界算术独立复核。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# —— 覆盖口径 ——
sed -n '1674,1722p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml \
  | grep -oP '(?<=- ")[^"]+' > /tmp/eng011_files.txt
wc -l /tmp/eng011_files.txt          # 48
while IFS= read -r f; do wc -l < "$f"; done < /tmp/eng011_files.txt | paste -sd+ | bc   # 10430

# —— B-1 阻断：判据读已删除文档 ——
sed -n '24p;86p' eng/tests/api/test_cli_protocol.py
ls docs/api                          # No such file or directory
find docs -name CLI_PROTOCOL_V1.md   # docs/engineering/CLI_PROTOCOL_V1.md
git -c core.quotepath=false log --oneline --diff-filter=D -- docs/api/CLI_PROTOCOL_V1.md   # dc5a2122
grep -n "恰一 acsd" docs/engineering/CLI_PROTOCOL_V1.md | wc -l   # 0

# —— B-2 阻断：自愈 + 恒真决策列 ——
sed -n '74,85p' eng/tests/backend/test_isa_avx2_fma.py
git -c core.quotepath=false ls-files --error-unmatch "实验/engineering-evidence/prerelease-v5/ISA-003/MEASUREMENTS.csv"
cat "实验/engineering-evidence/prerelease-v5/ISA-003/MEASUREMENTS.csv"   # calibration,...,-130.7,SHIP(avx2)
sed -n '21p;28p;33p' docs/engineering/ISA_VARIANTS.md
ls -d docs/architecture            # No such file or directory

# —— M-1 恒真门 + graphviz 真实存在 ——
sed -n '152,157p' eng/tests/monitoring/test_run_graph_render.py
grep -n "graphviz\|Graphviz" eng/tools/graph/render_run_graph.py      # :16 :18 :20

# —— M-2 seam max 被剪裁 ——
sed -n '8p;25,26p;136,137p;147p' eng/tests/backend/syn008_seam_main.cpp
sed -n '79,86p' eng/tests/api/test_seam_metric_gate.py

# —— M-3 恒真门 + 声称用例缺失 ——
sed -n '1p;53,56p;58,63p;65,66p' eng/tests/unit/cpu_profile_test.cpp

# —— M-4 零填充伪装 ——
sed -n '131p;135p;137,140p' eng/tests/unit/p2_rej/oracle_expected.inc

# —— M-5 / M-6 死断言 + 判据算桩 ——
sed -n '138,144p' eng/tests/runtime/integration/test_rt003_budget_wiring.py
sed -n '206,212p' eng/tests/runtime/integration/test_rt003_budget_wiring.py

# —— M-7 悬空归档 ——
sed -n '4,7p' eng/tests/validation/release02/c_delta_composition/check_renders.py
for p in run/RELEASE-02/L4-rebuild/p3_r_vis/output_phase3.fits \
         run/RELEASE-02/L4-rebuild/p3_r_vis_fixed/output_phase3.fits \
         run/RELEASE-02/L4-rebuild/p3_upmfix/output_phase3.fits; do ls "$p"; done  # 全部 No such file
git check-ignore -v run/RELEASE-02 ; grep -n "^run/\*" .gitignore   # :17

# —— 反例 C/D 独立手算依据（p1psfw_win_info 变异见证是真门）——
sed -n '121p;151p' eng/tests/unit/p1_psfw/p1psfw_tests_winfo.cpp
sed -n '36,66p' eng/tests/unit/p1_psfw/p1psfw_oracle.hpp   # Gauss-Jordan 显式逆，与生产 Cholesky 独立

# —— 反例 E：variance_floor α 边界算术复核 ——
sed -n '61,70p;102,112p' eng/tests/unit/variance_floor_test.cpp

# —— 反例 H：oracle_expected.inc 独立重算 ——
sed -n '5,7p;21,24p' eng/tests/unit/p2_rej/oracle_expected.inc
sed -n '1,13p' eng/tests/unit/p2_rej/oracle_rej.py

# —— 反例 A：负例 fixture 语义无门 ——
diff eng/tests/config/fixtures/positive/run_manifest.example.json \
     eng/tests/config/fixtures/negative/run_manifest_hardware_field.json   # 仅多 "workers": 8

# —— 支撑取证：测试目录确已纳入构建 / 探针确已被 ctest 消费 ——
sed -n '1486,1497p' CMakeLists.txt
sed -n '78,96p;124,129p' eng/tests/unit/aio/CMakeLists.txt
sed -n '668,669p' eng/tests/unit/CMakeLists.txt
```

---

## 9. 计数口径说明

本片为**测试/夹具层**，不含门登记表，故不涉及「门实例 / 去重门 / 整改分母」三层分母之争。涉及的计数只有两处，均已在正文标明口径：

- **覆盖率口径**：分母 = 权威清单 `实际行数` = 10430 行（48 份），分子 = 本人逐字读完的成员行数 = 1774 行（19 份）⇒ **17.0%**。子代理覆盖的 5526 行**单独列示，不并入本人分子**。
- **断言条数**：本片发现的恒真门（3 处：M-1、M-3、M-6）与死断言（2 处：M-5、零填充 M-4）均按**具体 `文件:行`** 计，不做去重推断。
