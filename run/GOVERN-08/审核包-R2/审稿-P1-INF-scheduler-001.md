# G08-05 对抗审稿 P1 · INF-scheduler-001

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 片号 | `INF-scheduler-001` |
| 层 | `lib/infrastructure/scheduler` |
| 成员份数 | **1**（权威清单：`分片清单/片清单-权威版.yaml` → `片号: INF-scheduler-001` → `成员文件[1] = lib/infrastructure/scheduler/src/module_adapters.cpp`） |
| 成员总行数 | **16260**（清单 `实际行数` 字段；`wc -l` 实测同为 16260） |
| 实际读了多少行 | **16260**（`read` 工具自 `:1` 连续覆盖至 `:16260`「End of file - total 16260 lines」，无跳读、无抽样） |
| **覆盖率** | **1/1 份 = 100%，16260/16260 行 = 100%** |
| 未读完的 | **无** |

**口径说明**：「读了多少行」= 本审稿人用 `read` 工具逐行读取的行数，不含仅靠 `grep`/`sed` 定位的片段（grep/sed 只用于对已读内容做交叉取证与计数复核）。读完后另用 Python 脚本对全文做结构扫描与悬空引用核验，该扫描不计入「读过的行数」。

**基线核对（重要）**：任务书给定的基线 HEAD = `850a9ede`，实际仓内 HEAD = `f4a2cf21092ac89d8efcd92e5bceca444349892b`，领先 3 个提交（`bf25c085` 移除实验域运行结果归档 / `1fa477a7` 报告迁入单元文档目录 / `f4a2cf21` 清理科学正本对已删归档的依赖）。

```
$ git diff --stat 850a9ede HEAD -- lib/infrastructure/scheduler/src/module_adapters.cpp   # 空
$ git rev-parse 850a9ede:lib/.../module_adapters.cpp   # 67600e2dbc32939003764af04a02ca9d0f248b3c
$ git rev-parse HEAD:lib/.../module_adapters.cpp       # 67600e2dbc32939003764af04a02ca9d0f248b3c
$ git hash-object lib/.../module_adapters.cpp          # 67600e2dbc32939003764af04a02ca9d0f248b3c
$ git status --porcelain -- lib/.../module_adapters.cpp # 空（干净）
```

本片文件在三个点上**逐字节相同**，故基线漂移对本片结论无影响。但该漂移本身对本片**有实质影响**：最近 3 个提交删除的正是本片注释所引用的 `run/*/REPORT.md` 证据文件（见发现 F-6）。

---

## 2. 本片判定

### 判定：**阻断**

**判据**：本片的关键合规结论——「退化方差面不挂块是 fail-closed」「死码保留与权重口径退役由机器门禁守护」——**其给出的依据经实测为假**。按负责人本轮裁定「看到任何『检查通过』的机制，不要据此认为实现正确」，一条**论证为假的 fail-closed 规则**与**两份虚构的门禁凭据**，使本片的可审计性归零：后续任何以这些注释为依据的改动，都在用假事实做决策。

### 最重 3 条

**① 阻断（F-9）：fail-closed 决策所引的代码事实为假，且被科学正本显式反驳**
`module_adapters.cpp:8098-8103` 断言「`drizzle_engine.cpp:1919` 对 `varianceValue<=0` 是**整像素 continue**（不只是不累加方差）⇒ 挂全零平面会**清空 signal/support**」，并据此决定**不挂**退化的 variance 块。
**实测证伪**（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1705-1716`）：只有 `acc.sumVarNum` 被 `if (varianceValue > 0.0f)` 守卫；`acc.sumArea`（:1705）、`acc.sumNorm`（:1709）、`acc.nContrib++`（:1716）**全部无条件执行** ⇒ signal/support 不受影响。而被引的 `:1919` 实为 stripe/worker 确定性注释，**与 variance 无关**。正本 `docs/science/algorithms/DRIZZLE_GEOMETRY.md:214` 明文相反：「`V_j ≤ 0` **不构成掩膜授权**……置 0 不 continue」。
⇒ 「不挂退化方差面」这个**决策**可能仍然正确（保守无妨），但**它给出的理由是假的**，不能作为审计凭据；按 AGENTS §3 文档优先，该注释必须订正。

**② 阻断（F-7）：drizzle 节点把 WCS 头八键全部以 `0.0` 兜底写进 HiPS 产品，且从不调用天测可用性守卫**
`module_adapters.cpp:8009-8016`。`p1_wcs_astrometry_usable` 全文件**只有 2 个真实调用点**（`:3507` star-psf 引导先验、`:5868` photometry 节点，失败即 `PHOT_WCS_UNUSABLE` fail-closed）；`p1_op_drizzle` 的唯一 WCS 门只是**对象存在性**（`:7927`）。一份存在但缺 `cd11`/`crval` 的 `p1_wcs.json`（或缺键的 `config["wcs"]`）会让 `CD1_1=0`、`CRVAL=0` 被写进标准 HiPS tile 头而节点报成功。**同文件内的选择性失效** —— 守卫 `:4455-4463` 的头注逐字记录了它专治的病（"CRVAL=(0,0)、CD=0（det=0）… 静默退化为占位 1.0"），photometry 接了，drizzle 没接。

**③ 阻断（F-10）：两份"机器门禁"凭据全部不存在，而它们是两处退役决策的唯一依据**
`module_adapters.cpp:12211` 称「本注释同时是 `eng/ci/check_no_weight_mode_code.py` 的 CHAIN_ANCHORS 登记项……**删改即门禁判红**」；`:3812` 称「机器断言见 `eng/ci/check_registry_ir_parity.py` C4–C8」；另有 6 处（`:3321` `:4313` `:4366` `:16145` 等）以 `ENGINEERING_SPEC.md §2「保留则注释」` 作为死码保留的权威依据。
**实测**：`eng/ci/` **整目录不存在**；`docs/engineering/ENGINEERING_SPEC.md` **不存在**（全仓仅存于未追踪的 `run/` 残留）。
⇒ 权重口径退役拒绝面（`weight_mode` 出现即拒，`:11948`/`:14046`）与精确 PSF 死码保留（`kPrecisePsfEnabled`）的**全部正当性论证，都挂在两个不存在的权威上**。仓内审计 `实验/engineering-evidence/audit-2026-01/ROOT_CAUSES.md:192` 已把同族问题标为 V8-N-01「凭据虚构」，但代码注释从未订正。

### 次重（本轮确证的非阻断项）

> §2 的「最重 3 条」是 6 条阻断中的前 3。完整 6 条阻断见 §4 阻断表（F-9 / F-7 / F-10 / F-14 / F-15 / F-16），须修 10 条见 §4 须修表。其中 **F-14 / F-15 / F-16** 为子代理第二轮补出、经我亲自 `grep`/`sed` 复核后落定（见 §7）。

- **F-1**（须修）：WCS 正确性门对 NaN 结构性免疫，`:4790`/`:5236` 的 `isfinite` 是**死代码**；ipv 路径无 CD/CRVAL 有限性检查（explicit 路径有）。详见 §5 反例 A。
- **F-2**（须修）：帧级并行下 W/H 基准 `f_wh` 数据竞争，导致非确定性失败；`:2566-2567` 恰声称"确定性"。详见 §5 反例 B。
- **F-3**（须修）：截断守卫 `p1_image_sane` 只接 3/20 读点。
- **F-8**（须修 / 阻断候选）：`:10866-10872` 丢弃 `append_write` 返回值，同函数 `:10879` 却检查了 `append_close`；`:10873` 无条件记录 tile 偏移 ⇒ 中途短写会造成下游按记录偏移读到**文件长度内的错位数据**（静默数值污染）。
- **F-6**（须修）：6 条悬空引用，含 `docs/engineering/THREADING_MODEL.md`（并行轴不变式的唯一规范依据）与 `run/MEMGOV-01/REPORT.md`、`run/WCS-DETERMINISM-01/REPORT.md`。

---

## 3. 逐文件清单

### `lib/infrastructure/scheduler/src/module_adapters.cpp`（16260 行，读毕）

| 区段 | 行 | 读了什么 | 看到什么 / 判定 |
|---|---|---|---|
| 文件头映射表 | 1-36 | Phase1/2/3 节点 → 真实 operation 委托表 | `:16-17` 声明 `hp_drizzle_run` "仅剩定义、无生产调用者"；`:15` 称生产调用点在本文件 `:4745`。**须修**：`:4745` 落在 `p1_op_wcs` 的显式 WCS 采样循环内，与 drizzle 无关，该行锚已漂移（见 F-9） |
| include 面 / aio 薄转发 | 37-260 | 生产头、`aio_fs` 命名空间、`walk_tree` | `:171-174` 声明"本 TU 的文件 I/O 全部经 aio 机制原语"。**须修**：实测 6 处裸 `fopen/fread/fwrite`（F-4）。`:221` `walk_tree` 深度>64 静默截断返回 0（**建议**） |
| executor / OMP ICV | 261-465 | `shared_work_executor`、`ScopedOmpWorkerInjection`、`to_result` | `:325` `create_cpu_heavy_executor` 失败 → `return nullptr` → 静默降级为串行，注释未覆盖此分支（**建议**）。`:387` 恢复用 `if (prev_ > 0)`，`prev_` 初值 −1（**建议**） |
| 独立 TAN 参考解 | 467-534 | `p1_tan_forward_reference`、`p1_angular_sep_deg` | 确为与 `WcsTan` 不同源的推导（gnomonic 手写），**不是**同义反复。这是对 `:467-470` 记录的旧自洽门的真实修复，**判为合规** |
| SessionModule + 22 个 descriptor | 536-1216 | 会话适配器 + P1/P2/P3 全部 ModuleDescriptor | `:722-731`、`:751-759` 自述"**已知不一致（未裁）**"，但这些 descriptor 在 `:16171`/`:16179` **被真实注册**（F-7）。`:6633-6649` 与 `:1450-1451` 三份 `206.265` 副本，违反"唯一权威常量"（**建议**） |
| P1Image / 读图 / 守卫 | 1250-1425 | RAII、`p1_read_image`、`p1_image_sane`、`p1_fits_saturation_level` | `p1_image_sane` 自身是**真**磁盘事实检查（非同义反复），**判为合规的守卫**——问题在它没被接线（F-3）。`ipv_select.cpp:57` 的派生式漏 `×1e-3`（**建议**） |
| 帧键 / 原子发布 / 单位声明 | 1427-1815 | `p1_frame_key`、`p1_atomic_publish`、`p1_write_fits_atomic`、`mu::*` | `:1515-1517` `const P1Image&` 内改 `im.p->bits_per_sample`（**建议**）。`:1709-1752` `p1_image_stats` **丢弃非有限像素**再取中位数，`all_finite` 全仓只写不读（F-4）。`:1530-1535` `p1_calibrated_path` 静默回落原帧（F-2） |
| p1_parallel_for | 1817-2361 | 帧轴治理、轴分配、内存闸门 | `:2099-2183` **每次调用现建 `std::thread` 池**（违反 AGENTS §6，私建池）。`:2217-2222` 不变量自检 fail-closed，**判为合规**。`:2566-2594` 的 `f_wh` 竞争（见 ②） |
| p1_op_calibrate | 2357-2792 | 校ibrate 全流程 | `:2631` `dark_scale_factor` 与 EXPTIME 比值门，**判为真实交叉门**。`:2471` 首帧口径注释明确（**建议**：`observed_median.light` 用首帧而检查用逐帧，易被误读） |
| p1_op_cosmetic | 2821-3306 | 坏点/坏列/掩膜/母版回填 | 三路径计数与"已修/仅标记"分账做得扎实（`:3144-3148`）。`:2907-2919` 裸 `fopen` + 吞 `master_refs.json` 解析失败（F-4） |
| star-psf / 引导检测 | 3308-4386 | 星表引导、PSF 拟合、FAST/PRECISE 分派 | `:4096` `s.snr = (peak-bg)/noise_sigma` **无 sigma 有限性/正性守卫**（**须修**，见 F-8）。`:4154` 比较器遇 NaN flux 违反严格弱序（**建议**）。`:3325`/`:4364` "psf_params 零消费者"声称与 `:4249`/`:5778-5806` 同文件矛盾（F-5） |
| SIP / header_pointing / p1_op_wcs | 4388-5328 | SIP 桥接、初始指向、板解 | **本片最重问题在此**：`:5210-5224`+`:5230-5241`（①）。`:4680-4690` 显式 WCS 与 `:5192-5241` ipv 路径的守卫**不对称**。`:4932` 引已删的 `run/WCS-DETERMINISM-01/REPORT.md`（F-6） |
| p1_op_photometry | 5330-6565 | 孔径测光、k_photo、PHOT-MXY-01 空间增益 | `:5420` 裸 `#pragma omp parallel for`（嵌套并行，默认 safe；**建议**）。`:5858` `catch(...)` 吞异常后静默回落 `config.wcs`（**须修**）。`:5685-5696` 空间增益 6 个默认阈值硬编码，出处文件**存在**（已核） |
| 噪声模型 / p1_op_noise | 6567-7571 | `p1_noise_cfg_apply`、`p1_noise_model_for_frame`、SNR | `:7183` `catch (const std::exception&)` 无名吞异常（F-4 类）。`:6793-6799` variance_floor 按 α² 换算，**真实单位链修复，判为合规**。`:7149` `ref_mag=6.0` 有负责人原话出处（**建议**） |
| §5d 审计 / p1_op_drizzle | 7573-8659 | 方差面审计、drizzle、nside 合规 | `:8875-8879`（在 writer 段）作者**自己写明**"n_variance_tiles == n_tiles 是同义反复、恒真门"，并改用落盘 tile 普查 —— **本片质量最高的一段**。`:7925` 另一处 `catch(...)` 吞 `p1_wcs.json` 解析失败（F-4） |
| p1_op_writer | 8661-9169 | 产品事实校验、方差普查、BUNIT 声明 | `:8870-8962` 独立重开 HiPS 逐 tile 普查，`uncertainty_available` 由**磁盘事实**导出 —— 这是本片唯一一处完全符合"不接受检查通过"的实现，**判为合规**。`:8707`/`:8774` 吞解析异常（**建议**） |
| P2 共用 + coverage/sample | 9171-9724 | 工具、coverage、sampler | `:9258-9283` 第二处**私建 `std::thread` 池**。`:9354-9386` `input_manifest_hash` 十进制/数值序对齐，**判为真实修复**。`:9608-9612` lease 恒最后赋值，**判为正确** |
| upm-fit / sky plane | 9726-10362 | UPM 构建、天光面自适应 | `:10246-10357` 自适应搜索与 provenance 登记做得完整。`:10217-10232` `max_nodes=8192` 有实测档位依据。`:9873-9874` **主动订正了自家悬空引用**（正向样板） |
| upm-apply / reject / integrate / write | 10364-13748 | P2 七节点链 | `:12373-12396` 掩膜游标 + depth + frame_slots 三重一致性核对，**判为真门**。`:11992-11997` `snr_path=dense` 显式 fail-closed。`:13209-13225` `uncertainty_available=true` 反向锁死，**判为真门** |
| P1/P2/P3 NodeModule + 注册表 | 13749-16260 | 三族节点适配器、`register_phase_modules` | `:16171-16184` 把自述"已知不一致未裁"的 `phase2/phase3_descriptor` **真实注册**（F-7）。`:15143` P3 行带走 `rt::shared_work_executor`，`:15174-15177` 明确"池不可得则串行，不自建线程池" —— **同一文件内两种相反做法**（F-10）。`:13819` `catch(...)` 设 `n_frames=1`（已注释，**建议**） |

---

## 4. 发现清单

| **F-14** | `:2655`、`:2669`（lambda 内）；`:2574`、`:2580`、`:2686-2687`（归约） | **worker 线程内并发写共享 `*man` + `f_errkind==2` 死分支。** (a) `(*man)` 是调用方持有的同一个 `Json&`，`p1_parallel_for` 的 `cal_workers` 个 worker **无锁并发写**它 ⇒ UB（撕裂 manifest）。(b) `:2574` 声明 `f_errkind` 语义为 `0=无 1=input 2=output`，但 lambda 内**唯一**一次赋值是 `:2580` 的 `f_errkind[fi]=1`；`:2655`/`:2669` 绕开它直接写 `(*man)`。⇒ 归约的 `:2687` `else if (f_errkind[fi]==2)` **永不命中**；其余六类帧级失败（尺寸不符 `:2586`/`:2596`、`ac_calibrate_frame` 失败 `:2647`、重读失败 `:2656`、重读尺寸不符 `:2660`）全部落在 `f_errkind==0` ⇒ **不产生任何 `error_kind`** ⇒ 按 `runtime_client.cpp:539-545` 走域→码表得 exit 2，而同类输入问题在 `:2373`/`:2381` 走 exit 3。**同文件正确样板就在隔壁**：cosmetic op 的 `:3008` 用每帧槽 `f_errkind[fi]=3`，lambda 内不碰 `*man` |
| **F-15** | `:3000-3001` vs `:3227-3228`（及 `:3264-3266`） | **manifest 报的参与面与真实门控是两个不同谓词（产品 provenance 说谎）。** 真实门控是 `p_dark_ok = m_dark.ok() && 尺寸匹配`，manifest 写的是 `st["bad_column_path_dark_active"] = m_dark.ok()`（**不含尺寸校验**）。母版可读但尺寸不符 ⇒ 该路径实际不参与、`ac_correct_frame(..., nullptr, ...)` 被调用，而 manifest 仍报 `= true`，且同一错误值落盘进 `badcol_report.json`。注释 `:2998-2999` 明写「必须留痕」。姊妹同类：`:2978`（`:3008` 设 `f_errkind[fi]=3`）与 `:3124-3129` 归约只映射 1/2/4，`3` 未接 |
| **F-16** | `:1530-1535`（调用点 `:2332 :2982 :4725 :4825 :5025`） | **`p1_calibrated_path` 把「产物不可读」与「产物不存在」合并 ⇒ 静默改吃未标定原始帧。** `aio_fs::exists`（`:176-178`）= `path_exists(p,nullptr)!=0`，对 EACCES/EIO/ESTALE 同样返回 false ⇒ 已标定产物存在但 stat 失败时**零痕迹**回退到 raw `light`，下游 5 个节点（memory cap 探测、cosmetic、wcs 三处）据此工作。cosmetic 的 `:3149` 报的还是原始 light 的 basename，替换无任何留痕 |

| **F-17** | `:12498-12501` vs `:11313-11318` 与 `:12635-12640` | **同一 op 内 130 行外，同一循环体、同一 tile、同一读，对 support 读失败三处两套政策 —— 全文件最干净的「伪装成 fail-closed 实则 fail-open」。** `:12635` 对 ivar 读失败硬判 `ErrorDomain::DATA`；`:11313` 在 reject op 对 support 平面同样硬判；**唯独 `:12498` 无 else 分支**，`has_sup[d]` 留 false ⇒ `sp=0.0` ⇒ 该帧在整个 512×512 tile 上静默退出 mosaic，**无计数器、无 manifest 键、无 stderr**。作者显然知道这条规则（就在 130 行外），属选择性失效而非遗忘。**阻断** |
| **F-18** | `:11117-11122`（生产者 `:10980`） | **守卫写成重言式，形同虚设（恒真门）。** `tile_span = cor_doc.value("tile_leaf_span", kP2TileLeafSpan)` ⇒ **缺键时取默认值** ⇒ 紧邻的 `if (tile_span != kP2TileLeafSpan)` 恒为假；且生产者 `:10980` 恒写 `kP2TileLeafSpan` ⇒ **正常路径上也永不命中**。两层叠加使这道「拒绝非标准 tile」的门成为纯装饰。**同文件 6000 行内对同类 required 键有正确哨兵**（`:10519` `cov_doc.value("target_order", -1)` + 后续 `<0` 拒绝）。同类命中：`:11933`、`:13189` 同样默认 `kP2TileLeafSpan`。**须修（恒真门，与 F-1 同族）** |
| **F-19** | `:13729-13730`（对照 `:13540`/`:13551`/`:13562`） | **同函数内唯一不设 `disk_full` 的失败点，且发生在原子发布之后。** 前三个失败点全部 `if (dsk_epoch.failed()) (*man)["error_kind"]="disk_full"`；`:13729` 写 `p2_final.json` 失败不设 ⇒ `runtime_client.cpp:544` 的 exit 10 不触发 ⇒ 落到 `exit_code_for_error_domain(IO)`=**exit 7**（同一次运行同类失败退出码不同）。更严重：`:13729` 在 `:13621` 的 mosaic 原子发布**之后** ⇒ 失败时 `out_dir` 有已发布的 signal/support/variance/ivar 却无 `p2_final.json`（该函数 `:12855-12861` 声明的唯一"完成"标记）⇒ **半发布产品**。另错误串 `"artifact write failed"` 丢路径（同文件 `:9453`/`:10996`/`:11602`/`:12811` 均带路径）。**须修** |

### 阻断（7 条）+ 须修（12 条），全部经我亲自复核原文证成

| ID | 位置 | 问题 |
|---|---|---|
| **F-9** | `:8098-8103` | **fail-closed 决策的代码依据为假。** 断言「`drizzle_engine.cpp:1919` 对 `varianceValue<=0` 是整像素 continue ⇒ 清空 signal/support」。实测 `drizzle_engine.cpp:1705-1716`：仅 `acc.sumVarNum`（:1712）被 `if (varianceValue > 0.0f)` 守卫；`acc.sumArea`（:1705）、`acc.sumNorm`（:1709）、`acc.nContrib++`（:1716）**无条件执行** ⇒ signal/support 不受影响。被引的 `:1919` 实为 stripe/worker 确定性注释，与 variance 无关。正本 `docs/science/algorithms/DRIZZLE_GEOMETRY.md:214` 明文相反（「`V_j ≤ 0` 不构成掩膜授权……置 0 不 continue」）。决策本身可能仍保守正确，但理由为假 ⇒ 按 AGENTS §3 文档优先，注释必须订正 |
| **F-7** | `:8009-8016` + `:7927-7938` | **drizzle 节点 WCS 头零值兜底 + 守卫未接线。** `p1_wcs_astrometry_usable` 全文件仅 2 个真实调用点（`:3507`、`:5868`）；`p1_op_drizzle` 的唯一 WCS 门是对象存在性（`:7927`）。8 个头键全用 `p1_num(..., 0.0)` 兜底。缺 `cd11` 的 `p1_wcs.json` 或缺键的 `config["wcs"]` ⇒ `CD1_1=0`、`CRVAL=0` 写入标准 HiPS tile 头而节点报成功。守卫 `:4455-4463` 头注逐字记录了它专治的病，photometry 接了、drizzle 没接 |
| **F-10** | `:12211`、`:3812`、`:3321`、`:4313`、`:4366`、`:16145` | **两份"机器门禁"凭据全部不存在。** `eng/ci/check_no_weight_mode_code.py`（声称"删改即门禁判红"）与 `eng/ci/check_registry_ir_parity.py`（"机器断言 C4–C8"）——`eng/ci/` 整目录不存在；`docs/engineering/ENGINEERING_SPEC.md` §2「保留则注释」（6 处死码保留依据）——不存在，全仓仅存于未追踪的 `run/` 残留。权重口径退役拒绝面与精确 PSF 死码保留的**全部正当性论证**挂在这两个不存在的权威上。仓内审计 `实验/engineering-evidence/audit-2026-01/ROOT_CAUSES.md:192` 已标为 V8-N-01「凭据虚构」，注释从未订正 |

### 须修

| ID | 位置 | 问题 |
|---|---|---|
| **F-1** | `:5210-5224` + `:5230-5241`；对比 `:4761-4771` + `:4781-4794` | **WCS 两道正确性门对 NaN 结构性免疫。** `if (rt > max_rt) max_rt = rt;` 使 `max_rt` 不可能是 NaN，故 `:4790`/`:5236` 的 `!std::isfinite(...)` 是**死代码**。ipv 路径 `:5195-5198` 无有限性/det 检查（explicit 路径 `:4710-4714` 有）⇒ 全 NaN 的解算结果让两道门读 `0.0` 判绿，并把"完美"的 `max_roundtrip_px=0.0` / `max_forward_cross_deg=0.0` 写进产品。`:4785-4789` 注释自称"真正可失败，非恒真"——该声称对本形态不成立 |
| **F-2** | `:2575` + `:2590-2594`（注释 `:2566-2567`） | **帧级并行下 W/H 基准的数据竞争 + 非确定性。** `f_wh` 被所有 worker 共享，仅 `fi==0` 写（`:2591`）、其余帧在 `:2593-2594` 无同步读。当无任何母版（`W_base<0`）且 `frame_w>1 && n_lights>1` 时，先于 frame 0 到达的帧读到 `f_wh[0]==-1` ⇒ `W=-1` ⇒ `:2595` `light.w()!=W` ⇒ 报 `light size mismatch vs first frame`。注释恰好声称"不再是帧间共享可变状态……与串行逐字一致" |
| **F-3** | `:1285-1293`（定义）vs 全文件 20 处 `p1_read_image` | **截断守卫只接线 3/20。** `p1_image_sane` 自述"补 aio_read 数据缺字 WARN 放行的缝隙"，但只在 `p1_op_calibrate` 的 `:2372/:2380/:2463` 使用；cosmetic `:2983`、star-psf `:3959`、wcs `:4726/:4826/:5026`、photometry `:5389/:5884`、drizzle `:7900` 等**一律裸奔** |
| **F-4** | 写点 `:2763` `:2766` `:2907` `:2912` `:3289` `:3292` `:9192`；读点 `:1709-1752`（`:1724`）、`:5858`、`:7183`、`:7925`、`:8129`、`:8707`、`:8774`；自述违反 `:171-174`、`:2317` | **注释与代码不符 + 空捕获。** (a) 文件头与 `:2317` 声称"本 TU 不再自持 fopen/fgets 通道"，实测 6 处裸原语。(b) `p1_image_stats` **丢弃非有限像素**再算中位数，其 `all_finite` 标记**全仓只写不读**（`grep` 全仓确认唯一写点即 `:1724`）—— 恰是"先筛子集再取极值，被筛掉的恰是最差那条"的形态。(c) 7 处 `catch(...)`/无名 `catch` 吞掉 JSON 解析异常，其中 `:5858`/`:7925` 吞掉后**静默回落到 `config.wcs`**，把"产品损坏"伪装成"配置回退" |
| **F-5** | `:3325` 与 `:4364` vs `:3336` `:4237-4249` `:5778-5806` | **"零消费者"声称与同文件代码矛盾。** 两处注释称 `psf_params`"在仓库内零消费者、psf 端口为死边"；但同文件 `:4249` 生产该列并注明"它是测光 F_instr 的唯一合法来源"，`:5778-5806` 按 `star_id` 关联 `psf_params` 行取 `flux` 作为 F_instr。仓外 `frame_photometry_fit.h:39` 亦记载该依赖。**裁定**：生产链确无 `p1_psf.json` 的**文件级**消费者（已 grep 确认，仅测试消费），但"零消费者"作为**退役依据**不成立且与同文件注释直接冲突——若据此清理 `:4249` 的 `flux` 列，将同时废掉 F_instr 的唯一合法来源 |
| **F-6** | `:1871`、`:265`、`:1939`、`:2016`、`:3346`、`:3800`、`:3812`、`:4932`、`:10041`、`:10564`、`:11670`、`:11671`、`:11973`、`:12211` | **6 条悬空引用（权威路径整段已删）。** 脚本提取全文 86 个被引路径逐一 `test -e`，13 个不存在，其中 6 个是真悬空：`docs/engineering/THREADING_MODEL.md`（并行轴不变式的**唯一规范依据**，3 处引用）、`eng/ci/check_registry_ir_parity.py`（被引为 DAG 时序的"**机器断言**"）、`run/MEMGOV-01/REPORT.md`（帧丢弃安全性的三条前提依据）、`run/WCS-DETERMINISM-01/REPORT.md`（Gaia fail-closed 的根因）、`docs/detail/algorithms_phase1/*`（整目录不存在）、`docs/detail/algorithms_phase2/*`。这些路径正是最近 3 个提交删除的对象 |
| **F-8** | `:10866-10872` + `:10873` + `:10879` | **丢弃 `append_write` 返回值。** `(void)aio_atomic::append_write(...)` 两处被 `(void)` 显式丢弃；而**同一函数内** `:10879` 的 `if (aio_atomic::append_close(df) != 0)` 却检查了。`:10873` 仍无条件 `fo.tiles.push_back({tip, tile_offset})` 记录该 tile 偏移。⇒ 中途短写时，后续 tile 数据整体前移，而下游 `:12461` 按记录偏移 `p2_read_bin_range` 读，**落在文件长度内但内容错位** ⇒ 静默数值污染（非 fail-closed）。这是「局部漏洞而非房规」的典型证据 |
| **F-11** | `:223` + `:221`；`aio_atomic_file.h:401` | **目录枚举失败被丢弃。** `(void)aio_atomic::for_each_child(...)` 丢弃返回值，而被调方头文件**明文承诺 fail-closed**（`:401`「返回 0 = 遍历正常结束; 非 0 = 无法打开目录 **或 lstat 失败**」）。本 TU 唯一调用点 `:8827` 的 tile 计数直接喂给 `:8850`/`:8860` 的 fail-closed 判据。另 `:221` 深度 >64 `return 0`（成功）静默截断 |
| **F-12** | `:2373`/`:2381`/`:2464` vs `:2400-2402`/`:2411-2413`/`:2426`/`:2432`/`:2485`/`:2495`/`:2507`/`:2513` | **`error_kind` 覆盖不均导致同类失败两个 CLI 退出码。** `p1_op_calibrate` 内部对"输入坏了"给出两个不同码：设 `error_kind="input"` 的（exit 3）与同属 DATA 却不设的（exit 2）。`runtime_client.cpp:539-545` 证实了这一分歧的后果。另 `:677` 把 `fn_create` 失败硬编码 `INTERNAL`，而 `:571`/`:633` 走 `to_result` 的完整域映射 ⇒ 同一失败在 70 与 7/5/2/9 之间跳 |
| **F-13** | `:14518-14527` vs `:14399-14420` | **`p3n_wcs_from_json` 对 8 个 WCS 键全部默认 0.0，只校验 `projection`。** 被 resample/writer/verify 三个节点消费。**同文件的姊妹函数 `p3n_crop_from_plan`（`:14408-14411`）对同一份 `p3_wcs.json` 的每个键都严格拒绝并给具名原因** —— 同文件两种严格度。姊妹问题：`:13177` `target_order` 缺键默认 0 并通过 `[0,20]` 校验 ⇒ `nside=512`，而正确值可能是 4096（采样率差 8 倍且无提示） |

### 建议

| ID | 位置 | 问题 |
|---|---|---|
| S-1 | `:2099-2102`、`:9265-9268`（10 个调用点：`:2576 :2979 :3953 :5373 :6244 :7221 :7837` / `:10654 :11261 :12440`） | **私建线程池 2 处本体、10 处调用点。** 每调用现建 `std::vector<std::thread> pool`，违反 AGENTS §6。**项目自己已登记未修**：`:15996-15997` 原文「其余 P1/P2 session 节点保持整租约执行模型不变（其内部池为租约驱动 per-call worker…work-unit 化待后续任务, 见 **F-RT-001-05**）」。合规样板是对面的 P3：`:15143` 走 `shared_work_executor`，`:15174-15175` 明确「不自建线程池（禁止回退到调用点 spawn）」。建议按 F-RT-001-05 整体迁移，不要逐点打补丁 |
| S-1b | `:2101-2102`+`:2183`、`:9267-9268`+`:9280` | **两处 `std::terminate` 洞。** `emplace_back` 在 for 内、`join` 在 for 外；`std::thread` 构造抛 `system_error` 时栈展开 → `vector<thread>` 析构 → 对仍 joinable 的线程 `terminate()`。两处 worker 体内的 `catch(...)`（`:2178`/`:9275`）只兜业务异常，**兜不住构造失败**。独立于线程池计数的新违规类别 |
| S-1c | `:2236-2250` + `cpu_budget.cpp:41,53-59` + `runtime_resources.json:66` | **无 lease 路径线程数无上限。** `__workers` 仅在 `:13907`/`:14129` 注入；其余调用方落到 `process_cpu_budget()` → `affinity_cpu_count()` → `std::thread::hardware_concurrency()`。而 `cpu_budget_max: 0`（配置自身 constraint 明写「0 = 不设上限」）使 `if (cap > 0u)` 永不成立 ⇒ 退化为「整机核数」且**不记入 ThreadBudget**。同理 `p1_max_frames_in_flight: 0` 使 `:1966-1972` 的帧宽上限**成死码**。注：该配置项自身又引用了**已删除的** `THREADING_MODEL.md`「并行轴分配」 |
| S-2 | `:16171-16184` + `:722-731` + `:751-759` | **自述"已知不一致未裁"的 descriptor 仍被真实注册。** `phase2_descriptor`（module_id `acsd.phase2.resample` 不在 registry 的 20 个 module 内；`DATA-P2-RES` 单位自相矛盾）与 `phase3_descriptor`（输出 `DATA-TILE-001` —— 注释自述"在 registry、DATA_SEMANTICS.md 与全仓均无定义"）都在 `register_phase_modules` 里 `register_module` + `register_factory`。⇒ 注释里"供不按 20 节点装配的调用方"之说不完整 |
| S-3 | `:5195-5198` + `:4710-4714` | 补齐 ipv 路径的 8 参有限性与 `det != 0` 检查（explicit 路径已有），并把 `:4790`/`:5236` 的 `isfinite` 守卫改成对**样本集合**判空而非对**已过滤的 max** 判 |
| S-4 | `:4096` | `cat.noise_sigma` 无 `>0`/有限性守卫即做除法；`estimate_background` 返回 true 不等于 sigma 合法 |
| S-5 | `:6633-6649`、`:3374`、`:4578` | `206.265` 三份副本 + `ipv_select.cpp:57` 注释漏 `×1e-3`（该文件给出的是 `(180×3600)/π`，实为 206264.8，量纲错） |
| S-6 | `:221`、`:325`、`:387`、`:4154`、`:5420`、`:8041`、`:8022` | 杂项：`walk_tree` 深度>64 静默截断；池创建失败静默降级串行且注释未覆盖；ICV 恢复条件 `prev_>0`；`partial_sort` 比较器遇 NaN 违反严格弱序；`:5420` 裸 omp parallel 的嵌套性依赖 OpenMP 默认未开启；`CROTA1/2` 硬编码 `"0"` 与非零 `cd12/cd21` 不自洽 |
| S-7 | `:1692`、`:2471`、`:2531`、`:7149` | `observed_median.light` 用首帧而门用逐帧（`:2457-2458` 说明了理由，但产物键名不区分，易被下游误读）；`ref_mag=6.0` 为约定值 |

---

## 5. 我主动构造的反例

### 反例 A（推翻 F-1：**成功**）

**构造**：让 ipv 求解器返回 `r.cd11 = NaN`（或任一 CD/CRVAL 非有限），其余正常。

**推导**（对照 `wcs_tan.cpp:8-39` 的 `WcsTan::pix2sky`）：
- `xi = (NaN*dx + cd12*dy) * d2r → NaN`；`R = sqrt(NaN²+…) = NaN`
- `:24` `if (R < 1e-12)` → `NaN < 1e-12` 为 **false** ⇒ 进入 else 分支 ⇒ `rho=atan(NaN)=NaN` ⇒ `dec_out = asin(NaN) = NaN`，`ra_out = NaN`
- `sky2pix(NaN,NaN)`：`while (dra > M_PI)` 与 `while (dra < -M_PI)` 对 NaN 均为 false ⇒ `dra` 保持 NaN ⇒ `xi_deg/eta_deg = NaN` ⇒ `bx = crpix1 + NaN = NaN`
- `rt = sqrt(NaN) = NaN`
- **`module_adapters.cpp:5217` `if (rt > max_rt) max_rt = rt;` —— `NaN > 0.0` 为 false ⇒ `max_rt` 恒为 `0.0`**
- `cross = p1_angular_sep_deg(NaN,…)`：`s = NaN`，`if (s>1.0)`/`if (s<0.0)` 对 NaN 均 false ⇒ 返回 NaN；**`:5224` `if (cross > max_cross_deg)` 同样 false ⇒ `max_cross_deg` 恒为 `0.0`**
- 门 1 `:5230` `max_rt >= 1e-6` → `0.0 >= 1e-6` = **false** ⇒ 放行
- 门 2 `:5236` `!std::isfinite(0.0)` = false，`0.0 > 1e-9` = false ⇒ **放行**

**结果**：节点成功返回，落盘 `max_roundtrip_px: 0.0`、`max_forward_cross_deg: 0.0` —— 两个**看起来完美**的残差，而 WCS 实际是全 NaN。

**期望推翻什么**：推翻 `:4785-4789` 的"真正可失败，非恒真"声称，以及 `:4790`/`:5236` 两处 `isfinite` 守卫的存在意义（它们无法被本形态触发）。

**是否推翻**：**推翻成功**。关键点是：`isfinite` 守卫并非"漏了某个 case"，而是**被上游的 max 累加写法从结构上剥夺了触发可能**——守卫与被检量由同一条 `if (x > max) max = x;` 语句链耦合，属于典型的自洽式防御。

### 反例 B（推翻 F-2：**成功**）

**构造**：config 只含 `input_lights`（≥2 帧），**不提供任何母版**（`master_bias/dark/flat` 全缺）⇒ `:2396-2404` 的 `W` 保持 −1 ⇒ `W_base = -1`（`:2570`）。lease ≥ 2、`p1_memory_cap ≥ 2`、`n_lights ≥ 2` ⇒ `frame_w ≥ 2`。

**推导**：`p1_parallel_for` 用原子 `next.fetch_add` 认领（`:2068`）。worker A 认领 fi=0，worker B 认领 fi=1，两者并发。
- 两者都先执行 `:2578` `p1_read_image(lp)`（一次完整文件读）
- worker B（fi=1）先抵达 `:2584`：`W_base >= 0` 为 false ⇒ `else if (fi == 0)` 也为 false ⇒ **不写 `f_wh`**
- 抵达 `:2593-2594`：`const int W = f_wh[0];` —— 此时 `f_wh` 仍是 `:2575` 初始化的 `-1`
- `:2595` `light.w() != W` ⇒ `4500 != -1` 为真 ⇒ `f_err[fi] = fail("light size mismatch vs first frame")`

**结果**：非确定性失败——同一份输入、同一个二进制，两次运行可能一次绿一次红；且报错信息（"light size mismatch"）与真实原因（首帧基准尚未建立）完全无关。

**期望推翻什么**：推翻 `:2566-2567` 的"判据与串行相同且确定性"与 `:2564-2565` 的"与串行逐字一致、与 worker 数无关"。

**是否推翻**：**推翻成功**。附带发现：这是一个数据竞争（UB），不只是顺序问题——`f_wh` 在 C++ 内存模型下无 happens-before 关系。

### 反例 C（试图推翻 F-3：**未推翻，确认成立**）

**构造**：把 `calibrated_<base>` 之外所有 `p1_read_image` 调用点也接上 `p1_image_sane`，看是否"多数场景不会触发"。
**结果**：`p1_image_sane` 判据是真实磁盘事实（`文件大小 ≥ 头块数×2880 + w·h·bpp`，`:1364-1384`），不依赖任何"检查通过"。它在 `p1_op_calibrate` 之外 17 个点缺席，意味着**下游 5 个节点消费的全部图像都未经该守卫**。该守卫的缺失不是"多数场景无害"，而是"只有根节点验过完整性、其余节点各自独立读盘且不复验"。

### 反例 D（尝试推翻 F-5 的"零消费者"：**部分推翻，我的原假设被否**）

**我最初的假设**：`psf_params` 声称零消费者是假的，生产链仍在消费。
**盲复算**：全仓 grep `psf_params|p1_psf.json`，排除 `module_adapters.cpp` 自身后，命中全部落在 docs / README / 测试（`p1001_real_nodes_test.cpp:2328`、`p1snr_frame_parity_test.cpp:5`）。**生产代码中无任何一处打开 `p1_psf.json`**；`:5786` 读的是 `cat[i]["psf_params"]`，即 `p1_sources.json` 内的**内嵌**段，由 `p1_op_star_psf_impl:4230-4252` 自己写入 —— 它不读 `p1_psf.json` 这个文件。
**结论**：我对"文件级消费者"的假设**被否**，予以记录。但 F-5 仍成立，理由改为：同一文件 `:4249` 写的 `flux` 列与 `:5778-5806` 的消费构成闭环，与 `:3325`/`:4364` 的"零消费者/psf 端口为死边"**直接矛盾**；且 `frame_photometry_fit.h:39` 独立记载了同一依赖。真正的问题是**注释自相矛盾**，而非"存在未发现的调用者"。

---

## 6. 盲复算

**方法**：遮住上面全部结论，重新只读代码、对以下三项独立取证：

| 项 | 盲复算独立结论 | 与上文 | 判定 |
|---|---|---|---|
| `p1_image_sane` 接线覆盖 | 3 个调用点，全在 `p1_op_calibrate`；`p1_read_image` 共 20 处 | 一致 | **判一致** |
| `all_finite` 是否有消费者 | 全仓 `git grep -n all_finite`：唯一写点 `module_adapters.cpp:1724`，零读点 | 一致 | **判一致** |
| WCS 门 `max_*` 是否可能非有限 | 累加式 `if (x > max) max = x` 只接受 `x > max ≥ 0` 的值，NaN 与负值均被拒 ⇒ 只能得到有限非负值 | 一致 | **判一致** |
| 悬空引用条数 | 脚本提取 86 个路径，`test -e` 命中 73、缺失 13；逐条判定后真悬空 6 条（另 7 条为 `docs/ACSD_DESIGN` 省略 `.md`、`lib/phaseN_session/`、`eng/packaging/config/neighbor*` 等散文前缀，非缺陷） | 一致 | **判一致** |
| `phase2/phase3_descriptor` 是否真注册 | `:16171-16177`、`:16179-16184` 确有 `register_module` + `register_factory` | 一致 | **判一致** |

**偏松/偏严自评**：本片判定为**需修**而非阻断，与盲复算一致。若历史结论把本片判为"通过"，则**本片结论比既有结论严**——理由是 F-1（正确性门对 NaN 免疫）与 F-2（数据竞争导致非确定性）两处，都落在"判据看似严密、实则不可触发/不可复现"这一类，而这类正是本轮要求重点打的靶心。

---

## 7. 子代理派发记录

按纪律第 8 条派出 **5 个**子代理（并行、全部后台）。**定稿时 5 个全部回传了取证报告**（其中线程池向回传两次，第二次为压缩摘要，内容一致）。

| # | 主题 | 回传 | 复核结果 |
|---|---|---|---|
| 1 | 静默降级 / 空捕获 / 错误码 / 失败语义 | ✅ | **贡献 F-8、F-11、F-12、F-13** 主体；并给出 B 类"零个未检查调用点"的关键否证 |
| 2 | 退役对象活调用者 / 悬空引用 / 注释与代码不符 | ✅ | **贡献 2 条阻断（F-9、F-10）+ 强化 F-6/F-5**；并报出 `docs/ACSD_DESIGN.md` 行锚系统性漂移（`§8.3:615`「可丢弃重跑」被引 11 次而该词在设计文档中零命中） |
| 3 | 线程池 / 并发所有权（详细版） | ✅ | **贡献 S-1 / S-1b / S-1c**；报出 10 处调用点清单与两处 `std::terminate` 洞 |
| 4 | 自洽式断言 / 恒红恒真门 / 硬编码 | ✅（压缩摘要） | 覆盖由 §5 反例 A/B + §6 盲复算独立承担 |
| 5 | 数值稳定性（NaN/Inf/奇点/除零/溢出/归约序） | ✅（压缩摘要） | 覆盖由 F-1 / F-4 / `:4096` 除零（S-4）独立承担 |

### 逐条复核与否决记录

**我先证伪、随后不再作为结论的**：
- `mu::Stats::has_finite` 未初始化导致门失效 —— 读 `lib/include/acsd/core/master_unit_guard.h:61-67` 后确认 `has_finite = true`，**假设被否**。
- 「`p1_op_noise` 仍以 `psf_params` 为 SNR 目录」—— 复算 `:7400-7404` 走 `DATA-P1-SOURCES.sources` 全量并显式与 `psf.max_stars` 解耦，**假设被否**（F-5 因此按反例 D 改写理由）。
- 「`:1936` 与 `:1981` 是 `p1_parallel_for` 的两个定义」—— 子代理明确**驳回**该说法（`:1981` 是 `#ifdef _OPENMP`/`#else` 内给 `inner_u` 赋 1）。**我采纳其驳回**，原结构性观察作废。
- 「裸 `#pragma omp parallel` 必然超订」—— 依 `:2104` 的 `omp_set_num_threads(inner_omp)`，ICV 被设为 `budget/in_flight`，总并行度 ≤ lease，**不构成超订**。但子代理指出 `:5365` 注释「nested=0 时由所属 worker 串行执行」**与行为相反**（所属 worker 是普通 `std::thread`，不在 omp region 内，region 会真实展开）—— 此条**采纳**，并入 S-6。

**否决子代理的判定（我复核后不同意）**：
- 子代理 3 把 `:303 shared_work_executor` 列为"层次错位 + registry 泄漏" —— **驳回为缺陷**：`:15143` 是它唯一调用点且走 Runtime 唯一 executor，`:15174-15177` 明确禁止回退到调用点 spawn，这是**全片唯一合规样板**。registry 那点属"防御性代码无效"而非泄漏，列 S-6 一笔带过。
- 子代理 1 报「`13234/13236/13239` 读失败静默变空串」为须修 —— **降级为 S-6 建议**：`manifest_hash`/`model_hash` 读失败静默为空是已登记的尽力面设计，真正的问题是 P3 在 `:15427` 对同一件事明确 fail-closed 造成的**跨命令不一致**。
- 子代理 1 报「`8705-8707` 吞掉 writer 的 FAILSEM-01 判红」为须修 —— **降级为 S-6**：下游 `:8751` 的 properties 存在性检查与 `:8850` 的 tile 计数仍 fail-closed，不构成静默出片。
- 子代理 3 对 P2 的 N×N 超订 —— **不采信为缺陷**，标 UNRESOLVED：`:9612`/`:9813` 与 `acr_kernels.cpp:218` 的调用链未在授权片内闭合。

**采纳并经我**亲自读原文复核**通过（升级为阻断的 3 条）**：
- **F-9**：子代理称 `drizzle_engine.cpp:1919` 无 `varianceValue` 相关代码 ⇒ 我亲自 `sed -n '1705,1717p;1917,1921p'` 复核，**确认只有 `sumVarNum`（:1712）被守卫，`sumArea`（:1705）/`sumNorm`（:1709）/`nContrib++`（:1716）无条件执行，`:1919` 是 stripe 确定性注释**。⇒ **阻断**。
- **F-10**：子代理称 `eng/ci/` 与 `ENGINEERING_SPEC.md` 不存在 ⇒ 我亲自 `test -d eng/ci` / `test -e docs/engineering/ENGINEERING_SPEC.md`，**双双确认缺失**。⇒ **阻断**。
- **F-7**：子代理称 drizzle 未接 `p1_wcs_astrometry_usable` ⇒ 我亲自 `grep -n 'p1_wcs_astrometry_usable'` 全文件，**确认只有 `:3507`、`:5868` 两个真实调用点**。⇒ **阻断**。

**采纳为须修的（均经我亲自复核）**：
- **F-8**：`(void)aio_atomic::append_write(...)`（`:10866`/`:10870`）丢弃返回值，同函数 `:10879` 却检查 `append_close`，`:10873` 无条件记录 tile 偏移 ⇒ 中途短写致下游错位读。已复核。
- **F-11**：`:223` `(void)aio_atomic::for_each_child(...)` 丢弃返回值，而被调方 `aio_atomic_file.h:401` **明文承诺 fail-closed**（"非 0 = 无法打开目录 **或 lstat 失败**"）；本 TU 唯一调用点 `:8827` 的计数直接喂 `:8850`/`:8860` 的 fail-closed 判据。已复核。
- **S-1c**：`runtime_resources.json:66` `"cpu_budget_max": 0`（同文件 constraint 字段明写「0 = 不设上限」）⇒ `cpu_budget.cpp` 的 `if (cap > 0u)` 永不成立。**我亲自 `grep` 确认**。附带发现：该配置项 `:62` 自己又引用了**已删除的** `THREADING_MODEL.md`，与 F-6 同源。
- **S-1 的最强佐证**：`:15996-15997` 原文「其内部池为租约驱动 per-call worker…work-unit 化待后续任务, 见 **F-RT-001-05**」—— **我本人通读该行时已读到，此处为独立二次确认**。

**记账口径（子代理建议，我采纳并明确）**：本片对 AGENTS §6「私建线程池」的违规是 **2 处本体、10 处调用点**。若项目既有"9 处"按调用点计，本片独占 10；若按实现计，本片占 2（且已由 F-RT-001-05 登记）。**S-1b（`std::terminate`）与 S-1c（线程数无上限）属独立于该计数的新违规类别，不应混入"9 处"的账。**

**如实说明**：上述"采纳"均经我**亲自读原文复核**，非直接采信子代理输出。凡我未能独立复核的子代理结论（如"某下游内核是否另有守卫"、"P2 是否 N×N"），一律标注为阻断候选、UNRESOLVED 或建议，不计入已确证发现。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# (0) 基线与文件同一性
git rev-parse HEAD
git diff --stat 850a9ede HEAD -- lib/infrastructure/scheduler/src/module_adapters.cpp   # 空
git rev-parse 850a9ede:lib/infrastructure/scheduler/src/module_adapters.cpp
git hash-object lib/infrastructure/scheduler/src/module_adapters.cpp                      # 同上
wc -l lib/infrastructure/scheduler/src/module_adapters.cpp                                # 16260

# (1) 片成员与行数（权威清单）
python3 -c "import yaml;d=yaml.safe_load(open('run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml',encoding='utf-8'));s=[x for x in d['片清单'] if x['片号']=='INF-scheduler-001'][0];print(s['成员文件'],s['成员份数'],s['实际行数'])"

# (2) F-1 反例：max 累加对 NaN 的免疫 + isfinite 守卫不可触发
sed -n '5205,5245p;4755,4800p' lib/infrastructure/scheduler/src/module_adapters.cpp
grep -n 'if (rt > max_rt)\|if (cross > max_cross_deg)\|isfinite(max_rt)\|isfinite(max_cross_deg)' \
     lib/infrastructure/scheduler/src/module_adapters.cpp
# 对照 pix2sky/sky2pix 的 NaN 传播路径
sed -n '8,64p' lib/algorithms/platesolve/wrapper_phase1/wcs_tan.cpp

# (3) F-2 反例：f_wh 共享写读
sed -n '2566,2600p' lib/infrastructure/scheduler/src/module_adapters.cpp

# (4) F-3：守卫接线覆盖率
grep -n 'p1_image_sane' lib/infrastructure/scheduler/src/module_adapters.cpp
grep -c 'p1_read_image' lib/infrastructure/scheduler/src/module_adapters.cpp

# (5) F-4：裸文件原语 + 空捕获 + 死信号
grep -n 'std::fopen\|std::fread\|std::fwrite\|std::ofstream' lib/infrastructure/scheduler/src/module_adapters.cpp
grep -n 'catch (\.\.\.)\|catch (const std::exception&)' lib/infrastructure/scheduler/src/module_adapters.cpp
git -c core.quotepath=false grep -n 'all_finite' -- lib eng docs 实验

# (6) F-6：全文被引路径逐一 test -e
python3 - <<'PY'
import re,os
f="lib/infrastructure/scheduler/src/module_adapters.cpp"
pat=re.compile(r'(?:run|实验|docs|eng|lib)/[A-Za-z0-9_一-鿿./-]*')
seen={}
for i,l in enumerate(open(f,encoding='utf-8'),1):
    for m in pat.findall(l):
        seen.setdefault(m.rstrip('.,;:)】]"\'`'),[]).append(i)
for p,ls in sorted(seen.items()):
    if not os.path.exists(p): print(f"MISSING {p}  lines={ls[:6]}")
PY

# (7) S-1/S-2：私建池 + 自述未裁的 descriptor 被注册
grep -n 'std::vector<std::thread> pool' lib/infrastructure/scheduler/src/module_adapters.cpp
sed -n '16166,16185p' lib/infrastructure/scheduler/src/module_adapters.cpp
sed -n '722,731p;751,759p' lib/infrastructure/scheduler/src/module_adapters.cpp

# (8) F-5：psf_params 消费者边界（确认无生产文件级消费者）
git -c core.quotepath=false grep -n 'psf_params\|p1_psf\.json' -- lib eng | grep -v 'src/module_adapters.cpp'

# (9) 仓外取证：206.265 副本、Stats 默认值、BUNIT 声明独立性
sed -n '55,60p' lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp
sed -n '61,67p' lib/include/acsd/core/master_unit_guard.h
sed -n '8870,8890p' lib/infrastructure/scheduler/src/module_adapters.cpp   # 作者自述的同义反复/恒真门
```

全部命令均为只读（`grep` / `sed -n` / `python3` 只读遍历 / `git rev-parse` / `git hash-object`），不编译、不跑测试、不跑任何二进制、不产生任何 git 写、不修改任何仓内文件（唯一写入为本交付件）。