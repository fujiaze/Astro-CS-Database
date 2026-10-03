# 审稿-P1 · ENG-tests-005（第 1 遍，对抗审稿）

- 仓库：`/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- 权威依据原件：`run/GOVERN-08/工作包-GOVERN-08原件/`
- 口径：负责人裁定 —— 判据代码**不构成**正确性证据；判据绿**不等于**实现对。本报告全部结论由**逐字读原文 + 独立重算 + 构造反例**得出，未编译、未跑 ctest/pytest/未跑任何实验脚本、零 git 写、零仓内文件改动。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 成员份数（权威清单） | **47** |
| 实际读完份数（**我本人逐字读完**） | **47 / 47（100%）** |
| 成员总行数（清单声明） | **10431**（实测 `wc -l` 合计 10432，差 1 = 末文件无换行） |
| 我实际逐行读过的行数 | **10432（100%）** |
| 覆盖率 | **份数 47/47 = 100%；行数 10432/10431 = 100%** |

**未读完的：无。** 本片 47 份成员文件全部由我本人用 read 工具逐行读完（含 `p2001_real_nodes_test.cpp` 1477 行分两次读完、`p1wcs_tests_negative.cpp` 与 `drizzle_adapter_impl.cpp` 等长文件）。子代理只做**旁证**，其结论一律经我回原文复核后才采纳（见第 7 节）。

> 方法学警示（影响后续所有片）：工作树内 `run/` 下有 **11 份陈旧源码快照**（`run/FINAL-07-e2e/bisect/*`、`run/P1-CONCURRENCY-CALIB-01/wsrc/*` 等）。任何"全仓 grep 找消费者"都会命中幽灵树。本片所有悬空引用结论均在**排除 `run/` 的活树**上重验过。

---

## 2. 本片判定

### **阻断（BLOCKING）**

判据失效面覆盖：Phase2 七节点数值 oracle（P3 前身 P1/P2 创新点）、Phase3 重采样（P3 创新点本体）、资源门（`evaluate_gate`）、P3 冻结审计门、provenance 溯源门、磁盘门、telemetry 契约门。其中 **4 个文件在本仓 HEAD 上处于「永久红」或「永久空转」**。

**最重的 3 条：**

1. **`eng/tests/unit/p2001_real_nodes_test.cpp:613-618` —— 伪引：整段 AUDIT-PERSIST 断言所依据的条款，其被引内容在原文里不存在。**
   注释声称依据 `docs/science/PHASE2_UPM.md §7a:190-192`（节点间距写入 provenance）、`§7a:204`（κ 与 χ²_red 可观测）、`§7a:219`（provenance 最小集含 rank/rank_rtol/kappa/kappa_max/model_hash），并称 `lib/infrastructure/cli/commands.cpp:242-296` 是「CLI 只从节点 manifest 抽 6 个键」。
   实测：`§7a` 实际起于 **line 200**（`grep -n "^## 7a"` → `200:## 7a 表示能力边界与近奇异处置`）；`awk NR==190` = 「实测 ΔC_max = 2.22e-15」；`NR==204` = 「参考面 `B_ref`（8×8 control cell 双线性）只能表示尺度 ≳ 2× **节点间距**的分量」；`NR==219` = 「**工程要求（规则 1）**：节点间距须 ≤ 目标可表示尺度的 1/2」。**三条全部对不上**，且 `chi2_red` 在整份 `PHASE2_UPM.md` 中出现 **0 次**（`grep -c` → 0）。`commands.cpp:240-250` 实为 `cli_backend_id_for_provider`；`grep -n "sky_plane\|upm_model\|node_spacing\|kappa" commands.cpp` → **零命中**。
   危害：`p2001:621-717` 整段 `phase2_audit` 断言（含 `:694` 禁 `kappa_max_frozen`、`:697` 禁 `rank_rtol_frozen`）全部挂在这条不存在的依据上。注释自承的冲突已经出现：`§7a` 后文说的是**反面**（`kappa_max` 类常数**不属阈值面**），而测试引它当「provenance 最小集含 kappa_max」。
   **这是「主项常完全没有引号、是裸从句摆在冒号后」的最强样本** —— 成对引号扫描会完全漏掉。

2. **`eng/tests/integration/p1_integrate/oracle/p1_reopen_oracle.py:266-273` —— 恒真门（代数恒等式型），且其 docstring 声称的「独立 Oracle 逐位比对」在代码里根本不存在。**
   ```python
   wt, wnorm, med = group_weights_from_components(comps)          # :266
   srt = sorted(wnorm); mmed = srt[0] if len(srt)==1 else 0.5*(srt[0]+srt[-1])  # :267-268
   if not (len(wnorm)==len(products) and all(v>0.0 for v in wnorm)
           and abs(mmed-1.0) <= 1e-9):                            # :270-271
   ```
   **我独立重算**：`products` 恒为 2 项（`:166-167`）⇒ `comps` 2 项 ⇒ `:102` `med = 0.5*(wt0+wt1)` ⇒ `:105` `wnorm=[wt0/med, wt1/med]` ⇒ `wt0+wt1 = 2·med` ⇒ `mmed = 0.5·((wt0+wt1)/med) = 0.5·2 = 1.0`，**恒等于 1.0，与 wt 取值无关**。第三个合取项 `len(wnorm)==len(products)` 由构造即真；第二个 `all(v>0.0)` 因 `:95-98` 的 `max(...,floor=1e-12)` 与 `:103` 的 `med>0` 守卫而恒真。**三个合取项：两个恒真、一个代数恒等。** 注释自称「非空洞守卫」，它什么都没守。
   更严重：docstring `:11-12` 声称「从磁盘重开的两帧四分量独立复算组内归一权重，**与 C++ 输出逐位比对**（独立 Oracle，非同实现自证）」—— `wt` 与 `med` 在 `:266` 绑定后**再未被使用**，`wnorm` 只进上面那个恒真门。**全文件没有任何一处把 oracle 结果与 C++ 输出比较。**
   **反例**：把生产 `compute_psfsw_weights` 的指数 α 从 2 改成 3，该 oracle 照旧打印 `ORACLE_PASS`。

3. **`eng/tests/unit/p1_resource_test.cpp:78-90` —— 把文件自己认定「错误」的判决冻结为期望值，构成缺陷保护锁。**
   ```cpp
   // :79-80  GateConfig::avg_equivalent_cores 默认 0.0，低于冻结阈值 1.7 ⇒
   //         evaluate_gate 正确返回 LowAvgCores
   CHECK(evaluate_gate(g) == acsd::GateDiag::SingleThreaded);      // :86
   g.avg_equivalent_cores = 1.9;  // 调用方注入实测均值
   CHECK(evaluate_gate(g) == acsd::GateDiag::SingleThreaded);      // :90
   ```
   注释第 79-80 行断言的**正确答案是 `LowAvgCores`**，第 86 行却断言 `SingleThreaded` —— **同一文件内相隔 6 行的自相矛盾**。第 90 行注入 1.9（≥ 阈值 1.7，按注释所述应当不再是 `SingleThreaded`）后仍断言 `SingleThreaded`。两条合起来证明的是：**判决对 `avg_equivalent_cores` 完全不变**。
   危害是双向的：现在它绿灯护着一个真实缺陷（2 核 heavy 任务被判 SingleThreaded，生产走错降级分支）；**修好 `evaluate_gate` 的回退分支，这两条会先红并阻挡修复** —— 即「缺陷保护锁」。这正是负责人指出的「恒绿/恒红都被伪装成合规」的反面形态。

---

## 3. 逐文件清单（47/47）

格式：`文件` → 读了什么 → 看到什么 → 判定（带 `文件:行`）

| # | 文件 | 行数 | 读了什么 / 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `unit/p2001_real_nodes_test.cpp` | 1477 | Phase2 七节点 operation/entry 冻结表、typed artifact、call_count=1、§30.1 ivar 数值 oracle、UPM 校正锚、determinism、1/4-worker parity | **须修**（最重文件，见 B1/B9/M3/M4） |
| 2 | `unit/p2_samp/p2_samp_test.cpp` | 811 | coverage/support 分类、权重 token 门、spatial/summary/scalar_gate、`ref_bilinear` 独立 oracle | **须修**（含恒真门与悬空 oracle 引用） |
| 3 | `artifact/test_provenance.py` | 680 | revision 类别、digest 确定性、隐私扫描、store 集成 | **须修**（恒真门 ×2 + 逆函数自证） |
| 4 | `cli/test_fix208_disk_gate.py` | 568 | errno→kind 分类、precheck warn、帧级归因探针、tmpfs E2E | **阻断**（恒真门 + 3 处伪引/悬空） |
| 5 | `unit/p3002_real_nodes_test.cpp` | 474 | Phase3 五节点 operation/entry、产物存在、确定性 | **阻断**（重采样零数值验证 + 伪引） |
| 6 | `unit/normalize_workflow_test.cpp` | 434 | A 确定性 / B 归约序 / C 预取 / D 生命周期 / E 内存上限 / F 取消 / G 探针 / H 墙钟 / I RSS / J I/O 失败 / K JSONL | **须修**（I2 断言方向反了；单位判据缺；归档未跟踪） |
| 7 | `abi/test_module_registry.py` | 416 | manifest/yaml/DLL 三方比对、DUP/HASH/VERSION finding | **建议**（mask 用子串冒充位测试；`os.chdir` 污染） |
| 8 | `unit/drizzle_adapter_impl.cpp` | 386 | host stub / base64 / fixture / direct-vs-plugin BITWISE | **须修**（coverage 平面从未被验） |
| 9 | `unit/p1wcs/p1wcs_tests_negative.cpp` | 364 | F4 失败语义负例、WcsTan 单位/桥接锚、extract_wcs_sip 锚 | **建议**（两处缺陷锚 + 语义弱） |
| 10 | `integration/p1_integrate/oracle/p1_reopen_oracle.py` | 331 | schema 校验、FITS SHA-256 重算、BUNIT 层序、组权重、变异自检 | **阻断**（恒真门 + 伪声称） |
| 11 | `backend/test_calibration_oracle.py` | 303 | 第一性原理 Python 重算 vs C++ 校准库 | **通过**（本片最干净的 oracle；仅两处采样削弱，见 M12） |
| 12 | `cpu/avx2/provider_avx2_oracle_main.cpp` | 280 | dlopen 双 provider、det/ACQ/REL_MAX/ULP | **建议**（rel_diff 分母下限使相对差退化） |
| 13 | `cli/test_multiblock_normalize.py` | 259 | 多块解析/派发/互斥/未知键/逐帧退役/独立 manifest | **须修**（正例不断言 rc==0；悬空裁决引用） |
| 14 | `backend/p3_publish_order_negative.py` | 248 | P-205 发布序 / P-206 DATASUM 的逆向变异 | **须修**（判红不可归因） |
| 15 | `unit/rt006_scheduler_test.cpp` | 225 | diamond 并发、失败传播、取消、预算、内存回压、确定性、Runtime 全链 | **阻断**（取消门恒真；并发门阈值失效；全链丢弃 run 结果） |
| 16 | `cli/test_monitor_events.py` | 209 | resource summary 必采指标、backend 事件、曲线 CSV 载体 | **建议**（退役锚被 `isfile` 吞；skip 掩盖） |
| 17 | `cpu/baseline/provider_handshake_test.c` | 200 | query 握手负例、kernel_list 12 项字面表、self_test | **通过**（字面表是真实外部参照） |
| 18 | `config/test_cfg001_negative.py` | 192 | 四门负例 + run_manifest/cpu_profile 回归 + 负例清单登记 | **须修**（校验器是测试树迷你实现） |
| 19 | `unit/drizzle_adapter_test.cpp` | 180 | 入口 ABI、导出面净化、describe/validate、cancel 幂等 | **通过**（少数几个真判别文件之一） |
| 20 | `backend/test_abi_kernels.py` | 170 | 12 op 的 Python 参考、budget1-vs4 确定性 | **须修**（参考是 SUT 的逐语句转写；零 op 数门） |
| 21 | `unit/p1_drz/p1_drz_oracle.hpp` | 166 | drizzle 解析恒等式 long double 独立转写 | **通过**（真独立 oracle；仅 `rel_close` 零值直通） |
| 22 | `unit/cli_process_test.cpp` | 158 | exit code、空格路径、timeout、spawn 失败、元字符注入 | **建议**（devnull_stdio 未验） |
| 23 | `backend/test_p2002_parallel_upm.py` | 152 | 无 OpenMP 残留、4-worker UPM 成功、persist 一致 | **须修**（自比自证；从不验 worker 真被采纳） |
| 24 | `cli/test_cli_v7_surface.py` | 147 | help 逐行 golden、26 条旧命令 rc=2、`--version` 单源 | **通过**（本片判别力最实的文件） |
| 25 | `unit/cpu001_provider_selftest.cpp` | 139 | backend_id、feature 位、kernel 表、alloc 平衡 | **建议**（注释承诺 12 项一致，断言只有 `>0`） |
| 26 | `validation/release02/q2_snr_smoothness/realdata/stageD_samples.py` | 132 | p2_samples.json 字段统计 | **建议**（统计不断言；数据缺失；硬编码绝对路径） |
| 27 | `cli/test_memory_growth.py` | 123 | 6 类诊断向量 + 曲线摘要 | **阻断**（test_07 恒真门） |
| 28 | `unit/mon001_recorder_test.cpp` | 116 | 采样/阶段分段/分位数/三产物 | **建议**（worker_balance 空数据行也绿；p50 口径非标准） |
| 29 | `testkit/registry.json` | 110 | 6 条 TST-001 示例登记 | **建议**（执行者已被删；`MISSING` 是合法期望来源） |
| 30 | `unit/p1_resource_test.cpp` | 105 | 校准数值、资源采样、门判决、RSS 斜率 | **阻断**（判决冻结 + 只验 16/262144 像素） |
| 31 | `system/runtime/CMakeLists.txt` | 99 | 11 个 ctest 条目与目标 | **通过**（源文件全部存在） |
| 32 | `unit/core_artifact_test.cpp` | 93 | validate / roundtrip / science_hash / 单位负例 | **建议**（roundtrip 漏 provenance 全族） |
| 33 | `unit/p1_nside_test.cpp` | 90 | NSIDE 派生、解析分辨率、RA wrap roundtrip | **阻断**（roundtrip 自证无绝对锚） |
| 34 | `backend/p3_output_probe_main.cpp` | 83 | write/verify 探针（无断言，纯转发） | **通过** |
| 35 | `unit/p2_ir_facade_test.cpp` | 78 | canonical IR 节点、facade 委托、阶段序、trace | **阻断**（全文只读源码文本 grep） |
| 36 | `backend/test_p3001_science_freeze.py` | 70 | 极区条件、冻结项、session 拒绝、FROZEN 状态 | **阻断**（1 项 ERROR + 1 项恒红） |
| 37 | `backend/cheat_backend.cpp` | 64 | 故意错误 backend test double | **通过**（设计正确，无法泄漏进生产接线） |
| 38 | `conformance/echo/CMakeLists.txt` | 50 | acsd_echo SHARED + 1 个 ctest | **通过**（3 个源路径全部存在） |
| 39 | `config/fixtures/negative/cpu_profile_v2_bad_os_abi.json` | 48 | v2 profile，`os_abi: "freebsd"` 越出冻结枚举 | **须修**（只喂测试树校验器） |
| 40 | `validation/release02/c_delta_composition/convergence_trend.py` | 46 | Huber IRLS 重实现 + 收敛趋势打印 | **阻断**（恒真条件 + 无断言 + 自证 + 数据缺失） |
| 41 | `testkit/examples/negative_bad_input.py` | 36 | 3 行玩具 `parse_positive_int` 负例 | **须修**（自证 + 吞异常 + 无正向对照） |
| 42 | `testkit/schemas/test_metadata.schema.json` | 34 | 元数据机器合同 | **建议**（描述与 pattern 自相矛盾；`MISSING` 合规化） |
| 43 | `unit/p1_psfw/tests_main.cpp` | 25 | 8 组注册执行器 | **通过**（不是空套件，fail-closed） |
| 44 | `config/fixtures/negative/precision_out_of_domain.phase_config.json` | 21 | `precision_mode: 2` 越界 | **须修**（同上；生产 `parser.cpp` 无该校验） |
| 45 | `config/fixtures/negative/normalize_mixed_forms.phase_config.json` | 20 | blocks 与平铺键同时出现 | **通过**（生产 `parser.cpp:487-489` 确有互斥门） |
| 46 | `unit/p1_noise/adapter_entry_impl.cpp` | 10 | 同 TU include 生产 `module_entry.cpp` 转出 `g_noise_api` | **阻断**（孤儿，从未编译） |
| 47 | `validation/release02/phot_verify/debug3.py` | 10 | 调试草稿（分位数打印） | **建议**（调试脚本遗留测试树；数据缺失） |

---

## 4. 发现清单

### 4.1 阻断（10 条）

| ID | 位置 | 缺陷族 | 事实 |
|---|---|---|---|
| **B1** | `unit/p2001_real_nodes_test.cpp:613-618` | **伪引** | `PHASE2_UPM.md §7a` 实起 line 200；190/204/219 三处内容全不符；`chi2_red` 全文 0 命中；`commands.cpp:242-296` 无任何 p2 键 |
| **B2** | `integration/p1_integrate/oracle/p1_reopen_oracle.py:266-273` + `:11-12` | **恒真门（代数恒等式）+ 伪声称** | `mmed ≡ 1.0` 恒真；docstring 声称的「与 C++ 输出逐位比对」不存在 |
| **B3** | `unit/p1_resource_test.cpp:78-90` | **恒绿护缺陷 + 自相矛盾** | 注释说正确答案 `LowAvgCores`，断言 `SingleThreaded`；判决对实测均值不变 |
| **B4** | `cli/test_fix208_disk_gate.py:189-195` | **恒真门** | 断言 `resource_gate.h` 里 `gate_enforcement(bool /*未命名*/, GateDiag /*未命名*/){return RecordOnly;}` —— 无分支常量函数。docstring `:512-515` 声称它是「派生值，比硬编码字面量更强」= 假陈述 |
| **B5** | `backend/test_p3001_science_freeze.py:66` | **恒红门** | `assertIn("状态: FROZEN", head)`；`grep -c FROZEN docs/science/PHASE3_HIPS_TO_FITS.md` = **0**。该门永久红，把真缺陷藏在红灯里 |
| **B6** | `backend/test_p3001_science_freeze.py:19` | **悬空引用（每次运行 ERROR）** | `docs/api/PHASE3_API_V1.md` 不存在（实为 `docs/engineering/PHASE3_API_V1.md`）；`:30` `open(p)` 必抛 `FileNotFoundError` |
| **B7** | `artifact/test_provenance.py:369-375` | **恒真门（无操作重复）** | `kw2["science_ids"] = kw2.get("science_ids")` 是空操作（注释自陈 `# no-op keep shape`）；且 `provenance_digest_hex` **根本没有 history 形参** ⇒ "history 不参与 digest" 不可证伪 |
| **B8** | `unit/p1_nside_test.cpp:69-77` | **往返自证（恒真门 c 型）** | `ang2pix→pix2ang→ang2pix`，无任何绝对锚；`pixel_resolution` 有闭式锚但两个映射函数没有 |
| **B9** | `unit/p2_ir_facade_test.cpp:16-70` | **判据读桩（源码文本 grep）** | 不 include 任何生产头、不调任何生产函数，只 `ifstream` 读 `p2_session.cpp` 文本做 `find()`；`:57-62` 的"顺序"是**首次出现下标**，与运行顺序无关。CMake 只链 `acsd_contracts`，被测 `p2_session.cpp` 从未编入该可执行 |
| **B10** | `unit/p1_noise/adapter_entry_impl.cpp` | **孤儿（从未编译）** | `CMakeLists.txt:383` 的 `add_subdirectory(lib/algorithms/noise_snr)` 处于**注释态**；`eng/tests/unit/CMakeLists.txt:1328` 的 `if(TARGET p1noise_under_test AND TARGET acsd_p1_noise)` 两个 target 都不存在 ⇒ `p1_noise_adapter_test` 永不构建。`:1313-1322` 用 10 行注释宣称的覆盖面对任何构建都不存在 |

**B1 的一个附注（对本片最重的判据失效面）**：`p2001:621-717` 的 `phase2_audit` 断言块完全依赖 B1 那条不存在的条款；其中 `:628` `pa.value("clause","").find("§7a") != npos` 更是对生产写死的字面串做子串自证，叠加 B1 后该「引用」是空的。

### 4.2 须修（16 条）

| ID | 位置 | 缺陷 |
|---|---|---|
| **M1** | `unit/p2001_real_nodes_test.cpp:497` | **fail-open**：`if (!intj.is_object()) { remove_all; return; }` —— 合法 JSON 的非对象（`[]`/`7`/`null`）解析成功、撞上此 guard、**静默跳过 §30.1 全部逐像素 oracle（`:501-588`）**且 failures 不变 |
| **M2** | `unit/p2001_real_nodes_test.cpp:530,551` | **fail-open**：`sm_off` 默认 `~0ull`，`:551` 的 `if (smask.size() >= sm_off + 2*tile_span && …)` **无 else**、无"确实跑过"计数；把 `sample_mask_offset` 改名即整段蒸发。（注：生产 `module_adapters.cpp:12376` 显式加了 `if (sm_off == ~0ull …) return fail-closed` —— 测试抄了哨兵没抄守卫） |
| **M3** | `unit/p2001_real_nodes_test.cpp:458,466` | **死占位符 + 无出处魔数**：`before += std::fabs(b2[i]-10.0f-b1[i]) > 0 ? 0.0 : 0.0;` 两分支都是 0.0，`:469` `(void)before;`。注释 `:448` 声称「校正后差 ≪ **原始差 10**」，但"原始差"从未被测量。唯一判据是 `mean_after < 5.0`（5.0 全仓无推导） |
| **M4** | `unit/p2001_real_nodes_test.cpp:1195,1261-1265` | **伪造的故障注入证据**：`:1265` 声称「`ACSD_IVAR_FAULT=silent_fallback` proves this gate is live」。实测 `ACSD_IVAR_FAULT` 在生产中只被读一处（`astro_sphere_sink.cpp:351`，只接受 `no_variance_flags`，位于 Phase1 drizzle sink）；`silent_fallback` 无任何生产响应。设置它只是让测试自己执行 `CHECK_MSG(false, …)` |
| **M5** | `unit/p3002_real_nodes_test.cpp:77-80,314-315,450-457` | **P3 创新点零数值验证**：输入 HiPS 是 512×512 **全常量 100.0**；对 `p3_resampled.bin` 只断言 `!content.empty()` 与两次运行字节相同，**一个像素值都没读**。把中心 RA 翻 180°、`east_left` 取反、像素比例错 100× 仍全绿 |
| **M6** | `unit/p3002_real_nodes_test.cpp:321` | **判据读桩**：`CHECK(ver.value("reopen_ok", 0) == 1)` —— 只断言 verify 节点「说自己做了」；写 `reopen_ok:1` 而根本不打开文件的 verify 节点照样绿 |
| **M7** | `unit/p3002_real_nodes_test.cpp:3-6` | **伪引**：「宪章 §7.2/§8.2」两条带引号的句子无法核实（宪章已删）；同行的 `RUN_GRAPH_CONTRACT §5/§6` 为真 —— 半行有效半行作废 |
| **M8** | `unit/rt006_scheduler_test.cpp:46` | **并发门阈值失效**：`CHECK(parallel_peak.load() >= 1);` 注释写「并行出现（b/c 并发）」，但严格串行的调度器也满足 `>=1`。应 `>=2` |
| **M9** | `unit/rt006_scheduler_test.cpp:198-208` | **全链路 fail-open**：`auto r = rt.value()->run(ctx);` 之后 `r` **从未被使用**；三个节点 config 都是 `{}` 必然失败，`:202-205` 只断 `!st.empty()` 与 id 非空 —— **没有任何 status 值被断言** |
| **M10** | `unit/rt006_scheduler_test.cpp:139-168` | **恒真门（往返自证）**：两个 Scheduler 各跑同一个 4 节点 DAG，所有节点无条件 `return Result<void>::success()`，无数据无 checksum，只比状态向量。结构上不可能红 |
| **M11** | `unit/normalize_workflow_test.cpp:376-381` | **断言方向与自身注释相反**：注释「峰值**不得**随并发度增长」，代码断的是 `if (peak < prev_peak) monotone_ok = false;`（**必须非递减**）。一个真正做了全局背压的实现（峰值持平甚至下降）会判红；一个每 worker 独占整帧、峰值线性增长的泄漏实现会判绿 |
| **M12** | `unit/normalize_workflow_test.cpp:12` | **声明了没做**：文件头声称「node_wall/queue_wait 单位 s、块事件单位 B」；全文件无一处断言单位取值，K2（`:423`）只查子串 `"unit":` 存在 |
| **M13** | `unit/normalize_workflow_test.cpp:327` | **无余量的墙钟门**：`check(on1.wall < off1.wall, …)` 在负载 CI 上天然 flaky，且它是 H 段唯一真正检验预取收益的门 |
| **M14** | `unit/normalize_workflow_test.cpp:28,336,364` + `run/RELEASE-05/evidence/*` | **归档未跟踪**：`ARCH502_EVIDENCE_DIR` 默认写 `run/RELEASE-05/evidence`；`git log -1 -- run/RELEASE-05/evidence/arch502_prefetch.txt` 只命中品牌改名 commit `14a50e3e`（不是重跑）。归档值 `peak_inflight_bytes=32768`（1/2/4 worker 恒定）说明性质**事实上成立**，但**判据测不到**（见 M11） |
| **M15** | `unit/drizzle_adapter_impl.cpp:341,360,372-385` | **coverage 平面从未被验**：`hp_drizzle_reverse_run(&rin, plane_direct, plane_plugin, &rres)` 第三实参是 `coverage_out`（拿到的是直连路径的 coverage），随后 `:372-383` 的 base64 解码又把它**整个覆写**成插件 signal —— 今天结果正确纯属赋值顺序的巧合 |
| **M16** | `backend/test_abi_kernels.py:72-148` | **参考是 SUT 的逐语句转写**：`:73` docstring 自陈「与 `baseline_kernels_impl.inc` **合同同式**」。逐条对应已核实（`1e-6` / `0.25` / `3.0*mad` / 同样的钳制与双线性权重 / `1.4826` vs `1.482602218505602f`）。只能抓转写笔误，**不能见证 kernel 公式在科学上是对的** |

**外加一条须修（判定层次，不在单文件维度）**：

| ID | 位置 | 缺陷 |
|---|---|---|
| **M17** | 本片 3 个负例 JSON + `config/test_cfg001_negative.py:15` | **判据读桩（整条线）**：`C.validate` → `cfg_common.py:10` `VALIDATOR = eng/tests/common/jsonschema_min.py`。实测 `lib/` 全树 55 处 `contracts/schemas` 命中**全是注释**，`lib/infrastructure/cli/parser.cpp` 不用 schema 文件（手写解析）。⇒ 负例证明的是「这份 schema 在这份迷你校验器下拒绝 X」，**不是「交付的 CLI 拒绝 X」**。反例：把 `parser.cpp` 的 `precision_mode` 枚举校验删掉 ⇒ `precision_out_of_domain.phase_config.json` 照样被产品接受，全部负例测试仍绿 |

### 4.3 建议（12 条）

| ID | 位置 | 建议 |
|---|---|---|
| S1 | `unit/p2_samp/p2_samp_test.cpp:11` + `:138` | 引用的「独立 Oracle」`run/v6/p2-samp/oracle/check_spec.py` **全仓不存在**（`run/v6` 下只有 `p3-rsmp`）。`:618-750` 的 `dump_json` 产物**无任何消费者**；「结构一致性另由 check_spec.py 机器校验」无凭据 |
| S2 | `unit/p2_samp/p2_samp_test.cpp:546-604` | `case_determinism` 把**纯函数与自身比较**（同进程、同输入、3 次 memcmp），结构上不可能红。对照 `p2001:1128` 的 `test_worker_parity` 才是真跨并发度对拍 |
| S3 | `unit/p2_samp/p2_samp_test.cpp:139-145` | forbidden token 表在测试里硬编码拷贝，且是**单向**的（生产**新增** token 永不可见） |
| S4 | `cli/test_memory_growth.py:115-119` | test_07 断言的 5 个键全部来自**测试自己的 printf 格式串**（`:25-28`）；探针打出的 peak/growth/pct/n/slope **无一个值被断言** |
| S5 | `unit/core_artifact_test.cpp:49-63` | roundtrip 只比对 7 个字段；`schema_version`/`coordinate`/`ownership` 与 **provenance 全族**未验 |
| S6 | `unit/core_artifact_test.cpp:65-72` | `science_hash` 只测「同输入同哈希 / 改 `input_hash` 则变 / 长 16」。若实现只哈希 `input_hash` 一个字段，三条全绿 |
| S7 | `unit/mon001_recorder_test.cpp:104-108` | 第二次 `getline` 失败时 `line` 保留表头内容（表头含逗号）⇒ **零数据行也绿** |
| S8 | `unit/cpu001_provider_selftest.cpp:110-115` | 注释承诺「kernel 表与共享注册表一致(**12 项**)」，断言只有 `kernel_count > 0`；且 `:114` 在 `kernels[0]` 前未复查非空 |
| S9 | `unit/p1_drz/p1_drz_oracle.hpp:158-162` | `rel_close` 的 `if (scale == 0.0L) return true;` —— 0 对 0 通过。当前消费方 fixture 非退化，未触发 |
| S10 | `testkit/registry.json:18` | **伪引**：`tolerance_source` 引 `docs/engineering/TRACEABILITY_SPEC.md §2`；实测 `:19` = 「## 2. 追溯链与分层」，全节讲 8 层必填矩阵，**无任何容差策略内容** |
| S11 | `testkit/schemas/test_metadata.schema.json:5,15,24-25` | `:5` description 写「五类 label」，`:15` pattern 强制 **8 种**前缀；`:24` 把 `MISSING`（期望缺失）列为**合法枚举值** = 把「期望缺失」编码成合规；`:25` `current_commit` 只验 40-hex 形状，registry 6 条全停在 `2a8676bf` 而 HEAD 是 `850a9ede`，**归档未重跑且 schema 结构上不可能发现** |
| S12 | `validation/release02/c_delta_composition/convergence_trend.py:44` | **恒真条件**：`np.where(denf+anchor>0, …, 0.0)` —— `denf` 是非负权重 bincount（≥0）、`anchor=0.001` 恒 >0 ⇒ `else` 分支永不可达。同文件无 `assert`/`sys.exit`，退出码恒 0；`:27` 的 Huber IRLS 是生产 `upm.cpp` 的同款重实现（`1.345` 两侧各自硬编码、互不绑定）；`:3` 的输入数据 MISSING |

---

## 5. 我主动构造的反例

> 纪律：未运行任何测试/编译。所有反例均为**按代码推演**的注入设计，供前台实际注入验证。

| # | 构造 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| R1 | 把 `p3_op_integrate` 的产物序列化成 JSON **数组**而非对象 | 推翻 p2001 §30.1 ivar oracle 的非退化性 | **推翻成功** —— `:497` `if (!intj.is_object()) return;` 命中，`failures` 不变，测试打印 PASS，26 万像素的 `Σivar` / 加权均值 oracle **零执行** |
| R2 | 把 `p2_rejection.json` 的 `sample_mask_offset` 键改名（或掩码截断到 1 字节） | 推翻 p2001 `:551` 的 oracle 守卫 | **推翻成功** —— `sm_off` 取默认 `~0ull` ⇒ 条件假 ⇒ 无 else ⇒ `:552-587` 整段逐像素 oracle 蒸发，测试仍绿 |
| R3 | 把 `apply_upm` 写帧 2 的 corrected 缓冲时**写成帧 1 的副本** | 推翻 p2001 的 UPM 校正锚 | **部分推翻** —— `mean_after==0 < 5.0` ⇒ 绿；而帧 2 的测光已被完全摧毁。即 `:466` 的门在"灾难性错误"方向无判别力（`:458` 的 `before` 是死占位） |
| R4 | 把 `Scheduler::cancel()` 改成空实现 `{}` | 推翻 rt006 的取消门 | **推翻成功** —— `:114` `CHECK(r.ok() \|\| r.failed())` 是重言式（`contracts.h:139-140` 两函数严格互补）⇒ `test_cancel` 无论取消是否实现都绿 |
| R5 | 把 worker 池换成**严格串行**循环 | 推翻 rt006 的 diamond 并发门 | **推翻成功** —— `parallel_peak` 恒为 1，满足 `>=1`，全绿 |
| R6 | 完整复现 FLAKE-01 生产缺陷（`tl_fail_seq()` 改非 thread_local 全局 + `product_begin` 里 `reset()`） | 推翻 fix208 的「帧级归因」T3 | **推翻成功** —— 两线程各自构造 epoch、各自在**同一线程内**置位并读，中间无共享写入 ⇒ `conc_a=conc_b=1` 照旧。`:288-290` 断言消息「旧实现必红」不成立（真正有判别力的是同线程串行的 T2） |
| R7 | 把 `evaluate_gate` 改成无条件 `return GateDiag::SingleThreaded;` | 推翻 p1_resource 的门判决 | **推翻成功** —— `:86` 与 `:90` 两条断言对 `avg_equivalent_cores` 无关，恒绿 |
| R8 | 把生产 `compute_psfsw_weights` 的指数 α 从 2 改成 3 | 推翻 p1_reopen_oracle 的「独立 Oracle」 | **推翻成功** —— oracle 从不读 C++ 任何权重（`wt`/`med` 绑定后未用），照旧打印 `ORACLE_PASS` |
| R9 | 把 `p2_session.cpp` 的 `p2_coverage_build` 函数体换成返回全零的桩，**名字只留在注释里** | 推翻 p2_ir_facade 的四条断言 | **推翻成功** —— 全部是 `std::string::find()` 源码文本匹配，注释即可满足；`:57-62` 的"顺序"取首次出现下标 |
| R10 | 把 `apply_upm` 改成"帧 2 = 帧 1 副本"，同时把中心 RA 翻 180° | 推翻 p3002 的 P3 重采样门 | **未推翻**（这正是缺陷本身）—— 全常量 100 夹具使任何采样器/投影等价，`!content.empty()` + 字节相等全绿。**P3 创新点在本文件零数值验证** |
| R11 | 把 `reverse` 的 coverage 语义整体写坏（`coverage_f32` 错写成 `signal_f32`） | 推翻 drizzle reverse BITWISE 门 | **推翻成功** —— coverage 平面从未被读取；今日正确纯属 `plane_plugin` 先被直连路径填 coverage、再被 base64 解码整体覆写的赋值顺序巧合 |
| R12 | 把 `p2_upm_model.json` 的 `identifiability` 段删掉 | 推翻 p2001 的 AUDIT-PERSIST 门 | **推翻成功** —— `:645-650` 读不到即 `upm_has_idn=false` ⇒ 走**更宽**的 legacy 分支（`:685-692` 只要求 `rank` 为 null + 具名 reason）⇒ 转绿。注释 `:641` 自承「判据由产品自己决定」 |
| R13 | 把 `ref_close` 的 fixture 退化为空 overlap 集（`D_` 全 0） | 推翻 p1_drz_oracle 的 7 条对拍 | **部分推翻** —— oracle 全返 0；SUT 同步退化则全空转。**当前未触发**（消费方 `p1_drz_test.cpp:128-130` 确有 `items.push_back`），登记为隐患 |
| R14 | 把 `resource_gate.h::gate_enforcement` 的两个未命名参数改成任意类型、函数体改成 `return e;` 之类 | 推翻 fix208 test_04 | **推翻成功** —— `:189-195` 四条断言等价于对常量函数断言常量 |

---

## 6. 盲复算

**方法**：对 §4 中判定为「阻断/须修」的 11 条，遮蔽我自己的判定文字，仅依据**原文**重新取证，再比对。

| 项 | 我的判定 | 盲复算结果 | 一致性 |
|---|---|---|---|
| p2001 PHASE2_UPM 伪引 | 阻断 | `awk NR==190/204/219` + `grep -c chi2_red` + `grep -n §7a` + `grep -n sky_plane commands.cpp` → 三行内容全不符、chi2_red=0、§7a@200、commands 零命中 | **一致** |
| p1_reopen 恒真门 | 阻断 | 手推 `med=0.5(w0+w1)`、`wnorm=w/med` ⇒ `mmed=0.5·(w0+w1)/med=0.5·2=1.0` | **一致** |
| p1_resource 判决冻结 | 阻断 | 只读 `:78-90`，不看我的结论：注释说 `LowAvgCores`，断言 `SingleThreaded`，注入 1.9 后仍 `SingleThreaded` | **一致** |
| rt006:114 恒真 | 阻断 | 读 `contracts.h:139-140`：`ok()=!has_value()`、`failed()=has_value()` ⇒ 析取恒真 | **一致** |
| rt006:46 并发门 | 须修 | 只读 `:46` 与注释「b/c 并发」：`>=1` 被单节点满足 | **一致** |
| fix208 test_04 | 阻断 | 读 `resource_gate.h:265-270`：两形参皆注释掉、函数体单一 return | **一致** |
| p3001 FROZEN | 恒红 | `grep -c "FROZEN" docs/science/PHASE3_HIPS_TO_FITS.md` = 0 | **一致** |
| p3001 docs/api | 悬空 | `ls docs/api` → No such file；`find docs -name PHASE3_API_V1.md` → `docs/engineering/…` | **一致** |
| p2_samp 是否孤儿 | — | 我原先采信子代理 A 的「孤儿」并准备登记；复核 `grep -n "p2_samp" CMakeLists.txt` → **`CMakeLists.txt:1491 add_subdirectory(eng/tests/unit/p2_samp p2_samp)` 存在** | **我推翻了自己（并推翻子代理 A）** |
| p1_noise 是否孤儿 | 阻断 | `CMakeLists.txt:383` 确为注释态；`eng/tests/unit/CMakeLists.txt:1328` 双 target 门卫；两 target 只经该注释行可达 | **一致** |
| 8 份 Python 是否零注册 | — | `eng/ci/`、`pytest.ini`、`pyproject.toml`、`.github/` 均不存在；CMake 对 8 文件零命中 ⇒ 但 `eng/tests/test_index.csv:2-10` 按目录登记了 `unittest discover` 运行器 | **部分推翻（子代理说「全部孤儿」过重）** |

**结论：11 条中 9 条盲复算一致，2 条为我推翻子代理、1 条我推翻自己。判定方向无偏松、无偏严。**

口径说明（涉及数量）：本片 §4 的「发现数」是**按文件:行计的独立缺陷条目**，不是"门实例/去重门/整改分母"任何一层。若换算为**门实例层**，47 份文件对应的可执行判据门约 180 条（按 `CHECK`/`assert`/`P1WCS_CHECK` 计数），其中判定失效的约 40 条 ⇒ **整改分母 ≈ 180 门实例**；若按**去重门**（同名判据合并）计，约 130 条，失效约 35 条。本报告不给出后两个数字的权威值，因未经前台复跑核准。

---

## 7. 子代理派发记录

**派发 5 个**（按成员文件分组，覆盖 47/47）。全部只读、零写、零测试执行。

| 组 | 成员 | 覆盖文件 |
|---|---|---|
| A | unit 五个大件 | p2001 / p2_samp / p3002 / normalize_workflow / rt006 |
| B | Python CLI/artifact/abi/config | test_provenance / test_fix208 / test_module_registry / test_multiblock / test_monitor_events / test_memory_growth / test_cli_v7_surface / test_cfg001 |
| C | backend oracle 组 | p1_reopen_oracle / test_calibration_oracle / p3_publish_order / test_abi_kernels / test_p2002_parallel_upm / p3_output_probe_main / test_p3001_science_freeze / cheat_backend |
| D | unit impl/oracle 组 | drizzle_adapter_impl+test / p1wcs_tests_negative / p1_drz_oracle / cli_process / cpu001 / mon001 / core_artifact / p1_nside / p2_ir_facade / p1_resource / p1_psfw / p1_noise |
| E | cpu/fixture/testkit/validation/cmake | provider_avx2_oracle_main / provider_handshake / registry.json / 两个 CMakeLists / 3 个负例 JSON / test_metadata.schema / convergence_trend / stageD_samples / debug3 / negative_bad_input |

### 逐条复核：采纳 / 修正 / **否决**

| 子代理结论 | 我的处置 | 理由 |
|---|---|---|
| **A：「`p2_samp` 是孤儿，全仓无 `add_subdirectory`」** | **否决** | A 只在 `eng/` 内 grep，漏查**根** `CMakeLists.txt`。实测 `CMakeLists.txt:1491 add_subdirectory(eng/tests/unit/p2_samp p2_samp)` 存在，且 `eng/tests/unit/p2_samp/CMakeLists.txt:25-66` 注册了 `p2_samp_test` + 9 个 per-case + `p2_samp_all`。**若采信 A 的误判，会把一次接线修复写成「补 add_subdirectory」，是错误施工指令。** |
| A：「`Result<void>` 的 `rc.failed()` 不是恒真门」 | 采纳 | 独立复核方向正确：`Result` 默认构造为 ok，`run_node` 提前 return 不回填 rc 时测试会**变红**而非变绿。这是我原本准备写的一条误报，子代理帮我排除了。 |
| B：「8 份 Python 文件全部是孤儿、零注册」 | **修正后采纳** | 方向对（`eng/ci/`、`pytest.ini`、`.github/` 确不存在；CMake 零命中），但「全部孤儿」过重：`eng/tests/test_index.csv:2-10` 按目录登记了 `unittest discover` 运行器，且 `eng/tests/artifact` 标 PASS/137 例。我把该条改写为「无自动执行面，仅有手工台账；其引用的 CI 注册表已随 `eng/ci/` 一并消失」。 |
| B：「`test_history_not_in_digest` 是空转恒真门」 | 采纳 | 我自己读 `:369-375` 得到同一结论，并进一步独立确认 `provenance_digest_hex` 无 history 形参。 |
| C：「`test_p3001_science_freeze.py:19` 指向不存在的 `docs/api/…`，每次 ERROR」 | 采纳 | 我自己 `ls docs/api` + `find docs -name PHASE3_API_V1.md` 复验。 |
| C：「`:266-273` 是代数恒等式」 | 采纳 | 我**独立重算**了恒等式（见 §5 R8），非转述。 |
| C：「`cheat_backend.cpp` 无法泄漏进生产接线」 | 采纳 | 方向正确；我补充：它被 `test_bench_harness.py` 断言必须判 `ORACLE_FAIL`，是本片最强的反恒真设计。 |
| **D：「`p1_resource_test.cpp:86,90` 把已知错误判决冻结」** | **采纳并加重** | 我读原文后发现比 D 说的更严重：D 说「恒真门」，我确认这是**文件内自相矛盾**（`:79-80` 注释说正确答案 `LowAvgCores`，`:86` 断言 `SingleThreaded`）**且**构成**缺陷保护锁**（修好生产会先红）。 |
| D：「`p2_ir_facade_test.cpp` 只 grep 源码文本」 | 采纳 | 我读全文 78 行确认；补一条 D 未提的：CMake 只链 `acsd_contracts`，被测 `p2_session.cpp` 从未编入该可执行。 |
| **D/S8：「`out/` 构建树的绿/红不能作 HEAD 证据」** | 采纳为方法学约束 | 与我自己发现的「`run/` 下 11 份陈旧源码快照」同族；我据此把所有 grep 重跑在排除 `run/` 的活树上。 |
| **E：「registry.json + test_metadata.schema.json 零活消费者，`check_testkit.py` 已删」** | 采纳 | 我复验：`eng/tools/testkit/` 只剩 `README.md`；`eng/tests/test_index.csv:21` 仍注册 `python3 eng/tools/testkit/check_testkit.py --list` 并注「检查器 --list 实测 PASS」。**K4「期望来源审计」已死 ⇒ `expectation_source` 字段无执行者。** |
| E：「`convergence_trend.py:44` 是恒真条件」 | 采纳 | 我自己推演 `denf≥0`（非负权重 bincount）+ `anchor=0.001` ⇒ 条件恒真，`else` 分支不可达。 |
| E：「`provider_handshake_test.c:162` 内层 `if` 恒真（冗余）」 | 采纳（降级为建议） | 我复核 `:157` 循环上界与 `:162` 的内层 `if` 同值，确为冗余；影响极小，列 S 级。 |
| E：「`provider_capability_gate_test.c` 存在，该注释诚实」 | 采纳（正面结论） | 我未亲自复核该文件存在性，但采信并**未据此下任何结论**（无风险方向）。 |
| A/B/E：「`p2_samp` 的禁用 token 表单向、check_spec.py 悬空」 | 采纳 | 我独立复验 `check_spec.py`：`glob **/check_spec.py` → **No files found**；`run/v6` 下只有 `p3-rsmp`。 |

**否决/修正合计：3 条**（A 的 p2_samp 孤儿、B 的「8 文件全部孤儿」表述、E 的冗余项降级），全部写明理由。

---

## 8. 自证段（可复跑命令）

> 全部只读；不编译、不跑测试、不写任何仓内文件。

```bash
cd "/workspace/Astro CS Database" && git -c core.quotepath=false rev-parse HEAD
# 期望 850a9edefd47434b9ab71bc907c3de1e0814b323

# ── B1 伪引：PHASE2_UPM.md 三条被引行 + commands.cpp ──────────────────
grep -n "^## 7a" docs/science/PHASE2_UPM.md
awk 'NR==190||NR==192||NR==204||NR==219{printf "%d: %s\n", NR, $0}' docs/science/PHASE2_UPM.md
grep -c "chi2_red" docs/science/PHASE2_UPM.md        # 期望 0
grep -n "sky_plane\|upm_model\|node_spacing\|kappa" lib/infrastructure/cli/commands.cpp   # 期望无输出
sed -n '240,250p' lib/infrastructure/cli/commands.cpp

# ── B5/B6 p3001 恒红 + 悬空 ─────────────────────────────────────────
grep -c "FROZEN" docs/science/PHASE3_HIPS_TO_FITS.md  # 期望 0 → :66 恒红
ls -d docs/api                                          # 期望 No such file
find docs -name "PHASE3_API_V1.md"                       # 期望 docs/engineering/PHASE3_API_V1.md

# ── B7 provenance 空转恒真门 ─────────────────────────────────────────
sed -n '369,375p' eng/tests/artifact/test_provenance.py
grep -n "def provenance_digest_hex" -A 8 lib/infrastructure/aio/runtime/artifact_store/provenance.py

# ── B10 p1_noise 孤儿 ────────────────────────────────────────────────
sed -n '383p' CMakeLists.txt                              # 期望注释态
sed -n '1328p' eng/tests/unit/CMakeLists.txt              # 期望 if(TARGET p1noise_under_test AND TARGET acsd_p1_noise)
grep -rn "p1noise_under_test\|acsd_p1_noise" --include=CMakeLists.txt . | grep -v "^./run/"

# ── 我否决子代理 A 的证据：p2_samp 确有接线 ───────────────────────────
grep -n "p2_samp" CMakeLists.txt                          # 期望 1491: add_subdirectory(eng/tests/unit/p2_samp p2_samp)
grep -rn "p2_samp" --include=CMakeLists.txt . | grep -v "^./run/"

# ── M17 负例校验器不是生产校验器 ─────────────────────────────────────
sed -n '9,11p;44,48p' eng/tests/config/cfg_common.py
grep -n "jsonschema\|contracts/schemas" lib/infrastructure/cli/parser.cpp | head   # 期望全是注释行

# ── S1 check_spec.py 不存在（用 glob 工具，因 grep 对纯文件名易误配）
# glob(pattern="**/check_spec.py")   → No files found
# glob(pattern="**/p1sess_fixtures*") → 实存 lib/phase1_session/tests/p1sess/（说明 include 可解析，非悬空）

# ── M4 伪造的故障注入 ────────────────────────────────────────────────
grep -rn "ACSD_IVAR_FAULT" --include=*.cpp --include=*.h lib/ eng/ | grep -v "^./run/"

# ── S10 registry 伪引 / S11 归档未重跑 ───────────────────────────────
sed -n '19,30p' docs/engineering/TRACEABILITY_SPEC.md
git -c core.quotepath=false log -1 --format='%h %ad %s' -- run/RELEASE-05/evidence/arch502_prefetch.txt
cat run/RELEASE-05/evidence/arch502_rss_curve.csv

# ── 接线面（用于方法学复现：排除 run/ 幽灵树）────────────────────────
ls -d eng/ci .github pytest.ini pyproject.toml setup.cfg tox.ini conftest.py 2>&1
sed -n '1,10p' eng/tests/test_index.csv
```

---

## 9. 建议处置顺序（供负责人裁决，本报告不改任何文件）

1. **立刻**：B1（伪引，`p2001:613-618`）。改引到真实条款，或如无对应条款则如实写「文档无此要求」——**不得保留悬空锚**。这是本片最重的一条，因为它让一整段审计断言失去依据。
2. **立刻**：B2 / B4 / B5 / B6 / B7 —— 五条恒真门与两条恒红/悬空。其中 B5+B6 使 `test_p3001_science_freeze.py` 在 HEAD 上**一半 ERROR 一半恒红**，等于该「科学与算法冻结审计」门不存在。
3. **立刻**：B3（缺陷保护锁）。修 `evaluate_gate` 的 worker-p50 缺采样回退分支，**同时**把 `:86/:90` 翻锚为「`avg_equivalent_cores` 真正影响判决」，两件事必须同批，否则修复被测试挡住。
4. **尽快**：M5/M6 —— `p3002` 换**非常量**夹具并对 `p3_resampled.bin` 断言若干采样点的解析值。**P3 是五大创新点之一，当前在其专用节点测试里零数值验证。**
5. **尽快**：M1/M2/M12 —— 给 p2001 的 §30.1 oracle 补 `else CHECK_MSG(false, "oracle 未执行")` 与 `CHECK(sm_off != ~0ull)`（抄生产 `module_adapters.cpp:12376` 的既有守卫）。
6. **尽快**：M15 —— `coverage_out` 传独立 scratch，并加一条 coverage 平面对拍。
7. **尽快**：M17 —— 补一条把三个负例喂给**生产** `parser.cpp`（或 `acsd normalize --config`）的测试。
8. **建议**：B10（孤儿）与 S1（悬空 oracle）按 AGENTS.md §6「退役代码从代码库删除，或保留统一注释块写明原因」处置；S2/S4 两条恒真门直接删除，不损失覆盖。
9. **登记 UNRESOLVED**：`test_provenance.py:656-667` 名为 `..._mismatch_..._rejected_at_publish`，函数体从无 mismatch 样本，docstring 自认「负测放在单元测」——测试名与所属类仍在宣称 store 级拒收。需裁决：改名还是补负例。