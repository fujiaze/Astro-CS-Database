# FINAL-AUDIT-001 — V6 并行包最终独立审查（V6 / Wave 13）

- 任务：`FINAL-AUDIT-001`（write_scope = `reports/v6/final-audit/`，只给 findings）
- 审查角色：**独立对抗性审查方**（本包科学实现与集成非本审查方完成）
- 基线 HEAD：`46ca757392eb9e81a1dacca0183a8883ea9eeacf`（`git rev-parse HEAD` 复核一致）
- 上游 `DOC-CONVERGE-001` 已 PASS；本任务读其非采信其结论
- 生成时间（容器 UTC）：2026-09-15T20:20Z 左右
- **总判定：NOT_READY（不可标 READY_FOR_OWNER_REVIEW）**
- 机器可读清单：`findings.json`（23 条）/ `findings.csv`
- 复现脚本与原始日志：`evidence/`

> 纪律声明：本次审查**未** commit / push / add / 分支 / worktree / stash / reset / clean / rebase，**未**派生任何子代理，**未**替负责人作发布决定。所有命令带 timeout；关键结论均有本机原始输出支撑。除 `reports/v6/final-audit/` 外未写任何文件（构建与 CI 运行产物落在 `/tmp/astrocs-audit/`，非仓库内）。

---

## 0. 结论摘要

| 严重度 | 条数 |
|---|---|
| BLOCKER | 5 |
| MAJOR | 5 |
| MINOR | 5 |
| INFO | 8 |

**NOT_READY 的逐条阻塞理由**

1. **B-01** GitHub Linux CI 在基线 SHA 为红（§14.5 第 2 步 / §17.12 门 7）：本机以 `ci/run.py --profile linux-main` 独立复现 `THREAD-BUDGET` FAIL、`CTEST-REGISTRATION` FAIL，整体 rc=1。
2. **B-02** §13.3 要求的最终 SHA 真实数据流终验未执行：无全 testdata/Gaia 数据流、无 M42/银心 Phase1→Phase2 分进程马赛克、无 §13.4 图像初审（§17.12 门 8/9 未满足）。
3. **B-03** Windows/Fatduck 正式复验未完成：本机独立探测 Fatduck 不可达（ssh rc=255、tcp rc=124、tailscale `offline, last seen 4h ago`）；Windows CI 无候选（run 35012779853 failure，且其 SHA=8f5ef3e9 ≠ 最终 SHA）。
4. **B-04** V6 三 Phase 生产入口在 CLI/生产中**无消费者**（库+测试层），`--mode point_information|psfsw_robust` 被接受后仍走 legacy 集成路径（§16.2 / §14.3）。
5. **B-05** §10.5/§17.6 资源门未达且 fail-open：16 worker CPU 均值 65.09%（门 85%）、p50 87.6%（门 90%）、内存增长 34.01 MB/s ≥ 32、全预算 `alloc_reclaim_missing`；仅以 `record_only_pending_owner_signoff` 放行。

---

## 1. 科学：独立重推与实现逐项对照（含自写 numpy 算例）

**方法**：不调用被测实现作真值。以 `docs/contracts/v6/frozen/`、`docs/science/v6/frozen/` 的权威式在纯 numpy 独立实现，再逐行阅读实现核对符号/指数/归一。

独立 Oracle：`evidence/independent_oracle.py` → `evidence/independent_oracle.log`。**17/17 PASS，TOTAL FAILURES: 0**：

| 检查 | 结论 |
|---|---|
| `Q=aPᵀC⁻¹d; W=a²PᵀC⁻¹P; F̂=ΣQ/ΣW; Var=1/ΣW` == 联合 GLS | PASS（同一 d 喂两条路径，F̂ 与 Var 到 1e-8 一致） |
| 白噪声 `W=a²/(σ²·A_NEA), A_NEA=1/ΣP²` == 对角 C⁻¹ 精确式 | PASS（1e-10） |
| GLS `x̂=(AᵀC⁻¹A)⁻¹AᵀC⁻¹d` 正规方程 / 与 OLS 有区别 | PASS |
| `C_out=R C_in Rᵀ` 逐元素 | PASS |
| Drizzle `S_p=Σ B_j a_jp/Σa_jp` == 实现 `Σ c_jp x_j`（`c=(a/A)/D`） | PASS（1e-12） |
| `w_SB_jp=a_jp/A_pixel,j` | PASS |
| 常量面亮度 `S_p=B0`、方差 `Σ v w²/D²`、缩放律 `x→ax ⇒ var→a²var` | PASS |
| PSFSW `Wt=C_norm·S^αConc^β/(N^γB^δ)`、组内 `median=1`、方向单调 | PASS |

**实现对照（抽点）**
- `lib/snr_estimator/cpp/src/information_weight.cpp`：`finish_from_x` 先解 `x=C⁻¹P`（对角 `x=p/σ²`；dense Cholesky；low-rank Woodbury `x=u−D⁻¹L(I+LᵀD⁻¹L)⁻¹Lᵀu`），再 `W=a²Pᵀx`、`Q=a dᵀx`；白噪函数 `A_NEA=1/ΣP²`、`W=a²/(σ²A_NEA)` 且校验 `ΣP=1`。符号/指数/归一**正确**。
- `lib/healpix_db/healpix_drizzle/v6_drizzle_science.cpp`：`w_sb=a/A_pixel`、`c=w_sb/D_p`、`signal_sb=Σc x`、`variance_sb=Σc²v`、`flux_out=ΣS_p D_p`（配合 overlap closure 得 pixfrac² 守恒）。**正确**；`legacy_a_drop` 在 pixfrac<1 显式 fail-closed。
- `lib/photometric_calib/cpp/src/psfsw.cpp`：`Wt` 复合、`group_normalize_by_median`、`Conc=(Σf̂/n)/A_NEA`、`α_k=w_k/Σw`（validity 门）、`var_out[p]=αᵀC_in α`。**正确**。
- `lib/phase2_int/v6/src/phase2_integrate.cpp`：联合 C 走真实联合 GLS（`W=AᵀC⁻¹A`），独立帧走 `ΣQ/ΣW` 并以 `|W−ΣW_info|/W ≤ 1e-9` 校验恒等式；`C_out=R C_in Rᵀ` 恒等式门 `≤kIdentRtol`。

未发现符号、指数或归一错误。相关面见 INFO `i-08`。

---

## 2. PSFSW 与权重面

- **无量纲**：`compute_psfsw_weights` 产出 `W_psfsw=Wt/median(Wt)`；产品 `weight.units="1"`、`group_normalized=true`、`normalization.scope="group"`、`median_target=1.0`；`C_norm` 在组内归一后精确抵消（独立复算 `cnorm∈{1,1000,1e-3}` 均 `median=1`）。
- **未被误写成 ivar/Fisher/1/W**：psfsw 记录块禁止键 `ivar/variance/sigma/fisher/w_info/w_psf…` 由 schema `propertyNames` 与 `forbidden_psfsw_product_keys()` 双重守卫；`covariance.psfsw_boundary.method=propagated_from_composite_coefficients`、`variance_from_weight=false`。
- **final covariance 仅由实际组合系数传播**：`propagate_covariance` 用 `conventional_coadd` 产出的逐像素 `α` 计算 `αᵀC_in α`，未从权重标量反推。
- **诊断量不得冒充权重**：禁止来源别名表（`median_source_snr/support/coverage/fwhm/residual/psfsw…`）在 `lib/phase2_int/v6` 与 `psfsw.cpp` 就位；Phase1 `snr_frame_coefficient`（median(SNR_F) 帧级系数）已从工作树删除且无悬空引用。
- **单一权威词表**：`contracts/data/v6_weight_vocabulary_v1.json` 为唯一 canonical（`weight.kind/units/group_normalized/normalization.*/weight_value`）；`relative_dimensionless` / `dimensionless_relative` 仅作 reader 别名。
- **`psf_snr_power` 严格 DEFERRED 且生产拒绝**：`route_phase2_mode` / `parse_weight_mode` / phase3 模式解析 / psfsw G20 均 REJECT；生产枚举 `{point_information,surface_gls,psfsw_robust}`。
- **legacy weight_mode 0 拒绝**：`route_legacy_weight_mode_int(0)` 与 CLI `mode_gate`（`config.weight_mode` 整数 0 → ARGS）拒绝；1/2 仅 baseline。

**但**：见 M-01（PENDING 阈值被生产硬门执行、OPEN 门未执行）与 m-03（内存路径守卫为死代码）。另外注意 legacy `lib/phase2/src/stage2_common.cpp:377-385` 仍接受 `auto/support_x_snr2` 并把后者映射为 `weight_mode=0`——它属 P36 回退态且不在 V6 写域，但 **CLI 的 legacy phase2 路径仍可到达它**（见 B-04），因此『legacy 0 不得进科学权重面』在真实生产路由上并非全然闭合。

---

## 3. 实现覆盖（自数、自构建、自跑）

- **任务覆盖**：`TASK_MANIFEST.json` 35 任务 == `TASK_LEDGER.csv` 35 行，任务卡文件全部存在（`evidence/manifest_coverage.log`）。唯一缺失证据路径：BASE-OWN-001 的 `run/v6/base/logs/21_verify.log`（实际为 `21_verify_inventory.log`，见 m-01）。
- **根构建图（自跑）**：`cmake -S . -B /tmp/astrocs-audit/build -G Ninja -DASTROCS_BUILD_TESTS=ON` **rc=0**；`cmake --build` **rc=0**（1041 目标）。
- **V6 ctest（自跑）**：`ctest -R v6_` → **100% tests passed, 0 failed out of 79**，无 `***Skipped***`。V6 用例为真实断言（如 `v6_p2_upm` 77 checks、`v6_p3_proj` 76 checks、`v6_p1_drz` 44 checks），无零用例 PASS、无 skip-only。测试以 `g_fail>0 ⇒ rc=1` 收敛。
- **独立性**：集成层带独立 numpy/stdlib Oracle（`tests/integration/v6_p2/oracle/v6_p2_oracle.py`、`tests/integration/v6_p3/p3_v6_export_oracle.py`、`tests/unit/v6_p1_drz/v6_p1_drz_oracle.hpp` 等），未发现以生产实现自证为唯一期望来源。
- **全量根 ctest 非全绿**：405/407；`cpu007_profile_store` 稳定 FAIL（CWD 依赖），`p1hips_performance` 首跑 FAIL、复跑 PASS（抖动）。控制器 C-008 将其描述为『未定向构建 Not Run』，与实际实跑不符（m-02）。
- **49 PENDING / 8 OPEN**：见 M-01——并非全部 fail-closed；OPEN 真实数据门未执行，PENDING 数值阈值已生效。

---

## 4. DAG / 台账一致性

- 33 个 PASS 任务均可对应 **单任务单 commit**（脚本 `evidence/scope_check.py` 与 `git show`）。depends_on 在提交时序上满足（例如 W6 `2ac6b758` 先于 W7 `a689eff2/960d6051` 先于 W8 `4c0296e0`；W3 七项先于 W4 `7a104485`）。
- **write_scope 未越过**：33/33 提交的改动文件集 ⊆ 声明 scope（仅控制器每提交更新 `TASK_LEDGER.csv`，控制器所有物）。**0 越界**，无 `git add -A` 迹象（12 项预存差异始终未被提交）。
- 台账状态：FINAL-AUDIT-001=`READY`，OWNER-PACK-001=`BLOCKED`，WIN-VERIFY-001=`WAITING_WINDOWS`；与任务图一致。
- 状态语义瑕疵：PERF-SCALE-001 记 PASS 而 artifact `recommended_status=REVIEW_REQUIRED`（M-05）。

---

## 5. 证据可信度抽查

- **PERF-SCALE-001 的 65.09% / 87.62% 可从原始样本复现**：本审查方以 `run/v6/performance/p1_real_ldn43_4k_w16_r*/resource_samples.csv` 独立复算得 r0 mean=62.86%/p50=84.56%、r1 65.88%/87.62%、r2 65.09%/88.25%，与报告一致，非编造。
- `resource_gate_record.json` 如实写 `hard_fail=false`、`record_only_pending_owner_signoff`、`would_fail_if_signed=true`，未把未决写成通过。
- **REAL-SCIENCE-001 的 DOCUMENTED_BASELINE 标注如实**：报告 §2.2 与 `five_mode_measurements.json` 明确 equal/exposure/ivar 为 `DOCUMENTED_BASELINE`，仅 W_info/PSFSW 走冻结库函数；§3 明确列出 5 项未测（含真实 FITS 产品链未跑）。无编造。
- 冻结合同计数 96=39/49/8 可复现（i-06）；发布口径无 RELEASED（i-05）。
- **覆盖声明缺口**：QA `case_ledger` 的 W10 真实数据门未执行且未回写（M-04）。

---

## 6. 控制器行为审计（C-002..C-009）

- **§14.5 提交纪律**：HEAD == origin/main（0/0）；V6 区间无 merge 提交；全部 V6 提交 author date == committer date（无 amend/rebase 迹象）；每任务单 commit、单目的（i-01/i-02）。历史残留的 `ci-fix` 分支与 45 个 worktree 创建于 2026-09-08（早于本包），**非本包产物**（m-05）。
- **F1 裁定**：C-006/C-007/C-008/C-009 以治理提交把预存工作树回退态固化为 V6 基线。事实核对：`44e1cb65`（P35 drizzle）、`ac04289d`（docs/contracts）、`95703e63`（P33+P35）、`393db3fb`（P36 六文件 + ci/checks.json）、`c7432fa0`（CMakeLists + 8 测试）。这些提交**确实删除 9 项 CI 检查登记与 8 个测试目标**，属对既有覆盖的削减，且 owner 未授权（M-03）。
- **AR-033 清账**：`3e7fbc44` 仅改 `CMakeLists.txt`（+27 行），注册 4 个 V6 生产库与 13 个 `add_subdirectory`；本审查方独立构建得 79 个 V6 ctest 全过，**AR-033 陈述属实**（i-03）。
- **AR-034 清账**：`b7c4f35e` 改名并显式 add_test；本机 `CTEST-REGISTRATION` 只剩 2 项非 V6 IPV 残项，与 C-009 一致。
- **C-009 CI 事实核实**：本机复现 `THREAD-BUDGET` FAIL 的根因正是 `lib/core/src/module_adapters.cpp:245,252`；`CTEST-REGISTRATION` FAIL 为 2 项 IPV；`AGENTS-GOV`/`VERSION-CONSISTENCY`/`KNOWN-FAILURES-BASELINE-VERIFY` PASS。陈述属实。
- **12 项预存差异**：`git diff --numstat | wc -l`=12，全部落在 `artifacts/prerelease_v5`、`evidence/v6_1_rework`、`reports/v19r2`、`问题扫描`，与 V6 写域无交集（i-04）。

---

## 7. Windows 与发布口径

- `VERSION` = `0.11.0-alpha.2`（**编号未提升**）；README/REVIEW/RELEASE_STATUS/CHANGELOG 一致 `NOT_READY`/`NOT_RELEASED`；`release_review_verdict.json` 的 `announce_release=false`。**无任何发布宣称**（i-05）。
- Fatduck 本机独立探测**不可达**（B-03）；Windows CI 无候选，且证据 SHA=8f5ef3e9 非最终 SHA。§17.12 门 7/8 未满足；§15.3 允许 `AWAITING_WINDOWS_VALIDATION` 但不许据此发布——报告与台账如实。
- `AWAITING_EXTERNAL_RELEASE_REVIEW` **未达成**（release-review §14.5 已如实说明）。

---

## 8. CI 事实

| 检查 | 本机独立结果 | 与 C-009 一致 |
|---|---|---|
| THREAD-BUDGET | **FAIL** rc=1（module_adapters.cpp:245,252） | 是 |
| CTEST-REGISTRATION | **FAIL** rc=1（2 项非 V6 IPV） | 是 |
| AGENTS-GOV | PASS rc=0 | 是 |
| VERSION-CONSISTENCY | PASS rc=0 | 是 |
| KNOWN-FAILURES-BASELINE-VERIFY | PASS rc=0 | 是 |

整体 `verdict=FAIL total=5 pass=3 fail=2`（`evidence/ci_repro.log`）。**Linux CI 为红，红因如上**。

- **Windows CI 失败根因**：**UNKNOWN**。失败步为 `Run MSVC tests and package candidate`（exit 1，candidate skipped）；无 token 无法取回 `win-stage-*.log`/`win-package-summary.json`。候选根因候选为 FD-F-003（Windows C++ 单测门长期 `No tests were found!!!`）但**未证实**（i-07）。
- 远程 GitHub API 复取受限（本机无 `gh`、无 token，匿名 API 触发 rate limit）；该事实在报告中如实标 UNKNOWN，不作正/负结论。

---

## 9. findings 清单（详见 `findings.json`）

| id | 严重度 | 摘要 | V6 归属 |
|---|---|---|---|
| B-01 | BLOCKER | Linux CI 红（THREAD-BUDGET / CTEST-REGISTRATION） | RUNTIME-CI-001 / 控制器 |
| B-02 | BLOCKER | §13.3 最终 SHA 真实数据终验未执行 | REAL-SCIENCE-001 |
| B-03 | BLOCKER | Windows/Fatduck 复验未完成 | WIN-VERIFY-001 |
| B-04 | BLOCKER | V6 三 Phase 生产入口无 CLI 消费者 | P1/P2/P3-INTEGRATE-001 / RUNTIME-CI-001 |
| B-05 | BLOCKER | §10.5 资源门未达且 fail-open（65.09%<85%） | PERF-SCALE-001 / SO-05 |
| M-01 | MAJOR | PENDING 阈值生产硬门未签字 + OPEN 门未执行 | CONTRACT-FREEZE-001 / QA-MATRIX-001 |
| M-02 | MAJOR | FROZEN 规范正文被无签字修改（ALG-P1 §4.2） | DOC-CONVERGE-001 |
| M-03 | MAJOR | 基线提交固化 P33/P35/P36 回退，删 9 CI 检查 + 8 测试 | 控制器 / CTRL-F1 |
| M-04 | MAJOR | QA W10 真实数据门未执行且 case_ledger 未回写 | QA-MATRIX-001 / REAL-SCIENCE-001 |
| M-05 | MAJOR | 台账 PASS 与 artifact REVIEW_REQUIRED 并存 | PERF-SCALE-001 |
| m-01 | MINOR | 台账证据路径 21_verify.log 不存在 | BASE-OWN-001 |
| m-02 | MINOR | 全量根 ctest 1 稳定 FAIL（CWD 依赖）+ 1 抖动 | 非 V6 |
| m-03 | MINOR | 内存路径 variance_from_weight 守卫为死代码 | IMPL-P1-PSFW-001 / P2-INTEGRATE-001 |
| m-04 | MINOR | PSFW 研究文档 concentration 单位不一致 | DOC-CONVERGE-001 |
| m-05 | MINOR | 预存 ci-fix 分支 + 45 worktree（§14.1） | 无（预存） |
| i-01..i-08 | INFO | 见 `findings.json` | — |

---

## 10. 独立发现的对抗性尝试（含失败尝试）

为尽量发现既有报告未覆盖的问题，本审查方另做了以下尝试；**失败/无果的尝试如实登记**：

1. **公式符号/指数反向篡改搜索**：逐行读 `information_weight.cpp`（对角/dense/low-rank/白噪）、`v6_drizzle_science.cpp`、`psfsw.cpp`、`phase2_integrate.cpp`。**未发现错误**（i-08）。
2. **联合 GLS vs ΣQ/ΣW 独立性**：以同一 d 构造两条路径；初次对照因 numpy 代码两次采样不同数据而误报 FAIL，修正为同数据后 PASS（保留此失败尝试以说明结论可靠性）。
3. **V6 入口可达性枚举**：对 `write_phase1_product / run_point_information / run_surface_gls / run_psfsw_robust / export_product / p3_v6_export` 做全仓（排除 tests/run）引用枚举与 `cmd_phase2_run` 追踪——**据此发现 B-04（既有报告未覆盖 P1/P3）**。
4. **预存差异归因**：对 12 项 tracked 差异逐项看 diff 域，确认与 V6 无交集。
5. **amend/rebase 探测**：比较 V6 全部提交 author/committer 时间与 merge 集合，未发现篡改迹象。
6. **数字复现**：从原始 `resource_samples.csv` 复算 16 worker CPU 均值/中位，验证报告数字；同时注意到报告用特定 run（r2）值代表 16w，已在 B-05 注明区间。
7. **未果尝试**：远程 GitHub API 复取（匿名 rate limit）——无法独立确认远程 CI 的逐 run 状态，标 UNKNOWN；`gh` 不可用；Fatduck 探测失败本身即 B-03 证据。

---

## 11. 复现命令索引

```bash
# 基线
git rev-parse HEAD                       # 46ca7573...
# 科学独立 Oracle
python3 reports/v6/final-audit/evidence/independent_oracle.py   # TOTAL FAILURES: 0
# 写域纪律
python3 reports/v6/final-audit/evidence/scope_check.py          # 33/33 0 越界
# 交付物/台账覆盖
python3 reports/v6/final-audit/evidence/manifest_coverage.py
# 根构建与 V6 测试
cmake -S . -B /tmp/astrocs-audit/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DASTROCS_BUILD_TESTS=ON
cmake --build /tmp/astrocs-audit/build -j "$(nproc)"
cd /tmp/astrocs-audit/build && ctest -R v6_ --output-on-failure   # 79/79
ctest -j4                                                          # 405/407（cpu007_profile_store FAIL）
# CI 红线复现
python3 ci/run.py --profile linux-main --output-root /tmp/astrocs-audit/ci \
  --check THREAD-BUDGET --check CTEST-REGISTRATION --check AGENTS-GOV \
  --check VERSION-CONSISTENCY --check KNOWN-FAILURES-BASELINE-VERIFY   # rc=1 FAIL
# Fatduck 可达性
timeout 25 ssh -i /home/dsh/.ssh/id_ed25519_fatduck fujia@100.104.10.71 hostname   # rc=255
```

---

## 12. 总判定

**NOT_READY。** 本包当前**不可**标 `READY_FOR_OWNER_REVIEW`。阻塞理由为 B-01..B-05（CI 红 / 真实数据终验缺失 / Windows 未复验 / 三 Phase 生产入口未接线 / 资源门未达且 fail-open），并伴随 M-01..M-05 五项 MAJOR 治理与覆盖问题。本审查方**不替负责人作发布决定**，也不宣布任何发布。
