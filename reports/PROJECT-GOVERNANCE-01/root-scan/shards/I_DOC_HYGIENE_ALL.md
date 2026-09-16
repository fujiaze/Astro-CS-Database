# 分片 I_DOC_HYGIENE_ALL（ROOT-004 分片执行）

- 分片名：I_DOC_HYGIENE_ALL（I_DOC_HYGIENE，P1 13 条 + P2 31 条 = 44 条）
- 产物：reports/PROJECT-GOVERNANCE-01/root-scan/shards/I_DOC_HYGIENE_ALL.psv（表头 + 44 行，每行 10 列）
- 四态计数：OPEN 38 / RESOLVED 4 / VOID 0 / UNVERIFIABLE 2
- 基线实测：git rev-parse --short HEAD = 2c328348（任务书写 ecf6ad6f，已前进；本片一律按当前树重取证）；VERSION = 0.11.0-alpha.2

## ID 覆盖自证

```text
$ timeout 120 python3 run/PROJECT-GOVERNANCE-01/ROOT-004/build_psv_I_DOC_HYGIENE_ALL.py
PSV lines (incl header): 45 ; bad column count rows: [] ; ID count: 44 ; ID order matches assign: True
verdict counts: {"OPEN": 38, "RESOLVED": 4, "UNVERIFIABLE": 2}
$ python3 -c "...cols/fullwidth-pipe/counter..."  ->  lines 45 ; cols set: {10}
$ wc -l -c .../I_DOC_HYGIENE_ALL.psv  ->      45 45307 reports/PROJECT-GOVERNANCE-01/root-scan/shards/I_DOC_HYGIENE_ALL.psv
```

- 列内禁竖线：证据内竖线已替换为全角 ｜（10 行），列内无换行；行序 = 分配表顺序，ID 逐字复制。

## UNVERIFIABLE 清单（2 条）

- M5a-I-001：两文件均在位且字面仍互斥，但 SCI 的『逐位等价』指 rejection+integrate 语义级等价、ALG 的 1e-6/1e-12 指分块数值容差，是否构成契约冲突取决于目标平台数值等价判据；缺负责人对 SCI-ACR-EQUIV-001 与 ALG-ACR-EQUIV-001 F4 容差层级关系的裁决及真实数据对拍口径，故不强判
- M7-I-101：8 处节锚均在位（原 finding 只给 path::节 符号锚、未给逐字引文），但『与自身伪代码/数据结构不相容』需逐式重推上界并与 benchmark 选点比对；缺本分片可执行的复杂度重推口径与对应基准数据，故不强判

## 异常 / 无法定位的条款

- 基线差：任务书写 HEAD=main=ecf6ad6f，实测 HEAD=2c328348（main）；未做任何 git 写。
- 旧锚当前树已不存在：docs/science/PHASE1_API.md、docs/RELEASE_AUDIT_2026-09-05.md、docs/validation/ACCEPTANCE_GATES.md、evidence/performance/、lib/plate_solve_old/、tools/stage2.cpp、docs/history/、schemas/、lib/algorithms/、五份 V7 编号标准。
- M7-I-202 按前台裁决只写「三处值域/措辞不一致」，不写「互操作被锁死」。
- 零修复、零 git 写：仅写 PSV/MD 与 run/PROJECT-GOVERNANCE-01/ROOT-004/ 下脚本与日志。

## 重要 OPEN 示例（前 3）

- M6b-I-001：ACTIVE 文档仍把 orchestrator.exe/astrocs-stage2 当现状与生产入口，唯一入口机器门只是两条字面量黑名单。
- M6a-I-002：result.coverage_ok 写路径仍无条件置 1，三处自测对写路径恒真断言。
- M5b-I-01：docs/RELEASE_STATUS.md 仍 ACTIVE_INFORMATIVE 登记者并用 PASS/PENDING 作现状声明，与 owner 版 §0 状态词口径并存。

## 判 RESOLVED 的 4 条：M3-I-002、M5b-I-02、M6a-I-003、M3-I-003
