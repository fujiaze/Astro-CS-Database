import io, sys
sys.stdout.reconfigure(encoding='utf-8')

p = r"独立审计/证据/通读-CR-240.md"

body = r"""
---

# 格 3 · `module_adapters.cpp:601-900`

本格内容 = `SessionModule::execute/inspect/last_manifest` 尾段（:601-690）＋ 11 个生产 `ModuleDescriptor`（:692-900）。

## §2.5 数值处置表（格 3，9 项）

| 位点(文件:行) | 符号或键 | 现行值 | 它是什么（一句话用途） | 处置 | 依据锚 | 待确认时给保守方向与影响范围 | 置信 |
|---|---|---|---|---|---|---|---|
| :608,:614 | `ctx.set_provider("baseline")` / `e.provider = "baseline"` | "baseline" | 唯一已接线 CPU 后端的自报名 | 结构性不适用 | 词表标识；与 `lib/infrastructure/benchmark/backend_host/cpu_routing.cpp` 的 provider 词汇同源（本批未逐行读该面，属 provider 词表合同，不在本段建立新指控） | — | PARTIAL |
| :616-617 | `e.workers = cap; e.granted_workers = host_workers;` | — | 把"请求数"与"授予数"分别写进 trace | 结构性不适用 | B2-A18（`lib/include/astrocs/core/context.h:93-95`「此前 Runtime trace 写的是配置 budget 而非实际授予」）——本行正是该整改的落实，方向正确 | — | CONFIRMED |
| :695,:714,:733,:756,:775,:794,:813,:832,:851,:871,:898 | `d.version = "1.0.0"`（11 处同值，合并一行） | "1.0.0" | 模块登记版本 | 待确认 | 与构建期单源 `ASTROCS_VERSION_STRING` 相斥：产品 `module_build_id` 取 `desc_.module_id + "@" + ASTROCS_VERSION_STRING`（:13176/:13384/:15295），CI 夹具显示实际值为 `0.11.0-alpha.2`（`eng/ci/fixtures/run_manifest/real_manifest_sample.json:56-59`）；`d.version` 的唯一读取点是 `module.cpp:163`（`export_index_json`），而该函数在 `lib/**` 内零调用者（`git grep -rn "export_index_json" -- lib eng` → 仅 `eng/tests/unit/core_module_test.cpp:74`、`rt005_registry_test.cpp:128`） | 保守方向＝删除该字段或改取 `ASTROCS_VERSION_STRING`；影响范围＝登记面自洽性（产品不受影响，因该产品字段走 build 版） | CONFIRMED |
| :700-701,:719-720,:738-739,:761-762,:780-781,:800-801,:819-820,:838-839,:857-858,:876-885,:906-907 | `PortDescriptor{..., UnitId::X, CoordinateFrame::Y}`（端口单位/坐标系登记，合并） | ADU / SURFACE_BRIGHTNESS / DIMENSIONLESS / DEGREE；PIXEL / HEALPIX / ICRS | 端口值域语义登记 | 结构性不适用（词表登记本身）——但**登记面的两面性**见 CR-240-08 | 词表权威：`lib/infrastructure/pipeline/module_ports.registry.json` 的 `unit_vocabulary.authority` = 「docs/contracts/DATA_SEMANTICS.md §31.1（单位表）/§31.2（BUNIT 量纲可判）」，其 `tokens` 恰为 ADU/SURFACE_BRIGHTNESS/DIMENSIONLESS/DEGREE 四个 ⇒ 本段用到的 token 全在词表内，未发明第五个 | 保守方向＝端口单位缺失应判红而非落 `UnitId::UNKNOWN`（`module.h:23` 默认值即 UNKNOWN，而 `pipeline.cpp:286-288` 对 UNKNOWN 双向豁免单位门）；影响范围＝所有省略 unit 的端口（本段 11 个 descriptor 无一处省略） | CONFIRMED |
| :703-707,:721-726,:741-745,:764-768,:783-787,:803-807,:822-826,:841-845,:860-864,:887-891,:909-913 | `sci_id/alg_id/data_id/api_id/test_id` 登记 | 见 CR-240-07 | 节点自报的合同引用 | 待确认 | `docs/contracts/INDEX.yaml` 是合同 ID 唯一事实源（`artifacts/evidence/audit-2026-01/FIX_LEDGER.csv:184` 引「`docs/contracts/INDEX.yaml` 为合同 ID 唯一事实源」）；本段若干 ID 不在该源内（逐条见 finding） | 保守方向＝ID 未登记即拒绝注册（现为前缀门放行）；影响范围＝产品 `run_manifest.provenance.algorithm_ids` | CONFIRMED |
| :816,:834 | `execution_class = "io"` | "io" | P3 writer/verify 的执行类 | 结构性不适用 | `module.h:35` 词表 `cpu_heavy \| io \| light`；`module.cpp:50-53` 的 heavy+serial 门只对 `cpu_heavy` 生效 | IR 侧若把 io 节点写成 `cpu_heavy` 无人判（见 CR-240-09 的方向段） | CONFIRMED |
| :835,:817 | `d.parallel_ok = false`（verify/writer） | false | io 节点不并行声明 | 结构性不适用 | 与 `module.cpp:50`「heavy+serial 拒绝」配套 | — | CONFIRMED |
| :873,:854 等 | `execution_class = "cpu_heavy"`＋`:874 parallel_ok = true` | — | P1 重节点执行类 | 结构性不适用 | 同上 | — | CONFIRMED |
| :895-900（续 :910） | `p1_wcs_descriptor` 的 `alg_id = "ALG-002"` | "ALG-002" | WCS 解算节点的算法 ID 自报 | 见 CR-240-07（与 :888 star-psf 同值） | 待确认 | 保守方向＝改 `docs/contracts/INDEX.yaml` 内真实 ID（`ALG-WCS-001` @ INDEX.yaml:155）；影响范围＝P1 WCS 节点 manifest → 产品 provenance | CONFIRMED |

## findings（格 3）

### CR-240-07 · 生产 descriptor 把"占位合同 ID"写进节点自报，产品 `run_manifest` 的 provenance 因此登记了唯一事实源里不存在的算法 ID，且两个语义不同的节点共用同一个占位 ID
- 轴：C（主轴）/ D
- 位点（逐字）：
  - `:888` `d.alg_id = "ALG-002";            // wcs-psf-batch kernel`（在 `p1_star_psf_descriptor()` 内）
  - `:910` `d.alg_id = "ALG-002";            // wcs-psf-batch kernel`（在 `p1_wcs_descriptor()` 内，`module_id = "astrocs.phase1.wcs-platesolve"` :897）
  - `:860-861` `d.sci_id = "SCI-P1-COS-001"; d.alg_id = "ALG-P1-COS-001";`（`p1_cosmetic_descriptor`）
  - `:703-704` `d.sci_id = "SCI-P1-CAL-001"; d.alg_id = "ALG-P1-CAL-001";`（`phase1_descriptor`）
- 上位依据：
  1. `docs/contracts/INDEX.yaml:195` 定义 star-psf 的算法合同是 **`ALG-STARPSF-001`**（同文件 :55 列其为 `SCI-PSF-001` 的下游），`:155` 定义 **`ALG-WCS-001`**；`docs/algorithms/STAR_PSF_ALGORITHMS.md:5`「ID: ALG-STARPSF-001 上游 SCI: SCI-PSF-001 状态: DERIVED」。`ALG-002` 在 `docs/contracts/INDEX.yaml` 与 `docs/algorithms/STAR_PSF_ALGORITHMS.md` **均不存在**（检索式：`git grep -n "ALG-002\|ALG-STARPSF-001" -- docs/contracts/INDEX.yaml docs/algorithms/STAR_PSF_ALGORITHMS.md` → INDEX 只命中 ALG-STARPSF-001）。
  2. `docs/contracts/PUBLIC_API.md:681-684` 原文："registry descriptor 占位 ID（module_adapters.cpp:492-510，sci_id=SCI-P1-PSF-001/alg_id=ALG-002/data_id=DATA-P1-SOURCES/api_id=API-P1-003/test_id=TEST-P1-PSF-001）由 P1-PSF-INT 对齐本合同；冻结依据只取上游依据" ⇒ 合同面**自己称这些 ID 为占位**，并声明对齐任务待做；顺带：该条的行锚 `module_adapters.cpp:492-510` 现已漂到 :868-913（漂移 ~376 行），锚失效。
  3. `docs/modules/registry/astrocs.phase1.cosmetic.md:16-19`："本页由源码核对后修订——合同 ID 由 W3 骨架占位（SCI-P1-COS-001/ALG-P1-COS-001）更正为真实冻结 ID；……descriptor 占位 ID（module_adapters.cpp p1_cosmetic_descriptor）将由 W3 接线任务对齐本页" ⇒ 文档侧已改，代码侧未改，两侧现在**互不相同**。
  4. `docs/contracts/RT-001.md:26` 把 `ModuleDescriptor` 的 `SCI/ALG/DATA/API/TEST 引用` 列为冻结接口内容。
- 现状：`SCI-P1-COS-001`/`ALG-P1-COS-001` 在全跟踪面的出现只有四处：本文件 :860-861、`docs/modules/registry/astrocs.phase1.cosmetic.md:18`（那句"更正为真实冻结 ID"的说明本身）、`lib/phase1_session/README.md:74`（复述 descriptor）、`eng/ci/fixtures/run_manifest/real_manifest_sample.json:52`（**产品夹具**）。即：没有任何一份 science/algorithms 文档定义它们。`SCI-P1-CAL-001`/`ALG-P1-CAL-001` 同理（`FIX_LEDGER.csv:184` 早已判为"registry 页引用两个全库未定义的合同 ID"）。
- 差在哪：这些 ID 不是停在登记面，而是**进产品字节**：`lib/infrastructure/scheduler/src/module_adapters.cpp:13175` `{"algorithm_id", desc_.alg_id}`（P1 节点 manifest；:13383/:15294 同构）→ `lib/infrastructure/cli/commands.cpp:264-275` `if (m.contains("algorithm_id") ...) algs.insert(...); p["algorithm_ids"] = json(algs);`（其 :237 注释同时声明"节点自报；不在 CLI 侧造占位"）。于是 `run_manifest.provenance.algorithm_ids` 里出现 `ALG-002`、`ALG-P1-CAL-001`、`ALG-P1-COS-001`；更糟的是 star-psf 与 wcs-platesolve 两个科学语义不同的节点自报**同一个** `ALG-002`，产品层面看得到"算法 ID 可追溯"，实际追到的是唯一事实源里没有的字符串。
- 后果：①对外主张"provenance 的 algorithm_ids 可追溯到冻结合同 ID"（B2-A10/宪章 §4.3，:13172-13174 注释原话）对产品不成立；②按 run_manifest 做合同追溯/发布核验的下游（`eng/ci/fixtures/run_manifest` 是 CI 门夹具，说明确有门读该字段）会把不存在的 ID 当成已验证引用；③两节点共用 `ALG-002` 使"哪个算法产出这个像素"在产品里不可分。
- 定级：**S1**，触发路径逐跳：`lib/infrastructure/cli/commands.cpp:1721`（normalize 读 config）→ `lib/infrastructure/cli/runtime_client.cpp:159-188`（构造 IR 节点，module_id=astrocs.phase1.star-psf / wcs-platesolve）→ `lib/infrastructure/scheduler/src/runtime.cpp:256`（`registry_.create`）→ `module_adapters.cpp:15461-15468`（`p1_nodes[]` 用 `p1_star_psf_descriptor()`/`p1_wcs_descriptor()`）→ `module_adapters.cpp:888`/`:910`（`d.alg_id="ALG-002"`）→ `module_adapters.cpp:13175`（节点 manifest `algorithm_id`）→ `lib/infrastructure/cli/commands.cpp:275`（`p["algorithm_ids"]`）→ run_manifest 落盘。其中 :13175/:15295/:15461 属本文件其它行段（CR-24x），本条以逐行引用取证；若那些行的上下文（例如条件写入或覆盖）与引用不符，**须与 CR-240…CR-250 合并定案**后调级。
- 怎么算修好：①`:888`→`ALG-STARPSF-001`、`:910`→`ALG-WCS-001`（`docs/contracts/INDEX.yaml:195/:155`），`:860-861`→cosmetic 的真实冻结 ID（`docs/algorithms/COSMETIC_ALGORITHMS.md` 的 `ALG-COS-001..005`，按 registry 页 :16-19 的"更正"结果取），`:703-704`→`SCI-CAL-001`/`ALG-CAL-001`（`docs/science/CALIBRATION.md:5` 声明的冻结 ID，本文件 :43 的注释用的正是这个）；②同批把 `docs/contracts/PUBLIC_API.md:681-684`、`docs/modules/registry/*.md` 的 upstream 行与漂移锚 `:492-510` 一起改，否则文档-代码门（`eng/tests/quality/test_doc_machine_check.py` 一类）会因两侧不一致判红；③把"ID 必须存在于 `docs/contracts/INDEX.yaml`"做成注册门（现在的 `module.cpp:57-72` 只判 `SCI-`/`ALG-` 前缀且允许空串 ⇒ 恒真式放行），并配一条负例：注册 `alg_id="ALG-002"` 必须判红。
- 置信：CONFIRMED（四处 ID 的有无、消费链的 `文件:行`、CI 夹具里的实际产品值均已逐处 grep 复算）

### CR-240-08 · 端口/单位登记有两套并行面（C++ descriptor 与 `module_ports.registry.json`），名字与完备度都不一致，运行时单位门只看其中较窄的那一面
- 轴：C（主轴）/ D
- 位点：`:699-702`
  ```
  d.ports = {
      {"frames", "DATA-P1-FRAME", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"calibrated", "DATA-P1-CAL", false, UnitId::ADU, CoordinateFrame::PIXEL},
  };
  ```
- 上位依据：`lib/infrastructure/pipeline/module_ports.registry.json` 顶层 `note` 自述："每个 node 唯一真实 operation 绑定表。**单一真源**：typed DAG compiler 按 (module_id, operation) 绑定到唯一真实 operation 与 DLL/入口……ports 逐条反映 module_adapters.cpp 中该节点实际打开/写出的产物……由 `eng/tools/quality/check_block_flow_ports_vs_code.py` 双向机器复核"；本文件 `:33-34` 亦自述"operation/entry 名与 `lib/infrastructure/pipeline/module_ports.registry.json` 冻结绑定表一致"。`docs/contracts/RT-001.md:41`「`PortDescriptor`：name、data_schema_id、is_input、unit（UnitId）、coordinate（CoordinateFrame）」。
- 现状：同一 `module_id = "astrocs.phase1.calibration"`，注册表登记 5 个端口 `lights`(DATA-P1-FRAME/ADU/PIXEL)、`master_bias`/`master_dark`/`master_flat`(DATA-P1-MASTER/ADU/PIXEL)、`p1_calibrated`(DATA-P1-CAL/ADU/PIXEL)；descriptor :700-701 只登记 2 个端口，且**两个名字都与注册表不同**（`frames`↔`lights`、`calibrated`↔`p1_calibrated`）。运行时唯一的端口消费者 `lib/infrastructure/scheduler/src/pipeline.cpp:217-235` 用 descriptor 名做 IR 边校验（`input port 'X' not in module ...`），`:282-293` 用 descriptor 的 unit/coordinate/data_schema 做 `UNIT_MISMATCH`/`COORDINATE_MISMATCH`/`DATA_MISMATCH` 判据；而 CLI 生产 IR 用的名字与 descriptor 同（`runtime_client.cpp:159` `{{"frames","artifact:in"}}, {{"calibrated","artifact:cal"}}`）。两套名字的桥是**手写字典**：`eng/ci/check_registry_ir_parity.py:46-58` `PHASE1_ARTIFACT_TO_PORT = {"artifact:in": "lights", "artifact:cal": "p1_calibrated", ...}`，且该脚本的判据只有 module_id 集合双向（docstring P1/P2/P3），不比对端口名与单位。
- 差在哪：自称"单一真源"的注册表与运行时实际使用的 descriptor 面是三套词汇（注册表名 / descriptor 名 / IR artifact 名），而唯一同时看得见它们的 `check_registry_ir_parity.py` 只核 module_id，不核端口名与单位；结果是注册表里 master_* 三口的单位/坐标系登记在运行面**永不进入任何判据**（descriptor 无此端口 ⇒ `pipeline.cpp` 的端口检查与单位门根本看不到它们），而 `pipeline.cpp:286-288` 的单位门本身对 `UnitId::UNKNOWN` 双向豁免（`prod_port.unit != UnitId::UNKNOWN && p.unit != UnitId::UNKNOWN`），豁免方向＝放行。
- 后果：端口级单位/坐标系一致性这一对外主张（DISP-CAL-013/§31.1 的"BUNIT 量纲可判"链条）只在 descriptor 覆盖到的端口上成立；master 帧（bias/dark/flat）的单位在运行面无门，任何一侧改名或改单位都不会被现有门判红，只会在另一侧留下悬空引用。
- 定级：S2（不改产品字节；错在"两份登记面说同一件事"这一主张；`UnitId::UNKNOWN` 豁免方向已钉＝放行）
- 怎么算修好：①把 descriptor 端口集与注册表端口集做成双向门（新 `chk` 或在 `check_registry_ir_parity.py` 内加 P4：按 `data_schema_id`＋`unit`＋`coordinate` 比对两侧，名字差异必须经声明的映射且映射表要能被双向验证）；②`pipeline.cpp:286-288` 的 `UNKNOWN` 豁免改为"任一侧 UNKNOWN 即判红（未登记单位不得放行）"，或让 `ModuleDescriptor::validate`（`module.cpp:34-46`）拒绝 `unit == UnitId::UNKNOWN` 的端口——注意本文件 11 个 descriptor 目前全部显式给单位，改成拒绝不会误伤现状；③master_* 三口若确实只走 `config_path` 载体，就在注册表里把它们标 `carrier=config_path` 且在合同面写明"不参与运行期端口单位门"，避免"登记了但判不到"。
- 置信：CONFIRMED（两侧端口集逐条比对；消费者与豁免表达式逐行读）

### CR-240-09 · 执行类校验是单向的：descriptor 说 `cpu_heavy` 而 IR 说不 heavy 才判红，反向（io 节点被 IR 当重节点派发）无判据
- 轴：R（主轴）/ C
- 位点：`:816` `d.execution_class = "io";`（`p3_writer_descriptor`）与 `:834-835` `d.execution_class = "io";   // 读回校验非计算 heavy(heavy+serial 资源门禁止)`＋`d.parallel_ok = false;`
- 上位依据：`lib/include/astrocs/core/module.h:35` `execution_class;  // cpu_heavy | io | light`；`lib/infrastructure/scheduler/src/module.cpp:50-53`「heavy+serial 拒绝（RT-005: cpu_heavy 必须 parallel_ok）」；`docs/contracts/RT-001.md:26`（descriptor 的执行模型属冻结接口）。条文面对"IR 侧 resource_class 必须与 descriptor 一致"零提及（检索式：`git grep -n "resource_class" -- docs/contracts ASTROCS_DESIGN.md` 本段未取证，属合并待办）。
- 现状：唯一比对点 `lib/infrastructure/scheduler/src/pipeline.cpp:210-215`：`if (desc->execution_class == "cpu_heavy" && n.resource_class != "cpu_heavy") issues.push_back(SERIAL_HEAVY);` ⇒ 只判"descriptor heavy、IR 不 heavy"一个方向；descriptor 为 `io`/`light` 而 IR 写 `cpu_heavy` 时不产生任何 issue，而 `runtime.cpp:176` 起就把 `spec.resource_class = n.resource_class` 交给调度器决定租约与池。
- 差在哪：判据方向没钉全。漏判一侧的后果是 io 型节点（writer/verify）被当作重计算节点占用 heavy 租约份额，反向不报 ⇒ 资源观测面（宪章 §10.5 利用率、P7 份额均分）与实际负载不再对应；这不是"保守"，因为另一节点会被饿成 `cap=1`（本文件 :599 的降级即在此发生），方向偏**放行**。
- 后果：写手/校验节点被登记为 heavy 时，同批在途节点的租约份额被压小（`context.cpp:236-237` 按 `dispatch_budget_hint()` 收缩），出现"科学节点静默单线程"，同时 trace 里 `workers=1` 看起来像代码退化——误导排障方向。
- 定级：S3（本段无法证明现网 IR 真把 io 节点写成 heavy；`runtime_client.cpp` 属 CLI 面，本批只读了 :159-236 的端口部分）
- 怎么算修好：`pipeline.cpp:210` 改成双向严格等值（`desc->execution_class != n.resource_class` ⇒ issue，除非 descriptor 为 `light` 且 IR 为 `io` 一类声明过的等价），并在 `eng/ci/fixtures` 配一条正例/负例；同批核对 `runtime_client.cpp` 各节点的 `resource_class` 实参，避免改完即红。
- 置信：PARTIAL（缺现网 IR 中 io 节点被标 heavy 的实际样本）

## 本格"核过但不报"的项与检索式

- `:601-619` host 初始化失败即 `RESOURCE` 返回、`:608` 只在真实成功路径置 provider（注释 :605-607 声明"非 config 值冒充"）：与 `context.h:93-95` 的 B2-A18 整改方向一致，不报。
- `:643-648` manifest 的 `allocator.free` 归属与 `PUBLIC_API.md:723`「out=host alloc，调用方经 host free 释放」一致，不报"内存泄漏/双重释放"；其返回码被忽略的问题已记 CR-240-06，不重复计。
- `:656-680` `inspect()` 内 `hs.init(workers_)` 的单位问题记在 CR-240-05；本段另查明 `inspect()`/`validate_config()` 在全仓无调用者（检索式：`git grep -rn -- "->validate_config(\|->inspect()" -- lib` → 仅 `runtime.cpp` 的 `plan/execute/last_manifest`），该事实作为 CR-240-04/05 的可达性限定，不单列 finding（属"接口存在但无宿主调用"，需与后续段确认是否由 orchestrator/DLL 面调用后才能定案）。
- `:692-747` 三个 phaseN_descriptor 的 module_id 为 `astrocs.phase1/2/3.*`；`astrocs.phase2.resample`/`astrocs.phase3.resample` 与注册表里 P2/P3 的模块名是否同名本段未逐一比对（注册表 `modules` 共 20 项），列入合并待办。
- `:835` 的注释"heavy+serial 资源门禁止"与 `module.cpp:50-53` 实际门一致（确为禁止 cpu_heavy+serial），不报注释失真。
- `:876-885` star-psf 的 4 端口（含 `wcs` 输入）与注册表 `astrocs.phase1.star-psf` 的 4 端口逐条比对：`data_schema_id`/`unit`/`coordinate`/`scalar` 全一致（registry `p1_wcs`＝DIMENSIONLESS/ICRS、`p1_sources`＝DIMENSIONLESS/ICRS、`p1_psf`＝DIMENSIONLESS/PIXEL）⇒ 该模块只有"名字"差异（并入 CR-240-08），数值面同一套，不虚报单位冲突。
"""

with io.open(p, "a", encoding="utf-8") as f:
    f.write(body)
s = io.open(p, encoding="utf-8").read()
s = s.replace("<!-- PROGRESS: 2/5 -->", "<!-- PROGRESS: 3/5 -->")
s = s.replace("2. `lib/infrastructure/scheduler/src/module_adapters.cpp:301-600` —— 已读：是（表 14 项，findings 2 条）",
              "2. `lib/infrastructure/scheduler/src/module_adapters.cpp:301-600` —— 已读：是（表 16 项，findings 3 条）")
s = s.replace("3. `lib/infrastructure/scheduler/src/module_adapters.cpp:601-900` —— 已读：否",
              "3. `lib/infrastructure/scheduler/src/module_adapters.cpp:601-900` —— 已读：是（表 9 项，findings 3 条）")
io.open(p, "w", encoding="utf-8").write(s)
print("cell3 appended, progress -> 3/5")
