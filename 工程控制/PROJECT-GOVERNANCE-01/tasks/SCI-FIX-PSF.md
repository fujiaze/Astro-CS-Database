# 任务：SCI-FIX-PSF 质心坐标契约修正（R-3 结论落地）

状态：NOT_STARTED　层：L1　依赖：R-3（已完成）
互斥组：S9-A（星检测/PSF 桥 + 共址测试 + 相关门 + 文档订正）

## 依据（负责人指令：文档必须正确，不要遵循旧制）

证据：`reports/PROJECT-GOVERNANCE-01/research/R-3_PSF质心科学门与容差.md` + `run/PROJECT-GOVERNANCE-01/R-3/**`（探针直链本树静态库 + scipy 独立复算）。

## 核心事实（必须修的真缺陷）

探针直链 `libastrocs_p1_sdet.a` + `libastrocs_p1_dpsf.a` 实测：**同一颗星 sdet 报 truth、dpsf 报 truth−0.5000**（三种初值都收敛到同一点）；而编排写端（约 `orchestrator.cpp:2498`）对 dpsf 输出**再减 0.5**（依据 `test_p1_batchH_star_coord.cpp:6-7` 的一条断言，该断言**实测为假**）。结果：
- `astro_det` 的 PSF 支路 median = **−0.5000 px**，fallback 支路 median = **+0.0001 px**，同星分离 **0.5000 px**；
- 去重阈值恰是**严格 ` < 0.5`** ⇒ **同一颗星双份进入 ipv**；
- 该 batchH 测试**未被任何 CMakeLists 引用**（未入门禁）。

## 要做

| # | 做什么 | 落点 |
|---|---|---|
| 1 | **修桥**：PSF 支路写端不再 −0.5（以 sdet 的绝对坐标为唯一约定） | 星检测/PSF 桥接实现 |
| 2 | **新增绝对位置门**：链接 sdet+dpsf 的共址测试，断言同一颗星两条支路坐标一致（容差按 R-3 实测给，如 ≤0.05 px），并**注册进构建图**（batchH 那条假前提断言一并修正或退役） | 共址测试 + CMakeLists |
| 3 | **D-16 重新定性**：0.5px 是 ALG-STARDET-001 §11.4 **F4 的 FP32/u16 量化通道容差**（实测 u16 量化 p95 0.0036 px，余量 ~90×，**可达未超标**）；0.897 px 是 plate-solve **外部闭环全帧中位残差**，不同门/不同域/不可比 ⇒ 整改对象改为**端到端坐标契约**（即第 1 项） | `docs/algorithms/**` + 议题台账 |
| 4 | **F1 只补量测域**（内点 trans 域 + 合成线性场 + n_pairs≥12），**门值 0.5″ 不动** | `docs/algorithms/**` |
| 5 | **M3b-A-01 门本身错**：SCI-PSF-001 §10 的禁则对象是 **dpsf**，被 STAR_DETECTION.md §2 越界扩张到检测域 ⇒ 改为冻结**双模型**（检测侧=高斯 / PSF 侧=Moffat4）+ 换算声明（同 sx 下 FWHM 差 **1.9140×**；高斯拟合 Moffat4 真星质心无偏 0.0047 px，但 FWHM 报值/真值=1.086、解析流量比=0.902）+ 函数改名 + DISP 登记 | `docs/science/**`、`docs/algorithms/**`、`docs/plugins/**` |
| 6 | **M3b-F-02**：「需新依赖」为假（scipy 仓内已在用）⇒ 落 `curve_fit` Oracle（R-3 已 9.5 ms 跑通：位置 0.0 px、FWHM 1.6e-16）+ **修 TRACEABILITY_MATRIX**（`test_path` 不得是 `docs/**::ID`；`VERIFIED` ⇒ `evidence_status ≠ MISSING`）+ 加机器门 | 树内 Oracle + 矩阵 |
| 7 | **M1a-A-001**：ASTROMETRY.md:48 把 px 量的 SIP_A/B 加到 deg 量的 CD·(xp−CRPIX)（量纲误差等效 3.7e3–4.4e5 px）⇒ **以实现为准订正 SCI**（实现零改动） | `docs/science/**` |
| 8 | **M1a-C-001 判据本身错**（7×7 自证门：自网格 1.8e-12 px vs 离网格 3.10 px）⇒ 取消 `NB_GRID=7` 的冻结地位，改「网格 ≥7×7（实现 41/81）+ 逆映射以迭代反演为准 + 不变量在**独立密集域**测」；登记 DISP-WCS-008，并把归档里的 owner 裁决 WCS-003 提回活动登记面 | 文档 + 门 |
| 9 | **F-2**：本域 7 条门**零标定依据** ⇒ 补一张**机器可校验的冻结门表**（门ID/判据/量测域/统计量/SNR定义/阈值来源/证据ID），并补 **SNR 定义**（现全文无定义；实测「峰值 SNR=20」时 sdet 检出 0 星、SNR=50 检出 36/40） | `docs/algorithms/**` + `contracts/**` |

## 硬纪律

1. **产品面影响评估**：第 1 项会改变已发布产品的像素坐标 0.5 px ⇒ 必须给影响面（哪些测试/合同/工件受影响）+ 版本递增建议；**所有「以绝对坐标比真值」的期望要重锚**（p1wcs/p1psf/p1star）；
2. 零 git 写；不得改 `ci/**`、`.github/**`（其门禁改动登记转 CI-003）；
3. 每条给「改前 → 改后 → 依据」；新增门给「改前红 → 改后绿」；
4. `ninja -C build -k 0` 0 FAILED；相关 ctest 全绿；改被锚文档后复跑 `check_doc_line_anchors.py --root .` rc=0；
5. 在 `SCIENCE_CORRECTNESS.md` 追加 claim SC-004；
6. 日志落 `run/PROJECT-GOVERNANCE-01/SCI-FIX-PSF/logs/`。

## 交付（中文，直白）

1. 逐条执行表；2. 修桥前后的坐标实测对照（0.5 px 偏差 → 0）；3. 新门红→绿；4. 产品面影响评估与重锚清单；5. 未做项；6. 自证摘要。

---

## 登记转 CI-003（本任务**未**改 `ci/**`、`.github/**`，以下为需 CI 侧登记的项）

| # | 要登记的检查 | 建议条目（要点） | 为什么必须 | 极性证据（本任务已备） |
|---|---|---|---|---|
| CI-003-SCI-FIX-PSF-1 | **门表机器门**：`python3 tools/check_gates_and_tolerances.py` | 新增 `checks.json` 条目（建议 ID `GATES-TOLERANCES`，`changed_paths` 含 `docs/algorithms/GATES_AND_TOLERANCES.md`、`docs/DOCUMENT_INDEX.yaml`）；两个 step：正例（默认模式）+ 负例（`--self-test`） | 检查器与自检脚本本身必须进门禁，否则"机器可校验门表"只是文本承诺 | 正例 `run/PROJECT-GOVERNANCE-01/SCI-FIX-PSF/logs/40_gate_table_check.txt`（rc=0，16 门）/ 负例 `41_gate_table_selftest.txt`（7 注入必红，rc=0） |
| CI-003-SCI-FIX-PSF-2 | **追溯矩阵机器门**：`python3 tools/check_traceability_matrix.py` | 新增 `checks.json` 条目（`test_path` 不得为 `docs/**::ID`；`VERIFIED ⇒ evidence_status≠MISSING`；路径存在性）；含 `--self-test` 负例 step | ENGINEERING_SPEC §8「每项检查有正例与负例」；现状 `test_status=VERIFIED` 与 `evidence_status=MISSING` 可长期共存无人发现（R-3 §4.6） | 见本任务 Agent C 报告与 `run/PROJECT-GOVERNANCE-01/SCI-FIX-PSF/logs/30_agentC_matrix_gate_*.txt` |
| CI-003-SCI-FIX-PSF-3 | **新 ctest 覆盖确认**：`p1psf_centroid_gate` / `p1psf_centroid_gate_neg` | `CHK-UNIT::CTEST-LINUX-FULL` 已能覆盖（全量 ctest）；建议在 `CHK-ORACLE` 增设显式 step `CTEST-P1PSF-CENTROID-GATE`（`deep_ci_driver.py ctest-target --target p1psf_centroid_gate`）以便快速定位 | 该门是 D-16 端到端坐标契约的唯一机器锁；不宜只靠全量套件间接覆盖 | `logs/04_ctest_gate.txt`（2/2 Passed） |
| CI-003-SCI-FIX-PSF-4 | **Python Oracle 发现面**：`tests/backend/test_psf_moffat_oracle.py` | `CHK-UNIT::UT-BACKEND` 用 `unittest discover -s tests/backend` ⇒ 该文件必须是 `unittest.TestCase` 形式（或加一个 TestCase 包装）才会被采集；请在 CI 侧确认采集数 +1 | 非 TestCase 的 `main()` 脚本不会被 `unittest discover` 执行 ⇒ 门形同虚设 | 见 Agent C 报告（`unittest` 运行输出） |
| CI-003-SCI-FIX-PSF-5 | **Windows 侧未入门禁的共址锁**：`lib/infrastructure/pipeline/orchestrator/cpp/tests/test_p1_batchH_star_coord.cpp` | 该测试有 Makefile 目标（`test_p1_batchH`）但**不在任何 CMakeLists / CI 面**；本任务已订正其假前提头注，建议把它并入 Windows 单测门（`WIN-TEST-UNIT` 阶段或 orchestrator Makefile 的 `run_test`） | R-3 §3.5：它是"未入门禁的锁"，正是 0.5px 双扣长期不被发现的制度原因 | 头注订正见 `lib/infrastructure/pipeline/orchestrator/cpp/tests/test_p1_batchH_star_coord.cpp:1-30` |