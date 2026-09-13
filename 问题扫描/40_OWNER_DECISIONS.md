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
| ~~A-05~~ | A | **本条已撤回（前台未核验即转发的跨域声称）**：M7 复算发现四处锚**全部不可复现** —— `(?i)\bzpf\b` 在 docs/ **0 命中**（全仓仅命中本 40 文件与 cfitsio 的 tzero）；`ap_corr/apcorr/2.559` 在 docs/ 0 命中；`CALIBRATION_ALGORITHMS.md:52/:56` 现文是 F1.1/F1.2 伪代码、`DATA_SEMANTICS.md:1253` 现文是「kernel 输出之外」。真正存在并被定稿的是 `PHASE2_SAMPLER.md:189-193` 的 **kcorr 300″/600″ 档"反保守"**（M7-A-112：scale 未知→300 档被称保守，但 300 档值 < 600 档值）。若原意指光度零点，其已定稿面是 M3-C-010（ALG-PHOT:9 声明 zero_point 字段而两结构体均无） | M7 §11 退回；已向 M3 复索原始术语 | **已撤回** | 教训：跨域移交的锚也必须逐个复验（本会话第 3 次因未复验而撤） |
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

## B. 需执行 / 跨节点权限才能定案（R 层是否授权）
| B-01 | B | p1noise / astrocs_p1_noise 是否被 git tracked（两陈述不可同真：tests/unit/CMakeLists.txt:515-528「从未入库」vs ci/ctest_baseline.json:15,123-130 冻结八目标） | M3 §6① | 需 git ls-files 或干净树 configure |
| B-02 | B | snr_estimator 交付导出面是否仍含退休符号 | M3 §6② | 需 nm/dumpbin（禁构建下不可得） |
| B-03 | B | A 线把 sigma_residual=0 转 inf 写 HISS 是否运行期发生 | M3 §6③ | 需运行（该线不在根 CMake 内） |
| B-04 | B | packaging/astrocs.exe 是否暴露 p1_session_run（决定 M3-C-005 / M3-G-001 是否升 P0） | M3 §6④ | 需 --help/dlopen 探测 |
| B-05 | B | Windows CRT rename 对已存在目标是失败还是替换（同仓四处注释两两矛盾）→ 决定 L24-013-① 与 S3-006 是否为真实数据丢失 | L24 §6A | 需 Fatduck 实测一次覆盖写 |
| B-06 | B | 只改 runtime/io 时增量 CI 实际选择集（impact_map 缺口）；check_ctest_registration 补反向闭包后的候选噪声量 | L24 §6B/G、M5b | 需 ci/run.py --changed-from 干跑 + 报告模式跑一轮 |
| B-07 | B | ENOSPC/只读目录注入下 properties/manifest 写失败能否穿过上层 exists() | L24 §6C | 需故障注入 |
| B-08 | B | 发布物是否声明 longPathAware（决定 512/PATH_MAX 系缺陷定级） | L24 §6D | 需读 packaging manifest 并核 Windows 行为 |
| B-09 | B | ASan/UBSan 实跑确证 L24-001/002/009/010 的越界（现全为静态推演+算术） | L24 §6E | 需 ASan 版构建，**与禁执行令直接冲突** |
| B-10 | B | hips_core.c 是否被在途批次纳入构建（决定 MOC 溢出与 NULL 解引用两族定级） | L24 §6F | 需读构建配置或 RESCUE-V3 任务书 |
| B-11 | B | mod001 64/64、两棵命令面差异、SparseEqualsDense 实跑、FOV 出片形态、inlier 语义、p1noise 追踪、avx512 exit-77 记账、fault-injection WILL_FAIL | M5b §6 + M1a §6 + L23 R1-R6 | 需构建/ctest |
| B-12 | B | 外部标准原文核验（Paper I §3.3.3 CROTA2 符号；HiPS 1.0 hips_frame 枚举行；IVOA 响应格式 search_radius 单位） | M1a/M2b/L26 §6 | 需可达外网（多代理 fetch 受阻，已拒用记忆替代） |
| B-13 | B | 真实数据面核对（GaiaDR3/GaiaDR3SP 与外部 HiPS/MOC 产品可达性；Fatduck 侧 astropy 交叉 oracle） | M2a/L23 R4 | 需数据目录与 Windows 节点 |

| B-14 | B | **ASan/UBSan 与 Windows 覆盖写、增量 CI 选择集、导出面符号表、真实数据可达性**四类的合并申请：是否允许在**并发工作结束后**由 R 层执行**白名单命令**（仅 ctest/ls-files/nm/--changed-from 干跑，不改源码不产根目录物） | 40 文件 B-01~B-13 汇总 | 需你授权一次 |

## C. 建议新增或订正的规范条款（先立规矩，再据此追检）
| C-01 | C | **判定式维度集合 ⊇ 条款面维度集合** 机器门：宪章 §12.3 增项，校验 registry 每行判定式是否覆盖其条款引用的全部判据维度 | 依据 M3-G-002（P0，六域同犯） | 最高优先：它决定 §17.1 发布前提是否可证伪 |
| C-02 | C | **EVIDENCE 三向门**：①discover 目录采集数>0 硬门 ②测试源 basename 未注册即 FAIL ③EVIDENCE 路径必须出现在 ctest 基线/checks.json/discover 之一 | 依据 L23 元结论 + L24-003 + M2a-F-1 | 封「门恒绿」三方向空集 |
| C-03 | C | **§6.3 support/coverage 永不作 ivar/SNR 权重**应加**可机检**判据（现状仅文字禁止，代码四面违反） | 依据 M4/M3/L18/L21 四面 | 与 C-01 配合可自动发现 |
| C-04 | C | **模块 README 必备要素入 DOCUMENTATION_STANDARD**（含构建安装、偏差登记、状态词三要素），并把 MODULE-READMES 门从 5/43 扩到全量且要求合同锚真实存在 | 依据 L27-001/002/003 | 落实你「每个子库都要有 README」的要求 |
| C-05 | C | **注释溯源要求入 §12.2 并加机器门**：算法来源（引用文献/公式编号）、输入消费来源、被消费者三类至少居一，禁止「冻结 N/实际 M」型注释 | 依据 L28（待交）+ M6a-D-001（9 实例）+ §12.2 现零实现 | 你要求的第二个检查面 |
| C-06 | C | **宪章 §12.3-9（文档内引用存在性）需实现机器门**；且 anchor 门 doc_globs 必须覆盖 docs/owner 与 docs/contracts，表格裸 :NN 锚纳入语法 | 依据 M6b（133+5 处假引用；owner 锚 4/4 全漂；223 处裸锚不在门内） | 一次性止住行锚漂移 |
| C-07 | C | **依赖与许可登记门**：PUBLIC 链接的非 vendored 库（如 GSL）必须出现在 DEPENDENCIES.md 与 dependency-lock.json 并带许可证；派生目录的 LICENSE 必须覆盖上游著作权 | 依据 L25-001/002（GSL 零登记；ipv 13 文件 Siril 派生挂 MIT/自著作权） | 发布合规阻断项 |
| C-08 | C | **「同事实一处定稿」与「转述数字皆为线索」写入协议**：本轮已 5 次由下属推翻上级/叶子转述数字 | 依据 H-1/H-2（_merge/00_COORDINATION.md） | 已在 _cache/F00 落为方法记录，建议升为正式规程条款 |
| C-09 | C | **`问题扫描/` 目录本身尚未在 AGENTS.md 目录规范登记**（新增根条目须先登记并获负责人确认） | 宪章 §14.2 + AGENTS.md 目录规范 | 待你确认：登记为常驻审计域，或扫描结束后归档进 engineering/ 并移除 |

## D. 纪律偏差自报（透明记录，不掩盖）
- **D-01**：M3 自报曾对**其自身产出文件**用一次 bash（printf + wc -c 统计字符数），未读写真源、未跑构建/git/测试，随后已用 write 重建该文件；此后再未使用。前台认定：不改变任何判据，但**记为纪律偏差**并再次向全层重申「只读 = 只用 read/grep/glob」。
- **D-02**：前台自己犯了两处（F00-03 计票过期、F00-07a 误记锚点），均已撤回订正并建机器闸门（_tools/verify_anchors.js）。
- **D-03**：前台一次派发把 L21/L22/L24 同时指派给两个合并域，已重切为互斥四分并逐条下达更正令。
- **D-04**：多个叶子代理（L12/L16/L24）曾出现「宣布即将落盘但文件仍是骨架」；已用分片回传 + 反骨架令根治，并规定部分交付优于空骨架。