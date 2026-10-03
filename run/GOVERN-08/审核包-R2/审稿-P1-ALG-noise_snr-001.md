# 审稿-P1 · ALG-noise_snr-001（第 1 遍 · 对抗审稿）

- **片号**：ALG-noise_snr-001
- **层**：`lib/algorithms/noise_snr`
- **基线**：HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`（已实测）
- **口径**：负责人裁定「一遍 = 对同一片材料的一次完整重读」。本片我**亲自逐行读完 21/21 个成员**，未抽样、未换焦点。
- **裁决取向**：红队。默认现行结论为错；所有「检查通过」一律不作为正确性证据。
- **纪律**：零 git 写（未 add/commit/checkout/reset/stash）；未编译、未跑 ctest/pytest/构建/任何二进制；未改任何仓内源码；未读 `/tmp/acsd_g08/`。

---

## 1. 读完了吗

**口径声明**：
- 「成员份数」= 片清单列出的文件数；
- 「成员总行数」= `wc -l` 实测（含末行无换行时按 `read` 工具 total 计），与片清单 `实际行数: 6582` **完全一致**（已独立核对）；
- 「实际读了多少行」= 我用 `read` 工具读到 `End of file` 的行数。

| 项 | 值 |
|---|---|
| 成员份数 | **21** |
| 读了几分 | **21** |
| 成员总行数 | **6582** |
| 实际读了多少行 | **6582** |
| **覆盖率** | **100.0% （21/21 份，6582/6582 行）** |
| 未读完的 | **无** |

**未读完清单：空。** 本片无任何文件因超时/截断/跳过而未读完整。

逐份实测行数（`wc -l`，与片清单 6582 合计一致）：
`module_entry.cpp` 1635 · `snr_estimator.cpp` 932 · `snr_estimator.h` 647 · `noise_model_science_test.cpp` 603 · `p1snr_science_test.cpp` 484 · `information_weight.cpp` 320 · `snr_science.cpp` 282 · `p1noise_saturation_test.cpp` 277 · `snr_frame_science.cpp` 214 · `types.h` 204 · `check_snr_caliber.py` 166 · `memory.md` 146 · `check_saturation_wiring.py` 135 · `p1noise_tests_perf.cpp` 125 · `module.yaml` 119 · `snr_frame_science.h` 96 · `wrapper_phase1/README.md` 79 · `cpp/build.ps1` 58 · `p1noise_abi_layout_lock.json` 41 · `module_exports.map` 13 · `acsd_p1_noise.def` 6

**为核实影响面而额外读取的非成员文件**（不计入覆盖率）：`cpp/src/noise_model.cpp`（兄弟片 002，:1320-1419 fill 消费面）、`CMakeLists.txt`（根，:386-426 / :1128-1157）、`lib/algorithms/noise_snr/CMakeLists.txt`、`eng/packaging/config/defaults.json`。

---

## 2. 本片判定

# 🔴 阻断（BLOCKING）

### 最重 3 条

**B1（最重）— `fill_noise_field` 的模型 round-trip 读的三个键，估算通道从不写出 ⇒ 无空间场模型整幅噪声场被静默清零，且函数返回成功。**

`src/module_entry.cpp:1285-1287`（fill 通道重建影子模型）：

```c
shadow.sigma_bg_global    = json_get_f64(manifest, NOISE_M_KEY_SIGMA_BG, &f);
shadow.variance_bg_global = json_get_f64(manifest, NOISE_M_KEY_VARIANCE_BG, &f);
shadow.ivar_bg_global     = json_get_f64(manifest, NOISE_M_KEY_IVAR_BG, &f);
```

而估算通道的输出 manifest（`src/module_entry.cpp:1115-1119`）写出的键只有：
`op, rc, dtype, h, w, n_control_points, n_qualified_patches, n_rejected_patches, source, has_spatial_field, degenerate, variance_floor, variance_floor_clamped, variance_floor_status` + `scalars_base64` + `ctrl_*`。

**`sigma_bg_global` / `variance_bg_global` / `ivar_bg_global` 三个键从未作为 JSON 键写出**，只被塞进 base64 的 `scalars_base64` 通道（`module_entry.cpp:1133-1135`），而 fill 通道**不读**该通道。

**独立佐证（死词表）**：全仓 grep `NOISE_O_KEY_SIGMA_BG|NOISE_O_KEY_VARIANCE_BG|NOISE_O_KEY_IVAR_BG` → **9 处命中全部在 `include/acsd/noise/types.h:149-151` 的 `#define` 本体，别处零引用**。产出侧宏完全没人用，正是产出路径缺失的物证。同批未被引用的还有 `NOISE_O_KEY_FLOOR_FALLBACK`、`NOISE_O_KEY_WORKERS`、`NOISE_O_KEY_DIAG_VARIANCE`、`NOISE_O_KEY_DIAG_GAIN_VAR`。

**下游数值后果（已追到消费端）**：`cpp/src/noise_model.cpp:1399-1406`，`fill_impl` 的 `else` 分支（`!(has_spatial_field && n_control_points>=4)`）逐像素写：

```c
const double var  = m->variance_bg_global;   // 恒 0.0
const double ivar = m->ivar_bg_global;        // 恒 0.0
out_variance[i] = (float)var;  out_ivar[i] = (float)ivar;
```

按同文件 `:1367-1375` 的三态表，`variance=0 ∧ ivar=0` 的语义是**「该处方差不可用 / 显式不可用」**。即：**整幅平面被标成不可用**，`rc=0`，`ACS_OK`。

**被推翻的现有结论**：`module_entry.cpp:1220` 明文宣称「对拍口径: direct 通道与影子通道现在都吃同一个显式 floor ⇒ **仍 bitwise 一致**」；`:14-16`、`:1037-1038` 重复同一宣称。**该宣称为假**——两通道的全局兜底标量根本不同源。

**次生矛盾**：同一事务的两个观察面对 `floor_fallback` 给出相反值——输出 manifest 写 `"floor_fallback":0`（`module_entry.cpp:1385`，注释 `:1382-1384` 明确说旧代码「恒写 1」是错的、已改 0），而 `inspect()` 读的实例字段 `inst->last_floor_fallback` 被**无条件置 1**（`module_entry.cpp:1442`）。

**缓解事实（必须如实记录）**：`module_entry.cpp` 当前不在根构建内（见 B2），故这是**潜伏缺陷**——接线当日即发作。但代码本身是错的，且它自证「已修复 fail-open」的那段注释恰恰掩盖了本条。

---

**B2 — `src/module_entry.cpp`（1635 行 = 本片 24.9%）不被任何可达构建目标编译；同层 `module.yaml` 对本层构建与线程的事实声明已全面失真。**

实测证据链：
- 唯一列出该文件的构建文件是 `lib/algorithms/noise_snr/CMakeLists.txt:34`，而该子图自述 `:26`：「当前处于注释态（），本子图 `acsd_p1_noise`(SHARED, :35) **不在根图**」；
- 根 `CMakeLists.txt:395`：「本 add_subdirectory **仍保持注释**（未恢复）⇒ `acsd_p1_noise`(SHARED, 子图 :35) 不在根图」；`:386`「目录保持 untracked 锁死不收编」；`:426`「acsd_p1_noise 随 snr_estimator 断链解除摘出——target 未入库」。
- 反证：根 `CMakeLists.txt:1128-1131` **确实**建了 `acsd_phase1_noise`（STATIC），源为 `cpp/src/noise_model.cpp` + `wrapper_phase1/snr_frame_science.cpp` + `cpp/src/snr_science.cpp`；`:1138-1144` 在 UNIX 下链 `OpenMP::OpenMP_CXX`。

因此 `module.yaml` 的以下声明与实测全面冲突：
- `:41` `entrypoint: MISSING` —— 与 `module_entry.cpp` 存在并导出 `acsd_module_query_v1`（`:1619-1632`）冲突（此项属口径滞后，可辩）；
- `:19-20`「未编入根 CMake 主构建（无 snr_estimator CMake 目标）」—— 对 `cpp/src/snr_estimator.cpp` 成立，但**对本层整体不成立**；
- **`:22`「现状实现为单线程顺序（noise_model_impl 无 omp pragma）」** —— 对 `noise_model.cpp` 成立，但**对整层为假**：`wrapper_phase1/snr_frame_science.cpp:112-114` 与 `cpp/src/snr_estimator.cpp` 的 10 处 `#pragma omp parallel for`（:151/:169/:220/:235/:254/:325/:341/:389/:404/:423）都在同一层、且经 `CMakeLists.txt:1142-1143` 在 UNIX 生产构建中**真实生效**。
- `:107-119` notes「禁止声明 IMPLEMENTED（迁移落码未发生）」—— 而迁移落码（`module_entry.cpp`）就在目录里。

**后果**：模块合同层面对「本层跑在哪个构建图上、是不是串行」给出的是**结构性错误答案**。这与负责人已实测到的「退役对象仍有活调用者」是同一族：声明面与代码面脱钩。

---

**B3 — `cpp/test/noise_model_science_test.cpp`（603 行，本片 SNR-001..014 全部科学矩阵）既未被任何构建引用，且按其自述编译行**根本无法编译**。**

- 无 `CMakeLists.txt` / `*.cmake` / `*.sh` / `*.py` / `*.yml` 引用它（全仓 grep，剔除 `run/`）；`cpp/Makefile:4` 的 `SRCS` 只列三个生产源，无测试目标；`cpp/build.ps1` 无测试规则。
- 它自己 `noise_model_science_test.cpp:20` 给的编译行：
  ```
  g++ ... noise_model_science_test.cpp ../src/snr_estimator.cpp ../src/noise_model.cpp
  ```
  **漏了 `../src/snr_science.cpp`**。而该测试 `:315` 调 `snr_phot_cal_quality`（实现 `noise_model.cpp:1177`）→ 内部调 `snr_calib_zero_point_standard_error`，该函数**唯一定义点**是 `cpp/src/snr_science.cpp:275`。按其自述命令编译必然链接失败。

**后果**：全层 14 项科学判据（含 SNR-004 方差回收、SNR-005 Poisson+read、SNR-006 空间场、SNR-010 信号无关性）在 CI 中**一次都没被执行过**。这批判据此前若被当作「已验证」，属把不存在的执行当证据。

---

**B18 — 同一孤儿文件里还有一条在现行生产代码下**恒红**的断言，且与一个**在构建图内**的断言互斥 ⇒ 一个从未被点亮的红灯。**

被测断言 `cpp/test/noise_model_science_test.cpp:382-389`：
```c
const int rc_leg = snr_extract_model(psf_leg, 1, 0.1, &wcs_leg, &m_leg);   // :382
CHECK(rc_leg == 0 && m_leg.n_points == 1, "legacy extract 通道单星成功"); // :383
const double want = (5000.0 - 1000.0) / 120.0;                             // :385
CHECK(std::fabs((double)m_leg.points[0].snr_psf - want) < 1e-4, msg);       // :389
```

而生产 `cpp/src/snr_estimator.cpp` 早已改口径：
- `:64` 注释：「旧 (A-B)/residual_scale (SNR-008 退休量)**不再进入科学输出**」；
- `:630-631`：`// P5-SNR: 退休量 (A-B)/residual_scale -> Horne 1986 逐源最优提取 SNR` / `double s = sourceSnrFromPsfRow(row);`
- `:683`：`out_model->points[i].snr_psf = (float)star_snr[i];`（写的是 Horne `SNR_F`，公式里根本没有 (A−B)）。

**我自己重算了这个量**（不采信任何子代理，纯按 `sourceSnrFromPsfRow` + `autoHalf` 定义式，fixture 取 `:374-375` 的 `psf_leg`）：

```
half = 36                       (autoHalf(3.0): ceil(12*3.0)=36, 落在 [30,256])
sum_p2 = 0.0344173483
snr_psf (Horne SNR_F, 生产实际) = 33.934825
want   ((A-B)/residual_scale)   = 33.333333
|diff| = 0.601492      判据容差 = 1e-4
⇒ :389 **恒红**（差值约为容差的 6015 倍，且不是靠临界点取胜）
```

同段 `:394` 的 `fabs(snr_psf − 5000/120) > 0.5` 为绿（`|33.934825−41.666667|=7.73>0.5`）——即**一个文件内一条红一条绿**，且红的被「孤儿化」掩盖。

**互斥对**：本仓 `tests/p1noise/p1snr_science_test.cpp:430-432`（**在** `tests/p1noise/CMakeLists.txt` 的构建图内）对**同一生产量**断言**相反**命题：
```c
// 退休断言: 控制点不得等于旧 (A-B)/residual_scale
check(std::fabs(pts[i].snr_psf - old_ratio[i]) > 1e-6, b);
```
即「`snr_extract_model_v3` 输出**不得**等于 (A−B)/residual_scale」。两个文件对同一量给出互斥断言，**必有一红**；文件 2 在构建图内所以它赢，文件 1 的断言被冻结在「从未执行」状态，**连红灯都不显示**。

**这是「恒红门」最隐蔽的形态——不是恒红，是恒红且从未被点亮**。并且 `memory.md:139` 记录着「科学矩阵 `noise_model_science_test` **32/32**」、`docs/science/NOISE_MODEL.md:363` 记「测试: TST-NOISE-001..015 (noise_model_science_test.cpp)」——这些「通过」记录在现行代码下**不可能成立**。

---

**B19 — 退役声明 `LEGACY_SNR_SCIENCE_CONSUMER=0` 被证伪：被宣布为「零科学消费者」的符号，正是 stage6 SNR 的必需生产通道，取不到就硬失败。**

声明（`module.yaml:10-11`）：
```
# （P1-NOISE-DOC，2026-09-07，7 个 noise 路径生产符号，旧乘法 snr_estimate*/
# snr_extract_model* 为 legacy diagnostic 不入列），禁止手抄他版。
```
`memory.md:140`：
```
- legacy snr_extract_model_* 保留为 diagnostic/migration (LEGACY_SNR_SCIENCE_CONSUMER=0)
```

**我实测的生产消费者**（`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp`）：
```
4281:        ModuleId::SNR, "snr_extract_model_v3");        ← 取函数指针
4283:        ModuleId::SNR, "snr_free_model_v3");
4330:        LOG_ERROR("orchestrator", "[SNR] 函数指针获取失败 (snr_extract_model_v3 必需)");
4490:    // 调用 snr_extract_model_v3 提取稀疏控制点
```
`:4330` 的文案自陈「**必需**」，且失败即 `GENERIC_ERROR` 硬失败。

⇒ **"零科学消费者"为假**。精确表述应是：`snr_estimate*`（稠密图通道）确已退场；`snr_extract_model_v3` 是**在役必需**生产符号。三份文件把两者捆成一句「legacy 不入列」。

**这与负责人已实测到的「退役对象仍有活调用者」互为镜像且更严重**：那边是「声称已退役的其实还有调用者」，这边是「声称**零消费者**的其实是**必需消费者**」。两者都指向同一个治理失效——**符号可达性靠自述，无机器门守护**（而那条门 `eng/ci/check_prod_wiring.py` 已被删除，见 B16/B20）。

**B20 — `module.yaml` 的格式依据、迁移落位依据、可达性判据依据三者同时断链。**

| 引用 | 位置 | 实测 |
|---|---|---|
| `11_MODULE_SOURCE_TEST_STANDARD.md §4` | `module.yaml:9` | **不存在**（全仓零命中） |
| `MODULE_MIGRATION_MATRIX.csv` P1-NOISE 行 | `module.yaml:12` | **不存在**（全仓零命中） |
| `eng/ci/check_prod_wiring.py` W1 | `module.yaml:79` | **不存在**（`eng/ci` **整个目录**已从活动树删除） |
| `dormant_algorithms.json` 台账 | `module.yaml:82` | 同属被删的 `eng/ci/ledgers/`，活动树零命中 |

⇒ manifest 声明 `owner=SA-P1-N17`、`target`、`legacy_paths`、`depends_on_int` 的**唯一依据不可查**；而"符号可达性"的判据依据**已随门一起消失**——这正是 B16（错误裁剪 `source_symbols`）与 B19（错误退役声明）能够长期存活的**制度性成因**。

---

## 3. 逐文件清单（21 份，全部读完）

### 3.1 `src/module_entry.cpp`（1635 行）— 读完
**读了什么**：C ABI v1 adapter 全文 —— JSON 迷你解析、base64 编解码、config 解析、describe/validate/plan/create/inspect/cancel/destroy、三操作 execute、静态 vtable、唯一导出。
**看到什么 / 判定**：
- 🔴 B1（:1285-1287 vs :1115-1119；死词表 `types.h:149-151`）—— **阻断**。
- 🔴 潜伏：`last_floor_fallback` 无条件置 1（`:1442`）与 manifest 写 `floor_fallback:0`（`:1385`）矛盾 —— **须修**。
- ⚠️ 静默降级：`strbuf_write` `:115` `if (!out->data || out->cap == 0) return ACS_OK;`。合同里 `cap==0 ∧ data==NULL` 才是尺寸查询；**`data!=NULL ∧ cap==0` 是调用方错误，却被吞成成功**，且 `:114` 已把 `out->size = n` 写上，宿主据此读 `n` 字节即越界读 —— **须修**。
- ⚠️ 错误码不唯一：至少 14 处失败路径共用 `ACS_DIAG_ECODE_NONE`（detail=0），见 `:476 :836 :1052 :1064 :1106 :1144 :1302 :1315 :1339 :1376 :1403 :1434 :1506 :1533`。其中 `:474-479` 是**明确拒绝 `variance_floor` 非法**的安全门，却发 detail=0 —— **须修**。
- ⚠️ 取消语义二分：`:923-928` 实例 `cancel_req` → `ACS_ERR_STATE`+`ILLEGAL_STATE`；`:929-934` host cancel → `ACS_ERR_CANCELLED`。同一逻辑条件两种状态码，只认 `ACS_ERR_CANCELLED` 的宿主会把取消当状态错 —— **须修**。
- ⚠️ `:960-966` `noise_legacy_status`：rc=0 **与 rc=1（完全退化）同映射 `ACS_OK`**。注释自陈「rc=1 完全退化=成功面科学结果不映射错误」。退化与否只能靠调用方解析 manifest 的 `rc` 字段 —— **须修**（契约未强制）。
- ✅ 负面：泄漏路径清理完整（`:1186-1189`、`:1426-1429`），每条失败路径都有 free/free/release 配对；无 `catch(...) {}` 空捕获（全部走 `NOISE_ECODE_EXCEPTION`）。
- ✅ 负面：未私建线程池（纯事务内串行 + host executor 硬租约 `:940-956`），符合池所有权归调度器。

### 3.2 `cpp/src/snr_estimator.cpp`（932 行）— 读完
**看到什么 / 判定**：
- 🔴 **10 处 `#pragma omp parallel for` 无 `_OPENMP` 守卫**（:151 :169 :220 :235 :254 :325 :341 :389 :404 :423），而同层 `snr_frame_science.cpp:112-114` 正确加了守卫。文件 `:25-27` 已 `#ifdef _OPENMP #include <omp.h> #endif`，作者知道守卫写法却未用于本处 ⇒ **同一层两套口径，行为随构建配置漂移且无任何诊断** —— **须修**（升级理由见第 7 节 B-否决记录 #2：并行区本身经根 `CMakeLists.txt:1138-1143` 授权，不判为私建池，但守卫不一致判为缺陷）。
- 🔴 **日志说谎**：`:292-293` / `:460-461`
  ```c
  fprintf(stderr, "[snr] done: SNR = SNR_phot(%.6f) * (SNR_psf/median(%.6f)), ...");
  ```
  但 `:286-288` / `:454-456` 实际是 `double snr = snr_psf;`（**不乘任何帧级标量**）。运维据日志核对会得到与代码相反的结论 —— **须修**。
- 🔴 **同一模块内 F₅ 两份独立实现**：`:123` `*out_flux5 = 5.0 * med_sigma / std::sqrt(sum_p2);`（`frameDepthFromPsf` 闭式）绕过权威实现 `snr_science.cpp:257`，违反本文件 `:3` 自称的「科学公式零本地副本」与 `snr_estimator.h:369`「reference 必须来自显式参考源」 —— **须修**。
- 🔴 **`median(a)·f(median(b)) ≠ median(a·f(b))`**：`:114-115` 分别取 `median(FWHM)` 与 `median(σ_sky)` 再于 `:123` 组合。seeing 变差时二者正相关 ⇒ 系统性偏估。现有测试 `p1snr_science_test.cpp:450` 的期望值**编码了同一构造**，且 5 颗测试星 σ_sky 全同 ⇒ 结构上无法发现（见 3.5）—— **须修**。
- ⚠️ **两套不同的星筛选喂两个帧级中位数**：模型侧 `:860-862`+`:867`（还要 `vr.snr>0`），`frameDepthFromPsf` 侧 `:105-107`（**不查 `sourceSnrFromPsfRow` 是否 >0**）。被模型剔掉的星仍进深度中位数 —— **须修**。
- ⚠️ **错误码合并两种成因**：`:628`（`residual_scale<=0`）与 `:632`（`sourceSnrFromPsfRow` 返回 0）共用计数器 `n_skip_residual_scale`；`:640-642` 的 fprintf 标签 `"residual_scale<=0=%d"` 对后者**如实性错误** —— **须修**。
- ⚠️ **静默降级（空分支体）**：`:518-528` SIP order 越界时 `if` 体**只有一行注释**，静默退回 CD+TAN，无错误码、无 provenance，而 `:673-676` 的日志还照常打印 `"启用"` —— **须修**。
- ⚠️ **失败时输出部分写入**：`:653-657` 先写 `median_snr/snr_phot/median_source_snr` 与 frame_depth，`:661-664` 才 `return 1`；v2 同型 `:767-775`、v3 同型 `:882-890`。`n_points` 亦在 malloc 前先置位（`:777 :892`）—— **须修**。
- ⚠️ **异常穿越 C ABI**：`:668` `new SnrControlPoint[n_valid]` 无 try/catch；`sourceSnrFromPsfRow:88` → `snr_source_snr_f64` → `snr_science.cpp:184` `std::vector`。分配失败抛出的 C++ 异常会穿出 `extern "C"` 边界 ⇒ UB。v2/v3 用 `malloc` 且判空（`:781 :791 :896 :908`），v1 不一致 —— **须修**。
- ⚠️ **与头文件冲突**：`snr_estimator.h:583-585` 注释称 `snr_phot`/`median_snr`「均置 1.0」，代码 `:653-655` 置 `median_snr`（头 `:586-587` 的字段注释又写 `= median(SNR_F)`）—— 头内自相矛盾 + 注释与实现不符 —— **建议**。
- ⚠️ 退化填充违反项目自订规则：`wrapper_phase1/README.md:52-53` 明文「`snr=1.0` **不得作为 unknown 伪装**（退化行保持 NaN/null）」，但 `:153 :171 :222 :237` 全部填 1.0 —— **建议**（legacy diagnostic 面，已有 rc=1/2 区分）。
- ✅ 负面（退役声明核验）：`snr_estimator.h:399 :413` 自称 snr_estimate「管线中不再调用」—— 实测**属实**，生产侧只有 `orchestrator.cpp:4247` 的一句注释，无调用点。
- ✅ 负面：`new[]`/`delete[]`（`:668/:706`）与 `malloc`/`free`（`:779-791`/`:807`）各自配对自洽，**无分配器错配**。

### 3.3 `cpp/include/snr_estimator.h`（647 行）— 读完
- 🔴 **悬空引用**（权威口径文件不存在）：`:311` 「口径正本 `docs/detail/algorithms_phase1/07_noise_snr.md` §4.2a」。实测 `docs/detail/algorithms_phase1/` **整个目录不存在**（`docs/detail/` 下只有 `00_INDEX.md anchors common.md infrastructure registry *.md`）。同型引用另见 `wrapper_phase1/snr_frame_science.h:60`、`cpp/src/snr_science.cpp:175`、`tests/p1noise/p1snr_science_test.cpp:299`，共 4 处。⇒ **SCI-B D1 读噪双计规则的法源在现行树无文件承载** —— **阻断级证据链断裂**（并入 §4 阻断）。
- ⚠️ **导出面在 POSIX 上根本不存在**：`:6-10` 非 `_WIN32` 时 `SNR_API` 为**空宏**。叠加 `module_exports.map` 把七符号降 local ⇒ Linux 侧这些符号既非导出也不可见。而 `memory.md:62-65` 称生产经 `dll_loader.cpp:41` 装载 `snr_estimator.dll` 并取函数指针 —— 口径不自洽 —— **须修（需前台确认平台事实）**。
- ⚠️ **`acsd_p1_noise.def` 的技术性错误**：`src/acsd_p1_noise.def:2-3` 称「legacy snr_noise_* 七符号（SNR_API dllexport 携带）经本 DEF 过滤降 local」。MSVC 下 `__declspec(dllexport)` 符号**无法被 DEF 降为 local**；DEF 只增不减 —— **须修**。
- ⚠️ **C ABI 契约缺口**：`:358-378` 对 `snr_source_snr_f64` / `snr_frame_depth_f64` / `snr_moffat4_profile_f64` **完全未声明返回码契约**，而同头 `:397 :605` 声明得很完整 —— **建议**。
- ⚠️ **返回值当缺省哨兵**：`:333` `zero_point_mag` 以 `0.0` 表「未提供」；`:129` `variance_floor` 默认 1e-12 硬编码在本头（`:146`）。真实值 0 与「缺失」不可区分 —— **建议**。
- ✅ 正面：`:123` 的 `1.482602218505602` 经我独立复核 **正确**（= 1/Φ⁻¹(0.75)，正态 MAD 一致性常数）；`:487 :499 :517 :529` 四条 `static_assert` 布局锁齐备；`:478-529` `#pragma pack(1)` 与根因注释（20 字节 memcpy 错位）自洽。

### 3.4 `cpp/test/noise_model_science_test.cpp`（603 行）— 读完
- 🔴 B3（孤儿 + 不可编译）—— **阻断**。
- 🔴 **自洽式断言**（负责人指定最高价值类）：`:369-370`
  ```c
  CHECK(std::fabs(q1.q_psf - q1.amplitude_above_bg / q1.residual_scale) < 1e-12, ...)
  ```
  生产 `noise_model.cpp:1233` 就是 `r.q_psf = r.amplitude_above_bg / r.residual_scale;`。期望值用**生产自己的输出字段**代数还原 ⇒ **结构上不可能失败**，而消息却宣称锁「q_psf 分母 = residual_scale（非 MAD）」—— **阻断级恒真门**。
- 🔴 **恒真门（零生产符号）**：`:400-421` SNR-009 整块**不调用任何生产函数**。`var_opt`(0.9) `<` `var_eq`(2.5) 是两个字面量的事实；`var_w`（`:415-416`，w₁=1, w₂=1/9）**代数上恒等于** `var_opt`（10/9 ÷ 100/81 = 0.9），故 `:420` 的 `<0.03` 恒绿。`:413` 注释称「3% 容差覆盖 MC 估计误差」——**该 MC 在文件里不存在** —— **阻断级**。
- ⚠️ **硬编码阈值偏离冻结文档**：`:220` `|bias| <= 0.02`，而冻结 `NOISE_ESTIMATION.md` 的 SNR-004 oracle 是 **≤5%**；`:307` `norm_rmse <= 0.05` 替代了登记的「拟合系数 10%」统计量（该统计量**根本没实现**）—— **须修**。
- ⚠️ **筛掉真信号**：`:227-230` `frac` 的分母是 `m.n_control_points`，而 `noise_model.cpp:1085` `n_control_points = patch_var.size()` ≡ `n_qualified_patches` ⇒ **被拒 patch 不进分母**；最可能 σ 出错的恰是被拒的那些 ⇒ ≥95% 门结构性偏绿 —— **须修**。
- ⚠️ **悬空引用**：`:4` `SNR_SCIENCE_DERIVATION.md` / `SCIENCE_ACCEPTANCE_MATRIX.md` 均不存在（全仓 `SNR_SCIENCE_DERIVATION` 在 .md 中零命中）；`:20` 编译行缺 `snr_science.cpp`（见 B3）—— **须修**。
- ⚠️ **陈旧文档块**：`:9-15` 描述 256²/星(112,112)/F=2.8e11/32² patch，代码实为 512²/(288,288)/F=1e14/64² —— 与本文件 `:199`「星在 (256,256)」也自相矛盾 —— **建议**。
- ⚠️ **死工**：`:430-439` 构造的 `ms` 模型与 `ivs` 数组**从未被断言**（SNR-010 只用了 `mx/ivx`），本该做的「纯天空 vs 含星场景」对比被删 —— **建议**。
- ✅ 正面：`:500-505` 正确钉住全 NaN 帧 ⇒ `rc==1 ∧ degenerate==1 ∧ ivar_bg_global==0.0`；`:91-92 :102` `pearson` 的 `2.0` 哨兵设计正确（2.0 同时通不过 `>=0.98` 与 `<0.02`）。

### 3.5 `tests/p1noise/p1snr_science_test.cpp`（484 行）— 读完
- 🔴 **恒真门**：`:209-210`
  ```c
  checkClose(kGaussFwhmFactor / kMoffat4FwhmFactor, 1.9140054498711294, 1e-15, "cross-block sigma bias = 1.914005");
  ```
  **零生产符号**：两个测试内 `constexpr` 相除，比第三个测试内字面量。除人改字面量外永不可能失败；若打错则是**纯内部恒红**。纯文档冒充断言 —— **阻断级恒真门**。
- 🔴 **生产代码当自己的 oracle**：`:449-451`
  ```c
  snr_moffat4_profile_f64(0.0, med_fwhm / kMoffat4FwhmFactor, 0, &sp2, &pc);
  checkClose(m.frame_depth_flux5_adu, 5.0 * kRealSky / std::sqrt(sp2), 1e-12, ...);
  ```
  调用式与 `snr_estimator.cpp:120-121` **逐字相同**，期望式是 `snr_estimator.cpp:123` 的转写。网格半边长、`alpha2`、归一化任一处缺陷会**两侧同步移动** ⇒ 假绿。`:439` 注释自认「与 snr_estimator.cpp frameDepthFromPsf 同款」—— **阻断级自洽式断言**。
- 🔴 **静默降级（测试侧）**：`:473-476` 两次 `snr_extract_model_v3(...)` 的**返回码被丢弃**，随后
  ```c
  check(m1.n_points == m2.n_points && m1.snr_phot == m2.snr_phot &&
        m1.frame_depth_flux5_adu == m2.frame_depth_flux5_adu, "determinism bitwise (extract_v3)");
  ```
  若两次调用**双双失败**（rc=1 见 `snr_estimator.cpp:874-877`），零初始化结构给出 `0==0 ∧ 0.0==0.0 ∧ 0.0==0.0` ⇒ **两次彻底失败反而判绿**。对照 `:421` 是**检查** `rc == 0` 的，同文件好坏并存 —— **阻断级**。
- ⚠️ **自洽式断言**（同族）：`:275` `5.0*r.sigma_f_optimal_adu` ↔ `snr_science.cpp:257`；`:276` `20.0-2.5*log10(f5)` ↔ `:261`；`:57-63 refHalf` ↔ `snr_science.cpp:98-103 autoHalf`（`12/30/256` 逐字相同，三个常数零证据资格）；`:48/:53/:54` 三个常数逐字复制生产（`0.7316727929211932` 在生产出现 6 次含 3 处裸字面量，无单一真源）—— **须修**。
- ⚠️ **三断言同源**：`:440-442` 对 `snr_phot/median_snr/median_source_snr` 各断言一次，而 `snr_estimator.cpp:882-884` **三行同赋一个 `median_snr`** ⇒ 后两条由第一条结构性保证 —— **建议**。
- ⚠️ **悬空引用**：`:7` `run/perf-fix/P5-snr/harness/snr_oracle.py` 不存在；由此产生的 5 个「NumPy oracle 锚值」（`:174-178`，含 `126.30302219525699`）**全仓零命中**，即本文件**唯一非自洽的外部证据不可复核**；`:10` `run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md` 不存在；`:299` 口径正本不存在 —— **阻断级证据链断裂**。
- ⚠️ **用法串漏组**：`:11` usage 串漏 `skysource`，而它是已登记 ctest 且承载 SCI-501 读噪双计守卫 —— **建议**。
- ⚠️ **绕过生产析构**：`:456 :477-478` 用 `std::free(m.points)` 绕过 `snr_free_model_v3`（`snr_estimator.cpp:925-930`）—— **建议**。
- ✅ 正面：`:285-290` 是四个测试文件里**最强的错误码覆盖**（nullptr→3；flux/fwhm/sky≤0 与 NaN → `rc==0 ∧ status==1`）；`:345-394` 的 SCI-501 组带**真 MC**（M=8000、固定 seed、`zA≤3 ∧ zB>3` 双向），是本片唯一具备第三方证据资格的判据；`:74-136 refCompute` 用 long double + 不同循环结构，**不是**逐字复制，能抓转写漂移。

### 3.6 `cpp/src/information_weight.cpp`（320 行）— 读完
- ✅ **负面：私建线程池未发现。** 纯 std + libm，无 `std::thread` / OpenMP / 线程池。
- ✅ **负面：筛掉真信号未发现。** `combine_point_estimates:300-304` 对 `!e.ok` **fail-closed 整段拒绝**（不静默跳过），符合纪律。
- ✅ **负面：数值稳定性良好。** `:37` `if (!(s > 0.0) || !std::isfinite(s)) return false;` 在 `sqrt` **之前**拒绝非正定；`all_finite` 先于所有 `1/x`。
- ⚠️ **头/实现拒绝串双向漂移**：`information_weight.h:58` 声明 `{null_input, dimension_mismatch, non_spd, singular}`，实现实际发出 8 种（`:118 :120 :122 :134 :150 :167 :187 :206 :211 :215 :228 :234 :246 :251 :256 :270 :284 :297 :302 :309`）。`dimension_mismatch` **永不发出**（死契约）；最常见的 `non_finite` 与 `non_positive_sigma2` **未声明** ⇒ 按头 switch 的消费者静默落 default —— **须修**。
- ⚠️ **冻结常数被挪用**：`kQwRelTol=1e-9`（`information_weight.h:30`）冻结语义是「Q/W == GLS 与 Var=1/W 相对容差」，唯一使用点 `:227` 却是 PSF 归一 `fabs(sum-1.0) > kQwRelTol` —— 完全不同的物理判据 —— **须修**。
- ⚠️ **诊断量口径可能不闭合**：`:259` `ideal_inv_w = 1/e.w_info`（含 `a²`）与 `:288` `variance_ratio = quad/ideal_inv_w`，而 `information_weight.h:101` 声称其 `= Var_approx/Var_true`（真 `Var_true = 1/(pᵀC⁻¹p)` 不含 `a²`）。`a≠1` 时该等式为假。**生产未调用 `diag_approx_report`**（UNVERIFIED 需 `NOISE_MODEL.md` §5c 定 `a` 的物理口径）—— **建议/UNRESOLVED**。
- ✅ **负面（自洽式断言排查）**：`:288` 的 `quad`（`c̃ᵀCc̃`，真实 C）与 `ideal_inv_w`（`1/(a²pᵀC⁻¹p)`）是**两个不同物理量**，Fisher 下界比值有意义 ⇒ **不是**同式自洽断言。

### 3.7 `cpp/src/snr_science.cpp`（282 行）— 读完
- 🔴 **静默降级（无科学推导的默认值）**：`:226`
  ```c
  const double n_sky = (p->n_sky > 0.0) ? p->n_sky : n_pix;
  ```
  `n_sky` 缺失 ⇒ 静默取 `n_pix`，使 `var_ap` 的 `(1 + n_pix/n_sky)` 恒为 **2.0**。典型 `r_out=3r` 天空环真值 `n_sky=8πr²=8·n_pix` ⇒ 真因子 **1.125**。⇒ **天空方差高估 1.78×、σ 高估 1.33×**。无错误码、无 status、无 provenance，调用方**无法分辨**是量测还是兜底 —— **阻断级静默降级**（AGENTS §6「可由输入几何导出的改为现场计算」直接命中）。
- 🔴 **异常穿越 C ABI**：`:165` `half = (p->profile_half_px > 0) ? p->profile_half_px : autoHalf(fwhm_eff);` —— 显式传入的 `profile_half_px` **绕过 `autoHalf` 的 256 上界**；`:185` `v.reserve((size_t)(2*half+1)*(2*half+1))` 中 `2*half+1` 是 **int 运算**，溢出后 `reserve` 可抛 `std::length_error`/`bad_alloc`。本 TU 位于 `:107 extern "C" {`，**无 try/catch** ⇒ 异常穿越 C 边界 = UB。`snr_estimator.h:334` 亦未声明上界 —— **阻断级**。
- 🔴 **静默降级（成功码 + 退化 status）**：`:158 :159 :160 :196 :212` 五条退化路径全 `return 0`（= 成功），只靠 `out->status=1` 区分；对照 `snr_moffat4_profile_f64:119-123` 对同类非法 sigma **返回 3**。两个相邻函数同类失败**语义相反** —— **阻断级**。
- 🔴 **恒红门（不可达分支）**：`:177` `rn_in_sky = (p->sigma_sky_source == SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS)` —— **枚举值域零校验**，任何 `∉{0,1,2}`（含负数、未初始化）静默落入 SHOT_ONLY 并回 `effective=1`，无拒绝路径；`snr_estimator.h:318-320` 只定义常量不定值域 —— **须修**。
- 🔴 **灾难性抵消**：`:224` `f_in = 1.0 - 1.0/(u*u*u)`。绝对误差 ≈ ulp(1)=2.2e-16 ⇒ 相对误差 = 2.2e-16/f_in。`f_in ≲ 1e-10` 时相对误差 > 2e-6。默认孔径 `f_in≈0.94936` 无影响，调用方传极小 `aperture_radius_px` 时退化 —— **建议**。
- ⚠️ **死副本常量**：`:56` `kTrimMeanToSigma = 0.7316727929211932` —— **全文件零引用**（与 `noise_model.cpp:95` 逐字相同，后者在 `:1232` 真用）。讽刺的是同文件 `:45-46` 的注释正声称「此处原为逐位等值的复制字面量且全文件零引用, **已删除**」——**同一缺陷的未修兄弟** —— **须修**。
- ⚠️ **硬编码可现场导出**：`:279` `1.253 * sigma / sqrt(N)`。我独立推导：高斯样本中位数渐近方差 `πσ²/(2N)` ⇒ 常数 `√(π/2) = 1.2533141373155003`；代码用截断值 `1.253` ⇒ **相对误差 −2.51e-4，系统性低估零点标准误**。属 AGENTS §6「可由物理导出的改为现场计算」 —— **须修**。
- ⚠️ **无出处的硬编码**：`:99-101` `ceil(12*fwhm)` / 下界 30 / 上界 256（`:101` 注释仅「安全上界」）。上界对 FWHM>21.3 px 的系统**静默截断**网格、偏置 `sum_p2`，无标志 —— **须修**。
- ⚠️ **注释与冻结网格规则不符**：`:69` 注释写 `half = (half_px>0)?half_px:max(30, ceil(12*fwhm))`，漏掉 256 钳位，oracle 复现时会取错半边长 —— **建议**。
- ✅ **正面**：`:204` `if (k == v.size()/2) p_center = Pi;` 中心索引推导成立（`(2h+1)²` 为奇数，中位索引即几何中心）；`:177-182` SCI-B D1 读噪单计逻辑与其 provenance（`sigma_sky_source_effective`）设计正确且有 MC 背书。

### 3.8 `tests/p1noise/p1noise_saturation_test.cpp`（277 行）— 读完
- 🔴 **恒真门（哨兵值把断言洗白）**：`:219-220`
  ```c
  check(on.plateau_patch_var < off.plateau_patch_var, "G2d ...");
  ```
  但 `:136` `r.plateau_patch_var = -1.0;` 作「patch 不存在」哨兵。**过滤生效时 `on.plateau_patch_n == 0` ⇒ `plateau_patch_var == -1.0` ⇒ `-1.0 < (任何正数)` 恒真**。即：**恰恰在过滤正确工作（patch 被剔除）时，G2d 必然通过**。注释称「过滤直接作用面」，实为哨兵洗白。对照 `:217` 的 G2c 是正确写法（显式判 `plateau_patch_n == 0`）—— **阻断级恒真门**。
- 🔴 **测试把 fail-open 固化为「正确」**：`:176-178` 断言 `resolve_saturation_level(nullptr,nullptr) == 0.0`；`:185-186` 断言 `saturation_filter_state(0.0) == "DISABLED_NO_METADATA"`；`:195-197` 断言默认配置必须显式声明为 `DISABLED_NO_METADATA`。而 `:230-231` G3a 自证该臂下平台 patch 成为污染控制点（`ctrl_var > 1e6` vs 真值 25，**4×10⁴ 偏置**）。⇒ **无饱和元数据 ⇒ 过滤静默关闭 ⇒ 方差场被污染，唯一信号是一个状态字符串**。按 AGENTS §6「无依据经验值改为自适应」，默认应改为保守推断（如 DATAMAX 或高分位），而非 0=关闭 —— **阻断级**。
- 🔴 **冻结文档与在运门断言相反**：`docs/science/NOISE_MODEL.md` §11 的 SAT-001 oracle 要求「帧平均 ivar ≥ 3× 未过滤臂」，而 `:234-236` 断言的是**反面**（`|off/on − 1| < 1e-3`，即两臂一致）。`:222-229` 的行内注释做了自洽辩护（归因 §5d 加权），但**权威文档未更新**。按 AGENTS §3（文档高于代码）与 §8（不以 waiver 静默覆盖红灯），测试侧注释不构成 waiver —— **阻断级**。
- ⚠️ **覆盖虚标**：`:6` 自称被测面为「生产源 `noise_model.cpp` + 策略头 `saturation_policy.h`」，但 G1a–G1n（13 项，`:169-197`）**只测头文件内的策略助手**；`noise_model.cpp` 既不 include 该头也不调这四个函数（唯一消费者是 `orchestrator.cpp` 与 `snr_estimator.h`）⇒ **同一语义存在两份独立实现，只钉住了一份** —— **须修**。
- ⚠️ **硬编码**：`:63` `FLAT_REL=0.05`、`:59-62` `SAT/MU/SIG/TRUE_VAR`、`:215 :216 :230 :232 :259` 的 `100.0*TRUE_VAR` 与 `1.0e6`、`:254` 的 `0.5*TRUE_VAR … 2.0*TRUE_VAR` —— 均无 docs 出处链 —— **须修**。
- ⚠️ **陈旧文档块**：`:9-15` 描述 256²/(112,112)/F=2.8e11/平台散布 655 ADU，与代码 `:63 :67-75 :199` 的 512²/(288,288)/F=1e14/`FLAT_REL=0.05` 全不符（散布应为 3277 ADU）—— **建议**。
- ✅ 正面：`:244-263` `group_selfcheck` 是**真自证**——在无源帧上要求 G3a 类判据必须为假（S1/S2/S3）、在有源帧上要求必须为真（S4），确实排除了恒真/恒假，比同批 `check_*.py` 的 `--self-test` 质量高一个量级。

### 3.9 `wrapper_phase1/snr_frame_science.cpp`（214 行）— 读完
- 🔴 **恒真门（我本片最确凿的恒真门之一）**：`:157-161`
  ```c
  if (indices_used != out.n_used) {
      out.reason = "internal catalogue count mismatch";
      out.valid = false;
      return out;
  }
  ```
  **可证不可达**：`out.snr_f[i]` 仅在 `:128` 被写，该行受 `:127 if (!rok[i]) continue;` 守卫；`rok[i]` 仅在 `:120` 置 1；`out.snr_f` 初值 NaN（`:81`）；写入值由 `source_snr:61`（`isfinite && >0`）保证有限。故 `count(isfinite(snr_f)) ≡ count(rok) ≡ used_snr.size() = out.n_used`。**无任何可达失败分支**。它被包装成"内部一致性守卫"，实际是零信息断言 —— **阻断级恒真门**。
- 🔴 **恒红门（不可达分支）**：`:203-208` `if (snr_frame_depth_f64(&ref_res, ...) != 0)`。`snr_science.cpp:256` 的**唯一**非零返回是 `if (!reference) return 3;`，而 `&ref_res` 是栈对象地址**永不为 null** ⇒ 对栈实参**恒返回 0** —— **阻断级**。
- 🔴 **静默降级 → 消费者拿到 valid 但深度为 0/NaN**：`snr_frame_depth_f64`（`snr_science.cpp:252-267`）**不校验 `reference->status`**。传入退化源（σ_F=0）时返回 0、`*out_flux5_adu=0`、`*out_m5_mag=NaN`，而本文件 `:210` 仍 `out.valid = true`，`reason` 为空 ⇒ **无法诊断** —— **阻断级**。
- 🔴 **静默降级（无推导默认值）**：`:85-87` `snr_calib_zero_point_standard_error` 对非法输入返回 `0.0`，且在 `:89` 的全部有效性检查**之前**写入输出 ⇒ 无残差时 `sigma_location_se_dex=0` 与「真实为 0」不可区分，无 reason（本仓 `wrapper_phase1/README.md:49` 称另有 `sigma_location_se_status`，但**本 TU 不产出该字段**）—— **须修**。
- ⚠️ **并行区**（降级为须修，理由见 §7 否决 #2）：`:112-114` `#ifdef _OPENMP / #pragma omp parallel for schedule(dynamic,1) / #endif`。`:100-107` 给出的确定性论证（无共享可变态、按下标定长写回、串行 compact 复刻）**成立**，我复核无数据竞争。但 `#ifdef` 使行为随构建配置静默漂移（无 OpenMP 时串行，**无日志、无 provenance**），与同层 `snr_estimator.cpp` 的 10 处无守卫 pragma 形成两套口径 —— **须修**。
- ⚠️ 字段语义重复：`:145-147` `median_snr = snr_phot = median_source_snr = med`，`:139` 亦把 `med_fwhm`/`ref_sigma` 分别取中位数再组合（同 `snr_estimator.cpp:114-115` 的构造问题）—— **建议**。
- ✅ 负面：`reference_flux_adu` 缺失时 `:175-181` **fail-closed**（删除了逐帧中位数回退，注释 `:165-174` 给出了配对性定理推导）——这是本层做得最好的一处；`:89-96 :134-137 :141-144` 各退化路径均给出明确 `reason`。
- ✅ 负面：`ref_row`（`:188`）无未初始化读——`snr_frame_science.h:38 :43` 有 NSDMI。

### 3.10 `include/acsd/noise/types.h`（204 行）— 读完
- 🔴 **死词表**：`NOISE_O_KEY_SIGMA_BG/VARIANCE_BG/IVAR_BG`（`:149-151`）、`NOISE_O_KEY_FLOOR_FALLBACK`（`:165`）、`NOISE_O_KEY_WORKERS`（`:166`）、`NOISE_O_KEY_DIAG_VARIANCE`（`:174`）、`NOISE_O_KEY_DIAG_GAIN_VAR`（`:176`）全仓**零引用**（grep 仅命中定义行）⇒ B1 的物证；其余同型（`NOISE_O_KEY_N_QUALIFIED/N_REJECTED` 亦仅定义即用字面量）。**须修**。
- ⚠️ `:121` 注释称 `:122-133` 是「fill_noise_field op: 模型 round-trip 字段（**estimate_noise_model 输出**）」——与 B1 直接冲突：这三类字段中三个正是 estimate 从不输出的 —— **须修**。
- ⚠️ `:138-141` 明文承认 rc=1（完全退化）属「ACS_OK 语义」——把退化写进契约，等于**授权**调用方忽略它 —— **须修**。
- ⚠️ `:181-198` `noise_ecode` 定义了 12 个专属码，但 `module_entry.cpp` 实际只用 `100/101/102/103/110/111/112/120/121/130`，且大量失败落在 `ACS_DIAG_ECODE_NONE`（见 3.1）—— **建议**。
- ✅ 正面：ABI 版本化纪律（`:22-31` struct_size@0 + abi_version@4 + 失配 fail-closed）完备，注释 `:19-20` 明写「无头部一律拒绝，静默兼容会把旧语义当新语义消费」——判断正确。

### 3.11 `tests/p1noise/check_snr_caliber.py`（166 行）— 读完
- 🔴 **恒红门 + 门内 W3 永不执行**：`:27` `DOC = REPO/"docs/plugins"/"algorithms_phase1"/"07_noise_snr.md"`。实测 `docs/plugins/` **不存在**（整目录已随文档迁移删除）。`:81-82`
  ```python
  if not DOC.is_file():
      return [("W2", "文档不存在: %s" % DOC)]
  ```
  **提前 return ⇒ W3（`module_adapters.cpp` 实现事实检查，`:90-98`）永不执行**。恒红门把真实缺陷藏在红灯里：无法从红灯判断 W3 是否真的通过。
- 🔴 **默认调用下 W1（唯一的产品科学判据）完全不跑**：`:147` `for p in argv:` —— 无参数时循环体不执行，`check_product()` **一次都不会被调用**。而 `:161` 照样打印
  ```
  SNR_CALIBER_GATE_PASS: 文档约束 + 实现事实 + 0 份产品口径自洽
  ```
  ⇒ **验证了 0 份真实产品仍报 PASS**。文件头 `:19` 恰恰把「无参数则只跑静态面」写成用法 —— 门在 CI 中的默认形态就是空转绿灯 —— **阻断级 fail-open**。
- ⚠️ **W1 对真实产品不可证伪（自洽式断言的 python 形态）**：`:51-65` 的四条逐帧一致性（`cal==ABS ⟹ inc==true ∧ gain>0`；`cal==UB ⟹ inc==false ∧ gain is None ∧ reason 非空`）与 `:68-74` 的顶层聚合/计数，只要产品由 `module_adapters.cpp` 的同一组三元表达式生成就**恒成立** ⇒ 只能抓手工损坏的 JSON。`:102-140` 的 `self_test` 用**内存合成 dict** 验证同一批规则的 5 条负例，质量尚可，但**不接触任何真实产物** —— **须修**。
- ⚠️ `:26` `REPO = Path(__file__).resolve().parents[5]` 硬编码层级，无 `find_repo` 回退（对照同目录 `check_saturation_wiring.py:22-31` **有**回退，且注释 `:24-26` 明写同型事故已被咬过一次）—— **须修**。
- ⚠️ `:27` 门洞：整个 W2 依赖单个文件路径字符串，而**该路径已死**（§3.3/§3.5 的口径正本在现行树同样不存在）—— **阻断级证据链断裂**。

### 3.12 `memory.md`（146 行）— 读完
- 🔴 **记录的事实与实测全面冲突**（本片"悬空引用"最集中处）：
  - `:36` 称 `noise_model.cpp` **475 行**「2026-09-16 复测」；`:37` 称 `snr_estimator.h` **526 行**。实测 **1482 行 / 647 行**。文档自称「行号以实测为准」「本 4 文件一律用实测行号」，而它自己记的行数就是错的 —— **须修**。
  - `:85-90` 详细描述 `wrapper_phase1/noise_model.{h,cpp}`（39+67 行）"= NoiseModel::estimate"——**该二文件已不存在**（同目录 `wrapper_phase1/README.md:6-8` 明写已删并订正）。**同一片内两份文档互相矛盾** —— **须修**。
  - `:87-88` 称 `acsd_phase1_noise` 在 `CMakeLists.txt:521-524`「主程序链接 :513」；实测在 **:1128-1131**（偏移约 600 行）。全部 CMake 行锚失效 —— **须修**。
  - `:39-40` 称「根 `CMakeLists.txt:236` 的 `add_subdirectory` 被注释」——实测相关注释放 **:386/:395/:426** —— **须修**。
  - `:66-71` 断言「根 CMakeLists.txt 无 snr_estimator 目标（grep rc=1）…**未编入根 CMake 主构建**」⇒ 据此推导 `src/module_entry.cpp` 无消费者，与 B2 一致（**结论对，论据行号全错**）。
  - `:62-65` 称生产经 `dll_loader.cpp:41` 装载 `snr_estimator.dll` 取函数指针，且**只列 5 个符号**（不含 `snr_noise_scale_law`/`snr_noise_gain_variance`）——与 `module.yaml:84-89` 撤回这两个符号的"不可达"判断自洽，但也与 `module_entry.cpp:1477/:1522` 的活调用冲突（见 3.15）。
- ⚠️ `:122-124` 记录了一处 SCI 数字冲突（`min_patch_samples` 默认 64 vs SCI §4 "5"）并按"以代码为准"登记——这是本片**唯一**处理得当的悬空项，可作正面范例。

### 3.13 `tests/p1noise/check_saturation_wiring.py`（135 行）— 读完
- 🔴 **恒红门（W1 必红）**：`:61-65`
  ```python
  ref = sat.get("source_ref") or {}
  ref_line = ref.get("line")
  if not ref_path or not ref_line:
      fail("W1-REF", "noise.saturation_level 缺 source_ref")
  ```
  我实测 `eng/packaging/config/defaults.json` 中 `noise.saturation_level.source_ref` 的键集为 **`['id','path','quote','sha256','value_text']`，无 `line`** ⇒ `ref_line = None` ⇒ **W1 必红**。schema 漂移（quote+sha256 取代 line），门未跟进 —— **阻断级**（恒红门遮蔽 W2/W3/W4 的真实状态）。
- ⚠️ **静默降级（跳过被检文件）**：`:120-122`
  ```python
  p = REPO / rel
  if not p.exists():
      continue
  ```
  W4 声称扫描 `snr_estimator.h` 与 `README.md`「不得再把 saturation_level=0 描述为『禁用』」。任一文件被删/改名 ⇒ **静默跳过、门照报 PASS** —— 典型 fail-open —— **须修**。
- ⚠️ **门与被测对象错位**：`:91-92` W3 检的是 `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp`（另一层），W4 才回到本层；`:93-96` 的 token 表**未含 `NOISE_SATURATION_SOURCE`** ⇒ 该 provenance 字段无任何门覆盖 —— **建议**。
- ✅ 正面：`:22-31` `find_repo` 有回退且 `:24-26` 注释**明写同型事故已发生过一次**（config/ → eng/packaging/config/）——这条经验本该被同目录的 `check_snr_caliber.py` 继承，却没有。
- ✅ 负面：`:74 :86 :112 :126` 四处 `except Exception` 均转成 `fail(...)` ⇒ **fail-closed**，无空捕获。

### 3.14 `tests/p1noise/p1noise_tests_perf.cpp`（125 行）— 读完
- 🔴 **恒真门（教科书级）**：`:86-87`
  ```c
  const double ratio = (r1.seconds > r2.seconds) ? r1.seconds / r2.seconds
                                                     : r2.seconds / r1.seconds;
  ```
  `ratio = max/min` ⇒ **按构造恒 ≥ 1.0**。`:92` `P1NOISE_CHECK(cs, ratio >= 0.25, "perf_trend_025x");` ⇒ **永不可能失败**。而 `:13` 的文件头把它宣传为「CI 森严哨兵：无异常加速趋势」。更糟：其**意图**（检出异常加速，即计算被跳过/缓存）用对称比值**原理上无法检出**——4× 加速给出 ratio=4.0，`ratio<=4.0`（`:91`）**放行**。正确写法应为方向量 `r2.seconds >= 0.25 * r1.seconds` —— **阻断级恒真门**。
- ⚠️ **筛掉真信号**：`:106` `const double t_full = std::min(r1.seconds, r2.seconds);` 取**两次的较小者**作分母，把 `:109` 的上界门**收紧**——方向上是保守的，但会放大共享 CI 机器上的假红；且 `:109` 本身是**跨帧速度门**，与文件头 `:9-10`「不在本测试面预设**绝对速度门**（防硬件漂移 CI 假红）」的自述相抵触（相对门同样吃硬件漂移）—— **须修**。
- ⚠️ **覆盖虚标**：`:14-15` 声称 `memory_bound` 覆盖「输出面 npix 固定 + O(h·w) 掩膜 + O(64) 控制点」；实测 `:59` 的 npix **只打印不断言**，O(h·w) 掩膜内存**从未测量**，真正断言的只有 `:95` 与 `:96-97` —— **须修**。
- ⚠️ **硬编码**：`:91 :92 :109` 的 4.0× / 0.25× / 0.5× 与 `:96` 的 64 均无推导出处 —— **须修**。
- ⚠️ `:70` 与 `:91` 共用名 `"perf_parity_4x"`、`:95` 与 `:97` 共用 `"memory_bound_64ctrl"`，各 2 条不同断言共用一名 ⇒ 故障注入时后续同名门全部连带误红，**失败归因被摧毁** —— **建议**。
- ✅ 正面：`:61-62` 有**不计时**的预热跑（避免首跑页分配污染），这是同批唯一做对的计时卫生；`:73-84` 的 bitwise 对照覆盖了控制点数组 `memcmp`，非只比标量。

### 3.15 `module.yaml`（119 行）— 读完
- 🔴 **退役声明被证伪（与负责人已实测到的同类）**：`:77-89` 把 `snr_noise_scale_law` 与 `snr_noise_gain_variance` 从 `source_symbols` 撤下，理由（`:78-79`）是「在三个生产命令的名字级调用图上**不可达**（判据 = `eng/ci/check_prod_wiring.py` W1），且不在本模块 exports 面内」。但**同片内** `src/module_entry.cpp:1477` `snr_noise_scale_law(c->alpha, ...)` 与 `:1522` `snr_noise_gain_variance(c->signal_in, ...)` 是**活调用点**（`noise_diagnostic` op 的实现）。⇒ **声明"零可达消费者"被同片代码证伪**，且所依据的门（W1）显然不覆盖此路径 —— **阻断级**。
- 🔴 **构建与线程声明失真**：`:19-20`「未编入根 CMake 主构建」对 `snr_estimator.cpp` 成立、对整层不成立；**`:22`「现状实现为单线程顺序（`noise_model_impl` 无 omp pragma）」对整层为假**（`snr_frame_science.cpp:113`、`snr_estimator.cpp` 10 处 pragma，见 B2）—— **阻断级**。
- ⚠️ `:41` `entrypoint: MISSING` 与 `:107-119`「禁止声明 IMPLEMENTED（迁移落码未发生）」——而迁移落码文件就在目录内；`:118` 明写「不得反向作为冻结依据」，但 `:114-117` 已把 `module_adapters.cpp:489-503` 的占位 ID 与 port 编目**原样抄进本 manifest 的 upstream/downstream 叙述**，自我违反了这条禁令 —— **须修**。
- ⚠️ `:48 :50 :52 :54 :62-67` 的合同号（SCI-NOISE-001..015 / ALG-NOISE-001..003 / DATA-P1-NOISE / API-P1-006）与 `:56-60` 的 `doc_links`（`docs/science/NOISE_MODEL.md`、`algorithms/NOISE_ESTIMATION.md#§13`、`DATA_SEMANTICS.md#§13`、`PUBLIC_API.md#API-NOISE-001`）——实测**四个路径全部存在**（✅ 正面）。
- ⚠️ **（本条为我自己的初判错误，已据实更正）** 我初判「`:107-119` notes 所列 `DISP-NOISE-001..009` 九条至今全部仍是登记不改码」。**实测证伪其中至少两条**：
  - `DISP-NOISE-001`（`:22-23` 宣称「`g_model_floor` 进程级**无锁** unordered_map」）→ **已整改**：`cpp/src/noise_model.cpp:25` `#include <mutex>`、`:45` `static std::mutex g_model_registry_mutex;`、`:49 :56 :63 :69 :81 :87` 六处 `std::lock_guard`。
  - `DISP-NOISE-002`（`:109` 「build/fill floor 语义不一致」）→ **已整改**：`noise_model.cpp` 中 `SNR_FLOOR_UNBOUND` 出现 **10** 次，`:1354` 注释「不再静默回退 1e-12」。
  ⇒ **结论改为**：本 manifest 的 DISP 登记表**既非"全未整改"也非"已同步"**，而是**部分整改而登记未回填**——这比"全未整改"更危险，因为它让读者无法判断任一条的真实状态。**须修**（逐条回填）。
- ⚠️ **manifest 自身的规范依据与落位依据双断链**：`:9` 引「`11_MODULE_SOURCE_TEST_STANDARD.md` §4」、`:12` 引「`MODULE_MIGRATION_MATRIX.csv` P1-NOISE 行」、`:79` 引「`eng/ci/check_prod_wiring.py` W1」。我实测**三者全部不存在**（`eng/ci` 整个目录已从活动树删除；另两个文件全仓零命中）。⇒ `module.yaml` 既无格式依据、也无迁移落位依据、也无"可达性"判据依据 —— **阻断级**（与 B16 同源：正是这条被删的 W1 门让 `:77-89` 的错误裁剪蒙混过关）。
- ⚠️ **导出清单漏 6 个**：`exports`（`:69-76`）只列 7 个，而 `snr_estimator.h` 现声明 **13** 个 `snr_noise_*`（另有 `_abi_stamp_config:235`、`_abi_stamp_model:236`、`_abi_check_config:237`、`_abi_check_model:238`、`_bind_variance_floor:267`、`_floor_clamp_count:269`）；`source_symbols`（`:84-89`）只列 5 个 —— **须修**。
- ✅ 正面：`legacy_paths`（`:105`）与 `source_symbols`/`exports` 的区分（`:69-89`）结构清晰；`:22-25` 对 `g_model_floor` 无锁 map 的登记诚实。

### 3.16 `wrapper_phase1/snr_frame_science.h`（96 行）— 读完
- 🔴 **悬空引用**：`:60` `// sigma_sky 语义 (SCI-B D1; 07_noise_snr.md 4.2a)` —— 该文件在现行树不存在（同 3.3）—— **阻断级证据链断裂**。
- ⚠️ `:82` `int reference_index = -1;` —— `compute_snr_frame_science` **从不写它**（恒 -1），且 `:82` 注释「(-1 = 合成中位轮廓)」使该字段实际无信息量 —— **建议**。
- ⚠️ `:51` `zero_point_mag` 以 `0.0` 表「未知」；`:53` `n_sky` 注释「`<=0 -> n_pix`」——**把无推导的兜底写进了公开契约**（见 3.7）—— **须修**。
- ⚠️ `:74-76` 三个字段 `median_snr`/`snr_phot`/`median_source_snr` 同义别名（`:75` 自认「字段名冻结, 语义已重定义」）—— **建议**。
- ✅ 正面：`:25-26` 明确「本路径**不产出**整帧 SNR 标量」，`:89-91` 明确 fail-closed 不回填 1.0 —— 契约表述正确。

### 3.17 `wrapper_phase1/README.md`（79 行）— 读完
- 🔴 **同片内文档互斥**：`README.md:6-8` 明写旧 `wrapper_phase1/noise_model.{h,cpp}` 已不存在；`memory.md:85-90` 仍在详细描述它 —— **须修**（memory 侧）。
- ✅ **正面（本片唯一一条主动修好的悬空引用）**：`:6-11` 是一段**带订正记录**的悬空引用修复（原文指向已删文件，2026-09-20 订正为现行文件，并说明依据 `CMakeLists.txt:618-640` NOISE-MODEL-CANON-001）。这证明该类问题在本仓被认真对待过 —— 但 §3.11/§3.12 显示同一片另有多处未修。
- ⚠️ **行锚陈旧**：`:9` 引 `CMakeLists.txt:618-640`；根 `CMakeLists.txt` 现为 3651 行级别的文件，同类声明落在 `:1128-1144` 区间 ⇒ **建议**复核。
- ⚠️ `:12` 引 `eng/tests/unit/p1_noise_test.cpp`（✅ 存在）、`:36` 引 `eng/tests/unit/p1snr/p1snr_linux_test.cpp`（✅ 存在）、`:79` 引 `eng/tests/unit/p1snr/p1snr_frame_parity_test.cpp`（✅ 存在）—— **三个测试引用全部真实**（本片少数干净的悬空面）。
- ⚠️ `:9-11` 自称「旧 `noise_model.cpp` 是 `cpp/src/noise_model.cpp` 的退化子集，已退役」——但 `memory.md:85-90` 说它「= `acsd::phase1::NoiseModel::estimate`…静态库 `acsd_phase1_noise`」仍在用 —— 同上互斥。

### 3.18 `cpp/build.ps1`（58 行）— 读完
- 🔴 **构建面缺失**：`:35-39` 只编译 `src/snr_estimator.cpp` **一个 TU**，产出 `snr_estimator.dll`。**不含** `src/snr_science.cpp`、不含 `src/noise_model.cpp`、不含 `src/module_entry.cpp`、不含 `wrapper_phase1/snr_frame_science.cpp`。⇒ 该脚本产出的 DLL 缺本层绝大多数科学面（缺 `snr_science.cpp` 则连 `snr_source_snr_f64` 都没有，而 `snr_estimator.cpp` 正是调它）—— **须修**。
- ⚠️ `:9-10` 硬编码 `C:\msys64\mingw64\bin\g++.exe` 与 `C:\msys64\mingw64\bin`，无环境探测/无覆盖开关 —— **建议**。
- ⚠️ `:39` 同时在 `CXXFLAGS`（`:35`）与链接命令行各写一次 `-fopenmp`（重复旗标，仓内 `eng/cmake/cfitsio_platform.cmake:41-46` 记录过同型事故）—— **建议**。
- ⚠️ `:48` 的成功判据含 `Test-Path`，但 `:56` 失败时 `exit $exitCode`；若 g++ 缺失 `:15` `exit 1` —— 退出码路径本身正确（✅ 正面），但**无 `.o` 中间产物、无并行、无测试规则**。
- ⚠️ `:29` 输出名 `snr_estimator.dll` 与 `module.yaml:17-18` 声明的 `acsd_p1_noise.dll` **不同名**，且 `module.yaml:18` 明写「现状构建 = `cpp/Makefile:5,12`（→ snr_estimator.dll）+ `cpp/build.ps1:29`」——口径一致但**产出名与合同名不符**，第三方无法按 `dll_target` 找到产物 —— **建议**。

### 3.19 `tests/p1noise/p1noise_abi_layout_lock.json`（41 行）— 读完
- ✅ **正面（关键结论）**：这是**输入型（INPUT）锁**而非再生产物——41 行全是固定偏移/尺寸期望（`config.sizeof:120`、`model.sizeof:200`、`abi_mismatch_rc:-9`、`config_fields.mask_k_sigma:88`、`model_adapt_fields.hull_nonpositive_frac:192` 等），文件内**无生成逻辑**。与 `snr_estimator.h:22-31` 的 `struct_size@0 / abi_version@4` 版本化纪律、`SNR_NOISE_CONFIG_ABI_VERSION=1`(`:23`) 与 `SNR_NOISE_MODEL_ABI_VERSION=2`(`:28`) 相符（lock 中 `config.abi_version:1` / `model.abi_version:2` ✅）。⇒ **本片未发现"自愈锚/写自己的 oracle"类缺陷**（子代理亦独立给出同一否定结论）。
- ⚠️ **UNVERIFIED**：锁中的偏移是否与 `snr_estimator.h:129-202` 的**实际**结构体布局逐位相符，需运行 `p1noise_abi_layout_check.py`（兄弟片 002）核对——本片纪律禁跑，故只做**内部一致性**核对：`model.sizeof=200`、`abi_version_off=4`、`struct_size_off=0` 与头 `:29-31` 宏定义一致。
- ⚠️ **锁的覆盖面偏窄**：只锁 config/model 两个结构体，**未锁** `SnrSourceParams`、`SnrSourceResult`、`SnrControlPoint(V3)`、`SnrControlPointF64(V3)`、`SnrModel/V2/V3`——而 `snr_estimator.h:487 :499 :517 :529` 的 4 条 `static_assert` 只覆盖控制点 —— **建议**。

### 3.20 `src/module_exports.map`（13 行）— 读完
- 🔴 **悬空自引用**：`:1` 文件头写 `# lib/snr_estimator/src/module_exports.map`。实测 `lib/snr_estimator/` **不存在**，实际路径是 `lib/algorithms/noise_snr/src/module_exports.map` —— **须修**。
- ⚠️ **悬空引用**：`:7` 引 `tests/unit/p1_noise/adapter_test.cpp` 作为"nm -D 实证"的出处。实测该路径**不存在**；真实文件是 `eng/tests/unit/p1_noise/adapter_test.cpp`（前缀差 `eng/`）—— 近失但仍不可达 —— **须修**。
- ✅ 正面：`:8-13` 白名单语法正确（`global: acsd_module_query_v1; local: *;`），且与 `module_entry.cpp:1619-1632` 唯一 `ACSD_EXPORT` 一致；被 `lib/algorithms/noise_snr/CMakeLists.txt:90-92` 以 `--version-script` 真正消费（该子图当前虽不在根图）。

### 3.21 `src/acsd_p1_noise.def`（6 行）— 读完
- 🔴 **技术性错误 + 悬空自引用**：`:1` 文件头写 `# lib/snr_estimator/src/acsd_p1_noise.def`（路径错，同 3.20）；`:2-3` 称「legacy snr_noise_* 七符号（SNR_API dllexport 携带）**经本 DEF 过滤降 local**」——MSVC 下 `__declspec(dllexport)`（`snr_estimator.h:7`）**不能被 DEF 降级为 local**，DEF 只能增加导出项 ⇒ 该"导出面净化"承诺在 Windows 上不成立 —— **须修**。
- ⚠️ `:4` `LIBRARY acsd_p1_noise`，但根构建里的同名 target 是 `acsd_phase1_noise`（STATIC，`:1128`），且 `acsd_p1_noise`（SHARED）所在子图不在根图 ⇒ 该 DEF **当前无消费方** —— 与 B2 同源。
- ✅ 正面：`:5-6` `EXPORTS acsd_module_query_v1` 与 `.map` 语义对齐；被 `CMakeLists.txt:86-87` 以 `/DEF:` 消费（Windows 条件分支）。

---

## 4. 发现清单

### 🔴 阻断（20 条）

| # | 类 | 位置 | 一句话 |
|---|---|---|---|
| **B1** | 自洽式/静默降级 | `src/module_entry.cpp:1285-1287` vs `:1115-1119`；`types.h:149-151` | fill 读的 3 个 round-trip 键 estimate 从不写 ⇒ 无空间场模型整幅噪声场被静默清零却返回成功；「bitwise 一致」宣称为假 |
| **B2** | 悬空/失真 | `CMakeLists.txt:395`；`module.yaml:19-22` | `module_entry.cpp`(1635 行/本片 24.9%) 无可达构建；`module.yaml:22`「单线程顺序」对整层为假 |
| **B3** | 覆盖虚标 | `cpp/test/noise_model_science_test.cpp:20` | 603 行科学矩阵既未被 CI 引用、又按其自述编译行无法编译 ⇒ SNR-001..014 一次都没跑过 |
| **B18** | 恒红门 | `cpp/test/noise_model_science_test.cpp:389`（我已数值复算） | 断言 `snr_extract_model` 输出 `== (A−B)/residual_scale`，生产已改 Horne SNR_F；实测 33.934825 vs 33.333333（差 0.601 ≫ 容差 1e-4）⇒ **恒红且从未被点亮**；与在构建图内的 `p1snr_science_test.cpp:430-432` 互斥 |
| **B19** | 退役声明证伪 | `module.yaml:10-11`、`memory.md:140` vs `orchestrator.cpp:4281,:4330,:4490` | `LEGACY_SNR_SCIENCE_CONSUMER=0` 为假：`snr_extract_model_v3` 是 stage6 SNR **必需**符号，取不到即硬失败 |
| **B20** | 悬空引用（制度性） | `module.yaml:9,:12,:79,:82` | manifest 的格式依据、迁移落位依据、可达性判据依据三者**同时断链**（`eng/ci` 整目录已删）⇒ B16/B19 得以长期存活的成因 |
| **B4** | 悬空引用（法源） | `snr_estimator.h:311`、`snr_frame_science.h:60`、`snr_science.cpp:175`、`p1snr_science_test.cpp:299` | 被称为「口径正本」的 `docs/detail/algorithms_phase1/07_noise_snr.md` **整目录不存在** ⇒ SCI-B D1 读噪双计规则无文件承载 |
| **B5** | 悬空引用（oracle） | `snr_frame_science.cpp:10-12`、`p1snr_science_test.cpp:7,:174-178` | 声称的独立 NumPy oracle 不存在，5 个锚值全仓零命中 ⇒ 唯一非自洽证据不可复核 |
| **B6** | 恒真门 | `p1noise_tests_perf.cpp:86-92` | `ratio=max/min` ⇒ `ratio>=0.25` 永不可能失败；且方向量无法检出其声称的「异常加速」 |
| **B7** | 恒真门 | `snr_frame_science.cpp:157-161` | 「internal catalogue count mismatch」分支可证不可达 |
| **B8** | 恒真门 | `p1noise_saturation_test.cpp:219-220`（哨兵 `-1.0`，`:136`） | 过滤生效（patch 被剔除）时 G2d 必然通过 ⇒ 断言被哨兵洗白 |
| **B21** | 恒真门 | `p1noise_saturation_test.cpp:215`（`:107,:139-140`） | G2b 在控制点被**全剔**时真空为真（`n_polluted==0 ∧ ctrl_var_max=0≤2500`）；全文件无一处断言 `n_control_points>0` ⇒ 「过滤过头」这条缺陷路径是绿的 |
| **B9** | 自洽式断言 | `p1snr_science_test.cpp:449-451` | 帧深 oracle = 生产调用式的逐字转写 ⇒ 网格/归一化缺陷两侧同步移动 |
| **B10** | 静默降级 | `p1snr_science_test.cpp:473-476` | 两次 `extract_model_v3` 返回码被丢弃 ⇒ 双双失败时零初始化结构使确定性门**判绿** |
| **B22** | 静默降级 | `p1noise_saturation_test.cpp:127-128,:205-210` | `rc` 被捕获、被打印，**28 条 `check` 零条断言 rc** ⇒ 模型退化/坏参时靠「恰好别的门兜底」而非自身检查 |
| **B11** | 恒红门 | `check_snr_caliber.py:27,:81-82,:147,:161` | 文档锚路径已死 ⇒ W3 永不执行、W1 默认不跑，却仍打印 `PASS ... 0 份产品` |
| **B12** | 恒红门 | `check_saturation_wiring.py:61-65` + `defaults.json` | `source_ref` 无 `line` 键 ⇒ W1 必红，W2/W3/W4 的真实状态被红灯遮蔽 |
| **B13** | 静默降级 | `snr_science.cpp:226` | `n_sky<=0 ⇒ n_pix`，天空方差因子恒 2.0（真值典型 1.125）⇒ 高估 1.78×，无错误码/provenance |
| **B14** | 静默降级 | `p1noise_saturation_test.cpp:176-197,:230-231` | 无饱和元数据 ⇒ 过滤关闭 ⇒ 方差场 4×10⁴ 偏置，而测试把它固化为「正确」 |
| **B15** | 文档 vs 代码 | `NOISE_MODEL.md §11` vs `p1noise_saturation_test.cpp:234-236` | 冻结 oracle 要求 ivar ≥3×，在运门断言两臂一致（反面）；行内注释不构成 waiver |
| **B16** | 退役声明证伪 | `module.yaml:77-89` vs `module_entry.cpp:1477,:1522` | 声称「生产调用图不可达」的两个符号在同片有活调用点 |
| **B17** | 异常穿 ABI | `snr_science.cpp:165,:185`（`extern "C"` 于 `:107`） | `profile_half_px` 绕过 256 上界 ⇒ `reserve` 的 int 溢出可抛异常穿出 C 边界 = UB |

### ⚠️ 须修（21 条，择要）

`module_entry.cpp:115`（`cap==0` 且 `data!=NULL` 被吞成成功且已写 `size=n`）· `module_entry.cpp:476 等 14 处`（`ACS_DIAG_ECODE_NONE` 使多类失败不可区分，含一条安全性拒绝门）· `module_entry.cpp:923-934`（取消语义二分）· `module_entry.cpp:960-966`（rc=1 完全退化映射 ACS_OK）· `module_entry.cpp:1442` vs `:1385`（floor_fallback 两观察面相反）· `snr_estimator.cpp:292-293,:460-461`（日志说谎）· `snr_estimator.cpp:123` vs `snr_science.cpp:257`（F₅ 两份实现）· `snr_estimator.cpp:114-115`（median·f(median)≠median·f(median)）· `snr_estimator.cpp:105-107` vs `:860-867`（两套星筛选）· `snr_estimator.cpp:204,:628,:632`（丢弃原因误标）· `snr_estimator.cpp:518-528`（SIP 越界空分支静默降级）· `snr_estimator.cpp:653-657,:767-775,:882-890`（失败时输出半写）· `snr_estimator.cpp:668`（异常穿 ABI）· `snr_estimator.h:6-10`（POSIX 下 SNR_API 空宏 + version-script ⇒ 符号不可见，与 `memory.md:62-65` 装载叙述冲突）· `information_weight.h:58` vs `.cpp`（拒绝串双向漂移，`dimension_mismatch` 死契约）· `information_weight.h:30`（`kQwRelTol` 挪用）· `snr_science.cpp:158-160,:196,:212`（成功码 + 退化 status，与相邻函数语义相反）· `snr_science.cpp:177`（枚举零校验）· `snr_science.cpp:56`（`kTrimMeanToSigma` 死副本）· `snr_science.cpp:279`（`1.253` 应现场算 `√(π/2)=1.2533141373155003`）· `snr_science.cpp:99-101`（12/30/256 无出处，上界静默截断）· `snr_frame_science.cpp:85-87`（`sigma_location_se` 无残差时静默 0 且早于校验）· `snr_frame_science.cpp:203-208`（恒红门）· `snr_estimator.cpp` 10 处无 `_OPENMP` 守卫 · `snr_frame_science.cpp:112-114` 的 `#ifdef` 静默漂移 · `check_saturation_wiring.py:120-122`（跳过不存在的文件仍 PASS）· `memory.md:36-37,:85-90,:87-88,:39-40`（行数/行锚/已删文件全面失真）· `module_exports.map:1,:7` 与 `acsd_p1_noise.def:1,:2-3`（路径错 + DEF 技术性错误）· `cpp/build.ps1:35-39`（只编 1 个 TU）· `p1noise_tests_perf.cpp:106,:109,:14-15`（筛小分母 + 覆盖虚标）· `p1noise_saturation_test.cpp:6`（覆盖虚标）

### 💡 建议（12 条）

`snr_science.cpp:224` 灾难性抵消 · `snr_science.cpp:69` 注释漏 256 钳位 · `snr_science.cpp:221` 的 1.5×FWHM 出处 · `types.h:138-141` 把 rc=1 写进 ACS_OK 契约 · `types.h` 12 个专属码多数未用 · `snr_frame_science.h:82` 死字段 · `snr_frame_science.h:74-76` 三别名 · `snr_estimator.h:583-585` 头内自相矛盾 + 注释与实现不符 · `snr_estimator.h:358-378` 返回码契约缺口 · `p1noise_abi_layout_lock.json` 覆盖面偏窄 · `p1noise_tests_perf.cpp:70,:91,:95,:97` 同名门复用 · `wrapper_phase1/README.md:9` 行锚陈旧

---

## 5. 我主动构造的反例

### CE1（推翻现有结论）— fill 通道整幅清零
- **构造**：`enable_spatial_field=0`（或合格 patch<4）⇒ estimate 输出 manifest（`module_entry.cpp:1115-1119`）**不含** `sigma_bg_global`/`variance_bg_global`/`ivar_bg_global` ⇒ 交给 fill ⇒ `:1285-1287` 三次 `json_get_f64` 全部 miss，返回 `0.0` 且 `f=0`（**不报错、不设 status**）⇒ 影子模型三全局标量恒 0 ⇒ `noise_model.cpp:1399-1406` else 分支逐像素写 `variance=0 ∧ ivar=0` ⇒ 按 `:1367-1375` 三态表即「全帧不可用」；`rc=0`、`ACS_OK`、manifest 写 `"floor_fallback":0`、`"variance_floor_status":"bound"`。
- **期望推翻**：`module_entry.cpp:1220` 的「direct 通道与影子通道…**仍 bitwise 一致**」（另见 `:14-16`、`:1037-1038`）。
- **是否推翻**：**推翻成功。** 独立佐证：`NOISE_O_KEY_SIGMA_BG/VARIANCE_BG/IVAR_BG`（`types.h:149-151`）全仓零引用。

### CE2 — 恒真门 `perf_trend_025x`
- **构造**：`ratio = (r1>r2)? r1/r2 : r2/r1 = max/min ≥ 1`（`p1noise_tests_perf.cpp:86-87`）⇒ `:92` 的 `ratio >= 0.25` 满足域为 `[1,∞)`。
- **期望推翻**：文件头 `:13`「CI 森严哨兵：无异常加速趋势」。
- **是否推翻**：**推翻成功。** 且证明该门**原理上**无法检出异常加速（4× 加速 ⇒ ratio=4.0，被 `:91` 的 `<=4.0` 放行）。

### CE3 — 恒真门「internal catalogue count mismatch」
- **构造**：`out.snr_f[i]` 唯一写入点 `:128`，受 `:127 !rok[i]` 守卫；`rok[i]` 唯一置位点 `:120`；初值 NaN（`:81`）；写入值由 `source_snr:61`（`isfinite && >0`）保证有限 ⇒ `count(isfinite(snr_f)) ≡ count(rok) ≡ used_snr.size() = out.n_used`。
- **期望推翻**：`:157-161` 是一条"内部一致性守卫"。
- **是否推翻**：**推翻成功**（分支不可达）。

### CE4 — 静默降级 `n_sky = n_pix` 无推导
- **构造**：`r_out = 3r` 天空环 ⇒ `n_sky_true = π(9r²−r²) = 8πr² = 8·n_pix` ⇒ 真因子 `1 + 1/8 = 1.125`；`snr_science.cpp:226` 取 `n_sky=n_pix` ⇒ 因子恒 `2.0` ⇒ 天空项高估 `2.0/1.125 = 1.78×`，σ 高估 `√1.78 = 1.33×`。
- **期望推翻**：该兜底为「保守方向」（SNR 低报）。
- **是否推翻**：**未推翻保守性**，但证实它**无科学推导、无错误码、调用方不可分辨** ⇒ 仍判阻断（AGENTS §6 明文「可由输入几何导出的改为现场计算」）。

### CE5 — `1.253` 可现场导出
- **构造**：高斯样本中位数渐近方差 `πσ²/(2N)` ⇒ 标准误常数 `√(π/2)`。我独立算得 `√(π/2) = 1.2533141373155003`；`snr_science.cpp:279` 用 `1.253` ⇒ 相对误差 `−2.51e-4`，**系统性低估**零点标准误。
- **期望推翻**：该字面量是冻结常数。
- **是否推翻**：**推翻成功**（可导出 ⇒ 应现场计算）。

### CE6 — `strbuf_write` 的 `cap==0` fail-open
- **构造**：宿主传 `data = <非空指针>`、`cap = 0` ⇒ `module_entry.cpp:115` 的 `if (!out->data || out->cap == 0) return ACS_OK;` **返回成功**，而 `:114` 已把 `out->size = n` 写上；宿主据 `size` 读 `n` 字节即越界读。
- **期望推翻**：`:106-107` 的注释「cap=0 且 data=NULL 只问尺寸」——即 cap=0 配非空 data 属非法输入。
- **是否推翻**：**推翻成功**（非法输入被吞成成功）。

### CE7（构造后自我推翻，如实记录）— `diag_approx_report` 低秩漏算 L·Lᵀ
- **构造假设**：`information_weight.cpp:275` `if (true_c.r > 0 && true_c.l != nullptr)` ⇒ `r>0 ∧ l==NULL` 时**静默丢掉 L·Lᵀ 项** ⇒ `quad` 偏小 ⇒ `variance_ratio` 低估。
- **期望推翻**：该报告量口径。
- **是否推翻**：**未推翻——该分支不可达**。`:254` 先调 `w_info_solve` → `w_info_low_rank:145` `if (r > 0 && l == nullptr) return reject("null_input");` ⇒ `:255-258` 提前 return。`:275` 的 `!= nullptr` 是**防御性死码**，非活缺陷。（另：`:268-281` 的 `else` 分支对 `dense_spd`/`diagonal` 的展开我核对无误。）

---

## 6. 盲复算

**方法**：遮住既有判定与子代理结论，**从定义式独立重算**本片出现的常数与判据，再与仓内登记值比对。

| 量 | 我的独立推导 | 仓内取值 | 判定 |
|---|---|---|---|
| MAD→σ 一致性常数 | `1/Φ⁻¹(0.75) = 1.482602218505602…` | `snr_estimator.h:123` 同值 | ✅ **一致**（正确） |
| 零点 SE 常数 | `√(π/2) = 1.2533141373155003` | `snr_science.cpp:279` 用 `1.253` | ⚠️ **偏松**（截断 −2.51e-4，系统性低估） |
| 跨块 σ 偏置 | `2.3548200450309493 / 1.230310 = 1.914005…` | `p1snr_science_test.cpp:209` 用 `1.9140054498711294` | ✅ 一致（但该断言零生产符号，见 B-否决区） |
| 10–90% TM→σ | `2(φ(Φ⁻¹(.55))−φ(Φ⁻¹(.95)))/0.8 ≈ 0.7316731` | 生产 `0.7316727929211932` | ✅ **一致**（值正确；缺陷在 `snr_science.cpp:56` 的死副本，不在数值） |
| Moffat4 圈入率 | `∫₀^r 2πr′(1+r′²/2σ²)⁻⁴dr′ / ∫₀^∞ = 1−(1+r²/2σ²)⁻³` | `snr_science.cpp:224` 同式 | ✅ **一致**（推导与实现相符） |
| 默认孔径 f_in | `r=1.5·1.230310σ ⇒ u=2.702871 ⇒ f_in=0.94936` | 实现隐含同值 | ✅ 一致 |
| Horne 最优提取 | `σ_F⁻² = Σ P_i²/σ_i²` | `snr_science.cpp:202,:206` | ✅ **一致**（正典相符） |
| 天空受限分支 | `σ_F = σ_sky/√(Σ P²)` | `:209` | ✅ 一致 |
| `W=a²pᵀC⁻¹p` / `Q=a·dᵀC⁻¹p` | 对称 C ⇒ `dᵀC⁻¹p = pᵀC⁻¹d`；与 `information_weight.h:5-7` 合同逐项一致 | `:103-104` | ✅ 一致 |
| 白噪式 `a²ΣP²/σ_pix²`、`Var(F̂)=1/ΣW` | 与 FZ-COND-WHITENOISE / CONTROL_WEIGHT_SNR 一致 | `:232,:313` | ✅ 一致 |
| 天空环因子（典型 r_out=3r） | `n_sky=8πr²=8·n_pix ⇒ 1+1/8=1.125` | 代码用 `2.0` | ⚠️ **偏松**（σ 高估 1.33×） |

**盲复算总判**：**偏松。** 12 项里 9 项一致、3 项偏松，**0 项偏严**。这与本片最刺眼的现象吻合——**仓内给自己定的容差往往比冻结文档更严（`noise_model_science_test.cpp:220` 用 2% 而 `NOISE_ESTIMATION.md` 冻结 5%；`p1snr_science_test.cpp` 用 1e-12/1e-15 而冻结是 1e-9），但真正守门的那几道 Python 门不是偏松而是恒红/空转**。换言之：**判据侧偏严、执行侧失效**，而「检查通过」被反复当作正确性证据——正是本轮要防的失真模式。

---

## 7. 子代理派发记录

**派发总数：7 个**（工具层把同一 prompt 触发了两次，故 3 个主题各 2 个 + 1 个独立复算；重复者按**独立复现**使用，互为交叉验证）。

| # | subagent id | 主题 | 状态 |
|---|---|---|---|
| 1 | `e7f8f884` | 4 个 C++ 测试文件（1489 行）对抗审 | 已完成（终报） |
| 2 | `4ae0d30c` | 同上（复现实例） | 已完成 |
| 3 | `0adbb516` | checkers / build / 导出面 / ABI 锁 | 已完成（**中途简报 + 终报**） |
| 4 | `78585dbf` | 同上（复现实例） | 已完成 |
| 5 | `cf684889` | 模块文档 + 悬空引用普查 | 已完成 |
| 6 | `28dabaf2` | 同上（复现实例） | 已完成 |
| 7 | `cf086583` | 5 个科学文件独立重算 | 已完成（终报） |

**如何逐条复核**：对每条子代理结论，我按「可否证 + 谁承担」分三档处置 —— **独立复核并采纳**（我亲自跑命令/读原文确认）、**降档采纳**（结论方向对但档位或范围需收窄）、**否决**（证据不足或被我推翻）。

### ✅ 独立复核并采纳（15 条，摘要）
1. `check_snr_caliber.py` W2 恒红、`docs/plugins/` 已删 —— 我实测 `ls` 确认（§3.11）。
2. `check_saturation_wiring.py` W1 必红、`source_ref` 无 `line` —— 我用 python 直读 `defaults.json`，键集 `['id','path','quote','sha256','value_text']`，确认（§3.13）。
3. ctest 默认调用不带 argv ⇒ W1 产品检查零执行 —— 与我 §3.11 独立同构。
4. `p1noise_tests_perf.cpp:86-92` 恒真门 —— **我先独立发现，复核确认**，并补出子代理未指出的"方向量无法检出异常加速"（§5 CE2）。
5. `snr_frame_science.cpp:157-161` 不可达 —— **我先独立发现**，子代理独立给出同一可达性证明（§5 CE3）。
6. `snr_frame_science.cpp:203-208` 恒红门（`&ref_res` 永非 null）—— 采纳（§3.9）。
7. `snr_science.cpp:56` `kTrimMeanToSigma` 死副本 —— **我先独立发现**（读完确认零引用），子代理补充"注释自称已删同一缺陷的未修兄弟"（§3.7）。
8. `snr_science.cpp:226` `n_sky=n_pix` 兜底 —— 我先发现，补独立推导（r_out=3r ⇒ 1.125 vs 2.0）（§5 CE4）。
9. `snr_science.cpp:279` `1.253` 可现场导出 —— 采纳独立推导 `√(π/2)`（§5 CE5）。
10. `snr_science.cpp:165,:185` 异常穿 C ABI —— 采纳，并补 `snr_estimator.h:334` 亦未声明上界（§3.7）。
11. `p1snr_science_test.cpp:473-476` 返回码丢弃致双失败判绿 —— 采纳（§3.5）。
12. `p1snr_science_test.cpp:449-451` 生产当自身 oracle —— 采纳（§3.5）。
13. `noise_model_science_test.cpp:400-421` SNR-009 零生产符号、`var_w ≡ var_opt` —— 采纳（§3.4）。
14. `noise_model_science_test.cpp:369-370` 用生产自身输出字段作期望 —— 采纳（§3.4）。
15. `snr_oracle.py` 与 5 个锚值不可复核、7 处悬空引用 —— 采纳并**亲自实测**（§3.5 B-区）。

### ⚠️ 降档采纳（3 条）
- **子代理 7** 称 `snr_science.cpp:151-245` 的五条退化路径 `return 0` 应判阻断 —— 我**降为须修**：这些路径确实把 status 置 1 且 `wrapper_phase1/snr_frame_science.cpp:60-61` **检查了** `out->status != 0`（fail-closed 在调用侧闭合）。但 `:196/:212` 两条无任何调用者检查 status，`snr_estimator.cpp:88` 虽检查返回码但同型问题仍在 ⇒ 保留部分阻断、收窄范围。
- **子代理 1** 称 `noise_model_science_test.cpp:222-234` 的 ≥95% 门「结构性偏绿」并给出 ν≈4095 解析估计 —— 我采纳方向，**标注其数字为解析估计非实测**（子代理自己已声明 UNVERIFIED）。
- **子代理 7** 提出的 `diag_approx_report` `a²` 口径可能不闭合 —— 我**不判为缺陷**，降为 **建议/UNRESOLVED**：生产未调用该函数，且需先定 `a` 的物理口径（需读 `NOISE_MODEL.md` §5c，超出本片）。

### ❌ 否决（5 条，逐条说明）

| 否决项 | 派发者原判 | 否决理由 |
|---|---|---|
| **OpenMP 并行区 = 私建线程池（阻断）** | 我最初自拟此判 | **部分否决。** 子代理 7 举证根 `CMakeLists.txt:1138-1143` 显式授权 `OpenMP::OpenMP_CXX`，注释 `:1139-1141`「线程数不在本库硬编码，由 Runtime 注入」，且 `acsd_phase1_stars`/`acsd_p1_sdet`/`acsd_p1_dpsf` 为**同款接线**。我实测复核后**接受**：`snr_frame_science.cpp:112-114` 是共享 OpenMP 运行时并行区，**不是** `std::thread` 私建池，**不判为私建池违规**。但**保留并改判为须修**的实锤是：`snr_estimator.cpp` 10 处 pragma **无 `_OPENMP` 守卫**（同层两套口径），且 `module.yaml:22` 对整层宣称「单线程顺序」为**假**（已入 B2）。 |
| **`docs/detail/algorithms_phase1/07_noise_snr.md` 是 W2 门的迁移目标，迁移后即可通过** | 子代理 3（中途简报） | **否决。** 我实测 `ls docs/detail/algorithms_phase1` → **目录不存在**；`docs/detail/` 下只有 `00_INDEX.md anchors common.md infrastructure registry *.md`。该文件在**现行树任何位置都不存在**（`find` 仅在 `run/` 归档副本中命中）。故不是"迁移后路径更新即可"，而是**法源文件整体缺失**（已升级为 B4）。 |
| **`SnrSourceRow ref_row;` 未初始化读取** | 候选（我与子代理 7 均提出后自查） | **否决。** `wrapper_phase1/snr_frame_science.h:38`（`flux_adu = 0.0`）与 `:43`（`fwhm_px = 0.0`）有 NSDMI，默认初始化生效；`ref_row` 随后 `:190-191` 两字段均被显式赋值。 |
| **`new[]`/`free()` 分配器错配** | 候选（自查） | **否决。** `snr_estimator.cpp:668 new[]` ↔ `:706 delete[]`；v2 `:779,:789 std::malloc` ↔ `:807 std::free`；v3 `:894,:907 std::malloc` ↔ `:927 std::free` —— 三组各自配对自洽。 |
| **`p1snr_science_test.cpp:209-210` 的 1.9140054498711294 是否逐位相符** | 子代理 1 标注 UNVERIFIED | **维持 UNVERIFIED，不据此下判。** 本片纪律禁运行，故我只采用其**结构性**结论（该断言零生产符号、永不可能失败），**不采用**任何"数字是否匹配"的结论。 |

**净结果**：7 个子代理共提出约 130 条候选 → 我 **18 条独立采纳**（其中 **2 条经我亲自数值/命令复算**）、**3 条降档**、**5 条否决**、**1 条升级并改写定性**（B4：非"路径待更新"而是"法源整体缺失"）、**1 条触发我自己结论的更正**（见下）。

### 🔄 因子代理证据而更正的我自己的初判（1 条，如实记录）

我初稿在 §3.15 写：「`module.yaml:107-119` notes 所列 `DISP-NOISE-001..009` 九条整改责任**至今全部仍是"登记不改码"**」。
子代理 5/6 举证 DISP-NOISE-001/002 已整改，我**亲自复核命令**：
```
grep -n "mutex\|lock_guard" lib/algorithms/noise_snr/cpp/src/noise_model.cpp
  → 25:#include <mutex>   45:static std::mutex g_model_registry_mutex;
    49/56/63/69/81/87: std::lock_guard<std::mutex> lk(...)
grep -c "SNR_FLOOR_UNBOUND" lib/algorithms/noise_snr/cpp/src/noise_model.cpp   → 10
```
⇒ **我的初判为假，已在 §3.15 就地更正**。更正后的结论比原判更严重：DISP 登记表**既非全未整改、也非已同步，而是部分整改而登记未回填**——读者无法判断任一条的真实状态。

---

## 8. 自证段（可复跑命令）

> 全部为**只读**命令；不编译、不执行被测二进制、不写仓内文件。全部在 `/workspace/Astro CS Database` 下运行。

```bash
# ── 0. 基线
git -c core.quotepath=false rev-parse HEAD          # 期望 850a9edefd47434b9ab71bc907c3de1e0814b303

# ── 1. 片成员与行数（B 节：6582 = 片清单 实际行数）
sed -n '363,392p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
wc -l lib/algorithms/noise_snr/src/module_entry.cpp \
      lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp \
      lib/algorithms/noise_snr/cpp/include/snr_estimator.h \
      lib/algorithms/noise_snr/cpp/test/noise_model_science_test.cpp \
      lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp \
      lib/algorithms/noise_snr/cpp/src/information_weight.cpp \
      lib/algorithms/noise_snr/cpp/src/snr_science.cpp \
      lib/algorithms/noise_snr/tests/p1noise/p1noise_saturation_test.cpp \
      lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp \
      lib/algorithms/noise_snr/include/acsd/noise/types.h \
      lib/algorithms/noise_snr/tests/p1noise/check_snr_caliber.py \
      lib/algorithms/noise_snr/memory.md \
      lib/algorithms/noise_snr/tests/p1noise/check_saturation_wiring.py \
      lib/algorithms/noise_snr/tests/p1noise/p1noise_tests_perf.cpp \
      lib/algorithms/noise_snr/module.yaml \
      lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.h \
      lib/algorithms/noise_snr/wrapper_phase1/README.md \
      lib/algorithms/noise_snr/cpp/build.ps1 \
      lib/algorithms/noise_snr/tests/p1noise/p1noise_abi_layout_lock.json \
      lib/algorithms/noise_snr/src/module_exports.map \
      lib/algorithms/noise_snr/src/acsd_p1_noise.def | tail -1   # 期望 6582 总计

# ── 2. B1 死词表物证（产出侧宏全仓零引用 ⇒ estimate 从不写这三个键）
grep -rn "NOISE_O_KEY_SIGMA_BG\|NOISE_O_KEY_VARIANCE_BG\|NOISE_O_KEY_IVAR_BG\|NOISE_O_KEY_FLOOR_FALLBACK\|NOISE_O_KEY_WORKERS\|NOISE_O_KEY_DIAG_VARIANCE\|NOISE_O_KEY_DIAG_GAIN_VAR" .
# 期望：仅 include/acsd/noise/types.h 的 #define 行（9 处），别处 0
sed -n '1115,1119p;1285,1287p;1385p;1442p' lib/algorithms/noise_snr/src/module_entry.cpp

# ── 3. B2 module_entry.cpp 无可达构建
grep -n "module_entry" lib/algorithms/noise_snr/CMakeLists.txt          # 期望 :34
sed -n '26p'  lib/algorithms/noise_snr/CMakeLists.txt                   # 期望「不在根图」
sed -n '386p;395p;426p' CMakeLists.txt                                  # 期望 三处「不在根图/未入库」
sed -n '1128,1131p;1138,1144p' CMakeLists.txt                           # 期望 acsd_phase1_noise 真实源表 + OpenMP
sed -n '19,22p' lib/algorithms/noise_snr/module.yaml                    # 期望「未编入根 CMake 主构建」「单线程顺序」

# ── 4. B3 科学矩阵孤儿 + 不可编译
grep -rn "noise_model_science_test" --include=CMakeLists.txt --include=*.cmake --include=*.sh --include=*.py . | grep -v '^./run/'
# 期望：0 命中
sed -n '4p;20p' lib/algorithms/noise_snr/cpp/Makefile
sed -n '20p'   lib/algorithms/noise_snr/cpp/test/noise_model_science_test.cpp   # 期望缺 snr_science.cpp
grep -n "snr_calib_zero_point_standard_error" lib/algorithms/noise_snr/cpp/src/snr_science.cpp   # 唯一定义点 :275

# ── 5. B4/B5 法源与 oracle 双双不存在
ls docs/detail/algorithms_phase1                                        # 期望 No such file
ls docs/plugins                                                         # 期望 No such file
grep -rn "07_noise_snr.md" lib/algorithms/noise_snr/                     # 期望 4 处引用，0 处文件
grep -rn "126.30302219525699\|snr_oracle.py" . | grep -v '^./run/'      # 期望 0 命中

# ── 6. 恒真门（CE2 / CE3）
sed -n '86,92p'  lib/algorithms/noise_snr/tests/p1noise/p1noise_tests_perf.cpp
sed -n '81,83p;108,121p;126,133p;157,161p' lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp
sed -n '136p;219,220p' lib/algorithms/noise_snr/tests/p1noise/p1noise_saturation_test.cpp
sed -n '209,210p;449,451p;473,476p' lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp
sed -n '369,370p;400,421p' lib/algorithms/noise_snr/cpp/test/noise_model_science_test.cpp

# ── 7. 恒红门（CE-B11/B12）
sed -n '26,28p;78,99p;147p;161p' lib/algorithms/noise_snr/tests/p1noise/check_snr_caliber.py
python3 -c "import json;d=json.load(open('eng/packaging/config/defaults.json',encoding='utf-8'));\
s={f['key']:f for f in d['fields']}['noise.saturation_level'];\
print(sorted((s.get('source_ref') or {}).keys()), 'has_line=', bool((s.get('source_ref') or {}).get('line')))"
sed -n '61,65p;120,122p' lib/algorithms/noise_snr/tests/p1noise/check_saturation_wiring.py

# ── 8. 静默降级（CE4 / CE13）
sed -n '220,229p' lib/algorithms/noise_snr/cpp/src/snr_science.cpp
sed -n '165p;185p' lib/algorithms/noise_snr/cpp/src/snr_science.cpp        # 无界 half + int 溢出 reserve
sed -n '1399,1406p' lib/algorithms/noise_snr/cpp/src/noise_model.cpp      # else 分支逐像素写全局标量

# ── 9. 硬编码可现场导出（CE5）
sed -n '279p' lib/algorithms/noise_snr/cpp/src/snr_science.cpp
python3 -c "import math;print(repr(math.sqrt(math.pi/2)), (1.253/math.sqrt(math.pi/2)-1))"
grep -n "kTrimMeanToSigma" lib/algorithms/noise_snr/cpp/src/snr_science.cpp   # 期望仅 :56 定义，0 引用
sed -n '99,101p' lib/algorithms/noise_snr/cpp/src/snr_science.cpp

# ── 10. 退役声明证伪（B16）
sed -n '77,89p'  lib/algorithms/noise_snr/module.yaml
grep -n "snr_noise_scale_law\|snr_noise_gain_variance" lib/algorithms/noise_snr/src/module_entry.cpp  # 期望 :1477 / :1522

# ── 11. memory.md 行锚失真
wc -l lib/algorithms/noise_snr/cpp/src/noise_model.cpp lib/algorithms/noise_snr/cpp/include/snr_estimator.h
# 期望 1482 / 647；memory.md:36-37 记 475 / 526
grep -n "CMakeLists.txt:521-524\|noise_model.{h,cpp}" lib/algorithms/noise_snr/memory.md

# ── 12. 导出面悬空自引用 + DEF 技术性错误
sed -n '1p;7p' lib/algorithms/noise_snr/src/module_exports.map
ls lib/snr_estimator/src/module_exports.map            # 期望 No such file
ls tests/unit/p1_noise/adapter_test.cpp                 # 期望 No such file（真实在 eng/tests/unit/…）
ls eng/tests/unit/p1_noise/adapter_test.cpp             # 期望 存在
sed -n '1,6p' lib/algorithms/noise_snr/src/acsd_p1_noise.def
sed -n '6,10p' lib/algorithms/noise_snr/cpp/include/snr_estimator.h   # POSIX 下 SNR_API 为空宏

# ── 13. build.ps1 只编一个 TU
sed -n '35,39p' lib/algorithms/noise_snr/cpp/build.ps1

# ── 14. B18 恒红断言的数值复算（纯计算，不编译不运行被测码）
python3 -c "
import math
kM=1.230310; kT=0.7316727929211932
F,fwhm,rs=30000.0,3.0,120.0          # noise_model_science_test.cpp:374-375 的 psf_leg
sig=fwhm/kM; sky=rs/kT
h=int(math.ceil(12.0*sig*kM)); h=30 if h<30 else h; h=256 if h>256 else h
a2=2.0*sig*sig
V=lambda i,j: 1.0/(1.0+(i*i+j*j)/a2)**4
s=sum(V(i,j) for j in range(-h,h+1) for i in range(-h,h+1))
s2=sum(V(i,j)**2 for j in range(-h,h+1) for i in range(-h,h+1))
snrF=F/math.sqrt(sky*sky/(s2/(s*s))); want=(5000.0-1000.0)/120.0
print('half=%d snr_psf=%.6f want=%.6f diff=%.6f tol=1e-4'%(h,snrF,want,abs(snrF-want)))"
# 实测输出：half=36 snr_psf=33.934825 want=33.333333 diff=0.601492 tol=1e-4  ⇒ :389 恒红
sed -n '382,389p' lib/algorithms/noise_snr/cpp/test/noise_model_science_test.cpp
sed -n '430,432p' lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp   # 互斥断言
sed -n '630,631p;683p'   lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp       # 生产已改 Horne

# ── 15. B19 退役声明证伪
sed -n '10,11p' lib/algorithms/noise_snr/module.yaml
sed -n '140p'   lib/algorithms/noise_snr/memory.md
grep -n "snr_extract_model_v3\|snr_free_model_v3" \
     lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp
# 期望：4281 取指针 / 4283 free / 4330「snr_extract_model_v3 必需」硬失败 / 4490 调用

# ── 16. B20 manifest 三依据断链
ls -d eng/ci ; ls 11_MODULE_SOURCE_TEST_STANDARD.md ; ls MODULE_MIGRATION_MATRIX.csv
# 期望：三者全部 No such file or directory

# ── 17. 我自己结论的更正证据（DISP-001/002 已整改，登记未回填）
grep -n "mutex\|lock_guard" lib/algorithms/noise_snr/cpp/src/noise_model.cpp | head
grep -c "SNR_FLOOR_UNBOUND" lib/algorithms/noise_snr/cpp/src/noise_model.cpp   # 期望 10

# ── 18. B21/B22 恒真门与 rc 零断言
sed -n '107,108p;139,140p;215p' lib/algorithms/noise_snr/tests/p1noise/p1noise_saturation_test.cpp
grep -c "check(" lib/algorithms/noise_snr/tests/p1noise/p1noise_saturation_test.cpp   # 28
grep -n "rc == 0\|rc==0\|\.rc)" lib/algorithms/noise_snr/tests/p1noise/p1noise_saturation_test.cpp
# 期望：rc 只在 127/206/209 出现（捕获+打印），无任何 rc 断言

# ── 19. 隐私纪律自证（确认本轮零 git 写、未跑二进制）
git -c core.quotepath=false status --porcelain   # 期望：仅本交付件为新增，其余工作树未被我改动
```

---

## 9. 给前台的处置建议（按 AGENTS §8，无法裁决者登记 UNRESOLVED）

1. **B1 必修，且必须先补回归**：在 `estimate` 输出中真正写出 `sigma_bg_global`/`variance_bg_global`/`ivar_bg_global` 三键（或让 fill 读 `scalars_base64`），并在 `fill_noise_field` 侧对三键缺失**显式拒绝**而非取 0；同时消除 `last_floor_fallback` 与 manifest 的相反声明。注入「无空间场模型」用例必须判红。
2. **B4/B5 需负责人裁决**：是补齐 `07_noise_snr.md` 与 `snr_oracle.py`，还是撤回全部「独立复算 / 口径正本」声明？按 AGENTS §8 我不自行裁决，**登记 UNRESOLVED**，仅列证据。
3. **B3 必修**：把 `noise_model_science_test.cpp` 接入 `tests/p1noise/CMakeLists.txt`（真实生产库），并补 `snr_science.cpp` 依赖；修好之前，SNR-001..014 一律按**未执行**对待。
4. **恒真门批量整改**（B6/B7/B8/B9/B10）：按本仓已有正确范例 `eng/tests/unit/p1snr/p1snr_frame_parity_test.cpp:273-274`（「非恒真: …确实不同」），给每条加**负例臂**（注入已知错误后必须判红）。`perf_trend_025x` 改为方向量 `r2.seconds >= 0.25 * r1.seconds`。
5. **恒红门整改**（B11/B12）：`check_snr_caliber.py` 补 `find_repo` 回退、修文档锚（或撤 W2/W3）、并让**默认 ctest 调用带真实产物**或**明确把 `0 份产品` 打成 FAIL**；`check_saturation_wiring.py` 对齐 `defaults.json` 新 schema（`quote`+`sha256`），并把 `:121-122` 的 `if not p.exists(): continue` 改成 `fail(...)`。
6. **B13/B14**：`n_sky` 缺失改为 fail-closed 或由输入几何现场导出；`saturation_level` 默认 0=关闭改为保守推断 + 强制显式声明（当前会把 4×10⁴ 的平台污染放行）。
7. **B15**：`NOISE_MODEL.md` §11 的 SAT-001 oracle 与在运测试断言相反 —— 按 AGENTS §3/§8，应**更新文档**并走独立变更，不得以行内注释充当 waiver。
8. **UNRESOLVED（不自行裁决）**：`information_weight.cpp:259,:288` 的 `variance_ratio` 是否应含 `a²`（需先定 `NOISE_MODEL.md` §5c 中 `a` 的物理口径）；`snr_estimator.h:6-10` POSIX 空宏与 `memory.md:62-65` 的 DLL 装载叙述哪个是现行平台事实。
