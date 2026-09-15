# E15 门规格：`AST-UNUSED-DECL`（一棵 AST 树覆盖「形参零引用」+「字段只写不读」+「读键无产键」）
来源：V20 轴 §5（终报）。状态：**待你裁后转工单**。本文件是我方施工口径，不是账本条目。

## 注册四件套（缺一即成新的「门注册与门脚本分裂」`W1-N-08`）
- `ci/checks.json` 新增 `AST-UNUSED-DECL`：`waivable=false`、`mutates_workspace=false`、`heavy=false`、三 profile；`command` = `python3 -B tools/quality/check_declared_but_unused.py --format github`；**必须同步 `changed_paths` 与 `prerequisite_tools`**（后者按 `A-28`/`W6-N-02` 现况：该脚本若走 libclang 需声明，否则又是一道未声明宿主工具的门）。骨架复用 `tools/check_serial_hardcode.py:34` 的词表 + 豁免结构。
- 脚本落 `tools/quality/`（受跟踪），且 `ci/tests` 侧须有一道**真被采集**的自锁（否则复现 `F-7` 有判据无载体）。

## 四条规则与起判级别
- **R1 形参零引用**：出参型（`T*`/`&` 语义为输出）UNREF ⇒ **ERROR**（本轴命中 2：`build_out_manifest` 的 `err`，函数恒 `return ACS_OK` 而出参签名承诺可报错）；`(void)p` ⇒ 归 `DELIBERATE` 单列不计（现 41 处）。
- **R2 字段只写不读**：读侧集为空 ⇒ WRITE_ONLY ⇒ **ERROR**（本轴 18 字段一次收口）；带 `/* ACS-ECHO: 理由 */` 才降 WARN。
- **R3 声明→读者可达**：含「**读键 ⊄ 上游产物键集**」⇒ **ERROR**（本轴命中 2：drizzle `reverse` 四键、`p3_verify.json` 的 `module_build_id`）。
- **R4 三态分档**：`ECHO-ONLY` ⇒ WARN；**`ECHO-FALSE` ⇒ ERROR**（回显写出的值**不数据依赖于该声明**＝常量冒充用户值，如 hips plan 的 `"max_workers":1`）——这是「仅回显」里最危险的一档。

## 必内置的假象豁免表（否则首跑被噪声淹没）
宏位置初始化（`ACS_KERNEL_ENTRY`/`AIO_ABI_INFO_V1_INIT`/`backend_table.inc`）、designated-init、`memset` 后逐字段写、整体拷贝（`cfg = *cfg_in`）、构造器初始化列表、unnamed 形参、`= nullptr` 默认实参、`(void)` 无参函数、模板参数、多行折行签名、**同名不同实体**（如 acr 测试里的 `build_curve`）。
- 依据：本轴「有读无写」候选 **28 条全为上述假象** ⇒ **未初始化读结论为零**。豁免表不是可选项。

## 区分回显与真消费（一句话 + 三条件）
- **看该声明的值是否成为某终端表达式的操作数。**
- 回显即消费假象（三条同时成立）：①出现点全在写出集合（`man[k]=`／artifact `{k,v}`／properties KV／`fprintf(stderr)`／logger／plan 字面量）；②全仓无第二读者（**`fs::exists` 类存在性断言不算读者**）；③写出表达式不进入任何算术或比较。
- **放行反例（既回显又真消费，勿误判）**：`reject_profile`、psf 的 `n_fit_input`/`psf_fit_truncated`。

## 配套三件事（不配套就是白装一道门）
1. 修 R2/R3 时**同步补产物判别位**：`snr_max_sources_given`、`p3_verify.json.module_build_id` 的非空断言（否则字段修完仍无质量位）。
2. 把 `test_monitor_events.py::test_04` 的「键存在」断言升级为**值/点数**断言：`0 < len(curve_points) <= downsample_max`（现状是自注 `only a marker array (empty)` ⇒ 把空壳钉成规格）。
3. 把**六个科学 DLL 纳入根 `CMakeLists.txt:646-657` 的 `-Wall -Wextra` 白名单**（现仅 `-fopenmp`）⇒ 否则 R1 的四条真零引用永远没有编译期兜底。

## 重发片新增的免重报锚（并入 §3，不改变任何判级）
- `wcs.init_source` **三来源均在 `:2231-2270` 读、`:2347-2368` 真入 ipv** ⇒ 非只回显；cosmetic 五键 `:1523-1537` 读 + `:1551` 入参；`phase3.sampler`/`parity`/`bitpix`/`projection` 消费；跨节点键另含 `files.accepted`。
- `reject_profile` 的完整链补一环：**`stage2_common.cpp:238-243` 先把 `wbpp_current` 归一为 `wbpp_2_9_1` 再拒其余** ⇒ 「默认值即非法」这一族指控在本键上不成立（可作 `C-20` 类判据的正对照模板：先归一再判域）。
- CLI 选项读点已逐名给号（`commands.cpp:108`/`:132-151`/`:150`/`:1510`/`:1567-1568`/`:1815`/`:2230-2231`/`:2373`/`:2395-2396`/`:2566`）⇒ 若他轴要报 CLI 参数零消费，**须先排除这 11 项**。

## 需运行期或未判的边界（不得由本门代判）
HISS metadata/props 的 `signal_dtype` 键配对（并发已把它移出 `module_adapters.cpp`，静态无法判生产者/消费者）；`reverse` 键是否应覆盖 manifest（**属负责人设计意图，准绳①**）；hips/snr 租约 detail 105 实际触发；snr TU 接线后须复跑；raw 曲线可用性；`upm.cpp:1295-1306` 半初始化脆弱点；`P2002_PROBE` 写侧 env。

## 判否负清单（19 条择要，**他轴免重报**）
`dark_scale_factor` 两分支都真消费（`:1457-1462` 入参／`:1446-1453` 参与 EXPTIME 一致性判定）；cosmetic 五键入参（`:1551`）；`psf.max_stars`（`:1805`/`:1809`）；drizzle `nside`/`nested`/`pixfrac`/`precision_mode`（`:2917-2988` 校验 + `:3190` 入引擎 + `:3211` 回显）；upm 四键 + `__workers`（`:3729-3756`→`upm.cpp:237-246` clamp 及多站）；**`P2UpmBuildConfig` 16 字段读写双方齐备**；`reject_profile`（`:4020`→`rejection.cpp:1039-1043`；**`wbpp_current` 是 `wbpp_2_9_1` 的 migration alias，非「默认即非法」**）；`tile_leaf_span`/`sample_mask_offset`/`depth`/`frame_slots`/`union_cells` 跨节点键配对齐全；`~0ull` 哨兵用法正确；snr 九键缺省有判别位（`:2717` nullptr／`:2772-2773` status+reason／`:2799` se_status）⇒ 判否；CLI 11 选项 + 5 环境键全有读者；`async_io_depth` 生产唯一调用点恒传 0 但已三重登记 ⇒ 不重报；撤销 3 类扫描器假阳性（`snr out_json`、`drz_rows_free data`、`fits_core fp`）。
- **现态订正（勿按旧口径重报）**：`--gate-workers`/`--gate-required` 已被 `tools/monitoring/run_monitored.py:714-725` 参与 `allocated` 计算 ⇒ 「零应用点」一半失效（`M5a-G-003`/`V21-N-15` 的两个硬编码常量仍在，不受影响）。
