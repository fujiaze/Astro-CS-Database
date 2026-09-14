
### FD-I-001 `providers/` 一名两指：不编译的源目录 与 登记为 IMPLEMENTED 的交付路径共用同一目录名
- **ID**：FD-I-001 ｜ **类别**：I_DOC_HYGIENE（治理向，附交付判定影响）｜ **优先级**：**P1**
- **位置**：`packaging/astrocs.product.json:17`（`unit_id=PROV-CPU-BASELINE`、`kind=provider`、`rel_path=providers/astrocs_cpu_baseline.so`、`status=IMPLEMENTED`）；
  `CMakeLists.txt:220-221 add_library(astrocs_cpu_baseline SHARED lib/backend_host/baseline_backend.cpp)`；
  `CMakeLists.txt:232-236 set_target_properties(astrocs_cpu_baseline PROPERTIES OUTPUT_NAME "astrocs_cpu_baseline" PREFIX "")`；
  非编译源面：`providers/cpu/{baseline,avx2,avx512}/**`；自证未接线：`CMakeLists.txt:125` 与 `:217-219`（「正式 provider ABI/Windows DLL → CPU-002 (providers/cpu/baseline)」）
- **问题说明**：全仓 CMake 里字符串 `providers/` **只出现在注释行**（非注释命中 = 0）⇒ `providers/cpu/**` 的源**不进任何 target**；
  而**交付清单**里的 `providers/astrocs_cpu_baseline.so` 是 install 路径别名，真实源是 `lib/backend_host/baseline_backend.cpp`。
  ⇒ 同一目录名同时指「不编译的源码目录」与「IMPLEMENTED 的交付单元所在路径」，**且无任何文件说明这一区分**（`contracts/`、`docs/contracts/` 内 `providers/` 0 命中）。
- **实测危害（不是理论）**：本审计**两个独立代理先后栽在同一处** —— L28b §4-3 写「根 :220 把 providers 编入生产」（假），L28c-D-001 写「两个**已交付** ISA 头」（假，
  实为计划中/未接线）。二者都是**读注释与路径名推断交付面**得出，而非读 target 源列表。⇒ 这条同时是**「路径名当交付判据」的方法论反例**，与 C-12（证据产物时效性）同族：
  **交付面的唯一判据是 target 源列表 + product.json 单元，不是目录名、不是注释、不是 install 路径字面量。**
- **影响**：任何人（人或 agent）据 `providers/` 目录内容判断"provider 已交付/其注释即算子语义权威"都会得到错误结论；
  CPU-002 落地时该批头与 .cpp **一次性激活**，届时其算子语义权威目前只存在于未编译的头注释里（L28b-D-002、L28c-D-001 的升级条件同源）。
- **依据条款**：宪章 §16.2（发布清单=交付单元唯一事实源）、§8.3、§12.3-10；根 AGENTS.md「目录规范（强制）」
- **建议处置**：①在 `DEPENDENCIES.md` 或 `docs/contracts/` 显式登记「`providers/` 源目录 ≠ install 路径 `providers/`」，或 ②把未接线源目录改名（如 `providers_draft/`）——
  **但负责人已就 `run/**` 内文件裁定"不改名仅登记"，本条同理建议只登记不动文件**；③机器门：`product.json` 每个 IMPLEMENTED 单元的 `rel_path` 必须能由某个 target 的
  `OUTPUT_NAME`/`install(` 规则**反查到源文件**，反查不到即 FAIL（与 L27-006 required_unit 不同源 同族，可一并实现）
- **取证口径**：`grep -rn "providers/" --include=CMakeLists.txt --include=*.cmake` 排除注释行后 0 命中；`sed` 直读 :220-236；`grep` product.json:17；`contracts/` 内 0 命中
- **置信度**：高 ｜ **related**：**L27-006**（required_unit 与安装面不同源，M8a 主落，本条不并档：那是"清单缺项/漂移"，本条是"同名异指致交付面误判"）、
  **C-12**（证据产物时效性）、**L28b-D-002 / L28c-D-001**（两个被本陷阱影响的判定，均已由前台追加订正行）、M5b-G-01、A-32（入口术语一名两指同型）
