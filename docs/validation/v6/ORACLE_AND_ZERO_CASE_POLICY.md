> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# 独立 Oracle、零用例即红与负向 mutation 政策（QA-MATRIX-001）

> 上游：ASTROCS_DESIGN.md §12（验证体系）

- 机器实现：`reports/v6/qa-design/oracle/qa_oracle.py`、`validate_spec.py`、`run_mutations.py`、`run_all.py`
- 证据：`reports/v6/qa-design/evidence/{oracle_baseline.json,rc_summary.json,mutations.json,logs/}`

## 1. Oracle 独立性（对应根因 R2/R4）

1. 独立真值来源白名单：`independent_numpy | independent_numpy_mc | independent_stdlib | structural |
   real_data_checklist | preregistered_comparison | independent_driver`。
2. 每条门必须声明 `oracle.truth`（真值来源）与 `oracle.must_not`（独立于被测对象的对象集，至少含 `acsd`）。
3. `qa_oracle.py` 只用 NumPy + 标准库，参考实现为显式矩阵、解析恒等与定种子 MC；被测公式以 subject 形式独立转写。
4. 禁止子串存在性断言替代值断言；禁止调用被测实现生成期望（对应 AR-042）。
5. 历史"Oracle 全过"结论不得直接继承（AR-043）；真实数据门禁止以生产输出为唯一 expected（PROJECT_SPEC §8）。

## 2. 零用例即红（对应宪章 §14.2 与任务强制纪律）

```text
case_ledger.json 每门：required_cases / executed_cases / skipped_cases / pends / counts_as_pass
runner 规则：
  executed_cases == 0 且非 pending                -> rc=2（不算 PASS）
  skipped_cases >= executed_cases（skip-only）    -> rc=2
  pending 门未声明 counts_as_pass=false / 无 owner -> rc!=0
  executed_cases < required_cases                 -> rc!=0
```

负向证据：MUT-SPEC-15（executed=0）、MUT-SPEC-16（skip-only）均由 `validate_spec.py` 检出（rc=1）。

## 3. 负向 mutation 驱动器

```text
science：qa_oracle.run(bug) -> 断言目标门至少一个 check 变红（rc=1）
spec   ：apply_spec_mutation -> validate_spec.validate -> 断言存在违规（rc=1）
doc    ：修改 QA_MATRIX.md 渲染块 -> check_docs.check_file -> 断言不一致（rc=1）
```

全部 56 条逐条断言 rc!=0；结果见 `evidence/mutations.json` 与 `SUMMARY.md`。

## 4. 局限（如实登记）

- 本任务是**验证设计**：science 门以原型 Oracle 证明"门能红"，生产实现期（W5/W7/W8/W10）必须把原型扩展为完整规模。
- 真实数据门（G-RD-01/02/06）与预注册比较（G-BASE-03）在 W10/W11 执行，本任务登记为 pending。
- `pending_freeze` 数值阈值由 ALG-W3/W4 冻结；在数值落地前，任何实现不得以本矩阵的度量冒充已冻结容差。
