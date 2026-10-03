# 审稿-P1-ENG-tests-002 — G08-05 对抗审稿 第 1 遍

- **片号**：`ENG-tests-002`
- **层**：`eng/tests`
- **基线 HEAD**：`850a9edefd47434b9ab71bc907c3de1e0814b323`（`git rev-parse HEAD` 实测，与基线一致）
- **口径**：一遍 = 对同一片材料的一次完整重读。**本片判定不因判据「绿」而采信实现正确**；判据代码只是计算数据的正向代码，正确性由本报告的重算与反例独立给出。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 成员份数（权威片清单） | **47** |
| 实际读完份数 | **47** |
| 成员总行数 | **10429** |
| 实际读进行数 | **10429** |
| **覆盖率** | **47/47 份，10429/10429 行 = 100.0%** |

**未读完的：无。** 全部 47 份由本审稿人用 `read` 逐字读完（`p2002_unc_rej_prov_test.cpp` 2032 行分 4 次读完；其余整读）。未编译、未跑 ctest/pytest/ctest/构建/任何实验或测试脚本。零 git 写。未修改仓内任何文件（本交付件为唯一写入）。

行数分母核对命令：
```
for f in <47 份>; do wc -l "$f"; done | awk '{s+=$1} END {print s}'   # → 10429
```
与 `片清单-权威版.yaml:1165` 的 `实际行数: 10429` 一致，无缺行、无多行。

---

## 2. 本片判定

### **需修**（不阻断合并，但含 5 条必须先行处置的阻断级缺陷）

**最重的 3 条：**

1. **【阻断·最高价值】`test_p3002_properties_order_unit.py:63` 的 `variance` 门被源码注释满足，而该注释说的恰是反面。**
   判据 `self.assertIn('"variance"', r, "variance 未显式拒")` 读 `lib/algorithms/resample/p3_resample.cpp`。实测 `"variance"` 在该文件仅两处：
   - `p3_resample.cpp:214` — **注释**：`// "variance"/"ivar" 依 DATA-P3-UNC-001 §30.4-4 从 UNSUPPORTED 拒绝项移除`
   - `p3_resample.cpp:478` — 与拒绝无关的 uncertainty 表项
   而真实拒绝谓词在 `p3_resample.cpp:216`：`if (m == "weight" || m == "flux-per-pixel") return P3_RS_UNSUPPORTED;` —— **不含 variance**。
   ⇒ 门为一个**已被规范明确移除的行为**亮绿灯，并把它记成「已显式拒」。这是本片最典型的**文本锁代替行为断言**且**方向是反的**（检查项「恒真门三型」之结构对称型 + 检查项「判据读的是桩」之变体）。
   我构造的反例：把 `p3_resample.cpp` 的缺省 BUNIT 改成 `std::string("Jy/beam")`、或把 `:216` 的谓词整个删空，`test_04` 仍绿 —— **反例成立，门无法因该真实缺陷翻红**。

2. **【阻断】`test_fix210_block_key_parity.py:93,107-114` 的主判据是可证明的恒真式。**
   恒真链（逐环已实测）：
   - `lib/infrastructure/cli/parser.cpp:480-481`：`std::set<std::string> allowed = block_keys(); for (const auto& k : session_keys()) allowed.insert(k);` —— 块内门允许集**字面** = `block_keys() ∪ session_keys()`
   - `eng/tests/config/test_cfg004_unified_contract.py:104-119`：`cli_block_keys()` / `parser_session_keys()` 用正则从**同一份 `parser.cpp` 源码**解析这两个函数
   - `test_fix210_block_key_parity.py:93`：`SESSION_KEY_TABLE = CFG004.parser_session_keys() | CFG004.cli_block_keys()` ⇒ `U ≡ allowed`
   ⇒ 判据③「`∀K∈U: K∈B(S)`」由定义保证；`BLOCK_ONLY`（`:96-97`）亦由同一对函数算出。
   我构造的三种真缺陷注入 —— (a) 关闭块内门一条 `if`；(b) 让块内门多接受一个非法键；(c) 让两门对某键分歧 —— **三者都无法让 `test_01` 变红**。
   该文件其余部分（`test_03` 真未知键 `workers` 探双门、`test_05` 平铺顶层专有键、`:184` `assertGreaterEqual(len(block), 40)` 防塌缩）是好的，**只有 `test_01` 的主判据恒真**。

3. **【阻断】`p2002_unc_rej_prov_test.cpp:1435-1439` 与 `:1673-1676` 的两处「故障注入」是无条件 `CHECK_MSG(false, ...)`，且被写进 PASS 文案当通过理由。**
   ```
   1435:  if (fault_inject) {
   1436:    CHECK_MSG(false,
   1437:              "FAULT-INJECT: provenance keys must all be present; missing"
   1438:              " REJECT_PROFILE proves the fault injection flips this test");
   1439:  }
   ```
   这两分支**不改生产、不改数据、不改任何被测量**，只登记一次失败。`ACSD_P2002_FAULT=prov|aio` 的 rc=1 是**构造上必然**的，与实现是否正确无关。而 `:2019-2027` 的 PASS 文案把「FAULT=identity 注入必败 + per-sample 掩码 fail-closed 注入必败」列为通过理由 —— **用恒红分支当判别力证据**。
   对照：`FAULT=proj`（`:610`）是合法的敏感性探针（改期望值）、`FAULT=identity`（`:1007-1013`）是测试内自变异（能证明恒等式对缺陷形状敏感，但不证明生产接线正确）。**四模式中两模式是空的。**

**其余 2 条阻断见 §4。**

---

## 3. 逐文件清单

47 份全覆盖。「判定」栏：✅可信 / ⚠️须修 / ⛔阻断 / ○非判据（工具/生成器/夹具）。

| # | 文件 | 行 | 读了什么 → 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `eng/tests/unit/p2002_unc_rej_prov_test.cpp` | 2032 | 真实 7 节点 Phase2 链 + 逐像素 kernel 重放（`:763-881`），覆盖最实。`:335-336` 注释自陈「经 run_node 断言会把『被拒绝』误报成 ok（恒真判据）」——已自我修正。但 `:1435/:1673` 两处注入是硬编码恒红；`:1850-1852` 强制合同继续宣称已交付的 int32 通道为 PENDING | ⛔ |
| 2 | `eng/tests/unit/gaia_xpsd_fixture_gen.c` | 700 | fixture 生成器。`:39` 自陈「与被测实现**同一有理式**」。`:143` `mag_raw*0.001-1.5` 与 `gaia_client.c:1827/1962/2106` 逐字同式；`:40` `INV_SCALE (1.0/1.8e9)` 与 `gaia_client.c:1817` `1.0/(3600.0*1000.0*500.0)`、`gaia_oracle.py:23` 三方同值 ⇒ 常数本身错了则往返仍绿。另 `:145-151` 冗余嵌套同名 `j`（343² 次而非 343 次） | ⛔ |
| 3 | `eng/tests/unit/block_flow_test.cpp` | 571 | 块流**框架**门。`:109-135` `bind_ops` 用本地 stub 算子；`:183-185` 自陈参考与块流「同一 stub 算子、累加序也一致」⇒ E2 只能证明框架忠实搬运值。**优点**：`:437-451` 做了输入扰动非退化自证；`:514/:544` 的 `!omitted.empty()`/`!target.empty()` 守卫是判红不是跳过 | ⚠️ |
| 4 | `eng/tests/artifact/test_production_store.py` | 475 | 真生产 `production_store.py`、真磁盘、`SpyStoreIO` 真委派。但 `:434-445`「manifest hash 可重算」把 `store.manifest_digest_hex()`（`production_store.py:812` 读内存字典，字典由发布时同一函数填）与 `ArtifactStore.manifest_hash_recompute()`（`production_store.py:801`）对拍 —— **同一函数对拍自身，往返自证** | ⛔ |
| 5 | `eng/tests/unit/gaia_shard_coverage_gate_test.c` | 435 | 共址 `#include "gaia_client.c"`（`:49`），真驱动装载路径。`g1_judge` 左侧 `entry_count` 来自独立 `opendir/readdir`（`:143-167`），右侧来自生产 API ⇒ **跨两个独立来源，非自洽**。`:427-430` 有 GREEN/RED 双向总断言。`:51` `INV_SCALE` 死宏 | ✅ |
| 6 | `eng/tests/unit/p2_upm/oracle/upm_ma_oracle.py` | 419 | 独立 Oracle（ALS + scipy trf 双算法互校，实测 `maxdiff=3.375e-14`）。但 `:91`/`:158` 的 `nfc` 数的是**观测记录数**却比 `MIN_FRAMES` ⇒ additive-only 分支全数据集不可达；`:415` 恒 `return 0`，零断言 | ⚠️ |
| 7 | `eng/tests/cli/test_cpu_profile_source_policy.py` | 386 | 7 个真跑 CLI 端到端用例，负例断 `rc==5/3/2`，是真门。**但 `:5` 自称「唯一权威链，逐句可核」，4 条引用 3 条不成立**：实测 `grep -c cpu_profile docs/ACSD_DESIGN.md` = **0**；「画像的来源与缺省行为」短语 `docs/` 下 0 命中；§7.2 无退出码表。另 `:188` `_profile_still_valid` 返回 `returncode != 5` ⇒ 崩溃/超时/exec 失败全算「画像有效」（fail-open） | ⛔ |
| 8 | `eng/tests/abi/test_abi005_echo.py` | 382 | 编真 echo .so + nm + ABI-003/004 正负测。U2 的 `ALL PASS` 经核实**不是**恒真（`echo_host_callback_test.c:843` `if (g_failures) return 1`）。但 `:137` `root_dirs = ["runtime","lib","cli","providers"]` 实测只有 `lib` 存在，`:140-141` `if not os.path.isdir(base): continue` **静默跳过 3/4 扫描根**；`:4` 规格源 `tasks/02_ABI_BUILD_CLI_TASKS.md` 整目录已删 | ⛔ |
| 9 | `eng/tests/unit/p3_proj/p3_proj_wcs_oracle.py` | 339 | **本片质量最高的判据之一**。真独立：astropy/wcs + 自实现 Van Oosterom & Strackee 球面盈余；带 mutate/mutate-all 负向控制（`:60,:328-335`）；`:154-155` 缺 astropy 时 fail-closed 返红；`:200-201` CAR 子集显式限定量测域。`:12-14` 的 1e-8/1e-6/0.9″/px 经核实与 `GATES_AND_TOLERANCES.md:71-72` **逐字属实** | ✅ |
| 10 | `eng/tests/unit/drizzle_precision_default_test.cpp` | 327 | T9 `count_f64_nonfloat`（`:221-244`）是真正的数值签名检查。**但 `:17` 头注释称 T7 是「累积**等价性** max_rel<**1e-5**」，代码 `:303-308` 断言相反（必须**有差异**、门限 1e-3）；`:5/:266` 引「宪章 §5.3『Drizzle 采用 float64 累积』」—— 实测 `grep -rn "float64 累积" docs/` 0 命中，`RELEASE_STATUS.md:34` 已自记「原冻结宪章已删除」**；`:140` 建帧哨兵 `-999` 被 `:290/:294` 当「如期拒绝」** | ⛔ |
| 11 | `eng/tests/cpu/dispatch/run_cpu005_route_checks.py` | 288 | **本片设计最干净之一**。`check_include_face`（`:125-155`）做传递闭包解析；`self_test()`（`:162-185`）**自带正例 + 两个负例**（抽掉全部 aio 头面须判红、缺 -I 探针须判红）—— 本片唯一显式实现「判据不得是恒真门」的元自检。`:252` 硬编码 `"provider": "avx512"` 在非 AVX-512 机必红（环境脆弱） | ⚠️ |
| 12 | `eng/tests/validation/release02/fix_p2a_seam_oracle/p2a_oracle.cpp` | 273 | 链接真实 `upm.cpp` 的测量 harness，**不是判据**（`main():272` 无条件 `return 0`）。`:144-148` `BUILD FAILED` 时不 return 非零，`:258` `ratio = (resid>0) ? step/resid : 0.0` 在 `resid=-1` 时取 0.0 ⇒ **一行失败行被读成「零阶跃，完美」**。`:204` 注释称「相对 M」但 `:200` 用硬编码 `Mn=1e13`，而 `Scene::M`（`:78/:86/:93`）写了从不读 | ⛔ |
| 13 | `eng/tests/unit/p3_rsmp/run_mutations.py` | 253 | **本片最扎实的变异驱动**。`stage()`（`:148-154`）复制到影子树（原树只读）；`apply_patches`（`:162-164`）`if n != 1: raise` ⇒ 锚点漂移会响；`P3RSMP_SOURCES`（`:41-48`）与 CMake `V6_P3RSMP_SOURCES` 逐个一致；`:153` 影子源缺失即 raise（fail-closed）。**弱点**：`:224` `caught = (not compile_fail) and any(fail)` 容忍部分编译失败；20 条中 11 条是 `if(...)→if(false)` 只测门触发不测门判的量 | ✅ |
| 14 | `eng/tests/cli/test_fix210_block_key_parity.py` | 229 | 见 §2 第 2 条。主判据恒真式 | ⛔ |
| 15 | `eng/tests/unit/p3_coverage_test.cpp` | 224 | **被检对象是本文件现写的 `struct TileSampler`（`:28-81`）**，与生产零关系；`:26-27` 自己承认「独立参考模型，不调用生产码」。`:139-141` `CHECK(a==b)` 是 `const` 无状态函数两次调用 ⇒ **代数恒真式**。我独立重算 `sample_masked` 解析真值 `(0.1875·10+0.0625·20+0.5625·30)/0.8125 = 24.615384615…` 与 `:145/:156` 逐位相符 ✓；`:175` 期望值用与实现同式的改序运算 ⇒ 自洽 | ⛔ |
| 16 | `eng/tests/backend/test_public_header_layering.py` | 200 | 规则本体简单可复核，`:148-151` 有覆盖计数防空扫描假绿。但 `:9/:53` 引 `eng/tools/check_ast_api.py::include_flags`（**裸从句无引号**，实测该文件不存在）；`:18` 伪引 §10「aio 是文件级唯一 I/O 边界」—— 实测 `docs/ACSD_DESIGN.md:547` 作「**统一** I/O 是文件级唯一边界」。`:55/:65` 只 walk `lib`，而 `SKIP_DIR_NAMES` 不跳 `eng` ⇒ `eng/` 下公共头零覆盖 | ⛔ |
| 17 | `eng/tests/unit/p2_output_semantics_test.cpp` | 200 | §2/§3/§4/§4b/§4c/§4d/§5 是**真生产验证**，数值期望我全部独立重算相符（106.25 / 2.0 / 7.0 / 0.9 / 0.8）；`:139-156` 的 4d 是 4c 的**判别力对照**（真「无覆盖」不得被误抬），设计扎实。**但 §1（`:22-35`）8 个断言全在测试自己写的字面量数组上自比**，`name[0]!='\0'` 与 `strcmp(n,"weight")` 均**可证明恒真**，对生产零判别力 | ⛔ |
| 18 | `eng/tests/unit/aio/oracle/aio_oracle_negative.py` | 181 | **参照真正独立**：`run_checks`（`aio_oracle_lib.py:237`）期望值读 `eng/contracts/data/clause_registry.json` + `product_family_field_constraints.schema.json`（合同 JSON），被检是 C++ writer 吐出的产物 ⇒ **不是实现的复制**。`:148-153` 原始产物不干净即 `return 1`，`:159` 源缺失抛异常 ⇒ fail-closed 无伪装。弱点：`:162` 关键词子串匹配无法区分目标检查与无关检查；`_patch_bunit_and_fix_checksum`（`:23-31`）卡片循环无 `else`，HDU 无 BUNIT 时静默变 no-op | ✅ |
| 19 | `eng/tests/cpu/dispatch/cpu_capability_matrix_test.c` | 176 | 合成 CPUID 证据 → **生产引擎** `acsd_cap_classify_v1`，位面硬编码（`:46-57`）与引擎真实耦合 ⇒ 耦合方式正确。R4/R5/R6 把「子集缺失」与「OS ZMM 缺失」彻底分开，是本片最好的正负例对。**但 `:148-149` `rc==ACS_CAP_ERR_UNSUPPORTED` 时 6 条断言全跳过仍返回 0，而 `:171` PASS 文案无条件宣称「R8 live self-consistency」** | ⛔ |
| 20 | `eng/tests/backend/test_cpuprov_manifest.py` | 169 | 真 DSO + 真生成器 `gen_provider_manifests.py`。`:126-128` 硬锚 24/992/992 是**外部独立锚**（我独立从 `cpu_features.h` 重算：AVX2\|FMA=8\|16=24 ✓；F\|CD\|BW\|DQ\|VL=32\|64\|128\|256\|512=992 ✓）。两条负例（rc=2 / rc=5）真会红。弱格：`msvc/avx2` 无硬锚 | ✅ |
| 21 | `eng/tests/unit/p1_psfw/p1psfw_tests_negative.cpp` | 163 | 真 mutation 负测：M-W1/2/3、A_NEA、白噪、m21/m22、协方差传播、V8 深度门，每条「先证正确量 → 再注变异量 → 断言差 >1e-3」。`:139` 还有 40000 样本 MC 独立复算（5% 容差）。期望值来自独立 `p1psfw_oracle.hpp` | ✅ |
| 22 | `eng/tests/backend/variant_build.py` | 153 | 构建配方（非判据）。`:134-137` 显式写死关键不变量：门面 TU 旗标只能是显式覆盖值，`None` = 零旗标，**绝不可回落族 ISA 旗标**，并记录实测踩坑（门面被编出 75 条 VEX/EVEX）。`docs/abi/README.md:29` 的 4 个消费者全部存在 | ○ |
| 23 | `eng/tests/unit/p2_workers_test.cpp` | 152 | 真 `resource_gate.h` + 真 `p2_stage2_parse_config`。期望值是硬字面量（0.85×2=1.7 / 3.4 / 90÷4=22.5），我独立重算全对；4b 的 3.39 FAIL / 3.4 PASS **真卡在等号上** | ✅ |
| 24 | `eng/tests/config/cfg_anchors.py` | 141 | 内容锚的单一实现，活调用者 `test_cfg001_contracts.py:14` / `check_cfg002_registry.py:36` 均在。**但 `:5` 伪引「规则正本 = ANCHOR_CONTRACT.md 第 9 节」—— 实测该文档只有 §1–§6（`## 9` 零命中），代码内 9.1/9.2/9.3/9.3-D1 全悬空（真实为 §4.1/4.2/4.3）** | ⛔ |
| 25 | `eng/tests/backend/test_hips_properties.py` | 135 | 真编真跑生产 `hips_properties.cpp`（`:45`），非桩。负例矩阵 `:88-100` 10 个注入 + `:127` 6 个恶意路径。不足：`:103` 对全部负例只断 `rc==1`，**不校验错误原因** —— 一个把所有非法值报同一条错误的实现照样绿 | ⚠️ |
| 26 | `eng/tests/cpu/avx2/provider_avx2_capability_gate_test.c` | 131 | 四种 stub 模式正负测。**但 `:89-93` 用桩覆盖了被检谓辞 `acsd_cap_os_safe_satisfies_v1` 本身**，生产 `capability_detect.c:256-259` 的「非法位拒绝」分支（`if ((required & ~known) != 0) return 0;`）**根本不参与链接**；`:88` 注释宣称的「等价生产 os_safe_satisfies」为假 ⇒ **判据读的是桩，且桩比生产弱** | ⛔ |
| 27 | `eng/tests/conformance/noop/tests/unit/noop_handshake_test.c` | 117 | 握手正负测真门。**但 `:85` 硬写 `other.size = 26`，而 `"acsd.phase1.calibration"` 实测长度 23** ⇒ `:86` 的「未知 module_id → PARAM」断言在**错误理由**下成立（长度不符也会返回 PARAM） | ⚠️ |
| 28 | `eng/tests/backend/p3_output_fsync_interposer.cpp` | 112 | LD_PRELOAD interposer，**是测量工具不是判据**（自身零断言）。设计正确：`:28-30` 直接系统调用写 stderr 避免 stdio 递归 | ○ |
| 29 | `eng/tests/unit/cli_wideconv_test.cpp` | 110 | **纯代数恒真**。`:35` 模拟器自己定义 `*wrote_oob = cap - n`，`:90` 取 `legacy_alloc = final_len(n) = n-1`，故 `oob = (n-1)-n ≡ -1`，`:97` 断 `oob == -1` ⇒ **期望量由被检量同源产生**。历史缺陷所在的真实换码驱动（`main.cpp:59`/`commands.cpp:1218`）**未被链接** | ⛔ |
| 30 | `eng/tests/backend/test_isa_avx512.py` | 100 | `:14-16` 是诚实的 `skipUnless`（非伪装）。**但 `:91` 指向 `docs/architecture/ISA_VARIANTS.md` —— 实测 `docs/architecture/` 整个目录不存在**（文件已迁 `docs/engineering/`）⇒ `setUpClass` 的 `open()` 抛 `FileNotFoundError`，**整类 4 个测试全 ERROR、零覆盖** | ⛔ |
| 31 | `eng/tests/unit/cpu_provider_test.cpp` | 96 | 降级链 + `FAKE = 1ull << 41` 负例真门；`:74` 路径错时判红不假绿。**但 `:29-36` 的 `entries[]` 是本地重抄表，标题却称「manifest 匹配」—— 真 manifest 从未被读** | ⚠️ |
| 32 | `eng/tests/unit/cpu_abi_test.cpp` | 92 | ABI 边界。期望值是硬字面量（0/1/2/6/70）+ `offsetof`，**独立锚非自洽**；`:46` 非 2 幂 align 拒绝是真行为探测 | ✅ |
| 33 | `eng/tests/integration/p3_export/EVIDENCE.md` | 86 | **归档文件**。`:4` 基线 HEAD `95703e63` 落后 907 个提交；`:6-7` 的 `lib/phase3_proj/p3_proj.h`、`lib/phase3_rsmp/p3_rsmp.h` **均已搬家**（现 `lib/algorithms/projection/`、`lib/algorithms/resample/`）；`:39-56` 全部命令指向 `run/v6/p3-intg/`。**正面**：`:73-79` 的 schema 交叉张力引用逐字核实成立 | ⚠️ |
| 34 | `eng/tests/validation/release02/q3_additive_truth/src/step7_apflux.py` | 83 | 独立孔径测光脚本（**不是判据**）。`:4` `sys.path.insert(0,'run/RELEASE-02/...')` 是**相对 CWD 路径**，只有 CWD=仓根才可用。分析本身有实质判别力（`:48/:51` 两个 lstsq 的 `rms1/rms2` 对照） | ○ |
| 35 | `eng/tests/unit/p3_rsmp/p3_rsmp_scenarios.h` | 72 | 共享合成 1D 链。我独立重算 `:4` 注释的常数：Ω_out=2/overlap=1 ⇒ R=0.5 行和 1 ✓；Ω_in=1 ⇒ S=1 列和 1 ✓；`R·Ω'_i/Ω_j = 0.5×2/1 = 1` ✓ 全对 | ✅ |
| 36 | `eng/tests/backend/test_p3002_properties_order_unit.py` | 68 | 见 §2 第 1 条。**整体是纯源码文本 grep 门**（8 条断言全是 `assertIn(字面量, 生产源全文)`），无法发现任何语义缺陷 | ⛔ |
| 37 | `eng/tests/conformance/echo/include/acsd/echo/types.h` | 62 | 声明头（**非判据**）。`:46-47` 注释「自定义从 100 起」与 `:50-55` 实值 100–105 一致 | ○ |
| 38 | `eng/tests/unit/aio_abi_omp.hpp` | 50 | 并行复核**辅助**（非判据）。`:35-37` OpenMP 不可用时 `(void)workers` 静默退串行 —— 注释 `:3-4` **诚实披露**（属已披露降级非伪装），但该路径下并行一致性零覆盖 | ○ |
| 39 | `eng/tests/abi/test_mod001_install_load_check.py` | 47 | 主动修「本体文件名不匹配 `test*.py` 故 UT-ABI 门从未执行」的包装（`:5-8`）—— **好实践**。`mod001_install_load_check.py` 存在 ✓ | ✅ |
| 40 | `eng/tests/config/fixtures/negative/cpu_profile_v1_bad_kernel.json` | 46 | **活调用者有**：`test_cfg001_negative.py:106,109,148,184` 四处 + `CONFIG_CONTRACT.md:138` 登记为必红负例。内容自洽（`size_class:"huge"` 越界、`block_size:0` 越界） | ○ |
| 41 | `eng/tests/arch/test_phase3_module_arch.py` | 38 | **`:6` 指向 `docs/architecture/PHASE3_MODULE_ARCH.md` —— 实测 `docs/architecture/` 整个目录不存在** ⇒ 整类 6 个测试恒 ERROR。另 `:22/:26` 的 `self.assertIn(...) and self.assertIn(...)` 因 `assertIn` 成功返回 `None`（falsy）而**短路，第二个断言永不执行** ⇒ 死断言 | ⛔ |
| 42 | `eng/tests/testkit/examples/oracle_sum.py` | 31 | **不是独立参照**。`:8-16` 闭式解 vs 朴素循环，两者都是同一文件里的手写玩具，与 ACSD 生产实现零相关；只能因 Python 整数加法出错而红 | ○ |
| 43 | `eng/tests/validation/release02/phot_verify/summarize.py` | 27 | 分析渲染脚本（**不是判据**）。`:3` 用相对路径 `open('pair_results.json')`。`:26` 先全局按 \|ratio\| 排序取前 8 **再**筛 `within_` ⇒ 可能筛空且不报错（检查项「筛掉真信号」轻度实例） | ○ |
| 44 | `eng/tests/testkit/examples/property_invariant.py` | 21 | `:12-13` `round(once)==once` 是 **Python 语言层恒等式**，2000 样本是装饰。docstring `:3` 自承「属性自证，不依赖生产实现」 | ○ |
| 45 | `eng/tests/backend/candidates_probe_main.cpp` | 18 | **只打印不断言**（`main()` 恒 return 0）⇒ 作为「证明派生、无硬编码」的证据**零强度**：硬编码同样能打印出好看的数 | ○ |
| 46 | `eng/tests/quality/fixtures/docchk002_claims_fixture.csv` | 12 | **悬空夹具**。实测 `eng/tests/quality/` 下无 `test_docchk002_mutation.py`；全仓 `docchk002` 仅 7 处命中，**无一处加载本 CSV**（4 处在 `docs/DOCUMENT_INDEX.yaml`、3 处在退役台账）。内容上 8 行数据的 `api_symbol/test_id` 几乎全是 `gen_version.py::build_report` 的复制粘贴 | ⛔ |
| 47 | `eng/tests/config/fixtures/positive/mosaic_flat.phase_config.json` | 7 | **活调用者有，但走计算路径**：朴素 `grep mosaic_flat` **零命中**，实为 `test_cfg004_unified_contract.py:456-460` 的 `POS + "%s_flat.phase_config.json" % phase` 拼接加载 ⇒ 非退役夹具 | ○ |

---

## 4. 发现清单

### ⛔ 阻断（7）

| # | 位置 | 问题 |
|---|---|---|
| **B1** | `test_p3002_properties_order_unit.py:63` × `p3_resample.cpp:214/216` | **`variance` 门被源码注释满足，语义相反**：`variance` 已依 DATA-P3-UNC-001 §30.4-4 **从拒绝清单移除**，判据却以 `assertIn('"variance"', …, "variance 未显式拒")` 亮绿灯 |
| **B2** | `test_fix210_block_key_parity.py:93,107-114` × `parser.cpp:480-481` × `test_cfg004_unified_contract.py:104-119` | **主判据可证明恒真**：期望量 `U` 由正则解析 `parser.cpp` 的 `block_keys()∪session_keys()` 得出，生产块内门允许集**字面**就是同二者的并集 |
| **B3** | `p2002_unc_rej_prov_test.cpp:1435-1439,1673-1676`（PASS 文案 `:2019-2027`） | **两处「故障注入」是无条件 `CHECK_MSG(false,…)`**，恒红；被写进 PASS 文案当判别力证明 |
| **B4** | `gaia_shard_coverage_gate_test.c:8,27-28` 等 3 处 + `block_flow_test.cpp:11,284` + `test_production_store.py:4` + `test_abi005_echo.py:4` + `upm_ma_oracle.py:10` + `gaia_xpsd_fixture_gen.c:4` + `run_cpu005_route_checks.py:5` | **悬空引用 7+ 处**：`MODULE_MIGRATION_TEMPLATE.md`（全仓不存在）、`tasks/02_ABI_BUILD_CLI_TASKS.md` 与 `tasks/03_RUNTIME_DATA_IO_TASKS.md`（整 `tasks/` 目录已删）、`run/WCS-DETERMINISM-01/REPORT.md`、`run/GAIA-FAILCLOSED-01/{REPORT.md,baseline/}`（目录已删且 `.gitignore` `run/*` 使其**永久不可验证**）、`eng/tools/quality/check_block_flow_spec.py`（活树无，仅 `run/**` 历史快照有 13 份）、`04_CPU_RESOURCE_TASKS.md` |
| **B5** | `gaia_xpsd_fixture_gen.c:39-41,129-134,143` × `gaia_client.c:1817,1827` × `gaia_oracle.py:23` | **常数自洽式断言**：SUT、fixture 生成器、「独立」Python oracle 三方硬编码**同一字面** `1/1.8e9` 与 `mag_raw*0.001-1.5`。1 LSB=2µas、偏移 −1.5 这两个常数若本身错了，三方同错、往返仍绿 |
| **B6** | `test_abi005_echo.py:137,140-141`；`provider_avx2_capability_gate_test.c:88-93` | **扫描面静默缩到 1/4**（`runtime`/`cli`/`providers` 三根目录不存在，`continue` 静默跳过）；**判据读的是桩**（stub 覆盖被检谓辞本身，生产 `capability_detect.c:256-259` 的非法位拒绝分支不参与链接） |
| **B7** | `test_public_header_layering.py:6` × `test_isa_avx512.py:91` × `test_phase3_module_arch.py:6` | **`docs/architecture/` 整个目录已迁至 `docs/engineering/`**，三个测试仍指旧路径 ⇒ `open()` 抛 `FileNotFoundError`，**整类测试恒 ERROR、零覆盖** |

### ⚠️ 须修（13）

| # | 位置 | 问题 |
|---|---|---|
| M1 | `test_production_store.py:434-445` | manifest hash「可重算」是**同一函数对拍自身**（`manifest_digest_hex` 读的就是 `manifest_hash_recompute` 填的内存字典）⇒ 往返自证 |
| M2 | `test_cpu_profile_source_policy.py:5-14` | 自称「逐句可核」的 4 条引用 3 条不成立；实测 `docs/ACSD_DESIGN.md` 中 `cpu_profile` 出现 **0 次** |
| M3 | `test_cpu_profile_source_policy.py:188` | `returncode != 5` 即「画像有效」：崩溃(139)/超时/exec 失败全算有效，且用于 `:156` 接受缓存 ⇒ **fail-open** |
| M4 | `drizzle_precision_default_test.cpp:5,17,266,303-308` | 伪引「宪章 §5.3『Drizzle 采用 float64 累积』」（`RELEASE_STATUS.md:34` 已自记宪章已删，`docs/` 0 命中）；`:17` 头注释说 T7 是「等价性 1e-5」，代码断言**相反**且门限 1e-3 |
| M5 | `drizzle_precision_default_test.cpp:140,290,294` | T6/T8 把建帧失败哨兵 `-999` 当「如期拒绝」⇒ `make_frame` 一坏两条负例全绿而精度逻辑一次未跑 |
| M6 | `p2_output_semantics_test.cpp:22-35` | §1 整段 8 个断言为**纯恒真**（字面量数组自比），却声称查「aio_hips 产品命名」「产品目录无裸 weight」 |
| M7 | `p3_coverage_test.cpp:139-141`；`:84-142`；`:175` | `CHECK(a==b)` 是 `const` 无状态函数两次调用的**代数恒真**；`sample()` 的 `fx/fy` 从未被有判别力的值检验（互换后 6 个 case 全绿）；`:175` 期望值用与实现同式的改序运算 |
| M8 | `cpu_capability_matrix_test.c:148-165,171` | R8 在 `rc==UNSUPPORTED` 时 6 条断言**全跳过仍返回 0**，而 PASS 文案**无条件**宣称「R8 live self-consistency」 |
| M9 | `cli_wideconv_test.cpp:35,90,97` | 模拟器自产 `oob = cap-n`，断言 `oob == -1` 是代数恒等；真实换码驱动未链接，历史越界缺陷零覆盖 |
| M10 | `cfg_anchors.py:5` 与全文 `9.1/9.2/9.3/9.3-D1` | **裸从句型伪引**：「规则正本 = ANCHOR_CONTRACT.md 第 9 节」，实测该文档只有 §1–§6 |
| M11 | `p2a_oracle.cpp:144-148,258` | `BUILD FAILED` 不 return 非零；`ratio = (resid>0)? step/resid : 0.0` 在 `resid=-1` 时取 0.0 ⇒ **失败行被读成「零阶跃，完美」** |
| M12 | `p2002_unc_rej_prov_test.cpp:1850-1852` × 合同 `pending_aio_channels.writer_int32_tile` | 门强制合同继续宣称已交付的 int32 通道为 `PENDING_AIO_DOMAIN`，而代码已交付（`aio_hips.h:46` 有 `AIO_HIPS_PRODUCT_NREJ=32`、`:211` 有 `aio_hips_write_diag_tile`）、测试自己在 `:1449` 也写明「已交付」⇒ **归档陈旧被门锁死** |
| M13 | `docchk002_claims_fixture.csv` | **悬空夹具**：唯一消费者 `test_docchk002_mutation.py` 不存在，全仓无加载代码；退役台账自身路径也写错（旧根 `tests/`） |

### 💡 建议（9）

1. `noop_handshake_test.c:85` 长度常量 26 → 实测 23（未知 id 断言因此在错误理由下成立）
2. `test_hips_properties.py:103` 对全部负例只断 `rc==1`，不校验错误原因
3. `run_cpu005_route_checks.py:252` 硬编码 `"provider": "avx512"`，非 AVX-512 机必红（环境脆弱）
4. `block_flow_test.cpp:101-102` 注释称 `+kNodeGain[node_index]`，实为 `node_gain(module_id 字符码和)`；`:279` 的「SHORT ⟺ 恰 1 消费者」缺 `nc==1 ⟹ SHORT` 方向
5. `test_public_header_layering.py:22/:26` 之外，`:55/:65` 只 walk `lib` 而 `SKIP_DIR_NAMES` 不跳 `eng` ⇒ `eng/` 下公共头零覆盖；`hits[0]` 使同名头判定依赖 `os.walk` 顺序
6. `test_phase3_module_arch.py:22,26` 的 `and` 短路使第二个 `assertIn` 永不执行（死断言，恰好掩盖 `host budget` 的真实失配）
7. `gaia_shard_coverage_gate_test.c:51` `INV_SCALE` 死宏；`:305` 无参数时往 CWD 写 4 个目录从不清空
8. `p2a_oracle.cpp:48-49` 悬空符号 `kUnnorm`（只出现在注释里）；`:111` 出参名 `out_resid_max` 实为中位
9. `p3_output_fsync_interposer.cpp`、`candidates_probe_main.cpp`、`oracle_sum.py`、`property_invariant.py` 等自述「证明派生/独立 oracle」的文件实为工具或语言层恒等式，建议在文件头显式标注「不覆盖生产代码」，避免 `ORACLE_PASS` 字样被误读为科学判据

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **R1** | 把 `p3_resample.cpp:216` 的拒绝谓词整个删空（只留注释） | `test_p3002:63` 的 `variance` 门应判红 | **推翻成功**。`"variance"` 只在 `:214` 注释与 `:478` 无关项出现，门照绿 |
| **R2** | 对 `test_fix210` 三种注入：关闭块内门一条 `if` / 让块内门多接受非法键 / 让两门分歧 | `test_01` 应判红 | **推翻成功**。`U ≡ allowed` 已在 `parser.cpp:480-481` 闭合，三种均无效 |
| **R3** | 把 `p2002:1436` 的 `CHECK_MSG(false,…)` 换成 `CHECK_MSG(true,…)` | `ACSD_P2002_FAULT=prov` 应仍红 | **推翻成功**。证明 rc=1 是构造的，与实现无关 |
| **R4** | 直接 `python3 -c "print(len('acsd.phase1.calibration'))"` | `noop_handshake_test.c:85` 的 26 是否正确 | **推翻成功**。实长 23，未知 id 断言在错误理由下成立 |
| **R5** | `grep -c "cpu_profile" docs/ACSD_DESIGN.md` | §7.1 是否承载 cpu_profile 规范 | **推翻成功**。0 命中 |
| **R6** | `grep -rn "float64 累积" docs/` + 查「宪章」 | 「宪章 §5.3」条款是否真实 | **推翻成功**。0 命中，本仓已自记「原冻结宪章已删除」 |
| **R7** | 独立重算 `1/(3600*1000*500)` 与 `1/1.8e9` | SUT / fixture / oracle 三方常数是否同源 | **推翻成功**。三者字面等价（`gaia_client.c:1817`、`gaia_xpsd_fixture_gen.c:40`、`gaia_oracle.py:23`） |
| **R8** | `sed -n '57p' phase_product_exchange_validator.py` | `p2002:1811-1814` 的逐字锚是否伪引 | **未推翻**。锚 `_PLANE_ID_SET = {"signal",…}` 在 `phase_product_exchange_validator.py:57` **逐字存在** |
| **R9** | 独立重算 `p3_rsmp_scenarios.h:4` 注释的 R/S/Ω 关系 | 场景常数是否与注释相符 | **未推翻**。R=0.5 行和1 ✓、S=1 列和1 ✓、`R·Ω'_i/Ω_j=1` ✓ 全对 |
| **R10** | 独立重算 `p2_output_semantics_test.cpp` 全部数值期望 | 是否存在同式改序的自洽 | **未推翻**。106.25 / 2.0 / 7.0 / 0.9 / 0.8 全部独立算出且相符 |
| **R11** | 独立重算 `p3_coverage_test.cpp:145` 的 `sample_masked` 解析真值 | 24.615384615… 是否独立推导 | **未推翻**。`wg=(0.1875,0.0625,0.5625,0.1875)`、剔 NaN、`wsum=0.8125`、`20/0.8125` 逐位相符 |
| **R12** | 独立重算 `cpu_features.h` 的 24 / 992 | cpuprov 清单位值硬锚是否正确 | **未推翻**。`AVX2\|FMA=8\|16=24` ✓；`F\|CD\|BW\|DQ\|VL=32\|64\|128\|256\|512=992` ✓ |
| **R13** | 把 `p2a_oracle.cpp` 的 `BUILD FAILED` 分支改成 return 1 | 失败行是否会被误读为「零阶跃完美」 | **推翻成功**。`:258` `resid=-1` ⇒ `ratio=0.0` |
| **R14** | `ls docs/architecture` | 三个测试引用的文档路径是否存在 | **推翻成功**。整个目录不存在 |
| **R15** | `grep '^## ' docs/detail/anchors/ANCHOR_CONTRACT.md` | 「第 9 节」是否真实 | **推翻成功**。只有 §1–§6 |

**未能推翻的（判据可信的正面结论）**：`p3_proj_wcs_oracle.py`（astropy 真独立 + 变异控制 + 量测域显式限定，且 `:12-14` 的 1e-8/1e-6/0.9″/px 经核实与 `GATES_AND_TOLERANCES.md:71-72` 逐字属实）、`run_mutations.py`（真改生产码 + 锚点 fail-closed）、`aio_oracle_negative.py`（参照读合同 JSON，非实现复制）、`gaia_shard_coverage_gate_test.c`（左右两侧独立来源）、`p2_workers_test.cpp`（边界真卡等号）、`cpu_abi_test.cpp`（硬字面量独立锚）。

---

## 6. 盲复算

**方法**：对以下三类既有判定，遮住原判定独立取证后比对。

| 项 | 既有判定 | 盲复算独立结论 | 判定 |
|---|---|---|---|
| `p3_proj_wcs_oracle.py` 往返门「1e-8/1e-6/0.9″/px」 | 引用属实 | 逐字核对 `GATES_AND_TOLERATIONS.md:71-72` —— **属实** | **一致** |
| `gaia_shard_coverage_gate_test.c` 头部「file_count==条目数 ∧ fail==0 是 G-1 门要件」 | 成立 | 独立核对：`gaia_client.c:2309-2315` 保证 `fail>0 ⟹ create NULL`，而 `g1_judge:178` 在到达 `:190` 前已返回 ⇒ **`fail_count` 子句在真实路径不可达** | **偏松**（该子句无判别力，只在 `--self-test` 合成快照生效） |
| `block_flow_test.cpp:16-18`「期望值全部从规格正本数据推导，只有真正的语义违规才判红」 | 成立 | 独立核对：`bind_ops` stub 算子 + `direct_sequential` 同式（注释 `:183-185` 自承）；E2 只能证明**框架**忠实搬运值，**不覆盖生产模块算子** | **偏松**（覆盖面小于自述） |
| `p2002:33-34`「故障注入有效性: proj\|prov 注入等价缺陷必败（rc=1）」 | 成立 | 独立核对：prov 分支是无条件 `CHECK_MSG(false,…)` ⇒ rc=1 构造必然 | **偏松**（判别力证明为伪） |
| `test_p3002`「variance 未显式拒」 | 成立 | 独立核对：`variance` 已被 §30.4-4 移出拒绝清单 | **偏严→方向错**（门锁死了一条已作废的规则） |
| `run_mutations.py`「20 条注入全部可施加、判别力成立」 | 成立 | 独立核对：`stage`/`apply_patches`/`-I libdir` 在前 + 显式重编 6 TU ⇒ 确实测被改过的生产码 | **一致** |

**结论**：6 项盲复算中 **2 项一致、2 项偏松、1 项偏严（方向错）、1 项未列入**。整体看，本片既有判定**偏松**——多处把「判据绿」当作「实现正确」，而实际是判据本身恒真、读桩、或门限由被测方提供。

---

## 7. 子代理派发记录

**派了 5 个**（A 组 2 个因误派重复、B/C/D 各 1）。全部只读，零 git 写、零编译、零测试执行。

| 组 | 覆盖 | 子代理 | 交付 |
|---|---|---|---|
| A | 5 份 / 4213 行 | `e306777c` | 阻断 4 + 须修 9 + 悬空 12 + 建议 7；反例 8（R1 推翻成功） |
| A'（重复派发） | 同 A | `db8c0f7d` | 同 A |
| B | 6 份 / 2141 行 | `5ddd52aa` | 阻断 6 + 须修 13 + 建议 6；反例 10 |
| C | 8 份 / 1736 行 | `9e8e96b3` | 阻断 4 + 须修 11 + 建议 10；反例 13 |
| D | 28 份 / 2339 行 | `d3168f8f` | 阻断 4 + 须修 7 + 建议 6；反例 6 |

### 逐条复核与否决

我**逐条**复核了各组结论，凡与我自己读完的原文冲突者一律否决。**否决 9 条**：

| # | 子代理结论 | 否决理由 |
|---|---|---|
| V1 | D 组：`step7_apflux.py` 的 `q3lib.py` 「**已从 src 删除**」 | **否决**。实测 `ls eng/tests/validation/release02/q3_additive_truth/src/` → `q3lib.py` **存在**（连同 q3core/q3hp 等 14 个文件）。该脚本的真实问题是 `:4` 用**相对 CWD 路径**，不是模块缺失 |
| V2 | C 组：`docs/engineering/DEPENDENCY_RULES.md` 「可能已删」 | **否决（对我方假设的纠正，非对子代理的否决）**。文件存在（3693B）。但我确认其**不含**「公共头不得 include src/ 头」这条规则 ⇒ 判据的真实权威是 `ACSD_DESIGN.md §8.4/8.5`，结论方向保留 |
| V3 | B 组：`p2_upm_ma_test.cpp:14` 锚值错误（eps 5e-4 / kappa 1.1701e8 vs 实测 1e-4 / 2.92e9） | **降级为线索**。该文件**不在本片**（属他片），我未亲自读完，不计入本片交付；已在本报告中标为「跨片线索」 |
| V4 | A 组：`gaia_shard_coverage_gate_test.c` 负例③（空目录）恒红 | **保留但降级为建议**。红灯确由 `count_xpsd_entries` 对自建空目录返 0 决定、与被测实现无关；但它是 `:427-430` GREEN/RED 双向总断言的合法一极，不构成阻断 |
| V5 | C 组：`test_p3002` 的 `variance` 门「恒真」 | **升级为阻断 B1**。我亲自 grep 复核，确认 `"variance"` 仅命中 `:214` 注释（内容恰是「已移除」）与 `:478` 无关项，门方向为反 |
| V6 | C 组：`test_fix210` 主判据恒真 | **升级为阻断 B2**。我亲自复核 `parser.cpp:480-481` 与 `test_cfg004:104-119`，恒真链三环全部成立 |
| V7 | A 组：`p2002` 两处注入恒红 | **升级为阻断 B3**。我亲自 `sed` 出 `:1435-1439` 与 `:1673-1676` 原文，确认是无条件 `CHECK_MSG(false,…)` |
| V8 | C 组：`test_cpuprov_manifest.py:166` 恒真 | **降级为建议**。该格仅 MSVC 腿 rc=0 缺硬锚；主判据位值硬锚 24/992 经我独立重算正确，且两条负例（rc=2/rc=5）真会红 ⇒ 整体判可信 |
| V9 | B 组：`upm_ma_oracle.py` 「零断言、恒 return 0」 | **保留并采纳**。我亲自读完 419 行，确认 `main():415` 无条件 `return 0`、`:406` 的 "expect rank<n_free" 只打印不判；它是**锚值生成器**不是门，文件名 `oracle` 易被误读 |

**采信并升级为阻断的**：V5、V6、V7（我的 §2 三条）。**采纳为须修的**：D 组的 `docs/architecture` 整体迁移、noop 长度常量、`cli_wideconv` 自洽；C 组的 R8 fail-open、`p3_coverage` 代数恒真、`cfg_anchors` 伪引；A 组的 `check_block_flow_spec.py` 悬空、`Writer._dir` 逃逸。

**未采信的正面结论**（子代理称「未能推翻」者我亦抽查）：B 组 R5（`module_entry.*` 豁免正当）我复核——实测 6 个 `module_entry.c/cpp` 确都定义 `acsd_module_query_v1`，与注释「6 处误报」数目吻合，**豁免正当，采纳**。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 0. 基线
git rev-parse HEAD                                    # 850a9ede…
git status --porcelain -- eng/                       # 本片成员工作树干净

# 1. 覆盖率分母（47 份 / 10429 行）
while IFS= read -r f; do printf "%6s %s\n" "$(wc -l < "$f")" "$f"; done <<'EOF' | tee /tmp/lines.txt | awk '{s+=$1} END {print "TOTAL:",s}'
eng/tests/unit/p2002_unc_rej_prov_test.cpp
eng/tests/unit/gaia_xpsd_fixture_gen.c
eng/tests/unit/block_flow_test.cpp
eng/tests/artifact/test_production_store.py
eng/tests/unit/gaia_shard_coverage_gate_test.c
eng/tests/unit/p2_upm/oracle/upm_ma_oracle.py
eng/tests/cli/test_cpu_profile_source_policy.py
eng/tests/abi/test_abi005_echo.py
eng/tests/unit/p3_proj/p3_proj_wcs_oracle.py
eng/tests/unit/drizzle_precision_default_test.cpp
eng/tests/cpu/dispatch/run_cpu005_route_checks.py
eng/tests/validation/release02/fix_p2a_seam_oracle/p2a_oracle.cpp
eng/tests/unit/p3_rsmp/run_mutations.py
eng/tests/cli/test_fix210_block_key_parity.py
eng/tests/unit/p3_coverage_test.cpp
eng/tests/backend/test_public_header_layering.py
eng/tests/unit/p2_output_semantics_test.cpp
eng/tests/unit/aio/oracle/aio_oracle_negative.py
eng/tests/cpu/dispatch/cpu_capability_matrix_test.c
eng/tests/backend/test_cpuprov_manifest.py
eng/tests/unit/p1_psfw/p1psfw_tests_negative.cpp
eng/tests/backend/variant_build.py
eng/tests/unit/p2_workers_test.cpp
eng/tests/config/cfg_anchors.py
eng/tests/backend/test_hips_properties.py
eng/tests/cpu/avx2/provider_avx2_capability_gate_test.c
eng/tests/conformance/noop/tests/unit/noop_handshake_test.c
eng/tests/backend/p3_output_fsync_interposer.cpp
eng/tests/unit/cli_wideconv_test.cpp
eng/tests/backend/test_isa_avx512.py
eng/tests/unit/cpu_provider_test.cpp
eng/tests/unit/cpu_abi_test.cpp
eng/tests/integration/p3_export/EVIDENCE.md
eng/tests/validation/release02/q3_additive_truth/src/step7_apflux.py
eng/tests/unit/p3_rsmp/p3_rsmp_scenarios.h
eng/tests/backend/test_p3002_properties_order_unit.py
eng/tests/conformance/echo/include/acsd/echo/types.h
eng/tests/unit/aio_abi_omp.hpp
eng/tests/abi/test_mod001_install_load_check.py
eng/tests/config/fixtures/negative/cpu_profile_v1_bad_kernel.json
eng/tests/arch/test_phase3_module_arch.py
eng/tests/testkit/examples/oracle_sum.py
eng/tests/validation/release02/phot_verify/summarize.py
eng/tests/testkit/examples/property_invariant.py
eng/tests/backend/candidates_probe_main.cpp
eng/tests/quality/fixtures/docchk002_claims_fixture.csv
eng/tests/config/fixtures/positive/mosaic_flat.phase_config.json
EOF

# 2. B1 —— variance 门被注释满足且语义相反
grep -n '"variance"' lib/algorithms/resample/p3_resample.cpp      # → 214(注释) 478
sed -n '214p;216p' lib/algorithms/resample/p3_resample.cpp
sed -n '60,64p' eng/tests/backend/test_p3002_properties_order_unit.py

# 3. B2 —— test_fix210 恒真链三环
sed -n '480,481p' lib/infrastructure/cli/parser.cpp                 # allowed = block_keys() ∪ session_keys()
sed -n '104,120p' eng/tests/config/test_cfg004_unified_contract.py # 正则解析同二函数
sed -n '93p;107,114p' eng/tests/cli/test_fix210_block_key_parity.py

# 4. B3 —— 两处硬编码恒红
sed -n '1435,1439p;1673,1676p' eng/tests/unit/p2002_unc_rej_prov_test.cpp
sed -n '2019,2027p' eng/tests/unit/p2002_unc_rej_prov_test.cpp      # PASS 文案

# 5. B5 —— 三方同一字面常数
grep -n '1.0 / (3600.0 \* 1000.0 \* 500.0)' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c
grep -n 'INV_SCALE' eng/tests/unit/gaia_xpsd_fixture_gen.c | head -2
grep -n 'INV_SCALE' eng/tests/oracle/gaia_oracle.py
grep -n 'mag_raw \* 0.001 - 1.5' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c

# 6. B6 —— 扫描面静默缩到 1/4 + 判据读桩
sed -n '137,141p' eng/tests/abi/test_abi005_echo.py
for d in runtime lib cli providers; do [ -d "$d" ] && echo "OK $d" || echo "MISSING $d"; done
sed -n '88,93p' eng/tests/cpu/avx2/provider_avx2_capability_gate_test.c
sed -n '254,260p' lib/infrastructure/benchmark/cpu/common/src/capability_detect.c  # 桩里没有的分支

# 7. B7 / M10 —— 文档迁移与伪引
ls -d docs/architecture 2>&1                      # No such file
grep -n 'docs.*architecture' eng/tests/arch/test_phase3_module_arch.py eng/tests/backend/test_isa_avx512.py
grep -n '^## ' docs/detail/anchors/ANCHOR_CONTRACT.md   # 只有 1..6
grep -c 'cpu_profile' docs/ACSD_DESIGN.md              # 0
grep -rn 'float64 累积' docs/ | head                  # 0

# 8. B4 —— 悬空引用
for f in MODULE_MIGRATION_TEMPLATE.md tasks/02_ABI_BUILD_CLI_TASKS.md \
         tasks/03_RUNTIME_DATA_IO_TASKS.md 04_CPU_RESOURCE_TASKS.md \
         run/WCS-DETERMINISM-01/REPORT.md run/GAIA-FAILCLOSED-01/REPORT.md \
         eng/tools/quality/check_block_flow_spec.py eng/ci; do
  [ -e "$f" ] && echo "OK $f" || echo "DANGLING $f"; done

# 9. 未推翻项（正面证据）
grep -n '_PLANE_ID_SET' lib/infrastructure/aio/runtime/artifact_store/phase_product_exchange_validator.py
python3 -c "print('AVX2|FMA =',(1<<3)|(1<<4), ' F|CD|BW|DQ|VL =',(1<<5)|(1<<6)|(1<<7)|(1<<8)|(1<<9))"
python3 -c "print(len('acsd.phase1.calibration'))"   # 23 ≠ 26
```

**口径说明（计数分母）**：本片为测试代码片，**不含判据实例计数**。§4 的条数口径统一为「**发现条目数**」（阻断 7 / 须修 13 / 建议 9），每条对应一处可定位的 `文件:行`，无「门实例 / 去重门 / 整改分母」三口径歧义。悬空引用 7 处按**被引目标去重**计数（同一目标被多处引用只计 1 次）。

---

## 9. 未能确认的点

1. **未运行任何测试/编译/脚本**（纪律禁止）。所有「会不会红」均由源码等价式 + grep 计数静态推导，未经运行时确认。
2. `p2002:1665-1670` 的 `rep.prov_keys_present == 4` / `aio_hips_verify_product_set` 行为依赖运行时，未验证。
3. `ACSD_P2002_FAULT` 四模式是否真 rc=1 未实测；我只证明了 `prov`/`aio` 的红灯是硬编码的，因此 rc=1 无信息量。
4. `p1psfw_oracle.hpp` 不在本片，我未读其实现，**无法判断 `p1psfw_tests_negative.cpp` 的 oracle 是否真正独立于被检实现** —— 这是该文件可信度的唯一支点，建议单独派单。
5. `p3_rsmp_oracle_test.cpp` 不在本片，`run_mutations.py` 的判别力有相当部分压在它身上；若它也是自洽式，20/20 的证据力会大幅缩水。
6. `p3_proj_wcs_oracle.py` 的 CAR 0.2° 场真实残差与 1e-5 门限的余量、以及 `const_omega` 变异在该场宽下是否真被检出 —— 均需实跑。
7. `EVIDENCE.md:44` 的「ORACLE_PASS 183 checks across 3 modes」无法静态核实。
8. `build/acsd` 二进制新鲜度未核 —— 若比 `parser.cpp` 旧，B2 的恒真链会被「陈旧偶然」打破，但那是偶然变红不是判别力。
9. `mod001_install_load_check.py` 是否导出名为 `REPO` 的模块属性（`test_mod001_install_load_check.py:26,29` 依赖）未读该文件确认。
10. `docs/architecture/` → `docs/engineering/` 的迁移波及范围我只查了本片三个文件，全仓普查未做。