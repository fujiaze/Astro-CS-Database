import io, sys
sys.stdout.reconfigure(encoding='utf-8')

p = r"独立审计/证据/通读-CR-240.md"

body = r"""
---

# 格 5 · `module_adapters.cpp:1201-1500`

本格内容 = `P2Api`/`P3Api`（:1201-1216）＋ P1 真实节点的公共底座：图像句柄 RAII、FITS 完整性/饱和电平读取、config 取值助手、frame_key/落位路径、原子发布与写盘（:1218-1500）。这段是本文件里**离产品字节最近**的一节。

## §2.5 数值处置表（格 5，16 项）

| 位点(文件:行) | 符号或键 | 现行值 | 它是什么（一句话用途） | 处置 | 依据锚 | 待确认时给保守方向与影响范围 | 置信 |
|---|---|---|---|---|---|---|---|
| :1196（格 4 已列，此处按族补录） | `p1_session_run(h,c,0)` | 0 | 预读深度 | 待确认 | 见 CR-240-13 | — | CONFIRMED |
| :1265 | `aio_file::read_head(path, 6, &magic)` | 6 | FITS 主头魔数 "SIMPLE" 长度 | 结构性不适用 | 定义长度（FITS 主头首卡前 6 字节恒为 `SIMPLE`） | — | CONFIRMED |
| :1272,:1293 | `100ull * 2880ull`（两处同值，合并） | 288000 B | 主头扫描读入上界（100 块） | 待确认 | 条文面零提及（检索式：`git grep -n "288 KB\|100 块\|主头上界" -- docs ASTROCS_DESIGN.md` 零命中；仅 :1270/:1291 注释自述同一数）。超过此上界的主头判 `return 0` ⇒ 整帧被 `p1_image_sane` 判不可读 | 保守方向＝**提高**上界或在超界时报"主头过长"具名错误而非 0；影响范围＝所有 BITPIX 主头超 288 KB 的输入（含大量 SIP/COMMENT 卡的历史归档件） | PARTIAL（未取证现网输入是否可能超界） |
| :1273,:1275,:1279,:1282,:1309,:1311,:1346 | `2880ull`（块大小，7 处同值合并） | 2880 | FITS 逻辑记录长度 | 文献值 | FITS 标准（Heidt et al., `AFUV` 3.0 §2.1/`FITS` 强制 2880 B 块）；`docs/contracts/DATA_SEMANTICS.md` 亦以 2880 为块口径。非科学量、属格式定义常数 | — | CONFIRMED |
| :1276,:1312 | `i < 36`、`blk + i * 80` | 36, 80 | 每块 36 张 80 字节卡 | 文献值 | FITS 标准 2880/80=36，卡宽 80 字节 ⇒ 格式定义常数，`结构性` 与 `文献值` 双成立，按文献值登记 | — | CONFIRMED |
| :1298,:1300 | `c[8] != '='`、`memcpy(val, c + 10, 70)` | 8, 10, 70, 71 | FITS 固定格式卡的值起始列与取值窗 | 文献值 | FITS 标准：列 9（0-based 8）为 `=`，值区自列 11 起（0-based 10）至列 80 ⇒ 70 字节；`val[71]`/`val[70]='\0'` 为缓冲尺寸。我复算过窗不越卡尾（10+70=80） | — | CONFIRMED |
| :1294-1295,:1305,:1315-1316,:1319,:1322,:1328 | `0.0` 哨兵与 `v <= 0.0` 判据 | 0 | "未提供/无效饱和电平"的编码值 | 见 CR-240-16 | 待确认 | 保守方向＝把"读不出/超界/无 END"与"卡缺失"分成不同返回值；影响范围＝P1 噪声/SNR 节点的饱和域降级声明 | CONFIRMED |
| :1305 | `!std::isfinite(v) \|\| v <= 0.0 → return false` | — | 非有限/非正数值视为未提供 | 结构性不适用 | 方向＝保守（拒绝 NaN/Inf/0/负电平），非放行 | — | CONFIRMED |
| :1333 | `im.w() <= 0 \|\| im.h() <= 0 \|\| im.px()==nullptr` | 0 | 几何/指针有效性 | 结构性不适用 | 非负性判据 | — | CONFIRMED |
| :1335-1337 | `opt.bits_per_sample <= 0`、`/8ull`、`bpp == 0` | 8, 0 | 位深→字节数换算与"位深不可得即 fail-closed" | 公式导出（换算）＋待确认（下界） | 换算式 bpp=bits/8 正确（FITS BITPIX 8/16/32/64 均 8 的倍数）；但**若 aio 报出非 8 倍位深**（XISF 允许 12-bit 等，`lib/infrastructure/aio/src/aio_xisf.cpp` 存在），整除向下取整 ⇒ 低估所需字节 ⇒ 截断文件可通过。方向＝放行 | 保守方向＝`bits_per_sample % 8 != 0` 即 fail-closed；影响范围＝非 FITS 输入的完整性门 | PARTIAL（未逐一读 `aio_xisf.cpp` 的 sample format 集合） |
| :1340,:1342,:1347 | `UINT64_MAX` 溢出保护 ×3 | — | w*h、pixels*bpp、header+data 的乘法/加法溢出检查 | 结构性不适用 | 溢出界，非科学量；三处方向均为 fail-closed（返回 false） | — | CONFIRMED |
| :1346 | 非 FITS 分支的 `header = 2880ull` | 2880 | 非 FITS 文件头长的"保守下界" | 待确认 | 注释 :1344 自述"单块保守下界, 与旧实现同口径"；条文面零提及（检索式：`git grep -n "2880" -- docs/contracts` 命中的都是 FITS 产品条款，不覆盖 XISF 输入头长）。XISF 主头通常远大于 2880 B，且压缩数据使 `size >= header+raw` 根本不成立 ⇒ 对非 FITS 输入此判据既不保守也不充分 | 保守方向＝非 FITS 输入不走该判据而改走 aio 自身的完整性标志；影响范围＝XISF 亮场的截断检测 | PARTIAL（现网 testdata 以 FITS 为主，未证实 XISF 路径可达） |
| :1364-1366,:1371,:1377-1391 | config 取值的类型回退分支（`p1_flag`/`p1_num`/`p1_int`） | — | 错型/缺键时回退调用方给的默认值 | 见 CR-240-15 | 待确认 | 保守方向＝错型即 PARAM 拒绝；影响范围＝P1 全部节点的全部数值/开关键 | CONFIRMED（行为已按原文核清） |
| :1380,:1385,:1389 | `INT64_C(-2147483648)`/`2147483647`/`UINT64_C(2147483647)` | int32 边界 | 宽整型收窄到 `int` 的界检查 | 结构性不适用 | 32 位 int 值域定义常数；越界回退 dflt（方向：回退值本身见 CR-240-15） | — | CONFIRMED |
| :1418 | `out = "frame"` | "frame" | frame_key 净化后为空/`.`/`..` 时的兜底目录名 | 待确认 | `ASTROCS_DESIGN §3.4`「输出基数」（:1401 引用）要求每帧一目录；兜底成同一个 `"frame"` 会把多帧并入一个目录。方向：**放行**——但同块的重复 key 由 :1428 的唯一性门拦下（该门自身有 CR-240-17 的缺口） | 保守方向＝空/退化 key 直接 fail-closed 具名报错，不给公共兜底名；影响范围＝文件名全为非白名单字符的输入（如中文/空格命名） | PARTIAL |
| :1423,:1499,:1507,:1519 | `doc.value("output_dir", std::string("."))`（4 处同值合并） | "." | output_dir 缺键时落到进程 CWD | 见 CR-240-14 | 待确认 | 保守方向＝缺 output_dir 即 PARAM 拒绝；影响范围＝所有 P1 节点产物落位 | CONFIRMED |
| :1454 | `static std::atomic<uint64_t> seq{0}` | 0 | 临时文件序号起点 | 结构性不适用 | 计数起点，非科学量 | — | CONFIRMED |
| :1484-1485 | `im.p->bits_per_sample = 32; im.p->float_sample = 1;` | 32, 1 | 写出前把样本格式归一为 FP32 | 见 CR-240-18 | 产品面登记 `scalar=f32`（`module_ports.registry.json` 的 `p1_calibrated`/`p1_cleaned`/`p1_photoapplied` 均 f32）⇒ 目标值本身有据；**改的方式与副作用**才是问题 | 保守方向＝经公共 setter 或新建句柄，不改 caller 句柄；影响范围＝所有 P1 像素产品 + 同一句柄的后续判据 | CONFIRMED |

## findings（格 5）

### CR-240-14 · 四个落位助手把 `output_dir` 的缺省兜底写成 `"."`，与配置合同"不得以进程 CWD 作隐式缺省"的条文直接相反
- 轴：C（主轴）/ D
- 位点：`:1423` `return doc.value("output_dir", std::string(".")) + "/" + p1_frame_key(light);`（`p1_frame_dir`）；同族 `:1499`（`p1_calibrated_path`）、`:1507`（`p1_cosmetic_path`）、`:1519`（`p1_badcol_path`）
- 上位依据：`eng/contracts/schemas/phase_config_normalize.schema.json` 的 `$defs.output_dir.description` 原文："运行产物唯一落点；**不得以进程 CWD 作隐式缺省（ASTROCS_DESIGN.md §6.3）**。多块形态下是块级必填"；同文件把 `output_dir` 列入 `$defs/normalize_block.required` 与 `else.required`（mosaic/export 两个 schema 同样各 3 处 required）。`AGENTS.md §6` 硬禁令"不把运行产物散落根目录：一切输出落 output_dir 或 `run/`"。
- 现状：CLI 面（schema）确实必填，但生产中枢这四个助手仍带 `"."` 兜底；一旦 `output_dir` 不在 doc 里（非 CLI 入口、或配置块被上层重排后丢键），四个函数一律返回 `"./calibrated_x.fits"` 一类路径，写盘与读回都照常进行。
- 差在哪：判据方向＝**放行**（缺键不报错，改用一个能跑通但落点错误的目录）。合同要求的形态是"缺 output_dir 即拒绝"，代码形态是"缺 output_dir 即 CWD"，两侧相反；而且这个默认值在配置合同面没有登记位（不是 profile/默认配置，是源码字面）。
- 后果：产品可能写进进程当前目录（服务上下文里即 DSH/守护进程工作目录），`run manifest` 与 `p1_products.json` 里的路径与实际落点仍自洽 ⇒ 交付时找不到产物或产物覆盖上一轮同名文件；`AGENTS §6` 的"产物不散落根目录"主张在此不成立。
- 定级：S2 — CLI 三命令路径上不可达（schema 必填），故未判 S1；非 CLI 入口（`lib/infrastructure/pipeline/orchestrator`、进程内 API 消费者、测试夹具直调 `p1_op_*`）可达，但本段未取证这些入口是否绕过 schema（属合并待办）。
- 怎么算修好：①四处改为"缺 output_dir ⇒ `Result::fail(ErrorDomain::DATA, "output_dir missing")`"（与 :1428 门的 fail-closed 口径一致），或至少统一走一个 `p1_output_dir(doc, &err)` 助手；②同批把该必填性从 schema 侧延伸到节点侧（补一条负例：直调 `p1_op_*` 且 config 无 output_dir 必须判红），否则改了代码没有门守住。
- 置信：CONFIRMED（四处原文同值；合同描述句逐字引用）

### CR-240-15 · 节点 run 面对错型配置一律"回退默认、不抛"，而 P1 节点路径上没有任何 validate 面在拦错型 ⇒ 带错类型的键会被当作默认科学参数使用
- 轴：R（主轴）/ D
- 位点：`:1354-1355` 注释＋`:1360-1393` 三个助手
  ```
  // config 值读取（对齐 p1_session validate 合同: number|bool 均合法; 错型回退默认,
  // 不抛——validate 面拒绝合同外结构, run 面不因错型 terminate）
  bool p1_flag(...) { ... return dflt; }        // :1367 非 bool/number 一律回退
  double p1_num(...) { if (... || !it->is_number()) return dflt; }   // :1371
  int p1_int(...)    { ... return dflt; }       // :1376/:1381/:1385/:1392
  ```
- 上位依据：`docs/contracts/PUBLIC_API.md:721`「`p1_session_validate` …… config 键集校验（DATA-P1-SESSION §16.1）；**缺必需键/类型错→PARAM，无 silent default**」；`docs/contracts/RT-001.md:31-32`「IModule 生命周期：describe→validate_config→plan→create→execute→inspect→destroy」。
- 现状：注释把"拒绝错型"的责任交给 validate 面；而本批已查明 ①`IModule::validate_config` 在全仓**无调用者**（`git grep -rn -- "->validate_config(\|->plan(\|->execute(" -- lib` ⇒ 生产只有 `runtime.cpp:187/257/284` 的 plan/execute/last_manifest），②`SessionModule::execute` 会调 `fn_validate`（:631），但 P1 八个真实节点走的是 `P1NodeModule`（:15461 起），其 execute 路径直接 `Json::parse(config_)` 后用 :1360-1393 这套助手取值。⇒ 节点路径上"validate 面拒绝合同外结构"这一前提不成立，错型只剩 CLI 的 JSON Schema 一道拦。
- 差在哪：判据方向＝**放行**，且方向可判定：`p1_flag(c,"x",true)` 遇到 `"x":"false"`（字符串）走 :1367 `return dflt` ⇒ 结果是 **true**，即"用户写了关闭、系统按开启算"；`p1_num` 遇 `"3e5"`（字符串）走 :1371 回退默认阈值。缺键与错型被编码成同一个返回值，合同要求的 PARAM 退出码无处产生。
- 后果：任一 P1 科学开关/阈值（测光是否施加、饱和处理、拟合并数、SNR 门限等——具体键在 :1500 之后的各 `p1_op_*` 里）在配置写成字符串型时静默取默认值，产品照常发布且 manifest 不记降级 ⇒ "配置忠实性"这一对外主张在节点路径上不成立。这也是本仓已实证过的"同一键多侧默认＝fail-open"母题在**类型维度**的实例。
- 定级：S2（键名与各自 dflt 的逐条取证在 :1500 之后，属其它段；要判 S1 需给出某个 dflt 与科学默认值不一致的具体键，**须与 CR-240…CR-250 合并定案**）
- 怎么算修好：①三助手增加"键在位但类型不合 ⇒ 返回失败"的通路（例如 `p1_num_checked(c,key,dflt,&ok)`），节点侧对 `!ok` 走 `ErrorDomain::DATA`（CLI→2/3）；②把 `PUBLIC_API.md:721` 的"无 silent default"从会话面扩到节点面，并在 `docs/contracts/CONFIG_CONTRACT.md` 登记"错型＝PARAM"；③同批补机器门：对 `phase_config_*.schema.json` 的 `propertyNames` 与各 `p1_op_*` 里读取的键做集合比对，出现"代码读、schema 不收"的键即判红（否则改完只是把洞挪个位置）。
- 置信：PARTIAL（助手行为已 CONFIRMED；受影响的键清单属其它段）

### CR-240-16 · 饱和电平读取把"文件读不出"与"元数据缺失"压成同一个 `0.0`，噪声/饱和域的降级声明因此可能说谎
- 轴：R（主轴）/ C
- 位点：`:1290-1330`，关键三行：`:1293` `if (!aio_file::read_head(path.c_str(), 100ull * 2880ull, &blob)) return 0.0;`、`:1317` `return 0.0;  // 主头无 END → fail-closed`（函数内）、`:1329` `return datamax;`
- 上位依据：函数自己的注释 `:1286-1289`："SCI NOISE_MODEL §4「饱和域」(claim SC-008) 要求：未提供电平时调用方**必须**在帧产品写显式降级声明（禁止静默）。来源优先级 = cfg > SATURATE > DATAMAX，与 `snr_estimator.h:134-136` 的声明一致。返回 >0 = 有效电平（ADU）；**0 = 未提供**（调用方须写 `DISABLED_NO_METADATA`）"。
- 现状：`0.0` 同时表示四种互不相同的事实——①`read_head` 失败（IO 错/文件不存在/权限）；②文件不是 FITS 或首块无 `END`（结构非法）；③确有 `END` 但既无 SATURATE 也无 DATAMAX（真正的"元数据缺失"）；④SATURATE/DATAMAX 在位但值非法（非有限/非正/字符串卡，:1304-1305 丢弃）。四种全部落到同一个"未提供"编码，调用方据注释写 `DISABLED_NO_METADATA`。
- 差在哪：合同要求声明的语义是"元数据里没有电平"（③），代码在①②④时也声明成③。方向＝**放行**：结构非法/读不出的帧不会因"饱和信息不可得"判红，而是带一条内容不真的降级声明继续产出。注意与 `:1282` 的"主头无 END → fail-closed"不矛盾——那里的 fail-closed 只作用于 `p1_fits_primary_header_bytes`（被 `p1_image_sane` 消费），本函数对同一条件只回 0.0。
- 后果：`SCI NOISE_MODEL §4` 的"饱和域显式降级"声明在部分帧上是错的（真因是 IO/结构问题），下游按 `DISABLED_NO_METADATA` 统计饱和处理覆盖率的面板被污染；且④情形下"写错值的 SATURATE 卡"（如负数、字符串）与"没有该卡"不可分，掩盖头写坏这类真实缺陷。
- 定级：S3（不改数值路径本身：0 一律走"未提供"分支；错的是声明内容）
- 怎么算修好：返回三态（`>0` 有效 / `0` 确无卡 / `-1` 读不出或结构非法）并让调用方对 `-1` 走 `ErrorDomain::IO`（CLI→4）；或在 manifest 里同时写 `saturation_source ∈ {card, absent, unreadable}`。改时须同步 `sci` 面的声明词汇（`docs/science/NOISE_MODEL.md` §4 的降级枚举），否则科学面与实现面对不上。
- 置信：PARTIAL（调用点如何用 0.0 写 `DISABLED_NO_METADATA` 在 :1500 之后，属其它段）

### CR-240-17 · frame_key 唯一性门在 `input_lights` 缺键/非数组/元素非字符串三种形态下直接返回"成功"
- 轴：R
- 位点：`:1428-1441`
  ```
  Result<void> p1_require_unique_frame_keys(const Json& doc) {
    if (!p1_has(doc, "input_lights") || !doc["input_lights"].is_array())
      return Result<void>::success();
    std::set<std::string> seen;
    for (const auto& l : doc["input_lights"]) {
      if (!l.is_string()) continue;
  ```
- 上位依据：`:1401-1404` 自述"P0-21: 一组进一组出（ASTROCS_DESIGN §3.4「输出基数」）……同块内重复 key ⇒ fail-closed（两帧共用目录会互相覆盖产品与中间产物, 属'静默丢弃'）"；`:1426-1427`"任何按帧派生落位的操作器（wcs/drizzle/writer）必须先过此门"。
- 现状：门只覆盖"字符串元素且 key 相同"这一种冲突。三种形态整体放行：①`input_lights` 缺键或非数组 ⇒ 立即 `success()`；②数组里**非字符串**元素被 `continue` 跳过，不参与查重（两帧以对象形式给出、指向同一基名时门看不见）；③查重按 `p1_frame_key()` 之后的名字，而 `:1418` 的兜底 `"frame"` 会把多个"净化后为空/./.."的输入并成同一个 key——这一支门**能**拦（值相同），但拦下后报的也是 collision，而非"退化名"这一真因。
- 差在哪：判据方向＝放行（门的"通过"包含"我什么都没查"）。合同语句是"必须先过此门"，而门自身可在缺结构时自证通过。
- 后果：`§3.4 输出基数`主张（每帧一个 HiPS 目录）在畸形配置下失去守卫；非字符串元素路径还会与后续各 `p1_op_*` 自己的 `is_string()` 判定耦合——那些判定若也 `continue`，则该帧被**静默跳过**（产品数少于输入数而运行报成功），这一点在 :1500 之后，须合并定案。
- 定级：S3（本段范围内门自身的行为已核清；错值需读各 op 的循环，属其它段）
- 怎么算修好：`input_lights` 缺失/非数组/元素非字符串一律 `fail(DATA, "input_lights malformed")`（与各 op 的入参要求一致），并把"key 退化到兜底名"作为独立错误报出（区别于 collision）；同批补一条负例：两帧以非字符串给出必须判红。
- 置信：CONFIRMED（门自身逻辑逐行核过；跨 op 部分明确标为待合并）

### CR-240-18 · P1 像素产品发布序缺 `fsync(fd)`，与冻结发布序"close → fsync(fd) → rename"相反，且 `fsync_parent_dir` 失败完全不可见
- 轴：R（主轴）/ D
- 位点：`:1460-1474`（`p1_atomic_publish`）＋`:1481-1495`（`p1_write_fits_atomic`）
  ```
  if (!aio_fs::rename_replace(staging, final_path)) { ... return false; }
  aio_atomic::fsync_parent_dir(final_path);
  return true;
  ```
  与
  ```
  if (im.ok() && im.p != nullptr) { im.p->bits_per_sample = 32; im.p->float_sample = 1; }
  const std::string staging = p1_staging_path(final_path);
  if (aio_write_fits(im.p, staging.c_str()) != 0) { ... }
  if (!p1_atomic_publish(staging, final_path, err)) return false;
  ```
- 上位依据：`ASTROCS_DESIGN.md:688`「所有产品（含 HiPS tile）走：本次运行私有临时区 → 校验 → **fsync** → 算哈希 → 原子改名发布 → 最后落完成清单；……正式目录只出现完整产品」；`docs/contracts/PUBLIC_API.md:1914`「→ close → **fsync(fd)** → rename（R10-C 冻结序，:221-273）」；`docs/contracts/DATA_SEMANTICS.md:2413`「发布协议冻结（R10-C）：fits_flush_file → close → fsync(fd) → …」。同仓另一面已按此实现：`lib/infrastructure/aio/src/aio_atomic_file.h:102-148` 的 `write_file_atomic` 顺序是 fwrite → fflush → **fsync(fileno)** → fclose → `atomic_replace`。
- 现状：FITS 产品走的是本文件自建的"临时文件 + rename"路径：`aio_write_fits` 把 staging 写完关闭（`lib/infrastructure/aio/src/aio_fits.cpp` 的写路径**无 fsync**，`git grep -n "fsync" -- lib/infrastructure/aio/src/aio_api.cpp` 亦零命中），随后立刻 rename，rename 之后才 `fsync_parent_dir`（只同步目录项，且 `aio_atomic_file.h:605` 是 `void` 函数、内部 `(void)fsync_path(...)` 连返回码都不出）。aio 另有同语义且带 fsync 的流式原语 `write_file_atomic_stream`（`aio_atomic_file.h:149-197`，注释："与 write_file_atomic 同语义, 供内容由多次 fprintf/fwrite 生成的场景使用"），FITS 面没用。
- 差在哪：冻结序要求"数据先落盘再改名"，这里是"改名后才碰目录"。后果方向＝**放行**：崩溃/断电窗口内 `final_path` 可以是一个已改名但数据页未落盘的文件（长度对、内容为 0 或部分），而登记面（:1493 之后各 op 的 manifest/产物清单）仍把它当完整产品。`fsync_parent_dir` 的失败又完全不进 `err`（与 :1468 的 `*err = "atomic publish failed"` 形成对照）。同族小洞：`:1469` `aio_fs::remove(staging)` 的返回值被吞（`aio_fs::remove` 本身是 `void`，:190-192），清理失败不可见。
- 后果：`ASTROCS_DESIGN §688` 的"正式目录只出现完整产品"、`:533` 的"收尾 fsync + 算哈希 + 原子发布"对**像素产品**不成立；HiPS/FITS 产品的 `sha256` 登记可能在重启后与磁盘内容不一致。JSON 侧（`aio_fs::write_atomic`）反而正确 ⇒ 同一运行里两类产品的发布保证不同级。
- 定级：**S1**，触发路径逐跳：`lib/infrastructure/cli/commands.cpp:1721`（normalize）→ `lib/infrastructure/scheduler/src/runtime.cpp:284`（`m.value()->execute(ctx)`）→ `module_adapters.cpp:15461-15468`（`p1_nodes[]` 的 cal/cos 等真实节点）→ `module_adapters.cpp:2995`（`p1_write_fits_atomic(im, out_path, &werr)`，cosmetic 产物；另 :2596/:3017/:3033/:6084）→ `module_adapters.cpp:1488`（`aio_write_fits`，无 fsync）→ `module_adapters.cpp:1467`（rename）→ `module_adapters.cpp:1472`（只同步目录）。其中 :2995/:15461 属本文件其它行段（CR-24x），本条以逐行引用取证；若其上下文另有 fsync，**须与 CR-240…CR-250 合并定案**后调级。
- 怎么算修好：①`p1_write_fits_atomic` 改为"aio 侧提供 `aio_write_fits_fsync`（写完 close 后 `fsync(fd)` 再返回）"或让本函数走 `aio_atomic::write_file_atomic_stream` 同语义序；②`p1_atomic_publish` 增加 `bool fsynced_data` 前置条件并在失败时填 `*err`；③`fsync_parent_dir` 改为返回 int（aio 面）并把失败并进产品错误；④同批给发布序补一条可执行判据（现有门只看 `data > UINT64_MAX - header` 一类，不看 fsync），例如按 `strace`/ETW 断言 rename 前有 fsync 调用——否则"改对反而判红"的风险在别处（没有门守这条）。
- 置信：CONFIRMED（三处冻结序条文逐字引用；aio 写路径 fsync 缺失以检索式确认：`git grep -n "fsync" -- lib/infrastructure/aio/src/aio_fits.cpp lib/infrastructure/aio/src/aio_api.cpp` 仅 `aio_atomic_file.h` 命中）

### CR-240-19 · 写出前用 `const` 引用改调用方句柄内部字段：aio 私有结构被生产中枢直接写入，且该改动能反噬同一句柄的完整性判据
- 轴：D（主轴）/ R
- 位点：`:1483-1486`（`im.p->bits_per_sample = 32; im.p->float_sample = 1;`）＋ `:67`（`#include "aio_fits.h"  // AIOImageData 完整布局: 写出前归一化样本格式为 FP32`）
- 上位依据：`ASTROCS_DESIGN.md §10`「aio 是文件级唯一 I/O 边界：任何文件读写经 aio」（本文件 :170-172 据此建 `aio_fs` 薄转发层，并声明"机制唯一实现在 lib/infrastructure/aio/src/**"）；aio 公共面 `lib/infrastructure/aio/include/astro_image_io.h:117-128` 只提供 `aio_get_options`/`aio_get_metadata` 等**只读**访问器与 `aio_free_image_data`，**没有任何样本格式 setter**；`AIOImageData` 的完整定义在私有头 `lib/infrastructure/aio/src/aio_fits.h:15-28`。
- 现状：本文件包含 aio 的 **src 私有头**，直接改 `AIOImageData` 的两个 int 字段，作用对象是 caller 持有的句柄（`p1_write_fits_atomic` 形参是 `const P1Image&`，`const` 只约束指针本身，句柄内容被就地改了）。修的方向是对的——`aio_fits.cpp:1109` 的写出分支确实是 `image->bits_per_sample > 0 && !image->float_sample ? 16 : -32`，所以改成 32/1 才会写 BITPIX=-32；我复算过该三元式与 :1477-1480 记述的旧缺陷（int16 回绕、cal 产物与源相关性 0.065）一致。
- 差在哪：两点。①边界方向倒了：为了让写出面用对格式，改的是**上游模块的内部表示**，且经私有头，等于把 aio 的结构体字段名变成 scheduler 的编译期依赖（aio 侧同一 struct 里已经并存第二套表示 `uint8_t dtype`，0=FP32/1=FP64，:1056 起的写路径正是用 `dtype` 拒绝 FP64 ⇒ 两套字段的职责边界只体现在 aio 的私有实现里）。②副作用可回吃：同一句柄若在某次写之后又被 `p1_image_sane(im, 源路径)` 判完整性（:1336 `bpp = bits_per_sample/8`），位深已被改成 4 B/px，而源文件是 2 B/px ⇒ `need` 翻倍 ⇒ 合法文件被判"不可读"。方向＝保守（误判为红），但误判原因是被自己改过的字段。是否真的发生取决于调用序，均在 :1500 之后 ⇒ **须与 CR-240…CR-250 合并定案**。
- 后果：短期无错值；结构性后果是 aio 的"唯一 I/O 边界"变成"唯一 I/O 边界＋可被外部改写的内部状态"，任何 aio 侧重构（把格式决策收敛到 `dtype` 或加 setter）都可能让 :1484-1485 静默失效，退化回"按源格式写 int16 并回绕"的损坏产品，而这条退化没有任何编译期或门检。
- 定级：S2（无现网错值证据，但缺陷类属"修复静默失效即产出损坏产品"）
- 怎么算修好：①aio 公共面加 setter（`aio_set_sample_format(image, bits, is_float)` 或 `aio_image_to_fp32(image)`，内部保证 `bits_per_sample`/`float_sample`/`dtype` 三字段一致）；②本文件删 `:67` 的私有头包含与 :1484-1485 的裸字段写；③`p1_write_fits_atomic` 形参改非 const 或明确文档化"本函数会归一化句柄格式"，并把 `p1_image_sane` 对 `bits_per_sample` 的依赖改成"读入时快照"（在 :1332-1351 里就把 bpp 记进返回值结构，别让后续读到被改过的值）。
- 置信：CONFIRMED（字段写入与三元式判据逐字核过；句柄复用序属其它段，已标合并）

## 本格"核过但不报"的项与检索式

- `:1201-1216` `P2Api`/`P3Api`：与 `:15447/:15455` 的注册配对（本批以 grep 取证），委托签名与 `p2_session.h`/`p3_session.h` 一致 ⇒ 不报"死壳"（与 P1Api 的区别就在这里，见 CR-240-13）。
- `:1226-1247` `P1Image`：拷贝删除、移动释放旧指针、析构 `aio_free_image_data` ⇒ RAII 正确；`ok()`/`px()` 对 `data_f64`（FP64 图）会返回 `nullptr`，但 `p1_image_sane` :1333 正当地把 `px()==nullptr` 判为不可用（FP64 源在 P1 像素面被拒），方向＝保守，不报。
- `:1249-1251` `p1_read_image` 直接包 `aio_read`，失败返回空句柄并由 :1333 拦 ⇒ 不报"未检查返回值"。
- `:1262-1267` `p1_is_fits_file` 只比 6 字节魔数（不比 `"SIMPLE  ="` 全卡）：误接受需文件名以 `SIMPLE` 开头的非 FITS，实践不可达，且误接受方向是让头长判据走"非 FITS 分支"（下界更小）⇒ 归入 :1346 一行的 `待确认`，不单列。
- `:1269-1283` `p1_fits_primary_header_bytes` 的块扫描边界：我核过 `have_blocks = blob.size()/2880` 向下取整 ⇒ 最大偏移 `(have_blocks-1)*2880 + 35*80 + 80 = have_blocks*2880 ≤ blob.size()`，不越界；`strncmp(c,"END",3)` 与 `c[3]` 亦在界内 ⇒ 不报越界读。
- `:1296-1308` `parse_card`：取值窗 `c+10..c+79` 在 80 字节卡内；`strtod` 的 `end==val` 判据正确拦掉逻辑卡 `T/F` 与字符串卡；`val[70]='\0'` 与 `char val[71]` 匹配 ⇒ 不报缓冲问题。
- `:1332-1351` `p1_image_sane` 的三处 `UINT64_MAX` 溢出检查与"头/数据分段"逻辑对 FITS 是正确的（`need = header + w*h*bpp`）；其问题只在非 FITS 下界与位深非 8 倍数两例，已入表。
- `:1356-1359` `p1_has`（键在位且非 null）与 `:1396-1399` `p1_base_name`、`:1405-1420` `p1_frame_key` 的字符白名单（alnum `_ - .`，其余替换为 `_`）与 `.`/`..`/空串的退化处理符合 :1401-1404 声明的意图 ⇒ 不报路径注入（HiPS 目录名不会含分隔符）。
- `:1453-1458` `p1_staging_path` 的 `pid + 进程内 atomic 序号`唯一性：同机不同进程靠 pid 区分、同进程靠 seq ⇒ 不报临时文件互踩。
- `:1460-1466` 的注释删除"先删目标再 rename"兜底并引 `aio_atomic_file.h` 冻结禁令：与本文件 `:193-195` `rename_replace` 走 `MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)`/`rename(2)` 的实现一致 ⇒ 不报"跨文件系统 rename"（staging 与目标同目录，:1476 注释明说）。
- 本段无新增科学常数/阈值/容差：全部数值是 FITS 格式定义常数（2880/80/36/值区偏移）、溢出界与类型边界，唯一带科学语义的目标值 :1484 的 `32`（FP32 产品格式）已在 §2.5 表中给出产品面依据（注册表 `scalar=f32`）。
"""

with io.open(p, "a", encoding="utf-8") as f:
    f.write(body)
s = io.open(p, encoding="utf-8").read()
s = s.replace("<!-- PROGRESS: 4/5 -->", "<!-- PROGRESS: 5/5 -->")
s = s.replace("5. `lib/infrastructure/scheduler/src/module_adapters.cpp:1201-1500` —— 已读：否",
              "5. `lib/infrastructure/scheduler/src/module_adapters.cpp:1201-1500` —— 已读：是（表 16 项，findings 6 条）")
io.open(p, "w", encoding="utf-8").write(s)
print("cell5 appended, progress -> 5/5")
