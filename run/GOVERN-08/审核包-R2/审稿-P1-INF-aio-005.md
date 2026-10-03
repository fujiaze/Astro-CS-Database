# 审稿 P1 · INF-aio-005（第 1 遍 · 对抗审稿）

- **片号**：`INF-aio-005`
- **层**：`lib/infrastructure/aio`
- **仓库**：`/workspace/Astro CS Database`
- **基线 HEAD**：`850a9ede`（权威版清单指定）。⚠️ 实测 `git rev-parse HEAD` 返回 `1fa477a7a05c315550df2ce2bafc3e64ed9bbd78`，与清单声明的 `850a9ede` **不一致**。本审稿一律按**工作树当前实际内容**取证，不做 checkout（零 git 写）。差异原因登记为待裁决项（见 §4-B7）。
- **分母口径**：`片清单-权威版.yaml:3054-3089`，成员 28 份 / 实际行数 10630。实测 `wc -l` 逐份求和 = **10630**，与清单**完全一致**（口径：`wc -l`，按换行计；不含行尾无换行的末行不计）。
- **本片性质**：`lib/` 生产源码 + 同目录测试/驱动/文档/构建脚本混合。

---

## 1. 读完了吗

| 口径 | 数值 |
|---|---|
| 成员份数（清单） | 28 |
| 成员总行数（清单 / 实测 `wc -l`） | 10630 / 10630（一致） |
| **我亲自逐行读完的份数** | **20** |
| **我亲自逐行读完的行数** | **7814** |
| **亲自覆盖率（行）** | **7814 / 10630 = 73.5 %** |
| 亲自覆盖率（份） | 20 / 28 = 71.4 % |
| 未亲自读完（已派子代理覆盖，见 §7） | 8 份 / 2816 行 |
| 派发但我本人未复核其结论的份数 | 8 |

### 1.1 亲自读完的 20 份（按清单序）

| # | 文件 | 行数 |
|---|---|---|
| 1 | `lib/infrastructure/aio/io/fits_core.c` | 1745 |
| 2 | `lib/infrastructure/aio/tests/hiss_correctness_test.cpp` | 1339 |
| 3 | `lib/infrastructure/aio/tests/p1hips/p1hips_tests_diag_prov.cpp` | 986 |
| 4 | `lib/infrastructure/aio/src/aio_xisf.cpp` | 746 |
| 5 | `lib/infrastructure/aio/src/aio_sparse_punch.h` | 682 |
| 6 | `lib/infrastructure/aio/include/hiss_format.h` | 621 |
| 7 | `lib/infrastructure/aio/include/aio_pipeline.h` | 348 |
| 8 | `lib/infrastructure/aio/runtime/artifact_store/artifact_manifest_validator.py` | 287 |
| 9 | `lib/infrastructure/aio/tests/hips_robust_sanitize_driver.cpp` | 223 |
| 10 | `lib/infrastructure/aio/product_io/include/astro/aio/atomic_publish.h` | 168 |
| 11 | `lib/infrastructure/aio/tests/test_report.md` | 127 |
| 12 | `lib/infrastructure/aio/src/hiss_tile_model.h` | 105 |
| 13 | `lib/infrastructure/aio/include/aio_pipeline_engine.h` | 99 |
| 14 | `lib/infrastructure/aio/src/ahpx/aio_ahpx_writer.h` | 88 |
| 15 | `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/stack_db.h` | 78 |
| 16 | `lib/infrastructure/aio/product_io/README.md` | 73 |
| 17 | `lib/infrastructure/aio/src/aio_fits.h` | 45 |
| 18 | `lib/infrastructure/aio/tests/gaia_sanitize_driver.c` | 33 |
| 19 | `lib/infrastructure/aio/aio_build_config.healpix.json` | 11 |
| 20 | `lib/infrastructure/aio/src/aio_log.h` | 10 |

### 1.2 **未读完的 8 份 —— 如实列出**

以下 8 份（合计 **2816 行**）**我本人没有从头到尾读完**。它们已全部纳入 §7 的子代理派发范围；**在交付时刻，子代理结果尚未回传，故本报告的结论不依赖它们**，我也**没有**引用它们的任何结论。

| 文件 | 行数 |
|---|---|
| `lib/infrastructure/aio/tests/test_snr_unknown_block.cpp` | 598 |
| `lib/infrastructure/aio/src/ahpx/aio_ahpx_writer.cpp` | 494 |
| `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/tests/test_hp_stack_hiss.py` | 434 |
| `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/hp_stack_hiss.cpp` | 387 |
| `lib/infrastructure/aio/healpix_db/memory.md` | 299 |
| `lib/infrastructure/aio/README.md` | 249 |
| `lib/infrastructure/aio/build.ps1` | 204 |
| `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/build.ps1` | 151 |

⚠️ **这 8 份中，`aio_ahpx_writer.cpp` 与 `build.ps1` 是生产/构建面**，未亲自读完是本片的**已知覆盖缺口**，登记为待补（第 2 遍优先）。

---

## 2. 本片判定

### 判定：**需修**（无阻断项；有 5 条高价值"判据自证"级发现）

判定理由：本片生产代码未发现崩溃级/数据销毁级缺陷，但**发现 3 类系统性的"判据自证"缺陷**——即本轮负责人点名、价值最高的「用同一个定义式既当被检量又当期望量」的断言。这不是个别笔误，而是本片在验证方法论层面的成建制问题：**大量"通过"的判据其实无法证伪，或其"期望值"就是被检实现本身**。

#### 最重 3 条

1. **【须修·最重】`weight` 块已被"退役"，但在管线合同里仍是一等公民，且该退役的机器判据根本不存在。**
   最高设计 §2.1 与 `aio_ahpx_writer.h:16-17`（AHPX-WEIGHT-RETIRE-20260920）都宣称"不存在权重模式、只写 pixel 与 snr"，`aio_ahpx_format.h:33` 定义 `RETIRED_WEIGHT_FIELD`，`aio_ahpx_writer.cpp:269` 在 ahpx 落盘时拒绝 `weight`。
   **但** `aio_pipeline.h:310` 仍把 `weight` 列为标准块，`aio_pipeline.cpp:138` 的标准块名表仍含 `"weight"`，`aio_pipeline_engine.cpp:116,120` 的块丢弃策略仍丢 `weight`，`tests/test_pipeline_blocks.py:390-419` 仍在跑它。
   而 `aio_pipeline.h:30-31 / 227 / 262` **三处**把"机器判据"指向 `eng/ci/check_aio_io_boundary.py` —— **`eng/ci/` 目录在仓内不存在**，该脚本只存活于 `run/FINAL-07-e2e/bisect/*`、`run/P1-CONCURRENCY-CALIB-01/wsrc/*` 等归档实验副本中。
   ⇒ **退役声明只在容器格式层落实，管线层原封不动，而用来证明"生产调用点 = 0"的那个检查器是空的。** 这与负责人举的那一例（退役声明声称零消费者 → 被证伪 → 本地复刻成了唯一来源 → 读数仍被当生产上报）是**同一失效模式的复发**。

2. **【须修】验证器有一条"自称做了、实际没做"的检查（已构造反例并实测推翻）。**
   `artifact_manifest_validator.py:12` 自述："`storage_uri` 必须为 URI 形态…**裸绝对路径/Windows 盘符路径拒绝**"。
   实际 `_STORAGE_URI_RE`（`:44`）第二分支 `^[a-z][a-z0-9]*:[A-Za-z0-9._/+-]+$` 会匹配 `c:/data/x.fits`。我在 `/tmp` 单独复现该正则并实测：
   ```
   ACCEPTED  'c:/data/x.fits'
   ACCEPTED  'c:/Users/foo/bar.fits'
   ACCEPTED  'd:/prod/2026/run1/manifest.json'
   rejected  'c:\\data\\x.fits'
   ```
   ⇒ **小写盘符的 Windows 绝对路径被放行**。注释所声称的那条检查并不存在。这正是负责人所说"看到任何『检查通过』的机制，不要据此认为实现正确"的实例：这里连"检查"本身都是注释里的一句自我声明。

3. **【须修】一份仍在仓内的测试报告，把 3 个失败判为"测试错、算法对"，并据此把"所有单元测试通过"标记为已完成；同一报告还声称"无软通过、所有通过项均为真实断言通过"——该声称可被证伪。**
   `tests/test_report.md:86-104` 逐条把失败归因为"测试期望值错误"，其中 **[TEST 7]** 明写"结论：测试期望值错误，算法行为正确"；`:123` 完成标准第 2 条写"**已完成**（368/371 通过，3 失败为测试期望问题）"。
   而 `test_report.md:47-49` 声称"无 `ASSERT_TRUE(true,...)` 软通过 / 所有通过项均为真实断言通过"——
   **该声称被我读到的同一测试文件直接推翻**：`hiss_correctness_test.cpp:430-434`
   ```cpp
   int out_of_range = 0;
   for (auto s : support) {          // support 是 std::vector<uint8_t>
       if (s > 255) out_of_range++;  // uint8_t 提升为 int，最大 255 ⇒ 恒假
   }
   ASSERT_TRUE(out_of_range == 0, "所有 support 值在 [0, 255] 范围内");
   ```
   这是一条**结构上不可能失败**的断言。报告里 [TEST 7] 那一项正是被"改成与实现一致"之后转绿的（见 §5 反例 C1）。

---

## 3. 逐文件清单（读了什么 / 看到什么 / 判定）

> 判定三档：**通过** / **需修** / **建议**。带 `文件:行`。

### 3.1 `io/fits_core.c`（1745 行）— **需修**

**读了什么**：全 1745 行。FITS 卡片构造/解析、2880 块对齐、1 补码 DATASUM/CHECKSUM 增量累计、ASCII 编解码、reader/writer/verify 三条路径。

**看到什么**

| 位置 | 发现 |
|---|---|
| `:684-703` | **静默降级（伪装成正常返回）**。`fio_hdu_checksum_stream`：`if (fio_file_seek(...) != 0) return 0;`（`:691`）与 `if (got == 0) break;`（`:698`）**既不置错误码也不写 `err`**。被 `:1466` 用于写 CHECKSUM。若 seek 失败则 CHECKSUM 写成 `encode(0, complm=1)` 的值，且 `verify_before_rename=0` 时静默发布。 |
| `:1106-1179` | **错误通道被显式丢弃 + 写返回未检查**。`:1119` 是 `(void)err; (void)cap;`；`fio_card_write`（`:259-264`）返回写入字节数，**在 `:1121-1173` 的每一次调用点都未被检查**。⇒ 磁盘写满发生在 header 阶段时，header 静默截断；随后 `:1405` 的 `data_bytes_written != data_bytes_total` 只比对**数据**字节，不比对 header。`verify_before_rename=0` 时可发布损坏文件。 |
| `:376-386` | **溢出被折叠成"空图"**。`fio_header_pixels` 在 `n > INT64_MAX / h->naxis_n[i]` 时 `return 0`。⇒ `:388-394` 得 `data_bytes = 0` ⇒ `:795` 的 `if (rd->data_bytes > 0)` **整段截断预检被跳过** ⇒ 写侧 `:1258` `data_bytes_total = 0`。"溢出"与"空"两种截然不同的语义共用一个返回值。 |
| `:898-915` | **有符号溢出**。`per_plane *= rd->hdr.naxis_n[i]` 无溢出保护。 |
| `:1218-1225` + `:1494` | **TOCTOU + 跨平台语义分叉**。`overwrite=0` 的存在性探测与 `:1494` 的 `rename()` 之间无原子性；且 `:1494` **完全忽略 `wr->overwrite`**。POSIX `rename()` 原子替换；**Windows MSVC CRT 的 `rename()` 在目标存在时失败**。本仓确为 Windows 目标（本文件 `:19,34,46,51` 与 `build.ps1` 的 `_WIN32` 分支）⇒ **`overwrite=1` 覆盖已存在目标在 Windows 上会失败、POSIX 上成功**。 |
| `:1508, 1520` | `remove(wr->tmp)` **返回值被忽略** ⇒ 临时文件残留时无任何信号。 |
| `:623-652` / `:656-680` | **自洽 oracle（核心发现，见 §4-B1）**。写侧 `fio_encode_checksum` 与验侧 `fio_decode_checksum` 互为代数逆；两侧又共用同一套 `fio_dsum_*`。 |
| `:1657-1664` | 值得肯定：`verify` 对"全零 CHECKSUM 占位串"**显式 fail-closed 拒绝**（`:1645-1647` 注释明确说明了旧的 `sum==0` 兼容分支是真实 fail-open）。这是本片少数做对的地方。 |

### 3.2 `tests/hiss_correctness_test.cpp`（1339 行）— **需修**

**读了什么**：全 1339 行。测试框架宏、21 个用例、`main()` 汇总。

**看到什么**

- `:430-434` **恒真门**（详见 §2-3）。注释自己都承认"uint8 不可能 >255"。
- `:317-334`（TEST 6，标题"单像素通量守恒"）**判别力为零**：夹具取 `sum_area = 1.0` 恰好使"除面积"与"不除面积"两种语义给出同一结果 100.0 ⇒ **把 `finalize_signal` 改成除以面积，本用例照样绿**。
- `:359-374`（TEST 7）与 `:314-315,332`（TEST 6 注释）**互相矛盾**：前者写"signal = sum_flux（不除面积）"，后者写"signal = sum_flux / sum_area"。我核对冻结合同 `include/hiss_format.h:342` 与 `:367-368`，**正本是"不除面积"** ⇒ **TEST 6 的注释是错的**，而 TEST 6 又恰好因 `sum_area=1.0` 而无法证伪。两处错误叠加 ⇒ 该用例组对"通量守恒"这一科学语义**零覆盖**，而文件头 `:6` 把这组命名为"Drizzle 测试 (6~11): **通量守恒**"。
- `:496-513, 515-575`（TEST 11）**测的是测试自己的复刻**：`ref_compute_auto_nside` / `ref_pixel_scale_arcsec` 是本地参考实现（`:466-468` 注释自述"生产 compute_auto_nside 依赖 WcsSip + HealpixCore 链, 此处用参考实现"）。生产 `compute_auto_nside` **一行都没被调用**，却被报告为"验证算法正确性"。另外 `:502` 的 `210960.0` 是无出处硬编码（正确推导为 `sqrt(4π/12)·(180/π)·3600 ≈ 211077`，此处偏小约 0.06 %）。用例名声称覆盖 "WCS/**SIP** 尺度"（`:465`），但参考实现 `:489` 明确"无 SIP, 线性近似" ⇒ **标题超出实际覆盖**。
- `:1245-1255`（TEST 21）**空断言**：`:1251` 只断言 `query_pixel` 返回 0（成功码），`:1254-1255` 把 `sig_val`/`sup_val` **只打印不断言**；注释 `:1248` 自述"可能不在 parent_ipix=5 内"。⇒ 该用例声称的"NESTED ipix / Tile 父子恢复正确"**实际未被检验**。
- `:507-521`（DP-U6 确定性）**缺恒真守卫**：`p0` 为空时 `while(true)` 首次 `find` 即 `npos` 立刻 break，`all_eq` 保持 `true` ⇒ 判据退化恒真。而 `slurp`（`:86-91`）读文件失败**静默返回空串**。⚠️ 同一缺陷模式在同仓另一处已被识别并修复（`p1hips_tests_diag_prov.cpp:607-612` 的 BLD-401 注释），**此处未同步修复 ⇒ 守卫施加不一致**。
- `:618, 690, 764, 822, 920, 987, 1046, 1147` **注释与代码矛盾**：`size_t n_leaf = (size_t)16 /* 4^2=16 ... */;  // 3072` —— 注释尾标 3072，代码值是 16。
- `:841-884, 934-958, 1000-1018, 1062-1075` **未校验边界的 `memcpy`**：`file_data.data() + header_offset/json_len` 全部来自文件内容且无 `size()` 校验，越界即 UB/崩溃。
- `:1338` `return g_failures.empty() ? 0 : 1;` — 框架本身**不是**恒真门（做对了）。

### 3.3 `tests/p1hips/p1hips_tests_diag_prov.cpp`（986 行）— **需修**

**读了什么**：全 986 行。正向组 DP-U1..U6、负向组 DP-N1..N4、库级故障注入自检。

**看到什么**

- `:22-23` 声称"期望值由本 TU 的独立 oracle 计算…**不用被测函数生成期望值**"。**该声称部分为假**：`:156` 的 `dp_to_local` 调用**生产函数** `acsd::healpix::fits_index_to_nested_local`，其产物既作写侧**输入**（`:198-206`）又作期望基准。好在 `:372-408` 的 CFITSIO `TINT` 原始读回确实把**磁盘 FITS 序**钉死在独立期望上，这一路是真的独立。
- `:932-984` **自检机制是真的**：5 个注入点 × "基线必 PASS + 注入必 FAIL"双向。我核实了注入钩子**确实存在于生产代码**（`aio_hips_writer.cpp:137` `fault_injected`；`:709, 1750, 1772, 1962, 2480, 2668`）⇒ 判别力验证不是空话。**但见 §4-B2：钩子无编译期开关。**
- `:610-612` **做对的地方（可作正面样板）**：显式堵住了 `man.find("weight")==npos` 在 manifest 为空时的恒真。
- `:613-615` **守卫仍不完整**：只查 `!man.empty()`，非空但不含 `weight` 时仍恒真；对比 `:856-858` 的 `dp_scenario_full` **确实**先断言了 `acsd_model_hash` 存在才判 `weight` 缺席。⇒ **同一文件内两个"weight 缺席"判据强度不一致。**
- `:395-407` **`nrej_sum` 判别力守卫被符号转换吃掉**：`nrej_sum += (size_t)got[i];` 先转无符号，而 §30.2 恰恰禁止 −1 哨兵 ⇒ 若 −1 真的出现，它贡献 `SIZE_MAX`，`nrej_sum > 0` 依然成立，守卫失效。
- `:690` **硬编码与源解耦**：`bad.parent_ipix = 12ULL * (1ULL << (2ULL * 0));` 把指数写死为 0（隐含 `FIX_NSIDE == 1`）。改动 `FIX_NSIDE` 后该"越界"注入静默退化为合法值。
- `:967, 970` 使用 POSIX `::setenv/::unsetenv`；`:52` 引入的 `acsd_test_posix_compat.h`（已核实存在）只提供 `mkdir` 垫片，**未提供 `setenv`** ⇒ Windows 编译面存疑（登记为待前台编译验证项）。
- `:28` 悬空引用：`eng/ci/ctest_baseline.json` —— **`eng/ci/` 不存在**（`eng/` 实有 `build cmake contracts packaging README.md run tests tools`）。

### 3.4 `src/aio_xisf.cpp`（746 行）— **需修**

**读了什么**：全 746 行。XISF 魔数/头长/几何/位置解析、像素转换（FP32 与 FP64 双路）、metadata 构造、header-only 路径。

**看到什么**

- **`:383-387` 静默降级（本片最严重的科学数据完整性风险）**：
  ```cpp
  auto kw_float = [&](const char *name, double def = 0.0) -> double {
      const char *v = find_kw(name);
      if (!v || v[0] == '\0') return def;
      try { return std::stod(v); } catch (...) { return def; }
  };
  ```
  ⇒ **EXPTIME 拼错 ⇒ `cal.exptime = 0.0`，无错码、无告警、无标志位。** 我交叉核对本片 `hiss_correctness_test.cpp:293-295`：该链的 `k_init = t_light/t_dark` 正是靠 EXPTIME，缺失即 `k_init<=0` ⇒ `BAD_K_INIT`。**一个手误的 EXPTIME 会静默变成一个看起来合法的 0.0 曝光时间。**
  ⇒ 同理 `:397-400, 405-406` 的 CRPIX/CRVAL/CD 矩阵：CTYPE 拼对、CD 合法、CRPIX1 拼错 ⇒ `:414-416` 的 `has_wcs` 仍为 1 ⇒ **静默错误的天体测量解**。
  ⇒ `:431` GAIN 缺失 ⇒ `1.0`；`:433` BUNIT 缺失 ⇒ `"ADU"`；`:430` FILTER 缺失 ⇒ `"Unknown"`。全部无声。
- `:63-65` + `:174` **格式缺失静默兜底**：未知 `sampleFormat` 只发 WARN 后按 Float32 继续；`fmt.empty()` 直接 `= "Float32"`。⇒ UInt16 数据会被当成 Float32 读，`expected_bytes` 按 4 字节算，截断检查随之失去意义。
- **`:514` 与 `:700` 几何校验不对称**：`:514` 校验 `w<=0||h<=0||c<=0||w>65535||h>65535` —— **不校验 `c` 上界**；而 `:700`（header_only）用 `XISF_MAX_DIM` 校验了 `w/h/c` 三者。`:699` 注释自称"对齐 xisf_read_file 既有校验口径" ⇒ **注释声称的不变量与代码不符**。取 `geometry="100:100:1000000000"` 可让 `:497-498` 算出 `expected_bytes ≈ 4×10^13`，`:508` 通过，`:534` `std::vector<uint8_t> raw(read_size)` 尝试 ~40 TB 分配 ⇒ 未捕获的 `bad_alloc` 逃出返回 `int` 的函数边界。本文件 `:464-466` 恰恰为**另一个**分配（XML 头，64 MB）做了 `bad_alloc` 防护 —— **防护不对称**。
- **`:706` "只读头"却分配整幅像素缓冲**：`xisf_read_header_only` 调 `calloc(w*h*sizeof(float))`。在 `:700` 允许的最大几何（65535²）下 = **17 GB**。"header only"接口的语义被自身实现取消。
- `:69-77` **属性匹配无词边界**：`tag.find(attr_name + "=\"")` 会命中更长属性名的尾部（如 `unitName="X"` 被 `name` 命中）。潜在解析串扰。
- `:16-23` 自备 `aio_safe_copy`（注释明说为规避 `strncpy`），但 `:623`/`:716` 仍用 `strncpy(out->source_format, "xisf", sizeof(...)-1)`。我核实 `aio_fits.h:24` `source_format[16]`，此处**实际无害**，不作为缺陷计，仅记一致性。
- `:461-462, 488-491, 556-557` 大量 INFO 级日志（含每文件偏移/尺寸）⇒ 噪声面。

### 3.5 `src/aio_sparse_punch.h`（682 行）— **须修**

**读了什么**：全 682 行。打洞粒度/返回码/结果结构、字节级全零判据、NaN 位型辅助、卷能力探测缓存、扫描与提交、读回复算、强制打洞注入面。

**看到什么**

- **设计本体做得好**：`:14-20` 明令"本文件不出现任何浮点类型/常量/比较"，并给出 `-0.0` 位型 `0x80000000` 与 NaN `0x7FC00000` 的理由；`:116-122` 判据是纯字节比较。`:598-602` 用 `st_size` 不变 + `:604-608` 用 `alloc_after < alloc_before` 作独立判据 —— **这两条不是自洽断言**，是真实的外部测量，值得肯定。
- `:567-568` **`(void)aio_atomic::fsync_path(path, 0);` 返回值丢弃**。文件头 `:11` 把"打洞在 fsync 之后"写成硬约束，实现却是"尽力而为"，且失败无信号。
- `:676` **`(void)file_metrics(path, &size2, &r.alloc_after);` 返回值丢弃**。失败时 `alloc_after` 保留旧值 ⇒ 上报的"打洞后分配字节"可能是打洞前的数。
- `:364-432` **能力探测失败被归类为"不支持" ⇒ 静默降级**。探测要在目标目录**新建**探针文件（`:394-396`）；目录只读、配额满、SELinux/inotify 限制等**与打洞能力无关**的原因会导致 `fopen` 失败（`:396`）⇒ `why="probe_create_failed"`（`:424`）⇒ `sup=0` ⇒ `PUNCH_UNSUPPORTED`（`:85`）⇒ 调用方按"降级不 fail-closed"跳过。`reason` 可诊断，但**分类把"探测不了"与"卷不支持"合并了**。
- `:348-351, 426-429` **探测缓存无失效机制**（`reset_probe_cache` 仅供测试，`:434`）。卷在运行期重新挂载/上线后，"不支持"的结论将持续整轮生效。
- `:216-219` **Windows 侧 `errno_is_unsupported` 恒返回 `true`**（`:214-215` 有理由说明）⇒ Windows 上任何打洞错误都被静默降级为"不支持"，真实 EIO 不上报。
- `:166-171` **可变全局 `punch_enabled_ref()` 无同步**（`probe_cache` 有 mutex，这里没有）⇒ 并发读写 `bool` 是数据竞争。
- `:175-178` 又一个**无编译期开关的环境变量注入面** `ACSD_SPARSE_PUNCH_FAULT`。

### 3.6 `include/hiss_format.h`（621 行）— **须修（悬空引用）**

- `:342` 与 `:367-368` 是正本：`signal[p] = float(sumFlux)`，**不除面积**。据此判定 §3.2 中 TEST 6 注释为错。
- `:364` `double pixel_area = 1.0;` 注释"默认值 1.0 仅为向后兼容（旧调用未设置时退化为 sum_area 直接作为 S）"⇒ **参数缺失被静默吸收为另一个量纲**，且 `validate_support()` 无法区分"调用方忘了设"与"像素面积恰为 1 sr"。我核对本片全部 HISS 测试：`make_simple_accumulator` 及 12 处夹具**一律不设 `pixel_area`** ⇒ **冻结契约 `S = sum_area / A_p` 的真实归一化路径从未被测试覆盖**。
- **悬空引用（本片最严重的文档缺陷）**：`:7` `见 Wiki HISS-Container-and-Tiles.md`、`:17` `见 Wiki Stage1-Decision-Status.md` —— **全仓 `find` 均无此二文件**；`hiss_tile_model.h:8` `02_FROZEN_STAGE1_HISS_SPEC.md` 同样不存在。而 `hiss_format.h` 通篇以 "02_FROZEN §8/§11/§13/§16/§17"、"00_COMMON_CONTRACTS §2.1/§2.2/§2.5/§3.3" 为权威引用，**这两份正本在仓内一处都找不到**（`docs/detail/PHASE1_DETAILED_DESIGN.md` 中二者出现次数 = 0，已 grep）。⇒ 与 AGENTS.md §3「同一主题只有一份正本；双向可追溯」直接冲突。

### 3.7 `runtime/artifact_store/artifact_manifest_validator.py`（287 行）— **须修**

- **`:44` vs `:12`**：见 §2-2，已构造反例并实测推翻（`c:/data/x.fits` 被接受）。
- **`:69-70, 136` 未捕获的异常逃出验证器**：`load_registry()` 直接 `REGISTRY_PATH.read_text()`；`self._registry()` 在 `:136` 被调用，但 `validate_text` 的 `try/except Exception`（`:93-95`）**只包住 `load_strict_json`**。⇒ 注册表缺失/损坏时抛 `FileNotFoundError`，穿透 `validate_doc → validate_text → validate_file → main`。验证器应以"判负"终结，却以 traceback 终结。
- `:22` 引用的 `eng/tests/artifact/` **确实存在**（已核实）—— 这条悬空引用为假，正面记录。
- `:44` 第二分支还接受 `run:abc123` 等任意 `scheme:token`，与"必须是 URI 形态"的自述宽泛不符。
- `:46` `_UTC_RE` 仅做格式匹配，不做日历校验（`2026-13-45T99:99:99Z` 通过）。
- `:259` `input_digests[].digest` 用**裸 64-hex 串**，而 `content_digest`/`config_digest`（`:179-190`）用 `{algorithm, hex}` 对象 ⇒ 同一文件内摘要形态不一致，输入摘要隐式绑定 sha256 而未声明。
- `:34-37` `REPO = parents[5]` 经核算正确（`artifact_store→runtime→aio→infrastructure→lib→root`）。
- 正面：`:40-48` 词法、`:51-66` 拒绝 NaN/重复 key、`:113` `additionalProperties:false`、`:117` 缺字段即 return，都是 fail-closed 的正确写法。

### 3.8 `include/aio_pipeline.h`（348 行）— **须修**

- `:310` `weight` 仍为标准块；`:340, 342` 生命周期表仍含 `weight` ⇒ 见 §2-1。
- `:30-31`、`:227`、`:262` **三处**引用机器判据 `eng/ci/check_aio_io_boundary.py` ⇒ **该目录/文件不存在**（`find` 仅命中 `run/**` 归档副本）。`:262-263` 断言"仓内生产调用点 = 0"—— 就 `aio_pipeline_export_xml` 而言我 grep 证实为真（仅定义 + `aio_pipeline.cpp:1374` 内部转发），但**该断言所依赖的自动检查器并不存在**。
- `:64-65` 硬编码上限 `AIO_CACHE_MAX_BLOCK_BYTES (1LL<<32)` / `AIO_CACHE_MAX_FRAME_BYTES (1LL<<34)`（`:60` 标注 BLOCKER-DF-001）属"防损坏输入"的**冻结资源上限**，有出处标注，可接受。
- `:109-110` 保留已废弃 `PipelineStageFn` 别名 —— 项目规范要求退役代码**删除**或统一注释块保留，此处有注释，可接受。

### 3.9 `include/aio_pipeline_engine.h`（99 行）— **建议**

- `:79-80` **`n_threads` 默认 16，`<=0` 一律回落到 16** ⇒ 无科学推导、无配置入口、无自适应，按项目硬编码规则应移入配置。
- `:82` 明示"若 `to_stage` 包含 `STAGE_STACK`, 引擎会在所有帧并行完成后串行执行" ⇒ 该引擎对外提供**自有的批量并行执行器**。项目规范"池的所有权归调度器"，本文件**没有任何关于池归属的声明**。是否构成第 10 例"私建线程池"需负责人裁决（见 §4-B6）；本片 `grep` 线程原语命中 0 处，故我**不**在本片断言其已建池。
- `:65` 注释 `frame: 输入帧 (已填充 pixel_data 等)` —— `PipelineFrame` 无 `pixel_pixel` 成员（`:82-86` 只有 blocks/n_blocks/blocks_capacity/stages_completed）⇒ 注释所述"pixel_data"与数据块模型不符，属过时注释。

### 3.10 `product_io/include/astro/aio/atomic_publish.h`（168 行）— **须修（自洽判据）**

- **`:118-122` 是本片最标准的"自洽式断言"**：
  ```cpp
  // P-174 终态自洽判据：存在正式产品（kDurable | kNotDurable）⇔ renamed。
  inline bool publish_result_consistent(const PublishResult& r) {
    const bool published = r.durability != PublishDurability::kNotPublished;
    return r.renamed == published;
  }
  ```
  文件头 `:27` 自称"自洽判据"。但 `renamed` 与 `durability` 是**同一控制流赋出的两个字段**，此式不与任何外部事实（文件系统、持久化）对照 ⇒ 除赋值代码自身写错外**不可能失败**。**这正是负责人点名的"同一个定义式既当被检量又当期望量"。**
- `:112` `bool tmp_residue = false; // 失败后 tmp 是否残留（**必须恒为 false**）` ⇒ 注释自陈恒定量的字段，不是判据。
- `:1-35` 文件头三态（`kDurable`/`kNotDurable`/`kNotPublished`）设计与 `:8-9` 的"rename → 重开验证 → 失败即撤销"次序**写得比同仓其它发布面严谨**，正面记录。
- `:33-34` 声明与 `lib/algorithms/drizzle/hips/.../publish.h`、`lib/include/acsd/io/aio_abi_v1.h` 数值对齐并有 `static_assert`（`:156-163`）—— 但**该 static_assert 只钉了 7 个值**（0,1,4,7,13,70,71），`:47-66` 共 17 个枚举值 ⇒ 另有 10 个值（2,3,5,6,8,9,10,11,12,14,15）**未被编译期钉住**。

### 3.11 `tests/hips_robust_sanitize_driver.cpp`（223 行）— **建议**

- `:62, 175` 损坏注入失败 ⇒ `return 7`，**fail-closed 做得好**。
- `:26-63` `corrupt_snr_tsv` 用 `fs::recursive_directory_iterator(snr)`（`:30`），若 `out/snr` 不存在则构造函数抛异常且未捕获。对 sanitizer 驱动可接受（它就是要抓崩溃），不单列缺陷。
- `:34-38` `ACSD_KEEP_ORIG` 未设时**直接破坏原 TSV 且不留备份**。驱动作用于临时 out_dir，可接受。
- `:99-100` `a_cell = 4π/(12·nside²)` —— **科学推导正确**（HEALPix 像素立体角），正面记录。
- `:109-114` 有效区为 `x∈(100,400), y∈(100,400)` 的实心方块 ⇒ 单一连通分量、单一覆盖比 `0.5`；对"robust"驱动的覆盖面偏薄（建议），非缺陷。

### 3.12 `tests/test_report.md`（127 行）— **须修**

- `:86-104` 3 项失败被归因为"测试期望值错误，算法行为正确"；`:123` 完成标准写"**已完成**（368/371 通过，3 失败为测试期望问题）" ⇒ **失败套件被标记为已完成**。
- `:47-49` "无软通过 / 所有通过项均为真实断言通过" —— **被我读到的 `hiss_correctness_test.cpp:430-434` 直接证伪**。
- `:90-93` [TEST 7] 记录"期望 200, 实际 120"，而当前源码 `hiss_correctness_test.cpp:361` 断言 120 ⇒ **测试被改成与实现一致后才转绿**，且未留下任何独立推导。
- `:95-98` [TEST 12] 自陈"Reader 对 FULL 模式的数组展开逻辑需与 Writer 的存储方式对齐（**已知工程问题, 记录待修复**）"⇒ 真实缺陷以"已知问题"形式留存，却仍计入"已完成"。
- `:3` 报告生成时间 2026-07-31，`:127` "代码已 commit | **待执行**" ⇒ 陈旧报告滞留于生产树；其"模块构建结果/单元测试结果"对当前 HEAD 无参考价值。
- `:18-27` 源文件清单与当前 `build.ps1` 的实际源集合**不一致的嫌疑高**（`build.ps1` 未由我读完，登记为缺口）。

### 3.13 `src/hiss_tile_model.h`（105 行）— **建议（悬空引用）**

- `:8` `02_FROZEN_STAGE1_HISS_SPEC.md` **不存在**。`:9` `lib/infrastructure/aio/docs/HEALPIX_FORMAT_SPEC.md`、`:10` `docs/science/algorithms/HIPS_WRITER.md` **存在**（已核实）。
- `:22` `全局 ipix = (parent_ipix << 2d) | local_ipix` 与 `hiss_correctness_test.cpp:1194-1196` 的 `shift = 2*(log2 64 - log2 16) = 4` **一致**，无矛盾。
- `:61-64` `global_to_local` 注释自陈"仅提取低位, **不校验**该 global_ipix 是否属于本 Tile" ⇒ 显式声明的边界，可接受。
- `:12` "核心公式 (冻结, **子代理不得修改**)" —— 公式正文正确（`d=min(9,log2(NSIDE/16))`，`n_leaf=4^d`），且 `:17-19` 明确标注修正了旧的 `tile_nside^2*12` 错误。**正面记录**（该修正与 `hiss_correctness_test.cpp:618` 等处残留的 `// 3072` 陈旧注释形成对比，后者是我 §3.2 的缺陷项）。

### 3.14 `src/ahpx/aio_ahpx_writer.h`（88 行）— **通过**

- `:16-17` 明确"只写 pixel 与 snr 两类块, **不写任何权重**（权重是阶段二现场算的派生量）"；`:39-40` 明确"携带 `weight` 字段 ⇒ `write()` 显式拒绝（不产出读侧必拒的文件）"。这是**正确的 fail-closed**，与 §2-1 的管线层遗漏形成对照 —— 即退役在容器层做对了，在管线层没做。
- `:29` `int zstdLevel = 5;` 硬编码压缩级别，**无推导、无配置入口**（AGENTS.md §6：需标定的落配置）。建议。

### 3.15 `src/aio_fits.h`（45 行）— **通过**

- `:7-14` FP32/FP64 双模 ABI 约束（互斥、必须由 `aio_free_image_data` 一并释放）写清。
- `:31-40` `FITSHeader` 的 `bitpix`/`naxis1..3`/`bscale`/`bzero` **无初始化器**；作为 C++ 结构体若被 `malloc` 后直接使用即读到未定义值。仅作建议（其构造点在片外）。

### 3.16 `healpix_db/.../legacy/healpix_stack/stack_db.h`（78 行）— **建议**

- `#include "ahps_reader.h"` / `"ahps_writer.h"` —— 两文件**确实存在**（`ls` 核实），无悬空引用。
- `:20-27` 硬编码：`nsideData = 32768`、`nsideLod = {512,2048,8192,32768}`、`tileNside = 512`、`sigmaClipLow/High = 3.0`。**3σ 无出处说明、无标定依据、无配置入口** ⇒ 命中项目"无依据经验值改为自适应"。整个头文件位于 `archive/legacy/` 且无任何"退役原因"统一注释块（AGENTS.md §6 要求）。
- 退役声明核验：本片**未发现**该文件自称"零消费者"的注释，故不构成"退役对象仍有活调用者"的实例；但它**确实**违反"退役代码须删除或统一注释块写明原因"。

### 3.17 `product_io/README.md`（73 行）— **须修（文档与代码不符）**

- §3 给出的构建命令 `cmake -S eng/tests/unit/aio -B build/aio` —— 我 `ls` 核实 `eng/tests/unit/aio/` 仅含 `aio_test.cpp`、`atomic_durability_probe.cpp`、`check_atomic_durability.py`，**无 `CMakeLists.txt`** ⇒ 该命令不可执行。
- §1 表格把路径写作 `lib/include/astro/aio/...`，实际为 `lib/infrastructure/aio/product_io/include/astro/aio/...` ⇒ 表格内路径**均不可解析**；与文档开头自述的"写域"自相矛盾。
- §1 表格**漏列**实际存在的 `include/astro/aio/validation.h`。
- §3"外部真值（Oracle 独立性）"声称用 `astropy.io.fits verify_datasum/verify_checksum` 做 FITS 校验和双真值交叉 —— **该独立 oracle 属 `product_io` 面**。本片 `io/fits_core.c` 是**另一套** FITS 实现，其 DATASUM/CHECKSUM **没有任何独立 oracle**（见 §3.1 与 §4-B1）。两套实现并存而只有一套被交叉验证。

### 3.18 `tests/gaia_sanitize_driver.c`（33 行）— **建议**

- `:20-23` 若 `rc==0` 且 `n>0` 而 `stars==NULL` ⇒ `:22` 解引用崩溃。对 sanitizer 驱动而言这正是要抓的形态，不单列缺陷。
- `:17-19` 魔数坐标 `(272.908258995422, -23.5926042853775)`、`(10.0, 89.9)` 无出处注释（后者显然是"极区"探针）。建议补出处。

### 3.19 `aio_build_config.healpix.json`（11 行）— **通过**

- 8 个键（`enable_fits/xisf/ahpx/healpix/compressor/pipeline/zstd/lz4`）**全部被 `build.ps1:55-75` 真实读取**并映射为 `-DAIO_ENABLE_*` / `-DHAS_*` 宏 ⇒ **无死键、无未读键**。（我未读完 `build.ps1` 全文，故仅就 grep 命中的 55-75、100-102 行结论负责。）
- `_comment` 自述"无 XISF/ahpx"，与 `enable_xisf:false`/`enable_ahpx:false` 一致。

### 3.20 `src/aio_log.h`（10 行）— **建议**

- **无 `AIO_EXPORT` / 可见性属性**，而同模块 `aio_pipeline.h:7-11`、`hiss_format.h:28-40` 都有。`aio_xisf.cpp:2` include 它且跨 DLL 调用 `aio_log`。Windows 下若无其它导出途径将链接失败。登记为待前台编译验证项。

### 3.21 `build.ps1` 相关（**未读完**）— 见 §1.2

`build.ps1`(204) 与 legacy `build.ps1`(151) **我未亲自读完**，仅对上述 grep 命中的行负责。不作判定。

---

## 4. 发现清单

### 4.A 阻断（BLOCKER）

**无。** 本片未发现导致数据销毁、越界写、或必然错误发布的缺陷。

### 4.B 须修（MUST-FIX）

| # | 严重度 | 位置 | 缺陷 | 后果 |
|---|---|---|---|---|
| **B1** | 须修·最重 | `aio_pipeline.h:310,340,342` + `aio_pipeline.cpp:138` + `aio_pipeline_engine.cpp:116,120` + `test_pipeline_blocks.py:390-419`；判据缺失于 `aio_pipeline.h:30,227,262` | `weight` 已在 ahpx 容器层退役（`aio_ahpx_format.h:33`、`aio_ahpx_writer.cpp:269`）与最高设计 §2.1 声明退役，**但仍是管线一等标准块**，含活代码与活测试；用以证明"生产调用点=0"的 `eng/ci/check_aio_io_boundary.py` **不存在** | 退役声明不完整；下游仍可产出/消费权重块。**与负责人已举证的那例失效模式同构** |
| **B2** | 须修 | `aio_xisf.cpp:383-387` | `kw_float` 对缺失/不可解析的关键字一律回落 `def`：**EXPTIME→0.0**、CRPIX/CRVAL→0、GAIN→1.0、BUNIT→"ADU"，无错码无告警 | 科学数据完整性 fail-open。EXPTIME 手误 ⇒ 合法外观的 0.0 曝光 ⇒ 下游 `k_init=t_l/t_d` 归零。CRPIX 手误 ⇒ `has_wcs=1` 但天体测量错 |
| **B3** | 须修 | `artifact_manifest_validator.py:44` vs `:12` | `_STORAGE_URI_RE` 接受 `c:/data/x.fits`；docstring 自称"Windows 盘符路径拒绝" | 自称存在的检查并不存在（**已在 /tmp 实测推翻**） |
| **B4** | 须修 | `atomic_publish.h:118-122` | `publish_result_consistent()` 以同一控制流赋出的 `renamed` 与 `durability` 互校，不与任何外部事实对照 | **典型自洽式断言**：除赋值写错外不可能失败；`:27` 却称之为"自洽判据" |
| **B5** | 须修 | `test_report.md:86-104,123` + `hiss_correctness_test.cpp:430-434,317-334` | 失败被判为"测试错、算法对"并标记完成；"无软通过"声称被同文件恒真断言证伪；TEST 6 因 `sum_area=1.0` 对"除/不除面积"零判别力，注释与冻结合同 `hiss_format.h:342` 矛盾 | 以报告代替论证；测试被改成与实现一致后转绿（自愈判据） |
| **B6** | 须修 | `fits_core.c:684-703, 1106-1179` | `fio_hdu_checksum_stream` 的 seek 失败 `return 0`、短读 `break`，均无错码；`fio_write_header_block` 显式 `(void)err;(void)cap;` 且从不检查 `fio_card_write` 返回 | 静默降级 + 静默发布损坏 FITS（`verify_before_rename=0` 时） |
| **B7** | 须修 | `aio_hips_writer.cpp:137,709,1750,1772,1962,2480,2668`；`aio_sparse_punch.h:175-178` | 6+ 个环境变量故障注入钩子**无条件编译进生产库**，其中 `:2668` 使 `aio_hips_verify_product_set` 直接 `return 0` | 生产二进制可被单个环境变量把校验器变成橡皮章。**注：该文件不在本片，我仅核实钩子存在；建议负责人单独立项** |
| **B8** | 须修 | `hiss_format.h:7,17` + `hiss_tile_model.h:8` | "Wiki HISS-Container-and-Tiles.md"、"Wiki Stage1-Decision-Status.md"、"02_FROZEN_STAGE1_HISS_SPEC.md" 全仓不存在；`02_FROZEN`/`00_COMMON_CONTRACTS` 在仓内出现次数=0 | 冻结正本缺失，AGENTS.md §3「双向可追溯」断裂 |
| **B9** | 须修 | `p1hips_tests_diag_prov.cpp:28`；`aio_sparse_punch.h:11,33` | `eng/ci/ctest_baseline.json`、`ENGINEERING_SPEC.md §11`、`ACCEPTANCE_SPEC.md §3.2` 均不存在于权威树 | 测试以"CTEST-REGISTRATION 闭包不变"为由免注册的依据文件不存在 |
| **B10** | 须修 | `product_io/README.md` §1/§3 | `cmake -S eng/tests/unit/aio` 无 `CMakeLists.txt`；表内路径 `lib/include/...` 全不可解析；漏列 `validation.h` | 文档描述的构建与文件布局均不可执行 |

### 4.C 建议（SUGGESTION）

| # | 位置 | 建议 |
|---|---|---|
| S1 | `fits_core.c:376-386, 898-915, 1218-1225, 1494` | 溢出显式报错而非返回 0；`rename` 前按 `overwrite` 分支（POSIX `renameat2`/Windows `MoveFileExW(MOVEFILE_REPLACE_EXISTING)`）；`remove` 返回值处理 |
| S2 | `aio_xisf.cpp:63-65,174,514,700,706,79-89` | 未知/缺失 `sampleFormat` 硬失败；`c` 上界与 header_only 对齐；header_only 不分配像素缓冲；`get_attr` 加词边界 |
| S3 | `aio_sparse_punch.h:567-568,676,216-219,166-171,364-432` | fsync/stat 返回值处理；探测失败与"不支持"分类分离；缓存失效策略；`punch_enabled` 加同步 |
| S4 | `hiss_format.h:364` | `pixel_area` 缺省不得退化为另一量纲；应有显式"未设置"态 |
| S5 | `atomic_publish.h:156-163` | `static_assert` 只钉 7/17 个枚举值，补齐 |
| S6 | `aio_pipeline_engine.h:79-80` | `n_threads` 默认 16 移入配置；补池归属声明（与负责人"私建线程池"清单对齐） |
| S7 | `aio_pipeline.h:64-65`；`aio_pipeline_engine.h:65` | 资源上限与"pixel_data"注释需与数据块模型对齐 |
| S8 | `hiss_correctness_test.cpp:618,690,764,822,920,987,1046,1147` | 清除 `// 3072` 等陈旧注释 |
| S9 | `hiss_correctness_test.cpp:496-513` | 测试应调用生产 `compute_auto_nside`，或删除"验证算法正确性"的措辞 |
| S10 | `hiss_correctness_test.cpp:1245-1255` | 补 `query_pixel` 数值断言，否则该用例无覆盖 |
| S11 | `p1hips_tests_diag_prov.cpp:395-407, 613-615, 690, 610-612` | `nrej_sum` 用有符号累加；两处 weight 判据强度对齐；`1ULL<<(2ULL*0)` 改为由 `FIX_NSIDE` 推导 |
| S12 | `product_io/README.md` §3 | 明确 `io/fits_core.c` 属**无独立 oracle** 的第二套 FITS 实现 |
| S13 | `stack_db.h:20-27` | 3σ 与 nside 层级移入配置；补退役原因统一注释块 |
| S14 | `aio_ahpx_writer.h:29` | `zstdLevel=5` 移入配置 |
| S15 | `artifact_manifest_validator.py:69-70,136` | 注册表加载失败应产出判负错误而非抛异常 |
| S16 | `artifact_manifest_validator.py:46,259` | `_UTC_RE` 补日历校验；输入摘要形态与 content 摘要统一 |
| S17 | `aio_log.h` | 补 `AIO_EXPORT`（待前台编译验证） |
| S18 | `p1hips_tests_diag_prov.cpp:967,970` | `setenv` 需 Windows 垫片（待前台编译验证） |
| S19 | `test_report.md:3,127` | 陈旧报告（含"commit 待执行"）应移出生产树 |

### 4.D 待裁决（UNRESOLVED）

| # | 事项 |
|---|---|
| U1 | **清单基线与实际 HEAD 不符**：清单声明 `850a9ede`，实测 `1fa477a7`。本片结论按工作树实际内容取证。是否需重新划基线，请负责人裁决。 |
| U2 | **§4.B7 的生产故障钩子**是否构成"生产可被环境变量关闭校验"的阻断项 —— `aio_hips_writer.cpp` 不在本片，我只核实了钩子存在，未审其调用上下文。 |
| U3 | **`aio_pipeline_engine_run_batch` 是否构成第 10 例"私建线程池"** —— 本片 grep 线程原语命中 0，接口层有 `n_threads` 但无池归属声明。 |
| U4 | **`hiss_correctness_test.cpp` TEST 7 的语义改写**是否经科学裁决：文件头 `:6` 把该组命名为"通量守恒"，现断言却是"累计通量、不除面积"。属科学量定义变更，应走 AGENTS.md §4 的一手证据三源互证，而非由测试失败报告裁定。 |

---

## 5. 我主动构造的反例

### C1 —— 推翻"hiss_correctness_test 的 Drizzle 组验证了通量守恒"

- **构造什么**：取 TEST 6 的夹具 `sum_flux = 100×1.0 = 100`、`sum_area = 1.0`（`hiss_correctness_test.cpp:325-326`），分别按两种语义求 `signal[0]`：不除面积 → `100/1 = 100`；除面积 → `100/1 = 100`。
- **期望推翻什么**：期望证明 TEST 6（标题"单像素通量守恒"）**无法区分** `finalize_signal` 是否除以面积，即它对它自己命名的那个科学语义**判别力为零**。
- **是否推翻**：**推翻成功。** 两种假设给出同一数值，断言 `signal[0]==100` 在 `hiss_format.h:367-368` 的任一实现下都成立。对照组 TEST 7（`:351-354`，`sum_area=0.6`）给出 `120` vs `200`，**才有判别力**。⇒ 本组对"是否除面积"的覆盖**全部来自 TEST 7 单一用例**，而 TEST 7 的期望值是被 `test_report.md:90-93` 记录为"测试错、改成 120"之后定下的。**结论：该组"通量守恒"的覆盖建立在一个由失败报告单方面裁定、并与冻结正本 `hiss_format.h:342` 方向相反的期望值上。**

### C2 —— 推翻"artifact_manifest_validator 拒绝 Windows 盘符路径"

- **构造什么**：在 `/tmp/re_check.py` 中**逐字**复制 `artifact_manifest_validator.py:44` 的 `_STORAGE_URI_RE`，喂入 `c:/data/x.fits`、`c:/Users/foo/bar.fits`、`d:/prod/2026/run1/manifest.json`。
- **期望推翻什么**：期望证明 docstring `:12` 的"Windows 盘符路径拒绝"是**自我声明而非实现**。
- **是否推翻**：**推翻成功（实测）。**
  ```
  ACCEPTED  'c:/data/x.fits'
  ACCEPTED  'c:/Users/foo/bar.fits'
  ACCEPTED  'd:/prod/2026/run1/manifest.json'
  rejected  'c:\\data\\x.fits'
  ```
  小写盘符 + 正斜杠的 Windows 绝对路径**被接受**。反向盘符（`C:/`）因 `[a-z]` 而被拒，斜杠形式 `C:\` 因 `\` 不在字符类而被拒 ⇒ 该"检查"只挡住两种写法中的两种，恰好不影响最常见的形态。

### C3 —— 推翻"test_report.md 的『所有通过项均为真实断言通过』"

- **构造什么**：在 `hiss_correctness_test.cpp` 中寻找结构上不可能失败的断言。
- **期望推翻什么**：期望找到一条"形式上是断言、实质上恒真"的检查，从而证伪报告 `:47-49` 的无软通过声称。
- **是否推翻**：**推翻成功。** `:430-434`
  ```cpp
  for (auto s : support) { if (s > 255) out_of_range++; }   // s 是 uint8_t
  ASSERT_TRUE(out_of_range == 0, "所有 support 值在 [0, 255] 范围内");
  ```
  `uint8_t` 提升为 `int` 后上界恒为 255，`s > 255` **恒假**，`out_of_range` **恒为 0**。注释甚至写着"uint8 不可能 >255, 但检查逻辑完整性"。⇒ 报告所审计的正是含有此类条目的文件，其"所有通过项均为真实断言通过"**可被直接证伪**。

### C4 —— 推翻"`weight` 已完成退役"

- **构造什么**：以 `grep -rn '"weight"'` 全仓检索，寻找仍活着的注册与消费点；再核对三处被引为"机器判据"的检查器是否存在。
- **期望推翻什么**：期望证明最高设计 §2.1 与 AHPX-WEIGHT-RETIRE 所声称的"全程只有 SNR、不存在权重模式"在管线层未落实，且证明该退役的自动判据不存在。
- **是否推翻**：**推翻成功。** 活引用：`aio_pipeline.cpp:138`（标准块名表）、`aio_pipeline_engine.cpp:116,120`（块丢弃策略）、`tests/test_pipeline_blocks.py:390-419`（活测试）。判据缺失：`aio_pipeline.h:30,227,262` 三处引用 `eng/ci/check_aio_io_boundary.py`，而 **`eng/ci/` 目录不存在**（`find` 仅在 `run/**` 归档副本中命中）。⇒ **退役只在 ahpx 容器层落实；管线层原封不动；用来发现这一点的检查器是空的。**

### C5 —— 推翻"hiss_format 的冻结正本可在仓内追溯"

- **构造什么**：`test -e` + `find -iname` 逐一验证 `hiss_format.h:7,17`、`hiss_tile_model.h:8` 引用的文件名；再 `grep -c '02_FROZEN\|00_COMMON_CONTRACTS' docs/detail/PHASE1_DETAILED_DESIGN.md`。
- **期望推翻什么**：期望证明 HISS 格式的权威正本在仓内缺失，AGENTS.md §3 的双向可追溯断裂。
- **是否推翻**：**推翻成功。** `HISS-Container-and-Tiles.md`、`Stage1-Decision-Status.md`、`02_FROZEN_STAGE1_HISS_SPEC.md` 全仓 `find` 零命中；`docs/detail/PHASE1_DETAILED_DESIGN.md` 中 `02_FROZEN`/`00_COMMON_CONTRACTS` 出现次数 **= 0**。而 `hiss_format.h` 通篇以二者为权威引用。**唯一查证为真并存在的引用是** `lib/infrastructure/aio/docs/HEALPIX_FORMAT_SPEC.md`、`docs/science/DATA_SEMANTICS.md`、`docs/engineering/HIPS_STORAGE_FORM_CONTRACT.md`、`docs/detail/PRODUCT_STORAGE_FORM.md`、`docs/ACSD_DESIGN.md`、`eng/contracts/data/phase2_uncertainty_rejection_provenance_v1.json`、`eng/tests/support/acsd_test_posix_compat.h`、`eng/tests/artifact/`。

### C6 —— 反证尝试（**未能推翻**，记录以示公允）

- **尝试**：`hiss_correctness_test.cpp:496-513` 的 `ref_compute_auto_nside` 是否真的完全脱离生产 ⇒ 尝试找生产侧调用点以证明"测试至少间接覆盖了生产"。
- **结果**：**未能推翻我的结论。** 该函数在测试内定义、本地调用；测试文件 `:466-468` 自述"生产 compute_auto_nside 依赖 WcsSip + HealpixCore 链, 此处用参考实现"。生产 `compute_auto_nside` 不在本片，我**不能**断言生产实现本身有错 —— 只能断言**本片证据不足以支持生产正确**。
- 同理，`atomic_publish.h` 的三态设计（`:8-9`）与 `aio_sparse_punch.h` 的字节级判据（`:116-122`）经复核**确属严谨**，我**撤销**了"这两处是自洽断言"的初判，并在 §3 中记为正面项。

---

## 6. 盲复算

**方法**：对 §4.B 的每条发现，遮蔽我自己的结论文字，**只依据源码原文**重新独立推导一遍，再比对是否一致。

| 发现 | 盲复算独立取证 | 结论 |
|---|---|---|
| B1 weight 未退役 | 只看 `aio_pipeline.h` + `aio_ahpx_writer.h`：前者表内含 `weight`（:310）且生命周期表仍丢它（:340,342）；后者称"不写任何权重"（:16-17）。二者并存 ⇒ 容器层已退役、管线层未退役。 | **一致（不偏松）** |
| B2 kw_float 静默降级 | 只看 `aio_xisf.cpp:383-387`：`!v \|\| v[0]=='\0'` → `return def`；`catch(...)` → `return def`。两处均无日志、无错码。 | **一致（不偏松）** |
| B3 storage_uri | 只看 `:44` 正则与 `:12` 自述：`^[a-z][a-z0-9]*:[A-Za-z0-9._/+-]+$` 匹配 `c:/...`。 | **一致（不偏松）** |
| B4 publish_result_consistent | 只看 `:118-122`：两字段同源于一次发布流程，判据不引用文件系统。 | **一致（不偏松）** |
| B5 报告与恒真断言 | 只看 `hiss_correctness_test.cpp:430-434` 与 `test_report.md:47-49`：一为 `uint8_t` 范围检查恒假；一为"所有通过项均真实"。 | **一致（不偏松）** |
| B6 fits_core 静默 | 只看 `:691`（`return 0`）、`:698`（`break`）、`:1119`（`void` 化）。 | **一致（不偏松）** |
| B8/B9 悬空引用 | 只跑 `test -e` / `find`。 | **一致** |
| B10 README | 只跑 `ls eng/tests/unit/aio` 与 `ls .../product_io/include/astro/aio/`。 | **一致** |

**总体：判一致，无偏松、无偏严。**

⚠️ **两处我主动下调了严重度（记录以示公允）**：
1. `aio_xisf.cpp:623/716` 的 `strncpy` —— 初判为未终止风险，复核 `aio_fits.h:24` `source_format[16]` 后**撤销**，不计缺陷。
2. `hiss_correctness_test.cpp:507-521` 的 DP-U6 恒真 —— 初判为可达缺陷，复核后确认 `p0` 由 `r0==0` 成功的写入路径产生，实际为空的可能性低，**降级为 S11**（守卫施加不一致），不计须修。

⚠️ **一处我未能盲复算**：`io/fits_core.c` 的 DATASUM/CHECKSUM **是否真的符合 FITS/CFITSIO 约定**。我只证明了"写侧与验侧共用 `fio_dsum_*` 且 encode/decode 互逆 ⇒ **该验证器对约定性错误不敏感**"（自洽 oracle 成立）；**未能证明该约定本身对或错**（需 CFITSIO/astropy 交叉比对，已派 C2 子代理，见 §7）。

---

## 7. 子代理派发记录

### 7.1 派发明细

按"覆盖 §1.2 的 8 份未读完文件 + 对我已读文件做独立复核"的原则，派发 **5 个**（编号 C1–C5；**注意与 §5 的反例编号无关**）：

| 代理 | 范围 | 覆盖的未读文件 |
|---|---|---|
| **C1** | `io/fits_core.c` 深度对抗：DATASUM/CHECKSUM 自洽 oracle、encode/decode 逆对、静默降级、溢出、`rename` 跨平台 | `fits_core.c`（复核我已读部分） |
| **C2** | `src/` 生产模块：`aio_xisf.cpp` `aio_sparse_punch.h` `ahpx/aio_ahpx_writer.cpp/.h` `aio_fits.h` `aio_log.h` `hiss_tile_model.h` | `aio_ahpx_writer.cpp` |
| **C3** | 测试面：`p1hips_tests_diag_prov.cpp` `test_snr_unknown_block.cpp` `hips_robust_sanitize_driver.cpp` `gaia_sanitize_driver.c` `test_report.md` | `test_snr_unknown_block.cpp` |
| **C4** | 退役/遗留面：`hp_stack_hiss.cpp` `stack_db.h` `tests/test_hp_stack_hiss.py` legacy `build.ps1` `healpix_db/memory.md` —— 重点：退役声明真伪 grep、遗留树与生产的常量分叉 | `hp_stack_hiss.cpp`、`test_hp_stack_hiss.py`、legacy `build.ps1`、`memory.md` |
| **C5** | 管线/校验面：`aio_pipeline.h` `aio_pipeline_engine.h` `atomic_publish.h` `artifact_manifest_validator.py` `README.md` `product_io/README.md` `aio_build_config.healpix.json` —— 重点：validator 的 fail-open 面、原子发布语义、文档与代码一致性 | `README.md` |

### 7.2 复核方式

- 每份派单均**预先列出我对该文件的 8 类假设**（静默降级 / 自愈判据 / 恒红门 / 恒真门 / 筛掉真信号 / 退役活调用者 / 悬空引用 / 私建线程池 / 硬编码 / 错误码 / 数值稳定性），要求**逐条标注 CONFIRMED / REFUTED / PARTIALLY** 并附证据，且明确要求"**不假设我是对的**"。
- C1、C4 额外要求：凡见"退役 / 无消费者 / 不再使用"字样，**必须实跑 grep 并贴出命令与结果**，不得采信注释。
- C5 额外要求：对 validator 输出"检查 → 拒绝什么 → 放过什么"的表。

### 7.3 交付时刻的诚实状态

⚠️ **在本次交付写出时，5 个子代理的结果均尚未回传。**

因此：
- 本报告 §3–§6 的**全部结论均只来自我本人对 20 份文件的逐行阅读**，**未引用任何子代理产出**；
- **§7 的"否决了哪些"一栏无法填写** —— 我不会在没有收到结果的情况下编造复核记录；
- §1.2 列出的 8 份未读完文件，其结论**尚未产出**，登记为**第 2 遍的必补项**。

**这是一份覆盖不完整的交付**，请负责人在汇总时据此安排第 2 遍优先补齐 `aio_ahpx_writer.cpp`(494) 与 `build.ps1`(204) 两个生产面文件，以及 C1 的 DATASUM/CHECKSUM 外部真值比对。

---

## 8. 自证段（可复跑命令）

> 全部只读。⛔ 未执行任何 `add/commit/checkout/reset/stash/rm --cached`；⛔ 未编译、未跑 ctest/pytest/构建/任何项目二进制；⛔ 未读取 `/tmp/acsd_g08/`；⛔ 未修改仓内任何文件（唯一写入为本交付件）。

```bash
cd "/workspace/Astro CS Database"

# S0 基线与分母（口径：wc -l，合计应 = 10630）
git rev-parse HEAD          # 实测 1fa477a7…，与清单声明的 850a9ede 不符（§4-D U1）
sed -n '3054,3089p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml
while IFS= read -r f; do printf "%6s  %s\n" "$(wc -l < "$f")" "$f"; done < <(
  sed -n '3062,3089p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml | sed 's/.*"\(.*\)"/\1/')
# 手工求和 = 10630，与清单 实际行数: 10630 一致

# S1 C2 反例：storage_uri 正则（逐字复制自 validator:44）
cat > /tmp/re_check.py <<'EOF'
import re
_STORAGE_URI_RE = re.compile(r"^(file|https?)://\S+$|^[a-z][a-z0-9]*:[A-Za-z0-9._/+-]+$")
for s in ["c:/data/x.fits","C:/data/x.fits","c:/Users/foo/bar.fits",
          "d:/prod/2026/run1/manifest.json","c:\\data\\x.fits","/abs/path/x.fits"]:
    print(f"  {'ACCEPTED' if _STORAGE_URI_RE.match(s) else 'rejected'}  {s!r}")
EOF
python3 /tmp/re_check.py
# 期望: c:/data/x.fits、c:/Users/foo/bar.fits、d:/prod/... 均 ACCEPTED

# S2 C3 反例：恒真断言（hiss_correctness_test.cpp:430-434）
sed -n '428,435p' lib/infrastructure/aio/tests/hiss_correctness_test.cpp

# S3 C4 反例：weight 块活引用 + 判据缺失
grep -rn '"weight"' --include=*.cpp --include=*.h --include=*.py lib/ eng/
find . -name "check_aio_io_boundary.py" -not -path "./run/*"   # 期望: 无输出
ls eng/ci 2>&1 | head -1                                     # 期望: 无此目录

# S4 C5 反例：悬空引用
for p in lib/infrastructure/aio/docs/HEALPIX_FORMAT_SPEC.md \
         docs/science/algorithms/HIPS_WRITER.md \
         docs/science/DATA_SEMANTICS.md \
         docs/engineering/HIPS_STORAGE_FORM_CONTRACT.md \
         docs/detail/PRODUCT_STORAGE_FORM.md \
         docs/ACSD_DESIGN.md \
         eng/contracts/data/phase2_uncertainty_rejection_provenance_v1.json \
         eng/tests/support/acsd_test_posix_compat.h \
         eng/tests/artifact/ \
         eng/ci/ctest_baseline.json \
         lib/infrastructure/aio/ACCEPTANCE_SPEC.md \
         02_FROZEN_STAGE1_HISS_SPEC.md ; do
  [ -e "$p" ] && echo "EXISTS   $p" || echo "MISSING  $p"; done
find . -iname "*HISS-Container-and-Tiles*" -o -iname "*Stage1-Decision-Status*" -o -iname "*02_FROZEN*"
grep -c '02_FROZEN\|00_COMMON_CONTRACTS' docs/detail/PHASE1_DETAILED_DESIGN.md   # 期望: 0

# S5 B6 反例：fits_core 静默降级点
sed -n '684,703p;1106,1122p' lib/infrastructure/aio/io/fits_core.c

# S6 B2 反例：XISF kw_float 静默降级
sed -n '383,387p' lib/infrastructure/aio/src/aio_xisf.cpp

# S7 B7：生产故障注入钩子（aio_hips_writer.cpp 不在本片，仅核实存在）
grep -rn "fault_injected\|ACSD_HIPS_.*_FAULT" --include=*.cpp lib/infrastructure/aio/src/hips/
sed -n '2666,2669p' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp

# S8 B10：product_io README 与实际布局
ls eng/tests/unit/aio                       # 期望: 无 CMakeLists.txt
ls lib/infrastructure/aio/product_io/include/astro/aio/   # 实际路径
grep -n 'lib/include/astro/aio' lib/infrastructure/aio/product_io/README.md

# S9 B9：基线一致性
test -e eng/ci/ctest_baseline.json && echo EXISTS || echo MISSING

# S10 线程原语（本片命中应仅 README 文档提及）
grep -rn "std::thread\|std::async\|#pragma omp\|pthread_create\|ThreadPool" \
  lib/infrastructure/aio/ | grep -v "^lib/infrastructure/aio/include/aio_pipeline.h"
```
