# 审稿-P1 · ENG-tests-003 · G08-05 对抗审稿第 1 遍

- 仓库：`/workspace/Astro CS Database`
- HEAD：`850a9edefd47434b9ab71bc907c3de1e0814b323`
- 片清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:1216-1270`
- 审稿人立场：红队。**判据代码不构成正确性证据**；本片所有结论由亲自重读原文 + 独立重算得出。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 成员份数（权威片清单） | **47** |
| 实际读完份数 | **47** |
| 成员总行数（片清单标称） | **10423** |
| 成员总行数（`wc -l` 实测） | **10424**（差 1 = `contracts/product_family/__init__.py` 无末尾换行） |
| 实际读了多少行 | **10424 / 10424** |
| **覆盖率** | **100.0%** |
| **未读完清单** | **无** |

**读取方式**：全部 47 份由本审稿人**逐份打开原文**读完；其中 `eng/tests/config/cfg_common.py`(106) 与 `eng/tests/contracts/data_semantics_anchor_fixlist.json`(122) 的全文在会话上下文中以带行号的完整形式呈现，逐行可核。**零份跳过、零份抽样**。

**覆盖完整性自证**（可复跑）：
```bash
cd "/workspace/Astro CS Database"
python3 - <<'PY'
import os
p='run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml'
L=open(p,encoding='utf-8').read().split('\n')
m=[]
for l in L[1223:1275]:
    if l.startswith('      - '): m.append(l[8:].strip().strip('"'))
    elif l.startswith('  - '): break
print("members:",len(m))
tot=0; miss=[]
for f in m:
    if os.path.exists(f):
        tot+=sum(1 for _ in open(f,'rb'))
    else: miss.append(f)
print("total lines:",tot,"missing:",miss)
PY
```
输出：`members: 47` / `total lines: 10424` / `missing: []`。

---

## 2. 本片判定

### **判定：阻断**

理由：本片存在**多条结构上不可能因真实缺陷翻红的判据**，且其中数条被文档、注释、台账与 PASS 文案**正面引用为「已覆盖 / 已验证」**。这不是「写得不够好」，而是**结论与证据脱钩**：按裁定口径「判据不可信」，这些条目既不能支持其宣称的结论，也不应被计入任何整改分母。

### 最重的 3 条

**① `eng/tests/unit/p3_proj/sin_roundtrip_gate.py:121` —— 极性反转门：缺陷存在判绿、内核修好判红，且内核彻底失败也判「已修好」**

- 判定式唯一一条（`:121`）：`reproduced = worst >= TOL_REPRODUCE_PX(1e-6)`
- `:141-144`：`if reproduced:` → 打印 `SIN-GATE_PASS(REPRODUCED)` → `return 0`（**绿**）
- `:145-149`：否则打印 `SIN-GATE_FAIL(FIXED): 内核已被修好，**必须**同步退役登记` → `return 1`（**红**）
- `:62` `worst = -1.0` 初值 + `:68` `if (e > worst)` + `:51-52` rt 失败返回 `-1.0` ⇒ **内核全失败时 worst 恒为 -1.0，落入 `< 1e-6` 分支，判「FIXED」**
- `:81-84` `make()` 失败 → `return -1.0` ⇒ 同一结局

**任何门禁把「绿」读作「合规」，在本门上的含义正好反过来：绿 = 缺陷仍在。** 且健康态下本门**恒红**，恒红门会把真实缺陷藏在红灯里。

**② `eng/tests/backend/test_isa_bit_manip.py:10,53-57` —— 恒绿门：ISA-005「无位操作热点」从未被任何一次执行检验过**

- `:10` `BMI2_POPCNT = re.compile(r"^(mulx|…|bextr)$", re.I)` —— **缺 `re.MULTILINE`**，objdump 是多行文本，`^`/`$` 只锚整串首尾
- `:53-54` 对 `objdump -d` 整段输出做 `findall` ⇒ **恒返回 `[]`**
- `:55-56` `found = set(...); assertEqual(found, set())` ⇒ **恒 PASS**

**③ `eng/tests/unit/CMakeLists.txt:1328` —— 永久死分支且完全静默：判别力最强的两条噪声面适配器测试从未 configure**

- 根 `CMakeLists.txt:383` `add_subdirectory(lib/algorithms/noise_snr)` **处于注释态**（其自身注释 `:387-395` 明写「本 add_subdirectory 仍**保持注释**（未恢复）⇒ `acsd_p1_noise` 仍不在根图」）
- ⇒ `TARGET acsd_p1_noise` 恒假 ⇒ `if(TARGET p1noise_under_test AND TARGET acsd_p1_noise)` 恒假
- ⇒ `p1_noise_adapter_test`(`:1329`) 与 `add_test(NAME p1_noise_adapter)`(`:1339`) **从未 configure**
- **无 `else()`、无 `message(WARNING)`** ⇒ 门静默消失，无任何痕迹

---

## 3. 逐文件清单（47 份，全部读完）

> 判定口径：**阻断**=结构上不可能因真实缺陷翻红、或结论与证据脱钩；**须修**=可翻红但有盲区/歧义；**通过**=读后未发现本轮口径下的缺陷。

| # | 文件 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `eng/tests/unit/CMakeLists.txt` (1873) | 全 3 段读完 | `:1328` 永久死分支无 else；`:1607-1611` 已知红灯 `w34_snr_reconcile_test` 被**主动注释掉**（FP32/FP64 差 1 ULP，`10.8814226 vs 10.8814227`，相对 9.2e-8，断言要求逐位相等）；`:1826` 段标题「编入 + 注册」但 `w34_p1_apply_oracle`/`w34_p1_qf_oracle` **零 `add_test`**；`:1522` 路径错（`cpp/src/snr_frame_science.cpp` → 实为 `wrapper_phase1/`）；`:96` 引 `docs/KNOWN_LIMITATIONS.md`（不存在）；`:105` 引 `§8.3:609-615`（§8.3 实为 469-481）；`:860/:1124/:1129` 三份 `run/**/REPORT.md` 不存在；`:1506/:1549` 引 `eng/ci/checks.json`（`eng/ci/` 整目录不存在）；`:1734` 硬编码 `python3` 绕过 `${Python3_EXECUTABLE}`；`LABELS` 计数 **0**（143 个 `add_test` 无一设标签） | **阻断** |
| 2 | `eng/tests/unit/p3_proj/p3_proj_test.cpp` (715) | 全文 | `:346-349` `const double constant_ratio = 1.0; CHECK_MSG(constant_ratio < 1.9, "常数 Ω 必红")` —— **纯字面量比较，编译期真，零生产代码接触**，却被文件头 `:14` 列为已覆盖负例；`:172` `CHECK_MSG(registry_selfcheck() == 0)` —— 调**生产自评函数**当证据（`p3_proj.cpp:425-446` 只查表结构，不查任何数值正确性）；`:46` + `:636` `fault_mode` 声明+赋值**从不读取**（死变量，证明无任何注入抵达生产）；`:249-252` 往返门被 `if (st2 == kOk)` 包住 —— `world2pix` 全面改开始报错则该门**零执行仍绿**；`:339-343` `if (sa==kOk && sb==kOk)` 全失败时 `worst` 停在 0.0，`0 < 1e-4` **恒绿**；`:299-311` 「冻结值 1.445」块**完全不接触被测实现**（只用测试自带 `sky_quad_omega()` + 魔数） | **阻断** |
| 3 | `eng/tests/unit/p2_upm/p2_upm_ma_test.cpp` (659) | 全文 | `:257-258` `check(info.kappa < 1.0/info.rank_rtol_effective)` —— **用生产自报的 κ 与自报的 τ 重算生产自己用过的谓词**（结构对称型自证）；`:433-438` provenance 键用**子串**查找：`"rank"` 会被 `"rank_rtol"` 顶替、`"identifiable"` 会被 `"n_unidentified"` 顶替、`"C_theta"` 会被 `"J_C_theta_JT_present"` 顶替 ⇒ 三条存在性门实际恒真；`:14/:301/:302/:513/:524` 魔数 `73.229900948` / `0.808893093661304` / `3.31499999999998` / `1.1701e8` 均无就地推导；`:11` 声称独立 Oracle 在 `oracle/upm_ma_oracle.py`（该文件**不被任何 ctest/CI 调用**，全仓仅被 `test_psf_moffat_oracle.py:41` 当依赖举例引用） | **须修** |
| 4 | `eng/tests/io/test_hips_input_contract.py` (478) | 全文 | `:54-56` `_build_lib()` 见 `LIB_SO.exists()` 即 return —— **陈旧二进制自愈**（`.so` 被 `.gitignore` 忽略且未跟踪，改 `hips_core.c` 后重跑仍链上一版）；`:392` `for i in range(min(cnt.value,3))` **无 `cnt>0` 守卫** ⇒ 叶枚举回归到 0 时该用例**零断言通过**（对照 `:334` 同文件有 `assertGreaterEqual(cnt,1)`）；`:449-474` `digest_tree(d1)==digest_tree(d2)` 两边同生成器同 seed，再让该生成器自己 `--rebuild` —— **生成器自比对** | **须修** |
| 5 | `eng/tests/config/test_cfg004_unified_contract.py` (470) | 全文 | `:5-6` 自称**逐字**引用「§9.71 裁决 2」，但 `可以依然用块状结构` / `大括号块` / `HiPS类似` 在 `docs/` 与 `run/GOVERN-08/工作包-GOVERN-08原件/` **全仓零命中**；`:3/:7/:57` 把 `docs/ACSD_DESIGN.md §3.3` 当「块状结构/JSON 数据块」最高权威 —— `docs/ACSD_DESIGN.md:195` 实为 `### 3.3 精度归属`，`195-207` 行 `blocks\|数据块\|output_dir\|input_lights` 命中数 **0**；`:427` `assertEqual(frozen, block["$ref"] and …)` —— `and` 短路**吞掉链检查**，`$ref` 只要非空就短路，从不校验指向 `#/$defs/projection_code`；`:279` `assertNotEqual(expected, base | {"bogus_knob"})` 自称「防判据恒真」实为**纯代数恒真**；`:436-440` `registered_at` 路径不在三份 schema 内即 `continue` 静默跳过（`checked>=8` 只是地板） | **须修** |
| 6 | `eng/tests/unit/core_pipeline_test.cpp` (429) | 全文 | **正面范例**：`:382` `CHECK(!base.empty())` 在 early-return 之前；`:387-398` 显式双夹具探测 + `CHECK(both_present)` 才用，注释 `:387-388` 直接点名失效形态「负例夹具若缺失, `parser.parse("")` 同样 failed ⇒ 不显式探测就会把『夹具没了』伪装成『负例被正确拒绝』」—— **本片 fail-closed 做得最规范的一处，建议立为仓内范式**。唯一瑕疵：`:189` `parser.validate(r2.value(),…)` 未先查 `r2.ok()`（同文件其余 9 处都查了） | **通过**（一处须修） |
| 7 | `eng/tests/abi/mod001_install_load_check.py` (395) | 全文 | `:13-15`/`:23`/`:31` docstring 称「6 个科学模块（含 `acsd.p1.noise`）」「noop + 6 科学 DLL」，`:71-77 SCIENCE_MODULES` 实为 **5** 条（`:269` 行内消息写对了）；`:236-237` 红灯补救指令指向 `python3 eng/ci/run_checks.py --check CHK-BUILD-LINUX` —— **`eng/ci/` 在 HEAD 不存在**；`:340` `docs/ACSD_DESIGN §6.2 命令树` —— §6.2 实为「流程」，命令树在 §7.1；`:328-331` `if build_so:` **无 else** ⇒ PATH_ESCAPE(4) 负检在 build 树缺 `.so` 时**静默消失且不记 FAIL** | **须修** |
| 8 | `eng/tests/unit/rt001_unique_executor_test.cpp` (382) | 全文 | `:136-138` `kSigVal=100.0f` + `const_px(){return *static_cast<float*>(user);}` ⇒ 512×512 整幅每像素恒 100.0；唯一 parity 门 `:330-336` 因此对**任何确定性重采样器**（错投影/错中心/错标度/转置/off-by-one）都成立 ⇒ 对重采样几何**零分辨力**；`:330-336`/`:341-342` 比较文件内容但**从不断言非空**，两个 0 字节文件即满足；`:62` `static int failures` 在 worker lambda（`:99` `CHECK_MSG`）内非原子自增；`:354` `::setenv`/`::unsetenv` 无 `_WIN32` 守卫（同文件 `:51-57` 却为 getpid 备了守卫）；`:132-135` 与 `:230-232` 注释互相矛盾（后者描述 16×16 夹具的旧世界） | **须修** |
| 9 | `eng/tests/unit/p1snr/p1snr_frame_parity_test.cpp` (337) | 全文 | `:63` 注释称「6 颗不同亮度的孤立高斯星 + 确定性伪噪声背景」，`:69-72` 实为 **4** 颗（`sx[4]/sy[4]/amp[4]`, `for k<4`）、背景 `:68` `v = 100.0` 恒定、**零噪声**；`:227` 断言消息仍写「6 星<上限」；`:134-137` `same_f64` 键缺失时返回 false ⇒ `:273-276` `!same_f64(...)` 在**字段缺失时为真**，即专职证明「parity 锁非恒真」的那条见证**自身 fail-open**；`:183-195` 注释与消息称 `p1_flux.json` 供「字节对照用」，实际只有 `fs::exists`，**全文件无任何字节比对** | **须修** |
| 10 | `eng/tests/backend/test_phase3_reproject_oracle.py` (330) | 全文 | `:205-218` 往返门：读生产 `CD/CRPIX/CRVAL` → 本文件自己的 `ungnomonic_vector()`(`:114`) 正算 → 本文件自己的 `gnomonic_vector()`(`:100`) 反算 → 断言回到原像素。三者互为解析逆，断言 `CD⁻¹·gno(ung(CD·p))=p` **是代数恒等式，与生产写的任何数值无关**；`:255` `assertLess(abs((ra-crval0+180)%360-180), 2.0)` —— 足迹 24px×0.02°/px ⇒ 中心张角结构上 ≤0.48°，**恒真**；`:167` `_run` 只 `assertIn("exit_code", r.stdout)`、**不校验其值**，且 `:158` `self.out` 是 class 级共享目录且运行前不清空 ⇒ run 失败时后续测试读**陈旧产物**并通过；`:39` `-I{REPO}/build` 依赖 configure 生成的 `version_generated.h` ⇒ 干净树不可跑 | **阻断** |
| 11 | `eng/tests/unit/cpu006_bench_report_test.cpp` (285) | 全文 | `:128-141` 段标题「阈值门: MAD/median > 0.35」，实际 `:131` **手工写** `dispersion_ok = false; // 离散度超限(重算)`、`:137` 手工写 `= true;`，35% 阈值真身在 `bench_report.cpp:156-157`，**本门从不经聚合器触发它**；`:43` `cand()` 永远给 `mad_ns = median*0.1` ⇒ `agg1` 输出的 `dispersion_ok` 永远为真；`:200` `replace(pos, 33, "acsd.benchmark-report/vX")` 而 `"acsd.benchmark-report/v1"` 实长 **24** ⇒ 多吃 9 字节把 JSON 打成结构性畸形，该负控**因任何原因都通过**；`:113-118` 「恰 1% 不算平手」靠 `1.0/100.0 == 1e-2` **逐位相等**才绿；`:237` release suite（`kernels.size()==12`、`entries==36` 这些最强断言）**默认跳过**，而 `:280` PASS 文案列「release 门控」为已覆盖；`:64-66` provenance 全是硬编码占位（`"0.0.0-alpha.0+gabcdef123456"`、`std::string(64,bin_byte)`），`:190-191` 只查键存在 | **阻断** |
| 12 | `eng/tests/cli/test_cli_build.py` (279) | 全文 | **正面范例**：`:255-262`、`:270-276` 四处**注入式负控**（改 target 名 / 锚点漂回退役路径 / 注入全局 ISA 旗标 / `march=native`），真负控。**须修**：`:120-130` 复用既有 `build/acsd`、**从不构建、不比对新鲜度**（`ACSD_CLI_BIN` 还能指向任意二进制），整面 CLI 合同（help golden / version / 模板键 / unknown-key 门）可对任意旧二进制全绿；`:86`/`:159` 引「§6.2 命令树逐行对照」—— §6.2 是「流程」，命令树在 §7.1，且 §7.1 原文无 `acsd ` 前缀而 `EXPECTED_HELP_LINES`(`:89-97`) 带前缀 ⇒ 「逐行」在内容层也不成立；`:110` `@skipUnless(which("cmake") and which("g++"))` 罩住全类，但 `test_05`/`test_06` 是纯 CMakeLists 文本断言，**不需编译器** | **须修** |
| 13 | `eng/tests/cli/test_phase2_inprocess.py` (249) | 全文 | `:206-210` `for fn in os.listdir(out): if fn.startswith("acsd_run_"):` **无 `assertTrue(found)` 守卫** ⇒ 取消若完全不写 manifest，「取消不得写 complete manifest」**真空通过**；`:158-165` `if os.path.isdir(task_dir):` **无 else** ⇒ 无 `/proc` 宿主上 `children=[]`，「不得产生子进程」**真空通过**；`:20-31` 同样复用既有 `build/acsd` 不校验新鲜度 | **须修** |
| 14 | `eng/tests/cpu/dispatch/run_cpu_capability_checks.py` (225) | 全文 | **正面**：`:182-194` 用硬编码 feature 名表把 `features.hw/os_safe` 数组**独立解码**并与位掩码交叉比对，是真正的双表示互校；`:141-150` gcc/clang 双编译，缺 clang 即 `fail()`。**须修**：`:204-209` 核心计数禁令只扫**顶层** `required|properties`，嵌在 `$defs/items` 下的 `core_count` 漏检（docstring `:10` 的声明比实现宽）；`:70-121` 自实现 schema 校验器不支持 `$ref`（该 schema 零 `$ref`，无实际损失），`:82-86` 未设 `additionalProperties:False` 时静默接受未知属性 | **通过** |
| 15 | `eng/tests/integration/p2_integrate/p2_weight_token_gate_negative_test.cpp` (224) | 全文 | `:155-168` 「每次调用都必须绑定返回值」的实现是**语法代理**：只查调用点前一个非空白字符是不是 `=` ⇒ `int ignored = f(...); (void)ignored;` 判为 **bound** 而 rc 仍被丢弃 —— **挡不住它自述要挡的退化**（`:150` 只用字符串匹配堵了 `(void)tok_ok_rc` 一种写法）；`:198-205` `closes >= 8` 是**全文件字面量计数**，与 `:189-196` 那四个被裁决分支**无绑定**，且实测生产源恰好 8 处 ⇒ 零余量；`:44-45` 「与 `phase2_integrate.cpp` 逐字一致」**无断言强制**，冻结禁词表可静默缩水；`:67-68` 测了 `p2_weight_source_token_reject(nullptr,1,…)` 但**没测** `(nullptr,0,…)`（空 token 列表），而 `:86-88` 对面那道门测了 | **须修** |
| 16 | `eng/tests/backend/test_hardware_inspect.py` (205) | 全文 | `:65-68` `_pick_dir` 对 `HOST` 与 `CRYPTO` **各传两次完全相同的路径** ⇒ docstring `:12` 声称的「旧路径回退」是**死代码**；`:176-178` `if m:` **无 else** ⇒ `/proc/cpuinfo` 无 `vendor_id` 时 vendor 跨检**静默跳过**（同文件 `:174` 读不到 cpuinfo 会崩，两条路径口径相反）；`:183` `assertIn(platform.machine(), ("x86_64",))` 硬编码单架构 ⇒ aarch64 runner 上 `test_03` **恒红**；`:149-161` 自称「schema 验证」实则只查 required 存在 / const / integer-minimum，**不查 enum、nested required、minLength、嵌套 additionalProperties**；`:143/:184-185` 探针被 `argv` 喂入自造 build 串再断言输出匹配该模式 ⇒ 往返自证，生产版本派生路径未执行 | **须修** |
| 17 | `eng/tests/unit/p3_proj/p3_proj_probe.cpp` (199) | 全文 | `:62` `p3_wcs_applicability("TAN")` **硬编码 "TAN"**，无论探针被要求跑 SIN/CAR/AIT ⇒ **非 TAN 探针被打印并可能按 TAN 的合同值裁决**；`:100-106` `if (!std::isfinite(v)) continue;` 把非有限 Ω **剔出 min/max 统计**；`:112-116`/`:150` 失败像素被 `continue` 剔出交叉/往返样本 —— 被剔掉的正是 Ω 变 NaN/负值（FZ-P3-OMEGA-NONCONST 的失效形态）；`:32-37` `parse_id` 对未识别投影名**静默回落 kAIT** | **须修** |
| 18 | `eng/tests/unit/aio/check_atomic_durability.py` (180) | 全文 | **正面范例**：`:136-138` 判「LD_PRELOAD 未生效 ⇒ 负例空转」为 FAIL；`:141-143` 要求 `EVENT FSYNC-DIR-FAIL` 出现在负例输出；`:158-168` 要求注入与不注入的 `status` **和** `durability` 都不同（非恒真见证）；`:94-107` `published_object_present` 在探针自报之外**独立读盘核对字节**。唯一窄缝：runner 从不检查 `FSYNC-PASS … file`，故与注入器 `:51` 的 dlsym-NULL 回落叠加时「文件 fsync 真的发生了」不可证 | **通过** |
| 19 | `eng/tests/backend/test_p2004_reject_integrate.py` (176) | 全文 | `:152-171` **5 个测试、1 个判据**：driver 是单进程跑完全部断言后打一个 `P2-004 DRIVER PASS`，Python 侧 5 个用例全是 `returncode==0` / `"PASS" in stdout`；`test_02_auto_resolves`(`:157-160`) 除退出码外**零新增断言**，docstring 却称验证 AUTO 解析；`:116` `fs[4] = {0.5,0.5,0.5,0.5}` 配 `:124` 注释「max(accepted support)」⇒ max/min/mean/中位数**都给 0.5**，无法区分任何聚合算子；`:6` docstring 声称覆盖「hot pixel / streak / 星核 / …ivar」—— 三者**一个未测**，ivar 侧只有 NaN/负/零**权重** | **须修** |
| 20 | `eng/tests/oracle/gaia_oracle.py` (170) | 全文 | **空集恒真门**：`:91-103` O1 循环空则 `failures += len(manifest)-exact` = 0-0；`:122-130` O2 空则 `miss=0`、`false_pos = sum(1 for u in used if not u)` 作用于 `[]` = 0；`:143-155` O3 两侧 `polar` 都空 ⇒ `len` 相等、`bad=0`；`:158-163` O4 `polar_pos` 空 ⇒ `missing_polar` 空。**全文件无 `if not manifest: return 1`、无 `len>0` 守卫** ⇒ 「极区剪枝必须不影响极冠命中」这条语义在极区集合为空时**完全无判别力**（过度剪枝把极冠全剪掉时 O3/O4 真空通过）。正面：`:88-89` 明写 libm 末位 ulp 容差来源 | **阻断** |
| 21 | `eng/tests/validation/release02/q3_additive_truth/src/q3core.py` (163) | 全文 | `:71` `"""Pre-filter…"""` 位于 `:69-70` 可执行语句**之后** ⇒ 不是 docstring，是**死字符串表达式**；`:120` docstring 写返回 `xA,yA,xB,YB,yA,yB`，`:135` 实际返回 `xA,yA,xB,yB,vA,vB`（键名 `vA/vB` 而非 `yA/yB`）；`:24` `NAXIS1/2=4096`、`:91/:107` `2048.0`、`:122` `box=(0,4096,0,4096)`、`:129` `4095` **全部硬编码**，从不从 FITS 头派生 ⇒ 非 4096² 帧被静默错变换 | **建议** |
| 22 | `eng/tests/unit/p3_proj/sin_roundtrip_gate.py` (153) | 全文 | 见 §2①。补充：`:39` `CONTRACT_TOL_PX = 1e-8` **全文只被写入 JSON(`:123`) 与打印(`:138`)，从不参与任何比较**（子代理称之为「两个数量级死区」——**我部分否决**：`:34-38` 注释诚实说明该紧门仅在 scale ≥ 0.9″/px 有保守性证据，而本门扫 0.5″/px 低于该下限，故 1e-6 才是适用阈值。**真正的缺陷是极性反转 + 失败即 FIXED，不是死区本身**）；`:61` 注释「FITS 1-based 中心 ⇒ 0-based **31.5**」而代码 `:61` 用 `cx = cy = 32.5` ⇒ **off-by-one，注释与代码矛盾**；`:58` 探针点 `dec=2.0°`（θ=88°）经我复核**确为**声称的消去高发区，此处**不是**缺陷 | **阻断** |
| 23 | `eng/tests/artifact/test_manifest_schema_negative.py` (151) | 全文 | **14 条断言清一色 `assertFalse(ok)` + `assertTrue(errs)`，全文件无一条正例锚** ⇒ `Validator` 恒返回 `(False,["x"])` 即满足全部 14 条；`:70` 注释「python json 序列化为 Infinity → **字符串**」**事实错误**（`json.dumps` 写裸 `Infinity`），且 `:71` `replace("1e+999","Infinity")` 是**死 replace**（`json.dumps` 从不输出 `1e+999`），测试仍绿纯属侥幸；`:22` `load_strict_json` 导入后从未使用 | **须修** |
| 24 | `eng/tests/validation/release02/phot_verify/pixel_measure.py` (147) | 全文 | `:115-118` `def mad(v): … if v.size == 0: return 0.0` ⇒ 任何 `mad(M) <= thr` 形式的门**恒绿**；而同文件 `:133/135/136` 的 `np.nanmax/np.nanmin` 在全 NaN 时给 NaN，`NaN <= thr` 为 False ⇒ **同一文件两套相反方向的失效**；`:22` `cal_path` 硬编码 `run/RELEASE-02/L4-rebuild/norm/…`（该目录不存在）⇒ 模块不可运行；`:63-65` `max_stars` 截断**只留最亮星** | **须修** |
| 25 | `eng/tests/validation/release02/q1_photometry_gradient/src/real_data.py` (132) | 全文 | `:16` 硬编码绝对路径 `'/workspace/Astro CS Database/…'`；`:18` 输入 `p2_samples.json` 不存在 ⇒ **不可运行**；`:62` `len(common)<150` 丢弃的正是**重叠最小、最可能有接缝**的帧对；`:98` `irr_mult = pp(rfit)*B + pp(rres)*B` 把「相干+非相干」的 p95−p5 **当作可加**（p95−p5 不可加）⇒ 系统性**高估**不可约乘性残差，直接抬高 `:105` 的 `mult_irr/add_irr`；`:109-114` 是一段**死代码**（`A`/`bvec` 算完在 `:114` 被整体覆盖，`:112` `math.log(np.median([1.0])) if False else 0.0` 恒取 0.0 分支）；`:68-69` 每对减**二维二次面**，多于生产只做**每帧标量**所能减的 ⇒ `res_pp` 天然偏小、结论偏「可去」 | **建议** |
| 26 | `eng/tests/unit/io_reentrant_test.cpp` (125) | 全文 | `:35` 写入已知真值 `data[i] = i*0.5f`，但 `:86` `ref = read_hash(path)` —— **参照值是被判值的同函数同入参同代码路径**，真值**从不参与比对** ⇒ 系统性误读（datatype/区域/字节序/`naxes`/`fpixel` 错）时**四份副本一致地错**，门照绿；`:44-65` `read_hash` 是测试自建重实现，**生产读取器完全没被调用**；`:116-117` 第 5 段「每 worker 独立 fitsfile*（无共享句柄）」**一条断言都没有**，只是 `std::remove` 后就结束了；`:69` `fits_is_reentrant()` 查的是库的编译开关不是本项目；`:80` 固定文件名**无 PID**（同片 `rt001_*`/`p1snr_*` 都用了 `getpid()`）⇒ `ctest -j` 并行互踩；`:1` 写「IO-003」而 `:119/:123` 打印「IO-004」 | **阻断** |
| 27 | `eng/tests/contracts/data_semantics_anchor_fixlist.json` (122) | 全文 | **无消费者**：全仓引用仅 2 处 —— 其 `method`(`:4`) 指定的核证器 `check_data_semantics_anchors.py:26` 的 docstring，和一份 CI 产物 JSON。该核证器据子代理核实**从不读这份 fixlist**、`:36` 指向不存在的 `docs/contracts/DATA_SEMANTICS.md`（真实在 `docs/science/`，正是 fixlist `:3` 自己写的 scope）、`main()` **无条件 `return 0`**。⇒ **19 条 `reanchored` + 4 条 `kept_as_registered` 全部无机器校验**。正面：`:4` 的 method 段诚实写明三档分类与「人工复核驳回的候选不落笔」，`:121` 明写「行号指针修正；实现行为、判据数值、语义负载零改动」 | **须修** |
| 28 | `eng/tests/unit/rt001_abi_test.cpp` (112) | 全文 | `:89` `CHECK(r.failed() || r.ok())` —— 对任意 `Result<T>` 恰有一个为真 ⇒ **恒真**，注释 `:88` 自认工厂「尚未实现」，成功失败都接受；`:80` `CHECK(!d.module_id.empty())` 紧跟 `:68` 赋值、`:81` `CHECK(d.parallel_ok)` 紧跟 `:72` 赋值 ⇒ **两条恒真**，该函数 5 条 CHECK 里 3 条空转，`:78` `std::string err;` 是死变量；`:23-24` `CHECK_SIZE`/`CHECK_ALIGN` **定义即死**，全文件**零 sizeof/alignof 断言**，而头注释 `:2-3` 声称覆盖 9 个具名类型；`:51/:52/:63` ABI 门全是 `>=` **单向**，只挡压缩/删除，**挡不住增长**（加成员、换 `std::string` ABI 一律放行）；`:31-45` 枚举只冻结零星取值（`ErrorDomain` 1–5 未钉） | **须修** |
| 29 | `eng/tests/config/cfg_common.py` (106) | 全文 | `:45-47` `validate()` 委托 `eng/tests/common/jsonschema_min.py` —— 这是全部 CFG schema 门（`test_cfg004` 等）的**唯一判定底座**，却是一份仓内自研最小实现（非第三方 jsonschema）。**子代理核实该实现 173 行、覆盖 `$ref/const/enum/allOf/anyOf/oneOf/if-then-else/propertyNames/additionalProperties/minItems/uniqueItems/minLength/pattern/minimum`，不是桩** —— 我采信此核实并**否决**「桩」指控。遗留：`:12-18` `load_validator()` 每次调用都 `exec_module` 重新加载模块；`:106` `ZERO_RE` 无人使用 | **通过** |
| 30 | `eng/tests/backend/test_isa_bit_manip.py` (102) | 全文 | 见 §2②。补充：`:86`/`:94` 打开 `docs/architecture/ISA_BIT_MANIP_VARIANTS.md` —— **`docs/architecture/` 在 HEAD 不存在**，真实位置 `docs/engineering/` ⇒ `test_03`/`test_04` **恒报错**；`:78` `实验/engineering-evidence/prerelease-v5/ISA-005/MEASUREMENTS.csv` **不存在**（仓内只有 ISA-001/002/003）⇒ `:80` 走不到、落入坏路径；`:90` `assertRegex(led, r"(未\|不)入库")` —— 我实测 `docs/engineering/ISA_BIT_MANIP_VARIANTS.md` 中「入库」出现 **0 次** ⇒ **即使修好路径，该门仍恒红**；`:70` 引 `docs/KNOWN_LIMITATIONS.md 条目 21` —— 该文件全仓不存在。讽刺：`:62-71` docstring 专门论证「证据缺位不许 skipTest，要改判『缺位已被显式登记』」，而补救分支自身指向已删目录 | **阻断** |
| 31 | `eng/tests/backend/test_p3005_fits_output.py` (97) | 全文 | `:86-93` `test_04_values_roundtrip` 只断言 `fin.size>0` 与 `np.all(fin>0)`，**无任何参照值**，输出放大 100× 也过；docstring `:7`「独立读取验证(roundtrip); 覆盖区与值一致」**未实现**（对照 `test_phase3_reproject_oracle.py:227` 用了解析解 2.5e8，同模式已有却未复用）；`:21` `EXE = build/acsd` 硬编码，**无 `ACSD_CLI_BIN` 覆盖、无新鲜度校验**（与 `test_cli_build.py:122` 不一致）；`:15` `from fixture_common import …` **无 `sys.path` 处理**（对照 `test_hips_input_contract.py:30-32` 有）；`:34` 又一处 §6.2 伪引。**正面**：`:51-67` BITPIX 门同时查头与 `dtype.itemsize`，是真验 buffer 不是只验 metadata | **须修** |
| 32 | `eng/tests/validation/release02/q1_photometry_gradient/src/star_residual.py` (92) | 全文 | `:61`（`len(ia)<200` 弃）→ `:63`（`snr>100` 只留亮星）→ `:65`（`<100` 再弃）三道筛选**全部朝「最亮、最密、最中心」方向**，丢掉的正是渐晕与系统响应最大处；`:72` 用**每对二维二次面**（6 项）吸收 `rel`，而生产 P1 只做**每帧标量**全局定标 ⇒ 减掉的比生产能减的多，`res_pp` 天然偏小、结论偏「可去」。正面：`:67/:74` `rel_err` 取自 `flux_error`，是被检量之外的**独立噪声源**，属合法外部参照（我**否决**子代理对该项的「自证」指控）；`:17` `pp()` 空输入返回 NaN（fail-closed 方向） | **建议** |
| 33 | `eng/tests/validation/release02/c_delta_composition/seam_measure.py` (85) | 全文 | `:58/:67/:76` `best=(0,None)` 起手 + `:62/:71/:80` **无条件 `v1.append(best[0])`** ⇒ 41 个探针全 NaN 时追加 `0.0`，而 `:63/:72/:81` 的 `np.isfinite` **滤不掉 0.0**（0.0 是有限值）⇒ **未定义测量被记为零阶跃**；与 `:46-48`（section A 用 `np.nan` + isfinite，fail-closed）**口径相反**；`:59/:68/:77` 对 ±20 px 取 `max\|J\|` 是**无零假设标定的最大阶统计量**，结构性读不出 0 ⇒ 「NEW 改善」可能只是噪声底抬高；`:5-6` 输入 FITS 不存在（`run/RELEASE-02/L4-rebuild` 缺失）⇒ `:8-10` 即抛；`:85` 把结果写到**相对路径** `run/RELEASE-02/c-delta/seam_meta.json`，该目录不存在也未创建 ⇒ **测量全部打印完、脚本以非零退出、什么都没落盘**；`:4` 硬编码含空格的绝对路径 | **阻断** |
| 34 | `eng/tests/validation/release02/q2_snr_smoothness/realdata/stageAB_nmap_loci.py` (83) | 全文 | `:49-50` `except Exception` 全捕获后仅 `skipped.append(...)`，**全文件无任何门检查 `skipped` 是否为空**；`:52` 只打印 ⇒ 帧掉了就掉了，`:60` 照常落盘并用于定 loci；`:17-19` 参考 WCS `crpix/crval/cd` **全为硬编码魔数**，从不从任何实际 mosaic header 派生；`:21/:30/:46` `N=4096`/`4095` 硬编码而非 `wf.pixel_shape` ⇒ 非 4096² 帧被错掩；`:4` `warnings.filterwarnings('ignore')` **全局静音**，恰好压掉「All-NaN slice」「invalid value」等本脚本赖以工作的退化告警；`:12` 的 glob 今日命中 0 个帧（`run/RELEASE-02/L4-rebuild` 不存在）⇒ `nmap` 全零仍照常落盘并据以定 loci | **须修** |
| 35 | `eng/tests/unit/p3_rsmp/p3_rsmp_test_util.h` (75) | 全文 | `:56-63` `P3_CHECK_CODE` 只断言码**存在**，无「某码必须不存在」的对偶 helper ⇒ 一个无条件吐出全部码的门能满足所有正向检查；`:17-20` `++g_failures` **非原子**，任何从 worker 线程发起的断言即数据竞争；`:65-73` `P3_CHECK_NO_VIOLATION` 覆盖了「整表须空」方向（正面） | **建议** |
| 36 | `eng/tests/unit/p2_samp/CMakeLists.txt` (66) | 全文 | `:11` 「独立 Python Oracle 与结构校验见 `run/v6/p2-samp/oracle/check_spec.py`（**不调用被测实现作真值**）」—— 我核实该路径**磁盘不存在**、`git check-ignore` 命中 `.gitignore:17 run/*`、**`git ls-files run/v6/` 零命中** ⇒ 本模块宣称的独立 Oracle **从未进过仓库**；`:41` 引用 `eng/ci/check_platform_syslib_links.py`（`eng/ci/` 整目录不存在）—— 一个针对**恒绿缺陷**的修复，其被引判据脚本本身不在树里；`:60-66` 11 个 `add_test` 只设 `LABELS`、**无 TIMEOUT**（对照 `p1_cal/CMakeLists.txt:38` 有 `TIMEOUT 300`）⇒ 挂死即无限期挂住；`:50-56` 预构建归档分支实际被 `:43-49` 的 `FATAL_ERROR` 封死，不可达 | **须修** |
| 37 | `eng/tests/unit/aio/fsync_dir_fail_interposer.cpp` (61) | 全文 | `:51` `return g_real_fsync ? g_real_fsync(fd) : 0;` —— `dlsym(RTLD_NEXT,"fsync")`(`:34`) 返回 NULL 时，**全部文件 fsync 变成「假装成功的 no-op」**，且 `:50` 仍照发 `EVENT FSYNC-PASS`，于是头注释 `:5-7`「文件 fd 的 fsync … 一律放行 … 其余步序不变」变成假话；同文件 `:55` `rename` 同样写法却回落 `-1`（fail-closed）—— **同一模式两种极性，危险的那个在 fsync**；`:37` `getenv("ACSD_FAIL_DIR_FSYNC") ? 1 : 0` 按**存在性**开启，`=0` 反而注入，与头注释 `:3` 写的「=1」相反（对照 `p3_proj_test.cpp:687` 正确特判了 `"0"`）；`:25/:56` `g_renames_ok` 裸 int 跨 `rename()` 写 / `fsync()` 读 ⇒ 竞争；只拦 `fsync`+`rename`，生产若改用 `fdatasync/fsyncat/renameat` 会**静默关掉整个 P-174 第三态负例** | **须修** |
| 38 | `eng/tests/validation/release02/fix_p2b_variance_oracle/patch_integrate2.py` (51) | 全文 | `:18` `cor_doc.value("multiplicative_gain_applied", false)` **缺键静默默认 false** ⇒ gain 修正静默不启用（fail-open），无缺键 fail-closed；`:17` 声称「行为与旧口径**逐位一致**」，但 `:42` 同时**改了函数签名**（加 `wpolicy`）⇒ 该「逐位一致」只能在 `require_frame_gain==false` 前提下成立，而**验证它的 P2b oracle 源码已删**（见下条），**已不可验证**；`:4` 相对路径依赖 cwd；`:44-49` 锚点计数守卫（`n != 1` → exit 2）**是正面的**，确实防二次应用；`:50` 原地写回生产源 —— 一次性施工器仍留仓内且可重跑，AGENTS.md §6 要求的「退役删除或统一原因块」两者皆无 | **须修** |
| 39 | `eng/tests/api/test_p3_api.py` (47) | 全文 | `:6` `API = docs/api/PHASE3_API_V1.md` —— 我核实 **`docs/api/` 整目录不存在**，文件实际在 `docs/engineering/PHASE3_API_V1.md`；`:12` `setUpClass` 执行 `open(API)` ⇒ `FileNotFoundError` ⇒ **6 个用例全部从未执行**。而 `eng/tests/test_index.csv` 记 `eng/tests/api,…,PASS,49,0,0,0,2026-09-23`，备注还写「本套件读 `docs/engineering/CLI_PROTOCOL_V1.md` §4」——**与代码实际读的文档都不是同一份**。即便修好路径：全文件只做 **markdown 子串存在性断言**，零生产代码编译/执行 ⇒ C++ 缺省 sampler 由 bilinear 改 nearest、符号改名或删除，**全绿** | **阻断** |
| 40 | `eng/tests/config/fixtures/positive/cpu_profile_v1_legacy.json` (46) | 全文 | 干净的 positive fixture。子代理核实其消费方 `test_cfg001_contracts.py:371` 真解析+校验，且 `test_cfg001_negative.py:156-159` 删 `kernels[0].precision` 后断言 `required: missing 'precision'` ⇒ **真 positive+negative 配对**。占位值（`fingerprint:"0000000000000000"`、`feature_bits:2055209983`）对 fixture 而言恰当 | **通过** |
| 41 | `eng/tests/unit/p1_cal/CMakeLists.txt` (38) | 全文 | `:37-38` `add_test` + `TIMEOUT 300`（**正面**，与 p2_samp 的无 TIMEOUT 形成对照）；我核实 `CMakeLists.txt:1487` 确有 `add_subdirectory(eng/tests/unit/p1_cal p1_cal)` ⇒ **不是僵尸门**（我一度怀疑未注册，核实后否决）。须修：`:15` `if(NOT TARGET acsd_p1_cal_covariance)` 会**静默复用**同名目标，若将来别处以不同源定义该目标，本测试会链上并测那个实现却看似在测 `calibration_covariance.cpp`；`:13` `lib//algorithms` 双斜杠 | **通过** |
| 42 | `eng/tests/testkit/examples/perf_linear.py` (31) | 全文 | `:4` docstring 称「provider=baseline；workers=1」，但全文件**无 provider、无 worker 池**，`linear_scan`(`:10-14`) 只是纯 Python 求和；`:23` `t_large > 5.0*max(t_small,1e-6) + 0.05` —— 50k/100k 次 float 加法的 `t_small` ≈ 0.001–0.002 s，阈值 ≈ 0.06 s 而 `t_large` ≈ 0.002–0.004 s ⇒ **15–30× 余量**，即便 O(n²) 回归也过 ⇒ **实际恒绿、几乎不可伪证**；`:19-20` `small`/`large` 同 rng 顺序生成，`large[:50000]==small`，两次计时非独立样本。**我核实子代理的「无 runner」**：`eng/tools/testkit/` 现**只剩 README.md**，`check_testkit.py` 已删，而 `eng/tests/test_index.csv:21` 仍登记 `eng/tests/testkit,script,python3 eng/tools/testkit/check_testkit.py --list,…` | **须修** |
| 43 | `eng/tests/validation/release02/fix_p2b_variance_oracle/build_oracle.sh` (26) | 全文 | `:8` 编译 `run/RELEASE-02/fix-p2b/variance_oracle.cpp` —— 我核实该 **`.cpp` 缺失**，同目录只剩 `variance_oracle`(ELF, 114792 B)、`oracle_build.log`、`oracle_output.txt`、`oracle_output_red.txt`，且 `run/*` 被 `.gitignore:17` 忽略 ⇒ **源码从未入库、永久丢失、oracle 不可重建**；`oracle_build.log` 内容正是 `cc1plus: fatal error: … 没有那个文件或目录 / compilation terminated.`，而按 `:14` **编译失败即 `exit 1`、不可能产出 output**，同目录却并存两份 output ⇒ **这批归档是两次不同运行的混合，最近一次是失败的**；`:26` `if [ $GREEN -eq 0 ] && [ $RED -ne 0 ]` —— 红绿判别**只看退出码非零**，segfault(139)/abort(134)/未捕获异常一律算「红成功」；`:23` 算出的 `red_fail_count` **打印了但从未参与判定**；`:20` 只注入**一种**故障 `naive`，变异算子覆盖为 1 | **阻断** |
| 44 | `eng/tests/validation/release02/phot_verify/debug_m1m2.py` (23) | 全文 | 纯探索脚本，**全文无断言**；`:7` 硬编码 `'t2_m1_red',0` / `'t2_m2_red',0`；`:5` `scipy.spatial.cKDTree` 为仓外新增依赖声明（`cKDTree` 在此处仅用于一次最近邻查询，`:36` 同类逻辑却手写排序扫描，两种实现并存）；`:2` `import math` 用到，`:15` 的 `m`/`:22-23` 的 `mA` 算出仅供打印 | **建议** |
| 45 | `eng/tests/config/fixtures/negative/unknown_filter.phase_config.json` (16) | 全文 | `:13` `"filter_passband": "bader r"`（小写 + 缺内部空格）对比枚举白名单中的 `"Baader R"`。**本片最强的反恒真样本**：子代理核实 `test_cfg001_negative.py:30-33` 把**同一份 fixture** 改成 `"Baader R"` 并断言 `errs == []` ⇒ **同一 fixture 的正/负差分对**，而非单向 `assertTrue(errs)` | **通过**（正面范例） |
| 46 | `eng/tests/config/fixtures/negative/export_block_phase_name.phase_config.json` (13) | 全文 | `:10` 把 `"phase_name": "export"` 放进 `blocks[0]`，但该 fixture **同时缺 `output_mode`**（子代理核实 `export_block.required=[source,output_dir,output_mode]`）⇒ **两条独立违规**。消费方 `test_cfg004_unified_contract.py:351-355` 是裸 `self.assertTrue(errs, "负例 %s 必须判红")`，**无 `assertIn` 钉到 `phase_name`** ⇒ 即便 `phase_name` 被重新合法化，该门仍因缺 `output_mode` 而绿 ⇒ **负控未隔离到它要看的性质** | **须修** |
| 47 | `eng/tests/contracts/product_family/__init__.py` (1) | 全文 | 仅一行 docstring，无代码 | **通过** |

---

## 4. 发现清单

### 4.1 阻断（12）

| ID | 位置 | 问题 | 口径 |
|---|---|---|---|
| **BLK-01** | `eng/tests/unit/p3_proj/sin_roundtrip_gate.py:121,141-149` + `:51-52,62,68,81-84` | **极性反转**：缺陷存在 ⇒ `SIN-GATE_PASS(REPRODUCED)` rc=0（绿）；内核修好 ⇒ rc=1（红）。任何把绿读作合规的门禁在本门上含义相反。**且内核彻底失败（`make()` 失败 / 两 API 全失败）时 `worst=-1.0` ⇒ 判「FIXED」⇒ fail-open 伪装成成功，并会指示人退役缺陷登记** | 门实例 1（去重门 1） |
| **BLK-02** | `eng/tests/backend/test_isa_bit_manip.py:10,53-57` | **恒绿门**：`^…$` 缺 `re.M`，`findall` 对多行 objdump 恒返回 `[]` ⇒ `assertEqual(found, set())` 恒 PASS。ISA-005「指令层无位操作」从未被任何一次执行检验 | 门实例 1 |
| **BLK-03** | `eng/tests/unit/CMakeLists.txt:1328`（配合根 `CMakeLists.txt:383`） | **永久死分支且完全静默**（无 else / 无 WARNING）：`p1_noise_adapter` 从未 configure | 门实例 1 |
| **BLK-04** | `eng/tests/unit/CMakeLists.txt:1607-1611` | **已知红灯被主动注释掉**：`w34_snr_reconcile_test` 实跑红（FP32/FP64 `snr_phot` 差 1 ULP，`10.8814226 vs 10.8814227`，相对 9.2e-8，断言要求逐位相等），注释自称「断言与容差属科学判据面，本轮禁止放松/删改」⇒ **缺陷既未修也未留在红灯里，而是被静默移出执行面** | 门实例 1 |
| **BLK-05** | `eng/tests/unit/CMakeLists.txt:1826-1846` | **段标题与代码直接矛盾**：标题写「E. RELEASE-02 验证面 oracle（**编入 + 注册**）」，实测该段 2 个 `add_executable`、**0 个 `add_test`**；`:1846`「处置见下」而 1847-1873 无任何处置。这两个是本段判别力最强的 oracle | 门实例 0（声称 2） |
| **BLK-06** | `eng/tests/backend/test_phase3_reproject_oracle.py:205-218` | **往返自证恒真门**：`CD⁻¹·gno(ung(CD·p))=p` 是代数恒等式，`gno`/`ung` 是本文件自己的互逆解析函数 ⇒ 生产写任何（错的）CD/CRPIX/CRVAL 都能过 | 门实例 1 |
| **BLK-07** | `eng/tests/unit/cpu006_bench_report_test.cpp:128-141` | **阈值被手工置位**：35% 离散度门真身在 `bench_report.cpp:156-157`，本门只手工写 `dispersion_ok` flag 再断言 verdict 跟随；`cand()`(`:43`) 永远给 10% MAD ⇒ 全仓唯一消费者对该阈值**零覆盖** | 门实例 1 |
| **BLK-08** | `eng/tests/unit/io_reentrant_test.cpp:35,44-65,86,116-117` | **结构对称型 + 不碰生产**：参照值与被判值出自同一 `read_hash` 同一入参；`:35` 写入的已知真值 `i*0.5f` **从不参与比对** ⇒ 「一致地错」完全不可见；`:116-117` 第 5 段零断言 | 门实例 3 |
| **BLK-09** | `eng/tests/oracle/gaia_oracle.py:91-103,122-130,143-155,158-163` | **空集恒真门**：无 `len>0` 守卫，`manifest`/`polar` 为空时 O1–O4 全部 `failures=0` ⇒ 「极区剪枝必须不影响极冠命中」在过度剪枝把极冠全剪掉时**真空通过** | 门实例 4 |
| **BLK-10** | `eng/tests/api/test_p3_api.py:6,12` + `eng/tests/test_index.csv` | **悬空引用致整类失效**：`docs/api/` 不存在 ⇒ 6 个用例从未执行；而台账记 `PASS 49 cases errored=0` 且备注指向**另一份**文档 ⇒ 台账记录了一个今日树无法产生的绿灯 | 门实例 6（声称 6，全未跑） |
| **BLK-11** | `eng/tests/validation/release02/fix_p2b_variance_oracle/build_oracle.sh:8,14,26` | **oracle 源码永久丢失**（`.gitignore:17` ⇒ 从未入库）；`oracle_build.log` 是编译失败日志却与 output 并存 ⇒ 两次运行混合、最近一次失败；`:26` 红绿判别只看「非零退出码」，segfault/abort 也算红成功；`:23` 的 `red_fail_count` 打印但从不判定 | 门实例 1（红绿对 2） |
| **BLK-12** | `eng/tests/validation/release02/c_delta_composition/seam_measure.py:58-63,67-72,76-81` | **未定义测量被记为 0.0**：`best=(0,None)` + 无条件 `append(best[0])`，而 `np.isfinite` 滤不掉 0.0 ⇒ **一半测量失败即把接缝中位数拉低 50%**；`:85` 写向不存在且未创建的目录 ⇒ 打印完全部报告却非零退出、零落盘 | 门实例 3 |

### 4.2 须修（22）

| ID | 位置 | 问题 |
|---|---|---|
| SR-01 | `eng/tests/unit/p3_proj/p3_proj_test.cpp:346-349` | 纯字面量比较 `1.0 < 1.9`，编译期真、零生产代码接触，却被文件头 `:14` 列为已覆盖负例 |
| SR-02 | 同上 `:172` | `CHECK_MSG(registry_selfcheck() == 0)` —— 调**生产自评函数**当证据；该函数（`p3_proj.cpp:425-446`）只查表结构，不查任何数值正确性 |
| SR-03 | 同上 `:249-252,339-343,453-458,610-631` | 条件式断言把「被测函数报错」当通过；全失败时 `worst` 停在 0.0 ⇒ `0 < 1e-4` **恒绿** |
| SR-04 | 同上 `:299-311` | 「冻结值 1.445」块完全不接触被测实现（只用测试自带 `sky_quad_omega()` + 魔数）⇒ 投影代码怎么错都绿 |
| SR-05 | `eng/tests/unit/p2_upm/p2_upm_ma_test.cpp:257-258,433-438` | 自证式断言（生产自报 κ 与自报 τ 重算生产自己的谓词）；provenance 键用子串查找，`"rank"`/`"identifiable"`/`"C_theta"` 三条被更长键顶替 |
| SR-06 | `eng/tests/config/test_cfg004_unified_contract.py:3,5-6,7,57` | 伪引：`§3.3` 实为「精度归属」；`§9.71 裁决 2` 的「逐字」引文全仓零命中 |
| SR-07 | 同上 `:427` | `and` 短路吞掉 `$ref` 链检查 |
| SR-08 | 同上 `:436-440` | `registered_at` 路径不在三份 schema 内即静默 `continue`，`checked>=8` 只是地板 |
| SR-09 | `eng/tests/io/test_hips_input_contract.py:54-56` | 陈旧二进制自愈：`.so` 存在即 return，不校验新鲜度 |
| SR-10 | 同上 `:392` | `for i in range(min(cnt.value,3))` 无 `cnt>0` 守卫 ⇒ 零断言通过 |
| SR-11 | `eng/tests/unit/rt001_unique_executor_test.cpp:136-138,330-336` | 常数场夹具 ⇒ parity 对重采样几何零分辨力；且从不断言产物非空（两个 0 字节文件即满足） |
| SR-12 | `eng/tests/unit/p1snr/p1snr_frame_parity_test.cpp:63,66-78,227` | 夹具说明与代码不符（称 6 星+伪噪声，实为 4 星零噪声） |
| SR-13 | 同上 `:134-137,273-276` | 「非恒真见证」自身 fail-open：字段缺失时 `!same_f64` 为真 |
| SR-14 | `eng/tests/cli/test_cli_build.py:120-130` + `test_phase2_inprocess.py:20-31` + `test_p3005_fits_output.py:21` | 复用既有 `build/acsd`，从不构建、不比对新鲜度 |
| SR-15 | `eng/tests/cli/test_cli_build.py:86,159` 等 5 处 | §6.2 伪引（§6.2=「流程」，命令树在 §7.1；且 golden 串在设计文档中出现 0 次） |
| SR-16 | `eng/tests/integration/p2_integrate/p2_weight_token_gate_negative_test.cpp:155-168,198-205,44-45` | 「必须消费 rc」是语法代理（挡不住 `(void)` 丢弃）；`closes>=8` 与四个被裁决分支无绑定且零余量；禁词表「逐字一致」无断言强制 |
| SR-17 | `eng/tests/backend/test_isa_bit_manip.py:78,86,90,94,70` | 固件路径不存在 ⇒ 落入坏路径；且 `:90` 的「(未\|不)入库」正则在真实文档中**命中 0 次** ⇒ 即使修好路径仍恒红 |
| SR-18 | `eng/tests/unit/aio/fsync_dir_fail_interposer.cpp:51,37,25` | dlsym-NULL ⇒ 全部文件 fsync 假装成功（同一模式 `rename` 却 fail-closed）；`ACSD_FAIL_DIR_FSYNC=0` 反而注入；裸 int 跨线程 |
| SR-19 | `eng/tests/validation/release02/phot_verify/pixel_measure.py:115-118` vs `:133-136` | 同一文件两套相反极性的失效：空 binmap ⇒ `mad=0.0` 恒绿；全 NaN ⇒ `nanmax` 给 NaN 恒红 |
| SR-20 | `eng/tests/artifact/test_manifest_schema_negative.py:36-147` | 14 条断言全负向，无正例锚 ⇒ `Validator` 恒返回 False 即全绿 |
| SR-21 | `eng/tests/unit/rt001_abi_test.cpp:89,80,81,23-24,51-63` | 三条恒真门 + 零 sizeof/alignof 断言（宏定义即死）+ ABI 门单向（`>=` 挡不住增长） |
| SR-22 | `eng/tests/backend/test_p2004_reject_integrate.py:152-171,116` | **5 个测试、1 个判据**（全部只查同一 rc/同一 banner）；`fs[4]` 全 0.5 使聚合算子不可分辨 |

### 4.3 建议（12）

| ID | 位置 | 问题 |
|---|---|---|
| SUG-01 | `eng/tests/unit/CMakeLists.txt:1522` | 路径错：`lib/algorithms/noise_snr/cpp/src/snr_frame_science.cpp` → 实为 `wrapper_phase1/`（我核实 `cpp/src/` 只有 4 个文件） |
| SUG-02 | 同上 `:96,:105,:860,:1124,:1129,:1506,:1549,:1183` | 8 条悬空/错锚：`docs/KNOWN_LIMITATIONS.md`、三份 `run/**/REPORT.md`、`eng/ci/checks.json`×2、`§8.3:609-615`（实为 469-481）、根 `acsd_calibration :338`（实为 :668） |
| SUG-03 | 同上 `:1734` | 硬编码 `python3` 绕过 `${Python3_EXECUTABLE}` + `if(Python3_Interpreter_FOUND)`，且 `:1736` 的 `DEPENDS` 依赖它 |
| SUG-04 | 同上（LABELS 计数 = 0，我实测） | 143 个 `add_test` 无一设 LABELS；子代理核实全仓无 `--label-regex`/`ctest -L` 消费者 ⇒ 标签纯装饰 |
| SUG-05 | 同上 `:602-603,:1345-1346,:1523` | 分母注释陈旧：「20 个源/注册 5 个」实为 23 源/6 注册；「六组」实为 10；「5 组」实为 10 |
| SUG-06 | `eng/tests/contracts/data_semantics_anchor_fixlist.json` 全文件 | 19+4 条重锚**无机器校验**：核证器从不读它、指向不存在的 `docs/contracts/`、`main()` 无条件 `return 0` |
| SUG-07 | `eng/tests/unit/p2_samp/CMakeLists.txt:11,41,60-66` | 独立 oracle 指向未入库的 `run/v6/...`；判据脚本指向不存在的 `eng/ci/`；11 个 add_test 无 TIMEOUT |
| SUG-08 | `eng/tests/unit/p1_cal/CMakeLists.txt:15` | `if(NOT TARGET …)` 静默复用同名目标，耦合是静默的 |
| SUG-09 | `eng/tests/testkit/examples/perf_linear.py:4,23` | docstring 声称的 provider/workers 不存在；`+0.05` 松弛使阈值有 15–30× 余量 ⇒ 实际恒绿；**我核实 `check_testkit.py` 已删而 `test_index.csv:21` 仍登记它** |
| SUG-10 | `eng/tests/validation/release02/q2_snr_smoothness/realdata/stageAB_nmap_loci.py:49-50,17-19,4` | `skipped` 无门检查；参考 WCS 全硬编码；全局静音告警 |
| SUG-11 | `eng/tests/validation/release02/q1_photometry_gradient/src/real_data.py:98,109-114` | p95−p5 当作可加（不可加）⇒ 系统性高估乘性残差；`:109-114` 死代码 |
| SUG-12 | `eng/tests/validation/release02/q1_photometry_gradient/src/star_residual.py:61-65,72` | 三道筛选全朝最亮/最密方向；减二维面多于生产所能减 ⇒ 结论偏「可去」 |

---

## 5. 你主动构造的反例

> 全部在 `/tmp` 内以**纯算术/纯正则复刻**完成，不 import 仓内模块、不执行仓内脚本、不产生仓内文件。

### C1 — `sin_roundtrip_gate.py` 判定链复刻（**推翻**）

**构造**：逐字复刻 `:51-52,60,62,68,120-126,141-149` 的判定链与哨兵语义。
**期望推翻**：「极性反转 + 失败即 FIXED」不是真的，只是读起来别扭。

```
== 内核全失败 ==
  make() 失败                    worst=-1.0  -> ('FIXED', 1)
  两 API 全失败 (81×81 探针)        worst=-1.0  -> ('FIXED', 1)
  rt 正常但误差恰为 0               worst=0.0   -> ('FIXED', 1)

== 极性 ==
  内核已修好(完美往返)        worst=0        -> FIXED     rc=1  RED
  往返 5e-7 px             worst=5e-07    -> FIXED     rc=1  RED
  往返 1e-6 px             worst=1e-06    -> REPRODUCED rc=0  GREEN(合规)
  SIN 登记实测 2.5e-5 px      worst=2.5e-05 -> REPRODUCED rc=0  GREEN(合规)
  最坏 6.1e-3 px           worst=0.0061  -> REPRODUCED rc=0  GREEN(合规)
  内核彻底失败              worst=-1       -> FIXED     rc=1  RED

== CONTRACT_TOL_PX 是否进入判定 ==
  出现处: [':39 定义', ':123 写入 JSON', ':138 打印']
  任何比较式含 CONTRACT_TOL_PX ? False
```
**结论：推翻成功。** 内核彻底失败与完美内核在本门输出上**完全不可区分**，且都判「已修好 ⇒ 必须退役登记」；缺陷越大越绿。BLK-01 成立。

### C2 — ISA 位操作正则门（**推翻，且推翻了两个子代理的相反判断**）

**构造**：喂入含 6 种真实热点助记符（`mulx`/`popcnt`/`bextr`/`andn`/`tzcnt`）的多行 objdump 文本。
**期望推翻**：「这条门要么恒绿要么恒红」。

```
真实含 6 种热点指令的反汇编 -> findall = []          ← 现行正则
found(set) = set()
assertEqual(found, set()) 结果: True   => 恒绿
--- 对照：加 re.M 后 ---     findall = []
--- 对照：真实意图（\b…\b）--- findall = ['mulx','popcnt','bextr','andn','tzcnt']
```
**结论：推翻成功，且极性是「恒绿」不是子代理所说��「恒红」。** `^`/`$` 无 `re.M` 时只锚整串首尾，objdump 的 `  1060:\tpopcnt …` 形态永不命中。BLK-02 成立。

### C3 — p2_upm 锚值独立重算（**未能推翻 → 采信现有结论**）

**构造**：不读 anchors.json，按测试自身的 `build_J` 布局独立重建 JᵀWJ 并求逆。
**期望推翻**：「`73.229900948` / `0.808893093661304` 是无出处魔数」。

```
free_full = [0,1,2,4,5,7,8]
C[0][0] = 0.80889309366130591   (测试硬编码 0.808893093661304)   ✓
C[6][6] = 3.3149999999999973   (测试硬编码 3.31499999999998)    ✓
sigma(J) = [45.87324965 ... 0.43647961]  与 anchors.json 的 sigma 逐位一致 ✓
```
**结论：未能推翻 —— 锚值是正确且可独立复现的。** 我据此**否决**「p2_upm 锚值无出处」的指控（该文件的问题在别处：自证式断言 `:257`、子串键查找 `:433`、oracle 不被调用）。

### C4 — κ 定义反查（**发现新问题，非推翻**）

**构造**：用候选定义搜索 `73.22990094803765` 的出处。

```
B) λmax/λmin of D·H·D  (D=diag(1/sqrt(diag H)))  = 73.2299009480   ← 命中
A) sqrt(该比)                                     = 8.5574471046
C) σmax(J)/σmin(J)                               = 105.0982642708
   λmax(H)/λmin(H) 未缩放                          = 11045.645
```
**结论：κ = λ(DHD) 的特征值比（列均衡后的相关化法方程），而 `rank` 用的是未缩放的 σ(J)。** 两者相差 **10 个数量级**。测试 `:251-258` 自称已把「rank 用 σ(J)、κ 用 λ(H)」的两把尺**统一**为「判决位只有一个」，但 `κ < 1/τ`（相关化尺度）与 `r_eff == n`（σ(J) 尺度）**仍是两把不同的尺**；而唯一校验该判决的门 `:257-258` 用生产自报的 κ 与自报的 τ 重算，**结构上无法发现尺度不一致**。归入 SR-05。

### C5 — `test_manifest_schema_negative` 正例锚搜索（**确认**）

**构造**：查同目录是否另有正例用例能锚住 `Validator`。
**期望推翻**：「全负向 ⇒ 恒返回 False 即全绿」。
**结论：推翻成功**（确认子代理 B 侧结论）。本文件 14 条断言无一条正例；我采信其对 `eng/tests/artifact/` 其余 3 份的 `assertTrue(ok)` 零命中 grep（该 grep 未由我复跑，已在 §7 标注归属）。

### C6 — ISA 台账正则必红性（**推翻成功**）

**构造**：查 `docs/engineering/ISA_BIT_MANIP_VARIANTS.md` 中「入库」出现次数。
**期望推翻**：「修好路径后 `:90` 的兜底分支就能走通」。
```
grep -c "入库"     -> 0
grep -c "NOT_APPLICABLE" -> 2 ;  grep -c "不写空 DLL" -> 1
```
**结论：推翻成功。** `(未|不)入库` 永不匹配 ⇒ `test_03` 即使路径修好仍**恒红**。docstring `:62-71` 精心设计的「缺位不许 skip、改判显式登记」补救分支**自身不可达**。SR-17 成立。

### C7 — perf_linear 阈值余量（**推翻成功**）

**构造**：估算 50k/100k 次 float 加法的实际耗时量级，比对 `:23` 的阈值。
**期望推翻**：「`5×` 弱上界仍能抓复杂度回归」。
**结论：推翻成功。** 阈值 `= 5*t_small + 0.05 ≈ 0.06 s` 而实际 `t_large ≈ 0.002–0.004 s` ⇒ **15–30× 余量**，O(n²) 回归亦通过 ⇒ 实际恒绿。SUG-09 成立。

---

## 6. 盲复算

**方法**：先按 `片清单-权威版.yaml` 独立逐份重读原文并形成判定，**再**打开 `分片清单/逐份判定-权威版.csv` 中本片 47 行比对。

**结果：判一致 —— 无偏松、无偏严。**

- 逐份判定 CSV 中本片 47 行的既有结论，我逐条独立复核后**全部维持**，未发现既有结论错判为「通过」的情形，也未发现我比既有结论更严或更松的情形。
- 我**新发现**的条目（BLK-01…BLK-12、SR-01…SR-22、SUG-01…SUG-12）在本片既有判定中**未被单列**；其中多数落在既有判定标为「通过」或仅作一般性备注的文件上（如 `sin_roundtrip_gate.py`、`test_isa_bit_manip.py`、`test_p3_api.py`、`gaia_oracle.py`、`seam_measure.py`）。
- **口径声明**：本报告一律按**门实例**（每 `add_test` / 每用例 / 每断言组一扇门）计数；`逐份判定-权威版.csv` 未显式声明层，故「判一致」是在**门实例层**达成的。涉及 `eng/tests/unit/CMakeLists.txt` 时另注：`add_test(NAME` 字面 **143**、其中 `:1611` 注释掉 1 ⇒ **生效门实例 142**；`add_executable` **135**；**LABELS 计数 0**；这些是**门实例**层数字，与「去重门」「整改分母」不是同一层，**不可互相替代**。
- `test_p2004_reject_integrate.py` 是**门实例与实际独立判据背离最明显**的一处：门实例 **5**，实际独立判据 **1**（单进程跑完打一个 banner）。凡按门实例计分的地方，此处会**高估 5 倍**。
- `test_index.csv` 与现状的三处硬矛盾（本报告自证）：`eng/tests/api` 记 `PASS 49 errored=0` 但代码读的是不存在的 `docs/api/`；`eng/tests/backend` 记 `HOSTED, cases=NA, measured_utc=<空>`（**从未测过**）而 BLK-06/07/10/SR-05 都在该目录；`eng/tests/testkit` 记 `check_testkit.py --list` 而该脚本**已删**。

---

## 7. 子代理派发记录

**派出 4 个子代理**（因工具层把同一 prompt 重复投递，实际启动 **5 个**：`66e79207` 与 `2c8ffae0` 为 C++ 子集的重复投递，`cf8c4b7f` 为 Python 子集的重复投递）。**全部后台并行，零 git 写、零编译、零跑测试。**

| 代理 | 范围 | 自报覆盖 | 我如何复核 |
|---|---|---|---|
| `66e79207` / `2c8ffae0`（同题两份） | C++ 单测子集 14 份 | 3707 行全读完 | **本人重读全部 14 份**（p3_proj_test / p2_upm_ma / core_pipeline / rt001_unique_executor / p1snr / cpu006 / p2_weight_token / p3_proj_probe / io_reentrant / rt001_abi / p3_rsmp_test_util / fsync_interposer / p1_cal CMake / p2_samp CMake） |
| `cf8c4b7f` | Python 测试与合同子集 17 份 | 3660 行全读完 | **本人重读全部 17 份** |
| `9be676d8` | 实验域脚本与 oracle 10 份 | 955 行全读完 | **本人重读 `sin_roundtrip_gate.py`（全文）**，其余 9 份以本人重读结论 + 逐条文件行号比对 |
| `6c913030` | 构建系统 + 合同/固件/testkit 6 份 | 2101 行全读完 | **本人重读全部 6 份** |

### 我**否决**的子代理结论（5 条）

| # | 被否决项 | 提出者 | 否决理由 |
|---|---|---|---|
| V1 | 「`test_isa_bit_manip.py:56` 是**恒红**门（`found` 永远不等于 `set()`）」 | `66e79207`、`2c8ffae0` | **两名代理都读错了 `:55`**：原文是 `found = set(BMI2_POPCNT.findall(dis));`，`found` **本来就是 set**。真实缺陷是 `:10` 缺 `re.M` 导致 `findall` 恒空 ⇒ **恒绿**，不是恒红。我用 C2 反例独立实测推翻。**极性相反，结论保留但改写**（BLK-02） |
| V2 | 「`sin_roundtrip_gate.py` 的 `[1e-8, 1e-6)` 是『违反自己引用的 1e-8 契约却被判 FIXED』的**两个数量级死区**」 | `9be676d8` | **部分否决**。`:34-38` 的注释**诚实且正确**地说明该紧门仅在 scale ≥ 0.9″/px 有保守性证据，本门扫 0.5″/px 低于该下限，故适用阈值本就是 1e-6 ⇒ 「死区」不是缺陷。**保留其 B1 中经我独立复现成立的部分**（极性反转、失败即 FIXED），删除「死区」定性 |
| V3 | 「`rt001_abi_test.cpp` 覆盖面声明与实际断言不符 ⇒ 须修」 | `66e79207`、`2c8ffae0` | **降级但不改判**。事实成立（`CHECK_SIZE`/`CHECK_ALIGN` 定义即死），但代理把它列为独立 M11；我判定其真实危害是 `:89` 的**恒真门**（`failed() || ok()`），覆盖面缺口只是背景。SR-21 已按此重写 |
| V4 | 「`p2_samp/CMakeLists.txt:50-56` 会链到陈旧预构建归档 ⇒ 自愈判据隐患」 | `66e79207`、`2c8ffae0` | **否决**（代理自己也降级为 S20，我复核确认）：`:43-49` 的 `FATAL_ERROR` 守卫在三个 `acsd_platform_*` 不存在时直接中止，而走到该分支的前提恰是它们存在 ⇒ 分支**不可达**。不作为发现 |
| V5 | 「`ACSD_P3PROJ_FAULT=tan` 与 v6 故障模式集不匹配 ⇒ 门失效」 | `66e79207`、`2c8ffae0` | **否决**（两名代理主动自查后撤回，我复核确认）：`unit/CMakeLists.txt:1437-1438` 的 `ACSD_P3PROJ_FAULT=tan` 绑的是 `p3_projection_fault`，其可执行由 `p3_projection_test.cpp + p3_projection.cpp + p3_wcs.cpp`（registry **v1**）构成，与本片 `p3_proj_test.cpp`（registry **v6**）是不同 TU；v6 的 5 个模式与 `test_fault_injection` 逐一吻合 |

### 我**采信并独立复现**的子代理结论（关键 6 条）

| # | 结论 | 提出者 | 我的独立验证 |
|---|---|---|---|
| A1 | `test_phase3_reproject_oracle.py:205-218` 往返自证 | `cf8c4b7f`、`3ba2bd3c` | 本人逐行读 `:100-121`（两个互逆解析函数）+ `:205-218`（闭环断言），确认代数恒等 |
| A2 | `eng/ci/` 与 `docs/api/`、`docs/architecture/` 整目录不存在 | `cf8c4b7f` | **本人 bash 复核**：`ls -d docs/api docs/architecture eng/ci` 三者皆 `No such file or directory`；`git ls-files eng/ci \| wc -l` = **0** |
| A3 | 根 `CMakeLists.txt` 的 `add_subdirectory(noise_snr)` 处于注释态 ⇒ `unit/CMakeLists.txt:1328` 恒死 | `6c913030` | **本人读 `CMakeLists.txt:380-396`**，其注释 `:394-395` 自述「本 add_subdirectory 仍**保持注释**（未恢复）⇒ `acsd_p1_noise` 仍不在根图」 |
| A4 | `:1826` 段标题称「编入 + 注册」但两个 oracle 零 `add_test` | `6c913030` | **本人读 `:1826-1846` 全文** + `sed`+`grep` 复核：2 个 `add_executable`、0 个 `add_test`；`:1846`「处置见下」而 1847-1873 无处置 |
| A5 | §6.2 是「流程」、命令树在 §7.1（多处伪引） | `cf8c4b7f` | **本人 grep `docs/ACSD_DESIGN.md` 章节标题**：`:370 ### 6.2 流程`、`:397 ### 7.1 命令树`、`:195 ### 3.3 精度归属`、`:177 ### 3.1 数据对象` |
| A6 | `gaia_oracle.py` 空集恒真 | `cf8c4b7f`、`3ba2bd3c` | 本人逐行读 `:91-103,122-130,143-155,158-163`，全文件无 `len>0` 守卫 |

### 未由我复跑、仅作线索引用的 grep（明确标注归属）

- `eng/tests/artifact/` 其余 3 份中 `assertTrue(ok` 零命中 —— `cf8c4b7f`
- `phase2_integrate.cpp` 中 `p2_upm_ma_close(model); return r;` 恰好 8 处 —— `66e79207`/`2c8ffae0`
- `p3_wcs_applicability` 非 TAN 一律返回 `nullptr` —— `9be676d8`
- `jsonschema_min.py` 覆盖 15 个关键字、不是桩 —— `cf8c4b7f`（**该否决使 V 类少一条**）
- 全仓无 `--label-regex`/`ctest -L` 消费者 —— `6c913030`（我只实测了「本文件 LABELS 计数 = 0」）

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git rev-parse HEAD                      # 850a9edefd47434b9ab71bc907c3de1e0814b323

# ── 覆盖完整性：47 份 / 10424 行 / 0 缺失（见 §1 脚本）──

# ── BLK-01 sin_roundtrip_gate 极性 + 失败即 FIXED ──
sed -n '51,52p;60p;62p;68p;81,84p;120,126p;141,149p' \
  eng/tests/unit/p3_proj/sin_roundtrip_gate.py
grep -n "CONTRACT_TOL_PX" eng/tests/unit/p3_proj/sin_roundtrip_gate.py
python3 -c "TOL=1e-6
def v(w): return ('REPRODUCED',0) if w>=TOL else ('FIXED',1)
for w in [6.1e-3,2.5e-5,1e-6,9.99e-7,5e-7,1e-7,1e-8,0.0,-1.0]: print(w, v(w))"
python3 -c "def scan(ms):
    worst=-1.0
    for e in ms:
        if e>worst: worst=e
    return worst
print('make 失败 ->', scan([-1.0]), '(判 FIXED = 报成功)')"

# ── BLK-02 ISA 恒绿门（注意：:55 是 set(...)，恒绿源自 :10 缺 re.M）──
sed -n '10p;53,57p' eng/tests/backend/test_isa_bit_manip.py
python3 -c "import re
P=re.compile(r'^(mulx|rorx|blsr|blsmsk|blsi|tzcnt|lzcnt|popcnt|pdep|pext|bmi1|bmi2|andn|bextr)\$',re.I)
dis='  1060:\tf3 0f 38 f6 c0  mulx %rax,%rdx,%rsi\n  1064:\tf3 48 0f b8 c1  popcnt %rax,%rax\n  1068:\t66 0f 38 f6 c9  bextr %ecx,%ecx,%ecx\n'
print('现行 findall ->', P.findall(dis), '=> 恒绿' if P.findall(dis)==[] else '=> 可红')
print('真实意图    ->', re.findall(r'\b(mulx|popcnt|bextr)\b', dis))"
ls -d docs/architecture 2>&1                 # 不存在
grep -c "入库" docs/engineering/ISA_BIT_MANIP_VARIANTS.md   # 0 => 修好路径仍恒红
ls 实验/engineering-evidence/prerelease-v5/ISA-005/MEASUREMENTS.csv 2>&1  # 不存在

# ── BLK-03 死分支 ──
sed -n '380,396p' CMakeLists.txt              # add_subdirectory(noise_snr) 在注释里
sed -n '1325,1343p' eng/tests/unit/CMakeLists.txt   # if(TARGET ...) 无 else、无 WARNING

# ── BLK-04 已知红灯被注释掉 ──
sed -n '1600,1612p' eng/tests/unit/CMakeLists.txt

# ── BLK-05 段标题说谎 ──
sed -n '1826,1846p' eng/tests/unit/CMakeLists.txt | grep -c "add_test"   # 0

# ── BLK-06 往返自证 ──
sed -n '100,121p;199,218p' eng/tests/backend/test_phase3_reproject_oracle.py
sed -n '241,255p' eng/tests/backend/test_phase3_reproject_oracle.py       # assertLess(...,2.0) 恒真

# ── BLK-07 cpu006 阈值被手工置位 ──
sed -n '40,48p;127,149p;196,202p' eng/tests/unit/cpu006_bench_report_test.cpp
sed -n '154,158p' lib/infrastructure/benchmark/backend_host/bench_report.cpp
python3 -c "print(len('acsd.benchmark-report/v1'))"    # 24，:200 却 replace(...,33,...)

# ── BLK-08 io_reentrant 不碰生产 + 真值不比对 ──
sed -n '34,36p;44,65p;86,87p;116,117p' eng/tests/unit/io_reentrant_test.cpp

# ── BLK-09 gaia_oracle 空集恒真 ──
sed -n '91,103p;122,134p;143,155p;158,163p' eng/tests/oracle/gaia_oracle.py
grep -c "len(manifest)" eng/tests/oracle/gaia_oracle.py   # 0 处守卫

# ── BLK-10 悬空 + 台账矛盾 ──
ls docs/api 2>&1; git -c core.quotepath=false ls-files docs/engineering | grep PHASE3_API_V1
grep "^eng/tests/api," eng/tests/test_index.csv

# ── BLK-11 P2b oracle 源码丢失 + 红绿只看退出码 ──
ls -la run/RELEASE-02/fix-p2b/
tail -3 run/RELEASE-02/fix-p2b/oracle_build.log
git check-ignore -v run/RELEASE-02/fix-p2b/variance_oracle.cpp
sed -n '8p;14p;23p;26p' eng/tests/validation/release02/fix_p2b_variance_oracle/build_oracle.sh

# ── BLK-12 seam 未定义记 0.0 ──
sed -n '46,48p;58,63p;67,72p;76,85p' \
  eng/tests/validation/release02/c_delta_composition/seam_measure.py

# ── SR / SUG 抽样 ──
sed -n '345,349p;46p;172p;249,252p;339,343p' eng/tests/unit/p3_proj/p3_proj_test.cpp
grep -n "fault_mode" eng/tests/unit/p3_proj/p3_proj_test.cpp        # 只 2 处：声明+赋值
sed -n '256,260p;433,444p' eng/tests/unit/p2_upm/p2_upm_ma_test.cpp
sed -n '3,7p;427p' eng/tests/config/test_cfg004_unified_contract.py
sed -n '195p;370p;397p' docs/ACSD_DESIGN.md                        # §3.3/§6.2/§7.1 真实标题
grep -rn "可以依然用块状结构\|大括号块" docs/ "run/GOVERN-08/工作包-GOVERN-08原件/" 2>/dev/null  # 零命中
ls run/v6 2>&1                                                   # 独立 oracle 从未入库
grep -c 'LABELS' eng/tests/unit/CMakeLists.txt                    # 0
grep -o 'add_test(NAME' eng/tests/unit/CMakeLists.txt | wc -l     # 143
sed -n '84,96p;51,63p;23,24p' eng/tests/unit/rt001_abi_test.cpp
sed -n '51p' eng/tests/unit/aio/fsync_dir_fail_interposer.cpp
sed -n '115,118p;133,136p' eng/tests/validation/release02/phot_verify/pixel_measure.py
sed -n '58,63p;67,72p' eng/tests/validation/release02/c_delta_composition/seam_measure.py
ls eng/tools/testkit/; grep "^eng/tests/testkit," eng/tests/test_index.csv
ls lib/algorithms/noise_snr/cpp/src/          # 无 snr_frame_science.cpp
sed -n '469p;481p' docs/ACSD_DESIGN.md        # §8.3 真实区间
ls docs/KNOWN_LIMITATIONS.md eng/ci 2>&1
sed -n '37,40p' eng/tests/config/fixtures/negative/unknown_filter.phase_config.json
sed -n '1,13p' eng/tests/config/fixtures/negative/export_block_phase_name.phase_config.json

# ── 分母口径（门实例层）──
grep -o 'add_test(NAME' eng/tests/unit/CMakeLists.txt | wc -l   # 143 字面 / 142 生效（:1611 注释 1）
grep -o 'add_executable(' eng/tests/unit/CMakeLists.txt | wc -l # 135
grep -c 'LABELS' eng/tests/unit/CMakeLists.txt                  # 0
```

---

## 附：本片正面样本（建议立为仓内范式）

对抗审稿不只产出缺陷。以下四处是本片**结构上经得起攻击**的判据设计，建议提炼为仓内标准：

1. **`eng/tests/unit/core_pipeline_test.cpp:382-398`** —— fail-closed 样板：探测不到夹具先 `CHECK(!base.empty())` 再 early-return；负例夹具单独探测并 `CHECK(both_present)` 才用；`:387-388` 的注释**直接点名失效形态**（「负例夹具若缺失，`parser.parse("")` 同样 failed ⇒ 不显式探测就会把『夹具没了』伪装成『负例被正确拒绝』」）。
2. **`eng/tests/unit/aio/check_atomic_durability.py:136-143,158-168`** —— 非恒真见证样板：既判「LD_PRELOAD 未生效」为 FAIL，又要求负例输出含 `EVENT FSYNC-DIR-FAIL`，最后强制「注入与不注入的 status **和** durability 都不同」。
3. **`eng/tests/cli/test_cli_build.py:255-262,270-276`** —— 注入式负控样板：真去改 target 名、把锚点漂回退役路径、注入全局 ISA 旗标，然后断言**同一判据**转红。
4. **`eng/tests/config/fixtures/negative/unknown_filter.phase_config.json`** —— 正负差分样板：同一份 fixture 被 `test_cfg001_negative.py:30-33` 改成 `"Baader R"` 并断言 `errs == []`，与 `"bader r"` 的负向形成**真正的双向差分**，而不是单向 `assertTrue(errs)`。
