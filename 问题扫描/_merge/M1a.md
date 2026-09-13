# M1a 合并复核报告（第二层 · 域: Phase1 天测/plate solve + Phase3 投影/WCS/重采样/FITS 写出）

- 代理: M1a · 输入切片: `_cache/L01.md`（20 条 P0:6/P1:11/P2:3）+ `_cache/L02.md`（20 条 P0:2/P1:13/P2:5）
- 状态: **完成**
- 纪律: 全程只读（read/grep/glob；web 仅用于 FITS WCS Paper II 原文核对）。唯一写入面 = 本文件 + `问题扫描/findings/**`。
- **本轮关键前提（母控并发修复令）**: 真源正被并发修复（提交 `9a3b5a7d`「HISS 部分覆盖/support 量化 + SIP 桥接 + 资源门真实观测」等批次）。因此**每条定稿前均按当前文件内容以符号/关键语句重读一次**，不信叶子行号；四态分别列表于 §一～§四。全部结论一律标注「复核时点成立」，不假设任何叶子结论仍新鲜。
- 定稿规模: **44 条 finding**（P0 10 / P1 25 / P2 9），分布在 9 个类别目录下的 18 个 `M1a_L01_L02.md` 合并文件（每「类别×优先级」一个文件，内部按编号分节）。

---

## §〇 四态汇总

| 处置态 | 来源条数 | 说明 |
|---|---|---|
| **仍成立**（当下树内可复现） | **35 / 40** | 其中 3 条经"重锚"、2 条经"半边剔除/范围修正"后方成立 |
| **已被修复**（并发提交期间，整条消失） | **0 / 40** | 修复全部以"半边"形态出现（见 §二） |
| **部分修复**（定稿为残余事实） | **5 / 40** | L01-002 / L01-011 / L01-012 / L02-012 / L02-013 |
| **无法判定**（整条） | **0 / 40** | 但 **6 个子问题**转 §六 待复核（需运行/外网/数据） |
| 剔除的半边事实 | 6 处 | §五-1～6，逐条给理由（不静默丢弃） |
| 优先级调整 | 降级 2 条 / 加立或拆分 4 条 | §五-表 |

---

## §一 逐条处置表（40 条 · 四态）

图例: ①仍成立 ②已被修复 ③部分修复→残余 ④无法判定

| # | 来源 | 叶子类别/优先级 | 态 | 定稿条号 · 最终优先级 | 判据（复核时点，均按现树重读） |
|---|---|---|---|---|---|
| 1 | L01-001 | A_SCI_DEF/P0 | ① | M1a-A-001 · P0 | SCI §5:48 把像素域 SIP 加在 deg 中间坐标上（量纲不闭合），:66 仍自称"与实现一致"；实现为像素域（ipv_wcs.cpp 牛顿块解 u+A(u)=UV）；oracle :228 自证同类错曾被 F2 捕获 |
| 2 | L01-002 | B_STD_MISMATCH/P0 | ③ | M1a-B-003 · **P1（降级）** | 已修半边=Phase3 单点桥 + 九宫格 + 四向扫描（§二-1，有回归锁）；残余=注册表"已闭环"与同树测试"owner decision"并存、报告串方向与代码相反、ipv 头注标签与数值语义互斥 |
| 3 | L01-003 | C_DOC_CODE_GAP/P0 | ① | M1a-C-003 · P0 | 现树仍以 x∈[0,W) 数组下标喂 dx=x−crpix1（wcs_tan.h 钉 1-based）；往返门与"独立前向参考解"（u=x−crpix1）共用同一原点 ⇒ 原点平移零判别力；p1_wcs.json 的 samples 无像素原点声明 |
| 4 | L01-004 | C_DOC_CODE_GAP/P0 | ① | M1a-C-001 · P0 + 加重 M1a-B-001 · P0 | SCI §5:57 与 §9:125 仍写 7×7/NB_GRID=7；代码 41×41 阶5 + 81×81 阶7；:397-400 自证"1e-4 冻结门数学不可达，需负责人裁决 (finding)"未入台账 |
| 5 | L01-005 | C_DOC_CODE_GAP/P0 | ① | M1a-C-002 · P0 | SCI §8:118 承诺"拒 SIP 推导，返回参数错误"、ALG:160-162 禁"以 CRPIX 坍缩值冒充解"；实现仅 warn、success=true 无条件；负面用例只测 iter_trans_solve，registry:68 却称"已由负面用例覆盖" |
| 6 | L01-006 | A_SCI_DEF/P0 | ①（重锚 + 半边剔除） | M1a-A-002 · P0 | 叶子所引 SCI §8 句在 docs/science/ **0 命中** → 该半边剔除（§五-1）；成立半边=memory.md:55-63（内 0.151px vs 外 0.897px=5.9×、Y 0.848 vs 0.218、Q4 偏置、VERDICT: PASS）与 ALG:186 0.5″ 门互斥 |
| 7 | L01-007 | A_SCI_DEF/P1 | ① | M1a-A-004 · P1 | SCI:19 把 cd_inv 定义为 CD^-1 却注 pixel/arcsec；同文 :53 与实现为 inv(trans.linear)；代码 :338 自警"差 3600 倍"；:138 只禁 deg/pixel 一侧 |
| 8 | L01-008 | A_SCI_DEF/P1 | ① | M1a-A-005 · P1 | ALG:83 "尺度容差 0.002（各向异性 0.2%）"被 :187 挪用为"≤2%（同源）"（十倍）；尺度窗 0.002 / ±10% / [0.95,1.05] / 20% 四套并存；good_rms/τ 名角秒实像素 |
| 9 | L01-009 | C_DOC_CODE_GAP/P1 | ① | M1a-C-005 · P1 | ALG:33 把 IRLS 15×/Huber 1.345（legacy ipv_sip）写进生产 build_sip 步骤；ALG:166-170 自认生产走网格 LSQ，两处互斥；5.0" 阈值挂到 kd_match 步 |
| 10 | L01-010 | F_TEST_GAP/P1 | ① | M1a-F-002 · P1 | ALG:201-202 钉 <1e-6 deg 并指锚测试 :50；该断言为像素域；节点措辞 px ⇒ 合同角域精度全域无断言（仅 CRVAL 锚点单点） |
| 11 | L01-011 | F_TEST_GAP/P1 | ③ | M1a-F-003 · P1 | 已修半边=端到端链真接 ipv_solve_from_memory_with_callback_d（§二-2，**无回归保护** → 另立 F-005）；残余=0.1431 在 tests/ 0 命中，历史诊断脚本位于免报区 |
| 12 | L01-012 | C_DOC_CODE_GAP/P1 | ③ | M1a-C-006 · P1 | 已修半边=节点侧自选星/60 硬编码消失（§二-3）；残余=solver:1159 n_target=60 覆盖入参 + select:16（flux 降序）vs :462-470（box-mag 升序、flux 未使用）+ ALG:115 三源互斥 |
| 13 | L01-013 | E_TRACE_BREAK/P1 | ① | M1a-E-001 · P1 | module.yaml:29-30 仍称"根 CMakeLists.txt 无 ipv 目标"，实测 CMakeLists.txt:507 add_library(astrocs_p1_ipv) + :649 已入链接面；12 个 C ABI 锚整体偏 -11～-29 行 |
| 14 | L01-014 | E_TRACE_BREAK/P1 | ① | M1a-E-002 · P1 | docs/05、docs/24/25、lib/plate_solve_old/v4_archive 在仓内 0 命中；真实件只存在于 engineering/control/archive/** 与 run/**（免报/运行区），却被 C ABI 注释当权威引用 |
| 15 | L01-015 | A_SCI_DEF/P1 | ① | M1a-A-006 · P1 | gaia client 与 ipv_api.h 内 epoch/历元/自行/视差 关键字 0 命中；gaia_client.c:1829 契约置 0；registry:45/:186/:193 以 J2016.0 判 CONFORMANT 而 SCI §3a 写"ICRS/J2000" |
| 16 | L01-016 | B_STD_MISMATCH/P1 | ①（降级） | M1a-B-006 · **P2** | CDELT 仅 :4226-4227 写、全文件无删除路径 ⇒ CD 与 CDELT 可混写；SIP 回写循环 :2088-2095 自 i=0,j=0 起 ⇒ 可写 A_0_0/A_1_0/A_0_1（SIP 仅定义 i+j≥2） |
| 17 | L01-017 | D_COMMENT/P1 | ① | M1a-D-001 · P1 | ALG:128/:147-148 "+0.5 契约" vs ipv_api.h:203-219 "图像中心原点, Y 轴向上" —— 同一 inlier 权威缓冲两说（孰为生效语义 → §六-5） |
| 18 | L01-018 | D_COMMENT/P2 | ①（重锚） | M1a-D-002 · P2 | 原锚 lib/phase1/ups/build_upm.cpp 不存在 → 重锚至 ipv_types.h:26-29（StarPoint 注"角秒"而 ALG:115 为像素）+ :68-69/:72（order 注 2/3/4、上限 9 vs 实现 5/7） |
| 19 | L01-019 | E_TRACE_BREAK/P2 | ①（重锚） | M1a-E-004 · P2 | 原锚 README:4831/:24178 0 命中 → 重锚为 ALG:89（合成恢复/极区保守）vs SCI:162（Astropy 比对/往返），同 ID 双语义 |
| 20 | L01-020 | I_DOC_HYGIENE/P2 | ① | M1a-I-001 · P2 | REPORT.md:5-10 现在时态"20 帧真实数据 100% 成功" + 悬空迁移声明；同目录混入 make_clean_out.txt / make_clean_err.txt / siril_atpmatch_b64.txt |

| 21 | L02-001 | B_STD_MISMATCH/P0 | ①（拆 3 条） | M1a-A-003 · P0 + M1a-B-002 · P0 + M1a-F-001 · P0 | CAR/AIT 映射不含 CRVAL2（dec(CRPIX)=0）+ Y=−θ 与 CD=diag(−s,+s) 合成北南镜像；registry:64 判 §5 投影族 CONFORMANT、偏差"无"；两套 oracle 期望式即实现式 |
| 22 | L02-002 | C_DOC_CODE_GAP/P0 | ① | M1a-C-004 · P0 | FOV≤20° 仅存于 SCI §9a-12:109 与 §15:115；四处校验面（p3_wcs_validate / p3_session / p3_projection_make 四角守卫 / 节点参数）无一实现；ALG:382-384 反称"非 make 硬门" ⇒ 三合同互斥 |
| 23 | L02-003 | C_DOC_CODE_GAP/P1 | ①（半边剔除） | M1a-C-007 · P1 | 合同记头文件 58 行、实现 239 行；实测 **lib/phase3_session/p3_resample.h = 116 行**（且不在合同所称的 lib/phase3_rsmp/）；§1 非目标"不实现 variance/weight/ivar"被 :23-27 supersession 与四平面实现推翻；§4 十符号签名与实现不符；§6.5 "coverage 恒 1" 与节点 :4856 逐像素覆盖互斥。剔除 has_unc 出参细节（§五-3） |
| 24 | L02-004 | E_TRACE_BREAK/P1 | ① | M1a-E-003 · P1 | PROJ_IMPL:11-12 以"实测"名义钉 p3_wcs.h=50 / p3_wcs.cpp=165 行，现 63 / 230 行 |
| 25 | L02-005 | F_TEST_GAP/P2 | ① | M1a-F-006 · P2 | p3_interp_test.cpp:17-19 测试自带 private bilinear 并对自身断言（:44），未经生产 p3_sample_bilinear_ex；合同仍列其为插值/覆盖语义证据 |
| 26 | L02-006 | A_SCI_DEF/P1 | ① | M1a-A-007 · P1 | RSMP:54 写 validate 条件 \`|center.Dec|≥5°\`（SCI §4:48 为 \|dec\|≤85°，语义相反）；:22 像素→中间坐标使用 CD^-1（Paper I 该方向为 CD） |
| 27 | L02-007 | A_SCI_DEF/P1 | ① | M1a-A-008 · P1 | SCI:66 "C(x,y)=1 ⇔ 采样足迹内存在**有限** tile 像素; 否则 C=0 且 S=NaN" vs SCI:86 "tile 内 NaN → 传播为输出 NaN（C=1）"；实现取后者 ⇒ 不变量陈述不可测 |
| 28 | L02-008 | A_SCI_DEF/P1 | ① | M1a-A-009 · P1 | DATA §30.4 条目3 把 ivar==0 归入"负/Inf（含 ivar==0 导出）=产品损坏→显式错误"，同文条目1 定"NaN 传播态"；p3_resample.cpp:324 注释按后者自行裁决，测试仅覆盖 u<0 |
| 29 | L02-009 | A_SCI_DEF/P1 | ① | M1a-A-010 · P1 | 前向 :162-163 以 r≥π/2 判界（r=tan d ⇒ d≥57.5194°），反向 :187-189 以 denom≤0（d≥90°）；DATA:2189 称"数学等价"不成立 ⇒ d∈[57.52°,90°) 单侧不可往返 |
| 30 | L02-010 | B_STD_MISMATCH/P1 | ① | M1a-B-004 · P1 | p3_wcs.cpp:215-216 "CTYPE1= '" 使 '=' 落第 7 列、:220-221 落第 8 列；FITS 4.0 §2.2 要求关键字列 1-8、'=' 第 9 列；PROJ_IMPL:112 合同示例与代码相反 |
| 31 | L02-011 | B_STD_MISMATCH/P1 | ① | M1a-B-005 · P1 | aio_hips_writer.cpp:905/:1141 写死 hips_frame=equatorial；hips_properties.cpp:125-126 将 equatorial 与 icrs 恒等消费；SCI:47 冻结文本含未决「?」（枚举行原文 → §六-4） |
| 32 | L02-012 | C_DOC_CODE_GAP/P1 | ③ | M1a-C-008 · P1 | 已修半边=节点面真实 manifest 哈希 + run_context 缺失 fail-closed（§二-4，**无 HISTORY 断言** → F-005）；残余=会话面 p3_session.cpp:365-366 仍 manifest_hash=nullptr、hips_id 两面恒占位、README"恒 nullptr"与 FITS_IMPL:344"已闭合"互斥、「SCI-P3 §96」悬空（该文件 143 行，grep 96 → 0） |
| 33 | L02-013 | G_GOV_GATE/P1 | ③（范围修正） | M1a-G-001 · P1 | 叶子"无声明面"过宽：进程内 descriptor（module_adapters:574 UnitId::SURFACE_BRIGHTNESS / CoordinateFrame::PIXEL）已有；但产品落盘面（p3_output 头键无 RADESYS/frame、p3_resampled.json、会话 result JSON）仍缺宪章 §4.3 的"frame + 像素/采样语义"两格 |
| 34 | L02-014 | H_NUMERIC/P1 | ① | M1a-H-001 · P1（升格候选 P0） | 现树 :4827-4829 signal sampler open 失败 → 裸 return（无标志/无计数区分）；:4913-4918 band_executed 无条件 +1；:4942 门只比计数；对照 :4832-4836 uncertainty open 失败置 corrupt(-2) 并在 :4953-4956 失败；注入用例（rt001:318）只覆盖"任务被吞" |
| 35 | L02-015 | G_GOV_GATE/P1 | ① | M1a-G-002 · P1 | API_V1:3 状态 FROZEN(2026-08-28) 未随 §18.1 四投影 / DATA §30.4 uncertainty 订正；:16 引用的 contracts/schemas/phase3_request_v1.schema.json 不存在（contracts/schemas 0 命中）；test_p3_api.py:16-31 全为对 .md 的子串断言（零判别力）；TRACEABILITY.csv:289 状态 MISSING |
| 36 | L02-016 | F_TEST_GAP/P1 | ①（半边剔除） | M1a-F-004 · P1 | 成立半边=test_p3004 头注 1e-3（:7）vs 断言 1e-2（:157）、常数场以 std()<1e-3 代替合同要求的 max_abs=0、解析场容差推给"SYN-007 预冻结"而仓内无该数值表；**剔除**"1e-6→1e-4"半边（p3_wcs_test.cpp:62-63 两门并列，1e-6 在位） |
| 37 | L02-017 | D_COMMENT/P2 | ① | M1a-D-003 · P2 | 注释宣布废止的非目标仍在合同正文；B2-A*/HISS-* 提交号注释内联于源码（其语义只在免报归档区）；p3_wcs.h 头注部分指向已重构行为 |
| 38 | L02-018 | I_DOC_HYGIENE/P2 | ① | M1a-I-002 · P2 | p3_wcs.cpp:18-22 以 #ifndef 预留 kMaxSide 编译期覆盖；全仓构建面 0 定义该宏 ⇒ 无现行违规，定 P2（预留口子 + DATA:2141 把它写作"如实冻结"，与 §15"不得自行放宽"张力） |
| 39 | L02-019 | G_GOV_GATE/P2 | ① | M1a-G-003 · P2 | 同一条件（\|dec\|≤85 / 投影族 / 尺寸）在 p3_session.cpp:107、p3_wcs.cpp 校验段、节点参数层三处内联字面量重复，无单一机器源（schema 缺失见 G-002） |
| 40 | L02-020 | I_DOC_HYGIENE/P2 | ① | M1a-I-003 · P2 | FITS_IMPL:54（删除线）/:191（已删）/:90 三处宣告 fdatasum 废除，而 :248 复杂度式仍含 fdatasum；README 状态段与 :344"已闭合"未同步 |

**四态计数校验**: ①35（其中 3 重锚 / 2 半边剔除后成立 / 1 拆三条）+ ③5 + ②0 + ④0 = **40** ✔

---

## §二 已被修复（并发提交期间）—— 不进 findings；逐条记原判据 / 现状事实 / 是否有回归保护

| # | 涉及来源 | 叶子原判据 | 现在代码事实（复核时点） | 回归保护？ | 未固化处置 |
|---|---|---|---|---|---|
| 1 | L01-002 | CRPIX 契约与实现数值相悖，导出边界"两处/缺失 +1" | p3_wcs.cpp::fits_pixel_1based —— :29-40 合同注释"全文件唯一 +1 点"；:156-157 正向唯一使用点；:199-200 反向对应；SCI 增 §5a、§10:137、§15:173-174；registry:62/:74 订正 | **有（三重）**：p3_wcs_test.cpp:62-63/:289-298（九宫格两 parity + 双桥/无桥必败）+ p1wcs_std_f1_bridge_cross.py:56-60/:309-310（BRIDGE_DECLARED=+1、WRONG_OFFSETS={0,-1,+2} 四向扫描）+ 登记 ctest(CMakeLists:150) 与 ci/checks.json:3117 | 无需另立 |
| 2 | L01-011 | p1_op_wcs 求解分支绕开三角/迭代全链，manifest 记 solver=ipv_solve_from_detections_v1 | module_adapters.cpp::p1_op_wcs :1866-2036 重写：:1867-1877 硬要求 ra0/dec0/focal_length_mm/pixel_size_um/gaia_data_dir；:1901 sp.maxStars=2000；:1933-1935 调 ipv_solve_from_memory_with_callback_d；:1940 上抛 r.error_msg；:2011 solver 字段改实 | **无**（仅产物/键存在性断言） | **另立 M1a-F-005 · P1** |
| 3 | L01-012 | 节点侧自选星并以常量覆盖 img_n_target | p1_pick_stars / gaia_n_target / 节点侧 ipv_solve_from_detections_v1 调用在 lib/ 内 0 命中；选星归一到 C ABI 内部 | **无** | **另立 M1a-F-005 · P1** |
| 4 | L02-012 | HISTORY 的 manifest 恒空、provenance 全链缺失 | module_adapters.cpp::p3n_input_manifest_hash :5065-5081 读 run_context.json 计真实哈希，缺失即 fail-closed；p3_output.cpp:224-227 消费 | **无**（tests/ 内 manifest_hash 命中全属 ArtifactStore 侧） | **另立 M1a-F-005 · P1** |
| 5 | L02-014（邻近） | 行带被取消/丢弃无观测 | :4897-4951 新增 work_units / band_executed / band_active_peak 实测入 manifest；rt001_unique_executor_test.cpp:318 drop-band 注入用例在位 | **部分**（只覆盖任务被吞，不覆盖 sampler open 失败） | 残余定稿 **M1a-H-001** |
| 6 | L01-004（邻近） | APx 阶 7 系数可被任意下游消费 | :1999-2002 solver SIP 阶越界拒绝；wcs_transform.cpp:46-50 sip_order∈[0,5] 硬抛 | **有**（校验即防线） | "导出面一步逆无精度门"仍定稿 **M1a-C-001** |

---

## §三 部分修复 → 定稿为残余事实（标题写实，不沿用叶子旧全称）

| 来源 | 叶子旧标题 | 定稿新标题（残余） | 定稿号 | 被剔除的旧全称成分 |
|---|---|---|---|---|
| L01-002 | CRPIX"0-based 自洽、与标准差恒定 1px"契约与实现数值相悖；STD-F1 登记"已闭环"与测试面"待 owner 裁决"冲突 | STD-F1 **残余**：注册表"已闭环/CONFORMANT"与同树测试"待 owner 裁决"并存，报告字符串方向与代码相反，ipv 侧像素口径标签仍与实现对不上 | B-003 | "导出边界缺失/两处 +1"（已修）；"三方互斥导致产品 1px"（产品出口复算未见偏移） |
| L01-011 | ALG 冻结的实场/合成回归宣称无对应可执行测试或 fixture（含全链不可达） | ALG 冻结的实场/合成精度锚无可执行 fixture，**新接线的端到端链只有产物存在性断言** | F-003 | "求解全链在节点不可达"（已修） |
| L01-012 | 选星规则与 img_n_target 契约脱节（含节点硬编码 60） | 选星口径三处互斥（**残余**）：ALG/头注 flux 降序·默认 20·自适应 20→40→60 vs 实现 box-mag 升序·固定 60 覆盖 | C-006 | 节点侧硬编码半边（符号已消失） |
| L02-012 | HISTORY provenance：会话面恒空、hips_id 恒占位，与"已闭合"矛盾 | HISTORY provenance **残余**：节点面哈希已真实化，会话面仍恒 nullptr、hips_id 两面仍占位，合同仍整体登记"已闭合"并引用不存在的「SCI-P3 §96」 | C-008 | "全链 provenance 缺失"（节点面已修） |
| L02-013 | 产品/manifest 无「像素/采样语义」声明面 | 产品头/manifest 无「frame 与像素-采样语义」声明面（**语义只在进程内描述符，不随产品落盘**） | G-001 | "全仓无采样语义声明"（descriptor 已有枚举） |

---

## §四 无法判定（整条）: 0 条 —— 5 个子问题转 §六 待复核。凡"锚找不到/行号漂移"一律重锚后再判，未据此驳回任何一条。

---

## §五 剔除与降级显式清单（不得静默丢弃）

### 1) 剔除的半边事实（6 处，逐条可复核理由）

| # | 被剔除成分 | 理由（本轮实测） | 该条剩余定稿 |
|---|---|---|---|
| 1 | L01-006 所引 SCI §8"RMS 与 WCS Gate v2 外部闭环一致" | grep `闭环\|外部闭环\|RMS/p95` 于 docs/science/ → **0 命中**；现 SCI §8 表内无该行 ⇒ 引文不存在，不立案 | A-002（重锚 memory.md + ALG:186） |
| 2 | L02-016 的"往返门被放宽 1e-6→1e-4" | p3_wcs_test.cpp:62-63 同时定义 kRoundtripGatePx=1e-6 与 kFrozenGatePx=1e-4（两门并列，非放宽） | F-004（1e-3↔1e-2 + 常数场判据） |
| 3 | L02-003 的"*has_unc 出参缺失"细节 | 现行 p3_resample.h 无该出参，签名比对对象不存在 | C-007（行数/路径/非目标/签名/coverage 四项） |
| 4 | L01-011、L01-012 的节点侧前提 | 三符号（p1_pick_stars、gaia_n_target、节点 ipv_solve_from_detections_v1）现 0 命中 ⇒ 前提随并发提交消失 | 转 §二-2/§二-3；残余另立 F-003/C-006 |
| 5 | L01-002 的"产品 FITS 头存在 1px 偏移"读法 | 本轮按 Paper I 复算（§八-R1）+ Phase3 九宫格/双桥必败锁 ⇒ 产品出口未见 1px；成立的是"逆向输出为 1-based 却标 0-based + 测试侧以 +1 桥三方对照" | B-003（P1） |
| 6 | L02-013 的"全仓无采样语义声明" | module_adapters.cpp:574 已有 UnitId::SURFACE_BRIGHTNESS / CoordinateFrame::PIXEL（进程内 descriptor） | G-001（收窄为"落盘面缺失"） |

### 2) 优先级调整（降级 2 条 / 加立与拆分 4 条）

| 条 | 变化 | 理由 |
|---|---|---|
| L01-002 | **P0 → P1**（B-003） | 产品出口侧已修且有回归锁；残余属治理登记/注释方向/标签口径，无产品数值后果 |
| L01-016 | **P1 → P2**（B-006） | 触发需输入自带 CDELT；SIP i+j<2 项实测系数为 0 未写入；无产品失败记录 |
| L02-001 | P0 → **P0 并拆三条**（A-003/B-002/F-001） | 实质偏离、注册表不实声明、oracle 共源三面各自独立可复现；合并会掩盖治理面（母控要求单列后两条） |
| L01-004 | P0 + **加立加重条 B-001（P0）** | 母控指定加重事实：注册表把 41×41/81×81 描述为"7×7…SCI 已显式冻结"并评 severity 低（§八-R8） |
| L02-014 | P1 保持，**标注升格候选 P0** | 代码事实确定；"高并发可达"须运行验证（§六-2），验证为可达即升 |
| L01-006 | P0 **保留**（重锚） | 精度宣称与仓内唯一外部实测互斥（0.887″ > 0.5″ 门）且判 PASS、无偏差登记 ⇒ 门禁与证据断链 |

## §六 待复核清单（只读不能定者；标注所需权限）

| # | 子问题 | 相关定稿条 | 为何只读不能定 | 所需权限 |
|---|---|---|---|---|
| 1 | FOV≤20° 超限帧的**实际出片形态**（57.5°～90° 环带 NaN 是否出现在真产品） | C-004、A-010 | 需以真实 s_out/尺寸跑 Phase3 并读回产品 | 运行 `astrocs phase3 run`（或等价 ctest） |
| 2 | 节点 resample 的 sampler open 失败在真实并发下的可达性（fd/内存） | H-001（及其升格判定） | 需 fd 耗尽 / 并发压力注入 | 运行 + 资源注入 |
| 3 | 端到端 IPV 链改接后的**数值**回归（是否静默退化） | F-003、F-005 | 需真实 Gaia 目录 + 真图跑通并复算 rms | 运行 + testdata/Gaia |
| 4 | IVOA HiPS 1.0 `hips_frame` 枚举行的**原文** | B-005 | aanda 站本轮返回 403；arXiv 渲染窗口未含该枚举行（本轮判定按通识，已在条内标注"待正式核验"） | 外网可访问规范原文或其镜像 |
| 5 | inlier 权威缓冲**实际生效语义**（+0.5 中心 vs 图像中心 Y 向上） | D-001 | 两注释互斥为事实，孰生效需看回写面 | 运行 + 日志/工件核对 |
| 6 | 本地 XPSD 导出端是否已在导出时作 J2016.0→J2000 归算 | A-006 | 导出工具面位于数据/免报区，仓内 grep 不到传播 | 数据核对（GaiaDR3*/testdata 侧） |

---

## §七 行号漂移处置（专项）

**纪律**：行号漂移从不作为驳回理由。全部 40 条先按符号/关键语句重定位再定稿；所有定稿 finding 的「位置」字段一律写成 `<path>::<symbol>`（不与 `path:line` 混用）。

| 对象 | 叶子/合同锚 | 复核时点实测 | 处置 |
|---|---|---|---|
| SCI §5:66 一致性锚 | `ipv_wcs.cpp:13-16,153-164,274-420,530-576` | 网格 LSQ 在 :402-527、牛顿迭代在 :790-872 | 重锚为 `::extract_wcs_sip` / `::solve_pix2world_iterative`，并在 A-001 内记该锚自身失真 |
| module.yaml 12 个 C ABI 锚 | :237/:249/:260/:273/:286/:314/:328/:345/:377/:524/:566/:610 | :266/:278/:289/:302/:315/:343/:357/:374/:406/:553/:595/:639（ipv_entry.cpp 现 678 行） | 系统性偏 −11～−29 行 → E-001 |
| ALG 符号表 ipv_wcs 锚 | :256-266 / :264-277 / :283-290 / :322-365 | CRPIX :288-289；det_lin :340-343；Y 翻转 :655-696 | 偏 18～127 行 → E-001 |
| SCI 符号表 cd_inv 锚 | `ipv_wcs.cpp:328-331` | 实测 :340-348 | 偏 12 行 → A-004 内记 |
| p1wcs_oracle.hpp 量纲错自证 | 叶子记 :60-61 | 实测 **:228** | 重锚；引文逐字复核通过 → A-001 |
| RSMP 实现头文件 | `lib/phase3_rsmp/p3_resample.h`（58 行） | 真实路径 **`lib/phase3_session/p3_resample.h`（116 行）**；`lib/phase3_rsmp/` 仅剩 README/module.yaml/memory.md | 重锚 + 路径失真立案 → C-007 |
| PROJ 实现合同行数 | p3_wcs.h 50 / p3_wcs.cpp 165 | 63 / 230 | → E-003 |
| 节点重采样 worker | 叶子 :4787-4792 / :4795-4799 / :4869-4875 / :4879-4881 | :4827-4829 / :4832-4836 / :4913-4918 / :4942-4946 | 重锚，事实原样成立 → H-001 |
| module_adapters 的 p1_ipv 面 | 叶子 :1758→:1967、:2003-2050 | :1866-2036、:2062-2100（并发重写） | 按现树重述；其"绕链"前提判已修 → §二-2 |
| L01-018 / L01-019 原锚 | `lib/phase1/ups/build_upm.cpp`、`README:4831/:24178` | 路径与锚 **0 命中**（该目录不存在） | 重锚至 ipv_types.h 注释族 / ALG:89 vs SCI:162 → D-002、E-004 均成立 |
| L01-006 原锚 | SCI §8 表格行 | docs/science/ 0 命中 | 该半边剔除（§五-1），其余重锚后成立 |
| L02-012 的「SCI-P3 §96」 | 该节号 | PHASE3_HIPS_TO_FITS.md 共 **143 行**，grep `96` → 0 命中 | 定稿为断链事实 → C-008 |

---

## §八 独立复算与外部原文验核记录（本轮亲做）

### R1 CRPIX / 像素原点复算（Paper I §2.1.1）
标准：`xp = x + 1`，`(ξ,η) = CD·(xp − CRPIX)`。
- 生产前向（网格构造 :446-452）：`u = x − x00`，`x00 = crpix − 1` ⇒ `ξ = CD·(x0 + 1 − crpix)` ⇒ **与标准一致**（输入为 0-based 数组下标）。
- 生产逆向（:801-813 与 :869-872）：`uv = CD⁻¹·(ξ,η) = x0 − (crpix−1)`，返回 `out.x = u + crpix = x0 + 1 = xp` ⇒ **逆向输出是 1-based 数值**，而 `ipv_wcs.h:70-71` 注为"0-based FITS 像素"。
- 第三方：astropy `origin=0` 时 `u = x + 1 − crpix'`，要与生产对齐须 `crpix' = crpix + 1` —— 正是 `p1wcs_astropy_cross.py:296` 与新测试 `p1wcs_std_f1_bridge_cross.py:59`（`BRIDGE_DECLARED = 1.0`）所做的事。
⇒ 三条结论：①"内层 0-based 自洽"的标签与数值语义不兼容（B-003）；②Phase3 产品 FITS 头侧本轮未见 1px（故不采信产品级读法，§五-5）；③`p1_op_wcs` 自检环是**边界之外的第三处**未桥接面，正对应 SCI §10:137 所禁的"缺失 +1 桥接 ⇒ 恒定 1px"（C-003）。

### R2 SIP 门可达性与路径归属
| 路径 | 阶/网格 | 可经 C ABI 导出 | 被 1e-4 门断言 |
|---|---|---|---|
| 前向 A/B（解析） | 阶 5 | 是（A[36]/B[36]） | 间接（经迭代门） |
| **一步 AP/BP（导出面）** | 阶 5 / 41×41 | **是** | **否**（apbp:256-257 仅 50 px"非验收线"；F2 实测 ~7.6 px） |
| APx/BPx（扩展一步） | 阶 7 / 81×81 | **否**（wcs_transform.cpp:46-50 限 [0,5]） | **是**（bridge 交叉测试 :304-305 喂 astropy 的正是 APx 10×10） |
| 迭代反演 | — | 是（函数调用） | 是（:514-521 自述"由迭代反演达成"） |
⇒ 被 1e-4 门钉住的两条路径都不是下游可消费的系数面；可消费的 36 项一步面自证"数学不可达"。故 SCI §15:172 的"Oracle 全过"字面为真、在产品口径上失真（C-001 + B-001）。

### R3 CAR/AIT fiducial 与手性（Paper II 原文本轮亲验）
取回 `https://ar5iv.labs.arxiv.org/html/astro-ph/0207413`（HTTP 200，约 100 KB，尾部截断）。决定性段落（逐字）：
> "…a fiducial celestial coordinate pair (α₀,δ₀) given by the CRVAL_ia will be associated with a fiducial native coordinate pair (φ₀,θ₀) defined explicitly for each projection. For example, zenithal projections all have (φ₀,θ₀)=(0,90°), while cylindricals have (φ₀,θ₀)=(0,0). The AIPS convention has been honored here as far as practicable by constructing the projection equations so that (φ₀,θ₀) transforms to the reference point, (x,y)=(0,0)."
> 同节："In cylindrical projections, where θ₀=0, the default value for LONPOLE_α is 0 for δ₀ ≥ 0, but it is 180° for δ₀ < 0"，其理由为"the condition for the celestial latitude to increase in the same direction as the native latitude at the reference point"。
诚实标注：`grep 'Table 1|carree|Aitoff'` 在取回窗口内 **0 命中** ⇒ §5 Table 1 的逐行比对本轮未完成（不影响判定：§2.2 的 fiducial 配对条款即为充分依据）。aanda 镜像 `/aah3860/aah3860.right.html` 返回 **403**，故取 arXiv 渲染版（A&A 与 arXiv 同文）。
复算：实现 `dec = θ = −y·kRad·kDeg` ⇒ CRPIX 处（x=y=0）`dec = 0°`，与头里写出的 CRVAL2 相差 |δ₀|；像素错位 `Δ = δ₀/s_out`（δ₀=30°、s_out=0.5″/px ⇒ 216 000 px）。又 `east_left ⇒ CD = diag(−s, +s)`（:56-68），与 `θ = −y` 合成 `∂dec/∂y = −s < 0`（行号增大→纬度下降），与写出的 CD2_2=+s、CUNIT2='deg' 向标准读者宣告的方向相反；ALG:398-401 却把该镜像写成"CRVAL2 仅记录于 header 不进入映射"的**显式冻结声明**（即缺陷被制度化）。

### R4 FOV 可达性（修正叶子措辞）
前向半球门 `r ≥ π/2`，而 `r = tan d` ⇒ 拒绝阈 `d ≥ arctan(1.5708) = 57.5194°`（**不是** 90°）。以 W=H=20000（kMaxSide）计：s_out=0.5″/px ⇒ 半对角 = 14142×0.5″ = 1.964°（不触门）；要触门需 `s_out×半对角 ≥ 57.5°`（例 s_out≈20.7″/px）。
⇒ 精确表述：**任何 FOV 都不被拒绝（20° 约束零强制点）**；仅当半视角越过 57.5° 时，界外像素被逐点静默留 NaN/覆盖=0，且该行带仍计数并发布。叶子"≈140° 静默出片"不精确，已改写（C-004 / A-010 / H-001 三处同步）。

### R5 半球界不对称
正向 `d ≥ 57.5194°` 即 P3_WCS_HEMISPHERE；反向 `denom = cos d ≤ 0` 即 `d ≥ 90°` ⇒ `d ∈ [57.52°, 90°)` 内"像素→天球可用、天球→像素被拒"，正反映射定义域不互补（宽 32.48° 单侧环带）。合同 DATA:2189 称二者"数学等价"，与上式矛盾（A-010）。

### R6 天测闭环与门
memory.md:55-56：T3 内 0.151 px（31 pairs）vs 外 0.897 px（702 matched）；T2 0.108 vs 0.772（1237）。按同记录 s0≈0.989″/px 折算外部中位 ≈ **0.887″**，超 ALG:186 的 F1 门 0.5″ 约 **1.77×**（内部口径 0.149″ 通过）。:59-63 另记 Y 向 0.848 vs 0.218 与"Q4 象限偏多（两帧一致）"（系统性非随机），仍判 **VERDICT: PASS**，且诊断工具位于 `engineering_v1.2/evidence/**`（免报区，不可作 CI 证据）。⇒ 门的量测域未定义（内点 trans 域 vs 全帧头域）是本条根因（A-002）。

### R7 历元传播量级
DR3 自行典型 1–10 mas/yr；J2016.0 → 观测历元 ~10 yr ⇒ 10–100 mas，即 0.01–0.1″，在 s_out≈0.5″/px 下为 0.02–0.2 px；高自行/长基线可达亚像素级。与 R6 的 Y/象限偏置在量级上相容（非充分解释，属必查上游项，A-006）。

### R8 注册表加重事实（母控指定）
registry:66 逐字：`DISP-WCS-006（AP/BP 以 7×7 网格最小二乘拟合而非标准迭代反演；SCI 层已显式冻结该口径）`；:76 严重度 **低**，处置列"AP/BP 网格拟合口径**与 SCI 冻结一致**，维护歧义"；:241 状态 TRACKED。代码事实：41×41 阶5（:402-403）+ 81×81 阶7（:526-527），且 :397-400 自证"冻结 1e-4 px 在该 fixture 下**数学不可达, 需负责人裁决 (finding)**"。⇒ 注册表三处与代码事实、与"待裁决"状态双双互斥，据此定的 severity 低估。**定稿 M1a-B-001（P0，related: F00-06）**。

### R9 oracle 共源（母控指定，宪章 §12.3 末段）
先读测试原文再下判：`p3_projection_test.cpp:212-217` 与 `test_p3_projection_oracle.py:153-157/:189-195` 的 CAR/AIT 期望式即 `dec_o = −y_deg`、`ra_o = ra0 + x_deg`（dec0 完全不进期望式），与 `p3_projection.cpp:198/:204/:216` 同一推导同一符号；**同文件 TAN/SIN 分支改用球面基 `basis(ra0, dec0)` 独立推导** ⇒ 本可独立而未做。往返自洽（<1e-6 px）同样不能发现该错（错映射自反一致）。⇒ 对 CAR/AIT，"独立 Oracle 在位"（ALG:467、registry:64 的 CONFORMANT 依据）不成立；**定稿 M1a-F-001（P0）**。

### R10 cd_inv 3600×
`cd_inv` 正确定义 = `inv(trans.linear)`（pixel/arcsec）；`CD⁻¹ = 3600·inv(trans.linear)`（pixel/deg）。SCI:19 以 `CD^{-1}` 命名却注 `pixel/arcsec` ⇒ 按命名复算即 3600× 错；代码 :338 已自警"不是 result->cd 的逆（差 3600 倍）"，而 :138 只禁 deg/pixel 一侧 ⇒ 文档自相矛盾且禁不掉实际风险（A-004）。

### R11 F6 单位域
`wcs_tan.h:2` 与 ALG:201-202 钉 `<1e-6 deg`；`p1_wcs_phot_test.cpp:50` 断言 `fabs(x2−x) > 1e-6`（px）；角域仅在 CRVAL 锚点单点检查（:57-59）。s_out=0.5″/px 时 1e-6 px = 1.4e-7 deg（更严但不同量）；换尺度即失去等价（F-002）。

### R12 其他实测锚（供前台拼装 INDEX/SUMMARY）
- 现树规模：`p3_wcs.h` 63 行 / `p3_wcs.cpp` 230 行 / `p3_resample.h` 116 行 / `ipv_entry.cpp` 678 行 / `p1wcs_std_f1_bridge_cross.py` 467 行。
- TAN-only 的四道独立强制点：`p3_wcs.cpp:58-64`、`p3_session.cpp:95-97`、`module_adapters.cpp:4499-4513 / :4547 / :4617-4628`、`cli/runtime_client.cpp:52-60` ⇒ CAR/AIT 目前仅库级暴露（"错误口径已固定化但产品未受损"的定级依据）。
- STD-F1 回归锁登记处：`tests/unit/p1wcs/CMakeLists.txt:150` + `ci/checks.json:3117`。

---

## §九 跨域移交清单（不在本域定稿，以 related 互指）

| 移交对象 | 事项 | M1a 定稿条 | 说明 |
|---|---|---|---|
| **M6 / L13** | DISP-WCS-006（SIP 网格口径与 1e-4 门） | C-001 + B-001 | 共签：**M1a 定稿**，M6 侧以 `related: L01-004` 指回，不得两处各定一份 |
| **L09（Phase1 上游/静默降级族）** | 逐资源打开失败被静默吞（worker open / continue / band 计数） | H-001（`related: L09-003`） | 同一形态多处复现，建议前台按根因合并展示 |
| **L10（星表/获取域）** | 历元传播缺失、pm/parallax 置 0、XPSD 导出端归算待核 | A-006（`related: F00-06, L10-001`）+ §六-6 | 若导出端已归算，A-006 只余"无历元入参/无误差预算"两半 |
| **F00（前台基准与登记面）** | registry 三处不实（SIP 描述 / Paper II CONFORMANT / STD-F1 已闭环） | B-001、B-002、B-003 | 三条同属"合规声明与实现或裁决状态互斥"，但证据面各自独立，前台去重时**不可并成一条** |
| **astro_image_io（HiPS 写出域）** | `aio_hips_writer.cpp:905/:1141` 写死 `equatorial` | B-005 | 本域只定稿消费面（恒等当 ICRS），写出面归图像 IO |
| **CI / 测试域** | oracle 独立性（§12.3 末段）、并发修复未固化、SYN-007 容差表缺失 | F-001、F-004、F-005 | 建议新增轻门：对合同内 `*.cpp:NNN` 形态锚做存在性/邻近符号校验（防再漂移） |
| **负责人裁决请求（经前台）** | ①WCS-003-F1 是否书面结案 ②CAR/AIT 口径（改映射 or 强制 CRVAL2==0）③FOV≤20° 是否设硬门 ④ivar==0 单裁 ⑤APx 是否进导出 ABI ⑥SIP 冻结门适用路径 | B-003、A-003、C-004、A-009、C-001、C-001 | 六条均为只读面不能代裁项，且全部阻断 §17.1 相关证据项 |
| **免报区证据声明** | `run/std_f1_adj/**`、`run/tmp_wcs002`、`engineering/control/archive/**/docs/24_*,25_*`、`engineering_v1.2/evidence/P11-002/scripts/**` | A-002、E-001、E-002、B-001 | 这些路径按规程免报，但**被权威文档当引用目标**即构成断链；已在各条内区分表述，未据此驳回任何一条 |
| **L15（属 M7 域）** | 本轮未发现与 L15 重叠事实 | — | 若前台发现同文件重叠，请以 M7 为准并回指 |

---

## §十 定稿统计

| 类别 | P0 | P1 | P2 | 小计 | 文件 |
|---|---|---|---|---|---|
| A_SCI_DEF | 3 | 7 | 0 | 10 | `A_SCI_DEF/p0/M1a_L01_L02.md`、`A_SCI_DEF/p1/…` |
| B_STD_MISMATCH | 2 | 3 | 1 | 6 | `B_STD_MISMATCH/p0\|p1\|p2/M1a_L01_L02.md` |
| C_DOC_CODE_GAP | 4 | 4 | 0 | 8 | `C_DOC_CODE_GAP/p0\|p1/M1a_L01_L02.md` |
| D_COMMENT | 0 | 1 | 2 | 3 | `D_COMMENT/p1\|p2/M1a_L01_L02.md` |
| E_TRACE_BREAK | 0 | 3 | 1 | 4 | `E_TRACE_BREAK/p1\|p2/M1a_L01_L02.md` |
| F_TEST_GAP | 1 | 4 | 1 | 6 | `F_TEST_GAP/p0\|p1\|p2/M1a_L01_L02.md` |
| G_GOV_GATE | 0 | 2 | 1 | 3 | `G_GOV_GATE/p1\|p2/M1a_L01_L02.md` |
| H_NUMERIC | 0 | 1 | 0 | 1 | `H_NUMERIC/p1/M1a_L01_L02.md` |
| I_DOC_HYGIENE | 0 | 0 | 3 | 3 | `I_DOC_HYGIENE/p2/M1a_L01_L02.md` |
| **合计** | **10** | **25** | **9** | **44** | 18 个合并文件（每「类别×优先级」一个） |

- 40 条来源 → 44 条定稿：+1（母控指定加重事实 B-001）、+1（oracle 共源 F-001）、+1（并发修复未固化 F-005）、L02-001 拆 3 条（净 +2）、L01-002/011/012/ L02-012 各按残余定稿（1:1）。
- 每条定稿字段齐备：类别码 / 优先级 / 位置(`path::symbol`) / 逐字证据摘录 / 权威依据（宪章条款号、SCI-ID、标准条款、合同 ID）/ 问题说明 / 影响 / 建议处置 / 置信度 / related / 来源(Lxx-nnn)。

