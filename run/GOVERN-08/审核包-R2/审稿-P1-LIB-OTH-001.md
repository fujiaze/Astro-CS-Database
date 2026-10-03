# 审稿 P1 · LIB-OTH-001 · G08-05 对抗审稿第 1 遍

- 审稿片：`LIB-OTH-001`（层 `lib/(其他)`）
- 基准：`/workspace/Astro CS Database`
- **实际 HEAD = `f4a2cf21092ac89d8efcd92e5bceca444349892b`**（`清理科学正本对已删归档的依赖，登记证据面已失处`）
  ⚠️ 派单书写的是 `850a9ede`。两者不符。本片全部结论以**实读到的 HEAD 内容**为准，行号锚均对当前工作树复核过。
- 成员清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:3495-3565`
- 纪律：零 git 写、零编译、零 ctest/pytest/零二进制、零仓内文件改动。全部取证为 `read` / `grep` / 只读 `bash`。

---

## 1 读完了吗

| 项 | 数 |
|---|---|
| 成员份数（片清单） | 63 |
| 清单存在性核对 | **63/63 全部存在，0 缺失** |
| 成员总行数（片清单声明） | 10648 |
| 成员总行数（本人 `wc -l` 实测） | **10649** |
| 实际读完份数 | **63 / 63** |
| 实际读完行数 | **10649** |
| 覆盖率 | **100.00%（63 份 / 10649 行）** |

**计数口径**：行数 = `sum(1 for _ in open(f, encoding='utf-8', errors='replace'))`，即 `wc -l` 的换行计数，不含无换行末行。与片清单 10648 差 1，属计数约定差（末行是否带换行），非成员差异。

**未读完的：无。** 无跳过、无抽样、无"顺便"。本片 63 个成员我逐个 `read` 到 EOF。

**基线偏差**：本片 4 个子代理另读域外文件（`upm.h/upm.cpp`、`p3_resample.h`、`atomic_publish.h`、`ac_api.cpp`、`module_adapters.cpp`、`ipv_types.h`、`calibrator.cpp`、`cosmetic_corrector.cpp`、`eng/tests/unit/CMakeLists.txt`）。这些**不计入**我的 63 份覆盖率，仅用于核验本片结论。

---

## 2 本片判定

# 阻断

本片同时命中负责人本轮点名的四类：**恒真门（成建制）**、**静默降级（成建制）**、**悬空引用（退役理由整块失效）**、**用同一个定义式既当被检量又当期望量（AIO 对齐"证明"）**。

### 最重 3 条

**【重 1 · 恒真门不是个别缺陷，是验收链条的承重结构塌了，且根因在本片文档里可逐字指出**

`p1_session.cpp` 的顶层 `status` 取值集合是 `{created, failed, partial}`——**`"complete"` 在整个 TU 里只出现在注释中**：

```
$ grep -n 'complete' lib/phase1_session/p1_session.cpp
522:    // P1-001 (attempt 2): complete 门 fail-closed（…）
526:    // 期间 status="partial"（不写 complete）, availability 8 域如实报告。
527:    // 链完整迁移完成后按控制包门禁恢复 complete 语义（不得由本文件单方放宽）。
$ grep -n 'manifest\["status"\]' lib/phase1_session/p1_session.cpp
528:    s->manifest["status"] = "partial";
551:        s->manifest["status"] = "created";
553:        s->manifest["status"] = "failed";
```

而承载验收关键词**「取消/失败不会写完成 manifest」**的三处断言，全部写成 `!= "complete"`：

- `p1sess_tests_units.cpp:343-344`（U3 `u3_not_complete`）
- `p1sess_tests_units.cpp:408-409`（U6 `u6_not_complete`）
- `p1sess_tests_properties.cpp:231-232`（P3 `p3_not_complete`）

⇒ 这三处**恒真**，永不可能转红。负责人本轮说"恒红门把真实缺陷藏在红灯里，比恒真门更隐蔽"——这里是恒真门把**取消态被误报成 `created`** 这个真缺陷藏在绿灯里（见 §5 反例 4）。

**根因在本片文档内，且是自相矛盾的**：`lib/phase1_session/README.md` 里同一件事有两种写法——
- `README.md:117-118`：「成功后 `manifest["frames"]` 与 `manifest["status"]="complete"`（:333-334）」
- `README.md:175`：「manifest 状态机：`created`（:342-343）→ `failed`+`error`（:344-346）→ **`complete`（:334）**」
- 但 `README.md:216-222`（§7）：「run 成功路径 manifest `status="partial"`（不写 complete）」
- 而 `README.md:74` / `:113-114` 仍写「master nullptr 恒等 = DISP-COS-009 语义」「master_dark/master_bias 实参传 nullptr」，被 `README.md:234-240`（§8-3「**已更正**…旧版…均已作废注销，现场锚 :295-296 漂到 :479-484」）直接推翻。

我实读 `p1_session.cpp:479-484`，代码**确实**传了真实 dark/bias 平面，§8-3 是对的；§2/§3/§5 是陈旧的。**同一文件四个小节两两对立**。

**【重 2 · 悬空引用：退役代码块"不能删"的全部理由，已指向不存在的文件】

`p3_export.cpp:1-33` 与 `p3_export.h:1-33`（同一段 RETIRED-CODE-RETAINED 重复两份）的 WHY-KEPT 列出三条"他域锚"，逐条实测：

| 声明 | 实测 | 结论 |
|---|---|---|
| `① eng/ci/checks.json CHK-CONTRACT-TEST 登记 p3_export_positive/negative/oracle` | `ls -d eng/ci` → **不存在** | **悬空** |
| `② eng/ci/spec_named_impls.json SNI-S4-P3X-06/P3X-12`、`eng/ci/ledgers/spec_named_impl_gaps.json` | 同上，目录不存在 | **悬空** |
| `③ docs/engineering/BUILD_GRAPH.md:338` | 该文确为 127 行、`:338` 越界、无 p3_export 行 | **正确（已自纠）** |
| STATUS 证据「`grep -c p3_export CMakeLists.txt = 0`」 | **实测 = 2**（`:1483` 注释、`:1510` add_subdirectory） | **假证据** |
| `:15` 称 add_subdirectory 在 `CMakeLists.txt:1510` | `:1510` 确有 | 正确 |
| `:29` 称同一 add_subdirectory 在 `CMakeLists.txt:980` | `:980` 是注释，**与 :15 自相矛盾** | **错** |
| `atomic_publish.cpp:154-160`、`atomic_publish.h:28-31` | 文件存在于 `lib/infrastructure/aio/product_io/` | 存在（行号未逐一验） |

⇒ 保留该文件的**全部现行依据已随 `eng/ci/` 整目录删除而失效**，而作为"未接入生产"的举证命令本身是错的。这正是负责人警告的「退役对象仍有活调用者 / 悬空引用」复合形态：**不是退役对象有活调用者，而是退役对象用一批已删的注册面给自己作护身符**。

**根因（子代理 #5 定位，我复核）**：`e5f589a6 G08-01 物理删除旧门禁与 CI（344 件 / -136676 行）` 删除了 `eng/ci/**`。**那次治理提交同时废止了保留本文件的理由，却没人回来改注释块**——同一批被删的还有 `eng/tools/quality/check_ctest_registration.py`、`check_p1_symbol_map.py`、`check_prod_reachability.py`、`check_pipeline_trace.py`（后两个仍被 `README.md:206,208-209` 引为验证锚，见 D11 类）。

⇒ **本片的头号治理结论**：G08-01 的删除留下了**成片的、无人认领的悬空理由**（退役块 + README §7 验证锚 + `lib/README.md:8`），它们全部指向同一批已删文件。修本片必须与 G08-01 的删除**同批回填**，否则下一轮审稿还会再报一遍。

**【重 3 · AIO 的"编译期对齐证明"是自洽式断言 —— 本轮最标准的"同一式当被检量又当期望量"】

`lib/include/acsd/io/aio_abi_v1.h:16-18` 声称：
> `0..7` 与 `lib/include/acsd/common_abi_v1.h` acsd_status 共同子域数值一致。
> `0..13` 与 `.../fits_stream_v1.h` acsd_fio_status 全域一致（IO 家族同域; **_Static_assert 编译期对齐证明**）。

但该头 `:27-28` 只 `#include <stddef.h> <stdint.h>` —— **它根本没有 include `common_abi_v1.h`，也不可能引用 `acsd_status`**。它的 `_Static_assert`（`:142-165`）全部是 `AIO_ERR_TRUNCATED == 8`、`AIO_ERR_BAD_HEADER == 9` 这类**拿 AIO 常量比整数字面量**：

```c
static_assert(AIO_OK == 0 && AIO_ERR_PARAM == 1 && … AIO_ERR_TRUNCATED == 8,
              "AIO-001: aio_status 0..8 必须与 acsd_status/acsd_fio_status 数值一致");
```

⇒ 该断言**只能证明 AIO 自身没改**，对它声称守护的 `acsd_status` 漂移**零检出能力**。

**反例（我构造并推翻成功）**：把 `common_abi_v1.h:64` 的 `ACS_ERR_BUDGET = 8` 改成 `80`，`aio_abi_v1.h` 全部 static_assert **依然全绿**，而 AIO 的 `8`/`80` 语义已与 `acsd_status` 错位。这就是"用同一个定义式既当被检量又当期望量"的教科书形态，且它自称"编译期对齐证明"。

---

## 3 逐文件清单（63 份，全读）

判定栏：✗=有问题 ✔=本片内未见问题。"读了什么 / 看到什么"栏只记实际读到的内容。

### 3.1 生产源码 · 会话层（10 份）

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `lib/phase1_session/p1_session.cpp` 580 | 全读 | 异常屏障(253-271)齐备；但 `output_dir` 兜底 `""`(318)、`cosmetic_flag` 合同外类型 `return dflt`(86)、master 读取失败不置 stage fail(333/337/341)、`aio_read_fits` 重读失败不置 stage fail(392)、零帧 run 返回 ACS_OK | ✗ |
| `lib/phase1_session/p1_session.h` 40 | 全读 | `:18` 承诺"拒绝未知键…(无 silent default)" — 实现无未知键检查；`:21` 承诺 async_io_depth≥1 起预读 worker — 该参数只在 `:241` 校验后**再无引用** | ✗ |
| `lib/phase2_session/p2_session.cpp` 318 | 全读 | `p2_upm_info` 失败仍 `stage("upm_build","ok")`(233-235)；`!model` 走 `map_rc(0)`→ACS_OK(221-224+46)；`persist_upm` 缺 path 静默跳过(239)；无 C 边界异常屏障(109 只包 parse)；`p2_coverage_free` 在 143 失败路径泄漏 | ✗ |
| `lib/phase2_session/p2_session.h` 39 | 全读 | `:19` 同样承诺"拒未知键" — 未实现 | ✗ |
| `lib/phase3_session/p3_session.cpp` 441 | 全读 | `parse_request` 只捕 `parse_error`(79)，后续 `get<T>`/`value<T>` 全可穿 extern "C"；`p3_order_select` 返回值丢弃(200)；逐像素 `rst!=OK → continue`(292/312) 与真无覆盖不可分；`cancelled_row==-2` 返回 IO 但不设 last_error(352-356)；`corrupt_at=-2`(270) 被 357 当损坏报 PARAM；inspect 失败 run 返回 `{}`+ACS_OK | ✗ |
| `lib/phase3_session/p3_session.h` 40 | 全读 | `:18-19` 拒清单含 `source.properties` — 实现不查；`:22` "全程经 logger 发 stage 事件" — `log()` 全文 0 调用 | ✗ |
| `lib/phase3_session/p3_export.cpp` 907 | 全读 | **visualization 分支 636-638 丢弃重开验证**（SB/PSF 分支 831-837 正确）；退役块证据失效（见重 2）；`mkdirs` 忽略全部 mkdir 返回值(113-126)；`k_corr.domain.patch_size=8` 硬编码(300) | ✗ |
| `lib/phase3_session/p3_export.h` 256 | 全读 | 退役块与 .cpp 逐字重复（两处同错）；`:239` "任何冻结门命中即返回非 Ok" 被 636-638 违反 | ✗ |
| `lib/phase3_session/README.md` 19 | 全读 | `:4` 称 `hips_properties.h`（本目录）— **不存在**；`:5` 5 个测试文件 — 全部存在 ✔ | ✗ |
| `lib/phase2_session/README.md` 115 | 全读 | 逐条登记 DISP-P2SES-001..008；`:11` 称 `runtime_client.cpp:28 passthrough 不自动补 output_dir` — 与我实测 p2_session.cpp run 不消费 output_dir 一致。**本片文档诚实度最高的一份** | ✔ |

### 3.2 测试（11 份）

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `p1sess_tests_units.cpp` 463 | 全读 | `!= "complete"` 恒真门(343/408)；`if(px.size()==64)` 筛掉真信号(293/321/430)；`u1_manifest_kind` 常量回声(212 vs p1_session.cpp:167)；U6 只断言弱否定而 U3 断言 `=="failed"`；`artifacts[0].get<>()` 非 const json 无守卫(244) | ✗ |
| `p1sess_tests_properties.cpp` 317 | 全读 | P2 `ref_px` = SUT 自身输出(186)；cancel 只断言 `!=complete`(231)；cancel flag 预置位致 3 个取消点只 1 个可达(205)；P5 四次 run 后从不 inspect manifest(276-280)；`LogSink::stage_events` 只写不读(72/77) | ✗ |
| `p1sess_tests_negative.cpp` 165 | 全读 | 17 case 逐键拒收矩阵 + N2/N2b/N3 正对照 — **质量好，应保留**。仅：17 case 共用单一 fault name | ✔ |
| `p1sess_tests_selfcheck.cpp` 199 | 全读 | **判据 `child_rc != 0`**(154/168)，127/-1/2 全算"必败验证通过"；`child_env[]` 整表替换丢 TMPDIR(58)；`check_guard_path` 五处 `1==1`(89/97/108/117/126)，宏的真失败分支 `else if(!(cond))` 全套件从未执行 | ✗ |
| `p1sess_tests_perf.cpp` 115 | 全读 | 头(8)称"两次 run 均 complete"vs 体(100)断言 `partial`；`seconds_1w` 算后 `(void)` 丢弃(104/108)；`sec<20.0` 对基线 <1s = 20× 宽松 | ✗ |
| `p1sess_oracle.hpp` 130 | 全读 | `oracle_calibrate_const`(93-101) 是**真独立 oracle**（标量 double 复式 vs 实现逐像素 float）— 应保留；头注释(12-15)却仍写**已废止**的 `(light-dark)/flat` | ✗（仅注释） |
| `p1sess_fixtures.hpp` 193 | 全读 | splitmix64 + 手写 FITS，不调生产符号 — ✔；`frame_const_pixel`(164) 与 `exptime`/EXPTIME 分支(122-129) 为死代码 | ✔ |
| `p1sess_test_main.hpp` 192 | 全读 | `:107-109` 注入后**其后全部具名 CHECK 无条件失败**（`cond` 根本不求值）→"注入必败"与被检条件无关；`:169-181` 未知组名 `continue` 全部 → `total_fail==0` → **打印 PASS 退出 0**；`P1SESS_CHECK_EQ`(140-151) 无注入钩子 | ✗ |
| `p1sess_tests_main.cpp` 23 | 全读 | 4 组注册（units/properties/negative/performance），无 oracle 组 — 但 `test_main.hpp:11` 用法行写了 `oracle` | ✗ |
| `p1sess/CMakeLists.txt` 86 | 全读 | `:30` 称 negative "13 case" — 实为 **17**；`:41` 称 "worker 完成性" — perf 组无该断言；`:68/:86` 两个 ctest 条目**无 RUN_SERIAL** 且共用 fixture 根 | ✗ |
| `test_p1_session_manifest.cpp` 305 | 全读 | T8b `hot_fixed >= 1` 而真值恰为 1(293) → 过度修正到 64 像素也绿；`:184-189` 注释称"fixture 可被 AIO 读回"而断言是 `fopen!=nullptr`；`:139-143` `drive()` 泄漏 inspect 缓冲 | ✗ |

### 3.3 ABI / 合同头（8 份）

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `include/acsd/common_abi_v1.h` 181 | 全读 | legacy `acsd_status`(55-67) **无** `ACS_ERR_EXCEPTION`；`common_abi_v1.h:64 ACS_ERR_BUDGET=8` 即重 3 反例的改动点 | ✔（被新版取代的 legacy） |
| `include/acsd/abi/status_codes.h` 217 | 全读 | 新版 `acsd_status`(153-166) 比 legacy 多 `ACS_ERR_EXCEPTION=10`；`:22` 强制"任何 DLL 导出 C 函数必须 try/catch 全包裹转 ACS_ERR_EXCEPTION"；`:210-211` 声明 `acsd_status_name_v1`/`acsd_status_domain_name_v1` — **全仓无定义、无调用者** | ✗ |
| `include/acsd/abi/module_api_v1.h` 151 | 全读 | vtable 形状与生命周期注释自洽；`:24` 同样要求异常转 ACS_ERR_EXCEPTION | ✔ |
| `include/acsd/abi/host_api_v1.h` 101 | 全读 | `:14` "module 不得私建无界线程池"；`:59-62` "重计算只使用 host 授予的租借（FORBID-003）" | ✔ |
| `include/acsd/abi/artifact_api_v1.h` 106 | 全读 | 服务表契约完整 | ✔ |
| `include/acsd/abi/lifecycle_v1.h` 299 | 全读 | **8 个函数全部只在 `eng/tests/abi/abi002_lifecycle_probe.c`（测试文件）有定义**，生产无实现；`:50-51` 明文"module 不得私建线程池" | ✗ |
| `include/acsd/io/aio_abi_v1.h` 167 | 全读 | **重 3 的自洽式"对齐证明"**；`:16-19` 三条对齐声明只有第 3 条（14/15 扩展）真实 | ✗ |
| `include/acsd/contracts/artifact_abi_v1.h` 91 | 全读 | `:48` 三个路径引用**逐条实测全部存在** ✔ | ✔ |

### 3.4 core 合同头（26 份）

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `core/context.h` 266 | 全读 | `ThreadLease` RAII 归还、`ThreadBudget` 原子预留完整；`:98` 哨兵纪律"0=未观测，不得以配置值回填" — 少见的高质量自纠 | ✔ |
| `core/contracts.h` 231 | 全读 | `Result<T>`/`Error`/`CancellationToken`/`TraceEvent` 自洽 | ✔ |
| `core/scheduler.h` 123 | 全读 | `:31` "模块只投递 work, 不建私有 pool" | ✔ |
| `core/executor.h` 160 | 全读 | `:6` "std::thread 私建池 → **静态扫描拒绝**" | ✔ |
| `core/module.h` 120 | 全读 | `IModule`/`ModulePlan` 契约完整 | ✔ |
| `core/module_adapters.h` 39 | 全读 | **`:31` 的「当前 `DATA-P1-PSF.psf_params` **零消费者**、psf 端口为死边」是假声明** — `module_ports.registry.json:325` 明写「下游消费者：photometry（p1_sources）、noise-snr（p1_sources）、drizzle（逐星掩膜）」；同源假声明在 `module_adapters.cpp:3325` 复述 | ✗ **阻断** |
| `core/build_stamp.h` 36 | 全读 | `:12-15` 明确"source_digest 才是唯一指纹，head_sha 不得单独当指纹" | ✔ |
| `core/canonical_hash.h` 43 | 全读 | 排除清单显式列出，且显式声明"明确**不排除** DATE-OBS/BUNIT/…" | ✔ |
| `core/runtime.h` 134 | 全读 | `:35` "禁止第二套调度器；模块不建私有 pool" | ✔ |
| `core/variance_floor.h` 162 | 全读 | `:13-19` 论证"绝对常数为何不可用"（α² 跨 1e-17），`:124` 不可用方差**原样透传不伪装** — 数值纪律优秀 | ✔ |
| `core/plan_estimator.h` 146 | 全读 | **`:19` 引用 `lib/phase3_session/hips_properties.h kHipsTileWidth=512` — 该文件不存在，`kHipsTileWidth`/`kMaxOrder` 全仓无定义** | ✗ |
| `core/wcs_degrade_policy.h` 183 | 全读 | `:16`/`:87` 锚 `module_adapters.cpp:5380-5388`（实为 5541）；`:47` 锚 `4896`（实为 4921）；`:48` 锚 `ipv_types.h:251,277`（实为 **250,276** — 我首读判"正确"是**错的**，见 §7）；`:22` 自称"不含任何容差/默认星数"却含 `:52/:53` 默认星数与 `:119/:120` 两个容差 | ✗ |
| `core/memory_pressure.h` 259 | 全读 | 滞回/丢弃语义完整，`:58-59` 显式"不可判定⇒不启用并显式落盘，不冒充" | ✔ |
| `core/memory_budget.h` 73 | 全读 | `:63-67` 三种边界（percent=0 / 越界 / available=0）语义明确，**显式拒绝静默 clamp** | ✔ |
| `core/mosaic_window.h` 151 | 全读 | `:14-17` 与 `:76-85` **显式记录"恒真门"缺陷史并给出反恒真四判据**（含"严格"而非"不减"）；`:137-139` 显式声明 run 作用域有界池合规 | ✔ |
| `core/export_stream.h` 183 | 全读 | `:177-180` 同上线程池合规声明；`:123-128` `WorkerEntryGuard` 说明异常不得逃出线程入口 | ✔ |
| `core/normalize_workflow.h` 220 | 全读 | `:199-202` 同上线程池声明 | ✔ |
| `core/block_frame.h` 157 | 全读 | 四条非法图判据 + 生命周期状态机完整 | ✔ |
| `core/block_flow.h` 114 | 全读 | `:10` "未声明读/写 ⇒ fail-closed，不静默" | ✔ |
| `core/pipeline.h` 74 | 全读 | `IrError` 12 值数值冻结，含 `UNCONSUMED`/`SERIAL_HEAVY` | ✔ |
| `core/artifact.h` 117 / `core/artifact_store.h` 108 | 全读 | 唯一 producer、role 绑定、篡改硬失败 | ✔ |
| `core/checkpoint.h` 80 | 全读 | scope 隔离 + resume 前逐项校验 hash/schema | ✔ |
| `core/logging.h` 103 / `core/trace.h` 95 | 全读 | `:10-11` "禁止 config 值冒充观测" | ✔ |
| `io/io_adapter.h` 74 | 全读 | **`:45` FNV-1a 种子 `1469598103934665603` = 标准 offset basis `14695981039346656037` 的 1/10（少一位数字）**，注释与 `:33` 均自称 FNV-1a | ✗ |

### 3.5 模块文档（8 份）

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `phase1_session/README.md` 268 | 全读 | §2/§3/§5 与 §7/§8 三方矛盾（`complete` vs `partial`；`nullptr` vs 已修）——见重 1；`:98/:152/:225/:262` 等行号锚全部指向 ~372 行前身 | ✗ |
| `phase1_session/module.yaml` 137 | 全读 | **5 处源码行号锚全部失效**（`:152`→实际 241；`:225`→实际 354；`:262`→实际 397/491；`:295-296`→实际 479-484；`:71-74`→实际 141-148）；`:131` 仍把 DISP-COS-009 登记为未修，与 `memory.md:20-21` 直接冲突 | ✗ |
| `phase1_session/memory.md` 124 | 全读 | `:19-21` 锚 378/481/479-480 **实测正确**，且明确"旧记 nullptr 已作废注销"；但 `:26-30` 又用 `:152/:177-181/:289/:349-357` 等失效锚 — **文件内部新旧混杂** | ✗ |
| `phase2_session/module.yaml` 109 | 全读 | `:31-37` 登记 8 条 DISP，与我独立复算**逐条吻合** | ✔ |
| `phase2_session/memory.md` 78 | 全读 | `:53-68` 逐条展开 8 条 DISP 及其行号 — 本片最诚实、最可复核的登记面 | ✔ |
| `lib/README.md` 26 | 全读 | `:15` 把 `pipeline/scheduler/aio/benchmark/observability/gaia_xpsd_client/acr/hips_browser` 与 `algorithms/`、`infrastructure/` **平级列出** — 实测 9 个全在 `lib/infrastructure/` 下；`:8` 引用不存在的 `eng/ci/` | ✗ |
| `lib/include/acsd/README.md` 21 | 全读 | `:15` 称 core 合同头 "**24 个**" — 实测 `ls lib/include/acsd/core/*.h \| wc -l` = **26**；`:20` 引用不存在的 `eng/ci/checks.json` | ✗ |

---

## 4 发现清单

### 4.1 阻断（12 条）

| # | `文件:行` | 问题 | 类别 |
|---|---|---|---|
| B-01 | `p1sess_tests_units.cpp:343`、`:408`；`p1sess_tests_properties.cpp:231` | `status != "complete"` 三处恒真门，生产无任何路径可写 `"complete"`。承载验收关键词 | **恒真门** |
| B-02 | `p1_session/README.md:117-118,175` vs `:216-222`；`:74,113-114` vs `:234-240` | 同一文件两两对立，是 B-01 的根因 | **文档自相矛盾** |
| B-03 | `io/aio_abi_v1.h:142-165`（对 `:16-18` 的"编译期对齐证明"） | 拿 AIO 常量比整数字面量，对 `acsd_status` 漂移零检出能力 | **自洽式断言** |
| B-04 | `p3_export.cpp:6-16`、`p3_export.h:6-16`；`lib/README.md:8`；`include/acsd/README.md:20` | 4 处引用 `eng/ci/**`，该目录**整目录已删** | **悬空引用** |
| B-05 | `p3_export.cpp:17`、`.h:17` | 「`grep -c p3_export CMakeLists.txt = 0`」**实测 = 2**，举证命令自身是假证据 | **假证据** |
| B-06 | `p3_export.cpp:15` vs `:29` | 同一 add_subdirectory 同时被指为 `CMakeLists.txt:1510` 与 `:980`，`:980` 是注释 | **块内自相矛盾** |
| B-07 | `p1sess_test_main.hpp:107-109` | 注入后其��全部具名 CHECK **无条件 `++failures`**（`cond` 不求值）⇒ "注入必败"与被检条件无关 | **恒真门（级联）** |
| B-08 | `p1sess_tests_selfcheck.cpp:154,168`（配合 `:44,69,74,78`） | 判据 `child_rc != 0`；127(fork/exec 失败)、-1(崩溃)、2(fixture 失败) 全算"必败验证通过" | **恒真门** |
| B-09 | `p1sess_test_main.hpp:169-181` | 未知组名 → 全部 `continue` → `total_fail==0` → **打印 PASS 退出 0**；`:11` 用法行还把未注册的 `oracle` 列为合法组 | **恒真门** |
| B-10 | `p3_export.cpp:636-638`（对照正确的 `:831-837`） | visualization 分支重开验证结果**存了不看**，`Status::Ok` 直返 | **静默降级** |
| B-11 | `p2_session.cpp:226-235` | `p2_upm_info` 失败 → `stage("upm_build","ok")`；`control_count/model_hash` 等唯一审计信息静默丢弃 | **静默降级** |
| B-12 | `abi/status_codes.h:210-211`；`abi/lifecycle_v1.h:158,163,169,177,194,234,237,238` | 2 个函数声明无定义、8 个函数**只在测试文件** `eng/tests/abi/abi002_lifecycle_probe.c` 有定义 — 冻结 ABI 合同层无生产实现 | **悬空声明** |
| B-13 | `core/module_adapters.h:31`（+ `module_adapters.cpp:3325`） | 「`psf_params` **零消费者**、psf 端口为死边」是假声明；`module_ports.registry.json:325` 登记了 3 个下游消费者。**这是负责人本轮点名「已实测存在一例」的同一缺陷再次发生** | **假零消费者** |
| B-14 | `p3_export.h:176-181` → `p3_export.cpp:297,303-305,316` | provenance 写入**从未运行过的标定**：`k_corr_calibration_script="run/quality/phase2/upm/kcorr_calib.py"`（**该文件不存在**）、`k_corr_value=1.4`（`DATA_SEMANTICS.md:1748` 明说这是"实现记录不是普适常数"且需逐帧查表+适用域，代码无查表无查域）、`generated_utc="2026-09-16T00:00:00Z"`（**冻结假时间戳**，且被 `canonical_hash.h:12-14` 排除在规范哈希外 ⇒ 复现性检查永远抓不到） | **伪造 provenance** |
| B-15 | `core/variance_floor.h:111` + `:132-142` | dtype 地板取 `float denorm_min=1.4013e-45`，`1/floor=7.136e44 > FLT_MAX` ⇒ **`ivar` 溢出为 `+inf`**，正是该头 `:8-9` 自称要防的、也是 `NOISE_MODEL.md:292` 禁止的 `(0,+inf)` 对；`to_product_dtype_keeping_availability:139` 还主动"救援"下溢、**制造**该情形 | **数值稳定性** |

### 4.2 须修（23 条，摘主要）

| # | `文件:行` | 问题 |
|---|---|---|
| M-01 | `p1_session.cpp:318` + `:396` | `output_dir` 缺失 → 兜底 `""` → 产物写 `/calibrated_<x>.fits`（文件系统根），仍报 `ok` |
| M-02 | `p1_session.cpp:86`（对照 `:450-456` 已 fail-closed 的 `method`） | `cosmetic_flag/float/int` 合同外类型 `return dflt` — 同一函数内 `method` 严、`enabled/hot_sigma/max_structure_size` 松 |
| M-03 | `p1_session.cpp:333,337,341,392` | 4 处返回 `ACS_ERR_IO` 但不置 `stages.back()["status"]="fail"` ⇒ 失败 run 的 manifest 停在 `"running"`（`fail_trailing_running_stage` 只在异常路径生效） |
| M-04 | `p1_session.cpp:296-315,509-519,521-540` | `input_lights: []`（run 不校验）⇒ 四段全 `ok`、`frames:0`、返回 `ACS_OK` |
| M-05 | `p1_session.h:18`、`p2_session.h:19` | 两头都承诺"拒绝未知键(无 silent default)"，实现均无未知键检查；`p1_session.h:21` 的 async 预读 worker 不存在 |
| M-06 | `p3_session.cpp:73-132`、`p2_session.cpp:102-112` | 无 p1 那样的 C 边界异常屏障；`json::type_error` 可穿 `extern "C"`（p1 `:253-271` 已修，P2/P3 未修） |
| M-07 | `p3_session.cpp:200` | `p3_order_select` 返回值丢弃 ⇒ `order_sel=-1` 写进 FITS provenance 与结果 JSON，仍 `exit_code:0` |
| M-08 | `p3_session.cpp:292,312` | 逐像素 `rst!=OK → continue` ⇒ I/O 失败与真无覆盖**逐位相同**（`NaN`/`cov=0`），无计数无日志 |
| M-09 | `p3_session.cpp:352-356` | 返回 `ACS_ERR_IO` 但 `last_error` 从不赋值 ⇒ 诊断面空白 |
| M-10 | `p3_session.cpp:270` vs `:357-362` | worker open 的 IO 失败被报成 `ACS_ERR_PARAM` + "uncertainty product corrupt (negative/inf variance pixel)" — 状态码与诊断双错 |
| M-11 | `p2_session.cpp:239` | `persist_upm:true` 缺 `upm_save_path` ⇒ 整段静默跳过，无 stage/无 artifact/无错误，仍 `ACS_OK` |
| M-12 | `p2_session.cpp:221-224` + `:46` | `!model` 走 `map_rc(0)` → **返回 ACS_OK** 并继续以 `model==nullptr` 跑到 `ran=true` |
| M-13 | `p2_session.cpp:142-146` | 第二次 `p2_coverage_build` 失败直接 return，`p2_coverage_free` **未调**（guard 在 :147 才建立） |
| M-14 | `p1sess_tests_units.cpp:293,321,430` | `if (px.size()==64)` — 尺寸来自 SUT 自己写的 NAXIS ⇒ 写错几何时 64 条像素断言**零执行且绿** |
| M-15 | `p1sess_tests_properties.cpp:186` | P2 的 `ref_px` 就是 SUT 自己的 workers=1 输出；`oracle_calibrate_const` 在本组从未被调用 |
| M-16 | `p1sess_tests_properties.cpp:205` vs `p1_session.cpp:300/360/462` | cancel flag 预置位 ⇒ 3 个取消点只有 io_read 那个可达，帧粒度两点结构性不可达 |
| M-17 | `p1sess_tests_properties.cpp:276-280` | 同 handle 连续 4 次 run 后**从不 inspect**，manifest 的 stages/artifacts 累积状态机零断言 |
| M-18 | `test_p1_session_manifest.cpp:293` | `hot_fixed >= 1` 而真值恰为 1 ⇒ 过度修正到 64 像素（摧毁整帧）仍绿 |
| M-19 | `io/io_adapter.h:45` | FNV-1a 种子少一位数字（`1469598103934665603` vs 标准 `14695981039346656037`） |
| M-20 | `core/plan_estimator.h:19`；`phase3_session/README.md:4` | 两处引用不存在的 `lib/phase3_session/hips_properties.h` 及其 `kHipsTileWidth`/`kMaxOrder` |
| M-21 | `phase1_session/module.yaml:131-135` | 5 处源码行号锚全部失效；`:131` 把 memory.md 已注销的 DISP-COS-009 仍登记为未修 |
| M-22 | `core/wcs_degrade_policy.h:47` | 锚 `module_adapters.cpp:4896` — 实测 `:4921`（该仓 `:3318` 记录过同类旧锚误标） |
| M-23 | `lib/README.md:15`；`include/acsd/README.md:15` | 9 个 infra 子目录被写成 `lib/` 平级（实为 `lib/infrastructure/`）；core 头计数"24 个"实为 26 |
| M-24 | `common_abi_v1.h:105-106` vs p1/p2/p3 三会话 | `acquire`/`release` 定义了原子租借，**全 `lib/` 只有 2 个调用点**（`host_services.cpp:117-118` 接线 + `executor.cpp:96`）——**三个会话无一处租借**，都把 `max_workers` 当上限直接用 ⇒ P1/P3 并发时各自取满、全局超卖 |
| M-25 | `core/memory_pressure.h:4-15` | 头级权威锚 `:609-615` 实测是**实验单元目录/实验报告**内容；五条编排策略实际在 `docs/ACSD_DESIGN.md:478-479`；`:680` 声称的"内存闸门 × Runtime lease"**全文不存在** |
| M-26 | `p3_session.cpp:227-235` | `wpx/hpx ≤ 20000` 是唯一守卫 ⇒ 单次 export 最高 **6.4 GB** 四连分配，且仓库有 `memory_budget.h`/`plan_estimator.h`/`budget` 字段**均未被咨询**；`bad_alloc` 亦穿 `extern "C"` |
| M-27 | `p3_session.cpp:189-194` | `max_tiles` 超守卫时 `return ACS_ERR_BUDGET`，但 `:171` 打开的 `samp` **未 close** — 全函数唯一泄漏点（其余 6 处错误路径都 close） |
| M-28 | `p3_session.cpp:321-328` | `u_out=+Inf` 时 `isnan` 假、`>0` 真 ⇒ `ivar=0` 而 `var=+Inf`；`:307-308` 注释明写"Inf 产品损坏 → 显式拒绝（禁 clamp/补 0）"，**检查放错了层** |
| M-29 | `p3_session.cpp:112` vs `phase3_session/README.md:18` | README 称"最大尺寸来自配置合同，不硬编码 20000"，代码硬编码两次且从不引用 `ACSD_P3_MAX_SIDE`（该宏在 `p3_wcs.cpp`/`p3_proj.cpp` 被消费） |
| M-30 | `p3_export.cpp:213` vs `p3_session.cpp:391-397` | `make_layer` 对**所有** HDU 硬编码 `bitpix=-64`，而 session 接受 `{-32,-64}` ⇒ 请求 `-32` 时 FLUX HDU 仍为 `-64`，无错误无记录 |
| M-31 | `p3_export.cpp:294-295` | `flux_conservation_factor=1.0` 恒定并恒声明存在；`p3_session.cpp:294` 明明逐像素算了真实 coverage ⇒ 覆盖率 40% 的产品仍背书"100% 通量守恒" |
| M-32 | `p3_session.cpp:179` 注释 vs `:185-186` 代码 | 注释写 `ceil(W·H/W²)+16`，代码是 `ceil(W·H/512²)+16`；且字面量 `8` 在 `:180/:187` 重复，而 `plan_estimator.h:45` 已有 `kPeakTilesCacheDefault=8` |

### 4.3 建议（9 条）

1. `p1sess_tests_units.cpp:296-299` 注释里的 `42.1519`/`39.816` 是装饰性魔数，测试从不计算，且与本 fixture 的实值（≈45.5/≈43.0）不符 — 易被后续审稿误当已断言值。
2. `p1sess_test_main.hpp:140-151` `P1SESS_CHECK_EQ` 无注入钩子 — 82/129 断言点（约 64%）在证据网之外，包括全部结构计数断言。
3. `p1sess_tests_units.cpp:184-189` 注释称"fixture 可被 AIO 读回"而断言是 `fopen!=nullptr` — 未兑现的承诺。
4. `test_p1_session_manifest.cpp:139-143` `drive()` 泄漏 inspect 缓冲，与 `units.cpp:91,454-455` 主张的所有权合同相悖。
5. `p1sess_fixtures.hpp:164-166`（`frame_const_pixel`）、`:122-129`（EXPTIME 分支）为死代码，AGENTS §6 要求退役即删或标注。
6. `p1sess/CMakeLists.txt:30` 称 negative "13 case"（实 17）；`:41` 称 "worker 完成性"（perf 组无此断言）。
7. `p1sess_oracle.hpp:12-15` 头注释仍写**已废止**的 `(light-dark)/flat`，与 `:93-101` 代码矛盾 — 后人可能据此"修复"回坏公式。
8. `p1sess_tests_properties.cpp:262` `PARAM || INTERNAL` 析取放宽了门；实跑路径恒为 PARAM。
9. `p3_session.cpp:427-439` inspect 在失败 run 后返回 `{}`+ACS_OK（`s->ran=true` 于 :147 提前置位），p1/p2 有 `!ran` 分支而 p3 没有。

---

## 5 我主动构造的反例

口径：每个反例都写明「构造什么 / 期望推翻什么 / 是否推翻」。推翻=成功找到缺陷。

**反例 1 · AIO 对齐证明（成功推翻）**
构造：把 `common_abi_v1.h:64` 的 `ACS_ERR_BUDGET = 8` 改为 `80`，其余不动。
期望推翻：`aio_abi_v1.h:142-165` 的"编译期对齐证明"会变红。
结果：**推翻成功 —— 断言全绿**。`aio_abi_v1.h` 只 include `<stddef.h>/<stdint.h>`，从结构上不可能引用 `acsd_status`。该头自称的证明对其声称守护的漂移**零检出能力**。（未落盘任何改动，仅逻辑推演 + include 清单实测。）

**反例 2 · selfcheck 在无 /proc 环境下误判为"注入必败验证通过"（成功推翻）**
构造：推理 `run_injected_child` 在 `/proc/self/exe` 不存在时（macOS、chroot、部分容器）走 `execve` 失败 → `_exit(127)`；父进程 `WEXITSTATUS==127`；`p1sess_tests_selfcheck.cpp:154` 只判 `child_rc == 0`。
期望推翻：阶段 2/3 会报红。
结果：**推翻成功 —— 报绿**，并打印 `SELFCHECK phase2: fault-inject 'u1_manifest_kind' → child rc=127 (必败验证通过)`。**注入从未发生**，一行测试都未执行。

**反例 3 · selfcheck 剥掉环境后 fixture 落到不可写 CWD（成功推翻）**
构造：`child_env[] = {fbuf.data(), nullptr}`（`:58`）整表替换 ⇒ 子进程无 `TMPDIR/TEMP/TMP` ⇒ `fixture_base()`（`units.cpp:136-144`）退化到 `"."`。若 CWD 只读，`setup_fixtures` 失败 → `run_units` 返回 2（`units.cpp:191-194`）。
期望推翻：阶段 2 报红。
结果：**推翻成功 —— `child_rc==2` 仍判绿**。叠加 `CMakeLists.txt:68,86` 无 `RUN_SERIAL` 且 `p1sess_units` 与注入子进程在 `TMPDIR` 未设时**共用同一 fixture 根**，两者都先 `remove_all` ⇒ `ctest -j` 下互相删除，可在**完全正确的实现**上制造"注入已验证"的假绿。

**反例 4 · 取消态被误报为 `created` 而验收门仍绿（成功推翻，且缺陷当前就存在）**
构造：跟踪 `p1_session.cpp:300-303` 的取消路径 —— 置 `stages.back()["status"]="cancelled"` 后 `return ACS_ERR_CANCELLED`，**既不设 `last_error` 也不置 `ran`**。`inspect`（`:550-555`）判 `!ran && last_error.empty()` → 写 `status="created"`。
期望推翻：`p3_not_complete`（`properties.cpp:231`）会因状态可疑而红。
结果：**推翻成功 —— 绿**。`"created" != "complete"` 恒真。更进一步：manifest **根本没有 `"cancelled"` 这个顶层状态**，所以"取消后 manifest 顶层状态歧义"这一缺陷在 p1 侧**至今未修且未被任何测试看见**（p2 侧已登记为 DISP-P2SES-003，p1 侧未登记）。

**反例 5 · `output_dir` 缺失 ⇒ 写文件系统根（成功推翻）**
构造：`p1_session_run` 不强制 validate（`:449` 自陈"run 可被直接调用而无 validate 前置"），传入不含 `output_dir` 的 config。`:318` `doc.value("output_dir", std::string())` 兜底为 `""`；`:396` 拼成 `"/calibrated_<x>.fts"`。
期望推翻：`io_write`（`:511-517`）应因目录不存在而红，或阶段应 fail。
结果：**推翻成功 —— 若进程对 `/` 可写则 `aio_write_fits` 成功**，`io_write` 查存在性通过，manifest 记 `/calibrated_x.fits` 并报 `ok`，返回 `ACS_OK`。这是必填键在 run 路径上变成"写根目录"。

**反例 6 · visual 导出丢弃重开验证（成功推翻）**
构造：让 `visualization` 模式产物的 `provenance.json` 触发 `validate_provenance_json` 失败（如 `omit_provenance_key` 命中一个必需键且 validator 未捕获该键）。
期望推翻：`:636` 之后应走 `:832-837` 那样的 Reject。
结果：**推翻成功 —— `:636-638` 直接 `out.status = Ok`**。对照 SB/PSF 分支 `:831-837` 有 `if (!ok || !reopen.ok)` 判红。两条分支对同一门处置相反。

**反例 7 · `P1SESS_CHECK_EQ` 覆盖不到的断言（成功推翻）**
构造：只把 `units.cpp:219` 的 `stages.size()==3` 改成错误值。
期望推翻：注入 `u1_manifest_kind` 能让 units 组转红。
结果：**推翻成功 —— 能**，但**与被改断言无关**：`test_main.hpp:107-109` 的级联让首个注入之后的每个具名 CHECK 无条件失败；且 `CHECK_EQ` 本身无注入钩子。⇒ "注入能让测试失败"这件事成立，但它对"这条断言是否有鉴别力"零信息。

**反例 8 · `hot_fixed >= 1` 放过灾难性过度修正（成功推翻）**
构造：让 `detect_hot_pixels` 把 64 个像素全判为 hot（T8b fixture 中 63 个 100.0 + 1 个 9000.0，MAD=0 ⇒ sigma=0 ⇒ 判据退化为 `> median`，极易全判）。
期望推翻：`test_p1_session_manifest.cpp:293` 的 `>= 1` 会红。
结果：**推翻成功 —— 仍绿**。真值恰为 1，断言只有下界；而全判 64 会让每张科学帧被 5×5 中值整体替换，是该路径上最具破坏性的现实缺陷。

**反例 9 · p3 `output_dir` 静默改写到输入树之外（成功推翻）**
构造：`{"output_dir": 42}` 或键名拼错。`parse_request`（`:84-129`）根本不查 `output_dir`。
期望推翻：应报 PARAM。
结果：**推翻成功 ——** 回退到 `:384` 的 `hips_dir + "/../output_phase3.fits"`，产物写进**输入产品树的父目录**，而结果 JSON `:411` 忠实报告这个 `..` 路径为 `output_fits_path`。

**反例 10 · 退役块护身符失效（成功推翻）**
构造：按 `p3_export.cpp:8-9` 的说法删除 `p3_export_*` 三个 ctest 目标。
期望推翻：`check_ctest_registration.py` C4 fail-closed 会判红，从而"本轮不能删"成立。
结果：**推翻成功 —— 判红不可能发生**，因为它守的 `eng/ci/checks.json` 根本不存在。保留理由①②双双落空。

**未推翻（诚实记录）**：我尝试用"改动 `common_abi_v1.h` 使 legacy 与新 `status_codes.h` 枚举冲突"来证伪"两套 `acsd_status` 共存是安全的"，**未推翻** —— 因为 `status_codes.h:25-29` 已明文禁止同 TU 混用，且我实测**无任何 TU 同时 include 两者**（`fits_stream_v1.h` 的 grep 命中是注释不是 include，我最初误判、复核后撤回）。故该项降级为"文档已如实登记，不列缺陷"。

---

## 6 盲复算

口径：先不读 `片清单-权威版.yaml` 里的既有判定，独立取证后比对。计数口径 = 独立复算命中的条目数。

**复算前屏蔽**：既有判定 CSV、`审稿-RR*.md`/`审稿-R2-*.md`/`审稿-R3-*.md`/`审稿-P1-*.md` 中涉及本片的结论、`UNRESOLVED.md`。
**复算后重新打开**：仅用于比对，不用于生成结论。

| 我独立取证的范围 | 独立命中 | 与仓内既有判定一致 | 判定 |
|---|---|---|---|
| 恒真门（`"complete"` 不可达） | 3 处断言 + 1 处根因 | **一致** | **偏松**（既有登记未把 §2/§3/§5 与 §7/§8 的自相矛盾点名为根因） |
| `eng/ci/**` 悬空 | 4 处引用 + 1 处假 grep 证据 | **一致** | 一致 |
| AIO 自洽式对齐证明 | 1 | **新增**（既有登记未覆盖） | **偏松** |
| p2 静默降级（upm_info/persist/!model/收敛） | 4 | **一致**（memory.md:53-68 已逐条登记） | 一致 |
| p1 静默降级（output_dir/cosmetic_flag/stage-running/零帧） | 4 | **不一致** — p1 侧**无对应登记**（`module.yaml:131-135` 只登记 dark_scale_factor 与 async_io_depth） | **既有判定偏松** |
| p3 静默降级（visual 丢弃重开验证/丢弃 rc/逐像素 continue/last_error 空/错误码误报） | 5 | **不一致** — **p3_session 侧无任何 known_defects 登记面**（`p3_session/` 下无 module.yaml/memory.md） | **既有判定偏松** |
| selfcheck 判据 `child_rc != 0` | 1（+2 衍生） | **一致**（`自证式判据登记.md` 域内已记 TAUT-NULL-GUARD，但未记本条） | 偏松 |
| 线程池违规 | 1（p3 std::thread） | **不一致 — 我自己否决了这条** | 见下 |
| FNV 种子少一位 | 1 | **新增** | 偏松 |

**我自己否决的一条（红队对自己的反证）—— 后经子代理交叉证据**：
我最初把 `p3_session.cpp:334-341` 的 `std::vector<std::thread> pool` 判为"私建线程池违规"。读到 `mosaic_window.h:137-139`、`export_stream.h:177-180`、`normalize_workflow.h:199-202` 后一度**撤回**：三处均显式声明"run 作用域有界池 + 返回前 join + 无 detach + 头文件不得出现线程容器成员"是 RT-004 认可的形态，p3 与之同构。

**子代理 #5 补上决定性反证后，我恢复该项并改写定性**：`lib/infrastructure/scheduler/src/executor.cpp:20` 明写 **"scheduler 不再自建 std::thread 池（RT-004 消灭 per-run 池）"** —— 这与 `mosaic_window.h:137-139` 的"run 作用域有界池"**直接对立**。

⇒ **本片自身携带两套互斥的线程规则**：
- 认可派：`mosaic_window.h:137-139`、`export_stream.h:177-180`、`normalize_workflow.h:199-202`（三处一致）
- 禁止派：`executor.h:6`（"静态扫描拒绝"）、`common_abi_v1.h:99`（"backend 禁自建线程池"）、`scheduler.h:31`（"不建私有 pool"）、`host_api_v1.h:14`

`p3_session.cpp:334-341` 站在认可派。**因此这不是实现违规，是合同自相矛盾导致的不可判定**——但它使 `executor.h:6` 声称的"静态扫描拒绝"成为空文（仓内 7 处 `std::vector<std::thread> pool` 全在 `.cpp` 函数体内，逃过头文件扫描）。定性为**须修（合同对齐）**，不定性为实现违规。同时补记 M-24：三个会话**从不调用 `budget.acquire/release`**，都把上限当租约用，全局可超卖。

**总体判定：既有判定对本片偏松。** 主要漏项集中在 p3_session（**整个目录无 known_defects 登记面**，其 5 条静默降级无人认领）、p1_session 的 4 条静默降级、`module_adapters.h` 的假零消费者、以及 AIO 的自洽式对齐证明。

---

## 7 子代理派发记录

派发 **5 个**子代理（按关注面分片，互斥）。纪律要求 3-5 个。

| # | job id | 关注面 | 状态 | 主要产出 |
|---|---|---|---|---|
| 1 | `845874d5-8108-4caa-86e8-3ca2a0076c4b` | 生产源码静默降级（p1/p2/p3 + p3_export） | **已回** | 7 阻断 + 11 须修 + (a)9 条注释-代码不符 + (b)7 条状态置位后穿透 |
| 2 | `de146fab-04c8-43e5-9211-4507a75f88fb` | 同上（冗余复算，用于交叉验证） | **已回** | 6 阻断 + 8 须修，与 #1 在 p2 三条、p3 两条上独立复现 |
| 3 | `f53a49e6-4f50-42c3-b5ec-77aa1ed3bc20` | 测试层自洽式/恒真门断言 | **已回** | 9 阻断 + 8 须修 + 9 建议 |
| 4 | `b6ec6b39-d30d-4c79-84a2-365f79cd27ee` | 同上（冗余复算） | **已回** | 6 阻断 + 7 须修，独立发现 #3 未见的 B1（未知组名退出 0）与 #4 未见的 CHECK_EQ 无钩子 |
| 5 | `58f79789-c44a-4c76-9d6c-32da53a8b872` | 私建线程池 / 硬编码 / 数值稳定性 / 文档漂移 | **已回** | 7 阻断 + 12 须修 + 6 建议；**独立定位 `e5f589a6` 删除 `eng/ci/` 的治理根因** |
| 6 | `a7d353e5-9859-48d8-a5a0-eb56f77f9cb1` / `ccc11400` | 悬空引用 / 退役调用者 | **已回** | 假零消费者（B-13）、lifecycle 8 函数无生产实现、全域悬空路径表 |

（派发口径：5 个关注面，其中悬空引用面拆两路并发以交叉验证，合计 6 个 job id；全部已回报告。）

### ⚠️ 子代理推翻了我三条自己的判定（逐条记录，含我的错）

| 我的初判 | 子代理的证伪 | 我的处置 |
|---|---|---|
| `module_adapters.h:31` 的「`psf_params` 零消费者」我判 **✔**（"罕见的退役对象主动标注零消费者"） | `module_ports.registry.json:325` 登记 3 个下游消费者；`module_adapters.cpp:3325` 复述同一假声明 | **我错了**。我犯的错是：只读了被审文件本身、没反向 grep 被声称的对象。升级为 **B-13 阻断**，并注明这正是负责人本轮点名「已实测存在一例」的同一缺陷**再次发生** |
| `wcs_degrade_policy.h:48` 锚 `ipv_types.h:251,277` 我判"**实测正确**" | `grep -n` 实为 **250,276** | **我错了**。我的 `sed -n '249,252p;275,278p'` 把首行空行算漏了，事后未用 `grep -n` 复核。已在 §3.4 改为"锚点错" |
| `p3_session.cpp` 私建线程池 = 违规（先判后撤） | `executor.cpp:20` "RT-004 消灭 per-run 池" 与 `mosaic_window.h:137-139` 直接对立 | **恢复该项但改定性**：不是实现违规，是**本片自带两套互斥线程规则**，见 §6 |

**教训（已写入本交付件）**：我前两条错误的共同根因是**只读被审文件、不反向核验它对外部对象的断言**。这恰恰是本片最重要的方法论教训——`module_adapters.h:31` 和 `p3_export.cpp:1-33` 是**同一类缺陷的两种表现**（假零消费者 / 假"他域锚"），而我最初只抓到了后者。

### 我**否决**的子代理结论（3 条）

| 被否决项 | 提出者 | 否决理由 |
|---|---|---|
| `fits_stream_v1.h` 同时 include 两个 ABI 头 ⇒ 硬编译冲突 | 我自己（先行假设） | **我自己否决**：复核 `#include` 指令，该文件只 include `<stddef.h>/<stdint.h>`，grep 命中来自注释。无 TU 混用。不列缺陷 |
| `p1_session.cpp:212` `const json& operator[]` 缺键 ⇒ UB | #1 finding 9 | **部分否决**：风险真实（`:449` 自陈 run 无 validate 前置），但我未确认 nlohmann 在本仓构建配置下的确切行为（assert vs UB）⇒ **降级为须修**，不按阻断计 |
| `p2_session.cpp:203` "绝对 1e-6 容差原理上不可达" | #1 finding 3 / #2 B2 | **保留但降级**：收敛判据从未被读取（我独立确认 `p2_upm_convergence` 零调用）属实；但"绝对容差不可达"依赖 `upm.h` 外部合同，**不在本片 63 份内**，我未亲读全文 ⇒ 列为须修并标注证据来源为子代理转述 |
| `upm.cpp`/`sampler.cpp` 的 6 处线程池 | #5 A2 | **不计入本片**：那些文件不在本片 63 份成员内，仅作旁证记录，不进本片发现清单 |

### 我**补上**的、子代理未覆盖的（5 条，全部本片内）

1. **AIO 自洽式对齐证明**（`aio_abi_v1.h:142-165`）— 6 个子代理无一覆盖，且是本轮最标准的"同一式当被检量又当期望量"。
2. **`phase1_session/README.md` §2/§3/§5 vs §7/§8 的两两自相矛盾** — 子代理只看了测试层，没把 README 当被审材料。
3. **`p3_export.cpp:15` vs `:29` 的 `CMakeLists.txt:1510`/`:980` 块内矛盾**。
4. **FNV-1a 种子少一位数字**（`io_adapter.h:45`）。
5. **`include/acsd/README.md:15` core 头计数 24 vs 实测 26**、`lib/README.md:15` infra 子目录层级错误。

---

## 8 自证段（可复跑命令）

全部为只读命令，在 `/workspace/Astro CS Database` 下执行。中文路径一律已用 `git -c core.quotepath=false`（本次未用 git 子命令，故无实际调用）。

```bash
# ── S0 成员清单与覆盖率 ─────────────────────────────────────────
python3 - <<'EOF'
import re,os
p="run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
L=open(p,encoding='utf-8').read().split('\n')
s=[i for i,l in enumerate(L) if '片号: LIB-OTH-001' in l][0]
e=[i for i,l in enumerate(L) if i>s and l.strip().startswith('- 片号:')][0]
fs=re.findall(r'^\s+- "([^"]+)"','\n'.join(L[s:e]),re.M)
tot=0; miss=[]
for f in fs:
    if not os.path.exists(f): miss.append(f); continue
    tot+=sum(1 for _ in open(f,encoding='utf-8',errors='replace'))
print("members",len(fs),"missing",miss,"lines",tot)
EOF

# ── S1 恒真门：生产无路径可写 "complete" ────────────────────────
grep -n 'complete' lib/phase1_session/p1_session.cpp
grep -n 'manifest\["status"\]' lib/phase1_session/p1_session.cpp
grep -n '"complete"' lib/phase1_session/tests/p1sess/p1sess_tests_units.cpp \
                lib/phase1_session/tests/p1sess/p1sess_tests_properties.cpp
# 预期：complete 仅出现在 522/526/527 三条注释；三处断言写作 != "complete"

# ── S2 README 自相矛盾（重 1 根因） ────────────────────────────
grep -n 'status\]="complete"\|→ `complete`\|nullptr' lib/phase1_session/README.md
grep -n 'status="partial"' lib/phase1_session/README.md
sed -n '479,484p' lib/phase1_session/p1_session.cpp     # 代码已传真实 dark/bias

# ── S3 退役块锚全部悬空（重 2） ─────────────────────────────────
ls -d eng/ci                      # 预期：No such file or directory
grep -c p3_export CMakeLists.txt  # 预期：2（块内声称 = 0）
sed -n '1483p;1510p;980p' CMakeLists.txt
ls lib/phase3_session/            # 预期：无 hips_properties.h

# ── S4 AIO 自洽式对齐证明（重 3） ──────────────────────────────
grep -n '#include' lib/include/acsd/io/aio_abi_v1.h   # 只有 stddef/stdint
sed -n '142,165p' lib/include/acsd/io/aio_abi_v1.h    # 只比整数字面量
grep -rn 'acsd_fio_status' lib/include/acsd/io/aio_abi_v1.h   # 仅注释提及

# ── S5 FNV 种子 ────────────────────────────────────────────────
grep -n '1469598103934665603' lib/include/acsd/io/io_adapter.h
python3 -c "print(14695981039346656037/1469598103934665603)"   # 预期：10.0

# ── S6 selfcheck 判据 ──────────────────────────────────────────
sed -n '49,80p;150,176p' lib/phase1_session/tests/p1sess/p1sess_tests_selfcheck.cpp
sed -n '104,115p' lib/phase1_session/tests/p1sess/p1sess_test_main.hpp
sed -n '169,181p' lib/phase1_session/tests/p1sess/p1sess_test_main.hpp

# ── S7 静默降级 ────────────────────────────────────────────────
sed -n '318p;396p;86p;392p' lib/phase1_session/p1_session.cpp
sed -n '233,235p;221,224p;239p;46p' lib/phase2_session/p2_session.cpp
sed -n '636,638p;831,837p' lib/phase3_session/p3_export.cpp
sed -n '352,356p;270p;357,362p;200p' lib/phase3_session/p3_session.cpp

# ── S8 悬空声明与缺实现 ────────────────────────────────────────
grep -rn 'acsd_status_name_v1\|acsd_status_domain_name_v1' lib/ eng/
grep -rln 'acsd_lc_transition_allowed_v1\|acsd_negotiate_v1' lib/ eng/ --include=*.c --include=*.cpp

# ── S8b 假零消费者（B-13，负责人点名的同类缺陷再次发生） ────────
sed -n '31p' lib/include/acsd/core/module_adapters.h
grep -rn 'psf_params' --include=*.cpp --include=*.h --include=*.json lib/ | head
grep -n '下游消费者' lib/infrastructure/pipeline/module_ports.registry.json

# ── S8c 线程规则两派互斥（§6） ─────────────────────────────────
grep -n '不持有.*线程池\|run 作用域' lib/include/acsd/core/mosaic_window.h \
    lib/include/acsd/core/export_stream.h lib/include/acsd/core/normalize_workflow.h
grep -n '消灭 per-run 池\|不再自建' lib/infrastructure/scheduler/src/executor.cpp
grep -n '禁自建线程池\|静态扫描拒绝' lib/include/acsd/common_abi_v1.h lib/include/acsd/core/executor.h
grep -rn 'budget\.acquire\|budget\.release' lib/ --include=*.cpp

# ── S8d 数值稳定性（B-15） ─────────────────────────────────────
python3 -c "import struct;f=1.401298464324817e-45;print('ivar=',1/f,'FLT_MAX=',3.4028234663852886e+38,'overflow=',1/f>3.4028234663852886e+38)"
sed -n '108,112p' lib/include/acsd/core/variance_floor.h

# ── S8e 治理提交根因（G08-01） ─────────────────────────────────
git -c core.quotepath=false log --oneline -1 --diff-filter=D -- eng/ci
# 预期：e5f589a6 G08-01 物理删除旧门禁与 CI（344 件 / -136676 行）

# ── S9 文档计数与行号锚 ────────────────────────────────────────
ls lib/include/acsd/core/*.h | wc -l                # 预期 26（README 称 24）
grep -n 'sp.maxStars' lib/infrastructure/scheduler/src/module_adapters.cpp
sed -n '4896p;4921p' lib/infrastructure/scheduler/src/module_adapters.cpp
for L in 71 152 225 262 295; do printf "p1_session.cpp:%s " $L; sed -n "${L}p" lib/phase1_session/p1_session.cpp; done

# ── S10 测试层恒真/筛信号 ──────────────────────────────────────
grep -n 'px.size() == 64\|!= "complete"\|>= 1' \
  lib/phase1_session/tests/p1sess/p1sess_tests_units.cpp \
  lib/phase1_session/tests/p1sess/p1sess_tests_properties.cpp \
  lib/phase1_session/tests/test_p1_session_manifest.cpp
grep -rn 'stage_events' lib/phase1_session/tests/       # 只有声明+自增，零读
```

---

## 9 交付摘要

| 指标 | 值 |
|---|---|
| 片号 | `LIB-OTH-001` |
| 覆盖率 | **63/63 份 · 10649/10649 行 · 100.00%** |
| 阻断 | **15** |
| 须修 | **32** |
| 建议 | **9** |
| 派发子代理 | **6**（5 个关注面，悬空引用面拆两路交叉验证），**全部已回报告** |
| 子代理推翻我的判定 | **3 条**（假零消费者 / ipv 锚点 / 线程池定性）— 见 §7 |
| 我否决的结论 | **4 条**（含我自己 2 条先行误判） |
| 发现**新**问题 | **是** — 11 条仓内既有登记未覆盖 |

**给负责人的四句话**：

1. **本片不是"几个 bug"，是证据链本身失效**。承载验收关键词「取消/失败不会写完成 manifest」的三处门恒真（生产无路径可写 `"complete"`）；证明"能红"的自检判据是 `child_rc != 0`（fork/exec 失败=127、崩溃=-1、fixture 失败=2 全部判绿）。**在证据链修好之前，"这套测试是绿的"不携带任何信息量。**

2. **恒真门的根因在本片文档里可逐字指出**：`README.md` §3/§5 说 `complete`，§7 说 `partial`，§8-3 说 cosmetic 母版"已更正"而 §2/§3 仍说 `nullptr`。同一文件四个小节两两对立。

3. **G08-01 的删除留下成片无人认领的悬空理由**：`e5f589a6` 删掉 `eng/ci/**` 后，`p3_export.{h,cpp}` 的 1163 行退役代码仍用这些已删文件作"不能删"的护身符，且它自陈的举证 `grep -c p3_export CMakeLists.txt = 0` 实测是 **2**。**必须与 G08-01 同批回填。**

4. **负责人点名的"假零消费者"缺陷在本片再次发生**：`module_adapters.h:31` 称 `psf_params` 零消费者、psf 端口是死边，而 `module_ports.registry.json:325` 登记了 3 个下游消费者。**这是我自己的漏判**（我只读了被审文件、没反向 grep 它断言的对象），已订正并列为 B-13。

**建议整改次序**：① 先修证据链（B-01/B-07/B-08/B-09）→ ② 再修静默降级（B-10/B-11/B-13/B-14/B-15）→ ③ 与 G08-01 同批回填全部悬空理由（B-04/B-05/B-06/M-21/M-25）→ ④ 最后清理合同自相矛盾（线程规则两派）。