# 需负责人裁决与授权清单（前台汇总，跨域综合只由前台做）

- 用途：本轮扫描**不能由 Agent 自行判定**的事项集中在此，避免散落在各 findings 文件里被漏看。
- 三类：A 科学/规范口径裁决（Agent 无权改判，宪章 §1.2/§17.12）；B 需执行或跨节点权限才能定案；C 建议新增/订正规范条款。

| # | 类型 | 事项 | 证据位置 | 各域已定级别 | 前台建议 |
|---|---|---|---|---|---|

## A. 科学与规范口径裁决（择一或给新口径）
| A-01 | A | **进入生产权重的 SNR 定义**：SCI-NOISE:98 唯一授权 signal/sqrt(variance)，但实现进权重的是已退休的 (A−B)/residual_scale 质量比 | M3-A-001（P0）；docs/science/NOISE_MODEL.md::§7 | P0 成立 | 采 SCI 定义并改实现；若采实现，须按 §1.2 走宪章/SCI 变更流程并登记偏差 |
| A-02 | A | **weight_mode=2 的语义两份 FROZEN SCI 互斥**：SCI-CW:27/38/42 写 support×snr²，而 SCI-UPM-WEIGHT-001:54 + 实现（:1136/:1396）写逐像素 ivar | M3-A-002（P0） | P0 成立（两份冻结文档不可能同真） | 负责人择一；另一份改注 SUPERSEDED，且四处权威一致站 ivar |
| A-03 | A | **min_samples 冻结值 5 vs 实现 64（12.8 倍）**，且 ALG:151-153 明写「不改 SCI，以代码为准登记」= 权威层级倒置 | M3-C-003（P0）；docs/science/CALIBRATION.md::min_samples | P0 成立 | 若 64 有实测依据，须先改 SCI 再改代码；「以代码为准登记」写法应废止 |
| A-04 | A | **flat median≈1 前提**：母版生成已自带 /median（故数量级差不成立），但 SCI 前提是否仍要求「真归一」未决 | M3 降级 L05-001 P0→P1（写明升回 P0 条件） | P1 | 裁决走「补 SCI 说明」还是「实现真归一」 |
| ~~A-05~~ | A | **本条已撤回，且归因更正：不是 M3 移交，是前台合成产物（详见 F00-08，本会话第 2 次凭空生成）**：M7 复算发现四处锚**全部不可复现** —— `(?i)\bzpf\b` 在 docs/ **0 命中**（全仓仅命中本 40 文件与 cfitsio 的 tzero）；`ap_corr/apcorr/2.559` 在 docs/ 0 命中；`CALIBRATION_ALGORITHMS.md:52/:56` 现文是 F1.1/F1.2 伪代码、`DATA_SEMANTICS.md:1253` 现文是「kernel 输出之外」。真正存在并被定稿的是 `PHASE2_SAMPLER.md:189-193` 的 **kcorr 300″/600″ 档"反保守"**（M7-A-112：scale 未知→300 档被称保守，但 300 档值 < 600 档值）。若原意指光度零点，其已定稿面是 M3-C-010（ALG-PHOT:9 声明 zero_point 字段而两结构体均无） | M7 复验退回；M3 复验并**拒绝归属**（其域内唯一相关符号是 zero_point）→ 定性为前台自造 | **已撤回** | 教训：跨域移交的锚也必须逐个复验（本会话第 3 次因未复验而撤） |
| A-06 | A | **pixfrac 通量不变性分母**：S_p=B0/pf² 与网格无关（0.8→1.5625×，0.5→4×），故 §5+§7:82 与 §7:84、§11:117 三者不可能同真 | M2a-A-1（P0，缺陷在冻结合同） | P0 成立 | 裁决归一分母（A_drop 还是 A_pixel）；这是 SCI 层修改 |
| A-07 | A | **CAR/AIT 投影口径**：实现丢 CRVAL2 致 dec(CRPIX)=0（错位最大 216000 px 且北南镜像）；选项＝改映射 or 在 make 处强制 CRVAL2==0 | M1a-A-003 + B-002 + F-001（三条同根） | P0 | Paper II §2.2 已亲验（fiducial 必须映到 (0,0)）→ 建议改映射，不接受"锁死参数"式规避 |
| A-08 | A | **FOV≤20° 是否设硬门**：全仓零强制点，ALG:382-384 反称「非 make 硬门」；触门阈 **arctan(π/2)=57.5184°**（前台已用数值库复核，M1a 的 57.5194° 是第 4 位小数笔误，级别不变）；M7 另补 CAR 极点塌缩反证（M7-A-137） | M1a-C-004（P0）、M7-A-137 | P0 | 若坚持 ≤20°，需在 make 面设门并登记 DISP；若放弃，须删 SCI 条款 |
| A-09 | A | **SIP 1e-4 精度门的适用路径**：达门的两条路径（迭代反演、APx 阶 7）都不可经 36 项 A/AP ABI 消费（sip_order 硬限 [0,5]）⇒「Oracle 全过」字面真、产品口径失真 | M1a-C-001（P0）+ M1a-B-001（P0 加重事实） | P0 | 裁决该门适用于「可导出 ABI」还是「内部求解器」；现行 SCI/ALG/registry 三处需同步改写 |
| A-10 | A | **APx 是否进导出 ABI**（与 A-09 联动） | M1a 裁决 7 项之一 | — | 决定 §15:172 的验收面 |
| A-11 | A | **ivar==0 单裁语义**：NaN 传播态 vs 产品损坏；实现 variance<=0 在 nSourcePixels++ 之前 continue ⇒ 信号/覆盖/nContrib 一并静默消失 | M2a-H-2（P0）、M3 域同型 | P0 | 裁决语义后统一三处文本（头:265 / DATA:254 / ALG:119 / 实现） |
| A-12 | A | **1.4826 截断是否属 SCI-CAL §9 约束域**（文档自述偏差 <1e-12，实算 5.602e-12；MAD 常数 15 位与 4 位并存） | M3-A-008、L23 R5 | 待定 | 若属约束域即违规；否则 SCI 须写明可截断位数 |
| A-13 | A | **inlier 缓冲生效语义**（控制参数被计算被记录但不参与执行族的成员） | M1a 裁决项 | — | 与 L14-005 ORDERSEL、L09-010 weight_mode、L11-001 planned-workers 并族 |
| A-14 | A | **WCS-003-F1 是否书面结案**（注册表称「已闭环/CONFORMANT」而同树测试标「owner decision」） | M1a-B-003（P1） | P1 | 要么正式结案并登记偏差，要么改回未闭环 |
| A-15 | A | **「每个子库应有 README」目前无任何条文支撑**；DOCUMENTATION_STANDARD 缺「构建安装/偏差登记/状态词」三要素；README-DOCS 分层把模块 README 挤出 L2 层级 | L27-001/012（I_DOC_HYGIENE） | P1/P2 | 建议新增条款（见 C 组）后再据此追检 |
| A-16 | A | **module.yaml 的 entrypoint 一词两义**（DLL 导出 vs host-registry 接线），致 L27-007 类判词无法定档 | L27 §6 待复核 | — | 先做术语裁决，再复核 L27-007 |
| A-17 | A | **两套测光实现「均未登记为任何检查项」= 第五种状态**（不是四态任何一种，是「无声明主体」） | M3 A.2 表；另 F00-04 双实现 | P1 已定，状态性质待认 | 认可「无声明主体」为独立状态并要求登记 |
| A-18 | A | **`AstroCS.wiki/` 是否属现行权威**（M6a 待复核：多份文档以 wiki 为出处，但它是指向外部 wiki 的本地克隆且 gitignore） | M6a §10 | — | 明确 wiki 一律按 ARCHIVED 处理，或指定其镜像入 docs/ |
| A-19 | A | **「36/36 PASS」是否曾经真跑过**（UT-ABI 编排面与 test_secure_loader 无 TestCase 的矛盾，静态无法判定历史执行） | M6a §10、M5b | — | 需 CI 历史日志权限；否则一律按「不可证伪」定档 |
| A-20 | A | **`order_sel` 的冻结语义是否要求实现**（SCI §9a-5 冻结 `leaf_nside=2^(order_sel+9)` 未实现，两通道全部后继只有 provenance） | M6a-C-001（P0） | P0 | 要么实现，要么删条款并登记偏差；现行「被计算被记录但不参与执行」应作为禁止形态入 §12.3 |

| A-21b | A | **SCI-UPM §5 两式择一**（:46 与 :47 在 Σ 域下不可能同真）+ 连带 §3 量纲、§7 k_corr 缩放不变量、§10「禁改」条款是否降为诊断面 | M7-A-001（**P1→P0**，Σ 域读码闭环） | P0 | 与 A-02 weight_mode 一并裁决（同一族：权重语义无单一权威） |
| A-21 | A | **HiPS 写出归属未定**：`lib/infrastructure/**` 实测 **0 文件**（宪章 §8.3 点名的位置不存在），而 `lib/hips` 已成 packaging required_unit → 无人能指出"唯一写出路径"落在哪个 target | M2b-G-01（P1，事实已备齐）；宪章 §8.3 | 待裁决 | 指定唯一写出 target 并订正 §8.3 措辞；否则"唯一写出路径"无法机检 |
| A-22 | A | **DISP-HIPS-004 整改路线二选一**：①INT 层接线 `aio_publish_*`（tree hash 归属 INT）②writer 内嵌事务（tree hash 归属 AIO）| M2b-C-01（P0）+ M2a 域 IO_003 合同侧 | P0 已定 | 关键前提已实测修正：**不是能力缺口**——同库 `aio_pipeline.cpp:876-918` 临时+rename、`hiss_stream_writer.cpp:178` atomic_replace、`lib/hips/src/aio_publish.cpp:285` 都在用，**唯独 HiPS 写出未接线**；故选型只是"接哪一层"，不是"能不能做" |
| A-23 | A | **`require_valid_nside` 的 order≤29 上界是否要保留**：Górski 2005 全文"29"0 次、"power of"0 次，注册表 :136 把**工程上界冒充标准条款**；且 `npix`(:333) 已不再调用该校验 → nside=2^31 静默 uint64 回绕，`test_healpix_neighbors.cpp:224` 还把 order-31 断言为"边界内合法" | M2b-B-07（P0） | P0 | 保留则须把 DISP 与注册表措辞改为"工程约定"；取消则 npix 必须自加防溢出并补 order 上界负测 |

| A-24 | A | **约 60 个零注册测试源的处置**：确认废弃者移 archive/删除，不得留在树里充当证据（留着的 TU 既编译不了也证明不了任何事） | M8 §9-1；M8-F-004 附表 | P0 相关 | 逐域确认后批量清理，配合 C-02 的 basename 未注册即 FAIL |
| A-25 | A | **tests/cpu/baseline 三个 C 探针是否纳建**（avx2/avx512 有 UT-CPU-* 而 baseline 无） | M8 §9-2 | — | 建议纳建：baseline 是唯一"无 ISA"参照面 |
| A-26 | A | **product.json:9 SKELETON vs windows.json.in:9 IMPLEMENTED 双口径**由谁统一 | M8 §9-3 | P1 | 与 M5b 版本簇 G-04 同族，建议一并交 packaging 单一事实源 |
| A-27 | A | **基线 F-032/033/036/037 四个恒真探针**算"在册已知失败"还是判失效 | M8 §9-4 | — | 建议判失效并删除：恒真探针占基线名额会**制造虚假的覆盖率** |
| A-28 | A | **对外订正冲突**：L23 §6「linux-main 已装 scipy」与 `ci-linux.yml:125-127`（scipy 仅 linux-deep）不一致 | M8 §9-5 | — | 以 workflow 文本为准，登记 prerequisite_tools 缺口 |
| A-29 | A | **三条建议门由谁实现**（validate_registry / check_ctest_registration / check_standards_registry 三处改动，需同步 checks.json 与 docs） | M8 §9-6；对应 C-02 | — | 建议一次提交内三处同改，否则又一例"改过没人守住" |

| A-30 | A | **REVIEW_PENDING 三方矛盾**（AGENTS.md 权威文本已证实）：`check_agents_gov.py:15` **强制** AGENTS.md 保留该字面量作为要素匹配证据，而 `validate_task_ledger.py:42-43` 判其为**非法状态**、且该验证器**未登记为 CI 检查项**；AGENTS.md 自己写明"仅作历史映射、不得据此新增状态" | M6b-G-002（P0）+ 前台读到 AGENTS.md 原文后闭合 | P0 | 以 §14.5 状态机为准删字面量并改 check_agents_gov 判据，**同时**把 validate_task_ledger 注册进 ci/checks.json（否则仍是"改过没人守住"） |
| A-09b | A | **`问题扫描/` 是未登记的根目录条目**（违 AGENTS.md 目录规范「先登记并获负责人确认」；建目录本身有你的直接指令授权，缺的是登记步骤） | 前台自纠，见 _merge/00_COORDINATION.md 末节；已在协议 §0.4 免报区与核验器排除表内，不污染覆盖率 | — | A 授权登记为常驻审计域 / B 终报后归位 engineering/ 或 reports/ 并注明去向 / C 暂保持现状 |

| A-31 | A | **依赖与许可登记面三处失真（发布阻断级 P0，前台已独立复验五锚全命中）**：`CMakeLists.txt:549` PUBLIC 链入 gsl/gslcblas 进 exe 闭包，而 DEPENDENCIES.md 与 dependency-lock.json 对 gsl **命中 0**、packaging/licenses 只有 CFITSIO/nlohmann/LICENSE-INDEX；`lib/plate_solve/LICENSE` 是 MIT 而 `ipv/src` **13 个 TU 含 SPDX/Siril/Copyright 者为 0**；HEALPix **同目录 5 行距离自相矛盾**：`lib/common/healpix/THIRD_PARTY_NOTICE.md:19/:22`「未迁移邻居查询/未复制任何 GPL」vs 同目录 `healpix_core.cpp:337-341/:416-419`「移植自 Healpix_3.83（GPL-2+ 参考）」并在 `:345/:348` 内联上游 nb 表（`:395/:403` 使用）；该 TU 经 `CMakeLists.txt:265-267` STATIC 并进 exe；另该 NOTICE:14-17 以「astropy-healpix 百万点 mismatch=0」自证，而 M2b-F-01 实测该 oracle 与测试在非 run 区 0 命中且未进 CMake ⇒ 同一句无支撑出现在两处；包内 NOTICE/SBOM 系 `make_windows_release.py:101-127` 硬编码字面量（漏列 GSL/Siril、HEALPix 误指 plate_solve/LICENSE、cfitsio 标 BSD 而仓内为 NASA/USG）且 `LICENSE-INDEX.txt:7-10` 自认根无 LICENSE 而 `check_release_layout.py` 要求必含 | M8a-G-001（一条四面）+ M8a-C-006；基线 §16.2/§17.11；C-07 是配套门 | **P0** | 裁三事：①派生性质认定（Siril/HEALPix 两例）②copyleft 履行方式（随包 GPL 文本+源码承诺／兼容授权／替换求解器或改链接策略）③根 LICENSE 落地。**§17.10 要求 P0/P1=0，现状 §17.11 不可能全过** |
| A-32 | A | **`entrypoint` 一词两义且仓内无权威定义**（义A DLL 导出 / 义B host-registry 接线；GLOSSARY、docs/standards 14 份、宪章、schema、modules、contracts 全域检索无定义，schema 只有另一键名 `entrypoint_abi`，所依 15/13 号标准不在仓内）→ **它是"manifest↔registry 一致性门"的前置条件**，先建门必对同一模块给互斥结论 | M8a-I-006（术语缺口）；连带 M8a-C-002/G-006 | P2 但具阻塞性 | 建议：义A 保留 `entrypoint`、义B 另起键 `registry_descriptor_wired`，并同步 schema/README/generator |
| ~~A-34~~ | A | **【负责人已裁定：不改名，仅登记为问题】** 原议题为 18 份 agent 指令文件、仅 1 份受版本控制 —— 请裁定影子树指令文件的地位**：视为纯历史快照（则在目录规范中禁止 `run/**` 出现 AGENTS.md，或统一改名 `AGENTS.snapshot.md` 以免被 harness 当指令注入），还是视为需与根同步的活跃规范（则须先解决 §3.1/§8.1/§1.1 三处互斥）。另有 4 种长度并存（207/78/57/5 行）需一并说明 | F00-11 §5 | P1（**依它提交代码即升 P0**） | **已裁定：不处置、原地保留** → 已定稿为条目 **FD-G-001**（`findings/G_GOV_GATE/p1/FD_shadow_agents_md.md`），后续改由门处理（见 C-10），不得再改名或删除 |
| A-33 | A | **宪章 §8.4 的义务范围目录 `lib/algorithms/` 全仓 0 文件、CMakeLists 与 docs 0 引用** ⇒ 该硬义务对实际 21 个模块字面不成立 | M8a-I-001 相关；按 §1.1 登记，Agent 无改动权 | P2 | §1.2 流程订正条款措辞（改为覆盖 `lib/**` 或枚举实际模块）|

| A-35 | A | **注释溯源无条文支撑（与 A-15 同型）**：宪章 §12.2 逐字只有 L1/L2/L3 三子句 + 一句注释规则（:429），零溯源义务；义务实际在 §12.3-3（主语是实现）与 COMMENT_STANDARD:10/:15/:17。L28b 已交**可直接采用的 §12.2 追加条文草案**（三式溯源锚 / 伪锚排除 / 豁免须显式写理由 / strict 对新代码与合同头生效 + loose 为存量基线），并附实测成本：strict 现网报 **442 单元（E1 重跑得 345，差 −21.9% ⇒ 只能按量级指示 ±20-25% 引用**；根因：R-4 词表与 G-1 白名单原文以省略号收尾、未全函数化，L28b 自己两遍也差 2,827↔2,822），R-6a/b/c 三条命中面 3/8/1 处**可当天入门、零假阳**（空括号子式须加「紧邻数值常量」限定） | L28b §1、§1.5；findings/D_COMMENT/p1/L28b.md | 建议直接采纳 | 先立 §12.2 与 A-15 的 README 条文（C-04/C-05），再据此追检；两份现存注释门按 C-11 合并改造 |

## B. 需执行 / 跨节点权限才能定案（R 层是否授权）
| B-01 | B | p1noise / astrocs_p1_noise 是否被 git tracked（两陈述不可同真：tests/unit/CMakeLists.txt:515-528「从未入库」vs ci/ctest_baseline.json:15,123-130 冻结八目标） | M3 §6① | 需 git ls-files 或干净树 configure |
| B-02 | B | snr_estimator 交付导出面是否仍含退休符号 | M3 §6② | 需 nm/dumpbin（禁构建下不可得） |
| B-03 | B | A 线把 sigma_residual=0 转 inf 写 HISS 是否运行期发生 | M3 §6③ | 需运行（该线不在根 CMake 内） |
| B-04 | B | packaging/astrocs.exe 是否暴露 p1_session_run（决定 M3-C-005 / M3-G-001 是否升 P0） | M3 §6④ | 需 --help/dlopen 探测 |
| B-05 | B | Windows CRT rename 对已存在目标是失败还是替换（同仓四处注释两两矛盾）→ 决定 L24-013-① 与 S3-006 是否为真实数据丢失 | L24 §6A | 需 Fatduck 实测一次覆盖写 |
| ~~B-06~~ | **已答且被证伪（E2 用真代码跑）**：两个 workflow **都不传 `--changed-from`**（`ci-linux.yml:141` = `python3 ci/run.py --profile …`；`ci-windows.yml:110` 同）⇒ **增量选择在当前 CI 路径上根本不生效**（`--plan-only` 实测 selected_count=104 全量）。
  E2 未跑真 CLI（`ci/run.py:227` 的 `git()` 无 `--no-optional-locks`、`:460` 跑 `git status --porcelain` ⇒ **会刷新 `.git/index` 与并发 agent 冲突**），改用 `importlib` 载入原模块、只注入预取改动集、`select_checks`/`_match_prefix`/`impact_map` **全走原函数**：只改 `runtime/io`(4 文件) → **21/104**，而 `tests/io/test_fits_stream_contract.py:24`、`test_hips_input_contract.py:36` 逐字硬引用 `runtime/io/{fits_core.c,hips_core.c}`（2665 行）
  ⇒ **改这两个 C 文件恰好不触发唯一编译/装载它们的 UT-IO，也不触发任何 CTEST-*** |
| B-06 | B | 只改 runtime/io 时增量 CI 实际选择集（impact_map 缺口）；check_ctest_registration 补反向闭包后的候选噪声量 | L24 §6B/G、M5b | 需 ci/run.py --changed-from 干跑 + 报告模式跑一轮 |
| B-07 | B | ENOSPC/只读目录注入下 properties/manifest 写失败能否穿过上层 exists() | L24 §6C | 需故障注入 |
| B-08 | B | 发布物是否声明 longPathAware（决定 512/PATH_MAX 系缺陷定级） | L24 §6D | 需读 packaging manifest 并核 Windows 行为 |
| ~~B-09~~ | **已答（E3 挖既有证据，无需新构建）**：四条里 **L24-001/002 的 ASan 构建条件已具备、但从无针对其触发面的运行记录** —— `build/asan` 是 `ASTROCS_ENABLE_SANITIZERS=ON` + `-fsanitize=address,undefined -fno-sanitize-recover=all` 完整插桩树，`module_entry.c.o` 带该 FLAGS、`nm` 命中 `gaia_execute`/`json_append_escaped`、`.so` 链 `libasan.so.8`；全树 `stack-buffer-overflow` 内容级 **0 命中**。
  **缺的只是语料**：在册 fixture 生成器硬编码 `spectrumCount=343`（真实合法值）、负向仅 truncate ⇒ 触发面从未执行；08-09 V4 那轮全套 ASan/UBSan/LSan 跑的是**真实 DR3SP**（files=20 / sources=219165266 / GAIA_SANITIZE_OK）⇒ **通过≠缺陷不存在**。
  **L24-009/010 连构建接线都不存在**（`aio_ahpx_reader.cpp` 真源零 CMake 接线、asan `build.ninja` 0 命中；v19r3 曾编进 `aio_san` 但驱动不调用 = **编译在场、执行缺位**），牵出 `ahpx/DEPRECATED.md`（tracked、07-13 自述废弃不入生产）⇒ **影响面须按死代码复核**。
  树内 sanitizer 执行史 **11 批**，反复抓到本仓真 bug（akima 越界、`p2002 rejection.cpp:1185` 越界、gaia LSan 泄漏 294912B、2 处 SEGV）⇒ **空白≠能力空白**。**最小真跑成本已量化**（既有 asan 树增量重编 gaia 段 + 造 2 个语料 + 定 LSan 豁免策略）；**它需真编译+运行，超出当前 B-14 白名单，是否授权请你拍** |
| B-09 | B | ASan/UBSan 实跑确证 L24-001/002/009/010 的越界（现全为静态推演+算术） | L24 §6E | 需 ASan 版构建，**与禁执行令直接冲突** |
| B-10 | B | hips_core.c 是否被在途批次纳入构建（决定 MOC 溢出与 NULL 解引用两族定级） | L24 §6F | 需读构建配置或 RESCUE-V3 任务书 |
| B-11 | B | mod001 64/64、两棵命令面差异、SparseEqualsDense 实跑、FOV 出片形态、inlier 语义、p1noise 追踪、avx512 exit-77 记账、fault-injection WILL_FAIL | M5b §6 + M1a §6 + L23 R1-R6 | 需构建/ctest |
| B-12 | B | 外部标准原文核验（Paper I §3.3.3 CROTA2 符号；HiPS 1.0 hips_frame 枚举行；IVOA 响应格式 search_radius 单位） | M1a/M2b/L26 §6 | 需可达外网（多代理 fetch 受阻，已拒用记忆替代） |
| B-13 | B | 真实数据面核对（GaiaDR3/GaiaDR3SP 与外部 HiPS/MOC 产品可达性；Fatduck 侧 astropy 交叉 oracle） | M2a/L23 R4 | 需数据目录与 Windows 节点 |

| B-14 | **已授权（负责人 2026 裁定）** | **ASan/UBSan 与 Windows 覆盖写、增量 CI 选择集、导出面符号表、真实数据可达性**四类的合并申请：是否允许在**并发工作结束后**由 R 层执行**白名单命令**（仅 ctest/ls-files/nm/--changed-from 干跑，不改源码不产根目录物） | 40 文件 B-01~B-13 汇总 | 需你授权一次 |

## C. 建议新增或订正的规范条款（先立规矩，再据此追检）
| C-01 | C | **判定式维度集合 ⊇ 条款面维度集合** 机器门：宪章 §12.3 增项，校验 registry 每行判定式是否覆盖其条款引用的全部判据维度 | 依据 M3-G-002（P0，六域同犯） | 最高优先：它决定 §17.1 发布前提是否可证伪 |
| C-02 | C | **EVIDENCE 三向门**：①discover 目录采集数>0 硬门 ②测试源 basename 未注册即 FAIL ③EVIDENCE 路径必须出现在 ctest 基线/checks.json/discover 之一 | 依据 L23 元结论 + L24-003 + M2a-F-1 | 封「门恒绿」三方向空集 |
| C-03 | C | **§6.3 support/coverage 永不作 ivar/SNR 权重**应加**可机检**判据（现状仅文字禁止，代码四面违反） | 依据 M4/M3/L18/L21 四面 | 与 C-01 配合可自动发现 |
| C-04 | C | **模块 README 必备要素入 DOCUMENTATION_STANDARD**（含构建安装、偏差登记、状态词三要素），并把 MODULE-READMES 门从 5/43 扩到全量且要求合同锚真实存在 | 依据 L27-001/002/003 | 落实你「每个子库都要有 README」的要求 |
| C-05 | C | **注释溯源要求入 §12.2 并加机器门**：算法来源（引用文献/公式编号）、输入消费来源、被消费者三类至少居一，禁止「冻结 N/实际 M」型注释 | 依据 L28（待交）+ M6a-D-001（9 实例）+ §12.2 现零实现 | 你要求的第二个检查面 |
| C-06 | C | **宪章 §12.3-9（文档内引用存在性）需实现机器门**；且 anchor 门 doc_globs 必须覆盖 docs/owner 与 docs/contracts，表格裸 :NN 锚纳入语法 | 依据 M6b（133+5 处假引用；owner 锚 4/4 全漂；223 处裸锚不在门内） | 一次性止住行锚漂移 |
| C-07 | C | **依赖与许可登记门**：PUBLIC 链接的非 vendored 库（如 GSL）必须出现在 DEPENDENCIES.md 与 dependency-lock.json 并带许可证；派生目录的 LICENSE 必须覆盖上游著作权 | 依据 L25-001/002（GSL 零登记；ipv 13 文件 Siril 派生挂 MIT/自著作权） | 发布合规阻断项 |
| C-08 | C | **「同事实一处定稿」与「转述数字皆为线索」写入协议**：本轮已 5 次由下属推翻上级/叶子转述数字 | 依据 H-1/H-2（_merge/00_COORDINATION.md） | 已在 _cache/F00 落为方法记录，建议升为正式规程条款 |
| C-12 | C | **证据产物时效性门**：E1 实测 `build/lib/snr_estimator/astrocs_p1_noise.so`（09-11 遗留）在**当前根配置里已不产生该 target**（CMakeCache 0 引用），
  但 `nm -D` 仍能读出导出面 ⇒ **任何以 build 产物为证据的检查都可能对"已经不存在的交付物"下结论**（本次是假否：导出面只剩 `astrocs_module_query_v1`）。
  建议：凡以二进制为证据的门必须先断言「该 target 仍在当前配置图中」（比对 `CMakeCache.txt`/`cmake --build --target list` 与产物路径），否则 FAIL；
  并把「build 树内存在当前配置不产出的产物」本身列为一条可报告项（与 M5b-G-01 同名第二二进制、L27-006 required_unit 不同源 同族） | E1 回执 §④ | 高（它会让"实测"给出假否） |
| C-10 | C | **agent 指令文件的可见性与一致性门**：全仓 18 份 AGENTS.md 仅 1 份被跟踪；影子版明文写「正式运行只有 orchestrator.exe」「toolchain.ps1 run 是唯一正式入口」「Python 生产层已删除」「wiki 是唯一权威文档」，与 §3.1/§8.1/§1.1 及交付面（bin/_astrocs.pyd 仍 required）互斥。建议：①指令文件只允许存在于被跟踪且受文档门覆盖的路径，或②新增机器门校验影子指令与根 AGENTS.md 十要素 + §1.1 分层一致（免报区出现即 FAIL）。注意与 L25-005 同根因（判据按字面目录名切分 → 换目录即逃检），须与 C-07 同批改 | F00-11 §5（当场清点，含 17 个 UNTRACKED 判定） | 最高优先（活的指令注入） |
| C-11 | C | **注释门合并改造 + 「门必须能红」判据入 §12.3**：现存 `check_comments.py`（恒真判据、文件级粒度、现网 16 触发 / 0 报警）与 `check_comment_hygiene.py`（`main():116` **无条件 return 0**、违例只写进 reports/v19r2、ci 对它 0 引用）**两份并存零拦截**；建议合并为 strict（符号级，新代码与合同头生效）+ loose（文件级，存量基线名单）两档，并把「违例必须影响退出码、0 用例/0 违例须自证、门自身须有负例自检」写成 §12.3 新项 | L28b 移交件 + M6a-G-001（P0）+ 簇 1 | 高 | 与 C-02 三向门（含第四向 failures 进退出码）同批实现，避免改一次红一次 |
| C-09 | C | **`问题扫描/` 目录本身尚未在 AGENTS.md 目录规范登记**（新增根条目须先登记并获负责人确认） | 宪章 §14.2 + AGENTS.md 目录规范 | 待你确认：登记为常驻审计域，或扫描结束后归档进 engineering/ 并移除 |

## D. 纪律偏差自报（透明记录，不掩盖）
- **D-01**：M3 自报曾对**其自身产出文件**用一次 bash（printf + wc -c 统计字符数），未读写真源、未跑构建/git/测试，随后已用 write 重建该文件；此后再未使用。前台认定：不改变任何判据，但**记为纪律偏差**并再次向全层重申「只读 = 只用 read/grep/glob」。
- **D-02**：前台自己犯了两处（F00-03 计票过期、F00-07a 误记锚点），均已撤回订正并建机器闸门（_tools/verify_anchors.js）。
- **D-03**：前台一次派发把 L21/L22/L24 同时指派给两个合并域，已重切为互斥四分并逐条下达更正令。
- **D-04**：多个叶子代理（L12/L16/L24）曾出现「宣布即将落盘但文件仍是骨架」；已用分片回传 + 反骨架令根治，并规定部分交付优于空骨架。
- **D-05**：M8 自报启动时曾两次列目录（并**明确拒绝把自己的过程描述成"全程零 shell"**）。另记 M8 与 M6b 各一次「局部 read → 全量 write」把自己档案写坏
  （M8 的 `_merge/M8.md` 一度截断为 160 行，已逐节重建至 244 行并自检定义数=引用数）。⇒ 协议已立禁令：**禁局部 read → 全量 write，一律整读整写或用 edit**。
- **D-06**：**E 层授权的第一例副作用，责任在边界不在代理**。E3 按我给的白名单跑 `ctest -N`（在 `build/asan/` 下），而 `ctest -N` 在 CMake 生成的构建树里会初始化 `Testing/Temporary/` 并覆写 `LastTest.log`；
  **订正（E4 并发观察 + 前台当场复验，D-06 归因不成立）**：我先把这次覆写归到 E3/我的授权上，随后 E4 报出 **11:03–11:07 并发构建 agent 正在写 `build/`**（`build/astrocs` 11:05、`build/install_manifest.txt` 11:12、`build/linux-control/**` 一批 11:03-11:07），
  而我复验 `build/asan/Testing/Temporary/LastTest.log` 全文只有 `Start testing: Sep 14 11:04 CST` + `End testing: Sep 14 11:04 CST` **两行、零测试清单** ⇒ **不含任何 `ctest -N` 特征**（grep `169|288|Total Tests` = **0**）。
  且 E3 自述是"发现它**已经被**覆写为空记录"（并提到一个 10:29 的前序进程）。⇒ **归因不成立，我撤回"是 E3/我授权造成"的判断，改判「原因未定，最可能是并发构建 agent 的一次 ctest 运行（0 用例）」**；
  我无法用实验判定（做对照实验需 `cmake -B` 构建树，属禁项）。**但两条禁令保留**——它们的风险论证独立成立、不依赖此归因：对非自建构建树禁一切 `ctest` 形态；禁跑 `ci/run.py` 非 `--plan-only` 路径。
  **元教训（这条比原判断更有价值）**：**我把"未证实的归因"写成了"我犯的错误"并向负责人报告**。自我批评也必须过三级复核：**归因与事实不同，宁可写"原因未定"也不要把没证据的因果链写成自己的错**。已入簇 10。

---

### A-40（最高优先）新 SNR 模型的落点是否越过你的 Q-5 待裁项
- **冲突**：`设计大纲 §6.3` 末句与待确认清单 **Q-5** 明示「Phase1 SNR catalogue 现降为 **legacy 诊断**，终态去留〔问 5〕」；而 `b0353303`(P5) + `35c85f53`(P8) 实际把它**升格为承载科学 `SNR_F` 的交付量**（`snr_phot`/`median_snr`/`local_snr`/`snr_f`/`sigma_f_adu`），并与 `SCI-CW §4` 保留的 `support×snr_v²` 拼接 ⇒ **生产权重可能变成 `support×(真SNR)²`**。
- **程序面**：`A-01` 处方要求「若采实现须登记偏差」，本批走的是「**改 SCI 以纳实现**」且**无新 `DISP-*` 条目**。V1 与前台**均不判越权**（你已明示批准改冻结文档），只请终裁落点。
- **一条易被忽略的关键事实**：**P5 单独看不在根交付图**（`CMakeLists.txt:210-215` 已按 `F-CI-002-01` 解除 `add_subdirectory(lib/snr_estimator)`），且 P5 commit 自陈「Linux 二进制改前/改后 sha256 完全相同」⇒ **SNR 修正的交付闭合完全依赖 P8，复核与验收必须两半一起看**。
- **两个分支的处置**：①若裁定 SNR 是**交付量** ⇒ 必须补 `DISP-*` 偏差登记，并纠正 `CONTROL_WEIGHT_SNR.md:66/71/40/§2a` 仍把 `weights[s]=support[s]×snr_v²` 标为 `weight_mode=2` 的残留（= `M3-A-002`/`A-02` 本体，本次授权编辑"改了一半"）⇒ 才能关掉 A-02；②若裁定仍是**诊断量** ⇒ P8 写入交付 JSON 的 `snr_phot=median(SNR_F)` 需回退，且 `A-40` 与 `V1-N-04`（SCI 文本称"不产出 σ_F"而代码正产出）一并订正。
- **附带催办**：`M3-A-001`/`M3-A-002`（本域两个 P0）在 `账本/FIX_LEDGER.csv` 仍 `fix_state=OPEN` ⇒ **已改代码未挂账**。

### A-41（V4 转达，需你裁并落档）`allocator_cache_residual_bytes` 是否设上界
- 现状：该字段**全仓无任何判据**（只出现在 `memory_report.h` 与一个测试里），而 T4 证据的 `cache_residual ≈ 2.53 GB` 与真泄漏在数据上**不可区分**，字段名还过实（读起来像"已判定的缓存残留"）。⇒ **非 malloc 通道（直接 mmap / 自池 / 栈 / 大静态）的泄漏零判别力**（`V4-N-12`，P2）。
- 建议：给一个**明确上界或比值判据**，或把它降级为纯诊断字段（不进 verdict）并改名 `allocator_cache_residual_bytes_unbounded_diag`；无论哪种都须在 `RESOURCE_MONITORING_CONTRACT` 登记（现在没登记）。

### A-42（V4 转达，需你裁并落档）F-14 的 `0.5` / `32MiB` 是否随新被测量重标定
- 证据（V4 独立复算 gitignored 的 T4 `alloc_report.json`，schema 仍 v1）：`frac 0.414`（RSS 口径）对 `frac_alloc 0.92264`（分配器口径）⇒ **同一次 run 换口径前判负、换后判正，门强度降 2.23×，而 0.5/32MiB 一字未改、无出处**；`OWNER-07` 未落本档（V4 grep 0 命中）、账本亦无 B7/`c1959436` 行。
- 建议：要么按新口径重标定（给统计依据，如多轮 T4 分布的分位数），要么把阈值挂 `DISP-*` 登记为**已知偏差**并说明为何 0.5 仍适用。**注意**：此项与 `V4-N-10`（RSS 回退分支非单调、且 Windows/MSVC 正式节点恒走此分支）必须同批裁——先裁阈值会留下"单调泄漏在发布平台恒通过"。

### A-43（V4 转达）`allocated_capacity_cores` 取义 = 即 A-39，代码现自标 `NEEDS_DECISION` 未落
- 补充事实（V4 结论 2/5 与 V6 一致）：**同一文件文案仍滞留旧值** `resource_gate.h:170/:171/:395/:432` 的 `0.75/0.50/min(workers,cpus)` 属**交付面**（进 verdict JSON 与 stderr）⇒ 与实现不同源（`V4-N-02`）。请 A-39 一并裁定时要求文案同源订正。

### C. 建议新增条款（本轮新增 C-13..C-17，均来自复算为真的机制，非风格偏好）
- **C-13｜改生产代码必挂账**：任何改动 `lib/**`、`cli/**`、`include/**`、`runtime/**`、`providers/**` 的提交，必须在 `账本/FIX_LEDGER.csv` 至少有一行处置记录（新缺陷或修旧条皆可），且 `regression_test` 非空。机器化：以 `git log --name-only` 与账本 `fix_commit` 集合做差，非空即红。依据：`V6-N-10` 实测本轮 9 个改码提交账本 ref=0。
- **C-14｜跨语言/跨二进制消费的公共结构体必须自带 `struct_size` 或 `abi_version`**，且镜像侧（ctypes/cffi/struct 格式串/PInvoke）须有一条 `sizeof` 相等断言并进 CI 采集。依据：`V2-N-01` 的 72 字节越界写；`V11` 轴正在统计该族的完整规模。
- **C-15｜仓内文件的权威依据禁止指向 gitignored 工作区**（`run/**`、`logs/**`、`out/**`、`artifacts/**` 的运行产物）。要作权威必须入库。机器化一行判据：tracked 文件出现 `run/…` 形态的路径引用即红。依据：`V6-N-04` 实测 50 处新引用/14 文件，含 FROZEN SCI 的推导权威与新锁的期望值来源。
- **C-16｜禁止以文本命中代替行为断言**：`find`/`contains`/正则命中字面量即通过的"守卫"，若其参数含路径、JSON 键值对或诊断文案，一律视为无效锁；须改值断言/结构断言/行为断言或注入式反向锁。依据：`FD-F-001/F-002`、`V4-N-16`、`M8-F-009`，`V15` 轴正在给出全仓规模。
- **C-17｜门禁阈值与被测量口径必须同提交重标定并留出处**：更换被测量（如 RSS→分配器实占）而未重标定阈值、或阈值无 `DISP-*` 出处，判"修复不完整"。依据：`V4-N-13`（同一次 T4 数据两口径判定相反、门强度降 2.23× 而 0.5/32MiB 一字未改）。

### C-18｜编号生命周期（登记 → 引用 → 承载 → 退役）——V13 判定为「制度缺口即交付缺陷」
- **缺的是什么**：`contracts/schemas/traceability_matrix.schema.json` 对 `EVID` **仅格式正则**；`SPEC` 与 `docs/standards` **没有「编号须先登记后使用」也没有「EVID 须对应一件可定位证据」**。⇒ 本轮所有铸造行为（`V13-N-01/03/04/05`、`V6-N-06`、`L28e-E-001`）**在制度上无可违之规**。
- **建议条文（三条可机器执行）**：①新编号必须出现在登记面（`docs/standards/` 或 contract schema 的枚举/清单）才可被引用；②声明 `VERIFIED` 者须提供**可解析证据锚**且**引用计数 `refs>=2`**（仅在自身行内出现的 ID 判 `MISSING`）；③编号退役须在**权威视图**登记（不得只在生成物里写 SUPERSEDED，见 `V13-N-07` 的自毁实例），且**删行须有对应登记动作**（`V13-N-06`：删 4 行而 CHANGELOG/裁决/账本 fix_note 零提及）。
- **配套门（成本最低、拦面最大，建议同批做）**：`INDEX.yaml` 中 `status=ACTIVE` 的合同 ID 必须出现在 `TRACEABILITY.csv`（**ID 集对账**），缺失即红 ⇒ 现况是 `CONTRACT-GRAPH` 只验自身内部一致性，合同面与追溯面**永久分叉恒不红**。

### C-19｜交付单元 `status` 须有可执行判据，且跨平台清单由同一源派生——V14 片1 实证
- **实证**：`MOD-NOOP` 在两份清单都是 `IMPLEMENTED`，而它自身 README/`module.yaml`/`CMakeLists` 三处都是 `SKELETON` 且明写三个动词全返回 `ACS_ERR_UNSUPPORTED`；真正实现动词的 `echo` **三面零登记**（`V14-N-02`）。`PLATFORM-RUNTIME`/`PLATFORM-IO` 在 Windows 清单是 `IMPLEMENTED`，主清单是 `SKELETON` 并附言「不伪装实现完成」，实况各仅 1 个 C ABI 源（`V14-N-01`）。**同一枚举下两例判据互斥 ⇒ 缺定义，不是缺诚实。**
- **建议条文**：①`SKELETON`/`IMPLEMENTED` 给**可执行判据**（建议：三动词皆 `UNSUPPORTED` ⇒ 必为 SKELETON；任一动词真执行 ⇒ 方可 IMPLEMENTED，且须给出宿主接线证据）；②**一份 status 源派生两平台视图**（禁 Windows `.in` 与主清单各写各的）；③contract 面补 `status` 字段，令 S4 之类的清单门**能够**校验它（现 contract 无此字段，门结构上无从下手）；④与 **C-13** 合并：登记面 status 变更须有锚与挂账。
- **另附 V14-N-03 的独立建议**：两侧手写的 kernel 表须由**同一生成器**派生（禁双写），且守卫须是**逐列结构比较**而非文件名字符串命中（`FD-F-001` 的 `strstr` 守卫实测让 6/12 行 precision 漂移通过）。

### C-20（V7 片1-2 归纳，建议入宪章/标准）静默兜底的三条硬判据
- **①消费面必填键清单 = 写面清单**：typed-artifact（`p1_wcs.json`/`p1_snr.json`/`p1_sources.json`/`p2` 上游产物等）消费端不得对写端必写的键设默认；缺键 ⇒ `DATA` 拒并要求 schema 位。依据：`V7-N-03` 四连 + `V3-N-03` + `V7-N-05` 九键。
- **②兜底默认值禁止与「显式拒绝值/非法值」重合**：`weight_mode=0`（上游 integrate 显式拒绝）、`target_order=0`（`nside` 全天塌缩却过值域门）、`σ_sky=0.0`、`aperture=0`、`dark_scale=1.0` 全部违规——**0 在物理上常是合法值，正是它最危险的地方**。
- **③同一键全仓只允许一个缺省值，且判别位必须记「生效值 + 来源」而非「请求值/键存在」**：`target_order` 缺→0（p2 view）与 →−1（upm-fit）并存即违规；`sip_present`/`psf_mode`/`res.nested`/`dark_scale` 均只记声明或只记生效值的一端。依据：`V7-N-02/04/05/06/07`、`V2-N-09`、`V4-N-10`。
- **配套机器门（并入工单 E 节）**：`E8` 遍历写端产出键集与消费端 `value(k,d)`/`count(k)` 读取点做差，缺键兜底即红；`E9` 判别位字段名与来源（`*_mode`/`*_present`/`*_source`）必须在 manifest 有「生效值+来源」二元组。