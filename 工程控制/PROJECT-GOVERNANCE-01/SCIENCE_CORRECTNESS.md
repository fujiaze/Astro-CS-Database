# SCIENCE_CORRECTNESS —— 科学正确性优先（负责人指令 2026-09-16）

## 一句话

**科学文档必须正确。** 有独立证据证明它错了，就改它——不需要为"它是冻结的"而保留错误，也不需要等负责人逐条批准。

依据：负责人原话「**我不要遵循旧制，我要求科学文档必须是正确的**」，与 `AGENTS.md §8`（科学歧义才问负责人）一致：
**证据能定的事，执行行自己定；证据不能定的，才上呈。**

## 判定规则（四条，取代旧的"一律不得修改"）

| 情形 | 处置 | 谁决定 |
|---|---|---|
| 文档与**外部标准/文献**不符（如 FITS WCS Paper II、IVOA 规范） | **改文档**（+ 改实现，若实现也随之错） | 执行行，凭标准原文 + 实验 |
| 文档与**可复现实验**不符（如蒙特卡洛证明某默认值统计上不成立） | **改文档**（或改实现，取决于哪边被证明错） | 执行行，凭实验（脚本 + 输出 + 样本量） |
| 文档**自相矛盾**（同文两处互斥） | 判定哪一处与标准/实验一致，改另一处 | 执行行，凭证据 |
| 两份权威文档互斥**且证据无法判定** | 上呈负责人 | 负责人 |

## 变更 claim（每次订正必填，落在本文件 §登记表）

订正科学文档时必须在同一提交里登记一条 claim：**改了什么 / 依据什么证据（可复跑）/ 影响面（哪些模块、合同、registry、测试）/ 版本递增**。

## 登记表

| # | 日期 | 文档 | 改什么 | 证据（可复跑） | 影响面 | 状态 |
|---|---|---|---|---|---|---|
| SC-001 | 2026-09-16 | `docs/science/PHASE3_HIPS_TO_FITS*.md`、`docs/algorithms/PHASE3_PROJ_IMPL*.md`、`docs/algorithms/PHASE3_RESAMPLE*.md`、`docs/contracts/DATA_SEMANTICS.md`、`docs/plugins/algorithms_phase3/14_projection.md` | ① CAR/AIT 按 FITS WCS Paper II 把 `CRVAL2`（含 LONPOLE 默认规则）纳入映射；② AIT 域界 `A<2` → `A≤1`；③ CAR native 极行（θ=±90°）fail-closed；④ 投影集合以 `ASTROCS_DESIGN §5.3` 的**八投影**为准，删除对**已废止宪章**条款的引用；⑤ registry 升 v3、v1 退场 | `reports/PROJECT-GOVERNANCE-01/research/R-1_投影WCS数学正确性.md`；`run/PROJECT-GOVERNANCE-01/R-1/`（astropy 7.0.1 逐点对拍 22 组，max 6.854e-13°；`probe_p3proj` vs `probe_fixed` 显示 CRPIX 处 dec 0→30；追加 dec0≠0 用例后现 Oracle 必红） | `lib/algorithms/projection/**`（公式+registry）、共址测试、`tests/unit/v6_p3_proj/**`、相关门 | 待执行 |
| SC-002 | 2026-09-16 | `docs/science/NOISE_MODEL.md`、`docs/algorithms/NOISE_ESTIMATION.md`、`docs/contracts/{DATA_SEMANTICS,PUBLIC_API}.md`、`lib/algorithms/noise_snr/README.md` | **文档侧**：① §4 `min_samples` 默认值 5 → **64**；② §2:16 ivar 单位 `pixel⁻²·ADU⁻²` → `ADU⁻²`；③ §5:51 兜底式记号拆为两条显式式（合格 patch 支 `max(vmed,floor)` / 全帧退化支 `max(sig²,floor)`）；④ ALG §13.2 删除「不改 SCI，以代码为准」的权威倒置表述，改「以 SCI 为准」；⑤ §4/§5 增平面几何启用条件（点云相对条件数 `λlo/λhi≥1/16`，κ≤4）；⑥ §9a 补 `sigma_cal_rel`（逐星散度 ≠ 零点不确定度；零点标准误 `1.253·σ/√N_eff`）；⑦ §14 增默认值导出依据；⑧ ALG §13.3 增 DISP-NOISE-010、§13.3a 增四处实现整改登记；⑨ 全部受影响锚点同步（noise_model.cpp 行号表 + snr_estimator.h/README/合同）。**实现侧（同 claim，非架构改动）**：⑩ noise_model.cpp 相对条件数判据 + build 阶段回退 `has_spatial_field`；⑪ wrapper 注释补 `/gain²`、单测 fixture 增益方向回 SCI 约定 + 容差收到 5%、`photometer.cpp:90` 常数改冻结值；⑫ `orchestrator.cpp` 删 `\|\| psf_status==3.0`（3=ITERATION_LIMIT 失败码不再拿满权）；⑬ 删除 `snr_science.cpp` 零引用死常数 `kLn10`（模块单一定义点） | `reports/PROJECT-GOVERNANCE-01/research/R-5_噪声SNR与统计口径.md`；`run/PROJECT-GOVERNANCE-01/R-5/exp1..exp9`（N=5 ⇒ 单 patch 偏差 −19.2%、SCI 自设 5% oracle 通过率 0.6%；N=64 ⇒ −1.25%、92.8%）；**本任务新增** `run/PROJECT-GOVERNANCE-01/SCI-FIX-NOISE/logs/`：01 几何判据数值表（**推翻** R-5 §5.4 建议式 `det/(sxx·syy)`——实测 B=0.794 vs 满格 1.0 无分离度，改用点云条件数 A=0/B=0.0133/C=0.200/满格 1.0）、10-13 新测试红→绿、15 Oracle 注入红、17/19 单测红→绿、21/22 四处整改探针红→绿、30/31 锚点 JSON（814 锚，NOISE 域 0 失败） | `lib/algorithms/noise_snr/**`（默认配置/测试/Oracle）、`lib/algorithms/photometry/wrapper_phase1/photometer.cpp`、`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp`、`tests/unit/p1_noise_test.cpp`、`docs/contracts/*`、SCI §11 oracle、`config/**` 反射（`source_ref` 仍指 ALG 旧行，见 SCI-FIX-NOISE 自证摘要"未做项"） | 已执行（工作区，待前台提交） |

| SC-004 | 2026-09-16 | `docs/science/STAR_DETECTION.md`、`docs/algorithms/STAR_DETECTION_ALGORITHMS.md`、`docs/science/PSF.md`、`docs/science/ASTROMETRY.md`、`docs/algorithms/PLATESOLVE.md`、`docs/contracts/DATA_SEMANTICS.md`、`docs/standards/STANDARDS_REGISTRY.md`、`docs/algorithms/GATES_AND_TOLERANCES.md`（新，ALG-GATES-001）、`docs/DOCUMENT_INDEX.yaml`、`lib/algorithms/star_detection/README.md`+`module.yaml` | **SCI-FIX-PSF 质心坐标契约修正（9 项）**：① D-16 重新定性——0.5px = ALG-STARDET-001 §11.4 **F4 FP32/u16 量化通道容差**（实测 u16 量化 median 0.0018 / p95 0.0036 / max 0.0056 px，余量 ~90×，**可达未超标**），0.897px 系 plate-solve **外部闭环台账值**（不同门/不同域/不同工具代，不可比）⇒ 整改对象改为**端到端坐标契约**：同星 sdet 报 truth、dpsf 报 truth−0.5，编排写端再 −0.5 ⇒ PSF 支路 median −0.5000 px、fallback +0.0001 px、**分离恰 0.5000 px 卡在 ipv 去重阈值严格 <0.5 之外 ⇒ 同星双份进入 ipv**；修桥=PSF 支路写端不再 −0.5（dpsf 输出即统一契约 index-is-center），并新增链接 sdet+dpsf 的绝对位置门 **G-P1-CENTROID-1**（ctest `p1psf_centroid_gate`/`_neg`）；② M3b-A-01——SCI-PSF-001 §10 禁则对象是 dpsf，被 STAR_DETECTION §2 越界扩张到检测域 ⇒ 冻结**双模型**（检测侧=椭圆高斯 / PSF 侧=Moffat4）+ 换算声明（同 sx 下 FWHM 差 **1.9140×**；高斯拟合 Moffat4 真星质心无偏 0.0047 px，但 FWHM 报值/真值=1.086、解析流量比=0.902）+ 函数改名 `sdet_moffat4_fit→sdet_gauss_fit` + DISP-STAR-007；③ F1 只补量测域（**门值 0.5″ 不动**：内点 trans 域 + 合成线性场 + n_pairs≥12；0.897px 归独立证据面 G-P1-WCS-CLOSURE）；④ M3b-F-02——scipy `curve_fit` 独立 Oracle 落地 + TRACEABILITY_MATRIX `test_path` 由文档锚改为可执行目标 + `VERIFIED⇒evidence≠MISSING` 机器门；⑤ M1a-A-001——ASTROMETRY.md §5 前向式改 SIP 标准形 `(ξ,η)=CD·[(xp−CRPIX)+SIP_A/B(xp−CRPIX)]`（原式把 px 量 SIP 加到 deg 量 CD 上，等效误差 3.7e3–4.4e5 px；**实现零改动，以实现为准订正 SCI**）；⑥ M1a-C-001——**判据本身错**（7×7 自证门：自网格 1.8e-12 px vs 离网格 3.10 px）⇒ 取消 `NB_GRID=7` 冻结地位、往返不变量改**独立密集域** ≤1e-4 px（原 1e-6 px/7×7 口径废止）、登记 DISP-WCS-008、归档 owner 裁决 WCS-003 提回活动登记面；⑦ F-2——新增**机器可校验冻结门表** ALG-GATES-001（16 门，列=门ID/判据式/量测域/统计量/SNR定义/阈值/阈值来源/证据ID/发布门 + 规则 R1 表外阈值禁止、R2 证据必需、R3 量测域独立、R4 统计量显式、R5 SNR 统一）+ 补 **SNR 定义** `SNR_peak:=A_fit/sigma_bg`（原全文无定义；实测同一合成场 SNR_peak=20 检出 0 星、=50 检出 36/40）+ 机器门 `tools/check_gates_and_tolerances.py`；⑧ DATA_SEMANTICS §17.2 的 fallback「−0.5 转统一契约」过期句订正为**直送**（与实码互斥） | `reports/PROJECT-GOVERNANCE-01/research/R-3_PSF质心科学门与容差.md` + `run/PROJECT-GOVERNANCE-01/R-3/**`（探针直链本树 `libastrocs_p1_sdet.a`+`libastrocs_p1_dpsf.a`：sdet−truthFITS median 0.0035 px、dpsf−truthFITS 恒 −0.5000 px（三种初值同解）、PSF 支路 −0.5000 / fallback +0.0001 / 分离 0.5000 px；u16 量化 p95 0.0036 px；SIP 网格对照 7/41/81）＋ `run/PROJECT-GOVERNANCE-01/SCI-FIX-PSF/logs/**`（**门红→绿**：`p1psf_centroid_gate` 14/14 PASS、双支路分离 −0.0000/+0.0001 px；`p1psf_centroid_gate_neg` 必红检出 −0.5000 px；`tools/check_gates_and_tolerances.py --self-test` 7/7 注入必红） | `lib/infrastructure/pipeline/orchestrator/**`（坐标桥 + 新 `star_coord_contract.h`）、`lib/algorithms/star_detection/**`（改名）、`lib/algorithms/psf/tests/p1psf/**`（新门）、`docs/traceability/TRACEABILITY_MATRIX.{json,csv}`、`docs/standards/STANDARDS_REGISTRY.md`、`问题扫描/账本/FIX_LEDGER.{jsonl,csv}`（7 行处置列）、`lib/algorithms/platesolve/cpp/ipv/src/*.cpp`（仅注释）；**产品面**：PSF 支路像素坐标 **+0.5 px**（同星不再双份进 ipv）⇒ 依产品面影响评估走版本递增（PSF 走有效星 +0.5 px，fallback 星不变） | 待执行 |

## 与旧做法的差别（一句话对照）

- 旧：文档冻结 ⇒ 发现文档错也只能"登记、上呈、等批准"，甚至写出"以代码为准"来绕过；
- 新：**文档错就改文档**（凭标准或实验），改了登记 claim；只有"证据判不了"的才上呈。
