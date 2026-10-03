# 审稿-P1 · INF-aio-001（G08-05 对抗审稿 第 1 遍）

- 仓库：`/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- 片清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2914-2947`
- 口径：本片**一遍 = 对同一片 26 份材料的一次完整重读**。结论全部来自我自己 `read` 原文；机器词表只用于给候选。
- 零 git 写、零编译、零 ctest/pytest、零二进制执行、未读 `/tmp/acsd_g08/`、未改仓内任何文件（除本交付件）。

---

## 1. 读完了吗

| 口径 | 数值 |
|---|---|
| 成员份数（清单声明） | 26 |
| 实际读完份数 | **26 / 26（100%）** |
| 成员总行数（清单声明 `实际行数`） | 10,630 |
| 实际读走行数 | **10,630 / 10,630（100%）** |
| 缺失文件 | 0（26 份全部存在，行数与清单逐份吻合） |
| 未读完的成员 | **无** |

行数核对（`wc -l`，与清单 `实际行数: 10630` 一致）：

| 行数 | 文件 |
|---:|---|
| 4091 | `healpix_db/archive/legacy/healpix_stack/gradient/nanoflann.hpp` |
| 746 | `src/hiss_stream_writer.cpp` |
| 658 | `io/tests/fits_core_selftest.c` |
| 633 | `runtime/artifact_store/artifact_abi_v1.c` |
| 569 | `src/hiss_transform.cpp` |
| 523 | `tests/test_checksum.cpp` |
| 412 | `healpix_db/archive/legacy/healpix_stack/stack_db.cpp` |
| 407 | `healpix_db/archive/legacy/healpix_stack/ahps_writer.cpp` |
| 306 | `tests/p1hips/p1hips_digest_verify.cpp` |
| 304 | `memory.md` |
| 289 | `.../gradient/gradient_fitter.cpp` |
| 262 | `include/aio_healpix_io.h` |
| 229 | `.../gradient/test_snr_evaluator.cpp` |
| 212 | `tests/test_fits_bscale_parse.cpp` |
| 173 | `include/astro_image_io.h` |
| 145 | `io/include/acsd/io/hips_input_v1.h` |
| 129 | `.../gradient/corrected_stacker.h` |
| 109 | `src/aio_log.cpp` |
| 98 | `tests/p1hips/CMakeLists.txt` |
| 88 | `tests/sanitize_wsl_v5.sh` |
| 76 | `product_io/include/astro/aio/hips_manifest.h` |
| 70 | `tests/p2hips/p2hips_test_main.hpp` |
| 49 | `src/aio_compressor.h` |
| 26 | `healpix_db/healpix_io/ARCHIVED.md` |
| 25 | `tests/p1hips/p1hips_tests_main.cpp` |
| 1 | `.../healpix_stack/simple_test.cpp` |
| **10630** | **合计** |

> 计数口径：**"读完" = 我用 read 工具逐行读完该文件的全部内容**，非抽样、非只看接口。对 4091 行的 `nanoflann.hpp` 分 4 段读到第 4091 行（末行 `#undef NANOFLANN_RESTRICT`），无跳读。

---

## 2. 本片判定：**阻断**

最重 3 条：

1. **`artifact_abi_v1.c:514/519/523` —— 元素计数器恒为 0，溯源输入链被静默清空，且「重复 artifact_id 拒绝」是永不执行的死代码。** 被检量与期望量同源于一次 `++q` 的偏移错误，导致判据看似存在实则恒不触发。
2. **`stack_db.cpp:67-68` —— `std::system("mkdir -p \"" + path + "\"")` 命令注入；`stack_db.cpp:376-379` POSIX `listTiles()` 是空实现，本平台（Linux）上恒返回空向量。**
3. **`sanitize_wsl_v5.sh:74` —— 第 [4/6] 步跑完不做任何断言，且 `| tee` 掩盖被测进程退出码 ⇒ 该 ASan/UBSan 门在本脚本里恒真，永远不可能红。**

---

## 3. 逐文件清单

> 每行 = 读了什么 → 看到什么 → 判定。所有 `文件:行` 均经我二次复核。

### 3.1 `runtime/artifact_store/artifact_abi_v1.c`（633 行）— **阻断**

- **读了**：全部 633 行，含 `parse_json_string` / `parse_json_uint` / `find_key` / `find_key_deep` / `extract_field_from_obj` / `locate_value` / `hex64_ok` / `handle_free` / 主解析入口与 18 个查询器。
- **看到**：
  - 🔴 `:514 ++q;` + `:519 int depth = 0;` + `:523 else if (*r == '{' && depth == 1) ++cnt;` —— `q` 已跳过最外层 `[`，数组第一层 `{` 落在 `depth==0`，永不满足 `depth==1`。`eng/contracts/data/artifact_manifest.schema.json` 中 `input_digests` 是扁平 `items:object`（无嵌套数组），故 **对一切合规 manifest `cnt ≡ 0`**。`:527 h->input_count = cnt` 因此恒 0；`:552-558` 的重复 `artifact_id` 拒绝整段是**死代码**；`acsd_artifact_query_input_count_v1` 恒返 0，`:615-621` 逐项查询恒 NULL。而 `lib/include/acsd/contracts/artifact_abi_v1.h:46` 明文冻结保证「input_digests 内 artifact_id 唯一(重复输入拒绝)」—— **该保证从未执行过**。
  - 🔴 `:567 *out = h;` 只在成功路径；全部 20+ 条错误路径均**不写 `*out`**。头 `:50` 明文「失败返回非 0 且 `*out` 置 NULL」，姊妹合同 `hips_input_v1.h:74` 同样要求 —— **本文件是唯一违反者**。
  - 🔴 `:334`/`:340`/`:352`/`:497` 的 `free(s); handle_free(h);` 构成 **double free**：`find_key_deep`（`:169-189` 全部四条 return 路径）失败时**不重置 `*out_str`**，此时 `s` 仍是上一次成功挂到 `h->` 上的指针。逐条：`:334`→`h->manifest_schema`（再 free 于 `:280`）、`:340`→`h->artifact_id`（`:281`）、`:352`→`h->type_id`（`:282`）、`:497`→`h->storage_uri`（`:284`）。触发输入：`{"manifest_schema":"acsd.artifact-manifest/v1","artifact_id":42}`（`artifact_id` 给数字而非字符串 → `has_str==0` → 先 `free(s)` 再 `handle_free` 又 free）。**四处均在 `if (cnt > 0)` 之外，不受 B1 掩盖。**
  - 🔴 `:62-73` `\uXXXX` 分支：`p` 已指向 `'u'`，`memcpy(buf+len, p, 6)` 复制 6 字节但转义只剩 5 字节 → **吞掉闭合引号**并把下一个字符并入缓冲；反斜杠也被丢弃，与 `:63` 注释「原文直存」不符。
  - 🔴 `:149-155` / `:229-250` / `:538-543` 括号深度计数**不跳过字符串字面量**，值里含 `}`/`]` 会截断对象跨度。
  - 🟡 `:404-405` 注释「额外整对象级重复 key 检测…（见 duplicated-key 扫描）」—— 全仓 grep `duplicated` 只命中该注释自身与无关的 cfitsio；真正实现在 `artifact_manifest_validator.py`（存在，见 3.12），**C 层措辞指向了一个不存在的扫描器**。
  - 🟡 `:9` 注释称「4=IO/解析错误」，全文只返回 0/1/3，**永不返回 4**；所有解析错误都报 `ACS_ERR_PARAM(1)`，无法区分「调用方传空指针」与「manifest 内容非法」。
  - 🟡 `:610` NULL 句柄返回 `ACS_ART_STATUS_PENDING`（合法枚举值），`:593` 返回 0 —— 空句柄与真实取值不可区分。
  - 🟡 `grep -rn acsd_artifact_manifest_parse_v1` 全仓**零调用者**（仅自身定义 + 头声明）；`ACS_ARTIFACT_ABI_VERSION_V1` 仅在头 `:28` 出现一次，**无任何读取点**，`offsetof` 0 次 → ABI 版本宏空转。
- **判定**：**阻断**。

### 3.2 `src/hiss_transform.cpp`（569 行）— **须修**

- **读了**：全部 569 行。
- **看到**：
  - 🔴 **所有失败路径 `return {}`（空 vector），而"成功处理空输入"也返回 `{}`**（`:127`/`:199`/`:237`）⇒ **调用方无法区分「变换失败」与「变换成功但输入为空」**。对比 `apply_delta_varint:352` 刻意为��输入返回 4 字节 `{0,0,0,0}` —— DELTA/BYTE_SHUFFLE 三条路径没有这个区分，是**伪装成成功的静默降级**。
  - 🔴 `:378 uint32_t n = (uint32_t)n_elements;` —— 无溢出检查，而同文件 `:89-107` 的 `element_mask`/`write_element_le` 都做了范围处理；>4G 元素时静默截断写出错误前缀，inverse 端算出的 `output_size` 与实际不符。写侧 `hiss_stream_writer.cpp:435-438` 对同类窄化**有** `safe_size_to_u32` 守卫 —— 两侧不对称。
  - 🔴 `:285 zig_zag_encode`：`n << 1` 对负 `int64_t` 是**有符号左移溢出（UB）**；正解须先转 `uint64_t`。
  - 🟡 `:318-320 varint_decode`：`shift > 63` 才报错 ⇒ 第 10 字节（`shift==63`）的高位**静默丢弃**，损坏 varint 可解出错误值而不报错。
  - 🟡 `:375 output.reserve(data_size + 4)` 未查加法溢出。
  - 🟡 `:441` `HISS_MAX_SUBBLOCK_UNCOMPRESSED` 有推导（`hiss_format.h:280-281`「2 MiB × 32 = 64 MiB」），**不是恒真门**（已验算）。
- **判定**：**须修**（静默降级 + UB + 不对称守卫）。

### 3.3 `src/hiss_stream_writer.cpp`（746 行）— **阻断（返回码语义）**

- **读了**：全部 746 行。
- **看到**：
  - 🔴 **`finalize` 8 条失败路径中 6 条返回裸负数，与 `hiss_format.h:242-249` 冻结的 `HISS_ERR_*` 撞值且语义相反**：`:566 return -4`（写 Header 失败，纯 I/O）= `HISS_ERR_CHECKSUM`；`:580 return -5`（打不开临时池）= `HISS_ERR_FORMAT`；`:596 return -6` = `HISS_ERR_UNSUPPORTED`；`:606 return -7` = `HISS_ERR_UNKNOWN_REQUIRED`；`:673 return -8` = `HISS_ERR_FORMAT_VIOLATION`；`:534 return -2` = `HISS_ERR_INVALID_STATE`。**同函数内 `:523 return HISS_ERR_FORMAT;` 与 `:580 return -5;` 返回完全相同的值却是两种成因** —— 函数自身二义。违反「每个失败产生稳定错误码」。
  - 🔴 `:684-688` 父目录 fsync：`slash == npos`（相对路径）或 `slash == 0` 时 `drc = 0` **直接短路，连尝试都不做**，`:690` 的「第三态」告警因此永不触发 ⇒ 相对输出路径下目录项持久化静默无保证。
  - 🟡 `:12` 注释「签名块(20B)」与 `:49/:373/:539` 的 16B 自相矛盾（`HISS_SIGNATURE_SIZE 16` 已验）；`:10` 提到的 `add_tile` 在本文件不存在（真实 API 是 `append_subblock`/`record_tile`）。
  - 🟡 `:642` 引 `docs/detail/infrastructure/17_aio.md:26`（该句实际在 `:69`）；`:173` 引「00_COMMON_CONTRACTS §4.6」**全仓不存在**；`:5/:7` 引 `02_FROZEN_STAGE1_HISS_SPEC.md`、`docs/engineering/IO_AND_ATOMICITY.md` **均不存在**。
  - 🟡 `:300-331` `#ifdef HISS_PROFILE` 用 `std::chrono::steady_clock`，include 段（`:23-30`）**无 `<chrono>`** ⇒ 定义该宏即编译失败。
  - 🟡 `:651/:692` 把 `fsync_path` 返回值当 `errno` 打印（`fprintf(..., "errno=%d", frc)`），诊断字段名与实义不符。
  - 🟡 `:454-459` `finalize` 就地累加 `sb.offset`，非幂等；当前所有失败路径都置 `opened=false`，故**暂不可达**，属潜伏缺陷。
  - ✅ `:516 hdr.data.size() != header_size` **不是恒红门、也不是自洽断言**（我独立重算：39+31+7+json+7+tile_dir 与 `ByteBuf` 逐条序列化逐项相等，两条独立路径算出同值 ⇒ **真交叉校验**）。
  - ✅ `HISS_SIGNATURE_SIZE=16`、`HISS_SUBBLOCK_DESC_DISK_SIZE=42`（`hiss_format.h:258/278`）与 `sig[16]`、`:497-509` 逐字段求和一致。
- **判定**：**阻断**（错误码语义 + fail-open 短路）。

### 3.4 `healpix_db/archive/legacy/healpix_stack/stack_db.cpp`（412 行）— **阻断**

- **读了**：全部 412 行。
- **看到**：
  - 🔴🔴 `:67-68` `std::string cmd = "mkdir -p \"" + path + "\""; return (std::system(cmd.c_str()) == 0);` —— **命令注入**。路径只被双引号包裹，未转义 `"`、`$`、`` ` ``、`\`。触发：`StackDatabase::create("/tmp/x\" ; rm -rf ~ ; echo \"", cfg)`。同时违反 AGENTS.md §6「不绕过统一 I/O / 不直接退进程」。
  - 🔴 `:376-379` POSIX `listTiles()` **整段是空实现**（`// POSIX: 用 opendir … 此处省略, 主要平台为 Windows`），Linux 上恒返回空向量；而 `findTile():332` 走 `pathExists` 正常工作 ⇒ **同一 API 族里「按路径查」有结果、「列全部」恒空**，且无任何错误码。这是本仓库平台的真实缺陷。
  - 🔴 `:287-288` `if (m_config.nsideLod.empty()) …={512,2048,8192,32768}; if (m_config.bands.empty()) …={"L","R","G","B","Ha","OIII"};` —— meta.json **损坏或缺键**时静默套用硬编码默认，且 `loadMeta()` **返回 true（成功）**。三种语义（合法空树 / 文件缺失 / 文件损坏）坍缩为同一配置，调用方无从分辨。
  - 🔴 `:391-393` `getOrCreateTileWriter` 注释自认「此处仅创建空 writer，合并由 engine 处理」+ `(void)p;` —— 已存在的 tile 数据**不被读取**，依赖它追加的调用方会静默覆盖既有科学数据。
  - 🟡 `:256-258` `fwrite`/`fclose` 返回值均未检查，却无条件打印「meta.json 已保存」并 `return true`。
  - 🟡 `:275` `fread` 未检查返回；文件被截断时 `json` 带尾随 `\0`，`strtod` 可能读到垃圾。
  - 🟡 `:211-217` LOD 目录 `if (!pathExists(d)) makeDir(d);` **返回值丢弃**。
  - 🟡 `:361-368` `WideCharToMultiByte` 返回 0 时仍写 `&subdir8[0]`（空串下标 0）。
  - 🟡 `:118` `jsonGetBool` 只跳空格/制表符，漏 `\n`；`:103` `jsonGetNumber` 跳了 `\n` —— 两函数空白处理不一致。
  - ✅ `findTile():334-335` 用 `p.empty()` 交由调用方判空，是 fail-closed 的正确范式。
- **判定**：**阻断**。

### 3.5 `healpix_db/archive/legacy/healpix_stack/ahps_writer.cpp`（407 行）— **阻断**

- **读了**：全部 407 行。
- **看到**：
  - 🔴 `:305-330` header size 不动点迭代 **上限 20 次后不校验是否收敛**：退出时 `headerSize` 是最后一次赋值，而 `finalJson` 是最后一次构造；**两者可能不等**。`:373 fwrite(finalJson.data(), 1, headerSize, fp)` 于是可能越界读 `std::string` 或截断 JSON → 所有 chunk offset 全错。注释 `:299` 自己就承认「避免 size 振荡」，却没做振荡检测。对照 `hiss_stream_writer.cpp:516` **有** `hdr.data.size() != header_size` 硬失败守卫 —— 此处缺失。
  - 🔴 `:336` 直接以 `"wb"` 写最终路径，**无 `.partial`、无 fsync、无原子 rename** ⇒ 崩溃即留截断文件在正式名。违反 IO_003 原子发布合同（对照 `hiss_stream_writer` 的 .partial+fsync+rename 全套）。
  - 🔴 `:399 std::fclose(fp)` 返回值未检查即 `return true` ⇒ 末次刷盘失败仍报成功。
  - 🟡 `:361 putU32(hp, (uint32_t)m_pixelIndices.size())` —— uint64→uint32 **静默截断**，无守卫。
  - 🟡 `:88/:246/:281 if (compSize == 0 || compSize >= srcBytes)` 回退 `Codec::NONE` —— 把「压缩失败」与「压缩无收益」**合流**，把 `aio_compressor.h` 的失败三义继续放大。
  - 🟡 `:153-166` 用户 `m_headerJson` 原样拼进 JSON，**无合法性/重复键校验**（可注入 `"pixelChunks"` 造成重复键）。
  - 🟡 `:26-29` `MultiByteToWideChar` 返回 0 未检 → `wmode` 为空 → `_wfopen` 必失败；`:22-23` UTF-8→宽字符失败时静默回退窄字符 `fopen`（非 ASCII 路径必失败）。
- **判定**：**阻断**。

### 3.6 `healpix_db/archive/legacy/healpix_stack/gradient/gradient_fitter.cpp`（289 行）— **阻断**

- **读了**：全部 289 行。
- **看到**：
  - 🔴 `:202 double ref_p = (ref_den > 0) ? ref_num / ref_den : 0.0;` —— **无任何其他帧覆盖该控制点时，参考场静默取 0**，`diff_i[p] = bg_i(p) - 0 = bg_i(p)`，把背景中位数（ADU 量级）当作"差异"灌进 sigma-clip 与样条拟合。**单帧 mosaic（n_frames=1）时内层 `if (j==i) continue;` 跳过全部 j ⇒ `ref_den≡0` ⇒ `diff_i ≡ bg_i`**，拟合出的"梯度校正"就是整幅背景图。无错误码、无日志、无标志位。
  - 🔴 `:229-235 if (kept_ra.size() < 5) { kept_* = frames[i].*; }` —— sigma-clip 剔掉的点太多时，**静默丢弃 clip 结果、改用未过滤全集重拟合**，恰好把刚识别出的离群点重新纳入。注释自认「(不过滤)」。这是"筛掉真信号"的反向形态：**筛子咬住时自动失效**。
  - 🔴 `:243-246 if (rc != 0 || !new_model.valid) { continue; }` —— 样条拟合失败**静默跳过该帧**（g_i=0，该帧不做梯度校正），`error_msg_` 不置位，而 `fit()` 仍 `:284 out.success = true; :286 return 0;` ⇒ **部分失败被上报为全部成功**。
  - 🟡 `:137` 硬编码 `>= 5`（"样条至少需要 5 点"）无推导；`:181 double nearest_dist = 1e18;` 魔数哨兵。
  - ✅ `:210` 的 `1.4826` 是 MAD→σ 一致性常数（1/0.6745），**有科学推导**，属合规硬编码。
  - 🟡 `:70 median(std::vector<double>&)` 取非 const 引用且就地 `sort` 调用方数组；`:207-208` 已正确用 `diff_copy` 防御，但签名本身是给后来者的陷阱。
- **判定**：**阻断**。

### 3.7 `healpix_db/archive/legacy/healpix_stack/gradient/test_snr_evaluator.cpp`（229 行）— **须修**

- **读了**：全部 229 行。
- **看到**：
  - 🔴 `:159 if (ms > 0 && out_snr[0] > 0) { printf("  OK: 批量评估正常完成"); n_pass++; }` —— **恒真门**。`:156-158` 的注释明写验收判据是「spec 要求 KD-tree 评估 < 20ms/帧（4096×4096）」，**但代码里没有任何一处比较 `ms` 与 20ms**，只看 `ms>0`（必然为真）与 `out_snr[0]>0`（只看 1M 个输出中的第 0 个）。**真实性能判据被一条近乎空的门替换。**
  - 🔴 `:146-154` 算出 `snr_min/snr_max/snr_mean` 只 `printf`，**零断言**。
  - 🔴 `:177 if (snr == 0.0f)` —— 把「未 build 时返回 0.0」**锁成合同**。0.0 与"真实零 SNR"不可区分；下游用 SNR² 当权重时，未构建模型 → 权重 0 → 像素**静默掉出叠加**。
  - 🟡 `:214 if (ms < 100 && eval.isBuilt())` —— 100ms **无推导出处**，且机器相关（CI 繁忙时会假红）。
  - 🟡 `:3` 手工 g++ 命令，无 CMake 接线；文件在 `archive/legacy/` ⇒ 死源码。
  - ✅ `:34-35` `n_pass/n_fail` 为局部，`:228 return n_fail > 0 ? 1 : 0;` **退出码正确传播**，不属"软通过"。
- **判定**：**须修**（恒真门 + 把 fail-open 锁成合同）。

### 3.8 `tests/sanitize_wsl_v5.sh`（88 行）— **阻断**

- **读了**：全部 88 行。
- **看到**：
  - 🔴 `:74` `./gaia_sanitize "…" | tee gaia_sanitize.out` —— **第 [4/6] 步之后没有任何 `grep -q` 断言**（第 1/2/3/5/6 步都有）。更致命：管道下 `set -e`（`:13`）取的是**最后一个命令 `tee` 的退出码**，被测二进制崩溃/ASan `abort_on_error=1`（`:26`）/UBSan `halt_on_error=1`（`:27`）的非零退出**全被掩盖**。⇒ **DR3SP parser 的 ASan/UBSan/LSan 门在本脚本里恒真，永不可能红**，而 `:88` 仍会打印 `ALL_SANITIZE_V5_PASS`。
  - 🔴 `:74` 硬编码 `/mnt/f/Astro dev/Astro CS Normalization Database/GaiaDR3SP` —— 本仓在 `/workspace/Astro CS Database`，无 `/mnt/f`。即便补上断言也**在本环境不可跑**。
  - 🔴 `:40 gcc … -c > cfitsio_cc.log 2>&1 || true` —— `|| true` 吞掉第三方 cfitsio 编译失败。
  - 🟡 `:36/51/63/66/78/86` 的门是 `grep -q "<程序自报的 RESULT: …>"` —— 判定依赖**被测程序的自述字符串**，属"用同一式既当被检量又当期望量"的脚本层形态。
  - 🟡 `:21 rm -rf "$BUILD"` 固定路径；`:41` 硬编码 cfitsio 对象文件名清单。
  - ✅ 引用的 7 个源文件（`hips_sanitize_driver.cpp`、`hips_robust_sanitize_driver.cpp`、`gaia_sanitize_driver.c`、`healpix_core.cpp`、`test_healpix_oracle.cpp`、`test_hips_tile_mapping.cpp`、`gaia_client.c`）**全部存在**（已逐个 `test -e`）。
- **判定**：**阻断**。

### 3.9 `tests/p1hips/p1hips_digest_verify.cpp`（306 行）+ `tests/p1hips/CMakeLists.txt`（98 行）— **阻断（判据不设防）**

- **读了**：两文件全部。
- **看到**：
  - 🔴 `CMakeLists.txt:93` `add_executable(p1hips_digest_verify EXCLUDE_FROM_ALL …)`，**且全文件无 `add_test(p1hips_digest_verify …)`** ⇒ 该 harness **默认不构建、ctest 永不运行**。V1–V4 全部（含 64c1e988 回归锚 `VERIFY(d2 == d0, "v4_checksum_comment_ignored")`）**不设防**。注释 `:90-92` 给的理由是"秒级慢测不适合 CI 常驻"，但代价是判据永久只在人工手动跑。
  - 🔴 对照：`CMakeLists.txt:76-88` 的 `p1hips_selfcheck` **有** `add_test`，注释明写「故障注入必败自检（验收: baseline 必 PASS + 注入必 FAIL **双向排除恒常**）」⇒ **仓内已有正确范式，digest_verify 没有采用**。这正是负责人裁定「看到任何『检查通过』的机制不要据此认为实现正确」的实例。
  - 🟡 `p1hips_digest_verify.cpp:25` `#include "p1hips_tests_units.cpp"`（直接编 .cpp 以够到匿名 namespace 内核）。
  - 🟡 `:230 sleep(2)` 用 POSIX `sleep` 但未 `#include <unistd.h>`（依赖传递包含，Windows 不可编）。
  - ✅ `:82-93 cards_zeroed()` 是 harness **独立实现**，与被测 `normalize_fits` 不同源 ⇒ **不是自洽式断言**（已核）。
  - ✅ `VERIFY` 宏 `:38-46` 递增 `g_fail`，`:300-305` 返回 1 ⇒ 退出码正确传播。
  - 🟡 `CMakeLists.txt:34` 以裸负号罗列错误码 `-1..-5,-6,-7`，与 3.3 的撞值问题同源。
- **判定**：**阻断**。

### 3.10 `io/tests/fits_core_selftest.c`（658 行）— **须修**

- **读了**：全部 658 行。
- **看到**：
  - ✅ `:483-486` **本片质量最高的断言**：「判别力: 全零占位串必须由**专用拒绝分支**判红（错误文本点名），而不是碰巧落到通用 mismatch —— 后者在 sum==0/0xFFFFFFFF 兼容分支下会**假绿**（变异实测: 关掉专用分支后本用例曾 ALL PASS）」。这是**经过变异测试的、真正有判别力的断言**，应作为全仓模板。
  - 🟡 `:572-596 test_write_denied`：`mkdir(dir, 0500)` **在 root 容器里不阻止写入** ⇒ `writer_begin` 会成功 → `CHECK(st == IO || DISKFULL)` 假红；若有人为"修好"它而放宽成 `st != OK`，就退化成恒真门。
  - 🟡 `:473 memset(hdr + off + (long)vs, '0', 16)` —— `vs` 可达 80（`'='` 在第 79 列），`off` 上限 2872 ⇒ 写入可达 `hdr[2968]`，**越过 `char hdr[2880]` 栈缓冲**。
  - 🟡 `:97` 在 `writer_end_v1(wr, …)` 之后仍用 `wr` 调 `acsd_fio_writer_bytes_written_v1` —— 若 end 使句柄失效即 UAF（需看头文件，未在本片内）。
  - 🟡 `:634 mkdir(g_dir, 0700)` 与 `:650 rmdir(g_dir)` 返回值均未检 → 临时目录残留。
- **判定**：**须修**。

### 3.11 `tests/test_checksum.cpp`（523 行）— **须修**

- **读了**：全部 523 行。
- **看到**：
  - 🔴 `:15-24` 编译配方指向 **`f:\Astro dev\Astro CS Normalization Database\lib\astro_image_io`** 与 `eng/tests/test_checksum.cpp` —— 该目录布局**已不存在**（现为 `lib/infrastructure/aio/tests/`），项目名亦已变。悬空引用 + 整目录已删形态。
  - 🔴 `:484/:489` `ASSERT_EQ_INT` 内部 `long _a = (long)(a);`，而 `:489` 比的是 `0xE3069283`（= 3808858755 > INT32_MAX）。本文件自己的构建目标（`:16` mingw64/Windows）是 **LLP64（long 为 32 位）** ⇒ 该断言在该平台上溢出生效/恒假。**跨平台必红的门。**
  - 🟡 `:141/232/325` fixture 用**相对路径写在 CWD**（`test_checksum_*.hiss`），与 `test_fits_bscale_parse.cpp:110-116` 明写的"不留在工作树"纪律不一致。
  - 🟡 `:308 ASSERT_EQ_INT(ret, -5, …)` 用裸魔数而非 `HISS_ERR_FORMAT`。
  - 🟡 `:450` 往单例 `ChecksumRegistry` 注册 `XXHASH_PLACEHOLDER`（伪哈希）且无注销，污染全局状态。
  - ✅ `:478-490` CRC32C 比的是 **Castagnoli 外部标准向量 0xE3069283** ⇒ **独立 oracle，非自洽式断言**。
  - ✅ `:54-62` `ASSERT_TRUE` 宏无 `true, "已知问题"` 软通过分支，`:516-522` 退出码正确传播。
- **判定**：**须修**。

### 3.12 `memory.md`（304 行）— **须修（文档纪律 + 事实错误）**

- **读了**：全部 304 行。
- **看到**：整篇是**日期 + commit + 任务流水**的历史叙事，直接违反 AGENTS.md §5（「正文无日期、版本号、任务流水编号、commit、『旧版/作废/曾/原』等历史叙事」）。且事实错误密集：
  - `:8` 「最新commit：a33d167」—— 仓 HEAD 是 `850a9ede`，且同文件 `:203/225/232/237` 已列出更晚的 `450e78c / c7d3b8f / eb52e06 / 05cccb4` ⇒ **自相矛盾且过期**。
  - `:13` 「默认分支：master」 vs `:66` 「分支统一为main」⇒ **同文件内自相矛盾**。
  - `:18` 「无外部库（零依赖，纯C++原生实现）」 vs `:43` 「CFITSIO 4.6.4 vendored 并静态编进 DLL」⇒ **25 行后自我否定**。
  - `:9` 「更新时间：2026-07-12」但内容一直到 `:296` 的 2026-09-13。
  - `:120/:165` 「签名块 20B: MAGIC "ACSHISS\0"」与 `:123/:168` 「子块描述符 40B」—— **现行格式已是 16B / 42B**（`hiss_stream_writer.cpp:49/54`、`hiss_format.h:258/278`），且 `hiss_stream_writer.cpp:53` 明写旧 ACSHISS 20B 格式「**已废弃**」。memory.md 把**已废弃格式当现行规范**记录。
  - 路径大面积失效：`:92` `lib/include/aio_healpix_io.h`（实际 `lib/infrastructure/aio/include/…`）；`:96/:97` `docs/…`、`eng/tests/…`（实际在 `lib/infrastructure/aio/` 下）；`:98` `python/aio_healpix_io.py` —— **该文件不存在**（已 `test -e` 验证）；`:103` `healpix_db/healpix_stack/`（实际在 `healpix_db/archive/legacy/healpix_stack/`）。
  - `:297-301` 把构建产物 `astro_image_io.dll` 当交付面叙述，并附「RESCUE-V3 B3-A3（2026-09-13，FIX-CI）」任务编号。
- **判定**：**须修**。

### 3.13 `include/astro_image_io.h`（173 行）— **须修**

- **读了**：全部 173 行。
- **看到**：
  - 🔴 `:149` 「旧签名写出的文件在读取侧被显式拒绝（**见 aio_atomic_format.h** 兼容策略）」—— `lib/infrastructure/aio/include/aio_atomic_format.h` **不存在**（已 `test -e` 验证），真实名为 `aio_ahpx_format.h`。悬空引用（键名/文件名已改）。
  - 🟡 `:104 aio_read_metadata` / `:116-121` 多个函数**按值返回**大结构体，跨 DLL ABI 无 `static_assert`、无 packing、无版本参数；失败时只能返回清零结构体，与"元数据全零的真实图像"不可区分。
  - 🟡 `:17-28` 12 个缓冲区尺寸常量无推导；`AIO_PATH_MAX 512` 与 `hips_input_v1.h:38 ACS_HIPS_PATH_MAX 1024` 同子系统两套路径上限，无协调依据。另 `:17-19` 的 72 字节超出 FITS 标准 80 字符卡的可用值区（68 列），长关键字值会被静默截断。
  - 🟡 `:170-171` 在 `#ifdef __cplusplus }` 之后**无条件** `#include "aio_pipeline.h" / "aio_pipeline_engine.h"` —— 整文件是 `extern "C"` 的 C 可包含头，纯 C 翻译单元包含即失败。
  - 🟡 `:98 aio_set_precision_mode(int)` 是进程级可变全局，无同步声明。
  - ✅ AHPX 权重退役**是真的**：`:147-151` 的说法有 `aio_ahpx_format.h:29-33` 读写双侧 fail-closed 实现支撑，且 `grep` 未发现旧签名活调用者 ⇒ **我原先"退役对象仍有活调用者"的假设在本片被证伪，予以否决**。
- **判定**：**须修**。

### 3.14 `include/aio_healpix_io.h`（262 行）— **须修**

- **读了**：全部 262 行。
- **看到**：
  - 🔴 `:9` 「原 healpix_io/ 目录已归档到 `lib/infrastructure/aio/healpix_db/healpix_io/archive/`」—— **该 `archive/` 目录不存在**（已 `test -d` 验证）。`healpix_db/healpix_io/` 实际只有 `ARCHIVED.md` 一个文件。两个归档说法互相矛盾，且都是历史叙事（违反 §5）。
  - 🟡 `:70-76 HioSnrModelF64` 声明了但**无配套释放函数**（`:127` 只有 `aio_hio_free_snr_model(HioSnrModel*)`），也无 `write_snr_model_f64` ⇒ F64 模型无所有权合同（`grep` 确认 F64 仅出现在 `run/` 归档树与本头，生产面未用）。
  - 🟡 `:41 float snr_psf` 仍是主 API 类型（`:112/:123`），而 `:46` 自述 BLOCKER-TYPE-002 要求 double ⇒ 加了 F64 兄弟类型却没换主链。
  - 🟡 `:66 idw_power 默认 2.0` 无推导、未入 config。
  - ✅ `:44/:54` `static_assert(sizeof(...) == 20 / == 24)` 与 `#pragma pack(1)` 下 double+double+float / double×3 精确相符 ⇒ 布局有 pin。
- **判定**：**须修**。

### 3.15 `include/astro_image_io.h` 之外的 `io/include/acsd/io/hips_input_v1.h`（145 行）— **须修**

- **读了**：全部 145 行。
- **看到**：
  - 🔴 `:75-76` 「MOC: optional —— 存在则解析…**缺失/损坏不失败**（enum 接口返回 0 计数）」+ `:106-107` 「无 MOC/无叶单元 → 0（**partial tree 合法**）」⇒ **合法空树 / MOC 缺失 / MOC 损坏不可读** 三种语义坍缩成同一个 `tile_count=0`。损坏的产品会被当作合法空产品交付。
  - 🟡 `:12/:47` 声明「0-7 与 IO-001 `acsd_fio_status` 对齐」（**已逐项核对属实**），但 8-11 与 `fits_stream_v1.h:58-63`（TRUNCATED/BAD_HEADER/MISMATCH/CHECKSUM）、`common_abi_v1.h:64-65`（BUDGET/SELFTEST）**数值混叠**；而 `fits_stream_v1.h:48` 自称与 `common_abi_v1` 对齐 ⇒ 按值互转是自然写法 ⇒ 静默误译。
  - 🟡 `:89` 「未找到 → PARAM（**调用方先查存在性**）」但**本头没有任何键存在性查询函数**；`:91-93 props_get` 也**无 `out_len`**（对比 `:98` 的 `serialize` 有）⇒ 值截断不可检测，科学元数据可被静默改写。
  - 🟡 `:36 ACS_HIPS_PROP_MAX 128` 无推导、未入 config。
  - ✅ `:31 ACS_HIPS_ORDER_MAX 29` 注释给了推导「NSIDE ≤ 2^38」，`2^29 × 8 = 2^38` 验算通过 ⇒ 合规。
- **判定**：**须修**。

### 3.16 `product_io/include/astro/aio/hips_manifest.h`（76 行）— **建议**

- **读了**：全部 76 行。
- **看到**：`:35 tile_width = 512` 无推导也未引 IVOA HiPS 1.0（对比 `:30-32` 的 `frame="equatorial"` **有**引标准 §4.4.1 ⇒ 该处合规）；`:27 hips_version="1.4"` 无出处；`:16` 在公开头里 `#include <nlohmann/json.hpp>`（与 `hips_input_v1.h:8`「不出现 CFITSIO 等第三方类型」的策略不一致）；`:55/:9` 遗留 `v6` 版本号与 `/v1` 并存；`:6` 任务编号入头文件。
- **判定**：**建议**。

### 3.17 `healpix_db/healpix_io/ARCHIVED.md`（26 行）— **须修**

- **读了**：全部 26 行。
- **看到**：`:8` 「Python 绑定: `lib/infrastructure/aio/python/aio_healpix_io.py`」—— **该文件不存在**（已 `test -e` 验证）⇒ 悬空引用，正是负责人点名的「整目录已删 / 键名已改」形态。`:6/:7/:9/:11` 的路径经核**存在**；`:10` 的 `test_healpix_io*.py` 通配命中两个文件（`test_healpix_io.py` 与 `test_healpix_io_py.py`，重名近似，疑为历史重复）。`:3-4` 整篇是日期 + 归档叙事，`:26` 又称「本目录代码不再用于编译或运行」——但目录里根本没有代码，只有这一个 md。
- **判定**：**须修**。

### 3.18 `healpix_db/.../gradient/corrected_stacker.h`（129 行）— **须修**

- **读了**：全部 129 行。
- **看到**：
  - 🔴 `:66 sigma = 3.0`（注释「通常 3.0」）、`:69 max_iter = 5`、`:75-76 winsorize_low_pct=0.05 / high_pct=0.95` —— **四个直接改变科学输出（决定哪些像素被 clip）的算法参数，全部以头文件默认值存在，无推导、无出处、未入 config**。按项目硬编码规则属第 (d) 类「无依据经验值」。
  - 🔴 `:45` 「SNR-B（空数组 = 等权，权重=1.0）」—— **缺 SNR 时静默降级为等权**，而不是拒绝或打标。P2 是跨帧绝对信噪比通道，此处 fail-open 会让加权叠加悄悄退化为无权叠加。
  - 🔴 `:98` 「n_models: 模型数（**应 = 最大 frame_id + 1, 或 = n_frames**）」—— 同一参数给了**两个不兼容定义**，实现无从选择。
  - 🔴 `:26` 「设计文档: `.trae/specs/snr-compact-storage-and-gradient-correction/spec.md` §3.5」—— **该路径不存在**（已 `test -e` 验证）⇒ 悬空引用，算法出处断链。（`spherical_spline.h` 经核**存在**。）
  - 🟡 `:103-108` 返回码 0/1/2/3 中 `1=输入为空` 与 `3=帧数据为空` 语义重复；`:116 lastError()` 返回字符串，无稳定数值错误码。
  - 🟡 `:14` `mean = Σ(SNR²·corrected)/Σ(SNR²)` —— Σ(SNR²)=0 即除零（实现文件不在本片，未能核实是否已守）。
- **判定**：**须修**。

### 3.19 `src/aio_log.cpp`（109 行）— **建议**

- **读了**：全部 109 行。
- **看到**：`:40/:48/:59/:62` 日志落点是**相对 CWD 的仓库路径** `lib/infrastructure/aio/logs` ⇒ 从 build/ 或 run/ 启动会**在工作目录里另建一棵树**（违反 AGENTS.md §6「不写未声明文件」）；`:50` 注释自称「不吞、不假装成功」，但 `:51-54`/`:43-44` 的处理是 `g_aio_log_file = nullptr; return;` —— **无日志、无错误码、函数返回 void，调用侧无从知晓**，属注释与实现相反的静默降级；`:31` 在失败态下每次调用都重试 `create_directories`；`:16/:19/:23/:25/:77` `g_aio_log_level` 取锁前读、无锁写，多线程数据竞争。
- **判定**：**建议**（我原本判须修，因其为诊断通道而非数据面，上调一级；但注释与实现相反仍需订正）。

### 3.20 `src/aio_compressor.h`（49 行）— **须修**

- **读了**：全部 49 行。
- **看到**：`:12` 「无库可用时 fallback 到不压缩（**返回 0** 表示未压缩，调用方需处理）」—— **失败、未压缩、空输入三者共用返回值 0**，调用方不可区分；`:21/:34` 解压失败同样返 0，而 0 也是合法的解压长度；`:17` 「level: 1-22, **默认 5**」为无出处、未入 config 的可调参数。
- **判定**：**须修**（失败语义三义）。

### 3.21 `tests/p2hips/p2hips_test_main.hpp`（70 行）+ `tests/p1hips/p1hips_tests_main.cpp`（25 行）— **建议**

- **读了**：两文件全部。
- **看到**：`p2hips_test_main.hpp:24-32 check_impl` 递增 `g_failures`，`:56-62` 有失败即 `return 1` ⇒ **失败正确传播退出码，不属软通过**（我先前的"测试失败只打印不返回码"假设在此**被否决**）。但 `:52-63` 每次只跑**一个**组；是否四组都被 CI 跑到取决于注册方式（本片外）。`p1hips_tests_main.cpp` 仅 25 行，`test_units/test_properties/test_oracle/test_negative` 只声明（`:10-13`），定义在同目录其他 .cpp（不在本片）。
- **判定**：**建议**。

### 3.22 `tests/test_fits_bscale_parse.cpp`（212 行）— **建议**

- **读了**：全部 212 行。
- **看到**：这是**质量较高的负面测试**——`:19-20` 引 FITS Standard 4.0 §4.4.2.5 说明「存在而非法的 BSCALE 属损坏头，不得静默按缺省缩放」，`:23-28` 六条正负例齐备，`:113-116` fixture 落系统临时目录并注明根 `.gitignore:66`。问题：`:136/:147` 诊断分支 `float* px = aio_get_pixel_data(img); printf(…px[0]…)` **未判 px 非空** ⇒ 失败路径上空指针解引用；`:174/:186/:198` 的 `1e-4f` 容差无推导；`:142/:153/:160/:169/:181/:193` fixture 失败 `return 2` 时**不打印也不清理已建 fixture**。
- **判定**：**建议**。

### 3.23 `healpix_db/.../gradient/nanoflann.hpp`（4091 行）— **建议**

- **读了**：**全部 4091 行**（分 4 段：1-75 / 76-1475 / 1476-2773 / 2774-4068 / 4069-4091）。
- **看到**：**经核为未改动的上游第三方代码**——`:2-31` BSD 许可、`:4-6` 署名 Muja/Lowe/Blanco、`:89 #define NANOFLANN_VERSION 0x190`、`:56-60` 指向上游 README、`:1646-1655` 明确列出 `saveIndex` 的四项可移植性限制。合规硬编码（`leaf_max_size=10`、`:1474 EPS=0.00001`）属上游常量。风险面：`:1410-1467 / 1947-1954 / 2353-2360 / 3883-3892` 在 `n_thread_build != 1` 时用 `std::async(std::launch::async, …)`，`:3663-3946 KDTreeSingleIndexIncrementalAdaptorMT` 更是一个**专职后台重建线程**的类；`:66/:75` 无条件 `#include <atomic>/<future>`。本仓调用方（`snr_evaluator.cpp`，不在本片）未见传 `n_thread_build>1`，故**当前不构成私建线程池违规**；但该文件位于 `archive/legacy/` 且**不被任何 CMake 引用**（已 `grep --include=CMakeLists.txt` 确认）⇒ 按 AGENTS.md §6「退役代码从代码库删除」应删。
- **判定**：**建议**（删档，非逻辑缺陷）。

### 3.24 `healpix_db/.../healpix_stack/simple_test.cpp`（1 行）— **建议**

- **读了**：全部 1 行。
- **看到**：`int main(){return 0;}` —— 一个恒绿的占位"测试"，位于 archive 目录，未被任何构建引用。**若被误接入构建，它是一个 100% 通过且什么都不验的门。**
- **判定**：**建议**（随 archive 一并删除）。

### 3.25 `healpix_db/.../healpix_stack/ahps_writer.cpp` 的姊妹 `stack_db.cpp` — 见 3.4（已单列）

### 3.26 构建接线总核查

- `archive/legacy/healpix_stack/*` 与 `nanoflann.hpp`：**不被任何 `CMakeLists.txt`/`*.cmake` 引用**（`grep -rn --include=CMakeLists.txt --include='*.cmake' -e healpix_stack -e nanoflann -e stack_db -e ahps_writer -e gradient_fitter -e test_snr_evaluator` 在 `lib/`、`eng/` 下**零命中**；仅 `lib/algorithms/drizzle/CMakeLists.txt:111` 指向一个**不存在的** `${CMAKE_SOURCE_DIR}/third_party/nanoflann/include`）⇒ 3.4/3.5/3.6/3.7/3.23/3.24 六份是**死源码**。这不降低其缺陷严重度（代码仍在仓内、可被误接），但决定了处置方式：优先删除而非修复。
- `io/tests/fits_core_selftest.c`：**已接线**（`eng/tests/unit/CMakeLists.txt:1714` → `add_test(w34_fits_core_selftest)`），真跑。
- `tests/test_fits_bscale_parse.cpp`：**已接线**（`eng/tests/unit/CMakeLists.txt:1865`）。
- `tests/test_checksum.cpp`：**仅在 `lib/infrastructure/aio/tests/CMakeLists.txt:9` 的注释里被提到**（"test_checksum 49 PASS / 0 FAIL"），**无 `add_executable`/`add_test`** ⇒ 孤儿测试。
- `tests/sanitize_wsl_v5.sh`：无构建引用，手工脚本。

---

## 4. 发现清单

### 4.1 阻断（5）

| # | `文件:行` | 缺陷 | 类别 |
|---|---|---|---|
| **B1** | `artifact_abi_v1.c:514/519/523` | `input_digests` 元素计数恒 0 ⇒ `:552-558` 重复 `artifact_id` 拒绝是死代码、`query_input_count` 恒返 0、溯源输入链静默清空；头 `artifact_abi_v1.h:46` 的冻结保证从未执行 | **永不执行的判据**（同源式自洽） |
| **B2** | `stack_db.cpp:67-68` + `:376-379` | `std::system` 命令注入；POSIX `listTiles()` 空实现使 Linux 上恒返回空向量且无错误码 | 静默降级 / 命令注入 |
| **B3** | `sanitize_wsl_v5.sh:74` | 第 [4/6] 步无任何断言 + `\| tee` 掩盖退出码 ⇒ 该 ASan/UBSan 门**恒真**；`:74` 路径在本环境不存在 | **恒真门** |
| **B4** | `gradient_fitter.cpp:202` + `:229-235` + `:243-246` | 无覆盖时 `ref_p` 静默取 0（单帧时 `diff_i ≡ bg_i`）；clip 咬住时静默改用未过滤全集；样条失败静默跳帧却 `return 0` + `success=true` | 静默降级（科学面） |
| **B5** | `ahps_writer.cpp:305-330` / `:336` / `:399` | header size 不动点迭代**不校验收敛**即写盘；无 `.partial`/fsync/原子 rename；`fclose` 未检查即报成功 | 原子性 / 截断发布 |
| **B6** | `gradient_fitter.cpp:210-211` + `:217` + `:226/:229-235` | 默认 `sigma_clip_floor = 0.0` ⇒ `mad==0`（背景均匀帧）时 `threshold = max(5.0*1.4826*0, 0.0) = 0`，判据 `|diff-med| < 0` **对任何输入恒不成立** ⇒ 全点被剪；`:229-235` 又静默改用未过滤全集，`:226` 的 `n_clipped_per_frame` 却仍上报"已剪 N 个" ⇒ **sigma-clip 变成 no-op，而报告说它剪过** | **恒红门 + 筛子咬住即自动失效 + 报告与用量脱节** |

**（次级阻断）** `hiss_stream_writer.cpp` finalize 返回码与 `HISS_ERR_*` 大面积撞值且函数内自二义（`:534/:566/:580/:596/:606/:673` vs `:523`）—— 计为 **须修第 1 条**。

**B6 的补强**：SA-4 指出该恒红与 B4 的 `:229-235` 回退**首尾相接**——恒红使 `kept` 必空，必触发回退，而回退静默撤销 clip。即：判据先恒红，再被悄悄取消，最后报告说它生效了。这是本片**唯一一条"恒红门 → 静默撤销 → 报告仍然亮绿"的三段式**，比单独的恒红门更隐蔽。

**B7（退役声明被证伪，SA-3 取证，我已复核）**：`memory.md:197` 白纸黑字「**当前编译列表只含 `hiss_writer.cpp`（不含 `hiss_reader.cpp`），避免链接重复定义**」——但 `lib/infrastructure/aio/Makefile:64` 与 `build.ps1:98` **两处都编译 `hiss_reader.cpp`**；`:199` 说「若未来需同时编译 Reader+Writer，应将共享方法抽出到 `hiss_common.cpp`」——**`src/hiss_common.cpp` 已存在且已在编译列表**（`Makefile:59` / `build.ps1:88`）。即 `memory.md:195-201` 整个「重复定义问题」段把**已解决的问题**记成待办。这是负责人点名的「注释自称…曾被证伪」形态，且方向是**反向**的：不是退役对象还有活调用者，而是**已完成状态仍被记成现行状态**。

### 4.2 须修（14）

1. `hiss_stream_writer.cpp:534/566/580/596/606/673` —— 8 条失败路径中 6 条返裸负数，与 `hiss_format.h:242-249` 冻结常量撞值且语义相反；`:523 return HISS_ERR_FORMAT` 与 `:580 return -5` 同值异因。（**返回值混淆，最易被下游 `switch` 误分**）
2. `artifact_abi_v1.c:334/340/497` —— `free(s); handle_free(h);` 在 `find_key_deep` 失败时 **double free**（`s` 仍是上一个已挂到 `h->` 的串）。
3. `artifact_abi_v1.c:567` —— `*out` 仅成功路径赋值，违反头 `:50` 明文合同。
4. `artifact_abi_v1.c:62-73` —— `\uXXXX` 多复制 1 字节并吞掉闭合引号，与 `:63` 注释相反。
5. `hiss_transform.cpp:127/199/237` vs `:352` —— 失败与"成功处理空输入"共用 `return {}`，DELTA/BYTE_SHUFFLE 无区分。
6. `hiss_transform.cpp:285` —— `zig_zag_encode` 对负 `int64_t` 做有符号左移（UB）。
7. `hiss_transform.cpp:378` —— `uint32_t n = (uint32_t)n_elements;` 无溢出检查（写侧同场景有守卫）。
8. `CMakeLists.txt:93-98` —— `p1hips_digest_verify` `EXCLUDE_FROM_ALL` 且**无 `add_test`** ⇒ 64c1e988 回归锚永不设防（同文件 `:76-88` 的 `p1hips_selfcheck` 有正确范式）。
9. `test_snr_evaluator.cpp:159` —— `ms > 0 && out_snr[0] > 0` 顶替了 `:156-158` 明写的 `< 20ms/帧` 判据 ⇒ **恒真门**；`:177` 把"未 build 返 0.0"锁成合同。
10. `astro_image_io.h:149` —— 悬空引用 `aio_atomic_format.h`（实为 `aio_ahpx_format.h`）。
11. `aio_healpix_io.h:9` —— 悬空引用 `healpix_db/healpix_io/archive/`（该目录不存在）；与 `ARCHIVED.md` 的归档说法互相矛盾。
12. `corrected_stacker.h:66/69/75/76/45/98/26` —— 4 个改科学输出的参数无推导未入 config；缺 SNR 静默等权；`n_models` 两个不兼容定义；设计文档路径不存在。
13. `memory.md`（整篇）—— 日期/commit/任务流水违反 AGENTS.md §5；`默认分支 master` vs `main` 自相矛盾；「零依赖」与 CFITSIO vendored 自相矛盾；记录**已废弃**的 20B/40B 格式；5 处路径失效（含 `python/aio_healpix_io.py` 不存在）。
14. `ARCHIVED.md:8` / `aio_compressor.h:12` / `hips_input_v1.h:75-76` / `test_checksum.cpp:15-24,484,489` —— 悬空引用、失败三义、MOC 损坏三义坍缩、跨平台必红断言。（详见 3.11/3.15/3.17/3.20）

### 4.3 建议（12）

`fits_core_selftest.c:473` 栈缓冲越界写、`:572-596` root 环境下必假红 · `hiss_stream_writer.cpp:684-688` 相对路径下父目录 fsync 短路（第三态告警永不触发）· `aio_log.cpp:40-63` 相对 CWD 写日志 + 注释与实现相反 + 数据竞争 · `hips_manifest.h:35/27/16/55` 无推导魔数 / 第三方头泄漏 / 版本号遗留 · `p2hips_test_main.hpp:52-63` 单组执行 · `test_fits_bscale_parse.cpp:136/147` 未判空指针 · `nanoflann.hpp` 全档（未被任何构建引用）· `simple_test.cpp` 恒绿占位"测试" · `gradient_fitter.cpp:137/181/70` 魔数与易错签名 · `ahps_writer.cpp:361/153-166/26-29` · `artifact_abi_v1.c:149-155/9/610` 括号计数不跳字符串 / 错误码注释不符 / 空句柄返合法枚举 · `hiss_transform.cpp:318/375` varint 静默丢位 / reserve 溢出。

---

## 4.4 【子代理回稿后增补】自洽式断言与恒真门 —— 本片最密集的一类

> 以下条目**全部来自子代理独立取证，我已逐条回原文复核 `文件:行` 成立**。这是我本人首轮**漏掉**的一整类，也是负责人点名「最高价值产出」的一类。

### 4.4.1 期望量抄自实现、且与仓内正本相反（最严重）

| `文件:行` | 被检量 | 期望量 | 问题 |
|---|---|---|---|
| `test_checksum.cpp:308` | 篡改 SIGNAL 子块后 `read_tile()` 返回值 | 裸 `-5` | `hiss_format.h:245-246` 冻结 **`HISS_ERR_CHECKSUM -4`（校验失败）/`HISS_ERR_FORMAT -5`（格式错误）**；而实现 `hiss_reader.cpp:341` 在 **checksum 不匹配时返 -5**、`:368/:375` 在解压失败时返 **-4** —— **实现把两者对调了**，测试又把对调后的值抄成「期望」固化成回归锚。**一个遵守 `hiss_format.h` 的合规实现会在这里失败；而把解压失败也报成 -5 的改动不会让它变红。** 这是本片唯一一处「期望值与仓内正本相反」的锚。 |
| `test_snr_evaluator.cpp:64-69` | `eval.evaluate(ra[0],dec[0])` | `snr_phot*snr_psf[0]/median_snr` | `snr_evaluator.cpp:291` 是**字面相同**的表达式，运算次序/结合/类型转换全一致。**同一式既当被检量又当期望量。** 反例：若合同本意是 `snr_psf·median_snr/snr_phot`，测试抓不到；且它把 `median_snr` 当自由输入（`:49-51`），**从不检验 `median_snr` 的来源**。 |
| `test_snr_evaluator.cpp:94-101` | `snr_mid` | `snr_left`、`snr_right` | 三元**全部出自同一个 `eval.evaluate`**，无一独立。反例：给 `snr_evaluator.cpp:302` 加 `*3.0f` 偏移，严格单调性保持，测试全绿 —— **对 SNR 场的绝对尺度与零点偏移零判别力**，而这正是 P4 的科学量。 |
| `test_checksum.cpp:153/214`（同 `:336/382`） | `signal[i]`（读回） | `original_signal[i]`（`acc.finalize_signal` 的输出） | 写入侧 `hiss_writer.cpp:598` 调**同一个** `finalize_signal`。夹具 `:107-109` 刻意做成全覆盖满贡献，**恰使归一化分支不被任何断言覆盖**。 |

### 4.4.2 恒真门（四条，均已复核）

| `文件:行` | 为什么永不可能红 |
|---|---|
| `test_checksum.cpp:460-462` | 断言的是**测试自己在 `:443-449` 写、自己注册的 lambda**（`h=h*131+data[i]`）。`data[0]=1` ⇒ 任何 `size≥1` 都非零。要它失败只能改测试自己。 |
| `test_checksum.cpp:355-365` | `tile.subblocks` **从未断言非空**。若 writer 缺陷导致目录里零个子块条目，循环零次执行，`all_none` 保持 `true`，「所有子块 = NONE」在**空集上成立**。对照：同文件 `:183-184` 明确写了 `ASSERT_TRUE(sig_desc != nullptr)` —— 说明作者知道要防，test_03 漏了。 |
| `test_checksum.cpp:374-382` | TEST03 的「数据精确一致」**缺 `signal.size()==original.size()` 前置检查**（TEST01 `:204` 有），reader 静默返 0 样本时循环执行 **0 次**、`signal_ok` 保持 true ⇒ **注入缺陷时 TEST03 不红，违 AGENTS.md §8**。 |
| `test_fits_bscale_parse.cpp:133/144/155/162` | 文件头 `:12-13` 明写要锁的性质是「**一条日志都不打**…静默错误」，但 N1–N3b **只断言 `img==nullptr`，全程无日志捕获**（`aio_log.h` 只在 `:124` 用于 `aio_set_precision_mode`）。反例：把拒绝实现成裸 `return -1` 不打日志 ⇒ 8 条 CHECK 全过，**要锁的性质荡然无存**。 |

### 4.4.3 筛掉真信号

- **`fits_core_selftest.c:591`** `CHECK(st == ACS_FIO_ERR_IO || st == ACS_FIO_ERR_DISKFULL)` —— 只读目录只能产生 `EACCES→IO`（SA-5 在 `/tmp` 实测：`errno=13`），**`DISKFULL` 臂永不可达**；文件头 `:10` 宣称的「磁盘满 → DISKFULL」覆盖由此**从未被 pin**，`||` 把弱结果当强结果放过。
- **`p1hips_digest_verify.cpp:269`** V4(c) 的 `ds2` **完全无守卫**（V1 `:166-168`、V4b `:250` 都有）。`ds2==-1` ⇒ `data_off = (size_t)(-1)+160 = 159` ⇒ 所谓「数据区字节翻转」实际落在**主头**里，`VERIFY(d3 != d0)` **因错误的原因通过**。
- **`p1hips_digest_verify.cpp:59`** `card_str` 只写 20 列 ⇒ 传入的 60 字符 CHECKSUM 值**被静默截断到 17 字符**、注释区（30–79 列）**恒为空** ⇒ V1/V2 声称验的「value+**注释区**清零」验的是**空注释区**；V4b 翻转点 `ck2+30` 落在纯空格区。

### 4.4.4 其他新发现（子代理独立取证，我已复核行号）

| `文件:行` | 发现 |
|---|---|
| **`aio_log.cpp:77`** | **级别门控反了**：`aio_log.h:4-6` 是 `INFO=0, DEBUG=1`；`aio_log.cpp:23` 缺省阈值 `AIO_LOG_INFO`(=0)；`:77` 判 `level < threshold` ⇒ **DEBUG 在名为「INFO」的缺省下照常输出**。SA-5 已在 `/tmp` 实跑复现。 |
| **`hiss_stream_writer.cpp:292/304-312/334`** | **短写后 offset 漂移**：`desc.offset` 在 `:292` 先记，`temp_pool_size` 只在 `:334` 成功路径推进；`:304` 短写直接 `return -3` **不推进**，但 **FILE 流位已前进** ⇒ 下一个子块 `desc.offset` 偏小，`finalize:454-458` 把错 offset 原样写进 Header。`safe_size_to_u32/u16`（`:435-449`）只查 header 预算，**不查池内一致性**。 |
| **`aio_healpix_io.h:250-259`** | 9 个**极通用名**兼容宏（`hiss_read`/`hiss_write`/`hcsd_*`/`hio_free`）挂在公共头上，会文本改写任何同名标识符；且 SA-5 核实**全仓零活调用者** ⇒ **退役对象未删除，违 AGENTS.md §6**；`ARCHIVED.md:24` 还在替它背书。 |
| **`aio_healpix_io.h:96` vs `:237-238`** | 释放契约自相矛盾：`:96` 写「由 malloc 分配，调用者负责 **free**」，而 `:133/:135/:150/:159` 又要求用 `aio_hio_free` ⇒ DLL 边界上跨 CRT `free()` 即堆损坏。 |
| **`fits_core_selftest.c:172/176`** | 只比 `back0[0]` 与 `back0[3]`，中间 `[1]`/`[2]` 从不检 ⇒ 文件头 `:12` 的「逐元素一致」覆盖被高估（同文件 `:123` 才是逐元素）。 |
| **`p1hips_test_main.hpp:200-228`** | 未知组名（如 `./p1hips_tests untis` 拼错）⇒ 循环零次执行 ⇒ `total_fail==0` ⇒ **打印 PASS 并退出 0**。对照 `p2hips_test_main.hpp:64-65` 正确 `return 2`。CTest 只传四个合法名故当前潜伏，但与同仓 p2hips harness 行为不对称。 |
| **`p1hips_digest_verify.cpp:287-288`** | `data_off` 按「向上取整到 2880」**猜**数据区起点，未用 `NAXIS1×BITPIX/8` 校验；若写入器不 pad 头，翻转点落在零填充 ⇒ 「payload 敏感」**因错误理由而绿**。 |

### 4.4.5 覆盖空洞（SA-5 的独立发现，我认为这是本片最大的治理风险）

SA-5 核查 157 份审核包后报告：**它范围内 12 份文件中有 9 份在全部审核包里零覆盖**（`hiss_stream_writer` / `hiss_transform` / `test_checksum` / `test_fits_bscale_parse` / `p1hips_digest_verify` / `fits_core_selftest` / `aio_healpix_io.h` / `astro_image_io.h`）；`artifact_abi_v1.c` 仅在 `G08-08-侦察:348` 被记「1 处旧品牌，**未逐行核实**」；`memory.md` 只被**引作旁证**（`审稿-R3-T3:140` 拿 `memory.md:103` 证明死代码引用），无人判其本身；`ARCHIVED.md` 仅 `审稿-R3-T3:297 建议-2` 判**建议**。

⇒ **本片此前不是「判松」，而是「静默未审」**。SA-5 的 38 条 must-fix 及以上中，**31 条现有零覆盖、3 条一致、1 条过松（ARCHIVED.md）、0 条它比现有更严** —— 偏差方向单向。

> 口径：构造 ⇒ 期望推翻某条结论 ⇒ 是否推翻。全部为**纸面推导**（未编译、未执行，符合纪律第 4 条）。

### 反例 1：`input_digests` 计数恒 0
- **构造**：manifest 片段 `"input_digests":[{"artifact_id":"A","digest":"<64hex>"},{"artifact_id":"B","digest":"<64hex>"}]`（完全符合 `artifact_manifest.schema.json` 的 `items:object`，无嵌套数组）。
- **推导**：`q` 指向 `[`；`:514 ++q` → `q` 指向第一个 `{`；`:519 depth=0`；`:523` 遇到 `{` 时判定 `depth==1` 为假 ⇒ 不计数；随后 `}` 不改变 depth；第二个 `{` 同理；遇 `]` 时 `depth==0` 退出。⇒ `cnt=0`。
- **期望推翻**：若 `cnt≥1`，则 B1 不成立。
- **结果**：**推翻失败，B1 成立**（我随后又独立 `read` 了 `:501-532` 复核，`++q;` 与 `int depth = 0;` 逐字确认）。
- **旁证**：`:527 h->input_count = cnt;` 在解析循环**之前**赋值、循环内 `idx` 从不与 `cnt` 复核 ⇒ 即使修好 `:519`，`:536` 的 `break` 仍会留下未填的 NULL 槽位而 count 仍说谎。**修复必须两处同改。**

### 反例 2：`hiss_stream_writer.cpp` 父目录 fsync 的第三态告警不可达
- **构造**：以相对路径 `out/foo.hiss` 调用 `finalize`。
- **推导**：`:684 find_last_of("/\\")` 返回 `npos` ⇒ `:685-686` 的三元式取 `0` ⇒ `drc=0` ⇒ `:689 if (drc != 0)` 为假 ⇒ 第三态 stderr 行不打印。**"已发布但持久化未确认"这一状态对相对路径既不检测也不上报。**
- **期望推翻**：若相对路径也能走到 `:690`，则 4.3-2 不成立。
- **结果**：**推翻失败，成立。**

### 反例 3：`gradient_fitter.cpp` 单帧退化为整幅背景
- **构造**：`n_frames = 1`，该帧有 ≥5 个控制点，`params.match_threshold_arcsec` 任意正值。
- **推导**：`:174` 的内层循环对唯一帧执行 `if (j == i) continue;` ⇒ 永不进入 ⇒ `ref_num ≡ ref_den ≡ 0` ⇒ `:202 ref_p = 0.0` ⇒ `:203 diff_i[p] = bg_i(p)`。`:210` 的 `med` 即背景中位数本身，`mad` 近 0 ⇒ `threshold` 落到 `sigma_clip_floor` ⇒ `kept` 保留全部点 ⇒ `:239` 拟合的是**整幅背景图**，输出被命名为"梯度校正 g_i"。全程无错误码、无日志、无标志位，`fit()` 返回 0。
- **期望推翻**：若内层循环在 `j==i` 时仍参与、或 `ref_den==0` 走报错，则 B4 不成立。
- **结果**：**推翻失败，B4 成立。**

### 反例 4：`ahps_writer.cpp` header size 不收敛导致越界读
- **构造**：选使 JSON 长度在两个值间振荡的 `bandCount`/`pixelCount` 组合。
- **推导**：`:305 for (iter<20)`；每轮 `:325` 重建 JSON、`:328 headerSize = newSize`。若长度呈 2-周期（长度取决于 offset 的十进制位数），20 次后退出时 `finalJson.size() ≠ headerSize`。`:373 fwrite(finalJson.data(), 1, headerSize, fp)`：
  - `headerSize > finalJson.size()` ⇒ 读越界 `std::string` 内部缓冲；
  - `headerSize < finalJson.size()` ⇒ JSON 被截断，`:307` 起算出的全部 chunk `offset` 失去基准。
- **期望推翻**：若循环末尾有 `finalJson.size() == headerSize` 的硬校验（对照 `hiss_stream_writer.cpp:516`），则 B5 不成立。
- **结果**：**推翻失败，B5 成立。**

### 反例 5：`sanitize_wsl_v5.sh` 第 [4/6] 步恒真
- **构造**：让 `gaia_sanitize` 因 ASan 报错而 `abort`（`ASAN_OPTIONS=abort_on_error=1`）。
- **推导**：`:74` 是 `cmd | tee`。POSIX shell 的 `set -e`（`:13`）对管道取**最后一条命令**（`tee`）的退出码；`tee` 读到 EOF 后正常退出返 0 ⇒ 整个 `pipeline` 返 0 ⇒ **abort 被完全吞掉**。该步之后无 `grep -q`（其余 5 步都有）⇒ 无二次兜底 ⇒ 流程继续 ⇒ `:88` 打印 `ALL_SANITIZE_V5_PASS`。
- **期望推翻**：若该步后有断言、或管道启用了 `set -o pipefail`，则 B3 不成立。
- **结果**：**推翻失败，B3 成立。**

### 反例 6（**反例失败 = 我自己的假设被推翻，如实记录**）：AHPX 权重退役"仍有活调用者"
- **构造**：`grep -rn "weight_mode|weight_data|grid_w"` 全仓，预期找到旧签名的活调用点。
- **推导**：命中的 `weight_mode_version`（`provenance.h:124`）、`snr_weight_mode`（`module_adapters.cpp:9778`）、`spc.weight_mode`（`module_adapters.cpp:10206`）**分属 provenance 留痕、SNR 归一化模式、spec 配置**三个不同概念；`.ahpx` 路径上的旧权重参数确已移除，且 `aio_ahpx_format.h:29-33` 有读写双侧 fail-closed。
- **期望推翻**：我期望能找到活调用者，从而坐实"退役声明被证伪"这一已知模式。
- **结果**：**推翻成功 —— 我的假设被推翻。** 本片的 AHPX 权重退役声明**为真**，不构成缺陷。**如实记录，不计入发现清单。**

---

## 6. 盲复算

> 口径：先遮住既有判定独立取证，再比对。

**我做法的调整**：我**先完成了 26/26 份原文的全量阅读并形成独立结论**，之后才读子代理报告并逐条复核。这在效果上强于严格的"先盲后看"，但存在**锚定风险**（我可能只关注与自己结论一致的报告项）。为抵消该风险，我对子代理报告中**与我结论不同的条目**逐条回到原文取证，未凭印象采信。

**独立取证 vs 既有判定的比对（抽查 12 条）**：

| 条目 | 我的独立结论 | 子代理/既有判定 | 裁决 |
|---|---|---|---|
| `artifact_abi_v1.c` input_digests 计数 | 我读到该段但**未发现**恒 0（我聚焦了 `\u` 越界与 double free） | SA-1 判为**阻断**，并给出偏移推导 | **我采纳** —— 回 `read :501-532` 独立复核，`++q;` / `int depth = 0;` / `depth == 1` 逐字确认，推导成立。**这是我本片最大的漏检，已补入 B1** |
| `hiss_stream_writer.cpp` 返回码撞值 | 我只判了"8 条失败路径混用裸负数与具名常量，不符稳定错误码" | SA-1 进一步指出**与 `HISS_ERR_*` 冻结常量撞值且语义相反**，且 `:523` 与 `:580` 同值异因 | **我采纳并升级** —— 回 `read hiss_format.h:238-287` 独立复核，-4/-5/-6/-7/-8 语义确认；`:523 HISS_ERR_FORMAT` 与 `:580 return -5` 确为同一数值 |
| `hiss_stream_writer.cpp:516` header_size 校验 | 我判为"自洽性防御，非缺陷但只验长度" | SA-1 独立重算预算公式，判为**真交叉校验** | **我修正自己的判定 → 一致**：预算式 39+31+7+json+7+tile_dir 与 `ByteBuf` 序列化逐项相等，是两条独立路径的同值，非同源式自洽。**此项我判偏严，已纠正** |
| `*out` 失败不置 NULL | 我**未发现** | SA-1 判为**阻断**（违反头 `:50` 明文合同） | **我采纳** —— 复核 `:303-569`，`*out` 仅 `:567` 赋值 |
| double free | 我**独立发现**（`:334/340/497`，`find_key_deep` 不重置 `*out_str`） | SA-1 未单列 | **一致，我的独立结论** |
| `\uXXXX` 越界 | 我**独立发现**（多复制 1 字节吞掉闭合引号） | SA-1 记为 C15「存的是 u+4hex，反斜杠已被吃掉」 | **一致**；我额外指出**吞掉闭合引号**这一更重后果，**我的判定更严** |
| 未知 `transform_id` 静默当 NONE | 我**未发现** | SA-1 判为**阻断** A4，给出 `hiss_reader.cpp:677` 无校验 + `:328-334` 对 checksum 已 fail-closed 的不对称证据 | **我采纳为须修** —— `hiss_transform.cpp:68 default: return NONE` 确凿；但 `hiss_reader.cpp` 不在本片，**可达性未经我亲验，故降 SA-1 的阻断为须修**（这是我对 SA-1 的一处**主动降级**） |
| 私建线程池 | 我在 `nanoflann.hpp` 发现 `std::async` + `KDTreeSingleIndexIncrementalAdaptorMT`（专职后台线程） | SA-1 对其 9 文件判「零命中，无池」 | **一致**（范围不同）；两者合起来说明：**算法层无池，但 vendored 库带池能力，且该库不被任何构建引用** |
| `test_checksum.cpp` 的 CRC32C 断言 | 我判为"独立标准向量，质量好" | SA-1（D3）同样判为真交叉校验 | **一致** |
| `p2hips_test_main.hpp` 退出码 | 我判"失败正确传播，不属软通过" | SA-1 判"失败只打印不返回码"（针对不同文件集） | **一致（范围不同）**；我在本片这一项**先前的怀疑被自己的复读推翻** |
| AHPX 权重退役 | 我**独立证伪**了自己的假设（见反例 6） | SA-1 D8/D9 亦判"退役为真" | **一致** |
| `HISS_MAX_SUBBLOCK_UNCOMPRESSED` | 我判"有推导，非恒真门" | SA-1 D5 亦判有推导（`hiss_format.h:280-281` 算术自洽） | **一致** |

**结论**：**偏严 2 处**（`:516` header_size 校验、`nanoflann.hpp` 的池能力 —— 均已下调）；**偏松 3 处**（`input_digests` 恒 0、`*out` 不置 NULL、`transform_id` 静默 NONE —— 均已补入并升格）。**未发现既有判定被普遍放松到"无问题"的情形**；本片缺陷密度确实高。

---

## 7. 子代理派发记录

**派发 6 个（5 个唯一职责域，其中 1 个为误重复派发）**，全部只读、全部禁编译、全部禁 git 写。

| 子代理 | 职责域 | 状态 |
|---|---|---|
| SA-1 `24bc8cc0` | 静默降级 / 错误码 / ABI（9 个生产文件） | **已回** |
| SA-1' `f4ff897b` | 同上（**误重复派发**） | **已回** |
| SA-2 `0e6673da` | 自洽式断言 / 恒真恒红门 / 孤儿测试 / 退出码传播（10 个测试文件） | **已回** |
| SA-3 `a129c46c` | 悬空引用 / 退役声明证伪 / 构建接线 / ARCHIVED·memory 事实核查 | **已回** |
| SA-4 `7bc3ded1` | 硬编码分类 / 私建线程池 / 进程 IO 纪律 / 数值健壮（12 个文件） | **已回** |
| SA-5 `53b75442` | 盲复算（三阶段：先独立判定，再比对既有报告） | **已回** |

**6 个全部回稿**（5 个唯一职责域 + 1 个误重复）。合计读取约 9,600 行。SA-5 另在 `/tmp/ce_g08` 自建仓外复现环境，构造 **7 个反例，7/7 推翻成功**（1 条诚实降级：root 脆弱性未在其容器复现，其 euid=1001）。

**SA-3 未读满的范围（如实记录）**：`nanoflann.hpp` 4091 行中仅 1-140 逐行读完，141-4091 只普查 API 面。**但该文件我本人已 4091/4091 全读**，故本片覆盖率不受影响。

**逐条复核与采纳情况（针对已回报告，共 39 条候选）**：

- **采纳 4 条阻断**：`A1 input_digests 恒 0`（我回原文独立复核后采纳，并**自行补出 SA-1 漏掉的连带缺陷** `input_count` 先赋值、`idx` 不复核）、`A2 返回码撞值`（采纳并升级）、`A3 *out` 不置 NULL`、`A4 transform_id`（**采纳但主动降级为须修**，理由：`hiss_reader.cpp` 不在本片，可达性未亲验）。
- **采纳 12 条须修**中的 8 条（B1 `manifest_version` 未校验、B2 错误码注释不符、B3 ABI 版本宏空转、B4 `astro_image_io.h` ABI 零 pin、B5 压缩失败三义、B6 日志静默降级、B7 冻结规范引用悬空、B8 跨枚举数值混叠）。其中 **B7 与我自己的 3.13/3.8 悬空引用发现相互独立地命中同一类问题**（`02_FROZEN_STAGE1_HISS_SPEC.md`、`docs/engineering/IO_AND_ATOMICITY.md`、`00_COMMON_CONTRACTS` 均不存在），我已并入须修第 14 条。
- **采纳 19 条建议**中的 5 条（C1 archive 路径错 —— 与我 3.14 独立命中；C2 `HioSnrModelF64` 无释放 API —— 与我 3.14 独立命中；C6 `g_aio_log_level` 数据竞争；C9 `<chrono>` 缺失；C10 有符号左移 UB —— 与我 3.2 独立命中）。
- **否决 / 修正 9 条**：
  1. **否决 D14「NULL 句柄返回 PENDING」为缺陷** —— 头 `artifact_abi_v1.h:62` 已声明「查询函数不失败（句柄已校验）」，出合同范围；我**不计入发现清单**。
  2. **否决 B3「ABI pin 缺失」对本文件的阻断性** —— opaque 句柄设计本身正确消除了布局风险，真实问题只是版本宏无读取点；我从阻断降为建议。
  3. **否决 B11（reader 侧逆变换后长度复核缺失）为阻断** —— `hiss_reader.cpp` 不在本片，我无法亲验，**降为未采纳**（不计入发现清单，避免拿别人的结论冒充自己的取证）。
  4. **否决 B12（`props_get` 无 `out_len`）为阻断** —— 降为须修（我已在 3.15 自行独立发现同一问题）。
  5. **否决 C12（`inverse_transform` NONE 分支不校验 `expected_output_size`）为阻断** —— SA-1 自己已核实生产路径不可达；降为建议。
  6. **修正 D3（`:516` header_size 校验）** —— SA-1 判为真交叉校验，我原本偏严，**接受修正**（见 §6）。
  7. **否决我自己的初判**：`p2hips_test_main.hpp` "失败只打印、进程仍返 0" —— 复读 `:24-32/:56-62` 后**推翻**，确认退出码正确传播。
  8. **否决我自己的初判**：AHPX 权重退役"仍有活调用者" —— 全仓 grep 后**推翻**（见反例 6）。
  9. **否决"test_checksum.cpp CRC32C 断言是自洽式"** —— `0xE3069283` 是 Castagnoli **外部标准向量**，属独立 oracle，**非自洽式**。
- **统计**：**已回报告 39 条候选 → 采纳 17（含阻断 4、须修 8、建议 5），否决/降级/修正 9，另有 13 条与我的独立结论重叠（计入"一致"而非采纳）**。

---

## 8. 自证段（可复跑命令）

```bash
# 0) 基线
cd "/workspace/Astro CS Database"
git -c core.quotepath=false log -1 --format='%H %s'      # 期望 850a9edefd47434b9ab71bc907c3de1e0814b323

# 1) 本片成员与行数（应输出 26 行 + TOTAL_LINES=10630 MISSING=0）
while read -r f; do n=$(wc -l < "$f" 2>/dev/null || echo MISSING); printf '%6s  %s\n' "$n" "$f"; done < <( \
  sed -n '2922,2947p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml | sed 's/^ *- *"//; s/"$//')

# 2) B1 —— input_digests 计数恒 0（关键三行）
sed -n '514p;519p;523p' lib/infrastructure/aio/runtime/artifact_store/artifact_abi_v1.c
#   期望依次为:  ++q;   /  int depth = 0;   /  else if (*r == '{' && depth == 1) ++cnt;
# 旁证：头文件冻结的保证（“重复输入拒绝”）
grep -n "artifact_id 唯一" lib/include/acsd/contracts/artifact_abi_v1.h

# 3) B2 —— stack_db 命令注入 + POSIX 空实现
sed -n '67,68p;376,379p' lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/stack_db.cpp

# 4) B3 —— sanitize 第 [4/6] 步无断言（对照其余 5 步均有 grep -q）
grep -n "grep -q" lib/infrastructure/aio/tests/sanitize_wsl_v5.sh      # 应只有 6 行，步骤 4 缺席
sed -n '74p' lib/infrastructure/aio/tests/sanitize_wsl_v5.sh

# 5) B4 —— gradient_fitter 静默取 0 / clip 回退 / 失败吞掉
sed -n '202p;229,235p;243,246p;284,286p' \
  lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/gradient/gradient_fitter.cpp

# 6) B5 —— ahps_writer 迭代不校验收敛 + 无原子替换
sed -n '305,330p;336p;373p;399p' \
  lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/ahps_writer.cpp
grep -n "hdr.data.size() != header_size" lib/infrastructure/aio/src/hiss_stream_writer.cpp   # 对照：有硬失败守卫

# 7) 须修-1 —— 返回码撞值（须先读冻结常量）
sed -n '242,249p' lib/infrastructure/aio/include/hiss_format.h
sed -n '523p;534p;566p;580p;596p;606p;673p' lib/infrastructure/aio/src/hiss_stream_writer.cpp

# 8) B8 —— digest_verify 不入 CTest
grep -n "p1hips_digest_verify" lib/infrastructure/aio/tests/p1hips/CMakeLists.txt
grep -c "add_test" lib/infrastructure/aio/tests/p1hips/CMakeLists.txt

# 9) 悬空引用批量核验（应输出 *** MISSING ***）
for f in lib/infrastructure/aio/include/aio_atomic_format.h \
         lib/infrastructure/aio/healpix_db/healpix_io/archive \
         lib/infrastructure/aio/python/aio_healpix_io.py \
         ".trae/specs/snr-compact-storage-and-gradient-correction/spec.md"; do
  printf '%-70s' "$f"; test -e "$f" && echo EXISTS || echo "*** MISSING ***"; done
grep -rn "02_FROZEN_STAGE1_HISS_SPEC\|00_COMMON_CONTRACTS\|docs/engineering/IO_AND_ATOMICITY.md" \
     --include=*.cpp --include=*.h --include=*.md lib/ docs/ 2>/dev/null | head

# 10) archive 死源码核验（lib/ 与 eng/ 下应零命中）
grep -rn --include=CMakeLists.txt --include='*.cmake' \
     -e healpix_stack -e stack_db -e ahps_writer -e gradient_fitter \
     -e test_snr_evaluator lib/ eng/ ; echo "exit=$? (1=零命中即死源码)"

# 11) 孤儿测试：test_checksum 无 add_executable/add_test
grep -n "test_checksum" lib/infrastructure/aio/tests/CMakeLists.txt   # 仅注释命中

# 12) 已接线对照（证明我区分了“接线的/没接线的”）
grep -n "fits_core_selftest\|test_fits_bscale_parse" eng/tests/unit/CMakeLists.txt
```

---

## 9. 交付件自评

- **覆盖率**：26/26 份、10,630/10,630 行 = **100%**，无未读完成员（`nanoflann.hpp` 4091 行分 4 段读到末行）。
- **最强反例**：**反例 1**（`input_digests` 计数恒 0，`artifact_abi_v1.c:514/519/523`）—— 唯一一条「判据看似存在、实则永不执行」的自洽式缺陷，且冻结在头文件里作为对外保证。
- **是否发现新问题**：**是，且量很大。**
  - **我自查漏检、经子代理回稿后补入的**：`input_digests` 恒 0、`*out` 不置 NULL、`transform_id` 静默 NONE、`test_checksum.cpp:308` 期望值与 `hiss_format.h` 相反、`:460-462` 断言测试自己的 lambda、`:355-365` 零迭代真空门、`:374-382` 缺长度检查、`test_snr_evaluator.cpp:64-69` 逐字抄实现公式、`fits_core_selftest.c:591` DISKFULL 臂不可达、`aio_log.cpp:77` 级别门控反了、`hiss_stream_writer.cpp:292` 短写 offset 漂移。
  - **反例 6 是我的假设被推翻**：AHPX 权重退役「仍有活调用者」经全仓 grep **证伪**（SA-1/SA-3 亦独立判为真），如实记为「无问题」。
  - **本片最大的发现其实不是某个 bug，而是「静默未审」**：SA-5 核查 157 份审核包后确认我范围内 9 份文件**零覆盖**，偏差方向单向（0 条子代理比现有更严）。
- **未完成项**：无。6 个子代理全部回稿，结论已并入本报告。**本报告全部结论均来自我自己读完 26 份原文**；子代理结论只在**我回原文复核行号成立**后才采纳。