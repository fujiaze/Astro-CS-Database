# -*- coding: utf-8 -*-
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emit import flush

ROWS = [
    ("86", "docs/architecture/production_call_paths_stage2.c :: R5-64.md::D64-11",
     "docs/architecture/production_call_paths_stage2.c", "R5-64.md::D64-11", "S1/S2/降级",
     "ACR 在该清单里自称 `production_reachable=yes`，与同行 risk_note、同目录另一清单和最高设计 §8 三处对撞",
     "/stage2_common.h:140; docs/architecture/production_call_paths_stage2.c; docs/architecture/execution_inventory.c",
     "docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv:300（acr_kernels.cpp production/yes）与 :62-:69; docs/architecture/production_call_paths_stage2.csv:10; lib/algorithms/coverage/CMakeLists.txt:41-58; CMakeLists.txt:18; lib/algorithms/coverage/include/astro/phase2/stage2_common.h:163/178-182",
     "仍在（被点名件侧的措辞已按 DORMANT 归一，对撞移到同目录另一清单与构建闭包本身）",
     "被点名行的措辞确已订正（不计为已修，因实质对撞未消）：`production_call_paths_stage2.csv` 的列表头是 `entry_symbol,config_gate,target,source_symbol,execution_mode,diagnostic_field,test_id,notes`（**无** `production_reachable` 列），其 ACR 行 `:10` 现读「acr_routing(DORMANT，非生产) … DORMANT（保留源码与隔离测试；生产构建/加载/路由/benchmark/发布不含 ACR/CUDA，最高设计 §8）…」，`git show c4af4136` 对该行只把「禁止编译或链接 ACR 源文件」改为「编译链接面只含非 ACR 源文件」（正向化，非事实订正）。三处对撞现读如下：①同目录另一清单仍自称生产可达 —— `PRODUCTION_EXECUTION_INVENTORY.csv:300`「openmp_kernel,acr_kernels.cpp,lib/algorithms/coverage/src/acr_kernels.cpp,**production,yes**,Phase1/2,…」（列义见该件 `:1` `category,symbol,location,classification,production_reachable,phase,…`），而同件 `:62-:69` 的 ACR 例子/测试/基准一律 `tool,no,非发布目标` ⇒ 同一件内 ACR 两组行给出相反的 `production_reachable`；②`execution_inventory.csv:12`（Integration）仍写「ACR path frame/chunk 872/866」「ACR block(if on) shares out_* buffs」、`:13`（Rejection）写「ACR kernel same interface acr_kernels.cpp:161」⇒ 运行期叙述仍以 ACR 为在图分支；③最高设计 §8 的口径被两处引为正本 —— `stage2_common.h:178-182`「ACR-IVAR-001…对本仓**恒成立** ⇒ 本函数恒 false（ACR 块生产不可达；ASTROCS_DESIGN.md §2「纯 CPU 生产，ACR 生产不可达」）」、`docs/ci/03_GATES.md:20` 与 `docs/architecture/THREAD_BUDGET_ARCH.md:36`「ACR 与浏览器层 dormant/not-shipped(ACR 不接入)」⇒ 与①的 production/yes 互斥。且①并非登记笔误而是构建事实：`lib/algorithms/coverage/CMakeLists.txt:41-58` 的生产库 `add_library(phase2 STATIC …)` **无条件**列入 `src/acr_kernels.cpp`、`../../infrastructure/acr/api/kernel_registry.cpp`、`../../infrastructure/acr/scheduler/device_executor.cpp` 与 `$<$<PLATFORM_ID:Windows>:…/cuda/cuda_bridge_loader.cpp>`，而根 `CMakeLists.txt:18` 的 `option(ASTROCS_ENABLE_ACR 「Build dormant ACR tree (default OFF, production exclusion)」 OFF)` 在 `lib/algorithms/**` 内 **0** 处引用（全仓 13 个文件的命中限于根 `CMakeLists.txt`、`CMakePresets.json`、`eng/cmake/toolchain/verify_toolchain.py`、`eng/tools/check_legacy_exit.py`、`lib/infrastructure/acr/**` 与若干文档/台账）⇒ 该选项对 phase2 的 ACR 源集不起作用，「生产排除」在链接面不成立；`lib/infrastructure/acr/CMakeLists.txt:24-26` 的自述只保证「显式 ON 仍不引入 ACR **target**」，管不住别的生产库以**源文件路径**直取 `acr/**` 的 .cpp；在册休眠守卫 `lib/infrastructure/acr/ci/check_acr_dormant.py:108` 的判据是 `option\s*\(\s*ASTROCS_ENABLE_ACR` 一类的文本检索 ⇒ 亦测不到该形态。运行期另有开关：`stage2_common.h:163` `std::string acr_route = 「auto」;` ⇒ 路由配置项仍在生产配置面。",
     "高（三处对撞与链接闭包均现读；未跑构建，闭包判定按 CMake 文本）",
     "是（S2）：要么把 ACR 源集真正置于 `ASTROCS_ENABLE_ACR` 守卫之下（生产库不含 acr_*），要么按实测链接面承认「编译/链接可达」并把 §8 的「生产不可达」限定为「运行期恒不路由」；同时统一两份清单对 ACR 的 `production_reachable` 取值"),

    ("87", "docs/ci/03_GATES.md :: 质量-标准CI与验证.md::Q-D5-06",
     "docs/ci/03_GATES.md", "质量-标准CI与验证.md::Q-D5-06", "S1/S2/降级",
     "[S1] `03_GATES.md §2` 整表复制 `ASTROCS_DESIGN.md §12.5`，复制时把 `VERIFIED` 的适用平台写窄为「Windows x64」——下级副本比「唯一口径」正本更严，Linux amd64 真实数据终验从此词消失",
     "ASTROCS_DESIGN.md:746; AGENTS.md; docs/ci/03_GATES.md:13-19; docs/ci/02_PIPELINE.md:69",
     "docs/ci/03_GATES.md:11-12（自称唯一口径）/:18（写窄行）; ASTROCS_DESIGN.md:790-800（§12.5 正本，:797 无平台括号）与 :705-706（两平台交付面）; README.md:105; docs/KNOWN_LIMITATIONS.md:13; docs/owner/RELEASE_STATUS.md:18",
     "仍在（并已扩散到四处）",
     "副本仍写窄且仍自称一致：`03_GATES.md:11-12`「## 2. 状态语义（**唯一口径**，与最高设计 §12.5 一致）」，`:18`「| VERIFIED | 正式平台**（Windows x64）** + 真实数据验收通过 | 负责人触发复验，非自动 |」；正本 `ASTROCS_DESIGN.md` §12.5 标题即「### 12.5 状态阶梯（**唯一口径**）」（`:790`）且其同一条为 `:797`「| VERIFIED | 正式平台 + 真实数据验收通过 |」——**无平台括号**。正本的「正式平台」是双平台：交付面表 `:705`「| Windows 10+ amd64 | `acsd.exe` | …」、`:706`「| Linux amd64 | `acsd` | …」（另 `:487`「全部命令由唯一可执行程序提供：Windows 为 acsd.exe，Linux 为 acsd」）⇒ 副本把 VERIFIED 的适用平台集合改窄为单一 Windows，Linux amd64 腿在「真实数据终验」这一最高状态词里从此无位置，而下级文档无权改写正本口径（口径唯一）。该窄写已被复制扩散：`git grep -n 「正式平台（Windows x64）」` ⇒ 4 文件命中 —— `README.md:105`、`docs/KNOWN_LIMITATIONS.md:13`（「Linux 腿全绿不等于 Windows 绿，Windows 腿未实测」）、`docs/ci/03_GATES.md:18`、`docs/owner/RELEASE_STATUS.md:18`（「Fatduck/真实数据证据（**当前无此项**）」）。历史：`git log -- docs/ci/03_GATES.md` 最新 `c4af4136`(09-25) 的 ± 行只改了 §4 标题与两处否定式措辞（「禁止事项→判据边界」等），`:18` 与 `:11` 未动 ⇒ 非整改。",
     "高",
     "是（S2，口径唯一）：`:18` 删去平台括号（按 §12.5 逐字），若要表达「Windows 腿未实测」应在状态表之外用具名平台状态（每平台一行）表达；四处扩散点一并归一，避免下级副本比正本更严"),

    ("88", "docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md :: 质量-合同与接口.md::Q-合同与接口-09",
     "docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md", "质量-合同与接口.md::Q-合同与接口-09", "S1/S2/降级",
     "[S1] DATA-002 的 NaN/无效处置：同一条规则内「不合格样本」有三种范围，强制计数出现四个字段名且「权重非正」这一类无承载面",
     "docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md",
     "docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:129/:154-157/:166-167/:185/:188/:202; docs/contracts/DATA_ARTIFACTS.md:38; docs/contracts/DATA_SEMANTICS.md:2482; lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:288; lib/algorithms/coverage/src/sky_plane.h:364",
     "仍在（两半存、一半收窄）",
     "四个字段名并存现读（`git grep -o 「n_rejected[a-z_]*」 … | sort | uniq -c`）：`6× n_rejected`、`4× n_rejected_nonfinite`、`2× n_rejected_nonfinite_variance`、`1× n_rejected_nonfinite_total` ⇒ 同一条「强制计数」规则族里一名四写。其中 `:166-167` 要求单字段带三类原因：「每个输出像素**必须**同时暴露被剔除样本的计数 `n_rejected_nonfinite`（按原因分类：值非有限 / 方差非有限 / **权重非正**）」，而 `:188` 又要求方差类另计一字段「`V_j` 非有限 ⇒ …计入 `n_rejected_nonfinite_variance`」⇒ 同一原因既被并进一个字段又被单列，字段级不可核。「不合格样本」三范围现读三处同页：`:154`「样本级掩膜（只作用于不合格样本）：不合格样本（**值非有限**）」、`:129`/`:188`「**方差面损坏**：该样本按不合格样本剔除并计入 `n_rejected_nonfinite_variance`」、`:176-177`「**帧间集成**：某帧在该像素的输出非有限时，该帧作为**候选被剔除并计数**」（样本级 ⇒ 帧级 ⇒ 像素级三种范围共用一词，且 `:185` 再给一种「该源像素在全部帧都非有限 ⇒ `n_rejected>0`」）⇒ 范围漂移未收口。第三半（「权重非正」无承载面）**收窄为具体缺口**：实现侧确有该原因计数（`drizzle_engine.cpp:288` `int64_t rejected_nonpositive_weight = 0; // 权重非有限或 ≤0`，`:309` 聚合），合同侧承载面却只有单值平面 —— `DATA_ARTIFACTS.md:38` 与 `DATA_SEMANTICS.md:2482` 定义的 `n_rejected_nonfinite` 是「逐像素被剔除样本计数（int32、W×H）」无原因分解面，`docs/standards/NUMERIC_STANDARD.md`/`lib/algorithms/coverage/**` 亦无 `nonpositive_weight` 的具名字段（`sky_plane.h:364` 只有 `n_rejected; // 稳健迭代剔除的点`）⇒ 「权重非正」在合同输出面上无法与另两类区分，本条按此存续上报。历史：`git log -- DATA-002…` 最新 `c4af4136`(09-25) 的 ± 行仅为否定式正向化（「禁止」改「一律不参与」等），`:166`/`:188` 的字段名与分类面未动；另 `:168`「**Phase3 承载面（已冻结，2026-09-23）**」在合同正文里写日期，与「文档内禁日期与历史」的口径不合（附带登记）。",
     "高（四名字与三范围为现读计数；实现承载面按符号检索）",
     "是（S2）：把「强制计数」收敛为一个带具名原因枚举的字段集（值非有限／方差非有限／权重非正各自可核），并区分样本级/帧级/像素级三处「不合格」用词；删除合同正文日期"),

    ("89", "docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md :: 质量-合同与接口.md::Q-合同与接口-07",
     "docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md", "质量-合同与接口.md::Q-合同与接口-07", "S1/S2/降级",
     "[S1] IO-002 用同一个词「回退」表达两件相反的事，并留下「M2b-B-01 前」的历史句 ⇒ 读者可据合同判出「允许父 order 回退」",
     "docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md:100-102",
     "docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md:102（历史句 + 只读回退）与 :29/:34/:117/:164/:205/:255/:298（禁止静默回退）",
     "仍在",
     "被点名的行本身逐字未改（`git log -- docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md` 最新 `a6f602fe`(09-24 23:49，仅项目名贯穿) ⇒ 工单后的 `c4af4136`/`5f8c237b` 未触及本件）：`:102`「  M2b-B-01 前的 `D = ipix / 10000`、`N = ipix % 10000` 与标准相反，现仅作**只读**回退；」——同句还带历史时态标记「M2b-B-01 前」与「与标准相反」的旧新对照（合同正文写历史，与「文档内禁日期与历史／只写正向约束」的口径不合）。相反二义的另一侧现读六处：`:29`「映射稳定错误码；**不做父 order 静默回退**（调用方请求 order K tile 缺失时，…）」、`:34`「归档形态必须有产品级索引，缺失即 fail-closed（不回退为扫描归档或逐瓦片探测）」、`:117`「**不做父 order 静默回退**：…」、`:164`「| 10 | `ACS_HIPS_ERR_TILE_MISSING` | tile 不存在（order-K 域内但无文件；**不做父回退**）」、`:205`「**MISSING 绝不触发父 order 静默回退。**」、`:255`「（**不返回父 tile 内容/不回退**）」、`:298`「不做父 order 静默回退；也不提供显式父 tile 定位/层级回退接口（v1 最小合同…）」⇒ 「回退」在 :102 是「允许（只读、旧命名方案）」、在 :29/:117/:164/:205/:255/:298 是「禁止（层级/父 order）」，同词两义且相反；`git grep -c 「M2b-B-01」 -- 「*.md」` ⇒ 2 命中（本件 `:102` + `docs/contracts/DATA_SEMANTICS.md`）⇒ 历史句尚未从合同面清除。读者若按 :102 的「现仅作只读回退」推及层级，即得「允许父 order 回退」这一与 :205 绝对禁令相反的读法。",
     "高",
     "是（S2）：把 :102 改为正向的「旧命名方案仅作地址解析兼容读取」并消词（不与层级回退共用「回退」），删去「M2b-B-01 前」历史句（该变更号的沿革留在证据/台账，合同只写现行规则）；全篇「回退」二义一次收口"),

    ("90", "docs/modules/hips_p2.md :: 质量-模块与插件.md::Q-D4-11",
     "docs/modules/hips_p2.md", "质量-模块与插件.md::Q-D4-11", "S1/S1/确认",
     "`hips_p2.md` 用 §12.5 交付状态词 `VERIFIED` 描述「登记面」，同页两次，且同句承认可执行测试 MISSING",
     "docs/modules/hips_p2.md:28-29",
     "docs/modules/hips_p2.md:28/:65; ASTROCS_DESIGN.md:797（VERIFIED 定义）; docs/ci/03_GATES.md:18; eng/tests（无该 ID）",
     "仍在",
     "被点名两行逐字未改（`git log -- docs/modules/hips_p2.md` 最新 `c4af4136`(09-25) 只对本件做否定式正向化，未触及状态词用法）：`:27-29`「→ TEST-P2-HIPS-001（**登记面**=ALG 文档 §11.4 设计冻结 **VERIFIED**，COV 先例；**可执行测试 MISSING** 归 P2-HIPS-TEST）」与 `:65`「可执行 `TEST-P2-HIPS-001` MISSING（P2-HIPS-TEST 建立）；登记面=设计冻结 VERIFIED；」⇒ 同页两次以 §12.5 的最高交付状态词描述「文档登记」，且同句自承无可执行测试。与状态词定义互斥：正本 `ASTROCS_DESIGN.md` §12.5 `:797`「| VERIFIED | 正式平台 + 真实数据验收通过 |」（其下 `:798` 特别注明「READY_FOR_OWNER_REVIEW … **不等于 VERIFIED**，也不等于发布」），`docs/ci/03_GATES.md:18` 更要求「负责人触发复验，非自动」⇒ 「设计冻结 + 可执行测试 MISSING」在任何口径下都至多是 `CONTRACT_READY`，把 `VERIFIED` 用于登记面即自证绿。可执行面缺席复验：`git grep -ln 「TEST-P2-HIPS-001」` ⇒ 12 处命中**全为文档/矩阵行**（`docs/DOCUMENT_INDEX.yaml`、`docs/algorithms/PHASE2_MOSAIC_WRITE.md`、`docs/contracts/DATA_SEMANTICS.md`、`docs/contracts/PUBLIC_API.md`、`docs/modules/hips_p2.md`、`docs/modules/registry/astrocs.phase2.write.md`、`docs/traceability/TRACEABILITY_MATRIX.{csv,json}` 等），`eng/tests/**` 与 `lib/**/tests/**` 命中 0 ⇒ 该 ID 无在册可执行判据（与 ROW 78 的追踪表全 VERIFIED 同源：文档侧登记即可称 VERIFIED）。",
     "高",
     "是（S1 口径面）：登记面状态词降为 `CONTRACT_READY`（并全仓搜同类「设计冻结 VERIFIED」用法一并订正），可执行测试缺席由 `TEST-P2-HIPS-001` 的实建（P2-HIPS-TEST）补齐后再逐档升阶"),
]

flush(ROWS)
print("batch6 ok:", [r[0] for r in ROWS])
