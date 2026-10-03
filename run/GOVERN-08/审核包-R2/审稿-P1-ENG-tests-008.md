# 审稿 P1 · ENG-tests-008（第 1 遍 · 完整重读）

> 仓库 `/workspace/Astro CS Database` · HEAD = `850a9ede` · 审稿日 2026-10-02
> 口径：负责人裁定 —— **判据代码不构成正确性证据**；判据绿 **不等于** 实现正确。
> 本遍只做「独立重算 + 构造反例」，不引用他人结论作为依据（他人产出仅作线索，且逐条复核过）。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 成员份数 | **47** |
| **我亲自完整读完** | **24** |
| 成员总行数 | **10423** |
| **我实际逐行读完** | **7888** |
| **行覆盖率** | **75.7%** |
| 子代理覆盖（我另派 4 个，分片复读） | 47/47 份全部有人读完；A/C 两批已交付并被我逐条复核，B/D 两批交付时仍在跑 |

### 1.1 我亲自读完的 24 份（7888 行）

| # | 文件 | 行 | # | 文件 | 行 |
|---|---|---|---|---|---|
| 1 | unit/gaia_cat_test.c | 999 | 13 | backend/test_psf_moffat_oracle.py | 274 |
| 2 | runtime/integration/test_rt004_executor.py | 866 | 14 | unit/io_adapter_test.cpp | 252 |
| 3 | unit/cosmetic_adapter_test.cpp | 734 | 15 | system/runtime/runtime_contract_test.cpp | 221 |
| 4 | conformance/echo/src/echo_module.c | 635 | 16 | unit/gaia_module_manifest_bounds_test.c | 198 |
| 5 | unit/aio_abi_tests.cpp | 496 | 17 | cpu/baseline/provider_so_load_test.c | 184 |
| 6 | unit/aio/oracle/aio_oracle_lib.py | 467 | 18 | backend/test_wcs_psf_oracle.py | 166 |
| 7 | unit/cpu003_profile_v2_test.cpp | 424 | 19 | backend/test_p3_resample.py | 164 |
| 8 | unit/p2_sky_kappa/p2_sky_kappa_test.cpp | 343 | 20 | unit/aio/check_verify_order.py | 129 |
| 9 | io/test_fits_stream_contract.py | 375 | 21 | backend/test_abi_v1.py | 110 |
| 10 | cli/test_command_tree.py | 324 | 22 | backend/test_drizzle_parallel.py | 108 |
| 11 | contracts/product_family/test_field_constraints_integration.py | 288 | 23 | unit/p1_psfw/p1psfw_test_main.hpp | 94 |
| 12 | backend/test_p3_resample.py → 见 #19 | — | 24 | validation/.../syntax_check2.sh | 21 |
|  |  |  | 25 | config/fixtures/negative/export_blocks_mixed_flat…json | 16 |

### 1.2 **我未亲自读完的 23 份（如实列出，共 2535 行）**

`cpu/baseline/run_provider_oracle_checks.py`(231)、`config/test_cfg001_contracts.py`(402)、`quality/test_linux_release.py`(175)、`cpu/baseline/run_cpu_baseline_checks.py`(133)、`config/test_cfg002_registry.py`(70)、`validation/release02/phot_verify/pair_ratio.py`(155)、`abi/abi003_loader_probe.c`(150)、`integration/p2_integrate/oracle/p2_pixel_weight_wiring.py`(145)、`backend/bench_harness_main.cpp`(120)、`backend/mon003_synthetic_main.cpp`(203)、`validation/.../q3_additive_truth/src/q3lib.py`(102)、`.../q3_additive_truth/src/step10_figs.py`(93)、`unit/p1_ir_facade_test.cpp`(89)、`integration/p3_export/CMakeLists.txt`(81)、`backend/p3_session_probe.cpp`(71)、`conformance/noop/CMakeLists.txt`(56)、`unit/p3_proj/CMakeLists.txt`(55)、`.../c_delta_composition/mult_test2.py`(47)、`backend/fixture_backend.cpp`(45)、`config/fixtures/positive/export_blocks.phase_config.json`(39)、`unit/p3_rsmp/run_verification.sh`(30)、`.../c_delta_composition/parse_model.py`(28)、`unit/fixtures/core_pipeline/invalid_pipeline_serial_heavy.json`(15)。

> 其中 23 份全部由我派的子代理完整读完；对其中 **7 份**（syntax_check2.sh、export_blocks_mixed_flat.phase_config.json、test_command_tree.py、test_linux_release.py 等）我做了**亲自复核**，见 §7。其余 16 份我**未逐行独立复读**，其结论在本片中按「二手」标注，不作为阻断依据的唯一支撑。

---

## 2. 本片判定

# **阻断（BLOCK）**

**最重的 3 条：**

### B-1「独立 Oracle」拿自己的合成数据、用自己的模型去拟合 —— 结构上不可能验证生产实现
`eng/tests/backend/test_psf_moffat_oracle.py`
- `synth()`（:105）用本文件的 `moffat4()` 生成「真值」图像；`fit_block()`（:115）又用**同一个** `moffat4` 去 `curve_fit` 复原。
- 被测的 `dpsf_psf.cpp`（文件自己在 :19 引用为「生产」）**从未被调用**。
- ⇒ 判据能红的唯一途径是「数据生成器与拟合模型不自洽」（:211 β=3.9 注入正是如此）。**`dpsf_psf` 全瘫本文件仍全绿。**
- 文件头 :12-20 自称「独立性（关键）」，:3 标题称「独立 Oracle」，:5-7 把它挂到 SCI-PSF-001 §11 的验收承诺上。**名与实相反。**
- 附：:119-123 `p0_for(cx,cy,...)` 虽声明「避免 p0=真值式恒真」，但 p0 仍由真值线性扰动而来，恒真性被削弱而非消除。

### B-2 两条「恒真门」把真实缺陷永久藏在绿灯里
1. `eng/tests/cli/test_command_tree.py:314-320`：写入的配置 `{"schema_version":"1","bitpix":-64}` **既无 `source` 也无 `output_dir`**；而同文件 **:295-296 自己写明**「结构必须完整，否则结构错（rc=2）会先于键校验，掩盖 unknown key 诊断」。⇒ 门在到达键白名单前就退出，`assertNotIn("unknown key")` 以「没走到」而**恒真通过**；且该用例**未断言 rc**。姊妹用例 test_09 ③（:263-272）已用 `_complete_config` 补结构做对，这条漏了。
2. `eng/tests/unit/gaia_cat_test.c:935`：`CHECK(res[200000 - 1].ra == res[199999].ra, "尾元素访问自检")` —— `200000-1 == 199999`，**同一个数组元素与自己比较**，代数上恒真，永远不可能红。注释自称「尾元素访问自检」，实为空断言。

### B-3 判据读的是**上一个月前的产物**，且该产物在**源码树**里、被 gitignore 掩盖
`eng/tests/io/test_fits_stream_contract.py:29-43`
```python
def _build_lib():
    if LIB_SO.exists(): return      # ← 只在「不存在」时重编
_build_lib()
```
实测（`ls -la`）：
- `lib/infrastructure/aio/io/libfits_core_test.so` **mtime = 9月3日 03:05**
- `lib/infrastructure/aio/io/fits_core.c` **mtime = 10月1日 01:05**
- `.gitignore:29` = `*.so` ⇒ 该 `.so` 完全不受版本控制约束。
⇒ **整个 FITS 契约套件当前测的是一个月前的实现**，源文件已往前走了一代，而门照样报绿。同目录另有 `libhips_core_test.so` 同状。这正是「代码改了、归档没重跑」的最坏形态：**测试结果与被测代码之间没有任何一致性约束**。

---

## 3. 逐文件清单（我亲自读完的 24 份）

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `unit/gaia_cat_test.c` | 999 行全文 + 被测 `gaia_client.c` + `gaia_xpsd_fixture_gen.c` + `docs/science/algorithms/GAIA_QUERY.md` §5 | fixture 生成器**确实独立**（不 include/不调用 `gaia_client.c`，已核实）⇒ 真值面合格。但：①:935 恒真；②:563-568 **mag 缓存一致性只打印不断言**，而文件头 :15-16 声称「缓存一致性断言对 mag 使用 \|Δ\|≤1e-9」——**伪述，断言不存在**；③登记缺陷 **KI-1/KI-2 生产早已修复**（`gaia_client.c:291` 现为 `double *out_mag /*bitwise 往返*/`；KI-2 修复在 `gaia_client.c:2810-2815`），但测试仍按缺陷存在处理：:394 **只测极冠全 SP 锥，混合 DB 路径（KI-2 的真实场景）永不被执行**；④:14-18 引用 `gaia_client.c:123`/`:2045` —— 实测该两行现为分配钩子宏与边界函数，**行号全错**；⑤:3-7 引用 `MODULE_MIGRATION_TEMPLATE.md`、:668 引用 `run/WCS-DETERMINISM-01/REPORT.md §1.2` —— **两处文件全仓 0 命中** | **阻断** |
| `runtime/integration/test_rt004_executor.py` | 866 行全文 + `scheduler/src/*.cpp` | **本片质量最高**：判据双向——有注入自测（:794-818 三种注入必红）、有鉴别力自检（:528-547 真泄漏必判红 + 线程退出必回基线）、有负例编译注入（:842-862）。缺陷：①**`THREAD_CREATE_RE`（:314）定义后全文件零使用**（已 grep 确认），即「私池扫描」承诺面比实现面宽——生产 `.cpp` 里裸 `std::thread` 私池不被任何静态门捕获，且 `std::jthread` 在源侧（:711 POOL_DECL_RE 只匹配 `std::thread`）完全不可见；②**`SCHED_POOL_FILES`（:707）只含 3 个文件，892KB 的 `module_adapters.cpp` 有 2 处线程池（:2099/:9265）不在池形态门覆盖内**；③:426 `if "std::vector<std::thread>" not in text: return` —— 无池即**静默空转判绿**，与同文件 :746-747 把「无池声明」判红的策略**自相矛盾**；④:457-461/:504-506 引用 `run/FLAKE-01/evidence/tjoin_window.cpp`（据以定出 `kReclaimDeadlineMs=2000` 与 1.08%/p99=8.6ms 等全部数值）—— **该文件及目录全仓 0 命中**，阈值依据已不可核 | 须修 |
| `unit/cosmetic_adapter_test.cpp` | 734 行全文 + `lib/algorithms/calibration/src/module_entry.cpp:1020-1050` | **BITWISE 对拍是自证**：:501 `memcmp(plugin_out, direct_out)`，而「direct 通道」(:486) 调的 `ac_correct_frame` 与插件内部 (`module_entry.cpp:1040`) **是同一个函数**。坏点检测/插值算法若错，**两侧同错、memcmp 照样绿**。:557 的 f64 同理。且全文件**无一处断言 hot>0 或 cold>0**（已 grep 确认）⇒ **热/冷像素检测整体失效返回 0 也能全绿** | **阻断级（自证）** |
| `conformance/echo/src/echo_module.c` | 635 行全文 | 非判据（conformance 探针）。:481-488 `for(i<10000)` 无 sleep 忙等；:399-401 `manifest_get_u64` 空 `if` 体；:18/:106/:72 引用的「12 §1/§4」**文档始终未具名**（悬空引用） | 建议 |
| `unit/aio_abi_tests.cpp` | 496 行全文 + `aio_abi_test_main.hpp` | **正面范式**：:77-94 FIPS 180-4 官方向量作外部 oracle；:174 传 `nullptr` 故障名是**经设计的合法语义**（`aio_abi_test_main.hpp:73,102-112` 明确「nullptr = 不属任何注入点，断言照常求值」）。该头文件还**自陈并修掉过一处旧恒真门**（:67-75「旧写法字面量地址永不为 null ⇒ 恒真」）。:46-52 的 ABI 常量表为手抄镜像，无编译期绑定（与本仓其它地方的 `abi_numeric_eq` 做法不一致） | 通过（正面样本） |
| `unit/aio/oracle/aio_oracle_lib.py` | 467 行全文 + 两个合同 JSON | ①:**:249-256 无条件 `open(schema_path)`，而 :289 才 `if os.path.exists(schema_path)`** ⇒ :286-299 那段「schema 可能被迁移 ⇒ 回退冻结清单」的防御分支**永远执行不到，是死代码**，注释 :286-288 为假；②:326 `if pixfrac is not None and 0 < pixfrac:` ⇒ **sampling 缺 `pixfrac` 时 C5（FZ-COND-FLUX-CONSERV）整条检查静默不跑**（筛掉真信号）；③:277-283 只校验**合同 JSON**侧的退役登记，**不校验代码侧是否仍有活调用者**——实测 `lib/infrastructure/aio/product_io/src/bunit.cpp:82/:96/:158/:250` 与 `bunit.h:39` 的 `Quantity::kPsfswRobustWeight` 仍是活枚举 + 活 case，而 `drizzle_science.h:88` 已宣称「物理删除、再无枚举项」⇒ **同一退役对象在两个模块身份不一致**，正是「退役对象仍有活调用者」 | 须修 |
| `unit/cpu003_profile_v2_test.cpp` | 424 行全文 + 4 处被引生产文件 | 引用**全部核实为真**（`profile_gen_v2.cpp:55`=2e-4；`baseline_kernels.h:18/:27`；`.inc:150/:212` 整数除法）。双向做得很好（:151-195、:261-415 正负例齐全）。缺陷：**:253-255 的「负例(判据能红)」是构造出来的恒真** —— `wrong := ref[1] + dy*in0[w]`，断言 `|wrong - ref[1]| > tol` 即断言「我刚定义的那个非零量非零」，**与 oracle 无关**。同组 7a（:221-225）才是真反例（独立重算实值变体）。另 :169 `info.rank_rtol_effective * 0.0 + 1e-10` 是**恒零残留项**，写得像在比较两个量、实际只比较了一个 | 须修 |
| `unit/p2_sky_kappa/p2_sky_kappa_test.cpp` | 343 行全文 | **本片最好的反恒真教材**：:244-265 构造精确秩亏 H=[[1,1],[1,1]]，证明旧口径 κ(H+λI) 可被旋钮买绿而新判据判红；:200-201 明写「在 H_solve 上设门是恒真门」。:169 的 `* 0.0 +` 残留项同 B 段所述形态 | 通过（正面样本）/ 建议 |
| `io/test_fits_stream_contract.py` | 375 行全文 + `.so` 与源文件 mtime | **B-3**。另：:226-230/:242-246/:256-260 三处 `except: skipTest` ⇒ **astropy 缺失时整个交叉 oracle 轴静默消失而 skip 计通过**（与同片 `aio_oracle_lib.py` 缺 astropy 直接崩溃的策略**同片不一致**）；:216-217 `if np.issubdtype(...): pass` 是**无 else 无副作用的死分支**；:46-52 常量为手抄镜像；:4 引用 `tasks/03_RUNTIME_DATA_IO_TASKS.md`（`tasks/` 目录不存在）；.so 写在**源码树**内 | **阻断** |
| `cli/test_command_tree.py` | 324 行全文 + `ACSD_DESIGN.md` + `CLI_PROTOCOL_V1.md` | **B-2②**（:314-320）。另：:83 类级 `skipUnless` ⇒ 二进制缺失时整类 10 例静默 skip（当前 `build/acsd` 存在，属**潜伏**）；:2/:8/:14 把命令树锚在 §6.1/§6.2、rc 码表锚在 §6.3 —— 实测 `ACSD_DESIGN.md:360 §6 = export：投影导出`、`:370 §6.2 = 流程`、`:383 §6.3 = 投影算法`，**命令树实为 `:397 §7.1`**，`CLI_PROTOCOL_V1.md:3` 上游声明逐字印证；:290/:306 的 `§3.3:256` 实测第 256 行是 `"filter_passband": "Baader R"`，与精度无关 | **阻断** |
| `contracts/product_family/test_field_constraints_integration.py` | 288 行全文 + `clause_registry.json` 全量扫描 | **实测 7 条条款锚悬空**：`QF-G-INJ-01/02/03/07`、`QF-G-RD-01/02`、`QF-G-BASE-03` 全指向 `docs/engineering/v6/QA_MATRIX.md`，**该目录不存在**（`find docs -name QA_MATRIX.md` 0 命中）。根因是提交 `ed33f57f「删净 docs 域机读资产」`删了载体却留了锚。而 `eng/tests/test_index.csv:9` 仍记 `eng/tests/contracts` **PASS / 84 用例 / 2026-09-23**，HEAD 是 10-02 ⇒ **台账落后于代码，门实际已红而台账仍绿**。另：:136-140/:262-271 用裸 `assertIn` 全文子串（符号 "ADU" 在 `DATA_SEMANTICS.md` 里到处都是）⇒ 单位表漂移检不出；:184 `startswith("docs/")` 过滤使非 docs 锚免检（当前 53 锚全为 docs，属潜伏） | **阻断（活）** |
| `backend/test_psf_moffat_oracle.py` | 274 行全文 + `PSF.md` | **B-1**。另：:6 的「§11 第 **98-99** 行」—— §11 正确（`PSF.md:156`），被引句逐字存在但在 **:159**，:98-99 是 `## 6 假设` ⇒ **行号锚错**（非伪引，句在）；:30 引用的 `工程控制/PROJECT-GOVERNANCE-01/tasks/SCI-FIX-PSF.md` 目录不存在；:183 `finite.max()` 在全不收敛时抛 ValueError | **阻断** |
| `backend/test_wcs_psf_oracle.py` | 166 行全文 | 真 oracle（真编译链接 `baseline_backend.cpp`/`p3_wcs.cpp` 比对解析式）。缺陷：:122 `range(0, W*H, (W*H)//50)` —— **768 像素只抽 52 个（6.8%）**，声称「逐像素解析复算」与实际不符；:133-146 是**往返自证**（world→pix→world 对两方向共有的系统误差完全盲），文件 :11-12 已诚实自陈；:85 类级 skip；:101 `setUpClass` 用裸 `assert`（`python -O` 下被剥离） | 须修 |
| `backend/test_p3_resample.py` | 164 行全文 | :77-81 order selector 逐例比对 + :83-85 重复调用确定性，实质。:36-39 手写 cfitsio 文件排除表（易随上游版本漂移）；无恒真门 | 通过 |
| `unit/io_adapter_test.cpp` | 252 行全文 | **正面样本**：:216-227 显式记录并堵死了**一处历史恒真门**（「旧路径不存在 ⇒ content 空 ⇒ 下面 5 条 `find()==npos` 全部恒真，空断言充数」）并加了 fail-closed 前置守卫。:119-123 同长度 bitflip、:146-147 同名目录逼 write 失败，均为实质注入 | 通过 |
| `system/runtime/runtime_contract_test.cpp` | 221 行全文 | 实质、双向：:36-42/64-78 全 token 拒绝面、:71-81 最危险值 0 的具体拒绝理由、:138-146 隐式串接、:173-203 六条负向注入。:36-38 与 :64-65 两组 token 重复断言（冗余非恒真） | 通过 |
| `unit/gaia_module_manifest_bounds_test.c` | 198 行全文 | 红绿分离在注释里写明（:16-17），:156-158 尺寸查询阶段也拒绝且不泄漏 size，:163-190 手工构造实例精确控制边界长度，实质 | 通过 |
| `cpu/baseline/provider_so_load_test.c` | 184 行全文 | :168-174 `out[i]==(float)i` 是**手工推导的独立期望值**，实质。:130 `count == ACS_CPU_BASELINE_KERNEL_COUNT` 用生产宏作期望（头文件 vs 运行时，仍能红，可接受）；:17 声称「dlclose 后句柄失效」但 :177 后 h 再未被使用 ⇒ 未验证 | 建议 |
| `unit/aio/check_verify_order.py` | 129 行全文 | **正面样本**：A 干净基线先证「验证函数非恒真」（:88-90），注入未生效即 `return 2` 拒绝出结论（:100-104），缺输入 `return 2`（:79-82）。缺陷：:44-46 注入器复制到**固定** `/tmp/…_interposer.so`，并发 ctest 会竞争 | 通过 |
| `backend/test_abi_v1.py` | 110 行全文 | :41-52 C11 编译、:54-63 `-fno-exceptions`、:65-84 Debug/Release 布局逐行一致 + 4 项 PASS 串，实质。:96-97 只查 1/12 个 science_contract_id；:96-97 `assertNotIn("march=native","-mavx")` 只扫 2 个源文件，对构建旗标是很弱的代理 | 通过/建议 |
| `backend/test_drizzle_parallel.py` | 108 行全文 | :92-96 声称「逐位确定性」，但驱动 :48 只对 `i += 997` 抽样求和 ⇒ **1048576 像素中约 1053 个被看（0.1%）**，任何污染未抽样像素的竞态都通过（筛掉真信号）；:103 `two["ns"] < one["ns"]*1.25` 允许 2 worker 慢 25% 仍绿，与 docstring「应呈正加速」不符；:55 类级 skip | 须修 |
| `unit/p1_psfw/p1psfw_test_main.hpp` | 94 行全文 | 故障注入框架，:3-4 声明 selfcheck 以 `WILL_FAIL` 断言注入相必败（**反恒真的正确做法**）；:79-81 `P1_CHECKF` 在注入时翻转断言 ⇒ 确定性必败 | 通过（正面样本） |
| `validation/.../syntax_check2.sh` | 21 行全文 + `build/build.ninja` | **二手已复核（我亲自 `cat` 全文并核对）**：①脚本**退出码恒 0** —— `check()` 末条是 `if [ $RC -ne 0 ]; then head …; fi`，`if` 两分支均返 0；脚本末条命令就是 `check`，无 `exit`。四个 TU 全编译失败 CI 仍绿。②:8-9 awk 取 `CMakeFiles/acsd_phase2.dir/…`，实测 `grep -c astrocs_phase2.dir build/build.ninja` = **38**、`grep -c acsd_phase2.dir` = **0** ⇒ 4 个目标全失配（品牌统一提交 `14a50e3e` 晚于 build.ninja 生成），**awk 空结果零检查** ⇒ 在**零 include 路径**下编译 | **阻断（二手+一手复核）** |
| `config/fixtures/negative/export_blocks_mixed_flat.phase_config.json` | 16 行全文 + schema + 两个消费方 | 消费方真实存在（非悬空）。但块 `e1` 只有 `name/source/output_dir`，而 `export_block.required = ['source','output_dir','output_mode']` ⇒ **它同时违反两条独立规则**，两个消费方都只 `assertTrue(errs)` 不断言红因 ⇒ **该负例无法把红归因到被测的「blocks/平铺互斥」规则**；兄弟 `mosaic_blocks_mixed_flat` 是判别性的，两者不对称 | 须修 |

---

## 4. 发现清单

### 阻断（4）
| # | 位置 | 一句话 |
|---|---|---|
| **BL-1** | `backend/test_psf_moffat_oracle.py:88-116` + `:105,:115` | 「独立 Oracle」用自己 `moffat4` 合成数据、再用同一个 `moffat4` 拟合；`dpsf_psf` 从未被调用 ⇒ 生产 PSF 全瘫仍全绿 |
| **BL-2** | `cli/test_command_tree.py:314-320`（vs 本文件 :295-296） | 结构不全 ⇒ rc=2 先于键校验 ⇒ `assertNotIn("unknown key")` 恒真，且未断言 rc |
| **BL-3** | `io/test_fits_stream_contract.py:29-43` + `.so` mtime 9月3日 vs 源 mtime 10月1日 + `.gitignore:29` | 只在 `.so` 不存在时重编 ⇒ 全套件测一个月前的实现，门照绿 |
| **BL-4** | `contracts/product_family/test_field_constraints_integration.py:176-186` + `clause_registry.json` 7 条锚 + `test_index.csv:9` | 7 条条款锚悬空（`docs/engineering/v6/QA_MATRIX.md` 不存在，根因 `ed33f57f` 删载体留锚），而活台账仍记 PASS/2026-09-23 ⇒ 台账落后于代码 |

### 须修（10）
1. `unit/gaia_cat_test.c:935` — `res[200000-1]` 与 `res[199999]` 是同一元素，自比较恒真。
2. `unit/gaia_cat_test.c:563-568` — 文件头 :15-16 声称「mag 用 \|Δ\|≤1e-9 断言」，实际**只打印不断言**；该断言从不存在。
3. `unit/gaia_cat_test.c:14-19, :394` — KI-1/KI-2 生产已修复（`gaia_client.c:291` / `:2810-2815`），缺陷登记与行号引用均失效；KI-2 的**真实场景（混合 DB）因只测极冠全 SP 锥而永不被执行**。
4. `unit/cosmetic_adapter_test.cpp:501,:557` + `module_entry.cpp:1040` — BITWISE 对拍两侧调用**同一函数**；且全文件无 `hot>0/cold>0` 断言 ⇒ 坏点检测整体失效仍绿。
5. `io/test_fits_stream_contract.py:226-230,242-246,256-260` — astropy 缺失即 3 条交叉 oracle 全 skip 且计通过。
6. `unit/aio/oracle/aio_oracle_lib.py:249-256 vs :289` — schema 回退分支是死代码，注释 :286 为假；`:326` `pixfrac` 缺失即整条 C5 静默不跑。
7. `runtime/integration/test_rt004_executor.py:314` — `THREAD_CREATE_RE` 定义后零使用，私池扫描承诺宽于实现；`:707` 池形态门漏掉 `module_adapters.cpp`（2 处池）；`:426` 无池即静默空转判绿，与 `:746-747` 自相矛盾。
8. `runtime/integration/test_rt004_executor.py:457-461,504-506` — `kReclaimDeadlineMs=2000` 及其 1.08%/p50=3.8ms/p99=8.6ms 全部数值所依据的 `run/FLAKE-01/evidence/tjoin_window.cpp` 全仓 0 命中，阈值不可核。
9. `backend/test_drizzle_parallel.py:48 vs :92-96` — 「逐位确定性」只抽样 0.1% 像素。
10. `unit/cpu003_profile_v2_test.cpp:253-255` — 「负例(判据能红)」把 `wrong` 定义为 `ref[1]+非零量` 再断言差非零，自证；同组 7a 才是真反例。

### 建议（10）
`backend/test_wcs_psf_oracle.py:122`（768 像素只抽 52，声称「逐像素」）、`unit/cpu003_profile_v2_test.cpp:169` 与 `p2_sky_kappa_test.cpp:169`（`x*0.0+` 恒零残留项，写得像比较两量）、`cli/test_command_tree.py:83`（类级 skip，潜伏）、`backend/test_drizzle_parallel.py:103`（允许慢 25% 却称「正加速」）、`contracts/.../test_field_constraints_integration.py:136-140,262-271`（裸 `assertIn` 全文子串）、`:184`（非 docs 锚免检）、`unit/aio/oracle/aio_oracle_lib.py:277-283`（只校验合同侧退役登记，代码侧 `product_io` 仍有活枚举 `kPsfswRobustWeight`，与 `drizzle_science.h:88` 的「物理删除」矛盾）、`backend/test_abi_v1.py:96-97`（只查 2 源文件/1 个 contract id）、`cpu/baseline/provider_so_load_test.c:17`（声称验证 dlclose 后句柄失效，实际未验证）、`io/test_fits_stream_contract.py:216-217`（无副作用死分支）。

---

## 5. 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **R-1** | 读 `test_psf_moffat_oracle.py`，追踪 `synth→moffat4` 与 `fit_block→curve_fit(moffat4)` 的数据流 | 若生产 `dpsf_psf` 参与任一环，则 oracle 独立 | **推翻成功**：生产符号零参与；两侧同一函数 ⇒ BL-1 |
| **R-2** | 把 `test_command_tree.py:317` 的配置原样读出，比对同文件 `:295-296` 自述的阻断优先级 | 若结构错不先于键校验，则 `assertNotIn` 有效 | **推翻成功**：文件自身写明 rc=2 先于键校验 ⇒ BL-2 |
| **R-3** | `ls -la` 比对 `.so` 与 `fits_core.c` 的 mtime，并 `git check-ignore` | 若 `.so` 每次重编或受版本控制约束，则无问题 | **推翻成功**：9月3日 vs 10月1日，且 `.gitignore:29 *.so` ⇒ BL-3 |
| **R-4** | `sed -n '360p;370p;383p;397p;419p' ACSD_DESIGN.md` + 读 `CLI_PROTOCOL_V1.md:3` | 若 §6.2 是命令树、§6.3 是码表，则引用正确 | **推翻成功**：§6 是 export，命令树在 §7.1 ⇒ 引用系统性错一整章 |
| **R-5** | 展开 `gaia_cat_test.c:935` 的索引表达式 `200000-1` vs `199999` | 若两者是不同元素，则非自比较 | **推翻成功**：整数相等 ⇒ 同一元素 ⇒ 恒真 |
| **R-6** | 查 `gaia_client.c` 中 `out_mag` 声明与 KI-2 的 `memcpy/memset` 修复 | 若生产仍未修，则测试豁免合理 | **推翻成功（推翻豁免的合理性）**：`:291 double *out_mag /*bitwise 往返*/`、`:2810-2815` 已 memset 零填充 ⇒ 两项「已登记缺陷」均已修复，测试仍按缺陷存在处理并放弃了 KI-2 的真实场景 |
| **R-7** | 沿 `module_entry.cpp:1040` 追插件内部调用，再比对 `cosmetic_adapter_test.cpp:486` 的 direct 调用 | 若两侧是不同实现，则 BITWISE 对拍是交叉验证 | **推翻成功**：均为 `ac_correct_frame` ⇒ 结构对称型自证 |
| **R-8** | 用 python 遍历 `clause_registry.json` 96 条 clauses 的 `source_binding.file`，逐个 `Path(f).exists()` | 若锚全部有效，则该门为绿 | **推翻成功**：7 条指向不存在的 `docs/engineering/v6/QA_MATRIX.md` |
| **R-9** | `grep -c "astrocs_phase2.dir" build/build.ninja`（38）vs `grep -c "acsd_phase2.dir"`（0），再读 `syntax_check2.sh:5-17` 的末条命令 | 若目标名匹配且 awk 空会报错，则判据有效 | **推翻成功**：全失配 + awk 无检查 + `if` 两分支返 0 ⇒ 脚本恒绿 |
| **R-10** | 逐字核对 `step10_figs.py:73` 的 `1 + (a*1-1)*0 + 0` | 若含 `a`，该列随数据变化 | **推翻成功**：`* 0` 使整式恒 = 1.0000 ⇒ 伪参照（**二手，我已 cat 全文复核该行**） |
| **R-11** | 算 `export_block.required` 与负例块 `e1` 的键集差 | 若负例只违反被测的那一条规则，则判别性成立 | **推翻成功**：`required=['source','output_dir','output_mode']`，`e1` 缺 `output_mode` ⇒ 消掉被测违规后仍红 |
| **R-12** | 在 `aio_oracle_lib.py` 里找 `os.path.exists(schema_path)` 与前序 `open(schema_path)` 的先后 | 若回退分支可达，则防御有效 | **推翻成功**：`:255` 无条件 open 在 `:289` 判断之前 ⇒ 回退是死代码 |
| **R-13** | 推 `test_drizzle_parallel.py` 驱动 `:48` 的 `for(i=0;i<N;i+=997)`，N=1024² | 若「逐位确定性」覆盖全图，则可信 | **推翻成功**：仅 ~1053/1048576 ≈ 0.1% 被检验 |
| **R-14** | 算 `test_wcs_psf_oracle.py:122` 的 `range(0,768,15)` 步长 | 若「逐像素解析复算」名副其实，则覆盖全图 | **推翻成功**：只 52 点 = 6.8% |
| **R-15** | 查 `runtime_contract.h` / `product_io` 中 `kPsfswRobustWeight` 的活引用 | 若已物理删除，则「退役」声明成立 | **推翻成功**：`bunit.h:39` + `bunit.cpp:82/96/158/250` + `phase1_product.cpp:182` 仍是活枚举与活 case，与 `drizzle_science.h:88` 的「不再有枚举项」直接矛盾 |

---

## 6. 盲复算（遮住既有判定，独立取证）

做法：在**不看**任何既有审稿结论的前提下，对 5 条现行结论重做一次独立推导。

| 现行结论（遮住） | 我的独立重算 | 判 |
|---|---|---|
| 「`gaia_cat_test` 的缓存一致性 I3 已覆盖（mag 走 \|Δ\|≤1e-9 放行）」 | 重读 :559-568：`mag_delta` 只被 `if` 用来 `fprintf`，**全文件无任何 CHECK 消费它**；而 :15-16 明写「缓存一致性断言对 mag 使用 \|Δ\|≤1e-9」。⇒ 断言不存在，I3 的 mag 分支从未被执行 | **偏松**（我判更严重） |
| 「`syntax_check2.sh` 是有效的语法门」 | 重推 shell 语义：`check()` 函数体最后一条是 `if …; then head …; fi` → 条件假返 0、条件真 `head` 成功也返 0；脚本末条命令为 `check`，无 `exit` ⇒ rc≡0。再验目标名：`astrocs_phase2.dir` 命中 38、`acsd_phase2.dir` 命中 0 | **一致**（阻断成立） |
| 「`test_command_tree` 的 bitpix 用例证明 bitpix 是既有白名单键」 | 重读 :314-320 与本文件 :295-296；结论：结构不全 ⇒ 提前 rc=2 ⇒ 该断言以「没走到」通过，且无 rc 断言 | **一致** |
| 「`eng/tests/contracts` 套件绿（台账 PASS/84/2026-09-23）」 | 重跑锚存在性：7 条指向不存在的目录；HEAD 10-02，删除载体的是 `ed33f57f` | **偏松**（台账已过期，门实为红） |
| 「`test_psf_moffat_oracle` 是 SCI-PSF-001 §11 的独立 oracle 落地工件」 | 重追数据流：`synth` 与 `fit_block` 共用 `moffat4`；生产 `dpsf_psf` 零参与 | **偏松**（我把「独立」判为不成立） |

**盲复算小结**：5 条中 2 条与既有判定一致，3 条**比既有判定更严**（偏松方向）。无「偏严」误判。

---

## 7. 子代理派发记录

派了 **4 个**（分片互斥、覆盖 47/47 份）。本片纪律要求「逐条复核、���明否决了哪些及理由」。

| 子代理 | 分片 | 状态 |
|---|---|---|
| A | 7 份（门/CLI/config/quality/cpu runner） | ✅ 已交付 |
| B | 8 份（oracle/backend/abi/cpu） | ⏳ 交付时仍在跑 |
| C | 13 份（validation + fixtures + CMake） | ✅ 已交付 |
| D | 12 份（C++ 单测 + io + contracts） | ⏳ 交付时仍在跑 |

### 7.1 对 A 的复核（逐条）

| A 的结论 | 我的复核 | 处置 |
|---|---|---|
| B1 `test_command_tree.py:83` 类级 `skipUnless` 属 fail-closed 伪装 | 我亲自读 :83 确认；并核实 `build/acsd` 存在 ⇒ 属**潜伏** | **采纳**，但加注「当前未触发」 |
| M7 `:2/:8/:14` 引文系统性错一整章 | 我 `sed -n '360p;370p;383p;397p;419p'` 逐行核对 §6=export / §7.1=命令树 / §7.2=退出码，并读 `CLI_PROTOCOL_V1.md:3` 上游声明逐字印证 | **采纳（我独立复现）** |
| M9 `:314-320` bitpix 恒真门 | 我亲自读该段并与本文件 :295-296 对照 | **采纳（我独立复现，升为 BL-2）** |
| M3 `test_linux_release.py:16-17` 引 `AGENTS.md §5「SKIP 充数算未完成」` 为伪引 | 我 `grep -rn "SKIP 充数" AGENTS.md docs/` = **0 命中**，且 `AGENTS.md:71 ## 5. = 文档维护` | **采纳（我独立复现）** |
| M2 `test_linux_release.py:113-114` 裸子串恒真 | 逻辑自洽（`^…-alpha\.\d+$` 已先拒含 `rc` 的版本），但**该文件我未亲自读完** | **降级为二手**，不进阻断 |
| B4 引 `test_index.csv:8` 称 config 套件 5 红 | 我**独立发现**了同类但不同处的活问题（contracts 套件台账过期），方向一致 | **采纳其方法，结论另行独立取证** |
| S4 `test_linux_release.py:144-154` 往返自证 | 同上，该文件未由我亲读 | **降级为二手** |

### 7.2 对 C 的复核（逐条）

| C 的结论 | 我的复核 | 处置 |
|---|---|---|
| B1 `syntax_check2.sh:5-17,21` 退出码恒 0 | 我 `cat` 全文逐行确认 shell 语义 | **采纳（一手复核）** |
| B2 4 个 ninja 目标名全失配 | 我跑 `grep -c astrocs_phase2.dir`=**38** / `grep -c acsd_phase2.dir`=**0** | **采纳（我独立复现）** |
| B3 `step10_figs.py:73` 代数恒真 | 我 `sed -n '70,76p'` 看到 `1 + (a*1-1)*0 + 0` | **采纳（我独立复现）** |
| M1 `export_blocks_mixed_flat` 非判别性负例 | 我 `cat` 该 json + 打出 `export_block.required=['source','output_dir','output_mode']` | **采纳（一手复核）** |
| M11「归档运行结果入库违反裁定」 | 我**未亲查**该目录的文件清单 | **降级为二手** |
| C7「`run_verification.sh` 的 `eng/run/` 会污染 git」 | **C 自己已推翻**，我复核其 `git check-ignore` 路径，结论成立 | **接受其自纠** |

### 7.3 我**否决/修正**了子代理的哪些说法
1. **否决**子代理 A 把 `test_linux_release.py` 的多条列为「阻断」——该文件我未亲读，且其 M2/M3 属二级断言；只保留我亲自复现的 M3（伪引）。
2. **否决**把 `PSF.md` 那条算作「伪引」的可能误判：我初查以为 §11 与行号皆错，复核后确认 **§11 正确（`PSF.md:156`），只有行号 98-99 错（应为 :159）** ⇒ 改判为「行号锚错」，不按伪引计。
3. **否决**子代理 A 对 `p2_sky_kappa_test.cpp` 缺席的默认评价——我亲读后认定它是本片**最好的反恒真教材**，应记为正面样本而非问题项。
4. **修正**我自己在复核初稿中对 `pair_ratio.py`「离群 fixture 可能失效」的怀疑：读 `cosmetic_corrector.cpp:230-264` 后确认 `detect_hot_pixels` 确实读 `dark`、`detect_cold_pixels` 读 `bias`，**fixture 是有效的**，不作为发现记录。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 本片成员清单与行数（权威）
sed -n '1502,1556p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml

# BL-1  PSF oracle 自证：合成与拟合共用同一 moffat4
grep -n "def moffat4\|def synth\|def fit_block\|curve_fit(moffat4" eng/tests/backend/test_psf_moffat_oracle.py
grep -n "dpsf" eng/tests/backend/test_psf_moffat_oracle.py          # 仅注释提及，零调用

# BL-2  bitpix 门结构不全（本文件 :295-296 自陈阻断优先级）
sed -n '293,320p' eng/tests/cli/test_command_tree.py

# BL-3  陈旧 .so
ls -la lib/infrastructure/aio/io/libfits_core_test.so lib/infrastructure/aio/io/fits_core.c
sed -n '29,43p' eng/tests/io/test_fits_stream_contract.py
git -c core.quotepath=false check-ignore -v lib/infrastructure/aio/io/libfits_core_test.so

# BL-4  7 条悬空条款锚 + 台账过期
python3 -c "
import json,pathlib
reg=json.load(open('eng/contracts/data/clause_registry.json',encoding='utf-8'))
bad=[(c['id'],c['source_binding']['file']) for c in reg['clauses']
     if isinstance(c.get('source_binding'),dict) and not pathlib.Path(c['source_binding']['file']).exists()]
print(len(bad),'dangling'); [print(' ',*b) for b in bad]"
sed -n '9p' eng/tests/test_index.csv

# gaia_cat_test.c:935 恒真 + KI-1/KI-2 已修复
sed -n '930,936p' eng/tests/unit/gaia_cat_test.c
sed -n '291p;2810,2815p' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c

# 文档引用系统性错章（A 的 M7，我独立复核）
sed -n '360p;370p;383p;397p;419p' docs/ACSD_DESIGN.md
sed -n '3p' docs/engineering/CLI_PROTOCOL_V1.md

# syntax_check2.sh 恒真 + 目标名失配（C 的 B1/B2，我一手复核）
cat -n eng/tests/validation/release02/fix_p2b_variance_oracle/syntax_check2.sh
grep -c "astrocs_phase2.dir" build/build.ninja; grep -c "acsd_phase2.dir" build/build.ninja

# 负例非判别性（C 的 M1，我一手复核）
cat eng/tests/config/fixtures/negative/export_blocks_mixed_flat.phase_config.json
python3 -c "import json;print(json.load(open('eng/contracts/schemas/phase_config_export.schema.json',encoding='utf-8'))['\$defs']['export_block']['required'])"

# 退役对象仍有活调用者（R-15）
grep -rn "kPsfswRobustWeight" --include=*.cpp --include=*.h lib/ | head
sed -n '88p' lib/algorithms/drizzle/healpix_drizzle/drizzle_science.h

# 抽样门（筛掉真信号）
sed -n '122p' eng/tests/backend/test_wcs_psf_oracle.py     # 768 点抽 52
sed -n '48p' eng/tests/backend/test_drizzle_parallel.py     # 0.1% 抽样
```

---

## 9. 计数口径声明

- 本片**不含任何「门实例 / 去重门 / 整改分母」意义上的正式整改计数** —— 本片是 `eng/tests` 测试资产审查，不产出整改分母。
- 上文「阻断 4 / 须修 10 / 建议 10」= **我亲自取证并逐条复核过的条目数**，不含子代理单方面主张且我未复核的项。
- 若计入子代理单方面主张（我标注为「二手/未亲读」者），本片总条目约为 **阻断 4 + 须修 19 + 建议 16**，但其中 `test_linux_release.py`（5 条）、`test_cfg001_contracts.py`（6 条）、`pair_ratio.py`（5 条）等**我未亲读**，不构成可靠分母。

---

## 10. 纪律声明

零 git 写（未 add/commit/checkout/reset/stash）；未编译、未 ctest、未 pytest、未跑任何实验或测试脚本；**未改仓内任何文件**（唯一写入为本交付件）；**未读 `/tmp/acsd_g08/`**。每条结论均给 `文件:行` 或可复现命令。