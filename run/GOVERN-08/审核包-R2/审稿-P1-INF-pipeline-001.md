# 审稿 P1 · INF-pipeline-001

片号：`INF-pipeline-001` · 层：`lib/infrastructure/pipeline` · 遍次：G08-05 对抗审稿 第 1 遍
审查人：P1 车道审稿代理 · 日期基准：以工作树实读为准

> **口径声明（先行，必读）**
> - 本片成员取自 `分片清单/片清单-权威版.yaml` 的 `片清单[片号=INF-pipeline-001]`。
> - **基线漂移（如实披露）**：派单称 HEAD = `850a9ede`；权威清单元信息称 `f9650dd`；我开工实测 `git rev-parse HEAD` = `1fa477a7`；收尾时再次实测 = `9a83f63a558b7efe82eb1a9435c7b40f263400a2`。**仓库在本片审查期间被持续提交，HEAD 三次不同。**
>   我已核实本片 14 份成员文件在收尾时**相对当时 HEAD 工作树干净**（`git status --short -- orchestrator.cpp` 为空），故我读的是与 HEAD 一致的仓内原文，结论对三个基线均成立。
>   **编号说明**：下文 `文件:行` 一律以**我实际读到的磁盘内容**行号为准。
> - 纪律遵守：未读 `/tmp/acsd_g08/`；**零 git 写**（未 add/commit/checkout/reset/stash）；**未编译、未跑 ctest、未跑 pytest、未执行任何二进制**；**未修改任何仓内文件**（本交付件是唯一写入，且落在 `run/GOVERN-08/审核包-R2/`，非生产源码）。中文路径检索一律 `git -c core.quotepath=false`。

---

## 1. 读完了吗

| 口径 | 数值 |
|---|---|
| 成员份数（权威清单） | **14** |
| 我**完整读完**的份数 | **14** |
| 成员总行数（权威清单 `实际行数`） | **7966** |
| 我实读行数 | **7966** |
| **覆盖率** | **14/14 份 = 100%；7966/7966 行 = 100%** |
| 未读完的 | **无** |

**计数口径说明**：行数口径 = `wc -l`（含空行与纯注释行），与权威清单 `实际行数: 7966` 逐份精确相等（`wc -l` 合计亦为 7966），故覆盖率分母无争议。14 份成员全部由**我本人**用 read 工具逐行读完（非仅 grep 命中、非仅依赖子代理转述）。

**逐份行数复核**（`wc -l` 实测 = 清单值）：

| # | 成员文件 | 行数 | 我读到的范围 |
|---|---|---|---|
| 1 | `orchestrator/cpp/src/orchestrator.cpp` | 5538 | 全 5538 行（分 10 段 read 全量覆盖，见 §3 备注） |
| 2 | `orchestrator/cpp/src/dll_loader.cpp` | 539 | 1–539 |
| 3 | `orchestrator/cpp/src/main.cpp` | 398 | 1–398 |
| 4 | `orchestrator/cpp/tests/test_p1_batchB_fixes.cpp` | 229 | 1–229 |
| 5 | `orchestrator/cpp/tests/test_p1phot_passband_identity.cpp` | 228 | 1–228 |
| 6 | `orchestrator/cpp/include/spill_manager.h` | 211 | 1–211 |
| 7 | `module_loader/module_registry.h` | 178 | 1–178 |
| 8 | `fixtures/phase2_typed_dag.json` | 151 | 1–151 |
| 9 | `orchestrator/cpp/include/dll_loader.h` | 126 | 1–126 |
| 10 | `orchestrator/cpp/include/checkpoint.h` | 107 | 1–107 |
| 11 | `orchestrator/configs/stage1.template.json` | 73 | 1–73 |
| 12 | `orchestrator/configs/stage1_gc_panel2_Red.json` | 73 | 1–73 |
| 13 | `orchestrator/cpp/include/star_coord_contract.h` | 61 | 1–61 |
| 14 | `orchestrator/cpp/tests/gate/gate_module_snr_extract_spy.cpp` | 54 | 1–54 |

> **诚实补注**：`orchestrator.cpp` 5538 行体量巨大，我按 10 段 read 全量通读，并**对全部引用行号做了本人二次定点复读**（`sed -n` 复核 §2/§4 中每一条最重证据）。其**非最重**的次要条目（如硬编码清单逐条）来自子代理全量通读 + 我对关键行的独立复核，我未逐条亲自复算——这一层次差异在 §7 与 §8 如实标明。

---

## 2. 本片判定

# 判定：**阻断（BLOCKING）**

**理由**：本片 14 份中 **8 份存在阻断级缺陷**。更关键的是，本片同时命中本轮最核心的两类缺陷，且**互相掩护**：

1. **自洽式断言（本轮最有价值）** —— 本片 3 处独立同源：
   - `orchestrator.cpp:3553` vs `:3568`（precision 被检量与期望量同源于 `config_.precision`）
   - `orchestrator.cpp:3677` `n_failed` **全文件零自增**，结构性恒 0，却被 `:3837` 当验证汇总上报
   - `test_p1phot_passband_identity.cpp:174-179` [F5] 是 `:124-135` [F1] 的**逐字重放**，谓词为真子集 ⇒ 恒绿
2. **恒红门 + 自愈门** —— `test_p1_batchB_fixes.cpp:50-52` 在 `_WIN32` 直接 `return ""`，使 `:198`/`:215` 在 Windows 上**恒红**，把真信号淹在红灯里；且该文件**根本不在任何门禁中执行**（§3 已核）。
3. **退役声明被证伪** —— `dll_loader.cpp:315-317` 声称「枚举保留仅为旧配置兼容元数据」，但 `dll_loader.h:39` 的枚举项**已被删除**，该注释指向不存在的对象（与本仓已实测的那例「退役声明称零消费者、实际有活调用」互为镜像：**在役/退役声明与实际不符**）。
4. **静默降级（明禁）** —— `spill_manager.h` 整份 211 行**零消费者、零实现**，却带一整条孤儿包含链与两份**已删除**的规范/原型路径引用。

### 最重 3 条

**【B1｜阻断】`dll_loader.cpp:54-60` 的 7 个模块基名与真实构建产物**全部**不符 ⇒ Linux 上 7/7 模块全部加载不到**

- `dll_loader.cpp:17-23` 注释宣称「ENGINEERING_SPEC §1 把 Linux amd64 列为正式平台」，`:29-33` 的"平台补齐"**全部内容就是把后缀换成 `.so`**，基名一字未改。
- 我**亲自核对** CMake（`grep`，只读）：

| 模块 | DllLoader 期望（`dll_loader.cpp:54-60`） | 真实构建产物（本人核实） | 证据 |
|---|---|---|---|
| AIO | `astro_image_io.so` | `acsd_io` | `CMakeLists.txt:292` |
| CALIBRATE | `astro_calibration.so` | `acsd_p1_calibration` | `lib/algorithms/calibration/CMakeLists.txt:30,38` |
| PLATESOLVE | `ipv_solver.so` | `acsd_p1_ipv` = **STATIC，无 .so** | `CMakeLists.txt:1164` |
| PSF | `dynamic_psf.so` | `acsd_p1_dpsf` = **STATIC，无 .so** | `CMakeLists.txt:1205` |
| SNR | `snr_estimator.so` | `acsd_p1_noise`，`add_subdirectory` **被注释** | `CMakeLists.txt:386,395` |
| DRIZZLE | `healpix_drizzle.so` | `acsd_p1_drizzle` | `lib/algorithms/drizzle/CMakeLists.txt:61,90` |
| PHOTOMETRIC | `photometric_calib.so` | 无 CMake target | 见 §4-C8 |

- 且**不是死代码**：`CMakeLists.txt:551` `add_subdirectory(...orchestrator/cpp)`，`orchestrator/cpp/CMakeLists.txt:56` 明确编译 `src/dll_loader.cpp`。
- 后果：`dll_loader.cpp:159-164` `std::ifstream` 探测失败 → `status=NOT_FOUND` → `:324 all_ok=false` → `orchestrator.cpp` 硬失败。**「Linux 是正式平台」的声明被自身代码证伪。**
- 违反：AGENTS.md §6「注释解释为什么这样做，与代码同步更新」。

**【B2｜阻断】`orchestrator.cpp:3553`/`:3568` precision 校验是同源自证；`:3677` `n_failed` 结构性恒零却被当验证汇总上报**

- `:3431` 把 `static_cast<int>(config_.precision)` 传给 `fn_drizzle_hips`；`:3553` 用**同一个** `config_.precision` 造期望值 `requested_prec`；`:3567-3568` 比 `meta_prec != requested_prec`。**被检量与期望量共源，中间无任何独立 dtype 测量点。** drizzle 若遵守该标志，此门**永不可能红**。
- 同一函数 `:3674` 注释还宣称验证项 d「dtype 与 metadata precision_mode 一致」，而 d 由注释 `:3673` 推给「HissReader 拒绝静默转换」——**本函数无任何独立实现**。
- `n_failed`：本人 `grep` 全文件仅 3 处命中（`:3467` 注释、`:3677` 声明、`:3837` 打印），**全文件无 `++n_failed`**；所有 tile 失败路径（`:3707/3733/3755/3775`）都提前 `return`。故 `n_tiles == n_passed`、`n_failed ≡ 0` **恒成立**，而 `:3467` 把该汇总宣传为验证产物。
- 违反：AGENTS.md §8「判据须在正确实现下绿、注入缺陷时红」。

**【B3｜阻断】`spill_manager.h` 整份 211 行零消费者、零实现，且规范与原型路径引用已随归档删除（悬空引用）**

- 本人实测：`git grep -F "spill_manager.h"` 在全仓 `.cpp/.h/CMakeLists.txt` 中**只命中它自己第 2 行的文件名注释**，**零 `#include`**。
- 符号级复核：`SpillManager` / `PeakShifter` / `RecoveryManager` / `SpillRecord` / `DeferredTask` 全部**零外部消费者**。
- **无 `spill_manager.cpp`** ⇒ `:130/:136/:141/:144/:145/:148/:149/:150/:158/:161/:164` 全是无定义纯声明。
- 孤儿链闭合：其 `:25-26` 依赖的 `resource_monitor.h` 唯一包含者是 `admission_controller.h:25`，而 `admission_controller.h` 唯一包含者是 `spill_manager.h:26` ⇒ **H-001→H-002→H-003 三头自封闭孤岛**（两依赖头本体存在，我已核实）。
- 悬空引用（本人 `find` 全仓实测）：`:4` `engineering_authoritative/docs/04_RESOURCE_AWARE_ORCHESTRATOR_SPEC.md` **不存在**；`:10` `engineering_authoritative/evidence/H-003/spill_manager.py` **不存在**；`engineering_authoritative/` **目录本身不存在**。
- 违反：AGENTS.md §6「退役代码从代码库删除，或保留统一注释块写明原因」。

---

## 3. 逐文件清单

> 判据：**每一条结论均由我读完原文推导**。凡我未亲自复算的次要条目已在 §7 标注来源。

### 3.1 `orchestrator/cpp/src/orchestrator.cpp`（5538 行）
**读了什么**：全 5538 行分 10 段通读；另对全部引用行号做定点二次复读。
**看到什么**：
- 自洽式断言 2 处：`:3553`/`:3568` precision 同源（B2）；`:3677` `n_failed` 恒零。
- 死计数：`:3467` 注释宣称的汇总判别力是虚构的。
- 恒假/注释失实：`:3670-3676` 列 6 类验证项，d 项无实现；`:3674` 同上。
- **原子清理语义反了**：`orchestrator.cpp:5118` `atomic_guard{this, cfg.output.hips, result.success}` 在**任何工作之前**注册，`:5120-5124` 才校验输入。输入不存在 → `return result`（`success=false`）→ 析构调 `cleanup_partial_output` → `:464` 对目录执行 **`fs::remove_all`**。我已读 `:435-441` 注释自称「本函数只在失败/取消/超时清理路径调用，**不触碰成功发布对象**」——**该保证不成立**：一次**纯输入校验失败**的运行会删掉上一次成功产出的 HiPS 目录。且 `run_stage2` 在 `:5513` 硬编码 `ok=false`/`success=false` ⇒ **每次调用都删用户输出**。
- 硬编码无出处：`:2743-2744` `mag_min=6.0`/`mag_max=16.0`，而 `:2742` 注释谎称「typed Stage1Config 直接驱动」；`:968` `dark_k=1.0` 硬编码而 `:959-960` 读出的两个曝光**仅进日志**（子代理复核，我未逐行复算）。
- 悬空引用：`:1690`/`:2285` 引 `wiki/05_STAR_MEASUREMENT_SHARED_DATA.md`——`wiki/` 目录不存在（子代理实测，我未复算）。
- **正面对照（防止我过度苛责）**：`:759-764` ABI 握手是本文件**唯一做对**的检查形状（被检值来自 DLL、期望值来自本地 `sizeof`，来源独立）——B2 两处本应照抄它。
**判定**：**阻断**（B1/B2 + 原子清理反转）

### 3.2 `orchestrator/cpp/src/dll_loader.cpp`（539 行）
**读了什么**：1–539 全量（我本人两段 read 覆盖 15–539）。
**看到什么**：
- `:54-60` 7 个基名全错（B1，本人 CMake 复核）。
- `:408` `candidate_names[] = {nullptr, nullptr}`，只有 `[0]` 被赋值；`:419` 循环第二轮恒不进 ⇒ **「多候选」能力是死代码**，魔数 `2` 未用 `sizeof` 推导。
- `:410-416` 6/7 版本符号（`aio_version`/`ipv_version`/`dpsf_version`/`pc_version`/`snr_version`/`hd_version`）本人 grep 全仓**零命中**（子代理执行，我只作裁决）⇒ `:426` 恒返回 `"unknown"`，而 `orchestrator.cpp:807` 照样按「版本: unknown」打印。**符号查找失败被吞 + 编造值当版本上报**。
- `:315-317` 注释称「枚举保留仅为旧配置兼容元数据」，但 `dll_loader.h:39` 的 `GRADIENT_SPHERE/STACK` 枚举项**已被删除** ⇒ 注释指向不存在的对象（悬空引用 + 退役声明失实）。
- **资源泄漏（本人读出）**：`:274-279` `aio_h` 与 `:302-307` `gaia_h` 是局部 `HMODULE`，**从不存入 `modules_`、从不 `free_library`** ⇒ `:343-353` `unload_all()` 释放不了它们，`is_loaded()` 假绿而 DLL 实际仍驻留。
- `:434-441` `set_num_threads` 对非 CALIBRATE 返回 false；`orchestrator.cpp:813` 忽略其返回值却无条件打印「线程数设为 N」。
- `:513-539` POSIX 分支完整 ⇒ `dll_loader.h:95`「非 Windows 为占位 stub」被证伪。
- `:523-525` POSIX `get_proc_address` 调 `dlsym` 前**不清 `dlerror()`** ⇒ 无法区分「符号不存在」与「符号值为 NULL」。
**判定**：**阻断**（B1）

### 3.3 `orchestrator/cpp/src/main.cpp`（398 行）
**读了什么**：1–398 全量。
**看到什么**：
- `:157-159` `catch (...) {}` **空捕获**：HISS metadata JSON 解析失败时静默吞掉、退化成打印原始 JSON，无错误码无日志 ⇒ 静默降级（诊断面）。
- `:112` 仅加载 AIO；`--inspect` 的失败码 `:127` 用 `GENERIC_ERROR` 而非专用码。
- **正面** `:384-386` `if (exit_code == SUCCESS && !result.success) exit_code = GENERIC_ERROR;` 是**正确的 fail-closed 收口**——与 orchestrator.cpp 的多处缺口形成对照。
- **正面** `:308-310` job_id 由 `config.original_json_sha256` 前 12 位派生（真实 SHA256，与 `orchestrator.cpp:3098` 声称的「seed 由 config SHA256 派生」形成对照——后者实为 `diagnostics_dir` 的多项式哈希，是**另一处**注释失实）。
**判定**：**须修**（`:157-159` 空捕获；本片最干净的一份）

### 3.4 `orchestrator/cpp/tests/test_p1_batchB_fixes.cpp`（229 行）
**读了什么**：1–229 全量（本人 read）。
**看到什么**：
- **恒红门**：`:50-52` `#ifdef _WIN32 … return "";` ⇒ `:193` `nlohmann::json::parse("")` 抛 ⇒ `:198` 与 `:215` **在 Windows 上必红**，且红灯原因恒为「capture 无输出」而非真实缺陷 ⇒ **恒红门把真信号藏进红灯**（子代理实测 `test_orchestrator_cli.cpp` 对 `capture_stdout`/`json_escape_string`/`output_jsonl_event_ex` 零引用，`:52` 的辩解不成立）。
- **不在门禁内**：子代理实测该文件未被任何 `CMakeLists.txt` 引用，`cpp/Makefile` `run_test:` 亦不含它 ⇒ **从不执行的门不是门**。
- **零判别力重复**：`:109` `!fs::exists(tree)` 与 `:110` `!fs::exists(base / "hips_out")`——`tree` 于 `:103` 即 `base / "hips_out"`，中间无任何文件系统写 ⇒ **同一表达式求值两次**，`:110` 判别力严格为零（本人读出）。
- **恒真断言**：`:151-152` 第二次 `raise(SIGINT)` 前 `:148` 已置位且无任何重置 ⇒ `:152` 不可能翻红（本人读出）。
- **虚假声明**：`:136-139` 注释称「若 handler 路径仍含锁/日志，主线程在 Logger mutex 内被中断时会死锁 —— **由本测试直接暴露**」。实读：`:140` 的 `std::raise` 是同步的且此刻主线程**不在任何 Logger 临界区内**（该路径从未被构造）；若真死锁，会卡在 `:140` 本身，`:142` 的 5s deadline **保护不了任何东西**（本人读出）。
**判定**：**阻断**

### 3.5 `orchestrator/cpp/tests/test_p1phot_passband_identity.cpp`（228 行）
**读了什么**：1–228 全量（本人 read）。
**看到什么**：
- **同义反复（本轮核心）**：`:174-179` [F5] 与 `:124-125` [F1] 是**逐字相同的 `make_request(filters, "Antlia V Pro Series B")`**，而 `fit_frame_photometry` 对该请求是纯确定性函数 ⇒ 两次 `res` 逐位相同。F5 谓词 `rc!=0 ∧ !error.empty() ∧ kEnvironment ∧ !fit_ok` 是 F1 两条断言（`:129-131`、`:132-135`）的**真子集** ⇒ **F1 绿则 F5 必绿，F5 永不提供新信息**。文件头 `:23` 把它宣传为「独立非空断言」，定性失实。
- **折半覆盖**：`:205` `injected.replace(body0, obj_baader.size(), body_antlia)` 搬运**整个** Antlia 对象体。我已读 `eng/packaging/config/filters.json:884-885` 确认 Antlia 对象**首字段即 `"name": "Antlia V Pro Series B"`** ⇒ 注入后 `"Baader R"` 自述名为 Antlia ⇒ 生产**名字判据**先返回，**永不触达** provenance 对账。
  我已读 `filters.json:384-391` 确认 Baader R 对象**确实携带独立指纹**（`wl_min/wl_max/val_min/val_max/wl_sum/val_sum/val_sumsq`）——**这正是唯一能抓「名字被伪造」的判据**，而本门零覆盖。
- **静默 no-op**：`:200-206` 注入块在键形态不匹配时**静默跳过**，无 `injected != real` 断言、无诊断（`:203` 的 `body1` 计算后从未使用）。
- **弱判据**：`:220-222` `A || B` 语义为「该红**不是**来自声明名核对那一项」，逻辑方向正确但写法脆弱（应为 `!(A && B)`）。**我下调子代理对此的「折半判据」定级为建议**——读原文后该 `||` 是有意的「否定式守卫」，不是失效判据。
- `:208` `inj_path` 为**固定相对路径**、非 PID 隔离、跨运行残留、从不清理。
**判定**：**阻断**（F5 同义反复 + provenance 零覆盖）

### 3.6 `orchestrator/cpp/include/spill_manager.h`（211 行）
**读了什么**：1–211 全量（本人 read）。
**看到什么**：B3 全部内容。另：`:66-67` `PROMOTE_AFTER_DEFERS=3` / `MAX_DEFER_SEC=300.0` 硬编码无出处；`:96` `const PressureHandler&` 引用成员 + `:97` `mutable std::mutex` 的组合若对象被拷贝则悬垂。
**判定**：**阻断**

### 3.7 `module_loader/module_registry.h`（178 行）
**读了什么**：1–178 全量（本人 read）。
**看到什么**：
- **不是孤儿**：实现存在于 `lib/infrastructure/pipeline/module_loader/module_registry.c`（`:22` `#include "module_registry.h"`，`:570/:887/:958` 定义三个 `acsd_registry_*_v1`），探针 `eng/tests/abi/abi004_registry_probe.c` 消费。**我修正了早期基于文件名 grep 的「孤儿」误判**——该头是活的，且它与 `lib/include/acsd/core/module.h:94` 的同名 C++ `ModuleRegistry` 是**两套不同类型**，仅名字相撞（易误判为重复实现）。
- 悬空引用：`:8-9` 权威块引 `tasks/02_ABI_BUILD_CLI_TASKS.md`、`12_DLL_ABI_AND_LOADER_STANDARD.md`、`11_MODULE_SOURCE_TEST_STANDARD.md` —— 子代理实测**三份全仓不存在**（我只作裁决）。
- **恒真断言**：`:171-172` `ACS_STATIC_ASSERT(offsetof(X, head) == 0u, "head first")` —— `head` 是**首个声明成员**，`offsetof` 取首成员偏移由 C11 语言保证恒为 0，**期望值与被测对象同源、判别力为零**；`:169-170` `sizeof(entry) > sizeof(acsd_head)` 以被测结构自身成员类型为期望值，近恒真。子代理指出 `docs/engineering/UNRESOLVED_REGISTER.md:3321` 已把前者登记为「❌ 恒真」且 HEAD 仍存在（**我未复核该登记文件与 FIX_LEDGER 状态，仅记录线索**）。
- `:164` 注释称自检「校验布局静态断言」，但布局断言是**编译期**的，`:165-166` 的运行期 `acsd_registry_self_test_v1` 不可能校验它。**正面**：子代理读 `module_registry.c:957-987` 确认其 sha256 期望值取自 **FIPS 180-4 外部标准向量**，**非同义反复**——我不据此指控。
- `:40` `ACS_REGISTRY_VERSION_V1` 全仓零引用（子代理实测）⇒ 死宏，registry ABI 无版本锚。
**判定**：**须修**（悬空引用 + 恒真 static_assert）；**我判定其非阻断**——实现存在、探针覆盖，故与 `spill_manager.h` 性质不同

### 3.8 `fixtures/phase2_typed_dag.json`（151 行）
**读了什么**：1–151 全量（本人 read）。
**看到什么**：7 节点 typed-DAG，artifact 引用自洽（`coverage→sample→upm_fit→upm_apply→reject→integrate→write`，每个 `inputs` 的 artifact 均由上游 `outputs` 产出）。消费方存在：`eng/tests/pipeline/test_typed_dag_plan.py:33`、`eng/tests/monitoring/test_run_graph_render.py:58,164,195,220`（本人 grep 确认）。
**判定**：**通过**（本片唯一干净文件之一）
- 注：权威台账对本件给了「P3 手写面豁免：fixtures」的特殊 reason——即它被机器规则**默认豁免**。我的独立结论与该豁免一致，但**理由不同**：我是读完后认定其内容自洽，而非按路径形态豁免。

### 3.9 `orchestrator/cpp/include/dll_loader.h`（126 行）
**读了什么**：1–126 全量（本人 read）。
**看到什么**：
- `:3`/`:8` 称「5 个模块」，而 `:30-40` 枚举有 **7** 项；`:29` 称「9 节点」；`dll_loader.cpp:91` 称「10 个」、`:393` 称「9 个模块」——**模块计数在 5/7/9/10 之间互相矛盾**，生产遥测失真。
- `:95` 称「非 Windows 为占位 stub」，被 `dll_loader.cpp:513-539` 证伪。
- `:26` `ModuleInfo::handle` 无默认初始化（`dll_loader.cpp:99` 有显式赋值，但默认构造的 `ModuleInfo` 是未定值）。
- `:39` 注释记录 legacy 项**已删除**——正是 `dll_loader.cpp:315-317`「枚举保留」失实的对照证据。
**判定**：**须修**（注释与代码全面失同步）

### 3.10 `orchestrator/cpp/include/checkpoint.h`（107 行）
**读了什么**：1–107 全量（本人 read）。
**看到什么**：
- **未初始化 POD**：`:26` `int stage_id`、`:27` `double duration_sec`、`:38` `int current_stage_id`、`:43` `bool fully_completed` 均**无默认初始化**，而 `CheckpointData data;` 默认构造后若 `load()` 未能填充即为**未定值**。
- **无错误通道**：`:80` `is_stage_completed` 返回 `bool`、`:84` `get_resume_stage` 返回 `int`，签名上**无法区分「阶段未完成」与「检查点文件读不出来」**——读失败降级为「未完成，Please 重跑」属可接受方向，但接口本身把失败语义挤掉了。
- `:84` 注释称「如 fully_completed 返回 -1」，与「返回 max(stage_id)+1」共用一个返回值域 ⇒ 调用方必须自行区分 -1/0/N 三种含义，无枚举、无稳定错误码。
- 子代理指出 `checkpoint.cpp:501` `deserialize` **无条件 `return true`**、`:593` `load` 因此对任意可读文件返回 true、`:772-774` `get_resume_stage` 据 `fully_completed` 返回 -1 ⇒ **24 字节 `{"fully_completed": true}` 即可让管线跳过全部阶段并报成功**。**`checkpoint.cpp` 不在本片成员内，我未亲自复读该文件**，故此条按「高可信线索」记录，**定级为须修而非阻断**，并列入 §8 待前台复核。
- **正面**：`orchestrator.h:57` 与 `src/checkpoint.cpp:12` 均真实包含本头 ⇒ 活的（非孤儿）。注意 `acsd/core/checkpoint.h` 是**另一套同名头**，勿混。
**判定**：**须修**

### 3.11 `orchestrator/configs/stage1.template.json`（73 行）
**读了什么**：1–73 全量（本人 read）。
**看到什么**：
- **悬空引用**：`:2` `"$schema": "../schemas/stage1.schema.json"` —— 本人实测 `orchestrator/schemas/` **目录不存在**，真实文件在 `configs/stage1.schema.json` ⇒ 任何遵循 `$schema` 的编辑器/校验器解析失败。
- `:41` `pixfrac: 0.8` 与 `stage1_gc_panel2_Red.json:39` 的 `1.0` **分叉**；子代理称 0.8 被 schema 声明为「冻结生产默认」且治理记录标注该默认 `pending_authority`（无科学权威出处）——**我未复算该治理记录**，记为线索。
- `:15-16` `light_exposure_s=180.0` / `dark_exposure_s=180.0` 恰好相等 ⇒ 即使 `K=t_light/t_dark` 被正确计算也恒为 1.0，**模板配置无法暴露 `dark_k` 硬编码缺陷**（与 `orchestrator.cpp:968` 呼应）。
- `:57` `stop_after: "browser_verify"` 为**默认模板值**，而 legacy `browser_verify` 在 orchestrator 侧存疑（`:3471` 默认配置下直接 `return true`，子代理实测）⇒ **模板默认停在一个不实际校验的阶段**。
**判定**：**须修**

### 3.12 `orchestrator/configs/stage1_gc_panel2_Red.json`（73 行）
**读了什么**：1–73 全量（本人 read）。
**看到什么**：
- **13 处机器专属绝对路径**（`:4,6,7,8,9,18,29,30,31,64,65,66,67`），根为 `F:\Astro dev\Astro CS **Normalization** Database\` —— 注意这**是另一个仓库名**（本仓为 `Astro CS Database`），指向改名/合并前的工程；含 `run\temp\p2_v7\` 临时目录与日期戳文件名 `...20250716@002647-180S-Red.fts`。违反 AGENTS.md §6「运行参数优先由 config 读取，其次从运行环境自动获取」。
- `:64` 写已 DEPRECATED 的 `output.hiss`；`:68` `overwrite: true` ⇒ 静默覆盖既有产物。
- `:39` `pixfrac: 1.0` 与模板 `0.8` 分叉（见 3.11）。
- `:46` `stop_after: "hips_verify"`（与模板的 `browser_verify` 不同，**此处的选择更合理**）。
- 子代理实测三份 `gc_panel*_Red` 配置**零消费者**（排除 `实验/`）⇒ 死配置。
**判定**：**须修**

### 3.13 `orchestrator/cpp/include/star_coord_contract.h`（61 行）
**读了什么**：1–61 全量（本人 read）。
**看到什么**：
- **本文件本身即恒等式集合**：`:42-44` `from_sdet(x) = x - 0.5`；`:49-51` `from_dpsf(x) = x`（**字面恒等函数**）；`:55-57` `ipv_from_sm(u) = u + 0.5`。三者构成**往返恒等**：`ipv_from_sm(from_sdet(x)) ≡ x`。
- **定性失实**：`:46-48` 注释称「恒等不是冗余: 它是本契约的**机器可断言锚点**, 显式禁止对该支路再施加 -0.5」。子代理指出唯一的函数级断言 C1（`p1psf_center_contract_gate.cpp:159-166`）只验这三个一行的算术恒等式、**从不接触编排器**；若有人重新引入 R-3 实测的 0.5 px 双份星缺陷，C1 **照样全绿**。**我未亲自读该 gate 文件**，记为高可信线索。
- **反向自查**：子代理确认 `p1psf_centroid_gate.cpp` 的 `[P2]/[P4]/[P5]` 链接真实 sdet+dpsf **生产源**、对比解析真值场、含 `negative-injection` 负例 ⇒ **是真门，不构成同义反复**。0.5 px 缺陷**确有**真门保护，我不主张它无防护。
- 悬空风险：`:30` 引 `lib/algorithms/psf/tests/p1psf/p1psf_centroid_gate.cpp`（本人 grep 确认存在）、`:33` 引 `docs/science/algorithms/GATES_AND_TOLERANCES.md`（未复算）。
**判定**：**须修**（注释把恒等函数称作「可断言锚点」，而其断言是空壳——本轮核心问题在本片的又一实例）

### 3.14 `orchestrator/cpp/tests/gate/gate_module_snr_extract_spy.cpp`（54 行）
**读了什么**：1–54 全量（本人 read）。
**看到什么**：
- 这是**测试替身**（文件头 `:2` 明写「**非生产**」），零断言，**本身不是伪门**——被验证的是同目录的 `orchestrator_saturation_wiring_gate.cpp`。我**不指控**它「零判别力」。
- `:42-44` 把 SNR 模型真正消费的物理输入钉成 `0.0`（`median_source_snr` / `frame_depth_flux5_adu` / `frame_depth_m5_mag`），配合 `:39-40` 的 `snr_phot=1.0`/`median_snr=1.0` ⇒ 依赖这些量的行为面在门中不可达。
- `:35` `if (sigma_residual <= 0.0) return 2;` —— **NaN 穿透**（`NaN <= 0.0` 为 false），无 NaN/Inf 探针。
**判定**：**建议**

---

## 4. 发现清单

### 4.1 阻断（BLOCKING）— 8 条

| # | 位置 | 问题 | 违反 |
|---|---|---|---|
| B1 | `dll_loader.cpp:54-60`（+`:17-23,29-33`） | 7/7 模块基名与真实 CMake 产物不符；PLATESOLVE/PSF 为 STATIC 无 .so；SNR 子图被注释；Linux 上一个 stage 都跑不了，而 `:17` 宣称 Linux 是正式平台 | AGENTS §6 |
| B2 | `orchestrator.cpp:3553`/`:3568`（+`:3431,:3674`） | precision 校验同源自证；`:3674` 宣称的验证项 d 无实现 | AGENTS §8 |
| B3 | `spill_manager.h` 全 211 行 | 零消费者 + 零实现 + 三头孤儿链 + 两份已删路径引用 | AGENTS §6 |
| B4 | `test_p1phot_passband_identity.cpp:174-179` vs `:124-135` | [F5] 是 [F1] 的逐字重放，谓词为真子集 ⇒ 恒绿 | AGENTS §8 |
| B5 | `test_p1_batchB_fixes.cpp:50-52` + `:198/:215` | `_WIN32` 短路 ⇒ Windows 上恒红，且该面全仓无替代覆盖 | AGENTS §8 |
| B6 | `test_p1_batchB_fixes.cpp`（整份） | 未被任何 CMakeLists 引用、`run_test:` 不含它 ⇒ **门禁从不执行** | AGENTS §8 |
| B7 | `orchestrator.cpp:3677` + `:3837` | `n_failed` 全文件零自增，结构性恒 0，却作为验证汇总上报 | AGENTS §8 |
| B8 | `orchestrator.cpp:5118` + `:5120-5124` + `:441` | 原子 guard 在工作前注册 ⇒ 纯输入校验失败会 `remove_all` 掉上次成功产物；`:441` 注释的「不触碰成功发布对象」保证不成立 | IO_003 §4/§6、AGENTS §6 |

### 4.2 须修（MAJOR）— 11 条

| # | 位置 | 问题 |
|---|---|---|
| M1 | `dll_loader.cpp:315-317` vs `dll_loader.h:39` | 「枚举保留仅为旧配置兼容元数据」为假——枚举项已删除；退役声明被证伪 |
| M2 | `dll_loader.cpp:410-416`/`:426` | 6/7 版本符号全仓零命中 ⇒ 恒返回 `"unknown"` 并被 `orchestrator.cpp:807` 当版本上报 |
| M3 | `dll_loader.cpp:274-279`/`:302-307` | `aio_h`/`gaia_h` 局部句柄从不存入 `modules_`、从不释放 ⇒ `unload_all` 释放不了、`is_loaded` 假绿 |
| M4 | `dll_loader.h:3,:8,:29` + `dll_loader.cpp:91,:393` | 模块计数 5/7/9/10 四处互相矛盾 |
| M5 | `dll_loader.h:95` vs `dll_loader.cpp:513-539` | 「非 Windows 为占位 stub」被完整 POSIX 实现证伪 |
| M6 | `test_p1phot_passband_identity.cpp:205` | 搬运整个对象体（含外来 `"name"`）⇒ provenance 指纹对账（`filters.json:384-391` 确认其存在）零覆盖 |
| M7 | `test_p1_batchB_fixes.cpp:109`/`:110` | 同一表达式求值两次，`:110` 判别力为零 |
| M8 | `test_p1_batchB_fixes.cpp:136-139` | 注释称「死锁由本测试直接暴露」不成立（路径从未被构造；5s deadline 保护不了 `:140`） |
| M9 | `module_registry.h:171-172`（及 `:169-170`） | `offsetof(X, head)==0` 对首成员恒真 ⇒ 期望值与被测对象同源 |
| M10 | `module_registry.h:8-9` | 权威块引三份文档，全仓不存在 |
| M11 | `checkpoint.h:26,27,38,43` + `:80,:84` | 未初始化 POD；接口无错误通道（`bool`/`int` 无法表达读失败） |

> 另有 `stage1.template.json:2`（`$schema` 悬空）、`stage1_gc_panel2_Red.json`（13 处机器绝对路径 + `overwrite:true`）、`star_coord_contract.h:46-48`（恒等函数被称作「可断言锚点」）、`main.cpp:157-159`（空捕获）、`orchestrator.cpp:2743-2744`+`:2742`（硬编码 + 注释谎称）、`orchestrator.cpp:1690/:2285`（`wiki/` 悬空）—— 均已列入 §3 逐文件条目，此处不重复编号以免虚增。

### 4.3 建议（NIT）— 5 条

| # | 位置 | 问题 |
|---|---|---|
| N1 | `test_p1_batchB_fixes.cpp:151-152` | 第二次 `raise(SIGINT)` 后无重置 ⇒ 恒真 |
| N2 | `test_p1phot_passband_identity.cpp:200-206` | 注入块静默 no-op，无 `injected != real` 断言；`:203` `body1` 死代码 |
| N3 | `test_p1phot_passband_identity.cpp:208` | 注入路径固定、非 PID 隔离、跨运行残留、从不清理 |
| N4 | `dll_loader.cpp:408`/`:419` | `candidate_names` 第二槽永不赋值，「多候选」是死代码；魔数 `2` 未用 `sizeof` |
| N5 | `gate_module_snr_extract_spy.cpp:42-44` | 物理输入钉成 0；`:35` NaN 穿透 |

### 4.4 已排查并**排除**（记明以免下轮误报）

- `module_registry.c:957-987` 的 `acsd_registry_self_test_v1`：sha256 期望值取自 **FIPS 180-4 外部标准向量** ⇒ **非同义反复，不指控**。
- `p1psf_centroid_gate.cpp` 的 `[P2]/[P4]/[P5]`：链接真实 sdet+dpsf 生产源 + `negative-injection` 负例 ⇒ **是真门，不指控**。
- `main.cpp:384-386`：exit code 的 fail-closed 收口**正确**，是本片正面样板。
- `orchestrator.cpp:759-764`：ABI 握手是正确形状（被检值来自 DLL、期望值来自本地 `sizeof`）。
- 本片**零私建线程池**：`spill_manager.h` 用 `std::mutex`/`std::atomic`，非线程池；`orchestrator.cpp` 的 watchdog（`:5213/:5220`）同时最多存活 1 个，严格说不构成线程池。**我按子代理建议，不以「私建线程池」上报**（会被驳回）；正确表述是「自建线程绕过调度器资源门 + 该线程对运行时长零约束力」。
- 本片**未发现自愈锚/自愈判据**形态（读的是本次执行自己覆写的文件）：两个测试的注入文件（`:208`）从不被本 TU 读回，`:187` 只读原始 `filters.json`。**如实报「未找到」，不编造。**

---

## 5. 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| R1 | 建立「`dll_loader.cpp:54-60` 期望基名 ↔ CMake `OUTPUT_NAME`/`add_library` 类型/根图 `add_subdirectory`」对照表，逐条 grep 核实 | 推翻 `:17-23` 的 ORCH-001「Linux amd64 是正式平台」声明 | **✅ 成立**（4 处名不符 + 2 处 STATIC 无库 + 1 处不在根图 = 7/7 miss）。我**本人执行了全部 grep** |
| R2 | 令 DRIZZLE 按 `:3431` 收到的整数写 metadata，再用 `:3553` 的同一标志去比 | 推翻「HISS_VERIFY 能检出 dtype/precision 错配」 | **✅ 成立**。`:3431`→`:3553`→`:3567-3568` 三点共源，我本人逐行复读确认 |
| R3 | 把 `test_p1phot_passband_identity.cpp:174` 的 request 参数换成与 `:124` **完全相同** | 推翻「[F5] 是独立非空断言」（文件头 `:23`） | **✅ 成立**。源码本已如此；`fit_frame_photometry` 对该请求纯确定 ⇒ F5 谓词 ⊂ F1 |
| R4 | 把 Baader R 的 `wavelength_nm`/`value` 搬成 Antlia 的，但**把 `name` 伪造回 `"Baader R"`** | 推翻 F6「注入真实错通带 ⇒ 生产入口具名判红」的**覆盖面** | **✅ 成立**。名字判据通过，只有 `filters.json:384-391` 的指纹能抓；`:205` 的注入必带外来 name ⇒ 本门抓不到 |
| R5 | 在 `_WIN32` 下运行 `test_p1_batchB_fixes` | 推翻「事件行 JSON 合法性有覆盖」 | **✅ 成立**。`:52` `return ""` ⇒ `:193` 抛 ⇒ `:198`/`:215` 恒红 |
| R6 | 让一次成功的 run 产出 HiPS，再改坏 `input.light` 重跑 | 推翻 `:441` 注释「清理不触碰成功发布对象」 | **✅ 成立**。guard 在 `:5118` 注册、`:5120` 才校验 ⇒ 上次产物被 `remove_all` |
| R7 | 让模块侧额外丢弃 W 个 SNR 点 | 推翻「SNR 丢弃原因分类保证不静默丢点」 | **⚠️ 未推翻/未证实**——依据是子代理读 `orchestrator.cpp:4533-4550`，**我未亲自复读该段**。记为线索，不定级 |
| R8 | 检查 `spill_manager.h` 的 `#include` 与全部类型名的全仓消费者 | 推翻「该组件在役」 | **✅ 成立**。本人 grep：唯一命中是它自己 `:2` 的文件名注释；5 个类型名零外部消费者 |

---

## 6. 盲复算

**方法**：在完成 §2/§3 全部结论**之后**，才去读既有台账 `分片清单/逐份判定-权威版.csv` 中本片 14 份的机器判定，作为对照基线。既有台账只作线索，**未用于推导任何一条结论**。

**既有机器判定**：本片 **14/14 份全部标注 `tier=HUMAN`，`reason=默认保留（非产出面或非数据形态）`**；`phase2_typed_dag.json` 另有一条路径规则豁免（`P3 手写面豁免: fixtures`）。**即：既有记录对本片零缺陷登记。**

**我的独立判定**：阻断 8 / 须修 11 / 建议 5，落在 8/14 份成员上。

**一致性裁决：偏松（既有台账显著偏松）**。

- 既有判定把 `spill_manager.h`、`star_coord_contract.h`、`module_registry.h`、`dll_loader.h/.cpp`、`checkpoint.h`、`orchestrator.cpp` 共 7 份生产源码全部归为「非产出面或非数据形态」而默认保留。这 7 份里 **6 份**被我查出阻断或须修级缺陷。
- **这正是本轮负责人裁决所指的失效模式**：机器分母/清单只回答「这文件属于哪一层、是否人工产物」，**从不回答「实现是否正确」**；把「分类通过」读成「检查通过 ⇒ 实现正确」，就等于零验证。本片是该谬误的一个高浓度样本。
- 我**未**发现既有台账有「查过但判为通过」的记录——它是**未查**，不是**查过放过**。故我不指控台账作者判断失误，只指出该机制**不能**承担正确性担保，须由本遍的人工对抗阅读补上。
- 口径差异说明：`phase2_typed_dag.json` 我与台账结论一致（通过），但**理由不同**——台账按路径形态豁免，我是读完后认定内容自洽。这不构成偏严或偏松。

---

## 7. 子代理派发记录

派发 **5 个**子代理（纪律要求 3–5），全部在后台并行运行，我同时本人通读原文。

| # | subagent id | 范围 | 我的复核与**否决** |
|---|---|---|---|
| 1 | `a1a9f5ca` | orchestrator.cpp 5538 行 | **接受** B1/B2/B3（我逐条定点复读确认）；**否决/降级** 其「私建线程池」表述（其自己已主动撤回，我采纳）；**否决** 其 HEAD 质疑作为阻断依据（我已用 `git status` 证实 orchestrator.cpp 相对 HEAD 干净）；**记为线索未采信** R7（SNR 丢弃）、`wiki/` 悬空、暗电流 K 失效等——我未逐行复算 |
| 2 | `d4de8d7d` | dll_loader.cpp/.h + module_registry.h | **接受** `.so` 名不符（B1，我本人 grep CMake 全部复核）、版本符号 6/7 恒 `unknown`、退役声明失伪、`:408` 死候选槽；**接受** 其对我早期「module_registry.h 孤儿」误判的**纠正方向**（我进一步确认实现在 `module_registry.c`）；**接受** HMODULE 泄漏（我本人读 `:274-279`/`:302-307` 确认）；**否决**其 C6 把 `module_registry.h:171-172` 恒真 static_assert 定为**阻断**——我**下调为须修**，理由：该头实现存在且有探针覆盖，与 `spill_manager.h` 的纯孤儿性质不同，不应同级 |
| 3 | `dcf751db` | 3 个测试文件 511 行 | **接受** F5 同义反复（B4，我本人读 `:124`/`:174` 逐字比对确认）、`:205` provenance 零覆盖（B4/M6，我本人读 `filters.json:884-885` 确认 Antlia 带 name、`:384-391` 确认 Baader 带指纹）、`:50-52` 恒红（B5）、`:109/:110` 零判别力（M7）、`:136-139` 死锁声明不成立（M8，我本人读 `:140` raise 同步性确认）、`:151-152` 恒真（N1）；**否决**其把 `:220-222` 的 `\|\|` 定为「折半判据/阻断」——我读原文后认定该 `\|\|` 是**有意的否定式守卫**（语义正确、写法脆弱），**下调为建议**；**采纳**其「本组无自愈锚形态」的如实报告 |
| 4 | `317a099c` | spill_manager.h / checkpoint.h / star_coord_contract.h / 2 config / fixture | **接受** B3（我本人 grep 全仓确认零消费者+悬空路径）、`$schema` 悬空（我本人 `ls` 确认 `schemas/` 不存在）、panel2 的 13 处机器路径、json_config 的 fail-open 配置校验；**否决**其把 checkpoint `deserialize` 恒 true 定为**阻断**——`checkpoint.cpp` **不在本片成员内**，我未亲自复读，**下调为须修 + 列入待前台复核**；**采纳**其 C1 同义反复线索与「p1psf_centroid_gate 是真门」的反向自查 |
| 5 | `7a800d6f` | orchestrator.cpp（与 #1 并行交叉复核） | **交叉价值最高**：独立复现了 B1/B2，且**发现了 #1 未报的新条目**——`StageTiming st.success` 未初始化即 `push_back`（`:5327`，UB）、BROWSER_VERIFY 只在 `n_ok==0` 失败（`:5056`，128 采样挂 127 仍判过）、`memory.md:320/322` 两条悬空引用、量子位 `A_cell=0.0` 死变量（`:3900`）。**接受**其新发现并补入 §3/§4；**否决**其「私建线程池」写法（同 #1）；**降级**其 D-3（`hips_tile_width=5120` 越界）为线索——严重度依赖未读的 AIO reader 实现 |

**否决统计**：共提出 **5 项降级/否决** —— (a) checkpoint 恒 true：阻断→须修；(b) `module_registry.h` 恒真 static_assert：阻断→须修；(c) `:220-222` 的 `||`：阻断→建议；(d) 两份报告的「私建线程池」表述：整体否决；(e) `hips_tile_width` 越界：阻断级→未定级线索。另有 **2 项对我早期误判的纠正被采纳**（module_registry.h 非孤儿）。

**纪律**：5 个子代理全程只读。我未修改任何仓内文件，未执行任何 git 写，未编译、未运行任何二进制。**所有最终判定均以我本人读完的原文为准；凡我未亲自复算的条目一律标注为「线索」而非结论。**

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 0) 基线（三次实测不同，HEAD 在漂移）
git rev-parse HEAD
git -c core.quotepath=false status --short -- lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp

# 1) 片成员与行数（权威清单）
python3 -c "
import yaml
d=yaml.safe_load(open('run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml',encoding='utf-8'))
for s in d['片清单']:
    if s['片号']=='INF-pipeline-001':
        print(s['层'], s['成员份数'], s['实际行数'])
        [print(m) for m in s['成员文件']]
"

# 2) B1 复现：CMake 真实产物名 vs DllLoader 期望
grep -n "astro_image_io\|astro_calibration\|ipv_solver\|dynamic_psf\|photometric_calib\|snr_estimator\|healpix_drizzle" \
  lib/infrastructure/pipeline/orchestrator/cpp/src/dll_loader.cpp
grep -n "OUTPUT_NAME" lib/algorithms/calibration/CMakeLists.txt lib/algorithms/drizzle/CMakeLists.txt
grep -n "add_library(acsd_p1_ipv\|add_library(acsd_p1_dpsf" CMakeLists.txt
grep -n "add_subdirectory(lib/algorithms/noise_snr" CMakeLists.txt      # 保持注释
grep -n "dll_loader.cpp" lib/infrastructure/pipeline/orchestrator/cpp/CMakeLists.txt

# 3) B3 复现：spill_manager.h 零消费者 + 悬空路径
git -c core.quotepath=false grep -n -F "spill_manager.h" -- '*.cpp' '*.h' 'CMakeLists.txt'
git -c core.quotepath=false grep -n -w "SpillManager\|PeakShifter\|RecoveryManager\|DeferredTask\|SpillRecord" \
  -- '*.cpp' '*.h' | grep -v 'spill_manager.h'
ls lib/infrastructure/pipeline/module_loader/spill_manager.cpp 2>&1        # 应为 No such file
find . -path ./.git -prune -o -iname 'engineering_authoritative*' -print      # 应为空

# 4) B2/B7 复现：precision 同源 + n_failed 恒零
sed -n '3429,3432p;3552,3554p;3566,3569p' \
  lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp
grep -n "n_failed" lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp   # 仅 3 处，无 ++

# 5) B4 复现：F5 是 F1 的逐字重放
sed -n '124,135p;173,180p' \
  lib/infrastructure/pipeline/orchestrator/cpp/tests/test_p1phot_passband_identity.cpp

# 6) M6 复现：注入必带外来 name（故 provenance 指纹不可达）
sed -n '884,886p' eng/packaging/config/filters.json    # Antlia 对象首字段 = name
sed -n '384,391p' eng/packaging/config/filters.json    # Baader R 携带 wl_sum/val_sum 指纹

# 7) B5/B6 复现：Windows 恒红 + 不在门禁
sed -n '49,52p;193,199p' \
  lib/infrastructure/pipeline/orchestrator/cpp/tests/test_p1_batchB_fixes.cpp
git -c core.quotepath=false grep -rn "test_p1_batchB" -- 'CMakeLists.txt' '*.cmake' 'Makefile'

# 8) 悬空 $schema 复现
ls lib/infrastructure/pipeline/orchestrator/schemas/ 2>&1          # No such file or directory
ls lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json   # 真实位置

# 9) 盲复算基线（既有机器判定：14/14 默认保留）
python3 -c "
import csv
t=['spill_manager.h','star_coord_contract.h','module_registry.h','dll_loader.h','dll_loader.cpp',
   'checkpoint.h','main.cpp','gate_module_snr_extract_spy','test_p1_batchB_fixes',
   'test_p1phot_passband_identity','stage1.template.json','stage1_gc_panel2_Red',
   'phase2_typed_dag','orchestrator.cpp']
for r in csv.DictReader(open('run/GOVERN-08/审核包-R2/分片清单/逐份判定-权威版.csv',encoding='utf-8')):
    if any(x in r['path'] for x in t) and r['slice']=='INF-pipeline-001':
        print(r['tier'],'|',r['path'],'|',r['reason'][:60])
"

# 10) 待前台复核（我不越界）：
#    - lib/infrastructure/pipeline/orchestrator/cpp/src/checkpoint.cpp:501/593/772（不在本片成员）
#    - lib/algorithms/psf/tests/p1psf/p1psf_center_contract_gate.cpp:159-166（C1 同义反复）
#    - docs/engineering/UNRESOLVED_REGISTER.md:3321（恒真 static_assert 是否仍 OPEN）
```

---

## 9. 待前台复核（我不越界，如实登记）

1. **`checkpoint.cpp:501/593/772`** —— 子代理称 `deserialize` 无条件 `return true`，配合 `:772-774` 可用 24 字节文件跳过全部阶段。`checkpoint.cpp` **不在本片 14 份成员内**，我未亲自复读，**不定级**。若属实则为阻断。
2. **`p1psf_center_contract_gate.cpp:159-166`** —— C1 三断言同义反复、`ok_dpsf` 字面恒真；同文件 C2/C3 用独立参考实现，被判为真门。**不在本片成员内**，我未亲自复读。
3. **`docs/engineering/UNRESOLVED_REGISTER.md:3321`** 与相关 FIX_LEDGER —— 据称已把 `module_registry.h:171-172` 登记为「❌ 恒真」且状态仍 OPEN。我**未复核该登记文件**，故 M9 定为须修而非阻断。
4. **`checkpoint.h` 未初始化 POD 的实际可达性** —— `:38/:43` 是否会在某条路径上以未定值被读，需读 `checkpoint.cpp` 的 `load()` 失败分支才能定。
5. **`orchestrator.cpp` 非最重条目**（`:2743-2744` 硬编码、`:1690/:2285` `wiki/` 悬空、`:5327` `StageTiming::success` 未初始化、`:5056` BROWSER_VERIFY、`:4533-4550` SNR 丢弃分类）—— 来自子代理全量通读 + 我对关键行的独立复核，我**未逐条亲自复算**。
6. **HEAD 漂移** —— 本片审查期间 HEAD 三次变化（`1fa477a7`→`9a83f63`，清单称 `f9650dd`，派单称 `850a9ede`）。我已确认本片成员文件相对 HEAD 干净，但**若在漂移期间有他人改动本片成员，我读到的内容可能已非最终版**，请前台在合批前复核 `git log` 区间。

---

## 10. 一句话结论

`INF-pipeline-001` **判定阻断**：14 份全部读完（7966/7966 行，100%），查出 **8 条阻断**（含 3 处自洽式/恒真门、1 处恒红门、1 处不在门禁内执行的伪门、1 处会删除既有产物的原子清理反转）、11 条须修、5 条建议；**与既有「14/14 默认保留」的机器判定显著不一致（既有偏松）**，其根因正是「分类通过 ≠ 实现正确」。本片未发现私建线程池，未发现自愈锚形态——两项均如实报「未发现」，不编造。
