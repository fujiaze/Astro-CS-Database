# RC8 分片重验档案（第 8 片 / 共 8 片）

- 时点：HEAD a3a343a44080d917089e1f8d548ed2d0400b0c61（git --no-optional-locks rev-parse HEAD 实测一致）；base 521095b8。
- 子集来源：问题扫描/_cache/recheck_round1.json -> verify[7::8]（按下标取模第 8 组），共 61 条，全部命中 问题扫描/账本/FIX_LEDGER.jsonl。
- 工作区注意：docs/TRACEABILITY.csv、reports/v19r2/*、reports/v19r3/contract_inventory.csv、evidence/v6_1_rework/tasks/CHK-001/* 在工作树为脏 -> 涉及条目以 git show HEAD:<path> 为准；lib/snr_estimator/{src,include,CMakeLists.txt}、lib/plate_solve/cpp/ipv/test/ipv_dead_params_lock.py、lib/phase1/tests/、tests/unit/p1_noise/ 为未跟踪新文件，HEAD 上不存在，不作为 FIXED 依据。
- 判定口径：四态 STILL / FIXED / MOVED / CANNOT_STATIC；一律按 文件::符号 重定位；FIXED 须给出修在哪的证据并判是否只修一站。
- 统计：STILL = _ / FIXED = _ / MOVED = _ / CANNOT_STATIC = _（收工时回填）。

## 逐条结论

### [0] L28c-E-001（P1/E_TRACE_BREAK）— 判定：待填
- 原报标题：runtime/pipeline/typed_dag_contract.h 以「编译期合同 + ABI 冒烟测试锁定」自我背书，但该头全仓零消费者、不在任何构建/测试源列表，被指派的锁不存在

### [1] M1a-A-004（P1/A_SCI_DEF）— 判定：待填
- 原报标题：cd_inv 在 SCI 符号表被定义为 CD⁻¹ 却注单位 pixel/arcsec，与同文 §5 及实现 inv(trans.linear) 相差 3600 倍

### [2] M1a-C-002（P0/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：trans 线性奇异：SCI 承诺"返回参数错误"，实现仅 warn 并恒设 success=true，可回写冒称 TAN-SIP 的退化头

### [3] M1a-D-002（P2/D_COMMENT）— 判定：待填
- 原报标题：科学链路单位注释自矛盾群（重锚定稿）：StarPoint/U 注"角秒"而实际为像素、SIP order 值域注释过期、x00 日志单位名实不符

### [4] M1a-F-005（P1/F_TEST_GAP）— 判定：待填
- 原报标题：并发修复面缺回归保护（新增条目：改过但没人守住）

### [5] M2a-A-1（P0/A_SCI_DEF）— 判定：待填
- 原报标题：SCI-DRZ-001 的 pixfrac 归一化与其自身"常数场流量守恒"不变量互斥：常数面亮度 B0 的输出为 B0/pixfrac²

### [6] M2a-C-10（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：registry 登记的 `astrocs.calibrated_frame.v1` 不满足自家 type_id 词法，该类型一经使用必被词法拒绝（登记即合法 vs 未登记即拒 互斥）

### [7] M2a-C-5（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：Gaia 模块 README 与 ALG-GAIA-001 的现状陈述整体滞后于迁移落地（无测试/无 target/无 plan-cancel-lease 三条"缺口"均已不成立）

### [8] M2a-D-4（P2/D_COMMENT）— 判定：待填
- 原报标题：Gaia 侧注释/断言簇：算法名与实现不符、扩展函数声明无对应实现、已修缺陷仍留过期弱断言与恒真断言

### [9] M2a-F-4（P1/F_TEST_GAP）— 判定：待填
- 原报标题：球面 drizzle 的注册测试面不含任何 pixfrac<1 的正面数值用例，M2a-A-1 的偏差因子无人守住

### [10] M2a-I-1（P2/I_DOC_HYGIENE）— 判定：待填
- 原报标题：SCI-DRZ-001 的符号表与验证段落进了 Gaia 极区 prune 的专用常数（C=π/2、C45=π/(2√2)），drizzle 域内无对应实现

### [11] M2b-B-04（P0/B_STD_MISMATCH）— 判定：待填
- 原报标题：HiPS 域条款锚整体错挂；properties 必需键集用自定 5 键替代标准 9 键；STD-F4 引用不存在的 obs_bandpass

### [12] M2b-E-01（P1/E_TRACE_BREAK）— 判定：待填
- 原报标题：ALG/DATA/PUBLIC_API/PHASE3 的源码行号锚系统性漂移（+25 ~ +423 行、导出符号数由 9 变 12），而 DOC-LINE-ANCHORS 门只作「范围在界内 + 声明式符号绑定」两项检查故对此完全无判据

### [13] M3-A-001（P0/A_SCI_DEF）— 判定：待填
- 原报标题：SNR 无唯一定义：进入生产权重的「SNR」实为已退休的 PSF 拟合质量比

### [14] M3-C-003（P0/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：冻结 SCI 的 `min_samples` 默认 5 与实现 64 相差 12.8 倍，ALG 径行「不改 SCI，以代码为准」= 权威层级倒置

### [15] M3-C-011（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：`sigma_residual=0` 一值三义、`fit_status` 编码冲突，且暴露一个实现完全不读取的配置字段

### [16] M3-G-001（P1/G_GOV_GATE）— 判定：待填
- 原报标题：产品 manifest 把 cosmetic 报为 `available`，但两通道给检测的 Dark/Bias 恒为 `nullptr` → 检测在配置层不可能生效

### [17] M3b-A-04（P1/A_SCI_DEF）— 判定：待填
- 原报标题：5σ 检测阈的 σ 取自原图、判决作用于 σ=2 平滑图，虚警预算不自洽

### [18] M3b-C-05（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：SDetParams 五字段中四个全仓零读取而文档称有消费面；检测阈三套并存；伪代码与实现相反

### [19] M3b-H-03（P1/H_NUMERIC）— 判定：待填
- 原报标题：FP64 通道按 uint16 满值 65535 判饱和、mag 中间量降级 float32，与合同「全程不降级」字面冲突

### [20] M4-C-05（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：采样器并发声明「并行路径不经 g_aio_mu」不实：全部 tile 读经全局锁串行

### [21] M4-F-04（P1/F_TEST_GAP）— 判定：待填
- 原报标题：tests/api UPM 确定性/恢复/接缝门永久静默跳过；驱动 calibrate_sum 恒打印 0.0，worker 等价从未被比较

### [22] M5a-C-003（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：宪章 §10.5 必采的 PSS/每线程 CPU/I/O wait 未采集，资源工件名与 §11 清单两套且未登记偏差

### [23] M5a-G-005（P0/G_GOV_GATE）— 判定：待填
- 原报标题：THREAD-BUDGET 机器门空转：扫描面、正则与行级豁免三重漏检并被单测固化

### [24] M5b-C-03（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：顶层幽灵命令 drizzle 在分发表却不在 help 与冻结命令树；未接线用 ARGS(2) 表达；诊断指向已删选项

### [25] M5b-E-04（P1/E_TRACE_BREAK）— 判定：待填
- 原报标题：acs_status/acs_head 在 legacy 与 abi 头族重复定义，其声称的机器检查文件不存在

### [26] M5b-G-06（P0/G_GOV_GATE）— 判定：待填
- 原报标题：§18.4「只加载随产品签名清单发布的官方模块」在机器上无实现：产品不加载、hash 全 null、唯一闭环测试不进 CI

### [27] M5b-G-14（P1/G_GOV_GATE）— 判定：待填
- 原报标题：导出符号与 ABI 合同一致性（§12.3-6）实际无有效检查：门只判符号表非空，Linux 侧零覆盖

### [28] M5b-I-05（P2/I_DOC_HYGIENE）— 判定：待填
- 原报标题：CI 元文档的规模计数与多项路径/条目已与注册表脱节

### [29] M6a-D-001（P1/D_COMMENT）— 判定：待填
- 原报标题：生产注释承担「未登记的裁决请求」角色：SCI 冻结 1e-4 px 门被同文件注释自证一步法数学不可达；裁决与偏差 ID 只活在注释与控制包台账里，docs 权威登记面（STANDARDS_REGISTRY / PLATESOLVE §11.3）对 DISP-WCS-007、DISP-WCS-008 零命中（来源 L13-001 的注释层残余；科学定档在 M1a）

### [30] M6a-D-009（P2/D_COMMENT）— 判定：待填
- 原报标题：§12.2 禁止形态（任务号/审计轮次/commit 哈希/修复流水）成为注释主干：Phase2/3 侧六目录实测 257 处命中 + Phase1 侧抽样清单，被登记偏差条目自身的注释也在复述历史（来源 L13-015 全域 + L14-003 量化，两切片并档）

### [31] M6a-G-001（P0/G_GOV_GATE）— 判定：待填
- 原报标题：注释卫生门 CON-COMMENTS 是空壳机器门：宪章 §12.2/§12.3-10 要求的四类判据中三类无实现，唯一有实现的判据被「冻结」二字永久豁免、每文件至多报一条、扫描面只覆盖 lib 的一部分，却恒定自报 coverage=full/1.0 —— 注释质量没有任何机器防线（来源 L14-004，根因单条定稿）

### [32] M6b-C-003（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：同一 ACTIVE 文档内冻结投影集新旧两版并存：§0 非目标仍写「不实现 SIN/ZEA/CAR/AIT」，§15 已按负责人裁决冻结 TAN/SIN/CAR/AIT

### [33] M6b-G-001（P0/G_GOV_GATE）— 判定：待填
- 原报标题：未经证据支撑的「已验证」声明（单一根因，四组实例）：八层矩阵在 TEST / EVIDENCE / SRC / 整行四个层面给出 VERIFIED，而门的结构只可能判「文件存在 + 符号 token 可见」

### [34] M6b-I-003（P2/I_DOC_HYGIENE）— 判定：待填
- 原报标题：文档内统计与清单陈述与实测不符（可复算的口径失真清单）

### [35] M7-A-123（P1/A_SCI_DEF）— 判定：待填
- 原报标题：交换合同要求三 Phase 最小平面集含「mask」，而 mask 在三个 DATA 节都被判「非产品输出」且语义三方不一（L21-007）

### [36] M7-A-131（P1/A_SCI_DEF）— 判定：待填
- 原报标题：MAD=0 时热/冷检测阈值退化为「高于中位即热、低于中位即冷」，文档两次判为良性（L20-011）

### [37] M7-E-201（P2/E_TRACE_BREAK）— 判定：待填
- 原报标题：SCI/合同/注释三处「与实现一致」声明所引代码行锚整段漂移（本代理读码亲证）

### [38] M7-I-202（P2/I_DOC_HYGIENE）— 判定：待填
- 原报标题：hips_frame 字面值在合同与写侧措辞不一致（L21-009 **按前台裁决降级定稿**）

### [39] M8-F-002（P0/F_TEST_GAP）— 判定：待填
- 原报标题：`tests/unit/io_ownership_test.cpp` 在 ctest 下**不可能失败**：唯一 CHECK 的失败计数不参与退出码，`main` 无条件 `return 0`（覆盖 L23-002 与 L23 §1 的旧措辞）

### [40] M8-F-010（P1/F_TEST_GAP）— 判定：待填
- 原报标题：修复未固化：B2-A14 新增 PHOTDEGRADE / uncalibrated_adu_allowed 显式降级路径已实现并回写文档，但无任何回归用例钉住

### [41] M8a-C-001（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：已废弃第二调度器被 README 写成「正式科学运行只有一条命令」，并整套冻结第二套 `ASTROCS_*` 退出码

### [42] M8a-E-003（P2/E_TRACE_BREAK）— 判定：待填
- 原报标题：DISP-PSF-007 被 README/manifest/测试/HANDOVER 四处当偏差 ID 使用，偏差登记表只到 006

### [43] M8a-G-007（P1/G_GOV_GATE）— 判定：待填
- 原报标题：生成器自述「checker 以本生成器输出为源」不实：registry 无 diff 门，手写页与 descriptor 双向漂移（裁决：维持 P1）

### [44] M8a-I-007（P2/I_DOC_HYGIENE）— 判定：待填
- 原报标题：toolchain.lock 已刷新，INVENTORY_REPORT 仍以现势口吻归因锁「九类工具全部缺失」— 修复未回写型失真

### [45] M9-D-1（P2/D_COMMENT）— 判定：待填
- 原报标题：drizzle 局部尺度推导注释把"量纲为一的切平面坐标"表述为"仍是弧度量级小值"，注释表述失准而实现结果正确

### [46] M9-G-5（P1/G_GOV_GATE）— 判定：待填
- 原报标题：IO-001 原子写提交点全程无耐久性屏障：rename 前只 fflush 不 fsync，目录项亦不 sync（掉电/崩溃可留零长度或旧内容产品）

### [47] V1-N-02（P1/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：SNR 物理量改了但二进制版本位没改 ⇒ 新旧 `.hiss` 同标签承载不同物理量；JSON 面加了 `snr_schema` 而二进制面没加

### [48] V10-N-02（P1/C_ALG_IMPL）— 判定：待填
- 原报标题：`A_ORDER`/`AP_ORDER` 未做 `[0,5]` 校验 ⇒ SIP 求值按 order 索引 36 元素数组：`order=6` 越入相邻数组、`order=1000` 索引 6000

### [49] V11-N-01（P0/G_GOV_GATE）— 判定：待填
- 原报标题：`AioHipsSnrPoint` 镜像落后 C 头两字段 ⇒ 越界读 24 字节，且**交付 HiPS SNR 目录第 2/3 条记录是邻堆垃圾**

### [50] V11-N-09（P1/G_GOV_GATE）— 判定：待填
- 原报标题：（P1）导出面白名单只覆盖 5 个模块 DLL，**被跨语言消费的 5 个 DLL 零裁剪**；Gaia 的公共 ABI 头住在 `src/` ⇒ 天然逃过任何 `include/` 面普查

### [51] V12-N-07（P2/A_SCI_DEF）— 判定：待填
- 原报标题：（P2）割线迭代 5 个兜底字面量与结构默认值**各写一遍**；`±6.0` 全仓无承载

### [52] V12-N-15（P2/A_SCI_DEF）— 判定：待填
- 原报标题：（P2）门常量 `511` 与 `512` **互为字面量、无派生、无 `static_assert`**

### [53] V13-N-06（P1/G_GOV_GATE）— 判定：待填
- 原报标题：`c3452d48` 删掉的 4 行 VERIFIED 合同行**无任何对应登记动作** ⇒ 合同面与追溯面永久分叉且恒不红

### [54] V15-N-16（P1/F_TEST_GAP）— 判定：待填
- 原报标题：（P1·需运行期定性）`UT-BACKEND` 的 command 只有 `unittest discover`、**无 build 依赖声明**，而三个 backend 测试都走 `EXE = REPO/build/astrocs` 真跑 ⇒ **干净 checkout 上"真行为断言"整体 error**

### [55] V18-N-14（P1/G_GOV_GATE）— 判定：待填
- 原报标题：（P1·两套重入判据并存，只有前一套会被空值污染）selfcheck 的"我在哪一阶段"判据**两种写法分裂**

### [56] V3-N-02（P2/C_DOC_CODE_GAP）— 判定：待填
- 原报标题：plugin 面把 -14 折叠进默认域，DATA 语义在第二出口丢失

### [57] V6-N-10（P1/G_GOV_GATE）— 判定：待填
- 原报标题：有修无账：账本 20 条的 fix_commit 全集与本轮 9 个真改代码的提交完全不相交

### [58] V7-N-08（P?/C_ALG_IMPL）— 判定：待填
- 原报标题：（P3）亲和性探测失败 ⇒ 静默按 1 核，**无「探测失败」判别位** ⇒ run manifest 的 `budget=1` 与"真 1 核机"同值

### [59] V8-N-08（P2/D_COMMENT）— 判定：待填
- 原报标题：（P3·判据②）角度归一化契约**区间端点写错**：头注释 `(-90, 90]` 左开，而 `-90` 是可达返回值

### [60] V9-N-09（P1/G_GOV_GATE）— 判定：待填
- 原报标题：（P1·机制⑤）39 道 `ctest-target` 门**不带 `--fail-if-no-tests`** ⇒ 目标改名/未构建即记 PASS，且本仓有真机直证其退出码为 0

## 统计表

| 状态 | 条数 | 条目 |
|---|---|---|
| STILL | | |
| FIXED | | |
| MOVED | | |
| CANNOT_STATIC | | |
