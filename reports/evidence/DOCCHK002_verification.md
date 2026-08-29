# DOCCHK-002 验证报告 — 六层追溯闭环 + 单位二义性 mutation 门

SHA 基线: 本报告验证时当前 SHA(在 DOCCHK-001 `ea1145f`/登记 `eb18b64` 之后)。
结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L133 + 08 §1/§2)
> 对 traceability 六层、单位 glossary、test/oracle IDs 做闭环; 注入 Drizzle 单位二义性。
> PASS = 100% 核心 claims 闭环; 单位 mutation 被抓(修复 V3 漏检)。

六层单向推导(08 §1): `SCI 定义 → ALG 离散算法 → ARCH 数据流/并发 → API/ABI → CODE → TEST/ORACLE`。

## 2. 交付物
- `artifacts/prerelease_v5/tables/TRACEABILITY.csv`(66 claim, 10 域闭环):
  域 = CAL/WCS/PSF/PHOT/NOISE/DRZ/UPM/REJ/INT/P3 + 原 VER;每域 6 层(SCI→ALG→ARCH→API→CODE→TEST)。
  每行映射真实:science_doc+anchor / algorithm_doc+anchor / architecture_doc / api_symbol(path::sym) /
  source_symbol(path::sym) / test_id(真实 oracle 测试文件) / oracle_id(SYN-00x, V5 稳定合成 Oracle ID) /
  unit / precision_contract。
- `tools/check_traceability.py`: 原 TRACE-001 检查器, 本任务扩展 R5 oracle_id 合法形态
  `ORC-<DOM>-NNN | SYN-NNN`(V5 真实 oracle 用 SYN-00x, 全仓无 ORC-* —— 子代理核验)。
- `tools/check_unit_closure.py`(新): 单位二义性检查器(U1 `X 或 Y` 二选一 / U2 `X/Y` 双信号单位,
  须见"语义固定/冻结/等价标注"消歧语否则 FAIL)。修复 V3 漏检: check_glossary 的 G3 只查表内短语,
  未扫 science/API 文档中的"ADU/e⁻"/"ADU 或 ADU/pixel"式二义。
- `tests/quality/test_docchk002_mutation.py`(8 用例)。

## 3. 测试与结果
`python3 -m unittest tests.traceability.test_traceability tests.quality.test_doc_machine_check tests.quality.test_docchk002_mutation` → **OK(20 用例)**。

| 用例 | mutation | 期望 | 结果 |
|---|---|---|---|
| test_01_real_repo_passes | 无(干净仓库) | traceability+unit 双 PASS | OK |
| test_02_delete_sci_doc_link_fails | 删 SCI-CAL-001 的 science_doc | FAIL(R3) | OK |
| test_03_delete_api_symbol_fails | 删 API-WCS-001 的 api_symbol | FAIL(R3) | OK |
| test_04_delete_oracle_fails | 删 TEST-DRZ-001 的 algorithm_doc | FAIL(R3) | OK |
| test_05_removing_test_endpoint_fails_R6 | 删 TEST-INT-* 行 | FAIL(R6 断链) | OK |
| test_06_unit_ambiguity_injection_fails | 注入裸 `ADU/e⁻`(无冻结) | FAIL(U2) | OK |
| test_07_unit_or_ambiguity_injection_fails | 注入 `ADU 或 ADU/pixel` | FAIL(U1) | OK |
| test_08_unit_ambiguity_with_freezing_ok | 注入但带"语义固定: 以 ADU 为主" | PASS(放行) | OK |

干净树: `python3 tools/check_traceability.py` → `TRACEABILITY_PASS claims=66` rc=0;
`python3 tools/check_unit_closure.py --stdout-json` → `{"status":"PASS","failures":0}` rc=0。

## 4. 关键核验
- **100% 核心 claims 闭环**: 10 个科学域每域均有 SCI 端点 + TEST 端点(检查器 R6 强制), 且每层引用
  (文档/schema/头文件/源码/测试/ORC 格式) R4 逐项存在。域覆盖 = CAL/WCS/PSF/PHOT/NOISE/DRZ/UPM/REJ/INT/P3。
- **oracle_id 权威来源**: 子代理确认 V5 独立合成 Oracle 稳定 ID 为 SYN-001..009(Calibration=SYN-001,
  WCS/PSF/PHOT=SYN-002, Noise/SNR=SYN-003, Drizzle=SYN-004, UPM=SYN-005, Rej/Int=SYN-006, Phase3=SYN-007);
  故 R5 兼容 `SYN-\d{3}`。
- **Drizzle 单位二义性**: 文档 DRIZZLE §3/§6 与 NOISE_MODEL §2 存在 `ADU/e⁻`(V3 漏检), 本任务单位检查器
  要求这类二义必须就近有"语义固定/冻结/等价标注"消歧语; 干净树 DRIZZLE 已有 SCI-003 语义固定块, 故 PASS;
  mutation 注入无冻结二义即 FAIL。
- **test/oracle ID 闭环**: 每域 TEST 行 test_id 指向真实 oracle 测试文件(R4 校验), oracle_id 指向 SYN-00x(R5 校验)。

## 5. 限制
- unit 检查器基于正则启发; 冻结消歧语命中即放行, 不解析全部叙述(对"单位: ADU 或 ADU/pixel"
  这类被 FREEZE 误放行的边界已收紧: 移除宽泛 `单位[：:]...ADU` 与 `换算` 分支)。
- 六层表每域取代表性核心符号(如 CAL 用 ac_calibrate_frame); 子域变体(_f64)由头文件下游 SYMMETRIC CLI
  测试覆盖, 不逐行枚举。
- 文档 anchor 用 §标题(如 "3 物理量和单位"), 检查器 R4 对 science_doc/algorithm_doc 只校验文件存在
  (anchor 为建议定位, 不强制标题匹配)。

## 6. 独立确认
删除任一层引用(Science/API/Algorithm/测试端点)均使 `check_traceability` R3/R6 FAIL; 注入 Drizzle 式
单位二义(U2 裸 `ADU/e⁻` / U1 `ADU 或 ADU/pixel`)均使 `check_unit_closure` FAIL; 带冻结语则放行 ——
"100% 核心 claims 闭环; 单位 mutation 被抓"满足。
