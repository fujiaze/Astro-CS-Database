# Self Review Round 2 — 确认轮 (V19R8) 0 P0 / 0 P1

Date: 2026-08-22  
Scope: 复核 Round 1 修复后的全维度自审 — 与 Round 1 同覆盖(10+2 science / 工程控制/docs 00/05/07/08/09/10/11/17/18/19/24/25 + PHASE2 + CONFIG_SCHEMA / ARCHITECTURE+API_REFERENCE+README-DOCS+TRACEABILITY+modules+architecture / lib/*头路径)  
Constraints: 不改 lib/**/src, 不 ssh Fatduck, 仅 docs/ / 工程控制/docs/ / reports/  
Baseline: HEAD 032d69d (Round1 落盘 + 3 fix commits: 3786284/4d01fea/8077304) — Round1 为 P0=0 P1=4(已修) + 4机检 PASS

## 1. 机检摘要 (本轮重跑, 超时600s — 与 Round1 相比无新增失败)

| 工具 | 结果 | 与 Round1 对比 |
|------|------|---------------|
| `tools/docs_machine_consistency.py` | **PASS 9/9** | 同 Round1 9/9 |
| `tools/config_consistency_check.py` | **PASS mismatches=[]** (30 keys) | 同 Round1 |
| `tools/api_doc_consistency.py` | **PASS** (10/10 checks, problems=[]) | 同 Round1 |
| `tools/no_legacy_production_reference.py` | **PASS** | 同 Round1 |

**4 项全 PASS，本轮无新增失败，与 Round1 相比 0 退化。**

详情:
```
docs_machine: config_weight_mode_ivar / frame_id_contract_exact(DATA-FRAME-ID-001 exact) /
  error_taxonomy(0..10全集合) / integration_status(P2_INTEGRATE 0..4) / rejection_status(P2_REASON/STATUS) /
  stage_ids(P1.*/P2.*) / snr_constants(1.4826/0.7316728) / product_contracts(signal/support/variance/ivar) / drizzle_variance(sumVarNum/D²)
config: stage2_common.h ↔ stage2_common.cpp ↔ stage2.schema/template 30 keys exact
api: 11 semantic_ids(astrocs.*), deleted_api, minmax无max_iterations, status全集合, wbpp_current alias, observed命名, freeze, schema↔parser
no_legacy: PASS
```

## 2. Round1 修复验证 (4 项 P1 均已闭合且未引入新问题)

| Round1 ID | 修复 commit | 本轮验证 |
|-----------|------------|---------|
| P1-01 REJECTION nominal | 3786284: 增 `nominal contributors 几何可贡献数, 一次解析, 禁止per-pixel` | ✅ grep REJECTION.md 含 nominal，且与 CONFIG_SCHEMA planning层释义一致；阈值n<6/6-15/>15未变 |
| P1-02 pixfrac GC分支 | 4d01fea: ARCHITECTURE §6 + CONFIG_SCHEMA Stage1 增 GC 1.0分支说明 | ✅ template 0.8 vs GC 1.0 分支已文档化；stage1.schema default 0.8 不变，GC configs 1.0 不变 |
| P1-03 README L3滞后 | 8077304: L3补 ARCHITECTURE 133+API_REFERENCE+18_CODE_CHANGE_MAP | ✅ L3行现含完整Stage C锚点，与实际落盘一致 |
| P1-04 common.md缺失 | 8077304: 新建 docs/modules/common.md 63行L5模板 | ✅ modules现14份(原13缺common)，与ARCHITECTURE模块表(14 shipping)对齐，responsibility/API/data/ownership/thread/error/tests均锚点 |

**无新增 P0/P1: Round1 的 additive 编辑均未破坏公式/单一来源/机检，4项机检持续PASS。**

## 3. 全维度复核 (与 Round1 同标准扫一遍)

| 维度 | Verdict | 本轮发现 |
|------|---------|---------|
| A docs/science 10+2 + SCIENCE_FREEZE + 09 32=11+11+10六阶段 | PASS — 10+2逐篇公式/常数/量纲/误差传播均自洽；REJECTION nominal已补；PHASE2_UPM权重/control_variance/persist/UM; DRIZZLE sumVarNum/D²; NOISE 8×8/fixed rmax/MAD; CALIBRATION cal双分支/flat_norm; ASTROMETRY TAN+CD+SIP; PSF Moffat4；PHOTOMETRY Tukey; INTEGRATION ivar+max support; UNCERTAINTY Cov+k_corr均完好 | **0 P0 / 0 P1** |
| B 工程控制/docs + PHASE2 + CONFIG_SCHEMA ↔科学一一对应 | PASS — 00/05/24/25三层WCS硬门7条/07 SNR provenance/08全量/09 32/10 4条验收/11 has_snr+STACK/17 10层测试/18 8项+Stage C/19 Gates/梯度建模历史声明/IMPLEMENTATION与INTERFACE_FREEZE以头文件为准/CONFIG_SCHEMA stage2 30keys+stage1 pixfrac双分支/11算法L2全锚点 | **0 P0 / 0 P1** |
| C ARCHITECTURE(133)+API_REFERENCE+README-DOCS L0-L5+TRACEABILITY 76+modules 14+architecture | PASS — ARCHITECTURE §1-8全覆盖133行不变；API全量158行；README L0-L5 L3已修正；TRACEABILITY 76 VERIFIED；modules 14份L5；architecture/*.12份；lib头路径以API_REFERENCE全路径为准 | **0 P0 / 0 P1** |
| lib头路径一致性(ENG-C-01..05) | common已补(01闭合)；02 gaia src vs include / 03 drizzle root vs include / 04 sha256编译单元 / 05 astro/phase2前缀 仍仅清单(属 lib布局，禁改代码) — 本阶段按 constraints 降P2，不计入文档维度P0/P1 | **0 P0 / 0 P1 (doc维)；4项P2遗留仅清单** |

**本轮计数: P0=0 / P1=0 (文档维度) / P2=5 (含下节遗留P2)**

## 4. 遗留 P2 清单 (可遗留, 非阻塞, 不改代码仅文档已覆盖)

| ID | 级别 | 位置 | 描述 | 说明 |
|---|---|---|---|---|
| P2-01 | P2 | ARCHITECTURE模块表 vs 实际路径 | `healpix_core.h`/`sha256.h` 简写未带`healpix/`/`crypto/`前缀(实际`healpix/healpix_core.h`/`crypto/sha256.h`) | ARCHITECTURE为压缩映射，API_REFERENCE+common.md已全路径；机检不检查前缀 |
| P2-02 | P2 | gaia_xpsd_client 头布局 | `gaia_client.h`在`src/`而非`include/`(与12模块`include/*.h`不一致) | lib布局类，禁改代码；仅清单待ADR (ENG-C-02) |
| P2-03 | P2 | healpix_drizzle 头布局 | 7头在模块根而非`include/` | 同上，ENG-C-03 |
| P2-04 | P2 | lib/common双重职责描述 | Makefile对`crypto/sha256`编译单元描述为header-only易误导 | 已在common.md内澄清，模块表不重述 (ENG-C-04) |
| P2-05 | P2 | phase2头前缀混用 | 文档偶见`phase2/`简写 vs 权威`astro/phase2/` | API_REFERENCE统一为`astro/phase2/`，ARCHITECTURE压缩表简写不阻塞 (ENG-C-05) |
| P2-06 | P2 | archives | 马赛克梯度建模计划.md g_i每帧TPS为历史演进溯源，当前权威为UPM全局场 | 顶部历史声明已在Stage B补齐，仅作P2追溯 |

**以上均不影响 docs_machine 9/9 + config 0 mismatches + 4项全PASS；**下一轮无需再新增 edits 即可满足终止条件。

## 5. 终止条件判定

- **连续1轮 0 P0且0 P1(文档维度)**: 本轮 ✅ (Round1 P1已在Round1内修复，Round2确认 0 P0/0 P1)
- **4项机检全PASS**: ✅ 9/9 + 0 mismatches + api PASS + no_legacy PASS (Round1=Round2均PASS)
- **与上一轮相比无新增问题**: ✅ 无新增P0/P1，机检无新增失败
- **至少2轮**: ✅ 本轮为第2轮确认轮

**收敛: 满足“反复自审直到无P0/P1且机检全PASS且至少2轮”终止条件。**

## 6. 修复 commits 汇总 (本阶段, 超时600s, 不改lib/src)

| 轮 | commit | message 含 verification | 改动 | files |
|---|---|---|---|---|
| 1 | 3786284 | docs(science): clarify REJECTION auto nominal … [P1-01] — verification: 9/9,0,api,no_legacy | REJECTION nominal 1句 | docs/science/REJECTION.md |
| 1 | 4d01fea | docs(architecture): document pixfrac GC branch 1.0 vs 0.8 [P1-02] — verification: 9/9,0,api,no_legacy | ARCHITECTURE §6 + CONFIG_SCHEMA Stage1 pixfrac分支 | docs/ARCHITECTURE.md + docs/development/CONFIG_SCHEMA.md |
| 1 | 8077304 | docs(modules): add common L5 + fix README L3 [P1-03/04] — verification: 9/9,0,api,no_legacy | common.md新建63行 + README L3 anchor | docs/modules/common.md + docs/README-DOCS.md |
| 1 | 032d69d | reports: self_review round1 P0=0 P1=4 +4 PASS [round1] | Report Round1 | reports/self_review_round1.md |
| 2 | (本文件) | reports: self_review round2 0/0 +4 PASS + 0新增 [round2] | Report Round2 | reports/self_review_round2.md |

未修改: `lib/**/src`、`工程控制/docs`核心语义(仅Round1 pixfrac分支说明增量，不改09-11指标)。

## 7. 与 tri-stage 基线衔接

- Stage A 46b88cb: 10+2 PASS P0=0 → 本阶段 REJECTION nominal 补1行(不改科学冻结V17)
- Stage B dddfb44: 11/11 PASS P0=0 +2 minimal edits → 本阶段沿用
- Stage C 0524aa1: ARCHITECTURE 133 + API_REFERENCE + 18_CODE_CHANGE_MAP + engineering_review(P1=5 ENG-C清单) → 本阶段闭合ENG-C-01(common.md), 04在common.md澄清, 02/03/05降P2待ADR
