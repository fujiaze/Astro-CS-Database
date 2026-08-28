# TRACE-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS TRACE-001 行; 模板 TRACEABILITY.csv 表头(13列扁平六层); docs/VERSIONING.md。

## 动作
1. claim_id 格式冻结: `^(SCI|ALG|ARCH|API|CODE|TEST)-[A-Z0-9]{2,8}-\d{3}$`; oracle_id `ORC-<DOM>-NNN`。
2. 六层链完整性规则 R1-R7 (tools/check_traceability.py 文头): 表头逐字一致/ID 唯一/逐层累积非空/引用文件与符号存在/域含 SCI+TEST 端点/status 合法。
3. 引用存在带牙齿: 文档路径 `path#anchor`; api/source_symbol `path::symbol` 文件存在且符号可见; test_id 路径存在。
4. 种子追溯表: VER 域 6 claim (SCI/ALG/ARCH/API/CODE/TEST-VER-001) 锚定 docs/VERSIONING.md §1/§2/§3 与 gen_version 实现及 5 用例测试。
5. 测试 tests/traceability/ 5 用例: 真实表 PASS、删任一层引用必失败(ALG/CODE/TEST 三处 mutation)、重复与非法 ID、引用不存在、域端点断链。
6. CI 聚合: tests 包 __init__.py 使 `python3 -m unittest discover -s tests` 一键回归(10/10); CTest 挂接待 C2 构建系统建立后执行。

## 验证
- TRACEABILITY_PASS claims=6; mutation: 删 ALG algorithm_doc/删 CODE 的 SCI 引用/删 TEST 的 test_id 均 FAIL(计数 3/3)。
- 发现并修复: 种子表手工列错位(14列)→改脚本按索引生成; 测试 SEED 同源化; architecture_doc 锚点 `#` 剥离。
- 全量回归 unittest discover **10/10 OK**; 三 checker (GOV/VERSION/TRACEABILITY) 全 PASS。

## 产物
tools/check_traceability.py; tests/traceability/; artifacts/prerelease_v5/tables/TRACEABILITY.csv(种子6行); tests/*/__init__.py; 本日志。

## PASS 判定
schema+checker+mutation tests 齐备; 删除任一层引用 checker 必失败(已证); 引用存在/无断链已执行。CI/CTest: unittest 聚合就绪, CTest 随 C2 构建系统接入。TRACE-001 = PASS。
