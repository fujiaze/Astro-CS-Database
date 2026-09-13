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

<!--PART2-->
