import io, sys
sys.stdout.reconfigure(encoding='utf-8')

p = r"独立审计/证据/通读-CR-240.md"

body = r"""
---

# 格 4 · `module_adapters.cpp:901-1200`

本格内容 = P1 剩余 descriptor（photometry/noise-snr/drizzle/writer）＋ P2 七节点 descriptor（:1046-1184）＋ `make_session_module`/`P1Api`（:1187-1200）。

## §2.5 数值处置表（格 4，8 项）

| 位点(文件:行) | 符号或键 | 现行值 | 它是什么（一句话用途） | 处置 | 依据锚 | 待确认时给保守方向与影响范围 | 置信 |
|---|---|---|---|---|---|---|---|
| :920,:959,:987,:1025,:1049,:1068,:1087,:1106,:1126,:1145,:1164（＋格 3 的 :695,:714,:733,:756,:775,:794,:813,:832,:851,:871,:898，同族同值合并） | `d.version` | "1.0.0" | 模块登记版本 | 待确认 | 同格 3 该行：与 `ASTROCS_VERSION_STRING`（CI 夹具实为 `0.11.0-alpha.2`，`eng/ci/fixtures/run_manifest/real_manifest_sample.json:56-59`）互斥；唯一读取点 `module.cpp:163` 的 `export_index_json` 在生产面无调用者 | 保守方向＝删或改取构建版；影响范围＝仅登记面 | CONFIRMED |
| :1054,:1112 | `DATA-P2-CAL` 端口的 `UnitId` | SURFACE_BRIGHTNESS | P2 入端像素面单位登记 | 待确认 | 见 CR-240-10：`docs/contracts/PUBLIC_API.md:1137`、`docs/modules/phase2_upm.md:108`、`docs/modules/registry/astrocs.phase2.upm-apply.md:72` 三侧均登记 **ADU/PIXEL**；注册表该模块根本没有 `DATA-P2-CAL` 端口（用 `frame_hips`/DATA-HIPS-001/SURFACE_BRIGHTNESS/**HEALPIX**） | 保守方向＝按 §31.1（signal 面 `ADU/sr`）取 SB 并同批改三侧文档＋把 coordinate 改 HEALPIX；影响范围＝P2 全链单位判据 | PARTIAL（"哪一侧为科学正本"依 §31.1/§5.6 推定，未做产品字节复算） |
| :1054,:1074,:1093,:1111-1113,:1132,:1152,:1175-1176 | `CoordinateFrame::PIXEL`（P2 端口坐标系登记，合并） | PIXEL | 把球面 NESTED tile 产品登记成平面像素域 | 待确认 | `docs/contracts/DATA_SEMANTICS.md:1336-1344` 原文："registry descriptor p2_write_descriptor … mosaic=DATA-P2-RES out（UnitId::ADU/CoordinateFrame::PIXEL，:684-687）为编排层词汇，与球面 NESTED 马赛克实际不符（产品为 NESTED 球面 tile，非 PIXEL 平面），以本节为准修订，P2-XX-INT 对齐"；注册表 `mosaic_hips` 登记 HEALPIX | 保守方向＝球面载体端口一律 HEALPIX；影响范围＝`pipeline.cpp:291-294` 的 COORDINATE_MISMATCH 判据（现无豁免，两侧同错则永不红） | CONFIRMED（条文原文已逐字引） |
| :720 与 :1176（同文件两处，跨格对照） | `DATA-P2-RES` 端口的 `UnitId` | ADU（:720）／SURFACE_BRIGHTNESS（:1176） | 同一 schema ID 在本文件内两种单位 | 见 CR-240-10 | 待确认 | 保守方向＝取 §31.1 的 SB；影响范围＝凡按 descriptor 判单位的代码/文档 | CONFIRMED |
| :1012,:1030-1031 | `DATA-P1-STACK`/`DATA-P1-FITS` 端口单位 | SURFACE_BRIGHTNESS＋**ICRS** | P1 栈与 P1 FITS 端口的量纲/坐标系登记 | 待确认 | 注册表 `p1_stack`/`frame_hips` 登记 SURFACE_BRIGHTNESS＋（`astrocs.phase1.writer` 的 `frame_hips`/`p1_final` 亦 SB）；但 coordinate 用 `ICRS` 表示球面产品，与 P3 侧同一族产品用 `HEALPIX`（:739、注册表 `mosaic_hips`）不同口径 | 保守方向＝球面产品统一 HEALPIX；影响范围＝跨阶段坐标登记一致性（产品 BUNIT 不受影响，走节点 manifest 的 `bunit` 字面） | PARTIAL |
| :939,:973 | `DATA-P1-FLUX` 端口单位 | ADU＋ICRS | 测客流量的量纲登记 | 待确认 | 注册表 `p1_flux` 亦 ADU/ICRS ⇒ 两面一致；但"测光校准到星等坐标系"（本仓创新点一，`ASTROCS_DESIGN §2`/AGENTS §2）下对外产品量是星等，端口面只登记 ADU 而星等由 provenance 承载——本段无星等 token | 保守方向＝保持 ADU（原始计数面）并在合同面写明星等在何处；影响范围＝P1 测光端口语义 | PARTIAL（未读到 `p1_op_photometry` 的写出面，属 CR-24x） |
| :1196 | `p1_session_run(h, c, 0)` 的 `async_io_depth` | 0 | 会话预读深度钉死为"不预读" | 待确认 | `docs/contracts/PUBLIC_API.md:722`「async_io_depth∈{0,1,2}（:152）」；`docs/traceability/TRACEABILITY_MATRIX.csv:10` 自登记"async_io_depth∈{0,1,2} 强校验(:152)但预读未按 depth 启用"。**且该调用点在零引用的 `P1Api` 内**（见 CR-240-13） | 保守方向＝0（不预读）已是安全侧；影响范围＝仅 Phase1 会话路径（现生产不走） | CONFIRMED |
| :1193-1200 | `struct P1Api` 五函数适配器 | — | Phase1 会话的五函数委托壳 | 见 CR-240-13 | 待确认 | 保守方向＝删除或改注"仅历史保留"；影响范围＝四份合同/追溯页的现行委托陈述 | CONFIRMED |

## findings（格 4）

### CR-240-10 · P2 端口的单位与坐标系登记与两份合同/注册表三侧互斥，且同一 schema 在本文件内就被登记成两种单位
- 轴：C（主轴）/ R
- 位点（逐字）：
  - `:1054` `{"calibrated", "DATA-P2-CAL", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},`
  - `:1112` `{"calibrated_frames", "DATA-P2-CAL", true, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},`
  - `:1176` `{"mosaic", "DATA-P2-RES", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::PIXEL},`
  - 同文件对照 `:720`（格 3）`{"resampled", "DATA-P2-RES", false, UnitId::ADU, CoordinateFrame::PIXEL},`
- 上位依据（三侧原文）：
  1. `docs/contracts/DATA_SEMANTICS.md:1336-1344`："registry descriptor p2_write_descriptor（…module_id="astrocs.phase2.write"）端口表 integrated=DATA-P2-INT in / mosaic=DATA-P2-RES out（UnitId::ADU/CoordinateFrame::PIXEL，:684-687）为编排层词汇，与球面 NESTED 马赛克实际不符（产品为 NESTED 球面 tile，非 PIXEL 平面），以本节为准修订，P2-XX-INT 对齐"。
  2. `docs/contracts/PUBLIC_API.md:1137`："端口 calibrated=DATA-P2-CAL/**ADU**/PIXEL→coverage=DATA-P2-COV/…"；`docs/modules/phase2_upm.md:108` 与 `docs/modules/registry/astrocs.phase2.upm-apply.md:72` 端口表同写 `calibrated_frames | DATA-P2-CAL | 必 | UnitId::ADU | CoordinateFrame::PIXEL`。
  3. `lib/infrastructure/pipeline/module_ports.registry.json`（自称"单一真源"，`frozen_at=2026-09-02`）里 `astrocs.phase2.coverage` 的输入端口是 `frame_hips`／`DATA-HIPS-001`／`SURFACE_BRIGHTNESS`／**`HEALPIX`**（`carrier=hips_product_tree`，artifacts=`<frame_key>/signal|support|properties`），并且**整个注册表没有 `DATA-P2-CAL` 这个 schema**；`astrocs.phase2.write` 的输出是 `mosaic_hips`／DATA-HIPS-001／SB／HEALPIX ＋ `p2_final`／DATA-P2-RES／SB／PIXEL。
- 现状：descriptor 侧对同一件事给三种说法——`DATA-P2-CAL` 单位 SB（而三份文档面钉 ADU）、坐标系 PIXEL（而注册表钉 HEALPIX）；`DATA-P2-RES` 在 :720 是 ADU、在 :1176 是 SB；`p2_write_descriptor` 只登记 1 个输出端口，注册表登记 2 个（球面树 + 摘要）。:1170-1174 的注释只引用了"写出端口单位 = SURFACE_BRIGHTNESS"这半句整改依据，未提同一条文的坐标系修订。
- 差在哪：条文说"以本节为准修订"，代码只改了单位半边（且注释按改完的口径自述），坐标系半边（PIXEL→HEALPIX）与本文件内部的同-schema 双单位都没动。判据方向：`pipeline.cpp:291-294` 的 COORDINATE_MISMATCH 无豁免、两侧比对，而 P2 链两侧 descriptor 都写 PIXEL ⇒ 恒不红；跨阶段边（P1 树→P2 入、P2 树→P3 入）在同一 IR 内没有 producer ⇒ 单位/坐标门**根本不比较**（同 check `runtime_client.cpp:244` `std::string prev = "artifact:cal";`，该 artifact 在 mosaic IR 内无生产者）。⇒ 三侧互斥没有任何一台门能发现，属放行侧。
- 后果：①"端口单位/坐标系登记＝同一套数"的对外主张（`RT-001.md:41`＋注册表 note）不成立；②把球面 NESTED 产品登记成 PIXEL 平面，使任何据此生成的 DAG/文档/校验（`docs/modules/registry/*.md` 端口表即由此派生）对"平面 vs 球面"的混淆不敏感——这正是 §31.1/§5.6 反复强调面亮度必须按立体角归一的那类量纲事故；③`run_manifest.provenance.coordinate_frames` 另有来源（节点 manifest 的字面 `"equatorial"`，本文件 :8959/:13021，属 CR-24x 段）⇒ 三套坐标口径互不引用。
- 定级：S2（判据缺失＋登记面互斥；未取到产品字节错值证据——错值路径要读 `p2_op_*` 写出面，属其它段，**须与 CR-240…CR-250 合并定案**）
- 怎么算修好：①`DATA-P2-RES` 在 :720/:1176 取同一单位（按 §31.1 取 SB），并让 `DATA-P2-CAL` 这一 descriptor-only 词汇要么删除改用 `DATA-HIPS-001`＋HEALPIX，要么在 `DATA_SEMANTICS` 面正式登记其单位/坐标系；②`PUBLIC_API.md:1137`、`docs/modules/phase2_upm.md:108`、`docs/modules/registry/astrocs.phase2.upm-apply.md:72` 三处 ADU 同步改（不改文档只改代码 ⇒ 文档-代码门会红）；③新增一台门：descriptor 端口集 ↔ 注册表端口集按 `data_schema_id`＋`unit`＋`coordinate` 双向比对（现 `eng/ci/check_registry_ir_parity.py` 只比 module_id 集合，`check_block_flow_ports_vs_code.py` 只验注册表 `code.token` 是否出现在源码，两者都比不到这里）。
- 置信：CONFIRMED（三侧原文逐字引用；本文件内双单位为同行直读）

### CR-240-11 · `p1_noise_snr_descriptor` 登记了一个"本节点无读取点、生产 IR 不绑"的输入端口，仅服务单元测试合成图
- 轴：D（主轴）/ C
- 位点：`:971-973`
  ```
  // 合成 IR（单元测试构造的最小图）以本端口绑定 photometry→noise-snr 依赖;
  // 生产 IR 不声明本端口（p1_flux.json 在本节点无读取点）。
  {"fluxes", "DATA-P1-FLUX", true, UnitId::ADU, CoordinateFrame::ICRS},
  ```
- 上位依据：`lib/infrastructure/pipeline/module_ports.registry.json` 顶层 `note`："ports 逐条反映 module_adapters.cpp 中该节点**实际打开/写出**的产物……由 `eng/tools/quality/check_block_flow_ports_vs_code.py` 双向机器复核（声明⇒实现 / 实现⇒声明）"；注册表 `astrocs.phase1.noise-snr` 的端口集是 `p1_sources`/`p1_phot`/`p1_cleaned`(in)＋`p1_snr`(out)，**没有** `p1_flux` 输入；`lib/infrastructure/cli/runtime_client.cpp:197-198` 原文："旧 IR 的 artifact:p1_flux 输入是**幻边**（本节点无 p1_flux.json 读取点），已删。"
- 现状：生产 IR 已把这条边判为幻边删除，注册表也不登记它，但 descriptor 仍然带着这个端口（注释说明留它的唯一理由是测试夹具的合图能通过）。
- 差在哪：`ModuleDescriptor::ports` 是运行时唯一被读的端口面（`pipeline.cpp:217-235` 的存在性检查＋`:262-294` 的单位/坐标比对），所以这条"产品面无读取点"的端口在运行时有真实效力：任何 IR 都能把 `p1_flux.json` 接到 noise-snr 而**不会**被判红，而接线事实是 SNR 的测光来源应为 `p1_phot.json`（:968 的 `photprov`）。
- 后果：端口登记面（＝运行时判据面）与产品读写事实不符，且与"单一真源"注册表互相矛盾；注册表所承诺的"双向机器复核"对 descriptor↔注册表的端口集差异实际为 0 覆盖（两门都比不到，见 CR-240-10 的 ③）。
- 定级：S3（现网 canonical IR 不绑它 ⇒ 不产生错值；错在登记面自述与实现不符，属"登记的端口从不被消费"母题）
- 怎么算修好：删 :973 该行，把单测合成图的依赖边改绑 `photprov`（真实读取点）；若必须保留 `fluxes` 边，则同批在注册表补 `p1_flux` 输入端口并给出读取锚点 `symbol`+`token`，让 `check_block_flow_ports_vs_code.py` 的双向复核真能命中。
- 置信：CONFIRMED（三面原文均已逐字引用；`git grep -n "p1_flux" -- module_adapters.cpp` 显示除 photometry 写出面 :5263 外无读取点）

### CR-240-12 · 注册表声明的"施加测光后的像素面"产品边在生产 descriptor 与生产 IR 两处都不存在
- 轴：C（主轴）/ R
- 位点：`:991-1012`（`p1_drizzle_descriptor` 的端口集：`calibrated`/`wcs`/`photprov`/`sources`/`snr`/`stacked`）与 `:924-946`（`p1_photometry_descriptor` 的输出只有 `fluxes`/`photprov`）——两处都没有 `photoapplied` 端口。
- 上位依据：`lib/infrastructure/pipeline/module_ports.registry.json`：
  - photometry 输出 `{"name":"p1_photoapplied","artifacts":["photoapplied_<light_basename>"],"data_schema_id":"DATA-P1-CAL","unit":"ADU","coordinate":"PIXEL","code":[{symbol:"p1_op_photometry","token":"p1_photoapplied_path"}],"note":"I_photo = k_photo·I_cal 的施加后像素面；drizzle 在 photometry_applied=true 时消费它。"}`
  - drizzle 输入 `{"name":"p1_photoapplied", … ,"note":"photometry_applied=true 时改读该面；缺失即 fail-closed（禁静默退回未测光 ADU）。"}`
  - 本文件 :113-118 也自述该链："RELEASE-02 FIX-P1 (P1-1): Phase1 测光归一化真正接到像素。- apply_photometry: I_photo = k_photo·I_cal"。
- 现状：`runtime_client.cpp:218-222` 把 drz 的像素输入只绑成 `{{"calibrated","artifact:cal"}}`（即标定节点产物），photometry 的施加后像素面在 typed 面无端口、无 artifact、无依赖边；两个节点之间的顺序只靠 `photprov`（`p1_phot.json`）这条 provenance 边间接保证（:220、:997-1011 注释自述该边是为了"drz 的存在性判定不再依赖并发文件约定"）。
- 差在哪：注册表为这条边写明了 fail-closed 语义（"缺失即 fail-closed，禁静默退回未测光 ADU"），但在运行时端口面上没有承载体 ⇒ 该 fail-closed 只能靠节点内部的文件判据实现，typed DAG 看不见它；一旦 canonical 链变更（例如 photprov 边被删、或某配置路径不产 p1_phot.json），drizzle 与 photometry 之间的**像素面**依赖不再有任何边约束，调度顺序回到"读得到就读、读不到就用未测光面"的并发文件约定——正是本文件 :931-944/:997-1011 反复记录过的非确定性形态（实测 14 次 10/4 翻转）。
- 后果：P1 栈产品的光度标定依赖在登记面不完整；`ASTROCS_DESIGN §2` 的"测光校准到星等坐标系"这一创新点在数据流登记上缺一条像素边，跨阶段追溯（p1_stack 是否含 k_photo）只能读 provenance 文本而非 DAG。
- 定级：S2（现网顺序恰好由 photprov 边间接保证 ⇒ 未证成错值；判据缺失是事实，缺运行时触发路径）
- 怎么算修好：①`p1_photometry_descriptor` 增输出 `{"photoapplied","DATA-P1-CAL",false,UnitId::ADU,CoordinateFrame::PIXEL}`、`p1_drizzle_descriptor` 增同名输入；②`runtime_client.cpp` 的 drz 输入按 `photometry_applied` 语义绑定（或双绑），并把 `artifact:` 映射加进 `check_registry_ir_parity.py:46-58` 的 `PHASE1_ARTIFACT_TO_PORT`（同批改，否则该门因新端口判红）；③在 `p1_op_drizzle` 侧给"photoapplied 缺失 ⇒ fail-closed 具名报错"补一条负例测试，使注册表那句 fail-closed 有可执行凭据。
- 置信：PARTIAL（未读 `p1_op_drizzle` 取像素面的实际分支，属其它段 ⇒ **须与 CR-240…CR-250 合并定案**后才能确认现网是否真的从未在无 photprov 时读 photoapplied）

### CR-240-13 · `struct P1Api` 是零引用死适配器，但四份合同/追溯页把它当作现行生产委托链陈述
- 轴：D（主轴）/ C
- 位点：`:1193-1200`
  ```
  struct P1Api {
    static acs_status create(...) { return p1_session_create(h, o); }
    static acs_status run(acs_handle h, acs_span_u8 c) { return p1_session_run(h, c, 0); }
    ...
  ```
- 上位依据：`docs/contracts/PUBLIC_API.md:706`"registry 关系：五函数经 P1Api …"；`docs/modules/phase1_session.md:12`"五函数经 P1Api（lib/infrastructure/scheduler/src/module_adapters.cpp:755-762）被 8 个 …"；`docs/modules/registry/astrocs.phase1.session.md:23`"五函数经 P1Api 被 8 descriptor 工厂委托"；`docs/traceability/TRACEABILITY_MATRIX.csv:10`（同一陈述且该行状态为 **VERIFIED**）。同文件 :20-21 自述现状是 P1-001 attempt 2 起 8 个 Phase1 节点改走"唯一真实 operation 委托"。
- 现状：`git grep -rn "P1Api"` 全跟踪面 ⇒ 代码里只有 :1193 的定义，**无一处实例化**（`make_session_module<P1Api>` 零命中）；本文件对 `p1_session_*` 的引用也只有 :1194-1198 这五处死壳内。P1 侧生产工厂在 :15461-15468 用 `P1NodeSpec`/`P1NodeModule`（属其它段，本条按 grep 事实引用）。`lib/phase1_session/README.md:30` 已经改口为"历史（P1-001 前）：P1Api 五函数指针委托 + make_session_module<P1Api>"，但其余四处未同步；`PUBLIC_API.md:777` 只写"P1Api/SessionModule 保留"。
- 差在哪：文档把"Phase1 会话五函数是生产委托链"当作 VERIFIED 的追溯事实陈述，而生产入口到五函数的调用路径已断（唯一路径在死结构里）；同时 `async_io_depth` 被 :1196 钉 0，与 `PUBLIC_API.md:722` 的 `{0,1,2}` 语义之间没有任何生产调用者可验证（TRACEABILITY 自己也登记"预读未按 depth 启用"）。
- 后果：按追溯矩阵/SRC 锚去核对"Phase1 会话合同在生产中被执行"会核到死代码；对外发布的 API 面（p1_session_*）实际只能由测试与 `lib/phase1_session` 自身触达。不改产品字节。
- 定级：S3（登记/追溯陈述失真，无产物错值）
- 怎么算修好：①删 `P1Api` 与 :265-269 的 p1 会话声明（若 `lib/phase1_session` 仍由别处调用则只删本文件死壳）；②把 `PUBLIC_API.md:706/777`、`docs/modules/phase1_session.md:12`、`docs/modules/registry/astrocs.phase1.session.md:23`、`TRACEABILITY_MATRIX.csv:10` 的"经 P1Api 委托"统一改为现状（P1NodeModule operation 委托），并把该行的 VERIFIED 降为带注记；③若打算继续保留 `{0,1,2}` 语义，就把 `async_io_depth` 变成配置键并接进 :1196 的位置，别留"合同有窗口、代码钉 0、还挂在死壳上"的三不像。
- 置信：CONFIRMED（`git grep -rn "P1Api"` 双向：定义命中、实例化零命中）

## 本格"核过但不报"的项与检索式

- `:1187-1191` `make_session_module` **没有**调用 `desc.validate(&err)`（本文件内 `git grep "desc.validate\|validate(&err)"` 零命中）：descriptor 的校验实际发生在 `ModuleRegistry::register_module`（`module.cpp:85-87`，失败即 `return fail`）⇒ 不存在"返回值被丢弃"，不报。
- `:1046-1184` 七个 P2 descriptor 的端口 `data_schema_id` 链（DATA-P2-CAL→COV→SMP→UPM→COR→REJ→INT→RES）逐端口比对：生产者/消费者两侧 schema 串完全相同（:1055↔:1073、:1074↔:1092、:1093↔:1111、:1113↔:1131/:1151、:1132↔:1150、:1152↔:1175）⇒ 无 DATA_MISMATCH 类冲突，不虚报；单位/坐标问题集中在 CR-240-10。
- `:1027`/`:1167` writer/verify 的 `execution_class="io"`＋`parallel_ok=false` 与 `module.cpp:50-53` 的 heavy+serial 门自洽（门只禁 cpu_heavy+serial）；`runtime_client.cpp:224-225`/`:257` 也确实以 `{"class","io"},{"parallel",false}` 绑定 ⇒ 不报"声明与派发矛盾"。
- `:940-946` photometry 的 `photprov` 输出与 `:968`/`:1002` 两处输入登记（noise-snr、drizzle）schema/单位一致（DATA-P1-PHOTPROV-001/DIMENSIONLESS/ICRS）⇒ 与注册表 `p1_phot` 三面一致，不报。
- `:903-905` 的注释（wcs 节点自行做星点检测、不消费 `p1_sources.json`，故 sources/wcs 不成环）与 `runtime_client.cpp:164-168` 的删除"幻边"说明一致，且注册表 wcs-platesolve 端口集（`p1_calibrated` in／`p1_wcs` out）与之相符 ⇒ 不报。
- 本段无新的科学常数/阈值：出现的"值"全是登记词汇（module_id、DATA-、UnitId、CoordinateFrame）与 :1196 的 `0`，已逐行入表。
"""

with io.open(p, "a", encoding="utf-8") as f:
    f.write(body)
s = io.open(p, encoding="utf-8").read()
s = s.replace("<!-- PROGRESS: 3/5 -->", "<!-- PROGRESS: 4/5 -->")
s = s.replace("4. `lib/infrastructure/scheduler/src/module_adapters.cpp:901-1200` —— 已读：否",
              "4. `lib/infrastructure/scheduler/src/module_adapters.cpp:901-1200` —— 已读：是（表 8 项，findings 4 条）")
io.open(p, "w", encoding="utf-8").write(s)
print("cell4 appended, progress -> 4/5")
