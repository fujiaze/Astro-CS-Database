
---

## W4 · §8.4 承诺的加载校验与 CPU provider 不在图，但安装布局仍装 DSO

**判定：部分确认 + 部分推翻**。四件事里"谁不编译""装出去几份"确认；**"产品内无 `dlopen`"这一条我推翻**——产品里有一条活的动态加载路径，而且它恰好就是 §8.4:659 那五项检查的实现。定性因此从"承诺能力缺失"改为"承诺能力有另一份实现在跑，装出去的模块 DSO 属无人加载的死重量"。

### 我的独立取证

**(a) 谁编译 / 谁不编译（只读 CMake 文本）**

- `lib/infrastructure/pipeline/module_loader/{secure_loader.c, module_registry.c}`：`git grep -n "secure_loader\|module_registry" -- '*CMakeLists.txt' '*.cmake'` ⇒ **0 命中（rc=1）**；`git log --diff-filter=D --name-only -- lib/infrastructure/pipeline/module_loader` ⇒ 无删除记录 ⇒ 二者**不被任何 target 编译**，术语是"不被任何 target 编译"（不是"不在生产 target"，更不是"已退役"）。
  - 但它们**不是纯死码**：`eng/tests/abi/mod001_install_load_check.py:108-125` 用 `CC`（默认 gcc）**现场编译 `secure_loader.c`** 成探针 `abi003_probe` 再逐 unit 装载验证（`:113` 断言"compile secure_loader.c (loader 合同源)"）。⇒ 精确表述：**不被任何构建 target 编译，只被一个测试脚本按需现场编译**。
- CPU provider 正式 ABI 面：`git ls-files lib/infrastructure/benchmark/ | grep CMakeLists` ⇒ `lib/infrastructure/benchmark/cpu/**` **整个子树没有任何 CMakeLists**；`git grep -n "baseline_provider\|avx2_provider\|avx512_provider\|capability_detect" -- '*CMakeLists.txt' '*.cmake'` ⇒ 只命中注释（`CMakeLists.txt:235/343` "CPU-002 交付，本任务不伪装 provider 完成"）与 `eng/tests/unit/CMakeLists.txt:1809` 的注释。⇒ `cpu/{baseline,avx2,avx512}/src/*_provider.cpp` 与 `common/src/capability_detect.c` **不被任何 target 编译**。
- 安装树实际编译出来的 SHARED：全仓 `add_library(... SHARED/MODULE)` 共 16 个 target；其中 `astrocs_p1_noise` 声明在已解除的 `add_subdirectory` 子图内（`CMakeLists.txt:320-332` 注释保持注释态）⇒ 不配置；`install_layout.cmake` 的 `if(TARGET)` 门卫随之自动不装。真正装出去 **9 个**：根 `libacsd_runtime.so`+`libacsd_io.so`、`modules/` 6 个（`astrocs_noop` + `astrocs_catalog_gaia` + `astrocs_p1_{drizzle,calibration,cosmetic,hips_writer}`，见 `install_layout.cmake:79-82,104-111`）、`providers/astrocs_cpu_baseline.so`（`:85-88`）。
  ⇒ 成稿说"5 个模块 DSO 装进 `modules/`"：科学模块确为 5，但 `modules/` 实收 **6** 个文件（含 conformance `astrocs_noop`）。

**(b) 有没有动态加载入口 —— 这一条把成稿推翻**

产品里**有**活的 dlopen/dlsym，且在被 `acsd` 链接的静态库里：

- `lib/infrastructure/benchmark/backend_host/backend_loader.cpp:152-168` 真实调用 `LoadLibraryExA`/`LoadLibraryA`/`GetProcAddress`/`dlopen(RTLD_NOW|RTLD_LOCAL)`/`dlsym("astrocs_backend_get_api_v1")`；该 TU 在 `astrocs_cpu` 源清单（`CMakeLists.txt:701`+`:705`）⇒ 链进 `acsd`（`:1011`）与 `astrocs_module_adapters`（`:1071`）。
- 活的调用链（生产命令 `benchmark`）：`commands.cpp:2313 std::ifstream mf("backends.manifest.json")` → `:2318 parse_backends_manifest` → `:2321 preflight_entry(".", e, …)`；以及 `commands.cpp:2375 generate_profile_v2(…, install_dir)` → `profile_gen_v2.cpp:358` 读 `backends_dir + "/backends.manifest.json"` → `:365 load_backend(...)` → 上述 dlopen。
- 更关键：**§8.4:659 承诺的五项加载前检查，`backend_loader.cpp` 逐条实现**（`preflight_entry`，`:100-131`）：①只认私有目录内裸文件名（`is_bare_filename`，注释"禁 PATH/LD_LIBRARY_PATH 注入"）②ABI 版本 ③仅在私有 backends 目录内解析存在性 ④sha256 实测比对 ⑤required CPU 特征 ⊆ detected（"CPUID+OSXSAVE+XGETBV 实测；不支持绝不尝试执行"，即"OS 可安全执行状态"）。
⇒ 所以"§8.4 承诺的加载校验不在图"不成立：承诺的那件事**有一份在图上跑的实现的**；不在图的是**第二份**实现（`secure_loader.c`，模块/provider ABI 通道，多做了 canonical 路径 + `allowed_root` + ELF 校验）。这正是 AGENTS §8「同一能力两份实现」的形态，而不是"能力缺失"。

**(c) 装出去的 DSO 有没有人加载 —— 这一条比成稿更具体，且更难看**

- 产品唯一活加载器找的是 `<install_dir>/backends.manifest.json` + 该目录内的裸文件名（`commands.cpp:2313` 用 CWD 相对路径，`profile_gen_v2` 用 CLI 所在目录）。而安装布局定义的目录只有 `modules/ providers/ schemas/ licenses/`（`install_layout.cmake:52-55`），**没有 `backends/`**；`git ls-files | grep -i backends.manifest` ⇒ 仓库里**没有** `backends.manifest.json`，只有生成器 `eng/tools/gen_backends_manifest.py`。
  ⇒ 交付树里活加载器**永远找不到清单**，`preflight_entry` 不被调用、`load_backend` 不被调用 ⇒ 动态 ISA 选择在产品上恒回落静态 baseline（`CMakeLists.txt:728` 注释自证"acsd 的链接闭包只含基线段"，且 `astrocs_cpu_avx2/avx512` 是 **STATIC**（`:729/:736`），根本不产 DSO）。
- 近失（成稿未记）：唯一装出去的 provider DSO `providers/astrocs_cpu_baseline.so` 的导出入口正是 `astrocs_backend_get_api_v1`（`baseline_backend.cpp:2` 注释"astrocs_backend_get_api_v1 唯一入口"），**与活加载器查找的符号同名同 ABI**；只是它被放进 `providers/`，而加载器只扫 `backends/` 且无清单 ⇒ 这份 DSO 在 ABI 上可加载、在布局上永不被加载。
- `modules/` 那 6 份说的是另一个 ABI（`astrocs_module_query_v1`，`install_layout.cmake:91-95` 注释；唯一实现其加载通道的是不在图的 `secure_loader.c`/`module_registry.c`）⇒ **确证"永不加载"**。
- 产品侧曾经的用户入口已删：`commands.cpp:2238-2245`「CLI-001 已删除 modules list/verify/selftest 用户命令…原实现（`cmd_modules_list`/`cmd_modules_verify`/`cmd_selftest` 及 `locate_product_manifest`/`load_product_manifest`…）为零调用点死代码，按 ENGINEERING_SPEC §8 显式退役」⇒ 今天用户无法让 `acsd` 去加载任何 `modules/` 单元。

**(d) "同一能力两份二进制" 的确证（比成稿口径更硬）**

- drizzle：`lib/algorithms/drizzle/CMakeLists.txt:18` 注释自陈"生产源（与根 CMakeLists.txt `astrocs_drizzle` **逐文件一致**）"，`:20-31` 列 10 个 TU 与根 `CMakeLists.txt:669-680` 的 `astrocs_drizzle`（STATIC，进 `acsd`）**同一批文件**；`:36-39` 进一步说明为何把依赖闭包整体重编进 `.so`（"STATIC 对象非 PIC…链 .so 失败实证 ⇒ 不可链 .a"），并把 `aio_*` 源（`:40-44`）与整份 cfitsio 清单（`:67-71`）再编一遍。⇒ 同一科学能力在**同一棵源树**上编两份，靠人工"逐文件一致"注释维持，无机器门防漂移。
- baseline 内核：`baseline_backend.cpp` **一个文件**同时是 `astrocs_cpu`（STATIC→`acsd`，`CMakeLists.txt:705`）与 `astrocs_cpu_baseline`（SHARED→`providers/`，`:344-345`）的源；另 `baseline_kernels_impl.inc` 被 4 个 backend TU 文本包含（见 W3(c)）。
- 汇总"交付树 SHARED 有多少消费者"：安装树共 **9** 份 SHARED（`modules/` 6 + `providers/` 1 + 根 `libacsd_runtime.so`/`libacsd_io.so` 2）。`acsd` 的链接行是 `CMakeLists.txt:1008-1015`（`astrocs_core astrocs_cli_runtime astrocs_common astrocs_cfitsio astrocs_aio astrocs_hips astrocs_p3_projection_wcs astrocs_calibration astrocs_phase2 astrocs_drizzle astrocs_cpu astrocs_phase{1,2,3}_session astrocs_cli_subcommands`）——**9 份里没有任何一份在上面**；`acsd_runtime` 亦不被任何 target 链接（只被 install），`CMakeLists.txt:224` 自称"平台 SHARED DLL **骨架**"；`acsd_io` 只被一个测试 target 链接（`eng/tests/unit/CMakeLists.txt:1685`）。⇒ **交付树里 9 份 SHARED，产品侧零消费者**（唯一 dlopen 只扫不存在的 `backends/`）。

**(e) 双平台分开判（按题给的裁决纪律）**

- **Linux（脚本 + 手动解压）**：上面 (a)(c)(d) 全部在 Linux 侧成立且已交付（`eng/packaging/astrocs.product.json` 10 units，6 带 `module_id`）。`module_loader/README.md:46-52` 明文宣告该通道"不在三个生产命令的运行期调用图上…本通道是安装/交付面"，并经 `CHK-PROD-WIRING` W6 立案（台账 `eng/ci/ledgers/prod_wiring.json:680-724`）。⇒ **Linux 侧是"已登记的交付面冗余 + §8.4:657 未达成"，不是"未开工"**。
- **Windows（安装器）**：`install_layout.cmake:9-10`「Windows 安装树布局本文件同步声明，**实际编译/加载验证由 WIN-\* 执行**」；`astrocs.product.windows.json.in:6` 末句同义；`secure_loader` 的 `_WIN32` 分支 `ACS_ERR_UNSUPPORTED`，README:53-55 明写"由 WIN-* 按本头契约落地…当前返回 UNSUPPORTED，**不伪装完成**"。⇒ **Windows 侧是"未开工"，按"别把没做当已破"不判缺陷**。风险点只有一条：Windows 清单同样登记 6 个 `module_id` 单元为 `IMPLEMENTED`，若 WIN-* 交付时照抄 Linux 的"装了但无人加载"事实，就会把同一缺陷带进正式平台——这是**前瞻登记**，不是当前缺陷。
- 该 loader 是否只在单平台有意义：**不是**。`modules/` 通道在两平台都声明安装；`backends/`（活加载器）通道在两平台都无产物。所以必须按通道判、不按平台判——我上面已分开。

**(f) 设计内部打架（影响这条该怎么定级，须登记）**

- §8.4:657「每个算法模块是**独立 DLL/SO**」与 §7.1:487「其余能力**以 DLL/SO 形式存在或在可执行程序内部实现**」是同文档两句、给出相反许可：§7.1 明文允许静态内联。实测科学模块 SHARED target 只有 5 个在配置（`astrocs_p1_{calibration,cosmetic,drizzle,hips_writer}` + `astrocs_catalog_gaia`），而有代码的算法模块目录 14 个 ⇒ 若按 §8.4:657 判，缺口 9/14；若按 §7.1:487 判，现状合法、只剩"多余 DSO"问题。**这条定级的落点取决于哪句为准，不是我能在复算层裁决的**（见文末上呈）。

### 与成稿差异

| 成稿（AUD401-014/018 面） | 我的复算 |
|---|---|
| 唯一实现的两份 loader 源不被任何 target 编译 | **确认**（0 CMake 命中 + 无删除历史），但补一条：`mod001_install_load_check.py:108-125` 会现场 gcc 编译 `secure_loader.c` ⇒ 它是"无 target"，不是"无执行" |
| 各 provider 源不被任何 target 编译 | **确认**，并给机理：`lib/infrastructure/benchmark/cpu/` 子树**根本没有 CMakeLists** |
| **产品内无 `dlopen`** | **推翻**：`backend_loader.cpp`（在 `astrocs_cpu`→`acsd`）提供活的 `dlopen/LoadLibraryExA/dlsym`，由 `benchmark` 命令经 `commands.cpp:2318/2321/2375` 调用；且 `preflight_entry` 已实现 §8.4:659 全部五项检查 ⇒ 该条不是"承诺能力缺失"，是"两份 loader 实现、图上那份不是 §8.4 指名的那份" |
| 安装布局照旧把 5 个模块 DSO 装进 `modules/` | **基本确认**，数量修正：`modules/` 实装 **6**（含 conformance `astrocs_noop`），另有 `providers/` 1 + 根 2 |
| 同一能力两份二进制，DSO 那份永不加载 | **确认且更强**：drizzle 靠注释维持"逐文件一致"无门防漂移；`baseline_backend.cpp` 一文件双角色；且**唯一活加载器找的 `backends/` 目录与清单在交付树里不存在**（`gen_backends_manifest.py` 有、无安装规则、`avx2/avx512` 是 STATIC）⇒ 动态 ISA 能力在产品上恒 inert；`providers/astrocs_cpu_baseline.so` 与活加载器同 ABI 同符号名却放错目录永不被扫 |

### 定级建议

- **交付树装了 9 份任何产品代码路径都不加载的 SHARED（`modules/` 6 + `providers/` 1 + 根 2），且产品内那条活加载器要的清单/目录从未交付**：**P1**。理由：(i) `astrocs.product.json` 把其中 6 个单元标 `IMPLEMENTED` 并有门（`mod001`/`verify_install_tree.py`）在自证——但门验的是"DSO 能被现场编译的探针加载"，**没有任何门验"产品是否会加载它"** ⇒ 交付台账与实际能力之间存在结构性虚高，正撞 AGENTS 硬禁令"不用 facade/空骨架冒充实现完成"；(ii) 同一科学源被编两份且只靠注释对齐，属可引爆的一致性债；(iii) 动态 ISA 选择链在产品上不可达 ⇒ §9:672「profile 选路 + 安装目录」这条治理只在 `cpu_profile.json` 写出侧成立。
- **`secure_loader.c`/`module_registry.c` 与 provider ABI 三 TU 不在图**：**P2**（连同上面"两份 loader"归入既有 AUD401-012/017 一致性面）。理由：该通道已被 `module_loader/README.md:46-52` + W6 台账**如实登记为安装/交付面**，且 §7.1:487 允许静态内联 ⇒ 不是隐瞒、不是功能缺失；产品不因它坏。
- **Windows 侧**：**非缺陷（待开工）**，只登记一条前瞻：WIN-* 不得照抄"IMPLEMENTED = 会被加载"的口径。
- 不建议 P0：无任何证据显示产品产出错误结果或数据丢失；影响面是交付体积、一致性漂移概率与台账诚实度。

### 缺什么证据

1. `backends.manifest.json` + ISA 后端 DSO 是否曾计划进安装树（只找到 `eng/tools/gen_backends_manifest.py`，未找到安装规则，亦无"故意不装"的登记）——需查 `docs/ci`/`eng/packaging/README.md` 之外的变更单或上呈一句确认。我这层只能证"当前不装"，不能证"决定不装"。
2. §8.4:657 与 §7.1:487 谁为准（我的 (f)）——这是**必须负责人裁**的一条，因为它决定这条是 P1 还是 P2，也决定 AUD401-001/014 相关改法的方向。
3. 产品是否在其它我没扫到的入口（如 doctor）加载模块 DSO：我扫的是全部 166 个生产编译源里的 `dlopen/LoadLibrary/dlsym/GetProcAddress`（7 处命中，全在 `backend_loader.cpp` + 1 处注释），可视为穷尽；唯 `acsd` 之外若引入外部脚本做加载，不在静态可读范围内。

---

## 进 P0/P1（只列"确认"）

| W | 判定 | 定级 | 一句话（问题 / 谁修 / 怎么算修好） |
|---|---|---|---|
| W1 | 确认 | **P1** | normalize 与 mosaic 两种阶段调度形态在生产调用面上零引用（`PrefetchCache`/`MosaicWindow` 亦零引用），三阶段实由一个通用 DAG `Scheduler` 跑；修好 = 按 export 既有范式（阶段调度器嵌在节点内、并发度取 lease）把两条接上，并让 §8.3 表格的编排形态在端到端探针里可见 |
| W3(f) | 确认（新条，非成稿条目） | **P1** | 通用调度器自起 `budget_` 个常驻 worker 却不向 `ThreadBudget` 记账（`scheduler.cpp:373-375` vs `context.h:110-111` 自陈不变量），稳态存活线程可达 2×budget，内存回压按 budget 校准 ⇒ 判据失真；修好 = worker 侧入同一账本，并用探针实测 4/8/16 档曲线钉住 |
| W3(c)+(b) | 确认（计数修正为 16） | **P1** | "唯一池"实现成"唯一数量源"：16 处私建池（算法模块内 6、基建内 10）+ `executor.cpp` 靠 `module_adapters.cpp:255` 文本包含进生产、防重定义只有一行注释；修好 = `executor.cpp` 进 `astrocs_core` 源清单、算法模块只提交 work |
| W3(g) | 确认（新增） | **P1** | `check_thread_budget.py` 豁免按文件级（`REGISTERED` 25 个文件键、无计数）且扫描面后缀不含 `.inc` ⇒ 已登记文件内加池、`.inc` 里的池（实测 1 处生产池）都不会变红；修好 = 计数登记 + `.inc` 入扫描面，配 `--self-test` 正负例 |
| W4 | 部分确认（"无 dlopen"被推翻） | **P1** | 安装树装 9 份无人加载的 SHARED（6 份标 `IMPLEMENTED` 且门只验"探针能加载"），活加载器要的 `backends/` 目录与清单在交付树不存在，同 ABI 的 `providers/astrocs_cpu_baseline` 放错目录永不被扫；修好 = 要么交付 `backends/`+清单并让 profile 真选路，要么如实改清单 status 并停止装 DSO |

不成"确认+P0/P1"的：**W2 = P2（治理，非功能）**；**W3(e) `hardware_concurrency` 第二默认 = P2**（成稿若按 P1 立案，我判**降级**）；**W4 中 `secure_loader`/provider 不在图本身 = P2**（已如实登记且 §7.1:487 允许静态内联）。

## 需负责人裁的

**裁-1（承 W4(f)，必须裁）：同一份最高设计里两句相反，判 W4/W2/AUD401-001 的模块边界口径要先选一句。**
- 二选一：**(A)** 以 §8.4:657「每个算法模块是独立 DLL/SO」为准 ⇒ 现状缺口 9/14，W4 按 P1 且需补"模块必须独立 DSO + 加载通道接线"的整改工单；**(B)** 以 §7.1:487「其余能力以 DLL/SO 形式存在**或在可执行程序内部实现**」为准 ⇒ 静态内联合法，W4 收缩为"停止装无人加载的 DSO + 如实改清单 status"，`secure_loader` 通道降为 P2 交付面债。
- 我的推荐：**(B)**，并把 §8.4:657 改写成"可选"或删句。理由：产品已按内联形态跑通且三阶段数值有不变量兜底；补 9 个 DSO + 接线是纯形态工程，收益只在"跨编译器隔离/热插拔"，而 §8.4:659 的加载前检查在 (B) 下已由 `backend_loader.cpp` 达成。
- 若不裁：W4 的定级悬着（P1 与 P2 都有权威句子支撑），下游 AUD-101/102 文档面与整改工单批次无法排序；仓库已有 5 份 module.yaml 与 `install_layout` 按 (A) 登记、`astrocs_core` 与 `astrocs_cpu` 按 (B) 编译，两套口径会继续各自长大。

**裁-2（承 W1，我倾向自裁但需一句确认）：阶段调度器与通用调度器的关系是哪一种？**
- 二选一：**(A)** 阶段调度器 = 该阶段唯一执行者（§8.3:599 字面），则须把通用 `Scheduler` 的 DAG/回压/取消下沉为被三者复用的底座；**(B)** 阶段调度器 = 节点内执行体、通用 `Scheduler` 保持唯一全局执行者（**export 现状即此**，`module_adapters.cpp:15270→14842`），则 §8.3:599"唯一执行者"要改口径。
- 我的推荐：**(B)**——已由在仓事实自证可组合，且不触碰 §8.3:607 唯一池。
- 若不裁：接手 W1 的人可能按 (A) 重写三遍 DAG 逻辑，或按 (B) 被指控"违反 §8.3"；两种改法的工单规模差一个量级。
- 说明：这条**不是纯查证题**（问代码查不出设计意图），故上呈；W1 其余部分我已自行定案，不上呈。

**裁-3（承 W3(f)，一句风险接受度裁决）：内存/线程回压账本是否要求把调度器常驻 worker 计入？**
- 二选一：**(A)** 要求（则 W3(f) 是必须修的 P1，实现侧把 worker 纳 lease 或把节点子池纳账）；**(B)** 只要求"数量单源"、不要求"存活线程数 ≤ budget"（则 §8.3:611 静态预算/回压的措辞须补"仅约束节点内并行度"，并把 §9:673/§8.3:607 的"唯一预算源"改窄）。
- 我的推荐：**(A)**，并先做一次 4/8/16 worker 实测曲线定实数——若稳态确为 2×，此条升 P0（回压判据失真直接影响 §8.3"内存占用永不越界"这一科学不变量）。
- 若不裁：W3 的整改会退化成"把 16 个池搬进 executor"的形式工程，而真正会让预算翻倍的那处记账缺失可以原样存活并继续全绿。

<!-- PROGRESS: 4/4 -->
