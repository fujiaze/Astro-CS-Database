# G08-05 对抗审稿 P1 · 第 1 遍 · 片 `INF-aio-002`

- **层**：`lib/infrastructure/aio`（生产源码）
- **基线**：任务单给 HEAD=`850a9ede`；**实际工作树在审稿期间漂移**，交付时为 `1fa477a7`。所有关键行号已在 `1fa477a7` 复验（见 §8）。漂移事实记为 `UNRESOLVED-BASE`。
- **纪律**：全程只读；零 git 写；未编译、未跑 ctest/pytest/任何二进制；未读 `/tmp/acsd_g08/`；未改仓内任何文件。
- **口径**：结论全部来自本人逐行通读原文 + 5 个子代理独立通读 + 本人复验。凡本轮亲自重算的条目标 `[本人]`；凡只由子代理提出、本人已复验行号但未逐行重推的条目标 `[SA-n]`。

---

## 1. 读完了吗

| 口径 | 数值 |
|---|---|
| 成员份数（权威清单） | **26** |
| 实际读了多少份 | **26** |
| 成员总行数（权威清单 `实际行数: 10630`） | **10630** |
| 实际读了多少行 | **10630** |
| **覆盖率** | **26/26 份，10630/10630 行 = 100.0%** |

**未读完的：无。** 本片 26 个成员文件全部由本人用 `read` 工具从头到尾逐段读完（`aio_hips_writer.cpp` 2935 行分 7 段、`test_query_pixel.cpp` 1170 行分 2 段，其余整篇单读）。`grep` 仅用于候选定位与交叉取证，未替代通读。

**附带通读的非本片成员**（取证用，不计入覆盖率）：`src/hiss_reader.cpp:140-259`、`lib/algorithms/shared/healpix/healpix_core.cpp:285-318`、`src/ahpx/aio_ahpx_reader.cpp:566-596`、根 `CMakeLists.txt`、`eng/tests/unit/CMakeLists.txt` 相关段。

---

## 2. 本片判定

### **判定：阻断**

理由：本片同时命中「静默降级」「恒真门」「自洽式断言」「筛掉真信号」「悬空引用」「退役对象声明被证伪」六类，且**校验面本身失效** —— `aio_hips_verify_product_set` 与 `verify_fits_*` 这两道闸门在多条路径上对残缺产品签发绿灯。即「看到检查通过」在本片**不构成任何证据**。

### 最重 3 条

**【阻断-1】`metadata.fits` 是全片唯一非原子写的 FITS，且创建失败被整块静默吞掉 —— 硬失败降级为 rc=0**
`aio_hips_writer.cpp:1994-2011`。`if (!fits_create_file(&fptr, mp.c_str(), &status))` —— `fits_create_file` 返回 status，0=成功。**失败时（status≠0，典型 ENOSPC/EACCES/只读卷/残留文件导致 `FILE_EXISTS`）条件取假，整块被跳过**：不 `set_error`、不 `return false`，控制流直接落到 `:2019` 的 MOC 写、`:2020 return true`，`finalize` 上层据此把子产品报成 finalize 成功。同块内 `:2005-2009` 五次 `fits_write_key_*` 与 `:2010` `fits_close_file` 返回值全不查。加重：`:1994 std::remove` + 直写**正式路径**，与本文件 `:442-459` 注释自己宣称已修复的形态（「直写正式路径 ⇒ 留下截断的半成品」）**逐字同型**。`[本人]`

**【阻断-2】`aio_hips_verify_product_set` 对「signal 打不开 / manifest 缺失」fail-open —— 完整性闸门对残缺产品签发绿灯**
`aio_hips_writer.cpp:2645-2649` 的 `if (ds) { … }` **无 else、无 set_error**：`aio_hips_open(SIGNAL)` 返回 nullptr 时 `n_signal_tiles` 停在初值 `-1`（`:2631`）。随后 `:2693` 与 `:2768` 的守卫都是 `out->n_signal_tiles > 0`，`-1 > 0` 为假 ⇒ **V3 的 tile 数一致性、V4 的 nrej/nused tile 数一致性两条断言双双被跳过**；`:2690/:2700` 的 `uncertainty_available` 停在 `-1`（`:2633`）⇒ **V3 两个分支都不进**；`:2710` 的 `if (file_exists(manifest_path))` 同样**无 else** ⇒ `mdoc` 空 ⇒ `:2787` 的 V6 双写一致性整段跳过。**一条 assert 都不触发 ⇒ `:2823 return 0`。**
构造反例（已逐行走查）：写完 `signal/`+`support/` 后进程被 kill（manifest.json 未落）→ verify 返回 0，且报告内容为 `signal_present=1, n_signal_tiles=-1`。
**同一模块内方向相反**：读侧 `aio_hips_reader.cpp:408-421` 明确写「§10: 完成清单 fail-closed」，对 manifest 缺失严格拒绝。**写侧自带的 verify 面是唯一漏网面，而它才是消费侧真正的闸门。** `[本人]`

**【阻断-3】`test_query_pixel.cpp` 的「独立 oracle」是生产实现的逐字副本 —— 球面变换判别力为零**
`test_query_pixel.cpp:93-170` 的 `oracle_ang2xy` / `oracle_xy2nest` / `oracle_radec_to_nested_ipix`，与生产 `src/hiss_reader.cpp:160-249` 的 `hpx_ang2xy` / `hpx_xy2nest` / `radec_to_nested_ipix` **除标识符改名外逐字相同**（4 个常量 `kPi/kTwoPi/kHalfPi/kTwoThirds` 同值）。测试文件 `:78` 自己承认「复制自 hiss_reader.cpp 内部 static 实现」，而文件头 `:11` 宣称「不替代生产 query_pixel … 两条独立生产路径交叉验证」。
**反例**：设 `hpx_ang2xy` 极冠区 north/south 判定或赤道带四分支 offset 取错 ⇒ oracle 与 SUT **同错** ⇒ `find_tile_pixels` 用错映射找 ra/dec ⇒ `query_pixel` 用同一错映射算出同一个 local_ipix ⇒ 读出该下标的元素，而该下标的期望值正是测试自己按该 local_ipix 写进去的 100/180/250 ⇒ **全绿**。
`:527-532`「query_pixel 与 read_tile 交叉验证」救不了这个洞：`read_tile` 按 parent_ipix 直接索引，**不经过球面变换**。仓内本就存在独立实现 `lib/algorithms/shared/healpix/healpix_core.cpp:77-149`（极冠区走 `cos/sqrt(1+z²)` 路径，与 reader 的 `sqrt(3(1−z)·…)` 不同源），测试未用。
**本人复核方式**（可复跑，见 §8）：`sed` 抽出两段去掉注释与前缀后 `diff`，差异**仅剩尾随注释与缩进**，可执行语句零差异。`[本人]`

---

## 3. 逐文件清单

26 份全列。每份给出「读了什么 / 看到什么 / 判定」。

### 3.1 `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（2935 行）— **阻断**

**读了什么**：全文 7 段连续读完（1-400/400-849/849-1299/1299-1749/1749-2199/2199-2599/2599-2935）。ABI 前置校验族、原子落盘链、hierarchy 稀疏累加器、三态表、MOC/manifest/properties/SNR 各写出面、`verify_product_set` 全部 V1-V6 门。

**看到什么**：
1. **`:1995` metadata.fits 创建失败整块静默跳过 + `:2005-2010` 返回值不查 + 直写正式路径** → 阻断-1。
2. **`:2645`/`:2676`/`:2710` 三处 `if(...)` 无 else 把读侧失败吞成「不可得」哨兵，配合 `:2693`/`:2768`/`:2690` 的 `>0`/`==1`/`==0` 守卫 ⇒ 断言从「判红」退化为「跳过」** → 阻断-2。
3. **`:2058-2072` SNR `.tsv` 用裸 `fopen/fputs/fprintf/fclose`，`fclose` 返回值丢弃，且非原子落盘** —— ENOSPC 在 `fclose` 才暴露却被当成功；同目录 sibling（properties `:714`、`metadata.xml` `:2131`、`Moc.fits` `:2167`）全走了 `aio_atomic`，唯独它没有 → 须修。`[本人]`
4. **`:1481` `if (!v || !(area > 0.0) || !std::isfinite(flux)) continue;` —— 筛掉真信号（本片唯一一条 5 个子代理均未命中的发现）**：叶级在 `:1366-1383` 刻意把「覆盖」与「信号可用」**解耦**（有覆盖 ∧ flux 非有限 ⇒ `sig=NaN ∧ sup>0 ∧ area_true=area`），`:1047-1049` 的注释还专门写「父级 support 也必须在『有覆盖但信号不可用』时发布」。但 hierarchy 累加处 `flux = sig_n[i]*area_n[i] = NaN*area = NaN` ⇒ 被 `continue` **连同 area 一起丢掉**。⇒ **同一产品内叶面与父面 support 不自洽**：叶 tile 发布 `sup>0`，父 tile 该像素 `sup=0`（`areaD` 从未累加）。本轮 5 个子代理全部聚焦恒真门/退役核查，**无人命中此条**。`[本人]`
5. `:1947` `std::to_string(covered_frac)`（6 位小数）vs `:2457` `%.8f`（8 位小数）—— 同一 `acsd_covered_sky_fraction` 在 properties 与 manifest **写出两个不同字面量**；`:1846-1852` 的注释正是把这一族缺陷作为 bug 记录并为此建了 `fmt_sky_fraction`，但**紧邻的同一个键没跟着修** → 须修（仓内有可照抄的先例）。`[SA-1/SA-2]`
6. `:2471-2474` manifest 的 `nrej_tiles/nused_tiles` 用 `ps->flags ? leaf_ipix_list.size() : 0`，而同一 JSON 的 `products` 列表用 `ps->diag_nrej_tiles`（真值源）—— **同一产物两个真值源**；调用方置位 NREJ 但从不调 `write_diag_tile` 时，products 无 `"nrej"` 而计数非零 → 建议。[SA-1/SA-2]
7. `:237-246` `tile_rel_path_legacy` 零调用，其注释宣称的「标准路径不存在时回退」在本文件内**无任何实现载体**（本 writer 是纯写侧）→「活注释 + 死实现」，比死代码更危险 → 建议。[SA-1]
8. `:372-378` `write_fits_image_raw` 对未知 bitpix **无 else** ⇒ 一行 `fits_write_pix` 都不执行、`status` 仍 0 ⇒ 返回 true，产出「合法但零数据」FITS。当前三个调用点只传 {-32,-64,32} 故不可达，但属 fail-open 通路 → 建议。[SA-2]

**判定**：阻断。

**确认无问题（本人独立重算，给行号）**：
- `:65-131` 五个 `abi_ok_*` 逐字段比 `struct_size`+`abi_version`，返回专用码 `AIO_HIPS_ABI_MISMATCH`，数组形态逐元素校验后整体写入，无部分写入。`[SA-1]`
- `:281-315` `write_chksum_deterministic`：占位 CHECKSUM=ASCII0 → `fits_set_hdustruc` 重算头区偏移 → 取 datasum → 回写 → 再取 hdusum → 编码回写，顺序与 CFITSIO `ffpcks` 先例一致。`[SA-1]`
- `:544-649` `write_fits_atomic` 全链：tmp(带 pid+seq，同目录) → 写 → `verify_fits_checksum` → fsync → punch → rename → fsync 父目录；任一环失败 remove+return false。ENOSPC 分类在**清理之前**取（`:557-558`），与 `aio_disk_full.h` 「清理会释放空间，事后探针必然 fail-open」一致。`[SA-1]`
- `:612-628` `PUNCH_NO_RELEASE` 按合同 T1 第⑤条判红、拒发布，且与 `:629-635` 的「其余码仍降级 warn」显式分开 —— 本文件最重要的修复之一，方向正确。`[SA-1]`
- `:472-540` `verify_fits_checksum` 逐 HDU、只校验带 CHECKSUM 的 HDU、且 `:535-538` 要求**至少一个**校验成功 ⇒ 堵死「零 HDU 被校验 ⇒ 恒绿」。`[SA-1]`
- **`:1485-1496` z 映射互斥性（本人独立重算）**：`s = parent_ipix & (2^shift−1) ⇒ s ∈ [0, 2^shift)`；`i ∈ [0, 4^9)`；`full=(s<<18)|i ⇒ full ∈ [s·2^18, (s+1)·2^18)`；`z = full >> 2·dk ⇒ z ∈ [s·2^(18−2dk), (s+1)·2^(18−2dk))`。对不同 s 区间**互不相交**且 `z < 2^18 = n` 恒不越界 ⇒ 每个祖先像素全生命周期恰好被一个叶 tile 写一次 ⇒ 与到达序无关。**该推导正确**（但在重复叶写的违约序列下会被打破，见 §5 反例 3）。`[本人/SA-1]`
- **`:2347` `sig_min <= sig_max` 守卫（曾被我怀疑恒红，推导后否决）**：初值 `1e300/-1e300`（`:980`），且 `:1386-1387` 在**同一个 if 块内**同步更新 ⇒ 只要有一个像素走完该分支就必有 `sig_min ≤ sig_max` ⇒ 守卫精确等价于「至少有一个有限 signal 被发布」，既不恒真也不恒红。`[SA-1]`
- `:772-773` 两条 `static_assert` **不是恒真门**（曾被我列为候选，推导后否决）：`kShift` 是字面量 12、`kElems=kSide*kSide`，改 `kSide=32` 漏改 `kShift` 即触发；`kPerAxis = 512/kSide` 是整数除法，`kSide=100` 时 `(512/100)*100=500 ≠ 512` 即触发。`[SA-1]`
- **私建线程池**：全文 grep `std::thread|pthread|thread_pool|std::async|omp_` **零命中**；唯一 `<thread>` 使用是 `:569-571` 测试专用 `sleep_for`（默认不触发）。**本片无线程池违规**（与 `snr_evaluator.cpp:321` 的 `#pragma omp parallel for` 相反）。`[SA-1/SA-3/本人]`
- **自愈判据/锚**：`:573` 校验读的是**本次刚写、尚未 rename 的私有 tmp**，不是会被本次执行覆写的正式文件；verify 面读 finalize 之后的磁盘终态。**本片生产代码无自愈锚**。`[SA-1/本人]`

---

### 3.2 `lib/infrastructure/aio/tests/test_query_pixel.cpp`（1170 行）— **阻断**

**读了什么**：全文 2 段读完。oracle 实现、fixture 构造、16 个用例、`find_tile_pixels`/`find_pixel_loc` 辅助、main 汇总。

**看到什么**：
1. `:93-170` oracle = 生产逐字副本 → **阻断-3**（已用 `diff` 复核，见 §8）。
2. **`:1044-1055`/`:1067-1080`/`:1092-1105` 筛掉真信号 + 弱门**：随机抽样循环 `for (trial<12 && checked<6) { lip = rng()%n_leaf; if (loc.local_ipix != lip) continue; … }`，网格扫描未命中即跳过；最后只断言 `checked > 0` —— **16 个像素里只要 1 个对就通过**。FULL/BITMAP/SPARSE 三处同款。
3. `:852-858` 条件包裹的断言块：`if (loc40.local_ipix == 40) { …三条断言… }` —— 扫不到就**整块静默跳过**，测试仍绿（恰是球面变换退化时最该报警的地方）。
4. `:601-604` **把契约违背写成期望**：断言 `read_tile` 返回长度 `== 8`，而契约要求 FULL 返回 `n_leaf_per_tile=16`；按 AGENTS §3「代码与文档冲突以文档为准」，此处应红而非绿。
5. `:290-297` 与 `:383`/`:388` **同一文件内注释自相矛盾**：结构体字段写 `sparse_nside=256/sparse_n_leaf=256`，而 `:383` 的小节标题写「NSIDE=128, n_leaf=64」，`:388` 又写「NSIDE=256」。`:352` 的「5/16 = 31.25% >= 0.1 → BITMAP」描述的是一个**不存在的阈值**（真实判据是编码字节数比较）。
6. `:187-189` `find_tile_pixels` 的 `tile_nside` 形参从未使用。

**判定**：阻断（因阻断-3）。

**确认无问题**：`:519-532`/`:549-559`/`:576-586` 的 signal 期望值 100/180/250 来自 setup 写入的 `i*10+100`（`:332-336`），是**独立于被测实现的硬编码常数**，有真实判别力；`:641`/`:757` 的 `occ_mode` 断言与 writer 自动选择实算一致，非恒真门；`:1154` 的 `g_test_passed = total − failures` 口径统一。[SA-4/本人]

---

### 3.3 `lib/infrastructure/aio/product_io/src/fits.cpp`（708 行）— **阻断**

**读了什么**：全文读完。Seaman 校验和族（`fold_halves`/`fits_ones_complement_sum`/`fits_oc_add`/`fits_encode_checksum`/`fits_decode_checksum`）、`FitsStreamWriter`（begin/write/end/fsync）、卡格式化、`parse_one_card`、`read_naxis`、`verify_impl`。

**看到什么**：
1. **`:360-369`（配合 `:345`）静默降级 → 全零产品且重开验证看不出**：`write_data` 只设**上界**（`:345` `data_written_ + len > data_size_`），**全程没有「必须写满」的门**；`end_hdu` 把「补到 2880 对齐」与「整行整行补零」混进同一分支并 `return true`。生产调用方 `product_io/src/product_io.cpp:50-52` 是 `if (!layer.data.empty() && !w.write_data(...)) return false;` —— **data 为空时 `write_data` 整个被跳过**，直接 `end_hdu`，得到「声明形状正确、全零、DATASUM/CHECKSUM 全对」并原子发布的产品。**且验证器抓不到**：`:613` 重算 DATASUM 用的正是盘面字节（零字不改变 1 补码和，见 `:359` 注释），`:649` 比对的 NAXIS 来自同一 spec。⇒ **验证器没坏，坏的是缺完整性门；判据与被检量同源。** `[SA-3]`
2. **`:524`/`:674`/`:679` 空输入 fail-open**：`while (pos < size)` 在 size=0 时不进循环；`:674` 的计数检查因 `expected` 空而跳过；`:679` `res.ok = violations.empty()` ⇒ **0 字节文件被判「验证通过」**。写失败/拷贝中断产生的 0 字节文件正好落进这个洞。`[SA-3]`
3. **`:76-82` 长字符串卡静默截断成非法卡，本仓验证器自洽接受**：`room=70`，`field = "'"+value+"'"`，**value ≥ 69 时 `substr(0,70)` 把结尾 `'` 切掉**，写出无闭合引号的非法卡且注释被静默丢弃；回读侧 `:444-459` 扫到字段末尾找不到闭合符就 `break`，把截断文本当合法字符串存入，**不产生任何 Violation**。即「写侧产出非法卡 + 验侧自洽放行 + 外部 cfitsio/astropy 读失败」。`[SA-3]`
4. **`:162-176` `fits_decode_checksum` 是恒真门**：函数体无任何输入校验，末尾无条件 `return true`。形状像校验器，实为死门；其唯一消费方 `eng/tests/unit/aio/aio_test.cpp:272` 的 `CHECK(fits_decode_checksum(...))` 因此**恒绿，不构成正确性证据**。`[SA-3]`
5. `:197-198`（写侧）与 `:595-596`（验侧）`for (d : naxis) total *= d` 无溢出防护；`:493` 的 `read_naxis` 只要求 NAXIS ∈ [1,999]、NAXISn ≥ 0，无上界 ⇒ `raw` 可回绕为 0 ⇒ `:599` 截断检查通过、`:670` 的 `pos` 只推进头长 ⇒ **HDU 遍历失步**。`[SA-3]`
6. `:53-58` `make_real` 用 `%.12g`：NaN/Inf 写成 `"nan"/"inf"`（FITS 合法写法是 `NAN`），且 12 位有效数字低于 double 往返精度（需 17 位）⇒ WCS 常量引入 ~1e-13 相对误差，**本仓验证器自洽接受**（`:481-484 strtod`），外部读者不认。`[SA-3]`
7. `:459/477/483/486` 重复关键字**后写覆盖前写**，与 FITS「首次出现优先」相反 ⇒ 同一文件两套结论。`[SA-3]`
8. `:316-334` `write_raw` 先改校验和累加器（`:316-332`）后落盘（`:334`）⇒ pwrite 失败时累加器已污染且无回滚入口。`[SA-3]`

**确认无问题**：`:40-49` `fold_halves` 与 `:118-124` `fits_oc_add` 的 1 补码进位折叠逐位正确；`:126-159` `fits_encode_checksum` 与 astropy/cfitsio 的 Seaman 编码逐行对齐，排除字符调整循环必然收敛；`:112/321/330` 每 720 个 16-bit 字折叠 = 每 1440 字节，单周期上界 `720*0xFFFF < 2^32` 无溢出；`:228` EINTR 重试正确；`:345` + `:178-181` 保证 `data_written_ ≤ data_size_ ≤ padded`，`:361-362` 无 size_t 下溢；`:376-397` 的自检读的是 `:398` 即将落盘的**同一个 `header_` 缓冲区**，故**不是自愈式恒真断言**（encode 被破坏则 `combined` 不为 0/0xFFFFFFFF ⇒ 真红）。[SA-3]

---

### 3.4 `lib/infrastructure/aio/tests/p1hips/p1hips_tests_negative.cpp`（687 行）— **阻断**

**读了什么**：全文读完。N1–N11 + DP 组，helper（`is_root`/`make_ro_dir`/`has_tmp_leftover`）。

**看到什么**：
1. **`:295-297` 与 `:398-400` root 下整块跳过，却仍打印 PASS**：
```cpp
if (is_root()) { fprintf(stdout, "… N4 跳过 (root 权限绕过只读目录)\n"); } else { … }
```
root 下 `else` 整块不执行、**不发任何 CHECK**（既不 PASS 也不 FAIL），而 `:680` **无条件**打印 `"[p1hips] negative: N1..N11 + DP-N1..DP-N4 PASS"`。后果：N4（FITS 路径不可写 → −4/−6/−7）三组错误码零覆盖；**N6（`:414-416` 的 I9「失败句柄二次 finalize → −2」）是全仓唯一验证 I9 的地方**，且必须先依赖 N4 式失败路径才构造得出「失败句柄」，跳过即等于该冻结不变量从未被检。**这正是本项目明禁的静默降级**，且发生在判别力最关键的负向面上。`[SA-5]`
2. `:276-289`（N3b）「三态表回归锁」的两个像素级断言在 **`pix_d` 为空时恒绿**：`sig_all_nan`/`sup_all_pos` 初值 true、只在循环体内置 false；若写入端把 FLOAT64 产品误写成 BITPIX=−32（或写 NAXIS=0），`read_tile_fits` 按 bitpix 分派只填 `pix_f` ⇒ `pix_d` 为空 ⇒ 两个循环体一次不跑 ⇒ 两个 CHECK 恒 PASS。`:278` 的 `ok_s && ok_p` 只证伪「读不出」，不证明「读出了任何像素」。`[SA-5]`
3. **只锁叶面、不锁 hierarchy 面**：N3b 用 `fix_hips_f_no_signal_tile` 构造「有覆盖 ∧ flux=NaN」，但只回读 `signal/Norder0/…` 与 `support/Norder0/…`。`FIX_NSIDE=512 ⇒ leaf_order=9 ⇒ tile_order=0 ⇒ ps->hier.resize(0)` 为空 ⇒ **hierarchy 路径根本没被触发**。这正是 §3.1 第 4 条缺陷（`:1481` 筛掉真信号）能长期存活的原因：**测试锁住了被修好的叶面，没锁未修的父面。** `[SA-5/本人]`
4. `:234-238`（N3）「有覆盖但无方差信息」只断言返回码 0，**不查落盘内容** —— 同文件 N3b 做了 readback，此处断裂；写入端对该态写 `variance=NaN ∧ ivar=NaN` 仍返回 0 则全绿（而注释明写「禁 NaN」「rc=7」两个后果）。`[SA-5]`
5. `:200-201` `bad_parent.view.parent_ipix = 12; // ≥ 12·4^0` —— `FIX_NSIDE=512 ⇒ tile_order=0 ⇒ npix_order=12`，这是**紧邻边界的真越界值**，负例构造正确（`[SA-5]` 确认，本人不降级）。
6. `:104-172`（N1）8 处 `ps == nullptr` 断言后未 `abort`，句柄泄漏；`:122` 的 `{600,513,768,1000,1536}` 全为真非 2 的幂（真变异）。`[SA-5]`
7. `:370-375` N5 的 `strstr` 子串依赖中文硬编码（"非有限"/"必须 > 0"/"超出物理域"），措辞调整即红，且子串命中≠「点名了原因」；`:467` N9 依赖英文串 "parent"。`[SA-5]`

**判定**：阻断（因第 1 条）。

---

### 3.5 `lib/infrastructure/aio/tests/p1hips/p1hips_oracle.hpp`（388 行）— **须修**

**读了什么**：全文读完。四组：位解交织/映射、期望值闭式、产物读回解析器、临时目录工具。

**看到什么**：
1. **文件头 `:1` 声明「独立 oracle (不调用被测函数, 不复制同一实现)」，`:4` 声明「不复制源码公式」—— 两条都被自身内容证伪**：
   - `:82-88` `local_to_fits_index` 与生产 `lib/algorithms/shared/healpix/healpix_core.cpp:288-293` `nested_local_to_fits_index` **结构逐字同构**（同一 `local_to_xy` 解交织 + `(maxv−x)*width + y`），仅差 `shift` 硬编码与 `tile_width==0` 守卫。
   - `:156-160` `moc_uniq` 与生产 `aio_hips_writer.cpp:2016` 的 MOC UNIQ 表达式**逐字相同**（`4*4^order + ipix>>2Δ`）。
   - `:139-152` `hierarchy_pixel_expect` 中 `flux = sig*sup*A; area = sup*A; sig_parent = flux/area` **代数上恒等于 `sig_child`**（IEEE 下差 ~2 ulp）⇒ **对父级 signal 不做任何判别**；且它只处理**单个子单元**，不含求和，而 `:135-136` 的注释声称建模的是「Σ子flux / Σ子area」的逐父聚合。**真正的判别力全部寄存在调用方的聚合写法上（不在本文件）。** `[SA-5/本人]`
2. **`:120-125` `leaf_variance` 与生产在合法产品态上不一致**：oracle 在 `!(var_num > 0.0)` 时返回 **NaN**；生产 `aio_hips_writer.cpp:1601-1610` 在 `area>0 ∧ vnum==0` 时写 **0/0**（DATA_SEMANTICS §4a:49 的「显式不可用，禁 NaN」）。⇒ **oracle 把合同规定的态编成了另一个值**。反向问题：`:112-118` `leaf_support` **不检查 `isfinite(area)`**（生产 `:1374` 检查），`area=+Inf` 时 oracle 给 `min(Inf/A_cell,1)=1.0`、生产给 `0.0`。
3. **`:319-322` `read_moc_uniq` 在 `fits_open_file` 失败分支里调用 `fits_close_file(fptr, ...)`，而此时 `fptr` 是 nullptr** —— 对未打开句柄调 CFITSIO close，属 API 误用（返回错误或行为未定义）。`[SA-5]`
4. `:249-257` `read_snr_tsv` 把 `sscanf != 6` 的行**静默丢弃**却仍 `return true`（表示「读成功」）；`:191` 文件缺失与空文件返回同一个空 map；`:293-296` `fits_read_key_str` 取不到数值型键时把 status 清零并**静默丢键**；`:382` `make_tmp_dir` 全失败时返回**空串**，后续 `dir + "/signal/..."` 会退化成 `/signal/...` 写到文件系统根。`[SA-5]`
5. `:82-98` `local_to_fits_index` 硬编码 `shift=9u` 却接受 `tile_width` 形参 ⇒ 传 256 时 `maxv−x` 在 uint32 下溢、返回天文数字。当前 fixture 恒为 512 故掩盖。`[SA-5]`
6. `:108/123/129/149` 用 `std::numeric_limits` 但 include 清单（`:39-48`）**无 `<limits>`**，当前靠传递包含侥幸成立。`[SA-5]`

**判定**：须修（oracle 的「独立性」是本项目合同明文要求，而本文件自证不成立）。

**确认无问题**：`:61-78` 的 NESTED 位解交织（LSB 起 `bit(2i)→x`、`bit(2i+1)→y`）确为标准 NESTED 定义且与生产无代码共享；`:162-164` `moc_cell_area_sr = 4π/(12·4^K)` 正确；`:176-182` 球面余弦式 `ang_dist_deg` 独立于生产的 `angular_distance_deg`（后者在 `healpix_core.cpp:304-312`）。`[SA-5]`

---

### 3.6 `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/ahps_reader.cpp`（453 行）— **须修**

**读了什么**：全文读完。UTF-8 打开辅助、手写 JSON 解析、固定头解析、`readChunk`/`readPixelIndices`/`readBandStats`/`readBandValues`/`close`。

**看到什么**：
1. **全文件 fail-with-empty-vector 风格**：`readPixelIndices:362-365`、`readBandStats:386-396`、`readChunk:332/344/347` 一律「打 stderr + 返回空」，无错误码；`readBandValues:416` `if (stats.empty()) return false;` 把「波段为空」与「读取失败」**合并成同一个 false**。
2. **`:422-429` 数值稳定性（双重问题）**：
```cpp
float val = s.sum / s.weightSum;
float e_x2 = s.sumSq / s.weightSum;
float var = e_x2 - val * val;
if (var < 0.0f) var = 0.0f;
```
`PixelStats` 三项均为 float32（`ahps_format.h:53-56`）。均值 65000 时 `e_x2 ≈ 4.2e9`，float32 在该量级 ULP ≈ **256**；真实方差 25（σ=5）时相减结果被量化到 0 或 ±256 ⇒ `:428` 的**静默钳位**把抵消失败压成「方差为 0」（下游 SNR 被乐观高估）。无 double 提升、无两遍/Welford。**且无 NaN/Inf 守卫**：`sumSq` 为 NaN 时 `NaN < 0.0f` 为假 ⇒ var 保持 NaN 静默传播。与本仓 `aio_hips_writer.cpp:1094` 明写的「禁 clamp、禁静默跳过」正面冲突。
3. **`:105-116` `parseChunkObj` 是恒真门**：四个 `if (p != npos)` 各自填字段，**无论一个键都没找到也 `return true`**；`:137-139` 先 `memset` 归零再 push ⇒ 畸形头产出全零 `ChunkIndex`，`:315` 见 `size==0` 直接返回空 vector ⇒ 上层看到的是「没有数据」而不是「头解析失败」。
4. **`:300` `std::fseek(m_fp, (long)offset, SEEK_SET)`** —— `offset` 是 `uint64_t`，`long` 在 LLP64（Windows/MinGW，本仓工具链正是 MinGW64）下为 32 位 ⇒ >2 GB 偏移**静默截断**。
5. **`:321-345` 解压扩容循环既不区分「缓冲不足」也不封顶**：`est = chunk.size*8`，8 轮 `×4` ⇒ 最坏缓冲 ≈ `chunk.size × 8 × 4⁷` ≈ **131072×** chunk 大小（1 MB chunk ⇒ ~131 GB）；`chunk.size` 来自未校验的 JSON 头（`:110`），无上界、无与文件实际长度的一致性检查。
6. `:74-102` `extractString` 定义后**全仓零调用**（死代码）。
7. `:59-71` `extractNumber` 对垃圾输入静默返回 `0.0`。

**判定**：须修。

**确认无问题**：`:369`/`:400` 的尺寸校验是**有效交叉校验**（被检量 `all.size()` 来自实际解压字节流，期望量 `m_pixelCount*sizeof(...)` 来自文件头，两条独立来源），非恒真；`:174` 固定头 38 字节与 `ahps_format.h:10-12/37` 的布局逐字节吻合（4+2+4+4+8+4+4+4+4=38）。`[SA-4/本人]`

---

### 3.7 `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/gradient/snr_evaluator.cpp`（369 行）— **须修**

**读了什么**：全文读完。D2R/R2D、Vec3、大圆距离、`Impl`(PIMPL)、`build`/`buildF64`/`evaluate`/`evaluateBatch`。

**看到什么**：
1. **`:321` 私建并行域（本项目规范明禁，池的所有权归调度器）**：`#pragma omp parallel for schedule(static)`，无 `if(...)` 条款、无线程数上限、无宿主 executor 契约；`:12` 无条件 `#include <omp.h>`。**且活生产副本 `lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp` 保留同一 pragma** ⇒ 不止归档问题。
2. **`:196-197` 与 `:252-253` 非法入参静默替换**：`median_snr_ = (median_snr > 0.0) ? median_snr : 1.0`、`idw_power_ = (idw_power > 0.0) ? idw_power : 2.0;` —— 调用方传 0/负值时**无诊断**地改成缺省，科学参数被替换。同文件 `:158-161`/`:185-187` 对 build 失败是有 stderr 的，形成反差。
3. **`:312-317` `evaluateBatch` 失败时静默填零且返回 `void`**：`!built_ || !tree || 空指针` ⇒ `std::fill(out_snr, …, 0.0f)` 后 return，**无返回码、无 stderr** ⇒ 调用方无法区分「未 build → 全零」与「真实 SNR 全零」。
4. **校验不对称**：`:174-176`/`:238-239` 对控制点过滤 NaN/Inf，`:267`/`:323-324` 对**查询点不校验** ⇒ NaN 的 ra/dec 生成 NaN 查询向量，nanoflann 的距离比较对 NaN 恒假，可能返回 0.0f 也可能返回看似合法的近邻。
5. **`:271`/`:319`/`:332` K 值来源分叉**：`DEFAULT_KNN=16`（`snr_evaluator.h:113`），但 `:330-331` 缓冲 `idx_buf[16]`/`:332` `k_use = min(k,16)` **各写死一个 16** ⇒ 若有人把 `DEFAULT_KNN` 改成 32，`evaluate()` 用 32 而 `evaluateBatch()` 静默停用 16，两条路径结果不再等价且无告警。
6. `:17-19` `#define M_PI` 在本文件内**从未被使用**（`:26-27` 另写死 `D2R/R2D`）。

**判定**：须修。

**确认无问题**：`:289-292` 与 `:349-353` 的 γ→0 分支语义**等价**（前者提前 return `snr_phot*snr/median`，后者置 `ws_sum=snr; w_sum=1.0; break;` 后走 `:363-364` 得同一表达式）—— 我原本判为不一致，逐项算过后**否决**；`:110` 适配器持 vector 对象指针而非元素缓存，后续 `push_back` 重分配不使树失效；`:143-147`/`:216-220` 析构顺序正确（先 `delete tree` 再 `clear()`），无双重释放；`:56` 的 `acos` clamp 是真防护。`[SA-4]`

---

### 3.8 `lib/infrastructure/aio/src/ahpx/aio_ahpx_api.cpp`（293 行）— **须修**

**读了什么**：全文读完。4 个 C 入口 + 异常屏障。

**看到什么**：
1. **`:279-283` 静默降级：成功返回但输出参数根本没写**
```cpp
int w = 0, h = 0, c = 0;
if (reader.getImageInfo(&w, &h, &c)) { if (width) *width = w; if (height) *height = h; }
return 0;
```
`getImageInfo` 失败时 `*width/*height` **完全不被写**却返回 0（成功）⇒ 调用方读到未初始化内存当几何。对比 `read_pixels:184-191` 对同一调用做了失败分支返回 4 **且**校验 `w/h/c>0`。更实质：`aio_ahpx_reader.cpp:603-619` 的 `readSnr()` **对 snr 块完全不做几何/长度校验**，直接把块字节数/4 当 float 数返回，而 `aio_ahpx_reader.h:60-62` 把它文档化为「读取 SNR 图 (W×H float32)」、`:278` 注释断言「SNR 与图像同尺寸」。反例：图像头声明 10×10、snr 块只含 7 个 float ⇒ 返回 0、几何 10×10、缓冲区只有 7 个有效 float。
2. **`:126-131` 返回值语义撞车 + 溢出**：`return (int)required;`（正数=所需容量）与 `:118` 的失败码 `2` **同值空间**。分支触发条件是 `required > capacity ≥ 1` 故 `required ≥ 2`；当 `headerJson.size()==1` 且 `capacity==1` 时返回 **2**，与「打开文件失败」完全同值，调用方无法区分。且 `m_headerSize` 是 `uint32_t`（`aio_ahpx_reader.h:75`），头可达 4 GiB，`(int)required` 为实现定义且可能为负。该协议在公共头 `astro_image_io.h:157-158` **完全没写**。
3. **`:13` 悬空引用**：`#include "../include/astro_image_io.h"` 从 `src/ahpx/` 出发指向 `lib/infrastructure/aio/src/include/` —— **已实测该目录不存在**（`[本人]` 复验）。同一字符串在 `src/aio_api.cpp` 是正确的，属跨层级复制粘贴遗留；能编译仅因测试目标加了 `-I .../aio/src`。
4. `:70` `if (snr && snr_w > 0 && snr_h > 0)` —— 调用方传了非空 snr 指针但几何 ≤0 时 SNR 被**无声丢弃**、`write` 返回 0 ⇒ 下游 P2 少一个输入且无错误码。读侧 `read_snr:265` 对同类情形返回 4，读写语义不一致。
5. `:41` 文档「zstd_level: 1-22」但 `:76` 直接透传不校验 —— 同函数 `:51-55` 的 path/pixels/w/h/c 全部严格校验，唯独此项不校验。
6. `:125` `if (metadata_json && metadata_capacity > 0)` 之后 `return 0` —— 缓冲区给了但 capacity 传 0 ⇒ 拷贝跳过、缓冲区一字节未写、也不返回所需容量 ⇒ 扩容握手协议在此情形静默放弃。

**判定**：须修。

**确认无问题**：`:57`/`:106`/`:168`/`:247` 四个入口的异常码 6/3/8/6 分别大于各自既有用码上界 5/2/7/5，与 1..N 无冲突；`astro_image_io.h:152-164` 与本文件定义**逐参数同形**（`:44` 注释所述为真）；`:115`/`:172`/`:251` 对 open 失败区分「旧格式显式拒绝」与「打开文件失败」并各自打诊断，无原因空串化路径。`[SA-3/本人]`

---

### 3.9 `lib/infrastructure/aio/tests/test_healpix_io_py.py`（466 行）— **阻断（整份不可运行）**

**读了什么**：全文读完。4 个（实为 5 个）测试函数 + main。

**看到什么**：
1. **`:32` `import healpix_io` + `:28-30` 只把自身目录塞进 `sys.path` ⇒ 模块不在仓内。** 本人复验：`lib/infrastructure/aio/tests/healpix_io.py` **不存在**；`healpix_db/healpix_io/` 目录**只有 `ARCHIVED.md`，零源码**。⇒ **466 行在 import 阶段即 ModuleNotFoundError，永不执行**，且未被任何 ctest/checks.json 登记。
2. **`:344-351` 假绿报告**：注释写「验证 JSON 头包含 snr_format=1」，但代码只 `f.read(4)` 读 magic、`uncomp_len`、`comp_len` 后**一个断言都没有**，紧接着
```python
expected_data = 100*8 + 100*4 + 4 + 50*20 + 24
print(f"  [OK] 数据区预期 ~{expected_data} 字节 …")
```
打印 **`[OK]` 却从未做过该检查**；`uncomp_len`/`comp_len` 读出后从未使用。
3. `:13-18` 头部只列 4 个用例，实际有 5 个（`:298` 的 SNR 模型往返未列）。
4. `:434-440` `test_hcsd_read_leaf` 依赖 `hcsd_path`，若上游未产出则记 `results.append((name, False))` 混入 SKIP 与 FAIL 两种语义。

**判定**：阻断（整份死代码 + 一处打印 `[OK]` 的假验证声明）。

---

### 3.10 `lib/infrastructure/aio/tests/test_export_fits_fix.py`（282 行）— **阻断（整份不可运行）**

**读了什么**：全文读完。6 个 test 函数、`check()` 计数器、main。

**看到什么**：
1. **`:23` `_PYTHON_DIR = ../python` → `lib/infrastructure/aio/python`；`:27` `from astro_image_io import PipelineFramePy`。** 本人复验：`lib/infrastructure/aio/python` **不存在**；`git ls-files | grep -E "astro_image_io.*\.py$"` **零命中** ⇒ **282 行在 import 阶段即死**。
2. **`check()` 全是全局计数器，6 个 `test_*` 函数体内没有任何 `assert`** —— 一旦将来接入 pytest，「既不抛异常也无失败断言」的用例会一律判 **PASSED**。而本文件要验证的正是「导出 FITS 数据值非垃圾」（`:114`）：若 C++ 端回归成字节序错误，`check` 记 FAIL 但 pytest 报绿。
3. `:1-4` 头部自述「NON_PRODUCTION_TOOL_ONLY … The production pipeline uses orchestrator.exe exclusively」—— `orchestrator.exe` 在本仓不存在。

**判定**：阻断。

---

### 3.11 `lib/infrastructure/aio/tests/test_precision_dual.cpp`（613 行）— **建议**

**读了什么**：全文读完。5 个测试（FP32/FP64 roundtrip、cross-mode、metadata 字段、精度差异）+ main。

**看到什么**：
1. **`:546` 弱门**：`ASSERT_TRUE(diff_count > 0, "5a: FP32 roundtrip 至少 1 个值与原始 double 不同")`。输入 16 个值除 0.0 外全部 float32 不可精确表示（`:513-518`），实际 `diff_count` 应为 **15**；门槛 1 意味着即使只坏一个元素也照样绿。与同文件 `:188`/`:280`/`:576` 的 `bitexact_count == n_leaf` 全量门不是一个量级的严度。
2. **`:49-77` 计数口径错乱**：`TEST_CASE` 按**用例数**自增 `g_test_total`（5），`ASSERT_TRUE` 按**断言条数**自增 `g_test_passed`（≈31）⇒ `:599-601` 直接打印两个数，日志出现「总计: 5 / 通过: 31 / 失败: 0」。对比 `test_query_pixel.cpp:1154` 用统一口径。
3. **`:15-22` 编译说明悬空**：说「编译 (从 `lib/infrastructure/aio/` 目录)」，却给出源文件路径 `eng/tests/test_precision_dual.cpp` —— 该目录**不存在**（本人复验）。
4. 三份测试均**无 NaN/Inf 输入**：FP64 路径字节直通，NaN payload/±Inf/−0.0 的位模式理应原样保留，**无一条断言**。

**判定**：建议（判别力未失效，但两处门松 + 一处口径错 + 一处悬空）。

**确认无问题**：`:174`/`:267`/`:540`/`:570` 的 bit-exact oracle 来自**输入侧** `flux_values[i]`，与 writer 的强转动作同名但是**规格契约的独立表达**，能区分「未强转」与「强转正确」；`:335`/`:358`/`:365` 的 cross-mode 拒绝是**真门**（已在 SUT 逐条核实）；`:150` 的 `255.99609375 = 65535/2⁸` 在 float32 尾数内精确；`:240` 的 `1e-100` 是正常 double（远大于 DBL_MIN≈2.2e-308），非 subnormal，FP64 bit-exact 断言不会必红。`[SA-4/本人]`

---

### 3.12 `lib/infrastructure/aio/tests/test_drizzle_integration.cpp`（607 行）— **阻断**

**读了什么**：全文读完。8 个测试 + fixture 查找 + main。

**看到什么**：
1. **`:351` 与 `:358` 字面重言式恒真门**：
```cpp
CHECK(ret == 0 || ret < 0, "aio_hiss_query_pixel 返回合理值 (0=找到, <0=未找到)");
```
`aio_hiss_query_pixel` 全部返回路径为 `HIO_ERR_PARAM`/`HIO_ERR_FILE`/`HIO_OK`/`HIO_ERR_INTERNAL`，即 `HIO_OK=0, -1, -2, -8`（`aio_healpix_io.cpp:54-64`, `:1121-1143`）⇒ **没有任何路径能返回 >0** ⇒ 对该函数可能返回的每一个值恒为真，**断言在数学上不可失败**。消息文字本身即自证：把「找到」与「未找到」并列为合格。**该测试对 signal/support 读数一个断言都没有，只 printf。**
2. **`:419`/`:519` 用了头文件点名的错误公式 + `:450`/`:549` 自洽式长度断言**：`n_leaf = tile_nside*tile_nside*12` = `16*16*12` = **3072**，而正确 `n_leaf_per_tile = (64/16)² = 16` —— `hiss_format.h:61` 明文写 `// 关键公式 (修正了旧版 "tile_nside^2 * 12" 错误):`，**头文件已把该公式点名为已修正的旧错误，测试仍在用**（错 192 倍）。绿灯掩盖机制：`:450` `CHECK(signal_out.size() == acc.pixels.size(), …)` 的期望量就是产生该输出的**同一份输入缓冲区长度** ⇒ 写 3072 读 3072 必相等，**零判别力**。正确值 `g_n_leaf` 在 `:256` 已取出、**仅用于 printf，从未参与断言**。SUT 侧 `hiss_writer.cpp:579`/`:628` 完全信任调用方给的像素数，从不与网格真实 `n_leaf_per_tile` 校验 ⇒「写出叶像素数错 192 倍的 tile」这条真缺陷被这两个用例判绿。
3. **`:303-311` 筛掉真信号**：`if (std::isnan(signal[i])) { has_nan = true; break; }` —— break 之后不再扫描，(a) `has_nonzero` 只在**前缀**上判定，(b) 打印的 `min/max` 只覆盖前缀（NaN 在 index 0 时会打印 `min=1e30 max=-1e30`），(c) 前缀全 0 而后半有值时会**误报 FAIL**，把真缺陷的因果指向「全零」而非「有 NaN」。
4. **`:403-405`/`:505-507` SUPPORT 子块设了 LZ4/ZSTD，但 `support_out` 在 `:446`/`:545` 取出后全文再未出现** —— 用例名为「codec 往返」实际只覆盖 SIGNAL。
5. **`:400-405` 从不断言 codec 真的被用上，而 Writer 有静默 RAW 回退**：`hiss_writer.cpp:208-214` `if (compressed_size >= size_to_compress) final_codec = RAW;`，该提示仅走 `HISS_DLOG`，而 `HISS_DLOG` 在 `hiss_writer.cpp:32-36` 是 `#ifdef HISS_VERBOSE` 才 fprintf 的宏（本测试编译标志未定义）⇒ **回退完全静默**，用例无法区分「LZ4 往返成功」与「静默退回 RAW 往返成功」。
6. **`:407-414` 注释说「用真实 drizzle 数据」但紧接 `:416-427` 直接 new 全合成 acc**；`it` 只在 `:413`/`:513` 用于取 `parent_ipix` 这个**键**，累加器内容从未被读取。`:422`/`:522` 的 `std::mt19937 rng(42)` 声明后从未使用。
7. **`:88-95` 5 个搜索目录全部不存在**（实测 `testdata/results/Galaxy_Center_T4/...` 不存在，真实布局是 `testdata/Galaxy_Center_T4/lights/...`）且为 CWD 相对 ⇒ 从仓库根以外启动必失败（`:577` return 1，硬失败非绿灯）。**但绿灯侧另有一条**：若有人传入合法路径而该 FITS 无 WCS，`:164-167`/`:203-206` 跳过 ⇒ `g_accumulators` 空 ⇒ test_04~08 全部 SKIP ⇒ `g_fail=0` ⇒ `:606 return 0`，**一条实质断言都没跑却报成功**。
8. `:231-232` `CHECK(exists(...))` 后不 return，紧接 `std::filesystem::file_size(...)` 对不存在路径抛 `filesystem_error`，全文无 try/catch ⇒ `terminate`，后续 7 个用例全部不执行。

**判定**：阻断（因第 1 条恒真门 + 第 2 条自洽式断言）。

---

### 3.13 `lib/infrastructure/aio/io/tests/hips_core_selftest.c`（213 行）— **须修**

**读了什么**：全文读完。`run_one` / `run_missing_props` / `main`。

**看到什么**：
1. **`:5-10` 头注释声明的 4 项覆盖，正文一项未测**：无 `ACS_HIPS_TILE_INVALID` 引用、无 tile_width 非 2 次幂用例、无「未知 frame / png-only / snr 产品」用例、无「无 MOC → count=0 不失败」分支。实有的是 props 解析（`:68-81`）、PRESENT/MISSING（`:86-115`）、f32/f64 读回（`:118-164`）、ipix 越界（`:167-169`）、容量不足（`:172-178`）。**只有第 3 项「缺 tile 绝不父回退」有实测支撑（`:107-112` 走 TILE_MISSING 而非父阶）。覆盖声明失真比不写更危险。**
2. **`:200-206` `run_missing_props` 在 CTest 下永不可达**：需 `@` 前缀（`:201`），而 `eng/tests/unit/CMakeLists.txt:1736-1737` 的 `add_test(... COMMAND w34_hips_core_selftest ${ACS_W34_HIPS_FIXTURE})` **只传 fixture_dir，无 `@empty_dir`** ⇒ `:185-192` 的「空目录 → PROPERTIES」负例**永不执行**，`:207-210` 仍打印 `ALL PASS`。**且本文件两处用法说明都漏了 `@` 约定**：头注释 `:14` 写 `用法: <fixture_dir> [<fixture_dir2>...]`，main 自用串 `:197` 写 `<fixture_dir> [<empty_dir>]`。按文档照做 ⇒ 空目录被当 fixture 传入 `run_one` ⇒ PROPERTIES 检查静默消失 + 误报。
3. `:83-92` `CHECK(count >= 1)` 无上界、循环硬截断 `i < 8`，而注释 `:83` 给出精确期望列表 `[0,5,8,18,28,40,46]`（7 项）**却不校验** ⇒ MOC 枚举多吐/少吐/乱序仍全绿。
4. `:43-44` 注释写「用容差 1e-2」，代码 `:133`/`:152` 实际用 **0.02**。

**判定**：须修。

**确认无问题**：`:60` `if (!h) return g_failures;` 不会把失败吞成成功（对照 `p1hips_tests_negative.cpp:680` 的相反口径）；`:107-112` 的 TILE_MISSING 断言是真门；`:43-46` 的期望值公式与生成器同规律但经本人验算在 ipix0=0 时为二进制精确值，f32 无损。`[SA-5/本人]`

---

### 3.14 `lib/infrastructure/aio/src/aio_abi.cpp`（210 行）— **须修**

**读了什么**：全文读完。FaultRegistry、`decl_is_valid`、4 个 `extern "C"` 入口。

**看到什么**：
1. **`:58-81`/`:151-154`/`:172-175` 环境变量可无条件关闭篡改检测，且该 TU 在生产 target 内**：`ACSD_AIO_FAULT=n2_verify_mismatch_shortcut` 使 `aio_content_hash_verify_buffer_v1` 与 `aio_content_hash_verify_file_v1` 对**任意被篡改的内容/文件**返回 `AIO_OK`。**无编译期隔离**（无 `#ifdef NDEBUG`/测试宏）、无运行期告警，唯一防线是注释里的「生产 env 为空」。根 `CMakeLists.txt:618-628` 确认 `aio_abi.cpp` 在生产静态库 `acsd_aio` 的源文件列表内，不是测试专用编译单元。另 `:135-138` 的 `n1_hash_value_flip` 改写 `out_hex[0]` 却仍 `return AIO_OK` ⇒ **「成功 + 数据已被篡改」**。
2. **`:101` ABI 布局锁只比 `sizeof` 不比 `offsetof`**：上方注释自称「失配即拒, 不猜布局」；同尺寸但字段重排/不同 packing 的结构体**完全通过**。加重：仓内确有逐字段布局锁（`tests/abi/aio_abi_layout_lock.py`、`tools/aio_abi_mirror.py`、`tests/abi/aio_abi_layout_probe.cpp`），但覆盖的是 `aio_hips.h` 的 5 个 HiPS 结构体，**`aio_abi_info_v1` 不在其中任何一处**。
3. **`:197` 是死代码且注释所述因果不成立**：`:191` 与 `:197` 完全相同、夹在 `:195` 的 `return AIO_ERR_TRUNCATED;` 之后，注释写「截断路径同样回填实际字节数」；到达 `:197` 的所有路径必已执行过 `:191`。行为正确（契约确实满足），但会误导维护者以为 `:191` 不覆盖截断路径。
4. **`:15-16` 文件头断言「全部公共入口 try/catch」与实现不符**：`:98`（`aio_abi_query_v1`）与 `:147`（`aio_content_hash_verify_buffer_v1`）无 try。
5. `:193-196` `AIO_ERR_TRUNCATED` 同时表示「偏小」与「偏大」（expected=100 实际 200 也返回 TRUNCATED），调用方无法区分被截断与被追加。
6. `:177` `fopen` 与 `:189` `fclose` 同在 try 内，`:205-209` 的 catch 恰是为 `bad_alloc` 准备的 ⇒ 窄窗口句柄泄漏（无 RAII）。

**判定**：须修。

**确认无问题**：`:114` `!data && len != 0` 拒绝、len==0 走合法空内容路径（e3b0c442…），与 `aio_abi_v1.h:105-108` 契约一致；`:58-81` 的 magic static 线程安全且只初始化一次，`has()` 的 `pos` 最大取到 `size+1` 仅作循环条件不作索引，无越界；`:184-190` 先 `ferror` 再 `fclose` 再判 `io_err` 顺序正确（`fclose` 会清错误位）；`aio_abi_v1.h:43` 的 `AIO_HASH_FILE_CHUNK=32768` ⇒ 栈缓冲 32 KB 不溢出。`[SA-3/本人]`

---

### 3.15 `lib/infrastructure/aio/tests/v5_snr_precision_roundtrip.py`（170 行）— **建议**

**读了什么**：全文读完。`make_values`、ctypes 绑定、双 dtype 主循环、TSV 解析、结果落盘。

**看到什么**：
1. **`:146` `pack = struct.pack if False else None`** ⇒ **恒为 None**，是死代码；`:150` 实际用的是完整的 `struct.pack(fmt, …)`。**`:152` `meta = (out/"snr"/"properties").read_text(...)`** 读出来后**从未参与任何断言**。文件头 `:5-7` 宣称「逐行解析 TSV 并与期望值做 bitwise (struct.pack) 比较」——比较本身是真的，但这两处死代码说明此处曾有被删掉的断言。
2. `:36-37` `AIO_DLL` 依赖仓内 `astro_image_io.dll`；`:31` 依赖 `lib/infrastructure/aio/tools/aio_abi_mirror.py`（存在，`:26-30` 有显式存在性检查与清晰报错）。
3. **未注册**：`grep` 在 `eng/ci/checks.json`、`eng/ci/ctest_baseline.json`、`eng/tests/unit/CMakeLists.txt` 中对本文件**零命中**。

**判定**：建议。

**确认无问题**：`:122` `expected[sid] = float(v)` 取自**输入侧**，`:140` `actual[sid] = float(ff[3])` 取自**落盘文本**，`:150` 的 `struct.pack` 比对是真正的「写—读」比对；`missing`/`extra`（`:144-145`）+ `dup`（`:141-142`）+ `datatype_ok`（`:157`）四向在 `:158` 合取完整，**无恒真门**。`[SA-5/本人]`

---

### 3.16 `lib/infrastructure/aio/healpix_db/README.md`（113 行）— **阻断**

**读了什么**：全文读完。模块表、归档内容、关联仓库、依赖、构建、使用、文件格式、技术特点。

**看到什么**：**本 README 的模块表 6 行 + 归档内容 5 行，路径声明无一为真。** 本人逐条复验（§8 可复跑）：

| 声称 | 行 | 实测 |
|---|---|---|
| `healpix_browser_qt/` | 16 | **不存在**（实际在 `lib/infrastructure/hips_browser/healpix_browser_qt/`；而 README 自身 `:56`/`:68`/`:77`/`:86` 给的都是 hips_browser 路径 ⇒ **模块表与正文自相矛盾**） |
| `healpix_io/` 已归档 | 17 | 目录仍在活跃模块树下，且**只有 `ARCHIVED.md`，零源码** |
| `healpix_stack/` 「独立仓库本地副本，**.gitignore 忽略**」+「**活跃**」 | 18 | 路径应为 `healpix_db/archive/legacy/healpix_stack/`；`git check-ignore` → **NOT-IGNORED**；`git ls-files` 追踪 **37 个文件** ⇒ **三重证伪**，且与同目录 `.gitignore:21-22`「不再忽略」正面冲突 |
| `healpix_drizzle/` | 19 | **不存在** |
| `docs/` | 20 | 存在（唯一为真的模块行） |
| `archive/` 五条子项 | 25-29 | **五条全部不存在**（`archive/healpix_browser_cpp`、`archive/healpix_browser_web`、`archive/legacy/healpix_browser_python`、`archive/legacy/healpix_lod`、`archive/legacy/tests`） |
| `:3` 版本 v2.0 / 2026-07-16 | 3 | 与根 `VERSION`（AGENTS §11 唯一来源）、`healpix_stack/memory.md:7` v1.0 **三处互斥** |

加重：`:16` 把一个**已被归档**的 Qt 浏览器标为「活跃」；`memory.md:75` 称 `healpix_stack.dll 1437.2 KB` 而 `lib/infrastructure/aio/memory.md:88` 称同 dll `1430.8 KB`，**两处互斥且 dll 在树内不存在**（本人复验 `find` 零命中）。

**判定**：阻断（悬空引用 + 退役/活跃声明被证伪，且 README 是该目录的唯一门面文档）。

---

### 3.17 `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/memory.md`（79 行）— **须修**

**读了什么**：全文读完。模块职责、依赖、决策记录、三段进度日志。

**看到什么**：
1. **`:31` 与 `:62` 自相矛盾**：`:31`「内存占用 = 3 × n_unique_pix × 8B …，**与帧数无关**」；`:62`「Winsorized 分支引入 `masked[f][k]` 数组跟踪每帧每像素是否被剔除」⇒ 那是 **O(n_frames × n_unique_pix)**，`:31` 的核心卖点在 winsorized 模式下**不成立**，且 `:31` 从未随 GAP-017 更新。
2. **`:44` 灾难性抵消**：`std = sqrt(sum_wsq/weight − mean²)` —— 与 `ahps_reader.cpp:427` 同型；此处**连钳位都没有**。
3. **`:79` 悬空引用**：`orchestrator.cpp run_stage_gradient_sphere 从 stage2_config.json 读 …` —— **`stage2_config.json` 全仓零命中**（本人复验）。
4. **`:75` 悬空引用**：`healpix_stack.dll 编译成功（1437.2 KB）` —— 树内无此 dll，无任何构建图产出它。
5. **`:16`「OpenMP（16线程并行）」** 与 `snr_evaluator.cpp:321` 的裸 `#pragma omp parallel for` 呼应（硬编码线程数）。
6. **文档形态违反 AGENTS §5/§11**：`:7` 版本号、`:26-39`/`:39`/`:48` 日期流水、`:27` commit 号 `b8a8814`、`:35` 三个 bug 的流水叙事。
7. **`:10` 外部仓库 `https://github.com/fujiaze/Healpix-Mosaic-Cpp` 的本地副本被完整纳入主仓** —— AGENTS §4 对传染性许可要求「只读、不复制进仓库」。**这条建议交前台做许可复核，本人不下结论。**

**判定**：须修。

---

### 3.18 `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/gradient/gradient_fitter.h`（147 行）— **须修**

**读了什么**：全文读完。算法说明、`FitterParams`、`FitterResult`、`GradientFitter` 声明。

**看到什么**：
1. **`:67` `sigma_clip_floor = 0.0` 与 `:65-66` 的注释目的正好相反**：注释写「sigma-clip 地板阈值（**避免 MAD 过小时全部保留**）」，但实现 `threshold = max(factor*1.4826*mad, floor_threshold)` 后判 `|diff−med| < threshold`：`mad==0 ∧ floor==0.0 ⇒ threshold==0.0 ⇒ 条件恒假 ⇒ 该帧全部控制点被剔 ⇒ 该帧不产出梯度模型`（静默整帧降级为「不校正」）。**保护机制在出厂默认下完全失效**，方向与注释相反。触发条件现实（过半点位 diff 精确相等）。`[SA-4]`
2. **`:39` 悬空引用**：`.trae/specs/snr-compact-storage-and-gradient-correction/spec.md §3.4` —— **`.trae` 目录不存在**（本人复验）。`:8`/`:13`/`:16`/`:19`/`:25`/`:61` 同一 spec 的 6 处引用**全部指空**。
3. **`:19` 与 `gradient_sampler.h:9` 正面矛盾**：本文件三处强调「**无迭代**」「**无 Gauss-Seidel 迭代**」，而 `gradient_sampler.h:9` 写「为阶段2（`gradient_fitter` **Gauss-Seidel 迭代**）提供输入样本」。
4. `:59`/`:63`/`:67`/`:72` 四个科学/标定参数为硬编码默认值，**无出处注释、无配置化落点**。

**判定**：须修。

**确认无问题**：无断言可恒真（本格无 `static_assert`）。`[SA-4]`

---

### 3.19 `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/gradient/gradient_sampler.h`（128 行）— **须修**

**读了什么**：全文读完。`SampleRow`（pack + static_assert）、`FrameInfo`、`SamplerParams`、`SampleResult`、`GradientSampler`。

**看到什么**：
1. **`:31-41` 的 `static_assert` 是本片唯一一处「真判据」**：被检量 `sizeof(SampleRow)` 由编译器按 `#pragma pack(1)` 算出，期望量 36 是**独立字面量**，且 `:33-38` 的偏移标注 `[0:8][8:16][16:20][20:24][24:28][28:36]` 之和**独立等于 36** ⇒ 三方交叉一致，改字段宽度即红。**确认无问题**（该点恰好反衬 `ahps_format.h:50` 的漏 pack）。
2. **`:77` `double idw_power = 2.0` 硬编码** —— 与归档 `snr_evaluator.cpp:196-197`/`:252-253` 的缺省 2.0 同源；而 `docs/science/DISPUTE_RESOLUTION.md:74-77` 已终裁**默认 `idw_power = 1.0`**（p=2 为无噪/光滑极限最优；生产含噪默认 p=1.0）。**归档里躺着一个活组件的陈旧分叉副本，其科学默认已被终裁作废，零退役标记。** `[SA-4]`
3. **`:9` 与 `gradient_fitter.h:19` 的迭代描述正面矛盾**（见 §3.18 第 3 条）。
4. **`:12-15` 三条悬空引用**：`healpix_core` 静态编译（归档 `Makefile:28` 的 SRCS **不含 `gradient/*`**，该依赖在本目录内也不成立）、`healpix_io.dll`、`gaia_client.dll` —— 后两者全仓零命中（本人复验）。
5. **`:17` 悬空引用** `.trae/specs/...` §3.3（目录不存在）。
6. `:62`/`:65-67`/`:70`/`:73`/`:82-84` 七组标定参数硬编码无出处；`:79-84` 的 `mosaic_fov_*` 注释说「默认 0 = 不截断」而 `:83-84` 用三字段而非一个 `has_fov` 标志表达「是否截断」，`mosaic_fov_ra=0/dec=0/radius=0` 与「显式中心 (0,0) 半径 0」**语义不可区分**。

**判定**：须修。

---

### 3.20 `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/ahps_writer.h`（83 行）— **须修**

**读了什么**：全文读完。用法示例、6 个 setter、`write`、3 个私有助手。

**看到什么**：
1. **`:68` 与 `:78` 使用 `FILE*`，但头内 include 只有 `ahps_format.h` + `<cstdint>/<string>/<vector>/<map>`，**无 `<cstdio>`** ⇒ 头不自足，当前靠 `ahps_writer.cpp` 的 include 顺序侥幸成立，换 include 顺序即断。
2. **`:18` 用法示例 `writer.setNside(32768)`** —— nside=32768 的全天像素数 = `12 × 32768²` = **12,884,901,888 > 2³²−1**，而 `ahps_format.h:12` 的 `PixelCount` 是 **4 字节** ⇒ 示例自己给不出可寻址的值，写端截断、读端 `(int64_t)(uint32_t)` 截断，**静默少数据且无任何告警**。
3. `:50` `write(path, zstdLevel = 5)` 不校验 `zstdLevel ∈ [1,22]`。

**判定**：须修。

---

### 3.21 `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/ahps_format.h`（69 行）— **须修**

**读了什么**：全文读完。布局注释、常量、`Codec`、`PixelStats`、`ChunkIndex`。

**看到什么**：
1. **`:50` 注释「单波段统计量结构 (每像素 **14** 字节)」，`:52-57` 的结构实际是 16 字节**：`uint16_t count` 占 0-1 → 下一个 `float` 对齐要求 4 ⇒ 填充 2-3 → `sum@4, sumSq@8, weightSum@12` ⇒ `sizeof = 16`。结构**无 `#pragma pack`** ⇒ 写盘 `memcpy` 原样搬运结构字节 ⇒ **每像素 2 字节未初始化填充被压进文件**；读端 `ahps_reader.cpp:399` 用同一个 `sizeof(PixelStats)` ⇒ 读写自洽但**文件内容非确定性、跨编译器/ABI 不可移植**。同一归档树内 `gradient_sampler.h:31-41` **做了** `#pragma pack(push,1)` + `static_assert` ⇒ 两种相反做法并存，佐证此处 pack 是漏的。`[SA-4]`
2. **`:12` `PixelCount(4)` 4 字节** —— 见 §3.20 第 2 条。
3. `:37` `HEADER_FIXED_SIZE = 38` 与 `:10-12` 的布局逐字节吻合（4+2+4+4+8+4+4+4+4=38），**自洽**。

**判定**：须修。

---

### 3.22 `lib/infrastructure/aio/product_io/include/astro/aio/validation.h`（49 行）— **建议**

**读了什么**：全文读完。`Violation`、`ValidationReport` 全类。

**看到什么**：
1. **`:24` `bool ok() const { return violations_.empty(); }` + 类型上没有「已执行」标志位** ⇒ **空报告恒绿**，「漏调用校验即得绿」，与 `fits.cpp` 空输入 fail-open 同构。
2. **`:19` 注释自述 `message` 「不得为空占位」，但 `:26-29` 的 `add()` 无任何检查**，`add("G","F","")` 被接受 ⇒ 声明的不变量无强制点。
3. `:4-5` 注释「禁止『无违规即静默通过』以外的语义」——而当前实现**恰恰**只有这一种语义。

**判定**：建议（当前调用方均先构造后逐门 `add`，未构成实际漏检）。`[SA-3/本人]`

---

### 3.23 `lib/infrastructure/aio/src/ahpx/aio_ahpx_reader.h`（97 行）— **建议**

**读了什么**：全文读完。类声明 + 私有成员 + 用法示例。

**看到什么**：
1. **`:25-27` 用法示例是无法编译的 C++**：`reader.getHeaderJson.c_str`（`getHeaderJson` 是函数，`.c_str` 又缺 `()`）、`auto pixels = reader.readPixels;`（取成员函数指针而非调用）、`reader.close;`（同）—— **三处全错**，读者照抄即编译失败。
2. **`:72-73` 裸 `FILE* m_fp` + `:33` 用户声明析构 ⇒ 隐式拷贝构造/赋值仍被生成** ⇒ `AhpxReader b = a;` 造成 double fclose/UB。当前 4 个 C 入口都局部构造实例故不可达。
3. `:60-62` 把 `readSnr()` 文档化为「读取 SNR 图 (W×H float32)」，但实现（`aio_ahpx_reader.cpp:603-619`）**不做任何几何/长度校验** ⇒ 文档承诺与实现不符。
4. `:4` `#include "../../include/aio_ahpx_format.h"` —— 从 `src/ahpx/` 出发 = `src/../include/` = `lib/infrastructure/aio/include/`，**该文件存在**，此项无问题。

**判定**：建议。`[SA-3/本人]`

---

### 3.24 `lib/infrastructure/aio/tests/abi/CMakeLists.txt`（32 行）— **确认无问题**

**读了什么**：全文读完。注释 + 2 个 `add_executable`/`add_test`。

**看到什么**：
- `:20-31` 注册 `aio_abi_layout_lock` 与 `aio_abi_layout_lock_selfcheck`；**`:27-31` 的 selfcheck 用「被破坏镜像」（字段重排 + 漏字段 + 常量污染）反向证明该门非恒真** —— 这是本项目认可的强形式，本片唯一。
- 依赖文件全部存在（`aio_abi_layout_probe.cpp`、`aio_abi_layout_lock.py`、`tools/aio_abi_mirror.py`）；注册链完整（`eng/tests/unit/CMakeLists.txt:613 add_subdirectory(.../aio/tests/abi aio_abi)`）。
- `:18` `${CMAKE_CURRENT_SOURCE_DIR}/../../include` 从 `tests/abi/` 出发 = `aio/include`，路径正确。

**判定**：通过。**但注意它只锁 5 个 HiPS 结构体，不含 `aio_abi_info_v1`**（见 §3.14 第 2 条）。`[SA-5/本人]`

---

### 3.25 `lib/infrastructure/aio/pyproject.toml`（21 行）— **须修**

**读了什么**：全文读完。

**看到什么**：
1. **`:20-21` `[tool.setuptools.packages.find] include = ["astro_image_io*"]`** —— 本人复验：`git ls-files | grep -E "astro_image_io.*\.py$"` **零命中**，`lib/infrastructure/aio/python` 目录不存在 ⇒ **匹配不到任何包，wheel 是空包**。
2. **`:7` `readme = "README.md"`** —— `lib/infrastructure/aio/README.md` 是否存在需前台核（本人未在片内验证，记为待核）。
3. **`:17-18` 声明 `dev = ["pytest>=7.0"]` 却无任何 pytest 配置消费它**；全仓无 `pytest.ini`/`conftest.py`/`[tool.pytest.ini_options]`。
4. `:5` 版本 `1.0.0` 与根 `VERSION`（AGENTS §11 唯一来源）**不是同一来源**，二者无派生关系。

**判定**：须修（构建产物为空 + 依赖声明悬空）。`[SA-5/本人]`

---

### 3.26 `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/ahps_writer.h` — 已并入 §3.20。

> 说明：§3.20 即 `ahps_writer.h`，本片 26 份已在 §3.1–§3.25 全部列出（`ahps_writer.h` 计为 §3.20，`ahps_format.h` 计为 §3.21）。

---

## 4. 发现清单

### 4.1 阻断（7 条）

| # | 位置 | 性质 | 出处 |
|---|---|---|---|
| **B1** | `aio_hips_writer.cpp:1994-2011` | `metadata.fits` 创建失败整块静默跳过（`if (!fits_create_file)` 取反失败 ⇒ 无 else/无 set_error/无 return false）⇒ 硬失败降级为 rc=0；同块 5 次 `fits_write_key_*` + `fits_close_file` 返回值不查；且直写正式路径（本文件 `:442-459` 注释自认已修复的同型形态） | `[本人]` |
| **B2** | `aio_hips_writer.cpp:2645-2649`, `2674-2677`, `2710-2719`, `2787` | `verify_product_set` 三处 `if(...)` 无 else 把读侧失败吞成 `-1`/空串哨兵，配合 `>0`/`==1`/`==0` 守卫 ⇒ **V3/V4/V6 三组断言从「判红」退化为「跳过」，rc=0**。同模块读侧 `aio_hips_reader.cpp:408-421` 对同一事实严格 fail-closed | `[本人]` |
| **B3** | `test_query_pixel.cpp:93-170` | 「独立 oracle」是生产 `hiss_reader.cpp:160-249` 的**逐字副本**（已 `diff` 复核）⇒ 球面变换判别力为零；`:11` 的「两条独立生产路径交叉验证」不成立（`read_tile` 不经球面变换）。仓内独立实现 `healpix_core.cpp:77-149` 未用 | `[本人]` |
| **B4** | `test_drizzle_integration.cpp:351`, `:358` | **字面重言式恒真门**：`ret == 0 \|\| ret < 0` 对 `aio_hiss_query_pixel` 的全部可能返回值恒真（错误码 `0,-1,-2,-8`）；该测试对 signal/support 读数零断言 | `[本人]` |
| **B5** | `fits.cpp:360-369`（配合 `:345`） | `end_hdu` 零填充掩盖「数据未写全」：`write_data` 只设上界、无「必须写满」门 ⇒ `product_io.cpp:50-52` 在 `layer.data` 为空时整个跳过写入，产出**全零但 DATASUM/CHECKSUM 全对**并原子发布的产品；`verify` 抓不到（`:613` 重算用的是盘面零字节，`:649` 比对的 NAXIS 来自同一 spec）⇒ **判据与被检量同源** | `[SA-3]` |
| **B6** | `test_healpix_io_py.py:32`（466 行）、`test_export_fits_fix.py:27`（282 行） | **748 行（占本片测试代码 30%）在 import 阶段即死**：`healpix_io` 与 `astro_image_io` 两个 Python 模块均不存在（本人复验）。前者 `:344-351` 还打印 `[OK]` 却从未做过其声称的检查 | `[SA-5/本人]` |
| **B7** | `healpix_db/README.md:16-29` | 模块表 6 行 + 归档 5 行**路径声明无一为真**；`healpix_stack` 的「活跃 + .gitignore 忽略」被三重证伪（`check-ignore` NOT-IGNORED、`ls-files` 追踪 37 文件、与同目录 `.gitignore:21-22` 冲突） | `[SA-4/本人]` |

### 4.2 须修（15 条）

| # | 位置 | 性质 |
|---|---|---|
| M1 | `aio_hips_writer.cpp:1481` | **筛掉真信号（本轮唯一 5 个子代理均未命中）**：hierarchy 累加处 `!isfinite(flux) ⇒ continue` 把「有覆盖 ∧ 信号不可用」像素的 **area 一起丢掉**，与叶面 `:1366-1383`/`:1047-1049` 明写的解耦语义正面矛盾 ⇒ 叶面 `sup>0` 而父面 `sup=0`。`p1hips_tests_negative.cpp` N3b 只锁叶面（`FIX_NSIDE=512 ⇒ tile_order=0 ⇒ hierarchy 为空）⇒ 该面无任何测试 |
| M2 | `aio_hips_writer.cpp:2058-2072` | SNR `.tsv` 裸 `fopen/fputs/fprintf/fclose`，`fclose` 返回值丢弃且非原子落盘；同目录 sibling 全走 `aio_atomic` |
| M3 | `aio_hips_writer.cpp:1947` vs `:2457` | `acsd_covered_sky_fraction` properties(6dp) 与 manifest(8dp) **写出两个不同字面量**；`:1846-1852` 已为此建 `fmt_sky_fraction` 但紧邻的同一键未修 |
| M4 | `fits.cpp:524`/`:674`/`:679` | 空输入 fail-open：0 字节文件被判「验证通过」 |
| M5 | `fits.cpp:76-82` | 长字符串卡截断掉闭合引号产出**非法卡**，而本仓 `parse_one_card:444-459` **自洽接受**（外部 cfitsio/astropy 读失败） |
| M6 | `fits.cpp:162-176` | `fits_decode_checksum` 是恒真门（无输入校验，无条件 `return true`）；其唯一消费方断言因此恒绿 |
| M7 | `p1hips_tests_negative.cpp:295-297`, `:398-400`, `:680` | root 下 N4/N6 整块跳过、不发任何 CHECK，却无条件打印「N1..N11 PASS」；N6 是**全仓唯一**验证冻结不变量 I9 的地方 |
| M8 | `p1hips_oracle.hpp:1`, `:4`, `:82-88`, `:139-152`, `:156-160`, `:120-125` | 文件头自证「不复制同一实现」不成立（FITS 映射与 MOC UNIQ 均为生产逐字转写；`hierarchy_pixel_expect` 代数退化为恒等映射且不含求和）；`leaf_variance` 在合法态上与生产不一致（NaN vs 0/0） |
| M9 | `aio_ahpx_api.cpp:279-283` | `getImageInfo` 失败时 `*width/*height` **完全不被写**却 `return 0` ⇒ 调用方读未初始化内存；`readSnr` 无几何/长度校验，文档承诺「W×H」不成立 |
| M10 | `aio_abi.cpp:58-81`/`:151-154`/`:172-175`/`:135-138` | 环境变量 `ACSD_AIO_FAULT=n2_verify_mismatch_shortcut` **无条件关闭篡改检测**，且该 TU 在生产 `acsd_aio` target 内；`n1` 改写摘要首字符仍返回 `AIO_OK` |
| M11 | `aio_abi.cpp:101` | ABI 布局锁只比 `sizeof` 不比 `offsetof`；`aio_abi_info_v1` 不在仓内任何逐字段布局锁的覆盖内 |
| M12 | `ahps_reader.cpp:422-429` | float32 `E[x²]−E[x]²` 灾难性抵消 + `:428` 静默钳位把抵消失败压成「方差为 0」；无 NaN/Inf 守卫 |
| M13 | `gradient_fitter.h:65-67` | `sigma_clip_floor = 0.0` 的出厂默认使 sigma-clip **恒红**（`mad==0 ⇒ threshold==0 ⇒ 该帧全部控制点被剔 ⇒ 静默整帧降级为不校正），与注释声明的目的**正好相反** |
| M14 | `snr_evaluator.cpp:321` | 私建并行域 `#pragma omp parallel for`（本项目规范明禁，池所有权归调度器）；且**活生产副本保留同一 pragma** |
| M15 | `ahps_format.h:50`/`:52-57`、`:12`；`ahps_writer.h:18`、`:68`/`:78` | ①「14 字节」实为 16 且无 pack ⇒ 每像素 2 字节**未初始化填充被写进文件**；②`PixelCount` 4 字节无法寻址示例自己给的 nside=32768；③`ahps_writer.h` 用 `FILE*` 未 include `<cstdio>`，头不自足 |

### 4.3 建议（10 条）

| # | 位置 | 性质 |
|---|---|---|
| S1 | `test_drizzle_integration.cpp:419`/`:519` + `:450`/`:549` | 用了 `hiss_format.h:61` 明文点名为「已修正旧错误」的公式（3072 vs 正确 16，错 192 倍），绿灯靠「输出长度 == 同一份输入缓冲区长度」这一自洽式断言；正确值 `g_n_leaf:256` 取出后从未参与断言 |
| S2 | `test_drizzle_integration.cpp:303-311` | 遇首个 NaN 即 `break`，`has_nonzero` 只在前缀上判定、打印的 min/max 只覆盖前缀 |
| S3 | `test_query_pixel.cpp:1044-1055`/`:1067-1080`/`:1092-1105` | 随机抽样遇未命中即 `continue`，最终只断言 `checked > 0` ⇒ 16 个像素里 1 个对就通过 |
| S4 | `test_query_pixel.cpp:852-858` | `if (loc40.local_ipix == 40) { …三条断言… }` 条件包裹，扫不到就整块静默跳过 |
| S5 | `test_p0_io_hardening.cpp:87-95`+`:196-202` | **恒真门**：fixture 只有 24 字节无 XML 体 ⇒ 删掉 `aio_xisf.cpp:467` 的 64 MB 上限判断，代码仍落到 `:475` 短读拒绝 ⇒ CHECK 依旧绿。T2 对 P0-4 无界分配防护零判别力 |
| S6 | `hips_core_selftest.c:5-10` vs 正文 | 头注释声明的 4 项覆盖正文一项未测（`ACS_HIPS_TILE_INVALID` 零引用）；`:200-206` 的 `run_missing_props` 需 `@` 前缀而 CTest 只传 fixture_dir ⇒ 该负例**在 CI 下永不可达**，且两处用法说明都漏了 `@` |
| S7 | `validation.h:24` + `:26-29` | 空报告恒绿（类型上无「已执行」标志位）；`add()` 不强制 `:19` 自述的 message 非空 |
| S8 | `pyproject.toml:20-21`、`:17-18` | `packages.find` 匹配不到任何包（wheel 空包）；声明 `pytest` 依赖却无任何 pytest 配置消费 |
| S9 | `aio_hips_writer.cpp:2471-2474` vs `:2428-2436` | 同一 manifest 内对「是否存在」用了两套判据（`products` 用真值源 `diag_*_tiles`，`n*_tiles` 计数用 `ps->flags`） |
| S10 | `test_healpix_io_py.py:1-4`、`aio_ahpx_reader.h:25-27`、`test_precision_dual.cpp:15-22`、`test_p0_io_hardening.cpp:20-28` | 悬空/失效文档：编排器名 `orchestrator.exe` 不存在；C++ 用法示例三处缺 `()`；两份编译说明指向不存在的 `eng/tests/` |

---

## 5. 你主动构造的反例

共 8 个。**5 个推翻（成立），3 个未推翻（否决我自己）。**

### 5.1 ✅ 反例 A（成立）：删掉 `metadata.fits` 的失败分支，产品仍报「完成」
**构造**：让 `finalize_image_product` 在 `dir` 为只读卷时运行。
**期望推翻**：`:1995` 的 `if (!fits_create_file(...))` 会因失败而跳过 ⇒ 若上层有兜底，就不是阻断。
**是否推翻**：**否，成立**。逐步走查：跳过 ⇒ 无 set_error ⇒ 落到 `:2019 write_moc_fits`（`uniq` 非空时成功）⇒ `:2020 return true` ⇒ `aio_hips_finalize` 该子产品分支 `if (!finalize_image_product(...)) return -3` 不触发 ⇒ 上层拿到 0。**磁盘上无 metadata.fits，报告上「子产品已 finalize」。** 已在当前 HEAD 复验行号（§8 命令 1）。

### 5.2 ✅ 反例 B（成立，**本轮最强**）：产品写到一半被 kill ⇒ `verify` 对残缺产品签发绿灯
**构造**：`begin` → `write_signal_support_tile` ×N → 在 `manifest.json` 原子写之前被 kill。
**期望推翻**：若 verify 对「manifest 缺失」fail-closed，则 `:2645` 的 `if(ds)` 吞 nullptr 不是问题。
**是否推翻**：**否，成立**。逐步走查（`n_signal_tiles=-1`）：
① `:2637` `signal/properties` 存在 ⇒ sprops 非空 ⇒ 过；
② `:2645` `aio_hips_open` 返回 nullptr ⇒ `if(ds)` 假 ⇒ **无 else、无 set_error** ⇒ `n_signal_tiles` 停在 `-1`；
③ `:2653` prov 未设 ⇒ 跳过；
④ `:2690`/`:2700` `uncertainty_available` 停在 `-1`（`:2633`）⇒ **两分支都不进**；
⑤ `:2710` manifest 不存在 ⇒ `mdoc` 空 ⇒ `:2730` `!mdoc.empty()` 假 ⇒ nrej/nused declared=false；磁盘也无 ⇒ present=0 ⇒ `:2758`/`:2763` 都不触发；
⑥ `:2776` `unreadable_tiles=0`、`:2781` `diag_negative=0`；
⑦ `:2787` `!mdoc.empty()` 假 ⇒ V6 整段跳过；
⑧ `:2823 return 0`。
**即使 MOC 一起删**：`n_signal_tiles=0` ⇒ `:2693`/`:2768` 的 `>0` 守卫为假 ⇒ 断言变空真 ⇒ 仍 `return 0`。
**这是「断言的使能条件与被检量共用同一个易变量」的变体**：不是断言写错，而是参照量取不到时断言**静默变空真**而非 fail-closed。

### 5.3 ✅ 反例 C（成立）：重复写同一叶 tile（祖先未完备时）⇒ 父级 variance 恰好减半、support 虚高
**构造**：设 `tile_order=K`，祖先 cell A 在 k=K−1（`dk=1`，`slots_total=4`）。只写叶 P 的 1 个兄弟 ⇒ `slots_seen=1 < 4` ⇒ A 永不出流式。序列 `signal(P) → variance(P) → signal(P) → variance(P)`。
**期望推翻**：若重复写被 `new_leaf` 拦住，则不成立。
**是否推翻**：**否，成立**。逐步走查：
- `signal#1`：`new_leaf=true` ⇒ `:1465` `++slots_seen`、`pending_var_slot=s`；`:1472-1498` `acc.add(z,f,a)`
- `variance#1`：`:1680` `pending==s` ⇒ `++var_slots_seen`、`pending=MAX`；`:1688-1692` `add_var(z,v)`
- `signal#2`：`new_leaf=**false**` ⇒ `:1438` `hier_duplicate_allowed` 只在祖先**已 flushed** 时返回 false，此时 A 未 flushed ⇒ 返回 true ⇒ **不返回 −8**；`:1465` 整块跳过（槽计数与 pending 都不更新）⇒ `:1472-1498` **再次** `add(z,f,a)`
- `variance#2`：`pending=MAX≠s` ⇒ `:1688-1692` **再次** `add_var(z,v)`
⇒ `Σflux=2f, Σarea=2a, Σvar_num=2v`。
父级发布面（`:1116-1117`）：`var_parent = 2v/(2a)² = v/(2a²)` = **正确值的 1/2**，ivar 翻倍；`sup_parent = min(2a/A_cell_k, 1)` ⇒ support 系统性虚报；`:1441` `covered_area_sr` 二次累加。
**附带**：`:1488-1490` 明文写的 P8 不变量「每个祖先像素 z 在整个产品集生命周期内恰好被一个叶 tile 写一次」在该序列下被打破 ⇒ 浮点结果重新变成到达序相关，`HIPS-DETERMINISM-01` 的论证前提失效。

### 5.4 ✅ 反例 D（成立）：有覆盖 ∧ flux=NaN 的叶 ⇒ 叶面 `sup>0`、父面 `sup=0`
**构造**：调用方传 `covered_area = 0.5·A_cell > 0`、`flux_sum = NaN`，`tile_order ≥ 1`。
**期望推翻**：若 hierarchy 与叶级用同一 covered 判据，两面应一致。
**是否推翻**：**否，成立**。叶级 `:1374-1390`：`covered = v && area>0 && isfinite(area)` 成立 ⇒ `sup = area/A_cell > 0`、`sig = NaN`、`area_true = area`。父级累加 `:1479-1481`：`flux = NaN*area = NaN` ⇒ `!isfinite(flux)` ⇒ **`continue`，area 未累加** ⇒ 父 cell 该 z 的 `areaD` 恒 0 ⇒ `write_hierarchy_cell:1050` `covered = area>0` 为假 ⇒ 父面 `sup=0`。
⇒ **同一产品内叶面与父面 support 不自洽**，且与 `:1047-1049` 注释「父级 support 也必须在『有覆盖但信号不可用』时发布，否则层级面的覆盖并集被低估」**正面矛盾**。**5 个子代理全部未命中此条。**

### 5.5 ✅ 反例 E（成立）：`fits.cpp` 零填充 ⇒ 全零产品通过重开验证
**构造**：`FitsHduSpec` 声明 512×512，`layer.data` 为空 ⇒ 生产 `product_io.cpp:50` 的 `!layer.data.empty() && …` 短路 ⇒ `write_data` 从未调用。
**期望推翻**：若 `end_hdu` 有「必须写满」门，或 `verify` 能看出全零，则不成立。
**是否推翻**：**否，成立**。`:345` 只设上界；`:360-369` 把「2880 对齐补零」与「整行补零」混成同一分支并 `return true`。验证侧：`:613` 重算 DATASUM 用的正是盘面字节，而 `:359` 注释明写「零字不改变 1 补码和」⇒ DATASUM 必然吻合；`:649` 比对的 NAXIS 来自同一 spec ⇒ 必然吻合。**验证器没坏，坏的是缺完整性门；判据与被检量同源。**

### 5.6 ❌ 反例 F（否决我自己）：`test_query_pixel.cpp` 的数值断言也是自洽的 ⇒ 阻断-3 应降级
**构造**：假设 `setup` 写数据与 `find_tile_pixels` 定位共用同一套 local_ipix 约定 ⇒ `:524` 的 `sig==100` 也是自洽的。
**是否推翻**：**推翻**。`setup:332-336` 写的是 `acc.pixels[i].sum_flux = (double)i*10.0 + 100.0` —— **按数组下标 `i` 写**，与任何球面变换无关；期望值 100/180/250 是**硬编码字面量**。所以「哪个 ra/dec 映射到哪个下标」这一段确实是零判别力（阻断-3 成立），但「读出的数值对不对」这一段有真实判别力。⇒ **阻断-3 限定在球面变换维度，不扩散到数值维度。**

### 5.7 ❌ 反例 G（否决我自己）：`a1=1e300` 哨兵会泄漏成假 data_range ⇒ `:2347` 是恒红门
**构造**：若某像素 `sig = 1e301`，则 `:1386` 的 `sig < ps->sig_min` 为假 ⇒ `sig_min` 保持哨兵 `1e300` 而 `sig_max = 1e301` ⇒ 发布 `"1e+300 1e+301"`。
**是否推翻**：**推翻**。`sig = flux/area` 达 1e300 需 flux ~1e294 sr⁻¹，非物理可达。更重要的是 `:2347` 的守卫**不是**恒红门：初值 `1e300/-1e300`，而 `:1386-1387` 在**同一个 if 块内**同步更新 ⇒ 只要有一个像素走完该分支就必有 `sig_min ≤ sig_max`。守卫精确等价于「至少有一个有限 signal 被发布」。**不列为缺陷。**

### 5.8 ❌ 反例 H（否决我自己）：`:1481` 的 `!isfinite(flux)` 会把**最差**那批像素筛掉 ⇒ 典型「筛掉真信号」
**构造**：若该 continue 把方差/异常像素滤掉，则局部绿掩护真红灯。
**是否推翻**：**推翻（针对本条）**。`:1481` 的筛除条件是 `flux` 非有限，而 `flux = sig_n[i]*area_n[i]`；非有限 flux 只在 `sig_n[i]` 非有限时产生（`area` 已保证有限且 >0），而这**正是**信号不可用的像素 —— 它们不是「最差」而是「不可用」，滤掉是有意的。**但滤掉的副作用是 area 一起丢**，这才是 5.4 的真缺陷。⇒ 缺陷成立但**归因要精确到「area 未被累加」而非「continue 本身」**。

---

## 6. 盲复算

**做法**：先封存既有判定（`分片清单/逐份判定-权威版.csv`），再遮住它独立取证，最后比对。

**既有判定**：`grep "INF-aio-002" 逐份判定-权威版.csv` ⇒ **26 行，tier 全部 `HUMAN`，reason 全部是同一句「默认保留（非产出面或非数据形态）」**。

| 口径 | 结果 |
|---|---|
| 逐份判定与本轮是否一致 | **0/26 一致**（26 份全部从「默认保留」上调） |
| 整体判定方向 | **偏严** |

**偏严的依据（不是口味差异，是可证伪的口径差异）**：

1. **既有口径把 26 份打成一个字符串。** 同一句 `默认保留（非产出面或非数据形态）` 同时套在 2935 行的生产 HiPS writer 和 21 行的 `pyproject.toml` 上。它不携带任何逐份证据，因此**不可能发现**本轮任何一条发现 —— 这本身就是「恒真门」在台账层的形态：**分类判据对全部成员恒成立 ⇒ 零判别力**。
2. **`默认保留（非产出面或非数据形态）` 这一 reason 在本片至少对 5 份是事实错误**：§3.1（生产 writer，三条 fail-open）、§3.3（生产 `fits.cpp`，零填充 + 空输入 fail-open）、§3.14（生产 `aio_abi.cpp`，注入面在生产 target 内）、§3.9/§3.10（**两份测试从不执行**，正是「非产出面」的最坏形态 —— 一份死掉的「非产出面」代码占本片测试代码 30%）、§3.8（生产 `aio_ahpx_api.cpp`，成功返回但输出未写）。
3. **本轮 7 条阻断全部落在「生产源码」上**，而既有口径把它们归为「非产出面或非数据形态」—— 该分类把生产 writer 排除在「产出面」之外，与 AGENTS §1（三个命令各自产出 HiPS/FITS 产品）不符。

**本轮判「偏严」而非「相反」的部分（自身校准）**：本片确有几处被既有口径正确覆盖 —— §3.24（`tests/abi/CMakeLists.txt` 的 selfcheck 反向证伪机制是本片唯一真判据）、`tests/p1hips/p1hips_oracle.hpp:61-78` 的位解交织、`.ahps` 固定头 38 字节布局自洽、`write_fits_atomic` 全链。这些我确认无问题，与既有「默认保留」不冲突。**差异集中在 7 条阻断 + 15 条须修上。**

---

## 7. 子代理派发记录

**派发 6 个**（任务要求 3-5；因一次重复派发，实际覆盖 **5 个互不重叠的片组**）。全部后台并发，无失败。

| ID | 覆盖范围 | 读了多少 | 报告 |
|---|---|---|---|
| SA-1 | `aio_hips_writer.cpp` | 2935/2935 | 3 阻断 + 7 须修 + 5 建议 + **8 条否决** |
| SA-2 | `aio_hips_writer.cpp`（**与 SA-1 重复**） | 2935/2935 | 2 阻断 + 6 须修 + **6 条否决** |
| SA-3 | `fits.cpp`、`aio_ahpx_api.cpp`、`aio_abi.cpp`、`aio_ahpx_reader.h`、`validation.h` | 1357/1357 | 4 HIGH + 24 中低 + **7 条否决** |
| SA-4 | `ahps_reader.cpp`、`snr_evaluator.cpp`、`gradient_fitter.h`、`gradient_sampler.h`、`ahps_writer.h`、`memory.md`、`ahps_format.h`、`healpix_db/README.md` | 1441/1441 | 4×P1 + 16×P2/P3 + **10 条否决** |
| SA-5 | 测试组 9 份（`test_healpix_io_py.py`、`test_export_fits_fix.py`、`test_p0_io_hardening.cpp`、`hips_core_selftest.c`、`v5_snr_precision_roundtrip.py`、`p1hips_tests_negative.cpp`、`p1hips_oracle.hpp`、`tests/abi/CMakeLists.txt`、`pyproject.toml`） | 2507/2507 | 4 CRITICAL + 4 HIGH + 9 MEDIUM + **9 条否决** |
| SA-6 | 算法测试组 3 份（`test_query_pixel.cpp`、`test_precision_dual.cpp`、`test_drizzle_integration.cpp`） | 2390/2390 | 4 严重 + 6 高 + 7 中低 + **10 条否决** |

> SA-1 与 SA-2 重复派发同一文件（SA-2 的原定目标 `aio_hips_writer.cpp` 与 SA-1 撞车）。我未能在派发时纠正（`send_message` 的 target 在本会话不可寻址）。**处置**：两份报告都保留，**取其交集作为结论**（见下表），差异部分一律由本人亲自复核后再采信；SA-2 独有结论我未独立复算的一律标 `[SA-2]` 且不升为阻断。

### 逐条复核：采纳 / 部分采纳 / 否决

**独立复核后采纳并升级为阻断（4 条）**：

| 结论 | 提出者 | 本人复核 | 处置 |
|---|---|---|---|
| `metadata.fits` 创建失败整块静默跳过 | SA-1、SA-2 | 本人独立走查 `:1995` 的取反语义 + `:2019/:2020` 的控制流，并在 HEAD `1fa477a7` 复验行号 | **采纳为 B1** |
| `verify` 的 `if(ds)` / `if(file_exists)` 无 else 造成 fail-open | SA-1、SA-2 | 本人**独立构造反例 B**（逐行走查 8 步），并复验 `:2690/:2768/:2693` 的守卫短路 | **采纳为 B2** |
| `acsd_covered_sky_fraction` 双面字面量分叉 | SA-1、SA-2 | 本人复验 `:1947` 与 `:2457` 两行原文；量化（`std::to_string` 步长 5e-7 = 容差 1e-9 的 500 倍）本人重算 | 采纳为 **M3**（降为须修：需确认是否有下游按字面量比对） |
| `test_drizzle_integration.cpp:351` 字面重言式 | SA-6 | 本人**独立读 SUT** 确认错误码域（`HIO_OK=0,-1,-2,-8`）⇒ 恒真 | **采纳为 B4** |

**独立复核后采纳但降级（3 条）**：

| 结论 | 提出者 | 复核 | 处置 |
|---|---|---|---|
| `p1hips_oracle.hpp` 整体「是自洽式断言」 | SA-5（F13 自称"待调用侧复核，暂不判绿/红"） | 本人逐行核对：`:61-78` 位解交织、`:162-164` 面积、`:176-182` 球面余弦**确为独立实现**；但 `:82-88`/`:156-160` 确为生产转写，`:139-152` 确为代数退化 | **部分采纳为 M8**：不判「整个 oracle 自洽」，只判「三条具体公式非独立 + `leaf_variance` 与生产不一致」 |
| `root 下 N4/N6 跳过` 判 CRITICAL | SA-5（F2） | 本人复核 `:295-297`/`:398-400`/`:680` 原文，确认与 `hips_core_selftest.c:439-440`（skip 记为 FAIL）**方向相反** | **采纳为 M7**（维持须修而非阻断：N4/N6 在非 root 环境仍执行，且 CI 环境未核实为 root） |
| `sigma_clip_floor=0.0` 判 P1 | SA-4（N5） | 本人复核 `gradient_fitter.h:65-67` 默认值与注释目的确实相反；**但触发的 `.cpp` 不在本片** | **采纳为 M13**，标注「触发的实现在同归档树非本片，未实测」 |

**独立复核后否决 / 降级的子代理结论（4 条）**：

| 结论 | 提出者 | 否决理由 |
|---|---|---|
| `PixelStats` 实为 16 字节 ⇒ 2 字节未初始化填充被写进文件 | SA-4（N3） | **部分采纳**：布局推导本人独立重算无误（`uint16@0-1` → 填充 2-3 → `float@4/8/12` → 16），写入侧 `ahps_writer.cpp` **不在本片**且未实测。采纳为 **M15**，明确标注「sizeof 按 C++ 布局规则推导，非实测」 |
| `snr_evaluator` 的 `idw_power` 缺省 2.0 与活副本 1.0 分叉 ⇒ 归档复活即改科学结论 | SA-4（N1/N2） | **采纳发现但归因降级**：活副本 `lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.*` **不在本片**，本人只复核了归档侧 `:196-197`/`:252-253` 的 `2.0` 与 `gradient_sampler.h:77` 的硬编码 `2.0`。`DISPUTE_RESOLUTION.md:74` 的终裁内容由 SA-4 举证，**本人未读该文件**。⇒ 记入 §8 待核清单，**不计入阻断数** |
| `verify_impl` 空输入 fail-open（0 字节判通过） | SA-3（F-09） | **采纳为 M4**，但**缓解已核实**：`fits.h:144` `bool ok = false` 确认为默认值 ⇒ `:695`/`:701` 两个错误分支是 fail-closed 的，空输入是**唯一**漏口；且生产两处调用均传 `expected`（是否恒非空 SA-3 自己标注为未确认）⇒ 维持须修 |
| `p1hips_tests_negative.cpp` N2/N3 负向输入「构造成功」 | SA-5（列为确认无问题） | **本人独立复核并采纳该正面结论**：`FIX_NSIDE=512 ⇒ tile_order=0 ⇒ npix_order=12`，`:201` 的 `parent_ipix=12` 是紧邻边界的真越界值；`:228` 的 `width=128`、`:197` 的 `data_type=FLOAT32`（产品为 FLOAT64）均为真变异。**这是本片少见的正面确认，予以记录。** |

**SA-1 与 SA-2 的分歧（取交集）**：两份报告在 3 条阻断上**完全一致**（metadata.fits、verify fail-open、SNR `.tsv`），在「重复叶写导致父级 variance 减半」上**结论一致但触发路径描述略异**（SA-1 强调 variance 侧 `pending_var_slot` 消耗，SA-2 强调 signal 侧 `new_leaf` 不约束 `add`）。本人**独立重走两种调用序**（见 §5.3）后确认**两种描述指向同一缺陷**，采纳并归并为 §5.3 单条。

**子代理整体可信度评估**：5 个子代理共提出 **约 95 条**候选结论，本人逐条比对后**采纳 26 条**（含 4 条阻断）、**部分采纳 3 条**、**否决/降级 4 条**，其余为「建议级」或与本人结论重复。**子代理的自查质量高**：合计 **50 条主动否决**（SA-1:8, SA-2:6, SA-3:7, SA-4:10, SA-5:9, SA-6:10），且 SA-3 明确写「否决 1 需要读被调方实现，仅凭 api.cpp 会误报」—— 这种自我限制提高了报告可信度。**但有一条系统性盲区**：6 个代理**全部未命中** §3.1 第 4 条（`:1481` 筛掉真信号）与 §3.4 第 3 条（N3b 只锁叶面 ⇒ hierarchy 面零覆盖）。这两条是本轮我亲自通读才发现的，**说明「恒真门/退役核查」这类关键词驱动的复核有结构性盲区**。

---

## 8. 自证段（可复跑命令）

**基线漂移声明**：审稿期间仓库 HEAD 从任务单的 `850a9ede` 漂移到 `1fa477a7`（并行车道提交）。**所有 §3 中的关键行号已在 `1fa477a7` 复验**（命令 1）。记为 `UNRESOLVED-BASE`，需前台裁定采信哪个基线。

```bash
# 0) 纪律自证：本会话未做任何 git 写
cd "/workspace/Astro CS Database" && git status --porcelain | head -20
#   注：工作树有并行车道的改动（docs/、run/），非本审稿人所致。

# 1) [B1][B2][M1][M2][M3] 五处关键行号复验 @ 1fa477a7
cd "/workspace/Astro CS Database" && git rev-parse --short HEAD
sed -n '1993,1996p;2010,2011p' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
sed -n '2644,2649p;2708,2711p' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
sed -n '2058,2059p;2072p'       lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
sed -n '1479,1482p'             lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
sed -n '1947p'                  lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
sed -n '2457p'                  lib/infrastructure/aio/src/hips/aio_hips_writer.cpp

# 2) [B3] 「独立 oracle」= 生产逐字副本（可执行语句零差异）
cd "/workspace/Astro CS Database/lib/infrastructure/aio"
sed -n '160,249p' src/hiss_reader.cpp | sed 's/hpx_/oracle_/g' \
  | grep -v '^\s*//' | grep -v '^\s*$' > /tmp/prod_ang.txt
sed -n ' 93,170p' tests/test_query_pixel.cpp \
  | grep -v '^\s*//' | grep -v '^\s*$' > /tmp/test_ang.txt
diff /tmp/prod_ang.txt /tmp/test_ang.txt
#   预期：仅尾随注释 + 缩进差异；可执行语句零差异

# 3) [B4] test_drizzle_integration 的恒真门 —— 错误码域佐证
cd "/workspace/Astro CS Database/lib/infrastructure/aio"
sed -n '351p;358p' tests/test_drizzle_integration.cpp
grep -n "HIO_OK\|HIO_ERR_PARAM\|HIO_ERR_FILE\|HIO_ERR_INTERNAL" src/healpix/aio_healpix_io.cpp | head
#   预期：CHECK(ret == 0 || ret < 0, ...)；错误码 0/-1/-2/-8 全 ≤0 ⇒ 恒真

# 4) [B6] 两个 Python 模块确不存在 ⇒ 748 行测试死在 import
cd "/workspace/Astro CS Database/lib/infrastructure/aio"
ls tests/healpix_io.py python 2>&1            # 两个都 No such file
ls healpix_db/healpix_io/                     # 只剩 ARCHIVED.md
git -c core.quotepath=false ls-files | grep -iE "astro_image_io.*\.py$" | wc -l   # 预期 0
#   对照：test_export_fits_fix.py:23 的 _PYTHON_DIR 指向 lib/infrastructure/aio/python

# 5) [B7] healpix_db/README 的悬空路径（9 条，逐条 test -e）
cd "/workspace/Astro CS Database"
for p in lib/infrastructure/aio/healpix_db/healpix_browser_qt \
         lib/infrastructure/aio/healpix_db/healpix_stack \
         lib/infrastructure/aio/healpix_db/healpix_drizzle \
         lib/infrastructure/aio/healpix_db/archive/healpix_browser_cpp \
         lib/infrastructure/aio/healpix_db/archive/healpix_browser_web \
         lib/infrastructure/aio/healpix_db/archive/legacy/healpix_browser_python \
         lib/infrastructure/aio/healpix_db/archive/legacy/healpix_lod \
         lib/infrastructure/aio/healpix_db/archive/legacy/tests .trae ; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"; done
#   预期：8 条 MISSING（仅 healpix_db/docs 为真，见正文）

# 6) [B7] 「.gitignore 忽略」声明被证伪
cd "/workspace/Astro CS Database"
git check-ignore -q lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/ahps_format.h \
  && echo IGNORED || echo NOT-IGNORED          # 预期 NOT-IGNORED
git -c core.quotepath=false ls-files lib/infrastructure/aio/healpix_db/archive | wc -l   # 预期 37
sed -n '20,23p' lib/infrastructure/aio/healpix_db/.gitignore   # 「不再忽略」—— 与 README:18 正面冲突

# 7) [M15][S] ahps_format.h 的 PixelCount 4 字节无法寻址示例给的 nside=32768
python3 -c "print('12*32768^2 =', 12*32768**2, ' 2^32-1 =', 2**32-1, ' overflow =', 12*32768**2 > 2**32-1)"
sed -n '12p;50,57p' lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/ahps_format.h
sed -n '18p'       lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/ahps_writer.h

# 8) [M4][S] 归档 Makefile / memory.md 的悬空引用
cd "/workspace/Astro CS Database"
for f in stage2_config.json healpix_io.dll gaia_client.dll healpix_stack.dll ; do
  echo -n "$f -> "; find . -name "$f" -not -path './run/*' -not -path './build/*' 2>/dev/null | wc -l ; done
#   预期：全 0 命中

# 9) [S5] T2 恒真门：fixture 只有 24 字节（8 magic + 8 长度，无 XML 体）
sed -n '87,95p;196,202p' lib/infrastructure/aio/tests/test_p0_io_hardening.cpp
#   预期：write_xisf_huge_xmllen 只写 magic+长度即 fclose，无 XML 体

# 10) 覆盖率自证：26 份 / 10630 行
cd "/workspace/Astro CS Database/run/GOVERN-08/审核包-R2/分片清单"
grep -A 28 '片号: INF-aio-002' 片清单-权威版.yaml | head -30
grep "INF-aio-002" 逐份判定-权威版.csv | wc -l    # 既有判定 26 行，reason 全同
awk -F, '$7=="INF-aio-002"{s+=$2} END{print "总行数 =", s}' 逐份判定-权威版.csv   # 预期 10630
```

---

## 9. 待前台核实清单（本轮未验证，不计入结论）

| # | 事项 | 原因 |
|---|---|---|
| `UNRESOLVED-BASE` | 采信 `850a9ede` 还是 `1fa477a7` | 审稿期间并行车道提交导致 HEAD 漂移；关键行号已在 `1fa477a7` 复验，但若以 `850a9ede` 为准需整体重跑 |
| U1 | `docs/science/DISPUTE_RESOLUTION.md:74` 的 `idw_power` 终裁内容 | SA-4 举证，本**人未读该文件**；该文件不在本片 |
| U2 | 活副本 `lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.*` 的 `idw_power` 缺省是否为 1.0 | 不在本片，未复核；决定归档副本分叉的实际危害面 |
| U3 | `ahps_writer.cpp` 写盘处是否真把 2 字节未初始化 padding `memcpy` 进文件 | 该 `.cpp` 不在本片；M15 按 C++ 布局规则推导，非实测。可加一条 `static_assert(sizeof(ahps::PixelStats)==14)` 红灯坐实 |
| U4 | CI/Docker 是否以 root 运行 `p1hips_negative` | 决定 M7 是「须修」还是「阻断」；root 下 I9 零覆盖 |
| U5 | `lib/infrastructure/aio/README.md` 是否存在（`pyproject.toml:7` 的 `readme` 指向它） | 未核 |
| U6 | `healpix_db/README.md:10` 引用的外部仓库（`fujiaze/Healpix-Database` 等）引入的第三方代码许可 | AGENTS §4 要求传染性许可只读不复制；归档树内是完整本地副本，建议做许可复核 |
| U7 | `product_io.cpp` 两处调用是否恒传非空 `expected` | 决定 M4 的实际暴露面（SA-3 自标未确认） |
