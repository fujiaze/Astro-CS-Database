# T06 第 1 轮订正 · 子代理 E（`docs/engineering/governance/**`、`docs/engineering/build/**`、`docs/engineering/README.md`）

车道范围：`docs/engineering/governance/*.md`、`docs/engineering/build/*.md`、`docs/engineering/README.md`。
**全程无 git 写操作**，git 只读且一律 `git -c core.quotepath=false`。`.gitignore`、
`docs/DOCUMENT_INDEX.yaml`、`contracts/**`、`standards/**`、`resources/**`、`testing/**`、
`architecture/**`、`api/**`、`data/**` 未被本车道触碰（见 §7 自证段的范围核对）。

审稿意见来源：`run/GOVERN-08/审核包-R2/T06-审稿-DOC-ENG.md`。**每条都自己跑命令复核**，不采信审稿员转述。

---

## 1 逐条处置表

| 编号 | 处置 | 改前逐字 | 改后逐字 | 复核命令与输出 |
|---|---|---|---|---|
| **1-1 / R1** 构建正本不在版本控制内 | **登记（本单不能修）** | `docs/engineering/build/README.md` 全文 2 行，无任何版本控制状态说明 | 新增「**本目录当前不在版本控制内，属仓库侧缺陷。**」段 + 三条复现命令 + 后果 + 订正方向（规则锚到 `/build/`、补 `!docs/engineering/build/`、四份正本入库） | `git check-ignore -v docs/engineering/build/BUILD_GRAPH.md` → `.gitignore:20:build/  docs/engineering/build/BUILD_GRAPH.md`；`git -c core.quotepath=false ls-files docs/engineering/build \| wc -l` → `0`；`sed -n '20p' .gitignore` → `build/` |
| **8-11** 生产构建图闭包不符 | **已改**（手工重导 + 登记） | 表 31 行；`acsd_hips \| 11 \| c417-1ccb-f83e`；`acsd_phase2 \| 9 \| 1f29-dde6-2cba`；**无** `acsd_gaia_zlib_include` / `acsd_platform_math` / `acsd_platform_zlib` | 表 34 行；`acsd_hips \| 12 \| 3f23-587c-c49f`；`acsd_phase2 \| 8 \| f368-2182-1625`；补入 `acsd_gaia_zlib_include \| add_library \| lib/infrastructure/gaia_xpsd_client/CMakeLists.txt \| 0 \| e3b0-c442-98fc`、`acsd_platform_math \| add_library \| CMakeLists.txt \| 0 \| e3b0-c442-98fc`、`acsd_platform_zlib \| add_library \| CMakeLists.txt \| 0 \| e3b0-c442-98fc` | `python3` 调 `cmake_graph.py` 的 `parse_cmake_graph` / `production_entry` / `production_closure` / `source_fingerprint`，`CLOSURE SIZE: 34`、`ENTRY: acsd`、`TOTAL TARGETS IN ROOT GRAPH: 62`。渲染用 `gen_build_graph_doc.py` 自带的 `md_table(..., per_chunk=9)`，分块边界与生成器逐字一致（4 块：9/9/9/7 行） |
| **10-5** 复算链三重失效 | **登记（需代码侧）** | `## 复算` 只有一句命令 + 一段判读，无任何失效登记 | 新增三行阻断表（落点常量 / fail-closed 退出码 / 非生产登记项含非根图目标）+ 「生成器跑不出这个结果」的明示 + 34 行的可复算口径说明 | `ls docs/engineering/BUILD_GRAPH.md` → 不存在；`python3 eng/tools/arch/gen_build_graph_doc.py --out /tmp/bg.md` → `EXIT=1`，stderr `NONPROD 登记项不在根构建图: acsd-stage2（删掉该行或改对名字）`；逐项核对 NONPROD 13 条 ⇒ `acsd-stage2` / `calibrated_pair_diag` / `rejection_cli` 三条 `in_root_graph=False` |
| **8-12** 交付产物名四点不符 | **已改** | `acsd-linux-<sha>.tar.gz`（Alpha 前无版本号，见最高设计 ）` / `acsd-win-<sha>.zip`（同）；`产物名含 commit SHA 短 8（Alpha 前无版本号；可发布 Alpha 后按 加版本）`；`每产物附 SHA256SUMS.txt` | 三行产物行改写为 `ACSD-Linux-amd64-<VERSION>.tar.zst`、`ACSD-Linux-amd64-<VERSION>.tar.gz`（`--tar-gz`/无 zstd 时）、`ACSD-Windows-amd64-<VERSION>.zip`，并新增「打包器」「`<归档文件>.sha256`」两列；命名节改为「产物名含版本位，取根 `VERSION` 的整行内容；commit SHA 不进文件名」「每个归档内附 `SHA256SUMS`…归档外另有 `<归档文件>.sha256`」 | `grep -n "ACSD-Linux\|pkg_name_base\|tar.zst" eng/tools/make_linux_release.py` → `:5 ACSD-Linux-amd64-<X.Y.Z-alpha.N>.tar.zst`、`:58 pkg_name_base = f"ACSD-Linux-amd64-{base}"`、`:54 base = open(VERSION_FILE…).read().strip()`、`:150-157` zstd/`.tar.gz` 二选一；`make_windows_release.py` → `:5 ACSD-Windows-amd64-<X.Y.Z-alpha.N>.zip`、`:70 pkg_name_base = f"ACSD-Windows-amd64-{base}"`、`:66` 同读 `VERSION`、`:165 arch = …f"{pkg_name_base}.zip"` |
| **BUILD_NODES `-G Ninja` 与 preset 合同冲突**（审稿 §6 `[转]`） | **已改** | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` + `ninja -C build`；`两侧走同一个 preset` | 改为 `cmake --preset win-msvc-17.14.39-x64` / `cmake --build --preset win-rel` 与 `cmake --preset linux-control` / `cmake --build --preset linux`，并加两平台 preset 面表（generator / binaryDir / 配置类型）+ 三条约束（`condition` 互斥、preset 合同里没有 Ninja、`CMAKE_BUILD_TYPE` 对 VS 生成器无效）；`两侧走同一个 preset` → `两侧各走本平台的 preset` | `python3 -c "json.load(open('CMakePresets.json'))"` → `base-msvc` generator `Visual Studio 17 2022` / binaryDir `${sourceDir}/build/win-msvc-17.14.39-x64`；`linux-control` generator `Unix Makefiles` / binaryDir `${sourceDir}/build/linux-control` / `CMAKE_BUILD_TYPE: Release`；`buildPresets.win-rel.configuration: RelWithDebInfo`。preset 描述自述「Never edits this preset to another generator」「不固定 Ninja/MinGW」 |
| **1-12** `UNRESOLVED.md` 章节编号断裂 | **已改** | 物理序 `## 1`(5) / `## 2`(14) / `## 3`(43) / `## 5`(58) / `## 4`(66) / `## 5`(78) / `## 6`(85) | 物理序改为单调 `## 1…## 7`（工程执行面阻塞项 = 4、已裁 = 5、更新规则 = 6、登记面边界 = 7） | `grep -n "^## " docs/engineering/governance/UNRESOLVED.md` → `5:## 1 本面的定位` / `18:## 2 方向裁决` / `47:## 3 来源核实` / `62:## 4 工程执行面阻塞项` / `70:## 5 已裁` / `82:## 6 更新规则` / `89:## 7 登记面边界` / `94:## 参考文献` |
| **1-12 附带** 双编号空间无优先级 | **已改（写明范围 + 登记待裁）** | 无任何编号空间声明 | 在「本面的定位」节末新增：「**本文件的编号空间只服务本目录**：`## n` 与 `裁-n` / `核-n` / `ENG-Bn` 各自的含义仅在本目录内有效，不与 `run/` 下审核包的登记编号混用……两个编号空间之间是否需要优先级，属登记面范围权，待负责人裁定后补写在此。」 | 文件内新增段落；审核包侧 `run/GOVERN-08/审核包-R2/UNRESOLVED.md` 独立存在，两处编号各自有定义 |
| **1-12 附带** 章节条数失真 | **已改** | `（18 条）` | `（22 条）` | 逐行数表行：方向裁决层 22 行（`裁-5,6,7,8,9,10,11,12,13,14,15,16,18,19,20,21,22,23,24,25,26,27`）、来源核实层 6 行（`核-1…核-6`，与原文「6 条」相符）、工程执行面 2 行、已裁 5 行 |
| **9-10** `UNRESOLVED.md:41` 编码损坏 | **降级（不猜改）** | `science 分册的稀疏控制点权��卷 验证证据标准的纪律一节a 第 3 条与 …` | `science 分册的稀疏控制点权重〔两字符不可辨识〕卷 验证证据标准的纪律一节a 第 3 条与 …` | 文件本身是**合法 UTF-8**（`open(p,encoding='utf-8').read()` 无异常），坏的是两个**字面 U+FFFD**：`b'...\xe7\x9a\x83\xef\xbf\xbd\xef\xbf\xbd\xe5\x8d\xb7...'`；改前 `t.count('\ufffd') = 2`，改后 `= 0` |
| **9-11** `TRACEABILITY.md` HTML 订正注记 + 版本代次 | **已改** | `production UPM 权重 = quality × control_reliability（旧名 geometric_reliability） × control_ivar；禁止 star-SNR/support^p 乘因子<!-- 订正: 检查-跨文档冲突 绿12——authority 正本 PHASE2_UPM.md:23 已定名 control_reliability 并注旧名 -->`；七行 `UPM 权重门 UPMW-00n（V19R3）` | `production UPM 权重 = quality × control_reliability × control_ivar；禁止 star-SNR/support^p 乘因子`；七行 `UPM 权重门 UPMW-00n`（其余各列逐字保留） | `grep -n "V19R3\|<!--" docs/engineering/governance/TRACEABILITY.md` → 无命中（仅剩 BUILD_GRAPH 的机器块标记与合法合同 ID 的正则误配，见 §6 自证段） |
| **1-13** `DOCUMENT_GOVERNANCE.md` 目录拓扑失真 | **已改** | ` │   └── abi/ cpu/ data/ io/ observability/` | ` │   ├── api/（含 api/abi/）对外与跨库接口、C ABI 基础层` … ` │   ├── resources/（含 resources/cpu/、resources/observability/）性能模型与可观测性` … ` │   └── testing/ 测试标准与验证证据`（共 9 行） | `git -c core.quotepath=false ls-files docs/engineering \| sed 's\|/[^/]*$\|\|' \| sort -u` → `api`、`api/abi`、`architecture`、`contracts`、`data`、`governance`、`resources`、`resources/cpu`、`resources/observability`、`standards`、`testing`（+ 根）；**无** `abi/`、`cpu/`、`io/`、`observability/` |
| **2-9** 排异算法数 | **已改** | `Rejection 7 种排异自动选择科学门` | `Rejection 生产算法集 4 种（none / percentile / winsorized / linear fit）排异自动选择科学门` | `sed -n '294p' docs/ACSD_DESIGN.md` → 「按 N 自动选择排异算法；生产算法集为 none、percentile、winsorized、linear fit」= **4 种**；代码 `lib/algorithms/coverage/include/astro/phase2/rejection.h:45–67` 的 `P2RejectionMethod` 有 12 个枚举值（`AUTO=10` 只在 planning 层、永不进 kernel）⇒ 11 个 kernel 方法。**7 这个数两侧都对不上**，来源见 §5 |
| **2-18** 文件域不互斥 | **已改** | B 线 `eng/tests/**`、`eng/contracts/**`、**`docs/engineering/**`**、…「文件域点名清单」一节 B 域 19 篇 ／ shared 线 `docs/science/**`、**`docs/engineering/**`**、…「文件域点名清单」一节 shared 域 60 篇 | B 线 `eng/tests/**`、`eng/contracts/**`、B 域点名清单 ／ shared 线 `docs/science/**`、**`docs/engineering/**`**、shared 域点名清单；glob 记法段补「`docs/engineering/**` 与 `docs/science/**` 整体归 shared，因此本表的四条线互斥，任一文件只属一条」 | `grep -n "docs/engineering/\*\*" docs/engineering/governance/DUAL_LINE.md` → 只剩 shared 线那一行（第 19 行）+ 记述句，`docs/engineering/**` 不再同时出现在两条线上 |
| **2-18 附带** 表头篇数与清单实数不符 | **已改** | `…B 域 19 篇` / `…shared 域 60 篇` / `B 域 19 篇：` / `shared 域（篇数待归属裁决后重算）：` | `B 域点名清单` / `shared 域点名清单`（表头删篇数）；清单抬头改 `B 域（按下列点名清单计 1 篇）：` 与 `shared 域（按下列点名清单计 4 条；归属裁决后重算总篇数）：`；生效范围句改 `B 域 1 条、shared 域 4 条，共五条` | 点名清单的反引号行实测：B 域 1 条（第 32 行 `../testing/VALIDATION_EVIDENCE.md`）、shared 域 4 条（38/40/42/44 行） |
| **2-18 附带** 残留交叉 | **登记（待裁）** | 无 | 新增「**残留交叉**：`docs/engineering/**` 整体归 shared 后，B 域点名清单里唯一那一条 `../testing/VALIDATION_EVIDENCE.md` 本身落在 shared 的 glob 内，两条线在这一份文件上仍交叉。该交叉属归属问题、待裁，本篇不擅自把它划给任一条线。」 | `docs/engineering/testing/VALIDATION_EVIDENCE.md` 存在且被 shared 的 `docs/engineering/**` 覆盖 |
| **7-7** `--fault-inject` 无执行器 | **已改（降级为「无执行器」）** | `…两类归零后删除本文件 ⇒ 门转全量强制（任何 error 即 rc=1）。负例注入（--fault-inject）一律绕过基线、全量强制。` | 同句后接：`负例注入（`--fault-inject`）一律绕过基线、全量强制。**该机制当前无执行器**：全仓检索 `--fault-inject` 只命中本行与 `../resources/cpu/ISA_VARIANTS.md` 的两处提及，仓内没有消费该开关的程序，也没有对应的失败注入点；因此这条判据描述的是目标态，不是已生效的门。执行器补齐前，负例面在本仓不可复跑，不得以「本判据已生效」作为判据覆盖的证据。` | `grep -rn -- "--fault-inject" docs/ eng/ lib/` → **恰好 2 处**：`docs/engineering/governance/TRACEABILITY.md:142`、`docs/engineering/resources/cpu/ISA_VARIANTS.md:162`。`find eng lib -name "*fault*"` 无执行器文件（仅 `eng/packaging/config/defaults.json` 因内容匹配被列出）。另注：`aio_hips_writer.cpp` 有一整套 `fault_injected()` + `ACSD_HIPS_*_FAULT` 环境变量机制，**与 `--fault-inject` 不是同一机制**，不能充作该判据的执行器 |
| **1-14** `README.md` 漏列 `UNIFIED_OBJECTS.md` | **已改** | 目录表首行为 `\| 目录 \| 内容 \|` + `\| architecture/ \| …` | 表头 `目录` → `路径`，首行插入 `\| UNIFIED_OBJECTS.md \| 顶层正本：13 个统一数据对象到 canonical schema 的逐对象合同 \|` | `wc -l docs/engineering/UNIFIED_OBJECTS.md` → `137` |
| **1-14 附带** COMMENT.md 预登记 | **已改（只写路径与一句说明）** | `\| standards/ \| 代码、数值、并发、缓存、兼容性、优化、文档、依赖八项工程标准 \|` | `\| standards/ \| 工程标准：代码（`CODE.md`）、注释（`COMMENT.md`）、数值、并发、缓存、兼容性、优化、文档、依赖 \|` | 另有人已建 `docs/engineering/standards/COMMENT.md`（2378 字节）；本车道**只登记路径与一句说明，未编内容、未读其正文**。注意：本 README 只有一张表（目录表），不存在「contracts/standards 表」，故 COMMENT.md 只登记在这一处；若后续要单列 contracts 表，需先有那张表 |
| **5-8 / 5-7** 参考文献孤儿条目 | **已改（正文补 `[n]`）** | 7 份文件正文 `[n]` 引用数全为 **0**，文末条目 17 条全为孤儿 | 7 份文件全部 0 孤儿 | 见 §2 验证输出 |
| **7-1 / 10-1** 指向不存在路径的引用 | **已改（无待处置项）** | — | — | 修正正则后全车道 50 条缺失路径，**白名单内 0 条**。审稿员脚本报的两条是**正则截断假阳性**：`lib/infrastructure/cli/version_generated.h`（实为 `.h.in`）与 `lib/infrastructure/scheduler/src/module_adapters.c`（实为 `.cpp`），`ls` 两者均存在 |

---

## 2 验证输出（改完必跑，四条全过）

```console
$ grep -n "^## " docs/engineering/governance/UNRESOLVED.md
5:## 1 本面的定位
18:## 2 方向裁决
47:## 3 来源核实
62:## 4 工程执行面阻塞项
70:## 5 已裁
82:## 6 更新规则
89:## 7 登记面边界
94:## 参考文献
```

```console
$ grep -rnE "V1[0-9]|V2[0-9]|R-[0-9]+|P-[0-9]+|<!--" docs/engineering/governance docs/engineering/build
（命中 32 行，逐类判定见下）
  TRACEABILITY.md:198,204,210,211,213,216,221,280,282,286-300  ← 正则误配：合同 ID 里的 `WR-001`、`P-RSMP`、`SCI-P2-SMP-001` 等（`R-[0-9]+` 命中 `WR-001`）
  TRACEABILITY.md:268,269,276                              ← `V17NonFiniteWeightInvalid` / `V17StatusesExplicit` / `V17NonFiniteSupportInvalid`：测试标识符里的版本代次，无测试源文件承载（见 §3）
  BUILD_GRAPH.md:13,59,66,85,92,97                          ← `<!-- BUILD-GRAPH-*:BEGIN/END -->`：**生成器的机器块定位标记**，
                                                             `gen_build_graph_doc.py:154-161` 按这两个串切块，删掉生成器就写不进去，必须保留
（`V19R3`、`<!-- 订正: ... -->`、`〔D-08〕` 已全部消除，无一命中）
```

```console
$ grep -n "docs/engineering/\*\*" docs/engineering/governance/DUAL_LINE.md
19:| shared（共享面） | `docs/science/**`、`docs/engineering/**`、shared 域点名清单 | 改动走登记，串行合并 |
22:glob 记法：... 本篇的点名清单是上表的组成部分：`docs/engineering/**` 与 `docs/science/**` 整体归 shared，
65:**残留交叉**：`docs/engineering/**` 整体归 shared 后，B 域点名清单里唯一那一条
```

```console
$ python3 - <<'PY'   # 参考文献孤儿条目
... （白名单 7 份文件）
PY
files with orphan refs: 0
```

逐份明细（改前 → 改后）：

| 文件 | 改前 refs / body / orphan | 改后 refs / body / orphan |
|---|---|---|
| `build/BUILD_GRAPH.md` | `['1','2']` / `[]` / `['1','2']` | `['1','2']` / `['1','2']` / `[]` |
| `build/BUILD_NODES.md` | `['1','2','3','4']` / `[]` / 全孤儿 | `['1','2','3','4']` / 全 4 条 / `[]` |
| `build/RELEASE.md` | `['1','2','3']` / `[]` / 全孤儿 | `['1','2','3']` / 全 3 条 / `[]` |
| `governance/DOCUMENT_GOVERNANCE.md` | `['1','2','3']` / `[]` / 全孤儿 | `['1','2','3']` / 全 3 条 / `[]` |
| `governance/DUAL_LINE.md` | `['1','2']` / `[]` / 全孤儿 | `['1','2']` / 全 2 条 / `[]` |
| `governance/TRACEABILITY.md` | `['1','2']` / `[]` / 全孤儿 | `['1','2','3','4']` / 全 4 条 / `[]` |
| `governance/UNRESOLVED.md` | `['1','2','3']` / `[]` / 全孤儿 | `['1','2','3']` / 全 3 条 / `[]` |

`docs/engineering/README.md` 无参考文献节（正文中「参考文献」一词出自写作口径句，非章节名），无需处置。

---

## 3 我推翻或改写的审稿判定

| # | 审稿判定 | 我的结论 | 依据 |
|---|---|---|---|
| R-1 | 8-11「生产构建图表 31 行 vs 实 34」 | **采纳，数字逐位一致** | 我自己跑 `cmake_graph.py` 得 `CLOSURE SIZE: 34`，漏的三行、两个手数失配全部对上 |
| R-2 | 10-5「生成器 `DOC_REL` 仍写 `docs/engineering/BUILD_GRAPH.md`（不存在）」 | **采纳并加重** | 常量在 `gen_build_graph_doc.py:33`，值确实不存在；我另外查出**第三重**失效（NONPROD 登记项含三条非根图目标），审稿只报了 fail-closed 退出这一重 |
| R-3 | 8-12「产物名四点不符」 | **采纳并补足第 5、6 点** | 审稿列了包名不符；我另核出：③ 版本位取 `VERSION` 而非 commit SHA（`:54` / `:66`）、④ 「Alpha 前无版本号」与打包器无条件拼版本相反、⑤ `SHA256SUMS.txt` 实为 `SHA256SUMS`、⑥ 归档外另有 `<归档文件>.sha256` |
| R-4 | 1-13「`docs/` 下无 `io/`」 | **改写后采纳** | `docs/engineering/io/` 在本地磁盘上**确实存在**，但 `find docs/engineering/io -type f` → 0 文件、`git ls-files` → 0 条，即空目录残留、干净克隆上不存在。审稿的「实际拓扑」结论对，「`docs/` 下无 `io/`」这句事实描述不准。订正按 git 跟踪集口径写，与 DOCUMENT_GOVERNANCE 自身的 C2 口径（只从 `git ls-files` 派生对象集）一致 |
| R-5 | 2-9「最高设计写 4 种 ⇒ 以最高设计为准」 | **采纳结论、改写理由** | 审稿预设「另 3 档在代码里真实存在」。实测代码的 `P2RejectionMethod` 有 **11 个 kernel 方法**（不是 4 也不是 7），而 `docs/science/REJECTION.md:58` 自报「7 种方法」并列出 None/Sigma/Winsorized/AveragedSigma/LinearFit/GeneralizedESD/RCR —— **该清单不含 percentile**，而同篇生产默认 profile `acsd_adaptive_pixel` 的 `4 ≤ n ≤ 5` 档正是 percentile。所以「7 种」的来源既不等于生产集也不等于 kernel 全集，审稿的「另 3 档」预设不成立。我按最高设计改成 4 种（权威链如此），另把 7 的来源问题登记到 §5 |
| R-6 | 7-1 / 10-1「白名单内也有悬空路径」 | **推翻** | 修正正则后白名单内 **0 条**缺失。审稿脚本报的 2 条是正则截断：`...version_generated.h`（真实 `.h.in`）、`module_adapters.c`（真实 `.cpp`），`ls` 均在。**保留审稿的全车道判据**，只推翻其套到本车道的部分 |
| R-7 | §2 表头「全车道缺失 51 条」 | **改写数字，保留结论** | 我用同族正则（补 `.in` 等扩展名、修掉反引号截断）跑出 **50 条**。差 1 条应属正则版本差异，不影响「判据载体失联」这个结论 |
| R-8 | 9-10「按上下文补回」 | **降级执行，判不准不猜** | 坏的是两个字面 U+FFFD。`grep -rn "稀疏控制点权" docs/` 全树**只命中这一处**，`docs/science/noise_snr/NOISE_SNR.md` 的 21 个小节里没有任何一节能对上「稀疏控制点权??卷」，`run/GOVERN-08/审核包-R2/T04T05-索引与引用收口.md:272` 也已记「疑为车道写入时损坏。未猜改」。三个独立来源都拿不到原字 ⇒ 不猜，改为显式标注不可辨识并登记 |
| R-9 | §6 `[转]` 列表含「BUILD_NODES `-G Ninja` 与 preset 合同冲突」 | **采纳并独立取证** | 我读 `CMakePresets.json` 全文，除生成器与 binaryDir 两处冲突外，另查出 `CMAKE_BUILD_TYPE` 对 VS 生成器无效、以及「两侧走同一个 preset」与两条 preset 的 `condition` 互斥相矛盾，共 5 点 |

---

## 4 需代码侧订正的问题（文档侧改不了，必须动代码或仓库配置）

| # | 事项 | 复现 | 建议落法 |
|---|---|---|---|
| **C-1** | **`.gitignore:20` 的 `build/` 未锚到仓库根**，把 `docs/engineering/build/` 四份正本整层排除 | `git check-ignore -v docs/engineering/build/BUILD_GRAPH.md` → `.gitignore:20:build/`；`git ls-files docs/engineering/build \| wc -l` → `0` | 改成 `/build/` 并补 `!docs/engineering/build/`；这是**仓库侧**动作，不属文档、也不属代码。**本车道绝对未改 `.gitignore`** |
| **C-2** | 生成器落点常量 `DOC_REL = "docs/engineering/BUILD_GRAPH.md"` 指向不存在的路径 | `grep -n 'DOC_REL = ' eng/tools/arch/gen_build_graph_doc.py` | 改为 `docs/engineering/build/BUILD_GRAPH.md`。**生成器是代码，不在本车道白名单，未改** |
| **C-3** | 生成器 NONPROD 登记册含三条非根图目标，`--out` 一律返回 1 | `python3 eng/tools/arch/gen_build_graph_doc.py --out /tmp/bg.md` → `EXIT=1`，stderr `NONPROD 登记项不在根构建图: acsd-stage2` | 三条目标由 `lib/algorithms/coverage/CMakeLists.txt:94,110,115` 声明，但该目录未被根 `CMakeLists.txt` 的 `add_subdirectory` 纳入。应把这三条从 NONPROD 移到 NONROOT，或改声明位置 |
| **C-4** | 生成器 NONPROD/NONROOT 的理由字面量自带机械锚 | `gen_build_graph_doc.py:48,49,60,62` 写 `ARCHITECTURE §1 迁移冻结` | 生成器一旦能跑，会把这四行覆盖成 `§1` 形态，与 AGENTS §5「不使用见 §几式跳转锚」冲突。**代码侧修字面量**，否则重导后文档又会退回机械锚 |
| **C-5** | `--fault-inject` 全仓无执行器 | `grep -rn -- "--fault-inject" docs/ eng/ lib/` → 仅 2 处提及；`find eng lib -name "*fault*"` 无执行器 | 补执行器，或由负责人裁定删除该判据。文档侧已降级为「目标态、不得作为已生效门的证据」，**不再写成已生效** |
| **C-6** | `TRACEABILITY.md` 里三个 `V17*` 测试标识符无测试源文件承载 | `grep -rn "V17NonFiniteWeightInvalid\|V17StatusesExplicit\|V17NonFiniteSupportInvalid" lib/ eng/` → 4 处，全在 `lib/algorithms/integration/README.md` 与 `eng/tools/quality/v19r3_traceability.py`，**无 `.cpp`/`.h`/`.c`** | 测试源已不存在，标识符只剩文档与一个工具在用。要么补测试源，要么由测试车道统一改名并同批改 `v19r3_traceability.py` 的比对列表；本车道只去掉散文里的版本代次，未动标识符本身（改标识符会与该工具失配） |
| **C-7** | 复跑链整体不可复跑 | C-1…C-6 任一未修，`BUILD_GRAPH.md` 的复算节就仍是「生成器跑不出这个结果」 | 顺序：C-1（入库）→ C-2/C-3（生成器能跑）→ C-4（字面量）→ 以生成器就地重导一次并以其输出为准 |

---

## 5 需权威补充 / 需负责人裁决的问题

| # | 事项 | 分歧 | 需要的输入 |
|---|---|---|---|
| **A-1** | `UNRESOLVED.md` 与 `run/` 审核包 UNRESOLVED 的**编号空间优先级** | 两者各有 `## n` 与 `裁-n` / `核-n` / `ENG-Bn`，形同而义不同；审稿指出「双编号空间无优先级」 | 已在文件内写明「本文件的编号空间只服务本目录，不与 `run/` 下审核包混用」；**是否还需要跨文件优先级，由负责人裁定后补写** |
| **A-2** | `UNRESOLVED.md:41` 两个不可辨识字符的原字 | `稀疏控制点权〔两字符不可辨识〕卷`；全树与审核包均无第二处可对照 | 属主提供原句；本车道**不猜** |
| **A-3** | 排异算法数「7 种」的来源 | 最高设计 5.5 = **4 种**（生产集）；代码 `P2RejectionMethod` = **11 个 kernel 方法**（`rejection.h:45–67`）；科学正本 `REJECTION.md:58` 自报 **7 种**且列出的清单**不含 percentile**，而同篇生产默认 profile 的 `4 ≤ n ≤ 5` 档正是 percentile | 三处须同批收敛。`REJECTION.md` 属科学车道，本车道只按权威链把追溯台账改成 4 种并保留指向 `docs/science/REJECTION.md`；**科学侧那份 7 的清单请科学车道复核** |
| **A-4** | `DUAL_LINE.md` B 域与 shared 的残留交叉 | `docs/engineering/**` 整体归 shared 后，B 域唯一那条 `../testing/VALIDATION_EVIDENCE.md` 仍两线交叉 | 归属裁决。已在文件内标为「待裁」，未擅自划线 |
| **A-5** | `BUILD_GRAPH.md` 非生产闭包面里 `acsd-stage2` / `calibrated_pair_diag` / `rejection_cli` 三行 | 本节抬头断言「target 真实存在于根构建图」，这三条不在（它们所在目录没被 `add_subdirectory` 纳入） | 随 C-3 一并由代码侧处置；文档侧已把该断言的适用范围写明 |
| **A-6** | `docs/engineering/{abi,cpu,io,observability}/` 四个空目录 | 本地磁盘存在、git 跟踪集为 0，属空目录残留 | 删目录或纳入跟踪，由仓库侧决定。**本车道无权删目录**（白名单只含 `*.md`） |

---

## 6 我否决 / 未做的事（诚实边界）

1. **未改 `.gitignore`**（C-1）。审核员给了改法，但 `.gitignore` 不在本车道白名单，我照做就是越权。改为在 `docs/engineering/build/README.md` 如实登记并给出复现命令。
2. **未改生成器**（C-2/C-3/C-4）。生成器是代码。我没有为了「让复算节看起来能跑」而去改 `DOC_REL` 或 NONPROD 登记册，也没有删掉那三行失真登记来让 fail-closed 通过 —— 删登记行等于消掉真问题，违反红线 3。
3. **未手编造任何指纹**。34 行里的每个 `sources` 数字与 `src_fingerprint` 都由我跑出的 `cmake_graph.py` 直接产出，渲染用生成器自己的 `md_table()`，不是我手打的表格。
4. **未猜补 U+FFFD**（见 R-8）。
5. **未删真内容消问题**。DUAL_LINE 的「残留交叉」用登记句解决而不是删掉那条点名；UNRESOLVED 的「7 种」类内容一条未动；`V17*` 标识符保留原样只去掉散文里的版本代次。
6. **未动 `V19R3` 之外的合法合同 ID**。`SCI-P1-WR-001`、`ARCH-001` 等是合同 ID 命名空间的合法内容（TRACEABILITY.md:160 自订该命名空间豁免流水号形态），正则误配不算缺陷。
7. **未改 BUILD_GRAPH 的机器块标记**。`<!-- BUILD-GRAPH-*:BEGIN/END -->` 看着像 HTML 注记，但它是生成器的切块锚点（`gen_build_graph_doc.py:154-161`），删掉生成器就无法就地重导。**审稿 9-11 的 HTML 订正注记与这类结构标记不是一回事**，前者已删、后者保留。
8. **未编译、未运行任何二进制**。BUILD_GRAPH 的闭包结论来自源码解析（`cmake_graph.py` 是纯 Python 解析器，不 configure、不编译），打包器结论来自读源文件字符串，preset 结论来自读 JSON。三者都不是运行读数 —— 这是我的诚实边界。
9. **未取任何外部文献原文**。本车道没有新增外部引用，参考文献里全是内部文档，故本轮无文献核对动作。
10. **未改 `docs/engineering/standards/COMMENT.md`**（不在白名单，且是别人正在建的文件）。只在 README 登记了路径与一句说明，**没有读它的正文、没有替它写内容**。
11. **本 README 只有一张目录表**，审稿/派单提到的「contracts/standards 表」在该文件里不存在。COMMENT.md 只登记在目录表的 `standards/` 行内；若后续要单列 contracts 表，需先有那张表。

---

## 7 自证段

### 7.1 改动范围核对

本车道只写了 7 个文件，全部在白名单内：

```console
$ git -c core.quotepath=false diff --stat -- docs/engineering/README.md docs/engineering/governance/
 docs/engineering/README.md                         |  5 ++--
 docs/engineering/governance/DOCUMENT_GOVERNANCE.md | 14 +++++++---
 docs/engineering/governance/DUAL_LINE.md           | 21 +++++++++------
 docs/engineering/governance/TRACEABILITY.md        | 30 ++++++++++++----------
 docs/engineering/governance/UNRESOLVED.md          | 24 +++++++++--------
 5 files changed, 58 insertions(+), 36 deletions(-)
```

外加 `docs/engineering/build/` 下 4 份（该目录被 `.gitignore` 排除，git 看不到，`wc -l`：
`BUILD_GRAPH.md 139`、`BUILD_NODES.md 100`、`RELEASE.md 139`、`README.md 15`）。

工作树里另有大量 `docs/` 改动与 `COMMENT.md` / `ERROR_MODEL.md` 两个新文件，**都不是本车道产生的** ——
是父代理并行派出的其他子代理留下的。判别依据：本车道全部写操作都经 `write`/`edit` 工具或
以字面路径为目标的 `python3` 脚本，脚本内每条 `P = pathlib.Path(...)` 都锁定在上列 7 个路径上。
**本车道未新建任何文件**（`/tmp` 下的临时脚本不入库、已删）。

### 7.2 复算链的完整复现序列

```console
$ git check-ignore -v docs/engineering/build/BUILD_GRAPH.md
.gitignore:20:build/	docs/engineering/build/BUILD_GRAPH.md
$ git -c core.quotepath=false ls-files docs/engineering/build | wc -l
0
$ sed -n '20p' .gitignore
build/
$ ls docs/engineering/BUILD_GRAPH.md
ls: 无法访问 'docs/engineering/BUILD_GRAPH.md': 没有那个文件或目录
$ python3 eng/tools/arch/gen_build_graph_doc.py --out /tmp/bg.md ; echo EXIT=$?
EXIT=1
NONPROD 登记项不在根构建图: acsd-stage2（删掉该行或改对名字）
$ python3 <调 cmake_graph.py 的 parse/production_entry/production_closure/source_fingerprint>
ENTRY: acsd
CLOSURE SIZE: 34
TOTAL TARGETS IN ROOT GRAPH: 62
（34 行逐行输出 = 写进 BUILD_GRAPH.md 机器块的内容）
```

### 7.3 打包器与 preset 的取证

```console
$ grep -n "ACSD-Linux\|tar.zst\|pkg_name_base\|VERSION_FILE" eng/tools/make_linux_release.py
5:   ACSD-Linux-amd64-<X.Y.Z-alpha.N>.tar.zst
54:    base = open(VERSION_FILE, ...).read().strip() ... else "0.10.0-alpha.2"
58:    pkg_name_base = f"ACSD-Linux-amd64-{base}"
150:    if args.tar_gz or shutil.which("zstd") is None:
151:        arch = os.path.join(args.out, f"{pkg_name_base}.tar.gz")
155:        arch = os.path.join(args.out, f"{pkg_name_base}.tar.zst")
$ grep -n "ACSD-Windows\|pkg_name_base\|arch = " eng/tools/make_windows_release.py
5:   ACSD-Windows-amd64-<X.Y.Z-alpha.N>.zip
66:    base = open(VERSION_FILE, ...).read().strip() ... else "0.10.0-alpha.2"
70:    pkg_name_base = f"ACSD-Windows-amd64-{base}"
165:    arch = os.path.join(args.out, f"{pkg_name_base}.zip")
$ python3 -c "import json; [print(p['name'], p.get('generator'), p.get('binaryDir')) for p in json.load(open('CMakePresets.json'))['configurePresets']]"
base-msvc                   Visual Studio 17 2022   ${sourceDir}/build/win-msvc-17.14.39-x64
win-msvc-17.14.39-x64       None                   None
linux-control               Unix Makefiles          ${sourceDir}/build/linux-control
```

### 7.4 目录拓扑的取证

```console
$ git -c core.quotepath=false ls-files docs/engineering | sed 's|/[^/]*$||' | sort -u
docs/engineering
docs/engineering/api
docs/engineering/api/abi
docs/engineering/architecture
docs/engineering/contracts
docs/engineering/data
docs/engineering/governance
docs/engineering/resources
docs/engineering/resources/cpu
docs/engineering/resources/observability
docs/engineering/standards
docs/engineering/testing
$ for d in abi cpu io observability; do echo "$d files=$(find docs/engineering/$d -type f | wc -l) tracked=$(git -c core.quotepath=false ls-files docs/engineering/$d | wc -l)"; done
abi files=0 tracked=0
cpu files=0 tracked=0
io files=0 tracked=0
observability files=0 tracked=0
```

### 7.5 编码损坏的取证

```console
$ python3 -c "t=open(p,encoding='utf-8').read(); ..."     # 文件是合法 UTF-8
文件能以 strict utf-8 解码：是
REPLACEMENT CHARS count: 2
  at char 5182 '²`，science 分册的稀疏控制点权??卷 验证证据标准的纪律一节a 第 3 条'
$ 原始字节: b' \xe5\x88\x86\xe5\x86\x8c\xe7\x9a\x84\xe7\xa8\x80\xe7\x96\x8f\xe6\x8e\xa7\xe5\x88\xb6\xe7\x82\xb9\xe6\x9d\x83\xef\xbf\xbd\xef\xbf\xbd\xe5\x8d\xb7'
                                                                          ^^^^^^^^ 两个 U+FFFD
$ 改后 REPLACEMENT CHARS count: 0
$ grep -rn "稀疏控制点权" docs/ | wc -l
1        （全 docs/ 树只有 UNRESOLVED.md 这一处，无可对照的第二处）
```

### 7.6 悬空路径审计（修正正则）

审稿员的正则在本 shell 下会把 `.h.in` / `.cpp` 截断成 `.h` / `.c`，产生假阳性。修正版
（补 `.in` 等扩展名、去截断）：

```
MISSING-ALL-LANE: 50
MISSING-IN-WHITELIST: 0
  (none)
```

两条假阳性的单独否证：

```console
$ ls -la lib/infrastructure/cli/version_generated.h.in
-rw-r--r-- 1 dsh dsh 114 ... lib/infrastructure/cli/version_generated.h.in     ← 存在
$ ls -la lib/infrastructure/scheduler/src/module_adapters.cpp
-rw-r--r-- 1 dsh dsh 892584 ... lib/infrastructure/scheduler/src/module_adapters.cpp   ← 存在
```

### 7.7 本单不声称的结论

- 不声称 34 行机器块**由生成器产出** —— 生成器跑不出来，本车道是用它自己的渲染函数
  配自己脚本算出的闭包手工重导的，这一点已写进 `BUILD_GRAPH.md` 的复算节。
- 不声称 A-2 的原字、A-3 的 7 的来源、A-4 的归属、A-6 的空目录处置已有答案。
- 不声称 C-1…C-7 任何一条已被修 —— 全部是登记，代码侧与仓库侧尚未动。
- 不声称其他车道（contracts / architecture / data / standards / testing / api / resources）
  的问题已被处理；`git status` 里那些改动来自并行的其他子代理，本车道未复核也不背书。