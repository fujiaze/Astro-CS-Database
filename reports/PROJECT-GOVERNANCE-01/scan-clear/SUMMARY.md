# SCAN-CLEAR 台账清零核查 —— SUMMARY

> 任务：`工程控制/PROJECT-GOVERNANCE-01/tasks/SCAN-CLEAR.md`（阶段 1）　台账：`问题扫描/REBASE_TABLE.md`（785 行，md5 7f96ef632badda9e39d14dc897e386a6）
> 基线锚点：HEAD `fdf6296b1dd2`（main）；复跑锚点见 `run/PROJECT-GOVERNANCE-01/SCAN-CLEAR/anchor.json`（含关键文件 sha256 前 16 位）。**复核期间仓库由并发执行线持续推进**（HEAD 在 1 小时内前进 4 次），文档/代码随时间漂移 ⇒ 本报告结论以该锚点时刻的树为准。
> 纪律：**零 git 写**；未改 `问题扫描/**` 与任何被跟踪文件；产物只落 `reports/PROJECT-GOVERNANCE-01/scan-clear/**` 与 `run/PROJECT-GOVERNANCE-01/SCAN-CLEAR/**`。

## 0. 结论（先看这一段）

- 台账 **785 行全覆盖**，逐行判定见 `REBASE_VERDICT.csv`（列：ID/原判据摘要/判定/证据/归属任务/根因/方案/验收门/依据/备注）。
- 计数：**FIXED 138 · OPEN 606 · CRITERION-WRONG 27 · NOT-APPLICABLE 12**（四类之和 = 783 = 台账行数 785 − UNVERIFIABLE 2）；另有 UNVERIFIABLE 2 条（单独列，见 §4）
- 与 `DISPATCH_MATRIX.md` 既有计数：**总量与四态合计逐项相等**（条目 785 = 785；OPEN 727、RESOLVED 42、VOID 13、UNVERIFIABLE 3），差异只在 10 个归属的 ±1..3 条**归属漂移**（正负相抵、净 0），逐项对差见 §2。
- **判据错清单 27 条**（见 §5）：每条给「为什么错 + 正确口径 + 依据」，此类**不得按原处方整改**。

## 1. 四类计数（唯一口径）

| 判定 | 条数 | 语义 |
|---|---|---|
| FIXED | 138 | 缺陷对象在当前树已不存在/已改正（每条有复跑命令+rc） |
| OPEN | 606 | 当前树仍违反（每条有复跑命令+当前原文） |
| CRITERION-WRONG | 27 | 判据本身错（对象错/前提不成立/算术错/权威越界/自证门），不得按原处方整改 |
| NOT-APPLICABLE | 12 | 对象已退役/删除/移出构建图（有退役记录） |
| UNVERIFIABLE | 2 | 证据不足（单独列，不计入四类之和） |

对差说明：台账原四态（OPEN 727 / RESOLVED 42 / VOID 13 / UNVERIFIABLE 3）与本轮四类**不是同一分类轴**——台账问「对照最新权威条款是否仍成立」，本轮问「当前树现状是否仍存在该缺陷对象」；`RESOLVED/VOID` 在本轮分别落到 `FIXED` 与 `NOT-APPLICABLE/CRITERION-WRONG`，`OPEN` 按当前树重新分流为 `OPEN/FIXED/CRITERION-WRONG`。逐行映射见 CSV。

## 2. 与 DISPATCH_MATRIX.md 的逐项对差

| 归属 | 台账(条目/OPEN/RESOLVED/VOID/UNVERIFIABLE) | 矩阵(同序) | 差(条目,OPEN,R,V,U) |
|---|---|---|---|
| AIO-001 | 35/34/1/0/0 | 32/31/1/0/0 | **(+3,+3,+0,+0,+0)** |
| ARCH-001 | 13/12/0/1/0 | 14/13/0/1/0 | **(-1,-1,+0,+0,+0)** |
| CFG-001 | 4/4/0/0/0 | 4/4/0/0/0 | 0 |
| CI-001 | 95/89/4/2/0 | 94/88/4/2/0 | **(+1,+1,+0,+0,+0)** |
| CLI-001 | 9/6/3/0/0 | 9/6/3/0/0 | 0 |
| CLI-002 | 2/2/0/0/0 | 2/2/0/0/0 | 0 |
| CLI-003 | 7/7/0/0/0 | 7/7/0/0/0 | 0 |
| CPU-001 | 17/17/0/0/0 | 16/16/0/0/0 | **(+1,+1,+0,+0,+0)** |
| DATA-001 | 36/34/2/0/0 | 36/34/2/0/0 | 0 |
| DOC-001 | 93/85/5/2/1 | 93/85/5/2/1 | 0 |
| FINAL-001 | 3/1/0/1/1 | 3/1/0/1/1 | 0 |
| GOV-001 | 28/25/0/3/0 | 28/25/0/3/0 | 0 |
| INT-001 | 5/5/0/0/0 | 4/4/0/0/0 | **(+1,+1,+0,+0,+0)** |
| MOD-001 | 38/38/0/0/0 | 39/39/0/0/0 | **(-1,-1,+0,+0,+0)** |
| NEXT-PACK:NP-01 | 8/4/3/1/0 | 8/4/3/1/0 | 0 |
| NEXT-PACK:NP-02 | 4/4/0/0/0 | 4/4/0/0/0 | 0 |
| NEXT-PACK:NP-03 | 2/1/0/0/1 | 2/1/0/0/1 | 0 |
| NEXT-PACK:NP-04 | 2/2/0/0/0 | 2/2/0/0/0 | 0 |
| NEXT-PACK:NP-DC-01 | 8/8/0/0/0 | 8/8/0/0/0 | 0 |
| NEXT-PACK:NP-DC-02 | 15/15/0/0/0 | 15/15/0/0/0 | 0 |
| NEXT-PACK:NP-DC-03 | 1/1/0/0/0 | 1/1/0/0/0 | 0 |
| NEXT-PACK:NP-DC1 | 4/4/0/0/0 | 4/4/0/0/0 | 0 |
| NEXT-PACK:NP-GAIA-01 | 3/3/0/0/0 | 3/3/0/0/0 | 0 |
| OBS-001 | 25/22/2/1/0 | 25/22/2/1/0 | 0 |
| P1-001 | 113/105/8/0/0 | 112/104/8/0/0 | **(+1,+1,+0,+0,+0)** |
| P1-002 | 40/34/5/1/0 | 42/36/5/1/0 | **(-2,-2,+0,+0,+0)** |
| P2-001 | 42/38/4/0/0 | 42/38/4/0/0 | 0 |
| P2-002 | 20/18/2/0/0 | 21/19/2/0/0 | **(-1,-1,+0,+0,+0)** |
| P3-001 | 16/15/0/1/0 | 18/17/0/1/0 | **(-2,-2,+0,+0,+0)** |
| P3-002 | 24/23/1/0/0 | 24/23/1/0/0 | 0 |
| PKG-001 | 20/20/0/0/0 | 20/20/0/0/0 | 0 |
| QA-001 | 20/20/0/0/0 | 20/20/0/0/0 | 0 |
| REAL-001 | 2/2/0/0/0 | 2/2/0/0/0 | 0 |
| ROOT-001 | 1/1/0/0/0 | 1/1/0/0/0 | 0 |
| ROOT-002 | 2/2/0/0/0 | 2/2/0/0/0 | 0 |
| ROOT-003 | 2/2/0/0/0 | 2/2/0/0/0 | 0 |
| RT-001 | 26/24/2/0/0 | 26/24/2/0/0 | 0 |

**合计**：台账 `{'条目': 0, 'OPEN': 727, 'RESOLVED': 42, 'VOID': 13, 'UNVERIFIABLE': 3}` vs 矩阵 `{'条目': 785, 'OPEN': 727, 'RESOLVED': 42, 'VOID': 13, 'UNVERIFIABLE': 3}`。
差异全部为**归属漂移**（矩阵由 26 片 PSV 聚合产生，台账为合并后定稿；两轮之间前台按域重派过 10 条），各归属正负相抵、净 0，**四态合计逐项相等**。
## 3. 未清零清单（OPEN，按域分组）

> 共 **606** 条。每条含：ID / 一句话缺陷 / 根因 / 方案 / 归属 / 验收门。完整命令与当前原文见 `REBASE_VERDICT.csv` 的「证据」列。

### P1-001（89 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M1a-A-001 | 【人工复核覆盖：原分批复核判 OPEN】台账原判 OPEN/P0。 | R-3 要求「§5 及 §5a」一并订正；现状只改了 §5 主式，§5a 的桥接条款仍是漏乘 CD 的简写 | 把 §5a:78 的简写改为与 §5 等价的显式式（或写 CD·[ (xp−CRPIX) + SIP_A/B(xp−CRPIX) ]），并在该句回指 §5 的符号定义 | docs/science/ASTROMETRY.md 内 CD·(xp − CRPIX) + SIP（不含方括号形式）零命中（grep -F + rc=1），且 §5/§5a 两式逐符号一致 |
| M1a-A-004 | 台账 :19/:29 现漂到 :20/:30 | 同一符号 cd_inv 既名 CD^{-1}（按同表 CD: deg/pixel ⇒ 应 pixel/deg）又注 pixel/arcsec；实现 inv(trans)≠inv(CD)，相差 3600×，§10:141 … | 把该行符号改为 inv(trans)（或 (3600·CD)^{-1}）并保留 pixel/arcsec 单位注；§5a:78 的 CD⁻¹ 逆式同步订正；实现零改动 | grep -n 'cd_inv' docs/science/ASTROMETRY.md 中不再出现 'CD^{-1}' 命名；§2 符号表与 §5/§10 单位自洽 |
| M1a-A-005 | s_min/s_max=0.90/1.10 现只在 ipv_types.h:226-227，PLATESOLVE.md 已无该窗（rc=1） | 0.002(0.2%) 与 2% 门被写成「同源」（相差 10×，且量类不同：尺度各向异性 vs CD 元素相对误差）；good_rms_threshold 结构注释写角秒、实现按像素比较 | §11.4 删除/改写「同源」表述并把 2% 门的标定依据落到 GATES_AND_TOLERANCES.md；ipv_types.h:225 单位注改为「像素」（与 ipv_ransac.cpp:485 一致）；不改门… | grep -n '同源' docs/algorithms/PLATESOLVE.md 无 0.002↔2% 关联；grep -n 'good_rms_threshold' ipv_types.h 单位注与 ipv_ran… |
| M1a-A-006 | 台账签发时 STANDARDS_REGISTRY.md 曾被记为已删，实测存在（ls docs/standards rc=0） | SCI 权威把 Gaia DR3 位置记为 J2000（实为 ICRS@J2016.0），与注册表/上游 DR3 数据模型冲突；全仓无历元/自行/视差传播，ipv 入口无 date_obs | 把 §3a/§6 的 ICRS/J2000 改为 ICRS（Gaia DR3 参考历元 J2016.0），并在 §2 单位表注明不传播自行/视差；合同侧同族归 DATA-001 | grep -rn 'ICRS/J2000' docs/science/ASTROMETRY.md 零命中；与 STANDARDS_REGISTRY.md:202 表述一致 |
| M1a-B-005 | HIP-001/AIO-001 未覆盖该键；行号 951→1081（漂移 130） | 写出侧写死非标准枚举值 equatorial（IVOA REC-HIPS-1.0 §4.4.1 值域 icrs/galactic/ecliptic）；SCI 冻结文本仍带未决问号，读取侧（phase3_session）放… | 写侧按实际内部 frame 写 icrs（或按帧映射到标准值域，非法 frame fail-closed）；PHASE3_HIPS_TO_FITS.md §3 去问号并冻结 icrs；读取侧校验随 INT-001 删除 | grep -n 'equatorial' aio_hips_writer.cpp 在 hips_frame 键处零命中；产 properties 的 hips_frame ∈ {icrs,galactic,eclipti… |
| M1a-B-006 | CDELT 命中 2 处仍在（:4226-4227→:4236-4237），无删除路径 | 同一头面同时写 CD 与 CDELT（FITS WCS Paper I §1.1/§8.1：二者同现行为未定义）；SIP 回写未跳过 i+j<2 的未定义项 | 写回块在写 CD 时删除/不写 CDELT（读入 passthrough 应在写回前剥离）；SIP 写回从 i+j≥2 起，回读面同步；不改科学值 | 头面断言 CDi_j 与 CDELTi 不同现（正负例各一）；grep -n 'A_0_0\/A_1_0' orchestrator.cpp 写回块零命中 |
| M1a-C-003 | 行号 1920→2149/2384 漂移，结构未变；文件域属 scheduler/INT-001 | 像素原点契约（1-based WcsTan vs 0-based 数组下标）无独立参考；自检环与参考解同用 0-based x，原点平移误差在门内不可见 | 自检环改用 1-based 像素 xp=x+1（或显式声明 WcsTan 接受 0-based 并加原点桥断言）；参考解独立施加 xp−CRPIX，另加一条 x=0 边界断言 | 新增正例（正确原点）与负例（人为 1px 原点错位必红）；自检环与独立参考解不再共用同一 x |
| M1a-C-005 | 台账 :166-167 → 现 :169-171（漂移 3） | §3 伪代码仍把 legacy IRLS 路径写成生产 SIP 步并锚 ipv_sip.cpp，与 §11.1 已登记的双路径口径互斥（登记 DISP 不等于订正伪代码） | §3 伪代码改为 'sip = extract_wcs_sip(trans)：TRANS 解析 A/B + 采样网格 ≥7×7 反变换 AP/BP (ipv_wcs.cpp:229)'；legacy IRLS 只在 DI… | grep -n 'IRLS' docs/algorithms/PLATESOLVE.md 仅出现在 legacy/DISP 段落；§3 生产流程与 extract_wcs_sip 一致 |
| M1a-C-006 | 头注与 :745 排序实现本批实测已改正（台账第一子项 FIXED），残余为默认值/文档两条 | 三套口径互斥：结构默认 20/注释 20→40→60 vs solver 硬编码 60；文档选星行仍写 flux 降序（实码 mag 升序） | 选星行改为 mag(box 积分) 升序；ipv_types.h:233 默认值与 :232 注释同 solver 的 60 对齐（或把 60 提升为 params 字段并在文档登记）；不改排序算法 | grep -n 'flux 降序' docs/algorithms/PLATESOLVE.md 零命中；grep -n 'img_n_target' 默认值/注释/调用点三者同值 |
| M1a-D-002 | 部分修复：:26 已改对，:82 为同族残余 | 同一结构内 U 向量单位两说（:26 像素 vs :82 角秒），实码为像素；SIPCoeffs 阶数注已修 | 把 :82 改为「图像侧星点 (像素, 原点图像中心, Y-up)」；W 保持角秒 | grep -n '图像侧星点' ipv_types.h 单位注与 ipv_select.cpp:948 一致 |
| M1a-F-002 | 行号 50 与台账一致；p1_wcs_phot_test.cpp 使用 WcsTan（w.pix2sky/sky2pix） | 合同文本（deg）与断言（px）不同域：像素域往返恒自洽，不覆盖角域全域误差；无 deg 域 roundtrip 断言 | 在 p1_wcs_phot_test.cpp 补角域往返断言（sky→pix→sky，/ΔRA·cosDec/、/ΔDec/ < 1e-6 deg，含 CRVAL 大角与极区点）；或把 wcs_tan.h 合同改为像素域… | 测试含 deg 域断言且注入 1e-5 deg 偏差必红（正负例）；合同单位与断言单位一致 |
| M1a-F-003 | 台账「tests 全树 0 命中」已被 1 处子串命中取代，结论不变 | PLATESOLVE.md:197 冻结的 Galaxy_Center=0.1431″ 锚无可执行 fixture；端到端仍只断产物存在性 | 把 0.1431″ 实场锚落为仓内 fixture（真帧子集或冻结期望数组）并在 p1wcs 套件中断言（rtol 按 F1）；无真实数据则登记 UNVERIFIABLE 到 GATES_AND_TOLERANCES 证… | 新增锚用例可复跑且注入 0.5″ 偏差必红；tests/ 内 0.1431 命中不再是无关数组子串 |
| M1a-F-005 | module_adapters.cpp 行号 5338/5852→5340/5854（漂移 2） | 三处并发修复落在真源（provenance 哈希、IPV 端到端链、选星归一）但无回归断言，追溯面无机器可红点 | 为 p3n_input_manifest_hash 增节点级回归用例（改输入 manifest → 产物哈希变，未改 → 稳），并覆盖 IPV 端到端链与选星归一的位断 | 新增用例正例绿/负例（篡改 manifest）必红；cmake/ctest 注册 |
| M2a-A-1 | 需科学裁决（docs/science 只读）；行号 40/61/81/117 与台账逐字一致 | 三种面亮度陈述（w=a/A_drop、S_p=B0、常数场 S_p=C）在 pixfrac≠1 下不可同真：复算 S_p=B0/pixfrac²，仅 pixfrac=1 等于 §7 的 S_p=B0 | SCI 侧冻结唯一定义（面亮度 S_p=B0 且声明 pixfrac 归一），或把 §7 改为含 pixfrac 的守恒式；不改实现（代码逐字落地 §5） | DRIZZLE.md 内三式在 pixfrac=0.8 数值解一致（oracle 复算）+ 文档无互斥断言；走 SCI 变更流程签字 |
| M2a-D-1 | 部分修复（代码面 FIXED，文档面 OPEN）；台账 1.25/1.15 双子项已消失 | 代码注释已订正，ALG 权威常数表仍写错误值 211034.6（相对偏差 2e-4）；1.25/1.15 双裕量已收敛为 1.25 单系数（1.15 零命中） | DRIZZLE_GEOMETRY.md:87/:89 常数改为 211076.3（或写表达式 sqrt(π/3)·(180/π)·3600），并在 §buffer 说明与 HP_CIRCUMRADIUS_FACTOR=1… | docs/algorithms/DRIZZLE_GEOMETRY.md 中 '211034.6' 零命中；常数表值与 drizzle_engine.cpp:685 表达式一致 |
| M2a-H-4 | 行号 1026/1030→1030/1033（漂移 3-4） | variance 块 dtype=FLOAT64 仍被逐元素 static_cast<float> 收进 float 缓冲；signal 侧已强制 dtype 与 precision_mode 一致，两侧口径不对称，FP… | 方差缓冲按 dtype 选 double/float（或按 precision_mode 分派模板），与 signal 侧同规则；补 FP64 variance 精度用例 | pm=1 + FLOAT64 variance 输入下方差累加保持 double（精度断言）；两侧 dtype 规则一致 |
| M2b-B-01 | writer:137→204、reader:41、hips_core.c:558；三处同型未修 | 生产写/读/runtime 三处把商写入 Dir、余数写入 Npix，与 IVOA §4.1 算式（D=(N/10000)*10000，文件名 NpixN）相反；注册表把该实现算式写成『与读侧同一合同』并判 CONFOR… | 三处按标准改为 Dir=(ipix/10000)*10000、Npix=ipix（文件名带完整 tile 号），保留旧路径读兼容（或一次性迁移）；注册表该行改 PARTIAL + DISP | 产出的 tile 路径与 IVOA 示例（10302@order6 → Dir10000/Npix10302.fits）逐字一致；round-trip 读写用例正负例 |
| M2b-B-02 | 台账 writer:293-314→现 :360-381；注册表 :97→待复核 | write_moc_fits 只写 TTYPE1=UNIQ 与 MOCORDER/PIXCOUNT，强制键 ORDERING=NUNIQ、COORDSYS=C 缺失（NUNIQ 全文件 0 命中）；注册表 D.hips … | MOC HDU 补 ORDERING='NUNIQ' 与 COORDSYS='C'（MOC2.0 要求），并在注册表登记/复核；补 Moc.fits 键断言用例 | 产 Moc.fits 头含 ORDERING=NUNIQ 与 COORDSYS=C（断言）；无该键用例必红 |
| M2b-B-03 | 行号 943→1073（漂移 130）；HIPS_WRITER.md 同口径需一并订正 | hips_pixel_scale 写出值为角度制换算后的角秒数（≈211076.3/nside arcsec），IVOA 同名键单位为度 ⇒ 相差 3600×；注册表未登记该偏差 | 写侧改为度（去掉 3600 因子，或写 3600*.../3600 的显式换算并注释），并在 standards 注册表登记该键单位；读侧若有反向换算同步 | 产 properties 的 hips_pixel_scale 与 s_pixel_scale 单位=deg 有断言（对同一 nside 与标准公式复算一致） |
| M2b-B-05 | 行号 1249-1251→1382-1384（漂移 133） | VOTable 头常量串把命名空间 URI 写成 'http:// www.ivoa.net/...'（含空格），产 metadata.xml 根元素不被识别为 VOTable；注册表 D.hips 未登记 | 删除 4 处 'http:// ' 中的空格，恢复合法 URI；补一条 metadata.xml 可被 XML 解析器解析的用例（正负例） | python3 xml.etree 解析产出的 metadata.xml 成功（rc=0）；grep -c 'http:// ' aio_hips_writer.cpp = 0 |
| M2b-B-06 | 行号 958/983→1088/1112（漂移 130） | hips_status 缺第三组克隆策略（受控词表须三组）；hips_hierarchy 用布尔 true 而非聚合方法词（标准示例 mean） | hips_status 补第三组（如 'private master unclonable'）；hips_hierarchy 按实际聚合语义写标准词（或删除该可选键）；更新 standards 注册表 DISP-HIPS… | 产 properties 的 hips_status 三组词、hips_hierarchy 取值在标准词表内，有正负例 |
| M2b-B-09 | runtime/io→lib/infrastructure/aio/io；注册表行 223→232（漂移 9） | 头块无条件写 CHECKSUM 全零占位串且注释自称 updated；verify 把 0 当合法值豁免 ⇒ 交付物带未复算校验和仍判 CONFORMANT | 写路径仅在 write_checksum=1 时写 CHECKSUM/DATASUM 卡（否则不预留），verify 侧对 0 全零串判失败；注册表该行改为 PARTIAL 并挂 DISP | 产出文件 CHECKSUM 可复算（正例）且人为篡改数据后 verify 必红（负例）；注册表条款锚改 §6 + 偏差登记 |
| M3-A-003 | 生产 calibrate 在 calibrator.cpp:122 只做 max(flat,0.1) 地板；median 归一在母版生成段完成 | SCI 符号表把 flat_norm 锚到 normalize_flat，而该函数全仓零调用方（实际归一内联在 master_generator.cpp:284-285）；SCI 未指向真实实现 ⇒ 追溯断链（DISP-… | SCI §10/符号表把 flat_norm 的实现锚改为 master_generator.cpp:284-285（或把 normalize_flat 接到生产路径）；删除/标注死函数；不改归一语义 | docs/science/CALIBRATION.md 的 flat_norm 实现锚有可复跑调用点（grep 到调用方）；DISP-CAL-007 关闭 |
| M3-A-007 | CALIBRATION.md:65 与 master_generator.cpp:284-285 均未变；p1cal_tests_core.cpp:271-289 不含该域 | 先除 median 再逐像素夹 0.1：被夹像素>50% 时 median(once)=0.1，二次归一整体 ×10 ⇒ 幂等不变量在缺前提下不成立；SCI §7 未声明该前提，I3 用例不含该域 | 二选一：(a) 把夹逼顺序改为先夹后归一，使不变量无前提成立；(b) 在 SCI §7 显式写前提（夹逼像素占比 <50%）并补边界用例（占比>50% 必红） | 新增边界用例（夹逼像素>50%）下不变量或显式前提断言可红可绿；SCI §7 前提与实现一致 |
| M3-C-012 | 行号 181/566/827 与台账一致；DISP-PHOT-005 已登记（PUBLIC_API.md:456、DATA_SEMANTICS.md:485） | 三份 pc_calibrate_* 全部忽略调用方 mag_max，改用硬编码五级锥；DISP-PHOT-005 只登记在合同/公开 API 表，未覆盖标定依据 | 把 mag_max 接回（或用 IPVSolverParams 同族字段），阈值 12..16 与 n_gaia>=2000 落 GATES_AND_TOLERANCES.md 标定依据；不改算法数值前先登记 | mag_max 入参改变时产物随之改变（行为断言）；阈值表登记且与实现同值 |
| M3-D-004 | 部分修复；p1_session.cpp:438 的 P1-001/PROD-P0-001 流水属待删 phase1_session（DEFERRED） | §flat_norm 不可定位锚已订正；残余为注释中的任务/轮次编号 B13-R13-7（DISP-CAL-010 为合法偏差 ID，可保留） | 删除 master_generator.cpp:224/:232 的 B13-R13-7 历史编号，只留 DISP-CAL-010 与 SCI 条款锚 | grep -n 'B13-R13-7' lib/ 零命中；注释依据只指向 SCI/ALG/DISP |
| M3-D-006 | 部分修复：OpenMP 16 线程错文已删；行号 579→577 | 两处残余：(a) star_matcher.cpp 收敛注释写 scale 而实现比较 location（注释与实现相反）；(b) 18 处 P12-*/阶段N 任务流水注释 | (a) :26 注释改为 'location 变化 < 1e-6'（与 :577-580 一致）；(b) 清 P12-*/阶段号，改为 SCI/ALG 条款锚。不改收敛判据本身 | grep -n 'scale_new - scale_old' star_matcher.cpp 零命中；grep -c 'P12-00\/阶段[0-9]' 同文件 = 0 |
| M3-E-001 | docs/TRACEABILITY.csv 与 docs/traceability/TRACEABILITY_MATRIX.json 并存（后者为生成物），需一并同步 | 校准科学门 TEST-CAL-001 标 VERIFIED，但 test_path 只指测光比例应用测试、implementation_symbols 列无 ac_correct_frame（实测 False）⇒ 追踪断… | 把该行 test_path 指向真正覆盖 ac_correct_frame 的用例（或降级状态），并在 TRACEABILITY_MATRIX 重建后同步；非法 TST-* 前缀统一为既有 ID 体系 | 该行 test_path 中用例实际调用 ac_correct_frame（grep 到符号）；矩阵生成器校验 test_path 可执行且 test_ids 前缀合法 |
| M3-F-002 | 行号 232-274 与台账一致 | oracle 容差 1e-3~3e-3 比 SCI §11/§15 声明的 rtol=1e-6/atol=1e-7 放宽 3~4 个数量级且改为抽样比对，未登记放宽依据 | 按 SCI 声明改回逐像素 rtol=1e-6/atol=1e-7（若 FP32 累加确实达不到，走 SCI 变更流程或登记 DISP 并给误差分析）；不得静默放宽 | oracle 断言与 SCI §11/§15 数值一致；注入 1e-5 相对偏差必红 |
| M3-G-001 | module_adapters.cpp:1551 为活通道；p1_session 属 DEFERRED 待删 | 两个通道都给 ac_correct_frame 传 nullptr 母版（hot/cold 恒 0），manifest 仍报 cosmetic available ⇒ 未执行节点冒充可用 | cosmetic 节点在 master 缺失时 availability 报 unavailable/status 显式降级（并写 hot/cold 计数为 0 的原因码）；或补 master 路径键接通 | 无母版时 manifest availability[cosmetic]!=available（负例断言）；配置面出现 master 路径键 |
| M3b-A-03 | 行号 2470/2506→2475/2516（漂移 5/10）；装配码在 lib/infrastructure/pipeline/orchestrator | dpsf 的 10–90% 截尾平均绝对残差（ADU/像元）被原样写进名为 flux_uncertainty 的积分流量不确定度列，缺 √N_eff 与沿 A/sx/sy 的传播；DISP-PSF-005/PSF §9a… | 合同列名改为 residual_mad（诊断量）或补真实不确定度换算；在 DATA_SEMANTICS 增列语义与量纲；实现按合同二选一，不改拟合数值 | 列值量纲/统计量与列名一致（负例：把 MAD 当 flux 误差注入 → 门必红）；DATA_SEMANTICS 增列定义与单位 |
| M3b-A-04 | 行号 1772/1790 与台账一致，未修复 | σ 取自原图、判决作用于 σ_k=2 平滑图：白噪声被降到 0.14105σ_raw ⇒ 等效阈 ~35σ_smooth，5σ 语义与虚警预算不自洽 | 阈值与判决同域：在平滑图上估 bgnoise（或按核因子折算 5σ→5·0.14105），并在 SCI 写明判决图与噪声域；不改检测流程结构 | 常数噪声场上实测虚警率与 5σ 名义一致（正例/负例各一）；SCI 明确 bgnoise 取自判决图 |
| M3b-A-05 | 行号 411 与台账一致；docs/science 只读 | 候选集非 Moffat4 对称群（π−θ 为镜像，π/2−θ 须同换 sx/sy），消歧不换轴且 fwhm 先算 ⇒ SCI 不变量恒真、不可证伪 | 候选生成按对称群作用同时变换 (sx,sy) 与 θ；消歧后再算 fwhm；补 mutation 用例（人为轴交换必红） | 注入 sx/sy 交换的负例必红；SCI §7 不变量与候选集群作用一致 |
| M3b-A-06 | 行号 59/1879 与台账一致 | SCI 无最小源间距/密度有效域，实现含 x+=r 跳步与 0.2R 去重、拟合窗无邻星掩膜；「块状共享」与逐星独立拟合方向相反 ⇒ 有效域未入合同 | 在 SCI/ALG 写最小源间距、邻星掩膜半径与去重规则的有效域条款；实现按条款加拟合窗邻星掩膜或声明近似误差 | 密集星场（间距<拟合窗）用例下质心偏差有门且可见；合同写明最小间距前提 |
| M3b-A-07 | 行号 1548/1608/2348 与台账一致；台账已更正原判「A_fit 已消失」为「两式并存」 | mag 定义链不唯一：A_fit（拟合幅值，饱和支路）与 box_sum（孔径积分，主路径）两式并存，零点/误差未定义，SCI/ALG/DATA 三方不一致 | 冻结唯一 mag 定义（建议 box_sum 主路径 + 明确 A_fit 支路的适用与换算），在 SCI/ALG/DATA_SEMANTICS 同源登记零点与误差；不改排序实现 | 同一星两路径 mag 差有明确定义与容差门；grep 三处文档 mag 公式同源 |
| M3b-C-03 | 行号 703/1979/1535 与台账一致；DATA 去重阈 :720→:731（漂移 11） | 三处不一致：输出中心取拟合外推（SCI 说饱和平台中心=edge-walking 几何中心）；同一几何中心两式（浮点 vs 整型除法）；去重阈 DATA d²≤1.0 vs SCI d²<4.0 | 统一几何中心计算（浮点式），输出按合同取正确中心；DATA §17.3 去重阈与 SCI 对齐（4.0）或 SCI 订正；拟合失败候选写状态而非静默 continue | 饱和星中心断言按合同口径；去重边界用例（d²=1.0/4.0）可红可绿；grep 两条几何中心式合一 |
| M3b-D-01 | 部分修复：sdet_api.cpp 内两处残句已删；残余在公共头与 psf 头 | 公共头 SDetParams 无单位/值域契约（C ABI 版本化契约面）；dpsf 头含 B4-14 任务号与 v1.1 残断句；原报的『修复: RMSE 系数』『FWHM 上限完全』残句已删 | 给 SDetParams 每字段补单位/值域注释；删除 B4-14 任务号与 v1.1 残断句，改为 SCI/ALG 条款锚 | star_detector.h 9 字段均有单位/值域；grep -n 'B4-14\/v1.1 新增,' lib/ 零命中 |
| M3b-F-01 | ci/** 属 CI-001 文件域；本批只读复核 | 生产路径 0.3px 质心门在机器面上无覆盖：脚本零 CI 登记 + evidence 生产门=false；脚本内 ROOT 仍硬编码 F:\Astro dev 路径跨节点不可移植 | 把 gate2 脚本接入 ci/checks.json（可红可绿），去掉跨节点硬编码根，并在 PSF-001 收口后转绿；不作为发布门前保持 blocker 记录 | CHK 注册项存在且本地可复跑（rc 可红可绿）；evidence 生产门转 true 有对应提交 |
| M3b-F-03 | 部分修复（注释）；补样本可能转红，需独立任务 | 注释面已修，但冻结召回域 SNR∈[10,32.5) 仍零样本、质心门用 0.5px（非 SCI 0.3px）且断言包在空真守卫内、F1 统计只计被 1.5px 匹配器接受的星 | 补 SNR≥10 谱段样本（或按 SCI 变更流程收窄冻结域）；断言改 0.3px 并去条件守卫；漏检进分母 | 冻结域内样本数>0 且召回/质心门可红可绿；空输入用例显式失败而非 PASS |
| M3b-H-02 | 行号 2493→2504（漂移 11） | 迭代耗尽（status=3）仍被视为有效 PSF，且步后钳位值被回填；测光侧只取 status=0，两处口径相反；fvec=1e10/λ 无界仍在 | psf_valid 仅接受 status=0（或显式把 3 标为降级并写 validity 位）；钳位点在拟合内以边界约束实现而非步后回填；补迭代耗尽负例 | 迭代耗尽样本下 psf_valid=false（负例必红）；validity 位随残差超门置位 |
| M3b-H-03 | 行号 1834/2341/2533 与台账一致 | FP64 入口走同一 impl<double>，但 dynrange/minsatlevel/satrange 仍按 uint16 满值 65535 求，mag 分支把 B 降为 float32 并用 log10f ⇒ … | FP64 路径的 uint16 归一化按数据类型选满值（或显式声明该标定仅适用 u16），mag 计算保持 double；头注释与实现对齐 | FP64 图像下 dynrange/mag 分支无双精度降级（ASSERT/负例）；头注释与实现一致 |
| M6a-E-002 | 计数 39+1 与台账一致，两文件仍不在仓内 | 40 处注释以仓外不存在的上游 C 文件行为作为公式/行为依据，不满足「非显然决定须可核出处」（悬空引用） | 把公式出处下沉 docs/algorithms/STAR_DETECTION_ALGORITHMS.md 条款锚（或显式外部化并给版本/URL）；删除裸文件名行号锚 | grep -c 'star_finder.c:\/PSF.c:' lib/algorithms/star_detection/src/sdet_api.cpp = 0；每条非显然决定有仓内可解析锚 |
| M7-A-116 | 行号 38/50/102 与台账 :38/:50 一致 | 该分支令 flat_norm 回落到 ADU 量纲（与 §3 量纲表『无量纲 median=1』断链），同为平场不可用却与 NULL 跳过分流且无错误码 ⇒ cal 被静默缩放 | median<=0 与 NULL 同走显式错误/状态位（或返回未校准标记），并补 §3 量纲断链说明；不改 floor 数值 | median<=0 平场用例下输出状态非『正常校准』（负例必红）；§3/§8 量纲与错误码一致 |
| M7-A-118 | 行号 20/51/63 与台账一致；需科学裁决 | 解析值 1.2303076 与冻结常数 1.230310 在第 6 位分歧：§7『恒为 1.230310』/§10 禁改/§11『解析一致性 max_abs==0』三语句不可同真 | 二选一：把冻结常数改为解析值 1.2303076526（走 SCI 变更流程，实现同步 15 位常数），或把 §11 的 max_abs==0 改为按冻结常数舍入后的容差式；不得两存 | 常数与 Oracle 同一数值、同一个容差式；注入 1e-6 相对偏差必红 |
| M7-A-128 | 行号 257→266（漂移 9） | 判决域用未平滑原图 5·σ_raw，平滑核把白噪声降为 0.14105σ_raw ⇒ 阈等效 35.45σ_smooth；SNR=10 源在判决图峰值上界 10σ_smooth，门数学不可达且无在册用例 | 在 ALG 写明判决图与噪声域（或把 5σ 折算到平滑域），并补 SNR≥10 可达性用例；不改门值前先做可达性证明 | SNR=10 合成源在冻结域内可被检出（正例）；门式与判决域/噪声域同域 |
| M7-A-130 | 行号 75/151 与台账一致 | 量纲错式：FWHM 随 sy 增大而减小、随 sx 增大而增大，与同篇 F2（各向同性 FWHM=1.230310σ）及 SCI-PSF 解析比值不可同真 | 把 :75/:151 改为 FWHM_x=1.230310·sx、FWHM_y=1.230310·sy（或按几何平均 sqrt(sx·sy)），与 SCI-PSF 解析式一致；实现不改 | ALG 内 FWHM 式各向同性退化为 1.230310σ；grep -n 'sx/sy' docs/algorithms/STAR_PSF_ALGORITHMS.md = 0 |
| M7-A-131 | 原『判为良性』表述已不在树；DATA_SEMANTICS:201 未变 | MAD=0 ⇒ 阈≡med 的退化分类仍在冻结式内，无『不可判』降级与计数暴露；欠曝/常量母版下必然发生 | 在 ALG/合同增加 mad=0 的显式分支（判不可判/跳过并计数暴露），或声明该退化下 hot/cold 全帧分类无效；补边界用例 | 常量母版用例下检测计数与降级状态有断言（可红可绿）；合同写明该退化语义 |
| M7-A-134 | 行号 149/47 与台账一致；读码证据 cosmetic_corrector.cpp:145 | 两篇 ALG 对同一 detect_cold_pixels 给互斥符号描述；读码确认 COSMETIC 侧为真，CALIB §F4.1 冷分支错接热块统计量 | CALIBRATION_ALGORITHMS.md §F4.1 冷分支改为 med_b/σ_b（bias 独立统计量），与 COSMETIC 及实现一致；不改代码 | grep -n 'cold = (bias < med − cold_sigma·σ)' CALIBRATION_ALGORITHMS.md = 0；两篇 ALG 同式 |
| M7-A-135 | 行号 93/148→95/150（漂移 2） | 文档 §3/§6 仍描述 x+=5 跳步，实现已无任何 stride；判据锚 :1715-1734 已失效 ⇒ 文档与实现三方不一致 | 按现实现重写 §3/§6（无跳步、全像素扫描 + local_max 判决），删除失效 :1715-1734 锚；不改实现 | grep -n 'x+=5' docs/algorithms/STAR_DETECTION_ALGORITHMS.md = 0；文档流程与 sdet_api.cpp 扫描循环逐行对应 |
| M7-A-138 | 35 行/1 命中与台账一致 | ALG-HEALPIX-* 域无判据落点：全篇只给 tile_shift=9/mask=(1<<18)-1/nested_local_to_xy 三条不变量，缺闭式与无效值语义 | 补 ang2pix/nest2ang 闭式（含 NESTED/RING 区别）、单位/坐标系与无效输入语义（-1/异常返回）并给独立 Oracle 锚 | 文档含闭式与边界语义；新增 Oracle 用例（极点/RA 跨 0/非法 nside）可红可绿 |
| M7-A-141 | 行号 65-69 与台账一致；1.15 在实码中已 0 命中（同族 M2a-D-1 已收敛） | 三系数（1.25/1.15/3.0）关系未论证：1.25 缺 sup(外接半径) 推导，3.0 保守查询圆与 1.15 快速路径畸变系数的读法未声明，任一改动无安全边界可依 | 在 DRIZZLE_GEOMETRY.md §buffer 补 1.25 的外接半径上界推导（sup）与 3.0/1.15 的来源与相互关系；给零漏选 mutation 门锚 | 文档给出 1.25≥sup 的推导；改动任一系数时 candidate_oracle_test 必红（mutation） |
| M7-A-201 | 行号 65/75 与台账一致；仅表述缺陷 | 把半径 1.25 与全 φ-对角 1.532 并列作『覆盖』比较，按字面 1.25≥1.532 为假命题且未声明 1.532 的量类（数值行为不改，1.25 实测够用） | 改写该句：明确 1.532 是平面像素对角线与球面半径不同量类（或给出 1.25 半径对 1.532 对角的实际覆盖论证）；不改 HP_CIRCUMRADIUS_FACTOR | DRIZZLE.md 无 1.25≥1.532 的字面蕴含；量类声明与 §buffer 推导一致 |
| M7-A-202 | 行号 85/108/115 与台账一致；旧注册表 :162 CONFORMANT 抄录需同步 | 因子笛卡尔积≥12·4·5·7·2·2·2=13440 且为偶，9003=3×3001 为奇且不被 1680 整除 ⇒『全枚举』与所列因子不可同真 | 二选一：补足/改写因子列表使积=9003（或 9003 为去重后计数并写明去重规则），或把用例数改为真实全枚举值；同步 TEST-DRZ-CAND-001 的门文本 | 文档因子积与用例数一致（脚本复算 rc=0）；门可复跑且计数自洽 |
| M7-A-206 | 行号 98 与台账一致；M7-A-119 面已闭合但本条残余覆盖缺口独立 | §11 门侧仍不含 BUNIT/单位断言，px² 抵消的残余覆盖缺口仍在（flux 单位已在 §4:28/§9a:87 声明为 ADU，但门不校验单位） | 给 §11 解析门加单位/量纲断言（flux 的 ADU 与 px² 抵消路径各一条），或登记该缺口为 DISP 并给证据面；不改公式 | 门含单位断言（注入单位错位必红）；PSF.md §11 门表与 GATES_AND_TOLERANCES 同源 |
| M7-A-207 | §8 表现 6 行；行号 261-270→265-268（漂移 4） | 代码侧已显式拒绝（DISP-CAL-010），但 SCI §8 退化表缺「全 NaN 像素」行、合同未登记该退化路径与计数 ⇒ 文档/合同面缺口仍在 | 在 SCI §8 表补「全 NaN 像素 → AC_ERR_PARAM（含日志/计数）」行，并在 DATA_SEMANTICS 登记；不改代码 | §8 表含全 NaN 行且与实码错误码一致；grep -n '全 NaN' CALIBRATION.md 命中 |
| M7-H-102 | 行号 215/1526 与台账一致；几何面积 NaN 与值面 NaN 为两条 | NAN 哨兵与拒绝判据不匹配：NaN 使 overlap_area<1e-20 与 weight<=0 均为假，NaN 通过两道门进 sumFlux 累加污染整 tile，且无计数暴露（DISP-DRZ-004 只登记值… | 在面积/权重门加 std::isfinite 检查（NaN/Inf 与 <1e-20 同路拒绝并计数暴露）；同步 DISP-DRZ-004 | 人为注入 NaN 面积的用例下 tile 不产出污染值且计数增加（负例必红） |
| M9-A-1 | 行号 244/1770 与台账一致；面积路径已统一 Eriksson，仅阈值未登记 | 路径切换判据仍用 60″ 阈值且无数学依据/DISP 编号/边界用例，ALG 文档对阈值去向零陈述 | 在 DRIZZLE_GEOMETRY.md 登记 60″ 阈值来源与适用域（或给解析依据），补 59″/61″ 边界 mutation 用例；不改阈值前先登记 DISP | 阈值有 ALG 登记行 + 边界用例可红可绿；grep -c 'cos_thresh' DRIZZLE_GEOMETRY.md > 0 |
| M9-B-1 | ipv_wcs.cpp 注释面已修（P1-001 记录），写盘面 :1933/:1948/:3131 未变；行号 361→364 | 残余两处：桥注释引用不存在的 FITS paper IV §2.1（正确出处=SIP, Shupe 2005 §2/§3 或 WCS Paper II）；写盘面仍以标准键承载像素域系数（键语义归科学/合同裁决） | (a) 注释改为 SIP（Shupe 2005）锚；(b) 键语义二选一：写盘前把 A/B 换到中间世界坐标（deg/px^{i+j}）或改键名/加 ASTROCS 私有前缀并在 DATA_SEMANTICS 登记；不改… | grep -c 'FITS paper IV' module_adapters.cpp = 0；DATA_SEMANTICS 对 A_i_j 数组的单位声明唯一 |
| M9-C-3 | 行号 42/287 与台账一致；REPORT.md:335 的 sigma_d_adaptive 公式代码中不存在 | 同名视场量两套口径：polygon/vote 族按角秒、选择器/求解器主链已改度（ipv_types.h:91 fov_diag_deg）；度值若传入 geometric_vote 则 is_wide_fov 永假、r_… | 统一为单一单位（建议 deg）并改 polygon/vote 族签名与阈值（3.0*3600、日志 /3600、r_local），或加单位后缀防混用；补单位一致性单测 | geometric_vote 与选择器同单位（正例）；混用单位注入必红（负例） |
| V10-N-01 | 行号 425-428/530 与台账一致 | 短读只缩 pixels 向量、宽高仍取头部 NAXIS ⇒ 输入面按 y*width+x 索引可越界且函数报成功；输出面另有 pixels.size() 校验但不覆盖输入面 | 短读时 fail-closed（返回 false/错误码）或把 width/height 同步为实际读入量并在调用点校验；补短读用例 | 截断文件输入下 fits_reader 返回失败或尺寸自洽（负例必红）；调用点无越界读 |
| V10-N-02 | 行号 225/379 与台账一致；wcs_sip.cpp 固定 6×6 布局 | 读侧不夹阶数即以外部件数索引 36 槽固定系数池（a[36]）；同族 KV 边已夹 [0,5]（hp_drizzle_api.cpp:437-441 return -10）⇒ 同一字段两种口径 | 读侧对 A/B/AP/BP_ORDER 施加 [0,5] 判定，越界 fail-closed（与 hp_drizzle_api 同口径）；补越界用例 | A_ORDER=9 的 FITS 输入被拒（负例必红）；两侧阶数口径一致 |
| V10-N-03 | 行号 1269/1284 与台账一致 | 同一函数相邻两处：itemSize 用裸 atoi（负值/垃圾值无错误分支），item_size 是解交织步长，进入后续解码 | itemSize 改用 parse_bounded_int 并给合法域（>0 且 ≤ 上限），越界显式错误返回 | 非法 itemSize 输入下解析失败并给出错误码（负例必红）；无未校验 atoi |
| V10-N-06 | 行号 315/419-420→315/422 与台账一致 | 仅判正数、无上限即把头部尺寸直乘成分配量 ⇒ 恶意/损坏头可触无界内存增长；同族 aio_fits.cpp:554/aio_xisf.cpp:33 已有 65535 上限口径 | 读侧加维度上限（与 aio_fits parse_fits_header 的 65535 同口径）并 fail-closed；补超限头用例 | NAXIS>65535 的输入被拒（负例必红）；三处 FITS 读面口径一致 |
| V12-N-05 | 行号 333/336/437 与台账一致；API_CONTRACTS.csv 该行末列 VERIFIED 属非链上 | 同一式两份字面实现（权威函数零调用者 + 内联副本）；m_lim_m0_offset=-4 ⇒ 两路径同输入差 4.000 mag；13.0 仍非 IPVSolverParams 字段 | 删除权威函数或把内联副本改为调用同一函数（单一实现）；13.0 上界落 params/文档登记；补两路径一致性断言 | grep 调用点=1 且无第二份字面式；两路径同输入同输出（正例） |
| V12-N-07 | 行号 424-429/244-262 与台账一致；ipv_types.h:211 自记 m_lim_* 配置不可达 | 五对「兜底字面量 vs 结构默认值」各写一遍且互不引用，±6.0 无 params 字段/文档登记/static_assert ⇒ 默认值多源 | 兜底路径改为直接引用 params 默认值（或 static_assert 同值），±6.0 提升为 params 字段并在 ALG 登记；不改数值 | 每对同值有 static_assert 或单一定义点；grep 无第二份字面量 |
| V12-N-13 | 行号 317-318/232-233 未漂移 | 三套口径互斥：代码夹进 [50,60]（实际取值域 20% 带宽，60/50 皆裸）/注释写『统一为 60』/结构默认 20 且注释写 20→40→60；原文档锚 PLATESOLVE.md:115 已不含该句 | 把 50/60 提升为 params 字段并在 ALG 登记，结构默认值与注释同源；补取值域断言 | 默认值/注释/调用点三者同值且可复算；grep 无裸 50/60 阈值 |
| V2-N-03 | 行号 111→113（漂移 2）；原 finding 的第二子锚（README:132）确已不再含该等式 | ALG 文档把 RMSE 与 mad·1.4826/A 画等号（RMSE≠1.4826·MAD/A 定义），并写判据 RMSE/A≤0.2；实码按 σ_res=MAD·1.4826 判、result->mad 存未换算 … | ALG 文档把该量改名为 σ_res/A（或 residual_scale_ratio）并给定义；与 DATA_SEMANTICS 的 mad 语义对齐；不改门值 | 文档公式定义=实码变量定义（σ_res=MAD·1.4826）；grep -n 'RMSE=mad' doc = 0 |
| V2-N-04 | 行号 1112/2979 见台账；本批实测 orchestrator:2989（漂移 10） | 库层门正确（size<3→NO_DATA/scale=1.0/return {}），但 pc_api 在 NO_DATA 后仍 return 0、orchestrator 无条件写 STATUS=OK ⇒ 交付头把未定标… | pc_api 把 NO_DATA 透传为非 0/显式状态；orchestrator 按实际 fit_used/scale 写 STATUS（未定标写 SKIPPED/FAILED） | 未定标帧交付头 STATUS≠OK（负例必红）；三处状态链一致 |
| V2-N-05 | 行号 227/450 与台账一致；与 M3b-H-01 同址不同判据 | 对称/量化残差下 MAD 可精确为 0 ⇒ 门 1.4826·MAD/A>0.2 恒不触发（fail-open、无下界判别），SCI/ALG 未描述该退化 | 给该门加 MAD/A 下界判别（MAD=0 时按残差为 0 的正常态或显式『不可判』分支），并在 SCI/ALG 写退化条件；补对称残差用例 | MAD=0 且 A>0 的用例断言明确（可红可绿）；退化条件入文档 |
| V20-N-01 | 行号 313/456 与台账一致；文件在 lib/algorithms/drizzle（活） | 四个反向几何配置键解析后整仓零读取（声明+写入即止）⇒ 用户改这些配置不改产物；反向几何实取输入 manifest 的 crval/crpix/cd | 二选一：把 rev_* 接到 op=reverse 路径（或在 manifest 回显生效值），或删除该死配置键并在文档注明 | 改 rev_* 配置后产物/回显随之改变（行为断言）；或键从合同面消失且无解析代码 |
| V21-N-11 | 路径 lib/astro_image_io→lib/infrastructure/aio；本行 解析后目标 为空（批内未给），按符号重锚 | 自述『诊断用』字段仍只写不读：生产面 3 处=2 写+1 声明 ⇒ 诊断量无消费者（计划值/诊断位不落交付面） | 二选一：把 n_contrib 接入诊断输出/manifest（消费者+测试），或删除该字段并在格式面注明 | n_contrib 有读取点并落交付面（断言）；或字段移除且格式兼容性说明 |
| V3-N-02 | 行号 862-864 与台账一致 | 第二出口把精度/数据不匹配（-14）归入科学前置域；行锚由 :862-866 漂到 :862-864，逻辑未变 | 把 -14（精度/数据不匹配）加入 DATA 或 CONFIG 分支，与库边界 hp_drizzle_api 的 -14 语义一致 | -14 输入的错误域=DATA/CONFIG（断言），与 hp_drizzle_api.cpp:1010 口径一致 |
| V5-N-04 | 测试面 :323 已补；本行 P2，随 gaia/normalize 科学同源整改 | 谓词面已可测（prune_legal+剪枝/不剪 bitwise 一致断言），残余为文档面：GAIA_QUERY.md 无『声明下界+0.25 裕量』登记行、0.25 权威出处不入库（与同文件 bbox 1.2 裕量有登… | 在 docs/algorithms/GAIA_QUERY.md 补 0.25 星等剪枝裕量的登记行并把出处（设计/推导）落到 docs/algorithms；不改实现 | GAIA_QUERY.md 含该阈值登记行且与实现同值；出处可在仓内解析 |
| V6-N-03 | 行号 258 与台账一致；PC_QF 位定义在 star_matcher.h:10-16 | star_matcher.h:6-7 把宿主指向无该符号的 photometric_calib.h（真宿主为 star_matcher.h 自身与 snr_estimator.h:412）；测试只断言两行宏值 ⇒ 悬空锚… | 改注释宿主为 star_matcher.h/snr_estimator.h；把测试改为断言 6 个导出实际带 quality 位（或删除该测试声明） | 注释锚可解析（grep 到符号宿主）；测试断言与声明一致（正负例） |
| V7-N-01 | 行号 1140-1158/1524-1535→1530/1539/1543 漂移 ±6 | IR 侧 p1_flag/p1_num 对错型仍静默取默认（enabled:"false"→dflt=true 开关反向；hot_sigma 非数字→5.0；1e999→inf 过 hot_sigma>0 门 ⇒ 整帧零… | IR 侧 p1_flag/p1_num 增加类型门（错误显式报错，与 DLL 侧同口径）；hot_sigma 加 isfinite/正域校验；补错型配置用例 | enabled:"false"/hot_sigma:"x"/1e999 三类输入均显式失败（负例必红）；两通道值域一致 |
| V7-N-02 | 行号 1880/1899→1887/1906（漂移 7）；阶数门 [0,5] 已在位（与 V10-N-02 同一门的另一侧） | config.wcs.sip={order:2,ap_order:0} 不带数组时仍 return true（允许缺省全零）、present 置 true 而 36 系数全 0 ⇒ 判别位记声明值不记生效值；sip 键非… | present 仅在数组实际提供且非全零时置 true（否则置 false 并显式说明）；sip 键类型错显式报错；补两分支用例 | 缺数组/全零时 present=false 且 CTYPE 去 -SIP（断言）；类型错必红 |
| V7-N-04 | 行号 428/433 与台账一致 | 只检 CD1_1/CD2_2 两角元 ⇒ 反对角或纯旋转 CD（对角为 0、非对角有值）被判『无 CD』并静默改用 CDELT·CROTA2 重建，交付天区几何可整体反转/转置且 rc=0 | 判据改为 CD 矩阵四元任一非零（或 det≠0）；对 CD/CDELT/SIP 读入加 isfinite 门；补纯旋转 CD 用例 | 纯旋转 CD 输入下使用 CD 而非 CDELT 重建（正例）+ 非有限 CD 必拒（负例） |
| V7-N-06 | 行号 1391/1494-1495→1398/1501（漂移 7） | 同一 per_frame.dark_scale 承载『固定 K』与『实测优化 K』两种含义且无来源标记 ⇒ 交付值 1.0 无法区分『应用了 K=1.0』与『暗优化未执行』 | manifest 增写 dark_optimization 开关与 k_source（fixed/optimized），或拆成两个字段；补区分断言 | K=1.0 两种来源在清单可区分（断言）；字段语义唯一 |
| V7-N-07 | 行号 2966→2999（漂移 33） | 布尔意图字段用整型助手解析 ⇒ 字符串 "0"/"false" 落缺省 1=NESTED，交付索引语义被反转且 rc=0；同文件别处已建类型门，纪律不一致 | nested 改用布尔/严格整型解析（类型不符显式报错），并加 HEALPix 索引语义断言；不改默认值语义 | nested:"false" 输入显式失败或正确解析为 0（负例必红）；同文件类型门口径一致 |
| V7-N-10 | IR 节点 module_adapters.cpp:2967 仍取 1.0，需同步 | 注释以 API-DRZ-001 作 pixfrac 缺键默认 1.0 的依据，但该节内 pixfrac 只出现在签名与返回码/双轨说明，无默认条款 ⇒ 杜撰锚仍在（权威语义只定 (0,1] 与拒绝） | 二选一：在 PUBLIC_API.md/SCI-DRIZZLE 增补 pixfrac 缺省 1.0 的正式条款，或删除该默认（缺键 fail-closed）并把注释锚改为真实条款 | 注释锚可解析到写明默认值的条款；缺键行为与条款一致（正负例） |
| V8-N-08 | 行号 34/38/59/70 与台账一致；:34『64 次（≥11610°）』与 :24『有限值逐位不变』复算为真 | 区间端点应为闭区间 [-90, 90]（-90 可达却被头注释排除）；头注释左开与测试闭区间两处口径不一 | 头注释改为 [-90, 90]（与实现/测试一致）；不改归一化逻辑 | grep -n '(-90, 90]' sdet_angle_guard.h = 0；注释/实现/测试三处端点一致 |
| W1-N-01 | 原判据机制描述（每文件顺序截断）已被现树否证，但判据本身仍不成立；行号 2099/2124 与 :487-488 见台账 | n_ret 是跨文件求和（gaia_cone_search 聚合 count），却与每文件上限同型比较 ⇒ 多文件各未触顶时仍判 capped 并 break（converged=false，丢更紧收敛解）；公开头 :2… | 触顶判据改为逐文件判定（或 cap_unit×nfiles 聚合口径），公开头注释与实现统一；补多文件用例 | 多文件各未触顶时 converged 不被误置 false（负例必红）；头注释与实现同口径 |
| W1-N-08 | 证据 TSV 在 run/（gitignored，仓内不可复核），本轮按盘上文件复算 | 注释把 DR3-only 口径（high 下界 16.59）写成 36 片合并包络（实为 13.62）；结论『[-10,40] 完全包含包络』不受影响故 P2 | 注释改为 high ∈ [13.62, 25.59]（并注明 16.59 为 DR3-only 下界）；出处 TSV 在 gitignored run/，需把结论落 docs/algorithms 或 tests 证据 | 注释数值与复算一致；出处可在仓内解析（或登记为运行产物引用） |
| W1-N-09 | 仓内 writer 恒带引号，按静态口径判『行为≠注释』而非生产可达 | 只有空引号形走告警路径；未加引号形 magnitudeRange=10,20 被当属性缺失静默处理（不告警、不计 reject_count）⇒ 行为不等于注释 | 按注释实现：区分『属性缺失』与『存在但未加引号/畸形』两种返回（或改注释承认未加引号=缺失）；未加引号形至少计 reject_count | 未加引号 magnitudeRange 用例产生告警/计数（负例必红）；注释与行为一致 |
| W1-N-11 | 行号 2962/3150/3170/3176→2989/2991/3172 漂移 | finest<0.0503″/px 时引擎钳位 2^22 使 hp_res 粗于 finest，auto 运行也进欠采样分支并打出 explicit 归因（nside_source=explicit）；建议值 auto_… | 欠采样归因按 nside 来源分流：auto 运行写 nside_source=auto（钳位单独记 clamp 位），告警串不再写 explicit；数据面 p1_stack.json 已如实 | auto+钳位场景下 nside_source=auto 且告警含 clamp（断言）；建议值≠实际采用值时给冲突标记 |
| W2-N-08 | 台账外部指令『python3 读 testdata/NGC55…』不可复跑（P1-001 已登记），本轮按源码口径判定 | 旋转下 abs(CD1_1) 不是像素角尺度；同仓另两处定义（orchestrator 0.5·(‖col1‖+‖col2‖)·3600、aio_wcs_pixel_scale √abs(det)·3600）仅 PA=0… | 统一用 √abs(det(CD))·3600（或与另两处同式），三处同源并补旋转场用例（PA≠0 时三式一致） | PA=0/90/45 场下三处像素尺度一致（rtol 1e-6）；K_CORR_DOMAIN 选档正确 |
| W2-N-12 | 行号 35-36/29 与台账一致；实现属 wrapper_phase1（活） | WcsTan 交付 RA 值域 [−180,180) 与合同 [0,360) 互斥；交叉参考门用同一 wrap 且 p1_angular_sep_deg wrap-safe ⇒ 门在数值上恒不可见 | wcs_tan.cpp 归一化改为 [0,360)（ra_out<0 时 +2π）；补 CRVAL1=210° 帧中心用例断言 RA∈[0,360) | 帧中心 RA=210° 类用例交付 +210.00033（正例）；RA<0 输出 0 命中 |
| W2-N-15 | python3 复算 211076.28514206142（相对 210960 偏 −0.0551%） | 『独立参考实现』硬写本仓明令禁止的魔数 210960（相对 −0.0551%），且测试私有上限 2^20 与生产 2^22 不同制；断言容差 hp_res<=finest*1.01 恰好吸收该偏置 ⇒ 差异永不可见 | 测试改用与生产同源的解析式 sqrt(π/3)·(180/π)·3600（不调用生产 symbol），上限与生产同制，容差收紧到 1e-3 并补边界 | 测试独立复算值与生产同式同值（rtol 1e-6）；注入 210960 魔数必红 |
| W5-N-14 | 两处锚 :1149/:2275 未漂移；drizzle 属 normalize 产品面 | GAIN 解析失败仍静默保留结构体初值并写出『看似产品事实、实为初值』的头值 ⇒ 交付头假值 | 解析失败置未设置位/写 NaN 并记日志+计数（或 fail-closed）；头写出只在解析成功时写 GAIN；补非法 GAIN 用例 | GAIN="abc" 输入下头不写 GAIN 且计数/告警可见（负例必红） |

### CI-001（79 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| FD-F-003 | 台账证据路径已失效，本轮改以执行器源码取证（行号漂移 :103-106 → :78/:356） | win 测试阶段只落 JUnit 不解析用例数，tests=0 仍可判 PASS；台账的 junit 样本已随旧世代证据树删除 | 解析 JUnit testcase 计数并断言 tests>0；junit 与 CI_RESULT 的 pass/fail 数量互校 | 注入空 JUnit（0 testcase）⇒ WIN-TEST-UNIT 判红 |
| M5a-G-009 | 文件仍 172 行，与台账同构 | provider 侧断言以「库存在」为前提，缺参静默跳过；交付二进制从未被看，真实判定模式无注册 step | check() 对 --avx2-lib/--avx512-lib 缺失判红；新增真形态 step（--binary/--avx2-lib）并保留 --selftest 作负例面 | 缺 provider 库或 --binary 缺失时 ISA-LEAK 判红（可注入负例） |
| M5b-C-01 | 数字与台账一致（382/356）；tests/ 与 docs/traceability/ 零命中本轮复现 | 356 行共用占位测试 ID，该 ID 在任何测试面零定义，status=VERIFIED 指向未定义 ID；注册检查器 required 不含测试 ID 列 | 为 TST-GEN-001 建真实宿主或逐行改写测试 ID；check_api_contracts 增 test id 列并校验 tests/ 命中 | 注入未定义 TST ID ⇒ API-DOCS 判红 |
| M5b-F-01 | def 数由台账 379 漂到 383（同期新增） | CI 控制面（注册表/waiver/绑定/负例守卫）仅 2/21 文件进任何 profile，test_ci001_failclosed.py 等不执行 | 扩 pattern 至 test_*.py 或按文件多 step 登记；保留 --self-test 负例 | CI 控制面测试全量采集；discover 采集 0 用例时判红（R11） |
| M5b-G-02 | TRACEABILITY-CODE（tools/check_traceability.py）已按 GAP-032 于 docs/ci/01_CHECKS.md §2.1 退役（147→146），本行残留对象仅为 tool… | TRACEABILITY 门末行无条件 return 0（waivable=false 却不可能红），且仍写受跟踪 reports/ 路径；TRACEABILITY-MATRIX 注册命令无 --strict | 退出码按 errors 决定；输出改 run/；把 --strict 纳入 TRACEABILITY-MATRIX 注册命令 | 注入断链 ⇒ TRACEABILITY 判红 |
| M5b-G-03 | 文件 71 行与台账同构 | 判据是同头文件正则声明名 ⊆ 同头文件 AST 名集（恒真），nparams 提取后未参与比较，零引用 API 文档 | 改为 AST ↔ docs/contracts 公共 API 文档双向比对，nparams 参与断言 | 注入文档与 AST 参数个数不一致 ⇒ CHK-ABI 下 AST-API 判红 |
| M5b-G-10 | 台账三条腿中两条已修，本行仅因 timeout 腿判 OPEN；同一文件在 M5a-G-008/W6-N-08 的主缺陷已判 FIXED | 残留「外部命令不带 timeout」：nm 子进程无超时，挂起/无响应无法判红；另两条腿（缺二进制静默 PASS、源码面只 glob lib/phase2/src）已按 GAP-027 与 ARCH-001 修好 | binary_symbols 的 subprocess.run 增 timeout 参数并纳入 fail-closed（超时即 errors） | nm 挂起或超时 ⇒ 判红而非挂死（可注入桩复核） |
| M5b-G-11 | 台账「零 --gate-required」一腿已由 CI-003 修复（RESOURCE-GATE-REAL 注册 P0） | 静态硬编码扫描面未覆盖 providers/**（ISA provider 侧 workers=1 不可见）；重计算分类仍只有 heavy→monitor 反向规则 | PROD_FILES 增补 providers/（或复用 check_thread_budget 的 SCAN_ROOTS）；R7 增补正向要求 | 在 providers 注入 workers=1 ⇒ SERIAL-HARDCODE 判红 |
| M5b-G-12 | 文件 189 行；台账锚 58-60 漂到 58-59 | 根 CMake 已含 -fopenmp ⇒ 四条件 AND 恒假（结构式 fail-open）；ALLOWLIST 定义后从不被 scan 引用 | 逐 cpu_heavy 节点核对实现与编译 target，删除恒假 AND，启用 ALLOWLIST | 注入串行 single-thread 实现 ⇒ NO-SERIAL-HEAVY 判红 |
| M5b-G-13 | 机械信号签名桶 some(0.4) 与台账一致 | 模块化 ABI 头族与 provider 头不在扫描范围（HEADERS 收集后未进 :51 循环），struct_size 判据是死分支，PASS 文案虚报覆盖头数 | 扫描 include/astrocs/abi 全族与各模块 ABI 头；struct_size 按结构体块判定 | 移除某结构体 struct_size ⇒ ABI-BOUNDARY 判红 |
| M5b-G-14 | 行号 585-589 → 587 附近 | 唯一导出符号门只判「符号表非空」且仅 Windows hosted 面；5 个科学模块 DLL/SO 与 Linux 面零覆盖 | 断言导出名集合（acs_artifact_* 等）而非非空；Linux 侧加 readelf 等价断言 | 导出名缺失/多余 ⇒ CHK-ABI 判红 |
| M5b-I-05 | 台账 145 项时点 → 现 42 聚合项（方向一致） | CI 元文档规模计数与失效触发前缀未随注册表换代回写 | INVENTORY_REPORT 改机器生成或标注时点；impact_map 前缀改 packaging/launch | CHK-REGISTRY-DOC-SYNC 扩到 INVENTORY_REPORT 计数与 impact_map 触发前缀存在性 |
| M6a-G-001 | STALE_PATTERNS 仍只有 :9-10 定义行（零引用） | 判定仍只剩 V19R2/V19R3 两字面量且被「冻结」抑制；扫描面缺 .c/cli/runtime/include，coverage 恒 1.0 | 用 STALE_PATTERNS 全表 + 行级豁免；扫描面补齐；coverage 用真实分母 | 注入 V20R1 陈旧轮次注释 ⇒ CHK-STALE-DOC 下 CON-COMMENTS 判红 |
| M6b-E-002 | 行号漂移，原文逐字在位 | 锚门作用域仅 science+algorithms 两 glob，docs/architecture 与 docs/owner 的漂移锚无判据；绑定 41 条为整条通过口径 | doc_globs 扩到 docs/**（或按目录分别登记）；锚失效按目录分桶逐条可红 | 在 docs/architecture 注入失锚 ⇒ DOC-LINE-ANCHORS 判红 |
| M6b-E-004 | 台账 grep -m1 取样与全量引用并存（远多于 1 处） | 真源注释把已删除控制包文档当权威依据，无悬空引用门覆盖代码注释面 | 改指现行 docs/science 或标注历史溯源；CHK-DANGLING 增「注释中的规范路径须存在」判据 | 注入悬空规范路径注释 ⇒ CHK-DANGLING 判红 |
| M6b-E-005 | 唯一性互斥半订正仍在 | 规范引根级 schemas/ 悬空、8 必填层与层表 9 行不一致、PASS 契约字段 rows= 未实现 | 路径改 contracts/schemas/；层数口径统一；checker PASS 行补 rows= | checker PASS 输出与 SPEC:159 逐字一致 |
| M6b-G-001 | evidence_VERIFIED 由台账 6 增至 7（缺口扩大） | C7 对 MOD/SRC/TEST 直接 continue，schema 无 evidence_path 列，7 条 EVIDENCE=VERIFIED 无路径 | schema 增 evidence_path 并做存在性/采集面校验；C7 覆盖 EVID | EVIDENCE=VERIFIED 无路径 ⇒ TRACEABILITY-MATRIX 判红 |
| M6b-G-004 | 台账 DEFAULT_TABLES 腿随 TRACEABILITY-CODE 退役消失（tools/check_traceability.py 已无默认输入常量） | 追溯门不可红 + changed_paths 指向不存在面 + 伞形豁免 | 退出码按 errors；changed_paths 改真实路径并覆盖 lib/cli/tests；删伞形豁免 | 改 lib/ 下追溯相关文件能触发 TRACEABILITY-MATRIX 复跑；注入断链判红 |
| M6b-G-005 | 「悬空引用门零实现」一腿已由 CHK-DANGLING（CON-DOC-SYMBOLS/DOC-INDEX，P1）承接 | module_index 真值比对未实现；条款/引用有效性只跑自检形态 | 实现 module_index 比对并注册真形态（--ir/--module-index/--trace） | 注入模块注册表不一致 ⇒ PRODUCTION-GRAPH 判红 |
| M6b-G-006 | 台账 305/190/50 为旧时点，现 333/222/169 | 内容级门作用域未覆盖 docs/owner、docs/standards、docs/traceability 等，索引自述未限定作用域 | 明确索引覆盖与门覆盖差集登记，或把 STALE-DOC/DANGLING 面扩到全部 active 条目 | 在 docs/owner 注入陈旧版本号 ⇒ 有门判红 |
| M6b-I-003 | 台账 5 项中 family.json 与 CHANGELOG.md 两对象已不存在（CHANGELOG 已删） | 活动文档统计字面量无机器门校验，长期漂移 | 统计值改生成器注入或新增「文档统计值 vs 实测值」门（CHK-STALE-DOC 扩展） | 注入 76 vs 63 不符 ⇒ CHK-STALE-DOC 判红 |
| M7-G-104 | 归集条性质，本轮以实例 1 取证 | Oracle 源在盘但不在任何 CTest 执行面，真值文件缺失 | 注册 oracle 测试目标并补真值生成器 | 目标进 ctest -N 且能红能绿 |
| M8-F-004 | 台账「判定器在 ci/ 内零注册 rc=1」一腿已修（STD-REG 已注册） | C4 证据判据只验 path_exists，无「证据须被 ci/checks.json/ctest_baseline/discover 采集面消费」规则 ⇒ 仍可以未执行测试源作 EVIDENCE | C4 增证据登记面校验（EVIDENCE 须被注册表或采集面消费） | 把 EVIDENCE 指向未注册测试源 ⇒ STD-REG 判红 |
| M8-F-005 | 台账「28/29 drizzle cpp 零命中」本轮未逐文件复算，仅复核契约面无该规则 | 注册门是「已注册目标是否登记」的子集判定，存在但从未注册的测试源永久隐形 | 增加「磁盘 TU 源 → 构建目标」反向闭包，或按 basename 在 CMake/CI 配置面强制命中 | 新增未注册测试源 ⇒ CHK-MODULE-MANIFEST 判红 |
| M8-F-007 | p1_hips_writer_test 行号由 89 漂到 90 | 逻辑或 true 使断言恒真；恒真探针覆盖单文件单字面串 | 去除逻辑或 true 并给出真实期望；探针改为全仓恒真模式扫描 | 注入恒真 CHECK ⇒ 已知失败基线自检判红 |
| M8-F-008 | 台账名单与现树一一对应 | 组名不匹配即循环零执行 total_fail=0，仍打印 TESTS PASS 且 rc=0 | 全部主头补 unknown group fail-fast（回灌 p1star 写法） | 传错组名 ⇒ 对应测试 rc!=0 |
| M8-F-009 | cpu_monitor_test:74 期望 1.7=0.85×2 已整改（子事实①） | 子事实②自证式阈值输入仍在；子事实③「把零 --gate-required 钉成期望」已按 GATE-FIX-RES 反转修好 | p2_seam_gate_test 阈值改用独立 oracle 常量或解析解；保持 CI-003 的正向要求 | 注入低于阈值的等效核数 ⇒ 资源门判红 |
| M8-F-013 | 台账四名中 concurrency_cache_test/variance_propagation_test 连注释命中都没有 | k_corr 自产 MC 证据与唯一守护仍是构建孤儿（tracked-but-unbuilt），ctest -N 无这些目标 | 在 tests/unit 注册四个 MC/回归目标，或显式退役其证据角色 | 目标进 ctest -N 且能红能绿 |
| M8-G-001 | 台账「三方向全部无门」不再成立：采集零用例（R11）与判定器注册（STD-REG）两腿已闭 | 残留「存在但从未注册的测试源」方向无门（CTEST-REGISTRATION 反向闭包缺失） | 同 M8-F-005：增加磁盘 TU 源到构建目标的反向闭包 | 新增未注册测试源 ⇒ CHK-MODULE-MANIFEST 判红 |
| M8-G-002 | no_checks_selected 分支 :1057 仍在（台账「0 命中」旧证据已过期） | 全合同化 SKIP 无 profile 级有效用例下限即判 PASS | 引入有效执行数下限或要求至少一项非 SKIP 真执行；SKIP 须显式登记 | 全 SKIP 场景 ⇒ 至少判 FAIL(no_effective_execution) |
| M8-G-003 | 与台账逐行一致 | 4 个恒真探针 + F-022 单文件半径；自检仍把恒真计入通过数 | 删 or True/True 探针并给出可失败判据；自检排除恒真项 | 注入恒真探针 ⇒ 基线自检判红 |
| M8a-F-001 | 脚本已随 ARCH-001 迁到 lib/algorithms/coverage/tools/（台账站点 tools/rcr_oracle_compare.py 已不存在） | RCR oracle 对照仍无 CI 载体与回归，缺省 CLI 指向不存在路径 | import 加守卫并在 CI 注册 RCR oracle 对照（可跳但须留痕）；缺省路径改现行构建树 | tests/ 出现 2.4.7 回归，或以 --require-rcr 判红 |
| M8a-G-004 | verify_actions_lock 在 checks.json 亦 0 命中 | 依赖锁/SBOM/actions 复验在注册表与 workflow 仍零接线 | 把 gen_sbom/dependency-lock/verify_actions_lock 注册为 CHK-PACKAGE/CHK-SCHEMA step | 注入锁漂移 ⇒ 对应门判红 |
| M8a-I-003 | tools 下仅 tools/README.md 与 tools/realdata/README.md | 工具 README 仍教用已不存在的 orchestrator.exe 与 FORBIDDEN 的 mingw64 PATH 注入；检查器资产无索引 | 重写 tools/README 为现行检查器索引，删除 orchestrator/mingw 章节 | README 内路径全部存在且无 FORBIDDEN 路径 |
| M8a-I-007 | 锁 generated_utc 未变，报告过期为主因 | 报告仍以现势口吻归因锁「九类工具全缺」，而锁当前仅 ccache 缺失；数据源已刷新、引用方未回写 | INVENTORY_REPORT 改为消费 toolchain.lock.json 生成，或标注时点 | 报告文本与锁 missing_tools 一致（机器比对） |
| M9-F-1 | 引用面 docs/interfaces/io/IO_002:46、PRODUCTION_EXECUTION_INVENTORY.csv:309 同型 | 8 个恶意输入 TU 仍 tracked-but-unregistered（根因按 M8-F-005 反向闭包缺失），5 处文档仍当符合性证据 | 注册这些 TU 或从证据面移除；按 M8-F-005 建反向闭包 | 四 TU 进 ctest，或不出现在任何符合性证据 |
| V13-N-01 | 默认形态 rc=1 由既有 errors 提供，EVID 仍只 WARN | EVID 层无路径列，3 枚自铸 EVID 结构上不可红；AUTHORITY_DIRS 的 returns/ 不存在 | EVID 增 evidence_path 列并纳入默认（--strict）判定 | EVID 无 authority 文件 ⇒ 默认形态即判红 |
| V13-N-02 | 本轮 7 errors 含 3 条 ID_FORMAT_VIOLATION（BOM 类 SCHEMA_VIOLATION 已不见） | 3-4 行 test_path 仍是散文串（含 (ctest ...) 与 tests/unit/CMakeLists.txt:441 master_flat_median） | test_path 规范为真实路径或结构化 test_id 列表；checker 对散文串判 SCHEMA_VIOLATION | test_path 每行可由存在性校验通过 |
| V13-N-04 | 生成器铸造族 42/63 口径未变（未逐行复算） | 63/63 全 VERIFIED 而三个读者零 VERIFIED 命中（status 列零门消费） | status 由生成器按采集面回写；check_traceability 系列增加 status 合法性断言 | 注入 VERIFIED 但无执行证据 ⇒ 门判红 |
| V13-N-05 | 台账「28 枚」为历史子口径，本轮以 63/63 + 逐点抽查复现 | 生成器铸造的 VERIFIED 行既无登记面宿主也不被任何门消费 | 同 V13-N-04：status 需可追溯宿主，或降级为 UNVERIFIED 并登记缺口 | 随机抽 3 枚铸造 ID 均能找到非铸造宿主 |
| V13-N-06 | CON-TRACEABILITY 已注册（CHK-SCI-REF step）但仍单向 | CON-TRACEABILITY 红而 CONTRACT-GRAPH 不读 TRACEABILITY.csv ⇒ 无 ID 集对账门，两面分叉恒不红 | CONTRACT-GRAPH 增 ID 集对账（docs/contracts/INDEX.yaml ↔ TRACEABILITY.csv） | 两表 ID 集差集非空 ⇒ 判红 |
| V15-N-16 | 运行期红绿未在本机定性（无构建树） | backend 测试真跑 build/astrocs 而门未声明构建前置；干净 checkout 上整体不可执行 | UT-BACKEND 增 build 前置声明（或改走 run/ci 构建树路径注入） | 干净检出上 UT-BACKEND 能真跑并红绿可判 |
| V16-N-01 | run/** 不入库；台账 P2 与 finding 自升 P1 并列记录以台账列为准 | 豁免依据指向 gitignored run/ 目录，干净检出不可复核 | 豁免依据入库（tracked+sha256）或改仓内可复算判据（JUnit 摘要/命令+rc） | 基线条目依据可在干净检出复现 |
| V17-N-03 | 可达性语义在 CI 侧仍只有 PROD-REACH-SELFTEST 一个 --selftest 执行点 | linux-control preset 与 CI 构建步均不开 CMAKE_EXPORT_COMPILE_COMMANDS ⇒ UT-CLI 该用例恒 SKIP | preset/构建步加 CMAKE_EXPORT_COMPILE_COMMANDS=ON，或把该用例改为不依赖 compile_commands | test_04 在 CI 构建树实际执行（非 SKIP） |
| V18-N-02 | 对照 NO-SERIAL-HEAVY 有真门（--selftest 与真形态并存） | 四类门在册只跑自检形态，交付二进制/生产图从未被看 | 为每门注册真形态 step（带真实输入路径），自检保留为负例面 | 真形态 step 在 CI 产物上执行并能判红 |
| V18-N-13 | 台账建议并入数值常量门而非新建门，本轮沿用 | 额度阈值无仓内来源锚（注释引控制包任务号 T411/T500，仓内零命中）；同源计数 + 超额截回 + 自造 finding 叠加 | 阈值入配置/合同并登记；禁止自造 finding 参与自身判定 | 注入硬编码线程数 ⇒ 该门判红 |
| V19-N-01 | total 由台账 40 增至 41（coverage 子树） | 41 处 gtest_discover_tests 在两个登记面双零覆盖，检查器对 gtest_discover_tests 零命中；schema 仍无 inputs/depends_on | check_ctest_registration 增 gtest_discover_tests 解析；schema 增依赖字段 | 注入未登记 gtest_discover_tests ⇒ CTEST-REGISTRATION 判红 |
| V19-N-02 | 台账 profiles 数字（68/137/74/7/0）已随聚合换代，本行以机制复现 | fatduck 零成员 profile 在零项执行下仍映射退出码 0（真机复验通道是恒绿空壳） | fatduck 空选应判 FAIL(no_checks_selected)，或把该 profile 的 harness 结果设为必需输入 | fatduck profile 零执行 ⇒ rc!=0 |
| V19-N-03 | step 名与台账一致（LINUX-PREPARE-FIXTURES/LINUX-BUILD-ROOT-GRAPH/WINDOWS-VALIDATE-CANDIDATE） | 两个 linux serve 步只声明 2 道消费者且无 require_outputs；B10 无反向闭包，新增消费门不会让任何门变红 | 为 serve 步补 require_outputs 与消费者计数；B10 增反向闭包（注册表→workflow 覆盖） | 新增消费门未被任一 step serve ⇒ 绑定门判红 |
| V19-N-04 | 不得把 BUILD 指回 build/linux-control（会降级判据） | UT-QUALITY 的门禁对象在 CI/CMakePresets/workflows 三面零生产者 ⇒ LNX-005 6 用例恒 skip 而非豁免门仍记 PASS | 在 CI 产 lnx_v5_clean_rel/astrocs（或把发布包判据改到真实打包步并登记豁免） | 发布包用例在 CI 真跑；skip 须显式登记豁免 |
| V19-N-05 | 口径由台账 21/135 重算为声明 27（step 级）/147→现 138 step | 实际依赖 gcc/tar/ctest 的门零宿主工具声明 ⇒ 缺工具时按 argv[0] 探测或被 except 吞成假绿，绕过 FAIL(prerequisite) 留痕 | 为使用 gcc/tar/ctest/taskset 的 step 补 prerequisite_tools（含 ctest），并加「声明面 ⊇ 命令面」校验 | 命令面出现未声明工具 ⇒ 注册表校验判红 |
| V19-N-07 | schema 已新增 steps 字段（相对台账的 145 项时点） | 跨门输入依赖登记面缺字段，且 run/ 前缀无条件豁免 ⇒ 登记面无法区分输入与产物，定向复跑选不到生产者 | schema 增 inputs/requires_artifacts；run/ 豁免改为按登记 outputs 精确匹配 | 改生产者能通过 --focus/--changed-from 选中消费者 |
| V19-N-08 | 5 份 CSV 已不再受跟踪（git ls-files 空），但写读机制未变 | 自写自读闭环仍在：测试自己写权威台账数值再读回当判据，非豁免门的 dirty 判定被自身豁免遮蔽 | 测量工件落 run/ 并作为 outputs 登记；判据改为独立 oracle；移除 artifacts 前缀豁免 | 干净检出下 UT-BACKEND 不得改写工作区（dirty 可红） |
| V19-N-09 | 台账点名 7 门中 WORKSPACE-ADOPTION/RECONCILE-STATE 已按 docs/ci/01_CHECKS.md §2.1 退役（2026-09-16），不计入现存门 | 死模式（零 tracked 命中路径）永不触发 ⇒ 对应门不能红；注册表换代后触发面未订正 | 每模式需≥1 tracked 命中的 R13 断言；impact_map 触发面随注册表重建 | 任一 rule path 零 tracked 命中 ⇒ 注册表校验判红 |
| V19-N-10 | WIN-PACKAGE-CANDIDATE 的 candidate.zip 项同型 | 门一执行即在证据目录留下不入库不被忽略的产物，outputs/dirty_ignore/.gitignore 三面零联动 | 产物改落 run/ci 或显式 gitignore + R14/R13 断言登记 | artifacts 下产物必属 tracked/ignored/登记三者之一 |
| V19-N-11 | 全链无处校验 glob 命中数（本轮仅读源码，不实跑 discover） | glob 型参数不校验命中数，pattern 改成匹配 0 文件时 discover 打 Ran 0 tests 仍 exit 0，FAIL(missing_output)/空输出守卫均不触发 | 执行器补「含星号/问号 pattern 探测命中数>0」；R4/R6 增 glob 命中数校验 | 把 -p 改为匹配 0 文件的 pattern ⇒ 判定 FAIL |
| V21-N-05 | 台账原判据「无构建树即 QA-001_SKIP + rc=0」一腿已按 GAP-027 修好，不再作依据 | 构建失败但无 warning 行时 warn 为空/0 ⇒ 判绿；退出码未纳入判定 | 采集 cmake 退出码并与 warn 计数联合判定；构建失败即判红 | 注入构建失败且无 warning ⇒ CHK-WARN 判红 |
| V21-N-06 | ci/exemptions.json 由 CI-001 建立（high_water max_entries=0） | 防掩盖门既不看子检查退出码（异常还 except: pass），又盯全仓无生产者的 waivers.json；现行豁免面是 ci/exemptions.json，两面脱钩 | 判据改读 ci/exemptions.json（R14 已强制存在）；子检查退出码纳入判定并禁止裸 except | 真实豁免面新增条目而防掩盖门无反应 ⇒ 判红 |
| V4-N-04 | R11 校验器在位但不进任何门（台账结论复现） | 「discover 目录 0 用例即 strict FAIL」这条新规则（R11）自身无 CI 执行面：ci/validate_registry.py 在注册表 0 载体 | 把 ci/validate_registry.py（含 R11/R14）注册为 CHK-REGISTRY-DOC-SYNC 或 CHK-MODULE-MANIFEST 的 step | 故意让某 discover 目录 0 用例 ⇒ CI 判红 |
| V9-N-02 | BOM 类 SCHEMA_VIOLATION 本轮已不见；现默认 rc=1（7 errors） | 早退使一次只暴露一类问题，门结构对可证伪性削弱 | 表头失配时不 return，继续收集行级 SCHEMA_VIOLATION 并一次性输出 | 表头错误 + 行级错误同时存在时两者都被报出 |
| V9-N-07 | 台账 5 门名单与现注册表一致 | 5 道非豁免门仍只注册 --selftest/--selfcheck，真实判定模式需交付物参数而 CI 从不传 | 为 5 门各注册真形态 step（真实二进制/图/日志输入） | 交付二进制 ISA 泄漏可在 CI 判红 |
| V9-N-08 | 台账 222679 字符单条注释现象本轮未复算，仅复核正则与豁免口径 | //.* 带 re.S 一路吞到文件尾，注释抽取失真；「冻结」豁免整文件 | 逐行/状态机抽取注释；豁免粒度改行级 | 对 lib/algorithms/coverage/tests/synthetic_gate.cpp 复算注释条数=逐行口径 |
| V9-N-09 | 台账原引 win-test-summary.json 已不在树内，本轮以 argv 取证 | 目标改名/未构建时 ctest 报 No tests were found 仍记 PASS；glob 目标无 ctest_targets 登记 | argv 增 --fail-if-no-tests；glob 目标登记 ctest_targets 以做存在性校验 | 把 phase2_.* 改为匹配 0 目标 ⇒ CTEST-PHASE2-GATES 判红 |
| V9-N-10 | 行号 897-908 → 898-906（微漂） | waivable=false 的门只要自查 sys.exit(77) 即可从门禁面隐身；waivable 字段与总判定可被绕过 | SKIP 退出码仅在 waivable=true 或显式登记豁免时接受，否则判 FAIL | waivable=false 的门返回 77 ⇒ 判红 |
| V9-N-11 | 台账「豁免硬编码」与 §1 登记制不符仍成立 | 空输出即绿（没有任何东西被证明）；豁免硬编码在执行器源码而非 ci/exemptions.json 登记面 | 移除名单或改结构化计数断言；豁免入 ci/exemptions.json | 清空两门输出 ⇒ 判定 FAIL(empty_output) |
| V9-N-12 | 台账 145 门/89 不可达 → 现 42 门/31 不可达，方向一致 | --changed-from HEAD 定向复跑选不到大部分聚合门，「定向复跑全绿」不能证明对应门绿 | impact_map 触发面按聚合门重建（或为每门登记 fallback） | 每个顶层门至少 1 条可达规则 |
| V9-N-13 | 台账 137/147 → 现 127/138（聚合换代后数字漂移，方向一致） | 改 checker 不触门，--focus/--changed-from 看不到该门新行为 | changed_paths 系统性补齐自身 checker 路径与 ci/checks.json | 改任一注册在册 checker ⇒ 至少 1 门被选中 |
| V9-N-14 | 台账 21/2/379/33 → 现 21/2/383/33 | CI-BINDING-TESTS pattern 过窄；validate_registry 等登记面工具零载体 | 扩采集面；把 validate_registry 注册为门 | CI 控制面测试 100% 进采集面 |
| V9-N-16 | 台账 P0 行；TRACEABILITY-CODE 已退役，本行对象为 TRACEABILITY | 末行无条件 return 0（查出什么都是绿）且仍改写受跟踪 reports/ 路径 | 退出码按 errors；输出改 run/ | 注入断链 ⇒ TRACEABILITY 判红且工作区 dirty 可判 |
| V9-N-17 | 脚本哈希未变，4 处跳过逐字在位 | 缺 taskset 的节点上 CTEST-P1DRZ-TASKSET-INVARIANCE 永不红，DRIZZLE-DET-001「带锁已修」不可证伪；SKIP 未登记豁免 | 跳过改 ctest SKIP_RETURN_CODE 或非零并登记豁免；缺工具时判红 | 缺 taskset 场景 ⇒ 该门按豁免登记处理而非静默绿 |
| V9-N-18 | 三处裸 except 同族随 CI-001 处置 | 字面量在场不等于合同一致；doc_symbols 用 rglob 使本机与干净检出结论可不同 | 判据改结构化比对；rglob 限定 tracked 面 | 注入影子树/计数下降 ⇒ 对应门判红 |
| V9-N-19 | 台账「六门证据全为 2026-09-05T20:48:42Z」的载体已随 artifacts/ 删除，缺口扩大 | 两门现无任何有效证据（旧证据树已删除），豁免状态翻转无一致性门拦截 | 非豁免 DEEP 门须产出当前 SHA 证据；翻转 waivable 需一致性校验 | 非豁免 DEEP 门缺当前 SHA 证据 ⇒ 判红 |
| W5-N-12 | 锚 65 → 63-66 微漂；台账要求随 CI-001 重写检查器时统一处置 | 活跃 checker 自身吞错导致分母静默缩水；同族裸 except 三处（check_doc_symbols.py:51、check_full_integration.py:34、generate_contract_r… | 异常显式计数或 fail-closed；新增 scanned 计数断言 | 存在不可读文件 ⇒ 判红或计数留痕 |
| W6-N-01 | 基线 160 目标与台账一致；该门现为 CHK-MODULE-MANIFEST 下 step CTEST-REGISTRATION | 门只看「注册名出现在活动 CMake 源文本」与「是否在基线名单」，无「已注册进真实构建目录/可被执行」维度 | 增构建图可达性断言（target 须在真实构建目录或 add_test 面） | 基线名不在任何构建图 ⇒ CTEST-REGISTRATION 判红 |
| W6-N-02 | 口径由 18/135 → 27/147 → 现 step 级 27/138；R12 名额已被 steps 派发规则占用 | 实际依赖 ctest/tar/gcc 的门零宿主工具声明；姊妹门 UT-BACKEND/UT-IO/UT-CLI 的 prerequisite_tools 为空 | 补声明 + 「声明面 ⊇ 命令面」校验；缺工具走 probe_prerequisite 的 FAIL(prerequisite) | 命令面出现未声明工具 ⇒ 注册表校验判红 |
| W6-N-04 | 台账四站点中三处已按 ARCH-001 重定位（旧 include/astrocs/... 路径不存在）；aio_hips.h 已含 8 处（同族已修面） | 三站点公开头仍无 struct_size/abi_version；现行 138 step 内无 ABI struct 尺寸/镜像一致性采集门 | 三头补 struct_size/abi_version；新增 ABI struct 尺寸门（或纳入 CHK-ABI） | 移除某头 struct_size ⇒ CHK-ABI 判红 |
| W6-N-06 | build.sh 与 preset 不同源一腿本轮未复核 | vendor.contract_schema 悬空引用未消；verify_toolchain 在注册表零载体，改 preset 不触发任何门 | schema 引用改 preset-contract.json；verify_toolchain 注册为 CHK-ENV-ADOPTION step | preset 改动触发对应门；悬空 schema 引用 ⇒ 判红 |
| W6-N-07 | 台账「引号 include 写仓库根相对路径」一腿已修（现为同目录 include）；ipv 两文件按旧路径不存在需另定位 | core/*.h 等仍显式用 size_t 而无 stddef/cstddef；仓内无「每头 -fsyntax-only」类门，仅手工把仓库根当 -I 才可解析 | 补 stddef 包含或类型别名；新增每头语法自足门（clang -fsyntax-only -I 各头目录） | 孤立编译任一头无隐式依赖 ⇒ 门判红 |
| W6-N-09 | 台账「受跟踪 CMake 面 0 定义点 + untracked lib/snr_estimator/CMakeLists.txt」两半均已变化：定义点现受跟踪、旧 untracked 路径已被迁移 | 受跟踪 CMake 面已有 add_library 定义点但根图 add_subdirectory 被注释（且注释事实陈述已过期）⇒ 门卫恒假、p1_noise_adapter 永久零注册却在 known_failure… | 要么收编噪声子图并同步 install_layout/tests 门卫，要么退役双在册条目并把注释改为事实 | 干净检出上 p1_noise_adapter 的注册状态与双在册口径一致（不再承诺自动复活） |

### DOC-001（74 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| L28c-E-002 | 台账另一锚 lib/algorithms/photometry/docs/algorithm.md 本轮实测存在（:3 被引） | 迁移后参考实现文件不在该路径（悬空注释引用） | 按 ARCH-001 清单改指实际位置，或删除该行参考注释 | grep -rn synthetic_photometry 的每个路径均存在 |
| L28e-D-002 | 台账备注称 docs/standards 已不在最新权威链，但该文件仍在 DOCUMENT_INDEX:526-553 登记为 ACTIVE_NORMATIVE 且 check_standards_registry.py… | 标准文档用不可解析的 ID 作示例，示例即伪锚，读者按示例写注释必产生悬空引用 | 示例改用可解析 ID（如 SCI-DRZ-001 / ALG-DRZ-OVERLAP-001）或显式标注为示意 | grep 示例 ID 全部可在 docs/contracts/INDEX.yaml 或对应 SCI/ALG 页解析 |
| L28e-E-001 | 台账记 16 行，本轮实算 17 行（表由 67 行缩到 63 行后仍含该 17 行） | ID 只在追溯表内被铸造，无任何可执行宿主（ctest/测试文件）承载，追踪链在实现侧断开 | 为两族 ID 建立共址测试宿主并在 module.yaml/tests 内回挂 ID，或把无宿主的行从追溯表撤除并登记 | grep -rl 'TEST-UPMW/TEST-PR-UPM' lib tests ci 非零命中，且 check_traceability.py 对这两族能红 |
| L28e-E-003 | 台账称「序号 ID 全仓零字面命中」，本轮实测 SCI 页内已存在（:138-141），缺口收窄为「实现侧零命中」 | SCI 登记面已给序号 ID，但实现/测试/manifest 无一处引用，登记面与落地面对不上 | 在 upm.cpp 与 p1/p2 测试头或 module.yaml test_ids 回挂序号 ID（或统一改挂语义式 ID 并同步 SCI 登记） | grep -rn 'ALG-UPM-002' lib tests ci 非零命中 |
| L28e-E-004 | 同头任务号 SCI-ANCHOR-001 有宿主（check_doc_line_anchors.py 等），佐证确属缺失 | 合同自声明 ACTIVE_NORMATIVE 但无宿主登记面（INDEX.yaml 零命中），锚合同 ID 不可解析 | 把 ALG-ANCHOR-001 登记进 docs/contracts/INDEX.yaml，或撤 ACTIVE_NORMATIVE 并入既有 ID | grep -c ALG-ANCHOR docs/contracts/INDEX.yaml >=1 |
| L28e-E-005 | 台账记「三处即全仓全部命中」，本轮仍为 3 处 | 跨域引用（STAR_DETECTION.md）使用未在任何登记面定义的全称别名，真源页只认短 ID | 二选一：在 docs/science/{ASTROMETRY,PHOTOMETRY}.md 与 INDEX.yaml 正式登记全称别名，或把 STAR_DETECTION.md 三处改用短 ID | grep -rn 'SCI-ASTROMETRY-001' 的命中全部可在 INDEX.yaml 解析 |
| M1a-E-001 | 台账称 CMakeLists.txt:528，本轮实测 :554（并发线改动，锚再漂移） | 活动文档以已失效的构建事实与行号锚冒充现状（根 CMake 早已收录 ipv 目标） | module.yaml 删『未编入根 CMake 主构建/无 ipv 目标』并改锚为 path::符号 形；导出清单一律去行号 | module.yaml 内 CMake 现状陈述与 CMakeLists.txt 一致，且全文无行号形锚 |
| M1a-E-002 | 原 finding 另有 docs/24_/25_ 归档子项，本轮未逐条重算 | 注释引用的是旧世代控制包文档编号，仓内无该文件（悬空引用） | 改指现行权威（docs/science/STAR_DETECTION.md / docs/algorithms/STAR_DETECTION_ALGORITHMS.md）或删该行 | grep -n '规范:' 得到的路径全部存在于当前树 |
| M1a-E-003 | 读时哈希已变：本行判据文件 docs/algorithms/PHASE3_PROJ_IMPL.md@0be9b231db9b（包内）仍为现文；被测源码在本次复核窗口内被并发线从 lib/phase3_session/ 迁… | 文档把一次性实测行数写进活动合同，源码增长后成为失真陈述（且 p3_resample.h 记 58 实为 150） | 合同头部改 path::符号 锚、不写死行数；并在迁移完成后同步生产源路径（现文所指 lib/phase3_session/p3_wcs.* 已被并发线移走） | grep 行数断言与 wc -l 一致或不存在行数断言 |
| M1a-E-004 | 行号漂移：：89/:162 → :92/:165 | 同一 ID 在 SCI/ALG 两侧承载不同测试内容，追溯 ID 不可唯一解析 | 由一域持有该 ID 并让另一域引用；或拆成两个 ID 并各自登记 | grep 同一 TST ID 的语义描述在两份文档内一致（或 ID 不同） |
| M2a-C-3 | 行号漂移：doc 内锚 :1715 现落到 :1891-1895 注释块；§10 表中 DISP-DRZ-004 行由包内 :230 漂到读时 :232（并发线在改） | 实现已删静默掩膜分支，ALG §5/§10 与注册表 §D.drizzle 三处登记未同步（登记与实现相反） | 把 DRIZZLE_GEOMETRY.md:118/:230 与 STANDARDS_REGISTRY.md:172/:182/:268 改写为「NaN/Inf 经 F_p 传播、不掩膜」并翻转 DISP-DRZ-004… | grep -rn 静默 continue docs/algorithms/DRIZZLE_GEOMETRY.md docs/standards/STANDARDS_REGISTRY.md 零命中且注册表 DISP-DRZ… |
| M2a-E-1 | drizzle target 行号台账记 :408，本轮实测 :434 | 合同/README 以不存在的文件与失效行号锚充当证据面，读者按锚复核必失败 | 把 api.cpp/02_FROZEN 锚改为现行文件（hp_drizzle_api.cpp 等），CMake 行锚改 path::target 名 | grep 引用的每个路径均存在；CMake 行锚改为 target 名后可解析 |
| M2a-E-2 | 台账记漂移 +279，本轮 unproject 622→901（+279 一致）；GAIA_QUERY.md 另有 :26/:40/:50/:76/:83… 多站同族锚 | 文档以行号锚绑定 C 源码，源码增长后锚整体失效；README 的核对基线数字未随实现更新 | 全部 gaia_client.c 行锚改为 path::函数 形；README:149 改述为『核对时点源码 SHA + 行数』 | grep 得到的锚均为符号名或带 SHA 的时点声明 |
| M2a-E-3 | 台账三处证据本轮逐字复现 | 跨文档节号锚失配（§6→§8）；module.yaml 仅登记设计 ID，无可执行测试面标识 | 把两处 §6 改为 §8；module.yaml 补 ctest/测试文件面 ID | grep 'DATA_SEMANTICS.md §6' GAIA_QUERY.md 零命中；module.yaml test_ids 含可执行面标识 |
| M2b-C-07 | 注册表 md5 由包内 c026817d6f93 变为读时 f404cbf8f58b285ff135deecb7b75f6c、复核末 3ad093e98498548bd7bbf47503c87f7f（CI-003 及并发… | 注册表与 ALG 页的活动事实性断言未随实现更新（两处均为可复算的计数差） | 注册表 :233 改 13 码（或改指 STATUS_COUNT）；PHASE3_PROJ_IMPL.md:16-17 改为不写死行数、只留 path::符号锚 | python3 -c 重算枚举项数=13 与 wc -l 一致，且 docs 内不再出现与实测不一致的「165 行/17 码」 |
| M2b-E-01 | 台账记 product_begin :386→:466，本轮进一步漂到 :551（并发线改动） | 锚与导出面计数均未随实现更新（:386→:551、9→14） | 行锚改 path::符号；PUBLIC_API 的导出行号改为符号清单或按头文件实际宏数自动核对 | grep 引用的 symbol 行号锚可解析；导出计数与 grep -c AIO_HIPS_EXPORT 一致 |
| M2b-G-03 | 读时 md5：CODE_STANDARD b8074c236b09、TEST_STANDARD c8f0b401e72b、COMMENT_STANDARD 6590933452f2（与包内一致） | L2 标准以已作废世代的控制包规格为权威来源，并写入与 ENGINEERING_SPEC §1 直接冲突的『正式工具链』 | CODE_STANDARD.md 权威来源改挂 ENGINEERING_SPEC §1/§7；MinGW 降为兼容构建而非正式工具链；COMMENT_STANDARD.md:45 的历史报告路径改述或删除 | grep V19R2/v19r2 docs/standards/ 零命中或仅在历史声明上下文；工具链表述与 ENGINEERING_SPEC §1 逐字一致 |
| M2b-G-04 | 断链 URL 子项见 M8a-I-004 | 第三方来源登记与实现注释对同一事实给出相反陈述（只判登记事实，不判法律定性） | NOTICE 按现实现改写（明列移植范围与许可来源），或撤『未迁移/未复制』句并附许可依据 | NOTICE 与源码注释对迁移范围/许可来源的陈述一致 |
| M3-A-009 | 台账三处证据本轮逐字复现（:77/:93 与 calibrator.cpp:122） | 术语错（binary32 无饱和语义）掩盖真实数值行为，并与同篇 §4 的不传播 NaN 承诺冲突 | 把 saturation 术语改为 IEEE-754 ±∞/NaN 语义并说明下游处置（fail-closed 或显式置 invalid） | CALIBRATION.md 内 saturation 行与 §4 不传播 NaN 条款一致，且与 calibrator.cpp 实际行为相符 |
| M3-C-004 | 台账记 README 仅一句话标注；本轮复核同 | 历史流程文档仍以活动文件形态存在于 lib/ 内且自身无历史标记，直接读到者会照旧文实现 | 给 CALIBRATION_PROCESS.md 加 ARCHIVED_NON_NORMATIVE 抬头（或移入 docs/archive/），并逐条标注与现行 ALG-CAL 的差异 | grep -c 'ARCHIVED_NON_NORMATIVE' CALIBRATION_PROCESS.md >=1 或文件不在活动树 |
| M3-C-006 | 台账记头内 17 处 AC_API 命中、12 处函数声明，本轮复核头注释自述 12 | 文档计数与头文件自述/实际符号数不符（按 14 核对会得出少两个符号的假结论） | 四处 14 改为 12 或改述为『12 个 legacy 符号 + entrypoint』 | grep '14 个 AC_API' 零命中且计数与头文件一致 |
| M3-C-007 | 台账三处证据本轮逐字复现 | registry 页引用了 INDEX.yaml 未定义的合同 ID，且版本/状态与 module.yaml、代码三口径并存 | registry 页 upstream 改用 INDEX.yaml 已定义的 ID；版本/状态与 module.yaml 单一源对齐 | grep registry 页引用的每个 ID 都能在 INDEX.yaml 解析；三处版本串一致 |
| M3-C-013 | 台账记头 :102 与 simple 体 38-177 的 0 命中，本轮复核同（范围放宽到 38-200 仍 0） | 头合同列的返回码在对应函数体内不可达，返回码作为唯一机器可读错误通道失真 | 要么实现 -3 分支，要么从头部合同删除该码并统一同条件返回码 | 头内每个返回码在同一函数体可 grep 到，且同条件不出现两种码 |
| M3-C-014 | 台账以 pragma 缺失立论（不以行号），本轮复核成立 | ALG 行文把未并行段描述为已并行，读者据此无法判断真实并发面 | ALG 按源码实况改写（去 OpenMP 声明或注明串行），或在实现侧补并行（另立代码域任务） | ALG 内每处 OpenMP 声明都能对应到源码 pragma |
| M3-I-001 | 台账记 tests/unit/CMakeLists.txt:776 add_test(cal_photometry_apply)，本轮实测 :792（行号漂移） | 活动文档的现状断言未随落码/接线/防护修复同步，六处均可被单条命令反证 | 按现树逐条改写这六处陈述（cosmetic 已有 src+CMake、dark_optimizer 已在 cli、负 median 已有守卫与专测） | grep 上述六处断言与当前树一致（每条都能被命令证实） |
| M3b-E-01 | 台账另记 csv PSF 行=0，本轮复核 TRACEABILITY.csv 的 PSF 关键词仍 0（见 V6-N-01） | 两文档共用同一失效行锚（实现 2555 行、函数起点 1749） | 行锚改 path::符号；行数断言删除或改带 SHA 的时点声明 | grep '1599-2353' 全仓零命中 |
| M3b-I-01 | 子项 1（不可达死守卫/NaN 漏检）本轮已修：:416 注释自述原实现问题、:423 改 normalize_angle_deg_bounded 显式失败；故本行残余仅字段计数文字 | 文档字段计数与结构体成员数不符（结构体 13 成员、含 status），且锚 :16-31 未覆盖全部成员 | 两处写『12 数据字段 + status（13 成员）』或按现文枚举；锚改 path::struct | grep '12 字段' 与结构体成员计数一致 |
| M4-E-01 | 台账记 +215/+108，本轮复核同量级 | 同 M1a-E-003：把一次性实测行数写进活动合同，源码增长后成失真陈述 | 合同头改 path::符号 锚；实测值只入带日期的偏差登记 | grep 行数断言与 wc -l 一致或不存在 |
| M4-E-02 | 台账记根 CMake 锚 :336-346 → :396，本轮未逐条重算该子项 | 同族：活动合同写死实测行数，源码增长后失真 | 去行数断言，改 path::符号 锚 | grep 行数断言与 wc -l 一致或不存在 |
| M4-G-01 | 台账记 :437/:544/:628/:691，本轮行号一致 | 同一文档内两代权威声明互斥，级别关系（ALG vs SCI）不可判定 | 删除遗留的『语义权威=本文件 §4.1』句（或改写为『本文件只在 SCI-REJ-001 授权域内细化』） | grep 语义权威 的命中不含 §0 链外的自我授权表述 |
| M5b-E-01 | 原判据的『宪章 §F.1』子项已修（MODULE_MAP.md:33 现引 ASTROCS_DESIGN §3.2/§4.2/§5.2，且 grep -c '§F\.1' ASTROCS_PROJECT_CONSTITU… | 节点表行锚未随源码增长更新（差 >2000 行）；同锚被 RELEASE_STATUS 等页面复制 | 全部改 path::符号（p1_nodes/p3_nodes）；行号锚一律禁用于活动文档 | grep ':4257' 全仓零命中 |
| M5b-E-02 | ARCH-001 迁移清单文件头自述当前 HEAD=8f0a4c6b（旧于本树），清单本身亦需重生成 | 架构文档以旧目录名描述现行结构，模块根不可解析 | 改指 lib/algorithms/projection/resample/fits_output + lib/phase3_session，并声明与 p3_nodes 五节点的对应 | 文档内每个目录路径均存在 |
| M5b-G-17 | 行号漂移：台账 :40 现仍落到 :40（同段） | 状态页以完成时口吻宣称当前提交实测绿，而证据锚仍是旧 BASE 且基线表列着同一批在册红灯 | 把「当前提交实测绿」改为绑定可复核三元组（SHA+命令+rc），或直接改为 NOT_VERIFIED 并指 known_failures.json | 文档内不再出现无 SHA/命令/日志指针的「当前提交实测绿」；known_failures 的在册 P1 与状态页口径一致 |
| M5b-I-01 | 台账两子项中「正文以 PASS 冒充现状」已修，余「登记面双活」 | 正文口径已收敛，但登记面仍把一个事实挂在两个活动条目上（旧页 ACTIVE_INFORMATIVE 无历史标注） | 在 DOCUMENT_INDEX.yaml:56-57 给旧页加 ARCHIVED/历史标注（或移出活动索引），使 RELEASE_STATUS 只有 owner 版一个活动落点 | grep -n 'docs/RELEASE_STATUS.md' docs/DOCUMENT_INDEX.yaml 的条目带历史/归档标注 |
| M5b-I-03 | 行号漂移：台账 :81/:82 现落到 :80/:81 | 表内重复行未删、状态词裸写与反引号混用，一份事实两个落点 | 删 :81 重复行；全表状态词统一按 ASTROCS_DESIGN §11.3 词表加反引号 | 同表内 (模块, 验收项) 唯一且状态词全部带反引号 |
| M5b-I-06 | 台账证据三点本轮逐字复现 | 历史审计快照未加归档标注却仍在活动索引，且表内路径已因迁移失效 | 给三份 CSV 加 ARCHIVED_NON_NORMATIVE（或移出活动索引）；表内路径按 ARCH-001 清单改写并注明时点 | DOCUMENT_INDEX 内三份 CSV 带归档标注；表内引用路径全部存在或在时点声明上下文中 |
| M6a-C-003 | 台账三处证据本轮逐字复现 | 节点表注释把目标态写成现状（AIO-002 未内建），snr 侧对『唯一生产实现』的落点与构建图/节点接线不一致 | 按构建图与 README 自证改写节点表注释与 memory/README 的『唯一生产实现』表述 | 节点表注释与 CMake 实际收录源一致；同一模块内不再出现互相否定的『唯一生产实现』句 |
| M6a-I-005 | 台账自述 38 条抽样中 20 条不符但本轮不重算该抽样数，只复核在位实例；lib/phase2/README.md 已随迁移落至 lib/algorithms/coverage/README.md | 同一族免锚现状断言分散在多份 README/memory，缺可机器核对的锚点 | 为每条断言补 path::符号/命令锚或删除；DEFERRED 的 session 目录随 INT-001 处置 | grep '唯一生产(入口/实现/调用点)' 的每条命中同句含可解析锚 |
| M6b-E-003 | 台账记 158/114/91，本轮 159/114/92（矩阵有并发增量） | 两个 ID 登记面并存且无包含关系，机器无从判某 ID 是否合法 | 收敛为单一登记面（矩阵引用必须能在 INDEX.yaml 解析，或把 INDEX.yaml 降为矩阵子集并机器校验） | check_traceability_matrix.py 增加「矩阵 ID ⊆ INDEX.yaml」规则并能红 |
| M6b-G-002 | 原判据中 §17-12 AGENTS.md 状态机行已修（本轮 0 命中） | 登记面 4 份并存未收敛；ACTIVE 架构页仍以已被取代的旧入口描述数据流 | 把 archive/review 两份移出活动面或加归档标注；docs/ARCHITECTURE.md 的旧入口改为 astrocs 三命令口径并给取代声明 | RELEASE_STATUS 活动面唯一；docs/ARCHITECTURE.md 内 orchestrator.exe/astrocs-stage2 仅出现在「已被取代」声明上下文 |
| M6b-G-003 | 台账三点证据本轮逐字复现（行号 :114→:116 漂移） | 以完成时口吻宣称机器门实测 PASS，但该检查器从未登记进唯一注册表（无 SHA/命令/时间/日志指针） | 要么把该检查器登记进 ci/checks.json 使其成为真门，要么把该节标为历史并删除 PASS 断言 | ci/checks.json 含该检查器且文档 PASS 断言绑定 SHA+命令+rc |
| M7-A-120 | 台账统计口径与本轮完全一致（19 行、六词全 0）；GLOSSARY.md 读时 md5 7c9c51ae7d37 与包内一致 | 词典自称唯一权威却缺六条核心量的词条出口，读者无法在唯一落点查到同名术语口径 | 为六条量补词条（或显式登记为 legacy alias 并指向权威锚），并让工具可判『核心术语全覆盖』 | python3 统计上述六词独立行数全部 >=1；且 docs_machine_consistency.py 类工具能对缺词条变红 |
| M7-A-121 | 台账记子事实『flags→因子映射无定义』已闭（impl 内 16→0/2→0.1/1→1.0/未知→0.5 可见），本轮复核属实 | 同名输入对象在两份 FROZEN SCI 里一为浮点 [0,1]、一为 uint32 位掩码且互不指认；quality_mode 形参被 (void) 丢弃，声明域无实现 | 统一 quality 的类型口径（或改名区分 quality_factor 与 quality_flags），quality_mode 要么实现要么从签名删除 | 两份 SCI 对同一字段的类型声明可交叉解析；quality_factor 不再出现 (void)mode |
| M7-A-124 | DATA_SEMANTICS.md 自述『本文不在此处单方改写（归 Phase1 域）』，故残留属登记未同步 | 词典两行的权威锚仍指 §4a，未同步同一合同 §30 的目标态双值口径，唯一术语权威与合同不一致 | 词典 variance/ivar 行补『§30 输出态为 NaN 同态双值』并改锚到 §30 | 词典行与 DATA_SEMANTICS §30 口径一致且可机器比对 |
| M7-A-212 | 台账三子项中 out_hot/out_cold 已闭；余两子项在位 | 数学断言不普遍成立（同值复制可移动中位数，如 {1,2,3}→{1,2,3,3}）；背景 label 0 无定义 size 的说明与连通域定义相反 | 把『天然免疫』改为带条件的正确表述（或给出反例边界）；label 0 行改为『不参与 size 统计』 | 文中不再出现无条件的『天然免疫』；label 0 语义与连通域定义一致 |
| M7-A-213 | 台账记 4 处（DRIZZLE :215 → 现 :217），本轮 :110/:101/:128 亦在位 | 不同量纲/类别项之间用『≪』作不可证伪的定性排序，无各自数值与比较阈 | 每处给出各项数值、单位与比较阈（或改为可判定的定量不等式） | grep '≪' 的每处同句含数值+单位+阈值，或零命中 |
| M7-E-201 | 台账引 :13-16,153-164,274-420,530-576；本轮锚已改写为 :154,276 但仍不中 | 行号锚在迁移+重写后整段失效（引用行不含所述语义） | 一律改 path::符号 锚（如 ipv_wcs.cpp::build_... / spherical_overlap.cpp::HP_CIRCUMRADIUS_FACTOR） | grep 引用的行号锚可解析到所述符号 |
| M7-E-202 | 同篇 :46/:80/:130 同族自引，本轮未逐条重算 | 文档内自引使用绝对行号，正文增删即失效 | 自引改节名/句锚（如 §4 nominal contributors 段） | grep 文档内自引零行号形 |
| M7-G-105 | 行号漂移：台账 :136 现落到 :146（healpix 偏差表前一行） | 条款列把项目工程上界写成 Górski §5.2/§5.3 的要求，且实现并无该上界检查，「checked 收口」为假 CONFORMANT | 要么删该行的「order ≤ 29」要求并如实标 PROJECT_DEFINED，要么在 require_valid_nside 补上界（代码域另立任务）；SIP 注释改挂 SIP 原始文献或删条款号 | check_standards_registry.py 对该行做负向注入时能红，且 require_valid_nside 与条款文字一致 |
| M7-I-201 | 台账三点证据本轮逐字复现 | 两份 SCI 页对入口级精度给出互斥默认；且 SCOPE 举例的 1e-12 门是 UPM 专属门，不适用于 CAL 等价路径 | 由 SCI 层收敛：SCOPE 改为『按域声明默认精度』并回指各域页；CAL 页显式声明其为默认 FP32 的例外域 | 两份 SCI 页对 calibration 域的默认精度陈述一致（或 SCOPE 不再作全局默认断言） |
| M7-I-205 | electron 许可域仍只在 GLOSSARY:9 一行标注（本轮同见） | 词典自述『每个核心术语恰一个含义』，却在同表内用 K 表达 healpix order 与 calibration_units 两义，SCI 页再给第三义，无消歧词条 | 为 K 增加消歧词条或改用非冲突记号（如 healpix order 写 O、暗场缩放写 k_dark）并同步 SCI/ALG | 词典内 K 的每处出现都能由唯一词条判定其义 |
| M8-C-003 | 台账路径锚漂移（lib/plate_solve/src → lib/algorithms/platesolve/cpp/ipv/src），本轮按现树取据 | 文档对调度策略的事实性陈述与实现相反（且确定性论证建立在错误的调度描述上） | README 改为 schedule(dynamic) 并重述确定性依据（整数归并可交换） | README 内的调度描述与源码 pragma 一致 |
| M8a-C-004 | 台账引根 CMake :408，本轮 :434 | README 的治理定性（外部副本/被忽略）与构建事实（仓内编译为 astrocs_drizzle 并交付）相反 | README 改为『仓内生产树，随 astrocs_drizzle 构建并交付』；活跃性表按现树重写 | README 内不再出现『本地副本/.gitignore 忽略』定性；活跃清单与 CMake 目标一致 |
| M8a-E-002 | 台账记 :127-129，本轮同 | README 引用从未入库（或已删除）的规划文档路径，悬空引用 | 三处改指现行文档或删除该『设计文档』小节并登记 | grep 路径全部存在 |
| M8a-E-003 | 台账记 docs 侧 0 命中，本轮 docs/archive/HANDOVER.md 有 1 处（归档面，非活动） | 偏差号在实现/测试/README 侧被使用，但 L2 登记表未收口也无关闭声明，ABI 边界整改历史不可回溯 | 在 STAR_PSF_ALGORITHMS.md §11.3 补 DISP-PSF-007 行（或显式声明不登记并给理由） | grep DISP-PSF-007 的每个使用面都能在登记表解析到其状态 |
| M8a-I-002 | 台账记行数 10/73/11/33/15（noise 与 stars 已增长），本轮复核同 | 最薄样本仍缺定位/构建 target/偏差登记/状态词四要素，并以 INDEX.yaml 内不存在的 ID 冒充合同引用 | 四要素补进 5 份 README；合同句改用 INDEX.yaml 已定义的 ID | 5 份 README 含四要素且引用的合同 ID 可在 INDEX.yaml 解析 |
| M8a-I-004 | 台账两点均逐字复现 | BSD-3 条款 1 要求的 copyright notice 保留形式缺失；出处 URL 不可寻址 | 修 URL；在 NOTICE 粘贴上游 copyright/许可原文并指到具体文件 | grep -i copyright 至少 1 命中且 URL 可解析 |
| M8a-I-005 | 台账记 25 处命中，本轮 28（并发线增量） | L2 层与 21 份 module.yaml 以仓内不可核的编号标准为字段依据 | 把这些字段依据改挂现行 docs/standards/<NAME>_STANDARD.md，或把编号标准正式入库 | lib/ 内不再出现仓内不可解析的标准编号引用 |
| M8a-I-006 | 台账记 entrypoint: MISSING 16 份，本轮 15 份（并发线有改动） | 同一键名在两义间混用（descriptor 未接线 vs ABI 符号名），仓内无值域定义、机器合同用另一键名 | 在 GLOSSARY 立 entrypoint 词条并给值域；或把 module.yaml 键改名 entrypoint_abi 与 schema 对齐，PUBLIC_API 的 entrypoint=MISSING 改… | module.yaml 的 entrypoint 值全部落在 schema/词典声明的值域内，且自动比对可判真假 |
| M9-E-1 | 单位=度的标准原文由包内 M2b-B-03 取回承担，本轮未自取 PDF | 注册表该行判 CONFORMANT 而偏差列挂的是另一件事（DISP-HIPS-012 FIRSTPIX/LASTPIX），单位口径违规从未获得偏差号；实现与文档同写角秒 | 新开 DISP-HIPS-013（hips_pixel_scale 单位：实现供角秒、IVOA HiPS 要度）并回挂该行；该行判据文字补单位条件 | 注册表 §6.3.1 行状态降 PARTIAL 且 DISP-HIPS-013 可 grep 到；check_standards_registry.py 能对「状态值与偏差列不覆盖本条款已知违规」变红 |
| SA-N-01 | 台账称语料=问题扫描/findings/** 约 296 份；本轮只复核 repair.json 计数未重算语料份数 | 批量回写序列（auto_safe/needs_human/reword/复跑断言）未执行，tracked 档案内锚仍不可解析 | 按 repair.json 的 action 字段分批回写，或由负责人作历史豁免并登记豁免出处 | 复跑 repair 脚本的断言：悬空锚计数=0，或存在带出处的历史豁免声明 |
| SA-N-02 | 台账记旧口径 4 处宿主今为 2 处，本轮复核为 2 | 虚构符号未被清除，历史档案仍以它为凭据，后续读者会据此找不到实现 | 在档案内把这批符号改述为『零命中+检索口径』，或加档案级豁免声明 | grep 虚构符号在 tracked 档案内的命中全部处于『零命中说明』上下文 |
| SA-N-03 | 台账称部分实例已自然消解（module_entry 转正、根级 csv 清运），本轮复核 git ls-files lib/snr_estimator/ 为空 | 自指档案与 run/ 区锚仍留在 tracked 档案内，未换真宿主 | 按 proposed_host 批量回写或作历史豁免；转正类实例从清单移除 | 回写后 A5/B1/B2 计数为 0 或附豁免声明 |
| SA-N-05 | 台账称 run_abi_checks.sh 与 PHASE_OVERVIEW.md 两例活动面引用已消失，本轮复核同（两者 0 命中） | 活动文档的 PASS 凭据指向已删除文件，宣称不可复核 | 改指现行可复跑证据（ci 门 rc + 日志）或删除该 PASS 断言 | grep 活动文档内的证据文件路径全部存在，或断言改为命令+rc |
| V1-N-03 | 台账引 :463-467，本轮落 :461-466（漂移 2 行） | 头文件同一块内散文与字段注释对同名字段给出互斥取值口径；snr_estimate 路径用 1.0、extract_model 路径置 median(SNR_F) | 统一字段语义：或把注释改为『本函数路径置 1.0；extract_model 路径置 median(SNR_F)』并拆名，或在两条路径间收敛取值 | 头文件内每处 snr_phot/median_snr 的取值口径唯一且与两条实现路径一致 |
| V12-N-04 | 台账引第三处文档 PSF_FITTING_VARIANTS.md，本轮复核该文件仍不存在（见 V12-N-14） | 代码与文档共用同一失准常数（相对差 -4.13e-07），且文档未声明分母域（另一自然口径 0.5586098711723649，差 24%） | 按 SCI 层复核后统一为一个全精度值（并声明分母域），文档与代码同源；常数入 CONST_FAMILIES 类机器判据 | 全仓该常数只有唯一全精度写法且与等价 Oracle 复算一致 |
| V15-N-12 | 台账备注所指一次性 evidence/** 归档现整体不在 HEAD（git ls-files evidence/ = 0，R-6 E-26 实测） | 测试名/docstring 与断言内容不符（不校验状态），且证据源指向已删除目录，测试实际不可执行 | 断言改为可复跑的现状证据（module.yaml/ctest rc），或把该测试标为历史并删除；不作为 IMPLEMENTED 凭据 | 该测试可在干净检出运行且断言的语义与 docstring 一致 |
| V2-N-07 | 台账三子项：(b) ipv_wcs:345 已显式失败、(c) 0.6745 相对差已如实登记 ⇒ 仅 (a) 锚失真残留 | 文档『与实现一致』的锚段未包含被引公式的实现点（:621 在 4 段之外），锚不可复核 | 锚改为 path::函数名（star_matcher.cpp::match_psf_to_gaia 等）或补上真实赋值行所属区间 | grep 引用的每段区间都含所述语义（或锚改为符号名） |
| V3-N-01 | 台账记生产者锚 :992-997 → :1011，本轮实测 :1015 | 生产者新增/使用返回码 -14，登记面未同步，错误码通道不可枚举 | PUBLIC_API.md 帧通道补 -14 行（或建立集中枚举） | 文档内帧通道返回码集合与实现 return 值集合一致 |
| V6-N-01 | 与 V9-N-01 同源同判据（不同 producer），保留原 ID 各自落行 | 被删的四行核心 SCI（SCI-PSF-001/SCI-REJ-001/SCI-INT-001/SCI-ACR-EQUIV-001）未恢复，检查器必然红 | 回滚补回被删行（而非改关键词凑子串），并让 check_traceability.py 进 CI 必跑 | check_traceability.py --repo . rc=0 |
| V6-N-04 | 台账记「本轮新增引用为 50 处级」，本轮未重算全量 | FROZEN 文档把不可检出的工作区路径当权威锚，clean checkout 无法复核 | 把该文献评审移入被跟踪面（如 docs/references/**）并改锚，或改锚到已入库的等价文档 | grep -n 'run/' docs/science/CONTROL_WEIGHT_SNR.md 零命中或锚指向 tracked 文件 |
| V8-N-06 | 台账记 CACHE_POLICY 半为真，本轮未否证 | 公共头以不存在的节名当契约锚，按注释检索必失败 | 注释改指实际节名（§4 并发模型 / 不变量段），或给文档补同名节 | grep 注释中每个节名在 GAIA_QUERY.md 命中 |
| V9-N-01 | 与 V6-N-01 同判据；台账备注自述「正确修法是回滚三行而非补关键词」 | 同上（被删核心 SCI 行未回滚） | 回滚三行并复跑 check_traceability.py | check_traceability.py --repo . rc=0 且 rows>=67 |
| V9-N-05 | 台账记 808/36/15；本轮窗口内先 PASS(842/38/0) 后 FAIL(842/38/18) —— 本行按复核末次实测判 OPEN，并如实记录门曾一度清零 | 文档行锚指向的源码文件在迁移窗口内被移动/删除，锚解析面未同步（C2_anchor_resolved 指到 untracked 新路径） | 迁移收口后同提交更新全部 p3_wcs 相关行锚（改 path::符号）并复跑该门；门应纳入 CI 必跑 | check_doc_line_anchors.py rc=0 且 errors 为空 |

### MOD-001（36 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M2a-C-13 | 台账行号与现址相同 :2684；读时 md5=0d12c9323623c5daa7e73cf8d0124c8a | 同族 6 处有 NULL 守卫、本函数漏守卫；README:71 与 docs/contracts/PUBLIC_API.md:78、docs/algorithms/GAIA_QUERY.md 三契约面未声明「句柄不得为… | 函数首行补 if (!client) return 0; 或三契约面同提交声明句柄非 NULL 前置条件 | client=NULL 负例返回确定值且不崩（可复跑）；文档路线则三面同改并过 API 家族一致性检查 |
| M2a-C-4 | 路径已 ARCH-001 迁移 lib/drizzle→lib/algorithms/drizzle；台账锚 :9/:24/:34 与现址一致 | P1-DRZ-IMPL 已落码（src tracked、CMakeLists:61 SHARED、packaging/astrocs.product.json:13 MOD-P1-DRIZZLE status=IMPLE… | README §1 身份表与 module.yaml 的 module_status/entrypoint 同提交订正为实存态（entrypoint=astrocs_module_query_v1）并写清 descrip… | README/module.yaml/CMake target/product.json 四面 module_id+entrypoint+status 一致，且门能吃负例 |
| M2a-C-5 | packaging/astrocs.product.json:12 MOD-CAT-GAIA 亦标 IMPLEMENTED（读时） | CAT-GAIA-IMPL 已建 astrocs_catalog_gaia SHARED target 与 7 项 gaia_cat_* ctest，README:13/:130/:137 与 GAIA_QUERY.md… | 同提交订正 README §1/§8/§9 与 GAIA_QUERY.md §3.1/§4，删除五处失效负向断言 | 文档五断言与 CMakeLists/add_test/gaia_client.c 符号三面对账，能红能绿 |
| M2a-F-1 | pack 内哈希 docs/science/DRIZZLE.md@81e769bdd8e7；本条与 STANDARDS_REGISTRY 现仍 tracked 有关 | docs 点名 TU 在构建图零注册，唯一编译入口用已不存在的旧路径且未接 CI ⇒ 文档证据与可执行面断链 | 把三个 TU 注册进 CMake/ctest，或删除 docs 点名改引已注册的 p1drz_oracle 组（tests/unit/CMakeLists.txt:493 已 add_subdirectory），并同步 … | ctest -N 能列出该 TU；EVIDENCE 指针须出现在 ctest 基线或 discover 之一（能红能绿） |
| M2b-C-02 | module.yaml 已同步（entrypoint 实值），仅 README/PUBLIC_API 残留 | P1-HIPS-IMPL 已建 SHARED target 且产品清单标 IMPLEMENTED，README:24 与 PUBLIC_API.md:350 的 entrypoint=MISSING 仍是迁移前稿，四处自… | 订正 README §1 身份表三断言与 PUBLIC_API 对应段（现状实现=独立 SHARED target astrocs_p1_hips_writer） | README/module.yaml/CMakeLists/product.json 四面 target+status+entrypoint 一致门 |
| M2b-C-03 | 台账命中片段与现址一致（:23） | 同文件旧结论与 RESCUE-V3 更正段并存，:23 明列 aio_hips_reader 不进 DLL 而 :41 实际纳入 ⇒ 按旧注释精简重构会再次产出未解析符号 | 删改 :22-24 旧结论、保留更正段为唯一叙述（或标注已由 RESCUE-FD-01 更正） | 注释与 target_sources 一致；dlopen 冒烟（undefined symbol 检查）能红能绿 |
| M2b-C-05 | 机械信号签名桶 none；台账所指 types.h:35 现址 :35-36 仍同址 | 类型头注释的 ALG ID↔语义映射比 ALG 权威页整体超前/错位一格（002 被写成 variance 而实为叶级信号归一），005 语义被拆名 | 按 HIPS_WRITER.md §2-§5 逐条订正 types.h 注释映射（002 叶级 tile 信号/支撑、003 方差、004 hierarchy、005 MOC/properties/SNR/manifes… | ALG-ID↔语义映射逐条对账脚本（types.h 注释表 vs ALG 页章节标题）能红能绿 |
| M2b-F-01 | path 已迁移 lib/common/healpix→lib/algorithms/shared/healpix；RUBRIC §4 称 registry 已删，实测 docs/standards/STANDARDS_… | 规范面宣称百万点/机器精度级 oracle 对拍，但冻结样本未入库、测试未注册，仓内生成器自标非生产工具且单阶 order 7，测试容差为 1.2×像素角分辨率并跳过极点 ⇒ 声明无证据支撑 | 二选一：入库 oracle 冻结样本并注册 ctest 真跑（多阶/百万点、容差与声明同口径），或把 STANDARDS_REGISTRY:144 与 healpix_core.h:6-7 声明降级为「设计目标，未闭环」… | oracle 样本 tracked + ctest 注册 + 实跑 mismatch=0/往返达标证据进基线；否则声明降级且门能红能绿 |
| M3-C-008 | 台账锚 :563/:720 与现址一致 | descriptor（22 个 astrocs.phaseN.*）与 module.yaml/合同（astrocs.pN.*）两套 module_id 无归一化登记面 ⇒ 任何以 module_id 为键的跨源自动比对必… | MOD-001 建立 module_id 归一映射（descriptor phaseN ↔ manifest pN）并入机读登记（MODULE_MAP 或 ports registry），或统一改名为单一词表 | 跨源 module_id 集合对账门：manifest∩descriptor 覆盖全集，差集即红 |
| M3b-C-02 | 台账锚 :1616/:1663 与现址一致（modules 目录已迁 scheduler） | 生产注册表 star-psf 节点实装第三套检测器（phase1::StarDetector），与 ALG/DATA 宣示的唯一生产源 sdet_api.cpp::sdet_detect_impl 不符；mag 非法值仍… | 节点改调唯一生产源，或把第三套实装登记为显式偏差并同步「唯一生产源」宣称；mag 缺测光改合同 NaN | 注册节点调用图与 ALG 唯一源一致（引用检查）；mag 哨兵负例（缺测光→NaN）测试能红能绿 |
| M4-G-02 | 台账锚 484-501/5451-5457 → 现 :582/:6262-6269；GAP-004 已登记 Session-as-module 编译驻留面 | 整阶段 4 段会话仍以 astrocs.phase2.resample 假身份驻留生产注册表并挂 make_session_module<P2Api> 工厂（P3 侧已有 5 节点拆分先例，P2 未拆） | MOD-001 映射门建立时移除该 descriptor，或以真名+有效端口+单节点 operation 重注（对齐 P3-002 拆分先例） | 23 模块映射门：每 descriptor 映射唯一真实 operation/entrypoint，session-as-module 形态即红 |
| M5b-C-04 | grep -n -e diag -e report include/astrocs/abi/module_api_v1.h 无输出（无诊断回调面） | ABI 头承诺的 double-destroy 报错在 vtable 层不可实现（destroy 返回 void，模块 API 无诊断回调），实现只能静默忽略；并发「原子置位」与普通 int state 相反 | 订正 ABI 文本为「double-destroy 幂等忽略、不回报」或给 destroy 增加返回码/host 诊断通道（升 ABI 版本）；并发语义按实际把 state 原子化或明确单线程 | ABI 文本与 vtable 能力一致；double-destroy 用例行为与文本一致（探针可复跑） |
| M5b-G-06 | 台账「文件名不以 test 开头」已部分修复（tests/abi/test_mod001_install_load_check.py 已建）；sha256 与构建面引用仍未解 | INSTALLED 的「CLI/loader 可发现」缺构建面证据：安全 loader 源码在盘但不在构建图（module_loader 无 CMakeLists、根/子 CMake 零引用），10 unit sha25… | 把 module_loader 纳入构建图并接线 loader 逐 unit 加载验证（tests/abi/test_mod001_install_load_check.py 包装已在盘可被 discover），sha2… | CI 内 loader 逐 unit 加载 + hash 负例（HASH_MISMATCH 拒绝）能红能绿；sha256 非 null |
| M5b-G-18 | 矩阵行号 24（0-based 表头）与台账一致；registry 页 tracked | 占位 descriptor（代码自注 P2 模板复制残留）仍被 registry 页标 production 且写「1/N 等价已验」，追溯矩阵同行 SCI/ALG/SRC/TEST/EVIDENCE 全 MISSING… | 由 P3-RSMP-INT 处理占位 descriptor（移除或改真名/真 operation）；registry 页 status 从 production 降为占位并同步矩阵行 | registry 页 status 与 descriptor 实存一致；追溯矩阵无未解释 MISSING（或显式 DEFERRED） |
| M5b-G-20 | registry 页 26 与台账一致；module.yaml 台账 21 实测 22（glob lib/**/module.yaml） | 门只做 5 条常量对的存在性/子串检查（且仍引 DEFERRED 的 lib/phase3_session），与 22 份 module.yaml、26 页 registry 无任何集合对账，实跑恒绿 | 门改为「lib/**/module.yaml × docs/modules/registry/*.md × descriptor」全量集合对账并加正负例；删除 lib/phase3_session 条目 | 门对缺失 README/缺 module.yaml/悬空 registry 页必红（positive+negative 双例） |
| M6a-I-004 | 台账锚 :139/:143/:196/:241 与文件头声明缺失仍复现；根 CMake 锚由 :374-377 漂至 :399-403 | 遗留分叉无 legacy 声明、含 9 处无条件 stderr、头文件 6 处 CC_EXPORT 默认可见性，而库存清单仍标 classification=production/production_reachable… | 三面统一：或在文件头加 legacy/未编译/公式差异声明并把 inventory classification 改为 legacy，或删除该分叉 | inventory classification 与构建图源清单一致（脚本比对）能红能绿 |
| M6b-E-007 | 台帐「modules/ 残体」prong 已随 ARCH-001 清退消解，另两 prong 仍在；IO 实现已落 lib/infrastructure/aio/io/{fits_core.c,src/io_adapte… | modules/ 残体虽已整树清退，但 IO_001 §2/§3 清单仍指不存在的 README/module.yaml/CMake/io_module_api_v1.h，contracts/data 两文件 doc_r… | IO_001 §2/§3 按现树订正（缺项标未建或补骨架）；contracts/data doc_ref 改指唯一事实源（contracts/schemas + docs/contracts/DATA_SEMANTICS… | doc_ref/路径引用存在性门（悬空即红）能红能绿 |
| M8-E-001 | 路径 lib/hips_p2→lib/algorithms/coverage/hips_p2、lib/phase2→lib/algorithms/coverage（ARCH-001 §1 行 31/33）；行数再漂移 1… | 三事实在新路径原样复现：合同目录零源文件、宿主 astrocs-stage2 仍 EXCLUDE_FROM_ALL 不入默认构建/install、README 行数（1762 vs 1784）与行锚（:103-110 v… | 追溯宿主字段统一写 target 名并符号化锚（禁裸行号）；README 实测行数/行锚同提交订正；零源目录登记为占位或补实现 | README 行锚解析门 + 追溯宿主指向可解析 target（能红能绿） |
| M8a-C-002 | calibration README:163 与 module.yaml:21 直接互斥；cosmetic 台账锚 :9 与现址一致 | 三处 README 状态句停留在迁移前（未建/尚无源码/entrypoint=MISSING），与 src 实存、CMakeLists SHARED target、module.yaml entrypoint 实值相反 | 三 README 同提交订正为实存状态，并同步 docs/contracts/PUBLIC_API.md 对应段 | README 状态句 ↔ CMake target ↔ module.yaml ↔ product.json 四面一致门 |
| M8a-C-005 | 台账报 21/22，现树实测 22/22（新增 lib/infrastructure/cli/module.yaml 等） | manifest 词表（astrocs.pN.*/catalog.gaia/infrastructure.cli）与 descriptor 词表（astrocs.phaseN.*）精确交集 0，缺归一化登记面 | 建立 module_id 归一映射（机读）或统一命名，MOD-001 映射门以此为跨源主键 | manifest∩descriptor 覆盖全集（差集即红）能红能绿 |
| M8a-C-008 | path providers/cpu/common→lib/infrastructure/benchmark/cpu/common（ARCH-001）；台账所写消费方文件为旧路径，现为 tests/cpu/**:68 | README §5 公共符号表漏登头文件实有导出 acs_cap_classify_v1（消费方 tests/cpu/dispatch/cpu_capability_matrix_test.c:68 已用），且模块文档状… | 补登该导出（含单位/所有权），状态词改阶梯内词（IMPLEMENTED/CONTRACT_READY） | 头文件导出 ↔ README 符号表族对账门 + 状态词白名单检查（能红能绿） |
| M8a-E-001 | 路径 lib/phase2→lib/algorithms/coverage（ARCH-001）；descriptor 行号 :874 与台账一致 | coverage 域 L2 三面（README:174/module.yaml:32/descriptor:874）引用未登记的 ALG-P2-COV-001，L1 权威登记为 ALG-COV-001 ⇒ 按 alg_i… | 三处改引 ALG-COV-001，或把 ALG-P2-COV-001 登记入 contracts/INDEX.yaml 并与 ALG-COV-001 显式互引 | 算法引用有效性门：代码/文档引用的 ALG/SCI ID 必须命中 docs/contracts/INDEX.yaml |
| M8a-E-004 | 台账「cosmetic README:7-8」现址 :8；CMakeLists.txt:321-333 锚亦已漂（现 :399-403） | ARCH-001 迁移后跨目录引用未改仓根相对全路径、行锚用裸行号，导致四处悬空/错位（含 4 处 ARCH_CONTRACTS.md 标 VERIFIED 的不存在文档） | docs 引用统一仓根相对全路径 + 符号锚；补建或删除 ARCH_CONTRACTS.md 并改写 4 处 VERIFIED 行；cosmetic README 改指真身路径 | 文档路径/行锚解析门（引用存在性+锚命中）能红能绿 |
| M8a-G-006 | lib 与 modules 内 README 实测 51 份、module.yaml 22 份，门实覆 5 份 | 门实覆 5 份 README（常量硬编码）且对缺失/错锚无分辨力，仍放行 wcs README 的 P1-004 错锚（注册表内不存在） | 门改为全量集合对账 + 正负例；删除 lib/phase3_session 条目（DEFERRED） | 门对缺失 README/悬空注册项必红（positive+negative 双例） |
| M8a-G-007 | 台账锚 :134-143 现为 :142 open(p,"w")；registry 页 26 与台账一致 | 生成器声明与事实相反（无 checker 以其输出为源）、双面 0 接线（CI/pre-commit 均无）、且以 descriptor 为唯一源无条件覆盖 26 页 registry；26 页 status 全为阶梯外… | 订正 docstring 或真正接入 CI 作 diff 门（生成物与在盘页不一致即红）；registry status 改阶梯内词或显式标注 GENERATED | CI diff 门 + 生成器/id 一致性（能红能绿） |
| V11-N-05 | path runtime/providers/cli 多数已退役；lib/cli 面仍零命中 | 6 个 ABI 协商/自检函数生产面零实现零调用，唯一定义在探针自带副本 ⇒ 断言对象是测试自证，门在自证 | 把协商/自检函数落到生产 loader/registry（或明确其为测试探针并改判据对象），探针改为调用生产实现 | 生产面存在定义+调用（生产路径 grep 非零）；探针不再自带副本 |
| V12-N-11 | ipv 触顶 fmod 判据已修（台账备注）；本条只报常量三份 | 同一上限以「文件私有 #define + 两处独立字面量」三份并存，缓存预算乘积在 gaia_client.c:143 与 module_entry.c:546 各算一遍，无头级单点权威 ⇒ 改一处不会同步其余 | 把上限提为头级常量（含单位/语义注释）并让三处引用同一点；ipv 的 m_lim_gaia_cap_per_file 显式写映射说明 | 常量单点门：同值字面量只允许出现在权威头；跨模块引用检查能红能绿 |
| V14-N-02 | 路径 modules/conformance/noop→tests/conformance/noop；台账锚 README:74-78 与现 :76 一致 | 骨架 NOOP 自身三处 SKELETON（module.yaml/README/CMake）而产品清单与安装树合同登记 IMPLEMENTED；真正实现动词的 echo（module_status: IMPLEMENT… | 产品清单 MOD-NOOP status 改 SKELETON（或按 §11.3 负向词如实标注）；按需把 echo 登记入清单 | module.yaml module_status ↔ 产品清单 status 一致门；清单 unit 与 module_status 集合对账 |
| V14-N-04 | path lib/backend_host→lib/infrastructure/benchmark/backend_host、providers/cpu/baseline→lib/infrastructure/benc… | 出厂 kernel_id/alg_id（ALG-004）在 docs 登记面零存在，且 ALG-002 被 star-psf/wcs/photometry 三模块共用 ⇒ 按 kernel_id/alg_id 追溯必断链 | 把 ALG-004 登记入 INDEX.yaml（或改引 ALG-NOISE-001..003）；ALG-002 拆分或显式登记共享映射 | kernel_id/alg_id 与 contracts INDEX 双向对账门（悬空即红）能红能绿 |
| V14-N-07 | path lib/snr_estimator→lib/algorithms/noise_snr（ARCH-001 + MOD-001 迁移） | header 导出与 module.yaml source_symbols 两登记面互不校验，四枚新导出零登记而字段头注自称源码核对清单 ⇒ 自称与源码不符 | 补登 module.yaml source_symbols（或删该字段改用机器提取），MOD-001 建立三面（header/yaml/合同视图）一致性门 | 头文件导出集合 ⊆ module.yaml source_symbols，差集即红 |
| V14-N-08 | 与 M8a-G-007 同一对象两行，本行聚焦「覆盖写+零接线」 | 生成器对 registry 页无条件 open(p,"w") 覆盖，且 ci/checks.json 与 .github 双面 0 接线（既不进 CI 也不进 pre-commit）⇒ 人工修订无保护、占位 ID 可被钉… | 生成器加人工区保护或改为生成到临时目录 + diff 门；接入 ci/checks.json | CI diff 门：生成物与在盘页不一致即红（能红能绿） |
| V20-N-07 | 根 CMakeLists -Wall -Wextra 白名单仍不含 cal/cos/drizzle/hips（其 CMakeLists 只加 -fopenmp）；文件总行数 1496 | build_out_manifest 声明 acs_error_info_v1* err 却永不填充（函数体仅签名一处 err、唯一出口 ACS_OK）；cal_describe 未 (void)self 而同族其余 3… | build_out_manifest 失败分支补 efill(err,...) 或删该出参；cal_describe 补 (void)self 或使用 self；what 写进诊断 detail | -Wall -Wextra（含未使用参数告警）白名单覆盖 cal/cos/drizzle/hips；err 非空负例测试 |
| V21-N-04 | path modules/conformance/echo→tests/conformance/echo；台账行号 [350,351,353]/[368,369,370] 与现址逐字一致 | aerr/rerr 三态（声明/memset/传址）后从无成员访问，宿主返回的真实 detail_code 被 efill 静态消息覆盖 ⇒ 统一诊断传播在 conformance 面未落地 | 失败分支读取 aerr/rerr 成员并透传（或删出参）；诊断消息携带真实 detail_code | 诊断传播负例：artifact_open/read_all 失败时 err 携带宿主 detail_code（探针可复跑） |
| V21-N-13 | 台账锚 :2843-2846 现址 :2844 相近；max_forward_cross_deg 数值本身在判定处有绝对门（台账限定保留） | 14 个 manifest 键全为首次写入后无人回读（nside_clamped 触顶无告警、snr_schema 与产物内 schema/schema_version 两键并存无人比对、product_sha256 只… | 为每个自述键指定消费者与告警路径（或删无消费者的键）；nside_clamped 触顶据此告警，snr_schema 与产物内 schema 键统一 | manifest 键读写对账门：每键至少一处回读/消费（差集即红）能红能绿 |
| V5-N-01 | 台账锚 :786/:788/:825-828 与现址一致；inspect 同型守卫 :1004-1011 仍在 | catalog_dir 的长度预算（manifest head[512]）只在 execute 内检查，validate_config/plan/create 仅调 gaia_cfg_parse 无预算检查 ⇒ 同一参数… | 把路径长度预算前移到 validate_config（fail-fast），plan/create 复用同一校验；inspect 出口同型守卫收敛到单点 | 超长 catalog_dir 在 validate 即被拒（负例可复跑）；三相同一错误码与诊断 |
| W1-N-03 | 文件已 tracked 并迁至 lib/algorithms/noise_snr（台账称 untracked 已不成立）；module_entry.cpp 1574 行量级未复算 | 子目录注释与在案 F-CI-002-01 裁决相反（根图注册被注释），tests/unit 门卫因 target 不在根图恒假 ⇒ 适配器代码零编译零测试零 CI 采集 | 订正 CMakeLists:26 注释为「未注册（F-CI-002-01 摘出，待 V7 残留收编）」；收编后恢复注册并让门卫真跑（或在未注册期显式标 SKIP 并计数） | 注释与根构建图一致；门卫 target 存在性检查能红能绿 |

### DATA-001（33 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| W5-N-15 | - | 缺值/解析失败/合法值三者不可辨（UNIFIED_MODEL §2 字段一义） | 引入 has_*/invalid 判别位或 NaN 哨兵，分离哨兵与合法值域 | 缺键、非法值、合法 0.0 三者可区分的用例 |
| M1a-G-002 | - | API-P3-001 文档与实现停在单投影 TAN，且引用不存在的 request schema，未随最高设计 §5.3 八投影更新 | 按 §5.3 实现/声明八投影并补 contracts/schemas/phase3_request_v1.schema.json（或退役该引用） | contracts 索引含该 schema，API 文档与 AST/实现一致（tests/api/test_p3_api.py 由 assertIn 文本断言升级为结构断言） |
| M2a-A-3 | - | 匹配语义（平手裁决/半径开闭/同星去重）未进 ALG 权威与 DATA 合同，只在实现里 | ALG-GAIA-001 补匹配/平手/半径开闭条款，DATA_SEMANTICS §8.2 补 out_match_idx 语义面 | 平手输入双跑 bitwise 一致 + 合同字段逐列登记门 |
| M2a-C-10 | 探针须先 json.load 再 compile：schema 原文含双反斜杠转义，直接拿 raw 文本当日志会得出全 False 的假信号 | 注册表词法与两份 schema 的 type_id pattern 不兼容，该类型一经使用必被自家校验器以 lexical invalid 拒绝 | 统一 type_id 词法（扩 pattern 允许三段式或改 ID 为四段），同步两处 schema | validator/checker 对全部注册 type_id 恒过一个正例（含 calibrated_frame.v1） |
| M2a-C-12 | docs/contracts/DATA_SEMANTICS.md 读时 hash=26a313aeba17（包内=af6883eeb9b3，并发线在改，行号已漂移） | 文档仍写 UB 面失实（代码已 calloc 置 0），且 cone 行 schema 下发恒 0 列却无 invalid 表达 | DATA_SEMANTICS/README 改为"显式置 0"，或在 schema 给四列 invalid/常量语义 | 文档与 calloc 实现一致 + 恒 0 列在合同中有 invalid 表达的门 |
| M2a-C-14 | - | 两合同各维护一份执行形态（schema/validator），同字段约束未同步 | storage_uri 单一定义；_TYPE_ID_RE 抽公共常量；校验器补 maximum 与 64hex/URI 约束 | 两 schema 字段与约束逐项一致门，且校验器对越界输入能红 |
| M2a-C-9 | - | 自写目录解析器不校验长度/偏移字段，坏文件可越界读 | 校验 header_len/root_pos/node_count 落在映射区间内并 fail-closed，child 索引与 node_count 比对 | 新增"长度字段撒谎/偏移越界/child 自指"负例（当前 negative 用例把静默跳过钉成期望） |
| M2a-E-5 | - | 合同按裸函数名引用测试，实际文件以 class TestNoImplicitNameBinding 组织，名字已不存在 | DATA-002 改引真实用例定位（file::class::method）或补同名用例 | CHK-CONTRACT-TEST 按名解析不再断链（能红能绿） |
| M2a-G-2 | - | 登记规范两式（带/不带 -NNN）与门正则单一式不一致，门给绿属假覆盖 | 放宽正则或按登记规范两式并采，并同步 DATA_ARTIFACTS:125 措辞 | 门对未登记 ID 必红（注入用例），覆盖 DATA_SEMANTICS 全部声明 ID |
| M2a-H-2 | OPEN_ITEMS.md 无 A-11 登记项，R-1..R-7 亦无该裁决 ⇒ 裁决仍缺，不能据"SCI-INT 认可"注释判 FIXED | UNIFIED_MODEL §2 要求 validity 是门、variance 是估计量；实现用 variance 值兼作门，被丢像素信号/覆盖/nContrib/source_pixels 全不计 | 待负责人 A-11 裁决语义后，统一实现与头注释/DATA/ALG 三处文本（或改为 validity 位 + 方差保留） | variance≤0 像素的 validity 计数与信号/覆盖口径一致的用例；三处文本与实现一致 |
| M2a-I-2 | - | 确定性标记是上层唯一读取口径，却与同模块算法文档自斥（行序依赖目录枚举） | 或修实现使输出行序确定（显式排序），或降级 manifest 标记并在 ALG/合同登记偏差 | 同输入双跑 bitwise 对拍门；manifest 声明与 ALG 文本一致 |
| M2b-C-04 | docs/standards/STANDARDS_REGISTRY.md 读时 hash=14217bb06978（包内=c026817d6f93，并发线在改）；RUBRIC §4 把该文件列为"已删"，实测仍存在 37… | 读/写/校验三层值域与测试夹具不统一；注册表以 PR 草案为基线而非 REC，且四层无单一事实源 | 以 IVOA HiPS REC 为基线统一 hips_version 值域（写 1.4、读校验 1.4、夹具 1.4），注册表登记实现偏差 | 写→读→校验闭环用例 + 注册表与实现一致性门（含夹具） |
| M2b-H-02 | - | 越界哨兵取合法天球点 (0,0)，调用方无法区分越界与真实坐标；nested_local shift 夹紧仍在 | 越界改返回状态/NaN 或输出 validity 位，头注释同步 | 越界输入与合法 (0°,0°) 可区分的用例 |
| M3b-A-02 | docs/contracts/DATA_SEMANTICS.md 读时 hash=26a313aeba17（包内=af6883eeb9b3） | 一个字段承载两义（UNIFIED_MODEL §2 / ENGINEERING_SPEC §3 禁），仅以偏差登记未拆列 | 拆分列名/加来源标签，或统一两链母函数与量 | 跨链同名列可比性或显式分列的门；DISP-STAR-007 关闭 |
| M3b-C-04 | docs/algorithms/STAR_PSF_ALGORITHMS.md 读时 hash=2a8af20a6d7f（包内=6c84e26b1cb9，并发线在改） | A/B 命名在两合同反向且交叉引用互错；ALG 布局 A 声明的 residual_scale/q_psf 在批缓冲中实为 fwhm_x/fwhm_y | 按语义重命名布局（去 A/B）并统一交叉引用与列序 | 两文档列序与 dpsf_psf.cpp 逐列一致门 |
| M7-A-119 | - | F_instr 单量双单位标签（ENGINEERING_SPEC §3 禁同一量两单位标签） | 定 F_instr 唯一量纲（是否已除孔径面积）并全局改文，回写引用它的 DATA/ALG | docs/science 单位与 DATA/ALG 引用一致的检查项 |
| M7-A-123 | docs/contracts/DATA_SEMANTICS.md 读时 hash=26a313aeba17（包内=af6883eeb9b3） | mask 权威定义域未钉：P1 坏点掩膜 / P2 rejection accepted / P3 coverage 派生三义互斥且均声明不外销，交换层却强制要求 | 先钉 mask 权威语义域，再统一 schema/validator 与 DATA_SEMANTICS §9.4/§10.3 | 合同平面集与 DATA 合同逐 phase 一致门 |
| M7-C-101 | - | ASTROCS_DESIGN §9 要求 manifest 至少记录算法 ID，但 schema 白名单与正则只有 SCI-* 槽位 | producer 增算法/测试合同字段，或明确以 module_build_id 反查为唯一口径并同步 §9 表述 | 产品级溯源可定位算法 ID 的用例；schema 与 validator 白名单一致 |
| M7-I-202 | 按前台裁决不写"双向锁死"，只判措辞/值域不一致 | 三处值域未单一事实源化（措辞/值域不一致，不构成"互操作锁死"） | 钉 hips_frame 权威值域并同步三处（或在 IO-002 显式登记 icrs 别名） | 三处值域一致性门 + icrs 输入行为用例 |
| M7-I-203 | docs/contracts/DATA_SEMANTICS.md 读时 hash=26a313aeba17（包内=af6883eeb9b3） | coverage 为整型 tile 计数（无量纲）却沿用能量单位标签，且以"口径透传"方式指向 DATA-COV-001 §19 | 改为无量纲/计数标签并同步 DATA-COV-001 §19 引用 | coverage BUNIT/单位与数据类型一致门 |
| M7-I-204 | docs/contracts/DATA_SEMANTICS.md 读时 hash=26a313aeba17（包内=af6883eeb9b3） | 同一 u8 reason 两套互斥值域且两节均自称权威，未单一事实源 | 定 reason 唯一值域表并在 §22.2/§23.3 互引，同步实现枚举 | 文档值域与实现枚举（P2RejectReason）逐值比对门 |
| M8-B-001 | tests/unit/core_artifact_test.cpp:65-72 用同一工厂构造两对象，对混入零鉴别力 | 双实现（C++ FNV-1a 16hex vs runtime sha256）口径不一，C++ 侧摘要含运行事实，与 DATA-004 §2 冲突 | C++ science_hash 去运行事实，或退役该实现只留 runtime 合规实现 | 同一 payload 跨实现 digest 一致 + 改 created_utc 不改 digest 的用例 |
| M9-A-3 | 当前无错算，属接口定义风险（台账原判 P2 一致） | 三处同名同返回类型仅第 4 参差 3600×，无静态断言/包装类型；消费面已按度传 half_fov | 统一单位或引入角度强类型/单位后缀命名 | 三处调用点单位一致门（含 browser_backend 传 half_fov 度） |
| M9-C-2 | 台账记 102 行，本轮复算 382 行、units 去重 1（与包内备注一致） | 角秒/像素只在实现尾乘 3600 隐含，名与注释均无单位，合同表未登记该 API | 函数改名带 _arcsec 或头内注明单位，并登记 API_CONTRACTS.csv 各行真实 units/valid_range | API 合同表单位列逐函数非样板 + 与实现一致门 |
| M9-D-2 | - | 同一头双单位无适用面说明，量化常量（2µas/10µas LSB）未回写代码与表 | 补单位注释/统一命名，并把 GAIA_QUERY.md §2.9 的 2µas/10µas 口径回写实现注释 | 头文件单位声明与 GAIA_QUERY.md §2.9 一致门 |
| V1-N-05 | module_adapters.cpp 并发线在改：读时 hash=3ac7b6514ae2，复核时=d167008bb60e（行号随移，判据内容逐条复核不变） | 端口标签与生产者单位不一致，须以 DATA 合同面收敛 | 钉 DATA-P1-FLUX 权威单位并同步 adapters 与两页 registry | 端口单位与生产者/合同一致门（非同标即过） |
| V11-N-04 | gate2_psf_oracle.py 正是 PSF 质心门量具，须与 R-3 裁决一并复核 | Python 镜像未随 C 原型更新；两镜像亦无布局/签名锁 | 两处镜像补第 9 参并按 ipv_abi_mirror 模式加 C 探针+镜像布局锁 | C 探针与 Python 镜像逐参/逐字段对账门（能红能绿） |
| V12-N-12 | 生产 336+342×2=1020nm vs fixture 336+342×1=678nm（跨度为真库 1/2） | 三元组无权威登记处，fixture 与真库分叉且被合成值断言锁死 | 登记唯一 (start,step,count) 权威，修 fixture 与断言 | fixture 元数据与真库/合同一致性门 |
| V2-N-02 | 与台账差异：工作树现已 FAIL（非"PASS 假绿"）—— 假绿面消失但门红面扩大到 145 条 | API_CONTRACTS.csv 与 AST 长期漂移（145 条），已删符号仍记 VERIFIED、新符号未登记 | 按 AST 重算 CSV（删已删符号、登记新符号）并修扫描面（是否含 run/** 需定论） | CON-API-CONTRACTS 在干净检出 rc=0 且能红能绿（注入用例） |
| V7-N-03 | module_adapters.cpp 并发线在改：读时 hash=3ac7b6514ae2、复核时=d167008bb60e；weight_mode 零点仍在（复核时 :4670 为 value(weight_mode… | 同键多缺省 + 兜底把上游非法声明变合法缺省（ASTROCS_DESIGN §9 禁把计划值伪装成实际值） | 统一 target_order/weight_mode 缺省语义并在两入口一致 fail-closed | 缺键/非法值输入在两入口的拒绝行为一致用例 |
| V9-N-04 | - | 合同状态字与 AST 未联动，删除/重命名留下悬空记录（ENGINEERING_SPEC §8） | 按 AST 重算状态，或引入"符号必须存在于 AST"的可失败状态门 | 删除符号后 CSV 必红（注入用例）；干净检出 rc=0 |
| W2-N-02 | - | "增益未知"在类型上不可表达，与 snr_science 的 gain<=0=未知 形成同帧双通道分叉 | 加 has_gain 判别位，缺键/非法值走 explicit unavailable | 缺 GAIN 与非法 GAIN 可区分的用例，且与 snr_science 口径一致 |
| W2-N-03 | module_adapters.cpp 并发线在改：读时 hash=3ac7b6514ae2，复核时=d167008bb60e（行号随移，判据内容逐条复核不变） | 端口标签与载荷不符，标签经 artifact.cpp/artifact_store.cpp 外销 | 以生产者/合同为准统一标签（ADU）或改载荷单位 | 端口标签与生产者声明可核的一致性门（禁自我印证） |

### P2-001（31 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M2a-F-4 | 行号 464 未漂；「16 处 make_cfg」与台账一致 | pixfrac 扫描只覆盖拒绝域；pixfrac=1.0 使 1/pixfrac² 因子恒为 1，偏亮缺陷不可见 | p1drz_properties 增 (0,1) 开区间正面扫描（0.3/0.5/0.8）+ 解析 Oracle 比对 | ctest -R p1drz_properties 中存在 pf∈(0,1) 且非 1.0 的断言并能对偏亮注入判红 |
| M2a-F-5 | 该文件未在任何 CMake/名单注册（grep test_drizzle_oracle 仅命中问题扫描/**） | Oracle 只驱动 planar OVERLAP/ACCUM/NORMALIZE，球面链零用例；ARCH-001 迁移后 HOST 常量未更新 ⇒ 该 Oracle 已不可构建 | 修 HOST 常量并补球面不变量用例（nside/ipix/旋转） | 球面链 grep 命中>0 且该 Oracle 能在 CI 复跑判红 |
| M2b-F-02 | 行号 89→90、94-96→97（内容不变） | 断言主体非被检对象（恒真析取）；确定性证据为同路径重复读 | 去 // true、键集改合取、hash 断言改为冻结期望值 | 删除任一产品键后该测试判红 |
| M2b-F-03 | p1hips_tests_properties.cpp:228 双侧 std::to_string 比较已修（:231 注释「修复前 …必然分叉」，改共用格式化 + i6 断言） | 真值夹具与生产模板不同构；断言只看根元素（非法命名空间同样通过） | 夹具改用与生产同构内容；断言改 XML 结构解析 | 非法命名空间/缺 xmlns 时判红 |
| M4-A-04 | REJECTION.md 读时哈希 3cb03c033f15、PHASE2_REJECTION.md=4b8c898a3acc（包内 e728a92cb3b9/fde33e3de0b4） | SCI §5 只定义 7 方法，实现多出 3 个可选枚举且 config 无背书 | SCI 补 3 方法的统计定义，或把 3 方法标 legacy/退役 | SCI 方法集与 rejection.h 枚举逐字可对照 |
| M4-A-05 | - | 接缝判据的 SCI 来源缺失，阈值自证于测试文件 | 在 SCI/ALG 定义接缝指标与阈值，测试改引用冻结常量 | SCI 含法向/support 连续性条款且测试阈值可回溯到该条款 |
| M4-A-06 | 行号 1040-1046 | SCI §4 与 §5 自斥；实现走 rc=1 而非 P2_STATUS_INVALID_CONFIGURATION（:1745/:1752 只覆盖 method×normalization） | 统一 SCI 合法集；非法 profile 通道改用 P2_STATUS_INVALID_CONFIGURATION | 非法 profile 的返回状态 == SCI 承诺值（新增门判红） |
| M4-C-04 | upm.cpp 读时哈希 304185bf462b（包内 4051321cde7b） | 生产装配无自由度（λs=0）也无全几何节点入口 ⇒ 延拓路径不可达 | 会话装配改走 p2_upm_build_geo，或把 SCI 延拓条款标为未启用路径并登记 | 单帧区产物与 SCI 延拓声明一致（新增门） |
| M4-C-07 | 「1762 行实测」已过期（现 1784 行） | PUBLIC_API.md :54 自斥已修，但 ALG slice 的「唯一生产源」表述与真实生产 DAG（module_adapters p2_op_*）仍不一致，且行数实测过期 | 把 stage2.cpp 标为工具/legacy 入口，生产源锚改指 p2_op_*；同步行数 | 四份 slice ALG 的生产源锚与 module_adapters 逐一对上 |
| M4-C-09 | 原 :890 现为 model_hash 区，真实 frames 校验在 :1069-1081 一带 | 错误码 ID 无源码落点；容器接受面与打开器接受面不对称且未登记 | 把 ERR-P2-UPM-001 落到真实校验点；ALG §10/§15 登记接受面不对称 | 该 ID 在源码可定位且 ALG 表列双格式接受策略 |
| M4-D-03 | 台账点名的 upm.cpp「B4-27」与「此前 huber_delta」已清零；残余落在另 5 个 coverage 文件 | 注释仍以任务号/变更流水作锚 | 改为契约/条款引用 | 活动源码与文档中任务号 token 归零 |
| M4-F-03 | 行号 125（与台账一致） | 测试只链接 contracts、无 astro/phase2 头，断言为空还打印 PASS | 链接 astrocs_phase2 并写真实不变量断言，或改名为框架自检 | 注入 zero-support 生成值后该门判红 |
| M4-F-04 | - | 跨 worker 数值等价从未被比较；跳过条件使该测试常态不跑 | 改用真实构建产物路径、断言 csum，并把 SKIP 纳入 CI 白名单 | 1T/2T calibrate 数值等价被真实比较并能判红 |
| M4-F-07 | 行号 89/136 与台账一致 | 占位断言充当证据 | 用真实不变量替换 CHECK(true)（枚举数值序、全零求和等） | 删除任一被检行为后对应断言判红 |
| M4-F-08 | - | 测试面走 compat 结构体绕开生产 strided 路径；frame_ids 与资格压缩后的样本错位 | Oracle 改走生产接口；frame_ids 随资格掩码同步压缩 | 构造含不合格样本的栈，断言 frame_ids 与 values 同源同位 |
| M4-H-01 | 行号 425→434 | 用 operator[] 隐式建键，无 rc 无告警，污染归一化/gauge/C 行 | 改 find + 未命中显式 rc | 注入未知 control_id ⇒ 显式失败（新增负例判红） |
| M6a-D-014 | 行号 157→165、683→701、689-690→711-712、881→900 | 注释未随实现收敛，读者无法判断该锁是必留约束还是待清除临时物 | 统一并发模型注释：删「禁全局 critical」或把 g_aio_mu 冻结并登记 | 同文件无自斥并发叙述（review 门） |
| M6b-C-002 | 读时哈希 PHASE2_INTEGRATION.md=a1e93858f263（包内 b6f8a4b81d43） | 过期登记未清，逐符号锚整体错位 | 删 :42 括注、§11.3 标 CLOSED 并清逐符号锚 | ALG 内无「现状=缺陷」与「已闭环」并存 |
| M7-A-108 | §8a 已由 R-2 M4-A-01 根因登记落地 | 方法核判据带未定义 tie/边界含入，门无法证伪 | SCI §5 补 quantile 型/tie/边界含入；按 §8a 另立缺陷裁决 scale | n=3/4 边界栈的判定可由 SCI 文本唯一推出 |
| M7-A-110 | REJECTION.md 读时哈希 3cb03c033f15（包内 e728a92cb3b9） | 尺度取 /median/ 无噪声项与下限，跨零不连续 | 按 §8a 裁决改用 max(/median/, MAD) 或显式下限并重标定 | 近零天光栈的全拒率门（现 73.9%/70.1%）判红 |
| M7-A-126 | 行号 1436-1439→1451-1452；rejection.cpp 读时哈希 41d63fe62a50（包内 9a281a3fb6dd） | σ 未按估计量闭式换算，文档与代码互斥，拒收率随法漂移 | 或把 σ 乘 √(π/2)（与 averaged_sigma 同口径），或改文档为「平均绝对残差」并重标定阈值 | 高斯栈下 linear_fit 的实际等效 σ 阈值与文档一致（可判红） |
| M7-A-127 | REJECTION_ALGORITHMS.md 读时哈希 4bd56755b392（包内 4f65c3f4d00f） | 统计方法的适用域未声明，小 n 无降级 | 补 n≥15/25 适用域与 n<15 路由（或标近似失效） | n<15 走登记降级路径（新增门） |
| M7-A-136 | PHASE2_REJECTION.md 读时哈希 4b8c898a3acc（包内 fde33e3de0b4） | 尺度无噪声项，与背景绝对电平耦合；N=4 全拒兜底是其症状 | 按 §8a 裁决改用 max(/median/, MAD) 并重标定 | 背景电平扫描下拒收率恒定（门可判红） |
| M7-A-214 | INTEGRATION.md 读时哈希 92ac00e963d6（包内 c69090065bf3） | 门对实现无判别力或阻碍正当实现 | 常量场门改 rtol 1e-12（与 :106 统一） | 等权 3×0.1 常量场通过且注入真实偏差判红 |
| M7-H-101 | ① 已由 R-2 M7-H-101 建议落地（新增 p2_upm_convergence），残留仅为 ④ | 伪代码符号名与实现不一致（:168 赋值的是 s0） | 把 :169 的 s1 改 s0（或补 s1 定义）；① 已修无需再动 | 伪代码符号可在全文唯一解析 |
| M8-F-010 | 行号漂移：hp_drizzle_api 读键 :946-949、拒绝语义 drizzle_engine.cpp:1053-1056 | 上游 module_adapters 已发射该 KV，生产可达但无门 | p1drz_negative 补「声明=1 放行 + provenance 降级」正例与「缺键/=0 拒绝」反例 | 两侧行为各 1 例且能判红 |
| M9-F-2 | 五套并存数值仍在：tests/io/make_hips_fixture.py:169、lib/algorithms/coverage/tools/controlled_rejection_truth.py:52、hips… | Oracle 与被检实现共用同一表达式/单位/精度，属自证门 | Oracle 改由 IVOA HiPS 1.0 的 unit 定义独立推值（或改为度单位对拍） | 注入单位/量纲错（如误用 arcsec）后该门判红 |
| V20-N-11 | 行号 2675-2676/:2787 与台账一致；module_adapters.cpp 读时哈希 713b9ba2bbfb（包内 10b472f2719e） | 非法值与「不限」「0」共用一个编码，无 provenance 判别位 | 三态分离（-1 显式拒绝或加 snr_max_sources_source 判别位） | 三态在产物中可区分（新增门判红） |
| V3-N-03 | module_adapters.cpp 读时哈希 713b9ba2bbfb（包内 10b472f2719e） | 读侧硬门与装配侧静默默认并存，跨版本面可能把不匹配模型当匹配 | fit 入口缺键改显式失败（或在 manifest 记 grid 来源） | 缺键时显式拒绝（新增负例） |
| W1-N-12 | finding 记 2 处，实测 3 处（与台账复核一致） | 常数无唯一承载头（文档权威点 docs/algorithms/DRIZZLE_GEOMETRY.md:87，未改） | 抽到公共头并让三处引用 | 全仓该常数定义唯一（grep 计数==1） |
| W2-N-06 | drizzle_engine.cpp 读时哈希 0a838c49ebca（包内 80b3c471d51f） | 跳过条件与合同不同集（缺 isfinite），分子分母判据不一致 | 两处统一为 `!(v>0) // !isfinite(v)` 并同集处理 | 注入 NaN variance 后交付 variance 与解析值一致（门判红） |

### AIO-001（24 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M1a-G-001 | 与 RUBRIC §4 假设不同：lib/phase3_session 虽在 ARCH-001 列 DEFERRED(不迁移,删除)，但根 CMakeLists:615-620 仍编入 astrocs_phase3_se… | 产品 manifest 落盘面仍只有 CUNIT1=deg/BUNIT，进程内端口描述符（UnitId::SURFACE_BRIGHTNESS+CoordinateFrame::PIXEL）未随产品落盘，最高设计 §9 … | p3_output/p3_session 的产品 manifest/inspect 补写坐标 frame 与像素/采样语义键，并与端口描述符同源 | 新用例断言 p3 产品 manifest/result JSON 含 RADESYS(EQUINOX) 与采样语义键且与端口描述符一致 |
| M2a-C-11 | - | 实现侧已落地（flags 32/64 + int32 诊断平面 + ASTROCS_* 五键），合同 JSON 仍记 PENDING 且无 resolution 回登 | 在合同 JSON 两通道补 resolution(status=IMPLEMENTED+证据路径)，并订正 gap 文本 | 合同登记与 aio_hips.h/aio_hips_writer.cpp 通道逐项一致门（含 findings_transfer 同款 IMPLEMENTED 惯例） |
| M2a-E-4 | - | 合同声明与构建图不一致：声明的 HiPS C ABI 无生产 target，生产读侧走 astrocs_hips/aio_hips_reader.cpp | 把 hips_core.c 登记进 astrocs_io（或改合同归属为实际 target） | 契约声明的符号可在构建产物中定位的门（io 契约测试接生产 target 而非自建 .so） |
| M2b-C-01 | module_adapters.cpp 并发线在改：读时 hash=3ac7b6514ae2，复核时=d167008bb60e（行号随移，判据内容逐条复核不变） | properties 已走 aio_atomic::write_file_atomic，但 tile 级 FITS 仍 remove+create 直写、make_dirs 无 fsync；调用点注释宣称的"原子发布内建… | tile 写路径复用 aio_atomic_file.h（临时+fsync+rename），或按 §12.5 明示 writer 不原子并改写调用点注释 | DISP-HIPS-004 关闭：注入中途失败不残留可误认的正式产品；文档/注释与实现一致 |
| M2b-C-06 | - | writer 新增 manifest 诊断计数键未回写 ALG 字段清单，§9 I8"计数字段=实际文件数"表述未覆盖诊断通道 | HIPS_WRITER §5(5d)/§9 I8 登记 nrej_tiles/nused_tiles 及取数口径 | ALG 字段清单与 writer 实际键集逐项比对门（能红能绿） |
| M8a-C-003 | - | README 停留迁移前快照（依赖/线程/目录树/构建段），与同目录 vendored cfitsio 与现行 src/ 树矛盾 | 按现行 src/ 树与 vendored cfitsio 重写 README（依赖、线程、构建、版本快照） | README 目录树与 ls src 一致 + 依赖声明与 CMake 一致门 |
| M9-C-1 | - | 同函数一宽一窄自证不变：_mkdir/_unlink/FindFirstFileA/MoveFileExA 按 ANSI 代码页解释 UTF-8 路径 | 改名/遍历走 widen+MoveFileExW（对齐 hiss_stream_writer 正范式）或复用 aio_fopen_utf8 适配层 | 非 ASCII 路径写出/发布用例（需 Windows 节点复验） |
| M9-G-4 | - | 定长缓冲 + 不查 snprintf 截断，超长路径下 tmp 退化为 target（原地覆写），begin_v1 无长度校验 | 改用动态/显式长度校验，截断即 fail-closed（返回 PARAM） | ≥511 字符路径写出的负例：不得原地覆写、须明确报错 |
| M9-G-5 | - | IO-001 未继承同仓发布实现的 fsync/父目录 fsync 纪律 | 提交前 fsync 文件句柄与父目录，再原子 rename | 断电/崩溃注入模型下产物完整性门（或显式降级并登记偏差） |
| M9-H-4 | - | 负 max 转 size_t 巨数后 min 取产品内点数，n 远超调用方缓冲容量 | 入口加 if (max < 0) return -2（或 fail-closed 并 set_err） | max=-1 调用不越界写的负例（ASan/边界检测） |
| M9-H-5 | - | 漏项：同文件 readPixels 有大小校验，snr/weight 两处无 | 同 readPixels 补 rawData.size()%4==0 与等值校验，失败即拒绝 | 非 4 倍数块长的负例（越界检测/ASan） |
| M9-H-6 | - | 越界浮点→整型转换 UB + Windows 长偏移截断，且无 offset+size ≤ 文件长度校验 | 引入 64 位 seek 包装（_fseeki64/fseeko）+ 有限性与区间校验 | >2GiB 与超范围 offset 负例（Windows+Linux 双平台） |
| V10-N-04 | - | 同一合法头两读面结果不同，解析失败静默退化为缺省（像素量级静默错） | 统一 D→E 归一 + 解析失败即诊断/拒绝，与 parseDouble 同口径 | 含 D 指数 BSCALE 与非法 BSCALE 的正/负例，两读面结果一致 |
| V10-N-05 | - | 坏行计数不外露、无 nan/inf 门，交付 SNR 星表静默少行不可观测 | 计数经 inspect/诊断外露并在阈值/非有限值上 fail-closed | 含坏行与 NaN 的负例：可观测且可拒绝 |
| V21-N-02 | - | 调用方无法区分"文件坏"与"数据坏"（ENGINEERING_SPEC §9 统一状态码+结构化诊断未落实） | 把 HISS 语义码映射到统一状态码后再上抛，禁一律 FILE | 校验和坏/缺必填键/格式违规三类返回码可区分的用例 |
| W2-N-09 | - | 读入面缺 PC 支路（AIOWCSKeywords 无 PC 字段），注册表 WCS CONFORMANT 判定失真 | 补 PCi_j 支路（或 CDELT×PC 合成）并同步注册表偏差登记 | CDELT+PC 头读入旋转量与参考实现一致的用例 |
| W3-R2-001 | - | remove→rename 窗口内旧正式文件已失，两次 rename 均失败即数据净损失；注释把该路径写成合规 | 改用带 REPLACE_EXISTING 的单步原子替换（对齐同库正范式） | 注入第二次 rename 失败时不丢旧正式文件（崩溃/失败注入用例） |
| W3-R2-009 | module_adapters.cpp 并发线在改：读时 hash=3ac7b6514ae2，复核时=d167008bb60e（行号随移，判据内容逐条复核不变） | 同仓两文一题互相矛盾（冻结禁令 vs 兜底实现+自证注释）；合同锚 00_COMMON_CONTRACTS.md 悬空 | 改为单步原子替换并清理悬空引用 | 崩溃注入下任一时刻观察到的都是完整文件（旧或新） |
| W3-R2-010 | - | 非 ASCII 路径下改名静默降级（ACP 解释） | 改名改 MoveFileExW（对齐同库正范式） | 非 ASCII 路径写出用例（Windows 节点复验） |
| W4-R2-01 | 台账自注">1024 实删需运行期复现"，本轮同样只判静态结构 | 同文件两种截断纪律并存，截断后可命中同名兄弟路径导致整删 | child 拼接统一判截断并 fail-closed；PUBLISH_PATH_MAX 单一定义 | >1024 深路径下不误删兄弟目录的负例 |
| W4-R2-02 | - | 父目录预置同名 symlink/junction 可被自愈 rmrf 整删；两进程并发写同 out_dir 后者删前者在途 staging；promote 无树内容校验 | lstat/重解析点检查 + stage 名加 pid/随机 + 先 EEXIST 后删除 | symlink 占位与并发同 out_dir 的负例（不误删他人在途物） |
| W4-R2-08 | orchestrator/cache 两站生产不可达系台账在册封顶理由 | 三站均无独占创建，POSIX 下后到者可顶掉先完成成品 | O_EXCL/随机后缀创建临时文件，end 时再校验目标状态 | 并发同名写出的独占性用例 |
| W5-N-06 | - | 失败态未编码（空串既非 null 也不匹配 pattern），provenance/verify 面无从区分"没算"与"算错" | 失败写 null 或中止，并读取 ok 出参；口径统一为 null | 不可读文件的 sha256 字段仍通过 schema 校验的负例（须红） |
| W5-N-13 | 峰值 8×4^k 为算术推论；是否触达 bad_alloc 属运行期未判 | 无字节封顶与错误传播（ASTROCS_DESIGN §12 禁"无界内存增长"，ENGINEERING_SPEC §10） | 设显式字节上限并在全败时返回错误而非空块 | 超大 blk->size/损坏块的负例：不静默空块、不无界增长 |

### P1-002（21 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M3-C-011 | 台账行锚 noise_model.cpp:282/:324 → 现 :311/:353 | 同一字段承载多义：0 同时表示「一致集为空 / IRLS 未启动 / mad_in=0」；fit_status=2 同时表示 sigma 非正与 n_eff<3；use_gain_model 字段存在但无读者易误用 | 公共头登记 0 与 2 的取值语义（或改用具名状态码）；use_gain_model 接线或删除 | 头文件值域表覆盖 0/2 的全部产生点；use_gain_model 有读取点或被移除（静态可判） |
| M3-E-002 | 台账 14 组中 13 组仍不命中的结论本轮复核成立（抽样 2 组） | 文档行锚未随实现漂移同步，claim↔evidence 一一对应破裂（至少 PHOTOMETRY.md:37 与 NOISE_ESTIMATION.md:120-121 两处） | 逐条重锚到当前符号并纳入锚点门（rc=0 才可提交） | 锚点门 rc=0；文档行数自述 == wc -l 实测 |
| M3-F-001 | 台账澄清仍成立：UT-BACKEND 确以 discover 执行该文件，问题在「承诺的验收手段无实现」 | SCI 承诺的独立验收手段无对应实现（全文 0 处 numpy），现有对拍是同源 ctypes；且整类由 g++ 存在性决定可跳过 | 建 NumPy 独立复算 Oracle，或从 SCI §15 验收条件中移除该承诺 | 独立 Oracle 存在、非整类 skip，且不调用生产实现 |
| M3-F-003 | 台账行锚 :304/:307 复现 | 公共 API 不输出 a,b,c 三系数，测试只能退化为相关性/RMSE；SCI §11 的系数复现门无可执行面 | 公共诊断输出 a,b,c，或经内部测试钩子断言系数在 10% 内 | 平面场恢复 a,b,c 逐项 ≤10% 断言在位且注入扰动可变红 |
| M3-F-004 | - | 追溯表把 VERIFIED 挂在不被任何 CMake 目标编译的文件上；p1noise 子图仍需 TARGET p1noise_under_test AND astrocs_p1_noise 才建 | 把 noise_model_science_test 纳入可编译目标，或改锚到在编目标 | 该文件出现在至少一个 add_executable/add_test 且可跑；TRACEABILITY 行可复跑 |
| M3-H-001 | 台账行锚 414/419/423 → 现 446/451/455 | 空间分支过 floor 而全局兜底分支不过 floor：无合格 patch 时该对量为 0，调用方仍调 fill 即得整帧 variance=0/ivar=0 且 fill 返回 0 | 全局分支同样经 variance_floor 夹逼，或在退化态让 fill 返回非 0 拒用 | 任意输入下 fill 输出 variance≥1e-12 且 ivar 有限（含无合格 patch 分支） |
| M3-H-002 | 台账行锚 458/461 → 现 490/492-493 | 两侧条件不对称：α=0 得 var=0 而 ivar 保留；α=NaN 得 var=NaN 而 ivar 保留 ⇒ var·ivar≠1 | 两侧同条件同域处理（NaN/≤0 统一拒或统一传播） | var·ivar==1 在 α∈{0,NaN,Inf,负} 边界用例下成立 |
| M7-A-106 | - | 把逐星回归散布 sigma_residual 直接当定标均值标准误（n=100 时约高估 10×），门余量过大不可证伪 | SCI 改为 sigma_cal_rel = ln10·sigma_residual/√N，或显式声明其为单星散布并另给均值不确定度 | 注入已知 scale 的 N=100 场，估计不确定度的覆盖率符合名义值（可证伪） |
| V1-N-02 | - | 同一二进制标签承载不同物理量：头注释仍是旧 (A-B)/mad 与 1/(ln10·σ)，生产者已改存最优提取 SNR_F；snr_format 无新版本位 | 更新头注释为现行物理量并加版本位（JSON 面已有 snr_schema=DATA-P1-SNR/2） | 头注释与生产赋值语义一致；旧值消费方有版本判别路径 |
| V1-N-04 | 台账行锚 2808/2816 → 现 2823/2832 | SCI 的否定句未随实现接线更新（文档否定 vs 交付 JSON 逐源写出 snr_f/sigma_f_adu） | SCI §2a 改为「已产出：逐源 snr_f/σ_F，帧级 5σ 深度 m_5」并保留 σ_F 定义 | SCI §2a 陈述与 module_adapters 交付键一一对应（可 grep 核） |
| V1-N-06 | 台账 N-06b 子锚（:1119-1121）本轮实测不存在（文件已缩至 900 余行） | 同一函数三条路径三套哨兵：v1 早退不写（调用方栈值）、v2/v3 置 0、头文件承诺 NaN；输出不可判 | 入口统一 memset/NaN 初始化全部输出字段（含早退路径） | 三条早退路径下 median_source_snr/frame_depth_m5_mag 均为 NaN（isnan 断言可证伪） |
| V1-N-07 | 文件头自述 NON_PRODUCTION_TOOL_ONLY 仍在 | 在册测试未接执行面（模块 CMakeLists 无 add_test），且依赖不存在的 python 包目录、钉旧物理量口径 | 退役该脚本，或重写为现行 SNR_F 口径并注册 ctest | 该测试被 CMake 注册且 rc=0；或显式退役并在 §9 证据列移除 |
| V1-N-09 | 原锚 EVIDENCE_MANIFEST.json 全仓不存在 | 参考实现与生产共用同一常数（kFwhmFactor 等）与同一网格规则，声称的独立 NumPy oracle 不在盘且 run/ 不入库 | 建仓内可跑、与被测零共享常数的独立 Oracle | Oracle 脚本入库、与被测零共享常数，且被 ctest/pytest 注册执行 |
| V1-N-12 | 五点本轮逐条复现（(c) gain 分支见 snr_science.cpp:145-149/:196） | 逐源提取按整数格点像素中心采样（无子像素质心）；椭率列丢弃 ⇒ 仅各向同性适用；F_5 用参考源 σ_F 而非逐源自洽值；gain>0 时在经验 MAD σ 上再叠加 (RN/g)² 与 S/g（SCI §9a:100 … | 按子像素质心采样并使用 ecc 列；F_5 改用逐源自洽 σ_F；gain 分支与 SCI 诊断口径分离 | 各向异性 PSF 注入场 SNR_F 偏差 ≤5%；F_5 与逐源 σ_F 自洽；gain 分支有 SCI 出处 |
| V20-N-03 | 台账「该 TU 仍 untracked」部分已消（git ls-files 已入库）；alpha_given/variance_given/ivar_given 仍 read=3 作正对照 | 死存储：cfg_present 13 写 0 读（唯一其他引用 :360 同段判定），外层 variance_floor 仅 :472 写而唯一读点读内层 cfg.variance_floor | 删除死字段或补读取点；接线前不得宣称配置生效 | 静态扫描 0 写或 0 读的配置字段清零；配置面有正/负用例 |
| V7-N-05 | 台账行锚 2664-2666/2838-2846 → 现 2691-2701/2869-2875 | snr 节点被写成非 object 则整块静默忽略（不报错不降级）；缺键落 0.0 与显式 0.0 在产物面不可区分；manifest 不回显生效 SNR 科学配置 | 缺键 fail 或显式标记为 unknown；manifest 增生效 snr 配置块（与 sci_cfg 同源） | 缺键/非 object 输入 rc≠0 或产物含显式 unknown 标记；p1_snr.json/manifest 可复算生效配置 |
| W1-N-04 | 台账原四字段已改 double（不再有 (int)d）；TU 已入库（git ls-files），但仍不在根 CMake 主图 | JSON 取值面允许 nan/inf 字面量，double→int 在 NaN/超范围时为 UB；台账原四字段（hreadn/dark/full_well/gain）已改 double，但同类无守卫转换仍在四处 | 四处补 isfinite + 范围夹取，超界拒绝并返回 ACS_ERR_PARAM | 注入 nan/inf/1e30 的 patch_grid_x/min_patch_samples 后 rc≠0 且 UBSan 无告警 |
| W2-N-01 | - | 甲式缺 (1+n_ap/n_sky) 天空项且泊松项按 ADU 直加（Photometer 全类无 gain 入参即隐式 gain≡1），与同域冻结 CCD 方程三方互斥 | 按同一 CCD 方程改写 measure_flux 的 flux_error，或显式声明其为诊断量并登记 | 同一注入场下 Photometer 与 snr_science 的方差相对差 ≤5%（可证伪） |
| W2-N-04 | 台账行锚 orchestrator.cpp:3007-3008 → 现 :3017-3018 | 该分支既不 gate out_n_matched>0 也不 gate out_scale≠1.0，退化恒等拷贝同样落此并被声明为已应用测光；模块管线侧已改读 p1_phot.json，两路口径不一致 | orchestrator 改读 p1_phot.json provenance（与节点侧同源），删硬编码 1 | 退化恒等拷贝时 PHOTAPPL=0 且 PHOTDEGRADE=1；两条路径产物一致 |
| W2-N-07 | 台账行锚 377-379/782 → 现 465-466/892 | 只累加 Σvar 分子并显式丢弃 area，等价 Σ_j v_j·Σ_p w_jp²/D² 而非 (Σ_p w_jp)²/D²，差额正是文档要求显式加的交叉项 | 按文档协方差式累加（保留 area 权重），或显式登记该近似及其适用域 | 一 drop 均分 2 子叶场景下父元方差与解析式一致（相对差 ≤预设容差） |
| W2-N-10 | - | 5×5 窗减背景截断和充当「总通量」并被 f_in 二次惩罚（同字段宣称已改正）；负残差整流使 flux 恒正，失效判据永不触发；与 photometer 整格和/sdet box_sum 三套定义并存 | 明确 flux 语义（截断和 vs 总通量）并只施加一次孔径改正；保留负残差 | 已知总通量注入场下 flux 与 f_in 语义自洽；负残差场不产生恒正假象 |

### PKG-001（20 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M5b-C-02 | GAP-018 登记同证据但不合并（本条保留原 ID） | 两平台清单对同一 unit 的状态来源不同步：Windows 模板无条件 IMPLEMENTED，Linux 清单保持 SKELETON 并自述不伪装完成，跨平台无一致性门 | 把两 unit 状态绑定到同一事实源（生成时按平台实现面派生），并加跨平台清单比对门 | 新增门：两清单同 unit_id 状态集合断言（除平台限定项外必须一致）；不一致即红 |
| M5b-G-04 | 同族 M5b-I-04/V21-N-14 分别记 README 与 contract/schema 面，本条保留 product/contract/lock 面 | 产品版本多副本手工维护且与根 VERSION 漂移一档，VERSION-CONSISTENCY 门扫描面不含 packaging/** | packaging/** 版本改为生成期注入（configure_file/模板）或纳入 VERSION-CONSISTENCY 扫描面，禁止手抄字面量 | 把 packaging/ 任一版本字面量改错，VERSION-CONSISTENCY 必须判红；四份副本与 VERSION 逐字一致 |
| M5b-I-04 | CHANGELOG.md 已删 ⇒ 台账「已更新」一腿不可复现，本条改按 packaging 残余面成立 | 版本单源未贯通到 packaging 面；台账所依 CHANGELOG 面已随文件删除消失，残余面集中到 packaging | packaging 版本改生成期注入并纳入 VERSION-CONSISTENCY 扫描 | packaging/** 版本字面量与根 VERSION 逐字一致（门可红） |
| M8a-C-006 | 目标 lib/astro_image_io → lib/infrastructure/aio（ARCH-001 #13） | 许可/来源声明由脚本手写字面量，与仓内实际许可文本和链接闭包不一致；SBOM 关键字段 NOASSERTION；许可权威指针悬空 | NOTICE/SBOM 从 dependency-lock 与 third_party 许可文件生成（含哈希），修正 cfitsio 许可名，删除悬空路径 | 生成物中的许可名/版本/哈希与 dependency-lock+许可文件逐项一致（机器比对）；发布包不含 NOASSERTION |
| M8a-C-007 | 本条按静态读入判断，未跑 MSVC 构建 | 锁描述的是 cli/ 遗留 compat 工程行为，根图无降级分支：zcompress.c 无条件编译 → 未设 ACS_ZLIB_ROOT 的 MSVC 检出为编译期硬失败 | 根图补 else 剔除 zcompress.c/zuncompress.c（或显式 FATAL 但同步改锁描述），使锁描述与构建图行为一致 | 未设 ACS_ZLIB_ROOT 时 MSVC 配置必须按锁描述降级（可配置成功）；锁 policy 文本与构建图分支一一对应 |
| M8a-C-009 | 目标 lib/phase2/tools → lib/algorithms/coverage/tools（ARCH-001 #31） | 实际参与 oracle 的第三方包(astropy-healpix/rcr/scipy)未在依赖锁登记版本，锁内 astropy/numpy/pytest 亦为区间/system → oracle 不可复现 | 把 tools 实际 import 的测试依赖补入 lock 并锁版本；移除零引用依赖(scipy)或在锁内标注用途 | 锁内 test_only_oracles 覆盖 tools/** 全部 import（机器扫描 import↔lock 差集为空） |
| M8a-G-001 | 台账行号 :570 漂移为 CMakeLists.txt:596 | copyleft(GPL)依赖进入交付链接闭包却未在任一登记面登记许可与来源，发布 provenance 不成立 | 在 DEPENDENCIES.md/dependency-lock 登记 gsl(GPL) 并评估再分发义务（或移除依赖换实现）；许可文本补入 packaging/licenses | 发布候选门断言链接闭包 ⊆ 登记面（机器扫描 target_link_libraries ↔ lock）；GPL 项有负责人处置结论 |
| M8a-G-003 | 目标 tools/quality/gen_source_index_v61.py 存在（我首轮存在性 sweep 因解析串含「未解析:」误判为缺失，已复核存在） | vendored 判定用路径段而非来源/许可证据，派生实现(ipv/nanoflann/healpix_core)既不在第三方清单也无豁免登记 | 改为按来源+许可证据判定并在 lock/NOTICE 登记派生实现；证据落 artifacts/**（现行落位）而非已删 evidence/ | 机器扫描：链接闭包内每个非自研来源均在登记面有条目（差集为空） |
| M8a-G-005 | 目标 lib/drizzle → lib/algorithms/drizzle（ARCH-001 #9） | 再分发署名义务的载体（许可文本/版本/哈希）未登记；构建脚本保留指向不存在目录的 if(EXISTS) 幻影守卫 | 补 nanoflann/healpix 等 BSD 源的许可文本与版本/哈希登记；删除或落实幻影守卫 | 发布候选门断言：链接闭包内 BSD/MIT 源均有许可文本+哈希（机器可判） |
| M8a-G-010 | 上游版本以本轮 web_fetch 现势为准（与台账 200 复现一致） | vendored 树哈希锚只覆盖编译集（2 个锚），非编译面与上游跟踪/升级评估无登记 → provenance 不完整、上游落后无决策记录 | 按目录级清单(逐文件 sha256)登记 vendored 树，并对上游 4.7.0 给出升级或暂不跟进的理由登记 | 发布候选门断言 vendored 树逐文件哈希可复算；上游版本决策有登记条目（可机器查） |
| V14-N-01 | 与 M5b-C-02 同证据、独立 ID（本条判「无跨平台门」） | 两平台清单状态各自维护、无一致性门；同两 unit 的 SKELETON/IMPLEMENTED 互斥属发布面失真 | 状态绑定同一事实源并在 CI 加跨平台清单差异门（除平台限定项） | 新增门：两清单同 unit 状态集合一致（不一致即红） |
| V14-N-05 | 台账「lib/snr_estimator/CMakeLists.txt untracked 而根 CMake 已解除 add_subdirectory」属未现发结构缺陷 | 安装规则按 if(TARGET) 自动跟随构建图，产品清单/断言却钉死 10 项 → 正向钉在册计数，补登记先破断言 | 清单与安装规则同源派生（生成期取 target 清单）；断言改为「清单 ⊆ 构建图且每项有 target」的结构判据 | 新增一个模块 target 后：安装树、清单、断言三者一致且不出现「补登记即红」 |
| V21-N-07 | 台账 scripts/package_audit.py 已不存在，本条按残余三工具成立 | 打包链无一步校验子进程 rc，失败静默继续；三个 build_v19r* 工具仍可复跑且未按 RETIRE-001 登记退役 | 要么按 RETIRE-001 给三工具加退役抬头（显式失败），要么补 rc 判定并注册为检查项 | 打包脚本任一子进程失败必须非 0 退出（可注入 git 失败复现）；未注册工具须有退役或注册二者之一 |
| V21-N-12 | 订正：loader 侧非空分支现已存在（secure_loader.c:499 判 expected_sha256.size>0），台账「读端无非空分支」一腿不再成立 | 清单哈希面为空，交付物完整性校验（签名清单/哈希比对）在装配与校验两侧都无数据来源 | 装配期计算各 unit 产物 sha256 写入清单；校验器与 loader 断言非空并与实际产物比对 | 注入产物篡改，校验必须红；清单 units[].sha256 非空率 100%（除显式 SKELETON 且登记） |
| V21-N-14 | 台账行号 :5/:12 漂移为 install-tree.contract.json 与 schema target_version 段 | 版本字面量在 packaging 合同/schema 手工钉死且与根 VERSION 漂移；两平台清单生成方式不同导致新鲜度不对称、读端零判定 | 合同/schema 版本改生成期注入并纳入 VERSION-CONSISTENCY 扫描面；两平台统一生成路径 | contract/schema 版本与 VERSION 逐字一致（门可红）；两平台产物版本来源同一 |
| W4-R2-05 | 本条无门注册与零外部引用系 finding 在册事实，不重报 | 布局/命名三套并存（dist/astrocs-alpha、SHA256SUMS、SHA256SUMS.txt/AstroCS_Review_*.zip），生成侧与校验侧不同源 → 对账工具必红或验错物 | 按 ASTROCS_DESIGN §10.1 安装树形态统一布局与清单命名，废弃幽灵路径；生成侧产出校验侧读的文件名 | 生成→校验闭环：对同一候选包跑 check_release_layout 必须 rc=0；布局字面量单源（可机器比对） |
| W4-R2-06 | 历史包是否带假时间戳需运行期（台账自注未判），本轮只判当前树常数化 | provenance 时间戳与版本兜底均为手写常数（同文件另有实时 rev-parse commit 字段自相矛盾），且兜底版本已落后 | 时间戳改实时生成；版本缺失改显式失败（fail-closed）不兜底；产物按 DESIGN §12 前不携带版本信息 | 注入 VERSION 缺失，脚本必须非 0 退出；产物 generated_at_utc 与实际运行时间一致（可判） |
| W4-R2-07 | 引用 0 命中系 finding 在册事实，不重报 | 死工具仍完整存续无退役标记：一经使用即产自宣 PASS 的假清单且直撞发布白名单（含全量 git 历史） | 按 RETIRE-001 口径加退役抬头并改入口为显式失败（exit 2），或重写为从实际检查结果取数的工具 | 工具无参调用必须显式退役失败或产出可复核数据；产物不得含计划值冒充实际值 |
| W6-N-03 | 实际交付树是否多装需一次 cmake --install 实测（本轮未跑），本条只判口径与注册缺失 | 装配期通配安装与合同白名单不同源 ⇒ 白名单不闭合（安装树可多装未被合同声明的 schema），唯一闭集校验器无人调用 | 装配规则改为按合同白名单逐文件安装（或从合同生成安装清单），并把 verify_install_tree 注册为检查项 | 新增/删除一份 schema：安装树与合同 units 必须同时变化（机器断言差集为空）；校验器在注册表内 |
| W6-N-05 | 真机导出集是否越界需 Windows x64 dumpbin（本轮无该节点），不另判 | 导出集由链接器全开(WINDOWS_EXPORT_ALL_SYMBOLS ON)决定，符号门只判非空 ⇒ 导出面白名单合同无判据承载；Linux 侧无采集 | 按 module_dll_contract 生成/校验导出白名单（Windows dumpbin/Linux nm 双侧），符号门断言集合相等 | 注入一个计划外导出符号，符号门必须红；导出集与合同白名单差集为空（两平台） |

### QA-001（20 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M3-I-004 | 台账路径 lib/phase1/tests/p1phot 已迁 wrapper_phase1；三份同名文件行数与台账一致 | 同名异体副本未清理；未注册副本头注指向一份不存在的「CMakeLists.txt 头注」，构成悬空自证；注册面仅一份 | 删除未注册副本（或明确标 legacy/对照并移出共址目录），头注改指现行共址与注册点 | 仓内同名测试文件唯一；未注册目录不得出现指向不存在 CMakeLists 的锚 |
| M5a-I-002 | 台账行号 :51/:53/:55/:67 漂移为 §1.5 尾句与 §1.6 段落（同文同义） | 同一文档对同一 ISA 的平台归属与证据状态两套口径并存，旧句未随 §1.6 更新 | 删除/改写 §1.5 尾句并在 §1.6 引用平台职责条款（Windows 复验仍归 WIN-003/WIN-00x） | 文档级检查：同一 ISA 在全文的平台归属/状态字面量唯一（可 grep 断言） |
| M6a-F-001 | 台账路径 lib/common/healpix → lib/algorithms/shared/healpix；同目录现存 gen_spatial_fuzz.py/snr_hips_spatial_oracle.py | 宣称的 1e6 点 oracle 缺生成器与构建注册（测试文件在库但永不编译），宣称无机器证据承载 | 补生成器（或改述为现存的 snr_hips_spatial_oracle.py/gen_spatial_fuzz.py 路径）并把 test_healpix_oracle.cpp 注册进测试面 | ctest 内可见 healpix oracle 用例；healpix_core.h 的宣称与注册测试一一对应 |
| M8-F-006 | lib 侧唯一在册根是 gaia_xpsd_client/tests，其余 15 处零采集 | 检查注册表的 discover 面未覆盖 lib/** 与 tests/realdata，游离测试即便失败也不阻断；未采面在结果面上与通过同形 | 把 lib/** 与 tests/realdata 纳入 CHK-UNIT 的 discover 根（或显式登记豁免与理由），并修正 return-布尔型用例为 assert | 改动 lib/** 下任一 Python 测试使其失败，CHK-UNIT 必须判红（覆盖扫描面断言） |
| M8-F-011 | 台账 18 个 discover 根一腿本轮复现（同 M8-F-006 命令） | 真实数据层既未进阻断 profile，其用例形态又与 unittest discover 不兼容 → 双重静默不执行 | 把 tests/realdata 纳入 linux-main 的 discover 根并改为 unittest.TestCase（或显式 pytest 门），真实数据不可用时显式 NOT_APPLICABLE 而非静默 | 对齐真实数据夹具后跑 linux-main，tests/realdata 用例数>0 且失败能红 |
| M8-F-015 | 本轮重算口径与台账一致（5 文件 5 处） | 占位断言使门恒绿且以注释/常量冒充不变量（p2_upm_synthetic:125、p3_assembly:127） | 把 5 处 CHECK(true) 替换为真实不变量断言（或删除该断言并显式登记未覆盖） | 对每处占位注入反例，断言必须能红；tests/unit 内 CHECK(true) 计数为 0 |
| V17-N-01 | 台账 TU=11/17/1 本轮逐字复现（同 git ls-files 口径） | 父侧把「子进程返回非 0」一律记为必败验证通过，127(execve 失败) 碰撞不分类 → 元门 fail-open | 父侧对 127/信号/超时独立分类并显式报错；统一用显式相位参数而非 rc 值域推断 | 注入 execve 失败（PATH 破坏）场景，自检必须报元门错误而非「必败验证通过」 |
| V17-N-04 | 同仓正对照 tests/backend/test_isa_bit_manip.py:32 用 assert r.returncode == 0 | 编译非零被降级为 skip（非 fail），而门是 non-waivable 的 linux-main discover ⇒ 环境/编译失败静默转绿 | 编译失败改 self.fail（或 setUpClass raise），确需跳过时按 §8 显式登记豁免 | 破坏编译（注入语法错误），UT-BACKEND 必须判红而非 skipped |
| V17-N-05 | 目标 cli/** → lib/infrastructure/cli/**；lib/phase*_session/* 仍在盘上（DEFERRED，见 ARCH-001 §1） | 扫描面缺失被当「无违规」跳过（fail-open），纯静态文本检查还被运行期产物 skipUnless 门连坐 | 路径缺失改显式 fail；静态检查与 EXE 存在性解耦（拆成独立用例） | 删除任一清单路径，该测试必须红；build/cli/astrocs 不存在时静态检查仍执行 |
| V17-N-06 | 台账证据取 runtime/io/*.so（该目录已删），本条以现行 lib/infrastructure/aio/io/ 产物复现同型缺陷 | 测试按存在性复用宿主 .so，无 mtime/哈希新鲜度前置 ⇒ 宿主机可长期对旧产物判绿；台账所引 runtime/io/ 目录已不存在，残余面在 tests/io 的复用逻辑 | 三站加「源哈希/提交号 ↔ 产物」新鲜度前置或每次强制重建；把生成步纳入 CI | 改动 hips_core.c 后不重建，测试必须红（新鲜度断言）；CI 内 .so 由固定步骤生成 |
| V18-N-01 | 台账行号（p1hips:69 等）随格式化改行，机制不变 | 注入语义用 else-if 短路替换条件求值（断言失去判据），且无任何门校验分支次序 | 改为「先求值 cond 并记录，再叠加注入结果」或把注入结果并入失败计数而非短路 | 注入名命中时 cond 仍须被求值（可用副作用/计数断言）；机器检查断言宏内 !(cond) 在注入支之前 |
| V18-N-05 | 台账行号 :288-318 漂移为 :286-316 | 自检取自被检物自身（循环 oracle），唯一负例在同仓 tests/monitoring 归 UT-MONITORING 门、本门不跑 | 在 --selfcheck 内加入独立构造的非法行负例（缺字段/坏 seq/超长行），或把该负例纳入本门 steps | --selfcheck 必须包含至少 1 条独立负例且能红；schema 来源不由 checker 自身常量单向定义 |
| V18-N-09 | 台账口径 133，本轮按去注释后统计得 136（口径差已记）；文件已迁 lib/dynamic_psf→lib/algorithms/psf | 注入名不落在调用点字面量，静态枚举/注册表对齐无从机器校验，自检默认名与注册面可漂移 | 注入名改为字面量或将名表集中在可机器读取的注册表（数组/JSON），并加集合相等断言 | 注入名注册表与调用点集合相等（机器断言）；自检默认名必须在注册表内 |
| V18-N-10 | 台账 9 名与 3 名差集本轮复现 | 手抄清单与注册表漂移且无集合相等断言 → 3 个注入名「注册但从不驱动」 | 注入名集中为单一注册表并加集合相等断言（注册集合 == 被驱动集合） | 对 k_injections 与调用点做集合断言，差集非空即红 |
| V18-N-11 | 与 V17-N-01/V18-N-14 同族，本条独立 ID 保留 | 元门 fail-open：127 与「必败验证通过」同形；重入判据 argv 口径恒真，置空值即跳过两阶段并 PASS | 父侧分类 127/信号；重入判据统一为显式相位参数（如 argv[1]=="inject"） | 破坏 execve 或置空注入变量，自检必须报错而非 PASS |
| V18-N-12 | 与 M8-F-014 同文件不同判据面（本条 P0 记门名承诺的不变量） | 门承诺的「AVX 必被 AVX2 严格主导」无断言承载，判据工件自产自读，末行空断言 | 实现比值断言（读 benchmark 当轮产物），删除自产工件与空断言 | 注入 avx 优于 avx2 场景门必须红；测试内 assertTrue(True)/空断言计数为 0 |
| V18-N-14 | 台账已实测置空值整门跳过并 exit=0（本轮复核判据文本一致） | 重入判据不统一，argv 口径恒真使 p1drz 退化为「env 存在即注入相」，置空值即跳过并 PASS | 统一为显式相位参数（argv[1]=="inject" 或 --phase 参数）并加仓级一致性断言 | 逐站补不得漏：机器检查断言所有 selfcheck TU 使用同一重入判据形态 |
| V19-N-06 | 台账口径 1005/208(20.7%) 与本轮 1118/204 口径不同，已在证据写明重算值 | 「整类被 skip」与「整类通过」在结果面上同形：R11 只判采集数>0，runner 不解析用例级 skipped | runner/门解析 unittest 的 skipped/failed 计数并对 skip 比例设阈值或要求逐条豁免登记 | 注入整类 skip，门必须报告 skipped 用例数并判红/要求豁免登记 |
| V2-N-06 | 目标已迁 lib/star_detector→lib/algorithms/star_detection、lib/photometric_calib→lib/algorithms/photometry | 测试自述的断言缺失（文档-实现不符），oracle 直接复刻被测公式并从注释自认「同被测」→ 不是独立 Oracle | 补齐或删除自述断言；oracle 改为独立解析解/高精度参考（或明确标为一致性锁而非 oracle） | 断言数与自述一致（机器可查）；oracle 不含「同被测」复刻式（评审可判） |
| W1-N-07 | 路径 lib/plate_solve → lib/algorithms/platesolve（ARCH-001 #22） | 测试文件未进构建图：修改该文件的回归锁永不执行，同目录其余测试已注册形成正对照 | 把 test_last_inlier_reset.cpp 注册进 ipv/test/CMakeLists.txt 与 ctest | 新增/修改该文件后 ctest 内可见用例；注册面缺失由检查器判红（未注册测试文件扫描） |

### RT-001（20 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| L28c-E-001 | 行号较台账漂移 | 头为 RT-001 合同骨架未接线执行器；:62 的 ABI 布局断言声明无对应测试（tests/ 仅 py 侧 typed_dag 校验） | 或补 C++ 消费者与 ABI layout 冒烟测试并登记构建源，或把 :62 声明降级为「待接线」并标注所有权 | ninja -C build 中该头有 TU 依赖 + 布局断言测试存在且能红 |
| M2a-C-8 | - | 实现演进（per-file 锁+多查询并行轴）未回写合同 §4；lease/cancel 用文件级 static，多实例并发互相覆盖 | 合同 §4 按现树改写并行轴与锁语义；lease/cancel 改实例域（句柄内） | 合同 §4 与 gaia_client.c 并行/锁语义逐条对齐 + 双实例并发测试 |
| M4-C-05 | - | 文档按旧实现（per-worker 句柄无锁）写，实现已把所有 cfitsio 读串行化，未回写 | 改写 :295/:387 为「I/O 段经 g_aio_mu 全局串行」，或实现改为每 worker 独立句柄不共享 | 文档陈述与 sampler.cpp 锁语义一致；N-worker 数值一致性测试仍绿 |
| M5a-C-002 | - | 文档冻结在 hotfix 时点，实现后续改 std::thread 池未回写 | THREADING_MODEL.md:21 改写为 std::thread 池（N-worker，OpenMP 条件已移除），并同步 EXECUTION_MODEL | 文档与 sampler.cpp 并行实现一致；1 vs N worker 确定性测试仍绿 |
| M5a-D-002 | - | RQS 任务号/提交哈希/历史计数留存在实现注释与活动架构文档 | 注释改机制描述（去任务号/章节号/哈希/PASS 计数），历史信息归 reports/ 或控制包归档 | CHK-STALE-DOC 及 ENGINEERING_SPEC §2 类检查对 lib/ tools/ docs/architecture 的任务号与哈希零命中 |
| M5a-E-001 | 本行只判文档锚，不重复 GAP-019 计量 | ARCH-001 迁移与后续重构改了行号/文件，文档行锚未随更新，且引用已由 §0 下链的旧文档 | 行锚改符号锚（函数名/常量名）；删除或补 工程控制/schemas 悬空引用 | CHK-DANGLING/CHK-SCI-REF 对文档行锚与路径引用零悬空 |
| M5a-G-006 | - | 两套并行入口（OpenMP ICV + ThreadLease）无统一预算源 | OpenMP 线程数改由 Runtime 预算/profile 注入（或 host 统一设置 OMP_NUM_THREADS 并登记） | ENGINEERING_SPEC §10 单预算源检查（OpenMP 与租约同源）能红能绿 |
| M5a-G-007 | - | MSVC 生产构建不探测 OpenMP，遗留 target 静默串行；OpenMP imported target 依赖测试子图 | Windows 侧启用 OpenMP（MSVC /openmp）或为 Windows 提供串行等价实现并登记；find_package(OpenMP) 移入产品图 | 双平台构建 OpenMP 可用性一致（或显式登记降级）；CHK-BUILD-WIN 覆盖 |
| M8-C-001 | - | 同一文档族声明「旧锚点保留有效」，行锚与串行结论均已失效 | §4 改符号锚并按现树改写 sampler 并行事实 | 文档锚点全部命中真实符号；THREAD 类检查能红 |
| M8-F-012 | - | 被测对象是演示内核且只比抽样和，产品 drizzle 归约顺序未被覆盖 | 对产品 drizzle_engine::threadTiles 做 1 vs N worker 全量逐位对比（或显式登记降级） | 产品归约路径的跨 budget 逐位一致测试能红 |
| V14-N-06 | 行号随重构漂移（台账 5391-5489 → 现 5398-5499） | 元数据与实现脱钩，heavy 租约被单发 JSON 读写节点占用 | 按实现实际把两节点降为 metadata/io 类，或为 wcs/properties 引入真实并行轴 | descriptor.execution_class/parallel_ok 与实现并行轴一致（机器检查） |
| V20-N-02 | 六模块解析/四模块 want 收缩复核不变 | plan 用字面量且租约不随配置变，plan 与实际租约脱钩 | plan 的 min/max_workers 由注册表/配置解析结果写入，租约按实际宽度申请（或注释与键显式退役） | 用户设 workers=4 时 plan 回显 4 且租约申请一致 |
| V20-N-09 | 取消通道断线另见 W3-R2-002 | 取消通道/几何字段无消费端，观测与配置面残留 | 接线消费端（取消检查/投影消费）或删除字段并登记退役 | 每个字段有读点或显式退役登记（静态检查） |
| V21-N-17 | 站点已随 ARCH-001 由 runtime/runtime.cpp 迁至 lib/infrastructure/scheduler/src/runtime.cpp | 上限与估算均缺省，回压代码不可达 | 由 cpu_profile/配置注入 memory_limit_bytes 并填 estimated_memory_bytes | 超预算注入下门能红（阻断后续 heavy 调度） |
| W1-N-10 | p1drz/CMakeLists.txt:95/103 注册两把锁 | 顺序产物无比较器，锁只覆盖数值产物 | 两把锁加入 .order 逐字节比较（或 probe 输出序纳入判定） | 打乱 tile 写顺序时 merge/taskset 锁能红 |
| W3-R2-002 | 原判据第二腿（resample 无取消检查）已被 RunContext 取消安全点补上，不再作依据 | host 取消注入面未接线，DESIGN §6.3 协作取消对经 host 的模块不可达 | Runtime/CLI 在取消时调用 host set_cancel（并统一 gaia 第二套通道） | Ctrl-C 后模块取消点被触发并 exit 9（能红） |
| W3-R2-004 | 合规实现 ScopedOmpWorkerInjection 与 drizzle save/restore 仍在但未迁移到该点 | 进程级 ICV 改写无作用域恢复；登记锚未随行号更新 | 改用 ScopedOmpWorkerInjection（同仓 module_adapters.cpp:240-262 已有）或保存/恢复 ICV；订正登记锚 | 并发调用/嵌套 run 下线程预算不泄漏（能红） |
| W3-R2-008 | - | 注释与同树实现相反，per-run 池仍在（同 run 线程上界≈2×budget） | 删除 scheduler per-run 池改走 executor，或订正注释并登记双池预算 | 同 run 线程数上界 ≤budget（能红） |
| W3-R2-011 | 全仓生产链对该队列仍零接线 | 队列无超时/错误传播返回面，消费者僵死则生产者永久阻塞 | 加 wait_for + 超时返回（或取消令牌接线） | 消费者停摆时 push/pop 按超时返回错误（能红） |
| W3-R2-012 | 自报 1 的 writer 仍按整份预算申请并持有至 execute 返回 | 申请量语义与节点自报 max_workers 脱钩 | 租约按节点声明宽度申请（或声明值参与 heavy 判定） | 自报 1 的 writer 节点只申请 1（能红） |

### OBS-001（19 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| FD-F-002 | 行号随 CLI-001 漂移（台账:287→现:290） | 生产侧 fprintf 硬写常量 false、守卫侧 find 同一字面量，生产者与守卫同处，取值不可证伪 | 该键改由归一化结果（已分配容量分母判定）真实写入；测试改为语义断言（分母=0→false、分母>0→true）或删除该键 | 翻转归一化结果（负向注入）时 mon001_recorder_test 必红 |
| M5a-C-003 | 工件命名半已由 GATE-FIX-RES(D-15) 修复 | 门配置字段无生产填充点（无采集源），PSS 无 /proc/self/smaps_rollup 读取 | 或补采集（smaps_rollup Pss、/proc/pressure/io、cpu_profile 单 worker 基线、进度注入），或按 G-RES-01 §8 显式退役未采集字段并登记 | 每个门输入字段有生产赋值点或以「未采集=不判」显式登记（能红能绿） |
| M5a-G-001 | 数值源部分已修；残留为文案未同步 | 数值源已收敛到契约（85/60），但字段注释与诊断文案三处仍写已废止的 0.75/0.50 | 改 :207/:208/:524 注释与文案为契约值（0.85/0.60）或改为不带数值的语义描述 | 实现/诊断文本中已废止阈值字面量零命中（与契约 85/60 一致） |
| M5a-G-004 | - | 两形参同值且 queue/progress 无生产填充点 | 分别传 active/runnable 实测值；queue depth 与 progress 从采样/节点注入接线或以「未采集」显式登记 | worker_balance/queue/progress 判据在负向注入（单活跃线程/队列饿死）下能红 |
| M5b-C-06 | - | provider 未取实际后端；时间戳两次即时取值无真实起止 | provider 由实际执行后端填充（catalog/kernel provider id）；started/finished 用真实起止时刻（或删除 duration 语义） | 改 provider/暂停 run 的负向注入下 manifest 字段随实际变化 |
| M5b-E-06 | - | 词表与实现无映射关系，无生产者/校验者；文档括注指向不存在的文档名 | 统一 stage 词表（含 P3）并与事件 schema 绑定校验；修正文档括注为 LOGGING_DIAGNOSTICS_STANDARD | JSONL stage 取值经 schema enum 校验；词表每个 ID 有产出者 |
| M6a-G-002 | - | 模块私有日志通道未收敛到 host logger/JSONL；日志落点相对 CWD | 私有 env 与落点收归 host 日志接口（JSONL 事件/文件日志），科学路径 stderr 改结构化诊断 | 无模块私有 env 控级；日志落点只落 output_dir/run |
| V15-N-09 | - | 断言字符串呈现而非门语义，子串匹配无判别力（"1600"/"0.96" 亦命中） | 改断言 GateDiag 枚举/阈值常量/边界行为（如 84.9→FAIL、85.0→非 FAIL） | 阈值漂移或边界反转时测试能红 |
| V15-N-13 | 行号已随 CLI-001 漂移（原 :234-250/:281-284 → 现 :249-253/:323-324） | 断言弱半边：允许 5 种 verdict、产物仅存在性 | verdict 等值断言+产物行数/覆盖窗口断言（如 ≥N 样本、时间跨度≥窗） | 空/单样本产物注入下测试能红 |
| V21-N-03 | - | 采集失败信号在返回前被吞，唯一调用者 sample() 亦丢弃返回值 | ok=false → 返回 false/降级标记（事件标记 monitoring_degraded），调用方 fail-closed | 采样源不可得（负向注入）时门/事件显式标记失败而非全 0 成功 |
| V21-N-08 | M5a-C-003 未列新站点 | 采样层无系统级 CPU 采集，字段默认 0 冒充实测 | 补系统级 CPU 采集（/proc/stat）或删除该列并登记 | 该列由真实观测写入；无采集时显式标记未观测 |
| V21-N-09 | - | 采样→记录映射丢字段，GateConfig.io_ops/io_bytes 类判据原料不可达 | record() 映射补字段或删除无用采样字段；Linux rchar/wchar 挂 read_ops 名需正名（字节 vs 次数） | 采样字段在记录产物中有对应列且语义正确（字节/次数不混） |
| V21-N-10 | - | 观测 API 无消费者，host 事件面未接线 | 在生产节点/调度器接线 work units 上报（或显式退役该 API） | 一次真实 run 的 work units 非 0 且与节点实际处理量一致 |
| V21-N-15 | 行号已随 CLI-001 重定位（原 912/946 → 现 795/838） | 判定输入被常量填充，门失去判别力 | 由真实 stage 标注与监控有效性判定写入（或删除已不可达分支并登记） | 负向注入（无标注/无监控）时对应门能红 |
| V21-N-16 | 行号漂移（原 :795 → 现 :784） | kind 由常量决定，分类阈值面整体死代码；两向都无正确语义 | kind 由节点 descriptor（execution_class）注入，或删除未用分类分支 | Memory/Io/Mixed 节点跑门时走对应阈值（能红能绿） |
| W1-N-13 | 仅 ASTROCS_DRIZZLE_FINE_PROFILE=1 时输出 | 剖面字段无计时来源，字面量冒充实测 | 实测排序耗时或删除该字段（避免运维误读） | 字段为实测值或字段删除；剖面输出无字面量占位 |
| W3-R2-003 | - | 观测汇为进程单例，缺 epoch/reset；runtime 用全局历史峰值当本节点 workers | 每节点/run 边界复位或按 acquired_total 差分计算节点内峰值 | 连续两节点（窄→宽）trace workers 各自正确（能红） |
| W3-R2-006 | 行号已随 CLI-001 重定位（原 843-853/858-859 → 现 736/741-742） | 采样容器无锁，读侧在停止/汇合前访问 | 在 summary()/push_back 加锁，或先 store(false)+join 再 summary() | TSan 跑 CLI run 无 data race（能红） |
| W5-N-01 | IoExecutor（:254 同形 catch）不计数不写 COMPLETED，污染限于 CPU-heavy 路径 | 异常被吞且状态无条件 COMPLETED，无失败传播 | 捕获后记 FAILED/错误域并传播；失败计数与成功计数分开 | 注入抛异常任务后 trace 出现 FAILED（能红） |

### GOV-001（18 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M1a-B-002 | 台账行号 :64/:192-209 漂移为 :68/:196-203（lib/phase3_proj→lib/algorithms/projection） | 注册表清单行与偏差索引自相矛盾：第 3 行偏差列写「无」，索引却把 DISP-P3PROJ-001（且其口径只写 PA 未接线）挂在该行；实现侧 CRVAL2/√2/域三处偏离 Paper II Table 1 仍未消除… | 要么按实现冻结为 PROJECT_DEFINED 并写明 v1 仅对照/生产走 v6 线，要么把三处偏差并入 DISP-P3PROJ-001 并在第 3 行偏差列给出 ID | 注册表机器检查断言「索引行号→清单行偏差列非空」双向一致；CAR/AIT 的 CRVAL2 与 √2 项须有独立解析解 oracle 正负例 |
| M1a-B-003 | 行号 :296/:300 未变（文件哈希 186fd3c05788 一致）；ipv_wcs.h 的 0-based 标注已随 STD-F1-ADJ 方案 b 修正 | 报告串与实现方向相反（代码 crpix+1，报告串 crpix-1）且把 owner decision 写进测试输出串；ipv 侧 0-based/1-based 语义已在头文件澄清 | 改报告串为 crpix+1 并与代码同一常量派生；裁决登记改指现行登记面而非测试串 | 注入 crpix 桥接符号翻转，p1wcs astropy 交叉必须红；报告串与 crpix_bridge 表达式须同源 |
| M2a-B-1 | 目标 lib/gaia_xpsd_client → lib/infrastructure/gaia_xpsd_client；gaia_client.c@5e5dd1c992ee | 登记面把条款标识里的历元维度丢弃后判 CONFORMANT，且无任何历元换算/传播实现（模块代码历元零命中、头文件反称 J2000） | 在注册表要求列显式写出历元处理口径（J2016.0 直接采用或传播到 J2000 并给公式），模块侧同步坐标契约或登记偏差 ID | 机器检查断言「条款标识含 J2016.0 ⇒ 要求列/证据列必须出现历元处理」；gaia 查询输出须有历元一致性 oracle |
| M2a-B-2 | 与 M2a-B-1 同对象不同侧面（本条判不闭合/无机器判据，B-1 判历元实现缺失） | 检查器只做形态断言，缺「条款标识的每个维度都必须在要求列有对应命题」的机器判据，故条款面与判定面可长期不闭合 | 为注册表新增 C9 判据：条款标识中的限定词（历元/单位/坐标系/版本）必须逐项出现在要求列，否则 FAIL；并在 §5 合同登记 | 对 202 行注入「删历元维度」变体，check_standards_registry.py 必须判红（--fault-inject 负例面） |
| M2a-B-4 | 注册表 line 232 与台账 :223 同文但行号漂移；单点事实与 M2b-B-09 同源，本条独立面=登记面+测试覆盖 | 登记面宣称 CONFORMANT 且「复核在位」，但零占位路径(checksum=0)+verify 组合无用例，且 verify 把全零当通过（静默接受未计算校验和） | 补「占位卡×verify」负例/正例；verify 对全零 CHECKSUM 显式区分「未计算/占位」并给独立状态或错误码；注册表 §6 行降级为 PARTIAL 并挂偏差 ID | 注入写作 checksum=0 的产物，verify 必须有可判定的红/显式未计算状态；注册表 §6 行状态与测试面一致 |
| M2b-B-04 | 台账 :87/:95 漂移为 :95/:104；obs_bandpass 在注册表命中 2（104/114 行），仓内无第二真源 | 判定/接口面自定键集替代标准必填集，注册表条款锚与标准节号错挂，偏差指针 STD-F4 又指向「推荐键」缺失——三层口径互不对齐 | 按 IVOA HiPS 1.0 §4.4.1 九必填键(creator_did/obs_title/dataproduct_type/hips_version/hips_release_date/hips_status/… | 读端对缺任一必填键的 properties 必须拒绝（负例）；注册表 CLAUSES 节号与 IO_002 键集逐项一致 |
| M2b-B-07 | 目标 lib/common/healpix → lib/algorithms/shared/healpix（ARCH-001 #18）；台账 :136 漂移为 :145 | 公共 npix 无 checked 收口（回绕可复现），注册表把非标准条款(order≤29)写成 Górski 依据并判 CONFORMANT，测试还把 order 31 断言为合法 | npix/相关入口加显式上界与失败返回（fail-closed），注册表该行改述为 Project-defined 且挂偏差 ID，测试同步为越界必拒 | nside=2^31 注入用例必须显式拒绝（非回绕 0）；注册表条款锚不得含标准外条款 |
| M2b-B-08 | 台账口径 37 行/7 行/8 ID 已随注册表改版变化（现 43 行/6 行）；CLOSED=0 一腿已不成立 | 登记式仍不闭合：清单行判 CONFORMANT 却挂 TRACKED 偏差（6 行实测），检查器无「CONFORMANT ⇒ 偏差列为空/仅 CLOSED」判据；仅「零登记」「CLOSED=0」两腿已修 | check_standards_registry.py 增加 C9 判据：CONFORMANT 行的偏差列只允许空或 CLOSED ID，TRACKED/OPEN 偏差必须使该行降为 PARTIAL/PROJECT_DE… | 对 103 行注入「保留 TRACKED 偏差 ID 且判 CONFORMANT」变体，STD-REG 必须判红；43 行清单无 CONFORMANT×开放偏差组合 |
| M2b-B-10 | healpix §5.x 主题映射无法仓内自证，本轮仅以「C3 无映射表 + fits 同仓矛盾」成立 | C3 判据无「条款号→主题」映射表，故整域错挂可长期判绿；同仓 DATA_SEMANTICS/PHASE3_FITS_IMPL 已给出正确节号(§4.4.2.4/§4.4.2.5)未被引用 | 按 FITS 4.0 目录重写 fits CLAUSES（基本文件结构/基本头/扩展 HDU/表扩展/DATASUM-CHECKSUM 对应正确节号），healpix 行同步复核；C3 增加主题-节号映射表 | 对 CLAUSES 注入错挂节号变体，STD-REG 必须判红；注册表节号与 DATA_SEMANTICS/PHASE3_FITS_IMPL 引用一致 |
| M5b-E-05 | 台账「宪章全文 §[A-Z] 命中 0」一腿已升级为「宪章文件整体不存在」 | 活动 L0 文档引用已不存在的权威文件与其字母节号（§H/§F.1），发布权条款未改指现行权威链 | 改引 ASTROCS_DESIGN §0/§12（发布权）与 AGENTS.md §8；删除对已删文件的条款引用 | 文档引用门：活动文档引用的路径/文件必须存在（CHK-DANGLING 覆盖 docs/owner/**），字母节号不得出现在 L0 |
| M5b-I-08 | SCAN_FILES 中的 AstroCS_ENGINEERING_CONSTRAINTS.md 已不在 HEAD（同族 M5b-E-05/M6a-E-001） | 命名空间定义文件自身未纳入自校范围，doc-revision 值与其 front matter 不一致；检查器仍以文件/整行为豁免粒度 | VERSION_NAMESPACES.md 纳入 SCAN_FILES 或加 doc-revision 自校；豁免粒度从整行改为行内标记 | 对「本文档 = 1」注入与 front matter 不一致变体，VERSION-NAMESPACES 必须判红 |
| M6a-E-001 | 目标 runtime/module_loader → lib/infrastructure/pipeline/module_loader（ARCH-001） | 活动面注释/README 把不存在的旧世代标准章节当权威（悬空引用），清理工具对 8+8 截断哈希不设管 | 改引现行条款（ASTROCS_DESIGN §7.3/docs/contracts/ARC-001.md/ENGINEERING_SPEC §2），清理正则覆盖截断形哈希 | 活动文档/注释引用的标准文件必须存在于 HEAD（CHK-DANGLING 面）；清理工具对 8+8 形态有负例 |
| M6b-I-002 | 台账「evidence/performance、reports/self_review/round6 不存在」属旧路径沿用，残余面为上述三处 | 活动验证/发布页沿用已废止的 PASS 口径且证据指针不存在，与在册 known failure 并存 → 结论不可追溯 | 按现行状态阶梯重述（VERIFIED/PASS 须有在位证据指针），删除不存在指针或补建证据；known failure 未清前不得写 known P0/P1=0 | 文档引用门：结论句引用的证据路径必须存在；known_failures 与「P0/P1=0」不得并存（可机器判） |
| M7-G-102 | 两条行号 :236/:307 未漂移 | 推导文档自行宣告「无差异/不构成违反」而无 SHA/命令/日志证据，属越权结论（Agent 无权放宽或重新解释权威） | 改述为「与实现的一致性核对结果+证据(RUN/命令/日志/SHA)」或移交负责人裁决；偏差只登记不自我豁免 | 文档结论句必须附证据指针（命令+提交 SHA）；机器检查断言「无差异/不构成违反」类句式必须带证据锚 |
| M8a-B-001 | 台账「2.4.1 互斥」「astropy 冒名」两腿已不成立，本条按残余引用形式成立 | 冻结 SCI 文档把语义权威外挂到第三方软件版本（且自述非论文引用），无公式/推导位；版本字面量散落 SCI/头/工具映射 | 把 RCR 语义锚改为论文/公式位并说明与官方实现的一致性口径；版本字面量收敛到单一登记点 | SCI 引用项须为文献或公式锚（引用有效性门可判）；版本字面量单源且可机器比对 |
| SA-N-04 | 引用簇随 ARCH-001 迁移到 lib/infrastructure 与 lib/algorithms | 活动面引用已出库的控制包标准文本（悬空权威），改述未收敛 → 合规判据不可复核 | 把活动面引用改述为现行权威条款（ASTROCS_DESIGN §7.3/ENGINEERING_SPEC/docs/contracts）或加归档登记 | CHK-DANGLING 面覆盖注释/module.yaml：引用的标准文件必须存在或带归档标记 |
| V13-N-03 | 台账行号 :95 漂移为 :95-100 区间 | 审计状态由脚本按类型机械铸造（713/713）且无门校验，findings_p0..3=0/7 个 *_ok=PASS 同源 → 用登记掩盖红灯 | 铸造脚本改为从实际检查结果派生状态（或退役该 inventory），审核包只收机器可复核的清单 | 审计清单每个状态字段须有可复跑来源（命令+rc）；注入一个失败项，清单状态必须随之变化 |
| V6-N-10 | 台账文件现存(问题扫描/账本/FIX_LEDGER.csv)；本轮重算与台账口径一致（785/22/6/九提交交集空） | 账本的 fix_commit/regression_test 字段与实际改码提交不相交，账本无法作为追溯证据（REBASE_TABLE 785 条基线的可信度来源） | 对 9 个提交补登 fix_commit/regression_test，或按 REBASE.md 抬头把账本整体标为历史证据并在新登记面重建追溯 | 机器检查：账本中每个 fix_commit 非空行必须能 git show；改生产代码的提交须在登记面有条目 |

### CPU-001（17 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| FD-F-001 | 行号漂移(台账 :62/68/72 → 现行 :62/66/72)，目标哈希一致 tests/unit/cpu_provider_test.cpp@4aa984c0e0da | 门只匹配 .cpp 前 4096B 的 include 文件名字符串，不比对表内容/精度列；V14-N-03 实测 backend_table.inc 12 行 vs 实现 12 行中 6 行 precision 不一致… | 把守卫改为对 baseline_kernels_impl.inc/backend_table.inc 的内容哈希与逐行字段(kernel_id/precision_class)比对，并覆盖三个 backend TU | 注入一处 precision_class 漂移，cpu_provider_test 必须判红（能红能绿） |
| L28c-D-002 | 目标由 lib/backend_host → lib/infrastructure/benchmark/backend_host（ARCH-001 #16）；行号 40-42 未漂移 | 数值与「异常值剔除裁决」只存在于代码注释+常量：注释含任务编号 CPU-006 且该编号无现行文档；登记面(docs/contracts/ci)零命中 | 三阈值写入现行权威数值源(contracts/ 下的 benchmark 判据文件)与 benchmark/observability 文档，注释只留单位/统计理由 | 检查器断言三常量与登记面文件逐字一致；删注释中任务编号后仍可定位数值来源 |
| M5a-A-001 | ACR DORMANT（DESIGN §1.3/§7.1）故无运行值影响，仍属冻结文档缺陷；:85/:87 行号未漂移 | 冻结文档的误差界推导与自身数值不自洽（1e-7×32=3.2e-6 > 1e-6；再乘 714 更远离包络），且 max 判据被当作与像素数线性相关 | 改写推导为可复算式（给出 eps、累积步数上界与 max 判据的极值统计口径），或经 SCI 变更流程改容差 | 按改写后的推导复算得到 ≤1e-6 的界，且门在注入 1.1e-6 时判红 |
| M5a-C-001 | 目标 cli/** → lib/infrastructure/cli/**；根 CMakeLists.txt:637/651-655 显示该目录已在构建图内（RUBRIC §4 的「不在构建图」说法已过期） | cpu_profile 只进 show-effective 报告，run 路径的预算与 provider 选择不读 profile（恒 baseline）→ profile 选路对产品行为无效 | run 路径接入 profile 选路与 budget（或在文档/CLI 明确 --cpu-profile 仅为诊断报告项并从 run 命令取值面移除） | 注入 profile 指定 provider/workers，run manifest 的 provider/budget 必须随之变化（否则 CLI 帮助须声明不支持） |
| M5a-C-004 | tests/cpu/baseline/run_provider_oracle_checks.py:63-64 有 Python f64 参考（baseline 侧），avx2 侧无 | 变体科学门把「baseline 已被 f64 参考验证」当作传递独立性，未做误差合成，也未在变体侧设独立参考；文档声称与实现不符 | 在变体 runner 内加入独立 f64/Python 参考比对（或给出参考差与 2e-4 互比差的误差合成证明），ISA_VARIANTS:73 文本与实现对齐 | 对 avx2 变体注入数值偏差 ≤2e-4 但超 f64 参考容差时门必须红；ISA_VARIANTS 描述与 runner 通道一致 |
| M5a-G-010 | 目标 providers/cpu → lib/infrastructure/benchmark/cpu（ARCH-001 #16） | provider 在缺 executor 时静默退化为单线程整图且不上报降级，与 cpu_routing 的保守路由（≥1 但不退 1）方向相反 | 退化路径发显式降级事件/chain 记录（或按 available_cpus 自建线程预算），公共头不得把「无 executor=串行」写成合同 | 注入无 executor 场景，资源门/日志必须出现降级记录而非静默串行 |
| M5a-H-001 | 与 FD-F-001/V14-N-03 同源：该文件 12 行 kernel 表中 6 行声明 F64 而实现 F32 | 精度与零权重处理在实现里以 float 与裸阈值静默归零，未按声明精度执行、也未显式报告（UPM_SPMV 越界列同型） | 按 kernel 声明精度(表内 precision_class)执行并统一零权重/越界语义；静默归零改显式计数或错误码 | 注入 wsum=0 与越界列用例，输出必须带显式状态/计数；声明精度与实现精度一致（表↔实现逐行比对） |
| M8-A-001 | 文件已由 tools/monitoring/ 迁 tests/cpu/baseline/，行号 :111/:186/:221 未漂移 | 判定器自设容差与统计常数，未引用被判定文档的容差来源；失败分支为死代码；引用文件不存在 → 门的红/绿不承载科学判据 | 容差与常数改从被判定文档/合同读出（或写明误差合成依据），删除死分支，修正 docstring 引用 | 注入 1e-6 级偏差（文档容差内、2e-4 外）门必须红；docstring 引用文件存在 |
| M8-F-014 | 同族 V18-N-12（同一文件 :87/:109）保留独立 ID | 门对「AVX 不优于 AVX2」无任何断言，判据工件由被检测试自行生成 ⇒ 门不能红 | 把比值判据实现为真实断言（读 benchmark 产物或现场测量），判据工件由 benchmark 侧产出，测试只读 | 注入 avx 优于 avx2 的比值，该测试必须判红；测试不得写入自己的判据工件（机器检查 IO 白名单） |
| V14-N-03 | 目标 lib/backend_host → lib/infrastructure/benchmark/backend_host；与 FD-F-001 同源不同侧面 | 表内 precision 字段无消费者、守卫不判内容 → 登记面(表)与实现(provider)精度声明长期不一致仍绿 | 表与实现同源生成（或加逐行字段比对门）；precision 字段接入实际执行精度 | 逐行比对门：inc 与 provider 表 kernel_id/precision_class 全等，注入一行漂移即红 |
| V15-N-10 | 行号 :199/:212/:105 未漂移 | 判据建立在自由文本子串与析取式上（raised 或 verify、source_commit 或 invalid），hash 不符也能绿 | 改为断言结构化诊断字段/状态码（ENGINEERING_SPEC §9 统一状态码），删除析取式 | 注入「hash 不符」变体，测试必须红；测试内不出现 find(<自由文本>) 形态 |
| V15-N-11 | 行号 :56/:82/:94 未漂移 | 判据为文本锁与历史性能数字，且测试自行生成判据工件 → 门不承载当轮测量，「能红」只在文本变化时成立 | 改为读 benchmark 当轮产物并断言比值/共享源结构；历史数字从契约中移除或标为参考 | 删除被锁文本或改一处实际数值，门必须按语义判红；测试不写判据工件 |
| V21-N-01 | 路径 providers/cpu → lib/infrastructure/benchmark/cpu（ARCH-001 #16） | provider 入口把 status 塌缩为二值，宿主无法区分参数错/ABI 失配/不支持 → 诊断与退出码语义丢失（同文件 host_abi 边仍保留 ABI_MISMATCH，正对照） | 按 capability_v1.h 四值逐值映射到 ACS 错误码；宿主侧保留区分 | 构造 ERR_PARAM 与 ERR_ABI_MISMATCH 场景，宿主返回码必须不同（可区分） |
| V7-N-08 | 目标 cli/commands.cpp → lib/infrastructure/cli/commands.cpp（该目录已在构建图内，见 M5a-C-001 备注） | 探测失败无判别位、与真实单核同值 → 绿/红取决于一次静默探测失败，预算与 manifest 不可区分 | 探测失败返回显式错误/诊断位（不并入默认 1），manifest 记录 budget 来源 | 注入 sched_getaffinity 失败，manifest 必须出现探测失败标记且预算来源可区分 |
| W3-R2-005 | provider execute 面在 cli/lib 无直调（可达面为 tests/cpu oracle），故维持 P1 | acquire/release 不对称（跳过 acquire 仍 release、acquire 失败仍 release），且 release 无下限钳制 → Σactive 可为负，预算不变量永久失效（avx2/avx… | release 以「实际 acquire 成功量」为唯一入参（记录 granted），release 侧加下限钳制并断言 active≥0 | 注入 cap=1 与 acquire 失败场景，active_workers 恒 ≥0 且预算门读数非负（可复跑断言） |
| W5-N-02 | 目标 lib/backend_host → lib/infrastructure/benchmark/backend_host | 注释写 handled 而正文为空：解析异常与「键不存在」共用同一出口，回落 baseline 不留痕，注释与行为相反（§2 注释禁令） | 异常分支显式记录（chain/事件/错误码）并区分「profile 不可读」与「键缺失」；注释同步 | 注入损坏 profile，必须出现可查的回落记录（chain/事件）而非静默 baseline |
| W5-N-03 | 目标 lib/backend_host → lib/infrastructure/benchmark/backend_host | 解析异常与约束缺失不可区分且无记录 ⇒ 容器文本稍有出入即失去 cgroup 上限，按更大 workers 计算，无观测面 | 异常分支返回显式「不可读」状态并记录 chain；上层对「不可读」按保守上限处理（不按无约束） | 注入畸形 cgroup 文本：必须记录不可读状态并采用保守 workers（不得放宽） |

### NEXT-PACK:NP-DC-02（14 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| L28b-D-003 | aio_api.cpp 的 25 与实际 AIO_EXPORT=25 相符(原报"少报 1"不成立) | reader/writer 异常屏障注释的自报分母与块内实际导出定义数不符(少报 1/少报 3) | 分母改由脚本生成或删数字改为"全部导出入口", 并把计数纳入 CI 断言 | 新增断言: 注释分母 == 块内列 0 导出定义数, 或注释不再含数字 |
| L28d-D-001 | 原报"有效阈=2 倍"已被台账自身复算推翻, 只计单位注释写反 | 同址单位注释三处互斥: ipv_ransac.cpp:10 把 U 写成角秒, :1012 自述坐标系为角秒, 而 select.cpp:947/solver.cpp:12 实为像素 | 统一 ipv_ransac.cpp :10/:942/:1012 口径为 U 像素 / W 角秒(与 select/solver 及 MAX_DIST 角秒域一致) | grep -n 'U = 图像侧星点 (角秒坐标' ipv_ransac.cpp 归零, 且 residual 注释域与 s0/MAX_DIST 口径一致 |
| M1a-D-001 | 原报锚 :203-219 漂到 :236-244; solver 的 (无 +0.5 转换) 已不存在, 结论收窄 | 推导权威要求消费方按 +0.5 契约解读缓冲坐标, 但 C 接口头(唯一合同面)只写原点/轴向; 原报"无 +0.5 转换"字样已从 solver 删除, 残余为声明缺失 | 在 ipv_api.h 缓冲字段表写明 center=index+0.5 与 CRPIX 1-based 的关系, 与 PLATESOLVE §11.2 对齐 | grep -n '+0.5' ipv_api.h >=1 且与 PLATESOLVE.md §11.2 措辞一致 |
| M3-D-001 | 文件哈希=包内 38dfc45e9e68(工作树有并发修改标记但读时哈希一致) | 文件头保留整改前设计说明, 把不存在的 optimize_dark_scale 登记为"实现函数 3", 与同文件 :99 互斥; 真实现名 optimize_dark_k 且其 TU 在构建图外 | 改写 :5/:14/:15/:25 四处头注(删函数名与 16 线程措辞或注明历史设计未接线); 明确 dark_optimizer 归属 | grep -n optimize_dark_scale calibrator.cpp 归零 且 头注与 :99 一致(无 16 线程硬编码宣称) |
| M3-D-002 | docs/architecture/PUBLIC_API.md 已不存在, 原引用面失效 | 同一段头注"不降级"总括与"转 float 调用"细则并存; 被声明不受影响的函数恰在生产适配层被实调; 所谓已登记偏差在 docs/contracts 无落点 | 按实际精度分档改写头注(哪些入口全 double/哪些转 float)并把偏差登记落到 contracts | 头注不再自相矛盾; API-CAL-* 在 API_CONTRACTS.csv 有行 |
| M3-D-003 | NaN status→fit_status=1 与头注 1=rejected(status!=0) 字面一致, 本轮不计为缺口 | 公共头文档承诺的参数在实现中被唯一一次使用即丢弃, 结构体无处承载 star_id ⇒ 承诺的输出通道不存在 | 删除 star_ids 参数或补 star_id 输出字段并接线; 头注同步; 公共头补线程安全/生命周期说明 | 头注每条语义在实现可复现; 若保留参数则有消费者与断言 |
| M4-D-01 | 包内 upm.h:10/:176 现为 :10/:192 | 公共头注释未随 PHASE2_UPM/REJECTION 定义回写; workers<=0=auto 注释与两处 0→1 实现相反 | 删 SNR-aware 措辞; 统一 0 的语义(改实现走 budget 或改注释), 并补 0 值用例 | upm.h 无 SNR-aware; 0 值语义头/实现一致且有用例钉住 |
| M4-D-02 | low_fraction 0.1 与实现 0.2 的半句已登记 DISP-P2REJ-001, 本行不重复计 | 同一头 :107-108 与 :337 自相矛盾; profile 注释漏列 nullptr 实际解析到的 canonical 名 wbpp_2_9_1 | 头注改为 5.0/3.5 并补 canonical profile 名; 由阈值冻结断言反向校验注释 | 头注默认值与 rejection.cpp:1058 及冻结断言一致 |
| M5a-D-001 | sampler.cpp:880-882 自述 OpenMP 条件已移除 | 三个公共头把 0 定义为 auto=硬件并发, 两个消费点把 0 释为 1(串行), 语义互斥 | 统一 0 的单一权威语义: 消费点改用 effective_cpu_workers 或头注改为 0→1 | 头注与 sampler/upm 行为一致, 且 0 值有用例钉住 |
| M6a-D-002 | CAL 头 :15 与实现 :930 相距约 915 行 | CAL 文件头(与其实现相距约 900 行)语义相反; COS 已按 CAL 实现回写, DRZ 仍真降级且无偏差登记 | 改 CAL 头注为硬 BUDGET; DRZ 降级路径登记偏差或改硬失败 | 四适配器头/实现口径一致, DRZ 差异有登记 ID |
| M6a-D-004 | 与 M3-D-002 同址, 两行分别记总括互斥与 R10+结论前提 | 头注以 R10 任务号承载结论, 且"不影响 FP64 全链路精度"所依赖的"仅 orchestrator 旧通道不调用"前提被两条已接线 FP64 生产路径推翻 | 删除 R10 任务号; 按实际调用面重述精度结论并登记偏差 | 头注无历史任务号; 精度结论与生产调用面一致(可用 grep 复算调用点) |
| M6a-D-005 | 内部头 types.h 的 DISP-NOISE-002 如实登记未见更新 | 公共头未写注册表/生命周期/禁拷贝约束; 未登记键静默退 1e-12, 语义依赖 build 期建立的进程级指针键侧表 | 头注补注册表语义与所有权/生命周期; 未登记路径改 fail-closed 或可见告警 | 头注含注册表/所有权说明; 未登记路径有用例与告警 |
| M6a-D-006 | 原报 drizzle 宿主库名错在现树不成立: 符号在 lib/algorithms/drizzle/healpix_drizzle, 头注所指库名正确 | 头注停留在整改前委托关系: wcs 求解器已接线却写归 IMPL 任务, writer 头注写 aio_write_fits 而其 op 实为校验 hp_drizzle_run_phase1_hips 产物 | 回写头注三条映射(wcs/drizzle/writer)到当前实现 | 头注每条映射可用 grep 在实现处复现 |
| V8-N-02 | 包内 :3/:6/:30/:86/:155 行号未漂 | 宣称零本地副本/全部数值来自 C ABI 与同 TU 内三处本地数值实现(median_of、2.5*dex→mag、local_snr)互斥, 且列出的权威函数之一零调用 | 删宣称或把本地副本改为调用 C ABI; README 零公式副本同步 | 宣称与实际调用集合一致(可用 grep 复算: 列出的每个权威函数 >=1 调用, 本地副本归零) |

### P3-002（12 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M6a-C-001 | 台账锚 p3_resample:280-281 属 DEFERRED；本行按 module_adapters 侧在役路径取证 | 读路径层级仍由 properties.hips_order 决定，SCI §9a-5 的 leaf_order=order_sel+log2(W) 未实现；provenance 记计划值而非实际读取层级 | 读路径按 order_sel 计算 leaf_order，或 provenance 明示实际读取层级（二者取一并登记） | 变更 hips_order 而 order_sel 不变时，provenance 记录值与实际采样层级一致 |
| M1a-C-007 | 台账锚 lib/phase3_session 与 ALG 文档并存；本轮以文档侧为缺陷主体 | 合同文档仍以待删会话模块为「生产源」且行数三年未复测；§1 非目标已被 lib/algorithms/resample/p3_rsmp_propagation.cpp、p3_rsmp_covariance.cpp 的实现… | 更新 PHASE3_RSMP_IMPL.md §1/§3 生产源图与非目标，改指 lib/algorithms/resample/** 与 V6_PHASE3_RSMP_IMPL.md | 文档「生产源」声明与 wc -l 实测一致；§1 非目标与生产符号表一致（variance/ivar 路径存在） |
| M1a-D-003 | 会话部分属 DEFERRED；本轮按文档侧残余判 OPEN | 会话源码侧审计流水随 INT-001 删除即消；文档侧「非目标」与 lib/algorithms/resample 现产源（p3_rsmp_propagation/covariance）相反，双源未订正 | ALG §1 非目标按 lib/algorithms/resample 现产源改写；会话文件随 INT-001 删除 | ALG §1 与生产符号表一致；ENGINEERING_SPEC §2 审计流水在源码注释中零命中 |
| M1a-F-006 | - | 测试自造本地参考实现，与生产采样符号无接口对拍；合同仍把该文件列为「现有证据」 | 合同 §12 证据列改指 v6 线 tests/unit/v6_p3_rsmp/p3_rsmp_oracle.h，或补生产符号对拍 | 测试内出现生产采样符号引用（grep -c p3_sample_bilinear* > 0）且断言可证伪 |
| M1a-H-001 | 台账行锚 5589/5591/5690 → 现 5591/5593/5692；同函数 uncertainty open 失败置 corrupt=-2（fail-closed 不对称）仍成立 | worker 内采样器打开失败仅 return，不置 corrupt、不记失败带；band_task 在 worker 返回后无条件 +1，故行带计数门恒过，节点可发布无数据的「成功」产物 | 失败分支置 corrupt 并让该行带不计入 executed（或直接 fail-closed 中止） | 负向注入（HIPS 目录不可读）后 resample 节点 rc≠0 且不产出 p3_resampled.bin/json |
| M1a-I-003 | 台账锚 lib/phase3_fits/README.md 已迁 lib/algorithms/fits_output/README.md | 同一合同族内「已删除/已闭合」与「仍作成本项/现状恒 nullptr」并存，会话路径与节点路径语义未分列声明 | 逐处标注适用路径（会话 p3_session vs 节点 p3_op_writer），删复杂度段 fdatasum 成本项 | 三份文档对 fdatasum/manifest_hash 的表述一致且可逐条复跑核 |
| M6a-I-001 | - | 文档自述行数与状态码产生点未随实现漂移同步，claim↔evidence 破裂（coverage README、projection memory.md 等多处） | 逐处重测行数并对状态码产生点重锚；删除「现无产生点」表述 | 文档自述行数 == wc -l；状态码产生点声明与 grep 结果一致 |
| M7-F-201 | 【人工复核覆盖：原分批复核判 OPEN】台账原判 OPEN/P2/F_TEST_GAP。 | 三个点位只改了两处：PHASE3_RESAMPLE.md 已改 ±k·ULP，PHASE3_FITS_IMPL.md:325 仍以「构造保证」充当容差 | 把 :325 的「构造保证」改为「Σw = 1 ± k·ULP（FP64 累加；不作逐位断言）」并补一条能红的 CTest（常数场 bilinear /Δ/ ≤ k·ULP·/C/） | docs/algorithms/** 内 构造保证 零命中（grep + rc=1）且新增 CTest rc=0 能红能绿 |
| V15-N-14 | 台账行锚 p3002:275 复现；同形态另两处为 M8-F-007 已立案 | 恒真断言使写后读回类 CHECK 永绿，注入故障（ASTROCS_P3002_FAULT=skip_hdu）也不影响助手返回值 | 删去 // true 并让助手返回真实状态 | 注入 ASTROCS_P3002_FAULT=skip_hdu 后该测试变红 |
| V20-N-08 | 台账行锚 5305/5306 → 现 5334/5335 | 两个字段可配域为单点（frame∈{icrs,ICRS}、coverage_output=mask）且全仓零读者；manifest 因此记录计划值而非实际观测 | 删除零读字段，或接线消费并在 manifest 回显实际值 | 两字段有读取点，或从 P3nGeom 移除；manifest 字段有实际值来源 |
| V20-N-12 | 台账行锚 5885-5916/5999 → 现 5931-5952/6028；读时并发线在改，module_adapters.cpp 行号本轮内已漂移（md5 79b2b2013ebb2df50169595e707dce… | 读键 ⊄ 上游产键集：p3_verify.json 的该 provenance 字段恒空串，5990 附近注释仍自称「透传真实值」 | 写入侧补 module_build_id（desc_.module_id + ASTROCS_VERSION_STRING，与节点 manifest 同源） | p3_verify.json 的 module_build_id 非空且与节点 manifest 一致（测试断言该键值非空） |
| W2-N-11 | 台账行锚 3085-3087/3104-3116 → 现 3120/3149；读时并发线在改，行号本轮内已漂移 | 重构式 diag(CD1_1,CD2_2) ≠ 原 CD（PA=30°/s=1.1e-5 时 abs(det′)/abs(det)=0.7500，等价 CROTA2 应为 150.0° 而非 0） | 帧头不再写由 CD 派生的 CDELT/CROTA，或写正确的等价 CROTA2 | 外部 WCS 库分别按 CD 与按 CDELT/CROTA2 解释得到的几何一致（角距差 ≤1e-9 deg） |

### ARCH-001（11 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M5b-E-04 | - | 头注释悬空引用不存在的机器检查；同名枚举在 legacy 与 abi 两头族各定义一次，语义由 TU 选哪份头决定 | 补 tests/abi/run_abi_checks.sh 或改注释指向现存检查；两族枚举收敛为单一源 | 注释所指脚本存在且至少一条能红能绿的检查；acs_status 定义点=1 |
| V11-N-03 | 台账锚 108-111 → 现 78-81 | Python 以 24B 步长解释 C 的 32B 数组：i≥1 星坐标静默错位，无 rc 无告警，PSF-001 归因证据图的产错性质不变 | 补 flux_min/flux_mul 两字段，或退役该 diag 并在证据条目加「仅第 1 颗星可信」失效声明 | ctypes.sizeof(GaiaSpectrumStar)==sizeof(C 结构)==32 |
| V11-N-06 | 台账「严格相等 8 处 / ==0 放行 3 处」口径复核成立（本轮抽样 3 处） | 头文件规定的 peer≥self 兼容规则几乎无人用；三种会话入口+装载器按严格相等、runtime/io 按零放行，语义由调用点决定 | 统一为头文件声明的兼容规则（peer≥self 或严格相等）并在头注登记 | 三种入口对同一 struct_size 行为一致；负向注入尺寸不匹配 → 一致拒绝 |
| V11-N-07 | - | 被跨 DLL/跨语言消费的 8 个公开头不在检查语料内，检查对真实 ABI 边界零覆盖 | 把 8 个头纳入 HEADERS 并支持匿名结构扫描（现正则要求 typedef struct 具名 tag） | 改动任一头字段后该检查 rc≠0（能红） |
| V11-N-08 | - | 两份字段顺序相反的握手结构并存；orchestrator 只比 5 项却在日志打印 struct_size，enum_fingerprint 全仓无比较点 | 收敛为单一握手结构，或声明两者的显式转换关系并纳入门检 | 交付路径使用的结构与门检结构同型（字段顺序一致）；enum_fingerprint 有比较点 |
| V11-N-09 | 台账 10 个（5 .def + 5 .map）计数复现 | 5 个被跨语言消费的目录导出面仍等于 *_EXPORT 全开（无白名单）；公共 ABI 头住 src/ 逃过任何 include/ 面普查 | 为 5 个 DLL 补 .def/.map 导出白名单；gaia_client.h 移入 include/ | 改动导出符号后 ABI 检查能红；gaia_client.h 位于 include/ 面 |
| V11-N-10 | S-1/S-2 分母口径已变（include/ 现 23 文件），已按新布局复核 | 漂移面已从 7 处收窄到 GaiaSpectrumStar 一处，但布局锁未注册为 CI 项 ⇒ 「零采集 ⇒ 必漂」的单向包含关系仍在 | 修 GaiaSpectrumStar 镜像；把 aio_abi_layout_lock 注册进 ci/checks.json | 全仓 ctypes 镜像 sizeof 与 C 头一致且为注册检查；篡改字段后门能红 |
| V12-N-01 | 台账 23 文件口径本轮复现 | 科学常数无单点权威头，权威点数=0，架构上无处写对；各域继续裸字面量复制 | 建 include/astrocs/science/constants_v1.h 并把各域常数改引用 | 每个冻结常数定义点=1 且被 CI 常数一致性门覆盖（改一处即全局生效） |
| W1-N-05 | 台账对象 lib/healpix_db/healpix_drizzle 已迁 lib/algorithms/drizzle/healpix_drizzle | 新边界结构未继承 ASTROCS_DESIGN §7.3 的四件套；DLL 新旧错配无运行期自拒（同 V2-N-01 同类错位复现路径） | 为该结构补 struct_size/abi_version 并在导出入门校验尺寸 | 头内该结构含两字段且入口拒绝尺寸不匹配（负向注入能红） |
| W5-N-07 | 台账行锚 status_codes.h:210-211 复现 | 声明即链接失败：无任何 TU 提供定义体，Python 侧亦无名值映射；同族 artifact 有生产实现、lifecycle 仅测试探针实现、status 全无 | 补生产实现（与 acs_artifact_status_name_v1 同规），或删除声明 | 该符号有唯一定义体且被 ABI/悬空引用检查覆盖（改声明即门变红） |
| W5-N-08 | 台账行锚 :299-315/:317-329 复现 | 表缺项（10 落 UNKNOWN）+ to_result 把 10/7/9 折叠到 INTERNAL，语义由 TU 选哪份头决定；与统一状态码要求冲突 | 名值表改用 abi/status_codes.h 单源（含 10），to_result 补 10/7/9 的域映射 | 表覆盖 status_codes.h 全部枚举值；负向注入 10 → 非 UNKNOWN 且域正确 |

### P2-002（10 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M2a-F-6 | 文档命中数由台账 2/2 变为 4/3（并发线已改） | 文档两处数值无门可红；TU 未注册构建 | 把 ρ 断言写成 CHECK 并注册进 ctest | ctest -N 含该测试名且注入相关性破坏后判红 |
| M3-F-005 | 占位路径由 lib/phase2/tests 漂到 lib/algorithms/coverage/tests（仍为 ... 占位） | SCI-CW 族无任何可执行 TEST-*；ALG-CW-001 无独立文档（INDEX.yaml 指回同文件） | 落 TEST-CW-001..008 并补 TRACEABILITY 行 + 拆出 ALG-CW 文档 | TRACEABILITY.csv 含 SCI-CW-001..008 且 TEST 路径为真实文件 |
| M4-A-03 | 行号 829→847 | 实现以 1e-12 零尺度 floor 绕过「不计 control」语义，SCI 无该 floor 条款、无 DISP 登记 | 或按 SCI 跳过该 control，或把 floor 写进 SCI 并登记 DISP-P2SMP | MAD=0 patch 的 control_ivar 行为与 SCI 条款逐字一致（新增门判红） |
| M4-C-08 | - | 错误码在中间层被归一；公共 ABI 参数校验面小于 ALG §10 承诺 | 原样传播 raw_weight rc；calibrate_block 补 leaf_ipix 判空 | 缺 control_ivar ⇒ rc=2 直达调用方；leaf_ipix=nullptr ⇒ rc=1 不 UB（各 1 负例） |
| M4-F-02 | - | 文档承诺的独立参考实现不存在 | 落 NumPy Huber IRLS + control_ivar 复算脚本并注册，或把该句降级为计划项 | 存在 rtol 1e-9 的被采集参考对拍并能判红 |
| M4-F-05 | drizzle 源目录已迁至 lib/algorithms/drizzle/healpix_drizzle | 门无判别力（不锚 1.3883/1.4）且未注册，SCI 的「硬门」宣称无可执行证据 | 门改锚 1.3883/1.4 容差，或按 R-2 收进 ctest 并限时 | ctest 采集到该 MC 门且注入 k_corr=0.99 判红 |
| M6b-C-001 | - | 文档未标该路径为 legacy/显式开关（实现默认 fail-closed） | 两文档改用「仅 legacy_allow_weight_fallback=true 的显式降级路径，生产禁用」 | 文档与 §4.3/UNIFIED_MODEL §2 口径一致（lint 门） |
| M7-A-113 | 读时哈希 UNCERTAINTY_AND_COVARIANCE.md=918c1032369e（包内 122fcdce4423） | 渐近式无成立域，小 N_retained 时低估 | 声明适用域并给小 N 降级路径 | N 小于域界时走登记路径（新增门） |
| M9-B-2 | docs/standards/STANDARDS_REGISTRY.md 现存在（372 行，权限 600），非退役对象；读时哈希 f404cbf8f58b（包内 c026817d6f93） | 方向/符号口径只存在于代码，注册表与 DATA_SEMANTICS 未声明；无独立对照 | 在标准注册表/DATA_SEMANTICS 声明 CDELT+CROTA2 方向与符号；补 CROTA2≠0 对照用例 | CROTA2=30° 用例与 FITS WCS Paper I §3 解析值一致 |
| W2-N-05 | image_corrector.cpp 读时哈希 400f039c3bf9（包内 d10aac8def74）；correctImage 调用者仅 lib/algorithms/photometry/cpp/src/pc_… | 乘性标定未同步缩放方差，跨帧 ivar 求和失同单位前提（高标定帧权重少计） | 在 scale 路径同步 variance×α²、ivar÷α²（或显式标该路径不产 variance） | 标定后 variance/ivar 与 α 的平方律一致（新增门判红） |

### NEXT-PACK:NP-DC-01（7 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| L28b-D-001 | 包内报 14 处, 本次读时 8 处; 文件哈希=包内 5a41c3851093(sha256 前缀一致) | 常量块把已删除/仓外 star_finder.c 的行号当锚((line 47)落空行, 48 才是 TWO_SQRT_2_LOG2; (line 50)落 struct), 全树无宿主文件 | 删除 8 处裸行锚, 改为可解析宿主(算法文档 §/符号名), 或把常量块并入单一常量头 | grep -c -E '\(line [0-9]+\)' lib/algorithms/star_detection/src/sdet_api.cpp 归零, 或每条锚 grep 可命中被标注符号 |
| L28b-D-002 | 包内 baseline_provider_v1.h:97/98 现漂到 :98/99(内容未变) | kernel 表把 ALG-008/009 编入 astrocs_cpu, 但 docs/traceability 八层矩阵无这两行; docs 内命中全是版本标签而非算法登记 | 在 TRACEABILITY_MATRIX 登记 ALG-008/009(或改 kernel 表引用已登记 ID), 并把 baseline 头注逐 op 公式迁到 ALG 文档 | TRACEABILITY_MATRIX.csv 含 ALG-008/ALG-009 行, 与 backend_table.inc 条目一一对应 |
| L28b-D-005 | check_comments.py 已在 ci/checks.json:1411 注册(可复跑) | 占位串自带 SCI-/ALG- 前缀使文件级子串门恒绿; 字段真值无注册面校验 | 门改为按具体 ID 形态与矩阵存在性校验(能红能绿); 头注占位示例改为真实登记 ID 或标注示例 | 注入 'SCI-xxx' 时 check_comments.py 变红; contract id 字段与 TRACEABILITY_MATRIX 比对脚本在册 |
| L28c-D-001 | 包内 avx2:47 现漂到 :48-49(内容仍在) | 唯一逐 op 槽位/公式载体仍是 baseline 头注释(非权威文档); ALG-008/009 矩阵零登记却随 backend_table.inc 进编译单元 | 槽位/公式迁 docs/algorithms 并在矩阵登记 ALG-008/009; avx2 头注改引用文档 §而非他头文件注释 | avx2/baseline 头注不再以另一头注释为权威; TRACEABILITY_MATRIX 含 ALG-008/009 |
| V8-N-01 | 精确路径保留有负责人裁决 2026-09-14 依据(注释 :1813-1816), 本行只判符号虚构 | 注释引用的 :: 符号整体不存在(非行号漂移): 真实用例名为 test_psf_fast_cap_and_inactive_precise | 改注释指向真实符号, 或按注释补直调用例; inactive 精确路径的保留凭据需可 grep | 注释内 :: 符号可解析(>=1 命中且已注册) |
| V8-N-04 | 本轮同口径重算 lib/ 内 5 处(包内报 3 处) | 相对锚 REPORT.md §N 指向不存在的根文件, 而承载的恰是最需凭证的宣称(逐字节不变/末位 ulp/耗时) | 改为带路径的可解析锚或迁入现行报告; 裸文档名+§N 加入机器门 | 所有 REPORT.md 引用带可解析路径且目标存在; 注入裸锚时门变红 |
| W1-N-02 | 包内两处注释行号(:72 / :36)未漂 | 两符号全树无定义, 注释虚构的直调形态与测试真实形态(经生产注册表 register_phase_modules 端到端跑节点)互相矛盾 | 改注释为真实入口形态, 或补直调钩子并在注册面导出 | 两符号 grep 命中均有定义/导出; 注释与 tests/unit/p1snr 实现一致 |

### CLI-003（6 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M5b-C-05 | - | 文档未随单一退出码源更新；映射函数把 RESOURCE 归到 BACKEND=5 | ERROR_MODEL 改为引用 lib/infrastructure/cli/exit_codes.h 11 码表；:398 改 return 10 | CHK-CONTRACT-TEST/check_api_docs 对退出码唯一源与全码映射全覆盖且能红 |
| W3-R2-007 | 同仓正对照 ci/run.py:691-696 走 os.killpg 树杀 | 进程创建未建进程组，超时只 kill 直接子进程；协作取消与 tmp 清扫被跳过 | POSIX setpgid+killpg、Windows Job Object 树杀；先 SIGTERM 宽限再 SIGKILL | 子进程再派生孙进程时超时能整树回收（能红） |
| W4-R2-04 | ENOSPC 时序需运行期；行号系本轮实测 | 错误信号被吞，失败仍原子落位或写非法哈希 | 显式 flush+close 后判 good；create_directories ec 判错返回；sha 失败返回错误而非空串 | ENOSPC/sha 失败注入下 run 返回非 0 且不落半成品（能红） |
| W5-N-04 | (a) 解析失败静默丢弃致 DATA→2/IO→7；(b) 多节点 run 串扰 | 解析失败静默 continue；判定主体为全节点集合 | 解析失败按 IO/INPUT 显式返回；归因限定失败节点 manifest | 坏 manifest 注入下退出码稳定且归因正确（能红） |
| W5-N-05 | 6 处 json::parse(cfg_text)，行号已随 CLI-001 重定位（原 :888 等 → 现 :749/:1003/:1105/:1338/:1696） | 解析失败静默回落 CWD，无 warning 事件/stderr；信任边界后重复解析 | catch 分支返回错误（rc=2/3）或显式 warning+落 run/；复用已解析文档 | 坏 config 时 run 非 0 且不落 CWD 产物（能红） |
| W5-N-10 | 台账原报 commands.cpp:306/309 的 rc=124/127 已随 RUNTIME-CI-001 移出，约定改驻 process.cpp | 子进程 exec/chdir 失败用 shell 约定码；唯一源路径与文档/DESIGN §6.3 不一致 | 126/127 映射为 INTERNAL=70（或登记为子进程内部约定，不进 CLI 退出面）；订正唯一源路径 | CLI 退出码恒在冻结集内（能红） |

### P3-001（6 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M1a-A-010 | 台账目标 lib/phase3_session/p3_wcs.cpp 属 ARCH-001 DEFERRED(不迁移删除)；本轮在同款缺陷所在的新模块 p3_proj_v6.cpp 取证 | 在役 v6 沿用 legacy 的 r≥π/2 作半球判据：r=cotθ ⇒ r≥π/2 ⇔ d≥arctan(π/2)=57.519°，而 world2pix 用 stheta>0 ⇔ d<90°，两向有效域不对称，5… | 正向判据改为 θ>0（denom>0）单一口径，或按 Paper II 声明 d<90° 并同步 DATA_SEMANTICS §28 文本 | v6_p3_proj 往返门覆盖 d∈(57.52°,90°) 用例；DATA_SEMANTICS §28 删「数学等价」表述 |
| M1a-F-004 | - | 文件头承诺的容差与断言容差相差 10 倍；常数场不变量用 std() 替代 max_abs==0（可被非零但零均值扰动绕过） | 统一头注与断言容差并登记依据；常数场改 max_abs==0 断言 | 头注容差==断言容差；常数场不变量以 max_abs==0 判且注入非零扰动可变红 |
| M1a-I-002 | 原三处 p3_wcs/p3_projection/p3_proj_v6 复现；p3_proj_v6.cpp 为在役新模块 | 合同承认编译期覆盖可放宽 SCI 冻结上限（W/H∈[1,20000]），构建面仍无任何 target 定义该宏，口子在三处源文件复制 | 合同删「编译期覆盖」表述，或把覆盖入口收归配置合同并走变更流程 | 该宏在构建面定义点 = 0 且合同不再承认可覆盖（或覆盖入口有登记与门） |
| M2a-I-1 | 解析后目标 lib/healpix_db/healpix_drizzle 已迁 lib/algorithms/drizzle/healpix_drizzle | 常数真实归属域是 GAIA cone 查询（GAIA_QUERY §2.5），仍被登记在 DRIZZLE.md 符号表定义面，域界未清 | 从 DRIZZLE.md 符号表移除该行并交叉引用 GAIA_QUERY §2.5 | drizzle 侧文档对 C45/Lipschitz 零定义；GAIA 侧为唯一定义点 |
| M6a-D-012 | 台账对象 lib/phase3_proj/p3_projection.* 已迁且 RETIRED；同款在新模块 p3_proj_v6.cpp 复现 | 两个 spec 字段的唯一读取点是 selfcheck 的 >0.0，make 实际用文件级 kMaxAbsDec ⇒ registry 声明与强制面脱钩（latent 一致性缺陷） | make/validate 改为读 registry spec 字段（max_abs_crval_dec_deg / max_fov_deg） | 改 registry 某行 max_abs_crval_dec_deg 后行为随之变化（门能红） |
| M7-G-101 | SCI 侧已加「会话仅 TAN 与 registry 八投影不冲突」的收窄声明（PHASE3_HIPS_TO_FITS.md:56-59），残余在注册表 | 注册表仍按四投影登记 CONFORMANT 且偏差列「无」，未登记 DESIGN §5.3 八投影要求与 v3 实测 4/8 的差距（GAP-011） | 注册表 D.spherical-projection 行改按八投影重述，并把 4/8 差距登记为偏差指针 | 注册表投影条款与 DESIGN §5.3/ALG §15 一致；偏差列非「无」 |

### CLI-001（5 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M5b-E-03 | schema 路径半已修 | 文档未随 ARCH-001 迁移更新唯一源路径；检查器候选回退掩盖失配 | 文档改指 lib/infrastructure/cli/exit_codes.h；检查器删除不存在的候选或对文档路径显式校验 | 文档声明路径 test -e 通过；检查器在文档路径错误时 FAIL |
| M5b-G-15 | - | 文档承诺的协议检查器未建立，命令树/退出码唯一源无对应门禁 | 建立协议检查器（§6 六条）并注册 CI，或删除 §6 承诺并显式退役 | 新建检查器能红能绿并注册；否则 §6 退役登记 |
| M6b-I-001 | ARCHITECTURE.md/API_REFERENCE.md 已归档（该半已修） | 旧文档体系迁移只覆盖部分文件，ACTIVE 面仍有废止入口表述 | 按 DESIGN §6.2 改写 docs/modules/phase2.md 与 TROUBLESHOOTING 入口行（或一并归档） | ACTIVE 文档中 orchestrator.exe/astrocs-stage2 非「现状/生产入口」表述；CHK-STALE-DOC 覆盖 |
| M8a-C-001 | - | 旧 orchestrator 文档未随单一 CLI 架构退役 | README 加 ARCHIVED/DORMANT 标注并指向 §6.2 命令树，或按 INT-001 处置该目录 | ACTIVE 文档中 orchestrator 不得自称唯一入口；产品清单/构建图一致 |
| V15-N-15 | - | 门断言迁移过渡态（production exe=0）而非交付对象（唯一 astrocs 入口） | 改为断言「清单恰一条 production exe=astrocs」+ install 树唯一入口 | astrocs 登记为 production exe 时门仍绿；出现第二个 production exe 即红 |

### CFG-001（4 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M2a-B-3 | 目标 lib/drizzle → lib/algorithms/drizzle；module_entry.cpp:355 的 API-DRZ-001 锚已改为「契约只约束 (0,1])」 | 接口层注释/校验用 [0,1]，引擎与 SCI 用 (0,1]，拒绝时机被推迟到引擎层（fail-late）；偏差已登记为 DISP-DRZ-003（PUBLIC_API.md:264 亦自认）但代码未改 | 模块入口校验改 pixfrac<=0 即拒（NO_DATA），统一 hp_drizzle_api.h:37/126 与 types.h 注释为 (0,1] | negative 用例 pixfrac=0.0 在 validate/plan 阶段即返回 NO_DATA；三处定义域文本逐字一致 |
| M2a-C-6 | module_entry.cpp 的 API-DRZ-001 锚注释已改；PUBLIC_API.md API-DRZ-001 节(:207-262)确无默认值条款 | 同一配置项在两个入口各自定义缺键默认且数值不同（0.8 vs 1.0）；科学默认值来源空缺已由 config/defaults.json 以 pending_authority 占位承认但未收口 | 以 config/defaults.json 为唯一数值源生成两入口默认；默认值经 R-005 定权威或显式标 pending_authority 并在两岸同步 | config 校验门断言 code 内不出现 pixfrac 字面量默认（除生成物）；两入口同缺键场景输出一致（oracle 对拍） |
| M3-C-009 | 台账 :957-974 与现行 :957-958 同址；json_config.h 哈希 d3147c6fd848 一致 | A 线编排入口把暴露的配置键与恒定分支解耦：dark_opt/dark_k 是常量、曝光键仅告警比较，IR 通道已有 dark_optimization 与 k 推导（module_adapters.cpp:1280-1… | 删除或接通 A 线 dark 配置分支（与 IR 通道同源）；未接通前把键标为 NOT_IMPLEMENTED 并按 §12 明确报告 | 配置键要么有消费分支与 oracle，要么在 schema/文档标 NOT_IMPLEMENTED 且机器检查可判（键=实现态一致） |
| M3b-C-05 | 台账路径 lib/star_detector → lib/algorithms/star_detection；:31 行号未漂移 | ALG/module.yaml 宣称的消费路径不存在（4 字段零读取），默认值与生产硬编码两套值并存 → 声明与实现脱钩 | 删除未消费字段或接通；硬编码处改从参数/默认值单源取 | 参数或删或通：机器检查断言 Params 字段均有读取点（生产 TU 内非默认赋值） |

### NEXT-PACK:NP-DC1（4 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M2a-D-3 | drizzle_engine.cpp:1890 行号未漂; writer 侧另有 :1139 同簇 | 整改批次号/审计流水是注释主干且带已漂移的文档行号锚(§8 :96) | 按 §2 一次瘦身: 删批次号, 结论改指 SCI/ALG 文档 §锚; B*/P1-*/R10 模式加入机器门 | grep -rn -E 'P1-DRZ-/B2-A[0-9]+/ORACLE_HARDENING' lib/algorithms/drizzle lib/infrastructure/aio/src/hips 归零(或仅… |
| M2a-D-4 | tests/unit/gaia_cat_test.c 的 KI-1 打印属另一行(不重复计) | 头注与 CMake 依赖句描述的实现形态已被 acos 点积式改写却未回写; tail_keys 为死代码残留 | 回写头注(:11/:17)与 CMakeLists:100 依赖句; 删 tail_keys 并复验构建 | grep -c sincos src+CMakeLists 归零且构建通过; /dec/>85 句删除或实现补分支; tail_keys 0 命中 |
| M6a-D-009 | 本轮数字口径与原报不同(旧路径已迁移), 不复用原报 67/39/43/63/27 | 整改批次号/commit 短哈希仍是注释主干; 仓库有 check_comment_hygiene.py 但未注册进 ci/checks.json, 无在册机器门 | 一次性瘦身 + 把注释卫生规则注册进 ci/checks.json(能红能绿) | ci/checks.json 含注释卫生项且注入样例可令其变红 |
| M6a-D-015 | ASTROCS_DESIGN.md:380 hips_browser/ 未来 GUI 可视化组件（不进产品 manifest）; 无单独退役记录, 不判 NOT-APPLICABLE | 头注残留私有线性约定[y*512+x], 与自身实现(511-x)*512+y 及唯一权威映射均为转置+翻转关系; 模块不在根构建图故无产品后果但文本仍错 | 头注改为与 healpix_core.h 权威映射一致; 或在模块 PENDING/文档标注不维护 | backend.h:9 与 healpix_core.h:44-49 口径一致(或模块显式标注不维护) |

### INT-001（3 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M5b-G-05 | 台账行锚 25-26 → 现 25-27 | 检查器不读根 CMakeLists/install_layout/File API，只做字符串存在性检查；BUILD_GRAPH.md 仍不含实际产品目标名（文档订正反而变红的结构未变） | 检查器改为从根 CMake/install_layout 派生目标集合并与文档比对 | 检查器能红能绿（改目标名即红）；BUILD_GRAPH.md 目标名与构建图一致 |
| V10-N-08 | 台账订正：orchestrator 的 A_ORDER 腿现已带 SNR_SIP_MAX_ORDER 界，残余为出界整段静默跳过 SIP | 同一 hips_order 字段在 aio 读面与 hp_drizzle_api 已夹界（后者越界 return -10），此两面 atoi 结果直进移位；两子树均不在根构建图，补注册即红 | 补 IVOA HiPS order 值域判定，读侧复用 aio（禁止自带 properties/FITS 解析） | 越界 order 输入显式报错；两子树入图后负向注入门能红 |
| W5-N-09 | engineering/ 目录存在但为空（0 条目）复现 | 错误码登记事实源文件不存在 ⇒ 悬空权威引用，AstroCsExitCode 与 CLI 码段之间无分配表载体 | 建 engineering/contracts/error_code_registry.csv 或删除两处引用 | 引用路径存在且被悬空引用检查覆盖（删除该文件即门变红） |

### NEXT-PACK:NP-02（3 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M7-T-103 | :301 删除线条目已在; 缺的是正向断言与新锚 | 缺陷侧已 fail-closed 但测试设计仍把旧行为列为断言锚, 且正向断言(空/缺键拒绝)无实体用例 | 文档 :344 改锚拒绝行为; 补 empty/缺键 filter 的 rc=1+载因用例 | grep -rn 'missing obs_filter' tests/ >=1 且用例在册; 文档不再以静默放行为锚 |
| M7-T-104 | 原报 upm.cpp:1353-1354 锚已漂(该处现为 legacy ablation), 现 payload 在 :1386-1388 | 全部用例只取默认 G=8; geometry 身份只含 UPM 自身 order/grid/cell, 采样器实际 G 不入 hash; 已有拒绝分支无测试钉住 | 补 G∈{1,3,5,16} 正/负用例(含越界拒绝), 并把采样器 G 纳入 geometry 身份或显式声明不入 | ctest 内 G≠8 用例 >=1; hash 口径有断言 |
| V12-N-10 | 漏星是否实际发生需运行期+真实 XPSD 树, 本行只判静态三事 | 头注保持无假阴性与权威文档经验保守，非数学证明互斥; 1.2 裕量所依赖的赤道树赤纬覆盖域无文档数值也无断言; 赤道分支无 AE 分支同款边界回退守卫 | 头注改经验措辞或补证明; 补赤道覆盖域声明与锥触极回退; 裕量值加断言 | 三处文本口径一致; 边界(锥跨极//dec/>85)有用例或显式保守声明 |

### NEXT-PACK:NP-GAIA-01（3 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M9-G-1 | 包内"函数首行 memset"现为 :1194, 校验失败点 :1219 | 失败 return 前无清理路径, memset 使已建立的映射/句柄失联; 调用方仅在 rc==0 时 close ⇒ 泄漏不可回收 | load_xpsd_file 统一 fail 标签(先 close_xpsd_file 再 return), 调用方失败分支也 close; 失败路径加用例 | 注入坏魔数/短头后重复加载 fd 计数不增长(或 LSAN 干净), 用例在册 |
| M9-G-2 | 原报 :1333-1334 现漂到 :1334-1335 | 分配失败 break 跳出的是解析所有 Tree 的循环, 出口仍返回成功; 被丢树天区在锥查询中静默为空, 与"该天区无亮星"不可区分 | 记录失败树并置错误/告警位, 或按 fail-closed 返回错误; 补部分树失败用例 | 注入受限配额后查询返回错误或可见告警计数>0, 用例在册 |
| M9-G-3 | 探针 = 同构循环提取(非整函数), 落 run/PROJECT-GOVERNANCE-01/SCAN-CLEAR/logs/g3_probe.c | strtod 未消费字符时 endptr 不前进而 count 仍自增, 计数循环无零推进保护 ⇒ 活性挂死 + int 溢出 UB | 检查 endptr 是否前进, 不前进即报错返回; count 改 int64 并加显式上限 | 输入 '[-]' 的用例返回错误码且不挂死(复跑超时 124→正常退出), 用例在册 |

### CLI-002（2 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M5b-G-16 | - | 配置缺 output_dir 时静默回落 CWD，且门禁把这类产物列为 dirty 豁免 | 缺 output_dir 即 fail-closed（rc=2/3）或强制默认落 run/；dirty 豁免项改为拦截 | 缺 output_dir 的 run 不产出根目录文件；dirty 门对根产物能红 |
| V20-N-05 | - | 键集仅作旁路开关、值零消费，校验强度按键集存在与否切换 | 三键要么接线到真实消费者，要么删除并收紧校验；flat_session 分支显式登记 | 键集变化不改变必填校验强度（或校验差异有测试钉住） |

### NEXT-PACK:NP-01（2 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M7-A-210 | wmode 在 lib/algorithms/coverage 内 0 命中(ACR 只 DORMANT 源), 故列 NP-01 文档项 | 数值表与名称表无双向映射, 仅禁 mode=2 上 GPU 护栏的覆盖域不可判定; ACR 已 DORMANT 无生产后果 | 文档补单一映射表(名↔数值)并与 acr_kernels.cpp/stage2_common.cpp 实现对齐 | 两表可由同一映射互换(auto/support_x_snr2 均有数值), 或显式声明名称态与数值态各自独立 |
| M9-H-3 | hiss_reader.cpp 有正对照; 本行只在 gaia 侧成立 | 指针相加两侧(data_position/block_offset)与 memcpy 长度全来自文件自报字段(QTNode:157/:159), 函数体内无 mmap_size 边界校验, scratch 尺寸也由自报 … | 校验 data_position+block_offset+block_size <= mmap_size 且 scratch 容量 >= block_size, 越界 fail-closed | 畸变 shard(超界偏移)用例返回错误而非读越界; ASAN 下干净 |

### NEXT-PACK:NP-04（2 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M2a-F-2 | GAIA_QUERY.md 已无"深树"字样; gaia_xpsd_fixture_sp_gen.c 已不存在 | fixture 生成端只产单叶树, 解析与检索的深树/非叶分支无执行载体 | fixture_gen 支持多级四叉树(含 child_nw≠0), 新增深树锥查询用例并断言命中非叶分支 | fixture 至少一份 nodeCount>1 且含 child_nw≠0; 对应用例在 ctest 中执行 |
| M2a-F-3 | gaia_race 在三处注册面 0 命中(本轮复算 rc=1) | 用例文件在 test/ 但未进任何构建/CI 注册面; 恒零天测列(parallax/pmra/pmdec/source_id)全测试面零引用 | 把 test_gaia_race.c 注册进 ctest(TSAN 配置)并补恒零列断言 | ctest -R gaia_race 可执行且 TSAN 干净; tests/ 内 parallax/pmra 断言 >=1 |

### REAL-001（2 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M5b-G-19 | launch/ 悬空引用已由 GOV-001 归档处置（docs/DOCUMENT_INDEX.yaml:845） | 最高设计 §11.2/§12 的图像终审无机器承载（无检查项、无审核产物 schema/落点） | 建立图像量化证据产物 + 审核记录 schema + CI 检查项（证据缺失即红），Owner 终审落 reports/ | CI 存在图像审核检查项并能红能绿；发布验收含审核记录路径 |
| M7-A-208 | 数值为四舍五入近似，仍不可互推 | 两条实测行的折算隐式依赖未写明的 plate scale/帧标识，无法从文内量复推 | 补帧标识与 CD/plate scale（或直接给 px 偏差），并复核两行数字是否同帧 | 表中每个 deg 数字可由文内 scale 复推；第三方交叉门 1e-4 px 不变 |

### ROOT-002（2 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| V9-N-15 | 台账「根产物计数 0」不变，缺陷在豁免面 | 豁免清单替代拦截；且豁免名 resource_samples.csv 已随 D-15 改名失效（豁免对象漂移） | 移除豁免改为拦截（根出现 astrocs_run_*/三产物即红），豁免清单与实现名同步 | 根目录产出 run manifest 时门能红 |
| W4-R2-03 | 物理残留已删（根清洁线所为），机制面存续 | 测试 tmp 回退当前目录 + 全局忽略规则，复跑即复现根污染且门不可见 | 三环境变量全缺即 fail（或改用 output_dir/run 临时目录）；收紧忽略规则 | 无 tmp env 时测试失败而非落根；CHK-ROOT-CLEAN 能红 |

### ROOT-003（2 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| FD-G-001 | 与 M1a-I-001/V13-N-08 同族临时面污染 | run/ 为 gitignore 临时面，历史 runbook AGENTS.md 持续复制扩散（86→88），与现行 §6.2 唯一命令树冲突且机器门不可见 | 清运/归档 run/ 下影子 AGENTS.md；把「agent 指令文件唯一=根 AGENTS.md」落成 CI 检查（root_manifest/dirty 面） | 复用 CHK-ROOT-CLEAN 类检查：run/ 下出现 AGENTS.md 即红 |
| M1a-I-001 | 与 V13-N-08 同族 | 旧 V4/V4.6 报告未随 ARCH-001 迁移更新，探针/构建残留未清运 | 清运三个残留文件；REPORT.md 加 ARCHIVED 标注或改指当前算法目录，删除指向不存在归档的陈述 | ipv/ 目录仅含源码/构建脚本/文档；REPORT.md 无失效路径 |

### FINAL-001（1 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M7-G-001 | 家族判词（P0），成员级完整复算不在本批 | 门文本声明「Oracle 全过」但未声明判决域/折算因子，成员级算术近似无容差归属 | 为每个模型 Oracle 声明判决域与折算因子（或在 PSF.md 声明 1.230310 的舍入容差），并让门能红 | 每个近似有 mutation 证明门能红（DESIGN §11.1） |

### NEXT-PACK:NP-03（1 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M5a-F-001 | ci/checks.json:914 有 ACR-DORMANT 项(仅休眠声明, 非等价测试) | 文档声明 TST-ACR-* 但无实体 ID/注册; 权威八层矩阵无 SCI-ACR 行; 最接近用例仍条件跳过 | 把 TST-ACR-* 落到 tests 与矩阵, 或按 ACR DORMANT 撤销声明并标 MISSING | 矩阵含 SCI-ACR 行且 TEST 列指向可执行用例, 或文档显式标 MISSING |

### NEXT-PACK:NP-DC-03（1 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M6a-D-010 | 原报 15%/10%/55% 为 20 样本判读, 本轮不作规模结论 | 头注返回码表与实现不一致(缺 -5); 版本化 C ABI 合同头缺单位/默认值/所有权/线程安全说明 | 补 -5 及语义; 补 SDetParams 单位/值域与 sdet_create 生命周期/所有权/线程安全 | 头注码表=实现全部分支; star_detector.h 含单位与所有权声明 |

### P2-001（writer 面 AIO-001）（1 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M2a-A-2 | 读时哈希 DRIZZLE.md=2e414f9b8723、DATA_SEMANTICS.md=03b401c8a0ae、aio_hips_writer.cpp=eaf4fa4c6033（包内 81e769bdd8e7/a… | HiPS 写出面从不声明 BUNIT；SCI §3 与 DATA 单位表对 a/A_drop/D 的 px²↔sr 只写「等价」不写换算与产品声明 | SCI-DRZ 补 px²↔sr 换算与产品单位声明；AIO 写出面按 FITS 4.0 §4.4.1 写 BUNIT | tile FITS 头含 BUNIT 且与 DATA 单位表逐字一致（grep 命中>0） |

### P3-001,CFG-001（1 条）

| ID | 缺陷（当前树） | 根因 | 方案 | 验收门 |
|---|---|---|---|---|
| M1a-G-003 | 台账三处 → 本轮 5 处（新增 p3_proj_v6.cpp 常量与 registry 四行）；module_adapters 行号 5311→5313 | 同一冻结边界在投影层/会话层/节点参数层各写裸字面量，无共享常数源；新模块 p3_proj_v6.cpp:273-280 registry 再写 4 行 85.0 | 由 DESIGN §5.3 投影适用域声明 + 配置合同单源派生该边界 | 全仓 dec≤85 常数定义点 = 1 且改一处即可全局生效 |

## 4. UNVERIFIABLE（单独列，不计入四类之和）

| ID | 归属 | 缺什么 |
|---|---|---|
| M7-I-101 | DOC-001 | 缺什么：本分片无可执行的『复杂度重推口径』（需按各节伪代码逐式推上界）与对应 benchmark 选点数据（benchmark profile 在当前树未生成、CPU profile 由 benchmark 决定，见 ASTROCS_DESIGN §8）；台账原判即为 UNVERIFIABLE，本轮只复核 8 处节锚在位，未做强判 |
| V12-N-17 | REAL-001（建议改判归属：需 V12 轴补交宿主） | 缺什么：条目的证据宿主 V12.md §2.17 在 问题扫描/findings/A_SCI_DEF/{p1,p2}/V12*.md 与 _recheck/_work/V12-N-17.md 中均不存在（grep 零命中）；四套零点常数（22.5/20.6/20.48/14.0）在当前树测光域也零命中，无从复算「四套+两处兜底」。 |

## 5. 判据错清单（CRITERION-WRONG，27 条）

> 以下条目**不得按原处方整改**；每条给「为什么错 + 正确口径 + 依据」。

| ID | 归属 | 为什么错 + 正确口径 + 依据 |
|---|---|---|
| M2b-G-01 | AIO-001 | 为什么错：判据前提（存在复制 AIO 的第二 writer 目录 + p2 entrypoint 缺失）在当前树不成立——旧路径已由 ARCH-001 迁移（cmake/ARCH-001-migration-manifest.md 第 10 行 lib/hips→lib/algorithms/drizzle/hips DONE、第 33 行 lib/hips_p2→lib/algorithms/coverage/hips_p2 DONE，且注明"生产源 stage2.cpp 现位于 algorithms/coverage"）。正确口径：按模块归属核对"是否存在第二份 reader/writer 实现"——P1-HIPS 模块 src/module_entry.cpp:9-10,888 复用同一 AIO writer（aio_hips.h 九导出→aio_hips_product_begin），符合 ASTROCS_DESIGN.md:421「aio 是唯一 FITS/HiPS/manifest 读写边界；禁止各自复制 reader/writer」。依据：ASTROCS_DESIGN.md:421 + 迁移清单第 10/33 行 + 上述三命令 |
| V10-N-07 | AIO-001 | R-5 #12（:26）：**别题误挂**——该条被挂在噪声/统计域，实为 JSON 解析器数值健壮性问题（与噪声/统计口径无关）；事实成立但**严重度高估**（生产平台可见行为是 1e999→0，不是内存破坏）。正确口径：三处解析器加 `isfinite` 与 2^53 上限 + 数组 count 上限；归属数据解析器域（AIO-001）。 |
| FD-G-003 | CI-001 | 为什么错=判据是旧宪章 §12.2 与旧 145 项注册表时点，该权威已不在现行链（ASTROCS_DESIGN §0 链）；正确口径=该字段是「不可豁免清单」声明，与 docs/ci/01_CHECKS.md §1/§3（P0 红灯无 waiver）+ CONTROL_PACK_SPEC §7.3 一致，不构成待整改偏差；依据=docs/ci/01_CHECKS.md:129-135 与当前字段原文 |
| V19-N-12 | CI-001 | 为什么错=台账自述本条为「负结果·不立条」（不存在需整改偏差），其附带根事实（旧 schema 无依赖表达字段）不是现行权威的违例：ENGINEERING_SPEC §8 要求的是 Git diff 映射（impact_map 承担）+ 唯一注册表，未要求依赖字段；正确口径=登记面换代后由 V19-N-07 的 inputs 需求单独承接；依据=docs/ci/01_CHECKS.md §2+§4 与 ci/checks.schema.json 现文 |
| V9-N-06 | CI-001 | 为什么错=台账自述「非独立缺陷但决定工单量」，其内容（10 项聚合 + 基线 fail-closed）在现行树逐字复现且非违例；正确口径=该条是施工顺序说明而非缺陷条款，不得按 OPEN 建整改处方；依据=tools/quality/contracts/generate_contract_report.py:11-20 与 known_failures_baseline.py:16 现文 |
| V9-N-20 | CI-001 | 为什么错=台账本条为「负结果·不立危害条」（判否清单），不是缺陷条款；其事实前提「两门复算均无红项」已失效（CON-COMMENTS 现为 FAIL 2 findings），该红归 CHK-STALE-DOC 判据域另行定性；正确口径=本行不产生整改处方；依据=两条 checker 本轮实跑输出 |
| M3b-G-01 | DOC-001 | R-3 §0（:14）：把三条互不相干的门混为一条——「0.5px」是 ALG-STARDET-001 §11.4 **F4 FP32/u16 量化通道**容差（实测余量 ~90×，可达且未超标），「0.897px」是 plate-solve **外部闭环全帧中位残差**（legacy 工具）。不存在「未闭合质心 BLOCKER」⇒ 原判据前提不成立。真正的缺陷是**端到端坐标契约**（PSF 支路写端 −0.5px），已由 SCI-FIX-PSF 修复。 |
| M5b-G-07 | DOC-001 | 原判据（旧宪章 §12.1 的 docs/owner 文件清单）已无载体：现行 L0 口径为 docs/DOCUMENT_INDEX.yaml active 登记 + docs/review C1/C2 回归（tools/check_l0_docs.py:20-30 逐字），docs/ci/01_CHECKS.md §2 的 27 个 CHK-* 亦无 L0 文档清单项；ASTROCS_DESIGN §11.3/§12 均未规定该清单 ⇒ 判据前提不成立，不得按原处方要求补 PHASE_OVERVIEW.md |
| M5b-I-07 | DOC-001 | 判据错在两处：(a)『API_STANDARD 声明 ≥11 项字段』无载体——现行 API_STANDARD 只声明 6 项注释要素，检查器的 4 项 required 是 API_CONTRACTS.csv 的列 schema，两者对象不同，不构成不一致；(b)『DOC-004 与 RT-001 表内同 ID 占用』已被当前树反证（RT-001.md 0 命中）。正确口径：DOC-004 只作为任务号自述，ID 唯一性判据应落在 docs/contracts/INDEX.yaml；附带事实『API_STANDARD 仍把追溯交给旧 CSV』已修（:14 现指 MATRIX.json） |
| V12-N-14 | DOC-001 | 判据对象在当前树与全史均无载体（文档不存在、常量 0 命中、结构体无 β 参数），台账复核亦记 REJECT-NEVER-EXISTED ⇒ 属对象错而非待修缺陷；正确口径：Moffat β 由 docs/science/PSF.md §10 固定为 4，任何 β≠4 变更须走 SCI 变更流程（现行无偏差） |
| M5a-I-001 | FINAL-001 | 为什么错：两文档不是同一层判据——SCI §1-§3 定义「legacy launcher 与 per-pixel rejection+integrate 语义等价」，ALG F2/F4 把它落成「分块实现的数值容差」，ALG 明示上游=SCI-ACR-EQUIV-001，属层级别而非契约冲突。正确口径：语义等价由 SCI 定义、数值容差由 ALG F4 定义（float32 max_abs ≤1e-6、float64 ≤1e-12、support/rejection exact），二者叠加构成完整判据；且 ACR 生产不可达（ASTROCS_DESIGN:410），不存在需裁决的生产路由冲突。依据：docs/science/ACR_EQUIVALENCE.md:41-42,84-88 + docs/algorithms/ACR_EQUIVALENCE.md:9,15,17 + ASTROCS_DESIGN.md:68,410。 |
| V7-N-09 | FINAL-001 | 为什么错：本条为旧扫描轴 _verify/V7.md §三「判否负清单」条目（台账 verified_state=NOT_A_DEFECT），原判据出自已下链的旧扫描档案与已废止旧宪章 §12.3-8/§13，本身不构成缺陷判据。正确口径：该组条目作用仅是「各轴不得重报」；现状复核仍非偏差——precision_mode 缺键/错型双拒（module_adapters.cpp:3009）、p3 projection≠TAN 与 frame≠icrs 硬拒（p3_session.cpp:97/:102）。依据：ENGINEERING_SPEC §3 迁移 bitwise 相等+§5.1 边界/错误输入 + ASTROCS_DESIGN §11.1 + 台账 verified_state=NOT_A_DEFECT。 |
| M2a-G-3 | GOV-001 | 为什么错：旧宪章 §16.1「module/ABI/schema 版本不得与产品版本混用」已随旧宪章删除，现行权威链(ASTROCS_DESIGN §0/§12、ENGINEERING_SPEC §7)不含「命名空间分离」要求，判据前提不成立；正确口径：Alpha 前程序与代码中根本不应存在版本信息（DESIGN §12:511、SPEC §7:115），该问题形态已由 GAP-017 单列并归 GOV-001/PKG-001 清理，「混用/不分离」不构成独立偏差 |
| V5-N-02 | GOV-001 | 为什么错：①「拼接点必须带 end 容量」在现行权威链（ENGINEERING_SPEC/AGENTS/DESIGN）零条款，判据前提不成立；②代码点静态有界、无当前缺陷；③「修复声明覆盖面不实」的宿主是旧账本 fix_note，而账本已按 问题扫描/REBASE.md 降为历史证据，不再单独作整改依据。正确口径：完成声明面由 CONTROL_PACK_SPEC §3.3（现状可复核）+ASTROCS_DESIGN §12（未验收必须如实报告）约束 |
| V8-N-07 | GOV-001 | 为什么错：原判据要求登记到 CHANGELOG.md/memory.md/问题扫描/40_OWNER_DECISIONS.md，而 CHANGELOG.md 已整体删除、问题扫描/ 自述为「在用待整改台账」非权威链登记面，§0 链上无「负责人裁决登记册」机构（R-6 §1.5/§5.1，工程控制/PROJECT-GOVERNANCE-01/OWNER_DECISIONS.md:37 已把 D-17 登记为 open item）。正确口径：裁决存在⇒不撤销测试期望与注释，改为把三族裁决补登记到 §0 链上文档并新增通用登记形态条款（R-6 §5.1） |
| W1-N-06 | GOV-001 | 为什么错：与 V8-N-07 同族——原判据的登记面（问题扫描/40_OWNER_DECISIONS.md、CHANGELOG/memory）在现行权威链不存在或非权威（R-6 §1.5/§5.1，OWNER_DECISIONS.md:37 D-17 已登记为 open item）。正确口径：补登记面到 §0 链文档并固定「负责人裁决」登记形态，不得据「登记面命中 0」判偏差或撤销测试 |
| M3b-C-01 | MOD-001 | R-3 §0（:14）：0.5px 不是「未闭合 BLOCKER」而是 F4 量化通道容差 ⇒ 「规范层仍正面宣称无量化损失」与「存在未闭合 BLOCKER」不能同时成立；正确口径=端到端坐标契约（PSF 支路 +0.5px 桥接），已由 SCI-FIX-PSF 落地（OPEN_ITEMS A10）。 |
| V10-N-09 | NEXT-PACK:NP-01 | 为什么错: 判据源自旧扫描轴自设"同仓正对照"口径(问题扫描/_verify/V10.md §2, 非权威链文档); 现行 ASTROCS_DESIGN §6.1 只限定 CLI 薄入口职责, 21_observability §5 只要求 log_level 默认 info, 均无"环境变量严格解析"条款; 实测越界值回退默认 INFO(=0) 非静默关闭, strtol 站点全在 ASTROCS_TEST_* 测试钩子(与 V7-N-09 第 9 条同判)。正确口径: 不按严格解析整改; 若要严格化须新立规范并落到 CLI 合同 |
| L28e-D-001 | NEXT-PACK:NP-DC-01 | 为什么错: 范围串本身就是登记面声明形式(NOISE_MODEL.md:3 的"范围:"、noise_snr/module.yaml:48/62/92、矩阵 note 的"集合 SCI-REJ-001..008"), 且实注释多带宿主文档名(p1noise_tests_core.cpp:7 写 SCI-NOISE-001..015 (NOISE_MODEL.md, FROZEN T104)) ⇒ "整串不是任何已注册 ID"前提不成立; v19r3_traceability.py 的禁令只约束 contract inventory 行, 不约束注释。正确口径: 范围锚在现行权威链(ENGINEERING_SPEC §8 + COMMENT_STANDARD §必须注释)下可解析, 不得按原处方批量重写; 若要逐 ID 锚须新立规范。依据: docs/science/NOISE_MODEL.md:3 / tools/quality/v19r3_traceability.py:4-6 / docs/standards/COMMENT_STANDARD.md:8-14 |
| W5-N-11 | OBS-001 | 为什么错：原判据出自 docs/standards/**（已由 §0 权威链下链的旧 standards 注册表）；现行权威对「诊断可定位性」的要求是统一状态码 + JSONL 事件 schema + run-trace（ENGINEERING_SPEC §9 + 21_observability:15,24），不含 __FILE__/__LINE__ 源码行承载机制要求。正确口径：可定位性由退出码唯一源 + 事件 schema + troubleshooting 条目承载；源码行承载不是判据，观测事件字段完善度归 OBS-001 按 21_observability 推进。依据：docs/README-DOCS.md:29 旧体系已删 + ENGINEERING_SPEC §9 + docs/plugins/infrastructure/21_observability.md:15,24。 |
| M1a-C-001 | P1-001 | R-3 §0（R-3_PSF质心科学门与容差.md:19）：7×7 既是**拟合域**又是**检验域** ⇒ 自证门，判据本身错；且「1e-4px 数学不可达」对当前实现不成立（实测 1e-9 级）。正确口径：网格 ≥7×7 + 逆映射以迭代反演为准 + 不变量在**独立密集域**上测（DISP-WCS-008 已登记）。 |
| M3b-A-01 | P1-001 | R-3 §0（:16）：**门本身错**——SCI-PSF-001 §10 的禁则适用范围被越界扩张到检测域。正确口径：冻结「检测侧=椭圆高斯 / PSF 侧=Moffat4」双模型，写死不可互换的换算声明并登记 DISP（现已按此登记 DISP-STAR-007）。不得按原处方把检测侧改为 Moffat4。 |
| M7-A-105 | P1-002 | R-5 #7（:21）：**前提部分失效**——参考量可构造（k 的语义由 :73 零点平移不变量给出），缺的是**实现（测试）**，不是 SCI 定义。正确口径：由 A_SCI_DEF **改归 F_TEST_GAP**，补 PHOTOMETRY §11 合成注入 + NumPy 复算用例；不判 SCI 缺陷。 |
| M7-A-203 | P1-002 | R-5 #2（R-5_噪声SNR与统计口径.md:16）：**判据对象写错**——该条是 §5 兜底式记号「两读」，不是 ivar 单位（ivar 单位属 M3-A-004）。正确口径：§5 由 `max(vmed_or_sig², floor)` 拆成两条显式式（已落地）、不改码。 |
| M7-H-104 | P1-002 | 为什么错：原判据把「负预测方差夹到 variance_floor」当缺陷，与最新权威相反——SCI §7 要求任意 variance 经 max(…,1e-12)、§9 明文「平面预测负值 clamp 至 floor」，实现即合规；要改为 ivar=0 必须走 ENGINEERING_SPEC §3 科学变更/负责人裁决通道，不属代码偏差；依据 docs/science/NOISE_MODEL.md §7/§9 + 台账原判 VOID 的自证 |
| M7-A-117 | P3-002 | R-1 §E（reports/PROJECT-GOVERNANCE-01/research/R-1_投影WCS数学正确性.md:328-331）：账本的订正建议 `+2·log2(W)` **多一倍**（W=512 ⇒ +18 ⇒ nside 偏大 512 倍）；正确口径 `leaf_order = order_sel + log2(W)`、`>>2·log2(W)` 是 tile 内索引位移。**不得按原处方整改**；现状 SCI 已按正确口径订正。 |
| M7-A-129 | P3-002 | R-1 §F（R-1 报告:333-336）：①「跳象限不重归一 ⇒ Σ(kept w)<1」对实现是**误报**（实现是最近点填充、非丢弃）；②「Σw=1 精确成立」在 FP64 下不是逐位命题（实测 /Σw−1/max=2.22e-16=1 ULP，逐位为 1 的比例 96.1%）⇒ 正确口径「Σw = 1 ± k·ULP」。 |

## 6. NOT-APPLICABLE（对象已退役/删除，12 条）

| ID | 归属 | 退役/删除依据 |
|---|---|---|
| M8a-G-009 | ARCH-001 | 退役记录：缺陷对象目录本轮实测已不存在（ARCH-001 迁移清运对象）；现行权威 ASTROCS_DESIGN §12（发布候选门）+ §7（源码根=lib/algorithms+lib/infrastructure）无该 vendored 通道 |
| M6b-F-001 | GOV-001 | 退役记录：docs/ci/01_CHECKS.md §2.1 TRACEABILITY-CODE 退役行（2026-09-16）+ 实测该门无参调用打印 TRACEABILITY_RETIRED 并 exit 2（不再回退被删快照、不伪装绿）；能力去向与可复跑用法同表登记 |
| M6b-G-007 | GOV-001 | 退役/废止记录：缺陷载体 docs/README-DOCS.md 已删除、被指缺页 docs/owner/PHASE_OVERVIEW.md 亦不存在；该制度随旧宪章下链，现行替代=ASTROCS_DESIGN §0 权威链+AGENTS.md §1 必读清单（设计 §0 已废止世代） |
| M1a-B-004 | P1-001 | 退役记录：cmake/ARCH-001-migration-manifest.md:48（lib/phase3_session DEFERRED）+ p3_projection.h:4/34 RETIRED 头注（SCI-FIX-PROJ 处置）；缺陷对象只存在于待删 Session 模块与 RETIRED v1 库，生产 DLL 不构建 |
| M3-C-005 | P1-001 | 退役记录：cmake/ARCH-001-migration-manifest.md §1 第 46 行 + ARCH-001 §4「仍待 INT-001 处理」；IR 节点通道（module_adapters.cpp:1303/1384）已 fail-closed，缺陷对象只在待删的 lib/phase1_session |
| M1a-C-004 | INT-001 | 退役记录：cmake/ARCH-001-migration-manifest.md:48「lib/phase3_session（不迁移，删除）DEFERRED」+ 工程控制/PROJECT-GOVERNANCE-01/OPEN_ITEMS.md:18 A9（INT-001 剩余范围含 3 个 *_session 目录删除） |
| W2-N-13 | INT-001 | 退役记录：cmake/ARCH-001-migration-manifest.md:48 + OPEN_ITEMS.md:18 A9（缺陷对象在待删会话模块）；在役 v6 已改用逐边采样 + 极点余量（pole_margin_deg/domain_valid）判定 |
| M1a-C-008 | INT-001 | 退役记录：ARCH-001 manifest:48（lib/phase3_session 不迁移、删除）+ OPEN_ITEMS:18 A9；残余对象只在该待删会话模块 |
| M6a-A-001 | INT-001 | 退役记录：cmake/ARCH-001-migration-manifest.md:48（lib/phase3_session 不迁移、删除）+ OPEN_ITEMS.md:18 A9 |
| M6a-D-013 | INT-001 | 退役记录：cmake/ARCH-001-migration-manifest.md:48 + OPEN_ITEMS.md:18 A9（lib/phase3_session 整体删除） |
| M6a-I-002 | INT-001 | 退役记录：cmake/ARCH-001-migration-manifest.md:48 + OPEN_ITEMS.md:18 A9 |
| M7-A-139 | INT-001 | 退役记录：cmake/ARCH-001-migration-manifest.md:48 + OPEN_ITEMS.md:18 A9（lib/phase3_session 不迁移、删除） |

## 7. 复核方法（可复跑）

1. **台账解析**：`run/PROJECT-GOVERNANCE-01/SCAN-CLEAR/parse_table.py` → 785 行 × 10 列 → `rows.jsonl`（ID 与 `问题扫描/账本/FIX_LEDGER.csv` 双向差集为空）；
2. **旧→新路径翻译**：`resolve_paths.py`（三级：原样命中 / `cmake/ARCH-001-migration-manifest.md` 翻译 / 同名唯一匹配），结果 `path_resolve.json`；**台账行号 100% 失效**，一律按内容检索复核；
3. **机械复跑**：`run_probes.py` 把台账「当前证据」列的 1613 条命令按新路径重放（12 并发、全部 timeout、写命令拒绝），结果 `probe_results.jsonl`；
4. **内容签名检索**：`sigsearch3.py`（抗行号漂移，判断台账引用原文是否仍在当前文件）；
5. **分批复核**：10 个分域批次（`batches/*.md` + `RUBRIC.md`）逐行在当前树取证，产物 `batches/out/*.csv`；
6. **人工复核覆盖**：`overrides_manual.json`（23 行；按 R-1..R-7 裁定与本人一手命令覆盖分批复核，覆盖留痕写进 CSV 备注）；
7. **抽审**：`audit_probe.py` 对每类 5% 抽样（种子 20260917）独立重跑证据列命令，结果 `audit_probe.json`（45 条，RAN 45 / NO-CMD 0）。

**复跑全表**（只读，本机可跑）：

```bash
cd "/workspace/Astro CS Database"
python3 run/PROJECT-GOVERNANCE-01/SCAN-CLEAR/parse_table.py            # 785 行解析
python3 run/PROJECT-GOVERNANCE-01/SCAN-CLEAR/resolve_paths.py          # 路径翻译
python3 run/PROJECT-GOVERNANCE-01/SCAN-CLEAR/run_probes.py             # 机械复跑 1613 命令
python3 run/PROJECT-GOVERNANCE-01/SCAN-CLEAR/merge_verdicts.py         # 合并 + 计数自洽校验
python3 run/PROJECT-GOVERNANCE-01/SCAN-CLEAR/audit_probe.py            # 抽审复跑
```

## 8. 本次复核暴露的**新缺陷**（不在台账内，建议进阶段 4 或新任务）

| # | 来源 | 对象 | 事实（可复跑） | 建议归属 |
|---|---|---|---|---|
| R-NEW-01 | 我（SCAN-CLEAR 主控） | tests/backend/test_p3006_production_pipeline.py:133-139 | 该测试仍读 evidence/v6_1_rework/TASK_LEDGER.csv，该文件已不存在（ls rc=2）；断言只检查 P3-006 在 id 列表内，属悬空一次性证据面。 | DOC-001 或 TEST 域 |
| R-NEW-02 | B05_P2 | tests/backend/test_drizzle_oracle.py:21 | HOST = <repo>/lib/backend_host 已由 ARCH-001 迁到 lib/infrastructure/benchmark/backend_host（ls rc=2）⇒ 该 Oracle 现不可构建，drizzle 精度对拍证据缺失。 | CI-001 / AIO-001 |
| R-NEW-03 | B06_DATA_AIO | tools/quality/contracts/check_api_contracts.py | 工作树实跑现为 rc=1 / status=FAIL / findings=145（ast_symbols=362）；台账所称「run/** 影子树假绿」已消失，但 docs/contracts/API_CONTRACTS.csv:331 仍记 VERIFIED ⇒ 门红面扩大。 | QA-001 / CI-001 |
| R-NEW-04 | B04_MOD | docs/modules/registry/** 与 22 份 module.yaml | descriptor 侧的 22 个 astrocs.phaseN.* 与 module.yaml 的 22 个 astrocs.pN.* 交集为 0；drizzle/hips/cosmetic/calibration/gaia 五处 README+module.yaml 仍写「无源码/entrypoint=MISSING/无 CMake target」，而对应 target、astrocs_module_query_v1、product.json IMPLEMENTED 与 7 项 gaia_cat_* ctest 均在位。 | MOD-001 |
| R-NEW-05 | B10_NEXT | lib/algorithms/**/backend_table.inc 与 TRACEABILITY_MATRIX.json | ALG-008/009 在追溯矩阵零登记，却经 backend_table.inc:29-30 进编译；aio reader/writer 自报 12/9 而实际导出定义 13/12（原判「少报 1」不成立）。 | MOD-001 / DOC-001 |
| R-NEW-06 | B10_NEXT | lib/infrastructure/gaia_xpsd_client/** | M9-G-1 失败路径无 munmap/CloseHandle（泄漏）、M9-G-2 部分树失败仍 return 0、M9-H-3 read_leaf_block 无 mmap 边界校验；M9-G-3 已用同构 C 探针复现挂死（gcc rc=0；timeout 5 → rc=124）。 | AIO-001 / GAIA 域 |
| R-NEW-07 | B08_P1P3 | lib/algorithms/resample/** 与 lib/algorithms/projection/**（新模块） | 新模块存在与旧会话模块同款的缺陷对象：M1a-A-010（p3_proj_v6.cpp:152 r>=kHalfPi 与 :194 stheta<=0 有效域不对称，DATA_SEMANTICS:2287 仍称「数学等价」）；M6a-D-012（registry spec 字段只被 selfcheck 断言 >0.0）⇒ 8 条以「待删会话模块」为据的 NOT-APPLICABLE 之外，另有 2 条在新模块判 OPEN。 | P3-001 / P3-002 |

## 9. 局限与口径边界（必须与结论一起读）

- **树在动**：复核期间 HEAD 由 `fdf6296b1dd2` 起连续前进（并发执行线在改 docs/lib/ci），关键文件哈希在 `anchor.json`；CSV「备注」列逐行标了读时哈希/行号漂移。**任何结论都以复跑时刻的树为准**，本报告的计数是锚点时刻的快照。
- **NOT-APPLICABLE 中的「待删模块」前提**：`lib/phase3_session/**` 仍在构建图内（根 CMakeLists.txt:614-620），其退役依据是 `cmake/ARCH-001-migration-manifest.md:46-48`（DEFERRED 不迁移、删除）+ `OPEN_ITEMS.md` A9（INT-001）。**若 INT-001 取消删除，这些条目应回到 OPEN**；逐条见 CSV。
- **机械交叉信号不作判定依据**：`mech_verdicts.json` 是「台账引用原文是否仍在当前文件」的粗略信号（308 行 FIXED 中已知有不少属「文字改写而缺陷仍在」），仅用于给分批复核挑刺，最终判定以 CSV 的证据列为准。
- **UNVERIFIABLE = 2**（见 §4）：仅当证据依赖外部标准原文/宿主指针而仓内不可自证时使用；其余 783 行均给出四类判定。
- **判定与台账分类轴不同**：台账四态（OPEN/RESOLVED/VOID/UNVERIFIABLE）问「对照最新权威条款是否仍成立」；本轮四类问「当前树是否仍存在该缺陷对象」。两者合计相同（785），但行级不可直接对齐。

## 10. 逐条日志索引

- `run/PROJECT-GOVERNANCE-01/SCAN-CLEAR/logs/per_row/<ID>.md`：**785 个逐条日志**（台账原文 + 路径翻译 + 机械复跑 rc/输出 + 本轮判定/证据/根因/方案/验收门 + 交叉信号）；
- `logs/00_anchor.log`…`logs/26_*.log`：主流程日志（编号即执行顺序）；
- `logs/B0*_*.log|json|py`：各分批复核的原始探针与证据日志；
- `batches/out/*.csv`：10 个分批复核的原始产物（未合并、未改写）。
