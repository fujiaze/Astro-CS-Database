# FIX_LEDGER · 修复账本（人读视图：P0 全列）

- 条目总数 **532**（P0 82 / P1 306 / P2 144 / 其它 0）；机器读写面 = 
  `问题扫描/账本/FIX_LEDGER.csv`（**隔壁只填后 9 列**，判定列由前台重跑刷新并按 id 保留你的填写）
- 重跑：`python3 问题扫描/_tools/gen_fix_ledger.py`

| id | P | 类别 | 产出方 | 裁决 | 标题（截断） | fix_state | fix_commit | 回归锁 |
|---|---|---|---|---|---|---|---|---|
| FD-F-003 | P0 | F_TEST_GAP | FD |  | 唯一跑 Windows C++ 单测的门连续 10 次"0 用例 PASS"，且空输出被 `EMPTY_OUTPUT_SILENCE_EXE | OPEN |  | NOT_NEEDED:按负责人指令本包无 Windows 内容，Windows 测试门后置 |
| M1a-A-001 | P0 | A_SCI_DEF | M1a |  | SCI 前向 WCS 公式把像素域 SIP 加在天球域中间坐标上，量纲不闭合，且同节自称"与实现一致" | OPEN |  |  |
| M1a-A-002 | P0 | A_SCI_DEF | M1a |  | 天测精度门只约束内部拟合 RMS 域；仓内唯一外部闭环实测超 ALG F1 门 1.8× 且判 PASS、无偏差登记 | OPEN |  |  |
| M1a-A-003 | P0 | A_SCI_DEF | M1a | A-07 | CAR/AIT 投影丢失 Paper II fiducial 偏移（CRVAL2 不进映射）且 Y=−θ 手性与 CD 合成北南镜像 | OPEN |  |  |
| M1a-B-001 | P0 | B_STD_MISMATCH | M1a | A-09 | 注册表把 41×41/81×81 的 AP/BP 网格拟合登记为"7×7 网格…SCI 层已显式冻结该口径"并评severity低（M1a  | OPEN |  |  |
| M1a-B-002 | P0 | B_STD_MISMATCH | M1a |  | Paper II 相关三行注册表判 CONFORMANT/偏差"无"，与 CAR/AIT 实质偏离及"仅 TAN 受控"现状互斥 | OPEN |  |  |
| M1a-C-001 | P0 | C_DOC_CODE_GAP | M1a | A-09 | SIP 逆向冻结口径三方割裂（SCI/ALG 说 7×7、代码是 41×41 阶5 + 81×81 阶7）；SCI 冻结 1e-4 px 门 | OPEN |  |  |
| M1a-C-002 | P0 | C_DOC_CODE_GAP | M1a |  | trans 线性奇异：SCI 承诺"返回参数错误"，实现仅 warn 并恒设 success=true，可回写冒称 TAN-SIP 的退化头 | FIXED | c3452d48 | ADDED:ipv_extract_wcs_sip_failclosed |
| M1a-C-003 | P0 | C_DOC_CODE_GAP | M1a |  | p1_op_wcs 自检以 0-based 数组下标喂 1-based 契约的 WcsTan；两条门与"独立参考解"共用同一原点 ⇒ 对原点 | OPEN |  |  |
| M1a-C-004 | P0 | C_DOC_CODE_GAP | M1a | A-08 | alpha 适用 FOV ≤ 20° 为冻结约束但全仓零强制点；三处合同对"是否硬门"互斥；超限/半球外像素静默留 NaN 仍计数发布 | OPEN |  |  |
| M1a-F-001 | P0 | F_TEST_GAP | M1a |  | CAR/AIT"独立 Oracle"与被测实现同源（解析式取自同一推导），错误口径被测试钉死为正确 | OPEN |  |  |
| M2a-A-1 | P0 | A_SCI_DEF | M2a | A-06 | SCI-DRZ-001 的 pixfrac 归一化与其自身"常数场流量守恒"不变量互斥：常数面亮度 B0 的输出为 B0/pixfrac² | OPEN |  |  |
| M2a-B-1 | P0 | B_STD_MISMATCH | M2a |  | 星表位置历元：注册表条款面要求 J2016.0，产品面四处声称 J2000，模块零历元处理且无天测列可传播——CONFORMANT 判定与偏 | OPEN |  |  |
| M2a-C-1 | P0 | C_DOC_CODE_GAP | M2a |  | DLL 通道把 out_match_idx 截断为 matched_count 个元素，破坏"坐标序 + −1 表示未匹配"合同；部分未命中 | FIXED | dce8abd4 | ADDED:tests/unit/gaia_adapter_test.c::by_coords_partial_miss |
| M2a-F-1 | P0 | F_TEST_GAP | M2a | C-02 | SCI-DRZ-001 §11 的五个验证 Oracle 中三个所指测试文件从未注册进任何构建/CTest/CI 面，注册表还以其中未注册的 | OPEN |  |  |
| M2a-H-1 | P0 | H_NUMERIC | M2a |  | precision_mode 不参与累加域选择：核心 IR 通道恒 FP32 累加，config 要 FP64 也拿不到 float64 累 | FIXED | dce8abd4 | ADDED:tests/unit/drizzle_precision_default_test.cpp::T8,T9 |
| M2a-H-2 | P0 | H_NUMERIC | M2a | A-11 | variance≤0 的源像素被整颗丢弃（信号/覆盖/nContrib 一并丢失，无计数）：不确定度面被当 validity 掩膜，且该语义 | OPEN |  |  |
| M2b-A-01 | P0 | A_SCI_DEF | M2b |  | HiPS 写入口对 nside 零校验：非 2 的幂与 order>29 均被接受并静默产出错产品；lib/hips 的正确校验在生产调用点 | OPEN |  |  |
| M2b-A-02 | P0 | A_SCI_DEF | M2b |  | tile 几何合同三源互斥：层级 tile NSIDE 卡（代码 2^(k+9) vs ALG 2^k vs IO_002 2^(K+9)） | OPEN |  |  |
| M2b-B-01 | P0 | B_STD_MISMATCH | M2b |  | HiPS tile 目录名写「商」、文件名写「余数」，与 IVOA §4.1 算式相反 | OPEN |  |  |
| M2b-B-02 | P0 | B_STD_MISMATCH | M2b |  | Moc.fits 缺 MOC Table 3 强制键 ORDERING=NUNIQ / COORDSYS=C，注册表仍判 CONFORMAN | OPEN |  |  |
| M2b-B-03 | P0 | B_STD_MISMATCH | M2b |  | hips_pixel_scale 写角秒，标准规定单位为度（相差 3600 倍） | OPEN |  |  |
| M2b-B-04 | P0 | B_STD_MISMATCH | M2b |  | HiPS 域条款锚整体错挂；properties 必需键集用自定 5 键替代标准 9 键；STD-F4 引用不存在的 obs_bandpas | OPEN |  |  |
| M2b-B-07 | P0 | B_STD_MISMATCH | M2b | A-23 | 「order ≤ 29」被写成 Górski 标准条款，代码实际无 order 上界且 npix 在 2^31 处静默回绕 | OPEN |  |  |
| M2b-B-09 | P0 | B_STD_MISMATCH | M2b |  | 交付 FITS 可恒带 CHECKSUM 全零占位卡（注释谎称「HDU checksum updated」），且 verify 的豁免设在* | OPEN |  |  |
| M2b-C-01 | P0 | C_DOC_CODE_GAP | M2b | A-22 | 【部分修复】HiPS 生产写出仍为直写（无 staging/rename/fsync/COMPLETE），AIO 树内其它子系统已用原子原语 | OPEN |  |  |
| M2b-F-01 | P0 | F_TEST_GAP | M2b | A-31 | D.healpix 判 CONFORMANT 所依的「astropy-healpix 百万点独立 oracle / 往返 ≤1e-12 de | OPEN |  |  |
| M3-A-001 | P0 | A_SCI_DEF | M3 | A-01 | SNR 无唯一定义：进入生产权重的「SNR」实为已退休的 PSF 拟合质量比 | OPEN |  |  |
| M3-A-002 | P0 | A_SCI_DEF | M3 | A-02 | SCI-CW 把 `weight_mode=2` 像素权重定义为 `support × snr²`，与实现、另两份冻结合同及其自身条款互斥 | OPEN |  |  |
| M3-C-001 | P0 | C_DOC_CODE_GAP | M3 |  | SCI-PHOT 冻结的参考星数门（`/r_consistent/>=3`/`/r_inliers/>=2`）在实现中不存在：一颗星即可定标 | FIXED | c3452d48 | ADDED:p1phot_fixgates |
| M3-C-002 | P0 | C_DOC_CODE_GAP | M3 |  | SCI-PHOT 的饱和/质量标志判据在接口与实现中都不存在，饱和星可进零点拟合 | PARTIAL | c3452d48 | ADDED:p1phot_fixgates |
| M3-C-003 | P0 | C_DOC_CODE_GAP | M3 | A-03 | 冻结 SCI 的 `min_samples` 默认 5 与实现 64 相差 12.8 倍，ALG 径行「不改 SCI，以代码为准」= 权威层 | OPEN |  |  |
| M3-E-001 | P0 | E_TRACE_BREAK | M3 |  | 校准 / 测光 / 噪声三域的追溯行与合同尾注存在行级失真：VERIFIED 行的测试锚不覆盖被验门，合同尾注用非法测试 ID | FIXED | c3452d48 | NOT_NEEDED:同 M3b-F-02 的追溯校验器覆盖 |
| M3-F-001 | P0 | F_TEST_GAP | M3 |  | SCI-NOISE §11/§15 承诺的「NumPy rtol 1e-9 复算」在仓库内不存在，唯一 Python 对拍面是容差带断言且可 | OPEN |  |  |
| M3b-A-01 | P0 | A_SCI_DEF | M3b |  | 生产检测拟合内核实为椭圆高斯，SCI 冻结基线却写 Moffat4 且禁止高斯主路径 | OPEN |  |  |
| M3b-A-02 | P0 | A_SCI_DEF | M3b |  | star_det flux 列实为峰值振幅/m00 两链不同量，合同却以流量名义登记 | OPEN |  |  |
| M3b-A-03 | P0 | A_SCI_DEF | M3b |  | 每像元截尾残差被合同命名为 flux_uncertainty，DISP-PSF-005 自认无协方差 | OPEN |  |  |
| M3b-C-01 | P0 | C_DOC_CODE_GAP | M3b |  | 生产 PSF 质心 ~0.5px 系统性偏差：已知未闭合的 BLOCKER 只活在归档层，规范层反宣称「无 0.5px 量化损失」 | OPEN |  |  |
| M3b-C-02 | P0 | C_DOC_CODE_GAP | M3b |  | 注册节点 star-psf 实装第三套未登记检测器；「唯一生产源」三处宣称被证伪（第三次同模式） | OPEN |  |  |
| M3b-F-01 | P0 | F_TEST_GAP | M3b |  | 新失效形态：质心验收门按「非生产初始化配置」定义判据，生产路径质心无门且脚本零登记 | PARTIAL | c3452d48 | ADDED:p1psf_prodpath_centroid |
| M3b-F-02 | P0 | F_TEST_GAP | M3b |  | SCI-PSF-001 的 VERIFIED 声明全部证据不成立：§11 承诺的 scipy/curve_fit 复算不存在、追溯行指向无断 | FIXED | c3452d48 | NOT_NEEDED:追溯锚订正由既有 tools/quality/check_traceability.py(rows=63 ok=63) 与 v19r3_traceability.py(authority_path_broken=0) 覆盖 |
| M3b-F-03 | P0 | F_TEST_GAP | M3b |  | 冻结验收项在测试面零覆盖：SNR 谱段缺口按测试自身常数可判、空值守卫、永真虚警门（升 P0） | OPEN |  |  |
| M3b-G-01 | P0 | G_GOV_GATE | M3b |  | 未闭合 BLOCKER（PSF-001 ~0.5px）只活在归档文档：规范限制层零提及、偏差登记层零条目、门禁层零登记 | OPEN |  |  |
| M3b-H-01 | P0 | H_NUMERIC | M3b |  | mad 列实为 RMSE 再乘 MAD→σ 系数 1.4826 充当残差 σ，排异门等效收紧 | FIXED | c3452d48 | ADDED:p1star_mad |
| M4-A-01 | P0 | A_SCI_DEF | M4 |  | N≤4 全拒像素被静默全接受为 UNDERDETERMINED，FROZEN SCI 未订正且零测试 | OPEN |  |  |
| M4-A-02 | P0 | A_SCI_DEF | M4 |  | 冻结 SCI 内 support reducer 两口径互斥：代码取 accepted 口径并有门，§5 公式未订正、旧 ALG 仍称现状= | OPEN |  |  |
| M4-C-01 | P0 | C_DOC_CODE_GAP | M4 |  | kcorr_lookup 把非均匀 pixfrac 网格当均匀插值：生产默认 0.8 列返回表外值，偏差 +1.5%/+2.1% | FIXED | dce8abd4 | ADDED:phase2_sampler.kcorr.corner_exact |
| M4-C-02 | P0 | C_DOC_CODE_GAP | M4 |  | SCI 冻结弱零锚 1e-3 在两条生产装配均为 0；ALG「同值生产装配」为不实陈述 | FIXED | dce8abd4 | ADDED:phase2_synthetic_gate.P2EntryParity.zero_anchor_default |
| M4-C-03 | P0 | C_DOC_CODE_GAP | M4 |  | stage2 ivar tile 读失败逐像素静默换成 support 作权重：无开关、无计数；文档「显式 fallback 并计数」失实； | FIXED | dce8abd4 | ADDED:phase2_ivar_wiring.Phase2IvarWiring.IvarTileMissingFailClosed |
| M4-F-01 | P0 | F_TEST_GAP | M4 |  | UPM 硬门/持久化门/MC 标定门全部不挂产品构建，SCI「Oracle 全过」无运行时证据 | OPEN |  |  |
| M4-F-02 | P0 | F_TEST_GAP | M4 |  | SCI 声称的 NumPy 独立 Huber-IRLS Oracle 真源不存在，「Python 参考 rtol 1e-9」为空头支票 | OPEN |  |  |
| M5a-G-001 | P0 | G_GOV_GATE | M5a |  | 生产利用率门沿用 §18.2 已废止的 80%/75%/50%，宪章冻结的 85%/60% 无实现 | FIXED | adaeb531 | ADDED:tests/unit/p2_workers_test.cpp::case_avg_085_boundary |
| M5a-G-002 | P0 | G_GOV_GATE | M5a |  | CPU 利用率门的分母是 1 核：85%/90% 实为 0.85/0.90 核下限，四处注释自述相反口径 | FIXED | adaeb531 | ADDED:tests/unit/p2_workers_test.cpp::case_alloc4_observed_0p9_core_fails |
| M5a-G-003 | P0 | G_GOV_GATE | M5a |  | 唯一逐字实现 §10.5 的门禁零应用点，L0 口径却称「缺失即 fail-closed」 | OPEN |  |  |
| M5a-G-005 | P0 | G_GOV_GATE | M5a |  | THREAD-BUDGET 机器门空转：扫描面、正则与行级豁免三重漏检并被单测固化 | OPEN |  |  |
| M5b-C-01 | P0 | C_DOC_CODE_GAP | M5b |  | API 合同表 422 行全部标 VERIFIED、396 行共用未定义占位 test_ids，门只比 4 列且自述"真实校验由 AST-A | OPEN |  |  |
| M5b-C-02 | P0 | C_DOC_CODE_GAP | M5b |  | 两平台交付同一 manifest：Windows 侧把 Linux 侧标 SKELETON 的两个平台库标为 IMPLEMENTED | OPEN |  |  |
| M5b-G-01 | P0 | G_GOV_GATE | M5b | C-12 | CLI 命令树与协议门禁全部打在 COMPATIBILITY 二进制上；二进制缺失时门静默零检查 | OPEN |  |  |
| M5b-G-02 | P0 | G_GOV_GATE | M5b |  | 追溯门 fail-open：`tools/quality/check_traceability.py` 无条件 return 0；TRACE | OPEN |  |  |
| M5b-G-03 | P0 | G_GOV_GATE | M5b |  | AST-API 门为同一头文件自反比对（恒真），参数数提取后弃用，且不读任何 API 文档 | OPEN |  |  |
| M5b-G-04 | P0 | G_GOV_GATE | M5b |  | 版本单源簇：交付清单/安装树合同/依赖锁/schema 并存 4 份手工版本副本，两道版本门的扫描面都不覆盖它们 | OPEN |  |  |
| M5b-G-05 | P0 | G_GOV_GATE | M5b |  | CON-BUILD-GRAPH 只做子串存在性检查：订正 BUILD_GRAPH.md 反而变红，产品构建面无任何机器校验 | OPEN |  |  |
| M5b-G-06 | P0 | G_GOV_GATE | M5b |  | §18.4「只加载随产品签名清单发布的官方模块」在机器上无实现：产品不加载、hash 全 null、唯一闭环测试不进 CI | OPEN |  |  |
| M6a-C-001 | P0 | C_DOC_CODE_GAP | M6a | A-20 | SCI-P3 冻结的 order_sel「读层级 leaf_nside=2^(order_sel+9)」在两条通道都不参与执行：order_ | OPEN |  |  |
| M6a-G-001 | P0 | G_GOV_GATE | M6a | C-11 | 注释卫生门 CON-COMMENTS 是空壳机器门：宪章 §12.2/§12.3-10 要求的四类判据中三类无实现，唯一有实现的判据被「冻结 | OPEN |  |  |
| M6b-E-001 | P0 | E_TRACE_BREAK | M6b |  | 追溯矩阵双头：权威明文是 SPEC JSON，但全部活动路由面（README-DOCS / DEVELOPER_GUIDE / API_ST | OPEN |  |  |
| M6b-G-001 | P0 | G_GOV_GATE | M6b |  | 未经证据支撑的「已验证」声明（单一根因，四组实例）：八层矩阵在 TEST / EVIDENCE / SRC / 整行四个层面给出 VERIF | OPEN |  |  |
| M6b-G-002 | P0 | G_GOV_GATE | M6b | A-30 | 发布与完成状态在仓库内无单一事实源：四份 RELEASE_STATUS 并存、状态词三套、AGENTS.md 状态行被自家校验器判非法却被另 | OPEN |  |  |
| M6b-G-003 | P0 | G_GOV_GATE | M6b |  | 「本版实测」类无据证据：ACTIVE 文档以完成时宣称机器门 PASS 9/9 与 mismatches=[]，既无 SHA/命令/时间/日 | OPEN |  |  |
| M7-A-001 | P0 | A_SCI_DEF | M7 | A-20 | 「科学权重」唯一性被三处不同对象宣称，且 SCI-UPM §5 两式在实现侧被证明不等价（Σ 域裁决 = 单 control 内跨帧） | OPEN |  |  |
| M7-A-002 | P0 | A_SCI_DEF | M7 |  | ALG 权威层把「ivar 缺失时回退 support」写成 weight_mode=2 的唯一语义（L21-001，维持 P0） | OPEN |  |  |
| M7-C-001 | P0 | C_DOC_CODE_GAP | M7 |  | 采样器控制网格边长可配（1..64）而 UPM 把 grid=8/cell_side=64/tile_shift=9 编成常数，两侧无一致性 | FIXED | dce8abd4 | ADDED:phase2_synthetic_gate.Phase2Upm.ControlGridMismatchRejected |
| M7-G-001 | P0 | G_GOV_GATE | M7 |  | 「不可达验收门」家族汇总与升档判定：冻结门在自身冻结公式或 IEEE-754 下数学不可满足（第 1 类；第 2 类见 M7-G-105）， | OPEN |  |  |
| M8-F-001 | P0 | F_TEST_GAP | M8 |  | `tests/abi` 四个验收脚本被 UT-ABI 以「0 用例」方式采集，该门的加载/注册/echo 三面永不失败；文档仍以「36/36 | FIXED | adaeb531 | ADDED:ci/validate_registry.py::R11 |
| M8-F-002 | P0 | F_TEST_GAP | M8 |  | `tests/unit/io_ownership_test.cpp` 在 ctest 下**不可能失败**：唯一 CHECK 的失败计数不参 | FIXED | adaeb531 | ADDED:tests/unit/io_ownership_test.cpp::failure_sets_nonzero_exit |
| M8-F-003 | P0 | F_TEST_GAP | M8 |  | 单入口安装树合同门（`tests/cli/test_cli_single_install.py`）在 CI 侧恒 SKIP：前置产物在根 ` | FIXED | adaeb531 | ADDED:tests/cli/test_cli_single_install.py::ci_missing_prereq_fails |
| M8-F-004 | P0 | F_TEST_GAP | M8 | A-24 | 被 STANDARDS_REGISTRY / TEST_MATRIX / TRACEABILITY 当作符合性证据的测试源从不在任何执行面出 | OPEN |  |  |
| M8a-G-001 | P0 | G_GOV_GATE | M8a | A-31 | 依赖与许可登记面三处失真：GPL 组件进入产品链接闭包、五处登记面零覆盖、派生代码挂自著作权 MIT | OPEN |  |  |
| M9-B-1 | P0 | B_STD_MISMATCH | M9 |  | SIP 前向 A/B/AP/BP 系数以「像素域」数值直写 FITS 标准键 A_i_j/B_i_j；同一数组在合同面有四处互斥单位；桥注释 | OPEN |  |  |
| M9-F-1 | P0 | F_TEST_GAP | M9 |  | 本域八个恶意输入回归 TU 从不进构建：它们恰是全部 M9 边界缺陷的唯一复现件，却被 docs 当符合性证据引用 | OPEN |  |  |
| M9-H-1 | P0 | H_NUMERIC | M9 |  | Gaia 模块 manifest 拼接：snprintf 返回「本应写入数」推进指针 + 转义函数无容量入参 → 512 字节栈缓冲越界写， | FIXED | 07eb229b | ADDED:gaia_module_manifest_bounds |
| M9-H-2 | P0 | H_NUMERIC | M9 |  | XPSD 自报 spectrumCount 无任何上限即用作分配步长与每星 memcpy 长度 → 堆越界读 + 数百 MB 级 OOM 放 | FIXED | 07eb229b | ADDED:xpsd_spectrum_count_bounds |
