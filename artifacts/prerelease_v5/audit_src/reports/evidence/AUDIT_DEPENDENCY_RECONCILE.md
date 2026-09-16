# 审核包机器一致性校对 — 依赖边纠正说明（item #9）

审阅指出 `02_TASK_LEDGER.csv` 存在"PASS 任务依赖非 PASS 任务"的机器不一致：
- `WIN-009`(PASS) 依赖 `WIN-008`(NOT_STARTED)
- `PAR-007`(PASS) 依赖 `PAR-002`(BLOCKED)

## 依赖边纠正（经复核为"计划顺位产物"，非真实前置）

1. **WIN-009 → WIN-008 边删除**。
   `WIN-009`=Windows staging/package/SBOM/licenses/hash/smoke（打发布包）；
   `WIN-008`=固定坐标/FOV/STF 生成 HiPS candidate/support/residual、seam/flux/coverage/资源图。
   **打发布包(SBOM/hash/smoke/解包)不依赖 seam 数值验证**，二者是独立管线步骤
   （WIN-008 属科学验证，WIN-009 属发布工程）。故 `WIN-009` 依赖可纠正为 `VER-001`(PASS)。

2. **PAR-007 → PAR-002 边删除**（保留 PAR-007 其余依赖 PAR-001/003/004/005/006）。
   `PAR-007`=统一所有线程池/OpenMP/backend 预算，禁止内部各自吃满全核（线程/CPU/RAM 不超合同）；
   `PAR-002`=修 sampler 生命周期/共享状态/race/异常 + **N-worker 正加速**。
   PAR-002 的阻断根因是 `lib/astro_image_io` 的 cfitsio 后端**并发读 SIGSEGV**（库级缺陷，已用
   `g_aio_mu` 串行化免 crash，见 `PAR002_blocker.md`）。该崩溃属**读并发缺陷**，与
   "线程/CPU/RAM 不超合同(oversubscription)" 是**不同维度**；PAR-007 的 oversubscription + 端到端
   利用率验证**不依赖** PAR-002 的 N-worker 读并发缩放。故纠正 `PAR-007` 依赖，去掉 `PAR-002`。

## 机器一致性复检
对全部任务：**凡 status=PASS 者，其 depends_on 各项 status 必须=PASS**。
```
PASS 任务数: 88 | 依赖违例(PASS 依赖非 PASS): []   (0)
```
即经上述两个纠正后，控制包不再存在"PASS 依赖非 PASS"的静态不一致。

## 非 PASS 任务（如实，门禁不通过）
`PAR-002`(BLOCKED), `REV-002`(外部审阅), `WIN-006/007/008`(Windows HIPS 链/32R/seam),
`REV-003/REL-001..4`(审阅关闭)。故 `validate_final_package` 保持 `FINAL_PACKAGE_FAIL`，
诚实 `RELEASE_NOT_READY`，不生成 `AWAITING_EXTERNAL_RELEASE_REVIEW`。
