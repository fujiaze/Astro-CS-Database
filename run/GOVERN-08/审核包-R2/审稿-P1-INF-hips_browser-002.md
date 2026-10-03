# 审稿-P1-INF-hips_browser-002

> G08-05 对抗审稿 第 1 遍 · 片 `INF-hips_browser-002`
> 层：`lib/infrastructure/hips_browser` · 23 份 / 6039 行
> 审稿人：G08-05-P1 分片审稿 agent　基线：任务书声明 HEAD = `850a9ede`，**实测工作树 HEAD = `1fa477a7a05c315550df2ce2bafc3e64ed9bbd78`**（见 §8 偏差登记）
> 纪律：亲自读原文 / 红队姿态 / 零 git 写 / 未编译未运行 / 未改仓内任何文件 / 未读 `/tmp/acsd_g08/`

---

## 1. 读完了吗

**口径**：成员清单取自 `run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml` 的 `INF-hips_browser-002` 块；「读了多少行」= 本人用 `read` 工具从第 1 行读到 EOF 的物理行数（`wc -l` 等价）。

| 口径 | 数值 |
|---|---|
| 成员份数（清单声明） | 23 |
| 实际读到份数 | **23 / 23** |
| 成员总行数（清单声明） | 6039 |
| 实测总行数（逐份 `wc -l` 求和） | **6039**（与清单零偏差） |
| 实际读了多少行 | **6039** |
| **覆盖率** | **100.0%（份数 23/23，行数 6039/6039）** |
| 未读完的成员 | **无** |

逐份计数（实测 = 清单声明）：

```
1348 browser_cli.cpp        1096 main_window.cpp         536 hips_sky_view.cpp
 502 sphere_view.cpp         287 abstract_view.cpp        260 gen_geometry_truth.py
 240 test_geometry_truth.cpp 227 gl_renderer.h            182 CMakeLists.txt
 180 stf_engine.cpp          158 hips_sky_view.h          147 main_window.h
 134 gen_ref_source.py       110 healpix_math.cpp         104 stf_engine.h
  98 deploy.ps1               94 test_browser_dual_dtype.cpp 91 abstract_view.h
  82 test_stf_engine.cpp      56 stf_panel.cpp             54 Makefile
  48 healpix_math.h            5 run_healpix.bat
```

**跨片读取（仅用于验证本片断言，不计入覆盖率）**：`lib/algorithms/shared/healpix/tests/test_hips_tile_mapping.cpp`、`core/hips_browser_backend.cpp:220`、`core/healpix_math.h`、`CMakeLists.txt` 引用的路径。

**未读且属本片者：无。** 本片不含 `sphere_view.h` / `hips_view.cpp` / `gl_renderer.cpp` / `logger.h` / `main.cpp` / `browser_backend.cpp` / `hips_browser_backend.cpp` —— 这些属**其它片**，本报告只把它们当线索登记（§4-C），不替别的片下结论。

---

## 2. 本片判定：**需修**（偏重；含 2 条阻断）

**最重 3 条：**

1. **【阻断 B-1】`test_geometry_truth.cpp:226-227` 的 `cache_bounded` 是永真门，声称验证 LRU 有界却与缓存完全无关。**
   `sky.target_order()` 在 `hips_sky_view.cpp:152-160` 被 `:158 order = std::max(0, std::min(order, leaf))` 双向钳位，`:153` 另有 `if (!bk_) return 0;` ⇒ 返回值恒 ∈ `[0, leaf]`。故 `>= 0` **永不可能为假**。而被它「代表」的真实性质 `cache_.size() <= cache_cap_` 全片**无任何断言**，`cache_` 在 `hips_sky_view.h:153` 是 private，`HipsSkyView` 未暴露任何 cache 大小访问器。判据名字承诺的量与实际断言的量不是同一个量。

2. **【阻断 B-2】`hips_sky_view.cpp:376→451-452` 自动标尺在零样本时把 `±FLT_MAX` 写进持久态，产出「看起来正常」的纯黑图，且自称已计算。**
   完整推导见 §5 CE-6。关键链：`:399-400` 逐 tile 读失败 `continue`（无计数无日志）→ `:410`/`:429` 两处 `if (!samples.empty())` 均不成立 → `dmin/dmax` 保持 `:376` 的 `FLT_MAX/-FLT_MAX` → **`:445 auto_range_dirty_ = false`（不再重试）**、`:446 auto_range_computed_ = true`（谎称已算）、`:447 ++auto_recompute_count_`（`--stf-lock-probe` 仍判 PASS）→ `:451-452 lo_=+3.4e38, hi_=-3.4e38` → `:468` 取 `range=1.0f` → `:507/512/516` 每个有限像素夹到 0 ⇒ 全屏 `0xFF000000`。
   唯一能暴露它的信号 `stats_.valid_pixels`（`:450`）**写了就没人读**——本人 `grep -rn "valid_pixels" lib/ eng/` 独立复核：全仓仅 `hips_sky_view.cpp:450`（写）与 `hips_sky_view.h:98`（声明），**零消费者**。

3. **【阻断 B-3】验收层自身失效：默认构建把全部 `assert` 编译掉，且唯一非空洞的门被注册成恒红。**
   `CMakeLists.txt:16-18` 强制 `CMAKE_BUILD_TYPE Release` ⇒ `-DNDEBUG`。`tests/test_stf_engine.cpp`（本片）17 处裸 `assert` 全部被消除，`main` 无条件 `printf("ALL PASS")` + `return 0`（`:80-81`）。
   更糟：`tests/test_stf_engine.cpp:70-71` 断言的是**代码已明文删除的旧契约**——`stf_engine.cpp:167-171` 写 `(void)data_min; (void)data_max; // 保留接口兼容, 不参与 uniform 归一化` 且 `u.shadows = params.shadows`（=10.0），而 `:163` 注释直书「不参与 uniform 归一化」；`:161-162` 与 `stf_engine.h:88` 声称归一化。三方矛盾下，`|10.0-0.1| = 9.9 > 1e-5` ⇒ **一旦打开 assert，该测试第一条断言即确定性 abort**。当前全绿**只靠 NDEBUG 这个意外**。
   同时 `CMakeLists.txt:181 add_test(NAME geometry_truth COMMAND test_geometry_truth)` 不传参，而 `test_geometry_truth.cpp:44-47` 要求 `argc == 2` 否则 `return 2` ⇒ **该门永久 exit 2（恒红门）**，本片唯一真正能失败的测试内容从不执行。

   **本人独立复核追加（把 B-3 从「门是空的」升级为「门根本没接上电」）：本片 5 个 CTest 目标在本仓任何自动化执行器上都跑不到。**
   - `CMakeLists.txt:9 project(healpix_browser_qt LANGUAGES CXX)` 是**独立 project()**；
   - 根 `CMakeLists.txt` 中 `add_subdirectory` 涉及本目录的命中数为 **0**（本人 `grep -rn "add_subdirectory" --include=CMakeLists.txt . | grep -i hips_browser` → NONE）；
   - 根用 `option(ACSD_BUILD_TESTS ... ON)`（根 `:19`，在根 `:1463` 消费），本片用**另一个变量** `option(BUILD_TESTS ... OFF)`（本片 `:126`）⇒ 二者互不影响，根把测试打开也不会让本片编出一个测试。
   ⇒ 后果：本片缺陷同时具备「若 assert 生效则恒红」与「若 NDEBUG 生效则恒绿」**两种**失效，二者都是承重的；而在此之上还有第三层——**整组门根本不在本仓构建图里**。因此「把失败的测试改绿」不是本片的修法；修法是先接线，再谈判据。

---

## 3. 逐文件清单

> 每行：读了什么 → 看到什么 → 判定。凡「跨片」结论一律注明证据取自非本片文件，仅作线索。

| # | 文件 | 读到 | 看到（关键） | 判定 |
|---|---|---|---|---|
| 1 | `app/browser_cli.cpp` 1348 | 全 | ① `:232` 参考侧与被检侧共用 `nested_local_to_fits_index(z,9u,512u)`（注释 `:231` 自认「与 Browser 同一共享标准映射」）；② `:994` `atoi` 无校验 + `:204` 循环 ⇒ `--queries abc` ⇒ **恒真门 PASS**（§5 CE-3）；③ `:327-340` 空 `samples` 时 `size()-1` 无符号下溢 ⇒ 三处越界读，且 `:363 return 0` 无条件；④ `:1178-1186` 四个 double 无初始化、`:1223/:1266` 无条件读；⑤ `:220-226` 四个 `aio_hips_read_tile_*` 返回值丢弃；⑥ `:1029-1031` 未知 `--flag` 静默吞掉；⑦ `:1317-1318` `--frames 0` ⇒ 除零 ⇒ JSON 写入 `inf`/`nan`；⑧ `:65-69 key_str` 不转义；⑨ `:937-941` `fwrite` 返回值丢弃、无 temp+rename；⑩ `:1147 return 0` 非 HiPS 路径所有结局恒 0 | **需修** |
| 2 | `app/main_window.cpp` 1096 | 全 | ① `:694` 编码 `fov*100+layer` / `:733-734` 解码 ⇒ `layer` **恒解出 0**，「Support View」预设（`:686`）失效（§5 CE-11）；② `:884-885 close_file()` 漏放 `current_tile_signal_f64_`（对比 `:320-323` 有放）⇒ **每次开关文件泄漏**；③ `:309-312` 早退在释放旧 tile **之前**，`clear()` 触发 `currentRowChanged(-1)` ⇒ 泄漏；④ `:420/462` NaN → `(uint)NaN` **UB**（每像素）；⑤ `:808-811`/`:826-829` `save_snapshot`/`pm.save` 的 `bool` 不影响退出码；⑥ `:582-584` 以 `n_tiles` 为界索引可能更短的 `tile_ipix_list`；⑦ `:624/678/960` 三处重复的 `base.find("gc")` 子串选坐标；⑧ `:1069-1077` 面板覆写 `shadows/highlights` 为 0/1 ⇒ **B-2 的中毒态在 UI 上完全不可见**；⑨ `:809` `LOG_INFO("main_window", …)` 把 tag 当格式串，真消息成弃用实参 | **阻断**（B-2 出口面） |
| 3 | `core/hips_sky_view.cpp` 536 | 全 | ① §5 CE-6 全部；② `:83-84` 进程级 `static std::map` 只按 root 路径键、`:413` 写入后**永不失效**，`set_backend :88-93` 与 `reset_metrics :267-270` 都不清 ⇒ **自愈锚**（§5 CE-7）；③ `:28-30 clampf` 对 NaN 两比较皆假 ⇒ **原样返回 NaN**，`:99-100` 无 isfinite 校验；④ `:156` LOD 公式锚点硬编码 `7.0` 而 `:154` 的 `leaf` 已在作用域内；⑤ `:445-452` 状态谎报；⑥ `:401-408` 采样步长硬编码 512/16 且**无 `sig.size()` 检查** | **阻断** |
| 4 | `widgets/sphere_view.cpp` 502 | 全 | ① `:2/6-9/161-164/338` 注释宣称「双向量四元数 + Rodrigues」，实现 `:398-429` 是纯赤道仪相机；`:394` 注释「up 绝不携带」与 `:8`「up 不变 → 画面不旋转」**同文件自相矛盾**；② `:60-64` `reset_view` 两分支同赋 50.0（死分支）；③ `:36` ctor `fov_deg_(60.0)` 与 `:103` 的 else 分支 `=60.0` **绕过 `:101` 的 clamp**；④ `:311-313` `dist` 未防 0 ⇒ `scale=+Inf` ⇒ FOV 跳 `MAX_FOV`；⑤ `:288-297` ≥3 指不刷新锚点 ⇒ 下次单指拖动跳变；⑥ `:489` `cos_dec<1e-6⇒1e-6` 把奇点放大 1e6 倍再由 `:498-499` 折回 `[0,360)` ⇒ **把奇点变成看似合法的坐标**；⑦ `:419-420`/`:498-499` 对 NaN 不触发 ⇒ NaN 视角**永久化**，全片无 isfinite；⑧ `:84-88` pending 无次数上限无终态 | **需修** |
| 5 | `widgets/abstract_view.cpp` 287 | 全 | ① `:258-261` 空样本 → `[0,1]` 兜底**且该分支一行日志都没有**（else 分支 `:272` 有）；`:276 data_range_computed_=true` **永久锁定**；`:280` 把假标尺推进 `ud_grade` 的 uint8 归一化；② `:131-133`/`:78-80`/`:94-96` 三条静默 `return;`，`auto_stretch` 返回 `void`（`abstract_view.h:31`）⇒ 调用方无法区分「按了没反应」；③ `:145` `initializeOpenGLFunctions()` 的 `bool` 丢弃；④ `:179` `renderer_->render()` 返回值丢弃（`gl_renderer.h:57` 明写返回状态）；⑤ `:148-152` 致命失败只剩 `qCritical`（release 下默认过滤）+ `gl_initialized_` 永假 ⇒ 永久空白控件；⑥ `:40-49` 析构里 `makeCurrent()`（Qt 明令避免，待 Qt 版本定论） | **须修** |
| 6 | `tools/gen_geometry_truth.py` 260 | 全 | ① `:79-86 local_to_fits_perm` 是**独立 numpy 位解交织** ⇒ 与 C++ `nested_local_to_fits_index` 互为独立实现，**像素映射这一项是真有覆盖的**（公允认定）；② `:177 hips_pixel_scale=58.6` 硬编码且与 `:168 hips_order={leaf_order}` 矛盾（58.6 是 nside=1 尺度，本品 leaf_order=2 ⇒ nside=2048 ⇒ 真实 0.02863°，**差 2048 倍**）；③ `:179-180` 硬写 `moc_sky_fraction=1.0`/`acsd_covered_sky_fraction=1.0`，而 `:253 write_moc(root,2,o2)` 只写 4 张 order-2 tile（≈2.08% 天区）⇒ **truth 件自相矛盾**；④ `:142/:183/:200/:212` 无 temp+rename；⑤ `:221 mkdir(exist_ok=True)` 不清旧 ⇒ 重跑留残 tile；⑥ `:162 n_tiles` 形参从未使用；⑦ `:4` `V9 P9-3` 轮次号进生产注释 | **须修** |
| 7 | `tests/test_geometry_truth.cpp` 240 | 全 | ① `:226-227` **恒真门**（B-1）；② `:113 + :120 + :128` **筛掉真信号**：`:113` 跳过失败/非有限样本，`:120` 只在残差**已经** <0.03 时才计数，`:128` 阈值 `>=9/12` 允许 3 个 face 全错仍绿，而 `:103/:122` 算出的 `worst_face_err` **只在 `:234` 打印、永不断言**；③ `:116-118` `face_here` 与被检侧 `hips_sky_view.cpp:246` 共用同一 `acsd::healpix::ang2pix_nest` ⇒ **face 指派缺陷自洽抵消**（§5 CE-1）；④ `:114-115` 手抄 `gen_geometry_truth.py:57/61` 且用 `std::fmod` 而生成器用 `np.mod` ⇒ `ra+off<0` 时残差≈0.5 被 `:120` 静默丢弃；⑤ `:231-232 raster_nonempty` 只断言缓冲区**尺寸**，而 `:289` 保证任何渲染都填满 ⇒ 空白渲染亦 PASS；⑥ `:219` 把 cap 降到 8 强制驱逐，`:228` 只断言 `evictions>0`，**从不验证驱逐后仍取到正确 order 的像素** | **阻断** |
| 8 | `core/gl_renderer.h` 227 | 全 | ① `:6-7` 注释编译命令指向 `../../astro_image_io`（实测不存在，真实 `../../aio`）；② `:8` 指向 `docs/detail/healpix_browser_qt.md`（实测不存在）；③ `:24` 宣布 `SINGLE_FRAME 已废弃`，但 `:81/:90-91/:110/:204/:208` 共 5 处单帧机制**全部保留**且 `:208 render_single_frame` 全仓零调用者（跨片 grep）⇒ 违反 AGENTS §6「退役代码删除或保留统一注释块写明原因」；④ `:81/:84-91/:110` 这些 GL 句柄**无类内初始化器**（对比 `:117-133` 全有 `=0`），是否安全取决于 ctor（跨片）；⑤ `:67-73 get_hiss_bbox` 无条件赋全部四个出参 ⇒ **据此我判定 `sphere_view.cpp:121-122` 的未初始化局部变量是安全的**（§7 否决项） | **须修** |
| 9 | `CMakeLists.txt` 182 | 全 | ① `:16-18` 强制 Release ⇒ NDEBUG（**B-3 根因**）；② `:181` 恒红门（**B-3**）；③ `:45`/`:111` include 目录 `…/healpix_browser_qt/infrastructure/scheduler` **实测不存在**（本人 `ls` 复核）；④ `:6` 文档路径悬空；⑤ `:53 gdi32` 无 `if(WIN32)` 守卫 ⇒ 非 Windows 无法 configure；⑥ `:126 BUILD_TESTS` 默认 OFF ⇒ 裸 `cmake --build` 一个测试都不编 | **阻断** |
| 10 | `core/stf_engine.cpp` 180 | 全 | ① `:167-171` 显式丢弃 `data_min/data_max`、直接透传原始像素值 ⇒ 与 `test_stf_engine.cpp:70-71` 的断言**直接冲突**（**B-3**）；② `:56` 「与 Siril 显示传递函数对齐」**无版本/文件/行号出处**，`:64-85` 的四组 `(midtones,compression)` 数值无一手证据；③ `:104-120` 两种失败都返回 `STFParams()` 默认值且**返回值不可与合法线性拉伸区分**；④ `:115` 只滤 `> no_data_value`，**无 isfinite** ⇒ 单个 `+Inf` 使 `p_hi=+Inf` ⇒ 全图黑；⑤ `:143-147` 只修局部 `range`，`p.shadows==p.highlights` 仍原样送出，`validate()` 在渲染路径上**无人调用**；⑥ `:95` 负的极小分母被替换成正的 ⇒ 符号翻转（对 `x∈[0,1],m∈(0,1)` 可证不可达） | **须修** |
| 11 | `core/hips_sky_view.h` 158 | 全 | ① `:72-73` **public static** 可变进程级 map（CE-7 的载体）；② `:98 valid_pixels` 是唯一能暴露 B-2 的字段，**全仓零消费者**；③ `:60-63 display_min/display_max` 亦无消费者；④ `:40 refresh_auto_range()` 是 B-2 之后**唯一**恢复入口；⑤ `:131 sample_value` 声明，跨片称无调用者 | **须修** |
| 12 | `app/main_window.h` 147 | 全 | ① `:142 current_tile_signal_f64_` 成员存在且有初值，但 `close_file()` 不释放它（**泄漏实证链闭合**）；② `:51` 第三参命名 `zoom`，实参是 FOV（`:1085` 注释自承「语义变更」）；③ `:29/:78/:88/:98/:129` `WP-H 步骤14` 轮次号进生产头文件 | **须修** |
| 13 | `tools/gen_ref_source.py` 134 | 全 | ① `:46-55 pixel_to_sky` 是**线性**映射，`:29-43` 写出的头却是 `CTYPE1='RA---TAN'`（真逆含 `cos(dec0)`）⇒ **生成器自身数据场与自身 WCS 不一致**（`1/cos20°=1.064`，6.4% RA 剪切）；② `:121` 的自检**只探中心像素**——恰是线性映射与 TAN 映射唯一重合的点 ⇒ **这是本片最纯粹的一例「同一定义式既当被检量又当期望量」**（§5 CE-13）；③ `:8-9` 文档称编码 `ra*1e6*1e6+(dec+90)*1e6`「可恢复坐标」，实现是 `:108-110 A*ra+B*dec` ⇒ **一个标量无法反解二维坐标**，文档与实现不符；④ `:113 bitpix` 形参从未使用；⑤ `:117` 无 temp+rename；⑥ `:121` 用 `assert`，`python -O` 下整段消失；⑦ `:126` 不校验 argv，多余参数静默忽略（对比 `gen_geometry_truth.py:217` 有校验） | **须修** |
| 14 | `core/healpix_math.cpp` 110 | 全 | ① `:70-71/:73` 三种非法输入**静默返回空结果、一行日志都没有**，而 `result.nside = target_nside` 在 `:68` **先于校验**写入 ⇒ 调用方拿到「声称 nside 正确、像素为空」的对象；② `:74-78` 只校验**商**的 2 的幂，从不校验整除（`ud_grade(12,…,5)` → ratio=2 通过）；③ `:86 std::min` 静默截断长度不匹配的输入；④ `:41-57 query_disc` 暴力遍历 `12·nside²`，与 `healpix_math.h:9`「任意 nside（最大 8192）」矛盾（8192 ⇒ 8.05 亿次迭代）；⑤ `:90` 无 isfinite，单个 NaN 污染整个粗像元；⑥ `:97-98` 取**均值**（与跨片 `browser_backend.cpp:629` 的**求和**语义相反——跨片线索，不在本片定论） | **须修** |
| 15 | `core/stf_engine.h` 104 | 全 | ① `:14-15` 文档写 `shadows [0,1)` / `highlights (0,1]`，实现 `stf_engine.cpp:61-62` 存的是**原始像素值**；`:88` 声称 uniform「归一化到 [0,1]」，`:161-163` 明确否认 ⇒ **三层单位契约互斥**（UI 结构 / 实现 / 文档）；② `:84` 声称 `O(n) 用 nth_element`，实现 `:127` 是 `std::sort`（O(n log n)）；③ `:63` 「对齐 Siril」无出处 | **须修** |
| 16 | `deploy.ps1` 98 | 全 | ① `:74` 硬编码 **`f:\Astro dev\Astro CS Normalization Database\lib\astro_image_io\astro_image_io.dll`** —— 仓库名都不同（本仓为 `Astro CS Database`）；② 缺失时只 `Write-Host … Yellow` **继续**，末尾仍无条件打印 `部署完成`；③ `:82-94` 第 4 步「验证」只断言「3 秒后进程还活着」，**不加载任何产品** ⇒ 部署脚本无法证伪它声称要防的故障；④ `:84` 把 `PATH` 砍到最小再启进程，若 exe 因缺 DLL 秒退则退出码非 0 才被抓——但见 ② | **须修** |
| 17 | `tests/test_browser_dual_dtype.cpp` 94 | 全 | ① **本片唯一不受 NDEBUG 影响的测试**（`:13-17` 自带 `g_fail` 计数器 + `:93` 真实退出码）——公允认定；② 但 `:52/:57/:61/:76/:82/:86/:89` 只覆盖 `read_tile_signal(_f64)`/`query_pixel(_f64)`，**从不覆盖 `load_leaf`**（渲染器实际走的那条路）⇒ 「禁止静默转换」的证明不覆盖渲染路径；③ `:29-33 pick()` 静默改用**另一个** fixture，`:35-38` 的 `QUERY_RA/DEC` 是为 tiny fixture 推导的 ⇒ 对 fallback 而言是任意天区；④ `:50/:75` `tile_ipix_list.empty()?0:[0]`，前面的 CHECK 已计失败但仍继续读 tile 0 | **须修** |
| 18 | `widgets/abstract_view.h` 91 | 全 | ① `:4` 指向 `widgets/archive/`（跨片实测已删）；② `:7` 文档路径悬空；③ `:44` 第三参命名 `zoom`，实参是 FOV；④ `:78-79 data_min_/data_max_` 无类内初值（靠 ctor 初始化，脆但当前安全） | **建议** |
| 19 | `tests/test_stf_engine.cpp` 82 | 全 | ① 全部 17 处裸 `assert` ⇒ **NDEBUG 下全灭**，`main :80-81` 无条件 `ALL PASS`（**B-3**）；② `:70-71` 断言被 `stf_engine.cpp:167-171` 明文删除的旧契约（**B-3**）；③ `:4` 「编译：`make eng/tests/test_stf_engine.exe`」——该路径**实测不存在**，Makefile 目标是 `tests/test_stf_engine.exe`（悬空引用）；④ `:54-56` 三条断言经我手算**成立**（数据 40.0..59.8，p_lo≈40.1、p_hi≈59.7、midtones≈0.505），故只有 `:70-71` 是坏的——精确定位，不夸大 | **阻断** |
| 20 | `app/stf_panel.cpp` 56 | 全 | ① `:31 auto_button_ = new QPushButton(...)` 是**局部变量遮蔽成员**（成员在 `:16` 被初始化为 `nullptr` 且**此后永不赋值**）⇒ 任何未来读者取该成员即空指针；② `:5` 文档路径悬空；③ `:53-54` 把范围推给 `STFBar::set_range`（跨片称面板从不读 `dmin_/dmax_`，属线索） | **须修** |
| 21 | `Makefile` 54 | 全 | ① `:8 AIO_DIR = ../../astro_image_io` **实测不存在**（真实 `../../aio`）⇒ `make` 整链不可用；② `:11 CORE_SRCS` **漏掉** `hips_browser_backend.cpp`、`hips_sky_view.cpp`、`healpix_core.cpp` ⇒ 产出的 `libhealpix_browser_core.a` 与 `CMakeLists.txt:35-43` 是**两份互相矛盾的定义**，且缺 `acsd::healpix::*` 符号 ⇒ 链接必失败；③ `:15 TESTS` 漏掉两个 HiPS 测试、却包含 CMake 完全没注册的 `test_browser_dual_dtype`；④ `:51-54` 在 GNU make recipe 里用 `del /Q` | **须修** |
| 22 | `core/healpix_math.h` 48 | 全 | ① `:36-37` 契约「必须为 src_nside 的整数次幂分之一 / 4^k 像素**求均值**」——实现 `:70-78` 既不校验整除也不校验可除性；② `:9` 「任意 nside（最大 8192）」与 `healpix_math.cpp:47` 的 `12·nside²` 暴力实现矛盾；③ `:16-17 pix2ang_nest` 返回 `void`，越界 ipix 静默给 `(0,0)`（跨片线索） | **建议** |
| 23 | `run_healpix.bat` 5 | 全 | 整份硬编码**另一个仓库**的绝对路径（`f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\…`，且 `lib/healpix_db/` 已被 `drizzle/healpix_drizzle` 取代）⇒ **不可能运行** | **建议** |

---

## 4. 发现清单

### 4-A 阻断（BLOCKER）

| ID | 位置 | 缺陷 | 反例 / 后果 |
|---|---|---|---|
| **B-1** | `tests/test_geometry_truth.cpp:226-227` + `core/hips_sky_view.cpp:158` | `cache_bounded` 恒真门；真性质（cache 有界）零覆盖 | 任何 `target_order()` 返回值都 ≥0；把缓存上限改成 100000 该检查仍绿 |
| **B-2** | `core/hips_sky_view.cpp:376,399-400,410,429,444-452,468,507-516` | 零样本时把 `±FLT_MAX` 写入持久态 `lo_/hi_`，谎称已计算，全屏纯黑且不可重试 | 全部 tile 读取失败 / 全部 support=0 ⇒ 全黑 + 无日志 + `--stf-bench` 把 `range_lo=3.4e38` 当测量写进 JSON 并 exit 0 |
| **B-3** | `CMakeLists.txt:16-18` + `:181` + `tests/test_stf_engine.cpp:70-71` + `core/stf_engine.cpp:167-171` | 验收层三重失效：NDEBUG 消除全部 assert；唯一非空洞门恒红；且该测试断言的是代码已删除的契约 | CTest 全绿而什么都没验；一旦开 assert，`test_stf_engine` 第一条即 abort |

### 4-B 须修（MUST-FIX）

| ID | 位置 | 缺陷 |
|---|---|---|
| M-1 | `tests/test_geometry_truth.cpp:113,120,122,128,234` | 筛掉真信号 + 阈值放宽到 9/12，且真正测到的 `worst_face_err` 只打印不断言。**本人已把容差量化为两个具体反例（见 §5 CE-14）**：face 步长 `0.012`（`gen_geometry_truth.py:61`）vs 容差 `0.03`（`:120`）⇒ **容许 2 个 face 的错指**；且全渲染器 **+0.02 的恒定系统偏差**同样落入容差 ⇒ 全局标定错误不可见 |
| M-2 | `tests/test_geometry_truth.cpp:116-118` × `core/hips_sky_view.cpp:246` | 期望值与被检量共用 `acsd::healpix::ang2pix_nest` ⇒ **base-face 指派缺陷自洽抵消**（§5 CE-1） |
| M-3 | `tests/test_geometry_truth.cpp:231-232` × `core/hips_sky_view.cpp:289` | 只断言缓冲区尺寸 ⇒ 全黑/全背景渲染亦 PASS |
| M-4 | `app/browser_cli.cpp:994,204,281` | `--queries abc`（`atoi`=0）⇒ 循环不执行仍报 `RESULT: PASS`，**恒真门** |
| M-5 | `app/browser_cli.cpp:327-340,363` | 空 `samples` 时 `size()-1` 下溢 ⇒ 三处越界读（UB），且无条件 `return 0` |
| M-6 | `core/hips_sky_view.cpp:83-84,386-392,413` × `:88-93` × `:267-270` | `g_global_scan_cache_` 只按路径键、永不失效 ⇒ **自愈锚**；缓存命中还被记成 `valid=1` 掩盖真实样本量 |
| M-7 | `core/hips_sky_view.cpp:28-30,99-100` | `clampf` 放行 NaN，`set_view` 无 isfinite ⇒ NaN 贯穿到 `:516` 的 `(uint32_t)NaN`（UB） |
| M-8 | `app/main_window.cpp:694,733-734,738` | `fov*100+layer` 编码不可逆 ⇒ Support View 预设恒为 layer 0 |
| M-9 | `app/main_window.cpp:884-885` × `:320-323` × `main_window.h:142` | `close_file()` 漏放 FP64 tile 缓冲 ⇒ 每次开关文件泄漏 |
| M-10 | `app/main_window.cpp:420,459-462` | 全 NaN tile ⇒ `range` 退化 ⇒ `(uint)NaN` 逐像素 UB |
| M-11 | `widgets/abstract_view.cpp:258-261,276,280` | 空样本 `[0,1]` 兜底**无日志 + 永久锁定 + 污染后端 uint8 归一化** |
| M-12 | `widgets/abstract_view.cpp:145,148-152,179` | 两个 GL 能力探测的返回值全丢弃；致命失败只剩 release 下被过滤的 `qCritical` ⇒ 永久空白控件、渲染失败不可观测 |
| M-13 | `core/stf_engine.cpp:56,64-85` × `stf_engine.h:63-70` | 「与 Siril 对齐」无任何一手出处；四组数值无证据（**UNCONFIRMED：值本身是否真与 Siril 一致，需查 Siril `stf.c`**） |
| M-14 | `core/stf_engine.h:14-15,88` × `stf_engine.cpp:61-62,161-163` | shadows/highlights 单位契约三层互斥 |
| M-15 | `tools/gen_ref_source.py:46-55,121` × `:29-43` | 数据场用线性映射而自写 TAN 头；自检只探中心（两映射唯一重合点）⇒ **判据无法发现自身不一致**（§5 CE-13） |
| M-16 | `tools/gen_geometry_truth.py:177,179-180` × `:168,253` | truth 件元数据自相矛盾（像素尺度差 2048 倍；MOC 覆盖 2.08% 却写 1.0） |
| M-17 | `CMakeLists.txt:45,111`；`Makefile:8,11`；`deploy.ps1:74`；`run_healpix.bat:5` | 悬空路径：include 目录不存在、AIO 目录不存在、Makefile 与 CMake 两份矛盾定义、两个脚本写死**另一个仓库** |
| M-18 | `core/healpix_math.cpp:68,70-78,86` | `result.nside` 先于校验写入；三条非法输入静默返回空；只校验商的幂次不校验整除 |
| M-19 | `app/stf_panel.cpp:16,31` | 局部变量遮蔽成员 `auto_button_`，成员永为 `nullptr` |
| M-20 | `app/browser_cli.cpp:220-226` | 参考基准的四个读取返回值全丢弃（方向：通常造成假 FAIL；与 B-2 叠加时可造成假 PASS） |
| M-21 | `app/browser_cli.cpp:1029-1031,1147,1317-1318` | 未知 `--flag` 静默吞掉；非 HiPS 路径恒 exit 0；`--frames 0` 产出 `inf`/`nan` 的非法 JSON |
| M-22 | `core/gl_renderer.h:24,81,90-91,110,204,208` | 宣布退役的 SINGLE_FRAME 机制整套保留，且 `render_single_frame` 零调用者（AGENTS §6） |
| M-23 | `tests/test_stf_engine.cpp:20,21,23` × `core/stf_engine.cpp:93` | `test_mtf` 的 4 条断言中 3 条传 `m=0.5`，**全部走 `:93` 的恒等早返回分支**，只有 `:22`（m=0.25）触及通用公式。⇒ 整个二维 midtone/对比度曲面只在**一个对角点**取样。把 `:96` 改成「m≠0.5 时恒返回 0.5」⇒ 四条断言全绿，而 `get_preset` 的全部非线性预设（midtones=0.25/0.25/0.15）渲染成平坦 0.5。（已复核） |
| M-24 | `tests/test_geometry_truth.cpp:204-212` × `gen_geometry_truth.py:40` | `zero_coverage_region` 探测点 (180.5, 0) 位于 `ZERO_REGION=(178,182,-1.5,1.5)` 的**正中心**；order 0（nside=512）像元间距 ≈0.1145° ⇒ NaN 盒约 35×26 像元。⇒ 索引错误在 ±13 行 / ±17 列以内**仍落在盒内**并通过。该检查声称验证布局约定（`:202` 注释），但夹具太粗，无法作证。（已复核） |
| M-25 | `tests/test_geometry_truth.cpp:186-200` | `seam_continuous` **从不验证两个探针落在不同 tile**（本人 grep 该区块：无任何 `tile_ipix` / `>>18` 断言）⇒ 只在 tile 边界不连续的缺陷对它不可见。且 `:196` 是「首个通过即置位、永不清除」的 any-筛选，`:195` 的最大误差 `seam_err` 只在 `:234` 打印、**从不被门控**。一个专名为几何接缝性质的判据，**没有任何机制证明它测过接缝**。（结构层已复核；见 §7 对其量化概率主张的否决） |

### 4-C 建议（ADVISORY，跨片线索，本片不下结论）

- `core/hips_browser_backend.cpp:112-177` `open_product` 对不存在目录返回 0（三个子代理独立复现，**属他片**）；`:74,80` `f.read` 返回值丢弃 ⇒ 截断 tile 被当完整。
- `core/browser_backend.cpp:508-516` FP64 `.hiss` 走 FP32-only API 渲染为空（他片）。
- `app/main.cpp:143` `return app.exec()` 实质恒 0；`:102` 截图靠 1200 ms 硬编码猜时序（他片）。
- `core/gl_renderer.cpp:155,162,210` 文件级 `static bool gl_functions_loaded_` 跨 GL 上下文存活（他片）。
- `widgets/sphere_view.h` 的 `MIN_FOV/MAX_FOV` 实值、`:27` 的「FOV=60°」文本 —— **该文件不属本片，我只引用了 `sphere_view.cpp:36,60-64,101,103` 的可证部分**。

### 4-D 明确查过且确认 CLEAN（本片）

1. **私建线程池：全片零命中。** 独立 `grep -E 'std::thread|std::async|QThread|QThreadPool|QRunnable|QtConcurrent|thread_pool|std::jthread|concurrency::'` 于全目录 → 0 命中。唯一并行是 OpenMP（`hips_sky_view.cpp:315-320,469`），属运行时 team，非私建池。
2. **`hips_sky_view.cpp:322` 的行分块无竞态。** 已证 `jobs*(h_/jobs+1) >= h_` 对任意 `jobs` 成立 ⇒ 每个 `j` 必被恰好一个线程覆盖，`leaves[]` 全写。
3. **`hips_sky_view.cpp:200` 的缓存有界性成立。** `if (cache_.size() >= cache_cap_) evict_one();` 在插入前 ⇒ 稳态 ≤ `cache_cap_`。（**注意：这不等于 B-1 那个检查在验证它** —— 代码对、判据假。）
4. **零 `try`/`catch`/`throw`** 于本片全部 C++ 文件 ⇒ 「catch 后继续」一类缺陷在本片无面。
5. **`gen_geometry_truth.py:79-86` 的 `perm` 是真双射** ⇒ `:156-157` 的 scatter 无未填 NaN。且它与 C++ `nested_local_to_fits_index` 是**两套独立实现** ⇒ 像素映射这一项有真覆盖（对 B-2/M-2 而言是唯一的正面认定）。
6. **`test_geometry_truth.cpp:82-86` 的 0/360 wrap 用例正确**：`-0.1 ≡ 359.9 (mod 360)` 是同一天区位置，`1e-6` 容差合理。（初读时我误判为相差 0.2° 的两点，自行推翻。）
7. **`gen_geometry_truth.py:117-125 order1_region_tiles` 的 `elif` 可达**：`t=0..47` 映射到 ipix `0..12,320,256`，在 nside=1024 下 12 个 face 各含 4 张 order-1 tile，故 `:124` 的 RA 判据真实生效。
8. **`test_browser_dual_dtype.cpp` 是本片唯一 NDEBUG 免疫的测试**（`:13-17`+`:93`）。
9. **`browser_cli.cpp:1146-1186` 的 `get_data_bbox` 失败分支**：`open_file` 成功后该分支是否可达依赖跨片 `browser_backend.cpp`，我**未能独立确认**，故把该项从「须修」降为「潜在」（见 §7）。

---

## 5. 我主动构造的反例

> 每条都标明：构造什么 / 期望推翻什么 / 是否推翻。

### CE-1 ★ 推翻 `multi_face_labels` 的判别力 —— **推翻成功**
- **构造**：令 `acsd::healpix::ang2pix_nest` 的 base-face 指派出错（face f ↔ astropy face g，f≠g 的一个固定置换）。
- **推导**：生成器 `gen_geometry_truth.py:59-61` 用**独立的 astropy** 给每个天区打上 `0.012·astropy_face` 标签。
  被检侧 `hips_sky_view.cpp:246-248` 用 C++ `ang2pix_nest` 得 `tile_ipix = leaf>>18`（错误 face g 的 tile，读到标签 `0.012·g`）；
  期望侧 `test_geometry_truth.cpp:116-118` 用**同一个** `ang2pix_nest` 得 `face_here = g`；
  于是 `:119 res = v - base - 0.012·face_here = (base+0.012·g) - base - 0.012·g = 0` ⇒ `< 0.03` ⇒ `:121` 计数 ⇒ **PASS**。
- **期望推翻**：「multi_face 标签能检出 base-face 指派错误」。
- **是否推翻**：**是，推翻成功**。NESTED 下 base-face 置换会把整片天区完全错位，而本判据对其**结构性免疫**。

### CE-2 ★ `cache_bounded` 能否失败 —— **推翻成功（永真）**
- **构造**：审 `target_order()` 的值域。`hips_sky_view.cpp:153 if(!bk_) return 0;`；`:157 order=(int)lround(o)`；`:158 order=std::max(0,std::min(order,leaf))` ⇒ 值域恒 `[0, leaf]`。
- **期望推翻**：「`sky.target_order() >= 0` 能在缓存无界时变红」。
- **是否推翻**：**是**。它与 `cache_` 毫无关系；`:219 set_cache_cap(8)` + `:228 evictions>0` 只证明「能驱逐」，不证明「驱逐后仍取到正确 order 的像素」（`M-3` 补不上这个洞）。

### CE-3 ★ `--queries abc` 是否产生空转 PASS —— **推翻成功**
- **链**：`:994 atoi("abc")==0` → `:204` 主循环零次 → `mismatch==0` → `:249` 的 64 次 outside-MOC 循环**独立于 `n_queries`** 照跑，`outside_ok≥1` 可达 → `:280 snr_ok` 真 → `:281 pass=true` → `:283 "RESULT: PASS"` → `:284 return 0`。
- **期望推翻**：「`hips_pass` 意味着验过 1024 个点」。
- **是否推翻**：**是**。一个 CI 脚本里的笔误把硬门静默变成 no-op。

### CE-4 ★ `test_geometry_truth` 的 CTest 注册 —— **推翻成功（恒红）**
- **链**：`CMakeLists.txt:181 add_test(NAME geometry_truth COMMAND test_geometry_truth)` 不带参数 ⇒ `argc==1` ⇒ `test_geometry_truth.cpp:44-46` `return 2`。
- **期望推翻**：「该测试在 CI 中实际执行」。
- **是否推翻**：**是**。本片唯一真正能失败的门**从不执行**；团队会把它当「已知红」长期忽略——恒红门比恒真门更隐蔽，因为它把真红灯藏进了「反正一直红」的噪声里。

### CE-5 ★ `test_stf_engine` 在默认构建下的真值 —— **推翻成功（双重）**
- **链**：`CMakeLists.txt:16-18` ⇒ Release ⇒ `-DNDEBUG` ⇒ `:70 assert` 变 `((void)0)` ⇒ `:80-81` 打印 `ALL PASS` 并 `return 0` ⇒ **CTest 绿、零断言**。
  反向：若去掉 Release ⇒ `stf_engine.cpp:170 u.shadows=params.shadows=10.0` ⇒ `approx(10.0,0.1)` 为假 ⇒ **第一条 assert 即 abort**。
- **期望推翻**：「这个测试当前是绿的，所以 uniform 归一化是对的」。
- **是否推翻**：**是**。绿是 NDEBUG 的意外，不是通过；且它断言的契约被 `stf_engine.cpp:161-163` 明文删除。**这正是「看到『检查通过』的机制，不要据此认为实现正确」的标准样本。**

### CE-6 ★ 零样本自动标尺 —— **推翻成功（端到端）**
见 §2 第 2 条完整推导。补充两条**下游可观测性被摧毁**的证据（均由我本人读出）：
- `main_window.cpp:1069-1071` 用 `get_preset(preset, range_lo(), range_hi())` 拿到 `shadows=+3.4e38, highlights=-3.4e38`（`validate()` 会失败，但无人调用），随后 `:1073-1074` **把 `p.shadows/p.highlights` 覆写成 0/1** ⇒ **STF 面板显示健康的 [0,1]，渲染器却在用倒置标尺** ⇒ 中毒态在 UI 上完全不可见。
- `browser_cli.cpp:428-429` 把 `range_lo=3.4e38, range_hi=-3.4e38` 写进 JSON 证据，`:436 return 0`。
- 且 `:450 stats_.valid_pixels=0` 是唯一信号 —— 本人独立 grep 确认**全仓零消费者**。

### CE-7 ★ `g_global_scan_cache_` 的自愈性 —— **推翻成功**
- **构造**：同一进程内，run 1 扫描成功 ⇒ `:413` 写入 `{dmin,dmax}`；随后产品文件被重生成/截断/以不同 `hips_order` 重新发布；run 2（GUI 重开同 root，或 `browser_cli` 连续两次探针）。
- **链**：`:387` 查表命中 ⇒ `:388-392` 直接取用，**不重扫** ⇒ `:391 valid=1`（把「复用了标尺」记成「1 个有效像素」，掩盖真实样本量）。键只有 `:386 root` 字符串，**不含 nside/leaf_order/dtype/mtime**。`set_backend :88-93` 清 `cache_/metrics_/stats_` 但不清它；`reset_metrics :267-270` 也不清。
- **期望推翻**：「重新打开产品会重算标尺」。
- **是否推翻**：**是**。缓存把一次瞬时读失败升级成**永久性错误显示标尺**，并幸存于 `set_backend` 与 `reset_metrics` —— 正是本轮点名的「读的是会被本次执行自己影响/污染的锚」类。

### CE-8 ★ `--dataset-stats` 在产品不可读时 —— **推翻成功**
- **链**：`:317 read_tile_at_order!=0 → continue`（静默，无计数门）⇒ `samples` 空 ⇒ `:329 idx = q*(size()-1)`，`size()-1` 为 `size_t` 下溢 = 2^64−1 ⇒ `:330 lo` 天文数字 ⇒ `:332 samples[lo]` **越界读**；`:335 samples[0]` 空容器读；`:340 dev[0]` 空容器读 ⇒ 三处 UB；`:363 return 0` **无条件**。
- **公允对照**：同文件 `run_hips_mode :190-194` **确实**做了 `if (tiles.empty()) … return 1` ⇒ 证明这是**疏漏而非策略**。
- **是否推翻**：**是**。证据工具在什么都没读到的情况下 exit 0 并发布垃圾分位数。

### CE-9 `browser_cli.cpp:1178` 未初始化 —— **未推翻（主动降级）**
四个 double 无初始化、`:1223/:1266` 无条件读，这是真的。但「`get_data_bbox` 失败分支是否可达」取决于**非本片**的 `browser_backend.cpp:791`。我未能独立确认其守卫，故**不**把它列为须修，标为潜在。**这是本轮我刻意不夸大的地方。**

### CE-10 ★ `stf_panel.cpp:31` 成员遮蔽 —— **推翻成功**
`:16` 初始化列表 `auto_button_(nullptr)`；`:31` `auto_button_ = new QPushButton(...)` 无 `this->` ⇒ 声明**局部**同名变量，成员**永不赋值**。期望推翻「成员指向按钮」——**推翻成功，成员恒为 `nullptr`**。

### CE-11 ★ 预设编码不可逆 —— **推翻成功**
编码 `:694 p.fov*100.0 + p.layer`；解码 `:733 fov=code/100.0`、`:734 layer=round(code-fov*100.0)`。取 `:686` 的 Support View（fov=15.0, layer=1）：code=1501.0 → fov=15.01 → `code-fov*100 = 1501.0−1501.0 = 0` → **layer=0** ⇒ `:738 setChecked(0==1)` 为假。**期望推翻「Support View 预设能切到 support 层」——推翻成功。**（其余预设编码时本就无 layer，解出 0 属巧合正确。）

### CE-12 FP64 tile 泄漏 —— **推翻成功**
三文件闭合：`main_window.h:142` 声明并初始化 `current_tile_signal_f64_`；`main_window.cpp:320-323` 选中 tile 时**有**释放；`main_window.cpp:884-885 close_file()`（`:83-85 ~MainWindow` 调用）**只放 `current_tile_signal_` 和 `current_tile_support_`**。⇒ 每次「开 FP64 .hiss → 选 tile → 关闭」泄漏一次。

### CE-13 ★★ `gen_ref_source.py` 的自检与被检同源 —— **推翻成功（本片最纯粹的一例）**
- **构造**：`:29-43` 写出的头声明 `CTYPE1='RA---TAN'`；`:46-55 pixel_to_sky` 却用**纯线性** `ra = ra0 + (-x0·PIXEL)`。TAN 的真逆含 `cos(dec0)` 因子，两者相差 `1/cos(20°)=1.064`。
- **期望推翻**：`:121` 的 `assert abs(ra[0]-RA0)<1e-9 and abs(dec[0]-DEC0)<1e-9` 能发现这一不一致。
- **推导**：`:120` 只探**中心** `(W-1)/2, (H-1)/2`。在中心，`x0=0, y0=0` ⇒ 线性映射与 TAN 映射**都**给出 `(RA0, DEC0)` ⇒ assert 恒真。
- **是否推翻**：**是，推翻成功**。判据只在被检量与期望量**恰好重合**的那一点取样 —— 这就是本轮要找的「同一个定义式既当被检量又当期望量」的教科书形态。
- **实际危害**：`build_visual` 里 `:74` 意图画在 RA 60.18 的标记，真 TAN WCS 读回 60.18×… 偏移约 0.0116°；`:63-65` 的圆在天球上是 6.4% 拉伸的椭圆。一个自称「外部标准判决」的产物带着未修正的 6.4% RA 剪切。

### CE-14 ★ `multi_face_labels` 的容差被量化 —— **推翻成功（两个新反例）**
- **构造 A（两 face 错指）**：face 标签步长 `0.012`（`gen_geometry_truth.py:61` `v += 0.012 * face`），门限 `:120 if (fabs(res) < 0.03)`。若 tile 取自 `face+1` ⇒ res=0.012；`face+2` ⇒ res=0.024。**两者都 < 0.03 ⇒ 仍被计为「已覆盖」**。⇒ 该门在构造上就容许 **2 个 base face 的错指**。
- **构造 B（全局系统偏差）**：令 `sample_at` 对每个像素返回 `v + 0.02`。在正确采样点上 `v = base + 0.012·face`（`.37` 偏移已避开网格与 marker），故 `res = 0.02`，`|0.02| < 0.03` ⇒ 48 个探针全部计入、`face_ok = 12` ⇒ **PASS**。⇒ **整个渲染器 2% 的恒定标定偏差对该判据完全不可见**。
- **是否推翻**：**是，两者均推翻成功。**（数字由我本人从 `gen_geometry_truth.py:61` 与 `test_geometry_truth.cpp:120` 直接核算。）

### CE-15 ★ `gen_ref_source` 自检的「恒真」有一个**约定无关**的证明 —— **推翻成功（比 CE-13 更强）**
- **构造**：`:35-36` 写 `CRPIX1 = W/2.0 + 0.5 = 1024.5`、`CRPIX2` 同；`:120` 探 `all_pix2world([(W-1)/2], [(H-1)/2], 0)`，即 `(2048-1)/2 = 1023.5`。
- **约定无关的论证**：**1023.5 恰是该 WCS 自身的参考像元**（1-based `CRPIX1−1 = 1023.5`）。而**在 WCS 的参考点上，`all_pix2world` 恒返回 `(CRVAL1, CRVAL2)`，与 CD 矩阵无关**——把 CD 转置、把 `CD1_1` 符号翻转、把 CD 整体清零，`:121` 的 `assert` **照样通过**。
- **期望推翻**：`:118-121` 的「WCS 合法性自检」能发现 `wcs_header` 写错。
- **是否推翻**：**是，推翻成功，且结论不依赖 0-based/1-based 约定之争**。而 `:82-84` 的方向箭头与 `:63-65` 的圆正是靠 CD 的东西向约定判读的 ⇒ **自检恰好对最该验的东西失明**。

### CE-16 `sphere_view.cpp:311-313` 除零 —— **推翻成功（可触发路径未证）**
`dist` 无 0 守卫 ⇒ `scale = last_touch_dist_/0 = +Inf` ⇒ `std::clamp(Inf, MIN, MAX) = MAX` ⇒ FOV 跳到上限。**但**「两个不同触点报出相同坐标」需要特定触控硬件，属 **UNCONFIRMED**；守卫成本极低故仍列须修。

---

## 6. 盲复算

**做法**：对 §2 三条阻断与 §5 全部反例，先在**不看任何子代理报告**的前提下从源码独立推导并记录结论，再与子代理结论对照。

| 项 | 我的盲复算结论 | 与子代理对照 | 判定 |
|---|---|---|---|
| `cache_bounded` 恒真 | 恒真（`:158` 钳位） | 三个子代理独立得出同结论 | **一致** |
| 零样本 → `±FLT_MAX` | 成立，且端到端到黑屏 | 两个子代理独立得出；第三个补充 `valid_pixels` 无人读 | **一致**（我独立补出面板不可见这一环） |
| NDEBUG 消除 assert | 成立 | 三个子代理独立得出 | **一致** |
| `test_stf_engine:70-71` 与实现冲突 | 成立（`|10−0.1|=9.9>1e-5`） | 两个子代理独立得出 | **一致** |
| `geometry_truth` 恒红 | 成立 | 两个子代理独立得出 | **一致** |
| `nested_local_to_fits_index` 共用是否构成同源断言 | **构成同源，但盲区窄于直觉** —— 数据由 numpy 独立摆放，故**像素映射**一项有真覆盖；**真正被掩盖的只有 `ang2pix_nest` 的 base-face 指派** | 无子代理提出此精确界定 | **我更严**：我把结论收窄到可证的窄版本，而不是笼统说「全是同义反复」 |
| `--queries 0` 空转 PASS | 成立 | 无子代理提出（他们提了 `--queries abc` 同源问题） | **一致** |
| `browser_cli.cpp:220-226` 忽略返回值的**方向** | 浏览器侧被检查 ⇒ 通常**假 FAIL**；仅当两侧同时返回 0（叠加 B-2 截断 tile）才假 PASS | eadc562f 称「假 PASS 或假 FAIL」；545d2fba 判为 fail-CLOSED | **我判偏严于 545d2fba / 偏松于 eadc562f**：我保留双方向，但把常态方向定为假 FAIL |

**结论：判一致。** 无「偏松」项（B-1/B-2/B-3 均被我独立确认为真阻断，非过苛）；唯一被我主动放宽的是 CE-9（未初始化 double，降为潜在）。

---

## 7. 子代理派发记录

**派发 5 个**（全部只读、禁 `/tmp/acsd_g08/`、禁编译运行、禁改仓内文件、结论须带 `file:line`）：

| # | 主题 | 覆盖 | 产出量 |
|---|---|---|---|
| 1 | 静默降级 / fail-open | 全目录 44 文件 12053 行 | 5 BLOCKER + 31 MUST-FIX + 26 ADVISORY + CLEAN 清单 |
| 2 | 自洽式断言 / 恒真恒红门 | 测试 + 生成器 + 实现 | 恒真门、恒红门、共享定义清单 |
| 3 | 线程池 / 硬编码 / 悬空引用 / 退役活调用者 | 全目录 + 全仓 grep | 13 代码点 + 10 文档点悬空；退役件 5 处 |
| 4 | 错误码语义 + 数值稳定性 + 资源生命周期 | 全目录 + AIO 契约 | 退出码表、NaN/Inf/奇点/空输入清单 |
| 5 | 重复派发 #1（我不小心重复，**如实登记**） | 同 #1 | 与 #1 结论收敛，无新增矛盾 |

**逐条复核与否决**（我只对**本片 23 个文件**的结论负责；子代理跑全目录，绝大多数命中在他片）：

| 子代理结论 | 我的处置 | 依据 |
|---|---|---|
| f921597f「`sphere_view.cpp:121-122` 未初始化 `ra/dec/w/h` 可能读未初始化值」 | ✅ **否决（撤回为 CLEAN）** | 本片 `core/gl_renderer.h:67-73` 显示 `get_hiss_bbox` 是**内联**成员且**无条件赋全部四个出参**，源成员 `:123-126` 有类内初值 `= 0.0`。调用方局部未初始化但必被全写。子代理随后自行 Addendum#2 撤回，与我独立结论一致。 |
| eadc562f「`browser_cli.cpp:1178` 四个未初始化 double 是 MUST-FIX」 | ⬇️ **降级为潜在** | 可达性依赖非本片 `browser_backend.cpp`，我无法独立确认 ⇒ 不计入须修（见 CE-9）。 |
| 1fce6b05「A-1 OpenMP 运行在 Qt GUI 线程，超出授权线程模型」 | ⬇️ **降级为建议** | OpenMP 运行时 team **不是**本轮点名的「私建线程池」；我独立 grep 确认全片私建池构造 **0 命中**。该类在本片判 CLEAN。 |
| 1fce6b05 C-14 关于 `sphere_view.h:27/MAX_FOV=50` 的具体文本 | ⬇️ **仅采信我能自证的子集** | `sphere_view.h` **不属本片**。我只保留 `sphere_view.cpp:36`（ctor=60）、`:60-64`（两分支=50）、`:101/:103`（一处 clamp 一处绕过）这些我亲自读到的部分。 |
| 545d2fba B3「`test_browser_backend.cpp:72` 两参调用 vs 四参签名 ⇒ 编译不过」 | ❌ **不采信（不可验证）** | `tests/test_browser_backend.cpp` 与 `core/browser_backend.cpp` **均不属本片**，两侧我都未读 ⇒ 登记为「他片待核」，不在本报告定论。 |
| eadc562f M-28 vs 545d2fba M-28（`browser_cli.cpp:220-226` 方向之争） | ⚖️ **我裁定** | 见 §6：常态为**假 FAIL**（fail-closed 方向），仅在与 B-2 叠加时可假 PASS。保留为 M-20 但明确方向。 |
| 三个子代理共同：「`open_product` 对不存在目录返回 0」 | ⏭️ **登记为他片线索，不在本报告定论** | `core/hips_browser_backend.cpp` 不属本片。我只在本报告中引用其**对 B-2 的放大效应**（截断 tile 使两侧同时返回 0 → 假 PASS）。 |
| 三个子代理共同：「`stats_.valid_pixels` 全仓无人读」 | ✅ **采信并由我独立复核** | 我本人 `grep -rn "valid_pixels" lib/ eng/` → 仅写点 + 声明点，零消费者。这是 B-2 最有力的「信号被丢弃」证据。 |
| 三个子代理共同：「零 try/catch/throw」 | ✅ **采信并由我独立复核** | 本片无异常吞噬面。 |
| dff9abd3「`multi_face_labels` 三重抵消 + 两 face 错指 + 全局 2% 偏差不可见」 | ✅ **采信并由我独立复核** | 我本人核算 `gen_geometry_truth.py:61`（步长 0.012）与 `test_geometry_truth.cpp:120`（容差 0.03）⇒ 两 face 错指与 +0.02 恒偏差均落入容差。已升为 M-1 的量化反例（CE-14）。 |
| dff9abd3「`test_mtf` 只测 `m==0.5` 早返回分支」 | ✅ **采信并由我独立复核** | `:20/:21/:23` 传 `m=0.5`，`:93` `if (fabs(m-0.5f)<1e-10f) return x;` ⇒ 三条走恒等分支，仅 `:22` 触通用式。已列 M-23。 |
| dff9abd3「`zero_coverage_region` 对 ±13 行/±17 列内的索引错误不敏感」 | ✅ **采信并由我独立复核** | `gen_geometry_truth.py:40` `ZERO_REGION=(178,182,-1.5,1.5)`，探针 (180.5,0) 在正中；order 0 像元间距 ≈0.1145°。已列 M-24。**注意这不推翻我 §4-D 第 5 条**（`perm` 是真双射说的是生成端 scatter 正确，与探针分辨率无关，二者可并存）。 |
| dff9abd3「`gen_ref_source` WCS 自检是恒真断言」 | ✅ **采信并由我独立复核，且采其更约定无关的表述** | 我本人验 `CRPIX1=1024.5` 与探针 `1023.5` 相合 ⇒ 探针即参考像元，返回 CRVAL 与 CD 无关。已列 CE-15（比我的 CE-13 更强）。 |
| dff9abd3「`seam_continuous` 探针几乎从不跨越接缝，5 探针命中概率 ≈0.5%」 | ❌ **否决其量化部分，保留其结构部分** | 结构部分我已独立证实且已列 M-25（无跨 tile 断言 + any 筛选 + max 不断言）。但「≈0.5%」这一概率**推导前提不成立**：`:168-174` 是先在 48 个 order-1 tile 中**搜最近的一对**再用其中心取中点，中点**是被特意选在接缝附近的**，不是随机点。按随机分布估概率属于误用。我不采纳该数字。 |
| dff9abd3 A5「`pixel_to_sky` 与自身 WCS 差半个像元」 | ❌ **不采纳数值** | 该项依赖 0-based / 1-based 与 astropy `origin` 参数的约定解读，我无法在不运行的前提下裁断（子代理自己也标为 UNCONFIRMED U5）。我改用 CE-15 的**约定无关**论证覆盖同一目标。 |
| dff9abd3 MUST-FIX-4「`hips_tile_oracle.py:39-40` 抄用 `gen_ref_source.py:108-109` 的 A/B 常量」 | ⏭️ **登记为他片线索** | `tools/hips_tile_oracle.py` **不属本片 23 份**，我未读，不定论。但若属实，这是「生成器与判官共用常量表」的最强一例，且该文件被 `docs/science/DATA_SEMANTICS.md` 列为外部权威 ⇒ 建议转交该片重点核。 |
| 子代理共报约 5 BLOCKER / 31 MUST-FIX | 🔽 **本报告只收本片可证的 3 BLOCKER / 22 MUST-FIX**，其余全部降级为 §4-C 他片线索 | 口径纪律：结论必须来自我自己读完原文。 |

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false rev-parse HEAD     # 实测 1fa477a7…，与任务书声明的 850a9ede 不符（见下）

# (1) 成员清单与行数：23 份 / 6039 行
python3 - <<'EOF'
import re
s=open('run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml',encoding='utf-8').read().split('\n')
st=next(i for i,l in enumerate(s) if 'INF-hips_browser-002' in l)
en=next((j for j in range(st+1,len(s)) if s[j].startswith('  - 片号:')),len(s))
print('\n'.join(s[st:en]))
EOF

# (2) B-1 恒真门：target_order() 值域被钳位
sed -n '152,160p' lib/infrastructure/hips_browser/healpix_browser_qt/core/hips_sky_view.cpp
sed -n '226,227p' lib/infrastructure/hips_browser/healpix_browser_qt/tests/test_geometry_truth.cpp

# (3) B-2 零样本 → ±FLT_MAX 持久化
sed -n '376,377p;396,416p;429,452p;468p;507,517p' \
  lib/infrastructure/hips_browser/healpix_browser_qt/core/hips_sky_view.cpp
grep -rn "valid_pixels" lib/ eng/          # 仅写点 + 声明点，零消费者

# (4) B-3 NDEBUG + 恒红门 + 旧契约
sed -n '16,18p;126p;181p' lib/infrastructure/hips_browser/healpix_browser_qt/CMakeLists.txt
sed -n '44,47p'  lib/infrastructure/hips_browser/healpix_browser_qt/tests/test_geometry_truth.cpp
sed -n '70,71p;80,81p' lib/infrastructure/hips_browser/healpix_browser_qt/tests/test_stf_engine.cpp
sed -n '161,171p' lib/infrastructure/hips_browser/healpix_browser_qt/core/stf_engine.cpp

# (5) CE-1 同源断言：期望值与被检量共用 ang2pix_nest
sed -n '116,119p' lib/infrastructure/hips_browser/healpix_browser_qt/tests/test_geometry_truth.cpp
sed -n '246,253p' lib/infrastructure/hips_browser/healpix_browser_qt/core/hips_sky_view.cpp

# (6) CE-3 恒真门：--queries 0
sed -n '204,205p;249,255p;280,284p;994p' lib/infrastructure/hips_browser/healpix_browser_qt/app/browser_cli.cpp

# (7) CE-13 自检与被检同源（TAN 头 vs 线性映射，只探中心）
sed -n '29,55p;118,122p' lib/infrastructure/hips_browser/healpix_browser_qt/tools/gen_ref_source.py

# (8) CE-7 自愈锚 + 线程池 CLEAN
sed -n '83,84p;386,392p;413p' lib/infrastructure/hips_browser/healpix_browser_qt/core/hips_sky_view.cpp
sed -n '88,93p;267,270p' lib/infrastructure/hips_browser/healpix_browser_qt/core/hips_sky_view.cpp
grep -rnE 'std::thread|std::async|QThread|QThreadPool|QRunnable|QtConcurrent|thread_pool|std::jthread|concurrency::' \
  lib/infrastructure/hips_browser/healpix_browser_qt/ ; echo "exit=$? (1=零命中)"

# (9) 悬空引用逐条验证
for p in docs/detail/healpix_browser_qt.md docs/detail/infrastructure/23_hips_browser.md \
         lib/infrastructure/aio lib/infrastructure/hips_browser/astro_image_io \
         eng/tests/test_stf_engine.cpp lib/include/healpix_browser_core.h \
         lib/infrastructure/hips_browser/healpix_browser_qt/infrastructure/scheduler; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"; done

# (10) CE-11 预设编码不可逆 / CE-12 FP64 泄漏 / CE-10 成员遮蔽
sed -n '686,695p;732,738p' lib/infrastructure/hips_browser/healpix_browser_qt/app/main_window.cpp
sed -n '320,323p;884,885p' lib/infrastructure/hips_browser/healpix_browser_qt/app/main_window.cpp
sed -n '142p' lib/infrastructure/hips_browser/healpix_browser_qt/app/main_window.h
sed -n '16p;31,33p' lib/infrastructure/hips_browser/healpix_browser_qt/app/stf_panel.cpp

# (11) M-16 truth 件自相矛盾
sed -n '168p;177p;179,180p;253p' lib/infrastructure/hips_browser/healpix_browser_qt/tools/gen_geometry_truth.py

# (12) 确认未改动任何仓内文件（除本交付件）
git -c core.quotepath=false status --porcelain
```

---

## 9. 偏差与未决登记

**基线偏差**：任务书声明 HEAD = `850a9ede`，实测工作树 HEAD = `1fa477a7a05c315550df2ce2bafc3e64ed9bbd78`。清单头部另记 `# HEAD: f9650dd0…`。三个值互不相同。本报告所有行号与结论均基于**实测**的 `1fa477a7`；若负责人指定的 `850a9ede` 与之有差异，需重跑 §8(1) 复核成员清单。

**未决（UNRESOLVED，需上层裁决）**
1. **STF 预设是否真与 Siril 对齐**（M-13）。`stf_engine.cpp:56` 声称对齐但无出处；我未取得 Siril 源码，**不判定数值对错**，只判定「缺出处」。
2. **HiPS `.hiss` 多边形网格的降采样聚合语义**：本片 `healpix_math.cpp:97-98` 取**均值**，跨片 `browser_backend.cpp:629` 取**求和**。按 AGENTS §3/§4，应由 `docs/science/` 裁定而非由代码裁定 —— **本报告不裁决**。
3. **`--standard-hips` 的 signal-only 契约**是否成立（跨片线索，依赖非本片文件）。
4. **本片不含的测试面**：`tests/test_healpix_math.cpp`、`tests/test_hips_browser_backend.cpp`（均由 `CMakeLists.txt` 注册但不在本片 23 份内）—— 其门禁有效性未审。

**过程登记（如实）**
- 子代理 #5 与 #1 主题重复，系我派发时误操作，已如实登记；两者结论收敛，未引入矛盾。
- 全程未读 `/tmp/acsd_g08/`、未编译、未运行 ctest/pytest/任何二进制、未执行任何 git 写操作、未修改仓内任何文件（唯一写入为本交付件）。